#include "Harness.h"

#include "AtlasFixture.h"

TEST(AtlasReadsWhatBuildAtlasWrites)
{
	std::string error;
	const auto  atlas = cpmc::ParseAtlas(fixture::AtlasBytes(), error);
	CHECK(atlas.has_value());
	CHECK_STR(error, "");
	CHECK_EQ(atlas->width, 128u);
	CHECK_EQ(atlas->height, 16u);
	CHECK_EQ(atlas->Levels(), 5u);
	CHECK_EQ(atlas->levelOffsets[1], 128u * 16u);
	CHECK_EQ(atlas->levelOffsets[4], 2048u + 512u + 128u + 32u);
	CHECK_EQ(atlas->texels.size(), size_t(2048 + 512 + 128 + 32 + 8));
	CHECK_EQ(atlas->texels[5], 0xFF000005u);
	CHECK_EQ(atlas->sprites[3].x, 48u);
	CHECK_EQ(atlas->blocks.size(), size_t(5));
	const cpmc::AtlasBlock& grass = atlas->blocks[1];
	CHECK_STR(grass.name, "grass_block");
	CHECK_EQ(grass.faces[1], uint16_t(2));
	CHECK_EQ(grass.tint[0], uint8_t(0x91));
	CHECK_EQ(grass.tintedFaces, uint8_t(0b000010));
	CHECK(atlas->blocks[2].cutout);
	CHECK(!atlas->blocks[0].cutout);
}

TEST(AtlasNumbersBlocksFromOne)
{
	const cpmc::Atlas atlas = fixture::Atlas();
	CHECK_EQ(*atlas.BlockId("stone"), uint16_t(1));
	CHECK_EQ(*atlas.BlockId("oak_log"), uint16_t(5));
	CHECK(!atlas.BlockId("bedrock"));
	CHECK(atlas.Block(0) == nullptr);  // air
	CHECK_STR(atlas.Block(3)->name, "glass");
	CHECK(atlas.Block(6) == nullptr);
}

TEST(AtlasRefusesBrokenFiles)
{
	std::string error;
	std::vector<uint8_t> bytes = fixture::AtlasBytes();
	bytes.pop_back();
	CHECK(!cpmc::ParseAtlas(bytes, error));
	CHECK(error.find("its header says") != std::string::npos);

	std::vector<uint8_t> wrongMagic = fixture::AtlasBytes();
	wrongMagic[0] = 'X';
	CHECK(!cpmc::ParseAtlas(wrongMagic, error));

	std::vector<uint8_t> badFace = fixture::AtlasBytes();
	badFace[32 + 8 * 16 + 32] = 9;  // stone's down face: sprite 9 of 8
	CHECK(!cpmc::ParseAtlas(badFace, error));
	CHECK(error.find("past the 8 sprites") != std::string::npos);

	CHECK(!cpmc::ParseAtlas({}, error));
}
