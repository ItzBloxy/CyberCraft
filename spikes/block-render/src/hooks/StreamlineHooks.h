#pragma once

#include "LogQueue.h"
#include "hooks/HookEvents.h"
#include "sl/Sl.h"

#include <d3d12.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The Streamline side of the hooks (the spec's "Hooks"): the game exe's imports of slGetNewFrameToken, slSetConstants,
// slSetTag and slEvaluateFeature from sl.interposer.dll point at these, which record what they're given, tell the
// listeners, and call Streamline.
namespace cpmc::slhooks
{
// Patches the four imports. False, with nothing patched, if the exe doesn't import them all by name.
bool Install(LogQueue& aLog);
void Remove();

void AddListener(HookEvents* aListener);

uint64_t FrameTokens();  // how many frames Streamline has started

// The native object under a Streamline proxy (a device, queue, list or swap chain), through slGetNativeInterface,
// with one reference the caller owns. Null if aObject's vtable isn't in sl.interposer.dll.
void* NativeInterface(void* aObject);

// The native D3D12 list under a list the game handed Streamline (itself, or the list inside a Streamline proxy).
// Null if it's neither. When it isn't null, the caller owns one reference to it.
ID3D12GraphicsCommandList* NativeList(void* aGameList);

std::string Summary();
}
