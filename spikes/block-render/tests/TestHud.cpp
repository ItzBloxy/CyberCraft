#include "Harness.h"

#include "render/Hud.h"

using namespace cpmc;

TEST(HudWantedFollowsTheSetting)
{
	CHECK(!HudWanted(0, true, true));  // never
	CHECK(HudWanted(1, true, true));   // in build mode, the default
	CHECK(!HudWanted(1, false, true));
	CHECK(HudWanted(2, false, true));  // always
	CHECK(!HudWanted(2, true, false));  // but only with Minecraft in its world
	CHECK(!HudWanted(3, true, true));
}

TEST(HudViewportIsTheBackBufferOverTheScale)
{
	const HudSize half = HudViewport(2560, 1440, 2);
	CHECK_EQ(half.width, 1280u);
	CHECK_EQ(half.height, 720u);
	CHECK_EQ(HudViewport(2560, 1440, 1).width, 2560u);
	CHECK_EQ(HudViewport(2560, 1440, 4).height, 360u);
	CHECK_EQ(HudViewport(1366, 768, 4).width, 341u);  // rounded down: the last 2 columns show nothing
	CHECK_EQ(HudViewport(7680, 4320, 1).width, 3840u);  // at most a slot's size
	CHECK_EQ(HudViewport(7680, 4320, 1).height, 2160u);
	CHECK_EQ(HudViewport(0, 0, 2).width, 0u);  // the back buffer isn't known yet: Minecraft's window is left alone
	CHECK_EQ(HudViewport(2560, 1440, 3).width, 0u);  // 1, 2 or 4 only
}

TEST(HudScaleComesFromTheImage)
{
	CHECK_EQ(HudScaleFor(2560, 1440, 1280, 720), 2u);
	CHECK_EQ(HudScaleFor(1366, 768, 341, 192), 4u);
	CHECK_EQ(HudScaleFor(2560, 1440, 2560, 1440), 1u);
	CHECK_EQ(HudScaleFor(2560, 1440, 1280, 1440), 1u);  // the smaller of the two ways
	CHECK_EQ(HudScaleFor(1280, 720, 2560, 1440), 1u);   // an image bigger than the screen: cut off, never shrunk
	CHECK_EQ(HudScaleFor(2560, 1440, 0, 0), 1u);
}

TEST(HudTexelScalesAndFlips)
{
	// A 4 x 2 image at scale 2, rows bottom-up: the top left screen pixels show the image's last row.
	CHECK_EQ(*HudTexel(0, 0, 4, 2, 2, true), 4u);
	CHECK_EQ(*HudTexel(1, 1, 4, 2, 2, true), 4u);
	CHECK_EQ(*HudTexel(2, 0, 4, 2, 2, true), 5u);
	CHECK_EQ(*HudTexel(7, 3, 4, 2, 2, true), 3u);
	CHECK_EQ(*HudTexel(7, 3, 4, 2, 2, false), 7u);  // top first: no flip
	CHECK_EQ(*HudTexel(3, 1, 4, 2, 1, false), 7u);  // scale 1
}

TEST(HudTexelShowsNothingOutsideTheImage)
{
	CHECK(!HudTexel(8, 0, 4, 2, 2, true).has_value());
	CHECK(!HudTexel(0, 4, 4, 2, 2, false).has_value());
	CHECK(!HudTexel(0, 0, 0, 0, 2, false).has_value());
	CHECK(!HudTexel(0, 0, 4, 2, 0, false).has_value());
}

TEST(HudSlotsWriteTakeAndRead)
{
	HudSlots slots;
	CHECK(!slots.TakeNewest().has_value());  // nothing written yet
	CHECK_EQ(*slots.BeginWrite(0), 0u);
	slots.EndWrite(0);
	CHECK_EQ(*slots.TakeNewest(), 0u);
	CHECK(!slots.TakeNewest().has_value());  // each image once
	slots.Reading(0, 5);
	CHECK_EQ(*slots.BeginWrite(4), 1u);  // slot 0 is the GPU's until fence 5
	slots.EndWrite(1);
	CHECK_EQ(*slots.BeginWrite(4), 2u);
	slots.EndWrite(2);  // replaces slot 1, never taken, which is free again
	CHECK_EQ(*slots.TakeNewest(), 2u);
	CHECK_EQ(*slots.BeginWrite(4), 1u);
	slots.EndWrite(1);
	CHECK_EQ(*slots.BeginWrite(5), 0u);  // fence 5 has passed
}

TEST(HudSlotsAbandonAFailedWrite)
{
	HudSlots slots;
	CHECK_EQ(*slots.BeginWrite(0), 0u);
	slots.Abandon(0);  // its upload buffer couldn't be made
	CHECK(!slots.TakeNewest().has_value());
	CHECK_EQ(*slots.BeginWrite(0), 0u);  // free again
}

TEST(HudSlotsReplaceTheWaitingImageWhenTheGpuHasTheRest)
{
	HudSlots slots;
	for (uint32_t i = 0; i < 2; ++i)
	{
		const uint32_t slot = *slots.BeginWrite(0);
		slots.EndWrite(slot);
		slots.Reading(*slots.TakeNewest(), 10 + i);
	}
	CHECK_EQ(*slots.BeginWrite(0), 2u);
	slots.EndWrite(2);
	CHECK_EQ(*slots.BeginWrite(0), 2u);  // only the waiting image's slot is left: the newer one replaces it
	CHECK(!slots.TakeNewest().has_value());  // not while it's being written
	CHECK(!slots.BeginWrite(0).has_value());  // and nothing else is free
	slots.EndWrite(2);
	CHECK_EQ(*slots.TakeNewest(), 2u);
}
