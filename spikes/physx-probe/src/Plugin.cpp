// PhysXProbe: a read-only RED4ext plugin that checks whether Cyberpunk's PhysX 3.4.2 can be read through NVIDIA's
// headers, corrected where the game's build differs, and dumps the collision around V. F10 or a request file starts a
// dump; there is no automatic one. See docs/superpowers/specs/2026-10-01-physx-probe-design.md.
#include <RED4ext/RED4ext.hpp>

#include "DumpRequest.h"
#include "DumpWriter.h"
#include "OriginWatch.h"
#include "PlayerLocator.h"
#include "Probe.h"
#include "physx/Checks.h"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <string>

namespace
{
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

RED4ext::v1::PluginHandle   g_handle = nullptr;
const RED4ext::v1::Sdk*     g_sdk = nullptr;
std::filesystem::path       g_pluginDir;
cpmc::px::PhysxSession      g_session;
cpmc::DumpWriter            g_writer;
std::string                 g_runtime;

bool                        g_f10WasDown = false;
Clock::time_point           g_lastDump{};
Clock::time_point           g_lastSecondlyCheck{};

template<class... Args>
void Log(std::format_string<Args...> aFormat, Args&&... aArgs)
{
	if (g_sdk)
	{
		g_sdk->logger->Info(g_handle, std::format(aFormat, std::forward<Args>(aArgs)...).c_str());
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

bool GameInForeground()
{
	DWORD processId = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &processId);
	return processId == GetCurrentProcessId();
}

// "2026-10-01T12:34:56" for the report, "2026-10-01_12-34-56" for the folder.
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

void RunOne(const std::string& aTrigger, const std::optional<cpmc::Vec3d>& aPhysxOrigin)
{
	const auto pose = cpmc::LocatePlayer();
	if (!pose)
	{
		Log("{} trigger: no player", aTrigger);
		return;
	}
	if (aPhysxOrigin)
	{
		Log("{} trigger: reading PhysX at world - ({:.3f}, {:.3f}, {:.3f})", aTrigger, aPhysxOrigin->x, aPhysxOrigin->y,
		    aPhysxOrigin->z);
	}
	const auto [time, folderName] = Timestamps();
	LARGE_INTEGER start;
	LARGE_INTEGER end;
	LARGE_INTEGER frequency;
	QueryPerformanceCounter(&start);
	cpmc::DumpData data = cpmc::RunDump(g_session, *pose, aTrigger, g_runtime, time, aPhysxOrigin);
	QueryPerformanceCounter(&end);
	QueryPerformanceFrequency(&frequency);
	data.report.mainThreadPauseMs = double(end.QuadPart - start.QuadPart) * 1000.0 / double(frequency.QuadPart);

	const auto& report = data.report;
	Log("dump ({}): compatibility {}, {} scene(s), {} triangles, coordinate check {}, main-thread pause {:.1f} ms, {} object(s) skipped",
	    aTrigger, report.compatibilityPass ? "passed" : "FAILED", report.scenes.size(),
	    report.region ? report.region->triangles : 0,
	    report.coordinateCheck ? (report.coordinateCheck->pass ? "passed" : "FAILED") : "not run", report.mainThreadPauseMs,
	    report.censusSkipped);
	for (const auto& step : report.steps)
	{
		if (!step.pass)
		{
			Log("  compatibility step {} ({}) failed: {}", step.step, step.name, step.detail);
		}
	}
	cpmc::DumpJob job{ g_pluginDir / "dumps" / folderName, std::move(data.report), std::move(data.groups), pose->feet };
	g_writer.Start(std::move(job));
}

// The origin watch: while it's on, V's controller tells where PhysX's origin is, every frame. Each change is logged
// with V's position in the frames before and after, plus a status line every 2 seconds.
struct OriginWatch
{
	bool                       on = false;
	uint64_t                   frame = 0;
	std::optional<cpmc::Vec3d> origin;
	std::optional<cpmc::Vec3d> lastFeet;
	const void*                controller = nullptr;
	cpmc::Vec3d                residual;
	double                     largestAcross = 0.0;  // since the last status line
	uint64_t                   framesSinceStatus = 0;
	uint64_t                   searchesSinceStatus = 0;
	Clock::time_point          lastStatus{};
};

OriginWatch g_watch;

std::string Point(const cpmc::Vec3d& aPoint)
{
	return std::format("({:.3f}, {:.3f}, {:.3f})", aPoint.x, aPoint.y, aPoint.z);
}

std::string Origin(const cpmc::Vec3d& aOrigin)
{
	return std::format("({:.0f}, {:.0f}, {:.0f})", aOrigin.x, aOrigin.y, aOrigin.z);
}

void WatchFrame(Clock::time_point aNow)
{
	++g_watch.frame;
	++g_watch.framesSinceStatus;
	const auto pose = cpmc::LocatePlayer();
	if (!pose)
	{
		return;
	}
	// A search costs a millisecond or two, so while V's controller is lost (in a vehicle, say) look only every 10th frame.
	const cpmc::Vec3d                               feet = pose->feet;
	const bool                                      look = g_watch.controller != nullptr || g_watch.frame % 10 == 0;
	const std::optional<cpmc::px::ControllerSample> sample = look ? g_session.ReadController(feet) : std::nullopt;
	if (!sample)
	{
		if (g_watch.controller)
		{
			Log("watch: lost V's controller at V {}, frame {}", Point(feet), g_watch.frame);
			g_watch.controller = nullptr;
		}
	}
	else
	{
		if (sample->searched)
		{
			++g_watch.searchesSinceStatus;
			if (sample->actor != g_watch.controller)
			{
				Log("watch: V's controller is {} (radius {:.2f}, half-height {:.2f}), frame {}", sample->actor, sample->radius,
				    sample->halfHeight, g_watch.frame);
			}
		}
		g_watch.controller = sample->actor;
		const cpmc::OriginReading reading = cpmc::OriginFromController(feet, sample->centre);
		g_watch.residual = reading.residual;
		g_watch.largestAcross = std::max(g_watch.largestAcross, std::hypot(reading.residual.x, reading.residual.y));
		const cpmc::Vec3d& origin = reading.origin;
		if (!g_watch.origin)
		{
			Log("watch: origin {} at V {}, frame {}", Origin(origin), Point(feet), g_watch.frame);
		}
		else if (origin.x != g_watch.origin->x || origin.y != g_watch.origin->y || origin.z != g_watch.origin->z)
		{
			Log("watch: origin {} -> {} between V {} and V {}, frame {}, residual {}", Origin(*g_watch.origin), Origin(origin),
			    g_watch.lastFeet ? Point(*g_watch.lastFeet) : std::string("(unknown)"), Point(feet), g_watch.frame,
			    Point(reading.residual));
		}
		g_watch.origin = origin;
	}
	g_watch.lastFeet = feet;
	if (aNow - g_watch.lastStatus >= 2s)
	{
		g_watch.lastStatus = aNow;
		Log("watch: V {}, origin {}, residual {}, largest across {:.3f} m, {} frames, {} searches", Point(feet),
		    g_watch.origin ? Origin(*g_watch.origin) : std::string("unknown"),
		    g_watch.controller ? Point(g_watch.residual) : std::string("(no controller)"), g_watch.largestAcross,
		    g_watch.framesSinceStatus, g_watch.searchesSinceStatus);
		g_watch.largestAcross = 0.0;
		g_watch.framesSinceStatus = 0;
		g_watch.searchesSinceStatus = 0;
	}
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
	if (auto written = g_writer.TakeResult())
	{
		if (written->ok)
		{
			Log("dump written to {} ({} triangles, {:.0f} ms)", written->folder.string(), written->triangles, written->writeMs);
		}
		else
		{
			Log("couldn't write the dump to {}: {}", written->folder.string(), written->error);
		}
	}

	const auto                 now = Clock::now();
	std::string                trigger;
	std::optional<cpmc::Vec3d> physxOrigin;

	const bool f10Down = (GetAsyncKeyState(VK_F10) & 0x8000) != 0 && GameInForeground();
	if (f10Down && !g_f10WasDown)
	{
		trigger = "hotkey";
	}
	g_f10WasDown = f10Down;

	if (now - g_lastSecondlyCheck >= 1s)
	{
		g_lastSecondlyCheck = now;
		const auto      request = g_pluginDir / "dump.request";
		std::error_code error;
		if (trigger.empty() && std::filesystem::exists(request, error))
		{
			const std::string text = ReadSmallFile(request);
			std::filesystem::remove(request, error);
			if (const std::optional<cpmc::DumpRequest> parsed = cpmc::ParseDumpRequest(text))
			{
				switch (parsed->action)
				{
				case cpmc::DumpRequest::Action::Dump:
					trigger = "request";
					physxOrigin = parsed->physxOrigin;
					break;
				case cpmc::DumpRequest::Action::StartWatch:
					g_watch = OriginWatch{};
					g_watch.on = true;
					g_watch.lastStatus = now;
					Log("watch: started");
					break;
				case cpmc::DumpRequest::Action::StopWatch:
					Log("watch: stopped after {} frames", g_watch.frame);
					g_watch.on = false;
					break;
				}
			}
			else
			{
				Log("dump.request ignored: expected nothing, \"origin <x> <y> <z>\", \"watch\" or \"stop\", got \"{}\"",
				    cpmc::px::SanitizeName(text.c_str(), 200));
			}
		}
	}

	if (g_watch.on)
	{
		WatchFrame(now);
	}

	if (trigger.empty() || now - g_lastDump < 2s)
	{
		return false;
	}
	if (g_writer.Busy())
	{
		Log("{} trigger ignored: the previous dump is still being written", trigger);
		return false;
	}
	g_lastDump = now;
	RunOne(trigger, physxOrigin);
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
		g_pluginDir = PluginFolder();
		g_runtime = std::format("{}.{}.{}", unsigned(aSdk->runtime->major), aSdk->runtime->minor, aSdk->runtime->patch);
		static RED4ext::v1::GameState running{ &OnEnter, &OnUpdate, &OnExit };
		aSdk->gameStates->Add(aHandle, RED4ext::EGameStateType::Running, &running);
		Log("PhysXProbe 0.4.0 loaded (game {}); F10 or {} triggers a dump, and \"watch\" or \"stop\" in that file starts or "
		    "stops the origin watch",
		    g_runtime, (g_pluginDir / "dump.request").string());
		break;
	}
	case RED4ext::v1::EMainReason::Unload:
		g_writer.Join();
		break;
	}
	return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* aInfo)
{
	aInfo->name = L"PhysXProbe";
	aInfo->author = L"Cyberpunk-Minecraft";
	aInfo->version = RED4EXT_V1_SEMVER(0, 4, 0);
	aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
	aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
	return RED4EXT_API_VERSION_1;
}
