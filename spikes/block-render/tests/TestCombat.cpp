#include "Harness.h"

#include "link/Combat.h"

#include <cmath>
#include <limits>

using namespace cpmc;
using namespace cpmc::link;

namespace
{
// Minecraft's hit on stand-in aFormId: aDamage, with knockback aKnockback, and the crit flag if aCrit.
proto::McEvent Hit(uint32_t aFormId, float aDamage, float aKnockback = 0.4f, bool aCrit = false)
{
	proto::McEvent event{};
	event.type = proto::kEvHitActor;
	event.formId = aFormId;
	event.a = aDamage;
	event.d = aKnockback;
	event.flags = aCrit ? proto::kHitCritical : 0u;
	event.weapon = proto::kWeaponBlade;
	return event;
}

// Minecraft's blast at aX aY aZ (its axes), of radius aRadius.
proto::McEvent Explosion(float aX, float aY, float aZ, float aRadius)
{
	proto::McEvent event{};
	event.type = proto::kEvExplosion;
	event.a = aX;
	event.b = aY;
	event.c = aZ;
	event.d = aRadius;
	return event;
}

// gamedataAttackType's values in the game 2.31.
constexpr int32_t kQuickMelee = 9;
constexpr int32_t kStrongMelee = 12;
constexpr int32_t kRanged = 10;
constexpr int32_t kHack = 6;
constexpr int32_t kExplosion = 3;
}

TEST(CombatCountsHitsOnListedNpcsWithDamage)
{
	ActorIds       ids;
	const uint32_t id = ids.Id(0xABC, 10.0);
	HitDrop        drop = HitDrop::Count;
	// A diamond sword at full cooldown, and V's punch of 102, at the hit power by default: 102 x the square root of 7.
	const std::optional<NpcHit> hit = AcceptHit(Hit(id, 7.0f), ids, 10.5, 102.0, kHitPower, drop);
	CHECK(hit.has_value());
	CHECK(hit && hit->entity == 0xABC && hit->formId == id && !hit->strong);
	CHECK_NEAR(hit ? hit->amount : 0.0, 102.0 * std::sqrt(7.0), 1e-9);
	// Strong: a crit, or knockback over 0.45 (a sprint hit); 0.45 itself isn't, compared as SkyCraft compares, in float.
	CHECK(AcceptHit(Hit(id, 10.5f, 0.4f, true), ids, 10.5, 102.0, kHitPower, drop).value_or(NpcHit{}).strong);
	CHECK(AcceptHit(Hit(id, 7.0f, 0.5f), ids, 10.5, 102.0, kHitPower, drop).value_or(NpcHit{}).strong);
	CHECK(!AcceptHit(Hit(id, 7.0f, 0.45f), ids, 10.5, 102.0, kHitPower, drop).value_or(NpcHit{ 0, 0, 0.0, 0.0, true }).strong);
	// /kill's 3.4e38 is capped at 1,000 before the power.
	const std::optional<NpcHit> kill = AcceptHit(Hit(id, std::numeric_limits<float>::max()), ids, 10.5, 102.0, kHitPower, drop);
	CHECK_NEAR(kill ? kill->damage : 0.0, 1000.0, 1e-9);
	CHECK_NEAR(kill ? kill->amount : 0.0, 102.0 * std::sqrt(1000.0), 1e-9);
	// A stand-in listed 2 s ago still counts; 2.5 s ago it doesn't, and nor does a number never given.
	CHECK(AcceptHit(Hit(id, 7.0f), ids, 12.0, 102.0, kHitPower, drop).has_value());
	CHECK(!AcceptHit(Hit(id, 7.0f), ids, 12.5, 102.0, kHitPower, drop).has_value());
	CHECK(drop == HitDrop::NotListed);
	CHECK(!AcceptHit(Hit(id + 1, 7.0f), ids, 10.5, 102.0, kHitPower, drop).has_value());
	CHECK(drop == HitDrop::NotListed);
	// Knockback only, or damage that isn't finite: skipped.
	CHECK(!AcceptHit(Hit(id, 0.0f), ids, 10.5, 102.0, kHitPower, drop).has_value());
	CHECK(drop == HitDrop::NoDamage);
	CHECK(!AcceptHit(Hit(id, std::numeric_limits<float>::quiet_NaN()), ids, 10.5, 102.0, kHitPower, drop).has_value());
	CHECK(drop == HitDrop::NoDamage);
	CHECK(!AcceptHit(Hit(id, std::numeric_limits<float>::infinity()), ids, 10.5, 102.0, kHitPower, drop).has_value());
	CHECK(drop == HitDrop::NoDamage);
	// V's punch not read: no hit.
	CHECK(!AcceptHit(Hit(id, 7.0f), ids, 10.5, 0.0, kHitPower, drop).has_value());
	CHECK(drop == HitDrop::NoPunch);
	CHECK_STR(HitDropName(HitDrop::NoPunch), "no punch");
	CHECK_STR(HitResultName(-2), "the NPC is dead");
	CHECK_STR(HitResultName(7), "result 7");
}

