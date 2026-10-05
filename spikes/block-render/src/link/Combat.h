#pragma once

#include "link/Actors.h"
#include "link/Rings.h"

#include <cstdint>
#include <optional>
#include <string>

// Combat both ways (spike 6's "Part 2"), the parts that don't need the game: which of Minecraft's hits on NPCs count and
// how much each does; V's hurts as SkyCraft's kInHurt, summed so that Minecraft takes them all; when Minecraft's death
// kills V; and when V's god mode goes on and off. Pure: the tests give it hits and hurts of their own.
namespace cpmc::link
{
constexpr double kHitCap = 1000.0;         // Minecraft's damage, at most: /kill on a stand-in sends 3.4e38
constexpr double kStrongKnockback = 0.45;  // SkyCraft staggers above this knockback, or on a crit, comparing floats
constexpr double kDeathWindow = 2.0;       // seconds after build mode ends that Minecraft's death still kills V
constexpr int    kHurtTicks = 11;          // Minecraft's ticks between kInHurts: within 10 it takes only the excess

// A hit of Minecraft's on an NPC, as it's applied.
struct NpcHit
{
	uint32_t formId = 0;
	uint64_t entity = 0;    // the NPC, as listed
	double   damage = 0.0;  // Minecraft's, capped
	double   amount = 0.0;  // damage x V's punch
	bool     strong = false;
};

// Why a hit doesn't count.
enum class HitDrop
{
	NotListed,  // its formId wasn't listed in the last kHitTrail seconds
	NoDamage,   // knockback only, or damage that isn't finite
	NoPunch,    // V's punch isn't read, or isn't over 0
	Count,
};

const char* HitDropName(HitDrop aDrop);

// Whether a kEvHitActor counts: its NPC listed in the last kHitTrail seconds (aIds), its damage finite and over 0, capped
// at kHitCap, and V's punch, aPunch, over 0. Strong: a crit, or knockback over kStrongKnockback. Nothing, with aDrop
// saying why, if it doesn't count.
std::optional<NpcHit> AcceptHit(const proto::McEvent& aEvent, const ActorIds& aIds, double aNow, double aPunch, HitDrop& aDrop);

// What the module's Hit says about a hit it was given: 1 queued; 0 V isn't found; -1 the NPC is gone; -2 it's dead; -3
// there's no attack record to make it with.
std::string HitResultName(int32_t aResult);

// A hit on V that the module's DealDamages wrap took, in the game's terms: the health it would have taken, after V's
// armour and the difficulty; its gamedataAttackType, as a number; and its instigator's entity, or 0.
struct GameHurt
{
	double   amount = 0.0;
	int32_t  attackType = 0;
	uint64_t instigator = 0;
};

// The same, for Minecraft: its share of V's max health, kind and attacker (its stand-in's formId, or 0), and whether it's
// a power attack.
struct Hurt
{
	double          share = 0.0;
	proto::HurtKind kind = proto::kHurtOther;
	uint32_t        attacker = 0;
	bool            power = false;
};

// SkyCraft's kind for a gamedataAttackType: melee for QuickMelee, StrongMelee, Melee, GuardBreak, WhipAttack,
// ChargedWhipAttack and Reflect; projectile for Ranged, Thrown and Direct; magic for Hack; other for the rest.
proto::HurtKind HurtKindOf(int32_t aAttackType);
// A power attack: StrongMelee, ChargedWhipAttack and GuardBreak.
bool PowerAttack(int32_t aAttackType);
// The gamedataAttackType's name, "QuickMelee"; or "attack type 17" for one the game 2.31 doesn't have.
std::string AttackTypeName(int32_t aAttackType);
const char* HurtKindName(proto::HurtKind aKind);  // "melee", "projectile", "magic", "other"

// aHurt for Minecraft: its share of aMaxHealth, V's max health, held to 0 to 1; its kind and power from its attack type;
// its attacker's number if aIds has the instigator listed, else 0.
Hurt HurtFrom(const GameHurt& aHurt, double aMaxHealth, const ActorIds& aIds);

// kInHurt: code = the kind, a = the share x 10,000 rounded (the same share of Minecraft's 20 health, x 5 and x 100 for
// SkyCraft's / 5 and / 100), b = the attacker, c = the power attack flag. "Blocked" is never set: V can't block in build
// mode.
proto::InputEvent HurtInput(const Hurt& aHurt);

// Hurts summed so that Minecraft takes them all: for 10 of its ticks after a hurt it takes from another only what's more
// than the first, and it applies each at its next tick. A hurt goes at once when none went in the last kHurtTicks ticks;
// otherwise it joins a sum, which goes when those ticks are up, with its biggest part's kind and attacker, and a power
// attack if any part had one.
class HurtSums
{
public:
	// aHurt at aNow (seconds). aTickMs: Minecraft's milliseconds a tick (McState's tickMs; 50 if it isn't from 1 to 1,000).
	// It, if it goes now.
	std::optional<Hurt> Add(const Hurt& aHurt, double aNow, double aTickMs);
	// The sum, once its ticks are up.
	std::optional<Hurt> Due(double aNow, double aTickMs);

	bool     Waiting() const { return sum_.has_value(); }
	uint64_t Summed() const { return summed_; }  // hurts that joined a sum, since the start

private:
	struct Sum
	{
		Hurt   hurt;
		double biggest = 0.0;  // its biggest part's share
	};

	std::optional<double> lastSent_;
	std::optional<Sum>    sum_;
	uint64_t              summed_ = 0;
};

// Whether Minecraft's player's death kills V: with combat on, while build mode is on, or within kDeathWindow seconds of
// its end (aBuildEndedAt; build mode ends as soon as McState says Minecraft's player is dead, which can come first).
bool DeathKillsV(bool aCombat, bool aBuildMode, std::optional<double> aBuildEndedAt, double aNow);

// V's god mode, Immortal under BlockRender's own source name: on while combat and build mode are both on, whichever came
// first; off when either ends; and off once whenever V is found again after not being found, as after a load, in case a
// save kept it.
class GodMode
{
public:
	// The frame's state. The change to make now, through the module: on (true) or off (false). Nothing while V isn't found.
	std::optional<bool> Frame(bool aCombat, bool aBuildMode, bool aVFound);
	bool                On() const { return on_; }

private:
	bool on_ = false;
	bool vFound_ = false;
	bool clearDue_ = false;
};
}
