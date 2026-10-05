#pragma once

#include "Coords.h"
#include "link/Actors.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

// NPCs' sizes from their capsules (spike 6's "Sizes"): an NPC's character controller is, as V's is, a kinematic capsule
// standing around it, which PhysxGround::Capsules reads through the exporter's checks. Each NPC's is looked for when
// it's first listed, and again every 2 s until it's found, in case the game gives an NPC its capsule only near V, or a
// little after it appears. Pure: the tests give it capsules of their own.
namespace cpmc::px
{
constexpr double kCapsuleSearchRadius = 1.0;  // the overlap: a sphere of this radius,
constexpr double kCapsuleSearchHeight = 1.0;  // centred this far above the NPC's feet
constexpr double kCapsuleAcross = 0.2;        // how far an NPC's capsule's axis may pass from its feet
constexpr double kCapsuleTilt = 25.0;         // and how far it may lean from upright, in degrees
constexpr size_t kCapsulesPerFrame = 8;       // NPCs looked for in one frame
// A character controller's bit, in its capsule's query filter's third word: V's is 0:0:2000:0, and step 2's vendor's
// 0:0:10002000:0, while the vendor's other capsules, radius 0.40 and 0.06, and V's radius-1 one, lack it.
constexpr uint32_t kControllerQueryBit = 0x2000;
constexpr double kCapsuleRetry = 2.0;         // seconds before an NPC whose capsule wasn't found is looked for again

// A kinematic actor's capsule, in the world's axes.
struct CapsuleSeen
{
	Vec3d                   centre;
	Vec3d                   axis{ 0.0, 0.0, 1.0 };  // along its length: the shape's x axis, posed
	double                  radius = 0.0;
	double                  halfHeight = 0.0;  // half the length between its caps' centres
	std::array<uint32_t, 4> queryFilter{};     // its shape's, for the npcs request
};

// The NPC's own among aSeen, for an NPC whose feet are at aFeet: a character controller's, with kControllerQueryBit in
// its query filter, leaning at most kCapsuleTilt from upright, whose axis passes within kCapsuleAcross of the feet; of
// several, the narrowest. Nothing if none is.
std::optional<CapsuleSeen> PickCapsule(std::span<const CapsuleSeen> aSeen, const Vec3d& aFeet);

// Its box: twice the radius wide, reaching from the lower of the feet and the capsule's bottom up to its top.
link::NpcBox BoxOf(const CapsuleSeen& aCapsule, const Vec3d& aFeet);

// Which NPCs' capsules to look for, and the boxes found.
class NpcSizes
{
public:
	// Of aListed, the entities listed this frame, nearest first, at most kCapsulesPerFrame to look for at aNow (seconds):
	// those never looked for, then those whose capsule wasn't found when last looked for, kCapsuleRetry or more ago.
	// Each counts as looked for at aNow.
	std::vector<uint64_t> ToLookFor(std::span<const uint64_t> aListed, double aNow);
	// aEntity's capsule, found with its feet at aFeet: its box from now on.
	void Found(uint64_t aEntity, const CapsuleSeen& aCapsule, const Vec3d& aFeet);
	// Forgets the NPCs aIds has forgotten, so one listed again is looked for again.
	void Forget(const link::ActorIds& aIds);

	const std::unordered_map<uint64_t, link::NpcBox>& Boxes() const { return boxes_; }
	// The capsule aEntity's box came from; null if it has none.
	const CapsuleSeen* Capsule(uint64_t aEntity) const;
	bool               LookedFor(uint64_t aEntity) const { return looked_.contains(aEntity); }
	size_t             LookedForCount() const { return looked_.size(); }
	size_t             FoundCount() const { return capsules_.size(); }

private:
	std::unordered_map<uint64_t, double>       looked_;  // when each was last looked for
	std::unordered_map<uint64_t, link::NpcBox> boxes_;
	std::unordered_map<uint64_t, CapsuleSeen>  capsules_;
};

// An NPC's line for the npcs request: its number, name and distance (metres); its box and where it came from; its health,
// level and flags.
std::string DescribeNpc(const link::NpcSample& aNpc, uint32_t aId, double aDistance, const NpcSizes& aSizes);
}
