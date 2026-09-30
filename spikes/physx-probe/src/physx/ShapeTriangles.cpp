#include "physx/ShapeTriangles.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

using namespace physx;

namespace cpmc::px
{
namespace
{
constexpr uint32_t kSegments = 8;
constexpr uint32_t kRings = 4;
constexpr float    kPi = 3.14159265358979f;

using Ring = std::array<PxVec3, kSegments>;

void Push(std::vector<Tri>& aOut, const PxVec3& aA, const PxVec3& aB, const PxVec3& aC)
{
	aOut.push_back({ { aA, aB, aC } });
}

// Latitude ring aLatitude (1 .. kRings-1) of a sphere whose poles are on X, shifted along X.
Ring MakeRing(uint32_t aLatitude, float aRadius, float aShiftX)
{
	Ring ring{};
	const float theta = kPi * float(aLatitude) / float(kRings);
	for (uint32_t j = 0; j < kSegments; ++j)
	{
		const float phi = 2.0f * kPi * float(j) / float(kSegments);
		ring[j] = PxVec3(std::cos(theta) * aRadius + aShiftX, std::sin(theta) * std::cos(phi) * aRadius,
		                 std::sin(theta) * std::sin(phi) * aRadius);
	}
	return ring;
}

// Quads between ring aA and ring aB, where aA is nearer the +X pole.
void Band(const Ring& aA, const Ring& aB, std::vector<Tri>& aOut)
{
	for (uint32_t j = 0; j < kSegments; ++j)
	{
		const uint32_t k = (j + 1) % kSegments;
		Push(aOut, aA[j], aB[j], aB[k]);
		Push(aOut, aA[j], aB[k], aA[k]);
	}
}

void CapPositive(const PxVec3& aPole, const Ring& aRing, std::vector<Tri>& aOut)
{
	for (uint32_t j = 0; j < kSegments; ++j)
	{
		Push(aOut, aPole, aRing[j], aRing[(j + 1) % kSegments]);
	}
}

void CapNegative(const PxVec3& aPole, const Ring& aRing, std::vector<Tri>& aOut)
{
	for (uint32_t j = 0; j < kSegments; ++j)
	{
		Push(aOut, aPole, aRing[(j + 1) % kSegments], aRing[j]);
	}
}

// Two hemispheres at +-aHalfHeight on X, joined by a cylinder when aHalfHeight > 0.
void Rounded(float aRadius, float aHalfHeight, const PxTransform& aPose, std::vector<Tri>& aOut)
{
	const size_t first = aOut.size();
	const Ring   ring1 = MakeRing(1, aRadius, aHalfHeight);
	const Ring   equatorPositive = MakeRing(2, aRadius, aHalfHeight);
	const Ring   equatorNegative = MakeRing(2, aRadius, -aHalfHeight);
	const Ring   ring3 = MakeRing(3, aRadius, -aHalfHeight);
	CapPositive(PxVec3(aRadius + aHalfHeight, 0.0f, 0.0f), ring1, aOut);
	Band(ring1, equatorPositive, aOut);
	if (aHalfHeight > 0.0f)
	{
		Band(equatorPositive, equatorNegative, aOut);
	}
	Band(aHalfHeight > 0.0f ? equatorNegative : equatorPositive, ring3, aOut);
	CapNegative(PxVec3(-aRadius - aHalfHeight, 0.0f, 0.0f), ring3, aOut);
	for (size_t i = first; i < aOut.size(); ++i)
	{
		for (PxVec3& v : aOut[i].v)
		{
			v = aPose.transform(v);
		}
	}
}
}

bool KeepIfTouches(const Tri& aTri, const PxBounds3& aBox)
{
	for (unsigned axis = 0; axis < 3; ++axis)
	{
		const float lo = std::min({ aTri.v[0][axis], aTri.v[1][axis], aTri.v[2][axis] });
		const float hi = std::max({ aTri.v[0][axis], aTri.v[1][axis], aTri.v[2][axis] });
		if (hi < aBox.minimum[axis] || lo > aBox.maximum[axis])
		{
			return false;
		}
	}
	return true;
}

void BoxTriangles(const PxVec3& aHalfExtents, const PxTransform& aPose, std::vector<Tri>& aOut)
{
	// Corner i: bit 0 picks +x, bit 1 +y, bit 2 +z. Each face is listed counter-clockwise seen from outside.
	std::array<PxVec3, 8> c{};
	for (uint32_t i = 0; i < 8; ++i)
	{
		const PxVec3 local((i & 1) ? aHalfExtents.x : -aHalfExtents.x, (i & 2) ? aHalfExtents.y : -aHalfExtents.y,
		                   (i & 4) ? aHalfExtents.z : -aHalfExtents.z);
		c[i] = aPose.transform(local);
	}
	static constexpr uint8_t kFaces[36] = {
		0, 4, 6, 0, 6, 2,  // -x
		1, 3, 7, 1, 7, 5,  // +x
		0, 1, 5, 0, 5, 4,  // -y
		2, 6, 7, 2, 7, 3,  // +y
		0, 2, 3, 0, 3, 1,  // -z
		4, 5, 7, 4, 7, 6,  // +z
	};
	for (uint32_t i = 0; i < 36; i += 3)
	{
		Push(aOut, c[kFaces[i]], c[kFaces[i + 1]], c[kFaces[i + 2]]);
	}
}

void SphereTriangles(float aRadius, const PxTransform& aPose, std::vector<Tri>& aOut)
{
	Rounded(aRadius, 0.0f, aPose, aOut);
}

void CapsuleTriangles(float aRadius, float aHalfHeight, const PxTransform& aPose, std::vector<Tri>& aOut)
{
	Rounded(aRadius, std::max(aHalfHeight, 0.0f), aPose, aOut);
}
}
