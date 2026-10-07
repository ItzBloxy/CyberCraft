#include "Harness.h"

#include "entities/Entities.h"
#include "entities/WorldShapes.h"

using namespace cpmc;
using namespace cpmc::entities;

namespace
{
namespace proto = skycraft::proto;

// aCount vertices, the i-th at x = i with block light aLight.
std::vector<Vertex> Vertices(uint32_t aCount, uint32_t aLight = 0)
{
	std::vector<Vertex> vertices(aCount);
	for (uint32_t i = 0; i < aCount; ++i)
	{
		vertices[i].x = float(i);
		vertices[i].light = aLight | (15u << 8);
	}
	return vertices;
}

// A scene from aOrigin over aVertices, which must outlive it, as ParseScene makes one.
link::SceneMessage Scene(const std::vector<Vertex>& aVertices, std::vector<link::SceneBatch> aBatches, const Vec3d& aOrigin)
{
	link::SceneMessage scene;
	scene.origin = aOrigin;
	scene.batches = std::move(aBatches);
	scene.vertexCount = uint32_t(aVertices.size());
	scene.vertices = { reinterpret_cast<const uint8_t*>(aVertices.data()), aVertices.size() * sizeof(Vertex) };
	return scene;
}

proto::WorldEntity Record(uint32_t aKind, float aX, float aY, float aZ)
{
	proto::WorldEntity record{};
	record.kind = aKind;
	record.x = aX;
	record.y = aY;
	record.z = aZ;
	record.scale = 0.5f;
	record.ext[0] = record.ext[1] = record.ext[2] = 1.008f;
	return record;
}

const Vec3d kOrigin{ 880.0, 7.0, 2007.0 };
const Vec3d kEye{ 880.5, 7.76, 2007.5 };
}

TEST(EntitiesDrawTheSceneInItsOrder)
{
	const std::vector<Vertex> vertices = Vertices(12);
	Entities                  entities;
	entities.Scene(Scene(vertices, { { 5, 0, 6, 0 }, { 12, 6, 3, 1 }, { 0, 9, 3, 0 } }, kOrigin));
	const EntityFrame frame = entities.Frame(kEye);
	CHECK_EQ(frame.origin.x, 880.0);
	CHECK_EQ(frame.origin.y, 7.0);
	CHECK_EQ(frame.vertices.size(), size_t(12));
	CHECK_EQ(frame.draws.size(), size_t(3));
	if (frame.draws.size() != 3 || frame.vertices.size() != 12)
	{
		return;
	}
	CHECK_EQ(frame.vertices[7].x, 7.0f);
	CHECK_EQ(frame.draws[0].texture, 5u);
	CHECK(frame.draws[0].kind == EntityDraw::Kind::Model);
	CHECK_EQ(frame.draws[1].first, 6u);
	CHECK_EQ(frame.draws[1].count, 3u);
	CHECK(frame.draws[1].kind == EntityDraw::Kind::Translucent);  // flagged translucent: step B's
	CHECK_EQ(frame.draws[2].texture, 0u);  // the atlas: a falling block, say
	CHECK(frame.draws[2].kind == EntityDraw::Kind::Model);
}

TEST(EntitiesAddTheWorldEntitiesAfterTheScene)
{
	const std::vector<Vertex> vertices = Vertices(3);
	Entities                  entities;
	entities.Scene(Scene(vertices, { { 4, 0, 3, 0 } }, kOrigin));
	link::WorldTable table;
	table.entities = { Record(proto::kWeItem, 883.5f, 7.75f, 2007.5f), Record(proto::kWeCrack, 885.0f, 6.0f, 2010.0f),
		               Record(proto::kWeShadow, 885.5f, 6.12f, 2007.5f), Record(proto::kWeArrow, 890.0f, 7.0f, 2007.0f) };
	entities.World(table);
	const EntityFrame frame = entities.Frame(kEye);
	// The scene's draw, then the item's and the arrow's shapes in one draw, then the crack's: the shadow isn't drawn.
	CHECK_EQ(frame.draws.size(), size_t(3));
	CHECK_EQ(frame.vertices.size(), size_t(3 + 6 + 18 + 36));
	if (frame.draws.size() != 3 || frame.vertices.size() != 63)
	{
		return;
	}
	CHECK_EQ(frame.draws[1].first, 3u);
	CHECK_EQ(frame.draws[1].count, 24u);
	CHECK_EQ(frame.draws[1].texture, 0u);
	CHECK(frame.draws[1].kind == EntityDraw::Kind::Model);
	CHECK_EQ(frame.draws[2].first, 27u);
	CHECK_EQ(frame.draws[2].count, 36u);
	CHECK(frame.draws[2].kind == EntityDraw::Kind::Crack);
	// Relative to the scene's origin: the item's centre 3.5 east and 0.75 up of it.
	CHECK_NEAR((frame.vertices[3].y + frame.vertices[5].y) / 2.0f, 0.75f, 1e-4);
	CHECK_EQ(frame.vertices[3].flags, kShapeFlags);
}

