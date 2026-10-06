#pragma once

#include <skycraft_protocol.h>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

// Spike 7: Minecraft's world entities (SkyCraft's WorldExporter), the light path for small things SkyCraft's game side
// builds itself: arrows, dropped and thrown items, tridents, dropped blocks, block-breaking cracks and contact shadows,
// and the targeted block's outline. Checked before use: shared memory is written by another process.
namespace cpmc::link
{
namespace proto = skycraft::proto;

struct WorldTable
{
	std::vector<proto::WorldEntity>     entities;  // the records that passed the checks, in the table's order
	std::optional<std::array<float, 6>> outline;   // the targeted block's box, its low corner then its high, Minecraft axes
	uint32_t                            leftOut = 0;  // records with a kind it doesn't have, or a number that isn't finite
};

// The table's records, at most kMaxWorldEntities whatever its count says, and its outline when it has a whole one.
WorldTable CheckWorldTable(const proto::WorldEntities& aTable);

// A record's kind as a word: "arrow", "item", "trident", "block", "crack" or "shadow"; "other" for anything else.
const char* WorldKindName(uint32_t aKind);
}
