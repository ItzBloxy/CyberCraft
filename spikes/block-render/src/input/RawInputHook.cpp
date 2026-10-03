#include "input/RawInputHook.h"

#include "hooks/ImportPatch.h"
#include "input/FocusWatch.h"

#include <Windows.h>

#include <atomic>
#include <optional>

namespace cpmc::input
{
namespace
{
using GetRawInputDataFn = UINT(WINAPI*)(HRAWINPUT, UINT, LPVOID, PUINT, UINT);

GetRawInputDataFn          g_original = nullptr;
Router*                    g_router = nullptr;
std::optional<ImportPatch> g_patch;
std::atomic<bool>          g_watching{ false };

UINT WINAPI GetRawInputDataHook(HRAWINPUT aInput, UINT aCommand, LPVOID aData, PUINT aSize, UINT aHeaderSize)
{
	const UINT copied = g_original(aInput, aCommand, aData, aSize, aHeaderSize);
	// Only an event copied out, with the header this build knows. A size query (no buffer), or a failure, passes.
	if (aCommand == RID_INPUT && aData && aHeaderSize == sizeof(RAWINPUTHEADER) && copied != UINT(-1) && copied != 0)
	{
		g_router->Inspect(aData, copied);
	}
	return copied;
}

// The focus watch: ten times a second, whether the game has just stopped being in front. Nothing waits for it to end.
DWORD WINAPI WatchFocus(LPVOID)
{
	FocusWatch watch;
	while (g_watching.load())
	{
		Sleep(100);
		if (watch.LostFront(GameInFront()) && g_watching.load())
		{
			g_router->FrontLost();
		}
	}
	return 0;
}
}

bool InstallRawInputHook(Router& aRouter, LogQueue& aLog)
{
	std::optional<ImportPatch> patch = FindImport(GetModuleHandleW(nullptr), "user32.dll", "GetRawInputData");
	if (!patch)
	{
		aLog.Push("the game doesn't import GetRawInputData by name: input isn't hooked, so build mode can't work");
		return false;
	}
	// Both before the patch, so no call reaches the replacement without them.
	g_router = &aRouter;
	g_original = reinterpret_cast<GetRawInputDataFn>(patch->original);
	if (!ApplyImport(*patch, reinterpret_cast<void*>(&GetRawInputDataHook)))
	{
		aLog.Push("GetRawInputData's import couldn't be patched: input isn't hooked, so build mode can't work");
		return false;
	}
	g_patch = *patch;
	g_watching = true;
	const HANDLE thread = CreateThread(nullptr, 0, &WatchFocus, nullptr, 0, nullptr);
	if (thread)
	{
		CloseHandle(thread);
	}
	aLog.Push(thread ? "GetRawInputData hooked; the focus watch runs"
	                 : "GetRawInputData hooked, but the focus watch didn't start: switching windows won't let go of held inputs");
	return true;
}

void RemoveRawInputHook()
{
	g_watching = false;
	if (g_patch)
	{
		RestoreImport(*g_patch);
		g_patch.reset();
	}
}
}
