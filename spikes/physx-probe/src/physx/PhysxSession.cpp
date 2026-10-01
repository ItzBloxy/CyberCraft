#include "physx/PhysxSession.h"

#include "OriginWatch.h"
#include "physx/Checks.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <initializer_list>
#include <set>
#include <string_view>
#include <utility>

using namespace physx;

namespace cpmc::px
{
FileVersions ReadFileVersions(void* aModule)
{
	FileVersions versions;
	wchar_t      path[MAX_PATH]{};
	if (!GetModuleFileNameW(static_cast<HMODULE>(aModule), path, MAX_PATH))
	{
		return versions;
	}
	DWORD       handle = 0;
	const DWORD size = GetFileVersionInfoSizeW(path, &handle);
	if (size == 0)
	{
		return versions;
	}
	std::vector<uint8_t> data(size);
	if (!GetFileVersionInfoW(path, 0, size, data.data()))
	{
		return versions;
	}
	VS_FIXEDFILEINFO* info = nullptr;
	UINT              length = 0;
	if (VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &length) && info)
	{
		versions.binary = std::format("{}.{}.{}.{}", HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
		                              HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
	}
	// The string table is keyed by the resource's first language and code page.
	struct Translation
	{
		WORD language;
		WORD codePage;
	};
	Translation* translations = nullptr;
	if (VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&translations), &length) &&
	    translations && length >= sizeof(Translation))
	{
		const std::wstring key =
		    std::format(L"\\StringFileInfo\\{:04x}{:04x}\\FileVersion", translations[0].language, translations[0].codePage);
		wchar_t* text = nullptr;
		UINT     textLength = 0;
		if (VerQueryValueW(data.data(), key.c_str(), reinterpret_cast<void**>(&text), &textLength) && text)
		{
			for (size_t i = 0; i < textLength && text[i] != L'\0'; ++i)
			{
				versions.text.push_back(text[i] < 0x80 ? char(text[i]) : '?');  // version strings are ASCII
			}
		}
	}
	return versions;
}

namespace
{
constexpr uint32_t kMaxScenes = 16;
constexpr uint32_t kMaxActorsPerScene = 10'000'000;
constexpr size_t   kSampleActors = 64;
constexpr uint32_t kMaxSampledActors = 10'000;
constexpr size_t   kMaxNameSamples = 32;
constexpr size_t   kMaxNameLength = 128;
constexpr size_t   kOverlapBuffer = 8192;
constexpr size_t   kOverlapCap = 1'000'000;
constexpr uint64_t kMaxSkipsPerScene = 100;
constexpr size_t   kMaxCapsules = 4096;
constexpr float    kCapsuleSearch = 1.5f;
constexpr size_t   kActorBytes = 24;  // the vtable pointer, the type ID and base flags, and userData

const PxActorTypeFlags kRigidActors = PxActorTypeFlag::eRIGID_STATIC | PxActorTypeFlag::eRIGID_DYNAMIC;

double NowMs()
{
	LARGE_INTEGER counter;
	LARGE_INTEGER frequency;
	QueryPerformanceCounter(&counter);
	QueryPerformanceFrequency(&frequency);
	return double(counter.QuadPart) * 1000.0 / double(frequency.QuadPart);
}

std::string Narrow(const wchar_t* aText)
{
	std::string out;
	for (; *aText; ++aText)
	{
		out.push_back(char(*aText));  // module names are ASCII
	}
	return out;
}

std::string Describe(const ObjectCheck& aCheck)
{
	switch (aCheck.verdict)
	{
	case ObjectVerdict::Ok:
		return std::format("a {} vtable", InterfaceName(aCheck.cls));
	case ObjectVerdict::Unreadable:
		return "not readable";
	case ObjectVerdict::Unaudited:
		return "a vtable the audit doesn't cover";
	case ObjectVerdict::WrongType:
		return std::format("a {} vtable but a different type ID", InterfaceName(aCheck.cls));
	}
	return {};
}

// What checking objects found, in a census or in the compatibility check's sample.
struct Findings
{
	uint64_t    objectsChecked = 0;
	uint64_t    unreadable = 0;
	uint64_t    unaudited = 0;
	uint64_t    typeMismatches = 0;      // a type ID that disagrees with the vtable, or the wrong class for the place
	uint64_t    geometryMismatches = 0;  // a shape whose mesh doesn't fit its geometry type
	std::string firstProblem;
	std::string firstGeometryMismatch;
	bool        seenTriangleMesh = false;
	bool        seenHeightField = false;
	bool        seenConvex = false;

