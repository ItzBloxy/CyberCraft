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

PxVec3 ApplyMeshScale(const PxMeshScale& aScale, const PxVec3& aVertex)
{
	return aScale.transform(aVertex);
}

bool FlipsWinding(const PxMeshScale& aScale)
{
	return aScale.scale.x * aScale.scale.y * aScale.scale.z < 0.0f;
}

void MeshTriangles(const PxVec3* aVertices, uint32_t aVertexCount, const void* aIndices, bool aIndices16,
                   uint32_t aTriangleCount, const PxMeshScale& aScale, const PxTransform& aPose,
                   const PxBounds3& aRegion, std::vector<Tri>& aOut, uint32_t& aBadTriangles)
{
	const bool  flip = FlipsWinding(aScale);
	const auto* indices16 = static_cast<const uint16_t*>(aIndices);
	const auto* indices32 = static_cast<const uint32_t*>(aIndices);
	for (uint32_t t = 0; t < aTriangleCount; ++t)
	{
		uint32_t index[3];
		for (uint32_t k = 0; k < 3; ++k)
		{
			index[k] = aIndices16 ? indices16[t * 3 + k] : indices32[t * 3 + k];
		}
		if (index[0] >= aVertexCount || index[1] >= aVertexCount || index[2] >= aVertexCount)
		{
			++aBadTriangles;
			continue;
		}
		Tri tri{};
		for (uint32_t k = 0; k < 3; ++k)
		{
			tri.v[k] = aPose.transform(ApplyMeshScale(aScale, aVertices[index[k]]));
		}
		if (flip)
		{
			std::swap(tri.v[1], tri.v[2]);
		}
		if (KeepIfTouches(tri, aRegion))
		{
			aOut.push_back(tri);
		}
	}
}

void ConvexPolygonTriangles(const PxVec3* aVertices, uint32_t aVertexCount, const uint8_t* aIndexBuffer,
                            const PxHullPolygon* aPolygons, uint32_t aPolygonCount, const PxMeshScale& aScale,
                            const PxTransform& aPose, std::vector<Tri>& aOut)
{
	const bool flip = FlipsWinding(aScale);
	for (uint32_t p = 0; p < aPolygonCount; ++p)
	{
		const PxHullPolygon& polygon = aPolygons[p];
		const uint8_t*       index = aIndexBuffer + polygon.mIndexBase;
		if (polygon.mNbVerts < 3 ||
		    std::any_of(index, index + polygon.mNbVerts, [&](uint8_t aIndex) { return aIndex >= aVertexCount; }))
		{
			continue;
		}
		const PxVec3 first = aPose.transform(ApplyMeshScale(aScale, aVertices[index[0]]));
		for (uint32_t k = 1; k + 1 < polygon.mNbVerts; ++k)
		{
			PxVec3 b = aPose.transform(ApplyMeshScale(aScale, aVertices[index[k]]));
			PxVec3 c = aPose.transform(ApplyMeshScale(aScale, aVertices[index[k + 1]]));
			if (flip)
			{
				std::swap(b, c);
			}
			Push(aOut, first, b, c);
		}
	}
}

namespace
{
constexpr PxU8 kHoleMaterial = PxHeightFieldMaterial::eHOLE;
}

CellRange HeightfieldCellsUnder(const HeightfieldView& aField, const PxTransform& aPose, const PxBounds3& aBox)
{
	CellRange cells;
	if (aField.rows < 2 || aField.columns < 2 || aField.rowScale <= 0.0f || aField.columnScale <= 0.0f)
	{
		return cells;
	}
	PxBounds3 local = PxBounds3::empty();
	for (uint32_t i = 0; i < 8; ++i)
	{
		const PxVec3 corner((i & 1) ? aBox.maximum.x : aBox.minimum.x, (i & 2) ? aBox.maximum.y : aBox.minimum.y,
		                    (i & 4) ? aBox.maximum.z : aBox.minimum.z);
		local.include(aPose.transformInv(corner));
	}
	const auto clampTo = [](double aValue, uint32_t aMax) { return uint32_t(std::clamp(aValue, 0.0, double(aMax))); };
	const uint32_t rowEnd = aField.rows - 1;  // cells exist for rows 0 .. rows-2
	const uint32_t columnEnd = aField.columns - 1;
	cells.r0 = clampTo(std::floor(double(local.minimum.x) / aField.rowScale), rowEnd);
	cells.r1 = clampTo(std::floor(double(local.maximum.x) / aField.rowScale) + 1.0, rowEnd);
	cells.c0 = clampTo(std::floor(double(local.minimum.z) / aField.columnScale), columnEnd);
	cells.c1 = clampTo(std::floor(double(local.maximum.z) / aField.columnScale) + 1.0, columnEnd);
	return cells;
}

