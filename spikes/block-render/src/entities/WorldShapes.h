#pragma once

#include "Coords.h"
#include "world/Vertex.h"

#include <skycraft_protocol.h>

#include <cstdint>
#include <vector>

// Spike 7: Minecraft's world entities as triangles (the spec's "Entities: one frame's geometry"), shaped as SkyCraft's
// own game side shapes them (WorldRender.cpp's BuildEntities), at Minecraft's size: a dropped item's icon, a dropped
// block's spinning cube, an arrow's fins and back plate, a trident's icon on two crossed quads, and the cracks over a
// block being mined. Positions are relative to the frame's origin, a whole block, in Minecraft axes; UVs are into
// Minecraft's combined atlas. Pure.
namespace cpmc::entities
{
namespace proto = skycraft::proto;

// Every shape's vertices: cut-out, without mipmaps, lit by their triangle's own normal, with sky light 15 and block
// light 0, so none of them glows, as SkyCraft's are.
constexpr uint32_t kShapeFlags = kFlagAlphaTested | (1u << 3) | (7u << 4);
constexpr uint32_t kShapeLight = 15u << 8;

// A block per unit of Minecraft's ArrowModel, which ArrowRenderer scales by 0.05625. SkyCraft's own game side draws
// arrows at 0.55 of it, to suit Skyrim's people; Night City's are Minecraft's height.
constexpr float kArrowUnit = 0.9f / 16.0f;
constexpr float kTridentHalf = 0.9f;  // a trident's icon: its corners this far out along and across its flight

// The record's triangles, relative to aOrigin, appended to aOut: an item, a block, an arrow or a trident. A crack, a
// shadow or a kind it doesn't know adds none.
void AppendShape(const proto::WorldEntity& aRecord, const Vec3d& aOrigin, std::vector<Vertex>& aOut);

// A crack's box, its six faces with the stage's texture, relative to aOrigin, appended to aOut. Any other kind adds
// none.
void AppendCrack(const proto::WorldEntity& aRecord, const Vec3d& aOrigin, std::vector<Vertex>& aOut);
}
