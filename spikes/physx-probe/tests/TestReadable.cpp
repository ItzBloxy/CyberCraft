#include "Harness.h"

#include "physx/Readable.h"

#include <Windows.h>

#include <cstdint>

using cpmc::px::ReadableMemory;

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
