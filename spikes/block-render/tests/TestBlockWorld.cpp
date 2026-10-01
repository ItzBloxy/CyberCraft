#include "Harness.h"

#include "world/BlockWorld.h"

using cpmc::BlockPos;
using cpmc::BlockWorld;
using cpmc::SectionPos;

TEST(BlockWorldStoresBlocksInSections)
{
	BlockWorld world;
	CHECK_EQ(world.Get({ 3, 4, 5 }), uint16_t(0));
	world.Set({ 3, 4, 5 }, 7);
	world.Set({ -1, -1, -1 }, 2);
	CHECK_EQ(world.Get({ 3, 4, 5 }), uint16_t(7));
	CHECK_EQ(world.Get({ -1, -1, -1 }), uint16_t(2));
	CHECK_EQ(world.BlockCount(), size_t(2));
	CHECK_EQ(world.SectionCount(), size_t(2));
	CHECK(world.Find({ -1, -1, -1 }) != nullptr);
	CHECK(world.Find({ 1, 0, 0 }) == nullptr);
	world.Set({ 3, 4, 5 }, 0);
	CHECK_EQ(world.BlockCount(), size_t(1));
	world.Set({ -1, -1, -1 }, 3);  // replacing a block doesn't add one
	CHECK_EQ(world.BlockCount(), size_t(1));
}

TEST(BlockWorldSplitsNegativeCoordinatesByFloor)
{
	CHECK(cpmc::SectionOf({ -1, 0, 15 }) == (SectionPos{ -1, 0, 0 }));
	CHECK(cpmc::SectionOf({ -16, -17, 16 }) == (SectionPos{ -1, -2, 1 }));
}

TEST(BlockWorldMarksChangedSectionsAndBorderNeighbours)
{
	BlockWorld world;
	world.Set({ 5, 5, 5 }, 1);
	auto dirty = world.TakeDirty();
	CHECK_EQ(dirty.size(), size_t(1));
	CHECK(dirty[0] == (SectionPos{ 0, 0, 0 }));
	CHECK(world.TakeDirty().empty());

	world.Set({ 0, 5, 15 }, 1);  // on the west and south borders
	dirty = world.TakeDirty();
	CHECK_EQ(dirty.size(), size_t(3));
	CHECK(dirty[0] == (SectionPos{ -1, 0, 0 }));
	CHECK(dirty[1] == (SectionPos{ 0, 0, 0 }));
	CHECK(dirty[2] == (SectionPos{ 0, 0, 1 }));

	world.Set({ 5, 5, 5 }, 1);  // no change, nothing dirty
	CHECK(world.TakeDirty().empty());
}

TEST(BlockWorldCanMarkEverySectionDirty)
{
	BlockWorld world;
	world.Set({ 5, 5, 5 }, 1);
	world.Set({ 40, 5, 5 }, 1);
	world.TakeDirty();
	world.MarkAllDirty();
	CHECK_EQ(world.TakeDirty().size(), size_t(2));
	CHECK_EQ(world.BlockCount(), size_t(2));
}

TEST(BlockWorldClearEmptiesEverySection)
{
	BlockWorld world;
	world.Set({ 5, 5, 5 }, 1);
	world.Set({ 40, 5, 5 }, 1);
	world.TakeDirty();
	world.Clear();
	CHECK_EQ(world.BlockCount(), size_t(0));
	CHECK_EQ(world.SectionCount(), size_t(0));
	CHECK_EQ(world.TakeDirty().size(), size_t(2));
	CHECK_EQ(world.Get({ 5, 5, 5 }), uint16_t(0));
}
