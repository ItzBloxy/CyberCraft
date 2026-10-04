#include "Harness.h"

#include "link/Ground.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

using namespace cpmc;
using namespace cpmc::link;

namespace
{
McTriangle Tri(float aX0, float aY0, float aZ0, float aX1, float aY1, float aZ1, float aX2, float aY2, float aZ2)
{
	return { { aX0, aY0, aZ0, aX1, aY1, aZ1, aX2, aY2, aZ2 } };
}

// A small flat triangle a quarter block across, around (aX, aY, aZ).
McTriangle Speck(float aX, float aY, float aZ)
{
	return Tri(aX - 0.125f, aY, aZ - 0.125f, aX + 0.125f, aY, aZ - 0.125f, aX, aY, aZ + 0.125f);
}

bool Same(const McTriangle& aA, const McTriangle& aB)
{
	return std::equal(std::begin(aA.v), std::end(aA.v), std::begin(aB.v));
}

RegionPos Region(int32_t aX, int32_t aY, int32_t aZ)
{
	return { aX, aY, aZ };
}

BatchPos Batch(int32_t aX, int32_t aY, int32_t aZ)
{
	return { aX, aY, aZ };
}
}

TEST(GroundFindsRegionsAndBatches)
{
	// Spike 3's corner, Minecraft -119.5 6.14 2007.5, is in region -15 0 250, of batch -8 0 125.
	CHECK(RegionAt({ -119.5, 6.14, 2007.5 }) == Region(-15, 0, 250));
	CHECK(RegionAt({ -0.01, -0.01, 8.0 }) == Region(-1, -1, 1));
	CHECK(!RegionAt({ std::nan(""), 0.0, 0.0 }));
	CHECK(!RegionAt({ 0.0, 2.0e6, 0.0 }));
	CHECK(BatchOf({ -15, 0, 250 }) == Batch(-8, 0, 125));
	CHECK(BatchOf({ -16, -1, 251 }) == Batch(-8, -1, 125));
	const std::array<RegionPos, 8> regions = RegionsOf({ -8, 0, 125 });
	CHECK(regions[0] == Region(-16, 0, 250));
	CHECK(regions[1] == Region(-15, 0, 250));  // x first
	CHECK(regions[2] == Region(-16, 0, 251));  // then z
	CHECK(regions[4] == Region(-16, 1, 250));  // then y
	CHECK(regions[7] == Region(-15, 1, 251));
	const McBox box = BatchBox({ -8, 0, 125 });  // a quarter block more each way
	CHECK_NEAR(box.low.x, -128.25, 1e-12);
	CHECK_NEAR(box.high.x, -111.75, 1e-12);
	CHECK_NEAR(box.low.y, -0.25, 1e-12);
	CHECK_NEAR(box.high.y, 16.25, 1e-12);
	CHECK_NEAR(box.low.z, 1999.75, 1e-12);
	CHECK_NEAR(box.high.z, 2016.25, 1e-12);
}

TEST(GroundFilesATriangleUnderEveryRegionItTouches)
{
	// Batch 0 0 0 is blocks 0 to 16 each way. Its regions, in order: 000, 100, 001, 101, 010, 110, 011, 111.
	const std::vector<McTriangle> triangles = {
		Speck(4.0f, 4.0f, 4.0f),                                     // inside region 000
		Tri(7.0f, 4.0f, 4.0f, 9.0f, 4.0f, 4.0f, 8.0f, 4.0f, 5.0f),  // across x = 8: regions 000 and 100
		Speck(8.3f, 12.0f, 12.0f),                                   // in region 111, and within half a block of 011
		Speck(16.5f, 12.0f, 12.0f),                                  // past the batch, within half a block of 111
		Speck(17.2f, 12.0f, 12.0f),                                  // farther: in none
	};
	const auto filed = FileBatch({ 0, 0, 0 }, triangles);
	CHECK(filed[0].size() == 2 && Same(filed[0][0], triangles[0]) && Same(filed[0][1], triangles[1]));
	CHECK(filed[1].size() == 1 && Same(filed[1][0], triangles[1]));
	CHECK(filed[6].size() == 1 && Same(filed[6][0], triangles[2]));
	CHECK(filed[7].size() == 2 && Same(filed[7][0], triangles[2]) && Same(filed[7][1], triangles[3]));
	for (const size_t i : { 2, 3, 4, 5 })
	{
		CHECK(filed[i].empty());
	}
}

TEST(GroundLeavesOutTrianglesThatArentUsable)
{
	McTriangle broken = Speck(4.0f, 4.0f, 4.0f);
	broken.v[4] = std::numeric_limits<float>::quiet_NaN();
	McTriangle distant = Speck(4.0f, 4.0f, 4.0f);
	distant.v[0] = 2.0e6f;  // its bounds still touch region 000
	const auto filed = FileBatch({ 0, 0, 0 }, std::vector<McTriangle>{ broken, distant, Speck(4.0f, 4.0f, 4.0f) });
	CHECK_EQ(filed[0].size(), size_t(1));
}

TEST(GroundHashesTrianglesWhateverTheirOrder)
{
	const std::vector<McTriangle> triangles = { Speck(1.0f, 2.0f, 3.0f), Speck(4.0f, 5.0f, 6.0f), Speck(7.0f, 8.0f, 9.0f) };
	const std::vector<McTriangle> reordered = { triangles[2], triangles[0], triangles[1] };
	CHECK_EQ(TrianglesHash(triangles), TrianglesHash(reordered));
	std::vector<McTriangle> moved = triangles;
	moved[1].v[3] += 0.01f;
	CHECK(TrianglesHash(moved) != TrianglesHash(triangles));
	CHECK(TrianglesHash(std::vector<McTriangle>{ triangles[0], triangles[0] }) != TrianglesHash(std::vector<McTriangle>{ triangles[0] }));
	CHECK(TrianglesHash({}) != TrianglesHash(std::vector<McTriangle>{ triangles[0] }));
}
