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
	                   aLink.turnRatio ? std::format("{:.4f} degrees a count from {} turns", *aLink.turnRatio, aLink.turnTurns)
	                                   : std::format("ratio not measured yet ({} of {} turns)", aLink.turnTurns, link::TurnRatio::kKnown),
	                   MinecraftMovement(aLink.minecraftFlags));
	return out;
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
	const std::string floor = aLink.floorSent ? std::format("floor {} at height {:.2f} around {:.1f} {:.1f}", aLink.floorEpoch,
	                                                        aLink.floorCentre.y, aLink.floorCentre.x, aLink.floorCentre.z)
	                                          : "floor not sent";
	out += std::format("positions=Minecraft's eye {}; V's camera {}{}; {} teleports in the last second{}; {}\n",
	                   aLink.minecraft ? Position(aLink.minecraftEye) : "unknown", aLink.camera ? Position(*aLink.camera) : "unknown",
	                   aLink.minecraft || aLink.camera ? " (Minecraft axes)" : "", aLink.teleports,
	                   aLink.inGame || aLink.teleports > 0 ? ", the last to " + Position(aLink.target) : "", floor);
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
