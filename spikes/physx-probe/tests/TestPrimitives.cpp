#include "Harness.h"

#include "physx/ShapeTriangles.h"

#include <algorithm>
#include <map>
#include <tuple>

using namespace physx;
using cpmc::px::Tri;

namespace
{
using Key = std::tuple<float, float, float>;
using Edge = std::pair<Key, Key>;

Key K(const PxVec3& aV)
{
	return { aV.x, aV.y, aV.z };
}

// Every edge is shared by exactly two triangles, once in each direction: a closed, consistently wound mesh.
bool IsClosed(const std::vector<Tri>& aTris)
{
	std::map<Edge, int> directed;
	for (const Tri& t : aTris)
	{
		for (int k = 0; k < 3; ++k)
		{
			++directed[{ K(t.v[k]), K(t.v[(k + 1) % 3]) }];
		}
	}
	for (const auto& [edge, count] : directed)
	{
		const auto reverse = directed.find({ edge.second, edge.first });
		if (count != 1 || reverse == directed.end() || reverse->second != 1)
		{
			return false;
		}
	}
	return true;
}

// Every face normal points away from aCenter.
bool FacesOutward(const std::vector<Tri>& aTris, const PxVec3& aCenter)
{
	for (const Tri& t : aTris)
	{
		const PxVec3 normal = (t.v[1] - t.v[0]).cross(t.v[2] - t.v[0]);
		const PxVec3 centroid = (t.v[0] + t.v[1] + t.v[2]) / 3.0f;
		if (normal.dot(centroid - aCenter) <= 0.0f)
		{
			return false;
		}
	}
	return true;
}
}

TEST(BoxHas12OutwardTrianglesAndIsClosed)
{
	std::vector<Tri> tris;
	const PxTransform pose(PxVec3(10.0f, 20.0f, 30.0f), PxQuat(0.7f, PxVec3(0.0f, 0.0f, 1.0f)));
	cpmc::px::BoxTriangles(PxVec3(1.0f, 2.0f, 3.0f), pose, tris);
	CHECK_EQ(tris.size(), size_t(12));
	CHECK(IsClosed(tris));
	CHECK(FacesOutward(tris, pose.p));
	for (const Tri& t : tris)
	{
		for (const PxVec3& v : t.v)
		{
			const PxVec3 local = pose.transformInv(v);
			CHECK_NEAR(std::fabs(local.x), 1.0, 1e-4);
			CHECK_NEAR(std::fabs(local.y), 2.0, 1e-4);
			CHECK_NEAR(std::fabs(local.z), 3.0, 1e-4);
		}
	}
}

TEST(SphereHas48OutwardTrianglesOnItsRadius)
{
	std::vector<Tri> tris;
	const PxTransform pose(PxVec3(1.0f, 2.0f, 3.0f));
	cpmc::px::SphereTriangles(0.5f, pose, tris);
	CHECK_EQ(tris.size(), size_t(48));
	CHECK(IsClosed(tris));
	CHECK(FacesOutward(tris, pose.p));
	for (const Tri& t : tris)
	{
		for (const PxVec3& v : t.v)
		{
			CHECK_NEAR((v - pose.p).magnitude(), 0.5, 1e-4);
		}
	}
}

TEST(CapsuleHas64OutwardTrianglesAroundItsXAxis)
{
	std::vector<Tri> tris;
	const PxTransform pose(PxVec3(0.0f), PxQuat(PxIdentity));
	cpmc::px::CapsuleTriangles(0.4f, 0.5f, pose, tris);
	CHECK_EQ(tris.size(), size_t(64));
	CHECK(IsClosed(tris));
	CHECK(FacesOutward(tris, PxVec3(0.0f)));
	for (const Tri& t : tris)
	{
		for (const PxVec3& v : t.v)
		{
			// Distance to the segment from (-0.5, 0, 0) to (0.5, 0, 0) equals the radius.
			const float  x = std::clamp(v.x, -0.5f, 0.5f);
			const PxVec3 nearest(x, 0.0f, 0.0f);
			CHECK_NEAR((v - nearest).magnitude(), 0.4, 1e-4);
		}
	}
}

TEST(KeepIfTouchesUsesTheTrianglesBounds)
{
	const PxBounds3 box(PxVec3(0.0f), PxVec3(1.0f));
	const Tri inside{ { PxVec3(0.2f), PxVec3(0.3f), PxVec3(0.4f, 0.2f, 0.2f) } };
	const Tri touching{ { PxVec3(1.0f, 0.5f, 0.5f), PxVec3(2.0f, 0.5f, 0.5f), PxVec3(2.0f, 0.6f, 0.5f) } };
	const Tri outside{ { PxVec3(1.1f, 0.5f, 0.5f), PxVec3(2.0f, 0.5f, 0.5f), PxVec3(2.0f, 0.6f, 0.5f) } };
	const Tri spanning{ { PxVec3(-1.0f, 0.5f, 0.5f), PxVec3(2.0f, 0.5f, 0.5f), PxVec3(2.0f, 0.6f, 0.5f) } };
	CHECK(cpmc::px::KeepIfTouches(inside, box));
	CHECK(cpmc::px::KeepIfTouches(touching, box));
	CHECK(!cpmc::px::KeepIfTouches(outside, box));
	CHECK(cpmc::px::KeepIfTouches(spanning, box));
}
