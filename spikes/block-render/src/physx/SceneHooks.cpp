#include "physx/SceneHooks.h"

#include "hooks/VtablePatch.h"

#include <Windows.h>

#include <array>
#include <atomic>
#include <format>

using namespace physx;

namespace cpmc::px
{
namespace
{
std::atomic<PxScene*>          g_scene{ nullptr };  // scene 0: the only one whose calls are recorded
std::atomic<bool>              g_inUpdate{ false };
SceneRecorder                  g_recorder;
VtablePatcher                  g_patcher;
bool                           g_installed = false;
uint64_t                       g_installedAt = 0;  // GetTickCount64() as the hooks went in
std::array<void*, kSceneCalls> g_originals{};      // each slot's function, which its hook calls on

// How long after the hooks go in the count starts (SceneRecorder::Start). A thread holds the write lock for a moment at
// most, since the game's own reads wait for it.
constexpr uint64_t kSettleMs = 1000;

template<class Fn>
Fn Original(SceneCall aCall)
{
	return reinterpret_cast<Fn>(g_originals[size_t(aCall)]);
}

bool Watched(PxScene* aThis)
{
	return aThis == g_scene.load(std::memory_order_relaxed);
}

void Record(PxScene* aThis, SceneCall aCall)
{
	if (Watched(aThis))
	{
		g_recorder.Note(aCall, GetCurrentThreadId(), g_inUpdate.load(std::memory_order_relaxed));
	}
}

// One hook a slot, each with its function's own parameters, as PxScene.h declares them. On x64, a member function takes
// its object first, as a free function takes its first parameter, so the hooks can call the originals this way.
void AddActor(PxScene* aThis, PxActor& aActor)
{
	Record(aThis, SceneCall::AddActor);
	Original<void (*)(PxScene*, PxActor&)>(SceneCall::AddActor)(aThis, aActor);
}

void AddActorsPruning(PxScene* aThis, const PxPruningStructure& aPruning)
{
	Record(aThis, SceneCall::AddActorsPruning);
	Original<void (*)(PxScene*, const PxPruningStructure&)>(SceneCall::AddActorsPruning)(aThis, aPruning);
}

void AddActors(PxScene* aThis, PxActor* const* aActors, PxU32 aCount)
{
	Record(aThis, SceneCall::AddActors);
	Original<void (*)(PxScene*, PxActor* const*, PxU32)>(SceneCall::AddActors)(aThis, aActors, aCount);
}

void RemoveActor(PxScene* aThis, PxActor& aActor, bool aWake)
{
	Record(aThis, SceneCall::RemoveActor);
	Original<void (*)(PxScene*, PxActor&, bool)>(SceneCall::RemoveActor)(aThis, aActor, aWake);
}

void RemoveActors(PxScene* aThis, PxActor* const* aActors, PxU32 aCount, bool aWake)
{
	Record(aThis, SceneCall::RemoveActors);
	Original<void (*)(PxScene*, PxActor* const*, PxU32, bool)>(SceneCall::RemoveActors)(aThis, aActors, aCount, aWake);
}

void AddAggregate(PxScene* aThis, PxAggregate& aAggregate)
{
	Record(aThis, SceneCall::AddAggregate);
	Original<void (*)(PxScene*, PxAggregate&)>(SceneCall::AddAggregate)(aThis, aAggregate);
}

void RemoveAggregate(PxScene* aThis, PxAggregate& aAggregate, bool aWake)
{
	Record(aThis, SceneCall::RemoveAggregate);
	Original<void (*)(PxScene*, PxAggregate&, bool)>(SceneCall::RemoveAggregate)(aThis, aAggregate, aWake);
}

void Simulate(PxScene* aThis, PxReal aTime, PxBaseTask* aTask, void* aScratch, PxU32 aScratchSize, bool aControl)
{
	Record(aThis, SceneCall::Simulate);
	Original<void (*)(PxScene*, PxReal, PxBaseTask*, void*, PxU32, bool)>(SceneCall::Simulate)(aThis, aTime, aTask, aScratch,
	                                                                                           aScratchSize, aControl);
}

void Advance(PxScene* aThis, PxBaseTask* aTask)
{
	Record(aThis, SceneCall::Advance);
	Original<void (*)(PxScene*, PxBaseTask*)>(SceneCall::Advance)(aThis, aTask);
}

void Collide(PxScene* aThis, PxReal aTime, PxBaseTask* aTask, void* aScratch, PxU32 aScratchSize, bool aControl)
{
	Record(aThis, SceneCall::Collide);
	Original<void (*)(PxScene*, PxReal, PxBaseTask*, void*, PxU32, bool)>(SceneCall::Collide)(aThis, aTime, aTask, aScratch,
	                                                                                          aScratchSize, aControl);
}

bool FetchResults(PxScene* aThis, bool aBlock, PxU32* aError)
{
	Record(aThis, SceneCall::FetchResults);
	return Original<bool (*)(PxScene*, bool, PxU32*)>(SceneCall::FetchResults)(aThis, aBlock, aError);
}

bool FetchResultsStart(PxScene* aThis, const PxContactPairHeader*& aPairs, PxU32& aCount, bool aBlock)
{
	Record(aThis, SceneCall::FetchResultsStart);
	return Original<bool (*)(PxScene*, const PxContactPairHeader*&, PxU32&, bool)>(SceneCall::FetchResultsStart)(aThis, aPairs,
	                                                                                                             aCount, aBlock);
}

void FetchResultsFinish(PxScene* aThis, PxU32* aError)
{
	Record(aThis, SceneCall::FetchResultsFinish);
	Original<void (*)(PxScene*, PxU32*)>(SceneCall::FetchResultsFinish)(aThis, aError);
}

// The queries are const members, which take their object first all the same.
bool Raycast(PxScene* aThis, const PxVec3& aOrigin, const PxVec3& aDirection, const PxReal aDistance, PxRaycastCallback& aHits,
             PxHitFlags aFlags, const PxQueryFilterData& aFilter, PxQueryFilterCallback* aFilterCall, const PxQueryCache* aCache)
{
	Record(aThis, SceneCall::Raycast);
	return Original<bool (*)(PxScene*, const PxVec3&, const PxVec3&, const PxReal, PxRaycastCallback&, PxHitFlags, const PxQueryFilterData&,
	                         PxQueryFilterCallback*, const PxQueryCache*)>(SceneCall::Raycast)(aThis, aOrigin, aDirection, aDistance, aHits,
	                                                                                           aFlags, aFilter, aFilterCall, aCache);
}

bool Sweep(PxScene* aThis, const PxGeometry& aGeometry, const PxTransform& aPose, const PxVec3& aDirection, const PxReal aDistance,
           PxSweepCallback& aHits, PxHitFlags aFlags, const PxQueryFilterData& aFilter, PxQueryFilterCallback* aFilterCall,
           const PxQueryCache* aCache, const PxReal aInflation)
{
	Record(aThis, SceneCall::Sweep);
	return Original<bool (*)(PxScene*, const PxGeometry&, const PxTransform&, const PxVec3&, const PxReal, PxSweepCallback&, PxHitFlags,
	                         const PxQueryFilterData&, PxQueryFilterCallback*, const PxQueryCache*, const PxReal)>(SceneCall::Sweep)(
	    aThis, aGeometry, aPose, aDirection, aDistance, aHits, aFlags, aFilter, aFilterCall, aCache, aInflation);
}

bool Overlap(PxScene* aThis, const PxGeometry& aGeometry, const PxTransform& aPose, PxOverlapCallback& aHits,
             const PxQueryFilterData& aFilter, PxQueryFilterCallback* aFilterCall)
{
	Record(aThis, SceneCall::Overlap);
	return Original<bool (*)(PxScene*, const PxGeometry&, const PxTransform&, PxOverlapCallback&, const PxQueryFilterData&,
	                         PxQueryFilterCallback*)>(SceneCall::Overlap)(aThis, aGeometry, aPose, aHits, aFilter, aFilterCall);
}

void LockWrite(PxScene* aThis, const char* aFile, PxU32 aLine)
{
	Record(aThis, SceneCall::LockWrite);
	Original<void (*)(PxScene*, const char*, PxU32)>(SceneCall::LockWrite)(aThis, aFile, aLine);
	if (Watched(aThis))
	{
		g_recorder.Lock(GetCurrentThreadId(), 1);  // held from here
	}
}

void UnlockWrite(PxScene* aThis)
{
	Record(aThis, SceneCall::UnlockWrite);
	if (Watched(aThis))
	{
		g_recorder.Lock(GetCurrentThreadId(), -1);
	}
	Original<void (*)(PxScene*)>(SceneCall::UnlockWrite)(aThis);
}

void ShiftOrigin(PxScene* aThis, const PxVec3& aShift)
{
	Record(aThis, SceneCall::ShiftOrigin);
	if (Watched(aThis))
	{
		g_recorder.NoteShift(GetCurrentThreadId(), g_inUpdate.load(std::memory_order_relaxed), aShift.x, aShift.y, aShift.z);
	}
	Original<void (*)(PxScene*, const PxVec3&)>(SceneCall::ShiftOrigin)(aThis, aShift);
}

// In SceneCall's order.
const std::array<void*, kSceneCalls> kHooks = {
	reinterpret_cast<void*>(&AddActor),
	reinterpret_cast<void*>(&AddActorsPruning),
	reinterpret_cast<void*>(&AddActors),
	reinterpret_cast<void*>(&RemoveActor),
	reinterpret_cast<void*>(&RemoveActors),
	reinterpret_cast<void*>(&AddAggregate),
	reinterpret_cast<void*>(&RemoveAggregate),
	reinterpret_cast<void*>(&Simulate),
	reinterpret_cast<void*>(&Advance),
	reinterpret_cast<void*>(&Collide),
	reinterpret_cast<void*>(&FetchResults),
	reinterpret_cast<void*>(&FetchResultsStart),
	reinterpret_cast<void*>(&FetchResultsFinish),
	reinterpret_cast<void*>(&Raycast),
	reinterpret_cast<void*>(&Sweep),
	reinterpret_cast<void*>(&Overlap),
	reinterpret_cast<void*>(&LockWrite),
	reinterpret_cast<void*>(&UnlockWrite),
	reinterpret_cast<void*>(&ShiftOrigin),
};
}

bool InstallSceneHooks(PxScene* aScene, uintptr_t aPhysx3, std::string& aWhy)
{
	if (g_installed)
	{
		return true;
	}
	void* const* const vtable = *reinterpret_cast<void* const* const*>(aScene);
	for (size_t i = 0; i < kSceneCalls; ++i)
	{
		const SceneCallInfo& info = InfoOf(SceneCall(i));
		if (reinterpret_cast<uintptr_t>(vtable[info.slot]) != aPhysx3 + info.rva)
		{
			aWhy = std::format("scene slot {} ({}) doesn't hold the audited function: another hook may be there", info.slot, info.name);
			return false;
		}
	}
	g_recorder.Reset();
	g_scene.store(aScene);
	for (size_t i = 0; i < kSceneCalls; ++i)
	{
		const uint32_t slot = InfoOf(SceneCall(i)).slot;
		g_originals[i] = vtable[slot];  // before the slot points at the hook that calls it
		if (!g_patcher.Patch(aScene, slot, kHooks[i]))
		{
			g_patcher.RestoreAll();
			aWhy = std::format("scene slot {} couldn't be written", slot);
			return false;
		}
	}
	g_installedAt = GetTickCount64();
	g_installed = true;
	return true;
}

void RemoveSceneHooks()
{
	g_patcher.RestoreAll();
	g_installed = false;
}

bool SceneHooksInstalled()
{
	return g_installed;
}

void SetInUpdate(bool aIn)
{
	g_inUpdate.store(aIn, std::memory_order_relaxed);
	if (aIn && g_installed && !g_recorder.Started() && GetTickCount64() - g_installedAt >= kSettleMs)
	{
		g_recorder.Start();
	}
}

const SceneRecorder& Recorder()
{
	return g_recorder;
}
}
