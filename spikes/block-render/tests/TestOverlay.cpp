#include "Harness.h"

#include "link/Overlay.h"

#include <skycraft_protocol.h>

#include <cstring>
#include <vector>

using namespace cpmc::link;
namespace proto = skycraft::proto;

namespace
{
constexpr uint64_t kSlotBytes = 64;  // 4 x 4 pixels, small for the tests

// The overlay's control word, three slot headers and three small slots of pixels, with Minecraft's writer as
// SkyCraft's SkyLink.publishOverlay has it: its back slot starts at 1.
struct Triple
{
	Triple() : words(512) {}
	uint8_t*  Control() { return reinterpret_cast<uint8_t*>(words.data()); }
	uint8_t*  Headers() { return Control() + 64; }
	uint8_t*  Pixels() { return Control() + 64 + 3 * sizeof(proto::OverlaySlotHdr); }
	uint32_t& State() { return *reinterpret_cast<uint32_t*>(Control()); }

	void Publish(uint32_t aWidth, uint32_t aHeight, uint64_t aFrameId, uint8_t aFill)
	{
		proto::OverlaySlotHdr header{};
		header.width = aWidth;
		header.height = aHeight;
		header.flags = 1;  // bottom-up, as Minecraft's readback is
		header.frameId = aFrameId;
		std::memcpy(Headers() + back * sizeof header, &header, sizeof header);
		std::memset(Pixels() + back * kSlotBytes, aFill, kSlotBytes);
		const uint32_t old = State();
		State() = back | proto::kOverlayDirty;
		back = old & 3;
	}

	std::vector<uint64_t> words;
	uint32_t              back = 1;
};
}

TEST(OverlayNothingUntilPublished)
{
	Triple        triple;
	OverlayReader reader(triple.Control(), triple.Headers(), triple.Pixels(), kSlotBytes);
	CHECK(!reader.Acquire().has_value());
}

TEST(OverlayTakesTheNewestFrame)
{
	Triple        triple;
	OverlayReader reader(triple.Control(), triple.Headers(), triple.Pixels(), kSlotBytes);
	triple.Publish(4, 4, 5, 0xAA);
	const auto first = reader.Acquire();
	CHECK(first.has_value());
	CHECK_EQ(first->width, 4u);
	CHECK_EQ(first->height, 4u);
	CHECK(first->bottomUp);
	CHECK_EQ(first->frameId, 5ull);
	CHECK(first->pixels == triple.Pixels() + 1 * kSlotBytes);  // Minecraft's first back slot
	CHECK_EQ(int(first->pixels[0]), 0xAA);
	CHECK_EQ(triple.State(), 2u);  // our old front went into the middle, unread bit clear
	CHECK(!reader.Acquire().has_value());
	triple.Publish(2, 2, 6, 0xBB);  // into slot 0, the back slot Minecraft got from its first swap
	triple.Publish(4, 2, 7, 0xCC);  // and again, into slot 2: only the newest is read
	const auto newest = reader.Acquire();
	CHECK(newest.has_value());
	CHECK_EQ(newest->frameId, 7ull);
	CHECK(newest->pixels == triple.Pixels() + 2 * kSlotBytes);
	CHECK_EQ(int(newest->pixels[0]), 0xCC);
}

TEST(OverlayResetStartsOver)
{
	Triple        triple;
	OverlayReader reader(triple.Control(), triple.Headers(), triple.Pixels(), kSlotBytes);
	triple.Publish(4, 4, 1, 1);
	reader.Acquire();
	reader.Reset();
	CHECK_EQ(triple.State(), 0u);
	triple.back = 1;  // Minecraft's writer starts over too
	triple.Publish(4, 4, 2, 2);
	const auto frame = reader.Acquire();
	CHECK(frame.has_value());
	CHECK(frame->pixels == triple.Pixels() + 1 * kSlotBytes);
	CHECK_EQ(triple.State(), 2u);
}

TEST(OverlayRefusesBadSlotsAndSizes)
{
	Triple        triple;
	OverlayReader reader(triple.Control(), triple.Headers(), triple.Pixels(), kSlotBytes);
	triple.State() = 3 | proto::kOverlayDirty;
	CHECK(!reader.Acquire().has_value());
	CHECK_EQ(reader.Refused(), 1ull);
	CHECK_EQ(triple.State(), 0u);  // reset
	triple.back = 1;
	triple.Publish(5000, 4, 1, 0);
	CHECK(!reader.Acquire().has_value());
	triple.Publish(8, 8, 2, 0);  // 256 bytes: more than its slot
	CHECK(!reader.Acquire().has_value());
	CHECK_EQ(reader.Refused(), 3ull);
}
