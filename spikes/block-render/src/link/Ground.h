#pragma once

#include "Coords.h"
#include "link/Voxels.h"

#include <array>
#include <compare>
#include <cstdint>
#include <map>
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

// Which batches to read, around a centre that moves (the spec's "The area", "Refreshing" and "Settling"): SkyCraft's
// area, its order (nearest first, height counting double), its refresh of the regions next to the centre, and its
// forgetting of regions far away, so they're read again on return.
class GroundAreas
{
public:
	static constexpr int32_t kAcross = 5;     // regions out from the centre's, each way horizontally
	static constexpr int32_t kBelow = 3;      // regions below the centre's
	static constexpr int32_t kAbove = 2;      // and above
	static constexpr double  kRefresh = 1.0;  // seconds before a batch next to the centre is read again
	static constexpr double  kSettle = 1.5;   // seconds SkyCraft gives the collision to stream in, after a load say

	// Forgets every batch read, so each is read and sent again. Nothing is read before aReadyAt, nor before the time an
	// earlier reset gave.
	void Reset(double aReadyAt);
	// Where the area is centred. Batches with no region within one more than the area are forgotten.
	void Centre(const RegionPos& aCentre);

	// The next batch to read: the nearest not read yet, or one next to the centre read kRefresh or more ago. Nothing
	// before a centre is set, before the time Reset gave, or while every batch is read and fresh.
	std::optional<BatchPos> Next(double aNow) const;

	// aBatch was read at aNow and filed (FileBatch's order). Which of its regions to send, as indices into RegionsOf: those
	// whose triangles changed, and every one the first time.
	std::vector<size_t> Read(const BatchPos& aBatch, double aNow, const std::array<std::vector<McTriangle>, 8>& aFiled);

	std::optional<RegionPos> CentreRegion() const { return centre_; }
	size_t                   Known() const { return read_.size() * 8; }  // regions read since the last reset
	size_t                   Waiting() const;                            // the area's batches not read yet

private:
	struct Wanted
	{
		BatchPos batch;
		bool     nextToCentre = false;  // holds a region next to the centre: read again every kRefresh
	};

	struct Done
	{
		double                  at = 0.0;
		std::array<uint64_t, 8> hashes{};
	};

	std::optional<RegionPos> centre_;
	std::vector<Wanted>      wanted_;  // the area's batches, nearest first
	std::map<BatchPos, Done> read_;
	double                   readyAt_ = 0.0;
};
}
