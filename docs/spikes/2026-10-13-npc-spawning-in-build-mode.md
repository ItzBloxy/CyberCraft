# NPCs stopped spawning around V in build mode: results

A bug, not a spike. NPCs thinned out around V in build mode until none were left, and FreeFly (the user's flying mod)
did the same. Both teleport V every frame. Found with BlockRender 0.23.0 and fixed in 0.24.0
(`spikes/block-render/src/PopulationGate.*`).

## The answer

> Why do NPCs vanish around V in build mode?

**Nothing despawns them: the game stops spawning new ones.** Every teleport of V tells the game's population system
that the player was teleported, and its next update skips all of its spawning. Build mode teleports V every frame, so
the spawning never ran. The NPCs already there walked off or despawned as usual, and none replaced them. FreeFly hits
the same chain. No setting copied from FreeFly was left on: of the four things FreeFly does while flying (teleport V,
set V's health to 100, `NoMovement` and `NoZooming`, holster the weapon), BlockRender copies only the teleport.

**The fix:** BlockRender 0.24.0 gates that one notice, only for a quarter-second after each of build mode's
teleports. NPCs now spawn on through build mode, and there's no burst when it ends. Every other teleport, FreeFly's
included, still reaches the game as before: the user chose build mode only.

## The runs

Status sampled 4 times a second (`npcs=` line: the NPCs listed within 80 m, the NPCs the game has attached, the
nearest).

**Before (0.23.0), 2026-10-13 18:08:**
- **Standing still in build mode for 27 s:** the world's count only fell, 66 → 52. Not one NPC attached. Outside
  build mode it rose and fell every second.
- **The backlog:** at F8 off, +18 NPCs within a second. After a longer build-mode run, 15 → 77 within a second.
- **The NPCs already there stayed.** The user saw them walk out of range and none come in, so the area emptied.

**After (0.24.0), 18:49 to 19:16:**

| Build mode | Length | World's count | Seconds it rose / fell | Nearest (median) | At F8 off |
|---|---|---|---|---|---|
| 18:49 | 53 s | 59 → 55 | 24 / 21 | 7.1 m | 56 |
| 18:50 | 205 s | 62 → 48 | 69 / 80 | 12.1 m | 50 |
| 18:53, V nearly still | 136 s | 49 → 53 | 40 / 41 | 11.0 m | 53 |
| 18:56, V nearly still | 466 s | 52 → 47 | 58 / 101 | 7.5 m | 49 |
| 19:04 | 268 s | 55 → 59 | 79 / 80 | 5.7 m | 59 |

- **Muted and passed on:** 88,389 notices muted in build mode, about one a frame, and 1,673 passed on, all outside
  build mode.
- **Health:** the gate went in the first time build mode placed V. There were no errors in the log and no crash
  report.

## The chain, in the 2.31 exe

From the Epic build's `Cyberpunk2077.exe` (FileVersion 3.0.5294808), with the scripts in `tools/re/`:

1. **The native wrapper.** `TeleportationFacility.Teleport`'s native, `0x1413a09dc`, reads its 3 arguments and calls
   the facility's vtable (`0x142cb1208`) slot 54.
2. **The queue.** Slot 54, `0x1413a0adc`, queues `{V's handle, position, angles}` in a ring buffer of 0x30-byte
   requests at facility `+0x64`, under a byte spinlock at `+0x60`. Nothing moves yet.
3. **The drain.** A job labelled `TeleportationFacility_Tick` (`0x141cd0140` → `0x141cd1db0`) drains the ring
   (`0x1406847c8`). For each request, `0x140804b80` calls the entity's vtable slot 67.
4. **V's teleport.** V is a script-only `PlayerPuppet`, so its native class is `gamePuppet`, whose slot 67 is
   `0x140804cf0`. It calls `0x140804df0`, which sets the root transform. Then, if the puppet's byte `+0x1fc` has bit 4
   (V's does), it gets the system typed `gameIPopulationSystem` and calls its vtable slot 97.
5. **The notice.** `gamePopulationSystem`'s slot 97, `0x140804ea0`, is `mov byte ptr [rcx+0x20b4], 1; ret`. Nothing
   else in the exe calls slot 97 on the population system.
6. **The skip.** The population system's update, `0x1403a7898`, clears that byte and jumps past all of its spawn
   work for the frame, including its 3 KB tick, `0x140213874`.

**What doesn't hold spawns:**
- **The population system's own big-move branch** is distance-based: more than 20 m (distance squared 400) from its
  reference point. Build mode's moves are centimetres.
- **Streaming's teleport detection** is separate, and distance-based too: `World/StreamingTeleportMagSq`, default
  4096, which is 64 m. It has a flag that skips it, probably what the quest node `questDisableTeleportingMonitoring`
  sets.

## The fix

`population::Install` swaps the population system's vtable entry 97 for the gate, only when:
- the entry and its code lie in the game's image;
- the code is exactly those 8 bytes, `C6 81 B4 20 00 00 01 C3`.

Otherwise it changes nothing, and the log and the `population=` status line say why. Then:
- **While build mode places V,** `PlaceV` notes each teleport, and the gate skips the notice for `kHoldSeconds`
  (0.25 s) after the last one. That's long enough to cover the request, which the facility's job applies a frame or
  so later.
- **At any other time,** the gate calls the game's function.
- **The patch lasts** until the plugin unloads, since the vtable is the game's rather than a session's.

**Tests:** 5 new.
- The fake system runs the game's 8 bytes from executable memory, so a test sees the byte set or left as the game
  would.
- The checks refuse code from outside the image, and code that differs by any byte.
- Three mutations of the gate each failed a test: never muting, no code check, and no image check.

## What's left

- **FreeFly still stops spawning,** by the user's choice. Gating every teleport of V would fix it too, but would
  mute quests' teleports as well. Their one skipped frame is probably redundant with the 20 m branch, but that's
  unproven.
- **A new game version needs the chain re-read.** The gate refuses anything but the exact code, so a patch that
  moves it costs build mode its NPCs, never a fault.
