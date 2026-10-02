#include "Requests.h"

#include "link/Commands.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <initializer_list>
#include <vector>

namespace cpmc
{
namespace
{
bool IsSpace(char aChar)
{
	return aChar == ' ' || aChar == '\t' || aChar == '\r' || aChar == '\n';
}

std::vector<std::string_view> Words(std::string_view aText)
{
	std::vector<std::string_view> words;
	size_t                        i = 0;
	while (i < aText.size())
	{
		while (i < aText.size() && IsSpace(aText[i]))
		{
			++i;
		}
		const size_t start = i;
		while (i < aText.size() && !IsSpace(aText[i]))
		{
			++i;
		}
		if (i > start)
		{
			words.push_back(aText.substr(start, i - start));
		}
	}
	return words;
}

// A finite number that is the whole word.
std::optional<double> Number(std::string_view aWord)
{
	double     value = 0.0;
	const auto end = aWord.data() + aWord.size();
	const auto [stop, error] = std::from_chars(aWord.data(), end, value);
	if (error != std::errc() || stop != end || !std::isfinite(value))
	{
		return std::nullopt;
	}
	return value;
}

bool OneOf(std::string_view aWord, std::initializer_list<std::string_view> aChoices)
{
	return std::find(aChoices.begin(), aChoices.end(), aWord) != aChoices.end();
}

bool IsSettingName(std::string_view aWord)
{
	return !aWord.empty() && aWord.size() <= 40 && std::all_of(aWord.begin(), aWord.end(), [](char aChar) {
		return (aChar >= 'a' && aChar <= 'z') || (aChar >= '0' && aChar <= '9') || aChar == '.' || aChar == '_';
	});
}

std::optional<Request> Fail(std::string& aError, std::string aWhy)
{
	aError = std::move(aWhy);
	return std::nullopt;
}

std::string_view Trim(std::string_view aText)
{
	while (!aText.empty() && IsSpace(aText.front()))
	{
		aText.remove_prefix(1);
	}
	while (!aText.empty() && IsSpace(aText.back()))
	{
		aText.remove_suffix(1);
	}
	return aText;
}
}

std::optional<Request> ParseRequest(std::string_view aText, std::string& aError)
{
	const std::vector<std::string_view> words = Words(aText);
	if (words.empty())
	{
		return Fail(aError, "the request is empty");
	}
	const std::string_view command = words[0];
	const size_t           arguments = words.size() - 1;
	if (command == "log" || command == "clear" || command == "reload" || command == "floor")
	{
		if (arguments != 0)
		{
			return Fail(aError, std::string(command) + " takes nothing after it");
		}
		const Request::Kind kind = command == "log"     ? Request::Kind::Log
		                           : command == "clear" ? Request::Kind::Clear
		                           : command == "floor" ? Request::Kind::Floor
		                                                : Request::Kind::Reload;
		return Request{ kind };
	}
	if (command == "link")
	{
		if (arguments != 1 || !OneOf(words[1], { "on", "off" }))
		{
			return Fail(aError, "link takes on or off");
		}
		return Request{ Request::Kind::Link, std::string(words[1]) };
	}
	if (command == "mc")
	{
		// The rest of the line, verbatim: a Minecraft command keeps its own spacing.
		const std::string_view rest = Trim(aText.substr(size_t(command.data() - aText.data()) + command.size()));
		if (rest.empty())
		{
			return Fail(aError, "mc takes a set's name, or a command starting with /");
		}
		std::string why;
		if (!link::ValidCommandOrSet(rest, why))
		{
			return Fail(aError, "mc: " + why);
		}
		return Request{ Request::Kind::Mc, std::string(rest) };
	}
	if (command == "dump")
	{
		if (arguments != 1 || !OneOf(words[1], { "depth", "mvec", "color", "gbuffer", "all" }))
		{
			return Fail(aError, "dump takes one of: depth, mvec, color, gbuffer, all");
		}
		return Request{ Request::Kind::Dump, std::string(words[1]) };
	}
	if (command == "mode")
	{
		if (arguments != 1 || !OneOf(words[1], { "off", "present", "predlss", "gbuffer" }))
		{
			return Fail(aError, "mode takes one of: off, present, predlss, gbuffer");
		}
		return Request{ Request::Kind::Mode, std::string(words[1]) };
	}
	if (command == "place")
	{
		if (arguments == 1 && OneOf(words[1], { "cube", "sampler", "house" }))
		{
			return Request{ Request::Kind::Place, std::string(words[1]) };
		}
		if (arguments == 2 && words[1] == "field")
		{
			const std::optional<double> blocks = Number(words[2]);
			if (!blocks || *blocks < 1.0 || *blocks > 1'000'000.0 || *blocks != std::floor(*blocks))
			{
				return Fail(aError, "place field takes a whole number of blocks from 1 to 1000000");
			}
			return Request{ Request::Kind::Place, "field", *blocks };
		}
		return Fail(aError, "place takes cube, sampler, house, or field <blocks>");
	}
	if (command == "set")
	{
		if (arguments != 2 || !IsSettingName(words[1]))
		{
			return Fail(aError, "set takes a name (a-z, 0-9, '.', '_') and a number");
		}
		const std::optional<double> value = Number(words[2]);
		if (!value)
		{
			return Fail(aError, "set takes a finite number as its value");
		}
		return Request{ Request::Kind::Set, std::string(words[1]), *value };
	}
	return Fail(aError, "unknown command \"" + std::string(command.substr(0, 40)) + "\"");
}

PluginSettings ParseSettings(std::string_view aText)
{
	PluginSettings settings;
	for (size_t start = 0; start <= aText.size();)
	{
		const size_t     end = std::min(aText.find('\n', start), aText.size());
		std::string_view line = aText.substr(start, end - start);
		start = end + 1;
		line = line.substr(0, line.find('#'));
		const size_t equals = line.find('=');
		if (equals == std::string_view::npos)
		{
			continue;
		}
		const std::vector<std::string_view> name = Words(line.substr(0, equals));
		const std::vector<std::string_view> value = Words(line.substr(equals + 1));
		if (name.size() != 1 || value.size() != 1)
		{
			continue;
		}
		if (name[0] == "dred")
		{
			settings.dred = value[0] == "1";
		}
		else if (name[0] == "debuglayer")
		{
			settings.debugLayer = value[0] == "1";
		}
	}
	return settings;
}
}
