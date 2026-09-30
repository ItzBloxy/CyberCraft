#include "PlayerLocator.h"

#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/Generated/Transform.hpp>
#include <RED4ext/Scripting/Natives/Generated/Vector4.hpp>

#include <cmath>

namespace cpmc
{
namespace
{
RED4ext::CClass* ClassByScriptName(const char* aName)
{
	auto* rtti = RED4ext::CRTTISystem::Get();
	return rtti ? rtti->GetClassByScriptName(aName) : nullptr;
}

bool GameReady()
{
	auto* engine = RED4ext::CGameEngine::Get();
	return engine && engine->framework && engine->framework->gameInstance;
}

// Calls a no-argument script function on an object, if the object's class (or a parent) has it.
template<class Out>
bool CallOn(RED4ext::IScriptable* aObject, const char* aFunction, Out& aOut)
{
	auto* function = aObject->GetType()->GetFunction(aFunction);
	return function && RED4ext::ExecuteFunction(aObject, function, &aOut);
}

float Length(const RED4ext::Vector4& aVector)
{
	return std::sqrt(aVector.X * aVector.X + aVector.Y * aVector.Y + aVector.Z * aVector.Z);
}

std::optional<CameraPose> LocateCamera(const Vec3d& aFeet)
{
	auto* cameraSystem = ClassByScriptName("CameraSystem");
	if (!cameraSystem)
	{
		return std::nullopt;
	}
	RED4ext::Transform transform{};
	bool               haveTransform = false;
	RED4ext::Vector4   forward{};
	float              fov = 0.0f;
	if (!RED4ext::ExecuteFunction(cameraSystem, "GetActiveCameraWorldTransform", &haveTransform, transform) || !haveTransform ||
	    !RED4ext::ExecuteFunction(cameraSystem, "GetActiveCameraForward", &forward) ||
	    !RED4ext::ExecuteFunction(cameraSystem, "GetActiveCameraFOV", &fov))
	{
		return std::nullopt;
	}
	const Vec3d position{ transform.position.X, transform.position.Y, transform.position.Z };
	const double dx = position.x - aFeet.x, dy = position.y - aFeet.y, dz = position.z - aFeet.z;
	// A camera far from V (a scene camera, a vehicle camera) or a bad read is not V's eye.
	if (std::sqrt(dx * dx + dy * dy + dz * dz) > 5.0 || Length(forward) < 0.5f || !(fov > 10.0f && fov < 170.0f))
	{
		return std::nullopt;
	}
	return CameraPose{ position, { forward.X, forward.Y, forward.Z }, fov };
}
}

std::optional<PlayerPose> LocatePlayer()
{
	if (!GameReady())
	{
		return std::nullopt;
	}
	auto* playerSystem = ClassByScriptName("PlayerSystem");
	if (!playerSystem)
	{
		return std::nullopt;
	}
	RED4ext::Handle<RED4ext::IScriptable> player;
	if (!RED4ext::ExecuteFunction(playerSystem, "GetLocalPlayerMainGameObject", &player) || !player)
	{
		return std::nullopt;
	}
	RED4ext::Vector4 position{};
	RED4ext::Vector4 forward{};
	if (!CallOn(player.GetPtr(), "GetWorldPosition", position) || !CallOn(player.GetPtr(), "GetWorldForward", forward))
	{
		return std::nullopt;
	}
	PlayerPose pose;
	pose.feet = { position.X, position.Y, position.Z };
	pose.forward = { forward.X, forward.Y, forward.Z };
	pose.camera = LocateCamera(pose.feet);
	return pose;
}
}
