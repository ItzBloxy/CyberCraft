#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace cpmc
{
// Minecraft block coordinates: x east, y up, z south.
struct BlockPos
{
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;

	auto operator<=>(const BlockPos&) const = default;
};

// A 16 x 16 x 16 section: block (x, y, z) lies in section (x >> 4, y >> 4, z >> 4).
struct SectionPos
{
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;

	auto operator<=>(const SectionPos&) const = default;
};

SectionPos SectionOf(const BlockPos& aBlock);

// Blocks by position, in sparse sections. Block IDs are the atlas's (0 is air). Main thread only.
class BlockWorld
{
public:
	using Section = std::array<uint16_t, 4096>;  // index: x + 16 * (z + 16 * y), local coordinates

	uint16_t Get(const BlockPos& aBlock) const;

	// Changes one block. Its section needs meshing again, and so does a neighbouring section when the block lies on
	// the border with it.
	void Set(const BlockPos& aBlock, uint16_t aId);

	// Removes every block. Every section that had blocks needs meshing again (to nothing).
	void Clear();

	// Every section needs meshing again, as when the meshing options change.
	void MarkAllDirty();

	// The sections changed since the last call, in order.
	std::vector<SectionPos> TakeDirty();

	const Section* Find(const SectionPos& aSection) const;  // null if it has never held a block
	size_t         BlockCount() const { return blocks_; }
	size_t         SectionCount() const { return sections_.size(); }

private:
	std::map<SectionPos, std::unique_ptr<Section>> sections_;
	std::set<SectionPos>                           dirty_;
	size_t                                         blocks_ = 0;
};
}
