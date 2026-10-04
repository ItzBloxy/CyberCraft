#include "Harness.h"

#include "link/GroundWorker.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

using namespace cpmc::link;

namespace
{
// Takes the worker's messages until aCount have come, or 5 s have gone.
std::vector<GroundWorker::Message> TakeAll(GroundWorker& aWorker, size_t aCount)
{
	std::vector<GroundWorker::Message> messages;
	for (int i = 0; i < 5000 && messages.size() < aCount; ++i)
	{
		for (GroundWorker::Message& message : aWorker.Take())
		{
			messages.push_back(std::move(message));
		}
		if (messages.size() < aCount)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
	return messages;
}

// A flat floor over the region's 8 x 8 blocks, at height aY.
std::vector<McTriangle> Floor(const RegionPos& aRegion, float aY)
{
	const float x = float(aRegion.x * kRegionBlocks), z = float(aRegion.z * kRegionBlocks);
	return { { { x, aY, z, x + 8, aY, z, x + 8, aY, z + 8 } }, { { x, aY, z, x + 8, aY, z + 8, x, aY, z + 8 } } };
}

// Dense ground: the floor's two triangles 75 times over, which takes the worker about a tenth of a second.
std::vector<McTriangle> Dense(const RegionPos& aRegion)
{
	std::vector<McTriangle> triangles;
	for (int i = 0; i < 75; ++i)
	{
		for (const McTriangle& triangle : Floor(aRegion, 3.0f))
		{
			triangles.push_back(triangle);
		}
	}
	return triangles;
}

proto::ColRegion HeaderOf(const GroundWorker::Message& aMessage)
{
	proto::ColRegion header{};
	std::memcpy(&header, aMessage.payload.data(), sizeof header);
	return header;
}

// Waits up to 5 s for a job to be under way.
bool UnderWay(const GroundWorker& aWorker)
{
	for (int i = 0; i < 5000 && aWorker.Busy() == 0; ++i)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return aWorker.Busy() > 0;
}
}

TEST(GroundWorkerMakesEachRegionsMessages)
{
	GroundWorker                 worker(1);
	const std::vector<RegionPos> regions = { { -15, 0, 250 }, { -14, 0, 250 }, { -15, -1, 250 } };
	std::vector<GroundWorker::Job> jobs;
	for (const RegionPos& region : regions)
	{
		jobs.push_back({ 3, region, region.y == 0 ? Floor(region, 6.14f) : std::vector<McTriangle>() });
	}
	worker.Add(std::move(jobs));
	CHECK_EQ(worker.Waiting(), size_t(3));
	const auto messages = TakeAll(worker, 6);
	CHECK_EQ(messages.size(), size_t(6));
	CHECK_EQ(worker.Waiting(), size_t(0));  // every job's messages taken
	for (size_t i = 0; i < regions.size() && messages.size() == 6; ++i)
	{
		// Each region's triangles, then its voxels, in the order they came: there's no focus.
		const RegionPos&              r = regions[i];
		const std::vector<McTriangle> triangles = r.y == 0 ? Floor(r, 6.14f) : std::vector<McTriangle>();
		CHECK_EQ(messages[2 * i].type, uint32_t(proto::kColTris));
		CHECK(messages[2 * i].payload == TrisPayload(r.x, r.y, r.z, 3, triangles));
		CHECK_EQ(messages[2 * i + 1].type, uint32_t(proto::kColRegion));
		CHECK(messages[2 * i + 1].payload == RegionPayload(r.x, r.y, r.z, 3, VoxelizeRegion(r.x, r.y, r.z, triangles)));
		CHECK_EQ(messages[2 * i + 1].epoch, 3u);
	}
	CHECK(worker.Take().empty());  // taken already
	CHECK_EQ(worker.Regions(), 3ull);
	CHECK_EQ(worker.Triangles(), 4ull);  // the two floors' two each
	CHECK_EQ(worker.Blocks(), 128ull);   // and their 64 blocks each
	CHECK_EQ(worker.Failed(), 0ull);
	CHECK(worker.Ms() > 0.0);
	CHECK(worker.Cycles() > 0 && worker.Ticks() > 0);  // the thread's own time, against the clock's
}

TEST(GroundWorkerStartsTheJobNearestItsFocus)
{
	// Minecraft's player is at region 49 0 0: of the 50 regions given together, the worker starts there and works back.
	GroundWorker worker(1);
	worker.Focus({ 49, 0, 0 });
	std::vector<GroundWorker::Job> jobs;
	for (int32_t x = 0; x < 50; ++x)
	{
		jobs.push_back({ 1, { x, 0, 0 }, Floor({ x, 0, 0 }, 3.0f) });
	}
	worker.Add(std::move(jobs));
	const auto messages = TakeAll(worker, 100);
	CHECK_EQ(messages.size(), size_t(100));
	for (size_t i = 0; i < messages.size(); ++i)
	{
		CHECK_EQ(HeaderOf(messages[i]).minX, (49 - int32_t(i / 2)) * 8);
	}
}

TEST(GroundWorkerReplacesAJobNotStarted)
{
	// While the one thread voxelizes dense ground, a region is given twice: the newer job replaces the older, which was
	// still waiting, so only the newer triangles go out.
	GroundWorker worker(1);
	worker.Add({ { 1, { 0, 0, 0 }, Dense({ 0, 0, 0 }) } });
	CHECK(UnderWay(worker));
	worker.Add({ { 1, { 5, 0, 0 }, Floor({ 5, 0, 0 }, 3.0f) } });
	worker.Add({ { 1, { 5, 0, 0 }, Floor({ 5, 0, 0 }, 4.0f) } });
	CHECK_EQ(worker.Replaced(), 1ull);
	CHECK_EQ(worker.Waiting(), size_t(2));
	const auto messages = TakeAll(worker, 4);
	CHECK_EQ(messages.size(), size_t(4));
	if (messages.size() == 4)
	{
		CHECK_EQ(HeaderOf(messages[0]).minX, 0);
		CHECK(messages[2].payload == TrisPayload(5, 0, 0, 1, Floor({ 5, 0, 0 }, 4.0f)));
	}
	CHECK(worker.Take().empty());  // nothing more: the replaced job was never made
	CHECK_EQ(worker.Waiting(), size_t(0));
	CHECK_EQ(worker.Regions(), 2ull);
}

TEST(GroundWorkerKeepsARegionsJobsInOrder)
{
	// Two threads: a region's newer job doesn't start while its older one is under way, so the older one's messages,
	// though slower to make, can't come after it and undo it.
	GroundWorker worker(2);
	worker.Add({ { 1, { 0, 0, 0 }, Dense({ 0, 0, 0 }) } });
	CHECK(UnderWay(worker));
	worker.Add({ { 1, { 0, 0, 0 }, Floor({ 0, 0, 0 }, 4.0f) } });
	const auto messages = TakeAll(worker, 4);
	CHECK_EQ(messages.size(), size_t(4));
	if (messages.size() == 4)
	{
		CHECK_EQ(HeaderOf(messages[0]).count, 150u);  // the dense ground's triangles first
		CHECK(messages[2].payload == TrisPayload(0, 0, 0, 1, Floor({ 0, 0, 0 }, 4.0f)));
	}
	CHECK_EQ(worker.Replaced(), 0ull);
}

TEST(GroundWorkerRunsSeveralThreads)
{
	CHECK(GroundWorker::DefaultThreads() >= 1 && GroundWorker::DefaultThreads() <= 4);
	GroundWorker worker(3);
	CHECK_EQ(worker.Threads(), 3u);
	std::vector<GroundWorker::Job> jobs;
	for (int32_t x = 0; x < 30; ++x)
	{
		jobs.push_back({ 1, { x, 0, 0 }, Floor({ x, 0, 0 }, 3.0f) });
	}
	worker.Add(std::move(jobs));
	const auto messages = TakeAll(worker, 60);
	CHECK_EQ(messages.size(), size_t(60));
	std::vector<bool> seen(30, false);
	for (size_t i = 0; i + 1 < messages.size(); i += 2)
	{
		// Each region's two messages stay together, whichever thread made them.
		const int32_t x = HeaderOf(messages[i]).minX / 8;
		CHECK(messages[i].type == proto::kColTris && messages[i + 1].type == proto::kColRegion);
		CHECK_EQ(HeaderOf(messages[i + 1]).minX, x * 8);
		if (x >= 0 && x < 30)
		{
			seen[size_t(x)] = true;
		}
	}
	CHECK(std::all_of(seen.begin(), seen.end(), [](bool aSeen) { return aSeen; }));
	CHECK_EQ(worker.Waiting(), size_t(0));
}

TEST(GroundWorkerStopsWithJobsLeft)
{
	// Destroyed with 2,000 regions waiting, about two seconds of work for its one thread: it drops them, rather than
	// finishing them first.
	const auto start = std::chrono::steady_clock::now();
	{
		GroundWorker                   worker(1);
		std::vector<GroundWorker::Job> jobs;
		for (int32_t x = 0; x < 2000; ++x)
		{
			jobs.push_back({ 1, { x, 0, 0 }, Floor({ x, 0, 0 }, 3.0f) });
		}
		worker.Add(std::move(jobs));
	}
	CHECK(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500));
}
