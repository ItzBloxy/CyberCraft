#include "link/Rings.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <format>
#include <intrin.h>

namespace cpmc::link
{
namespace
{
constexpr uint64_t kHead = 0x00;
constexpr uint64_t kTail = 0x40;
constexpr uint64_t kData = 0x80;
static_assert(proto::kRenRingHeadOff == kHead && proto::kRenRingTailOff == kTail && proto::kRenRingDataOff == kData);
static_assert(proto::kColRingHeadOff == kHead && proto::kColRingTailOff == kTail && proto::kColRingDataOff == kData);
static_assert(proto::kInputRingHeadOff == kHead && proto::kInputRingTailOff == kTail && proto::kInputRingDataOff == kData);
static_assert(proto::kEventRingHeadOff == kHead && proto::kEventRingTailOff == kTail && proto::kEventRingDataOff == kData);
static_assert(proto::kRenPad == 0 && proto::kColPad == 0);

template<class T>
std::atomic_ref<T> At(uint8_t* aBase, uint64_t aOffset)
{
	return std::atomic_ref<T>(*reinterpret_cast<T*>(aBase + aOffset));
}

// A whole message, header and padding included: messages start 8-byte aligned.
uint64_t MessageBytes(uint64_t aPayloadBytes)
{
	return (sizeof(proto::ColMsgHeader) + aPayloadBytes + 7) & ~7ull;
}
}

void WriteSkyState(uint8_t* aBase, const proto::SkyState& aState)
{
	auto           seq = At<uint32_t>(aBase, proto::kOffSkyState);
	const uint32_t count = seq.load(std::memory_order_relaxed);
	seq.store(count + 1, std::memory_order_relaxed);
	std::atomic_thread_fence(std::memory_order_release);
	std::memcpy(aBase + proto::kOffSkyState + 4, reinterpret_cast<const uint8_t*>(&aState) + 4, sizeof(proto::SkyState) - 4);
	seq.store(count + 2, std::memory_order_release);
}

void WriteActorTable(uint8_t* aBase, std::span<const proto::ActorRecord> aRecords)
{
	auto           seq = At<uint32_t>(aBase, proto::kOffActorTable);
	const uint32_t count = seq.load(std::memory_order_relaxed);
	seq.store(count + 1, std::memory_order_relaxed);
	std::atomic_thread_fence(std::memory_order_release);
	auto* const    table = reinterpret_cast<proto::ActorTable*>(aBase + proto::kOffActorTable);
	const uint32_t records = uint32_t(std::min<size_t>(aRecords.size(), proto::kMaxActors));
	table->count = records;
	if (records != 0)
	{
		std::memcpy(table->actors, aRecords.data(), sizeof(proto::ActorRecord) * records);
	}
	seq.store(count + 2, std::memory_order_release);
}

std::optional<proto::McState> ReadMcState(const uint8_t* aBase)
{
	uint8_t* const source = const_cast<uint8_t*>(aBase) + proto::kOffMcState;  // only loaded from
	auto           seq = std::atomic_ref<uint32_t>(*reinterpret_cast<uint32_t*>(source));
	for (int attempt = 0; attempt < 64; ++attempt)
	{
		const uint32_t before = seq.load(std::memory_order_acquire);
		if (before & 1)
		{
			_mm_pause();
			continue;
		}
		proto::McState state;
		std::memcpy(&state, source, sizeof state);
		std::atomic_thread_fence(std::memory_order_acquire);
		if (seq.load(std::memory_order_relaxed) == before)
		{
			return state;
		}
	}
	return std::nullopt;
}

std::optional<proto::WorldEntities> ReadWorldEntities(const uint8_t* aBase)
{
	uint8_t* const source = const_cast<uint8_t*>(aBase) + proto::kOffWorldEntities;  // only loaded from
	auto           seq = std::atomic_ref<uint32_t>(*reinterpret_cast<uint32_t*>(source));
	for (int attempt = 0; attempt < 64; ++attempt)
	{
		const uint32_t before = seq.load(std::memory_order_acquire);
		if (before & 1)
		{
			_mm_pause();
			continue;
		}
		proto::WorldEntities table;
		std::memcpy(&table, source, sizeof table);
		std::atomic_thread_fence(std::memory_order_acquire);
		if (seq.load(std::memory_order_relaxed) == before)
		{
			return table;
		}
	}
	return std::nullopt;
}

DrainResult DrainByteRing(uint8_t* aRing, uint64_t aDataBytes, uint64_t aBudget, const std::function<bool(const RingMessage&)>& aVisit)
{
	DrainResult    result;
	auto           headRef = At<uint64_t>(aRing, kHead);
	auto           tailRef = At<uint64_t>(aRing, kTail);
	const uint8_t* data = aRing + kData;
	const uint64_t head = headRef.load(std::memory_order_acquire);
	uint64_t       tail = tailRef.load(std::memory_order_relaxed);
	const auto     resync = [&](std::string aWhy) {
        result.resynced = true;
        result.why = std::move(aWhy);
        tail = head;
	};
	if (head < tail || head - tail > aDataBytes || tail % 8 != 0)
	{
		resync(std::format("head {} and tail {} don't fit a ring of {} bytes", head, tail, aDataBytes));
	}
	while (tail < head && result.takenBytes < aBudget)
	{
		const uint64_t position = tail % aDataBytes;
		const uint64_t waiting = head - tail;
		if (waiting < sizeof(proto::ColMsgHeader) || aDataBytes - position < sizeof(proto::ColMsgHeader))
		{
			resync(std::format("no room for a message header at {}", tail));
			break;
		}
		proto::ColMsgHeader header;
		std::memcpy(&header, data + position, sizeof header);
		if (header.type == proto::kRenPad)
		{
			const uint64_t skip = aDataBytes - position;
			if (skip > waiting)
			{
				resync(std::format("a pad at {} goes past the head", tail));
				break;
			}
			tail += skip;
			result.bytes += skip;
			continue;
		}
		const uint64_t bytes = MessageBytes(header.payloadBytes);
		if (bytes > aDataBytes - position || bytes > waiting)
		{
			resync(std::format("a message of type {} at {} claims {} payload bytes", header.type, tail, header.payloadBytes));
			break;
		}
		const bool taken = aVisit({ header.type, { data + position + sizeof header, header.payloadBytes } });
		++result.messages;
		tail += bytes;
		result.bytes += bytes;
		if (taken)
		{
			result.takenBytes += bytes;
		}
	}
	tailRef.store(tail, std::memory_order_release);
	return result;
}

bool WriteByteRing(uint8_t* aRing, uint64_t aDataBytes, uint32_t aType, std::span<const uint8_t> aPayload)
{
	const uint64_t bytes = MessageBytes(aPayload.size());
	if (bytes > aDataBytes / 2)
	{
		return false;
	}
	auto           headRef = At<uint64_t>(aRing, kHead);
	auto           tailRef = At<uint64_t>(aRing, kTail);
	uint8_t*       data = aRing + kData;
	uint64_t       head = headRef.load(std::memory_order_relaxed);
	const uint64_t tail = tailRef.load(std::memory_order_acquire);
	uint64_t       position = head % aDataBytes;
	const uint64_t pad = position + bytes > aDataBytes ? aDataBytes - position : 0;
	if (head - tail > aDataBytes || aDataBytes - (head - tail) < bytes + pad)
	{
		return false;
	}
	if (pad)
	{
		const proto::ColMsgHeader padHeader{ proto::kColPad, 0 };
		std::memcpy(data + position, &padHeader, sizeof padHeader);
		head += pad;
		position = 0;
	}
	const proto::ColMsgHeader header{ aType, uint32_t(aPayload.size()) };
	std::memcpy(data + position, &header, sizeof header);
	if (!aPayload.empty())
	{
		std::memcpy(data + position + sizeof header, aPayload.data(), aPayload.size());
	}
	headRef.store(head + bytes, std::memory_order_release);
	return true;
}

bool PushInput(uint8_t* aRing, const proto::InputEvent& aEvent)
{
	auto           headRef = At<uint64_t>(aRing, kHead);
	auto           tailRef = At<uint64_t>(aRing, kTail);
	const uint64_t head = headRef.load(std::memory_order_relaxed);
	const uint64_t tail = tailRef.load(std::memory_order_acquire);
	if (head - tail >= proto::kInputRingEntries)
	{
		return false;
	}
	std::memcpy(aRing + kData + (head & (proto::kInputRingEntries - 1)) * sizeof(proto::InputEvent), &aEvent, sizeof aEvent);
	headRef.store(head + 1, std::memory_order_release);
	return true;
}

uint32_t DrainEvents(uint8_t* aRing, const std::function<void(const proto::McEvent&)>& aVisit)
{
	auto           headRef = At<uint64_t>(aRing, kHead);
	auto           tailRef = At<uint64_t>(aRing, kTail);
	const uint64_t head = headRef.load(std::memory_order_acquire);
	uint64_t       tail = tailRef.load(std::memory_order_relaxed);
	if (tail > head)
	{
		tail = head;
	}
	if (head - tail > proto::kEventRingEntries)
	{
		tail = head - proto::kEventRingEntries;
	}
	uint32_t count = 0;
	for (; tail < head; ++tail, ++count)
	{
		proto::McEvent event;
		std::memcpy(&event, aRing + kData + (tail & (proto::kEventRingEntries - 1)) * sizeof(proto::McEvent), sizeof event);
		aVisit(event);
	}
	tailRef.store(tail, std::memory_order_release);
	return count;
}
}
