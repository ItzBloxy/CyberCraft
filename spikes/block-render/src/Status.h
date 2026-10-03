#pragma once

#include "Coords.h"
#include "Measurements.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cpmc
{
// What status.txt says (the spec's "Status and the overlay"). The plugin writes it once a second into the overlay's
// folder; tools/blockrender_hud/init.lua shows it.
// Spike 4's lines (the spec's "Measurements"): the link, build mode, the render ring, Minecraft's atlas, the HUD, latency,
// the last burst, and both games' processes. "In the last second" is from one status to the next.
struct LinkReport
{
	std::string          mapping = "not opened yet";  // created, reused, or refused and why
	bool                 linked = false;
	bool                 minecraft = false;  // its heartbeat is under 3 s old
	uint32_t             minecraftPid = 0;
	uint64_t             heartbeatAgeMs = 0;
	bool                 inWorld = false;
	bool                 screenOpen = false;
	double               minecraftFps = 0.0;  // its frame counter's, over the last second
	bool                 inGame = false;      // linked, with V found
	Vec3d                minecraftEye;        // Minecraft axes
	std::optional<Vec3d> camera;              // V's, Minecraft axes; none while V isn't found
	uint64_t             teleports = 0;       // in the last second
	Vec3d                target;              // the last teleport's, Minecraft axes
	uint32_t             floorEpoch = 0;
	bool                 floorSent = false;
	Vec3d                floorCentre;  // Minecraft axes; y is the floor's height
	// Build mode, and the GetRawInputData patch; counts since the start
	bool               inputHooked = false;
	bool               buildMode = false;
	std::string        buildWhy;             // why it's off
	std::optional<int> slot;                 // Minecraft's hotbar slot, once a number key has set it
	uint64_t           inputsRead = 0;       // keyboard and mouse events
	uint64_t           inputsForwarded = 0;  // to Minecraft
	uint64_t           inputsHidden = 0;     // from the game
	uint64_t           inputsDropped = 0;    // with the input ring full
	std::string        lastInput;            // the last forwarded
	// The render ring
	double      ringMb = 0.0;            // in the last second
	double      ringMbWorstFrame = 0.0;  // the most in one frame of it
	double      drainMsAverage = 0.0;
	double      drainMsWorst = 0.0;
	std::string ringTypes;     // messages and MB by type, in the last second; empty: none
	uint64_t    rejected = 0;  // since the start
	uint64_t    resyncs = 0;
	// Minecraft's blocks
	std::string atlas = "none yet";
	uint64_t    translucentDropped = 0;  // since the start
	double      updateMsAverage = -1.0;  // the renderer's Update; below 0: not measured
	double      updateMsWorst = -1.0;
	// The HUD
	int         hudSetting = 1;  // 0 never, 1 in build mode, 2 always
	uint32_t    hudScale = 2;
	bool        hudWanted = false;
	uint64_t    hudReceived = 0;  // in the last second
	double      hudMb = 0.0;      // of the images received
	uint64_t    hudDropped = 0;   // with no upload buffer free
	uint64_t    hudDrawn = 0;
	std::string hudSize;                  // the newest image's; empty: none yet
	double      hudCopyMsAverage = -1.0;  // SetHudImage's copy; below 0: not measured
	double      hudGpuMsAverage = -1.0;   // the pass's
	uint64_t    hudSkipped = 0;           // in the last second
	std::string hudLastSkip;
	uint64_t    hudRefused = 0;  // by the link, since the start: a bad slot or size
	// Latency, the last burst, and the processes
	std::optional<LatencySample> latencyLast;
	std::optional<LatencySample> latencyAverage;
	size_t                       latencySamples = 0;
	std::optional<BurstReport>   burst;
	double                       cyberpunkCpu = 0.0;   // a share of one core
	uint64_t                     cyberpunkMemory = 0;  // its working set, bytes
	bool                         minecraftProcessKnown = false;
	double                       minecraftCpu = 0.0;
	uint64_t                     minecraftMemory = 0;
};

struct StatusReport
{
	struct Structure
	{
		std::string name;
		Vec3d       worldMin;  // the box it fills, in world coordinates
		Vec3d       worldMax;
	};

	std::string            mode = "off";
	uint64_t               framesDrawn = 0;    // in the last second
	uint64_t               framesPartial = 0;  // of those, gbuffer's drawn into the G-buffer but not the tagged depth
	uint64_t               framesSkipped = 0;  // in the last second
	std::string            lastSkip;           // why the latest skipped frame was skipped
	uint64_t               lateCamera = 0;     // frames drawn with the previous frame's camera, in the last second
	size_t                 sections = 0;
	size_t                 blocks = 0;
	bool                   minecraftBlocks = false;  // the sections are Minecraft's, whose blocks aren't counted
	size_t                 vertices = 0;
	double                 gpuMsAverage = -1.0;  // below 0: not measured
	double                 gpuMsWorst = -1.0;
	double                 frameMsAverage = -1.0;
	double                 frameMsWorst = -1.0;
	uint32_t               streamlineFrames = 0;  // frames Streamline started in the last second
	std::string            hooks;
	std::string               lastRequest;
	std::vector<Structure>    structures;
	std::optional<LinkReport> link;  // spike 4's lines, once the plugin has tried the mapping
};

// Lines of "key=value": mode, frames, blocks, gpu, frame, streamline, hooks, request; with a link, its lines (link, build,
// positions, ring, atlas, hud, latency, burst and processes); then one structure line per structure:
// "structure=<name> <min x> <min y> <min z> <max x> <max y> <max z>".
std::string FormatStatus(const StatusReport& aReport);
}
