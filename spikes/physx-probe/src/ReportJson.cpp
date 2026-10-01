#include "ReportJson.h"

#include "Json.h"

#include <format>
#include <utility>

namespace cpmc
{
namespace
{
void Vec(JsonWriter& aJson, const Vec3d& aValue)
{
	aJson.BeginArray().Number(aValue.x).Number(aValue.y).Number(aValue.z).EndArray();
}

void Vec(JsonWriter& aJson, const Vec3f& aValue)
{
	aJson.BeginArray().Number(aValue.x).Number(aValue.y).Number(aValue.z).EndArray();
}

void Counts(JsonWriter& aJson, const Tally& aTally)
{
	aJson.BeginObject();
	for (const auto& [name, count] : aTally)
	{
		aJson.Key(name).UInt(count);
	}
	aJson.EndObject();
}

// The minimum, median and maximum are null when there are no points.
void Points(JsonWriter& aJson, const Spread& aSpread)
{
	aJson.BeginObject();
	aJson.Key("count").UInt(aSpread.count);
	for (const auto& [key, value] : { std::pair{ "min", &aSpread.min }, std::pair{ "median", &aSpread.median },
	                                  std::pair{ "max", &aSpread.max } })
	{
		aJson.Key(key);
		aSpread.count ? Vec(aJson, *value) : void(aJson.Null());
	}
	aJson.EndObject();
}

void Capsule(JsonWriter& aJson, const CapsuleSample& aCapsule)
{
	aJson.BeginObject();
	aJson.Key("actor").String(aCapsule.actor);
	aJson.Key("center");
	Vec(aJson, aCapsule.center);
	aJson.Key("axis");
	Vec(aJson, aCapsule.axis);
	aJson.Key("radius").Number(aCapsule.radius);
	aJson.Key("halfHeight").Number(aCapsule.halfHeight);
	aJson.Key("kind").String(aCapsule.kind);
	aJson.Key("queryFilter").String(aCapsule.queryFilter);
	aJson.Key("simulationFilter").String(aCapsule.simulationFilter);
	aJson.EndObject();
}

void Player(JsonWriter& aJson, const PlayerPose& aPose)
{
	aJson.BeginObject();
	aJson.Key("feet");
	Vec(aJson, aPose.feet);
	aJson.Key("forward");
	Vec(aJson, aPose.forward);
	aJson.Key("camera");
	if (aPose.camera)
	{
		aJson.BeginObject();
		aJson.Key("position");
		Vec(aJson, aPose.camera->position);
		aJson.Key("forward");
		Vec(aJson, aPose.camera->forward);
		aJson.Key("fov").Number(aPose.camera->fov);
		aJson.EndObject();
	}
	else
	{
		aJson.Null();
	}
	aJson.EndObject();
}

void Scene(JsonWriter& aJson, const SceneCensus& aScene)
{
	aJson.BeginObject();
	aJson.Key("index").Int(aScene.index);
	aJson.Key("gravity");
	Vec(aJson, aScene.gravity);
	aJson.Key("flags").String(std::format("0x{:x}", aScene.flags));
	aJson.Key("requiresRwLock").Bool(aScene.requiresRwLock);
	aJson.Key("staticActors").UInt(aScene.staticActors);
	aJson.Key("dynamicActors").UInt(aScene.dynamicActors);
	aJson.Key("aggregates").UInt(aScene.aggregates);
	aJson.Key("actorsInAggregates").UInt(aScene.actorsInAggregates);
	aJson.Key("articulations").UInt(aScene.articulations);
	aJson.Key("byConcreteType");
	Counts(aJson, aScene.byConcreteType);
	aJson.Key("byGeometryType");
	Counts(aJson, aScene.byGeometryType);
	aJson.Key("shapeFlags");
	Counts(aJson, aScene.shapeFlags);
	aJson.Key("queryFilter");
	Counts(aJson, aScene.queryFilter);
	aJson.Key("simulationFilter");
	Counts(aJson, aScene.simulationFilter);
	aJson.Key("namedActors").UInt(aScene.namedActors);
	aJson.Key("actorsWithUserData").UInt(aScene.actorsWithUserData);
	aJson.Key("actorNameSamples").BeginArray();
	for (const auto& name : aScene.actorNameSamples)
	{
		aJson.String(name);
	}
	aJson.EndArray();
	aJson.Key("skipped").BeginObject().Key("unreadable").UInt(aScene.skippedUnreadable);
	aJson.Key("unauditedVtable").UInt(aScene.skippedUnaudited).EndObject();
	aJson.Key("mismatches").UInt(aScene.mismatches);
	aJson.Key("censusMs").Number(aScene.censusMs);
	aJson.Key("staticCenters");
	Points(aJson, aScene.staticCenters);
	aJson.Key("dynamicCenters");
	Points(aJson, aScene.dynamicCenters);
	aJson.Key("capsules").BeginArray();
	for (const CapsuleSample& capsule : aScene.capsules)
	{
		Capsule(aJson, capsule);
	}
	aJson.EndArray();
	aJson.EndObject();
}

void Region(JsonWriter& aJson, const RegionResult& aRegion)
{
	aJson.BeginObject();
	aJson.Key("center");
	Vec(aJson, aRegion.center);
	aJson.Key("halfExtents");
	Vec(aJson, aRegion.halfExtents);
	aJson.Key("boundsSelectedActors").UInt(aRegion.boundsSelectedActors);
	aJson.Key("overlapHits").UInt(aRegion.overlapHits);
	aJson.Key("overlapOverflow").Bool(aRegion.overlapOverflow);
	aJson.Key("selectMs").Number(aRegion.selectMs);
	aJson.Key("overlapMs").Number(aRegion.overlapMs);
	aJson.Key("extractMs").Number(aRegion.extractMs);
	aJson.Key("triangles").UInt(aRegion.triangles);
	aJson.Key("trianglesByCategory");
	Counts(aJson, aRegion.trianglesByCategory);
	aJson.Key("skipped").BeginObject().Key("plane").UInt(aRegion.skippedPlanes);
	aJson.Key("unknownGeometry").UInt(aRegion.unknownGeometry);
	aJson.Key("badTriangles").UInt(aRegion.badTriangles);
	aJson.Key("unreadable").UInt(aRegion.unreadable).EndObject();
	aJson.Key("triangleCapHit").Bool(aRegion.triangleCapHit);
	aJson.EndObject();
}

void Coordinates(JsonWriter& aJson, const CoordinateCheck& aCheck)
{
	aJson.BeginObject();
	aJson.Key("rayHit").Bool(aCheck.rayHit);
	aJson.Key("hitDistance");
	aCheck.rayHit ? aJson.Number(aCheck.hitDistance) : aJson.Null();
	aJson.Key("scene").Int(aCheck.scene);
	aJson.Key("pass").Bool(aCheck.pass);
	aJson.Key("nearestCapsule");
	if (aCheck.nearestCapsule)
	{
		aJson.BeginObject();
		aJson.Key("distance").Number(aCheck.nearestCapsule->distance);
		aJson.Key("radius").Number(aCheck.nearestCapsule->radius);
		aJson.Key("halfHeight").Number(aCheck.nearestCapsule->halfHeight);
		aJson.Key("kind").String(aCheck.nearestCapsule->kind);
		aJson.EndObject();
	}
	else
	{
		aJson.Null();
	}
	aJson.EndObject();
}
}

std::string ReportToJson(const DumpReport& aReport)
{
	JsonWriter json;
	json.BeginObject();
	json.Key("probeVersion").String(aReport.probeVersion);
	json.Key("time").String(aReport.time);
	json.Key("trigger").String(aReport.trigger);
	json.Key("game").BeginObject();
	json.Key("runtime").String(aReport.runtime);
	json.Key("physxFileVersion").String(aReport.physxFileVersion);
	json.Key("modules").BeginArray();
	for (const ModuleReport& module : aReport.modules)
	{
		json.BeginObject();
		json.Key("name").String(module.name);
		json.Key("timestamp").String(std::format("0x{:x}", module.timestamp));
		json.Key("imageSize").String(std::format("0x{:x}", module.imageSize));
		json.Key("audited").Bool(module.audited);
		json.EndObject();
	}
	json.EndArray();
	json.EndObject();
	json.Key("player");
	aReport.player ? Player(json, *aReport.player) : void(json.Null());
	json.Key("physxOrigin");
	aReport.physxOrigin ? Vec(json, *aReport.physxOrigin) : void(json.Null());
	json.Key("compatibility").BeginObject();
	json.Key("pass").Bool(aReport.compatibilityPass);
	json.Key("steps").BeginArray();
	for (const CompatStep& step : aReport.steps)
	{
		json.BeginObject();
		json.Key("step").Int(step.step);
		json.Key("name").String(step.name);
		json.Key("pass").Bool(step.pass);
		json.Key("detail").String(step.detail);
		json.EndObject();
	}
	json.EndArray();
	json.Key("censusSkipped").UInt(aReport.censusSkipped);
	json.Key("censusMismatches").UInt(aReport.censusMismatches);
	json.EndObject();
	json.Key("scenes").BeginArray();
	for (const SceneCensus& scene : aReport.scenes)
	{
		Scene(json, scene);
	}
	json.EndArray();
	json.Key("region");
	aReport.region ? Region(json, *aReport.region) : void(json.Null());
	json.Key("coordinateCheck");
	aReport.coordinateCheck ? Coordinates(json, *aReport.coordinateCheck) : void(json.Null());
	json.Key("mainThreadPauseMs").Number(aReport.mainThreadPauseMs);
	json.Key("files").BeginObject();
	if (aReport.objWritten)
	{
		json.Key("obj").String("collision.obj");
		json.Key("mtl").String("collision.mtl");
	}
	json.EndObject();
	json.EndObject();
	return json.Str() + "\n";
}
}
