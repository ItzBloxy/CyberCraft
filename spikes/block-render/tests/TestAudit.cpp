#include "Harness.h"

#include "physx/PxHeaders.h"
#include "physx/SceneWatch.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

// Our side of the PhysX ABI, checked against what the audit verified in the game's DLLs
// (docs/spikes/2026-10-01-physx-abi-audit.md): the vtable slot of every call made, the layout of the hit
// callbacks the game fills in and calls back through, and the structs both sides share.
//
// Slots are measured with a fake object whose vtable entries each record their own index. Each entry returns its
// second argument, which is where a call that returns a struct passes the hidden return pointer. Calling through a
// fake object is outside standard C++, but it compiles to exactly what BlockRender does to the game's objects.
using namespace physx;
using cpmc::px::InfoOf;
using cpmc::px::SceneCall;

namespace
{
int g_slot = -1;

template<int I>
void* Record(void*, void* aSecond)
{
	g_slot = I;
	return aSecond;
}

using Entry = void* (*)(void*, void*);

template<int... I>
constexpr std::array<Entry, sizeof...(I)> MakeVtable(std::integer_sequence<int, I...>)
{
	return { &Record<I>... };
}

constexpr auto kVtable = MakeVtable(std::make_integer_sequence<int, 128>{});

// A stand-in object: a vtable pointer, then the object's data from offset 8.
struct Fake
{
	const Entry*  vtable = kVtable.data();
	unsigned char body[256] = {};
};

// The vtable slot a call on a T goes through.
template<class T, class Call>
int SlotOf(Call aCall)
{
	Fake fake;
	g_slot = -1;
	aCall(*reinterpret_cast<T*>(&fake));
	return g_slot;
}

void Expect(const char* aCall, int aSlot, int aGameSlot)
{
	if (aSlot != aGameSlot)
	{
		th::Fail(__FILE__, __LINE__,
		         std::string(aCall) + " goes through slot " + std::to_string(aSlot) + "; the game's is " + std::to_string(aGameSlot));
	}
}

// The byte offset of a member within an object.
template<class T, class M>
size_t OffsetOf(const T& aObject, const M& aMember)
{
	return size_t(reinterpret_cast<const char*>(&aMember) - reinterpret_cast<const char*>(&aObject));
}
}

// The sizes of the structs the game copies into our memory, or reads from it.
static_assert(sizeof(PxOverlapHit) == 32);
static_assert(sizeof(PxRaycastHit) == 64);
static_assert(sizeof(PxBoxGeometry) == 16);
static_assert(sizeof(PxSphereGeometry) == 8);
static_assert(sizeof(PxCapsuleGeometry) == 12);
static_assert(sizeof(PxGeometryHolder) == 48);
static_assert(sizeof(PxConvexMeshGeometry) == 48);
static_assert(sizeof(PxTriangleMeshGeometry) == 48);
static_assert(sizeof(PxHeightFieldGeometry) == 32);
static_assert(sizeof(PxHullPolygon) == 20);
static_assert(sizeof(PxHeightFieldSample) == 4);
static_assert(sizeof(PxTransform) == 28);
static_assert(sizeof(PxFilterData) == 16);

TEST(PhysicsCallsUseTheGamesSlots)
{
	Expect("PxPhysics::getTolerancesScale", SlotOf<PxPhysics>([](PxPhysics& a) { (void)&a.getTolerancesScale(); }), 4);
	Expect("PxPhysics::getNbScenes", SlotOf<PxPhysics>([](PxPhysics& a) { (void)a.getNbScenes(); }), 19);
	Expect("PxPhysics::getScenes", SlotOf<PxPhysics>([](PxPhysics& a) { (void)a.getScenes(nullptr, 0); }), 20);
}

