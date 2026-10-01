#pragma once

#include "Report.h"
#include "physx/GameAbi.h"
#include "physx/PxHeaders.h"
#include "physx/Readable.h"
#include "physx/ShapeTriangles.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

// The only code that calls the game's PhysX. It catches no faults: the game's crash handler sees a fault before any
// handler of ours could, and freezes the game if we carry on. So it checks before it touches anything. It calls only
// the methods the audit verified, only on objects whose vtables are audited tables of the audited build, and it
// checks every raw array before reading it.
namespace cpmc::px
{
// A module's version resource: the binary FILEVERSION field and the "FileVersion" string. They can disagree:
// NVIDIA's PhysX 3.4.2 release says 3.4.1.0 in the binary field and 3.4.2.0 in the string. Empty if unreadable.
struct FileVersions
{
	std::string binary;
	std::string text;
};

// aModule is an HMODULE.
FileVersions ReadFileVersions(void* aModule);

// The compatibility check's result for one dump.
struct CheckResult
{
	bool                         pass = false;
	std::vector<CompatStep>      steps;
	std::string                  physxFileVersion;
	std::vector<ModuleReport>    modules;
	std::vector<physx::PxScene*> scenes;  // checked, once step 4 has passed
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
	// Starts a dump: forgets remembered memory regions, then runs the compatibility check. Steps 1-3 describe the
	// build and run once per session; steps 4-7 describe the world and run every time.
	CheckResult Check();

	// Census, region selection, overlap query and triangle extraction, all under the scene's read lock. aScene must
	// come from this dump's Check(). aRegion and aFeet (V's feet, used for the nearest capsule) are in PhysX
	// coordinates. At most aTriangleBudget triangles are kept. The census also notes where the scene's actors are and
	// every capsule on a dynamic actor. After 100 skipped objects the census stops and the region work is left out.
	SceneDump DumpScene(physx::PxScene* aScene, int aIndex, const physx::PxBounds3& aRegion, const physx::PxVec3& aFeet,
	                    uint64_t aTriangleBudget);

	// A ray straight down against static geometry. The distance to the nearest hit, if any.
	std::optional<float> RaycastDown(physx::PxScene* aScene, const physx::PxVec3& aFrom, float aMaxDistance);

private:
	struct Build
	{
		bool                      pass = false;
		std::vector<CompatStep>   steps;
		std::string               physxFileVersion;
		std::vector<ModuleReport> modules;
		ModuleBases               bases;
		physx::PxPhysics*         physics = nullptr;
	};

	// Steps 1-3.
	Build CheckBuild();

	std::optional<Build> build_;
	ReadableMemory       memory_;
};
}
