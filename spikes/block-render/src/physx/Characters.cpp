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

// Whether the capsule stands as an NPC's: leaning at most kCapsuleTilt from upright, its axis within kCapsuleAcross of
// aFeet.
bool StandsAt(const CapsuleSeen& aCapsule, const Vec3d& aFeet)
{
	if (Upright(aCapsule) < std::cos(kCapsuleTilt * std::numbers::pi / 180.0))
	{
		return false;
	}
	// The axis's distance from the feet: the length of (feet - centre) x axis, over the axis's length.
	const Vec3d  to{ aFeet.x - aCapsule.centre.x, aFeet.y - aCapsule.centre.y, aFeet.z - aCapsule.centre.z };
	const Vec3d& axis = aCapsule.axis;
	const Vec3d  cross{ to.y * axis.z - to.z * axis.y, to.z * axis.x - to.x * axis.z, to.x * axis.y - to.y * axis.x };
	return Length(cross) / Length(axis) <= kCapsuleAcross;
}

// How far the capsule reaches up and down from its centre.
double Reach(const CapsuleSeen& aCapsule)
{
	return Upright(aCapsule) * aCapsule.halfHeight + aCapsule.radius;
}
}

std::optional<CapsuleSeen> PickCapsule(std::span<const CapsuleSeen> aSeen, const Vec3d& aFeet)
{
	std::optional<CapsuleSeen> best;
	for (const CapsuleSeen& capsule : aSeen)
	{
		if ((capsule.queryFilter[2] & kControllerQueryBit) == 0 || !StandsAt(capsule, aFeet))
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

CapsuleSeen TallestCapsule(std::span<const CapsuleSeen> aSeen, const CapsuleSeen& aController, const Vec3d& aFeet)
{
	CapsuleSeen tallest = aController;
	for (const CapsuleSeen& capsule : aSeen)
	{
		if (StandsAt(capsule, aFeet) && capsule.centre.z + Reach(capsule) > tallest.centre.z + Reach(tallest))
		{
			tallest = capsule;
		}
	}
	return tallest;
}

link::NpcBox BoxOf(const CapsuleSeen& aController, const CapsuleSeen& aTallest, const Vec3d& aFeet)
{
	const double bottom = aController.centre.z - Reach(aController);
	const double top = std::max(aController.centre.z + Reach(aController), aTallest.centre.z + Reach(aTallest));
	return { 2.0 * aController.radius, std::max(0.0, aFeet.z - bottom), top - aFeet.z };
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

void NpcSizes::Found(uint64_t aEntity, const CapsuleSeen& aController, const CapsuleSeen& aTallest, const Vec3d& aFeet)
{
	boxes_[aEntity] = BoxOf(aController, aTallest, aFeet);
	capsules_[aEntity] = aController;
	tallest_[aEntity] = aTallest;
}

void NpcSizes::Forget(const link::ActorIds& aIds)
{
	std::erase_if(looked_, [&](const auto& aEntry) { return !aIds.IdOf(aEntry.first); });
	std::erase_if(boxes_, [&](const auto& aEntry) { return !aIds.IdOf(aEntry.first); });
	std::erase_if(capsules_, [&](const auto& aEntry) { return !aIds.IdOf(aEntry.first); });
	std::erase_if(tallest_, [&](const auto& aEntry) { return !aIds.IdOf(aEntry.first); });
}

const CapsuleSeen* NpcSizes::Capsule(uint64_t aEntity) const
{
	const auto found = capsules_.find(aEntity);
	return found != capsules_.end() ? &found->second : nullptr;
}

const CapsuleSeen* NpcSizes::Tallest(uint64_t aEntity) const
{
	const auto found = tallest_.find(aEntity);
	return found != tallest_.end() ? &found->second : nullptr;
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
		const CapsuleSeen* tallest = aSizes.Tallest(aNpc.entity);
		if (tallest && tallest->centre.z + Reach(*tallest) > capsule->centre.z + Reach(*capsule))
		{
			from += std::format(", its top another's: radius {:.2f}, query filter {:x}:{:x}:{:x}:{:x}", tallest->radius,
			                    tallest->queryFilter[0], tallest->queryFilter[1], tallest->queryFilter[2], tallest->queryFilter[3]);
		}
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
