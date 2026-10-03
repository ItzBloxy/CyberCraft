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
