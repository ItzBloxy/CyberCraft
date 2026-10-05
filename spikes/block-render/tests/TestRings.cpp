#include "Harness.h"

#include "link/Rings.h"

#include <cstring>
#include <vector>

using namespace cpmc::link;

namespace
{
// 8-byte aligned memory, as the mapping is: a ring's header (0x80 bytes) and aDataBytes of data.
struct Memory
{
	explicit Memory(size_t aBytes) : words((aBytes + 7) / 8) {}
	uint8_t*              Bytes() { return reinterpret_cast<uint8_t*>(words.data()); }
	uint64_t&             Head() { return *reinterpret_cast<uint64_t*>(Bytes()); }
	uint64_t&             Tail() { return *reinterpret_cast<uint64_t*>(Bytes() + 0x40); }
	std::vector<uint64_t> words;
};

std::vector<uint8_t> Payload(size_t aBytes, uint8_t aFirst)
{
	std::vector<uint8_t> bytes(aBytes);
	for (size_t i = 0; i < aBytes; ++i)
	{
		bytes[i] = uint8_t(aFirst + i);
	}
	return bytes;
}

struct Seen
{
	uint32_t             type;
	std::vector<uint8_t> payload;
};

DrainResult Drain(Memory& aRing, uint64_t aDataBytes, std::vector<Seen>& aSeen, uint64_t aBudget = ~0ull, uint32_t aSkipType = 99)
{
	return DrainByteRing(aRing.Bytes(), aDataBytes, aBudget, [&](const RingMessage& aMessage) {
		aSeen.push_back({ aMessage.type, { aMessage.payload.begin(), aMessage.payload.end() } });
		return aMessage.type != aSkipType;
	});
}
}

TEST(RingsSkyStateCountsUpByTwo)
{
	Memory           memory(0x1000);
	proto::SkyState state{};
	state.flags = proto::kSkyInGame;
	state.posX = 1.5;
	state.teleportSeq = 7;
	WriteSkyState(memory.Bytes(), state);
	state.posY = -2.25;
	WriteSkyState(memory.Bytes(), state);
	proto::SkyState written;
	std::memcpy(&written, memory.Bytes() + proto::kOffSkyState, sizeof written);
	CHECK_EQ(written.seq, 4u);
	CHECK_EQ(written.flags, uint32_t(proto::kSkyInGame));
	CHECK_EQ(written.posX, 1.5);
	CHECK_EQ(written.posY, -2.25);
	CHECK_EQ(written.teleportSeq, 7u);
}

TEST(RingsActorTableCountsUpByTwoAndHoldsAtMost256)
{
	Memory memory(proto::kOffActorTable + sizeof(proto::ActorTable) + 64);
	std::memset(memory.Bytes(), 0xCD, memory.words.size() * 8);
	std::memset(memory.Bytes() + proto::kOffActorTable, 0, sizeof(proto::ActorTable));
	std::vector<proto::ActorRecord> records(2);
	records[0].formId = 1;
	records[0].x = -119.5f;
	records[1].formId = 2;
	records[1].name[0] = 'A';
	WriteActorTable(memory.Bytes(), records);
	proto::ActorTable table;
	std::memcpy(&table, memory.Bytes() + proto::kOffActorTable, sizeof table);
	CHECK_EQ(table.seq, 2u);
	CHECK_EQ(table.count, 2u);
	CHECK(std::memcmp(table.actors, records.data(), 2 * sizeof(proto::ActorRecord)) == 0);
	WriteActorTable(memory.Bytes(), {});  // empty: SkyCraft reads only the count's records
	std::memcpy(&table, memory.Bytes() + proto::kOffActorTable, sizeof table);
	CHECK_EQ(table.seq, 4u);
	CHECK_EQ(table.count, 0u);
	records.resize(300);
	records[299].formId = 300;
	WriteActorTable(memory.Bytes(), records);
	std::memcpy(&table, memory.Bytes() + proto::kOffActorTable, sizeof table);
	CHECK_EQ(table.seq, 6u);
	CHECK_EQ(table.count, proto::kMaxActors);
	CHECK_EQ(memory.Bytes()[proto::kOffActorTable + sizeof(proto::ActorTable)], 0xCD);  // nothing past the table
	CHECK_EQ(memory.Bytes()[proto::kOffActorTable - 1], 0xCD);                          // nor before it
}

