#include "Readable.h"

#include <Windows.h>
#include <psapi.h>

#include <algorithm>
#include <chrono>
#include <iterator>

namespace cpmc
{
namespace
{
constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                            PAGE_EXECUTE_WRITECOPY;
constexpr uintptr_t kPage = 4096;          // x64 Windows' page size
constexpr size_t    kChunkPages = 256;     // pages asked about in one QueryWorkingSetEx call
constexpr size_t    kMostPages = 1 << 16;  // longer ranges, over 256 MB, go to VirtualQuery

bool ReadableProtection(DWORD aProtect)
{
	return (aProtect & kReadable) != 0 && (aProtect & (PAGE_GUARD | PAGE_NOACCESS)) == 0;
}
}

bool ReadableMemory::Range(const void* aAddress, size_t aSize)
{
	const auto start = reinterpret_cast<uintptr_t>(aAddress);
	if (start == 0 || start + aSize < start)
	{
		return false;
	}
	const uintptr_t end = start + aSize;
	if (aSize == 0 || Resident(start, end))
	{
		return true;
	}
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

bool ReadableMemory::Resident(uintptr_t aStart, uintptr_t aEnd) const
{
	const uintptr_t first = aStart & ~(kPage - 1);
	const size_t    pages = size_t((aEnd - 1 - first) / kPage) + 1;
	if (pages > kMostPages)
	{
		return false;
	}
	PSAPI_WORKING_SET_EX_INFORMATION entries[kChunkPages]{};
	for (size_t done = 0; done < pages;)
	{
		const size_t count = std::min(kChunkPages, pages - done);
		for (size_t i = 0; i < count; ++i)
		{
			entries[i].VirtualAddress = reinterpret_cast<void*>(first + (done + i) * kPage);
		}
		if (!QueryWorkingSetEx(GetCurrentProcess(), entries, DWORD(count * sizeof(entries[0]))))
		{
			return false;
		}
		for (size_t i = 0; i < count; ++i)
		{
			const PSAPI_WORKING_SET_EX_BLOCK& block = entries[i].VirtualAttributes;
			if (!block.Valid || !ReadableProtection(DWORD(block.Win32Protection)))
			{
				return false;
			}
		}
		done += count;
	}
	return true;
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
	++slowQueries_;
	const auto   start = std::chrono::steady_clock::now();
	const SIZE_T got = VirtualQuery(reinterpret_cast<const void*>(aAddress), &info, sizeof(info));
	slowMs_ += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	if (got == 0)
	{
		return false;
	}
	largestRegion_ = std::max(largestRegion_, size_t(info.RegionSize));
	const auto base = reinterpret_cast<uintptr_t>(info.BaseAddress);
	aOut.end = base + info.RegionSize;
	aOut.readable = info.State == MEM_COMMIT && ReadableProtection(info.Protect);
	regions_[base] = aOut;
	return true;
}
}
