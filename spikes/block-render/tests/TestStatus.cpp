#include "Harness.h"

#include "Status.h"

#include <skycraft_protocol.h>

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

namespace
{
bool Has(const std::string& aStatus, const std::string& aLine)
{
	return aStatus.find(aLine + "\n") != std::string::npos;
}
}

TEST(StatusWritesTheLinksLines)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	link.mapping = "created";
	link.linked = true;
	link.minecraft = true;
	link.minecraftPid = 9052;
	link.heartbeatAgeMs = 16;
	link.inWorld = true;
	link.minecraftFps = 60.0;
	link.inGame = true;
	link.minecraftEye = { -119.5, 7.76, 2007.5 };
	link.camera = cpmc::Vec3d{ -119.5, 7.76, 2007.5 };
	link.teleports = 3;
	link.target = { -119.5, 6.14, 2007.5 };
	link.floorEpoch = 2;
	link.floorSent = true;
	link.floorCentre = { -119.5, 6.14, 2007.5 };
	link.ringMb = 7.52;
	link.ringMbWorstFrame = 0.15;
	link.drainMsAverage = 0.12;
	link.drainMsWorst = 0.9;
	link.ringTypes = "812 atlas patch (7.52 MB), 3 section (0.01 MB)";
	link.atlas = "2048 x 2576, 5 levels, Minecraft's";
	link.updateMsAverage = 0.05;
	link.updateMsWorst = 1.2;
	link.hudSetting = 2;
	link.hudWanted = true;
	link.hudReceived = 60;
	link.hudMb = 210.9375;
	link.hudDrawn = 59;
	link.hudSize = "1280 x 720, rows bottom-up";
	link.hudCopyMsAverage = 0.4;
	link.hudGpuMsAverage = 0.05;
	link.latencyLast = cpmc::LatencySample{ 120.0, 25.0, 2.0 };
	link.latencyAverage = cpmc::LatencySample{ 110.0, 22.0, 1.7 };
	link.latencySamples = 3;
	link.burst = cpmc::BurstReport{ "explosion", 61, 25, 5'000'000, 1.5, 6.0, 30.0 };
	link.cyberpunkCpu = 0.85;
	link.cyberpunkMemory = 6'100ull << 20;
	link.minecraftProcessKnown = true;
	link.minecraftCpu = 0.4;
	link.minecraftMemory = 1'900ull << 20;
	report.link = link;
	report.minecraftBlocks = true;
	report.sections = 81;
	report.vertices = 123'456;
	const std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "blocks=Minecraft's in 81 sections, 123456 vertices"));
	CHECK(Has(status, "link=linked; the mapping created; Minecraft process 9052, heartbeat 16 ms ago, in its world, 60 frames/s; in game"));
	CHECK(Has(status, "positions=Minecraft's eye -119.50 7.76 2007.50; V's camera -119.50 7.76 2007.50 (Minecraft axes); "
	                  "3 teleports in the last second, the last to -119.50 6.14 2007.50; floor 2 at height 6.14 around -119.5 2007.5"));
	CHECK(Has(status, "ring=7.52 MB in the last second, at most 0.15 MB in one frame; drain 0.120 ms average, 0.900 ms worst; "
	                  "812 atlas patch (7.52 MB), 3 section (0.01 MB); 0 rejected, 0 resyncs"));
	CHECK(Has(status, "atlas=2048 x 2576, 5 levels, Minecraft's; 0 translucent triangles dropped; update 0.050 ms average, "
	                  "1.200 ms worst"));
	CHECK(Has(status, "hud=always, scale 2, showing; 60 images received (210.9 MB), 0 dropped and 59 drawn in the last second, "
	                  "1280 x 720, rows bottom-up; copy 0.400 ms average; pass 0.050 ms average on the GPU; 0 skipped; "
	                  "0 refused since the start"));
	CHECK(Has(status, "latency=last: Minecraft 120 ms, ours 25.0 ms (2.0 frames); average of 3: Minecraft 110 ms, ours 22.0 ms "
	                  "(1.7 frames)"));
	CHECK(Has(status, "burst=explosion: 25 sections, 4.77 MB over 61 frames; worst drain 1.500 ms, update 6.000 ms, frame 30.0 ms"));
	CHECK(Has(status, "processes=Cyberpunk 85% of a core, 6100 MB; Minecraft 40% of a core, 1900 MB"));
}

