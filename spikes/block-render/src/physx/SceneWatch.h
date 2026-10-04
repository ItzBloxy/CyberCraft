#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Step 3's watch of scene 0 (spike 5's "The scene watch"): which of the game's threads add and remove actors, step and
// fetch the simulation, take the write lock and shift the origin, and whether while OnUpdate runs. That decided where
// the exporter can read the scene. Part 3 adds actors to the scene in OnUpdate, so the watch also counts the scene's
// queries (raycasts, sweeps and overlaps), which no other thread may make meanwhile. This is the record the hooks keep,
// and its summary; SceneHooks.h installs the hooks.
namespace cpmc::px
{
// The calls watched, in PxScene's vtable order.
enum class SceneCall : uint8_t
{
	AddActor,
	AddActorsPruning,  // addActors(const PxPruningStructure&)
	AddActors,         // addActors(PxActor* const*, PxU32)
	RemoveActor,
	RemoveActors,
	AddAggregate,
	RemoveAggregate,
	Simulate,
	Advance,
	Collide,
	FetchResults,
	FetchResultsStart,
	FetchResultsFinish,
	Raycast,
	Sweep,
	Overlap,
	LockWrite,
	UnlockWrite,
	ShiftOrigin,
};

constexpr size_t kSceneCalls = 19;

// A call's name, its slot in PxScene's vtable, and the RVA in PhysX3_x64.dll of the function the audit found there.
struct SceneCallInfo
{
	const char* name;
	uint32_t    slot;
	uint32_t    rva;
};

const SceneCallInfo& InfoOf(SceneCall aCall);

// The hooks' record: for each thread, how many of each call it made, how many while OnUpdate ran, and how many with the
// write lock held by that thread; and the last origin shifts. The hooks run on the game's threads, so it takes no lock
// and allocates nothing: fixed tables of atomics.
class SceneRecorder
{
public:
	static constexpr size_t kThreads = 32;  // threads told apart; any after them count as one, with the id 0
	static constexpr size_t kShifts = 16;   // the latest shifts kept

	// From the hooks. aThread: GetCurrentThreadId(). aInUpdate: OnUpdate was running. Counted only once started.
	void Note(SceneCall aCall, uint32_t aThread, bool aInUpdate);
	void NoteShift(uint32_t aThread, bool aInUpdate, float aX, float aY, float aZ);
	// The thread has taken the write lock (+1, after lockWrite returns), or is letting it go (-1, before unlockWrite),
	// followed from the moment the hooks go in. A thread that held the lock then lets it go without having taken it
	// here: it's left unlocked, not owing a lock.
	void Lock(uint32_t aThread, int aBy);

	// From the main thread, a second after the hooks go in: calls count from here. Any write lock a thread held as the
	// hooks went in has been let go by then, so it can't make that thread's later calls look unlocked.
	void Start() { started_.store(true); }
	bool Started() const { return started_.load(); }

	// From the main thread, with no hook installed. Not started again until Start.
	void Reset();

	struct Thread
	{
		uint32_t                           id = 0;
		std::array<uint32_t, kSceneCalls> calls{};
		std::array<uint32_t, kSceneCalls> inUpdate{};          // of those, while OnUpdate ran
		std::array<uint32_t, kSceneCalls> locked{};            // with the thread holding the write lock
		std::array<uint32_t, kSceneCalls> inUpdateUnlocked{};  // while OnUpdate ran, without it
	};

	struct Shift
	{
		uint32_t thread = 0;
		bool     inUpdate = false;
		float    x = 0.0f, y = 0.0f, z = 0.0f;
	};

	std::vector<Thread> Threads() const;  // the threads that made a call, in the order they first did
	std::vector<Shift>  Shifts() const;   // the latest kShifts, oldest first
	uint64_t            ShiftCount() const { return shifts_.load(); }

private:
	struct Slot
	{
		std::atomic<uint32_t>                          id{ 0 };
		std::atomic<int32_t>                           lockDepth{ 0 };
		std::array<std::atomic<uint32_t>, kSceneCalls> calls{};
		std::array<std::atomic<uint32_t>, kSceneCalls> inUpdate{};
		std::array<std::atomic<uint32_t>, kSceneCalls> locked{};
		std::array<std::atomic<uint32_t>, kSceneCalls> inUpdateUnlocked{};
	};

	struct ShiftSlot
	{
		std::atomic<uint32_t> thread{ 0 };
		std::atomic<bool>     inUpdate{ false };
		std::atomic<float>    x{ 0.0f }, y{ 0.0f }, z{ 0.0f };
	};

	Slot& SlotOf(uint32_t aThread);

	std::array<Slot, kThreads>     slots_;  // the last is for every thread past the others
	std::array<ShiftSlot, kShifts> shiftSlots_;
	std::atomic<uint64_t>          shifts_{ 0 };
	std::atomic<bool>              started_{ false };
};

// The status's line for a watch: each thread's calls by kind (add, remove, step, fetch, query, lock and shift), and of
// each kind, how many came while OnUpdate ran, and of those that write, how many without the write lock; then how many
// calls other threads than aMainThread, OnUpdate's own, made while it ran; then the shifts.
std::string WatchSummary(const std::vector<SceneRecorder::Thread>& aThreads, uint32_t aMainThread,
                         const std::vector<SceneRecorder::Shift>& aShifts, uint64_t aShiftCount);
}
