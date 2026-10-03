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
