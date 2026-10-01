#pragma once

#include "render/Atlas.h"
#include "world/BlockWorld.h"
#include "world/Vertex.h"

#include <vector>

namespace cpmc
{
struct MeshOptions
{
	bool ambientOcclusion = true;  // Minecraft-style corner shading in the vertex colour
};

// The visible faces of one section's blocks as SkyCraft vertices, positions relative to the section's minimum corner.
// - A face is hidden by an opaque neighbour, in this section or the next. A cut-out block (glass, leaves) hides a
//   face only of the same block.
// - Colour: the face's tint (if the atlas tints it) times the corner's ambient occlusion: (1 + side1 + side2 +
//   corner) / 4, where an opaque block counts 0.2 and anything else 1, and two opaque sides make the corner count
//   0.2 too. Minecraft's per-face brightness is left to the shader.
// - Light: sky light 15, block light 0. Flags: alpha-tested, plus the face's direction.
// - Each quad is split along its darker diagonal, so a shaded corner's gradient spreads over both triangles alike.
std::vector<Vertex> MeshSection(const BlockWorld& aWorld, const SectionPos& aSection, const Atlas& aAtlas,
                                const MeshOptions& aOptions);
}
