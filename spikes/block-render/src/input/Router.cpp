#include "input/Router.h"

#include "Measurements.h"

#include <Windows.h>

#include <cstring>
#include <utility>

namespace cpmc::input
{
namespace
{
// The mouse flags build mode may take: the five buttons' presses and releases, and the wheel. An event without them is
// movement, or the horizontal wheel, and stays the game's without taking the lock.
constexpr USHORT kTaken = RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_LEFT_BUTTON_UP | RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_UP |
                          RI_MOUSE_MIDDLE_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_UP | RI_MOUSE_BUTTON_4_DOWN | RI_MOUSE_BUTTON_4_UP |
                          RI_MOUSE_BUTTON_5_DOWN | RI_MOUSE_BUTTON_5_UP | RI_MOUSE_WHEEL;
}

bool GameInFront()
{
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	return process == GetCurrentProcessId();
}

Router::Router(LogQueue& aLog, FrontCheck aInFront) : log_(aLog), inFront_(aInFront)
{
}

void Router::Inspect(void* aData, uint32_t aBytes)
{
	if (!aData || aBytes < sizeof(RAWINPUTHEADER))
	{
		return;
	}
	RAWINPUTHEADER header;
	std::memcpy(&header, aData, sizeof header);
	uint8_t* const body = static_cast<uint8_t*>(aData) + sizeof header;
	if (header.dwType == RIM_TYPEMOUSE && aBytes >= sizeof header + sizeof(RAWMOUSE))
	{
		++read_;
		RAWMOUSE mouse;
		std::memcpy(&mouse, body, sizeof mouse);
		if (!(mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
		{
			mouseX_ += mouse.lLastX;
		}
		if ((mouse.usButtonFlags & kTaken) == 0)
		{
			return;
		}
		const bool      inFront = inFront_();
		std::lock_guard lock(mutex_);
		Situation       situation = situation_;
		situation.inFront = inFront;
		const bool        wasOn = mode_.On();
		const bool        wasPaused = mode_.Paused();
		const std::string whyBefore = mode_.Why();
		const Decision    decision = mode_.Mouse({ mouse.usButtonFlags, int16_t(mouse.usButtonData) }, situation);
		if (decision.hideButtons != 0)
		{
			// The game reads buttons and the wheel only from these flags, and the movement from its own fields, which stay.
			mouse.usButtonFlags = USHORT(mouse.usButtonFlags & ~decision.hideButtons);
			std::memcpy(body, &mouse, sizeof mouse);
		}
		Apply(decision, wasOn, wasPaused, whyBefore);
	}
	else if (header.dwType == RIM_TYPEKEYBOARD && aBytes >= sizeof header + sizeof(RAWKEYBOARD))
	{
		++read_;
		RAWKEYBOARD key;
		std::memcpy(&key, body, sizeof key);
		const bool      inFront = inFront_();
		std::lock_guard lock(mutex_);
		Situation       situation = situation_;
		situation.inFront = inFront;
		const bool        wasOn = mode_.On();
		const bool        wasPaused = mode_.Paused();
		const std::string whyBefore = mode_.Why();
		const Decision    decision = mode_.Key({ key.MakeCode, (key.Flags & RI_KEY_E0) != 0, (key.Flags & RI_KEY_BREAK) != 0 }, situation);
		if (decision.hideKey)
		{
			// A no-op: the game skips a key whose virtual key is 0xFF.
			key.MakeCode = 0;
			key.VKey = 0xFF;
			std::memcpy(body, &key, sizeof key);
		}
		Apply(decision, wasOn, wasPaused, whyBefore);
	}
}

void Router::SetLink(link::Link* aLink)
{
	std::lock_guard lock(mutex_);
	link_ = aLink;
}

void Router::Update(bool aLinked, const link::Link::Status& aState, const std::string& aVBlocked)
{
	std::lock_guard lock(mutex_);
	situation_.linked = aLinked;
	situation_.inGame = aState.inGame;
	situation_.minecraft = aState.minecraft;
	situation_.minecraftInWorld = aState.inWorld;
	situation_.blocked = (aState.minecraftFlags & proto::kMcDead) ? "Minecraft's player is dead" : aVBlocked;
	if (mode_.On())
	{
		std::string why = !aState.minecraft             ? "Minecraft has gone"
		                  : !aState.inWorld             ? "Minecraft left its world"
		                  : !aLinked                    ? "unlinked"
		                  : !situation_.blocked.empty() ? situation_.blocked
		                  : !aState.inGame              ? "V isn't found"
		                                                : std::string();
		if (!why.empty())
		{
			Off(std::move(why));
		}
	}
}

int64_t Router::TakeMouseX()
{
	const int64_t   counts = mouseX_.exchange(0);
	std::lock_guard lock(mutex_);
	return mode_.Paused() ? 0 : counts;  // CET's overlay's cursor, which turns nothing
}

std::optional<std::string> Router::TurnOn()
{
	std::lock_guard lock(mutex_);
	if (mode_.On())
	{
		return std::nullopt;
	}
	if (!mode_.TurnOn(situation_))
	{
		log_.Push("build mode stays off: " + mode_.Why());
		return mode_.Why();
	}
	log_.Push("build mode on");
	LogPause(false);
	return std::nullopt;
}

bool Router::TurnOff(std::string aWhy)
{
	std::lock_guard lock(mutex_);
	return Off(std::move(aWhy));
}

bool Router::On() const
{
	std::lock_guard lock(mutex_);
	return mode_.On();
}

bool Router::TakeLinkRequest()
{
	std::lock_guard lock(mutex_);
	return std::exchange(linkRequested_, false);
}

std::optional<double> Router::TakePress()
{
	std::lock_guard lock(mutex_);
	return std::exchange(press_, std::nullopt);
}

InputReport Router::Report() const
{
	std::lock_guard lock(mutex_);
	return { mode_.On(), mode_.Why(), mode_.Slot(), read_.load(), mode_.Forwarded(), mode_.Hidden(), mode_.Last(), mode_.Paused() };
}

void Router::FrontLost()
{
	std::lock_guard lock(mutex_);
	if (mode_.LostFront())
	{
		Push({ proto::kInReleaseAll, 0, 0, 0, 0 });
		log_.Push("the game lost the front in build mode: Minecraft lets go of everything");
	}
}

void Router::SetOverlay(bool aOpen)
{
	std::lock_guard lock(mutex_);
	const bool      wasPaused = mode_.Paused();
	if (mode_.SetOverlay(aOpen))
	{
		Push({ proto::kInReleaseAll, 0, 0, 0, 0 });
	}
	LogPause(wasPaused);
}

void Router::Apply(const Decision& aDecision, bool aWasOn, bool aWasPaused, const std::string& aWhyBefore)
{
	if (aDecision.releaseAll)
	{
		Push({ proto::kInReleaseAll, 0, 0, 0, 0 });
	}
	for (size_t i = 0; i < aDecision.forwarded; ++i)
	{
		Push(aDecision.forward[i]);
	}
	if (aDecision.press)
	{
		press_ = SteadySeconds();
	}
	if (aDecision.requestLink)
	{
		linkRequested_ = true;
		log_.Push("F8: linking first, then build mode");
	}
	if (mode_.On() != aWasOn)
	{
		log_.Push(mode_.On() ? "build mode on" : "build mode off: " + mode_.Why());
	}
	else if (!mode_.On() && mode_.Why() != aWhyBefore)
	{
		log_.Push("build mode stays off: " + mode_.Why());
	}
	LogPause(aWasPaused);
}

void Router::LogPause(bool aWasPaused)
{
	if (mode_.Paused() && !aWasPaused)
	{
		log_.Push("build mode paused: CET's overlay is open");
	}
	else if (!mode_.Paused() && aWasPaused && mode_.On())
	{
		log_.Push("build mode goes on: CET's overlay closed");
	}
}

bool Router::Off(std::string aWhy)
{
	if (!mode_.TurnOff(std::move(aWhy)))
	{
		return false;
	}
	Push({ proto::kInReleaseAll, 0, 0, 0, 0 });
	log_.Push("build mode off: " + mode_.Why());
	return true;
}

void Router::Push(const proto::InputEvent& aEvent)
{
	if (link_)
	{
		link_->Push(aEvent);  // a full ring drops it, and the link counts it
	}
}
}
