#pragma once

#include "ObjWriter.h"
#include "Report.h"
#include "physx/PhysxSession.h"

#include <string>
#include <vector>

namespace cpmc
{
constexpr uint64_t kTriangleCap = 1'000'000;

struct DumpData
{
	DumpReport            report;
	std::vector<ObjGroup> groups;
};

// One dump on the main thread: the compatibility check, then per scene the census and region work under its read
// lock, then the coordinate check. The region box is V's feet + (0, 0, 8) with half-extents (32, 32, 24) m.
DumpData RunDump(px::PhysxSession& aSession, const PlayerPose& aPose, const std::string& aTrigger,
                 const std::string& aRuntime, const std::string& aTime);
}
