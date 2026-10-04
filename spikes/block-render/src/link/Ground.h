#pragma once

#include "Coords.h"
#include "link/Voxels.h"

#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

// Night City's shape in Minecraft (spike 5's "The exporter"), the parts that don't touch the game: SkyCraft's area of
// regions around a centre, read 2 x 2 x 2 regions at a time, nearest first; each triangle filed under the regions it
// touches; and a region sent again only when its triangles change. Pure: the game reads PhysX for the triangles, and
// mclink a saved dump.
namespace cpmc::link
{
// A collision region: the 8-block cube from (x, y, z) * 8, in Minecraft's blocks.
struct RegionPos
{
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;

	auto operator<=>(const RegionPos&) const = default;
};

// The region holding a point, Minecraft axes. Nothing for a point that isn't finite, or is over a million blocks out.
std::optional<RegionPos> RegionAt(const Vec3d& aMc);

// A batch: the 2 x 2 x 2 regions from (2x, 2y, 2z) to (2x + 1, 2y + 1, 2z + 1), read together, so a large shape's
// triangles are culled once for eight regions.
struct BatchPos
{
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;

	auto operator<=>(const BatchPos&) const = default;
};

BatchPos BatchOf(const RegionPos& aRegion);

// The batch's eight regions, in order of y, then z, then x.
std::array<RegionPos, 8> RegionsOf(const BatchPos& aBatch);

// A box in Minecraft's blocks.
struct McBox
{
	Vec3d low;
	Vec3d high;
};

constexpr double kReadMargin = 0.25;  // blocks read around a batch, as SkyCraft's harvest reads around a region
constexpr double kFileMargin = 0.5;   // a triangle goes to every region it touches within this, as SkyCraft sends them

// The batch's blocks, aMargin more each way.
McBox BatchBox(const BatchPos& aBatch, double aMargin = kReadMargin);

// The batch's triangles by region, in RegionsOf's order: each triangle under every region whose blocks, kFileMargin more
// each way, its bounds touch. A triangle with a coordinate that isn't finite, or is over a million blocks out, is left
// out, as SkyCraft leaves it out.
std::array<std::vector<McTriangle>, 8> FileBatch(const BatchPos& aBatch, std::span<const McTriangle> aTriangles);

// A hash of a region's triangles that doesn't depend on their order: a read can list the same shapes in another order.
uint64_t TrianglesHash(std::span<const McTriangle> aTriangles);
}
