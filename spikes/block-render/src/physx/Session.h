#pragma once

#include "Readable.h"
#include "physx/GameAbi.h"
#include "physx/PxHeaders.h"

#include <string>

// The game's PhysX, through spike 1's checks (spike 5's "The exporter" and "The scene watch"): the two DLLs with the
// audited identities, PxGetPhysics's object, and scene 0, Night City's, each checked against the audit before anything
// is called on it. Main thread only.
namespace cpmc::px
{
class Session
{
public:
	// Scene 0, checked; null, with aWhy saying why, if PhysX isn't the audited build or has no scene 0 now.
	physx::PxScene* Scene0(std::string& aWhy);

	const ModuleBases& Bases() const { return bases_; }  // where the audited DLLs are, once Scene0 has found them
	ReadableMemory&    Memory() { return memory_; }

private:
	// The DLLs' identities, PxGetPhysics and its object, and the tolerances: once they pass, they're kept.
	bool CheckBuild(std::string& aWhy);

	ModuleBases       bases_;
	physx::PxPhysics* physics_ = nullptr;
	ReadableMemory    memory_;
};
}
