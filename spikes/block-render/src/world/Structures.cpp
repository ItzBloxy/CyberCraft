#include "world/Structures.h"

#include <cmath>

namespace cpmc
{
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
	aError = "no structure called \"" + std::string(aName) + "\" in this build";
	return std::nullopt;
}
}
