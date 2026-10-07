#include "Harness.h"

#include "HudBox.h"

using namespace cpmc::mclink;

namespace
{
// A 3 x 2 image: each pixel's red is its x, green its y counted from the top, blue 10, alpha 255. As stored top row
// first, or bottom row first.
std::vector<uint8_t> Image(bool aBottomUp)
{
	std::vector<uint8_t> rgba;
	for (uint32_t row = 0; row < 2; ++row)
	{
		const uint32_t y = aBottomUp ? 1 - row : row;
		for (uint32_t x = 0; x < 3; ++x)
		{
			rgba.insert(rgba.end(), { uint8_t(x), uint8_t(y), 10, 255 });
		}
	}
	return rgba;
}
}

TEST(MeanOfBoxCountsFromTheTopLeft)
{
	for (const bool bottomUp : { false, true })
	{
		const auto top = MeanOfBox(Image(bottomUp), 3, 2, bottomUp, 1, 0, 3, 1);  // x 1 and 2 of the top row
		CHECK(top.has_value());
		if (top)
		{
			CHECK_EQ(top->r, 1.5);
			CHECK_EQ(top->g, 0.0);
			CHECK_EQ(top->b, 10.0);
			CHECK_EQ(top->a, 255.0);
			CHECK_EQ(top->pixels, uint64_t(2));
		}
		const auto all = MeanOfBox(Image(bottomUp), 3, 2, bottomUp, 0, 0, 3, 2);
		CHECK(all.has_value());
		if (all)
		{
			CHECK_EQ(all->r, 1.0);
			CHECK_EQ(all->g, 0.5);
			CHECK_EQ(all->pixels, uint64_t(6));
		}
		const auto bottom = MeanOfBox(Image(bottomUp), 3, 2, bottomUp, 0, 1, 1, 2);  // the bottom left pixel
		CHECK(bottom.has_value() && bottom->r == 0.0 && bottom->g == 1.0);
	}
}

TEST(MeanOfBoxRefusesWhatIsOutside)
{
	const std::vector<uint8_t> image = Image(false);
	CHECK(!MeanOfBox(image, 3, 2, false, 0, 0, 4, 2).has_value());  // past the right edge
	CHECK(!MeanOfBox(image, 3, 2, false, 0, 0, 3, 3).has_value());  // past the bottom
	CHECK(!MeanOfBox(image, 3, 2, false, 2, 0, 2, 2).has_value());  // no width
	CHECK(!MeanOfBox(image, 3, 2, false, 0, 1, 3, 1).has_value());  // no height
	CHECK(!MeanOfBox(image, 4, 2, false, 0, 0, 1, 1).has_value());  // the bytes aren't a 4 x 2 image
	CHECK(!MeanOfBox({}, 0, 0, false, 0, 0, 1, 1).has_value());
}
