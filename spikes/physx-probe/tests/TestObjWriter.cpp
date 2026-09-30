#include "Harness.h"

#include "ObjWriter.h"

#include <sstream>

using namespace physx;
using cpmc::ObjGroup;
using cpmc::px::Tri;

TEST(ObjIsInMinecraftAxesRelativeToTheOriginWithSharedVertices)
{
	// World origin (10, 20, 30). A quad in the world XY plane, as two triangles sharing an edge, and one more group.
	const cpmc::Vec3d origin{ 10.0, 20.0, 30.0 };
	ObjGroup          quad{ "static_trimesh", cpmc::CategoryColor("static_trimesh"), {} };
	quad.triangles.push_back({ { PxVec3(10, 20, 30), PxVec3(11, 20, 30), PxVec3(10, 21, 30) } });
	quad.triangles.push_back({ { PxVec3(11, 20, 30), PxVec3(11, 21, 30), PxVec3(10, 21, 30) } });
	ObjGroup box{ "dynamic_box", cpmc::CategoryColor("dynamic_box"), {} };
	box.triangles.push_back({ { PxVec3(10, 20, 31.5f), PxVec3(10.25f, 20, 31.5f), PxVec3(10, 20, 30) } });
	// A face that collapses at millimetre precision is dropped.
	box.triangles.push_back({ { PxVec3(10, 20, 30), PxVec3(10.0001f, 20, 30), PxVec3(10, 20, 31.5f) } });

	std::ostringstream out;
	cpmc::WriteObj(out, { quad, box }, origin, "collision.mtl", "test");
	CHECK_STR(out.str(),
	          "# PhysXProbe collision dump\n"
	          "# test\n"
	          "mtllib collision.mtl\n"
	          "o static_trimesh\n"
	          "usemtl static_trimesh\n"
	          "v 0.000 0.000 0.000\n"
	          "v 1.000 0.000 0.000\n"
	          "v 0.000 0.000 -1.000\n"
	          "v 1.000 0.000 -1.000\n"
	          "f 1 2 3\n"
	          "f 2 4 3\n"
	          "o dynamic_box\n"
	          "usemtl dynamic_box\n"
	          "v 0.000 1.500 0.000\n"
	          "v 0.250 1.500 0.000\n"
	          "v 0.000 0.000 0.000\n"
	          "f 5 6 7\n");
}

TEST(MtlHasOneMaterialPerGroup)
{
	const ObjGroup     group{ "static_box", { 0.55f, 0.6f, 0.7f }, {} };
	std::ostringstream out;
	cpmc::WriteMtl(out, { group });
	CHECK_STR(out.str(), "newmtl static_box\nKd 0.550 0.600 0.700\n");
}

TEST(CategoryColorsIgnoreTheScenePrefix)
{
	CHECK(cpmc::CategoryColor("s1_trigger_box") == cpmc::CategoryColor("trigger_box"));
	CHECK(cpmc::CategoryColor("dynamic_convex") != cpmc::CategoryColor("static_convex"));
}
