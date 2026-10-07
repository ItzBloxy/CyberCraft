#include "input/MessageHook.h"

#include "hooks/ImportPatch.h"

#include <Windows.h>

#include <optional>

namespace cpmc::input
{
namespace
{
using PeekMessageWFn = BOOL(WINAPI*)(LPMSG, HWND, UINT, UINT, UINT);

PeekMessageWFn             g_original = nullptr;
Router*                    g_router = nullptr;
std::optional<ImportPatch> g_patch;

BOOL WINAPI PeekMessageHook(LPMSG aMessage, HWND aWindow, UINT aFirst, UINT aLast, UINT aRemove)
{
	const BOOL taken = g_original(aMessage, aWindow, aFirst, aLast, aRemove);
	// Only a message taken off the queue: a peek that leaves it there would see it again.
	if (taken && aMessage && (aRemove & PM_REMOVE) && aMessage->message == WM_CHAR)
	{
		g_router->Char(uint16_t(aMessage->wParam));  // a UTF-16 code unit, from the W function
	}
	return taken;
}
}

bool InstallMessageHook(Router& aRouter, LogQueue& aLog)
{
	std::optional<ImportPatch> patch = FindImport(GetModuleHandleW(nullptr), "user32.dll", "PeekMessageW");
	if (!patch)
	{
		aLog.Push("the game doesn't import PeekMessageW by name: typing in Minecraft's screens won't work");
		return false;
	}
	// Both before the patch, so no call reaches the replacement without them.
	g_router = &aRouter;
	g_original = reinterpret_cast<PeekMessageWFn>(patch->original);
	if (!ApplyImport(*patch, reinterpret_cast<void*>(&PeekMessageHook)))
	{
		aLog.Push("PeekMessageW's import couldn't be patched: typing in Minecraft's screens won't work");
		return false;
	}
	g_patch = *patch;
	aLog.Push("PeekMessageW hooked: what's typed reaches Minecraft's screens");
	return true;
}

void RemoveMessageHook()
{
	if (g_patch)
	{
		RestoreImport(*g_patch);
		g_patch.reset();
	}
}
}
