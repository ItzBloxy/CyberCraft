#include "Harness.h"

#include "link/Messages.h"

#include <array>
#include <cmath>
#include <cstring>

using namespace cpmc;
using namespace cpmc::link;

namespace
{
// Little-endian bytes, as SkyCraft's WorldExporter writes them with a little-endian ByteBuffer.
struct Bytes
{
	template<class T>
	Bytes& Put(T aValue)
	{
		uint8_t raw[sizeof(T)];
		std::memcpy(raw, &aValue, sizeof raw);
		data.insert(data.end(), raw, raw + sizeof raw);
		return *this;
	}
	std::vector<uint8_t> data;
};

// MeshBuilder.vertex: x, y, z, u, v as floats, then r, g, b, a, then the light and the flags.
void PutVertex(Bytes& aBytes, float aX, uint32_t aFlags)
{
	aBytes.Put(aX).Put(2.0f).Put(3.0f).Put(0.25f).Put(0.5f);
	aBytes.Put(uint8_t(200)).Put(uint8_t(150)).Put(uint8_t(100)).Put(uint8_t(255));
	aBytes.Put(uint32_t(15 << 8)).Put(aFlags);
}

std::vector<uint8_t> Section(int32_t aX, int32_t aY, int32_t aZ, uint32_t aCount, const std::vector<uint32_t>& aFlags)
{
	Bytes bytes;
	bytes.Put(aX).Put(aY).Put(aZ).Put(aCount);
	for (size_t i = 0; i < aFlags.size(); ++i)
	{
		PutVertex(bytes, float(i), aFlags[i]);
	}
	return bytes.data;
}

constexpr uint32_t kOpaqueUp = 1 | (2 << 4);    // cut-out, facing up (Direction.UP + 1)
constexpr uint32_t kWater = 2 | (2 << 4);       // translucent
}

TEST(MessagesSectionAsWorldExporterWritesIt)
{
	const auto  payload = Section(-8, 0, 125, 6, std::vector<uint32_t>(6, kOpaqueUp));
	std::string error;
	const auto  mesh = ParseSection(payload, false, error);
	CHECK(mesh.has_value());
	CHECK_EQ(mesh->x, -8);
	CHECK_EQ(mesh->y, 0);
	CHECK_EQ(mesh->z, 125);
	CHECK_EQ(mesh->vertices.size(), size_t(6));
	const Vertex& v = mesh->vertices[1];
	CHECK_EQ(v.x, 1.0f);
	CHECK_EQ(v.y, 2.0f);
	CHECK_EQ(v.u, 0.25f);
	CHECK_EQ(v.v, 0.5f);
	CHECK_EQ(int(v.r), 200);
	CHECK_EQ(int(v.a), 255);
	CHECK_EQ(v.light, uint32_t(15 << 8));
	CHECK_EQ(v.flags, kOpaqueUp);
	CHECK_EQ(mesh->translucentDropped, 0u);
}

TEST(MessagesSectionKeepsWholeTriangles)
{
	const auto  payload = Section(0, 0, 0, 7, std::vector<uint32_t>(7, kOpaqueUp));
	std::string error;
	const auto  mesh = ParseSection(payload, false, error);
	CHECK(mesh.has_value());
	CHECK_EQ(mesh->vertices.size(), size_t(6));
}

TEST(MessagesTranslucentTrianglesAreDropped)
{
	const auto  payload = Section(0, 0, 0, 6, { kOpaqueUp, kOpaqueUp, kOpaqueUp, kWater, kWater, kWater });
	std::string error;
	const auto  dropped = ParseSection(payload, false, error);
	CHECK(dropped.has_value());
	CHECK_EQ(dropped->vertices.size(), size_t(3));
	CHECK_EQ(dropped->translucentDropped, 1u);
	const auto kept = ParseSection(payload, true, error);
	CHECK_EQ(kept->vertices.size(), size_t(6));
	CHECK_EQ(kept->translucentDropped, 0u);
}

TEST(MessagesEmptySectionIsARemoval)
{
	const auto  payload = Section(3, -4, 5, 0, {});
	std::string error;
	const auto  mesh = ParseSection(payload, false, error);
	CHECK(mesh.has_value());
	CHECK(mesh->vertices.empty());
}

