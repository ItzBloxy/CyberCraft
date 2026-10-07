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
	link.buildPaused = true;
	report.link = link;
	CHECK(cpmc::FormatStatus(report).find("\nbuild=on, paused while CET's overlay is open; slot 3;") != std::string::npos);
	link.buildPaused = false;
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
	// The last session's ratio, until this one has measured its own
	link.turnRatio = -0.05;
	link.turnRemembered = true;
	link.turnTurns = 2;
	report.link = link;
	CHECK(cpmc::FormatStatus(report).find("; turn on, -0.0500 degrees a count, remembered from the last session (2 of 5 turns "
	                                      "measured); ") != std::string::npos);
	// Before anything has been measured
	report.link = cpmc::LinkReport{};
	status = cpmc::FormatStatus(report);
	CHECK(Has(status, "drive=off; V's eye 1.60 m above its feet; render delay 0.0 ms, 0 frames waited for a late tick in the "
	                  "last second; 0 bad targets skipped since the start; teleport not called; V's health unknown; turn off, "
	                  "ratio not measured yet (0 of 5 turns); Minecraft's player in the air"));
}

TEST(StatusWritesMinecraftsSolids)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	link.solidSections = 625;
	link.solidBlocks = 640000;
	report.link = link;
	const std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "solids=Minecraft's solid blocks: 640000 in 625 sections"));
	CHECK(status.find("ground=") < status.find("solids=") && status.find("solids=") < status.find("ring="));
}

TEST(StatusWritesTheBodies)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	report.link = link;
	std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "bodies=off: 0 sections with bodies, 0 boxes; 0 made, 0 removed, 0 failed and 0 lost since the start, the slowest "
	                  "making 0.00 ms; made of what V stands on, once copied; 0 origin changes, 0 bodies posed again; the test box off"));
	CHECK(status.find("solids=") < status.find("bodies=") && status.find("bodies=") < status.find("ring="));
	link.bodiesOn = true;
	link.bodiesWhy = "they wait for V to stand on the city, to be made of what it's made of";
	report.link = link;
	CHECK(cpmc::FormatStatus(report).find("bodies=on: 0 sections with bodies, ") != std::string::npos);
	CHECK(cpmc::FormatStatus(report).find("; the test box off; waiting: they wait for V to stand on the city, to be made of what it's "
	                                      "made of\n") != std::string::npos);
	link.bodiesWhy.clear();
	link.bodySections = 3;
	link.bodyBoxes = 7;
	link.bodiesMade = 5;
	link.bodiesRemoved = 2;
	link.bodiesFailed = 1;
	link.bodiesLost = 0;
	link.bodySlowestMs = 0.416;
	link.bodiesMadeOf = "a static triangleMesh's: query filter 0:0:449c004:0, simulation filter 5c004:0:13ce:0, flags 0x7";
	link.originChanges = 1;
	link.bodiesReposed = 0;
	link.box = cpmc::Vec3d{ -116.5, -2002.25, 6.64 };
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report),
	          "bodies=on: 3 sections with bodies, 7 boxes; 5 made, 2 removed, 1 failed and 0 lost since the start, the slowest "
	          "making 0.42 ms; made of a static triangleMesh's: query filter 0:0:449c004:0, simulation filter 5c004:0:13ce:0, flags "
	          "0x7; 1 origin change, 0 bodies posed again; the test box at world -116.50 -2002.25 6.64"));
}

TEST(StatusWritesTheSceneWatch)
{
	cpmc::StatusReport report;
	CHECK(cpmc::FormatStatus(report).find("scenewatch=") == std::string::npos);  // never on: no line
	report.sceneWatch = "on for 12 s: thread 9120, the main thread: step 720, fetch 720; no shifts";
	CHECK(Has(cpmc::FormatStatus(report), "scenewatch=on for 12 s: thread 9120, the main thread: step 720, fetch 720; no shifts"));
}

