#include "Harness.h"

#include "DumpObj.h"

using namespace cpmc::mclink;

namespace
{
// A dump as the probe writes it (ObjWriter.cpp): one object a category, its vertices then its faces, the faces counting
// vertices from the file's first.
constexpr const char* kDump = "# PhysXProbe collision dump\n"
                              "# V's feet (world, metres, Z up): -189.898 616.754 74.432; axes: Minecraft (Y up)\n"
                              "mtllib collision.mtl\n"
                              "o s0_dynamic_box\n"
                              "usemtl s0_dynamic_box\n"
                              "v 1.000 0.000 1.000\n"
                              "v 2.000 0.000 1.000\n"
                              "v 2.000 1.000 1.000\n"
                              "f 1 2 3\n"
                              "o s0_static_trimesh\n"
                              "usemtl s0_static_trimesh\n"
                              "v -1.500 -0.120 -2.000\n"
                              "v 3.250 -0.120 -2.000\n"
                              "v 3.250 -0.120 4.000\n"
                              "v -1.500 -0.120 4.000\n"
                              "f 4 5 6\n"
                              "f 4 6 7\n"
                              "o s0_queryonly_capsule\n"
                              "usemtl s0_queryonly_capsule\n"
                              "v 0.000 0.000 0.000\n"
                              "f 1 4 8\n";
}

TEST(DumpObjKeepsTheObjectsAskedFor)
{
	std::string error;
	const auto  triangles = ParseDumpObj(kDump, "s0_static_", error);
	CHECK(triangles.has_value());
	CHECK_EQ(triangles.value_or(std::vector<cpmc::link::McTriangle>()).size(), size_t(2));
	if (triangles && triangles->size() == 2)
	{
		const float* first = triangles->at(0).v;
		CHECK(first[0] == -1.5f && first[1] == -0.12f && first[2] == -2.0f && first[3] == 3.25f && first[8] == 4.0f);
		const float* second = triangles->at(1).v;
		CHECK(second[6] == -1.5f && second[7] == -0.12f && second[8] == 4.0f);
	}
	CHECK_EQ(ParseDumpObj(kDump, "s0_", error).value_or(std::vector<cpmc::link::McTriangle>()).size(), size_t(4));
}

TEST(DumpObjRefusesWhatItCantRead)
{
	std::string error;
	CHECK(!ParseDumpObj("o s0_static_box\nv 1 2 3\nf 1 2 3\n", "s0_static_", error));  // vertices 2 and 3 aren't there
	CHECK_STR(error, "line 3: \"f 1 2 3\" isn't a vertex, a triangle of vertices before it, or an object");
	CHECK(!ParseDumpObj("v 1 2\n", "s0_static_", error));
	CHECK(!ParseDumpObj("v 1 2 3\nf 1 1 1 1\n", "s0_static_", error));
	CHECK(!ParseDumpObj("v nan 0 0\n", "s0_static_", error));
	CHECK(!ParseDumpObj("v 1 2 3\nf 0 1 1\n", "s0_static_", error));
	CHECK(ParseDumpObj("\r\n# a comment\r\n", "s0_static_", error).value_or(std::vector<cpmc::link::McTriangle>(1)).empty());
}
