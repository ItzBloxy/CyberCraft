#include "world/BlockWorld.h"

namespace cpmc
{
namespace
{
size_t IndexOf(const BlockPos& aBlock)
{
	return size_t(aBlock.x & 15) + 16 * (size_t(aBlock.z & 15) + 16 * size_t(aBlock.y & 15));
}
}

SectionPos SectionOf(const BlockPos& aBlock)
{
	// An arithmetic shift is a floor division, negative coordinates included.
	return { aBlock.x >> 4, aBlock.y >> 4, aBlock.z >> 4 };
}

uint16_t BlockWorld::Get(const BlockPos& aBlock) const
{
	const Section* section = Find(SectionOf(aBlock));
	return section ? (*section)[IndexOf(aBlock)] : 0;
}

void BlockWorld::Set(const BlockPos& aBlock, uint16_t aId)
{
	const SectionPos at = SectionOf(aBlock);
	auto&            section = sections_[at];
	if (!section)
	{
		section = std::make_unique<Section>();
		section->fill(0);
	}
	uint16_t& block = (*section)[IndexOf(aBlock)];
	if (block == aId)
	{
		return;
	}
	blocks_ = blocks_ - (block != 0 ? 1 : 0) + (aId != 0 ? 1 : 0);
	block = aId;
	dirty_.insert(at);
	const int32_t local[3] = { aBlock.x & 15, aBlock.y & 15, aBlock.z & 15 };
	for (int axis = 0; axis < 3; ++axis)
	{
		for (const int32_t side : { -1, 1 })
		{
			if (local[axis] == (side < 0 ? 0 : 15))
			{
				SectionPos neighbour = at;
				(axis == 0 ? neighbour.x : axis == 1 ? neighbour.y : neighbour.z) += side;
				dirty_.insert(neighbour);
			}
		}
	}
}

void BlockWorld::Clear()
{
	for (const auto& [at, section] : sections_)
	{
		dirty_.insert(at);
	}
	sections_.clear();
	blocks_ = 0;
}

void BlockWorld::MarkAllDirty()
{
	for (const auto& [at, section] : sections_)
	{
		dirty_.insert(at);
	}
}

std::vector<SectionPos> BlockWorld::TakeDirty()
{
	std::vector<SectionPos> dirty(dirty_.begin(), dirty_.end());
	dirty_.clear();
	return dirty;
}

const BlockWorld::Section* BlockWorld::Find(const SectionPos& aSection) const
{
	const auto found = sections_.find(aSection);
	return found == sections_.end() ? nullptr : found->second.get();
}
}
