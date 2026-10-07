#pragma once

#include "LogQueue.h"
#include "input/Router.h"

// The patch on Cyberpunk2077.exe's import of PeekMessageW (spike 8's "Typed text"). The game's one message pump takes
// every message through it with PM_REMOVE, then calls TranslateMessage, which makes each key's WM_CHAR, and
// DispatchMessageW; its other call only peeks for sent messages. The replacement calls the original, then hands each
// WM_CHAR it took off the queue to the router, which types it into Minecraft's screen while one is build mode's. The
// game gets every message unchanged. Only the game can test this file.
namespace cpmc::input
{
// Patches the import. False, with nothing patched, if the exe doesn't import it by name.
bool InstallMessageHook(Router& aRouter, LogQueue& aLog);

// Puts the import back. A call already in the replacement finishes through the router, which outlives it.
void RemoveMessageHook();
}
