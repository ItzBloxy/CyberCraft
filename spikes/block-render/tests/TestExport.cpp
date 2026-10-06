#include "Harness.h"

#include "physx/Export.h"

using namespace physx;
using namespace cpmc;
using namespace cpmc::px;

TEST(ExportKeepsStaticSceneQueryShapes)
{
	const PxShapeFlags both = PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE;
	const PxShapeFlags queryOnly(PxShapeFlag::eSCENE_QUERY_SHAPE);
	const PxShapeFlags simulationOnly(PxShapeFlag::eSIMULATION_SHAPE);
	const PxShapeFlags trigger = PxShapeFlag::eTRIGGER_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE;
	CHECK(KeptShape(AuditedClass::RigidStatic, both, PxGeometryType::eTRIANGLEMESH, false));
	CHECK(KeptShape(AuditedClass::RigidStatic, queryOnly, PxGeometryType::eBOX, false));
	CHECK(!KeptShape(AuditedClass::RigidStatic, simulationOnly, PxGeometryType::eBOX, false));  // V's controller doesn't sweep against it
	CHECK(!KeptShape(AuditedClass::RigidStatic, trigger, PxGeometryType::eBOX, false));
	CHECK(!KeptShape(AuditedClass::RigidStatic, both, PxGeometryType::ePLANE, false));
	CHECK(!KeptShape(AuditedClass::RigidDynamic, both, PxGeometryType::eCONVEXMESH, false));  // cars, litter, controllers
}

TEST(ExportLeavesOutBlockRendersOwnBodies)
{
	// Part 3's bodies are static scene-query shapes like the city's, made of what V stands on: the exporter leaves them
	// out, so Minecraft never gets its own blocks back as Night City's shape.
	const PxShapeFlags both = PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE;
	CHECK(!KeptShape(AuditedClass::RigidStatic, both, PxGeometryType::eBOX, true));
	int       a = 0, b = 0, c = 0;
	OwnActors own;
	CHECK(!own.Has(&a));
	own.Set({ &c, &a });
	CHECK(own.Has(&a) && own.Has(&c) && !own.Has(&b));
	CHECK_EQ(own.Size(), size_t(2));
	own.Set({});
	CHECK(!own.Has(&a));
}

TEST(ExportTurnsPhysxIntoMinecraftWithTheOrigin)
{
	// Past world y 512 the game's PhysX origin is (0, 1024, 0): world = PhysX + origin, and Minecraft's z is world -y.
	const Vec3d origin{ 0.0, 1024.0, 0.0 };
	const Vec3d mc = McFromPhysx(PxVec3(-189.9f, -407.25f, 74.4f), origin);
	CHECK_NEAR(mc.x, -189.9, 1e-4);
	CHECK_NEAR(mc.y, 74.4, 1e-4);
	CHECK_NEAR(mc.z, -616.75, 1e-4);
	// A box goes the other way: Minecraft z -620 to -604 is world y 604 to 620.
	const PxBounds3 box = PhysxBox({ { 100.0, 0.0, -620.0 }, { 116.0, 16.0, -604.0 } }, origin);
	CHECK_NEAR(box.minimum.x, 100.0, 1e-4);
	CHECK_NEAR(box.maximum.x, 116.0, 1e-4);
	CHECK_NEAR(box.minimum.y, 604.0 - 1024.0, 1e-4);
	CHECK_NEAR(box.maximum.y, 620.0 - 1024.0, 1e-4);
	CHECK_NEAR(box.minimum.z, 0.0, 1e-4);
	CHECK_NEAR(box.maximum.z, 16.0, 1e-4);
}

TEST(ExportNamesShapesForTheShapesRequest)
{
	CHECK_STR(GeometryName(PxGeometryType::eHEIGHTFIELD), "heightField");
	CHECK_STR(GeometryName(PxGeometryType::eTRIANGLEMESH), "triangleMesh");
	CHECK_STR(FilterKey(PxFilterData(0, 0, 0x449c004, 0)), "0:0:449c004:0");
}

