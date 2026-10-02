#include "link/Commands.h"

#include <format>

namespace cpmc::link
{
namespace
{
proto::InputEvent Key(uint16_t aScancode, bool aDown)
{
	return { proto::kInKey, aScancode, aDown ? 1 : 0, 0, 0 };
}
}

std::optional<std::vector<std::string>> NamedCommands(std::string_view aName)
{
	if (aName == "creative")
	{
		return std::vector<std::string>{ "/gamemode creative" };
	}
	if (aName == "tnt-kit")
	{
		return std::vector<std::string>{ "/give @s tnt 64", "/give @s flint_and_steel" };
	}
	// "Ahead" is level: ^ ^ ^N follows the look's pitch too, which would put these underground whenever V looks down.
	if (aName == "tnt-ahead")
	{
		// On a stone block: only the player collides with the floor's triangles, so a TNT in the air falls into the void.
		return std::vector<std::string>{ "/gamemode creative", "/execute rotated ~ 0 positioned ^ ^ ^4 run setblock ~ ~-1 ~ stone",
			                             "/execute rotated ~ 0 positioned ^ ^ ^4 run summon tnt" };
	}
	if (aName == "wall")
	{
		return std::vector<std::string>{ "/execute rotated ~ 0 positioned ^ ^ ^4 run fill ~-2 ~ ~ ~2 ~3 ~ stone_bricks" };
	}
	if (aName == "tnt-cube")
	{
		return std::vector<std::string>{ "/gamemode creative", "/execute rotated ~ 0 positioned ^ ^ ^6 run fill ~-2 ~ ~-2 ~2 ~4 ~2 tnt",
			                             "/execute rotated ~ 0 positioned ^ ^ ^6 run summon tnt ~ ~5 ~" };
	}
	if (aName == "field")
	{
		return std::vector<std::string>{ "/fill ~-63 ~ ~-63 ~64 ~ ~64 grass_block" };
	}
	if (aName == "wipe")
	{
		return std::vector<std::string>{ "/fill ~-63 ~ ~-63 ~64 ~ ~64 air", "/fill ~-16 ~ ~-16 ~15 ~31 ~15 air" };
	}
	return std::nullopt;
}

bool ValidCommand(std::string_view aCommand, std::string& aError)
{
	if (aCommand.empty() || aCommand.front() != '/')
	{
		aError = "a command starts with /";
		return false;
	}
	if (aCommand.size() > 256)
	{
		aError = std::format("a command is at most 256 characters, and this is {}", aCommand.size());
		return false;
	}
	for (const char ch : aCommand)
	{
		if (ch < 32 || ch > 126)
		{
			aError = "a command is printable ASCII";
			return false;
		}
	}
	return true;
}

bool CommandTyper::Queue(std::string_view aCommandOrName, std::string& aError)
{
	if (const auto named = NamedCommands(aCommandOrName))
	{
		queue_.insert(queue_.end(), named->begin(), named->end());
		return true;
	}
	if (!ValidCommand(aCommandOrName, aError))
	{
		if (aCommandOrName.empty() || aCommandOrName.front() != '/')
		{
			aError = std::format("\"{}\" is neither a command nor a set (creative, tnt-kit, tnt-ahead, wall, tnt-cube, field, wipe)",
			                     aCommandOrName);
		}
		return false;
	}
	queue_.emplace_back(aCommandOrName);
	return true;
}

CommandTyper::Step CommandTyper::Advance(double aNow, const Minecraft& aMinecraft)
{
	Step step;
	switch (state_)
	{
	case State::Idle:
		if (queue_.empty())
		{
			break;
		}
		if (!aMinecraft.inWorld || aMinecraft.screenOpen)
		{
			step.log.push_back(std::format("not typed: {}: {}", aMinecraft.inWorld ? "a Minecraft screen is open" : "Minecraft isn't in its world",
			                               queue_.front()));
			queue_.pop_front();
			break;
		}
		step.events = { Key(kSdlT, true), Key(kSdlT, false) };
		state_ = State::WaitingToOpen;
		deadline_ = aNow + kTypingTimeout;
		break;
	case State::WaitingToOpen:
		if (aMinecraft.screenOpen)
		{
			for (const char ch : queue_.front())
			{
				step.events.push_back({ proto::kInText, 0, int32_t(ch), 0, 0 });
			}
			step.events.push_back(Key(kSdlReturn, true));
			step.events.push_back(Key(kSdlReturn, false));
			step.entered = true;
			state_ = State::WaitingToClose;
			deadline_ = aNow + kTypingTimeout;
		}
		else if (aNow > deadline_)
		{
			step.log.push_back("not typed: chat didn't open within 2 s: " + queue_.front());
			queue_.pop_front();
			state_ = State::Idle;
		}
		break;
	case State::WaitingToClose:
		if (!aMinecraft.screenOpen)
		{
			step.log.push_back("typed: " + queue_.front());
			queue_.pop_front();
			state_ = State::Idle;
		}
		else if (aNow > deadline_)
		{
			step.log.push_back("typed, but chat didn't close within 2 s: " + queue_.front());
			queue_.pop_front();
			state_ = State::Idle;
		}
		break;
	}
	return step;
}

void CommandTyper::Clear()
{
	queue_.clear();
	state_ = State::Idle;
}
}
