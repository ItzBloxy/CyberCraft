#include "Measurements.h"

#include <algorithm>

namespace cpmc
{
void LatencyTracker::Mark(double aNow)
{
	mark_ = aNow;
}

bool LatencyTracker::SectionArrived(double aNow, uint64_t aFrame)
{
	if (!mark_)
	{
		return false;
	}
	const double mark = *mark_;
	mark_.reset();
	if (aNow - mark > 1.0)
	{
		return false;
	}
	timedMark_ = mark;
	arrival_ = aNow;
	arrivalFrame_ = aFrame;
	return true;
}

std::optional<LatencySample> LatencyTracker::Drawn(double aNow, uint64_t aFrame)
{
	if (!timedMark_)
	{
		return std::nullopt;
	}
	const LatencySample sample{ (arrival_ - *timedMark_) * 1000.0, (aNow - arrival_) * 1000.0,
		                        aFrame >= arrivalFrame_ ? double(aFrame - arrivalFrame_) : 0.0 };
	timedMark_.reset();
	last_ = sample;
	sum_.minecraftMs += sample.minecraftMs;
	sum_.oursMs += sample.oursMs;
	sum_.oursFrames += sample.oursFrames;
	++samples_;
	return sample;
}

std::optional<LatencySample> LatencyTracker::Average() const
{
	if (samples_ == 0)
	{
		return std::nullopt;
	}
	const double count = double(samples_);
	return LatencySample{ sum_.minecraftMs / count, sum_.oursMs / count, sum_.oursFrames / count };
}

void BurstWindow::Start(double aNow, std::string aWhat)
{
	if (start_)
	{
		return;
	}
	start_ = aNow;
	report_ = BurstReport{ std::move(aWhat) };
}

std::optional<BurstReport> BurstWindow::Frame(double aNow, const BurstFrame& aFrame)
{
	if (!start_)
	{
		return std::nullopt;
	}
	if (aNow - *start_ > 1.0)
	{
		start_.reset();
		return report_;
	}
	++report_.frames;
	report_.sections += aFrame.sections;
	report_.bytes += aFrame.bytes;
	report_.drainMsWorst = std::max(report_.drainMsWorst, aFrame.drainMs);
	report_.updateMsWorst = std::max(report_.updateMsWorst, aFrame.updateMs);
	report_.frameMsWorst = std::max(report_.frameMsWorst, aFrame.frameMs);
	return std::nullopt;
}
}
