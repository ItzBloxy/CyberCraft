#include "input/Text.h"

namespace cpmc::input
{
bool Printable(uint32_t aPoint)
{
	return aPoint >= 0x20 && aPoint != 0x7F;
}

std::optional<uint32_t> Utf16Text::Take(uint16_t aUnit)
{
	const bool first = aUnit >= 0xD800 && aUnit <= 0xDBFF;
	const bool second = aUnit >= 0xDC00 && aUnit <= 0xDFFF;
	if (high_ && second)
	{
		const uint32_t point = 0x10000 + ((uint32_t(*high_) - 0xD800) << 10) + (aUnit - 0xDC00u);
		high_.reset();
		return point;
	}
	if (high_)
	{
		++refused_;  // a first half with no second
		high_.reset();
	}
	if (first)
	{
		high_ = aUnit;
		return std::nullopt;
	}
	if (second)
	{
		++refused_;
		return std::nullopt;
	}
	return aUnit;
}
}
