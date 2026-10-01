#pragma once

#include "Coords.h"
#include "render/Atlas.h"
#include "world/BlockWorld.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace cpmc
{
// Where a structure goes, in Minecraft block coordinates: its minimum corner 3 blocks ahead of V (along V's forward,
// flattened), in the block layer that holds V's feet. V's feet and forward are in world coordinates.
BlockPos PlacementOrigin(const Vec3d& aFeet, const Vec3f& aForward);

struct Placed
{
	size_t   blocks = 0;
	BlockPos min;  // the corners of the box it fills, inclusive
	BlockPos max;
};

// The world box a structure fills: Minecraft blocks aPlaced.min to aPlaced.max, inclusive, in world coordinates.
std::pair<Vec3d, Vec3d> WorldBox(const Placed& aPlaced);

// Adds a structure with its minimum corner at aOrigin (see the spec's "Structures"):
// - "cube": one stone block;
// - "sampler": every block of the atlas, in a row along +x with a gap between each, on a stone base;
// - "house": 7 x 7 and 5 high: a cobblestone floor, oak plank walls with oak log corners, a glass window in each wall,
//   a doorway, a plank roof, and an oak leaves bush beside it.
// Nothing, with aError saying why, for another name or when the atlas lacks a block it needs.
std::optional<Placed> PlaceStructure(BlockWorld& aWorld, const Atlas& aAtlas, std::string_view aName, const BlockPos& aOrigin,
                                     std::string& aError);

// Adds rolling ground of grass over dirt over stone with its minimum corner at aOrigin, holding aBlocks blocks, or up to
// 9 more, since it's made of whole columns. Nothing, with aError saying why, for no blocks or when the atlas lacks a
// block it needs.
std::optional<Placed> PlaceField(BlockWorld& aWorld, const Atlas& aAtlas, const BlockPos& aOrigin, size_t aBlocks, std::string& aError);
}