TEST(StatusWritesTheLinkWithMinecraftAbsent)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	link.mapping = "refused: another game side (process 12) is using it";
	link.hudSkipped = 4;
	link.hudLastSkip = "no image yet";
	link.hudRefused = 2;
	link.cyberpunkCpu = 0.5;
	link.cyberpunkMemory = 100ull << 20;
	report.link = link;
	const std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "link=not linked; the mapping refused: another game side (process 12) is using it; Minecraft absent; not in game"));
	CHECK(Has(status, "positions=Minecraft's eye unknown; V's camera unknown; 0 teleports in the last second; floor not sent"));
	CHECK(Has(status, "atlas=none yet; 0 translucent triangles dropped; update not measured"));
	CHECK(Has(status, "hud=in build mode, scale 2, not showing; 0 images received (0.0 MB), 0 dropped and 0 drawn in the last "
	                  "second, no image yet; copy not measured; pass not measured; 4 skipped (no image yet); 2 refused since the "
	                  "start"));
	CHECK(Has(status, "latency=no samples yet: type a command, or click in build mode, while V stands still"));
	CHECK(Has(status, "burst=none yet"));
	CHECK(Has(status, "processes=Cyberpunk 50% of a core, 100 MB; Minecraft not running"));
}

TEST(StatusWritesBuildMode)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	link.inputHooked = true;
	link.buildMode = true;
	link.slot = 3;
	link.inputsRead = 5120;
	link.inputsForwarded = 14;
	link.inputsHidden = 15;
	link.lastInput = "the right button up";
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "build=on; slot 3; 14 inputs forwarded to Minecraft and 15 hidden from the game, the last: "
	                                      "the right button up; 0 dropped with the input ring full; GetRawInputData hooked, 5120 "
	                                      "keyboard and mouse events read"));
	link.inputHooked = false;
	link.buildMode = false;
	link.buildWhy = "Esc left it";
	link.slot.reset();
	link.inputsDropped = 2;
	report.link = link;
	const std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "build=off: Esc left it; slot unknown until a number key; 14 inputs forwarded to Minecraft and 15 hidden from "
	                  "the game, the last: the right button up; 2 dropped with the input ring full; GetRawInputData not hooked (see "
	                  "the log)"));
	CHECK(status.find("link=") < status.find("build=") && status.find("build=") < status.find("positions="));
}

TEST(StatusWritesTheDrive)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	link.drive = "driving";
	link.driveFor = 12.34;
	link.eyeHeight = 1.6;
	link.eyeDistanceLast = 0.012;
	link.eyeDistanceAverage = 0.0104;
	link.eyeDistanceWorst = 0.034;
	link.renderDelayMs = 17.716;
	link.waitedFrames = 2;
	link.skippedFrames = 3;
	link.teleportMsAverage = 0.045;
	link.teleportMsWorst = 0.12;
	link.health = 100.0;
	link.turn = true;
	link.turnRatio = -0.02134;
	link.turnTurns = 12;
	link.minecraftFlags = skycraft::proto::kMcInWorld | skycraft::proto::kMcOnGround | skycraft::proto::kMcSneaking;
	report.link = link;
	std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "drive=driving for 12.3 s; V's eye 1.60 m above its feet; V's camera from Minecraft's eye 0.012 m, "
	                  "0.010 m average and 0.034 m worst in the last second; render delay 17.7 ms, 2 frames waited for a late "
	                  "tick in the last second; 3 bad targets skipped since the start; teleport 0.045 ms average, 0.120 ms "
	                  "worst; V's health 100%; turn on, -0.0213 degrees a count from 12 turns; Minecraft's player on the "
	                  "ground, sneaking"));
	CHECK(status.find("build=") < status.find("drive=") && status.find("drive=") < status.find("positions="));
	// Before anything has been measured
	report.link = cpmc::LinkReport{};
	status = cpmc::FormatStatus(report);
	CHECK(Has(status, "drive=off; V's eye 1.60 m above its feet; render delay 0.0 ms, 0 frames waited for a late tick in the "
	                  "last second; 0 bad targets skipped since the start; teleport not called; V's health unknown; turn off, "
	                  "ratio not measured yet (0 of 5 turns); Minecraft's player in the air"));
}

TEST(StatusWritesTheSceneWatch)
{
	cpmc::StatusReport report;
	CHECK(cpmc::FormatStatus(report).find("scenewatch=") == std::string::npos);  // never on: no line
	report.sceneWatch = "on for 12 s: thread 9120, the main thread: step 720, fetch 720; no shifts";
	CHECK(Has(cpmc::FormatStatus(report), "scenewatch=on for 12 s: thread 9120, the main thread: step 720, fetch 720; no shifts"));
}
