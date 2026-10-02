#include "render/Hud.h"

#include <algorithm>

namespace cpmc
{
bool HudWanted(int aSetting, bool aBuildMode, bool aMinecraftInWorld)
{
	return aMinecraftInWorld && (aSetting == 2 || (aSetting == 1 && aBuildMode));
}

HudSize HudViewport(uint32_t aBackWidth, uint32_t aBackHeight, uint32_t aScale)
{
	if (aBackWidth == 0 || aBackHeight == 0 || (aScale != 1 && aScale != 2 && aScale != 4))
	{
		return {};
	}
	return { std::clamp(aBackWidth / aScale, 1u, 3840u), std::clamp(aBackHeight / aScale, 1u, 2160u) };
}

uint32_t HudScaleFor(uint32_t aBackWidth, uint32_t aBackHeight, uint32_t aWidth, uint32_t aHeight)
{
	if (aWidth == 0 || aHeight == 0)
	{
		return 1;
	}
	return std::max(1u, std::min(aBackWidth / aWidth, aBackHeight / aHeight));
}

std::optional<uint32_t> HudTexel(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight, uint32_t aScale, bool aBottomUp)
{
	if (aScale == 0)
	{
		return std::nullopt;
	}
	const uint32_t x = aX / aScale;
	const uint32_t y = aY / aScale;
	if (x >= aWidth || y >= aHeight)
	{
		return std::nullopt;
	}
	return (aBottomUp ? aHeight - 1 - y : y) * aWidth + x;
}

std::optional<uint32_t> HudSlots::BeginWrite(uint64_t aCompleted)
{
	std::optional<uint32_t> newest;
	for (uint32_t slot = 0; slot < kSlots; ++slot)
	{
		if (state_[slot] == State::Reading && fence_[slot] <= aCompleted)
		{
			state_[slot] = State::Free;
		}
		if (state_[slot] == State::Free)
		{
			state_[slot] = State::Writing;
			return slot;
		}
		if (state_[slot] == State::Newest)
		{
			newest = slot;
		}
	}
	if (newest)
	{
		state_[*newest] = State::Writing;
	}
	return newest;
}

void HudSlots::EndWrite(uint32_t aSlot)
{
	for (State& state : state_)
	{
		if (state == State::Newest)
		{
			state = State::Free;  // never taken
		}
	}
	state_[aSlot] = State::Newest;
}

void HudSlots::Abandon(uint32_t aSlot)
{
	state_[aSlot] = State::Free;
}

std::optional<uint32_t> HudSlots::TakeNewest()
{
	for (uint32_t slot = 0; slot < kSlots; ++slot)
	{
		if (state_[slot] == State::Newest)
		{
			state_[slot] = State::Reading;
			fence_[slot] = UINT64_MAX;  // until Reading says when the GPU is done with it
			return slot;
		}
	}
	return std::nullopt;
}

void HudSlots::Reading(uint32_t aSlot, uint64_t aFence)
{
	fence_[aSlot] = aFence;
}
}