TEST(SceneCallsUseTheGamesSlots)
{
	const PxActorTypeFlags rigid = PxActorTypeFlag::eRIGID_STATIC | PxActorTypeFlag::eRIGID_DYNAMIC;
	const PxBoxGeometry    box(1.0f, 1.0f, 1.0f);
	const PxTransform      pose(PxIdentity);
	PxOverlapBuffer        overlaps;
	PxRaycastBuffer        ray;
	Expect("PxScene::getFlags", SlotOf<PxScene>([](PxScene& a) { (void)a.getFlags(); }), 3);
	Expect("PxScene::getNbActors", SlotOf<PxScene>([&](PxScene& a) { (void)a.getNbActors(rigid); }), 18);
	Expect("PxScene::getActors", SlotOf<PxScene>([&](PxScene& a) { (void)a.getActors(rigid, nullptr, 0); }), 19);
	Expect("PxScene::getNbArticulations", SlotOf<PxScene>([](PxScene& a) { (void)a.getNbArticulations(); }), 22);
	Expect("PxScene::getNbAggregates", SlotOf<PxScene>([](PxScene& a) { (void)a.getNbAggregates(); }), 26);
	Expect("PxScene::getAggregates", SlotOf<PxScene>([](PxScene& a) { (void)a.getAggregates(nullptr, 0); }), 27);
	Expect("PxScene::getGravity", SlotOf<PxScene>([](PxScene& a) { (void)a.getGravity(); }), 67);
	Expect("PxScene::raycast", SlotOf<PxScene>([&](PxScene& a) { (void)a.raycast(PxVec3(0.0f), PxVec3(0.0f, 0.0f, -1.0f), 1.0f, ray); }), 94);
	Expect("PxScene::overlap", SlotOf<PxScene>([&](PxScene& a) { (void)a.overlap(box, pose, overlaps); }), 96);
	Expect("PxScene::lockRead", SlotOf<PxScene>([](PxScene& a) { a.lockRead(); }), 105);
	Expect("PxScene::unlockRead", SlotOf<PxScene>([](PxScene& a) { a.unlockRead(); }), 106);
}

TEST(SceneWatchHooksTheGamesSlots)
{
	// The scene watch's hooks go on these slots, and call on with these signatures (physx/SceneHooks.cpp).
	Fake                       dummy;  // an actor, an aggregate and a pruning structure: never touched
	PxActor&                   actor = reinterpret_cast<PxActor&>(dummy);
	PxActor* const             actors[1] = { &actor };
	PxAggregate&               aggregate = reinterpret_cast<PxAggregate&>(dummy);
	const PxPruningStructure&  pruning = reinterpret_cast<const PxPruningStructure&>(dummy);
	const PxContactPairHeader* pairs = nullptr;
	PxU32                      pairCount = 0;
	const PxBoxGeometry        box(1.0f, 1.0f, 1.0f);
	const PxTransform          pose(PxIdentity);
	PxRaycastBuffer            ray;
	PxSweepBuffer              sweep;
	PxOverlapBuffer            overlaps;
	const auto expect = [](SceneCall aCall, int aSlot) { Expect(InfoOf(aCall).name, aSlot, int(InfoOf(aCall).slot)); };
	expect(SceneCall::AddActor, SlotOf<PxScene>([&](PxScene& a) { a.addActor(actor); }));
	expect(SceneCall::AddActorsPruning, SlotOf<PxScene>([&](PxScene& a) { a.addActors(pruning); }));
	expect(SceneCall::AddActors, SlotOf<PxScene>([&](PxScene& a) { a.addActors(actors, 1); }));
	expect(SceneCall::RemoveActor, SlotOf<PxScene>([&](PxScene& a) { a.removeActor(actor); }));
	expect(SceneCall::RemoveActors, SlotOf<PxScene>([&](PxScene& a) { a.removeActors(actors, 1); }));
	expect(SceneCall::AddAggregate, SlotOf<PxScene>([&](PxScene& a) { a.addAggregate(aggregate); }));
	expect(SceneCall::RemoveAggregate, SlotOf<PxScene>([&](PxScene& a) { a.removeAggregate(aggregate); }));
	expect(SceneCall::Simulate, SlotOf<PxScene>([](PxScene& a) { a.simulate(1.0f / 60.0f); }));
	expect(SceneCall::Advance, SlotOf<PxScene>([](PxScene& a) { a.advance(); }));
	expect(SceneCall::Collide, SlotOf<PxScene>([](PxScene& a) { a.collide(1.0f / 60.0f); }));
	expect(SceneCall::FetchResults, SlotOf<PxScene>([](PxScene& a) { (void)a.fetchResults(); }));
	expect(SceneCall::FetchResultsStart, SlotOf<PxScene>([&](PxScene& a) { (void)a.fetchResultsStart(pairs, pairCount); }));
	expect(SceneCall::FetchResultsFinish, SlotOf<PxScene>([](PxScene& a) { a.fetchResultsFinish(); }));
	expect(SceneCall::Raycast, SlotOf<PxScene>([&](PxScene& a) { (void)a.raycast(PxVec3(0.0f), PxVec3(0.0f, 0.0f, -1.0f), 1.0f, ray); }));
	expect(SceneCall::Sweep, SlotOf<PxScene>([&](PxScene& a) { (void)a.sweep(box, pose, PxVec3(1.0f, 0.0f, 0.0f), 1.0f, sweep); }));
	expect(SceneCall::Overlap, SlotOf<PxScene>([&](PxScene& a) { (void)a.overlap(box, pose, overlaps); }));
	expect(SceneCall::LockWrite, SlotOf<PxScene>([](PxScene& a) { a.lockWrite(); }));
	expect(SceneCall::UnlockWrite, SlotOf<PxScene>([](PxScene& a) { a.unlockWrite(); }));
	expect(SceneCall::ShiftOrigin, SlotOf<PxScene>([](PxScene& a) { a.shiftOrigin(PxVec3(0.0f, 1024.0f, 0.0f)); }));
}

