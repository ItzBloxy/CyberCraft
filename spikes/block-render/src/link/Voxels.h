#pragma once

#include <skycraft_protocol.h>

#include <cstdint>
#include <span>
#include <vector>

// Triangles to SkyCraft's collision voxels (the spec's "The floor's voxels" and "The exporter"), ported from SkyCraft's
// Skyrim side (skse/src/Collision.cpp, Collision::Voxelize and its triangle-box test, MIT), for triangles alone. A
// region is 8 blocks a side, and each block 8 x 8 x 8 sub-voxels of 1/8 block. A sub-voxel is solid where a triangle
// passes through it (Akenine-Moller's triangle-box overlap test). Surfaces 50 to 84 degrees from flat (|ny| from 0.1 to
// 0.643) are coarsened to whole-block footprints, so the risers between neighbouring columns exceed Minecraft's 0.6
// step height, as SkyCraft does. Minecraft's player walks on the triangles; the voxels are what release it after a
// teleport, and what TNT, items and mobs land on.
namespace cpmc::link
{
namespace proto = skycraft::proto;

constexpr int32_t kRegionBlocks = 8;  // SkyCraft's collision regions are 8-block cubes

// A triangle in Minecraft's axes, in blocks.
struct McTriangle
{
	float v[9];
};

// Region (aRegionX, aRegionY, aRegionZ)'s blocks that have any solid sub-voxel, each with its sub-voxels: bits[y], bit
// z * 8 + x. In order of y, then z, then x. Parts of triangles outside the region are ignored.
std::vector<proto::ColBlock> VoxelizeRegion(int32_t aRegionX, int32_t aRegionY, int32_t aRegionZ, std::span<const McTriangle> aTriangles);

// A voxel region message's payload (kColRegion): the region's inclusive block box, the epoch, the count and the blocks.
// With no blocks, it marks the region known and empty.
std::vector<uint8_t> RegionPayload(int32_t aRegionX, int32_t aRegionY, int32_t aRegionZ, uint32_t aEpoch,
                                   std::span<const proto::ColBlock> aBlocks);

// A triangles message's payload (kColTris): the region's inclusive block box, the epoch, the count and the triangles,
// each with flags 0: not diggable, so attacking them does nothing. Minecraft files them under the box's corner region.
std::vector<uint8_t> TrisPayload(int32_t aRegionX, int32_t aRegionY, int32_t aRegionZ, uint32_t aEpoch,
                                 std::span<const McTriangle> aTriangles);
}
