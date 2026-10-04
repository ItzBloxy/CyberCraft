#include "Harness.h"

#include "link/Solids.h"

#include <cstring>
#include <initializer_list>
#include <vector>

using namespace cpmc;
using namespace cpmc::link;

namespace
{
// A kRenSolids payload, as SkyCraft's WorldExporter writes it: the section and the count, then the bits unless the count
// is 0. aBlocks are block positions within the section.
std::vector<uint8_t> SolidsPayload(int32_t aX, int32_t aY, int32_t aZ, std::initializer_list<BlockPos> aBlocks)
{
	SolidBits bits{};
	for (const BlockPos& block : aBlocks)
	{
		const int bit = block.x + 16 * block.z + 256 * block.y;
		bits[size_t(bit >> 6)] |= uint64_t(1) << (bit & 63);
	}
	const uint32_t       count = uint32_t(aBlocks.size());
	std::vector<uint8_t> payload(16 + (count ? 512 : 0));
	const int32_t        header[3] = { aX, aY, aZ };
	std::memcpy(payload.data(), header, 12);
	std::memcpy(payload.data() + 12, &count, 4);
	if (count)
	{
		std::memcpy(payload.data() + 16, bits.data(), 512);
	}
	return payload;
}

void SetCount(std::vector<uint8_t>& aPayload, uint32_t aCount)
{
	std::memcpy(aPayload.data() + 12, &aCount, 4);
}
}

TEST(SolidsParseSkyCraftsMessage)
{
	std::string                        error;
	const std::optional<SolidsMessage> solids = ParseSolids(SolidsPayload(-8, 0, 125, { { 0, 0, 0 }, { 15, 0, 0 }, { 0, 15, 15 } }), error);
	CHECK(solids.has_value());
	if (solids)
	{
		CHECK((solids->section == SectionPos{ -8, 0, 125 }));
		CHECK_EQ(solids->count, 3u);
		CHECK_EQ(solids->bits[0], (uint64_t(1) << 0) | (uint64_t(1) << 15));  // bits 0 and 15
		CHECK_EQ(solids->bits[63], uint64_t(1) << 48);                        // bit 4,080: 15z + 15 * 256y
	}
	const std::optional<SolidsMessage> none = ParseSolids(SolidsPayload(3, -4, 7, {}), error);  // the section has none left
	CHECK((none.has_value() && none->count == 0 && none->section == SectionPos{ 3, -4, 7 }));
}

TEST(SolidsRejectWhatIsntSkyCraftsMessage)
{
	std::string error;
	CHECK(!ParseSolids(std::vector<uint8_t>(8), error).has_value());
	CHECK_STR(error, "a solids message of 8 bytes has no room for its header");
	std::vector<uint8_t> payload = SolidsPayload(1, 2, 3, { { 1, 1, 1 } });
	payload.resize(16);
	CHECK(!ParseSolids(payload, error).has_value());
	CHECK_STR(error, "solids for section 1 2 3 claim 1 blocks, with no room for their bits");
	payload = SolidsPayload(1, 2, 3, { { 1, 1, 1 } });
	SetCount(payload, 5000);
	CHECK(!ParseSolids(payload, error).has_value());
	CHECK_STR(error, "solids for section 1 2 3 claim 5000 blocks, more than a section holds");
	SetCount(payload, 2);
	CHECK(!ParseSolids(payload, error).has_value());
	CHECK_STR(error, "solids for section 1 2 3 claim 2 blocks, and their bits hold 1");
	CHECK(!ParseSolids(SolidsPayload(1, 300, 3, { { 1, 1, 1 } }), error).has_value());
	CHECK_STR(error, "solids for section 1 300 3 are out of range");
	CHECK(!ParseSolids(SolidsPayload(-3'000'000, 0, 3, { { 1, 1, 1 } }), error).has_value());
}

TEST(SolidsKeepEachSectionsLatest)
{
	Solids      solids;
	std::string error;
	solids.Set(*ParseSolids(SolidsPayload(0, 0, 0, { { 0, 0, 0 }, { 1, 0, 0 } }), error));
	solids.Set(*ParseSolids(SolidsPayload(1, 0, 0, { { 5, 5, 5 } }), error));
	CHECK_EQ(solids.Sections(), size_t(2));
	CHECK_EQ(solids.Blocks(), 3ull);
	const Solids::Section* first = solids.Find({ 0, 0, 0 });
	CHECK(first != nullptr && first->bits[0] == 3);
	const uint64_t version = first ? first->version : 0;
	// A block placed: the section's bits are replaced, and its version goes up, so its body is rebuilt.
	solids.Set(*ParseSolids(SolidsPayload(0, 0, 0, { { 0, 0, 0 }, { 1, 0, 0 }, { 2, 0, 0 } }), error));
	first = solids.Find({ 0, 0, 0 });
	CHECK(first != nullptr && first->bits[0] == 7 && first->version > version);
	CHECK_EQ(solids.Blocks(), 4ull);
	// Its last block broken: the section goes.
	solids.Set(*ParseSolids(SolidsPayload(0, 0, 0, {}), error));
	CHECK(solids.Find({ 0, 0, 0 }) == nullptr);
	CHECK_EQ(solids.Sections(), size_t(1));
	CHECK_EQ(solids.Blocks(), 1ull);
	CHECK(solids.Find({ 2, 0, 0 }) == nullptr);  // never had any
	// Minecraft's clear-all: everything goes.
	solids.Clear();
	CHECK_EQ(solids.Sections(), size_t(0));
	CHECK_EQ(solids.Blocks(), 0ull);
}
