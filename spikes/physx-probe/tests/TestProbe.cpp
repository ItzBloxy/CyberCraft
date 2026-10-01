#include "Harness.h"

#include "Probe.h"

using namespace physx;

TEST(PhysxCoordinatesAreWorldMinusTheOrigin)
{
	const PxVec3 same = cpmc::PhysxFromWorld({ 1.5, -2.0, 3.0 }, std::nullopt);
	CHECK_EQ(same.x, 1.5f);
	CHECK_EQ(same.y, -2.0f);
	CHECK_EQ(same.z, 3.0f);
	const PxVec3 shifted = cpmc::PhysxFromWorld({ -191.66, 691.56, 77.47 }, cpmc::Vec3d{ -200.0, 700.0, 70.0 });
	CHECK_NEAR(shifted.x, 8.34, 1e-4);
	CHECK_NEAR(shifted.y, -8.44, 1e-4);
	CHECK_NEAR(shifted.z, 7.47, 1e-4);
}

TEST(TrianglesMoveBackIntoWorldCoordinates)
{
	std::vector<cpmc::px::Tri> triangles{ { { PxVec3(0.0f, 0.0f, 0.0f), PxVec3(1.0f, 0.0f, 0.0f), PxVec3(0.0f, 1.0f, 0.0f) } } };
	cpmc::MoveToWorld(triangles, { 10.0, -20.0, 0.5 });
	CHECK_NEAR(triangles[0].v[0].z, 0.5, 1e-6);
	CHECK_NEAR(triangles[0].v[1].x, 11.0, 1e-6);
	CHECK_NEAR(triangles[0].v[2].y, -19.0, 1e-6);
}