TEST(CombatStartsHitsAtVsPunchTimesDamageToTheHitPower)
{
	ActorIds       ids;
	const uint32_t id = ids.Id(0xABC, 10.0);
	HitDrop        drop = HitDrop::Count;
	const auto     start = [&](float aDamage, double aPower) {
		return AcceptHit(Hit(id, aDamage), ids, 10.5, 104.87, aPower, drop).value_or(NpcHit{}).amount;
	};
	// The square root by default (step 4's fix): a fist stays one of V's punches, a diamond sword is 2.65, an axe 3, and a
	// swing before its cooldown is full is more than its share of a punch.
	CHECK_NEAR(kHitPower, 0.5, 1e-12);
	CHECK_NEAR(start(1.0f, kHitPower), 104.87, 1e-9);
	CHECK_NEAR(start(7.0f, kHitPower), 104.87 * 2.6457513110645907, 1e-9);
	CHECK_NEAR(start(9.0f, kHitPower), 314.61, 1e-9);
	CHECK_NEAR(start(0.25f, kHitPower), 52.435, 1e-9);
	// 1: each point of Minecraft's damage a whole punch, as step 4 had it; 0: every hit one punch.
	CHECK_NEAR(start(7.0f, 1.0), 734.09, 1e-9);
	CHECK_NEAR(start(7.0f, 0.0), 104.87, 1e-9);
}

TEST(CombatMakesTheGamesHurtsIntoMinecraftsKinds)
{
	CHECK(HurtKindOf(kQuickMelee) == proto::kHurtMelee);
	CHECK(HurtKindOf(kStrongMelee) == proto::kHurtMelee);
	for (const int32_t melee : { 0, 5, 7, 11, 14 })  // ChargedWhipAttack, GuardBreak, Melee, Reflect, WhipAttack
	{
		CHECK(HurtKindOf(melee) == proto::kHurtMelee);
	}
	for (const int32_t projectile : { 1, 10, 13 })  // Direct, Ranged, Thrown
	{
		CHECK(HurtKindOf(projectile) == proto::kHurtProjectile);
	}
	CHECK(HurtKindOf(kHack) == proto::kHurtMagic);
	for (const int32_t other : { 2, 3, 4, 8, 15, 16, -1 })  // Effect, Explosion, ForceKill, PressureWave, and none
	{
		CHECK(HurtKindOf(other) == proto::kHurtOther);
	}
	CHECK(PowerAttack(kStrongMelee) && PowerAttack(0) && PowerAttack(5));
	CHECK(!PowerAttack(kQuickMelee) && !PowerAttack(kRanged));
	CHECK_STR(AttackTypeName(kRanged), "Ranged");
	CHECK_STR(AttackTypeName(14), "WhipAttack");
	CHECK_STR(AttackTypeName(17), "attack type 17");
	CHECK_STR(HurtKindName(proto::kHurtProjectile), "projectile");
	// A gunshot of 45 on V, with 456 max health, from a listed ganger: about a tenth of Minecraft's 20.
	ActorIds       ids;
	const uint32_t ganger = ids.Id(0x51, 0.0);
	const Hurt     shot = HurtFrom(GameHurt{ 45.0, kRanged, 0x51 }, 456.0, ids);
	CHECK_NEAR(shot.share, 45.0 / 456.0, 1e-12);
	CHECK(shot.kind == proto::kHurtProjectile && shot.attacker == ganger && !shot.power);
	const proto::InputEvent input = HurtInput(shot);
	CHECK_EQ(input.type, uint16_t(proto::kInHurt));
	CHECK_EQ(input.code, uint16_t(proto::kHurtProjectile));
	CHECK_EQ(input.a, int32_t(std::lround(45.0 / 456.0 * 10000.0)));  // 987: SkyCraft's 9.87, / 5 = 1.97 of Minecraft's 20
	CHECK_EQ(input.b, int32_t(ganger));
	CHECK_EQ(input.c, 0);
	// From someone not listed, or nobody: no attacker. A power attack is flagged. More than V's max health is held to it.
	const Hurt slam = HurtFrom(GameHurt{ 900.0, kStrongMelee, 0x99 }, 456.0, ids);
	CHECK(slam.attacker == 0 && slam.power);
	CHECK_NEAR(slam.share, 1.0, 1e-12);
	CHECK_EQ(HurtInput(slam).a, 10000);
	CHECK_EQ(HurtInput(slam).c, int32_t(proto::kHurtPowerAttack));
	CHECK_EQ(HurtFrom(GameHurt{ 10.0, kExplosion, 0 }, 456.0, ids).attacker, 0u);
	CHECK_NEAR(HurtFrom(GameHurt{ 10.0, kExplosion, 0 }, 0.0, ids).share, 0.0, 1e-12);  // no max health: nothing
}

