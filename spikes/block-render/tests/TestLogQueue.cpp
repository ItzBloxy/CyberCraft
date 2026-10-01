#include "Harness.h"

#include "LogQueue.h"

#include <thread>

TEST(LogQueueDrainsInOrder)
{
	cpmc::LogQueue queue;
	queue.Push("one");
	queue.Push("two");
	const auto lines = queue.Drain();
	CHECK_EQ(lines.size(), size_t(2));
	CHECK_STR(lines[0], "one");
	CHECK_STR(lines[1], "two");
	CHECK(queue.Drain().empty());
}

TEST(LogQueueCountsWhatItDrops)
{
	cpmc::LogQueue queue(2);
	queue.Push("a");
	queue.Push("b");
	queue.Push("c");
	queue.Push("d");
	const auto lines = queue.Drain();
	CHECK_EQ(lines.size(), size_t(3));
	CHECK_STR(lines[2], "(2 more log lines were dropped)");
	queue.Push("e");
	CHECK_EQ(queue.Drain().size(), size_t(1));
}

TEST(LogQueueTakesLinesFromManyThreads)
{
	cpmc::LogQueue           queue;
	std::vector<std::thread> threads;
	for (int t = 0; t < 4; ++t)
	{
		threads.emplace_back([&queue] {
			for (int i = 0; i < 100; ++i)
			{
				queue.Push("line");
			}
		});
	}
	for (auto& thread : threads)
	{
		thread.join();
	}
	CHECK_EQ(queue.Drain().size(), size_t(400));
}
