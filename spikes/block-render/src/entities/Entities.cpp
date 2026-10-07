#include "entities/Entities.h"

#include "entities/WorldShapes.h"

#include <skycraft_protocol.h>

#include <algorithm>
#include <cmath>
#include <format>

namespace cpmc::entities
{
bool ForwardDraws(const std::vector<EntityDraw>& aDraws, bool aOutline)
{
	return std::any_of(aDraws.begin(), aDraws.end(), [aOutline](const EntityDraw& aDraw) {
		return aDraw.kind == EntityDraw::Kind::Translucent || (aOutline && aDraw.kind == EntityDraw::Kind::Outline);
	});
}

void Entities::Scene(const link::SceneMessage& aScene)
{
	++scenes_;
	sceneBytes_ += sizeof(skycraft::proto::RenScene) + aScene.batches.size() * sizeof(skycraft::proto::RenBatch) + aScene.vertices.size();
	if (aScene.batches.empty())
	{
		Clear();  // SkyCraft's one empty scene, once the last thing has gone
		return;
	}
	origin_ = aScene.origin;
	vertices_ = link::SceneVertices(aScene);
	batches_ = aScene.batches;
}

void Entities::World(const link::WorldTable& aTable)
{
	world_ = aTable;
}

void Entities::Clear()
{
	origin_.reset();
	vertices_.clear();
	batches_.clear();
}

void Entities::InWorld(bool aInWorld)
{
	if (!aInWorld)
	{
		Clear();
	}
}

EntityFrame Entities::Frame(const Vec3d& aEye, bool aOutline) const
{
	EntityFrame frame;
	frame.origin = origin_ ? *origin_ : Vec3d{ std::floor(aEye.x), std::floor(aEye.y), std::floor(aEye.z) };
	frame.vertices = vertices_;
	for (const link::SceneBatch& batch : batches_)
	{
		if (batch.count > 0)
		{
			frame.draws.push_back({ batch.first, batch.count, batch.texture, (batch.flags & 1) ? EntityDraw::Kind::Translucent : EntityDraw::Kind::Model });
		}
	}
	// The world entities: their shapes in one draw and their cracks in another, all in Minecraft's combined atlas.
	const uint32_t shapes = uint32_t(frame.vertices.size());
	for (const auto& record : world_.entities)
	{
		AppendShape(record, frame.origin, frame.vertices);
	}
	if (const uint32_t count = uint32_t(frame.vertices.size()) - shapes; count > 0)
	{
		frame.draws.push_back({ shapes, count, 0, EntityDraw::Kind::Model });
	}
	const uint32_t cracks = uint32_t(frame.vertices.size());
	for (const auto& record : world_.entities)
	{
		AppendCrack(record, frame.origin, frame.vertices);
	}
	if (const uint32_t count = uint32_t(frame.vertices.size()) - cracks; count > 0)
	{
		frame.draws.push_back({ cracks, count, 0, EntityDraw::Kind::Crack });
	}
	if (aOutline && world_.outline)
	{
		const uint32_t outline = uint32_t(frame.vertices.size());
		AppendOutline(*world_.outline, frame.origin, frame.vertices);
		frame.draws.push_back({ outline, uint32_t(frame.vertices.size()) - outline, 0, EntityDraw::Kind::Outline });
	}
	return frame;
}

Entities::SceneCounts Entities::Counts(uint32_t aGlowLight) const
{
	SceneCounts counts;
	counts.batches = batches_.size();
	counts.vertices = uint32_t(vertices_.size());
	counts.glowing = uint32_t(std::count_if(vertices_.begin(), vertices_.end(), [aGlowLight](const Vertex& aVertex) {
		return (aVertex.light & 0xFF) >= aGlowLight;
	}));
	for (const link::SceneBatch& batch : batches_)
	{
		counts.translucent += (batch.flags & 1) ? batch.count : 0;
	}
	return counts;
}

std::optional<double> Entities::NearestMob(const Vec3d& aFeet) const
{
	std::optional<double> nearest;
	for (const auto& record : world_.entities)
	{
		if (record.kind == skycraft::proto::kWeShadow)
		{
			const double distance = std::hypot(record.x - aFeet.x, record.y - aFeet.y, record.z - aFeet.z);
			nearest = std::min(nearest.value_or(distance), distance);
		}
	}
	return nearest;
}

std::vector<std::string> Entities::Lines(uint32_t aGlowLight, const std::map<uint32_t, std::pair<uint32_t, uint32_t>>& aSizes) const
{
	std::vector<std::string> lines;
	const SceneCounts        counts = Counts(aGlowLight);
	lines.push_back(origin_ ? std::format("the newest scene, from {:.0f} {:.0f} {:.0f}: {} batches, {} vertices ({} glowing, {} translucent)",
	                                      origin_->x, origin_->y, origin_->z, counts.batches, counts.vertices, counts.glowing, counts.translucent)
	                        : std::string("no scene: nothing of Minecraft's is there"));
	for (size_t b = 0; b < batches_.size(); ++b)
	{
		const link::SceneBatch& batch = batches_[b];
		const auto              size = aSizes.find(batch.texture);
		const std::string       texture = batch.texture == 0       ? std::string("the atlas")
		                                  : size != aSizes.end() ? std::format("texture {} ({} x {})", batch.texture, size->second.first,
		                                                                       size->second.second)
		                                                         : std::format("texture {}", batch.texture);
		uint32_t glowing = 0;
		for (uint64_t i = batch.first; i < uint64_t(batch.first) + batch.count && i < vertices_.size(); ++i)
		{
			glowing += (vertices_[i].light & 0xFF) >= aGlowLight ? 1 : 0;
		}
		lines.push_back(std::format("  batch {}: {}, {} vertices from {}{}, {} glowing", b, texture, batch.count, batch.first,
		                            (batch.flags & 1) ? ", translucent" : "", glowing));
	}
	lines.push_back("world: " + link::WorldWords(world_));
	return lines;
}
}