TEST(CombatSumsHurtsElevenOfMinecraftsTicksApart)
{
	HurtSums   sums;
	const Hurt small{ 0.05, proto::kHurtProjectile, 4, false };
	const Hurt big{ 0.20, proto::kHurtMelee, 9, false };
	const Hurt power{ 0.01, proto::kHurtMelee, 3, true };
	const Hurt last{ 0.02, proto::kHurtMagic, 5, false };
	// The first goes at once.
	CHECK(sums.Add(small, 10.0, 50.0).has_value());
	// Within 11 ticks (0.55 s at 50 ms), the rest wait in a sum, which takes its biggest part's kind and attacker, and a
	// power attack if any part had one.
	CHECK(!sums.Add(big, 10.1, 50.0).has_value());
	CHECK(!sums.Add(power, 10.2, 50.0).has_value());
	CHECK(!sums.Add(last, 10.3, 50.0).has_value());
	CHECK(sums.Waiting());
	CHECK_EQ(sums.Summed(), uint64_t(3));
	CHECK(!sums.Due(10.54, 50.0).has_value());
	const std::optional<Hurt> sum = sums.Due(10.56, 50.0);
	CHECK(sum.has_value());
	CHECK_NEAR(sum ? sum->share : 0.0, 0.23, 1e-12);
	CHECK(sum && sum->kind == proto::kHurtMelee && sum->attacker == 9 && sum->power);
	CHECK(!sums.Waiting());
	CHECK(!sums.Due(20.0, 50.0).has_value());
	// The sum's send counts as the last: a hurt 0.3 s after it waits again, until 11 ticks after the sum.
	CHECK(!sums.Add(small, 10.85, 50.0).has_value());
	CHECK(!sums.Due(11.10, 50.0).has_value());
	CHECK(sums.Due(11.12, 50.0).has_value());
	// Long after, at once again.
	CHECK(sums.Add(small, 30.0, 50.0).has_value());
	// At 100 ms a tick, the wait is 1.1 s; a tick length outside 1 to 1,000 ms counts as 50.
	CHECK(!sums.Add(small, 31.0, 100.0).has_value());
	CHECK(!sums.Due(31.09, 100.0).has_value());
	CHECK(sums.Due(31.11, 100.0).has_value());
	CHECK(sums.Add(small, 31.67, 0.0).has_value());
}

TEST(CombatLetsMinecraftsDeathKillVInBuildModeOrJustAfter)
{
	CHECK(DeathKillsV(true, true, std::nullopt, 5.0));
	CHECK(!DeathKillsV(false, true, std::nullopt, 5.0));  // combat off
	CHECK(!DeathKillsV(true, false, std::nullopt, 5.0));  // build mode never on
	// Build mode ends as soon as Minecraft's player is dead, which can come before the event.
	CHECK(DeathKillsV(true, false, 3.0, 5.0));
	CHECK(!DeathKillsV(true, false, 2.9, 5.0));
}

TEST(CombatPutsVsGodModeOnWithBuildModeAndCombat)
{
	GodMode god;
	CHECK(!god.Frame(true, true, false).has_value());  // V not found: nothing
	// V found: off first, in case a save kept it; then on, since both are on.
	CHECK(god.Frame(true, true, true) == std::optional<bool>(false));
	CHECK(god.Frame(true, true, true) == std::optional<bool>(true));
	CHECK(god.On());
	CHECK(!god.Frame(true, true, true).has_value());
	// Build mode ends: off. Combat on first, then build mode: on.
	CHECK(god.Frame(true, false, true) == std::optional<bool>(false));
	CHECK(!god.Frame(true, false, true).has_value());
	CHECK(god.Frame(true, true, true) == std::optional<bool>(true));
	// Combat off during build mode: off.
	CHECK(god.Frame(false, true, true) == std::optional<bool>(false));
	CHECK(god.Frame(true, true, true) == std::optional<bool>(true));
	// V lost and found again, after a load: off once, then on again.
	CHECK(!god.Frame(true, true, false).has_value());
	CHECK(god.Frame(true, true, true) == std::optional<bool>(false));
	CHECK(!god.On());
	CHECK(god.Frame(true, true, true) == std::optional<bool>(true));
}