TEST(StatusWritesTheGround)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	link.groundWhy = "PhysX3_x64.dll isn't the audited build";
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "ground=floor; it fell back from the city: PhysX3_x64.dll isn't the audited build"));
	link.ground = "city";
	link.collisionEpoch = 2;
	link.cityCentre = cpmc::link::RegionPos{ -15, 0, 250 };
	link.cityKnown = 1152;
	link.cityReadMsAverage = 0.4;
	link.cityReadMsWorst = 1.25;
	link.cityBatches = 2.0;
	link.cityTriangles = 1300.0;
	link.workerBlocks = 450.0;
	link.workerMsPerRegion = 0.68;
	link.workerTrianglesPerRegion = 120.4;
	link.workerRunning = 0.93;
	link.workerThreads = 4;
	link.workerReplaced = 12;
	link.collisionMb = 0.12;
	link.lastHold = 0.07;
	link.physxOrigin = cpmc::Vec3d{ 0.0, 1024.0, 0.0 };
	link.controllerSearches = 2;
	link.shapesKept = 1500;
	link.shapesLeft = 30;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report),
	          "ground=city, epoch 2, around region -15 0 250: 1152 regions known, 0 batches to read; read 0.40 ms a frame on average, 1.25 ms "
	          "at worst; 2.0 batches, 1300 triangles and 450 voxel blocks a second; the worker 0.68 ms a region of 120 triangles, its "
	          "threads running 93% of that time; 4 threads, 0 regions waiting, 12 replaced by newer ones since the start; the ring 0.12 "
	          "MB a second, 0 messages waiting for room, 0 too big; Minecraft held its player 0.07 s when build mode last "
	          "started; PhysX's origin (0, 1024, 0), V's controller looked for 2 times; shapes since the start: 1500 kept, 30 left out, "
	          "0 unreadable"));
}

TEST(StatusWritesTheNpcs)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	report.link = link;
	const std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "npcs=on: 0 listed of 0 in the world; capsules found for 0 of 0 looked for; 0 numbers given; the module not "
	                  "called; 0 hits from Minecraft logged"));
	CHECK(status.find("bodies=") < status.find("npcs=") && status.find("npcs=") < status.find("ring="));
	link.npcsWhy = "the game has no BlockRenderNpcs, so redscript didn't compile BlockRender.reds";
	report.link = link;
	CHECK(cpmc::FormatStatus(report).find("; 0 hits from Minecraft logged; none listed: the game has no BlockRenderNpcs, so redscript "
	                                      "didn't compile BlockRender.reds") != std::string::npos);
	link.npcsWhy.clear();
	link.npcsListed = 12;
	link.npcsInWorld = 40;
	link.nearestNpc = 3.26;
	link.nearestNpcName = "Ganger One";
	link.capsulesFound = 10;
	link.capsulesLookedFor = 12;
	link.npcIdsGiven = 57;
	link.moduleMsAverage = 0.1234;
	link.moduleMsWorst = 0.4567;
	link.npcHits = 3;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "npcs=on: 12 listed of 40 in the world, the nearest Ganger One at 3.3 m; capsules found for 10 "
	                                      "of 12 looked for; 57 numbers given; the module 0.123 ms average, 0.457 ms worst a frame; 3 "
	                                      "hits from Minecraft logged"));
	link.npcsOn = false;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "npcs=off"));
}

TEST(StatusWritesCombat)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	report.link = link;
	const std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "combat=off: Minecraft's hits are logged, not applied; V's health is V's"));
	CHECK(status.find("npcs=") < status.find("combat=") && status.find("combat=") < status.find("ring="));
	link.combatOn = true;
	link.punch = 102.0;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "combat=on: V's punch 102.00, not drawn: hits skip the NPC's armour, weapon crits, stealth, "
	                                      "blocks and skill XP; hit power 0.50; hits 0 queued, 0 landed; V's hurts: 0 taken, 0 sent, 0 "
	                                      "summed, 0 left to V, 0 non-lethal left to V; Immortal off; 0 deaths; Minecraft's player alive"));
	link.hitPower = 1.0;
	link.punch = 105.0;
	link.punchItem = "Items.mantis_blade";
	link.punchDrawn = true;
	link.hitsQueued = 5;
	link.hitsLanded = 3;
	link.hitsDropped = "1 dodged, 1 not listed";
	link.lastHit = "NPC 35 (NC Resident): 7.00 of Minecraft's damage, started at 735.00, 612.40 dealt (critical, armour); 1514 -> 902";
	link.hurtsTaken = 3;
	link.hurtsSent = 2;
	link.hurtsSummed = 1;
	link.hurtsLeft = 7;
	link.hurtsNonLethal = 2;
	link.lastHurt = "45.0 of V's health (0.0987), 1.97 of Minecraft's 20, projectile (Ranged), from NPC 12";
	link.immortal = true;
	link.deaths = 1;
	link.minecraftDeadFor = 0.84;
	link.combatWhy = "V's max health isn't read, so V's hurts aren't taken";
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "combat=on: V's punch 105.00 from Items.mantis_blade, drawn; hit power 1.00; hits 5 queued, 3 "
	                                      "landed, dropped: 1 dodged, 1 not listed; the last NPC 35 (NC Resident): 7.00 of Minecraft's "
	                                      "damage, started at "
	                                      "735.00, 612.40 dealt (critical, armour); 1514 -> 902; V's hurts: 3 taken, 2 sent, 1 summed, 7 "
	                                      "left to V, 2 non-lethal left to V; the last 45.0 of V's health (0.0987), 1.97 of Minecraft's "
	                                      "20, projectile (Ranged), from NPC 12; Immortal on; 1 deaths; Minecraft's player dead for 0.8 "
	                                      "s; V's max health isn't read, so V's hurts aren't taken"));
}

