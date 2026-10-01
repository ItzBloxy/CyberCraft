#pragma once

#include "Coords.h"

#include <optional>
#include <string_view>

namespace cpmc
{
// What dump.request asks for. A blank file asks for a plain dump. "origin <x> <y> <z>" says where PhysX's origin
// sits in world coordinates (metres), so the dump reads PhysX at world - origin.
struct DumpRequest
{
	std::optional<Vec3d> physxOrigin;
};

// Nothing if the text is neither blank nor one valid origin line.
std::optional<DumpRequest> ParseDumpRequest(std::string_view aText);
}
