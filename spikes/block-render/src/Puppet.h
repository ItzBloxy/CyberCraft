#pragma once

#include "Coords.h"
#include "LogQueue.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>

namespace RED4ext
{
struct CBaseFunction;
struct CClass;
}

// V moved and read through the game's own script functions, for build mode's drive (spike 5's "Part 1"), found by name
// as PlayerLocator finds V: TeleportationFacility.Teleport and V's GetWorldYaw place V; the global GetMountedVehicle,
// WorkspotGameSystem.IsActorInWorkspot and V's IsDead keep build mode off; and StatPoolsSystem.GetStatPoolValue reads V's
// health. Each function is found once and picked by its parameters' and return types, so an overload with other types,
// or a signature a patch changed, is logged and never called with the wrong arguments. Main thread only.
namespace cpmc
{
class Puppet
{
public:
	explicit Puppet(LogQueue& aLog) : log_(aLog) {}

	// Why V can't be placed: in a vehicle, in a workspot, dead, or the functions that place it missing. Empty: it can be,
	// or V isn't there, which the link reports.
	std::string           Blocked();
	std::optional<double> Yaw();     // V's, in degrees
	std::optional<double> Health();  // V's, in percent
	// V's feet to aFeet (world axes), facing aYaw degrees. False, with aError saying why, if V wasn't moved.
	bool Teleport(const Vec3d& aFeet, double aYaw, std::string& aError);

private:
	enum Function : size_t
	{
		kPlayer,
		kTeleport,
		kYaw,
		kVehicle,
		kWorkspot,
		kDead,
		kEntityId,
		kPool,
		kFunctions,
	};

	// The function, found and checked the first time it's asked for, and logged; null if the game hasn't it as these
	// calls need it. aClass: where to look (V's own class for V's functions; unused for a global function). Without it,
	// nothing is looked for, and it's asked again later.
	RED4ext::CBaseFunction* Find(Function aFunction, RED4ext::CClass* aClass);

	LogQueue&                                       log_;
	std::array<RED4ext::CBaseFunction*, kFunctions> found_{};
	std::array<bool, kFunctions>                    looked_{};
};
}
