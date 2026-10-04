#pragma once

#include "Readable.h"
#include "physx/GameAbi.h"
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

// A heightfield's samples and scales. Sample (r, c) sits at local (r * rowScale, height * heightScale,
// c * columnScale): rows along local X, height along local Y, columns along local Z.
struct HeightfieldView
{
	uint32_t rows = 0;
	uint32_t columns = 0;
	float heightScale = 1.0f;
	float rowScale = 1.0f;
	float columnScale = 1.0f;
	std::function<physx::PxHeightFieldSample(uint32_t aRow, uint32_t aColumn)> sample;
};

// Cells [r0, r1) x [c0, c1). Cell (r, c) spans samples r..r+1 and c..c+1.
struct CellRange
{
	uint32_t r0 = 0;
	uint32_t r1 = 0;
	uint32_t c0 = 0;
	uint32_t c1 = 0;

	bool Empty() const { return r0 >= r1 || c0 >= c1; }
};

// The cells of a heightfield placed at aPose that lie under a world-space box.
CellRange HeightfieldCellsUnder(const HeightfieldView& aField, const physx::PxTransform& aPose, const physx::PxBounds3& aBox);

// The cells' triangles in PhysX's own vertex order (Gu::HeightField::getTriangleVertexIndices). A cell's first
// triangle uses materialIndex0 and its second materialIndex1; material 127 (a hole) leaves the triangle out.
void HeightfieldTriangles(const HeightfieldView& aField, const CellRange& aCells, const physx::PxTransform& aPose,
                          const physx::PxBounds3& aRegion, std::vector<Tri>& aOut);

struct AppendStats
{
	uint32_t planes = 0;
	uint32_t unknownGeometry = 0;
	uint32_t badTriangles = 0;
	uint32_t unreadable = 0;     // meshes that failed their checks
	uint64_t meshTriangles = 0;  // triangle meshes' triangles looked at, all of each mesh's
	uint64_t fieldCells = 0;     // heightfield cells looked at, those under the region
};

// World-space triangles of a live shape that touch aRegion. The caller has checked the shape (see CheckObject);
// this checks the shape's mesh the same way, and checks every array it reads for readability first. A mesh that fails
// is skipped and counted in aStats.unreadable.
void AppendTriangles(const physx::PxShape& aShape, const physx::PxTransform& aActorPose, const physx::PxBounds3& aRegion,
                     ReadableMemory& aMemory, const ModuleBases& aBases, std::vector<Tri>& aOut, AppendStats& aStats);
}
