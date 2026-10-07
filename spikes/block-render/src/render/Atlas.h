#pragma once

#include "render/FrameConstants.h"

#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace cpmc
{
// One block of tools/build_atlas.py's atlas.bin (its docstring gives the layout).
struct AtlasBlock
{
	std::string              name;
	std::array<uint16_t, 6>  faces{};  // sprite per face: down, up, north, south, west, east
	std::array<uint8_t, 3>   tint{};   // RGB multiplier for the tinted faces
	uint8_t                  tintedFaces = 0;  // bit per face, same order
	bool                     cutout = false;
};

struct AtlasSprite
{
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t w = 0;
	uint32_t h = 0;
};

struct Atlas
{
	uint32_t                 width = 0;   // level 0, in texels
	uint32_t                 height = 0;
	std::vector<AtlasSprite> sprites;
	std::vector<AtlasBlock>  blocks;
	std::vector<uint32_t>    levelOffsets;  // where each level starts in `texels`
	std::vector<uint32_t>    texels;        // RGBA8, one per uint32 (red in the low byte), every level in turn

	uint32_t Levels() const { return uint32_t(levelOffsets.size()); }

	// The block ID for a block name: its index in `blocks` plus one, since 0 is air. Nothing for an unknown name.
	std::optional<uint16_t> BlockId(std::string_view aName) const;
	const AtlasBlock*       Block(uint16_t aId) const;  // null for air or an unknown ID
};

// Reads atlas.bin. Nothing, with aError saying why, if it isn't a complete atlas of the expected layout.
std::optional<Atlas> ParseAtlas(std::span<const uint8_t> aBytes, std::string& aError);

constexpr uint32_t kMinecraftAtlasLevels = 5;

// Minecraft's atlas, as SkyCraft sends it (spike 4): RGBA8 rows, top first. Level 0 is the pixels as they are. Each
// further level halves the last, rounding down to at least 1 texel, each texel the rounded average of the 2 x 2 above
// it: an odd side's last row or column is left out, and a side of 1 is used twice. It has no sprites or blocks:
// Minecraft's vertices carry their own UVs. Nothing, with aError saying why, if a side is 0 or aRgba is short.
std::optional<Atlas> AtlasFromRgba(uint32_t aWidth, uint32_t aHeight, std::span<const uint8_t> aRgba, std::string& aError);

// A rectangle of one of an atlas's levels, in that level's texels.
struct AtlasArea
{
	uint32_t level = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t width = 0;
	uint32_t height = 0;
};

// Spike 7's Part 5: an animated sprite's frame into aAtlas, laid out as AtlasFromRgba lays an atlas out: aWidth x aHeight
// RGBA8 pixels, top row first, written at aX aY of its top level, then each level below rebuilt over the patch's area,
// each texel as AtlasFromRgba makes it. The areas it changed come back, one a level from the top, down to the first
// level the patch leaves as it was (where it lies in an odd side's last row or column, which the level below leaves
// out). Nothing changes, and none come back, if the patch doesn't lie inside the top level, aRgba is short, or aAtlas's
// levels aren't where AtlasFromRgba puts them.
std::vector<AtlasArea> PatchAtlas(Atlas& aAtlas, uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight, std::span<const uint8_t> aRgba);

// Part 5: the atlas's areas that patches changed, waiting for the upload list: each once, however often it was patched
// since, as the CPU's atlas holds its newest texels.
class AtlasDirty
{
public:
	void   Add(std::span<const AtlasArea> aAreas);
	bool   Waiting(const AtlasArea& aArea) const;  // patched since the last Take: a patch to it now replaces that one
	bool   Empty() const { return areas_.empty(); }
	size_t Size() const { return areas_.size(); }
	void   Clear() { areas_.clear(); }

	// The waiting areas packed for the patch pass (PatchCS): its table, and their texels from aAtlas, one area after
	// another, row by row. An area that doesn't lie inside its level of aAtlas is left out. At most aMostAreas come, as
	// a dispatch has at most so many groups; the rest wait for the next pass.
	struct Packed
	{
		std::vector<PatchAreaGpu> areas;
		std::vector<uint32_t>     texels;
	};
	Packed Take(const Atlas& aAtlas, size_t aMostAreas = SIZE_MAX);

private:
	std::set<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>> areas_;  // level, x, y, width, height
};
}