TEST(MessagesSectionRejections)
{
	std::string error;
	CHECK(!ParseSection(std::vector<uint8_t>(15), false, error).has_value());
	CHECK(!ParseSection(Section(0, 0, 0, 10, std::vector<uint32_t>(9, kOpaqueUp)), false, error).has_value());
	CHECK(error.find("claims 10 vertices") != std::string::npos);
	CHECK(!ParseSection(Section(3'000'000, 0, 0, 0, {}), false, error).has_value());
	CHECK(!ParseSection(Section(0, 300, 0, 0, {}), false, error).has_value());
	CHECK(error.find("out of range") != std::string::npos);
}

TEST(MessagesAtlas)
{
	Bytes bytes;
	bytes.Put(uint32_t(2)).Put(uint32_t(1));
	for (uint8_t i = 0; i < 8; ++i)
	{
		bytes.Put(uint8_t(i + 1));
	}
	std::string error;
	const auto  atlas = ParseAtlas(bytes.data, error);
	CHECK(atlas.has_value());
	CHECK_EQ(atlas->width, 2u);
	CHECK_EQ(atlas->height, 1u);
	CHECK_EQ(atlas->rgba.size(), size_t(8));
	CHECK_EQ(int(atlas->rgba[0]), 1);
	CHECK_EQ(int(atlas->rgba[7]), 8);
}

TEST(MessagesAtlasRejections)
{
	std::string error;
	Bytes       zero;
	zero.Put(uint32_t(0)).Put(uint32_t(4));
	CHECK(!ParseAtlas(zero.data, error).has_value());
	Bytes huge;
	huge.Put(uint32_t(20000)).Put(uint32_t(1));
	CHECK(!ParseAtlas(huge.data, error).has_value());
	Bytes shortOne;
	shortOne.Put(uint32_t(2)).Put(uint32_t(2)).Put(uint32_t(0));
	CHECK(!ParseAtlas(shortOne.data, error).has_value());
	CHECK(error.find("needs 16 bytes") != std::string::npos);
}

namespace
{
// AvatarExporter.textureId's header, {id, width, height, 0}, then aPixels bytes.
std::vector<uint8_t> Texture(uint32_t aId, uint32_t aWidth, uint32_t aHeight, size_t aPixels)
{
	Bytes bytes;
	bytes.Put(aId).Put(aWidth).Put(aHeight).Put(uint32_t(0));
	for (size_t i = 0; i < aPixels; ++i)
	{
		bytes.Put(uint8_t(i + 1));
	}
	return bytes.data;
}

// AvatarExporter.send's scene: the origin as three doubles, the counts, the batches {texture, first, count, flags},
// then aVertices vertices, the i-th at x = i with block light i.
std::vector<uint8_t> Scene(double aX, uint32_t aVertices, const std::vector<std::array<uint32_t, 4>>& aBatches, uint32_t aClaimed)
{
	Bytes bytes;
	bytes.Put(aX).Put(6.0).Put(2007.0).Put(uint32_t(aBatches.size())).Put(aClaimed);
	for (const auto& batch : aBatches)
	{
		bytes.Put(batch[0]).Put(batch[1]).Put(batch[2]).Put(batch[3]);
	}
	for (uint32_t i = 0; i < aVertices; ++i)
	{
		bytes.Put(float(i)).Put(2.0f).Put(3.0f).Put(0.25f).Put(0.5f);
		bytes.Put(uint8_t(255)).Put(uint8_t(255)).Put(uint8_t(255)).Put(uint8_t(255));
		bytes.Put(uint32_t(i | (15 << 8))).Put(uint32_t(1 | 8 | (7 << 4)));
	}
	return bytes.data;
}
}

TEST(MessagesTextureAsAvatarExporterWritesIt)
{
	const auto  payload = Texture(3, 2, 1, 8);
	std::string error;
	const auto  texture = ParseTexture(payload, error);
	CHECK(texture.has_value());
	if (!texture)
	{
		return;
	}
	CHECK_EQ(texture->id, 3u);
	CHECK_EQ(texture->width, 2u);
	CHECK_EQ(texture->height, 1u);
	CHECK_EQ(texture->rgba.size(), size_t(8));
	CHECK_EQ(int(texture->rgba[0]), 1);
	CHECK_EQ(int(texture->rgba[7]), 8);
}

