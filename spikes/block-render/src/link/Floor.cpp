#include "link/Floor.h"

#include <skycraft_protocol.h>

#include <array>
#include <cmath>
#include <cstring>

namespace cpmc::link
{
namespace proto = skycraft::proto;

namespace
{
template<class T>
void Append(std::vector<uint8_t>& aBytes, const T& aValue)
{
	const auto* raw = reinterpret_cast<const uint8_t*>(&aValue);
	aBytes.insert(aBytes.end(), raw, raw + sizeof aValue);
}

int32_t RegionOf(double aBlocks)
{
	return int32_t(std::floor(aBlocks / kRegionBlocks));
}

// A floor region's two triangles, covering its 8 by 8 blocks at height aY.
std::array<McTriangle, 2> Triangles(int32_t aRegionX, int32_t aRegionZ, double aY)
{
	const float x0 = float(aRegionX * kRegionBlocks);
	const float x1 = x0 + kRegionBlocks;
	const float z0 = float(aRegionZ * kRegionBlocks);
	const float z1 = z0 + kRegionBlocks;
	const float y = float(aY);
	return { McTriangle{ { x0, y, z0, x1, y, z0, x1, y, z1 } }, McTriangle{ { x0, y, z0, x1, y, z1, x0, y, z1 } } };
}
}

std::vector<uint8_t> FloorClear(uint32_t aEpoch)
{
	std::vector<uint8_t> bytes;
	Append(bytes, aEpoch);
	return bytes;
}

std::vector<uint8_t> FloorRegion(int32_t aRegionX, int32_t aRegionZ, double aY, uint32_t aEpoch)
{
	const int32_t     regionY = RegionOf(aY);
	proto::ColRegion region{ aRegionX * kRegionBlocks, regionY * kRegionBlocks, aRegionZ * kRegionBlocks,
		                     aRegionX * kRegionBlocks + kRegionBlocks - 1, regionY * kRegionBlocks + kRegionBlocks - 1,
		                     aRegionZ * kRegionBlocks + kRegionBlocks - 1, aEpoch, 2 };
	std::vector<uint8_t> bytes;
	Append(bytes, region);
	for (const McTriangle& triangle : Triangles(aRegionX, aRegionZ, aY))
	{
		proto::ColTri tri{};  // flags 0: not diggable, so attacking the floor does nothing
		std::memcpy(tri.v, triangle.v, sizeof tri.v);
		Append(bytes, tri);
	}
	return bytes;
}

FloorVoxeler::FloorVoxeler(double aY) : regionY_(RegionOf(aY))
{
	const std::array<McTriangle, 2> triangles = Triangles(0, 0, aY);
	blocks_ = VoxelizeRegion(0, regionY_, 0, triangles);
}

std::vector<std::vector<uint8_t>> FloorVoxeler::Column(int32_t aRegionX, int32_t aRegionZ, uint32_t aEpoch) const
{
	std::vector<proto::ColBlock> blocks = blocks_;
	for (proto::ColBlock& block : blocks)
	{
		block.x += aRegionX * kRegionBlocks;
		block.z += aRegionZ * kRegionBlocks;
	}
	return { RegionPayload(aRegionX, regionY_, aRegionZ, aEpoch, blocks), RegionPayload(aRegionX, regionY_ - 1, aRegionZ, aEpoch, {}),
		     RegionPayload(aRegionX, regionY_ + 1, aRegionZ, aEpoch, {}) };
}

std::vector<std::pair<int32_t, int32_t>> FloorRegions(double aX, double aZ)
{
	const int32_t centreX = RegionOf(aX);
	const int32_t centreZ = RegionOf(aZ);
	std::vector<std::pair<int32_t, int32_t>> regions;
	for (int32_t x = centreX - kFloorRadius; x <= centreX + kFloorRadius; ++x)
	{
		for (int32_t z = centreZ - kFloorRadius; z <= centreZ + kFloorRadius; ++z)
		{
			regions.emplace_back(x, z);
		}
	}
	return regions;
}
}
