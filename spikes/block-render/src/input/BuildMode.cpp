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
constexpr uint16_t kF3 = 0x3D;
constexpr uint16_t kF6 = 0x40;
constexpr uint16_t kF8 = 0x42;
constexpr uint16_t kO = 0x18;
constexpr uint16_t kLeftShift = 0x2A;
constexpr uint16_t kRightShift = 0x36;

// The keys that open the game's menus (Esc, I, M, J, P, K and H): its name, or null. T, the game's phone, is Minecraft's
// chat since spike 8.
const char* MenuKey(const RawKey& aKey)
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
constexpr size_t kButtonCount = std::size(kButtons);  // their holders come first, then the keys'
constexpr size_t kSideButtons = 3;                    // the first side button's index

// The other keys build mode takes with no Minecraft screen open: Minecraft's movement keys (spike 5's "Keys") and the
// keys of its screens (spike 8), forwarded, and the game's crouch keys, only hidden, so V can't crouch under the puppet.
// C is Minecraft's toolbar-saving key, so it isn't forwarded either. The forwarded keys are physical keys: Minecraft
// applies its own bindings to them. The keys 1 to 9 are taken too.
struct TakenKey
{
	uint16_t    scanCode;  // set 1, without its E0 prefix
	bool        extended;
	bool        forward;  // to Minecraft, as well as hidden from the game
	const char* name;
};

constexpr TakenKey kTakenKeys[] = {
	{ 0x11, false, true, "W" },        { 0x1E, false, true, "A" },         { 0x1F, false, true, "S" },
	{ 0x20, false, true, "D" },        { 0x39, false, true, "Space" },     { 0x2A, false, true, "Left Shift" },
	{ 0x1D, false, true, "Left Ctrl" }, { 0x12, false, true, "E" },        { 0x14, false, true, "T" },
	{ 0x35, false, true, "/" },        { 0x10, false, true, "Q" },         { kF3, false, true, "F3" },
	{ 0x2E, false, false, "C" },       { 0x1D, true, false, "Right Ctrl" },
};

const TakenKey* Taken(const RawKey& aKey)
{
	for (const TakenKey& taken : kTakenKeys)
	{
		if (aKey.scanCode == taken.scanCode && aKey.extended == taken.extended)
		{
			return &taken;
		}
	}
	return nullptr;
}

// The keys 1 to 9: the digit, or 0.
int Digit(const RawKey& aKey)
{
	return !aKey.extended && aKey.scanCode >= 0x02 && aKey.scanCode <= 0x0A ? aKey.scanCode - 0x01 : 0;
}

std::string KeyName(const RawKey& aKey)
{
	if (const int digit = Digit(aKey))
	{
		return std::format("the key {}", digit);
	}
	if (const TakenKey* taken = Taken(aKey))
	{
		return taken->name;
	}
	if (const char* menu = MenuKey(aKey))
	{
		return menu;
	}
	if (!aKey.extended && aKey.scanCode == kO)
	{
		return "O";
	}
	return std::format("the key of scan code {:#04x}{}", aKey.scanCode, aKey.extended ? " E0" : "");
}

// A key's holder: after the buttons', by its scan code, and those with the E0 prefix 256 further on. None past 0xFF.
std::optional<size_t> KeyIndex(const RawKey& aKey)
{
	if (aKey.scanCode > 0xFF)
	{
		return std::nullopt;
	}
	return kButtonCount + (aKey.extended ? 256u : 0u) + aKey.scanCode;
}

constexpr size_t kF3Index = kButtonCount + kF3;

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
	if (!aKey.extended && aKey.scanCode == kF6)
	{
		// Shift+F6 opens and closes CET's overlay, which pauses build mode while it's open, so its clicks don't reach
		// Minecraft. The overlay sees the key, and a repeat changes nothing. An F6 alone goes on as any other key: with a
		// Minecraft screen open, or F3 held, it's Minecraft's.
		const bool press = !aKey.up && !f6Down_;
		f6Down_ = !aKey.up;
		if (leftShift_ || rightShift_)
		{
			if (press)
			{
				decision.releaseAll = SetOverlay(!overlay_);
			}
			return decision;
		}
	}
	const std::optional<size_t> index = KeyIndex(aKey);
	if (!index)
	{
		return decision;
	}
	Holder& holder = held_[*index];
	if (!aKey.up && holder == Holder::None)
	{
		holder = PlaceKey(aKey, aSituation, decision);
	}
	const Holder to = holder;  // a repeat or a release goes where its press went
	if (aKey.up)
	{
		holder = Holder::None;
	}
	if (to == Holder::Minecraft || to == Holder::Neither)
	{
		decision.hideKey = true;
		++hidden_;
	}
	if (to == Holder::Minecraft)
	{
		Forward(decision, { proto::kInKey, SdlScancode(aKey.scanCode, aKey.extended), aKey.up ? 0 : 1, 0, 0 },
		        std::format("{} {}", KeyName(aKey), aKey.up ? "up" : "down"));
		const int digit = Digit(aKey);
		if (digit != 0 && !aKey.up && !aSituation.screenOpen)
		{
			slot_ = digit;  // in a Minecraft screen, 1 to 9 swap items instead
		}
	}
	return decision;
}

