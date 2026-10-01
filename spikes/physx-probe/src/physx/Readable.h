#pragma once

#include <cstddef>
#include <cstdint>
#include <map>

namespace cpmc::px
{
// Answers "can these bytes be read without faulting?" from VirtualQuery, and remembers each region it has asked
// about. The game's memory changes over time, so use one per dump and Clear() it before the next.
class ReadableMemory
{
public:
	// True when every byte of [aAddress, aAddress + aSize) is committed, readable and not a guard page. A null or
	// wrapping range never is; an empty range at a non-null address always is.
	bool Range(const void* aAddress, size_t aSize);

	void Clear();

private:
	struct Region
	{
		uintptr_t end = 0;
		bool      readable = false;
	};

	// The region holding aAddress, from memory or from VirtualQuery. False if VirtualQuery fails.
	bool Find(uintptr_t aAddress, Region& aOut);

	std::map<uintptr_t, Region> regions_;  // by start address
};
}
