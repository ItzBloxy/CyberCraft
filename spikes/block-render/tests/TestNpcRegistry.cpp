#include "Harness.h"

#include "NpcRegistry.h"

#include <thread>

namespace
{
std::vector<uint64_t> Copied(cpmc::NpcRegistry& aRegistry)
{
	std::vector<uint64_t> ids;
	const int32_t         count = aRegistry.Copy();
	for (int32_t i = 0; i < count; ++i)
	{
		ids.push_back(aRegistry.At(i));
	}
	return ids;
}
}

TEST(NpcRegistryKeepsIdsInAttachOrder)
{
	cpmc::NpcRegistry registry;
	registry.Attached(5, 1);
	registry.Attached(3, 1);
	registry.Attached(9, 1);
	CHECK(Copied(registry) == std::vector<uint64_t>({ 5, 3, 9 }));
	CHECK_EQ(registry.At(3), uint64_t(0));
	CHECK_EQ(registry.At(-1), uint64_t(0));
}

TEST(NpcRegistryDetachKeepsTheOthersInOrder)
{
	cpmc::NpcRegistry registry;
	for (uint64_t id = 1; id <= 5; ++id)
	{
		registry.Attached(id, 1);
	}
	registry.Detached(2, 1);
	registry.Detached(4, 1);
	registry.Attached(2, 1);  // back: at the end
	CHECK(Copied(registry) == std::vector<uint64_t>({ 1, 3, 5, 2 }));
}

TEST(NpcRegistryCountsRepeatsAndStrays)
{
	cpmc::NpcRegistry registry;
	registry.Attached(7, 1);
	registry.Attached(8, 1);
	registry.Attached(7, 1);  // already there: stays first
	registry.Detached(9, 1);
	registry.Detached(8, 1);
	registry.Detached(8, 1);
	CHECK(Copied(registry) == std::vector<uint64_t>({ 7 }));
	const cpmc::NpcRegistry::Counts counts = registry.Count();
	CHECK_EQ(counts.ids, size_t(1));
	CHECK_EQ(counts.attached, uint64_t(3));
	CHECK_EQ(counts.detached, uint64_t(3));
	CHECK_EQ(counts.repeats, uint64_t(1));
	CHECK_EQ(counts.strays, uint64_t(2));
	CHECK_EQ(counts.full, uint64_t(0));
}

TEST(NpcRegistryCopyStaysUntilTheNextCopy)
{
	cpmc::NpcRegistry registry;
	registry.Attached(1, 1);
	registry.Attached(2, 1);
	CHECK_EQ(registry.Copy(), 2);
	registry.Detached(1, 1);
	registry.Attached(3, 1);
	CHECK_EQ(registry.At(0), uint64_t(1));
	CHECK_EQ(registry.At(1), uint64_t(2));
	CHECK_EQ(registry.At(2), uint64_t(0));
	CHECK(Copied(registry) == std::vector<uint64_t>({ 2, 3 }));
}

TEST(NpcRegistryKeepsAtMostItsMaximum)
{
	cpmc::NpcRegistry registry;
	for (uint64_t id = 1; id <= cpmc::NpcRegistry::kMaxIds + 3; ++id)
	{
		registry.Attached(id, 1);
	}
	const cpmc::NpcRegistry::Counts counts = registry.Count();
	CHECK_EQ(counts.ids, cpmc::NpcRegistry::kMaxIds);
	CHECK_EQ(counts.full, uint64_t(3));
	CHECK_EQ(registry.Copy(), int32_t(cpmc::NpcRegistry::kMaxIds));
	CHECK_EQ(registry.At(int32_t(cpmc::NpcRegistry::kMaxIds) - 1), uint64_t(cpmc::NpcRegistry::kMaxIds));
}

