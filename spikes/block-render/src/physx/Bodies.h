#pragma once

#include "Coords.h"
#include "link/Solids.h"
#include "physx/BodyPlan.h"
#include "physx/Export.h"
#include "physx/PxHeaders.h"
#include "physx/Session.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

// V against Minecraft's blocks (spike 5's "Part 3"): a PhysX static actor for each section with solid blocks around V,
// its blocks merged into boxes, as BodyPlan says, each box a shape made of whatever V stood on; and step 4's test box.
// They're added and removed in OnUpdate, on the main thread, when step 3's scene watch found no other thread calling
// scene 0. Every PhysX object is checked against the audit before it's used, ours among them, and an actor of ours is
// touched only while scene 0 is the scene it went into and still lists it, with our shapes: a load may take either away,
// and PhysX may give a released actor's memory to one of the game's. Nothing is touched while V isn't found, or at
// unload. Main thread only.
namespace cpmc::px
{
class Bodies
{
public:
	Bodies(Session& aSession, PhysxGround& aGround) : session_(aSession), ground_(aGround) {}

	// Once a frame, after the ground's Frame: V's feet (world axes; nothing while V isn't found, as in a load), whether
	// bodies are on (set blocks 1, and drawing on, so blocks nobody can see never stop V), and Minecraft's solid blocks.
	void Frame(const std::optional<Vec3d>& aFeet, bool aOn, const link::Solids& aSolids);

	// box on|off (step 4's opening check): a box 2 m wide and deep and 1 m tall, 3 m ahead of V along aForward, standing
	// on V's feet's level, made as the bodies are. The ground's Frame must have had V's feet this frame. Says what
	// happened, for the request's report.
	std::string Box(bool aOn, const Vec3d& aFeet, const Vec3f& aForward);

	struct Report
	{
		size_t               sections = 0;       // with bodies
		size_t               boxes = 0;          // their shapes
		uint64_t             made = 0;           // bodies, the box among them, since the start
		uint64_t             removed = 0;
		uint64_t             failed = 0;         // that PhysX didn't make
		uint64_t             lost = 0;           // forgotten without being touched: scene 0 changed, or stopped listing them
		double               slowestMs = 0.0;    // the slowest making, since the start
		std::string          madeOf;             // what they're made of, once copied from what V stood on
		uint64_t             originChanges = 0;  // seen while actors of ours stood
		uint64_t             reposed = 0;        // actors posed again after one, which the game hadn't moved with its scene
		std::optional<Vec3d> box;                // the test box's centre, world axes, while it stands
		std::string          why;                // why bodies wait, while they do
	};

	const Report& State() const { return report_; }

private:
	struct Body
	{
		physx::PxRigidStatic* actor = nullptr;
		Vec3d                 world;  // where it stands, world axes
		size_t                boxes = 0;
		Vec3d                 firstHalf;  // its first box's half extents: with the count, how it's told from another actor
	};

	// What V stood on, copied once for every body: its filters, flags and first material, which a shape of ours, never
	// attached, keeps alive.
	struct Copy
	{
		physx::PxFilterData query;
		physx::PxFilterData simulation;
		physx::PxShapeFlags flags;
		physx::PxMaterial*  material = nullptr;
		physx::PxShape*     keeper = nullptr;
	};

	bool                  Prepare();  // scene 0, PhysX's origin and PxPhysics, this frame: false, with report_.why, if not
	bool                  CopyGround(const Vec3d& aFeet);
	physx::PxRigidStatic* MakeActor(const Vec3d& aWorld, const std::vector<LocalBox>& aBoxes);
	bool                  Ours(const Body& aBody);  // still in scene 0, as it went in, and still the actor we made
	void                  RemoveActor(const Body& aBody);
	void                  Forget();  // every actor of ours, without touching it: its scene is gone
	void                  Check();   // after V was lost: the actors that aren't ours in scene 0 now are forgotten
	void                  Repose();  // after PhysX's origin moved
	void                  Publish();  // which actors are ours, for the exporter, and the report's counts

	Session&                   session_;
	PhysxGround&               ground_;
	BodyPlan                   plan_;
	std::map<SectionPos, Body> bodies_;
	std::optional<Body>        box_;
	std::optional<Copy>        copy_;
	physx::PxScene*            addedTo_ = nullptr;  // the scene our actors went into
	std::optional<Vec3d>       posedBy_;            // PhysX's origin our actors were last posed by, or checked against
	bool                       lost_ = false;       // V was lost while actors of ours stood: check them first
	// This frame's, once prepared
	physx::PxScene*                         scene_ = nullptr;
	physx::PxPhysics*                       physics_ = nullptr;
	Vec3d                                   origin_;
	std::optional<std::vector<const void*>> statics_;  // scene 0's static actors, sorted, once listed
	Report                                  report_;
};
}
