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

// Part 5: an entity layer as CyberCraft Tweaks 1.6.0 marks it, aCount vertices with aFlags (translucent, no mipmaps and
// the triangle's own normal by default), block light aLight and alpha aAlpha, the i-th at x = aX + i / 6, so each quad
// (six vertices) lies one block on.
std::vector<Vertex> Layer(uint32_t aCount, uint32_t aFlags = 2 | 8 | (7 << 4), uint32_t aLight = 0, uint8_t aAlpha = 255, float aX = 0.0f)
{
	std::vector<Vertex> vertices(aCount);
	for (uint32_t i = 0; i < aCount; ++i)
	{
		vertices[i].x = aX + float(i / 6);
		vertices[i].flags = aFlags;
		vertices[i].light = aLight | (15u << 8);
		vertices[i].a = aAlpha;
	}
	return vertices;
}

// aTexels RGBA8 texels: aClear clear ones, aPartly half see-through, and the rest opaque.
std::vector<uint8_t> Texels(uint32_t aTexels, uint32_t aClear, uint32_t aPartly)
{
	std::vector<uint8_t> rgba(size_t(aTexels) * 4, 200);
	for (uint32_t i = 0; i < aTexels; ++i)
	{
		rgba[size_t(i) * 4 + 3] = i < aClear ? 0 : i < aClear + aPartly ? 128 : 255;
	}
	return rgba;
}
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

TEST(EntitiesAddTheOutlineInBuildMode)
{
	Entities         entities;
	link::WorldTable table;
	table.entities = { Record(proto::kWeCrack, 885.0f, 6.0f, 2010.0f) };
	table.outline = std::array<float, 6>{ 885.0f, 6.0f, 2010.0f, 886.0f, 7.0f, 2011.0f };
	entities.World(table);
	CHECK_EQ(entities.Frame(kEye).draws.size(), size_t(1));  // outside build mode, the crack alone
	const EntityFrame frame = entities.Frame(kEye, true);
	// The crack's draw, then the outline's, its 12 edges' ends after everything else.
	CHECK_EQ(frame.draws.size(), size_t(2));
	CHECK_EQ(frame.vertices.size(), size_t(36 + 24));
	if (frame.draws.size() != 2 || frame.vertices.size() != 60)
	{
		return;
	}
	CHECK_EQ(frame.draws[1].first, 36u);
	CHECK_EQ(frame.draws[1].count, 24u);
	CHECK_EQ(frame.draws[1].texture, 0u);
	CHECK(frame.draws[1].kind == EntityDraw::Kind::Outline);
	CHECK_NEAR(frame.vertices[36].x, 4.998f, 1e-4);  // from the whole block of Minecraft's eye
	table.outline.reset();
	entities.World(table);
	CHECK_EQ(entities.Frame(kEye, true).draws.size(), size_t(1));  // nothing targeted
}

TEST(EntitiesSayWhenTheForwardPassHasWork)
{
	using Kind = EntityDraw::Kind;
	CHECK(!ForwardDraws({ { 0, 3, 0, Kind::Model }, { 3, 36, 0, Kind::Crack } }, true));  // all into the G-buffer
	CHECK(ForwardDraws({ { 0, 3, 0, Kind::Model }, { 3, 6, 4, Kind::Translucent } }, false));
	CHECK(ForwardDraws({ { 0, 24, 0, Kind::Outline } }, true));
	CHECK(!ForwardDraws({ { 0, 24, 0, Kind::Outline } }, false));  // set outline 0
	CHECK(ForwardDraws({ { 0, 3, 0, Kind::Model }, { 3, 6, 4, Kind::Blended } }, false));  // Part 5's layers
	CHECK(ForwardDraws({ { 0, 3, 0, Kind::Model }, { 3, 6, 4, Kind::Additive } }, false));
}

TEST(EntitiesUnblendWithBlendOff)
{
	using Kind = EntityDraw::Kind;
	std::vector<EntityDraw> draws = { { 0, 6, 4, Kind::Blended }, { 6, 6, 5, Kind::Additive }, { 12, 6, 12, Kind::Translucent },
		                              { 18, 24, 0, Kind::Outline }, { 42, 36, 0, Kind::Crack } };
	UnblendDraws(draws);
	CHECK(draws[0].kind == Kind::Model);
	CHECK(draws[1].kind == Kind::Model);
	CHECK(draws[2].kind == Kind::Translucent);  // particles are as they were
	CHECK(draws[3].kind == Kind::Outline);
	CHECK(draws[4].kind == Kind::Crack);
	CHECK(!ForwardDraws({ draws[0], draws[1] }, false));  // nothing left for the forward pass
}

