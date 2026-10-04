#pragma once

#include "physx/PxHeaders.h"
#include "physx/SceneWatch.h"

#include <cstdint>
#include <string>

// Step 3's scene watch, in the game (spike 5's "The scene watch"): hooks on 19 audited slots of scene 0's vtable.
// Each hook records its call and calls on: no lock, no allocation, on whichever thread the game calls from. The small
// five-plane scenes share the vtable; their calls go straight on, unrecorded. Main thread only, but for the hooks.
namespace cpmc::px
{
// Hooks aScene's vtable, once each slot is checked to hold the audited function (aPhysx3: where PhysX3_x64.dll is
// loaded). False, with aWhy, if one doesn't, as when another hook is there already: then nothing is hooked. The record
// starts afresh: it follows the write lock at once, and counts calls from a second later.
bool InstallSceneHooks(physx::PxScene* aScene, uintptr_t aPhysx3, std::string& aWhy);
// Puts each original back. The hooks stay in memory, as the plugin is pinned, so a thread that read one from the vtable
// just before still calls on.
void RemoveSceneHooks();
bool SceneHooksInstalled();

// OnUpdate is starting (true) or ending (false): calls in between count as made while OnUpdate ran. The first start a
// second or more after the hooks go in starts the count.
void SetInUpdate(bool aIn);

const SceneRecorder& Recorder();
}