BuildMode::Holder BuildMode::PlaceKey(const RawKey& aKey, const Situation& aSituation, Decision& aDecision)
{
	if (!on_ || overlay_)
	{
		return Holder::Game;
	}
	if (aSituation.screenOpen)
	{
		return Holder::Minecraft;  // a Minecraft screen is open: every key is Minecraft's, typing and Esc included
	}
	if ((aKey.extended || aKey.scanCode != kF3) && held_[kF3Index] == Holder::Minecraft)
	{
		++f3Taken_;
		return Holder::Minecraft;  // F3's combinations: F3+F4, F3+N, F3+F6 and the rest
	}
	if (const char* menu = MenuKey(aKey))
	{
		aDecision.releaseAll = TurnOff(std::format("{} left it", menu));
		return Holder::Game;  // and the game opens its menu
	}
	if (!aKey.extended && aKey.scanCode == kO)
	{
		Forward(aDecision, { proto::kInOpenMenu, 0, 0, 0, 0 }, "O, Minecraft's pause menu");
		return Holder::Neither;  // one pause menu a press: its repeats and release go nowhere
	}
	if (Digit(aKey) != 0)
	{
		return Holder::Minecraft;
	}
	if (const TakenKey* taken = Taken(aKey))
	{
		return taken->forward ? Holder::Minecraft : Holder::Neither;
	}
	return Holder::Game;
}

Decision BuildMode::Mouse(const RawMouse& aMouse, const Situation& aSituation)
{
	Decision decision;
	if (!aSituation.inFront)
	{
		return decision;
	}
	const bool screen = on_ && !overlay_ && aSituation.screenOpen;
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
				if (screen)
				{
					// In a Minecraft screen a click changes no block, and a side button picks no slot.
				}
				else if (i < kSideButtons)
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
	if ((aMouse.buttons & RI_MOUSE_WHEEL) && on_ && !overlay_)
	{
		decision.hideButtons |= RI_MOUSE_WHEEL;
		++hidden_;
		Forward(decision, { proto::kInScroll, 0, aMouse.wheel, 0, 0 }, std::format("the wheel, {}", aMouse.wheel));
		if (!screen)
		{
			Scroll(aMouse.wheel);  // in a Minecraft screen, the wheel scrolls it instead
		}
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
	else if (!aSituation.linked)
	{
		why_ = "it isn't linked";
	}
	else if (!aSituation.blocked.empty())
	{
		why_ = aSituation.blocked;  // first: in a car the camera isn't V's eye, so the link finds no V
	}
	else if (!aSituation.inGame)
	{
		why_ = "V isn't found";
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
	LetGo();
	return true;
}

bool BuildMode::LostFront()
{
	held_.fill(Holder::None);
	f8Down_ = false;
	f6Down_ = false;
	leftShift_ = false;
	rightShift_ = false;
	return on_;
}

bool BuildMode::SetOverlay(bool aOpen)
{
	const bool pauses = aOpen && !overlay_ && on_;
	overlay_ = aOpen;
	if (pauses)
	{
		LetGo();
	}
	return pauses;
}

void BuildMode::LetGo()
{
	for (Holder& holder : held_)
	{
		if (holder == Holder::Minecraft)
		{
			holder = Holder::Neither;  // Minecraft lets go of it now: its repeats and release go nowhere
		}
	}
}

BuildMode::Holder BuildMode::Route(size_t aInput, bool aDown)
{
	Holder& holder = held_[aInput];
	if (aDown && holder == Holder::None)
	{
		holder = on_ && !overlay_ ? Holder::Minecraft : Holder::Game;  // a press goes where build mode says
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