TEST(EntitiesWithoutASceneUseMinecraftsEye)
{
	Entities         entities;
	link::WorldTable table;
	table.entities = { Record(proto::kWeItem, 883.5f, 7.75f, 2007.5f) };
	entities.World(table);
	const EntityFrame frame = entities.Frame(kEye);
	CHECK_EQ(frame.origin.x, 880.0);  // the whole block of Minecraft's eye
	CHECK_EQ(frame.origin.y, 7.0);
	CHECK_EQ(frame.origin.z, 2007.0);
	CHECK_EQ(frame.draws.size(), size_t(1));
	CHECK_EQ(frame.vertices.size(), size_t(6));
}

TEST(EntitiesEmptySceneMeansNothingIsThere)
{
	const std::vector<Vertex> vertices = Vertices(3);
	Entities                  entities;
	entities.Scene(Scene(vertices, { { 4, 0, 3, 0 } }, kOrigin));
	entities.Scene(Scene({}, {}, { 0.0, 0.0, 0.0 }));  // SkyCraft's one empty scene, origin 0
	const EntityFrame frame = entities.Frame({ 100.2, 64.5, -3.5 });
	CHECK(frame.draws.empty());
	CHECK(frame.vertices.empty());
	CHECK_EQ(frame.origin.x, 100.0);  // from the eye, not the empty scene's 0
	CHECK_EQ(frame.origin.z, -4.0);
	CHECK_EQ(entities.Scenes(), 2ull);
}

TEST(EntitiesDropTheSceneAtAClearAndOutOfTheWorld)
{
	const std::vector<Vertex> vertices = Vertices(3);
	Entities                  entities;
	entities.Scene(Scene(vertices, { { 4, 0, 3, 0 } }, kOrigin));
	entities.InWorld(true);
	CHECK_EQ(entities.Frame(kEye).draws.size(), size_t(1));
	entities.Clear();
	CHECK(entities.Frame(kEye).draws.empty());
	entities.Scene(Scene(vertices, { { 4, 0, 3, 0 } }, kOrigin));
	entities.InWorld(false);
	CHECK(entities.Frame(kEye).draws.empty());
}

TEST(EntitiesCountTheNewestScene)
{
	std::vector<Vertex> vertices = Vertices(9, 0);
	vertices[0].light = 15 | (15u << 8);  // block light 15: a blaze, say
	vertices[1].light = 15 | (15u << 8);
	vertices[2].light = 14 | (15u << 8);  // beside a torch
	Entities entities;
	entities.Scene(Scene(vertices, { { 11, 0, 3, 0 }, { 12, 3, 6, 1 } }, kOrigin));
	const Entities::SceneCounts at15 = entities.Counts(15);
	CHECK_EQ(at15.batches, size_t(2));
	CHECK_EQ(at15.vertices, 9u);
	CHECK_EQ(at15.glowing, 2u);
	CHECK_EQ(at15.translucent, 6u);
	CHECK_EQ(entities.Counts(14).glowing, 3u);
	CHECK_EQ(entities.SceneBytes(), uint64_t(32 + 2 * 16 + 9 * 32));
}

TEST(EntitiesFindTheNearestMob)
{
	Entities entities;
	CHECK(!entities.NearestMob(kEye).has_value());
	link::WorldTable table;
	table.entities = { Record(proto::kWeShadow, 885.5f, 7.76f, 2007.5f), Record(proto::kWeItem, 881.5f, 7.76f, 2007.5f),
		               Record(proto::kWeShadow, 880.5f, 7.76f, 2010.5f) };
	entities.World(table);
	const std::optional<double> nearest = entities.NearestMob(kEye);
	CHECK(nearest.has_value());
	CHECK_NEAR(nearest.value_or(0.0), 3.0, 1e-4);  // the second shadow's, 3 south: the item doesn't count
}
