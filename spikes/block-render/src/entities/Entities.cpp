#include "entities/Entities.h"

#include "entities/WorldShapes.h"

#include <skycraft_protocol.h>

#include <algorithm>
#include <cmath>
#include <format>

namespace cpmc::entities
{
const char* KindName(EntityDraw::Kind aKind)
{
	switch (aKind)
	{
	case EntityDraw::Kind::Model:
		return "model";
	case EntityDraw::Kind::Translucent:
		return "translucent";
	case EntityDraw::Kind::Crack:
		return "crack";
	case EntityDraw::Kind::Outline:
		return "outline";
	case EntityDraw::Kind::Blended:
		return "blended";
	case EntityDraw::Kind::Additive:
		return "additive";
	}
	return "?";
}

bool ForwardDraws(const std::vector<EntityDraw>& aDraws, bool aOutline)
{
	return std::any_of(aDraws.begin(), aDraws.end(), [aOutline](const EntityDraw& aDraw) {
		return aDraw.kind == EntityDraw::Kind::Translucent || aDraw.kind == EntityDraw::Kind::Blended || aDraw.kind == EntityDraw::Kind::Additive ||
		       (aOutline && aDraw.kind == EntityDraw::Kind::Outline);
	});
}

bool SeeThrough(std::span<const uint8_t> aRgba)
{
	uint64_t kept = 0;    // texels that aren't clear
	uint64_t partly = 0;  // of those, the ones not opaque either
	for (size_t i = 3; i < aRgba.size(); i += 4)
	{
		kept += aRgba[i] != 0 ? 1 : 0;
		partly += aRgba[i] != 0 && aRgba[i] != 255 ? 1 : 0;
	}
	return partly > 0 && partly * 50 >= kept;
}

EntityDraw::Kind KindOf(const link::SceneBatch& aBatch, std::span<const Vertex> aVertices, bool aSeeThrough, uint32_t aGlowLight)
{
	if ((aBatch.flags & 1) == 0)
	{
		return EntityDraw::Kind::Model;
	}
	if (aVertices.empty() || ((aVertices.front().flags >> 4) & 7) != 7)
	{
		return EntityDraw::Kind::Translucent;  // a particle: no normal of its own
	}
	if (aVertices.front().flags & kFlagAdditive)
	{
		return EntityDraw::Kind::Additive;
	}
	const bool emissive = (aVertices.front().light & 0xFF) >= aGlowLight;
	const bool faded = std::any_of(aVertices.begin(), aVertices.end(), [](const Vertex& aVertex) { return aVertex.a < 255; });
	return emissive || faded || aSeeThrough ? EntityDraw::Kind::Blended : EntityDraw::Kind::Model;
}

void SortBackToFront(std::vector<Vertex>& aVertices, uint32_t aFirst, uint32_t aCount, const Vec3d& aEye)
{
	if (uint64_t(aFirst) + aCount > aVertices.size())
	{
		return;
	}
	const uint32_t                            group = aCount % 6 == 0 ? 6 : 3;
	const uint32_t                            groups = aCount / group;
	std::vector<std::pair<double, uint32_t>> order(groups);  // each quad's or triangle's distance squared, and which it is
	for (uint32_t g = 0; g < groups; ++g)
	{
		Vec3d centre;
		for (uint32_t i = 0; i < group; ++i)
		{
			const Vertex& vertex = aVertices[size_t(aFirst) + size_t(g) * group + i];
			centre.x += vertex.x / double(group);
			centre.y += vertex.y / double(group);
			centre.z += vertex.z / double(group);
		}
		const double dx = centre.x - aEye.x;
		const double dy = centre.y - aEye.y;
		const double dz = centre.z - aEye.z;
		order[g] = { dx * dx + dy * dy + dz * dz, g };
	}
	std::stable_sort(order.begin(), order.end(), [](const auto& aA, const auto& aB) { return aA.first > aB.first; });
	std::vector<Vertex> sorted;
	sorted.reserve(size_t(groups) * group);
	for (const auto& [distance, g] : order)
	{
		const auto start = aVertices.begin() + aFirst + ptrdiff_t(g) * group;
		sorted.insert(sorted.end(), start, start + group);
	}
	std::copy(sorted.begin(), sorted.end(), aVertices.begin() + aFirst);
}

std::string KindWords(const link::SceneBatch& aBatch, EntityDraw::Kind aKind)
{
	if ((aBatch.flags & 1) == 0)
	{
		return {};
	}
	return aKind == EntityDraw::Kind::Model ? std::string(", translucent, drawn as a model") : std::format(", {}", KindName(aKind));
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

void Entities::ClearAll()
{
	Clear();
	seeThrough_.clear();
}

void Entities::Texture(uint32_t aId, std::span<const uint8_t> aRgba)
{
	seeThrough_[aId] = SeeThrough(aRgba);
}

EntityDraw::Kind Entities::BatchKind(const link::SceneBatch& aBatch, uint32_t aGlowLight) const
{
	const auto note = seeThrough_.find(aBatch.texture);
	return KindOf(aBatch, std::span<const Vertex>(vertices_).subspan(aBatch.first, aBatch.count), note != seeThrough_.end() && note->second,
	              aGlowLight);
}

void Entities::InWorld(bool aInWorld)
{
	if (!aInWorld)
	{
		Clear();
	}
}

EntityFrame Entities::Frame(const Vec3d& aEye, bool aOutline, uint32_t aGlowLight) const
{
	EntityFrame frame;
	frame.origin = origin_ ? *origin_ : Vec3d{ std::floor(aEye.x), std::floor(aEye.y), std::floor(aEye.z) };
	frame.vertices = vertices_;
	const Vec3d eye{ aEye.x - frame.origin.x, aEye.y - frame.origin.y, aEye.z - frame.origin.z };
	for (const link::SceneBatch& batch : batches_)
	{
		if (batch.count > 0)
		{
			const EntityDraw::Kind kind = BatchKind(batch, aGlowLight);
			if (kind == EntityDraw::Kind::Blended || kind == EntityDraw::Kind::Additive)
			{
				SortBackToFront(frame.vertices, batch.first, batch.count, eye);
			}
			frame.draws.push_back({ batch.first, batch.count, batch.texture, kind });
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
		const EntityDraw::Kind kind = BatchKind(batch, aGlowLight);
		counts.blended += kind == EntityDraw::Kind::Blended ? 1 : 0;
		counts.additive += kind == EntityDraw::Kind::Additive ? 1 : 0;
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
		                            KindWords(batch, BatchKind(batch, aGlowLight)), glowing));
	}
	lines.push_back("world: " + link::WorldWords(world_));
	return lines;
}
}
