#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>

// Small D3D12 helpers the features share.
namespace cpmc::gpu
{
using Microsoft::WRL::ComPtr;

// Where one subresource of a texture lands in a buffer: the footprint D3D12 gives for a copy at offset 0.
struct Footprint
{
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
	uint32_t                           rows = 0;
	uint64_t                           rowBytes = 0;
	uint64_t                           totalBytes = 0;
};

// The plane index a D3D12 subresource number needs: mip 0 and array slice 0 of plane aPlane.
uint32_t PlaneSubresource(const D3D12_RESOURCE_DESC& aDesc, uint32_t aPlane);

// How many planes the resource's format has (2 for depth-stencil formats, else 1).
uint32_t PlaneCount(ID3D12Device* aDevice, DXGI_FORMAT aFormat);

Footprint FootprintOf(ID3D12Device* aDevice, const D3D12_RESOURCE_DESC& aDesc, uint32_t aSubresource);

// A buffer in aHeap (default or readback), in aState, with aFlags (spike 7's Part 5: unordered access for the atlas).
ComPtr<ID3D12Resource> MakeBuffer(ID3D12Device* aDevice, uint64_t aBytes, D3D12_HEAP_TYPE aHeap, D3D12_RESOURCE_STATES aState,
                                  D3D12_RESOURCE_FLAGS aFlags = D3D12_RESOURCE_FLAG_NONE);

// Records into aList, which may be the game's: a copy of subresource aSubresource of aSource into aDest at offset 0,
// laid out as aFootprint. Unless aState already allows copying, aSource moves from aState to the copy state and back
// (all subresources), through the original ResourceBarrier so the tracker doesn't see it. False, with nothing
// recorded, if aList isn't a direct list and aState includes a state only direct lists may use.
bool RecordCopy(ID3D12GraphicsCommandList* aList, ID3D12Resource* aSource, uint32_t aSubresource, uint32_t aState,
                ID3D12Resource* aDest, const Footprint& aFootprint);
}
