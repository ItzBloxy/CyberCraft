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

- [ ] **Step 1: Write the failing test, `tests/TestTicks.cpp`**

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

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestRouter.cpp
	tools/mclink/McLinkScript.cpp
```
```cmake
	tests/TestRouter.cpp
	tests/TestTicks.cpp
	tools/mclink/McLinkScript.cpp
```

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Ticks.h'`.

- [ ] **Step 4: Write `src/link/Ticks.h`**

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

- [ ] **Step 5: Write `src/link/Ticks.cpp`**

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

- [ ] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Link.cpp
	src/render/PendingSections.cpp
```
```cmake
	src/link/Link.cpp
	src/link/Ticks.cpp
	src/render/PendingSections.cpp
```

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `241 test(s), 0 failure(s)`.

- [ ] **Step 8: Commit**

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

- [ ] **Step 1: Write the failing test, `tests/TestVoxels.cpp`**

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

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestTicks.cpp
	tools/mclink/McLinkScript.cpp
```
```cmake
	tests/TestTicks.cpp
	tests/TestVoxels.cpp
	tools/mclink/McLinkScript.cpp
```

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Voxels.h'`.

- [ ] **Step 4: Write `src/link/Voxels.h`**

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

- [ ] **Step 5: Write `src/link/Voxels.cpp`**

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

- [ ] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Ticks.cpp
	src/render/PendingSections.cpp
```
```cmake
	src/link/Ticks.cpp
	src/link/Voxels.cpp
	src/render/PendingSections.cpp
```

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `249 test(s), 0 failure(s)`.

- [ ] **Step 8: Commit**

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

- [ ] **Step 1: Edit `tests/TestFloor.cpp`: the failing test**

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

- [ ] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C3861: 'FloorVoxels': identifier not found`.

- [ ] **Step 3: Edit `src/link/Floor.h`**

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

- [ ] **Step 4: Edit `src/link/Floor.cpp`**

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

- [ ] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `250 test(s), 0 failure(s)`.

- [ ] **Step 6: Commit**

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

- [ ] **Step 1: Write the failing test, `tests/TestDrive.cpp`**

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

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestVoxels.cpp
	tools/mclink/McLinkScript.cpp
```
```cmake
	tests/TestVoxels.cpp
	tests/TestDrive.cpp
	tools/mclink/McLinkScript.cpp
```

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Drive.h'`.

- [ ] **Step 4: Write `src/link/Drive.h`**

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

- [ ] **Step 5: Write `src/link/Drive.cpp`**

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

- [ ] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Voxels.cpp
	src/render/PendingSections.cpp
```
```cmake
	src/link/Voxels.cpp
	src/link/Drive.cpp
	src/render/PendingSections.cpp
```

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `260 test(s), 0 failure(s)`.

- [ ] **Step 8: Commit**

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

- [ ] **Step 1: Edit `tests/TestLink.cpp`: the failing test**

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

- [ ] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C3861: 'FirstTeleportSeq': identifier not found`.

- [ ] **Step 3: Edit `src/link/Link.h`**

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

- [ ] **Step 4: Edit `src/link/Link.cpp`**

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

- [ ] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `261 test(s), 0 failure(s)`.

- [ ] **Step 6: Commit**

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

- [ ] **Step 1: Edit `tests/TestLink.cpp`: the failing tests**

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

- [ ] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'placeV': is not a member of 'cpmc::link::Link::Status'`.

- [ ] **Step 3: Edit `src/link/Link.h`**

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

- [ ] **Step 4: Edit `src/link/Link.cpp`**

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

- [ ] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `268 test(s), 0 failure(s)`.

- [ ] **Step 6: Commit**

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

- [ ] **Step 1: Edit `tests/TestMcLinkScript.cpp`: the failing tests**

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

- [ ] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2065: 'Down': undeclared identifier`.

- [ ] **Step 3: Edit `tools/mclink/McLinkScript.h`**

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

- [ ] **Step 4: Edit `tools/mclink/McLinkScript.cpp`**

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

- [ ] **Step 5: Edit `tools/mclink/Main.cpp`**

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

- [ ] **Step 6: Write `tools/mclink/drive.txt`**

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

- [ ] **Step 7: Write `tools/mclink/relink.txt`**

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

- [ ] **Step 8: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `270 test(s), 0 failure(s)`.

- [ ] **Step 9: Run mclink without Minecraft**

Minecraft and the game must not be running.
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && printf 'status\nquit\n' > build/selfcheck.txt \
  && ./build/RelWithDebInfo/mclink.exe build/selfcheck.txt; echo "exit: $?"
