#include "Harness.h"

#include "AtlasFixture.h"
#include "world/Structures.h"

using cpmc::BlockPos;
using cpmc::BlockWorld;

namespace
{
// An atlas with every block the structures use, each drawn with the same plain sprite.
cpmc::Atlas BuildingAtlas()
{
	std::vector<uint8_t> out;
	const char           magic[] = "CPMCATL1";
	out.insert(out.end(), magic, magic + 8);
	const char* names[] = { "stone", "cobblestone", "oak_planks", "oak_log", "glass", "oak_leaves", "grass_block", "dirt" };
	for (const uint32_t value : { 16u, 16u, 1u, 1u, uint32_t(std::size(names)), 0u })  // size, levels, sprites, blocks
	{
		fixture::Put32(out, value);
	}
	for (const uint32_t value : { 0u, 0u, 16u, 16u })
	{
		fixture::Put32(out, value);
	}
	for (const char* name : names)
	{
		fixture::PutBlock(out, name, { 0, 0, 0, 0, 0, 0 }, { 255, 255, 255 }, 0, false);
	}
	for (int i = 0; i < 16 * 16; ++i)
	{
		fixture::Put32(out, 0xFF808080u);
	}
	std::string error;
	return *cpmc::ParseAtlas(out, error);
}

bool Is(const BlockWorld& aWorld, const cpmc::Atlas& aAtlas, const BlockPos& aAt, const char* aName)
{
	return aWorld.Get(aAt) == aAtlas.BlockId(aName);
}
}

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

TEST(StructuresPlaceAHouse)
{
	const cpmc::Atlas atlas = BuildingAtlas();
	BlockWorld        world;
	std::string       error;
	const auto        placed = cpmc::PlaceStructure(world, atlas, "house", { 0, 64, 0 }, error);
	CHECK(placed.has_value());
	CHECK_EQ(placed->blocks, size_t(176));  // floor 49, roof 49, walls 72 less the doorway's 2, bush 8
	CHECK_EQ(world.BlockCount(), size_t(176));
	CHECK(placed->max == (BlockPos{ 9, 68, 6 }));
	CHECK(Is(world, atlas, { 0, 64, 0 }, "cobblestone") && Is(world, atlas, { 6, 64, 6 }, "cobblestone"));  // the floor
	CHECK(Is(world, atlas, { 0, 65, 0 }, "oak_log") && Is(world, atlas, { 6, 67, 6 }, "oak_log"));          // corners
	CHECK(Is(world, atlas, { 1, 65, 0 }, "oak_planks"));                                                    // a wall
	CHECK(Is(world, atlas, { 5, 66, 0 }, "glass") && Is(world, atlas, { 3, 66, 6 }, "glass"));              // windows
	CHECK(Is(world, atlas, { 0, 66, 3 }, "glass") && Is(world, atlas, { 6, 66, 3 }, "glass"));
	CHECK_EQ(world.Get({ 3, 65, 0 }), uint16_t(0));  // the doorway
	CHECK_EQ(world.Get({ 3, 66, 0 }), uint16_t(0));
	CHECK(Is(world, atlas, { 3, 67, 0 }, "oak_planks"));  // above it
	CHECK_EQ(world.Get({ 3, 65, 3 }), uint16_t(0));       // inside
	CHECK(Is(world, atlas, { 3, 68, 3 }, "oak_planks"));  // the roof
	CHECK(Is(world, atlas, { 8, 64, 2 }, "oak_leaves") && Is(world, atlas, { 9, 65, 3 }, "oak_leaves"));  // the bush
}

TEST(StructuresPlaceAFieldOfAboutTheBlocksAskedFor)
{
	const cpmc::Atlas atlas = BuildingAtlas();
	for (const size_t blocks : { size_t(1), size_t(1000), size_t(10000) })
	{
		BlockWorld  world;
		std::string error;
		const auto  placed = cpmc::PlaceField(world, atlas, { 0, 0, 0 }, blocks, error);
		CHECK(placed.has_value());
		CHECK(placed->blocks >= blocks && placed->blocks < blocks + 10);  // whole columns, of at most 10 blocks
		CHECK_EQ(world.BlockCount(), placed->blocks);
	}
	BlockWorld  world;
	std::string error;
	cpmc::PlaceField(world, atlas, { 0, 0, 0 }, 1000, error);
	// The first column is 6 + round(2 sin 0 + 2 cos 0) = 8 high: grass, three of dirt, then stone.
	CHECK(Is(world, atlas, { 0, 7, 0 }, "grass_block"));
	CHECK(Is(world, atlas, { 0, 6, 0 }, "dirt") && Is(world, atlas, { 0, 4, 0 }, "dirt"));
	CHECK(Is(world, atlas, { 0, 3, 0 }, "stone") && Is(world, atlas, { 0, 0, 0 }, "stone"));
	CHECK_EQ(world.Get({ 0, 8, 0 }), uint16_t(0));
}

TEST(StructuresNeedTheirBlocks)
{
	const cpmc::Atlas atlas = fixture::Atlas();  // no cobblestone, planks or dirt
	BlockWorld        world;
	std::string       error;
	CHECK(!cpmc::PlaceStructure(world, atlas, "house", {}, error));
	CHECK_STR(error, "the atlas has no cobblestone");
	CHECK(!cpmc::PlaceField(world, atlas, {}, 100, error));
	CHECK_STR(error, "the atlas has no dirt");
	CHECK(!cpmc::PlaceField(world, BuildingAtlas(), {}, 0, error));
	CHECK_STR(error, "a field needs at least one block");
	CHECK_EQ(world.BlockCount(), size_t(0));
}
