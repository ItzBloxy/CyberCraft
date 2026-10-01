#include "Harness.h"

#include "sl/Sl.h"

#include <cstddef>

using cpmc::ReadableMemory;

// Streamline 2.7.2's layout, as the plugin reads it from the game's 2.7.1 calls. A wrong offset fails the build.
static_assert(sizeof(sl::BaseStructure) == 32);
static_assert(offsetof(sl::BaseStructure, structType) == 8);
static_assert(offsetof(sl::BaseStructure, structVersion) == 24);
static_assert(offsetof(sl::Constants, cameraViewToClip) == 32);
static_assert(offsetof(sl::Constants, clipToCameraView) == 96);
static_assert(offsetof(sl::Constants, clipToPrevClip) == 224);
static_assert(offsetof(sl::Constants, prevClipToClip) == 288);
static_assert(offsetof(sl::Constants, jitterOffset) == 352);
static_assert(offsetof(sl::Constants, mvecScale) == 360);
static_assert(offsetof(sl::Constants, cameraPinholeOffset) == 368);
static_assert(offsetof(sl::Constants, cameraPos) == 376);
static_assert(offsetof(sl::Constants, cameraUp) == 388);
static_assert(offsetof(sl::Constants, cameraRight) == 400);
static_assert(offsetof(sl::Constants, cameraFwd) == 412);
static_assert(offsetof(sl::Constants, cameraNear) == 424);
static_assert(offsetof(sl::Constants, cameraFar) == 428);
static_assert(offsetof(sl::Constants, cameraFOV) == 432);
static_assert(offsetof(sl::Constants, cameraAspectRatio) == 436);
static_assert(offsetof(sl::Constants, motionVectorsInvalidValue) == 440);
static_assert(offsetof(sl::Constants, depthInverted) == 444);
static_assert(offsetof(sl::Constants, motionVectorsJittered) == 450);
static_assert(offsetof(sl::Resource, native) == 40);
static_assert(offsetof(sl::Resource, state) == 64);
static_assert(offsetof(sl::ResourceTag, resource) == 32);
static_assert(offsetof(sl::ResourceTag, type) == 40);
static_assert(offsetof(sl::ResourceTag, lifecycle) == 44);
static_assert(offsetof(sl::ResourceTag, extent) == 48);
static_assert(sizeof(sl::ViewportHandle) == 40);

TEST(SlReadsConstants)
{
	sl::Constants constants;
	constants.cameraViewToClip.row[3] = { 0.0f, 0.0f, 0.1f, 0.0f };
	constants.jitterOffset = { 0.25f, -0.5f };
	constants.cameraPos = { 1.0f, 2.0f, 3.0f };
	constants.cameraFwd = { 0.0f, 1.0f, 0.0f };
	constants.cameraNear = 0.1f;
	constants.depthInverted = sl::Boolean::eTrue;
	ReadableMemory memory;
	const auto     read = cpmc::ReadConstants(memory, &constants);
	CHECK(read.has_value());
	CHECK_EQ(read->structVersion, uint64_t(2));
	CHECK_EQ(read->viewToClip[14], 0.1f);
	CHECK_EQ(read->jitter[1], -0.5f);
	CHECK_EQ(read->position[2], 3.0f);
	CHECK_EQ(read->forward[1], 1.0f);
	CHECK_EQ(read->nearPlane, 0.1f);
	CHECK_EQ(int(read->depthInverted), 1);
	CHECK_EQ(int(read->cameraMotionIncluded), 2);  // not set
}

TEST(SlRefusesOtherStructsAndBadPointers)
{
	sl::ViewportHandle viewport(3u);
	ReadableMemory     memory;
	CHECK(!cpmc::ReadConstants(memory, &viewport));
	CHECK(!cpmc::ReadConstants(memory, nullptr));
	CHECK(!cpmc::ReadTag(memory, &viewport, false));
	sl::Constants old;
	old.structVersion = 0;
	CHECK(!cpmc::ReadConstants(memory, &old));
}

