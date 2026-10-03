#include "Harness.h"

#include "input/BuildMode.h"

#include <Windows.h>

using namespace cpmc::input;

namespace
{
// Linked, V found, Minecraft in its world, and the game in front: build mode can go on.
Situation Ready()
{
	return { true, true, true, true, true };
}

RawKey Press(uint16_t aScanCode, bool aExtended = false)
{
	return { aScanCode, aExtended, false };
}

RawKey Release(uint16_t aScanCode, bool aExtended = false)
{
	return { aScanCode, aExtended, true };
}

constexpr uint16_t kF8 = 0x42;

// Build mode, turned on with F8.
BuildMode On()
{
	BuildMode mode;
	mode.Key(Press(kF8), Ready());
	mode.Key(Release(kF8), Ready());
	return mode;
}

bool Passes(const Decision& aDecision)
{
	return aDecision.hideButtons == 0 && !aDecision.hideKey && aDecision.forwarded == 0 && !aDecision.releaseAll;
}

bool Is(const proto::InputEvent& aEvent, uint16_t aType, uint16_t aCode, int32_t aA)
{
	return aEvent.type == aType && aEvent.code == aCode && aEvent.a == aA;
}
}

TEST(BuildModeStartsOffAndPassesEverything)
{
	BuildMode mode;
	CHECK(!mode.On());
	CHECK_STR(mode.Why(), "F8 turns it on");
	CHECK(!mode.Slot());
	CHECK(Passes(mode.Mouse({ RI_MOUSE_LEFT_BUTTON_DOWN, 0 }, Ready())));
	CHECK(Passes(mode.Mouse({ RI_MOUSE_LEFT_BUTTON_UP, 0 }, Ready())));
	CHECK(Passes(mode.Mouse({ RI_MOUSE_WHEEL, 120 }, Ready())));
	CHECK(Passes(mode.Mouse({ RI_MOUSE_BUTTON_4_DOWN, 0 }, Ready())));
	CHECK(Passes(mode.Key(Press(0x04), Ready())));  // 3
	CHECK(Passes(mode.Key(Press(0x01), Ready())));  // Esc
	CHECK_EQ(mode.Forwarded(), 0ull);
	CHECK_EQ(mode.Hidden(), 0ull);
}

TEST(BuildModeTogglesOnF8)
{
	BuildMode      mode;
	const Decision on = mode.Key(Press(kF8), Ready());
	CHECK(mode.On());
	CHECK(on.hideKey);  // F8 is build mode's alone
	CHECK(!on.releaseAll && on.forwarded == 0 && !on.requestLink);
	CHECK(mode.Key(Press(kF8), Ready()).hideKey);  // a repeat: hidden, and nothing else
	CHECK(mode.On());
	CHECK(mode.Key(Release(kF8), Ready()).hideKey);
	const Decision off = mode.Key(Press(kF8), Ready());
	CHECK(!mode.On());
	CHECK(off.hideKey && off.releaseAll);  // Minecraft lets go of everything
	CHECK_STR(mode.Why(), "F8 turned it off");
	CHECK(!mode.Key(Press(kF8, true), Ready()).hideKey);  // an extended 0x42 isn't F8
}

TEST(BuildModeF8LinksFirst)
{
	BuildMode mode;
	Situation unlinked = Ready();
	unlinked.linked = false;
	unlinked.inGame = false;
	const Decision first = mode.Key(Press(kF8), unlinked);
	CHECK(first.requestLink && first.hideKey);
	CHECK(!mode.On());
	CHECK(mode.TurnOn(Ready()));  // the main thread has linked
	CHECK(mode.On());
	CHECK(mode.Why().empty());
}

