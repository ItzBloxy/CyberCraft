#include "physx/Export.h"

#include "physx/OriginWatch.h"
#include "physx/ShapeTriangles.h"

#include <algorithm>
#include <cmath>
#include <format>

using namespace physx;

namespace cpmc::px
{
namespace
{
constexpr size_t kActorBytes = 24;           // the vtable pointer, the type ID and base flags, and userData
constexpr PxU32  kMostActors = 1'000'000;    // a scene listing more is garbage
constexpr size_t kOverlapBuffer = 4096;      // hits PhysX hands over at a time
constexpr size_t kMostHits = 100'000;        // an overlap stops after this many

// Collects every overlap hit: PhysX hands over each full buffer, and the last partial one, through processTouches.
struct CollectOverlaps final : PxHitCallback<PxOverlapHit>
{
	CollectOverlaps(PxOverlapHit* aBuffer, PxU32 aSize, std::vector<PxOverlapHit>& aOut)
		: PxHitCallback<PxOverlapHit>(aBuffer, aSize)
		, out(aOut)
	{
	}

	PxAgain processTouches(const PxOverlapHit* aHits, PxU32 aCount) override
	{
		out.insert(out.end(), aHits, aHits + aCount);
		return out.size() < kMostHits;
	}

	std::vector<PxOverlapHit>& out;
};

std::vector<PxShape*> ShapesOf(const PxRigidActor& aActor)
{
	std::vector<PxShape*> shapes(aActor.getNbShapes());
	shapes.resize(aActor.getShapes(shapes.data(), PxU32(shapes.size())));
	return shapes;
}

// Every hit of aGeometry at aPose (PhysX's frame) among the actors aWhich asks for, under the scene's read lock, which
// the caller holds.
std::vector<PxOverlapHit> Overlap(PxScene& aScene, const PxGeometry& aGeometry, const PxTransform& aPose, PxQueryFlags aWhich)
{
	std::vector<PxOverlapHit> buffer(kOverlapBuffer);
	std::vector<PxOverlapHit> hits;
	CollectOverlaps           collect(buffer.data(), PxU32(buffer.size()), hits);
	aScene.overlap(aGeometry, aPose, collect, PxQueryFilterData(aWhich | PxQueryFlag::eNO_BLOCK));
	return hits;
}
}

bool KeptShape(AuditedClass aActor, PxShapeFlags aFlags, PxGeometryType::Enum aGeometry)
{
	return aActor == AuditedClass::RigidStatic && aFlags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE) && !aFlags.isSet(PxShapeFlag::eTRIGGER_SHAPE) &&
	       aGeometry != PxGeometryType::ePLANE;
}

Vec3d McFromPhysx(const PxVec3& aPoint, const Vec3d& aOrigin)
{
	return McFromWorld({ aPoint.x + aOrigin.x, aPoint.y + aOrigin.y, aPoint.z + aOrigin.z });
}

PxBounds3 PhysxBox(const link::McBox& aBox, const Vec3d& aOrigin)
{
	// The world's x is Minecraft's x, its y Minecraft's -z, and its z Minecraft's y.
	const Vec3d low = WorldFromMc({ aBox.low.x, aBox.low.y, aBox.high.z });
	const Vec3d high = WorldFromMc({ aBox.high.x, aBox.high.y, aBox.low.z });
	return PxBounds3(PxVec3(float(low.x - aOrigin.x), float(low.y - aOrigin.y), float(low.z - aOrigin.z)),
	                 PxVec3(float(high.x - aOrigin.x), float(high.y - aOrigin.y), float(high.z - aOrigin.z)));
}

const char* GeometryName(PxGeometryType::Enum aGeometry)
{
	switch (aGeometry)
	{
	case PxGeometryType::eSPHERE: return "sphere";
	case PxGeometryType::ePLANE: return "plane";
	case PxGeometryType::eCAPSULE: return "capsule";
	case PxGeometryType::eBOX: return "box";
	case PxGeometryType::eCONVEXMESH: return "convexMesh";
	case PxGeometryType::eTRIANGLEMESH: return "triangleMesh";
	case PxGeometryType::eHEIGHTFIELD: return "heightField";
	default: return "unknown";
	}
}

