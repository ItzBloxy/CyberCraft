#include "Status.h"

#include "link/Turn.h"

#include <skycraft_protocol.h>

#include <format>
#include <utility>

namespace cpmc
{
namespace
{
std::string Ms(double aAverage, double aWorst)
{
	if (aAverage < 0.0)
	{
		return "not measured";
	}
	return std::format("{:.3f} ms average, {:.3f} ms worst", aAverage, aWorst);
}

std::string OneLine(std::string aText)
{
	for (char& ch : aText)
	{
		if (ch == '\n' || ch == '\r')
		{
			ch = ' ';
		}
	}
	return aText;
}

std::string Position(const Vec3d& aPosition)
{
	return std::format("{:.2f} {:.2f} {:.2f}", aPosition.x, aPosition.y, aPosition.z);
}

std::string MsAverage(double aAverage, const char* aWhere = "")
{
	return aAverage < 0.0 ? "not measured" : std::format("{:.3f} ms average{}", aAverage, aWhere);
}

std::string Latency(const LatencySample& aSample)
{
	return std::format("Minecraft {:.0f} ms, ours {:.1f} ms ({:.1f} frames)", aSample.minecraftMs, aSample.oursMs, aSample.oursFrames);
}

// How Minecraft's player moves, from McState's flags: "on the ground, sneaking", "in the air, flying".
std::string MinecraftMovement(uint32_t aFlags)
{
	namespace proto = skycraft::proto;
	std::string out = aFlags & proto::kMcOnGround ? "on the ground" : "in the air";
	for (const auto& [flag, name] : { std::pair{ proto::kMcSneaking, "sneaking" }, std::pair{ proto::kMcSprinting, "sprinting" },
	                                  std::pair{ proto::kMcSwimming, "swimming" }, std::pair{ proto::kMcFlying, "flying" },
	                                  std::pair{ proto::kMcDead, "dead" } })
	{
		if (aFlags & flag)
		{
			out += std::string(", ") + name;
		}
	}
	return out;
}

// The turn's ratio: this session's, the last session's until then, or none yet.
std::string TurnRatioText(const LinkReport& aLink)
{
	if (!aLink.turnRatio)
	{
		return std::format("ratio not measured yet ({} of {} turns)", aLink.turnTurns, link::TurnRatio::kKnown);
	}
	return aLink.turnRemembered ? std::format("{:.4f} degrees a count, remembered from the last session ({} of {} turns measured)",
	                                          *aLink.turnRatio, aLink.turnTurns, link::TurnRatio::kKnown)
	                            : std::format("{:.4f} degrees a count from {} turns", *aLink.turnRatio, aLink.turnTurns);
}

std::string FormatDrive(const LinkReport& aLink)
{
	std::string out = "drive=" + aLink.drive;
	if (aLink.drive != "off")
	{
		out += std::format(" for {:.1f} s", aLink.driveFor);
	}
	out += std::format("; V's eye {:.2f} m above its feet", aLink.eyeHeight);
	if (aLink.eyeDistanceLast)
	{
		out += std::format("; V's camera from Minecraft's eye {:.3f} m, {:.3f} m average and {:.3f} m worst in the last second",
		                   *aLink.eyeDistanceLast, aLink.eyeDistanceAverage, aLink.eyeDistanceWorst);
	}
	out += std::format("; render delay {:.1f} ms, {} frames waited for a late tick in the last second; {} bad targets skipped "
	                   "since the start; teleport {}; V's health {}; turn {}, {}; Minecraft's player {}\n",
	                   aLink.renderDelayMs, aLink.waitedFrames, aLink.skippedFrames,
	                   aLink.teleportMsAverage < 0.0 ? std::string("not called") : Ms(aLink.teleportMsAverage, aLink.teleportMsWorst),
	                   aLink.health ? std::format("{:.0f}%", *aLink.health) : std::string("unknown"), aLink.turn ? "on" : "off",
	                   TurnRatioText(aLink), MinecraftMovement(aLink.minecraftFlags));
	return out;
}

// V's meshes in build mode (spike 6's "Part 2"): hidden while build mode is on, unless set hidev is 0.
std::string FormatV(const LinkReport& aLink)
{
	if (!aLink.hideV)
	{
		return "v=shown: set hidev is 0\n";
	}
	if (!aLink.vHidden)
	{
		return "v=shown, and hidden in build mode\n";
	}
	switch (aLink.vMeshes)
	{
	case -1:
		return "v=shown: hiding V's meshes needs Codeware, which isn't installed\n";
	case -2:
		return "v=shown: V isn't found\n";
	default:
		return std::format("v=hidden: {} meshes switched off, {} effects stopped{}\n", aLink.vMeshes, aLink.vEffects,
		                   aLink.vCrosshair > 0   ? ", the crosshair hidden"
		                   : aLink.vCrosshair < 0 ? ", the crosshair not found"
		                                          : "");
	}
}

// The ground's worker: how long a region took in the last second, of how many triangles, and how much of that time its
// threads ran, which says whether the game's threads kept them waiting.
std::string Worker(const LinkReport& aLink)
{
	if (aLink.workerThreads == 0)
	{
		return "the worker not started yet";
	}
	const std::string work = aLink.workerMsPerRegion < 0.0
	                             ? std::string("idle")
	                             : std::format("{:.2f} ms a region of {:.0f} triangles, its threads running {:.0f}% of that time",
	                                           aLink.workerMsPerRegion, aLink.workerTrianglesPerRegion, aLink.workerRunning * 100.0);
	return std::format("the worker {}; {} threads, {} regions waiting, {} replaced by newer ones since the start", work,
	                   aLink.workerThreads, aLink.workerJobs, aLink.workerReplaced);
}

std::string FormatGround(const LinkReport& aLink)
{
	if (aLink.ground != "city")
	{
		return std::format("ground=floor{}\n", aLink.groundWhy.empty() ? "" : "; it fell back from the city: " + OneLine(aLink.groundWhy));
	}
	std::string out = std::format("ground=city, epoch {}", aLink.collisionEpoch);
	out += aLink.cityCentre ? std::format(", around region {} {} {}: {} regions known, {} batches to read", aLink.cityCentre->x,
	                                      aLink.cityCentre->y, aLink.cityCentre->z, aLink.cityKnown, aLink.cityWaiting)
	                        : ", no centre yet";
	out += std::format("; read {:.2f} ms a frame on average, {:.2f} ms at worst; {:.1f} batches, {:.0f} triangles and {:.0f} voxel blocks "
	                   "a second; {}; the ring {:.2f} MB a second, {} messages waiting for room, {} too big",
	                   aLink.cityReadMsAverage, aLink.cityReadMsWorst, aLink.cityBatches, aLink.cityTriangles, aLink.workerBlocks,
	                   Worker(aLink), aLink.collisionMb, aLink.collisionWaiting, aLink.collisionDropped);
	if (aLink.lastHold)
	{
		out += std::format("; Minecraft held its player {:.2f} s when build mode last started", *aLink.lastHold);
	}
	out += aLink.physxOrigin
	           ? std::format("; PhysX's origin ({:.0f}, {:.0f}, {:.0f})", aLink.physxOrigin->x, aLink.physxOrigin->y, aLink.physxOrigin->z)
	           : std::string("; PhysX's origin unknown");
	out += std::format(", V's controller looked for {} times; shapes since the start: {} kept, {} left out, {} unreadable",
	                   aLink.controllerSearches, aLink.shapesKept, aLink.shapesLeft, aLink.shapesUnreadable);
	if (!aLink.physxWhy.empty())
	{
		out += "; nothing can be read now: " + OneLine(aLink.physxWhy);
	}
	return out + "\n";
}

// V's bodies for Minecraft's blocks, and step 4's test box, which stands whether the bodies are on or off.
std::string FormatBodies(const LinkReport& aLink)
{
	return std::format("bodies={}: {} sections with bodies, {} boxes; {} made, {} removed, {} failed and {} lost since the start, the "
	                   "slowest making {:.2f} ms; made of {}; {} origin change{}, {} bodies posed again; {}{}\n",
	                   aLink.bodiesOn ? "on" : "off", aLink.bodySections, aLink.bodyBoxes, aLink.bodiesMade, aLink.bodiesRemoved,
	                   aLink.bodiesFailed, aLink.bodiesLost, aLink.bodySlowestMs,
	                   aLink.bodiesMadeOf.empty() ? "what V stands on, once copied" : OneLine(aLink.bodiesMadeOf), aLink.originChanges,
	                   aLink.originChanges == 1 ? "" : "s", aLink.bodiesReposed,
	                   aLink.box ? "the test box at world " + Position(*aLink.box) : std::string("the test box off"),
	                   aLink.bodiesWhy.empty() ? "" : "; waiting: " + OneLine(aLink.bodiesWhy));
}

// NPCs in Minecraft: how many are listed, the nearest, their sizes, the module's time, and Minecraft's hits on them.
std::string FormatNpcs(const LinkReport& aLink)
{
	if (!aLink.npcsOn)
	{
		return "npcs=off\n";
	}
	std::string out = std::format("npcs=on: {} listed of {} in the world", aLink.npcsListed, aLink.npcsInWorld);
	if (aLink.nearestNpc)
	{
		out += std::format(", the nearest {} at {:.1f} m", aLink.nearestNpcName.empty() ? "(no name)" : OneLine(aLink.nearestNpcName),
		                   *aLink.nearestNpc);
	}
	out += std::format("; capsules found for {} of {} looked for; {} numbers given; the module {}; {} hits from Minecraft logged",
	                   aLink.capsulesFound, aLink.capsulesLookedFor, aLink.npcIdsGiven,
	                   aLink.moduleMsAverage < 0.0 ? std::string("not called") : Ms(aLink.moduleMsAverage, aLink.moduleMsWorst) + " a frame",
	                   aLink.npcHits);
	if (!aLink.npcsWhy.empty())
	{
		out += "; none listed: " + OneLine(aLink.npcsWhy);
	}
	return out + "\n";
}

std::string FormatCombat(const LinkReport& aLink)
{
	if (!aLink.combatOn)
	{
		return "combat=off: Minecraft's hits are logged, not applied; V's health is V's\n";
	}
	std::string out = std::format("combat=on: V's punch {:.2f}{}{}; hit power {:.2f}; hits {} queued, {} landed{}", aLink.punch,
	                              aLink.punchItem.empty() ? "" : " from " + OneLine(aLink.punchItem),
	                              aLink.punchDrawn ? ", drawn" : ", not drawn: hits skip the NPC's armour, weapon crits, stealth, blocks and skill XP",
	                              aLink.hitPower, aLink.hitsQueued, aLink.hitsLanded,
	                              aLink.hitsDropped.empty() ? "" : ", dropped: " + OneLine(aLink.hitsDropped));
	if (!aLink.lastHit.empty())
	{
		out += "; the last " + OneLine(aLink.lastHit);
	}
	out += std::format("; V's hurts: {} taken, {} sent, {} summed, {} left to V, {} non-lethal left to V", aLink.hurtsTaken, aLink.hurtsSent,
	                   aLink.hurtsSummed, aLink.hurtsLeft, aLink.hurtsNonLethal);
	if (!aLink.lastHurt.empty())
	{
		out += "; the last " + OneLine(aLink.lastHurt);
	}
	out += std::format("; Immortal {}; {} deaths; Minecraft's player {}", aLink.immortal ? "on" : "off", aLink.deaths,
	                   aLink.minecraftDeadFor ? std::format("dead for {:.1f} s", *aLink.minecraftDeadFor) : std::string("alive"));
	if (!aLink.combatWhy.empty())
	{
		out += "; " + OneLine(aLink.combatWhy);
	}
	return out + "\n";
}

std::string FormatLink(const LinkReport& aLink)
{
	std::string       out;
	const std::string minecraft = aLink.minecraft
	                                  ? std::format("Minecraft process {}, heartbeat {} ms ago{}{}, {:.0f} frames/s", aLink.minecraftPid,
	                                                aLink.heartbeatAgeMs, aLink.inWorld ? ", in its world" : "",
	                                                aLink.screenOpen ? ", a screen open" : "", aLink.minecraftFps)
	                                  : "Minecraft absent";
	out += std::format("link={}; the mapping {}; {}; {}\n", aLink.linked ? "linked" : "not linked", OneLine(aLink.mapping), minecraft,
	                   aLink.inGame ? "in game" : "not in game");
	out += std::format("build={}; {}; {} inputs forwarded to Minecraft and {} hidden from the game, the last: {}; {} dropped with the "
	                   "input ring full; {}\n",
	                   aLink.buildMode ? std::string("on") : "off: " + OneLine(aLink.buildWhy),
	                   aLink.slot ? std::format("slot {}", *aLink.slot) : std::string("slot unknown until a number key"),
	                   aLink.inputsForwarded, aLink.inputsHidden, OneLine(aLink.lastInput), aLink.inputsDropped,
	                   aLink.inputHooked ? std::format("GetRawInputData hooked, {} keyboard and mouse events read", aLink.inputsRead)
	                                     : std::string("GetRawInputData not hooked (see the log)"));
	out += FormatDrive(aLink);
	out += FormatV(aLink);
	const std::string floor = aLink.floorSent ? std::format("floor {} at height {:.2f} around {:.1f} {:.1f}", aLink.floorEpoch,
	                                                        aLink.floorCentre.y, aLink.floorCentre.x, aLink.floorCentre.z)
	                                          : "floor not sent";
	out += std::format("positions=Minecraft's eye {}; V's camera {}{}; {} teleports in the last second{}; {}\n",
	                   aLink.minecraft ? Position(aLink.minecraftEye) : "unknown", aLink.camera ? Position(*aLink.camera) : "unknown",
	                   aLink.minecraft || aLink.camera ? " (Minecraft axes)" : "", aLink.teleports,
	                   aLink.inGame || aLink.teleports > 0 ? ", the last to " + Position(aLink.target) : "", floor);
	out += FormatGround(aLink);
	out += std::format("solids=Minecraft's solid blocks: {} in {} sections\n", aLink.solidBlocks, aLink.solidSections);
	out += FormatBodies(aLink);
	out += FormatNpcs(aLink);
	out += FormatCombat(aLink);
	out += std::format("ring={:.2f} MB in the last second, at most {:.2f} MB in one frame; drain {:.3f} ms average, {:.3f} ms worst; {}; "
	                   "{} rejected, {} resyncs\n",
	                   aLink.ringMb, aLink.ringMbWorstFrame, aLink.drainMsAverage, aLink.drainMsWorst,
	                   aLink.ringTypes.empty() ? "no messages" : aLink.ringTypes, aLink.rejected, aLink.resyncs);
	out += std::format("atlas={}; {} translucent triangles dropped; update {}\n", aLink.atlas, aLink.translucentDropped,
	                   Ms(aLink.updateMsAverage, aLink.updateMsWorst));
	out += std::format("hud={}, scale {}, {}; {} images received ({:.1f} MB), {} dropped and {} drawn in the last second, {}; "
	                   "copy {}; pass {}; {} skipped{}; {} refused since the start\n",
	                   aLink.hudSetting == 0 ? "never" : aLink.hudSetting == 2 ? "always" : "in build mode", aLink.hudScale,
	                   aLink.hudWanted ? "showing" : "not showing", aLink.hudReceived, aLink.hudMb, aLink.hudDropped, aLink.hudDrawn,
	                   aLink.hudSize.empty() ? "no image yet" : aLink.hudSize, MsAverage(aLink.hudCopyMsAverage),
	                   MsAverage(aLink.hudGpuMsAverage, " on the GPU"), aLink.hudSkipped,
	                   aLink.hudLastSkip.empty() ? "" : " (" + OneLine(aLink.hudLastSkip) + ")", aLink.hudRefused);
	out += aLink.latencyLast && aLink.latencyAverage
	           ? std::format("latency=last: {}; average of {}: {}\n", Latency(*aLink.latencyLast), aLink.latencySamples,
	                         Latency(*aLink.latencyAverage))
	           : std::string("latency=no samples yet: type a command, or click in build mode, while V stands still\n");
	out += aLink.burst ? std::format("burst={}: {} sections, {:.2f} MB over {} frames; worst drain {:.3f} ms, update {:.3f} ms, "
	                                 "frame {:.1f} ms\n",
	                                 OneLine(aLink.burst->what), aLink.burst->sections, double(aLink.burst->bytes) / 1048576.0,
	                                 aLink.burst->frames, aLink.burst->drainMsWorst, aLink.burst->updateMsWorst, aLink.burst->frameMsWorst)
	                   : std::string("burst=none yet\n");
	out += std::format("processes=Cyberpunk {:.0f}% of a core, {} MB; {}\n", aLink.cyberpunkCpu * 100.0, aLink.cyberpunkMemory >> 20,
	                   aLink.minecraftProcessKnown
	                       ? std::format("Minecraft {:.0f}% of a core, {} MB", aLink.minecraftCpu * 100.0, aLink.minecraftMemory >> 20)
	                       : std::string("Minecraft not running"));
	return out;
}
}

std::string FormatStatus(const StatusReport& aReport)
{
	std::string out;
	out += std::format("mode={}\n", aReport.mode);
	out += std::format("frames={} drawn{}, {} skipped{}, {} with a late camera (last second)\n", aReport.framesDrawn,
	                   aReport.framesPartial == 0 ? "" : std::format(" ({} without the tagged depth)", aReport.framesPartial),
	                   aReport.framesSkipped, aReport.lastSkip.empty() ? "" : " (" + OneLine(aReport.lastSkip) + ")",
	                   aReport.lateCamera);
	out += std::format("blocks={} in {} sections, {} vertices\n",
	                   aReport.minecraftBlocks ? std::string("Minecraft's") : std::to_string(aReport.blocks), aReport.sections,
	                   aReport.vertices);
	out += std::format("gpu={}\n", Ms(aReport.gpuMsAverage, aReport.gpuMsWorst));
	out += std::format("frame={}\n", Ms(aReport.frameMsAverage, aReport.frameMsWorst));
	out += std::format("streamline={} frames in the last second\n", aReport.streamlineFrames);
	out += std::format("hooks={}\n", OneLine(aReport.hooks));
	out += std::format("request={}\n", OneLine(aReport.lastRequest));
	if (!aReport.sceneWatch.empty())
	{
		out += std::format("scenewatch={}\n", OneLine(aReport.sceneWatch));
	}
	if (aReport.link)
	{
		out += FormatLink(*aReport.link);
	}
	for (const StatusReport::Structure& structure : aReport.structures)
	{
		out += std::format("structure={} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f}\n", structure.name, structure.worldMin.x,
		                   structure.worldMin.y, structure.worldMin.z, structure.worldMax.x, structure.worldMax.y,
		                   structure.worldMax.z);
	}
	return out;
}
}
