#include "link/Overlay.h"

#include <skycraft_protocol.h>

#include <atomic>
#include <cstring>

namespace cpmc::link
{
namespace proto = skycraft::proto;

OverlayReader::OverlayReader(uint8_t* aControl, uint8_t* aHeaders, uint8_t* aPixels, uint64_t aSlotBytes)
    : control_(aControl), headers_(aHeaders), pixels_(aPixels), slotBytes_(aSlotBytes)
{
}

void OverlayReader::Reset()
{
	std::atomic_ref<uint32_t>(*reinterpret_cast<uint32_t*>(control_)).store(0, std::memory_order_release);
	front_ = 2;
}

std::optional<HudFrame> OverlayReader::Acquire()
{
	auto state = std::atomic_ref<uint32_t>(*reinterpret_cast<uint32_t*>(control_));
	if (!(state.load(std::memory_order_acquire) & proto::kOverlayDirty))
	{
		return std::nullopt;
	}
	const uint32_t slot = state.exchange(front_, std::memory_order_acq_rel) & 3;
	if (slot > 2)
	{
		++refused_;
		Reset();
		return std::nullopt;
	}
	front_ = slot;
	proto::OverlaySlotHdr header;
	std::memcpy(&header, headers_ + slot * sizeof header, sizeof header);
	if (header.width == 0 || header.height == 0 || header.width > proto::kMaxOverlayW || header.height > proto::kMaxOverlayH ||
	    uint64_t(header.width) * header.height * 4 > slotBytes_)
	{
		++refused_;
		return std::nullopt;
	}
	return HudFrame{ header.width, header.height, (header.flags & 1) != 0, header.frameId, pixels_ + slot * slotBytes_ };
}
}
