#include "physx/Readable.h"

#include <Windows.h>

#include <iterator>

namespace cpmc::px
{
namespace
{
constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                            PAGE_EXECUTE_WRITECOPY;
}

bool ReadableMemory::Range(const void* aAddress, size_t aSize)
{
	const auto start = reinterpret_cast<uintptr_t>(aAddress);
	if (start == 0 || start + aSize < start)
	{
		return false;
	}
	const uintptr_t end = start + aSize;
	for (uintptr_t at = start; at < end;)
	{
		Region region;
		if (!Find(at, region) || !region.readable)
		{
			return false;
		}
		at = region.end;
	}
	return true;
}

void ReadableMemory::Clear()
{
	regions_.clear();
}

bool ReadableMemory::Find(uintptr_t aAddress, Region& aOut)
{
	auto next = regions_.upper_bound(aAddress);
	if (next != regions_.begin())
	{
		const auto& [start, region] = *std::prev(next);
		if (aAddress < region.end)
		{
			aOut = region;
			return true;
		}
	}
	MEMORY_BASIC_INFORMATION info{};
	if (VirtualQuery(reinterpret_cast<const void*>(aAddress), &info, sizeof(info)) == 0)
	{
		return false;
	}
	const auto base = reinterpret_cast<uintptr_t>(info.BaseAddress);
	aOut.end = base + info.RegionSize;
	aOut.readable = info.State == MEM_COMMIT && (info.Protect & kReadable) != 0 &&
	                (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0;
	regions_[base] = aOut;
	return true;
}
}