	uint64_t Skipped() const { return unreadable + unaudited; }
	uint64_t Mismatches() const { return typeMismatches + geometryMismatches; }
};

// Checks game objects and memory for one dump.
struct Checker
{
	ReadableMemory&    memory;
	const ModuleBases& bases;

	// The object's audited class, if it passes CheckObject and is one of aAccepted. Counts what it finds either way.
	std::optional<AuditedClass> Accept(const void* aObject, std::initializer_list<AuditedClass> aAccepted, const char* aWhat,
	                                   Findings& aFindings, size_t aBytes = 16) const
	{
		++aFindings.objectsChecked;
		const ObjectCheck check = CheckObject(memory, bases, aObject, aBytes);
		const bool accepted = check.verdict == ObjectVerdict::Ok &&
		                      std::find(aAccepted.begin(), aAccepted.end(), check.cls) != aAccepted.end();
		if (accepted)
		{
			return check.cls;
		}
		switch (check.verdict)
		{
		case ObjectVerdict::Unreadable:
			++aFindings.unreadable;
			break;
		case ObjectVerdict::Unaudited:
			++aFindings.unaudited;
			break;
		default:
			++aFindings.typeMismatches;
			break;
		}
		if (aFindings.firstProblem.empty())
		{
			aFindings.firstProblem = std::format("{} at {} is {}", aWhat, aObject, Describe(check));
		}
		return std::nullopt;
	}