TEST(BuildModeGoesOnOnlyWithMinecraftInItsWorld)
{
	BuildMode mode;
	Situation situation = Ready();
	situation.minecraftInWorld = false;
	mode.Key(Press(kF8), situation);
	CHECK(!mode.On());
	CHECK_STR(mode.Why(), "Minecraft isn't in its world");
	situation.minecraft = false;
	CHECK(!mode.TurnOn(situation));
	CHECK_STR(mode.Why(), "Minecraft isn't there");
	situation = Ready();
	situation.inGame = false;
	CHECK(!mode.TurnOn(situation));
	CHECK_STR(mode.Why(), "V isn't found");
	CHECK(mode.TurnOn(Ready()));
}

TEST(BuildModeForwardsAndHidesTheButtons)
{
	BuildMode      mode = On();
	const Decision left = mode.Mouse({ RI_MOUSE_LEFT_BUTTON_DOWN, 0 }, Ready());
	CHECK_EQ(left.hideButtons, uint16_t(RI_MOUSE_LEFT_BUTTON_DOWN));
	CHECK_EQ(left.forwarded, size_t(1));
	CHECK(Is(left.forward[0], proto::kInMouseButton, 1, 1));
	CHECK(left.press);
	const Decision up = mode.Mouse({ RI_MOUSE_LEFT_BUTTON_UP, 0 }, Ready());
	CHECK(Is(up.forward[0], proto::kInMouseButton, 1, 0));
	CHECK(!up.press);
	// The right and middle buttons in one event: SDL numbers them 3 and 2.
	const Decision both = mode.Mouse({ RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_DOWN, 0 }, Ready());
	CHECK_EQ(both.hideButtons, uint16_t(RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_DOWN));
	CHECK_EQ(both.forwarded, size_t(2));
	CHECK(Is(both.forward[0], proto::kInMouseButton, 3, 1));
	CHECK(Is(both.forward[1], proto::kInMouseButton, 2, 1));
	CHECK(Passes(mode.Mouse({ RI_MOUSE_HWHEEL, 120 }, Ready())));  // the horizontal wheel stays the game's
	CHECK_STR(mode.Last(), "the middle button down");
}

TEST(BuildModeForwardsTheSideButtons)
{
	// Cyberpunk binds neither, and the user's Minecraft picks hotbar slots with them.
	BuildMode mode = On();
	mode.Key(Press(0x04), Ready());  // 3
	CHECK_EQ(mode.Slot().value_or(0), 3);
	const Decision back = mode.Mouse({ RI_MOUSE_BUTTON_4_DOWN, 0 }, Ready());
	CHECK_EQ(back.hideButtons, uint16_t(RI_MOUSE_BUTTON_4_DOWN));
	CHECK(Is(back.forward[0], proto::kInMouseButton, 4, 1));
	CHECK(!back.press);   // it changes no block: no latency to time
	CHECK(!mode.Slot());  // which slot it picks, if any, is Minecraft's binding
	const Decision both = mode.Mouse({ RI_MOUSE_BUTTON_4_UP | RI_MOUSE_BUTTON_5_DOWN, 0 }, Ready());
	CHECK_EQ(both.forwarded, size_t(2));
	CHECK(Is(both.forward[0], proto::kInMouseButton, 4, 0));
	CHECK(Is(both.forward[1], proto::kInMouseButton, 5, 1));
	CHECK_STR(mode.Last(), "mouse button 5 down");
	mode.Key(Press(0x0A), Ready());  // 9
	CHECK_EQ(mode.Slot().value_or(0), 9);
}

