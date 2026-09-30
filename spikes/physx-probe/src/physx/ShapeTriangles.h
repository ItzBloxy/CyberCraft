#pragma once

#include "physx/PxHeaders.h"

#include <cstdint>
#include <functional>
#include <vector>

// Turns PhysX shapes into world-space triangles. Everything except AppendTriangles works on plain data and
// is unit-tested without PhysX; AppendTriangles reads a live shape from the game.
namespace cpmc::px
{
struct Tri
{
	physx::PxVec3 v[3];
};

// True if the triangle's bounding box touches the box (touching edges count).
bool KeepIfTouches(const Tri& aTri, const physx::PxBounds3& aBox);

// Closed primitives with outward-facing triangles, placed by aPose.
void BoxTriangles(const physx::PxVec3& aHalfExtents, const physx::PxTransform& aPose, std::vector<Tri>& aOut);
// A UV sphere with 8 segments and 4 rings: 48 triangles, poles on the local X axis.
void SphereTriangles(float aRadius, const physx::PxTransform& aPose, std::vector<Tri>& aOut);
// The same sphere split at its equator, with an 8-segment cylinder between the halves: 64 triangles along local X.
void CapsuleTriangles(float aRadius, float aHalfHeight, const physx::PxTransform& aPose, std::vector<Tri>& aOut);

// PhysX's mesh scale: rotation^-1 * scale * rotation (PxMeshScale::transform).
physx::PxVec3 ApplyMeshScale(const physx::PxMeshScale& aScale, const physx::PxVec3& aVertex);
// A negative scale determinant mirrors the mesh, which reverses triangle winding.
bool FlipsWinding(const physx::PxMeshScale& aScale);

// A triangle mesh from its raw arrays. Triangles that reference a missing vertex are skipped and counted.
void MeshTriangles(const physx::PxVec3* aVertices, uint32_t aVertexCount, const void* aIndices, bool aIndices16,
                   uint32_t aTriangleCount, const physx::PxMeshScale& aScale, const physx::PxTransform& aPose,
                   const physx::PxBounds3& aRegion, std::vector<Tri>& aOut, uint32_t& aBadTriangles);

// Convex hull polygons, fan-triangulated. Polygons that reference a missing vertex are skipped.
void ConvexPolygonTriangles(const physx::PxVec3* aVertices, uint32_t aVertexCount, const uint8_t* aIndexBuffer,
                            const physx::PxHullPolygon* aPolygons, uint32_t aPolygonCount,
                            const physx::PxMeshScale& aScale, const physx::PxTransform& aPose, std::vector<Tri>& aOut);
}