std::string FilterKey(const PxFilterData& aData)
{
	return std::format("{:x}:{:x}:{:x}:{:x}", aData.word0, aData.word1, aData.word2, aData.word3);
}

void PhysxGround::Frame(const std::optional<Vec3d>& aFeet)
{
	feet_ = aFeet;
	prepared_ = false;
	scene_ = nullptr;
}

bool PhysxGround::Prepare()
{
	if (prepared_)
	{
		return scene_ != nullptr;
	}
	prepared_ = true;
	if (!feet_)
	{
		report_.why = "V isn't found";
		return false;
	}
	PxScene* const scene = session_.Scene0(report_.why);
	if (!scene)
	{
		return false;
	}
	const std::optional<Vec3d> origin = Origin(*scene, *feet_);
	if (!origin)
	{
		report_.why = "V's controller isn't found";
		return false;
	}
	report_.origin = origin;
	scene_ = scene;
	return true;
}

std::optional<Vec3d> PhysxGround::Origin(PxScene& aScene, const Vec3d& aFeet)
{
	ReadableMemory&    memory = session_.Memory();
	const ModuleBases& bases = session_.Bases();
	struct Candidate
	{
		const PxActor* actor = nullptr;
		Vec3d          origin;
		double         across = 0.0;
	};
	std::optional<Candidate> best;
	const auto               measure = [&](const PxActor* aActor) {
		const ObjectCheck check = CheckObject(memory, bases, aActor, kActorBytes);
		if (check.verdict != ObjectVerdict::Ok || check.cls != AuditedClass::RigidDynamic ||
		    !static_cast<const PxRigidBody&>(*aActor).getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC))
		{
			return;
		}
		const auto&       rigid = static_cast<const PxRigidActor&>(*aActor);
		const PxTransform pose = rigid.getGlobalPose();
		for (const PxShape* shape : ShapesOf(rigid))
		{
			const ObjectCheck shapeCheck = CheckObject(memory, bases, shape);
			PxCapsuleGeometry capsule;
			if (shapeCheck.verdict != ObjectVerdict::Ok || shapeCheck.cls != AuditedClass::Shape ||
			    shape->getGeometryType() != PxGeometryType::eCAPSULE || !shape->getCapsuleGeometry(capsule))
			{
				continue;
			}
			const PxVec3        centre = (pose * shape->getLocalPose()).p;
			const OriginReading reading = OriginFromController(aFeet, { centre.x, centre.y, centre.z });
			const double        across = std::hypot(reading.residual.x, reading.residual.y);
			if (FitsController(reading, capsule.radius) && (!best || across < best->across))
			{
				best = Candidate{ aActor, reading.origin, across };
			}
		}
	};
	aScene.lockRead();
	std::vector<PxActor*> actors(std::min(aScene.getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC), kMostActors));
	actors.resize(aScene.getActors(PxActorTypeFlag::eRIGID_DYNAMIC, actors.data(), PxU32(actors.size())));
	if (controller_ && std::find(actors.begin(), actors.end(), controller_) != actors.end())
	{
		measure(controller_);
	}
	if (!best)
	{
		++report_.searches;
		for (const PxActor* actor : actors)
		{
			measure(actor);
		}
	}
	aScene.unlockRead();
	controller_ = best ? best->actor : nullptr;
	return best ? std::optional<Vec3d>(best->origin) : std::nullopt;
}

