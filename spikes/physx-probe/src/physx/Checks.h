#pragma once

#include "Report.h"
#include "physx/PxHeaders.h"

#include <cstddef>
#include <string>

// The pure parts of the compatibility check and the census bookkeeping. No PhysX calls happen here.
namespace cpmc::px
{
// The PhysX 3.4.2 concrete type ID (PxConcreteType) for a type name, or -1 for a name 3.4.2 doesn't have.
int ConcreteTypeForName(const char* aName);

// PhysX 3.4.2, going by the "FileVersion" string of its version resource. NVIDIA's 3.4.2 release left the binary
// FILEVERSION field at 3.4.1.0 (see its PhysX3.rc), so that field can't tell 3.4.1 and 3.4.2 apart.
bool IsPhysX342(const std::string& aFileVersionString);

// PxTolerancesScale: length 0.01 .. 100, speed 0.1 .. 1000.
bool TolerancesPlausible(float aLength, float aSpeed);
// Finite, with a magnitude of at most 100.
bool GravityFinite(const physx::PxVec3& aGravity);
// A magnitude between 1 and 30.
bool GravityEarthLike(const physx::PxVec3& aGravity);

// A mesh-based geometry type references a mesh of the matching concrete type.
bool GeometryMatchesMesh(physx::PxGeometryType::Enum aGeometry, int aMeshConcreteType);

// "triangleMesh", "heightField", "convexMesh", "box", "sphere", "capsule", "plane" (report tallies).
const char* GeometryName(physx::PxGeometryType::Enum aGeometry);
// "trimesh", "heightfield", "convex", "box", "sphere", "capsule", "plane" (OBJ categories).
const char* GeometryShortName(physx::PxGeometryType::Enum aGeometry);

// The category of a shape: "trigger" if it has the trigger flag, otherwise "queryonly" if it has the
// scene-query flag but not the simulation flag, otherwise the actor's kind; then "_" and the short geometry name.
std::string Category(const char* aActorKind, physx::PxShapeFlags aShapeFlags, physx::PxGeometryType::Enum aGeometry);

// Filter data as "w0:w1:w2:w3" in hexadecimal.
std::string FilterKey(const physx::PxFilterData& aData);

// At most aMaxLength characters up to the first NUL; bytes outside printable ASCII become '?'.
std::string SanitizeName(const char* aName, size_t aMaxLength = 128);

// Keeps the aKeep most common entries (ties by name) and folds the rest into "other".
Tally TrimTally(const Tally& aTally, size_t aKeep = 64);
}