TEST(RingsMcStateReadsWholeStates)
{
	Memory         memory(0x1000);
	proto::McState state{};
	state.seq = 6;
	state.flags = proto::kMcInWorld | proto::kMcScreenOpen;
	state.x = -119.5;
	state.frameCounter = 42;
	std::memcpy(memory.Bytes() + proto::kOffMcState, &state, sizeof state);
	const auto read = ReadMcState(memory.Bytes());
	CHECK(read.has_value());
	CHECK_EQ(read->flags, uint32_t(proto::kMcInWorld | proto::kMcScreenOpen));
	CHECK_EQ(read->x, -119.5);
	CHECK_EQ(read->frameCounter, 42ull);
	const uint32_t writing = 7;  // odd: Minecraft is mid-write
	std::memcpy(memory.Bytes() + proto::kOffMcState, &writing, sizeof writing);
	CHECK(!ReadMcState(memory.Bytes()).has_value());
}

TEST(RingsByteRingRoundTrip)
{
	Memory     ring(0x80 + 128);
	const auto five = Payload(5, 10);
	CHECK(WriteByteRing(ring.Bytes(), 128, 2, five));
	CHECK(WriteByteRing(ring.Bytes(), 128, 3, {}));
	CHECK_EQ(ring.Head(), 24ull);  // 8 + 5 rounded up to 16, then a bare header
	std::vector<Seen> seen;
	const auto        result = Drain(ring, 128, seen);
	CHECK_EQ(seen.size(), size_t(2));
	CHECK_EQ(seen[0].type, 2u);
	CHECK(seen[0].payload == five);
	CHECK_EQ(seen[1].type, 3u);
	CHECK(seen[1].payload.empty());
	CHECK_EQ(result.messages, 2u);
	CHECK_EQ(result.bytes, 24ull);
	CHECK(!result.resynced);
	CHECK_EQ(ring.Tail(), 24ull);
}

TEST(RingsByteRingPadsAtTheEnd)
{
	Memory ring(0x80 + 64);
	for (int i = 0; i < 3; ++i)
	{
		CHECK(WriteByteRing(ring.Bytes(), 64, 2, Payload(8, 0)));  // 16 bytes each
	}
	std::vector<Seen> seen;
	Drain(ring, 64, seen);
	CHECK_EQ(ring.Tail(), 48ull);
	CHECK(WriteByteRing(ring.Bytes(), 64, 4, Payload(16, 1)));  // 24 bytes: no room before the end, so a pad first
	CHECK_EQ(ring.Head(), 88ull);
	seen.clear();
	const auto result = Drain(ring, 64, seen);
	CHECK_EQ(seen.size(), size_t(1));
	CHECK_EQ(seen[0].type, 4u);
	CHECK(seen[0].payload == Payload(16, 1));
	CHECK_EQ(result.bytes, 40ull);  // the 16-byte pad, then the message
	CHECK_EQ(ring.Tail(), 88ull);
}

TEST(RingsByteRingRefusesWhatDoesntFit)
{
	Memory ring(0x80 + 64);
	CHECK(!WriteByteRing(ring.Bytes(), 64, 2, Payload(32, 0)));  // 40 bytes: over half the ring
	for (int i = 0; i < 4; ++i)
	{
		CHECK(WriteByteRing(ring.Bytes(), 64, 2, Payload(8, 0)));
	}
	CHECK(!WriteByteRing(ring.Bytes(), 64, 2, Payload(8, 0)));  // full
	CHECK_EQ(ring.Head(), 64ull);
}

