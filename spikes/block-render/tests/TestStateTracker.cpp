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
	constexpr uint32_t all = ListState::kAllSubresources;
	ListState state;
	state.Begin(1, false);
	int resource = 0;
	CHECK(!state.StateOf(&resource));
	state.Transition(&resource, 0x0, 0x4, all, 0);  // common to render target
	CHECK_EQ(*state.StateOf(&resource), 0x4u);
	state.Transition(&resource, 0x4, 0x800, all, ListState::kBeginOnly);  // begun, not ended
	CHECK(!state.StateOf(&resource));
	state.Transition(&resource, 0x4, 0x800, all, ListState::kEndOnly);  // ended
	CHECK_EQ(*state.StateOf(&resource), 0x800u);
	state.Transition(&resource, 0x800, 0x8, 0, 0);  // one subresource: the whole resource's state is unknown
	CHECK(!state.StateOf(&resource));
	state.Begin(2, false);
	CHECK(!state.StateOf(&resource));  // a new recording forgets
}

TEST(ListStateKeepsEachResourcesFirstTransition)
{
	constexpr uint32_t all = ListState::kAllSubresources;
	ListState state;
	state.Begin(1, false);
	int colour = 0, depth = 0, untouched = 0;
	state.Transition(&colour, 0x4, 0x8, all, 0);   // render target to unordered access
	state.Transition(&colour, 0x8, 0xC0, all, 0);  // a later transition doesn't change the first
	state.Transition(&depth, 0x10, 0xC0, 0, ListState::kBeginOnly);
	const auto colourFirst = state.FirstTransitionOf(&colour);
	CHECK(colourFirst.has_value());
	CHECK_EQ(colourFirst->before, 0x4u);
	CHECK_EQ(colourFirst->subresource, all);
	CHECK_EQ(colourFirst->flags, 0u);
	const auto depthFirst = state.FirstTransitionOf(&depth);
	CHECK(depthFirst.has_value());
	CHECK_EQ(depthFirst->before, 0x10u);
	CHECK_EQ(depthFirst->subresource, 0u);
	CHECK_EQ(depthFirst->flags, ListState::kBeginOnly);
	CHECK(!state.FirstTransitionOf(&untouched));
	CHECK_EQ(state.FirstTransitions().size(), size_t(2));
	state.Begin(2, false);
	CHECK(!state.FirstTransitionOf(&colour));  // a new recording forgets
}

TEST(GbufferCandidatesNeedThreeTargetsAndADepthAtRenderSize)
{
	CHECK(cpmc::IsGbufferCandidate(Gbuffer(9), 1707, 960));
	CHECK(!cpmc::IsGbufferCandidate(Gbuffer(9), 2560, 1440));  // not the render size
	CHECK(!cpmc::IsGbufferCandidate(Gbuffer(9), 0, 0));        // no depth tag yet

	Targets two = Gbuffer(9);
	two.rtvs.pop_back();
	CHECK(!cpmc::IsGbufferCandidate(two, 1707, 960));

	Targets small = Gbuffer(9);
	small.rtvs[1] = Target(2, 853, 480);
	CHECK(!cpmc::IsGbufferCandidate(small, 1707, 960));

	Targets unknown = Gbuffer(9);
	unknown.rtvs[2].view.reset();
	CHECK(!cpmc::IsGbufferCandidate(unknown, 1707, 960));

	Targets noDepth = Gbuffer(9);
	noDepth.dsv.reset();
	CHECK(!cpmc::IsGbufferCandidate(noDepth, 1707, 960));

	Targets nullTarget = Gbuffer(9);  // the game's forward passes bind a null target, which has no size
	nullTarget.rtvs[1] = Target(0, 0, 0);
	CHECK(!cpmc::IsGbufferCandidate(nullTarget, 1707, 960));
}
