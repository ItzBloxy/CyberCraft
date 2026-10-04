#pragma once

#include "link/Solids.h"

#include <cstdint>
#include <vector>

// A section's solid blocks as few boxes, for its body (spike 5's "Bodies"): greedy along x, then z, then y, so a 16 x 16
// layer is one box and a whole section one. Every solid block is in exactly one box, and no box holds a block that isn't
// solid. Pure.
namespace cpmc::px
{
// Blocks within a section: from (x0, y0, z0) up to, but not including, (x1, y1, z1).
struct BlockBox
{
	uint8_t x0 = 0, y0 = 0, z0 = 0;
	uint8_t x1 = 0, y1 = 0, z1 = 0;
};

std::vector<BlockBox> MergeBoxes(const link::SolidBits& aBits);
}
