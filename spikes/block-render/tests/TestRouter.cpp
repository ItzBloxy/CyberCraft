#include "Harness.h"

#include "input/Router.h"

#include <Windows.h>

#include <cstring>
#include <vector>

using namespace cpmc;
using namespace cpmc::input;

namespace
{
bool g_inFront = true;
int  g_speed = 8;  // the user's

bool FakeFront()
{
	return g_inFront;
}

PointerSpeed FakeSpeed()
{
	return { g_speed, false };
}

// The mapping's layout in plain memory, with a link on it: what the router forwards lands in its input ring.
struct Mapping
{
	Mapping()
	{
		base = static_cast<uint8_t*>(VirtualAlloc(nullptr, proto::kMappingBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
		link = new link::Link(base);
	}
	~Mapping()
	{
		delete link;
		VirtualFree(base, 0, MEM_RELEASE);
	}

	std::vector<proto::InputEvent> Inputs()  // everything forwarded since the last call
	{
		uint8_t* const                 ring = base + proto::kOffInputRing;
		const uint64_t                 head = *reinterpret_cast<uint64_t*>(ring);
		uint64_t&                      tail = *reinterpret_cast<uint64_t*>(ring + 0x40);
		std::vector<proto::InputEvent> events;
		for (; tail < head; ++tail)
		{
			proto::InputEvent event;
			std::memcpy(&event, ring + 0x80 + (tail % proto::kInputRingEntries) * sizeof event, sizeof event);
			events.push_back(event);
		}
		return events;
	}

	uint8_t*    base = nullptr;
	link::Link* link = nullptr;
};

// What the link says once a frame: Minecraft there and in its world, and V found while linked.
link::Link::Status Linked()
{
	link::Link::Status state;
	state.minecraft = true;
	state.inWorld = true;
	state.inGame = true;
	return state;
}

// What the link says with a Minecraft screen open.
link::Link::Status WithAScreen()
{
	link::Link::Status state = Linked();
	state.screenOpen = true;
	return state;
}

// A router on a link, in build mode, with the user's 1280 by 720 HUD image drawn twice its size.
struct Building
{
	Building() : router(log, &FakeFront, &FakeSpeed)
	{
		g_inFront = true;
		g_speed = 8;
		router.SetHudCursor(&hud);
		router.SetHudArea(1280, 720, 2);
		router.SetLink(mapping.link);
		router.Update(true, Linked());
		CHECK(!router.TurnOn());
	}

	LogQueue  log;
	Mapping   mapping;
	HudCursor hud;
	Router    router;
};

// A mouse event as GetRawInputData copies it: the header, then RAWMOUSE.
std::vector<uint8_t> Mouse(USHORT aButtons, SHORT aWheel = 0, LONG aX = 0, LONG aY = 0)
{
	RAWINPUT input{};
	input.header.dwType = RIM_TYPEMOUSE;
	input.header.dwSize = sizeof(RAWINPUTHEADER) + sizeof(RAWMOUSE);
	input.data.mouse.usButtonFlags = aButtons;
	input.data.mouse.usButtonData = USHORT(aWheel);
	input.data.mouse.lLastX = aX;
	input.data.mouse.lLastY = aY;
	std::vector<uint8_t> bytes(input.header.dwSize);
	std::memcpy(bytes.data(), &input, bytes.size());
	return bytes;
}

// A key event: the header, then RAWKEYBOARD, with the virtual key the game reads.
std::vector<uint8_t> Key(USHORT aScanCode, USHORT aVirtualKey, bool aUp = false)
{
	RAWINPUT input{};
	input.header.dwType = RIM_TYPEKEYBOARD;
	input.header.dwSize = sizeof(RAWINPUTHEADER) + sizeof(RAWKEYBOARD);
	input.data.keyboard.MakeCode = aScanCode;
	input.data.keyboard.Flags = aUp ? RI_KEY_BREAK : RI_KEY_MAKE;
	input.data.keyboard.VKey = aVirtualKey;
	input.data.keyboard.Message = aUp ? WM_KEYUP : WM_KEYDOWN;
	std::vector<uint8_t> bytes(input.header.dwSize);
	std::memcpy(bytes.data(), &input, bytes.size());
	return bytes;
}

RAWMOUSE MouseOf(const std::vector<uint8_t>& aBytes)
{
	RAWMOUSE mouse;
	std::memcpy(&mouse, aBytes.data() + sizeof(RAWINPUTHEADER), sizeof mouse);
	return mouse;
}

RAWKEYBOARD KeyOf(const std::vector<uint8_t>& aBytes)
{
	RAWKEYBOARD key;
	std::memcpy(&key, aBytes.data() + sizeof(RAWINPUTHEADER), sizeof key);
	return key;
}

void Inspect(Router& aRouter, std::vector<uint8_t>& aEvent)
{
	aRouter.Inspect(aEvent.data(), uint32_t(aEvent.size()));
}

bool Logged(LogQueue& aLog, const std::string& aLine)
{
	for (const std::string& line : aLog.Drain())
	{
		if (line == aLine)
		{
			return true;
		}
	}
	return false;
}
}

TEST(RouterHidesAButtonAndKeepsItsMovement)
{
	Building building;
	auto     click = Mouse(RI_MOUSE_LEFT_BUTTON_DOWN, 0, 5, -3);
	Inspect(building.router, click);
	const RAWMOUSE mouse = MouseOf(click);
	CHECK_EQ(mouse.usButtonFlags, USHORT(0));  // the game sees no press
	CHECK_EQ(mouse.lLastX, LONG(5));           // but the look keeps working
	CHECK_EQ(mouse.lLastY, LONG(-3));
	const auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].type == proto::kInMouseButton && inputs[0].code == 1 && inputs[0].a == 1);
	CHECK(building.router.TakePress().has_value());  // a latency mark
	CHECK(!building.router.TakePress());
	// The release, with the horizontal wheel in the same event: that stays the game's.
	auto release = Mouse(RI_MOUSE_LEFT_BUTTON_UP | RI_MOUSE_HWHEEL, 120);
	Inspect(building.router, release);
	CHECK_EQ(MouseOf(release).usButtonFlags, USHORT(RI_MOUSE_HWHEEL));
	auto wheel = Mouse(RI_MOUSE_WHEEL, -120);
	Inspect(building.router, wheel);
	CHECK_EQ(MouseOf(wheel).usButtonFlags, USHORT(0));
	auto side = Mouse(RI_MOUSE_BUTTON_5_DOWN);  // the user's Minecraft picks hotbar slots with the side buttons
	Inspect(building.router, side);
	CHECK_EQ(MouseOf(side).usButtonFlags, USHORT(0));
	const auto rest = building.mapping.Inputs();
	CHECK(rest.size() == 3 && rest[0].a == 0 && rest[1].type == proto::kInScroll && rest[1].a == -120 && rest[2].code == 5);
}

