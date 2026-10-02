#pragma once

#include <cstdint>
#include <optional>

// The reader's side of SkyCraft's overlay triple buffer, where Minecraft publishes its HUD image (the spec's "The
// protocol"), as SkyCraft's Skyrim side reads it (skse/src/Link.cpp, MIT). Minecraft draws into its own back slot,
// then swaps it into the middle with the unread bit set. The reader swaps its front slot into the middle when the bit
// is set, and takes the slot it gets back, which Minecraft won't touch until it's swapped out again.
namespace cpmc::link
{
struct HudFrame
{
	uint32_t       width = 0;
	uint32_t       height = 0;
	bool           bottomUp = false;
	uint64_t       frameId = 0;
	const uint8_t* pixels = nullptr;  // width * height * 4 bytes, RGBA8 with premultiplied alpha; ours until the next Acquire
};

class OverlayReader
{
public:
	// aControl: the OverlayCtl; aHeaders: the 3 slot headers; aPixels: slot 0's pixels; aSlotBytes: bytes per slot.
	OverlayReader(uint8_t* aControl, uint8_t* aHeaders, uint8_t* aPixels, uint64_t aSlotBytes);

	// For a new Minecraft process: the state to 0, and our front to slot 2. Its writer starts over at slot 1.
	void Reset();

	// The newest image, if Minecraft has published one since the last call. A slot index of 3, or a size out of range
	// (1 to 3840 by 2160, within its slot), is refused and counted.
	std::optional<HudFrame> Acquire();

	uint64_t Refused() const { return refused_; }

private:
	uint8_t* control_;
	uint8_t* headers_;
	uint8_t* pixels_;
	uint64_t slotBytes_;
	uint32_t front_ = 2;
	uint64_t refused_ = 0;
};
}
