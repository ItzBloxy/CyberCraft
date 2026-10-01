/* Compiled as C, where the Windows SDK declares each COM interface as a struct of function pointers: the vtable slot
   of a method is its offset in that struct over the size of a pointer. tests/TestSlots.cpp compares these numbers with
   src/hooks/Slots.h. */
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_2.h>

#include <stddef.h>

#define SLOT(vtbl, method) (offsetof(vtbl, method) / sizeof(void*))

size_t SlotsC_Device(int aWhich)
{
	switch (aWhich)
	{
	case 0: return SLOT(ID3D12DeviceVtbl, CreateRenderTargetView);
	case 1: return SLOT(ID3D12DeviceVtbl, CreateDepthStencilView);
	case 2: return SLOT(ID3D12DeviceVtbl, CopyDescriptors);
	case 3: return SLOT(ID3D12DeviceVtbl, CopyDescriptorsSimple);
	default: return 0;
	}
}

size_t SlotsC_Queue(void)
{
	return SLOT(ID3D12CommandQueueVtbl, ExecuteCommandLists);
}

size_t SlotsC_List(int aWhich)
{
	switch (aWhich)
	{
	case 0: return SLOT(ID3D12GraphicsCommandList7Vtbl, Close);
	case 1: return SLOT(ID3D12GraphicsCommandList7Vtbl, Reset);
	case 2: return SLOT(ID3D12GraphicsCommandList7Vtbl, DrawInstanced);
	case 3: return SLOT(ID3D12GraphicsCommandList7Vtbl, DrawIndexedInstanced);
	case 4: return SLOT(ID3D12GraphicsCommandList7Vtbl, Dispatch);
	case 5: return SLOT(ID3D12GraphicsCommandList7Vtbl, ResourceBarrier);
	case 6: return SLOT(ID3D12GraphicsCommandList7Vtbl, ExecuteBundle);
	case 7: return SLOT(ID3D12GraphicsCommandList7Vtbl, OMSetRenderTargets);
	case 8: return SLOT(ID3D12GraphicsCommandList7Vtbl, ClearDepthStencilView);
	case 9: return SLOT(ID3D12GraphicsCommandList7Vtbl, ClearRenderTargetView);
	case 10: return SLOT(ID3D12GraphicsCommandList7Vtbl, SetMarker);
	case 11: return SLOT(ID3D12GraphicsCommandList7Vtbl, BeginEvent);
	case 12: return SLOT(ID3D12GraphicsCommandList7Vtbl, EndEvent);
	case 13: return SLOT(ID3D12GraphicsCommandList7Vtbl, ExecuteIndirect);
	case 14: return SLOT(ID3D12GraphicsCommandList7Vtbl, BeginRenderPass);
	case 15: return SLOT(ID3D12GraphicsCommandList7Vtbl, EndRenderPass);
	case 16: return SLOT(ID3D12GraphicsCommandList7Vtbl, Barrier);
	default: return 0;
	}
}

size_t SlotsC_Dxgi(int aWhich)
{
	switch (aWhich)
	{
	case 0: return SLOT(IDXGIFactory2Vtbl, CreateSwapChain);
	case 1: return SLOT(IDXGIFactory2Vtbl, CreateSwapChainForHwnd);
	case 2: return SLOT(IDXGISwapChain1Vtbl, Present);
	case 3: return SLOT(IDXGISwapChain1Vtbl, Present1);
	default: return 0;
	}
}
