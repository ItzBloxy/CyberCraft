#include "HudBox.h"

namespace cpmc::mclink
{
std::optional<BoxMean> MeanOfBox(const std::vector<uint8_t>& aRgba, uint32_t aWidth, uint32_t aHeight, bool aBottomUp, uint32_t aX0,
                                 uint32_t aY0, uint32_t aX1, uint32_t aY1)
{
	if (aRgba.size() != uint64_t(aWidth) * aHeight * 4 || aX0 >= aX1 || aY0 >= aY1 || aX1 > aWidth || aY1 > aHeight)
	{
		return std::nullopt;
	}
	uint64_t sums[4]{};
	for (uint32_t y = aY0; y < aY1; ++y)
	{
		const uint32_t row = aBottomUp ? aHeight - 1 - y : y;
		for (uint32_t x = aX0; x < aX1; ++x)
		{
			const uint8_t* pixel = aRgba.data() + (uint64_t(row) * aWidth + x) * 4;
			for (int c = 0; c < 4; ++c)
			{
				sums[c] += pixel[c];
			}
		}
	}
	BoxMean mean;
	mean.pixels = uint64_t(aX1 - aX0) * (aY1 - aY0);
	const double count = double(mean.pixels);
	mean.r = double(sums[0]) / count;
	mean.g = double(sums[1]) / count;
	mean.b = double(sums[2]) / count;
	mean.a = double(sums[3]) / count;
	return mean;
}
}
