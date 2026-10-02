#pragma once

#include "world/Vertex.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// The render ring's messages this spike takes (the spec's "Blocks"), checked against their layouts before use: shared
// memory is written by another process, so nothing in it is trusted.
namespace cpmc::link
{
constexpr int32_t  kMaxSectionXZ = 2'000'000;  // the world border is 30 million blocks, 1.9 million sections, out
constexpr int32_t  kMaxSectionY = 256;
constexpr uint32_t kMaxAtlasSide = 16384;

// A section's mesh (kRenSection): whole triangles in SkyCraft's vertex, which is BlockRender's Vertex.
struct SectionMesh
{
	int32_t             x = 0;  // Minecraft section coordinates
	int32_t             y = 0;
	int32_t             z = 0;
	std::vector<Vertex> vertices;  // none: the section is gone
	uint32_t            translucentDropped = 0;  // triangles left out
};

// A kRenSection payload: {i32 x, y, z; u32 vertices}, then the vertices. A count that isn't a multiple of 3 is cut down
// to one. Translucent triangles (flag bit 1) are left out unless aKeepTranslucent. Nothing, with aError saying why, if
// the payload is too short for its count or the coordinates are out of range.
std::optional<SectionMesh> ParseSection(std::span<const uint8_t> aPayload, bool aKeepTranslucent, std::string& aError);

// Minecraft's combined atlas (kRenAtlas): RGBA8, top row first.
struct AtlasImage
{
	uint32_t                 width = 0;
	uint32_t                 height = 0;
	std::span<const uint8_t> rgba;  // width * height * 4 bytes, inside the payload
};

// A kRenAtlas payload: {u32 width, height}, then the pixels. Nothing, with aError saying why, if a side is 0 or over
// 16,384, or the payload is too short.
std::optional<AtlasImage> ParseAtlas(std::span<const uint8_t> aPayload, std::string& aError);
}