TEST(RouterMakesAKeyANoOp)
{
	Building building;
	auto     one = Key(0x02, '1');
	Inspect(building.router, one);
	const RAWKEYBOARD key = KeyOf(one);
	CHECK_EQ(key.VKey, USHORT(0xFF));  // the game skips it
	CHECK_EQ(key.MakeCode, USHORT(0));
	CHECK_EQ(key.Message, UINT(WM_KEYDOWN));
	const auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].type == proto::kInKey && inputs[0].code == 30 && inputs[0].a == 1);
	CHECK(!building.router.TakePress());  // only buttons are marks
	auto f = Key(0x21, 'F');
	Inspect(building.router, f);
	CHECK_EQ(KeyOf(f).VKey, USHORT('F'));  // V still interacts
	CHECK(building.mapping.Inputs().empty());
}

TEST(RouterLeavesEverythingElseAlone)
{
	Building building;
	auto     move = Mouse(0, 0, 7, 7);
	Inspect(building.router, move);
	CHECK_EQ(building.router.Report().read, 1ull);
	// Too short for its type: left as it is, and not read.
	auto short1 = Mouse(RI_MOUSE_LEFT_BUTTON_DOWN);
	building.router.Inspect(short1.data(), uint32_t(sizeof(RAWINPUTHEADER) + sizeof(RAWMOUSE) - 1));
	CHECK_EQ(MouseOf(short1).usButtonFlags, USHORT(RI_MOUSE_LEFT_BUTTON_DOWN));
	auto short2 = Key(0x02, '1');
	building.router.Inspect(short2.data(), uint32_t(sizeof(RAWINPUTHEADER) + sizeof(RAWKEYBOARD) - 1));
	CHECK_EQ(KeyOf(short2).VKey, USHORT('1'));
	building.router.Inspect(short2.data(), 0);
	building.router.Inspect(nullptr, 64);
	// Another device's type, with a press's bytes in it.
	auto hid = Mouse(RI_MOUSE_LEFT_BUTTON_DOWN);
	hid[0] = RIM_TYPEHID;
	Inspect(building.router, hid);
	CHECK_EQ(MouseOf(hid).usButtonFlags, USHORT(RI_MOUSE_LEFT_BUTTON_DOWN));
	CHECK(building.mapping.Inputs().empty());
	CHECK_EQ(building.router.Report().read, 1ull);
}

