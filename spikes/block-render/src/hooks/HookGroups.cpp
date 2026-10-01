#include "hooks/HookGroups.h"

#include <array>
#include <atomic>

namespace cpmc
{
namespace
{
constexpr std::array<std::string_view, 5> kNames = { "lists", "views", "submit", "present", "sl" };
std::array<std::atomic<bool>, 5>          g_on = { true, true, true, true, true };
}

std::optional<HookGroup> HookGroupNamed(std::string_view aName)
{
	for (size_t i = 0; i < kNames.size(); ++i)
	{
		if (kNames[i] == aName)
		{
			return HookGroup(i);
		}
	}
	return std::nullopt;
}

void SetHookGroup(HookGroup aGroup, bool aOn)
{
	g_on[size_t(aGroup)].store(aOn, std::memory_order_relaxed);
}

bool HookGroupOn(HookGroup aGroup)
{
	return g_on[size_t(aGroup)].load(std::memory_order_relaxed);
}

std::string HookGroupsSummary()
{
	std::string off;
	for (size_t i = 0; i < kNames.size(); ++i)
	{
		if (!g_on[i].load(std::memory_order_relaxed))
		{
			off += (off.empty() ? "" : ", ") + std::string(kNames[i]);
		}
	}
	return off.empty() ? "all hooks on" : "hooks off: " + off;
}
}
