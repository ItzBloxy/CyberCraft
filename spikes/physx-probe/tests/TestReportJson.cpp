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
	report.runtime = "2.3.1";
	report.physxFileVersion = "3.4.2.0";
	report.modules.push_back({ "PhysX3_x64.dll", 0x64c978b4, 0x281000, true });
	report.modules.push_back({ "PhysX3Common_x64.dll", 0, 0, false });
	report.steps.push_back({ 1, "build", false, "PhysX3Common_x64.dll is not loaded" });
	const std::string json = cpmc::ReportToJson(report);
	CHECK(Contains(json, "\"probeVersion\": \"0.3.0\""));
	CHECK(Contains(json, "\"physxOrigin\": null"));
	CHECK(Contains(json, "\"trigger\": \"request\""));
	CHECK(Contains(json, "\"runtime\": \"2.3.1\""));
	CHECK(Contains(json, "\"name\": \"PhysX3_x64.dll\""));
	CHECK(Contains(json, "\"timestamp\": \"0x64c978b4\""));
	CHECK(Contains(json, "\"imageSize\": \"0x281000\""));
	CHECK(Contains(json, "\"audited\": true"));
	CHECK(Contains(json, "\"audited\": false"));
	CHECK(Contains(json, "\"player\": null"));
	CHECK(Contains(json, "\"pass\": false"));
	CHECK(Contains(json, "\"detail\": \"PhysX3Common_x64.dll is not loaded\""));
	CHECK(Contains(json, "\"censusSkipped\": 0"));
	CHECK(Contains(json, "\"region\": null"));
	CHECK(Contains(json, "\"coordinateCheck\": null"));
	CHECK(Contains(json, "\"files\": {}"));
	CHECK(!Contains(json, "faults"));
}

TEST(ReportWithGeometryHasItsSections)
{
	cpmc::DumpReport report;
	cpmc::PlayerPose pose;
	pose.feet = { 1.0, 2.0, 3.0 };
	pose.forward = { 0.0f, 1.0f, 0.0f };
	pose.camera = cpmc::CameraPose{ { 1.0, 2.0, 4.7 }, { 0.0f, 1.0f, 0.0f }, 80.0f };
	report.player = pose;
	report.censusSkipped = 5;
	cpmc::SceneCensus scene;
	scene.gravity = { 0.0f, 0.0f, -9.81f };
	scene.flags = 0x1000;
	scene.requiresRwLock = true;
	scene.byConcreteType["PxRigidStatic"] = 7;
	scene.actorNameSamples = { "door \"A\"" };
	scene.skippedUnreadable = 2;
	scene.skippedUnaudited = 3;
	scene.mismatches = 1;
	scene.staticCenters = { 2, { -10.0, 5.0, 0.0 }, { 1.0, 6.0, 2.0 }, { 12.0, 7.0, 4.0 } };
	scene.capsules.push_back({ "0x1234", { 1.0, 2.0, 3.0 }, { 0.0f, 0.0f, 1.0f }, 0.5f, 0.75f, "kinematic", "1:0:0:0", "0:0:0:0" });
	report.scenes.push_back(scene);
	report.physxOrigin = cpmc::Vec3d{ 100.0, -200.0, 0.5 };
	cpmc::RegionResult region;
	region.triangles = 42;
	region.trianglesByCategory["static_trimesh"] = 42;
	region.unreadable = 4;
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
	CHECK(Contains(json, "\"censusSkipped\": 5"));
	CHECK(Contains(json, "\"flags\": \"0x1000\""));
	CHECK(Contains(json, "\"requiresRwLock\": true"));
	CHECK(Contains(json, "\"PxRigidStatic\": 7"));
	CHECK(Contains(json, "\"door \\\"A\\\"\""));
	CHECK(Contains(json, "\"unreadable\": 2"));
	CHECK(Contains(json, "\"unauditedVtable\": 3"));
	CHECK(Contains(json, "\"mismatches\": 1"));
	CHECK(Contains(json, "\"static_trimesh\": 42"));
	CHECK(Contains(json, "\"unreadable\": 4"));
	CHECK(Contains(json, "\"hitDistance\": 1.75"));
	CHECK(Contains(json, "\"nearestCapsule\": null"));
	CHECK(Contains(json, "\"obj\": \"collision.obj\""));
	CHECK(Contains(json, "\"physxOrigin\": [\n    100,\n    -200,\n    0.5\n  ]"));
	CHECK(Contains(json, "\"staticCenters\": {\n        \"count\": 2,\n        \"min\": [\n          -10,"));
	CHECK(Contains(json, "\"dynamicCenters\": {\n        \"count\": 0,\n        \"min\": null,\n        \"median\": null,"));
	CHECK(Contains(json, "\"actor\": \"0x1234\""));
	CHECK(Contains(json, "\"radius\": 0.5"));
	CHECK(Contains(json, "\"halfHeight\": 0.75"));
	CHECK(Contains(json, "\"kind\": \"kinematic\""));
	CHECK(Contains(json, "\"queryFilter\": \"1:0:0:0\""));
	CHECK(Contains(json, "\"simulationFilter\": \"0:0:0:0\""));
}
