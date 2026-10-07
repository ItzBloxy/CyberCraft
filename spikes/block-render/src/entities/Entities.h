#pragma once

#include "Coords.h"
#include "link/Messages.h"
#include "link/WorldTable.h"
#include "world/Vertex.h"

#include <cstdint>
#include <map>
#include <optional>
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
		Translucent,  // the scene's blended batches, for step B's forward pass
		Crack,        // multiplied into the G-buffer's base colour
		Outline,      // step B's too: the targeted block's edges, a pair of vertices each, widened into strips
	};

	uint32_t first = 0;
	uint32_t count = 0;
	uint32_t texture = 0;  // 0: Minecraft's combined atlas; else a texture's id
	Kind     kind = Kind::Model;
};

// Whether step B's forward pass has any of aDraws to draw: translucent ones, or the outline while aOutline (set outline).
bool ForwardDraws(const std::vector<EntityDraw>& aDraws, bool aOutline);

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
	void Clear();                                  // Minecraft's clear: the scene goes
	void InWorld(bool aInWorld);                   // out of its world, or gone, the scene goes: SkyCraft stops sending

	// This frame's geometry. aEye, Minecraft's eye (McState), gives the origin when there's no scene: its whole block.
	// aOutline, while build mode is on, adds the outline of the block Minecraft's player targets: outside it, V's clicks
	// don't reach Minecraft, so an outline would mislead.
	EntityFrame Frame(const Vec3d& aEye, bool aOutline = false) const;

	// The newest scene: its batches and vertices, how many of those have block light of at least aGlowLight, and how many
	// are in translucent batches.
	struct SceneCounts
	{
		size_t   batches = 0;
		uint32_t vertices = 0;
		uint32_t glowing = 0;
		uint32_t translucent = 0;
	};
	SceneCounts                          Counts(uint32_t aGlowLight) const;
	const std::vector<link::SceneBatch>& Batches() const { return batches_; }
	const link::WorldTable&              Table() const { return world_; }
	uint64_t                             Scenes() const { return scenes_; }  // since the start, empty ones included
	uint64_t                             SceneBytes() const { return sceneBytes_; }
	// How far the nearest player's or mob's feet (the table's shadows) are from aFeet, Minecraft axes; nothing if none.
	std::optional<double> NearestMob(const Vec3d& aFeet) const;
	// The entities request's lines (the spec's "Requests and settings"): the newest scene, then each of its batches with
	// its texture (and its size, if aSizes has it, by id), vertices and those glowing, then the world entities by kind.
	std::vector<std::string> Lines(uint32_t aGlowLight, const std::map<uint32_t, std::pair<uint32_t, uint32_t>>& aSizes) const;

private:
	std::optional<Vec3d>          origin_;  // the newest scene's, while one is there
	std::vector<Vertex>           vertices_;
	std::vector<link::SceneBatch> batches_;
	link::WorldTable              world_;
	uint64_t                      scenes_ = 0;
	uint64_t                      sceneBytes_ = 0;
};
}
