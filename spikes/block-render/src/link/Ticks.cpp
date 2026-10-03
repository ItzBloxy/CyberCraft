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
