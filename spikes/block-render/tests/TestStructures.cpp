#include "Harness.h"

#include "AtlasFixture.h"
#include "world/Structures.h"

using cpmc::BlockPos;
using cpmc::BlockWorld;

TEST(StructuresGoThreeBlocksAheadInTheLayerOfVsFeet)
{
	// V at (10.4, 20.2, 7.9) facing north (+y): 3 m north is world (10.4, 23.2, 7.9), Minecraft (10.4, 7.9, -23.2).
	CHECK(cpmc::PlacementOrigin({ 10.4, 20.2, 7.9 }, { 0.0f, 1.0f, 0.0f }) == (BlockPos{ 10, 7, -24 }));
	// Facing east (+x) and a little down: only the horizontal direction counts.
	CHECK(cpmc::PlacementOrigin({ 0.5, 0.5, -0.5 }, { 0.8f, 0.0f, -0.6f }) == (BlockPos{ 3, -1, -1 }));
	// Looking straight down: north.
	CHECK(cpmc::PlacementOrigin({ 0.5, 0.5, 0.0 }, { 0.0f, 0.0f, -1.0f }) == (BlockPos{ 0, 0, -4 }));
}

TEST(StructuresPlaceACube)
{
	const cpmc::Atlas atlas = fixture::Atlas();
	BlockWorld        world;
	std::string       error;
	const auto        placed = cpmc::PlaceStructure(world, atlas, "cube", { 1, 2, 3 }, error);
	CHECK(placed.has_value());
	CHECK_EQ(placed->blocks, size_t(1));
	CHECK_EQ(world.BlockCount(), size_t(1));
	CHECK_EQ(world.Get({ 1, 2, 3 }), *atlas.BlockId("stone"));
}

TEST(StructuresPlaceASamplerOfEveryBlock)
{
	const cpmc::Atlas atlas = fixture::Atlas();
	BlockWorld        world;
	std::string       error;
	const auto        placed = cpmc::PlaceStructure(world, atlas, "sampler", { 0, 10, 0 }, error);
	CHECK(placed.has_value());
	CHECK_EQ(placed->blocks, size_t(15));  // 5 blocks on 10 base blocks
	CHECK(placed->max == (BlockPos{ 9, 11, 0 }));
	CHECK_EQ(world.Get({ 0, 11, 0 }), uint16_t(1));
	CHECK_EQ(world.Get({ 8, 11, 0 }), uint16_t(5));
	CHECK_EQ(world.Get({ 9, 11, 0 }), uint16_t(0));  // the gap
	CHECK_EQ(world.Get({ 9, 10, 0 }), *atlas.BlockId("stone"));
}

TEST(StructuresKnowTheirWorldBox)
{
	cpmc::Placed placed;
	placed.min = { 10, 7, -24 };
	placed.max = { 11, 8, -23 };
	const auto [low, high] = cpmc::WorldBox(placed);
	CHECK_EQ(low.x, 10.0);
	CHECK_EQ(low.y, 22.0);
	CHECK_EQ(low.z, 7.0);
	CHECK_EQ(high.x, 12.0);
	CHECK_EQ(high.y, 24.0);
	CHECK_EQ(high.z, 9.0);
}

TEST(StructuresRefuseUnknownNames)
{
	const cpmc::Atlas atlas = fixture::Atlas();
	BlockWorld        world;
	std::string       error;
	CHECK(!cpmc::PlaceStructure(world, atlas, "castle", {}, error));
	CHECK_STR(error, "no structure called \"castle\" in this build");
}