TEST(SlReadsTagsAndViewports)
{
	int             fakeTexture = 0;
	sl::Resource    resource(sl::ResourceType::eTex2d, &fakeTexture, 0x80u);
	const sl::Extent extent{ 0, 0, 1707, 960 };
	sl::ResourceTag tag(&resource, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, &extent);
	ReadableMemory  memory;
	const auto      read = cpmc::ReadTag(memory, &tag, true);
	CHECK(read.has_value());
	CHECK_EQ(read->type, sl::kBufferTypeDepth);
	CHECK(read->resource == &fakeTexture);
	CHECK_EQ(read->state, 0x80u);
	CHECK_EQ(read->lifecycle, uint32_t(sl::ResourceLifecycle::eValidUntilPresent));
	CHECK_EQ(read->extent.width, 1707u);
	CHECK(read->local);

	sl::ResourceTag dropped(nullptr, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent);
	const auto      none = cpmc::ReadTag(memory, &dropped, false);
	CHECK(none.has_value());
	CHECK(none->resource == nullptr);

	sl::ViewportHandle viewport(7u);
	CHECK_EQ(*cpmc::ReadViewport(memory, &viewport), 7u);
	CHECK(!cpmc::ReadViewport(memory, &tag));
}

TEST(SlWalksEvaluateInputsAndTheirChains)
{
	sl::ViewportHandle viewport(0u);
	int                fake = 0;
	sl::Resource       resource(sl::ResourceType::eTex2d, &fake, 0u);
	sl::ResourceTag    depth(&resource, sl::kBufferTypeDepth, sl::ResourceLifecycle::eOnlyValidNow);
	sl::ResourceTag    mvec(&resource, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eOnlyValidNow);
	depth.next = &mvec;
	const sl::BaseStructure* inputs[] = { &viewport, &depth };
	ReadableMemory           memory;
	const auto               structs = cpmc::EvaluateInputs(memory, inputs, 2);
	CHECK_EQ(structs.size(), size_t(3));
	CHECK(structs[0] == &viewport);
	CHECK(structs[1] == &depth);
	CHECK(structs[2] == &mvec);
	CHECK(cpmc::EvaluateInputs(memory, nullptr, 2).empty());
}

TEST(SlLocalTagsReplaceGlobalOnesOfTheirType)
{
	cpmc::SlTag globalDepth{ sl::kBufferTypeDepth, reinterpret_cast<void*>(1) };
	cpmc::SlTag globalColor{ sl::kBufferTypeScalingInputColor, reinterpret_cast<void*>(2) };
	cpmc::SlTag localDepth{ sl::kBufferTypeDepth, reinterpret_cast<void*>(3) };
	localDepth.local = true;
	const auto tags = cpmc::TagsForEvaluate({ globalDepth, globalColor }, { localDepth });
	CHECK_EQ(tags.size(), size_t(2));
	CHECK(tags[0].resource == reinterpret_cast<void*>(2));
	CHECK(tags[1].resource == reinterpret_cast<void*>(3));
}

TEST(SlNamesBufferTypes)
{
	CHECK_STR(cpmc::BufferTypeName(sl::kBufferTypeDepth), "depth");
	CHECK_STR(cpmc::BufferTypeName(sl::kBufferTypeScalingInputColor), "scalingInputColor");
	CHECK_STR(cpmc::BufferTypeName(sl::kBufferTypeExposure), "exposure");
	CHECK_STR(cpmc::BufferTypeName(sl::kBufferTypeUIColorAndAlpha), "uiColorAndAlpha");
	CHECK_STR(cpmc::BufferTypeName(sl::kBufferTypeColorAfterDepthOfField), "colorAfterDepthOfField");
	CHECK_STR(cpmc::BufferTypeName(500), "type 500");
}

TEST(SlNewestFrameStartsEachFrameOnce)
{
	// As the game's main menu logged it: three threads ask for tokens, the main thread a frame ahead of the others.
	cpmc::NewestFrame frames;
	CHECK(frames.Note(8877));
	CHECK(!frames.Note(8877));
	CHECK(frames.Note(8878));
	CHECK(!frames.Note(8877));
	CHECK(!frames.Note(8878));
	CHECK(frames.Note(8880));
	// A far older index: the count started again.
	CHECK(frames.Note(12));
	CHECK(!frames.Note(12));
	CHECK(!frames.Note(11));
	CHECK(frames.Note(13));
}