TEST(RouterPassesEverythingOutsideBuildMode)
{
	LogQueue log;
	Mapping  mapping;
	Router   router(log, &FakeFront);
	g_inFront = true;
	router.SetLink(mapping.link);
	router.Update(true, Linked());
	auto click = Mouse(RI_MOUSE_RIGHT_BUTTON_DOWN);
	Inspect(router, click);
	CHECK_EQ(MouseOf(click).usButtonFlags, USHORT(RI_MOUSE_RIGHT_BUTTON_DOWN));
	auto three = Key(0x04, '3');
	Inspect(router, three);
	CHECK_EQ(KeyOf(three).VKey, USHORT('3'));
	CHECK(mapping.Inputs().empty());
	CHECK(!router.On());
	auto f8 = Key(0x42, VK_F8);
	Inspect(router, f8);  // F8 turns it on, and is hidden
	CHECK(router.On());
	CHECK_EQ(KeyOf(f8).VKey, USHORT(0xFF));
	CHECK(Logged(log, "build mode on"));
}

TEST(RouterTakesF8sLinkRequest)
{
	LogQueue log;
	Mapping  mapping;
	Router   router(log, &FakeFront);
	g_inFront = true;
	router.SetLink(mapping.link);
	link::Link::Status state = Linked();
	state.inGame = false;
	router.Update(false, state);
	auto f8 = Key(0x42, VK_F8);
	Inspect(router, f8);
	CHECK(!router.On());
	CHECK(router.TakeLinkRequest());
	CHECK(!router.TakeLinkRequest());
	router.Update(true, Linked());  // the main thread linked
	CHECK(!router.TurnOn());
	CHECK(router.On());
	router.TurnOff("a build off request");
	state.inWorld = false;
	router.Update(true, state);
	CHECK_STR(router.TurnOn().value_or("on"), "Minecraft isn't in its world");
	CHECK(Logged(log, "build mode stays off: Minecraft isn't in its world"));
}

TEST(RouterEndsBuildModeWhenMinecraftOrVGoes)
{
	struct Case
	{
		bool        linked, minecraft, inWorld, inGame;
		const char* why;
	};
	for (const Case& c : { Case{ true, false, false, false, "Minecraft has gone" },
	                       Case{ true, true, false, true, "Minecraft left its world" }, Case{ false, true, true, false, "unlinked" },
	                       Case{ true, true, true, false, "V isn't found" } })
	{
		Building           building;
		link::Link::Status state;
		state.minecraft = c.minecraft;
		state.inWorld = c.inWorld;
		state.inGame = c.inGame;
		building.router.Update(c.linked, state);
		CHECK(!building.router.On());
		CHECK_STR(building.router.Report().why, c.why);
		const auto inputs = building.mapping.Inputs();
		CHECK(inputs.size() == 1 && inputs[0].type == proto::kInReleaseAll);  // Minecraft lets go of everything
	}
}

