#include "physx/SceneWatch.h"

#include <algorithm>
#include <format>
#include <initializer_list>

namespace cpmc::px
{
namespace
{
// The audit's addendum (docs/spikes/2026-10-01-physx-abi-audit.md): each slot's function, by its strings and its code,
// in PhysX3_x64.dll with the audited timestamp and size.
constexpr SceneCallInfo kInfo[kSceneCalls] = {
	{ "addActor", 10, 0x520d0 },
	{ "addActors(pruning structure)", 11, 0x54a30 },
	{ "addActors", 12, 0x54a20 },
	{ "removeActor", 13, 0x521e0 },
	{ "removeActors", 14, 0x54a90 },
	{ "addAggregate", 15, 0x524e0 },
	{ "removeAggregate", 16, 0x525d0 },
	{ "simulate", 56, 0x53760 },
	{ "advance", 57, 0x537a0 },
	{ "collide", 58, 0x53880 },
	{ "fetchResults", 61, 0x53980 },
	{ "fetchResultsStart", 62, 0x53a50 },
	{ "fetchResultsFinish", 64, 0x53d30 },
	{ "lockWrite", 107, 0x55180 },
	{ "unlockWrite", 108, 0x55280 },
	{ "shiftOrigin", 116, 0x55360 },
};

// The summary's kinds of call. unlockWrite only mirrors lockWrite, so it isn't counted.
struct Kind
{
	const char*                      name;
	std::initializer_list<SceneCall> calls;
};

const Kind kKinds[] = {
	{ "add", { SceneCall::AddActor, SceneCall::AddActorsPruning, SceneCall::AddActors, SceneCall::AddAggregate } },
	{ "remove", { SceneCall::RemoveActor, SceneCall::RemoveActors, SceneCall::RemoveAggregate } },
	{ "step", { SceneCall::Simulate, SceneCall::Advance, SceneCall::Collide } },
	{ "fetch", { SceneCall::FetchResults, SceneCall::FetchResultsStart, SceneCall::FetchResultsFinish } },
	{ "lock", { SceneCall::LockWrite } },
	{ "shift", { SceneCall::ShiftOrigin } },
};
}

const SceneCallInfo& InfoOf(SceneCall aCall)
{
	return kInfo[size_t(aCall)];
}

SceneRecorder::Slot& SceneRecorder::SlotOf(uint32_t aThread)
{
	for (size_t i = 0; i + 1 < kThreads; ++i)
	{
		const uint32_t id = slots_[i].id.load(std::memory_order_acquire);
		if (id == aThread)
		{
			return slots_[i];
		}
		if (id == 0)
		{
			uint32_t expected = 0;
			if (slots_[i].id.compare_exchange_strong(expected, aThread) || expected == aThread)
			{
				return slots_[i];
			}
		}
	}
	return slots_[kThreads - 1];
}

void SceneRecorder::Note(SceneCall aCall, uint32_t aThread, bool aInUpdate)
{
	if (!started_.load(std::memory_order_relaxed))
	{
		return;
	}
	Slot&        slot = SlotOf(aThread);
	const size_t call = size_t(aCall);
	const bool   locked = slot.lockDepth.load(std::memory_order_relaxed) > 0;
	slot.calls[call].fetch_add(1, std::memory_order_relaxed);
	if (aInUpdate)
	{
		slot.inUpdate[call].fetch_add(1, std::memory_order_relaxed);
	}
	if (locked)
	{
		slot.locked[call].fetch_add(1, std::memory_order_relaxed);
	}
	if (aInUpdate && !locked)
	{
		slot.inUpdateUnlocked[call].fetch_add(1, std::memory_order_relaxed);
	}
}

void SceneRecorder::NoteShift(uint32_t aThread, bool aInUpdate, float aX, float aY, float aZ)
{
	if (!started_.load(std::memory_order_relaxed))
	{
		return;
	}
	ShiftSlot& shift = shiftSlots_[shifts_.fetch_add(1) % kShifts];
	shift.thread.store(aThread);
	shift.inUpdate.store(aInUpdate);
	shift.x.store(aX);
	shift.y.store(aY);
	shift.z.store(aZ);
}

void SceneRecorder::Lock(uint32_t aThread, int aBy)
{
	// Never below none: a lock the thread held before the hooks went in is let go here without having been taken.
	std::atomic<int32_t>& depth = SlotOf(aThread).lockDepth;
	int32_t               now = depth.load(std::memory_order_relaxed);
	while (!depth.compare_exchange_weak(now, std::max(now + aBy, 0), std::memory_order_relaxed))
	{
	}
}

void SceneRecorder::Reset()
{
	started_.store(false);
	for (Slot& slot : slots_)
	{
		slot.id.store(0);
		slot.lockDepth.store(0);
		for (size_t call = 0; call < kSceneCalls; ++call)
		{
			slot.calls[call].store(0);
			slot.inUpdate[call].store(0);
			slot.locked[call].store(0);
			slot.inUpdateUnlocked[call].store(0);
		}
	}
	shifts_.store(0);
}

std::vector<SceneRecorder::Thread> SceneRecorder::Threads() const
{
	std::vector<Thread> threads;
	for (const Slot& slot : slots_)
	{
		Thread thread;
		thread.id = slot.id.load();
		bool any = false;
		for (size_t call = 0; call < kSceneCalls; ++call)
		{
			thread.calls[call] = slot.calls[call].load();
			thread.inUpdate[call] = slot.inUpdate[call].load();
			thread.locked[call] = slot.locked[call].load();
			thread.inUpdateUnlocked[call] = slot.inUpdateUnlocked[call].load();
			any = any || thread.calls[call] != 0;
		}
		if (any)
		{
			threads.push_back(thread);
		}
	}
	return threads;
}

std::vector<SceneRecorder::Shift> SceneRecorder::Shifts() const
{
	const uint64_t     count = shifts_.load();
	std::vector<Shift> shifts;
	for (uint64_t i = count > kShifts ? count - kShifts : 0; i < count; ++i)
	{
		const ShiftSlot& slot = shiftSlots_[i % kShifts];
		shifts.push_back({ slot.thread.load(), slot.inUpdate.load(), slot.x.load(), slot.y.load(), slot.z.load() });
	}
	return shifts;
}

std::string WatchSummary(const std::vector<SceneRecorder::Thread>& aThreads, uint32_t aMainThread,
                         const std::vector<SceneRecorder::Shift>& aShifts, uint64_t aShiftCount)
{
	std::string out;
	for (const SceneRecorder::Thread& thread : aThreads)
	{
		std::string kinds;
		for (const Kind& kind : kKinds)
		{
			uint64_t calls = 0, inUpdate = 0, unlocked = 0;
			for (const SceneCall call : kind.calls)
			{
				calls += thread.calls[size_t(call)];
				inUpdate += thread.inUpdate[size_t(call)];
				unlocked += thread.inUpdateUnlocked[size_t(call)];
			}
			if (calls != 0)
			{
				kinds += std::format("{}{} {}", kinds.empty() ? "" : ", ", kind.name, calls);
				if (inUpdate != 0)
				{
					kinds += std::format(" ({} in OnUpdate, {} of them without the write lock)", inUpdate, unlocked);
				}
			}
		}
		const std::string who = thread.id == 0            ? "the other threads"
		                        : thread.id == aMainThread ? std::format("thread {}, the main thread", thread.id)
		                                                   : std::format("thread {}", thread.id);
		out += std::format("{}{}: {}", out.empty() ? "" : "; ", who, kinds);
	}
	if (out.empty())
	{
		out = "no calls yet";
	}
	if (aShiftCount == 0 || aShifts.empty())
	{
		return out + "; no shifts";
	}
	const SceneRecorder::Shift& last = aShifts.back();
	return out + std::format("; {} shift{}, the last ({:g}, {:g}, {:g}) on thread {}, {} OnUpdate", aShiftCount, aShiftCount == 1 ? "" : "s",
	                         last.x, last.y, last.z, last.thread, last.inUpdate ? "in" : "outside");
}
}
