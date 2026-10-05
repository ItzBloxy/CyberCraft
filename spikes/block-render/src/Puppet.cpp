#include "Puppet.h"

#include "ScriptTypes.h"

#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/Generated/EulerAngles.hpp>
#include <RED4ext/Scripting/Natives/Generated/Vector4.hpp>
#include <RED4ext/Scripting/Natives/Generated/ent/EntityID.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/StatsObjectID.hpp>

#include <cmath>
#include <format>
#include <type_traits>
#include <vector>

namespace cpmc
{
namespace
{
using RED4ext::CName;
using Strong = RED4ext::Handle<RED4ext::IScriptable>;
using Weak = RED4ext::WeakHandle<RED4ext::IScriptable>;

enum class Owner
{
	System,  // a game system's, by the system's script name
	Player,  // V's own class's
	Global,  // a global function, by its full name
};

// A function as these calls need it: each parameter one of the type names given, and the return type too (none: it
// returns nothing). A name ending in '*' takes any type whose name starts with the rest.
struct Signature
{
	Owner                                 owner;
	const char*                           system;
	const char*                           name;
	std::vector<std::vector<const char*>> params;
	std::vector<const char*>              returns;
};

// In Puppet::Function's order. The types are the ones the game's declarations and other mods' calls show:
// Teleport(objectToTeleport: ref<GameObject>, position: Vector4, orientation: EulerAngles) and
// GetStatPoolValue(StatsObjectID, gamedataStatPoolType, perc: Bool) -> Float. GetMountedVehicle's and
// IsActorInWorkspot's GameObject may be a ref or a wref, and the vehicle comes back as either.
const std::vector<Signature>& Signatures()
{
	static const std::vector<Signature> kSignatures{
		{ Owner::System, "PlayerSystem", "GetLocalPlayerMainGameObject", {}, { "handle:gameObject" } },
		{ Owner::System, "TeleportationFacility", "Teleport", { { "handle:gameObject" }, { "Vector4" }, { "EulerAngles" } }, {} },
		{ Owner::Player, nullptr, "GetWorldYaw", {}, { "Float" } },
		{ Owner::Global, nullptr, "GetMountedVehicle;GameObject", { { "handle:gameObject", "whandle:gameObject" } }, { "handle:*", "whandle:*" } },
		{ Owner::System, "WorkspotGameSystem", "IsActorInWorkspot", { { "handle:gameObject", "whandle:gameObject" } }, { "Bool" } },
		{ Owner::Player, nullptr, "IsDead", {}, { "Bool" } },
		{ Owner::Player, nullptr, "GetEntityID", {}, { "entEntityID" } },
		{ Owner::System, "StatPoolsSystem", "GetStatPoolValue", { { "gameStatsObjectID" }, { "gamedataStatPoolType" }, { "Bool" } }, { "Float" } },
	};
	return kSignatures;
}

RED4ext::CClass* ClassNamed(const char* aScriptName)
{
	auto* rtti = RED4ext::CRTTISystem::Get();
	return rtti ? rtti->GetClassByScriptName(aScriptName) : nullptr;
}

// The game's instance of the system with this script name, or null: the instance is checked here, since
// ExecuteFunction's own lookup by class calls on whatever GetSystem returns.
RED4ext::IScriptable* SystemNamed(const char* aScriptName)
{
	auto*            engine = RED4ext::CGameEngine::Get();
	RED4ext::CClass* type = ClassNamed(aScriptName);
	if (!type || !engine || !engine->framework || !engine->framework->gameInstance)
	{
		return nullptr;
	}
	return engine->framework->gameInstance->GetSystem(type);
}

// V, through PlayerSystem's GetLocalPlayerMainGameObject (aGet); null when V isn't there.
Strong LocalPlayer(RED4ext::CBaseFunction* aGet)
{
	RED4ext::IScriptable* system = SystemNamed("PlayerSystem");
	Strong                player;
	if (!system || !aGet || !RED4ext::ExecuteFunction(system, aGet, &player))
	{
		return {};
	}
	return player;
}

// V as aFunction's parameter aIndex takes it: a weak handle for a wref, the handle for a ref.
void* PlayerArg(const RED4ext::CBaseFunction& aFunction, uint32_t aIndex, Strong& aStrong, Weak& aWeak)
{
	return TypeIs(aFunction.params[aIndex]->type, "whandle:*") ? static_cast<void*>(&aWeak) : static_cast<void*>(&aStrong);
}

// A static or global function: no instance. Cast, so the overload that looks a system up by class isn't the one taken.
constexpr void* kNoInstance = nullptr;
}

RED4ext::CBaseFunction* Puppet::Find(Function aFunction, RED4ext::CClass* aClass)
{
	if (looked_[aFunction])
	{
		return found_[aFunction];
	}
	const std::vector<Signature>& signatures = Signatures();
	if (signatures.size() != kFunctions)
	{
		return nullptr;
	}
	const Signature& signature = signatures[aFunction];
	auto*            rtti = RED4ext::CRTTISystem::Get();
	if (!rtti || (signature.owner != Owner::Global && !aClass))
	{
		return nullptr;
	}
	std::string others;
	auto        consider = [&](RED4ext::CBaseFunction* aCandidate) {
		if (!aCandidate || found_[aFunction])
		{
			return;
		}
		if (Matches(*aCandidate, signature.params, signature.returns))
		{
			found_[aFunction] = aCandidate;
		}
		else
		{
			others += (others.empty() ? "" : "; ") + Describe(*aCandidate);
		}
	};
	if (signature.owner == Owner::Global)
	{
		consider(rtti->GetFunction(CName(signature.name)));
	}
	else
	{
		// From the class up, so an override is found before what it overrides; and any overload whose types fit.
		const CName name(signature.name);
		for (RED4ext::CClass* type = aClass; type && !found_[aFunction]; type = type->parent)
		{
			for (RED4ext::CClassStaticFunction* function : type->staticFuncs)
			{
				if (function && function->shortName == name)
				{
					consider(function);
				}
			}
			for (RED4ext::CClassFunction* function : type->funcs)
			{
				if (function && function->shortName == name)
				{
					consider(function);
				}
			}
		}
	}
	looked_[aFunction] = true;
	const std::string where = signature.owner == Owner::System   ? std::string(signature.system) + "."
	                          : signature.owner == Owner::Player ? "V's "
	                                                             : "";
	log_.Push(found_[aFunction] ? std::format("puppet: found {}{}{}", where, signature.name, Describe(*found_[aFunction]))
	                            : std::format("puppet: {}{} isn't there with the types these calls pass{}", where, signature.name,
	                                          others.empty() ? "" : "; the game has " + others));
	return found_[aFunction];
}

std::string Puppet::Blocked()
{
	Strong player = LocalPlayer(Find(kPlayer, ClassNamed("PlayerSystem")));
	if (!player)
	{
		return {};
	}
	RED4ext::CClass* const type = player.GetPtr()->GetType();
	if (!Find(kTeleport, ClassNamed("TeleportationFacility")))
	{
		return "the game's TeleportationFacility.Teleport isn't there (see the log)";
	}
	if (!Find(kYaw, type))
	{
		return "V's GetWorldYaw isn't there (see the log)";
	}
	Weak weak(player);
	if (RED4ext::CBaseFunction* const function = Find(kVehicle, nullptr))
	{
		RED4ext::StackArgs_t args{ { nullptr, PlayerArg(*function, 0, player, weak) } };
		bool                 inVehicle = false;
		if (TypeIs(function->returnType->type, "whandle:*"))
		{
			Weak vehicle;
			inVehicle = RED4ext::ExecuteFunction(kNoInstance, function, &vehicle, args) && vehicle.Lock();
		}
		else
		{
			Strong vehicle;
			inVehicle = RED4ext::ExecuteFunction(kNoInstance, function, &vehicle, args) && vehicle;
		}
		if (inVehicle)
		{
			return "V is in a vehicle";
		}
	}
	RED4ext::IScriptable* const workspots = SystemNamed("WorkspotGameSystem");
	if (RED4ext::CBaseFunction* const function = workspots ? Find(kWorkspot, ClassNamed("WorkspotGameSystem")) : nullptr)
	{
		RED4ext::StackArgs_t args{ { nullptr, PlayerArg(*function, 0, player, weak) } };
		bool                 inWorkspot = false;
		if (RED4ext::ExecuteFunction(workspots, function, &inWorkspot, args) && inWorkspot)
		{
			return "V is in a workspot";
		}
	}
	if (RED4ext::CBaseFunction* const function = Find(kDead, type))
	{
		bool dead = false;
		if (RED4ext::ExecuteFunction(player.GetPtr(), function, &dead) && dead)
		{
			return "V is dead";
		}
	}
	return {};
}

std::optional<double> Puppet::Yaw()
{
	const Strong                  player = LocalPlayer(Find(kPlayer, ClassNamed("PlayerSystem")));
	RED4ext::CBaseFunction* const function = player ? Find(kYaw, player.GetPtr()->GetType()) : nullptr;
	float                         yaw = 0.0f;
	if (!function || !RED4ext::ExecuteFunction(player.GetPtr(), function, &yaw) || !std::isfinite(yaw))
	{
		return std::nullopt;
	}
	return yaw;
}

std::optional<double> Puppet::Health()
{
	const Strong                  player = LocalPlayer(Find(kPlayer, ClassNamed("PlayerSystem")));
	RED4ext::IScriptable* const   pools = SystemNamed("StatPoolsSystem");
	RED4ext::CBaseFunction* const entityId = player ? Find(kEntityId, player.GetPtr()->GetType()) : nullptr;
	RED4ext::CBaseFunction* const value = pools ? Find(kPool, ClassNamed("StatPoolsSystem")) : nullptr;
	RED4ext::ent::EntityID        id{};
	if (!entityId || !value || !RED4ext::ExecuteFunction(player.GetPtr(), entityId, &id))
	{
		return std::nullopt;
	}
	// As redscript's Cast<StatsObjectID>(EntityID) makes it.
	RED4ext::game::StatsObjectID object{};
	object.entityHash = id.hash;
	object.idType = RED4ext::game::StatIDType::EntityID;
	int64_t              pool = 17;  // gamedataStatPoolType.Health; wide enough for the enum, whatever its size
	bool                 percent = true;
	float                health = 0.0f;
	RED4ext::StackArgs_t args{ { nullptr, &object }, { nullptr, &pool }, { nullptr, &percent } };
	if (!RED4ext::ExecuteFunction(pools, value, &health, args) || !std::isfinite(health))
	{
		return std::nullopt;
	}
	return health;
}

bool Puppet::Teleport(const Vec3d& aFeet, double aYaw, std::string& aError)
{
	Strong                        player = LocalPlayer(Find(kPlayer, ClassNamed("PlayerSystem")));
	RED4ext::IScriptable* const   facility = SystemNamed("TeleportationFacility");
	RED4ext::CBaseFunction* const function = facility ? Find(kTeleport, ClassNamed("TeleportationFacility")) : nullptr;
	if (!player)
	{
		aError = "V isn't found";
		return false;
	}
	if (!function)
	{
		aError = "the game's TeleportationFacility.Teleport isn't there (see the log)";
		return false;
	}
	if (!std::isfinite(aFeet.x) || !std::isfinite(aFeet.y) || !std::isfinite(aFeet.z) || !std::isfinite(aYaw))
	{
		aError = "the target isn't finite";
		return false;
	}
	RED4ext::Vector4     position(float(aFeet.x), float(aFeet.y), float(aFeet.z), 1.0f);
	RED4ext::EulerAngles orientation{ 0.0f, 0.0f, float(aYaw) };  // roll, pitch, yaw, as FreeFly passes them
	RED4ext::StackArgs_t args{ { nullptr, &player }, { nullptr, &position }, { nullptr, &orientation } };
	if (!RED4ext::ExecuteFunction(facility, function, nullptr, args))
	{
		aError = "the game's Teleport call failed";
		return false;
	}
	return true;
}
}
