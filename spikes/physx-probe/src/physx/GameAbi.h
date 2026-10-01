#pragma once

#include "physx/PxHeaders.h"
#include "physx/Readable.h"

#include <cstddef>
#include <cstdint>
#include <optional>

// The game's PhysX build as audited in docs/spikes/2026-10-01-physx-abi-audit.md: the two DLLs' identities and the
// vtables of every class the probe calls into. The probe calls nothing on an object whose vtable isn't one of these.
namespace cpmc::px
{
// A module's PE timestamp and image size.
struct ModuleId
{
	uint32_t timestamp = 0;
	uint32_t imageSize = 0;

	bool operator==(const ModuleId&) const = default;
};

struct AuditedModule
{
	const wchar_t* name;
	ModuleId       id;
};

// PhysX3_x64.dll, then PhysX3Common_x64.dll, as audited.
inline constexpr AuditedModule kAuditedModules[2] = {
	{ L"PhysX3_x64.dll", { 0x64c978b4, 0x281000 } },
	{ L"PhysX3Common_x64.dll", { 0x64c978a8, 0x1f9000 } },
};

// The identity in the PE header of the module loaded at aBase; nothing unless it is a 64-bit PE image.
std::optional<ModuleId> ModuleIdentity(const void* aBase);

// Where the two DLLs are loaded.
struct ModuleBases
{
	uintptr_t physx3 = 0;
	uintptr_t common = 0;
};

enum class AuditedClass
{
	Physics,
	Scene,
	RigidStatic,
	RigidDynamic,
	Aggregate,
	Shape,
	ConvexMesh,
	TriangleMeshBv33,
	TriangleMeshBv34,
	HeightField,
};

// The audited class whose vtable starts at aVtable, if any.
std::optional<AuditedClass> AuditedClassOf(uintptr_t aVtable, const ModuleBases& aBases);

// The PxConcreteType ID the class's objects report, or -1 for the two classes that aren't PxBase objects.
int ConcreteTypeOf(AuditedClass aClass);

// The PhysX interface name, for reports: "PxRigidStatic", "PxShape", "PxBVH34TriangleMesh" and so on.
const char* InterfaceName(AuditedClass aClass);

enum class ObjectVerdict
{
	Ok,          // readable, an audited vtable, and (for PxBase objects) the type ID that vtable's class reports
	Unreadable,  // the bytes the caller reads aren't all readable
	Unaudited,   // the vtable isn't an audited table
	WrongType,   // an audited vtable, but the type ID disagrees with it
};

struct ObjectCheck
{
	ObjectVerdict verdict = ObjectVerdict::Unreadable;
	AuditedClass  cls = AuditedClass::Physics;  // meaningful for Ok and WrongType
};

// Checks a PhysX object before the probe touches it. At least its first 16 bytes must be readable: the vtable
// pointer and, for PxBase objects, the type ID at offset 8. Pass a larger aBytes to cover fields the caller reads
// itself, such as an actor's userData, which ends at 24.
ObjectCheck CheckObject(ReadableMemory& aMemory, const ModuleBases& aBases, const void* aObject, size_t aBytes = 16);
}
