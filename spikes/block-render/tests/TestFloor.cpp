#include "Harness.h"

#include "link/Floor.h"

#include <skycraft_protocol.h>

#include <algorithm>
#include <cstring>
#include <string>

using namespace cpmc::link;
namespace proto = skycraft::proto;

namespace
{
using Region = std::pair<int32_t, int32_t>;

std::vector<uint8_t> FromHex(const std::string& aHex)
{
	std::vector<uint8_t> bytes;
	for (size_t i = 0; i + 1 < aHex.size(); i += 2)
	{
		bytes.push_back(uint8_t(std::stoi(aHex.substr(i, 2), nullptr, 16)));
	}
	return bytes;
}
}

// SkyCraft's tools/fake_skyrim.py at bfcaf17: floor_tris_payload(12500, 12, -1, 1), its floor being at y = 100.
TEST(FloorMatchesFakeSkyrim)
{
	const auto expected = FromHex(
	    "a086010060000000f8ffffffa786010067000000ffffffff01000000020000000050c3470000c842000000c10054c3470000c842000000c1"
	    "0054c3470000c84200000000000000000050c3470000c842000000c10054c3470000c842000000000050c3470000c8420000000000000000");
	CHECK_EQ(expected.size(), size_t(112));
	CHECK(FloorRegion(12500, -1, 100.0, 1) == expected);
}

TEST(FloorRegionRowHoldsTheHeight)
{
	const auto       bytes = FloorRegion(-15, 250, 6.14, 3);
	proto::ColRegion region;
	std::memcpy(&region, bytes.data(), sizeof region);
	CHECK_EQ(region.minX, -120);
	CHECK_EQ(region.maxX, -113);
	CHECK_EQ(region.minY, 0);
	CHECK_EQ(region.maxY, 7);
	CHECK_EQ(region.minZ, 2000);
	CHECK_EQ(region.epoch, 3u);
	CHECK_EQ(region.count, 2u);
	proto::ColTri triangle;
	std::memcpy(&triangle, bytes.data() + sizeof region, sizeof triangle);
	CHECK_EQ(triangle.v[1], 6.14f);
	CHECK_EQ(triangle.flags, 0u);  // not diggable: attacking the floor does nothing
	std::memcpy(&region, FloorRegion(0, 0, -0.5, 1).data(), sizeof region);
	CHECK_EQ(region.minY, -8);
}

TEST(FloorCoversSevenBySevenRegions)
{
	const auto regions = FloorRegions(-119.5, 2007.5);  // where V stood at spike 3's corner
	CHECK_EQ(regions.size(), size_t(49));
	CHECK(std::find(regions.begin(), regions.end(), Region(-15, 250)) != regions.end());
	CHECK(regions.front() == Region(-18, 247));
	CHECK(regions.back() == Region(-12, 253));
}

TEST(FloorClearHoldsTheEpoch)
{
	CHECK(FloorClear(0x01020304) == std::vector<uint8_t>({ 4, 3, 2, 1 }));
}

TEST(FloorVoxelsMakeItsRegionsKnown)
{
	// The floor's own region gets its voxels: one sub-voxel layer of the block row at 6.14. The regions just below and
	// above come empty, so the feet's, the one under them and the one 8 blocks down are all known to SkyCraft.
	const auto payloads = FloorVoxels(-15, 250, 6.14, 3);
	CHECK_EQ(payloads.size(), size_t(3));
	const int32_t expectedMinY[3] = { 0, -8, 8 };
	const size_t  expectedBlocks[3] = { 64, 0, 0 };
	for (size_t i = 0; i < payloads.size(); ++i)
	{
		proto::ColRegion region;
		std::memcpy(&region, payloads[i].data(), sizeof region);
		CHECK_EQ(region.minX, -120);
		CHECK_EQ(region.minZ, 2000);
		CHECK_EQ(region.minY, expectedMinY[i]);
		CHECK_EQ(region.epoch, 3u);
		CHECK_EQ(size_t(region.count), expectedBlocks[i]);
		CHECK_EQ(payloads[i].size(), sizeof region + expectedBlocks[i] * sizeof(proto::ColBlock));
	}
	proto::ColBlock block;
	std::memcpy(&block, payloads[0].data() + sizeof(proto::ColRegion), sizeof block);
	CHECK(block.x == -120 && block.y == 6 && block.z == 2000);
	CHECK(block.bits[0] == 0 && block.bits[1] == ~0ull && block.bits[2] == 0);
}
