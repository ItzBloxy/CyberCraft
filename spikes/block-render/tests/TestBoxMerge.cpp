#include "Harness.h"

#include "physx/BoxMerge.h"

#include <array>

using namespace cpmc;
using namespace cpmc::px;

namespace
{
void Set(link::SolidBits& aBits, int aX, int aY, int aZ)
{
	const int bit = aX + 16 * aZ + 256 * aY;
	aBits[size_t(bit >> 6)] |= uint64_t(1) << (bit & 63);
}

bool Solid(const link::SolidBits& aBits, int aX, int aY, int aZ)
{
	const int bit = aX + 16 * aZ + 256 * aY;
	return (aBits[size_t(bit >> 6)] >> (bit & 63)) & 1;
}

// Whether aBoxes cover exactly aBits' solid blocks, each once.
bool CoverExactly(const std::vector<BlockBox>& aBoxes, const link::SolidBits& aBits)
{
	std::array<int, 4096> covered{};
	for (const BlockBox& box : aBoxes)
	{
		if (box.x0 >= box.x1 || box.y0 >= box.y1 || box.z0 >= box.z1 || box.x1 > 16 || box.y1 > 16 || box.z1 > 16)
		{
			return false;
		}
		for (int y = box.y0; y < box.y1; ++y)
		{
			for (int z = box.z0; z < box.z1; ++z)
			{
				for (int x = box.x0; x < box.x1; ++x)
				{
					++covered[size_t(x + 16 * z + 256 * y)];
				}
			}
		}
	}
	for (int y = 0; y < 16; ++y)
	{
		for (int z = 0; z < 16; ++z)
		{
			for (int x = 0; x < 16; ++x)
			{
				if (covered[size_t(x + 16 * z + 256 * y)] != (Solid(aBits, x, y, z) ? 1 : 0))
				{
					return false;
				}
			}
		}
	}
	return true;
}
}

TEST(BoxMergeMakesWholeLayersAndSectionsOneBox)
{
	link::SolidBits bits{};
	CHECK(MergeBoxes(bits).empty());
	for (int z = 0; z < 16; ++z)
	{
		for (int x = 0; x < 16; ++x)
		{
			Set(bits, x, 3, z);
		}
	}
	std::vector<BlockBox> boxes = MergeBoxes(bits);
	CHECK_EQ(boxes.size(), size_t(1));  // a 16 x 16 layer
	CHECK(boxes.size() == 1 && boxes[0].x0 == 0 && boxes[0].x1 == 16 && boxes[0].y0 == 3 && boxes[0].y1 == 4 && boxes[0].z0 == 0 &&
	      boxes[0].z1 == 16);
	bits.fill(~uint64_t(0));
	boxes = MergeBoxes(bits);
	CHECK(boxes.size() == 1 && boxes[0].y0 == 0 && boxes[0].y1 == 16);  // the whole section
}

TEST(BoxMergeGoesAlongXThenZThenY)
{
	// A wall 4 blocks along x, 3 high, and a tower 2 x 2 x 5 beside it: two boxes.
	link::SolidBits bits{};
	for (int y = 0; y < 3; ++y)
	{
		for (int x = 0; x < 4; ++x)
		{
			Set(bits, x, y, 0);
		}
	}
	for (int y = 0; y < 5; ++y)
	{
		for (int z = 3; z < 5; ++z)
		{
			for (int x = 7; x < 9; ++x)
			{
				Set(bits, x, y, z);
			}
		}
	}
	const std::vector<BlockBox> boxes = MergeBoxes(bits);
	CHECK_EQ(boxes.size(), size_t(2));
	CHECK(CoverExactly(boxes, bits));
	// An L along x then z: the first row goes as far along x as it can, so the L is two boxes.
	link::SolidBits l{};
	for (int x = 0; x < 4; ++x)
	{
		Set(l, x, 0, 0);
	}
	Set(l, 0, 0, 1);
	Set(l, 0, 0, 2);
	const std::vector<BlockBox> lBoxes = MergeBoxes(l);
	CHECK_EQ(lBoxes.size(), size_t(2));
	CHECK(lBoxes.size() == 2 && lBoxes[0].x1 == 4 && lBoxes[0].z1 == 1 && lBoxes[1].z0 == 1 && lBoxes[1].z1 == 3);
}

TEST(BoxMergeCoversAnyBlocksExactly)
{
	// A checkerboard, where no two solid blocks touch: a box each.
	link::SolidBits checker{};
	for (int y = 0; y < 16; ++y)
	{
		for (int z = 0; z < 16; ++z)
		{
			for (int x = 0; x < 16; ++x)
			{
				if ((x + y + z) % 2 == 0)
				{
					Set(checker, x, y, z);
				}
			}
		}
	}
	const std::vector<BlockBox> boxes = MergeBoxes(checker);
	CHECK_EQ(boxes.size(), size_t(2048));
	CHECK(CoverExactly(boxes, checker));
	// Random blocks, from sparse to dense.
	uint64_t seed = 12345;
	for (int round = 0; round < 40; ++round)
	{
		link::SolidBits bits{};
		for (uint64_t& word : bits)
		{
			for (int i = 0; i < 64; ++i)
			{
				seed = seed * 6364136223846793005ull + 1442695040888963407ull;
				if (int((seed >> 33) % 40) < round)
				{
					word |= uint64_t(1) << i;
				}
			}
		}
		CHECK(CoverExactly(MergeBoxes(bits), bits));
	}
}
