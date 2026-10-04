#include "link/GroundWorker.h"

#include <Windows.h>
#include <intrin.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <utility>

namespace cpmc::link
{
namespace
{
// SkyCraft's distance between regions, for its order: height counts double.
int64_t Distance(const RegionPos& aRegion, const RegionPos& aFocus)
{
	const int64_t dx = int64_t(aRegion.x) - aFocus.x, dy = int64_t(aRegion.y) - aFocus.y, dz = int64_t(aRegion.z) - aFocus.z;
	return dx * dx + dz * dz + 2 * dy * dy;
}

uint64_t ThreadCycles()
{
	ULONG64 cycles = 0;
	QueryThreadCycleTime(GetCurrentThread(), &cycles);
	return cycles;
}
}

unsigned GroundWorker::DefaultThreads()
{
	return std::clamp(std::thread::hardware_concurrency() / 4, 1u, 4u);
}

GroundWorker::GroundWorker(unsigned aThreads)
{
	for (unsigned i = 0; i < std::max(aThreads, 1u); ++i)
	{
		threads_.emplace_back([this] { Run(); });
	}
}

GroundWorker::~GroundWorker()
{
	{
		std::lock_guard lock(mutex_);
		stop_ = true;
	}
	wake_.notify_all();
	for (std::thread& thread : threads_)
	{
		thread.join();
	}
}

void GroundWorker::Add(std::vector<Job> aJobs)
{
	{
		std::lock_guard lock(mutex_);
		for (Job& job : aJobs)
		{
			const auto older = std::find_if(jobs_.begin(), jobs_.end(), [&](const Job& aJob) { return aJob.region == job.region; });
			if (older != jobs_.end())
			{
				*older = std::move(job);  // in its place: the older one would only be undone by this
				++replaced_;
				continue;
			}
			jobs_.push_back(std::move(job));
			++inHand_;
		}
	}
	wake_.notify_all();
}

void GroundWorker::Focus(const RegionPos& aRegion)
{
	std::lock_guard lock(mutex_);
	focus_ = aRegion;
}

std::vector<GroundWorker::Message> GroundWorker::Take()
{
	std::unique_lock lock(mutex_, std::try_to_lock);
	if (!lock.owns_lock())
	{
		return {};  // a thread has the jobs for a moment: they come at the next call
	}
	inHand_ -= finished_;
	finished_ = 0;
	return std::exchange(done_, {});
}

std::deque<GroundWorker::Job>::iterator GroundWorker::Next()
{
	auto    best = jobs_.end();
	int64_t nearest = 0;
	for (auto job = jobs_.begin(); job != jobs_.end(); ++job)
	{
		if (std::find(underWay_.begin(), underWay_.end(), job->region) != underWay_.end())
		{
			continue;  // its region's older job first
		}
		if (!focus_)
		{
			return job;  // in the order they came
		}
		const int64_t distance = Distance(job->region, *focus_);
		if (best == jobs_.end() || distance < nearest)
		{
			best = job;
			nearest = distance;
		}
	}
	return best;
}

void GroundWorker::Run()
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);  // the game's own threads come first
	std::unique_lock lock(mutex_);
	while (true)
	{
		auto next = jobs_.end();
		wake_.wait(lock, [&] { return stop_ || (next = Next()) != jobs_.end(); });
		if (stop_)
		{
			return;
		}
		const Job job = std::move(*next);
		jobs_.erase(next);
		underWay_.push_back(job.region);
		++busy_;
		lock.unlock();
		const auto     start = std::chrono::steady_clock::now();
		const uint64_t cyclesBefore = ThreadCycles();
		const uint64_t ticksBefore = __rdtsc();
		std::vector<Message> messages;
		size_t               blocks = 0;
		try
		{
			const RegionPos&                   at = job.region;
			const std::vector<proto::ColBlock> voxels = VoxelizeRegion(at.x, at.y, at.z, job.triangles);
			blocks = voxels.size();
			messages.push_back({ job.epoch, proto::kColTris, TrisPayload(at.x, at.y, at.z, job.epoch, job.triangles) });
			messages.push_back({ job.epoch, proto::kColRegion, RegionPayload(at.x, at.y, at.z, job.epoch, voxels) });
		}
		catch (const std::exception&)
		{
			messages.clear();  // out of memory, say: the region is left out, rather than the game ended
		}
		const uint64_t cycles = ThreadCycles() - cyclesBefore;
		const uint64_t ticks = __rdtsc() - ticksBefore;
		const double   ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		lock.lock();
		underWay_.erase(std::find(underWay_.begin(), underWay_.end(), job.region));
		--busy_;
		++finished_;
		ms_ += ms;
		cycles_ += cycles;
		ticks_ += ticks;
		if (messages.empty())
		{
			++failed_;
		}
		else
		{
			for (Message& message : messages)
			{
				done_.push_back(std::move(message));
			}
			++regions_;
			triangles_ += job.triangles.size();
			blocks_ += blocks;
		}
		wake_.notify_all();  // the region is free: a newer job of it may start
	}
}
}