TEST(MessagesTextureRejections)
{
	std::string error;
	CHECK(!ParseTexture(std::vector<uint8_t>(15), error).has_value());
	CHECK(error.find("no room for its header") != std::string::npos);
	CHECK(!ParseTexture(Texture(0, 2, 1, 8), error).has_value());
	CHECK(error.find("id 0") != std::string::npos);
	CHECK(!ParseTexture(Texture(3, 0, 1, 0), error).has_value());
	CHECK(error.find("out of range") != std::string::npos);
	CHECK(!ParseTexture(Texture(3, 4097, 1, 4097 * 4), error).has_value());
	CHECK(error.find("out of range") != std::string::npos);
	CHECK(!ParseTexture(Texture(3, 2, 2, 12), error).has_value());
	CHECK(error.find("needs 16 bytes, and has 12") != std::string::npos);
}

TEST(MessagesSceneAsAvatarExporterWritesIt)
{
	const auto  payload = Scene(-120.0, 9, { { 0, 0, 3, 0 }, { 5, 3, 6, 1 } }, 9);
	std::string error;
	const auto  scene = ParseScene(payload, error);
	CHECK(scene.has_value());
	if (!scene)
	{
		return;
	}
	CHECK_EQ(scene->origin.x, -120.0);
	CHECK_EQ(scene->origin.y, 6.0);
	CHECK_EQ(scene->origin.z, 2007.0);
	CHECK_EQ(scene->batches.size(), size_t(2));
	if (scene->batches.size() != 2)
	{
		return;
	}
	CHECK_EQ(scene->batches[1].texture, 5u);
	CHECK_EQ(scene->batches[1].first, 3u);
	CHECK_EQ(scene->batches[1].count, 6u);
	CHECK_EQ(scene->batches[1].flags, 1u);
	CHECK_EQ(scene->vertexCount, 9u);
	const std::vector<Vertex> vertices = SceneVertices(*scene);
	CHECK_EQ(vertices.size(), size_t(9));
	if (vertices.size() != 9)
	{
		return;
	}
	CHECK_EQ(vertices[4].x, 4.0f);
	CHECK_EQ(vertices[4].light, uint32_t(4 | (15 << 8)));
	CHECK_EQ(vertices[4].flags, uint32_t(1 | 8 | (7 << 4)));
}

TEST(MessagesEmptySceneSaysNothingIsThere)
{
	std::string error;
	const auto  scene = ParseScene(Scene(0.0, 0, {}, 0), error);
	CHECK(scene.has_value());
	if (!scene)
	{
		return;
	}
	CHECK(scene->batches.empty());
	CHECK_EQ(scene->vertexCount, 0u);
	CHECK(SceneVertices(*scene).empty());
}

TEST(MessagesSceneRejections)
{
	std::string error;
	CHECK(!ParseScene(std::vector<uint8_t>(31), error).has_value());
	CHECK(error.find("no room for its header") != std::string::npos);
	CHECK(!ParseScene(Scene(std::nan(""), 3, { { 0, 0, 3, 0 } }, 3), error).has_value());
	CHECK(error.find("isn't finite") != std::string::npos);
	CHECK(!ParseScene(Scene(0.0, 0, std::vector<std::array<uint32_t, 4>>(65537, { 0, 0, 0, 0 }), 0), error).has_value());
	CHECK(error.find("65537 batches and 0 vertices is too big") != std::string::npos);
	CHECK(!ParseScene(Scene(0.0, 0, {}, 1048577), error).has_value());
	CHECK(error.find("0 batches and 1048577 vertices is too big") != std::string::npos);
	CHECK(!ParseScene(Scene(0.0, 3, { { 0, 0, 3, 0 } }, 4), error).has_value());
	CHECK(error.find("needs") != std::string::npos);
	CHECK(!ParseScene(Scene(0.0, 6, { { 0, 0, 4, 0 } }, 6), error).has_value());
	CHECK(error.find("batch 0") != std::string::npos);
	CHECK(!ParseScene(Scene(0.0, 6, { { 0, 0, 3, 0 }, { 0, 3, 6, 0 } }, 6), error).has_value());
	CHECK(error.find("batch 1") != std::string::npos);
	CHECK(!ParseScene(Scene(0.0, 6, { { 0, 0xFFFFFFFF, 3, 0 } }, 6), error).has_value());
	CHECK(error.find("batch 0") != std::string::npos);
}
