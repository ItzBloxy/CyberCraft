#pragma once

#include <skycraft_protocol.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>

// SkyCraft's seqlocked blocks and rings, read and written on raw memory: the mapping in the game and in mclink, plain
// buffers in the tests. The memory orders are those of SkyCraft's Skyrim side (skse/src/Link.cpp, MIT), which its
// Java side matches. Every ring has its head at +0x00, its tail at +0x40 and its data at +0x80.
namespace cpmc::link
{
namespace proto = skycraft::proto;

// The game side's state, at aBase + kOffSkyState. The sequence goes odd while writing, and up by 2 each write.
void WriteSkyState(uint8_t* aBase, const proto::SkyState& aState);

// Minecraft's state, at aBase + kOffMcState. Nothing if it was mid-write all 64 times we looked.
std::optional<proto::McState> ReadMcState(const uint8_t* aBase);

// One byte-ring message: its type, and its payload, which points into the ring.
struct RingMessage
{
	uint32_t                 type = 0;
	std::span<const uint8_t> payload;
};

struct DrainResult
{
	uint32_t    messages = 0;    // handed to the visitor
	uint64_t    bytes = 0;       // ring bytes consumed, pads included
	uint64_t    takenBytes = 0;  // ring bytes of the messages the visitor took
	bool        resynced = false;
	std::string why;  // why the tail jumped to the head
};

// Consumes a byte ring (SkyCraft's render ring) from its tail towards its head, handing each message to aVisit, which
// returns whether it took it. Stops once taken messages reach aBudget bytes, checked between messages. A head and tail
// that make no sense, or a message that doesn't fit, move the tail to the head. Nothing outside the data is read.
DrainResult DrainByteRing(uint8_t* aRing, uint64_t aDataBytes, uint64_t aBudget, const std::function<bool(const RingMessage&)>& aVisit);

// Appends one message to a byte ring (SkyCraft's collision ring), with a pad to the start when it wouldn't fit before
// the end. False, writing nothing, if there's no room or the message is over half the ring.
bool WriteByteRing(uint8_t* aRing, uint64_t aDataBytes, uint32_t aType, std::span<const uint8_t> aPayload);

// The input ring (game to Minecraft). False, writing nothing, if Minecraft is a whole ring behind.
bool PushInput(uint8_t* aRing, const proto::InputEvent& aEvent);

// The event ring (Minecraft to game): every waiting event, oldest first. If Minecraft lapped us, the oldest are gone.
uint32_t DrainEvents(uint8_t* aRing, const std::function<void(const proto::McEvent&)>& aVisit);
}
