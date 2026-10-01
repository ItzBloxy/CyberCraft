#include "Harness.h"

#include "log/FrameLogData.h"

#include <dxgiformat.h>

using cpmc::FrameLogData;

namespace
{
bool Has(const std::string& aText, const std::string& aPart)
{
	const bool found = aText.find(aPart) != std::string::npos;
	if (!found)
	{
		std::printf("  missing: %s\n", aPart.c_str());
	}
	return found;
}

FrameLogData SampleLog()
{
	FrameLogData log;
	log.pluginVersion = "0.1.0";
	log.time = "2026-10-01T18:00:00";
	log.firstFrame = 100;
	log.lastFrame = 101;
	log.device.seenAtCreation = true;
	log.device.device = 0xABC;
	log.device.gpuApiDevice = "proxy";
	log.device.gpuApiMatches = true;
	log.device.streamline = "proxies";
	log.device.rtvs = 12;
	log.scriptCamera = cpmc::LogScriptCamera{ { 1.0, 2.0, 3.0 }, { 0.0f, 1.0f, 0.0f }, 68.25f };

	cpmc::LogConstants constants;
	constants.frame = 100;
	constants.constants.depthInverted = 1;
	constants.constants.cameraMotionIncluded = 0;
	constants.constants.fov = 1.0f;
	log.constants.push_back(constants);

	cpmc::LogTag depth;
	depth.frame = 100;
	depth.tag.type = 0;
	depth.tag.resource = reinterpret_cast<void*>(0x5000);
	depth.tag.state = 0x80;
	depth.tag.lifecycle = 1;
	depth.tag.extent = { 0, 0, 1707, 960 };
	depth.resource = { true, 3, DXGI_FORMAT_R32G8X24_TYPELESS, 2560, 1440, 1, 1, 0x2 };
	log.tags.push_back(depth);

	log.evaluates.push_back({ 100, 0, 0, 0x7000, 0x7100 });
	log.events.push_back({ 1.23456, 100, 42, "evaluate", "list 0x7000" });
	log.submissions.push_back({ 2.0, 100, 0x9000, { 0x7100, 0x7200 }, 1 });

	cpmc::PassRecord gbuffer;
	for (uintptr_t i = 1; i <= 3; ++i)
	{
		cpmc::ViewInfo view;
		view.resource = reinterpret_cast<void*>(i);
		view.viewFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
		view.width = 2560;
		view.height = 1440;
		gbuffer.targets.rtvs.push_back({ i * 16, view });
	}
	cpmc::ViewInfo dsv;
	dsv.kind = cpmc::ViewInfo::Kind::Dsv;
	dsv.resource = reinterpret_cast<void*>(0x5000);
	dsv.viewFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	gbuffer.targets.dsv = cpmc::BoundTarget{ 0x900, dsv };
	gbuffer.indexedDraws = 812;
	gbuffer.events.push_back("GBuffer");
	cpmc::PassRecord unknown;
	unknown.targets.rtvs.push_back({ 0x123, std::nullopt });
	log.lists.push_back({ 0x7100, "Scene", 100, { gbuffer, unknown },
	                      { { 0x1, 0x0, 0, cpmc::ListState::kBeginOnly, std::nullopt }, { 0x5000, 0x10, 0xFFFFFFFF, 0, 0x80 } } });
	log.gbufferCandidates.push_back({ 0x7100, 0 });
	return log;
}
}

TEST(FrameLogNamesFormats)
{
	CHECK_STR(cpmc::DxgiFormatName(DXGI_FORMAT_R10G10B10A2_UNORM), "R10G10B10A2_UNORM");
	CHECK_STR(cpmc::DxgiFormatName(DXGI_FORMAT_D32_FLOAT_S8X24_UINT), "D32_FLOAT_S8X24_UINT");
	CHECK_STR(cpmc::DxgiFormatName(DXGI_FORMAT_B4G4R4A4_UNORM), "B4G4R4A4_UNORM");
	CHECK_STR(cpmc::DxgiFormatName(500), "format 500");
}

TEST(FrameLogWritesEverySection)
{
	const std::string json = cpmc::FrameLogJson(SampleLog());
	CHECK(Has(json, "\"pluginVersion\": \"0.1.0\""));
	CHECK(Has(json, "\"frames\": [\n    100,\n    101\n  ]"));
	CHECK(Has(json, "\"seenAtCreation\": true"));
	CHECK(Has(json, "\"device\": \"proxy\""));
	CHECK(Has(json, "\"streamline\": \"proxies\""));
	CHECK(Has(json, "\"rtvs\": 12"));
	CHECK(Has(json, "\"fov\": 68.25"));
	CHECK(Has(json, "\"depthInverted\": \"true\""));
	CHECK(Has(json, "\"cameraMotionIncluded\": \"false\""));
	CHECK(Has(json, "\"motionVectors3D\": \"unset\""));
	CHECK(Has(json, "\"fovDegrees\": 57.29577951308232"));
	CHECK(Has(json, "\"name\": \"depth\""));
	CHECK(Has(json, "\"format\": \"R32G8X24_TYPELESS\""));
	CHECK(Has(json, "\"state\": \"0x80\""));
	CHECK(Has(json, "\"lifecycle\": \"validUntilPresent\""));
	CHECK(Has(json, "\"nativeList\": \"0x7100\""));
	CHECK(Has(json, "\"ms\": 1.235"));
	CHECK(Has(json, "\"what\": \"evaluate\""));
	CHECK(Has(json, "\"untracked\": 1"));
	CHECK(Has(json, "\"name\": \"Scene\""));
	CHECK(Has(json, "\"format\": \"R10G10B10A2_UNORM\""));
	CHECK(Has(json, "\"depthIsTagged\": true"));
	CHECK(Has(json, "\"indexedDraws\": 812"));
	CHECK(Has(json, "\"unknown\": true"));
	CHECK(Has(json, "\"resource\": \"0x1\",\n          \"firstBefore\": \"0x0\",\n          \"firstSubresource\": 0,\n"
	                "          \"firstFlags\": 1,\n          \"after\": null"));
	CHECK(Has(json, "\"firstSubresource\": \"all\""));
	CHECK(Has(json, "\"after\": \"0x80\""));
	CHECK(Has(json, "\"gbufferCandidates\": [\n    {\n      \"list\": \"0x7100\",\n      \"pass\": 0\n    }\n  ]"));
	CHECK_EQ(json.back(), '\n');
}

TEST(FrameLogTakesEachListsTransitionsByAddress)
{
	cpmc::ListState state;
	state.Begin(1, true);
	state.Transition(reinterpret_cast<void*>(0x20), 0x4, 0x8, cpmc::ListState::kAllSubresources, 0);
	state.Transition(reinterpret_cast<void*>(0x10), 0x10, 0xC0, 0, cpmc::ListState::kBeginOnly);
	const std::vector<cpmc::LogTransition> transitions = cpmc::TransitionsOf(state);
	CHECK_EQ(transitions.size(), size_t(2));
	CHECK_EQ(transitions[0].resource, uint64_t(0x10));
	CHECK_EQ(transitions[0].firstFlags, cpmc::ListState::kBeginOnly);
	CHECK(!transitions[0].after);  // begun, not ended
	CHECK_EQ(transitions[1].firstBefore, 0x4u);
	CHECK_EQ(transitions[1].after.value_or(0), 0x8u);
}

TEST(FrameLogWritesNullWithoutAScriptCamera)
{
	FrameLogData log = SampleLog();
	log.scriptCamera.reset();
	CHECK(Has(cpmc::FrameLogJson(log), "\"scriptCamera\": null"));
}
