#include "log/FrameLogData.h"

#include "Json.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>
#include <numbers>

namespace cpmc
{
namespace
{
// dxgiformat.h's names for formats 0 to 115, which covers every format a game renders with.
constexpr const char* kDxgiNames[] = {
	"UNKNOWN", "R32G32B32A32_TYPELESS", "R32G32B32A32_FLOAT", "R32G32B32A32_UINT", "R32G32B32A32_SINT",
	"R32G32B32_TYPELESS", "R32G32B32_FLOAT", "R32G32B32_UINT", "R32G32B32_SINT", "R16G16B16A16_TYPELESS",
	"R16G16B16A16_FLOAT", "R16G16B16A16_UNORM", "R16G16B16A16_UINT", "R16G16B16A16_SNORM", "R16G16B16A16_SINT",
	"R32G32_TYPELESS", "R32G32_FLOAT", "R32G32_UINT", "R32G32_SINT", "R32G8X24_TYPELESS", "D32_FLOAT_S8X24_UINT",
	"R32_FLOAT_X8X24_TYPELESS", "X32_TYPELESS_G8X24_UINT", "R10G10B10A2_TYPELESS", "R10G10B10A2_UNORM",
	"R10G10B10A2_UINT", "R11G11B10_FLOAT", "R8G8B8A8_TYPELESS", "R8G8B8A8_UNORM", "R8G8B8A8_UNORM_SRGB",
	"R8G8B8A8_UINT", "R8G8B8A8_SNORM", "R8G8B8A8_SINT", "R16G16_TYPELESS", "R16G16_FLOAT", "R16G16_UNORM",
	"R16G16_UINT", "R16G16_SNORM", "R16G16_SINT", "R32_TYPELESS", "D32_FLOAT", "R32_FLOAT", "R32_UINT", "R32_SINT",
	"R24G8_TYPELESS", "D24_UNORM_S8_UINT", "R24_UNORM_X8_TYPELESS", "X24_TYPELESS_G8_UINT", "R8G8_TYPELESS",
	"R8G8_UNORM", "R8G8_UINT", "R8G8_SNORM", "R8G8_SINT", "R16_TYPELESS", "R16_FLOAT", "D16_UNORM", "R16_UNORM",
	"R16_UINT", "R16_SNORM", "R16_SINT", "R8_TYPELESS", "R8_UNORM", "R8_UINT", "R8_SNORM", "R8_SINT", "A8_UNORM",
	"R1_UNORM", "R9G9B9E5_SHAREDEXP", "R8G8_B8G8_UNORM", "G8R8_G8B8_UNORM", "BC1_TYPELESS", "BC1_UNORM",
	"BC1_UNORM_SRGB", "BC2_TYPELESS", "BC2_UNORM", "BC2_UNORM_SRGB", "BC3_TYPELESS", "BC3_UNORM", "BC3_UNORM_SRGB",
	"BC4_TYPELESS", "BC4_UNORM", "BC4_SNORM", "BC5_TYPELESS", "BC5_UNORM", "BC5_SNORM", "B5G6R5_UNORM",
	"B5G5R5A1_UNORM", "B8G8R8A8_UNORM", "B8G8R8X8_UNORM", "R10G10B10_XR_BIAS_A2_UNORM", "B8G8R8A8_TYPELESS",
	"B8G8R8A8_UNORM_SRGB", "B8G8R8X8_TYPELESS", "B8G8R8X8_UNORM_SRGB", "BC6H_TYPELESS", "BC6H_UF16", "BC6H_SF16",
	"BC7_TYPELESS", "BC7_UNORM", "BC7_UNORM_SRGB", "AYUV", "Y410", "Y416", "NV12", "P010", "P016", "420_OPAQUE",
	"YUY2", "Y210", "Y216", "NV11", "AI44", "IA44", "P8", "A8P8", "B4G4R4A4_UNORM"
};

std::string Hex(uint64_t aValue)
{
	return std::format("0x{:x}", aValue);
}

template<class T>
void Numbers(JsonWriter& aJson, const T& aValues)
{
	aJson.BeginArray();
	for (const auto value : aValues)
	{
		aJson.Number(double(value));
	}
	aJson.EndArray();
}

const char* Boolean(uint8_t aValue)
{
	return aValue == 0 ? "false" : aValue == 1 ? "true" : "unset";
}

const char* Lifecycle(uint32_t aValue)
{
	switch (aValue)
	{
	case 0: return "onlyValidNow";
	case 1: return "validUntilPresent";
	case 2: return "validUntilEvaluate";
	default: return "unknown";
	}
}

void Target(JsonWriter& aJson, const BoundTarget& aTarget)
{
	aJson.BeginObject().Key("handle").String(Hex(aTarget.handle));
	if (aTarget.view)
	{
		const ViewInfo& view = *aTarget.view;
		aJson.Key("resource").String(Hex(reinterpret_cast<uintptr_t>(view.resource)));
		aJson.Key("format").String(DxgiFormatName(view.viewFormat));
		aJson.Key("size").BeginArray().UInt(view.width).UInt(view.height).EndArray();
		if (view.plane != 0 || view.mip != 0 || view.firstSlice != 0)
		{
			aJson.Key("subresource").BeginArray().UInt(view.mip).UInt(view.firstSlice).UInt(view.plane).EndArray();
		}
		if (view.readOnlyDepth || view.readOnlyStencil)
		{
			aJson.Key("readOnly").String(view.readOnlyDepth ? (view.readOnlyStencil ? "depth, stencil" : "depth") : "stencil");
		}
	}
	else
	{
		aJson.Key("unknown").Bool(true);
	}
	aJson.EndObject();
}

void Pass(JsonWriter& aJson, const PassRecord& aPass, const void* aTaggedDepth)
{
	aJson.BeginObject();
	aJson.Key("targets").BeginArray();
	for (const BoundTarget& target : aPass.targets.rtvs)
	{
		Target(aJson, target);
	}
	aJson.EndArray();
	aJson.Key("depth");
	if (aPass.targets.dsv)
	{
		Target(aJson, *aPass.targets.dsv);
	}
	else
	{
		aJson.Null();
	}
	const bool tagged = aPass.targets.dsv && aPass.targets.dsv->view && aTaggedDepth &&
	                    aPass.targets.dsv->view->resource == aTaggedDepth;
	aJson.Key("depthIsTagged").Bool(tagged);
	if (aPass.renderPass)
	{
		aJson.Key("renderPass").Bool(true);
	}
	aJson.Key("draws").UInt(aPass.draws);
	aJson.Key("indexedDraws").UInt(aPass.indexedDraws);
	aJson.Key("indirect").UInt(aPass.indirect);
	aJson.Key("bundles").UInt(aPass.bundles);
	aJson.Key("dispatches").UInt(aPass.dispatches);
	aJson.Key("clears").UInt(aPass.clears);
	aJson.Key("barriers").UInt(aPass.barriers);
	aJson.Key("enhancedBarriers").UInt(aPass.enhancedBarriers);
	aJson.Key("events").BeginArray();
	for (const std::string& event : aPass.events)
	{
		aJson.String(event);
	}
	aJson.EndArray();
	aJson.EndObject();
}

// The tagged depth buffer, from the log's latest depth tag.
const void* TaggedDepth(const FrameLogData& aLog)
{
	const void* depth = nullptr;
	for (const LogTag& tag : aLog.tags)
	{
		if (tag.tag.type == sl::kBufferTypeDepth && tag.tag.resource)
		{
			depth = tag.tag.resource;
		}
	}
	return depth;
}

void Device(JsonWriter& aJson, const LogDevice& aDevice)
{
	aJson.BeginObject();
	aJson.Key("seenAtCreation").Bool(aDevice.seenAtCreation);
	aJson.Key("device").String(Hex(aDevice.device));
	aJson.Key("gpuApi").BeginObject();
	aJson.Key("device").String(aDevice.gpuApiDevice);
	aJson.Key("matchesDetour").Bool(aDevice.gpuApiMatches);
	aJson.Key("queue").String(aDevice.gpuApiQueue);
	aJson.Key("swapChain").String(aDevice.gpuApiSwapChain);
	aJson.EndObject();
	aJson.Key("streamline").String(aDevice.streamline);
	aJson.Key("views").BeginObject();
	aJson.Key("rtvs").UInt(aDevice.rtvs).Key("dsvs").UInt(aDevice.dsvs).Key("unknownHandles").UInt(aDevice.unknownHandles);
	aJson.EndObject();
	aJson.Key("patches").BeginObject().Key("vtableEntries").UInt(aDevice.vtableEntries).Key("imports").UInt(aDevice.imports).EndObject();
	aJson.EndObject();
}

void Constants(JsonWriter& aJson, const LogConstants& aEntry)
{
	const SlConstants& c = aEntry.constants;
	aJson.BeginObject();
	aJson.Key("frame").UInt(aEntry.frame);
	aJson.Key("viewport").UInt(aEntry.viewport);
	aJson.Key("version").UInt(c.structVersion);
	aJson.Key("viewToClip");
	Numbers(aJson, c.viewToClip);
	aJson.Key("clipToView");
	Numbers(aJson, c.clipToView);
	aJson.Key("clipToPrevClip");
	Numbers(aJson, c.clipToPrevClip);
	aJson.Key("prevClipToClip");
	Numbers(aJson, c.prevClipToClip);
	aJson.Key("jitter");
	Numbers(aJson, c.jitter);
	aJson.Key("mvecScale");
	Numbers(aJson, c.mvecScale);
	aJson.Key("pinholeOffset");
	Numbers(aJson, c.pinholeOffset);
	aJson.Key("position");
	Numbers(aJson, c.position);
	aJson.Key("up");
	Numbers(aJson, c.up);
	aJson.Key("right");
	Numbers(aJson, c.right);
	aJson.Key("forward");
	Numbers(aJson, c.forward);
	aJson.Key("near").Number(c.nearPlane);
	aJson.Key("far").Number(c.farPlane);
	aJson.Key("fov").Number(c.fov);
	aJson.Key("fovDegrees").Number(c.fov * 180.0 / std::numbers::pi);
	aJson.Key("aspect").Number(c.aspect);
	aJson.Key("motionVectorsInvalidValue").Number(c.motionVectorsInvalidValue);
	aJson.Key("depthInverted").String(Boolean(c.depthInverted));
	aJson.Key("cameraMotionIncluded").String(Boolean(c.cameraMotionIncluded));
	aJson.Key("motionVectors3D").String(Boolean(c.motionVectors3D));
	aJson.Key("reset").String(Boolean(c.reset));
	aJson.Key("orthographic").String(Boolean(c.orthographic));
	aJson.Key("motionVectorsDilated").String(Boolean(c.motionVectorsDilated));
	aJson.Key("motionVectorsJittered").String(Boolean(c.motionVectorsJittered));
	aJson.EndObject();
}

void Tag(JsonWriter& aJson, const LogTag& aEntry)
{
	const SlTag& tag = aEntry.tag;
	aJson.BeginObject();
	aJson.Key("frame").UInt(aEntry.frame);
	aJson.Key("viewport").UInt(aEntry.viewport);
	aJson.Key("local").Bool(tag.local);
	aJson.Key("type").UInt(tag.type);
	aJson.Key("name").String(BufferTypeName(tag.type));
	aJson.Key("resource").String(Hex(reinterpret_cast<uintptr_t>(tag.resource)));
	if (aEntry.resource.known)
	{
		aJson.Key("format").String(DxgiFormatName(aEntry.resource.format));
		aJson.Key("size").BeginArray().UInt(aEntry.resource.width).UInt(aEntry.resource.height).EndArray();
		aJson.Key("flags").String(Hex(aEntry.resource.flags));
		aJson.Key("mips").UInt(aEntry.resource.mips);
		aJson.Key("depthOrArraySize").UInt(aEntry.resource.depthOrArraySize);
	}
	aJson.Key("state").String(Hex(tag.state));
	aJson.Key("lifecycle").String(Lifecycle(tag.lifecycle));
	aJson.Key("extent").BeginArray().UInt(tag.extent.top).UInt(tag.extent.left).UInt(tag.extent.width).UInt(tag.extent.height).EndArray();
	aJson.Key("list").String(Hex(aEntry.list));
	aJson.EndObject();
}

double Round3(double aValue)
{
	return std::round(aValue * 1000.0) / 1000.0;
}
}

std::string DxgiFormatName(uint32_t aFormat)
{
	if (aFormat < std::size(kDxgiNames))
	{
		return kDxgiNames[aFormat];
	}
	return std::format("format {}", aFormat);
}

std::vector<LogTransition> TransitionsOf(const ListState& aState)
{
	std::vector<LogTransition> transitions;
	for (const auto& [resource, first] : aState.FirstTransitions())
	{
		transitions.push_back({ reinterpret_cast<uintptr_t>(resource), first.before, first.subresource, first.flags,
		                        aState.StateOf(resource) });
	}
	std::sort(transitions.begin(), transitions.end(),
	          [](const LogTransition& aA, const LogTransition& aB) { return aA.resource < aB.resource; });
	return transitions;
}

std::string FrameLogJson(const FrameLogData& aLog)
{
	JsonWriter json;
	json.BeginObject();
	json.Key("pluginVersion").String(aLog.pluginVersion);
	json.Key("time").String(aLog.time);
	json.Key("frames").BeginArray().UInt(aLog.firstFrame).UInt(aLog.lastFrame).EndArray();
	json.Key("device");
	Device(json, aLog.device);

	json.Key("scriptCamera");
	if (aLog.scriptCamera)
	{
		const LogScriptCamera& camera = *aLog.scriptCamera;
		json.BeginObject();
		json.Key("position").BeginArray().Number(camera.position.x).Number(camera.position.y).Number(camera.position.z).EndArray();
		json.Key("forward").BeginArray().Number(camera.forward.x).Number(camera.forward.y).Number(camera.forward.z).EndArray();
		json.Key("fov").Number(camera.fov);
		json.EndObject();
	}
	else
	{
		json.Null();
	}

	json.Key("constants").BeginArray();
	for (const LogConstants& entry : aLog.constants)
	{
		Constants(json, entry);
	}
	json.EndArray();

	json.Key("tags").BeginArray();
	for (const LogTag& entry : aLog.tags)
	{
		Tag(json, entry);
	}
	json.EndArray();

	json.Key("evaluates").BeginArray();
	for (const LogEvaluate& evaluate : aLog.evaluates)
	{
		json.BeginObject();
		json.Key("frame").UInt(evaluate.frame);
		json.Key("feature").UInt(evaluate.feature);
		json.Key("viewport").UInt(evaluate.viewport);
		json.Key("list").String(Hex(evaluate.list));
		json.Key("nativeList").String(Hex(evaluate.nativeList));
		json.EndObject();
	}
	json.EndArray();

	json.Key("events").BeginArray();
	for (const LogEvent& event : aLog.events)
	{
		json.BeginObject();
		json.Key("ms").Number(Round3(event.ms));
		json.Key("frame").UInt(event.frame);
		json.Key("thread").UInt(event.thread);
		json.Key("what").String(event.what);
		json.Key("detail").String(event.detail);
		json.EndObject();
	}
	json.EndArray();

	json.Key("submissions").BeginArray();
	for (const LogSubmission& submission : aLog.submissions)
	{
		json.BeginObject();
		json.Key("ms").Number(Round3(submission.ms));
		json.Key("frame").UInt(submission.frame);
		json.Key("queue").String(Hex(submission.queue));
		json.Key("lists").BeginArray();
		for (const uint64_t list : submission.lists)
		{
			json.String(Hex(list));
		}
		json.EndArray();
		json.Key("untracked").UInt(submission.untracked);
		json.EndObject();
	}
	json.EndArray();

	const void* taggedDepth = TaggedDepth(aLog);
	json.Key("lists").BeginArray();
	for (const LogList& list : aLog.lists)
	{
		json.BeginObject();
		json.Key("list").String(Hex(list.list));
		json.Key("name").String(list.name);
		json.Key("frame").UInt(list.frame);
		json.Key("passes").BeginArray();
		for (const PassRecord& pass : list.passes)
		{
			Pass(json, pass, taggedDepth);
		}
		json.EndArray();
		json.Key("transitions").BeginArray();
		for (const LogTransition& transition : list.transitions)
		{
			json.BeginObject();
			json.Key("resource").String(Hex(transition.resource));
			json.Key("firstBefore").String(Hex(transition.firstBefore));
			json.Key("firstSubresource");
			transition.firstSubresource == ListState::kAllSubresources ? json.String("all") : json.UInt(transition.firstSubresource);
			json.Key("firstFlags").UInt(transition.firstFlags);
			json.Key("after");
			transition.after ? json.String(Hex(*transition.after)) : json.Null();
			json.EndObject();
		}
		json.EndArray();
		json.EndObject();
	}
	json.EndArray();

	json.Key("gbufferCandidates").BeginArray();
	for (const LogCandidate& candidate : aLog.gbufferCandidates)
	{
		json.BeginObject().Key("list").String(Hex(candidate.list)).Key("pass").UInt(candidate.pass).EndObject();
	}
	json.EndArray();
	json.EndObject();
	return json.Str() + "\n";
}
}
