#include "ObjWriter.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <unordered_map>

namespace cpmc
{
namespace
{
struct MmKey
{
	int64_t x, y, z;
	bool    operator==(const MmKey&) const = default;
};

struct MmKeyHash
{
	size_t operator()(const MmKey& aKey) const
	{
		return std::hash<int64_t>()(aKey.x) ^ (std::hash<int64_t>()(aKey.y) << 1) ^ (std::hash<int64_t>()(aKey.z) << 2);
	}
};

// Millimetres as "-12.345" without floating-point formatting (so no "-0.000").
std::string FormatMm(int64_t aMm)
{
	const bool    negative = aMm < 0;
	const int64_t magnitude = negative ? -aMm : aMm;
	return std::format("{}{}.{:03}", negative ? "-" : "", magnitude / 1000, magnitude % 1000);
}

std::string_view WithoutScenePrefix(std::string_view aCategory)
{
	// "s<digits>_" in front of a category name.
	if (aCategory.size() > 2 && aCategory[0] == 's')
	{
		size_t i = 1;
		while (i < aCategory.size() && aCategory[i] >= '0' && aCategory[i] <= '9')
		{
			++i;
		}
		if (i > 1 && i < aCategory.size() && aCategory[i] == '_')
		{
			return aCategory.substr(i + 1);
		}
	}
	return aCategory;
}
}

std::array<float, 3> CategoryColor(std::string_view aCategory)
{
	const std::string_view name = WithoutScenePrefix(aCategory);
	const auto             startsWith = [&](std::string_view aPrefix) { return name.substr(0, aPrefix.size()) == aPrefix; };
	if (startsWith("trigger_"))
	{
		return { 0.95f, 0.85f, 0.20f };
	}
	if (startsWith("queryonly_"))
	{
		return { 0.75f, 0.35f, 0.85f };
	}
	if (startsWith("kinematic_"))
	{
		return { 0.95f, 0.55f, 0.20f };
	}
	if (startsWith("dynamic_"))
	{
		return { 0.25f, 0.55f, 0.95f };
	}
	if (name == "static_heightfield")
	{
		return { 0.45f, 0.62f, 0.38f };
	}
	if (name == "static_convex")
	{
		return { 0.70f, 0.66f, 0.55f };
	}
	if (name == "static_box")
	{
		return { 0.55f, 0.60f, 0.70f };
	}
	if (name == "static_sphere" || name == "static_capsule")
	{
		return { 0.60f, 0.55f, 0.70f };
	}
	return { 0.62f, 0.62f, 0.62f };  // static_trimesh and anything else
}

void WriteObj(std::ostream& aOut, const std::vector<ObjGroup>& aGroups, const Vec3d& aOrigin,
              std::string_view aMtlFileName, std::string_view aComment)
{
	aOut << "# PhysXProbe collision dump\n";
	if (!aComment.empty())
	{
		aOut << "# " << aComment << '\n';
	}
	aOut << "mtllib " << aMtlFileName << '\n';
	uint64_t nextIndex = 1;  // OBJ indices are 1-based and count across the whole file
	for (const ObjGroup& group : aGroups)
	{
		aOut << "o " << group.name << '\n' << "usemtl " << group.name << '\n';
		std::unordered_map<MmKey, uint64_t, MmKeyHash> shared;
		std::string                                    faces;
		for (const px::Tri& tri : group.triangles)
		{
			uint64_t index[3];
			for (int k = 0; k < 3; ++k)
			{
				const Vec3d relative{ double(tri.v[k].x) - aOrigin.x, double(tri.v[k].y) - aOrigin.y,
				                      double(tri.v[k].z) - aOrigin.z };
				const Vec3d mc = McFromWorld(relative);
				const MmKey key{ std::llround(mc.x * 1000.0), std::llround(mc.y * 1000.0), std::llround(mc.z * 1000.0) };
				const auto  found = shared.find(key);
				if (found != shared.end())
				{
					index[k] = found->second;
					continue;
				}
				index[k] = nextIndex++;
				shared.emplace(key, index[k]);
				aOut << "v " << FormatMm(key.x) << ' ' << FormatMm(key.y) << ' ' << FormatMm(key.z) << '\n';
			}
			if (index[0] != index[1] && index[1] != index[2] && index[0] != index[2])
			{
				faces += std::format("f {} {} {}\n", index[0], index[1], index[2]);
			}
		}
		aOut << faces;
	}
}

void WriteMtl(std::ostream& aOut, const std::vector<ObjGroup>& aGroups)
{
	for (const ObjGroup& group : aGroups)
	{
		aOut << "newmtl " << group.name << '\n'
		     << std::format("Kd {:.3f} {:.3f} {:.3f}\n", group.color[0], group.color[1], group.color[2]);
	}
}
}
