#include "render/Hud.h"

#include <algorithm>
#include <cmath>

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

uint32_t CursorHeight(uint32_t aBackHeight)
{
	return std::max(1u, uint32_t(std::lround(18.0 * aBackHeight / 1080.0)));
}

CursorPart CursorAt(uint32_t aX, uint32_t aY, uint32_t aTipX, uint32_t aTipY, uint32_t aHeight)
{
	if (aHeight == 0)
	{
		return CursorPart::None;
	}
	const float height = float(aHeight);
	const float edge = 1.5f * height / 18.0f;
	const float x = float(aX) + 0.5f - float(aTipX);
	const float y = float(aY) + 0.5f - float(aTipY);
	if (x < 0.0f || y < 0.0f || y >= height || x > y * 0.6f)
	{
		return CursorPart::None;
	}
	return x < edge || x > y * 0.6f - edge || y > height - edge ? CursorPart::Edge : CursorPart::Fill;
}

void HudCursor::Set(const Spot& aSpot)
{
	packed_.store(uint64_t(aSpot.x & 0x7FFFFFFFu) | (uint64_t(aSpot.y & 0x7FFFFFFFu) << 31) | (aSpot.shown ? 1ull << 62 : 0ull));
}

HudCursor::Spot HudCursor::Get() const
{
	const uint64_t packed = packed_.load();
	return { uint32_t(packed & 0x7FFFFFFFu), uint32_t((packed >> 31) & 0x7FFFFFFFu), (packed >> 62) != 0 };
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
