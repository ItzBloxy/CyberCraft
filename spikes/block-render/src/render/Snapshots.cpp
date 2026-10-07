#include "render/Snapshots.h"

#include <algorithm>

namespace cpmc
{
Snapshots::Snapshots(uint32_t aCount)
	: holds_(aCount)
{
}

std::optional<uint32_t> Snapshots::Writable(uint64_t aCompleted) const
{
	for (uint32_t index = 0; index < holds_.size(); ++index)
	{
		const Hold& hold = holds_[index];
		if (index != newest_ && hold.onTheirWay == 0 && hold.fence <= aCompleted)
		{
			return index;
		}
	}
	return std::nullopt;
}

void Snapshots::Written(uint32_t aIndex)
{
	if (aIndex < holds_.size())
	{
		newest_ = aIndex;
	}
}

std::optional<uint32_t> Snapshots::TakeNewest()
{
	if (newest_)
	{
		++holds_[*newest_].onTheirWay;
	}
	return newest_;
}

void Snapshots::Submitted(uint32_t aIndex, uint64_t aFence)
{
	if (aIndex < holds_.size() && holds_[aIndex].onTheirWay > 0)
	{
		--holds_[aIndex].onTheirWay;
		holds_[aIndex].fence = std::max(holds_[aIndex].fence, aFence);
	}
}

void Snapshots::Abandon(uint32_t aIndex)
{
	if (aIndex < holds_.size() && holds_[aIndex].onTheirWay > 0)
	{
		--holds_[aIndex].onTheirWay;
	}
}
}
