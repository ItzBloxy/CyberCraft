#include "Harness.h"

#include "link/GroundWorker.h"

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
}

TEST(GroundWorkerMakesEachRegionsMessages)
{
	GroundWorker                 worker;
	const std::vector<RegionPos> regions = { { -15, 0, 250 }, { -14, 0, 250 }, { -15, -1, 250 } };
	for (const RegionPos& region : regions)
	{
		worker.Add({ 3, region, region.y == 0 ? Floor(region, 6.14f) : std::vector<McTriangle>() });
	}
	CHECK_EQ(worker.Waiting(), size_t(3));
	const auto messages = TakeAll(worker, 6);
	CHECK_EQ(messages.size(), size_t(6));
	CHECK_EQ(worker.Waiting(), size_t(0));  // every job's messages taken
	for (size_t i = 0; i < regions.size() && messages.size() == 6; ++i)
	{
		// Each region's triangles, then its voxels.
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
	CHECK_EQ(worker.Blocks(), 128ull);  // the two floors' 64 blocks each
	CHECK_EQ(worker.Failed(), 0ull);
	CHECK(worker.Ms() > 0.0);
}

TEST(GroundWorkerKeepsTheOrderOfItsJobs)
{
	GroundWorker worker;
	for (int32_t x = 0; x < 50; ++x)
	{
		worker.Add({ 1, { x, 0, 0 }, Floor({ x, 0, 0 }, 3.0f) });
	}
	const auto messages = TakeAll(worker, 100);
	CHECK_EQ(messages.size(), size_t(100));
	for (size_t i = 0; i < messages.size(); ++i)
	{
		proto::ColRegion header;
		std::memcpy(&header, messages[i].payload.data(), sizeof header);
		CHECK_EQ(header.minX, int32_t(i / 2) * 8);
	}
}

TEST(GroundWorkerStopsWithJobsLeft)
{
	// Destroyed with 2,000 regions waiting, about two seconds of work: it drops them, rather than finishing them first.
	const auto start = std::chrono::steady_clock::now();
	{
		GroundWorker worker;
		for (int32_t x = 0; x < 2000; ++x)
		{
			worker.Add({ 1, { x, 0, 0 }, Floor({ x, 0, 0 }, 3.0f) });
		}
	}
	CHECK(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500));
}