```
Expected: `the mapping: created (process …); waiting for Minecraft`, then the status with `Minecraft: absent`, a
`game side:` line whose sequence is a large odd number (it starts from the clock), and `drive: off, teleport 0 applied
of …`. Then `exit: 0`.

- [ ] **Step 10: Commit**

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

- [ ] **Step 1: Check that the game isn't running**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game: it would hold the mapping.

- [ ] **Step 2: Ask the user to start Minecraft**

The user starts the `CyberCraft` instance from the Modrinth App, and says when its title screen shows. Then:
```bash
grep -E "SkyCraft:" "/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log" | tail -3
```
Expected: `SkyCraft: can't open Skyrim's shared memory yet (Windows error 2: …)`: waiting for a game side.

- [ ] **Step 3: Run step 1's script**

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

- [ ] **Step 4: Look at the HUD images**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && uv run --no-project -q --with numpy --with pillow python tools/view_dump.py build/step1
```
Expected: `clicks.dds` and `count.dds` converted. Read `build/step1/clicks.png` and `build/step1/count.png`, and check:
- **`clicks.png`:** no red text in the chat, so no click was refused;
- **`count.png`:** the chat says the `/fill` replaced 20 blocks: all twenty right clicks while strafing placed stone.

- [ ] **Step 5: Run the relink script twice**

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

- [ ] **Step 6: Ask the user to stop Minecraft**

Its window is hidden, so the user stops it from the Modrinth App.

- [ ] **Step 7: Write step 1's results, `docs/spikes/2026-10-03-collision-and-movement-results.md`**

From `build/step1/drive.log`, the relink logs, the two PNGs and Minecraft's log, as spike 4's results doc does:
- the versions;
- each check above with its outcome and numbers: the release's time, the speeds, the eye heights, where the wall
  stopped the walk, the render delay and late ticks, and the clicks;
- anything that surprised, and what it means for step 2.

- [ ] **Step 8: Commit**

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

- [ ] **Step 1: Edit `tests/TestBuildMode.cpp`: the failing tests**

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

- [ ] **Step 2: Edit `tests/TestRouter.cpp`: F instead of W**

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

- [ ] **Step 3: Build and run the tests; the new ones must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | grep -E "^FAIL|test\(s\)"
```
Expected: `FAIL BuildModeGivesMinecraftTheMovementKeys`, `FAIL BuildModeHidesTheGamesCrouchKeys` and `FAIL BuildModeLeavesOnShiftF6WithLeftShiftForwarded` among the failures: the tests compile, since the keys go through the same calls as the buttons.

- [ ] **Step 4: Edit `src/input/BuildMode.h`**

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

- [ ] **Step 5: Edit `src/input/BuildMode.cpp`**

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

- [ ] **Step 6: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `274 test(s), 0 failure(s)`.

- [ ] **Step 7: Commit**

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

- [ ] **Step 1: Edit `tests/TestRouter.cpp`: the failing tests**

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

- [ ] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'TakeMouseX': is not a member of 'cpmc::input::Router'`.

- [ ] **Step 3: Edit `src/input/BuildMode.h`**

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

- [ ] **Step 4: Edit `src/input/BuildMode.cpp`**

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

- [ ] **Step 5: Edit `src/input/Router.h`**

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

- [ ] **Step 6: Edit `src/input/Router.cpp`**

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

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `278 test(s), 0 failure(s)`.

- [ ] **Step 8: Commit**

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

- [ ] **Step 1: Write the failing test, `tests/TestTurn.cpp`**

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

- [ ] **Step 2: Write the failing test, `tests/TestDriveLog.cpp`**

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

- [ ] **Step 3: Edit `CMakeLists.txt` to add the tests**

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

- [ ] **Step 4: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Turn.h'`, and the same for `log/DriveLog.h`.

- [ ] **Step 5: Write `src/link/Turn.h`**

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

- [ ] **Step 6: Write `src/link/Turn.cpp`**

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

- [ ] **Step 7: Write `src/log/DriveLog.h`**

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

- [ ] **Step 8: Write `src/log/DriveLog.cpp`**

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

- [ ] **Step 9: Edit `CMakeLists.txt` to add the sources**

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

- [ ] **Step 10: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `287 test(s), 0 failure(s)`.

- [ ] **Step 11: Commit**

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

- [ ] **Step 1: Edit `tests/TestRequests.cpp`: the failing test**

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

- [ ] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2065: 'DriveLog': undeclared identifier`.

- [ ] **Step 3: Edit `src/Requests.h`**

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

- [ ] **Step 4: Edit `src/Requests.cpp`**

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

- [ ] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `288 test(s), 0 failure(s)`.

- [ ] **Step 6: Commit**

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

- [ ] **Step 1: Edit `tests/TestStatus.cpp`: the failing test**

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

- [ ] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'drive': is not a member of 'cpmc::LinkReport'`.

- [ ] **Step 3: Edit `src/Status.h`**

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

- [ ] **Step 4: Edit `src/Status.cpp`**

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

- [ ] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `289 test(s), 0 failure(s)`.

- [ ] **Step 6: Commit**

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

- [ ] **Step 1: Write `src/Puppet.h`**

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

- [ ] **Step 2: Write `src/Puppet.cpp`**

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

- [ ] **Step 3: Edit `CMakeLists.txt` to add the source**

```cmake
	src/PlayerLocator.cpp
)
```
```cmake
	src/PlayerLocator.cpp
	src/Puppet.cpp
)
```

- [ ] **Step 4: Build and run the tests; they must pass**

The plugin compiles `Puppet`; no test can run it.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `289 test(s), 0 failure(s)`.

