#pragma once

// A small atlas.bin, laid out as tools/build_atlas.py writes it: 8 sprites in a row of 16 x 16 tiles, 5 levels, and
// the blocks stone, grass_block, glass, oak_leaves and oak_log.
#include "render/Atlas.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace fixture
{
inline void Put32(std::vector<uint8_t>& aOut, uint32_t aValue)
{
	for (int i = 0; i < 4; ++i)
	{
		aOut.push_back(uint8_t(aValue >> (8 * i)));
	}
}

inline void PutBlock(std::vector<uint8_t>& aOut, const char* aName, std::initializer_list<uint16_t> aFaces,
                     std::initializer_list<uint8_t> aTint, uint8_t aTintedFaces, bool aCutout)
{
	char name[32]{};
	std::memcpy(name, aName, std::min<size_t>(std::strlen(aName), 31));
	aOut.insert(aOut.end(), name, name + 32);
	for (const uint16_t face : aFaces)
	{
		aOut.push_back(uint8_t(face));
		aOut.push_back(uint8_t(face >> 8));
	}
	aOut.insert(aOut.end(), aTint);
	aOut.push_back(aTintedFaces);
	aOut.push_back(aCutout ? 1 : 0);
	aOut.insert(aOut.end(), 15, 0);
}

inline std::vector<uint8_t> AtlasBytes()
{
	std::vector<uint8_t> out;
	const char           magic[] = "CPMCATL1";
	out.insert(out.end(), magic, magic + 8);
	const uint32_t width = 128, height = 16, levels = 5, sprites = 8, blocks = 5;
	for (const uint32_t value : { width, height, levels, sprites, blocks, 0u })
	{
		Put32(out, value);
	}
	for (uint32_t i = 0; i < sprites; ++i)
	{
		for (const uint32_t value : { i * 16, 0u, 16u, 16u })
		{
			Put32(out, value);
		}
	}
	// Sprites: 0 stone, 1 dirt, 2 grass top, 3 grass side, 4 glass, 5 leaves, 6 log top, 7 log side.
	PutBlock(out, "stone", { 0, 0, 0, 0, 0, 0 }, { 255, 255, 255 }, 0, false);
	PutBlock(out, "grass_block", { 1, 2, 3, 3, 3, 3 }, { 0x91, 0xBD, 0x59 }, 0b000010, false);
	PutBlock(out, "glass", { 4, 4, 4, 4, 4, 4 }, { 255, 255, 255 }, 0, true);
	PutBlock(out, "oak_leaves", { 5, 5, 5, 5, 5, 5 }, { 0x77, 0xAB, 0x2F }, 0b111111, true);
	PutBlock(out, "oak_log", { 6, 6, 7, 7, 7, 7 }, { 255, 255, 255 }, 0, false);
	uint32_t texels = 0;
	for (uint32_t level = 0; level < levels; ++level)
	{
		texels += (width >> level) * (height >> level);
	}
	for (uint32_t i = 0; i < texels; ++i)
	{
		Put32(out, 0xFF000000u | i);  // opaque, each texel different
	}
	return out;
}

inline cpmc::Atlas Atlas()
{
	std::string error;
	return *cpmc::ParseAtlas(AtlasBytes(), error);
}
}
