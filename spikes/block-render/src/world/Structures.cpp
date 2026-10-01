#include "world/Structures.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

namespace cpmc
{
namespace
{
// The IDs of aNames in the atlas, in order. Nothing, with aError naming the first one missing.
std::optional<std::vector<uint16_t>> BlockIds(const Atlas& aAtlas, std::initializer_list<const char*> aNames, std::string& aError)
{
	std::vector<uint16_t> ids;
	for (const char* name : aNames)
	{
		const std::optional<uint16_t> id = aAtlas.BlockId(name);
		if (!id)
		{
			aError = std::string("the atlas has no ") + name;
			return std::nullopt;
		}
		ids.push_back(*id);
	}
	return ids;
}

// 7 x 7 and 5 high, with its doorway in the wall at the lowest z, and the bush beside the wall at the highest x.
std::optional<Placed> PlaceHouse(BlockWorld& aWorld, const Atlas& aAtlas, const BlockPos& aOrigin, std::string& aError)
{
	const auto ids = BlockIds(aAtlas, { "cobblestone", "oak_planks", "oak_log", "glass", "oak_leaves" }, aError);
	if (!ids)
	{
		return std::nullopt;
	}
	const uint16_t cobblestone = (*ids)[0], planks = (*ids)[1], log = (*ids)[2], glass = (*ids)[3], leaves = (*ids)[4];
	Placed placed;
	placed.min = aOrigin;
	placed.max = { aOrigin.x + 9, aOrigin.y + 4, aOrigin.z + 6 };
	const auto set = [&](int32_t aX, int32_t aY, int32_t aZ, uint16_t aBlock) {
		aWorld.Set({ aOrigin.x + aX, aOrigin.y + aY, aOrigin.z + aZ }, aBlock);
		++placed.blocks;
	};
	for (int32_t x = 0; x < 7; ++x)
	{
		for (int32_t z = 0; z < 7; ++z)
		{
			set(x, 0, z, cobblestone);  // the floor
			set(x, 4, z, planks);       // the roof
			const bool edgeX = x == 0 || x == 6;
			const bool edgeZ = z == 0 || z == 6;
			if (!edgeX && !edgeZ)
			{
				continue;  // inside
			}
			for (int32_t y = 1; y <= 3; ++y)
			{
				const bool doorway = z == 0 && x == 3 && y <= 2;
				const bool window = y == 2 && ((z == 0 && x == 5) || (z == 6 && x == 3) || (edgeX && z == 3));
				if (!doorway)
				{
					set(x, y, z, edgeX && edgeZ ? log : window ? glass : planks);
				}
			}
		}
	}
	for (int32_t x = 8; x <= 9; ++x)
	{
		for (int32_t y = 0; y <= 1; ++y)
		{
			for (int32_t z = 2; z <= 3; ++z)
			{
				set(x, y, z, leaves);
			}
		}
	}
	return placed;
}
}

BlockPos PlacementOrigin(const Vec3d& aFeet, const Vec3f& aForward)
{
	double       x = aForward.x, y = aForward.y;
	const double length = std::sqrt(x * x + y * y);
	if (length > 1e-6)
	{
		x /= length;
		y /= length;
	}
	else
	{
		x = 0.0;
		y = 1.0;  // looking straight up or down: place it to the north
	}
	const Vec3d ahead = McFromWorld({ aFeet.x + 3.0 * x, aFeet.y + 3.0 * y, aFeet.z });
	return { int32_t(std::floor(ahead.x)), int32_t(std::floor(ahead.y)), int32_t(std::floor(ahead.z)) };
}

std::pair<Vec3d, Vec3d> WorldBox(const Placed& aPlaced)
{
	// Minecraft block (x, y, z) fills world x..x+1, -z-1..-z, y..y+1.
	return { { double(aPlaced.min.x), double(-aPlaced.max.z - 1), double(aPlaced.min.y) },
	         { double(aPlaced.max.x + 1), double(-aPlaced.min.z), double(aPlaced.max.y + 1) } };
}

std::optional<Placed> PlaceStructure(BlockWorld& aWorld, const Atlas& aAtlas, std::string_view aName, const BlockPos& aOrigin,
                                     std::string& aError)
{
	const std::optional<uint16_t> stone = aAtlas.BlockId("stone");
	if (!stone)
	{
		aError = "the atlas has no stone";
		return std::nullopt;
	}
	Placed placed;
	placed.min = aOrigin;
	if (aName == "cube")
	{
		aWorld.Set(aOrigin, *stone);
		placed.blocks = 1;
		placed.max = aOrigin;
		return placed;
	}
	if (aName == "sampler")
	{
		const auto count = int32_t(aAtlas.blocks.size());
		for (int32_t i = 0; i < count; ++i)
		{
			aWorld.Set({ aOrigin.x + 2 * i, aOrigin.y, aOrigin.z }, *stone);
			aWorld.Set({ aOrigin.x + 2 * i + 1, aOrigin.y, aOrigin.z }, *stone);
			aWorld.Set({ aOrigin.x + 2 * i, aOrigin.y + 1, aOrigin.z }, uint16_t(i + 1));
		}
		placed.blocks = size_t(3 * count);
		placed.max = { aOrigin.x + 2 * count - 1, aOrigin.y + 1, aOrigin.z };
		return placed;
	}
	if (aName == "house")
	{
		return PlaceHouse(aWorld, aAtlas, aOrigin, aError);
	}
	aError = "no structure called \"" + std::string(aName) + "\" in this build";
	return std::nullopt;
}

std::optional<Placed> PlaceField(BlockWorld& aWorld, const Atlas& aAtlas, const BlockPos& aOrigin, size_t aBlocks, std::string& aError)
{
	const auto ids = BlockIds(aAtlas, { "grass_block", "dirt", "stone" }, aError);
	if (!ids)
	{
		return std::nullopt;
	}
	if (aBlocks == 0)
	{
		aError = "a field needs at least one block";
		return std::nullopt;
	}
	const uint16_t grass = (*ids)[0], dirt = (*ids)[1], stone = (*ids)[2];
	// Rows of columns along x, as many columns to a row as there would be rows if each column held its average of 6
	// blocks, until the field holds aBlocks.
	const auto width = int32_t(std::ceil(std::sqrt(double(aBlocks) / 6.0)));
	Placed     placed;
	placed.min = aOrigin;
	placed.max = aOrigin;
	for (int32_t z = 0; placed.blocks < aBlocks; ++z)
	{
		for (int32_t x = 0; x < width && placed.blocks < aBlocks; ++x)
		{
			// 2 to 10 blocks high, rolling: grass on top, then three of dirt, then stone.
			const auto height = int32_t(6 + std::lround(2.0 * std::sin(x / 5.0) + 2.0 * std::cos(z / 7.0)));
			for (int32_t y = 0; y < height; ++y)
			{
				aWorld.Set({ aOrigin.x + x, aOrigin.y + y, aOrigin.z + z }, y == height - 1 ? grass : y >= height - 4 ? dirt : stone);
			}
			placed.blocks += size_t(height);
			placed.max = { std::max(placed.max.x, aOrigin.x + x), std::max(placed.max.y, aOrigin.y + height - 1), aOrigin.z + z };
		}
	}
	return placed;
}
}
