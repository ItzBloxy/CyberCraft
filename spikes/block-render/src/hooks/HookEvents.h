#pragma once

#include "sl/Sl.h"
#include "track/StateTracker.h"

#include <d3d12.h>
#include <dxgi1_2.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace cpmc
{
// One DLSS super-resolution evaluate, just before Streamline runs it.
struct EvaluateCall
{
	uint32_t                   frame = 0;
	uint32_t                   viewport = 0;
	void*                      gameList = nullptr;  // as the game passed it: possibly a Streamline proxy
	ID3D12GraphicsCommandList* list = nullptr;      // the native list underneath; null if it couldn't be found
	std::vector<SlTag>         tags;                // the global tags, with local ones in their place
	std::optional<SlConstants> constants;           // this frame's, for this viewport
};

// What the hooks tell the plugin's features: the frame log, the dumps and the renderer. Every call comes from one of
// the game's threads, inside a D3D12 or Streamline call, so keep them short, and never fault. A feature that records
// commands into a game list uses d3d::Original(), so its commands never reach the tracker.
class HookEvents
{
public:
	virtual ~HookEvents() = default;

	virtual void OnFrameToken(uint32_t /*aFrame*/) {}
	virtual void OnConstants(uint32_t /*aFrame*/, uint32_t /*aViewport*/, const SlConstants& /*aConstants*/) {}
	virtual void OnTags(uint32_t /*aFrame*/, uint32_t /*aViewport*/, const std::vector<SlTag>& /*aTags*/, void* /*aList*/) {}
	virtual void OnEvaluate(const EvaluateCall& /*aCall*/) {}

	// A pass of a tracked list ends: the list is about to change render targets, close (aClosing), or has just ended
	// a render pass. aState.CurrentTargets() are still the pass's targets.
	virtual void OnPassEnd(ID3D12GraphicsCommandList* /*aList*/, ListState& /*aState*/, bool /*aClosing*/) {}
	virtual void OnClose(ID3D12GraphicsCommandList* /*aList*/, ListState& /*aState*/) {}

	virtual void OnSubmit(ID3D12CommandQueue* /*aQueue*/, UINT /*aCount*/, ID3D12CommandList* const* /*aLists*/) {}
	virtual void OnSubmitted(ID3D12CommandQueue* /*aQueue*/, UINT /*aCount*/, ID3D12CommandList* const* /*aLists*/) {}
	virtual void OnPresent(IDXGISwapChain* /*aSwapChain*/) {}
};
}
