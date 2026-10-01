#include "hooks/D3D12Hooks.h"

#include "hooks/Slots.h"
#include "hooks/StreamlineHooks.h"
#include "hooks/Submission.h"
#include "hooks/VtablePatch.h"
#include "log/EventNames.h"

#include <d3d12sdklayers.h>
#include <wrl/client.h>

#include <array>
#include <atomic>
#include <cstring>
#include <format>
#include <map>
#include <mutex>

namespace cpmc::d3d
{
namespace
{
using Microsoft::WRL::ComPtr;

using D3D12CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
using D3D12GetDebugInterfaceFn = HRESULT(WINAPI*)(REFIID, void**);
using CreateFactoryFn = HRESULT(WINAPI*)(REFIID, void**);
using CreateFactory2Fn = HRESULT(WINAPI*)(UINT, REFIID, void**);

LogQueue*                    g_log = nullptr;
Settings                     g_settings;
Originals                    g_original;
VtablePatcher                g_patcher;
ViewRegistry                 g_views;
StateTracker                 g_tracker;
std::array<HookEvents*, 4>   g_listeners{};
size_t                       g_listenerCount = 0;
std::atomic<bool>            g_tracking{ false };
std::atomic<int>             g_trackingRequests{ 0 };
std::atomic<bool>            g_logging{ false };
std::atomic<uint32_t>        g_frame{ 0 };
std::atomic<ID3D12Device*>   g_device{ nullptr };      // the latest device the detour or a late patch saw
std::atomic<ID3D12Device*>   g_mainDevice{ nullptr };  // the main queue's device
std::atomic<bool>            g_devicePatched{ false };
std::atomic<bool>            g_seenAtCreation{ false };
std::atomic<void*>           g_listVtable{ nullptr };
std::atomic<ID3D12CommandQueue*>        g_mainQueue{ nullptr };
std::atomic<ID3D12GraphicsCommandList*> g_evaluateList{ nullptr };
std::mutex                   g_patchMutex;  // device, factory and swap-chain patching
std::mutex                   g_swapChainMutex;
std::map<const void*, ID3D12CommandQueue*> g_swapChainQueues;  // the native queue each swap chain was made with

D3D12CreateDeviceFn      g_createDevice = nullptr;
D3D12GetDebugInterfaceFn g_getDebugInterface = nullptr;
CreateFactoryFn          g_createFactory = nullptr;
CreateFactoryFn          g_createFactory1 = nullptr;
CreateFactory2Fn         g_createFactory2 = nullptr;

void Log(std::string aLine)
{
	if (g_log)
	{
		g_log->Push(std::move(aLine));
	}
}

template<class Fn>
void Each(Fn&& aFn)
{
	for (size_t i = 0; i < g_listenerCount; ++i)
	{
		aFn(*g_listeners[i]);
	}
}

// The list's state, if it's being tracked and is recording.
ListState* Recording(void* aList)
{
	ListState* state = g_tracker.Find(aList);
	return state && state->Recording() ? state : nullptr;
}

ViewInfo MakeView(ViewInfo::Kind aKind, ID3D12Resource* aResource, uint32_t aViewFormat, uint32_t aMip, uint32_t aPlane,
                  uint32_t aFirstSlice, bool aReadOnlyDepth, bool aReadOnlyStencil)
{
	ViewInfo view;
	view.kind = aKind;
	view.resource = aResource;
	if (aResource)
	{
		const D3D12_RESOURCE_DESC desc = aResource->GetDesc();
		view.resourceFormat = desc.Format;
		view.width = desc.Width;
		view.height = desc.Height;
		view.resourceFlags = desc.Flags;
	}
	view.viewFormat = aViewFormat != DXGI_FORMAT_UNKNOWN ? aViewFormat : view.resourceFormat;
	view.mip = aMip;
	view.plane = aPlane;
	view.firstSlice = aFirstSlice;
	view.readOnlyDepth = aReadOnlyDepth;
	view.readOnlyStencil = aReadOnlyStencil;
	return view;
}

// ---- device -------------------------------------------------------------------------------------------------------

void STDMETHODCALLTYPE CreateRtv(ID3D12Device* aDevice, ID3D12Resource* aResource, const D3D12_RENDER_TARGET_VIEW_DESC* aDesc,
                                 D3D12_CPU_DESCRIPTOR_HANDLE aHandle)
{
	g_original.createRtv(aDevice, aResource, aDesc, aHandle);
	uint32_t mip = 0, plane = 0, slice = 0;
	if (aDesc)
	{
		switch (aDesc->ViewDimension)
		{
		case D3D12_RTV_DIMENSION_TEXTURE2D:
			mip = aDesc->Texture2D.MipSlice;
			plane = aDesc->Texture2D.PlaneSlice;
			break;
		case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
			mip = aDesc->Texture2DArray.MipSlice;
			plane = aDesc->Texture2DArray.PlaneSlice;
			slice = aDesc->Texture2DArray.FirstArraySlice;
			break;
		default: break;
		}
	}
	g_views.Record(aHandle.ptr, MakeView(ViewInfo::Kind::Rtv, aResource, aDesc ? aDesc->Format : DXGI_FORMAT_UNKNOWN, mip,
	                                      plane, slice, false, false));
}

void STDMETHODCALLTYPE CreateDsv(ID3D12Device* aDevice, ID3D12Resource* aResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* aDesc,
                                 D3D12_CPU_DESCRIPTOR_HANDLE aHandle)
{
	g_original.createDsv(aDevice, aResource, aDesc, aHandle);
	uint32_t mip = 0, slice = 0;
	if (aDesc)
	{
		switch (aDesc->ViewDimension)
		{
		case D3D12_DSV_DIMENSION_TEXTURE2D: mip = aDesc->Texture2D.MipSlice; break;
		case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
			mip = aDesc->Texture2DArray.MipSlice;
			slice = aDesc->Texture2DArray.FirstArraySlice;
			break;
		default: break;
		}
	}
	const bool readOnlyDepth = aDesc && (aDesc->Flags & D3D12_DSV_FLAG_READ_ONLY_DEPTH);
	const bool readOnlyStencil = aDesc && (aDesc->Flags & D3D12_DSV_FLAG_READ_ONLY_STENCIL);
	g_views.Record(aHandle.ptr, MakeView(ViewInfo::Kind::Dsv, aResource, aDesc ? aDesc->Format : DXGI_FORMAT_UNKNOWN, mip, 0,
	                                      slice, readOnlyDepth, readOnlyStencil));
}

std::optional<ViewInfo::Kind> KindOf(D3D12_DESCRIPTOR_HEAP_TYPE aType)
{
	if (aType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
	{
		return ViewInfo::Kind::Rtv;
	}
	if (aType == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
	{
		return ViewInfo::Kind::Dsv;
	}
	return std::nullopt;
}

void STDMETHODCALLTYPE CopyDescriptors(ID3D12Device* aDevice, UINT aDestRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* aDestStarts,
                                       const UINT* aDestSizes, UINT aSourceRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* aSourceStarts,
                                       const UINT* aSourceSizes, D3D12_DESCRIPTOR_HEAP_TYPE aType)
{
	g_original.copyDescriptors(aDevice, aDestRanges, aDestStarts, aDestSizes, aSourceRanges, aSourceStarts, aSourceSizes, aType);
	const auto kind = KindOf(aType);
	if (!kind)
	{
		return;
	}
	static_assert(sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) == sizeof(uint64_t));
	const auto runs = CopyRuns(aDestRanges, reinterpret_cast<const uint64_t*>(aDestStarts), aDestSizes, aSourceRanges,
	                           reinterpret_cast<const uint64_t*>(aSourceStarts), aSourceSizes, g_views.Increment(*kind));
	for (const DescriptorRun& run : runs)
	{
		g_views.Copy(*kind, run.dest, run.source, run.count);
	}
}

void STDMETHODCALLTYPE CopyDescriptorsSimple(ID3D12Device* aDevice, UINT aCount, D3D12_CPU_DESCRIPTOR_HANDLE aDest,
                                             D3D12_CPU_DESCRIPTOR_HANDLE aSource, D3D12_DESCRIPTOR_HEAP_TYPE aType)
{
	g_original.copyDescriptorsSimple(aDevice, aCount, aDest, aSource, aType);
	if (const auto kind = KindOf(aType))
	{
		g_views.Copy(*kind, aDest.ptr, aSource.ptr, aCount);
	}
}

// ---- command queue ------------------------------------------------------------------------------------------------

void STDMETHODCALLTYPE ExecuteCommandLists(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists)
{
	if (ID3D12GraphicsCommandList* evaluate = g_evaluateList.load())
	{
		for (UINT i = 0; i < aCount; ++i)
		{
			if (aLists[i] == evaluate && g_evaluateList.compare_exchange_strong(evaluate, nullptr))
			{
				// Both are held for the session: GetDevice's reference, and one more on the queue.
				ID3D12Device* device = nullptr;
				if (SUCCEEDED(aQueue->GetDevice(IID_PPV_ARGS(&device))))
				{
					g_mainDevice = device;
				}
				aQueue->AddRef();
				g_mainQueue = aQueue;
				Log(std::format("main queue 0x{:x}, on device 0x{:x}{}", reinterpret_cast<uintptr_t>(aQueue),
				                reinterpret_cast<uintptr_t>(device),
				                device == g_device.load() ? ", the one the detour saw" : ", NOT the one the detour saw"));
				break;
			}
		}
	}
	Each([&](HookEvents& aListener) { aListener.OnSubmit(aQueue, aCount, aLists); });
	const std::vector<ID3D12CommandList*> lists = WithListsBefore(aCount, aLists, [&](ID3D12CommandList* aList) {
		ID3D12CommandList* before = nullptr;
		Each([&](HookEvents& aListener) {
			if (!before)
			{
				before = aListener.ListBefore(aQueue, aList);
			}
		});
		return before;
	});
	if (lists.empty())
	{
		g_original.executeCommandLists(aQueue, aCount, aLists);
	}
	else
	{
		g_original.executeCommandLists(aQueue, UINT(lists.size()), lists.data());
	}
	Each([&](HookEvents& aListener) { aListener.OnSubmitted(aQueue, aCount, aLists); });
}

// ---- command lists ------------------------------------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE Reset(ID3D12GraphicsCommandList* aList, ID3D12CommandAllocator* aAllocator, ID3D12PipelineState* aState)
{
	const HRESULT result = g_original.reset(aList, aAllocator, aState);
	if (SUCCEEDED(result) && g_tracking.load(std::memory_order_relaxed))
	{
		g_tracker.ForReset(aList).Begin(g_frame.load(std::memory_order_relaxed), g_logging.load(std::memory_order_relaxed));
	}
	return result;
}

HRESULT STDMETHODCALLTYPE Close(ID3D12GraphicsCommandList* aList)
{
	if (ListState* state = Recording(aList))
	{
		Each([&](HookEvents& aListener) { aListener.OnPassEnd(aList, *state, true); });
		Each([&](HookEvents& aListener) { aListener.OnClose(aList, *state); });
		state->End();
	}
	return g_original.close(aList);
}

void STDMETHODCALLTYPE DrawInstanced(ID3D12GraphicsCommandList* aList, UINT aVertices, UINT aInstances, UINT aFirstVertex, UINT aFirstInstance)
{
	if (ListState* state = Recording(aList))
	{
		state->Count(ListState::Counter::Draw);
	}
	g_original.drawInstanced(aList, aVertices, aInstances, aFirstVertex, aFirstInstance);
}

void STDMETHODCALLTYPE DrawIndexedInstanced(ID3D12GraphicsCommandList* aList, UINT aIndices, UINT aInstances, UINT aFirstIndex,
                                            INT aBaseVertex, UINT aFirstInstance)
{
	if (ListState* state = Recording(aList))
	{
		state->Count(ListState::Counter::IndexedDraw);
	}
	g_original.drawIndexedInstanced(aList, aIndices, aInstances, aFirstIndex, aBaseVertex, aFirstInstance);
}

void STDMETHODCALLTYPE Dispatch(ID3D12GraphicsCommandList* aList, UINT aX, UINT aY, UINT aZ)
{
	if (ListState* state = Recording(aList))
	{
		state->Count(ListState::Counter::Dispatch);
	}
	g_original.dispatch(aList, aX, aY, aZ);
}

void STDMETHODCALLTYPE ResourceBarrier(ID3D12GraphicsCommandList* aList, UINT aCount, const D3D12_RESOURCE_BARRIER* aBarriers)
{
	if (ListState* state = Recording(aList))
	{
		state->Count(ListState::Counter::Barrier);
		for (UINT i = 0; i < aCount; ++i)
		{
			const D3D12_RESOURCE_BARRIER& barrier = aBarriers[i];
			if (barrier.Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
			{
				continue;
			}
			state->Transition(barrier.Transition.pResource, barrier.Transition.StateBefore, barrier.Transition.StateAfter,
			                  barrier.Transition.Subresource, barrier.Flags);
		}
	}
	g_original.resourceBarrier(aList, aCount, aBarriers);
}

void STDMETHODCALLTYPE ExecuteBundle(ID3D12GraphicsCommandList* aList, ID3D12GraphicsCommandList* aBundle)
{
	if (ListState* state = Recording(aList))
	{
		state->Count(ListState::Counter::Bundle);
	}
	g_original.executeBundle(aList, aBundle);
}

BoundTarget Bound(uint64_t aHandle)
{
	return { aHandle, g_views.Find(aHandle) };
}

void STDMETHODCALLTYPE OMSetRenderTargets(ID3D12GraphicsCommandList* aList, UINT aCount, const D3D12_CPU_DESCRIPTOR_HANDLE* aRtvs,
                                          BOOL aSingleRange, const D3D12_CPU_DESCRIPTOR_HANDLE* aDsv)
{
	if (ListState* state = Recording(aList))
	{
		Each([&](HookEvents& aListener) { aListener.OnPassEnd(aList, *state, false); });
		Targets  targets;
		const uint64_t step = g_views.Increment(ViewInfo::Kind::Rtv);
		for (UINT i = 0; aRtvs && i < aCount && i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		{
			targets.rtvs.push_back(Bound(aSingleRange ? aRtvs[0].ptr + i * step : aRtvs[i].ptr));
		}
		if (aDsv)
		{
			targets.dsv = Bound(aDsv->ptr);
		}
		state->SetTargets(std::move(targets));
	}
	g_original.omSetRenderTargets(aList, aCount, aRtvs, aSingleRange, aDsv);
}

void STDMETHODCALLTYPE ClearDsv(ID3D12GraphicsCommandList* aList, D3D12_CPU_DESCRIPTOR_HANDLE aDsv, D3D12_CLEAR_FLAGS aFlags,
                                FLOAT aDepth, UINT8 aStencil, UINT aRects, const D3D12_RECT* aRectList)
{
	if (ListState* state = Recording(aList))
	{
		state->Count(ListState::Counter::Clear);
	}
	g_original.clearDsv(aList, aDsv, aFlags, aDepth, aStencil, aRects, aRectList);
}

void STDMETHODCALLTYPE ClearRtv(ID3D12GraphicsCommandList* aList, D3D12_CPU_DESCRIPTOR_HANDLE aRtv, const FLOAT* aColour, UINT aRects,
                                const D3D12_RECT* aRectList)
{
	if (ListState* state = Recording(aList))
	{
		state->Count(ListState::Counter::Clear);
	}
	g_original.clearRtv(aList, aRtv, aColour, aRects, aRectList);
}

void STDMETHODCALLTYPE SetMarker(ID3D12GraphicsCommandList* aList, UINT aMetadata, const void* aData, UINT aSize)
{
	if (ListState* state = Recording(aList); state && state->Logging())
	{
		state->Event(EventName(aMetadata, aData, aSize));
	}
	g_original.setMarker(aList, aMetadata, aData, aSize);
}

void STDMETHODCALLTYPE BeginEvent(ID3D12GraphicsCommandList* aList, UINT aMetadata, const void* aData, UINT aSize)
{
	if (ListState* state = Recording(aList); state && state->Logging())
	{
		state->Event(EventName(aMetadata, aData, aSize));
	}
	g_original.beginEvent(aList, aMetadata, aData, aSize);
}

void STDMETHODCALLTYPE ExecuteIndirect(ID3D12GraphicsCommandList* aList, ID3D12CommandSignature* aSignature, UINT aMaxCommands,
                                       ID3D12Resource* aArguments, UINT64 aArgumentOffset, ID3D12Resource* aCount, UINT64 aCountOffset)
{
	if (ListState* state = Recording(aList))
	{
		state->Count(ListState::Counter::Indirect);
	}
	g_original.executeIndirect(aList, aSignature, aMaxCommands, aArguments, aArgumentOffset, aCount, aCountOffset);
}

void STDMETHODCALLTYPE BeginRenderPass(ID3D12GraphicsCommandList4* aList, UINT aCount, const D3D12_RENDER_PASS_RENDER_TARGET_DESC* aTargets,
                                       const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* aDepth, D3D12_RENDER_PASS_FLAGS aFlags)
{
	if (ListState* state = Recording(aList))
	{
		Each([&](HookEvents& aListener) { aListener.OnPassEnd(aList, *state, false); });
		Targets targets;
		for (UINT i = 0; aTargets && i < aCount && i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		{
			targets.rtvs.push_back(Bound(aTargets[i].cpuDescriptor.ptr));
		}
		if (aDepth)
		{
			targets.dsv = Bound(aDepth->cpuDescriptor.ptr);
		}
		state->SetTargets(std::move(targets), true);
	}
	g_original.beginRenderPass(aList, aCount, aTargets, aDepth, aFlags);
}

void STDMETHODCALLTYPE EndRenderPass(ID3D12GraphicsCommandList4* aList)
{
	g_original.endRenderPass(aList);
	if (ListState* state = Recording(aList))
	{
		// Outside the render pass now, so a listener may record into the list.
		Each([&](HookEvents& aListener) { aListener.OnPassEnd(aList, *state, false); });
		state->SetTargets({});
	}
}

void STDMETHODCALLTYPE Barrier(ID3D12GraphicsCommandList7* aList, UINT32 aGroups, const D3D12_BARRIER_GROUP* aBarrierGroups)
{
	if (ListState* state = Recording(aList))
	{
		state->Count(ListState::Counter::EnhancedBarrier);
	}
	g_original.barrier(aList, aGroups, aBarrierGroups);
}

// ---- DXGI ---------------------------------------------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE Present(IDXGISwapChain* aSwapChain, UINT aSyncInterval, UINT aFlags)
{
	if ((aFlags & DXGI_PRESENT_TEST) == 0)
	{
		Each([&](HookEvents& aListener) { aListener.OnPresent(aSwapChain); });
	}
	return g_original.present(aSwapChain, aSyncInterval, aFlags);
}

HRESULT STDMETHODCALLTYPE Present1(IDXGISwapChain1* aSwapChain, UINT aSyncInterval, UINT aFlags, const DXGI_PRESENT_PARAMETERS* aParameters)
{
	if ((aFlags & DXGI_PRESENT_TEST) == 0)
	{
		Each([&](HookEvents& aListener) { aListener.OnPresent(aSwapChain); });
	}
	return g_original.present1(aSwapChain, aSyncInterval, aFlags, aParameters);
}

// Patches one entry, once. The original is stored before the entry points at the hook, so no thread can reach the
// hook first. A second vtable whose entry holds a different function is left alone: each hook calls one original.
template<class Fn>
void PatchEntry(void* aObject, size_t aSlot, void* aHook, Fn& aOriginal, const char* aName)
{
	void* current = (*static_cast<void***>(aObject))[aSlot];
	if (current == aHook)
	{
		return;
	}
	if (aOriginal && current != reinterpret_cast<void*>(aOriginal))
	{
		Log(std::format("{}: a second vtable, left unpatched", aName));
		return;
	}
	aOriginal = reinterpret_cast<Fn>(current);
	if (!g_patcher.Patch(aObject, aSlot, aHook))
	{
		Log(std::format("couldn't patch {}", aName));
	}
}

// The native queue under a swap chain's "device", which for D3D12 is the queue it presents on. Null for anything else,
// such as a D3D11 device. No reference is kept: the pointer is only compared.
ID3D12CommandQueue* NativeQueueOf(IUnknown* aDevice)
{
	ComPtr<ID3D12CommandQueue> queue;
	if (!aDevice || FAILED(aDevice->QueryInterface(IID_PPV_ARGS(&queue))))
	{
		return nullptr;
	}
	if (void* native = slhooks::NativeInterface(queue.Get()))
	{
		static_cast<IUnknown*>(native)->Release();  // the proxy holds it
		return static_cast<ID3D12CommandQueue*>(native);
	}
	return queue.Get();  // the swap chain holds it
}

// The game's swap chain doesn't say which queue it presents on (GetDevice fails), so each swap chain's queue is noted
// when it's made.
void NoteSwapChain(IDXGISwapChain* aSwapChain, IUnknown* aDevice, const char* aHow)
{
	ID3D12CommandQueue* const queue = NativeQueueOf(aDevice);
	{
		std::lock_guard lock(g_swapChainMutex);
		g_swapChainQueues[aSwapChain] = queue;
	}
	Log(std::format("swap chain 0x{:x} created ({}), on queue 0x{:x}: Present patched", reinterpret_cast<uintptr_t>(aSwapChain), aHow,
	                reinterpret_cast<uintptr_t>(queue)));
}

void PatchSwapChain(IDXGISwapChain* aSwapChain)
{
	std::lock_guard lock(g_patchMutex);
	PatchEntry(aSwapChain, slots::kPresent, reinterpret_cast<void*>(&Present), g_original.present, "IDXGISwapChain::Present");
	ComPtr<IDXGISwapChain1> swapChain1;
	if (SUCCEEDED(aSwapChain->QueryInterface(IID_PPV_ARGS(&swapChain1))))
	{
		PatchEntry(swapChain1.Get(), slots::kPresent1, reinterpret_cast<void*>(&Present1), g_original.present1, "IDXGISwapChain1::Present1");
	}
}

HRESULT STDMETHODCALLTYPE CreateSwapChain(IDXGIFactory* aFactory, IUnknown* aDevice, DXGI_SWAP_CHAIN_DESC* aDesc, IDXGISwapChain** aSwapChain)
{
	const HRESULT result = g_original.createSwapChain(aFactory, aDevice, aDesc, aSwapChain);
	if (SUCCEEDED(result) && aSwapChain && *aSwapChain)
	{
		PatchSwapChain(*aSwapChain);
		NoteSwapChain(*aSwapChain, aDevice, "CreateSwapChain");
	}
	return result;
}

HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd(IDXGIFactory2* aFactory, IUnknown* aDevice, HWND aWindow, const DXGI_SWAP_CHAIN_DESC1* aDesc,
                                                 const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* aFullscreen, IDXGIOutput* aOutput, IDXGISwapChain1** aSwapChain)
{
	const HRESULT result = g_original.createSwapChainForHwnd(aFactory, aDevice, aWindow, aDesc, aFullscreen, aOutput, aSwapChain);
	if (SUCCEEDED(result) && aSwapChain && *aSwapChain)
	{
		PatchSwapChain(*aSwapChain);
		NoteSwapChain(*aSwapChain, aDevice, "CreateSwapChainForHwnd");
	}
	return result;
}

void PatchFactory(void* aFactory)
{
	auto* unknown = static_cast<IUnknown*>(aFactory);
	ComPtr<IDXGIFactory2> factory;
	if (FAILED(unknown->QueryInterface(IID_PPV_ARGS(&factory))))
	{
		return;
	}
	std::lock_guard lock(g_patchMutex);
	PatchEntry(factory.Get(), slots::kCreateSwapChain, reinterpret_cast<void*>(&CreateSwapChain), g_original.createSwapChain,
	           "IDXGIFactory::CreateSwapChain");
	PatchEntry(factory.Get(), slots::kCreateSwapChainForHwnd, reinterpret_cast<void*>(&CreateSwapChainForHwnd),
	           g_original.createSwapChainForHwnd, "IDXGIFactory2::CreateSwapChainForHwnd");
}

HRESULT WINAPI CreateFactoryDetour(REFIID aId, void** aFactory)
{
	const HRESULT result = g_createFactory(aId, aFactory);
	if (SUCCEEDED(result) && aFactory && *aFactory)
	{
		PatchFactory(*aFactory);
	}
	return result;
}

HRESULT WINAPI CreateFactory1Detour(REFIID aId, void** aFactory)
{
	const HRESULT result = g_createFactory1(aId, aFactory);
	if (SUCCEEDED(result) && aFactory && *aFactory)
	{
		PatchFactory(*aFactory);
	}
	return result;
}

HRESULT WINAPI CreateFactory2Detour(UINT aFlags, REFIID aId, void** aFactory)
{
	const HRESULT result = g_createFactory2(aFlags, aId, aFactory);
	if (SUCCEEDED(result) && aFactory && *aFactory)
	{
		PatchFactory(*aFactory);
	}
	return result;
}

// ---- the device ---------------------------------------------------------------------------------------------------

void PatchDevice(ID3D12Device* aDevice)
{
	std::lock_guard lock(g_patchMutex);
	if (g_device.load() == aDevice)
	{
		return;
	}
	aDevice->AddRef();  // held for the session, so Device() never hands out a released device
	g_device = aDevice;
	if (g_devicePatched.exchange(true))
	{
		Log(std::format("another device 0x{:x}: the vtables it shares are already patched", reinterpret_cast<uintptr_t>(aDevice)));
		return;
	}
	g_views.SetIncrements(aDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV),
	                      aDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
	PatchEntry(aDevice, slots::kCreateRenderTargetView, reinterpret_cast<void*>(&CreateRtv), g_original.createRtv, "CreateRenderTargetView");
	PatchEntry(aDevice, slots::kCreateDepthStencilView, reinterpret_cast<void*>(&CreateDsv), g_original.createDsv, "CreateDepthStencilView");
	PatchEntry(aDevice, slots::kCopyDescriptors, reinterpret_cast<void*>(&CopyDescriptors), g_original.copyDescriptors, "CopyDescriptors");
	PatchEntry(aDevice, slots::kCopyDescriptorsSimple, reinterpret_cast<void*>(&CopyDescriptorsSimple), g_original.copyDescriptorsSimple,
	           "CopyDescriptorsSimple");

	// A queue, an allocator and a list of our own, only to reach the vtables every queue and list shares.
	D3D12_COMMAND_QUEUE_DESC queueDesc{};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ComPtr<ID3D12CommandQueue>        queue;
	ComPtr<ID3D12CommandAllocator>    allocator;
	ComPtr<ID3D12GraphicsCommandList> list;
	if (FAILED(aDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue))) ||
	    FAILED(aDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) ||
	    FAILED(aDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list))))
	{
		Log("couldn't create a queue and list to reach their vtables: command lists aren't tracked");
		return;
	}
	list->Close();
	PatchEntry(queue.Get(), slots::kExecuteCommandLists, reinterpret_cast<void*>(&ExecuteCommandLists), g_original.executeCommandLists,
	           "ExecuteCommandLists");
	void* l = list.Get();
	PatchEntry(l, slots::kClose, reinterpret_cast<void*>(&Close), g_original.close, "Close");
	PatchEntry(l, slots::kReset, reinterpret_cast<void*>(&Reset), g_original.reset, "Reset");
	PatchEntry(l, slots::kDrawInstanced, reinterpret_cast<void*>(&DrawInstanced), g_original.drawInstanced, "DrawInstanced");
	PatchEntry(l, slots::kDrawIndexedInstanced, reinterpret_cast<void*>(&DrawIndexedInstanced), g_original.drawIndexedInstanced,
	           "DrawIndexedInstanced");
	PatchEntry(l, slots::kDispatch, reinterpret_cast<void*>(&Dispatch), g_original.dispatch, "Dispatch");
	PatchEntry(l, slots::kResourceBarrier, reinterpret_cast<void*>(&ResourceBarrier), g_original.resourceBarrier, "ResourceBarrier");
	PatchEntry(l, slots::kExecuteBundle, reinterpret_cast<void*>(&ExecuteBundle), g_original.executeBundle, "ExecuteBundle");
	PatchEntry(l, slots::kOMSetRenderTargets, reinterpret_cast<void*>(&OMSetRenderTargets), g_original.omSetRenderTargets,
	           "OMSetRenderTargets");
	PatchEntry(l, slots::kClearDepthStencilView, reinterpret_cast<void*>(&ClearDsv), g_original.clearDsv, "ClearDepthStencilView");
	PatchEntry(l, slots::kClearRenderTargetView, reinterpret_cast<void*>(&ClearRtv), g_original.clearRtv, "ClearRenderTargetView");
	PatchEntry(l, slots::kSetMarker, reinterpret_cast<void*>(&SetMarker), g_original.setMarker, "SetMarker");
	PatchEntry(l, slots::kBeginEvent, reinterpret_cast<void*>(&BeginEvent), g_original.beginEvent, "BeginEvent");
	PatchEntry(l, slots::kExecuteIndirect, reinterpret_cast<void*>(&ExecuteIndirect), g_original.executeIndirect, "ExecuteIndirect");
	// The later entries exist only if the list is a version-4 and version-7 list.
	ComPtr<ID3D12GraphicsCommandList4> list4;
	if (SUCCEEDED(list.As(&list4)))
	{
		PatchEntry(l, slots::kBeginRenderPass, reinterpret_cast<void*>(&BeginRenderPass), g_original.beginRenderPass, "BeginRenderPass");
		PatchEntry(l, slots::kEndRenderPass, reinterpret_cast<void*>(&EndRenderPass), g_original.endRenderPass, "EndRenderPass");
	}
	ComPtr<ID3D12GraphicsCommandList7> list7;
	if (SUCCEEDED(list.As(&list7)))
	{
		PatchEntry(l, slots::kBarrier, reinterpret_cast<void*>(&Barrier), g_original.barrier, "Barrier");
	}
	g_listVtable = *static_cast<void**>(l);
	Log(std::format("device 0x{:x} patched: {} vtable entries", reinterpret_cast<uintptr_t>(aDevice), g_patcher.Count()));
}

void EnableDebugTools()
{
	if (!g_getDebugInterface || (!g_settings.dred && !g_settings.debugLayer))
	{
		return;
	}
	if (g_settings.debugLayer)
	{
		ComPtr<ID3D12Debug> debug;
		if (SUCCEEDED(g_getDebugInterface(IID_PPV_ARGS(&debug))))
		{
			debug->EnableDebugLayer();
			Log("D3D12 debug layer on");
		}
		else
		{
			Log("couldn't turn the D3D12 debug layer on");
		}
	}
	if (g_settings.dred)
	{
		ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dred;
		if (SUCCEEDED(g_getDebugInterface(IID_PPV_ARGS(&dred))))
		{
			dred->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
			dred->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
			Log("DRED on");
		}
		else
		{
			Log("couldn't turn DRED on");
		}
	}
}

// The adapter's name, for the log.
std::string AdapterName(IUnknown* aAdapter)
{
	if (!aAdapter)
	{
		return "the default adapter";
	}
	ComPtr<IDXGIAdapter> adapter;
	DXGI_ADAPTER_DESC    desc{};
	if (FAILED(aAdapter->QueryInterface(IID_PPV_ARGS(&adapter))) || FAILED(adapter->GetDesc(&desc)))
	{
		return "an adapter that doesn't say what it is";
	}
	std::string name;
	for (size_t i = 0; i < std::size(desc.Description) && desc.Description[i] != 0; ++i)
	{
		const wchar_t ch = desc.Description[i];
		name += ch >= 32 && ch < 127 ? char(ch) : '?';
	}
	return name;
}

HRESULT WINAPI CreateDeviceDetour(IUnknown* aAdapter, D3D_FEATURE_LEVEL aLevel, REFIID aId, void** aDevice)
{
	if (aDevice && !g_device.load())
	{
		EnableDebugTools();
	}
	const HRESULT result = g_createDevice(aAdapter, aLevel, aId, aDevice);
	if (SUCCEEDED(result) && aDevice && *aDevice)
	{
		ComPtr<ID3D12Device> device;
		if (SUCCEEDED(static_cast<IUnknown*>(*aDevice)->QueryInterface(IID_PPV_ARGS(&device))))
		{
			g_seenAtCreation = true;
			Log(std::format("device 0x{:x} created on {}", reinterpret_cast<uintptr_t>(device.Get()), AdapterName(aAdapter)));
			PatchDevice(device.Get());
		}
	}
	return result;
}

template<class Fn>
bool Detour(const RED4ext::v1::Sdk* aSdk, RED4ext::v1::PluginHandle aHandle, HMODULE aModule, const char* aName, void* aDetour, Fn& aOriginal)
{
	void* target = aModule ? reinterpret_cast<void*>(GetProcAddress(aModule, aName)) : nullptr;
	if (!target || !aSdk->hooking->Attach(aHandle, target, aDetour, reinterpret_cast<void**>(&aOriginal)))
	{
		Log(std::format("couldn't detour {}", aName));
		return false;
	}
	return true;
}
}

const Originals& Original()
{
	return g_original;
}

bool InstallDetours(const RED4ext::v1::Sdk* aSdk, RED4ext::v1::PluginHandle aHandle, LogQueue& aLog, const Settings& aSettings)
{
	g_log = &aLog;
	g_settings = aSettings;
	const HMODULE d3d12 = LoadLibraryW(L"d3d12.dll");
	const HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
	g_getDebugInterface = d3d12 ? reinterpret_cast<D3D12GetDebugInterfaceFn>(GetProcAddress(d3d12, "D3D12GetDebugInterface")) : nullptr;
	bool ok = Detour(aSdk, aHandle, d3d12, "D3D12CreateDevice", reinterpret_cast<void*>(&CreateDeviceDetour), g_createDevice);
	ok = Detour(aSdk, aHandle, dxgi, "CreateDXGIFactory", reinterpret_cast<void*>(&CreateFactoryDetour), g_createFactory) && ok;
	ok = Detour(aSdk, aHandle, dxgi, "CreateDXGIFactory1", reinterpret_cast<void*>(&CreateFactory1Detour), g_createFactory1) && ok;
	ok = Detour(aSdk, aHandle, dxgi, "CreateDXGIFactory2", reinterpret_cast<void*>(&CreateFactory2Detour), g_createFactory2) && ok;
	return ok;
}

void AddListener(HookEvents* aListener)
{
	if (g_listenerCount < g_listeners.size())
	{
		g_listeners[g_listenerCount++] = aListener;
	}
}

void RequestTracking()
{
	g_tracking = ++g_trackingRequests > 0;
}

void ReleaseTracking()
{
	g_tracking = --g_trackingRequests > 0;
}

bool Tracking()
{
	return g_tracking.load();
}

void SetLogging(bool aLogging)
{
	g_logging = aLogging;
}

void PatchLateDevice(ID3D12Device* aDevice)
{
	if (aDevice && !g_device.load())
	{
		PatchDevice(aDevice);
	}
}

void PatchLateSwapChain(IDXGISwapChain* aSwapChain)
{
	if (aSwapChain)
	{
		PatchSwapChain(aSwapChain);
	}
}

ViewRegistry& Views()
{
	return g_views;
}

StateTracker& Tracker()
{
	return g_tracker;
}

ID3D12Device* Device()
{
	if (ID3D12Device* main = g_mainDevice.load())
	{
		return main;
	}
	return g_device.load();
}

bool SeenAtCreation()
{
	return g_seenAtCreation.load();
}

void* ListVtable()
{
	return g_listVtable.load();
}

ID3D12CommandQueue* MainQueue()
{
	return g_mainQueue.load();
}

ID3D12CommandQueue* PresentQueue(IDXGISwapChain* aSwapChain)
{
	{
		std::lock_guard lock(g_swapChainMutex);
		if (const auto found = g_swapChainQueues.find(aSwapChain); found != g_swapChainQueues.end())
		{
			return found->second;
		}
	}
	// A swap chain made before BlockRender loaded: ask it, though the game's may not say.
	ComPtr<IUnknown> device;
	if (!aSwapChain || FAILED(aSwapChain->GetDevice(IID_PPV_ARGS(&device))))
	{
		return nullptr;
	}
	return NativeQueueOf(device.Get());
}

void NoteEvaluateList(ID3D12GraphicsCommandList* aList)
{
	if (aList && !g_mainQueue.load())
	{
		g_evaluateList = aList;
	}
}

uint32_t CurrentFrame()
{
	return g_frame.load(std::memory_order_relaxed);
}

void SetCurrentFrame(uint32_t aFrame)
{
	g_frame.store(aFrame, std::memory_order_relaxed);
}

namespace
{
std::string NameOf(const char* aNarrow, const wchar_t* aWide)
{
	if (aNarrow)
	{
		return aNarrow;
	}
	std::string name;
	for (size_t i = 0; aWide && aWide[i] != 0 && i < 128; ++i)
	{
		name += aWide[i] >= 32 && aWide[i] < 127 ? char(aWide[i]) : '?';
	}
	return name.empty() ? "(no name)" : name;
}
}

std::vector<std::string> DredReport()
{
	std::vector<std::string>                       lines;
	ID3D12Device* const                            device = Device();
	ComPtr<ID3D12DeviceRemovedExtendedData>        dred;
	if (!g_settings.dred || !device || FAILED(device->QueryInterface(IID_PPV_ARGS(&dred))))
	{
		return lines;
	}
	D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbs{};
	if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput(&breadcrumbs)))
	{
		for (const D3D12_AUTO_BREADCRUMB_NODE* node = breadcrumbs.pHeadAutoBreadcrumbNode; node && lines.size() < 32; node = node->pNext)
		{
			const UINT32 done = node->pLastBreadcrumbValue ? *node->pLastBreadcrumbValue : 0;
			if (done >= node->BreadcrumbCount)
			{
				continue;  // it finished
			}
			lines.push_back(std::format("DRED: list {} on queue {} stopped after {} of {} operations; the next was operation {}",
			                            NameOf(node->pCommandListDebugNameA, node->pCommandListDebugNameW),
			                            NameOf(node->pCommandQueueDebugNameA, node->pCommandQueueDebugNameW), done, node->BreadcrumbCount,
			                            node->pCommandHistory ? int(node->pCommandHistory[done]) : -1));
		}
	}
	D3D12_DRED_PAGE_FAULT_OUTPUT fault{};
	if (SUCCEEDED(dred->GetPageFaultAllocationOutput(&fault)) && fault.PageFaultVA != 0)
	{
		lines.push_back(std::format("DRED: page fault at GPU address 0x{:x}", fault.PageFaultVA));
		size_t shown = 0;
		for (const D3D12_DRED_ALLOCATION_NODE* node = fault.pHeadExistingAllocationNode; node && shown < 16; node = node->pNext, ++shown)
		{
			lines.push_back("DRED: allocation there: " + NameOf(node->ObjectNameA, node->ObjectNameW));
		}
		for (const D3D12_DRED_ALLOCATION_NODE* node = fault.pHeadRecentFreedAllocationNode; node && shown < 32; node = node->pNext, ++shown)
		{
			lines.push_back("DRED: recently freed there: " + NameOf(node->ObjectNameA, node->ObjectNameW));
		}
	}
	if (lines.empty())
	{
		lines.push_back("DRED: no unfinished lists and no page fault recorded");
	}
	return lines;
}

