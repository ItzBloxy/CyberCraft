#include "GameScript.h"

#include "ScriptTypes.h"

#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/Generated/Vector4.hpp>
#include <RED4ext/Scripting/Natives/Generated/ent/EntityID.hpp>

#include <format>
#include <initializer_list>
#include <utility>

namespace cpmc
{
namespace
{
using RED4ext::CName;
using Strong = RED4ext::Handle<RED4ext::IScriptable>;

constexpr const char* kClassName = "BlockRenderNpcs";

// A function as BlockRender calls it: its name, a static function's global name, and its parameters' and return types,
// as scripts/BlockRender.reds declares them. The game keeps a script class's static functions among the global
// functions, named "Class::Name;" and their parameters' types, not with the class (step 2's opening check).
struct FunctionTypes
{
	const char*                           name;
	const char*                           global;  // null for a member function, found in the class
	std::vector<std::vector<const char*>> params;
	std::vector<const char*>              returns;
};

// In GameScript::Function's order.
const std::vector<FunctionTypes>& Functions()
{
	static const std::vector<FunctionTypes> kFunctionTypes{
		{ "Version", "BlockRenderNpcs::Version;", {}, { "Int32" } },
		{ "Current", "BlockRenderNpcs::Current;", {}, { "handle:BlockRenderNpcs" } },
		{ "List", nullptr, { { "Float" } }, { "Int32" } },
		{ "Compare", nullptr, { { "Float" } }, { "String" } },
	};
	return kFunctionTypes;
}

// In GameScript::Field's order: the fields List fills, and their types.
const std::vector<std::pair<const char*, const char*>>& Fields()
{
	static const std::vector<std::pair<const char*, const char*>> kFieldTypes{
		{ "npcIds", "array:entEntityID" }, { "npcPositions", "array:Vector4" }, { "npcForwards", "array:Vector4" },
		{ "npcHealth", "array:Float" },    { "npcHealthMax", "array:Float" },   { "npcLevels", "array:Float" },
		{ "npcNames", "array:String" },    { "npcFlags", "array:Int32" },       { "attached", "Int32" },
		{ "found", "Int32" },              { "punch", "Float" },                { "punchItem", "String" },
		{ "vHealthMax", "Float" },
	};
	return kFieldTypes;
}

// BlockRender's natives (Natives.cpp), as the module declares them: added when the game registered its types.
constexpr std::array<const char*, 4> kNatives{ "BlockRenderNpcAttached", "BlockRenderNpcDetached", "BlockRenderNpcCount",
	                                           "BlockRenderNpcAt" };

// A static function's call: no instance.
constexpr void* kNoInstance = nullptr;

// The function aTypes names, with those types: a static one among the global functions, a member one in aClass. Null if
// there's none, with aOthers listing those there are by that name.
RED4ext::CBaseFunction* FindFunction(RED4ext::CClass& aClass, const FunctionTypes& aTypes, std::string& aOthers)
{
	const CName             name(aTypes.name);
	RED4ext::CBaseFunction* found = nullptr;
	const auto              consider = [&](RED4ext::CBaseFunction* aCandidate) {
		if (!aCandidate || found || aCandidate->shortName != name)
		{
			return;
		}
		if (Matches(*aCandidate, aTypes.params, aTypes.returns))
		{
			found = aCandidate;
		}
		else
		{
			aOthers += (aOthers.empty() ? "" : "; ") + Describe(*aCandidate);
		}
	};
	if (aTypes.global)
	{
		auto* const rtti = RED4ext::CRTTISystem::Get();
		consider(rtti ? rtti->GetFunction(CName(aTypes.global)) : nullptr);
	}
	else
	{
		for (RED4ext::CClassFunction* function : aClass.funcs)
		{
			consider(function);
		}
	}
	return found;
}

// The game session's system, through the module's Current(); none, with aWhy saying why.
Strong SessionSystem(RED4ext::CBaseFunction& aCurrent, RED4ext::CClass& aClass, std::string& aWhy)
{
	Strong system;
	if (!RED4ext::ExecuteFunction(kNoInstance, &aCurrent, &system) || !system)
	{
		aWhy = "the game session has no BlockRenderNpcs system";
		return {};
	}
	RED4ext::CClass* const type = system->GetType();
	if (!type || !type->IsA(&aClass))
	{
		aWhy = "BlockRenderNpcs.Current() gave something else";
		return {};
	}
	return system;
}

// A field of the module's system, as the type its check found it to be.
template<class T>
const T& FieldOf(RED4ext::CProperty& aField, RED4ext::IScriptable& aSystem)
{
	return *aField.GetValuePtr<T>(&aSystem);
}
}

const std::string& GameScript::Check()
{
	if (checked_)
	{
		return missing_;
	}
	checked_ = true;
	const std::vector<FunctionTypes>&                         functions = Functions();
	const std::vector<std::pair<const char*, const char*>>& fields = Fields();
	auto* const                                               rtti = RED4ext::CRTTISystem::Get();
	class_ = rtti ? rtti->GetClass(CName(kClassName)) : nullptr;
	if (functions.size() != kFunctions || fields.size() != kFields)
	{
		missing_ = "GameScript's tables don't match its enums";
	}
	else if (!class_)
	{
		missing_ = "the game has no BlockRenderNpcs, so redscript didn't compile BlockRender.reds: r6/logs/redscript_rCURRENT.log says why";
	}
	for (size_t i = 0; missing_.empty() && i < kFunctions; ++i)
	{
		std::string others;
		functions_[i] = FindFunction(*class_, functions[i], others);
		if (!functions_[i])
		{
			missing_ = std::format("BlockRenderNpcs.{} isn't there with the types BlockRender passes{}", functions[i].name,
			                       others.empty() ? "" : "; the module has " + others);
		}
		else if (i == kVersion)
		{
			int32_t version = 0;
			if (!RED4ext::ExecuteFunction(kNoInstance, functions_[kVersion], &version) || version != kModuleVersion)
			{
				missing_ = std::format("BlockRender.reds is version {}, and this BlockRender needs version {}: they come from different builds",
				                       version, kModuleVersion);
			}
		}
	}
	for (size_t i = 0; missing_.empty() && i < kFields; ++i)
	{
		RED4ext::CProperty* const field = class_->GetProperty(CName(fields[i].first));
		if (!field || !TypeIs(field->type, fields[i].second))
		{
			missing_ = std::format("BlockRenderNpcs.{} isn't there as {}{}", fields[i].first, fields[i].second,
			                       field ? ": it's " + TypeName(field->type) : std::string());
		}
		fields_[i] = field;
	}
	for (const char* native : kNatives)
	{
		if (missing_.empty() && !rtti->GetFunction(CName(native)))
		{
			missing_ = std::format("BlockRender's native {} isn't among the game's functions", native);
		}
	}
	log_.Push(missing_.empty() ? std::format("module: found BlockRenderNpcs version {}, its {} functions, the {} fields List fills, and "
	                                         "BlockRender's {} natives",
	                                         kModuleVersion, size_t(kFunctions), size_t(kFields), kNatives.size())
	                           : "module: " + missing_);
	return missing_;
}

std::optional<NpcList> GameScript::List(double aRange, std::string& aWhy)
{
	if (const std::string& missing = Check(); !missing.empty())
	{
		aWhy = missing;
		return std::nullopt;
	}
	const Strong system = SessionSystem(*functions_[kCurrent], *class_, aWhy);
	if (!system)
	{
		return std::nullopt;
	}
	float                range = float(aRange);
	int32_t              count = 0;
	RED4ext::StackArgs_t args{ { nullptr, &range } };
	if (!RED4ext::ExecuteFunction(system.GetPtr(), functions_[kList], &count, args))
	{
		aWhy = "BlockRenderNpcs.List failed";
		return std::nullopt;
	}
	if (count < 0)
	{
		aWhy = "the module doesn't find V";
		return std::nullopt;
	}
	RED4ext::IScriptable& instance = *system.GetPtr();
	const auto&           ids = FieldOf<RED4ext::DynArray<RED4ext::ent::EntityID>>(*fields_[kIds], instance);
	const auto&           positions = FieldOf<RED4ext::DynArray<RED4ext::Vector4>>(*fields_[kPositions], instance);
	const auto&           forwards = FieldOf<RED4ext::DynArray<RED4ext::Vector4>>(*fields_[kForwards], instance);
	const auto&           health = FieldOf<RED4ext::DynArray<float>>(*fields_[kHealth], instance);
	const auto&           healthMax = FieldOf<RED4ext::DynArray<float>>(*fields_[kHealthMax], instance);
	const auto&           levels = FieldOf<RED4ext::DynArray<float>>(*fields_[kLevels], instance);
	const auto&           names = FieldOf<RED4ext::DynArray<RED4ext::CString>>(*fields_[kNames], instance);
	const auto&           flags = FieldOf<RED4ext::DynArray<int32_t>>(*fields_[kFlags], instance);
	const uint32_t        size = uint32_t(count);
	for (const uint32_t length : { ids.Size(), positions.Size(), forwards.Size(), health.Size(), healthMax.Size(), levels.Size(),
	                               names.Size(), flags.Size() })
	{
		if (length != size)
		{
			aWhy = "the module's lists disagree in length";
			return std::nullopt;
		}
	}
	NpcList list;
	list.npcs.resize(size);
	for (uint32_t i = 0; i < size; ++i)
	{
		link::NpcSample& npc = list.npcs[i];
		npc.entity = ids[i].hash;
		npc.position = { positions[i].X, positions[i].Y, positions[i].Z };
		npc.forward = { forwards[i].X, forwards[i].Y, forwards[i].Z };
		npc.health = health[i];
		npc.healthMax = healthMax[i];
		npc.level = levels[i];
		npc.name = names[i].c_str();
		npc.flags = uint32_t(flags[i]);
	}
	list.attached = FieldOf<int32_t>(*fields_[kAttached], instance);
	list.found = FieldOf<int32_t>(*fields_[kFound], instance);
	list.punch = FieldOf<float>(*fields_[kPunch], instance);
	list.punchItem = FieldOf<RED4ext::CString>(*fields_[kPunchItem], instance).c_str();
	list.vHealthMax = FieldOf<float>(*fields_[kVHealthMax], instance);
	return list;
}

std::string GameScript::Compare(double aRange)
{
	if (const std::string& missing = Check(); !missing.empty())
	{
		return missing;
	}
	std::string  why;
	const Strong system = SessionSystem(*functions_[kCurrent], *class_, why);
	if (!system)
	{
		return why;
	}
	float                range = float(aRange);
	RED4ext::CString     result;
	RED4ext::StackArgs_t args{ { nullptr, &range } };
	if (!RED4ext::ExecuteFunction(system.GetPtr(), functions_[kCompare], &result, args))
	{
		return "BlockRenderNpcs.Compare failed";
	}
	return result.c_str();
}
}
