#include "link/Messages.h"

#include <skycraft_protocol.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <format>

namespace cpmc::link
{
namespace proto = skycraft::proto;
static_assert(sizeof(proto::RenVertex) == sizeof(Vertex));
static_assert(sizeof(proto::RenSection) == 16 && sizeof(proto::RenAtlas) == 8);
static_assert(sizeof(proto::RenTexture) == 16 && sizeof(proto::RenScene) == 32 && sizeof(proto::RenBatch) == 16);

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

std::optional<TextureImage> ParseTexture(std::span<const uint8_t> aPayload, std::string& aError)
{
	proto::RenTexture header;
	if (aPayload.size() < sizeof header)
	{
		aError = std::format("a texture message of {} bytes has no room for its header", aPayload.size());
		return std::nullopt;
	}
	std::memcpy(&header, aPayload.data(), sizeof header);
	if (header.id == 0)
	{
		aError = std::format("a texture of {} x {} has id 0", header.width, header.height);
		return std::nullopt;
	}
	if (header.width == 0 || header.height == 0 || header.width > kMaxTextureSide || header.height > kMaxTextureSide)
	{
		aError = std::format("texture {} of {} x {} is out of range", header.id, header.width, header.height);
		return std::nullopt;
	}
	const uint64_t bytes = uint64_t(header.width) * header.height * 4;
	if (aPayload.size() - sizeof header < bytes)
	{
		aError = std::format("texture {} of {} x {} needs {} bytes, and has {}", header.id, header.width, header.height, bytes,
		                     aPayload.size() - sizeof header);
		return std::nullopt;
	}
	return TextureImage{ header.id, header.width, header.height, aPayload.subspan(sizeof header, size_t(bytes)) };
}

std::optional<SceneMessage> ParseScene(std::span<const uint8_t> aPayload, std::string& aError)
{
	proto::RenScene header;
	if (aPayload.size() < sizeof header)
	{
		aError = std::format("a scene message of {} bytes has no room for its header", aPayload.size());
		return std::nullopt;
	}
	std::memcpy(&header, aPayload.data(), sizeof header);
	if (!std::isfinite(header.originX) || !std::isfinite(header.originY) || !std::isfinite(header.originZ))
	{
		aError = "a scene's origin isn't finite";
		return std::nullopt;
	}
	if (header.batchCount > kMaxSceneBatches || header.vertexCount > kMaxSceneVertices)
	{
		aError = std::format("a scene of {} batches and {} vertices is too big", header.batchCount, header.vertexCount);
		return std::nullopt;
	}
	const uint64_t batchBytes = uint64_t(header.batchCount) * sizeof(proto::RenBatch);
	const uint64_t vertexBytes = uint64_t(header.vertexCount) * sizeof(Vertex);
	if (aPayload.size() - sizeof header < batchBytes + vertexBytes)
	{
		aError = std::format("a scene of {} batches and {} vertices needs {} bytes, and has {}", header.batchCount, header.vertexCount,
		                     batchBytes + vertexBytes, aPayload.size() - sizeof header);
		return std::nullopt;
	}
	SceneMessage scene;
	scene.origin = { header.originX, header.originY, header.originZ };
	scene.vertexCount = header.vertexCount;
	scene.batches.reserve(header.batchCount);
	for (uint32_t b = 0; b < header.batchCount; ++b)
	{
		proto::RenBatch batch;
		std::memcpy(&batch, aPayload.data() + sizeof header + size_t(b) * sizeof batch, sizeof batch);
		if (batch.count % 3 != 0 || uint64_t(batch.first) + batch.count > header.vertexCount)
		{
			aError = std::format("a scene's batch {} has {} vertices from {}, of {}", b, batch.count, batch.first, header.vertexCount);
			return std::nullopt;
		}
		scene.batches.push_back({ batch.texture, batch.first, batch.count, batch.flags });
	}
	scene.vertices = aPayload.subspan(sizeof header + size_t(batchBytes), size_t(vertexBytes));
	return scene;
}

std::vector<Vertex> SceneVertices(const SceneMessage& aScene)
{
	std::vector<Vertex> vertices(aScene.vertexCount);
	if (!vertices.empty())
	{
		std::memcpy(vertices.data(), aScene.vertices.data(), vertices.size() * sizeof(Vertex));
	}
	return vertices;
}
}
