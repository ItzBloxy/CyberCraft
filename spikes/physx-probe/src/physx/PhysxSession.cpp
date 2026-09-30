#include "physx/PhysxSession.h"

#include "physx/Checks.h"

#include <Windows.h>

#include <algorithm>
#include <format>
#include <set>
#include <string_view>

using namespace physx;

namespace cpmc::px
{
bool RunGuarded(GuardedFn aFn, void* aContext) noexcept
{
	__try
	{
		aFn(aContext);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

namespace
{
constexpr uint32_t kMaxScenes = 16;
constexpr uint32_t kMaxActorsPerScene = 10'000'000;
constexpr size_t   kSampleActors = 64;
constexpr uint32_t kMaxSampledActors = 10'000;
constexpr size_t   kMaxNameSamples = 32;
constexpr size_t   kOverlapBuffer = 8192;
constexpr size_t   kOverlapCap = 1'000'000;
constexpr uint32_t kMaxFaultsPerScene = 100;
constexpr float    kCapsuleSearch = 1.5f;

const PxActorTypeFlags kRigidActors = PxActorTypeFlag::eRIGID_STATIC | PxActorTypeFlag::eRIGID_DYNAMIC;

double NowMs()
{
	LARGE_INTEGER counter;
	LARGE_INTEGER frequency;
	QueryPerformanceCounter(&counter);
	QueryPerformanceFrequency(&frequency);
	return double(counter.QuadPart) * 1000.0 / double(frequency.QuadPart);
}

std::string FileVersionOf(HMODULE aModule)
{
	wchar_t path[MAX_PATH]{};
	if (!GetModuleFileNameW(aModule, path, MAX_PATH))
	{
		return {};
	}
	DWORD       handle = 0;
	const DWORD size = GetFileVersionInfoSizeW(path, &handle);
	if (size == 0)
	{
		return {};
	}
	std::vector<uint8_t> data(size);
	VS_FIXEDFILEINFO*    info = nullptr;
	UINT                 length = 0;
	if (!GetFileVersionInfoW(path, 0, size, data.data()) ||
	    !VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &length) || !info)
	{
		return {};
	}
	return std::format("{}.{}.{}.{}", HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
	                   HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
}

bool IsReadable(const void* aAddress)
{
	MEMORY_BASIC_INFORMATION info{};
	if (!VirtualQuery(aAddress, &info, sizeof(info)) || info.State != MEM_COMMIT)
	{
		return false;
	}
	constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
	                            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
	return (info.Protect & kReadable) != 0 && (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0;
}

bool LockRead(PxScene* aScene)
{
	return Guarded(*aScene, [](PxScene& aSceneRef) { aSceneRef.lockRead(); });
}

void UnlockRead(PxScene* aScene)
{
	Guarded(*aScene, [](PxScene& aSceneRef) { aSceneRef.unlockRead(); });
}

// The object's concrete type name, read from the game's PhysX.
std::string TypeName(const PxBase& aObject)
{
	return SanitizeName(aObject.getConcreteTypeName(), 64);
}

// The name agrees with the object's type ID, which is a plain data member (not a virtual call).
bool TypeAgrees(const PxBase& aObject, const std::string& aName)
{
	const int expected = ConcreteTypeForName(aName.c_str());
	return expected >= 0 && expected == int(aObject.getConcreteType());
}

// The mesh object a mesh-based shape references; nullptr for primitives.
const PxBase* MeshOf(const PxShape& aShape)
{
	switch (aShape.getGeometryType())
	{
	case PxGeometryType::eTRIANGLEMESH:
	{
		PxTriangleMeshGeometry geometry;
		return aShape.getTriangleMeshGeometry(geometry) ? geometry.triangleMesh : nullptr;
	}
	case PxGeometryType::eCONVEXMESH:
	{
		PxConvexMeshGeometry geometry;
		return aShape.getConvexMeshGeometry(geometry) ? geometry.convexMesh : nullptr;
	}
	case PxGeometryType::eHEIGHTFIELD:
	{
		PxHeightFieldGeometry geometry;
		return aShape.getHeightFieldGeometry(geometry) ? geometry.heightField : nullptr;
	}
	default:
		return nullptr;
	}
}

const char* ActorKind(const PxActor& aActor)
{
	switch (aActor.getType())
	{
	case PxActorType::eRIGID_STATIC:
		return "static";
	case PxActorType::eRIGID_DYNAMIC:
		return static_cast<const PxRigidBody&>(aActor).getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC)
		           ? "kinematic"
		           : "dynamic";
	default:
		return "other";
	}
}

std::vector<PxShape*> ShapesOf(const PxRigidActor& aActor)
{
	std::vector<PxShape*> shapes(aActor.getNbShapes());
	shapes.resize(aActor.getShapes(shapes.data(), PxU32(shapes.size())));
	return shapes;
}

// What checking objects found: type-name mismatches (step 6) and geometry/mesh disagreements (step 7).
struct Findings
{
	uint64_t    objectsChecked = 0;
	uint64_t    typeMismatches = 0;
	std::string firstTypeMismatch;
	uint64_t    geometryMismatches = 0;
	std::string firstGeometryMismatch;
	bool        seenTriangleMesh = false;
	bool        seenHeightField = false;
	bool        seenConvex = false;
};

void CheckType(const PxBase& aObject, const char* aWhat, Findings& aFindings, std::string& aNameOut)
{
	aNameOut = TypeName(aObject);
	++aFindings.objectsChecked;
	if (!TypeAgrees(aObject, aNameOut))
	{
		++aFindings.typeMismatches;
		if (aFindings.firstTypeMismatch.empty())
		{
			aFindings.firstTypeMismatch =
			    std::format("{} reports type name \"{}\" with type ID {}", aWhat, aNameOut, int(aObject.getConcreteType()));
		}
	}
}

// Checks one actor, its shapes and their meshes. With aCensus set, it also fills the census tallies.
void VisitActor(const PxActor& aActor, Findings& aFindings, SceneCensus* aCensus, std::set<std::string>* aNames)
{
	std::string actorName;
	CheckType(aActor, "an actor", aFindings, actorName);
	if (aCensus)
	{
		++aCensus->byConcreteType[actorName];
		if (const char* name = aActor.getName())
		{
			++aCensus->namedActors;
			if (aNames && aNames->size() < kMaxNameSamples)
			{
				aNames->insert(SanitizeName(name));
			}
		}
		if (aActor.userData)
		{
			++aCensus->actorsWithUserData;
		}
	}
	const PxActorType::Enum type = aActor.getType();
	if (type != PxActorType::eRIGID_STATIC && type != PxActorType::eRIGID_DYNAMIC)
	{
		return;
	}
	for (const PxShape* shape : ShapesOf(static_cast<const PxRigidActor&>(aActor)))
	{
		if (!shape)
		{
			continue;
		}
		std::string shapeName;
		CheckType(*shape, "a shape", aFindings, shapeName);
		const PxGeometryType::Enum geometry = shape->getGeometryType();
		if (aCensus)
		{
			++aCensus->byGeometryType[GeometryName(geometry)];
			const PxShapeFlags flags = shape->getFlags();
			if (flags.isSet(PxShapeFlag::eSIMULATION_SHAPE))
			{
				++aCensus->shapeFlags["simulation"];
			}
			if (flags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE))
			{
				++aCensus->shapeFlags["sceneQuery"];
			}
			if (flags.isSet(PxShapeFlag::eTRIGGER_SHAPE))
			{
				++aCensus->shapeFlags["trigger"];
			}
			++aCensus->queryFilter[FilterKey(shape->getQueryFilterData())];
			++aCensus->simulationFilter[FilterKey(shape->getSimulationFilterData())];
		}
		const PxBase* mesh = MeshOf(*shape);
		if (!mesh)
		{
			continue;
		}
		std::string meshName;
		CheckType(*mesh, "a mesh", aFindings, meshName);
		if (!GeometryMatchesMesh(geometry, ConcreteTypeForName(meshName.c_str())))
		{
			++aFindings.geometryMismatches;
			if (aFindings.firstGeometryMismatch.empty())
			{
				aFindings.firstGeometryMismatch =
				    std::format("a {} shape references a \"{}\"", GeometryName(geometry), meshName);
			}
		}
		aFindings.seenTriangleMesh |= geometry == PxGeometryType::eTRIANGLEMESH;
		aFindings.seenHeightField |= geometry == PxGeometryType::eHEIGHTFIELD;
		aFindings.seenConvex |= geometry == PxGeometryType::eCONVEXMESH;
	}
}

// Collects every overlap hit: PhysX hands over each full buffer, and the last partial one, via processTouches.
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
		if (out.size() >= kOverlapCap)
		{
			capped = true;
			return false;
		}
		return true;
	}

	std::vector<PxOverlapHit>& out;
	bool                       capped = false;
};

// Keeps the capsule nearest to V's feet (distance to its surface), if within kCapsuleSearch.
void NoteCapsule(const PxShape& aShape, const PxTransform& aActorPose, const PxVec3& aFeet, const char* aKind,
                 std::optional<NearestCapsule>& aNearest)
{
	PxCapsuleGeometry geometry;
	if (!aShape.getCapsuleGeometry(geometry))
	{
		return;
	}
	const PxTransform pose = aActorPose * aShape.getLocalPose();
	const PxVec3      axis = pose.q.rotate(PxVec3(geometry.halfHeight, 0.0f, 0.0f));  // capsules lie along local X
	const PxVec3      a = pose.p - axis;
	const PxVec3      ab = axis * 2.0f;
	const float       lengthSquared = ab.magnitudeSquared();
	const float       t = lengthSquared > 0.0f ? std::clamp((aFeet - a).dot(ab) / lengthSquared, 0.0f, 1.0f) : 0.0f;
	const float       distance = std::max(0.0f, (a + ab * t - aFeet).magnitude() - geometry.radius);
	if (distance <= kCapsuleSearch && (!aNearest || distance < aNearest->distance))
	{
		aNearest = NearestCapsule{ distance, geometry.radius, geometry.halfHeight, aKind };
	}
}

uint64_t CountTriangles(const std::map<std::string, std::vector<Tri>>& aTriangles)
{
	uint64_t count = 0;
	for (const auto& [category, triangles] : aTriangles)
	{
		count += triangles.size();
	}
	return count;
}

// Drops triangles beyond aBudget, starting from the last category.
void TrimToBudget(std::map<std::string, std::vector<Tri>>& aTriangles, uint64_t aBudget)
{
	uint64_t excess = CountTriangles(aTriangles);
	excess = excess > aBudget ? excess - aBudget : 0;
	for (auto it = aTriangles.rbegin(); it != aTriangles.rend() && excess > 0; ++it)
	{
		const uint64_t drop = std::min<uint64_t>(excess, it->second.size());
		it->second.resize(it->second.size() - drop);
		excess -= drop;
	}
}
}

