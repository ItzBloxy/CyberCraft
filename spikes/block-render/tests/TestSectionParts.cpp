#include "Harness.h"

#include "render/SectionParts.h"

#include <array>
#include <string>
#include <vector>

using cpmc::SectionPart;
using cpmc::Vertex;

namespace
{
constexpr uint32_t kUp = cpmc::kFlagAlphaTested | cpmc::FaceFlags(cpmc::Face::Up);
constexpr uint32_t kPlant = cpmc::kFlagAlphaTested;                                     // no direction of its own
constexpr uint32_t kWaterUp = cpmc::kFlagTranslucent | cpmc::FaceFlags(cpmc::Face::Up);  // as SkyCraft flags it

// A 32 x 16 atlas of two sprites, as Minecraft's: glass's at x 0 to 16, its texels clear or opaque, and water's at x 16
// to 32, every texel at alpha 180.
const cpmc::Atlas& TwoSprites()
{
	static const cpmc::Atlas atlas = [] {
		std::vector<uint8_t> rgba(32 * 16 * 4, 255);
		for (uint32_t y = 0; y < 16; ++y)
		{
			for (uint32_t x = 0; x < 32; ++x)
			{
				uint8_t& alpha = rgba[(y * 32 + x) * 4 + 3];
				alpha = x >= 16 ? 180 : (x == 0 || y == 0 ? 255 : 0);  // glass: a frame round a clear middle
			}
		}
		std::string error;
		return *cpmc::AtlasFromRgba(32, 16, rgba, error);
	}();
	return atlas;
}

// A triangle of the sprite at aU0 to aU1 across, the whole height, every corner with aFlags; aTag in x, to follow it.
std::array<Vertex, 3> Triangle(uint32_t aFlags, float aU0, float aU1, float aTag = 0.0f)
{
	std::array<Vertex, 3> corners{};
	corners[0].u = aU0;
	corners[0].v = 0.0f;
	corners[1].u = aU1;
	corners[1].v = 0.0f;
	corners[2].u = aU1;
	corners[2].v = 1.0f;
	for (Vertex& corner : corners)
	{
		corner.flags = aFlags;
		corner.x = aTag;
	}
	return corners;
}

std::array<Vertex, 3> Glass(uint32_t aFlags, float aTag = 0.0f)
{
	return Triangle(aFlags, 0.0f, 0.5f, aTag);
}

std::array<Vertex, 3> Water(uint32_t aFlags, float aTag = 0.0f)
{
	return Triangle(aFlags, 0.5f, 1.0f, aTag);
}

void Add(std::vector<Vertex>& aVertices, const std::array<Vertex, 3>& aTriangle)
{
	aVertices.insert(aVertices.end(), aTriangle.begin(), aTriangle.end());
}
}

TEST(SectionPartsSortTrianglesByPartKeepingTheirOrder)
{
	std::vector<Vertex> vertices;
	Add(vertices, Glass(kPlant, 1.0f));
	Add(vertices, Glass(kUp, 2.0f));
	Add(vertices, Water(kWaterUp, 3.0f));
	Add(vertices, Glass(kUp, 4.0f));
	Add(vertices, Glass(kPlant, 5.0f));
	const cpmc::SectionParts parts = cpmc::SplitSection(vertices, &TwoSprites());
	CHECK_EQ(parts.solid, 6u);
	CHECK_EQ(parts.plants, 6u);
	CHECK_EQ(parts.translucent, 3u);
	CHECK_EQ(vertices.size(), size_t(15));
	const float order[5] = { 2.0f, 4.0f, 1.0f, 5.0f, 3.0f };
	for (size_t t = 0; t < 5; ++t)
	{
		CHECK_EQ(vertices[t * 3].x, order[t]);
		CHECK_EQ(vertices[t * 3 + 2].x, order[t]);
	}
	CHECK_EQ(parts.First(SectionPart::Solid), 0u);
	CHECK_EQ(parts.First(SectionPart::Plant), 6u);
	CHECK_EQ(parts.First(SectionPart::Translucent), 12u);
	CHECK_EQ(parts.Count(SectionPart::Translucent), 3u);
}

TEST(SectionPartsTakeTranslucentGlassAsACutOut)
{
	// Minecraft 26.3 forces glass into its translucent layer, but its texels are clear or opaque.
	CHECK(!cpmc::PartlyClear(TwoSprites(), Glass(kWaterUp)));
	CHECK(cpmc::PartlyClear(TwoSprites(), Water(kWaterUp)));
	CHECK(cpmc::PartOf(Glass(kWaterUp), &TwoSprites()) == SectionPart::Solid);
	CHECK(cpmc::PartOf(Water(kWaterUp), &TwoSprites()) == SectionPart::Translucent);
	// A translucent triangle with no direction and only clear or opaque texels is a plant, as any other.
	CHECK(cpmc::PartOf(Glass(cpmc::kFlagTranslucent), &TwoSprites()) == SectionPart::Plant);
	CHECK(cpmc::PartOf(Water(cpmc::kFlagTranslucent), &TwoSprites()) == SectionPart::Translucent);
}

TEST(SectionPartsKeepTranslucentTrianglesWithoutAnAtlas)
{
	CHECK(cpmc::PartOf(Glass(kWaterUp), nullptr) == SectionPart::Translucent);
	CHECK(cpmc::PartOf(Glass(kUp), nullptr) == SectionPart::Solid);
	CHECK(cpmc::PartOf(Glass(kPlant), nullptr) == SectionPart::Plant);
}

TEST(SectionPartsReadOnlyTheSpritesUnderTheUvs)
{
	// UVs a little inside glass's edges, as Minecraft's are, still read glass alone, not water beside it.
	const float inside = 0.01f / 32.0f;
	CHECK(!cpmc::PartlyClear(TwoSprites(), Triangle(kWaterUp, inside, 0.5f - inside)));
	// Straddling both sprites reads water's texels too.
	CHECK(cpmc::PartlyClear(TwoSprites(), Triangle(kWaterUp, 0.25f, 0.75f)));
	// A box thinner than a texel reads the texel it's in.
	CHECK(cpmc::PartlyClear(TwoSprites(), Triangle(kWaterUp, 0.7f, 0.7f)));
	CHECK(!cpmc::PartlyClear(TwoSprites(), Triangle(kWaterUp, 0.2f, 0.2f)));
	// UVs past the atlas's edge are held to it.
	CHECK(cpmc::PartlyClear(TwoSprites(), Triangle(kWaterUp, 0.9f, 1.5f)));
}

TEST(SectionPartsDropVerticesAfterTheLastWholeTriangle)
{
	std::vector<Vertex> vertices;
	Add(vertices, Glass(kUp));
	vertices.push_back(Vertex{});
	vertices.push_back(Vertex{});
	const cpmc::SectionParts parts = cpmc::SplitSection(vertices, &TwoSprites());
	CHECK_EQ(parts.solid, 3u);
	CHECK_EQ(vertices.size(), size_t(3));
}
