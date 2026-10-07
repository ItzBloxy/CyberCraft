#include "input/Cursor.h"

#include <algorithm>
#include <cmath>

namespace cpmc::input
{
double PointerFactor(int aSpeed)
{
	if (aSpeed < 1 || aSpeed > 20)
	{
		return 1.0;
	}
	if (aSpeed <= 2)
	{
		return aSpeed == 1 ? 1.0 / 32.0 : 1.0 / 16.0;
	}
	if (aSpeed <= 10)
	{
		return (aSpeed - 2) / 8.0;
	}
	return 1.0 + (aSpeed - 10) / 4.0;
}

void Cursor::SetArea(uint32_t aImageWidth, uint32_t aImageHeight, uint32_t aScale)
{
	width_ = aImageWidth;
	height_ = aImageHeight;
	scale_ = std::max(aScale, 1u);
	Clamp();
}

void Cursor::SetScreen(bool aOpen)
{
	if (aOpen && !screen_ && HasArea())
	{
		x_ = double(width_ / 2) * scale_;
		y_ = double(height_ / 2) * scale_;
	}
	screen_ = aOpen;
}

std::optional<Pixel> Cursor::Move(int32_t aDx, int32_t aDy, double aFactor)
{
	if (!HasArea())
	{
		return std::nullopt;
	}
	const Pixel before = Image();
	x_ += aDx * aFactor;
	y_ += aDy * aFactor;
	Clamp();
	const Pixel after = Image();
	if (after == before)
	{
		return std::nullopt;
	}
	return after;
}

Pixel Cursor::Image() const
{
	if (!HasArea())
	{
		return {};
	}
	return { std::min(uint32_t(x_ / scale_), width_ - 1), std::min(uint32_t(y_ / scale_), height_ - 1) };
}

Pixel Cursor::Back() const
{
	return { uint32_t(x_), uint32_t(y_) };
}

void Cursor::Clamp()
{
	if (!HasArea())
	{
		x_ = 0.0;
		y_ = 0.0;
		return;
	}
	// Inside the area's last pixel, never on its far edge.
	x_ = std::clamp(x_, 0.0, std::nextafter(double(width_) * scale_, 0.0));
	y_ = std::clamp(y_, 0.0, std::nextafter(double(height_) * scale_, 0.0));
}
}