const OpenResult& PhysxSession::Open()
{
	if (open_)
	{
		return *open_;
	}
	OpenResult result;
	const auto step = [&](int aNumber, const char* aName, bool aPass, std::string aDetail) {
		result.steps.push_back({ aNumber, aName, aPass, std::move(aDetail) });
		return aPass;
	};
	const auto finish = [&]() -> const OpenResult& {
		result.pass = result.steps.size() == 7 &&
		              std::all_of(result.steps.begin(), result.steps.end(), [](const CompatStep& aStep) { return aStep.pass; });
		open_ = std::move(result);
		return *open_;
	};

	// 1. The module and its version.
	const HMODULE module = GetModuleHandleW(L"PhysX3_x64.dll");
	result.physxFileVersion = module ? FileVersionOf(module) : std::string();
	if (!step(1, "module", module && result.physxFileVersion.starts_with("3.4.2."),
	          module ? "PhysX3_x64.dll " + result.physxFileVersion : std::string("PhysX3_x64.dll is not loaded")))
	{
		return finish();
	}

	// 2. PxGetPhysics.
	using GetPhysicsFn = PxPhysics& (*)();
	struct GetPhysicsCall
	{
		GetPhysicsFn fn;
		PxPhysics*   physics;
	};
	GetPhysicsCall getPhysics{ reinterpret_cast<GetPhysicsFn>(GetProcAddress(module, "PxGetPhysics")), nullptr };
	const bool     called = getPhysics.fn && Guarded(getPhysics, [](GetPhysicsCall& aCall) { aCall.physics = &aCall.fn(); });
	PxPhysics*     physics = called ? getPhysics.physics : nullptr;
	const bool     readable = physics && IsReadable(physics) && IsReadable(*reinterpret_cast<void* const*>(physics));
	if (!step(2, "PxGetPhysics", readable,
	          !getPhysics.fn ? std::string("PxGetPhysics isn't exported")
	          : readable     ? std::format("PxPhysics at {}", static_cast<const void*>(physics))
	                         : std::string("PxGetPhysics faulted or returned unreadable memory")))
	{
		return finish();
	}

	// 3. Tolerances.
	struct TolerancesCall
	{
		PxPhysics* physics;
		float      length;
		float      speed;
	};
	TolerancesCall tolerances{ physics, 0.0f, 0.0f };
	const bool     gotTolerances = Guarded(tolerances, [](TolerancesCall& aCall) {
        const PxTolerancesScale& scale = aCall.physics->getTolerancesScale();
        aCall.length = scale.length;
        aCall.speed = scale.speed;
    });
	if (!step(3, "tolerances", gotTolerances && TolerancesPlausible(tolerances.length, tolerances.speed),
	          gotTolerances ? std::format("length {}, speed {}", tolerances.length, tolerances.speed)
	                        : std::string("getTolerancesScale faulted")))
	{
		return finish();
	}

	// 4. Scenes.
	struct ScenesCall
	{
		PxPhysics*            physics;
		uint32_t              count;
		std::vector<PxScene*> scenes;
	};
	ScenesCall scenes{ physics, 0, {} };
	const bool gotScenes = Guarded(scenes, [](ScenesCall& aCall) {
		aCall.count = aCall.physics->getNbScenes();
		if (aCall.count >= 1 && aCall.count <= kMaxScenes)
		{
			aCall.scenes.resize(aCall.count);
			aCall.scenes.resize(aCall.physics->getScenes(aCall.scenes.data(), aCall.count));
		}
	});
	const bool scenesOk = gotScenes && scenes.count >= 1 && scenes.count <= kMaxScenes &&
	                      scenes.scenes.size() == scenes.count &&
	                      std::none_of(scenes.scenes.begin(), scenes.scenes.end(), [](PxScene* aScene) { return !aScene; });
	if (!step(4, "scenes", scenesOk,
	          gotScenes ? std::format("{} scene(s)", scenes.count) : std::string("getNbScenes/getScenes faulted")))
	{
		return finish();
	}

	// 5. Gravity and actor counts.
	struct SceneFacts
	{
		PxScene* scene;
		PxVec3   gravity;
		uint32_t actors;
	};
	std::vector<SceneFacts> facts;
	bool                    factsOk = true;
	for (PxScene* scene : scenes.scenes)
	{
		SceneFacts sceneFacts{ scene, PxVec3(0.0f), 0 };
		factsOk &= Guarded(sceneFacts, [](SceneFacts& aFacts) {
			aFacts.gravity = aFacts.scene->getGravity();
			aFacts.actors = aFacts.scene->getNbActors(kRigidActors);
		});
		facts.push_back(sceneFacts);
	}
	const bool allSane = factsOk && std::all_of(facts.begin(), facts.end(), [](const SceneFacts& aFacts) {
		return GravityFinite(aFacts.gravity) && aFacts.actors <= kMaxActorsPerScene;
	});
	const bool anyEarthLike =
	    std::any_of(facts.begin(), facts.end(), [](const SceneFacts& aFacts) { return GravityEarthLike(aFacts.gravity); });
	std::string factsDetail;
	for (const SceneFacts& sceneFacts : facts)
	{
		factsDetail += std::format("{}gravity ({:.2f}, {:.2f}, {:.2f}), {} actors", factsDetail.empty() ? "" : "; ",
		                           sceneFacts.gravity.x, sceneFacts.gravity.y, sceneFacts.gravity.z, sceneFacts.actors);
	}
	if (!step(5, "gravity and counts", allSane && anyEarthLike,
	          factsOk ? factsDetail : std::string("getGravity/getNbActors faulted")))
	{
		return finish();
	}

	// 6 and 7. Type names and IDs of sampled actors, shapes and meshes; geometry types against mesh types.
	Findings findings;
	bool     sampleFaulted = false;
	for (PxScene* scene : scenes.scenes)
	{
		if (!LockRead(scene))
		{
			sampleFaulted = true;
			continue;
		}
		struct SampleCall
		{
			PxScene*  scene;
			Findings* findings;
		};
		SampleCall sample{ scene, &findings };
		sampleFaulted |= !Guarded(sample, [](SampleCall& aCall) {
			std::vector<PxActor*> actors(std::min<PxU32>(aCall.scene->getNbActors(kRigidActors), kMaxSampledActors));
			actors.resize(aCall.scene->getActors(kRigidActors, actors.data(), PxU32(actors.size())));
			for (size_t i = 0; i < actors.size() && aCall.findings->typeMismatches == 0; ++i)
			{
				const Findings& seen = *aCall.findings;
				if (i >= kSampleActors && seen.seenTriangleMesh && seen.seenHeightField && seen.seenConvex)
				{
					break;
				}
				if (actors[i])
				{
					VisitActor(*actors[i], *aCall.findings, nullptr, nullptr);
				}
			}
		});
		UnlockRead(scene);
	}
	const std::string seenDetail =
	    std::format("checked {} objects; saw a triangle mesh: {}, a heightfield: {}, a convex mesh: {}", findings.objectsChecked,
	                findings.seenTriangleMesh, findings.seenHeightField, findings.seenConvex);
	if (!step(6, "type names", !sampleFaulted && findings.typeMismatches == 0 && findings.objectsChecked > 0,
	          sampleFaulted            ? "faulted while sampling; " + seenDetail
	          : findings.typeMismatches ? findings.firstTypeMismatch
	                                    : seenDetail))
	{
		return finish();
	}
	step(7, "geometry and mesh types", findings.geometryMismatches == 0,
	     findings.geometryMismatches ? findings.firstGeometryMismatch : std::string("every checked mesh matches its shape"));
	if (result.steps.back().pass)
	{
		physics_ = physics;
	}
	return finish();
}

