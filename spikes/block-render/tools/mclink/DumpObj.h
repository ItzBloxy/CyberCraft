#pragma once

#include "link/Voxels.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Spike 1's saved collision, for step 3's offline half (spike 5's "mclink's dump loader"): a dump's collision.obj holds
// each category's triangles as one OBJ object ("s0_static_trimesh", "s0_dynamic_box" and so on), in Minecraft's axes
// relative to V's feet when it was dumped, to the millimetre. The "s0_static_" objects are scene 0's static actors'
// simulation shapes, nearly all of them scene-query shapes too: they stand in for what the exporter keeps.
namespace cpmc::mclink
{
// The triangles of the objects whose names start with aPrefix, relative to the dump's V's feet; or nothing, with aError
// naming the first line that isn't a vertex, a face of three vertices that are there, an object, or one of the lines
// the probe writes for its materials.
std::optional<std::vector<link::McTriangle>> ParseDumpObj(std::string_view aText, std::string_view aPrefix, std::string& aError);
}
