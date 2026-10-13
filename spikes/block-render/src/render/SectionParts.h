#pragma once

#include "render/Atlas.h"
#include "world/Vertex.h"

#include <cstdint>
#include <span>
#include <vector>

// The polish pass: a section's triangles in three parts, each drawn its own way.
// - Solid: into the G-buffer and the tagged depth, as every block was before. A translucent triangle whose texels are
//   all clear or opaque is one too: glass, which Minecraft 26.3 puts in its translucent layer (its model's
//   force_translucent) although it looks as a cut-out does, and which the game then lights as it lights the leaves.
// - Plant: a triangle with no direction of its own (bits 4 to 7 of its flags 0), which SkyCraft sends for the quads
//   Minecraft shades as if they faced up: its plants (the cross, tinted_cross and crop models), vines, coral and
//   seagrass, and a few thin things besides (torches, lanterns, chains, fire, ladders, redstone dust). Into the G-buffer
//   with the game's own class for small plants (its stencil value and material B), whose lighting doesn't shadow them
//   dark as the blocks' class does a field of grass.
// - Translucent: the rest of Minecraft's translucent layer (flag bit 1), whose texels are partly clear: water, stained
//   glass, ice, slime and honey. Blended into the scene's colour just before DLSS, as Minecraft blends them.
namespace cpmc
{
enum class SectionPart : uint8_t
{
	Solid,
	Plant,
	Translucent,
};

// Vertex counts of each part, which SplitSection lays out in this order.
struct SectionParts
{
	uint32_t solid = 0;
	uint32_t plants = 0;
	uint32_t translucent = 0;

	uint32_t First(SectionPart aPart) const;  // the part's first vertex
	uint32_t Count(SectionPart aPart) const;
};

// Whether any texel of aAtlas's top level under the box of aTriangle's UVs has an alpha between clear and opaque. The
// box's edges are rounded to the nearest texel edge, so a UV a little inside a sprite's edge reads that sprite alone.
bool PartlyClear(const Atlas& aAtlas, std::span<const Vertex, 3> aTriangle);

// Which part aTriangle belongs to. Without an atlas, a translucent triangle stays translucent.
SectionPart PartOf(std::span<const Vertex, 3> aTriangle, const Atlas* aAtlas);

// Orders aVertices' whole triangles by part, solid first, keeping each part's triangles in their order, and drops any
// vertices after the last whole triangle.
SectionParts SplitSection(std::vector<Vertex>& aVertices, const Atlas* aAtlas);
}