	// How many bytes from aText on can be read, up to aMax.
	size_t ReadableLength(const char* aText, size_t aMax) const
	{
		size_t length = 0;
		while (length < aMax && memory.Range(aText + length, 1))
		{
			++length;
		}
		return length;
	}
};

std::vector<PxShape*> ShapesOf(const PxRigidActor& aActor)
{
	std::vector<PxShape*> shapes(aActor.getNbShapes());
	shapes.resize(aActor.getShapes(shapes.data(), PxU32(shapes.size())));
	return shapes;
}

// The mesh object a mesh-based shape references; nullptr for primitives.
const PxBase* MeshOf(const PxShape& aShape, PxGeometryType::Enum aGeometry)
{
	switch (aGeometry)
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

// "static", "kinematic" or "dynamic". The audited class tells a static actor from a dynamic one.
const char* ActorKind(const PxActor& aActor, AuditedClass aClass)
{
	if (aClass == AuditedClass::RigidStatic)
	{
		return "static";
	}
	return static_cast<const PxRigidBody&>(aActor).getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC) ? "kinematic"
	                                                                                                       : "dynamic";
}

// Checks one actor, its shapes and their meshes; with aCensus set, it also fills the census tallies. Returns the
// actor's class if the actor itself passed its check.
std::optional<AuditedClass> VisitActor(const Checker& aChecker, const PxActor* aActor, Findings& aFindings,
                                       SceneCensus* aCensus, std::set<std::string>* aNames)
{
	const std::optional<AuditedClass> actorClass =
	    aChecker.Accept(aActor, { AuditedClass::RigidStatic, AuditedClass::RigidDynamic }, "an actor", aFindings, kActorBytes);
	if (!actorClass)
	{
		return std::nullopt;
	}
	if (aCensus)
	{
		++aCensus->byConcreteType[InterfaceName(*actorClass)];
		if (const char* name = aActor->getName())
		{
			++aCensus->namedActors;
			if (aNames && aNames->size() < kMaxNameSamples)
			{
				aNames->insert(SanitizeName(name, aChecker.ReadableLength(name, kMaxNameLength)));
			}
		}
		if (aActor->userData)
		{
			++aCensus->actorsWithUserData;
		}
	}
	for (const PxShape* shape : ShapesOf(static_cast<const PxRigidActor&>(*aActor)))
	{
		if (!aChecker.Accept(shape, { AuditedClass::Shape }, "a shape", aFindings))
		{
			continue;
		}
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
		const PxBase* mesh = MeshOf(*shape, geometry);
		if (!mesh)
		{
			continue;
		}
		const std::optional<AuditedClass> meshClass =
		    aChecker.Accept(mesh,
		                    { AuditedClass::ConvexMesh, AuditedClass::TriangleMeshBv33, AuditedClass::TriangleMeshBv34,
		                      AuditedClass::HeightField },
		                    "a mesh", aFindings);
		if (!meshClass)
		{
			continue;
		}
		if (!GeometryMatchesMesh(geometry, ConcreteTypeOf(*meshClass)))
		{
			++aFindings.geometryMismatches;
			if (aFindings.firstGeometryMismatch.empty())
			{
				aFindings.firstGeometryMismatch =
				    std::format("a {} shape references a {}", GeometryName(geometry), InterfaceName(*meshClass));
			}
		}
		aFindings.seenTriangleMesh |= geometry == PxGeometryType::eTRIANGLEMESH;
		aFindings.seenHeightField |= geometry == PxGeometryType::eHEIGHTFIELD;
		aFindings.seenConvex |= geometry == PxGeometryType::eCONVEXMESH;
	}
	return actorClass;
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

// A capsule that could be V's controller, and how far it sits from under V, across.
struct Candidate
{
	ControllerSample sample;
	PxScene*         scene = nullptr;
	double           across = 0.0;
};

// Keeps the best fit for V's controller among one actor's capsules, if it beats aBest. Only kinematic actors count.
void MeasureController(const Checker& aChecker, PxScene* aScene, const PxActor* aActor, const Vec3d& aFeet,
                       std::optional<Candidate>& aBest)
{
	Findings findings;  // the watch only needs the checks, not their tallies
	if (!aChecker.Accept(aActor, { AuditedClass::RigidDynamic }, "an actor", findings, kActorBytes) ||
	    !static_cast<const PxRigidBody&>(*aActor).getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC))
	{
		return;
	}
	const auto&       rigid = static_cast<const PxRigidActor&>(*aActor);
	const PxTransform pose = rigid.getGlobalPose();
	for (const PxShape* shape : ShapesOf(rigid))
	{
		PxCapsuleGeometry geometry;
		if (!aChecker.Accept(shape, { AuditedClass::Shape }, "a shape", findings) ||
		    shape->getGeometryType() != PxGeometryType::eCAPSULE || !shape->getCapsuleGeometry(geometry))
		{
			continue;
		}
		const PxVec3        p = (pose * shape->getLocalPose()).p;
		const Vec3d         centre{ p.x, p.y, p.z };
		const OriginReading reading = OriginFromController(aFeet, centre);
		const double        across = std::hypot(reading.residual.x, reading.residual.y);
		if (FitsController(reading, geometry.radius) && (!aBest || across < aBest->across))
		{
			aBest = Candidate{ { aActor, centre, geometry.radius, geometry.halfHeight, false }, aScene, across };
		}
	}
}
}

