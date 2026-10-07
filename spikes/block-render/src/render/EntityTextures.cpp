#include "render/EntityTextures.h"

#include <algorithm>

namespace cpmc
{
std::optional<EntityTextures::Place> EntityTextures::Add(uint32_t aId, uint32_t aWidth, uint32_t aHeight)
{
	const uint64_t texels = uint64_t(aWidth) * aHeight;
	if (texels == 0 || used_ + texels > kCapTexels)
	{
		return std::nullopt;
	}
	const Place place{ uint32_t(used_), aWidth, aHeight };
	places_[aId] = place;
	used_ += texels;
	return place;
}

const EntityTextures::Place* EntityTextures::Find(uint32_t aId) const
{
	const auto found = places_.find(aId);
	return found != places_.end() ? &found->second : nullptr;
}

void EntityTextures::Clear()
{
	places_.clear();
	used_ = 0;
}

uint64_t EntityTextures::Capacity(uint64_t aCurrent) const
{
	if (used_ <= aCurrent)
	{
		return aCurrent;
	}
	uint64_t capacity = std::max(aCurrent, kFirstTexels);
	while (capacity < used_)
	{
		capacity *= 2;
	}
	return std::min(capacity, kCapTexels);
}
}
