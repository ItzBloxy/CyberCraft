#pragma once

#include "Coords.h"
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
constexpr uint32_t kMaxTextureSide = 4096;       // SkyCraft's own game side refuses a bigger texture
constexpr uint32_t kMaxSceneBatches = 65536;
constexpr uint32_t kMaxSceneVertices = 1u << 20;  // 32 MB: SkyCraft's ring takes no message over half its 64 MB

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

// Spike 7's Part 5: an animated sprite's frame (kRenAtlasRegion), a patch of Minecraft's combined atlas: the sprite's
// cell, the frame's edge pixels repeated into its border. RGBA8, top row first.
struct AtlasPatch
{
	uint32_t                 x = 0;  // its top left texel, in the atlas's top level
	uint32_t                 y = 0;
	uint32_t                 width = 0;
	uint32_t                 height = 0;
	std::span<const uint8_t> rgba;  // width * height * 4 bytes, inside the payload
};

// A kRenAtlasRegion payload: {u32 x, y, width, height}, then the pixels. Nothing, with aError saying why, if a side is
// 0, the rectangle doesn't lie inside an atlas of aAtlasWidth x aAtlasHeight, or the payload is too short.
std::optional<AtlasPatch> ParseAtlasRegion(std::span<const uint8_t> aPayload, uint32_t aAtlasWidth, uint32_t aAtlasHeight, std::string& aError);

// Spike 7: a texture besides the atlas (kRenTexture), for the scene's batches that name its id: an entity's skin, or a
// whole atlas of Minecraft's other than the blocks' and items' (chests', banners', signs', particles'). RGBA8, top row
// first. SkyCraft sends each once, and all of them again only after its clear.
struct TextureImage
{
	uint32_t                 id = 0;
	uint32_t                 width = 0;
	uint32_t                 height = 0;
	std::span<const uint8_t> rgba;  // width * height * 4 bytes, inside the payload
};

// A kRenTexture payload: {u32 id, width, height, pad}, then the pixels. Nothing, with aError saying why, if the id is
// 0, a side is 0 or over 4,096, or the payload is too short.
std::optional<TextureImage> ParseTexture(std::span<const uint8_t> aPayload, std::string& aError);

// One of a scene's batches: a run of its vertices drawn with one texture.
struct SceneBatch
{
	uint32_t texture = 0;  // 0: Minecraft's combined atlas; else a texture's id
	uint32_t first = 0;    // its first vertex
	uint32_t count = 0;    // its vertices, whole triangles
	uint32_t flags = 0;    // bit 0: translucent, drawn blended
};

// Spike 7: everything Minecraft draws this frame besides its blocks and its player (kRenScene): entities, block
// entities and particles, posed, their positions in Minecraft axes from the origin. Each Minecraft frame sends one.
struct SceneMessage
{
	Vec3d                    origin;  // a whole block near Minecraft's camera, Minecraft axes
	std::vector<SceneBatch>  batches;
	uint32_t                 vertexCount = 0;
	std::span<const uint8_t> vertices;  // vertexCount vertices in SkyCraft's format, which is Vertex, inside the payload
};

// A kRenScene payload: {f64 origin x, y, z; u32 batches, vertices}, the batches {u32 texture, first, count, flags},
// then the vertices. Nothing, with aError saying why, if the origin isn't finite, there are over 65,536 batches or
// 1,048,576 vertices, the payload is too short for them, or a batch isn't whole triangles within the vertices. A scene
// of no batches says nothing is there.
std::optional<SceneMessage> ParseScene(std::span<const uint8_t> aPayload, std::string& aError);

// The scene's vertices, copied out of the payload.
std::vector<Vertex> SceneVertices(const SceneMessage& aScene);
}
