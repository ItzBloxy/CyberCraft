#include "hooks/StreamlineHooks.h"

#include "hooks/D3D12Hooks.h"
#include "hooks/HookGroups.h"
#include "hooks/ImportPatch.h"

#include <Windows.h>
#include <psapi.h>

#include <array>
#include <atomic>
#include <format>
#include <map>
#include <mutex>

namespace cpmc::slhooks
{
namespace
{
using GetNewFrameTokenFn = sl::Result (*)(sl::FrameToken*&, const uint32_t*);
using SetConstantsFn = sl::Result (*)(const sl::Constants&, const sl::FrameToken&, const sl::ViewportHandle&);
using SetTagFn = sl::Result (*)(const sl::ViewportHandle&, const sl::ResourceTag*, uint32_t, sl::CommandBuffer*);
using EvaluateFeatureFn = sl::Result (*)(sl::Feature, const sl::FrameToken&, const sl::BaseStructure**, uint32_t, sl::CommandBuffer*);
using GetNativeInterfaceFn = sl::Result (*)(void*, void**);

constexpr const char* kDll = "sl.interposer.dll";
constexpr uint32_t    kMaxTags = 64;
constexpr size_t      kConstantsKept = 8;

LogQueue*                  g_log = nullptr;
std::array<HookEvents*, 4> g_listeners{};
size_t                     g_listenerCount = 0;
std::vector<ImportPatch>   g_patches;
GetNewFrameTokenFn         g_getNewFrameToken = nullptr;
SetConstantsFn             g_setConstants = nullptr;
SetTagFn                   g_setTag = nullptr;
EvaluateFeatureFn          g_evaluateFeature = nullptr;
GetNativeInterfaceFn       g_getNativeInterface = nullptr;
uintptr_t                  g_slBase = 0;
uintptr_t                  g_slEnd = 0;
std::atomic<uint64_t>      g_frameTokens{ 0 };  // frames started
std::atomic<uint64_t>      g_tokenCalls{ 0 };   // every slGetNewFrameToken
std::atomic<uint64_t>      g_evaluates{ 0 };
NewestFrame                g_newestFrame;

// What Streamline has been told, by viewport.
struct Viewport
{
	std::map<uint32_t, SlTag>                          tags;       // the latest global tag of each type
	std::vector<std::pair<uint32_t, SlConstants>>      constants;  // the latest few frames' constants
};
std::mutex                   g_mutex;
std::map<uint32_t, Viewport> g_viewports;

template<class Fn>
void Each(Fn&& aFn)
{
	for (size_t i = 0; i < g_listenerCount; ++i)
	{
		aFn(*g_listeners[i]);
	}
}

// A frame token's index, through its virtual operator: the token is Streamline's own object.
uint32_t FrameOf(const sl::FrameToken& aToken)
{
	return static_cast<uint32_t>(aToken);
}

sl::Result GetNewFrameToken(sl::FrameToken*& aToken, const uint32_t* aFrameIndex)
{
	const sl::Result result = g_getNewFrameToken(aToken, aFrameIndex);
	if (result == sl::Result::eOk && aToken && HookGroupOn(HookGroup::Streamline))
	{
		++g_tokenCalls;
		const uint32_t frame = FrameOf(*aToken);
		if (g_newestFrame.Note(frame))
		{
			d3d::SetCurrentFrame(frame);
			++g_frameTokens;
			Each([&](HookEvents& aListener) { aListener.OnFrameToken(frame); });
		}
	}
	return result;
}

sl::Result SetConstants(const sl::Constants& aValues, const sl::FrameToken& aFrame, const sl::ViewportHandle& aViewport)
{
	if (!HookGroupOn(HookGroup::Streamline))
	{
		return g_setConstants(aValues, aFrame, aViewport);
	}
	ReadableMemory memory;
	const auto     constants = ReadConstants(memory, &aValues);
	const auto     viewport = ReadViewport(memory, &aViewport);
	if (constants && viewport)
	{
		const uint32_t frame = FrameOf(aFrame);
		{
			std::lock_guard lock(g_mutex);
			auto&           kept = g_viewports[*viewport].constants;
			kept.emplace_back(frame, *constants);
			if (kept.size() > kConstantsKept)
			{
				kept.erase(kept.begin());
			}
		}
		Each([&](HookEvents& aListener) { aListener.OnConstants(frame, *viewport, *constants); });
	}
	return g_setConstants(aValues, aFrame, aViewport);
}

sl::Result SetTag(const sl::ViewportHandle& aViewport, const sl::ResourceTag* aTags, uint32_t aCount, sl::CommandBuffer* aList)
{
	if (!HookGroupOn(HookGroup::Streamline))
	{
		return g_setTag(aViewport, aTags, aCount, aList);
	}
	ReadableMemory     memory;
	const auto         viewport = ReadViewport(memory, &aViewport);
	std::vector<SlTag> tags;
	for (uint32_t i = 0; viewport && aTags && i < aCount && i < kMaxTags; ++i)
	{
		if (const auto tag = ReadTag(memory, &aTags[i], false))
		{
			tags.push_back(*tag);
		}
	}
	if (viewport && !tags.empty())
	{
		{
			std::lock_guard lock(g_mutex);
			auto&           kept = g_viewports[*viewport].tags;
			for (const SlTag& tag : tags)
			{
				if (tag.resource)
				{
					kept[tag.type] = tag;
				}
				else
				{
					kept.erase(tag.type);  // the game dropped this tag
				}
			}
		}
		const uint32_t frame = d3d::CurrentFrame();
		Each([&](HookEvents& aListener) { aListener.OnTags(frame, *viewport, tags, aList); });
	}
	return g_setTag(aViewport, aTags, aCount, aList);
}

sl::Result EvaluateFeature(sl::Feature aFeature, const sl::FrameToken& aFrame, const sl::BaseStructure** aInputs, uint32_t aCount,
                           sl::CommandBuffer* aList)
{
	if (aFeature == sl::kFeatureDLSS && HookGroupOn(HookGroup::Streamline))
	{
		++g_evaluates;
		ReadableMemory     memory;
		EvaluateCall       call;
		std::vector<SlTag> local;
		call.frame = FrameOf(aFrame);
		call.gameList = aList;
		for (const sl::BaseStructure* input : EvaluateInputs(memory, aInputs, aCount))
		{
			if (const auto viewport = ReadViewport(memory, input))
			{
				call.viewport = *viewport;
			}
			else if (const auto tag = ReadTag(memory, input, true))
			{
				local.push_back(*tag);
			}
		}
		{
			std::lock_guard    lock(g_mutex);
			Viewport&          viewport = g_viewports[call.viewport];
			std::vector<SlTag> global;
			for (const auto& [type, tag] : viewport.tags)
			{
				global.push_back(tag);
			}
			call.tags = TagsForEvaluate(global, local);
			for (const auto& [frame, constants] : viewport.constants)
			{
				if (frame == call.frame)
				{
					call.constants = constants;
				}
			}
		}
		call.list = NativeList(aList);
		d3d::NoteEvaluateList(call.list);
		Each([&](HookEvents& aListener) { aListener.OnEvaluate(call); });
		if (call.list)
		{
			call.list->Release();
		}
	}
	return g_evaluateFeature(aFeature, aFrame, aInputs, aCount, aList);
}

// Stores the original before the entry changes, so a call arriving mid-way still reaches Streamline.
template<class Fn>
bool PatchOne(HMODULE aExe, const char* aFunction, void* aReplacement, Fn& aOriginal)
{
	auto patch = FindImport(aExe, kDll, aFunction);
	if (!patch)
	{
		return false;
	}
	aOriginal = reinterpret_cast<Fn>(patch->original);
	if (!ApplyImport(*patch, aReplacement))
	{
		return false;
	}
	g_patches.push_back(*patch);
	return true;
}
}

bool Install(LogQueue& aLog)
{
	g_log = &aLog;
	const HMODULE interposer = GetModuleHandleA(kDll);
	MODULEINFO    info{};
	if (!interposer || !GetModuleInformation(GetCurrentProcess(), interposer, &info, sizeof(info)))
	{
		aLog.Push("sl.interposer.dll isn't loaded: nothing hooked");
		return false;
	}
	g_slBase = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
	g_slEnd = g_slBase + info.SizeOfImage;
	g_getNativeInterface = reinterpret_cast<GetNativeInterfaceFn>(GetProcAddress(interposer, "slGetNativeInterface"));
	const HMODULE exe = GetModuleHandleW(nullptr);
	const bool    ok = PatchOne(exe, "slGetNewFrameToken", reinterpret_cast<void*>(&GetNewFrameToken), g_getNewFrameToken) &&
	                PatchOne(exe, "slSetConstants", reinterpret_cast<void*>(&SetConstants), g_setConstants) &&
	                PatchOne(exe, "slSetTag", reinterpret_cast<void*>(&SetTag), g_setTag) &&
	                PatchOne(exe, "slEvaluateFeature", reinterpret_cast<void*>(&EvaluateFeature), g_evaluateFeature);
	if (!ok)
	{
		Remove();
		aLog.Push("the game doesn't import all four Streamline functions by name: nothing hooked");
		return false;
	}
	aLog.Push(std::format("Streamline hooked: {} imports patched", g_patches.size()));
	return true;
}

void Remove()
{
	for (const ImportPatch& patch : g_patches)
	{
		RestoreImport(patch);
	}
	g_patches.clear();
}

void AddListener(HookEvents* aListener)
{
	if (g_listenerCount < g_listeners.size())
	{
		g_listeners[g_listenerCount++] = aListener;
	}
}

uint64_t FrameTokens()
{
	return g_frameTokens.load();
}

void* NativeInterface(void* aObject)
{
	ReadableMemory memory;
	if (!aObject || !g_getNativeInterface || !memory.Range(aObject, sizeof(void*)))
	{
		return nullptr;
	}
	const auto vtable = *static_cast<uintptr_t*>(aObject);
	void*      native = nullptr;
	if (vtable < g_slBase || vtable >= g_slEnd || g_getNativeInterface(aObject, &native) != sl::Result::eOk)
	{
		return nullptr;
	}
	return native;
}

ID3D12GraphicsCommandList* NativeList(void* aGameList)
{
	void* const listVtable = d3d::ListVtable();
	ReadableMemory memory;
	if (!aGameList || !listVtable || !memory.Range(aGameList, sizeof(void*)))
	{
		return nullptr;
	}
	const auto vtable = *static_cast<uintptr_t*>(aGameList);
	if (vtable == reinterpret_cast<uintptr_t>(listVtable))
	{
		auto* list = static_cast<ID3D12GraphicsCommandList*>(aGameList);
		list->AddRef();
		return list;
	}
	// A Streamline proxy's vtable lies in sl.interposer.dll. Streamline hands back the native list with a reference.
	if (void* native = NativeInterface(aGameList))
	{
		if (memory.Range(native, sizeof(void*)) && *static_cast<uintptr_t*>(native) == reinterpret_cast<uintptr_t>(listVtable))
		{
			return static_cast<ID3D12GraphicsCommandList*>(native);
		}
		static_cast<IUnknown*>(native)->Release();
	}
	return nullptr;
}

std::string Summary()
{
	return std::format("{} Streamline imports patched, {} frames ({} token calls), {} DLSS evaluates", g_patches.size(),
	                   g_frameTokens.load(), g_tokenCalls.load(), g_evaluates.load());
}
}
