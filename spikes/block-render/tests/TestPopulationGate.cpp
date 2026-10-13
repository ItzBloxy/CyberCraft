#include "Harness.h"

#include "PopulationGate.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace
{
using cpmc::population::kNoticeCode;
using cpmc::population::kNoticeSlot;

constexpr size_t kFlag = 0x20b4;  // the byte the game's notice sets

// A stand-in for the game's population system: an object whose vtable's notice entry runs the game's own 8 bytes, from
// executable memory, so calling through it sets the object's byte at kFlag as the game's does.
struct FakeSystem
{
	FakeSystem()
	{
		code = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
		std::memcpy(code, kNoticeCode.data(), kNoticeCode.size());
		vtable.assign(kNoticeSlot + 1, nullptr);
		vtable[kNoticeSlot] = code;
		object.assign(kFlag + 64, 0);
		void** const table = vtable.data();
		std::memcpy(object.data(), &table, sizeof(table));
	}
	~FakeSystem() { VirtualFree(code, 0, MEM_RELEASE); }

	// The image holding both the vtable and the code, as the game's executable holds both.
	cpmc::population::Image Image() const
	{
		const uintptr_t table = reinterpret_cast<uintptr_t>(vtable.data());
		const uintptr_t function = reinterpret_cast<uintptr_t>(code);
		return { std::min(table, function), std::max(table + vtable.size() * sizeof(void*), function + 4096) };
	}

	void Notice() { reinterpret_cast<void (*)(void*)>(vtable[kNoticeSlot])(object.data()); }

	uint8_t*             code = nullptr;
	std::vector<void*>   vtable;  // on the heap, as the patch changes its page's protection
	std::vector<uint8_t> object;
};
}

TEST(PopulationNoticeCodeIsTheGames)
{
	std::array<uint8_t, 8> code = kNoticeCode;
	CHECK(cpmc::population::IsNoticeCode(code.data()));
	for (size_t i = 0; i < code.size(); ++i)
	{
		code = kNoticeCode;
		code[i] ^= 0x01;
		CHECK(!cpmc::population::IsNoticeCode(code.data()));
	}
	CHECK(!cpmc::population::IsNoticeCode(nullptr));
}

TEST(PopulationMutedOnlyJustAfterATeleport)
{
	const int64_t second = 1000;
	CHECK(!cpmc::population::Muted(5000, 0, second));     // no teleport yet
	CHECK(cpmc::population::Muted(5000, 5000, second));   // the same moment
	CHECK(cpmc::population::Muted(5249, 5000, second));   // 0.249 s after
	CHECK(!cpmc::population::Muted(5250, 5000, second));  // kHoldSeconds after
	CHECK(!cpmc::population::Muted(9000, 5000, second));
	CHECK(cpmc::population::Muted(4990, 5000, second));   // another thread noted a teleport after this time was read
}

TEST(PopulationGateChecksBeforeItPatches)
{
	FakeSystem fake;
	void* const original = fake.vtable[kNoticeSlot];
	CHECK(!cpmc::population::Install(nullptr, fake.Image()).empty());
	// Code outside the game's image isn't the game's.
	const cpmc::population::Image elsewhere{ reinterpret_cast<uintptr_t>(fake.vtable.data()),
		                                     reinterpret_cast<uintptr_t>(fake.vtable.data()) + fake.vtable.size() * sizeof(void*) };
	if (reinterpret_cast<uintptr_t>(fake.code) < elsewhere.begin || reinterpret_cast<uintptr_t>(fake.code) >= elsewhere.end)
	{
		CHECK(!cpmc::population::Install(fake.object.data(), elsewhere).empty());
		CHECK(fake.vtable[kNoticeSlot] == original);
	}
	// Code that isn't the notice's, as after a game patch or another mod's hook.
	fake.code[6] = 0x02;
	CHECK(!cpmc::population::Install(fake.object.data(), fake.Image()).empty());
	CHECK(fake.vtable[kNoticeSlot] == original);
	CHECK(!cpmc::population::Read().installed);
}

TEST(PopulationGateMutesTheNoticeOnlyAfterATeleport)
{
	FakeSystem fake;
	void* const original = fake.vtable[kNoticeSlot];
	CHECK_STR(cpmc::population::Install(fake.object.data(), fake.Image()), "");
	CHECK(fake.vtable[kNoticeSlot] != original);
	CHECK(cpmc::population::Read().installed);
	CHECK_STR(cpmc::population::Install(fake.object.data(), fake.Image()), "");  // twice changes nothing

	// No teleport of build mode's yet: the game's own function runs.
	cpmc::population::Counts before = cpmc::population::Read();
	fake.Notice();
	CHECK_EQ(int(fake.object[kFlag]), 1);
	CHECK_EQ(cpmc::population::Read().passed, before.passed + 1);
	CHECK_EQ(cpmc::population::Read().muted, before.muted);

	// Just after one: skipped, so the system spawns on.
	fake.object[kFlag] = 0;
	cpmc::population::NoteTeleport();
	before = cpmc::population::Read();
	fake.Notice();
	CHECK_EQ(int(fake.object[kFlag]), 0);
	CHECK_EQ(cpmc::population::Read().muted, before.muted + 1);
	CHECK_EQ(cpmc::population::Read().passed, before.passed);

	CHECK_EQ(cpmc::population::Restore(), size_t(1));
	CHECK(fake.vtable[kNoticeSlot] == original);
	CHECK(!cpmc::population::Read().installed);
}
