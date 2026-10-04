#pragma once

#include <cstddef>
#include <cstdint>
#include <map>

namespace cpmc
{
// Answers "can these bytes be read without faulting?".
// - A page in the process's working set is answered by QueryWorkingSetEx, which costs the same for any page.
// - Any other page goes to VirtualQuery. That reports the size of the whole region an address lies in, so it takes
//   longer the bigger the region: run 5 found it costing 12 ms a frame in the game's heaps of gigabytes.
// VirtualQuery's answers are remembered by region. The game's memory changes over time, so use one per dump and Clear()
// it before the next.
class ReadableMemory
{
public:
	// True when every byte of [aAddress, aAddress + aSize) is committed, readable and not a guard page. A null or
	// wrapping range never is; an empty range at a non-null address always is.
	bool Range(const void* aAddress, size_t aSize);

	void Clear();

	size_t SlowQueries() const { return slowQueries_; }      // the VirtualQuery calls made so far
	double SlowMs() const { return slowMs_; }                // the time they took
	size_t LargestRegion() const { return largestRegion_; }  // the largest region one of them reported, in bytes

private:
	struct Region
	{
		uintptr_t end = 0;
		bool      readable = false;
	};

	// True when every page of [aStart, aEnd) is in the working set, readable and not a guard page. A range of more than
	// 16 pages isn't asked about, and goes to VirtualQuery.
	bool Resident(uintptr_t aStart, uintptr_t aEnd) const;

	// The region holding aAddress, from memory or from VirtualQuery. False if VirtualQuery fails.
	bool Find(uintptr_t aAddress, Region& aOut);

	std::map<uintptr_t, Region> regions_;  // by start address
	size_t                      slowQueries_ = 0;
	double                      slowMs_ = 0.0;
	size_t                      largestRegion_ = 0;
};
}
