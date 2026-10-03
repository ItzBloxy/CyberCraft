#pragma once

#include "LogQueue.h"
#include "input/Router.h"

// The patch on Cyberpunk2077.exe's import of GetRawInputData (the spec's "Input"). The game reads every key and click
// through it, in one place: its input system's window-message handler, for each WM_INPUT. The replacement calls the
// original, then hands the event it copied to the router, which hides from the game what Minecraft takes. The focus
// watch's thread starts and stops with it. Only the game can test this file.
namespace cpmc::input
{
// Patches the import, and starts the focus watch. False, with nothing patched, if the exe doesn't import it by name.
bool InstallRawInputHook(Router& aRouter, LogQueue& aLog);

// Puts the import back, and stops the focus watch. A call already in the replacement finishes through the router,
// which outlives it, as the focus watch does.
void RemoveRawInputHook();
}
