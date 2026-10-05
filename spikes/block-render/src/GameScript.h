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
constexpr int32_t kModuleVersion = 1;  // BlockRenderNpcs.Version(), as this BlockRender needs it

// What the module's List gives.
struct NpcList
{
	std::vector<link::NpcSample> npcs;          // within the range asked for, and not dead
	int32_t                      attached = 0;  // NPCs in the world
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

private:
	enum Function : size_t
	{
		kVersion,
		kCurrent,
		kList,
		kCompare,
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
		kPunch,
		kPunchItem,
		kVHealthMax,
		kFields,
	};

	// Checks the class, its version, functions and fields the first time, and logs what it found. Empty: all there.
	const std::string& Check();

	LogQueue&                                       log_;
	bool                                            checked_ = false;
	std::string                                     missing_;  // why the module can't be used; empty: it can
	RED4ext::CClass*                                class_ = nullptr;
	std::array<RED4ext::CBaseFunction*, kFunctions> functions_{};
	std::array<RED4ext::CProperty*, kFields>        fields_{};
};
}
