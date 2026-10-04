// BlockRender: spike 3's RED4ext plugin, linked to Minecraft by spike 4. It hooks the game's Streamline calls and D3D12
// to map Cyberpunk's frame (spike 3's step 1: the frame log and the dumps), and draws Minecraft blocks into it (step 2:
// on the finished frame; step 3: before DLSS; step 4: into the G-buffer). Spike 4 links it to SkyCraft's Minecraft
// through SkyCraft's shared memory: Minecraft's player follows V, Minecraft's blocks and HUD are drawn in the game, and
// in build mode (F8) the mouse buttons, the wheel and the keys 1 to 9 are Minecraft's. Spike 5 has Minecraft drive in
// build mode: the movement keys are Minecraft's too, and V is teleported to its player's eye every frame.
// Commands come through blockrender.request, in its folder or the overlay's, and status.txt in the overlay's folder says
// what it's doing. See docs/superpowers/specs/2026-10-01-block-render-design.md, 2026-10-02-minecraft-link-design.md and
// 2026-10-03-collision-and-movement-design.md.
#include <RED4ext/RED4ext.hpp>

#include "LogQueue.h"
#include "Measurements.h"
#include "PlayerLocator.h"
#include "ProcessUsage.h"
#include "Puppet.h"
#include "Requests.h"
#include "Status.h"
#include "hooks/D3D12Hooks.h"
#include "hooks/HookGroups.h"
#include "hooks/StreamlineHooks.h"
#include "input/RawInputHook.h"
#include "input/Router.h"
#include "link/Link.h"
#include "link/Mapping.h"
#include "link/Turn.h"
#include "log/DriveLog.h"
#include "log/Dumps.h"
#include "log/FrameLog.h"
#include "log/FrameLogData.h"
#include "render/Hud.h"
#include "render/Renderer.h"
#include "track/GpuApi.h"
#include "world/BlockWorld.h"
#include "world/Mesher.h"
#include "world/Structures.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr const char* kVersion = "0.11.0";

RED4ext::v1::PluginHandle g_handle = nullptr;
const RED4ext::v1::Sdk*   g_sdk = nullptr;
std::filesystem::path     g_pluginDir;
std::filesystem::path     g_hudDir;  // the CET overlay's folder, where status.txt goes
cpmc::LogQueue            g_log;
bool                      g_hooked = false;       // Streamline's imports are patched
bool                      g_inputHooked = false;  // GetRawInputData's import is patched

// Made at load and never destroyed: the hooks may call them until the process ends.
cpmc::FrameLog*      g_frameLog = nullptr;
cpmc::Dumps*         g_dumps = nullptr;
cpmc::Renderer*      g_renderer = nullptr;
cpmc::input::Router* g_router = nullptr;  // where keys and clicks go (the spec's "Input")

cpmc::BlockWorld                           g_world;
cpmc::MeshOptions                          g_meshOptions;
std::vector<cpmc::StatusReport::Structure> g_structures;
std::string                                g_lastRequest = "none yet";
Clock::time_point                          g_lastSecond{};
uint64_t                                   g_lastFrameTokens = 0;
bool                                       g_removalReported = false;
bool                                       g_hudMissingReported = false;
bool                                       g_trackingHeld = false;  // set tracking 1: to measure what tracking costs

// Spike 4: the link to SkyCraft's Minecraft (the spec's "The link"). Main thread only.
cpmc::link::Mapping              g_mapping;
cpmc::link::Link*                g_link = nullptr;  // once the mapping is open
std::string                      g_mappingState = "not opened yet";
cpmc::link::LinkSettings         g_linkSettings;     // set teleportstep, teleportrate, ringmb, translucent; and the viewport
bool                             g_linked = false;   // link on|off
int                              g_hudSetting = 1;   // set hud: 0 never, 1 in build mode, 2 always
uint32_t                         g_hudScale = 2;     // set hudscale: 1, 2 or 4
bool                             g_hudWanted = false;
bool                             g_minecraftAtlas = false;  // Minecraft's atlas has replaced atlas.bin's
std::optional<cpmc::Vec3d>       g_camera;                  // V's camera, Minecraft axes, as the link last had it
std::string                      g_lastTyped;               // the last mc request, which names its burst
std::optional<double>            g_lastEnter;               // the link's last Enter, once marked
bool                             g_buildWanted = false;     // F8's link, or build on: build mode, after the link's next update
cpmc::LatencyTracker             g_latency;
cpmc::BurstWindow                g_burst;
std::optional<cpmc::BurstReport> g_lastBurst;
cpmc::ProcessWatch               g_cyberpunkProcess;
cpmc::ProcessWatch               g_minecraftProcess;
double                           g_lastFrameAt = 0.0;  // SteadySeconds() at the last OnUpdate

