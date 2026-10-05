#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace cpmc
{
// The NPCs in the world, by their EntityIDs' hashes, as the redscript module's wraps of NPCPuppet's OnGameAttached and
// OnDetach report them through BlockRender's natives (spike 6's "The module's list"). The game calls those callbacks on
// several threads at once, so every call takes the lock; the module's List reads the IDs on the main thread, from a copy.
// The IDs keep the order they were attached in: a detach takes its ID out without moving the others, and an ID already
// there stays where it is, so the module can keep what it reads of each NPC in step with the list.
class NpcRegistry
{
public:
	static constexpr size_t kMaxIds = 1024;     // further attaches are counted, not kept
	static constexpr size_t kMaxThreads = 64;   // further threads' calls are counted under thread 0

	// A thread the wraps were called on, and how many times.
	struct ThreadCalls
	{
		uint32_t thread = 0;
		uint64_t attached = 0;
		uint64_t detached = 0;
	};

	// What the counts say, for the log.
	struct Counts
	{
		size_t   ids = 0;
		uint64_t attached = 0;
		uint64_t detached = 0;
		uint64_t repeats = 0;  // attaches of an ID already there
		uint64_t strays = 0;   // detaches of an ID not there
		uint64_t full = 0;     // attaches dropped, kMaxIds being there
	};

	void Attached(uint64_t aId, uint32_t aThread);
	void Detached(uint64_t aId, uint32_t aThread);

	// Copies the IDs for the module's List and gives how many there are; At gives the copy's aIndex-th, or 0 outside it.
	int32_t Copy();
	uint64_t At(int32_t aIndex) const;

	Counts                   Count() const;
	std::vector<ThreadCalls> Threads() const;  // in the order first seen

private:
	ThreadCalls& CallsOn(uint32_t aThread);  // under the lock

	mutable std::mutex       mutex_;
	std::vector<uint64_t>    ids_;
	std::vector<uint64_t>    copy_;
	std::vector<ThreadCalls> threads_;
	Counts                   counts_;
};

// "29680, the main thread: 120 attached, 98 detached; 1234: 50 attached, 3 detached", for the log; "none" if empty.
std::string DescribeThreads(const std::vector<NpcRegistry::ThreadCalls>& aThreads, uint32_t aMainThread);
}