TEST(RouterLetsGoWhenTheGameLosesTheFront)
{
	Building building;
	auto     use = Mouse(RI_MOUSE_RIGHT_BUTTON_DOWN);
	Inspect(building.router, use);
	building.mapping.Inputs();
	building.router.FrontLost();
	const auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].type == proto::kInReleaseAll);
	CHECK(building.router.On());
	CHECK(Logged(building.log, "the game lost the front in build mode: Minecraft lets go of everything"));
	building.router.TurnOff("a build off request");
	building.mapping.Inputs();
	building.router.FrontLost();
	CHECK(building.mapping.Inputs().empty());  // off: nothing to let go of
	// A click in another window that reached the game anyway isn't Minecraft's.
	CHECK(!building.router.TurnOn());
	g_inFront = false;
	auto away = Mouse(RI_MOUSE_LEFT_BUTTON_DOWN);
	Inspect(building.router, away);
	CHECK_EQ(MouseOf(away).usButtonFlags, USHORT(RI_MOUSE_LEFT_BUTTON_DOWN));
	CHECK(building.mapping.Inputs().empty());
	g_inFront = true;
}

TEST(RouterPausesBuildModeForCetsOverlay)
{
	Building building;
	auto     shift = Key(0x2A, VK_SHIFT);
	Inspect(building.router, shift);
	building.mapping.Inputs();
	auto f6 = Key(0x40, VK_F6);
	Inspect(building.router, f6);
	CHECK_EQ(KeyOf(f6).VKey, USHORT(VK_F6));  // CET's overlay opens
	auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].type == proto::kInReleaseAll);  // and Minecraft lets go of everything
	CHECK(building.router.On());
	CHECK(building.router.Report().paused);
	CHECK(Logged(building.log, "build mode paused: CET's overlay is open"));
	auto click = Mouse(RI_MOUSE_LEFT_BUTTON_DOWN);
	Inspect(building.router, click);
	CHECK_EQ(MouseOf(click).usButtonFlags, USHORT(RI_MOUSE_LEFT_BUTTON_DOWN));  // the overlay's click
	CHECK(building.mapping.Inputs().empty());
	CHECK(!Logged(building.log, "build mode goes on: CET's overlay closed"));  // not while it's open
	auto move = Mouse(0, 0, 9, 0);
	Inspect(building.router, move);
	CHECK_EQ(building.router.TakeMouseX(), 0ll);  // the overlay's cursor turns nothing
	building.router.SetOverlay(false);  // the overlay says it closed
	CHECK(!building.router.Report().paused);
	CHECK(Logged(building.log, "build mode goes on: CET's overlay closed"));
	Inspect(building.router, move);
	CHECK_EQ(building.router.TakeMouseX(), 9ll);
	building.router.SetOverlay(true);  // and opened
	inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].type == proto::kInReleaseAll);
	CHECK(building.router.Report().paused);
}

TEST(RouterPushesNothingOnceTheLinkIsGone)
{
	Building building;
	building.router.SetLink(nullptr);  // the plugin unloading
	auto click = Mouse(RI_MOUSE_LEFT_BUTTON_DOWN);
	Inspect(building.router, click);
	building.router.TurnOff("the plugin unloads");
	CHECK(building.mapping.Inputs().empty());
}

TEST(RouterReportsBuildMode)
{
	Building building;
	auto     three = Key(0x04, '3');
	Inspect(building.router, three);
	auto up = Mouse(RI_MOUSE_WHEEL, 120, 1, 1);
	Inspect(building.router, up);
	const InputReport on = building.router.Report();
	CHECK(on.on);
	CHECK_EQ(on.slot.value_or(0), 2);
	CHECK_EQ(on.read, 2ull);
	CHECK_EQ(on.forwarded, 2ull);
	CHECK_EQ(on.hidden, 2ull);
	CHECK_STR(on.last, "the wheel, 120");
	building.mapping.Inputs();
	auto esc = Key(0x01, VK_ESCAPE);
	Inspect(building.router, esc);
	CHECK_EQ(KeyOf(esc).VKey, USHORT(VK_ESCAPE));  // the game opens its menu
	const auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].type == proto::kInReleaseAll);  // and Minecraft lets go of everything
	CHECK(!building.router.On());
	CHECK_STR(building.router.Report().why, "Esc left it");
	CHECK(Logged(building.log, "build mode off: Esc left it"));
}

