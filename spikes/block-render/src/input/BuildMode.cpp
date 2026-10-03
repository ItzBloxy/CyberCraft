#include "input/BuildMode.h"

#include "input/Scancodes.h"

#include <Windows.h>

#include <cmath>
#include <format>
#include <initializer_list>
#include <iterator>
#include <utility>

namespace cpmc::input
{
namespace
{
constexpr uint16_t kF6 = 0x40;
constexpr uint16_t kF8 = 0x42;
constexpr uint16_t kLeftShift = 0x2A;
constexpr uint16_t kRightShift = 0x36;

// The keys that open the game's menus (Esc, I, M, J, P, K, H and T), and Shift with F6, which opens CET's overlay: its
// name, or null.
const char* MenuKey(const RawKey& aKey, bool aShift)
{
	if (aKey.extended)
	{
		return nullptr;
	}
	switch (aKey.scanCode)
	{
	case 0x01: return "Esc";
	case 0x17: return "I";
	case 0x32: return "M";
	case 0x24: return "J";
	case 0x19: return "P";
	case 0x25: return "K";
	case 0x23: return "H";
	case 0x14: return "T";
	case kF6: return aShift ? "Shift+F6" : nullptr;
	default: return nullptr;
	}
}

// The mouse buttons build mode takes, as Raw Input flags them and SDL numbers them. The side buttons are Minecraft's
// too: Cyberpunk binds neither, and the user's Minecraft picks hotbar slots with them.
struct Button
{
	uint16_t    down;
	uint16_t    up;
	uint16_t    sdl;
	const char* name;
};

constexpr Button kButtons[] = {
	{ RI_MOUSE_LEFT_BUTTON_DOWN, RI_MOUSE_LEFT_BUTTON_UP, 1, "the left button" },
	{ RI_MOUSE_RIGHT_BUTTON_DOWN, RI_MOUSE_RIGHT_BUTTON_UP, 3, "the right button" },
	{ RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP, 2, "the middle button" },
	{ RI_MOUSE_BUTTON_4_DOWN, RI_MOUSE_BUTTON_4_UP, 4, "mouse button 4" },
	{ RI_MOUSE_BUTTON_5_DOWN, RI_MOUSE_BUTTON_5_UP, 5, "mouse button 5" },
};
constexpr size_t kButtonCount = std::size(kButtons);  // their holders come first, then the keys 1 to 9's
constexpr size_t kSideButtons = 3;                    // the first side button's index

// The other keys build mode takes (spike 5's "Keys"): Minecraft's movement keys, forwarded, and the game's crouch keys,
// only hidden, so V can't crouch under the puppet. C is Minecraft's toolbar-saving key, so it isn't forwarded either.
// The forwarded keys are physical keys: Minecraft applies its own bindings to them.
struct TakenKey
{
	uint16_t    scanCode;  // set 1, without its E0 prefix
	bool        extended;
	bool        forward;  // to Minecraft, as well as hidden from the game
	const char* name;
};

constexpr TakenKey kTakenKeys[] = {
	{ 0x11, false, true, "W" },          { 0x1E, false, true, "A" },        { 0x1F, false, true, "S" },
	{ 0x20, false, true, "D" },          { 0x39, false, true, "Space" },    { 0x2A, false, true, "Left Shift" },
	{ 0x1D, false, true, "Left Ctrl" },  { 0x2E, false, false, "C" },       { 0x1D, true, false, "Right Ctrl" },
};
constexpr size_t kTakenFirst = kButtonCount + 9;  // their holders come after the buttons' and 1 to 9's

int Sign(double aValue)
{
	return aValue > 0.0 ? 1 : aValue < 0.0 ? -1 : 0;
}
}

Decision BuildMode::Key(const RawKey& aKey, const Situation& aSituation)
{
	Decision decision;
	if (!aKey.extended && (aKey.scanCode == kLeftShift || aKey.scanCode == kRightShift))
	{
		(aKey.scanCode == kLeftShift ? leftShift_ : rightShift_) = !aKey.up;
	}
	if (!aSituation.inFront)
	{
		return decision;  // not the user at the game: nothing goes to Minecraft
	}
	if (!aKey.extended && aKey.scanCode == kF8)
	{
		// F8 is build mode's alone, and its repeats do nothing.
		decision.hideKey = true;
		++hidden_;
		const bool press = !aKey.up && !f8Down_;
		f8Down_ = !aKey.up;
		if (!press)
		{
			return decision;
		}
		if (on_)
		{
			decision.releaseAll = TurnOff("F8 turned it off");
		}
		else if (!aSituation.linked)
		{
			decision.requestLink = true;
		}
		else
		{
			TurnOn(aSituation);
		}
		return decision;
	}
	if (on_ && !aKey.up)
	{
		if (const char* menu = MenuKey(aKey, leftShift_ || rightShift_))
		{
			decision.releaseAll = TurnOff(std::format("{} left it", menu));
			return decision;  // and the game opens its menu
		}
	}
	if (!aKey.extended && aKey.scanCode >= 0x02 && aKey.scanCode <= 0x0A)
	{
		const int    digit = aKey.scanCode - 0x01;  // the keys 1 to 9
		const Holder to = Route(kButtonCount - 1 + size_t(digit), !aKey.up);
		if (to == Holder::Minecraft || to == Holder::Neither)
		{
			decision.hideKey = true;
			++hidden_;
		}
		if (to == Holder::Minecraft)
		{
			Forward(decision, { proto::kInKey, SdlScancode(aKey.scanCode, false), aKey.up ? 0 : 1, 0, 0 },
			        std::format("the key {} {}", digit, aKey.up ? "up" : "down"));
			if (!aKey.up)
			{
				slot_ = digit;
			}
		}
	}
	for (size_t i = 0; i < std::size(kTakenKeys); ++i)
	{
		const TakenKey& taken = kTakenKeys[i];
		if (aKey.scanCode != taken.scanCode || aKey.extended != taken.extended)
		{
			continue;
		}
		const Holder to = Route(kTakenFirst + i, !aKey.up);
		if (to == Holder::Minecraft || to == Holder::Neither)
		{
			decision.hideKey = true;
			++hidden_;
		}
		if (to == Holder::Minecraft && taken.forward)
		{
			Forward(decision, { proto::kInKey, SdlScancode(aKey.scanCode, aKey.extended), aKey.up ? 0 : 1, 0, 0 },
			        std::format("{} {}", taken.name, aKey.up ? "up" : "down"));
		}
		break;
	}
	return decision;
}

Decision BuildMode::Mouse(const RawMouse& aMouse, const Situation& aSituation)
{
	Decision decision;
	if (!aSituation.inFront)
	{
		return decision;
	}
	for (size_t i = 0; i < kButtonCount; ++i)
	{
		for (const bool down : { true, false })
		{
			const uint16_t flag = down ? kButtons[i].down : kButtons[i].up;
			if (!(aMouse.buttons & flag))
			{
				continue;
			}
			const Holder to = Route(i, down);
			if (to == Holder::Minecraft || to == Holder::Neither)
			{
				decision.hideButtons |= flag;
				++hidden_;
			}
			if (to == Holder::Minecraft)
			{
				Forward(decision, { proto::kInMouseButton, kButtons[i].sdl, down ? 1 : 0, 0, 0 },
				        std::format("{} {}", kButtons[i].name, down ? "down" : "up"));
				if (i < kSideButtons)
				{
					decision.press = decision.press || down;  // a side button changes no block: no latency to time
				}
				else if (down)
				{
					slot_.reset();  // which slot it picks, if any, is Minecraft's own binding
				}
			}
		}
	}
	if ((aMouse.buttons & RI_MOUSE_WHEEL) && on_)
	{
		decision.hideButtons |= RI_MOUSE_WHEEL;
		++hidden_;
		Forward(decision, { proto::kInScroll, 0, aMouse.wheel, 0, 0 }, std::format("the wheel, {}", aMouse.wheel));
		Scroll(aMouse.wheel);
	}
	return decision;
}

bool BuildMode::TurnOn(const Situation& aSituation)
{
	if (on_)
	{
		return true;
	}
	if (!aSituation.minecraftInWorld)
	{
		why_ = aSituation.minecraft ? "Minecraft isn't in its world" : "Minecraft isn't there";
	}
	else if (!aSituation.inGame)
	{
		why_ = aSituation.linked ? "V isn't found" : "it isn't linked";
	}
	else if (!aSituation.blocked.empty())
	{
		why_ = aSituation.blocked;
	}
	else
	{
		on_ = true;
		why_.clear();
	}
	return on_;
}

bool BuildMode::TurnOff(std::string aWhy)
{
	if (!on_)
	{
		return false;
	}
	on_ = false;
	why_ = std::move(aWhy);
	for (Holder& holder : held_)
	{
		if (holder == Holder::Minecraft)
		{
			holder = Holder::Neither;  // Minecraft lets go of it now: its repeats and release go nowhere
		}
	}
	return true;
}

bool BuildMode::LostFront()
{
	held_.fill(Holder::None);
	f8Down_ = false;
	leftShift_ = false;
	rightShift_ = false;
	return on_;
}

BuildMode::Holder BuildMode::Route(size_t aInput, bool aDown)
{
	static_assert(kInputs == kTakenFirst + std::size(kTakenKeys));
	Holder& holder = held_[aInput];
	if (aDown && holder == Holder::None)
	{
		holder = on_ ? Holder::Minecraft : Holder::Game;  // a press goes where build mode says
	}
	const Holder to = holder;  // a repeat or a release goes where its press went
	if (!aDown)
	{
		holder = Holder::None;
	}
	return to;  // None: a release whose press was never seen, which the game ignores unless it holds the input
}

void BuildMode::Forward(Decision& aDecision, const proto::InputEvent& aEvent, std::string aWhat)
{
	if (aDecision.forwarded < aDecision.forward.size())
	{
		aDecision.forward[aDecision.forwarded++] = aEvent;
		++forwarded_;
		last_ = std::move(aWhat);
	}
}

void BuildMode::Scroll(int16_t aWheel)
{
	// As Minecraft's ScrollWheelHandler counts it: turning the other way starts over, and each event moves the slot one
	// step at most, by its whole notches, keeping the fraction for the next. Up is the slot to the left.
	const double notches = aWheel / 120.0;
	if (scroll_ != 0.0 && Sign(notches) != Sign(scroll_))
	{
		scroll_ = 0.0;
	}
	scroll_ += notches;
	const double whole = std::trunc(scroll_);
	scroll_ -= whole;
	if (whole != 0.0 && slot_)
	{
		slot_ = (*slot_ - 1 - Sign(whole) + 9) % 9 + 1;
	}
}
}
