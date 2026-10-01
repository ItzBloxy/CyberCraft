#pragma once

#include "track/ViewRegistry.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace cpmc
{
// One bound render target or depth buffer: its descriptor handle, and the view the registry knew for it then.
struct BoundTarget
{
	uint64_t                handle = 0;
	std::optional<ViewInfo> view;
};

struct Targets
{
	std::vector<BoundTarget>   rtvs;
	std::optional<BoundTarget> dsv;
};

// What a command list did between two changes of render targets, recorded while a frame log runs.
struct PassRecord
{
	Targets                  targets;
	bool                     renderPass = false;  // begun with BeginRenderPass
	uint32_t                 draws = 0;           // DrawInstanced
	uint32_t                 indexedDraws = 0;    // DrawIndexedInstanced
	uint32_t                 indirect = 0;        // ExecuteIndirect
	uint32_t                 bundles = 0;         // ExecuteBundle
	uint32_t                 dispatches = 0;
	uint32_t                 clears = 0;
	uint32_t                 barriers = 0;          // ResourceBarrier calls
	uint32_t                 enhancedBarriers = 0;  // Barrier calls
	std::vector<std::string> events;              // event and marker names, at most 16
};

// One command list's state from its Reset to its Close. Only the thread recording the list touches it.
class ListState
{
public:
	enum class Counter
	{
		Draw,
		IndexedDraw,
		Indirect,
		Bundle,
		Dispatch,
		Clear,
		Barrier,
		EnhancedBarrier,
	};

	static constexpr uint32_t kUnknownState = 0xFFFFFFFF;

	void Begin(uint32_t aFrame, bool aLogging);
	void End();
	bool Recording() const { return recording_; }
	bool Logging() const { return logging_; }
	uint32_t Frame() const { return frame_; }

	void SetTargets(Targets aTargets, bool aRenderPass = false);
	const Targets& CurrentTargets() const { return targets_; }

	// The distinct target sets the list bound with three or more render targets and a depth buffer, in the order it
	// bound them: the first kMostWideTargets. Kept whether or not a log runs, so step 4 can tell the G-buffer's lists.
	static constexpr size_t kMostWideTargets = 4;
	const std::vector<Targets>& WideTargets() const { return wide_; }

	void Count(Counter aCounter);
	void Event(std::string aName);

	// D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, and the two D3D12_RESOURCE_BARRIER_FLAGS.
	static constexpr uint32_t kAllSubresources = 0xFFFFFFFF;
	static constexpr uint32_t kBeginOnly = 0x1;
	static constexpr uint32_t kEndOnly = 0x2;

	// A resource's first transition in the list. Its state before is the state the list found the resource in.
	struct FirstTransition
	{
		uint32_t before = 0;
		uint32_t subresource = kAllSubresources;
		uint32_t flags = 0;
	};

	// A transition barrier. Afterwards the resource's state is known, unless the barrier is a begin-only half (until
	// its end-only half) or covers a single subresource.
	void Transition(void* aResource, uint32_t aBefore, uint32_t aAfter, uint32_t aSubresource, uint32_t aFlags);
	std::optional<uint32_t> StateOf(void* aResource) const;
	std::optional<FirstTransition> FirstTransitionOf(void* aResource) const;
	const std::unordered_map<void*, FirstTransition>& FirstTransitions() const { return firsts_; }

	const std::vector<PassRecord>& Passes() const { return passes_; }

private:
	PassRecord* CurrentPass();

	bool                                       recording_ = false;
	bool                                       logging_ = false;
	uint32_t                                   frame_ = 0;
	Targets                                    targets_;
	std::vector<Targets>                       wide_;
	std::unordered_map<void*, uint32_t>        states_;
	std::unordered_map<void*, FirstTransition> firsts_;
	std::vector<PassRecord>                    passes_;
};

// Every command list's state, found by the list's address. A list's state lives as long as the tracker, so a pointer
// to it stays valid; a new list at an old list's address reuses it from its first Reset.
class StateTracker
{
public:
	StateTracker();

	ListState& ForReset(void* aList);  // creates the state on a list's first Reset
	ListState* Find(void* aList);      // null for a list never reset since tracking began

private:
	uint64_t                                              id_;  // tells trackers apart in each thread's cache
	std::shared_mutex                                     mutex_;
	std::unordered_map<void*, std::unique_ptr<ListState>> lists_;
};

// True when aTargets look like a G-buffer: three or more known render targets and a known depth buffer, all
// aWidth x aHeight, the render size (the tagged depth's). The depth buffer isn't the tagged depth: the game tags a
// copy of its own.
bool IsGbufferCandidate(const Targets& aTargets, uint64_t aWidth, uint32_t aHeight);
}