TEST(BodyCallsUseTheGamesSlots)
{
	// Part 3's bodies (physx/Bodies.cpp) are made, shaped, posed and released through these slots, as the audit's second
	// addendum found them in the game's PhysX3_x64.dll. Each PxShape slot is one past NVIDIA's header's (see PxShape.h).
	Fake                dummy;  // a shape: never touched
	PxShape&            shape = reinterpret_cast<PxShape&>(dummy);
	PxMaterial*         material = nullptr;
	const PxBoxGeometry box(0.5f, 0.5f, 0.5f);
	const PxTransform   pose(PxIdentity);
	const PxFilterData  filter;
	Expect("PxPhysics::createRigidStatic", SlotOf<PxPhysics>([&](PxPhysics& a) { (void)a.createRigidStatic(pose); }), 21);
	Expect("PxPhysics::createShape",
	       SlotOf<PxPhysics>([&](PxPhysics& a) { (void)a.createShape(box, &material, 1, true, PxShapeFlag::eSCENE_QUERY_SHAPE); }), 27);
	Expect("PxActor::release", SlotOf<PxActor>([](PxActor& a) { a.release(); }), 0);
	Expect("PxActor::getScene", SlotOf<PxActor>([](PxActor& a) { (void)a.getScene(); }), 6);
	Expect("PxRigidActor::setGlobalPose", SlotOf<PxRigidActor>([&](PxRigidActor& a) { a.setGlobalPose(pose); }), 21);
	Expect("PxRigidActor::attachShape", SlotOf<PxRigidActor>([&](PxRigidActor& a) { a.attachShape(shape); }), 23);
	Expect("PxShape::release", SlotOf<PxShape>([](PxShape& a) { a.release(); }), 0);
	Expect("PxShape::setLocalPose", SlotOf<PxShape>([&](PxShape& a) { a.setLocalPose(pose); }), 18);
	Expect("PxShape::setSimulationFilterData", SlotOf<PxShape>([&](PxShape& a) { a.setSimulationFilterData(filter); }), 20);
	Expect("PxShape::setQueryFilterData", SlotOf<PxShape>([&](PxShape& a) { a.setQueryFilterData(filter); }), 22);
	Expect("PxShape::getMaterials", SlotOf<PxShape>([&](PxShape& a) { (void)a.getMaterials(&material, 1); }), 26);
}

TEST(ActorCallsUseTheGamesSlots)
{
	Expect("PxActor::getName", SlotOf<PxActor>([](PxActor& a) { (void)a.getName(); }), 8);
	Expect("PxActor::getWorldBounds", SlotOf<PxActor>([](PxActor& a) { (void)a.getWorldBounds(1.0f); }), 9);
	Expect("PxRigidActor::getGlobalPose", SlotOf<PxRigidActor>([](PxRigidActor& a) { (void)a.getGlobalPose(); }), 20);
	Expect("PxRigidActor::getNbShapes", SlotOf<PxRigidActor>([](PxRigidActor& a) { (void)a.getNbShapes(); }), 25);
	Expect("PxRigidActor::getShapes", SlotOf<PxRigidActor>([](PxRigidActor& a) { (void)a.getShapes(nullptr, 0); }), 26);
	Expect("PxRigidBody::getRigidBodyFlags", SlotOf<PxRigidBody>([](PxRigidBody& a) { (void)a.getRigidBodyFlags(); }), 47);
	Expect("PxAggregate::getNbActors", SlotOf<PxAggregate>([](PxAggregate& a) { (void)a.getNbActors(); }), 9);
}

