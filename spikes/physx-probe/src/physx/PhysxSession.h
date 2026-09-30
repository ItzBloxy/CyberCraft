#pragma once

#include "Report.h"
#include "physx/PxHeaders.h"
#include "physx/ShapeTriangles.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// The only code that calls the game's PhysX. Every call runs under a crash guard, so a bad read ends that
// operation (and counts a fault) instead of crashing the game.
namespace cpmc::px
{
using GuardedFn = void (*)(void* aContext);

// Runs aFn(aContext) under a structured-exception guard. False if it faulted (or threw).
bool RunGuarded(GuardedFn aFn, void* aContext) noexcept;

// Runs a captureless lambda taking a Context& under the guard.
template<class Context, class Fn>
bool Guarded(Context& aContext, Fn aFn)
{
	static_assert(std::is_convertible_v<Fn, void (*)(Context&)>, "use a captureless lambda");
	struct Thunk
	{
		static void Call(void* aRaw)
		{
			auto& pack = *static_cast<std::pair<Context*, void (*)(Context&)>*>(aRaw);
			pack.second(*pack.first);
		}
	};
	std::pair<Context*, void (*)(Context&)> pack{ &aContext, static_cast<void (*)(Context&)>(aFn) };
	return RunGuarded(&Thunk::Call, &pack);
}

// A module's version resource: the binary FILEVERSION field and the "FileVersion" string. They can disagree:
// NVIDIA's PhysX 3.4.2 release says 3.4.1.0 in the binary field and 3.4.2.0 in the string. Empty if unreadable.
struct FileVersions
{
	std::string binary;
	std::string text;
};

// aModule is an HMODULE.
FileVersions ReadFileVersions(void* aModule);

struct OpenResult
{
	bool                    pass = false;
	std::vector<CompatStep> steps;
	std::string             physxFileVersion;
};

// Census and region results for one scene.
struct SceneDump
{
	SceneCensus                             census;
	uint32_t                                boundsSelected = 0;
	uint32_t                                overlapHits = 0;
	bool                                    overlapCapped = false;
	double                                  selectMs = 0.0;
	double                                  overlapMs = 0.0;
	double                                  extractMs = 0.0;
	std::map<std::string, std::vector<Tri>> triangles;  // by category
	AppendStats                             stats;
	std::optional<NearestCapsule>           nearestCapsule;
	bool                                    triangleCapHit = false;
};

class PhysxSession
{
public:
	// The first call finds PhysX and runs the compatibility check; later calls return the cached result.
	const OpenResult& Open();

	// The current scenes, fetched fresh. Empty unless Open() passed.
	std::vector<physx::PxScene*> FetchScenes(uint32_t& aFaults) const;

	// Census, then region selection, overlap query and triangle extraction, all under the scene's read lock.
	// aFeet is V's feet (world), used for the nearest capsule. At most aTriangleBudget triangles are kept.
	SceneDump DumpScene(physx::PxScene* aScene, int aIndex, const physx::PxBounds3& aRegion, const physx::PxVec3& aFeet,
	                    uint64_t aTriangleBudget, uint32_t& aFaults) const;

	// A ray straight down against static geometry. The distance to the nearest hit, if any.
	std::optional<float> RaycastDown(physx::PxScene* aScene, const physx::PxVec3& aFrom, float aMaxDistance,
	                                 uint32_t& aFaults) const;

private:
	physx::PxPhysics*         physics_ = nullptr;
	std::optional<OpenResult> open_;
};
}
