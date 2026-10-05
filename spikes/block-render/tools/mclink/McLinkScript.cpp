#include "McLinkScript.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <format>

namespace cpmc::mclink
{
namespace
{
std::string_view Trim(std::string_view aText)
{
	const size_t first = aText.find_first_not_of(" \t\r");
	if (first == std::string_view::npos)
	{
		return {};
	}
	return aText.substr(first, aText.find_last_not_of(" \t\r") - first + 1);
}

std::vector<std::string_view> Words(std::string_view aText)
{
	std::vector<std::string_view> words;
	size_t                        at = 0;
	while (at < aText.size())
	{
		const size_t start = aText.find_first_not_of(" \t", at);
		if (start == std::string_view::npos)
		{
			break;
		}
		const size_t end = aText.find_first_of(" \t", start);
		words.push_back(aText.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
		at = end == std::string_view::npos ? aText.size() : end;
	}
	return words;
}

bool Number(std::string_view aWord, double& aOut)
{
	const auto [end, error] = std::from_chars(aWord.data(), aWord.data() + aWord.size(), aOut);
	return error == std::errc() && end == aWord.data() + aWord.size() && std::isfinite(aOut);
}

// An actor's number: a whole one from 1 to 4294967295, as SkyCraft's 32-bit formId holds it, 0 meaning none.
bool WholeId(double aValue)
{
	return aValue >= 1.0 && aValue <= 4294967295.0 && aValue == std::floor(aValue);
}
}

std::optional<std::vector<Action>> ParseScript(std::string_view aText, std::string& aError)
{
	std::vector<Action> actions;
	int                 lineNumber = 0;
	size_t              at = 0;
	while (at <= aText.size())
	{
		const size_t           end = aText.find('\n', at);
		const std::string_view line = Trim(aText.substr(at, end == std::string_view::npos ? std::string_view::npos : end - at));
		at = end == std::string_view::npos ? aText.size() + 1 : end + 1;
		++lineNumber;
		if (line.empty() || line.front() == '#')
		{
			continue;
		}
		const auto       words = Words(line);
		const auto       verb = words[0];
		const auto       rest = Trim(line.substr(verb.size()));
		Action           action;
		action.line = lineNumber;
		const auto fail = [&](std::string_view aWhy) {
			aError = std::format("line {}: {}: \"{}\"", lineNumber, aWhy, line);
			return std::nullopt;
		};
		const auto numbers = [&](size_t aCount) {
			if (words.size() != aCount + 1)
			{
				return false;
			}
			for (size_t i = 0; i < aCount; ++i)
			{
				if (!Number(words[i + 1], action.numbers[i]))
				{
					return false;
				}
			}
			return true;
		};
		const auto onOff = [&] {
			if (words.size() == 2 && (words[1] == "on" || words[1] == "off"))
			{
				action.on = words[1] == "on";
				return true;
			}
			return false;
		};
		if (verb == "wait")
		{
			action.kind = Action::Kind::Wait;
			if (!numbers(1) || action.numbers[0] < 0)
			{
				return fail("wait takes a number of seconds");
			}
		}
		else if (verb == "wait-world")
		{
			action.kind = Action::Kind::WaitWorld;
			action.numbers[0] = 120;
			if (words.size() > 1 && (!numbers(1) || action.numbers[0] <= 0))
			{
				return fail("wait-world takes an optional number of seconds");
			}
		}
		else if (verb == "link" || verb == "build" || verb == "hud")
		{
			action.kind = verb == "link" ? Action::Kind::Link : verb == "build" ? Action::Kind::Build : Action::Kind::Hud;
			if (!onOff())
			{
				return fail(std::format("{} takes on or off", verb));
			}
		}
		else if (verb == "at")
		{
			action.kind = Action::Kind::At;
			if (!numbers(3))
			{
				return fail("at takes x, y and z");
			}
		}
		else if (verb == "look")
		{
			action.kind = Action::Kind::Look;
			if (!numbers(2) || std::abs(action.numbers[1]) > 90)
			{
				return fail("look takes a yaw and a pitch from -90 to 90");
			}
		}
		else if (verb == "type")
		{
			action.kind = Action::Kind::Type;
			action.text = std::string(rest);
			if (action.text.empty())
			{
				return fail("type takes a command or a set");
			}
		}
		else if (verb == "key" || verb == "down" || verb == "up")
		{
			action.kind = verb == "key" ? Action::Kind::Key : verb == "down" ? Action::Kind::Down : Action::Kind::Up;
			if (!numbers(1) || action.numbers[0] < 1 || action.numbers[0] > 511 || action.numbers[0] != std::floor(action.numbers[0]))
			{
				return fail(std::format("{} takes an SDL scancode, 1 to 511", verb));
			}
		}
		else if (verb == "click")
		{
			action.kind = Action::Kind::Click;
			if (words.size() != 2 || (words[1] != "left" && words[1] != "middle" && words[1] != "right"))
			{
				return fail("click takes left, middle or right");
			}
			action.numbers[0] = words[1] == "left" ? 1 : words[1] == "middle" ? 2 : 3;
		}
		else if (verb == "save-atlas" || verb == "save-hud")
		{
			action.kind = verb == "save-atlas" ? Action::Kind::SaveAtlas : Action::Kind::SaveHud;
			action.text = std::string(rest);
			if (action.text.empty())
			{
				return fail(std::format("{} takes a file", verb));
			}
		}
		else if (verb == "hold-check" || verb == "track")
		{
			action.kind = verb == "hold-check" ? Action::Kind::HoldCheck : Action::Kind::Track;
			if (!numbers(1) || action.numbers[0] <= 0)
			{
				return fail(std::format("{} takes a number of seconds", verb));
			}
		}
		else if (verb == "ground")
		{
			action.kind = Action::Kind::Ground;
			if (words.size() != 2 || (words[1] != "floor" && words[1] != "city"))
			{
				return fail("ground takes floor or city");
			}
			action.text = std::string(words[1]);
		}
		else if (verb == "dump")
		{
			action.kind = Action::Kind::Dump;
			action.text = std::string(rest);
			if (action.text.empty())
			{
				return fail("dump takes a collision.obj");
			}
		}
		else if (verb == "wait-ground")
		{
			action.kind = Action::Kind::WaitGround;
			action.numbers[0] = 30;
			if (words.size() > 1 && (!numbers(1) || action.numbers[0] <= 0))
			{
				return fail("wait-ground takes an optional number of seconds");
			}
		}
		else if (verb == "actor")
		{
			action.kind = Action::Kind::Actor;
			action.numbers[5] = 0.6;
			action.numbers[6] = 1.8;
			const size_t given = words.size() >= 8 ? 7 : words.size() - 1;
			bool         ok = words.size() == 5 || words.size() >= 8;
			for (size_t i = 0; ok && i < given; ++i)
			{
				ok = Number(words[i + 1], action.numbers[i]);
			}
			if (!ok || !WholeId(action.numbers[0]) || action.numbers[5] <= 0 || action.numbers[6] <= 0)
			{
				return fail("actor takes a number from 1 to 4294967295, x, y and z; then a yaw, a width and a height above 0; then a name");
			}
			if (words.size() > 8)
			{
				action.text = std::string(Trim(line.substr(size_t(words[8].data() - line.data()))));  // the name, as written
			}
		}
		else if (verb == "actor-remove")
		{
			action.kind = Action::Kind::ActorRemove;
			if (!(words.size() == 2 && words[1] == "all") && (!numbers(1) || !WholeId(action.numbers[0])))
			{
				return fail("actor-remove takes a number from 1 to 4294967295, or all");
			}
		}
		else if (verb == "hurt")
		{
			action.kind = Action::Kind::Hurt;
			size_t given = words.size();
			action.on = given > 3 && words[given - 1] == "power";
			given -= action.on ? 1 : 0;
			const std::array<std::string_view, 4> kinds{ "melee", "projectile", "magic", "other" };
			const auto kind = given >= 2 ? std::find(kinds.begin(), kinds.end(), words[1]) : kinds.end();
			bool       ok = kind != kinds.end() && (given == 3 || given == 4) && Number(words[2], action.numbers[1]) &&
			          action.numbers[1] > 0.0 && action.numbers[1] <= 1.0;
			if (ok && given == 4)
			{
				ok = Number(words[3], action.numbers[2]) && (action.numbers[2] == 0.0 || WholeId(action.numbers[2]));
			}
			if (!ok)
			{
				return fail("hurt takes melee, projectile, magic or other; a share over 0 and up to 1; then an actor's number, or 0; "
				            "then power");
			}
			action.numbers[0] = double(kind - kinds.begin());
		}
		else if (verb == "floor" || verb == "status" || verb == "quit")
		{
			action.kind = verb == "floor" ? Action::Kind::Floor : verb == "status" ? Action::Kind::Status : Action::Kind::Quit;
			if (words.size() != 1)
			{
				return fail(std::format("{} takes nothing", verb));
			}
		}
		else
		{
			return fail("unknown action");
		}
		actions.push_back(std::move(action));
	}
	return actions;
}
}
