#pragma once

// The NVIDIA PhysX 3.4.2 public headers BlockRender uses, as spike 1's probe did. CMake defines PX_PHYSX_STATIC_LIB,
// which empties the core API macro. BlockRender only calls PhysX through virtual tables or inline header code, so the
// built DLL imports nothing from PhysX3*.dll (check with dumpbin /imports).
#include <PxActor.h>
#include <PxAggregate.h>
#include <PxPhysics.h>
#include <PxQueryFiltering.h>
#include <PxQueryReport.h>
#include <PxRigidActor.h>
#include <PxRigidBody.h>
#include <PxRigidDynamic.h>
#include <PxRigidStatic.h>
#include <PxScene.h>
#include <PxShape.h>
#include <common/PxTolerancesScale.h>
#include <common/PxTypeInfo.h>
#include <foundation/PxBounds3.h>
#include <foundation/PxTransform.h>
#include <geometry/PxBoxGeometry.h>
#include <geometry/PxCapsuleGeometry.h>
#include <geometry/PxConvexMesh.h>
#include <geometry/PxConvexMeshGeometry.h>
#include <geometry/PxHeightField.h>
#include <geometry/PxHeightFieldGeometry.h>
#include <geometry/PxHeightFieldSample.h>
#include <geometry/PxMeshScale.h>
#include <geometry/PxPlaneGeometry.h>
#include <geometry/PxSphereGeometry.h>
#include <geometry/PxTriangleMesh.h>
#include <geometry/PxTriangleMeshGeometry.h>
