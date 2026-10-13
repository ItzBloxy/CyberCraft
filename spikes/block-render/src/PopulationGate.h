#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

// Build mode's teleports of V, kept from stopping the game's NPC spawning.
//
// Each teleport of V tells the game's population system that the player was teleported: the puppet's teleport calls
// the system's virtual function 97, which only sets a byte (`mov byte ptr [rcx+0x20b4], 1; ret`), and nothing else
// calls it. On its next update the system clears the byte and skips all of its spawning for that frame. Build mode
// teleports V every frame, so no NPC ever spawned near a V in build mode, while those already there walked off and
// despawned. (Read from the 2.31 exe: docs/spikes/2026-10-13-npc-spawning-in-build-mode.md has the chain and the runs.)
//
// Build mode's teleports move V by centimetres, as walking does, so the gate swaps that vtable entry for one that skips
// the notice for a moment after each of them, and calls the game's own function at any other time. The system's own
// check for a big move (over 20 m) still runs every frame. Nothing is patched unless the entry holds exactly the code
// above, in the game's own image.
namespace cpmc::population
{
// The notice's code, as the game has it: `mov byte ptr [rcx+0x20b4], 1; ret`.
inline constexpr std::array<uint8_t, 8> kNoticeCode{ 0xC6, 0x81, 0xB4, 0x20, 0x00, 0x00, 0x01, 0xC3 };
inline constexpr size_t                 kNoticeSlot = 97;  // the notice's entry in the population system's vtable

// How long after build mode's last teleport the notice is skipped. The teleport is queued and applied later by the
// teleportation facility's job, within a frame or two, so the gate outlasts the request it covers.
inline constexpr double kHoldSeconds = 0.25;

// Where the game's own code and tables are, for checking that the entry is the game's before it's touched.
struct Image
{
	uintptr_t begin = 0;
	uintptr_t end = 0;  // one past the last byte
};

// True when aCode starts with kNoticeCode. Pure.
bool IsNoticeCode(const uint8_t* aCode);

// True when aNow is within kHoldSeconds after aLastTeleport, both QueryPerformanceCounter values, with aPerSecond
// counts a second. Never true before the first teleport (aLastTeleport 0). Pure.
bool Muted(int64_t aNow, int64_t aLastTeleport, int64_t aPerSecond);

// Swaps entry kNoticeSlot of aSystem's vtable for the gate. Empty when the gate is in, or already was; otherwise why it
// isn't, with nothing changed. aSystem is the game's population system (gameIPopulationSystem), aImage its executable's.
std::string Install(void* aSystem, const Image& aImage);

// Puts the game's own function back. Returns how many entries it restored (0 or 1).
size_t Restore();

// Build mode is about to teleport V: the notice is skipped from now until kHoldSeconds after the last such call. Any
// thread.
void NoteTeleport();

struct Counts
{
	bool     installed = false;
	uint64_t muted = 0;   // notices skipped since the gate went in
	uint64_t passed = 0;  // notices passed on to the game's function since then
};
Counts Read();
}
