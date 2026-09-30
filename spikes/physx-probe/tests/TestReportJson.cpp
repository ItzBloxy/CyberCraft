#include "Harness.h"

#include "ReportJson.h"

namespace
{
bool Contains(const std::string& aText, const std::string& aPart)
{
	return aText.find(aPart) != std::string::npos;
}
}

TEST(ReportWithoutGeometryHasNulls)
{
	cpmc::DumpReport report;
	report.time = "2026-10-01T12:00:00";
	report.trigger = "request";
	report.runtime = "2.31.0";
	report.physxFileVersion = "3.4.2.0";
	report.steps.push_back({ 1, "module", false, "PhysX3_x64.dll is not loaded" });
	const std::string json = cpmc::ReportToJson(report);
	CHECK(Contains(json, "\"probeVersion\": \"0.1.0\""));
	CHECK(Contains(json, "\"trigger\": \"request\""));
	CHECK(Contains(json, "\"runtime\": \"2.31.0\""));
	CHECK(Contains(json, "\"player\": null"));
	CHECK(Contains(json, "\"pass\": false"));
	CHECK(Contains(json, "\"detail\": \"PhysX3_x64.dll is not loaded\""));
	CHECK(Contains(json, "\"region\": null"));
	CHECK(Contains(json, "\"coordinateCheck\": null"));
	CHECK(Contains(json, "\"files\": {}"));
}

TEST(ReportWithGeometryHasItsSections)
{
	cpmc::DumpReport report;
	cpmc::PlayerPose pose;
	pose.feet = { 1.0, 2.0, 3.0 };
	pose.forward = { 0.0f, 1.0f, 0.0f };
	pose.camera = cpmc::CameraPose{ { 1.0, 2.0, 4.7 }, { 0.0f, 1.0f, 0.0f }, 80.0f };
	report.player = pose;
	cpmc::SceneCensus scene;
	scene.gravity = { 0.0f, 0.0f, -9.81f };
	scene.flags = 0x1000;
	scene.requiresRwLock = true;
	scene.byConcreteType["PxRigidStatic"] = 7;
	scene.actorNameSamples = { "door \"A\"" };
	report.scenes.push_back(scene);
	cpmc::RegionResult region;
	region.triangles = 42;
	region.trianglesByCategory["static_trimesh"] = 42;
	report.region = region;
	cpmc::CoordinateCheck check;
	check.rayHit = true;
	check.hitDistance = 1.75;
	check.pass = true;
	check.scene = 0;
	report.coordinateCheck = check;
	report.objWritten = true;
	const std::string json = cpmc::ReportToJson(report);
	CHECK(Contains(json, "\"fov\": 80"));
	CHECK(Contains(json, "\"flags\": \"0x1000\""));
	CHECK(Contains(json, "\"requiresRwLock\": true"));
	CHECK(Contains(json, "\"PxRigidStatic\": 7"));
	CHECK(Contains(json, "\"door \\\"A\\\"\""));
	CHECK(Contains(json, "\"static_trimesh\": 42"));
	CHECK(Contains(json, "\"hitDistance\": 1.75"));
	CHECK(Contains(json, "\"nearestCapsule\": null"));
	CHECK(Contains(json, "\"obj\": \"collision.obj\""));
}
