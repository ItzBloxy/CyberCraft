#include "DumpRequest.h"

#include <charconv>
#include <cmath>
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
}

std::optional<DumpRequest> ParseDumpRequest(std::string_view aText)
{
	const std::vector<std::string_view> words = Words(aText);
	if (words.empty())
	{
		return DumpRequest{};
	}
	if (words.size() != 4 || words[0] != "origin")
	{
		return std::nullopt;
	}
	const std::optional<double> x = Number(words[1]);
	const std::optional<double> y = Number(words[2]);
	const std::optional<double> z = Number(words[3]);
	if (!x || !y || !z)
	{
		return std::nullopt;
	}
	return DumpRequest{ Vec3d{ *x, *y, *z } };
}
}
