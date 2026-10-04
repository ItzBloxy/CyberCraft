#pragma once

#include "link/Ground.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

// The ground's worker (spike 5's "The worker"): threads voxelize each region they're given, with SkyCraft's
// triangle-box test, and make its messages, the triangles (kColTris) then the voxels (kColRegion), as SkyCraft's worker
// sends them. Voxelizing takes up to a few milliseconds a region, too long for the game's main thread, which only reads
// the triangles and writes the finished messages into the ring.
//
// A fast drop into dense ground once queued 1,500 regions for a single thread, and Minecraft's player fell through the
// ground (Part 2's follow-up). So several threads, below normal priority so the game's own threads come first, start the
// job nearest the focus (Minecraft's player) first. A region's newer job replaces one that hasn't started, and doesn't
// start while an older one of its region is under way, so the older one's messages can't come after it.
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

	static unsigned DefaultThreads();  // a quarter of the processors, from 1 to 4

	explicit GroundWorker(unsigned aThreads = DefaultThreads());
	~GroundWorker();  // drops the jobs not started, and waits for those under way
	GroundWorker(const GroundWorker&) = delete;
	GroundWorker& operator=(const GroundWorker&) = delete;

	// Jobs, added together. Each replaces a job of its region that hasn't started: its triangles are newer.
	void Add(std::vector<Job> aJobs);
	// Where the regions are wanted first: the job nearest it, as SkyCraft orders regions (height counting double), starts
	// next. Until it's set, jobs start in the order they came.
	void Focus(const RegionPos& aRegion);
	// The messages finished since the last call: each region's two together, in the order the regions finished. Nothing
	// while a thread holds the jobs for a moment: the main thread never waits, and they come at the next call.
	std::vector<Message> Take();

	unsigned Threads() const { return unsigned(threads_.size()); }
	size_t   Waiting() const { return inHand_.load(); }  // jobs added whose messages haven't been taken yet
	size_t   Busy() const { return busy_.load(); }       // jobs under way now
	uint64_t Regions() const { return regions_.load(); }      // regions voxelized, since the start
	uint64_t Triangles() const { return triangles_.load(); }  // their triangles
	uint64_t Blocks() const { return blocks_.load(); }        // voxel blocks made
	uint64_t Failed() const { return failed_.load(); }        // regions left out, for want of memory
	uint64_t Replaced() const { return replaced_.load(); }    // jobs replaced by a newer one of their region
	double   Ms() const { return ms_.load(); }                // time spent voxelizing and making messages, all threads
	// Meanwhile, the threads' own CPU cycles (QueryThreadCycleTime), and the processor clock's (rdtsc): Cycles over Ticks
	// is the share of that time they were running, so whether other threads kept them waiting.
	uint64_t Cycles() const { return cycles_.load(); }
	uint64_t Ticks() const { return ticks_.load(); }

private:
	void Run();
	// The job to start next: the nearest the focus whose region has no job under way; the end if none can start.
	std::deque<Job>::iterator Next();

	mutable std::mutex       mutex_;
	std::condition_variable  wake_;
	std::deque<Job>          jobs_;
	std::vector<RegionPos>   underWay_;  // the regions of the jobs under way
	std::vector<Message>     done_;
	size_t                   finished_ = 0;  // jobs finished since the last Take: their messages are in done_, or they failed
	std::optional<RegionPos> focus_;
	bool                     stop_ = false;
	std::atomic<size_t>      inHand_{ 0 };
	std::atomic<size_t>      busy_{ 0 };
	std::atomic<uint64_t>    regions_{ 0 };
	std::atomic<uint64_t>    triangles_{ 0 };
	std::atomic<uint64_t>    blocks_{ 0 };
	std::atomic<uint64_t>    failed_{ 0 };
	std::atomic<uint64_t>    replaced_{ 0 };
	std::atomic<double>      ms_{ 0.0 };
	std::atomic<uint64_t>    cycles_{ 0 };
	std::atomic<uint64_t>    ticks_{ 0 };
	std::vector<std::thread> threads_;  // last: they start once the rest is made
};
}
