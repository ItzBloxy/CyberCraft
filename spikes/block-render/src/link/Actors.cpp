#include "link/Actors.h"

#include "link/Look.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <iterator>
#include <utility>

namespace cpmc::link
{
uint32_t ActorIds::Id(uint64_t aEntity, double aNow)
{
	const auto found = byEntity_.find(aEntity);
	if (found != byEntity_.end())
	{
		found->second.listed = aNow;
		return found->second.id;
	}
	const uint32_t id = next_++;
	byEntity_.emplace(aEntity, Entry{ id, aNow });
	byId_.emplace(id, aEntity);
	return id;
}

std::optional<uint64_t> ActorIds::Entity(uint32_t aId, double aNow) const
{
	const auto entity = byId_.find(aId);
	if (entity == byId_.end())
	{
		return std::nullopt;
	}
	const auto entry = byEntity_.find(entity->second);
	if (entry == byEntity_.end() || aNow - entry->second.listed > kHitTrail)
	{
		return std::nullopt;
	}
	return entity->second;
}

void ActorIds::Forget(double aNow)
{
	for (auto entry = byEntity_.begin(); entry != byEntity_.end();)
	{
		if (aNow - entry->second.listed >= kActorForget)
		{
			byId_.erase(entry->second.id);
			entry = byEntity_.erase(entry);
		}
		else
		{
			++entry;
		}
	}
}

std::optional<uint32_t> ActorIds::IdOf(uint64_t aEntity) const
{
	const auto found = byEntity_.find(aEntity);
	return found != byEntity_.end() ? std::optional<uint32_t>(found->second.id) : std::nullopt;
}

std::string CutName(std::string_view aName, size_t aBytes)
{
	if (aName.size() <= aBytes)
	{
		return std::string(aName);
	}
	size_t cut = aBytes;
	while (cut > 0 && (uint8_t(aName[cut]) & 0xC0) == 0x80)
	{
		--cut;  // aName[cut] continues a character that starts before it: that character is left out whole
	}
	return std::string(aName.substr(0, cut));
}

proto::ActorRecord MakeActorRecord(const NpcSample& aNpc, uint32_t aId, const NpcBox& aBox)
{
	proto::ActorRecord record{};
	record.formId = aId;
	record.flags = aNpc.flags & (proto::kActorHostile | proto::kActorEssential | proto::kActorInCombat);
	const Vec3d feet = McFromWorld({ aNpc.position.x, aNpc.position.y, aNpc.position.z - aBox.below });
	record.x = float(feet.x);
	record.y = float(feet.y);
	record.z = float(feet.z);
	const std::optional<McLook> look = LookFromForward(aNpc.forward, 0.0f);
	record.yaw = look ? look->yaw : 0.0f;
	record.width = float(aBox.width);
	record.height = float(aBox.below + aBox.above);
	record.healthFrac = aNpc.healthMax > 0.0f && std::isfinite(aNpc.health) && std::isfinite(aNpc.healthMax)
	                        ? std::clamp(aNpc.health / aNpc.healthMax, 0.0f, 1.0f)
	                        : 0.0f;
	record.level = std::isfinite(aNpc.level) ? uint16_t(std::clamp(std::round(aNpc.level), 0.0f, 65535.0f)) : uint16_t(0);
	const std::string name = CutName(aNpc.name, sizeof record.name - 1);
	std::memcpy(record.name, name.data(), name.size());  // the rest stays 0: the NUL
	return record;
}

std::string HitWords(const proto::McEvent& aEvent)
{
	static const char* const weapons[] = { "unarmed", "blade", "axe", "blunt", "pierce", "arrow" };
	std::string words = aEvent.weapon < std::size(weapons) ? weapons[aEvent.weapon] : std::format("weapon {}", aEvent.weapon);
	for (const auto& [flag, word] : { std::pair{ proto::kHitCritical, "critical" }, std::pair{ proto::kHitProjectile, "projectile" },
	                                  std::pair{ proto::kHitSweep, "sweep" }, std::pair{ proto::kHitFire, "fire" } })
	{
		if (aEvent.flags & flag)
		{
			words += std::string(", ") + word;
		}
	}
	return words;
}

std::vector<proto::ActorRecord> ActorRecords(std::span<const NpcSample> aNpcs, const Vec3d& aV,
                                             const std::unordered_map<uint64_t, NpcBox>& aBoxes, ActorIds& aIds, double aNow)
{
	struct Near
	{
		double           distance = 0.0;
		const NpcSample* npc = nullptr;
	};
	std::vector<Near> near;
	for (const NpcSample& npc : aNpcs)
	{
		const double dx = npc.position.x - aV.x, dy = npc.position.y - aV.y, dz = npc.position.z - aV.z;
		const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
		if (distance <= kActorRange)
		{
			near.push_back({ distance, &npc });
		}
	}
	// Nearest first; the entity breaks a tie, so the same NPCs number the same way every run.
	std::sort(near.begin(), near.end(), [](const Near& aA, const Near& aB) {
		return aA.distance != aB.distance ? aA.distance < aB.distance : aA.npc->entity < aB.npc->entity;
	});
	near.resize(std::min<size_t>(near.size(), proto::kMaxActors));
	std::vector<proto::ActorRecord> records;
	records.reserve(near.size());
	for (const Near& entry : near)
	{
		const auto box = aBoxes.find(entry.npc->entity);
		records.push_back(MakeActorRecord(*entry.npc, aIds.Id(entry.npc->entity, aNow), box != aBoxes.end() ? box->second : kPersonBox));
	}
	return records;
}
}