std::vector<PxScene*> PhysxSession::FetchScenes(uint32_t& aFaults) const
{
	if (!physics_)
	{
		return {};
	}
	struct Call
	{
		PxPhysics*            physics;
		std::vector<PxScene*> scenes;
	};
	Call call{ physics_, {} };
	if (!Guarded(call, [](Call& aCall) {
		    aCall.scenes.resize(std::min<PxU32>(aCall.physics->getNbScenes(), kMaxScenes));
		    aCall.scenes.resize(aCall.physics->getScenes(aCall.scenes.data(), PxU32(aCall.scenes.size())));
	    }))
	{
		++aFaults;
		return {};
	}
	std::erase(call.scenes, nullptr);
	return call.scenes;
}

SceneDump PhysxSession::DumpScene(PxScene* aScene, int aIndex, const PxBounds3& aRegion, const PxVec3& aFeet,
                                  uint64_t aTriangleBudget, uint32_t& aFaults) const
{
	SceneDump dump;
	dump.census.index = aIndex;
	if (!LockRead(aScene))
	{
		++aFaults;
		return dump;
	}

	// Scene facts and the actor list.
	double t0 = NowMs();
	struct ListCall
	{
		PxScene*              scene;
		SceneCensus*          census;
		std::vector<PxActor*> actors;
	};
	ListCall list{ aScene, &dump.census, {} };
	if (!Guarded(list, [](ListCall& aCall) {
		    SceneCensus& census = *aCall.census;
		    const PxVec3 gravity = aCall.scene->getGravity();
		    census.gravity = { gravity.x, gravity.y, gravity.z };
		    const PxSceneFlags flags = aCall.scene->getFlags();
		    census.flags = static_cast<uint32_t>(flags);
		    census.requiresRwLock = flags.isSet(PxSceneFlag::eREQUIRE_RW_LOCK);
		    census.staticActors = aCall.scene->getNbActors(PxActorTypeFlag::eRIGID_STATIC);
		    census.dynamicActors = aCall.scene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC);
		    aCall.actors.resize(std::min<PxU32>(aCall.scene->getNbActors(kRigidActors), kMaxActorsPerScene));
		    aCall.actors.resize(aCall.scene->getActors(kRigidActors, aCall.actors.data(), PxU32(aCall.actors.size())));
		    std::vector<PxAggregate*> aggregates(aCall.scene->getNbAggregates());
		    aggregates.resize(aCall.scene->getAggregates(aggregates.data(), PxU32(aggregates.size())));
		    census.aggregates = uint32_t(aggregates.size());
		    for (const PxAggregate* aggregate : aggregates)
		    {
			    census.actorsInAggregates += aggregate ? aggregate->getNbActors() : 0;
		    }
		    census.articulations = aCall.scene->getNbArticulations();
	    }))
	{
		++aFaults;
	}

	// The census: every actor, shape and mesh, re-checking each one's type.
	Findings              findings;
	std::set<std::string> names;
	uint32_t              censusFaults = 0;
	for (PxActor* actor : list.actors)
	{
		struct VisitCall
		{
			PxActor*               actor;
			Findings*              findings;
			SceneCensus*           census;
			std::set<std::string>* names;
		};
		VisitCall visit{ actor, &findings, &dump.census, &names };
		if (actor && !Guarded(visit, [](VisitCall& aCall) { VisitActor(*aCall.actor, *aCall.findings, aCall.census, aCall.names); }))
		{
			++aFaults;
			if (++censusFaults >= kMaxFaultsPerScene)
			{
				break;
			}
		}
	}
	dump.census.mismatches = findings.typeMismatches + findings.geometryMismatches;
	dump.census.actorNameSamples.assign(names.begin(), names.end());
	dump.census.censusMs = NowMs() - t0;

	// Region selection by actor bounds.
	t0 = NowMs();
	std::vector<PxActor*> selected;
	for (PxActor* actor : list.actors)
	{
		struct BoundsCall
		{
			PxActor*  actor;
			PxBounds3 region;
			bool      touches;
		};
		BoundsCall bounds{ actor, aRegion, false };
		if (!actor)
		{
			continue;
		}
		if (!Guarded(bounds, [](BoundsCall& aCall) { aCall.touches = aCall.actor->getWorldBounds(1.0f).intersects(aCall.region); }))
		{
			++aFaults;
		}
		else if (bounds.touches)
		{
			selected.push_back(actor);
		}
	}
	dump.boundsSelected = uint32_t(selected.size());
	dump.selectMs = NowMs() - t0;

	// The same region through PhysX's own overlap query (scene-query shapes only), for comparison.
	t0 = NowMs();
	struct OverlapCall
	{
		PxScene*                  scene;
		PxBounds3                 region;
		std::vector<PxOverlapHit> hits;
		bool                      capped;
	};
	OverlapCall overlap{ aScene, aRegion, {}, false };
	if (!Guarded(overlap, [](OverlapCall& aCall) {
		    std::vector<PxOverlapHit> buffer(kOverlapBuffer);
		    CollectOverlaps           collect(buffer.data(), PxU32(buffer.size()), aCall.hits);
		    aCall.scene->overlap(PxBoxGeometry(aCall.region.getExtents()), PxTransform(aCall.region.getCenter()), collect,
		                         PxQueryFilterData(PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::eNO_BLOCK));
		    aCall.capped = collect.capped;
	    }))
	{
		++aFaults;
	}
	dump.overlapHits = uint32_t(overlap.hits.size());
	dump.overlapCapped = overlap.capped;
	dump.overlapMs = NowMs() - t0;

	// Triangles of every shape of the selected actors.
	t0 = NowMs();
	for (PxActor* actor : selected)
	{
		if (CountTriangles(dump.triangles) >= aTriangleBudget)
		{
			dump.triangleCapHit = true;
			break;
		}
		struct ExtractCall
		{
			PxActor*   actor;
			PxBounds3  region;
			PxVec3     feet;
			SceneDump* dump;
		};
		ExtractCall extract{ actor, aRegion, aFeet, &dump };
		if (!Guarded(extract, [](ExtractCall& aCall) {
			    const char*        kind = ActorKind(*aCall.actor);
			    const auto&        rigid = static_cast<const PxRigidActor&>(*aCall.actor);
			    const PxTransform  pose = rigid.getGlobalPose();
			    for (const PxShape* shape : ShapesOf(rigid))
			    {
				    if (!shape)
				    {
					    continue;
				    }
				    const PxGeometryType::Enum geometry = shape->getGeometryType();
				    AppendTriangles(*shape, pose, aCall.region,
				                    aCall.dump->triangles[Category(kind, shape->getFlags(), geometry)], aCall.dump->stats);
				    if (geometry == PxGeometryType::eCAPSULE && std::string_view(kind) != "static")
				    {
					    NoteCapsule(*shape, pose, aCall.feet, kind, aCall.dump->nearestCapsule);
				    }
			    }
		    }))
		{
			++aFaults;
		}
	}
	if (CountTriangles(dump.triangles) > aTriangleBudget)
	{
		TrimToBudget(dump.triangles, aTriangleBudget);
		dump.triangleCapHit = true;
	}
	std::erase_if(dump.triangles, [](const auto& aEntry) { return aEntry.second.empty(); });
	dump.extractMs = NowMs() - t0;

	UnlockRead(aScene);
	return dump;
}

std::optional<float> PhysxSession::RaycastDown(PxScene* aScene, const PxVec3& aFrom, float aMaxDistance, uint32_t& aFaults) const
{
	if (!LockRead(aScene))
	{
		++aFaults;
		return std::nullopt;
	}
	struct RayCall
	{
		PxScene* scene;
		PxVec3   from;
		float    maxDistance;
		bool     hit;
		float    distance;
	};
	RayCall    ray{ aScene, aFrom, aMaxDistance, false, 0.0f };
	const bool ok = Guarded(ray, [](RayCall& aCall) {
		PxRaycastBuffer buffer;
		aCall.scene->raycast(aCall.from, PxVec3(0.0f, 0.0f, -1.0f), aCall.maxDistance, buffer, PxHitFlag::eDEFAULT,
		                     PxQueryFilterData(PxQueryFlag::eSTATIC));
		aCall.hit = buffer.hasBlock;
		aCall.distance = buffer.hasBlock ? buffer.block.distance : 0.0f;
	});
	UnlockRead(aScene);
	if (!ok)
	{
		++aFaults;
		return std::nullopt;
	}
	return ray.hit ? std::optional<float>(ray.distance) : std::nullopt;
}
}
