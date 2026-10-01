#pragma once

#include <cstdint>

namespace cpmc
{
// SkyCraft's section vertex (the spec's "Vertex format"). Quads are two triangles, six vertices, with no index buffer.
struct Vertex
{
	float    x = 0.0f;  // from the section's minimum corner, in blocks, in Minecraft axes (x east, y up, z south)
	float    y = 0.0f;
	float    z = 0.0f;
	float    u = 0.0f;  // atlas coordinates, 0 to 1
	float    v = 0.0f;
	uint8_t  r = 255;  // tint and ambient occlusion, without Minecraft's per-face brightness
	uint8_t  g = 255;
	uint8_t  b = 255;
	uint8_t  a = 255;
	uint32_t light = 0;  // block light (0-15) in bits 0-7, sky light (0-15) in bits 8-15
	uint32_t flags = 0;  // bit 0 alpha-tested (opaque or cut-out), bit 1 translucent, bits 4-7 the face's direction + 1
};
static_assert(sizeof(Vertex) == 32);

// Minecraft's Direction order, which the vertex flags and the atlas's faces use.
enum class Face : uint8_t
{
	Down,
	Up,
	North,
	South,
	West,
	East,
};

constexpr uint32_t kFlagAlphaTested = 1u << 0;
constexpr uint32_t kFlagTranslucent = 1u << 1;

constexpr uint32_t FaceFlags(Face aFace)
{
	return (uint32_t(aFace) + 1u) << 4;
}
}
