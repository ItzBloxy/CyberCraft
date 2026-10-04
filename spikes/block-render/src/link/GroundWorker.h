#pragma once

#include "link/Ground.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

// The ground's worker (spike 5's "The worker"): one thread voxelizes each region it's given, with SkyCraft's
// triangle-box test, and makes its messages, the triangles (kColTris) then the voxels (kColRegion), as SkyCraft's worker
// sends them. Voxelizing takes up to a few milliseconds a region, too long for the game's main thread, which only reads
// the triangles and writes the finished messages into the ring, in order.
namespace cpmc::link
{
class GroundWorker
{
public:
	struct Job
	{
		uint32_t                epoch = 0;
		RegionPos               region;
		std::vector<McTriangle> triangles;  // filed under the region (FileBatch)
	};

	struct Message
	{
		uint32_t             epoch = 0;
		uint32_t             type = 0;  // kColTris or kColRegion
		std::vector<uint8_t> payload;
	};

	GroundWorker();
	~GroundWorker();  // drops the jobs not started, and waits for the one under way
	GroundWorker(const GroundWorker&) = delete;
	GroundWorker& operator=(const GroundWorker&) = delete;

	void Add(Job aJob);
	// The messages finished since the last call, in the order of their jobs.
	std::vector<Message> Take();

	size_t   Waiting() const;  // jobs added whose messages haven't been taken yet
	uint64_t Regions() const;  // regions voxelized, since the start
	uint64_t Blocks() const;   // voxel blocks made
	uint64_t Failed() const;   // regions left out, for want of memory
	double   Ms() const;       // time spent voxelizing and making messages

private:
	void Run();

	mutable std::mutex      mutex_;
	std::condition_variable wake_;
	std::deque<Job>         jobs_;
	std::vector<Message>    done_;
	size_t                  inHand_ = 0;    // jobs added whose messages haven't been taken
	size_t                  finished_ = 0;  // of those, the jobs whose messages are in done_ (none for a failed one)
	bool                    stop_ = false;
	uint64_t                regions_ = 0;
	uint64_t                blocks_ = 0;
	uint64_t                failed_ = 0;
	double                  ms_ = 0.0;
	std::thread             thread_;  // last: it starts once the rest is made
};
}
