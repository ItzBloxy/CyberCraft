// BlockRender: spike 3's RED4ext plugin. It hooks the game's Streamline calls and D3D12 to map Cyberpunk's frame
// (step 1: the frame log and the dumps), and draws Minecraft blocks into it (step 2: on the finished frame; step 3:
// before DLSS). Commands come through blockrender.request in its folder, and status.txt in the overlay's folder says
// what it's doing. See docs/superpowers/specs/2026-10-01-block-render-design.md.
#include <RED4ext/RED4ext.hpp>

#include "LogQueue.h"
#include "PlayerLocator.h"
#include "Requests.h"
#include "Status.h"
#include "hooks/D3D12Hooks.h"
#include "hooks/HookGroups.h"
#include "hooks/StreamlineHooks.h"
#include "log/Dumps.h"
#include "log/FrameLog.h"
#include "log/FrameLogData.h"
#include "render/Renderer.h"
#include "track/GpuApi.h"
#include "world/BlockWorld.h"
#include "world/Mesher.h"
#include "world/Structures.h"

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr const char* kVersion = "0.7.0";

RED4ext::v1::PluginHandle g_handle = nullptr;
const RED4ext::v1::Sdk*   g_sdk = nullptr;
std::filesystem::path     g_pluginDir;
std::filesystem::path     g_hudDir;  // the CET overlay's folder, where status.txt goes
cpmc::LogQueue            g_log;
bool                      g_hooked = false;  // Streamline's imports are patched

// Made at load and never destroyed: the hooks may call them until the process ends.
cpmc::FrameLog* g_frameLog = nullptr;
cpmc::Dumps*    g_dumps = nullptr;
cpmc::Renderer* g_renderer = nullptr;

cpmc::BlockWorld                           g_world;
cpmc::MeshOptions                          g_meshOptions;
std::vector<cpmc::StatusReport::Structure> g_structures;
std::string                                g_lastRequest = "none yet";
Clock::time_point                          g_lastSecond{};
uint64_t                                   g_lastFrameTokens = 0;
bool                                       g_removalReported = false;
bool                                       g_hudMissingReported = false;
bool                                       g_trackingHeld = false;  // set tracking 1: to measure what tracking costs

template<class... Args>
void Log(std::format_string<Args...> aFormat, Args&&... aArgs)
{
	if (g_sdk)
	{
		g_sdk->logger->Info(g_handle, std::format(aFormat, std::forward<Args>(aArgs)...).c_str());
	}
}

void DrainLog()
{
	for (const std::string& line : g_log.Drain())
	{
		Log("{}", line);
	}
}

std::filesystem::path PluginFolder()
{
	HMODULE module = nullptr;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                   reinterpret_cast<LPCWSTR>(&PluginFolder), &module);
	wchar_t path[MAX_PATH]{};
	GetModuleFileNameW(module, path, MAX_PATH);
	return std::filesystem::path(path).parent_path();
}

// "2026-10-01T12:34:56" for the logs, "2026-10-01_12-34-56" for folder names.
std::pair<std::string, std::string> Timestamps()
{
	SYSTEMTIME now{};
	GetLocalTime(&now);
	return { std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond),
	         std::format("{:04}-{:02}-{:02}_{:02}-{:02}-{:02}", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond) };
}

// At most the first 4 KB of a small text file; empty if it can't be read.
std::string ReadSmallFile(const std::filesystem::path& aPath)
{
	std::ifstream in(aPath, std::ios::binary);
	std::string   text(4096, '\0');
	in.read(text.data(), std::streamsize(text.size()));
	text.resize(size_t(in.gcount()));
	return text;
}

// aText on one line for the log: control characters and anything past ASCII become '?', at most 200 characters.
std::string Printable(const std::string& aText)
{
	std::string out;
	for (const char ch : aText.substr(0, 200))
	{
		out += ch >= 32 && ch < 127 ? ch : '?';
	}
	return out;
}

void Report(std::string aResult)
{
	Log("request: {}", aResult);
	g_lastRequest = std::move(aResult);
}

