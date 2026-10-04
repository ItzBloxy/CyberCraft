#include "Harness.h"

#include "link/Ground.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <set>

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

// SkyCraft's distance from aCentre to aBatch's nearest region: height counts double.
int Nearest(const BatchPos& aBatch, const RegionPos& aCentre)
{
	int nearest = std::numeric_limits<int>::max();
	for (const RegionPos& region : RegionsOf(aBatch))
	{
		const int dx = region.x - aCentre.x, dy = region.y - aCentre.y, dz = region.z - aCentre.z;
		nearest = std::min(nearest, dx * dx + dz * dz + 2 * dy * dy);
	}
	return nearest;
}

// Reads every batch the areas offer at aNow, empty, as a source with nothing in the area would give them.
std::vector<BatchPos> ReadAll(GroundAreas& aAreas, double aNow)
{
	std::vector<BatchPos> read;
	while (const std::optional<BatchPos> batch = aAreas.Next(aNow))
	{
		aAreas.Read(*batch, aNow, {});
		read.push_back(*batch);
		if (read.size() > 1000)
		{
			break;  // it never ends: the test fails on the count
		}
	}
	return read;
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

TEST(GroundAreasReadEveryBatchOfTheAreaNearestFirst)
{
	GroundAreas areas;
	CHECK(!areas.Next(0.0));  // no centre yet
	const RegionPos centre{ -15, 0, 250 };
	areas.Centre(centre);
	CHECK_EQ(areas.Waiting(), size_t(144));
	const std::vector<BatchPos> read = ReadAll(areas, 0.0);
	// x and z: 11 regions over 6 batches; y: 6 regions over 4.
	CHECK_EQ(read.size(), size_t(6 * 6 * 4));
	CHECK(read.front() == BatchOf(centre));
	std::set<RegionPos> regions;
	for (const BatchPos& batch : read)
	{
		const std::array<RegionPos, 8> its = RegionsOf(batch);
		regions.insert(its.begin(), its.end());
	}
	for (int32_t dy = -3; dy <= 2; ++dy)
	{
		for (int32_t dz = -5; dz <= 5; ++dz)
		{
			for (int32_t dx = -5; dx <= 5; ++dx)
			{
				CHECK(regions.contains({ centre.x + dx, centre.y + dy, centre.z + dz }));
			}
		}
	}
	int last = -1;
	for (const BatchPos& batch : read)
	{
		CHECK(Nearest(batch, centre) >= last);
		last = Nearest(batch, centre);
	}
	CHECK_EQ(areas.Waiting(), size_t(0));
	CHECK_EQ(areas.Known(), size_t(144 * 8));
}

TEST(GroundAreasWaitUntilTheyreReady)
{
	GroundAreas areas;
	areas.Centre({ 0, 0, 0 });
	CHECK(areas.Next(0.0).has_value());
	areas.Reset(11.5);  // the link started at 10: the city's collision gets SkyCraft's 1.5 s to stream in
	areas.Reset(10.2);  // and anything else that starts it over meanwhile doesn't cut that short
	CHECK(!areas.Next(11.4));
	CHECK(areas.Next(11.5) == BatchOf({ 0, 0, 0 }));
}

TEST(GroundAreasSendARegionAgainOnlyWhenItChanges)
{
	GroundAreas areas;
	areas.Centre({ 0, 0, 0 });
	std::array<std::vector<McTriangle>, 8> filed;
	filed[0] = { Speck(4.0f, 4.0f, 4.0f) };
	CHECK_EQ(areas.Read({ 0, 0, 0 }, 0.0, filed).size(), size_t(8));  // the first time: every region, empty ones too
	CHECK(areas.Read({ 0, 0, 0 }, 1.0, filed).empty());
	filed[3] = { Speck(12.0f, 4.0f, 12.0f) };
	CHECK(areas.Read({ 0, 0, 0 }, 2.0, filed) == std::vector<size_t>({ 3 }));
	filed[0].clear();
	CHECK(areas.Read({ 0, 0, 0 }, 3.0, filed) == std::vector<size_t>({ 0 }));
}

TEST(GroundAreasReadTheBatchesNextToTheCentreEverySecond)
{
	GroundAreas     areas;
	const RegionPos centre{ -15, 0, 250 };
	areas.Centre(centre);
	ReadAll(areas, 10.0);
	CHECK(!areas.Next(10.9));
	const std::vector<BatchPos> again = ReadAll(areas, 11.0);
	// The batches holding SkyCraft's regions next to the centre: 1 each way across, and from 1 below to level.
	std::set<BatchPos> next;
	for (int32_t dy = -1; dy <= 0; ++dy)
	{
		for (int32_t dz = -1; dz <= 1; ++dz)
		{
			for (int32_t dx = -1; dx <= 1; ++dx)
			{
				next.insert(BatchOf({ centre.x + dx, centre.y + dy, centre.z + dz }));
			}
		}
	}
	CHECK_EQ(again.size(), size_t(8));
	CHECK_EQ(next.size(), size_t(8));
	for (const BatchPos& batch : again)
	{
		CHECK(next.contains(batch));
	}
}

TEST(GroundAreasForgetWhatsFarBehind)
{
	GroundAreas areas;
	areas.Centre({ 0, 0, 0 });
	ReadAll(areas, 0.0);
	// Seven regions east: the batches from region -6 to -1 are more than one past the area, and forgotten.
	areas.Centre({ 7, 0, 0 });
	CHECK_EQ(areas.Known(), size_t((144 - 72) * 8));
	// Back again: they're read again, and each of their regions is sent again.
	areas.Centre({ 0, 0, 0 });
	CHECK_EQ(areas.Waiting(), size_t(72));
	const std::optional<BatchPos> first = areas.Next(0.5);
	CHECK(first.has_value() && first->x < 0);
	CHECK_EQ(areas.Read(*first, 0.5, {}).size(), size_t(8));
	const std::vector<BatchPos> rest = ReadAll(areas, 0.5);
	CHECK_EQ(rest.size(), size_t(71));
	CHECK(std::all_of(rest.begin(), rest.end(), [](const BatchPos& aBatch) { return aBatch.x < 0; }));
}
