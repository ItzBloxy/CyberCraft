#pragma once

#include <cstdint>
#include <optional>
#include <vector>

// mclink's hud-box (spike 8's step 0): the mean colour of a box of Minecraft's HUD image, to tell from the log whether
// Minecraft drew a slot's hover highlight where the cursor was sent.
namespace cpmc::mclink
{
struct BoxMean
{
	double   r = 0.0;  // each channel's mean, 0 to 255, as stored (premultiplied by alpha)
	double   g = 0.0;
	double   b = 0.0;
	double   a = 0.0;
	uint64_t pixels = 0;
};

// The mean of each channel over the box from (aX0, aY0) to (aX1, aY1), the far edges left out, counted in pixels from
// the image's top left, of an RGBA8 image aWidth by aHeight whose rows are stored bottom row first if aBottomUp. Nothing
// if the box is empty or not all inside the image, or aRgba isn't aWidth * aHeight * 4 bytes.
std::optional<BoxMean> MeanOfBox(const std::vector<uint8_t>& aRgba, uint32_t aWidth, uint32_t aHeight, bool aBottomUp, uint32_t aX0,
                                 uint32_t aY0, uint32_t aX1, uint32_t aY1);
}