void Place(const cpmc::Request& aRequest)
{
	std::string error;
	if (!g_renderer->AtlasData() && !g_renderer->LoadAtlas(error))
	{
		return Report("place " + aRequest.what + ": " + error);
	}
	const std::optional<cpmc::PlayerPose> pose = cpmc::LocatePlayer();
	if (!pose)
	{
		return Report("place " + aRequest.what + ": there's no player");
	}
	const cpmc::BlockPos              origin = cpmc::PlacementOrigin(pose->feet, pose->forward);
	const cpmc::Atlas&                atlas = *g_renderer->AtlasData();
	const std::optional<cpmc::Placed> placed = aRequest.what == "field"
	                                               ? cpmc::PlaceField(g_world, atlas, origin, size_t(aRequest.number), error)
	                                               : cpmc::PlaceStructure(g_world, atlas, aRequest.what, origin, error);
	if (!placed)
	{
		return Report("place " + aRequest.what + ": " + error);
	}
	const auto [low, high] = cpmc::WorldBox(*placed);
	g_structures.push_back({ aRequest.what, low, high });
	Report(std::format("placed {}: {} blocks, Minecraft {} {} {} to {} {} {}", aRequest.what, placed->blocks, placed->min.x, placed->min.y,
	                   placed->min.z, placed->max.x, placed->max.y, placed->max.z));
}

void Set(const cpmc::Request& aRequest)
{
	if (aRequest.what == "ao")
	{
		g_meshOptions.ambientOcclusion = aRequest.number != 0.0;
		g_world.MarkAllDirty();
		return Report(std::format("set ao {}", g_meshOptions.ambientOcclusion ? 1 : 0));
	}
	if (aRequest.what.starts_with("hooks."))
	{
		const std::string_view               name = std::string_view(aRequest.what).substr(6);
		const std::optional<cpmc::HookGroup> group = cpmc::HookGroupNamed(name);
		if (!group)
		{
			return Report(std::format("set: there's no hook group called \"{}\": they're lists, views, submit, present and sl", name));
		}
		cpmc::SetHookGroup(*group, aRequest.number != 0.0);
		return Report(std::format("set {} {}: {}", aRequest.what, aRequest.number != 0.0 ? 1 : 0, cpmc::HookGroupsSummary()));
	}
	if (aRequest.what == "tracking")
	{
		const bool hold = aRequest.number != 0.0;
		if (hold != g_trackingHeld)
		{
			hold ? cpmc::d3d::RequestTracking() : cpmc::d3d::ReleaseTracking();
			g_trackingHeld = hold;
		}
		return Report(std::format("set tracking {}: command lists are {}tracked", hold ? 1 : 0, cpmc::d3d::Tracking() ? "" : "not "));
	}
	std::string error;
	Report(g_renderer->Set(aRequest.what, aRequest.number, error) ? std::format("set {} {}", aRequest.what, aRequest.number)
	                                                              : "set: " + error);
}

void RunRequest()
{
	const std::filesystem::path path = g_pluginDir / "blockrender.request";
	std::error_code             error;
	if (!std::filesystem::exists(path, error))
	{
		return;
	}
	const std::string text = ReadSmallFile(path);
	std::filesystem::remove(path, error);
	std::string                         why;
	const std::optional<cpmc::Request> request = cpmc::ParseRequest(text, why);
	if (!request)
	{
		return Report(std::format("\"{}\" ignored: {}", Printable(text), why));
	}
	switch (request->kind)
	{
	case cpmc::Request::Kind::Log: Report(g_frameLog->Arm() ? "log: armed for the next two frames" : "log: one is already running"); break;
	case cpmc::Request::Kind::Dump:
		Report(g_dumps->Arm(request->what, why) ? "dump " + request->what + ": armed for the next frame" : "dump: " + why);
		break;
	case cpmc::Request::Kind::Mode:
		Report(g_renderer->SetMode(request->what, why) ? "mode " + request->what : "mode " + request->what + ": " + why);
		break;
	case cpmc::Request::Kind::Place: Place(*request); break;
	case cpmc::Request::Kind::Clear:
		g_world.Clear();
		g_structures.clear();
		Report("cleared");
		break;
	case cpmc::Request::Kind::Reload: Report(g_renderer->ReloadShaders(why) ? "reload: shaders compiled" : "reload: " + why); break;
	case cpmc::Request::Kind::Set: Set(*request); break;
	}
}