TEST(RouterSumsTheMouseMovementAlongX)
{
	// For the turn a teleport may have to add back (spike 5's "The teleport"), in or out of build mode.
	LogQueue log;
	Router   router(log, &FakeFront);
	auto     move = Mouse(0, 0, 7, 2);
	auto     click = Mouse(RI_MOUSE_LEFT_BUTTON_DOWN, 0, -3, 0);
	Inspect(router, move);
	Inspect(router, click);
	CHECK_EQ(router.TakeMouseX(), 4ll);
	CHECK_EQ(router.TakeMouseX(), 0ll);  // taken
	auto     absolute = Mouse(0, 0, 30000, 30000);  // a tablet's position, not a movement
	RAWMOUSE mouse = MouseOf(absolute);
	mouse.usFlags = MOUSE_MOVE_ABSOLUTE;
	std::memcpy(absolute.data() + sizeof(RAWINPUTHEADER), &mouse, sizeof mouse);
	Inspect(router, absolute);
	CHECK_EQ(router.TakeMouseX(), 0ll);
}

TEST(RouterKeepsBuildModeOffWhileVCantBePlaced)
{
	LogQueue log;
	Router   router(log, &FakeFront);
	router.Update(true, Linked(), "V is in a vehicle");
	CHECK_STR(router.TurnOn().value_or("on"), "V is in a vehicle");
	CHECK(Logged(log, "build mode stays off: V is in a vehicle"));
	router.Update(true, Linked(), "");
	CHECK(!router.TurnOn());
	CHECK(router.On());
}

TEST(RouterEndsBuildModeWhenVCantBePlaced)
{
	Building building;
	building.router.Update(true, Linked(), "V is in a workspot");
	CHECK(!building.router.On());
	CHECK_STR(building.router.Report().why, "V is in a workspot");
	const auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].type == proto::kInReleaseAll);
}

TEST(RouterSaysWhyVCantBePlacedWhenTheLinkFindsNoV)
{
	// In a car the camera isn't V's eye, so the link finds no V; V's own reason still comes first.
	Building           building;
	link::Link::Status state = Linked();
	state.inGame = false;
	building.router.Update(true, state, "V is in a vehicle");
	CHECK(!building.router.On());
	CHECK_STR(building.router.Report().why, "V is in a vehicle");
	CHECK_STR(building.router.TurnOn().value_or("on"), "V is in a vehicle");
	building.router.Update(true, state);
	CHECK_STR(building.router.TurnOn().value_or("on"), "V isn't found");
}

TEST(RouterEndsBuildModeWhenMinecraftsPlayerDies)
{
	Building           building;
	link::Link::Status state = Linked();
	state.minecraftFlags = proto::kMcInWorld | proto::kMcDead;
	building.router.Update(true, state);
	CHECK(!building.router.On());
	CHECK_STR(building.router.Report().why, "Minecraft's player is dead");
	const auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].type == proto::kInReleaseAll);
	CHECK_STR(building.router.TurnOn().value_or("on"), "Minecraft's player is dead");  // until it respawns
	state.minecraftFlags = proto::kMcInWorld;
	building.router.Update(true, state);
	CHECK(!building.router.TurnOn());
}

TEST(RouterMovesMinecraftsCursorWhileAScreenIsOpen)
{
	Building building;
	building.router.Update(true, WithAScreen());
	CHECK(Logged(building.log, "a Minecraft screen is build mode's: the mouse moves Minecraft's cursor, and every key is Minecraft's"));
	CHECK(building.hud.Get().shown);
	CHECK(building.hud.Get().x == 1280 && building.hud.Get().y == 720);  // the centre, as Minecraft puts its own position
	auto move = Mouse(0, 0, 10, -4);  // at speed 8, 7.5 and -3 back-buffer pixels: image pixel 643 358
	Inspect(building.router, move);
	CHECK_EQ(MouseOf(move).lLastX, LONG(0));  // V doesn't turn
	CHECK_EQ(MouseOf(move).lLastY, LONG(0));
	auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].type == proto::kInCursor && inputs[0].a == 643 && inputs[0].b == 358);
	CHECK(building.hud.Get().x == 1287 && building.hud.Get().y == 717);
	auto click = Mouse(RI_MOUSE_LEFT_BUTTON_DOWN, 0, 2, 0);  // a click and a move in one event: both Minecraft's
	Inspect(building.router, click);
	CHECK_EQ(MouseOf(click).usButtonFlags, USHORT(0));
	CHECK_EQ(MouseOf(click).lLastX, LONG(0));
	inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 2 && inputs[0].type == proto::kInCursor && inputs[0].a == 644 && inputs[1].type == proto::kInMouseButton);
	CHECK(!building.router.TakePress());               // a click in a screen is no latency mark
	CHECK_EQ(building.router.TakeMouseX(), 0ll);        // and the moves turn nothing
	building.router.Update(true, Linked());  // the screen closed
	CHECK(Logged(building.log, "no Minecraft screen is build mode's: the mouse turns V again"));
	CHECK(!building.hud.Get().shown);
	auto turn = Mouse(0, 0, 10, -4);
	Inspect(building.router, turn);
	CHECK_EQ(MouseOf(turn).lLastX, LONG(10));
	CHECK(building.mapping.Inputs().empty());
	CHECK_EQ(building.router.TakeMouseX(), 10ll);
}

