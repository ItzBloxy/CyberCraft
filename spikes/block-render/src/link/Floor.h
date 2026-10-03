#pragma once

#include "link/Voxels.h"

#include <cstdint>
#include <utility>
#include <vector>

// The stand-in floor (the minecraft-link spec's "The link"): flat collision triangles at V's feet, which Minecraft's
// crosshair hits and its player walks on. Spike 5 adds their voxels (its spec's "The floor's voxels"): with them,
// SkyCraft lets its player go after a teleport, and TNT and items land on the floor.
namespace cpmc::link
{
constexpr int32_t kFloorRadius = 3;  // regions each side of the centre's: 7 by 7, 56 blocks across

// The collision clear message's payload (kColClear): the new epoch.
std::vector<uint8_t> FloorClear(uint32_t aEpoch);

// One region's triangles message payload (kColTris): two triangles covering its 8 by 8 blocks at height aY, filed in
// the region row holding aY. The same bytes as SkyCraft's tools/fake_skyrim.py floor_tris_payload for its floor.
std::vector<uint8_t> FloorRegion(int32_t aRegionX, int32_t aRegionZ, double aY, uint32_t aEpoch);

// The voxel messages (kColRegion payloads) for one column of the floor: the region holding the floor, voxelized from
// FloorRegion's two triangles, then the regions just below and above it, empty. All three count as known, which
// SkyCraft's hold wants of the feet's region, the one holding the block under them, and the one 8 blocks down.
std::vector<std::vector<uint8_t>> FloorVoxels(int32_t aRegionX, int32_t aRegionZ, double aY, uint32_t aEpoch);

// The regions of a floor centred on Minecraft block column aX, aZ: (region x, region z) pairs, 7 by 7.
std::vector<std::pair<int32_t, int32_t>> FloorRegions(double aX, double aZ);
}
