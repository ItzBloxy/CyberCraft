#pragma once

#include "link/Actors.h"
#include "link/Rings.h"

#include <cstdint>
#include <optional>
#include <string>

// Combat both ways (spike 6's "Part 2"), the parts that don't need the game: which of Minecraft's hits on NPCs count, what
// each starts at, and what the game's damage pipeline made of it; V's hurts as SkyCraft's kInHurt, summed so that
// Minecraft takes them all; when Minecraft's death kills V; and when V's god mode, V's unarmed weapon and V's hidden
// meshes go on and off. Pure: the tests give it hits and hurts of their own.
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
	double   amount = 0.0;  // damage x V's punch: where the game's damage pipeline starts it
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

// What the game's damage pipeline did to one of our hits, as the module's DealDamages wrap reports it when it lands.
enum LandedFlag : uint32_t
{
	kLandedCritical = 1,    // V's crit, rolled by the game (hitFlag.CriticalHit)
	kLandedBlocked = 2,     // the NPC blocked it (WasBlocked)
	kLandedParried = 4,     // the NPC parried it (WasDeflected)
	kLandedArmour = 8,      // the NPC's armour took some, of a hit that isn't critical (the hit type Glance)
	kLandedMitigated = 16,  // the NPC's mitigation took some (WasMitigated)
	kLandedKilled = 32,     // it killed the NPC (WasKillingBlow)
};

// The flags in words, "critical, armour"; empty for none. Bits it doesn't know are left out.
std::string LandedWords(uint32_t aFlags);

// Why the game's damage pipeline dealt one of our hits nothing, as the module's wrap of ProcessPipeline reports it, from
// the source name of the hit's DealNoDamage flag: 1 friendly to V ("FriendlyFireIgnored"), 2 invulnerable, 3 already dead,
// 4 dodged ("ProcessDodge"), 5 evaded ("ProcessEvasion"), 6 killed at once ("instant_kill"), 7 no damage left
// ("no_valid_damage"); 0 another of the game's reasons. "dodged"; "reason 9" for one it doesn't know.
std::string HitEndedName(int32_t aWhy);

// A hit on V that the module's DealDamages wrap took, in the game's terms: the health it would have taken, after V's
// armour, the difficulty and V's overshield; its gamedataAttackType, as a number; its instigator's entity, or 0; and
// whether the game won't let it kill V (hitFlag.CannotKillPlayer, or a minimum health left), so it stays V's.
struct GameHurt
{
	double   amount = 0.0;
	int32_t  attackType = 0;
	uint64_t instigator = 0;
	bool     nonLethal = false;
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

// One of V's states that BlockRender switches through the module while it's wanted: V's unarmed weapon drawn (the
// module's Arm), and V's meshes hidden (HideV). Wanted, it's switched on, and on again every aRefresh seconds while it
// stays wanted, so that what appears meanwhile, such as the weapon once it's drawn, is caught too; not wanted, off, once.
// Losing V, as during a load, counts as off with nothing to switch back: the next session's V starts as its save left it.
class VSwitch
{
public:
	explicit VSwitch(double aRefresh = 0.0) : refresh_(aRefresh) {}

	// The frame's state at aNow (seconds). The call to make now, through the module: on (true) or off (false). Nothing
	// while V isn't found.
	std::optional<bool> Frame(bool aWanted, bool aVFound, double aNow);
	bool                On() const { return on_; }

private:
	double refresh_ = 0.0;  // seconds between calls while on; 0: once
	bool   on_ = false;
	double onAt_ = 0.0;     // when on was last called
};
}
