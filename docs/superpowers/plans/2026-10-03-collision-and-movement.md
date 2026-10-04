# Collision and Movement Implementation Plan, Part 1 (steps 1 and 2)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> In this project, plans run inline with superpowers:executing-plans, never through sub-agents.

**Goal:** Part 1 of spike 5, moving as Minecraft's player in build mode. Minecraft drives: the movement keys go to
Minecraft, and every frame V is teleported so that V's camera lands on Minecraft's eye. Step 1 checks the link's side
of it with `mclink` against the user's Minecraft; step 2 checks it in the game, on spike 4's floor.

**Architecture:** the drive is plain C++ in `render_core`, tested without the game or Minecraft.
- **`link/Ticks`:** SkyCraft's interpolation of Minecraft's ticks, on the game's clock.
- **`link/Voxels`:** triangles to SkyCraft's collision voxels. The floor sends its own, so Minecraft lets its player go
  after a teleport.
- **`link/Drive`:** waiting while Minecraft holds its player, driving after, V's eye height, and the target each frame.
- **`link/Link`:** build mode's one teleport, the floor's voxels, and where V goes each frame (`placeV`).
- **`input/BuildMode` and `input/Router`:** the movement keys go to Minecraft, the crouch keys are hidden, the mouse's
  movement is summed, and build mode stays off while V can't be placed.
- **`link/Turn`, `log/DriveLog` and `Status`:** the mouse's turn in degrees, a CSV of the drive, and the drive's
  status line.

Then what only the game can run:
- **`Puppet`:** the game's script calls that teleport V, read V's yaw and health, and say why V can't be placed.
- **`Plugin.cpp`:** BlockRender 0.11.0, which places V where the link says.
- **The overlay:** a drive line, and buttons for the turn and the drive log.

`mclink` gains held keys and a drive tracker, so step 1 drives Minecraft's player from a script, with a stand-in V that
goes where the link puts it, as the plugin teleports V.

**Tech stack:**
- C++20, MSVC 14.44 (Visual Studio 2022), CMake 3.31 (bundled with Visual Studio);
- SkyCraft's protocol (version 11) and its Fabric mod 0.1.2, unchanged, in the user's `CyberCraft` instance (Minecraft
  26.3);
- Cyberpunk 2077 2.31 with RED4ext 1.30.0 and the RED4ext SDK, header-only; CET for the overlay.

**Spec:** [docs/superpowers/specs/2026-10-03-collision-and-movement-design.md](../specs/2026-10-03-collision-and-movement-design.md)

**Parts 2 and 3:** each planned after the step before it, as the spec's approach says. Task 18 closes Part 1 and
plans Part 2.

---

## Before you start

- **The branch:** work on `spike/collision-and-movement`, which holds the handoff, the spec and this plan.
- **The shell:** commands are for Git Bash.
  - CMake isn't on PATH, and shell variables don't persist between tool calls, so every command block sets it:
    ```bash
    CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
    ```
  - Pass MSBuild switches as `-m`, never `/m`: Git Bash rewrites `/m` into a file path.
- **Edit steps:** an Edit step's code blocks come in pairs, applied in order: the text to find, which occurs exactly
  once in the file as the pairs before it left it, then the text that replaces it. A Write step's block is the whole
  file.
- **Already verified:** all the code in this plan has been compiled and tested once already, in a scratch clone.
  - Tasks 1 to 7 and 9 to 16 were then replayed in a fresh clone by a script that applied every file step exactly as
    written, and checked every build, test, `mclink` and overlay step's expected result. The results:
    - zero compiler warnings at `/W4`;
    - 289 C++ tests passing;
    - the replayed files identical to the scratch clone's.
  - Each new test was broken once, by a mutation of the code it covers, and failed as it should.
  - `Puppet`'s script calls were traced to the game's own declarations and to the user's installed mods that call
    them, and every address the SDK resolves on their path was found in the game's `cyberpunk2077_addresses.json`.
    Only the game can run them: Task 17's first check is that they're all found.

  So a failure while following the plan most likely means a transcription slip: compare against the plan's code first.
- **Test counts:** 232 at the start. Then 241, 249, 250, 260, 261, 268 and 270 after Tasks 1 to 7, and 274, 278, 287,
  288 and 289 after Tasks 9 to 13. Tasks 14 to 16 add none.
- **What only Minecraft and the game can test:** the drive against SkyCraft's real mod in Task 8, and in the game in
  Task 17, both with the user.
- **The user:**
  - **does everything in Minecraft, the Modrinth App and the game:** starting and stopping them, loading, moving,
    keys, clicks and screenshots. Never drive their mouse or keyboard: ask, then wait for them to say they're done;
  - **approves downloads first.** Nothing in Part 1 downloads anything.
- **The game:** builds deploy only with `CP2077_DIR` set, which only Task 17's deploy step sets, with the game closed,
  and clears straight after. `mclink` needs the game closed too, since the game would hold the mapping.
- **No faults:** the game's crash handler sees a fault before any `__try`, and freezes the game. `Puppet` calls only
  functions whose parameter and return types it has checked, and checks every system and handle before using it.
- **Commits** go on `spike/collision-and-movement`, and end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## File map (Part 1)

All paths under `src/`, `tests/` and `tools/` are relative to `spikes/block-render/`.

| File | Responsibility |
|---|---|
| `CMakeLists.txt` | The new sources and tests, `Puppet` in the plugin, and version 0.11.0 |
| `src/link/Ticks.*` | SkyCraft's tick interpolation: Minecraft's feet and eye height at the render time |
| `src/link/Voxels.*` | Triangles to SkyCraft's collision voxels, and the voxel region message |
| `src/link/Floor.*` | The floor's voxel regions, beside its triangles |
| `src/link/Drive.*` | The drive's states, V's eye height, and V's feet under Minecraft's eye |
| `src/link/Link.*` | `teleportSeq` from the clock; build mode's teleport, floor and drive; `placeV` and `endBuild` |
| `src/link/Turn.*` | The mouse's counts to V's degrees, measured outside build mode |
| `src/log/DriveLog.*` | `drivelog`'s CSV, a row a frame |
| `src/input/BuildMode.*` | The movement and crouch keys; build mode refused while V or Minecraft's player is blocked |
| `src/input/Router.*` | The mouse's movement along x, and why V can't be placed |
| `src/Requests.*` | `drivelog <seconds>` |
| `src/Status.*` | The drive line |
| `src/Puppet.*` | The game's script calls: teleport, yaw, health, and what keeps V from being placed |
| `src/Plugin.cpp` | 0.11.0: V placed each frame in build mode, `set turn`, `drivelog`, the drive's status |
| `tools/mclink/*` | Held keys, the drive tracker, and step 1's scripts `drive.txt` and `relink.txt` |
| `tools/blockrender_hud/init.lua` | The drive line, and the turn and drive log buttons |
| `tests/TestTicks.cpp` to `tests/TestDriveLog.cpp` | One test file per new unit; the others gain tests |

---

### Task 1: Minecraft's ticks on the game's clock

Sampling McState's position once a frame judders, SkyCraft found: Minecraft moves its player in 20 Hz ticks, out of
step with the game's frames. SkyCraft's game side renders a little in the past instead, between two ticks it already
has (the spec's "Each frame while Minecraft drives", and "SkyCraft's game side"). `TickInterpolator` ports it from
`skse/src/Game.cpp`, lines 662 to 786, with SkyCraft's constants:
- up to 8 ticks kept, each stamped on `QueryPerformanceCounter`, the clock both processes share, and locked to the
  50 ms rhythm;
- a render delay from 4 to 30 ms, set from how late the last 40 ticks (2 s) turned up, growing by up to 20 ms a second
  and shrinking by 2 ms a second.

It gives Minecraft's feet and eye height at the render time, and says when a frame used up the newest tick, which the
status counts as a late tick. McState is shared memory, so a tick is taken only with a rate from 1 to 1,000 ms and
finite positions and eye heights (the spec's "Safety"). The tests feed it SkyCraft's cases on a 60 Hz frame clock.

**Files:**
- Create: `src/link/Ticks.h`, `src/link/Ticks.cpp`, `tests/TestTicks.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestTicks.cpp`**

```cpp
#include "Harness.h"

#include "link/Ticks.h"

#include <algorithm>
#include <limits>

using namespace cpmc::link;

namespace
{
constexpr int64_t kPerSecond = 1'000'000;  // a clock in microseconds
constexpr int64_t kMs = 1000;

// One Minecraft tick: the feet from aPrevX to aCurX along x, and the eye height from aEyeO to aEye.
proto::McState Tick(int64_t aStamp, double aPrevX, double aCurX, float aEyeO = 1.62f, float aEye = 1.62f)
{
	proto::McState state{};
	state.tickQpc = aStamp;
	state.tickMs = 50.0f;
	state.prevX = aPrevX;
	state.curX = aCurX;
	state.prevY = state.curY = 6.0;
	state.prevZ = state.curZ = 2000.0;
	state.tickEyeO = aEyeO;
	state.tickEye = aEye;
	return state;
}
}

TEST(TicksGiveNothingBeforeMinecraftHasTicked)
{
	TickInterpolator ticks;
	CHECK(!ticks.Frame(proto::McState{}, 1'000'000, kPerSecond).has_value());
	CHECK(!ticks.Frame(Tick(0, 0.0, 1.0), 1'000'000, kPerSecond).has_value());  // its rate, but no tick stamped yet
	proto::McState noRate = Tick(1'000'000, 0.0, 1.0);
	noRate.tickMs = 0.0f;
	CHECK(!ticks.Frame(noRate, 1'010'000, kPerSecond).has_value());
	CHECK(!ticks.Frame(Tick(1'000'000, 0.0, 1.0), 1'020'000, 0).has_value());  // no clock rate
}

TEST(TicksRefuseTicksThatArentUsable)
{
	// McState is shared memory: a tick's rate must be 1 to 1,000 ms, and its positions and eye heights finite.
	TickInterpolator ticks;
	proto::McState   fast = Tick(1'000'000, 0.0, 1.0);
	fast.tickMs = 0.5f;
	CHECK(!ticks.Frame(fast, 1'010'000, kPerSecond).has_value());
	proto::McState slow = Tick(1'000'000, 0.0, 1.0);
	slow.tickMs = 1001.0f;
	CHECK(!ticks.Frame(slow, 1'020'000, kPerSecond).has_value());
	CHECK(!ticks.Frame(Tick(1'000'000, 0.0, std::numeric_limits<double>::quiet_NaN()), 1'030'000, kPerSecond).has_value());
	CHECK(!ticks.Frame(Tick(1'000'000, 0.0, 1.0, 1.62f, std::numeric_limits<float>::infinity()), 1'040'000, kPerSecond).has_value());
	CHECK(ticks.Frame(Tick(1'000'000, 0.0, 1.0), 1'050'000, kPerSecond).has_value());  // and then a good one is taken
}

TEST(TicksInterpolateTenMillisecondsBehind)
{
	TickInterpolator ticks;
	const int64_t    at = 1'000'000;
	// It starts 10 ms behind: 35 ms after the stamp, it renders the tick's middle, sneaking's eye halfway down.
	const auto sample = ticks.Frame(Tick(at, 0.0, 1.0, 1.62f, 1.27f), at + 35 * kMs, kPerSecond);
	CHECK(sample.has_value());
	CHECK_NEAR(sample->feet.x, 0.5, 1e-9);
	CHECK_NEAR(sample->feet.y, 6.0, 1e-9);
	CHECK_NEAR(sample->feet.z, 2000.0, 1e-9);
	CHECK_NEAR(sample->eyeHeight, 1.445, 1e-6);
	CHECK(!sample->waited);
	CHECK_NEAR(ticks.DelayMs(), 10.0, 1e-9);
}

TEST(TicksNeverExtrapolate)
{
	TickInterpolator ticks;
	const int64_t    at = 1'000'000;
	// 30 ms past the tick's end, with no newer tick: it stays at the end, and waits.
	const auto sample = ticks.Frame(Tick(at, 0.0, 1.0), at + 90 * kMs, kPerSecond);
	CHECK_NEAR(sample->feet.x, 1.0, 1e-9);
	CHECK(sample->waited);
}

TEST(TicksBridgeAMissedTick)
{
	TickInterpolator ticks;
	const int64_t    at = 1'000'000;
	ticks.Frame(Tick(at, 0.0, 1.0), at + 10 * kMs, kPerSecond);
	// The tick at +50 ms never shows. The one at +100 ms starts at 2: rendering at +95 ms is 45 ms into the 50 ms
	// between the first's end and its start.
	const auto sample = ticks.Frame(Tick(at + 100 * kMs, 2.0, 3.0), at + 105 * kMs, kPerSecond);
	CHECK_NEAR(sample->feet.x, 1.9, 1e-9);
	CHECK(!sample->waited);
}

TEST(TicksLockToTheRhythm)
{
	TickInterpolator ticks;
	const int64_t    at = 1'000'000;
	ticks.Frame(Tick(at, 0.0, 1.0), at + 10 * kMs, kPerSecond);
	// Stamped 2 ms late: the tick goes 1/16 of that late, at +50.125 ms. Rendering at +75 ms is 24.875 ms into it.
	const auto sample = ticks.Frame(Tick(at + 52 * kMs, 1.0, 2.0), at + 85 * kMs, kPerSecond);
	CHECK_NEAR(sample->feet.x, 1.0 + 24.875 / 50.0, 1e-9);
}

TEST(TicksSplitTwoSeenInOneFrame)
{
	TickInterpolator ticks;
	const int64_t    at = 1'000'000;
	ticks.Frame(Tick(at, 0.0, 1.0), at + 10 * kMs, kPerSecond);
	ticks.Frame(Tick(at + 100 * kMs, 1.0, 2.0), at + 101 * kMs, kPerSecond);  // two ticks on: one seemed missed
	// Another tick, stamped about when the last was: Minecraft ran two ticks in one frame, and the first carried the
	// second's stamp. It belongs a tick earlier, at +50 ms, so rendering at +95 ms is 45 ms into it.
	const auto sample = ticks.Frame(Tick(at + 102 * kMs, 2.0, 3.0), at + 105 * kMs, kPerSecond);
	CHECK_NEAR(sample->feet.x, 1.9, 1e-9);
}

TEST(TicksStartOverForANewMinecraft)
{
	TickInterpolator ticks;
	const int64_t    at = 5'000'000;
	ticks.Frame(Tick(at, 0.0, 1.0), at + 10 * kMs, kPerSecond);
	// A stamp from before the last: a new Minecraft. Only its tick counts, and it's long over.
	auto sample = ticks.Frame(Tick(at - 400 * kMs, 7.0, 8.0), at + 20 * kMs, kPerSecond);
	CHECK_NEAR(sample->feet.x, 8.0, 1e-9);
	CHECK(sample->waited);
	ticks.Reset();
	sample = ticks.Frame(Tick(at + 100 * kMs, 4.0, 5.0), at + 135 * kMs, kPerSecond);
	CHECK_NEAR(sample->feet.x, 4.5, 1e-9);
	CHECK_NEAR(ticks.DelayMs(), 10.0, 1e-9);
}

TEST(TicksWaitLongerForLateTicksAndShrinkSlowly)
{
	// Frames at 60 Hz, and each tick showing 30 ms after its stamp: on the frame before each shows, it was already
	// 16.7 ms due (16.716 at most, as the frames drift against the ticks). The delay grows to that, and 1 ms more, by
	// 20 ms a second. Once ticks come on time, the late ones leave the 2 s window, and then the delay shrinks by 2 ms a
	// second.
	TickInterpolator ticks;
	const int64_t    start = 1'000'000;
	const auto       run = [&](int aFirstFrame, int aFrames, int64_t aLag) {
		for (int frame = aFirstFrame; frame < aFirstFrame + aFrames; ++frame)
		{
			const int64_t now = start + int64_t(frame) * 16'667;
			const int64_t k = std::max<int64_t>(0, (now - start - aLag) / (50 * kMs));
			ticks.Frame(Tick(start + k * 50 * kMs, double(k), double(k + 1)), now, kPerSecond);
		}
	};
	run(2, 9, 30 * kMs);
	CHECK_NEAR(ticks.DelayMs(), 12.0, 0.001);  // the first late tick showed on frame 5: six frames of growth since
	run(11, 141, 30 * kMs);
	CHECK_NEAR(ticks.DelayMs(), 17.716, 0.001);
	run(152, 240, 0);
	CHECK_NEAR(ticks.DelayMs(), 13.650, 0.001);
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestRouter.cpp
	tools/mclink/McLinkScript.cpp
```
```cmake
	tests/TestRouter.cpp
	tests/TestTicks.cpp
	tools/mclink/McLinkScript.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Ticks.h'`.

- [x] **Step 4: Write `src/link/Ticks.h`**

```cpp
#pragma once

#include "Coords.h"

#include <skycraft_protocol.h>

#include <array>
#include <cstdint>
#include <deque>
#include <optional>

// Minecraft's 20 Hz physics ticks on the game's own clock (the spec's "Each frame while Minecraft drives"), ported from
// SkyCraft's Skyrim side (skse/src/Game.cpp, PerFrame, MIT). McState carries each tick's start and end feet and eye
// height, and when it happened on QueryPerformanceCounter, the clock both processes share. Rendering a little in the
// past, so that the next tick has always arrived, gives pure interpolation: no judder from the two games' frames being
// out of step, and no overshoot when the player stops or lands. SkyCraft found that sampling McState's per-frame
// position judders.
namespace cpmc::link
{
namespace proto = skycraft::proto;

struct TickSample
{
	Vec3d  feet;               // Minecraft axes
	double eyeHeight = 0.0;    // blocks above the feet
	bool   waited = false;     // the newest tick was used up: the player stands still this frame
};

class TickInterpolator
{
public:
	// Once a frame: McState as read this frame, and the clock now. Nothing until Minecraft has stamped a tick, or
	// without a clock rate.
	std::optional<TickSample> Frame(const proto::McState& aState, int64_t aNowQpc, int64_t aQpcPerSecond);

	double DelayMs() const { return renderDelayMs_; }  // how far in the past it renders
	void   Reset();                                     // forgets the ticks: a new Minecraft

private:
	struct Tick
	{
		proto::McState state;
		int64_t        at = 0;     // when it happened, locked to the rhythm
		int            slots = 1;  // ticks since the one seen before it: 2 or more, one was missed
	};

	std::deque<Tick>       history_;
	int64_t                lastFrameQpc_ = 0;
	int                    stampOutliers_ = 0;
	double                 renderDelayMs_ = 10.0;
	std::array<double, 40> tickDue_{};  // the last 2 s: how long each tick was already due when first seen, ms
	size_t                 tickDueNext_ = 0;
	bool                   tickDueInit_ = false;
};
}
```

- [x] **Step 5: Write `src/link/Ticks.cpp`**

```cpp
#include "link/Ticks.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace cpmc::link
{
namespace
{
// McState is shared memory, so untrusted (the spec's "Safety"): a tick is taken only with a rate from 1 to 1,000 ms,
// and finite positions and eye heights.
bool Usable(const proto::McState& aState)
{
	return aState.tickMs >= 1.0f && aState.tickMs <= 1000.0f && std::isfinite(aState.prevX) && std::isfinite(aState.prevY) &&
	       std::isfinite(aState.prevZ) && std::isfinite(aState.curX) && std::isfinite(aState.curY) && std::isfinite(aState.curZ) &&
	       std::isfinite(aState.tickEyeO) && std::isfinite(aState.tickEye);
}
}

std::optional<TickSample> TickInterpolator::Frame(const proto::McState& aState, int64_t aNowQpc, int64_t aQpcPerSecond)
{
	if (aState.tickQpc == 0 || !Usable(aState) || aQpcPerSecond <= 0)
	{
		return std::nullopt;
	}
	const double  qpcPerMs = double(aQpcPerSecond) / 1000.0;
	const int64_t period = std::max<int64_t>(1, std::llround(double(aState.tickMs) * qpcPerMs));
	if (history_.empty() || history_.back().state.tickQpc != aState.tickQpc)
	{
		if (!history_.empty() && aState.tickQpc < history_.back().state.tickQpc)
		{
			history_.clear();  // Minecraft restarted
		}
		Tick tick{ aState, aState.tickQpc, 1 };
		if (!history_.empty())
		{
			Tick&         last = history_.back();
			const int64_t n = std::llround(double(aState.tickQpc - last.at) / double(period));
			const int64_t error = aState.tickQpc - (last.at + n * period);
			if (n == 0 && last.slots >= 2)
			{
				// Minecraft ran two ticks in one frame and we saw both: the first one carries the second's stamp. It
				// belongs a tick earlier.
				last.at -= period;
				last.slots -= 1;
				tick.at = last.at + period;
			}
			else if (n >= 1 && n <= 10 && std::abs(error) < period * 3 / 10)
			{
				tick.at = last.at + n * period + error / 16;  // the rhythm is exact; the stamps are noisy
				tick.slots = int(n);
				stampOutliers_ = 0;
			}
			else if (n <= 10 && ++stampOutliers_ < 3)
			{
				tick.slots = int(std::max<int64_t>(n, 1));
				tick.at = last.at + tick.slots * period;  // one odd stamp (a hitch): keep the rhythm
			}
			else
			{
				stampOutliers_ = 0;  // lost the rhythm (a pause, a new tick rate): start from this stamp
			}
		}
		// Was it already due on our last frame? Then rendering had to wait for it.
		if (lastFrameQpc_ != 0)
		{
			if (!tickDueInit_)
			{
				tickDue_.fill(renderDelayMs_ - 1.0);
				tickDueInit_ = true;
			}
			const double dueMs = double(lastFrameQpc_ - tick.at) / qpcPerMs;
			if (dueMs < 30.0)  // later than that is a hitch, not a pattern to wait for
			{
				tickDue_[tickDueNext_++ % tickDue_.size()] = dueMs;
			}
		}
		history_.push_back(tick);
		if (history_.size() > 8)
		{
			history_.pop_front();
		}
	}

	// The render delay follows how late ticks have been over the last 2 s: it grows by up to 20 ms a second and shrinks
	// by 2, too little to see either way.
	const double frameMs = lastFrameQpc_ != 0 ? double(aNowQpc - lastFrameQpc_) / qpcPerMs : 0.0;
	lastFrameQpc_ = aNowQpc;
	if (tickDueInit_)
	{
		const double target = std::clamp(*std::max_element(tickDue_.begin(), tickDue_.end()) + 1.0, 4.0, 30.0);
		const double dt = std::min(frameMs, 100.0) / 1000.0;
		renderDelayMs_ = target > renderDelayMs_ ? std::min(target, renderDelayMs_ + 20.0 * dt) : std::max(target, renderDelayMs_ - 2.0 * dt);
	}
	const int64_t renderQpc = aNowQpc - std::llround(renderDelayMs_ * qpcPerMs);

	// The feet go through each tick's start (prev) and end (cur) positions.
	size_t i = 0;
	for (size_t k = history_.size(); k-- > 0;)
	{
		if (history_[k].at <= renderQpc)
		{
			i = k;
			break;
		}
	}
	const Tick&           tick = history_[i];
	const Tick*           next = i + 1 < history_.size() ? &history_[i + 1] : nullptr;
	const double          ticks = double(renderQpc - tick.at) / double(period);
	const double          t = std::clamp(ticks, 0.0, 1.0);
	const proto::McState& s = tick.state;
	TickSample            sample;
	sample.feet = { s.prevX + (s.curX - s.prevX) * t, s.prevY + (s.curY - s.prevY) * t, s.prevZ + (s.curZ - s.prevZ) * t };
	sample.eyeHeight = s.tickEyeO + (s.tickEye - s.tickEyeO) * t;
	if (ticks > 1.0 && next)
	{
		// Past this tick's end, and the next tick we have starts later: Minecraft ran one we never saw. Carry on from
		// this tick's end to the next one's start.
		const proto::McState& n = next->state;
		const double          gap = double(next->at - (tick.at + period));
		const double          u = gap > 0.0 ? std::clamp(double(renderQpc - (tick.at + period)) / gap, 0.0, 1.0) : 1.0;
		sample.feet = { s.curX + (n.prevX - s.curX) * u, s.curY + (n.prevY - s.curY) * u, s.curZ + (n.prevZ - s.curZ) * u };
		sample.eyeHeight = s.tickEye + (n.tickEyeO - s.tickEye) * u;
	}
	else if (ticks > 1.0)
	{
		sample.waited = true;  // the next tick hasn't arrived: the player stands still this frame
	}
	return sample;
}

void TickInterpolator::Reset()
{
	*this = TickInterpolator{};
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Link.cpp
	src/render/PendingSections.cpp
```
```cmake
	src/link/Link.cpp
	src/link/Ticks.cpp
	src/render/PendingSections.cpp
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `241 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: Minecraft's ticks on the game's clock" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 2: Triangles to SkyCraft's collision voxels

Minecraft holds its player after each teleport until the regions around it are known and there's voxel ground under
it (the spec's "SkyCraft's Minecraft side"), and the floor has sent only triangles so far. The floor's regions get
voxels from the city's own voxelizer (the spec's "The floor's voxels"), so the floor tests the same code Part 2's
exporter will use. `VoxelizeRegion` ports SkyCraft's `Collision::Voxelize` and its triangle-box test from
`skse/src/Collision.cpp`, for triangles alone:
- a region is 8 blocks a side, each block 8 × 8 × 8 sub-voxels, and a sub-voxel is solid where a triangle passes
  through it;
- surfaces 50 to 84 degrees from flat are coarsened to whole blocks, as SkyCraft does, so their risers are more than
  Minecraft's step height.

One change from SkyCraft: a triangle lying exactly on a sub-voxel boundary, as a flat floor at a whole height does, gave
SkyCraft an empty candidate range, so it filled nothing. Here it fills the layers on both sides of the boundary. The
code says so where it does it, and `VoxelsFillBothLayersAtABoundary` tests it.

`RegionPayload` lays out a `kColRegion` message: the region's box, the epoch and the blocks, `bits[y]` bit
`z * 8 + x` per sub-voxel, as `skycraft_protocol.h` has it. `kRegionBlocks` moves here from `Floor.h` in Task 3.

**Files:**
- Create: `src/link/Voxels.h`, `src/link/Voxels.cpp`, `tests/TestVoxels.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestVoxels.cpp`**

```cpp
#include "Harness.h"

#include "link/Voxels.h"

#include <cstring>

using namespace cpmc::link;

namespace
{
McTriangle Tri(float aX0, float aY0, float aZ0, float aX1, float aY1, float aZ1, float aX2, float aY2, float aZ2)
{
	return { { aX0, aY0, aZ0, aX1, aY1, aZ1, aX2, aY2, aZ2 } };
}

// A flat floor at height aY over the 8 x 8 blocks from aX, aZ: two triangles.
std::vector<McTriangle> Floor(float aY, float aX = 0.0f, float aZ = 0.0f)
{
	return { Tri(aX, aY, aZ, aX + 8, aY, aZ, aX + 8, aY, aZ + 8), Tri(aX, aY, aZ, aX + 8, aY, aZ + 8, aX, aY, aZ + 8) };
}

bool OnlyLayer(const proto::ColBlock& aBlock, int aLayer, uint64_t aBits)
{
	for (int y = 0; y < 8; ++y)
	{
		if (aBlock.bits[y] != (y == aLayer ? aBits : 0ull))
		{
			return false;
		}
	}
	return true;
}
}

TEST(VoxelsFillOneLayerUnderAFlatFloor)
{
	// Spike 3's corner: region -15 0 250 is blocks x -120 to -113 and z 2000 to 2007. The floor at 6.14 lies in sub-voxel
	// layer 1 of block row 6, which is 6.125 to 6.25.
	const auto blocks = VoxelizeRegion(-15, 0, 250, Floor(6.14f, -120.0f, 2000.0f));
	CHECK_EQ(blocks.size(), size_t(64));
	int good = 0;
	for (const proto::ColBlock& block : blocks)
	{
		good += block.y == 6 && block.x >= -120 && block.x <= -113 && block.z >= 2000 && block.z <= 2007 && OnlyLayer(block, 1, ~0ull);
	}
	CHECK_EQ(good, 64);
}

TEST(VoxelsFillBothLayersAtABoundary)
{
	// At exactly 6.0 the floor touches the top layer of block row 5 and the bottom layer of row 6.
	const auto blocks = VoxelizeRegion(0, 0, 0, Floor(6.0f));
	CHECK_EQ(blocks.size(), size_t(128));
	int below = 0;
	int above = 0;
	for (const proto::ColBlock& block : blocks)
	{
		below += block.y == 5 && OnlyLayer(block, 7, ~0ull);
		above += block.y == 6 && OnlyLayer(block, 0, ~0ull);
	}
	CHECK_EQ(below, 64);
	CHECK_EQ(above, 64);
}

TEST(VoxelsKeepWallsFine)
{
	// A wall at x 2.5, the edge between sub-voxels 3 and 4 of block column 2: both, all the way up and along.
	const std::vector<McTriangle> wall{ Tri(2.5f, 0, 0, 2.5f, 8, 0, 2.5f, 8, 8), Tri(2.5f, 0, 0, 2.5f, 8, 8, 2.5f, 0, 8) };
	const auto                    blocks = VoxelizeRegion(0, 0, 0, wall);
	CHECK_EQ(blocks.size(), size_t(64));
	int good = 0;
	for (const proto::ColBlock& block : blocks)
	{
		bool layers = true;
		for (uint64_t layer : block.bits)
		{
			layers = layers && layer == 0x1818181818181818ull;
		}
		good += block.x == 2 && layers;
	}
	CHECK_EQ(good, 64);
}

TEST(VoxelsFollowATrianglesEdges)
{
	// A triangle over half of block 0 0 0, cut along its diagonal, 0.55 up, in sub-voxel layer 4: the sub-voxels under it
	// are solid, and those beyond the diagonal stay empty, though they're inside its bounds.
	const std::vector<McTriangle> half{ Tri(0.05f, 0.55f, 0.05f, 0.95f, 0.55f, 0.05f, 0.05f, 0.55f, 0.95f) };
	const auto                    blocks = VoxelizeRegion(0, 0, 0, half);
	CHECK_EQ(blocks.size(), size_t(1));
	const uint64_t layer = blocks.empty() ? 0ull : blocks[0].bits[4];
	CHECK((layer & 1ull) != 0);                // x 0, z 0: under the triangle
	CHECK((layer & (1ull << (6 * 8 + 6))) == 0);  // x 6, z 6: beyond the diagonal
	CHECK((layer >> 63) == 0);                 // x 7, z 7
}

TEST(VoxelsCoarsenSteepSlopes)
{
	// About 63 degrees from flat (|ny| 0.447), from 0.1 to 0.7 up, within block 0 0 0: its layers 0 to 5 become solid
	// across the whole block, so a slope too steep to walk becomes a step Minecraft won't climb.
	const auto blocks = VoxelizeRegion(0, 0, 0, std::vector<McTriangle>{ Tri(0.1f, 0.1f, 0.1f, 0.4f, 0.7f, 0.1f, 0.1f, 0.1f, 0.4f) });
	CHECK_EQ(blocks.size(), size_t(1));
	CHECK(blocks[0].x == 0 && blocks[0].y == 0 && blocks[0].z == 0);
	for (int y = 0; y < 8; ++y)
	{
		CHECK_EQ(blocks[0].bits[y], y <= 5 ? ~0ull : 0ull);
	}
}

TEST(VoxelsIgnoreTrianglesOutside)
{
	CHECK(VoxelizeRegion(0, 0, 0, Floor(6.14f, 16.0f, 0.0f)).empty());
	CHECK(VoxelizeRegion(0, 0, 0, Floor(20.0f)).empty());
	CHECK(VoxelizeRegion(0, 0, 0, std::vector<McTriangle>{ Tri(1, 1, 1, 1, 1, 1, 1, 1, 1) }).empty());  // no area
}

TEST(VoxelsUseTheProtocolsBitOrder)
{
	// A speck inside sub-voxel x 1, y 2, z 3 of block 0 0 0: bits[2], bit 3 * 8 + 1.
	const auto blocks =
		VoxelizeRegion(0, 0, 0, std::vector<McTriangle>{ Tri(0.17f, 0.3125f, 0.42f, 0.20f, 0.3125f, 0.42f, 0.17f, 0.3125f, 0.45f) });
	CHECK_EQ(blocks.size(), size_t(1));
	CHECK(OnlyLayer(blocks[0], 2, 1ull << (3 * 8 + 1)));
}

TEST(VoxelsRegionPayload)
{
	proto::ColBlock block{};
	block.x = -120;
	block.y = 6;
	block.z = 2000;
	block.bits[1] = ~0ull;
	const auto payload = RegionPayload(-15, 0, 250, 7, std::vector<proto::ColBlock>{ block });
	CHECK_EQ(payload.size(), sizeof(proto::ColRegion) + sizeof(proto::ColBlock));
	proto::ColRegion header;
	std::memcpy(&header, payload.data(), sizeof header);
	CHECK(header.minX == -120 && header.minY == 0 && header.minZ == 2000);
	CHECK(header.maxX == -113 && header.maxY == 7 && header.maxZ == 2007);
	CHECK_EQ(header.epoch, 7u);
	CHECK_EQ(header.count, 1u);
	CHECK(std::memcmp(payload.data() + sizeof header, &block, sizeof block) == 0);
	const auto empty = RegionPayload(-15, -1, 250, 7, {});
	CHECK_EQ(empty.size(), sizeof(proto::ColRegion));
	std::memcpy(&header, empty.data(), sizeof header);
	CHECK(header.minY == -8 && header.maxY == -1 && header.count == 0);
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestTicks.cpp
	tools/mclink/McLinkScript.cpp
```
```cmake
	tests/TestTicks.cpp
	tests/TestVoxels.cpp
	tools/mclink/McLinkScript.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Voxels.h'`.

- [x] **Step 4: Write `src/link/Voxels.h`**

```cpp
#pragma once

#include <skycraft_protocol.h>

#include <cstdint>
#include <span>
#include <vector>

// Triangles to SkyCraft's collision voxels (the spec's "The floor's voxels" and "The exporter"), ported from SkyCraft's
// Skyrim side (skse/src/Collision.cpp, Collision::Voxelize and its triangle-box test, MIT), for triangles alone. A
// region is 8 blocks a side, and each block 8 x 8 x 8 sub-voxels of 1/8 block. A sub-voxel is solid where a triangle
// passes through it (Akenine-Moller's triangle-box overlap test). Surfaces 50 to 84 degrees from flat (|ny| from 0.1 to
// 0.643) are coarsened to whole-block footprints, so the risers between neighbouring columns exceed Minecraft's 0.6
// step height, as SkyCraft does. Minecraft's player walks on the triangles; the voxels are what release it after a
// teleport, and what TNT, items and mobs land on.
namespace cpmc::link
{
namespace proto = skycraft::proto;

constexpr int32_t kRegionBlocks = 8;  // SkyCraft's collision regions are 8-block cubes

// A triangle in Minecraft's axes, in blocks.
struct McTriangle
{
	float v[9];
};

// Region (aRegionX, aRegionY, aRegionZ)'s blocks that have any solid sub-voxel, each with its sub-voxels: bits[y], bit
// z * 8 + x. In order of y, then z, then x. Parts of triangles outside the region are ignored.
std::vector<proto::ColBlock> VoxelizeRegion(int32_t aRegionX, int32_t aRegionY, int32_t aRegionZ, std::span<const McTriangle> aTriangles);

// A voxel region message's payload (kColRegion): the region's inclusive block box, the epoch, the count and the blocks.
// With no blocks, it marks the region known and empty.
std::vector<uint8_t> RegionPayload(int32_t aRegionX, int32_t aRegionY, int32_t aRegionZ, uint32_t aEpoch,
                                   std::span<const proto::ColBlock> aBlocks);
}
```

- [x] **Step 5: Write `src/link/Voxels.cpp`**

```cpp
#include "link/Voxels.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace cpmc::link
{
namespace
{
constexpr int   kGrid = kRegionBlocks * 8;  // sub-voxels along a region's edge: 64, one bit each in a row
constexpr float kSteepMin = 0.1f;            // |n.y| below this is a wall: kept fine-grained
constexpr float kSteepMax = 0.643f;          // |n.y| below this (steeper than about 50 degrees) is coarsened

void Sub(const float* aA, const float* aB, float* aOut)
{
	aOut[0] = aA[0] - aB[0];
	aOut[1] = aA[1] - aB[1];
	aOut[2] = aA[2] - aB[2];
}

void Cross(const float* aA, const float* aB, float* aOut)
{
	aOut[0] = aA[1] * aB[2] - aA[2] * aB[1];
	aOut[1] = aA[2] * aB[0] - aA[0] * aB[2];
	aOut[2] = aA[0] * aB[1] - aA[1] * aB[0];
}

float Dot(const float* aA, const float* aB)
{
	return aA[0] * aB[0] + aA[1] * aB[1] + aA[2] * aB[2];
}

bool AxisTest(const float* aV0, const float* aV1, const float* aV2, const float* aAxis, float aHalf)
{
	const float p0 = Dot(aV0, aAxis), p1 = Dot(aV1, aAxis), p2 = Dot(aV2, aAxis);
	const float low = std::min({ p0, p1, p2 }), high = std::max({ p0, p1, p2 });
	const float r = aHalf * (std::fabs(aAxis[0]) + std::fabs(aAxis[1]) + std::fabs(aAxis[2]));
	return !(low > r || high < -r);
}

// The box centred at aCentre with half-size aHalf on every axis, against the triangle aA, aB, aC with face normal aN.
bool TriBoxOverlap(const float* aCentre, float aHalf, const float* aA, const float* aB, const float* aC, const float* aN)
{
	float v0[3], v1[3], v2[3];
	Sub(aA, aCentre, v0);
	Sub(aB, aCentre, v1);
	Sub(aC, aCentre, v2);
	for (int i = 0; i < 3; ++i)
	{
		const float low = std::min({ v0[i], v1[i], v2[i] }), high = std::max({ v0[i], v1[i], v2[i] });
		if (low > aHalf || high < -aHalf)
		{
			return false;
		}
	}
	const float d = Dot(aN, v0);
	const float r = aHalf * (std::fabs(aN[0]) + std::fabs(aN[1]) + std::fabs(aN[2]));
	if (std::fabs(d) > r)
	{
		return false;
	}
	float edges[3][3];
	Sub(v1, v0, edges[0]);
	Sub(v2, v1, edges[1]);
	Sub(v0, v2, edges[2]);
	static constexpr float kAxes[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	for (const auto& edge : edges)
	{
		for (const auto& unit : kAxes)
		{
			float axis[3];
			Cross(edge, unit, axis);
			if (!AxisTest(v0, v1, v2, axis, aHalf))
			{
				return false;
			}
		}
	}
	return true;
}
}

std::vector<proto::ColBlock> VoxelizeRegion(int32_t aRegionX, int32_t aRegionY, int32_t aRegionZ, std::span<const McTriangle> aTriangles)
{
	// grid[y * kGrid + z] holds a row of 64 sub-voxels along x.
	std::vector<uint64_t> solid(kGrid * kGrid, 0), steep(kGrid * kGrid, 0);
	const double          origin[3] = { double(aRegionX) * kRegionBlocks, double(aRegionY) * kRegionBlocks, double(aRegionZ) * kRegionBlocks };
	const auto            toVoxels = [&](const float* aMc, float* aOut) {
		for (int i = 0; i < 3; ++i)
		{
			aOut[i] = float((double(aMc[i]) - origin[i]) * 8.0);
		}
	};
	const auto low = [](float aValue) { return std::clamp(int(std::floor(aValue)), 0, kGrid - 1); };
	const auto high = [](float aValue) { return std::clamp(int(std::ceil(aValue)) - 1, 0, kGrid - 1); };

	for (const McTriangle& triangle : aTriangles)
	{
		float a[3], b[3], c[3];
		toVoxels(triangle.v, a);
		toVoxels(triangle.v + 3, b);
		toVoxels(triangle.v + 6, c);
		float lo[3], hi[3];
		for (int i = 0; i < 3; ++i)
		{
			lo[i] = std::min({ a[i], b[i], c[i] });
			hi[i] = std::max({ a[i], b[i], c[i] });
		}
		if (hi[0] < 0 || hi[1] < 0 || hi[2] < 0 || lo[0] > kGrid || lo[1] > kGrid || lo[2] > kGrid)
		{
			continue;
		}
		float e1[3], e2[3], n[3];
		Sub(b, a, e1);
		Sub(c, a, e2);
		Cross(e1, e2, n);
		const float length = std::sqrt(Dot(n, n));
		if (length < 1e-9f)
		{
			continue;
		}
		n[0] /= length, n[1] /= length, n[2] /= length;
		const float            ny = std::fabs(n[1]);
		std::vector<uint64_t>& grid = ny >= kSteepMax || ny < kSteepMin ? solid : steep;

		// Plane-guided: for each column along the normal's dominant axis, only the sub-voxels near the plane are
		// tested, so a big triangle costs its area, not the volume of its bounds.
		int dominant = 0;
		if (std::fabs(n[1]) > std::fabs(n[dominant]))
		{
			dominant = 1;
		}
		if (std::fabs(n[2]) > std::fabs(n[dominant]))
		{
			dominant = 2;
		}
		const int   u = (dominant + 1) % 3, v = (dominant + 2) % 3;
		const float d = Dot(n, a);
		const float r = 0.5f * (std::fabs(n[0]) + std::fabs(n[1]) + std::fabs(n[2]));
		// The sub-voxels along the dominant axis that the triangle spans. One lying exactly on a boundary between two
		// (a floor at a whole metre) spans none, so it tests the two it touches. SkyCraft's own drops it, which Skyrim's
		// coordinates rarely hit; Night City's geometry is often on whole metres.
		int domLow = low(lo[dominant]), domHigh = high(hi[dominant]);
		if (domHigh < domLow)
		{
			domLow = std::clamp(int(std::ceil(lo[dominant])) - 1, 0, kGrid - 1);
			domHigh = std::clamp(int(std::floor(hi[dominant])), 0, kGrid - 1);
		}
		for (int iu = low(lo[u]); iu <= high(hi[u]); ++iu)
		{
			for (int iv = low(lo[v]); iv <= high(hi[v]); ++iv)
			{
				const float cu = iu + 0.5f, cv = iv + 0.5f;
				const float s0 = (d - r - n[u] * cu - n[v] * cv) / n[dominant];
				const float s1 = (d + r - n[u] * cu - n[v] * cv) / n[dominant];
				const int   first = std::max(domLow, int(std::floor(std::min(s0, s1) - 0.5f)));
				const int   last = std::min(domHigh, int(std::ceil(std::max(s0, s1) - 0.5f)));
				for (int id = first; id <= last; ++id)
				{
					float centre[3];
					centre[dominant] = id + 0.5f;
					centre[u] = cu;
					centre[v] = cv;
					if (TriBoxOverlap(centre, 0.5f, a, b, c, n))
					{
						int at[3];
						at[dominant] = id, at[u] = iu, at[v] = iv;
						grid[size_t(at[1]) * kGrid + size_t(at[2])] |= 1ull << at[0];
					}
				}
			}
		}
	}

	// Steep surfaces fill their blocks' whole footprint between their lowest and highest sub-voxel layers.
	for (int by = 0; by < kRegionBlocks; ++by)
	{
		for (int bz = 0; bz < kRegionBlocks; ++bz)
		{
			for (int bx = 0; bx < kRegionBlocks; ++bx)
			{
				const uint64_t columns = 0xFFull << (bx * 8);
				int            lowest = kGrid, highest = -1;
				for (int y = by * 8; y < by * 8 + 8; ++y)
				{
					for (int z = bz * 8; z < bz * 8 + 8; ++z)
					{
						if (steep[size_t(y) * kGrid + size_t(z)] & columns)
						{
							lowest = std::min(lowest, y);
							highest = std::max(highest, y);
						}
					}
				}
				for (int y = lowest; y <= highest; ++y)
				{
					for (int z = bz * 8; z < bz * 8 + 8; ++z)
					{
						solid[size_t(y) * kGrid + size_t(z)] |= columns;
					}
				}
			}
		}
	}

	std::vector<proto::ColBlock> blocks;
	for (int by = 0; by < kRegionBlocks; ++by)
	{
		for (int bz = 0; bz < kRegionBlocks; ++bz)
		{
			for (int bx = 0; bx < kRegionBlocks; ++bx)
			{
				proto::ColBlock block{};
				bool            any = false;
				for (int sy = 0; sy < 8; ++sy)
				{
					uint64_t layer = 0;
					for (int sz = 0; sz < 8; ++sz)
					{
						const uint64_t row = (solid[size_t(by * 8 + sy) * kGrid + size_t(bz * 8 + sz)] >> (bx * 8)) & 0xFF;
						layer |= row << (sz * 8);
					}
					block.bits[sy] = layer;
					any = any || layer != 0;
				}
				if (any)
				{
					block.x = aRegionX * kRegionBlocks + bx;
					block.y = aRegionY * kRegionBlocks + by;
					block.z = aRegionZ * kRegionBlocks + bz;
					blocks.push_back(block);
				}
			}
		}
	}
	return blocks;
}

std::vector<uint8_t> RegionPayload(int32_t aRegionX, int32_t aRegionY, int32_t aRegionZ, uint32_t aEpoch,
                                   std::span<const proto::ColBlock> aBlocks)
{
	const proto::ColRegion header{ aRegionX * kRegionBlocks,
		                           aRegionY * kRegionBlocks,
		                           aRegionZ * kRegionBlocks,
		                           aRegionX * kRegionBlocks + kRegionBlocks - 1,
		                           aRegionY * kRegionBlocks + kRegionBlocks - 1,
		                           aRegionZ * kRegionBlocks + kRegionBlocks - 1,
		                           aEpoch,
		                           uint32_t(aBlocks.size()) };
	std::vector<uint8_t> payload(sizeof header + aBlocks.size_bytes());
	std::memcpy(payload.data(), &header, sizeof header);
	if (!aBlocks.empty())
	{
		std::memcpy(payload.data() + sizeof header, aBlocks.data(), aBlocks.size_bytes());
	}
	return payload;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Ticks.cpp
	src/render/PendingSections.cpp
```
```cmake
	src/link/Ticks.cpp
	src/link/Voxels.cpp
	src/render/PendingSections.cpp
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `249 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: triangles to SkyCraft's collision voxels" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 3: The floor's voxels

`FloorVoxels` gives a floor region's voxel messages (the spec's "The floor's voxels"): the region itself with the
voxels of the floor's own triangles, then the regions just below and above it, empty, so they count as known. With
them Minecraft lets its player go at once: its hold needs the regions at the feet, one block below and 8 blocks below,
and voxel ground within 12 blocks. `Floor.h` now takes `kRegionBlocks` from `Voxels.h`, and the floor's triangles come
from one helper that both the triangles and the voxels use.

**Files:**
- Modify: `src/link/Floor.h`, `src/link/Floor.cpp`, `tests/TestFloor.cpp`

- [x] **Step 1: Edit `tests/TestFloor.cpp`: the failing test**

```cpp
	CHECK(FloorClear(0x01020304) == std::vector<uint8_t>({ 4, 3, 2, 1 }));
}
```
```cpp
	CHECK(FloorClear(0x01020304) == std::vector<uint8_t>({ 4, 3, 2, 1 }));
}

TEST(FloorVoxelsMakeItsRegionsKnown)
{
	// The floor's own region gets its voxels: one sub-voxel layer of the block row at 6.14. The regions just below and
	// above come empty, so the feet's, the one under them and the one 8 blocks down are all known to SkyCraft.
	const auto payloads = FloorVoxels(-15, 250, 6.14, 3);
	CHECK_EQ(payloads.size(), size_t(3));
	const int32_t expectedMinY[3] = { 0, -8, 8 };
	const size_t  expectedBlocks[3] = { 64, 0, 0 };
	for (size_t i = 0; i < payloads.size(); ++i)
	{
		proto::ColRegion region;
		std::memcpy(&region, payloads[i].data(), sizeof region);
		CHECK_EQ(region.minX, -120);
		CHECK_EQ(region.minZ, 2000);
		CHECK_EQ(region.minY, expectedMinY[i]);
		CHECK_EQ(region.epoch, 3u);
		CHECK_EQ(size_t(region.count), expectedBlocks[i]);
		CHECK_EQ(payloads[i].size(), sizeof region + expectedBlocks[i] * sizeof(proto::ColBlock));
	}
	proto::ColBlock block;
	std::memcpy(&block, payloads[0].data() + sizeof(proto::ColRegion), sizeof block);
	CHECK(block.x == -120 && block.y == 6 && block.z == 2000);
	CHECK(block.bits[0] == 0 && block.bits[1] == ~0ull && block.bits[2] == 0);
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C3861: 'FloorVoxels': identifier not found`.

- [x] **Step 3: Edit `src/link/Floor.h`**

```cpp
#pragma once
```
```cpp
#pragma once

#include "link/Voxels.h"
```
```cpp
// The stand-in floor (the spec's "The link"): flat collision triangles at V's feet, which Minecraft's crosshair hits,
// so clicks have something to place blocks on. Triangles only: SkyCraft releases its held player only once voxel
// regions are known, so Minecraft's player stays pinned where it was teleported.
namespace cpmc::link
{
constexpr int32_t kRegionBlocks = 8;  // SkyCraft's collision regions are 8-block cubes
constexpr int32_t kFloorRadius = 3;   // regions each side of the centre's: 7 by 7, 56 blocks across
```
```cpp
// The stand-in floor (the minecraft-link spec's "The link"): flat collision triangles at V's feet, which Minecraft's
// crosshair hits and its player walks on. Spike 5 adds their voxels (its spec's "The floor's voxels"): with them,
// SkyCraft lets its player go after a teleport, and TNT and items land on the floor.
namespace cpmc::link
{
constexpr int32_t kFloorRadius = 3;  // regions each side of the centre's: 7 by 7, 56 blocks across
```
```cpp
// The regions of a floor centred on Minecraft block column aX, aZ: (region x, region z) pairs, 7 by 7.
```
```cpp
// The voxel messages (kColRegion payloads) for one column of the floor: the region holding the floor, voxelized from
// FloorRegion's two triangles, then the regions just below and above it, empty. All three count as known, which
// SkyCraft's hold wants of the feet's region, the one holding the block under them, and the one 8 blocks down.
std::vector<std::vector<uint8_t>> FloorVoxels(int32_t aRegionX, int32_t aRegionZ, double aY, uint32_t aEpoch);

// The regions of a floor centred on Minecraft block column aX, aZ: (region x, region z) pairs, 7 by 7.
```

- [x] **Step 4: Edit `src/link/Floor.cpp`**

```cpp
#include <cmath>
```
```cpp
#include <array>
#include <cmath>
```
```cpp
	return int32_t(std::floor(aBlocks / kRegionBlocks));
}
}
```
```cpp
	return int32_t(std::floor(aBlocks / kRegionBlocks));
}

// A floor region's two triangles, covering its 8 by 8 blocks at height aY.
std::array<McTriangle, 2> Triangles(int32_t aRegionX, int32_t aRegionZ, double aY)
{
	const float x0 = float(aRegionX * kRegionBlocks);
	const float x1 = x0 + kRegionBlocks;
	const float z0 = float(aRegionZ * kRegionBlocks);
	const float z1 = z0 + kRegionBlocks;
	const float y = float(aY);
	return { McTriangle{ { x0, y, z0, x1, y, z0, x1, y, z1 } }, McTriangle{ { x0, y, z0, x1, y, z1, x0, y, z1 } } };
}
}
```
```cpp
	const float x0 = float(aRegionX * kRegionBlocks);
	const float x1 = x0 + kRegionBlocks;
	const float z0 = float(aRegionZ * kRegionBlocks);
	const float z1 = z0 + kRegionBlocks;
	const float y = float(aY);
	const proto::ColTri first{ { x0, y, z0, x1, y, z0, x1, y, z1 }, 0 };
	const proto::ColTri second{ { x0, y, z0, x1, y, z1, x0, y, z1 }, 0 };
	std::vector<uint8_t> bytes;
	Append(bytes, region);
	Append(bytes, first);
	Append(bytes, second);
	return bytes;
```
```cpp
	std::vector<uint8_t> bytes;
	Append(bytes, region);
	for (const McTriangle& triangle : Triangles(aRegionX, aRegionZ, aY))
	{
		proto::ColTri tri{};  // flags 0: not diggable, so attacking the floor does nothing
		std::memcpy(tri.v, triangle.v, sizeof tri.v);
		Append(bytes, tri);
	}
	return bytes;
}

std::vector<std::vector<uint8_t>> FloorVoxels(int32_t aRegionX, int32_t aRegionZ, double aY, uint32_t aEpoch)
{
	const int32_t                      regionY = RegionOf(aY);
	const std::array<McTriangle, 2>    triangles = Triangles(aRegionX, aRegionZ, aY);
	const std::vector<proto::ColBlock> blocks = VoxelizeRegion(aRegionX, regionY, aRegionZ, triangles);
	return { RegionPayload(aRegionX, regionY, aRegionZ, aEpoch, blocks), RegionPayload(aRegionX, regionY - 1, aRegionZ, aEpoch, {}),
		     RegionPayload(aRegionX, regionY + 1, aRegionZ, aEpoch, {}) };
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `250 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the floor's voxels" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 4: Build mode's drive, and V's eye height

The drive's states, pure (the spec's "Entering" and "Each frame while Minecraft drives"):
- **Waiting:** build mode has started with one teleport of Minecraft's player to V's feet, and Minecraft still holds
  its player. V is held where it was, so a movement key that was down when build mode started can't walk V away.
- **Driving:** Minecraft has let go. Each frame V's feet go straight under Minecraft's eye, V's eye height below it, so
  V's camera lands on the eye.
- **Waiting again:** if the acknowledgement falls behind, Minecraft has teleported its player by itself and holds it
  again: V stays where it is until Minecraft lets go.
- **A bad target:** not finite, or more than 20 m from V. It isn't used, and a second of them ends build mode, saying
  why.

`EyeHeight` measures V's eye height outside build mode: the camera's height above V's feet, averaged over the frames
where the feet don't move up or down, from spike 4's 1.60 m until a frame counts.

**Files:**
- Create: `src/link/Drive.h`, `src/link/Drive.cpp`, `tests/TestDrive.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestDrive.cpp`**

```cpp
#include "Harness.h"

#include "link/Drive.h"

#include <limits>

using namespace cpmc;
using namespace cpmc::link;

namespace
{
// Minecraft's player at spike 3's corner, aEast blocks east: feet at Minecraft -119.5 6.14 2007.5, the eye aEyeHeight up.
TickSample Standing(double aEast = 0.0, double aEyeHeight = 1.62)
{
	TickSample sample;
	sample.feet = { -119.5 + aEast, 6.14, 2007.5 };
	sample.eyeHeight = aEyeHeight;
	return sample;
}

const Vec3d kVFeet{ -119.5, -2007.5, 6.14 };  // world axes: the same spot
}

TEST(DrivePutsVsCameraOnMinecraftsEye)
{
	// V's eye height 1.60, Minecraft's 1.62: V's feet go 2 cm above Minecraft's, in world axes.
	const Vec3d feet = FeetUnderEye(Standing(), 1.60);
	CHECK_NEAR(feet.x, -119.5, 1e-9);
	CHECK_NEAR(feet.y, -2007.5, 1e-9);
	CHECK_NEAR(feet.z, 6.16, 1e-9);
	// Sneaking: Minecraft's eye drops to 1.27, and V's feet a third of a metre into the floor.
	CHECK_NEAR(FeetUnderEye(Standing(0.0, 1.27), 1.60).z, 5.81, 1e-9);
}

TEST(DriveDoesNothingWhileOff)
{
	Drive      drive;
	const auto step = drive.Update(1.0, true, Standing(), kVFeet, 1.60);
	CHECK(!step.place.has_value());
	CHECK(step.end.empty());
	CHECK(drive.State() == DriveState::Off);
	CHECK_STR(DriveStateName(drive.State()), "off");
}

TEST(DriveHoldsVWhileMinecraftWaits)
{
	Drive drive;
	drive.Enter(kVFeet, 1.0);
	CHECK(drive.State() == DriveState::Waiting);
	// Minecraft hasn't let go yet: V stays where build mode started, though a key held from before walked it off.
	const auto step = drive.Update(1.1, false, Standing(3.0), { -118.0, -2007.5, 6.14 }, 1.60);
	CHECK(step.place.has_value());
	CHECK_NEAR(step.place->x, -119.5, 1e-9);
	CHECK(drive.State() == DriveState::Waiting);
	CHECK_NEAR(drive.Since(), 1.0, 1e-9);
	CHECK_STR(DriveStateName(drive.State()), "waiting");
}

TEST(DriveFollowsMinecraftOnceItLetsGo)
{
	Drive drive;
	drive.Enter(kVFeet, 1.0);
	const auto step = drive.Update(1.2, true, Standing(0.5), kVFeet, 1.60);
	CHECK(drive.State() == DriveState::Driving);
	CHECK_STR(DriveStateName(drive.State()), "driving");
	CHECK_NEAR(drive.Since(), 1.2, 1e-9);
	CHECK(step.place.has_value());
	CHECK_NEAR(step.place->x, -119.0, 1e-9);
	CHECK_NEAR(step.place->z, 6.16, 1e-9);
	CHECK(drive.Eye().has_value());
	CHECK_NEAR(drive.Eye()->z, 7.76, 1e-9);
}

TEST(DriveWaitsAgainWhenMinecraftTeleportsByItself)
{
	Drive drive;
	drive.Enter(kVFeet, 1.0);
	drive.Update(1.2, true, Standing(0.5), kVFeet, 1.60);
	// The acknowledgement falls behind: Minecraft holds its player again, wherever it teleported it. V stays put.
	const Vec3d now{ -119.0, -2007.5, 6.16 };
	const auto  step = drive.Update(1.3, false, Standing(9.0), now, 1.60);
	CHECK(drive.State() == DriveState::Waiting);
	CHECK_NEAR(step.place->x, -119.0, 1e-9);
	CHECK_NEAR(step.place->z, 6.16, 1e-9);
}

TEST(DriveSkipsTargetsTooFarAndEndsAfterASecond)
{
	Drive drive;
	drive.Enter(kVFeet, 1.0);
	drive.Update(1.1, true, Standing(), kVFeet, 1.60);
	auto step = drive.Update(1.2, true, Standing(25.0), kVFeet, 1.60);  // 25 m from V
	CHECK(!step.place.has_value());
	CHECK(step.end.empty());
	CHECK_EQ(drive.Skipped(), 1ull);
	step = drive.Update(2.1, true, Standing(25.0), kVFeet, 1.60);
	CHECK(step.end.empty());  // 0.9 s of bad targets
	step = drive.Update(2.2, true, Standing(25.0), kVFeet, 1.60);
	CHECK_STR(step.end, "Minecraft's player stayed over 20 m from V for a second");
	CHECK_EQ(drive.Skipped(), 3ull);
	// A good target in between starts the second over.
	Drive other;
	other.Enter(kVFeet, 1.0);
	other.Update(1.1, true, Standing(), kVFeet, 1.60);
	other.Update(1.2, true, Standing(25.0), kVFeet, 1.60);
	other.Update(1.9, true, Standing(), kVFeet, 1.60);
	CHECK(other.Update(2.5, true, Standing(25.0), kVFeet, 1.60).end.empty());
}

TEST(DriveSkipsTargetsThatArentThere)
{
	Drive drive;
	drive.Enter(kVFeet, 1.0);
	drive.Update(1.1, true, Standing(), kVFeet, 1.60);
	TickSample nowhere = Standing();
	nowhere.feet.x = std::numeric_limits<double>::quiet_NaN();
	CHECK(!drive.Update(1.2, true, nowhere, kVFeet, 1.60).place.has_value());
	CHECK(!drive.Update(1.3, true, std::nullopt, kVFeet, 1.60).place.has_value());
	CHECK_EQ(drive.Skipped(), 2ull);
	CHECK_STR(drive.Update(2.3, true, std::nullopt, kVFeet, 1.60).end, "Minecraft's player had no usable position for a second");
}

TEST(DriveLeaves)
{
	Drive drive;
	drive.Enter(kVFeet, 1.0);
	drive.Update(1.1, true, Standing(), kVFeet, 1.60);
	drive.Leave();
	CHECK(drive.State() == DriveState::Off);
	CHECK(!drive.Update(1.2, true, Standing(), kVFeet, 1.60).place.has_value());
}

TEST(EyeHeightStartsAtSpike4sMeasure)
{
	EyeHeight eye;
	CHECK_NEAR(eye.Value(), 1.60, 1e-9);
}

TEST(EyeHeightAveragesOnlyWhileTheFeetStayLevel)
{
	EyeHeight eye;
	eye.Note({ 0.0, 0.0, 6.0 }, { 0.0, 0.0, 7.62 });  // the first frame: nothing to compare the feet with
	CHECK_NEAR(eye.Value(), 1.60, 1e-9);
	eye.Note({ 0.1, 0.0, 6.0 }, { 0.1, 0.0, 7.62 });  // level: the first sample is taken as it is
	CHECK_NEAR(eye.Value(), 1.62, 1e-9);
	eye.Note({ 0.2, 0.0, 6.3 }, { 0.2, 0.0, 7.5 });  // the feet went up 0.3 m: a jump, or a read across a teleport
	CHECK_NEAR(eye.Value(), 1.62, 1e-9);
	eye.Note({ 0.2, 0.0, 6.3 }, { 0.2, 0.0, 7.82 });  // level again, at 1.52: a tenth of the way there
	CHECK_NEAR(eye.Value(), 1.61, 1e-9);
	eye.Note({ 0.2, 0.0, 6.3 }, { 0.2, 0.0, 16.3 });  // a camera 10 m up isn't V's eye
	CHECK_NEAR(eye.Value(), 1.61, 1e-9);
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestVoxels.cpp
	tools/mclink/McLinkScript.cpp
```
```cmake
	tests/TestVoxels.cpp
	tests/TestDrive.cpp
	tools/mclink/McLinkScript.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Drive.h'`.

- [x] **Step 4: Write `src/link/Drive.h`**

```cpp
#pragma once

#include "Coords.h"
#include "link/Ticks.h"

#include <cstdint>
#include <optional>
#include <string>

// Build mode's drive (the spec's "Part 1"): V placed where Minecraft's player is. Build mode starts with one teleport
// of Minecraft's player to V's feet; Minecraft holds its player until the ground around it has arrived, and meanwhile V
// is held where it is. Once Minecraft lets go, each frame V's camera goes to Minecraft's eye: V's feet under it, by V's
// own eye height. Pure: the plugin teleports V, and mclink moves its stand-in V, wherever it says.
namespace cpmc::link
{
enum class DriveState
{
	Off,
	Waiting,  // Minecraft holds its player: V is held where it is
	Driving,  // V follows Minecraft's player
};

const char* DriveStateName(DriveState aState);  // "off", "waiting", "driving"

// V's eye height: the camera's height above V's feet, averaged over frames where V's feet don't move up or down, so a
// jump, or a camera and feet read either side of a teleport, doesn't count. It's measured outside build mode only, and
// V can't crouch in build mode, so it holds while Minecraft drives. Spike 4's measure, 1.60 m, until a frame is taken.
class EyeHeight
{
public:
	void   Note(const Vec3d& aFeet, const Vec3d& aCamera);  // world axes
	double Value() const { return value_; }

private:
	std::optional<double> lastFeetZ_;
	bool                  measured_ = false;
	double                value_ = 1.60;
};

// Where V's feet go (world axes) so that V's camera, aEyeHeight above them, is at Minecraft's eye. Horizontally, they
// go straight under it.
Vec3d FeetUnderEye(const TickSample& aMinecraft, double aEyeHeight);

class Drive
{
public:
	static constexpr double kFarthest = 20.0;  // metres from V: a target farther away isn't used
	static constexpr double kBadFor = 1.0;     // seconds of targets not used that end build mode

	// Build mode started with V's feet at aFeet (world axes): V is held there until Minecraft lets go.
	void Enter(const Vec3d& aFeet, double aNow);
	void Leave();

	struct Step
	{
		std::optional<Vec3d> place;  // where V's feet go this frame (world axes); nothing: V is left as it is
		std::string          end;    // why build mode must end; empty: it goes on
	};

	// Once a frame while build mode is on. aAcked: Minecraft's teleportAck equals our teleportSeq, so it has let its
	// player go. aMinecraft: where its player is, interpolated, if it has ticked. aFeet: V's feet now (world axes).
	Step Update(double aNow, bool aAcked, const std::optional<TickSample>& aMinecraft, const Vec3d& aFeet, double aEyeHeight);

	DriveState           State() const { return state_; }
	double               Since() const { return since_; }      // when it entered its state
	uint64_t             Skipped() const { return skipped_; }  // frames whose target wasn't used, since the start
	std::optional<Vec3d> Eye() const { return eye_; }          // Minecraft's eye (world axes), the last frame it drove

private:
	DriveState            state_ = DriveState::Off;
	double                since_ = 0.0;
	Vec3d                 hold_;
	std::optional<double> badSince_;
	uint64_t              skipped_ = 0;
	std::optional<Vec3d>  eye_;
};
}
```

- [x] **Step 5: Write `src/link/Drive.cpp`**

```cpp
#include "link/Drive.h"

#include <cmath>

namespace cpmc::link
{
namespace
{
constexpr double kLevel = 0.005;     // metres the feet may move up or down between frames for a sample to count
constexpr double kEyeLowest = 0.5;   // a camera's height above the feet outside these isn't V's eye
constexpr double kEyeHighest = 2.5;
constexpr double kEyeWeight = 0.1;   // each sample's share of the average

bool Finite(const Vec3d& aPoint)
{
	return std::isfinite(aPoint.x) && std::isfinite(aPoint.y) && std::isfinite(aPoint.z);
}

double Distance(const Vec3d& aA, const Vec3d& aB)
{
	return std::sqrt((aA.x - aB.x) * (aA.x - aB.x) + (aA.y - aB.y) * (aA.y - aB.y) + (aA.z - aB.z) * (aA.z - aB.z));
}
}

const char* DriveStateName(DriveState aState)
{
	switch (aState)
	{
	case DriveState::Waiting: return "waiting";
	case DriveState::Driving: return "driving";
	default: return "off";
	}
}

void EyeHeight::Note(const Vec3d& aFeet, const Vec3d& aCamera)
{
	if (lastFeetZ_ && std::abs(aFeet.z - *lastFeetZ_) < kLevel)
	{
		const double height = aCamera.z - aFeet.z;
		if (height > kEyeLowest && height < kEyeHighest)
		{
			value_ = measured_ ? value_ + (height - value_) * kEyeWeight : height;
			measured_ = true;
		}
	}
	lastFeetZ_ = aFeet.z;
}

Vec3d FeetUnderEye(const TickSample& aMinecraft, double aEyeHeight)
{
	const Vec3d eye = WorldFromMc({ aMinecraft.feet.x, aMinecraft.feet.y + aMinecraft.eyeHeight, aMinecraft.feet.z });
	return { eye.x, eye.y, eye.z - aEyeHeight };
}

void Drive::Enter(const Vec3d& aFeet, double aNow)
{
	state_ = DriveState::Waiting;
	since_ = aNow;
	hold_ = aFeet;
	badSince_.reset();
	eye_.reset();
}

void Drive::Leave()
{
	state_ = DriveState::Off;
	badSince_.reset();
	eye_.reset();
}

Drive::Step Drive::Update(double aNow, bool aAcked, const std::optional<TickSample>& aMinecraft, const Vec3d& aFeet, double aEyeHeight)
{
	Step step;
	if (state_ == DriveState::Off)
	{
		return step;
	}
	if (state_ == DriveState::Driving && !aAcked)
	{
		// Minecraft teleported its player by itself, and holds it: V stays where it is until it lets go again.
		state_ = DriveState::Waiting;
		since_ = aNow;
		hold_ = aFeet;
		eye_.reset();
	}
	if (state_ == DriveState::Waiting)
	{
		if (!aAcked)
		{
			step.place = hold_;
			return step;
		}
		state_ = DriveState::Driving;
		since_ = aNow;
		badSince_.reset();
	}
	const char* bad = nullptr;
	if (!aMinecraft || !Finite(aMinecraft->feet) || !std::isfinite(aMinecraft->eyeHeight))
	{
		bad = "Minecraft's player had no usable position for a second";
	}
	else
	{
		const Vec3d target = FeetUnderEye(*aMinecraft, aEyeHeight);
		eye_ = Vec3d{ target.x, target.y, target.z + aEyeHeight };
		if (Distance(target, aFeet) > kFarthest)
		{
			bad = "Minecraft's player stayed over 20 m from V for a second";
		}
		else
		{
			badSince_.reset();
			step.place = target;
			return step;
		}
	}
	++skipped_;
	if (!badSince_)
	{
		badSince_ = aNow;
	}
	if (aNow - *badSince_ >= kBadFor)
	{
		step.end = bad;
	}
	return step;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Voxels.cpp
	src/render/PendingSections.cpp
```
```cmake
	src/link/Voxels.cpp
	src/link/Drive.cpp
	src/render/PendingSections.cpp
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `260 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: build mode's drive, and V's eye height" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 5: The link's teleports start from the clock, and the floor sends its voxels

Two changes to the link before the drive:
- **`teleportSeq` starts from the clock, odd,** as SkyCraft's Skyrim side starts its own. Minecraft teleports its
  player only when the sequence changes, and keeps the last value it applied when a new game side appears. A second
  run that started from 1 again could repeat that value, and its first teleport would be ignored (the spec's "Found
  while writing"). The tests give the link their own start, 0.
- **The floor sends its voxels:** each region's triangles, then its three `FloorVoxels` messages. A floor is now the
  clear, then 49 regions of four messages each.

**Files:**
- Modify: `src/link/Link.h`, `src/link/Link.cpp`, `tests/TestLink.cpp`

- [x] **Step 1: Edit `tests/TestLink.cpp`: the failing test**

```cpp
#include "link/Link.h"
```
```cpp
#include "link/Floor.h"
#include "link/Link.h"
```
```cpp
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	LinkSettings  settings;
```
```cpp
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
	Sink          sink;
	LinkSettings  settings;
```
```cpp
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	link.Update(At(1.0), kSettings, sink);
```
```cpp
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
	Sink          sink;
	link.Update(At(1.0), kSettings, sink);
```
```cpp
TEST(LinkTeleportsByStepAndRate)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
```
```cpp
TEST(LinkStartsItsTeleportSequenceFromTheClock)
{
	// A Minecraft that kept running keeps the last teleportSeq it applied, and teleports only when it changes. A new
	// game side starting from 0 again could repeat it: no teleport, and an acknowledgement that looks like an arrival.
	// So the sequence starts from the clock, as SkyCraft's does, and odd, so never 0.
	CHECK(FirstTeleportSeq() % 2 == 1);
	FakeMinecraft  minecraft;
	Link           link(minecraft.base);
	const uint32_t first = link.State().teleportSeq;
	CHECK(first % 2 == 1);
	Sink sink;
	link.Update(At(1.0), kSettings, sink);  // linked, with V: the first teleport
	CHECK_EQ(minecraft.Sky().teleportSeq, first + 1);
}

TEST(LinkTeleportsByStepAndRate)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
```
```cpp
	CHECK_EQ(messages.size(), size_t(50));
	CHECK_EQ(messages[0].type, uint32_t(proto::kColClear));
	CHECK(payloads[0] == std::vector<uint8_t>({ 1, 0, 0, 0 }));
	CHECK_EQ(messages[1].type, uint32_t(proto::kColTris));
```
```cpp
	CHECK_EQ(messages.size(), size_t(1 + 49 * 4));  // the clear, then each region's triangles and three voxel regions
	CHECK_EQ(messages[0].type, uint32_t(proto::kColClear));
	CHECK(payloads[0] == std::vector<uint8_t>({ 1, 0, 0, 0 }));
	CHECK_EQ(messages[1].type, uint32_t(proto::kColTris));
	CHECK_EQ(messages[2].type, uint32_t(proto::kColRegion));
	CHECK_EQ(messages[3].type, uint32_t(proto::kColRegion));
	CHECK_EQ(messages[4].type, uint32_t(proto::kColRegion));
	CHECK_EQ(messages[5].type, uint32_t(proto::kColTris));
	CHECK(payloads[2] == FloorVoxels(-18, 247, 6.14, 1)[0]);  // the first region's, the floor's own
```
```cpp
	CHECK_EQ(minecraft.Collision(payloads).size(), size_t(50));
```
```cpp
	CHECK_EQ(minecraft.Collision(payloads).size(), size_t(1 + 49 * 4));
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C3861: 'FirstTeleportSeq': identifier not found`.

- [x] **Step 3: Edit `src/link/Link.h`**

```cpp
class Link
{
public:
	explicit Link(uint8_t* aBase);  // the mapping's memory, laid out as skycraft_protocol.h has it
```
```cpp
// Where a run's teleportSeq starts: from the clock, and odd, as SkyCraft's Skyrim side starts its own. A Minecraft that
// kept running keeps the last value it applied and teleports only on a change, so a run must not repeat it.
uint32_t FirstTeleportSeq();

class Link
{
public:
	// aBase: the mapping's memory, laid out as skycraft_protocol.h has it. aFirstTeleportSeq: tests give their own.
	explicit Link(uint8_t* aBase, uint32_t aFirstTeleportSeq = FirstTeleportSeq());
```

- [x] **Step 4: Edit `src/link/Link.cpp`**

```cpp
Link::Link(uint8_t* aBase)
    : base_(aBase), overlay_(aBase + proto::kOffOverlayCtl, aBase + proto::kOffOverlaySlotHdr, aBase + proto::kOffOverlayPixels,
                             proto::kOverlaySlotBytes)
{
```
```cpp
uint32_t FirstTeleportSeq()
{
	return uint32_t(std::chrono::steady_clock::now().time_since_epoch().count()) | 1u;
}

Link::Link(uint8_t* aBase, uint32_t aFirstTeleportSeq)
    : base_(aBase), overlay_(aBase + proto::kOffOverlayCtl, aBase + proto::kOffOverlaySlotHdr, aBase + proto::kOffOverlayPixels,
                             proto::kOverlaySlotBytes)
{
	status_.teleportSeq = aFirstTeleportSeq;
```
```cpp
	{
		sent = sent && WriteByteRing(ring, proto::kColRingDataBytes, proto::kColTris, FloorRegion(x, z, aFeet.y, epoch));
```
```cpp
	{
		// Each region's triangles first, then its voxels, as SkyCraft sends them.
		sent = sent && WriteByteRing(ring, proto::kColRingDataBytes, proto::kColTris, FloorRegion(x, z, aFeet.y, epoch));
		for (const std::vector<uint8_t>& voxels : FloorVoxels(x, z, aFeet.y, epoch))
		{
			sent = sent && WriteByteRing(ring, proto::kColRingDataBytes, proto::kColRegion, voxels);
		}
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `261 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the link's teleports start from the clock, and the floor sends its voxels" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 6: The link drives V from Minecraft's player in build mode

`Link::Update` gains the drive (the spec's "Entering", "Each frame while Minecraft drives" and "Leaving"):
- **Every frame:** Minecraft's ticks go into the interpolator, on the frame's `QueryPerformanceCounter`, so it has
  settled by the time Minecraft drives. A new Minecraft process resets it.
- **Outside build mode:** V's eye height is measured.
- **Entering** (`Frame::buildMode` turning on): one teleport of Minecraft's player to V's feet, the drive waiting, and
  a fresh floor at V's feet.
- **In build mode:**
  - `Status::placeV` says where V's feet go this frame, and `endBuild` why build mode must end;
  - no teleports go to Minecraft, so no click is refused;
  - `SkyState`'s position is V's feet, and its look still V's camera's;
  - the floor follows Minecraft's player while it drives (V's feet while it waits), and grows past 16 m without a new
    epoch, so nothing is cleared from under the player.
- **Leaving:** the teleports resume from V's camera at once.
- **The status** gains Minecraft's flags and acknowledgement, the drive's state and since when, frames skipped, V's eye
  height, the render delay, frames that waited for a tick, and V's camera's distance from where the last frame put
  Minecraft's eye.

The tests' fake Minecraft gains ticks. `Corner()`'s camera moves to 1.60 m above the feet, spike 4's eye height.

**Files:**
- Modify: `src/link/Link.h`, `src/link/Link.cpp`, `tests/TestLink.cpp`

- [x] **Step 1: Edit `tests/TestLink.cpp`: the failing tests**

```cpp
		state.eyeZ = 2007.5;
```
```cpp
		state.eyeZ = 2007.5;
		state.frameCounter += 1;
		std::memcpy(base + proto::kOffMcState, &state, sizeof state);
	}

	// McState with a tick stamped at aStamp (microseconds, the tests' clock): Minecraft's player standing at aFeet
	// (Minecraft axes), its eye 1.62 up, having applied teleport aAck.
	void Ticked(uint32_t aAck, const Vec3d& aFeet, double aStamp)
	{
		proto::McState state{};
		std::memcpy(&state, base + proto::kOffMcState, sizeof state);
		state.seq += 2;
		state.flags = proto::kMcInWorld | proto::kMcOnGround;
		state.teleportAck = aAck;
		state.x = state.prevX = state.curX = aFeet.x;
		state.y = state.prevY = state.curY = aFeet.y;
		state.z = state.prevZ = state.curZ = aFeet.z;
		state.tickEyeO = state.tickEye = 1.62f;
		state.tickQpc = int64_t(aStamp * 1'000'000.0);
		state.tickMs = 50.0f;
```
```cpp
// V at spike 3's corner, looking north: feet on the road at z 6.14, the camera 1.62 m above.
GamePose Corner(double aEast = 0.0, double aUp = 0.0)
{
	return { { -119.5 + aEast, -2007.5, 6.14 + aUp }, { -119.5 + aEast, -2007.5, 7.76 + aUp }, { 0.0f, 1.0f, 0.0f } };
}

```
```cpp
// V at spike 3's corner, looking north: feet on the road at z 6.14, the camera 1.60 m above, as spike 4 measured.
GamePose Corner(double aEast = 0.0, double aUp = 0.0)
{
	return { { -119.5 + aEast, -2007.5, 6.14 + aUp }, { -119.5 + aEast, -2007.5, 7.74 + aUp }, { 0.0f, 1.0f, 0.0f } };
}

// A frame outside build mode, its clock in seconds and in microseconds for the ticks.
```
```cpp
	frame.tickMs = uint64_t(aNow * 1000.0);
	frame.pose = aPose;
	frame.linked = aLinked;
```
```cpp
	frame.tickMs = uint64_t(aNow * 1000.0);
	frame.qpc = int64_t(aNow * 1'000'000.0);
	frame.qpcPerSecond = 1'000'000;
	frame.pose = aPose;
	frame.linked = aLinked;
	return frame;
}

Link::Frame InBuild(double aNow, std::optional<GamePose> aPose = Corner())
{
	Link::Frame frame = At(aNow, aPose);
```
```cpp
	CHECK_EQ(sky.flags, uint32_t(proto::kSkyInGame));  // build mode on: no menu flag
	CHECK_EQ(sky.teleportSeq, 1u);
	CHECK_NEAR(sky.posX, -119.5, 1e-9);
	CHECK_NEAR(sky.posY, 6.14, 1e-9);
```
```cpp
	CHECK_EQ(sky.flags, uint32_t(proto::kSkyInGame | proto::kSkyMenuOpen));  // build mode off: the menu flag
	CHECK_EQ(sky.teleportSeq, 1u);
	CHECK_NEAR(sky.posX, -119.5, 1e-9);
	CHECK_NEAR(sky.posY, 6.12, 1e-9);  // the camera, less Minecraft's standing eye height
```
```cpp
	CHECK_EQ(minecraft.Sky().flags, 0u);  // unlinked
```
```cpp
	CHECK_EQ(minecraft.Sky().flags, uint32_t(proto::kSkyMenuOpen));  // unlinked
```
```cpp
	CHECK_EQ(link.State().inputsDropped, 1ull);
}
```
```cpp
	CHECK_EQ(link.State().inputsDropped, 1ull);
}

TEST(LinkEntersBuildModeWithOneTeleportToVsFeet)
{
	FakeMinecraft                     minecraft;
	Link                              link(minecraft.base, 0);
	Sink                              sink;
	std::vector<std::vector<uint8_t>> payloads;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), kSettings, sink);
	CHECK_EQ(minecraft.Sky().teleportSeq, 1u);  // outside build mode: Minecraft's eye at V's camera
	CHECK_EQ(link.State().floorEpoch, 1u);
	minecraft.Collision(payloads);
	link.Update(InBuild(1.1), kSettings, sink);
	// One teleport, to V's feet, and a fresh floor there.
	CHECK_EQ(minecraft.Sky().teleportSeq, 2u);
	CHECK_NEAR(minecraft.Sky().posY, 6.14, 1e-9);
	CHECK_EQ(minecraft.Sky().flags, uint32_t(proto::kSkyInGame));
	CHECK_EQ(link.State().floorEpoch, 2u);
	CHECK_EQ(minecraft.Collision(payloads).size(), size_t(1 + 49 * 4));
	CHECK(link.State().drive == DriveState::Waiting);
	CHECK(sink.Logged("build mode: Minecraft's player goes to V's feet"));
	// Minecraft hasn't let go: V is held where build mode started, though it moved (a key held from before).
	link.Update(InBuild(1.2, Corner(3.0)), kSettings, sink);
	CHECK(link.State().placeV.has_value());
	CHECK_NEAR(link.State().placeV->x, -119.5, 1e-9);
	CHECK_NEAR(link.State().placeV->z, 6.14, 1e-9);
	// No teleports in build mode, and the position is V's feet, every frame.
	CHECK_EQ(minecraft.Sky().teleportSeq, 2u);
	CHECK_NEAR(minecraft.Sky().posX, -116.5, 1e-9);
	CHECK_NEAR(minecraft.Sky().posY, 6.14, 1e-9);
}

TEST(LinkPlacesVUnderMinecraftsEyeOnceItLetsGo)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), kSettings, sink);
	link.Update(At(1.05), kSettings, sink);  // a second level frame: V's eye height, 1.60
	CHECK_NEAR(link.State().eyeHeight, 1.60, 1e-9);
	link.Update(InBuild(1.1), kSettings, sink);
	// Minecraft has applied teleport 2 and let its player go, and it has walked half a block east.
	minecraft.Ticked(2, { -119.0, 6.14, 2007.5 }, 1.15);
	link.Update(InBuild(1.2), kSettings, sink);
	CHECK(link.State().drive == DriveState::Driving);
	CHECK(sink.Logged("Minecraft let its player go"));
	CHECK(link.State().placeV.has_value());
	CHECK_NEAR(link.State().placeV->x, -119.0, 1e-9);
	CHECK_NEAR(link.State().placeV->y, -2007.5, 1e-9);
	CHECK_NEAR(link.State().placeV->z, 6.16, 1e-6);  // V's camera 1.60 up, at Minecraft's eye 1.62 up
	CHECK_EQ(link.State().teleportAck, 2u);
	CHECK(link.State().minecraftFlags & proto::kMcOnGround);
	CHECK_EQ(link.State().waitedFrames, 0ull);
	// Placed where it was sent: V's camera is right where Minecraft's eye was. No newer tick: this frame waited.
	link.Update(InBuild(1.25, Corner(0.5, 0.02)), kSettings, sink);
	CHECK(link.State().eyeDistance.has_value());
	CHECK_NEAR(*link.State().eyeDistance, 0.0, 1e-6);
	CHECK_EQ(link.State().waitedFrames, 1ull);
}

TEST(LinkKeepsItsFloorWhileMinecraftDrives)
{
	FakeMinecraft                     minecraft;
	Link                              link(minecraft.base, 0);
	Sink                              sink;
	std::vector<std::vector<uint8_t>> payloads;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), kSettings, sink);
	link.Update(InBuild(1.1), kSettings, sink);
	minecraft.Ticked(2, { -119.5, 6.14, 2007.5 }, 1.15);
	link.Update(InBuild(1.2), kSettings, sink);
	minecraft.Collision(payloads);
	// V placed 1.5 m up, Minecraft's player having jumped: the floor stays.
	link.Update(InBuild(1.3, Corner(0.0, 1.5)), kSettings, sink);
	CHECK(minecraft.Collision(payloads).empty());
	// Minecraft's player walked 20 m east: the floor grows around it at its height, with no clear, so no new epoch.
	minecraft.Ticked(2, { -99.5, 6.14, 2007.5 }, 1.35);
	link.Update(InBuild(1.4, Corner(20.0)), kSettings, sink);
	const auto messages = minecraft.Collision(payloads);
	CHECK_EQ(messages.size(), size_t(49 * 4));
	CHECK_EQ(messages[0].type, uint32_t(proto::kColTris));
	CHECK_EQ(link.State().floorEpoch, 2u);
	CHECK_NEAR(link.State().floorCentre.x, -99.5, 1e-9);
	CHECK_NEAR(link.State().floorCentre.y, 6.14, 1e-9);
	CHECK(sink.Logged("floor 2 grows"));
}

TEST(LinkWaitsAgainWhenMinecraftHoldsItsPlayer)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), kSettings, sink);
	link.Update(InBuild(1.1), kSettings, sink);
	minecraft.Ticked(2, { -119.5, 6.14, 2007.5 }, 1.15);
	link.Update(InBuild(1.2), kSettings, sink);
	// Minecraft teleported its player by itself, and holds it: its acknowledgement falls behind. V stays where it is.
	minecraft.Ticked(1, { -50.0, 6.14, 2007.5 }, 1.25);
	link.Update(InBuild(1.3, Corner(1.0)), kSettings, sink);
	CHECK(link.State().drive == DriveState::Waiting);
	CHECK_NEAR(link.State().placeV->x, -118.5, 1e-9);
	CHECK(sink.Logged("Minecraft holds its player again"));
}

TEST(LinkFollowsVAgainAfterBuildMode)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), kSettings, sink);
	link.Update(InBuild(1.1), kSettings, sink);
	minecraft.Ticked(2, { -119.5, 6.14, 2007.5 }, 1.15);
	link.Update(InBuild(1.2), kSettings, sink);
	link.Update(At(1.3, Corner(1.0)), kSettings, sink);  // build mode off
	CHECK(link.State().drive == DriveState::Off);
	CHECK(!link.State().placeV.has_value());
	CHECK_EQ(minecraft.Sky().teleportSeq, 3u);  // a teleport at once, to V's camera
	CHECK_NEAR(minecraft.Sky().posX, -118.5, 1e-9);
	CHECK_NEAR(minecraft.Sky().posY, 6.12, 1e-9);
	CHECK_EQ(minecraft.Sky().flags, uint32_t(proto::kSkyInGame | proto::kSkyMenuOpen));
	CHECK(sink.Logged("build mode ended"));
	// In and out again without V moving: V's camera is 2 cm from where build mode put Minecraft's player, inside the
	// teleport step, and the teleport still goes at once.
	link.Update(InBuild(2.0, Corner(1.0)), kSettings, sink);
	CHECK_EQ(minecraft.Sky().teleportSeq, 4u);  // build mode's own
	link.Update(At(2.2, Corner(1.0)), kSettings, sink);
	CHECK_EQ(minecraft.Sky().teleportSeq, 5u);
}

TEST(LinkEndsBuildModeWhenMinecraftStaysTooFar)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), kSettings, sink);
	link.Update(InBuild(1.1), kSettings, sink);
	minecraft.Ticked(2, { -94.5, 6.14, 2007.5 }, 1.15);  // 25 m east of V, which stays put
	for (int frame = 0; frame <= 10; ++frame)
	{
		link.Update(InBuild(1.2 + frame * 0.1), kSettings, sink);
		CHECK(link.State().endBuild.empty() == (frame < 10));
	}
	CHECK_STR(link.State().endBuild, "Minecraft's player stayed over 20 m from V for a second");
	CHECK_EQ(link.State().driveSkipped, 11ull);
}

TEST(LinkMeasuresVsEyeHeightOutsideBuildModeOnly)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
	Sink          sink;
	GamePose      pose = Corner();
	pose.camera.z = 6.14 + 1.70;
	link.Update(At(1.0, pose), kSettings, sink);
	link.Update(At(1.1, pose), kSettings, sink);
	CHECK_NEAR(link.State().eyeHeight, 1.70, 1e-9);
	pose.camera.z = 6.14 + 1.20;
	link.Update(InBuild(1.2, pose), kSettings, sink);
	link.Update(InBuild(1.3, pose), kSettings, sink);
	CHECK_NEAR(link.State().eyeHeight, 1.70, 1e-9);
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'placeV': is not a member of 'cpmc::link::Link::Status'`.

- [x] **Step 3: Edit `src/link/Link.h`**

```cpp
#include "link/Commands.h"
```
```cpp
#include "link/Commands.h"
#include "link/Drive.h"
```
```cpp
#include "link/Rings.h"
```
```cpp
#include "link/Rings.h"
#include "link/Ticks.h"
```
```cpp
// The game side of SkyCraft's protocol, once a frame (the spec's "The link" and "Blocks"): the heartbeat and SkyState,
// teleports and the floor, typed commands, Minecraft's events, the render ring, and HUD images. It works on the
// mapping's memory, so the tests give it plain memory with a fake Minecraft.
```
```cpp
// The game side of SkyCraft's protocol, once a frame (the minecraft-link spec's "The link" and "Blocks"): the heartbeat
// and SkyState, teleports and the floor, typed commands, Minecraft's events, the render ring, and HUD images. Spike 5
// adds build mode's drive (its spec's "Part 1"): one teleport, then Minecraft drives and the link says where V goes.
// It works on the mapping's memory, so the tests give it plain memory with a fake Minecraft.
```
```cpp
		uint64_t                tickMs = 0;         // GetTickCount64(), the clock both heartbeats use
		std::optional<GamePose> pose;               // nothing: V isn't there
		bool                    linked = false;     // the user's Link or Unlink
		bool                    buildMode = false;  // off sets SkyState's menu flag: Minecraft releases held keys
```
```cpp
		uint64_t                tickMs = 0;         // GetTickCount64(), the clock both heartbeats use
		int64_t                 qpc = 0;            // QueryPerformanceCounter now: the clock Minecraft stamps its ticks with
		int64_t                 qpcPerSecond = 0;   // its rate; 0: no ticks are interpolated
		std::optional<GamePose> pose;               // nothing: V isn't there
		bool                    linked = false;     // the user's Link or Unlink
		bool                    buildMode = false;  // Minecraft drives (spike 5); off sets SkyState's menu flag
```
```cpp
		uint64_t              hudRefused = 0;
```
```cpp
		uint64_t              hudRefused = 0;
		// Build mode's drive (spike 5)
		uint32_t              minecraftFlags = 0;  // McState's: on ground, sneaking, sprinting, flying and the rest
		uint32_t              teleportAck = 0;     // the last teleportSeq Minecraft applied, less 1 while it holds its player
		DriveState            drive = DriveState::Off;
		double                driveSince = 0.0;     // when the drive entered its state, on Frame::now's clock
		uint64_t              driveSkipped = 0;     // frames whose target wasn't used, since the start
		double                eyeHeight = 1.60;     // V's, measured outside build mode
		double                renderDelayMs = 0.0;  // how far in the past Minecraft's ticks are interpolated
		uint64_t              waitedFrames = 0;     // frames, while driving, that used up the newest tick, since the start
		std::optional<double> eyeDistance;          // V's camera from where the last frame put Minecraft's eye, metres
		std::optional<Vec3d>  placeV;               // this frame: where V's feet go (world axes); nothing: V is left be
		std::string           endBuild;             // this frame: why build mode must end; empty: it goes on
```
```cpp
	void SendFloor(const Vec3d& aFeet, LinkSink& aSink);
```
```cpp
	// A floor around aCentre (Minecraft axes; its y is the height). A new epoch clears the last one first; without one,
	// the floor grows, which never takes the ground from under Minecraft's player.
	void SendFloor(const Vec3d& aCentre, bool aNewEpoch, LinkSink& aSink);
```
```cpp
	std::string           lastProblem_;
```
```cpp
	std::string           lastProblem_;
	TickInterpolator      ticks_;
	Drive                 drive_;
	EyeHeight             eyeHeight_;
	bool                  wasBuild_ = false;
	std::optional<Vec3d>  lastEye_;  // where the last frame put Minecraft's eye, world axes
```

- [x] **Step 4: Edit `src/link/Link.cpp`**

```cpp
			overlay_.Reset();
```
```cpp
			overlay_.Reset();
			ticks_.Reset();
```
```cpp
	status_.screenOpen = mc && (mc->flags & proto::kMcScreenOpen);
```
```cpp
	status_.screenOpen = mc && (mc->flags & proto::kMcScreenOpen);
	std::optional<TickSample> sample;
```
```cpp
		status_.minecraftFrames = mc->frameCounter;
	}
```
```cpp
		status_.minecraftFrames = mc->frameCounter;
		status_.minecraftFlags = mc->flags;
		status_.teleportAck = mc->teleportAck;
		sample = ticks_.Frame(*mc, aFrame.qpc, aFrame.qpcPerSecond);  // every frame, so it has settled when it drives
	}
	status_.renderDelayMs = ticks_.DelayMs();
```
```cpp
	if (inGame)
```
```cpp

	// Build mode (spike 5): one teleport, of Minecraft's player to V's feet, and then Minecraft drives and V follows.
	const bool build = inGame && aFrame.buildMode;
	status_.placeV.reset();
	status_.endBuild.clear();
	status_.eyeDistance.reset();
	if (aFrame.pose && !build)
	{
		eyeHeight_.Note(aFrame.pose->feet, aFrame.pose->camera);
	}
	if (build && !wasBuild_)
	{
		++status_.teleportSeq;
		++status_.teleports;
		status_.target = McFromWorld(aFrame.pose->feet);
		lastTeleportAt_ = aFrame.now;
		teleportDue_ = false;
		drive_.Enter(aFrame.pose->feet, aFrame.now);
		lastEye_.reset();
		if (there)
		{
			SendFloor(status_.target, true, aSink);
		}
		aSink.Log("build mode: Minecraft's player goes to V's feet, and V waits there until Minecraft lets it go");
	}
	else if (!build && wasBuild_)
	{
		drive_.Leave();
		teleportDue_ = true;  // Minecraft's player follows V again, from where it is
		aSink.Log("build mode ended: Minecraft's player follows V again");
	}
	wasBuild_ = build;
	if (build)
	{
		const DriveState  before = drive_.State();
		const double      since = drive_.Since();
		const bool        acked = mc && mc->teleportAck == status_.teleportSeq;
		const Drive::Step driveStep = drive_.Update(aFrame.now, acked, sample, aFrame.pose->feet, eyeHeight_.Value());
		status_.placeV = driveStep.place;
		status_.endBuild = driveStep.end;
		if (drive_.State() == DriveState::Driving && before == DriveState::Waiting)
		{
			aSink.Log(std::format("Minecraft let its player go after {:.2f} s: V follows it", aFrame.now - since));
		}
		else if (drive_.State() == DriveState::Waiting && before == DriveState::Driving)
		{
			aSink.Log("Minecraft holds its player again: V waits where it is");
		}
		if (drive_.State() == DriveState::Driving)
		{
			if (sample && sample->waited)
			{
				++status_.waitedFrames;
			}
			if (lastEye_)
			{
				status_.eyeDistance = Distance(aFrame.pose->camera, *lastEye_);
			}
		}
		lastEye_ = drive_.State() == DriveState::Driving && driveStep.place ? drive_.Eye() : std::nullopt;
		// The floor stays at its height while Minecraft drives, and grows around its player, never with a new epoch,
		// whose clear would take the ground from under it. Until Minecraft has its player at V, it's around V.
		const bool  driving = drive_.State() == DriveState::Driving && mc;
		const Vec3d centre = driving ? Vec3d{ mc->x, mc->y, mc->z } : McFromWorld(aFrame.pose->feet);
		if (there && (!status_.floorSent || floorRequested_))
		{
			SendFloor(McFromWorld(aFrame.pose->feet), true, aSink);
			floorRequested_ = false;
		}
		else if (there && std::hypot(centre.x - status_.floorCentre.x, centre.z - status_.floorCentre.z) > 16.0)
		{
			SendFloor({ centre.x, status_.floorCentre.y, centre.z }, false, aSink);
		}
	}
	else if (inGame)
```
```cpp
			SendFloor(feet, aSink);
			floorRequested_ = false;
		}
	}

```
```cpp
			SendFloor(feet, true, aSink);
			floorRequested_ = false;
		}
	}
	status_.drive = drive_.State();
	status_.driveSince = drive_.Since();
	status_.driveSkipped = drive_.Skipped();
	status_.eyeHeight = eyeHeight_.Value();

	// The position: the last teleport's target, or in build mode V's feet, every frame, so that a teleport Minecraft
	// makes by itself (a new player object, or the in-game flag coming back) puts its player where V is.
	const Vec3d     position = build ? McFromWorld(aFrame.pose->feet) : status_.target;
```
```cpp
	state.posX = status_.target.x;
	state.posY = status_.target.y;
	state.posZ = status_.target.z;
```
```cpp
	state.posX = position.x;
	state.posY = position.y;
	state.posZ = position.z;
```
```cpp
void Link::SendFloor(const Vec3d& aFeet, LinkSink& aSink)
{
	// A new epoch each time: its clear message drops the last floor, and anything half-sent.
	const uint32_t epoch = status_.floorEpoch + 1;
	uint8_t* const ring = base_ + proto::kOffCollisionRing;
	bool           sent = WriteByteRing(ring, proto::kColRingDataBytes, proto::kColClear, FloorClear(epoch));
	for (const auto& [x, z] : FloorRegions(aFeet.x, aFeet.z))
	{
		// Each region's triangles first, then its voxels, as SkyCraft sends them.
		sent = sent && WriteByteRing(ring, proto::kColRingDataBytes, proto::kColTris, FloorRegion(x, z, aFeet.y, epoch));
		for (const std::vector<uint8_t>& voxels : FloorVoxels(x, z, aFeet.y, epoch))
```
```cpp
void Link::SendFloor(const Vec3d& aCentre, bool aNewEpoch, LinkSink& aSink)
{
	// A new epoch's clear message drops the last floor, and anything half-sent. Without one, the regions sent replace
	// those already there, and the rest stay.
	const uint32_t epoch = aNewEpoch ? status_.floorEpoch + 1 : status_.floorEpoch;
	uint8_t* const ring = base_ + proto::kOffCollisionRing;
	bool           sent = !aNewEpoch || WriteByteRing(ring, proto::kColRingDataBytes, proto::kColClear, FloorClear(epoch));
	for (const auto& [x, z] : FloorRegions(aCentre.x, aCentre.z))
	{
		// Each region's triangles first, then its voxels, as SkyCraft sends them.
		sent = sent && WriteByteRing(ring, proto::kColRingDataBytes, proto::kColTris, FloorRegion(x, z, aCentre.y, epoch));
		for (const std::vector<uint8_t>& voxels : FloorVoxels(x, z, aCentre.y, epoch))
```
```cpp
		status_.floorCentre = aFeet;
		aSink.Log(std::format("floor {} at height {:.2f} around Minecraft {:.1f} {:.1f}", epoch, aFeet.y, aFeet.x, aFeet.z));
```
```cpp
		status_.floorCentre = aCentre;
		aSink.Log(aNewEpoch ? std::format("floor {} at height {:.2f} around Minecraft {:.1f} {:.1f}", epoch, aCentre.y, aCentre.x, aCentre.z)
		                    : std::format("floor {} grows to Minecraft {:.1f} {:.1f}, at height {:.2f}", epoch, aCentre.x, aCentre.z, aCentre.y));
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `268 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the link drives V from Minecraft's player in build mode" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 7: `mclink` drives Minecraft's player

`mclink` becomes step 1's game side for the drive (the spec's "Step 1"):
- **`build on|off`** now turns the drive on and off, and starts off. While the drive places V, the stand-in V goes
  where the link puts it, as the plugin teleports V. When build mode ends, Minecraft lets go of everything, as the
  plugin has it do.
- **`down <scancode>` and `up <scancode>`** hold and release a key, as SDL scancodes, which SkyCraft forwards and the
  user's instance binds: W 26 walks, Space 44 jumps, Left Shift 225 toggles sprint, Left Ctrl 224 sneaks.
- **`track <seconds>`** prints the drive every 0.25 s: Minecraft's feet, eye height and flags, the acknowledgement,
  the drive's state, the stand-in V's feet, the render delay, and frames that waited or were skipped.
- **The ticks** run on `QueryPerformanceCounter`, as in the game; `status` gains the drive's line.
- **Two scripts:** `drive.txt`, step 1 itself, and `relink.txt`, a second game side's first link.

`ScriptsInTheRepoParse` parses every script in `tools/mclink/`, so a slip in one fails a test here, not step 1.

**Files:**
- Modify: `tools/mclink/McLinkScript.h`, `tools/mclink/McLinkScript.cpp`, `tools/mclink/Main.cpp`,
  `tests/TestMcLinkScript.cpp`
- Create: `tools/mclink/drive.txt`, `tools/mclink/relink.txt`

- [x] **Step 1: Edit `tests/TestMcLinkScript.cpp`: the failing tests**

```cpp
using namespace cpmc::mclink;
using Kind = Action::Kind;
```
```cpp
#include <fstream>
#include <sstream>

using namespace cpmc::mclink;
using Kind = Action::Kind;

TEST(ScriptsInTheRepoParse)
{
	// The tests run in spikes/block-render, where the scripts are.
	for (const char* name : { "step1.txt", "again.txt", "eye.txt", "drive.txt", "relink.txt" })
	{
		std::ifstream     in(std::string("tools/mclink/") + name, std::ios::binary);
		std::stringstream text;
		text << in.rdbuf();
		std::string error;
		const auto  actions = ParseScript(text.str(), error);
		CHECK(in.good() || in.eof());
		CHECK(actions.has_value() && !actions->empty());
		if (!actions)
		{
			std::printf("  %s: %s\n", name, error.c_str());
		}
	}
}
```
```cpp
TEST(ScriptTypeKeepsTheWholeCommand)
```
```cpp
TEST(ScriptParsesHeldKeysAndTracking)
{
	std::string error;
	const auto  actions = ParseScript("down 26\nwait 2\nup 26\ntrack 5\n", error);
	CHECK(actions.has_value());
	CHECK_EQ(actions->size(), size_t(4));
	const auto& a = *actions;
	CHECK(a[0].kind == Kind::Down);
	CHECK_EQ(a[0].numbers[0], 26.0);
	CHECK(a[2].kind == Kind::Up);
	CHECK_EQ(a[2].numbers[0], 26.0);
	CHECK(a[3].kind == Kind::Track);
	CHECK_EQ(a[3].numbers[0], 5.0);
	CHECK(!ParseScript("down 0", error).has_value());
	CHECK(!ParseScript("up 600", error).has_value());
	CHECK(!ParseScript("down", error).has_value());
	CHECK(!ParseScript("track 0", error).has_value());
	CHECK_STR(error, "line 1: track takes a number of seconds: \"track 0\"");
}

TEST(ScriptTypeKeepsTheWholeCommand)
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2065: 'Down': undeclared identifier`.

- [x] **Step 3: Edit `tools/mclink/McLinkScript.h`**

```cpp
//   build on|off                   build mode: off sets SkyState's menu flag (on by default)
//   hud on|off                     take HUD images (on by default)
//   at <x> <y> <z>                 Minecraft's player's feet, in Minecraft blocks
//   look <yaw> <pitch>             Minecraft degrees
//   type <command or set>          type it, and wait until it's typed (10 s at most)
//   key <scancode>                 press and release an SDL scancode
```
```cpp
//   build on|off                   build mode (off at first): on, Minecraft drives and the stand-in V follows; off, the
//                                  menu flag is set and Minecraft lets go of everything
//   hud on|off                     take HUD images (on by default)
//   at <x> <y> <z>                 the stand-in V's feet, in Minecraft blocks
//   look <yaw> <pitch>             Minecraft degrees
//   type <command or set>          type it, and wait until it's typed (10 s at most)
//   key <scancode>                 press and release an SDL scancode at once
//   down <scancode>                press an SDL scancode, and hold it
//   up <scancode>                  let it go
```
```cpp
//   hold-check <seconds>           how far Minecraft's feet stray from the last teleport target meanwhile
```
```cpp
//   hold-check <seconds>           how far Minecraft's feet stray from the last teleport target meanwhile
//   track <seconds>                print Minecraft's player and the drive four times a second meanwhile
```
```cpp
		Look,
		Type,
		Key,
		Click,
		Floor,
		SaveAtlas,
```
```cpp
		Look,
		Type,
		Key,
		Down,
		Up,
		Click,
		Floor,
		SaveAtlas,
```
```cpp
		HoldCheck,
```
```cpp
		HoldCheck,
		Track,
```

- [x] **Step 4: Edit `tools/mclink/McLinkScript.cpp`**

```cpp
		else if (verb == "key")
		{
			action.kind = Action::Kind::Key;
			if (!numbers(1) || action.numbers[0] < 1 || action.numbers[0] > 511 || action.numbers[0] != std::floor(action.numbers[0]))
			{
				return fail("key takes an SDL scancode, 1 to 511");
```
```cpp
		else if (verb == "key" || verb == "down" || verb == "up")
		{
			action.kind = verb == "key" ? Action::Kind::Key : verb == "down" ? Action::Kind::Down : Action::Kind::Up;
			if (!numbers(1) || action.numbers[0] < 1 || action.numbers[0] > 511 || action.numbers[0] != std::floor(action.numbers[0]))
			{
				return fail(std::format("{} takes an SDL scancode, 1 to 511", verb));
```
```cpp
		else if (verb == "hold-check")
		{
			action.kind = Action::Kind::HoldCheck;
			if (!numbers(1) || action.numbers[0] <= 0)
			{
				return fail("hold-check takes a number of seconds");
```
```cpp
		else if (verb == "hold-check" || verb == "track")
		{
			action.kind = verb == "hold-check" ? Action::Kind::HoldCheck : Action::Kind::Track;
			if (!numbers(1) || action.numbers[0] <= 0)
			{
				return fail(std::format("{} takes a number of seconds", verb));
```

- [x] **Step 5: Edit `tools/mclink/Main.cpp`**

```cpp
// mclink: a console stand-in for Cyberpunk's side of SkyCraft's protocol, for spike 4's step 1. It runs BlockRender's
// link code against the real Minecraft, without the game (docs/superpowers/specs/2026-10-02-minecraft-link-design.md).
//
//     mclink.exe <script.txt>
//
// It runs the script (McLinkScript.h) at 60 Hz with Minecraft's player pinned where the script says, and reports
// what Minecraft sends. It refuses to start while another game side uses the mapping.
```
```cpp
// mclink: a console stand-in for Cyberpunk's side of SkyCraft's protocol, for spike 4's step 1 and spike 5's. It runs
// BlockRender's link code against the real Minecraft, without the game (docs/superpowers/specs/2026-10-02-minecraft-
// link-design.md and 2026-10-03-collision-and-movement-design.md).
//
//     mclink.exe <script.txt>
//
// It runs the script (McLinkScript.h) at 60 Hz with a stand-in V where the script says, or, in build mode, where the
// drive puts it, as the plugin teleports V. It reports what Minecraft sends, and refuses to start while another game
// side uses the mapping.
```
```cpp
	    s.inputsDropped, s.commandsWaiting, s.hudImages, s.hudRefused);
```
```cpp
	    s.inputsDropped, s.commandsWaiting, s.hudImages, s.hudRefused);
	Say("drive: {}, teleport {} applied of {}; V's eye height {:.2f}; render delay {:.1f} ms, {} frames waited for a tick, {} targets skipped",
	    DriveStateName(s.drive), s.teleportAck, s.teleportSeq, s.eyeHeight, s.renderDelayMs, s.waitedFrames, s.driveSkipped);
}

// Minecraft's player's flags, as words.
std::string Flags(uint32_t aFlags)
{
	std::string words;
	for (const auto& [flag, word] : { std::pair{ proto::kMcOnGround, "on ground" }, std::pair{ proto::kMcSneaking, "sneaking" },
	                                  std::pair{ proto::kMcSprinting, "sprinting" }, std::pair{ proto::kMcFlying, "flying" } })
	{
		if (aFlags & flag)
		{
			words += std::string(words.empty() ? "" : ", ") + word;
		}
	}
	return words.empty() ? "in the air" : words;
```
```cpp
	Vec3d  feet{ -119.5, 6.14, 2007.5 };  // where V stood at spike 3's corner
	McLook look{ 180.0f, 0.0f };          // north
	bool   linked = false;
	bool   build = true;
	bool   hud = true;

```
```cpp
	Vec3d  feet{ -119.5, 6.14, 2007.5 };  // the stand-in V's, where V stood at spike 3's corner; its eye 1.62 up
	McLook look{ 180.0f, 0.0f };          // north
	bool   linked = false;
	bool   build = false;
	bool   hud = true;

	LARGE_INTEGER qpcRate;
	QueryPerformanceFrequency(&qpcRate);
```
```cpp
	uint64_t holdFrames = 0;
```
```cpp
	uint64_t holdFrames = 0;
	double   trackAt = 0.0;
```
```cpp
		}
		Link::Frame frame;
		frame.now = now;
		frame.tickMs = GetTickCount64();
```
```cpp
		}
		LARGE_INTEGER qpc;
		QueryPerformanceCounter(&qpc);
		Link::Frame frame;
		frame.now = now;
		frame.tickMs = GetTickCount64();
		frame.qpc = qpc.QuadPart;
		frame.qpcPerSecond = qpcRate.QuadPart;
```
```cpp
		const auto& state = link.State();
```
```cpp
		const auto& state = link.State();
		if (state.placeV)
		{
			feet = McFromWorld(*state.placeV);  // the stand-in V goes where the drive puts it, as the plugin teleports V
		}
		if (!state.endBuild.empty())
		{
			Say("build mode ends: {}", state.endBuild);
			build = false;
			link.Push({ proto::kInReleaseAll, 0, 0, 0, 0 });
		}
```
```cpp
			case Action::Kind::Build: build = action.on; break;
```
```cpp
			case Action::Kind::Build:
				if (build && !action.on)
				{
					link.Push({ proto::kInReleaseAll, 0, 0, 0, 0 });  // as the plugin does when build mode ends
				}
				build = action.on;
				break;
```
```cpp
				break;
			case Action::Kind::Click:
```
```cpp
				break;
			case Action::Kind::Down:
			case Action::Kind::Up:
				link.Push({ proto::kInKey, uint16_t(action.numbers[0]), action.kind == Action::Kind::Down ? 1 : 0, 0, 0 });
				break;
			case Action::Kind::Track: trackAt = now; break;
			case Action::Kind::Click:
```
```cpp
			break;
		case Action::Kind::Quit: next = script->size(); break;
```
```cpp
			break;
		case Action::Kind::Track:
			if (now >= trackAt)
			{
				trackAt += 0.25;
				const double since = state.drive == DriveState::Off ? 0.0 : now - state.driveSince;
				Say("track: Minecraft's feet {:.3f} {:.3f} {:.3f}, eye {:.2f} up, {}; teleport {} applied of {}; drive {}{}; V's feet "
				    "{:.3f} {:.3f} {:.3f}; delay {:.1f} ms, {} frames waited, {} skipped",
				    state.minecraftFeet.x, state.minecraftFeet.y, state.minecraftFeet.z, state.minecraftEye.y - state.minecraftFeet.y,
				    Flags(state.minecraftFlags), state.teleportAck, state.teleportSeq, DriveStateName(state.drive),
				    state.drive == DriveState::Off ? "" : std::format(" for {:.2f} s", since), feet.x, feet.y, feet.z, state.renderDelayMs,
				    state.waitedFrames, state.driveSkipped);
			}
			done = elapsed >= action.numbers[0];
			break;
		case Action::Kind::Quit: next = script->size(); break;
```

- [x] **Step 6: Write `tools/mclink/drive.txt`**

```text
# Spike 5's step 1: build mode's drive against the real Minecraft, without the game (docs/superpowers/specs/
# 2026-10-03-collision-and-movement-design.md, "Step 1"). Start Minecraft first: this waits for its world. The keys are
# SDL scancodes, and the user's CyberCraft instance binds them: W 26 walks, Space 44 jumps, Left Shift 225 toggles
# sprint, Left Ctrl 224 sneaks, and 1 (30) picks the first hotbar slot.
status
wait-world 180
at -119.5 6.14 2007.5
look 180 0
link on
wait 3
type /gamemode creative
type /item replace entity @s hotbar.0 with minecraft:stone 64
key 30
# Outside build mode the floor's voxels are known, so Minecraft lets its player go after the link's teleport.
track 1
# Build mode: one teleport to V's feet, then Minecraft drives, and the stand-in V follows it.
build on
track 2
# Walk north for 2 s, then sprint for 2 s, then stop.
down 26
track 2
key 225
track 2
up 26
key 225
track 1
# Sneak: the eye drops to 1.27.
down 224
track 1.5
up 224
track 1
# A wall of stone bricks 3 blocks north stops the walk.
type /fill ~-2 ~ ~-3 ~2 ~2 ~-3 minecraft:stone_bricks
down 26
track 2
up 26
# Jump onto a gold block 3 blocks east.
look 270 0
type /setblock ~3 ~ ~ minecraft:gold_block
down 26
down 44
track 1.5
up 44
up 26
track 1
# Fly: Space twice, then up for a second, down for a second, and Space twice again to fall back to the floor.
look 180 0
down 44
wait 0.1
up 44
wait 0.1
down 44
wait 0.1
up 44
track 0.5
down 44
track 1
up 44
down 224
track 1
up 224
down 44
wait 0.1
up 44
wait 0.1
down 44
wait 0.1
up 44
track 2
# Twenty right clicks while strafing east, looking north and down: a row of stone on the floor, 2 m apart.
look 180 50
down 7
click right
wait 0.45
click right
wait 0.45
click right
wait 0.45
click right
wait 0.45
click right
wait 0.45
click right
wait 0.45
click right
wait 0.45
click right
wait 0.45
click right
wait 0.45
click right
wait 0.45
click right
wait 0.45
click right
wait 0.45
click right
wait 0.45
click right
wait 0.45
click right
wait 0.45
click right
wait 0.45
click right
wait 0.45
click right
wait 0.45
click right
wait 0.45
click right
wait 0.45
up 7
track 1
save-hud clicks.dds
# Count the stone by removing it: Minecraft's chat says how many blocks it filled. Then clear the rest.
look 180 0
type /fill ~-48 ~-2 ~-6 ~4 ~2 ~1 minecraft:air replace minecraft:stone
wait 1
save-hud count.dds
type /fill ~-56 ~-3 ~-8 ~4 ~5 ~8 minecraft:air
# Build mode off: Minecraft lets go of everything, and follows the stand-in V by teleports again.
build off
track 1
status
quit
```

- [x] **Step 7: Write `tools/mclink/relink.txt`**

```text
# Spike 5's step 1: a new game side, with Minecraft still running, starts its teleportSeq from the clock, so its first
# link teleports Minecraft's player. Run it twice: Minecraft's log has a "teleported to" for each run.
status
wait-world 30
link on
wait 3
status
quit
```

- [x] **Step 8: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `270 test(s), 0 failure(s)`.

- [x] **Step 9: Run mclink without Minecraft**

Minecraft and the game must not be running.
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && printf 'status\nquit\n' > build/selfcheck.txt \
  && ./build/RelWithDebInfo/mclink.exe build/selfcheck.txt; echo "exit: $?"
```
Expected: `the mapping: created (process …); waiting for Minecraft`, then the status with `Minecraft: absent`, a
`game side:` line whose sequence is a large odd number (it starts from the clock), and `drive: off, teleport 0 applied
of …`. Then `exit: 0`.

- [x] **Step 10: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: mclink drives Minecraft's player, holds keys and tracks the drive" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 8: In Minecraft: step 1

Step 1 of the spec's "The steps", with the user: the drive run by `mclink` against SkyCraft's unchanged mod in the
`CyberCraft` instance, without the game.
- **The instance** has had SkyCraft 0.1.2 and Fabric API since spike 4, and the Java arguments
  `--enable-native-access=ALL-UNNAMED -Dskycraft.quitWithSkyrim=false`, so Minecraft keeps running between `mclink`
  runs. Its bindings: sneak on Left Ctrl, sprint on Left Shift (toggled), F for third person, F5 to swap hands, C and
  X to save and load toolbars.
- **If Minecraft doesn't link,** its log says why, in lines starting `SkyCraft:`.
- **If a check fails,** stop and find the cause with superpowers:systematic-debugging before going on. Fixes become new
  tasks.

**Files:**
- Create: `docs/spikes/2026-10-03-collision-and-movement-results.md`

- [x] **Step 1: Check that the game isn't running**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game: it would hold the mapping.

- [x] **Step 2: Ask the user to start Minecraft**

The user starts the `CyberCraft` instance from the Modrinth App, and says when its title screen shows. Then:
```bash
grep -E "SkyCraft:" "/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log" | tail -3
```
Expected: `SkyCraft: can't open Skyrim's shared memory yet (Windows error 2: …)`: waiting for a game side.

- [x] **Step 3: Run step 1's script**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/step1 && cd build/step1 \
  && ../RelWithDebInfo/mclink.exe ../../tools/mclink/drive.txt | tee drive.log; echo "exit: ${PIPESTATUS[0]}"
```
It takes about a minute once Minecraft is in its world, and up to four if it opens the world first: give the command
6 minutes. Expected, against the spec's "Step 1":
- **Linking:** `Minecraft is there (process …)`, then `floor 1 at height 6.14 around Minecraft -119.5 2007.5`. In the
  first `track` lines, outside build mode, the acknowledgement catches up with the sequence (`teleport S applied of
  S`): Minecraft let its player go, since the floor's voxels are known.
- **Entering:** after `build on`, `build mode: Minecraft's player goes to V's feet, …`, the drive `waiting`, then
  `Minecraft let its player go after … s: V follows it` within a second, and the drive `driving`.
- **The stand-in V follows:** in every `track` line while driving, V's feet within a few centimetres of Minecraft's
  feet: the stand-in's eye is 1.62 above its feet, as Minecraft's is.
- **Walking and sprinting:** with W held, Minecraft's feet move north (z falling) at about 4.3 blocks a second, then
  about 5.6 once Left Shift toggles sprint, with `sprinting` in the flags.
- **Sneaking:** with Left Ctrl held, `eye 1.27 up` and `sneaking`, and V's feet 0.35 below Minecraft's.
- **The wall:** the walk north stops against the stone bricks' south face, about 2 blocks on (the wall is 3 blocks
  north of the player's block), and stays there.
- **The gold block:** facing east with W and Space held, Minecraft's feet rise onto it, to 7.00: the block fills y 6
  to 7, and the floor is at 6.14.
- **Flying:** Space twice gives `flying`; Space held moves the feet up, and Left Ctrl down; Space twice again, and the
  player falls back to the floor, `on ground`.
- **The interpolator:** the render delay settles between 4 and 30 ms, and few frames wait for a late tick.
- **Leaving:** after `build off`, `build mode ended: Minecraft's player follows V again`, the drive `off`, and the
  last status's teleport count higher than before `build on`.
- **No `build mode ends:` line,** and `exit: 0`.

- [x] **Step 4: Look at the HUD images**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && uv run --no-project -q --with numpy --with pillow python tools/view_dump.py build/step1
```
Expected: `clicks.dds` and `count.dds` converted. Read `build/step1/clicks.png` and `build/step1/count.png`, and check:
- **`clicks.png`:** no red text in the chat, so no click was refused;
- **`count.png`:** the chat says the `/fill` replaced 20 blocks: all twenty right clicks while strafing placed stone.

- [x] **Step 5: Run the relink script twice**

Wait 10 seconds after Step 3: Minecraft drops the link after 8 s, and keeps running. Then:
```bash
L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"; \
  before=$(grep -c "SkyCraft: teleported to -119.5 6.14 2007.5" "$L"); \
  cd /d/Cyberpunk-Minecraft/spikes/block-render/build/step1 && ../RelWithDebInfo/mclink.exe ../../tools/mclink/relink.txt | tee relink1.log; \
  sleep 10; ../RelWithDebInfo/mclink.exe ../../tools/mclink/relink.txt | tee relink2.log; \
  echo "teleports logged: $(( $(grep -c "SkyCraft: teleported to -119.5 6.14 2007.5" "$L") - before ))"
```
Expected:
- each run's first `game side:` line has a different sequence, and its last says `teleport S applied of S` with its
  own sequence;
- `teleports logged:` at least 2: each run's first link teleported Minecraft's player, though Minecraft kept running.

- [x] **Step 6: Ask the user to stop Minecraft**

Its window is hidden, so the user stops it from the Modrinth App.

- [x] **Step 7: Write step 1's results, `docs/spikes/2026-10-03-collision-and-movement-results.md`**

From `build/step1/drive.log`, the relink logs, the two PNGs and Minecraft's log, as spike 4's results doc does:
- the versions;
- each check above with its outcome and numbers: the release's time, the speeds, the eye heights, where the wall
  stopped the walk, the render delay and late ticks, and the clicks;
- anything that surprised, and what it means for step 2.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-03-collision-and-movement-results.md \
  && git commit -q -m "collision-and-movement: step 1's results" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 9: Build mode gives Minecraft the movement keys, and hides the game's crouch keys

The spec's "Keys": in build mode, W, A, S, D, Space, Left Shift and Left Ctrl are forwarded to Minecraft and hidden
from the game. C and Right Ctrl are hidden without going anywhere: C toggles the game's crouch and Right Ctrl holds it,
and neither should crouch V under Minecraft's player. C is also Minecraft's toolbar-saving key in the user's instance.
- **Where a press goes:** each key follows spike 4's rule, that a press's repeats and its release go where the press
  went, so a key held when build mode starts stays the game's until it's released.
- **Shift+F6** still leaves build mode with Left Shift forwarded, since `BuildMode` notes both Shift keys before it
  decides anything else.

Two old test lines used W as a key build mode leaves alone: `BuildModeTakesOnlyTheKeys1To9` (now
`BuildModeTakesTheKeys1To9`) uses E instead, and `TestRouter` uses F, which V still interacts with.

**Files:**
- Modify: `src/input/BuildMode.h`, `src/input/BuildMode.cpp`, `tests/TestBuildMode.cpp`, `tests/TestRouter.cpp`

- [x] **Step 1: Edit `tests/TestBuildMode.cpp`: the failing tests**

```cpp
TEST(BuildModeTakesOnlyTheKeys1To9)
```
```cpp
TEST(BuildModeTakesTheKeys1To9)
```
```cpp
	CHECK(Passes(mode.Key(Press(0x11), Ready())));        // W
```
```cpp
	CHECK(Passes(mode.Key(Press(0x12), Ready())));        // E: the game's iconic weapon, still the game's
```
```cpp
	CHECK_STR(mode.Why(), "a build off request");
}
```
```cpp
	CHECK_STR(mode.Why(), "a build off request");
}

TEST(BuildModeGivesMinecraftTheMovementKeys)
{
	// W, A, S, D, Space, Left Shift and Left Ctrl as set-1 scan codes, and the SDL scancodes Minecraft gets for them.
	const std::pair<uint16_t, uint16_t> keys[] = { { 0x11, 26 }, { 0x1E, 4 },  { 0x1F, 22 }, { 0x20, 7 },
		                                           { 0x39, 44 }, { 0x2A, 225 }, { 0x1D, 224 } };
	BuildMode                           mode = On();
	for (const auto& [scanCode, sdl] : keys)
	{
		const Decision down = mode.Key(Press(scanCode), Ready());
		CHECK(down.hideKey && down.forwarded == 1 && Is(down.forward[0], proto::kInKey, sdl, 1));
		const Decision repeat = mode.Key(Press(scanCode), Ready());  // held: Windows repeats the press
		CHECK(repeat.hideKey && repeat.forwarded == 1 && Is(repeat.forward[0], proto::kInKey, sdl, 1));
		const Decision up = mode.Key(Release(scanCode), Ready());
		CHECK(up.hideKey && up.forwarded == 1 && Is(up.forward[0], proto::kInKey, sdl, 0));
	}
	CHECK_STR(mode.Last(), "Left Ctrl up");
	BuildMode off;  // outside build mode they're the game's
	for (const auto& [scanCode, sdl] : keys)
	{
		CHECK(Passes(off.Key(Press(scanCode), Ready())));
		CHECK(Passes(off.Key(Release(scanCode), Ready())));
	}
}

TEST(BuildModeHidesTheGamesCrouchKeys)
{
	// C toggles crouching and Right Ctrl holds it: hidden in build mode, so V can't crouch, and not forwarded.
	BuildMode mode = On();
	for (const RawKey& key : { Press(0x2E), Release(0x2E), Press(0x1D, true), Release(0x1D, true) })
	{
		const Decision decision = mode.Key(key, Ready());
		CHECK(decision.hideKey && decision.forwarded == 0);
	}
	BuildMode off;
	CHECK(Passes(off.Key(Press(0x2E), Ready())));
	CHECK(Passes(off.Key(Press(0x1D, true), Ready())));
}

TEST(BuildModeLeavesAMovementKeyHeldBeforeItWithTheGame)
{
	// W held when build mode starts: its repeats and its release are the game's, so the game lets go of it. Pressed
	// again, it's Minecraft's.
	BuildMode mode;
	CHECK(Passes(mode.Key(Press(0x11), Ready())));
	mode.Key(Press(kF8), Ready());
	CHECK(mode.On());
	CHECK(Passes(mode.Key(Press(0x11), Ready())));  // a repeat
	CHECK(Passes(mode.Key(Release(0x11), Ready())));
	const Decision again = mode.Key(Press(0x11), Ready());
	CHECK(again.hideKey && again.forwarded == 1);
}

TEST(BuildModeLeavesOnShiftF6WithLeftShiftForwarded)
{
	BuildMode      mode = On();
	const Decision shift = mode.Key(Press(0x2A), Ready());
	CHECK(shift.hideKey && shift.forwarded == 1);  // Minecraft's: in the user's instance it toggles sprint
	const Decision f6 = mode.Key(Press(0x40), Ready());
	CHECK(!mode.On());
	CHECK(f6.releaseAll && !f6.hideKey);  // and CET's overlay opens
	CHECK_STR(mode.Why(), "Shift+F6 left it");
	const Decision release = mode.Key(Release(0x2A), Ready());  // its press went to Minecraft, which let go of everything
	CHECK(release.hideKey && release.forwarded == 0);
}
```

- [x] **Step 2: Edit `tests/TestRouter.cpp`: F instead of W**

```cpp
	auto w = Key(0x11, 'W');
	Inspect(building.router, w);
	CHECK_EQ(KeyOf(w).VKey, USHORT('W'));  // V still walks
```
```cpp
	auto f = Key(0x21, 'F');
	Inspect(building.router, f);
	CHECK_EQ(KeyOf(f).VKey, USHORT('F'));  // V still interacts
```

- [x] **Step 3: Build and run the tests; the new ones must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | grep -E "^FAIL|test\(s\)"
```
Expected: `FAIL BuildModeGivesMinecraftTheMovementKeys`, `FAIL BuildModeHidesTheGamesCrouchKeys` and `FAIL BuildModeLeavesOnShiftF6WithLeftShiftForwarded` among the failures: the tests compile, since the keys go through the same calls as the buttons.

- [x] **Step 4: Edit `src/input/BuildMode.h`**

```cpp
// Build mode (the spec's "Input"): what becomes of each keyboard and mouse event the game reads. In build mode the mouse
// buttons, side buttons included, the wheel and the keys 1 to 9 go to Minecraft and are hidden from the game; F8
// toggles it; and the keys that open the game's menus leave it, and reach the game. A press's repeats and its release
// go where the press went, so neither side is left holding an input that only the other saw let go. Pure, and not
// thread-safe: the router (input/Router.h) holds its lock around every call.
```
```cpp
// Build mode (the minecraft-link spec's "Input"): what becomes of each keyboard and mouse event the game reads. In build
// mode the mouse buttons, side buttons included, the wheel and the keys 1 to 9 go to Minecraft and are hidden from the
// game; F8 toggles it; and the keys that open the game's menus leave it, and reach the game. Since spike 5 (its spec's
// "Keys"), Minecraft's movement keys go to Minecraft too, and the game's crouch keys are hidden without going anywhere.
// A press's repeats and its release go where the press went, so neither side is left holding an input that only the
// other saw let go. Pure, and not thread-safe: the router (input/Router.h) holds its lock around every call.
```
```cpp
	static constexpr size_t kInputs = 14;  // the five mouse buttons, then the keys 1 to 9
```
```cpp
	static constexpr size_t kInputs = 23;  // the five mouse buttons, the keys 1 to 9, then nine other keys it takes
```

- [x] **Step 5: Edit `src/input/BuildMode.cpp`**

```cpp
constexpr size_t kSideButtons = 3;                    // the first side button's index
```
```cpp
constexpr size_t kSideButtons = 3;                    // the first side button's index

// The other keys build mode takes (spike 5's "Keys"): Minecraft's movement keys, forwarded, and the game's crouch keys,
// only hidden, so V can't crouch under the puppet. C is Minecraft's toolbar-saving key, so it isn't forwarded either.
// The forwarded keys are physical keys: Minecraft applies its own bindings to them.
struct TakenKey
{
	uint16_t    scanCode;  // set 1, without its E0 prefix
	bool        extended;
	bool        forward;  // to Minecraft, as well as hidden from the game
	const char* name;
};

constexpr TakenKey kTakenKeys[] = {
	{ 0x11, false, true, "W" },          { 0x1E, false, true, "A" },        { 0x1F, false, true, "S" },
	{ 0x20, false, true, "D" },          { 0x39, false, true, "Space" },    { 0x2A, false, true, "Left Shift" },
	{ 0x1D, false, true, "Left Ctrl" },  { 0x2E, false, false, "C" },       { 0x1D, true, false, "Right Ctrl" },
};
constexpr size_t kTakenFirst = kButtonCount + 9;  // their holders come after the buttons' and 1 to 9's
```
```cpp
			}
		}
	}
	return decision;
```
```cpp
			}
		}
	}
	for (size_t i = 0; i < std::size(kTakenKeys); ++i)
	{
		const TakenKey& taken = kTakenKeys[i];
		if (aKey.scanCode != taken.scanCode || aKey.extended != taken.extended)
		{
			continue;
		}
		const Holder to = Route(kTakenFirst + i, !aKey.up);
		if (to == Holder::Minecraft || to == Holder::Neither)
		{
			decision.hideKey = true;
			++hidden_;
		}
		if (to == Holder::Minecraft && taken.forward)
		{
			Forward(decision, { proto::kInKey, SdlScancode(aKey.scanCode, aKey.extended), aKey.up ? 0 : 1, 0, 0 },
			        std::format("{} {}", taken.name, aKey.up ? "up" : "down"));
		}
		break;
	}
	return decision;
```
```cpp
	static_assert(kInputs == kButtonCount + 9);
```
```cpp
	static_assert(kInputs == kTakenFirst + std::size(kTakenKeys));
```

- [x] **Step 6: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `274 test(s), 0 failure(s)`.

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: build mode gives Minecraft the movement keys, and hides the game's crouch keys" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 10: The router sums the mouse's movement, and keeps build mode off while V can't be placed

Two more jobs for the router, for the plugin:
- **The mouse's movement along x,** summed from every raw mouse event the patch sees, in build mode or not, and taken
  once a frame. An absolute position, a tablet's, isn't movement. `set turn 1` turns it into degrees (Task 11).
- **What keeps build mode off:** the plugin passes, once a frame, why V can't be placed: in a vehicle, in a workspot,
  dead, or the functions that place V missing (Task 14). Minecraft's player dead, from McState's flags, counts too,
  since the spec's "Leaving" ends build mode then. Build mode won't turn on while there's a reason, and ends when one
  appears, saying why, and Minecraft lets go of everything.

**Files:**
- Modify: `src/input/BuildMode.h`, `src/input/BuildMode.cpp`, `src/input/Router.h`, `src/input/Router.cpp`,
  `tests/TestRouter.cpp`

- [x] **Step 1: Edit `tests/TestRouter.cpp`: the failing tests**

```cpp
	CHECK(Logged(building.log, "build mode off: Esc left it"));
}
```
```cpp
	CHECK(Logged(building.log, "build mode off: Esc left it"));
}

TEST(RouterSumsTheMouseMovementAlongX)
{
	// For the turn a teleport may have to add back (spike 5's "The teleport"), in or out of build mode.
	LogQueue log;
	Router   router(log, &FakeFront);
	auto     move = Mouse(0, 0, 7, 2);
	auto     click = Mouse(RI_MOUSE_LEFT_BUTTON_DOWN, 0, -3, 0);
	Inspect(router, move);
	Inspect(router, click);
	CHECK_EQ(router.TakeMouseX(), 4ll);
	CHECK_EQ(router.TakeMouseX(), 0ll);  // taken
	auto     absolute = Mouse(0, 0, 30000, 30000);  // a tablet's position, not a movement
	RAWMOUSE mouse = MouseOf(absolute);
	mouse.usFlags = MOUSE_MOVE_ABSOLUTE;
	std::memcpy(absolute.data() + sizeof(RAWINPUTHEADER), &mouse, sizeof mouse);
	Inspect(router, absolute);
	CHECK_EQ(router.TakeMouseX(), 0ll);
}

TEST(RouterKeepsBuildModeOffWhileVCantBePlaced)
{
	LogQueue log;
	Router   router(log, &FakeFront);
	router.Update(true, Linked(), "V is in a vehicle");
	CHECK_STR(router.TurnOn().value_or("on"), "V is in a vehicle");
	CHECK(Logged(log, "build mode stays off: V is in a vehicle"));
	router.Update(true, Linked(), "");
	CHECK(!router.TurnOn());
	CHECK(router.On());
}

TEST(RouterEndsBuildModeWhenVCantBePlaced)
{
	Building building;
	building.router.Update(true, Linked(), "V is in a workspot");
	CHECK(!building.router.On());
	CHECK_STR(building.router.Report().why, "V is in a workspot");
	const auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].type == proto::kInReleaseAll);
}

TEST(RouterEndsBuildModeWhenMinecraftsPlayerDies)
{
	Building           building;
	link::Link::Status state = Linked();
	state.minecraftFlags = proto::kMcInWorld | proto::kMcDead;
	building.router.Update(true, state);
	CHECK(!building.router.On());
	CHECK_STR(building.router.Report().why, "Minecraft's player is dead");
	const auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].type == proto::kInReleaseAll);
	CHECK_STR(building.router.TurnOn().value_or("on"), "Minecraft's player is dead");  // until it respawns
	state.minecraftFlags = proto::kMcInWorld;
	building.router.Update(true, state);
	CHECK(!building.router.TurnOn());
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'TakeMouseX': is not a member of 'cpmc::input::Router'`.

- [x] **Step 3: Edit `src/input/BuildMode.h`**

```cpp
	bool inFront = false;    // the foreground window is the game's
	bool linked = false;     // the user's Link
	bool inGame = false;     // linked, with V found
	bool minecraft = false;  // Minecraft's heartbeat is fresh
	bool minecraftInWorld = false;
```
```cpp
	bool        inFront = false;    // the foreground window is the game's
	bool        linked = false;     // the user's Link
	bool        inGame = false;     // linked, with V found
	bool        minecraft = false;  // Minecraft's heartbeat is fresh
	bool        minecraftInWorld = false;
	std::string blocked;  // why Minecraft can't drive V: V in a vehicle, a workspot or dead, or Minecraft's player dead (spike 5)
```

- [x] **Step 4: Edit `src/input/BuildMode.cpp`**

```cpp
		why_ = aSituation.linked ? "V isn't found" : "it isn't linked";
	}
	else
```
```cpp
		why_ = aSituation.linked ? "V isn't found" : "it isn't linked";
	}
	else if (!aSituation.blocked.empty())
	{
		why_ = aSituation.blocked;
	}
	else
```

- [x] **Step 5: Edit `src/input/Router.h`**

```cpp
	// Once a frame, after the link's update: whether the user linked, and what the link says. Build mode ends if
	// Minecraft or V is gone, or the link.
	void Update(bool aLinked, const link::Link::Status& aState);
```
```cpp
	// Once a frame, after the link's update: whether the user linked, what the link says, and why V can't be placed, if
	// it can't (spike 5). Build mode ends if Minecraft or V is gone, or the link, or V can't be placed, or Minecraft's
	// player dies.
	void Update(bool aLinked, const link::Link::Status& aState, const std::string& aVBlocked = {});
	// The mouse's movement along x since the last call, in raw counts: for the turn a teleport adds back (spike 5).
	int64_t TakeMouseX();
```
```cpp
	std::atomic<uint64_t> read_{ 0 };
```
```cpp
	std::atomic<uint64_t> read_{ 0 };
	std::atomic<int64_t>  mouseX_{ 0 };
```

- [x] **Step 6: Edit `src/input/Router.cpp`**

```cpp
		std::memcpy(&mouse, body, sizeof mouse);
```
```cpp
		std::memcpy(&mouse, body, sizeof mouse);
		if (!(mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
		{
			mouseX_ += mouse.lLastX;
		}
```
```cpp
void Router::Update(bool aLinked, const link::Link::Status& aState)
```
```cpp
void Router::Update(bool aLinked, const link::Link::Status& aState, const std::string& aVBlocked)
```
```cpp
	situation_.minecraftInWorld = aState.inWorld;
	if (mode_.On())
	{
		const char* why = !aState.minecraft  ? "Minecraft has gone"
		                  : !aState.inWorld  ? "Minecraft left its world"
		                  : !aLinked         ? "unlinked"
		                  : !aState.inGame   ? "V isn't found"
		                                     : nullptr;
		if (why)
		{
			Off(why);
		}
	}
```
```cpp
	situation_.minecraftInWorld = aState.inWorld;
	situation_.blocked = (aState.minecraftFlags & proto::kMcDead) ? "Minecraft's player is dead" : aVBlocked;
	if (mode_.On())
	{
		std::string why = !aState.minecraft ? "Minecraft has gone"
		                  : !aState.inWorld ? "Minecraft left its world"
		                  : !aLinked        ? "unlinked"
		                  : !aState.inGame  ? "V isn't found"
		                                    : situation_.blocked;
		if (!why.empty())
		{
			Off(std::move(why));
		}
	}
}

int64_t Router::TakeMouseX()
{
	return mouseX_.exchange(0);
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `278 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the router sums the mouse's movement, and keeps build mode off while V can't be placed or Minecraft's player is dead" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 11: The turn ratio, and the drive log

Two pure pieces for the plugin:
- **`TurnRatio`** (the spec's "The teleport"): if a teleport every frame with V's own yaw loses the frame's turn from
  the mouse, `set turn 1` adds it back, as FreeFly adds its own turn to the yaw it teleports with. The mouse's counts
  become degrees by a ratio measured outside build mode while V turns:
  - V's turn over a turn of the mouse, against the counts. A turn ends after 5 frames without movement, or after
    120, so counts the game takes a frame late still count with their turn;
  - each turn of at least 100 counts and a degree gives a ratio, and the median of the last 21 is used once 5 have
    counted, so a menu's cursor, or a turn the game made by itself, doesn't move it;
  - the game's yaw jumps from 180 to −180 degrees, and the change is taken across it.
- **`DriveLog`** (`drivelog <seconds>`): build mode's drive, a row a frame for 1 to 60 seconds, as CSV, so judder can
  be measured (see "Found while planning"). Each row has the frame time, the drive's state, V's camera, where the frame
  put it and its distance from where the last frame put it, the render delay, whether a tick was late, the teleport's
  time, the mouse's counts, the yaw and the turn. The plugin writes it to `logs\<time>\drive.csv`, beside the frame
  log's `frame.json`.

**Files:**
- Create: `src/link/Turn.h`, `src/link/Turn.cpp`, `src/log/DriveLog.h`, `src/log/DriveLog.cpp`, `tests/TestTurn.cpp`,
  `tests/TestDriveLog.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestTurn.cpp`**

```cpp
#include "Harness.h"

#include "link/Turn.h"

#include <cmath>

using namespace cpmc::link;

namespace
{
// One turn: aCounts every aEvery frames for aFrames frames, and V's yaw following by aDegreesACount a frame late, as the
// game may take them, then the mouse still until the turn ends. aYaw carries V's yaw from turn to turn; the game's stays
// between -180 and 180 degrees.
void Turn(TurnRatio& aRatio, double& aYaw, int64_t aCounts, int aFrames, double aDegreesACount, int aEvery = 1)
{
	int64_t late = 0;
	for (int i = 0; i < aFrames + TurnRatio::kStill; ++i)
	{
		const int64_t counts = i < aFrames && i % aEvery == 0 ? aCounts : 0;
		aYaw += double(late) * aDegreesACount;
		aRatio.Note(counts, std::remainder(aYaw, 360.0));
		late = counts;
	}
}
}

TEST(TurnRatioIsKnownAfterFiveTurns)
{
	TurnRatio ratio;
	double    yaw = 10.0;
	ratio.Note(0, yaw);
	for (int turn = 0; turn < 4; ++turn)
	{
		Turn(ratio, yaw, 30, 10, -0.02);  // 300 counts: 6 degrees
	}
	CHECK(!ratio.Ratio().has_value());
	CHECK_EQ(ratio.Turns(), uint64_t(4));
	CHECK_NEAR(ratio.Turn(100), 0.0, 1e-12);  // nothing is added until it's known
	Turn(ratio, yaw, 30, 10, -0.02);
	CHECK(ratio.Ratio().has_value());
	CHECK_NEAR(ratio.Ratio().value_or(0.0), -0.02, 1e-12);
	CHECK_NEAR(ratio.Turn(100), -2.0, 1e-12);
}

TEST(TurnRatioSkipsMenusAndSmallTurns)
{
	TurnRatio ratio;
	double    yaw = 0.0;
	ratio.Note(0, yaw);
	for (int turn = 0; turn < 10; ++turn)
	{
		Turn(ratio, yaw, 50, 10, 0.0);  // a menu: the mouse moves its cursor, and V doesn't turn
	}
	Turn(ratio, yaw, 5, 10, -0.05);  // 50 counts: too few, though V turned
	CHECK_EQ(ratio.Turns(), uint64_t(0));
	for (int turn = 0; turn < 5; ++turn)
	{
		Turn(ratio, yaw, 30, 10, -0.02);
	}
	CHECK_NEAR(ratio.Ratio().value_or(0.0), -0.02, 1e-12);
}

TEST(TurnRatioTakesTheMedianTurn)
{
	// A turn the game added to, a takedown's say, doesn't move it.
	TurnRatio ratio;
	double    yaw = 0.0;
	ratio.Note(0, yaw);
	Turn(ratio, yaw, 30, 10, -0.02);
	Turn(ratio, yaw, 30, 10, -0.5);
	Turn(ratio, yaw, 30, 10, -0.021);
	Turn(ratio, yaw, 30, 10, -0.02);
	Turn(ratio, yaw, 30, 10, -0.019);
	CHECK_NEAR(ratio.Ratio().value_or(0.0), -0.02, 1e-12);
}

TEST(TurnRatioWrapsTheYaw)
{
	// Turning back and forth across 180 degrees, where the game's yaw jumps to -180: every turn crosses it.
	TurnRatio ratio;
	double    yaw = 177.0;
	ratio.Note(0, yaw);
	for (int turn = 0; turn < 5; ++turn)
	{
		Turn(ratio, yaw, turn % 2 == 0 ? 30 : -30, 10, 0.02);
	}
	CHECK_NEAR(ratio.Ratio().value_or(0.0), 0.02, 1e-12);
}

TEST(TurnRatioKeepsASlowTurnTogether)
{
	// The mouse moves every third frame: the frames between don't end the turn.
	TurnRatio ratio;
	double    yaw = 0.0;
	ratio.Note(0, yaw);
	for (int turn = 0; turn < 5; ++turn)
	{
		Turn(ratio, yaw, 40, 30, -0.02, 3);
	}
	CHECK_NEAR(ratio.Ratio().value_or(0.0), -0.02, 1e-12);
}

TEST(TurnRatioEndsALongTurnAnyway)
{
	// Turning without a pause for five turns' worth of frames: each is cut at kLongest, and only the first misses a
	// frame of its turn, to the game taking the counts late.
	TurnRatio ratio;
	double    yaw = 0.0;
	ratio.Note(0, yaw);
	Turn(ratio, yaw, 10, TurnRatio::kLongest * 5, -0.02);
	CHECK_EQ(ratio.Turns(), uint64_t(5));
	CHECK_NEAR(ratio.Ratio().value_or(0.0), -0.02, 1e-12);
}

TEST(TurnRatioDropsATurnBrokenOff)
{
	// V gets into a car mid-turn, and the car turns V: that turn is dropped, and the yaw starts afresh after.
	TurnRatio ratio;
	ratio.Note(0, 0.0);
	ratio.Note(300, 0.0);
	ratio.Note(0, 90.0);
	ratio.Break();
	for (int i = 0; i < TurnRatio::kStill + 1; ++i)
	{
		ratio.Note(i == 0 ? 300 : 0, -45.0);
	}
	CHECK_EQ(ratio.Turns(), uint64_t(0));
}
```

- [x] **Step 2: Write the failing test, `tests/TestDriveLog.cpp`**

```cpp
#include "Harness.h"

#include "log/DriveLog.h"

using namespace cpmc;

namespace
{
DriveRow Row(double aTime, const char* aState)
{
	DriveRow row;
	row.time = aTime;
	row.frameMs = 16.667;
	row.state = aState;
	row.camera = Vec3d{ -119.5, -2007.5, 7.74 };
	row.target = Vec3d{ -119.5, -2007.5, 7.76 };
	row.lastDistance = 0.02;
	row.delayMs = 17.716;
	row.teleportMs = 0.0123;
	row.mouseX = -12;
	row.yaw = 90.0;
	row.turn = 0.24;
	return row;
}
}

TEST(DriveLogWritesAHeaderAndARowAFrame)
{
	DriveLog log;
	CHECK(!log.Recording());
	CHECK(log.Start(1.0));
	CHECK(log.Recording());
	log.Add(Row(100.0, "driving"));
	DriveRow waiting = Row(100.016667, "waiting");
	waiting.camera.reset();
	waiting.target.reset();
	waiting.lastDistance.reset();
	waiting.yaw.reset();
	waiting.waited = true;
	log.Add(waiting);
	CHECK(!log.TakeFinished().has_value());  // still recording
	log.Add(Row(101.0, "driving"));          // a second from the first row: it's done, without this one
	const std::optional<DriveLog::Finished> finished = log.TakeFinished();
	CHECK(finished.has_value());
	CHECK(!log.Recording());
	CHECK(!log.TakeFinished().has_value());  // once
	if (!finished)
	{
		return;
	}
	CHECK_EQ(finished->rows, size_t(2));
	CHECK_STR(finished->csv,
	          "t,frame_ms,state,camera_x,camera_y,camera_z,target_x,target_y,target_z,last_distance,delay_ms,waited,"
	          "teleport_ms,mouse_x,yaw,turn\n"
	          "0.0000,16.667,driving,-119.5000,-2007.5000,7.7400,-119.5000,-2007.5000,7.7600,0.0200,17.716,0,0.0123,-12,"
	          "90.000,0.240\n"
	          "0.0167,16.667,waiting,,,,,,,,17.716,1,0.0123,-12,,0.240\n");
}

TEST(DriveLogStopsWhenFull)
{
	DriveLog log;
	CHECK(log.Start(60.0));
	CHECK(!log.Start(5.0));  // one at a time
	for (size_t i = 0; i < DriveLog::kMostRows; ++i)
	{
		log.Add(Row(double(i) * 0.001, "off"));
	}
	const std::optional<DriveLog::Finished> finished = log.TakeFinished();
	CHECK(finished.has_value());
	CHECK_EQ(finished ? finished->rows : 0, DriveLog::kMostRows);
	CHECK(!log.Recording());
	CHECK(log.Start(5.0));  // and another can start
}
```

- [x] **Step 3: Edit `CMakeLists.txt` to add the tests**

```cmake
	tests/TestDrive.cpp
	tools/mclink/McLinkScript.cpp
```
```cmake
	tests/TestDrive.cpp
	tests/TestTurn.cpp
	tests/TestDriveLog.cpp
	tools/mclink/McLinkScript.cpp
```

- [x] **Step 4: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Turn.h'`, and the same for `log/DriveLog.h`.

- [x] **Step 5: Write `src/link/Turn.h`**

```cpp
#pragma once

#include <cstdint>
#include <deque>
#include <optional>

// The turn `set turn 1` adds to each teleport's yaw in build mode (spike 5's "Each frame while Minecraft drives"): if a
// teleport with V's own yaw loses the frame's turn from the mouse, this adds it back. The mouse's raw counts become
// degrees by a ratio measured outside build mode while V turns: V's turn over a turn of the mouse, against the counts the
// input patch saw. A turn ends once the mouse has been still for a few frames, since the game may take a frame's counts
// in the next one. Each turn gives a ratio, and the median of the last few is used, so a menu's cursor or a turn the
// game made by itself doesn't move it. Pure.
namespace cpmc::link
{
class TurnRatio
{
public:
	static constexpr int    kStill = 5;        // frames without movement that end a turn
	static constexpr int    kLongest = 120;    // frames that end a turn anyway
	static constexpr double kFewest = 100.0;   // counts a turn needs to count
	static constexpr double kSmallest = 1.0;   // degrees V must turn in it
	static constexpr size_t kKnown = 5;        // turns before the ratio is used
	static constexpr size_t kKept = 21;        // the last turns the median is taken from

	// Once a frame outside build mode while V can be placed: the mouse's counts along x since the last frame, and V's
	// yaw now, in degrees.
	void Note(int64_t aCounts, double aYaw);
	// Any other frame: V's yaw may change by itself (a vehicle, a scene, a load, build mode's teleports), so the turn
	// under way is dropped, and the yaw starts afresh.
	void Break();

	std::optional<double> Ratio() const;                 // degrees a count, once kKnown turns have counted
	double                Turn(int64_t aCounts) const;   // aCounts in degrees; 0 while the ratio isn't known
	uint64_t              Turns() const { return turns_; }  // turns that counted, since the start

private:
	void EndTurn();

	std::optional<double> lastYaw_;
	double                counts_ = 0.0;  // the turn under way
	double                degrees_ = 0.0;
	int                   frames_ = 0;
	int                   still_ = 0;
	std::deque<double>    ratios_;  // the last kKept turns'
	uint64_t              turns_ = 0;
};
}
```

- [x] **Step 6: Write `src/link/Turn.cpp`**

```cpp
#include "link/Turn.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace cpmc::link
{
void TurnRatio::Note(int64_t aCounts, double aYaw)
{
	if (!std::isfinite(aYaw))
	{
		return Break();
	}
	if (lastYaw_)
	{
		counts_ += double(aCounts);
		degrees_ += std::remainder(aYaw - *lastYaw_, 360.0);  // the game's yaw jumps from 180 to -180
		++frames_;
		still_ = aCounts == 0 ? still_ + 1 : 0;
		if (still_ >= kStill || frames_ >= kLongest)
		{
			EndTurn();
		}
	}
	lastYaw_ = aYaw;
}

void TurnRatio::Break()
{
	lastYaw_.reset();
	counts_ = 0.0;
	degrees_ = 0.0;
	frames_ = 0;
	still_ = 0;
}

void TurnRatio::EndTurn()
{
	if (std::abs(counts_) >= kFewest && std::abs(degrees_) >= kSmallest)
	{
		ratios_.push_back(degrees_ / counts_);
		if (ratios_.size() > kKept)
		{
			ratios_.pop_front();
		}
		++turns_;
	}
	counts_ = 0.0;
	degrees_ = 0.0;
	frames_ = 0;
	still_ = 0;
}

std::optional<double> TurnRatio::Ratio() const
{
	if (ratios_.size() < kKnown)
	{
		return std::nullopt;
	}
	std::vector<double> sorted(ratios_.begin(), ratios_.end());
	const auto          middle = sorted.begin() + std::ptrdiff_t(sorted.size() / 2);
	std::nth_element(sorted.begin(), middle, sorted.end());
	return *middle;
}

double TurnRatio::Turn(int64_t aCounts) const
{
	return Ratio().value_or(0.0) * double(aCounts);
}
}
```

- [x] **Step 7: Write `src/log/DriveLog.h`**

```cpp
#pragma once

#include "Coords.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

// `drivelog <seconds>` (spike 5's step 2): build mode's drive, a row a frame for a few seconds, as CSV, so judder can be
// measured: where V's camera was, where the frame put it, the interpolator's delay and late ticks, and the teleport's
// time and turn. The frame log (`log`) records two frames of the renderer's work; this one records the drive. Pure: the
// plugin adds a row a frame, and writes the text once it's finished.
namespace cpmc
{
struct DriveRow
{
	double                time = 0.0;      // seconds, on any steady clock
	double                frameMs = 0.0;   // since the last frame
	const char*           state = "off";   // the drive's
	std::optional<Vec3d>  camera;          // V's camera this frame (world axes)
	std::optional<Vec3d>  target;          // where this frame puts V's camera (world axes)
	std::optional<double> lastDistance;    // V's camera from where the last frame put it, metres
	double                delayMs = 0.0;   // the interpolator's render delay
	bool                  waited = false;  // the frame used up the newest tick
	double                teleportMs = 0.0;
	int64_t               mouseX = 0;      // the mouse's counts along x this frame
	std::optional<double> yaw;             // the teleport's, degrees, turn included
	double                turn = 0.0;      // the degrees `set turn 1` added
};

class DriveLog
{
public:
	static constexpr size_t kMostRows = 30000;  // a minute at 500 frames a second

	struct Finished
	{
		std::string csv;
		size_t      rows = 0;
	};

	// Records aSeconds from the next row. False if one is already recording.
	bool Start(double aSeconds);
	bool Recording() const { return seconds_ > 0.0; }
	// Once a frame while recording. A row aSeconds or more after the first finishes it, and is left out.
	void Add(const DriveRow& aRow);
	// The recording, once, when it has finished: its time is up, or it's full.
	std::optional<Finished> TakeFinished();

private:
	void Finish();

	double                  seconds_ = 0.0;  // 0: not recording
	std::optional<double>   start_;
	std::string             csv_;
	size_t                  rows_ = 0;
	std::optional<Finished> finished_;
};
}
```

- [x] **Step 8: Write `src/log/DriveLog.cpp`**

```cpp
#include "log/DriveLog.h"

#include <cmath>
#include <format>
#include <iterator>
#include <utility>

namespace cpmc
{
namespace
{
constexpr const char* kHeader = "t,frame_ms,state,camera_x,camera_y,camera_z,target_x,target_y,target_z,last_distance,"
                                "delay_ms,waited,teleport_ms,mouse_x,yaw,turn\n";

void AppendPoint(std::string& aOut, const std::optional<Vec3d>& aPoint)
{
	if (aPoint)
	{
		std::format_to(std::back_inserter(aOut), ",{:.4f},{:.4f},{:.4f}", aPoint->x, aPoint->y, aPoint->z);
	}
	else
	{
		aOut += ",,,";
	}
}
}

bool DriveLog::Start(double aSeconds)
{
	if (Recording() || !(aSeconds > 0.0))
	{
		return false;
	}
	seconds_ = aSeconds;
	start_.reset();
	csv_ = kHeader;
	rows_ = 0;
	return true;
}

void DriveLog::Add(const DriveRow& aRow)
{
	if (!Recording())
	{
		return;
	}
	if (!start_)
	{
		start_ = aRow.time;
	}
	const double t = aRow.time - *start_;
	if (t >= seconds_)
	{
		return Finish();
	}
	std::format_to(std::back_inserter(csv_), "{:.4f},{:.3f},{}", t, aRow.frameMs, aRow.state);
	AppendPoint(csv_, aRow.camera);
	AppendPoint(csv_, aRow.target);
	csv_ += aRow.lastDistance ? std::format(",{:.4f}", *aRow.lastDistance) : ",";
	std::format_to(std::back_inserter(csv_), ",{:.3f},{},{:.4f},{}", aRow.delayMs, aRow.waited ? 1 : 0, aRow.teleportMs, aRow.mouseX);
	csv_ += aRow.yaw ? std::format(",{:.3f}", *aRow.yaw) : ",";
	std::format_to(std::back_inserter(csv_), ",{:.3f}\n", aRow.turn);
	if (++rows_ >= kMostRows)
	{
		Finish();
	}
}

std::optional<DriveLog::Finished> DriveLog::TakeFinished()
{
	return std::exchange(finished_, std::nullopt);
}

void DriveLog::Finish()
{
	finished_ = Finished{ std::move(csv_), rows_ };
	csv_.clear();
	seconds_ = 0.0;
}
}
```

- [x] **Step 9: Edit `CMakeLists.txt` to add the sources**

```cmake
	src/link/Drive.cpp
	src/render/PendingSections.cpp
```
```cmake
	src/link/Drive.cpp
	src/link/Turn.cpp
	src/log/DriveLog.cpp
	src/render/PendingSections.cpp
```

- [x] **Step 10: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `287 test(s), 0 failure(s)`.

- [x] **Step 11: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the turn ratio, and the drive log" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 12: The `drivelog` request

`drivelog <seconds>`, from 1 to 60, records the drive (the spec's "Requests"). `set turn 0|1` needs nothing new
here: `set` already takes a name and a number, and the plugin handles `turn` in Task 15.

**Files:**
- Modify: `src/Requests.h`, `src/Requests.cpp`, `tests/TestRequests.cpp`

- [x] **Step 1: Edit `tests/TestRequests.cpp`: the failing test**

```cpp
TEST(RequestsTypeIntoMinecraft)
```
```cpp
TEST(RequestsForTheDriveLog)
{
	std::string error;
	const auto  ten = cpmc::ParseRequest("drivelog 10\n", error);
	CHECK(ten.has_value());
	CHECK(ten.value_or(cpmc::Request{}).kind == cpmc::Request::Kind::DriveLog);
	CHECK_EQ(ten.value_or(cpmc::Request{}).number, 10.0);
	CHECK_EQ(cpmc::ParseRequest("drivelog 2.5", error).value_or(cpmc::Request{}).number, 2.5);
	CHECK(cpmc::ParseRequest("drivelog 1", error).has_value());
	CHECK(cpmc::ParseRequest("drivelog 60", error).has_value());
	CHECK(!cpmc::ParseRequest("drivelog 0.5", error));
	CHECK(!cpmc::ParseRequest("drivelog 61", error));
	CHECK(!cpmc::ParseRequest("drivelog 10 now", error));
	CHECK(!cpmc::ParseRequest("drivelog", error));
	CHECK_STR(error, "drivelog takes a number of seconds from 1 to 60");
}

TEST(RequestsTypeIntoMinecraft)
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2065: 'DriveLog': undeclared identifier`.

- [x] **Step 3: Edit `src/Requests.h`**

```cpp
//                               '/' and runs verbatim to the end of the line)
```
```cpp
//                               '/' and runs verbatim to the end of the line)
//   drivelog <seconds>         (spike 5: build mode's drive, a row a frame, for 1 to 60 seconds)
```
```cpp
		Floor,
		Mc,
		Build,
	};

	Kind        kind = Kind::Log;
```
```cpp
		Floor,
		Mc,
		Build,
		DriveLog,
	};

	Kind        kind = Kind::Log;
```
```cpp
	double      number = 0.0;  // the field's block count, or the setting's value
```
```cpp
	double      number = 0.0;  // the field's block count, the setting's value, or the drive log's seconds
```

- [x] **Step 4: Edit `src/Requests.cpp`**

```cpp
	}
	if (command == "set")
```
```cpp
	}
	if (command == "drivelog")
	{
		const std::optional<double> seconds = arguments == 1 ? Number(words[1]) : std::nullopt;
		if (!seconds || *seconds < 1.0 || *seconds > 60.0)
		{
			return Fail(aError, "drivelog takes a number of seconds from 1 to 60");
		}
		return Request{ Request::Kind::DriveLog, {}, *seconds };
	}
	if (command == "set")
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `288 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the drivelog request" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 13: The status's drive line

The spec's "Status", as a `drive=` line between `build=` and `positions=`:
- off, or waiting or driving and for how long;
- V's eye height, and while driving, V's camera's distance from where the last frame put Minecraft's eye: the last,
  and the second's average and worst;
- the render delay, and the frames in the last second that used up the newest tick;
- bad targets skipped since the start, and the teleport call's time;
- V's health, so fall damage shows;
- the turn setting, and its ratio once measured;
- how Minecraft's player moves: on the ground or in the air, sneaking, sprinting, swimming, flying, dead.

Why build mode is off stays in the `build=` line just above it.

**Files:**
- Modify: `src/Status.h`, `src/Status.cpp`, `tests/TestStatus.cpp`

- [x] **Step 1: Edit `tests/TestStatus.cpp`: the failing test**

```cpp
#include "Status.h"
```
```cpp
#include "Status.h"

#include <skycraft_protocol.h>
```
```cpp
	CHECK(status.find("link=") < status.find("build=") && status.find("build=") < status.find("positions="));
}
```
```cpp
	CHECK(status.find("link=") < status.find("build=") && status.find("build=") < status.find("positions="));
}

TEST(StatusWritesTheDrive)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	link.drive = "driving";
	link.driveFor = 12.34;
	link.eyeHeight = 1.6;
	link.eyeDistanceLast = 0.012;
	link.eyeDistanceAverage = 0.0104;
	link.eyeDistanceWorst = 0.034;
	link.renderDelayMs = 17.716;
	link.waitedFrames = 2;
	link.skippedFrames = 3;
	link.teleportMsAverage = 0.045;
	link.teleportMsWorst = 0.12;
	link.health = 100.0;
	link.turn = true;
	link.turnRatio = -0.02134;
	link.turnTurns = 12;
	link.minecraftFlags = skycraft::proto::kMcInWorld | skycraft::proto::kMcOnGround | skycraft::proto::kMcSneaking;
	report.link = link;
	std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "drive=driving for 12.3 s; V's eye 1.60 m above its feet; V's camera from Minecraft's eye 0.012 m, "
	                  "0.010 m average and 0.034 m worst in the last second; render delay 17.7 ms, 2 frames waited for a late "
	                  "tick in the last second; 3 bad targets skipped since the start; teleport 0.045 ms average, 0.120 ms "
	                  "worst; V's health 100%; turn on, -0.0213 degrees a count from 12 turns; Minecraft's player on the "
	                  "ground, sneaking"));
	CHECK(status.find("build=") < status.find("drive=") && status.find("drive=") < status.find("positions="));
	// Before anything has been measured
	report.link = cpmc::LinkReport{};
	status = cpmc::FormatStatus(report);
	CHECK(Has(status, "drive=off; V's eye 1.60 m above its feet; render delay 0.0 ms, 0 frames waited for a late tick in the "
	                  "last second; 0 bad targets skipped since the start; teleport not called; V's health unknown; turn off, "
	                  "ratio not measured yet (0 of 5 turns); Minecraft's player in the air"));
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'drive': is not a member of 'cpmc::LinkReport'`.

- [x] **Step 3: Edit `src/Status.h`**

```cpp
	std::string        lastInput;            // the last forwarded
```
```cpp
	std::string        lastInput;            // the last forwarded
	// Build mode's drive (spike 5's "Status")
	std::string           drive = "off";             // off, waiting or driving
	double                driveFor = 0.0;            // seconds in that state
	double                eyeHeight = 1.60;          // V's, metres, measured outside build mode
	std::optional<double> eyeDistanceLast;           // V's camera from where the last frame put Minecraft's eye; none:
	double                eyeDistanceAverage = 0.0;  // not driving. Metres, in the last second
	double                eyeDistanceWorst = 0.0;
	double                renderDelayMs = 0.0;       // the tick interpolator's
	uint64_t              waitedFrames = 0;          // frames that used up the newest tick, in the last second
	uint64_t              skippedFrames = 0;         // bad targets, since the start
	double                teleportMsAverage = -1.0;  // Teleport's call, in the last second; below 0: not called
	double                teleportMsWorst = -1.0;
	std::optional<double> health;                    // V's, percent; none: not read
	bool                  turn = false;              // set turn 1: the mouse's turn is added to the teleport's yaw
	std::optional<double> turnRatio;                 // degrees a count, once measured
	uint64_t              turnTurns = 0;             // turns measured
	uint32_t              minecraftFlags = 0;        // McState's
```
```cpp
// positions, ring, atlas, hud, latency, burst and processes); then one structure line per structure:
```
```cpp
// drive, positions, ring, atlas, hud, latency, burst and processes); then one structure line per structure:
```

- [x] **Step 4: Edit `src/Status.cpp`**

```cpp
#include <format>
```
```cpp
#include "link/Turn.h"

#include <skycraft_protocol.h>

#include <format>
#include <utility>
```
```cpp
std::string FormatLink(const LinkReport& aLink)
```
```cpp
// How Minecraft's player moves, from McState's flags: "on the ground, sneaking", "in the air, flying".
std::string MinecraftMovement(uint32_t aFlags)
{
	namespace proto = skycraft::proto;
	std::string out = aFlags & proto::kMcOnGround ? "on the ground" : "in the air";
	for (const auto& [flag, name] : { std::pair{ proto::kMcSneaking, "sneaking" }, std::pair{ proto::kMcSprinting, "sprinting" },
	                                  std::pair{ proto::kMcSwimming, "swimming" }, std::pair{ proto::kMcFlying, "flying" },
	                                  std::pair{ proto::kMcDead, "dead" } })
	{
		if (aFlags & flag)
		{
			out += std::string(", ") + name;
		}
	}
	return out;
}

std::string FormatDrive(const LinkReport& aLink)
{
	std::string out = "drive=" + aLink.drive;
	if (aLink.drive != "off")
	{
		out += std::format(" for {:.1f} s", aLink.driveFor);
	}
	out += std::format("; V's eye {:.2f} m above its feet", aLink.eyeHeight);
	if (aLink.eyeDistanceLast)
	{
		out += std::format("; V's camera from Minecraft's eye {:.3f} m, {:.3f} m average and {:.3f} m worst in the last second",
		                   *aLink.eyeDistanceLast, aLink.eyeDistanceAverage, aLink.eyeDistanceWorst);
	}
	out += std::format("; render delay {:.1f} ms, {} frames waited for a late tick in the last second; {} bad targets skipped "
	                   "since the start; teleport {}; V's health {}; turn {}, {}; Minecraft's player {}\n",
	                   aLink.renderDelayMs, aLink.waitedFrames, aLink.skippedFrames,
	                   aLink.teleportMsAverage < 0.0 ? std::string("not called") : Ms(aLink.teleportMsAverage, aLink.teleportMsWorst),
	                   aLink.health ? std::format("{:.0f}%", *aLink.health) : std::string("unknown"), aLink.turn ? "on" : "off",
	                   aLink.turnRatio ? std::format("{:.4f} degrees a count from {} turns", *aLink.turnRatio, aLink.turnTurns)
	                                   : std::format("ratio not measured yet ({} of {} turns)", aLink.turnTurns, link::TurnRatio::kKnown),
	                   MinecraftMovement(aLink.minecraftFlags));
	return out;
}

std::string FormatLink(const LinkReport& aLink)
```
```cpp
	                                     : std::string("GetRawInputData not hooked (see the log)"));
```
```cpp
	                                     : std::string("GetRawInputData not hooked (see the log)"));
	out += FormatDrive(aLink);
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `289 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the status's drive line" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 14: `Puppet`, the game's script calls

The game's own script functions, found by name through RTTI as `PlayerLocator` finds V, and called with
`ExecuteFunction` (the spec's "Calling the game from C++"). Each was traced to a declaration or a call in the user's
installed mods:

| Call | For | Where it was traced |
|---|---|---|
| `PlayerSystem.GetLocalPlayerMainGameObject() -> ref<GameObject>` | V | `PlayerLocator`, and redscript mods that cast its result `as PlayerPuppet` |
| `TeleportationFacility.Teleport(ref<GameObject>, Vector4, EulerAngles)` | placing V | its declaration, quoted in ImmersiveFirstPerson's `init.lua`; FreeFly calls it every frame with `EulerAngles.new(0, 0, yaw)` |
| V's `GetWorldYaw() -> Float` | the teleport's yaw | FreeFly, beside that call |
| the global `GetMountedVehicle;GameObject` | in a vehicle | ImmersiveFirstPerson calls it by that name; FreeFly won't teleport while it returns one |
| `WorkspotGameSystem.IsActorInWorkspot(GameObject) -> Bool` | in a workspot: a seat, a scene, a vehicle | ImmersiveFirstPerson |
| V's `IsDead() -> Bool` | dead | the game's `GameObject`, which `ScriptedPuppet` overrides |
| V's `GetEntityID()`, then `StatPoolsSystem.GetStatPoolValue(StatsObjectID, gamedataStatPoolType, Bool) -> Float` | V's health, in percent | a redscript mod's `GetStatPoolValue(Cast<StatsObjectID>(…GetEntityID()), gamedataStatPoolType.Stamina, true)`; Health is 17 in the SDK's enum |

So nothing faults (the plan's "Before you start"):
- **Each function is found once, picked by its parameters' and return types,** as RTTI names them (`handle:gameObject`,
  `whandle:gameObject`, `Vector4` and so on). An overload with other types, or a signature a patch changed, is logged
  with what the game has instead, and never called. Where a `GameObject` may be a `ref` or a `wref`, V goes as the
  parameter takes it.
- **A system's instance comes from `GameInstance::GetSystem` and is checked first:** `ExecuteFunction`'s own lookup by
  class would call a member function on whatever `GetSystem` returned, null included. A static or global function gets
  a `void*` null instance, so that overload isn't the one taken.
- **No `ScriptGameInstance` is made:** the SDK's constructor aborts the game if its size isn't the game's. That's why
  the vehicle check isn't `VehicleComponent.IsMountedToVehicle(GameInstance, GameObject)`.
- **Every address the SDK resolves on these paths** is in the game's own `cyberpunk2077_addresses.json`:
  `rtti::Function::CallNative`, `rtti::Function::InternalCall` for the scripted functions, `CStack`'s vtable,
  `CGameEngine`, `CRTTISystem::Get` and `CNamePool::Get`. A hash RED4ext can't resolve would terminate the game.
- **The enum goes in 8 bytes,** whatever its size in the game, and every result is checked for being finite.

Only the game can run it: Task 17's first check is the log's `puppet: found …` line for each of the eight.

**Files:**
- Create: `src/Puppet.h`, `src/Puppet.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write `src/Puppet.h`**

```cpp
#pragma once

#include "Coords.h"
#include "LogQueue.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>

namespace RED4ext
{
struct CBaseFunction;
struct CClass;
}

// V moved and read through the game's own script functions, for build mode's drive (spike 5's "Part 1"), found by name
// as PlayerLocator finds V: TeleportationFacility.Teleport and V's GetWorldYaw place V; the global GetMountedVehicle,
// WorkspotGameSystem.IsActorInWorkspot and V's IsDead keep build mode off; and StatPoolsSystem.GetStatPoolValue reads V's
// health. Each function is found once and picked by its parameters' and return types, so an overload with other types,
// or a signature a patch changed, is logged and never called with the wrong arguments. Main thread only.
namespace cpmc
{
class Puppet
{
public:
	explicit Puppet(LogQueue& aLog) : log_(aLog) {}

	// Why V can't be placed: in a vehicle, in a workspot, dead, or the functions that place it missing. Empty: it can be,
	// or V isn't there, which the link reports.
	std::string           Blocked();
	std::optional<double> Yaw();     // V's, in degrees
	std::optional<double> Health();  // V's, in percent
	// V's feet to aFeet (world axes), facing aYaw degrees. False, with aError saying why, if V wasn't moved.
	bool Teleport(const Vec3d& aFeet, double aYaw, std::string& aError);

private:
	enum Function : size_t
	{
		kPlayer,
		kTeleport,
		kYaw,
		kVehicle,
		kWorkspot,
		kDead,
		kEntityId,
		kPool,
		kFunctions,
	};

	// The function, found and checked the first time it's asked for, and logged; null if the game hasn't it as these
	// calls need it. aClass: where to look (V's own class for V's functions; unused for a global function). Without it,
	// nothing is looked for, and it's asked again later.
	RED4ext::CBaseFunction* Find(Function aFunction, RED4ext::CClass* aClass);

	LogQueue&                                       log_;
	std::array<RED4ext::CBaseFunction*, kFunctions> found_{};
	std::array<bool, kFunctions>                    looked_{};
};
}
```

- [x] **Step 2: Write `src/Puppet.cpp`**

```cpp
#include "Puppet.h"

#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/Generated/EulerAngles.hpp>
#include <RED4ext/Scripting/Natives/Generated/Vector4.hpp>
#include <RED4ext/Scripting/Natives/Generated/ent/EntityID.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/StatsObjectID.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <string_view>
#include <type_traits>
#include <vector>

namespace cpmc
{
namespace
{
using RED4ext::CName;
using Strong = RED4ext::Handle<RED4ext::IScriptable>;
using Weak = RED4ext::WeakHandle<RED4ext::IScriptable>;

enum class Owner
{
	System,  // a game system's, by the system's script name
	Player,  // V's own class's
	Global,  // a global function, by its full name
};

// A function as these calls need it: each parameter one of the type names given, and the return type too (none: it
// returns nothing). A name ending in '*' takes any type whose name starts with the rest.
struct Signature
{
	Owner                                 owner;
	const char*                           system;
	const char*                           name;
	std::vector<std::vector<const char*>> params;
	std::vector<const char*>              returns;
};

// In Puppet::Function's order. The types are the ones the game's declarations and other mods' calls show:
// Teleport(objectToTeleport: ref<GameObject>, position: Vector4, orientation: EulerAngles) and
// GetStatPoolValue(StatsObjectID, gamedataStatPoolType, perc: Bool) -> Float. GetMountedVehicle's and
// IsActorInWorkspot's GameObject may be a ref or a wref, and the vehicle comes back as either.
const std::vector<Signature>& Signatures()
{
	static const std::vector<Signature> kSignatures{
		{ Owner::System, "PlayerSystem", "GetLocalPlayerMainGameObject", {}, { "handle:gameObject" } },
		{ Owner::System, "TeleportationFacility", "Teleport", { { "handle:gameObject" }, { "Vector4" }, { "EulerAngles" } }, {} },
		{ Owner::Player, nullptr, "GetWorldYaw", {}, { "Float" } },
		{ Owner::Global, nullptr, "GetMountedVehicle;GameObject", { { "handle:gameObject", "whandle:gameObject" } }, { "handle:*", "whandle:*" } },
		{ Owner::System, "WorkspotGameSystem", "IsActorInWorkspot", { { "handle:gameObject", "whandle:gameObject" } }, { "Bool" } },
		{ Owner::Player, nullptr, "IsDead", {}, { "Bool" } },
		{ Owner::Player, nullptr, "GetEntityID", {}, { "entEntityID" } },
		{ Owner::System, "StatPoolsSystem", "GetStatPoolValue", { { "gameStatsObjectID" }, { "gamedataStatPoolType" }, { "Bool" } }, { "Float" } },
	};
	return kSignatures;
}

bool TypeIs(const RED4ext::rtti::IType* aType, const char* aAccepted)
{
	if (!aType)
	{
		return false;
	}
	const std::string_view accepted(aAccepted);
	if (!accepted.ends_with('*'))
	{
		return aType->GetName() == CName(aAccepted);
	}
	const char* name = aType->GetName().ToString();
	return name && std::string_view(name).starts_with(accepted.substr(0, accepted.size() - 1));
}

bool Matches(const RED4ext::CBaseFunction& aFunction, const Signature& aSignature)
{
	if (aFunction.params.Size() != aSignature.params.size())
	{
		return false;
	}
	for (uint32_t i = 0; i < aFunction.params.Size(); ++i)
	{
		const RED4ext::CProperty* param = aFunction.params[i];
		const auto&               accepted = aSignature.params[i];
		if (!param || std::none_of(accepted.begin(), accepted.end(), [&](const char* aName) { return TypeIs(param->type, aName); }))
		{
			return false;
		}
	}
	if (aSignature.returns.empty())
	{
		return aFunction.returnType == nullptr;
	}
	return aFunction.returnType && std::any_of(aSignature.returns.begin(), aSignature.returns.end(), [&](const char* aName) {
		       return TypeIs(aFunction.returnType->type, aName);
	       });
}

std::string TypeName(const RED4ext::rtti::IType* aType)
{
	const char* name = aType ? aType->GetName().ToString() : nullptr;
	return name ? name : "?";
}

// "(handle:gameObject, Vector4) -> Bool", for the log.
std::string Describe(const RED4ext::CBaseFunction& aFunction)
{
	std::string out = "(";
	for (uint32_t i = 0; i < aFunction.params.Size(); ++i)
	{
		const RED4ext::CProperty* param = aFunction.params[i];
		out += (i == 0 ? "" : ", ") + TypeName(param ? param->type : nullptr);
	}
	return out + ")" + (aFunction.returnType ? " -> " + TypeName(aFunction.returnType->type) : "");
}

RED4ext::CClass* ClassNamed(const char* aScriptName)
{
	auto* rtti = RED4ext::CRTTISystem::Get();
	return rtti ? rtti->GetClassByScriptName(aScriptName) : nullptr;
}

// The game's instance of the system with this script name, or null: the instance is checked here, since
// ExecuteFunction's own lookup by class calls on whatever GetSystem returns.
RED4ext::IScriptable* SystemNamed(const char* aScriptName)
{
	auto*            engine = RED4ext::CGameEngine::Get();
	RED4ext::CClass* type = ClassNamed(aScriptName);
	if (!type || !engine || !engine->framework || !engine->framework->gameInstance)
	{
		return nullptr;
	}
	return engine->framework->gameInstance->GetSystem(type);
}

// V, through PlayerSystem's GetLocalPlayerMainGameObject (aGet); null when V isn't there.
Strong LocalPlayer(RED4ext::CBaseFunction* aGet)
{
	RED4ext::IScriptable* system = SystemNamed("PlayerSystem");
	Strong                player;
	if (!system || !aGet || !RED4ext::ExecuteFunction(system, aGet, &player))
	{
		return {};
	}
	return player;
}

// V as aFunction's parameter aIndex takes it: a weak handle for a wref, the handle for a ref.
void* PlayerArg(const RED4ext::CBaseFunction& aFunction, uint32_t aIndex, Strong& aStrong, Weak& aWeak)
{
	return TypeIs(aFunction.params[aIndex]->type, "whandle:*") ? static_cast<void*>(&aWeak) : static_cast<void*>(&aStrong);
}

// A static or global function: no instance. Cast, so the overload that looks a system up by class isn't the one taken.
constexpr void* kNoInstance = nullptr;
}

RED4ext::CBaseFunction* Puppet::Find(Function aFunction, RED4ext::CClass* aClass)
{
	if (looked_[aFunction])
	{
		return found_[aFunction];
	}
	const std::vector<Signature>& signatures = Signatures();
	if (signatures.size() != kFunctions)
	{
		return nullptr;
	}
	const Signature& signature = signatures[aFunction];
	auto*            rtti = RED4ext::CRTTISystem::Get();
	if (!rtti || (signature.owner != Owner::Global && !aClass))
	{
		return nullptr;
	}
	std::string others;
	auto        consider = [&](RED4ext::CBaseFunction* aCandidate) {
		if (!aCandidate || found_[aFunction])
		{
			return;
		}
		if (Matches(*aCandidate, signature))
		{
			found_[aFunction] = aCandidate;
		}
		else
		{
			others += (others.empty() ? "" : "; ") + Describe(*aCandidate);
		}
	};
	if (signature.owner == Owner::Global)
	{
		consider(rtti->GetFunction(CName(signature.name)));
	}
	else
	{
		// From the class up, so an override is found before what it overrides; and any overload whose types fit.
		const CName name(signature.name);
		for (RED4ext::CClass* type = aClass; type && !found_[aFunction]; type = type->parent)
		{
			for (RED4ext::CClassStaticFunction* function : type->staticFuncs)
			{
				if (function && function->shortName == name)
				{
					consider(function);
				}
			}
			for (RED4ext::CClassFunction* function : type->funcs)
			{
				if (function && function->shortName == name)
				{
					consider(function);
				}
			}
		}
	}
	looked_[aFunction] = true;
	const std::string where = signature.owner == Owner::System   ? std::string(signature.system) + "."
	                          : signature.owner == Owner::Player ? "V's "
	                                                             : "";
	log_.Push(found_[aFunction] ? std::format("puppet: found {}{}{}", where, signature.name, Describe(*found_[aFunction]))
	                            : std::format("puppet: {}{} isn't there with the types these calls pass{}", where, signature.name,
	                                          others.empty() ? "" : "; the game has " + others));
	return found_[aFunction];
}

std::string Puppet::Blocked()
{
	Strong player = LocalPlayer(Find(kPlayer, ClassNamed("PlayerSystem")));
	if (!player)
	{
		return {};
	}
	RED4ext::CClass* const type = player.GetPtr()->GetType();
	if (!Find(kTeleport, ClassNamed("TeleportationFacility")))
	{
		return "the game's TeleportationFacility.Teleport isn't there (see the log)";
	}
	if (!Find(kYaw, type))
	{
		return "V's GetWorldYaw isn't there (see the log)";
	}
	Weak weak(player);
	if (RED4ext::CBaseFunction* const function = Find(kVehicle, nullptr))
	{
		RED4ext::StackArgs_t args{ { nullptr, PlayerArg(*function, 0, player, weak) } };
		bool                 inVehicle = false;
		if (TypeIs(function->returnType->type, "whandle:*"))
		{
			Weak vehicle;
			inVehicle = RED4ext::ExecuteFunction(kNoInstance, function, &vehicle, args) && vehicle.Lock();
		}
		else
		{
			Strong vehicle;
			inVehicle = RED4ext::ExecuteFunction(kNoInstance, function, &vehicle, args) && vehicle;
		}
		if (inVehicle)
		{
			return "V is in a vehicle";
		}
	}
	RED4ext::IScriptable* const workspots = SystemNamed("WorkspotGameSystem");
	if (RED4ext::CBaseFunction* const function = workspots ? Find(kWorkspot, ClassNamed("WorkspotGameSystem")) : nullptr)
	{
		RED4ext::StackArgs_t args{ { nullptr, PlayerArg(*function, 0, player, weak) } };
		bool                 inWorkspot = false;
		if (RED4ext::ExecuteFunction(workspots, function, &inWorkspot, args) && inWorkspot)
		{
			return "V is in a workspot";
		}
	}
	if (RED4ext::CBaseFunction* const function = Find(kDead, type))
	{
		bool dead = false;
		if (RED4ext::ExecuteFunction(player.GetPtr(), function, &dead) && dead)
		{
			return "V is dead";
		}
	}
	return {};
}

std::optional<double> Puppet::Yaw()
{
	const Strong                  player = LocalPlayer(Find(kPlayer, ClassNamed("PlayerSystem")));
	RED4ext::CBaseFunction* const function = player ? Find(kYaw, player.GetPtr()->GetType()) : nullptr;
	float                         yaw = 0.0f;
	if (!function || !RED4ext::ExecuteFunction(player.GetPtr(), function, &yaw) || !std::isfinite(yaw))
	{
		return std::nullopt;
	}
	return yaw;
}

std::optional<double> Puppet::Health()
{
	const Strong                  player = LocalPlayer(Find(kPlayer, ClassNamed("PlayerSystem")));
	RED4ext::IScriptable* const   pools = SystemNamed("StatPoolsSystem");
	RED4ext::CBaseFunction* const entityId = player ? Find(kEntityId, player.GetPtr()->GetType()) : nullptr;
	RED4ext::CBaseFunction* const value = pools ? Find(kPool, ClassNamed("StatPoolsSystem")) : nullptr;
	RED4ext::ent::EntityID        id{};
	if (!entityId || !value || !RED4ext::ExecuteFunction(player.GetPtr(), entityId, &id))
	{
		return std::nullopt;
	}
	// As redscript's Cast<StatsObjectID>(EntityID) makes it.
	RED4ext::game::StatsObjectID object{};
	object.entityHash = id.hash;
	object.idType = RED4ext::game::StatIDType::EntityID;
	int64_t              pool = 17;  // gamedataStatPoolType.Health; wide enough for the enum, whatever its size
	bool                 percent = true;
	float                health = 0.0f;
	RED4ext::StackArgs_t args{ { nullptr, &object }, { nullptr, &pool }, { nullptr, &percent } };
	if (!RED4ext::ExecuteFunction(pools, value, &health, args) || !std::isfinite(health))
	{
		return std::nullopt;
	}
	return health;
}

bool Puppet::Teleport(const Vec3d& aFeet, double aYaw, std::string& aError)
{
	Strong                        player = LocalPlayer(Find(kPlayer, ClassNamed("PlayerSystem")));
	RED4ext::IScriptable* const   facility = SystemNamed("TeleportationFacility");
	RED4ext::CBaseFunction* const function = facility ? Find(kTeleport, ClassNamed("TeleportationFacility")) : nullptr;
	if (!player)
	{
		aError = "V isn't found";
		return false;
	}
	if (!function)
	{
		aError = "the game's TeleportationFacility.Teleport isn't there (see the log)";
		return false;
	}
	if (!std::isfinite(aFeet.x) || !std::isfinite(aFeet.y) || !std::isfinite(aFeet.z) || !std::isfinite(aYaw))
	{
		aError = "the target isn't finite";
		return false;
	}
	RED4ext::Vector4     position(float(aFeet.x), float(aFeet.y), float(aFeet.z), 1.0f);
	RED4ext::EulerAngles orientation{ 0.0f, 0.0f, float(aYaw) };  // roll, pitch, yaw, as FreeFly passes them
	RED4ext::StackArgs_t args{ { nullptr, &player }, { nullptr, &position }, { nullptr, &orientation } };
	if (!RED4ext::ExecuteFunction(facility, function, nullptr, args))
	{
		aError = "the game's Teleport call failed";
		return false;
	}
	return true;
}
}
```

- [x] **Step 3: Edit `CMakeLists.txt` to add the source**

```cmake
	src/PlayerLocator.cpp
)
```
```cmake
	src/PlayerLocator.cpp
	src/Puppet.cpp
)
```

- [x] **Step 4: Build and run the tests; they must pass**

The plugin compiles `Puppet`; no test can run it.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `289 test(s), 0 failure(s)`.

- [x] **Step 5: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: Puppet, the game's script calls that place V and say why it can't be" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 15: BlockRender 0.11.0 places V

The plugin's side of Part 1 (the spec's "Entering", "Each frame while Minecraft drives" and "Leaving"), in
`UpdateLink`:
- **Every frame:** the mouse's movement along x is taken. While linked with V found, V's yaw is read, and why V can't
  be placed. Outside build mode, while V can be placed, they feed the turn ratio; at any other time it starts afresh.
- **The link's frame** gains `QueryPerformanceCounter`, for the ticks.
- **After the link's update:**
  - the router gets V's reason, so build mode ends, or won't start, while there's one;
  - if the link says build mode must end, it ends, saying why;
  - otherwise, while build mode is on and the link places V, V is teleported there, facing its own yaw, plus the
    frame's turn with `set turn 1`. A teleport that fails ends build mode, saying why, so Minecraft never walks away
    from a V that can't follow.
- **The status:** the drive line's figures, from the link and this second's eye distances and teleport times; V's
  health, read once a second; the turn.
- **Requests:** `set turn 0|1`, and `drivelog <seconds>`, written to `logs\<time>\drive.csv` when it's done.
- **The version:** 0.11.0, in the log line, `Query` and CMake.

**Files:**
- Modify: `src/Plugin.cpp`, `CMakeLists.txt`

- [x] **Step 1: Edit `src/Plugin.cpp`**

```cpp
// in build mode (F8) the mouse buttons, the wheel and the keys 1 to 9 are Minecraft's.
// Commands come through blockrender.request, in its folder or the overlay's, and status.txt in the overlay's folder says
// what it's doing. See docs/superpowers/specs/2026-10-01-block-render-design.md and 2026-10-02-minecraft-link-design.md.
```
```cpp
// in build mode (F8) the mouse buttons, the wheel and the keys 1 to 9 are Minecraft's. Spike 5 has Minecraft drive in
// build mode: the movement keys are Minecraft's too, and V is teleported to its player's eye every frame.
// Commands come through blockrender.request, in its folder or the overlay's, and status.txt in the overlay's folder says
// what it's doing. See docs/superpowers/specs/2026-10-01-block-render-design.md, 2026-10-02-minecraft-link-design.md and
// 2026-10-03-collision-and-movement-design.md.
```
```cpp
#include "ProcessUsage.h"
```
```cpp
#include "ProcessUsage.h"
#include "Puppet.h"
```
```cpp
#include "link/Mapping.h"
```
```cpp
#include "link/Mapping.h"
#include "link/Turn.h"
#include "log/DriveLog.h"
```
```cpp
constexpr const char* kVersion = "0.10.0";
```
```cpp
constexpr const char* kVersion = "0.11.0";
```
```cpp
// This frame's figures, for bursts.
```
```cpp
// Spike 5: build mode's drive (its spec's "Part 1"). Main thread only.
cpmc::Puppet          g_puppet{ g_log };  // the game's script calls that place V
cpmc::link::TurnRatio g_turnRatio;        // the mouse's counts to V's degrees, measured outside build mode
bool                  g_turn = false;     // set turn 1: each teleport's yaw gains the frame's turn from the mouse
cpmc::DriveLog        g_driveLog;

// This frame's figures, for bursts.
```
```cpp
	std::array<uint64_t, cpmc::link::Link::kTypes> bytesAtStart{};
```
```cpp
	std::array<uint64_t, cpmc::link::Link::kTypes> bytesAtStart{};
	// The drive's
	uint64_t waitedAtStart = 0;
	double   eyeDistanceSum = 0.0;
	double   eyeDistanceWorst = 0.0;
	uint64_t eyeDistanceFrames = 0;
	double   teleportMsSum = 0.0;
	double   teleportMsWorst = 0.0;
	uint64_t teleportCalls = 0;
```
```cpp
// The link, once a frame (the spec's "The link"): V's pose and the HUD's size, then Link::Update, which hands what
// Minecraft sends to the sink.
void UpdateLink(double aNow)
{
	g_frame = {};
```
```cpp
// QueryPerformanceCounter: the clock Minecraft stamps its ticks with.
int64_t QpcNow()
{
	LARGE_INTEGER now{};
	QueryPerformanceCounter(&now);
	return now.QuadPart;
}

int64_t QpcPerSecond()
{
	static const int64_t perSecond = [] {
		LARGE_INTEGER frequency{};
		QueryPerformanceFrequency(&frequency);
		return frequency.QuadPart;
	}();
	return perSecond;
}

// What placing V did this frame, for the status and the drive log.
struct Placed
{
	double                teleportMs = 0.0;
	double                turn = 0.0;  // degrees set turn 1 added
	std::optional<double> yaw;         // the teleport's, if V was teleported
};

// Spike 5's drive, after the link's update: build mode ends if the link says so; otherwise, while it's on, V goes where
// the link puts it, facing its own yaw, and the frame's turn from the mouse with set turn 1. A teleport that fails ends
// build mode, so Minecraft never walks away from a V that can't follow.
Placed PlaceV(const cpmc::link::Link::Status& aState, const std::optional<double>& aYaw, int64_t aMouseX)
{
	Placed placed;
	if (!aState.endBuild.empty())
	{
		g_router->TurnOff(aState.endBuild);
		return placed;
	}
	if (!aState.placeV || !g_router->On())
	{
		return placed;
	}
	if (!aYaw)
	{
		g_router->TurnOff("V's yaw can't be read");
		return placed;
	}
	placed.turn = g_turn ? g_turnRatio.Turn(aMouseX) : 0.0;
	std::string  error;
	const double start = cpmc::SteadySeconds();
	const bool   moved = g_puppet.Teleport(*aState.placeV, *aYaw + placed.turn, error);
	placed.teleportMs = (cpmc::SteadySeconds() - start) * 1000.0;
	if (!moved)
	{
		g_router->TurnOff("V couldn't be placed: " + error);
		return placed;
	}
	placed.yaw = *aYaw + placed.turn;
	++g_second.teleportCalls;
	g_second.teleportMsSum += placed.teleportMs;
	g_second.teleportMsWorst = std::max(g_second.teleportMsWorst, placed.teleportMs);
	return placed;
}

// The link, once a frame (the spec's "The link"): V's pose and the HUD's size, then Link::Update, which hands what
// Minecraft sends to the sink. In build mode, V then goes where the link puts it (spike 5).
void UpdateLink(double aNow, double aFrameMs)
{
	g_frame = {};
	const int64_t mouseX = g_router->TakeMouseX();  // every frame, so a frame's turn is only its own
```
```cpp
	const bool buildMode = g_router->On();
```
```cpp
	const bool buildMode = g_router->On();
	// While linked: V's yaw, and what keeps V from being placed. The turn ratio is measured outside build mode, while V
	// can be placed, so its yaw is V's own doing.
	const std::optional<double> yaw = g_linked && pose ? g_puppet.Yaw() : std::nullopt;
	const std::string           blocked = g_linked && pose ? g_puppet.Blocked() : std::string();
	if (!buildMode && yaw && blocked.empty())
	{
		g_turnRatio.Note(mouseX, *yaw);
	}
	else
	{
		g_turnRatio.Break();
	}
```
```cpp
	frame.tickMs = GetTickCount64();
```
```cpp
	frame.tickMs = GetTickCount64();
	frame.qpc = QpcNow();
	frame.qpcPerSecond = QpcPerSecond();
```
```cpp
	const uint64_t drainedBefore = state.drainedBytes;
	g_link->Update(frame, g_linkSettings, g_sink);
	g_router->Update(g_linked, state);  // build mode ends if Minecraft or V has gone, or the link
	if (std::exchange(g_buildWanted, false))
	{
		const std::optional<std::string> why = g_router->TurnOn();
		Report(why ? "build on: it stays off: " + *why : "build on: the mouse buttons, the wheel and 1 to 9 are Minecraft's");
```
```cpp
	const uint64_t drainedBefore = state.drainedBytes;
	const uint64_t waitedBefore = state.waitedFrames;
	g_link->Update(frame, g_linkSettings, g_sink);
	g_router->Update(g_linked, state, blocked);  // build mode ends if Minecraft or V has gone, or the link, or V can't be placed
	const Placed placed = PlaceV(state, yaw, mouseX);
	if (state.eyeDistance)
	{
		g_second.eyeDistanceSum += *state.eyeDistance;
		g_second.eyeDistanceWorst = std::max(g_second.eyeDistanceWorst, *state.eyeDistance);
		++g_second.eyeDistanceFrames;
	}
	if (g_driveLog.Recording())
	{
		cpmc::DriveRow row;
		row.time = aNow;
		row.frameMs = aFrameMs;
		row.state = cpmc::link::DriveStateName(state.drive);
		row.camera = pose ? std::optional<cpmc::Vec3d>(pose->camera) : std::nullopt;
		row.target = state.placeV ? std::optional<cpmc::Vec3d>(cpmc::Vec3d{ state.placeV->x, state.placeV->y, state.placeV->z + state.eyeHeight })
		                          : std::nullopt;
		row.lastDistance = state.eyeDistance;
		row.delayMs = state.renderDelayMs;
		row.waited = state.waitedFrames != waitedBefore;
		row.teleportMs = placed.teleportMs;
		row.mouseX = mouseX;
		row.yaw = placed.yaw;
		row.turn = placed.turn;
		g_driveLog.Add(row);
	}
	if (std::exchange(g_buildWanted, false))
	{
		const std::optional<std::string> why = g_router->TurnOn();
		Report(why ? "build on: it stays off: " + *why
		           : "build on: Minecraft's player drives V; the movement keys, the mouse buttons, the wheel and 1 to 9 are Minecraft's");
```
```cpp
		g_hudScale = uint32_t(value);
```
```cpp
		g_hudScale = uint32_t(value);
	}
	else if (name == "turn")
	{
		if (value != 0.0 && value != 1.0)
		{
			Report("set turn: 0 teleports V with its own yaw, and 1 adds the frame's turn from the mouse");
			return true;
		}
		g_turn = value == 1.0;
```
```cpp
	case cpmc::Request::Kind::Build: BuildRequest(request->what == "on"); break;
```
```cpp
	case cpmc::Request::Kind::Build: BuildRequest(request->what == "on"); break;
	case cpmc::Request::Kind::DriveLog:
		Report(g_driveLog.Start(request->number) ? std::format("drivelog: recording the drive for {:g} s", request->number)
		                                         : "drivelog: one is already recording");
		break;
```
```cpp
// The link's lines of the status (the spec's "Measurements"), from the second since the last status.
```
```cpp
void WriteDriveLog(const cpmc::DriveLog::Finished& aLog, const std::string& aFolderName)
{
	const std::filesystem::path folder = g_pluginDir / "logs" / aFolderName;
	std::error_code             error;
	std::filesystem::create_directories(folder, error);
	std::ofstream out(folder / "drive.csv", std::ios::binary);
	out << aLog.csv;
	if (!out)
	{
		return Report("drivelog: couldn't write " + (folder / "drive.csv").string());
	}
	Report(std::format("drivelog: written to logs\\{}\\drive.csv ({} frames)", aFolderName, aLog.rows));
}

// The link's lines of the status (the spec's "Measurements"), from the second since the last status.
```
```cpp
		report.hudRefused = state.hudRefused;
	}
```
```cpp
		report.hudRefused = state.hudRefused;
		report.drive = cpmc::link::DriveStateName(state.drive);
		report.driveFor = state.drive == cpmc::link::DriveState::Off ? 0.0 : aNow - state.driveSince;
		report.eyeHeight = state.eyeHeight;
		report.eyeDistanceLast = state.eyeDistance;
		report.eyeDistanceAverage = g_second.eyeDistanceFrames ? g_second.eyeDistanceSum / double(g_second.eyeDistanceFrames) : 0.0;
		report.eyeDistanceWorst = g_second.eyeDistanceWorst;
		report.renderDelayMs = state.renderDelayMs;
		report.waitedFrames = state.waitedFrames - g_second.waitedAtStart;
		report.skippedFrames = state.driveSkipped;
		report.teleportMsAverage = g_second.teleportCalls ? g_second.teleportMsSum / double(g_second.teleportCalls) : -1.0;
		report.teleportMsWorst = g_second.teleportCalls ? g_second.teleportMsWorst : -1.0;
		report.minecraftFlags = state.minecraftFlags;
	}
	report.health = g_linked ? g_puppet.Health() : std::nullopt;  // once a second, so fall damage shows
	report.turn = g_turn;
	report.turnRatio = g_turnRatio.Ratio();
	report.turnTurns = g_turnRatio.Turns();
```
```cpp
		g_second.bytesAtStart = state.bytes;
```
```cpp
		g_second.bytesAtStart = state.bytes;
		g_second.waitedAtStart = state.waitedFrames;
```
```cpp
	UpdateLink(seconds);
```
```cpp
	UpdateLink(seconds, frameMs);
```
```cpp
		WriteFrameLog(*finished, folderName);
```
```cpp
		WriteFrameLog(*finished, folderName);
	}
	if (const std::optional<cpmc::DriveLog::Finished> finished = g_driveLog.TakeFinished())
	{
		WriteDriveLog(*finished, folderName);
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 10, 0);
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 11, 0);
```

- [x] **Step 2: Edit `CMakeLists.txt`: the version**

```cmake
project(BlockRender VERSION 0.10.0 LANGUAGES C CXX)
```
```cmake
project(BlockRender VERSION 0.11.0 LANGUAGES C CXX)
```

- [x] **Step 3: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `289 test(s), 0 failure(s)`.

- [x] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: BlockRender 0.11.0 places V where Minecraft's player is, in build mode" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 16: The overlay's drive line

The status's lines are long, so under build mode's line the overlay shows the drive in brief: its state, V's camera's
distance from Minecraft's eye while driving, and V's health. Beside it, a button toggles `set turn`, labelled with the
setting, and another records 10 s with `drivelog 10`, so step 2 needs no typed requests.

**Files:**
- Modify: `tools/blockrender_hud/init.lua`

- [x] **Step 1: Edit `tools/blockrender_hud/init.lua`**

```lua
-- Minecraft's hotbar slot. Delete this folder to remove it.
```
```lua
-- Minecraft's hotbar slot; and since spike 5, one shows build mode's drive, with buttons for its turn and its drive log.
-- Delete this folder to remove it.
```
```lua
local build = nil        -- the build line's first two parts: on, or off and why; and the slot
```
```lua
local build = nil        -- the build line's first two parts: on, or off and why; and the slot
local drive = nil        -- the drive line's state, V's camera from Minecraft's eye, and V's health
local turn = nil         -- the drive line's turn setting: "on" or "off"
```
```lua
    build = nil
```
```lua
    build = nil
    drive = nil
    turn = nil
```
```lua
            build = mode .. ", " .. slot
```
```lua
            build = mode .. ", " .. slot
        end
        local state = line:match("^drive=([^;]*)")
        if state ~= nil then
            local eye = line:match("V's camera from Minecraft's eye ([%d.]+ m)")
            local health = line:match("V's health ([^;]*)")
            drive = state .. (eye and (", V's camera " .. eye .. " from Minecraft's eye") or "")
                .. (health and (", V's health " .. health) or "")
            turn = line:match("; turn (%a+),")
```
```lua
        end
        ImGui.Separator()
```
```lua
        end
        if drive ~= nil then
            ImGui.Text("Drive: " .. drive)
            if turn ~= nil then
                -- "###turn" keeps the button's ID the same while its label changes.
                ImGui.SameLine()
                if ImGui.Button("Turn: " .. turn .. "###turn") then
                    request("set turn " .. (turn == "on" and 0 or 1))
                end
            end
            ImGui.SameLine()
            if ImGui.Button("Drive log 10 s") then
                request("drivelog 10")
            end
        end
        ImGui.Separator()
```

- [x] **Step 2: Check the overlay's syntax**

```bash
luac -p /d/Cyberpunk-Minecraft/spikes/block-render/tools/blockrender_hud/init.lua && echo "syntax ok"
```
Expected: `syntax ok`. (`luac` is Lua 5.4's; CET runs LuaJIT, and the overlay uses nothing that differs between them.)
Its patterns were also run on the two lines `StatusWritesTheDrive` expects, and gave `Drive: driving for 12.3 s,
V's camera 0.012 m from Minecraft's eye, V's health 100%` with the turn `on`, and `Drive: off, V's health unknown` with
the turn `off`.

- [x] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the overlay shows the drive, with buttons for the turn and the drive log" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 17: In the game: step 2

Step 2 of the spec's "The steps", with the user: BlockRender 0.11.0 in the game, linked to SkyCraft's Minecraft in the
`CyberCraft` instance, at spike 3's corner (world x −116.6, y −2005.2) with the same save, on spike 4's floor. Part 1
has no `ground` setting yet: the floor is the only ground.

The status's `drive=` line and the overlay's `Drive:` line show the drive; the log's lines starting `puppet:` show the
script calls. FreeFly (F7) must stay off throughout: it teleports V too (the spec's "Safety"). Note what you see as you
go: Step 11 records it. If a check fails, stop and find the cause with
superpowers:systematic-debugging before going on. Fixes become new tasks.

**Files:**
- Modify: `docs/spikes/2026-10-03-collision-and-movement-results.md`

- [x] **Step 1: Deploy (the game must not be running)**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game first, because a running game locks the DLL.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default -DCP2077_DIR=E:/Cyberpunk2077 > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C|error MSB"; \
  "$CMAKE" --preset default -DCP2077_DIR= > /dev/null; \
  ls /e/Cyberpunk2077/red4ext/plugins/BlockRender /e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud \
  && cmp build/RelWithDebInfo/BlockRender.dll /e/Cyberpunk2077/red4ext/plugins/BlockRender/BlockRender.dll && echo installed
```
Expected: the plugin's folder holds `BlockRender.dll`, `BlockRender.pdb` and `shaders`, with spike 3's `atlas.bin`
and `atlas.json`; the overlay's folder holds `init.lua`; then `installed`. The second configure clears `CP2077_DIR`
again.

- [x] **Step 2: In the game: load, link, and find the script calls**

Ask the user to start the `CyberCraft` instance from the Modrinth App, then to launch the game, load the free-roam
save, go to the corner, and stand still with open street ahead. Then ask the user to click the overlay's Link button.
Expected:
- in the log: `BlockRender 0.11.0 loaded`, then `link: Minecraft is there (process …)` and a floor at V's feet;
- eight `puppet: found …` lines: `PlayerSystem.GetLocalPlayerMainGameObject`, `TeleportationFacility.Teleport`, V's
  `GetWorldYaw`, `GetMountedVehicle;GameObject`, `WorkspotGameSystem.IsActorInWorkspot`, V's `IsDead`, and, within a
  second, V's `GetEntityID` and `StatPoolsSystem.GetStatPoolValue`, each with the types it was found with;
- in the status: `link=linked; …; in its world, …; in game`, and the drive line `drive=off; V's eye 1.6… m above its
  feet; …; V's health 100%; turn off, ratio not measured yet (0 of 5 turns); Minecraft's player on the ground`.

A `puppet: … isn't there with the types these calls pass; the game has …` line means a signature differs from the
plan's: stop, and fix `Puppet`'s table from what the line says, as a new task.

Then send `mc creative` and `mc /item replace entity @s hotbar.0 with stone 64`, each once the last shows in the
status's `request=` line. Ask the user to turn V left and right with the mouse a few times, pausing between turns. The
drive line's ratio must then be measured: `turn off, -0.0… degrees a count from … turns`, or positive. Its sign says
which way the game's yaw turns.

- [x] **Step 3: In the game: the opening check**

Ask the user to press F8 and to stand still in Minecraft. Expected: within a second, `build mode on`, `link: build
mode: Minecraft's player goes to V's feet, …` and `link: Minecraft let its player go after … s: V follows it`; the
overlay's `Drive: driving for …`, with V's camera within a few centimetres of Minecraft's eye.

Then ask the user to turn the mouse and look up and down, still standing, and to say whether the view moves as it does
outside build mode. If it lags, jumps back or won't turn, ask the user to click the overlay's `Turn: off` button, so
it reads `Turn: on`, and to try again. Record which works: it settles risk 1's first question.

- [x] **Step 4: In the game: moving**

With build mode on, ask the user to walk, strafe and back up, then sprint (Left Shift toggles it), jump, sneak (Left
Ctrl, held), and fly (Space twice) up with Space and down with Left Ctrl, and to say how smooth each is. While they
walk and turn, ask them to click `Drive log 10 s`. Expected:
- V moves as Minecraft's player does, at walking and sprinting speed, and sneaking drops the view about a third of a
  metre;
- the drive line's camera-to-eye distance stays within a few centimetres, its render delay between 4 and 30 ms, and
  few frames wait for a late tick;
- after 10 s, `request: drivelog: written to logs\…\drive.csv (… frames)`.

Read the CSV: the target column must move smoothly frame to frame. Where the camera column steps unevenly though the
target doesn't, the teleport lands late in the frame: it settles risk 1's second question.

- [x] **Step 5: In the game: entering with W held, and clicks while moving**

Ask the user to leave build mode (F8), hold W, press F8 while still holding it, then let go of W and press it again.
Expected: V stays where it is until the drive says `driving`, since the held W stays the game's and the drive holds
V; once W is pressed again, Minecraft's player walks, and V with it.

Then ask the user to place a row of stone on the street while walking: right clicks with W held, looking down a
little. Expected: every click places a block, and no red message shows in Minecraft's chat in the HUD.

- [x] **Step 6: In the game: fall damage while placed**

Ask the user to make a save first, in case V dies. Then, in build mode, fly up about 5 m (Space twice, then Space
held) and stop flying (Space twice) to drop to the street; then the same from about 20 m. Expected: Minecraft's
creative player takes no damage. Record the drive line's health after each drop: whether V's own movement fights the
teleports (risk 2).

- [x] **Step 7: In the game: leaving**

Ask the user to:
1. press F8 standing: V stays where Minecraft's player was, and the log says `build mode off: F8 turned it off`;
2. press F8 again, fly a few metres up, and press F8: V falls, as in the game, with the game's fall damage;
3. press F8, then Esc: `build mode off: Esc left it`, and the game's menu opens;
4. press F8 once more: build mode on again, and driving within a second.

After each, the status's teleport count grows again: the link's teleports have resumed from V's camera.

- [x] **Step 8: In the game: a vehicle**

With build mode on, ask the user to walk up to a car and get in with F, which V still interacts with. Expected:
`build mode off: V is in a vehicle`, or `V is in a workspot`, since a car's seat is one. Then, in the car, F8 must say
`build mode stays off: …` with the same reason.

- [x] **Step 9: In the game: the cost**

Ask the user to stand still for 10 s with build mode on, then 10 s with it off. Read the status's `frame=` line and
CET's frame time each time, and the drive line's teleport time (average and worst).

- [x] **Step 10: Ask the user to quit the game and stop Minecraft**

- [x] **Step 11: Record step 2's results in `docs/spikes/2026-10-03-collision-and-movement-results.md`**

A "Step 2" section: each check above with its outcome and numbers; which turn setting worked; what the drive log
showed; V's health after the drops; the cost; and anything that surprised, and what it means for Part 2.

- [x] **Step 12: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-03-collision-and-movement-results.md \
  && git commit -q -m "collision-and-movement: step 2's results" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 18: Close Part 1, and plan Part 2

- [x] **Step 1: Add "How Part 1 went" to this plan**

At the end, after "Found while planning": what ran as written, what changed, and what steps 1 and 2 found.

- [x] **Step 2: Plan Part 2**

Use superpowers:writing-plans to add Part 2's tasks, from Task 19 on, to this plan: Night City's shape in Minecraft
(the spec's "Part 2" and step 3). Verify them as Part 1's were. Changes to the spec found while planning go in its
"Changes while planning".

- [x] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/superpowers \
  && git commit -q -m "collision-and-movement plan: Part 1 closed, Part 2 planned" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

## Found while planning

The spec's "Changes while planning" records these too.
- **The voxelizer's boundary:** SkyCraft's candidate range was empty for a triangle lying on a sub-voxel boundary, so a
  floor at a whole height got no voxels. Task 2 fills the layers on both sides.
- **`drivelog` instead of the frame log:** the spec had the frame log (`log`) gain each frame's camera and target. The
  frame log records two frames of the renderer's D3D12 work; judder needs seconds of frames, so Task 11's drive log
  records them, as CSV.
- **`mclink`'s `build`** now turns the drive on and off, and starts off.
- **The order:** the tasks follow the testable steps. `mclink` and step 1 (Tasks 1 to 8) come before the plugin's
  tasks, so the link's side is checked against the real Minecraft before anything goes into the game.
- **Minecraft's player dead** ends build mode, and keeps it off, through the router, from McState's flag.
- **The vehicle check** is the global `GetMountedVehicle;GameObject`, not `VehicleComponent.IsMountedToVehicle`, whose
  `ScriptGameInstance` the SDK would abort the game over if its size changed. A vehicle's seat is a workspot too, so
  the workspot check also catches it.
- **`Puppet`'s safety:** functions picked by their types, systems checked before each call, and a `void*` instance for
  static and global calls (Task 14).
- **The turn ratio** is the median of the last turns, so a menu's cursor or a turn the game makes by itself doesn't
  move it.
- **The overlay** gains a short drive line, and buttons for `set turn` and `drivelog 10`.
- **The drive line** leaves why build mode is off to the `build=` line above it.

## How Part 1 went

Part 1 ran on 2026-10-03, and its answer is yes, with `set turn 1`. The results are in
[docs/spikes/2026-10-03-collision-and-movement-results.md](../../spikes/2026-10-03-collision-and-movement-results.md).
- **As written:** Tasks 1 to 7 and 9 to 16 ran through the plan runner in the real repo exactly as written. Every
  build, test, `mclink` and overlay expectation was met, and each task's files matched the verified scratch clone's.
  289 tests, no warnings. No code changed after running it.
- **Step 1 (Task 8):** passed against SkyCraft's real mod. Two of its expectations were worded too tightly:
  - the gold block: the script's held keys jumped onto it and straight off, shown by a jump peak only its top reaches;
  - V's feet trail Minecraft's newest position while moving, by the render delay: 4.7 cm walking, by design.
- **Step 2 (Task 17):** passed, with `set turn 1`.
  - **The look:** with V's own yaw, the teleport locked turning left and right. Adding the frame's mouse turn, at the
    measured −0.05 degrees a count, made it smooth, which settles risk 1's first question.
  - **The drive:** V's camera lands on where the last frame put it within 4 to 7 mm, one frame of latency and no
    judder, which settles risk 1's second question.
  - **Safe for V:** falls from up to 47 m did no damage, so risk 2 doesn't happen. A car's knockdown recovered in 4 s
    with build mode on.
  - **The script calls:** all eight were found with the types `Puppet` expects, and nothing faulted.
  - **The cost:** build mode costs about 0.3 ms a frame, all of it Minecraft's HUD.
- **One message differed:** F8 in a car says "V isn't found" rather than "V is in a vehicle", since the car's camera
  isn't V's eye. Build mode still stays off, as it should.
- **Left for Part 2:**
  - `set turn 1` by default;
  - V's reason even when the link finds no camera;
  - the eye height measured only while V can be placed, not in a car seat;
  - the long frame, 53 to 56 ms, on entering build mode;
  - and, as a choice, a field-of-view kick while sprinting. The speeds match Minecraft's, but the user found
    sprinting slow without it.

---

# Collision and Movement Implementation Plan, Part 2 (step 3)

> Planned on 2026-10-04, after Part 1 ran. As Part 1: inline, with superpowers:executing-plans, never through
> sub-agents. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Part 2 of spike 5. Minecraft gets Night City's real collision instead of the floor, read from the game's
PhysX around Minecraft's player, so its player walks the city's streets and stops at its walls, and V follows it. Part
1's follow-ups come first. Step 3 checks it offline with `mclink` and spike 1's saved dumps, then in the game.

**Architecture:**
- **Part 1's follow-ups** (Tasks 19 to 22): the turn on by default; V's reason said before "V isn't found"; V's eye
  height measured only while V can be placed; and the floor's voxels made once a height, which ends the long frame on
  entering build mode.
- **The ground, pure, in `render_core`** (Tasks 23 to 26):
  - `link/Ground`: regions and 2 × 2 × 2 batches, triangles filed into regions, a hash that ignores their order, and
    `GroundAreas`, SkyCraft's area around a centre, read nearest first, refreshed and forgotten as SkyCraft does;
  - `link/GroundWorker`: one thread that voxelizes each region and makes its messages;
  - `link/Link`: one collision epoch for the floor and the city, `set ground`'s switch, the city's clears and settling,
    and `ReadGround`, which reads batches from a `GroundSource` within a budget a frame. The worker's messages wait for
    room in the collision ring, in order.
- **Offline** (Tasks 27 and 28): `mclink` loads spike 1's dumps as a `GroundSource`, and Minecraft's player walks spot
  B and the ramp.
- **The scene watch** (Tasks 29 to 33): spike 1's PhysX reader moves in; a lock-free record of scene 0's calls; hooks on
  16 audited slots and `scenewatch on|off` in BlockRender 0.12.0; the audit's addendum; and, in the game, the watch
  settles where the exporter may read.
- **The exporter** (Tasks 34 to 36): `physx/Export` reads scene 0's static scene-query shapes as Minecraft's triangles;
  BlockRender 0.13.0 sends them, with `set ground`, `set exportbudget`, `shapes`, the status's ground line and the
  overlay's buttons; then step 3 in the game.

**Tech stack:** as Part 1's, with NVIDIA's PhysX 3.4.2 headers (`extern/physx-3.4.2`) as spike 1 used them: PhysX is
called only through its virtual tables and inline header code, and `PxGetPhysics` through `GetProcAddress`, so
BlockRender imports nothing from PhysX's DLLs.

**Spec:** the spec's "Part 2: Night City's shape in Minecraft", "The audit", "Step 3", "Measurements and the status"
and "Safety", with what planning changed in its "Changes while planning".

## Before you start (Part 2)

- **Part 1's "Before you start" holds:** the branch, the shell and `CMAKE`, Edit steps' pairs, the user, the game and
  deploying, no faults, and the commits.
- **Copy steps** (Task 29) copy a file unchanged, from the first path to the second; Edit steps then change it.
- **Already verified,** as Part 1 was:
  - all the code has been compiled and tested once already, in a scratch clone, and the tasks replayed in a fresh
    clone, applying every file step exactly as written and checking every build, test, `mclink` and overlay step;
  - zero compiler warnings at `/W4`, and BlockRender's DLL imports nothing from PhysX (`dumpbin /imports`);
  - each new test was broken once, by a mutation of the code it covers, and failed as it should;
  - the scene watch's 16 slots were found in the game's `PhysX3_x64.dll` (Task 32's addendum), and spot B's and the
    ramp's walks were worked out from the dumps' own triangles, which Task 28's expectations come from.

  So a failure while following the plan most likely means a transcription slip: compare against the plan's code first.
- **Test counts:** 289 at the start. Then 289, 291, 293 and 294 after Tasks 19 to 22; 299, 304, 307, 314 and 317 after
  Tasks 23 to 27; 348, 355 and 358 after Tasks 29 to 31; and 362 and 364 after Tasks 34 and 35.
- **What only Minecraft and the game can test:** step 3's offline half in Task 28, the scene watch and the follow-ups in
  Task 33, and the city in Task 36, all with the user.
- **Task 33 decides where the exporter reads.** If it finds the game writing to scene 0 while OnUpdate runs, without the
  write lock, stop after it: Tasks 34 to 36 read in OnUpdate, and are planned again first.
- **Deploys:** Task 33 deploys BlockRender 0.12.0, and Task 36 0.13.0, each with the game closed and `CP2077_DIR`
  cleared straight after.

## File map (Part 2)

All paths under `src/`, `tests/` and `tools/` are relative to `spikes/block-render/`.

| File | Responsibility |
|---|---|
| `CMakeLists.txt` | The new sources and tests, PhysX's headers, and versions 0.12.0 and 0.13.0 |
| `src/Plugin.cpp` | The turn by default, V's reason and placeability, `scenewatch`, the city's reads, `set ground`, `set exportbudget`, `shapes` |
| `src/input/BuildMode.*`, `src/input/Router.*` | V's reason before "V isn't found" |
| `src/link/Drive.*`, `src/link/Link.*` | V's eye height only while V can be placed; the city's ground, its epochs and its messages |
| `src/link/Floor.*` | The floor's voxels made once a height |
| `src/link/Voxels.*` | The triangles message, beside the voxel one |
| `src/link/Ground.*` | Regions, batches, filing, hashes, `GroundSource`, and `GroundAreas`: what to read next |
| `src/link/GroundWorker.*` | The worker thread: voxels and messages for each region |
| `src/physx/PxHeaders.h`, `GameAbi.*`, `ShapeTriangles.*`, `OriginWatch.*` | Spike 1's reader, moved in |
| `src/physx/SceneWatch.*` | The scene watch's record and summary |
| `src/physx/Session.*` | The audited build, and scene 0 |
| `src/physx/SceneHooks.*` | The scene watch's 16 hooks |
| `src/physx/Export.*` | The exporter: scene 0's kept shapes as Minecraft's triangles, and `shapes` |
| `src/Requests.*`, `src/Status.*` | `scenewatch`, `set ground`, `shapes`; the scene watch's and the ground's lines |
| `tools/mclink/DumpObj.*`, `Main.cpp`, `McLinkScript.*`, `city.txt` | The dump loader, `ground`, `dump` and `wait-ground`, and step 3's script |
| `tools/blockrender_hud/init.lua` | The ground's line and buttons |
| `tests/TestGround.cpp`, `TestGroundWorker.cpp`, `TestDumpObj.cpp`, `TestSceneWatch.cpp`, `TestPhysxSession.cpp`, `TestExport.cpp` | One test file per new unit; spike 1's tests move in with its reader; the others gain tests |
| `docs/spikes/2026-10-01-physx-abi-audit.md` | The addendum: the scene watch's slots |

---

### Task 19: Build mode turns with the mouse by default

Step 2 found that with `set turn 0` the teleport puts V's yaw back each frame, so V can't turn left or right in build
mode, and that `set turn 1` made turning smooth. It becomes the default; `set turn 0` stays, to compare. Until the
ratio is measured, five turns of the mouse outside build mode, the turn adds nothing, as before; the drive line says so.

**Files:**
- Modify: `src/Plugin.cpp`

- [x] **Step 1: Edit `src/Plugin.cpp`**

```cpp
bool                  g_turn = false;     // set turn 1: each teleport's yaw gains the frame's turn from the mouse
```
```cpp
bool                  g_turn = true;      // set turn 1, the default since step 2: the teleport's yaw gains the mouse's turn
```

- [x] **Step 2: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `289 test(s), 0 failure(s)`.

- [x] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: build mode turns with the mouse by default" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 20: Build mode says why V can't be placed before it says V isn't found

In a car, F8 said "V isn't found" rather than "V is in a vehicle": the car's camera isn't V's eye, so `PlayerLocator`
finds no camera, the link finds no V, and that came first. `Puppet` finds V itself, so the plugin now asks it why V
can't be placed whenever it's linked, camera or not, and build mode and the router say that reason before "V isn't
found".

**Files:**
- Modify: `src/input/BuildMode.h`, `src/input/BuildMode.cpp`, `src/input/Router.h`, `src/input/Router.cpp`,
  `src/Plugin.cpp`, `tests/TestBuildMode.cpp`, `tests/TestRouter.cpp`

- [x] **Step 1: Edit `tests/TestBuildMode.cpp`: the failing test**

```cpp
	CHECK(mode.TurnOn(Ready()));
```
```cpp
	CHECK(mode.TurnOn(Ready()));
}

TEST(BuildModeSaysWhyVCantBePlacedBeforeVIsntFound)
{
	// In a car the camera isn't V's eye, so the link finds no V, but the plugin still knows why V can't be placed.
	BuildMode mode;
	Situation situation = Ready();
	situation.inGame = false;
	situation.blocked = "V is in a vehicle";
	CHECK(!mode.TurnOn(situation));
	CHECK_STR(mode.Why(), "V is in a vehicle");
	situation.blocked.clear();
	CHECK(!mode.TurnOn(situation));
	CHECK_STR(mode.Why(), "V isn't found");
	situation.linked = false;
	CHECK(!mode.TurnOn(situation));
	CHECK_STR(mode.Why(), "it isn't linked");
```

- [x] **Step 2: Edit `tests/TestRouter.cpp`: the failing test**

```cpp
TEST(RouterEndsBuildModeWhenMinecraftsPlayerDies)
```
```cpp
TEST(RouterSaysWhyVCantBePlacedWhenTheLinkFindsNoV)
{
	// In a car the camera isn't V's eye, so the link finds no V; V's own reason still comes first.
	Building           building;
	link::Link::Status state = Linked();
	state.inGame = false;
	building.router.Update(true, state, "V is in a vehicle");
	CHECK(!building.router.On());
	CHECK_STR(building.router.Report().why, "V is in a vehicle");
	CHECK_STR(building.router.TurnOn().value_or("on"), "V is in a vehicle");
	building.router.Update(true, state);
	CHECK_STR(building.router.TurnOn().value_or("on"), "V isn't found");
}

TEST(RouterEndsBuildModeWhenMinecraftsPlayerDies)
```

- [x] **Step 3: Build and run the tests; the new ones must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | grep -E "^FAIL"
```
Expected: `FAIL BuildModeSaysWhyVCantBePlacedBeforeVIsntFound` and `FAIL RouterSaysWhyVCantBePlacedWhenTheLinkFindsNoV`.

- [x] **Step 4: Edit `src/input/BuildMode.h`**

```cpp
	std::string blocked;  // why Minecraft can't drive V: V in a vehicle, a workspot or dead, or Minecraft's player dead (spike 5)
```
```cpp
	std::string blocked;  // why Minecraft can't drive V: V in a vehicle, a workspot or dead, or Minecraft's player dead (spike 5).
	                      // Known even when the link finds no V, as in a car, where the camera isn't V's eye
```

- [x] **Step 5: Edit `src/input/BuildMode.cpp`**

```cpp
	else if (!aSituation.inGame)
	{
		why_ = aSituation.linked ? "V isn't found" : "it isn't linked";
	}
	else if (!aSituation.blocked.empty())
	{
		why_ = aSituation.blocked;
```
```cpp
	else if (!aSituation.linked)
	{
		why_ = "it isn't linked";
	}
	else if (!aSituation.blocked.empty())
	{
		why_ = aSituation.blocked;  // first: in a car the camera isn't V's eye, so the link finds no V
	}
	else if (!aSituation.inGame)
	{
		why_ = "V isn't found";
```

- [x] **Step 6: Edit `src/input/Router.h`**

```cpp
	// it can't (spike 5). Build mode ends if Minecraft or V is gone, or the link, or V can't be placed, or Minecraft's
	// player dies.
```
```cpp
	// it can't (spike 5), even when the link finds no V. Build mode ends if Minecraft or V is gone, or the link, or V
	// can't be placed, or Minecraft's player dies; why V can't be placed is said before "V isn't found".
```

- [x] **Step 7: Edit `src/input/Router.cpp`**

```cpp
		std::string why = !aState.minecraft ? "Minecraft has gone"
		                  : !aState.inWorld ? "Minecraft left its world"
		                  : !aLinked        ? "unlinked"
		                  : !aState.inGame  ? "V isn't found"
		                                    : situation_.blocked;
```
```cpp
		std::string why = !aState.minecraft             ? "Minecraft has gone"
		                  : !aState.inWorld             ? "Minecraft left its world"
		                  : !aLinked                    ? "unlinked"
		                  : !situation_.blocked.empty() ? situation_.blocked
		                  : !aState.inGame              ? "V isn't found"
		                                                : std::string();
```

- [x] **Step 8: Edit `src/Plugin.cpp`**

```cpp
	// While linked: V's yaw, and what keeps V from being placed. The turn ratio is measured outside build mode, while V
	// can be placed, so its yaw is V's own doing.
	const std::optional<double> yaw = g_linked && pose ? g_puppet.Yaw() : std::nullopt;
	const std::string           blocked = g_linked && pose ? g_puppet.Blocked() : std::string();
```
```cpp
	// While linked: V's yaw, and what keeps V from being placed. Puppet finds V itself, so the reason is known without
	// the camera too: in a car the camera isn't V's eye, and the link finds no V. The turn ratio is measured outside
	// build mode, while V can be placed, so its yaw is V's own doing.
	const std::optional<double> yaw = g_linked && pose ? g_puppet.Yaw() : std::nullopt;
	const std::string           blocked = g_linked ? g_puppet.Blocked() : std::string();
```

- [x] **Step 9: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `291 test(s), 0 failure(s)`.

- [x] **Step 10: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: build mode says why V can't be placed before saying V isn't found" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 21: V's eye height is measured only while V can be placed

In a car's seat the camera is 1.18 m above V's feet, and the seat is level, so step 2's eye height learned 1.18 m.
Getting out and pressing F8 at once would have placed V by it until the average caught up. The link's frame now says
whether V can be placed (`Frame::placeable`, from `Puppet`'s reason), and V's eye height is measured only then. A frame
that isn't measured breaks the run (`EyeHeight::Break`), so the first frame after it only notes V's feet.

**Files:**
- Modify: `src/link/Drive.h`, `src/link/Drive.cpp`, `src/link/Link.h`, `src/link/Link.cpp`, `src/Plugin.cpp`,
  `tests/TestDrive.cpp`, `tests/TestLink.cpp`

- [x] **Step 1: Edit `tests/TestDrive.cpp`: the failing test**

```cpp
	CHECK_NEAR(eye.Value(), 1.61, 1e-9);
}
```
```cpp
	CHECK_NEAR(eye.Value(), 1.61, 1e-9);
}

TEST(EyeHeightStartsAFreshRunAfterABreak)
{
	// A frame V can't be measured in, a car's seat say, breaks the run: the next frame only notes the feet.
	EyeHeight eye;
	eye.Note({ 0.0, 0.0, 6.0 }, { 0.0, 0.0, 7.7 });
	eye.Break();
	eye.Note({ 0.0, 0.0, 6.0 }, { 0.0, 0.0, 7.7 });
	CHECK_NEAR(eye.Value(), 1.60, 1e-9);
	eye.Note({ 0.0, 0.0, 6.0 }, { 0.0, 0.0, 7.7 });
	CHECK_NEAR(eye.Value(), 1.70, 1e-9);
}
```

- [x] **Step 2: Edit `tests/TestLink.cpp`: the failing test**

```cpp
	CHECK_NEAR(link.State().eyeHeight, 1.70, 1e-9);
}
```
```cpp
	CHECK_NEAR(link.State().eyeHeight, 1.70, 1e-9);
}

TEST(LinkMeasuresVsEyeHeightOnlyWhileVCanBePlaced)
{
	// In a car's seat, V's camera is 1.18 m above its feet (spike 5's step 2): that isn't V's eye height.
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
	Sink          sink;
	GamePose      pose = Corner();
	link.Update(At(1.0, pose), kSettings, sink);  // getting into the car: V's feet are noted
	pose.camera.z = 6.14 + 1.18;
	Link::Frame seated = At(1.1, pose);
	seated.placeable = false;
	link.Update(seated, kSettings, sink);
	seated.now = 1.2;
	link.Update(seated, kSettings, sink);
	CHECK_NEAR(link.State().eyeHeight, 1.60, 1e-9);
	// Out of the car and standing, at the same height: the first frame only notes V's feet, the second measures.
	pose.camera.z = 6.14 + 1.70;
	link.Update(At(1.3, pose), kSettings, sink);
	CHECK_NEAR(link.State().eyeHeight, 1.60, 1e-9);
	link.Update(At(1.4, pose), kSettings, sink);
	CHECK_NEAR(link.State().eyeHeight, 1.70, 1e-9);
}
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'Break': is not a member of 'cpmc::link::EyeHeight'`.

- [x] **Step 4: Edit `src/link/Drive.h`**

```cpp
// V can't crouch in build mode, so it holds while Minecraft drives. Spike 4's measure, 1.60 m, until a frame is taken.
```
```cpp
// V can't crouch in build mode, so it holds while Minecraft drives. It's measured only while V can be placed, too: in a
// car's seat the camera is 1.18 m above V's feet. Spike 4's measure, 1.60 m, until a frame is taken.
```
```cpp
	void   Note(const Vec3d& aFeet, const Vec3d& aCamera);  // world axes
```
```cpp
	void   Note(const Vec3d& aFeet, const Vec3d& aCamera);  // world axes
	void   Break();  // a frame V isn't measured in: the next frame's feet have nothing to be compared with
```

- [x] **Step 5: Edit `src/link/Drive.cpp`**

```cpp
	lastFeetZ_ = aFeet.z;
```
```cpp
	lastFeetZ_ = aFeet.z;
}

void EyeHeight::Break()
{
	lastFeetZ_.reset();
```

- [x] **Step 6: Edit `src/link/Link.h`**

```cpp
		bool                    buildMode = false;  // Minecraft drives (spike 5); off sets SkyState's menu flag
```
```cpp
		bool                    buildMode = false;  // Minecraft drives (spike 5); off sets SkyState's menu flag
		bool                    placeable = true;   // nothing keeps V from being placed (a vehicle, a workspot, death),
		                                            // so V's eye height can be measured
```

- [x] **Step 7: Edit `src/link/Link.cpp`**

```cpp
	if (aFrame.pose && !build)
	{
		eyeHeight_.Note(aFrame.pose->feet, aFrame.pose->camera);
```
```cpp
	if (aFrame.pose && !build && aFrame.placeable)
	{
		eyeHeight_.Note(aFrame.pose->feet, aFrame.pose->camera);
	}
	else
	{
		eyeHeight_.Break();
```

- [x] **Step 8: Edit `src/Plugin.cpp`**

```cpp
	frame.buildMode = buildMode;
```
```cpp
	frame.buildMode = buildMode;
	frame.placeable = blocked.empty();
```

- [x] **Step 9: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `293 test(s), 0 failure(s)`.

- [x] **Step 10: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: V's eye height is measured only while V can be placed" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 22: The floor's voxels are made once a height, not once a region

Entering build mode gave one frame of 53 to 56 ms in step 2. It's the fresh floor: `FloorVoxels` voxelized each of its
49 regions, about 45 ms in all, and every time the floor grows by 16 m in build mode it did so again. Each region's two
triangles are the same relative to its corner, so its voxels are the same too, moved by the region: `FloorVoxeler`
voxelizes one region a height, and moves its blocks to each, in about 1.3 ms in all. `FloorVoxelsAreEachRegionsOwn`
checks that they're byte for byte what voxelizing each region's own triangles gives.

**Files:**
- Modify: `src/link/Floor.h`, `src/link/Floor.cpp`, `src/link/Link.cpp`, `tests/TestFloor.cpp`, `tests/TestLink.cpp`

- [x] **Step 1: Edit `tests/TestFloor.cpp`: the failing test**

```cpp
	const auto payloads = FloorVoxels(-15, 250, 6.14, 3);
```
```cpp
	const auto payloads = FloorVoxeler(6.14).Column(-15, 250, 3);
```
```cpp
	CHECK(block.bits[0] == 0 && block.bits[1] == ~0ull && block.bits[2] == 0);
}
```
```cpp
	CHECK(block.bits[0] == 0 && block.bits[1] == ~0ull && block.bits[2] == 0);
}

TEST(FloorVoxelsAreEachRegionsOwn)
{
	// Made once a height and shifted to each region, they're byte for byte what each region's own two triangles give.
	for (const double y : { 6.14, 7.0, -0.5, -12.375, 64.99 })
	{
		const FloorVoxeler voxeler(y);
		for (const auto& [x, z] : { Region(-15, 250), Region(0, 0), Region(-1, -1), Region(1250, -900), Region(-100000, 100000) })
		{
			const std::vector<uint8_t> tris = FloorRegion(x, z, y, 5);
			proto::ColRegion           region;
			std::memcpy(&region, tris.data(), sizeof region);
			std::vector<McTriangle> triangles(region.count);
			for (uint32_t i = 0; i < region.count; ++i)
			{
				proto::ColTri tri;
				std::memcpy(&tri, tris.data() + sizeof region + i * sizeof tri, sizeof tri);
				std::memcpy(triangles[i].v, tri.v, sizeof tri.v);
			}
			const int32_t regionY = region.minY / kRegionBlocks;
			CHECK(voxeler.Column(x, z, 5)[0] == RegionPayload(x, regionY, z, 5, VoxelizeRegion(x, regionY, z, triangles)));
		}
	}
}
```

- [x] **Step 2: Edit `tests/TestLink.cpp`**

```cpp
	CHECK(payloads[2] == FloorVoxels(-18, 247, 6.14, 1)[0]);  // the first region's, the floor's own
```
```cpp
	CHECK(payloads[2] == FloorVoxeler(6.14).Column(-18, 247, 1)[0]);  // the first region's, the floor's own
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C3861: 'FloorVoxeler': identifier not found`.

- [x] **Step 4: Edit `src/link/Floor.h`**

```cpp
// The voxel messages (kColRegion payloads) for one column of the floor: the region holding the floor, voxelized from
// FloorRegion's two triangles, then the regions just below and above it, empty. All three count as known, which
// SkyCraft's hold wants of the feet's region, the one holding the block under them, and the one 8 blocks down.
std::vector<std::vector<uint8_t>> FloorVoxels(int32_t aRegionX, int32_t aRegionZ, double aY, uint32_t aEpoch);
```
```cpp
// The floor's voxels at height aY, made once for all its regions: each region's two triangles are the same relative to
// its corner, so its voxels are too, moved by the region. Voxelizing each of the 49 regions instead made entering build
// mode take one frame of about 55 ms (spike 5's step 2).
class FloorVoxeler
{
public:
	explicit FloorVoxeler(double aY);

	// The voxel messages (kColRegion payloads) for one column of the floor: the region holding the floor, voxelized from
	// FloorRegion's two triangles, then the regions just below and above it, empty. All three count as known, which
	// SkyCraft's hold wants of the feet's region, the one holding the block under them, and the one 8 blocks down.
	std::vector<std::vector<uint8_t>> Column(int32_t aRegionX, int32_t aRegionZ, uint32_t aEpoch) const;

private:
	int32_t                      regionY_;
	std::vector<proto::ColBlock> blocks_;  // region (0, regionY_, 0)'s
};
```

- [x] **Step 5: Edit `src/link/Floor.cpp`**

```cpp
std::vector<std::vector<uint8_t>> FloorVoxels(int32_t aRegionX, int32_t aRegionZ, double aY, uint32_t aEpoch)
{
	const int32_t                      regionY = RegionOf(aY);
	const std::array<McTriangle, 2>    triangles = Triangles(aRegionX, aRegionZ, aY);
	const std::vector<proto::ColBlock> blocks = VoxelizeRegion(aRegionX, regionY, aRegionZ, triangles);
	return { RegionPayload(aRegionX, regionY, aRegionZ, aEpoch, blocks), RegionPayload(aRegionX, regionY - 1, aRegionZ, aEpoch, {}),
		     RegionPayload(aRegionX, regionY + 1, aRegionZ, aEpoch, {}) };
```
```cpp
FloorVoxeler::FloorVoxeler(double aY) : regionY_(RegionOf(aY))
{
	const std::array<McTriangle, 2> triangles = Triangles(0, 0, aY);
	blocks_ = VoxelizeRegion(0, regionY_, 0, triangles);
}

std::vector<std::vector<uint8_t>> FloorVoxeler::Column(int32_t aRegionX, int32_t aRegionZ, uint32_t aEpoch) const
{
	std::vector<proto::ColBlock> blocks = blocks_;
	for (proto::ColBlock& block : blocks)
	{
		block.x += aRegionX * kRegionBlocks;
		block.z += aRegionZ * kRegionBlocks;
	}
	return { RegionPayload(aRegionX, regionY_, aRegionZ, aEpoch, blocks), RegionPayload(aRegionX, regionY_ - 1, aRegionZ, aEpoch, {}),
		     RegionPayload(aRegionX, regionY_ + 1, aRegionZ, aEpoch, {}) };
```

- [x] **Step 6: Edit `src/link/Link.cpp`**

```cpp
	bool           sent = !aNewEpoch || WriteByteRing(ring, proto::kColRingDataBytes, proto::kColClear, FloorClear(epoch));
```
```cpp
	bool           sent = !aNewEpoch || WriteByteRing(ring, proto::kColRingDataBytes, proto::kColClear, FloorClear(epoch));
	const FloorVoxeler voxeler(aCentre.y);
```
```cpp
		for (const std::vector<uint8_t>& voxels : FloorVoxels(x, z, aCentre.y, epoch))
```
```cpp
		for (const std::vector<uint8_t>& voxels : voxeler.Column(x, z, epoch))
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `294 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the floor's voxels are made once a height, not once a region" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 23: The city's regions, read in batches and filed, and the triangles message

The first of the city's pure parts (the spec's "The exporter"), in a new `link/Ground`:
- **Regions and batches:** a region is SkyCraft's 8-block cube; a batch is 2 × 2 × 2 regions, read together, so a large
  shape's triangles are culled once for eight regions. `BatchBox` is a batch's blocks a quarter block more each way, as
  SkyCraft's harvest reads around a region. `RegionAt` refuses a point that isn't finite or is over a million blocks
  out, so garbage never becomes a region.
- **Filing:** `FileBatch` puts each triangle under every region of the batch whose blocks, half a block more each way,
  its bounds touch, as SkyCraft's `SendTriangles` keeps them, and drops triangles with a coordinate that isn't finite or
  is over a million blocks out, as SkyCraft does.
- **The hash:** `TrianglesHash` is a sum of each triangle's hash, so a read that lists the same shapes in another order
  gives the same hash.
- **The triangles message:** `TrisPayload`, beside `RegionPayload` in `link/Voxels`; the floor's `FloorRegion` now uses
  it, and `FloorMatchesFakeSkyrim` checks its bytes stay SkyCraft's.

**Files:**
- Create: `src/link/Ground.h`, `src/link/Ground.cpp`, `tests/TestGround.cpp`
- Modify: `src/link/Voxels.h`, `src/link/Voxels.cpp`, `src/link/Floor.cpp`, `tests/TestVoxels.cpp`, `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestGround.cpp`**

```cpp
#include "Harness.h"

#include "link/Ground.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

using namespace cpmc;
using namespace cpmc::link;

namespace
{
McTriangle Tri(float aX0, float aY0, float aZ0, float aX1, float aY1, float aZ1, float aX2, float aY2, float aZ2)
{
	return { { aX0, aY0, aZ0, aX1, aY1, aZ1, aX2, aY2, aZ2 } };
}

// A small flat triangle a quarter block across, around (aX, aY, aZ).
McTriangle Speck(float aX, float aY, float aZ)
{
	return Tri(aX - 0.125f, aY, aZ - 0.125f, aX + 0.125f, aY, aZ - 0.125f, aX, aY, aZ + 0.125f);
}

bool Same(const McTriangle& aA, const McTriangle& aB)
{
	return std::equal(std::begin(aA.v), std::end(aA.v), std::begin(aB.v));
}

RegionPos Region(int32_t aX, int32_t aY, int32_t aZ)
{
	return { aX, aY, aZ };
}

BatchPos Batch(int32_t aX, int32_t aY, int32_t aZ)
{
	return { aX, aY, aZ };
}
}

TEST(GroundFindsRegionsAndBatches)
{
	// Spike 3's corner, Minecraft -119.5 6.14 2007.5, is in region -15 0 250, of batch -8 0 125.
	CHECK(RegionAt({ -119.5, 6.14, 2007.5 }) == Region(-15, 0, 250));
	CHECK(RegionAt({ -0.01, -0.01, 8.0 }) == Region(-1, -1, 1));
	CHECK(!RegionAt({ std::nan(""), 0.0, 0.0 }));
	CHECK(!RegionAt({ 0.0, 2.0e6, 0.0 }));
	CHECK(BatchOf({ -15, 0, 250 }) == Batch(-8, 0, 125));
	CHECK(BatchOf({ -16, -1, 251 }) == Batch(-8, -1, 125));
	const std::array<RegionPos, 8> regions = RegionsOf({ -8, 0, 125 });
	CHECK(regions[0] == Region(-16, 0, 250));
	CHECK(regions[1] == Region(-15, 0, 250));  // x first
	CHECK(regions[2] == Region(-16, 0, 251));  // then z
	CHECK(regions[4] == Region(-16, 1, 250));  // then y
	CHECK(regions[7] == Region(-15, 1, 251));
	const McBox box = BatchBox({ -8, 0, 125 });  // a quarter block more each way
	CHECK_NEAR(box.low.x, -128.25, 1e-12);
	CHECK_NEAR(box.high.x, -111.75, 1e-12);
	CHECK_NEAR(box.low.y, -0.25, 1e-12);
	CHECK_NEAR(box.high.y, 16.25, 1e-12);
	CHECK_NEAR(box.low.z, 1999.75, 1e-12);
	CHECK_NEAR(box.high.z, 2016.25, 1e-12);
}

TEST(GroundFilesATriangleUnderEveryRegionItTouches)
{
	// Batch 0 0 0 is blocks 0 to 16 each way. Its regions, in order: 000, 100, 001, 101, 010, 110, 011, 111.
	const std::vector<McTriangle> triangles = {
		Speck(4.0f, 4.0f, 4.0f),                                     // inside region 000
		Tri(7.0f, 4.0f, 4.0f, 9.0f, 4.0f, 4.0f, 8.0f, 4.0f, 5.0f),  // across x = 8: regions 000 and 100
		Speck(8.3f, 12.0f, 12.0f),                                   // in region 111, and within half a block of 011
		Speck(16.5f, 12.0f, 12.0f),                                  // past the batch, within half a block of 111
		Speck(17.2f, 12.0f, 12.0f),                                  // farther: in none
	};
	const auto filed = FileBatch({ 0, 0, 0 }, triangles);
	CHECK(filed[0].size() == 2 && Same(filed[0][0], triangles[0]) && Same(filed[0][1], triangles[1]));
	CHECK(filed[1].size() == 1 && Same(filed[1][0], triangles[1]));
	CHECK(filed[6].size() == 1 && Same(filed[6][0], triangles[2]));
	CHECK(filed[7].size() == 2 && Same(filed[7][0], triangles[2]) && Same(filed[7][1], triangles[3]));
	for (const size_t i : { 2, 3, 4, 5 })
	{
		CHECK(filed[i].empty());
	}
}

TEST(GroundLeavesOutTrianglesThatArentUsable)
{
	McTriangle broken = Speck(4.0f, 4.0f, 4.0f);
	broken.v[4] = std::numeric_limits<float>::quiet_NaN();
	McTriangle distant = Speck(4.0f, 4.0f, 4.0f);
	distant.v[0] = 2.0e6f;  // its bounds still touch region 000
	const auto filed = FileBatch({ 0, 0, 0 }, std::vector<McTriangle>{ broken, distant, Speck(4.0f, 4.0f, 4.0f) });
	CHECK_EQ(filed[0].size(), size_t(1));
}

TEST(GroundHashesTrianglesWhateverTheirOrder)
{
	const std::vector<McTriangle> triangles = { Speck(1.0f, 2.0f, 3.0f), Speck(4.0f, 5.0f, 6.0f), Speck(7.0f, 8.0f, 9.0f) };
	const std::vector<McTriangle> reordered = { triangles[2], triangles[0], triangles[1] };
	CHECK_EQ(TrianglesHash(triangles), TrianglesHash(reordered));
	std::vector<McTriangle> moved = triangles;
	moved[1].v[3] += 0.01f;
	CHECK(TrianglesHash(moved) != TrianglesHash(triangles));
	CHECK(TrianglesHash(std::vector<McTriangle>{ triangles[0], triangles[0] }) != TrianglesHash(std::vector<McTriangle>{ triangles[0] }));
	CHECK(TrianglesHash({}) != TrianglesHash(std::vector<McTriangle>{ triangles[0] }));
}
```
- [x] **Step 2: Edit `tests/TestVoxels.cpp`: the failing test**

```cpp
	CHECK(header.minY == -8 && header.maxY == -1 && header.count == 0);
}
```
```cpp
	CHECK(header.minY == -8 && header.maxY == -1 && header.count == 0);
}

TEST(VoxelsTrisPayload)
{
	const std::vector<McTriangle> triangles = { Tri(-120.0f, 6.14f, 2000.0f, -112.0f, 6.14f, 2000.0f, -112.0f, 6.14f, 2008.0f) };
	const auto                    payload = TrisPayload(-15, 0, 250, 9, triangles);
	CHECK_EQ(payload.size(), sizeof(proto::ColRegion) + sizeof(proto::ColTri));
	proto::ColRegion header;
	std::memcpy(&header, payload.data(), sizeof header);
	CHECK(header.minX == -120 && header.minY == 0 && header.minZ == 2000);
	CHECK(header.maxX == -113 && header.maxY == 7 && header.maxZ == 2007);
	CHECK_EQ(header.epoch, 9u);
	CHECK_EQ(header.count, 1u);
	proto::ColTri tri;
	std::memcpy(&tri, payload.data() + sizeof header, sizeof tri);
	CHECK(std::memcmp(tri.v, triangles[0].v, sizeof tri.v) == 0);
	CHECK_EQ(tri.flags, 0u);  // not diggable
	CHECK_EQ(TrisPayload(-15, 0, 250, 9, {}).size(), sizeof(proto::ColRegion));
}
```

- [x] **Step 3: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestVoxels.cpp
	tests/TestDrive.cpp
```
```cmake
	tests/TestVoxels.cpp
	tests/TestGround.cpp
	tests/TestDrive.cpp
```

- [x] **Step 4: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Ground.h'`.

- [x] **Step 5: Edit `src/link/Voxels.h`**

```cpp
                                   std::span<const proto::ColBlock> aBlocks);
```
```cpp
                                   std::span<const proto::ColBlock> aBlocks);

// A triangles message's payload (kColTris): the region's inclusive block box, the epoch, the count and the triangles,
// each with flags 0: not diggable, so attacking them does nothing. Minecraft files them under the box's corner region.
std::vector<uint8_t> TrisPayload(int32_t aRegionX, int32_t aRegionY, int32_t aRegionZ, uint32_t aEpoch,
                                 std::span<const McTriangle> aTriangles);
```

- [x] **Step 6: Edit `src/link/Voxels.cpp`**

```cpp
	return true;
```
```cpp
	return true;
}

// A region message's header: the region's inclusive block box, the epoch and the count.
proto::ColRegion Header(int32_t aRegionX, int32_t aRegionY, int32_t aRegionZ, uint32_t aEpoch, size_t aCount)
{
	return { aRegionX * kRegionBlocks,
		     aRegionY * kRegionBlocks,
		     aRegionZ * kRegionBlocks,
		     aRegionX * kRegionBlocks + kRegionBlocks - 1,
		     aRegionY * kRegionBlocks + kRegionBlocks - 1,
		     aRegionZ * kRegionBlocks + kRegionBlocks - 1,
		     aEpoch,
		     uint32_t(aCount) };
```
```cpp
	const proto::ColRegion header{ aRegionX * kRegionBlocks,
		                           aRegionY * kRegionBlocks,
		                           aRegionZ * kRegionBlocks,
		                           aRegionX * kRegionBlocks + kRegionBlocks - 1,
		                           aRegionY * kRegionBlocks + kRegionBlocks - 1,
		                           aRegionZ * kRegionBlocks + kRegionBlocks - 1,
		                           aEpoch,
		                           uint32_t(aBlocks.size()) };
	std::vector<uint8_t> payload(sizeof header + aBlocks.size_bytes());
```
```cpp
	const proto::ColRegion header = Header(aRegionX, aRegionY, aRegionZ, aEpoch, aBlocks.size());
	std::vector<uint8_t>   payload(sizeof header + aBlocks.size_bytes());
```
```cpp
	return payload;
}
}
```
```cpp
	return payload;
}

std::vector<uint8_t> TrisPayload(int32_t aRegionX, int32_t aRegionY, int32_t aRegionZ, uint32_t aEpoch,
                                 std::span<const McTriangle> aTriangles)
{
	const proto::ColRegion header = Header(aRegionX, aRegionY, aRegionZ, aEpoch, aTriangles.size());
	std::vector<uint8_t>   payload(sizeof header + aTriangles.size() * sizeof(proto::ColTri));
	std::memcpy(payload.data(), &header, sizeof header);
	for (size_t i = 0; i < aTriangles.size(); ++i)
	{
		proto::ColTri tri{};
		std::memcpy(tri.v, aTriangles[i].v, sizeof tri.v);
		std::memcpy(payload.data() + sizeof header + i * sizeof tri, &tri, sizeof tri);
	}
	return payload;
}
}
```

- [x] **Step 7: Edit `src/link/Floor.cpp`**

```cpp
#include <cstring>
```
```cpp
```
```cpp
	const int32_t     regionY = RegionOf(aY);
	proto::ColRegion region{ aRegionX * kRegionBlocks, regionY * kRegionBlocks, aRegionZ * kRegionBlocks,
		                     aRegionX * kRegionBlocks + kRegionBlocks - 1, regionY * kRegionBlocks + kRegionBlocks - 1,
		                     aRegionZ * kRegionBlocks + kRegionBlocks - 1, aEpoch, 2 };
	std::vector<uint8_t> bytes;
	Append(bytes, region);
	for (const McTriangle& triangle : Triangles(aRegionX, aRegionZ, aY))
	{
		proto::ColTri tri{};  // flags 0: not diggable, so attacking the floor does nothing
		std::memcpy(tri.v, triangle.v, sizeof tri.v);
		Append(bytes, tri);
	}
	return bytes;
```
```cpp
	return TrisPayload(aRegionX, RegionOf(aY), aRegionZ, aEpoch, Triangles(aRegionX, aRegionZ, aY));
```

- [x] **Step 8: Write `src/link/Ground.h`**

```cpp
#pragma once

#include "Coords.h"
#include "link/Voxels.h"

#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

// Night City's shape in Minecraft (spike 5's "The exporter"), the parts that don't touch the game: SkyCraft's area of
// regions around a centre, read 2 x 2 x 2 regions at a time, nearest first; each triangle filed under the regions it
// touches; and a region sent again only when its triangles change. Pure: the game reads PhysX for the triangles, and
// mclink a saved dump.
namespace cpmc::link
{
// A collision region: the 8-block cube from (x, y, z) * 8, in Minecraft's blocks.
struct RegionPos
{
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;

	auto operator<=>(const RegionPos&) const = default;
};

// The region holding a point, Minecraft axes. Nothing for a point that isn't finite, or is over a million blocks out.
std::optional<RegionPos> RegionAt(const Vec3d& aMc);

// A batch: the 2 x 2 x 2 regions from (2x, 2y, 2z) to (2x + 1, 2y + 1, 2z + 1), read together, so a large shape's
// triangles are culled once for eight regions.
struct BatchPos
{
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;

	auto operator<=>(const BatchPos&) const = default;
};

BatchPos BatchOf(const RegionPos& aRegion);

// The batch's eight regions, in order of y, then z, then x.
std::array<RegionPos, 8> RegionsOf(const BatchPos& aBatch);

// A box in Minecraft's blocks.
struct McBox
{
	Vec3d low;
	Vec3d high;
};

constexpr double kReadMargin = 0.25;  // blocks read around a batch, as SkyCraft's harvest reads around a region
constexpr double kFileMargin = 0.5;   // a triangle goes to every region it touches within this, as SkyCraft sends them

// The batch's blocks, aMargin more each way.
McBox BatchBox(const BatchPos& aBatch, double aMargin = kReadMargin);

// The batch's triangles by region, in RegionsOf's order: each triangle under every region whose blocks, kFileMargin more
// each way, its bounds touch. A triangle with a coordinate that isn't finite, or is over a million blocks out, is left
// out, as SkyCraft leaves it out.
std::array<std::vector<McTriangle>, 8> FileBatch(const BatchPos& aBatch, std::span<const McTriangle> aTriangles);

// A hash of a region's triangles that doesn't depend on their order: a read can list the same shapes in another order.
uint64_t TrianglesHash(std::span<const McTriangle> aTriangles);
}
```
- [x] **Step 9: Write `src/link/Ground.cpp`**

```cpp
#include "link/Ground.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace cpmc::link
{
namespace
{
constexpr double kFarthest = 1.0e6;  // blocks: a coordinate farther out is garbage, as SkyCraft takes it

// aValue / aBy, rounded down.
int32_t FloorDiv(int32_t aValue, int32_t aBy)
{
	return aValue >= 0 ? aValue / aBy : -((-aValue + aBy - 1) / aBy);
}

bool Usable(const McTriangle& aTriangle)
{
	return std::all_of(std::begin(aTriangle.v), std::end(aTriangle.v),
	                   [](float aValue) { return std::isfinite(aValue) && std::fabs(aValue) <= kFarthest; });
}

uint64_t Fnv1a(const void* aData, size_t aBytes)
{
	uint64_t hash = 14695981039346656037ull;
	for (size_t i = 0; i < aBytes; ++i)
	{
		hash = (hash ^ static_cast<const uint8_t*>(aData)[i]) * 1099511628211ull;
	}
	return hash;
}
}

std::optional<RegionPos> RegionAt(const Vec3d& aMc)
{
	const double at[3] = { aMc.x, aMc.y, aMc.z };
	if (!std::all_of(std::begin(at), std::end(at), [](double aValue) { return std::isfinite(aValue) && std::fabs(aValue) <= kFarthest; }))
	{
		return std::nullopt;
	}
	return RegionPos{ int32_t(std::floor(aMc.x / kRegionBlocks)), int32_t(std::floor(aMc.y / kRegionBlocks)),
		              int32_t(std::floor(aMc.z / kRegionBlocks)) };
}

BatchPos BatchOf(const RegionPos& aRegion)
{
	return { FloorDiv(aRegion.x, 2), FloorDiv(aRegion.y, 2), FloorDiv(aRegion.z, 2) };
}

std::array<RegionPos, 8> RegionsOf(const BatchPos& aBatch)
{
	std::array<RegionPos, 8> regions;
	size_t                   i = 0;
	for (int32_t y = 0; y < 2; ++y)
	{
		for (int32_t z = 0; z < 2; ++z)
		{
			for (int32_t x = 0; x < 2; ++x)
			{
				regions[i++] = { aBatch.x * 2 + x, aBatch.y * 2 + y, aBatch.z * 2 + z };
			}
		}
	}
	return regions;
}

McBox BatchBox(const BatchPos& aBatch, double aMargin)
{
	constexpr double kSide = 2.0 * kRegionBlocks;
	const Vec3d      low{ aBatch.x * kSide - aMargin, aBatch.y * kSide - aMargin, aBatch.z * kSide - aMargin };
	return { low, { low.x + kSide + 2.0 * aMargin, low.y + kSide + 2.0 * aMargin, low.z + kSide + 2.0 * aMargin } };
}

std::array<std::vector<McTriangle>, 8> FileBatch(const BatchPos& aBatch, std::span<const McTriangle> aTriangles)
{
	const std::array<RegionPos, 8>         regions = RegionsOf(aBatch);
	std::array<std::vector<McTriangle>, 8> filed;
	for (const McTriangle& triangle : aTriangles)
	{
		if (!Usable(triangle))
		{
			continue;
		}
		double low[3], high[3];
		for (int k = 0; k < 3; ++k)
		{
			low[k] = std::min({ triangle.v[k], triangle.v[3 + k], triangle.v[6 + k] });
			high[k] = std::max({ triangle.v[k], triangle.v[3 + k], triangle.v[6 + k] });
		}
		for (size_t i = 0; i < regions.size(); ++i)
		{
			const double corner[3] = { double(regions[i].x) * kRegionBlocks, double(regions[i].y) * kRegionBlocks,
				                       double(regions[i].z) * kRegionBlocks };
			bool         touches = true;
			for (int k = 0; k < 3 && touches; ++k)
			{
				touches = high[k] >= corner[k] - kFileMargin && low[k] <= corner[k] + kRegionBlocks + kFileMargin;
			}
			if (touches)
			{
				filed[i].push_back(triangle);
			}
		}
	}
	return filed;
}

uint64_t TrianglesHash(std::span<const McTriangle> aTriangles)
{
	// A sum of each triangle's hash: the same whatever the order.
	uint64_t hash = aTriangles.size();
	for (const McTriangle& triangle : aTriangles)
	{
		hash += Fnv1a(triangle.v, sizeof triangle.v);
	}
	return hash;
}
}
```
- [x] **Step 10: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Voxels.cpp
	src/link/Drive.cpp
```
```cmake
	src/link/Voxels.cpp
	src/link/Ground.cpp
	src/link/Drive.cpp
```

- [x] **Step 11: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `299 test(s), 0 failure(s)`.

- [x] **Step 12: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the city's regions, read in batches and filed, and the triangles message" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 24: Which of the city's batches to read next, around a centre that moves

`GroundAreas` keeps SkyCraft's plan (`skse/src/Collision.cpp`'s `Update`), in batches:
- **The area:** regions 5 out each way across, 3 below and 2 above the centre's: 726 regions in 144 batches.
- **The order:** nearest first, each batch by its nearest region, with height counting double, as SkyCraft sorts its
  offsets.
- **Refreshing:** the batches holding the regions next to the centre (1 out across, from 1 below to level) are read
  again every second, since the city streams its collision in as V moves. A region goes to Minecraft again only when its
  triangles' hash has changed, and every region the first time a batch is read, empty ones too, so Minecraft knows them.
- **Forgetting:** when the centre moves, batches with no region within one more than the area are forgotten, so they're
  read and sent again on return.
- **Settling:** `Reset` forgets everything, and nothing is read before the time it's given, nor before an earlier
  reset's, so a second reason to start over never cuts SkyCraft's 1.5 s (`kSettle`) short.

**Files:**
- Modify: `src/link/Ground.h`, `src/link/Ground.cpp`, `tests/TestGround.cpp`

- [x] **Step 1: Edit `tests/TestGround.cpp`: the failing tests**

```cpp
#include <limits>
```
```cpp
#include <limits>
#include <set>
```
```cpp
{
	return { aX, aY, aZ };
}
}
```
```cpp
{
	return { aX, aY, aZ };
}

// SkyCraft's distance from aCentre to aBatch's nearest region: height counts double.
int Nearest(const BatchPos& aBatch, const RegionPos& aCentre)
{
	int nearest = std::numeric_limits<int>::max();
	for (const RegionPos& region : RegionsOf(aBatch))
	{
		const int dx = region.x - aCentre.x, dy = region.y - aCentre.y, dz = region.z - aCentre.z;
		nearest = std::min(nearest, dx * dx + dz * dz + 2 * dy * dy);
	}
	return nearest;
}

// Reads every batch the areas offer at aNow, empty, as a source with nothing in the area would give them.
std::vector<BatchPos> ReadAll(GroundAreas& aAreas, double aNow)
{
	std::vector<BatchPos> read;
	while (const std::optional<BatchPos> batch = aAreas.Next(aNow))
	{
		aAreas.Read(*batch, aNow, {});
		read.push_back(*batch);
		if (read.size() > 1000)
		{
			break;  // it never ends: the test fails on the count
		}
	}
	return read;
}
}
```
```cpp
	CHECK(TrianglesHash({}) != TrianglesHash(std::vector<McTriangle>{ triangles[0] }));
}
```
```cpp
	CHECK(TrianglesHash({}) != TrianglesHash(std::vector<McTriangle>{ triangles[0] }));
}

TEST(GroundAreasReadEveryBatchOfTheAreaNearestFirst)
{
	GroundAreas areas;
	CHECK(!areas.Next(0.0));  // no centre yet
	const RegionPos centre{ -15, 0, 250 };
	areas.Centre(centre);
	CHECK_EQ(areas.Waiting(), size_t(144));
	const std::vector<BatchPos> read = ReadAll(areas, 0.0);
	// x and z: 11 regions over 6 batches; y: 6 regions over 4.
	CHECK_EQ(read.size(), size_t(6 * 6 * 4));
	CHECK(read.front() == BatchOf(centre));
	std::set<RegionPos> regions;
	for (const BatchPos& batch : read)
	{
		const std::array<RegionPos, 8> its = RegionsOf(batch);
		regions.insert(its.begin(), its.end());
	}
	for (int32_t dy = -3; dy <= 2; ++dy)
	{
		for (int32_t dz = -5; dz <= 5; ++dz)
		{
			for (int32_t dx = -5; dx <= 5; ++dx)
			{
				CHECK(regions.contains({ centre.x + dx, centre.y + dy, centre.z + dz }));
			}
		}
	}
	int last = -1;
	for (const BatchPos& batch : read)
	{
		CHECK(Nearest(batch, centre) >= last);
		last = Nearest(batch, centre);
	}
	CHECK_EQ(areas.Waiting(), size_t(0));
	CHECK_EQ(areas.Known(), size_t(144 * 8));
}

TEST(GroundAreasWaitUntilTheyreReady)
{
	GroundAreas areas;
	areas.Centre({ 0, 0, 0 });
	CHECK(areas.Next(0.0).has_value());
	areas.Reset(11.5);  // the link started at 10: the city's collision gets SkyCraft's 1.5 s to stream in
	areas.Reset(10.2);  // and anything else that starts it over meanwhile doesn't cut that short
	CHECK(!areas.Next(11.4));
	CHECK(areas.Next(11.5) == BatchOf({ 0, 0, 0 }));
}

TEST(GroundAreasSendARegionAgainOnlyWhenItChanges)
{
	GroundAreas areas;
	areas.Centre({ 0, 0, 0 });
	std::array<std::vector<McTriangle>, 8> filed;
	filed[0] = { Speck(4.0f, 4.0f, 4.0f) };
	CHECK_EQ(areas.Read({ 0, 0, 0 }, 0.0, filed).size(), size_t(8));  // the first time: every region, empty ones too
	CHECK(areas.Read({ 0, 0, 0 }, 1.0, filed).empty());
	filed[3] = { Speck(12.0f, 4.0f, 12.0f) };
	CHECK(areas.Read({ 0, 0, 0 }, 2.0, filed) == std::vector<size_t>({ 3 }));
	filed[0].clear();
	CHECK(areas.Read({ 0, 0, 0 }, 3.0, filed) == std::vector<size_t>({ 0 }));
}

TEST(GroundAreasReadTheBatchesNextToTheCentreEverySecond)
{
	GroundAreas     areas;
	const RegionPos centre{ -15, 0, 250 };
	areas.Centre(centre);
	ReadAll(areas, 10.0);
	CHECK(!areas.Next(10.9));
	const std::vector<BatchPos> again = ReadAll(areas, 11.0);
	// The batches holding SkyCraft's regions next to the centre: 1 each way across, and from 1 below to level.
	std::set<BatchPos> next;
	for (int32_t dy = -1; dy <= 0; ++dy)
	{
		for (int32_t dz = -1; dz <= 1; ++dz)
		{
			for (int32_t dx = -1; dx <= 1; ++dx)
			{
				next.insert(BatchOf({ centre.x + dx, centre.y + dy, centre.z + dz }));
			}
		}
	}
	CHECK_EQ(again.size(), size_t(8));
	CHECK_EQ(next.size(), size_t(8));
	for (const BatchPos& batch : again)
	{
		CHECK(next.contains(batch));
	}
}

TEST(GroundAreasForgetWhatsFarBehind)
{
	GroundAreas areas;
	areas.Centre({ 0, 0, 0 });
	ReadAll(areas, 0.0);
	// Seven regions east: the batches from region -6 to -1 are more than one past the area, and forgotten.
	areas.Centre({ 7, 0, 0 });
	CHECK_EQ(areas.Known(), size_t((144 - 72) * 8));
	// Back again: they're read again, and each of their regions is sent again.
	areas.Centre({ 0, 0, 0 });
	CHECK_EQ(areas.Waiting(), size_t(72));
	const std::optional<BatchPos> first = areas.Next(0.5);
	CHECK(first.has_value() && first->x < 0);
	CHECK_EQ(areas.Read(*first, 0.5, {}).size(), size_t(8));
	const std::vector<BatchPos> rest = ReadAll(areas, 0.5);
	CHECK_EQ(rest.size(), size_t(71));
	CHECK(std::all_of(rest.begin(), rest.end(), [](const BatchPos& aBatch) { return aBatch.x < 0; }));
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2065: 'GroundAreas': undeclared identifier`.

- [x] **Step 3: Edit `src/link/Ground.h`**

```cpp
#include <cstdint>
```
```cpp
#include <cstdint>
#include <map>
```
```cpp
uint64_t TrianglesHash(std::span<const McTriangle> aTriangles);
```
```cpp
uint64_t TrianglesHash(std::span<const McTriangle> aTriangles);

// Which batches to read, around a centre that moves (the spec's "The area", "Refreshing" and "Settling"): SkyCraft's
// area, its order (nearest first, height counting double), its refresh of the regions next to the centre, and its
// forgetting of regions far away, so they're read again on return.
class GroundAreas
{
public:
	static constexpr int32_t kAcross = 5;     // regions out from the centre's, each way horizontally
	static constexpr int32_t kBelow = 3;      // regions below the centre's
	static constexpr int32_t kAbove = 2;      // and above
	static constexpr double  kRefresh = 1.0;  // seconds before a batch next to the centre is read again
	static constexpr double  kSettle = 1.5;   // seconds SkyCraft gives the collision to stream in, after a load say

	// Forgets every batch read, so each is read and sent again. Nothing is read before aReadyAt, nor before the time an
	// earlier reset gave.
	void Reset(double aReadyAt);
	// Where the area is centred. Batches with no region within one more than the area are forgotten.
	void Centre(const RegionPos& aCentre);

	// The next batch to read: the nearest not read yet, or one next to the centre read kRefresh or more ago. Nothing
	// before a centre is set, before the time Reset gave, or while every batch is read and fresh.
	std::optional<BatchPos> Next(double aNow) const;

	// aBatch was read at aNow and filed (FileBatch's order). Which of its regions to send, as indices into RegionsOf: those
	// whose triangles changed, and every one the first time.
	std::vector<size_t> Read(const BatchPos& aBatch, double aNow, const std::array<std::vector<McTriangle>, 8>& aFiled);

	std::optional<RegionPos> CentreRegion() const { return centre_; }
	size_t                   Known() const { return read_.size() * 8; }  // regions read since the last reset
	size_t                   Waiting() const;                            // the area's batches not read yet

private:
	struct Wanted
	{
		BatchPos batch;
		bool     nextToCentre = false;  // holds a region next to the centre: read again every kRefresh
	};

	struct Done
	{
		double                  at = 0.0;
		std::array<uint64_t, 8> hashes{};
	};

	std::optional<RegionPos> centre_;
	std::vector<Wanted>      wanted_;  // the area's batches, nearest first
	std::map<BatchPos, Done> read_;
	double                   readyAt_ = 0.0;
};
```

- [x] **Step 4: Edit `src/link/Ground.cpp`**

```cpp
#include <cmath>
```
```cpp
#include <cmath>
#include <cstdlib>
```
```cpp
	                   [](float aValue) { return std::isfinite(aValue) && std::fabs(aValue) <= kFarthest; });
```
```cpp
	                   [](float aValue) { return std::isfinite(aValue) && std::fabs(aValue) <= kFarthest; });
}

// SkyCraft's distance, for its order: height counts double.
int32_t Distance(const RegionPos& aRegion, const RegionPos& aCentre)
{
	const int32_t dx = aRegion.x - aCentre.x, dy = aRegion.y - aCentre.y, dz = aRegion.z - aCentre.z;
	return dx * dx + dz * dz + 2 * dy * dy;
}

// Within the area around aCentre, and aMore regions each way.
bool InArea(const RegionPos& aRegion, const RegionPos& aCentre, int32_t aMore)
{
	return std::abs(aRegion.x - aCentre.x) <= GroundAreas::kAcross + aMore && std::abs(aRegion.z - aCentre.z) <= GroundAreas::kAcross + aMore &&
	       aRegion.y - aCentre.y >= -GroundAreas::kBelow - aMore && aRegion.y - aCentre.y <= GroundAreas::kAbove + aMore;
}

// SkyCraft's regions next to the centre, read again every second: the city streams its collision in as V moves.
bool Near(const RegionPos& aRegion, const RegionPos& aCentre)
{
	return std::abs(aRegion.x - aCentre.x) <= 1 && std::abs(aRegion.z - aCentre.z) <= 1 && aRegion.y - aCentre.y >= -1 &&
	       aRegion.y - aCentre.y <= 0;
```
```cpp
		hash += Fnv1a(triangle.v, sizeof triangle.v);
	}
	return hash;
}
}
```
```cpp
		hash += Fnv1a(triangle.v, sizeof triangle.v);
	}
	return hash;
}

void GroundAreas::Reset(double aReadyAt)
{
	read_.clear();
	readyAt_ = std::max(readyAt_, aReadyAt);
}

void GroundAreas::Centre(const RegionPos& aCentre)
{
	if (centre_ == aCentre)
	{
		return;
	}
	centre_ = aCentre;
	// Each batch holding a region of the area, ordered by its nearest region, as SkyCraft orders its regions.
	struct Candidate
	{
		Wanted  wanted;
		int32_t distance = 0;
	};
	std::map<BatchPos, Candidate> batches;
	for (int32_t dy = -kBelow; dy <= kAbove; ++dy)
	{
		for (int32_t dz = -kAcross; dz <= kAcross; ++dz)
		{
			for (int32_t dx = -kAcross; dx <= kAcross; ++dx)
			{
				const RegionPos region{ aCentre.x + dx, aCentre.y + dy, aCentre.z + dz };
				const auto [it, added] = batches.try_emplace(BatchOf(region), Candidate{ { BatchOf(region) }, Distance(region, aCentre) });
				it->second.distance = std::min(it->second.distance, Distance(region, aCentre));
				it->second.wanted.nextToCentre = it->second.wanted.nextToCentre || Near(region, aCentre);
			}
		}
	}
	std::vector<Candidate> ordered;
	for (const auto& [batch, candidate] : batches)
	{
		ordered.push_back(candidate);
	}
	std::stable_sort(ordered.begin(), ordered.end(), [](const Candidate& aA, const Candidate& aB) { return aA.distance < aB.distance; });
	wanted_.clear();
	for (const Candidate& candidate : ordered)
	{
		wanted_.push_back(candidate.wanted);
	}
	// Batches left behind are forgotten, so they're read and sent again on return.
	std::erase_if(read_, [&](const auto& aEntry) {
		const std::array<RegionPos, 8> regions = RegionsOf(aEntry.first);
		return std::none_of(regions.begin(), regions.end(), [&](const RegionPos& aRegion) { return InArea(aRegion, aCentre, 1); });
	});
}

std::optional<BatchPos> GroundAreas::Next(double aNow) const
{
	if (!centre_ || aNow < readyAt_)
	{
		return std::nullopt;
	}
	for (const Wanted& wanted : wanted_)
	{
		const auto done = read_.find(wanted.batch);
		if (done == read_.end() || (wanted.nextToCentre && aNow - done->second.at >= kRefresh))
		{
			return wanted.batch;
		}
	}
	return std::nullopt;
}

std::vector<size_t> GroundAreas::Read(const BatchPos& aBatch, double aNow, const std::array<std::vector<McTriangle>, 8>& aFiled)
{
	const auto [done, first] = read_.try_emplace(aBatch);
	done->second.at = aNow;
	std::vector<size_t> send;
	for (size_t i = 0; i < aFiled.size(); ++i)
	{
		const uint64_t hash = TrianglesHash(aFiled[i]);
		if (first || done->second.hashes[i] != hash)
		{
			send.push_back(i);
		}
		done->second.hashes[i] = hash;
	}
	return send;
}

size_t GroundAreas::Waiting() const
{
	return size_t(std::count_if(wanted_.begin(), wanted_.end(), [&](const Wanted& aWanted) { return !read_.contains(aWanted.batch); }));
}
}
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `304 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: which of the city's batches to read next, around a centre that moves" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 25: The ground's worker voxelizes regions off the main thread

Voxelizing a region takes up to a few milliseconds (spot B's averaged 0.68 ms), too long for the game's main thread,
which has 1.5 ms a frame for the whole exporter. `GroundWorker` is SkyCraft's worker thread: it takes each region's
triangles, voxelizes them, and makes its two messages, the triangles then the voxels; the main thread takes them in the
order given. It counts a job as waiting until its messages are taken, so "nothing waiting" means everything given is
out. It catches a C++ exception from a region, out of memory say, and leaves that region out rather than ending the
game. Destroyed, it drops the jobs not started and waits only for the one under way.

**Files:**
- Create: `src/link/GroundWorker.h`, `src/link/GroundWorker.cpp`, `tests/TestGroundWorker.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestGroundWorker.cpp`**

```cpp
#include "Harness.h"

#include "link/GroundWorker.h"

#include <chrono>
#include <cstring>
#include <thread>

using namespace cpmc::link;

namespace
{
// Takes the worker's messages until aCount have come, or 5 s have gone.
std::vector<GroundWorker::Message> TakeAll(GroundWorker& aWorker, size_t aCount)
{
	std::vector<GroundWorker::Message> messages;
	for (int i = 0; i < 5000 && messages.size() < aCount; ++i)
	{
		for (GroundWorker::Message& message : aWorker.Take())
		{
			messages.push_back(std::move(message));
		}
		if (messages.size() < aCount)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
	return messages;
}

// A flat floor over the region's 8 x 8 blocks, at height aY.
std::vector<McTriangle> Floor(const RegionPos& aRegion, float aY)
{
	const float x = float(aRegion.x * kRegionBlocks), z = float(aRegion.z * kRegionBlocks);
	return { { { x, aY, z, x + 8, aY, z, x + 8, aY, z + 8 } }, { { x, aY, z, x + 8, aY, z + 8, x, aY, z + 8 } } };
}
}

TEST(GroundWorkerMakesEachRegionsMessages)
{
	GroundWorker                 worker;
	const std::vector<RegionPos> regions = { { -15, 0, 250 }, { -14, 0, 250 }, { -15, -1, 250 } };
	for (const RegionPos& region : regions)
	{
		worker.Add({ 3, region, region.y == 0 ? Floor(region, 6.14f) : std::vector<McTriangle>() });
	}
	CHECK_EQ(worker.Waiting(), size_t(3));
	const auto messages = TakeAll(worker, 6);
	CHECK_EQ(messages.size(), size_t(6));
	CHECK_EQ(worker.Waiting(), size_t(0));  // every job's messages taken
	for (size_t i = 0; i < regions.size() && messages.size() == 6; ++i)
	{
		// Each region's triangles, then its voxels.
		const RegionPos&              r = regions[i];
		const std::vector<McTriangle> triangles = r.y == 0 ? Floor(r, 6.14f) : std::vector<McTriangle>();
		CHECK_EQ(messages[2 * i].type, uint32_t(proto::kColTris));
		CHECK(messages[2 * i].payload == TrisPayload(r.x, r.y, r.z, 3, triangles));
		CHECK_EQ(messages[2 * i + 1].type, uint32_t(proto::kColRegion));
		CHECK(messages[2 * i + 1].payload == RegionPayload(r.x, r.y, r.z, 3, VoxelizeRegion(r.x, r.y, r.z, triangles)));
		CHECK_EQ(messages[2 * i + 1].epoch, 3u);
	}
	CHECK(worker.Take().empty());  // taken already
	CHECK_EQ(worker.Regions(), 3ull);
	CHECK_EQ(worker.Blocks(), 128ull);  // the two floors' 64 blocks each
	CHECK_EQ(worker.Failed(), 0ull);
	CHECK(worker.Ms() > 0.0);
}

TEST(GroundWorkerKeepsTheOrderOfItsJobs)
{
	GroundWorker worker;
	for (int32_t x = 0; x < 50; ++x)
	{
		worker.Add({ 1, { x, 0, 0 }, Floor({ x, 0, 0 }, 3.0f) });
	}
	const auto messages = TakeAll(worker, 100);
	CHECK_EQ(messages.size(), size_t(100));
	for (size_t i = 0; i < messages.size(); ++i)
	{
		proto::ColRegion header;
		std::memcpy(&header, messages[i].payload.data(), sizeof header);
		CHECK_EQ(header.minX, int32_t(i / 2) * 8);
	}
}

TEST(GroundWorkerStopsWithJobsLeft)
{
	// Destroyed with 2,000 regions waiting, about two seconds of work: it drops them, rather than finishing them first.
	const auto start = std::chrono::steady_clock::now();
	{
		GroundWorker worker;
		for (int32_t x = 0; x < 2000; ++x)
		{
			worker.Add({ 1, { x, 0, 0 }, Floor({ x, 0, 0 }, 3.0f) });
		}
	}
	CHECK(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500));
}
```
- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestGround.cpp
	tests/TestDrive.cpp
```
```cmake
	tests/TestGround.cpp
	tests/TestGroundWorker.cpp
	tests/TestDrive.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/GroundWorker.h'`.

- [x] **Step 4: Write `src/link/GroundWorker.h`**

```cpp
#pragma once

#include "link/Ground.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

// The ground's worker (spike 5's "The worker"): one thread voxelizes each region it's given, with SkyCraft's
// triangle-box test, and makes its messages, the triangles (kColTris) then the voxels (kColRegion), as SkyCraft's worker
// sends them. Voxelizing takes up to a few milliseconds a region, too long for the game's main thread, which only reads
// the triangles and writes the finished messages into the ring, in order.
namespace cpmc::link
{
class GroundWorker
{
public:
	struct Job
	{
		uint32_t                epoch = 0;
		RegionPos               region;
		std::vector<McTriangle> triangles;  // filed under the region (FileBatch)
	};

	struct Message
	{
		uint32_t             epoch = 0;
		uint32_t             type = 0;  // kColTris or kColRegion
		std::vector<uint8_t> payload;
	};

	GroundWorker();
	~GroundWorker();  // drops the jobs not started, and waits for the one under way
	GroundWorker(const GroundWorker&) = delete;
	GroundWorker& operator=(const GroundWorker&) = delete;

	void Add(Job aJob);
	// The messages finished since the last call, in the order of their jobs.
	std::vector<Message> Take();

	size_t   Waiting() const;  // jobs added whose messages haven't been taken yet
	uint64_t Regions() const;  // regions voxelized, since the start
	uint64_t Blocks() const;   // voxel blocks made
	uint64_t Failed() const;   // regions left out, for want of memory
	double   Ms() const;       // time spent voxelizing and making messages

private:
	void Run();

	mutable std::mutex      mutex_;
	std::condition_variable wake_;
	std::deque<Job>         jobs_;
	std::vector<Message>    done_;
	size_t                  inHand_ = 0;    // jobs added whose messages haven't been taken
	size_t                  finished_ = 0;  // of those, the jobs whose messages are in done_ (none for a failed one)
	bool                    stop_ = false;
	uint64_t                regions_ = 0;
	uint64_t                blocks_ = 0;
	uint64_t                failed_ = 0;
	double                  ms_ = 0.0;
	std::thread             thread_;  // last: it starts once the rest is made
};
}
```
- [x] **Step 5: Write `src/link/GroundWorker.cpp`**

```cpp
#include "link/GroundWorker.h"

#include <chrono>
#include <exception>
#include <utility>

namespace cpmc::link
{
GroundWorker::GroundWorker() : thread_([this] { Run(); })
{
}

GroundWorker::~GroundWorker()
{
	{
		std::lock_guard lock(mutex_);
		stop_ = true;
	}
	wake_.notify_all();
	thread_.join();
}

void GroundWorker::Add(Job aJob)
{
	{
		std::lock_guard lock(mutex_);
		jobs_.push_back(std::move(aJob));
		++inHand_;
	}
	wake_.notify_one();
}

std::vector<GroundWorker::Message> GroundWorker::Take()
{
	std::lock_guard lock(mutex_);
	inHand_ -= finished_;
	finished_ = 0;
	return std::exchange(done_, {});
}

size_t GroundWorker::Waiting() const
{
	std::lock_guard lock(mutex_);
	return inHand_;
}

uint64_t GroundWorker::Regions() const
{
	std::lock_guard lock(mutex_);
	return regions_;
}

uint64_t GroundWorker::Blocks() const
{
	std::lock_guard lock(mutex_);
	return blocks_;
}

uint64_t GroundWorker::Failed() const
{
	std::lock_guard lock(mutex_);
	return failed_;
}

double GroundWorker::Ms() const
{
	std::lock_guard lock(mutex_);
	return ms_;
}

void GroundWorker::Run()
{
	std::unique_lock lock(mutex_);
	while (true)
	{
		wake_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
		if (stop_)
		{
			return;
		}
		const Job job = std::move(jobs_.front());
		jobs_.pop_front();
		lock.unlock();
		const auto start = std::chrono::steady_clock::now();
		std::vector<Message> messages;
		size_t               blocks = 0;
		try
		{
			const RegionPos&                   at = job.region;
			const std::vector<proto::ColBlock> voxels = VoxelizeRegion(at.x, at.y, at.z, job.triangles);
			blocks = voxels.size();
			messages.push_back({ job.epoch, proto::kColTris, TrisPayload(at.x, at.y, at.z, job.epoch, job.triangles) });
			messages.push_back({ job.epoch, proto::kColRegion, RegionPayload(at.x, at.y, at.z, job.epoch, voxels) });
		}
		catch (const std::exception&)
		{
			messages.clear();  // out of memory, say: the region is left out, rather than the game ended
		}
		const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		lock.lock();
		++finished_;
		ms_ += ms;
		if (messages.empty())
		{
			++failed_;
			continue;
		}
		for (Message& message : messages)
		{
			done_.push_back(std::move(message));
		}
		++regions_;
		blocks_ += blocks;
	}
}
}
```
- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Ground.cpp
	src/link/Drive.cpp
```
```cmake
	src/link/Ground.cpp
	src/link/GroundWorker.cpp
	src/link/Drive.cpp
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `307 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the ground's worker voxelizes regions off the main thread" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 26: The link sends the city's ground, with one collision epoch for the floor and the city

The link gains the city (the spec's "The ground setting", "The exporter"'s epochs, settling and coordinates):
- **`LinkSettings::ground`,** `Floor` or `City`. Switching clears the last ground with a new epoch: the city's own
  clear, or the floor's, sent fresh. The floor is sent only with the floor, in and out of build mode.
- **One epoch:** `Status::floorEpoch` becomes `collisionEpoch`, the collision ring's: the floor's new floors and the
  city's clears both move it on, and `SkyState` reports it.
- **The city starts over,** with a clear, when the link starts and when Minecraft comes back; then nothing is read for
  SkyCraft's 1.5 s. When V is found again, after a load say, everything is read again after 1.5 s, without a clear.
- **The centre:** Minecraft's player while it drives, and V otherwise.
- **`ReadGround(now, source, budget)`,** once a frame after `Update`: while the ground is the city's, the link is on,
  Minecraft is there and the centre is known, it reads batches from a `GroundSource` (a new interface in
  `link/Ground.h`), nearest first, until the budget has gone, a batch always whole, files them, and gives the regions
  that changed to the worker. A source that can't read leaves the batch for the next frame.
- **The ring:** each `Update` takes the worker's finished messages, drops any of an older epoch, and writes as many as
  fit into the collision ring, in order; the rest wait for the next frame. A message over half the ring would never fit,
  so it's dropped and counted rather than holding everything up: a region would need over 400,000 triangles, where
  spike 1's densest region of ±32 m had 77,000.
- **The status** gains the city's centre, regions known and waiting, batches, regions and triangles sent, the reading
  time, the worker's figures, the messages waiting and dropped, and how long Minecraft held its player when build mode
  last started.

`mclink` and the plugin only follow the rename here; Task 27 and Task 35 give them the city.

**Files:**
- Modify: `src/link/Ground.h`, `src/link/Link.h`, `src/link/Link.cpp`, `src/Plugin.cpp`, `tools/mclink/Main.cpp`,
  `tests/TestLink.cpp`

- [x] **Step 1: Edit `tests/TestLink.cpp`: the failing tests**

```cpp
#include <cstring>
```
```cpp
#include <algorithm>
#include <cstring>
```
```cpp
const LinkSettings kSettings;
```
```cpp
const LinkSettings kSettings;

// Night City as a ground source: a street at Minecraft height 6.14 everywhere, two triangles over each box read.
struct Street : GroundSource
{
	bool Read(const McBox& aBox, std::vector<McTriangle>& aOut) override
	{
		boxes.push_back(aBox);
		if (!readable)
		{
			return false;
		}
		const float x0 = float(aBox.low.x), x1 = float(aBox.high.x), z0 = float(aBox.low.z), z1 = float(aBox.high.z);
		if (aBox.low.y <= 6.14 && aBox.high.y >= 6.14)
		{
			aOut.push_back({ { x0, 6.14f, z0, x1, 6.14f, z0, x1, 6.14f, z1 } });
			aOut.push_back({ { x0, 6.14f, z0, x1, 6.14f, z1, x0, 6.14f, z1 } });
		}
		return true;
	}

	std::vector<McBox> boxes;  // read, in order
	bool               readable = true;
};

LinkSettings City()
{
	LinkSettings settings;
	settings.ground = Ground::City;
	return settings;
}

// Updates the link with aFrame until everything the ground's worker was given is taken: written into the collision
// ring, as far as it has room. False if that takes over 5 s.
bool Finish(Link& aLink, const Link::Frame& aFrame, const LinkSettings& aSettings, Sink& aSink)
{
	for (int i = 0; i < 5000; ++i)
	{
		aLink.Update(aFrame, aSettings, aSink);
		if (aLink.State().workerJobs == 0)
		{
			return true;
		}
		Sleep(1);
	}
	return false;
}

proto::ColRegion HeaderOf(const std::vector<uint8_t>& aPayload)
{
	proto::ColRegion header{};
	std::memcpy(&header, aPayload.data(), sizeof header);
	return header;
}
```
```cpp
	CHECK_EQ(link.State().floorEpoch, 2u);
	minecraft.Beat(77, 2300);
	link.Update(At(2.3, Corner(20.0, 1.5)), kSettings, sink);  // 1.5 m up
	CHECK_EQ(link.State().floorEpoch, 3u);
	link.RequestFloor();
	minecraft.Beat(77, 2400);
	link.Update(At(2.4, Corner(20.0, 1.5)), kSettings, sink);
	CHECK_EQ(link.State().floorEpoch, 4u);
```
```cpp
	CHECK_EQ(link.State().collisionEpoch, 2u);
	minecraft.Beat(77, 2300);
	link.Update(At(2.3, Corner(20.0, 1.5)), kSettings, sink);  // 1.5 m up
	CHECK_EQ(link.State().collisionEpoch, 3u);
	link.RequestFloor();
	minecraft.Beat(77, 2400);
	link.Update(At(2.4, Corner(20.0, 1.5)), kSettings, sink);
	CHECK_EQ(link.State().collisionEpoch, 4u);
```
```cpp
	CHECK_EQ(link.State().floorEpoch, 1u);
```
```cpp
	CHECK_EQ(link.State().collisionEpoch, 1u);
```
```cpp
	CHECK_EQ(minecraft.Sky().flags, uint32_t(proto::kSkyInGame));
	CHECK_EQ(link.State().floorEpoch, 2u);
```
```cpp
	CHECK_EQ(minecraft.Sky().flags, uint32_t(proto::kSkyInGame));
	CHECK_EQ(link.State().collisionEpoch, 2u);
```
```cpp
	CHECK(sink.Logged("Minecraft let its player go"));
```
```cpp
	CHECK(sink.Logged("Minecraft let its player go"));
	CHECK(link.State().lastHold.has_value());
	CHECK_NEAR(link.State().lastHold.value_or(0.0), 0.1, 1e-9);  // held from 1.1 to 1.2
```
```cpp
	CHECK_EQ(link.State().floorEpoch, 2u);
```
```cpp
	CHECK_EQ(link.State().collisionEpoch, 2u);
```
```cpp
	link.Update(At(1.4, pose), kSettings, sink);
	CHECK_NEAR(link.State().eyeHeight, 1.70, 1e-9);
}
```
```cpp
	link.Update(At(1.4, pose), kSettings, sink);
	CHECK_NEAR(link.State().eyeHeight, 1.70, 1e-9);
}

TEST(LinkSendsTheCityOnceMinecraftIsThereAndItsCollisionHasSettled)
{
	FakeMinecraft                     minecraft;
	Link                              link(minecraft.base, 0);
	Sink                              sink;
	Street                            street;
	std::vector<std::vector<uint8_t>> payloads;
	// Without Minecraft nothing is read or sent: nobody would read the ring.
	link.Update(At(1.0), City(), sink);
	link.ReadGround(1.0, street, 1.5);
	CHECK(street.boxes.empty());
	CHECK(minecraft.Collision(payloads).empty());
	// Minecraft comes: the city starts over with a clear, and waits 1.5 s for the city's collision to stream in.
	minecraft.Beat(77, 2000);
	link.Update(At(2.0), City(), sink);
	auto messages = minecraft.Collision(payloads);
	CHECK(messages.size() == 1 && messages[0].type == proto::kColClear);
	CHECK(payloads[0] == std::vector<uint8_t>({ 1, 0, 0, 0 }));
	CHECK_EQ(minecraft.Sky().collisionEpoch, 1u);
	CHECK(sink.Logged("the city starts over, as collision epoch 1"));
	link.ReadGround(3.4, street, 1.5);
	CHECK(street.boxes.empty());
	// Then the batch holding V's region comes first. A budget of 0 reads one batch.
	link.ReadGround(3.5, street, 0.0);
	CHECK_EQ(street.boxes.size(), size_t(1));
	CHECK_NEAR(street.boxes[0].low.x, -128.25, 1e-9);  // batch -8 0 125, which holds region -15 0 250
	CHECK_NEAR(street.boxes[0].low.y, -0.25, 1e-9);
	CHECK_NEAR(street.boxes[0].low.z, 1999.75, 1e-9);
	CHECK(Finish(link, At(3.5), City(), sink));
	CHECK_EQ(link.State().collisionWaiting, size_t(0));
	payloads.clear();
	messages = minecraft.Collision(payloads);
	CHECK_EQ(messages.size(), size_t(16));  // its eight regions, each its triangles, then its voxels
	if (messages.size() == 16)
	{
		CHECK_EQ(messages[0].type, uint32_t(proto::kColTris));
		CHECK_EQ(HeaderOf(payloads[0]).count, 2u);  // the street's two triangles
		CHECK_EQ(messages[1].type, uint32_t(proto::kColRegion));
		CHECK_EQ(HeaderOf(payloads[1]).minX, -128);
		CHECK_EQ(HeaderOf(payloads[1]).count, 64u);  // the street, in block row 6
		CHECK_EQ(HeaderOf(payloads[1]).epoch, 1u);
		CHECK_EQ(HeaderOf(payloads[9]).minY, 8);  // the regions above: empty, and known
		CHECK_EQ(HeaderOf(payloads[9]).count, 0u);
	}
	CHECK_EQ(link.State().cityBatches, 1ull);
	CHECK_EQ(link.State().cityRegions, 8ull);
	CHECK_EQ(link.State().cityTriangles, 8ull);
	CHECK_EQ(link.State().workerRegions, 8ull);
	CHECK_EQ(link.State().workerBlocks, 256ull);
	CHECK_EQ(link.State().cityKnown, size_t(8));
	CHECK_EQ(link.State().cityWaiting, size_t(143));
	CHECK(link.State().cityCentre.has_value() && link.State().cityCentre->x == -15 && link.State().cityCentre->z == 250);
	// Unlinked, nothing more is read, though the batches next to V are due again.
	link.Update(At(4.6, Corner(), false), City(), sink);
	link.ReadGround(4.6, street, 1.5);
	CHECK_EQ(street.boxes.size(), size_t(1));
	// Linked again, and past the settle, but Minecraft has gone: nothing is read for nobody.
	link.Update(At(4.7), City(), sink);
	link.Update(At(6.3), City(), sink);  // its heartbeat is 4.3 s old
	CHECK(!link.State().minecraft);
	link.ReadGround(6.3, street, 1.5);
	CHECK_EQ(street.boxes.size(), size_t(1));
}

TEST(LinkCentresTheCityOnMinecraftsPlayerWhileItDrives)
{
	FakeMinecraft                     minecraft;
	Link                              link(minecraft.base, 0);
	Sink                              sink;
	std::vector<std::vector<uint8_t>> payloads;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), City(), sink);
	CHECK(link.State().cityCentre.has_value() && link.State().cityCentre->x == -15);  // V's region
	link.Update(InBuild(1.1), City(), sink);
	// Minecraft has let its player go, 40 blocks east of V: the city's area goes around it.
	minecraft.Ticked(2, { -79.5, 6.14, 2007.5 }, 1.15);
	link.Update(InBuild(1.2), City(), sink);
	CHECK(link.State().drive == DriveState::Driving);
	CHECK(link.State().cityCentre.has_value() && link.State().cityCentre->x == -10);
	// No floor, in build mode or out of it: the city's clear is all there is.
	const auto messages = minecraft.Collision(payloads);
	CHECK(messages.size() == 1 && messages[0].type == proto::kColClear);
}

TEST(LinkKeepsTheCitysMessagesInOrderUntilTheRingHasRoom)
{
	FakeMinecraft                     minecraft;
	Link                              link(minecraft.base, 0);
	Sink                              sink;
	Street                            street;
	std::vector<std::vector<uint8_t>> payloads;
	minecraft.Beat(77, 1000);
	link.Update(At(1.0), City(), sink);
	// Minecraft stops reading, and the ring fills up.
	uint8_t* const ring = minecraft.base + proto::kOffCollisionRing;
	for (size_t size = size_t(1) << 20; size >= 1; size /= 2)
	{
		while (WriteByteRing(ring, proto::kColRingDataBytes, 99, std::vector<uint8_t>(size)))
		{
		}
	}
	link.ReadGround(2.5, street, 0.0);
	CHECK(Finish(link, At(2.5), City(), sink));
	CHECK_EQ(link.State().collisionWaiting, size_t(16));
	// Minecraft reads again: the next update writes them all, in their order.
	minecraft.Collision(payloads);
	payloads.clear();
	link.Update(At(2.6), City(), sink);
	CHECK_EQ(link.State().collisionWaiting, size_t(0));
	const auto messages = minecraft.Collision(payloads);
	CHECK_EQ(messages.size(), size_t(16));
	const std::array<RegionPos, 8> regions = RegionsOf({ -8, 0, 125 });
	for (size_t i = 0; i < 8 && messages.size() == 16; ++i)
	{
		CHECK(messages[2 * i].type == proto::kColTris && messages[2 * i + 1].type == proto::kColRegion);
		CHECK(HeaderOf(payloads[2 * i]).minX == regions[i].x * 8 && HeaderOf(payloads[2 * i]).minY == regions[i].y * 8 &&
		      HeaderOf(payloads[2 * i]).minZ == regions[i].z * 8);
	}
}

TEST(LinkStartsTheCityOverWhenMinecraftComesBack)
{
	FakeMinecraft                     minecraft;
	Link                              link(minecraft.base, 0);
	Sink                              sink;
	Street                            street;
	std::vector<std::vector<uint8_t>> payloads;
	minecraft.Beat(77, 1000);
	link.Update(At(1.0), City(), sink);
	link.ReadGround(2.5, street, 0.0);  // eight regions for the worker, of epoch 1
	// A new Minecraft before they're written: a new clear, and the last epoch's regions are dropped.
	minecraft.Beat(78, 2600);
	CHECK(Finish(link, At(2.6), City(), sink));
	auto messages = minecraft.Collision(payloads);
	CHECK_EQ(messages.size(), size_t(2));
	CHECK(payloads.size() == 2 && payloads[1] == std::vector<uint8_t>({ 2, 0, 0, 0 }));
	// The city is read again from the start, once the new Minecraft's 1.5 s have gone.
	link.ReadGround(4.0, street, 0.0);
	CHECK_EQ(street.boxes.size(), size_t(1));
	link.ReadGround(4.1, street, 0.0);
	CHECK_EQ(street.boxes.size(), size_t(2));
	CHECK_NEAR(street.boxes[1].low.x, street.boxes[0].low.x, 1e-9);
	CHECK(Finish(link, At(4.1), City(), sink));
	payloads.clear();
	messages = minecraft.Collision(payloads);
	CHECK_EQ(messages.size(), size_t(16));
	CHECK(std::all_of(payloads.begin(), payloads.end(), [](const std::vector<uint8_t>& aPayload) { return HeaderOf(aPayload).epoch == 2; }));
}

TEST(LinkSwitchesBetweenTheFloorAndTheCity)
{
	FakeMinecraft                     minecraft;
	Link                              link(minecraft.base, 0);
	Sink                              sink;
	Street                            street;
	std::vector<std::vector<uint8_t>> payloads;
	minecraft.Beat(77, 1000);
	link.Update(At(1.0), kSettings, sink);  // the floor, epoch 1
	CHECK_EQ(minecraft.Collision(payloads).size(), size_t(1 + 49 * 4));
	// The city: a clear with a new epoch, and no floor. Its collision is there already, so it's read at once.
	link.Update(At(3.0), City(), sink);
	payloads.clear();
	auto messages = minecraft.Collision(payloads);
	CHECK(messages.size() == 1 && messages[0].type == proto::kColClear);
	CHECK(payloads.size() == 1 && payloads[0] == std::vector<uint8_t>({ 2, 0, 0, 0 }));
	CHECK(sink.Logged("the ground is the city now"));
	link.ReadGround(3.0, street, 0.0);
	CHECK_EQ(street.boxes.size(), size_t(1));
	CHECK(Finish(link, At(3.0), City(), sink));
	CHECK_EQ(minecraft.Collision(payloads).size(), size_t(16));
	// Back to the floor: its own clear, with a new epoch, then the floor.
	link.Update(At(3.1), kSettings, sink);
	payloads.clear();
	messages = minecraft.Collision(payloads);
	CHECK_EQ(messages.size(), size_t(1 + 49 * 4));
	CHECK(payloads.size() == 1 + 49 * 4 && payloads[0] == std::vector<uint8_t>({ 3, 0, 0, 0 }));
	CHECK(sink.Logged("the ground is the floor now"));
	link.ReadGround(3.1, street, 0.0);
	CHECK_EQ(street.boxes.size(), size_t(1));
}

TEST(LinkReadsTheCityAgainWhenVIsFoundAgain)
{
	FakeMinecraft                     minecraft;
	Link                              link(minecraft.base, 0);
	Sink                              sink;
	Street                            street;
	std::vector<std::vector<uint8_t>> payloads;
	minecraft.Beat(77, 1000);
	link.Update(At(1.0), City(), sink);
	link.ReadGround(2.5, street, 0.0);
	// A source that can't read leaves the batch for the next frame.
	street.readable = false;
	link.ReadGround(2.6, street, 0.0);
	CHECK_EQ(link.State().cityUnread, 1ull);
	street.readable = true;
	link.ReadGround(2.7, street, 0.0);
	CHECK_EQ(street.boxes.size(), size_t(3));
	CHECK_NEAR(street.boxes[2].low.x, street.boxes[1].low.x, 1e-9);
	CHECK_NEAR(street.boxes[2].low.y, street.boxes[1].low.y, 1e-9);
	CHECK_NEAR(street.boxes[2].low.z, street.boxes[1].low.z, 1e-9);
	// V is lost, in a load say: nothing is read without a centre.
	link.Update(At(3.0, std::nullopt), City(), sink);
	link.ReadGround(3.0, street, 0.0);
	CHECK_EQ(street.boxes.size(), size_t(3));
	// V found again: everything is read again once 1.5 s have gone, and without a clear.
	minecraft.Beat(77, 4000);
	link.Update(At(4.0), City(), sink);
	link.ReadGround(5.4, street, 0.0);
	CHECK_EQ(street.boxes.size(), size_t(3));
	link.ReadGround(5.5, street, 0.0);
	CHECK_EQ(street.boxes.size(), size_t(4));
	CHECK_NEAR(street.boxes[3].low.x, street.boxes[0].low.x, 1e-9);
	CHECK(Finish(link, At(5.5), City(), sink));
	minecraft.Collision(payloads);
	CHECK_EQ(std::count_if(payloads.begin(), payloads.end(), [](const std::vector<uint8_t>& aPayload) { return aPayload.size() == 4; }), 1);
	CHECK_EQ(link.State().collisionEpoch, 1u);
}

TEST(LinkDropsAMessageTooBigForTheRing)
{
	// One region with 420,000 triangles: its triangles message, 16.8 MB, is over half the ring, so it would never fit.
	// It's dropped, rather than holding up everything after it.
	struct Dense : GroundSource
	{
		bool Read(const McBox& aBox, std::vector<McTriangle>& aOut) override
		{
			const float x = float(aBox.low.x) + 4.25f, z = float(aBox.low.z) + 4.25f;  // in the batch's first region
			aOut.assign(420'000, McTriangle{ { x, 6.14f, z, x + 0.1f, 6.14f, z, x, 6.14f, z + 0.1f } });
			return true;
		}
	};
	FakeMinecraft                     minecraft;
	Link                              link(minecraft.base, 0);
	Sink                              sink;
	Dense                             dense;
	std::vector<std::vector<uint8_t>> payloads;
	minecraft.Beat(77, 1000);
	link.Update(At(1.0), City(), sink);
	link.ReadGround(2.5, dense, 0.0);
	CHECK(Finish(link, At(2.5), City(), sink));
	CHECK_EQ(link.State().collisionDropped, 1ull);
	CHECK_EQ(link.State().collisionWaiting, size_t(0));
	CHECK_EQ(minecraft.Collision(payloads).size(), size_t(1 + 15));  // the clear, and the rest of the batch
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2504: 'GroundSource': base class undefined`.

- [x] **Step 3: Edit `src/link/Ground.h`**

```cpp
// Which batches to read, around a centre that moves (the spec's "The area", "Refreshing" and "Settling"): SkyCraft's
```
```cpp
// Where the triangles come from: PhysX in the game, a saved dump in mclink.
class GroundSource
{
public:
	virtual ~GroundSource() = default;
	// The triangles (Minecraft axes) of whatever touches aBox; more may come, which filing drops. False if nothing can
	// be read now: the batch waits.
	virtual bool Read(const McBox& aBox, std::vector<McTriangle>& aOut) = 0;
};

// Which batches to read, around a centre that moves (the spec's "The area", "Refreshing" and "Settling"): SkyCraft's
```

- [x] **Step 4: Edit `src/link/Link.h`**

```cpp
#include "link/Drive.h"
```
```cpp
#include "link/Drive.h"
#include "link/Ground.h"
#include "link/GroundWorker.h"
```
```cpp
#include <atomic>
```
```cpp
#include <atomic>
#include <deque>
#include <memory>
```
```cpp
// adds build mode's drive (its spec's "Part 1"): one teleport, then Minecraft drives and the link says where V goes.
// It works on the mapping's memory, so the tests give it plain memory with a fake Minecraft.
namespace cpmc::link
{
```
```cpp
// adds build mode's drive (its spec's "Part 1"): one teleport, then Minecraft drives and the link says where V goes. And
// Night City's ground (its "Part 2"): the city's collision instead of the floor, read around the centre and sent as it
// changes. It works on the mapping's memory, so the tests give it plain memory with a fake Minecraft.
namespace cpmc::link
{
// What Minecraft gets as the game's shape (set ground).
enum class Ground
{
	Floor,  // spike 4's flat floor at V's feet
	City,   // Night City's collision, from a GroundSource
};

const char* GroundName(Ground aGround);  // "floor" or "city"

```
```cpp
	uint32_t viewportHeight = 0;
```
```cpp
	uint32_t viewportHeight = 0;
	Ground   ground = Ground::Floor;    // switching sends a clear with a new epoch, then the other ground
```
```cpp
	// Push may be called from any thread. Everything else, from the thread that calls Update.
```
```cpp
	// Night City's ground, once a frame after Update (spike 5's "The exporter"): reads batches from aSource, nearest
	// first, until aBudgetMs has gone (a batch is always read whole), and hands the regions whose triangles changed to
	// the worker; the next updates write its messages into the collision ring, in order. It reads only with the ground
	// the city's, linked, Minecraft there, and the last update's centre known: Minecraft's player while it drives, and
	// V otherwise.
	void ReadGround(double aNow, GroundSource& aSource, double aBudgetMs);

	// Push may be called from any thread. Everything else, from the thread that calls Update.
```
```cpp
		uint32_t floorEpoch = 0;
		bool     floorSent = false;
		Vec3d    floorCentre;  // Minecraft axes; y is the floor's height
```
```cpp
		// The ground: the floor, or the city's (spike 5's "Part 2")
		Ground                   ground = Ground::Floor;  // what Minecraft gets now
		uint32_t                 collisionEpoch = 0;      // the collision ring's: the floor's or the city's last clear
		bool                     floorSent = false;
		Vec3d                    floorCentre;           // Minecraft axes; y is the floor's height
		std::optional<RegionPos> cityCentre;            // the region the city's area is around
		size_t                   cityKnown = 0;         // regions read since the city last started over
		size_t                   cityWaiting = 0;       // the area's batches not read yet
		uint64_t                 cityBatches = 0;       // batches read, since the start
		uint64_t                 cityUnread = 0;        // batches the source couldn't read
		uint64_t                 cityRegions = 0;       // regions sent to the worker: new, or changed
		uint64_t                 cityTriangles = 0;     // their triangles
		double                   cityReadMs = 0.0;      // the last ReadGround's
		size_t                   workerJobs = 0;        // regions given to the worker whose messages aren't taken yet
		uint64_t                 workerRegions = 0;     // regions it voxelized, since the start
		uint64_t                 workerBlocks = 0;      // voxel blocks it made
		double                   workerMs = 0.0;        // its time
		size_t                   collisionWaiting = 0;  // the city's messages waiting for room in the collision ring
		uint64_t                 collisionBytes = 0;    // the city's, written into it since the start
		uint64_t                 collisionDropped = 0;  // messages too big for it
```
```cpp
		std::string           endBuild;             // this frame: why build mode must end; empty: it goes on
```
```cpp
		std::string           endBuild;             // this frame: why build mode must end; empty: it goes on
		std::optional<double> lastHold;             // seconds Minecraft held its player when build mode last started
```
```cpp
	void SendFloor(const Vec3d& aCentre, bool aNewEpoch, LinkSink& aSink);
```
```cpp
	void SendFloor(const Vec3d& aCentre, bool aNewEpoch, LinkSink& aSink);
	// The city's messages into the collision ring, in order, as many as fit: a clear first if one is due, then the
	// worker's, those of an older epoch dropped. The rest wait for the next frame.
	void WriteCity(LinkSink& aSink);
```
```cpp
	std::optional<Vec3d>  lastEye_;  // where the last frame put Minecraft's eye, world axes
```
```cpp
	std::optional<Vec3d>  lastEye_;  // where the last frame put Minecraft's eye, world axes
	// The city's ground (spike 5's "Part 2")
	bool                              wasLinked_ = false;
	bool                              cityClear_ = false;  // a clear is due before the city's next regions
	bool                              centred_ = false;    // the last update knew the city's centre, linked
	GroundAreas                       areas_;
	std::unique_ptr<GroundWorker>     worker_;  // made when the city is first read
	std::deque<GroundWorker::Message> outbox_;  // the city's messages, waiting for room in the collision ring
```

- [x] **Step 5: Edit `src/link/Link.cpp`**

```cpp
	return names[aType < Link::kTypes ? aType : Link::kTypes - 1];
```
```cpp
	return names[aType < Link::kTypes ? aType : Link::kTypes - 1];
}

const char* GroundName(Ground aGround)
{
	return aGround == Ground::City ? "city" : "floor";
```
```cpp
		teleportDue_ = true;
		aSink.Log(std::format("Minecraft is there (process {})", pid));
```
```cpp
		teleportDue_ = true;
		cityClear_ = true;  // the city starts over, with a clear, once its collision has had time to stream in
		areas_.Reset(aFrame.now + GroundAreas::kSettle);
		aSink.Log(std::format("Minecraft is there (process {})", pid));
```
```cpp
	// In game while linked with V there. Linking teleports Minecraft's player and sends a fresh floor.
```
```cpp
	// The ground (spike 5's "Part 2"). Switching clears the last one, with a new epoch: the city's own clear, or the
	// floor's, sent fresh.
	if (aSettings.ground != status_.ground)
	{
		status_.ground = aSettings.ground;
		status_.floorSent = false;
		cityClear_ = aSettings.ground == Ground::City;
		areas_.Reset(aFrame.now);
		outbox_.clear();
		aSink.Log(std::format("the ground is the {} now", GroundName(aSettings.ground)));
	}
	const bool city = status_.ground == Ground::City;
	if (aFrame.linked && !wasLinked_)
	{
		cityClear_ = true;  // the link starts: the city starts over
		areas_.Reset(aFrame.now + GroundAreas::kSettle);
	}
	wasLinked_ = aFrame.linked;

	// In game while linked with V there. Linking teleports Minecraft's player and sends a fresh floor. V found again,
	// after a load say, has the city read again once its collision has streamed in.
```
```cpp
		status_.floorSent = false;
	}
```
```cpp
		status_.floorSent = false;
		areas_.Reset(aFrame.now + GroundAreas::kSettle);
	}
```
```cpp
		if (there)
```
```cpp
		if (there && !city)
```
```cpp
		{
			aSink.Log(std::format("Minecraft let its player go after {:.2f} s: V follows it", aFrame.now - since));
```
```cpp
		{
			status_.lastHold = aFrame.now - since;
			aSink.Log(std::format("Minecraft let its player go after {:.2f} s: V follows it", aFrame.now - since));
```
```cpp
		if (there && (!status_.floorSent || floorRequested_))
```
```cpp
		if (there && !city && (!status_.floorSent || floorRequested_))
```
```cpp
		else if (there && std::hypot(centre.x - status_.floorCentre.x, centre.z - status_.floorCentre.z) > 16.0)
```
```cpp
		else if (there && !city && std::hypot(centre.x - status_.floorCentre.x, centre.z - status_.floorCentre.z) > 16.0)
```
```cpp
		if (there && (!status_.floorSent || floorRequested_ || std::hypot(feet.x - status_.floorCentre.x, feet.z - status_.floorCentre.z) > 16.0 ||
		              std::abs(feet.y - status_.floorCentre.y) > 1.0))
```
```cpp
		if (there && !city &&
		    (!status_.floorSent || floorRequested_ || std::hypot(feet.x - status_.floorCentre.x, feet.z - status_.floorCentre.z) > 16.0 ||
		     std::abs(feet.y - status_.floorCentre.y) > 1.0))
```
```cpp
	// The position: the last teleport's target, or in build mode V's feet, every frame, so that a teleport Minecraft
```
```cpp
	// The city: its area goes around Minecraft's player while it drives, and V otherwise. Its messages go into the
	// collision ring only while Minecraft is there to read them.
	centred_ = false;
	if (city)
	{
		const bool                 driving = drive_.State() == DriveState::Driving && mc;
		const std::optional<Vec3d> centre = driving       ? std::optional<Vec3d>(Vec3d{ mc->x, mc->y, mc->z })
		                                    : aFrame.pose ? std::optional<Vec3d>(McFromWorld(aFrame.pose->feet))
		                                                  : std::nullopt;
		if (const std::optional<RegionPos> region = centre ? RegionAt(*centre) : std::nullopt)
		{
			areas_.Centre(*region);
			centred_ = aFrame.linked;
		}
		if (there)
		{
			WriteCity(aSink);
		}
	}
	status_.cityCentre = areas_.CentreRegion();
	status_.cityKnown = areas_.Known();
	status_.cityWaiting = areas_.Waiting();
	status_.workerJobs = worker_ ? worker_->Waiting() : 0;
	status_.workerRegions = worker_ ? worker_->Regions() : 0;
	status_.workerBlocks = worker_ ? worker_->Blocks() : 0;
	status_.workerMs = worker_ ? worker_->Ms() : 0.0;
	status_.collisionWaiting = outbox_.size();

	// The position: the last teleport's target, or in build mode V's feet, every frame, so that a teleport Minecraft
```
```cpp
	state.collisionEpoch = status_.floorEpoch;
```
```cpp
	state.collisionEpoch = status_.collisionEpoch;
```
```cpp
	const uint32_t epoch = aNewEpoch ? status_.floorEpoch + 1 : status_.floorEpoch;
```
```cpp
	const uint32_t epoch = aNewEpoch ? status_.collisionEpoch + 1 : status_.collisionEpoch;
```
```cpp
	status_.floorEpoch = epoch;
```
```cpp
	status_.collisionEpoch = epoch;
```
```cpp
		aSink.Log("the floor didn't fit in the collision ring; trying again next frame");
```
```cpp
		aSink.Log("the floor didn't fit in the collision ring; trying again next frame");
	}
}

void Link::ReadGround(double aNow, GroundSource& aSource, double aBudgetMs)
{
	status_.cityReadMs = 0.0;
	if (status_.ground != Ground::City || !status_.minecraft || !centred_ || cityClear_)
	{
		return;  // nobody would read it, or there's no centre, or the regions would be of the epoch the clear ends
	}
	if (!worker_)
	{
		worker_ = std::make_unique<GroundWorker>();
	}
	const auto              start = std::chrono::steady_clock::now();
	std::vector<McTriangle> triangles;
	while (const std::optional<BatchPos> batch = areas_.Next(aNow))
	{
		triangles.clear();
		if (!aSource.Read(BatchBox(*batch), triangles))
		{
			++status_.cityUnread;
			break;  // the batch waits for the next frame
		}
		std::array<std::vector<McTriangle>, 8> filed = FileBatch(*batch, triangles);
		const std::array<RegionPos, 8>         regions = RegionsOf(*batch);
		for (const size_t i : areas_.Read(*batch, aNow, filed))
		{
			++status_.cityRegions;
			status_.cityTriangles += filed[i].size();
			worker_->Add({ status_.collisionEpoch, regions[i], std::move(filed[i]) });
		}
		++status_.cityBatches;
		if (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() >= aBudgetMs)
		{
			break;
		}
	}
	status_.cityReadMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	status_.cityKnown = areas_.Known();
	status_.cityWaiting = areas_.Waiting();
	status_.workerJobs = worker_->Waiting();
}

void Link::WriteCity(LinkSink& aSink)
{
	if (cityClear_)
	{
		// Anything still waiting is of the last epoch, which the clear ends.
		outbox_.clear();
		++status_.collisionEpoch;
		outbox_.push_back({ status_.collisionEpoch, proto::kColClear, FloorClear(status_.collisionEpoch) });
		cityClear_ = false;
		aSink.Log(std::format("the city starts over, as collision epoch {}", status_.collisionEpoch));
	}
	if (worker_)
	{
		for (GroundWorker::Message& message : worker_->Take())
		{
			if (message.epoch == status_.collisionEpoch)
			{
				outbox_.push_back(std::move(message));
			}
		}
	}
	uint8_t* const ring = base_ + proto::kOffCollisionRing;
	while (!outbox_.empty())
	{
		const GroundWorker::Message& next = outbox_.front();
		if (next.payload.size() + 8 > proto::kColRingDataBytes / 2)
		{
			++status_.collisionDropped;  // it would never fit
		}
		else if (!WriteByteRing(ring, proto::kColRingDataBytes, next.type, next.payload))
		{
			break;  // no room until Minecraft reads: the rest wait for the next frame, in order
		}
		else
		{
			status_.collisionBytes += next.payload.size();
		}
		outbox_.pop_front();
```

- [x] **Step 6: Edit `src/Plugin.cpp`**

```cpp
		report.floorEpoch = state.floorEpoch;
```
```cpp
		report.floorEpoch = state.collisionEpoch;
```

- [x] **Step 7: Edit `tools/mclink/Main.cpp`**

```cpp
	    s.teleports, s.teleportSeq, s.target.x, s.target.y, s.target.z, s.look.yaw, s.look.pitch, s.floorEpoch,
```
```cpp
	    s.teleports, s.teleportSeq, s.target.x, s.target.y, s.target.z, s.look.yaw, s.look.pitch, s.collisionEpoch,
```

- [x] **Step 8: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `314 test(s), 0 failure(s)`.

- [x] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the link sends the city's ground, with one collision epoch for the floor and the city" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 27: `mclink` sends Night City's shape from spike 1's dumps

Step 3 opens offline (the spec's "`mclink`'s dump loader"): spike 1's dumps go through the city's own filing,
voxelizing and messages, against the real Minecraft, before the game is involved.
- **`DumpObj`:** a dump's `collision.obj` has one object a category, its vertices then its faces, the faces counting
  vertices from the file's first. `ParseDumpObj` keeps the objects whose names start with a prefix: `s0_static_` holds
  scene 0's static actors' simulation shapes, nearly all of them scene-query shapes too.
- **The script:** `ground floor|city`; `dump <collision.obj>`, which moves the dump's triangles, relative to the feet of
  V when it was dumped, to the stand-in V's feet; and `wait-ground [seconds]`, until the city around the centre is all
  read, voxelized and in the ring (30 s unless given). `status` gains a ground line.
- **The source:** the dump's triangles touching each box read, through the link's `ReadGround` within the plugin's
  1.5 ms a frame.
- **`city.txt`,** step 3's script, with spot B 200 blocks east of spike 3's corner and the ramp 100 farther, clear of the
  blocks built there: Minecraft's world there is a void, so nothing of its own is in the way.

`ScriptsInTheRepoParse` parses `city.txt` too.

**Files:**
- Create: `tools/mclink/DumpObj.h`, `tools/mclink/DumpObj.cpp`, `tools/mclink/city.txt`, `tests/TestDumpObj.cpp`
- Modify: `tools/mclink/McLinkScript.h`, `tools/mclink/McLinkScript.cpp`, `tools/mclink/Main.cpp`,
  `tests/TestMcLinkScript.cpp`, `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestDumpObj.cpp`**

```cpp
#include "Harness.h"

#include "DumpObj.h"

using namespace cpmc::mclink;

namespace
{
// A dump as the probe writes it (ObjWriter.cpp): one object a category, its vertices then its faces, the faces counting
// vertices from the file's first.
constexpr const char* kDump = "# PhysXProbe collision dump\n"
                              "# V's feet (world, metres, Z up): -189.898 616.754 74.432; axes: Minecraft (Y up)\n"
                              "mtllib collision.mtl\n"
                              "o s0_dynamic_box\n"
                              "usemtl s0_dynamic_box\n"
                              "v 1.000 0.000 1.000\n"
                              "v 2.000 0.000 1.000\n"
                              "v 2.000 1.000 1.000\n"
                              "f 1 2 3\n"
                              "o s0_static_trimesh\n"
                              "usemtl s0_static_trimesh\n"
                              "v -1.500 -0.120 -2.000\n"
                              "v 3.250 -0.120 -2.000\n"
                              "v 3.250 -0.120 4.000\n"
                              "v -1.500 -0.120 4.000\n"
                              "f 4 5 6\n"
                              "f 4 6 7\n"
                              "o s0_queryonly_capsule\n"
                              "usemtl s0_queryonly_capsule\n"
                              "v 0.000 0.000 0.000\n"
                              "f 1 4 8\n";
}

TEST(DumpObjKeepsTheObjectsAskedFor)
{
	std::string error;
	const auto  triangles = ParseDumpObj(kDump, "s0_static_", error);
	CHECK(triangles.has_value());
	CHECK_EQ(triangles.value_or(std::vector<cpmc::link::McTriangle>()).size(), size_t(2));
	if (triangles && triangles->size() == 2)
	{
		const float* first = triangles->at(0).v;
		CHECK(first[0] == -1.5f && first[1] == -0.12f && first[2] == -2.0f && first[3] == 3.25f && first[8] == 4.0f);
		const float* second = triangles->at(1).v;
		CHECK(second[6] == -1.5f && second[7] == -0.12f && second[8] == 4.0f);
	}
	CHECK_EQ(ParseDumpObj(kDump, "s0_", error).value_or(std::vector<cpmc::link::McTriangle>()).size(), size_t(4));
}

TEST(DumpObjRefusesWhatItCantRead)
{
	std::string error;
	CHECK(!ParseDumpObj("o s0_static_box\nv 1 2 3\nf 1 2 3\n", "s0_static_", error));  // vertices 2 and 3 aren't there
	CHECK_STR(error, "line 3: \"f 1 2 3\" isn't a vertex, a triangle of vertices before it, or an object");
	CHECK(!ParseDumpObj("v 1 2\n", "s0_static_", error));
	CHECK(!ParseDumpObj("v 1 2 3\nf 1 1 1 1\n", "s0_static_", error));
	CHECK(!ParseDumpObj("v nan 0 0\n", "s0_static_", error));
	CHECK(!ParseDumpObj("v 1 2 3\nf 0 1 1\n", "s0_static_", error));
	CHECK(ParseDumpObj("\r\n# a comment\r\n", "s0_static_", error).value_or(std::vector<cpmc::link::McTriangle>(1)).empty());
}
```
- [x] **Step 2: Edit `tests/TestMcLinkScript.cpp`: the failing test**

```cpp
	for (const char* name : { "step1.txt", "again.txt", "eye.txt", "drive.txt", "relink.txt" })
```
```cpp
	for (const char* name : { "step1.txt", "again.txt", "eye.txt", "drive.txt", "relink.txt", "city.txt" })
```
```cpp
	CHECK(!ParseScript("status now", error).has_value());
}
```
```cpp
	CHECK(!ParseScript("status now", error).has_value());
}

TEST(ScriptParsesTheGround)
{
	std::string error;
	const auto  actions =
	    ParseScript("ground city\ndump E:\\dumps\\2026-10-01_14-37-07\\collision.obj\nwait-ground\nwait-ground 60\nground floor\n", error);
	CHECK(actions.has_value());
	CHECK_EQ(actions->size(), size_t(5));
	const auto& a = *actions;
	CHECK(a[0].kind == Kind::Ground);
	CHECK_STR(a[0].text, "city");
	CHECK(a[1].kind == Kind::Dump);
	CHECK_STR(a[1].text, "E:\\dumps\\2026-10-01_14-37-07\\collision.obj");
	CHECK(a[2].kind == Kind::WaitGround);
	CHECK_EQ(a[2].numbers[0], 30.0);
	CHECK_EQ(a[3].numbers[0], 60.0);
	CHECK_STR(a[4].text, "floor");
	CHECK(!ParseScript("ground stone", error).has_value());
	CHECK_STR(error, "line 1: ground takes floor or city: \"ground stone\"");
	CHECK(!ParseScript("dump", error).has_value());
	CHECK(!ParseScript("wait-ground 0", error).has_value());
}
```

- [x] **Step 3: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestDriveLog.cpp
	tools/mclink/McLinkScript.cpp
)
```
```cmake
	tests/TestDriveLog.cpp
	tests/TestDumpObj.cpp
	tools/mclink/McLinkScript.cpp
)
```

- [x] **Step 4: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'DumpObj.h'`.

- [x] **Step 5: Write `tools/mclink/DumpObj.h`**

```cpp
#pragma once

#include "link/Voxels.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Spike 1's saved collision, for step 3's offline half (spike 5's "mclink's dump loader"): a dump's collision.obj holds
// each category's triangles as one OBJ object ("s0_static_trimesh", "s0_dynamic_box" and so on), in Minecraft's axes
// relative to V's feet when it was dumped, to the millimetre. The "s0_static_" objects are scene 0's static actors'
// simulation shapes, nearly all of them scene-query shapes too: they stand in for what the exporter keeps.
namespace cpmc::mclink
{
// The triangles of the objects whose names start with aPrefix, relative to the dump's V's feet; or nothing, with aError
// naming the first line that isn't a vertex, a face of three vertices that are there, an object, or one of the lines
// the probe writes for its materials.
std::optional<std::vector<link::McTriangle>> ParseDumpObj(std::string_view aText, std::string_view aPrefix, std::string& aError);
}
```
- [x] **Step 6: Write `tools/mclink/DumpObj.cpp`**

```cpp
#include "DumpObj.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <format>

namespace cpmc::mclink
{
namespace
{
std::vector<std::string_view> Words(std::string_view aLine)
{
	std::vector<std::string_view> words;
	size_t                        at = 0;
	while (at < aLine.size())
	{
		const size_t start = aLine.find_first_not_of(" \t", at);
		if (start == std::string_view::npos)
		{
			break;
		}
		const size_t end = aLine.find_first_of(" \t", start);
		words.push_back(aLine.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
		at = end == std::string_view::npos ? aLine.size() : end;
	}
	return words;
}

bool Number(std::string_view aWord, double& aOut)
{
	const auto [end, error] = std::from_chars(aWord.data(), aWord.data() + aWord.size(), aOut);
	return error == std::errc() && end == aWord.data() + aWord.size() && std::isfinite(aOut);
}

bool Index(std::string_view aWord, size_t& aOut)
{
	const auto [end, error] = std::from_chars(aWord.data(), aWord.data() + aWord.size(), aOut);
	return error == std::errc() && end == aWord.data() + aWord.size() && aOut >= 1;
}
}

std::optional<std::vector<link::McTriangle>> ParseDumpObj(std::string_view aText, std::string_view aPrefix, std::string& aError)
{
	std::vector<std::array<double, 3>> vertices;
	std::vector<link::McTriangle>      triangles;
	bool                               keep = false;  // the object the lines are in is one asked for
	int                                lineNumber = 0;
	size_t                             at = 0;
	while (at < aText.size())
	{
		const size_t     end = aText.find('\n', at);
		std::string_view line = aText.substr(at, end == std::string_view::npos ? std::string_view::npos : end - at);
		at = end == std::string_view::npos ? aText.size() : end + 1;
		++lineNumber;
		const std::vector<std::string_view> words = Words(line.ends_with('\r') ? line.substr(0, line.size() - 1) : line);
		if (words.empty() || words[0].starts_with('#') || words[0] == "mtllib" || words[0] == "usemtl")
		{
			continue;
		}
		if (words[0] == "o" && words.size() == 2)
		{
			keep = words[1].starts_with(aPrefix);
			continue;
		}
		if (words[0] == "v" && words.size() == 4)
		{
			std::array<double, 3> vertex{};
			if (Number(words[1], vertex[0]) && Number(words[2], vertex[1]) && Number(words[3], vertex[2]))
			{
				vertices.push_back(vertex);
				continue;
			}
		}
		else if (words[0] == "f" && words.size() == 4)
		{
			size_t corners[3] = {};
			if (Index(words[1], corners[0]) && Index(words[2], corners[1]) && Index(words[3], corners[2]) &&
			    std::max({ corners[0], corners[1], corners[2] }) <= vertices.size())
			{
				if (keep)
				{
					link::McTriangle triangle{};
					for (int k = 0; k < 3; ++k)
					{
						for (int axis = 0; axis < 3; ++axis)
						{
							triangle.v[k * 3 + axis] = float(vertices[corners[k] - 1][axis]);
						}
					}
					triangles.push_back(triangle);
				}
				continue;
			}
		}
		aError = std::format("line {}: \"{}\" isn't a vertex, a triangle of vertices before it, or an object", lineNumber,
		                     line.substr(0, 80));
		return std::nullopt;
	}
	return triangles;
}
}
```
- [x] **Step 7: Edit `tools/mclink/McLinkScript.h`**

```cpp
//   track <seconds>                print Minecraft's player and the drive four times a second meanwhile
```
```cpp
//   track <seconds>                print Minecraft's player and the drive four times a second meanwhile
//   ground floor|city              what Minecraft gets as the game's shape (floor at first)
//   dump <collision.obj>           spike 1's saved collision as the city: its static shapes, around the stand-in V's feet
//   wait-ground [seconds]          until the city around the centre is all read and in the ring (30 s unless given)
```
```cpp
		Track,
		Status,
```
```cpp
		Track,
		Ground,
		Dump,
		WaitGround,
		Status,
```
```cpp
	std::string text;          // the command, or the file
```
```cpp
	std::string text;          // the command, the file, or the ground
```

- [x] **Step 8: Edit `tools/mclink/McLinkScript.cpp`**

```cpp
		}
		else if (verb == "floor" || verb == "status" || verb == "quit")
```
```cpp
		}
		else if (verb == "ground")
		{
			action.kind = Action::Kind::Ground;
			if (words.size() != 2 || (words[1] != "floor" && words[1] != "city"))
			{
				return fail("ground takes floor or city");
			}
			action.text = std::string(words[1]);
		}
		else if (verb == "dump")
		{
			action.kind = Action::Kind::Dump;
			action.text = std::string(rest);
			if (action.text.empty())
			{
				return fail("dump takes a collision.obj");
			}
		}
		else if (verb == "wait-ground")
		{
			action.kind = Action::Kind::WaitGround;
			action.numbers[0] = 30;
			if (words.size() > 1 && (!numbers(1) || action.numbers[0] <= 0))
			{
				return fail("wait-ground takes an optional number of seconds");
			}
		}
		else if (verb == "floor" || verb == "status" || verb == "quit")
```

- [x] **Step 9: Edit `tools/mclink/Main.cpp`**

```cpp
// mclink: a console stand-in for Cyberpunk's side of SkyCraft's protocol, for spike 4's step 1 and spike 5's. It runs
// BlockRender's link code against the real Minecraft, without the game (docs/superpowers/specs/2026-10-02-minecraft-
// link-design.md and 2026-10-03-collision-and-movement-design.md).
```
```cpp
// mclink: a console stand-in for Cyberpunk's side of SkyCraft's protocol, for spike 4's step 1 and spike 5's steps 1
// and 3. It runs BlockRender's link code against the real Minecraft, without the game
// (docs/superpowers/specs/2026-10-02-minecraft-link-design.md and 2026-10-03-collision-and-movement-design.md).
```
```cpp
// drive puts it, as the plugin teleports V. It reports what Minecraft sends, and refuses to start while another game
// side uses the mapping.
```
```cpp
// drive puts it, as the plugin teleports V. With the city's ground, Night City's shape comes from a dump spike 1 saved.
// It reports what Minecraft sends, and refuses to start while another game side uses the mapping.
#include "DumpObj.h"
```
```cpp
	    DriveStateName(s.drive), s.teleportAck, s.teleportSeq, s.eyeHeight, s.renderDelayMs, s.waitedFrames, s.driveSkipped);
}
```
```cpp
	    DriveStateName(s.drive), s.teleportAck, s.teleportSeq, s.eyeHeight, s.renderDelayMs, s.waitedFrames, s.driveSkipped);
	Say("ground: the {}, epoch {}; {}; {} batches read ({} unread), {} regions sent ({} triangles); the worker: {} regions, {} voxel "
	    "blocks, {:.1f} ms, {} waiting; ring: {:.2f} MB, {} messages waiting, {} dropped",
	    GroundName(s.ground), s.collisionEpoch,
	    s.cityCentre ? std::format("around region {} {} {}, {} regions known, {} batches to read", s.cityCentre->x, s.cityCentre->y,
	                               s.cityCentre->z, s.cityKnown, s.cityWaiting)
	                 : "no centre yet",
	    s.cityBatches, s.cityUnread, s.cityRegions, s.cityTriangles, s.workerRegions, s.workerBlocks, s.workerMs, s.workerJobs,
	    double(s.collisionBytes) / 1048576.0, s.collisionWaiting, s.collisionDropped);
}

// Spike 1's saved collision as Night City's ground (step 3's offline half): the triangles touching each box read.
struct DumpGround : GroundSource
{
	bool Read(const McBox& aBox, std::vector<McTriangle>& aOut) override
	{
		const double low[3] = { aBox.low.x, aBox.low.y, aBox.low.z };
		const double high[3] = { aBox.high.x, aBox.high.y, aBox.high.z };
		for (const McTriangle& triangle : triangles)
		{
			bool touches = true;
			for (int axis = 0; axis < 3 && touches; ++axis)
			{
				touches = std::max({ triangle.v[axis], triangle.v[3 + axis], triangle.v[6 + axis] }) >= low[axis] &&
				          std::min({ triangle.v[axis], triangle.v[3 + axis], triangle.v[6 + axis] }) <= high[axis];
			}
			if (touches)
			{
				aOut.push_back(triangle);
			}
		}
		return true;
	}

	std::vector<McTriangle> triangles;  // Minecraft axes
};
```
```cpp
	bool   hud = true;
```
```cpp
	bool   hud = true;
	DumpGround dump;
```
```cpp
		link.Update(frame, settings, report);
```
```cpp
		link.Update(frame, settings, report);
		link.ReadGround(now, dump, 1.5);  // the plugin's budget a frame
```
```cpp
			case Action::Kind::Floor: link.RequestFloor(); break;
```
```cpp
			case Action::Kind::Floor: link.RequestFloor(); break;
			case Action::Kind::Ground: settings.ground = action.text == "city" ? Ground::City : Ground::Floor; break;
			case Action::Kind::Dump:
			{
				// Its triangles are relative to the feet of V when it was dumped: they go around the stand-in V's.
				std::ifstream     file(action.text, std::ios::binary);
				std::stringstream obj;
				obj << file.rdbuf();
				const auto triangles = file ? mclink::ParseDumpObj(obj.str(), "s0_static_", error) : std::nullopt;
				if (!triangles)
				{
					Say("line {}: can't load {}: {}", action.line, action.text, file ? error : "it can't be read");
					exitCode = 1;
					next = script->size();
					break;
				}
				dump.triangles = *triangles;
				for (McTriangle& triangle : dump.triangles)
				{
					for (int k = 0; k < 3; ++k)
					{
						triangle.v[k * 3] = float(triangle.v[k * 3] + feet.x);
						triangle.v[k * 3 + 1] = float(triangle.v[k * 3 + 1] + feet.y);
						triangle.v[k * 3 + 2] = float(triangle.v[k * 3 + 2] + feet.z);
					}
				}
				Say("dump: {} static triangles from {}, around Minecraft {:.2f} {:.2f} {:.2f}", dump.triangles.size(), action.text, feet.x,
				    feet.y, feet.z);
				break;
			}
```
```cpp
				next = script->size();
			}
```
```cpp
				next = script->size();
			}
			break;
		case Action::Kind::WaitGround:
			done = state.ground == Ground::City && state.cityKnown > 0 && state.cityWaiting == 0 && state.workerJobs == 0 &&
			       state.collisionWaiting == 0;
			if (done)
			{
				Say("the city around region {} {} {} is all in the ring: {} regions", state.cityCentre->x, state.cityCentre->y,
				    state.cityCentre->z, state.cityKnown);
			}
			else if (elapsed > action.numbers[0])
			{
				Say("line {}: the city wasn't all sent within {} s", action.line, action.numbers[0]);
				exitCode = 1;
				done = true;
			}
```

- [x] **Step 10: Edit `CMakeLists.txt` to add the source to `mclink` and the tests**

```cmake
add_executable(mclink
	tools/mclink/Main.cpp
	tools/mclink/McLinkScript.cpp
)
```
```cmake
add_executable(mclink
	tools/mclink/Main.cpp
	tools/mclink/DumpObj.cpp
	tools/mclink/McLinkScript.cpp
)
```
```cmake
	tests/TestDumpObj.cpp
	tools/mclink/McLinkScript.cpp
)
```
```cmake
	tests/TestDumpObj.cpp
	tools/mclink/DumpObj.cpp
	tools/mclink/McLinkScript.cpp
)
```

- [x] **Step 11: Write `tools/mclink/city.txt`**

```text
# Spike 5's step 3, offline: Night City's shape, from spike 1's saved dumps, through the exporter's own filing,
# voxelizing and messages, against the real Minecraft (docs/superpowers/specs/2026-10-03-collision-and-movement-
# design.md, "Step 3"). Start Minecraft first: this waits for its world. Each dump goes where the stand-in V stands,
# east of spike 3's corner and clear of the blocks built there, at the corner's height. W (26) walks.
status
wait-world 180
ground city
at 80.5 6.14 2007.5
look 270 0
dump E:\Cyberpunk2077\red4ext\plugins\PhysXProbe\dumps\2026-10-01_14-37-07\collision.obj
link on
type /gamemode creative
wait-ground 30
status
build on
track 1
# Spot B. West: off the kerb about 2 blocks out, 0.2 down, onto the road.
look 90 0
down 26
track 1
up 26
track 0.5
# East: back up the kerb, then the barrier about 4 blocks east of the start stops the walk.
look 270 0
down 26
track 3
up 26
track 0.5
# TNT 3 blocks west, over the sidewalk: it lands on the street's voxels, and goes off there.
type /summon minecraft:tnt ~-3 ~4 ~
wait 5
build off
# The ramp, 100 blocks farther east. The floor for a moment, so that the city starts over with the ramp's triangles: the
# batches read while V moves would go out empty otherwise, and only those next to V are read again.
ground floor
at 180.5 6.14 2007.5
dump E:\Cyberpunk2077\red4ext\plugins\PhysXProbe\dumps\2026-10-01_14-46-55\collision.obj
ground city
wait-ground 30
build on
track 1
# North: up the ramp's gentle slope, about 0.3 in 17 blocks.
look 180 0
down 26
track 4
up 26
track 0.5
# East: its wall is right beside V.
look 270 0
down 26
track 1
up 26
build off
status
quit
```
- [x] **Step 12: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `317 test(s), 0 failure(s)`.

- [x] **Step 13: Run mclink without Minecraft**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && printf 'status\nquit\n' > build/selfcheck.txt \
  && ./build/RelWithDebInfo/mclink.exe build/selfcheck.txt | head -2; echo "exit: ${PIPESTATUS[0]}"
```
Expected: `the mapping: created (process …); waiting for Minecraft`, then `Minecraft: absent, …`, and `exit: 0`. The game
and Minecraft must both be closed: either would hold the mapping, and Minecraft would link.

- [x] **Step 14: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: mclink sends Night City's shape from spike 1's dumps" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 28: In Minecraft: step 3's offline half

Step 3's offline half, with the user (the spec's "Step 3"): `city.txt` through `mclink`, against SkyCraft's unchanged
mod in the `CyberCraft` instance, without the game. Its expectations come from spot B's and the ramp's own triangles,
walked by a rough model of Minecraft's player (0.6 wide, 1.8 tall, stepping up to 0.6), so distances and heights are
roughly right rather than exact.
- **Spot B** (dump `2026-10-01_14-37-07`, 64,192 static triangles): V stood on a sidewalk 2 m east of its kerb, with the
  road 0.18 m lower beyond it, and a concrete barrier about 4 m east, past which the ground drops 6 to 7 m.
- **The ramp** (`2026-10-01_14-46-55`, 21,242): V stood against the ramp's east wall, on a lane that climbs about 0.33 m
  over 17 m northward.
- **If a check fails,** stop and find the cause with superpowers:systematic-debugging before going on. Fixes become new
  tasks.

**Files:**
- Modify: `docs/spikes/2026-10-03-collision-and-movement-results.md`

- [x] **Step 1: Check that the game isn't running**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game: it would hold the mapping.

- [x] **Step 2: Ask the user to start Minecraft**

The user starts the `CyberCraft` instance from the Modrinth App, and says when its title screen shows. Then:
```bash
grep -E "SkyCraft:" "/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log" | tail -3
```
Expected: `SkyCraft: can't open Skyrim's shared memory yet (Windows error 2: …)`: waiting for a game side.

- [x] **Step 3: Run step 3's offline script**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/step3 && cd build/step3 \
  && ../RelWithDebInfo/mclink.exe ../../tools/mclink/city.txt | tee city.log; echo "exit: ${PIPESTATUS[0]}"
```
It takes about a minute once Minecraft is in its world, and up to four if it opens the world first: give the command 6
minutes. Expected:
- **The city, not the floor:** `the ground is the city now`, `dump: 64192 static triangles from …14-37-07…, around
  Minecraft 80.50 6.14 2007.50`, `Minecraft is there (process …)` and `the city starts over, as collision epoch …`; no
  line starting `floor` until the ramp, where the script switches to the floor for a moment so that the city starts
  over with the ramp's triangles.
- **All of spot B's area sent:** `the city around region 10 0 250 is all in the ring: 1152 regions`, a few seconds
  after the link; the status's `ground:` line then has 144 batches or more read, about 108,000 triangles, and about
  24,000 voxel blocks, with nothing waiting or dropped.
- **Released at once:** after `build on`, `Minecraft let its player go after …` within a second, as the floor's voxels
  did, and in the first `track` lines Minecraft's feet at about 6.14, `on ground`.
- **The kerb:** walking west, the feet drop by about 0.2 at about 2 blocks out (x about 78.5), and stay `on ground` on
  the road.
- **The barrier:** walking east, the feet climb back up the kerb, then stop at x about 84.5, about 4 blocks east of the
  start, and stay there for the rest of the walk.
- **The TNT:** `explosion at Minecraft 81.5 …, radius 4.0`, with y within a quarter block of 6.3: it landed on the
  sidewalk's voxels. Fallen through them, it would go off tens of blocks lower.
- **The ramp:** `the ground is the floor now`, `dump: 21242 static triangles from …14-46-55…`, `the ground is the city
  now`, then `the city around region 22 0 250 is all in the ring: 1152 regions`. Walking north, the feet rise steadily,
  by about 0.3 in 17 blocks; walking east, x stays within a few tenths of a block of 180.5: the ramp's wall.
- **No `build mode ends:` line,** and `exit: 0`.

- [x] **Step 4: Ask the user to stop Minecraft**

Its window is hidden while it's linked, so the user stops it from the Modrinth App.

- [x] **Step 5: Record the offline half in `docs/spikes/2026-10-03-collision-and-movement-results.md`**

A "Step 3, offline" section: the versions; each check above with its outcome and numbers (the release's time, the
kerb's drop and where, where the barrier stopped the walk, the explosion's height, the ramp's climb); the status's
ground figures; and anything that surprised, and what it means for the game.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-03-collision-and-movement-results.md \
  && git commit -q -m "collision-and-movement: step 3's offline half" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 29: Spike 1's PhysX reader moves into BlockRender, with its tests

The spec's "src/physx/": spike 1's reader moves in, trimmed to what Part 2 uses. These files move as they are, with
their tests, and only their includes and the comments that named the probe change:
- `PxHeaders.h`: the PhysX 3.4.2 headers, with spike 1's `PxShape` placeholder kept in `extern/physx-3.4.2`;
- `GameAbi.*`: the audited DLLs' identities and vtables, and `CheckObject`;
- `ShapeTriangles.*`: shapes to triangles, with `AppendTriangles`, which checks each mesh before reading it;
- `OriginWatch.*`: PhysX's origin from V's controller, and which capsule can be it.

BlockRender's own `ReadableMemory` (`src/Readable.h`), which asks `QueryWorkingSetEx` first, replaces the probe's.
`CMakeLists.txt` gains PhysX's headers as system includes and `PX_PHYSX_STATIC_LIB`, as the probe's had. The probe's
census, dumps and session stay in `spikes/physx-probe`.

**Files:**
- Create (copied from `spikes/physx-probe/`): `src/physx/PxHeaders.h`, `src/physx/GameAbi.h`, `src/physx/GameAbi.cpp`,
  `src/physx/ShapeTriangles.h`, `src/physx/ShapeTriangles.cpp`, `src/physx/OriginWatch.h`, `src/physx/OriginWatch.cpp`,
  `tests/TestAudit.cpp`, `tests/TestGameAbi.cpp`, `tests/TestPrimitives.cpp`, `tests/TestMeshes.cpp`,
  `tests/TestHeightfield.cpp`, `tests/TestOriginWatch.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Copy `../physx-probe/src/physx/PxHeaders.h` to `src/physx/PxHeaders.h`**

- [x] **Step 2: Copy `../physx-probe/src/physx/GameAbi.h` to `src/physx/GameAbi.h`**

- [x] **Step 3: Copy `../physx-probe/src/physx/GameAbi.cpp` to `src/physx/GameAbi.cpp`**

- [x] **Step 4: Copy `../physx-probe/src/physx/ShapeTriangles.h` to `src/physx/ShapeTriangles.h`**

- [x] **Step 5: Copy `../physx-probe/src/physx/ShapeTriangles.cpp` to `src/physx/ShapeTriangles.cpp`**

- [x] **Step 6: Copy `../physx-probe/src/OriginWatch.h` to `src/physx/OriginWatch.h`**

- [x] **Step 7: Copy `../physx-probe/src/OriginWatch.cpp` to `src/physx/OriginWatch.cpp`**

- [x] **Step 8: Copy `../physx-probe/tests/TestAudit.cpp` to `tests/TestAudit.cpp`**

- [x] **Step 9: Copy `../physx-probe/tests/TestGameAbi.cpp` to `tests/TestGameAbi.cpp`**

- [x] **Step 10: Copy `../physx-probe/tests/TestPrimitives.cpp` to `tests/TestPrimitives.cpp`**

- [x] **Step 11: Copy `../physx-probe/tests/TestMeshes.cpp` to `tests/TestMeshes.cpp`**

- [x] **Step 12: Copy `../physx-probe/tests/TestHeightfield.cpp` to `tests/TestHeightfield.cpp`**

- [x] **Step 13: Copy `../physx-probe/tests/TestOriginWatch.cpp` to `tests/TestOriginWatch.cpp`**

- [x] **Step 14: Edit `src/physx/PxHeaders.h`**

```cpp
// The NVIDIA PhysX 3.4.2 public headers the probe uses. CMake defines PX_PHYSX_STATIC_LIB, which empties the
// core API macro. The probe only calls PhysX through virtual tables or inline header code, so the built DLL
// imports nothing from PhysX3*.dll (check with dumpbin /imports).
```
```cpp
// The NVIDIA PhysX 3.4.2 public headers BlockRender uses, as spike 1's probe did. CMake defines PX_PHYSX_STATIC_LIB,
// which empties the core API macro. BlockRender only calls PhysX through virtual tables or inline header code, so the
// built DLL imports nothing from PhysX3*.dll (check with dumpbin /imports).
```

- [x] **Step 15: Edit `src/physx/GameAbi.h`**

```cpp
#include "physx/PxHeaders.h"
#include "physx/Readable.h"
```
```cpp
#include "Readable.h"
#include "physx/PxHeaders.h"
```
```cpp
// vtables of every class the probe calls into. The probe calls nothing on an object whose vtable isn't one of these.
```
```cpp
// vtables of every class spike 1's probe called into, and BlockRender does now. Nothing is called on an object whose
// vtable isn't one of these.
```
```cpp
// Checks a PhysX object before the probe touches it. At least its first 16 bytes must be readable: the vtable
```
```cpp
// Checks a PhysX object before it's touched. At least its first 16 bytes must be readable: the vtable
```

- [x] **Step 16: Edit `src/physx/ShapeTriangles.h`**

```cpp
#include "physx/GameAbi.h"
#include "physx/PxHeaders.h"
#include "physx/Readable.h"
```
```cpp
#include "Readable.h"
#include "physx/GameAbi.h"
#include "physx/PxHeaders.h"
```

- [x] **Step 17: Edit `src/physx/OriginWatch.cpp`**

```cpp
#include "OriginWatch.h"
```
```cpp
#include "physx/OriginWatch.h"
```

- [x] **Step 18: Edit `tests/TestAudit.cpp`**

```cpp
// The probe's side of the PhysX ABI, checked against what the audit verified in the game's DLLs
// (docs/spikes/2026-10-01-physx-abi-audit.md): the vtable slot of every call the probe makes, the layout of the hit
```
```cpp
// Our side of the PhysX ABI, checked against what the audit verified in the game's DLLs
// (docs/spikes/2026-10-01-physx-abi-audit.md): the vtable slot of every call made, the layout of the hit
```
```cpp
// fake object is outside standard C++, but it compiles to exactly what the probe does to the game's objects.
```
```cpp
// fake object is outside standard C++, but it compiles to exactly what BlockRender does to the game's objects.
```
```cpp
// The sizes of the structs the game copies into the probe's memory, or reads from it.
```
```cpp
// The sizes of the structs the game copies into our memory, or reads from it.
```
```cpp
	// The probe reads two fields straight from game objects: PxBase's type ID and PxActor::userData.
```
```cpp
	// Two fields are read straight from game objects: PxBase's type ID and PxActor::userData.
```
```cpp
	// What the game's getters copy into the probe's structs, byte for byte.
```
```cpp
	// What the game's getters copy into our structs, byte for byte.
```

- [x] **Step 19: Edit `tests/TestGameAbi.cpp`**

```cpp
using namespace physx;
```
```cpp
using namespace physx;
using namespace cpmc;
```

- [x] **Step 20: Edit `tests/TestOriginWatch.cpp`**

```cpp
#include "OriginWatch.h"
```
```cpp
#include "physx/OriginWatch.h"
```

- [x] **Step 21: Edit `CMakeLists.txt`**

```cmake
set(SKYCRAFT_DIR "${REPO_ROOT}/extern/skycraft" CACHE PATH "SkyCraft's protocol header")
set(CP2077_DIR "" CACHE PATH "Cyberpunk 2077 folder; when set, each build copies the plugin into red4ext/plugins/BlockRender")

# Shared settings. Streamline's and SkyCraft's headers are SYSTEM, which keeps their warnings out of ours.
```
```cmake
set(SKYCRAFT_DIR "${REPO_ROOT}/extern/skycraft" CACHE PATH "SkyCraft's protocol header")
set(PHYSX_DIR "${REPO_ROOT}/extern/physx-3.4.2" CACHE PATH "PhysX 3.4.2 headers (PhysX_3.4/Include, PxShared/include)")
set(CP2077_DIR "" CACHE PATH "Cyberpunk 2077 folder; when set, each build copies the plugin into red4ext/plugins/BlockRender")

# Shared settings. Streamline's, SkyCraft's and PhysX's headers are SYSTEM, which keeps their warnings out of ours.
# PX_PHYSX_STATIC_LIB empties PhysX's import macro: PhysX is called only through its virtual tables and inline header
# code (spike 1's rule), so nothing is imported from its DLLs.
```
```cmake
target_compile_definitions(render_settings INTERFACE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(render_settings INTERFACE src)
target_include_directories(render_settings SYSTEM INTERFACE "${STREAMLINE_DIR}/include" "${SKYCRAFT_DIR}")
```
```cmake
target_compile_definitions(render_settings INTERFACE WIN32_LEAN_AND_MEAN NOMINMAX PX_PHYSX_STATIC_LIB)
target_include_directories(render_settings INTERFACE src)
target_include_directories(render_settings SYSTEM INTERFACE "${STREAMLINE_DIR}/include" "${SKYCRAFT_DIR}"
	"${PHYSX_DIR}/PhysX_3.4/Include" "${PHYSX_DIR}/PxShared/include")
```
```cmake
	src/link/GroundWorker.cpp
```
```cmake
	src/link/GroundWorker.cpp
	src/physx/GameAbi.cpp
	src/physx/ShapeTriangles.cpp
	src/physx/OriginWatch.cpp
```
```cmake
	tests/TestGroundWorker.cpp
```
```cmake
	tests/TestGroundWorker.cpp
	tests/TestAudit.cpp
	tests/TestGameAbi.cpp
	tests/TestPrimitives.cpp
	tests/TestMeshes.cpp
	tests/TestHeightfield.cpp
	tests/TestOriginWatch.cpp
```

- [x] **Step 22: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `348 test(s), 0 failure(s)`: the 31 tests that came with the reader pass here as
they did in the probe.

- [x] **Step 23: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: spike 1's PhysX reader moves into BlockRender, with its tests" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 30: The scene watch's record of who calls scene 0, and its audited slots

Step 3's in-game half opens with a watch of scene 0 (the spec's "The scene watch"), since the exporter will read the
scene every frame: it settles which of the game's threads write to scene 0, and when. This task is its record, pure:
- **`SceneCall`** names the 16 calls hooked, and `InfoOf` gives each its slot in `PxScene`'s vtable and the RVA in
  `PhysX3_x64.dll` of the function the audit found there (Task 32's addendum).
- **`SceneRecorder`:** for each thread, how many of each call it made, how many while OnUpdate ran, how many with the
  write lock held by that thread, and how many while OnUpdate ran without it; and the last 16 origin shifts. The hooks
  run on the game's threads, so it takes no lock and allocates nothing: fixed tables of atomics, 32 threads told apart
  and any more counted together.
- **The write lock before the count:** a thread may hold the write lock as the hooks go in. Its unlock then comes
  without a lock, so a thread's lock count never goes below none, and calls count only once `Start` is called, a
  second later (Task 31), when that lock has been let go. Otherwise every later locked call of that thread would look
  unlocked, and Task 33 would stop for nothing. `SceneWatchFollowsTheWriteLockBeforeItCounts` checks both.
- **`WatchSummary`:** the status's line, by thread and by kind of call: add, remove, step, fetch, lock and shift.
- **`TestAudit`'s `SceneWatchHooksTheGamesSlots`** checks that our headers put each call on the slot the audit found
  in the game.

**Files:**
- Create: `src/physx/SceneWatch.h`, `src/physx/SceneWatch.cpp`, `tests/TestSceneWatch.cpp`
- Modify: `tests/TestAudit.cpp`, `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestSceneWatch.cpp`**

```cpp
#include "Harness.h"

#include "physx/SceneWatch.h"

#include <thread>
#include <vector>

using namespace cpmc::px;

TEST(SceneWatchCountsEachThreadsCalls)
{
	SceneRecorder recorder;
	recorder.Start();
	recorder.Note(SceneCall::Simulate, 100, false);
	recorder.Note(SceneCall::FetchResults, 100, false);
	recorder.Note(SceneCall::AddActor, 200, true);  // while OnUpdate ran, without the write lock
	recorder.Lock(200, 1);
	recorder.Note(SceneCall::AddActor, 200, true);  // with it
	recorder.Note(SceneCall::RemoveActor, 200, false);
	recorder.Lock(200, -1);
	recorder.Note(SceneCall::AddActors, 200, true);
	const auto threads = recorder.Threads();
	CHECK_EQ(threads.size(), size_t(2));
	if (threads.size() == 2)
	{
		CHECK_EQ(threads[0].id, 100u);
		CHECK_EQ(threads[0].calls[size_t(SceneCall::Simulate)], 1u);
		CHECK_EQ(threads[0].calls[size_t(SceneCall::FetchResults)], 1u);
		CHECK_EQ(threads[1].id, 200u);
		const size_t add = size_t(SceneCall::AddActor);
		CHECK_EQ(threads[1].calls[add], 2u);
		CHECK_EQ(threads[1].inUpdate[add], 2u);
		CHECK_EQ(threads[1].locked[add], 1u);
		CHECK_EQ(threads[1].inUpdateUnlocked[add], 1u);
		CHECK_EQ(threads[1].locked[size_t(SceneCall::RemoveActor)], 1u);
		CHECK_EQ(threads[1].inUpdateUnlocked[size_t(SceneCall::AddActors)], 1u);
	}
}

TEST(SceneWatchFollowsTheWriteLockBeforeItCounts)
{
	// The hooks went in while thread 200 held the write lock. Its unlock comes without a lock, and leaves it unlocked
	// rather than owing one; its calls before the start, under a lock the hooks didn't see taken, aren't counted.
	SceneRecorder recorder;
	recorder.Note(SceneCall::AddActor, 200, true);
	recorder.Lock(200, -1);
	recorder.Lock(200, 1);
	recorder.Start();
	recorder.Note(SceneCall::AddActor, 200, true);
	recorder.Lock(200, -1);
	recorder.Note(SceneCall::RemoveActor, 200, true);
	const auto threads = recorder.Threads();
	CHECK_EQ(threads.size(), size_t(1));
	if (threads.size() == 1)
	{
		const size_t add = size_t(SceneCall::AddActor);
		CHECK_EQ(threads[0].calls[add], 1u);
		CHECK_EQ(threads[0].locked[add], 1u);
		CHECK_EQ(threads[0].inUpdateUnlocked[add], 0u);
		CHECK_EQ(threads[0].inUpdateUnlocked[size_t(SceneCall::RemoveActor)], 1u);
	}
	recorder.Reset();
	CHECK(!recorder.Started());
}

TEST(SceneWatchSummarizesByThreadAndKind)
{
	SceneRecorder recorder;
	recorder.Start();
	CHECK_STR(WatchSummary(recorder.Threads(), 100, recorder.Shifts(), recorder.ShiftCount()), "no calls yet; no shifts");
	for (int i = 0; i < 3; ++i)
	{
		recorder.Note(SceneCall::Simulate, 100, false);
		recorder.Note(SceneCall::FetchResults, 100, false);
	}
	recorder.Note(SceneCall::AddActor, 200, true);
	recorder.Note(SceneCall::AddAggregate, 200, false);
	recorder.Note(SceneCall::LockWrite, 200, false);
	recorder.Note(SceneCall::UnlockWrite, 200, false);  // not counted: it mirrors lockWrite
	CHECK_STR(WatchSummary(recorder.Threads(), 100, recorder.Shifts(), recorder.ShiftCount()),
	          "thread 100, the main thread: step 3, fetch 3; thread 200: add 2 (1 in OnUpdate, 1 of them without the write lock), "
	          "lock 1; no shifts");
}

TEST(SceneWatchKeepsTheLatestShifts)
{
	SceneRecorder recorder;
	recorder.Start();
	for (int i = 0; i < 20; ++i)
	{
		recorder.NoteShift(100, false, 0.0f, float(i), 0.0f);
	}
	recorder.NoteShift(300, true, 0.0f, -1024.0f, 0.0f);
	CHECK_EQ(recorder.ShiftCount(), 21ull);
	const auto shifts = recorder.Shifts();
	CHECK_EQ(shifts.size(), size_t(SceneRecorder::kShifts));
	CHECK_EQ(shifts.front().y, 5.0f);  // the latest 16 of 21, oldest first
	CHECK_EQ(shifts.back().y, -1024.0f);
	CHECK_STR(WatchSummary({}, 100, shifts, recorder.ShiftCount()),
	          "no calls yet; 21 shifts, the last (0, -1024, 0) on thread 300, in OnUpdate");
}

TEST(SceneWatchCountsThreadsPastItsTableTogether)
{
	SceneRecorder recorder;
	recorder.Start();
	for (uint32_t thread = 1; thread <= 40; ++thread)
	{
		recorder.Note(SceneCall::Simulate, thread, false);
	}
	const auto threads = recorder.Threads();
	CHECK_EQ(threads.size(), SceneRecorder::kThreads);
	CHECK_EQ(threads.back().id, 0u);  // the other threads
	CHECK_EQ(threads.back().calls[size_t(SceneCall::Simulate)], uint32_t(40 - (SceneRecorder::kThreads - 1)));
	CHECK(WatchSummary(threads, 1, {}, 0).find("; the other threads: step 9; no shifts") != std::string::npos);
	recorder.Reset();
	CHECK(recorder.Threads().empty());
}

TEST(SceneWatchTakesCallsFromManyThreadsAtOnce)
{
	SceneRecorder            recorder;
	std::vector<std::thread> threads;
	recorder.Start();
	for (uint32_t thread = 1; thread <= 8; ++thread)
	{
		threads.emplace_back([&recorder, thread] {
			for (int i = 0; i < 10000; ++i)
			{
				recorder.Note(SceneCall::FetchResults, thread, i % 2 == 0);
			}
		});
	}
	for (std::thread& thread : threads)
	{
		thread.join();
	}
	const auto counted = recorder.Threads();
	CHECK_EQ(counted.size(), size_t(8));
	for (const auto& thread : counted)
	{
		CHECK_EQ(thread.calls[size_t(SceneCall::FetchResults)], 10000u);
		CHECK_EQ(thread.inUpdate[size_t(SceneCall::FetchResults)], 5000u);
	}
}
```
- [x] **Step 2: Edit `tests/TestAudit.cpp`: the failing test**

```cpp
#include "physx/PxHeaders.h"
```
```cpp
#include "physx/PxHeaders.h"
#include "physx/SceneWatch.h"
```
```cpp
using namespace physx;
```
```cpp
using namespace physx;
using cpmc::px::InfoOf;
using cpmc::px::SceneCall;
```
```cpp
	Expect("PxScene::unlockRead", SlotOf<PxScene>([](PxScene& a) { a.unlockRead(); }), 106);
```
```cpp
	Expect("PxScene::unlockRead", SlotOf<PxScene>([](PxScene& a) { a.unlockRead(); }), 106);
}

TEST(SceneWatchHooksTheGamesSlots)
{
	// The scene watch's hooks go on these slots, and call on with these signatures (physx/SceneHooks.cpp).
	Fake                       dummy;  // an actor, an aggregate and a pruning structure: never touched
	PxActor&                   actor = reinterpret_cast<PxActor&>(dummy);
	PxActor* const             actors[1] = { &actor };
	PxAggregate&               aggregate = reinterpret_cast<PxAggregate&>(dummy);
	const PxPruningStructure&  pruning = reinterpret_cast<const PxPruningStructure&>(dummy);
	const PxContactPairHeader* pairs = nullptr;
	PxU32                      pairCount = 0;
	const auto expect = [](SceneCall aCall, int aSlot) { Expect(InfoOf(aCall).name, aSlot, int(InfoOf(aCall).slot)); };
	expect(SceneCall::AddActor, SlotOf<PxScene>([&](PxScene& a) { a.addActor(actor); }));
	expect(SceneCall::AddActorsPruning, SlotOf<PxScene>([&](PxScene& a) { a.addActors(pruning); }));
	expect(SceneCall::AddActors, SlotOf<PxScene>([&](PxScene& a) { a.addActors(actors, 1); }));
	expect(SceneCall::RemoveActor, SlotOf<PxScene>([&](PxScene& a) { a.removeActor(actor); }));
	expect(SceneCall::RemoveActors, SlotOf<PxScene>([&](PxScene& a) { a.removeActors(actors, 1); }));
	expect(SceneCall::AddAggregate, SlotOf<PxScene>([&](PxScene& a) { a.addAggregate(aggregate); }));
	expect(SceneCall::RemoveAggregate, SlotOf<PxScene>([&](PxScene& a) { a.removeAggregate(aggregate); }));
	expect(SceneCall::Simulate, SlotOf<PxScene>([](PxScene& a) { a.simulate(1.0f / 60.0f); }));
	expect(SceneCall::Advance, SlotOf<PxScene>([](PxScene& a) { a.advance(); }));
	expect(SceneCall::Collide, SlotOf<PxScene>([](PxScene& a) { a.collide(1.0f / 60.0f); }));
	expect(SceneCall::FetchResults, SlotOf<PxScene>([](PxScene& a) { (void)a.fetchResults(); }));
	expect(SceneCall::FetchResultsStart, SlotOf<PxScene>([&](PxScene& a) { (void)a.fetchResultsStart(pairs, pairCount); }));
	expect(SceneCall::FetchResultsFinish, SlotOf<PxScene>([](PxScene& a) { a.fetchResultsFinish(); }));
	expect(SceneCall::LockWrite, SlotOf<PxScene>([](PxScene& a) { a.lockWrite(); }));
	expect(SceneCall::UnlockWrite, SlotOf<PxScene>([](PxScene& a) { a.unlockWrite(); }));
	expect(SceneCall::ShiftOrigin, SlotOf<PxScene>([](PxScene& a) { a.shiftOrigin(PxVec3(0.0f, 1024.0f, 0.0f)); }));
```

- [x] **Step 3: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestOriginWatch.cpp
	tests/TestDrive.cpp
```
```cmake
	tests/TestOriginWatch.cpp
	tests/TestSceneWatch.cpp
	tests/TestDrive.cpp
```

- [x] **Step 4: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'physx/SceneWatch.h'`.

- [x] **Step 5: Write `src/physx/SceneWatch.h`**

```cpp
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Step 3's watch of scene 0 (spike 5's "The scene watch"): which of the game's threads add and remove actors, step and
// fetch the simulation, take the write lock and shift the origin, and whether while OnUpdate runs. That decides where
// the exporter can read the scene. This is the record the hooks keep, and its summary; SceneHooks.h installs the hooks.
namespace cpmc::px
{
// The calls watched, in PxScene's vtable order.
enum class SceneCall : uint8_t
{
	AddActor,
	AddActorsPruning,  // addActors(const PxPruningStructure&)
	AddActors,         // addActors(PxActor* const*, PxU32)
	RemoveActor,
	RemoveActors,
	AddAggregate,
	RemoveAggregate,
	Simulate,
	Advance,
	Collide,
	FetchResults,
	FetchResultsStart,
	FetchResultsFinish,
	LockWrite,
	UnlockWrite,
	ShiftOrigin,
};

constexpr size_t kSceneCalls = 16;

// A call's name, its slot in PxScene's vtable, and the RVA in PhysX3_x64.dll of the function the audit found there.
struct SceneCallInfo
{
	const char* name;
	uint32_t    slot;
	uint32_t    rva;
};

const SceneCallInfo& InfoOf(SceneCall aCall);

// The hooks' record: for each thread, how many of each call it made, how many while OnUpdate ran, and how many with the
// write lock held by that thread; and the last origin shifts. The hooks run on the game's threads, so it takes no lock
// and allocates nothing: fixed tables of atomics.
class SceneRecorder
{
public:
	static constexpr size_t kThreads = 32;  // threads told apart; any after them count as one, with the id 0
	static constexpr size_t kShifts = 16;   // the latest shifts kept

	// From the hooks. aThread: GetCurrentThreadId(). aInUpdate: OnUpdate was running. Counted only once started.
	void Note(SceneCall aCall, uint32_t aThread, bool aInUpdate);
	void NoteShift(uint32_t aThread, bool aInUpdate, float aX, float aY, float aZ);
	// The thread has taken the write lock (+1, after lockWrite returns), or is letting it go (-1, before unlockWrite),
	// followed from the moment the hooks go in. A thread that held the lock then lets it go without having taken it
	// here: it's left unlocked, not owing a lock.
	void Lock(uint32_t aThread, int aBy);

	// From the main thread, a second after the hooks go in: calls count from here. Any write lock a thread held as the
	// hooks went in has been let go by then, so it can't make that thread's later calls look unlocked.
	void Start() { started_.store(true); }
	bool Started() const { return started_.load(); }

	// From the main thread, with no hook installed. Not started again until Start.
	void Reset();

	struct Thread
	{
		uint32_t                           id = 0;
		std::array<uint32_t, kSceneCalls> calls{};
		std::array<uint32_t, kSceneCalls> inUpdate{};          // of those, while OnUpdate ran
		std::array<uint32_t, kSceneCalls> locked{};            // with the thread holding the write lock
		std::array<uint32_t, kSceneCalls> inUpdateUnlocked{};  // while OnUpdate ran, without it
	};

	struct Shift
	{
		uint32_t thread = 0;
		bool     inUpdate = false;
		float    x = 0.0f, y = 0.0f, z = 0.0f;
	};

	std::vector<Thread> Threads() const;  // the threads that made a call, in the order they first did
	std::vector<Shift>  Shifts() const;   // the latest kShifts, oldest first
	uint64_t            ShiftCount() const { return shifts_.load(); }

private:
	struct Slot
	{
		std::atomic<uint32_t>                          id{ 0 };
		std::atomic<int32_t>                           lockDepth{ 0 };
		std::array<std::atomic<uint32_t>, kSceneCalls> calls{};
		std::array<std::atomic<uint32_t>, kSceneCalls> inUpdate{};
		std::array<std::atomic<uint32_t>, kSceneCalls> locked{};
		std::array<std::atomic<uint32_t>, kSceneCalls> inUpdateUnlocked{};
	};

	struct ShiftSlot
	{
		std::atomic<uint32_t> thread{ 0 };
		std::atomic<bool>     inUpdate{ false };
		std::atomic<float>    x{ 0.0f }, y{ 0.0f }, z{ 0.0f };
	};

	Slot& SlotOf(uint32_t aThread);

	std::array<Slot, kThreads>     slots_;  // the last is for every thread past the others
	std::array<ShiftSlot, kShifts> shiftSlots_;
	std::atomic<uint64_t>          shifts_{ 0 };
	std::atomic<bool>              started_{ false };
};

// The status's line for a watch: each thread's calls by kind (add, remove, step, fetch, lock and shift), and of each
// kind, how many came while OnUpdate ran and how many of those without the write lock; then the shifts.
std::string WatchSummary(const std::vector<SceneRecorder::Thread>& aThreads, uint32_t aMainThread,
                         const std::vector<SceneRecorder::Shift>& aShifts, uint64_t aShiftCount);
}
```
- [x] **Step 6: Write `src/physx/SceneWatch.cpp`**

```cpp
#include "physx/SceneWatch.h"

#include <algorithm>
#include <format>
#include <initializer_list>

namespace cpmc::px
{
namespace
{
// The audit's addendum (docs/spikes/2026-10-01-physx-abi-audit.md): each slot's function, by its strings and its code,
// in PhysX3_x64.dll with the audited timestamp and size.
constexpr SceneCallInfo kInfo[kSceneCalls] = {
	{ "addActor", 10, 0x520d0 },
	{ "addActors(pruning structure)", 11, 0x54a30 },
	{ "addActors", 12, 0x54a20 },
	{ "removeActor", 13, 0x521e0 },
	{ "removeActors", 14, 0x54a90 },
	{ "addAggregate", 15, 0x524e0 },
	{ "removeAggregate", 16, 0x525d0 },
	{ "simulate", 56, 0x53760 },
	{ "advance", 57, 0x537a0 },
	{ "collide", 58, 0x53880 },
	{ "fetchResults", 61, 0x53980 },
	{ "fetchResultsStart", 62, 0x53a50 },
	{ "fetchResultsFinish", 64, 0x53d30 },
	{ "lockWrite", 107, 0x55180 },
	{ "unlockWrite", 108, 0x55280 },
	{ "shiftOrigin", 116, 0x55360 },
};

// The summary's kinds of call. unlockWrite only mirrors lockWrite, so it isn't counted.
struct Kind
{
	const char*                      name;
	std::initializer_list<SceneCall> calls;
};

const Kind kKinds[] = {
	{ "add", { SceneCall::AddActor, SceneCall::AddActorsPruning, SceneCall::AddActors, SceneCall::AddAggregate } },
	{ "remove", { SceneCall::RemoveActor, SceneCall::RemoveActors, SceneCall::RemoveAggregate } },
	{ "step", { SceneCall::Simulate, SceneCall::Advance, SceneCall::Collide } },
	{ "fetch", { SceneCall::FetchResults, SceneCall::FetchResultsStart, SceneCall::FetchResultsFinish } },
	{ "lock", { SceneCall::LockWrite } },
	{ "shift", { SceneCall::ShiftOrigin } },
};
}

const SceneCallInfo& InfoOf(SceneCall aCall)
{
	return kInfo[size_t(aCall)];
}

SceneRecorder::Slot& SceneRecorder::SlotOf(uint32_t aThread)
{
	for (size_t i = 0; i + 1 < kThreads; ++i)
	{
		const uint32_t id = slots_[i].id.load(std::memory_order_acquire);
		if (id == aThread)
		{
			return slots_[i];
		}
		if (id == 0)
		{
			uint32_t expected = 0;
			if (slots_[i].id.compare_exchange_strong(expected, aThread) || expected == aThread)
			{
				return slots_[i];
			}
		}
	}
	return slots_[kThreads - 1];
}

void SceneRecorder::Note(SceneCall aCall, uint32_t aThread, bool aInUpdate)
{
	if (!started_.load(std::memory_order_relaxed))
	{
		return;
	}
	Slot&        slot = SlotOf(aThread);
	const size_t call = size_t(aCall);
	const bool   locked = slot.lockDepth.load(std::memory_order_relaxed) > 0;
	slot.calls[call].fetch_add(1, std::memory_order_relaxed);
	if (aInUpdate)
	{
		slot.inUpdate[call].fetch_add(1, std::memory_order_relaxed);
	}
	if (locked)
	{
		slot.locked[call].fetch_add(1, std::memory_order_relaxed);
	}
	if (aInUpdate && !locked)
	{
		slot.inUpdateUnlocked[call].fetch_add(1, std::memory_order_relaxed);
	}
}

void SceneRecorder::NoteShift(uint32_t aThread, bool aInUpdate, float aX, float aY, float aZ)
{
	if (!started_.load(std::memory_order_relaxed))
	{
		return;
	}
	ShiftSlot& shift = shiftSlots_[shifts_.fetch_add(1) % kShifts];
	shift.thread.store(aThread);
	shift.inUpdate.store(aInUpdate);
	shift.x.store(aX);
	shift.y.store(aY);
	shift.z.store(aZ);
}

void SceneRecorder::Lock(uint32_t aThread, int aBy)
{
	// Never below none: a lock the thread held before the hooks went in is let go here without having been taken.
	std::atomic<int32_t>& depth = SlotOf(aThread).lockDepth;
	int32_t               now = depth.load(std::memory_order_relaxed);
	while (!depth.compare_exchange_weak(now, std::max(now + aBy, 0), std::memory_order_relaxed))
	{
	}
}

void SceneRecorder::Reset()
{
	started_.store(false);
	for (Slot& slot : slots_)
	{
		slot.id.store(0);
		slot.lockDepth.store(0);
		for (size_t call = 0; call < kSceneCalls; ++call)
		{
			slot.calls[call].store(0);
			slot.inUpdate[call].store(0);
			slot.locked[call].store(0);
			slot.inUpdateUnlocked[call].store(0);
		}
	}
	shifts_.store(0);
}

std::vector<SceneRecorder::Thread> SceneRecorder::Threads() const
{
	std::vector<Thread> threads;
	for (const Slot& slot : slots_)
	{
		Thread thread;
		thread.id = slot.id.load();
		bool any = false;
		for (size_t call = 0; call < kSceneCalls; ++call)
		{
			thread.calls[call] = slot.calls[call].load();
			thread.inUpdate[call] = slot.inUpdate[call].load();
			thread.locked[call] = slot.locked[call].load();
			thread.inUpdateUnlocked[call] = slot.inUpdateUnlocked[call].load();
			any = any || thread.calls[call] != 0;
		}
		if (any)
		{
			threads.push_back(thread);
		}
	}
	return threads;
}

std::vector<SceneRecorder::Shift> SceneRecorder::Shifts() const
{
	const uint64_t     count = shifts_.load();
	std::vector<Shift> shifts;
	for (uint64_t i = count > kShifts ? count - kShifts : 0; i < count; ++i)
	{
		const ShiftSlot& slot = shiftSlots_[i % kShifts];
		shifts.push_back({ slot.thread.load(), slot.inUpdate.load(), slot.x.load(), slot.y.load(), slot.z.load() });
	}
	return shifts;
}

std::string WatchSummary(const std::vector<SceneRecorder::Thread>& aThreads, uint32_t aMainThread,
                         const std::vector<SceneRecorder::Shift>& aShifts, uint64_t aShiftCount)
{
	std::string out;
	for (const SceneRecorder::Thread& thread : aThreads)
	{
		std::string kinds;
		for (const Kind& kind : kKinds)
		{
			uint64_t calls = 0, inUpdate = 0, unlocked = 0;
			for (const SceneCall call : kind.calls)
			{
				calls += thread.calls[size_t(call)];
				inUpdate += thread.inUpdate[size_t(call)];
				unlocked += thread.inUpdateUnlocked[size_t(call)];
			}
			if (calls != 0)
			{
				kinds += std::format("{}{} {}", kinds.empty() ? "" : ", ", kind.name, calls);
				if (inUpdate != 0)
				{
					kinds += std::format(" ({} in OnUpdate, {} of them without the write lock)", inUpdate, unlocked);
				}
			}
		}
		const std::string who = thread.id == 0            ? "the other threads"
		                        : thread.id == aMainThread ? std::format("thread {}, the main thread", thread.id)
		                                                   : std::format("thread {}", thread.id);
		out += std::format("{}{}: {}", out.empty() ? "" : "; ", who, kinds);
	}
	if (out.empty())
	{
		out = "no calls yet";
	}
	if (aShiftCount == 0 || aShifts.empty())
	{
		return out + "; no shifts";
	}
	const SceneRecorder::Shift& last = aShifts.back();
	return out + std::format("; {} shift{}, the last ({:g}, {:g}, {:g}) on thread {}, {} OnUpdate", aShiftCount, aShiftCount == 1 ? "" : "s",
	                         last.x, last.y, last.z, last.thread, last.inUpdate ? "in" : "outside");
}
}
```
- [x] **Step 7: Edit `CMakeLists.txt` to add the source**

```cmake
	src/physx/OriginWatch.cpp
	src/link/Drive.cpp
```
```cmake
	src/physx/OriginWatch.cpp
	src/physx/SceneWatch.cpp
	src/link/Drive.cpp
```

- [x] **Step 8: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `355 test(s), 0 failure(s)`.

- [x] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the scene watch's record of who calls scene 0, and its audited slots" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 31: BlockRender 0.12.0 watches who calls scene 0, on request

The scene watch in the game:
- **`physx/Session`:** the audited build (the two DLLs' identities, `PxGetPhysics`'s object, plausible tolerances), kept
  once it passes, and scene 0, checked each time it's asked for. Without the game it says why, which
  `PhysxSessionSaysWhyWithoutTheGame` checks.
- **`physx/SceneHooks`:** a hook on each of the 16 audited slots, with the slot's own signature. Each records its call
  if it's on scene 0 (the small five-plane scenes share the vtable) and calls on. `lockWrite`'s hook counts the lock as
  held once the original returns, and `unlockWrite`'s as let go before it calls on. Before patching, each slot must hold
  the audited function, or nothing is patched; each original is saved before its slot points at the hook that calls it.
  The hooks follow the write lock at once, and the first OnUpdate a second or more after they go in starts the count.
  `RemoveSceneHooks` puts the originals back; the hooks stay in memory, since the plugin is pinned.
- **The plugin:** `scenewatch on|off`; OnUpdate marks its start and end for the hooks; the status's `scenewatch=` line,
  once the watch has been on, sums the calls by thread, and keeps the last watch's summary after `off`, which also goes
  to the log; the hooks come out at unload. Version 0.12.0.

**Files:**
- Create: `src/physx/Session.h`, `src/physx/Session.cpp`, `src/physx/SceneHooks.h`, `src/physx/SceneHooks.cpp`,
  `tests/TestPhysxSession.cpp`
- Modify: `src/Requests.h`, `src/Requests.cpp`, `src/Status.h`, `src/Status.cpp`, `src/Plugin.cpp`,
  `tests/TestRequests.cpp`, `tests/TestStatus.cpp`, `CMakeLists.txt`

- [x] **Step 1: Edit `tests/TestRequests.cpp`: the failing test**

```cpp
	CHECK(error.find("printable ASCII") != std::string::npos);
}
```
```cpp
	CHECK(error.find("printable ASCII") != std::string::npos);
}

TEST(RequestsForTheSceneWatch)
{
	std::string error;
	const auto  on = cpmc::ParseRequest("scenewatch on", error);
	CHECK(on && on->kind == cpmc::Request::Kind::SceneWatch && on->what == "on");
	const auto off = cpmc::ParseRequest("scenewatch off\n", error);
	CHECK(off && off->kind == cpmc::Request::Kind::SceneWatch && off->what == "off");
	CHECK(!cpmc::ParseRequest("scenewatch", error));
	CHECK_STR(error, "scenewatch takes on or off");
}
```

- [x] **Step 2: Edit `tests/TestStatus.cpp`: the failing test**

```cpp
	                  "ratio not measured yet (0 of 5 turns); Minecraft's player in the air"));
}
```
```cpp
	                  "ratio not measured yet (0 of 5 turns); Minecraft's player in the air"));
}

TEST(StatusWritesTheSceneWatch)
{
	cpmc::StatusReport report;
	CHECK(cpmc::FormatStatus(report).find("scenewatch=") == std::string::npos);  // never on: no line
	report.sceneWatch = "on for 12 s: thread 9120, the main thread: step 720, fetch 720; no shifts";
	CHECK(Has(cpmc::FormatStatus(report), "scenewatch=on for 12 s: thread 9120, the main thread: step 720, fetch 720; no shifts"));
}
```

- [x] **Step 3: Write the failing test, `tests/TestPhysxSession.cpp`**

```cpp
#include "Harness.h"

#include "physx/Session.h"

TEST(PhysxSessionSaysWhyWithoutTheGame)
{
	// The tests have no PhysX loaded: nothing is called, and the session says so.
	cpmc::px::Session session;
	std::string       why;
	CHECK(session.Scene0(why) == nullptr);
	CHECK_STR(why, "PhysX3_x64.dll isn't loaded");
}
```
- [x] **Step 4: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestSceneWatch.cpp
	tests/TestDrive.cpp
```
```cmake
	tests/TestSceneWatch.cpp
	tests/TestPhysxSession.cpp
	tests/TestDrive.cpp
```

- [x] **Step 5: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'physx/Session.h'`.

- [x] **Step 6: Edit `src/Requests.h`**

```cpp
//   drivelog <seconds>         (spike 5: build mode's drive, a row a frame, for 1 to 60 seconds)
```
```cpp
//   drivelog <seconds>         (spike 5: build mode's drive, a row a frame, for 1 to 60 seconds)
//   scenewatch on|off          (spike 5's step 3: who calls scene 0, and when)
```
```cpp
		DriveLog,
```
```cpp
		DriveLog,
		SceneWatch,
```

- [x] **Step 7: Edit `src/Requests.cpp`**

```cpp
	if (command == "link" || command == "build")
```
```cpp
	if (command == "link" || command == "build" || command == "scenewatch")
```
```cpp
		return Request{ command == "link" ? Request::Kind::Link : Request::Kind::Build, std::string(words[1]) };
```
```cpp
		const Request::Kind kind = command == "link"    ? Request::Kind::Link
		                           : command == "build" ? Request::Kind::Build
		                                                : Request::Kind::SceneWatch;
		return Request{ kind, std::string(words[1]) };
```

- [x] **Step 8: Edit `src/Status.h`**

```cpp
	std::string               lastRequest;
```
```cpp
	std::string               lastRequest;
	std::string               sceneWatch;  // spike 5's step 3: the watch's summary; empty until it's first on
```

- [x] **Step 9: Edit `src/Status.cpp`**

```cpp
	out += std::format("request={}\n", OneLine(aReport.lastRequest));
```
```cpp
	out += std::format("request={}\n", OneLine(aReport.lastRequest));
	if (!aReport.sceneWatch.empty())
	{
		out += std::format("scenewatch={}\n", OneLine(aReport.sceneWatch));
	}
```

- [x] **Step 10: Write `src/physx/Session.h`**

```cpp
#pragma once

#include "Readable.h"
#include "physx/GameAbi.h"
#include "physx/PxHeaders.h"

#include <string>

// The game's PhysX, through spike 1's checks (spike 5's "The exporter" and "The scene watch"): the two DLLs with the
// audited identities, PxGetPhysics's object, and scene 0, Night City's, each checked against the audit before anything
// is called on it. Main thread only.
namespace cpmc::px
{
class Session
{
public:
	// Scene 0, checked; null, with aWhy saying why, if PhysX isn't the audited build or has no scene 0 now.
	physx::PxScene* Scene0(std::string& aWhy);

	const ModuleBases& Bases() const { return bases_; }  // where the audited DLLs are, once Scene0 has found them
	ReadableMemory&    Memory() { return memory_; }

private:
	// The DLLs' identities, PxGetPhysics and its object, and the tolerances: once they pass, they're kept.
	bool CheckBuild(std::string& aWhy);

	ModuleBases       bases_;
	physx::PxPhysics* physics_ = nullptr;
	ReadableMemory    memory_;
};
}
```
- [x] **Step 11: Write `src/physx/Session.cpp`**

```cpp
#include "physx/Session.h"

#include <Windows.h>

#include <format>

using namespace physx;

namespace cpmc::px
{
namespace
{
constexpr PxU32 kMostScenes = 16;

// PhysX's tolerances as a real build has them: length from 0.01 to 100, speed from 0.1 to 1,000.
bool Plausible(const PxTolerancesScale& aScale)
{
	return aScale.length >= 0.01f && aScale.length <= 100.0f && aScale.speed >= 0.1f && aScale.speed <= 1000.0f;
}

std::string Narrow(const wchar_t* aText)
{
	std::string out;
	for (; *aText; ++aText)
	{
		out.push_back(char(*aText));  // the DLLs' names are ASCII
	}
	return out;
}
}

bool Session::CheckBuild(std::string& aWhy)
{
	if (physics_)
	{
		return true;
	}
	HMODULE modules[2] = {};
	for (size_t i = 0; i < 2; ++i)
	{
		const AuditedModule& audited = kAuditedModules[i];
		modules[i] = GetModuleHandleW(audited.name);
		if (!modules[i])
		{
			aWhy = Narrow(audited.name) + " isn't loaded";
			return false;
		}
		const std::optional<ModuleId> id = ModuleIdentity(modules[i]);
		if (!id || !(*id == audited.id))
		{
			aWhy = std::format("{} isn't the audited build: {}", Narrow(audited.name),
			                   id ? std::format("timestamp 0x{:x}, image size 0x{:x}", id->timestamp, id->imageSize) : "no PE header");
			return false;
		}
	}
	const ModuleBases bases{ reinterpret_cast<uintptr_t>(modules[0]), reinterpret_cast<uintptr_t>(modules[1]) };
	using GetPhysicsFn = PxPhysics& (*)();
	const auto getPhysics = reinterpret_cast<GetPhysicsFn>(GetProcAddress(modules[0], "PxGetPhysics"));
	if (!getPhysics)
	{
		aWhy = "PhysX3_x64.dll doesn't export PxGetPhysics";
		return false;
	}
	PxPhysics* const  physics = &getPhysics();
	const ObjectCheck check = CheckObject(memory_, bases, physics);
	if (check.verdict != ObjectVerdict::Ok || check.cls != AuditedClass::Physics)
	{
		aWhy = "PxGetPhysics's object isn't the audited PxPhysics";
		return false;
	}
	const PxTolerancesScale& scale = physics->getTolerancesScale();
	if (!memory_.Range(&scale, sizeof scale) || !Plausible(scale))
	{
		aWhy = "PhysX's tolerances aren't readable, or not a real build's";
		return false;
	}
	bases_ = bases;
	physics_ = physics;
	return true;
}

PxScene* Session::Scene0(std::string& aWhy)
{
	memory_.Clear();  // memory changes from frame to frame
	if (!CheckBuild(aWhy))
	{
		return nullptr;
	}
	const PxU32 count = physics_->getNbScenes();
	if (count == 0 || count > kMostScenes)
	{
		aWhy = std::format("PhysX has {} scenes", count);
		return nullptr;
	}
	PxScene* scenes[kMostScenes] = {};
	if (physics_->getScenes(scenes, kMostScenes) == 0 || !scenes[0])
	{
		aWhy = "PhysX lists no scene";
		return nullptr;
	}
	const ObjectCheck check = CheckObject(memory_, bases_, scenes[0]);
	if (check.verdict != ObjectVerdict::Ok || check.cls != AuditedClass::Scene)
	{
		aWhy = "scene 0 isn't the audited PxScene";
		return nullptr;
	}
	return scenes[0];
}
}
```
- [x] **Step 12: Write `src/physx/SceneHooks.h`**

```cpp
#pragma once

#include "physx/PxHeaders.h"
#include "physx/SceneWatch.h"

#include <cstdint>
#include <string>

// Step 3's scene watch, in the game (spike 5's "The scene watch"): hooks on the 16 audited slots of scene 0's vtable.
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
```
- [x] **Step 13: Write `src/physx/SceneHooks.cpp`**

```cpp
#include "physx/SceneHooks.h"

#include "hooks/VtablePatch.h"

#include <Windows.h>

#include <array>
#include <atomic>
#include <format>

using namespace physx;

namespace cpmc::px
{
namespace
{
std::atomic<PxScene*>          g_scene{ nullptr };  // scene 0: the only one whose calls are recorded
std::atomic<bool>              g_inUpdate{ false };
SceneRecorder                  g_recorder;
VtablePatcher                  g_patcher;
bool                           g_installed = false;
uint64_t                       g_installedAt = 0;  // GetTickCount64() as the hooks went in
std::array<void*, kSceneCalls> g_originals{};      // each slot's function, which its hook calls on

// How long after the hooks go in the count starts (SceneRecorder::Start). A thread holds the write lock for a moment at
// most, since the game's own reads wait for it.
constexpr uint64_t kSettleMs = 1000;

template<class Fn>
Fn Original(SceneCall aCall)
{
	return reinterpret_cast<Fn>(g_originals[size_t(aCall)]);
}

bool Watched(PxScene* aThis)
{
	return aThis == g_scene.load(std::memory_order_relaxed);
}

void Record(PxScene* aThis, SceneCall aCall)
{
	if (Watched(aThis))
	{
		g_recorder.Note(aCall, GetCurrentThreadId(), g_inUpdate.load(std::memory_order_relaxed));
	}
}

// One hook a slot, each with its function's own parameters, as PxScene.h declares them. On x64, a member function takes
// its object first, as a free function takes its first parameter, so the hooks can call the originals this way.
void AddActor(PxScene* aThis, PxActor& aActor)
{
	Record(aThis, SceneCall::AddActor);
	Original<void (*)(PxScene*, PxActor&)>(SceneCall::AddActor)(aThis, aActor);
}

void AddActorsPruning(PxScene* aThis, const PxPruningStructure& aPruning)
{
	Record(aThis, SceneCall::AddActorsPruning);
	Original<void (*)(PxScene*, const PxPruningStructure&)>(SceneCall::AddActorsPruning)(aThis, aPruning);
}

void AddActors(PxScene* aThis, PxActor* const* aActors, PxU32 aCount)
{
	Record(aThis, SceneCall::AddActors);
	Original<void (*)(PxScene*, PxActor* const*, PxU32)>(SceneCall::AddActors)(aThis, aActors, aCount);
}

void RemoveActor(PxScene* aThis, PxActor& aActor, bool aWake)
{
	Record(aThis, SceneCall::RemoveActor);
	Original<void (*)(PxScene*, PxActor&, bool)>(SceneCall::RemoveActor)(aThis, aActor, aWake);
}

void RemoveActors(PxScene* aThis, PxActor* const* aActors, PxU32 aCount, bool aWake)
{
	Record(aThis, SceneCall::RemoveActors);
	Original<void (*)(PxScene*, PxActor* const*, PxU32, bool)>(SceneCall::RemoveActors)(aThis, aActors, aCount, aWake);
}

void AddAggregate(PxScene* aThis, PxAggregate& aAggregate)
{
	Record(aThis, SceneCall::AddAggregate);
	Original<void (*)(PxScene*, PxAggregate&)>(SceneCall::AddAggregate)(aThis, aAggregate);
}

void RemoveAggregate(PxScene* aThis, PxAggregate& aAggregate, bool aWake)
{
	Record(aThis, SceneCall::RemoveAggregate);
	Original<void (*)(PxScene*, PxAggregate&, bool)>(SceneCall::RemoveAggregate)(aThis, aAggregate, aWake);
}

void Simulate(PxScene* aThis, PxReal aTime, PxBaseTask* aTask, void* aScratch, PxU32 aScratchSize, bool aControl)
{
	Record(aThis, SceneCall::Simulate);
	Original<void (*)(PxScene*, PxReal, PxBaseTask*, void*, PxU32, bool)>(SceneCall::Simulate)(aThis, aTime, aTask, aScratch,
	                                                                                           aScratchSize, aControl);
}

void Advance(PxScene* aThis, PxBaseTask* aTask)
{
	Record(aThis, SceneCall::Advance);
	Original<void (*)(PxScene*, PxBaseTask*)>(SceneCall::Advance)(aThis, aTask);
}

void Collide(PxScene* aThis, PxReal aTime, PxBaseTask* aTask, void* aScratch, PxU32 aScratchSize, bool aControl)
{
	Record(aThis, SceneCall::Collide);
	Original<void (*)(PxScene*, PxReal, PxBaseTask*, void*, PxU32, bool)>(SceneCall::Collide)(aThis, aTime, aTask, aScratch,
	                                                                                          aScratchSize, aControl);
}

bool FetchResults(PxScene* aThis, bool aBlock, PxU32* aError)
{
	Record(aThis, SceneCall::FetchResults);
	return Original<bool (*)(PxScene*, bool, PxU32*)>(SceneCall::FetchResults)(aThis, aBlock, aError);
}

bool FetchResultsStart(PxScene* aThis, const PxContactPairHeader*& aPairs, PxU32& aCount, bool aBlock)
{
	Record(aThis, SceneCall::FetchResultsStart);
	return Original<bool (*)(PxScene*, const PxContactPairHeader*&, PxU32&, bool)>(SceneCall::FetchResultsStart)(aThis, aPairs,
	                                                                                                             aCount, aBlock);
}

void FetchResultsFinish(PxScene* aThis, PxU32* aError)
{
	Record(aThis, SceneCall::FetchResultsFinish);
	Original<void (*)(PxScene*, PxU32*)>(SceneCall::FetchResultsFinish)(aThis, aError);
}

void LockWrite(PxScene* aThis, const char* aFile, PxU32 aLine)
{
	Record(aThis, SceneCall::LockWrite);
	Original<void (*)(PxScene*, const char*, PxU32)>(SceneCall::LockWrite)(aThis, aFile, aLine);
	if (Watched(aThis))
	{
		g_recorder.Lock(GetCurrentThreadId(), 1);  // held from here
	}
}

void UnlockWrite(PxScene* aThis)
{
	Record(aThis, SceneCall::UnlockWrite);
	if (Watched(aThis))
	{
		g_recorder.Lock(GetCurrentThreadId(), -1);
	}
	Original<void (*)(PxScene*)>(SceneCall::UnlockWrite)(aThis);
}

void ShiftOrigin(PxScene* aThis, const PxVec3& aShift)
{
	Record(aThis, SceneCall::ShiftOrigin);
	if (Watched(aThis))
	{
		g_recorder.NoteShift(GetCurrentThreadId(), g_inUpdate.load(std::memory_order_relaxed), aShift.x, aShift.y, aShift.z);
	}
	Original<void (*)(PxScene*, const PxVec3&)>(SceneCall::ShiftOrigin)(aThis, aShift);
}

// In SceneCall's order.
const std::array<void*, kSceneCalls> kHooks = {
	reinterpret_cast<void*>(&AddActor),
	reinterpret_cast<void*>(&AddActorsPruning),
	reinterpret_cast<void*>(&AddActors),
	reinterpret_cast<void*>(&RemoveActor),
	reinterpret_cast<void*>(&RemoveActors),
	reinterpret_cast<void*>(&AddAggregate),
	reinterpret_cast<void*>(&RemoveAggregate),
	reinterpret_cast<void*>(&Simulate),
	reinterpret_cast<void*>(&Advance),
	reinterpret_cast<void*>(&Collide),
	reinterpret_cast<void*>(&FetchResults),
	reinterpret_cast<void*>(&FetchResultsStart),
	reinterpret_cast<void*>(&FetchResultsFinish),
	reinterpret_cast<void*>(&LockWrite),
	reinterpret_cast<void*>(&UnlockWrite),
	reinterpret_cast<void*>(&ShiftOrigin),
};
}

bool InstallSceneHooks(PxScene* aScene, uintptr_t aPhysx3, std::string& aWhy)
{
	if (g_installed)
	{
		return true;
	}
	void* const* const vtable = *reinterpret_cast<void* const* const*>(aScene);
	for (size_t i = 0; i < kSceneCalls; ++i)
	{
		const SceneCallInfo& info = InfoOf(SceneCall(i));
		if (reinterpret_cast<uintptr_t>(vtable[info.slot]) != aPhysx3 + info.rva)
		{
			aWhy = std::format("scene slot {} ({}) doesn't hold the audited function: another hook may be there", info.slot, info.name);
			return false;
		}
	}
	g_recorder.Reset();
	g_scene.store(aScene);
	for (size_t i = 0; i < kSceneCalls; ++i)
	{
		const uint32_t slot = InfoOf(SceneCall(i)).slot;
		g_originals[i] = vtable[slot];  // before the slot points at the hook that calls it
		if (!g_patcher.Patch(aScene, slot, kHooks[i]))
		{
			g_patcher.RestoreAll();
			aWhy = std::format("scene slot {} couldn't be written", slot);
			return false;
		}
	}
	g_installedAt = GetTickCount64();
	g_installed = true;
	return true;
}

void RemoveSceneHooks()
{
	g_patcher.RestoreAll();
	g_installed = false;
}

bool SceneHooksInstalled()
{
	return g_installed;
}

void SetInUpdate(bool aIn)
{
	g_inUpdate.store(aIn, std::memory_order_relaxed);
	if (aIn && g_installed && !g_recorder.Started() && GetTickCount64() - g_installedAt >= kSettleMs)
	{
		g_recorder.Start();
	}
}

const SceneRecorder& Recorder()
{
	return g_recorder;
}
}
```
- [x] **Step 14: Edit `src/Plugin.cpp`**

```cpp
// build mode: the movement keys are Minecraft's too, and V is teleported to its player's eye every frame.
```
```cpp
// build mode: the movement keys are Minecraft's too, and V is teleported to its player's eye every frame; and it reads
// Night City's shape for Minecraft from the game's PhysX.
```
```cpp
#include "log/FrameLogData.h"
```
```cpp
#include "log/FrameLogData.h"
#include "physx/SceneHooks.h"
#include "physx/Session.h"
```
```cpp
constexpr const char* kVersion = "0.11.0";
```
```cpp
constexpr const char* kVersion = "0.12.0";
```
```cpp
cpmc::DriveLog        g_driveLog;
```
```cpp
cpmc::DriveLog        g_driveLog;

// Spike 5's Night City in Minecraft (its spec's "Part 2"). Main thread only.
cpmc::px::Session     g_physx;            // the game's PhysX, checked against the audit
std::optional<double> g_watchSince;       // scenewatch on: since when (SteadySeconds)
std::string           g_lastWatch;        // the last watch's summary, once it's off
DWORD                 g_mainThread = 0;   // OnUpdate's
```
```cpp
// The command in aFolder's blockrender.request, if there is one: in the plugin's folder, where the agent writes it, or
```
```cpp
// The scene watch's summary (spike 5's "The scene watch"): each thread's calls on scene 0.
std::string WatchSummary()
{
	const cpmc::px::SceneRecorder& recorder = cpmc::px::Recorder();
	return cpmc::px::WatchSummary(recorder.Threads(), g_mainThread, recorder.Shifts(), recorder.ShiftCount());
}

// scenewatch on|off (spike 5's "The scene watch"): hooks on scene 0's calls, which the status sums by thread.
void SceneWatch(bool aOn)
{
	if (!aOn)
	{
		if (!cpmc::px::SceneHooksInstalled())
		{
			return Report("scenewatch off: it's off already");
		}
		cpmc::px::RemoveSceneHooks();
		g_lastWatch = std::format("{:.0f} s: {}", cpmc::SteadySeconds() - *g_watchSince, WatchSummary());
		g_watchSince.reset();
		Log("scene watch, {}", g_lastWatch);
		return Report("scenewatch off: the hooks are out, and the status keeps what the watch saw");
	}
	if (cpmc::px::SceneHooksInstalled())
	{
		return Report("scenewatch on: it's on already");
	}
	std::string           why;
	physx::PxScene* const scene = g_physx.Scene0(why);
	if (!scene || !cpmc::px::InstallSceneHooks(scene, g_physx.Bases().physx3, why))
	{
		return Report("scenewatch: " + why);
	}
	g_watchSince = cpmc::SteadySeconds();
	Report(std::format("scenewatch on: {} of scene 0's calls hooked; from a second on, the status sums them by thread",
	                   cpmc::px::kSceneCalls));
}

// The command in aFolder's blockrender.request, if there is one: in the plugin's folder, where the agent writes it, or
```
```cpp
		                                         : "drivelog: one is already recording");
		break;
	}
```
```cpp
		                                         : "drivelog: one is already recording");
		break;
	case cpmc::Request::Kind::SceneWatch: SceneWatch(request->what == "on"); break;
	}
```
```cpp
	report.lastRequest = g_lastRequest;
```
```cpp
	report.lastRequest = g_lastRequest;
	report.sceneWatch = g_watchSince ? std::format("on for {:.0f} s: {}", aNow - *g_watchSince, WatchSummary())
	                    : g_lastWatch.empty() ? std::string()
	                                          : "off; the last watch, " + g_lastWatch;
```
```cpp
{
	const auto   now = Clock::now();
```
```cpp
{
	cpmc::px::SetInUpdate(true);  // the scene watch's hooks note calls the game makes meanwhile
	g_mainThread = GetCurrentThreadId();
	const auto   now = Clock::now();
```
```cpp
	}
	return false;
```
```cpp
	}
	cpmc::px::SetInUpdate(false);
	return false;
```
```cpp
		cpmc::slhooks::Remove();
```
```cpp
		cpmc::slhooks::Remove();
		cpmc::px::RemoveSceneHooks();
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 11, 0);
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 12, 0);
```

- [x] **Step 15: Edit `CMakeLists.txt`: the sources, and version 0.12.0**

```cmake
project(BlockRender VERSION 0.11.0 LANGUAGES C CXX)
```
```cmake
project(BlockRender VERSION 0.12.0 LANGUAGES C CXX)
```
```cmake
	src/physx/SceneWatch.cpp
	src/link/Drive.cpp
```
```cmake
	src/physx/SceneWatch.cpp
	src/physx/Session.cpp
	src/link/Drive.cpp
```
```cmake
	src/input/RawInputHook.cpp
)
```
```cmake
	src/input/RawInputHook.cpp
	src/physx/SceneHooks.cpp
)
```

- [x] **Step 16: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `358 test(s), 0 failure(s)`.

- [x] **Step 17: Check that BlockRender imports nothing from PhysX**

```bash
DUMPBIN="$(ls "/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/"*/bin/Hostx64/x64/dumpbin.exe | head -1)"
cd /d/Cyberpunk-Minecraft/spikes/block-render && MSYS_NO_PATHCONV=1 "$DUMPBIN" -imports -nologo build\\RelWithDebInfo\\BlockRender.dll \
  | grep -ci "physx"
```
Expected: `0`.

- [x] **Step 18: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: BlockRender 0.12.0 watches who calls scene 0, on request" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 32: The PhysX audit's addendum: the scene watch's slots

The spec's "The audit", before step 3: the 16 slots the scene watch hooks, each found in the game's `PhysX3_x64.dll`
(the audited build) by its strings and its code, offline, with Python and Capstone, as spike 1's audit was done. The
calls that make and manage part 3's bodies are left for Part 3, before step 4.

**Files:**
- Modify: `docs/spikes/2026-10-01-physx-abi-audit.md`

- [x] **Step 1: Edit `docs/spikes/2026-10-01-physx-abi-audit.md`: the addendum, at its end**

```markdown
4. **When to dump:** the automatic dump must wait until a save is loaded, not just until a player object exists.
```
```markdown
4. **When to dump:** the automatic dump must wait until a save is loaded, not just until a player object exists.

## Addendum: spike 5's scene watch

Date: 2026-10-04. The same DLLs and the same method: our layout from `cl /d1reportAllClassLayout` on `PxScene.h`, and
the game's `PhysX3_x64.dll` read with Python and Capstone. Spike 5's step 3 hooks 16 of `PxScene`'s slots to watch which
threads write to scene 0, and when (`spikes/block-render/src/physx/SceneHooks.cpp`). Each hook calls the original
through the slot's own signature, so each slot's function had to be the one NVIDIA's header puts there.

| Slot | Call | Function's RVA | How it was told |
|---|---|---|---|
| 10 | `addActor(PxActor&)` | 0x520d0 | "PxScene::addActor(): actor is in a pruning structure …" |
| 11 | `addActors(const PxPruningStructure&)` | 0x54a30 | tests the structure's valid flag at +0x50, then "PxScene::addActors(): Provided pruning structure is not valid." |
| 12 | `addActors(PxActor* const*, PxU32)` | 0x54a20 | `xor r9d, r9d; jmp` to the internal add: no pruning structure, as `NpScene::addActors` passes `NULL` |
| 13 | `removeActor(PxActor&, bool)` | 0x521e0 | "PxScene::removeActor(): Actor … not assigned to scene …" |
| 14 | `removeActors(PxActor* const*, PxU32, bool)` | 0x54a90 | "PxScene::removeActors(): Actor … not assigned to scene …" |
| 15 | `addAggregate(PxAggregate&)` | 0x524e0 | "PxScene::addAggregate(): Aggregate already assigned to a scene." |
| 16 | `removeAggregate(PxAggregate&, bool)` | 0x525d0 | "PxScene::removeAggregate(): Aggregate … not assigned to scene …" |
| 56 | `simulate(PxReal, PxBaseTask*, void*, PxU32, bool)` | 0x53760 | "PxScene::simulate: Simulation is still processing last simulate call …"; calls the shared step with 3 |
| 57 | `advance(PxBaseTask*)` | 0x537a0 | "PxScene::advance: advance() called illegally! …" |
| 58 | `collide(PxReal, PxBaseTask*, void*, PxU32, bool)` | 0x53880 | the same shared step as `simulate`, called with 1 |
| 61 | `fetchResults(bool, PxU32*)` | 0x53980 | tests the simulation's stage at +0x1f54, then "PxScene::fetchResults: fetchResults() called illegally! …" |
| 62 | `fetchResultsStart(const PxContactPairHeader*&, PxU32&, bool)` | 0x53a50 | "PXScene::fetchResultsStart: fetchResultsStart() called illegally! …" |
| 64 | `fetchResultsFinish(PxU32*)` | 0x53d30 | calls only the fetch's last part (0x59730), which `fetchResults` (61) ends with; `fetchResultsStart` (62) shares its first part (0x59590) |
| 107 | `lockWrite(const char*, PxU32)` | 0x55180 | "PxScene::lockWrite() detected after a PxScene::lockRead(), lock upgrading is not supported …" |
| 108 | `unlockWrite()` | 0x55280 | "PxScene::unlockWrite() called without matching call to PxScene::lockWrite() …" |
| 116 | `shiftOrigin(const PxVec3&)` | 0x55360 | "PxScene::shiftOrigin() not allowed while simulation is running. …" |

What that settles:
- **Every slot is where NVIDIA's header puts it,** as our compiler lays it out: `TestAudit`'s `SceneWatchHooksTheGamesSlots`
  checks our side. The two `addActors` overloads sit in reverse order of declaration, as MSVC orders overloads, in the game
  and in our layout alike.
- **The calling convention:** on x64 a member function takes its object first, as a free function takes its first
  parameter, so a hook declared as a free function with the object first, then the declared parameters, calls the
  original correctly. `simulate` and `collide` take their time step in XMM1, which such a hook declares the same way. None
  of the 16 returns a struct, the one case where a member function passes its hidden return pointer differently.
- **The hooks check before they patch:** each slot must still hold the RVA above, plus where `PhysX3_x64.dll` is loaded.
  If another hook got there first, nothing is patched, and `scenewatch on` says which slot differs.

Still to audit before step 4: `PxPhysics::createRigidStatic` and `createShape`, `PxRigidActor::attachShape` and
`setGlobalPose`, `PxShape::setQueryFilterData`, `setSimulationFilterData` and `getMaterials`, `PxActor::getScene`, and
`PxBase::release` for actors and shapes.
```

- [x] **Step 2: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-01-physx-abi-audit.md \
  && git commit -q -m "collision-and-movement: the PhysX audit's addendum, the scene watch's slots" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 33: In the game: the follow-ups, and the scene watch

The first half of step 3 in the game, with the user, on BlockRender 0.12.0, which has no exporter yet: the ground is
the floor throughout.
- **Part 1's follow-ups:** the turn on by default, V's reason in a car, the eye height after a car, and the frame on
  entering build mode.
- **The scene watch** (the spec's "The scene watch"): a minute of walking and driving, across an origin line, with the
  hooks on. It shows which threads write to scene 0, and when; whether the game takes the write lock; and whether it
  calls `shiftOrigin` at the line. That settles where the exporter reads.

Spike 1's overlay, `physxprobe_hud`, still installed, shows V's distance to the nearest origin lines (512 + k × 1,024 m
on x and y). Spot B, where spike 1 dumped (world x −189.9, y 616.8), is 104 m north of the y = 512 line; spike 3's
corner is over 390 m from any. FreeFly (F7) stays off. Note what you see as you go: Step 9 records it. If a check fails,
stop and find the cause with superpowers:systematic-debugging before going on. Fixes become new tasks.

These commands come back, as in spike 4's in-game steps:
- **Send a request,** atomically: the plugin runs it within a second. Replace `scenewatch on` with the command, and send
  the next only once the status's `request=` line shows the last.
  ```bash
  P=/e/Cyberpunk2077/red4ext/plugins/BlockRender; printf '%s\n' "scenewatch on" > "$P/request.tmp" && mv "$P/request.tmp" "$P/blockrender.request"
  ```
- **The log's newest lines:**
  ```bash
  L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); tail -30 "/e/Cyberpunk2077/red4ext/logs/$L"
  ```
- **The status:**
  ```bash
  cat /e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud/status.txt
  ```

**Files:**
- Modify: `docs/spikes/2026-10-03-collision-and-movement-results.md`

- [x] **Step 1: Deploy (the game must not be running)**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game first, because a running game locks the DLL.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default -DCP2077_DIR=E:/Cyberpunk2077 > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C|error MSB"; \
  "$CMAKE" --preset default -DCP2077_DIR= > /dev/null; \
  cmp build/RelWithDebInfo/BlockRender.dll /e/Cyberpunk2077/red4ext/plugins/BlockRender/BlockRender.dll && echo installed
```
Expected: `installed`. The second configure clears `CP2077_DIR` again.

- [x] **Step 2: In the game: load, link, and the turn by default**

Ask the user to start the `CyberCraft` instance, then the game, load the free-roam save at spike 3's corner, and click
the overlay's Link button. Expected: in the log, `BlockRender 0.12.0 loaded`, then `link: Minecraft is there (process
…)`; in the status, the drive line says `turn on, ratio not measured yet (0 of 5 turns)` without anything being set.

Ask the user to turn V left and right a few times, pausing between turns, then to press F8 and turn the mouse.
Expected: `turn on, -0.0… degrees a count from … turns`, and in build mode V turns left and right with the mouse at
once, with no button clicked.

- [x] **Step 3: In the game: the frame on entering build mode**

Ask the user to press F8 to leave build mode and stand still, then click `Drive log 10 s`, press F8 within the next two
seconds, stand still for three, and press F8 again. Expected: `request: drivelog: written to logs\…\drive.csv`. Then:
```bash
D="$(ls -td /e/Cyberpunk2077/red4ext/plugins/BlockRender/logs/*/ | head -1)"; \
  awk -F, 'NR > 1 && $2 + 0 > worst { worst = $2 + 0 } END { print "worst frame", worst, "ms" }' "$D/drive.csv"
```
Expected: the worst frame well under the 53 to 56 ms step 2 found on entering: about 25 ms or less, as other frames.

- [x] **Step 4: In the game: a car**

Ask the user to get into a car with F, then press F8. Expected: `build mode stays off: V is in a vehicle`, or `V is in
a workspot`, since a car's seat is one; not `V isn't found`. Ask the user to drive a short way, get out, and stand
still for two seconds. Expected: the drive line's eye height still about 1.60 m, not the seat's 1.18.

- [x] **Step 5: In the game: the scene watch**

Ask the user to go near spot B on foot, where the probe's overlay shows the y = 512 line about 100 m south. Then send
`scenewatch on`. Expected: `request: scenewatch on: 16 of scene 0's calls hooked; from a second on, the status sums them
by thread`. If it says a slot doesn't hold the audited function, another mod has hooked it: stop, and record which.

Ask the user to walk for half a minute, then get into a car, drive south across the y = 512 line (the overlay's
distance passes 0) and back north across it, get out, and stand still. Meanwhile the status's `scenewatch=` line grows.
Then send `scenewatch off`. Expected: `request: scenewatch off: …`, and the log's `scene watch, … s: …` line, which the
status keeps. Read from it:
- which threads add and remove actors, step and fetch the simulation, and take the write lock;
- of each, how many calls came while OnUpdate ran, and how many of those without the write lock;
- whether `shiftOrigin` was called at the line, and its shift, which would be (0, ±1024, 0).

- [x] **Step 6: Decide where the exporter reads**

The exporter reads scene 0 under its read lock in OnUpdate (Tasks 34 to 36), as spike 1 read it. That's safe if no
other thread added, removed or fetched while OnUpdate ran without the write lock: the main thread's own calls can't
overlap OnUpdate, and the read lock excludes a writer holding the write lock. Stepping alone doesn't change what the
scene's queries see: PhysX updates them when the results are fetched.
- **If so,** go on with Task 34.
- **If not,** stop after Step 10: the exporter has to read where the watch shows the scene is quiet, such as just after
  the game fetches the simulation's results, and Tasks 34 to 36 are planned again first, as a change to this plan.

Either way, whether the game called `shiftOrigin` at the line says how part 3's bodies will follow the origin.

- [x] **Step 7: Ask the user to quit the game and stop Minecraft**

- [x] **Step 8: Check the game's crash reports**

```bash
ls -t "$LOCALAPPDATA/REDEngine/ReportQueue" 2>/dev/null | head -3
```
Expected: nothing newer than this session's start. A newer folder means the game faulted: read it before going on.

- [x] **Step 9: Record the opening check in `docs/spikes/2026-10-03-collision-and-movement-results.md`**

A "Step 3, the opening check" section: the follow-ups' outcomes (the turn, the frame on entering, the car's message, the
eye height after it); the watch's summary, thread by thread, with the counts in OnUpdate and without the write lock;
the shifts; and the decision, with why.

- [x] **Step 10: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-03-collision-and-movement-results.md \
  && git commit -q -m "collision-and-movement: step 3's opening check, the follow-ups and the scene watch" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 34: The exporter reads Night City's shape from scene 0

`physx/Export` (the spec's "The exporter"'s "What it reads", "What it keeps" and "Coordinates"):
- **`KeptShape`:** a static actor's scene-query shape, which V's controller sweeps against, and not a trigger or a
  plane. A simulation-only shape doesn't stop V; dynamic and kinematic actors are cars, litter, bodies and controllers.
- **Coordinates:** a batch's box goes from Minecraft's axes to the world's, then to PhysX's frame (world − origin), for
  the overlap; each triangle comes back the other way.
- **`PhysxGround`,** the link's `GroundSource` in the game. Once a frame, the first time the link reads, it takes scene
  0 (`Session`) and PhysX's origin from V's controller (spike 1's `ReadController`, scene 0 only: the last controller
  found, while it's still among scene 0's dynamic actors and fits under V; otherwise every kinematic capsule is
  measured). Then each box is one overlap of static actors under scene 0's read lock; each hit is checked against the
  audit, kept or left, and read with `AppendTriangles`, culled to the box. Without V, scene 0 or V's controller it reads
  nothing and says why, which `ExportReadsNothingWithoutTheGame` checks.
- **`Describe`,** for the `shapes` request: every shape within a radius, static or dynamic, with its geometry, actor,
  bounds, flags and filters, and whether the exporter keeps it.

**Files:**
- Create: `src/physx/Export.h`, `src/physx/Export.cpp`, `tests/TestExport.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestExport.cpp`**

```cpp
#include "Harness.h"

#include "physx/Export.h"

using namespace physx;
using namespace cpmc;
using namespace cpmc::px;

TEST(ExportKeepsStaticSceneQueryShapes)
{
	const PxShapeFlags both = PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE;
	const PxShapeFlags queryOnly(PxShapeFlag::eSCENE_QUERY_SHAPE);
	const PxShapeFlags simulationOnly(PxShapeFlag::eSIMULATION_SHAPE);
	const PxShapeFlags trigger = PxShapeFlag::eTRIGGER_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE;
	CHECK(KeptShape(AuditedClass::RigidStatic, both, PxGeometryType::eTRIANGLEMESH));
	CHECK(KeptShape(AuditedClass::RigidStatic, queryOnly, PxGeometryType::eBOX));
	CHECK(!KeptShape(AuditedClass::RigidStatic, simulationOnly, PxGeometryType::eBOX));  // V's controller doesn't sweep against it
	CHECK(!KeptShape(AuditedClass::RigidStatic, trigger, PxGeometryType::eBOX));
	CHECK(!KeptShape(AuditedClass::RigidStatic, both, PxGeometryType::ePLANE));
	CHECK(!KeptShape(AuditedClass::RigidDynamic, both, PxGeometryType::eCONVEXMESH));  // cars, litter, controllers
}

TEST(ExportTurnsPhysxIntoMinecraftWithTheOrigin)
{
	// Past world y 512 the game's PhysX origin is (0, 1024, 0): world = PhysX + origin, and Minecraft's z is world -y.
	const Vec3d origin{ 0.0, 1024.0, 0.0 };
	const Vec3d mc = McFromPhysx(PxVec3(-189.9f, -407.25f, 74.4f), origin);
	CHECK_NEAR(mc.x, -189.9, 1e-4);
	CHECK_NEAR(mc.y, 74.4, 1e-4);
	CHECK_NEAR(mc.z, -616.75, 1e-4);
	// A box goes the other way: Minecraft z -620 to -604 is world y 604 to 620.
	const PxBounds3 box = PhysxBox({ { 100.0, 0.0, -620.0 }, { 116.0, 16.0, -604.0 } }, origin);
	CHECK_NEAR(box.minimum.x, 100.0, 1e-4);
	CHECK_NEAR(box.maximum.x, 116.0, 1e-4);
	CHECK_NEAR(box.minimum.y, 604.0 - 1024.0, 1e-4);
	CHECK_NEAR(box.maximum.y, 620.0 - 1024.0, 1e-4);
	CHECK_NEAR(box.minimum.z, 0.0, 1e-4);
	CHECK_NEAR(box.maximum.z, 16.0, 1e-4);
}

TEST(ExportNamesShapesForTheShapesRequest)
{
	CHECK_STR(GeometryName(PxGeometryType::eHEIGHTFIELD), "heightField");
	CHECK_STR(GeometryName(PxGeometryType::eTRIANGLEMESH), "triangleMesh");
	CHECK_STR(FilterKey(PxFilterData(0, 0, 0x449c004, 0)), "0:0:449c004:0");
}

TEST(ExportReadsNothingWithoutTheGame)
{
	Session                       session;
	PhysxGround                   ground(session);
	std::vector<link::McTriangle> triangles;
	ground.Frame(std::nullopt);
	CHECK(!ground.Read(link::BatchBox({ 0, 0, 0 }), triangles));
	CHECK_STR(ground.State().why, "V isn't found");
	ground.Frame(Vec3d{ -119.5, -2007.5, 6.14 });
	CHECK(!ground.Read(link::BatchBox({ 0, 0, 0 }), triangles));
	CHECK_STR(ground.State().why, "PhysX3_x64.dll isn't loaded");
	CHECK(triangles.empty());
	CHECK_STR(ground.Describe({ 0.0, 0.0, 0.0 }, 2.0).front(), "none: PhysX3_x64.dll isn't loaded");
}
```
- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestPhysxSession.cpp
	tests/TestDrive.cpp
```
```cmake
	tests/TestPhysxSession.cpp
	tests/TestExport.cpp
	tests/TestDrive.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'physx/Export.h'`.

- [x] **Step 4: Write `src/physx/Export.h`**

```cpp
#pragma once

#include "Coords.h"
#include "link/Ground.h"
#include "physx/GameAbi.h"
#include "physx/PxHeaders.h"
#include "physx/Session.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Night City's shape for Minecraft, from the game's PhysX (spike 5's "The exporter"): scene 0's static scene-query
// shapes, which V's controller sweeps against, read through spike 1's checks and turned into Minecraft's triangles.
// PhysxGround is the link's GroundSource in the game. Main thread only.
namespace cpmc::px
{
// Whether the exporter keeps a shape: a static actor's scene-query shape, and not a trigger or a plane. A shape that's
// only a simulation shape doesn't stop V's controller, and dynamic and kinematic actors are cars, litter, bodies and
// controllers.
bool KeptShape(AuditedClass aActor, physx::PxShapeFlags aFlags, physx::PxGeometryType::Enum aGeometry);

// A point from PhysX's frame into Minecraft's axes: world = PhysX + aOrigin (PhysX's origin, from V's controller).
Vec3d McFromPhysx(const physx::PxVec3& aPoint, const Vec3d& aOrigin);
// A box in Minecraft's axes, as PhysX's frame has it: world - aOrigin.
physx::PxBounds3 PhysxBox(const link::McBox& aBox, const Vec3d& aOrigin);

// "box", "triangle mesh" and the rest, and a filter's words as "w0:w1:w2:w3" in hexadecimal: for the shapes request.
const char* GeometryName(physx::PxGeometryType::Enum aGeometry);
std::string FilterKey(const physx::PxFilterData& aData);

class PhysxGround : public link::GroundSource
{
public:
	explicit PhysxGround(Session& aSession) : session_(aSession) {}

	// Once a frame, before the link reads: V's feet (world axes), for V's controller, which gives PhysX's origin. Nothing:
	// V isn't found, and nothing is read.
	void Frame(const std::optional<Vec3d>& aFeet);

	// The kept shapes touching aBox, as triangles in Minecraft's axes, read under scene 0's read lock. False if scene 0
	// or PhysX's origin can't be had this frame.
	bool Read(const link::McBox& aBox, std::vector<link::McTriangle>& aOut) override;

	// Every shape within aRadius of aWorld (world axes), static or dynamic, described, for the shapes request; or why
	// there are none.
	std::vector<std::string> Describe(const Vec3d& aWorld, double aRadius);

	struct Report
	{
		std::optional<Vec3d> origin;  // PhysX's, the last frame it was read
		std::string          why;     // why nothing could be read, the last time that happened
		uint64_t             searches = 0;    // times V's controller was looked for among all of scene 0's capsules
		uint64_t             kept = 0;        // shapes read, since the start
		uint64_t             left = 0;        // shapes the overlap gave that aren't kept: simulation-only, triggers, planes
		uint64_t             unreadable = 0;  // objects that failed their checks, and meshes that couldn't be read
		uint64_t             triangles = 0;   // triangles read
	};

	const Report& State() const { return report_; }

private:
	// Scene 0 and PhysX's origin, once a frame, the first time they're needed.
	bool Prepare();
	// V's controller (OriginWatch.h): the last one found, while it's still among scene 0's dynamic actors and fits under
	// V; otherwise the kinematic capsule most nearly under V. PhysX's origin from it, or nothing.
	std::optional<Vec3d> Origin(physx::PxScene& aScene, const Vec3d& aFeet);

	Session&              session_;
	std::optional<Vec3d>  feet_;
	bool                  prepared_ = false;
	physx::PxScene*       scene_ = nullptr;
	const physx::PxActor* controller_ = nullptr;
	Report                report_;
};
}
```
- [x] **Step 5: Write `src/physx/Export.cpp`**

```cpp
#include "physx/Export.h"

#include "physx/OriginWatch.h"
#include "physx/ShapeTriangles.h"

#include <algorithm>
#include <cmath>
#include <format>

using namespace physx;

namespace cpmc::px
{
namespace
{
constexpr size_t kActorBytes = 24;           // the vtable pointer, the type ID and base flags, and userData
constexpr PxU32  kMostActors = 1'000'000;    // a scene listing more is garbage
constexpr size_t kOverlapBuffer = 4096;      // hits PhysX hands over at a time
constexpr size_t kMostHits = 100'000;        // an overlap stops after this many

// Collects every overlap hit: PhysX hands over each full buffer, and the last partial one, through processTouches.
struct CollectOverlaps final : PxHitCallback<PxOverlapHit>
{
	CollectOverlaps(PxOverlapHit* aBuffer, PxU32 aSize, std::vector<PxOverlapHit>& aOut)
		: PxHitCallback<PxOverlapHit>(aBuffer, aSize)
		, out(aOut)
	{
	}

	PxAgain processTouches(const PxOverlapHit* aHits, PxU32 aCount) override
	{
		out.insert(out.end(), aHits, aHits + aCount);
		return out.size() < kMostHits;
	}

	std::vector<PxOverlapHit>& out;
};

std::vector<PxShape*> ShapesOf(const PxRigidActor& aActor)
{
	std::vector<PxShape*> shapes(aActor.getNbShapes());
	shapes.resize(aActor.getShapes(shapes.data(), PxU32(shapes.size())));
	return shapes;
}

// Every hit of aGeometry at aPose (PhysX's frame) among the actors aWhich asks for, under the scene's read lock, which
// the caller holds.
std::vector<PxOverlapHit> Overlap(PxScene& aScene, const PxGeometry& aGeometry, const PxTransform& aPose, PxQueryFlags aWhich)
{
	std::vector<PxOverlapHit> buffer(kOverlapBuffer);
	std::vector<PxOverlapHit> hits;
	CollectOverlaps           collect(buffer.data(), PxU32(buffer.size()), hits);
	aScene.overlap(aGeometry, aPose, collect, PxQueryFilterData(aWhich | PxQueryFlag::eNO_BLOCK));
	return hits;
}
}

bool KeptShape(AuditedClass aActor, PxShapeFlags aFlags, PxGeometryType::Enum aGeometry)
{
	return aActor == AuditedClass::RigidStatic && aFlags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE) && !aFlags.isSet(PxShapeFlag::eTRIGGER_SHAPE) &&
	       aGeometry != PxGeometryType::ePLANE;
}

Vec3d McFromPhysx(const PxVec3& aPoint, const Vec3d& aOrigin)
{
	return McFromWorld({ aPoint.x + aOrigin.x, aPoint.y + aOrigin.y, aPoint.z + aOrigin.z });
}

PxBounds3 PhysxBox(const link::McBox& aBox, const Vec3d& aOrigin)
{
	// The world's x is Minecraft's x, its y Minecraft's -z, and its z Minecraft's y.
	const Vec3d low = WorldFromMc({ aBox.low.x, aBox.low.y, aBox.high.z });
	const Vec3d high = WorldFromMc({ aBox.high.x, aBox.high.y, aBox.low.z });
	return PxBounds3(PxVec3(float(low.x - aOrigin.x), float(low.y - aOrigin.y), float(low.z - aOrigin.z)),
	                 PxVec3(float(high.x - aOrigin.x), float(high.y - aOrigin.y), float(high.z - aOrigin.z)));
}

const char* GeometryName(PxGeometryType::Enum aGeometry)
{
	switch (aGeometry)
	{
	case PxGeometryType::eSPHERE: return "sphere";
	case PxGeometryType::ePLANE: return "plane";
	case PxGeometryType::eCAPSULE: return "capsule";
	case PxGeometryType::eBOX: return "box";
	case PxGeometryType::eCONVEXMESH: return "convexMesh";
	case PxGeometryType::eTRIANGLEMESH: return "triangleMesh";
	case PxGeometryType::eHEIGHTFIELD: return "heightField";
	default: return "unknown";
	}
}

std::string FilterKey(const PxFilterData& aData)
{
	return std::format("{:x}:{:x}:{:x}:{:x}", aData.word0, aData.word1, aData.word2, aData.word3);
}

void PhysxGround::Frame(const std::optional<Vec3d>& aFeet)
{
	feet_ = aFeet;
	prepared_ = false;
	scene_ = nullptr;
}

bool PhysxGround::Prepare()
{
	if (prepared_)
	{
		return scene_ != nullptr;
	}
	prepared_ = true;
	if (!feet_)
	{
		report_.why = "V isn't found";
		return false;
	}
	PxScene* const scene = session_.Scene0(report_.why);
	if (!scene)
	{
		return false;
	}
	const std::optional<Vec3d> origin = Origin(*scene, *feet_);
	if (!origin)
	{
		report_.why = "V's controller isn't found";
		return false;
	}
	report_.origin = origin;
	scene_ = scene;
	return true;
}

std::optional<Vec3d> PhysxGround::Origin(PxScene& aScene, const Vec3d& aFeet)
{
	ReadableMemory&    memory = session_.Memory();
	const ModuleBases& bases = session_.Bases();
	struct Candidate
	{
		const PxActor* actor = nullptr;
		Vec3d          origin;
		double         across = 0.0;
	};
	std::optional<Candidate> best;
	const auto               measure = [&](const PxActor* aActor) {
		const ObjectCheck check = CheckObject(memory, bases, aActor, kActorBytes);
		if (check.verdict != ObjectVerdict::Ok || check.cls != AuditedClass::RigidDynamic ||
		    !static_cast<const PxRigidBody&>(*aActor).getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC))
		{
			return;
		}
		const auto&       rigid = static_cast<const PxRigidActor&>(*aActor);
		const PxTransform pose = rigid.getGlobalPose();
		for (const PxShape* shape : ShapesOf(rigid))
		{
			const ObjectCheck shapeCheck = CheckObject(memory, bases, shape);
			PxCapsuleGeometry capsule;
			if (shapeCheck.verdict != ObjectVerdict::Ok || shapeCheck.cls != AuditedClass::Shape ||
			    shape->getGeometryType() != PxGeometryType::eCAPSULE || !shape->getCapsuleGeometry(capsule))
			{
				continue;
			}
			const PxVec3        centre = (pose * shape->getLocalPose()).p;
			const OriginReading reading = OriginFromController(aFeet, { centre.x, centre.y, centre.z });
			const double        across = std::hypot(reading.residual.x, reading.residual.y);
			if (FitsController(reading, capsule.radius) && (!best || across < best->across))
			{
				best = Candidate{ aActor, reading.origin, across };
			}
		}
	};
	aScene.lockRead();
	std::vector<PxActor*> actors(std::min(aScene.getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC), kMostActors));
	actors.resize(aScene.getActors(PxActorTypeFlag::eRIGID_DYNAMIC, actors.data(), PxU32(actors.size())));
	if (controller_ && std::find(actors.begin(), actors.end(), controller_) != actors.end())
	{
		measure(controller_);
	}
	if (!best)
	{
		++report_.searches;
		for (const PxActor* actor : actors)
		{
			measure(actor);
		}
	}
	aScene.unlockRead();
	controller_ = best ? best->actor : nullptr;
	return best ? std::optional<Vec3d>(best->origin) : std::nullopt;
}

bool PhysxGround::Read(const link::McBox& aBox, std::vector<link::McTriangle>& aOut)
{
	if (!Prepare())
	{
		return false;
	}
	ReadableMemory&    memory = session_.Memory();
	const ModuleBases& bases = session_.Bases();
	const Vec3d        origin = *report_.origin;
	const PxBounds3    box = PhysxBox(aBox, origin);
	std::vector<Tri>   triangles;
	AppendStats        stats;
	scene_->lockRead();
	for (const PxOverlapHit& hit : Overlap(*scene_, PxBoxGeometry(box.getExtents()), PxTransform(box.getCenter()), PxQueryFlag::eSTATIC))
	{
		const ObjectCheck actor = CheckObject(memory, bases, hit.actor, kActorBytes);
		const ObjectCheck shape = CheckObject(memory, bases, hit.shape);
		if (actor.verdict != ObjectVerdict::Ok || shape.verdict != ObjectVerdict::Ok || shape.cls != AuditedClass::Shape ||
		    (actor.cls != AuditedClass::RigidStatic && actor.cls != AuditedClass::RigidDynamic))
		{
			++report_.unreadable;
			continue;
		}
		if (!KeptShape(actor.cls, hit.shape->getFlags(), hit.shape->getGeometryType()))
		{
			++report_.left;
			continue;
		}
		++report_.kept;
		AppendTriangles(*hit.shape, hit.actor->getGlobalPose(), box, memory, bases, triangles, stats);
	}
	scene_->unlockRead();
	report_.unreadable += stats.unreadable;
	report_.triangles += triangles.size();
	for (const Tri& tri : triangles)
	{
		link::McTriangle out{};
		for (int k = 0; k < 3; ++k)
		{
			const Vec3d mc = McFromPhysx(tri.v[k], origin);
			out.v[k * 3] = float(mc.x);
			out.v[k * 3 + 1] = float(mc.y);
			out.v[k * 3 + 2] = float(mc.z);
		}
		aOut.push_back(out);
	}
	return true;
}

std::vector<std::string> PhysxGround::Describe(const Vec3d& aWorld, double aRadius)
{
	if (!Prepare())
	{
		return { "none: " + report_.why };
	}
	ReadableMemory&          memory = session_.Memory();
	const ModuleBases&       bases = session_.Bases();
	const Vec3d              origin = *report_.origin;
	const PxVec3             at(float(aWorld.x - origin.x), float(aWorld.y - origin.y), float(aWorld.z - origin.z));
	std::vector<std::string> lines;
	scene_->lockRead();
	for (const PxOverlapHit& hit :
	     Overlap(*scene_, PxSphereGeometry(float(aRadius)), PxTransform(at), PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC))
	{
		const ObjectCheck actor = CheckObject(memory, bases, hit.actor, kActorBytes);
		const ObjectCheck shape = CheckObject(memory, bases, hit.shape);
		if (actor.verdict != ObjectVerdict::Ok || shape.verdict != ObjectVerdict::Ok || shape.cls != AuditedClass::Shape ||
		    (actor.cls != AuditedClass::RigidStatic && actor.cls != AuditedClass::RigidDynamic))
		{
			lines.push_back(std::format("actor {} or its shape {} failed its checks", static_cast<const void*>(hit.actor),
			                            static_cast<const void*>(hit.shape)));
			continue;
		}
		const PxShapeFlags         flags = hit.shape->getFlags();
		const PxGeometryType::Enum geometry = hit.shape->getGeometryType();
		const char*                kind = actor.cls == AuditedClass::RigidStatic ? "static"
		                                  : static_cast<const PxRigidBody&>(*hit.actor).getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC)
		                                      ? "kinematic"
		                                      : "dynamic";
		const PxBounds3 bounds = hit.actor->getWorldBounds(1.0f);
		lines.push_back(std::format("{} {} of actor {}, actor bounds ({:.2f}, {:.2f}, {:.2f}) to ({:.2f}, {:.2f}, {:.2f}) in world axes; "
		                            "{}{}{}; query filter {}, simulation filter {}{}",
		                            kind, GeometryName(geometry), static_cast<const void*>(hit.actor), bounds.minimum.x + origin.x,
		                            bounds.minimum.y + origin.y, bounds.minimum.z + origin.z, bounds.maximum.x + origin.x,
		                            bounds.maximum.y + origin.y, bounds.maximum.z + origin.z,
		                            flags.isSet(PxShapeFlag::eSIMULATION_SHAPE) ? "simulation" : "not simulation",
		                            flags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE) ? ", scene query" : ", not scene query",
		                            flags.isSet(PxShapeFlag::eTRIGGER_SHAPE) ? ", trigger" : "", FilterKey(hit.shape->getQueryFilterData()),
		                            FilterKey(hit.shape->getSimulationFilterData()),
		                            KeptShape(actor.cls, flags, geometry) ? "; the exporter keeps it" : ""));
	}
	scene_->unlockRead();
	if (lines.empty())
	{
		lines.push_back("none");
	}
	return lines;
}
}
```
- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/physx/Session.cpp
	src/link/Drive.cpp
```
```cmake
	src/physx/Session.cpp
	src/physx/Export.cpp
	src/link/Drive.cpp
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `362 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the exporter reads Night City's shape from scene 0" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 35: BlockRender 0.13.0 sends Night City's shape to Minecraft

The plugin gives the link the city (the spec's "The ground setting", "Requests", "Measurements and the status" and
"Error handling"):
- **The city by default,** from step 3 on, with the floor as the fallback: if PhysX isn't the audited build, the ground
  falls back to the floor, the log and the status say why, and `set ground city` is refused with that reason.
- **Each frame,** after the drive, the link reads with `PhysxGround` within the 1.5 ms budget (`set exportbudget`, from
  0.1 to 20 ms), given V's feet from before this frame's teleport. V's controller matches them whether the teleport
  has moved it yet or not: a frame's move is a few centimetres, and the origin is the nearest 1,024 m grid point.
- **`set ground floor|city`** and **`shapes`**, the shapes within 2 m of V's middle into the log.
- **The status's `ground=` line:** the setting, or why it fell back; the epoch and the centre's region, regions known
  and batches to read; the reading time a frame, average and worst; batches, triangles and voxel blocks a second; the
  worker's time a region and its regions waiting; ring MB a second, messages waiting and dropped; Minecraft's hold when
  build mode last started; PhysX's origin and the controller's searches; shapes kept, left out and unreadable; and why
  nothing can be read, while that lasts.
- **The overlay** shows the ground's line, with buttons to switch it and to log the shapes. Version 0.13.0.

**Files:**
- Modify: `src/Requests.h`, `src/Requests.cpp`, `src/Status.h`, `src/Status.cpp`, `src/physx/Session.h`,
  `src/physx/Export.cpp`, `src/Plugin.cpp`, `tools/blockrender_hud/init.lua`, `tests/TestRequests.cpp`,
  `tests/TestStatus.cpp`, `CMakeLists.txt`

- [x] **Step 1: Edit `tests/TestRequests.cpp`: the failing test**

```cpp
	CHECK_STR(error, "scenewatch takes on or off");
}
```
```cpp
	CHECK_STR(error, "scenewatch takes on or off");
}

TEST(RequestsForTheGround)
{
	std::string error;
	const auto  city = cpmc::ParseRequest("set ground city", error);
	CHECK(city && city->kind == cpmc::Request::Kind::Set && city->what == "ground" && city->number == 1.0);
	const auto floor = cpmc::ParseRequest("set ground floor", error);
	CHECK(floor && floor->what == "ground" && floor->number == 0.0);
	CHECK(!cpmc::ParseRequest("set ground 1", error));
	CHECK_STR(error, "set ground takes floor or city");
	const auto budget = cpmc::ParseRequest("set exportbudget 2.5", error);
	CHECK(budget && budget->kind == cpmc::Request::Kind::Set && budget->what == "exportbudget" && budget->number == 2.5);
	const auto shapes = cpmc::ParseRequest("shapes", error);
	CHECK(shapes && shapes->kind == cpmc::Request::Kind::Shapes);
	CHECK(!cpmc::ParseRequest("shapes all", error));
}
```

- [x] **Step 2: Edit `tests/TestStatus.cpp`: the failing test**

```cpp
	CHECK(Has(cpmc::FormatStatus(report), "scenewatch=on for 12 s: thread 9120, the main thread: step 720, fetch 720; no shifts"));
}
```
```cpp
	CHECK(Has(cpmc::FormatStatus(report), "scenewatch=on for 12 s: thread 9120, the main thread: step 720, fetch 720; no shifts"));
}

TEST(StatusWritesTheGround)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	link.groundWhy = "PhysX3_x64.dll isn't the audited build";
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "ground=floor; it fell back from the city: PhysX3_x64.dll isn't the audited build"));
	link.ground = "city";
	link.collisionEpoch = 2;
	link.cityCentre = cpmc::link::RegionPos{ -15, 0, 250 };
	link.cityKnown = 1152;
	link.cityReadMsAverage = 0.4;
	link.cityReadMsWorst = 1.25;
	link.cityBatches = 2.0;
	link.cityTriangles = 1300.0;
	link.workerBlocks = 450.0;
	link.workerMsPerRegion = 0.68;
	link.collisionMb = 0.12;
	link.lastHold = 0.07;
	link.physxOrigin = cpmc::Vec3d{ 0.0, 1024.0, 0.0 };
	link.controllerSearches = 2;
	link.shapesKept = 1500;
	link.shapesLeft = 30;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report),
	          "ground=city, epoch 2, around region -15 0 250: 1152 regions known, 0 batches to read; read 0.40 ms a frame on average, 1.25 ms "
	          "at worst; 2.0 batches, 1300 triangles and 450 voxel blocks a second; the worker 0.68 ms a region, 0 regions waiting; the "
	          "ring 0.12 MB a second, 0 messages waiting for room, 0 too big; Minecraft held its player 0.07 s when build mode last "
	          "started; PhysX's origin (0, 1024, 0), V's controller looked for 2 times; shapes since the start: 1500 kept, 30 left out, "
	          "0 unreadable"));
}
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2065: 'Shapes': undeclared identifier`.

- [x] **Step 4: Edit `src/Requests.h`**

```cpp
//   set <name> <value>         (name: lower-case letters, digits, '.' and '_'; value: a finite number)
```
```cpp
//   set <name> <value>         (name: lower-case letters, digits, '.' and '_'; value: a finite number)
//   set ground floor|city      (spike 5: what Minecraft gets as the game's shape; the number is 0 or 1)
```
```cpp
//   scenewatch on|off          (spike 5's step 3: who calls scene 0, and when)
```
```cpp
//   scenewatch on|off          (spike 5's step 3: who calls scene 0, and when)
//   shapes                     (spike 5: the game's shapes within 2 m of V, into the log)
```
```cpp
		SceneWatch,
```
```cpp
		SceneWatch,
		Shapes,
```

- [x] **Step 5: Edit `src/Requests.cpp`**

```cpp
	if (command == "log" || command == "clear" || command == "reload" || command == "floor")
```
```cpp
	if (command == "log" || command == "clear" || command == "reload" || command == "floor" || command == "shapes")
```
```cpp
		const Request::Kind kind = command == "log"     ? Request::Kind::Log
		                           : command == "clear" ? Request::Kind::Clear
		                           : command == "floor" ? Request::Kind::Floor
		                                                : Request::Kind::Reload;
```
```cpp
		const Request::Kind kind = command == "log"      ? Request::Kind::Log
		                           : command == "clear"  ? Request::Kind::Clear
		                           : command == "floor"  ? Request::Kind::Floor
		                           : command == "shapes" ? Request::Kind::Shapes
		                                                 : Request::Kind::Reload;
```
```cpp
	}
	if (command == "set")
```
```cpp
	}
	if (command == "set" && arguments >= 1 && words[1] == "ground")
	{
		if (arguments != 2 || !OneOf(words[2], { "floor", "city" }))
		{
			return Fail(aError, "set ground takes floor or city");
		}
		return Request{ Request::Kind::Set, "ground", words[2] == "city" ? 1.0 : 0.0 };
	}
	if (command == "set")
```

- [x] **Step 6: Edit `src/Status.h`**

```cpp
#include "Measurements.h"
```
```cpp
#include "Measurements.h"
#include "link/Ground.h"
```
```cpp
	uint32_t              minecraftFlags = 0;        // McState's
```
```cpp
	uint32_t              minecraftFlags = 0;        // McState's
	// The ground (spike 5's "Part 2"): figures a second are over the last second
	std::string                    ground = "floor";          // what Minecraft gets as the game's shape
	std::string                    groundWhy;                 // why it fell back to the floor, if it did
	uint32_t                       collisionEpoch = 0;
	std::optional<link::RegionPos> cityCentre;                // the region the city's area is around
	size_t                         cityKnown = 0;             // regions read around it
	size_t                         cityWaiting = 0;           // the area's batches not read yet
	double                         cityReadMsAverage = 0.0;   // reading, a frame
	double                         cityReadMsWorst = 0.0;
	double                         cityBatches = 0.0;         // a second
	double                         cityTriangles = 0.0;       // a second, filed into regions sent
	double                         workerBlocks = 0.0;        // voxel blocks made, a second
	double                         workerMsPerRegion = -1.0;  // below 0: no region voxelized in the last second
	size_t                         workerJobs = 0;            // regions waiting for it
	double                         collisionMb = 0.0;         // the city's, into the collision ring, a second
	size_t                         collisionWaiting = 0;      // messages waiting for room
	uint64_t                       collisionDropped = 0;      // too big for it, since the start
	std::optional<double>          lastHold;                  // seconds Minecraft held its player when build mode last started
	std::optional<Vec3d>           physxOrigin;               // the last PhysX origin read, world axes
	uint64_t                       controllerSearches = 0;    // times V's controller was looked for among all the capsules
	uint64_t                       shapesKept = 0;            // since the start
	uint64_t                       shapesLeft = 0;            // given by the overlap but not kept: simulation-only, triggers
	uint64_t                       shapesUnreadable = 0;
	std::string                    physxWhy;                  // why the exporter couldn't read, while it can't
```

- [x] **Step 7: Edit `src/Status.cpp`**

```cpp
std::string FormatLink(const LinkReport& aLink)
```
```cpp
std::string FormatGround(const LinkReport& aLink)
{
	if (aLink.ground != "city")
	{
		return std::format("ground=floor{}\n", aLink.groundWhy.empty() ? "" : "; it fell back from the city: " + OneLine(aLink.groundWhy));
	}
	std::string out = std::format("ground=city, epoch {}", aLink.collisionEpoch);
	out += aLink.cityCentre ? std::format(", around region {} {} {}: {} regions known, {} batches to read", aLink.cityCentre->x,
	                                      aLink.cityCentre->y, aLink.cityCentre->z, aLink.cityKnown, aLink.cityWaiting)
	                        : ", no centre yet";
	out += std::format("; read {:.2f} ms a frame on average, {:.2f} ms at worst; {:.1f} batches, {:.0f} triangles and {:.0f} voxel blocks "
	                   "a second; the worker {}, {} regions waiting; the ring {:.2f} MB a second, {} messages waiting for room, {} too big",
	                   aLink.cityReadMsAverage, aLink.cityReadMsWorst, aLink.cityBatches, aLink.cityTriangles, aLink.workerBlocks,
	                   aLink.workerMsPerRegion < 0.0 ? std::string("idle") : std::format("{:.2f} ms a region", aLink.workerMsPerRegion),
	                   aLink.workerJobs, aLink.collisionMb, aLink.collisionWaiting, aLink.collisionDropped);
	if (aLink.lastHold)
	{
		out += std::format("; Minecraft held its player {:.2f} s when build mode last started", *aLink.lastHold);
	}
	out += aLink.physxOrigin
	           ? std::format("; PhysX's origin ({:.0f}, {:.0f}, {:.0f})", aLink.physxOrigin->x, aLink.physxOrigin->y, aLink.physxOrigin->z)
	           : std::string("; PhysX's origin unknown");
	out += std::format(", V's controller looked for {} times; shapes since the start: {} kept, {} left out, {} unreadable",
	                   aLink.controllerSearches, aLink.shapesKept, aLink.shapesLeft, aLink.shapesUnreadable);
	if (!aLink.physxWhy.empty())
	{
		out += "; nothing can be read now: " + OneLine(aLink.physxWhy);
	}
	return out + "\n";
}

std::string FormatLink(const LinkReport& aLink)
```
```cpp
	                   aLink.inGame || aLink.teleports > 0 ? ", the last to " + Position(aLink.target) : "", floor);
```
```cpp
	                   aLink.inGame || aLink.teleports > 0 ? ", the last to " + Position(aLink.target) : "", floor);
	out += FormatGround(aLink);
```

- [x] **Step 8: Edit `src/physx/Session.h`**

```cpp
	physx::PxScene* Scene0(std::string& aWhy);
```
```cpp
	physx::PxScene* Scene0(std::string& aWhy);
	// Whether PhysX is the audited build, with aWhy if it isn't: if not, nothing of it is ever read.
	bool Audited(std::string& aWhy) { return CheckBuild(aWhy); }
```

- [x] **Step 9: Edit `src/physx/Export.cpp`: its reason clears once it reads again**

```cpp
	report_.origin = origin;
```
```cpp
	report_.origin = origin;
	report_.why.clear();
```

- [x] **Step 10: Edit `src/Plugin.cpp`**

```cpp
#include "log/FrameLogData.h"
```
```cpp
#include "log/FrameLogData.h"
#include "physx/Export.h"
```
```cpp
constexpr const char* kVersion = "0.12.0";
```
```cpp
constexpr const char* kVersion = "0.13.0";
```
```cpp
cpmc::px::Session     g_physx;            // the game's PhysX, checked against the audit
```
```cpp
cpmc::px::Session     g_physx;            // the game's PhysX, checked against the audit
cpmc::px::PhysxGround g_ground{ g_physx };  // Night City's shape, read for the link
double                g_exportBudget = 1.5;  // set exportbudget: ms a frame the link may spend reading the city
std::string           g_groundWhy;        // why the ground fell back to the floor, if it did
```
```cpp
	uint64_t teleportCalls = 0;
```
```cpp
	uint64_t teleportCalls = 0;
	// The city's
	double   cityReadMsSum = 0.0;
	double   cityReadMsWorst = 0.0;
	uint64_t cityBatchesAtStart = 0;
	uint64_t cityTrianglesAtStart = 0;
	uint64_t workerRegionsAtStart = 0;
	uint64_t workerBlocksAtStart = 0;
	double   workerMsAtStart = 0.0;
	uint64_t collisionBytesAtStart = 0;
```
```cpp
	const cpmc::link::Link::Status&     state = g_link->State();
```
```cpp
	const cpmc::link::Link::Status&     state = g_link->State();
	if (std::string why; g_linkSettings.ground == cpmc::link::Ground::City && !g_physx.Audited(why))
	{
		// PhysX isn't the build the audit checked: nothing of it is read, and Minecraft gets the floor.
		g_linkSettings.ground = cpmc::link::Ground::Floor;
		g_groundWhy = why;
		Log("the ground falls back to the floor: {}", why);
	}
```
```cpp
	}
	if (std::exchange(g_buildWanted, false))
```
```cpp
	}
	// Night City's shape, given V's feet from before this frame's teleport: V's controller is within a frame's move of
	// them, whether the teleport has moved it yet or not.
	g_ground.Frame(pose ? std::optional<cpmc::Vec3d>(pose->feet) : std::nullopt);
	g_link->ReadGround(aNow, g_ground, g_exportBudget);
	g_second.cityReadMsSum += state.cityReadMs;
	g_second.cityReadMsWorst = std::max(g_second.cityReadMsWorst, state.cityReadMs);
	if (std::exchange(g_buildWanted, false))
```
```cpp
		g_hudScale = uint32_t(value);
```
```cpp
		g_hudScale = uint32_t(value);
	}
	else if (name == "ground")
	{
		std::string why;
		if (value == 1.0 && !g_physx.Audited(why))
		{
			Report("set ground city: " + why + ", so the ground stays the floor");
			return true;
		}
		g_linkSettings.ground = value == 1.0 ? cpmc::link::Ground::City : cpmc::link::Ground::Floor;
		g_groundWhy.clear();
		Report(std::format("set ground {}", cpmc::link::GroundName(g_linkSettings.ground)));
		return true;
	}
	else if (name == "exportbudget")
	{
		if (value < 0.1 || value > 20.0)
		{
			Report("set exportbudget: from 0.1 to 20 ms a frame");
			return true;
		}
		g_exportBudget = value;
```
```cpp
// The command in aFolder's blockrender.request, if there is one: in the plugin's folder, where the agent writes it, or
```
```cpp
// shapes (spike 5's "Requests"): every shape within 2 m of V's middle, into the log with its filters. Where Minecraft's
// player and V disagree about the ground, these say why.
void Shapes()
{
	const std::optional<cpmc::PlayerPose> pose = cpmc::LocatePlayer();
	if (!pose)
	{
		return Report("shapes: there's no player");
	}
	g_ground.Frame(pose->feet);
	const std::vector<std::string> lines = g_ground.Describe({ pose->feet.x, pose->feet.y, pose->feet.z + 1.0 }, 2.0);
	for (const std::string& line : lines)
	{
		Log("shapes: {}", line);
	}
	Report(std::format("shapes: {} lines in the log, of the shapes within 2 m of V's middle", lines.size()));
}

// The command in aFolder's blockrender.request, if there is one: in the plugin's folder, where the agent writes it, or
```
```cpp
	case cpmc::Request::Kind::SceneWatch: SceneWatch(request->what == "on"); break;
```
```cpp
	case cpmc::Request::Kind::SceneWatch: SceneWatch(request->what == "on"); break;
	case cpmc::Request::Kind::Shapes: Shapes(); break;
```
```cpp
		report.minecraftFlags = state.minecraftFlags;
	}
```
```cpp
		report.minecraftFlags = state.minecraftFlags;
		report.ground = cpmc::link::GroundName(state.ground);
		report.collisionEpoch = state.collisionEpoch;
		report.cityCentre = state.cityCentre;
		report.cityKnown = state.cityKnown;
		report.cityWaiting = state.cityWaiting;
		report.cityReadMsAverage = g_second.frames ? g_second.cityReadMsSum / double(g_second.frames) : 0.0;
		report.cityReadMsWorst = g_second.cityReadMsWorst;
		report.cityBatches = double(state.cityBatches - g_second.cityBatchesAtStart) / seconds;
		report.cityTriangles = double(state.cityTriangles - g_second.cityTrianglesAtStart) / seconds;
		report.workerBlocks = double(state.workerBlocks - g_second.workerBlocksAtStart) / seconds;
		const uint64_t regions = state.workerRegions - g_second.workerRegionsAtStart;
		report.workerMsPerRegion = regions ? (state.workerMs - g_second.workerMsAtStart) / double(regions) : -1.0;
		report.workerJobs = state.workerJobs;
		report.collisionMb = double(state.collisionBytes - g_second.collisionBytesAtStart) / 1048576.0 / seconds;
		report.collisionWaiting = state.collisionWaiting;
		report.collisionDropped = state.collisionDropped;
		report.lastHold = state.lastHold;
	}
	report.groundWhy = g_groundWhy;
	const cpmc::px::PhysxGround::Report& physx = g_ground.State();
	report.physxOrigin = physx.origin;
	report.controllerSearches = physx.searches;
	report.shapesKept = physx.kept;
	report.shapesLeft = physx.left;
	report.shapesUnreadable = physx.unreadable;
	report.physxWhy = physx.why;
```
```cpp
		g_second.waitedAtStart = state.waitedFrames;
```
```cpp
		g_second.waitedAtStart = state.waitedFrames;
		g_second.cityBatchesAtStart = state.cityBatches;
		g_second.cityTrianglesAtStart = state.cityTriangles;
		g_second.workerRegionsAtStart = state.workerRegions;
		g_second.workerBlocksAtStart = state.workerBlocks;
		g_second.workerMsAtStart = state.workerMs;
		g_second.collisionBytesAtStart = state.collisionBytes;
```
```cpp
		const cpmc::PluginSettings settings = cpmc::ParseSettings(ReadSmallFile(g_pluginDir / "settings.txt"));
```
```cpp
		const cpmc::PluginSettings settings = cpmc::ParseSettings(ReadSmallFile(g_pluginDir / "settings.txt"));
		g_linkSettings.ground = cpmc::link::Ground::City;  // Night City's shape from step 3 on; the floor is the fallback
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 12, 0);
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 13, 0);
```

- [x] **Step 11: Edit `CMakeLists.txt`: version 0.13.0**

```cmake
project(BlockRender VERSION 0.12.0 LANGUAGES C CXX)
```
```cmake
project(BlockRender VERSION 0.13.0 LANGUAGES C CXX)
```

- [x] **Step 12: Edit `tools/blockrender_hud/init.lua`**

```lua
-- Minecraft's hotbar slot; and since spike 5, one shows build mode's drive, with buttons for its turn and its drive log.
```
```lua
-- Minecraft's hotbar slot; and since spike 5, one shows build mode's drive, with buttons for its turn and its drive log,
-- and one the ground Minecraft gets, with buttons to switch it and to log the shapes around V.
```
```lua
local turn = nil         -- the drive line's turn setting: "on" or "off"
```
```lua
local turn = nil         -- the drive line's turn setting: "on" or "off"
local ground = nil       -- the ground line's setting, "floor" or "city", and how much of the city is known
local groundName = nil   -- "floor" or "city"
```
```lua
    turn = nil
```
```lua
    turn = nil
    ground = nil
    groundName = nil
```
```lua
            turn = line:match("; turn (%a+),")
```
```lua
            turn = line:match("; turn (%a+),")
        end
        local setting = line:match("^ground=(%a+)")
        if setting ~= nil then
            groundName = setting
            local known = line:match("(%d+ regions known, %d+ batches to read)")
            local fallback = line:match("; it fell back from the city: (.*)$")
            ground = setting .. (known and (", " .. known) or "") .. (fallback and (", fell back: " .. fallback) or "")
```
```lua
        end
        ImGui.Separator()
```
```lua
        end
        if ground ~= nil then
            ImGui.Text("Ground: " .. ground)
            -- "###ground" keeps the button's ID the same while its label changes.
            ImGui.SameLine()
            if ImGui.Button((groundName == "city" and "Use the floor" or "Use the city") .. "###ground") then
                request("set ground " .. (groundName == "city" and "floor" or "city"))
            end
            ImGui.SameLine()
            if ImGui.Button("Shapes") then
                request("shapes")
            end
        end
        ImGui.Separator()
```

- [x] **Step 13: Check the overlay's syntax**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && luac -p tools/blockrender_hud/init.lua && echo ok
```
Expected: `ok`.

- [x] **Step 14: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `364 test(s), 0 failure(s)`.

- [x] **Step 15: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: BlockRender 0.13.0 sends Night City's shape to Minecraft" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 36: In the game: step 3, Night City's shape

The rest of step 3 in the game, with the user, on BlockRender 0.13.0, whose ground is the city: at spike 3's corner,
then at spot B and the ramp (the spec's "Step 3"). Task 33's commands for requests, the log and the status come back
here. FreeFly (F7) stays off. Note what you see as you go: Step 11 records it. Where Minecraft's player and the city
disagree (V, placed where Minecraft's player is, walking through a wall, floating, or sinking into the street), ask the
user to click the overlay's `Shapes` button there: the log then lists the shapes within 2 m, with their filters. If a
check fails, stop and find the cause with superpowers:systematic-debugging before going on. Fixes become new tasks.

**Files:**
- Modify: `docs/spikes/2026-10-03-collision-and-movement-results.md`

- [x] **Step 1: Deploy (the game must not be running)**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game first, because a running game locks the DLL.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default -DCP2077_DIR=E:/Cyberpunk2077 > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C|error MSB"; \
  "$CMAKE" --preset default -DCP2077_DIR= > /dev/null; \
  cmp build/RelWithDebInfo/BlockRender.dll /e/Cyberpunk2077/red4ext/plugins/BlockRender/BlockRender.dll && echo installed
```
Expected: `installed`. The second configure clears `CP2077_DIR` again.

- [x] **Step 2: In the game: the city around V**

Ask the user to start the `CyberCraft` instance, then the game, load the free-roam save at spike 3's corner, and click
Link. Expected:
- in the log: `BlockRender 0.13.0 loaded`, `link: the ground is the city now`, `link: Minecraft is there (process …)`
  and `link: the city starts over, as collision epoch …`; no `floor` line;
- within a few seconds of the link, the status's `ground=city, epoch …, around region -15 0 250: 1152 regions known, 0
  batches to read`, PhysX's origin `(0, -2048, 0)`, and no `nothing can be read now`;
- the reading time a frame under the 1.5 ms budget on average, and a few milliseconds at worst; then, standing, about 8
  batches a second: the ones next to V, read again every second.

- [x] **Step 3: In the game: build mode on the city**

Ask the user to press F8. Expected: `Minecraft let its player go after …` within a second, and the ground line's `held
its player … s`. Then ask the user to walk the street and its kerbs, up and down any stairs or ramp near the corner,
and into a wall, and to say whether V, following Minecraft's player, stays on the visible ground and stops at the wall.

Added after Task 33: send `scenewatch on` before the walk, and `scenewatch off` after it. Expected: still no call in
`OnUpdate` (no "in OnUpdate" in the log's `scene watch, …` line), now that the exporter reads there. Task 33's watch
found none while `OnUpdate` did less.

- [x] **Step 4: In the game: under an overhang**

Ask the user to leave build mode, stand under something less than 2.5 m above V's feet (an awning, a stair, a low
ceiling), and press F8. Record whether Minecraft's `liftOutOfGeometry` puts its player, and so V, on top of it (risk
11).

- [x] **Step 5: In the game: blocks against the city**

In build mode, ask the user to place stone against the street, then against a wall, with right clicks. Expected: each
block lands on the street or against the wall where the crosshair is, since clicks now hit the city's triangles.

- [x] **Step 6: In the game: flying ahead of the regions**

Ask the user to fly (Space twice), sprint-fly a few hundred metres over the city, then stop flying (Space twice) above a
street. Expected: the ground line's batches to read climb while flying and fall back to 0; the player lands on the
street, not through it.

- [x] **Step 7: In the game: TNT on the street**

Ask the user to stand on open street in build mode, a few metres from any kerb, wall or placed block. Then send `mc
/summon minecraft:tnt ~2 ~4 ~`, which drops it 4 blocks onto the street 2 blocks east of the player (heading 90 on the
overlay), whichever way the user looks. Expected, about 4 s later, in the log: `explosion at Minecraft …` with y within
about a quarter block of the street's height, which is V's feet's (the overlay's `V:` z, Minecraft's y): the TNT landed
on the street's voxels, with no stone under it.

- [x] **Step 8: In the game: the cost**

Ask the user to stand still for 10 s with build mode on, then send `set ground floor`, wait 10 s, and send `set ground
city`. Read the status's `frame=` line each time, and the ground line's reading time, worker time and ring MB a second.

- [x] **Step 9: In the game: spot B and the ramp**

Ask the user to go to spot B, then the ramp, and in build mode walk them as Task 28's script did: at spot B west off the
kerb and back east to the barrier; at the ramp north up its lane and east into its wall. Compare with the offline half.

- [x] **Step 10: Ask the user to quit the game and stop Minecraft, then check the crash reports**

```bash
ls -t "$LOCALAPPDATA/REDEngine/ReportQueue" 2>/dev/null | head -3
```
Expected: nothing newer than this session's start.

- [x] **Step 11: Record step 3 in the game in `docs/spikes/2026-10-03-collision-and-movement-results.md`**

A "Step 3, in the game" section: each check above with its outcome and numbers; the `shapes` lines where Minecraft's
player and the city disagreed, and what their filters say (risk 4); the cost against the floor's; and what it means for
Part 3.

- [x] **Step 12: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-03-collision-and-movement-results.md \
  && git commit -q -m "collision-and-movement: step 3's results in the game" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 37: Close Part 2, and plan Part 3

- [x] **Step 1: Add "How Part 2 went" to this plan**

At the end: what ran as written, what changed, and what step 3 found.

- [x] **Step 2: Plan Part 3**

Use superpowers:writing-plans to add Part 3's tasks, from Task 38 on, to this plan: Part 2's two follow-ups first, as
the user asked, then V against blocks (the spec's "Part 3" and step 4), with the audit of the calls that make bodies
before step 4. Verify them as Parts 1 and 2 were. Changes to the spec found while planning go in its "Changes while
planning".

- [x] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/superpowers \
  && git commit -q -m "collision-and-movement plan: Part 3 planned" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

## Found while planning Part 2

The spec's "Changes while planning" records those that touch the design.
- **Part 1's follow-ups come first** (Tasks 19 to 22), as the user asked; the field-of-view kick while sprinting stays a
  non-goal.
- **Step 3's order:** offline first (Task 28), as the brainstorm chose; then the scene watch with the follow-ups in the
  game (Task 33), before the exporter is built, since it decides where the exporter reads; then the city (Task 36).
- **One collision epoch:** the floor's epoch becomes the collision ring's, which the city's clears move on too.
- **The city's messages wait in order** for room in the ring, and a message too big for it is dropped and counted
  rather than holding the rest up forever.
- **A batch's regions are all sent the first time,** empty ones too, so Minecraft knows them, and after that only those
  whose triangles' hash changed; the hash ignores the triangles' order.
- **V found again** reads the whole area again after the settle, without a clear: a load and a car's camera look the
  same to the link.
- **The worker counts a job as waiting until its messages are taken,** so "nothing waiting" means everything is out.
- **The overlap reads static actors only,** and `KeptShape` keeps their scene-query shapes that aren't triggers or
  planes. Part 3 adds leaving out BlockRender's own bodies.
- **PhysX's origin is read only on frames that read,** once, with V's feet from before that frame's teleport, which V's
  controller is within a frame's move of.
- **`set ground floor|city`** is parsed on its own, its words giving 0 or 1.
- **The scene watch's status line** shows once the watch has been on, and keeps its last summary after `off`.
- **The hooks check each slot holds the audited function** before patching anything, and save each original before its
  slot points at the hook.
- **The scene watch follows the write lock before it counts:** a thread's lock count never goes below none, and calls
  count from a second after the hooks go in. Found reviewing the plan: a thread holding the write lock as the hooks
  went in would otherwise have had every later locked call counted as unlocked, and Task 33 stop for nothing.
- **Versions:** 0.12.0 for the scene watch (Task 33's deploy) and 0.13.0 for the city (Task 36's), so each deploy's log
  says which it is.

## How Part 2 went

Part 2 ran on 2026-10-04, and step 3 passed after one fix. The results are in
[docs/spikes/2026-10-03-collision-and-movement-results.md](../../spikes/2026-10-03-collision-and-movement-results.md).
- **As written:** Tasks 19 to 27, 29 to 32, 34 and 35 ran through the plan runner in the real repo exactly as written.
  Every build, test, `mclink` and overlay expectation was met, and each task's files matched the verified scratch
  clone's. 364 tests, no warnings, no PhysX imports.
- **Step 3 offline (Task 28):** passed. Spot B's kerb and barrier, the ramp and the TNT all matched the dumps' own
  heights. `mclink` logs `explosion at …` without the plugin's word "Minecraft", and pushed into a wall the player
  slides along it, since the walls there are 8° off Minecraft's axes.
- **The opening check (Task 33):** passed. Part 1's follow-ups all work in the game, and in 140 s the scene watch saw
  37,770 calls to scene 0 and none while `OnUpdate` ran, nor any write lock, so the exporter reads in `OnUpdate` as
  planned. The game shifts scene 0 itself when PhysX's origin moves.
- **Step 3 in the game (Task 36):** passed after one fix. Task 36 also kept the scene watch on while the exporter
  read, an addition after Task 33: still nothing in `OnUpdate`.
  - **The stall:** each batch next to V read again every second stalled 3 to 14 ms in `VirtualQuery`, for
    heightfields' sample arrays in the game's 600 MB heap regions. Two commits outside the plan fixed it, the
    diagnostics (`71b93fd`) and the fix (`b5ac34f`, 365 tests): `QueryWorkingSetEx` now answers large ranges in the
    working set.
  - **What else it found:** a kinematic guard rail isn't sent, by design; the worker falls behind a fast drop into dense
    ground; the turn ratio isn't kept between sessions; and entering build mode under an overhang puts V on top of it
    (risk 11).
- **Left for Part 3, first:**
  - the turn ratio remembered between sessions;
  - the worker: more threads, and the regions nearest Minecraft's player first.
- **Deferred, by the user's choice:** kinematic props such as the rail. Moving props may suit Minecraft entities better,
  as boats are solid to stand on, but that's beyond this spike, so it waits for its own brainstorm.

---

# Collision and Movement Implementation Plan, Part 3 (step 4)

> Planned on 2026-10-04, after Part 2 ran. As Parts 1 and 2: inline, with superpowers:executing-plans, never through
> sub-agents. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Part 3 of spike 5, and its last. Minecraft's blocks become solid for V outside build mode: a PhysX static
actor for each section with solid blocks around V, so V stands on a build, bumps into its walls, and is left standing on
it when build mode ends on top. Part 2's two follow-ups come first. Step 4 checks it in the game, with a test box before
any block.

**Architecture:**
- **Part 2's follow-ups** (Tasks 38 to 40): the turn ratio kept in `turn.txt` between sessions; and the ground's worker
  on several threads, the regions nearest Minecraft's player first, with the figures that say why a region took long.
- **Before the bodies** (Tasks 41 to 46): the scene watch also counts scene 0's queries, since the bodies are added in
  OnUpdate and no other thread may query the scene meanwhile; the link keeps `kRenSolids` by section (`link/Solids`),
  checked against the real Minecraft with `mclink`; BlockRender 0.14.0; the audit's addendum for every PhysX call Part 3
  adds; and, in the game, the follow-ups and the query watch, which settles where bodies may be added.
- **The bodies** (Tasks 47 to 52): `physx/BoxMerge` (a section's solid blocks as few boxes) and `physx/BodyPlan` (which
  sections get bodies around V, and when), both pure; the exporter leaving BlockRender's own bodies out; `physx/Bodies`,
  the PhysX actors, made of whatever V stands on; BlockRender 0.15.0, with `set blocks` and `box on|off`; then step 4 in
  the game.

**Tech stack:** as Part 2's.

**Spec:** the spec's "Part 3: V against blocks", "Step 4", "The audit", "Requests", "Measurements and the status",
"Safety" and "Error handling", with what planning changed in its "Changes while planning".

## Before you start (Part 3)

- **Part 1's and Part 2's "Before you start" hold:** the branch, the shell and `CMAKE`, Write, Edit and Copy steps, the
  user, the game and deploying, no faults, and the commits.
- **Already verified,** as Parts 1 and 2 were:
  - all the code has been compiled and tested once already, in a scratch clone, and the tasks replayed in a fresh
    clone, applying every file step exactly as written and checking every build, test, `mclink` and overlay step;
  - zero compiler warnings at `/W4`, and BlockRender's DLL imports nothing from PhysX (`dumpbin /imports`);
  - each new test was broken once, by a mutation of the code it covers, and failed as it should;
  - every PhysX call Part 3 adds was found in the game's `PhysX3_x64.dll` (Task 45's addendum).

  So a failure while following the plan most likely means a transcription slip: compare against the plan's code first.
- **What no unit test covers:** `physx/Bodies` calls PhysX on the game's own objects, so only the game runs it. Task 52
  does, with the test box before any block.
- **Test counts:** 365 at the start. Then 367, 370, 371, 372 and 376 after Tasks 38 to 42; 377 and 378 after Tasks 44
  and 45; and 381, 389, 390, 390 and 392 after Tasks 47 to 51.
- **What only Minecraft and the game can test:** the worker and the solids against the real Minecraft in Task 43; the
  follow-ups, the query watch and the solids in the game in Task 46; and step 4 in Task 52, all with the user.
- **Task 46 decides where the bodies are added.** If it finds another thread calling scene 0 while OnUpdate runs, stop
  after it: Tasks 47 to 52 add and remove bodies in OnUpdate, and are planned again first.
- **Deploys:** Task 46 deploys BlockRender 0.14.0, and Task 52 0.15.0, each with the game closed and `CP2077_DIR`
  cleared straight after.

## File map (Part 3)

All paths under `src/`, `tests/` and `tools/` are relative to `spikes/block-render/`.

| File | Responsibility |
|---|---|
| `CMakeLists.txt` | The new sources and tests, and versions 0.14.0 and 0.15.0 |
| `src/link/Turn.*` | The last session's ratio, used until this one's is known, and `turn.txt`'s text |
| `src/link/GroundWorker.*` | Several threads, the job nearest the focus first, a region's jobs in order, the threads' running time |
| `src/link/Link.*` | The worker's focus and figures, and Minecraft's solids kept |
| `src/link/Solids.*` | Minecraft's solid blocks by section, from `kRenSolids` |
| `src/physx/SceneWatch.*`, `SceneHooks.*` | The scene's three queries, watched too |
| `src/physx/BoxMerge.*` | A section's solid blocks as few boxes |
| `src/physx/BodyPlan.*` | Which sections get bodies around V, and when; where their boxes stand |
| `src/physx/Export.*` | BlockRender's own bodies left out; scene 0 and the origin shared with the bodies |
| `src/physx/Session.h` | `PxGetPhysics`'s object, for making bodies |
| `src/physx/Bodies.*` | The bodies' PhysX actors, made of what V stands on, and step 4's test box |
| `src/Plugin.cpp` | `turn.txt`; the worker's, the solids' and the bodies' status; `set blocks`, `box on|off`, the bodies each frame |
| `src/Requests.*`, `src/Status.*` | `box on|off`; the turn, the worker, the solids and the bodies in the status |
| `tools/mclink/Main.cpp`, `solids.txt` | The worker's figures and the solids; the solids' script |
| `tools/blockrender_hud/init.lua` | The bodies' line, with Blocks and Box buttons |
| `tests/TestSolids.cpp`, `TestBoxMerge.cpp`, `TestBodyPlan.cpp` | One test file per new pure unit; the others gain tests |
| `docs/spikes/2026-10-01-physx-abi-audit.md` | The addendum: the bodies' and the queries' slots |

---

### Task 38: Build mode's turn ratio is remembered between sessions

Part 2's first follow-up, promised to the user. Build mode turns the view by each frame's mouse counts times a ratio,
measured from 5 turns outside build mode each session (`link/Turn.h`), so after every game start build mode couldn't
turn until V had turned 5 times. Now the plugin saves the measured ratio in `turn.txt`, in its folder, once a second
whenever it has changed, written whole and then renamed into place; at load it reads it back, and `TurnRatio::Remember`
uses it until this session has measured its own. The drive line says which it uses, and the log says what was
remembered at load and when this session's ratio is first saved. Delete `turn.txt` to measure afresh.

**Files:**
- Modify: `src/link/Turn.h`, `src/link/Turn.cpp`, `src/Status.h`, `src/Status.cpp`, `src/Plugin.cpp`,
  `tests/TestTurn.cpp`, `tests/TestStatus.cpp`

- [x] **Step 1: Edit `tests/TestTurn.cpp`: the failing tests**

```cpp
TEST(TurnRatioDropsATurnBrokenOff)
```
```cpp
TEST(TurnRatioUsesARememberedRatioUntilItsOwnIsKnown)
{
	// The last session's ratio turns build mode at once after a game start; this session's own takes over at kKnown turns.
	TurnRatio ratio;
	ratio.Remember(-0.05);
	CHECK(ratio.Remembered());
	CHECK(!ratio.Measured().has_value());
	CHECK_NEAR(ratio.Ratio().value_or(0.0), -0.05, 1e-12);
	CHECK_NEAR(ratio.Turn(100), -5.0, 1e-12);
	double yaw = 0.0;
	ratio.Note(0, yaw);
	for (int turn = 0; turn < 4; ++turn)
	{
		Turn(ratio, yaw, 30, 10, -0.02);
	}
	CHECK(ratio.Remembered());  // 4 of 5 turns: still the remembered one
	CHECK_NEAR(ratio.Ratio().value_or(0.0), -0.05, 1e-12);
	Turn(ratio, yaw, 30, 10, -0.02);
	CHECK(!ratio.Remembered());
	CHECK_NEAR(ratio.Measured().value_or(0.0), -0.02, 1e-12);
	CHECK_NEAR(ratio.Ratio().value_or(0.0), -0.02, 1e-12);
}

TEST(TurnFileKeepsTheRatioForTheNextSession)
{
	CHECK_NEAR(ParseTurnFile(TurnFileText(-0.0493)).value_or(0.0), -0.0493, 1e-9);
	CHECK_NEAR(ParseTurnFile("# a comment\r\n  ratio=0.0312  \r\n").value_or(0.0), 0.0312, 1e-12);
	for (const char* bad : { "", "ratio", "ratio =", "ratio = abc", "ratio = -0.05x", "ratio = 0", "ratio = 0.00001", "ratio = -2",
	                         "ratio = nan", "ratio = inf", "# ratio = -0.05", "turn = -0.05" })
	{
		CHECK(!ParseTurnFile(bad).has_value());
	}
}

TEST(TurnRatioDropsATurnBrokenOff)
```

- [x] **Step 2: Edit `tests/TestStatus.cpp`: the failing test**

```cpp
	CHECK(status.find("build=") < status.find("drive=") && status.find("drive=") < status.find("positions="));
```
```cpp
	CHECK(status.find("build=") < status.find("drive=") && status.find("drive=") < status.find("positions="));
	// The last session's ratio, until this one has measured its own
	link.turnRatio = -0.05;
	link.turnRemembered = true;
	link.turnTurns = 2;
	report.link = link;
	CHECK(cpmc::FormatStatus(report).find("; turn on, -0.0500 degrees a count, remembered from the last session (2 of 5 turns "
	                                      "measured); ") != std::string::npos);
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'turnRemembered': is not a member of 'cpmc::LinkReport'`, and `error C2039: 'Remember': is not a
member of 'cpmc::link::TurnRatio'`.

- [x] **Step 4: Edit `src/link/Turn.h`**

```cpp
#include <optional>
```
```cpp
#include <optional>
#include <string>
#include <string_view>
```
```cpp
// game made by itself doesn't move it. Pure.
namespace cpmc::link
{
```
```cpp
// game made by itself doesn't move it. The last session's ratio, kept in turn.txt, is used until this session has its
// own, so build mode turns at once after a game start. Pure.
namespace cpmc::link
{
// turn.txt, in the plugin's folder, keeps the last ratio measured for the next session. Its text, and the ratio in it:
// nothing unless a line "ratio = <degrees a count>" holds a finite number from 0.0001 to 1 either way.
std::string           TurnFileText(double aRatio);
std::optional<double> ParseTurnFile(std::string_view aText);

```
```cpp
	void Break();

	std::optional<double> Ratio() const;                 // degrees a count, once kKnown turns have counted
	double                Turn(int64_t aCounts) const;   // aCounts in degrees; 0 while the ratio isn't known
```
```cpp
	void Break();
	// A ratio from an earlier session (turn.txt), used until this session's own kKnown turns have counted.
	void Remember(double aRatio) { remembered_ = aRatio; }

	std::optional<double> Measured() const;    // this session's, the median of the last turns, once kKnown have counted
	std::optional<double> Ratio() const;       // degrees a count: this session's, or until it's known the remembered one
	bool                  Remembered() const;  // whether Ratio is the remembered one
	double                Turn(int64_t aCounts) const;   // aCounts in degrees; 0 while there's no ratio
```
```cpp
	uint64_t              turns_ = 0;
```
```cpp
	uint64_t              turns_ = 0;
	std::optional<double> remembered_;
```

- [x] **Step 5: Edit `src/link/Turn.cpp`**

```cpp
#include <algorithm>
#include <cmath>
```
```cpp
#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
```
```cpp
{
void TurnRatio::Note(int64_t aCounts, double aYaw)
```
```cpp
{
namespace
{
std::string_view Trim(std::string_view aText)
{
	const auto space = [](char aChar) { return aChar == ' ' || aChar == '\t' || aChar == '\r'; };
	while (!aText.empty() && space(aText.front()))
	{
		aText.remove_prefix(1);
	}
	while (!aText.empty() && space(aText.back()))
	{
		aText.remove_suffix(1);
	}
	return aText;
}
}

std::string TurnFileText(double aRatio)
{
	return std::format("# BlockRender: build mode's turn, in degrees a count of the mouse, as last measured outside build mode.\n"
	                   "# Delete this file to have it measured afresh.\nratio = {:.6f}\n",
	                   aRatio);
}

std::optional<double> ParseTurnFile(std::string_view aText)
{
	for (size_t start = 0; start < aText.size();)
	{
		const size_t           end = std::min(aText.find('\n', start), aText.size());
		const std::string_view line = aText.substr(start, end - start);
		start = end + 1;
		const size_t equals = line.find('=');
		if (equals == std::string_view::npos || Trim(line.substr(0, equals)) != "ratio")
		{
			continue;  // a comment, or another line
		}
		const std::string_view value = Trim(line.substr(equals + 1));
		double                 ratio = 0.0;
		const auto [stop, error] = std::from_chars(value.data(), value.data() + value.size(), ratio);
		if (error != std::errc() || stop != value.data() + value.size() || !std::isfinite(ratio) || std::abs(ratio) < 1e-4 ||
		    std::abs(ratio) > 1.0)
		{
			return std::nullopt;
		}
		return ratio;
	}
	return std::nullopt;
}

void TurnRatio::Note(int64_t aCounts, double aYaw)
```
```cpp
std::optional<double> TurnRatio::Ratio() const
```
```cpp
std::optional<double> TurnRatio::Measured() const
```
```cpp
double TurnRatio::Turn(int64_t aCounts) const
```
```cpp
std::optional<double> TurnRatio::Ratio() const
{
	const std::optional<double> measured = Measured();
	return measured ? measured : remembered_;
}

bool TurnRatio::Remembered() const
{
	return remembered_.has_value() && ratios_.size() < kKnown;
}

double TurnRatio::Turn(int64_t aCounts) const
```

- [x] **Step 6: Edit `src/Status.h`**

```cpp
	std::optional<double> turnRatio;                 // degrees a count, once measured
```
```cpp
	std::optional<double> turnRatio;                 // degrees a count, once measured or remembered
	bool                  turnRemembered = false;    // turnRatio is the last session's, from turn.txt
```

- [x] **Step 7: Edit `src/Status.cpp`**

```cpp
std::string FormatDrive(const LinkReport& aLink)
```
```cpp
// The turn's ratio: this session's, the last session's until then, or none yet.
std::string TurnRatioText(const LinkReport& aLink)
{
	if (!aLink.turnRatio)
	{
		return std::format("ratio not measured yet ({} of {} turns)", aLink.turnTurns, link::TurnRatio::kKnown);
	}
	return aLink.turnRemembered ? std::format("{:.4f} degrees a count, remembered from the last session ({} of {} turns measured)",
	                                          *aLink.turnRatio, aLink.turnTurns, link::TurnRatio::kKnown)
	                            : std::format("{:.4f} degrees a count from {} turns", *aLink.turnRatio, aLink.turnTurns);
}

std::string FormatDrive(const LinkReport& aLink)
```
```cpp
	                   aLink.turnRatio ? std::format("{:.4f} degrees a count from {} turns", *aLink.turnRatio, aLink.turnTurns)
	                                   : std::format("ratio not measured yet ({} of {} turns)", aLink.turnTurns, link::TurnRatio::kKnown),
	                   MinecraftMovement(aLink.minecraftFlags));
```
```cpp
	                   TurnRatioText(aLink), MinecraftMovement(aLink.minecraftFlags));
```

- [x] **Step 8: Edit `src/Plugin.cpp`**

```cpp
#include <chrono>
```
```cpp
#include <chrono>
#include <cmath>
```
```cpp
bool                  g_turn = true;      // set turn 1, the default since step 2: the teleport's yaw gains the mouse's turn
```
```cpp
bool                  g_turn = true;      // set turn 1, the default since step 2: the teleport's yaw gains the mouse's turn
std::optional<double> g_savedTurn;        // the ratio turn.txt holds: read at load, or saved since
bool                  g_turnSaveLogged = false;
```
```cpp
// The link's lines of the status (the spec's "Measurements"), from the second since the last status.
```
```cpp
// turn.txt keeps the turn ratio for the next session, so build mode turns at once after a game start: saved once this
// session has measured its own, and again whenever that changes, written whole and then renamed into place.
void SaveTurnRatio()
{
	const std::optional<double> measured = g_turnRatio.Measured();
	if (!measured || (g_savedTurn && std::abs(*measured - *g_savedTurn) < 1e-6))
	{
		return;
	}
	const std::filesystem::path temporary = g_pluginDir / "turn.tmp";
	{
		std::ofstream out(temporary, std::ios::binary);
		out << cpmc::link::TurnFileText(*measured);
	}
	std::error_code error;
	std::filesystem::rename(temporary, g_pluginDir / "turn.txt", error);
	if (error)
	{
		return;  // tried again next second
	}
	g_savedTurn = measured;
	if (!std::exchange(g_turnSaveLogged, true))
	{
		Log("the turn: {:.4f} degrees a count, measured, and saved in turn.txt for the next session", *measured);
	}
}

// The link's lines of the status (the spec's "Measurements"), from the second since the last status.
```
```cpp
	report.turnRatio = g_turnRatio.Ratio();
```
```cpp
	report.turnRatio = g_turnRatio.Ratio();
	report.turnRemembered = g_turnRatio.Remembered();
```
```cpp
		}
		WriteStatus(seconds);
```
```cpp
		}
		SaveTurnRatio();
		WriteStatus(seconds);
```
```cpp
		g_linkSettings.ground = cpmc::link::Ground::City;  // Night City's shape from step 3 on; the floor is the fallback
```
```cpp
		g_linkSettings.ground = cpmc::link::Ground::City;  // Night City's shape from step 3 on; the floor is the fallback
		g_savedTurn = cpmc::link::ParseTurnFile(ReadSmallFile(g_pluginDir / "turn.txt"));
		if (g_savedTurn)
		{
			g_turnRatio.Remember(*g_savedTurn);  // build mode turns at once, until this session has measured its own
		}
```
```cpp
		    settings.dred ? "on" : "off", settings.debugLayer ? "on" : "off", (g_pluginDir / "blockrender.request").string());
```
```cpp
		    settings.dred ? "on" : "off", settings.debugLayer ? "on" : "off", (g_pluginDir / "blockrender.request").string());
		Log("the turn: {}", g_savedTurn ? std::format("{:.4f} degrees a count, remembered from the last session, until this one has "
		                                              "measured its own", *g_savedTurn)
		                                : std::string("nothing remembered, so build mode turns once V has turned 5 times"));
```

- [x] **Step 9: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `367 test(s), 0 failure(s)`.

- [x] **Step 10: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: build mode's turn ratio is remembered between sessions" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 39: The ground's worker runs several threads, the nearest region first

Part 2's second follow-up. In a creative drop of about 150 blocks in 3 s into dense ground, every batch had been read,
but the one worker thread had 857, then 1,516 regions queued, at 3.4 to 15 ms a region against 0.74 offline, and
Minecraft's player fell through the grass once. Part 2's recorded status shows the queue: as the area followed the
falling player down, its batches were read within a few frames each, up to 319,000 triangles a second, 240 to 280 to a
region against spot B's 94; the one thread took the regions in the order they were read, so those of the ground under
the player waited behind the ones read before them, higher up. `GroundWorker` changes:
- **Several threads** (a quarter of the processors, from 1 to 4), below normal priority, so the game's own threads come
  first.
- **The job nearest a focus starts first** (`Focus`, SkyCraft's distance, height counting double); until a focus is
  set, jobs start in the order they came. Jobs are given a batch at a time (`Add` takes several), so the nearest of a
  batch's eight starts first.
- **A region's jobs stay in order:** a newer job replaces one of its region that hasn't started, and one under way
  holds back the next job of its region, so an older region's messages can't come after a newer one's and undo it.
- **Why a region takes long** (the follow-up's open question: the game's threads taking the cores, or denser regions):
  the worker counts the triangles it voxelized, and its threads' own CPU cycles (`QueryThreadCycleTime`) against the
  processor clock's (`rdtsc`) while they voxelized. Cycles over ticks is the share of that time the threads ran: near
  1, the regions cost what they cost; well below, other threads kept them waiting. Task 46 reads both in the drop.
- **`Take` never waits:** if a thread holds the jobs for a moment, the messages come at the next call, so the main
  thread is never held up by a thread below its priority; the counters are atomics.

The regions' messages now come in the order they finish, each region's two together, so the Link tests that relied on
the order they were given check by region instead.

**Files:**
- Modify: `src/link/GroundWorker.h`, `src/link/GroundWorker.cpp`, `src/link/Link.cpp`, `tests/TestGroundWorker.cpp`,
  `tests/TestLink.cpp`

- [x] **Step 1: Write the failing tests, `tests/TestGroundWorker.cpp`**

```cpp
#include "Harness.h"

#include "link/GroundWorker.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

using namespace cpmc::link;

namespace
{
// Takes the worker's messages until aCount have come, or 5 s have gone.
std::vector<GroundWorker::Message> TakeAll(GroundWorker& aWorker, size_t aCount)
{
	std::vector<GroundWorker::Message> messages;
	for (int i = 0; i < 5000 && messages.size() < aCount; ++i)
	{
		for (GroundWorker::Message& message : aWorker.Take())
		{
			messages.push_back(std::move(message));
		}
		if (messages.size() < aCount)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
	return messages;
}

// A flat floor over the region's 8 x 8 blocks, at height aY.
std::vector<McTriangle> Floor(const RegionPos& aRegion, float aY)
{
	const float x = float(aRegion.x * kRegionBlocks), z = float(aRegion.z * kRegionBlocks);
	return { { { x, aY, z, x + 8, aY, z, x + 8, aY, z + 8 } }, { { x, aY, z, x + 8, aY, z + 8, x, aY, z + 8 } } };
}

// Dense ground: the floor's two triangles 75 times over, which takes the worker about a tenth of a second.
std::vector<McTriangle> Dense(const RegionPos& aRegion)
{
	std::vector<McTriangle> triangles;
	for (int i = 0; i < 75; ++i)
	{
		for (const McTriangle& triangle : Floor(aRegion, 3.0f))
		{
			triangles.push_back(triangle);
		}
	}
	return triangles;
}

proto::ColRegion HeaderOf(const GroundWorker::Message& aMessage)
{
	proto::ColRegion header{};
	std::memcpy(&header, aMessage.payload.data(), sizeof header);
	return header;
}

// Waits up to 5 s for a job to be under way.
bool UnderWay(const GroundWorker& aWorker)
{
	for (int i = 0; i < 5000 && aWorker.Busy() == 0; ++i)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return aWorker.Busy() > 0;
}
}

TEST(GroundWorkerMakesEachRegionsMessages)
{
	GroundWorker                 worker(1);
	const std::vector<RegionPos> regions = { { -15, 0, 250 }, { -14, 0, 250 }, { -15, -1, 250 } };
	std::vector<GroundWorker::Job> jobs;
	for (const RegionPos& region : regions)
	{
		jobs.push_back({ 3, region, region.y == 0 ? Floor(region, 6.14f) : std::vector<McTriangle>() });
	}
	worker.Add(std::move(jobs));
	CHECK_EQ(worker.Waiting(), size_t(3));
	const auto messages = TakeAll(worker, 6);
	CHECK_EQ(messages.size(), size_t(6));
	CHECK_EQ(worker.Waiting(), size_t(0));  // every job's messages taken
	for (size_t i = 0; i < regions.size() && messages.size() == 6; ++i)
	{
		// Each region's triangles, then its voxels, in the order they came: there's no focus.
		const RegionPos&              r = regions[i];
		const std::vector<McTriangle> triangles = r.y == 0 ? Floor(r, 6.14f) : std::vector<McTriangle>();
		CHECK_EQ(messages[2 * i].type, uint32_t(proto::kColTris));
		CHECK(messages[2 * i].payload == TrisPayload(r.x, r.y, r.z, 3, triangles));
		CHECK_EQ(messages[2 * i + 1].type, uint32_t(proto::kColRegion));
		CHECK(messages[2 * i + 1].payload == RegionPayload(r.x, r.y, r.z, 3, VoxelizeRegion(r.x, r.y, r.z, triangles)));
		CHECK_EQ(messages[2 * i + 1].epoch, 3u);
	}
	CHECK(worker.Take().empty());  // taken already
	CHECK_EQ(worker.Regions(), 3ull);
	CHECK_EQ(worker.Triangles(), 4ull);  // the two floors' two each
	CHECK_EQ(worker.Blocks(), 128ull);   // and their 64 blocks each
	CHECK_EQ(worker.Failed(), 0ull);
	CHECK(worker.Ms() > 0.0);
	CHECK(worker.Cycles() > 0 && worker.Ticks() > 0);  // the thread's own time, against the clock's
}

TEST(GroundWorkerStartsTheJobNearestItsFocus)
{
	// Minecraft's player is at region 49 0 0: of the 50 regions given together, the worker starts there and works back.
	GroundWorker worker(1);
	worker.Focus({ 49, 0, 0 });
	std::vector<GroundWorker::Job> jobs;
	for (int32_t x = 0; x < 50; ++x)
	{
		jobs.push_back({ 1, { x, 0, 0 }, Floor({ x, 0, 0 }, 3.0f) });
	}
	worker.Add(std::move(jobs));
	const auto messages = TakeAll(worker, 100);
	CHECK_EQ(messages.size(), size_t(100));
	for (size_t i = 0; i < messages.size(); ++i)
	{
		CHECK_EQ(HeaderOf(messages[i]).minX, (49 - int32_t(i / 2)) * 8);
	}
}

TEST(GroundWorkerReplacesAJobNotStarted)
{
	// While the one thread voxelizes dense ground, a region is given twice: the newer job replaces the older, which was
	// still waiting, so only the newer triangles go out.
	GroundWorker worker(1);
	worker.Add({ { 1, { 0, 0, 0 }, Dense({ 0, 0, 0 }) } });
	CHECK(UnderWay(worker));
	worker.Add({ { 1, { 5, 0, 0 }, Floor({ 5, 0, 0 }, 3.0f) } });
	worker.Add({ { 1, { 5, 0, 0 }, Floor({ 5, 0, 0 }, 4.0f) } });
	CHECK_EQ(worker.Replaced(), 1ull);
	CHECK_EQ(worker.Waiting(), size_t(2));
	const auto messages = TakeAll(worker, 4);
	CHECK_EQ(messages.size(), size_t(4));
	if (messages.size() == 4)
	{
		CHECK_EQ(HeaderOf(messages[0]).minX, 0);
		CHECK(messages[2].payload == TrisPayload(5, 0, 0, 1, Floor({ 5, 0, 0 }, 4.0f)));
	}
	CHECK(worker.Take().empty());  // nothing more: the replaced job was never made
	CHECK_EQ(worker.Waiting(), size_t(0));
	CHECK_EQ(worker.Regions(), 2ull);
}

TEST(GroundWorkerKeepsARegionsJobsInOrder)
{
	// Two threads: a region's newer job doesn't start while its older one is under way, so the older one's messages,
	// though slower to make, can't come after it and undo it.
	GroundWorker worker(2);
	worker.Add({ { 1, { 0, 0, 0 }, Dense({ 0, 0, 0 }) } });
	CHECK(UnderWay(worker));
	worker.Add({ { 1, { 0, 0, 0 }, Floor({ 0, 0, 0 }, 4.0f) } });
	const auto messages = TakeAll(worker, 4);
	CHECK_EQ(messages.size(), size_t(4));
	if (messages.size() == 4)
	{
		CHECK_EQ(HeaderOf(messages[0]).count, 150u);  // the dense ground's triangles first
		CHECK(messages[2].payload == TrisPayload(0, 0, 0, 1, Floor({ 0, 0, 0 }, 4.0f)));
	}
	CHECK_EQ(worker.Replaced(), 0ull);
}

TEST(GroundWorkerRunsSeveralThreads)
{
	CHECK(GroundWorker::DefaultThreads() >= 1 && GroundWorker::DefaultThreads() <= 4);
	GroundWorker worker(3);
	CHECK_EQ(worker.Threads(), 3u);
	std::vector<GroundWorker::Job> jobs;
	for (int32_t x = 0; x < 30; ++x)
	{
		jobs.push_back({ 1, { x, 0, 0 }, Floor({ x, 0, 0 }, 3.0f) });
	}
	worker.Add(std::move(jobs));
	const auto messages = TakeAll(worker, 60);
	CHECK_EQ(messages.size(), size_t(60));
	std::vector<bool> seen(30, false);
	for (size_t i = 0; i + 1 < messages.size(); i += 2)
	{
		// Each region's two messages stay together, whichever thread made them.
		const int32_t x = HeaderOf(messages[i]).minX / 8;
		CHECK(messages[i].type == proto::kColTris && messages[i + 1].type == proto::kColRegion);
		CHECK_EQ(HeaderOf(messages[i + 1]).minX, x * 8);
		if (x >= 0 && x < 30)
		{
			seen[size_t(x)] = true;
		}
	}
	CHECK(std::all_of(seen.begin(), seen.end(), [](bool aSeen) { return aSeen; }));
	CHECK_EQ(worker.Waiting(), size_t(0));
}

TEST(GroundWorkerStopsWithJobsLeft)
{
	// Destroyed with 2,000 regions waiting, about two seconds of work for its one thread: it drops them, rather than
	// finishing them first.
	const auto start = std::chrono::steady_clock::now();
	{
		GroundWorker                   worker(1);
		std::vector<GroundWorker::Job> jobs;
		for (int32_t x = 0; x < 2000; ++x)
		{
			jobs.push_back({ 1, { x, 0, 0 }, Floor({ x, 0, 0 }, 3.0f) });
		}
		worker.Add(std::move(jobs));
	}
	CHECK(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500));
}
```
- [x] **Step 2: Edit `tests/TestLink.cpp`: the city's messages checked by region**

```cpp
	if (messages.size() == 16)
	{
		CHECK_EQ(messages[0].type, uint32_t(proto::kColTris));
		CHECK_EQ(HeaderOf(payloads[0]).count, 2u);  // the street's two triangles
		CHECK_EQ(messages[1].type, uint32_t(proto::kColRegion));
		CHECK_EQ(HeaderOf(payloads[1]).minX, -128);
		CHECK_EQ(HeaderOf(payloads[1]).count, 64u);  // the street, in block row 6
		CHECK_EQ(HeaderOf(payloads[1]).epoch, 1u);
		CHECK_EQ(HeaderOf(payloads[9]).minY, 8);  // the regions above: empty, and known
		CHECK_EQ(HeaderOf(payloads[9]).count, 0u);
	}
```
```cpp
	size_t streets = 0, above = 0;
	for (size_t i = 0; i + 1 < messages.size(); i += 2)
	{
		// The worker's threads may finish the regions in any order, each region's two messages together.
		CHECK(messages[i].type == proto::kColTris && messages[i + 1].type == proto::kColRegion);
		const proto::ColRegion tris = HeaderOf(payloads[i]);
		const proto::ColRegion voxels = HeaderOf(payloads[i + 1]);
		CHECK(tris.minX == voxels.minX && tris.minY == voxels.minY && tris.minZ == voxels.minZ && voxels.epoch == 1);
		if (voxels.minY == 0)
		{
			streets += tris.count == 2 && voxels.count == 64;  // the street's two triangles, in block row 6
		}
		else
		{
			above += voxels.minY == 8 && tris.count == 0 && voxels.count == 0;  // the regions above: empty, and known
		}
	}
	CHECK_EQ(streets, size_t(4));
	CHECK_EQ(above, size_t(4));
```
```cpp
TEST(LinkKeepsTheCitysMessagesInOrderUntilTheRingHasRoom)
```
```cpp
TEST(LinkKeepsTheCitysMessagesUntilTheRingHasRoom)
```
```cpp
	// Minecraft reads again: the next update writes them all, in their order.
```
```cpp
	// Minecraft reads again: the next update writes them all, each region's triangles then its voxels, whichever of the
	// worker's threads finished first.
```
```cpp
	for (size_t i = 0; i < 8 && messages.size() == 16; ++i)
	{
		CHECK(messages[2 * i].type == proto::kColTris && messages[2 * i + 1].type == proto::kColRegion);
		CHECK(HeaderOf(payloads[2 * i]).minX == regions[i].x * 8 && HeaderOf(payloads[2 * i]).minY == regions[i].y * 8 &&
		      HeaderOf(payloads[2 * i]).minZ == regions[i].z * 8);
	}
```
```cpp
	std::array<bool, 8>            seen{};
	for (size_t i = 0; i + 1 < messages.size(); i += 2)
	{
		CHECK(messages[i].type == proto::kColTris && messages[i + 1].type == proto::kColRegion);
		const proto::ColRegion tris = HeaderOf(payloads[i]);
		const proto::ColRegion voxels = HeaderOf(payloads[i + 1]);
		CHECK(tris.minX == voxels.minX && tris.minY == voxels.minY && tris.minZ == voxels.minZ);
		for (size_t r = 0; r < regions.size(); ++r)
		{
			seen[r] = seen[r] || (tris.minX == regions[r].x * 8 && tris.minY == regions[r].y * 8 && tris.minZ == regions[r].z * 8);
		}
	}
	CHECK(std::all_of(seen.begin(), seen.end(), [](bool aSeen) { return aSeen; }));
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'Cycles': is not a member of 'cpmc::link::GroundWorker'`.

- [x] **Step 4: Write `src/link/GroundWorker.h`**

```cpp
#pragma once

#include "link/Ground.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

// The ground's worker (spike 5's "The worker"): threads voxelize each region they're given, with SkyCraft's
// triangle-box test, and make its messages, the triangles (kColTris) then the voxels (kColRegion), as SkyCraft's worker
// sends them. Voxelizing takes up to a few milliseconds a region, too long for the game's main thread, which only reads
// the triangles and writes the finished messages into the ring.
//
// A fast drop into dense ground once queued 1,500 regions for a single thread, and Minecraft's player fell through the
// ground (Part 2's follow-up). So several threads, below normal priority so the game's own threads come first, start the
// job nearest the focus (Minecraft's player) first. A region's newer job replaces one that hasn't started, and doesn't
// start while an older one of its region is under way, so the older one's messages can't come after it.
namespace cpmc::link
{
class GroundWorker
{
public:
	struct Job
	{
		uint32_t                epoch = 0;
		RegionPos               region;
		std::vector<McTriangle> triangles;  // filed under the region (FileBatch)
	};

	struct Message
	{
		uint32_t             epoch = 0;
		uint32_t             type = 0;  // kColTris or kColRegion
		std::vector<uint8_t> payload;
	};

	static unsigned DefaultThreads();  // a quarter of the processors, from 1 to 4

	explicit GroundWorker(unsigned aThreads = DefaultThreads());
	~GroundWorker();  // drops the jobs not started, and waits for those under way
	GroundWorker(const GroundWorker&) = delete;
	GroundWorker& operator=(const GroundWorker&) = delete;

	// Jobs, added together. Each replaces a job of its region that hasn't started: its triangles are newer.
	void Add(std::vector<Job> aJobs);
	// Where the regions are wanted first: the job nearest it, as SkyCraft orders regions (height counting double), starts
	// next. Until it's set, jobs start in the order they came.
	void Focus(const RegionPos& aRegion);
	// The messages finished since the last call: each region's two together, in the order the regions finished. Nothing
	// while a thread holds the jobs for a moment: the main thread never waits, and they come at the next call.
	std::vector<Message> Take();

	unsigned Threads() const { return unsigned(threads_.size()); }
	size_t   Waiting() const { return inHand_.load(); }  // jobs added whose messages haven't been taken yet
	size_t   Busy() const { return busy_.load(); }       // jobs under way now
	uint64_t Regions() const { return regions_.load(); }      // regions voxelized, since the start
	uint64_t Triangles() const { return triangles_.load(); }  // their triangles
	uint64_t Blocks() const { return blocks_.load(); }        // voxel blocks made
	uint64_t Failed() const { return failed_.load(); }        // regions left out, for want of memory
	uint64_t Replaced() const { return replaced_.load(); }    // jobs replaced by a newer one of their region
	double   Ms() const { return ms_.load(); }                // time spent voxelizing and making messages, all threads
	// Meanwhile, the threads' own CPU cycles (QueryThreadCycleTime), and the processor clock's (rdtsc): Cycles over Ticks
	// is the share of that time they were running, so whether other threads kept them waiting.
	uint64_t Cycles() const { return cycles_.load(); }
	uint64_t Ticks() const { return ticks_.load(); }

private:
	void Run();
	// The job to start next: the nearest the focus whose region has no job under way; the end if none can start.
	std::deque<Job>::iterator Next();

	mutable std::mutex       mutex_;
	std::condition_variable  wake_;
	std::deque<Job>          jobs_;
	std::vector<RegionPos>   underWay_;  // the regions of the jobs under way
	std::vector<Message>     done_;
	size_t                   finished_ = 0;  // jobs finished since the last Take: their messages are in done_, or they failed
	std::optional<RegionPos> focus_;
	bool                     stop_ = false;
	std::atomic<size_t>      inHand_{ 0 };
	std::atomic<size_t>      busy_{ 0 };
	std::atomic<uint64_t>    regions_{ 0 };
	std::atomic<uint64_t>    triangles_{ 0 };
	std::atomic<uint64_t>    blocks_{ 0 };
	std::atomic<uint64_t>    failed_{ 0 };
	std::atomic<uint64_t>    replaced_{ 0 };
	std::atomic<double>      ms_{ 0.0 };
	std::atomic<uint64_t>    cycles_{ 0 };
	std::atomic<uint64_t>    ticks_{ 0 };
	std::vector<std::thread> threads_;  // last: they start once the rest is made
};
}
```
- [x] **Step 5: Write `src/link/GroundWorker.cpp`**

```cpp
#include "link/GroundWorker.h"

#include <Windows.h>
#include <intrin.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <utility>

namespace cpmc::link
{
namespace
{
// SkyCraft's distance between regions, for its order: height counts double.
int64_t Distance(const RegionPos& aRegion, const RegionPos& aFocus)
{
	const int64_t dx = int64_t(aRegion.x) - aFocus.x, dy = int64_t(aRegion.y) - aFocus.y, dz = int64_t(aRegion.z) - aFocus.z;
	return dx * dx + dz * dz + 2 * dy * dy;
}

uint64_t ThreadCycles()
{
	ULONG64 cycles = 0;
	QueryThreadCycleTime(GetCurrentThread(), &cycles);
	return cycles;
}
}

unsigned GroundWorker::DefaultThreads()
{
	return std::clamp(std::thread::hardware_concurrency() / 4, 1u, 4u);
}

GroundWorker::GroundWorker(unsigned aThreads)
{
	for (unsigned i = 0; i < std::max(aThreads, 1u); ++i)
	{
		threads_.emplace_back([this] { Run(); });
	}
}

GroundWorker::~GroundWorker()
{
	{
		std::lock_guard lock(mutex_);
		stop_ = true;
	}
	wake_.notify_all();
	for (std::thread& thread : threads_)
	{
		thread.join();
	}
}

void GroundWorker::Add(std::vector<Job> aJobs)
{
	{
		std::lock_guard lock(mutex_);
		for (Job& job : aJobs)
		{
			const auto older = std::find_if(jobs_.begin(), jobs_.end(), [&](const Job& aJob) { return aJob.region == job.region; });
			if (older != jobs_.end())
			{
				*older = std::move(job);  // in its place: the older one would only be undone by this
				++replaced_;
				continue;
			}
			jobs_.push_back(std::move(job));
			++inHand_;
		}
	}
	wake_.notify_all();
}

void GroundWorker::Focus(const RegionPos& aRegion)
{
	std::lock_guard lock(mutex_);
	focus_ = aRegion;
}

std::vector<GroundWorker::Message> GroundWorker::Take()
{
	std::unique_lock lock(mutex_, std::try_to_lock);
	if (!lock.owns_lock())
	{
		return {};  // a thread has the jobs for a moment: they come at the next call
	}
	inHand_ -= finished_;
	finished_ = 0;
	return std::exchange(done_, {});
}

std::deque<GroundWorker::Job>::iterator GroundWorker::Next()
{
	auto    best = jobs_.end();
	int64_t nearest = 0;
	for (auto job = jobs_.begin(); job != jobs_.end(); ++job)
	{
		if (std::find(underWay_.begin(), underWay_.end(), job->region) != underWay_.end())
		{
			continue;  // its region's older job first
		}
		if (!focus_)
		{
			return job;  // in the order they came
		}
		const int64_t distance = Distance(job->region, *focus_);
		if (best == jobs_.end() || distance < nearest)
		{
			best = job;
			nearest = distance;
		}
	}
	return best;
}

void GroundWorker::Run()
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);  // the game's own threads come first
	std::unique_lock lock(mutex_);
	while (true)
	{
		auto next = jobs_.end();
		wake_.wait(lock, [&] { return stop_ || (next = Next()) != jobs_.end(); });
		if (stop_)
		{
			return;
		}
		const Job job = std::move(*next);
		jobs_.erase(next);
		underWay_.push_back(job.region);
		++busy_;
		lock.unlock();
		const auto     start = std::chrono::steady_clock::now();
		const uint64_t cyclesBefore = ThreadCycles();
		const uint64_t ticksBefore = __rdtsc();
		std::vector<Message> messages;
		size_t               blocks = 0;
		try
		{
			const RegionPos&                   at = job.region;
			const std::vector<proto::ColBlock> voxels = VoxelizeRegion(at.x, at.y, at.z, job.triangles);
			blocks = voxels.size();
			messages.push_back({ job.epoch, proto::kColTris, TrisPayload(at.x, at.y, at.z, job.epoch, job.triangles) });
			messages.push_back({ job.epoch, proto::kColRegion, RegionPayload(at.x, at.y, at.z, job.epoch, voxels) });
		}
		catch (const std::exception&)
		{
			messages.clear();  // out of memory, say: the region is left out, rather than the game ended
		}
		const uint64_t cycles = ThreadCycles() - cyclesBefore;
		const uint64_t ticks = __rdtsc() - ticksBefore;
		const double   ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		lock.lock();
		underWay_.erase(std::find(underWay_.begin(), underWay_.end(), job.region));
		--busy_;
		++finished_;
		ms_ += ms;
		cycles_ += cycles;
		ticks_ += ticks;
		if (messages.empty())
		{
			++failed_;
		}
		else
		{
			for (Message& message : messages)
			{
				done_.push_back(std::move(message));
			}
			++regions_;
			triangles_ += job.triangles.size();
			blocks_ += blocks;
		}
		wake_.notify_all();  // the region is free: a newer job of it may start
	}
}
}
```
- [x] **Step 6: Edit `src/link/Link.cpp`: a batch's regions given together**

```cpp
		const std::array<RegionPos, 8>         regions = RegionsOf(*batch);
```
```cpp
		const std::array<RegionPos, 8>         regions = RegionsOf(*batch);
		std::vector<GroundWorker::Job>         jobs;
```
```cpp
			worker_->Add({ status_.collisionEpoch, regions[i], std::move(filed[i]) });
		}
```
```cpp
			jobs.push_back({ status_.collisionEpoch, regions[i], std::move(filed[i]) });
		}
		worker_->Add(std::move(jobs));
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `370 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the ground's worker runs several threads, the nearest region first" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 40: The worker starts at the city's centre, and the status says why a region takes long

The link gives the worker its focus: the city's centre, Minecraft's player's region while it drives and V's otherwise,
whenever that changes. `Link` takes the worker's thread count (tests give 1, so the order a batch's regions finish in is
known), and reports the worker's threads, triangles, cycles and ticks, and the jobs replaced. The ground line then says,
for the last second, how long a region took and of how many triangles, and what share of that time the worker's threads
ran; and `mclink` prints the same.

**Files:**
- Modify: `src/link/Link.h`, `src/link/Link.cpp`, `src/Status.h`, `src/Status.cpp`, `src/Plugin.cpp`,
  `tools/mclink/Main.cpp`, `tests/TestLink.cpp`, `tests/TestStatus.cpp`

- [x] **Step 1: Edit `tests/TestLink.cpp`: the failing test**

```cpp
TEST(LinkKeepsTheCitysMessagesUntilTheRingHasRoom)
```
```cpp
TEST(LinkGivesTheWorkerTheCitysCentreFirst)
{
	// V's region, -15 0 250, is the second of its batch's eight, as the batch lists them. With the city centred there, the
	// worker's one thread starts with it, then the regions next to it.
	FakeMinecraft                     minecraft;
	Link                              link(minecraft.base, 0, 1);
	Sink                              sink;
	Street                            street;
	std::vector<std::vector<uint8_t>> payloads;
	minecraft.Beat(77, 1000);
	link.Update(At(1.0), City(), sink);
	minecraft.Collision(payloads);  // the clear
	payloads.clear();
	link.ReadGround(2.5, street, 0.0);
	CHECK(Finish(link, At(2.5), City(), sink));
	const auto messages = minecraft.Collision(payloads);
	CHECK_EQ(messages.size(), size_t(16));
	if (messages.size() == 16)
	{
		CHECK(HeaderOf(payloads[0]).minX == -120 && HeaderOf(payloads[0]).minY == 0 && HeaderOf(payloads[0]).minZ == 2000);
		CHECK_EQ(HeaderOf(payloads[14]).minY, 8);  // the farthest, a region up and across: last
		CHECK_EQ(HeaderOf(payloads[14]).minX, -128);
		CHECK_EQ(HeaderOf(payloads[14]).minZ, 2008);
	}
	CHECK_EQ(link.State().workerThreads, 1u);
	CHECK_EQ(link.State().workerTriangles, 8ull);  // the street's two, in each of the four regions it crosses
	CHECK(link.State().workerCycles > 0 && link.State().workerTicks > 0);
	CHECK_EQ(link.State().workerReplaced, 0ull);
}

TEST(LinkKeepsTheCitysMessagesUntilTheRingHasRoom)
```

- [x] **Step 2: Edit `tests/TestStatus.cpp`: the failing test**

```cpp
	link.workerMsPerRegion = 0.68;
```
```cpp
	link.workerMsPerRegion = 0.68;
	link.workerTrianglesPerRegion = 120.4;
	link.workerRunning = 0.93;
	link.workerThreads = 4;
	link.workerReplaced = 12;
```
```cpp
	          "at worst; 2.0 batches, 1300 triangles and 450 voxel blocks a second; the worker 0.68 ms a region, 0 regions waiting; the "
	          "ring 0.12 MB a second, 0 messages waiting for room, 0 too big; Minecraft held its player 0.07 s when build mode last "
```
```cpp
	          "at worst; 2.0 batches, 1300 triangles and 450 voxel blocks a second; the worker 0.68 ms a region of 120 triangles, its "
	          "threads running 93% of that time; 4 threads, 0 regions waiting, 12 replaced by newer ones since the start; the ring 0.12 "
	          "MB a second, 0 messages waiting for room, 0 too big; Minecraft held its player 0.07 s when build mode last "
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2661: 'cpmc::link::Link::Link': no overloaded function takes 3 arguments`.

- [x] **Step 4: Edit `src/link/Link.h`**

```cpp
	// aBase: the mapping's memory, laid out as skycraft_protocol.h has it. aFirstTeleportSeq: tests give their own.
	explicit Link(uint8_t* aBase, uint32_t aFirstTeleportSeq = FirstTeleportSeq());
```
```cpp
	// aBase: the mapping's memory, laid out as skycraft_protocol.h has it. aFirstTeleportSeq and aGroundThreads, the
	// ground worker's threads: tests give their own.
	explicit Link(uint8_t* aBase, uint32_t aFirstTeleportSeq = FirstTeleportSeq(), unsigned aGroundThreads = GroundWorker::DefaultThreads());
```
```cpp
		size_t                   workerJobs = 0;        // regions given to the worker whose messages aren't taken yet
		uint64_t                 workerRegions = 0;     // regions it voxelized, since the start
		uint64_t                 workerBlocks = 0;      // voxel blocks it made
		double                   workerMs = 0.0;        // its time
```
```cpp
		size_t                   workerJobs = 0;        // regions given to the worker whose messages aren't taken yet
		unsigned                 workerThreads = 0;     // its threads, once it's made
		uint64_t                 workerRegions = 0;     // regions it voxelized, since the start
		uint64_t                 workerTriangles = 0;   // their triangles
		uint64_t                 workerBlocks = 0;      // voxel blocks it made
		uint64_t                 workerReplaced = 0;    // regions it was given again before it started them
		double                   workerMs = 0.0;        // its time, all threads together
		uint64_t                 workerCycles = 0;      // its threads' CPU cycles meanwhile, and the processor clock's:
		uint64_t                 workerTicks = 0;       // the share of that time they were running
```
```cpp
	GroundAreas                       areas_;
	std::unique_ptr<GroundWorker>     worker_;  // made when the city is first read
```
```cpp
	GroundAreas                       areas_;
	unsigned                          groundThreads_;
	std::unique_ptr<GroundWorker>     worker_;  // made when the city is first read
	std::optional<RegionPos>          focus_;   // the worker's, as last given it: the city's centre
```

- [x] **Step 5: Edit `src/link/Link.cpp`**

```cpp
Link::Link(uint8_t* aBase, uint32_t aFirstTeleportSeq)
    : base_(aBase), overlay_(aBase + proto::kOffOverlayCtl, aBase + proto::kOffOverlaySlotHdr, aBase + proto::kOffOverlayPixels,
                             proto::kOverlaySlotBytes)
```
```cpp
Link::Link(uint8_t* aBase, uint32_t aFirstTeleportSeq, unsigned aGroundThreads)
    : base_(aBase), overlay_(aBase + proto::kOffOverlayCtl, aBase + proto::kOffOverlaySlotHdr, aBase + proto::kOffOverlayPixels,
                             proto::kOverlaySlotBytes)
    , groundThreads_(aGroundThreads)
```
```cpp
	status_.workerJobs = worker_ ? worker_->Waiting() : 0;
	status_.workerRegions = worker_ ? worker_->Regions() : 0;
	status_.workerBlocks = worker_ ? worker_->Blocks() : 0;
	status_.workerMs = worker_ ? worker_->Ms() : 0.0;
```
```cpp
	status_.workerJobs = worker_ ? worker_->Waiting() : 0;
	status_.workerThreads = worker_ ? worker_->Threads() : 0;
	status_.workerRegions = worker_ ? worker_->Regions() : 0;
	status_.workerTriangles = worker_ ? worker_->Triangles() : 0;
	status_.workerBlocks = worker_ ? worker_->Blocks() : 0;
	status_.workerReplaced = worker_ ? worker_->Replaced() : 0;
	status_.workerMs = worker_ ? worker_->Ms() : 0.0;
	status_.workerCycles = worker_ ? worker_->Cycles() : 0;
	status_.workerTicks = worker_ ? worker_->Ticks() : 0;
```
```cpp
		worker_ = std::make_unique<GroundWorker>();
```
```cpp
		worker_ = std::make_unique<GroundWorker>(groundThreads_);
	}
	if (focus_ != areas_.CentreRegion())
	{
		// The worker starts the regions nearest the city's centre first: Minecraft's player while it drives, V otherwise.
		focus_ = areas_.CentreRegion();
		worker_->Focus(*focus_);
```

- [x] **Step 6: Edit `src/Status.h`**

```cpp
	double                         workerMsPerRegion = -1.0;  // below 0: no region voxelized in the last second
	size_t                         workerJobs = 0;            // regions waiting for it
```
```cpp
	double                         workerMsPerRegion = -1.0;  // below 0: no region voxelized in the last second
	double                         workerTrianglesPerRegion = 0.0;  // of those regions
	double                         workerRunning = 0.0;       // the share of that time its threads were running
	unsigned                       workerThreads = 0;         // none: it hasn't started
	size_t                         workerJobs = 0;            // regions waiting for it
	uint64_t                       workerReplaced = 0;        // regions given again before it started them, since the start
```

- [x] **Step 7: Edit `src/Status.cpp`**

```cpp
std::string FormatGround(const LinkReport& aLink)
```
```cpp
// The ground's worker: how long a region took in the last second, of how many triangles, and how much of that time its
// threads ran, which says whether the game's threads kept them waiting.
std::string Worker(const LinkReport& aLink)
{
	if (aLink.workerThreads == 0)
	{
		return "the worker not started yet";
	}
	const std::string work = aLink.workerMsPerRegion < 0.0
	                             ? std::string("idle")
	                             : std::format("{:.2f} ms a region of {:.0f} triangles, its threads running {:.0f}% of that time",
	                                           aLink.workerMsPerRegion, aLink.workerTrianglesPerRegion, aLink.workerRunning * 100.0);
	return std::format("the worker {}; {} threads, {} regions waiting, {} replaced by newer ones since the start", work,
	                   aLink.workerThreads, aLink.workerJobs, aLink.workerReplaced);
}

std::string FormatGround(const LinkReport& aLink)
```
```cpp
	                   "a second; the worker {}, {} regions waiting; the ring {:.2f} MB a second, {} messages waiting for room, {} too big",
	                   aLink.cityReadMsAverage, aLink.cityReadMsWorst, aLink.cityBatches, aLink.cityTriangles, aLink.workerBlocks,
	                   aLink.workerMsPerRegion < 0.0 ? std::string("idle") : std::format("{:.2f} ms a region", aLink.workerMsPerRegion),
	                   aLink.workerJobs, aLink.collisionMb, aLink.collisionWaiting, aLink.collisionDropped);
```
```cpp
	                   "a second; {}; the ring {:.2f} MB a second, {} messages waiting for room, {} too big",
	                   aLink.cityReadMsAverage, aLink.cityReadMsWorst, aLink.cityBatches, aLink.cityTriangles, aLink.workerBlocks,
	                   Worker(aLink), aLink.collisionMb, aLink.collisionWaiting, aLink.collisionDropped);
```

- [x] **Step 8: Edit `src/Plugin.cpp`**

```cpp
	uint64_t workerRegionsAtStart = 0;
	uint64_t workerBlocksAtStart = 0;
	double   workerMsAtStart = 0.0;
```
```cpp
	uint64_t workerRegionsAtStart = 0;
	uint64_t workerTrianglesAtStart = 0;
	uint64_t workerBlocksAtStart = 0;
	double   workerMsAtStart = 0.0;
	uint64_t workerCyclesAtStart = 0;
	uint64_t workerTicksAtStart = 0;
```
```cpp
		const uint64_t regions = state.workerRegions - g_second.workerRegionsAtStart;
		report.workerMsPerRegion = regions ? (state.workerMs - g_second.workerMsAtStart) / double(regions) : -1.0;
		report.workerJobs = state.workerJobs;
```
```cpp
		const uint64_t regions = state.workerRegions - g_second.workerRegionsAtStart;
		const uint64_t ticks = state.workerTicks - g_second.workerTicksAtStart;
		report.workerMsPerRegion = regions ? (state.workerMs - g_second.workerMsAtStart) / double(regions) : -1.0;
		report.workerTrianglesPerRegion = regions ? double(state.workerTriangles - g_second.workerTrianglesAtStart) / double(regions) : 0.0;
		report.workerRunning = ticks ? double(state.workerCycles - g_second.workerCyclesAtStart) / double(ticks) : 0.0;
		report.workerThreads = state.workerThreads;
		report.workerJobs = state.workerJobs;
		report.workerReplaced = state.workerReplaced;
```
```cpp
		g_second.workerRegionsAtStart = state.workerRegions;
		g_second.workerBlocksAtStart = state.workerBlocks;
		g_second.workerMsAtStart = state.workerMs;
```
```cpp
		g_second.workerRegionsAtStart = state.workerRegions;
		g_second.workerTrianglesAtStart = state.workerTriangles;
		g_second.workerBlocksAtStart = state.workerBlocks;
		g_second.workerMsAtStart = state.workerMs;
		g_second.workerCyclesAtStart = state.workerCycles;
		g_second.workerTicksAtStart = state.workerTicks;
```

- [x] **Step 9: Edit `tools/mclink/Main.cpp`**

```cpp
	Say("ground: the {}, epoch {}; {}; {} batches read ({} unread), {} regions sent ({} triangles); the worker: {} regions, {} voxel "
	    "blocks, {:.1f} ms, {} waiting; ring: {:.2f} MB, {} messages waiting, {} dropped",
```
```cpp
	Say("ground: the {}, epoch {}; {}; {} batches read ({} unread), {} regions sent ({} triangles); the worker: {} threads, {} regions "
	    "({} triangles), {} voxel blocks, {:.1f} ms, its threads running {:.0f}% of it, {} waiting, {} replaced; ring: {:.2f} MB, {} "
	    "messages waiting, {} dropped",
```
```cpp
	    s.cityBatches, s.cityUnread, s.cityRegions, s.cityTriangles, s.workerRegions, s.workerBlocks, s.workerMs, s.workerJobs,
```
```cpp
	    s.cityBatches, s.cityUnread, s.cityRegions, s.cityTriangles, s.workerThreads, s.workerRegions, s.workerTriangles, s.workerBlocks,
	    s.workerMs, s.workerTicks ? 100.0 * double(s.workerCycles) / double(s.workerTicks) : 0.0, s.workerJobs, s.workerReplaced,
```

- [x] **Step 10: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `371 test(s), 0 failure(s)`.

- [x] **Step 11: Run mclink without Minecraft**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && printf 'status\nquit\n' > build/selfcheck.txt \
  && ./build/RelWithDebInfo/mclink.exe build/selfcheck.txt | head -2; echo "exit: ${PIPESTATUS[0]}"
```
Expected: `the mapping: created (process …); waiting for Minecraft`, then `Minecraft: absent, …`, and `exit: 0`. The game
and Minecraft must both be closed: either would hold the mapping, and Minecraft would link.

- [x] **Step 12: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the worker starts at the city's centre, and the status says why a region takes long" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 41: The scene watch counts scene 0's queries too

Step 3's scene watch found no other thread adding, removing, stepping or fetching scene 0 while OnUpdate ran, so the
exporter's reads there race nothing. Part 3 writes to the scene in OnUpdate, so the opposite matters too: PhysX doesn't
lock a scene's queries against changes, and a raycast on another thread while a body is added could read the scene's
query structures as they change. The watch now hooks the three scene queries, `raycast`, `sweep` and `overlap` (slots
94 to 96, found in the game's DLL for Task 45's addendum), and its summary counts them as a kind, `query`, and says how
many calls other threads than OnUpdate's made while it ran. BlockRender's own queries in OnUpdate, the exporter's
overlaps, show as the main thread's. PhysX's batched queries (`PxBatchQuery`) don't go through the scene's vtable, so
the watch can't see them.

**Files:**
- Modify: `src/physx/SceneWatch.h`, `src/physx/SceneWatch.cpp`, `src/physx/SceneHooks.h`, `src/physx/SceneHooks.cpp`,
  `tests/TestSceneWatch.cpp`, `tests/TestAudit.cpp`

- [x] **Step 1: Edit `tests/TestSceneWatch.cpp`: the failing tests**

```cpp
	          "lock 1; no shifts");
```
```cpp
	          "lock 1; other threads made 1 call while OnUpdate ran; no shifts");
}

TEST(SceneWatchCountsQueries)
{
	// Part 3 adds and removes actors in OnUpdate, so no other thread may query scene 0 then either. The main thread's
	// queries in OnUpdate are BlockRender's own: the exporter's overlaps, say.
	SceneRecorder recorder;
	recorder.Start();
	recorder.Note(SceneCall::Overlap, 100, true);
	recorder.Note(SceneCall::Raycast, 100, true);
	recorder.Note(SceneCall::Raycast, 200, false);
	CHECK_STR(WatchSummary(recorder.Threads(), 100, recorder.Shifts(), recorder.ShiftCount()),
	          "thread 100, the main thread: query 2 (2 in OnUpdate); thread 200: query 1; no other thread called while OnUpdate ran; no "
	          "shifts");
	recorder.Note(SceneCall::Sweep, 200, true);
	CHECK_STR(WatchSummary(recorder.Threads(), 100, recorder.Shifts(), recorder.ShiftCount()),
	          "thread 100, the main thread: query 2 (2 in OnUpdate); thread 200: query 2 (1 in OnUpdate); other threads made 1 call "
	          "while OnUpdate ran; no shifts");
```
```cpp
	CHECK(WatchSummary(threads, 1, {}, 0).find("; the other threads: step 9; no shifts") != std::string::npos);
```
```cpp
	CHECK(WatchSummary(threads, 1, {}, 0).find("; the other threads: step 9; no other thread called while OnUpdate ran; no shifts") !=
	      std::string::npos);
```

- [x] **Step 2: Edit `tests/TestAudit.cpp`: the queries' slots**

```cpp
	PxU32                      pairCount = 0;
```
```cpp
	PxU32                      pairCount = 0;
	const PxBoxGeometry        box(1.0f, 1.0f, 1.0f);
	const PxTransform          pose(PxIdentity);
	PxRaycastBuffer            ray;
	PxSweepBuffer              sweep;
	PxOverlapBuffer            overlaps;
```
```cpp
	expect(SceneCall::FetchResultsFinish, SlotOf<PxScene>([](PxScene& a) { a.fetchResultsFinish(); }));
```
```cpp
	expect(SceneCall::FetchResultsFinish, SlotOf<PxScene>([](PxScene& a) { a.fetchResultsFinish(); }));
	expect(SceneCall::Raycast, SlotOf<PxScene>([&](PxScene& a) { (void)a.raycast(PxVec3(0.0f), PxVec3(0.0f, 0.0f, -1.0f), 1.0f, ray); }));
	expect(SceneCall::Sweep, SlotOf<PxScene>([&](PxScene& a) { (void)a.sweep(box, pose, PxVec3(1.0f, 0.0f, 0.0f), 1.0f, sweep); }));
	expect(SceneCall::Overlap, SlotOf<PxScene>([&](PxScene& a) { (void)a.overlap(box, pose, overlaps); }));
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2065: 'Raycast': undeclared identifier`.

- [x] **Step 4: Edit `src/physx/SceneWatch.h`**

```cpp
// fetch the simulation, take the write lock and shift the origin, and whether while OnUpdate runs. That decides where
// the exporter can read the scene. This is the record the hooks keep, and its summary; SceneHooks.h installs the hooks.
```
```cpp
// fetch the simulation, take the write lock and shift the origin, and whether while OnUpdate runs. That decided where
// the exporter can read the scene. Part 3 adds actors to the scene in OnUpdate, so the watch also counts the scene's
// queries (raycasts, sweeps and overlaps), which no other thread may make meanwhile. This is the record the hooks keep,
// and its summary; SceneHooks.h installs the hooks.
```
```cpp
	FetchResultsFinish,
```
```cpp
	FetchResultsFinish,
	Raycast,
	Sweep,
	Overlap,
```
```cpp
constexpr size_t kSceneCalls = 16;
```
```cpp
constexpr size_t kSceneCalls = 19;
```
```cpp
// The status's line for a watch: each thread's calls by kind (add, remove, step, fetch, lock and shift), and of each
// kind, how many came while OnUpdate ran and how many of those without the write lock; then the shifts.
```
```cpp
// The status's line for a watch: each thread's calls by kind (add, remove, step, fetch, query, lock and shift), and of
// each kind, how many came while OnUpdate ran, and of those that write, how many without the write lock; then how many
// calls other threads than aMainThread, OnUpdate's own, made while it ran; then the shifts.
```

- [x] **Step 5: Edit `src/physx/SceneWatch.cpp`**

```cpp
	{ "fetchResultsFinish", 64, 0x53d30 },
```
```cpp
	{ "fetchResultsFinish", 64, 0x53d30 },
	{ "raycast", 94, 0x60890 },
	{ "sweep", 95, 0x60960 },
	{ "overlap", 96, 0x60b70 },
```
```cpp
// The summary's kinds of call. unlockWrite only mirrors lockWrite, so it isn't counted.
```
```cpp
// The summary's kinds of call, and whether they write to the scene. unlockWrite only mirrors lockWrite, so it isn't
// counted.
```
```cpp
	std::initializer_list<SceneCall> calls;
```
```cpp
	std::initializer_list<SceneCall> calls;
	bool                             writes = true;
```
```cpp
	{ "fetch", { SceneCall::FetchResults, SceneCall::FetchResultsStart, SceneCall::FetchResultsFinish } },
```
```cpp
	{ "fetch", { SceneCall::FetchResults, SceneCall::FetchResultsStart, SceneCall::FetchResultsFinish } },
	{ "query", { SceneCall::Raycast, SceneCall::Sweep, SceneCall::Overlap }, false },
```
```cpp
	std::string out;
```
```cpp
	std::string out;
	uint64_t    othersInUpdate = 0;  // calls other threads made while OnUpdate ran
```
```cpp
					kinds += std::format(" ({} in OnUpdate, {} of them without the write lock)", inUpdate, unlocked);
				}
			}
```
```cpp
					kinds += kind.writes ? std::format(" ({} in OnUpdate, {} of them without the write lock)", inUpdate, unlocked)
					                     : std::format(" ({} in OnUpdate)", inUpdate);
				}
			}
			othersInUpdate += thread.id != aMainThread ? inUpdate : 0;
```
```cpp
	}
	if (aShiftCount == 0 || aShifts.empty())
```
```cpp
	}
	else
	{
		out += othersInUpdate == 0 ? "; no other thread called while OnUpdate ran"
		                           : std::format("; other threads made {} call{} while OnUpdate ran", othersInUpdate,
		                                         othersInUpdate == 1 ? "" : "s");
	}
	if (aShiftCount == 0 || aShifts.empty())
```

- [x] **Step 6: Edit `src/physx/SceneHooks.h`**

```cpp
// Step 3's scene watch, in the game (spike 5's "The scene watch"): hooks on the 16 audited slots of scene 0's vtable.
```
```cpp
// Step 3's scene watch, in the game (spike 5's "The scene watch"): hooks on 19 audited slots of scene 0's vtable.
```

- [x] **Step 7: Edit `src/physx/SceneHooks.cpp`**

```cpp
void LockWrite(PxScene* aThis, const char* aFile, PxU32 aLine)
```
```cpp
// The queries are const members, which take their object first all the same.
bool Raycast(PxScene* aThis, const PxVec3& aOrigin, const PxVec3& aDirection, const PxReal aDistance, PxRaycastCallback& aHits,
             PxHitFlags aFlags, const PxQueryFilterData& aFilter, PxQueryFilterCallback* aFilterCall, const PxQueryCache* aCache)
{
	Record(aThis, SceneCall::Raycast);
	return Original<bool (*)(PxScene*, const PxVec3&, const PxVec3&, const PxReal, PxRaycastCallback&, PxHitFlags, const PxQueryFilterData&,
	                         PxQueryFilterCallback*, const PxQueryCache*)>(SceneCall::Raycast)(aThis, aOrigin, aDirection, aDistance, aHits,
	                                                                                           aFlags, aFilter, aFilterCall, aCache);
}

bool Sweep(PxScene* aThis, const PxGeometry& aGeometry, const PxTransform& aPose, const PxVec3& aDirection, const PxReal aDistance,
           PxSweepCallback& aHits, PxHitFlags aFlags, const PxQueryFilterData& aFilter, PxQueryFilterCallback* aFilterCall,
           const PxQueryCache* aCache, const PxReal aInflation)
{
	Record(aThis, SceneCall::Sweep);
	return Original<bool (*)(PxScene*, const PxGeometry&, const PxTransform&, const PxVec3&, const PxReal, PxSweepCallback&, PxHitFlags,
	                         const PxQueryFilterData&, PxQueryFilterCallback*, const PxQueryCache*, const PxReal)>(SceneCall::Sweep)(
	    aThis, aGeometry, aPose, aDirection, aDistance, aHits, aFlags, aFilter, aFilterCall, aCache, aInflation);
}

bool Overlap(PxScene* aThis, const PxGeometry& aGeometry, const PxTransform& aPose, PxOverlapCallback& aHits,
             const PxQueryFilterData& aFilter, PxQueryFilterCallback* aFilterCall)
{
	Record(aThis, SceneCall::Overlap);
	return Original<bool (*)(PxScene*, const PxGeometry&, const PxTransform&, PxOverlapCallback&, const PxQueryFilterData&,
	                         PxQueryFilterCallback*)>(SceneCall::Overlap)(aThis, aGeometry, aPose, aHits, aFilter, aFilterCall);
}

void LockWrite(PxScene* aThis, const char* aFile, PxU32 aLine)
```
```cpp
	reinterpret_cast<void*>(&FetchResultsFinish),
```
```cpp
	reinterpret_cast<void*>(&FetchResultsFinish),
	reinterpret_cast<void*>(&Raycast),
	reinterpret_cast<void*>(&Sweep),
	reinterpret_cast<void*>(&Overlap),
```

- [x] **Step 8: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `372 test(s), 0 failure(s)`.

- [x] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the scene watch counts scene 0's queries too" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 42: The link keeps Minecraft's solid blocks by section

The spec's "Solids". After each section's mesh, SkyCraft's `WorldExporter` sends `kRenSolids`: the section, a count,
and unless the count is 0, 512 bytes of bits, bit x + 16z + 256y set for each block with any collision shape (glass and
slabs count; torches don't). `link/Solids` checks each message, as every render message is checked: coordinates within
spike 4's ranges, a count up to 4,096, the bits there when the count isn't 0, and the count equal to the bits set,
since SkyCraft counts one per bit. It keeps each section's latest bits, with a version that goes up with each change, so
a body built from older bits knows it's out of date. A count of 0 forgets the section, and Minecraft's clear-all
forgets every one, as SkyCraft's own game side does (`NpcBlocks.cpp`). The link keeps them whether drawing is on or not.
`mclink` prints the solids, and those in the 3 × 3 × 3 sections around the stand-in V, where V's bodies would go; its
new script, `solids.txt`, places and breaks blocks for Task 43.

**Files:**
- Create: `src/link/Solids.h`, `src/link/Solids.cpp`, `tests/TestSolids.cpp`, `tools/mclink/solids.txt`
- Modify: `src/link/Link.h`, `src/link/Link.cpp`, `tools/mclink/Main.cpp`, `tests/TestLink.cpp`, `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestSolids.cpp`**

```cpp
#include "Harness.h"

#include "link/Solids.h"

#include <cstring>
#include <initializer_list>
#include <vector>

using namespace cpmc;
using namespace cpmc::link;

namespace
{
// A kRenSolids payload, as SkyCraft's WorldExporter writes it: the section and the count, then the bits unless the count
// is 0. aBlocks are block positions within the section.
std::vector<uint8_t> SolidsPayload(int32_t aX, int32_t aY, int32_t aZ, std::initializer_list<BlockPos> aBlocks)
{
	SolidBits bits{};
	for (const BlockPos& block : aBlocks)
	{
		const int bit = block.x + 16 * block.z + 256 * block.y;
		bits[size_t(bit >> 6)] |= uint64_t(1) << (bit & 63);
	}
	const uint32_t       count = uint32_t(aBlocks.size());
	std::vector<uint8_t> payload(16 + (count ? 512 : 0));
	const int32_t        header[3] = { aX, aY, aZ };
	std::memcpy(payload.data(), header, 12);
	std::memcpy(payload.data() + 12, &count, 4);
	if (count)
	{
		std::memcpy(payload.data() + 16, bits.data(), 512);
	}
	return payload;
}

void SetCount(std::vector<uint8_t>& aPayload, uint32_t aCount)
{
	std::memcpy(aPayload.data() + 12, &aCount, 4);
}
}

TEST(SolidsParseSkyCraftsMessage)
{
	std::string                        error;
	const std::optional<SolidsMessage> solids = ParseSolids(SolidsPayload(-8, 0, 125, { { 0, 0, 0 }, { 15, 0, 0 }, { 0, 15, 15 } }), error);
	CHECK(solids.has_value());
	if (solids)
	{
		CHECK((solids->section == SectionPos{ -8, 0, 125 }));
		CHECK_EQ(solids->count, 3u);
		CHECK_EQ(solids->bits[0], (uint64_t(1) << 0) | (uint64_t(1) << 15));  // bits 0 and 15
		CHECK_EQ(solids->bits[63], uint64_t(1) << 48);                        // bit 4,080: 15z + 15 * 256y
	}
	const std::optional<SolidsMessage> none = ParseSolids(SolidsPayload(3, -4, 7, {}), error);  // the section has none left
	CHECK((none.has_value() && none->count == 0 && none->section == SectionPos{ 3, -4, 7 }));
}

TEST(SolidsRejectWhatIsntSkyCraftsMessage)
{
	std::string error;
	CHECK(!ParseSolids(std::vector<uint8_t>(8), error).has_value());
	CHECK_STR(error, "a solids message of 8 bytes has no room for its header");
	std::vector<uint8_t> payload = SolidsPayload(1, 2, 3, { { 1, 1, 1 } });
	payload.resize(16);
	CHECK(!ParseSolids(payload, error).has_value());
	CHECK_STR(error, "solids for section 1 2 3 claim 1 blocks, with no room for their bits");
	payload = SolidsPayload(1, 2, 3, { { 1, 1, 1 } });
	SetCount(payload, 5000);
	CHECK(!ParseSolids(payload, error).has_value());
	CHECK_STR(error, "solids for section 1 2 3 claim 5000 blocks, more than a section holds");
	SetCount(payload, 2);
	CHECK(!ParseSolids(payload, error).has_value());
	CHECK_STR(error, "solids for section 1 2 3 claim 2 blocks, and their bits hold 1");
	CHECK(!ParseSolids(SolidsPayload(1, 300, 3, { { 1, 1, 1 } }), error).has_value());
	CHECK_STR(error, "solids for section 1 300 3 are out of range");
	CHECK(!ParseSolids(SolidsPayload(-3'000'000, 0, 3, { { 1, 1, 1 } }), error).has_value());
}

TEST(SolidsKeepEachSectionsLatest)
{
	Solids      solids;
	std::string error;
	solids.Set(*ParseSolids(SolidsPayload(0, 0, 0, { { 0, 0, 0 }, { 1, 0, 0 } }), error));
	solids.Set(*ParseSolids(SolidsPayload(1, 0, 0, { { 5, 5, 5 } }), error));
	CHECK_EQ(solids.Sections(), size_t(2));
	CHECK_EQ(solids.Blocks(), 3ull);
	const Solids::Section* first = solids.Find({ 0, 0, 0 });
	CHECK(first != nullptr && first->bits[0] == 3);
	const uint64_t version = first ? first->version : 0;
	// A block placed: the section's bits are replaced, and its version goes up, so its body is rebuilt.
	solids.Set(*ParseSolids(SolidsPayload(0, 0, 0, { { 0, 0, 0 }, { 1, 0, 0 }, { 2, 0, 0 } }), error));
	first = solids.Find({ 0, 0, 0 });
	CHECK(first != nullptr && first->bits[0] == 7 && first->version > version);
	CHECK_EQ(solids.Blocks(), 4ull);
	// Its last block broken: the section goes.
	solids.Set(*ParseSolids(SolidsPayload(0, 0, 0, {}), error));
	CHECK(solids.Find({ 0, 0, 0 }) == nullptr);
	CHECK_EQ(solids.Sections(), size_t(1));
	CHECK_EQ(solids.Blocks(), 1ull);
	CHECK(solids.Find({ 2, 0, 0 }) == nullptr);  // never had any
	// Minecraft's clear-all: everything goes.
	solids.Clear();
	CHECK_EQ(solids.Sections(), size_t(0));
	CHECK_EQ(solids.Blocks(), 0ull);
}
```
- [x] **Step 2: Edit `tests/TestLink.cpp`: the failing test**

```cpp
	std::memcpy(payload.data() + 12, &aClaimed, 4);
```
```cpp
	std::memcpy(payload.data() + 12, &aClaimed, 4);
	return payload;
}

// A kRenSolids payload for section aX 0 0, with its first aBlocks blocks solid, as SkyCraft sends it.
std::vector<uint8_t> SolidsPayload(int32_t aX, uint32_t aBlocks)
{
	std::vector<uint8_t> payload(16 + (aBlocks ? 512 : 0));
	std::memcpy(payload.data(), &aX, 4);
	std::memcpy(payload.data() + 12, &aBlocks, 4);
	for (uint32_t bit = 0; bit < aBlocks; ++bit)
	{
		payload[16 + bit / 8] |= uint8_t(1u << (bit % 8));
	}
```
```cpp
	CHECK_EQ(link.State().resyncs, 0ull);
```
```cpp
	CHECK_EQ(link.State().resyncs, 0ull);
}

TEST(LinkKeepsMinecraftsSolids)
{
	// Solids are kept by section as Minecraft sends them, checked like the other render messages. A section with none
	// left goes, and so does everything at a clear-all.
	FakeMinecraft minecraft;
	Link          link(minecraft.base, 0);
	Sink          sink;
	minecraft.Send(proto::kRenSolids, SolidsPayload(1, 3));
	minecraft.Send(proto::kRenSolids, SolidsPayload(2, 5));
	std::vector<uint8_t> bad = SolidsPayload(3, 4);
	bad.resize(16);  // claims 4 blocks, without their bits
	minecraft.Send(proto::kRenSolids, bad);
	link.Update(At(1.0), kSettings, sink);
	CHECK_EQ(link.SolidSections().Sections(), size_t(2));
	CHECK_EQ(link.State().solidSections, size_t(2));
	CHECK_EQ(link.State().solidBlocks, 8ull);
	CHECK_EQ(link.State().messages[proto::kRenSolids], 3ull);
	CHECK_EQ(link.State().rejected, 1ull);
	CHECK(link.State().lastRejected.find("no room for their bits") != std::string::npos);
	const Solids::Section* first = link.SolidSections().Find({ 1, 0, 0 });
	CHECK(first != nullptr && first->bits[0] == 7);
	minecraft.Send(proto::kRenSolids, SolidsPayload(1, 0));  // section 1 0 0 has none left
	link.Update(At(1.1), kSettings, sink);
	CHECK(link.SolidSections().Find({ 1, 0, 0 }) == nullptr);
	CHECK_EQ(link.State().solidBlocks, 5ull);
	minecraft.Send(proto::kRenClearAll, {});
	link.Update(At(1.2), kSettings, sink);
	CHECK_EQ(link.State().solidSections, size_t(0));
	CHECK_EQ(link.State().solidBlocks, 0ull);
	CHECK_EQ(sink.clears, 1);
```

- [x] **Step 3: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestGroundWorker.cpp
	tests/TestAudit.cpp
```
```cmake
	tests/TestGroundWorker.cpp
	tests/TestSolids.cpp
	tests/TestAudit.cpp
```

- [x] **Step 4: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'SolidSections': is not a member of 'cpmc::link::Link'`, and further down,
`error C1083: Cannot open include file: 'link/Solids.h'`.

- [x] **Step 5: Write `src/link/Solids.h`**

```cpp
#pragma once

#include "world/BlockWorld.h"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>

// Minecraft's solid blocks (spike 5's "Part 3", "Solids"): after each section's mesh, SkyCraft sends kRenSolids, which of
// the section's blocks have any collision shape, as a 4,096-bit set (bit x + 16z + 256y). The link keeps them by section,
// for V's bodies, whether drawing is on or not, as it does sections. Shared memory is written by another process, so each
// message is checked before it's kept. Pure.
namespace cpmc::link
{
using SolidBits = std::array<uint64_t, 64>;  // bit x + 16z + 256y: the section's block (x, y, z) is solid

struct SolidsMessage
{
	SectionPos section;
	uint32_t   count = 0;  // solid blocks; 0: the section has none left
	SolidBits  bits{};
};

// A kRenSolids payload: {i32 sx, sy, sz; u32 count}, then 512 bytes of bits unless the count is 0. Nothing, with aError
// saying why, if the coordinates are out of spike 4's ranges, the count is over 4,096 or isn't the number of bits set, or
// the bits are missing.
std::optional<SolidsMessage> ParseSolids(std::span<const uint8_t> aPayload, std::string& aError);

class Solids
{
public:
	struct Section
	{
		SolidBits bits{};
		uint32_t  count = 0;
		uint64_t  version = 0;  // new with each change: a body built from an older one is out of date
	};

	// A section's solids, replacing what it had: a count of 0 forgets the section.
	void Set(const SolidsMessage& aMessage);
	// Minecraft's clear-all: every section goes.
	void Clear();

	const Section* Find(const SectionPos& aSection) const;  // null: no solid blocks there
	size_t         Sections() const { return sections_.size(); }
	uint64_t       Blocks() const { return blocks_; }  // the sections' solid blocks together

private:
	std::map<SectionPos, Section> sections_;
	uint64_t                      blocks_ = 0;
	uint64_t                      version_ = 0;
};
}
```
- [x] **Step 6: Write `src/link/Solids.cpp`**

```cpp
#include "link/Solids.h"

#include "link/Messages.h"

#include <skycraft_protocol.h>

#include <bit>
#include <cstdlib>
#include <cstring>
#include <format>

namespace cpmc::link
{
namespace proto = skycraft::proto;
static_assert(sizeof(proto::RenSolids) == 16 && sizeof(SolidBits) == 512);

std::optional<SolidsMessage> ParseSolids(std::span<const uint8_t> aPayload, std::string& aError)
{
	proto::RenSolids header;
	if (aPayload.size() < sizeof header)
	{
		aError = std::format("a solids message of {} bytes has no room for its header", aPayload.size());
		return std::nullopt;
	}
	std::memcpy(&header, aPayload.data(), sizeof header);
	if (std::abs(int64_t(header.sx)) > kMaxSectionXZ || std::abs(int64_t(header.sz)) > kMaxSectionXZ ||
	    std::abs(int64_t(header.sy)) > kMaxSectionY)
	{
		aError = std::format("solids for section {} {} {} are out of range", header.sx, header.sy, header.sz);
		return std::nullopt;
	}
	SolidsMessage solids{ { header.sx, header.sy, header.sz }, header.count };
	if (header.count == 0)
	{
		return solids;
	}
	if (header.count > 4096)
	{
		aError = std::format("solids for section {} {} {} claim {} blocks, more than a section holds", header.sx, header.sy, header.sz,
		                     header.count);
		return std::nullopt;
	}
	if (aPayload.size() - sizeof header < sizeof solids.bits)
	{
		aError = std::format("solids for section {} {} {} claim {} blocks, with no room for their bits", header.sx, header.sy,
		                     header.sz, header.count);
		return std::nullopt;
	}
	std::memcpy(solids.bits.data(), aPayload.data() + sizeof header, sizeof solids.bits);
	uint32_t set = 0;
	for (const uint64_t word : solids.bits)
	{
		set += uint32_t(std::popcount(word));
	}
	if (set != header.count)
	{
		aError = std::format("solids for section {} {} {} claim {} blocks, and their bits hold {}", header.sx, header.sy, header.sz,
		                     header.count, set);
		return std::nullopt;
	}
	return solids;
}

void Solids::Set(const SolidsMessage& aMessage)
{
	if (const auto old = sections_.find(aMessage.section); old != sections_.end())
	{
		blocks_ -= old->second.count;
		sections_.erase(old);
	}
	if (aMessage.count == 0)
	{
		return;
	}
	sections_[aMessage.section] = { aMessage.bits, aMessage.count, ++version_ };
	blocks_ += aMessage.count;
}

void Solids::Clear()
{
	sections_.clear();
	blocks_ = 0;
}

const Solids::Section* Solids::Find(const SectionPos& aSection) const
{
	const auto found = sections_.find(aSection);
	return found == sections_.end() ? nullptr : &found->second;
}
}
```
- [x] **Step 7: Edit `src/link/Link.h`**

```cpp
#include "link/Rings.h"
```
```cpp
#include "link/Rings.h"
#include "link/Solids.h"
```
```cpp
// adds build mode's drive (its spec's "Part 1"): one teleport, then Minecraft drives and the link says where V goes. And
// Night City's ground (its "Part 2"): the city's collision instead of the floor, read around the centre and sent as it
// changes. It works on the mapping's memory, so the tests give it plain memory with a fake Minecraft.
```
```cpp
// adds build mode's drive (its spec's "Part 1"): one teleport, then Minecraft drives and the link says where V goes;
// Night City's ground (its "Part 2"): the city's collision instead of the floor, read around the centre and sent as it
// changes; and Minecraft's solid blocks by section (its "Part 3"), for V's bodies. It works on the mapping's memory, so
// the tests give it plain memory with a fake Minecraft.
```
```cpp
		uint64_t                     translucentDropped = 0;
```
```cpp
		uint64_t                     translucentDropped = 0;
		size_t                       solidSections = 0;  // Minecraft's sections with solid blocks, kept (spike 5's "Part 3")
		uint64_t                     solidBlocks = 0;    // and their solid blocks
```
```cpp
	const Status& State() const { return status_; }
```
```cpp
	const Status& State() const { return status_; }
	const Solids& SolidSections() const { return solids_; }  // Minecraft's solid blocks, by section
```
```cpp
	Status                status_;
```
```cpp
	Status                status_;
	Solids                solids_;
```

- [x] **Step 8: Edit `src/link/Link.cpp`**

```cpp
	status_.drainedBytes += drained.bytes;
```
```cpp
	status_.drainedBytes += drained.bytes;
	status_.solidSections = solids_.Sections();
	status_.solidBlocks = solids_.Blocks();
```
```cpp
		status_.atlasSeen = false;  // Minecraft sends its atlas next
		aSink.ClearAll();
		return true;
	default:
		return false;  // atlas patches, lights, solids, digging, textures, avatar, scene, ragdoll: stepped over
```
```cpp
		status_.atlasSeen = false;  // Minecraft sends its atlas next
		solids_.Clear();
		aSink.ClearAll();
		return true;
	case proto::kRenSolids:
		if (const auto solids = ParseSolids(aMessage.payload, error))
		{
			solids_.Set(*solids);
		}
		else
		{
			reject();
		}
		return true;
	default:
		return false;  // atlas patches, lights, digging, textures, avatar, scene, ragdoll: stepped over
```

- [x] **Step 9: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/GroundWorker.cpp
	src/physx/GameAbi.cpp
```
```cmake
	src/link/GroundWorker.cpp
	src/link/Solids.cpp
	src/physx/GameAbi.cpp
```

- [x] **Step 10: Edit `tools/mclink/Main.cpp`**

```cpp
// It reports what Minecraft sends, and refuses to start while another game side uses the mapping.
```
```cpp
// It reports what Minecraft sends, its solid blocks among it, and refuses to start while another game side uses the
// mapping.
```
```cpp
void PrintStatus(const Link::Status& aStatus, const Report& aReport)
{
	const auto& s = aStatus;
```
```cpp
// Minecraft's solid blocks: all of them, and in the 3 x 3 x 3 sections around the stand-in V's, where V's bodies would go.
void PrintSolids(const Solids& aSolids, const Vec3d& aFeet)
{
	const SectionPos at = SectionOf({ int32_t(std::floor(aFeet.x)), int32_t(std::floor(aFeet.y)), int32_t(std::floor(aFeet.z)) });
	size_t           sections = 0;
	uint64_t         blocks = 0;
	for (int32_t dy = -1; dy <= 1; ++dy)
	{
		for (int32_t dz = -1; dz <= 1; ++dz)
		{
			for (int32_t dx = -1; dx <= 1; ++dx)
			{
				if (const Solids::Section* section = aSolids.Find({ at.x + dx, at.y + dy, at.z + dz }))
				{
					++sections;
					blocks += section->count;
				}
			}
		}
	}
	Say("solids: {} sections with {} solid blocks; around the stand-in V's section {} {} {}: {} sections with {} blocks", aSolids.Sections(),
	    aSolids.Blocks(), at.x, at.y, at.z, sections, blocks);
}

void PrintStatus(const Link& aLink, const Report& aReport, const Vec3d& aFeet)
{
	const auto& s = aLink.State();
```
```cpp
	    double(s.collisionBytes) / 1048576.0, s.collisionWaiting, s.collisionDropped);
```
```cpp
	    double(s.collisionBytes) / 1048576.0, s.collisionWaiting, s.collisionDropped);
	PrintSolids(aLink.SolidSections(), aFeet);
```
```cpp
			case Action::Kind::Status: PrintStatus(state, report); break;
```
```cpp
			case Action::Kind::Status: PrintStatus(link, report, feet); break;
```
```cpp
	PrintStatus(link.State(), report);
```
```cpp
	PrintStatus(link, report, feet);
```

- [x] **Step 11: Write `tools/mclink/solids.txt`**

```text
# Spike 5's Part 3, offline: Minecraft's solid blocks, as SkyCraft sends them after each section's mesh (kRenSolids),
# kept by the link's own code, against the real Minecraft (docs/superpowers/specs/2026-10-03-collision-and-movement-
# design.md, "Solids"). Start Minecraft first: this waits for its world. The blocks go in section 17 0 125, east of
# spike 3's corner and clear of the blocks built there and of city.txt's dumps; the stand-in V stands in it.
status
wait-world 180
at 280.5 6.14 2007.5
link on
type /gamemode creative
wait 3
status
# A 3 x 2 x 4 box of stone, from block 280 7 2004: 24 solid blocks.
type /fill 280 7 2004 282 8 2007 minecraft:stone
wait 2
status
# Glass and a slab count too, having collision shapes; a torch, with none, doesn't.
type /setblock 284 7 2004 minecraft:glass
type /setblock 285 7 2004 minecraft:oak_slab
type /setblock 286 7 2004 minecraft:torch
wait 2
status
# All broken again: the section has no solid blocks left, and goes.
type /fill 280 7 2004 286 8 2007 minecraft:air
wait 2
status
quit
```
- [x] **Step 12: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `376 test(s), 0 failure(s)`.

- [x] **Step 13: Run mclink without Minecraft**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && printf 'status\nquit\n' > build/selfcheck.txt \
  && ./build/RelWithDebInfo/mclink.exe build/selfcheck.txt | head -2; echo "exit: ${PIPESTATUS[0]}"
```
Expected: `the mapping: created (process …); waiting for Minecraft`, then `Minecraft: absent, …`, and `exit: 0`. The game
and Minecraft must both be closed: either would hold the mapping, and Minecraft would link.

- [x] **Step 14: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the link keeps Minecraft's solid blocks by section" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 43: In Minecraft: the worker and the solids, offline

With the user, without the game: the new worker against SkyCraft's unchanged mod, through Task 28's `city.txt` again,
and Minecraft's solid blocks through `solids.txt`. The city's checks are Task 28's, whose numbers came from spot B's and
the ramp's own triangles. `solids.txt` stands the stand-in V in section 17 0 125, east of spike 3's corner and clear of
the blocks built there, and places, then breaks, blocks in it with Minecraft's commands. If a check fails, stop and find
the cause with superpowers:systematic-debugging before going on. Fixes become new tasks.

**Files:**
- Modify: `docs/spikes/2026-10-03-collision-and-movement-results.md`

- [x] **Step 1: Check that the game isn't running**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game: it would hold the mapping.

- [x] **Step 2: Ask the user to start Minecraft**

The user starts the `CyberCraft` instance from the Modrinth App, and says when its title screen shows. Then:
```bash
grep -E "SkyCraft:" "/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log" | tail -3
```
Expected: `SkyCraft: can't open Skyrim's shared memory yet (Windows error 2: …)`: waiting for a game side.

- [x] **Step 3: Run step 3's offline script with the new worker**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/part3 && cd build/part3 \
  && ../RelWithDebInfo/mclink.exe ../../tools/mclink/city.txt | tee city.log; echo "exit: ${PIPESTATUS[0]}"
```
Give it 6 minutes, as Task 28. Expected, as Task 28 found (`docs/spikes/2026-10-03-collision-and-movement-results.md`,
"Step 3, offline"):
- `the city around region 10 0 250 is all in the ring: 1152 regions` a few seconds after the link, then Minecraft lets
  its player go within a second;
- the kerb's drop of about 0.2 about 2 blocks west, the barrier stopping the walk about 4 blocks east, and `explosion at
  81.6 6.3 2007.8, radius 4.0`, or within a quarter block of it;
- the ramp: `the city around region 22 0 250 is all in the ring: 1152 regions`, the feet rising about 0.33 over 17
  blocks north, and x staying at about 180.6 going east;
- no `build mode ends:` line, and `exit: 0`.

New with this worker, in the status's `ground:` lines:
- `the worker: N threads`, N a quarter of the processors, from 1 to 4;
- `its threads running …% of it` near 100: Minecraft takes little of the processor, so the threads aren't kept waiting;
- about 24,000 voxel blocks from about 108,000 triangles for spot B's area, as Task 28's; and `0 replaced`, since the
  script reads each batch once while it stands still.

Note how soon after the dump `is all in the ring` came, against Task 28's 2.5 s with one thread.

- [x] **Step 4: Run the solids' script**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render/build/part3 \
  && ../RelWithDebInfo/mclink.exe ../../tools/mclink/solids.txt | tee solids.log | grep -E "solids:|render ring: [0-9]+ resyncs"; \
  echo "exit: ${PIPESTATUS[0]}"
```
Expected, in the `solids:` lines, the 3 × 3 × 3 sections around the stand-in V's section 17 0 125:
- before any block is placed: `0 sections with 0 blocks`;
- after the stone box: `1 sections with 24 blocks`;
- after the glass, the slab and the torch: `1 sections with 26 blocks`;
- after they're all broken: `0 sections with 0 blocks`;
- `0 rejected` in every `render ring:` line, and `exit: 0`.

The total in each `solids:` line is every section Minecraft has sent with solid blocks, its world's own ground among
them: note it.

- [x] **Step 5: Ask the user to stop Minecraft**

Its window is hidden while it's linked, so the user stops it from the Modrinth App.

- [x] **Step 6: Record it in `docs/spikes/2026-10-03-collision-and-movement-results.md`**

A "Part 3, offline: the worker and the solids" section: the versions; the city's checks with their numbers against Task
28's; the worker's threads, running share and time for spot B's area, and how soon the area was all in the ring; and
the solids' counts at each stage, and the totals.

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-03-collision-and-movement-results.md \
  && git commit -q -m "collision-and-movement: Part 3 offline, the worker and the solids" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 44: BlockRender 0.14.0 remembers the turn, and keeps Minecraft's solid blocks

The first of Part 3's two deploys: the follow-ups, the query watch, and Minecraft's solid blocks kept, with no bodies
yet. The status gains a `solids=` line, the solid blocks the link keeps and their sections, between the ground's line and
the render ring's. Version 0.14.0.

**Files:**
- Modify: `src/Status.h`, `src/Status.cpp`, `src/Plugin.cpp`, `tests/TestStatus.cpp`, `CMakeLists.txt`

- [x] **Step 1: Edit `tests/TestStatus.cpp`: the failing test**

```cpp
TEST(StatusWritesTheSceneWatch)
```
```cpp
TEST(StatusWritesMinecraftsSolids)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	link.solidSections = 625;
	link.solidBlocks = 640000;
	report.link = link;
	const std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "solids=Minecraft's solid blocks: 640000 in 625 sections"));
	CHECK(status.find("ground=") < status.find("solids=") && status.find("solids=") < status.find("ring="));
}

TEST(StatusWritesTheSceneWatch)
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'solidSections': is not a member of 'cpmc::LinkReport'`.

- [x] **Step 3: Edit `src/Status.h`**

```cpp
	std::string                    physxWhy;                  // why the exporter couldn't read, while it can't
```
```cpp
	std::string                    physxWhy;                  // why the exporter couldn't read, while it can't
	// Minecraft's solid blocks (spike 5's "Part 3")
	size_t   solidSections = 0;
	uint64_t solidBlocks = 0;
```
```cpp
// drive, positions, ring, atlas, hud, latency, burst and processes); then one structure line per structure:
```
```cpp
// drive, positions, ground, solids, ring, atlas, hud, latency, burst and processes); then one structure line per structure:
```

- [x] **Step 4: Edit `src/Status.cpp`**

```cpp
	out += FormatGround(aLink);
```
```cpp
	out += FormatGround(aLink);
	out += std::format("solids=Minecraft's solid blocks: {} in {} sections\n", aLink.solidBlocks, aLink.solidSections);
```

- [x] **Step 5: Edit `src/Plugin.cpp`**

```cpp
// build mode: the movement keys are Minecraft's too, and V is teleported to its player's eye every frame; and it reads
// Night City's shape for Minecraft from the game's PhysX.
```
```cpp
// build mode: the movement keys are Minecraft's too, and V is teleported to its player's eye every frame; it reads Night
// City's shape for Minecraft from the game's PhysX; and it keeps Minecraft's solid blocks, for V to stand on.
```
```cpp
constexpr const char* kVersion = "0.13.0";
```
```cpp
constexpr const char* kVersion = "0.14.0";
```
```cpp
		report.lastHold = state.lastHold;
```
```cpp
		report.lastHold = state.lastHold;
		report.solidSections = state.solidSections;
		report.solidBlocks = state.solidBlocks;
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 13, 0);
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 14, 0);
```

- [x] **Step 6: Edit `CMakeLists.txt`: version 0.14.0**

```cmake
project(BlockRender VERSION 0.13.0 LANGUAGES C CXX)
```
```cmake
project(BlockRender VERSION 0.14.0 LANGUAGES C CXX)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `377 test(s), 0 failure(s)`.

- [x] **Step 8: Check that BlockRender imports nothing from PhysX**

```bash
DUMPBIN="$(ls "/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/"*/bin/Hostx64/x64/dumpbin.exe | head -1)"
cd /d/Cyberpunk-Minecraft/spikes/block-render && MSYS_NO_PATHCONV=1 "$DUMPBIN" -imports -nologo build\\RelWithDebInfo\\BlockRender.dll \
  | grep -ci "physx"
```
Expected: `0`.

- [x] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: BlockRender 0.14.0 remembers the turn, and keeps Minecraft's solid blocks" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 45: The PhysX audit's addendum: the bodies' and the queries' slots

The spec's "The audit", before step 4: each PhysX call Part 3 adds, found in the game's `PhysX3_x64.dll` (the audited
build) by its strings and its code, offline, with Python and Capstone, as spike 1's audit and Task 32's addendum were
done. Besides the calls the spec lists, `PxShape::setLocalPose` places each box on its body, and the scene's three
queries are the scene watch's (Task 41). Our side is a test: `BodyCallsUseTheGamesSlots` measures the slot each of the
bodies' calls goes through, as `TestAudit` measures the others. It passes as soon as it's added, since our headers
already have the game's layout; it's there to keep it so.

**Files:**
- Modify: `tests/TestAudit.cpp`, `docs/spikes/2026-10-01-physx-abi-audit.md`

- [x] **Step 1: Edit `tests/TestAudit.cpp`: the bodies' slots**

```cpp
TEST(ActorCallsUseTheGamesSlots)
```
```cpp
TEST(BodyCallsUseTheGamesSlots)
{
	// Part 3's bodies (physx/Bodies.cpp) are made, shaped, posed and released through these slots, as the audit's second
	// addendum found them in the game's PhysX3_x64.dll. Each PxShape slot is one past NVIDIA's header's (see PxShape.h).
	Fake                dummy;  // a shape: never touched
	PxShape&            shape = reinterpret_cast<PxShape&>(dummy);
	PxMaterial*         material = nullptr;
	const PxBoxGeometry box(0.5f, 0.5f, 0.5f);
	const PxTransform   pose(PxIdentity);
	const PxFilterData  filter;
	Expect("PxPhysics::createRigidStatic", SlotOf<PxPhysics>([&](PxPhysics& a) { (void)a.createRigidStatic(pose); }), 21);
	Expect("PxPhysics::createShape",
	       SlotOf<PxPhysics>([&](PxPhysics& a) { (void)a.createShape(box, &material, 1, true, PxShapeFlag::eSCENE_QUERY_SHAPE); }), 27);
	Expect("PxActor::release", SlotOf<PxActor>([](PxActor& a) { a.release(); }), 0);
	Expect("PxActor::getScene", SlotOf<PxActor>([](PxActor& a) { (void)a.getScene(); }), 6);
	Expect("PxRigidActor::setGlobalPose", SlotOf<PxRigidActor>([&](PxRigidActor& a) { a.setGlobalPose(pose); }), 21);
	Expect("PxRigidActor::attachShape", SlotOf<PxRigidActor>([&](PxRigidActor& a) { a.attachShape(shape); }), 23);
	Expect("PxShape::release", SlotOf<PxShape>([](PxShape& a) { a.release(); }), 0);
	Expect("PxShape::setLocalPose", SlotOf<PxShape>([&](PxShape& a) { a.setLocalPose(pose); }), 18);
	Expect("PxShape::setSimulationFilterData", SlotOf<PxShape>([&](PxShape& a) { a.setSimulationFilterData(filter); }), 20);
	Expect("PxShape::setQueryFilterData", SlotOf<PxShape>([&](PxShape& a) { a.setQueryFilterData(filter); }), 22);
	Expect("PxShape::getMaterials", SlotOf<PxShape>([&](PxShape& a) { (void)a.getMaterials(&material, 1); }), 26);
}

TEST(ActorCallsUseTheGamesSlots)
```

- [x] **Step 2: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `378 test(s), 0 failure(s)`.

- [x] **Step 3: Edit `docs/spikes/2026-10-01-physx-abi-audit.md`: the addendum, at its end**

```markdown
`PxBase::release` for actors and shapes.
```
```markdown
`PxBase::release` for actors and shapes.

## Addendum: spike 5's Part 3, the bodies and the query watch

Date: 2026-10-04. The same DLLs and the same method, starting from the vtables in the table above, and read with Python
and Capstone. Part 3 makes static actors for Minecraft's blocks (`spikes/block-render/src/physx/Bodies.cpp`), and its
scene watch also hooks the scene's three queries (`SceneHooks.cpp`). Our side is `TestAudit`'s `BodyCallsUseTheGamesSlots`
and `SceneWatchHooksTheGamesSlots`.

| Table | Slot | Call | Function's RVA | How it was told |
|---|---|---|---|---|
| `NpPhysics` | 21 | `createRigidStatic(const PxTransform&)` | 0x48090 | calls PhysX's factory (0x32850), whose constructor (0x505d0) writes `NpRigidStatic`'s vtable; slot 22, `createRigidDynamic`, does the same with `NpRigidDynamic`'s |
| `NpPhysics` | 27 | `createShape(const PxGeometry&, PxMaterial* const*, PxU16, bool, PxShapeFlags)` | 0x48270 | calls the factory's 0x32a40, whose constructor (0x65fb0) writes `NpShape`'s vtable |
| `NpRigidStatic` | 0 | `release()` | 0x501b0 | calls the rigid actor's release (0x51130): "PxRigidActor::release: Actor is part of a pruning structure …" |
| `NpRigidStatic` | 6 | `getScene()` | 0x50f00 | jumps to 0x22200, which `NpRigidDynamic`'s slot 6 shares: the actor's scene while it's being added or is in one, else null |
| `NpRigidStatic` | 21 | `setGlobalPose(const PxTransform&, bool)` | 0x50300 | "PxRigidStatic::setGlobalPose: Actor is part of a pruning structure …" |
| `NpRigidStatic` | 23 | `attachShape(PxShape&)` | 0x50b90 | "PxRigidActor::attachShape: Actor is part of a pruning structure …"; slot 24 has `detachShape`'s strings |
| `NpShape` | 0 | `release()` | 0x661e0 | takes one from the reference count at +0x20, which slot 6, the game's extra getter, reads, and calls the count's own handler when it reaches 0 |
| `NpShape` | 18 | `setLocalPose(const PxTransform&)` | 0x66700 | "PxShape::setLocalPose: Shape is a part of pruning structure …" |
| `NpShape` | 20 | `setSimulationFilterData(const PxFilterData&)` | 0x66960 | writes 16 bytes at +0x60, which slot 21 (`getSimulationFilterData`, audited) reads, or buffers them while the scene simulates |
| `NpShape` | 22 | `setQueryFilterData(const PxFilterData&)` | 0x66ac0 | writes 16 bytes at +0x50, which slot 23 (`getQueryFilterData`, audited) reads |
| `NpShape` | 26 | `getMaterials(PxMaterial**, PxU32, PxU32)` | 0x66d60 | jumps to 0x67d30, which writes the pointer PhysX's material table holds for each of the shape's material indices; slot 24, `setMaterials`, calls it too, and slot 27 is `getMaterialFromInternalFaceIndex` ("PxShape::getMaterialFromInternalFaceIndex received 0xFFFFffff …") |
| `NpScene` | 94 | `raycast(…)` | 0x60890 | takes its distance in XMM3 and its hit flags by address; the probe's raycast |
| `NpScene` | 95 | `sweep(…)` | 0x60960 | " Precise sweep doesn't support MTD …" |
| `NpScene` | 96 | `overlap(…)` | 0x60b70 | takes no float; the probe's and the exporter's overlap |

What that settles:
- **Every slot is where NVIDIA's header puts it,** `PxShape`'s one later, as our compiler lays them out with the repo's
  `PxShape.h`.
- **A shape keeps its materials:** the factory's `createShape` takes a reference on each material it's given (the loop at
  0x32c20 adds one at the material's +0x20, from the same table `getMaterials` reads). So a shape BlockRender keeps holds
  on to the material it copied, even after the game lets go of the shape it copied it from.
- **An actor's release takes it out of its scene,** as NVIDIA's `NpRigidActorTemplate::release` does. BlockRender still
  takes a body out first, with `removeActor` (slot 13, audited for the watch), then releases it.
- **The hit flags go by address:** `PxHitFlags` has a copy constructor of its own, so MSVC passes it as a pointer to a
  copy, in the game as in our hooks, which declare it the same way.
```

- [x] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render docs/spikes/2026-10-01-physx-abi-audit.md \
  && git commit -q -m "collision-and-movement: the PhysX audit's addendum, the bodies' and the queries' slots" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 46: In the game: the follow-ups, the query watch and the solids

The first half of step 4 in the game, with the user, on BlockRender 0.14.0, which makes no bodies yet:
- **Part 2's follow-ups:** the turn, remembered across a game start; and a fast drop onto the ground, with the worker's
  figures saying why a region takes as long as it does.
- **The query watch:** walking, driving and shooting with the watch on. Bodies are added and removed in OnUpdate, so no
  other thread may call scene 0 meanwhile, queries included. That settles where Tasks 47 to 52 add them.
- **The solids:** the status's `solids=` line as blocks are placed and broken.

Task 33's commands for requests, the log and the status come back here. FreeFly (F7) stays off. Note what you see as you
go: Step 10 records it. If a check fails, stop and find the cause with superpowers:systematic-debugging before going on.
Fixes become new tasks.

**Files:**
- Modify: `docs/spikes/2026-10-03-collision-and-movement-results.md`

- [x] **Step 1: Deploy (the game must not be running)**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game first, because a running game locks the DLL.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default -DCP2077_DIR=E:/Cyberpunk2077 > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C|error MSB"; \
  "$CMAKE" --preset default -DCP2077_DIR= > /dev/null; \
  cmp build/RelWithDebInfo/BlockRender.dll /e/Cyberpunk2077/red4ext/plugins/BlockRender/BlockRender.dll && echo installed
```
Expected: `installed`. The second configure clears `CP2077_DIR` again.

- [x] **Step 2: In the game: load, link, and the turn measured**

Ask the user to start the `CyberCraft` instance, then the game, load the free-roam save at spike 3's corner, and click
the overlay's Link button. Expected, in the log: `BlockRender 0.14.0 loaded`, `the turn: nothing remembered, so build
mode turns once V has turned 5 times`, and `link: Minecraft is there (process …)`.

Ask the user to turn V left and right a few times, pausing between turns. Expected: the drive line's `turn on, -0.0…
degrees a count from … turns`, and in the log `the turn: -0.0… degrees a count, measured, and saved in turn.txt for the
next session`. Then:
```bash
cat /e/Cyberpunk2077/red4ext/plugins/BlockRender/turn.txt
```
Expected: the file's two comment lines, then `ratio = -0.0…`, the drive line's ratio.

- [x] **Step 3: In the game: the turn after a game start**

Ask the user to quit the game, leaving Minecraft running, start the game again, load the same save, click Link, and
press F8 at once, before turning V; then to turn the mouse in build mode. Expected: in the log, `the turn: -0.0…
degrees a count, remembered from the last session, until this one has measured its own`; in the drive line, `turn on,
-0.0… degrees a count, remembered from the last session (0 of 5 turns measured)`; and V turning left and right with
the mouse at once.

- [x] **Step 4: In the game: the drop**

Record the ground line twice a second for three minutes, in the background (the Bash tool's `run_in_background`):
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/part3 \
  && uv run --no-project python tools/status_watch.py 180 0.5 "ground|positions|frame" > build/part3/drop.txt
```
Ask the user, in build mode, to fly (Space twice), rise about 150 blocks (the overlay's `V:` z), sprint-fly a few
hundred metres over the city, then stop flying (Space twice) above grass or a street, and drop. Expected: the player
lands on the ground, not through it. Then again over a street, if time allows. From the recording, during each drop:
- `regions waiting` climbs and falls back to 0 within a few seconds;
- `the worker … ms a region of … triangles, its threads running …% of that time; N threads`. Near 100%, a region costs
  what its triangles cost, against Task 43's figures for spot B; well below, the game's threads kept the worker's
  threads waiting. That answers Part 2's question of why a region cost 5 to 20 times more in the game.

- [x] **Step 5: In the game: the query watch**

Ask the user to go on foot to an open street. Then send `scenewatch on`. Expected: `request: scenewatch on: 19 of scene
0's calls hooked; from a second on, the status sums them by thread`. If it says a slot doesn't hold the audited
function, another mod has hooked it: stop, and record which.

Ask the user to walk for half a minute, get into a car and drive for half a minute, get out, draw a gun and fire a few
shots at a wall, and stand still for ten seconds. Then send `scenewatch off`. Expected: `request: scenewatch off: …`,
and the log's `scene watch, … s: …` line, which the status keeps. Read from it:
- which threads make queries, and how many;
- the main thread's queries in OnUpdate: BlockRender's own, the exporter's overlaps;
- `no other thread called while OnUpdate ran`, or `other threads made N calls while OnUpdate ran`.

- [x] **Step 6: Decide where the bodies are added**

The bodies are added, removed and posed in OnUpdate (Tasks 50 and 51). That's safe if no other thread called scene 0
while OnUpdate ran: neither the writes Task 33 watched, nor the queries, which a change to the scene could break.
- **If so,** go on with Task 47.
- **If not,** stop after Step 11: the bodies have to be added where the watch shows the scene is quiet, and Tasks 47 to
  52 are planned again first, as a change to this plan.

PhysX's batched queries (`PxBatchQuery`) don't go through the scene's vtable, so the watch can't see them; step 4's box
is the check of what's left.

- [x] **Step 7: In the game: the solids**

Ask the user, in build mode, to place a few blocks one at a time, a second apart, then break them, and leave build
mode. Expected: the status's `solids=Minecraft's solid blocks: … in … sections` up by one within a second of each block
placed, and down by one with each broken; the ring line's `0 rejected`.

- [x] **Step 8: Ask the user to quit the game and stop Minecraft**

- [x] **Step 9: Check the game's crash reports**

```bash
ls -t "$LOCALAPPDATA/REDEngine/ReportQueue" 2>/dev/null | head -3
```
Expected: nothing newer than this session's start. A newer folder means the game faulted: read it before going on.

- [x] **Step 10: Record it in `docs/spikes/2026-10-03-collision-and-movement-results.md`**

A "Part 3, in the game: the follow-ups and the query watch" section: the turn across a game start; each drop, whether
the player landed, and the worker's figures during it, with what they say about why a region costs what it does; the
watch's summary, thread by thread, with the queries in OnUpdate; the decision, and why; and the solids' counts.

- [x] **Step 11: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-03-collision-and-movement-results.md \
  && git commit -q -m "collision-and-movement: Part 3's follow-ups and the query watch in the game" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 47: A section's solid blocks merged into few boxes

The spec's "Bodies", its shape: a body's blocks merged into as few boxes as the greedy way finds, along x, then z, then
y, so a 16 × 16 layer is one box and a full section one. From each solid block not yet in a box, in order of y, z and x,
`MergeBoxes` goes as far along x as the blocks go, then takes as many such rows along z, then as many such layers up,
and marks them taken. Every solid block ends up in exactly one box, and no box holds a block that isn't solid, which
the tests check on a checkerboard (2,048 boxes, each one block) and on random sections from sparse to dense.

**Files:**
- Create: `src/physx/BoxMerge.h`, `src/physx/BoxMerge.cpp`, `tests/TestBoxMerge.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestBoxMerge.cpp`**

```cpp
#include "Harness.h"

#include "physx/BoxMerge.h"

#include <array>

using namespace cpmc;
using namespace cpmc::px;

namespace
{
void Set(link::SolidBits& aBits, int aX, int aY, int aZ)
{
	const int bit = aX + 16 * aZ + 256 * aY;
	aBits[size_t(bit >> 6)] |= uint64_t(1) << (bit & 63);
}

bool Solid(const link::SolidBits& aBits, int aX, int aY, int aZ)
{
	const int bit = aX + 16 * aZ + 256 * aY;
	return (aBits[size_t(bit >> 6)] >> (bit & 63)) & 1;
}

// Whether aBoxes cover exactly aBits' solid blocks, each once.
bool CoverExactly(const std::vector<BlockBox>& aBoxes, const link::SolidBits& aBits)
{
	std::array<int, 4096> covered{};
	for (const BlockBox& box : aBoxes)
	{
		if (box.x0 >= box.x1 || box.y0 >= box.y1 || box.z0 >= box.z1 || box.x1 > 16 || box.y1 > 16 || box.z1 > 16)
		{
			return false;
		}
		for (int y = box.y0; y < box.y1; ++y)
		{
			for (int z = box.z0; z < box.z1; ++z)
			{
				for (int x = box.x0; x < box.x1; ++x)
				{
					++covered[size_t(x + 16 * z + 256 * y)];
				}
			}
		}
	}
	for (int y = 0; y < 16; ++y)
	{
		for (int z = 0; z < 16; ++z)
		{
			for (int x = 0; x < 16; ++x)
			{
				if (covered[size_t(x + 16 * z + 256 * y)] != (Solid(aBits, x, y, z) ? 1 : 0))
				{
					return false;
				}
			}
		}
	}
	return true;
}
}

TEST(BoxMergeMakesWholeLayersAndSectionsOneBox)
{
	link::SolidBits bits{};
	CHECK(MergeBoxes(bits).empty());
	for (int z = 0; z < 16; ++z)
	{
		for (int x = 0; x < 16; ++x)
		{
			Set(bits, x, 3, z);
		}
	}
	std::vector<BlockBox> boxes = MergeBoxes(bits);
	CHECK_EQ(boxes.size(), size_t(1));  // a 16 x 16 layer
	CHECK(boxes.size() == 1 && boxes[0].x0 == 0 && boxes[0].x1 == 16 && boxes[0].y0 == 3 && boxes[0].y1 == 4 && boxes[0].z0 == 0 &&
	      boxes[0].z1 == 16);
	bits.fill(~uint64_t(0));
	boxes = MergeBoxes(bits);
	CHECK(boxes.size() == 1 && boxes[0].y0 == 0 && boxes[0].y1 == 16);  // the whole section
}

TEST(BoxMergeGoesAlongXThenZThenY)
{
	// A wall 4 blocks along x, 3 high, and a tower 2 x 2 x 5 beside it: two boxes.
	link::SolidBits bits{};
	for (int y = 0; y < 3; ++y)
	{
		for (int x = 0; x < 4; ++x)
		{
			Set(bits, x, y, 0);
		}
	}
	for (int y = 0; y < 5; ++y)
	{
		for (int z = 3; z < 5; ++z)
		{
			for (int x = 7; x < 9; ++x)
			{
				Set(bits, x, y, z);
			}
		}
	}
	const std::vector<BlockBox> boxes = MergeBoxes(bits);
	CHECK_EQ(boxes.size(), size_t(2));
	CHECK(CoverExactly(boxes, bits));
	// An L along x then z: the first row goes as far along x as it can, so the L is two boxes.
	link::SolidBits l{};
	for (int x = 0; x < 4; ++x)
	{
		Set(l, x, 0, 0);
	}
	Set(l, 0, 0, 1);
	Set(l, 0, 0, 2);
	const std::vector<BlockBox> lBoxes = MergeBoxes(l);
	CHECK_EQ(lBoxes.size(), size_t(2));
	CHECK(lBoxes.size() == 2 && lBoxes[0].x1 == 4 && lBoxes[0].z1 == 1 && lBoxes[1].z0 == 1 && lBoxes[1].z1 == 3);
}

TEST(BoxMergeCoversAnyBlocksExactly)
{
	// A checkerboard, where no two solid blocks touch: a box each.
	link::SolidBits checker{};
	for (int y = 0; y < 16; ++y)
	{
		for (int z = 0; z < 16; ++z)
		{
			for (int x = 0; x < 16; ++x)
			{
				if ((x + y + z) % 2 == 0)
				{
					Set(checker, x, y, z);
				}
			}
		}
	}
	const std::vector<BlockBox> boxes = MergeBoxes(checker);
	CHECK_EQ(boxes.size(), size_t(2048));
	CHECK(CoverExactly(boxes, checker));
	// Random blocks, from sparse to dense.
	uint64_t seed = 12345;
	for (int round = 0; round < 40; ++round)
	{
		link::SolidBits bits{};
		for (uint64_t& word : bits)
		{
			for (int i = 0; i < 64; ++i)
			{
				seed = seed * 6364136223846793005ull + 1442695040888963407ull;
				if (int((seed >> 33) % 40) < round)
				{
					word |= uint64_t(1) << i;
				}
			}
		}
		CHECK(CoverExactly(MergeBoxes(bits), bits));
	}
}
```
- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestExport.cpp
	tests/TestDrive.cpp
```
```cmake
	tests/TestExport.cpp
	tests/TestBoxMerge.cpp
	tests/TestDrive.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'physx/BoxMerge.h'`.

- [x] **Step 4: Write `src/physx/BoxMerge.h`**

```cpp
#pragma once

#include "link/Solids.h"

#include <cstdint>
#include <vector>

// A section's solid blocks as few boxes, for its body (spike 5's "Bodies"): greedy along x, then z, then y, so a 16 x 16
// layer is one box and a whole section one. Every solid block is in exactly one box, and no box holds a block that isn't
// solid. Pure.
namespace cpmc::px
{
// Blocks within a section: from (x0, y0, z0) up to, but not including, (x1, y1, z1).
struct BlockBox
{
	uint8_t x0 = 0, y0 = 0, z0 = 0;
	uint8_t x1 = 0, y1 = 0, z1 = 0;
};

std::vector<BlockBox> MergeBoxes(const link::SolidBits& aBits);
}
```
- [x] **Step 5: Write `src/physx/BoxMerge.cpp`**

```cpp
#include "physx/BoxMerge.h"

namespace cpmc::px
{
namespace
{
// The solid blocks not in a box yet.
class Left
{
public:
	explicit Left(const link::SolidBits& aBits) : bits_(aBits) {}

	bool Has(int aX, int aY, int aZ) const { return (bits_[size_t(Bit(aX, aY, aZ) >> 6)] >> (Bit(aX, aY, aZ) & 63)) & 1; }
	void Take(int aX, int aY, int aZ) { bits_[size_t(Bit(aX, aY, aZ) >> 6)] &= ~(uint64_t(1) << (Bit(aX, aY, aZ) & 63)); }

	// Whether blocks aX0 to aX1, not included, of row (aY, aZ) are all left.
	bool Row(int aX0, int aX1, int aY, int aZ) const
	{
		for (int x = aX0; x < aX1; ++x)
		{
			if (!Has(x, aY, aZ))
			{
				return false;
			}
		}
		return true;
	}

	// And of rows aZ0 to aZ1 of layer aY.
	bool Layer(int aX0, int aX1, int aZ0, int aZ1, int aY) const
	{
		for (int z = aZ0; z < aZ1; ++z)
		{
			if (!Row(aX0, aX1, aY, z))
			{
				return false;
			}
		}
		return true;
	}

private:
	static int Bit(int aX, int aY, int aZ) { return aX + 16 * aZ + 256 * aY; }

	link::SolidBits bits_;
};
}

std::vector<BlockBox> MergeBoxes(const link::SolidBits& aBits)
{
	Left                  left(aBits);
	std::vector<BlockBox> boxes;
	for (int y = 0; y < 16; ++y)
	{
		for (int z = 0; z < 16; ++z)
		{
			for (int x = 0; x < 16; ++x)
			{
				if (!left.Has(x, y, z))
				{
					continue;
				}
				// As far along x as the blocks go, then as many such rows along z, then as many such layers up.
				int x1 = x + 1;
				while (x1 < 16 && left.Has(x1, y, z))
				{
					++x1;
				}
				int z1 = z + 1;
				while (z1 < 16 && left.Row(x, x1, y, z1))
				{
					++z1;
				}
				int y1 = y + 1;
				while (y1 < 16 && left.Layer(x, x1, z, z1, y1))
				{
					++y1;
				}
				for (int by = y; by < y1; ++by)
				{
					for (int bz = z; bz < z1; ++bz)
					{
						for (int bx = x; bx < x1; ++bx)
						{
							left.Take(bx, by, bz);
						}
					}
				}
				boxes.push_back({ uint8_t(x), uint8_t(y), uint8_t(z), uint8_t(x1), uint8_t(y1), uint8_t(z1) });
			}
		}
	}
	return boxes;
}
}
```
- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/physx/Export.cpp
	src/link/Drive.cpp
```
```cmake
	src/physx/Export.cpp
	src/physx/BoxMerge.cpp
	src/link/Drive.cpp
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `381 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: a section's solid blocks merged into few boxes" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 48: Which sections get bodies around V, and when they change

The spec's "Bodies", where and when, as pure bookkeeping, `BodyPlan`:
- **Where:** a body for each section with solid blocks within one section of V's (the 3 × 3 × 3 around it), made once
  the section comes that near, and removed once it's more than two away, so walking along a boundary doesn't churn
  bodies.
- **When it changes:** a body whose section's solids changed (their version moved on) is removed and made again in the
  same frame; one whose solids went is removed. A body PhysX failed to make isn't tried again until its section's
  solids change, the spec's error handling.
- **Off:** while bodies are off (`set blocks 0`, or drawing off), every body is removed and none is made.
- **What it changes:** `Changes` only says what to remove, then what to make; `physx/Bodies` does it and says what it
  did (`Made`, `Failed`, `Removed`), so a frame where PhysX can't be reached leaves the plan as it was.

And where a body stands: `SectionAt` (the section holding a point in Minecraft's axes), `SectionCentre` (a section's
centre in the world's axes, where its body's actor goes), `LocalBoxOf` (a box of its blocks in the world's axes, from
that centre: Minecraft's x is the world's x, its y the world's z, and its z the world's -y), and `PosedAt` (whether an
actor stands where it should, given PhysX's origin, within a centimetre), for checking bodies after the origin moves.

**Files:**
- Create: `src/physx/BodyPlan.h`, `src/physx/BodyPlan.cpp`, `tests/TestBodyPlan.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestBodyPlan.cpp`**

```cpp
#include "Harness.h"

#include "physx/BodyPlan.h"

#include <cmath>
#include <format>
#include <initializer_list>
#include <limits>
#include <string>

using namespace cpmc;
using namespace cpmc::px;

namespace
{
// Sets aSection's solids: aBlocks solid blocks, from its first.
void SetSolids(link::Solids& aSolids, const SectionPos& aSection, uint32_t aBlocks = 1)
{
	link::SolidsMessage message{ aSection, aBlocks, {} };
	for (uint32_t bit = 0; bit < aBlocks; ++bit)
	{
		message.bits[bit >> 6] |= uint64_t(1) << (bit & 63);
	}
	aSolids.Set(message);
}

link::Solids With(std::initializer_list<SectionPos> aSections)
{
	link::Solids solids;
	for (const SectionPos& section : aSections)
	{
		SetSolids(solids, section);
	}
	return solids;
}

// "make 0 0 0, remove 1 0 0": the changes, in order.
std::string Describe(const std::vector<BodyPlan::Change>& aChanges)
{
	std::string out;
	for (const BodyPlan::Change& change : aChanges)
	{
		out += std::format("{}{} {} {} {}", out.empty() ? "" : ", ", change.make ? "make" : "remove", change.section.x, change.section.y,
		                   change.section.z);
	}
	return out;
}

// What Bodies does when every change works: the plan's next state.
std::string Apply(BodyPlan& aPlan, const SectionPos& aV, const link::Solids& aSolids, bool aOn = true)
{
	const std::vector<BodyPlan::Change> changes = aPlan.Changes(aV, aOn, aSolids);
	for (const BodyPlan::Change& change : changes)
	{
		change.make ? aPlan.Made(change.section, change.version) : aPlan.Removed(change.section);
	}
	return Describe(changes);
}

bool Near(const Vec3d& aA, const Vec3d& aB)
{
	return std::abs(aA.x - aB.x) < 1e-9 && std::abs(aA.y - aB.y) < 1e-9 && std::abs(aA.z - aB.z) < 1e-9;
}
}

TEST(BodyPlanMakesTheSectionsAroundV)
{
	// Solids in five sections, V in section 0 0 0: bodies for the three within one section of it, not for those two away.
	const link::Solids solids = With({ { 0, 0, 0 }, { 1, 0, 0 }, { 2, 0, 0 }, { 0, -1, 1 }, { 0, 2, 0 } });
	BodyPlan           plan;
	const auto         changes = plan.Changes({ 0, 0, 0 }, true, solids);
	CHECK_STR(Describe(changes), "make 0 -1 1, make 0 0 0, make 1 0 0");
	CHECK(changes.size() == 3 && changes[1].version == solids.Find({ 0, 0, 0 })->version);
	CHECK_STR(Describe(plan.Changes({ 0, 0, 0 }, true, solids)), "make 0 -1 1, make 0 0 0, make 1 0 0");  // nothing done yet
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "make 0 -1 1, make 0 0 0, make 1 0 0");
	CHECK_STR(Describe(plan.Changes({ 0, 0, 0 }, true, solids)), "");  // all made
}

TEST(BodyPlanKeepsBodiesUntilTheyAreTwoSectionsAway)
{
	// Walking along a boundary doesn't churn bodies: one is made within a section of V's, and removed only past two.
	const link::Solids solids = With({ { 0, 0, 0 }, { 1, 0, 0 }, { 2, 0, 0 }, { 0, -1, 1 } });
	BodyPlan           plan;
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "make 0 -1 1, make 0 0 0, make 1 0 0");
	CHECK_STR(Apply(plan, { 1, 0, 0 }, solids), "make 2 0 0");
	CHECK_STR(Apply(plan, { 2, 0, 0 }, solids), "");  // 0 0 0 is two away: kept
	CHECK_STR(Apply(plan, { 3, 0, 0 }, solids), "remove 0 -1 1, remove 0 0 0");
	CHECK_STR(Apply(plan, { 2, 0, 0 }, solids), "");  // 0 0 0 is two away again: not made
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "make 0 -1 1, make 0 0 0");
}

TEST(BodyPlanRebuildsABodyWhenItsSolidsChange)
{
	link::Solids solids = With({ { 0, 0, 0 } });
	BodyPlan     plan;
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "make 0 0 0");
	SetSolids(solids, { 0, 0, 0 }, 2);  // a block placed
	const auto changes = plan.Changes({ 0, 0, 0 }, true, solids);
	CHECK_STR(Describe(changes), "remove 0 0 0, make 0 0 0");
	CHECK(changes.size() == 2 && changes[1].version == solids.Find({ 0, 0, 0 })->version);
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "remove 0 0 0, make 0 0 0");
	SetSolids(solids, { 0, 0, 0 }, 0);  // every block broken
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "remove 0 0 0");
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "");
}

TEST(BodyPlanRemovesEveryBodyWhileBodiesAreOff)
{
	// Bodies stand only while blocks and drawing are both on: off, every body goes, and none is made.
	const link::Solids solids = With({ { 0, 0, 0 }, { 1, 0, 0 } });
	BodyPlan           plan;
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "make 0 0 0, make 1 0 0");
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids, false), "remove 0 0 0, remove 1 0 0");
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids, false), "");
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "make 0 0 0, make 1 0 0");  // on again
}

TEST(BodyPlanTriesAFailedBodyAgainOnlyWhenItsSolidsChange)
{
	link::Solids solids = With({ { 0, 0, 0 } });
	BodyPlan     plan;
	const auto   changes = plan.Changes({ 0, 0, 0 }, true, solids);
	CHECK(changes.size() == 1);
	plan.Failed({ 0, 0, 0 }, changes.empty() ? 0 : changes[0].version);
	CHECK_STR(Describe(plan.Changes({ 0, 0, 0 }, true, solids)), "");  // not every frame
	SetSolids(solids, { 0, 0, 0 }, 3);
	CHECK_STR(Describe(plan.Changes({ 0, 0, 0 }, true, solids)), "remove 0 0 0, make 0 0 0");  // the removal finds no body: harmless
}

TEST(BodyPlanStartsOverWhenCleared)
{
	const link::Solids solids = With({ { 0, 0, 0 }, { 1, 1, 1 } });
	BodyPlan           plan;
	CHECK_STR(Apply(plan, { 0, 0, 0 }, solids), "make 0 0 0, make 1 1 1");
	plan.Removed({ 1, 1, 1 });  // forgotten: its actor was lost in a load, say
	CHECK_STR(Describe(plan.Changes({ 0, 0, 0 }, true, solids)), "make 1 1 1");
	plan.Clear();
	CHECK_STR(Describe(plan.Changes({ 0, 0, 0 }, true, solids)), "make 0 0 0, make 1 1 1");
}

TEST(BodyPlanPutsBoxesInTheWorld)
{
	const auto section = SectionAt({ -0.5, 15.99, 16.0 });
	CHECK((section.has_value() && *section == SectionPos{ -1, 0, 1 }));
	CHECK(!SectionAt({ std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0 }).has_value());
	CHECK(!SectionAt({ 2.0e6, 0.0, 0.0 }).has_value());
	// Section 1 0 -2 holds Minecraft blocks 16 to 31, 0 to 15 and -32 to -17: its centre, in the world, is 24 24 8.
	const Vec3d centre = SectionCentre({ 1, 0, -2 });
	CHECK(Near(centre, { 24.0, 24.0, 8.0 }));
	// Its block 15 0 0, Minecraft block 31 0 -32, is centred at world 31.5 31.5 0.5.
	const LocalBox block = LocalBoxOf({ 15, 0, 0, 16, 1, 1 });
	CHECK(Near({ centre.x + block.centre.x, centre.y + block.centre.y, centre.z + block.centre.z }, { 31.5, 31.5, 0.5 }));
	CHECK(Near(block.half, { 0.5, 0.5, 0.5 }));
	// A layer at height 3 of the section, from Minecraft's x 0 to 16 and z 0 to 4: wide along the world's x, 4 m along
	// its y, which is Minecraft's -z, and 1 m tall.
	const LocalBox layer = LocalBoxOf({ 0, 3, 0, 16, 4, 4 });
	CHECK(Near(layer.centre, { 0.0, 6.0, -4.5 }));
	CHECK(Near(layer.half, { 8.0, 2.0, 0.5 }));
}

TEST(BodyPlanTellsWhetherABodyMovedWithTheOrigin)
{
	// PhysX = world - origin: an actor at PhysX 10 20 30 stands at world 10 1044 30 with the origin 0 1024 0.
	CHECK(PosedAt({ 10.0, 20.0, 30.0 }, { 10.0, 1044.0, 30.0 }, { 0.0, 1024.0, 0.0 }));
	CHECK(PosedAt({ 10.0, 20.005, 30.0 }, { 10.0, 1044.0, 30.0 }, { 0.0, 1024.0, 0.0 }));
	CHECK(!PosedAt({ 10.0, 20.02, 30.0 }, { 10.0, 1044.0, 30.0 }, { 0.0, 1024.0, 0.0 }));
	CHECK(!PosedAt({ 10.0, 20.0, 30.0 }, { 10.0, 1044.0, 30.0 }, { 0.0, 0.0, 0.0 }));  // not moved when the origin was
}
```
- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestBoxMerge.cpp
	tests/TestDrive.cpp
```
```cmake
	tests/TestBoxMerge.cpp
	tests/TestBodyPlan.cpp
	tests/TestDrive.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'physx/BodyPlan.h'`.

- [x] **Step 4: Write `src/physx/BodyPlan.h`**

```cpp
#pragma once

#include "Coords.h"
#include "link/Solids.h"
#include "physx/BoxMerge.h"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

// Where V's bodies go, and when they change (spike 5's "Bodies"): a body for each section with solid blocks among the
// 3 x 3 x 3 around V's, made when the section comes within one of V's and removed once it's more than two away, so
// walking along a boundary doesn't churn bodies, and rebuilt when the section's solids change; and none while bodies are
// off. And where a body's boxes stand in the world. Pure: physx/Bodies makes the actors, and says what it did.
namespace cpmc::px
{
// The section holding a point in Minecraft's axes. Nothing for a point that isn't finite, or is over a million blocks out.
std::optional<SectionPos> SectionAt(const Vec3d& aMc);

// A section's centre, in the world's axes (metres: x east, y north, z up): where its body stands.
Vec3d SectionCentre(const SectionPos& aSection);

// A box of a section's blocks in the world's axes: its centre, from the section's, and its half extents.
struct LocalBox
{
	Vec3d centre;
	Vec3d half;
};

LocalBox LocalBoxOf(const BlockBox& aBox);

// Whether an actor PhysX has at aPhysx, in PhysX's frame, stands at aWorld, given PhysX's origin: within a centimetre.
bool PosedAt(const Vec3d& aPhysx, const Vec3d& aWorld, const Vec3d& aOrigin);

class BodyPlan
{
public:
	struct Change
	{
		bool       make = false;  // false: the section's body is removed
		SectionPos section;
		uint64_t   version = 0;  // a making's: the version of the section's solids it's made from
	};

	// With V in section aV and bodies on (aOn): the bodies to remove, more than two sections away or with their solids
	// gone or changed; then those to make, within one section of V's, with solid blocks and no body, a changed one again.
	// With bodies off, every body is removed. Changes nothing itself: Bodies says what it did.
	std::vector<Change> Changes(const SectionPos& aV, bool aOn, const link::Solids& aSolids) const;

	void Made(const SectionPos& aSection, uint64_t aVersion);
	void Failed(const SectionPos& aSection, uint64_t aVersion);  // not tried again until the section's solids change
	void Removed(const SectionPos& aSection);                    // or forgotten, its actor lost
	void Clear();                                                // every body removed or forgotten

private:
	struct Entry
	{
		uint64_t version = 0;
		bool     made = false;  // false: it failed
	};

	std::map<SectionPos, Entry> entries_;
};
}
```
- [x] **Step 5: Write `src/physx/BodyPlan.cpp`**

```cpp
#include "physx/BodyPlan.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>

namespace cpmc::px
{
namespace
{
constexpr double kFarthest = 1.0e6;  // blocks: a coordinate farther out is garbage

// How many sections apart two are, along the axis they're farthest apart on.
int32_t Apart(const SectionPos& aA, const SectionPos& aB)
{
	return std::max({ std::abs(aA.x - aB.x), std::abs(aA.y - aB.y), std::abs(aA.z - aB.z) });
}
}

std::optional<SectionPos> SectionAt(const Vec3d& aMc)
{
	const double at[3] = { aMc.x, aMc.y, aMc.z };
	if (!std::all_of(std::begin(at), std::end(at), [](double aValue) { return std::isfinite(aValue) && std::fabs(aValue) <= kFarthest; }))
	{
		return std::nullopt;
	}
	return SectionPos{ int32_t(std::floor(aMc.x / 16.0)), int32_t(std::floor(aMc.y / 16.0)), int32_t(std::floor(aMc.z / 16.0)) };
}

Vec3d SectionCentre(const SectionPos& aSection)
{
	return WorldFromMc({ aSection.x * 16.0 + 8.0, aSection.y * 16.0 + 8.0, aSection.z * 16.0 + 8.0 });
}

LocalBox LocalBoxOf(const BlockBox& aBox)
{
	// Minecraft's x is the world's x, its y the world's z, and its z the world's -y.
	const Vec3d centre{ (aBox.x0 + aBox.x1) / 2.0 - 8.0, (aBox.y0 + aBox.y1) / 2.0 - 8.0, (aBox.z0 + aBox.z1) / 2.0 - 8.0 };
	return { WorldFromMc(centre), { (aBox.x1 - aBox.x0) / 2.0, (aBox.z1 - aBox.z0) / 2.0, (aBox.y1 - aBox.y0) / 2.0 } };
}

bool PosedAt(const Vec3d& aPhysx, const Vec3d& aWorld, const Vec3d& aOrigin)
{
	return std::hypot(aPhysx.x + aOrigin.x - aWorld.x, aPhysx.y + aOrigin.y - aWorld.y, aPhysx.z + aOrigin.z - aWorld.z) < 0.01;
}

std::vector<BodyPlan::Change> BodyPlan::Changes(const SectionPos& aV, bool aOn, const link::Solids& aSolids) const
{
	std::vector<Change> changes;
	for (const auto& [section, entry] : entries_)
	{
		const link::Solids::Section* solids = aSolids.Find(section);
		if (!aOn || !solids || Apart(section, aV) > 2)
		{
			changes.push_back({ false, section });
		}
		else if (solids->version != entry.version)
		{
			changes.push_back({ false, section });
			changes.push_back({ true, section, solids->version });
		}
	}
	if (!aOn)
	{
		return changes;
	}
	for (int32_t dx = -1; dx <= 1; ++dx)
	{
		for (int32_t dy = -1; dy <= 1; ++dy)
		{
			for (int32_t dz = -1; dz <= 1; ++dz)
			{
				const SectionPos section{ aV.x + dx, aV.y + dy, aV.z + dz };
				if (const link::Solids::Section* solids = aSolids.Find(section); solids && !entries_.contains(section))
				{
					changes.push_back({ true, section, solids->version });
				}
			}
		}
	}
	return changes;
}

void BodyPlan::Made(const SectionPos& aSection, uint64_t aVersion)
{
	entries_[aSection] = { aVersion, true };
}

void BodyPlan::Failed(const SectionPos& aSection, uint64_t aVersion)
{
	entries_[aSection] = { aVersion, false };
}

void BodyPlan::Removed(const SectionPos& aSection)
{
	entries_.erase(aSection);
}

void BodyPlan::Clear()
{
	entries_.clear();
}
}
```
- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/physx/BoxMerge.cpp
	src/link/Drive.cpp
```
```cmake
	src/physx/BoxMerge.cpp
	src/physx/BodyPlan.cpp
	src/link/Drive.cpp
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `389 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: which sections get bodies around V, and when they change" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 49: The exporter leaves out BlockRender's own bodies

The spec's "Kept out of the export": the bodies are static scene-query shapes made of the city's own filters, so the
exporter would send them to Minecraft as Night City's shape, and Minecraft would get its own blocks back as triangles
and voxels. `PhysxGround` keeps BlockRender's own actors (`OwnActors`, sorted pointers, which the bodies set), and
`KeptShape` leaves out a shape of one of them, as it leaves out triggers and planes. `shapes` marks them too.

**Files:**
- Modify: `src/physx/Export.h`, `src/physx/Export.cpp`, `tests/TestExport.cpp`

- [x] **Step 1: Edit `tests/TestExport.cpp`: the failing test**

```cpp
	CHECK(KeptShape(AuditedClass::RigidStatic, both, PxGeometryType::eTRIANGLEMESH));
	CHECK(KeptShape(AuditedClass::RigidStatic, queryOnly, PxGeometryType::eBOX));
	CHECK(!KeptShape(AuditedClass::RigidStatic, simulationOnly, PxGeometryType::eBOX));  // V's controller doesn't sweep against it
	CHECK(!KeptShape(AuditedClass::RigidStatic, trigger, PxGeometryType::eBOX));
	CHECK(!KeptShape(AuditedClass::RigidStatic, both, PxGeometryType::ePLANE));
	CHECK(!KeptShape(AuditedClass::RigidDynamic, both, PxGeometryType::eCONVEXMESH));  // cars, litter, controllers
```
```cpp
	CHECK(KeptShape(AuditedClass::RigidStatic, both, PxGeometryType::eTRIANGLEMESH, false));
	CHECK(KeptShape(AuditedClass::RigidStatic, queryOnly, PxGeometryType::eBOX, false));
	CHECK(!KeptShape(AuditedClass::RigidStatic, simulationOnly, PxGeometryType::eBOX, false));  // V's controller doesn't sweep against it
	CHECK(!KeptShape(AuditedClass::RigidStatic, trigger, PxGeometryType::eBOX, false));
	CHECK(!KeptShape(AuditedClass::RigidStatic, both, PxGeometryType::ePLANE, false));
	CHECK(!KeptShape(AuditedClass::RigidDynamic, both, PxGeometryType::eCONVEXMESH, false));  // cars, litter, controllers
}

TEST(ExportLeavesOutBlockRendersOwnBodies)
{
	// Part 3's bodies are static scene-query shapes like the city's, made of what V stands on: the exporter leaves them
	// out, so Minecraft never gets its own blocks back as Night City's shape.
	const PxShapeFlags both = PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE;
	CHECK(!KeptShape(AuditedClass::RigidStatic, both, PxGeometryType::eBOX, true));
	int       a = 0, b = 0, c = 0;
	OwnActors own;
	CHECK(!own.Has(&a));
	own.Set({ &c, &a });
	CHECK(own.Has(&a) && own.Has(&c) && !own.Has(&b));
	CHECK_EQ(own.Size(), size_t(2));
	own.Set({});
	CHECK(!own.Has(&a));
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2660: 'cpmc::px::KeptShape': function does not take 4 arguments`.

- [x] **Step 3: Edit `src/physx/Export.h`**

```cpp
// Whether the exporter keeps a shape: a static actor's scene-query shape, and not a trigger or a plane. A shape that's
// only a simulation shape doesn't stop V's controller, and dynamic and kinematic actors are cars, litter, bodies and
// controllers.
bool KeptShape(AuditedClass aActor, physx::PxShapeFlags aFlags, physx::PxGeometryType::Enum aGeometry);
```
```cpp
// Whether the exporter keeps a shape: a static actor's scene-query shape, and not a trigger or a plane, nor one of
// BlockRender's own bodies (aOurs), so Minecraft never gets its own blocks back as the city's shape. A shape that's only a
// simulation shape doesn't stop V's controller, and dynamic and kinematic actors are cars, litter, bodies and controllers.
bool KeptShape(AuditedClass aActor, physx::PxShapeFlags aFlags, physx::PxGeometryType::Enum aGeometry, bool aOurs);

// BlockRender's own actors, part 3's bodies, which the exporter leaves out.
class OwnActors
{
public:
	void   Set(std::vector<const void*> aActors);  // all of them, as they are now
	bool   Has(const void* aActor) const;
	size_t Size() const { return sorted_.size(); }

private:
	std::vector<const void*> sorted_;
};
```
```cpp
	const Report& State() const { return report_; }
```
```cpp
	const Report& State() const { return report_; }
	OwnActors&    Own() { return own_; }  // BlockRender's own, left out of what's read
```
```cpp
	const physx::PxActor*    controller_ = nullptr;
```
```cpp
	const physx::PxActor*    controller_ = nullptr;
	OwnActors                own_;
```

- [x] **Step 4: Edit `src/physx/Export.cpp`**

```cpp
bool KeptShape(AuditedClass aActor, PxShapeFlags aFlags, PxGeometryType::Enum aGeometry)
{
	return aActor == AuditedClass::RigidStatic && aFlags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE) && !aFlags.isSet(PxShapeFlag::eTRIGGER_SHAPE) &&
	       aGeometry != PxGeometryType::ePLANE;
```
```cpp
bool KeptShape(AuditedClass aActor, PxShapeFlags aFlags, PxGeometryType::Enum aGeometry, bool aOurs)
{
	return aActor == AuditedClass::RigidStatic && aFlags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE) && !aFlags.isSet(PxShapeFlag::eTRIGGER_SHAPE) &&
	       aGeometry != PxGeometryType::ePLANE && !aOurs;
}

void OwnActors::Set(std::vector<const void*> aActors)
{
	std::sort(aActors.begin(), aActors.end());
	sorted_ = std::move(aActors);
}

bool OwnActors::Has(const void* aActor) const
{
	return std::binary_search(sorted_.begin(), sorted_.end(), aActor);
```
```cpp
		if (!KeptShape(actor.cls, hit.shape->getFlags(), geometry))
```
```cpp
		if (!KeptShape(actor.cls, hit.shape->getFlags(), geometry, own_.Has(hit.actor)))
```
```cpp
		const PxGeometryType::Enum geometry = hit.shape->getGeometryType();
		const char*                kind = actor.cls == AuditedClass::RigidStatic ? "static"
```
```cpp
		const PxGeometryType::Enum geometry = hit.shape->getGeometryType();
		const bool                 ours = own_.Has(hit.actor);
		const char*                kind = actor.cls == AuditedClass::RigidStatic ? "static"
```
```cpp
		                            "{}{}{}; query filter {}, simulation filter {}{}",
```
```cpp
		                            "{}{}{}; query filter {}, simulation filter {}{}{}",
```
```cpp
		                            KeptShape(actor.cls, flags, geometry) ? "; the exporter keeps it" : ""));
```
```cpp
		                            KeptShape(actor.cls, flags, geometry, ours) ? "; the exporter keeps it" : "",
		                            ours ? "; BlockRender's own body" : ""));
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `390 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: the exporter leaves out BlockRender's own bodies" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 50: V's bodies, PhysX static actors for Minecraft's blocks

`physx/Bodies`, the spec's "Bodies", "What they're made of", "The origin" and "Loads", in `render_runtime`, since it
changes the game. No unit test can run it: it calls PhysX on the game's objects, through the slots Task 45's addendum
audited, so Task 52 checks it in the game, a test box first. What it does:
- **Each frame** (`Frame`, in OnUpdate after the ground's read): `BodyPlan` says what to remove and make around V's
  section; nothing is touched unless there's a change, or PhysX's origin has moved since the bodies were posed.
- **Scene 0 and the origin:** the exporter's own, read once a frame (`PhysxGround::Prepared`), from V's controller;
  `PxGetPhysics`'s object from the session (`Session::Physics`), which makes the actors.
- **What the bodies are made of:** copied once, when the first is made, from what V stands on. A ray down from 0.5 m
  above V's feet, 2 m long, through every static shape (`eNO_BLOCK`), takes the nearest that stops V: a static actor's
  shape that's both a scene-query and a simulation shape, not a trigger, and not ours. Its query and simulation filter
  data, its flags and its first material are copied. A shape of ours, never attached, made with that material, keeps it
  alive: PhysX takes a reference on each material a shape has (the addendum found it in the game's `createShape`).
  Until there's something to copy, the bodies wait, and the status says why.
- **Making a body:** `createRigidStatic` at the section's centre, in PhysX's frame (world − origin); then for each of
  `MergeBoxes`'s boxes, an exclusive `createShape` (a `PxBoxGeometry`, the copied material and flags), posed by
  `setLocalPose`, given the copied filters, attached, and released, since the actor holds it; then `addActor`. Every
  object PhysX returns is checked against the audited vtables first; if a shape fails, the actor is released with the
  shapes it has, and the section waits for its solids to change. `userData` stays empty.
- **Ours, before each touch:** an actor of ours is removed, posed or checked only if scene 0 is the scene it went into,
  and lists it among its static actors (listed once a frame, when needed), it passes the audit's checks, its
  `getScene` is scene 0, and it still has our shapes: as many as we attached, the first a box of our first box's size,
  with our copied query filter (`getNbShapes`, `getShapes`, `getBoxGeometry` and `getQueryFilterData`, audited in
  Part 2). PhysX reuses a released actor's memory, so if the game ever released one of ours, an actor of its own could
  stand at the same address; the shapes tell them apart. Otherwise it's forgotten without being touched: a load may have
  taken the scene or the actor. If scene 0 isn't the scene they went into, every one is forgotten at once.
- **Removing a body:** `removeActor`, then `release`, which releases its shapes.
- **The origin:** the game shifts all of scene 0 when PhysX's origin moves (step 3's watch saw `shiftOrigin`), so a body
  should stand where it was. When the exporter's last origin differs from the one the bodies were posed by, each is
  checked with `getGlobalPose`, and posed again with `setGlobalPose` only if it's more than a centimetre out. The status
  counts both, and is expected to show no posing.
- **Loads:** while V isn't found, nothing is touched. Once V is found again, each actor is checked as above, and those
  no longer ours are forgotten, and made again if they're still wanted.
- **Off:** with `blocks` off or drawing off, every body is removed; the test box stays, as the box request's own.
- **The test box** (`Box`): one actor, 2 m wide and deep and 1 m tall, 3 m ahead of V along its facing, standing on V's
  feet's level, made as the bodies are. A box request comes before the frame's `Frame`, so it first does what a frame
  would: checks the actors after V was lost, and their poses after PhysX's origin moved.
- **The exporter** is told which actors are ours after every change.

**Files:**
- Create: `src/physx/Bodies.h`, `src/physx/Bodies.cpp`
- Modify: `src/physx/Session.h`, `src/physx/Export.h`, `src/physx/Export.cpp`, `CMakeLists.txt`

- [x] **Step 1: Edit `src/physx/Session.h`**

```cpp
	bool Audited(std::string& aWhy) { return CheckBuild(aWhy); }
```
```cpp
	bool Audited(std::string& aWhy) { return CheckBuild(aWhy); }
	// PxGetPhysics's object, checked; null, with aWhy, if PhysX isn't the audited build. Part 3's bodies are made with it.
	physx::PxPhysics* Physics(std::string& aWhy) { return CheckBuild(aWhy) ? physics_ : nullptr; }
```

- [x] **Step 2: Edit `src/physx/Export.h`**

```cpp
	struct Report
```
```cpp
	// Scene 0 and PhysX's origin this frame, as the reads have them, for part 3's bodies: read once a frame, whoever asks
	// first. Null if they can't be had, with State().why saying why.
	physx::PxScene* Prepared(Vec3d& aOrigin);

	struct Report
```

- [x] **Step 3: Edit `src/physx/Export.cpp`**

```cpp
	scene_ = scene;
	return true;
}
```
```cpp
	scene_ = scene;
	return true;
}

PxScene* PhysxGround::Prepared(Vec3d& aOrigin)
{
	if (!Prepare())
	{
		return nullptr;
	}
	aOrigin = *report_.origin;
	return scene_;
}
```

- [x] **Step 4: Write `src/physx/Bodies.h`**

```cpp
#pragma once

#include "Coords.h"
#include "link/Solids.h"
#include "physx/BodyPlan.h"
#include "physx/Export.h"
#include "physx/PxHeaders.h"
#include "physx/Session.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

// V against Minecraft's blocks (spike 5's "Part 3"): a PhysX static actor for each section with solid blocks around V,
// its blocks merged into boxes, as BodyPlan says, each box a shape made of whatever V stood on; and step 4's test box.
// They're added and removed in OnUpdate, on the main thread, when step 3's scene watch found no other thread calling
// scene 0. Every PhysX object is checked against the audit before it's used, ours among them, and an actor of ours is
// touched only while scene 0 is the scene it went into and still lists it, with our shapes: a load may take either away,
// and PhysX may give a released actor's memory to one of the game's. Nothing is touched while V isn't found, or at
// unload. Main thread only.
namespace cpmc::px
{
class Bodies
{
public:
	Bodies(Session& aSession, PhysxGround& aGround) : session_(aSession), ground_(aGround) {}

	// Once a frame, after the ground's Frame: V's feet (world axes; nothing while V isn't found, as in a load), whether
	// bodies are on (set blocks 1, and drawing on, so blocks nobody can see never stop V), and Minecraft's solid blocks.
	void Frame(const std::optional<Vec3d>& aFeet, bool aOn, const link::Solids& aSolids);

	// box on|off (step 4's opening check): a box 2 m wide and deep and 1 m tall, 3 m ahead of V along aForward, standing
	// on V's feet's level, made as the bodies are. The ground's Frame must have had V's feet this frame. Says what
	// happened, for the request's report.
	std::string Box(bool aOn, const Vec3d& aFeet, const Vec3f& aForward);

	struct Report
	{
		size_t               sections = 0;       // with bodies
		size_t               boxes = 0;          // their shapes
		uint64_t             made = 0;           // bodies, the box among them, since the start
		uint64_t             removed = 0;
		uint64_t             failed = 0;         // that PhysX didn't make
		uint64_t             lost = 0;           // forgotten without being touched: scene 0 changed, or stopped listing them
		double               slowestMs = 0.0;    // the slowest making, since the start
		std::string          madeOf;             // what they're made of, once copied from what V stood on
		uint64_t             originChanges = 0;  // seen while actors of ours stood
		uint64_t             reposed = 0;        // actors posed again after one, which the game hadn't moved with its scene
		std::optional<Vec3d> box;                // the test box's centre, world axes, while it stands
		std::string          why;                // why bodies wait, while they do
	};

	const Report& State() const { return report_; }

private:
	struct Body
	{
		physx::PxRigidStatic* actor = nullptr;
		Vec3d                 world;  // where it stands, world axes
		size_t                boxes = 0;
		Vec3d                 firstHalf;  // its first box's half extents: with the count, how it's told from another actor
	};

	// What V stood on, copied once for every body: its filters, flags and first material, which a shape of ours, never
	// attached, keeps alive.
	struct Copy
	{
		physx::PxFilterData query;
		physx::PxFilterData simulation;
		physx::PxShapeFlags flags;
		physx::PxMaterial*  material = nullptr;
		physx::PxShape*     keeper = nullptr;
	};

	bool                  Prepare();  // scene 0, PhysX's origin and PxPhysics, this frame: false, with report_.why, if not
	bool                  CopyGround(const Vec3d& aFeet);
	physx::PxRigidStatic* MakeActor(const Vec3d& aWorld, const std::vector<LocalBox>& aBoxes);
	bool                  Ours(const Body& aBody);  // still in scene 0, as it went in, and still the actor we made
	void                  RemoveActor(const Body& aBody);
	void                  Forget();  // every actor of ours, without touching it: its scene is gone
	void                  Check();   // after V was lost: the actors that aren't ours in scene 0 now are forgotten
	void                  Repose();  // after PhysX's origin moved
	void                  Publish();  // which actors are ours, for the exporter, and the report's counts

	Session&                   session_;
	PhysxGround&               ground_;
	BodyPlan                   plan_;
	std::map<SectionPos, Body> bodies_;
	std::optional<Body>        box_;
	std::optional<Copy>        copy_;
	physx::PxScene*            addedTo_ = nullptr;  // the scene our actors went into
	std::optional<Vec3d>       posedBy_;            // PhysX's origin our actors were last posed by, or checked against
	bool                       lost_ = false;       // V was lost while actors of ours stood: check them first
	// This frame's, once prepared
	physx::PxScene*                         scene_ = nullptr;
	physx::PxPhysics*                       physics_ = nullptr;
	Vec3d                                   origin_;
	std::optional<std::vector<const void*>> statics_;  // scene 0's static actors, sorted, once listed
	Report                                  report_;
};
}
```
- [x] **Step 5: Write `src/physx/Bodies.cpp`**

```cpp
#include "physx/Bodies.h"

#include "physx/GameAbi.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>

using namespace physx;

namespace cpmc::px
{
namespace
{
constexpr size_t kActorBytes = 24;         // the vtable pointer, the type ID and base flags, and userData
constexpr PxU32  kMostActors = 1'000'000;  // a scene listing more is garbage
constexpr PxU32  kRayHits = 16;            // the most hits the ray down looks at
constexpr double kRayAbove = 0.5;          // metres above V's feet the ray down starts
constexpr float  kRayLength = 2.0f;        // and how far it goes

// A point in PhysX's frame: world - origin.
PxTransform At(const Vec3d& aWorld, const Vec3d& aOrigin)
{
	return PxTransform(PxVec3(float(aWorld.x - aOrigin.x), float(aWorld.y - aOrigin.y), float(aWorld.z - aOrigin.z)));
}

bool Is(const ObjectCheck& aCheck, AuditedClass aClass)
{
	return aCheck.verdict == ObjectVerdict::Ok && aCheck.cls == aClass;
}

bool SameOrigin(const Vec3d& aA, const Vec3d& aB)
{
	return aA.x == aB.x && aA.y == aB.y && aA.z == aB.z;  // whole numbers of the 1,024 m grid
}
}

void Bodies::Frame(const std::optional<Vec3d>& aFeet, bool aOn, const link::Solids& aSolids)
{
	scene_ = nullptr;
	statics_.reset();
	if (!aFeet)
	{
		lost_ = lost_ || box_ || !bodies_.empty();  // nothing is touched while V isn't found, as in a load
		return;
	}
	if (lost_)
	{
		if (!Prepare())
		{
			return;  // tried again next frame
		}
		Check();
		lost_ = false;
	}
	std::vector<BodyPlan::Change> changes;
	if (const std::optional<SectionPos> v = SectionAt(McFromWorld(*aFeet)))
	{
		changes = plan_.Changes(*v, aOn, aSolids);  // with bodies off, every one is removed
	}
	// PhysX's origin, as the exporter last read it: if it has moved, the actors' poses are checked.
	const std::optional<Vec3d>& seen = ground_.State().origin;
	const bool                  moved = posedBy_ && seen && !SameOrigin(*seen, *posedBy_);
	if (changes.empty() && !moved)
	{
		report_.why.clear();  // nothing waits
		return;
	}
	if (!Prepare())
	{
		return;  // the plan still has the changes: tried again next frame
	}
	report_.why.clear();
	if (posedBy_ && !SameOrigin(origin_, *posedBy_))
	{
		Repose();
	}
	for (const BodyPlan::Change& change : changes)
	{
		if (!change.make)
		{
			if (const auto body = bodies_.find(change.section); body != bodies_.end())
			{
				RemoveActor(body->second);
				bodies_.erase(body);
			}
			plan_.Removed(change.section);
			continue;
		}
		const link::Solids::Section* solids = aSolids.Find(change.section);
		if (!solids)
		{
			continue;
		}
		if (!copy_ && !CopyGround(*aFeet))
		{
			break;  // the bodies wait for V to stand on the city: the plan still has them
		}
		std::vector<LocalBox> boxes;
		for (const BlockBox& box : MergeBoxes(solids->bits))
		{
			boxes.push_back(LocalBoxOf(box));
		}
		const Vec3d                 world = SectionCentre(change.section);
		const auto                  start = std::chrono::steady_clock::now();
		PxRigidStatic* const        actor = MakeActor(world, boxes);
		report_.slowestMs = std::max(report_.slowestMs, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
		if (!actor)
		{
			plan_.Failed(change.section, change.version);  // tried again when the section's solids next change
			continue;
		}
		bodies_[change.section] = { actor, world, boxes.size(), boxes.front().half };
		plan_.Made(change.section, change.version);
	}
	posedBy_ = box_ || !bodies_.empty() ? std::optional<Vec3d>(origin_) : std::nullopt;
	Publish();
}

std::string Bodies::Box(bool aOn, const Vec3d& aFeet, const Vec3f& aForward)
{
	scene_ = nullptr;  // a request comes before this frame's Frame
	statics_.reset();
	if (!aOn && !box_)
	{
		return "box off: there's no box";
	}
	if (aOn && box_)
	{
		return "box on: the box is there already";
	}
	if (!Prepare())
	{
		return std::format("box {}: {}", aOn ? "on" : "off", report_.why);
	}
	if (lost_)
	{
		Check();
		lost_ = false;
	}
	if (posedBy_ && !SameOrigin(origin_, *posedBy_))
	{
		Repose();  // before the bodies count as posed by this origin
	}
	if (!aOn)
	{
		if (box_)
		{
			RemoveActor(*box_);
			box_.reset();
		}
		posedBy_ = bodies_.empty() ? std::nullopt : std::optional<Vec3d>(origin_);
		Publish();
		return "box off: the box is gone";
	}
	if (!copy_ && !CopyGround(aFeet))
	{
		return "box on: " + report_.why;
	}
	// 3 m ahead of V, along the ground: V's facing, or north if V faces straight up or down.
	const double                length = std::hypot(aForward.x, aForward.y);
	const Vec3d                 ahead = length > 0.1 ? Vec3d{ aForward.x / length, aForward.y / length, 0.0 } : Vec3d{ 0.0, 1.0, 0.0 };
	const Vec3d                 centre{ aFeet.x + 3.0 * ahead.x, aFeet.y + 3.0 * ahead.y, aFeet.z + 0.5 };
	const std::vector<LocalBox> boxes = { { { 0.0, 0.0, 0.0 }, { 1.0, 1.0, 0.5 } } };
	PxRigidStatic* const        actor = MakeActor(centre, boxes);
	if (!actor)
	{
		return "box on: PhysX made no actor";
	}
	box_ = Body{ actor, centre, 1, boxes.front().half };
	posedBy_ = origin_;
	Publish();
	return std::format("box on: 2 x 2 x 1 m, centred at world {:.2f} {:.2f} {:.2f}, made of {}", centre.x, centre.y, centre.z,
	                   report_.madeOf);
}

bool Bodies::Prepare()
{
	if (scene_)
	{
		return true;
	}
	Vec3d          origin;
	PxScene* const scene = ground_.Prepared(origin);
	if (!scene)
	{
		report_.why = ground_.State().why;
		return false;
	}
	PxPhysics* const physics = session_.Physics(report_.why);
	if (!physics)
	{
		return false;
	}
	if (addedTo_ && scene != addedTo_)
	{
		Forget();  // the scene our actors went into isn't scene 0 now: a load took it
	}
	scene_ = scene;
	physics_ = physics;
	origin_ = origin;
	return true;
}

bool Bodies::CopyGround(const Vec3d& aFeet)
{
	// The ray down from just above V's feet, through every static shape: the nearest one that stops V, a scene-query and
	// simulation shape that isn't a trigger, and isn't ours, is what the bodies are made of.
	ReadableMemory&    memory = session_.Memory();
	const ModuleBases& bases = session_.Bases();
	PxRaycastHit       hits[kRayHits];
	PxRaycastBuffer    buffer(hits, kRayHits);
	scene_->lockRead();
	scene_->raycast(At({ aFeet.x, aFeet.y, aFeet.z + kRayAbove }, origin_).p, PxVec3(0.0f, 0.0f, -1.0f), kRayLength, buffer,
	                PxHitFlag::eDEFAULT, PxQueryFilterData(PxQueryFlag::eSTATIC | PxQueryFlag::eNO_BLOCK));
	const PxRaycastHit* ground = nullptr;
	for (PxU32 i = 0; i < std::min(buffer.nbTouches, kRayHits); ++i)
	{
		const PxRaycastHit& hit = hits[i];
		if (!Is(CheckObject(memory, bases, hit.actor, kActorBytes), AuditedClass::RigidStatic) ||
		    !Is(CheckObject(memory, bases, hit.shape), AuditedClass::Shape) || ground_.Own().Has(hit.actor))
		{
			continue;
		}
		const PxShapeFlags flags = hit.shape->getFlags();
		if (flags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE) && flags.isSet(PxShapeFlag::eSIMULATION_SHAPE) &&
		    !flags.isSet(PxShapeFlag::eTRIGGER_SHAPE) && (!ground || hit.distance < ground->distance))
		{
			ground = &hit;
		}
	}
	Copy        copy;
	std::string kind;
	if (ground)
	{
		copy.query = ground->shape->getQueryFilterData();
		copy.simulation = ground->shape->getSimulationFilterData();
		copy.flags = ground->shape->getFlags();
		kind = GeometryName(ground->shape->getGeometryType());
		if (ground->shape->getMaterials(&copy.material, 1) != 1 || !memory.Range(copy.material, 16))
		{
			copy.material = nullptr;
		}
	}
	scene_->unlockRead();
	if (!copy.material)
	{
		report_.why = ground ? "the ground under V has no material to copy" : "they wait for V to stand on the city, to be made of what it's made of";
		return false;
	}
	// A shape of ours, never attached, keeps the material alive: PhysX takes a reference on each material a shape has.
	copy.keeper = physics_->createShape(PxBoxGeometry(0.5f, 0.5f, 0.5f), &copy.material, 1, true, copy.flags);
	if (!Is(CheckObject(memory, bases, copy.keeper), AuditedClass::Shape))
	{
		report_.why = "PhysX made no shape of the ground's material";
		return false;
	}
	copy_ = copy;
	report_.madeOf = std::format("a static {}'s: query filter {}, simulation filter {}, flags 0x{:x}", kind, FilterKey(copy.query),
	                             FilterKey(copy.simulation), uint32_t(PxU8(copy.flags)));
	return true;
}

PxRigidStatic* Bodies::MakeActor(const Vec3d& aWorld, const std::vector<LocalBox>& aBoxes)
{
	ReadableMemory&      memory = session_.Memory();
	const ModuleBases&   bases = session_.Bases();
	PxRigidStatic* const actor = physics_->createRigidStatic(At(aWorld, origin_));
	if (!Is(CheckObject(memory, bases, actor, kActorBytes), AuditedClass::RigidStatic))
	{
		++report_.failed;  // what isn't the audited actor is left alone
		return nullptr;
	}
	for (const LocalBox& box : aBoxes)
	{
		PxShape* const shape = physics_->createShape(PxBoxGeometry(float(box.half.x), float(box.half.y), float(box.half.z)), &copy_->material,
		                                             1, true, copy_->flags);
		if (!Is(CheckObject(memory, bases, shape), AuditedClass::Shape))
		{
			actor->release();  // with the shapes attached so far
			++report_.failed;
			return nullptr;
		}
		shape->setLocalPose(PxTransform(PxVec3(float(box.centre.x), float(box.centre.y), float(box.centre.z))));
		shape->setQueryFilterData(copy_->query);
		shape->setSimulationFilterData(copy_->simulation);
		actor->attachShape(*shape);
		shape->release();  // the actor holds it now
	}
	scene_->addActor(*actor);
	addedTo_ = scene_;
	if (statics_)
	{
		statics_->insert(std::upper_bound(statics_->begin(), statics_->end(), static_cast<const void*>(actor)), actor);
	}
	++report_.made;
	return actor;
}

bool Bodies::Ours(const Body& aBody)
{
	if (!statics_)
	{
		// Scene 0's static actors, listed once a frame when needed: one of ours that isn't among them was taken away.
		scene_->lockRead();
		std::vector<PxActor*> actors(std::min(scene_->getNbActors(PxActorTypeFlag::eRIGID_STATIC), kMostActors));
		actors.resize(scene_->getActors(PxActorTypeFlag::eRIGID_STATIC, actors.data(), PxU32(actors.size())));
		scene_->unlockRead();
		statics_.emplace(actors.begin(), actors.end());
		std::sort(statics_->begin(), statics_->end());
	}
	ReadableMemory&            memory = session_.Memory();
	const ModuleBases&         bases = session_.Bases();
	const PxRigidStatic* const actor = aBody.actor;
	if (!std::binary_search(statics_->begin(), statics_->end(), static_cast<const void*>(actor)) ||
	    !Is(CheckObject(memory, bases, actor, kActorBytes), AuditedClass::RigidStatic) || actor->getScene() != scene_ ||
	    actor->getNbShapes() != aBody.boxes || !copy_)
	{
		return false;
	}
	// PhysX reuses a released actor's memory, so an actor of the game's may stand where ours stood: ours still has our
	// first box, filtered as we filtered it.
	PxShape*      shape = nullptr;
	PxBoxGeometry box;
	return actor->getShapes(&shape, 1) == 1 && Is(CheckObject(memory, bases, shape), AuditedClass::Shape) && shape->getBoxGeometry(box) &&
	       std::abs(box.halfExtents.x - float(aBody.firstHalf.x)) < 1e-3f && std::abs(box.halfExtents.y - float(aBody.firstHalf.y)) < 1e-3f &&
	       std::abs(box.halfExtents.z - float(aBody.firstHalf.z)) < 1e-3f && shape->getQueryFilterData() == copy_->query;
}

void Bodies::RemoveActor(const Body& aBody)
{
	if (!Ours(aBody))
	{
		++report_.lost;  // scene 0 no longer lists it, or it isn't ours now: forgotten, not touched
		return;
	}
	scene_->removeActor(*aBody.actor);
	aBody.actor->release();
	statics_->erase(std::lower_bound(statics_->begin(), statics_->end(), static_cast<const void*>(aBody.actor)));
	++report_.removed;
}

void Bodies::Forget()
{
	report_.lost += bodies_.size() + (box_ ? 1 : 0);
	bodies_.clear();
	box_.reset();
	plan_.Clear();
	addedTo_ = nullptr;
	posedBy_.reset();
	Publish();
}

void Bodies::Check()
{
	for (auto body = bodies_.begin(); body != bodies_.end();)
	{
		if (Ours(body->second))
		{
			++body;
			continue;
		}
		++report_.lost;
		plan_.Removed(body->first);  // made again, if it's still wanted
		body = bodies_.erase(body);
	}
	if (box_ && !Ours(*box_))
	{
		++report_.lost;
		box_.reset();
	}
	Publish();
}

void Bodies::Repose()
{
	// The game shifts all of scene 0 when PhysX's origin moves (step 3's scene watch), ours with it: each actor should
	// already stand where it was. One that doesn't is posed again.
	++report_.originChanges;
	const auto check = [&](const Body& aBody) {
		if (!Ours(aBody))
		{
			return false;
		}
		const PxTransform pose = aBody.actor->getGlobalPose();
		if (!PosedAt({ pose.p.x, pose.p.y, pose.p.z }, aBody.world, origin_))
		{
			aBody.actor->setGlobalPose(At(aBody.world, origin_));
			++report_.reposed;
		}
		return true;
	};
	for (auto body = bodies_.begin(); body != bodies_.end();)
	{
		if (check(body->second))
		{
			++body;
			continue;
		}
		++report_.lost;
		plan_.Removed(body->first);
		body = bodies_.erase(body);
	}
	if (box_ && !check(*box_))
	{
		++report_.lost;
		box_.reset();
	}
	posedBy_ = origin_;
}

void Bodies::Publish()
{
	std::vector<const void*> actors;
	report_.boxes = 0;
	for (const auto& [section, body] : bodies_)
	{
		actors.push_back(body.actor);
		report_.boxes += body.boxes;
	}
	if (box_)
	{
		actors.push_back(box_->actor);
	}
	report_.sections = bodies_.size();
	report_.box = box_ ? std::optional<Vec3d>(box_->world) : std::nullopt;
	ground_.Own().Set(std::move(actors));
}
}
```
- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/physx/SceneHooks.cpp
)
```
```cmake
	src/physx/SceneHooks.cpp
	src/physx/Bodies.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `390 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: V's bodies, PhysX static actors for Minecraft's blocks" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 51: BlockRender 0.15.0 gives Minecraft's blocks around V bodies, and a test box

The bodies in the game:
- **`set blocks 0|1`:** bodies for Minecraft's blocks around V, off by default until step 4 passes. They're made only
  while drawing is on, so blocks nobody can see never stop V, and they stay while unlinked or with Minecraft gone, as
  the drawn blocks do.
- **`box on|off`:** step 4's test box, through the same code, before any block.
- **Each frame,** after the ground's read, `Bodies::Frame` with V's feet. The ground's `Frame` now gets V's feet even
  without V's camera, as in a car, so the bodies follow V there; the city is still read only with the camera.
- **The status:** a `bodies=` line after `solids=`: on or off; how many sections and boxes; what was made, removed,
  failed and lost since the start, and the slowest making; what they're made of; origin changes and posings; the test
  box; and why they wait, while they do. It shows all of it with the bodies off too, since the box stands either way.
  The box is also a `structure=testbox` line, so the overlay shows V's distance to it, since nobody can see it.
- **The overlay:** a bodies line, with a Blocks button (`set blocks`) and a Box button (`box on|off`).
- Version 0.15.0.

**Files:**
- Modify: `src/Requests.h`, `src/Requests.cpp`, `src/Status.h`, `src/Status.cpp`, `src/Plugin.cpp`,
  `tools/blockrender_hud/init.lua`, `tests/TestRequests.cpp`, `tests/TestStatus.cpp`, `CMakeLists.txt`

- [x] **Step 1: Edit `tests/TestRequests.cpp`: the failing test**

```cpp
TEST(RequestsForTheGround)
```
```cpp
TEST(RequestsForTheBodies)
{
	std::string error;
	const auto  on = cpmc::ParseRequest("box on", error);
	CHECK(on && on->kind == cpmc::Request::Kind::Box && on->what == "on");
	const auto off = cpmc::ParseRequest("box off", error);
	CHECK(off && off->kind == cpmc::Request::Kind::Box && off->what == "off");
	CHECK(!cpmc::ParseRequest("box", error));
	CHECK_STR(error, "box takes on or off");
	const auto blocks = cpmc::ParseRequest("set blocks 1", error);
	CHECK(blocks && blocks->kind == cpmc::Request::Kind::Set && blocks->what == "blocks" && blocks->number == 1.0);
}

TEST(RequestsForTheGround)
```

- [x] **Step 2: Edit `tests/TestStatus.cpp`: the failing test**

```cpp
TEST(StatusWritesTheSceneWatch)
```
```cpp
TEST(StatusWritesTheBodies)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	report.link = link;
	std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "bodies=off: 0 sections with bodies, 0 boxes; 0 made, 0 removed, 0 failed and 0 lost since the start, the slowest "
	                  "making 0.00 ms; made of what V stands on, once copied; 0 origin changes, 0 bodies posed again; the test box off"));
	CHECK(status.find("solids=") < status.find("bodies=") && status.find("bodies=") < status.find("ring="));
	link.bodiesOn = true;
	link.bodiesWhy = "they wait for V to stand on the city, to be made of what it's made of";
	report.link = link;
	CHECK(cpmc::FormatStatus(report).find("bodies=on: 0 sections with bodies, ") != std::string::npos);
	CHECK(cpmc::FormatStatus(report).find("; the test box off; waiting: they wait for V to stand on the city, to be made of what it's "
	                                      "made of\n") != std::string::npos);
	link.bodiesWhy.clear();
	link.bodySections = 3;
	link.bodyBoxes = 7;
	link.bodiesMade = 5;
	link.bodiesRemoved = 2;
	link.bodiesFailed = 1;
	link.bodiesLost = 0;
	link.bodySlowestMs = 0.416;
	link.bodiesMadeOf = "a static triangleMesh's: query filter 0:0:449c004:0, simulation filter 5c004:0:13ce:0, flags 0x7";
	link.originChanges = 1;
	link.bodiesReposed = 0;
	link.box = cpmc::Vec3d{ -116.5, -2002.25, 6.64 };
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report),
	          "bodies=on: 3 sections with bodies, 7 boxes; 5 made, 2 removed, 1 failed and 0 lost since the start, the slowest "
	          "making 0.42 ms; made of a static triangleMesh's: query filter 0:0:449c004:0, simulation filter 5c004:0:13ce:0, flags "
	          "0x7; 1 origin change, 0 bodies posed again; the test box at world -116.50 -2002.25 6.64"));
}

TEST(StatusWritesTheSceneWatch)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2065: 'Box': undeclared identifier`.

- [x] **Step 4: Edit `src/Requests.h`**

```cpp
//   shapes                     (spike 5: the game's shapes within 2 m of V, into the log)
```
```cpp
//   shapes                     (spike 5: the game's shapes within 2 m of V, into the log)
//   box on|off                 (spike 5's step 4: a test box 3 m ahead of V, made as the bodies for blocks are)
```
```cpp
		Shapes,
```
```cpp
		Shapes,
		Box,
```

- [x] **Step 5: Edit `src/Requests.cpp`**

```cpp
	if (command == "link" || command == "build" || command == "scenewatch")
```
```cpp
	if (command == "link" || command == "build" || command == "scenewatch" || command == "box")
```
```cpp
		const Request::Kind kind = command == "link"    ? Request::Kind::Link
		                           : command == "build" ? Request::Kind::Build
		                                                : Request::Kind::SceneWatch;
```
```cpp
		const Request::Kind kind = command == "link"         ? Request::Kind::Link
		                           : command == "build"      ? Request::Kind::Build
		                           : command == "scenewatch" ? Request::Kind::SceneWatch
		                                                     : Request::Kind::Box;
```

- [x] **Step 6: Edit `src/Status.h`**

```cpp
	// Minecraft's solid blocks (spike 5's "Part 3")
	size_t   solidSections = 0;
	uint64_t solidBlocks = 0;
```
```cpp
	// Minecraft's solid blocks, and V's bodies for them (spike 5's "Part 3"): the counts are since the start
	size_t               solidSections = 0;
	uint64_t             solidBlocks = 0;
	bool                 bodiesOn = false;  // set blocks 1, with drawing on
	std::string          bodiesWhy;         // why they wait, while they do
	size_t               bodySections = 0;
	size_t               bodyBoxes = 0;
	uint64_t             bodiesMade = 0;
	uint64_t             bodiesRemoved = 0;
	uint64_t             bodiesFailed = 0;
	uint64_t             bodiesLost = 0;
	double               bodySlowestMs = 0.0;
	std::string          bodiesMadeOf;      // empty: not copied yet
	uint64_t             originChanges = 0;
	uint64_t             bodiesReposed = 0;
	std::optional<Vec3d> box;               // the test box's centre, world axes, while it stands
```
```cpp
// drive, positions, ground, solids, ring, atlas, hud, latency, burst and processes); then one structure line per structure:
```
```cpp
// drive, positions, ground, solids, bodies, ring, atlas, hud, latency, burst and processes); then one structure line per
// structure:
```

- [x] **Step 7: Edit `src/Status.cpp`**

```cpp
std::string FormatLink(const LinkReport& aLink)
```
```cpp
// V's bodies for Minecraft's blocks, and step 4's test box, which stands whether the bodies are on or off.
std::string FormatBodies(const LinkReport& aLink)
{
	return std::format("bodies={}: {} sections with bodies, {} boxes; {} made, {} removed, {} failed and {} lost since the start, the "
	                   "slowest making {:.2f} ms; made of {}; {} origin change{}, {} bodies posed again; {}{}\n",
	                   aLink.bodiesOn ? "on" : "off", aLink.bodySections, aLink.bodyBoxes, aLink.bodiesMade, aLink.bodiesRemoved,
	                   aLink.bodiesFailed, aLink.bodiesLost, aLink.bodySlowestMs,
	                   aLink.bodiesMadeOf.empty() ? "what V stands on, once copied" : OneLine(aLink.bodiesMadeOf), aLink.originChanges,
	                   aLink.originChanges == 1 ? "" : "s", aLink.bodiesReposed,
	                   aLink.box ? "the test box at world " + Position(*aLink.box) : std::string("the test box off"),
	                   aLink.bodiesWhy.empty() ? "" : "; waiting: " + OneLine(aLink.bodiesWhy));
}

std::string FormatLink(const LinkReport& aLink)
```
```cpp
	out += std::format("solids=Minecraft's solid blocks: {} in {} sections\n", aLink.solidBlocks, aLink.solidSections);
```
```cpp
	out += std::format("solids=Minecraft's solid blocks: {} in {} sections\n", aLink.solidBlocks, aLink.solidSections);
	out += FormatBodies(aLink);
```

- [x] **Step 8: Edit `src/Plugin.cpp`**

```cpp
// City's shape for Minecraft from the game's PhysX; and it keeps Minecraft's solid blocks, for V to stand on.
```
```cpp
// City's shape for Minecraft from the game's PhysX; and it makes Minecraft's solid blocks around V into PhysX bodies,
// for V to stand on.
```
```cpp
#include "log/FrameLogData.h"
```
```cpp
#include "log/FrameLogData.h"
#include "physx/Bodies.h"
```
```cpp
constexpr const char* kVersion = "0.14.0";
```
```cpp
constexpr const char* kVersion = "0.15.0";
```
```cpp
DWORD                 g_mainThread = 0;   // OnUpdate's
```
```cpp
DWORD                 g_mainThread = 0;   // OnUpdate's

// Spike 5's V against blocks (its spec's "Part 3"). Main thread only.
cpmc::px::Bodies g_bodies{ g_physx, g_ground };  // PhysX bodies for Minecraft's blocks around V, and step 4's test box
bool             g_blocks = false;               // set blocks 1: the bodies, while drawing is on
```
```cpp
	std::optional<cpmc::link::GamePose> pose;
	if (const std::optional<cpmc::PlayerPose> player = cpmc::LocatePlayer(); player && player->camera)
```
```cpp
	const std::optional<cpmc::PlayerPose> player = cpmc::LocatePlayer();
	std::optional<cpmc::link::GamePose>   pose;
	if (player && player->camera)
```
```cpp
	// Night City's shape, given V's feet from before this frame's teleport: V's controller is within a frame's move of
	// them, whether the teleport has moved it yet or not.
	g_ground.Frame(pose ? std::optional<cpmc::Vec3d>(pose->feet) : std::nullopt);
```
```cpp
	// Night City's shape, and V's bodies, given V's feet from before this frame's teleport, in a car too: V's controller is
	// within a frame's move of them, whether the teleport has moved it yet or not. The city is read only with V's camera.
	const std::optional<cpmc::Vec3d> feet = player ? std::optional<cpmc::Vec3d>(player->feet) : std::nullopt;
	g_ground.Frame(feet);
```
```cpp
	}
	g_second.cityReadMsSum += state.cityReadMs;
```
```cpp
	}
	g_bodies.Frame(feet, g_blocks && g_renderer->ModeName() != "off", g_link->SolidSections());
	g_second.cityReadMsSum += state.cityReadMs;
```
```cpp
	}
	if (aRequest.what.starts_with("hooks."))
```
```cpp
	}
	if (aRequest.what == "blocks")
	{
		if (aRequest.number != 0.0 && aRequest.number != 1.0)
		{
			return Report("set blocks: 0 leaves Minecraft's blocks without bodies, and 1 gives those around V bodies V stands on");
		}
		g_blocks = aRequest.number == 1.0;
		return Report(!g_blocks                            ? "set blocks 0: the bodies go"
		              : g_renderer->ModeName() == "off" ? "set blocks 1: the bodies come once drawing is on"
		                                                  : "set blocks 1: Minecraft's blocks around V get bodies");
	}
	if (aRequest.what.starts_with("hooks."))
```
```cpp
// The command in aFolder's blockrender.request, if there is one: in the plugin's folder, where the agent writes it, or
```
```cpp
// box on|off (spike 5's step 4): a test box 3 m ahead of V, made as the bodies for blocks are, before any block is.
void BoxRequest(bool aOn)
{
	const std::optional<cpmc::PlayerPose> player = cpmc::LocatePlayer();
	if (!player)
	{
		return Report(std::format("box {}: there's no player", aOn ? "on" : "off"));
	}
	g_ground.Frame(player->feet);  // scene 0 and PhysX's origin this frame, from V's controller
	Report(g_bodies.Box(aOn, player->feet, player->forward));
}

// The command in aFolder's blockrender.request, if there is one: in the plugin's folder, where the agent writes it, or
```
```cpp
	case cpmc::Request::Kind::Shapes: Shapes(); break;
```
```cpp
	case cpmc::Request::Kind::Shapes: Shapes(); break;
	case cpmc::Request::Kind::Box: BoxRequest(request->what == "on"); break;
```
```cpp
	report.physxWhy = physx.why;
```
```cpp
	report.physxWhy = physx.why;
	const cpmc::px::Bodies::Report& bodies = g_bodies.State();
	report.bodiesOn = g_blocks && g_renderer->ModeName() != "off";
	report.bodiesWhy = bodies.why;
	report.bodySections = bodies.sections;
	report.bodyBoxes = bodies.boxes;
	report.bodiesMade = bodies.made;
	report.bodiesRemoved = bodies.removed;
	report.bodiesFailed = bodies.failed;
	report.bodiesLost = bodies.lost;
	report.bodySlowestMs = bodies.slowestMs;
	report.bodiesMadeOf = bodies.madeOf;
	report.originChanges = bodies.originChanges;
	report.bodiesReposed = bodies.reposed;
	report.box = bodies.box;
```
```cpp
	report.structures = g_structures;
```
```cpp
	report.structures = g_structures;
	if (const std::optional<cpmc::Vec3d> box = g_bodies.State().box)
	{
		// Step 4's test box can't be seen: as a structure, the overlay shows how far V is from it.
		report.structures.push_back({ "testbox", { box->x - 1.0, box->y - 1.0, box->z - 0.5 }, { box->x + 1.0, box->y + 1.0, box->z + 0.5 } });
	}
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 14, 0);
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 15, 0);
```

- [x] **Step 9: Edit `tools/blockrender_hud/init.lua`**

```lua
-- and one the ground Minecraft gets, with buttons to switch it and to log the shapes around V.
```
```lua
-- and one the ground Minecraft gets, with buttons to switch it and to log the shapes around V; and one V's bodies for
-- Minecraft's blocks, with buttons to turn them on and off, and step 4's test box.
```
```lua
local groundName = nil   -- "floor" or "city"
```
```lua
local groundName = nil   -- "floor" or "city"
local bodies = nil       -- the bodies line: on or off, and how many, or why they wait; and the test box
local blocksOn = nil     -- whether bodies are on: "on" or "off"
local boxOn = false      -- whether the test box stands
```
```lua
    groundName = nil
```
```lua
    groundName = nil
    bodies = nil
    blocksOn = nil
    boxOn = false
```
```lua
            ground = setting .. (known and (", " .. known) or "") .. (fallback and (", fell back: " .. fallback) or "")
```
```lua
            ground = setting .. (known and (", " .. known) or "") .. (fallback and (", fell back: " .. fallback) or "")
        end
        local state = line:match("^bodies=(%a+)")
        if state ~= nil then
            blocksOn = state
            boxOn = line:match("the test box at world") ~= nil
            local counts = line:match("^bodies=%a+: (%d+ sections with bodies, %d+ boxes)")
            local waiting = line:match("; waiting: (.*)$")
            bodies = state .. (counts and (", " .. counts) or "") .. (waiting and (", waiting: " .. waiting) or "")
                .. (boxOn and ", the test box standing" or "")
```
```lua
        end
        ImGui.Separator()
```
```lua
        end
        if bodies ~= nil then
            ImGui.Text("Bodies: " .. bodies)
            -- "###blocks" and "###box" keep the buttons' IDs the same while their labels change.
            ImGui.SameLine()
            if ImGui.Button("Blocks: " .. blocksOn .. "###blocks") then
                request("set blocks " .. (blocksOn == "on" and 0 or 1))
            end
            ImGui.SameLine()
            if ImGui.Button((boxOn and "Box: on" or "Box: off") .. "###box") then
                request("box " .. (boxOn and "off" or "on"))
            end
        end
        ImGui.Separator()
```

- [x] **Step 10: Edit `CMakeLists.txt`: version 0.15.0**

```cmake
project(BlockRender VERSION 0.14.0 LANGUAGES C CXX)
```
```cmake
project(BlockRender VERSION 0.15.0 LANGUAGES C CXX)
```

- [x] **Step 11: Check the overlay's syntax**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && luac -p tools/blockrender_hud/init.lua && echo ok
```
Expected: `ok`.

- [x] **Step 12: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `392 test(s), 0 failure(s)`.

- [x] **Step 13: Check that BlockRender imports nothing from PhysX**

```bash
DUMPBIN="$(ls "/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/"*/bin/Hostx64/x64/dumpbin.exe | head -1)"
cd /d/Cyberpunk-Minecraft/spikes/block-render && MSYS_NO_PATHCONV=1 "$DUMPBIN" -imports -nologo build\\RelWithDebInfo\\BlockRender.dll \
  | grep -ci "physx"
```
Expected: `0`.

- [x] **Step 14: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "collision-and-movement: BlockRender 0.15.0 gives Minecraft's blocks around V bodies, and a test box" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 52: In the game: step 4, V against blocks

The spec's "Step 4", with the user, on BlockRender 0.15.0. The test box first, one interaction at a time, with a save
made just before: if the game's own code expects a game object behind whatever it touches, a body's empty `userData`
could fault it, and the game freezes (risk 6). Then `set blocks 1`, and Minecraft's own blocks. Nobody can see the box:
BlockRender's overlay shows `nearest structure: testbox, … m away`, from V's feet, and spike 1's overlay V's distance to
the origin lines. Task 33's commands for requests, the log and the status come back here. FreeFly (F7) stays off. Note
what you see as you go: Step 18 records it. If the game freezes, read the logs and the newest `ReportQueue` folder, and
force-close it only when the user asks. If a check fails, stop and find the cause with superpowers:systematic-debugging
before going on. Fixes become new tasks.

**Files:**
- Modify: `docs/spikes/2026-10-03-collision-and-movement-results.md`

- [x] **Step 1: Deploy (the game must not be running)**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game first, because a running game locks the DLL.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default -DCP2077_DIR=E:/Cyberpunk2077 > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C|error MSB"; \
  "$CMAKE" --preset default -DCP2077_DIR= > /dev/null; \
  cmp build/RelWithDebInfo/BlockRender.dll /e/Cyberpunk2077/red4ext/plugins/BlockRender/BlockRender.dll && echo installed
```
Expected: `installed`. The second configure clears `CP2077_DIR` again.

- [x] **Step 2: In the game: load, link, and a save**

Ask the user to start the `CyberCraft` instance, then the game, load the free-roam save at spike 3's corner, click Link,
and make a new save there. Expected: in the log, `BlockRender 0.15.0 loaded` and `the turn: … remembered from the last
session …`; in the status, `bodies=off: 0 sections with bodies, 0 boxes; …; the test box off`.

- [x] **Step 3: In the game: the box, walked into**

Ask the user to stand on an open street, facing along it with nothing within 5 m ahead, and click the overlay's Box
button. Expected: `request: box on: 2 x 2 x 1 m, centred at world …, made of a static …'s: query filter …, simulation
filter …, flags 0x…`; the bodies line's `1 made` and `the test box at world …`; and the overlay's `nearest structure:
testbox`, about 2 m away (a little less if V faced across the world's axes, as the box is square to them). Note the
copied filters: a street's were `0:0:449c004:0` and `5c004:0:13ce:0` in step 3.

Ask the user to walk slowly into it. Expected: V stops at about 0.3 m, the radius of V's controller; nothing faults.

- [x] **Step 4: In the game: on the box**

Ask the user to jump onto the box, and stand on it. Expected: V stands on it, the overlay's `V:` z about 1 m above the
street's, and the distance 0.

- [x] **Step 5: In the game: the box shot**

Ask the user to step back a few metres, draw a gun, and fire a few shots at where the box is, a metre up. Record whether
the shots stop at it (impacts in the air) or go through, and that nothing faults.

- [x] **Step 6: In the game: the box driven into**

Ask the user to get into a car and drive slowly into the box, guided by the overlay's distance. Record what the car does,
and that nothing faults.

- [x] **Step 7: In the game: across an origin line**

Ask the user to go on foot to about 10 m from an origin line (spike 1's overlay: the distance to the nearest), face it,
click Box twice (off, then on: the box goes about 3 m ahead, short of the line), and note the bodies line's origin
changes. Then to walk around the box and on across the line a few metres, come back across it, and walk into the box
from beyond. Expected: the origin changes up by 2, one each way, and still `0 bodies posed again`: the game moved the box
with its scene each time, as step 3's watch suggested; and V stopping at the box again.

- [x] **Step 8: In the game: a load with the box there**

With the box standing, ask the user to load the save made in Step 2. Expected: nothing faults. Record what the status
says once V is found again: the box still standing (scene 0 kept, and still listing it), or `1 lost` and `the test box
off`. Then ask the user to click Box until the line says `the test box off`.

- [x] **Step 9: In the game: blocks on**

Ask the user to make a new save, then click the overlay's Blocks button. Expected: `request: set blocks 1: Minecraft's
blocks around V get bodies`, and the bodies line `bodies=on: …`. Blocks within a section of V's get bodies at once:
spike 3's corner still has step 3's blocks.

- [x] **Step 10: In the game: a wall and a tower, and leaving build mode on top**

Ask the user, in build mode, to build a wall 5 blocks long and 3 high and a tower 1 by 1 and 5 high beside it, then to
stand on top of the tower and press F8. Expected: the bodies line's sections, boxes and `made` going up as the blocks
are placed, a few milliseconds at most for the slowest making; and V standing on the tower once build mode ends, not
falling through it.

- [x] **Step 11: In the game: leaving build mode sneaking**

Ask the user to press F8 again, hold Left Ctrl to sneak on top of the tower, and press F8 while still sneaking. V's
controller then starts about a third of a metre inside the top block (risk 9). Record what it does: pushed up onto the
block, falling through it, or stuck.

- [x] **Step 12: In the game: walking and jumping into blocks**

Outside build mode, ask the user to walk into the wall, then to build, in build mode, a step one block high and leave
build mode, and jump onto it. Expected: the wall stops V, and V stands on the step.

- [x] **Step 13: In the game: blocks placed and broken by V**

Outside build mode, with V standing still on open ground, send `mc /setblock ~2 ~ ~ minecraft:stone`: a block 2 blocks
east of Minecraft's player, which follows V. Ask the user to walk east (heading 90 on the overlay) into it: it stops V.
Send `mc /setblock ~2 ~ ~ minecraft:air`, and ask the user to walk east again: V goes through where it was. Then send
`mc /setblock ~ ~ ~ minecraft:stone`, a block where V stands. Record what V's controller does (risk 9), then send
`mc /setblock ~ ~ ~ minecraft:air`.

- [x] **Step 14: In the game: TNT under V**

Ask the user, in build mode, to build a platform 3 by 3 blocks and one high, away from the other builds, stand in its
middle, and leave build mode. Then send `mc /summon minecraft:tnt ~1 ~ ~`: TNT on the platform beside V. Expected: about 4 s later, `explosion at
Minecraft …` in the log, and the platform's blocks gone, their bodies with them. The blast also digs into the street
below, and SkyCraft sets real blocks where it digs (Task 43), which get bodies too: record where they stand (the bodies
line, and what the user sees), and whether V drops to the street or onto them.

- [x] **Step 15: In the game: NPCs, a car and bullets against blocks**

Ask the user to build a wall of blocks across a pavement where people walk, and watch: an NPC walking into it; a car
driven into it; shots fired at it. Record what each does: stops at it, goes through, or anything odd. NPCs don't route
around blocks (a non-goal).

- [x] **Step 16: In the game: the cost**

Ask the user to stand by a build for 10 s with the blocks on, then click Blocks to turn them off, wait 10 s, and click it
again. Read the status's `frame=` line each time, and the bodies line's sections, boxes and slowest making.

- [x] **Step 17: Ask the user to quit the game and stop Minecraft, then check the crash reports**

```bash
ls -t "$LOCALAPPDATA/REDEngine/ReportQueue" 2>/dev/null | head -3
```
Expected: nothing newer than this session's start.

- [x] **Step 18: Record step 4 in `docs/spikes/2026-10-03-collision-and-movement-results.md`**

A "Step 4, in the game: V against blocks" section: each check above with its outcome and numbers; what the bodies were
made of; the origin changes and posings; the load; what V's controller did inside a block; what NPCs, cars and bullets
did; the cost; and what it means: whether Part 3's question is answered, and what's left.

- [x] **Step 19: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-03-collision-and-movement-results.md \
  && git commit -q -m "collision-and-movement: step 4's results in the game" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 53: Close Part 3, and spike 5

- [x] **Step 1: Add "How Part 3 went" to this plan**

At the end: what ran as written, what changed, and what Part 3's checks found.

- [x] **Step 2: Answer spike 5's question in the results**

At the top of `docs/spikes/2026-10-03-collision-and-movement-results.md`, beside Part 1's answer: Parts 2 and 3's, and
the spike's as a whole, against the spec's question; what's left, and whether `set blocks 1` should become the default.

- [x] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs \
  && git commit -q -m "collision-and-movement plan: Part 3 closed, and spike 5" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

- [x] **Step 4: Ask the user whether to finish the branch**

With superpowers:finishing-a-development-branch, before Entities, which starts with its own brainstorm
(`docs/superpowers/handoffs/2026-10-04-spike-5-part-3.md`, "After spike 5: Entities"). Every spike so far was merged into
`main` by a local fast-forward; ask again.

## Found while planning Part 3

The spec's "Changes while planning" records those that touch the design.
- **Part 2's follow-ups come first** (Tasks 38 to 40), as the user asked:
  - the turn ratio is kept in `turn.txt`, and used until the session has its own;
  - the worker runs a quarter of the processors' threads, 1 to 4, below normal priority, and starts the region nearest
    Minecraft's player first; a region's newer job replaces one not started, and waits for one under way. Part 2's
    recording shows why the drop outran it: its regions held 240 to 280 triangles against spot B's 94, and the ground
    under the player waited behind the regions read before it. Whether the game's threads also slowed the worker is
    what its threads' running share, new in the status, settles in Task 46.
- **The query watch** (Task 41): Part 2's watch settled that the game writes nothing to scene 0 while OnUpdate runs, so
  BlockRender's reads there are safe. Part 3 writes there too, and a query on another thread could read the scene's query
  structures while a body changes them, so the watch now counts the scene's queries as well, and Task 46 checks them
  before any body exists.
- **Solids' checks:** a message's count must equal the bits set, as SkyCraft counts them, besides the spec's checks.
- **The plan and the bodies:** `BodyPlan` says what to change, and `Bodies` says what it did, so a frame where PhysX can't
  be reached changes nothing; a body whose solids changed is removed and made again in the same frame.
- **Ours, before each touch:** an actor of ours is touched only while scene 0 is the scene it went into and lists it,
  it passes the audit's checks, its scene is scene 0, and it still has our shapes (their count, the first box's size
  and our query filter), since PhysX reuses a released actor's memory; otherwise it's forgotten, untouched.
- **Loads:** the spec removed the bodies when V was lost. Step 3's watch never saw a load, so nothing is touched while V
  isn't found; once V is found again, each actor is checked, and those no longer ours are forgotten and made again.
- **What they're made of:** the ray down takes the nearest static shape that's a scene-query and a simulation shape,
  not a trigger and not ours; and a shape of ours keeps the copied material alive, since PhysX's `createShape` takes a
  reference on its materials (Task 45's addendum).
- **`PxShape::setLocalPose`** joins the audit: each box is posed on its body's actor, at its section's centre.
- **In a car,** the bodies follow V: the ground's `Frame` gets V's feet without V's camera, though the city is still read
  only with it.
- **The status** counts since the start (made, removed, failed, lost) and the slowest making, rather than a second's,
  and shows them with the bodies off, since the test box stands either way; the box is a structure, so the overlay
  shows V's distance to it.
- **The origin:** a change is noticed from the exporter's last origin, and each body checked then, expecting none to need
  posing: the game shifts all of scene 0.
- **Versions:** 0.14.0 for the follow-ups, the query watch and the solids (Task 46's deploy), and 0.15.0 for the bodies
  (Task 52's).

## How Part 3 went

Part 3 ran on 2026-10-04: Part 2's follow-ups passed, the query watch cleared OnUpdate for the bodies, and step 4
passed, with one GPU crash on the way. The results are in
[docs/spikes/2026-10-03-collision-and-movement-results.md](../../spikes/2026-10-03-collision-and-movement-results.md),
which now answers spike 5's question at its top.
- **As written:** Tasks 38 to 42, 44, 45 and 47 to 51 ran through the plan runner in the real repo exactly as written.
  Every build, test, `mclink` and overlay expectation was met, and each task's files matched the verified scratch
  clone's. 392 tests, no warnings, no PhysX imports.
- **Offline (Task 43):** passed, but for the TNT. The new worker made Task 28's city to the millimetre on 4 threads;
  `mclink`'s reads, not the worker, set the pace offline; and the solids counted 0, 24, 26 and 0 blocks as planned.
  The TNT went off 3.8 blocks high, on blocks SkyCraft had set when Task 28's own blast dug into spot B's ground, kept
  in its saved world: SkyCraft's digging, not the code. So Task 52's step 14 was changed to expect such blocks.
- **The follow-ups and the query watch (Task 46):** passed. The turn came back across a game start; four fast drops
  landed, the worker's queue draining within two seconds; a region costs what its triangles cost, the threads running
  91 to 98% of the time, so Part 2's slow regions were dense, not starved. In 132 s no other thread called scene 0
  while OnUpdate ran, queries included, so Tasks 47 to 52 added the bodies there as planned. The drop's recording ran
  5 minutes rather than 3, to leave the user time.
- **Step 4 (Task 52):** passed, after one crash.
  - **The crash:** building quickly in the first run, the GPU hung (`DXGI_ERROR_DEVICE_HUNG`) at about the point where
    BlockRender inserts its G-buffer list. A race in the renderer, there since spike 4, could explain it: a section's
    old buffer, replaced while our list was on its way to the GPU, was stamped with the fence before the list. One
    commit outside the plan fixed it (`ead8772`, 396 tests), but in 571 block changes after it the fix never had to
    act, so the cause isn't settled. DRED is on (`settings.txt` in the plugin's folder) to name it if it comes back.
  - **The overlay,** at the user's request, shows one row of buttons and five lines, with the rest folded away
    (`87adb6c`, outside the plan).
  - **Changed while running:** steps 13 and 14 placed blocks and TNT by block coordinates, since the plan's relative
    ones follow Minecraft's player, which moves with V (and in step 14 had dropped into the platform).
  - **What it found:** V walks into, stands and jumps on blocks; cars and ordinary bullets stop at them; NPCs'
    navigation walks them through; a block at V's feet holds V until V moves, then V is put on top; leaving build mode
    sneaking pushes V up; origin moves need no posing; a load forgets the bodies untouched; and the bodies cost nothing
    measurable. A blast that digs into the city leaves SkyCraft's blocks in the air above it.
- **Left after spike 5:**
  - `set blocks 1` by default, recommended, waiting for the user's word;
  - the GPU crash's cause, from DRED if it comes back;
  - SkyCraft's blocks from blasts, which the user would rather not have: a Fabric mixin of our own, or a change to
    SkyCraft;
  - NPCs and moving props, for the Entities brainstorm.
