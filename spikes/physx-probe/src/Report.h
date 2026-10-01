#pragma once

#include "Coords.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Plain data for one dump's report.json (see the spec's report schema).
namespace cpmc
{
using Tally = std::map<std::string, uint64_t>;

struct CameraPose
{
	Vec3d position;
	Vec3f forward;
	float fov = 0.0f;
};

struct PlayerPose
{
	Vec3d feet;
	Vec3f forward;
	std::optional<CameraPose> camera;
};

// One of the game's PhysX DLLs, and whether it is the build the audit covers.
struct ModuleReport
{
	std::string name;
	uint32_t timestamp = 0;
	uint32_t imageSize = 0;
	bool audited = false;
};

struct CompatStep
{
	int step = 0;
	std::string name;
	bool pass = false;
	std::string detail;
};

struct SceneCensus
{
	int index = 0;
	Vec3f gravity;
	uint32_t flags = 0;
	bool requiresRwLock = false;
	uint32_t staticActors = 0;
	uint32_t dynamicActors = 0;
	uint32_t aggregates = 0;
	uint32_t actorsInAggregates = 0;
	uint32_t articulations = 0;
	Tally byConcreteType;
	Tally byGeometryType;
	Tally shapeFlags;
	Tally queryFilter;
	Tally simulationFilter;
	uint32_t namedActors = 0;
	uint32_t actorsWithUserData = 0;
	std::vector<std::string> actorNameSamples;
	uint64_t skippedUnreadable = 0;  // objects whose memory couldn't be read
	uint64_t skippedUnaudited = 0;   // objects whose vtable isn't an audited table
	uint64_t mismatches = 0;         // type IDs that disagree with the vtable, meshes that don't fit their shape
	double censusMs = 0.0;
};

struct RegionResult
{
	Vec3d center;
	Vec3f halfExtents;
	uint32_t boundsSelectedActors = 0;
	uint32_t overlapHits = 0;
	bool overlapOverflow = false;
	double selectMs = 0.0;
	double overlapMs = 0.0;
	double extractMs = 0.0;
	uint64_t triangles = 0;
	Tally trianglesByCategory;
	uint32_t skippedPlanes = 0;
	uint32_t unknownGeometry = 0;
	uint32_t badTriangles = 0;
	uint32_t unreadable = 0;  // shapes and meshes that failed their checks during extraction
	bool triangleCapHit = false;
};

struct NearestCapsule
{
	double distance = 0.0;
	float radius = 0.0f;
	float halfHeight = 0.0f;
	std::string kind;
};

struct CoordinateCheck
{
	bool rayHit = false;
	double hitDistance = 0.0;
	int scene = -1;
	bool pass = false;
	std::optional<NearestCapsule> nearestCapsule;
};

struct DumpReport
{
	std::string probeVersion = "0.2.0";
	std::string time;
	std::string trigger;
	std::string runtime;
	std::string physxFileVersion;
	std::vector<ModuleReport> modules;
	std::optional<PlayerPose> player;
	bool compatibilityPass = false;
	std::vector<CompatStep> steps;
	uint64_t censusSkipped = 0;
	uint64_t censusMismatches = 0;
	std::vector<SceneCensus> scenes;
	std::optional<RegionResult> region;
	std::optional<CoordinateCheck> coordinateCheck;
	double mainThreadPauseMs = 0.0;
	bool objWritten = false;
};
}