TEST(ShapeCallsUseTheGamesSlots)
{
	// The game's PxShape has a reference-count getter at slot 6 that NVIDIA's public headers lack (see PxShape.h).
	PxBoxGeometry          box;
	PxSphereGeometry       sphere;
	PxCapsuleGeometry      capsule;
	PxConvexMeshGeometry   convex;
	PxTriangleMeshGeometry mesh;
	PxHeightFieldGeometry  field;
	Expect("PxShape::getGeometryType", SlotOf<PxShape>([](PxShape& a) { (void)a.getGeometryType(); }), 7);
	Expect("PxShape::getBoxGeometry", SlotOf<PxShape>([&](PxShape& a) { (void)a.getBoxGeometry(box); }), 10);
	Expect("PxShape::getSphereGeometry", SlotOf<PxShape>([&](PxShape& a) { (void)a.getSphereGeometry(sphere); }), 11);
	Expect("PxShape::getCapsuleGeometry", SlotOf<PxShape>([&](PxShape& a) { (void)a.getCapsuleGeometry(capsule); }), 12);
	Expect("PxShape::getConvexMeshGeometry", SlotOf<PxShape>([&](PxShape& a) { (void)a.getConvexMeshGeometry(convex); }), 14);
	Expect("PxShape::getTriangleMeshGeometry", SlotOf<PxShape>([&](PxShape& a) { (void)a.getTriangleMeshGeometry(mesh); }), 15);
	Expect("PxShape::getHeightFieldGeometry", SlotOf<PxShape>([&](PxShape& a) { (void)a.getHeightFieldGeometry(field); }), 16);
	Expect("PxShape::getLocalPose", SlotOf<PxShape>([](PxShape& a) { (void)a.getLocalPose(); }), 19);
	Expect("PxShape::getSimulationFilterData", SlotOf<PxShape>([](PxShape& a) { (void)a.getSimulationFilterData(); }), 21);
	Expect("PxShape::getQueryFilterData", SlotOf<PxShape>([](PxShape& a) { (void)a.getQueryFilterData(); }), 23);
	Expect("PxShape::getFlags", SlotOf<PxShape>([](PxShape& a) { (void)a.getFlags(); }), 34);
}

TEST(MeshCallsUseTheGamesSlots)
{
	PxHullPolygon polygon;
	Expect("PxConvexMesh::getNbVertices", SlotOf<PxConvexMesh>([](PxConvexMesh& a) { (void)a.getNbVertices(); }), 5);
	Expect("PxConvexMesh::getVertices", SlotOf<PxConvexMesh>([](PxConvexMesh& a) { (void)a.getVertices(); }), 6);
	Expect("PxConvexMesh::getIndexBuffer", SlotOf<PxConvexMesh>([](PxConvexMesh& a) { (void)a.getIndexBuffer(); }), 7);
	Expect("PxConvexMesh::getNbPolygons", SlotOf<PxConvexMesh>([](PxConvexMesh& a) { (void)a.getNbPolygons(); }), 8);
	Expect("PxConvexMesh::getPolygonData", SlotOf<PxConvexMesh>([&](PxConvexMesh& a) { (void)a.getPolygonData(0, polygon); }), 9);
	Expect("PxTriangleMesh::getNbVertices", SlotOf<PxTriangleMesh>([](PxTriangleMesh& a) { (void)a.getNbVertices(); }), 5);
	Expect("PxTriangleMesh::getVertices", SlotOf<PxTriangleMesh>([](PxTriangleMesh& a) { (void)a.getVertices(); }), 6);
	Expect("PxTriangleMesh::getNbTriangles", SlotOf<PxTriangleMesh>([](PxTriangleMesh& a) { (void)a.getNbTriangles(); }), 9);
	Expect("PxTriangleMesh::getTriangles", SlotOf<PxTriangleMesh>([](PxTriangleMesh& a) { (void)a.getTriangles(); }), 10);
	Expect("PxTriangleMesh::getTriangleMeshFlags",
	       SlotOf<PxTriangleMesh>([](PxTriangleMesh& a) { (void)a.getTriangleMeshFlags(); }), 11);
	Expect("PxHeightField::getNbRows", SlotOf<PxHeightField>([](PxHeightField& a) { (void)a.getNbRows(); }), 7);
	Expect("PxHeightField::getNbColumns", SlotOf<PxHeightField>([](PxHeightField& a) { (void)a.getNbColumns(); }), 8);
	Expect("PxHeightField::getSample", SlotOf<PxHeightField>([](PxHeightField& a) { (void)&a.getSample(1, 2); }), 19);
}