TEST(StatusWritesWhetherVIsHidden)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	report.link = link;
	std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "v=shown, and hidden in build mode"));
	CHECK(status.find("drive=") < status.find("v=") && status.find("v=") < status.find("positions="));
	link.vHidden = true;
	link.vMeshes = 23;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "v=hidden: 23 meshes switched off, 0 effects stopped"));
	link.vEffects = 3;
	link.vCrosshair = 1;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "v=hidden: 23 meshes switched off, 3 effects stopped, the crosshair hidden"));
	link.vCrosshair = -1;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "v=hidden: 23 meshes switched off, 3 effects stopped, the crosshair not found"));
	link.vMeshes = -1;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "v=shown: hiding V's meshes needs Codeware, which isn't installed"));
	link.vMeshes = -2;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "v=shown: V isn't found"));
	link.hideV = false;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "v=shown: set hidev is 0"));
}

TEST(StatusWritesBlasts)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	report.link = link;
	std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "blasts=off: Minecraft's blasts are logged, not applied"));
	CHECK(status.find("combat=") < status.find("blasts=") && status.find("blasts=") < status.find("ring="));
	link.combatOn = true;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "blasts=on: 0 taken, blast power 0.50; none yet"));
	link.blasts = 2;
	link.blastPower = 1.0;
	link.lastBlast = "at world (680.50, -1997.50, 6.20), reaching 8.0 m: 5 NPCs within it, 4 hits of its own; after its push 2 "
	                 "knocked down, 2 dead";
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "blasts=on: 2 taken, blast power 1.00; the last at world (680.50, -1997.50, 6.20), "
	                                      "reaching 8.0 m: 5 NPCs within it, 4 hits of its own; after its push 2 knocked down, 2 dead"));
	// The forced knockdowns are on unless set blastknock 0 (step 5b's fix): the line says only when they're off.
	link.blastKnock = false;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "blasts=on: 2 taken, blast power 1.00, the forced knockdowns off; the last at world "
	                                      "(680.50, -1997.50, 6.20), reaching 8.0 m: 5 NPCs within it, 4 hits of its own; after its "
	                                      "push 2 knocked down, 2 dead"));
	// V's health share for Minecraft's player goes in the combat line, and the exporter's bush rule in the city's ground line.
	link.lastHealth = "set to V's share 0.7500 when build mode last started";
	link.ground = "city";
	link.bushRule = "sight-only shapes";
	link.shapesBush = 12;
	report.link = link;
	status = cpmc::FormatStatus(report);
	CHECK(Has(status, "; Minecraft's health set to V's share 0.7500 when build mode last started; Immortal off; 0 deaths; Minecraft's "
	                  "player alive"));
	CHECK(Has(status, "0 unreadable; the bush rule leaves out sight-only shapes: 12 so far"));
}

TEST(StatusWritesTheEntities)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	report.link = link;
	std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "entities=on: 0 scenes a second (0.00 MB/s); the newest 0 batches, 0 vertices, 0 glowing from block light 15, 0 "
	                  "translucent; 0 textures (0.00 MB), 0 left out; drawn in 0 frames, 0 draws skipped, GPU not measured; world: nothing"));
	CHECK(status.find("blasts=") < status.find("entities=") && status.find("entities=") < status.find("ring="));
	link.scenesPerSecond = 60.0;
	link.sceneMbPerSecond = 7.25;
	link.sceneBatches = 12;
	link.sceneVertices = 4152;
	link.sceneGlowing = 720;
	link.sceneTranslucent = 78;
	link.glowLight = 14;
	link.entityTextures = 14;
	link.entityTextureMb = 3.64;
	link.entityFrames = 59;
	link.entityDrawsSkipped = 2;
	link.entityGpuMsAverage = 0.05;
	link.worldEntities = "1 arrow, 1 block, 1 item, 4 shadows";
	link.nearestMob = 5.04;
	link.snapshotWaits = 1;
	link.snapshotCut = 3;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report),
	          "entities=on: 60 scenes a second (7.25 MB/s); the newest 12 batches, 4152 vertices, 720 glowing from block light 14, 78 "
	          "translucent; 14 textures (3.64 MB), 0 left out; drawn in 59 frames, 2 draws skipped, GPU 0.050 ms average; world: 1 arrow, "
	          "1 block, 1 item, 4 shadows; the nearest mob 5.0 m away; 1 frames waited for a snapshot; 3 draws past a snapshot's 8 MB"));
	link.entitiesOn = false;
	report.link = link;
	CHECK(cpmc::FormatStatus(report).find("\nentities=off: 60 scenes a second") != std::string::npos);
}
