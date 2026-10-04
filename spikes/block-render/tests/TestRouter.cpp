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

bool FakeFront()
{
	return g_inFront;
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

// A router on a link, in build mode.
struct Building
{
	Building() : router(log, &FakeFront)
	{
		g_inFront = true;
		router.SetLink(mapping.link);
		router.Update(true, Linked());
		CHECK(!router.TurnOn());
	}

	LogQueue log;
	Mapping  mapping;
	Router   router;
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
