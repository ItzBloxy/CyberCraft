#include "Harness.h"

#include "AtlasFixture.h"

#include <algorithm>

namespace
{
// A aWidth x aHeight image whose every channel differs from texel to texel, from aSeed on.
std::vector<uint8_t> Pixels(uint32_t aWidth, uint32_t aHeight, uint32_t aSeed)
{
	std::vector<uint8_t> rgba(size_t(aWidth) * aHeight * 4);
	for (size_t i = 0; i < rgba.size(); ++i)
	{
		rgba[i] = uint8_t((i * 37 + aSeed * 101) % 251);
	}
	return rgba;
}

// aImage, aWidth wide, with the aPatchWidth x aPatchHeight aPatch written at aX aY.
std::vector<uint8_t> Pasted(std::vector<uint8_t> aImage, uint32_t aWidth, uint32_t aX, uint32_t aY, uint32_t aPatchWidth, uint32_t aPatchHeight,
                            const std::vector<uint8_t>& aPatch)
{
	for (uint32_t row = 0; row < aPatchHeight; ++row)
	{
		std::copy_n(aPatch.begin() + size_t(row) * aPatchWidth * 4, size_t(aPatchWidth) * 4, aImage.begin() + (size_t(aY + row) * aWidth + aX) * 4);
	}
	return aImage;
}

bool SameArea(const cpmc::AtlasArea& aArea, uint32_t aLevel, uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight)
{
	return aArea.level == aLevel && aArea.x == aX && aArea.y == aY && aArea.width == aWidth && aArea.height == aHeight;
}
}

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

TEST(AtlasPatchedAsBuiltFromThePatchedImage)
{
	// 9 x 7, odd sides: levels of 9 x 7, 4 x 3, 2 x 1, 1 x 1 and 1 x 1. A 3 x 2 patch at the top left reaches every level.
	const std::vector<uint8_t> image = Pixels(9, 7, 1);
	const std::vector<uint8_t> patch = Pixels(3, 2, 2);
	std::string                error;
	std::optional<cpmc::Atlas> atlas = cpmc::AtlasFromRgba(9, 7, image, error);
	const auto                 areas = cpmc::PatchAtlas(*atlas, 1, 0, 3, 2, patch);
	const auto                 built = cpmc::AtlasFromRgba(9, 7, Pasted(image, 9, 1, 0, 3, 2, patch), error);
	CHECK(atlas->texels == built->texels);
	CHECK_EQ(areas.size(), size_t(5));
	if (areas.size() == 5)
	{
		CHECK(SameArea(areas[0], 0, 1, 0, 3, 2));
		CHECK(SameArea(areas[1], 1, 0, 0, 2, 1));
		CHECK(SameArea(areas[2], 2, 0, 0, 1, 1));
		CHECK(SameArea(areas[3], 3, 0, 0, 1, 1));
		CHECK(SameArea(areas[4], 4, 0, 0, 1, 1));
	}
}

TEST(AtlasPatchStopsWhereALevelLeavesItOut)
{
	// At 9 x 7's bottom rows: level 1 (4 x 3) takes row 5 into its row 2, but level 2 (2 x 1) leaves level 1's odd last
	// row out, so it and those below are as they were. Then the far corner: a column level 1 leaves out at once.
	const std::vector<uint8_t> image = Pixels(9, 7, 3);
	const std::vector<uint8_t> patch = Pixels(3, 2, 4);
	std::string                error;
	std::optional<cpmc::Atlas> atlas = cpmc::AtlasFromRgba(9, 7, image, error);
	const auto                 areas = cpmc::PatchAtlas(*atlas, 5, 4, 3, 2, patch);
	const auto                 built = cpmc::AtlasFromRgba(9, 7, Pasted(image, 9, 5, 4, 3, 2, patch), error);
	CHECK(atlas->texels == built->texels);
	CHECK_EQ(areas.size(), size_t(2));
	if (areas.size() == 2)
	{
		CHECK(SameArea(areas[0], 0, 5, 4, 3, 2));
		CHECK(SameArea(areas[1], 1, 2, 2, 2, 1));
	}
	const std::vector<uint8_t> corner = Pixels(1, 1, 5);
	const auto                 cornerAreas = cpmc::PatchAtlas(*atlas, 8, 6, 1, 1, corner);
	const auto                 cornerBuilt = cpmc::AtlasFromRgba(9, 7, Pasted(Pasted(image, 9, 5, 4, 3, 2, patch), 9, 8, 6, 1, 1, corner), error);
	CHECK(atlas->texels == cornerBuilt->texels);
	CHECK_EQ(cornerAreas.size(), size_t(1));
}

