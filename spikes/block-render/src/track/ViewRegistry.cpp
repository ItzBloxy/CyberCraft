#include "track/ViewRegistry.h"

#include <algorithm>
#include <mutex>

namespace cpmc
{
std::vector<DescriptorRun> CopyRuns(uint32_t aDestRanges, const uint64_t* aDestStarts, const uint32_t* aDestSizes,
                                    uint32_t aSourceRanges, const uint64_t* aSourceStarts, const uint32_t* aSourceSizes,
                                    uint32_t aIncrement)
{
	std::vector<DescriptorRun> runs;
	if (!aDestStarts || !aSourceStarts)
	{
		return runs;
	}
	uint32_t destRange = 0, destOffset = 0;
	uint32_t sourceRange = 0, sourceOffset = 0;
	while (destRange < aDestRanges && sourceRange < aSourceRanges)
	{
		const uint32_t destSize = aDestSizes ? aDestSizes[destRange] : 1;
		const uint32_t sourceSize = aSourceSizes ? aSourceSizes[sourceRange] : 1;
		if (destOffset >= destSize)
		{
			++destRange;
			destOffset = 0;
			continue;
		}
		if (sourceOffset >= sourceSize)
		{
			++sourceRange;
			sourceOffset = 0;
			continue;
		}
		const uint32_t count = std::min(destSize - destOffset, sourceSize - sourceOffset);
		runs.push_back({ aDestStarts[destRange] + uint64_t(destOffset) * aIncrement,
		                 aSourceStarts[sourceRange] + uint64_t(sourceOffset) * aIncrement, count });
		destOffset += count;
		sourceOffset += count;
	}
	return runs;
}

void ViewRegistry::SetIncrements(uint32_t aRtv, uint32_t aDsv)
{
	std::unique_lock lock(mutex_);
	rtvIncrement_ = aRtv;
	dsvIncrement_ = aDsv;
}

uint32_t ViewRegistry::Increment(ViewInfo::Kind aKind) const
{
	std::shared_lock lock(mutex_);
	return aKind == ViewInfo::Kind::Rtv ? rtvIncrement_ : dsvIncrement_;
}

void ViewRegistry::Record(uint64_t aHandle, const ViewInfo& aView)
{
	std::unique_lock lock(mutex_);
	views_[aHandle] = aView;
}

void ViewRegistry::Copy(ViewInfo::Kind aKind, uint64_t aDest, uint64_t aSource, uint32_t aCount)
{
	std::unique_lock lock(mutex_);
	const uint64_t   step = aKind == ViewInfo::Kind::Rtv ? rtvIncrement_ : dsvIncrement_;
	if (step == 0)
	{
		return;
	}
	// Read every source first: the ranges may overlap.
	std::vector<std::optional<ViewInfo>> copied(aCount);
	for (uint32_t i = 0; i < aCount; ++i)
	{
		const auto found = views_.find(aSource + i * step);
		if (found != views_.end() && found->second.kind == aKind)
		{
			copied[i] = found->second;
		}
	}
	for (uint32_t i = 0; i < aCount; ++i)
	{
		if (copied[i])
		{
			views_[aDest + i * step] = *copied[i];
		}
		else
		{
			views_.erase(aDest + i * step);
		}
	}
}

std::optional<ViewInfo> ViewRegistry::Find(uint64_t aHandle) const
{
	std::shared_lock lock(mutex_);
	const auto       found = views_.find(aHandle);
	if (found == views_.end())
	{
		return std::nullopt;
	}
	return found->second;
}

size_t ViewRegistry::Count(ViewInfo::Kind aKind) const
{
	std::shared_lock lock(mutex_);
	return size_t(std::count_if(views_.begin(), views_.end(), [aKind](const auto& aEntry) { return aEntry.second.kind == aKind; }));
}
}
