// PhysXProbe: a read-only RED4ext plugin that checks whether Cyberpunk's PhysX 3.4.2 can be read through
// NVIDIA's public headers and dumps the collision around V. See docs/superpowers/specs/2026-10-01-physx-probe-design.md.
#include <RED4ext/RED4ext.hpp>

#include "DumpWriter.h"
#include "PlayerLocator.h"
#include "Probe.h"

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <format>
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
std::optional<Clock::time_point> g_firstSeenPlayer;
bool                        g_autoDone = false;

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

void RunOne(const std::string& aTrigger)
{
	const auto pose = cpmc::LocatePlayer();
	if (!pose)
	{
		Log("{} trigger: no player", aTrigger);
		return;
	}
	const auto [time, folderName] = Timestamps();
	LARGE_INTEGER start;
	LARGE_INTEGER end;
	LARGE_INTEGER frequency;
	QueryPerformanceCounter(&start);
	cpmc::DumpData data = cpmc::RunDump(g_session, *pose, aTrigger, g_runtime, time);
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

	const auto  now = Clock::now();
	std::string trigger;

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
			std::filesystem::remove(request, error);
			trigger = "request";
		}
		if (!g_autoDone)
		{
			if (!g_firstSeenPlayer && cpmc::LocatePlayer())
			{
				g_firstSeenPlayer = now;
			}
			if (trigger.empty() && g_firstSeenPlayer && now - *g_firstSeenPlayer >= 10s)
			{
				g_autoDone = true;
				trigger = "auto";
			}
		}
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
	RunOne(trigger);
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
		Log("PhysXProbe 0.1.0 loaded (game {}); F10 or {} triggers a dump", g_runtime, (g_pluginDir / "dump.request").string());
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
	aInfo->version = RED4EXT_V1_SEMVER(0, 1, 0);
	aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
	aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
	return RED4EXT_API_VERSION_1;
}
