#include "Harness.h"

#include "entities/WorldShapes.h"

#include <cmath>

using namespace cpmc;
using namespace cpmc::entities;

namespace
{
const Vec3d kOrigin{ 880.0, 6.0, 2007.0 };

// A record of aKind at (aX, aY, aZ) from kOrigin, its atlas rectangles {0.1, 0.2, 0.3, 0.4} and on.
proto::WorldEntity Record(uint32_t aKind, float aX, float aY, float aZ)
{
	proto::WorldEntity record{};
	record.kind = aKind;
	record.x = float(kOrigin.x) + aX;
	record.y = float(kOrigin.y) + aY;
	record.z = float(kOrigin.z) + aZ;
	for (int r = 0; r < 3; ++r)
	{
		for (int k = 0; k < 4; ++k)
		{
			record.uv[r][k] = 0.1f * float(r * 4 + k + 1);
		}
	}
	return record;
}

void CheckAt(const Vertex& aVertex, float aX, float aY, float aZ)
{
	CHECK_NEAR(aVertex.x, aX, 1e-4);
	CHECK_NEAR(aVertex.y, aY, 1e-4);
	CHECK_NEAR(aVertex.z, aZ, 1e-4);
}
}

TEST(WorldShapesItemTurnsAboutTheVertical)
{
	proto::WorldEntity item = Record(proto::kWeItem, 3.5f, 0.75f, 0.5f);
	item.yaw = 90.0f;
	item.scale = 0.5f;
	std::vector<Vertex> vertices;
	AppendShape(item, kOrigin, vertices);
	CHECK_EQ(vertices.size(), size_t(6));
	if (vertices.size() != 6)
	{
		return;
	}
	// At yaw 90 the icon faces east: it spans z, 0.5 wide and tall, centred on the record.
	CheckAt(vertices[0], 3.5f, 1.0f, 0.25f);  // top left
	CheckAt(vertices[1], 3.5f, 1.0f, 0.75f);  // top right
	CheckAt(vertices[2], 3.5f, 0.5f, 0.75f);  // bottom right
	CheckAt(vertices[5], 3.5f, 0.5f, 0.25f);  // bottom left
	CHECK_NEAR(vertices[0].u, 0.1f, 1e-6);
	CHECK_NEAR(vertices[0].v, 0.2f, 1e-6);
	CHECK_NEAR(vertices[2].u, 0.3f, 1e-6);
	CHECK_NEAR(vertices[2].v, 0.4f, 1e-6);
	for (const Vertex& vertex : vertices)
	{
		CHECK_EQ(vertex.flags, kShapeFlags);
		CHECK_EQ(vertex.light, kShapeLight);
		CHECK_EQ(int(vertex.r) + vertex.g + vertex.b + vertex.a, 4 * 255);
	}
}

TEST(WorldShapesBlockIsACubeWithItsTopTinted)
{
	proto::WorldEntity block = Record(proto::kWeBlock, 1.0f, 2.0f, 3.0f);
	block.scale = 0.25f;
	block.tint = 0xFF3C9B5Bu;  // red 0x5B, green 0x9B, blue 0x3C: a grass top
	std::vector<Vertex> vertices;
	AppendShape(block, kOrigin, vertices);
	CHECK_EQ(vertices.size(), size_t(36));
	if (vertices.size() != 36)
	{
		return;
	}
	// The top, the fifth face: corners (-x, -z), (+x, -z), (+x, +z), (-x, +z) at the top, 0.125 above the centre.
	CheckAt(vertices[24], 0.875f, 2.125f, 2.875f);
	CheckAt(vertices[25], 1.125f, 2.125f, 2.875f);
	CheckAt(vertices[26], 1.125f, 2.125f, 3.125f);
	CheckAt(vertices[29], 0.875f, 2.125f, 3.125f);
	CHECK_NEAR(vertices[24].u, 0.5f, 1e-6);  // the top's rectangle, uv[1]
	CHECK_EQ(int(vertices[24].r), 0x5B);
	CHECK_EQ(int(vertices[24].g), 0x9B);
	CHECK_EQ(int(vertices[24].b), 0x3C);
	CHECK_EQ(int(vertices[0].g), 255);  // the sides are white
	CHECK_NEAR(vertices[0].u, 0.1f, 1e-6);  // with uv[0]
	CheckAt(vertices[30], 0.875f, 1.875f, 3.125f);  // the bottom, with uv[2]
	CHECK_NEAR(vertices[30].u, 0.9f, 1e-6);
	CHECK_EQ(int(vertices[30].g), 255);
}

