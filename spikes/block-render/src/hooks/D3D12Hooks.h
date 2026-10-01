#pragma once

#include "LogQueue.h"
#include "hooks/HookEvents.h"
#include "track/StateTracker.h"
#include "track/ViewRegistry.h"

#include <RED4ext/RED4ext.hpp>

#include <d3d12.h>
#include <dxgi1_2.h>

#include <cstdint>
#include <string>
#include <vector>

// The D3D12 and DXGI side of the hooks (the spec's "Hooks"): detours on D3D12CreateDevice and CreateDXGIFactory*,
// set at load, then vtable patches on the native device, command queue, command lists, factory and swap chain.
namespace cpmc::d3d
{
struct Settings
{
	bool dred = false;        // turn on DRED's breadcrumbs and page faults when the device is created
	bool debugLayer = false;  // turn on the D3D12 debug layer when the device is created
};

// The functions behind the patched vtable entries. The plugin calls these when it records into a game list, so its
// commands never reach the tracker or the features.
struct Originals
{
	void(STDMETHODCALLTYPE* createRtv)(ID3D12Device*, ID3D12Resource*, const D3D12_RENDER_TARGET_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE) = nullptr;
	void(STDMETHODCALLTYPE* createDsv)(ID3D12Device*, ID3D12Resource*, const D3D12_DEPTH_STENCIL_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE) = nullptr;
	void(STDMETHODCALLTYPE* copyDescriptors)(ID3D12Device*, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE*, const UINT*, UINT,
	                                         const D3D12_CPU_DESCRIPTOR_HANDLE*, const UINT*, D3D12_DESCRIPTOR_HEAP_TYPE) = nullptr;
	void(STDMETHODCALLTYPE* copyDescriptorsSimple)(ID3D12Device*, UINT, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_CPU_DESCRIPTOR_HANDLE,
	                                               D3D12_DESCRIPTOR_HEAP_TYPE) = nullptr;
	void(STDMETHODCALLTYPE* executeCommandLists)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*) = nullptr;
	HRESULT(STDMETHODCALLTYPE* close)(ID3D12GraphicsCommandList*) = nullptr;
	HRESULT(STDMETHODCALLTYPE* reset)(ID3D12GraphicsCommandList*, ID3D12CommandAllocator*, ID3D12PipelineState*) = nullptr;
	void(STDMETHODCALLTYPE* drawInstanced)(ID3D12GraphicsCommandList*, UINT, UINT, UINT, UINT) = nullptr;
	void(STDMETHODCALLTYPE* drawIndexedInstanced)(ID3D12GraphicsCommandList*, UINT, UINT, UINT, INT, UINT) = nullptr;
	void(STDMETHODCALLTYPE* dispatch)(ID3D12GraphicsCommandList*, UINT, UINT, UINT) = nullptr;
	void(STDMETHODCALLTYPE* resourceBarrier)(ID3D12GraphicsCommandList*, UINT, const D3D12_RESOURCE_BARRIER*) = nullptr;
	void(STDMETHODCALLTYPE* executeBundle)(ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*) = nullptr;
	void(STDMETHODCALLTYPE* omSetRenderTargets)(ID3D12GraphicsCommandList*, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE*, BOOL,
	                                            const D3D12_CPU_DESCRIPTOR_HANDLE*) = nullptr;
	void(STDMETHODCALLTYPE* clearDsv)(ID3D12GraphicsCommandList*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_CLEAR_FLAGS, FLOAT, UINT8, UINT,
	                                  const D3D12_RECT*) = nullptr;
	void(STDMETHODCALLTYPE* clearRtv)(ID3D12GraphicsCommandList*, D3D12_CPU_DESCRIPTOR_HANDLE, const FLOAT*, UINT, const D3D12_RECT*) = nullptr;
	void(STDMETHODCALLTYPE* setMarker)(ID3D12GraphicsCommandList*, UINT, const void*, UINT) = nullptr;
	void(STDMETHODCALLTYPE* beginEvent)(ID3D12GraphicsCommandList*, UINT, const void*, UINT) = nullptr;
	void(STDMETHODCALLTYPE* executeIndirect)(ID3D12GraphicsCommandList*, ID3D12CommandSignature*, UINT, ID3D12Resource*, UINT64,
	                                         ID3D12Resource*, UINT64) = nullptr;
	void(STDMETHODCALLTYPE* beginRenderPass)(ID3D12GraphicsCommandList4*, UINT, const D3D12_RENDER_PASS_RENDER_TARGET_DESC*,
	                                         const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC*, D3D12_RENDER_PASS_FLAGS) = nullptr;
	void(STDMETHODCALLTYPE* endRenderPass)(ID3D12GraphicsCommandList4*) = nullptr;
	void(STDMETHODCALLTYPE* barrier)(ID3D12GraphicsCommandList7*, UINT32, const D3D12_BARRIER_GROUP*) = nullptr;
	HRESULT(STDMETHODCALLTYPE* createSwapChain)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**) = nullptr;
	HRESULT(STDMETHODCALLTYPE* createSwapChainForHwnd)(IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*,
	                                                   const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**) = nullptr;
	HRESULT(STDMETHODCALLTYPE* present)(IDXGISwapChain*, UINT, UINT) = nullptr;
	HRESULT(STDMETHODCALLTYPE* present1)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*) = nullptr;
};