// Spike 5: build mode's drive (its spec's "Part 1"). Main thread only.
cpmc::Puppet          g_puppet{ g_log };  // the game's script calls that place V
cpmc::link::TurnRatio g_turnRatio;        // the mouse's counts to V's degrees, measured outside build mode
bool                  g_turn = true;      // set turn 1, the default since step 2: the teleport's yaw gains the mouse's turn
cpmc::DriveLog        g_driveLog;

// This frame's figures, for bursts.
struct FrameFigures
{
	uint64_t sections = 0;
	uint64_t bytes = 0;
	double   drainMs = 0.0;
};
FrameFigures g_frame;

// This second's, for the status.
struct SecondFigures
{
	double                                         start = 0.0;  // SteadySeconds()
	uint64_t                                       frames = 0;
	uint64_t                                       bytes = 0;
	uint64_t                                       bytesWorstFrame = 0;
	double                                         drainMsSum = 0.0;
	double                                         drainMsWorst = 0.0;
	uint64_t                                       teleportsAtStart = 0;
	uint64_t                                       minecraftFramesAtStart = 0;
	std::array<uint64_t, cpmc::link::Link::kTypes> messagesAtStart{};
	std::array<uint64_t, cpmc::link::Link::kTypes> bytesAtStart{};
	// The drive's
	uint64_t waitedAtStart = 0;
	double   eyeDistanceSum = 0.0;
	double   eyeDistanceWorst = 0.0;
	uint64_t eyeDistanceFrames = 0;
	double   teleportMsSum = 0.0;
	double   teleportMsWorst = 0.0;
	uint64_t teleportCalls = 0;
};
SecondFigures g_second;

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

// What Minecraft sends, handed to the renderer (the spec's "Blocks" and "The HUD").
class MinecraftSink : public cpmc::link::LinkSink
{
public:
	void Atlas(const cpmc::link::AtlasImage& aAtlas) override
	{
		const double               start = cpmc::SteadySeconds();
		std::string                error;
		std::optional<cpmc::Atlas> atlas = cpmc::AtlasFromRgba(aAtlas.width, aAtlas.height, aAtlas.rgba, error);
		if (!atlas)
		{
			return ::Log("Minecraft's atlas can't be built ({}), so the atlas stays as it was", error);
		}
		// Spike 3's own structures go, with every section: their UVs belong to the atlas before. Spike 3's world isn't
		// meshed again while Minecraft's atlas is loaded, so its removals can't undo Minecraft's sections.
		g_world.Clear();
		g_world.TakeDirty();
		g_structures.clear();
		g_renderer->ClearSections();
		g_renderer->SetAtlas(std::move(*atlas));
		g_minecraftAtlas = true;
		::Log("Minecraft's atlas: {} x {}, {} levels, built in {:.1f} ms", aAtlas.width, aAtlas.height, cpmc::kMinecraftAtlasLevels,
		      (cpmc::SteadySeconds() - start) * 1000.0);
	}

	void Section(cpmc::link::SectionMesh&& aMesh) override
	{
		const cpmc::SectionPos section{ aMesh.x, aMesh.y, aMesh.z };
		++g_frame.sections;
		if (g_latency.SectionArrived(cpmc::SteadySeconds(), g_renderer->Presents()))
		{
			g_renderer->TimeUpload(section);  // the first section after a mark: timed to the list that first draws it
		}
		g_renderer->UploadSection(section, std::move(aMesh.vertices));
	}

	void ClearAll() override
	{
		g_renderer->ClearSections();
		::Log("Minecraft cleared its sections: it sends everything again");
	}

	void Explosion(const skycraft::proto::McEvent& aEvent) override
	{
		::Log("explosion at Minecraft {:.1f} {:.1f} {:.1f}, radius {:.1f}", aEvent.a, aEvent.b, aEvent.c, aEvent.d);
		g_burst.Start(cpmc::SteadySeconds(), "explosion");
	}

	void Hud(const cpmc::link::HudFrame& aFrame) override
	{
		g_renderer->SetHudImage(aFrame.width, aFrame.height, aFrame.bottomUp, aFrame.pixels);
	}

