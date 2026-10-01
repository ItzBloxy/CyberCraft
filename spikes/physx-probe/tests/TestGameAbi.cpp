#include "Harness.h"

#include "physx/GameAbi.h"

#include <Windows.h>

#include <Psapi.h>

#include <cstdint>

using namespace physx;
using namespace cpmc::px;

namespace
{
// Where the crashed run had the two DLLs.
constexpr ModuleBases kBases{ 0x7ffcb6820000, 0x7ffcb6ab0000 };

// A PhysX object's leading fields: the vtable pointer, the type ID, the base flags and, for actors, userData.
struct FakeObject
{
	uintptr_t     vtable = 0;
	std::uint16_t type = 0;
	std::uint16_t baseFlags = 0;
	std::uint32_t padding = 0;
	void*         userData = nullptr;
};
}

TEST(ModuleIdentityReadsThePeHeader)
{
	alignas(8) unsigned char image[0x200] = {};
	auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
	dos->e_magic = IMAGE_DOS_SIGNATURE;
	dos->e_lfanew = 0x80;
	auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(image + 0x80);
	nt->Signature = IMAGE_NT_SIGNATURE;
	nt->FileHeader.TimeDateStamp = 0x64c978b4;
	nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
	nt->OptionalHeader.SizeOfImage = 0x281000;
	const auto id = ModuleIdentity(image);
	CHECK(id && *id == kAuditedModules[0].id);
	nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
	CHECK(!ModuleIdentity(image));
	nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
	nt->Signature = 0;
	CHECK(!ModuleIdentity(image));
	dos->e_magic = 0;
	CHECK(!ModuleIdentity(image));
	CHECK(!ModuleIdentity(nullptr));
}

TEST(ModuleIdentityOfALoadedDll)
{
	// kernel32's image size, checked against GetModuleInformation.
	const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
	MODULEINFO    info{};
	CHECK(GetModuleInformation(GetCurrentProcess(), kernel32, &info, sizeof(info)));
	const auto id = ModuleIdentity(kernel32);
	CHECK(id && id->imageSize == info.SizeOfImage);
	CHECK(id && id->timestamp != 0);
}

TEST(AuditedClassesAreFoundByVtable)
{
	CHECK(AuditedClassOf(kBases.physx3 + 0x20a310, kBases) == AuditedClass::Physics);
	CHECK(AuditedClassOf(kBases.physx3 + 0x20caf0, kBases) == AuditedClass::Scene);
	CHECK(AuditedClassOf(kBases.physx3 + 0x20b980, kBases) == AuditedClass::RigidStatic);
	CHECK(AuditedClassOf(kBases.physx3 + 0x20b2e8, kBases) == AuditedClass::RigidDynamic);
	CHECK(AuditedClassOf(kBases.physx3 + 0x206e68, kBases) == AuditedClass::Aggregate);
	CHECK(AuditedClassOf(kBases.physx3 + 0x20e9a8, kBases) == AuditedClass::Shape);
	CHECK(AuditedClassOf(kBases.common + 0x1b5200, kBases) == AuditedClass::ConvexMesh);
	CHECK(AuditedClassOf(kBases.common + 0x1b6978, kBases) == AuditedClass::TriangleMeshBv33);
	CHECK(AuditedClassOf(kBases.common + 0x1b6868, kBases) == AuditedClass::TriangleMeshBv34);
	CHECK(AuditedClassOf(kBases.common + 0x1b6b18, kBases) == AuditedClass::HeightField);
	CHECK(!AuditedClassOf(kBases.physx3 + 0x20e9a8 + 8, kBases));  // one slot into NpShape's table
	CHECK(!AuditedClassOf(kBases.physx3 + 0x20e878, kBases));      // the abstract PxShape table
	CHECK(!AuditedClassOf(kBases.common + 0x20e9a8, kBases));      // the right RVA in the wrong DLL
	const ModuleBases moved{ 0x180000000, 0x190000000 };
	CHECK(AuditedClassOf(moved.physx3 + 0x20e9a8, moved) == AuditedClass::Shape);
	CHECK(!AuditedClassOf(kBases.physx3 + 0x20e9a8, moved));
	CHECK(!AuditedClassOf(0x20e9a8, ModuleBases{}));  // the DLLs aren't loaded
}

TEST(AuditedClassesKnowTheirTypeIdsAndNames)
{
	CHECK_EQ(ConcreteTypeOf(AuditedClass::Shape), int(PxConcreteType::eSHAPE));
	CHECK_EQ(ConcreteTypeOf(AuditedClass::TriangleMeshBv34), int(PxConcreteType::eTRIANGLE_MESH_BVH34));
	CHECK_EQ(ConcreteTypeOf(AuditedClass::Scene), -1);
	CHECK_STR(InterfaceName(AuditedClass::RigidStatic), "PxRigidStatic");
	CHECK_STR(InterfaceName(AuditedClass::TriangleMeshBv33), "PxBVH33TriangleMesh");
}

TEST(CheckObjectNeedsReadableMemoryAnAuditedVtableAndTheRightType)
{
	ReadableMemory    memory;
	FakeObject        shape{ kBases.physx3 + 0x20e9a8, std::uint16_t(PxConcreteType::eSHAPE) };
	const ObjectCheck check = CheckObject(memory, kBases, &shape);
	CHECK(check.verdict == ObjectVerdict::Ok && check.cls == AuditedClass::Shape);
	shape.type = std::uint16_t(PxConcreteType::eCONVEX_MESH);
	CHECK(CheckObject(memory, kBases, &shape).verdict == ObjectVerdict::WrongType);
	shape.vtable += 8;
	CHECK(CheckObject(memory, kBases, &shape).verdict == ObjectVerdict::Unaudited);
	CHECK(CheckObject(memory, kBases, nullptr).verdict == ObjectVerdict::Unreadable);
	FakeObject scene{ kBases.physx3 + 0x20caf0, 0xffff };  // not a PxBase object: no type ID to check
	CHECK(CheckObject(memory, kBases, &scene).verdict == ObjectVerdict::Ok);
}

TEST(CheckObjectCoversTheBytesTheCallerReads)
{
	// An actor whose first 16 bytes end a readable page: its userData (bytes 16 to 24) would fault.
	SYSTEM_INFO system{};
	GetSystemInfo(&system);
	const size_t page = system.dwPageSize;
	auto*        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, 2 * page, MEM_RESERVE, PAGE_NOACCESS));
	CHECK(base != nullptr);
	if (!base)
	{
		return;
	}
	VirtualAlloc(base, page, MEM_COMMIT, PAGE_READWRITE);
	auto* actor = reinterpret_cast<uintptr_t*>(base + page - 16);
	actor[0] = kBases.physx3 + 0x20b980;
	actor[1] = PxConcreteType::eRIGID_STATIC;
	ReadableMemory memory;
	CHECK(CheckObject(memory, kBases, actor).verdict == ObjectVerdict::Ok);
	CHECK(CheckObject(memory, kBases, actor, 24).verdict == ObjectVerdict::Unreadable);
	VirtualFree(base, 0, MEM_RELEASE);
}