void HeightfieldTriangles(const HeightfieldView& aField, const CellRange& aCells, const PxTransform& aPose,
                          const PxBounds3& aRegion, std::vector<Tri>& aOut)
{
	const auto vertex = [&](uint32_t aRow, uint32_t aColumn) {
		const PxHeightFieldSample sample = aField.sample(aRow, aColumn);
		const PxVec3 local(float(aRow) * aField.rowScale, float(sample.height) * aField.heightScale,
		                   float(aColumn) * aField.columnScale);
		return aPose.transform(local);
	};
	for (uint32_t r = aCells.r0; r < aCells.r1; ++r)
	{
		for (uint32_t c = aCells.c0; c < aCells.c1; ++c)
		{
			const PxHeightFieldSample cell = aField.sample(r, c);
			const PxU8   material0 = cell.materialIndex0;  // the conversion drops the tess bit
			const PxU8   material1 = cell.materialIndex1;
			const PxVec3 v00 = vertex(r, c);
			const PxVec3 v01 = vertex(r, c + 1);
			const PxVec3 v10 = vertex(r + 1, c);
			const PxVec3 v11 = vertex(r + 1, c + 1);
			Tri first{};
			Tri second{};
			if (cell.tessFlag())
			{
				first = { { v10, v00, v11 } };  // diagonal (r, c) - (r+1, c+1)
				second = { { v01, v11, v00 } };
			}
			else
			{
				first = { { v00, v01, v10 } };  // diagonal (r, c+1) - (r+1, c)
				second = { { v11, v10, v01 } };
			}
			if (material0 != kHoleMaterial && KeepIfTouches(first, aRegion))
			{
				aOut.push_back(first);
			}
			if (material1 != kHoleMaterial && KeepIfTouches(second, aRegion))
			{
				aOut.push_back(second);
			}
		}
	}
}