TEST(RingsDrainStopsAtTheBudget)
{
	Memory ring(0x80 + 128);
	for (int i = 0; i < 3; ++i)
	{
		CHECK(WriteByteRing(ring.Bytes(), 128, 2, Payload(8, uint8_t(i))));
	}
	std::vector<Seen> seen;
	auto              result = Drain(ring, 128, seen, 20);
	CHECK_EQ(result.messages, 2u);  // 16 is under 20, so a second is taken; 32 isn't
	CHECK_EQ(result.takenBytes, 32ull);
	result = Drain(ring, 128, seen, 20);
	CHECK_EQ(result.messages, 1u);
	CHECK_EQ(seen.size(), size_t(3));
	CHECK_EQ(int(seen[2].payload[0]), 2);
}

TEST(RingsSkippedMessagesDontCountTowardTheBudget)
{
	Memory ring(0x80 + 128);
	for (int i = 0; i < 4; ++i)
	{
		CHECK(WriteByteRing(ring.Bytes(), 128, 7, Payload(8, 0)));
	}
	CHECK(WriteByteRing(ring.Bytes(), 128, 2, Payload(8, 0)));
	std::vector<Seen> seen;
	const auto        result = Drain(ring, 128, seen, 16, 7);
	CHECK_EQ(result.messages, 5u);
	CHECK_EQ(result.takenBytes, 16ull);
}

TEST(RingsDrainResyncsOnABadMessage)
{
	Memory                    ring(0x80 + 64);
	const proto::ColMsgHeader liar{ 2, 1000 };
	std::memcpy(ring.Bytes() + 0x80, &liar, sizeof liar);
	ring.Head() = 16;
	std::vector<Seen> seen;
	const auto        result = Drain(ring, 64, seen);
	CHECK(result.resynced);
	CHECK(seen.empty());
	CHECK_EQ(ring.Tail(), 16ull);
}

TEST(RingsDrainResyncsWhenTheTailIsAhead)
{
	Memory ring(0x80 + 64);
	ring.Head() = 8;
	ring.Tail() = 16;
	std::vector<Seen> seen;
	const auto        result = Drain(ring, 64, seen);
	CHECK(result.resynced);
	CHECK_EQ(ring.Tail(), 8ull);
}

TEST(RingsInputRingFillsUp)
{
	Memory                  ring(0x80 + proto::kInputRingEntries * sizeof(proto::InputEvent));
	const proto::InputEvent key{ proto::kInKey, 23, 1, 0, 0 };
	for (uint32_t i = 0; i < proto::kInputRingEntries; ++i)
	{
		CHECK(PushInput(ring.Bytes(), key));
	}
	CHECK(!PushInput(ring.Bytes(), key));
	ring.Tail() = 1;
	const proto::InputEvent up{ proto::kInKey, 23, 0, 0, 0 };
	CHECK(PushInput(ring.Bytes(), up));
	proto::InputEvent stored;
	std::memcpy(&stored, ring.Bytes() + 0x80, sizeof stored);  // entry 4096 wraps to slot 0
	CHECK_EQ(int(stored.a), 0);
	CHECK_EQ(ring.Head(), uint64_t(proto::kInputRingEntries) + 1);
}

TEST(RingsEventsDrainOldestFirst)
{
	Memory ring(0x80 + proto::kEventRingEntries * sizeof(proto::McEvent));
	for (uint32_t i = 0; i < 3; ++i)
	{
		proto::McEvent event{};
		event.type = proto::kEvExplosion;
		event.a = float(i);
		std::memcpy(ring.Bytes() + 0x80 + i * sizeof event, &event, sizeof event);
	}
	ring.Head() = 3;
	std::vector<float> seen;
	CHECK_EQ(DrainEvents(ring.Bytes(), [&](const proto::McEvent& aEvent) { seen.push_back(aEvent.a); }), 3u);
	CHECK(seen == std::vector<float>({ 0.0f, 1.0f, 2.0f }));
	CHECK_EQ(ring.Tail(), 3ull);
	ring.Head() = 3 + 600;  // lapped: only the newest 512 are still there
	CHECK_EQ(DrainEvents(ring.Bytes(), [](const proto::McEvent&) {}), proto::kEventRingEntries);
	CHECK_EQ(ring.Tail(), 603ull);
}
