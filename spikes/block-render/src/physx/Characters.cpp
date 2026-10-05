#include "physx/Characters.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>
#include <utility>

namespace cpmc::px
{
namespace
{
double Length(const Vec3d& aVector)
{
	return std::sqrt(aVector.x * aVector.x + aVector.y * aVector.y + aVector.z * aVector.z);
}

// How far the capsule's axis leans from upright, as the cosine of the angle: 1 standing, 0 lying down.
double Upright(const CapsuleSeen& aCapsule)
{
	const double length = Length(aCapsule.axis);
	return length > 0.0 ? std::fabs(aCapsule.axis.z) / length : 0.0;
}
}

std::optional<CapsuleSeen> PickCapsule(std::span<const CapsuleSeen> aSeen, const Vec3d& aFeet)
{
	const double               upright = std::cos(kCapsuleTilt * std::numbers::pi / 180.0);
	std::optional<CapsuleSeen> best;
	for (const CapsuleSeen& capsule : aSeen)
	{
		if ((capsule.queryFilter[2] & kControllerQueryBit) == 0 || Upright(capsule) < upright)
		{
			continue;
		}
		// The axis's distance from the feet: the length of (feet - centre) x axis, over the axis's length.
		const Vec3d  to{ aFeet.x - capsule.centre.x, aFeet.y - capsule.centre.y, aFeet.z - capsule.centre.z };
		const Vec3d& axis = capsule.axis;
		const Vec3d  cross{ to.y * axis.z - to.z * axis.y, to.z * axis.x - to.x * axis.z, to.x * axis.y - to.y * axis.x };
		if (Length(cross) / Length(axis) > kCapsuleAcross)
		{
			continue;
		}
		if (!best || capsule.radius < best->radius)
		{
			best = capsule;
		}
	}
	return best;
}

link::NpcBox BoxOf(const CapsuleSeen& aCapsule, const Vec3d& aFeet)
{
	const double reach = Upright(aCapsule) * aCapsule.halfHeight + aCapsule.radius;  // up and down from its centre
	const double bottom = aCapsule.centre.z - reach;
	const double top = aCapsule.centre.z + reach;
	return { 2.0 * aCapsule.radius, std::max(0.0, aFeet.z - bottom), top - aFeet.z };
}

std::vector<uint64_t> NpcSizes::ToLookFor(std::span<const uint64_t> aListed, double aNow)
{
	std::vector<uint64_t> out;
	for (const uint64_t entity : aListed)  // never looked for
	{
		if (out.size() == kCapsulesPerFrame)
		{
			break;
		}
		if (looked_.try_emplace(entity, aNow).second)
		{
			out.push_back(entity);
		}
	}
	for (const uint64_t entity : aListed)  // then not found, and due again
	{
		if (out.size() == kCapsulesPerFrame)
		{
			break;
		}
		const auto looked = looked_.find(entity);
		if (looked != looked_.end() && !capsules_.contains(entity) && aNow - looked->second >= kCapsuleRetry)
		{
			looked->second = aNow;
			out.push_back(entity);
		}
	}
	return out;
}

void NpcSizes::Found(uint64_t aEntity, const CapsuleSeen& aCapsule, const Vec3d& aFeet)
{
	boxes_[aEntity] = BoxOf(aCapsule, aFeet);
	capsules_[aEntity] = aCapsule;
}

void NpcSizes::Forget(const link::ActorIds& aIds)
{
	std::erase_if(looked_, [&](const auto& aEntry) { return !aIds.IdOf(aEntry.first); });
	std::erase_if(boxes_, [&](const auto& aEntry) { return !aIds.IdOf(aEntry.first); });
	std::erase_if(capsules_, [&](const auto& aEntry) { return !aIds.IdOf(aEntry.first); });
}

const CapsuleSeen* NpcSizes::Capsule(uint64_t aEntity) const
{
	const auto found = capsules_.find(aEntity);
	return found != capsules_.end() ? &found->second : nullptr;
}

std::string DescribeNpc(const link::NpcSample& aNpc, uint32_t aId, double aDistance, const NpcSizes& aSizes)
{
	const auto         found = aSizes.Boxes().find(aNpc.entity);
	const link::NpcBox box = found != aSizes.Boxes().end() ? found->second : link::kPersonBox;
	std::string        from;
	if (const CapsuleSeen* capsule = aSizes.Capsule(aNpc.entity))
	{
		from = std::format("its capsule's: radius {:.2f}, half-height {:.2f}, leaning {:.0f} degrees, query filter {:x}:{:x}:{:x}:{:x}",
		                   capsule->radius, capsule->halfHeight, std::acos(std::min(Upright(*capsule), 1.0)) * 180.0 / std::numbers::pi,
		                   capsule->queryFilter[0], capsule->queryFilter[1], capsule->queryFilter[2], capsule->queryFilter[3]);
	}
	else
	{
		from = aSizes.LookedFor(aNpc.entity) ? "a person's: no capsule found" : "a person's until its capsule is looked for";
	}
	std::string flags;
	for (const auto& [flag, word] : { std::pair{ link::proto::kActorHostile, "hostile" }, std::pair{ link::proto::kActorEssential, "essential" },
	                                  std::pair{ link::proto::kActorInCombat, "in combat" } })
	{
		if (aNpc.flags & flag)
		{
			flags += (flags.empty() ? "" : ", ") + std::string(word);
		}
	}
	return std::format("{} {}, {:.1f} m: {:.2f} wide, from {:.2f} below its position to {:.2f} above, {}; health {:.0f} of {:.0f}, level "
	                   "{:.0f}; {}",
	                   aId, aNpc.name.empty() ? "(no name)" : aNpc.name, aDistance, box.width, box.below, box.above, from, aNpc.health,
	                   aNpc.healthMax, aNpc.level, flags.empty() ? "no flags" : flags);
}
}
