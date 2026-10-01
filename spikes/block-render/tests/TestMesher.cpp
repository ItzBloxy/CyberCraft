#include "Harness.h"

#include "AtlasFixture.h"
#include "world/Mesher.h"

#include <cstddef>

using cpmc::BlockWorld;
using cpmc::MeshSection;
using cpmc::Vertex;

// SkyCraft's byte layout (the spec's "Vertex format").
static_assert(offsetof(Vertex, x) == 0);
static_assert(offsetof(Vertex, u) == 12);
static_assert(offsetof(Vertex, r) == 20);
static_assert(offsetof(Vertex, a) == 23);
static_assert(offsetof(Vertex, light) == 24);
static_assert(offsetof(Vertex, flags) == 28);

namespace
{
const cpmc::Atlas& TestAtlas()
{
	static const cpmc::Atlas atlas = fixture::Atlas();
	return atlas;
}

uint16_t Id(const char* aName)
{
	return *TestAtlas().BlockId(aName);
}

std::vector<Vertex> Mesh(const BlockWorld& aWorld, bool aOcclusion = true, cpmc::SectionPos aSection = {})
{
	return MeshSection(aWorld, aSection, TestAtlas(), { aOcclusion });
}

size_t Faces(const std::vector<Vertex>& aVertices)
{
	return aVertices.size() / 6;
}
}

TEST(MesherGivesALoneBlockSixFaces)
{
	BlockWorld world;
	world.Set({ 2, 3, 4 }, Id("stone"));
	const auto vertices = Mesh(world);
	CHECK_EQ(vertices.size(), size_t(36));
	int perFace[7] = {};
	for (const Vertex& v : vertices)
	{
		CHECK(v.x >= 2.0f && v.x <= 3.0f && v.y >= 3.0f && v.y <= 4.0f && v.z >= 4.0f && v.z <= 5.0f);
		CHECK_EQ(v.flags & 1u, 1u);  // alpha-tested
		CHECK_EQ(v.light, 15u << 8);
		CHECK_EQ(int(v.r), 255);  // no tint, nothing nearby to occlude
		++perFace[(v.flags >> 4) & 15];
	}
	for (int face = 1; face <= 6; ++face)
	{
		CHECK_EQ(perFace[face], 6);
	}
	CHECK(MeshSection(world, { 1, 0, 0 }, TestAtlas(), {}).empty());
}

TEST(MesherHidesFacesBetweenOpaqueNeighbours)
{
	BlockWorld world;
	world.Set({ 0, 0, 0 }, Id("stone"));
	world.Set({ 1, 0, 0 }, Id("stone"));
	CHECK_EQ(Faces(Mesh(world)), size_t(10));
}

TEST(MesherHidesFacesAgainstTheNextSection)
{
	BlockWorld world;
	world.Set({ 15, 0, 0 }, Id("stone"));
	world.Set({ 16, 0, 0 }, Id("stone"));
	CHECK_EQ(Faces(Mesh(world, true, { 0, 0, 0 })), size_t(5));
	CHECK_EQ(Faces(Mesh(world, true, { 1, 0, 0 })), size_t(5));
}

TEST(MesherLetsCutoutBlocksShowWhatsBehindThem)
{
	BlockWorld glassAndStone;
	glassAndStone.Set({ 0, 0, 0 }, Id("glass"));
	glassAndStone.Set({ 1, 0, 0 }, Id("stone"));
	CHECK_EQ(Faces(Mesh(glassAndStone)), size_t(11));  // the stone's face shows through the glass

	BlockWorld twoGlass;
	twoGlass.Set({ 0, 0, 0 }, Id("glass"));
	twoGlass.Set({ 1, 0, 0 }, Id("glass"));
	CHECK_EQ(Faces(Mesh(twoGlass)), size_t(10));  // glass hides glass

	BlockWorld glassAndLeaves;
	glassAndLeaves.Set({ 0, 0, 0 }, Id("glass"));
	glassAndLeaves.Set({ 1, 0, 0 }, Id("oak_leaves"));
	CHECK_EQ(Faces(Mesh(glassAndLeaves)), size_t(12));  // different cut-out blocks hide nothing
}

TEST(MesherTintsOnlyTheTintedFaces)
{
	BlockWorld world;
	world.Set({ 0, 0, 0 }, Id("grass_block"));
	for (const Vertex& v : Mesh(world))
	{
		const uint32_t face = ((v.flags >> 4) & 15) - 1;
		if (face == uint32_t(cpmc::Face::Up))
		{
			CHECK_EQ(int(v.r), 0x91);
			CHECK_EQ(int(v.g), 0xBD);
			CHECK_EQ(int(v.b), 0x59);
		}
		else
		{
			CHECK_EQ(int(v.g), 255);
		}
	}
}

TEST(MesherMapsEachFaceToItsSprite)
{
	BlockWorld world;
	world.Set({ 0, 0, 0 }, Id("oak_log"));
	for (const Vertex& v : Mesh(world))
	{
		const uint32_t face = ((v.flags >> 4) & 15) - 1;
		const float    left = (face <= 1 ? 6.0f : 7.0f) * 16.0f / 128.0f;  // log top is sprite 6, log side sprite 7
		CHECK(v.u >= left - 1e-6f && v.u <= left + 16.0f / 128.0f + 1e-6f);
		CHECK(v.v >= 0.0f && v.v <= 1.0f);
	}
}

TEST(MesherShadesCornersLikeMinecraft)
{
	BlockWorld world;
	world.Set({ 0, 0, 0 }, Id("stone"));
	world.Set({ 1, 1, 0 }, Id("stone"));  // above the east half of the top face
	const auto vertices = Mesh(world);
	int        dark = 0;
	for (size_t i = 6; i < 12; ++i)  // the first block's top face
	{
		const Vertex& v = vertices[i];
		CHECK_EQ(int(v.r), v.x == 1.0f ? 204 : 255);  // (1 + 0.2 + 1 + 1) / 4 = 0.8
		dark += v.x == 1.0f ? 1 : 0;
	}
	CHECK_EQ(dark, 3);  // two corners, one of them in both triangles
	for (const Vertex& v : Mesh(world, false))
	{
		CHECK_EQ(int(v.r), 255);
	}
}

TEST(MesherSplitsQuadsAlongTheDarkerDiagonal)
{
	BlockWorld world;
	world.Set({ 0, 0, 0 }, Id("stone"));
	world.Set({ 1, 1, 1 }, Id("stone"));  // diagonal to the top face's corner (1, 1, 1) only
	const auto vertices = Mesh(world);
	// That corner is corner 1, the only shaded one, so the quad is split along 1-3 and starts there.
	CHECK_EQ(vertices[6].x, 1.0f);
	CHECK_EQ(vertices[6].z, 1.0f);
	CHECK_EQ(int(vertices[6].r), 204);
}