- [ ] **Step 5: Commit**

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

- [ ] **Step 1: Edit `src/Plugin.cpp`**

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

- [ ] **Step 2: Edit `CMakeLists.txt`: the version**

```cmake
project(BlockRender VERSION 0.10.0 LANGUAGES C CXX)
```
```cmake
project(BlockRender VERSION 0.11.0 LANGUAGES C CXX)
```

- [ ] **Step 3: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `289 test(s), 0 failure(s)`.

- [ ] **Step 4: Commit**

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

- [ ] **Step 1: Edit `tools/blockrender_hud/init.lua`**

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

- [ ] **Step 2: Check the overlay's syntax**

```bash
luac -p /d/Cyberpunk-Minecraft/spikes/block-render/tools/blockrender_hud/init.lua && echo "syntax ok"
```
Expected: `syntax ok`. (`luac` is Lua 5.4's; CET runs LuaJIT, and the overlay uses nothing that differs between them.)
Its patterns were also run on the two lines `StatusWritesTheDrive` expects, and gave `Drive: driving for 12.3 s,
V's camera 0.012 m from Minecraft's eye, V's health 100%` with the turn `on`, and `Drive: off, V's health unknown` with
the turn `off`.

- [ ] **Step 3: Commit**

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

- [ ] **Step 1: Deploy (the game must not be running)**

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

- [ ] **Step 2: In the game: load, link, and find the script calls**

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

- [ ] **Step 3: In the game: the opening check**

Ask the user to press F8 and to stand still in Minecraft. Expected: within a second, `build mode on`, `link: build
mode: Minecraft's player goes to V's feet, …` and `link: Minecraft let its player go after … s: V follows it`; the
overlay's `Drive: driving for …`, with V's camera within a few centimetres of Minecraft's eye.

Then ask the user to turn the mouse and look up and down, still standing, and to say whether the view moves as it does
outside build mode. If it lags, jumps back or won't turn, ask the user to click the overlay's `Turn: off` button, so
it reads `Turn: on`, and to try again. Record which works: it settles risk 1's first question.

- [ ] **Step 4: In the game: moving**

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

- [ ] **Step 5: In the game: entering with W held, and clicks while moving**

Ask the user to leave build mode (F8), hold W, press F8 while still holding it, then let go of W and press it again.
Expected: V stays where it is until the drive says `driving`, since the held W stays the game's and the drive holds
V; once W is pressed again, Minecraft's player walks, and V with it.

Then ask the user to place a row of stone on the street while walking: right clicks with W held, looking down a
little. Expected: every click places a block, and no red message shows in Minecraft's chat in the HUD.

- [ ] **Step 6: In the game: fall damage while placed**

Ask the user to make a save first, in case V dies. Then, in build mode, fly up about 5 m (Space twice, then Space
held) and stop flying (Space twice) to drop to the street; then the same from about 20 m. Expected: Minecraft's
creative player takes no damage. Record the drive line's health after each drop: whether V's own movement fights the
teleports (risk 2).

- [ ] **Step 7: In the game: leaving**

Ask the user to:
1. press F8 standing: V stays where Minecraft's player was, and the log says `build mode off: F8 turned it off`;
2. press F8 again, fly a few metres up, and press F8: V falls, as in the game, with the game's fall damage;
3. press F8, then Esc: `build mode off: Esc left it`, and the game's menu opens;
4. press F8 once more: build mode on again, and driving within a second.

After each, the status's teleport count grows again: the link's teleports have resumed from V's camera.

- [ ] **Step 8: In the game: a vehicle**

With build mode on, ask the user to walk up to a car and get in with F, which V still interacts with. Expected:
`build mode off: V is in a vehicle`, or `V is in a workspot`, since a car's seat is one. Then, in the car, F8 must say
`build mode stays off: …` with the same reason.

- [ ] **Step 9: In the game: the cost**

Ask the user to stand still for 10 s with build mode on, then 10 s with it off. Read the status's `frame=` line and
CET's frame time each time, and the drive line's teleport time (average and worst).

- [ ] **Step 10: Ask the user to quit the game and stop Minecraft**

- [ ] **Step 11: Record step 2's results in `docs/spikes/2026-10-03-collision-and-movement-results.md`**

A "Step 2" section: each check above with its outcome and numbers; which turn setting worked; what the drive log
showed; V's health after the drops; the cost; and anything that surprised, and what it means for Part 2.

- [ ] **Step 12: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-03-collision-and-movement-results.md \
  && git commit -q -m "collision-and-movement: step 2's results" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 18: Close Part 1, and plan Part 2

- [ ] **Step 1: Add "How Part 1 went" to this plan**

At the end, after "Found while planning": what ran as written, what changed, and what steps 1 and 2 found.

- [ ] **Step 2: Plan Part 2**

Use superpowers:writing-plans to add Part 2's tasks, from Task 19 on, to this plan: Night City's shape in Minecraft
(the spec's "Part 2" and step 3). Verify them as Part 1's were. Changes to the spec found while planning go in its
"Changes while planning".

- [ ] **Step 3: Commit**

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
