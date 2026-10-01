#pragma once

#include "Coords.h"

#include <optional>
#include <string_view>

namespace cpmc
{
// What dump.request asks for. A blank file asks for a plain dump. "origin <x> <y> <z>" says where PhysX's origin
// sits in world coordinates (metres), so the dump reads PhysX at world - origin. "watch" and "stop" start and stop
// the origin watch.
struct DumpRequest
{
	enum class Action
	{
		Dump,
		StartWatch,
		StopWatch,
	};

	Action               action = Action::Dump;
	std::optional<Vec3d> physxOrigin;
};

// Nothing if the text is none of: blank, one valid origin line, "watch" or "stop".
std::optional<DumpRequest> ParseDumpRequest(std::string_view aText);
}
