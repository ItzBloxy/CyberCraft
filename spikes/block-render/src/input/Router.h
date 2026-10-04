#pragma once

#include "LogQueue.h"
#include "input/BuildMode.h"
#include "link/Link.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

// Where the game's keyboard and mouse events go (the spec's "Input"), between three threads: the game's input thread,
// through the GetRawInputData patch (input/RawInputHook.h); the focus watch's; and the main thread, which keeps the link
// and what it says about Minecraft and V current, and turns build mode on and off. One lock guards build mode and the
// link, so once the main thread takes the link away, nothing pushes into the mapping.
namespace cpmc::input
{
// Whether the foreground window belongs to this process.
bool GameInFront();

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
};

class Router
{
public:
	using FrontCheck = bool (*)();
	explicit Router(LogQueue& aLog, FrontCheck aInFront = &GameInFront);

	// From the patch, once GetRawInputData has copied an event into aData, aBytes of it. A keyboard or mouse event that
	// build mode takes is hidden from the game in place, and forwarded. Anything else, or too short, is left alone.
	void Inspect(void* aData, uint32_t aBytes);

	// From the main thread. The link: once the mapping opens, and null again at unload.
	void SetLink(link::Link* aLink);
	// Once a frame, after the link's update: whether the user linked, what the link says, and why V can't be placed, if
	// it can't (spike 5), even when the link finds no V. Build mode ends if Minecraft or V is gone, or the link, or V
	// can't be placed, or Minecraft's player dies; why V can't be placed is said before "V isn't found".
	void Update(bool aLinked, const link::Link::Status& aState, const std::string& aVBlocked = {});
	// The mouse's movement along x since the last call, in raw counts: for the turn a teleport adds back (spike 5).
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

private:
	// With the lock held.
	void Apply(const Decision& aDecision, bool aWasOn, const std::string& aWhyBefore);
	bool Off(std::string aWhy);
	void Push(const proto::InputEvent& aEvent);

	LogQueue&             log_;
	FrontCheck            inFront_;
	mutable std::mutex    mutex_;
	BuildMode             mode_;
	link::Link*           link_ = nullptr;
	Situation             situation_;  // the main thread's part: the link, Minecraft and V
	bool                  linkRequested_ = false;
	std::optional<double> press_;
	std::atomic<uint64_t> read_{ 0 };
	std::atomic<int64_t>  mouseX_{ 0 };
};
}