// If the device or the swap chain was made before BlockRender loaded, the detours never saw it: patch it through
// GpuApi instead (risk 1), once its vtable checks out.
void PatchLate()
{
	if (cpmc::d3d::Device() && cpmc::d3d::Original().present)
	{
		return;
	}
	const cpmc::gpuapi::Report report = cpmc::gpuapi::Check(cpmc::d3d::Device());
	if (!cpmc::d3d::Device() && report.nativeDevice)
	{
		cpmc::d3d::PatchLateDevice(report.nativeDevice);
		Log("the device was made before BlockRender loaded: patched it through GpuApi ({})", report.device);
	}
	if (!cpmc::d3d::Original().present && report.swapChainObject)
	{
		void* native = cpmc::slhooks::NativeInterface(report.swapChainObject);  // null unless it's a Streamline proxy
		cpmc::d3d::PatchLateSwapChain(native ? static_cast<IDXGISwapChain*>(native) : report.swapChainObject);
		if (native)
		{
			static_cast<IUnknown*>(native)->Release();
		}
		Log("the swap chain was made before BlockRender loaded: patched it through GpuApi ({})", report.swapChain);
	}
}

void CheckDevice()
{
	ID3D12Device* const device = cpmc::d3d::Device();
	if (!device || g_removalReported)
	{
		return;
	}
	const HRESULT reason = device->GetDeviceRemovedReason();
	if (reason == S_OK)
	{
		return;
	}
	g_removalReported = true;
	std::string error;
	g_renderer->SetMode("off", error);
	Log("the D3D12 device was removed (0x{:08x}), so drawing is off", uint32_t(reason));
	for (const std::string& line : cpmc::d3d::DredReport())
	{
		Log("{}", line);
	}
}

void WriteFrameLog(const cpmc::FrameLogData& aLog, const std::string& aFolderName)
{
	const std::filesystem::path folder = g_pluginDir / "logs" / aFolderName;
	std::error_code             error;
	std::filesystem::create_directories(folder, error);
	std::ofstream out(folder / "frame.json", std::ios::binary);
	out << cpmc::FrameLogJson(aLog);
	if (!out)
	{
		return Report("log: couldn't write " + (folder / "frame.json").string());
	}
	Report(std::format("log: written to logs\\{}\\frame.json ({} evaluates, {} submissions, {} lists, {} G-buffer candidates)",
	                   aFolderName, aLog.evaluates.size(), aLog.submissions.size(), aLog.lists.size(), aLog.gbufferCandidates.size()));
}

void WriteStatus()
{
	const cpmc::Renderer::Stats stats = g_renderer->TakeStats();
	cpmc::StatusReport          report;
	report.mode = g_renderer->ModeName();
	report.framesDrawn = stats.drawn;
	report.framesSkipped = stats.skipped;
	report.lastSkip = stats.lastSkip;
	report.lateCamera = stats.lateCamera;
	report.sections = stats.sections;
	report.blocks = g_world.BlockCount();
	report.vertices = stats.vertices;
	report.gpuMsAverage = stats.gpuMsAverage;
	report.gpuMsWorst = stats.gpuMsWorst;
	report.frameMsAverage = stats.frameMsAverage;
	report.frameMsWorst = stats.frameMsWorst;
	const uint64_t tokens = cpmc::slhooks::FrameTokens();
	report.streamlineFrames = uint32_t(tokens - g_lastFrameTokens);
	g_lastFrameTokens = tokens;
	report.hooks = g_hooked ? std::format("{}; {}; {}; log {}; dump {}", cpmc::slhooks::Summary(), cpmc::d3d::Summary(),
	                                      cpmc::HookGroupsSummary(), g_frameLog->State(), g_dumps->State())
	                        : "not installed (see red4ext\\logs)";
	report.lastRequest = g_lastRequest;
	report.structures = g_structures;
	std::error_code error;
	if (!std::filesystem::is_directory(g_hudDir, error))
	{
		if (!g_hudMissingReported)
		{
			g_hudMissingReported = true;
			Log("the overlay's folder {} is missing, so there's no status file", g_hudDir.string());
		}
		return;
	}
	const std::filesystem::path temporary = g_hudDir / "status.tmp";
	{
		std::ofstream out(temporary, std::ios::binary);
		out << cpmc::FormatStatus(report);
	}
	// While the overlay is reading status.txt this fails; the next second's file replaces it.
	std::filesystem::rename(temporary, g_hudDir / "status.txt", error);
}

bool OnEnter(RED4ext::CGameApplication*)
{
	return true;
}

bool OnExit(RED4ext::CGameApplication*)
{
	return true;
}

