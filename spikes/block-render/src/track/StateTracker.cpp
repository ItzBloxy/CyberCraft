#include "track/StateTracker.h"

#include <atomic>
#include <mutex>

namespace cpmc
{
namespace
{
constexpr size_t kMaxEvents = 16;

// The last list each thread looked up. A thread records one list at a time, so this saves taking the lock on almost
// every call.
struct Cached
{
	uint64_t   tracker = 0;
	void*      list = nullptr;
	ListState* state = nullptr;
};
thread_local Cached t_cached;

std::atomic<uint64_t> g_nextTrackerId{ 1 };
}

void ListState::Begin(uint32_t aFrame, bool aLogging)
{
	recording_ = true;
	logging_ = aLogging;
	frame_ = aFrame;
	targets_ = {};
	states_.clear();
	firsts_.clear();
	passes_.clear();
}

void ListState::End()
{
	recording_ = false;
}

PassRecord* ListState::CurrentPass()
{
	if (!logging_)
	{
		return nullptr;
	}
	if (passes_.empty())
	{
		passes_.emplace_back();  // work before the first render-target change
	}
	return &passes_.back();
}

void ListState::SetTargets(Targets aTargets, bool aRenderPass)
{
	targets_ = std::move(aTargets);
	if (logging_)
	{
		PassRecord pass;
		pass.targets = targets_;
		pass.renderPass = aRenderPass;
		passes_.push_back(std::move(pass));
	}
}

void ListState::Count(Counter aCounter)
{
	PassRecord* pass = CurrentPass();
	if (!pass)
	{
		return;
	}
	switch (aCounter)
	{
	case Counter::Draw: ++pass->draws; break;
	case Counter::IndexedDraw: ++pass->indexedDraws; break;
	case Counter::Indirect: ++pass->indirect; break;
	case Counter::Bundle: ++pass->bundles; break;
	case Counter::Dispatch: ++pass->dispatches; break;
	case Counter::Clear: ++pass->clears; break;
	case Counter::Barrier: ++pass->barriers; break;
	case Counter::EnhancedBarrier: ++pass->enhancedBarriers; break;
	}
}

void ListState::Event(std::string aName)
{
	PassRecord* pass = CurrentPass();
	if (pass && pass->events.size() < kMaxEvents)
	{
		pass->events.push_back(std::move(aName));
	}
}

void ListState::Transition(void* aResource, uint32_t aBefore, uint32_t aAfter, uint32_t aSubresource, uint32_t aFlags)
{
	firsts_.try_emplace(aResource, FirstTransition{ aBefore, aSubresource, aFlags });
	const bool known = aSubresource == kAllSubresources && (aFlags & kBeginOnly) == 0;
	states_[aResource] = known ? aAfter : kUnknownState;
}

std::optional<uint32_t> ListState::StateOf(void* aResource) const
{
	const auto found = states_.find(aResource);
	if (found == states_.end() || found->second == kUnknownState)
	{
		return std::nullopt;
	}
	return found->second;
}

std::optional<ListState::FirstTransition> ListState::FirstTransitionOf(void* aResource) const
{
	const auto found = firsts_.find(aResource);
	if (found == firsts_.end())
	{
		return std::nullopt;
	}
	return found->second;
}

StateTracker::StateTracker()
	: id_(g_nextTrackerId++)
{
}

ListState& StateTracker::ForReset(void* aList)
{
	std::unique_lock lock(mutex_);
	auto&            state = lists_[aList];
	if (!state)
	{
		state = std::make_unique<ListState>();
	}
	t_cached = { id_, aList, state.get() };
	return *state;
}

ListState* StateTracker::Find(void* aList)
{
	if (t_cached.tracker == id_ && t_cached.list == aList)
	{
		return t_cached.state;
	}
	std::shared_lock lock(mutex_);
	const auto       found = lists_.find(aList);
	if (found == lists_.end())
	{
		return nullptr;
	}
	t_cached = { id_, aList, found->second.get() };
	return found->second.get();
}

bool IsGbufferCandidate(const Targets& aTargets, uint64_t aWidth, uint32_t aHeight)
{
	const auto renderSized = [&](const std::optional<ViewInfo>& aView) {
		return aView && aView->width == aWidth && aView->height == aHeight;
	};
	if (aWidth == 0 || aHeight == 0 || !aTargets.dsv || !renderSized(aTargets.dsv->view) || aTargets.rtvs.size() < 3)
	{
		return false;
	}
	for (const BoundTarget& target : aTargets.rtvs)
	{
		if (!renderSized(target.view))
		{
			return false;
		}
	}
	return true;
}
}
