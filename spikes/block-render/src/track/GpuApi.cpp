#include "track/GpuApi.h"

#include "hooks/StreamlineHooks.h"

#include <RED4ext/GpuApi/DeviceData.hpp>

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <memory>

namespace cpmc::gpuapi
{
namespace
{
using ResolveFn = uintptr_t (*)(uint32_t);
constexpr uint32_t kDeviceDataHash = 1239944840u;  // RED4ext.SDK's Detail::AddressHashes::g_DeviceData

// The file name of the module holding aAddress, lower case; empty if none.
std::string ModuleOf(const void* aAddress)
{
	HMODULE module = nullptr;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                        static_cast<LPCWSTR>(aAddress), &module))
	{
		return {};
	}
	char path[MAX_PATH]{};
	const DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
	std::string name(path, length);
	name = name.substr(name.find_last_of("\\/") + 1);
	std::transform(name.begin(), name.end(), name.begin(), [](unsigned char aChar) { return char(std::tolower(aChar)); });
	return name;
}

// The module an object's vtable lies in, or empty if the object can't be read.
std::string VtableModule(ReadableMemory& aMemory, const void* aObject)
{
	if (!aObject || !aMemory.Range(aObject, sizeof(void*)))
	{
		return {};
	}
	return ModuleOf(*static_cast<void* const*>(aObject));
}

std::string Describe(const std::string& aModule)
{
	if (aModule.empty())
	{
		return "missing";
	}
	if (aModule == "d3d12core.dll" || aModule == "dxgi.dll")
	{
		return "native";
	}
	if (aModule == "sl.interposer.dll")
	{
		return "proxy";
	}
	if (aModule == "d3d12sdklayers.dll")
	{
		return "debug layer";
	}
	return "other (" + aModule + ")";
}

RED4ext::GpuApi::SDeviceData* DeviceData(ReadableMemory& aMemory)
{
	const HMODULE red4ext = GetModuleHandleW(L"RED4ext.dll");
	const auto    resolve = red4ext ? reinterpret_cast<ResolveFn>(GetProcAddress(red4ext, "RED4ext_ResolveAddress")) : nullptr;
	const auto    address = resolve ? resolve(kDeviceDataHash) : 0;
	if (address == 0 || !aMemory.Range(reinterpret_cast<const void*>(address), sizeof(void*)))
	{
		return nullptr;
	}
	// The struct is 27 MB; check the fields read straight from it. Container entries are checked as they're read.
	auto* data = *reinterpret_cast<RED4ext::GpuApi::SDeviceData**>(address);
	if (!data || !aMemory.Range(std::addressof(data->device), sizeof(data->device)) ||
	    !aMemory.Range(std::addressof(data->directCommandQueue), sizeof(data->directCommandQueue)))
	{
		return nullptr;
	}
	return data;
}

// The CString's text, if every byte of it can be read.
std::string Text(ReadableMemory& aMemory, const RED4ext::CString& aString)
{
	const uint32_t length = aString.length & 0x3FFFFFFF;
	if (length == 0 || length > 256)
	{
		return {};
	}
	const char* text = aString.length < 0x40000000 ? aString.text.inline_str : aString.text.str.ptr;
	if (aString.length < 0x40000000 && length >= sizeof(aString.text.inline_str))
	{
		return {};
	}
	if (!aMemory.Range(text, length))
	{
		return {};
	}
	return std::string(text, length);
}
}

Report Check(ID3D12Device* aDetourDevice)
{
	ReadableMemory                memory;
	Report                        report;
	RED4ext::GpuApi::SDeviceData* data = DeviceData(memory);
	if (!data)
	{
		report.device = report.queue = report.swapChain = "not found";
		return report;
	}
	report.found = true;
	ID3D12Device* device = data->device.Get();
	const std::string deviceModule = VtableModule(memory, device);
	report.device = Describe(deviceModule);
	if (deviceModule == "d3d12core.dll" || deviceModule == "d3d12sdklayers.dll")
	{
		report.nativeDevice = device;
		report.deviceMatches = device == aDetourDevice;
	}
	else if (deviceModule == "sl.interposer.dll")
	{
		if (void* native = slhooks::NativeInterface(device))
		{
			report.nativeDevice = static_cast<ID3D12Device*>(native);
			report.deviceMatches = native == aDetourDevice;
			static_cast<IUnknown*>(native)->Release();
		}
	}
	report.queue = Describe(VtableModule(memory, data->directCommandQueue.Get()));
	report.swapChain = "missing";
	auto& swapChains = data->swapChains;
	for (auto& handle : swapChains.resources)
	{
		if (!memory.Range(&handle, sizeof(handle)) || !handle.IsUsed())
		{
			continue;
		}
		IDXGISwapChain4*  swapChain = handle.instance.swapChain.Get();
		const std::string module = VtableModule(memory, swapChain);
		report.swapChain = Describe(module);
		if (module == "dxgi.dll" || module == "sl.interposer.dll")
		{
			report.swapChainObject = swapChain;
		}
		break;
	}
	return report;
}

ListNames::~ListNames()
{
	for (const auto& [proxy, native] : nativeOf_)
	{
		if (native)
		{
			static_cast<IUnknown*>(native)->Release();
		}
	}
}

std::string ListNames::Find(ID3D12GraphicsCommandList* aNativeList)
{
	RED4ext::GpuApi::SDeviceData* data = DeviceData(memory_);
	if (!data || !aNativeList)
	{
		return {};
	}
	void* const listVtable = *reinterpret_cast<void**>(aNativeList);
	for (auto& handle : data->commandLists.resources)
	{
		if (!memory_.Range(&handle, sizeof(handle)) || !handle.IsUsed())
		{
			continue;
		}
		RED4ext::GpuApi::CommandListContext* context = handle.instance.GetPtr();
		if (!context || !memory_.Range(context, sizeof(*context)))
		{
			continue;
		}
		void* list = context->commandList.Get();
		if (!list || !memory_.Range(list, sizeof(void*)))
		{
			continue;
		}
		if (*static_cast<void**>(list) != listVtable)
		{
			// A Streamline proxy: compare the list underneath, looked up once per log.
			auto found = nativeOf_.find(list);
			if (found == nativeOf_.end())
			{
				found = nativeOf_.emplace(list, slhooks::NativeInterface(list)).first;
			}
			list = found->second;
		}
		if (list == aNativeList)
		{
			return Text(memory_, context->debugName);
		}
	}
	return {};
}
}
