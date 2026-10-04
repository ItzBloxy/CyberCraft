#include "Harness.h"

#include "physx/ShapeTriangles.h"

using namespace physx;
using cpmc::px::Tri;

namespace
{
const PxBounds3 kEverywhere(PxVec3(-1e6f), PxVec3(1e6f));

bool Near(const PxVec3& aA, const PxVec3& aB, float aEps = 1e-4f)
{
	return (aA - aB).magnitude() <= aEps;
}
}

TEST(MeshScaleMatchesPhysXWithRotation)
{
	// Scale 2 along the axis rotated from X to Y: a vertex on Y doubles, one on X stays.
	const PxMeshScale scale(PxVec3(2.0f, 1.0f, 1.0f), PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f)));
	CHECK(Near(cpmc::px::ApplyMeshScale(scale, PxVec3(0.0f, 1.0f, 0.0f)), PxVec3(0.0f, 2.0f, 0.0f)));
	CHECK(Near(cpmc::px::ApplyMeshScale(scale, PxVec3(1.0f, 0.0f, 0.0f)), PxVec3(1.0f, 0.0f, 0.0f)));
	CHECK(!cpmc::px::FlipsWinding(scale));
	CHECK(cpmc::px::FlipsWinding(PxMeshScale(PxVec3(-1.0f, 1.0f, 1.0f), PxQuat(PxIdentity))));
}

TEST(MeshTrianglesReadsBothIndexSizesAndFlipsMirroredMeshes)
{
	const PxVec3   vertices[4] = { PxVec3(0, 0, 0), PxVec3(1, 0, 0), PxVec3(0, 1, 0), PxVec3(0, 0, 1) };
	const uint16_t indices16[6] = { 0, 1, 2, 0, 2, 3 };
	const uint32_t indices32[6] = { 0, 1, 2, 0, 2, 3 };
	const PxTransform pose(PxVec3(10.0f, 0.0f, 0.0f));
	uint32_t          bad = 0;

	std::vector<Tri> from16;
	cpmc::px::MeshTriangles(vertices, 4, indices16, true, 2, PxMeshScale(), pose, kEverywhere, from16, bad);
	std::vector<Tri> from32;
	cpmc::px::MeshTriangles(vertices, 4, indices32, false, 2, PxMeshScale(), pose, kEverywhere, from32, bad);
	CHECK_EQ(from16.size(), size_t(2));
	CHECK_EQ(from32.size(), size_t(2));
	CHECK(Near(from16[0].v[1], PxVec3(11.0f, 0.0f, 0.0f)));
	CHECK(Near(from32[1].v[2], PxVec3(10.0f, 0.0f, 1.0f)));

	std::vector<Tri> mirrored;
	cpmc::px::MeshTriangles(vertices, 4, indices16, true, 1, PxMeshScale(PxVec3(-1.0f, 1.0f, 1.0f), PxQuat(PxIdentity)),
	                        pose, kEverywhere, mirrored, bad);
	CHECK_EQ(mirrored.size(), size_t(1));
	CHECK(Near(mirrored[0].v[1], PxVec3(10.0f, 1.0f, 0.0f)));  // v1 and v2 swapped
	CHECK(Near(mirrored[0].v[2], PxVec3(9.0f, 0.0f, 0.0f)));
	CHECK_EQ(bad, uint32_t(0));
}

TEST(MeshTrianglesSkipsBadIndicesAndCullsByRegion)
{
	const PxVec3   vertices[3] = { PxVec3(0, 0, 0), PxVec3(1, 0, 0), PxVec3(0, 1, 0) };
	const uint32_t indices[6] = { 0, 1, 2, 0, 1, 7 };
	uint32_t       bad = 0;
	std::vector<Tri> tris;
	cpmc::px::MeshTriangles(vertices, 3, indices, false, 2, PxMeshScale(), PxTransform(PxIdentity), kEverywhere, tris, bad);
	CHECK_EQ(tris.size(), size_t(1));
	CHECK_EQ(bad, uint32_t(1));

	std::vector<Tri> culled;
	const PxBounds3  farAway(PxVec3(50.0f), PxVec3(60.0f));
	cpmc::px::MeshTriangles(vertices, 3, indices, false, 1, PxMeshScale(), PxTransform(PxIdentity), farAway, culled, bad);
	CHECK_EQ(culled.size(), size_t(0));
}

TEST(ConvexPolygonsAreFanTriangulated)
{
	// A square and a pentagon: 2 + 3 triangles.
	const PxVec3 vertices[9] = { PxVec3(0, 0, 0), PxVec3(1, 0, 0), PxVec3(1, 1, 0), PxVec3(0, 1, 0),
		                         PxVec3(0, 0, 2), PxVec3(1, 0, 2), PxVec3(1.5f, 1, 2), PxVec3(0.5f, 2, 2), PxVec3(-0.5f, 1, 2) };
	const uint8_t indexBuffer[9] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
	PxHullPolygon polygons[2]{};
	polygons[0].mNbVerts = 4;
	polygons[0].mIndexBase = 0;
	polygons[1].mNbVerts = 5;
	polygons[1].mIndexBase = 4;
	std::vector<Tri> tris;
	cpmc::px::ConvexPolygonTriangles(vertices, 9, indexBuffer, polygons, 2, PxMeshScale(), PxTransform(PxIdentity), tris);
	CHECK_EQ(tris.size(), size_t(5));
	CHECK(Near(tris[0].v[0], vertices[0]));
	CHECK(Near(tris[0].v[1], vertices[1]));
	CHECK(Near(tris[0].v[2], vertices[2]));
	CHECK(Near(tris[1].v[1], vertices[2]));
	CHECK(Near(tris[1].v[2], vertices[3]));
	CHECK(Near(tris[4].v[0], vertices[4]));

	// A polygon that points past the vertex array is skipped.
	polygons[1].mIndexBase = 6;  // uses indices 6..10; 9 and 10 are past the buffer's vertices
	const uint8_t badIndexBuffer[11] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	std::vector<Tri> skipped;
	cpmc::px::ConvexPolygonTriangles(vertices, 9, badIndexBuffer, polygons, 2, PxMeshScale(), PxTransform(PxIdentity), skipped);
	CHECK_EQ(skipped.size(), size_t(2));
}
