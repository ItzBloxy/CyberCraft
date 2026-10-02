#include "Harness.h"

#include "link/Mapping.h"

#include <skycraft_protocol.h>

#include <cstring>
#include <format>

using namespace cpmc::link;
namespace proto = skycraft::proto;

namespace
{
// A name of the tests' own, so they never touch SkyCraft's real mapping.
std::wstring TestName()
{
	static int count = 0;
	return std::format(L"Local\\BlockRenderTest_{}_{}", GetCurrentProcessId(), count++);
}

constexpr uint32_t kGonePid = 0xFFFFFFF0;  // no process has it

proto::Header HeaderOf(const Mapping& aMapping)
{
	proto::Header header;
	std::memcpy(&header, aMapping.Base(), sizeof header);
	return header;
}
}

TEST(MappingCreatesAndWritesTheHeader)
{
	Mapping    mapping;
	const auto opened = mapping.Open(TestName().c_str(), 1234, 5000);
	CHECK(opened.outcome == Mapping::Outcome::Created);
	CHECK(mapping.Base() != nullptr);
	auto header = HeaderOf(mapping);
	CHECK_EQ(header.magic, proto::kMagic);
	CHECK_EQ(header.version, proto::kVersion);
	CHECK_EQ(header.skyrimPid, 1234u);
	CHECK_EQ(header.skyrimHeartbeatMs, 5000ull);
	mapping.Heartbeat(6000);
	CHECK_EQ(HeaderOf(mapping).skyrimHeartbeatMs, 6000ull);
	CHECK_EQ(mapping.McPid(), 0u);
	CHECK_EQ(mapping.McHeartbeatMs(), 0ull);
}

TEST(MappingIsReusedAfterAGameSideHasGone)
{
	const auto name = TestName();
	Mapping    minecraft;  // stands for Minecraft, holding the mapping open after the game side has gone
	CHECK(minecraft.Open(name.c_str(), kGonePid, 1000).outcome == Mapping::Outcome::Created);
	const uint64_t staleTail = 77;
	std::memcpy(minecraft.Base() + proto::kOffInputRing + proto::kInputRingTailOff, &staleTail, sizeof staleTail);
	Mapping    game;
	const auto opened = game.Open(name.c_str(), GetCurrentProcessId(), 1500);
	CHECK(opened.outcome == Mapping::Outcome::Reused);
	CHECK_EQ(HeaderOf(game).skyrimPid, uint32_t(GetCurrentProcessId()));
	uint64_t tail = 1;
	std::memcpy(&tail, game.Base() + proto::kOffInputRing + proto::kInputRingTailOff, sizeof tail);
	CHECK_EQ(tail, 0ull);  // reset, as SkyCraft does
}

TEST(MappingRefusesALiveGameSide)
{
	const auto name = TestName();
	Mapping    first;
	CHECK(first.Open(name.c_str(), GetCurrentProcessId(), 10'000).outcome == Mapping::Outcome::Created);
	Mapping    second;
	const auto refused = second.Open(name.c_str(), 12, 10'100);  // as if we were process 12: this one is another, alive
	CHECK(refused.outcome == Mapping::Outcome::Refused);
	CHECK(refused.why.find("another game side") != std::string::npos);
	CHECK(second.Base() == nullptr);
	CHECK(second.Open(name.c_str(), 12, 20'000).outcome == Mapping::Outcome::Reused);  // its heartbeat has gone stale
}

TEST(MappingRefusesOneTooSmall)
{
	const auto name = TestName();
	HANDLE     small = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 64 * 1024, name.c_str());
	CHECK(small != nullptr);
	Mapping    mapping;
	const auto refused = mapping.Open(name.c_str(), GetCurrentProcessId(), 1000);
	CHECK(refused.outcome == Mapping::Outcome::Refused);
	CHECK(refused.why.find("smaller") != std::string::npos);
	CloseHandle(small);
}
