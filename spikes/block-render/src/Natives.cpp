#include "Natives.h"

#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/Generated/ent/EntityID.hpp>

#include <Windows.h>

#include <initializer_list>
#include <utility>

namespace cpmc
{
namespace
{
NpcRegistry g_reported;
CombatInbox g_combat;
LogQueue*   g_nativesLog = nullptr;

// BlockRenderNpcAttached(id: EntityID), from the wrap of NPCPuppet.OnGameAttached, on any thread.
void NpcAttached(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, void*, int64_t)
{
	RED4ext::ent::EntityID id;
	RED4ext::GetParameter(aFrame, &id);
	++aFrame->code;  // past the parameters' end
	g_reported.Attached(id.hash, GetCurrentThreadId());
}

// BlockRenderNpcDetached(id: EntityID), from the wrap of NPCPuppet.OnDetach, on any thread.
void NpcDetached(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, void*, int64_t)
{
	RED4ext::ent::EntityID id;
	RED4ext::GetParameter(aFrame, &id);
	++aFrame->code;
	g_reported.Detached(id.hash, GetCurrentThreadId());
}

// BlockRenderNpcCount() -> Int32, from List: copies the list, and gives how many it holds.
void NpcCount(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, int32_t* aOut, int64_t)
{
	++aFrame->code;
	const int32_t count = g_reported.Copy();
	if (aOut)
	{
		*aOut = count;
	}
}

// BlockRenderNpcAt(i: Int32) -> EntityID, from List: the copy's i-th ID, or no entity's outside it.
void NpcAt(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::ent::EntityID* aOut, int64_t)
{
	int32_t index = -1;
	RED4ext::GetParameter(aFrame, &index);
	++aFrame->code;
	if (aOut)
	{
		aOut->hash = g_reported.At(index);
	}
}

// BlockRenderVHurt(amount: Float, attackType: Int32, instigator: EntityID, nonLethal: Bool) -> Bool, from the wrap of
// DealDamages, on any thread: a hit on V, taken (true) while combat and build mode are on, so the wrap deals none of it;
// one the game won't let kill V stays V's.
void VHurt(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
{
	float                  amount = 0.0f;
	int32_t                attackType = 0;
	RED4ext::ent::EntityID instigator;
	bool                   nonLethal = false;
	RED4ext::GetParameter(aFrame, &amount);
	RED4ext::GetParameter(aFrame, &attackType);
	RED4ext::GetParameter(aFrame, &instigator);
	RED4ext::GetParameter(aFrame, &nonLethal);
	++aFrame->code;
	const bool taken = g_combat.Offer({ double(amount), attackType, instigator.hash, nonLethal });
	if (aOut)
	{
		*aOut = taken;
	}
}

// BlockRenderHitLanded(target: EntityID, amount: Float, before: Float, after: Float, flags: Int32), from the wrap of
// DealDamages, on any thread: one of our hits, dealt: what the pipeline made of it, the NPC's health before and after,
// and what the pipeline did to it (link::LandedFlag).
void HitLanded(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, void*, int64_t)
{
	RED4ext::ent::EntityID target;
	float                  amount = 0.0f;
	float                  before = 0.0f;
	float                  after = 0.0f;
	int32_t                flags = 0;
	RED4ext::GetParameter(aFrame, &target);
	RED4ext::GetParameter(aFrame, &amount);
	RED4ext::GetParameter(aFrame, &before);
	RED4ext::GetParameter(aFrame, &after);
	RED4ext::GetParameter(aFrame, &flags);
	++aFrame->code;
	g_combat.Land({ target.hash, double(amount), double(before), double(after), uint32_t(flags) });
}

// BlockRenderHitEnded(target: EntityID, why: Int32), from the wrap of ProcessPipeline, on any thread: one of our hits, dealt
// nothing, and why (link::HitEndedName).
void HitEnded(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, void*, int64_t)
{
	RED4ext::ent::EntityID target;
	int32_t                why = 0;
	RED4ext::GetParameter(aFrame, &target);
	RED4ext::GetParameter(aFrame, &why);
	++aFrame->code;
	g_combat.End({ target.hash, why });
}

// A global native function, with its parameters' and return types as the module declares them; false if a type isn't
// there or the function can't be made.
template<class Out>
bool Add(RED4ext::CRTTISystem& aRtti, const char* aName, RED4ext::ScriptingFunction_t<Out> aHandler,
         std::initializer_list<std::pair<const char*, const char*>> aParams, const char* aReturns)
{
	RED4ext::CGlobalFunction* const function = RED4ext::CGlobalFunction::Create(aName, aName, aHandler);
	if (!function)
	{
		return false;
	}
	function->flags = {};
	function->flags.isNative = true;
	function->flags.isStatic = true;
	function->flags.isThreadsafe = true;  // the lock makes them so
	bool typed = true;
	for (const auto& [type, name] : aParams)
	{
		typed = function->AddParam(type, name) && typed;
	}
	if (aReturns)
	{
		typed = function->SetReturnType(aReturns) && typed;
	}
	aRtti.RegisterFunction(function);
	return typed;
}

void RegisterTypes()
{
}

void PostRegisterTypes()
{
	RED4ext::CRTTISystem* const rtti = RED4ext::CRTTISystem::Get();
	if (!rtti)
	{
		if (g_nativesLog)
		{
			g_nativesLog->Push("natives: NOT added: the game's type system isn't there");
		}
		return;
	}
	bool typed = Add<void*>(*rtti, "BlockRenderNpcAttached", &NpcAttached, { { "entEntityID", "id" } }, nullptr);
	typed = Add<void*>(*rtti, "BlockRenderNpcDetached", &NpcDetached, { { "entEntityID", "id" } }, nullptr) && typed;
	typed = Add<int32_t*>(*rtti, "BlockRenderNpcCount", &NpcCount, {}, "Int32") && typed;
	typed = Add<RED4ext::ent::EntityID*>(*rtti, "BlockRenderNpcAt", &NpcAt, { { "Int32", "i" } }, "entEntityID") && typed;
	typed = Add<bool*>(*rtti, "BlockRenderVHurt", &VHurt,
	                   { { "Float", "amount" }, { "Int32", "attackType" }, { "entEntityID", "instigator" }, { "Bool", "nonLethal" } }, "Bool") &&
	        typed;
	typed = Add<void*>(*rtti, "BlockRenderHitLanded", &HitLanded,
	                   { { "entEntityID", "target" }, { "Float", "amount" }, { "Float", "before" }, { "Float", "after" }, { "Int32", "flags" } },
	                   nullptr) &&
	        typed;
	typed = Add<void*>(*rtti, "BlockRenderHitEnded", &HitEnded, { { "entEntityID", "target" }, { "Int32", "why" } }, nullptr) && typed;
	if (g_nativesLog)
	{
		g_nativesLog->Push(typed ? "natives: BlockRenderNpcAttached, BlockRenderNpcDetached, BlockRenderNpcCount, BlockRenderNpcAt, "
		                           "BlockRenderVHurt, BlockRenderHitLanded and BlockRenderHitEnded added"
		                         : "natives: added, but some of their types weren't found");
	}
}
}

bool RegisterNatives(LogQueue& aLog)
{
	g_nativesLog = &aLog;
	RED4ext::CRTTISystem* const rtti = RED4ext::CRTTISystem::Get();
	if (!rtti)
	{
		return false;
	}
	rtti->AddRegisterCallback(&RegisterTypes);
	rtti->AddPostRegisterCallback(&PostRegisterTypes);
	return true;
}

NpcRegistry& NpcsReported()
{
	return g_reported;
}

CombatInbox& CombatReported()
{
	return g_combat;
}
}