TEST(WorldShapesArrowFliesAlongItsYawAndPitch)
{
	proto::WorldEntity arrow = Record(proto::kWeArrow, 0.0f, 0.0f, 0.0f);
	arrow.yaw = 180.0f;  // north
	arrow.pitch = 30.0f;  // and up
	std::vector<Vertex> vertices;
	AppendShape(arrow, kOrigin, vertices);
	CHECK_EQ(vertices.size(), size_t(18));  // two fins and the back plate
	if (vertices.size() != 18)
	{
		return;
	}
	const float k = kArrowUnit;
	const float dy = 0.5f;
	const float dz = -std::sqrt(3.0f) / 2.0f;
	// The first fin: its head end 4 units ahead along the flight, its fletching 12 behind, 4 wide.
	CheckAt({ (vertices[1].x + vertices[2].x) / 2, (vertices[1].y + vertices[2].y) / 2, (vertices[1].z + vertices[2].z) / 2 }, 0.0f,
	        4 * k * dy, 4 * k * dz);
	CheckAt({ (vertices[0].x + vertices[5].x) / 2, (vertices[0].y + vertices[5].y) / 2, (vertices[0].z + vertices[5].z) / 2 }, 0.0f,
	        -12 * k * dy, -12 * k * dz);
	CHECK_NEAR(std::hypot(vertices[2].x - vertices[1].x, vertices[2].y - vertices[1].y, vertices[2].z - vertices[1].z), 4 * k, 1e-5);
	CHECK_NEAR(vertices[0].u, 0.1f, 1e-6);  // the fins, the side view
	// The back plate, 11 units behind, its rectangle uv[1].
	CheckAt({ (vertices[12].x + vertices[13].x + vertices[14].x + vertices[17].x) / 4,
	          (vertices[12].y + vertices[13].y + vertices[14].y + vertices[17].y) / 4,
	          (vertices[12].z + vertices[13].z + vertices[14].z + vertices[17].z) / 4 },
	        0.0f, -11 * k * dy, -11 * k * dz);
	CHECK_NEAR(vertices[12].u, 0.5f, 1e-6);
}

TEST(WorldShapesTridentLiesAlongItsFlight)
{
	proto::WorldEntity trident = Record(proto::kWeTrident, 0.0f, 0.0f, 0.0f);
	trident.yaw = 90.0f;  // east, level
	std::vector<Vertex> vertices;
	AppendShape(trident, kOrigin, vertices);
	CHECK_EQ(vertices.size(), size_t(12));  // its icon on two crossed quads
	if (vertices.size() != 12)
	{
		return;
	}
	CheckAt(vertices[1], kTridentHalf, 0.0f, 0.0f);   // the tip, its icon's top right, ahead
	CheckAt(vertices[5], -kTridentHalf, 0.0f, 0.0f);  // the handle, its bottom left, behind
	CHECK_NEAR(vertices[1].u, 0.3f, 1e-6);
}

TEST(WorldShapesCrackCoversItsBox)
{
	proto::WorldEntity crack = Record(proto::kWeCrack, 5.0f, 1.0f, 7.0f);
	crack.ext[0] = 1.008f;
	crack.ext[1] = 0.508f;
	crack.ext[2] = 1.008f;
	std::vector<Vertex> vertices;
	AppendShape(crack, kOrigin, vertices);
	CHECK(vertices.empty());  // a crack isn't a shape
	AppendCrack(crack, kOrigin, vertices);
	CHECK_EQ(vertices.size(), size_t(36));
	if (vertices.size() != 36)
	{
		return;
	}
	CheckAt(vertices[24], 5.0f, 1.508f, 7.0f);  // the top, from the box's low corner
	CheckAt(vertices[26], 6.008f, 1.508f, 8.008f);
	for (const Vertex& vertex : vertices)
	{
		CHECK(vertex.u == crack.uv[0][0] || vertex.u == crack.uv[0][2]);  // every face the stage's, uv[0]
	}
}

TEST(WorldShapesShadowsAndOthersAddNothing)
{
	std::vector<Vertex> vertices;
	AppendShape(Record(proto::kWeShadow, 0.0f, 0.0f, 0.0f), kOrigin, vertices);
	AppendShape(Record(9, 0.0f, 0.0f, 0.0f), kOrigin, vertices);
	AppendCrack(Record(proto::kWeItem, 0.0f, 0.0f, 0.0f), kOrigin, vertices);
	CHECK(vertices.empty());
}