TEST(NpcRegistryCountsCallsByThread)
{
	cpmc::NpcRegistry registry;
	registry.Attached(1, 10);
	registry.Attached(2, 10);
	registry.Detached(1, 20);
	const std::vector<cpmc::NpcRegistry::ThreadCalls> threads = registry.Threads();
	CHECK_EQ(threads.size(), size_t(2));
	CHECK(threads.size() == 2 && threads[0].thread == 10 && threads[0].attached == 2 && threads[0].detached == 0);
	CHECK(threads.size() == 2 && threads[1].thread == 20 && threads[1].attached == 0 && threads[1].detached == 1);
	CHECK_STR(cpmc::DescribeThreads(threads, 10), "10, the main thread: 2 attached, 0 detached; 20: 0 attached, 1 detached");
	CHECK_STR(cpmc::DescribeThreads({}, 10), "none");
}

TEST(NpcRegistryGathersThreadsPastItsMaximum)
{
	cpmc::NpcRegistry registry;
	const uint32_t    threadCount = uint32_t(cpmc::NpcRegistry::kMaxThreads) + 5;
	for (uint32_t thread = 1; thread <= threadCount; ++thread)
	{
		registry.Attached(thread, thread);
	}
	const std::vector<cpmc::NpcRegistry::ThreadCalls> threads = registry.Threads();
	CHECK_EQ(threads.size(), cpmc::NpcRegistry::kMaxThreads);
	uint64_t attached = 0;
	for (const cpmc::NpcRegistry::ThreadCalls& calls : threads)
	{
		attached += calls.attached;
	}
	CHECK_EQ(attached, uint64_t(threadCount));
	CHECK(!threads.empty() && threads.back().thread == 0 && threads.back().attached == threadCount - (cpmc::NpcRegistry::kMaxThreads - 1));
	CHECK(cpmc::DescribeThreads(threads, 1).ends_with("other threads: 6 attached, 0 detached"));
}

// As the game does: wraps on several threads at once, while the main thread copies and reads the list.
TEST(NpcRegistryTakesCallsFromManyThreadsAtOnce)
{
	constexpr uint64_t kThreads = 8;
	constexpr uint64_t kIdsEach = 100;
	constexpr int      kRounds = 50;
	cpmc::NpcRegistry  registry;
	std::vector<std::thread> threads;
	for (uint64_t t = 0; t < kThreads; ++t)
	{
		// Each thread's own IDs: every round attaches them all and detaches the even ones, so the odd ones stay.
		threads.emplace_back([&registry, t] {
			for (int round = 0; round < kRounds; ++round)
			{
				for (uint64_t i = 0; i < kIdsEach; ++i)
				{
					registry.Attached(1000 * (t + 1) + i, uint32_t(t + 1));
				}
				for (uint64_t i = 0; i < kIdsEach; i += 2)
				{
					registry.Detached(1000 * (t + 1) + i, uint32_t(t + 1));
				}
			}
		});
	}
	bool readsFine = true;
	for (int read = 0; read < 200; ++read)
	{
		const int32_t count = registry.Copy();
		for (int32_t i = 0; i < count; ++i)
		{
			const uint64_t id = registry.At(i);
			readsFine = readsFine && id >= 1000 && id < 1000 * (kThreads + 1) && id % 1000 < kIdsEach;
		}
	}
	for (std::thread& thread : threads)
	{
		thread.join();
	}
	CHECK(readsFine);
	const std::vector<uint64_t> ids = Copied(registry);
	CHECK_EQ(ids.size(), size_t(kThreads * kIdsEach / 2));
	for (uint64_t t = 0; t < kThreads; ++t)
	{
		// This thread's odd IDs, each once, in the order attached.
		std::vector<uint64_t> own;
		for (const uint64_t id : ids)
		{
			if (id / 1000 == t + 1)
			{
				own.push_back(id);
			}
		}
		std::vector<uint64_t> expected;
		for (uint64_t i = 1; i < kIdsEach; i += 2)
		{
			expected.push_back(1000 * (t + 1) + i);
		}
		CHECK(own == expected);
	}
	const cpmc::NpcRegistry::Counts counts = registry.Count();
	CHECK_EQ(counts.attached, kThreads * kRounds * kIdsEach);
	CHECK_EQ(counts.detached, kThreads * kRounds * kIdsEach / 2);
	CHECK_EQ(counts.repeats, kThreads * (kRounds - 1) * kIdsEach / 2);
	CHECK_EQ(counts.strays, uint64_t(0));
	CHECK_EQ(registry.Threads().size(), size_t(kThreads));
}
