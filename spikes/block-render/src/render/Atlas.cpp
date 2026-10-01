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
}
