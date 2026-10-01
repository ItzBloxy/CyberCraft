#include "Probe.h"

#include "physx/Checks.h"

#include <cmath>
#include <format>
#include <map>

using namespace physx;

namespace cpmc
{
PxVec3 PhysxFromWorld(const Vec3d& aWorld, const std::optional<Vec3d>& aPhysxOrigin)
{
	const Vec3d origin = aPhysxOrigin.value_or(Vec3d{});
	return PxVec3(float(aWorld.x - origin.x), float(aWorld.y - origin.y), float(aWorld.z - origin.z));
}

void MoveToWorld(std::vector<px::Tri>& aTriangles, const Vec3d& aPhysxOrigin)
{
	for (px::Tri& tri : aTriangles)
	{
		for (PxVec3& v : tri.v)
		{
			v = PxVec3(float(v.x + aPhysxOrigin.x), float(v.y + aPhysxOrigin.y), float(v.z + aPhysxOrigin.z));
		}
	}
}

DumpData RunDump(px::PhysxSession& aSession, const PlayerPose& aPose, const std::string& aTrigger,
                 const std::string& aRuntime, const std::string& aTime, const std::optional<Vec3d>& aPhysxOrigin)
{
	DumpData    data;
	DumpReport& report = data.report;
	report.time = aTime;
	report.trigger = aTrigger;
	report.runtime = aRuntime;
	report.player = aPose;
	report.physxOrigin = aPhysxOrigin;

	const px::CheckResult compat = aSession.Check();
	report.physxFileVersion = compat.physxFileVersion;
	report.modules = compat.modules;
	report.steps = compat.steps;
	report.compatibilityPass = compat.pass;
	if (!compat.pass)
	{
		return data;  // no geometry without a passing compatibility check
	}

	// The region, the ray and the nearest capsule work in PhysX coordinates; the report and the OBJ stay in world ones.
	const std::vector<PxScene*>&                scenes = compat.scenes;
	const PxVec3                                feet = PhysxFromWorld(aPose.feet, aPhysxOrigin);
	const PxVec3                                center = feet + PxVec3(0.0f, 0.0f, 8.0f);
	const PxVec3                                halfExtents(32.0f, 32.0f, 24.0f);
	const PxBounds3                             regionBox = PxBounds3::centerExtents(center, halfExtents);
	RegionResult                                region;
	region.center = { aPose.feet.x, aPose.feet.y, aPose.feet.z + 8.0 };
	region.halfExtents = { halfExtents.x, halfExtents.y, halfExtents.z };
	std::map<std::string, std::vector<px::Tri>> byCategory;
	std::optional<NearestCapsule>               nearest;
	uint64_t                                    kept = 0;

	for (size_t i = 0; i < scenes.size(); ++i)
	{
		px::SceneDump scene = aSession.DumpScene(scenes[i], int(i), regionBox, feet, kTriangleCap - kept);
		report.censusSkipped += scene.census.skippedUnreadable + scene.census.skippedUnaudited;
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
		region.unreadable += scene.stats.unreadable;
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
		const std::optional<float> distance = aSession.RaycastDown(scenes[i], from, 3.0f);
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
	report.compatibilityPass = compat.pass && report.censusMismatches == 0 && report.censusSkipped == 0;
	for (auto& [name, triangles] : byCategory)
	{
		if (aPhysxOrigin)
		{
			MoveToWorld(triangles, *aPhysxOrigin);
		}
		data.groups.push_back({ name, CategoryColor(name), std::move(triangles) });
	}
	report.objWritten = !data.groups.empty();
	return data;
}
}
