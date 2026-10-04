#include "link/Voxels.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace cpmc::link
{
namespace
{
constexpr int   kGrid = kRegionBlocks * 8;  // sub-voxels along a region's edge: 64, one bit each in a row
constexpr float kSteepMin = 0.1f;            // |n.y| below this is a wall: kept fine-grained
constexpr float kSteepMax = 0.643f;          // |n.y| below this (steeper than about 50 degrees) is coarsened

void Sub(const float* aA, const float* aB, float* aOut)
{
	aOut[0] = aA[0] - aB[0];
	aOut[1] = aA[1] - aB[1];
	aOut[2] = aA[2] - aB[2];
}

void Cross(const float* aA, const float* aB, float* aOut)
{
	aOut[0] = aA[1] * aB[2] - aA[2] * aB[1];
	aOut[1] = aA[2] * aB[0] - aA[0] * aB[2];
	aOut[2] = aA[0] * aB[1] - aA[1] * aB[0];
}

float Dot(const float* aA, const float* aB)
{
	return aA[0] * aB[0] + aA[1] * aB[1] + aA[2] * aB[2];
}

bool AxisTest(const float* aV0, const float* aV1, const float* aV2, const float* aAxis, float aHalf)
{
	const float p0 = Dot(aV0, aAxis), p1 = Dot(aV1, aAxis), p2 = Dot(aV2, aAxis);
	const float low = std::min({ p0, p1, p2 }), high = std::max({ p0, p1, p2 });
	const float r = aHalf * (std::fabs(aAxis[0]) + std::fabs(aAxis[1]) + std::fabs(aAxis[2]));
	return !(low > r || high < -r);
}

// The box centred at aCentre with half-size aHalf on every axis, against the triangle aA, aB, aC with face normal aN.
bool TriBoxOverlap(const float* aCentre, float aHalf, const float* aA, const float* aB, const float* aC, const float* aN)
{
	float v0[3], v1[3], v2[3];
	Sub(aA, aCentre, v0);
	Sub(aB, aCentre, v1);
	Sub(aC, aCentre, v2);
	for (int i = 0; i < 3; ++i)
	{
		const float low = std::min({ v0[i], v1[i], v2[i] }), high = std::max({ v0[i], v1[i], v2[i] });
		if (low > aHalf || high < -aHalf)
		{
			return false;
		}
	}
	const float d = Dot(aN, v0);
	const float r = aHalf * (std::fabs(aN[0]) + std::fabs(aN[1]) + std::fabs(aN[2]));
	if (std::fabs(d) > r)
	{
		return false;
	}
	float edges[3][3];
	Sub(v1, v0, edges[0]);
	Sub(v2, v1, edges[1]);
	Sub(v0, v2, edges[2]);
	static constexpr float kAxes[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	for (const auto& edge : edges)
	{
		for (const auto& unit : kAxes)
		{
			float axis[3];
			Cross(edge, unit, axis);
			if (!AxisTest(v0, v1, v2, axis, aHalf))
			{
				return false;
			}
		}
	}
	return true;
}

// A region message's header: the region's inclusive block box, the epoch and the count.
proto::ColRegion Header(int32_t aRegionX, int32_t aRegionY, int32_t aRegionZ, uint32_t aEpoch, size_t aCount)
{
	return { aRegionX * kRegionBlocks,
		     aRegionY * kRegionBlocks,
		     aRegionZ * kRegionBlocks,
		     aRegionX * kRegionBlocks + kRegionBlocks - 1,
		     aRegionY * kRegionBlocks + kRegionBlocks - 1,
		     aRegionZ * kRegionBlocks + kRegionBlocks - 1,
		     aEpoch,
		     uint32_t(aCount) };
}
}

std::vector<proto::ColBlock> VoxelizeRegion(int32_t aRegionX, int32_t aRegionY, int32_t aRegionZ, std::span<const McTriangle> aTriangles)
{
	// grid[y * kGrid + z] holds a row of 64 sub-voxels along x.
	std::vector<uint64_t> solid(kGrid * kGrid, 0), steep(kGrid * kGrid, 0);
	const double          origin[3] = { double(aRegionX) * kRegionBlocks, double(aRegionY) * kRegionBlocks, double(aRegionZ) * kRegionBlocks };
	const auto            toVoxels = [&](const float* aMc, float* aOut) {
		for (int i = 0; i < 3; ++i)
		{
			aOut[i] = float((double(aMc[i]) - origin[i]) * 8.0);
		}
	};
	const auto low = [](float aValue) { return std::clamp(int(std::floor(aValue)), 0, kGrid - 1); };
	const auto high = [](float aValue) { return std::clamp(int(std::ceil(aValue)) - 1, 0, kGrid - 1); };

	for (const McTriangle& triangle : aTriangles)
	{
		float a[3], b[3], c[3];
		toVoxels(triangle.v, a);
		toVoxels(triangle.v + 3, b);
		toVoxels(triangle.v + 6, c);
		float lo[3], hi[3];
		for (int i = 0; i < 3; ++i)
		{
			lo[i] = std::min({ a[i], b[i], c[i] });
			hi[i] = std::max({ a[i], b[i], c[i] });
		}
		if (hi[0] < 0 || hi[1] < 0 || hi[2] < 0 || lo[0] > kGrid || lo[1] > kGrid || lo[2] > kGrid)
		{
			continue;
		}
		float e1[3], e2[3], n[3];
		Sub(b, a, e1);
		Sub(c, a, e2);
		Cross(e1, e2, n);
		const float length = std::sqrt(Dot(n, n));
		if (length < 1e-9f)
		{
			continue;
		}
		n[0] /= length, n[1] /= length, n[2] /= length;
		const float            ny = std::fabs(n[1]);
		std::vector<uint64_t>& grid = ny >= kSteepMax || ny < kSteepMin ? solid : steep;

		// Plane-guided: for each column along the normal's dominant axis, only the sub-voxels near the plane are
		// tested, so a big triangle costs its area, not the volume of its bounds.
		int dominant = 0;
		if (std::fabs(n[1]) > std::fabs(n[dominant]))
		{
			dominant = 1;
		}
		if (std::fabs(n[2]) > std::fabs(n[dominant]))
		{
			dominant = 2;
		}
		const int   u = (dominant + 1) % 3, v = (dominant + 2) % 3;
		const float d = Dot(n, a);
		const float r = 0.5f * (std::fabs(n[0]) + std::fabs(n[1]) + std::fabs(n[2]));
		// The sub-voxels along the dominant axis that the triangle spans. One lying exactly on a boundary between two
		// (a floor at a whole metre) spans none, so it tests the two it touches. SkyCraft's own drops it, which Skyrim's
		// coordinates rarely hit; Night City's geometry is often on whole metres.
		int domLow = low(lo[dominant]), domHigh = high(hi[dominant]);
		if (domHigh < domLow)
		{
			domLow = std::clamp(int(std::ceil(lo[dominant])) - 1, 0, kGrid - 1);
			domHigh = std::clamp(int(std::floor(hi[dominant])), 0, kGrid - 1);
		}
		for (int iu = low(lo[u]); iu <= high(hi[u]); ++iu)
		{
			for (int iv = low(lo[v]); iv <= high(hi[v]); ++iv)
			{
				const float cu = iu + 0.5f, cv = iv + 0.5f;
				const float s0 = (d - r - n[u] * cu - n[v] * cv) / n[dominant];
				const float s1 = (d + r - n[u] * cu - n[v] * cv) / n[dominant];
				const int   first = std::max(domLow, int(std::floor(std::min(s0, s1) - 0.5f)));
				const int   last = std::min(domHigh, int(std::ceil(std::max(s0, s1) - 0.5f)));
				for (int id = first; id <= last; ++id)
				{
					float centre[3];
					centre[dominant] = id + 0.5f;
					centre[u] = cu;
					centre[v] = cv;
					if (TriBoxOverlap(centre, 0.5f, a, b, c, n))
					{
						int at[3];
						at[dominant] = id, at[u] = iu, at[v] = iv;
						grid[size_t(at[1]) * kGrid + size_t(at[2])] |= 1ull << at[0];
					}
				}
			}
		}
	}

	// Steep surfaces fill their blocks' whole footprint between their lowest and highest sub-voxel layers.
	for (int by = 0; by < kRegionBlocks; ++by)
	{
		for (int bz = 0; bz < kRegionBlocks; ++bz)
		{
			for (int bx = 0; bx < kRegionBlocks; ++bx)
			{
				const uint64_t columns = 0xFFull << (bx * 8);
				int            lowest = kGrid, highest = -1;
				for (int y = by * 8; y < by * 8 + 8; ++y)
				{
					for (int z = bz * 8; z < bz * 8 + 8; ++z)
					{
						if (steep[size_t(y) * kGrid + size_t(z)] & columns)
						{
							lowest = std::min(lowest, y);
							highest = std::max(highest, y);
						}
					}
				}
				for (int y = lowest; y <= highest; ++y)
				{
					for (int z = bz * 8; z < bz * 8 + 8; ++z)
					{
						solid[size_t(y) * kGrid + size_t(z)] |= columns;
					}
				}
			}
		}
	}

	std::vector<proto::ColBlock> blocks;
	for (int by = 0; by < kRegionBlocks; ++by)
	{
		for (int bz = 0; bz < kRegionBlocks; ++bz)
		{
			for (int bx = 0; bx < kRegionBlocks; ++bx)
			{
				proto::ColBlock block{};
				bool            any = false;
				for (int sy = 0; sy < 8; ++sy)
				{
					uint64_t layer = 0;
					for (int sz = 0; sz < 8; ++sz)
					{
						const uint64_t row = (solid[size_t(by * 8 + sy) * kGrid + size_t(bz * 8 + sz)] >> (bx * 8)) & 0xFF;
						layer |= row << (sz * 8);
					}
					block.bits[sy] = layer;
					any = any || layer != 0;
				}
				if (any)
				{
					block.x = aRegionX * kRegionBlocks + bx;
					block.y = aRegionY * kRegionBlocks + by;
					block.z = aRegionZ * kRegionBlocks + bz;
					blocks.push_back(block);
				}
			}
		}
	}
	return blocks;
}

std::vector<uint8_t> RegionPayload(int32_t aRegionX, int32_t aRegionY, int32_t aRegionZ, uint32_t aEpoch,
                                   std::span<const proto::ColBlock> aBlocks)
{
	const proto::ColRegion header = Header(aRegionX, aRegionY, aRegionZ, aEpoch, aBlocks.size());
	std::vector<uint8_t>   payload(sizeof header + aBlocks.size_bytes());
	std::memcpy(payload.data(), &header, sizeof header);
	if (!aBlocks.empty())
	{
		std::memcpy(payload.data() + sizeof header, aBlocks.data(), aBlocks.size_bytes());
	}
	return payload;
}

std::vector<uint8_t> TrisPayload(int32_t aRegionX, int32_t aRegionY, int32_t aRegionZ, uint32_t aEpoch,
                                 std::span<const McTriangle> aTriangles)
{
	const proto::ColRegion header = Header(aRegionX, aRegionY, aRegionZ, aEpoch, aTriangles.size());
	std::vector<uint8_t>   payload(sizeof header + aTriangles.size() * sizeof(proto::ColTri));
	std::memcpy(payload.data(), &header, sizeof header);
	for (size_t i = 0; i < aTriangles.size(); ++i)
	{
		proto::ColTri tri{};
		std::memcpy(tri.v, aTriangles[i].v, sizeof tri.v);
		std::memcpy(payload.data() + sizeof header + i * sizeof tri, &tri, sizeof tri);
	}
	return payload;
}
}
