#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace cpmc
{
// Groups of hooks. `set hooks.<name> 0` makes a group's hooks call their originals and do nothing else, so the game
// can show what each group costs: run 4 found the plugin doubled the frame time with drawing off.
enum class HookGroup
{
	Lists,       // "lists": the command list calls the tracker follows
	Views,       // "views": the device's view creation and descriptor copies
	Submit,      // "submit": ExecuteCommandLists
	Present,     // "present": Present and Present1
	Streamline,  // "sl": the four Streamline imports
};

// The group called aName, if there is one.
std::optional<HookGroup> HookGroupNamed(std::string_view aName);
void                     SetHookGroup(HookGroup aGroup, bool aOn);
bool                     HookGroupOn(HookGroup aGroup);  // one relaxed atomic read: cheap enough for every call
std::string              HookGroupsSummary();             // "all hooks on", or "hooks off: lists, views"
}
