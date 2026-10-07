#pragma once

#include <skycraft_protocol.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

// Build mode (the minecraft-link spec's "Input"): what becomes of each keyboard and mouse event the game reads. In build
// mode the mouse buttons, side buttons included, the wheel and the keys 1 to 9 go to Minecraft and are hidden from the
// game; F8 toggles it; and the keys that open the game's menus leave it, and reach the game. Since spike 5 (its spec's
// "Keys"), Minecraft's movement keys go to Minecraft too, and the game's crouch keys are hidden without going anywhere.
// Since spike 7's Part 3, Shift+F6, which opens and closes CET's overlay, pauses it instead: while the overlay is open
// every input is the overlay's and the game's, and build mode goes on when it closes. A press's repeats and its release
// go where the press went, so neither side is left holding an input that only the other saw let go. Pure, and not
// thread-safe: the router (input/Router.h) holds its lock around every call.
namespace cpmc::input
{
namespace proto = skycraft::proto;

// A keyboard event, from RAWKEYBOARD.
struct RawKey
{
	uint16_t scanCode = 0;      // set 1, without its E0 prefix (MakeCode)
	bool     extended = false;  // the E0 prefix (RI_KEY_E0)
	bool     up = false;        // a release (RI_KEY_BREAK); a key held down repeats its press
};

// A mouse event's buttons and wheel, from RAWMOUSE. Its movement isn't build mode's business.
struct RawMouse
{
	uint16_t buttons = 0;  // usButtonFlags: RI_MOUSE_LEFT_BUTTON_DOWN and the rest, and RI_MOUSE_WHEEL
	int16_t  wheel = 0;    // usButtonData, with RI_MOUSE_WHEEL: notches x 120, positive away from the user (up)
};

// Where things stand: whether the game is in front, and what the link said last frame.
struct Situation
{
	bool        inFront = false;    // the foreground window is the game's
	bool        linked = false;     // the user's Link
	bool        inGame = false;     // linked, with V found
	bool        minecraft = false;  // Minecraft's heartbeat is fresh
	bool        minecraftInWorld = false;
	std::string blocked;  // why Minecraft can't drive V: V in a vehicle, a workspot or dead, or Minecraft's player dead (spike 5).
	                      // Known even when the link finds no V, as in a car, where the camera isn't V's eye
};

// What becomes of one event.
struct Decision
{
	static constexpr size_t kMost = 11;  // five buttons pressed and released, and the wheel

	uint16_t                             hideButtons = 0;      // mouse: the flags to clear, so the game doesn't see them
	bool                                 hideKey = false;      // keyboard: make the key a no-op for the game
	bool                                 releaseAll = false;   // build mode ended: Minecraft lets go of everything, first
	std::array<proto::InputEvent, kMost> forward{};            // then these go to Minecraft's input ring, in order
	size_t                               forwarded = 0;
	bool                                 press = false;        // a left, right or middle press went to Minecraft: a latency mark
	bool                                 requestLink = false;  // F8 while unlinked: the main thread links, then turns it on
};

class BuildMode
{
public:
	Decision Key(const RawKey& aKey, const Situation& aSituation);
	Decision Mouse(const RawMouse& aMouse, const Situation& aSituation);

	// On, from the main thread: a build request, or F8's link once it's done. True if it's on; if not, Why() says why.
	bool TurnOn(const Situation& aSituation);
	// Off, saying why: true if it was on, so Minecraft must let go of everything.
	bool TurnOff(std::string aWhy);
	// The game stopped being in front, so what's held is let go of in another window, where neither side sees it. True
	// if build mode is on, so Minecraft must let go of everything. Build mode stays on for when the user comes back.
	bool LostFront();
	// CET's overlay opened or closed, as the overlay itself says (CET's events), in case it was toggled some other way
	// than Shift+F6. True if this pauses build mode, so Minecraft must let go of everything.
	bool SetOverlay(bool aOpen);

	bool               On() const { return on_; }
	bool               Paused() const { return on_ && overlay_; }  // on, behind CET's overlay
	const std::string& Why() const { return why_; }          // why it's off
	std::optional<int> Slot() const { return slot_; }        // Minecraft's hotbar slot, 1 to 9, as far as it's known
	uint64_t           Forwarded() const { return forwarded_; }  // events for Minecraft's input ring
	uint64_t           Hidden() const { return hidden_; }        // inputs hidden from the game
	const std::string& Last() const { return last_; }            // the last input forwarded

private:
	// Where a held input's press went.
	enum class Holder : uint8_t
	{
		None,
		Game,
		Minecraft,
		Neither,  // Minecraft's, until build mode ended and Minecraft let go of everything
	};

	static constexpr size_t kInputs = 23;  // the five mouse buttons, the keys 1 to 9, then nine other keys it takes

	Holder Route(size_t aInput, bool aDown);
	void   Forward(Decision& aDecision, const proto::InputEvent& aEvent, std::string aWhat);
	void   Scroll(int16_t aWheel);
	void   LetGo();  // Minecraft lets go of what it holds: those inputs' repeats and releases go nowhere

	bool                        on_ = false;
	std::string                 why_ = "F8 turns it on";
	std::array<Holder, kInputs> held_{};
	bool                        overlay_ = false;  // CET's overlay is open, as Shift+F6 and the overlay say
	bool                        f8Down_ = false;
	bool                        f6Down_ = false;
	bool                        leftShift_ = false;
	bool                        rightShift_ = false;
	std::optional<int>          slot_;
	double                      scroll_ = 0.0;  // the wheel's notches not yet whole, as Minecraft keeps them
	uint64_t                    forwarded_ = 0;
	uint64_t                    hidden_ = 0;
	std::string                 last_ = "nothing yet";
};
}
