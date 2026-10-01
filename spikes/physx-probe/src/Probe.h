#pragma once

#include "ObjWriter.h"
#include "Report.h"
#include "physx/PhysxSession.h"

#include <optional>
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

// PhysX coordinates for a world position: world - aPhysxOrigin, or the same when there's no origin.
physx::PxVec3 PhysxFromWorld(const Vec3d& aWorld, const std::optional<Vec3d>& aPhysxOrigin);

// Moves triangles from PhysX coordinates into world coordinates (adds aPhysxOrigin).
void MoveToWorld(std::vector<px::Tri>& aTriangles, const Vec3d& aPhysxOrigin);

// One dump on the main thread: the compatibility check, then per scene the census and region work under its read
// lock, then the coordinate check. The region box is V's feet + (0, 0, 8) with half-extents (32, 32, 24) m. With
// aPhysxOrigin, PhysX is read at world - aPhysxOrigin, and the triangles come back in world coordinates.
DumpData RunDump(px::PhysxSession& aSession, const PlayerPose& aPose, const std::string& aTrigger,
                 const std::string& aRuntime, const std::string& aTime,
                 const std::optional<Vec3d>& aPhysxOrigin = std::nullopt);
}