PhysxSession::Build PhysxSession::CheckBuild()
{
	Build      build;
	const auto step = [&](int aNumber, const char* aName, bool aPass, std::string aDetail) {
		build.steps.push_back({ aNumber, aName, aPass, std::move(aDetail) });
		return aPass;
	};

	// 1. The build: both DLLs loaded, with the audited timestamps and image sizes.
	HMODULE     modules[2] = {};
	bool        audited = true;
	std::string detail;
	for (size_t i = 0; i < 2; ++i)
	{
		const AuditedModule& expected = kAuditedModules[i];
		modules[i] = GetModuleHandleW(expected.name);
		ModuleReport report;
		report.name = Narrow(expected.name);
		const std::optional<ModuleId> id = modules[i] ? ModuleIdentity(modules[i]) : std::nullopt;
		if (id)
		{
			report.timestamp = id->timestamp;
			report.imageSize = id->imageSize;
			report.audited = *id == expected.id;
		}
		audited &= report.audited;
		detail += std::format("{}{} ", detail.empty() ? "" : "; ", report.name);
		detail += !modules[i] ? std::string("is not loaded")
		          : !id       ? std::string("has no readable PE header")
		                      : std::format("timestamp 0x{:x}, image size 0x{:x}{}", report.timestamp, report.imageSize,
		                                    report.audited ? "" : ": not the audited build");
		build.modules.push_back(std::move(report));
	}
	const FileVersions versions = modules[0] ? ReadFileVersions(modules[0]) : FileVersions{};
	build.physxFileVersion = versions.text;
	if (modules[0])
	{
		detail += std::format("; PhysX3_x64.dll FileVersion \"{}\" (binary field {})", versions.text, versions.binary);
	}
	if (!step(1, "build", audited, detail))
	{
		return build;
	}
	build.bases = { reinterpret_cast<uintptr_t>(modules[0]), reinterpret_cast<uintptr_t>(modules[1]) };

	// 2. The physics object. PxGetPhysics is the only function called before this check.
	using GetPhysicsFn = PxPhysics& (*)();
	const auto        getPhysics = reinterpret_cast<GetPhysicsFn>(GetProcAddress(modules[0], "PxGetPhysics"));
	PxPhysics*        physics = getPhysics ? &getPhysics() : nullptr;
	const ObjectCheck check = CheckObject(memory_, build.bases, physics);
	if (!step(2, "physics", check.verdict == ObjectVerdict::Ok && check.cls == AuditedClass::Physics,
	          getPhysics ? std::format("PxPhysics at {}: {}", static_cast<const void*>(physics), Describe(check))
	                     : std::string("PxGetPhysics isn't exported")))
	{
		return build;
	}

	// 3. Tolerances.
	const PxTolerancesScale& scale = physics->getTolerancesScale();
	const bool               readable = memory_.Range(&scale, sizeof(scale));
	if (!step(3, "tolerances", readable && TolerancesPlausible(scale.length, scale.speed),
	          readable ? std::format("length {}, speed {}", scale.length, scale.speed)
	                   : std::string("the tolerances aren't readable")))
	{
		return build;
	}
	build.physics = physics;
	build.pass = true;
	return build;
}

