#include "link/GroundWorker.h"

#include <chrono>
#include <exception>
#include <utility>

namespace cpmc::link
{
GroundWorker::GroundWorker() : thread_([this] { Run(); })
{
}

GroundWorker::~GroundWorker()
{
	{
		std::lock_guard lock(mutex_);
		stop_ = true;
	}
	wake_.notify_all();
	thread_.join();
}

void GroundWorker::Add(Job aJob)
{
	{
		std::lock_guard lock(mutex_);
		jobs_.push_back(std::move(aJob));
		++inHand_;
	}
	wake_.notify_one();
}

std::vector<GroundWorker::Message> GroundWorker::Take()
{
	std::lock_guard lock(mutex_);
	inHand_ -= finished_;
	finished_ = 0;
	return std::exchange(done_, {});
}

size_t GroundWorker::Waiting() const
{
	std::lock_guard lock(mutex_);
	return inHand_;
}

uint64_t GroundWorker::Regions() const
{
	std::lock_guard lock(mutex_);
	return regions_;
}

uint64_t GroundWorker::Blocks() const
{
	std::lock_guard lock(mutex_);
	return blocks_;
}

uint64_t GroundWorker::Failed() const
{
	std::lock_guard lock(mutex_);
	return failed_;
}

double GroundWorker::Ms() const
{
	std::lock_guard lock(mutex_);
	return ms_;
}

void GroundWorker::Run()
{
	std::unique_lock lock(mutex_);
	while (true)
	{
		wake_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
		if (stop_)
		{
			return;
		}
		const Job job = std::move(jobs_.front());
		jobs_.pop_front();
		lock.unlock();
		const auto start = std::chrono::steady_clock::now();
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
		const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		lock.lock();
		++finished_;
		ms_ += ms;
		if (messages.empty())
		{
			++failed_;
			continue;
		}
		for (Message& message : messages)
		{
			done_.push_back(std::move(message));
		}
		++regions_;
		blocks_ += blocks;
	}
}
}
