#include "physx/Session.h"

#include <Windows.h>

#include <format>

using namespace physx;

namespace cpmc::px
{
namespace
{
constexpr PxU32 kMostScenes = 16;

// PhysX's tolerances as a real build has them: length from 0.01 to 100, speed from 0.1 to 1,000.
bool Plausible(const PxTolerancesScale& aScale)
{
	return aScale.length >= 0.01f && aScale.length <= 100.0f && aScale.speed >= 0.1f && aScale.speed <= 1000.0f;
}

std::string Narrow(const wchar_t* aText)
{
	std::string out;
	for (; *aText; ++aText)
	{
		out.push_back(char(*aText));  // the DLLs' names are ASCII
	}
	return out;
}
}

bool Session::CheckBuild(std::string& aWhy)
{
	if (physics_)
	{
		return true;
	}
	HMODULE modules[2] = {};
	for (size_t i = 0; i < 2; ++i)
	{
		const AuditedModule& audited = kAuditedModules[i];
		modules[i] = GetModuleHandleW(audited.name);
		if (!modules[i])
		{
			aWhy = Narrow(audited.name) + " isn't loaded";
			return false;
		}
		const std::optional<ModuleId> id = ModuleIdentity(modules[i]);
		if (!id || !(*id == audited.id))
		{
			aWhy = std::format("{} isn't the audited build: {}", Narrow(audited.name),
			                   id ? std::format("timestamp 0x{:x}, image size 0x{:x}", id->timestamp, id->imageSize) : "no PE header");
			return false;
		}
	}
	const ModuleBases bases{ reinterpret_cast<uintptr_t>(modules[0]), reinterpret_cast<uintptr_t>(modules[1]) };
	using GetPhysicsFn = PxPhysics& (*)();
	const auto getPhysics = reinterpret_cast<GetPhysicsFn>(GetProcAddress(modules[0], "PxGetPhysics"));
	if (!getPhysics)
	{
		aWhy = "PhysX3_x64.dll doesn't export PxGetPhysics";
		return false;
	}
	PxPhysics* const  physics = &getPhysics();
	const ObjectCheck check = CheckObject(memory_, bases, physics);
	if (check.verdict != ObjectVerdict::Ok || check.cls != AuditedClass::Physics)
	{
		aWhy = "PxGetPhysics's object isn't the audited PxPhysics";
		return false;
	}
	const PxTolerancesScale& scale = physics->getTolerancesScale();
	if (!memory_.Range(&scale, sizeof scale) || !Plausible(scale))
	{
		aWhy = "PhysX's tolerances aren't readable, or not a real build's";
		return false;
	}
	bases_ = bases;
	physics_ = physics;
	return true;
}

PxScene* Session::Scene0(std::string& aWhy)
{
	memory_.Clear();  // memory changes from frame to frame
	if (!CheckBuild(aWhy))
	{
		return nullptr;
	}
	const PxU32 count = physics_->getNbScenes();
	if (count == 0 || count > kMostScenes)
	{
		aWhy = std::format("PhysX has {} scenes", count);
		return nullptr;
	}
	PxScene* scenes[kMostScenes] = {};
	if (physics_->getScenes(scenes, kMostScenes) == 0 || !scenes[0])
	{
		aWhy = "PhysX lists no scene";
		return nullptr;
	}
	const ObjectCheck check = CheckObject(memory_, bases_, scenes[0]);
	if (check.verdict != ObjectVerdict::Ok || check.cls != AuditedClass::Scene)
	{
		aWhy = "scene 0 isn't the audited PxScene";
		return nullptr;
	}
	return scenes[0];
}
}
