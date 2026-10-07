#include "Harness.h"

#include "render/EntityTextures.h"

using cpmc::EntityTextures;

TEST(EntityTexturesGoOneAfterAnother)
{
	EntityTextures textures;
	const auto     pig = textures.Add(4, 64, 64);
	const auto     spider = textures.Add(9, 64, 32);
	CHECK(pig.has_value() && spider.has_value());
	CHECK_EQ(pig ? pig->start : 1u, 0u);
	CHECK_EQ(spider ? spider->start : 0u, 4096u);  // right after the pig's 64 x 64
	const EntityTextures::Place* found = textures.Find(9);
	CHECK(found != nullptr);
	if (found)
	{
		CHECK_EQ(found->start, 4096u);
		CHECK_EQ(found->width, 64u);
		CHECK_EQ(found->height, 32u);
	}
	CHECK(textures.Find(5) == nullptr);
	CHECK_EQ(textures.Used(), 6144ull);
	CHECK_EQ(textures.Count(), size_t(2));
}

TEST(EntityTexturesStopAtTheCap)
{
	EntityTextures textures;
	CHECK(textures.Add(1, 4096, 4096).has_value());  // 64 MB: the cap exactly
	CHECK(!textures.Add(2, 1, 1).has_value());
	CHECK(textures.Find(2) == nullptr);
	CHECK_EQ(textures.Used(), EntityTextures::kCapTexels);
	CHECK(!textures.Add(3, 0, 4).has_value());  // nothing to place
}

TEST(EntityTexturesBufferDoublesFrom4Mb)
{
	EntityTextures textures;
	CHECK_EQ(textures.Capacity(0), 0ull);  // nothing placed: no buffer yet
	textures.Add(1, 64, 64);
	CHECK_EQ(textures.Capacity(0), EntityTextures::kFirstTexels);
	CHECK_EQ(textures.Capacity(EntityTextures::kFirstTexels), EntityTextures::kFirstTexels);
	textures.Add(2, 1024, 1024);  // past 4 MB now
	CHECK_EQ(textures.Capacity(EntityTextures::kFirstTexels), 2 * EntityTextures::kFirstTexels);
	textures.Add(3, 4096, 3000);
	CHECK_EQ(textures.Capacity(2 * EntityTextures::kFirstTexels), EntityTextures::kCapTexels);  // doubled to the cap
}

TEST(EntityTexturesClearStartsAgain)
{
	EntityTextures textures;
	textures.Add(1, 64, 64);
	textures.Clear();
	CHECK_EQ(textures.Used(), 0ull);
	CHECK_EQ(textures.Count(), size_t(0));
	CHECK(textures.Find(1) == nullptr);
	const auto again = textures.Add(1, 32, 32);  // SkyCraft's ids start at 1 again after its clear
	CHECK_EQ(again ? again->start : 1u, 0u);
}

TEST(EntityTexturesPlaceAnIdAgainAfterTheLast)
{
	EntityTextures textures;
	textures.Add(1, 2, 2);
	const auto again = textures.Add(1, 4, 4);
	CHECK_EQ(again ? again->start : 0u, 4u);
	CHECK_EQ(textures.Find(1) ? textures.Find(1)->width : 0u, 4u);
	CHECK_EQ(textures.Used(), 20ull);
	CHECK_EQ(textures.Count(), size_t(1));
}