TEST(CombatSaysWhatThePipelineDidToOurHits)
{
	CHECK_STR(LandedWords(0), "");
	CHECK_STR(LandedWords(kLandedCritical | kLandedArmour), "critical, armour");
	CHECK_STR(LandedWords(kLandedBlocked | kLandedParried | kLandedMitigated | kLandedKilled), "blocked, parried, mitigated, killed it");
	CHECK_STR(LandedWords(64), "");  // a bit it doesn't know
	CHECK_STR(HitEndedName(4), "dodged");
	CHECK_STR(HitEndedName(5), "evaded");
	CHECK_STR(HitEndedName(1), "friendly to V");
	CHECK_STR(HitEndedName(0), "another of the game's reasons");
	CHECK_STR(HitEndedName(9), "reason 9");
}

TEST(CombatSwitchesVsWeaponAndMeshesWhileWanted)
{
	// Drawing V's unarmed weapon: on once, off once.
	VSwitch arm;
	CHECK(!arm.Frame(true, false, 0.0).has_value());  // V not found: nothing
	CHECK(arm.Frame(true, true, 1.0) == std::optional<bool>(true));
	CHECK(arm.On());
	CHECK(!arm.Frame(true, true, 9.0).has_value());
	CHECK(arm.Frame(false, true, 10.0) == std::optional<bool>(false));
	CHECK(!arm.Frame(false, true, 11.0).has_value());
	// Lost while on, as during a load: off, with nothing to switch back; found again while wanted: on.
	CHECK(arm.Frame(true, true, 12.0) == std::optional<bool>(true));
	CHECK(!arm.Frame(true, false, 13.0).has_value());
	CHECK(!arm.On());
	CHECK(!arm.Frame(false, true, 14.0).has_value());
	CHECK(arm.Frame(true, true, 15.0) == std::optional<bool>(true));
	// Hiding V's meshes: on again every half second while wanted, so that a weapon drawn meanwhile is hidden too.
	VSwitch hide(0.5);
	CHECK(hide.Frame(true, true, 1.0) == std::optional<bool>(true));
	CHECK(!hide.Frame(true, true, 1.4).has_value());
	CHECK(hide.Frame(true, true, 1.5) == std::optional<bool>(true));
	CHECK(!hide.Frame(true, true, 1.9).has_value());
	CHECK(hide.Frame(false, true, 2.0) == std::optional<bool>(false));
	CHECK(!hide.Frame(false, true, 2.6).has_value());
}

TEST(CombatTakesMinecraftsBlastsIntoTheGamesAxes)
{
	// TNT at Minecraft 620.5 6.25 2007.5, radius 4: its centre in the world's axes (x, -z, y), and a reach of twice the
	// radius, as SkyCraft's.
	const std::optional<Blast> blast = AcceptBlast(Explosion(620.5f, 6.25f, 2007.5f, 4.0f), 12.0);
	CHECK(blast.has_value());
	CHECK_NEAR(blast ? blast->centre.x : 0.0, 620.5, 1e-9);
	CHECK_NEAR(blast ? blast->centre.y : 0.0, -2007.5, 1e-9);
	CHECK_NEAR(blast ? blast->centre.z : 0.0, 6.25, 1e-9);
	CHECK_NEAR(blast ? blast->reach : 0.0, 8.0, 1e-9);
	CHECK_NEAR(blast ? blast->at : 0.0, 12.0, 1e-9);
	CHECK_EQ(blast ? blast->hits : 1u, 0u);
	// A radius past kBlastRadiusCap is held to it: TNT summoned with an explosion power of 100 reaches 32 m, not 200.
	CHECK_NEAR(AcceptBlast(Explosion(620.5f, 6.25f, 2007.5f, 100.0f), 12.0).value_or(Blast{}).reach, 32.0, 1e-9);
	// No radius, or a centre or radius that isn't finite: no blast.
	CHECK(!AcceptBlast(Explosion(620.5f, 6.25f, 2007.5f, 0.0f), 12.0).has_value());
	CHECK(!AcceptBlast(Explosion(620.5f, 6.25f, 2007.5f, std::numeric_limits<float>::quiet_NaN()), 12.0).has_value());
	CHECK(!AcceptBlast(Explosion(std::numeric_limits<float>::infinity(), 6.25f, 2007.5f, 4.0f), 12.0).has_value());
}

