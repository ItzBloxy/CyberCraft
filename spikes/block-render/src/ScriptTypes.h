#pragma once

#include <string>
#include <vector>

namespace RED4ext
{
struct CBaseFunction;
namespace rtti
{
struct IType;
}
}

// The game's script functions and fields, checked by their types' names before BlockRender calls or reads them (Puppet,
// GameScript): a call with other types than a function takes, or a field read as the wrong type, would fault, and the
// game freezes on any fault.
namespace cpmc
{
// Whether aType is named aAccepted; a name ending in '*' takes any type whose name starts with the rest.
bool TypeIs(const RED4ext::rtti::IType* aType, const char* aAccepted);
// Whether aFunction takes exactly as many parameters as aParams, each of one of the types named for it, and returns one
// of aReturns, or nothing when aReturns is empty.
bool Matches(const RED4ext::CBaseFunction& aFunction, const std::vector<std::vector<const char*>>& aParams,
             const std::vector<const char*>& aReturns);
// A type's name, or "?".
std::string TypeName(const RED4ext::rtti::IType* aType);
// "(handle:gameObject, Vector4) -> Bool", for the log.
std::string Describe(const RED4ext::CBaseFunction& aFunction);
}