// Every frame on the main thread while the game runs (RED4ext ignores the return value in this state).
bool OnUpdate(RED4ext::CGameApplication*)
{
	const auto now = Clock::now();
	const bool newSecond = now - g_lastSecond >= 1s;
	if (newSecond)
	{
		g_lastSecond = now;
		if (g_hooked)
		{
			PatchLate();
			CheckDevice();
		}
		RunRequest();
	}
	const auto [time, folderName] = Timestamps();
	if (g_frameLog->Capturing())
	{
		if (const std::optional<cpmc::PlayerPose> pose = cpmc::LocatePlayer(); pose && pose->camera)
		{
			g_frameLog->NoteScriptCamera({ pose->camera->position, pose->camera->forward, pose->camera->fov });
		}
	}
	if (const std::optional<cpmc::FrameLogData> finished = g_frameLog->TakeFinished(kVersion, time))
	{
		WriteFrameLog(*finished, folderName);
	}
	g_dumps->Update(folderName);
	if (const cpmc::Atlas* atlas = g_renderer->AtlasData())
	{
		for (const cpmc::SectionPos& section : g_world.TakeDirty())
		{
			g_renderer->UploadSection(section, cpmc::MeshSection(g_world, section, *atlas, g_meshOptions));
		}
	}
	g_renderer->Update();
	DrainLog();
	if (newSecond)
	{
		for (const std::string& line : cpmc::d3d::DebugMessages())
		{
			Log("{}", line);
		}
		WriteStatus();
	}
	return false;
}
}

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::v1::PluginHandle aHandle, RED4ext::v1::EMainReason aReason,
                                        const RED4ext::v1::Sdk* aSdk)
{
	switch (aReason)
	{
	case RED4ext::v1::EMainReason::Load:
	{
		g_handle = aHandle;
		g_sdk = aSdk;
		// Pinned: the hooks point into this DLL, and threads may be inside them when the game unloads plugins.
		HMODULE self = nullptr;
		GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&PluginFolder),
		                   &self);
		g_pluginDir = PluginFolder();
		g_hudDir = g_pluginDir.parent_path().parent_path().parent_path() / "bin" / "x64" / "plugins" / "cyber_engine_tweaks" / "mods" /
		           "blockrender_hud";
		const cpmc::PluginSettings settings = cpmc::ParseSettings(ReadSmallFile(g_pluginDir / "settings.txt"));
		g_frameLog = new cpmc::FrameLog(g_log);
		g_dumps = new cpmc::Dumps(g_log, g_pluginDir);
		g_renderer = new cpmc::Renderer(g_log, g_pluginDir);
		for (cpmc::HookEvents* listener : std::initializer_list<cpmc::HookEvents*>{ g_frameLog, g_dumps, g_renderer })
		{
			cpmc::d3d::AddListener(listener);
			cpmc::slhooks::AddListener(listener);
		}
		g_hooked = cpmc::slhooks::Install(g_log);
		if (g_hooked && !cpmc::d3d::InstallDetours(aSdk, aHandle, g_log, { settings.dred, settings.debugLayer }))
		{
			g_log.Push("some D3D12 or DXGI detours are missing (see above)");
		}
		static RED4ext::v1::GameState running{ &OnEnter, &OnUpdate, &OnExit };
		aSdk->gameStates->Add(aHandle, RED4ext::EGameStateType::Running, &running);
		DrainLog();
		Log("BlockRender {} loaded (game {}.{}.{}), {}; dred {}, debug layer {}; requests go in {}", kVersion,
		    unsigned(aSdk->runtime->major), aSdk->runtime->minor, aSdk->runtime->patch, g_hooked ? "hooked" : "NOT hooked",
		    settings.dred ? "on" : "off", settings.debugLayer ? "on" : "off", (g_pluginDir / "blockrender.request").string());
		break;
	}
	case RED4ext::v1::EMainReason::Unload:
		if (g_renderer)
		{
			g_renderer->Shutdown();
		}
		cpmc::d3d::RemovePatches();
		cpmc::slhooks::Remove();
		DrainLog();
		break;
	}
	return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* aInfo)
{
	aInfo->name = L"BlockRender";
	aInfo->author = L"Cyberpunk-Minecraft";
	aInfo->version = RED4EXT_V1_SEMVER(0, 7, 0);
	aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
	aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
	return RED4EXT_API_VERSION_1;
}
