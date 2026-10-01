#include "Harness.h"

#include "hooks/HookGroups.h"

using cpmc::HookGroup;

TEST(HookGroupsGoByTheirNames)
{
	CHECK(cpmc::HookGroupNamed("lists") == HookGroup::Lists);
	CHECK(cpmc::HookGroupNamed("views") == HookGroup::Views);
	CHECK(cpmc::HookGroupNamed("submit") == HookGroup::Submit);
	CHECK(cpmc::HookGroupNamed("present") == HookGroup::Present);
	CHECK(cpmc::HookGroupNamed("sl") == HookGroup::Streamline);
	CHECK(!cpmc::HookGroupNamed("everything"));
}

TEST(HookGroupsSwitchOffAndOnAgain)
{
	CHECK_STR(cpmc::HookGroupsSummary(), "all hooks on");
	cpmc::SetHookGroup(HookGroup::Views, false);
	cpmc::SetHookGroup(HookGroup::Submit, false);
	CHECK(!cpmc::HookGroupOn(HookGroup::Views));
	CHECK(cpmc::HookGroupOn(HookGroup::Lists));
	CHECK_STR(cpmc::HookGroupsSummary(), "hooks off: views, submit");
	cpmc::SetHookGroup(HookGroup::Views, true);
	cpmc::SetHookGroup(HookGroup::Submit, true);
	CHECK_STR(cpmc::HookGroupsSummary(), "all hooks on");
}
