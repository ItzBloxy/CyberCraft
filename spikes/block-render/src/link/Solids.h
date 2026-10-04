#pragma once

#include "world/BlockWorld.h"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>

// Minecraft's solid blocks (spike 5's "Part 3", "Solids"): after each section's mesh, SkyCraft sends kRenSolids, which of
// the section's blocks have any collision shape, as a 4,096-bit set (bit x + 16z + 256y). The link keeps them by section,
// for V's bodies, whether drawing is on or not, as it does sections. Shared memory is written by another process, so each
// message is checked before it's kept. Pure.
namespace cpmc::link
{
using SolidBits = std::array<uint64_t, 64>;  // bit x + 16z + 256y: the section's block (x, y, z) is solid

struct SolidsMessage
{
	SectionPos section;
	uint32_t   count = 0;  // solid blocks; 0: the section has none left
	SolidBits  bits{};
};

// A kRenSolids payload: {i32 sx, sy, sz; u32 count}, then 512 bytes of bits unless the count is 0. Nothing, with aError
// saying why, if the coordinates are out of spike 4's ranges, the count is over 4,096 or isn't the number of bits set, or
// the bits are missing.
std::optional<SolidsMessage> ParseSolids(std::span<const uint8_t> aPayload, std::string& aError);

class Solids
{
public:
	struct Section
	{
		SolidBits bits{};
		uint32_t  count = 0;
		uint64_t  version = 0;  // new with each change: a body built from an older one is out of date
	};

	// A section's solids, replacing what it had: a count of 0 forgets the section.
	void Set(const SolidsMessage& aMessage);
	// Minecraft's clear-all: every section goes.
	void Clear();

	const Section* Find(const SectionPos& aSection) const;  // null: no solid blocks there
	size_t         Sections() const { return sections_.size(); }
	uint64_t       Blocks() const { return blocks_; }  // the sections' solid blocks together

private:
	std::map<SectionPos, Section> sections_;
	uint64_t                      blocks_ = 0;
	uint64_t                      version_ = 0;
};
}
