#include "DumpObj.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <format>

namespace cpmc::mclink
{
namespace
{
std::vector<std::string_view> Words(std::string_view aLine)
{
	std::vector<std::string_view> words;
	size_t                        at = 0;
	while (at < aLine.size())
	{
		const size_t start = aLine.find_first_not_of(" \t", at);
		if (start == std::string_view::npos)
		{
			break;
		}
		const size_t end = aLine.find_first_of(" \t", start);
		words.push_back(aLine.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
		at = end == std::string_view::npos ? aLine.size() : end;
	}
	return words;
}

bool Number(std::string_view aWord, double& aOut)
{
	const auto [end, error] = std::from_chars(aWord.data(), aWord.data() + aWord.size(), aOut);
	return error == std::errc() && end == aWord.data() + aWord.size() && std::isfinite(aOut);
}

bool Index(std::string_view aWord, size_t& aOut)
{
	const auto [end, error] = std::from_chars(aWord.data(), aWord.data() + aWord.size(), aOut);
	return error == std::errc() && end == aWord.data() + aWord.size() && aOut >= 1;
}
}

std::optional<std::vector<link::McTriangle>> ParseDumpObj(std::string_view aText, std::string_view aPrefix, std::string& aError)
{
	std::vector<std::array<double, 3>> vertices;
	std::vector<link::McTriangle>      triangles;
	bool                               keep = false;  // the object the lines are in is one asked for
	int                                lineNumber = 0;
	size_t                             at = 0;
	while (at < aText.size())
	{
		const size_t     end = aText.find('\n', at);
		std::string_view line = aText.substr(at, end == std::string_view::npos ? std::string_view::npos : end - at);
		at = end == std::string_view::npos ? aText.size() : end + 1;
		++lineNumber;
		const std::vector<std::string_view> words = Words(line.ends_with('\r') ? line.substr(0, line.size() - 1) : line);
		if (words.empty() || words[0].starts_with('#') || words[0] == "mtllib" || words[0] == "usemtl")
		{
			continue;
		}
		if (words[0] == "o" && words.size() == 2)
		{
			keep = words[1].starts_with(aPrefix);
			continue;
		}
		if (words[0] == "v" && words.size() == 4)
		{
			std::array<double, 3> vertex{};
			if (Number(words[1], vertex[0]) && Number(words[2], vertex[1]) && Number(words[3], vertex[2]))
			{
				vertices.push_back(vertex);
				continue;
			}
		}
		else if (words[0] == "f" && words.size() == 4)
		{
			size_t corners[3] = {};
			if (Index(words[1], corners[0]) && Index(words[2], corners[1]) && Index(words[3], corners[2]) &&
			    std::max({ corners[0], corners[1], corners[2] }) <= vertices.size())
			{
				if (keep)
				{
					link::McTriangle triangle{};
					for (int k = 0; k < 3; ++k)
					{
						for (int axis = 0; axis < 3; ++axis)
						{
							triangle.v[k * 3 + axis] = float(vertices[corners[k] - 1][axis]);
						}
					}
					triangles.push_back(triangle);
				}
				continue;
			}
		}
		aError = std::format("line {}: \"{}\" isn't a vertex, a triangle of vertices before it, or an object", lineNumber,
		                     line.substr(0, 80));
		return std::nullopt;
	}
	return triangles;
}
}
