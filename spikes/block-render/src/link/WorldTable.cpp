#include "link/WorldTable.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <map>

namespace cpmc::link
{
namespace
{
bool Finite(const float* aValues, size_t aCount)
{
	return std::all_of(aValues, aValues + aCount, [](float aValue) { return std::isfinite(aValue); });
}

bool Usable(const proto::WorldEntity& aRecord)
{
	const float numbers[] = { aRecord.x, aRecord.y, aRecord.z, aRecord.yaw, aRecord.pitch, aRecord.scale };
	return aRecord.kind >= proto::kWeArrow && aRecord.kind <= proto::kWeShadow && Finite(numbers, std::size(numbers)) &&
	       Finite(aRecord.ext, 3) && Finite(&aRecord.uv[0][0], 12);
}
}

WorldTable CheckWorldTable(const proto::WorldEntities& aTable)
{
	WorldTable     table;
	const uint32_t count = std::min(aTable.count, proto::kMaxWorldEntities);
	for (uint32_t i = 0; i < count; ++i)
	{
		if (Usable(aTable.entities[i]))
		{
			table.entities.push_back(aTable.entities[i]);
		}
		else
		{
			++table.leftOut;
		}
	}
	if (aTable.hasSelection && Finite(aTable.selMin, 3) && Finite(aTable.selMax, 3))
	{
		table.outline = std::array<float, 6>{ aTable.selMin[0], aTable.selMin[1], aTable.selMin[2],
			                                  aTable.selMax[0], aTable.selMax[1], aTable.selMax[2] };
	}
	return table;
}

const char* WorldKindName(uint32_t aKind)
{
	switch (aKind)
	{
	case proto::kWeArrow: return "arrow";
	case proto::kWeItem: return "item";
	case proto::kWeTrident: return "trident";
	case proto::kWeBlock: return "block";
	case proto::kWeCrack: return "crack";
	case proto::kWeShadow: return "shadow";
	default: return "other";
	}
}

std::string WorldWords(const WorldTable& aTable)
{
	std::map<std::string, int> kinds;
	for (const proto::WorldEntity& record : aTable.entities)
	{
		++kinds[WorldKindName(record.kind)];
	}
	std::string words;
	for (const auto& [kind, count] : kinds)
	{
		words += std::format("{}{} {}{}", words.empty() ? "" : ", ", count, kind, count == 1 ? "" : "s");
	}
	if (aTable.outline)
	{
		words += words.empty() ? "an outline" : "; an outline";
	}
	return words.empty() ? "nothing" : words;
}
}
