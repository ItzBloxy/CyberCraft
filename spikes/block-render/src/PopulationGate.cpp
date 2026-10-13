#include "PopulationGate.h"

#include "Readable.h"
#include "hooks/VtablePatch.h"

#include <Windows.h>

#include <atomic>
#include <cstring>
#include <format>
#include <mutex>

namespace cpmc::population
{
namespace
{
using Notice = void (*)(void*);

std::mutex            g_mutex;  // Install and Restore
VtablePatcher         g_patcher;
std::atomic<Notice>   g_original{ nullptr };  // the game's function, stored before the entry points at the gate
std::atomic<bool>     g_installed{ false };
std::atomic<int64_t>  g_lastTeleport{ 0 };  // QueryPerformanceCounter at build mode's last teleport; 0 before any
std::atomic<uint64_t> g_muted{ 0 };
std::atomic<uint64_t> g_passed{ 0 };

int64_t QpcNow()
{
	LARGE_INTEGER now{};
	QueryPerformanceCounter(&now);
	return now.QuadPart;
}

int64_t QpcPerSecond()
{
	static const int64_t perSecond = [] {
		LARGE_INTEGER frequency{};
		QueryPerformanceFrequency(&frequency);
		return frequency.QuadPart;
	}();
	return perSecond;
}

bool Inside(const Image& aImage, const void* aAddress, size_t aSize)
{
	const uintptr_t at = reinterpret_cast<uintptr_t>(aAddress);
	return at >= aImage.begin && at < aImage.end && aSize <= aImage.end - at;
}

// The notice's vtable entry, once the gate is in. The game calls it from the teleportation facility's job, on whichever
// thread runs it, so it reads only atomics.
void Gate(void* aSystem)
{
	if (Muted(QpcNow(), g_lastTeleport.load(std::memory_order_relaxed), QpcPerSecond()))
	{
		g_muted.fetch_add(1, std::memory_order_relaxed);
		return;
	}
	g_passed.fetch_add(1, std::memory_order_relaxed);
	g_original.load(std::memory_order_acquire)(aSystem);
}
}

bool IsNoticeCode(const uint8_t* aCode)
{
	return aCode && std::memcmp(aCode, kNoticeCode.data(), kNoticeCode.size()) == 0;
}

bool Muted(int64_t aNow, int64_t aLastTeleport, int64_t aPerSecond)
{
	return aLastTeleport != 0 && double(aNow - aLastTeleport) < kHoldSeconds * double(aPerSecond);
}

std::string Install(void* aSystem, const Image& aImage)
{
	std::lock_guard lock(g_mutex);
	if (!aSystem)
	{
		return "the game's population system isn't there";
	}
	// Mod code must never fault: every pointer is checked readable before it's read.
	ReadableMemory memory;
	if (!memory.Range(aSystem, sizeof(void*)))
	{
		return "the population system can't be read";
	}
	void** const vtable = *static_cast<void***>(aSystem);
	void** const slot = vtable + kNoticeSlot;
	if (!Inside(aImage, slot, sizeof(void*)) || !memory.Range(slot, sizeof(void*)))
	{
		return "the population system's vtable isn't in the game's executable";
	}
	void* const current = *slot;
	if (current == reinterpret_cast<void*>(&Gate))
	{
		return {};
	}
	if (!Inside(aImage, current, kNoticeCode.size()) || !memory.Range(current, kNoticeCode.size()))
	{
		return std::format("the teleport notice (vtable entry {}) isn't the game's own code: another mod may have hooked it", kNoticeSlot);
	}
	if (!IsNoticeCode(static_cast<const uint8_t*>(current)))
	{
		return std::format("the teleport notice (vtable entry {}) isn't the code the gate knows, `mov byte ptr [rcx+0x20b4], 1; ret`: "
		                   "another game version, or another mod's hook",
		                   kNoticeSlot);
	}
	// Stored first, so no call reaches the gate before it can pass the notice on.
	g_original.store(reinterpret_cast<Notice>(current), std::memory_order_release);
	if (!g_patcher.Patch(aSystem, kNoticeSlot, reinterpret_cast<void*>(&Gate)))
	{
		return "the population system's vtable entry couldn't be written";
	}
	g_muted.store(0);
	g_passed.store(0);
	g_installed.store(true);
	return {};
}

size_t Restore()
{
	std::lock_guard lock(g_mutex);
	const size_t restored = g_patcher.RestoreAll();
	g_installed.store(false);
	return restored;
}

void NoteTeleport()
{
	g_lastTeleport.store(QpcNow(), std::memory_order_relaxed);
}

Counts Read()
{
	return { g_installed.load(), g_muted.load(), g_passed.load() };
}
}
