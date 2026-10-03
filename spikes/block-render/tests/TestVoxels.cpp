#include "Harness.h"

#include "link/Voxels.h"

#include <cstring>

using namespace cpmc::link;

namespace
{
McTriangle Tri(float aX0, float aY0, float aZ0, float aX1, float aY1, float aZ1, float aX2, float aY2, float aZ2)
{
	return { { aX0, aY0, aZ0, aX1, aY1, aZ1, aX2, aY2, aZ2 } };
}

// A flat floor at height aY over the 8 x 8 blocks from aX, aZ: two triangles.
std::vector<McTriangle> Floor(float aY, float aX = 0.0f, float aZ = 0.0f)
{
	return { Tri(aX, aY, aZ, aX + 8, aY, aZ, aX + 8, aY, aZ + 8), Tri(aX, aY, aZ, aX + 8, aY, aZ + 8, aX, aY, aZ + 8) };
}

bool OnlyLayer(const proto::ColBlock& aBlock, int aLayer, uint64_t aBits)
{
	for (int y = 0; y < 8; ++y)
	{
		if (aBlock.bits[y] != (y == aLayer ? aBits : 0ull))
		{
			return false;
		}
	}
	return true;
}
}

TEST(VoxelsFillOneLayerUnderAFlatFloor)
{
	// Spike 3's corner: region -15 0 250 is blocks x -120 to -113 and z 2000 to 2007. The floor at 6.14 lies in sub-voxel
	// layer 1 of block row 6, which is 6.125 to 6.25.
	const auto blocks = VoxelizeRegion(-15, 0, 250, Floor(6.14f, -120.0f, 2000.0f));
	CHECK_EQ(blocks.size(), size_t(64));
	int good = 0;
	for (const proto::ColBlock& block : blocks)
	{
		good += block.y == 6 && block.x >= -120 && block.x <= -113 && block.z >= 2000 && block.z <= 2007 && OnlyLayer(block, 1, ~0ull);
	}
	CHECK_EQ(good, 64);
}

TEST(VoxelsFillBothLayersAtABoundary)
{
	// At exactly 6.0 the floor touches the top layer of block row 5 and the bottom layer of row 6.
	const auto blocks = VoxelizeRegion(0, 0, 0, Floor(6.0f));
	CHECK_EQ(blocks.size(), size_t(128));
	int below = 0;
	int above = 0;
	for (const proto::ColBlock& block : blocks)
	{
		below += block.y == 5 && OnlyLayer(block, 7, ~0ull);
		above += block.y == 6 && OnlyLayer(block, 0, ~0ull);
	}
	CHECK_EQ(below, 64);
	CHECK_EQ(above, 64);
}

TEST(VoxelsKeepWallsFine)
{
	// A wall at x 2.5, the edge between sub-voxels 3 and 4 of block column 2: both, all the way up and along.
	const std::vector<McTriangle> wall{ Tri(2.5f, 0, 0, 2.5f, 8, 0, 2.5f, 8, 8), Tri(2.5f, 0, 0, 2.5f, 8, 8, 2.5f, 0, 8) };
	const auto                    blocks = VoxelizeRegion(0, 0, 0, wall);
	CHECK_EQ(blocks.size(), size_t(64));
	int good = 0;
	for (const proto::ColBlock& block : blocks)
	{
		bool layers = true;
		for (uint64_t layer : block.bits)
		{
			layers = layers && layer == 0x1818181818181818ull;
		}
		good += block.x == 2 && layers;
	}
	CHECK_EQ(good, 64);
}

TEST(VoxelsFollowATrianglesEdges)
{
	// A triangle over half of block 0 0 0, cut along its diagonal, 0.55 up, in sub-voxel layer 4: the sub-voxels under it
	// are solid, and those beyond the diagonal stay empty, though they're inside its bounds.
	const std::vector<McTriangle> half{ Tri(0.05f, 0.55f, 0.05f, 0.95f, 0.55f, 0.05f, 0.05f, 0.55f, 0.95f) };
	const auto                    blocks = VoxelizeRegion(0, 0, 0, half);
	CHECK_EQ(blocks.size(), size_t(1));
	const uint64_t layer = blocks.empty() ? 0ull : blocks[0].bits[4];
	CHECK((layer & 1ull) != 0);                // x 0, z 0: under the triangle
	CHECK((layer & (1ull << (6 * 8 + 6))) == 0);  // x 6, z 6: beyond the diagonal
	CHECK((layer >> 63) == 0);                 // x 7, z 7
}

TEST(VoxelsCoarsenSteepSlopes)
{
	// About 63 degrees from flat (|ny| 0.447), from 0.1 to 0.7 up, within block 0 0 0: its layers 0 to 5 become solid
	// across the whole block, so a slope too steep to walk becomes a step Minecraft won't climb.
	const auto blocks = VoxelizeRegion(0, 0, 0, std::vector<McTriangle>{ Tri(0.1f, 0.1f, 0.1f, 0.4f, 0.7f, 0.1f, 0.1f, 0.1f, 0.4f) });
	CHECK_EQ(blocks.size(), size_t(1));
	CHECK(blocks[0].x == 0 && blocks[0].y == 0 && blocks[0].z == 0);
	for (int y = 0; y < 8; ++y)
	{
		CHECK_EQ(blocks[0].bits[y], y <= 5 ? ~0ull : 0ull);
	}
}

TEST(VoxelsIgnoreTrianglesOutside)
{
	CHECK(VoxelizeRegion(0, 0, 0, Floor(6.14f, 16.0f, 0.0f)).empty());
	CHECK(VoxelizeRegion(0, 0, 0, Floor(20.0f)).empty());
	CHECK(VoxelizeRegion(0, 0, 0, std::vector<McTriangle>{ Tri(1, 1, 1, 1, 1, 1, 1, 1, 1) }).empty());  // no area
}

TEST(VoxelsUseTheProtocolsBitOrder)
{
	// A speck inside sub-voxel x 1, y 2, z 3 of block 0 0 0: bits[2], bit 3 * 8 + 1.
	const auto blocks =
		VoxelizeRegion(0, 0, 0, std::vector<McTriangle>{ Tri(0.17f, 0.3125f, 0.42f, 0.20f, 0.3125f, 0.42f, 0.17f, 0.3125f, 0.45f) });
	CHECK_EQ(blocks.size(), size_t(1));
	CHECK(OnlyLayer(blocks[0], 2, 1ull << (3 * 8 + 1)));
}

TEST(VoxelsRegionPayload)
{
	proto::ColBlock block{};
	block.x = -120;
	block.y = 6;
	block.z = 2000;
	block.bits[1] = ~0ull;
	const auto payload = RegionPayload(-15, 0, 250, 7, std::vector<proto::ColBlock>{ block });
	CHECK_EQ(payload.size(), sizeof(proto::ColRegion) + sizeof(proto::ColBlock));
	proto::ColRegion header;
	std::memcpy(&header, payload.data(), sizeof header);
	CHECK(header.minX == -120 && header.minY == 0 && header.minZ == 2000);
	CHECK(header.maxX == -113 && header.maxY == 7 && header.maxZ == 2007);
	CHECK_EQ(header.epoch, 7u);
	CHECK_EQ(header.count, 1u);
	CHECK(std::memcmp(payload.data() + sizeof header, &block, sizeof block) == 0);
	const auto empty = RegionPayload(-15, -1, 250, 7, {});
	CHECK_EQ(empty.size(), sizeof(proto::ColRegion));
	std::memcpy(&header, empty.data(), sizeof header);
	CHECK(header.minY == -8 && header.maxY == -1 && header.count == 0);
}