TEST(RouterSendsTheCursorOnlyAsItsPixelChanges)
{
	Building building;
	building.router.Update(true, WithAScreen());
	building.mapping.Inputs();
	for (int i = 0; i < 2; ++i)
	{
		auto small = Mouse(0, 0, 1, 0);  // 0.75 back-buffer pixels each: still image pixel 640
		Inspect(building.router, small);
	}
	CHECK(building.mapping.Inputs().empty());
	auto third = Mouse(0, 0, 1, 0);  // 2.25 in all: pixel 641
	Inspect(building.router, third);
	const auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].a == 641 && inputs[0].b == 360);
	CHECK_EQ(building.router.Report().cursorEvents, 1ull);
	auto     absolute = Mouse(0, 0, 30000, 30000);  // a remote desktop's position, not a movement: it moves nothing
	RAWMOUSE mouse = MouseOf(absolute);
	mouse.usFlags = MOUSE_MOVE_ABSOLUTE;
	std::memcpy(absolute.data() + sizeof(RAWINPUTHEADER), &mouse, sizeof mouse);
	Inspect(building.router, absolute);
	CHECK_EQ(MouseOf(absolute).lLastX, LONG(30000));
	CHECK(building.mapping.Inputs().empty());
}

TEST(RouterTypesTextIntoAScreen)
{
	Building building;
	building.router.Char(u'a');  // no screen: nothing to type into
	CHECK(building.mapping.Inputs().empty());
	building.router.Update(true, WithAScreen());
	building.mapping.Inputs();
	building.router.Char(0x00E6);  // æ
	building.router.Char(0x000D);  // Enter's character: Enter reaches Minecraft as a key
	building.router.Char(0xD83D);
	building.router.Char(0xDE42);  // 🙂
	building.router.Char(0xDE42);  // a lone second half
	const auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 2 && inputs[0].type == proto::kInText && inputs[0].a == 0xE6 && inputs[1].a == 0x1F642);
	const InputReport report = building.router.Report();
	CHECK_EQ(report.charsTyped, 2ull);
	CHECK_EQ(report.charsRefused, 1ull);
	building.router.SetOverlay(true);  // paused: what's typed is CET's overlay's
	building.mapping.Inputs();
	building.router.Char(u'b');
	CHECK(building.mapping.Inputs().empty());
}

