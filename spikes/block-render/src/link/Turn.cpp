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
