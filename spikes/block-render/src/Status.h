#pragma once

#include "Coords.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cpmc
{
// What status.txt says (the spec's "Status and the overlay"). The plugin writes it once a second into the overlay's
// folder; tools/blockrender_hud/init.lua shows it.
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
	size_t                 vertices = 0;
	double                 gpuMsAverage = -1.0;  // below 0: not measured
	double                 gpuMsWorst = -1.0;
	double                 frameMsAverage = -1.0;
	double                 frameMsWorst = -1.0;
	uint32_t               streamlineFrames = 0;  // frames Streamline started in the last second
	std::string            hooks;
	std::string            lastRequest;
	std::vector<Structure> structures;
};

// Lines of "key=value": mode, frames, blocks, gpu, frame, streamline, hooks, request, then one structure line per
// structure: "structure=<name> <min x> <min y> <min z> <max x> <max y> <max z>".
std::string FormatStatus(const StatusReport& aReport);
}
