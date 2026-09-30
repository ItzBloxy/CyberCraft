#pragma once

#include "ObjWriter.h"
#include "Report.h"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace cpmc
{
// Everything one dump writes: the folder, the report, and the triangles (world space) by category.
struct DumpJob
{
	std::filesystem::path folder;
	DumpReport            report;
	std::vector<ObjGroup> groups;
	Vec3d                 origin;  // V's feet: OBJ coordinates are relative to this
};

struct WriteResult
{
	std::filesystem::path folder;
	bool                  ok = false;
	std::string           error;
	uint64_t              triangles = 0;
	double                writeMs = 0.0;
};

// Writes report.json, and collision.obj/.mtl when there are groups, into aJob.folder. Never throws.
WriteResult WriteDump(const DumpJob& aJob);

// Writes dumps on a background thread. That thread never calls PhysX, the game or RED4ext.
class DumpWriter
{
public:
	~DumpWriter();

	bool Busy() const { return busy_.load(); }
	// Starts writing aJob. Returns false, and does nothing, while the previous dump is still being written.
	bool Start(DumpJob aJob);
	// The result of the last finished write, once.
	std::optional<WriteResult> TakeResult();
	// Waits for a running write to finish.
	void Join();

private:
	std::thread                thread_;
	std::atomic<bool>          busy_{ false };
	std::mutex                 mutex_;
	std::optional<WriteResult> result_;
};
}