void AppendTriangles(const PxShape& aShape, const PxTransform& aActorPose, const PxBounds3& aRegion,
                     ReadableMemory& aMemory, const ModuleBases& aBases, std::vector<Tri>& aOut, AppendStats& aStats)
{
	// A mesh object passes when it is readable and its vtable is the audited table of aClass or aOther.
	const auto meshOk = [&](const void* aMesh, AuditedClass aClass, AuditedClass aOther) {
		const ObjectCheck check = CheckObject(aMemory, aBases, aMesh);
		return check.verdict == ObjectVerdict::Ok && (check.cls == aClass || check.cls == aOther);
	};
	const PxTransform pose = aActorPose * aShape.getLocalPose();
	std::vector<Tri>  small;
	switch (aShape.getGeometryType())
	{
	case PxGeometryType::eBOX:
	{
		PxBoxGeometry geometry;
		if (aShape.getBoxGeometry(geometry))
		{
			BoxTriangles(geometry.halfExtents, pose, small);
		}
		break;
	}
	case PxGeometryType::eSPHERE:
	{
		PxSphereGeometry geometry;
		if (aShape.getSphereGeometry(geometry))
		{
			SphereTriangles(geometry.radius, pose, small);
		}
		break;
	}
	case PxGeometryType::eCAPSULE:
	{
		PxCapsuleGeometry geometry;
		if (aShape.getCapsuleGeometry(geometry))
		{
			CapsuleTriangles(geometry.radius, geometry.halfHeight, pose, small);
		}
		break;
	}
	case PxGeometryType::ePLANE:
		++aStats.planes;
		break;
	case PxGeometryType::eCONVEXMESH:
	{
		PxConvexMeshGeometry geometry;
		if (!aShape.getConvexMeshGeometry(geometry) || !geometry.convexMesh)
		{
			break;
		}
		if (!meshOk(geometry.convexMesh, AuditedClass::ConvexMesh, AuditedClass::ConvexMesh))
		{
			++aStats.unreadable;
			break;
		}
		const PxConvexMesh&        mesh = *geometry.convexMesh;
		std::vector<PxHullPolygon> polygons(mesh.getNbPolygons());
		uint32_t                   indexCount = 0;
		for (PxU32 i = 0; i < PxU32(polygons.size()); ++i)
		{
			mesh.getPolygonData(i, polygons[i]);
			indexCount = std::max<uint32_t>(indexCount, uint32_t(polygons[i].mIndexBase) + polygons[i].mNbVerts);
		}
		const PxVec3*  vertices = mesh.getVertices();
		const uint32_t vertexCount = mesh.getNbVertices();
		const PxU8*    indices = mesh.getIndexBuffer();
		if (vertexCount == 0 || indexCount == 0)
		{
			break;
		}
		if (!aMemory.Range(vertices, size_t(vertexCount) * sizeof(PxVec3)) || !aMemory.Range(indices, indexCount))
		{
			++aStats.unreadable;
			break;
		}
		ConvexPolygonTriangles(vertices, vertexCount, indices, polygons.data(), PxU32(polygons.size()), geometry.scale, pose, small);
		break;
	}
	case PxGeometryType::eTRIANGLEMESH:
	{
		PxTriangleMeshGeometry geometry;
		if (!aShape.getTriangleMeshGeometry(geometry) || !geometry.triangleMesh)
		{
			break;
		}
		if (!meshOk(geometry.triangleMesh, AuditedClass::TriangleMeshBv33, AuditedClass::TriangleMeshBv34))
		{
			++aStats.unreadable;
			break;
		}
		const PxTriangleMesh& mesh = *geometry.triangleMesh;
		const PxVec3*         vertices = mesh.getVertices();
		const uint32_t        vertexCount = mesh.getNbVertices();
		const void*           triangles = mesh.getTriangles();
		const bool            indices16 = mesh.getTriangleMeshFlags().isSet(PxTriangleMeshFlag::e16_BIT_INDICES);
		const uint32_t        triangleCount = mesh.getNbTriangles();
		if (vertexCount == 0 || triangleCount == 0)
		{
			break;
		}
		if (!aMemory.Range(vertices, size_t(vertexCount) * sizeof(PxVec3)) ||
		    !aMemory.Range(triangles, size_t(triangleCount) * 3 * (indices16 ? 2 : 4)))
		{
			++aStats.unreadable;
			break;
		}
		aStats.meshTriangles += triangleCount;
		MeshTriangles(vertices, vertexCount, triangles, indices16, triangleCount, geometry.scale, pose, aRegion, aOut,
		              aStats.badTriangles);
		break;
	}
	case PxGeometryType::eHEIGHTFIELD:
	{
		PxHeightFieldGeometry geometry;
		if (!aShape.getHeightFieldGeometry(geometry) || !geometry.heightField)
		{
			break;
		}
		if (!meshOk(geometry.heightField, AuditedClass::HeightField, AuditedClass::HeightField))
		{
			++aStats.unreadable;
			break;
		}
		const PxHeightField& field = *geometry.heightField;
		HeightfieldView      view;
		view.rows = field.getNbRows();
		view.columns = field.getNbColumns();
		if (view.rows == 0 || view.columns == 0)
		{
			break;
		}
		// The samples are one array of rows x columns; getSample returns a reference into it.
		if (!aMemory.Range(&field.getSample(0, 0), size_t(view.rows) * view.columns * sizeof(PxHeightFieldSample)))
		{
			++aStats.unreadable;
			break;
		}
		view.heightScale = geometry.heightScale;
		view.rowScale = geometry.rowScale;
		view.columnScale = geometry.columnScale;
		view.sample = [&field](uint32_t aRow, uint32_t aColumn) { return field.getSample(aRow, aColumn); };
		const CellRange cells = HeightfieldCellsUnder(view, pose, aRegion);
		if (!cells.Empty())
		{
			aStats.fieldCells += uint64_t(cells.r1 - cells.r0) * (cells.c1 - cells.c0);
			HeightfieldTriangles(view, cells, pose, aRegion, aOut);
		}
		break;
	}
	default:
		++aStats.unknownGeometry;
		break;
	}
	for (const Tri& tri : small)
	{
		if (KeepIfTouches(tri, aRegion))
		{
			aOut.push_back(tri);
		}
	}
}
}