TEST(CombatCountsHitsWithinABlastsWindowAndReach)
{
	Blasts blasts;
	blasts.Add(*AcceptBlast(Explosion(0.0f, 0.0f, 0.0f, 4.0f), 0.0));  // at the world's origin, reach 8
	// An NPC 7.9 m from the centre, hit 0.1 s after the blast: one of its hits. 8.1 m away: not.
	const std::optional<Blast> hit = blasts.HitOn({ 7.9, 0.0, 0.0 }, 0.1);
	CHECK(hit.has_value());
	CHECK_EQ(hit ? hit->hits : 0u, 1u);
	CHECK(!blasts.HitOn({ 8.1, 0.0, 0.0 }, 0.1).has_value());
	// The reach is straight-line, height included: 6 east and 6 up is 8.49 m away. Exactly 8 is within it.
	CHECK(!blasts.HitOn({ 6.0, 0.0, 6.0 }, 0.1).has_value());
	CHECK(blasts.HitOn({ 0.0, 8.0, 0.0 }, 0.1).has_value());
	// 0.15 s after the blast still counts; later doesn't, and nor does a hit from before it.
	CHECK(blasts.HitOn({ 1.0, 0.0, 0.0 }, 0.15).has_value());
	CHECK(!blasts.HitOn({ 1.0, 0.0, 0.0 }, 0.16).has_value());
	CHECK(!blasts.HitOn({ 1.0, 0.0, 0.0 }, -0.01).has_value());
	// Of two blasts, the newer counts the hit.
	blasts.Add(*AcceptBlast(Explosion(0.0f, 0.0f, 0.0f, 4.0f), 0.125));
	const std::optional<Blast> newer = blasts.HitOn({ 1.0, 0.0, 0.0 }, 0.13);
	CHECK(newer.has_value());
	CHECK(newer && newer->at == 0.125 && newer->hits == 1);
	CHECK_EQ(blasts.Count(), uint64_t(2));
}

TEST(CombatPushesEachBlastOnceAQuarterSecondAfter)
{
	Blasts blasts;
	blasts.Add(*AcceptBlast(Explosion(1.0f, 2.0f, 3.0f, 4.0f), 0.0));
	blasts.HitOn({ 1.0, -3.0, 2.0 }, 0.0625);
	blasts.HitOn({ 2.0, -3.0, 2.0 }, 0.125);
	blasts.Add(*AcceptBlast(Explosion(4.0f, 5.0f, 6.0f, 3.0f), 0.125));
	CHECK(blasts.Pushes(0.24).empty());
	// The first's push, a quarter second after it, with its two hits; then the second's.
	const std::vector<Blast> first = blasts.Pushes(0.25);
	CHECK_EQ(first.size(), size_t(1));
	CHECK_NEAR(first.empty() ? 0.0 : first[0].reach, 8.0, 1e-9);
	CHECK_EQ(first.empty() ? 0u : first[0].hits, 2u);
	CHECK(blasts.Pushes(0.3).empty());  // once
	const std::vector<Blast> second = blasts.Pushes(0.375);
	CHECK_EQ(second.size(), size_t(1));
	CHECK_NEAR(second.empty() ? 0.0 : second[0].reach, 6.0, 1e-9);
	CHECK(blasts.Pushes(5.0).empty());
	CHECK_EQ(blasts.Count(), uint64_t(2));
}

TEST(CombatSetsMinecraftsHealthThroughCyberCraftTweaks)
{
	// V's health share, for Minecraft's player: kInHurt of no damage, so SkyCraft's own hurtPlayer leaves it, with
	// CyberCraft Tweaks' flag, the share x 10,000 in b.
	const proto::InputEvent half = HealthInput(0.5);
	CHECK_EQ(half.type, uint16_t(proto::kInHurt));
	CHECK_EQ(half.code, uint16_t(proto::kHurtOther));
	CHECK_EQ(half.a, 0);
	CHECK_EQ(half.b, 5000);
	CHECK_EQ(half.c, int32_t(kHurtSetHealth));
	CHECK_EQ(HealthInput(0.12345).b, 1235);  // rounded
	CHECK_EQ(HealthInput(1.5).b, 10000);     // held to 0 to 1
	CHECK_EQ(HealthInput(-0.5).b, 0);
	CHECK_EQ(HealthInput(std::numeric_limits<double>::quiet_NaN()).b, 0);
}
