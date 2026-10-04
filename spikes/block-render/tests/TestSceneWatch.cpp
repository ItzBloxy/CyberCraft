#include "Harness.h"

#include "physx/SceneWatch.h"

#include <thread>
#include <vector>

using namespace cpmc::px;

TEST(SceneWatchCountsEachThreadsCalls)
{
	SceneRecorder recorder;
	recorder.Start();
	recorder.Note(SceneCall::Simulate, 100, false);
	recorder.Note(SceneCall::FetchResults, 100, false);
	recorder.Note(SceneCall::AddActor, 200, true);  // while OnUpdate ran, without the write lock
	recorder.Lock(200, 1);
	recorder.Note(SceneCall::AddActor, 200, true);  // with it
	recorder.Note(SceneCall::RemoveActor, 200, false);
	recorder.Lock(200, -1);
	recorder.Note(SceneCall::AddActors, 200, true);
	const auto threads = recorder.Threads();
	CHECK_EQ(threads.size(), size_t(2));
	if (threads.size() == 2)
	{
		CHECK_EQ(threads[0].id, 100u);
		CHECK_EQ(threads[0].calls[size_t(SceneCall::Simulate)], 1u);
		CHECK_EQ(threads[0].calls[size_t(SceneCall::FetchResults)], 1u);
		CHECK_EQ(threads[1].id, 200u);
		const size_t add = size_t(SceneCall::AddActor);
		CHECK_EQ(threads[1].calls[add], 2u);
		CHECK_EQ(threads[1].inUpdate[add], 2u);
		CHECK_EQ(threads[1].locked[add], 1u);
		CHECK_EQ(threads[1].inUpdateUnlocked[add], 1u);
		CHECK_EQ(threads[1].locked[size_t(SceneCall::RemoveActor)], 1u);
		CHECK_EQ(threads[1].inUpdateUnlocked[size_t(SceneCall::AddActors)], 1u);
	}
}

TEST(SceneWatchFollowsTheWriteLockBeforeItCounts)
{
	// The hooks went in while thread 200 held the write lock. Its unlock comes without a lock, and leaves it unlocked
	// rather than owing one; its calls before the start, under a lock the hooks didn't see taken, aren't counted.
	SceneRecorder recorder;
	recorder.Note(SceneCall::AddActor, 200, true);
	recorder.Lock(200, -1);
	recorder.Lock(200, 1);
	recorder.Start();
	recorder.Note(SceneCall::AddActor, 200, true);
	recorder.Lock(200, -1);
	recorder.Note(SceneCall::RemoveActor, 200, true);
	const auto threads = recorder.Threads();
	CHECK_EQ(threads.size(), size_t(1));
	if (threads.size() == 1)
	{
		const size_t add = size_t(SceneCall::AddActor);
		CHECK_EQ(threads[0].calls[add], 1u);
		CHECK_EQ(threads[0].locked[add], 1u);
		CHECK_EQ(threads[0].inUpdateUnlocked[add], 0u);
		CHECK_EQ(threads[0].inUpdateUnlocked[size_t(SceneCall::RemoveActor)], 1u);
	}
	recorder.Reset();
	CHECK(!recorder.Started());
}

TEST(SceneWatchSummarizesByThreadAndKind)
{
	SceneRecorder recorder;
	recorder.Start();
	CHECK_STR(WatchSummary(recorder.Threads(), 100, recorder.Shifts(), recorder.ShiftCount()), "no calls yet; no shifts");
	for (int i = 0; i < 3; ++i)
	{
		recorder.Note(SceneCall::Simulate, 100, false);
		recorder.Note(SceneCall::FetchResults, 100, false);
	}
	recorder.Note(SceneCall::AddActor, 200, true);
	recorder.Note(SceneCall::AddAggregate, 200, false);
	recorder.Note(SceneCall::LockWrite, 200, false);
	recorder.Note(SceneCall::UnlockWrite, 200, false);  // not counted: it mirrors lockWrite
	CHECK_STR(WatchSummary(recorder.Threads(), 100, recorder.Shifts(), recorder.ShiftCount()),
	          "thread 100, the main thread: step 3, fetch 3; thread 200: add 2 (1 in OnUpdate, 1 of them without the write lock), "
	          "lock 1; no shifts");
}

TEST(SceneWatchKeepsTheLatestShifts)
{
	SceneRecorder recorder;
	recorder.Start();
	for (int i = 0; i < 20; ++i)
	{
		recorder.NoteShift(100, false, 0.0f, float(i), 0.0f);
	}
	recorder.NoteShift(300, true, 0.0f, -1024.0f, 0.0f);
	CHECK_EQ(recorder.ShiftCount(), 21ull);
	const auto shifts = recorder.Shifts();
	CHECK_EQ(shifts.size(), size_t(SceneRecorder::kShifts));
	CHECK_EQ(shifts.front().y, 5.0f);  // the latest 16 of 21, oldest first
	CHECK_EQ(shifts.back().y, -1024.0f);
	CHECK_STR(WatchSummary({}, 100, shifts, recorder.ShiftCount()),
	          "no calls yet; 21 shifts, the last (0, -1024, 0) on thread 300, in OnUpdate");
}

TEST(SceneWatchCountsThreadsPastItsTableTogether)
{
	SceneRecorder recorder;
	recorder.Start();
	for (uint32_t thread = 1; thread <= 40; ++thread)
	{
		recorder.Note(SceneCall::Simulate, thread, false);
	}
	const auto threads = recorder.Threads();
	CHECK_EQ(threads.size(), SceneRecorder::kThreads);
	CHECK_EQ(threads.back().id, 0u);  // the other threads
	CHECK_EQ(threads.back().calls[size_t(SceneCall::Simulate)], uint32_t(40 - (SceneRecorder::kThreads - 1)));
	CHECK(WatchSummary(threads, 1, {}, 0).find("; the other threads: step 9; no shifts") != std::string::npos);
	recorder.Reset();
	CHECK(recorder.Threads().empty());
}

TEST(SceneWatchTakesCallsFromManyThreadsAtOnce)
{
	SceneRecorder            recorder;
	std::vector<std::thread> threads;
	recorder.Start();
	for (uint32_t thread = 1; thread <= 8; ++thread)
	{
		threads.emplace_back([&recorder, thread] {
			for (int i = 0; i < 10000; ++i)
			{
				recorder.Note(SceneCall::FetchResults, thread, i % 2 == 0);
			}
		});
	}
	for (std::thread& thread : threads)
	{
		thread.join();
	}
	const auto counted = recorder.Threads();
	CHECK_EQ(counted.size(), size_t(8));
	for (const auto& thread : counted)
	{
		CHECK_EQ(thread.calls[size_t(SceneCall::FetchResults)], 10000u);
		CHECK_EQ(thread.inUpdate[size_t(SceneCall::FetchResults)], 5000u);
	}
}
