#pragma once

#include "LogQueue.h"
#include "link/Actors.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace RED4ext
{
struct CBaseFunction;
struct CClass;
struct CProperty;
}

// BlockRender's redscript module, scripts/BlockRender.reds (spike 6's "The module's list"), called from C++. Its class,
// BlockRenderNpcs, is checked once, the first time it's needed: its version; its functions, picked by their types as
// Puppet's are; and the fields List fills, by their types. Its system is the game session's, found through its static
// Current() on every call, since a load makes a new one. Main thread only, while V is found.
namespace cpmc
{
constexpr int32_t kModuleVersion = 3;  // BlockRenderNpcs.Version(), as this BlockRender needs it

// What the module's List gives.
struct NpcList
{
	std::vector<link::NpcSample> npcs;          // within the range asked for, and not dead
	int32_t                      attached = 0;  // NPCs in the world: on the list the wraps report
	int32_t                      found = 0;     // of those, found by their EntityIDs
	float                        punch = 0.0f;  // V's punch, read from punchItem
	std::string                  punchItem;
	float                        vHealthMax = 0.0f;
};

class GameScript
{
public:
	explicit GameScript(LogQueue& aLog) : log_(aLog) {}

	// The NPCs within aRange metres of V. Nothing, with aWhy saying why, if the module can't be called or read.
	std::optional<NpcList> List(double aRange, std::string& aWhy);
	// What the game's targeting finds within aRange metres of V, and which of those the list lacks: for the npcs request.
	std::string Compare(double aRange);
	// Combat (spike 6's "Part 2"). One of Minecraft's hits on the NPC aEntity, as V's: aAmount off its health, the strong
	// attack if aStrong. The module's Hit says what became of it (link::HitResultName). Nothing, with aWhy saying why, if
	// the module can't be called.
	std::optional<int32_t> Hit(uint64_t aEntity, double aAmount, bool aStrong, std::string& aWhy);
	// V's Immortal, under BlockRender's own source name: on or off. Whether the game says it changed.
	std::optional<bool> SetImmortal(bool aOn, std::string& aWhy);
	// Minecraft's death, as V's, killed by aKiller (an entity, or 0). False if V isn't found or another god mode keeps V
	// alive.
	std::optional<bool> KillV(uint64_t aKiller, std::string& aWhy);
	// V's punch, what it's read from, and what hits are made with: for the punch request.
	std::string Punch();
	// Checks the class, its version, functions and fields, and BlockRender's natives, the first time, and logs what it
	// found. Empty: all there. List and Compare check first; BlockRender checks before its first List too, and writes the
	// log, so a fault in the module's first call still leaves the check's line.
	const std::string& Check();

private:
	enum Function : size_t
	{
		kVersion,
		kCurrent,
		kList,
		kCompare,
		kHit,
		kSetImmortal,
		kKillV,
		kPunchReport,
		kFunctions,
	};

	enum Field : size_t
	{
		kIds,
		kPositions,
		kForwards,
		kHealth,
		kHealthMax,
		kLevels,
		kNames,
		kFlags,
		kAttached,
		kFound,
		kPunch,
		kPunchItem,
		kVHealthMax,
		kFields,
	};

	LogQueue&                                       log_;
	bool                                            checked_ = false;
	std::string                                     missing_;  // why the module can't be used; empty: it can
	RED4ext::CClass*                                class_ = nullptr;
	std::array<RED4ext::CBaseFunction*, kFunctions> functions_{};
	std::array<RED4ext::CProperty*, kFields>        fields_{};
};
}