TEST(HitCallbacksHaveTheGamesLayout)
{
	// The game writes hits into these objects and calls processTouches (slot 0) and finalizeQuery (slot 1) on them.
	struct Overlaps final : PxHitCallback<PxOverlapHit>
	{
		Overlaps()
			: PxHitCallback<PxOverlapHit>(nullptr, 0)
		{
		}
		PxAgain processTouches(const PxOverlapHit*, PxU32) override
		{
			g_slot = 0;
			return true;
		}
		void finalizeQuery() override { g_slot = 1; }
	};
	Overlaps   overlaps;
	auto*      vtable = *reinterpret_cast<void* const* const*>(&overlaps);
	const auto touches = reinterpret_cast<PxAgain (*)(void*, const PxOverlapHit*, PxU32)>(vtable[0]);
	const auto finalize = reinterpret_cast<void (*)(void*)>(vtable[1]);
	g_slot = -1;
	touches(&overlaps, nullptr, 0);
	CHECK_EQ(g_slot, 0);
	finalize(&overlaps);
	CHECK_EQ(g_slot, 1);

	CHECK_EQ(OffsetOf(overlaps, overlaps.block), size_t(8));
	CHECK_EQ(OffsetOf(overlaps, overlaps.hasBlock), size_t(40));
	CHECK_EQ(OffsetOf(overlaps, overlaps.touches), size_t(48));
	CHECK_EQ(OffsetOf(overlaps, overlaps.maxNbTouches), size_t(56));
	CHECK_EQ(OffsetOf(overlaps, overlaps.nbTouches), size_t(60));

	PxRaycastBuffer ray;
	CHECK_EQ(OffsetOf(ray, ray.block), size_t(8));
	CHECK_EQ(OffsetOf(ray.block, ray.block.distance), size_t(52));
	CHECK_EQ(OffsetOf(ray, ray.hasBlock), size_t(72));
	CHECK_EQ(OffsetOf(ray, ray.touches), size_t(80));
	CHECK_EQ(OffsetOf(ray, ray.maxNbTouches), size_t(88));
	CHECK_EQ(OffsetOf(ray, ray.nbTouches), size_t(92));
}

TEST(ObjectFieldsHaveTheGamesLayout)
{
	// Two fields are read straight from game objects: PxBase's type ID and PxActor::userData.
	Fake                 object;
	const std::uint16_t type = PxConcreteType::eSHAPE;
	std::memcpy(object.body, &type, sizeof(type));  // offset 8
	CHECK_EQ(int(reinterpret_cast<const PxBase&>(object).getConcreteType()), int(PxConcreteType::eSHAPE));
	const auto& actor = reinterpret_cast<const PxActor&>(object);
	CHECK_EQ(OffsetOf(actor, actor.userData), size_t(16));
}

TEST(SharedStructsHaveTheGamesLayout)
{
	// What the game's getters copy into our structs, byte for byte.
	PxConvexMeshGeometry convex;
	CHECK_EQ(OffsetOf(convex, convex.convexMesh), size_t(32));
	CHECK_EQ(OffsetOf(convex, convex.maxMargin), size_t(40));
	CHECK_EQ(OffsetOf(convex, convex.meshFlags), size_t(44));
	PxTriangleMeshGeometry mesh;
	CHECK_EQ(OffsetOf(mesh, mesh.meshFlags), size_t(32));
	CHECK_EQ(OffsetOf(mesh, mesh.triangleMesh), size_t(40));
	PxHeightFieldGeometry field;
	CHECK_EQ(OffsetOf(field, field.heightField), size_t(8));
	CHECK_EQ(OffsetOf(field, field.heightScale), size_t(16));
	CHECK_EQ(OffsetOf(field, field.heightFieldFlags), size_t(28));
	PxHullPolygon polygon;
	CHECK_EQ(OffsetOf(polygon, polygon.mNbVerts), size_t(16));
	CHECK_EQ(OffsetOf(polygon, polygon.mIndexBase), size_t(18));
	PxQueryFilterData filter;
	CHECK_EQ(OffsetOf(filter, filter.flags), size_t(16));
	PxTolerancesScale scale;
	CHECK_EQ(OffsetOf(scale, scale.length), size_t(0));
	CHECK_EQ(OffsetOf(scale, scale.speed), size_t(8));
}
