#include "link/Combat.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <utility>

namespace cpmc::link
{
namespace
{
// gamedataAttackType in the game 2.31, by value (the decompiled scripts' orphans.swift).
constexpr std::array<const char*, 15> kAttackTypes{ "ChargedWhipAttack", "Direct", "Effect",       "Explosion",  "ForceKill",
	                                                 "GuardBreak",        "Hack",   "Melee",        "PressureWave", "QuickMelee",
	                                                 "Ranged",            "Reflect", "StrongMelee", "Thrown",     "WhipAttack" };
enum AttackType : int32_t
{
	kChargedWhipAttack = 0,
	kDirect = 1,
	kGuardBreak = 5,
	kHack = 6,
	kMelee = 7,
	kQuickMelee = 9,
	kRanged = 10,
	kReflect = 11,
	kStrongMelee = 12,
	kThrown = 13,
	kWhipAttack = 14,
};

// The sums' wait, in seconds: kHurtTicks of Minecraft's ticks.
double Window(double aTickMs)
{
	const double tickMs = aTickMs >= 1.0 && aTickMs <= 1000.0 ? aTickMs : 50.0;
	return kHurtTicks * tickMs / 1000.0;
}

// The set-health flag is a bit of its own, beside SkyCraft's blocked and power attack flags.
static_assert((kHurtSetHealth & (proto::kHurtBlockedInSkyrim | proto::kHurtPowerAttack)) == 0);
}

const char* HitDropName(HitDrop aDrop)
{
	switch (aDrop)
	{
	case HitDrop::NotListed:
		return "not listed";
	case HitDrop::NoDamage:
		return "no damage";
	case HitDrop::NoPunch:
		return "no punch";
	default:
		return "?";
	}
}

std::optional<NpcHit> AcceptHit(const proto::McEvent& aEvent, const ActorIds& aIds, double aNow, double aPunch, double aPower,
                                HitDrop& aDrop)
{
	const std::optional<uint64_t> entity = aIds.Entity(aEvent.formId, aNow);
	if (!entity)
	{
		aDrop = HitDrop::NotListed;
		return std::nullopt;
	}
	const double damage = aEvent.a;
	if (!std::isfinite(damage) || damage <= 0.0)
	{
		aDrop = HitDrop::NoDamage;
		return std::nullopt;
	}
	if (!std::isfinite(aPunch) || aPunch <= 0.0)
	{
		aDrop = HitDrop::NoPunch;
		return std::nullopt;
	}
	NpcHit hit;
	hit.formId = aEvent.formId;
	hit.entity = *entity;
	hit.damage = std::min(damage, kHitCap);
	hit.amount = aPunch * std::pow(hit.damage, aPower);
	hit.strong = (aEvent.flags & proto::kHitCritical) != 0 || aEvent.d > float(kStrongKnockback);  // as SkyCraft's push > 0.45f
	return hit;
}

std::string HitResultName(int32_t aResult)
{
	switch (aResult)
	{
	case 1:
		return "queued";
	case 0:
		return "V isn't found";
	case -1:
		return "the NPC is gone";
	case -2:
		return "the NPC is dead";
	case -3:
		return "no attack record to make it with";
	default:
		return std::format("result {}", aResult);
	}
}

std::string LandedWords(uint32_t aFlags)
{
	constexpr std::array<std::pair<uint32_t, const char*>, 6> kWords{ { { kLandedCritical, "critical" },
		                                                                { kLandedBlocked, "blocked" },
		                                                                { kLandedParried, "parried" },
		                                                                { kLandedArmour, "armour" },
		                                                                { kLandedMitigated, "mitigated" },
		                                                                { kLandedKilled, "killed it" } } };
	std::string words;
	for (const auto& [flag, word] : kWords)
	{
		if ((aFlags & flag) != 0)
		{
			words += (words.empty() ? "" : ", ") + std::string(word);
		}
	}
	return words;
}

std::string HitEndedName(int32_t aWhy)
{
	switch (aWhy)
	{
	case 0:
		return "another of the game's reasons";
	case 1:
		return "friendly to V";
	case 2:
		return "invulnerable";
	case 3:
		return "already dead";
	case 4:
		return "dodged";
	case 5:
		return "evaded";
	case 6:
		return "killed at once";
	case 7:
		return "no damage left";
	default:
		return std::format("reason {}", aWhy);
	}
}

proto::HurtKind HurtKindOf(int32_t aAttackType)
{
	switch (aAttackType)
	{
	case kQuickMelee:
	case kStrongMelee:
	case kMelee:
	case kGuardBreak:
	case kWhipAttack:
	case kChargedWhipAttack:
	case kReflect:
		return proto::kHurtMelee;
	case kRanged:
	case kThrown:
	case kDirect:
		return proto::kHurtProjectile;
	case kHack:
		return proto::kHurtMagic;
	default:
		return proto::kHurtOther;
	}
}

bool PowerAttack(int32_t aAttackType)
{
	return aAttackType == kStrongMelee || aAttackType == kChargedWhipAttack || aAttackType == kGuardBreak;
}

std::string AttackTypeName(int32_t aAttackType)
{
	return aAttackType >= 0 && size_t(aAttackType) < kAttackTypes.size() ? kAttackTypes[size_t(aAttackType)]
	                                                                     : std::format("attack type {}", aAttackType);
}

const char* HurtKindName(proto::HurtKind aKind)
{
	switch (aKind)
	{
	case proto::kHurtMelee:
		return "melee";
	case proto::kHurtProjectile:
		return "projectile";
	case proto::kHurtMagic:
		return "magic";
	default:
		return "other";
	}
}

Hurt HurtFrom(const GameHurt& aHurt, double aMaxHealth, const ActorIds& aIds)
{
	Hurt hurt;
	hurt.share = aMaxHealth > 0.0 && std::isfinite(aHurt.amount) ? std::clamp(aHurt.amount / aMaxHealth, 0.0, 1.0) : 0.0;
	hurt.kind = HurtKindOf(aHurt.attackType);
	hurt.power = PowerAttack(aHurt.attackType);
	hurt.attacker = aHurt.instigator != 0 ? aIds.IdOf(aHurt.instigator).value_or(0) : 0;
	return hurt;
}

proto::InputEvent HurtInput(const Hurt& aHurt)
{
	proto::InputEvent input{};
	input.type = proto::kInHurt;
	input.code = aHurt.kind;
	input.a = int32_t(std::lround(std::clamp(aHurt.share, 0.0, 1.0) * 10000.0));
	input.b = int32_t(aHurt.attacker);
	input.c = aHurt.power ? int32_t(proto::kHurtPowerAttack) : 0;
	return input;
}

std::optional<Hurt> HurtSums::Add(const Hurt& aHurt, double aNow, double aTickMs)
{
	if (!sum_ && (!lastSent_ || aNow - *lastSent_ >= Window(aTickMs)))
	{
		lastSent_ = aNow;
		return aHurt;
	}
	++summed_;
	if (!sum_)
	{
		sum_ = Sum{ aHurt, aHurt.share };
		return std::nullopt;
	}
	Sum& sum = *sum_;
	if (aHurt.share > sum.biggest)
	{
		sum.biggest = aHurt.share;
		sum.hurt.kind = aHurt.kind;
		sum.hurt.attacker = aHurt.attacker;
	}
	sum.hurt.share += aHurt.share;
	sum.hurt.power = sum.hurt.power || aHurt.power;
	return std::nullopt;
}

std::optional<Hurt> HurtSums::Due(double aNow, double aTickMs)
{
	if (!sum_ || (lastSent_ && aNow - *lastSent_ < Window(aTickMs)))
	{
		return std::nullopt;
	}
	const Hurt hurt = sum_->hurt;
	sum_.reset();
	lastSent_ = aNow;
	return hurt;
}

bool DeathKillsV(bool aCombat, bool aBuildMode, std::optional<double> aBuildEndedAt, double aNow)
{
	return aCombat && (aBuildMode || (aBuildEndedAt && aNow - *aBuildEndedAt <= kDeathWindow));
}

std::optional<bool> GodMode::Frame(bool aCombat, bool aBuildMode, bool aVFound)
{
	if (!aVFound)
	{
		vFound_ = false;
		return std::nullopt;
	}
	if (!vFound_)
	{
		vFound_ = true;
		clearDue_ = true;
	}
	if (clearDue_)
	{
		clearDue_ = false;
		on_ = false;
		return false;
	}
	const bool wanted = aCombat && aBuildMode;
	if (wanted == on_)
	{
		return std::nullopt;
	}
	on_ = wanted;
	return wanted;
}

std::optional<bool> VSwitch::Frame(bool aWanted, bool aVFound, double aNow)
{
	if (!aVFound)
	{
		on_ = false;
		return std::nullopt;
	}
	if (aWanted && (!on_ || (refresh_ > 0.0 && aNow - onAt_ >= refresh_)))
	{
		on_ = true;
		onAt_ = aNow;
		return true;
	}
	if (!aWanted && on_)
	{
		on_ = false;
		return false;
	}
	return std::nullopt;
}

std::optional<Blast> AcceptBlast(const proto::McEvent& aEvent, double aNow)
{
	if (!std::isfinite(aEvent.a) || !std::isfinite(aEvent.b) || !std::isfinite(aEvent.c) || !std::isfinite(aEvent.d) || aEvent.d <= 0.0f)
	{
		return std::nullopt;
	}
	Blast blast;
	blast.centre = WorldFromMc({ aEvent.a, aEvent.b, aEvent.c });
	blast.reach = 2.0 * std::min(double(aEvent.d), kBlastRadiusCap);
	blast.at = aNow;
	return blast;
}

void Blasts::Add(const Blast& aBlast)
{
	recent_.push_back(aBlast);
	++count_;
}

std::optional<Blast> Blasts::HitOn(const Vec3d& aFeet, double aNow)
{
	for (auto blast = recent_.rbegin(); blast != recent_.rend(); ++blast)
	{
		const double after = aNow - blast->at;
		const double distance = std::hypot(aFeet.x - blast->centre.x, aFeet.y - blast->centre.y, aFeet.z - blast->centre.z);
		if (after >= 0.0 && after <= kBlastWindow && distance <= blast->reach)
		{
			++blast->hits;
			return *blast;
		}
	}
	return std::nullopt;
}

std::vector<Blast> Blasts::Pushes(double aNow)
{
	std::vector<Blast> due;
	std::erase_if(recent_, [&](const Blast& aBlast) {
		if (aNow - aBlast.at < kPushDelay)
		{
			return false;
		}
		due.push_back(aBlast);
		return true;
	});
	return due;
}

proto::InputEvent HealthInput(double aShare)
{
	proto::InputEvent input{};
	input.type = proto::kInHurt;
	input.code = proto::kHurtOther;
	input.a = 0;
	input.b = std::isfinite(aShare) ? int32_t(std::lround(std::clamp(aShare, 0.0, 1.0) * 10000.0)) : 0;
	input.c = int32_t(kHurtSetHealth);
	return input;
}
}
