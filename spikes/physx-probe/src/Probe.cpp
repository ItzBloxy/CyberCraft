#include "Probe.h"

#include "physx/Checks.h"

#include <cmath>
#include <format>
#include <map>

using namespace physx;

namespace cpmc
{
DumpData RunDump(px::PhysxSession& aSession, const PlayerPose& aPose, const std::string& aTrigger,
                 const std::string& aRuntime, const std::string& aTime)
{
	DumpData    data;
	DumpReport& report = data.report;
	report.time = aTime;
	report.trigger = aTrigger;
	report.runtime = aRuntime;
	report.player = aPose;

	const px::OpenResult& open = aSession.Open();
	report.physxFileVersion = open.physxFileVersion;
	report.steps = open.steps;
	report.compatibilityPass = open.pass;
	if (!open.pass)
	{
		return data;  // no geometry without a passing compatibility check
	}

	uint32_t                     faults = 0;
	const std::vector<PxScene*>  scenes = aSession.FetchScenes(faults);
	const PxVec3                 feet(float(aPose.feet.x), float(aPose.feet.y), float(aPose.feet.z));
	const PxVec3                 center = feet + PxVec3(0.0f, 0.0f, 8.0f);
	const PxVec3                 halfExtents(32.0f, 32.0f, 24.0f);
	const PxBounds3              regionBox = PxBounds3::centerExtents(center, halfExtents);
	RegionResult                 region;
	region.center = { center.x, center.y, center.z };
	region.halfExtents = { halfExtents.x, halfExtents.y, halfExtents.z };
	std::map<std::string, std::vector<px::Tri>> byCategory;
	std::optional<NearestCapsule>               nearest;
	uint64_t                                    kept = 0;

	for (size_t i = 0; i < scenes.size(); ++i)
	{
		px::SceneDump scene = aSession.DumpScene(scenes[i], int(i), regionBox, feet, kTriangleCap - kept, faults);
		report.censusMismatches += scene.census.mismatches;
		region.boundsSelectedActors += scene.boundsSelected;
		region.overlapHits += scene.overlapHits;
		region.overlapOverflow |= scene.overlapCapped;
		region.selectMs += scene.selectMs;
		region.overlapMs += scene.overlapMs;
		region.extractMs += scene.extractMs;
		region.skippedPlanes += scene.stats.planes;
		region.unknownGeometry += scene.stats.unknownGeometry;
		region.badTriangles += scene.stats.badTriangles;
		region.triangleCapHit |= scene.triangleCapHit;
		for (auto& [category, triangles] : scene.triangles)
		{
			const std::string name = scenes.size() > 1 ? std::format("s{}_{}", i, category) : category;
			region.trianglesByCategory[name] += triangles.size();
			region.triangles += triangles.size();
			kept += triangles.size();
			auto& destination = byCategory[name];
			destination.insert(destination.end(), triangles.begin(), triangles.end());
		}
		if (scene.nearestCapsule && (!nearest || scene.nearestCapsule->distance < nearest->distance))
		{
			nearest = scene.nearestCapsule;
		}
		scene.census.queryFilter = px::TrimTally(scene.census.queryFilter);
		scene.census.simulationFilter = px::TrimTally(scene.census.simulationFilter);
		report.scenes.push_back(std::move(scene.census));
	}

	// The coordinate check: a ray down from 1.8 m above V's feet should hit the ground about 1.8 m down.
	CoordinateCheck check;
	check.nearestCapsule = nearest;
	const PxVec3 from = feet + PxVec3(0.0f, 0.0f, 1.8f);
	for (size_t i = 0; i < scenes.size(); ++i)
	{
		const std::optional<float> distance = aSession.RaycastDown(scenes[i], from, 3.0f, faults);
		if (distance && (!check.rayHit || *distance < check.hitDistance))
		{
			check.rayHit = true;
			check.hitDistance = *distance;
			check.scene = int(i);
		}
	}
	check.pass = check.rayHit && std::fabs(check.hitDistance - 1.8) <= 0.3;

	report.region = region;
	report.coordinateCheck = check;
	report.faults = faults;
	report.compatibilityPass = open.pass && report.censusMismatches == 0;
	for (auto& [name, triangles] : byCategory)
	{
		data.groups.push_back({ name, CategoryColor(name), std::move(triangles) });
	}
	report.objWritten = !data.groups.empty();
	return data;
}
}
