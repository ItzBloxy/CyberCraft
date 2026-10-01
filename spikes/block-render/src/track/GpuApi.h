#pragma once

#include "Readable.h"

#include <d3d12.h>
#include <dxgi.h>

#include <map>
#include <string>

// What the game's own GPU layer (REDengine's GpuApi, through RED4ext.SDK's GpuApi::SDeviceData) holds, read with
// care: the address comes from RED4ext's resolver called directly (the SDK's own lookup ends the game when an address
// is missing), every read is range-checked, and an object is trusted only once its vtable lies in the right DLL.
// Nothing confirms the SDK's offsets for 2.31; step 1 checks them by comparing the device found here with the one the
// D3D12CreateDevice detour saw.
namespace cpmc::gpuapi
{
struct Report
{
	bool            found = false;  // the device data's address resolved and is readable
	std::string     device;         // native, proxy, debug layer, other, or missing
	bool            deviceMatches = false;  // it's the detour's device, or Streamline's proxy for it
	std::string     queue;
	std::string     swapChain;
	IDXGISwapChain* swapChainObject = nullptr;  // the first swap chain, if its vtable lies in dxgi.dll or sl.interposer.dll
	ID3D12Device*   nativeDevice = nullptr;     // the device found here, under any proxy, if its vtable is D3D12Core's
};

Report Check(ID3D12Device* aDetourDevice);

// The game's names for its command lists (CommandListContext::debugName). Use one per frame log: it remembers the
// memory it checked and the native list under each Streamline proxy.
class ListNames
{
public:
	~ListNames();

	// The name of the context recording aNativeList now, or empty.
	std::string Find(ID3D12GraphicsCommandList* aNativeList);

private:
	ReadableMemory                      memory_;
	std::map<void*, void*>              nativeOf_;  // proxy list -> native list (each holding one reference)
};
}
