#pragma once

#include <cstdint>
#include <utility>
#include <vector>

// The stand-in floor (the spec's "The link"): flat collision triangles at V's feet, which Minecraft's crosshair hits,
// so clicks have something to place blocks on. Triangles only: SkyCraft releases its held player only once voxel
// regions are known, so Minecraft's player stays pinned where it was teleported.
namespace cpmc::link
{
constexpr int32_t kRegionBlocks = 8;  // SkyCraft's collision regions are 8-block cubes
constexpr int32_t kFloorRadius = 3;   // regions each side of the centre's: 7 by 7, 56 blocks across

// The collision clear message's payload (kColClear): the new epoch.
std::vector<uint8_t> FloorClear(uint32_t aEpoch);

// One region's triangles message payload (kColTris): two triangles covering its 8 by 8 blocks at height aY, filed in
// the region row holding aY. The same bytes as SkyCraft's tools/fake_skyrim.py floor_tris_payload for its floor.
std::vector<uint8_t> FloorRegion(int32_t aRegionX, int32_t aRegionZ, double aY, uint32_t aEpoch);

// The regions of a floor centred on Minecraft block column aX, aZ: (region x, region z) pairs, 7 by 7.
std::vector<std::pair<int32_t, int32_t>> FloorRegions(double aX, double aZ);
}
