#include "link/Solids.h"

#include "link/Messages.h"

#include <skycraft_protocol.h>

#include <bit>
#include <cstdlib>
#include <cstring>
#include <format>

namespace cpmc::link
{
namespace proto = skycraft::proto;
static_assert(sizeof(proto::RenSolids) == 16 && sizeof(SolidBits) == 512);

std::optional<SolidsMessage> ParseSolids(std::span<const uint8_t> aPayload, std::string& aError)
{
	proto::RenSolids header;
	if (aPayload.size() < sizeof header)
	{
		aError = std::format("a solids message of {} bytes has no room for its header", aPayload.size());
		return std::nullopt;
	}
	std::memcpy(&header, aPayload.data(), sizeof header);
	if (std::abs(int64_t(header.sx)) > kMaxSectionXZ || std::abs(int64_t(header.sz)) > kMaxSectionXZ ||
	    std::abs(int64_t(header.sy)) > kMaxSectionY)
	{
		aError = std::format("solids for section {} {} {} are out of range", header.sx, header.sy, header.sz);
		return std::nullopt;
	}
	SolidsMessage solids{ { header.sx, header.sy, header.sz }, header.count };
	if (header.count == 0)
	{
		return solids;
	}
	if (header.count > 4096)
	{
		aError = std::format("solids for section {} {} {} claim {} blocks, more than a section holds", header.sx, header.sy, header.sz,
		                     header.count);
		return std::nullopt;
	}
	if (aPayload.size() - sizeof header < sizeof solids.bits)
	{
		aError = std::format("solids for section {} {} {} claim {} blocks, with no room for their bits", header.sx, header.sy,
		                     header.sz, header.count);
		return std::nullopt;
	}
	std::memcpy(solids.bits.data(), aPayload.data() + sizeof header, sizeof solids.bits);
	uint32_t set = 0;
	for (const uint64_t word : solids.bits)
	{
		set += uint32_t(std::popcount(word));
	}
	if (set != header.count)
	{
		aError = std::format("solids for section {} {} {} claim {} blocks, and their bits hold {}", header.sx, header.sy, header.sz,
		                     header.count, set);
		return std::nullopt;
	}
	return solids;
}

void Solids::Set(const SolidsMessage& aMessage)
{
	if (const auto old = sections_.find(aMessage.section); old != sections_.end())
	{
		blocks_ -= old->second.count;
		sections_.erase(old);
	}
	if (aMessage.count == 0)
	{
		return;
	}
	sections_[aMessage.section] = { aMessage.bits, aMessage.count, ++version_ };
	blocks_ += aMessage.count;
}

void Solids::Clear()
{
	sections_.clear();
	blocks_ = 0;
}

const Solids::Section* Solids::Find(const SectionPos& aSection) const
{
	const auto found = sections_.find(aSection);
	return found == sections_.end() ? nullptr : &found->second;
}
}
