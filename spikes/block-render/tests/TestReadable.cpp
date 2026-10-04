#include "Harness.h"

#include "Readable.h"

#include <Windows.h>

#include <cstdint>

using cpmc::ReadableMemory;

namespace
{
size_t PageSize()
{
	SYSTEM_INFO system{};
	GetSystemInfo(&system);
	return system.dwPageSize;
}
}

TEST(ReadableMemoryAcceptsOrdinaryMemory)
{
	ReadableMemory memory;
	const int      values[16] = {};
	CHECK(memory.Range(values, sizeof(values)));
	CHECK(memory.Range(values, 0));  // nothing to read at a real address
}

TEST(ReadableMemoryRejectsPagesThatWouldFault)
{
	// Four pages: readable, no access, a guard page, and reserved but never committed.
	const size_t page = PageSize();
	auto*        base = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4 * page, MEM_RESERVE, PAGE_NOACCESS));
	CHECK(base != nullptr);
	if (!base)
	{
		return;
	}
	VirtualAlloc(base, page, MEM_COMMIT, PAGE_READWRITE);
	VirtualAlloc(base + page, page, MEM_COMMIT, PAGE_NOACCESS);
	VirtualAlloc(base + 2 * page, page, MEM_COMMIT, PAGE_READWRITE | PAGE_GUARD);
	ReadableMemory memory;
	CHECK(memory.Range(base, page));
	CHECK(!memory.Range(base + page, 1));
	CHECK(!memory.Range(base + 2 * page, 1));
	CHECK(!memory.Range(base + 3 * page, 1));
	CHECK(memory.Range(base + page - 8, 8));
	CHECK(!memory.Range(base + page - 8, 16));  // starts readable, runs into the no-access page
	VirtualFree(base, 0, MEM_RELEASE);
}

TEST(ReadableMemoryRejectsNullAndWrappingRanges)
{
	ReadableMemory memory;
	CHECK(!memory.Range(nullptr, 1));
	CHECK(!memory.Range(nullptr, 0));
	const int value = 0;
	CHECK(!memory.Range(&value, SIZE_MAX));
}

TEST(ReadableMemoryRemembersRegionsUntilCleared)
{
	const size_t page = PageSize();
	auto*        base = static_cast<uint8_t*>(VirtualAlloc(nullptr, page, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
	CHECK(base != nullptr);
	if (!base)
	{
		return;
	}
	ReadableMemory memory;
	CHECK(!memory.Range(base, 1));
	DWORD old = 0;
	VirtualProtect(base, page, PAGE_READONLY, &old);
	CHECK(!memory.Range(base, 1));  // still the remembered answer
	memory.Clear();
	CHECK(memory.Range(base, 1));
	VirtualFree(base, 0, MEM_RELEASE);
}

TEST(ReadableMemoryAnswersPagesInTheWorkingSetWithoutVirtualQuery)
{
	// VirtualQuery reports the size of the whole region an address lies in, which takes longer the bigger the region:
	// in the game's heaps it cost 12 ms a frame. Pages in the working set are answered without it.
	const size_t page = PageSize();
	auto*        base = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	CHECK(base != nullptr);
	if (!base)
	{
		return;
	}
	base[0] = 1;  // written to, so in the working set
	base[page] = 1;
	ReadableMemory memory;
	CHECK(memory.Range(base + 100, page));  // across both pages
	CHECK_EQ(memory.SlowQueries(), size_t(0));
	CHECK(memory.Range(base + 3 * page, 16));  // never touched, so not in the working set yet: VirtualQuery answers
	CHECK_EQ(memory.SlowQueries(), size_t(1));
	VirtualFree(base, 0, MEM_RELEASE);
}

TEST(ReadableMemoryAnswersLargeRangesInTheWorkingSetWithoutVirtualQuery)
{
	// A heightfield's samples or a mesh's triangles run to hundreds of pages, in heaps whose regions are hundreds of
	// megabytes: in step 3 one VirtualQuery there took about 3.5 ms. Pages in the working set are answered without it.
	const size_t page = PageSize();
	const size_t pages = 300;
	auto*        base = static_cast<uint8_t*>(VirtualAlloc(nullptr, (pages + 1) * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	CHECK(base != nullptr);
	if (!base)
	{
		return;
	}
	for (size_t i = 0; i < pages; ++i)
	{
		base[i * page] = 1;  // written to, so in the working set
	}
	ReadableMemory memory;
	CHECK(memory.Range(base + 100, pages * page - 200));
	CHECK_EQ(memory.SlowQueries(), size_t(0));
	CHECK(memory.Range(base, (pages + 1) * page));  // the last page never touched: VirtualQuery answers
	CHECK_EQ(memory.SlowQueries(), size_t(1));
	DWORD old = 0;
	VirtualProtect(base + 150 * page, page, PAGE_NOACCESS, &old);
	memory.Clear();
	CHECK(!memory.Range(base, pages * page));  // a page in the middle would fault
	VirtualFree(base, 0, MEM_RELEASE);
}