TEST(BuildModeForwardsTheWheelAndFollowsTheSlot)
{
	BuildMode      mode = On();
	const Decision three = mode.Key(Press(0x04), Ready());
	CHECK(three.hideKey);
	CHECK(Is(three.forward[0], proto::kInKey, 32, 1));  // SDL's 3
	CHECK(mode.Key(Release(0x04), Ready()).hideKey);
	CHECK_EQ(mode.Slot().value_or(0), 3);
	const Decision up = mode.Mouse({ RI_MOUSE_WHEEL, 120 }, Ready());
	CHECK_EQ(up.hideButtons, uint16_t(RI_MOUSE_WHEEL));
	CHECK(Is(up.forward[0], proto::kInScroll, 0, 120));
	CHECK_EQ(mode.Slot().value_or(0), 2);  // up is the slot to the left
	mode.Mouse({ RI_MOUSE_WHEEL, -120 }, Ready());
	CHECK_EQ(mode.Slot().value_or(0), 3);
	mode.Mouse({ RI_MOUSE_WHEEL, 360 }, Ready());  // three notches in one event move it one slot, as in Minecraft
	CHECK_EQ(mode.Slot().value_or(0), 2);
	mode.Mouse({ RI_MOUSE_WHEEL, 60 }, Ready());  // half a notch waits for the other half
	CHECK_EQ(mode.Slot().value_or(0), 2);
	mode.Mouse({ RI_MOUSE_WHEEL, 60 }, Ready());
	CHECK_EQ(mode.Slot().value_or(0), 1);
	mode.Mouse({ RI_MOUSE_WHEEL, 120 }, Ready());  // from 1 round to 9
	CHECK_EQ(mode.Slot().value_or(0), 9);
	mode.Mouse({ RI_MOUSE_WHEEL, 60 }, Ready());    // half a notch up waits,
	mode.Mouse({ RI_MOUSE_WHEEL, -120 }, Ready());  // but turning the other way starts over: a whole notch down
	CHECK_EQ(mode.Slot().value_or(0), 1);
	mode.Key(Press(0x0A), Ready());  // 9
	CHECK_EQ(mode.Slot().value_or(0), 9);
}

TEST(BuildModeTakesTheKeys1To9)
{
	BuildMode mode = On();
	CHECK(Is(mode.Key(Press(0x02), Ready()).forward[0], proto::kInKey, 30, 1));  // 1
	mode.Key(Press(0x0A), Ready());
	CHECK(Is(mode.Key(Release(0x0A), Ready()).forward[0], proto::kInKey, 38, 0));  // 9's release goes too
	CHECK(Passes(mode.Key(Release(0x05), Ready())));      // 4's release, its press never seen: the game's
	CHECK(Passes(mode.Key(Press(0x0B), Ready())));        // 0
	CHECK(Passes(mode.Key(Press(0x12), Ready())));        // E: the game's iconic weapon, still the game's
	CHECK(Passes(mode.Key(Press(0x02, true), Ready())));  // an extended 0x02 isn't 1
	CHECK(Passes(mode.Key(Press(0x4F), Ready())));        // the keypad's 1
	CHECK(mode.On());
}

TEST(BuildModeLeavesOnTheMenuKeys)
{
	for (const int key : { 0x01, 0x17, 0x32, 0x24, 0x19, 0x25, 0x23, 0x14 })  // Esc I M J P K H T
	{
		BuildMode      mode = On();
		const Decision decision = mode.Key(Press(uint16_t(key)), Ready());
		CHECK(!mode.On());
		CHECK(decision.releaseAll);
		CHECK(!decision.hideKey && decision.forwarded == 0);  // the game opens its menu
	}
	BuildMode mode = On();
	mode.Key(Press(0x17), Ready());
	CHECK_STR(mode.Why(), "I left it");
	CHECK(Passes(mode.Key(Release(0x17), Ready())));
	mode = On();
	CHECK(Passes(mode.Key(Press(0x40), Ready())));  // F6 alone
	CHECK(mode.On());
	mode.Key(Press(0x36), Ready());                    // the right Shift
	CHECK(mode.Key(Press(0x40), Ready()).releaseAll);  // Shift+F6: CET's overlay
	CHECK_STR(mode.Why(), "Shift+F6 left it");
	mode = On();
	mode.Key(Press(0x2A, true), Ready());  // the fake Shift Windows sends around some extended keys isn't Shift
	CHECK(Passes(mode.Key(Press(0x40), Ready())));
	CHECK(mode.On());
	mode.Key(Press(0x01, true), Ready());  // an extended 0x01 isn't Esc
	CHECK(mode.On());
}

