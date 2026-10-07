#pragma once

#include <cstdint>
#include <optional>

// The cursor BlockRender keeps while a Minecraft screen is open in build mode (spike 8's "The cursor"): moved by the
// mouse's raw counts at Windows' pointer speed, kept in back-buffer pixels with their fractions, clamped to the area the
// HUD image covers, and given to Minecraft in the image's pixels, which are its window's. Pure, and not thread-safe: the
// router holds its lock around every call.
namespace cpmc::input
{
// Windows' pointer speed (SPI_GETMOUSESPEED, 1 to 20, 10 by default), and whether "Enhance pointer precision" is on.
struct PointerSpeed
{
	int  speed = 10;
	bool enhanced = false;
};

// Back-buffer pixels a raw count, by Windows' table with "Enhance pointer precision" off: 1/32 and 1/16 at 1 and 2, then
// eighths up to 1 at 10, then quarters up to 3.5 at 20. Its curve, when it's on, isn't followed. A speed out of range
// counts as the default's.
double PointerFactor(int aSpeed);

// A pixel, whole, from the top left.
struct Pixel
{
	uint32_t x = 0;
	uint32_t y = 0;

	bool operator==(const Pixel&) const = default;
};

class Cursor
{
public:
	// The HUD image's size, and the back-buffer pixels each of its texels covers each way: the area the cursor stays in.
	// The position is clamped into a new area. A size of 0 leaves no area, and the cursor nowhere; a scale of 0 counts
	// as 1.
	void SetArea(uint32_t aImageWidth, uint32_t aImageHeight, uint32_t aScale);
	// Minecraft's screen flag, each frame. Going from clear to set puts the cursor at the image's centre, as Minecraft's
	// own MouseHandler.releaseMouse puts its position as a screen opens; from one screen straight to another it stays.
	void SetScreen(bool aOpen);
	// A mouse event's raw counts, at aFactor back-buffer pixels a count: the image pixel it moved to, if that changed.
	std::optional<Pixel> Move(int32_t aDx, int32_t aDy, double aFactor);

	bool     HasArea() const { return width_ > 0 && height_ > 0; }
	Pixel    Image() const;  // in the image's pixels, for Minecraft
	Pixel    Back() const;   // in back-buffer pixels, for the arrow
	uint32_t ImageWidth() const { return width_; }
	uint32_t ImageHeight() const { return height_; }

private:
	void Clamp();

	uint32_t width_ = 0;  // the image's
	uint32_t height_ = 0;
	uint32_t scale_ = 1;
	double   x_ = 0.0;  // back-buffer pixels
	double   y_ = 0.0;
	bool     screen_ = false;
};
}
