#include "link/Ground.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace cpmc::link
{
namespace
{
constexpr double kFarthest = 1.0e6;  // blocks: a coordinate farther out is garbage, as SkyCraft takes it

// aValue / aBy, rounded down.
int32_t FloorDiv(int32_t aValue, int32_t aBy)
{
	return aValue >= 0 ? aValue / aBy : -((-aValue + aBy - 1) / aBy);
}

bool Usable(const McTriangle& aTriangle)
{
	return std::all_of(std::begin(aTriangle.v), std::end(aTriangle.v),
	                   [](float aValue) { return std::isfinite(aValue) && std::fabs(aValue) <= kFarthest; });
}

uint64_t Fnv1a(const void* aData, size_t aBytes)
{
	uint64_t hash = 14695981039346656037ull;
	for (size_t i = 0; i < aBytes; ++i)
	{
		hash = (hash ^ static_cast<const uint8_t*>(aData)[i]) * 1099511628211ull;
	}
	return hash;
}
}

std::optional<RegionPos> RegionAt(const Vec3d& aMc)
{
	const double at[3] = { aMc.x, aMc.y, aMc.z };
	if (!std::all_of(std::begin(at), std::end(at), [](double aValue) { return std::isfinite(aValue) && std::fabs(aValue) <= kFarthest; }))
	{
		return std::nullopt;
	}
	return RegionPos{ int32_t(std::floor(aMc.x / kRegionBlocks)), int32_t(std::floor(aMc.y / kRegionBlocks)),
		              int32_t(std::floor(aMc.z / kRegionBlocks)) };
}

BatchPos BatchOf(const RegionPos& aRegion)
{
	return { FloorDiv(aRegion.x, 2), FloorDiv(aRegion.y, 2), FloorDiv(aRegion.z, 2) };
}

std::array<RegionPos, 8> RegionsOf(const BatchPos& aBatch)
{
	std::array<RegionPos, 8> regions;
	size_t                   i = 0;
	for (int32_t y = 0; y < 2; ++y)
	{
		for (int32_t z = 0; z < 2; ++z)
		{
			for (int32_t x = 0; x < 2; ++x)
			{
				regions[i++] = { aBatch.x * 2 + x, aBatch.y * 2 + y, aBatch.z * 2 + z };
			}
		}
	}
	return regions;
}

McBox BatchBox(const BatchPos& aBatch, double aMargin)
{
	constexpr double kSide = 2.0 * kRegionBlocks;
	const Vec3d      low{ aBatch.x * kSide - aMargin, aBatch.y * kSide - aMargin, aBatch.z * kSide - aMargin };
	return { low, { low.x + kSide + 2.0 * aMargin, low.y + kSide + 2.0 * aMargin, low.z + kSide + 2.0 * aMargin } };
}

std::array<std::vector<McTriangle>, 8> FileBatch(const BatchPos& aBatch, std::span<const McTriangle> aTriangles)
{
	const std::array<RegionPos, 8>         regions = RegionsOf(aBatch);
	std::array<std::vector<McTriangle>, 8> filed;
	for (const McTriangle& triangle : aTriangles)
	{
		if (!Usable(triangle))
		{
			continue;
		}
		double low[3], high[3];
		for (int k = 0; k < 3; ++k)
		{
			low[k] = std::min({ triangle.v[k], triangle.v[3 + k], triangle.v[6 + k] });
			high[k] = std::max({ triangle.v[k], triangle.v[3 + k], triangle.v[6 + k] });
		}
		for (size_t i = 0; i < regions.size(); ++i)
		{
			const double corner[3] = { double(regions[i].x) * kRegionBlocks, double(regions[i].y) * kRegionBlocks,
				                       double(regions[i].z) * kRegionBlocks };
			bool         touches = true;
			for (int k = 0; k < 3 && touches; ++k)
			{
				touches = high[k] >= corner[k] - kFileMargin && low[k] <= corner[k] + kRegionBlocks + kFileMargin;
			}
			if (touches)
			{
				filed[i].push_back(triangle);
			}
		}
	}
	return filed;
}

uint64_t TrianglesHash(std::span<const McTriangle> aTriangles)
{
	// A sum of each triangle's hash: the same whatever the order.
	uint64_t hash = aTriangles.size();
	for (const McTriangle& triangle : aTriangles)
	{
		hash += Fnv1a(triangle.v, sizeof triangle.v);
	}
	return hash;
}
}
