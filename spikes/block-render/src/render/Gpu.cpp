#include "render/Gpu.h"

#include "hooks/D3D12Hooks.h"

#include <utility>

namespace cpmc::gpu
{
uint32_t PlaneSubresource(const D3D12_RESOURCE_DESC& aDesc, uint32_t aPlane)
{
	const uint32_t arraySize = aDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1u : aDesc.DepthOrArraySize;
	return aPlane * aDesc.MipLevels * arraySize;
}

uint32_t PlaneCount(ID3D12Device* aDevice, DXGI_FORMAT aFormat)
{
	D3D12_FEATURE_DATA_FORMAT_INFO info{ aFormat, 0 };
	if (FAILED(aDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &info, sizeof(info))) || info.PlaneCount == 0)
	{
		return 1;
	}
	return info.PlaneCount;
}

Footprint FootprintOf(ID3D12Device* aDevice, const D3D12_RESOURCE_DESC& aDesc, uint32_t aSubresource)
{
	Footprint footprint;
	UINT      rows = 0;
	UINT64    rowBytes = 0;
	UINT64    total = 0;
	aDevice->GetCopyableFootprints(&aDesc, aSubresource, 1, 0, &footprint.layout, &rows, &rowBytes, &total);
	footprint.rows = rows;
	footprint.rowBytes = rowBytes;
	footprint.totalBytes = total;
	return footprint;
}

ComPtr<ID3D12Resource> MakeBuffer(ID3D12Device* aDevice, uint64_t aBytes, D3D12_HEAP_TYPE aHeap, D3D12_RESOURCE_STATES aState)
{
	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = aHeap;
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = aBytes;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ComPtr<ID3D12Resource> buffer;
	if (FAILED(aDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, aState, nullptr, IID_PPV_ARGS(&buffer))))
	{
		return nullptr;
	}
	return buffer;
}

bool RecordCopy(ID3D12GraphicsCommandList* aList, ID3D12Resource* aSource, uint32_t aSubresource, uint32_t aState,
                ID3D12Resource* aDest, const Footprint& aFootprint)
{
	constexpr uint32_t kDirectOnly = D3D12_RESOURCE_STATE_INDEX_BUFFER | D3D12_RESOURCE_STATE_RENDER_TARGET | D3D12_RESOURCE_STATE_DEPTH_WRITE |
	                                 D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
	                                 D3D12_RESOURCE_STATE_STREAM_OUT | D3D12_RESOURCE_STATE_RESOLVE_DEST | D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
	const bool transition = (aState & D3D12_RESOURCE_STATE_COPY_SOURCE) == 0;
	if (transition && (aState & kDirectOnly) != 0 && aList->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		return false;
	}
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = aSource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATES(aState);
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	if (transition)
	{
		d3d::Original().resourceBarrier(aList, 1, &barrier);
	}
	D3D12_TEXTURE_COPY_LOCATION to{};
	to.pResource = aDest;
	to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	to.PlacedFootprint = aFootprint.layout;
	D3D12_TEXTURE_COPY_LOCATION from{};
	from.pResource = aSource;
	from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	from.SubresourceIndex = aSubresource;
	aList->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
	if (transition)
	{
		std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
		d3d::Original().resourceBarrier(aList, 1, &barrier);
	}
	return true;
}
}
