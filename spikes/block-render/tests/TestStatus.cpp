#include "Harness.h"

#include "Status.h"

TEST(StatusWritesOneLinePerItem)
{
	cpmc::StatusReport report;
	report.mode = "present";
	report.framesDrawn = 58;
	report.framesSkipped = 2;
	report.lastSkip = "no depth tag\nyet";
	report.sections = 1;
	report.blocks = 1;
	report.vertices = 36;
	report.gpuMsAverage = 0.05;
	report.gpuMsWorst = 0.08;
	report.streamlineFrames = 60;
	report.hooks = "device seen at creation";
	report.lastRequest = "place cube: placed at block (10, 7, -24)";
	report.structures.push_back({ "cube", { 10.0, 23.0, 7.0 }, { 11.0, 24.0, 8.0 } });
	CHECK_STR(cpmc::FormatStatus(report),
	          "mode=present\n"
	          "frames=58 drawn, 2 skipped (no depth tag yet), 0 with a late camera (last second)\n"
	          "blocks=1 in 1 sections, 36 vertices\n"
	          "gpu=0.050 ms average, 0.080 ms worst\n"
	          "frame=not measured\n"
	          "streamline=60 frames in the last second\n"
	          "hooks=device seen at creation\n"
	          "request=place cube: placed at block (10, 7, -24)\n"
	          "structure=cube 10.00 23.00 7.00 11.00 24.00 8.00\n");
}

TEST(StatusCountsFramesDrawnWithoutTheTaggedDepth)
{
	cpmc::StatusReport report;
	report.mode = "gbuffer";
	report.framesDrawn = 70;
	report.framesPartial = 3;
	const std::string status = cpmc::FormatStatus(report);
	CHECK(status.find("frames=70 drawn (3 without the tagged depth), 0 skipped, 0 with a late camera (last second)\n") !=
	      std::string::npos);
}
