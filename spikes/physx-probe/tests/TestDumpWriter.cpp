#include "Harness.h"

#include "DumpWriter.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

using namespace physx;

namespace
{
std::string ReadFile(const std::filesystem::path& aPath)
{
	std::ifstream      in(aPath, std::ios::binary);
	std::ostringstream text;
	text << in.rdbuf();
	return text.str();
}

cpmc::DumpJob MakeJob(const std::filesystem::path& aFolder, bool aWithGeometry)
{
	cpmc::DumpJob job;
	job.folder = aFolder;
	job.report.trigger = "request";
	job.origin = { 0.0, 0.0, 0.0 };
	if (aWithGeometry)
	{
		cpmc::ObjGroup group{ "static_box", cpmc::CategoryColor("static_box"), {} };
		group.triangles.push_back({ { PxVec3(0, 0, 0), PxVec3(1, 0, 0), PxVec3(0, 1, 0) } });
		job.groups.push_back(group);
		job.report.objWritten = true;
	}
	return job;
}
}

TEST(WriteDumpWritesReportAndObj)
{
	const auto folder = std::filesystem::temp_directory_path() / "cpmc-physx-probe-test" / "with-geometry";
	std::filesystem::remove_all(folder);
	const cpmc::WriteResult result = cpmc::WriteDump(MakeJob(folder, true));
	CHECK(result.ok);
	CHECK_EQ(result.triangles, uint64_t(1));
	CHECK(ReadFile(folder / "report.json").starts_with("{\n"));
	CHECK(ReadFile(folder / "collision.obj").find("o static_box\n") != std::string::npos);
	CHECK(ReadFile(folder / "collision.mtl").starts_with("newmtl static_box\n"));
	std::filesystem::remove_all(folder.parent_path());
}

TEST(WriteDumpWithoutGeometryWritesOnlyTheReport)
{
	const auto folder = std::filesystem::temp_directory_path() / "cpmc-physx-probe-test" / "no-geometry";
	std::filesystem::remove_all(folder);
	const cpmc::WriteResult result = cpmc::WriteDump(MakeJob(folder, false));
	CHECK(result.ok);
	CHECK(std::filesystem::exists(folder / "report.json"));
	CHECK(!std::filesystem::exists(folder / "collision.obj"));
	std::filesystem::remove_all(folder.parent_path());
}

TEST(DumpWriterRunsOneJobAtATime)
{
	const auto       folder = std::filesystem::temp_directory_path() / "cpmc-physx-probe-test" / "threaded";
	cpmc::DumpWriter writer;
	CHECK(writer.Start(MakeJob(folder, true)));
	writer.Join();
	CHECK(!writer.Busy());
	const auto result = writer.TakeResult();
	CHECK(result.has_value() && result->ok);
	CHECK(!writer.TakeResult().has_value());  // taken once
	std::filesystem::remove_all(folder.parent_path());
}
