#pragma once

#include <skycraft_protocol.h>

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Typing commands into Minecraft's chat through the input ring (the spec's "Typed commands"): press T, wait for the
// chat screen, send the text, press Enter, and wait for the screen to close before the next one.
namespace cpmc::link
{
namespace proto = skycraft::proto;

constexpr uint16_t kSdlT = 23;       // SDL scancodes (USB HID usages)
constexpr uint16_t kSdlReturn = 40;
constexpr double   kTypingTimeout = 2.0;  // seconds to wait for the chat screen to open, and then to close

// The overlay's command sets, by name: creative, tnt-kit, tnt-ahead, wall, tnt-cube, field and wipe.
std::optional<std::vector<std::string>> NamedCommands(std::string_view aName);

// A command starts with '/', is printable ASCII, and is at most 256 characters (Minecraft's chat limit).
bool ValidCommand(std::string_view aCommand, std::string& aError);

// A valid command, or the name of a set. False, with aError, for anything else.
bool ValidCommandOrSet(std::string_view aCommandOrName, std::string& aError);

class CommandTyper
{
public:
	// A command, or the name of a set, which queues all of its commands. False, with aError, for anything else.
	bool Queue(std::string_view aCommandOrName, std::string& aError);

	// What Minecraft's state says, read from McState each frame.
	struct Minecraft
	{
		bool inWorld = false;
		bool screenOpen = false;
	};

	struct Step
	{
		std::vector<proto::InputEvent> events;  // to push into the input ring this frame, in order
		std::vector<std::string>       log;
		bool                           entered = false;  // Enter went this frame: a latency mark
	};

	// Once a frame. A command whose turn comes while Minecraft isn't in its world, or has a screen open, is dropped.
	Step Advance(double aNow, const Minecraft& aMinecraft);

	size_t Waiting() const { return queue_.size(); }  // the one being typed included
	void   Clear();                                   // drops everything: Minecraft has gone

private:
	enum class State
	{
		Idle,
		WaitingToOpen,
		WaitingToClose,
	};

	std::deque<std::string> queue_;
	State                   state_ = State::Idle;
	double                  deadline_ = 0.0;
};
}
