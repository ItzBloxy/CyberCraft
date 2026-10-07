#pragma once

#include "LogQueue.h"
#include "input/BuildMode.h"
#include "input/Cursor.h"
#include "input/Text.h"
#include "link/Link.h"
#include "render/Hud.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

// Where the game's keyboard and mouse events go (the spec's "Input"), between four threads: the game's input thread,
// through the GetRawInputData patch (input/RawInputHook.h) and, for typed text, the PeekMessageW patch
// (input/MessageHook.h); the focus watch's; and the main thread, which keeps the link and what it says about Minecraft
// and V current, and turns build mode on and off. One lock guards build mode and the link, so once the main thread takes
// the link away, nothing pushes into the mapping. Since spike 8, while a Minecraft screen is build mode's (on, not
// paused, and Minecraft saying a screen is open), the mouse's movement moves Minecraft's cursor instead of turning V,
// typed characters go to Minecraft, and build mode ending closes the screen with Esc.
namespace cpmc::input
{
// Whether the foreground window belongs to this process.
bool GameInFront();
// Windows' pointer speed, and whether "Enhance pointer precision" is on, as SystemParametersInfo says.
PointerSpeed ReadPointerSpeed();

// Build mode and the hook, for the status.
struct InputReport
{
	bool               on = false;
	std::string        why;            // why it's off
	std::optional<int> slot;           // Minecraft's hotbar slot, 1 to 9, once a number key has set it
	uint64_t           read = 0;       // keyboard and mouse events read
	uint64_t           forwarded = 0;  // events for Minecraft's input ring
	uint64_t           hidden = 0;     // inputs hidden from the game
	std::string        last;           // the last input forwarded
	bool               paused = false;  // on, behind CET's overlay
	// Spike 8
	bool         screenOpen = false;  // Minecraft says a screen is open
	bool         screen = false;      // and it's build mode's: the mouse moves Minecraft's cursor, and every key is Minecraft's
	Pixel        cursor;              // in the HUD image's pixels
	uint32_t     imageWidth = 0;      // the HUD image the cursor stays in; 0 before one has come
	uint32_t     imageHeight = 0;
	PointerSpeed speed;               // Windows', as last read
	uint64_t     cursorEvents = 0;    // kInCursor sent
	uint64_t     f3Taken = 0;         // keys Minecraft got because F3 was held
	uint64_t     charsTyped = 0;      // kInText sent
	uint64_t     charsRefused = 0;    // lone surrogates
};

class Router
{
public:
	using FrontCheck = bool (*)();
	using SpeedCheck = PointerSpeed (*)();
	explicit Router(LogQueue& aLog, FrontCheck aInFront = &GameInFront, SpeedCheck aSpeed = &ReadPointerSpeed);

	// From the patch, once GetRawInputData has copied an event into aData, aBytes of it. A keyboard or mouse event that
	// build mode takes is hidden from the game in place, and forwarded. Anything else, or too short, is left alone.
	void Inspect(void* aData, uint32_t aBytes);
	// From the PeekMessageW patch: one of WM_CHAR's UTF-16 code units. While a Minecraft screen is build mode's, each whole,
	// printable code point goes to Minecraft as kInText.
	void Char(uint16_t aUnit);

	// From the main thread. The link: once the mapping opens, and null again at unload.
	void SetLink(link::Link* aLink);
	// Once a frame, after the link's update: whether the user linked, what the link says, and why V can't be placed, if
	// it can't (spike 5), even when the link finds no V. Build mode ends if Minecraft or V is gone, or the link, or V
	// can't be placed, or Minecraft's player dies; why V can't be placed is said before "V isn't found". A Minecraft
	// screen opening re-reads Windows' pointer speed, and puts the cursor at the image's centre.
	void Update(bool aLinked, const link::Link::Status& aState, const std::string& aVBlocked = {});
	// Once a frame, before Update: the HUD image's size and the back-buffer pixels each of its texels covers, which the
	// cursor stays in.
	void SetHudArea(uint32_t aImageWidth, uint32_t aImageHeight, uint32_t aScale);
	// Where the HUD pass finds the cursor, which the router keeps current; null for none.
	void SetHudCursor(HudCursor* aCursor);
	// The mouse's movement along x since the last call, in raw counts: for the turn a teleport adds back (spike 5). None
	// while build mode is paused, when the mouse is CET's overlay's, or while a Minecraft screen is build mode's.
	int64_t TakeMouseX();
	// Build mode on: nothing, or why it stays off.
	std::optional<std::string> TurnOn();
	// Build mode off, saying why: true if it was on.
	bool                  TurnOff(std::string aWhy);
	bool                  On() const;
	bool                  TakeLinkRequest();  // F8 asked for the link, since the last call
	std::optional<double> TakePress();        // when a mouse button's press last went to Minecraft (SteadySeconds), once
	InputReport           Report() const;

	// From the focus watch: the game has just stopped being in front.
	void FrontLost();
	// From the main thread: CET's overlay opened or closed, as the overlay itself says (spike 7's `overlay` request).
	void SetOverlay(bool aOpen);

private:
	// With the lock held.
	void Apply(const Decision& aDecision, bool aWasOn, bool aWasPaused, const std::string& aWhyBefore);
	void LogPause(bool aWasPaused);  // build mode paused, or going on again, since aWasPaused
	bool Off(std::string aWhy);
	void Push(const proto::InputEvent& aEvent);
	bool ScreenOn() const;  // a Minecraft screen is build mode's
	void Refresh();         // the screen state, and the HUD's cursor, after anything that may change them
	void Publish();         // the HUD's cursor
	void CloseScreen();     // build mode ended: Esc closes Minecraft's screen, if it's there to close
	void ReadSpeed();

	LogQueue&             log_;
	FrontCheck            inFront_;
	SpeedCheck            speedCheck_;
	mutable std::mutex    mutex_;
	BuildMode             mode_;
	link::Link*           link_ = nullptr;
	Situation             situation_;  // the main thread's part: the link, Minecraft and V
	bool                  linkRequested_ = false;
	std::optional<double> press_;
	std::atomic<uint64_t> read_{ 0 };
	std::atomic<int64_t>  mouseX_{ 0 };
	// Spike 8
	std::atomic<bool>           screen_{ false };  // ScreenOn, for the movement's way in without the lock
	bool                        closeable_ = false;  // linked, Minecraft in its world, its player alive, a screen open
	Cursor                      cursor_;
	Utf16Text                   text_;
	HudCursor*                  hudCursor_ = nullptr;
	PointerSpeed                speed_;
	std::optional<PointerSpeed> speedLogged_;
	double                      factor_ = 1.0;
	uint64_t                    cursorEvents_ = 0;
	uint64_t                    charsTyped_ = 0;
};
}
