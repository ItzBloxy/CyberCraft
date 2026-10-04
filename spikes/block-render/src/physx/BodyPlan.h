#pragma once

#include "Coords.h"
#include "link/Solids.h"
#include "physx/BoxMerge.h"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

// Where V's bodies go, and when they change (spike 5's "Bodies"): a body for each section with solid blocks among the
// 3 x 3 x 3 around V's, made when the section comes within one of V's and removed once it's more than two away, so
// walking along a boundary doesn't churn bodies, and rebuilt when the section's solids change; and none while bodies are
// off. And where a body's boxes stand in the world. Pure: physx/Bodies makes the actors, and says what it did.
namespace cpmc::px
{
// The section holding a point in Minecraft's axes. Nothing for a point that isn't finite, or is over a million blocks out.
std::optional<SectionPos> SectionAt(const Vec3d& aMc);

// A section's centre, in the world's axes (metres: x east, y north, z up): where its body stands.
Vec3d SectionCentre(const SectionPos& aSection);

// A box of a section's blocks in the world's axes: its centre, from the section's, and its half extents.
struct LocalBox
{
	Vec3d centre;
	Vec3d half;
};

LocalBox LocalBoxOf(const BlockBox& aBox);

// Whether an actor PhysX has at aPhysx, in PhysX's frame, stands at aWorld, given PhysX's origin: within a centimetre.
bool PosedAt(const Vec3d& aPhysx, const Vec3d& aWorld, const Vec3d& aOrigin);

class BodyPlan
{
public:
	struct Change
	{
		bool       make = false;  // false: the section's body is removed
		SectionPos section;
		uint64_t   version = 0;  // a making's: the version of the section's solids it's made from
	};

	// With V in section aV and bodies on (aOn): the bodies to remove, more than two sections away or with their solids
	// gone or changed; then those to make, within one section of V's, with solid blocks and no body, a changed one again.
	// With bodies off, every body is removed. Changes nothing itself: Bodies says what it did.
	std::vector<Change> Changes(const SectionPos& aV, bool aOn, const link::Solids& aSolids) const;

	void Made(const SectionPos& aSection, uint64_t aVersion);
	void Failed(const SectionPos& aSection, uint64_t aVersion);  // not tried again until the section's solids change
	void Removed(const SectionPos& aSection);                    // or forgotten, its actor lost
	void Clear();                                                // every body removed or forgotten

private:
	struct Entry
	{
		uint64_t version = 0;
		bool     made = false;  // false: it failed
	};

	std::map<SectionPos, Entry> entries_;
};
}