CheckResult PhysxSession::Check()
{
	memory_.Clear();
	if (!build_)
	{
		build_ = CheckBuild();
	}
	CheckResult result;
	result.steps = build_->steps;
	result.physxFileVersion = build_->physxFileVersion;
	result.modules = build_->modules;
	if (!build_->pass)
	{
		return result;
	}
	const auto step = [&](int aNumber, const char* aName, bool aPass, std::string aDetail) {
		result.steps.push_back({ aNumber, aName, aPass, std::move(aDetail) });
		return aPass;
	};
	PxPhysics&    physics = *build_->physics;
	const Checker checker{ memory_, build_->bases };

	// 4. Scenes.
	const PxU32           count = physics.getNbScenes();
	std::vector<PxScene*> scenes;
	if (count >= 1 && count <= kMaxScenes)
	{
		scenes.resize(count);
		scenes.resize(physics.getScenes(scenes.data(), count));
	}
	std::string sceneProblem;
	for (PxScene* scene : scenes)
	{
		const ObjectCheck check = CheckObject(memory_, build_->bases, scene);
		if (sceneProblem.empty() && (check.verdict != ObjectVerdict::Ok || check.cls != AuditedClass::Scene))
		{
			sceneProblem = std::format("; the scene at {} is {}", static_cast<const void*>(scene), Describe(check));
		}
	}
	const bool scenesOk = count >= 1 && count <= kMaxScenes && scenes.size() == count && sceneProblem.empty();
	if (!step(4, "scenes", scenesOk, std::format("{} scene(s){}", count, sceneProblem)))
	{
		return result;
	}
	result.scenes = scenes;

	// 5. Gravity and counts.
	bool        allSane = true;
	bool        anyEarthLike = false;
	std::string factsDetail;
	for (PxScene* scene : scenes)
	{
		const PxVec3 gravity = scene->getGravity();
		const PxU32  actors = scene->getNbActors(kRigidActors);
		allSane &= GravityFinite(gravity) && actors <= kMaxActorsPerScene;
		anyEarthLike |= GravityEarthLike(gravity);
		factsDetail += std::format("{}gravity ({:.2f}, {:.2f}, {:.2f}), {} actors", factsDetail.empty() ? "" : "; ", gravity.x,
		                           gravity.y, gravity.z, actors);
	}
	if (!step(5, "gravity and counts", allSane && anyEarthLike, factsDetail))
	{
		return result;
	}

	// 6 and 7. Sampled actors, their shapes and their meshes; then each shape's geometry against its mesh.
	Findings findings;
	for (PxScene* scene : scenes)
	{
		scene->lockRead();
		std::vector<PxActor*> actors(std::min<PxU32>(scene->getNbActors(kRigidActors), kMaxSampledActors));
		actors.resize(scene->getActors(kRigidActors, actors.data(), PxU32(actors.size())));
		for (size_t i = 0; i < actors.size(); ++i)
		{
			const bool seenAll = findings.seenTriangleMesh && findings.seenHeightField && findings.seenConvex;
			if ((i >= kSampleActors && seenAll) || findings.Skipped() + findings.typeMismatches > 0)
			{
				break;
			}
			VisitActor(checker, actors[i], findings, nullptr, nullptr);
		}
		scene->unlockRead();
	}
	const bool objectsOk = findings.objectsChecked > 0 && findings.Skipped() + findings.typeMismatches == 0;
	if (!step(6, "objects", objectsOk,
	          objectsOk ? std::format("checked {} objects; saw a triangle mesh: {}, a heightfield: {}, a convex mesh: {}",
	                                  findings.objectsChecked, findings.seenTriangleMesh, findings.seenHeightField,
	                                  findings.seenConvex)
	                    : findings.firstProblem))
	{
		return result;
	}
	step(7, "geometry and meshes", findings.geometryMismatches == 0,
	     findings.geometryMismatches ? findings.firstGeometryMismatch : std::string("every checked mesh matches its shape"));
	result.pass = result.steps.size() == 7 &&
	              std::all_of(result.steps.begin(), result.steps.end(), [](const CompatStep& aStep) { return aStep.pass; });
	return result;
}

