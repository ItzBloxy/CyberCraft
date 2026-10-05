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
constexpr int32_t kModuleVersion = 5;  // BlockRenderNpcs.Version(), as this BlockRender needs it

// What the module's List gives.
struct NpcList
{
	std::vector<link::NpcSample> npcs;          // within the range asked for, and not dead
	int32_t                      attached = 0;  // NPCs in the world: on the list the wraps report
	int32_t                      found = 0;     // of those, found by their EntityIDs
	float                        punch = 0.0f;  // V's punch, read from punchItem, what V's unarmed slot draws
	std::string                  punchItem;
	bool                         punchDrawn = false;  // V has it drawn, so hits carry it as their weapon
	float                        vHealthMax = 0.0f;
};

// What the module's HideV did: the meshes hidden, or shown again (-1 without Codeware, -2 V isn't found). Hiding: the
// effects stopped since V was hidden, and the game's crosshair (1 hidden, -1 not found among the HUD layer's game
// controllers). Showing: the effects started again, and the crosshair (1 shown again, 0 HideV hadn't hidden it).
struct VLook
{
	int32_t meshes = 0;
	int32_t effects = 0;
	int32_t crosshair = 0;

	bool operator==(const VLook&) const = default;
};

class GameScript
{
public:
	explicit GameScript(LogQueue& aLog) : log_(aLog) {}

	// The NPCs within aRange metres of V. Nothing, with aWhy saying why, if the module can't be called or read.
	std::optional<NpcList> List(double aRange, std::string& aWhy);
	// What the game's targeting finds within aRange metres of V, and which of those the list lacks: for the npcs request.
	std::string Compare(double aRange);
	// Combat (spike 6's "Part 2"). One of Minecraft's hits on the NPC aEntity, as V's: aAmount where the game's damage
	// pipeline starts it, the strong attack if aStrong. The module's Hit says what became of it (link::HitResultName).
	// Nothing, with aWhy saying why, if the module can't be called.
	std::optional<int32_t> Hit(uint64_t aEntity, double aAmount, bool aStrong, std::string& aWhy);
	// V's unarmed weapon drawn (aOn), or V's own weapon put back: the module's Arm says how it went (1 asked, 2 nothing to
	// ask, 0 V isn't found, -1 no unarmed weapon, -2 no equipment system).
	std::optional<int32_t> Arm(bool aOn, std::string& aWhy);
	// V hidden (aOn), or shown again: what the module's HideV did, with its meshes, effects and the game's crosshair.
	std::optional<VLook> HideV(bool aOn, std::string& aWhy);
	// V's Immortal, the game's override under BlockRender's own source name: on or off. Whether the game then says V is
	// Immortal.
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
		kArm,
		kHideV,
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
		kPunchDrawn,
		kVHealthMax,
		kVEffects,
		kVCrosshair,
		kFields,
	};

	// A member function of the module's taking a Bool and giving an Int32: Arm and HideV.
	std::optional<int32_t> Switch(Function aFunction, const char* aName, bool aOn, std::string& aWhy);

	LogQueue&                                       log_;
	bool                                            checked_ = false;
	std::string                                     missing_;  // why the module can't be used; empty: it can
	RED4ext::CClass*                                class_ = nullptr;
	std::array<RED4ext::CBaseFunction*, kFunctions> functions_{};
	std::array<RED4ext::CProperty*, kFields>        fields_{};
};
}
