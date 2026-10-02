#include "Harness.h"

#include "link/Messages.h"

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
