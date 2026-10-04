#pragma once

#include "Coords.h"
#include "link/Ground.h"
#include "physx/GameAbi.h"
#include "physx/PxHeaders.h"
#include "physx/Session.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Night City's shape for Minecraft, from the game's PhysX (spike 5's "The exporter"): scene 0's static scene-query
// shapes, which V's controller sweeps against, read through spike 1's checks and turned into Minecraft's triangles.
// PhysxGround is the link's GroundSource in the game. Main thread only.
namespace cpmc::px
{
// Whether the exporter keeps a shape: a static actor's scene-query shape, and not a trigger or a plane, nor one of
// BlockRender's own bodies (aOurs), so Minecraft never gets its own blocks back as the city's shape. A shape that's only a
// simulation shape doesn't stop V's controller, and dynamic and kinematic actors are cars, litter, bodies and controllers.
bool KeptShape(AuditedClass aActor, physx::PxShapeFlags aFlags, physx::PxGeometryType::Enum aGeometry, bool aOurs);

// BlockRender's own actors, part 3's bodies, which the exporter leaves out.
class OwnActors
{
public:
	void   Set(std::vector<const void*> aActors);  // all of them, as they are now
	bool   Has(const void* aActor) const;
	size_t Size() const { return sorted_.size(); }

private:
	std::vector<const void*> sorted_;
};

// A point from PhysX's frame into Minecraft's axes: world = PhysX + aOrigin (PhysX's origin, from V's controller).
Vec3d McFromPhysx(const physx::PxVec3& aPoint, const Vec3d& aOrigin);
// A box in Minecraft's axes, as PhysX's frame has it: world - aOrigin.
physx::PxBounds3 PhysxBox(const link::McBox& aBox, const Vec3d& aOrigin);

// "box", "triangle mesh" and the rest, and a filter's words as "w0:w1:w2:w3" in hexadecimal: for the shapes request.
const char* GeometryName(physx::PxGeometryType::Enum aGeometry);
std::string FilterKey(const physx::PxFilterData& aData);

class PhysxGround : public link::GroundSource
{
public:
	explicit PhysxGround(Session& aSession) : session_(aSession) {}

	// Once a frame, before the link reads: V's feet (world axes), for V's controller, which gives PhysX's origin. Nothing:
	// V isn't found, and nothing is read.
	void Frame(const std::optional<Vec3d>& aFeet);

	// The kept shapes touching aBox, as triangles in Minecraft's axes, read under scene 0's read lock. False if scene 0
	// or PhysX's origin can't be had this frame.
	bool Read(const link::McBox& aBox, std::vector<link::McTriangle>& aOut) override;

	// Every shape within aRadius of aWorld (world axes), static or dynamic, described, for the shapes request; or why
	// there are none.
	std::vector<std::string> Describe(const Vec3d& aWorld, double aRadius);

	// Scene 0 and PhysX's origin this frame, as the reads have them, for part 3's bodies: read once a frame, whoever asks
	// first. Null if they can't be had, with State().why saying why.
	physx::PxScene* Prepared(Vec3d& aOrigin);

	struct Report
	{
		std::optional<Vec3d> origin;  // PhysX's, the last frame it was read
		std::string          why;     // why nothing could be read, the last time that happened
		uint64_t             searches = 0;    // times V's controller was looked for among all of scene 0's capsules
		uint64_t             kept = 0;        // shapes read, since the start
		uint64_t             left = 0;        // shapes the overlap gave that aren't kept: simulation-only, triggers, planes
		uint64_t             unreadable = 0;  // objects that failed their checks, and meshes that couldn't be read
		uint64_t             triangles = 0;   // triangles read
	};

	const Report& State() const { return report_; }
	OwnActors&    Own() { return own_; }  // BlockRender's own, left out of what's read

	// Diagnostics for step 3: a line for each batch read that took 3 ms or more, saying where its time went, for the
	// plugin's log. At most 120 lines in all.
	std::vector<std::string> TakeSlowBatches() { return std::exchange(slow_, {}); }

private:
	// Scene 0 and PhysX's origin, once a frame, the first time they're needed.
	bool Prepare();
	// V's controller (OriginWatch.h): the last one found, while it's still among scene 0's dynamic actors and fits under
	// V; otherwise the kinematic capsule most nearly under V. PhysX's origin from it, or nothing.
	std::optional<Vec3d> Origin(physx::PxScene& aScene, const Vec3d& aFeet);

	Session&              session_;
	std::optional<Vec3d>  feet_;
	bool                  prepared_ = false;
	physx::PxScene*       scene_ = nullptr;
	const physx::PxActor*    controller_ = nullptr;
	OwnActors                own_;
	Report                   report_;
	std::vector<std::string> slow_;
	uint32_t                 slowLogged_ = 0;
};
}
