#include "Harness.h"

#include "track/StateTracker.h"

#include <thread>

using cpmc::BoundTarget;
using cpmc::ListState;
using cpmc::StateTracker;
using cpmc::Targets;
using cpmc::ViewInfo;

namespace
{
BoundTarget Target(uintptr_t aResource, uint64_t aWidth, uint32_t aHeight, ViewInfo::Kind aKind = ViewInfo::Kind::Rtv)
{
	ViewInfo view;
	view.kind = aKind;
	view.resource = reinterpret_cast<void*>(aResource);
	view.width = aWidth;
	view.height = aHeight;
	return { aResource * 100, view };
}

Targets Gbuffer(uintptr_t aDepth)
{
	Targets targets;
	targets.rtvs = { Target(1, 1707, 960), Target(2, 1707, 960), Target(3, 1707, 960) };
	targets.dsv = Target(aDepth, 1707, 960, ViewInfo::Kind::Dsv);
	return targets;
}
}

TEST(StateTrackerFindsOnlyListsItSawReset)
{
	StateTracker tracker;
	int          listA = 0, listB = 0;
	CHECK(tracker.Find(&listA) == nullptr);
	ListState& a = tracker.ForReset(&listA);
	a.Begin(7, false);
	CHECK(tracker.Find(&listA) == &a);
	CHECK(tracker.Find(&listB) == nullptr);
	CHECK(tracker.Find(&listA)->Recording());
	CHECK_EQ(tracker.Find(&listA)->Frame(), 7u);
	a.End();
	CHECK(!a.Recording());
	CHECK(&tracker.ForReset(&listA) == &a);  // reused
}

TEST(StateTrackerFindsListsFromOtherThreads)
{
	StateTracker tracker;
	int          list = 0;
	tracker.ForReset(&list).Begin(1, false);
	ListState* seen = nullptr;
	std::thread other([&] { seen = tracker.Find(&list); });
	other.join();
	CHECK(seen == tracker.Find(&list));
}

TEST(ListStateRecordsPassesOnlyWhileLogging)
{
	ListState quiet;
	quiet.Begin(1, false);
	quiet.SetTargets(Gbuffer(9));
	quiet.Count(ListState::Counter::Draw);
	CHECK(quiet.Passes().empty());
	CHECK_EQ(quiet.CurrentTargets().rtvs.size(), size_t(3));  // targets are kept regardless

	ListState logged;
	logged.Begin(1, true);
	logged.Count(ListState::Counter::Dispatch);  // before any targets: a pass of its own
	logged.SetTargets(Gbuffer(9));
	logged.Count(ListState::Counter::IndexedDraw);
	logged.Count(ListState::Counter::IndexedDraw);
	logged.Count(ListState::Counter::Clear);
	logged.Event("GBuffer");
	logged.SetTargets({}, true);
	logged.Count(ListState::Counter::Indirect);
	CHECK_EQ(logged.Passes().size(), size_t(3));
	CHECK_EQ(logged.Passes()[0].dispatches, 1u);
	CHECK_EQ(logged.Passes()[1].indexedDraws, 2u);
	CHECK_EQ(logged.Passes()[1].clears, 1u);
	CHECK_STR(logged.Passes()[1].events[0], "GBuffer");
	CHECK(logged.Passes()[2].renderPass);
	CHECK_EQ(logged.Passes()[2].indirect, 1u);
}

TEST(ListStateFollowsTransitionsAndSplitBarriers)
{
	ListState state;
	state.Begin(1, false);
	int resource = 0;
	CHECK(!state.StateOf(&resource));
	state.Transition(&resource, 0x4, false);  // render target
	CHECK_EQ(*state.StateOf(&resource), 0x4u);
	state.Transition(&resource, 0x800, true);  // begun, not ended
	CHECK(!state.StateOf(&resource));
	state.Transition(&resource, 0x800, false);  // ended
	CHECK_EQ(*state.StateOf(&resource), 0x800u);
	state.Begin(2, false);
	CHECK(!state.StateOf(&resource));  // a new recording forgets
}

TEST(GbufferCandidatesNeedTheTaggedDepthAndMatchingTargets)
{
	CHECK(cpmc::IsGbufferCandidate(Gbuffer(9), reinterpret_cast<void*>(9)));
	CHECK(!cpmc::IsGbufferCandidate(Gbuffer(9), reinterpret_cast<void*>(8)));  // another depth buffer (a mirror)
	CHECK(!cpmc::IsGbufferCandidate(Gbuffer(9), nullptr));

	Targets two = Gbuffer(9);
	two.rtvs.pop_back();
	CHECK(!cpmc::IsGbufferCandidate(two, reinterpret_cast<void*>(9)));

	Targets small = Gbuffer(9);
	small.rtvs[1] = Target(2, 853, 480);
	CHECK(!cpmc::IsGbufferCandidate(small, reinterpret_cast<void*>(9)));

	Targets unknown = Gbuffer(9);
	unknown.rtvs[2].view.reset();
	CHECK(!cpmc::IsGbufferCandidate(unknown, reinterpret_cast<void*>(9)));
}
