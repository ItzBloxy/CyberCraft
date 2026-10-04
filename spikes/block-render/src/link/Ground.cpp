#include "link/Ground.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
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

// SkyCraft's distance, for its order: height counts double.
int32_t Distance(const RegionPos& aRegion, const RegionPos& aCentre)
{
	const int32_t dx = aRegion.x - aCentre.x, dy = aRegion.y - aCentre.y, dz = aRegion.z - aCentre.z;
	return dx * dx + dz * dz + 2 * dy * dy;
}

// Within the area around aCentre, and aMore regions each way.
bool InArea(const RegionPos& aRegion, const RegionPos& aCentre, int32_t aMore)
{
	return std::abs(aRegion.x - aCentre.x) <= GroundAreas::kAcross + aMore && std::abs(aRegion.z - aCentre.z) <= GroundAreas::kAcross + aMore &&
	       aRegion.y - aCentre.y >= -GroundAreas::kBelow - aMore && aRegion.y - aCentre.y <= GroundAreas::kAbove + aMore;
}

// SkyCraft's regions next to the centre, read again every second: the city streams its collision in as V moves.
bool Near(const RegionPos& aRegion, const RegionPos& aCentre)
{
	return std::abs(aRegion.x - aCentre.x) <= 1 && std::abs(aRegion.z - aCentre.z) <= 1 && aRegion.y - aCentre.y >= -1 &&
	       aRegion.y - aCentre.y <= 0;
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

void GroundAreas::Reset(double aReadyAt)
{
	read_.clear();
	readyAt_ = std::max(readyAt_, aReadyAt);
}

void GroundAreas::Centre(const RegionPos& aCentre)
{
	if (centre_ == aCentre)
	{
		return;
	}
	centre_ = aCentre;
	// Each batch holding a region of the area, ordered by its nearest region, as SkyCraft orders its regions.
	struct Candidate
	{
		Wanted  wanted;
		int32_t distance = 0;
	};
	std::map<BatchPos, Candidate> batches;
	for (int32_t dy = -kBelow; dy <= kAbove; ++dy)
	{
		for (int32_t dz = -kAcross; dz <= kAcross; ++dz)
		{
			for (int32_t dx = -kAcross; dx <= kAcross; ++dx)
			{
				const RegionPos region{ aCentre.x + dx, aCentre.y + dy, aCentre.z + dz };
				const auto [it, added] = batches.try_emplace(BatchOf(region), Candidate{ { BatchOf(region) }, Distance(region, aCentre) });
				it->second.distance = std::min(it->second.distance, Distance(region, aCentre));
				it->second.wanted.nextToCentre = it->second.wanted.nextToCentre || Near(region, aCentre);
			}
		}
	}
	std::vector<Candidate> ordered;
	for (const auto& [batch, candidate] : batches)
	{
		ordered.push_back(candidate);
	}
	std::stable_sort(ordered.begin(), ordered.end(), [](const Candidate& aA, const Candidate& aB) { return aA.distance < aB.distance; });
	wanted_.clear();
	for (const Candidate& candidate : ordered)
	{
		wanted_.push_back(candidate.wanted);
	}
	// Batches left behind are forgotten, so they're read and sent again on return.
	std::erase_if(read_, [&](const auto& aEntry) {
		const std::array<RegionPos, 8> regions = RegionsOf(aEntry.first);
		return std::none_of(regions.begin(), regions.end(), [&](const RegionPos& aRegion) { return InArea(aRegion, aCentre, 1); });
	});
}

std::optional<BatchPos> GroundAreas::Next(double aNow) const
{
	if (!centre_ || aNow < readyAt_)
	{
		return std::nullopt;
	}
	for (const Wanted& wanted : wanted_)
	{
		const auto done = read_.find(wanted.batch);
		if (done == read_.end() || (wanted.nextToCentre && aNow - done->second.at >= kRefresh))
		{
			return wanted.batch;
		}
	}
	return std::nullopt;
}

std::vector<size_t> GroundAreas::Read(const BatchPos& aBatch, double aNow, const std::array<std::vector<McTriangle>, 8>& aFiled)
{
	const auto [done, first] = read_.try_emplace(aBatch);
	done->second.at = aNow;
	std::vector<size_t> send;
	for (size_t i = 0; i < aFiled.size(); ++i)
	{
		const uint64_t hash = TrianglesHash(aFiled[i]);
		if (first || done->second.hashes[i] != hash)
		{
			send.push_back(i);
		}
		done->second.hashes[i] = hash;
	}
	return send;
}

size_t GroundAreas::Waiting() const
{
	return size_t(std::count_if(wanted_.begin(), wanted_.end(), [&](const Wanted& aWanted) { return !read_.contains(aWanted.batch); }));
}
}