TEST(EntitiesTellASeeThroughTexture)
{
	CHECK(SeeThrough(Texels(343, 0, 46)));     // a vex's share: 46 of 343
	CHECK(!SeeThrough(Texels(1886, 0, 6)));    // a piglin's: 6 stray of 1,886, opaque all the same
	CHECK(SeeThrough(Texels(150, 50, 2)));     // 1 in 50 of the 100 not clear: the clear ones don't count
	CHECK(!SeeThrough(Texels(151, 50, 2)));    // just under
	CHECK(!SeeThrough(Texels(64, 64, 0)));     // all clear
	CHECK(!SeeThrough(Texels(64, 0, 0)));      // all opaque
	CHECK(!SeeThrough({}));
}

TEST(EntitiesKnowEachKindOfLayer)
{
	using Kind = EntityDraw::Kind;
	const link::SceneBatch translucent{ 4, 0, 6, 1 };
	CHECK(KindOf({ 4, 0, 6, 0 }, Layer(6), true, 15) == Kind::Model);                     // not translucent
	CHECK(KindOf(translucent, Layer(6, 2 | 8), false, 15) == Kind::Translucent);          // a particle: direction 0
	CHECK(KindOf(translucent, Layer(6, 2 | 8 | (7 << 4) | kFlagAdditive), false, 15) == Kind::Additive);  // energy_swirl
	CHECK(KindOf(translucent, Layer(6), false, 15) == Kind::Model);                       // marked, but opaque: a piglin
	CHECK(KindOf(translucent, Layer(6), true, 15) == Kind::Blended);                      // its texture see-through: a slime
	CHECK(KindOf(translucent, Layer(6, 2 | 8 | (7 << 4), 15), false, 15) == Kind::Blended);  // emissive: the warden's
	CHECK(KindOf(translucent, Layer(6, 2 | 8 | (7 << 4), 14), false, 15) == Kind::Model);
	CHECK(KindOf(translucent, Layer(6, 2 | 8 | (7 << 4), 14), false, 14) == Kind::Blended);  // set glowlight 14
	std::vector<Vertex> fading = Layer(6);
	fading[5].a = 64;  // a vertex faded: the warden's spots
	CHECK(KindOf(translucent, fading, false, 15) == Kind::Blended);
	CHECK_STR(KindName(Kind::Blended), "blended");
	CHECK_STR(KindName(Kind::Additive), "additive");
}

TEST(EntitiesNameEachBatchsKind)
{
	using Kind = EntityDraw::Kind;
	const link::SceneBatch translucent{ 4, 0, 6, 1 };
	CHECK_STR(KindWords({ 4, 0, 6, 0 }, Kind::Model), "");  // not translucent: nothing said
	CHECK_STR(KindWords(translucent, Kind::Translucent), ", translucent");
	CHECK_STR(KindWords(translucent, Kind::Blended), ", blended");
	CHECK_STR(KindWords(translucent, Kind::Additive), ", additive");
	CHECK_STR(KindWords(translucent, Kind::Model), ", translucent, drawn as a model");
}

TEST(EntitiesSortLayersBackToFront)
{
	// Three quads at x 0, 1 and 2, seen from x 5: the farthest, at 0, first. Then two triangles by their own centres.
	std::vector<Vertex> vertices = Layer(18);
	SortBackToFront(vertices, 0, 18, { 5.0, 0.0, 0.0 });
	CHECK_EQ(vertices[0].x, 0.0f);
	CHECK_EQ(vertices[6].x, 1.0f);
	CHECK_EQ(vertices[12].x, 2.0f);
	SortBackToFront(vertices, 0, 18, { -5.0, 0.0, 0.0 });
	CHECK_EQ(vertices[0].x, 2.0f);
	CHECK_EQ(vertices[5].x, 2.0f);
	CHECK_EQ(vertices[17].x, 0.0f);
	std::vector<Vertex> triangles = Layer(9);  // 9: not whole quads, so by triangle, at x 3, 0 and 1
	triangles[0].x = triangles[1].x = triangles[2].x = 3.0f;
	SortBackToFront(triangles, 0, 9, { -5.0, 0.0, 0.0 });
	CHECK_EQ(triangles[0].x, 3.0f);
	CHECK_EQ(triangles[3].x, 1.0f);
	CHECK_EQ(triangles[6].x, 0.0f);
	SortBackToFront(triangles, 6, 6, { 0.0, 0.0, 0.0 });  // past the end: left alone
	CHECK_EQ(triangles[6].x, 0.0f);
}

