#pragma once

#include "track/StateTracker.h"

#include <d3d12.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Drawing before DLSS (step 3): the parts that need no GPU. Our list goes just before the game's DLSS list, and draws
// into the tagged scaling input colour, motion vectors and depth.
namespace cpmc
{
// The checks before every draw (the spec's "Safety"): the colour must be an R16G16B16A16_FLOAT render target, the
// motion vectors an R16G16_FLOAT render target, and the depth a 32-bit depth buffer, as step 1 found them. All three
// must be the same size, and plain 2D textures with one subresource each. aWhy says what failed.
bool CheckPredlssTargets(const D3D12_RESOURCE_DESC& aColour, const D3D12_RESOURCE_DESC& aMotion, const D3D12_RESOURCE_DESC& aDepth,
                         std::string& aWhy);

// The format of a depth-stencil view of a depth buffer in aFormat: D32_FLOAT for R32_TYPELESS or D32_FLOAT, UNKNOWN for
// anything else.
DXGI_FORMAT DepthViewFormat(DXGI_FORMAT aFormat);

// A tagged buffer's state just before aList, the DLSS list: the state its first transition in aList starts from, or,
// if aList never moves it, its state at the evaluate (aTaggedState). Nothing if that state is unknown, or if the
// first transition ends a split barrier or covers another subresource than the buffer's only one.
std::optional<uint32_t> StateBeforeList(const ListState& aList, void* aResource, uint32_t aTaggedState);

// A resource moving from one state to another.
struct Move
{
	void*    resource = nullptr;
	uint32_t before = 0;
	uint32_t after = 0;
};

// The moves that take each resource to the state wanted, leaving out those already in it: D3D12 refuses a transition
// to the state a resource is in.
std::vector<Move> MovesInto(const std::vector<Move>& aWanted);
// The moves that put them back, in reverse order.
std::vector<Move> MovesBack(const std::vector<Move>& aMoves);
// Whole-resource transition barriers for aMoves.
std::vector<D3D12_RESOURCE_BARRIER> Barriers(const std::vector<Move>& aMoves);
}
