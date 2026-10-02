#include "link/Messages.h"

#include <skycraft_protocol.h>

#include <cstdlib>
#include <cstring>
#include <format>

namespace cpmc::link
{
namespace proto = skycraft::proto;
static_assert(sizeof(proto::RenVertex) == sizeof(Vertex));
static_assert(sizeof(proto::RenSection) == 16 && sizeof(proto::RenAtlas) == 8);

std::optional<SectionMesh> ParseSection(std::span<const uint8_t> aPayload, bool aKeepTranslucent, std::string& aError)
{
	proto::RenSection header;
	if (aPayload.size() < sizeof header)
	{
		aError = std::format("a section message of {} bytes has no room for its header", aPayload.size());
		return std::nullopt;
	}
	std::memcpy(&header, aPayload.data(), sizeof header);
	const uint64_t room = (aPayload.size() - sizeof header) / sizeof(Vertex);
	if (header.vertexCount > room)
	{
		aError = std::format("section {} {} {} claims {} vertices, with room for {}", header.sx, header.sy, header.sz,
		                     header.vertexCount, room);
		return std::nullopt;
	}
	if (std::abs(int64_t(header.sx)) > kMaxSectionXZ || std::abs(int64_t(header.sz)) > kMaxSectionXZ ||
	    std::abs(int64_t(header.sy)) > kMaxSectionY)
	{
		aError = std::format("section {} {} {} is out of range", header.sx, header.sy, header.sz);
		return std::nullopt;
	}
	SectionMesh mesh{ header.sx, header.sy, header.sz };
	const uint32_t triangles = header.vertexCount / 3;
	mesh.vertices.reserve(size_t(triangles) * 3);
	const uint8_t* source = aPayload.data() + sizeof header;
	for (uint32_t t = 0; t < triangles; ++t)
	{
		Vertex corners[3];
		std::memcpy(corners, source + size_t(t) * sizeof corners, sizeof corners);
		if ((corners[0].flags & kFlagTranslucent) && !aKeepTranslucent)
		{
			++mesh.translucentDropped;
			continue;
		}
		mesh.vertices.insert(mesh.vertices.end(), corners, corners + 3);
	}
	return mesh;
}

std::optional<AtlasImage> ParseAtlas(std::span<const uint8_t> aPayload, std::string& aError)
{
	proto::RenAtlas header;
	if (aPayload.size() < sizeof header)
	{
		aError = std::format("an atlas message of {} bytes has no room for its header", aPayload.size());
		return std::nullopt;
	}
	std::memcpy(&header, aPayload.data(), sizeof header);
	if (header.width == 0 || header.height == 0 || header.width > kMaxAtlasSide || header.height > kMaxAtlasSide)
	{
		aError = std::format("an atlas of {} x {} is out of range", header.width, header.height);
		return std::nullopt;
	}
	const uint64_t bytes = uint64_t(header.width) * header.height * 4;
	if (aPayload.size() - sizeof header < bytes)
	{
		aError = std::format("a {} x {} atlas needs {} bytes, and has {}", header.width, header.height, bytes, aPayload.size() - sizeof header);
		return std::nullopt;
	}
	return AtlasImage{ header.width, header.height, aPayload.subspan(sizeof header, size_t(bytes)) };
}
}
