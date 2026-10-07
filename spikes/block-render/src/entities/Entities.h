#pragma once

#include "Coords.h"
#include "link/Messages.h"
#include "link/WorldTable.h"
#include "world/Vertex.h"

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

// Spike 7: one frame's geometry of Minecraft's entities (the spec's "Entities: one frame's geometry"), from the newest
// scene and world-entity table the link handed on. Main thread; it calls no GPU, so every rule is tested without the
// game. The renderer copies each frame into a snapshot and draws its draws.
namespace cpmc::entities
{
// One draw: a run of the frame's vertices with one texture.
struct EntityDraw
{
	enum class Kind : uint8_t
	{
		Model,        // into the G-buffer: the scene's opaque batches, and the world entities' shapes
		Translucent,  // the scene's blended particles, for step B's forward pass
		Crack,        // multiplied into the G-buffer's base colour
		Outline,      // step B's too: the targeted block's edges, a pair of vertices each, widened into strips
		Blended,      // Part 5, step B's too: an entity layer Minecraft blends, really see-through
		Additive,     // Part 5, step B's too: an entity layer Minecraft adds (energy_swirl)
	};

	uint32_t first = 0;
	uint32_t count = 0;
	uint32_t texture = 0;  // 0: Minecraft's combined atlas; else a texture's id
	Kind     kind = Kind::Model;
};

// A kind's name, for the entities request and mclink: model, translucent, crack, outline, blended or additive.
const char* KindName(EntityDraw::Kind aKind);

// Whether step B's forward pass has any of aDraws to draw: translucent, blended or additive ones, or the outline while
// aOutline (set outline).
bool ForwardDraws(const std::vector<EntityDraw>& aDraws, bool aOutline);

// Part 5: bit 7 of an entity vertex's flags, which CyberCraft Tweaks 1.6.0 sets on a layer Minecraft adds rather than
// blends. SkyCraft keys a batch's flags in 8 bits, and its own flags leave this one free.
constexpr uint32_t kFlagAdditive = 1u << 7;

// Part 5: whether a texture is see-through: at least 1 in 50 of its texels that aren't clear have an alpha between 0
// and 255. Vanilla's stray ones stay under it: a piglin's texture has 6 of 1,886 (0.32%), while a vex's has 13.4%.
bool SeeThrough(std::span<const uint8_t> aRgba);

// Part 5: the kind aBatch is drawn as, aVertices its own. Not translucent: a model. Translucent with no normal of its own
// (direction 0): a particle, translucent. Else an entity layer CyberCraft Tweaks marked: additive with kFlagAdditive;
// blended when it's really see-through, its first vertex with block light of at least aGlowLight (emissive), a vertex
// with an alpha under 255, or aSeeThrough, its texture's; else a model, a cut-out the game lights.
EntityDraw::Kind KindOf(const link::SceneBatch& aBatch, std::span<const Vertex> aVertices, bool aSeeThrough, uint32_t aGlowLight);

// Part 5: aCount of aVertices from aFirst, back to front, as Minecraft sorts its translucent layers: by the distance of
// their quads' centres from aEye (SkyCraft sends each quad as two triangles in a row), or their triangles' when aCount
// isn't a multiple of 6. Positions and aEye alike are relative to the frame's origin. Ties keep their order.
void SortBackToFront(std::vector<Vertex>& aVertices, uint32_t aFirst, uint32_t aCount, const Vec3d& aEye);

// How the entities request and mclink tell a batch drawn as aKind: nothing for one that isn't translucent; for one
// that is, ", translucent" (a particle), ", blended", ", additive", or ", translucent, drawn as a model".
std::string KindWords(const link::SceneBatch& aBatch, EntityDraw::Kind aKind);

struct EntityFrame
{
	Vec3d                   origin;    // a whole block, in Minecraft axes: every position is relative to it
	std::vector<Vertex>     vertices;  // the scene's as they came, then the world entities' shapes, their cracks, and the
	                                   // outline
	std::vector<EntityDraw> draws;     // the scene's batches in the order they came, then one each of shapes, cracks and
	                                   // the outline
};

class Entities
{
public:
	void Scene(const link::SceneMessage& aScene);  // the newest scene, copied; one of no batches says nothing is there
	void World(const link::WorldTable& aTable);    // the newest world-entity table, as the link hands it on each update
	void Clear();                                  // the scene goes
	void ClearAll();                               // Minecraft's clear: the scene, and what Texture noted, go
	void InWorld(bool aInWorld);                   // out of its world, or gone, the scene goes: SkyCraft stops sending
	// Part 5: an entity texture as it comes, whether it's see-through noted (SeeThrough) for its batches' kinds. SkyCraft
	// sends each once, and all of them again after its clear.
	void Texture(uint32_t aId, std::span<const uint8_t> aRgba);

	// This frame's geometry. aEye, Minecraft's eye (McState), gives the origin when there's no scene: its whole block.
	// aOutline, while build mode is on, adds the outline of the block Minecraft's player targets: outside it, V's clicks
	// don't reach Minecraft, so an outline would mislead. Each batch's kind is KindOf's at aGlowLight (set glowlight), and
	// the blended and additive ones are sorted back to front from aEye.
	EntityFrame Frame(const Vec3d& aEye, bool aOutline = false, uint32_t aGlowLight = 15) const;

	// The newest scene: its batches and vertices, how many of those have block light of at least aGlowLight, how many are
	// in translucent batches, and its batches drawn blended and additive (Part 5).
	struct SceneCounts
	{
		size_t   batches = 0;
		uint32_t vertices = 0;
		uint32_t glowing = 0;
		uint32_t translucent = 0;
		uint32_t blended = 0;
		uint32_t additive = 0;
	};
	SceneCounts                          Counts(uint32_t aGlowLight) const;
	const std::vector<link::SceneBatch>& Batches() const { return batches_; }
	const link::WorldTable&              Table() const { return world_; }
	uint64_t                             Scenes() const { return scenes_; }  // since the start, empty ones included
	uint64_t                             SceneBytes() const { return sceneBytes_; }
	// How far the nearest player's or mob's feet (the table's shadows) are from aFeet, Minecraft axes; nothing if none.
	std::optional<double> NearestMob(const Vec3d& aFeet) const;
	// The entities request's lines (the spec's "Requests and settings"): the newest scene, then each of its batches with
	// its texture (and its size, if aSizes has it, by id), vertices, its kind if translucent, and those glowing, then the
	// world entities by kind.
	std::vector<std::string> Lines(uint32_t aGlowLight, const std::map<uint32_t, std::pair<uint32_t, uint32_t>>& aSizes) const;

private:
	EntityDraw::Kind BatchKind(const link::SceneBatch& aBatch, uint32_t aGlowLight) const;  // KindOf, with its texture's note

	std::optional<Vec3d>          origin_;  // the newest scene's, while one is there
	std::vector<Vertex>           vertices_;
	std::vector<link::SceneBatch> batches_;
	link::WorldTable              world_;
	uint64_t                      scenes_ = 0;
	uint64_t                      sceneBytes_ = 0;
	std::map<uint32_t, bool>      seeThrough_;  // Part 5: each texture's, by id
};
}
