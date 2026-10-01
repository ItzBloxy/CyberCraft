#include "Harness.h"

#include "physx/Checks.h"

using namespace physx;
using namespace cpmc::px;

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

TEST(SpreadGivesTheMinimumMedianAndMaximumOfEachAxis)
{
	const cpmc::Spread spread = SpreadOf({ { 1.0, 30.0, -5.0 }, { 3.0, 10.0, -1.0 }, { 2.0, 20.0, -3.0 } });
	CHECK_EQ(spread.count, uint64_t(3));
	CHECK_EQ(spread.min.x, 1.0);
	CHECK_EQ(spread.min.y, 10.0);
	CHECK_EQ(spread.min.z, -5.0);
	CHECK_EQ(spread.median.x, 2.0);
	CHECK_EQ(spread.median.y, 20.0);
	CHECK_EQ(spread.median.z, -3.0);
	CHECK_EQ(spread.max.x, 3.0);
	CHECK_EQ(spread.max.y, 30.0);
	CHECK_EQ(spread.max.z, -1.0);
}

TEST(SpreadOfAnEvenCountTakesTheLowerMiddle)
{
	CHECK_EQ(SpreadOf({ { 4.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 3.0, 0.0, 0.0 }, { 2.0, 0.0, 0.0 } }).median.x, 2.0);
	CHECK_EQ(SpreadOf({}).count, uint64_t(0));
}

TEST(FiniteBoundsLeaveOutPlanesAndEmptyActors)
{
	CHECK(FiniteBounds(PxBounds3(PxVec3(-1.0f), PxVec3(1.0f))));
	CHECK(FiniteBounds(PxBounds3(PxVec3(-3000.0f, 2000.0f, -50.0f), PxVec3(-2900.0f, 2400.0f, 300.0f))));
	CHECK(!FiniteBounds(PxBounds3(PxVec3(-PX_MAX_BOUNDS_EXTENTS), PxVec3(PX_MAX_BOUNDS_EXTENTS))));  // a plane's
	CHECK(!FiniteBounds(PxBounds3::empty()));  // an actor without shapes
	CHECK(!FiniteBounds(PxBounds3(PxVec3(0.0f), PxVec3(std::nanf(""), 0.0f, 0.0f))));
}
