#include "Harness.h"

#include "physx/Checks.h"

using namespace physx;
using namespace cpmc::px;

TEST(ConcreteTypeNamesMapToTheirIds)
{
	CHECK_EQ(ConcreteTypeForName("PxRigidStatic"), int(PxConcreteType::eRIGID_STATIC));
	CHECK_EQ(ConcreteTypeForName("PxRigidDynamic"), int(PxConcreteType::eRIGID_DYNAMIC));
	CHECK_EQ(ConcreteTypeForName("PxShape"), int(PxConcreteType::eSHAPE));
	CHECK_EQ(ConcreteTypeForName("PxBVH33TriangleMesh"), int(PxConcreteType::eTRIANGLE_MESH_BVH33));
	CHECK_EQ(ConcreteTypeForName("PxBVH34TriangleMesh"), int(PxConcreteType::eTRIANGLE_MESH_BVH34));
	CHECK_EQ(ConcreteTypeForName("PxHeightField"), int(PxConcreteType::eHEIGHTFIELD));
	CHECK_EQ(ConcreteTypeForName("PxConvexMesh"), int(PxConcreteType::eCONVEX_MESH));
	CHECK_EQ(ConcreteTypeForName("PxTriangleMesh"), -1);  // an interface name, never a concrete type
	CHECK_EQ(ConcreteTypeForName("garbage"), -1);
	CHECK_EQ(ConcreteTypeForName(nullptr), -1);
}

TEST(PhysXVersionComesFromTheFileVersionString)
{
	// NVIDIA's 3.4.2 release says 3.4.2.0 in the FileVersion string but 3.4.1.0 in the binary field.
	CHECK(IsPhysX342("3.4.2.0"));
	CHECK(!IsPhysX342("3.4.1.0"));
	CHECK(!IsPhysX342("13.4.2.0"));
	CHECK(!IsPhysX342(""));
}

TEST(PlausibilityRanges)
{
	CHECK(TolerancesPlausible(1.0f, 10.0f));
	CHECK(!TolerancesPlausible(0.0f, 10.0f));
	CHECK(!TolerancesPlausible(1.0f, 5000.0f));
	CHECK(GravityFinite(PxVec3(0.0f, 0.0f, -9.81f)));
	CHECK(GravityFinite(PxVec3(0.0f)));
	CHECK(!GravityFinite(PxVec3(0.0f, 0.0f, std::nanf(""))));
	CHECK(!GravityFinite(PxVec3(0.0f, 0.0f, -1000.0f)));
	CHECK(GravityEarthLike(PxVec3(0.0f, 0.0f, -9.81f)));
	CHECK(!GravityEarthLike(PxVec3(0.0f)));
}

TEST(GeometryMustMatchItsMesh)
{
	CHECK(GeometryMatchesMesh(PxGeometryType::eTRIANGLEMESH, PxConcreteType::eTRIANGLE_MESH_BVH33));
	CHECK(GeometryMatchesMesh(PxGeometryType::eTRIANGLEMESH, PxConcreteType::eTRIANGLE_MESH_BVH34));
	CHECK(GeometryMatchesMesh(PxGeometryType::eCONVEXMESH, PxConcreteType::eCONVEX_MESH));
	CHECK(GeometryMatchesMesh(PxGeometryType::eHEIGHTFIELD, PxConcreteType::eHEIGHTFIELD));
	CHECK(!GeometryMatchesMesh(PxGeometryType::eTRIANGLEMESH, PxConcreteType::eCONVEX_MESH));
	CHECK(!GeometryMatchesMesh(PxGeometryType::eBOX, PxConcreteType::eCONVEX_MESH));
}

TEST(CategoriesFollowTheKindOrder)
{
	const PxShapeFlags trigger = PxShapeFlag::eTRIGGER_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE;
	const PxShapeFlags queryOnly = PxShapeFlag::eSCENE_QUERY_SHAPE;
	const PxShapeFlags solid = PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE;
	CHECK_STR(Category("static", trigger, PxGeometryType::eBOX), "trigger_box");
	CHECK_STR(Category("dynamic", queryOnly, PxGeometryType::eCONVEXMESH), "queryonly_convex");
	CHECK_STR(Category("static", solid, PxGeometryType::eTRIANGLEMESH), "static_trimesh");
	CHECK_STR(Category("kinematic", solid, PxGeometryType::eCAPSULE), "kinematic_capsule");
	CHECK_STR(Category("static", solid, PxGeometryType::eHEIGHTFIELD), "static_heightfield");
	CHECK_STR(GeometryName(PxGeometryType::eHEIGHTFIELD), "heightField");
}

TEST(FilterKeysAreHex)
{
	CHECK_STR(FilterKey(PxFilterData(1, 0xff, 0, 0x10)), "1:ff:0:10");
}

TEST(SanitizeNameKeepsPrintableAscii)
{
	CHECK_STR(SanitizeName("base\\world\\door_01"), "base\\world\\door_01");
	CHECK_STR(SanitizeName("a\tb\x01" "c"), "a?b?c");
	CHECK_STR(SanitizeName("abcdef", 3), "abc");
	CHECK_STR(SanitizeName(nullptr), "");
}

TEST(TrimTallyKeepsTheMostCommon)
{
	const cpmc::Tally tally{ { "a", 5 }, { "b", 1 }, { "c", 9 }, { "d", 1 } };
	const cpmc::Tally trimmed = TrimTally(tally, 2);
	CHECK_EQ(trimmed.size(), size_t(3));
	CHECK_EQ(trimmed.at("c"), uint64_t(9));
	CHECK_EQ(trimmed.at("a"), uint64_t(5));
	CHECK_EQ(trimmed.at("other"), uint64_t(2));
	CHECK_EQ(TrimTally(tally, 10).size(), size_t(4));  // nothing to fold, so no "other"
}
