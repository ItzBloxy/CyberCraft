#include "physx/Bodies.h"

#include "physx/GameAbi.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>

using namespace physx;

namespace cpmc::px
{
namespace
{
constexpr size_t kActorBytes = 24;         // the vtable pointer, the type ID and base flags, and userData
constexpr PxU32  kMostActors = 1'000'000;  // a scene listing more is garbage
constexpr PxU32  kRayHits = 16;            // the most hits the ray down looks at
constexpr double kRayAbove = 0.5;          // metres above V's feet the ray down starts
constexpr float  kRayLength = 2.0f;        // and how far it goes

// A point in PhysX's frame: world - origin.
PxTransform At(const Vec3d& aWorld, const Vec3d& aOrigin)
{
	return PxTransform(PxVec3(float(aWorld.x - aOrigin.x), float(aWorld.y - aOrigin.y), float(aWorld.z - aOrigin.z)));
}

bool Is(const ObjectCheck& aCheck, AuditedClass aClass)
{
	return aCheck.verdict == ObjectVerdict::Ok && aCheck.cls == aClass;
}

bool SameOrigin(const Vec3d& aA, const Vec3d& aB)
{
	return aA.x == aB.x && aA.y == aB.y && aA.z == aB.z;  // whole numbers of the 1,024 m grid
}
}

void Bodies::Frame(const std::optional<Vec3d>& aFeet, bool aOn, const link::Solids& aSolids)
{
	scene_ = nullptr;
	statics_.reset();
	if (!aFeet)
	{
		lost_ = lost_ || box_ || !bodies_.empty();  // nothing is touched while V isn't found, as in a load
		return;
	}
	if (lost_)
	{
		if (!Prepare())
		{
			return;  // tried again next frame
		}
		Check();
		lost_ = false;
	}
	std::vector<BodyPlan::Change> changes;
	if (const std::optional<SectionPos> v = SectionAt(McFromWorld(*aFeet)))
	{
		changes = plan_.Changes(*v, aOn, aSolids);  // with bodies off, every one is removed
	}
	// PhysX's origin, as the exporter last read it: if it has moved, the actors' poses are checked.
	const std::optional<Vec3d>& seen = ground_.State().origin;
	const bool                  moved = posedBy_ && seen && !SameOrigin(*seen, *posedBy_);
	if (changes.empty() && !moved)
	{
		report_.why.clear();  // nothing waits
		return;
	}
	if (!Prepare())
	{
		return;  // the plan still has the changes: tried again next frame
	}
	report_.why.clear();
	if (posedBy_ && !SameOrigin(origin_, *posedBy_))
	{
		Repose();
	}
	for (const BodyPlan::Change& change : changes)
	{
		if (!change.make)
		{
			if (const auto body = bodies_.find(change.section); body != bodies_.end())
			{
				RemoveActor(body->second);
				bodies_.erase(body);
			}
			plan_.Removed(change.section);
			continue;
		}
		const link::Solids::Section* solids = aSolids.Find(change.section);
		if (!solids)
		{
			continue;
		}
		if (!copy_ && !CopyGround(*aFeet))
		{
			break;  // the bodies wait for V to stand on the city: the plan still has them
		}
		std::vector<LocalBox> boxes;
		for (const BlockBox& box : MergeBoxes(solids->bits))
		{
			boxes.push_back(LocalBoxOf(box));
		}
		const Vec3d                 world = SectionCentre(change.section);
		const auto                  start = std::chrono::steady_clock::now();
		PxRigidStatic* const        actor = MakeActor(world, boxes);
		report_.slowestMs = std::max(report_.slowestMs, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
		if (!actor)
		{
			plan_.Failed(change.section, change.version);  // tried again when the section's solids next change
			continue;
		}
		bodies_[change.section] = { actor, world, boxes.size(), boxes.front().half };
		plan_.Made(change.section, change.version);
	}
	posedBy_ = box_ || !bodies_.empty() ? std::optional<Vec3d>(origin_) : std::nullopt;
	Publish();
}

std::string Bodies::Box(bool aOn, const Vec3d& aFeet, const Vec3f& aForward)
{
	scene_ = nullptr;  // a request comes before this frame's Frame
	statics_.reset();
	if (!aOn && !box_)
	{
		return "box off: there's no box";
	}
	if (aOn && box_)
	{
		return "box on: the box is there already";
	}
	if (!Prepare())
	{
		return std::format("box {}: {}", aOn ? "on" : "off", report_.why);
	}
	if (lost_)
	{
		Check();
		lost_ = false;
	}
	if (posedBy_ && !SameOrigin(origin_, *posedBy_))
	{
		Repose();  // before the bodies count as posed by this origin
	}
	if (!aOn)
	{
		if (box_)
		{
			RemoveActor(*box_);
			box_.reset();
		}
		posedBy_ = bodies_.empty() ? std::nullopt : std::optional<Vec3d>(origin_);
		Publish();
		return "box off: the box is gone";
	}
	if (!copy_ && !CopyGround(aFeet))
	{
		return "box on: " + report_.why;
	}
	// 3 m ahead of V, along the ground: V's facing, or north if V faces straight up or down.
	const double                length = std::hypot(aForward.x, aForward.y);
	const Vec3d                 ahead = length > 0.1 ? Vec3d{ aForward.x / length, aForward.y / length, 0.0 } : Vec3d{ 0.0, 1.0, 0.0 };
	const Vec3d                 centre{ aFeet.x + 3.0 * ahead.x, aFeet.y + 3.0 * ahead.y, aFeet.z + 0.5 };
	const std::vector<LocalBox> boxes = { { { 0.0, 0.0, 0.0 }, { 1.0, 1.0, 0.5 } } };
	PxRigidStatic* const        actor = MakeActor(centre, boxes);
	if (!actor)
	{
		return "box on: PhysX made no actor";
	}
	box_ = Body{ actor, centre, 1, boxes.front().half };
	posedBy_ = origin_;
	Publish();
	return std::format("box on: 2 x 2 x 1 m, centred at world {:.2f} {:.2f} {:.2f}, made of {}", centre.x, centre.y, centre.z,
	                   report_.madeOf);
}

bool Bodies::Prepare()
{
	if (scene_)
	{
		return true;
	}
	Vec3d          origin;
	PxScene* const scene = ground_.Prepared(origin);
	if (!scene)
	{
		report_.why = ground_.State().why;
		return false;
	}
	PxPhysics* const physics = session_.Physics(report_.why);
	if (!physics)
	{
		return false;
	}
	if (addedTo_ && scene != addedTo_)
	{
		Forget();  // the scene our actors went into isn't scene 0 now: a load took it
	}
	scene_ = scene;
	physics_ = physics;
	origin_ = origin;
	return true;
}

bool Bodies::CopyGround(const Vec3d& aFeet)
{
	// The ray down from just above V's feet, through every static shape: the nearest one that stops V, a scene-query and
	// simulation shape that isn't a trigger, and isn't ours, is what the bodies are made of.
	ReadableMemory&    memory = session_.Memory();
	const ModuleBases& bases = session_.Bases();
	PxRaycastHit       hits[kRayHits];
	PxRaycastBuffer    buffer(hits, kRayHits);
	scene_->lockRead();
	scene_->raycast(At({ aFeet.x, aFeet.y, aFeet.z + kRayAbove }, origin_).p, PxVec3(0.0f, 0.0f, -1.0f), kRayLength, buffer,
	                PxHitFlag::eDEFAULT, PxQueryFilterData(PxQueryFlag::eSTATIC | PxQueryFlag::eNO_BLOCK));
	const PxRaycastHit* ground = nullptr;
	for (PxU32 i = 0; i < std::min(buffer.nbTouches, kRayHits); ++i)
	{
		const PxRaycastHit& hit = hits[i];
		if (!Is(CheckObject(memory, bases, hit.actor, kActorBytes), AuditedClass::RigidStatic) ||
		    !Is(CheckObject(memory, bases, hit.shape), AuditedClass::Shape) || ground_.Own().Has(hit.actor))
		{
			continue;
		}
		const PxShapeFlags flags = hit.shape->getFlags();
		if (flags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE) && flags.isSet(PxShapeFlag::eSIMULATION_SHAPE) &&
		    !flags.isSet(PxShapeFlag::eTRIGGER_SHAPE) && (!ground || hit.distance < ground->distance))
		{
			ground = &hit;
		}
	}
	Copy        copy;
	std::string kind;
	if (ground)
	{
		copy.query = ground->shape->getQueryFilterData();
		copy.simulation = ground->shape->getSimulationFilterData();
		copy.flags = ground->shape->getFlags();
		kind = GeometryName(ground->shape->getGeometryType());
		if (ground->shape->getMaterials(&copy.material, 1) != 1 || !memory.Range(copy.material, 16))
		{
			copy.material = nullptr;
		}
	}
	scene_->unlockRead();
	if (!copy.material)
	{
		report_.why = ground ? "the ground under V has no material to copy" : "they wait for V to stand on the city, to be made of what it's made of";
		return false;
	}
	// A shape of ours, never attached, keeps the material alive: PhysX takes a reference on each material a shape has.
	copy.keeper = physics_->createShape(PxBoxGeometry(0.5f, 0.5f, 0.5f), &copy.material, 1, true, copy.flags);
	if (!Is(CheckObject(memory, bases, copy.keeper), AuditedClass::Shape))
	{
		report_.why = "PhysX made no shape of the ground's material";
		return false;
	}
	copy_ = copy;
	report_.madeOf = std::format("a static {}'s: query filter {}, simulation filter {}, flags 0x{:x}", kind, FilterKey(copy.query),
	                             FilterKey(copy.simulation), uint32_t(PxU8(copy.flags)));
	return true;
}

PxRigidStatic* Bodies::MakeActor(const Vec3d& aWorld, const std::vector<LocalBox>& aBoxes)
{
	ReadableMemory&      memory = session_.Memory();
	const ModuleBases&   bases = session_.Bases();
	PxRigidStatic* const actor = physics_->createRigidStatic(At(aWorld, origin_));
	if (!Is(CheckObject(memory, bases, actor, kActorBytes), AuditedClass::RigidStatic))
	{
		++report_.failed;  // what isn't the audited actor is left alone
		return nullptr;
	}
	for (const LocalBox& box : aBoxes)
	{
		PxShape* const shape = physics_->createShape(PxBoxGeometry(float(box.half.x), float(box.half.y), float(box.half.z)), &copy_->material,
		                                             1, true, copy_->flags);
		if (!Is(CheckObject(memory, bases, shape), AuditedClass::Shape))
		{
			actor->release();  // with the shapes attached so far
			++report_.failed;
			return nullptr;
		}
		shape->setLocalPose(PxTransform(PxVec3(float(box.centre.x), float(box.centre.y), float(box.centre.z))));
		shape->setQueryFilterData(copy_->query);
		shape->setSimulationFilterData(copy_->simulation);
		actor->attachShape(*shape);
		shape->release();  // the actor holds it now
	}
	scene_->addActor(*actor);
	addedTo_ = scene_;
	if (statics_)
	{
		statics_->insert(std::upper_bound(statics_->begin(), statics_->end(), static_cast<const void*>(actor)), actor);
	}
	++report_.made;
	return actor;
}

bool Bodies::Ours(const Body& aBody)
{
	if (!statics_)
	{
		// Scene 0's static actors, listed once a frame when needed: one of ours that isn't among them was taken away.
		scene_->lockRead();
		std::vector<PxActor*> actors(std::min(scene_->getNbActors(PxActorTypeFlag::eRIGID_STATIC), kMostActors));
		actors.resize(scene_->getActors(PxActorTypeFlag::eRIGID_STATIC, actors.data(), PxU32(actors.size())));
		scene_->unlockRead();
		statics_.emplace(actors.begin(), actors.end());
		std::sort(statics_->begin(), statics_->end());
	}
	ReadableMemory&            memory = session_.Memory();
	const ModuleBases&         bases = session_.Bases();
	const PxRigidStatic* const actor = aBody.actor;
	if (!std::binary_search(statics_->begin(), statics_->end(), static_cast<const void*>(actor)) ||
	    !Is(CheckObject(memory, bases, actor, kActorBytes), AuditedClass::RigidStatic) || actor->getScene() != scene_ ||
	    actor->getNbShapes() != aBody.boxes || !copy_)
	{
		return false;
	}
	// PhysX reuses a released actor's memory, so an actor of the game's may stand where ours stood: ours still has our
	// first box, filtered as we filtered it.
	PxShape*      shape = nullptr;
	PxBoxGeometry box;
	return actor->getShapes(&shape, 1) == 1 && Is(CheckObject(memory, bases, shape), AuditedClass::Shape) && shape->getBoxGeometry(box) &&
	       std::abs(box.halfExtents.x - float(aBody.firstHalf.x)) < 1e-3f && std::abs(box.halfExtents.y - float(aBody.firstHalf.y)) < 1e-3f &&
	       std::abs(box.halfExtents.z - float(aBody.firstHalf.z)) < 1e-3f && shape->getQueryFilterData() == copy_->query;
}

void Bodies::RemoveActor(const Body& aBody)
{
	if (!Ours(aBody))
	{
		++report_.lost;  // scene 0 no longer lists it, or it isn't ours now: forgotten, not touched
		return;
	}
	scene_->removeActor(*aBody.actor);
	aBody.actor->release();
	statics_->erase(std::lower_bound(statics_->begin(), statics_->end(), static_cast<const void*>(aBody.actor)));
	++report_.removed;
}

void Bodies::Forget()
{
	report_.lost += bodies_.size() + (box_ ? 1 : 0);
	bodies_.clear();
	box_.reset();
	plan_.Clear();
	addedTo_ = nullptr;
	posedBy_.reset();
	Publish();
}

void Bodies::Check()
{
	for (auto body = bodies_.begin(); body != bodies_.end();)
	{
		if (Ours(body->second))
		{
			++body;
			continue;
		}
		++report_.lost;
		plan_.Removed(body->first);  // made again, if it's still wanted
		body = bodies_.erase(body);
	}
	if (box_ && !Ours(*box_))
	{
		++report_.lost;
		box_.reset();
	}
	Publish();
}

void Bodies::Repose()
{
	// The game shifts all of scene 0 when PhysX's origin moves (step 3's scene watch), ours with it: each actor should
	// already stand where it was. One that doesn't is posed again.
	++report_.originChanges;
	const auto check = [&](const Body& aBody) {
		if (!Ours(aBody))
		{
			return false;
		}
		const PxTransform pose = aBody.actor->getGlobalPose();
		if (!PosedAt({ pose.p.x, pose.p.y, pose.p.z }, aBody.world, origin_))
		{
			aBody.actor->setGlobalPose(At(aBody.world, origin_));
			++report_.reposed;
		}
		return true;
	};
	for (auto body = bodies_.begin(); body != bodies_.end();)
	{
		if (check(body->second))
		{
			++body;
			continue;
		}
		++report_.lost;
		plan_.Removed(body->first);
		body = bodies_.erase(body);
	}
	if (box_ && !check(*box_))
	{
		++report_.lost;
		box_.reset();
	}
	posedBy_ = origin_;
}

void Bodies::Publish()
{
	std::vector<const void*> actors;
	report_.boxes = 0;
	for (const auto& [section, body] : bodies_)
	{
		actors.push_back(body.actor);
		report_.boxes += body.boxes;
	}
	if (box_)
	{
		actors.push_back(box_->actor);
	}
	report_.sections = bodies_.size();
	report_.box = box_ ? std::optional<Vec3d>(box_->world) : std::nullopt;
	ground_.Own().Set(std::move(actors));
}
}