bool PhysxGround::Read(const link::McBox& aBox, std::vector<link::McTriangle>& aOut)
{
	if (!Prepare())
	{
		return false;
	}
	ReadableMemory&    memory = session_.Memory();
	const ModuleBases& bases = session_.Bases();
	const Vec3d        origin = *report_.origin;
	const PxBounds3    box = PhysxBox(aBox, origin);
	std::vector<Tri>   triangles;
	AppendStats        stats;
	scene_->lockRead();
	for (const PxOverlapHit& hit : Overlap(*scene_, PxBoxGeometry(box.getExtents()), PxTransform(box.getCenter()), PxQueryFlag::eSTATIC))
	{
		const ObjectCheck actor = CheckObject(memory, bases, hit.actor, kActorBytes);
		const ObjectCheck shape = CheckObject(memory, bases, hit.shape);
		if (actor.verdict != ObjectVerdict::Ok || shape.verdict != ObjectVerdict::Ok || shape.cls != AuditedClass::Shape ||
		    (actor.cls != AuditedClass::RigidStatic && actor.cls != AuditedClass::RigidDynamic))
		{
			++report_.unreadable;
			continue;
		}
		if (!KeptShape(actor.cls, hit.shape->getFlags(), hit.shape->getGeometryType()))
		{
			++report_.left;
			continue;
		}
		++report_.kept;
		AppendTriangles(*hit.shape, hit.actor->getGlobalPose(), box, memory, bases, triangles, stats);
	}
	scene_->unlockRead();
	report_.unreadable += stats.unreadable;
	report_.triangles += triangles.size();
	for (const Tri& tri : triangles)
	{
		link::McTriangle out{};
		for (int k = 0; k < 3; ++k)
		{
			const Vec3d mc = McFromPhysx(tri.v[k], origin);
			out.v[k * 3] = float(mc.x);
			out.v[k * 3 + 1] = float(mc.y);
			out.v[k * 3 + 2] = float(mc.z);
		}
		aOut.push_back(out);
	}
	return true;
}

std::vector<std::string> PhysxGround::Describe(const Vec3d& aWorld, double aRadius)
{
	if (!Prepare())
	{
		return { "none: " + report_.why };
	}
	ReadableMemory&          memory = session_.Memory();
	const ModuleBases&       bases = session_.Bases();
	const Vec3d              origin = *report_.origin;
	const PxVec3             at(float(aWorld.x - origin.x), float(aWorld.y - origin.y), float(aWorld.z - origin.z));
	std::vector<std::string> lines;
	scene_->lockRead();
	for (const PxOverlapHit& hit :
	     Overlap(*scene_, PxSphereGeometry(float(aRadius)), PxTransform(at), PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC))
	{
		const ObjectCheck actor = CheckObject(memory, bases, hit.actor, kActorBytes);
		const ObjectCheck shape = CheckObject(memory, bases, hit.shape);
		if (actor.verdict != ObjectVerdict::Ok || shape.verdict != ObjectVerdict::Ok || shape.cls != AuditedClass::Shape ||
		    (actor.cls != AuditedClass::RigidStatic && actor.cls != AuditedClass::RigidDynamic))
		{
			lines.push_back(std::format("actor {} or its shape {} failed its checks", static_cast<const void*>(hit.actor),
			                            static_cast<const void*>(hit.shape)));
			continue;
		}
		const PxShapeFlags         flags = hit.shape->getFlags();
		const PxGeometryType::Enum geometry = hit.shape->getGeometryType();
		const char*                kind = actor.cls == AuditedClass::RigidStatic ? "static"
		                                  : static_cast<const PxRigidBody&>(*hit.actor).getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC)
		                                      ? "kinematic"
		                                      : "dynamic";
		const PxBounds3 bounds = hit.actor->getWorldBounds(1.0f);
		lines.push_back(std::format("{} {} of actor {}, actor bounds ({:.2f}, {:.2f}, {:.2f}) to ({:.2f}, {:.2f}, {:.2f}) in world axes; "
		                            "{}{}{}; query filter {}, simulation filter {}{}",
		                            kind, GeometryName(geometry), static_cast<const void*>(hit.actor), bounds.minimum.x + origin.x,
		                            bounds.minimum.y + origin.y, bounds.minimum.z + origin.z, bounds.maximum.x + origin.x,
		                            bounds.maximum.y + origin.y, bounds.maximum.z + origin.z,
		                            flags.isSet(PxShapeFlag::eSIMULATION_SHAPE) ? "simulation" : "not simulation",
		                            flags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE) ? ", scene query" : ", not scene query",
		                            flags.isSet(PxShapeFlag::eTRIGGER_SHAPE) ? ", trigger" : "", FilterKey(hit.shape->getQueryFilterData()),
		                            FilterKey(hit.shape->getSimulationFilterData()),
		                            KeptShape(actor.cls, flags, geometry) ? "; the exporter keeps it" : ""));
	}
	scene_->unlockRead();
	if (lines.empty())
	{
		lines.push_back("none");
	}
	return lines;
}
}
