#include "DumpWriter.h"

#include "ReportJson.h"

#include <chrono>
#include <fstream>
#include <format>
#include <stdexcept>
#include <utility>

namespace cpmc
{
WriteResult WriteDump(const DumpJob& aJob)
{
	WriteResult result;
	result.folder = aJob.folder;
	const auto start = std::chrono::steady_clock::now();
	try
	{
		std::filesystem::create_directories(aJob.folder);
		if (!aJob.groups.empty())
		{
			std::ofstream obj(aJob.folder / "collision.obj", std::ios::binary);
			const std::string comment = std::format("V's feet (world, metres, Z up): {:.3f} {:.3f} {:.3f}; axes: Minecraft (Y up)",
			                                        aJob.origin.x, aJob.origin.y, aJob.origin.z);
			WriteObj(obj, aJob.groups, aJob.origin, "collision.mtl", comment);
			obj.close();
			std::ofstream mtl(aJob.folder / "collision.mtl", std::ios::binary);
			WriteMtl(mtl, aJob.groups);
			mtl.close();
			if (obj.fail() || mtl.fail())
			{
				throw std::runtime_error("couldn't write collision.obj or collision.mtl");
			}
			for (const ObjGroup& group : aJob.groups)
			{
				result.triangles += group.triangles.size();
			}
		}
		std::ofstream report(aJob.folder / "report.json", std::ios::binary);
		report << ReportToJson(aJob.report);
		report.close();
		if (report.fail())
		{
			throw std::runtime_error("couldn't write report.json");
		}
		result.ok = true;
	}
	catch (const std::exception& e)
	{
		result.error = e.what();
	}
	result.writeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	return result;
}

DumpWriter::~DumpWriter()
{
	Join();
}

bool DumpWriter::Start(DumpJob aJob)
{
	if (busy_.load())
	{
		return false;
	}
	Join();  // the previous thread has finished; reclaim it
	busy_ = true;
	thread_ = std::thread([this, job = std::move(aJob)] {
		WriteResult result = WriteDump(job);
		{
			std::scoped_lock lock(mutex_);
			result_ = std::move(result);
		}
		busy_ = false;
	});
	return true;
}

std::optional<WriteResult> DumpWriter::TakeResult()
{
	std::scoped_lock lock(mutex_);
	return std::exchange(result_, std::nullopt);
}

void DumpWriter::Join()
{
	if (thread_.joinable())
	{
		thread_.join();
	}
}
}