SceneDump PhysxSession::DumpScene(PxScene* aScene, int aIndex, const PxBounds3& aRegion, const PxVec3& aFeet,
                                  uint64_t aTriangleBudget)
{
	SceneDump     dump;
	SceneCensus&  census = dump.census;
	const Checker checker{ memory_, build_->bases };
	census.index = aIndex;
	aScene->lockRead();

	// Scene facts and the actor list.
	double       t0 = NowMs();
	const PxVec3 gravity = aScene->getGravity();
	census.gravity = { gravity.x, gravity.y, gravity.z };
	const PxSceneFlags flags = aScene->getFlags();
	census.flags = static_cast<uint32_t>(flags);
	census.requiresRwLock = flags.isSet(PxSceneFlag::eREQUIRE_RW_LOCK);
	census.staticActors = aScene->getNbActors(PxActorTypeFlag::eRIGID_STATIC);
	census.dynamicActors = aScene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC);
	census.articulations = aScene->getNbArticulations();
	std::vector<PxActor*> actors(std::min<PxU32>(aScene->getNbActors(kRigidActors), kMaxActorsPerScene));
	actors.resize(aScene->getActors(kRigidActors, actors.data(), PxU32(actors.size())));
	std::vector<PxAggregate*> aggregates(aScene->getNbAggregates());
	aggregates.resize(aScene->getAggregates(aggregates.data(), PxU32(aggregates.size())));
	census.aggregates = uint32_t(aggregates.size());

	// The census: every aggregate, actor, shape and mesh, each checked before anything is called on it.
	Findings findings;
	for (const PxAggregate* aggregate : aggregates)
	{
		if (checker.Accept(aggregate, { AuditedClass::Aggregate }, "an aggregate", findings))
		{
			census.actorsInAggregates += aggregate->getNbActors();
		}
	}
	std::set<std::string>                                names;
	std::vector<std::pair<const PxActor*, AuditedClass>> usable;
	bool                                                 abandoned = false;
	for (const PxActor* actor : actors)
	{
		if (findings.Skipped() >= kMaxSkipsPerScene)
		{
			abandoned = true;
			break;
		}
		if (const std::optional<AuditedClass> cls = VisitActor(checker, actor, findings, &census, &names))
		{
			usable.emplace_back(actor, *cls);
		}
	}
	census.skippedUnreadable = findings.unreadable;
	census.skippedUnaudited = findings.unaudited;
	census.mismatches = findings.Mismatches();
	census.actorNameSamples.assign(names.begin(), names.end());
	census.censusMs = NowMs() - t0;
	if (abandoned)
	{
		aScene->unlockRead();
		return dump;
	}

	// Region selection by actor bounds, noting where every actor is.
	t0 = NowMs();
	std::vector<std::pair<const PxActor*, AuditedClass>> selected;
	std::vector<Vec3d>                                   staticCenters;
	std::vector<Vec3d>                                   dynamicCenters;
	for (const auto& [actor, cls] : usable)
	{
		const PxBounds3 bounds = actor->getWorldBounds(1.0f);
		if (bounds.intersects(aRegion))
		{
			selected.emplace_back(actor, cls);
		}
		if (FiniteBounds(bounds))
		{
			const PxVec3 center = bounds.getCenter();
			(cls == AuditedClass::RigidStatic ? staticCenters : dynamicCenters).push_back({ center.x, center.y, center.z });
		}
	}
	census.staticCenters = SpreadOf(staticCenters);
	census.dynamicCenters = SpreadOf(dynamicCenters);
	dump.boundsSelected = uint32_t(selected.size());
	dump.selectMs = NowMs() - t0;

	// The same region through PhysX's own overlap query (scene-query shapes only), for comparison.
	t0 = NowMs();
	std::vector<PxOverlapHit> buffer(kOverlapBuffer);
	std::vector<PxOverlapHit> hits;
	CollectOverlaps           collect(buffer.data(), PxU32(buffer.size()), hits);
	aScene->overlap(PxBoxGeometry(aRegion.getExtents()), PxTransform(aRegion.getCenter()), collect,
	                PxQueryFilterData(PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::eNO_BLOCK));
	dump.overlapHits = uint32_t(hits.size());
	dump.overlapCapped = collect.capped;
	dump.overlapMs = NowMs() - t0;

	// Triangles of every shape of the selected actors.
	t0 = NowMs();
	for (const auto& [actor, cls] : selected)
	{
		if (CountTriangles(dump.triangles) >= aTriangleBudget)
		{
			dump.triangleCapHit = true;
			break;
		}
		const char*        kind = ActorKind(*actor, cls);
		const auto&        rigid = static_cast<const PxRigidActor&>(*actor);
		const PxTransform  pose = rigid.getGlobalPose();
		for (const PxShape* shape : ShapesOf(rigid))
		{
			const ObjectCheck check = CheckObject(memory_, build_->bases, shape);
			if (check.verdict != ObjectVerdict::Ok || check.cls != AuditedClass::Shape)
			{
				++dump.stats.unreadable;
				continue;
			}
			const PxGeometryType::Enum geometry = shape->getGeometryType();
			AppendTriangles(*shape, pose, aRegion, memory_, build_->bases,
			                dump.triangles[Category(kind, shape->getFlags(), geometry)], dump.stats);
			if (geometry == PxGeometryType::eCAPSULE && std::string_view(kind) != "static")
			{
				NoteCapsule(*shape, pose, aFeet, kind, dump.nearestCapsule);
			}
		}
	}
	if (CountTriangles(dump.triangles) > aTriangleBudget)
	{
		TrimToBudget(dump.triangles, aTriangleBudget);
		dump.triangleCapHit = true;
	}
	std::erase_if(dump.triangles, [](const auto& aEntry) { return aEntry.second.empty(); });
	dump.extractMs = NowMs() - t0;

	// Every capsule on a dynamic actor: character controllers are among them.
	for (const auto& [actor, cls] : usable)
	{
		if (cls != AuditedClass::RigidDynamic || census.capsules.size() >= kMaxCapsules)
		{
			continue;
		}
		const auto&       rigid = static_cast<const PxRigidActor&>(*actor);
		const PxTransform pose = rigid.getGlobalPose();
		const char*       kind = ActorKind(*actor, cls);
		for (const PxShape* shape : ShapesOf(rigid))
		{
			const ObjectCheck check = CheckObject(memory_, build_->bases, shape);
			PxCapsuleGeometry geometry;
			if (check.verdict != ObjectVerdict::Ok || check.cls != AuditedClass::Shape ||
			    shape->getGeometryType() != PxGeometryType::eCAPSULE || !shape->getCapsuleGeometry(geometry))
			{
				continue;
			}
			const PxTransform world = pose * shape->getLocalPose();
			const PxVec3      axis = world.q.rotate(PxVec3(1.0f, 0.0f, 0.0f));
			census.capsules.push_back({ std::format("{}", static_cast<const void*>(actor)),
			                            { world.p.x, world.p.y, world.p.z },
			                            { axis.x, axis.y, axis.z },
			                            geometry.radius,
			                            geometry.halfHeight,
			                            kind,
			                            FilterKey(shape->getQueryFilterData()),
			                            FilterKey(shape->getSimulationFilterData()) });
		}
	}

	aScene->unlockRead();
	return dump;
}