TEST(BuildModeSendsAReleaseWhereItsPressWent)
{
	BuildMode mode;
	CHECK(Passes(mode.Mouse({ RI_MOUSE_RIGHT_BUTTON_DOWN, 0 }, Ready())));  // V aiming before build mode
	CHECK(Passes(mode.Key(Press(0x03), Ready())));                          // and 2 held
	mode.Key(Press(kF8), Ready());
	mode.Key(Release(kF8), Ready());
	CHECK(mode.On());
	CHECK(Passes(mode.Key(Press(0x03), Ready())));                        // 2's repeat: still the game's
	CHECK(Passes(mode.Mouse({ RI_MOUSE_RIGHT_BUTTON_UP, 0 }, Ready())));  // so V stops aiming
	CHECK(mode.Key(Press(0x02), Ready()).hideKey);                        // 1, pressed now: Minecraft's
	// Minecraft's press, then build mode ends: Minecraft lets go of everything, so its repeats and release go nowhere.
	mode.Mouse({ RI_MOUSE_LEFT_BUTTON_DOWN, 0 }, Ready());
	CHECK(mode.Key(Press(kF8), Ready()).releaseAll);
	const Decision release = mode.Mouse({ RI_MOUSE_LEFT_BUTTON_UP, 0 }, Ready());
	CHECK_EQ(release.hideButtons, uint16_t(RI_MOUSE_LEFT_BUTTON_UP));
	CHECK_EQ(release.forwarded, size_t(0));
	const Decision repeat = mode.Key(Press(0x02), Ready());
	CHECK(repeat.hideKey && repeat.forwarded == 0);
	CHECK(mode.Key(Release(0x02), Ready()).hideKey);
	CHECK(Passes(mode.Key(Press(0x02), Ready())));  // a new press, with build mode off: the game's
}

TEST(BuildModeForwardsNothingWhileTheGameIsntInFront)
{
	BuildMode mode = On();
	Situation away = Ready();
	away.inFront = false;
	CHECK(Passes(mode.Mouse({ RI_MOUSE_LEFT_BUTTON_DOWN, 0 }, away)));
	CHECK(Passes(mode.Mouse({ RI_MOUSE_WHEEL, 120 }, away)));
	CHECK(Passes(mode.Key(Press(0x02), away)));
	CHECK(Passes(mode.Key(Press(kF8), away)));
	CHECK(mode.On());  // an F8 meant for another window doesn't toggle it
}

TEST(BuildModeLetsGoWhenTheGameLosesTheFront)
{
	BuildMode mode;
	mode.Mouse({ RI_MOUSE_RIGHT_BUTTON_DOWN, 0 }, Ready());  // the game's
	mode.Key(Press(kF8), Ready());
	mode.Key(Release(kF8), Ready());
	mode.Mouse({ RI_MOUSE_LEFT_BUTTON_DOWN, 0 }, Ready());  // Minecraft's
	CHECK(mode.LostFront());  // on: Minecraft lets go of everything
	CHECK(mode.On());         // and build mode stays on for when the user comes back
	// Both were let go of in another window. Back in the game, the next presses are Minecraft's.
	const Decision again = mode.Mouse({ RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_LEFT_BUTTON_DOWN, 0 }, Ready());
	CHECK_EQ(again.forwarded, size_t(2));
	CHECK(Is(again.forward[1], proto::kInMouseButton, 3, 1));
	mode.Key(Press(kF8), Ready());  // off, with F8 held as the front goes
	CHECK(!mode.On());
	CHECK(!mode.LostFront());       // off: nothing to let go of
	mode.Key(Press(kF8), Ready());  // F8 was let go of elsewhere: this is a press, not a repeat
	CHECK(mode.On());
}

TEST(BuildModeCountsWhatItForwardsAndHides)
{
	BuildMode mode = On();  // F8's press and release: 2 hidden
	mode.Mouse({ RI_MOUSE_RIGHT_BUTTON_DOWN, 0 }, Ready());
	mode.Mouse({ RI_MOUSE_RIGHT_BUTTON_UP, 0 }, Ready());
	mode.Mouse({ RI_MOUSE_WHEEL, -120 }, Ready());
	CHECK_EQ(mode.Forwarded(), 3ull);
	CHECK_EQ(mode.Hidden(), 5ull);
	CHECK_STR(mode.Last(), "the wheel, -120");
	CHECK(mode.TurnOff("a build off request"));
	CHECK(!mode.TurnOff("again"));
	CHECK_STR(mode.Why(), "a build off request");
}

