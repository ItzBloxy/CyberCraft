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
	CHECK(KeptShape(AuditedClass::RigidStatic, both, PxGeometryType::eTRIANGLEMESH));
	CHECK(KeptShape(AuditedClass::RigidStatic, queryOnly, PxGeometryType::eBOX));
	CHECK(!KeptShape(AuditedClass::RigidStatic, simulationOnly, PxGeometryType::eBOX));  // V's controller doesn't sweep against it
	CHECK(!KeptShape(AuditedClass::RigidStatic, trigger, PxGeometryType::eBOX));
	CHECK(!KeptShape(AuditedClass::RigidStatic, both, PxGeometryType::ePLANE));
	CHECK(!KeptShape(AuditedClass::RigidDynamic, both, PxGeometryType::eCONVEXMESH));  // cars, litter, controllers
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