	void Log(const std::string& aLine) override
	{
		::Log("link: {}", aLine);
	}
};
MinecraftSink g_sink;

// The mapping, made as SkyCraft's Skyrim side makes it (the spec's "link/"), at the first update rather than at load:
// the heartbeat comes once a frame from Link::Update, and the game doesn't update until it has loaded, so Minecraft
// would link to a game side that then falls silent. While another game side uses it, it's refused, and tried again
// once a second.
void OpenMapping()
{
	using Outcome = cpmc::link::Mapping::Outcome;
	const cpmc::link::Mapping::Opened opened = g_mapping.Open(skycraft::proto::kMappingName, GetCurrentProcessId(), GetTickCount64());
	const bool                        open = opened.outcome == Outcome::Created || opened.outcome == Outcome::Reused;
	const std::string state = open ? opened.why : (opened.outcome == Outcome::Refused ? "refused: " : "failed: ") + opened.why;
	if (state != g_mappingState)
	{
		Log("the mapping: {}", state);
	}
	g_mappingState = state;
	if (open)
	{
		g_link = new cpmc::link::Link(g_mapping.Base());
		g_router->SetLink(g_link);
		g_second = {};
		g_second.start = cpmc::SteadySeconds();
	}
}

// QueryPerformanceCounter: the clock Minecraft stamps its ticks with.
int64_t QpcNow()
{
	LARGE_INTEGER now{};
	QueryPerformanceCounter(&now);
	return now.QuadPart;
}

int64_t QpcPerSecond()
{
	static const int64_t perSecond = [] {
		LARGE_INTEGER frequency{};
		QueryPerformanceFrequency(&frequency);
		return frequency.QuadPart;
	}();
	return perSecond;
}

// What placing V did this frame, for the status and the drive log.
struct Placed
{
	double                teleportMs = 0.0;
	double                turn = 0.0;  // degrees set turn 1 added
	std::optional<double> yaw;         // the teleport's, if V was teleported
};

// Spike 5's drive, after the link's update: build mode ends if the link says so; otherwise, while it's on, V goes where
// the link puts it, facing its own yaw, and the frame's turn from the mouse with set turn 1. A teleport that fails ends
// build mode, so Minecraft never walks away from a V that can't follow.
Placed PlaceV(const cpmc::link::Link::Status& aState, const std::optional<double>& aYaw, int64_t aMouseX)
{
	Placed placed;
	if (!aState.endBuild.empty())
	{
		g_router->TurnOff(aState.endBuild);
		return placed;
	}
	if (!aState.placeV || !g_router->On())
	{
		return placed;
	}
	if (!aYaw)
	{
		g_router->TurnOff("V's yaw can't be read");
		return placed;
	}
	placed.turn = g_turn ? g_turnRatio.Turn(aMouseX) : 0.0;
	std::string  error;
	const double start = cpmc::SteadySeconds();
	const bool   moved = g_puppet.Teleport(*aState.placeV, *aYaw + placed.turn, error);
	placed.teleportMs = (cpmc::SteadySeconds() - start) * 1000.0;
	if (!moved)
	{
		g_router->TurnOff("V couldn't be placed: " + error);
		return placed;
	}
	placed.yaw = *aYaw + placed.turn;
	++g_second.teleportCalls;
	g_second.teleportMsSum += placed.teleportMs;
	g_second.teleportMsWorst = std::max(g_second.teleportMsWorst, placed.teleportMs);
	return placed;
}

