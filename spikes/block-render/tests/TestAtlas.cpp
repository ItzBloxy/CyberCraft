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

TEST(AtlasFromMinecraftsPixelsHalvesFiveTimes)
{
	// 5 x 3 texels, each with its own red: level 1 is 2 x 1, its last column and row of the level above unused, then
	// 1 x 1 three times, each the one row of the level above used twice.
	std::vector<uint8_t> rgba;
	for (uint8_t i = 0; i < 15; ++i)
	{
		rgba.insert(rgba.end(), { uint8_t(i * 10), 0, 0, 255 });
	}
	std::string error;
	const auto  atlas = cpmc::AtlasFromRgba(5, 3, rgba, error);
	CHECK(atlas.has_value());
	CHECK_EQ(atlas->width, 5u);
	CHECK_EQ(atlas->height, 3u);
	CHECK_EQ(atlas->Levels(), 5u);
	CHECK(atlas->levelOffsets == std::vector<uint32_t>({ 0, 15, 17, 18, 19 }));
	CHECK_EQ(atlas->texels.size(), size_t(20));
	CHECK_EQ(atlas->texels[6], 0xFF00003Cu);   // level 0 is the pixels: red 60 in the low byte
	CHECK_EQ(atlas->texels[15], 0xFF00001Eu);  // (0 + 10 + 50 + 60) / 4
	CHECK_EQ(atlas->texels[16], 0xFF000032u);  // (20 + 30 + 70 + 80) / 4
	CHECK_EQ(atlas->texels[17], 0xFF000028u);  // (30 + 50 + 30 + 50) / 4
	CHECK_EQ(atlas->texels[19], 0xFF000028u);
	CHECK(atlas->sprites.empty());
	CHECK(atlas->blocks.empty());
}

TEST(AtlasFromMinecraftsPixelsAveragesEachChannel)
{
	// One opaque white texel and three transparent black ones: a quarter of each, rounded.
	const std::vector<uint8_t> rgba = { 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	std::string                error;
	const auto                 atlas = cpmc::AtlasFromRgba(2, 2, rgba, error);
	CHECK_EQ(atlas->texels[4], 0x40404040u);
}

TEST(AtlasFromMinecraftsPixelsAtStepOnesSize)
{
	// Step 1's atlas: 2048 x 2576, five levels as the shader finds them.
	const std::vector<uint8_t> rgba(size_t(2048) * 2576 * 4, 0);
	std::string                error;
	const auto                 atlas = cpmc::AtlasFromRgba(2048, 2576, rgba, error);
	CHECK(atlas.has_value());
	CHECK(atlas->levelOffsets == std::vector<uint32_t>({ 0, 5'275'648, 6'594'560, 6'924'288, 7'006'720 }));
	CHECK_EQ(atlas->texels.size(), size_t(7'027'328));
	CHECK(!cpmc::AtlasFromRgba(2048, 2576, std::span(rgba).first(100), error));
	CHECK(error.find("needs 21102592 bytes") != std::string::npos);
	CHECK(!cpmc::AtlasFromRgba(0, 4, rgba, error));
}
