#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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
}
