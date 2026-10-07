#include "Harness.h"

#include "input/Cursor.h"

using namespace cpmc::input;

namespace
{
// The user's: a 1280 by 720 HUD image drawn at twice its size on a 2560 by 1440 back buffer.
Cursor OnTheUsersScreen()
{
	Cursor cursor;
	cursor.SetArea(1280, 720, 2);
	return cursor;
}

bool At(const Pixel& aPixel, uint32_t aX, uint32_t aY)
{
	return aPixel.x == aX && aPixel.y == aY;
}
}

TEST(CursorFollowsWindowsPointerSpeed)
{
	// Windows' factors with "Enhance pointer precision" off, by speed 1 to 20.
	const double factors[] = { 1.0 / 32, 1.0 / 16, 1.0 / 8, 2.0 / 8, 3.0 / 8, 4.0 / 8, 5.0 / 8, 6.0 / 8, 7.0 / 8, 1.0,
		                       1.25,     1.5,      1.75,    2.0,     2.25,    2.5,     2.75,    3.0,     3.25,    3.5 };
	for (int speed = 1; speed <= 20; ++speed)
	{
		CHECK_EQ(PointerFactor(speed), factors[speed - 1]);
	}
	CHECK_EQ(PointerFactor(0), 1.0);  // out of range: the default's
	CHECK_EQ(PointerFactor(21), 1.0);
}

TEST(CursorStartsAtTheCentreAsAScreenOpens)
{
	Cursor cursor = OnTheUsersScreen();
	cursor.SetScreen(true);
	CHECK(At(cursor.Image(), 640, 360));  // as Minecraft puts its own position: the window's size over 2
	CHECK(At(cursor.Back(), 1280, 720));
	cursor.Move(100, 50, 1.0);
	cursor.SetScreen(true);  // still open, or one screen straight after another: it stays
	CHECK(At(cursor.Image(), 690, 385));
	cursor.SetScreen(false);
	CHECK(At(cursor.Image(), 690, 385));  // closed: it stays where it was, unseen
	cursor.SetScreen(true);
	CHECK(At(cursor.Image(), 640, 360));
	Cursor odd;
	odd.SetArea(1281, 721, 1);
	odd.SetScreen(true);
	CHECK(At(odd.Image(), 640, 360));  // Minecraft's 640.5 and 360.5, in whole pixels
}

TEST(CursorMovesInBackBufferPixelsAndCarriesTheFractions)
{
	Cursor cursor = OnTheUsersScreen();
	cursor.SetScreen(true);
	CHECK(!cursor.Move(1, 0, 0.75).has_value());  // 1280.75: still image pixel 640
	CHECK(!cursor.Move(1, 0, 0.75).has_value());  // 1281.5
	const auto moved = cursor.Move(1, 0, 0.75);   // 1282.25: pixel 641
	CHECK(moved.has_value() && At(*moved, 641, 360));
	CHECK(At(cursor.Back(), 1282, 720));
	const auto down = cursor.Move(-3, 4, 0.75);  // back to 1280, and 723
	CHECK(down.has_value() && At(*down, 640, 361));
	CHECK(!cursor.Move(0, 0, 0.75).has_value());
}

TEST(CursorStaysInTheImage)
{
	Cursor cursor = OnTheUsersScreen();
	cursor.SetScreen(true);
	const auto corner = cursor.Move(-100000, -100000, 1.0);
	CHECK(corner.has_value() && At(*corner, 0, 0));
	const auto far = cursor.Move(1000000, 1000000, 1.0);
	CHECK(far.has_value() && At(*far, 1279, 719));
	CHECK(cursor.Back().x < 2560 && cursor.Back().y < 1440);
	cursor.SetArea(640, 360, 4);  // hudscale 4: the same screen, a smaller image
	CHECK(At(cursor.Image(), 639, 359));
	cursor.SetArea(320, 180, 4);  // and a smaller screen
	CHECK(At(cursor.Image(), 319, 179));
	CHECK(At(cursor.Back(), 1279, 719));
}

TEST(CursorWithoutAnImageGoesNowhere)
{
	Cursor cursor;
	CHECK(!cursor.HasArea());
	cursor.SetScreen(true);
	CHECK(!cursor.Move(10, 10, 1.0).has_value());
	CHECK(At(cursor.Image(), 0, 0));
	cursor.SetArea(0, 0, 2);  // Minecraft has sent no HUD image yet
	CHECK(!cursor.HasArea());
	cursor.SetArea(1280, 720, 0);  // a scale of 0 counts as 1
	CHECK(cursor.HasArea());
	const auto moved = cursor.Move(5, 5, 1.0);
	CHECK(moved.has_value() && At(*moved, 5, 5));
}
