#pragma once

#include "Coords.h"
#include "link/Rings.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Night City's NPCs as SkyCraft's actor records (spike 6's "Part 1"): BlockRender numbers the NPCs the redscript module
// lists, and writes the nearest into SkyCraft's actor table, where SkyCraft's mod gives each an invisible stand-in that
// Minecraft's weapons hit and that presses pressure plates. Pure: the tests give it NPCs of their own.
namespace cpmc::link
{
// One NPC as the module lists it, in the game's world axes (metres, z up).
struct NpcSample
{
	uint64_t    entity = 0;  // its EntityID, all 64 bits: EntityID.GetHash gives only 32
	Vec3d       position;    // GetWorldPosition: its feet, as V's is
	Vec3f       forward;     // GetWorldForward
	float       health = 0.0f;  // its health pool, now and at most
	float       healthMax = 0.0f;
	float       level = 0.0f;  // its PowerLevel
	std::string name;          // GetDisplayName, UTF-8
	uint32_t    flags = 0;     // proto::ActorFlags: hostile, essential, in combat
};

// An NPC's box, from its capsule (physx/Characters): its width, and how far it reaches below and above the NPC's
// position, which moves with it.
struct NpcBox
{
	double width = 0.6;
	double below = 0.0;
	double above = 1.8;
};

// The box of an NPC without a capsule: the stand-in's own default size, 0.6 by 1.8 (SkyCraft's sized(0.6F, 1.8F)).
constexpr NpcBox kPersonBox{};

constexpr double kActorRange = 80.0;   // metres, straight-line: SkyCraft's 80 blocks
constexpr double kActorForget = 60.0;  // seconds unlisted before an NPC's number is forgotten
constexpr double kHitTrail = 2.0;      // seconds a hit may trail its stand-in's removal

// Numbers NPCs from 1 as each is first listed, keyed by entity, and never gives the same number twice in a run.
class ActorIds
{
public:
	// aEntity's number, given now if it has none; aNow (seconds) marks it listed.
	uint32_t Id(uint64_t aEntity, double aNow);
	// The entity behind aId, if it was listed within the last kHitTrail seconds.
	std::optional<uint64_t> Entity(uint32_t aId, double aNow) const;
	// Forgets the NPCs unlisted for kActorForget seconds: listed again, they get new numbers.
	void Forget(double aNow);

	size_t   Known() const { return byEntity_.size(); }
	uint32_t Given() const { return next_ - 1; }  // numbers given since the start

private:
	struct Entry
	{
		uint32_t id = 0;
		double   listed = 0.0;
	};

	std::unordered_map<uint64_t, Entry>    byEntity_;
	std::unordered_map<uint32_t, uint64_t> byId_;
	uint32_t                               next_ = 1;
};

// aName cut to at most aBytes bytes, at the start of a UTF-8 character.
std::string CutName(std::string_view aName, size_t aBytes);

// An NPC's record: its feet in Minecraft's axes, aBox's reach below its position under it; its yaw from its forward
// vector; aBox's size; its health over its max, or 0 when the max isn't over 0; its level, rounded and held to 0 to
// 65,535; its name cut to 23 bytes; its hostile, essential and in-combat flags.
proto::ActorRecord MakeActorRecord(const NpcSample& aNpc, uint32_t aId, const NpcBox& aBox);

// A hit's weapon class and flags, as SkyCraft's SkyrimActorEntity sets them: "blade, critical, sweep".
std::string HitWords(const proto::McEvent& aEvent);

// The records of the nearest proto::kMaxActors NPCs within kActorRange of aV (world axes), nearest first, numbered by
// aIds. aBoxes holds the boxes found so far, by entity; the other NPCs are person-sized.
std::vector<proto::ActorRecord> ActorRecords(std::span<const NpcSample> aNpcs, const Vec3d& aV,
                                             const std::unordered_map<uint64_t, NpcBox>& aBoxes, ActorIds& aIds, double aNow);
}