std::vector<std::string> DebugMessages()
{
	std::vector<std::string> lines;
	ID3D12Device* const      device = Device();
	ComPtr<ID3D12InfoQueue>  queue;
	if (!g_settings.debugLayer || !device || FAILED(device->QueryInterface(IID_PPV_ARGS(&queue))))
	{
		return lines;
	}
	size_t        others = 0;
	const UINT64  count = queue->GetNumStoredMessages();
	std::vector<uint8_t> buffer;
	for (UINT64 i = 0; i < count; ++i)
	{
		SIZE_T size = 0;
		if (FAILED(queue->GetMessage(i, nullptr, &size)) || size < sizeof(D3D12_MESSAGE))
		{
			continue;
		}
		buffer.resize(size);
		auto* message = reinterpret_cast<D3D12_MESSAGE*>(buffer.data());
		if (FAILED(queue->GetMessage(i, message, &size)) || message->Severity > D3D12_MESSAGE_SEVERITY_WARNING || !message->pDescription)
		{
			continue;
		}
		const std::string text(message->pDescription, strnlen(message->pDescription, message->DescriptionByteLength));
		if (text.find("BlockRender") == std::string::npos || lines.size() >= 20)
		{
			++others;
			continue;
		}
		lines.push_back(std::format("D3D12 {}: {}", message->Severity == D3D12_MESSAGE_SEVERITY_WARNING ? "warning" : "error", text));
	}
	queue->ClearStoredMessages();
	if (others > 0)
	{
		lines.push_back(std::format("D3D12: {} more errors and warnings, about the game's own objects or past the first 20", others));
	}
	return lines;
}

std::string Summary()
{
	return std::format("device {}, {} vtable entries, present {}, main queue {}, tracking {}",
	                   g_device.load() ? (g_seenAtCreation.load() ? "seen at creation" : "patched late") : "not seen",
	                   g_patcher.Count(), g_original.present ? "patched" : "not patched", g_mainQueue.load() ? "known" : "unknown",
	                   g_tracking.load() ? "on" : "off");
}

size_t PatchedEntries()
{
	return g_patcher.Count();
}

void RemovePatches()
{
	const size_t restored = g_patcher.RestoreAll();
	Log(std::format("{} vtable entries restored", restored));
}
}