const Originals& Original();

// Installs the detours on d3d12.dll's D3D12CreateDevice and dxgi.dll's CreateDXGIFactory, CreateDXGIFactory1 and
// CreateDXGIFactory2, loading both DLLs to find them. Call once, from Main at load, before any listener is needed.
bool InstallDetours(const RED4ext::v1::Sdk* aSdk, RED4ext::v1::PluginHandle aHandle, LogQueue& aLog, const Settings& aSettings);

// Listeners, in the order they're called. At most 4; add them all at load.
void AddListener(HookEvents* aListener);

// Command lists are tracked from their next Reset while any feature asks for it: a frame log or a dump. Tracking costs
// frame time, so each feature releases its request when it's done.
void RequestTracking();
void ReleaseTracking();
bool Tracking();

// A list to track from its next Reset even while tracking is off: drawing before DLSS names each evaluate's list, the
// only lists it needs. The last few named stay watched until UnwatchLists.
void WatchList(const void* aList);
void UnwatchLists();

// Lists reset while this is on record their passes for the frame log.
void SetLogging(bool aLogging);

// Patches a device the detour missed (it was created before the plugin loaded), once its vtable has been checked.
void PatchLateDevice(ID3D12Device* aDevice);
// Patches a swap chain the factory detour missed.
void PatchLateSwapChain(IDXGISwapChain* aSwapChain);

ViewRegistry& Views();
StateTracker& Tracker();

// The game's device: once the main queue is known, the device it belongs to; until then, the latest device the
// detour or a late patch saw (D3D12 gives one device per adapter, so that's the game's unless it made devices on two
// adapters). Every device stored here is held for the session, so the pointer never dangles.
ID3D12Device*      Device();
bool               SeenAtCreation();
void*              ListVtable();  // native command lists' vtable, once seen
ID3D12CommandQueue* MainQueue();  // the queue the DLSS evaluate's list goes to, once seen; held for the session
// The native queue a swap chain presents on, under any Streamline proxy, as noted when the swap chain was made; null if
// it can't be found.
ID3D12CommandQueue* PresentQueue(IDXGISwapChain* aSwapChain);
void               NoteEvaluateList(ID3D12GraphicsCommandList* aList);
uint32_t           CurrentFrame();
void               SetCurrentFrame(uint32_t aFrame);

// After the device is removed: DRED's unfinished command lists and its page fault, when settings.txt turned DRED on.
std::vector<std::string> DredReport();

// The debug layer's errors and warnings that name BlockRender's objects, since the last call, plus a count of the
// others, when settings.txt turned the debug layer on. Main thread.
std::vector<std::string> DebugMessages();

// A one-line summary for the status file.
std::string Summary();
size_t      PatchedEntries();

// Restores every vtable entry that still holds a patch. The detours stay: RED4ext removes them.
void RemovePatches();
}
