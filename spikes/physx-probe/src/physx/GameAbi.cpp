#include "physx/GameAbi.h"

#include <Windows.h>

#include <algorithm>
#include <iterator>

using namespace physx;

namespace cpmc::px
{
namespace
{
struct AuditedVtable
{
	AuditedClass cls;
	bool         common;  // in PhysX3Common_x64.dll rather than PhysX3_x64.dll
	uintptr_t    rva;
	int          concreteType;
	const char*  name;
};

// Each audited class's primary vtable, from the audit.
constexpr AuditedVtable kVtables[] = {
	{ AuditedClass::Physics, false, 0x20a310, -1, "PxPhysics" },
	{ AuditedClass::Scene, false, 0x20caf0, -1, "PxScene" },
	{ AuditedClass::RigidStatic, false, 0x20b980, PxConcreteType::eRIGID_STATIC, "PxRigidStatic" },
	{ AuditedClass::RigidDynamic, false, 0x20b2e8, PxConcreteType::eRIGID_DYNAMIC, "PxRigidDynamic" },
	{ AuditedClass::Aggregate, false, 0x206e68, PxConcreteType::eAGGREGATE, "PxAggregate" },
	{ AuditedClass::Shape, false, 0x20e9a8, PxConcreteType::eSHAPE, "PxShape" },
	{ AuditedClass::ConvexMesh, true, 0x1b5200, PxConcreteType::eCONVEX_MESH, "PxConvexMesh" },
	{ AuditedClass::TriangleMeshBv33, true, 0x1b6978, PxConcreteType::eTRIANGLE_MESH_BVH33, "PxBVH33TriangleMesh" },
	{ AuditedClass::TriangleMeshBv34, true, 0x1b6868, PxConcreteType::eTRIANGLE_MESH_BVH34, "PxBVH34TriangleMesh" },
	{ AuditedClass::HeightField, true, 0x1b6b18, PxConcreteType::eHEIGHTFIELD, "PxHeightField" },
};

const AuditedVtable& Entry(AuditedClass aClass)
{
	return *std::find_if(std::begin(kVtables), std::end(kVtables), [&](const AuditedVtable& aEntry) { return aEntry.cls == aClass; });
}
}

std::optional<ModuleId> ModuleIdentity(const void* aBase)
{
	if (!aBase)
	{
		return std::nullopt;
	}
	const auto* dos = static_cast<const IMAGE_DOS_HEADER*>(aBase);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000 - LONG(sizeof(IMAGE_NT_HEADERS64)))
	{
		return std::nullopt;
	}
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(static_cast<const uint8_t*>(aBase) + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
	{
		return std::nullopt;
	}
	return ModuleId{ nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage };
}

std::optional<AuditedClass> AuditedClassOf(uintptr_t aVtable, const ModuleBases& aBases)
{
	for (const AuditedVtable& entry : kVtables)
	{
		const uintptr_t base = entry.common ? aBases.common : aBases.physx3;
		if (base != 0 && aVtable == base + entry.rva)
		{
			return entry.cls;
		}
	}
	return std::nullopt;
}

int ConcreteTypeOf(AuditedClass aClass)
{
	return Entry(aClass).concreteType;
}

const char* InterfaceName(AuditedClass aClass)
{
	return Entry(aClass).name;
}

ObjectCheck CheckObject(ReadableMemory& aMemory, const ModuleBases& aBases, const void* aObject, size_t aBytes)
{
	if (!aMemory.Range(aObject, std::max<size_t>(aBytes, 16)))
	{
		return { ObjectVerdict::Unreadable };
	}
	const std::optional<AuditedClass> cls = AuditedClassOf(*static_cast<const uintptr_t*>(aObject), aBases);
	if (!cls)
	{
		return { ObjectVerdict::Unaudited };
	}
	const int type = ConcreteTypeOf(*cls);
	if (type >= 0 && int(static_cast<const PxBase*>(aObject)->getConcreteType()) != type)
	{
		return { ObjectVerdict::WrongType, *cls };
	}
	return { ObjectVerdict::Ok, *cls };
}
}