TEST(RouterClosesMinecraftsScreenAsBuildModeEnds)
{
	{
		Building building;
		building.router.Update(true, WithAScreen());
		building.mapping.Inputs();
		auto f8 = Key(0x42, VK_F8);
		Inspect(building.router, f8);
		const auto inputs = building.mapping.Inputs();
		CHECK(inputs.size() == 3 && inputs[0].type == proto::kInReleaseAll);  // Minecraft lets go of everything, then Esc
		CHECK(inputs.size() == 3 && inputs[1].type == proto::kInKey && inputs[1].code == 41 && inputs[1].a == 1);
		CHECK(inputs.size() == 3 && inputs[2].type == proto::kInKey && inputs[2].code == 41 && inputs[2].a == 0);
		CHECK(Logged(building.log, "build mode ended with a Minecraft screen open: Esc closes it"));
	}
	{
		Building building;
		building.router.Update(true, WithAScreen(), "V is in a workspot");
		const auto inputs = building.mapping.Inputs();
		CHECK(inputs.size() == 3 && inputs[1].code == 41);
	}
	for (const bool dead : { false, true })
	{
		// Minecraft leaving its world, or its player dying and respawning, changes its screens itself: an Esc could find
		// none, and open the pause menu.
		Building           building;
		link::Link::Status state = WithAScreen();
		if (dead)
		{
			state.minecraftFlags = proto::kMcInWorld | proto::kMcDead;
		}
		else
		{
			state.inWorld = false;
		}
		building.router.Update(true, state);
		CHECK(!building.router.On());
		const auto inputs = building.mapping.Inputs();
		CHECK(inputs.size() == 1 && inputs[0].type == proto::kInReleaseAll);
	}
	Building building;
	building.router.Update(false, WithAScreen());  // unlinked
	CHECK(!building.router.On());
	CHECK_EQ(building.mapping.Inputs().size(), size_t(1));
}

TEST(RouterLeavesTheMouseToTheGameWhileAScreenIsntBuildModes)
{
	Building building;
	building.router.SetOverlay(true);  // paused behind CET's overlay
	building.router.Update(true, WithAScreen());
	building.mapping.Inputs();
	auto move = Mouse(0, 0, 10, 0);
	Inspect(building.router, move);
	CHECK_EQ(MouseOf(move).lLastX, LONG(10));
	CHECK(building.mapping.Inputs().empty());
	CHECK(!building.hud.Get().shown);
	building.router.SetOverlay(false);  // the overlay closed: the screen is build mode's again
	CHECK(building.hud.Get().shown);
	building.router.TurnOff("a build off request");
	CHECK(!building.hud.Get().shown);
	auto off = Mouse(0, 0, 10, 0);
	Inspect(building.router, off);
	CHECK_EQ(MouseOf(off).lLastX, LONG(10));
}

TEST(RouterRereadsWindowsPointerSpeedAsAScreenOpens)
{
	Building building;
	CHECK(Logged(building.log, "Windows' pointer speed is 8 of 20: Minecraft's cursor moves 0.750 back-buffer pixels a count"));
	g_speed = 12;  // changed in Windows while the game runs
	building.router.Update(true, Linked());
	CHECK_EQ(building.router.Report().speed.speed, 8);  // read again only as a screen opens
	building.router.Update(true, WithAScreen());
	CHECK_EQ(building.router.Report().speed.speed, 12);
	CHECK(Logged(building.log, "Windows' pointer speed is 12 of 20: Minecraft's cursor moves 1.500 back-buffer pixels a count"));
	building.mapping.Inputs();
	auto move = Mouse(0, 0, 2, 0);  // 3 back-buffer pixels: from 1280 to 1283, image pixel 641
	Inspect(building.router, move);
	const auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].a == 641);
	g_speed = 8;
}

TEST(RouterReportsTheScreen)
{
	Building building;
	auto     f3 = Key(0x3D, VK_F3);
	auto     f4 = Key(0x3E, VK_F4);
	Inspect(building.router, f3);
	Inspect(building.router, f4);  // F3+F4, before the switcher's screen is known
	building.router.Update(true, WithAScreen());
	auto move = Mouse(0, 0, 4, 0);  // 3 back-buffer pixels: image pixel 641
	Inspect(building.router, move);
	building.router.Char(u'x');
	const InputReport report = building.router.Report();
	CHECK(report.screenOpen && report.screen);
	CHECK(report.cursor.x == 641 && report.cursor.y == 360);
	CHECK(report.imageWidth == 1280 && report.imageHeight == 720);
	CHECK_EQ(report.speed.speed, 8);
	CHECK(!report.speed.enhanced);
	CHECK_EQ(report.cursorEvents, 1ull);
	CHECK_EQ(report.f3Taken, 1ull);
	CHECK_EQ(report.charsTyped, 1ull);
	building.router.SetOverlay(true);
	const InputReport paused = building.router.Report();
	CHECK(paused.screenOpen && !paused.screen);
}
