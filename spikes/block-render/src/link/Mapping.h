#pragma once

#include <Windows.h>

#include <cstdint>
#include <string>

// The shared mapping, made the way SkyCraft's Skyrim side makes it (skse/src/Link.cpp, MIT): the game side creates it,
// and Minecraft opens it. It allows this user, the system and administrators at medium integrity, so a game run
// elevated doesn't lock Minecraft out.
namespace cpmc::link
{
constexpr uint64_t kAliveMs = 3000;  // a heartbeat younger than this means that side is there (SkyCraft's kMcTimeoutMs)

class Mapping
{
public:
	enum class Outcome
	{
		Created,
		Reused,
		Refused,
		Failed,
	};

	struct Opened
	{
		Outcome     outcome = Outcome::Failed;
		std::string why;
	};

	Mapping() = default;
	Mapping(const Mapping&) = delete;
	Mapping& operator=(const Mapping&) = delete;
	~Mapping();

	// Creates aName at the protocol's size, or opens the one already there: Minecraft keeps it open between game
	// sides. A mapping smaller than the protocol's layout is refused, and so is one another game side still uses (the
	// header's game process ID is a live process other than aOurPid, and its heartbeat is under 3 s old). Otherwise
	// the regions the game side owns are reset, and the header is written for aOurPid.
	Opened Open(const wchar_t* aName, uint32_t aOurPid, uint64_t aNowMs);
	void   Close();

	uint8_t* Base() const { return base_; }  // null until opened
	void     Heartbeat(uint64_t aNowMs);
	uint32_t McPid() const;
	uint64_t McHeartbeatMs() const;

private:
	HANDLE   handle_ = nullptr;
	uint8_t* base_ = nullptr;
};
}