// The link, once a frame (the spec's "The link"): V's pose and the HUD's size, then Link::Update, which hands what
// Minecraft sends to the sink. In build mode, V then goes where the link puts it (spike 5).
void UpdateLink(double aNow, double aFrameMs)
{
	g_frame = {};
	const int64_t mouseX = g_router->TakeMouseX();  // every frame, so a frame's turn is only its own
	if (!g_link)
	{
		return;
	}
	const cpmc::link::Link::Status&     state = g_link->State();
	std::optional<cpmc::link::GamePose> pose;
	if (const std::optional<cpmc::PlayerPose> player = cpmc::LocatePlayer(); player && player->camera)
	{
		pose = cpmc::link::GamePose{ player->feet, player->camera->position, player->camera->forward };
	}
	g_camera = pose ? std::optional<cpmc::Vec3d>(cpmc::McFromWorld(pose->camera)) : std::nullopt;
	const bool buildMode = g_router->On();
	// While linked: V's yaw, and what keeps V from being placed. Puppet finds V itself, so the reason is known without
	// the camera too: in a car the camera isn't V's eye, and the link finds no V. The turn ratio is measured outside
	// build mode, while V can be placed, so its yaw is V's own doing.
	const std::optional<double> yaw = g_linked && pose ? g_puppet.Yaw() : std::nullopt;
	const std::string           blocked = g_linked ? g_puppet.Blocked() : std::string();
	if (!buildMode && yaw && blocked.empty())
	{
		g_turnRatio.Note(mouseX, *yaw);
	}
	else
	{
		g_turnRatio.Break();
	}
	g_hudWanted = cpmc::HudWanted(g_hudSetting, buildMode, state.inWorld);
	g_renderer->SetHud(g_hudWanted);
	const cpmc::HudSize back = g_renderer->BackBufferSize();
	const cpmc::HudSize viewport = cpmc::HudViewport(back.width, back.height, g_hudScale);
	g_linkSettings.viewportWidth = viewport.width;
	g_linkSettings.viewportHeight = viewport.height;

	cpmc::link::Link::Frame frame;
	frame.now = aNow;
	frame.tickMs = GetTickCount64();
	frame.qpc = QpcNow();
	frame.qpcPerSecond = QpcPerSecond();
	frame.pose = pose;
	frame.linked = g_linked;
	frame.buildMode = buildMode;
	frame.placeable = blocked.empty();
	frame.takeHud = g_hudWanted;
	const uint64_t drainedBefore = state.drainedBytes;
	const uint64_t waitedBefore = state.waitedFrames;
	g_link->Update(frame, g_linkSettings, g_sink);
	g_router->Update(g_linked, state, blocked);  // build mode ends if Minecraft or V has gone, or the link, or V can't be placed
	const Placed placed = PlaceV(state, yaw, mouseX);
	if (state.eyeDistance)
	{
		g_second.eyeDistanceSum += *state.eyeDistance;
		g_second.eyeDistanceWorst = std::max(g_second.eyeDistanceWorst, *state.eyeDistance);
		++g_second.eyeDistanceFrames;
	}
	if (g_driveLog.Recording())
	{
		cpmc::DriveRow row;
		row.time = aNow;
		row.frameMs = aFrameMs;
		row.state = cpmc::link::DriveStateName(state.drive);
		row.camera = pose ? std::optional<cpmc::Vec3d>(pose->camera) : std::nullopt;
		row.target = state.placeV ? std::optional<cpmc::Vec3d>(cpmc::Vec3d{ state.placeV->x, state.placeV->y, state.placeV->z + state.eyeHeight })
		                          : std::nullopt;
		row.lastDistance = state.eyeDistance;
		row.delayMs = state.renderDelayMs;
		row.waited = state.waitedFrames != waitedBefore;
		row.teleportMs = placed.teleportMs;
		row.mouseX = mouseX;
		row.yaw = placed.yaw;
		row.turn = placed.turn;
		g_driveLog.Add(row);
	}
	if (std::exchange(g_buildWanted, false))
	{
		const std::optional<std::string> why = g_router->TurnOn();
		Report(why ? "build on: it stays off: " + *why
		           : "build on: Minecraft's player drives V; the movement keys, the mouse buttons, the wheel and 1 to 9 are Minecraft's");
	}
	g_frame.bytes = state.drainedBytes - drainedBefore;
	g_frame.drainMs = state.drainMs;
	++g_second.frames;
	g_second.bytes += g_frame.bytes;
	g_second.bytesWorstFrame = std::max(g_second.bytesWorstFrame, g_frame.bytes);
	g_second.drainMsSum += g_frame.drainMs;
	g_second.drainMsWorst = std::max(g_second.drainMsWorst, g_frame.drainMs);
	if (state.lastEnter && state.lastEnter != g_lastEnter)
	{
		// A typed command's Enter: a latency mark, and a burst's start.
		g_lastEnter = state.lastEnter;
		g_latency.Mark(*state.lastEnter);
		g_burst.Start(*state.lastEnter, "mc " + g_lastTyped);
	}
	if (const std::optional<double> press = g_router->TakePress())
	{
		g_latency.Mark(*press);  // a click that went to Minecraft
	}
	if (const std::optional<cpmc::Renderer::TimedUpload> timed = g_renderer->TakeTimedUpload())
	{
		if (const std::optional<cpmc::LatencySample> sample = g_latency.Drawn(timed->drawn, timed->drawnFrame))
		{
			Log("latency: Minecraft {:.0f} ms, ours {:.1f} ms ({:.0f} frames)", sample->minecraftMs, sample->oursMs, sample->oursFrames);
		}
	}
}