TEST(BuildModeGivesMinecraftTheMovementKeys)
{
	// W, A, S, D, Space, Left Shift and Left Ctrl as set-1 scan codes, and the SDL scancodes Minecraft gets for them.
	const std::pair<uint16_t, uint16_t> keys[] = { { 0x11, 26 }, { 0x1E, 4 },  { 0x1F, 22 }, { 0x20, 7 },
		                                           { 0x39, 44 }, { 0x2A, 225 }, { 0x1D, 224 } };
	BuildMode                           mode = On();
	for (const auto& [scanCode, sdl] : keys)
	{
		const Decision down = mode.Key(Press(scanCode), Ready());
		CHECK(down.hideKey && down.forwarded == 1 && Is(down.forward[0], proto::kInKey, sdl, 1));
		const Decision repeat = mode.Key(Press(scanCode), Ready());  // held: Windows repeats the press
		CHECK(repeat.hideKey && repeat.forwarded == 1 && Is(repeat.forward[0], proto::kInKey, sdl, 1));
		const Decision up = mode.Key(Release(scanCode), Ready());
		CHECK(up.hideKey && up.forwarded == 1 && Is(up.forward[0], proto::kInKey, sdl, 0));
	}
	CHECK_STR(mode.Last(), "Left Ctrl up");
	BuildMode off;  // outside build mode they're the game's
	for (const auto& [scanCode, sdl] : keys)
	{
		CHECK(Passes(off.Key(Press(scanCode), Ready())));
		CHECK(Passes(off.Key(Release(scanCode), Ready())));
	}
}

TEST(BuildModeHidesTheGamesCrouchKeys)
{
	// C toggles crouching and Right Ctrl holds it: hidden in build mode, so V can't crouch, and not forwarded.
	BuildMode mode = On();
	for (const RawKey& key : { Press(0x2E), Release(0x2E), Press(0x1D, true), Release(0x1D, true) })
	{
		const Decision decision = mode.Key(key, Ready());
		CHECK(decision.hideKey && decision.forwarded == 0);
	}
	BuildMode off;
	CHECK(Passes(off.Key(Press(0x2E), Ready())));
	CHECK(Passes(off.Key(Press(0x1D, true), Ready())));
}

TEST(BuildModeLeavesAMovementKeyHeldBeforeItWithTheGame)
{
	// W held when build mode starts: its repeats and its release are the game's, so the game lets go of it. Pressed
	// again, it's Minecraft's.
	BuildMode mode;
	CHECK(Passes(mode.Key(Press(0x11), Ready())));
	mode.Key(Press(kF8), Ready());
	CHECK(mode.On());
	CHECK(Passes(mode.Key(Press(0x11), Ready())));  // a repeat
	CHECK(Passes(mode.Key(Release(0x11), Ready())));
	const Decision again = mode.Key(Press(0x11), Ready());
	CHECK(again.hideKey && again.forwarded == 1);
}

TEST(BuildModeLeavesOnShiftF6WithLeftShiftForwarded)
{
	BuildMode      mode = On();
	const Decision shift = mode.Key(Press(0x2A), Ready());
	CHECK(shift.hideKey && shift.forwarded == 1);  // Minecraft's: in the user's instance it toggles sprint
	const Decision f6 = mode.Key(Press(0x40), Ready());
	CHECK(!mode.On());
	CHECK(f6.releaseAll && !f6.hideKey);  // and CET's overlay opens
	CHECK_STR(mode.Why(), "Shift+F6 left it");
	const Decision release = mode.Key(Release(0x2A), Ready());  // its press went to Minecraft, which let go of everything
	CHECK(release.hideKey && release.forwarded == 0);
}
