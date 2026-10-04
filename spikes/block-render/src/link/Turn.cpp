#include "link/Turn.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <vector>

namespace cpmc::link
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

std::optional<double> TurnRatio::Measured() const
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
{
	return Ratio().value_or(0.0) * double(aCounts);
}
}