TEST(ExportLeavesOutWhatTheBushRuleSays)
{
	// A bush's foliage, as Task 36's shapes found it: query filter the sight layer alone, simulation filter all zeros. The
	// terrain's heightfield and the bush's capsule have the sight layer among others, and real simulation filters.
	const PxFilterData foliageQuery(0, 0, 0x80000, 0), foliageSimulation(0, 0, 0, 0);
	const PxFilterData terrainQuery(0, 0, 0x449c080, 0), terrainSimulation(0x1c080, 0, 0x104a, 0);
	const PxFilterData capsuleQuery(0, 0, 0x4484000, 0), capsuleSimulation(0x4004, 0, 0x1e0c, 0);
	// Nothing: every static scene-query shape stays, as before.
	CHECK(!LeftOut(foliageQuery, foliageSimulation, LeaveOut::Nothing));
	// Sight-only, the default: the foliage goes, and the terrain and the capsule stay; so does a shape with the sight layer
	// alone but another word set.
	CHECK(LeftOut(foliageQuery, foliageSimulation, LeaveOut::SightOnly));
	CHECK(!LeftOut(terrainQuery, terrainSimulation, LeaveOut::SightOnly));
	CHECK(!LeftOut(capsuleQuery, capsuleSimulation, LeaveOut::SightOnly));
	CHECK(!LeftOut(PxFilterData(1, 0, 0x80000, 0), foliageSimulation, LeaveOut::SightOnly));
	// Unsimulated: whatever has an all-zero simulation filter goes, whatever its query filter.
	CHECK(LeftOut(foliageQuery, foliageSimulation, LeaveOut::Unsimulated));
	CHECK(LeftOut(terrainQuery, foliageSimulation, LeaveOut::Unsimulated));
	CHECK(!LeftOut(terrainQuery, terrainSimulation, LeaveOut::Unsimulated));
	// The exporter starts with the sight-only rule.
	Session     session;
	PhysxGround ground(session);
	CHECK(ground.Rule() == LeaveOut::SightOnly);
	ground.SetRule(LeaveOut::Nothing);
	CHECK(ground.Rule() == LeaveOut::Nothing);
	CHECK_STR(LeaveOutName(LeaveOut::SightOnly), "sight-only shapes");
	CHECK_STR(LeaveOutName(LeaveOut::Unsimulated), "unsimulated shapes");
	CHECK_STR(LeaveOutName(LeaveOut::Nothing), "nothing");
}

TEST(ExportDescribesEachPrimitiveShape)
{
	// PhysX's frame is the world less its origin; a capsule lies along its local x axis. Upright, as an NPC's controller
	// stands: its local x turned to world z.
	const Vec3d       origin{ 0.0, -2048.0, 0.0 };
	const PxTransform upright(PxVec3(-26.0f, -176.0f, 7.0f), PxQuat(-PxHalfPi, PxVec3(0.0f, 1.0f, 0.0f)));
	CHECK_STR(SphereWords(1.07f, PxTransform(PxVec3(-26.39f, -175.71f, 6.94f)), origin),
	          "sphere of radius 1.07 at (-26.39, -2223.71, 6.94)");
	CHECK_STR(CapsuleWords(0.3f, 0.5f, upright, origin), "capsule of radius 0.30 from (-26.00, -2224.00, 6.50) to (-26.00, -2224.00, 7.50)");
	CHECK_STR(BoxWords(PxVec3(1.0f, 0.25f, 1.5f), PxTransform(PxVec3(10.0f, 20.0f, 30.0f)), origin), "box of 2.00 x 0.50 x 3.00 at (10.00, -2028.00, 30.00)");
}

TEST(ExportReadsNothingWithoutTheGame)
{
	Session                       session;
	PhysxGround                   ground(session);
	std::vector<link::McTriangle> triangles;
	ground.Frame(std::nullopt);
	CHECK(!ground.Read(link::BatchBox({ 0, 0, 0 }), triangles));
	CHECK_STR(ground.State().why, "V isn't found");
	ground.Frame(Vec3d{ -119.5, -2007.5, 6.14 });
	CHECK(!ground.Read(link::BatchBox({ 0, 0, 0 }), triangles));
	CHECK_STR(ground.State().why, "PhysX3_x64.dll isn't loaded");
	CHECK(triangles.empty());
	CHECK_STR(ground.Describe({ 0.0, 0.0, 0.0 }, 2.0).front(), "none: PhysX3_x64.dll isn't loaded");
}