std::optional<float> PhysxSession::RaycastDown(PxScene* aScene, const PxVec3& aFrom, float aMaxDistance)
{
	aScene->lockRead();
	PxRaycastBuffer buffer;
	aScene->raycast(aFrom, PxVec3(0.0f, 0.0f, -1.0f), aMaxDistance, buffer, PxHitFlag::eDEFAULT,
	                PxQueryFilterData(PxQueryFlag::eSTATIC));
	aScene->unlockRead();
	return buffer.hasBlock ? std::optional<float>(buffer.block.distance) : std::nullopt;
}

std::optional<ControllerSample> PhysxSession::ReadController(const Vec3d& aFeet)
{
	memory_.Clear();  // memory changes from frame to frame
	if (!build_)
	{
		build_ = CheckBuild();
	}
	if (!build_->pass)
	{
		return std::nullopt;
	}
	const Checker checker{ memory_, build_->bases };

	// The scenes, each checked.
	PxPhysics&            physics = *build_->physics;
	std::vector<PxScene*> scenes(std::min<PxU32>(physics.getNbScenes(), kMaxScenes));
	scenes.resize(physics.getScenes(scenes.data(), PxU32(scenes.size())));
	std::erase_if(scenes, [&](PxScene* aScene) {
		const ObjectCheck check = CheckObject(memory_, build_->bases, aScene);
		return check.verdict != ObjectVerdict::Ok || check.cls != AuditedClass::Scene;
	});

	// One scene's dynamic actors, or only aOnly if it is still one of them.
	const auto search = [&](PxScene* aScene, const PxActor* aOnly) {
		std::optional<Candidate> best;
		aScene->lockRead();
		std::vector<PxActor*> actors(std::min<PxU32>(aScene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC), kMaxActorsPerScene));
		actors.resize(aScene->getActors(PxActorTypeFlag::eRIGID_DYNAMIC, actors.data(), PxU32(actors.size())));
		for (const PxActor* actor : actors)
		{
			if (!aOnly || actor == aOnly)
			{
				MeasureController(checker, aScene, actor, aFeet, best);
			}
		}
		aScene->unlockRead();
		return best;
	};

	if (controller_ && std::find(scenes.begin(), scenes.end(), controller_->scene) != scenes.end())
	{
		if (const std::optional<Candidate> again = search(controller_->scene, controller_->actor))
		{
			return again->sample;
		}
	}
	controller_.reset();
	std::optional<Candidate> best;
	for (PxScene* scene : scenes)
	{
		const std::optional<Candidate> found = search(scene, nullptr);
		if (found && (!best || found->across < best->across))
		{
			best = found;
		}
	}
	if (!best)
	{
		return std::nullopt;
	}
	controller_ = ControllerRef{ best->scene, static_cast<const PxActor*>(best->sample.actor) };
	best->sample.searched = true;
	return best->sample;
}
}