TEST(EntitiesDrawEachLayerByItsKind)
{
	// A particle, a slime's outer layer (its texture see-through), the creeper's armour, and a piglin: each six
	// vertices, the slime's quads at x 0 and 1 seen from Minecraft's eye east of them.
	std::vector<Vertex> vertices = Layer(6, 2 | 8);
	for (const auto& layer : { Layer(12), Layer(6, 2 | 8 | (7 << 4) | kFlagAdditive), Layer(6) })
	{
		vertices.insert(vertices.end(), layer.begin(), layer.end());
	}
	Entities entities;
	entities.Texture(7, Texels(655, 0, 385));  // slime.png's share
	entities.Texture(8, Texels(1886, 0, 6));   // piglin.png's
	entities.Scene(Scene(vertices, { { 12, 0, 6, 1 }, { 7, 6, 12, 1 }, { 9, 18, 6, 1 }, { 8, 24, 6, 1 } }, kOrigin));
	using Kind = EntityDraw::Kind;
	EntityFrame frame = entities.Frame({ 885.0, 7.0, 2007.0 });
	CHECK_EQ(frame.draws.size(), size_t(4));
	if (frame.draws.size() == 4)
	{
		CHECK(frame.draws[0].kind == Kind::Translucent);
		CHECK(frame.draws[1].kind == Kind::Blended);
		CHECK(frame.draws[2].kind == Kind::Additive);
		CHECK(frame.draws[3].kind == Kind::Model);
		CHECK_EQ(frame.vertices[6].x, 0.0f);  // the slime's far quad first, from the east
		CHECK_EQ(frame.vertices[12].x, 1.0f);
	}
	frame = entities.Frame({ 875.0, 7.0, 2007.0 });  // from the west: the other way round
	CHECK_EQ(frame.vertices[6].x, 1.0f);
	const Entities::SceneCounts counts = entities.Counts(15);
	CHECK_EQ(counts.blended, 1u);
	CHECK_EQ(counts.additive, 1u);
	const std::vector<std::string> lines = entities.Lines(15, {});
	CHECK_EQ(lines.size(), size_t(6));
	if (lines.size() == 6)
	{
		CHECK_STR(lines[1], "  batch 0: texture 12, 6 vertices from 0, translucent, 0 glowing");
		CHECK_STR(lines[2], "  batch 1: texture 7, 12 vertices from 6, blended, 0 glowing");
		CHECK_STR(lines[3], "  batch 2: texture 9, 6 vertices from 18, additive, 0 glowing");
		CHECK_STR(lines[4], "  batch 3: texture 8, 6 vertices from 24, translucent, drawn as a model, 0 glowing");
	}
	entities.Clear();  // the scene goes, the notes stay: SkyCraft won't send its textures again
	entities.Scene(Scene(vertices, { { 7, 6, 12, 1 } }, kOrigin));
	CHECK(entities.Frame(kEye).draws[0].kind == Kind::Blended);
	entities.ClearAll();  // Minecraft's clear: they go too
	entities.Scene(Scene(vertices, { { 7, 6, 12, 1 } }, kOrigin));
	CHECK(entities.Frame(kEye).draws[0].kind == Kind::Model);
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

TEST(EntitiesDescribeTheNewestScene)
{
	Entities entities;
	CHECK_STR(entities.Lines(15, {}).front(), "no scene: nothing of Minecraft's is there");
	std::vector<Vertex> vertices = Vertices(9);
	vertices[3].light = 15 | (15u << 8);
	entities.Scene(Scene(vertices, { { 0, 0, 3, 0 }, { 5, 3, 6, 1 } }, kOrigin));
	link::WorldTable table;
	table.entities = { Record(proto::kWeShadow, 885.5f, 6.12f, 2007.5f) };
	entities.World(table);
	const std::vector<std::string> lines = entities.Lines(15, { { 5, { 64, 64 } } });
	CHECK_EQ(lines.size(), size_t(4));
	if (lines.size() == 4)
	{
		CHECK_STR(lines[0], "the newest scene, from 880 7 2007: 2 batches, 9 vertices (1 glowing, 6 translucent)");
		CHECK_STR(lines[1], "  batch 0: the atlas, 3 vertices from 0, 0 glowing");
		CHECK_STR(lines[2], "  batch 1: texture 5 (64 x 64), 6 vertices from 3, translucent, 1 glowing");
		CHECK_STR(lines[3], "world: 1 shadow");
	}
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