TEST(AtlasPatchRefusesWhatDoesntFit)
{
	std::string                error;
	std::optional<cpmc::Atlas> atlas = cpmc::AtlasFromRgba(9, 7, Pixels(9, 7, 6), error);
	const std::vector<uint32_t> before = atlas->texels;
	CHECK(cpmc::PatchAtlas(*atlas, 7, 0, 3, 1, Pixels(3, 1, 7)).empty());           // past the right edge
	CHECK(cpmc::PatchAtlas(*atlas, 0, 6, 1, 2, Pixels(1, 2, 7)).empty());           // past the bottom
	CHECK(cpmc::PatchAtlas(*atlas, 0xFFFFFFFF, 0, 2, 1, Pixels(2, 1, 7)).empty());  // x + width wraps in 32 bits
	CHECK(cpmc::PatchAtlas(*atlas, 0, 0, 0, 1, {}).empty());
	CHECK(cpmc::PatchAtlas(*atlas, 0, 0, 2, 2, Pixels(2, 1, 7)).empty());           // short pixels
	CHECK(atlas->texels == before);
	atlas->texels.pop_back();  // levels no longer where AtlasFromRgba puts them
	CHECK(cpmc::PatchAtlas(*atlas, 0, 0, 1, 1, Pixels(1, 1, 7)).empty());
	atlas->levelOffsets[1] += 1;
	atlas->texels.push_back(0);
	CHECK(cpmc::PatchAtlas(*atlas, 0, 0, 1, 1, Pixels(1, 1, 7)).empty());
}

TEST(AtlasDirtyAreasPackedForThePatchPass)
{
	// A sprite patched twice waits once, at its newest; each area comes with where it goes in the atlas buffer, and its
	// texels row by row, one area after another.
	std::string                error;
	std::optional<cpmc::Atlas> atlas = cpmc::AtlasFromRgba(9, 7, Pixels(9, 7, 8), error);
	cpmc::AtlasDirty           dirty;
	const cpmc::AtlasArea      patched = { 0, 5, 4, 3, 2 };
	CHECK(!dirty.Waiting(patched));
	dirty.Add(cpmc::PatchAtlas(*atlas, 5, 4, 3, 2, Pixels(3, 2, 9)));  // levels 0 and 1
	CHECK(dirty.Waiting(patched));
	CHECK(!dirty.Waiting({ 0, 5, 4, 3, 1 }));  // another area
	dirty.Add(cpmc::PatchAtlas(*atlas, 5, 4, 3, 2, Pixels(3, 2, 10)));
	CHECK_EQ(dirty.Size(), size_t(2));
	const std::vector<cpmc::AtlasArea> outside = { { 1, 3, 2, 2, 1 }, { 5, 0, 0, 1, 1 } };  // past level 1's side; no level 5
	dirty.Add(outside);
	const cpmc::AtlasDirty::Packed packed = dirty.Take(*atlas);
	CHECK(dirty.Empty());
	CHECK_EQ(packed.areas.size(), size_t(2));
	CHECK_EQ(packed.texels.size(), size_t(3 * 2 + 2 * 1));
	if (packed.areas.size() == 2 && packed.texels.size() == 8)
	{
		const cpmc::PatchAreaGpu& top = packed.areas[0];  // level 0's, at 5 4: 4 rows of 9 down, 5 along
		CHECK_EQ(top.start, 4u * 9u + 5u);
		CHECK_EQ(top.stride, 9u);
		CHECK_EQ(top.width, 3u);
		CHECK_EQ(top.height, 2u);
		CHECK_EQ(top.source, 0u);
		CHECK_EQ(packed.texels[0], atlas->texels[4 * 9 + 5]);
		CHECK_EQ(packed.texels[3], atlas->texels[5 * 9 + 5]);  // the second row
		const cpmc::PatchAreaGpu& next = packed.areas[1];  // level 1's (4 x 3), at 2 2
		CHECK_EQ(next.start, atlas->levelOffsets[1] + 2u * 4u + 2u);
		CHECK_EQ(next.stride, 4u);
		CHECK_EQ(next.source, 6u);
		CHECK_EQ(packed.texels[7], atlas->texels[next.start + 1]);
	}
	CHECK(!dirty.Waiting(patched));
	CHECK(dirty.Take(*atlas).areas.empty());
	// At most so many areas a pass, as a dispatch has at most so many groups: the rest wait for the next.
	dirty.Add(cpmc::PatchAtlas(*atlas, 5, 4, 3, 2, Pixels(3, 2, 11)));
	CHECK_EQ(dirty.Take(*atlas, 1).areas.size(), size_t(1));
	CHECK_EQ(dirty.Size(), size_t(1));
	CHECK_EQ(dirty.Take(*atlas, 1).areas.size(), size_t(1));
	CHECK(dirty.Empty());
}
