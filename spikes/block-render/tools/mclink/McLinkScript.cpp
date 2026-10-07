#include "McLinkScript.h"

#include <skycraft_protocol.h>

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

// A whole number from 0 to aMost.
bool WholeUpTo(double aValue, double aMost)
{
	return aValue >= 0.0 && aValue <= aMost && aValue == std::floor(aValue);
}

namespace proto = skycraft::proto;
}

std::optional<std::vector<uint32_t>> CodePoints(std::string_view aUtf8)
{
	constexpr uint32_t    kSmallest[5] = { 0, 0, 0x80, 0x800, 0x10000 };  // by length: anything below is overlong
	std::vector<uint32_t> points;
	size_t                at = 0;
	while (at < aUtf8.size())
	{
		const uint8_t lead = uint8_t(aUtf8[at]);
		const size_t  length = lead < 0x80 ? 1 : (lead & 0xE0) == 0xC0 ? 2 : (lead & 0xF0) == 0xE0 ? 3 : (lead & 0xF8) == 0xF0 ? 4 : 0;
		if (length == 0 || at + length > aUtf8.size())
		{
			return std::nullopt;
		}
		uint32_t point = length == 1 ? lead : lead & (0x7Fu >> length);
		for (size_t i = 1; i < length; ++i)
		{
			const uint8_t next = uint8_t(aUtf8[at + i]);
			if ((next & 0xC0) != 0x80)
			{
				return std::nullopt;
			}
			point = point << 6 | (next & 0x3Fu);
		}
		if (point < kSmallest[length] || point > 0x10FFFF || (point >= 0xD800 && point <= 0xDFFF))
		{
			return std::nullopt;
		}
		points.push_back(point);
		at += length;
	}
	return points;
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
		else if (verb == "health")
		{
			action.kind = Action::Kind::Health;
			if (!numbers(1) || action.numbers[0] <= 0.0 || action.numbers[0] > 1.0)
			{
				return fail("health takes a share of Minecraft's max health, over 0 and up to 1");
			}
		}
		else if (verb == "save-textures")
		{
			action.kind = Action::Kind::SaveTextures;
			action.text = std::string(rest);
			if (action.text.empty())
			{
				return fail("save-textures takes a folder");
			}
		}
		else if (verb == "cursor")
		{
			action.kind = Action::Kind::Cursor;
			if (!numbers(2) || !WholeUpTo(action.numbers[0], proto::kMaxOverlayW - 1) || !WholeUpTo(action.numbers[1], proto::kMaxOverlayH - 1))
			{
				return fail("cursor takes whole HUD pixels, x from 0 to 3839 and y from 0 to 2159");
			}
		}
		else if (verb == "text")
		{
			action.kind = Action::Kind::Text;
			action.text = std::string(rest);
			const auto points = CodePoints(action.text);
			if (action.text.empty() || !points ||
			    std::any_of(points->begin(), points->end(), [](uint32_t aPoint) { return aPoint < 0x20 || aPoint == 0x7F; }))
			{
				return fail("text takes printable UTF-8");
			}
		}
		else if (verb == "wait-screen")
		{
			action.kind = Action::Kind::WaitScreen;
			action.numbers[0] = 5;
			action.on = words.size() >= 2 && words[1] == "open";
			bool ok = (words.size() == 2 || words.size() == 3) && (action.on || words[1] == "closed");
			if (ok && words.size() == 3)
			{
				ok = Number(words[2], action.numbers[0]) && action.numbers[0] > 0;
			}
			if (!ok)
			{
				return fail("wait-screen takes open or closed, then an optional number of seconds");
			}
		}
		else if (verb == "hud-box")
		{
			action.kind = Action::Kind::HudBox;
			if (!numbers(4) || !WholeUpTo(action.numbers[0], proto::kMaxOverlayW) || !WholeUpTo(action.numbers[1], proto::kMaxOverlayH) ||
			    !WholeUpTo(action.numbers[2], proto::kMaxOverlayW) || !WholeUpTo(action.numbers[3], proto::kMaxOverlayH) ||
			    action.numbers[0] >= action.numbers[2] || action.numbers[1] >= action.numbers[3])
			{
				return fail("hud-box takes whole HUD pixels x0 y0 x1 y1, x0 below x1 up to 3840, y0 below y1 up to 2160");
			}
		}
		else if (verb == "floor" || verb == "scene" || verb == "pause" || verb == "status" || verb == "quit")
		{
			action.kind = verb == "floor"    ? Action::Kind::Floor
			              : verb == "scene"  ? Action::Kind::Scene
			              : verb == "pause"  ? Action::Kind::Pause
			              : verb == "status" ? Action::Kind::Status
			                                 : Action::Kind::Quit;
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
