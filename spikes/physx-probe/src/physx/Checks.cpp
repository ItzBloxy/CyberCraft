#include "physx/Checks.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <utility>
#include <vector>

using namespace physx;

namespace cpmc::px
{
namespace
{
struct NamedType
{
	const char* name;
	int         type;
};

// Every concrete type name PhysX 3.4.2 returns from getConcreteTypeName(), with its PxConcreteType.
constexpr NamedType kTypes[] = {
	{ "PxHeightField", PxConcreteType::eHEIGHTFIELD },
	{ "PxConvexMesh", PxConcreteType::eCONVEX_MESH },
	{ "PxBVH33TriangleMesh", PxConcreteType::eTRIANGLE_MESH_BVH33 },
	{ "PxBVH34TriangleMesh", PxConcreteType::eTRIANGLE_MESH_BVH34 },
	{ "PxClothFabric", PxConcreteType::eCLOTH_FABRIC },
	{ "PxRigidDynamic", PxConcreteType::eRIGID_DYNAMIC },
	{ "PxRigidStatic", PxConcreteType::eRIGID_STATIC },
	{ "PxShape", PxConcreteType::eSHAPE },
	{ "PxMaterial", PxConcreteType::eMATERIAL },
	{ "PxConstraint", PxConcreteType::eCONSTRAINT },
	{ "PxCloth", PxConcreteType::eCLOTH },
	{ "PxParticleSystem", PxConcreteType::ePARTICLE_SYSTEM },
	{ "PxParticleFluid", PxConcreteType::ePARTICLE_FLUID },
	{ "PxAggregate", PxConcreteType::eAGGREGATE },
	{ "PxArticulation", PxConcreteType::eARTICULATION },
	{ "PxArticulationLink", PxConcreteType::eARTICULATION_LINK },
	{ "PxPruningStructure", PxConcreteType::ePRUNING_STRUCTURE },
};
}

int ConcreteTypeForName(const char* aName)
{
	if (!aName)
	{
		return -1;
	}
	for (const auto& entry : kTypes)
	{
		if (std::strcmp(entry.name, aName) == 0)
		{
			return entry.type;
		}
	}
	return -1;
}

bool TolerancesPlausible(float aLength, float aSpeed)
{
	return aLength >= 0.01f && aLength <= 100.0f && aSpeed >= 0.1f && aSpeed <= 1000.0f;
}

bool GravityFinite(const PxVec3& aGravity)
{
	return std::isfinite(aGravity.x) && std::isfinite(aGravity.y) && std::isfinite(aGravity.z) &&
	       aGravity.magnitude() <= 100.0f;
}

bool GravityEarthLike(const PxVec3& aGravity)
{
	const float magnitude = aGravity.magnitude();
	return magnitude >= 1.0f && magnitude <= 30.0f;
}

bool GeometryMatchesMesh(PxGeometryType::Enum aGeometry, int aMeshConcreteType)
{
	switch (aGeometry)
	{
	case PxGeometryType::eTRIANGLEMESH:
		return aMeshConcreteType == PxConcreteType::eTRIANGLE_MESH_BVH33 ||
		       aMeshConcreteType == PxConcreteType::eTRIANGLE_MESH_BVH34;
	case PxGeometryType::eCONVEXMESH:
		return aMeshConcreteType == PxConcreteType::eCONVEX_MESH;
	case PxGeometryType::eHEIGHTFIELD:
		return aMeshConcreteType == PxConcreteType::eHEIGHTFIELD;
	default:
		return false;
	}
}

const char* GeometryName(PxGeometryType::Enum aGeometry)
{
	switch (aGeometry)
	{
	case PxGeometryType::eSPHERE: return "sphere";
	case PxGeometryType::ePLANE: return "plane";
	case PxGeometryType::eCAPSULE: return "capsule";
	case PxGeometryType::eBOX: return "box";
	case PxGeometryType::eCONVEXMESH: return "convexMesh";
	case PxGeometryType::eTRIANGLEMESH: return "triangleMesh";
	case PxGeometryType::eHEIGHTFIELD: return "heightField";
	default: return "unknown";
	}
}

const char* GeometryShortName(PxGeometryType::Enum aGeometry)
{
	switch (aGeometry)
	{
	case PxGeometryType::eSPHERE: return "sphere";
	case PxGeometryType::ePLANE: return "plane";
	case PxGeometryType::eCAPSULE: return "capsule";
	case PxGeometryType::eBOX: return "box";
	case PxGeometryType::eCONVEXMESH: return "convex";
	case PxGeometryType::eTRIANGLEMESH: return "trimesh";
	case PxGeometryType::eHEIGHTFIELD: return "heightfield";
	default: return "unknown";
	}
}

std::string Category(const char* aActorKind, PxShapeFlags aShapeFlags, PxGeometryType::Enum aGeometry)
{
	const char* kind = aActorKind;
	if (aShapeFlags.isSet(PxShapeFlag::eTRIGGER_SHAPE))
	{
		kind = "trigger";
	}
	else if (aShapeFlags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE) && !aShapeFlags.isSet(PxShapeFlag::eSIMULATION_SHAPE))
	{
		kind = "queryonly";
	}
	return std::string(kind) + "_" + GeometryShortName(aGeometry);
}

std::string FilterKey(const PxFilterData& aData)
{
	return std::format("{:x}:{:x}:{:x}:{:x}", aData.word0, aData.word1, aData.word2, aData.word3);
}

std::string SanitizeName(const char* aName, size_t aMaxLength)
{
	std::string out;
	if (!aName)
	{
		return out;
	}
	for (size_t i = 0; i < aMaxLength && aName[i] != '\0'; ++i)
	{
		const auto c = static_cast<unsigned char>(aName[i]);
		out.push_back(c >= 0x20 && c <= 0x7E ? char(c) : '?');
	}
	return out;
}

Tally TrimTally(const Tally& aTally, size_t aKeep)
{
	std::vector<std::pair<std::string, uint64_t>> entries(aTally.begin(), aTally.end());
	std::stable_sort(entries.begin(), entries.end(), [](const auto& aA, const auto& aB) { return aA.second > aB.second; });
	Tally    kept;
	uint64_t other = 0;
	for (size_t i = 0; i < entries.size(); ++i)
	{
		if (i < aKeep)
		{
			kept[entries[i].first] = entries[i].second;
		}
		else
		{
			other += entries[i].second;
		}
	}
	if (other > 0)
	{
		kept["other"] += other;
	}
	return kept;
}
}