void Place(const cpmc::Request& aRequest)
{
	if (g_minecraftAtlas)
	{
		return Report("place " + aRequest.what + ": refused while Minecraft's atlas is loaded");
	}
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

// Spike 4's settings (the spec's "Requests"). True if aRequest names one of them.
bool SetLinkSetting(const cpmc::Request& aRequest)
{
	const std::string& name = aRequest.what;
	const double       value = aRequest.number;
	if (name == "translucent")
	{
		if (value != 0.0 && value != 1.0)
		{
			Report("set translucent: 0 drops translucent triangles, and 1 draws them as cut-outs");
			return true;
		}
		g_linkSettings.keepTranslucent = value == 1.0;
	}
	else if (name == "teleportstep" || name == "teleportrate")
	{
		if (value <= 0.0)
		{
			Report(std::format("set {}: it's above 0", name));
			return true;
		}
		(name == "teleportstep" ? g_linkSettings.teleportStep : g_linkSettings.teleportRate) = value;
	}
	else if (name == "ringmb")
	{
		if (value < 1.0 || value > 64.0)
		{
			Report("set ringmb: from 1 to 64");
			return true;
		}
		g_linkSettings.ringBudget = uint64_t(value * 1048576.0);
	}
	else if (name == "hud")
	{
		if (value != 0.0 && value != 1.0 && value != 2.0)
		{
			Report("set hud: 0 never, 1 in build mode, 2 always");
			return true;
		}
		g_hudSetting = int(value);
	}
	else if (name == "hudscale")
	{
		if (value != 1.0 && value != 2.0 && value != 4.0)
		{
			Report("set hudscale: 1, 2 or 4");
			return true;
		}
		g_hudScale = uint32_t(value);
	}
	else if (name == "turn")
	{
		if (value != 0.0 && value != 1.0)
		{
			Report("set turn: 0 teleports V with its own yaw, and 1 adds the frame's turn from the mouse");
			return true;
		}
		g_turn = value == 1.0;
	}
	else
	{
		return false;
	}
	Report(std::format("set {} {}", name, value));
	return true;
}

void Set(const cpmc::Request& aRequest)
{
	if (SetLinkSetting(aRequest))
	{
		return;
	}
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

// link on|off (the spec's "Linking"): linking also switches drawing to gbuffer if it's off. aBy says what asked, when it
// wasn't a link request.
void LinkRequest(bool aOn, std::string_view aBy = {})
{
	g_linked = aOn;
	if (!aOn)
	{
		return Report("link off: Minecraft holds its player where it is, and the blocks stay");
	}
	std::string why;
	if (g_renderer->ModeName() == "off" && !g_renderer->SetMode("gbuffer", why))
	{
		return Report(std::format("{}link on, but drawing stays off: {}", aBy, why));
	}
	const std::string link = g_link ? "Minecraft's player follows V" : "it starts once the mapping opens, which is " + g_mappingState;
	Report(std::format("{}link on: {}; drawing is {}", aBy, link, g_renderer->ModeName()));
}

// build on|off (the spec's "Requests"): as F8. On links first, if it isn't linked, and goes on after the link's next
// update, once it knows whether V and Minecraft are there.
void BuildRequest(bool aOn)
{
	if (!aOn)
	{
		return Report(g_router->TurnOff("a build off request") ? "build off: Minecraft lets go of everything"
		                                                         : "build off: it's off already");
	}
	if (g_router->On())
	{
		return Report("build on: it's on already");
	}
	if (!g_linked)
	{
		LinkRequest(true, "build on: ");
	}
	g_buildWanted = true;
}

void TypeRequest(const std::string& aWhat)
{
	if (!g_link)
	{
		return Report("mc: there's no link: the mapping is " + g_mappingState);
	}
	std::string error;
	if (!g_link->Type(aWhat, error))
	{
		return Report("mc: " + error);
	}
	g_lastTyped = aWhat;
	Report("mc " + aWhat + ": typing it into Minecraft's chat");
}

// The command in aFolder's blockrender.request, if there is one: in the plugin's folder, where the agent writes it, or
// the overlay's, where its buttons do. The overlay can't rename a finished file into place, so there a command counts
// only once its line ends (aWholeLine).
void RunRequest(const std::filesystem::path& aFolder, bool aWholeLine)
{
	const std::filesystem::path path = aFolder / "blockrender.request";
	std::error_code             error;
	if (!std::filesystem::exists(path, error))
	{
		return;
	}
	const std::string text = ReadSmallFile(path);
	if (aWholeLine && (text.empty() || text.back() != '\n'))
	{
		return;  // still being written: read it next second
	}
	std::filesystem::remove(path, error);
	std::string                        why;
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
		g_renderer->ClearSections();
		Report(g_minecraftAtlas ? "cleared: Minecraft's sections come back as they change, or when it next sends everything" : "cleared");
		break;
	case cpmc::Request::Kind::Reload: Report(g_renderer->ReloadShaders(why) ? "reload: shaders compiled" : "reload: " + why); break;
	case cpmc::Request::Kind::Set: Set(*request); break;
	case cpmc::Request::Kind::Link: LinkRequest(request->what == "on"); break;
	case cpmc::Request::Kind::Floor:
		if (!g_link)
		{
			Report("floor: there's no link: the mapping is " + g_mappingState);
			break;
		}
		g_link->RequestFloor();
		Report("floor: a fresh one at the next update, once Minecraft is there");
		break;
	case cpmc::Request::Kind::Mc: TypeRequest(request->what); break;
	case cpmc::Request::Kind::Build: BuildRequest(request->what == "on"); break;
	case cpmc::Request::Kind::DriveLog:
		Report(g_driveLog.Start(request->number) ? std::format("drivelog: recording the drive for {:g} s", request->number)
		                                         : "drivelog: one is already recording");
		break;
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

void WriteDriveLog(const cpmc::DriveLog::Finished& aLog, const std::string& aFolderName)
{
	const std::filesystem::path folder = g_pluginDir / "logs" / aFolderName;
	std::error_code             error;
	std::filesystem::create_directories(folder, error);
	std::ofstream out(folder / "drive.csv", std::ios::binary);
	out << aLog.csv;
	if (!out)
	{
		return Report("drivelog: couldn't write " + (folder / "drive.csv").string());
	}
	Report(std::format("drivelog: written to logs\\{}\\drive.csv ({} frames)", aFolderName, aLog.rows));
}

// The link's lines of the status (the spec's "Measurements"), from the second since the last status.
cpmc::LinkReport LinkLines(const cpmc::Renderer::Stats& aStats, double aNow)
{
	cpmc::LinkReport report;
	report.mapping = g_mappingState;
	report.linked = g_linked;
	report.camera = g_camera;
	const cpmc::input::InputReport input = g_router->Report();
	report.inputHooked = g_inputHooked;
	report.buildMode = input.on;
	report.buildWhy = input.why;
	report.slot = input.slot;
	report.inputsRead = input.read;
	report.inputsForwarded = input.forwarded;
	report.inputsHidden = input.hidden;
	report.lastInput = input.last;
	if (g_link)
	{
		const cpmc::link::Link::Status& state = g_link->State();
		const double                    seconds = std::max(aNow - g_second.start, 0.001);
		const uint64_t                  frames = state.minecraftFrames >= g_second.minecraftFramesAtStart
		                                             ? state.minecraftFrames - g_second.minecraftFramesAtStart
		                                             : state.minecraftFrames;  // a new Minecraft counts from 0
		report.minecraft = state.minecraft;
		report.minecraftPid = state.minecraftPid;
		report.heartbeatAgeMs = state.heartbeatAgeMs;
		report.inWorld = state.inWorld;
		report.screenOpen = state.screenOpen;
		report.minecraftFps = double(frames) / seconds;
		report.inGame = state.inGame;
		report.minecraftEye = state.minecraftEye;
		report.teleports = state.teleports - g_second.teleportsAtStart;
		report.target = state.target;
		report.floorEpoch = state.floorEpoch;
		report.floorSent = state.floorSent;
		report.floorCentre = state.floorCentre;
		report.inputsDropped = state.inputsDropped;
		report.ringMb = double(g_second.bytes) / 1048576.0;
		report.ringMbWorstFrame = double(g_second.bytesWorstFrame) / 1048576.0;
		report.drainMsAverage = g_second.frames ? g_second.drainMsSum / double(g_second.frames) : 0.0;
		report.drainMsWorst = g_second.drainMsWorst;
		for (size_t type = 0; type < cpmc::link::Link::kTypes; ++type)
		{
			const uint64_t messages = state.messages[type] - g_second.messagesAtStart[type];
			if (messages != 0)
			{
				report.ringTypes += std::format("{}{} {} ({:.2f} MB)", report.ringTypes.empty() ? "" : ", ", messages,
				                                cpmc::link::RenderTypeName(type),
				                                double(state.bytes[type] - g_second.bytesAtStart[type]) / 1048576.0);
			}
		}
		report.rejected = state.rejected;
		report.resyncs = state.resyncs;
		report.translucentDropped = state.translucentDropped;
		report.hudRefused = state.hudRefused;
		report.drive = cpmc::link::DriveStateName(state.drive);
		report.driveFor = state.drive == cpmc::link::DriveState::Off ? 0.0 : aNow - state.driveSince;
		report.eyeHeight = state.eyeHeight;
		report.eyeDistanceLast = state.eyeDistance;
		report.eyeDistanceAverage = g_second.eyeDistanceFrames ? g_second.eyeDistanceSum / double(g_second.eyeDistanceFrames) : 0.0;
		report.eyeDistanceWorst = g_second.eyeDistanceWorst;
		report.renderDelayMs = state.renderDelayMs;
		report.waitedFrames = state.waitedFrames - g_second.waitedAtStart;
		report.skippedFrames = state.driveSkipped;
		report.teleportMsAverage = g_second.teleportCalls ? g_second.teleportMsSum / double(g_second.teleportCalls) : -1.0;
		report.teleportMsWorst = g_second.teleportCalls ? g_second.teleportMsWorst : -1.0;
		report.minecraftFlags = state.minecraftFlags;
	}
	report.health = g_linked ? g_puppet.Health() : std::nullopt;  // once a second, so fall damage shows
	report.turn = g_turn;
	report.turnRatio = g_turnRatio.Ratio();
	report.turnTurns = g_turnRatio.Turns();
	report.atlas = aStats.atlasWidth == 0 ? "none yet"
	                                      : std::format("{} x {}, {} levels, {}", aStats.atlasWidth, aStats.atlasHeight, aStats.atlasLevels,
	                                                    g_minecraftAtlas ? "Minecraft's" : "atlas.bin's");
	report.updateMsAverage = aStats.updateMsAverage;
	report.updateMsWorst = aStats.updateMsWorst;
	report.hudSetting = g_hudSetting;
	report.hudScale = g_hudScale;
	report.hudWanted = g_hudWanted;
	report.hudReceived = aStats.hudReceived;
	report.hudMb = double(aStats.hudBytes) / 1048576.0;
	report.hudDropped = aStats.hudDropped;
	report.hudDrawn = aStats.hudDrawn;
	report.hudSize = aStats.hudWidth == 0 ? ""
	                                      : std::format("{} x {}, rows {}", aStats.hudWidth, aStats.hudHeight,
	                                                    aStats.hudBottomUp ? "bottom-up" : "top-down");
	report.hudCopyMsAverage = aStats.hudCopyMsAverage;
	report.hudGpuMsAverage = aStats.hudGpuMsAverage;
	report.hudSkipped = aStats.hudSkipped;
	report.hudLastSkip = aStats.hudLastSkip;
	report.latencyLast = g_latency.Last();
	report.latencyAverage = g_latency.Average();
	report.latencySamples = g_latency.Samples();
	report.burst = g_lastBurst;
	report.cyberpunkCpu = g_cyberpunkProcess.CpuShare();
	report.cyberpunkMemory = g_cyberpunkProcess.WorkingSet();
	report.minecraftProcessKnown = g_minecraftProcess.Known();
	report.minecraftCpu = g_minecraftProcess.CpuShare();
	report.minecraftMemory = g_minecraftProcess.WorkingSet();
	return report;
}

// The next second's figures start from where the link is now.
void StartSecond(double aNow)
{
	g_second = {};
	g_second.start = aNow;
	if (g_link)
	{
		const cpmc::link::Link::Status& state = g_link->State();
		g_second.teleportsAtStart = state.teleports;
		g_second.minecraftFramesAtStart = state.minecraftFrames;
		g_second.messagesAtStart = state.messages;
		g_second.bytesAtStart = state.bytes;
		g_second.waitedAtStart = state.waitedFrames;
	}
}

void WriteStatus(double aNow)
{
	g_cyberpunkProcess.Sample(GetCurrentProcessId(), aNow);
	g_minecraftProcess.Sample(g_link && g_link->State().minecraft ? g_link->State().minecraftPid : 0, aNow);  // not one that's gone
	const cpmc::Renderer::Stats stats = g_renderer->TakeStats();
	cpmc::StatusReport          report;
	report.mode = g_renderer->ModeName();
	report.framesDrawn = stats.drawn;
	report.framesPartial = stats.partial;
	report.framesSkipped = stats.skipped;
	report.lastSkip = stats.lastSkip;
	report.lateCamera = stats.lateCamera;
	report.sections = stats.sections;
	report.blocks = g_world.BlockCount();
	report.minecraftBlocks = g_minecraftAtlas;
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
	report.link = LinkLines(stats, aNow);
	StartSecond(aNow);
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

// Every frame on the main thread while the game runs (RED4ext ignores the return value in this state), in the spec's
// order: requests once a second, the link, the frame log and the dumps, meshing and the renderer's Update, the log,
// and the status once a second.
bool OnUpdate(RED4ext::CGameApplication*)
{
	const auto   now = Clock::now();
	const double seconds = cpmc::SteadySeconds();
	const double frameMs = g_lastFrameAt > 0.0 ? (seconds - g_lastFrameAt) * 1000.0 : 0.0;
	g_lastFrameAt = seconds;
	const bool newSecond = now - g_lastSecond >= 1s;
	if (newSecond)
	{
		g_lastSecond = now;
		if (g_hooked)
		{
			PatchLate();
			CheckDevice();
		}
		if (!g_link)
		{
			OpenMapping();
		}
		RunRequest(g_pluginDir, false);
		RunRequest(g_hudDir, true);
	}
	if (g_link && g_router->TakeLinkRequest())
	{
		LinkRequest(true, "F8: ");  // F8 while unlinked: the link, then build mode once the link has seen V
		g_buildWanted = true;
	}
	UpdateLink(seconds, frameMs);
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
	if (const std::optional<cpmc::DriveLog::Finished> finished = g_driveLog.TakeFinished())
	{
		WriteDriveLog(*finished, folderName);
	}
	g_dumps->Update(folderName);
	if (g_minecraftAtlas)
	{
		g_world.TakeDirty();  // spike 3's world stays empty, and isn't meshed with Minecraft's atlas
	}
	else if (const cpmc::Atlas* atlas = g_renderer->AtlasData())
	{
		for (const cpmc::SectionPos& section : g_world.TakeDirty())
		{
			g_renderer->UploadSection(section, cpmc::MeshSection(g_world, section, *atlas, g_meshOptions));
		}
	}
	const double updateStart = cpmc::SteadySeconds();
	g_renderer->Update();
	const double updateMs = (cpmc::SteadySeconds() - updateStart) * 1000.0;
	const cpmc::BurstFrame burstFrame{ g_frame.sections, g_frame.bytes, g_frame.drainMs, updateMs, frameMs };
	if (const std::optional<cpmc::BurstReport> burst = g_burst.Frame(seconds, burstFrame))
	{
		g_lastBurst = burst;
		Log("burst after {}: {} sections, {:.2f} MB over {} frames; worst drain {:.3f} ms, update {:.3f} ms, frame {:.1f} ms", burst->what,
		    burst->sections, double(burst->bytes) / 1048576.0, burst->frames, burst->drainMsWorst, burst->updateMsWorst,
		    burst->frameMsWorst);
	}
	DrainLog();
	if (newSecond)
	{
		for (const std::string& line : cpmc::d3d::DebugMessages())
		{
			Log("{}", line);
		}
		WriteStatus(seconds);
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
		// Input first: it passes everything until the user links or presses F8.
		g_router = new cpmc::input::Router(g_log);
		g_inputHooked = cpmc::input::InstallRawInputHook(*g_router, g_log);
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
		if (g_router)
		{
			g_router->TurnOff("the plugin unloads");  // Minecraft lets go of everything
			cpmc::input::RemoveRawInputHook();
			g_router->SetLink(nullptr);  // from here on, nothing pushes into the mapping
		}
		if (g_renderer)
		{
			g_renderer->Shutdown();
		}
		cpmc::d3d::RemovePatches();
		cpmc::slhooks::Remove();
		delete g_link;  // main thread only, as everything of the link's
		g_link = nullptr;
		g_mapping.Close();  // Minecraft sees the heartbeat stop
		DrainLog();
		break;
	}
	return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* aInfo)
{
	aInfo->name = L"BlockRender";
	aInfo->author = L"Cyberpunk-Minecraft";
	aInfo->version = RED4EXT_V1_SEMVER(0, 11, 0);
	aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
	aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
	return RED4EXT_API_VERSION_1;
}
