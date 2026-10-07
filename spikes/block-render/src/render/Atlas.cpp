#include "render/Atlas.h"

#include <algorithm>
#include <cstring>
#include <format>

namespace cpmc
{
namespace
{
constexpr size_t   kHeaderBytes = 32;
constexpr size_t   kSpriteBytes = 16;
constexpr size_t   kBlockBytes = 64;
constexpr uint32_t kMaxLevels = 8;

uint32_t U32(std::span<const uint8_t> aBytes, size_t aAt)
{
	uint32_t value = 0;
	std::memcpy(&value, aBytes.data() + aAt, sizeof(value));
	return value;
}

uint16_t U16(std::span<const uint8_t> aBytes, size_t aAt)
{
	uint16_t value = 0;
	std::memcpy(&value, aBytes.data() + aAt, sizeof(value));
	return value;
}

std::optional<Atlas> Fail(std::string& aError, std::string aWhy)
{
	aError = std::move(aWhy);
	return std::nullopt;
}

// Four RGBA8 texels averaged channel by channel, rounded to nearest.
uint32_t Average(uint32_t aA, uint32_t aB, uint32_t aC, uint32_t aD)
{
	uint32_t out = 0;
	for (uint32_t shift = 0; shift < 32; shift += 8)
	{
		const uint32_t sum = ((aA >> shift) & 0xFF) + ((aB >> shift) & 0xFF) + ((aC >> shift) & 0xFF) + ((aD >> shift) & 0xFF);
		out |= ((sum + 2) / 4) << shift;
	}
	return out;
}

// A side at aLevel, as AtlasFromRgba lays the levels out: half the level above's, rounded down, at least 1.
uint32_t LevelSide(uint32_t aSide, uint32_t aLevel)
{
	return std::max(1u, aSide >> aLevel);
}

// aLevel's texels from aX0 to aX1 and aY0 to aY1, each the rounded average of the 2 x 2 above it: an odd side's last row
// or column is left out, and a side of 1 is used twice.
void Downsample(Atlas& aAtlas, uint32_t aLevel, uint32_t aX0, uint32_t aY0, uint32_t aX1, uint32_t aY1)
{
	const uint32_t  aboveWidth = LevelSide(aAtlas.width, aLevel - 1);
	const uint32_t  aboveHeight = LevelSide(aAtlas.height, aLevel - 1);
	const uint32_t  width = LevelSide(aAtlas.width, aLevel);
	const uint32_t* above = aAtlas.texels.data() + aAtlas.levelOffsets[aLevel - 1];
	uint32_t*       out = aAtlas.texels.data() + aAtlas.levelOffsets[aLevel];
	for (uint32_t y = aY0; y <= aY1; ++y)
	{
		const size_t row0 = size_t(y * 2) * aboveWidth;
		const size_t row1 = size_t(std::min(y * 2 + 1, aboveHeight - 1)) * aboveWidth;
		for (uint32_t x = aX0; x <= aX1; ++x)
		{
			const uint32_t x0 = x * 2;
			const uint32_t x1 = std::min(x0 + 1, aboveWidth - 1);
			out[size_t(y) * width + x] = Average(above[row0 + x0], above[row0 + x1], above[row1 + x0], above[row1 + x1]);
		}
	}
}
}

std::optional<uint16_t> Atlas::BlockId(std::string_view aName) const
{
	for (size_t i = 0; i < blocks.size(); ++i)
	{
		if (blocks[i].name == aName)
		{
			return uint16_t(i + 1);
		}
	}
	return std::nullopt;
}

const AtlasBlock* Atlas::Block(uint16_t aId) const
{
	return aId >= 1 && aId <= blocks.size() ? &blocks[aId - 1] : nullptr;
}

std::optional<Atlas> ParseAtlas(std::span<const uint8_t> aBytes, std::string& aError)
{
	if (aBytes.size() < kHeaderBytes || std::memcmp(aBytes.data(), "CPMCATL1", 8) != 0)
	{
		return Fail(aError, "not an atlas: no CPMCATL1 header");
	}
	Atlas atlas;
	atlas.width = U32(aBytes, 8);
	atlas.height = U32(aBytes, 12);
	const uint32_t levels = U32(aBytes, 16);
	const uint32_t spriteCount = U32(aBytes, 20);
	const uint32_t blockCount = U32(aBytes, 24);
	if (atlas.width == 0 || atlas.height == 0 || atlas.width > 4096 || atlas.height > 4096 || levels == 0 ||
	    levels > kMaxLevels || spriteCount == 0 || spriteCount > 4096 || blockCount == 0 || blockCount > 4096)
	{
		return Fail(aError, std::format("atlas header out of range: {} x {}, {} levels, {} sprites, {} blocks", atlas.width,
		                                atlas.height, levels, spriteCount, blockCount));
	}
	size_t texelCount = 0;
	for (uint32_t level = 0; level < levels; ++level)
	{
		atlas.levelOffsets.push_back(uint32_t(texelCount));
		texelCount += size_t(std::max(1u, atlas.width >> level)) * std::max(1u, atlas.height >> level);
	}
	const size_t expected = kHeaderBytes + spriteCount * kSpriteBytes + blockCount * kBlockBytes + texelCount * 4;
	if (aBytes.size() != expected)
	{
		return Fail(aError, std::format("atlas is {} bytes; its header says {}", aBytes.size(), expected));
	}
	size_t at = kHeaderBytes;
	for (uint32_t i = 0; i < spriteCount; ++i, at += kSpriteBytes)
	{
		AtlasSprite sprite{ U32(aBytes, at), U32(aBytes, at + 4), U32(aBytes, at + 8), U32(aBytes, at + 12) };
		if (sprite.w == 0 || sprite.h == 0 || sprite.x + sprite.w > atlas.width || sprite.y + sprite.h > atlas.height)
		{
			return Fail(aError, std::format("sprite {} lies outside the atlas", i));
		}
		atlas.sprites.push_back(sprite);
	}
	for (uint32_t i = 0; i < blockCount; ++i, at += kBlockBytes)
	{
		AtlasBlock block;
		const char* name = reinterpret_cast<const char*>(aBytes.data() + at);
		block.name.assign(name, strnlen(name, 32));
		for (size_t face = 0; face < 6; ++face)
		{
			block.faces[face] = U16(aBytes, at + 32 + face * 2);
			if (block.faces[face] >= spriteCount)
			{
				return Fail(aError, std::format("block {} uses sprite {}, past the {} sprites", block.name, block.faces[face], spriteCount));
			}
		}
		block.tint = { aBytes[at + 44], aBytes[at + 45], aBytes[at + 46] };
		block.tintedFaces = aBytes[at + 47];
		block.cutout = aBytes[at + 48] != 0;
		atlas.blocks.push_back(std::move(block));
	}
	atlas.texels.resize(texelCount);
	std::memcpy(atlas.texels.data(), aBytes.data() + at, texelCount * 4);
	return atlas;
}

std::optional<Atlas> AtlasFromRgba(uint32_t aWidth, uint32_t aHeight, std::span<const uint8_t> aRgba, std::string& aError)
{
	const uint64_t bytes = uint64_t(aWidth) * aHeight * 4;
	if (aWidth == 0 || aHeight == 0 || aRgba.size() < bytes)
	{
		return Fail(aError, std::format("a {} x {} atlas needs {} bytes of pixels, and has {}", aWidth, aHeight, bytes, aRgba.size()));
	}
	Atlas  atlas;
	size_t texelCount = 0;
	atlas.width = aWidth;
	atlas.height = aHeight;
	for (uint32_t level = 0; level < kMinecraftAtlasLevels; ++level)
	{
		atlas.levelOffsets.push_back(uint32_t(texelCount));
		texelCount += size_t(LevelSide(aWidth, level)) * LevelSide(aHeight, level);
	}
	atlas.texels.resize(texelCount);
	std::memcpy(atlas.texels.data(), aRgba.data(), size_t(bytes));
	for (uint32_t level = 1; level < kMinecraftAtlasLevels; ++level)
	{
		Downsample(atlas, level, 0, 0, LevelSide(aWidth, level) - 1, LevelSide(aHeight, level) - 1);
	}
	return atlas;
}

std::vector<AtlasArea> PatchAtlas(Atlas& aAtlas, uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight, std::span<const uint8_t> aRgba)
{
	size_t texelCount = 0;  // every level where AtlasFromRgba puts it, and nothing past the last
	for (uint32_t level = 0; level < aAtlas.Levels(); ++level)
	{
		if (aAtlas.levelOffsets[level] != texelCount)
		{
			return {};
		}
		texelCount += size_t(LevelSide(aAtlas.width, level)) * LevelSide(aAtlas.height, level);
	}
	if (aAtlas.Levels() == 0 || aAtlas.texels.size() != texelCount || aWidth == 0 || aHeight == 0 ||
	    uint64_t(aX) + aWidth > aAtlas.width || uint64_t(aY) + aHeight > aAtlas.height || aRgba.size() < uint64_t(aWidth) * aHeight * 4)
	{
		return {};
	}
	for (uint32_t row = 0; row < aHeight; ++row)
	{
		std::memcpy(aAtlas.texels.data() + aAtlas.levelOffsets[0] + size_t(aY + row) * aAtlas.width + aX, aRgba.data() + size_t(row) * aWidth * 4,
		            size_t(aWidth) * 4);
	}
	std::vector<AtlasArea> areas{ { 0, aX, aY, aWidth, aHeight } };
	const uint32_t         right = aX + aWidth - 1;
	const uint32_t         bottom = aY + aHeight - 1;
	for (uint32_t level = 1; level < aAtlas.Levels(); ++level)
	{
		const uint32_t width = LevelSide(aAtlas.width, level);
		const uint32_t height = LevelSide(aAtlas.height, level);
		const uint32_t x0 = aX >> level;
		const uint32_t y0 = aY >> level;
		if (x0 >= width || y0 >= height)
		{
			break;  // the patch lies in the row or column the level leaves out: it, and those below, are as they were
		}
		const uint32_t x1 = std::min(right >> level, width - 1);
		const uint32_t y1 = std::min(bottom >> level, height - 1);
		Downsample(aAtlas, level, x0, y0, x1, y1);
		areas.push_back({ level, x0, y0, x1 - x0 + 1, y1 - y0 + 1 });
	}
	return areas;
}

void AtlasDirty::Add(std::span<const AtlasArea> aAreas)
{
	for (const AtlasArea& area : aAreas)
	{
		areas_.insert({ area.level, area.x, area.y, area.width, area.height });
	}
}

bool AtlasDirty::Waiting(const AtlasArea& aArea) const
{
	return areas_.contains({ aArea.level, aArea.x, aArea.y, aArea.width, aArea.height });
}

AtlasDirty::Packed AtlasDirty::Take(const Atlas& aAtlas, size_t aMostAreas)
{
	Packed packed;
	auto   next = areas_.begin();
	for (; next != areas_.end() && packed.areas.size() < aMostAreas; ++next)
	{
		const auto& [level, x, y, width, height] = *next;
		if (level >= aAtlas.Levels())
		{
			continue;
		}
		const uint32_t levelWidth = LevelSide(aAtlas.width, level);
		const uint32_t levelHeight = LevelSide(aAtlas.height, level);
		const uint64_t start = uint64_t(aAtlas.levelOffsets[level]) + uint64_t(y) * levelWidth + x;
		if (width == 0 || height == 0 || uint64_t(x) + width > levelWidth || uint64_t(y) + height > levelHeight ||
		    start + uint64_t(height - 1) * levelWidth + width > aAtlas.texels.size())
		{
			continue;
		}
		PatchAreaGpu area;
		area.start = uint32_t(start);
		area.stride = levelWidth;
		area.width = width;
		area.height = height;
		area.source = uint32_t(packed.texels.size());
		for (uint32_t row = 0; row < height; ++row)
		{
			const uint32_t* from = aAtlas.texels.data() + start + size_t(row) * levelWidth;
			packed.texels.insert(packed.texels.end(), from, from + width);
		}
		packed.areas.push_back(area);
	}
	areas_.erase(areas_.begin(), next);
	return packed;
}
}
