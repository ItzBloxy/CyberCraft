#pragma once

#include "Coords.h"
#include "physx/ShapeTriangles.h"

#include <array>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace cpmc
{
// One OBJ object group: world-space triangles of one category.
struct ObjGroup
{
	std::string          name;
	std::array<float, 3> color{};
	std::vector<px::Tri> triangles;
};

// A fixed colour per category, by kind (static, kinematic, dynamic, trigger, queryonly) and geometry.
// A scene prefix such as "s1_" is ignored.
std::array<float, 3> CategoryColor(std::string_view aCategory);

// Writes the groups as OBJ text in Minecraft axes, relative to aOrigin (world), with millimetre precision.
// Identical vertices are shared within each group; faces that collapse to fewer than 3 vertices are dropped.
void WriteObj(std::ostream& aOut, const std::vector<ObjGroup>& aGroups, const Vec3d& aOrigin,
              std::string_view aMtlFileName, std::string_view aComment);

// One material per group, named after it, with the group's colour as Kd.
void WriteMtl(std::ostream& aOut, const std::vector<ObjGroup>& aGroups);
}
