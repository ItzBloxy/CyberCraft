#include "sl/Sl.h"

#include <cstddef>
#include <cstring>
#include <format>
#include <string>

namespace cpmc
{
// The plugin reads these offsets; their values are pinned by tests/TestSl.cpp.
static_assert(sizeof(sl::BaseStructure) == 32);
static_assert(offsetof(sl::Constants, motionVectorsJittered) == 450);
static_assert(offsetof(sl::ResourceTag, extent) == 48);
static_assert(offsetof(sl::Resource, state) == 64);

namespace
{
// Every version-1 field of sl::Constants ends before this many bytes.
constexpr size_t kConstantsV1Bytes = offsetof(sl::Constants, motionVectorsJittered) + 1;
// Of sl::Resource the plugin reads up to its state.
constexpr size_t kResourceBytes = offsetof(sl::Resource, state) + sizeof(uint32_t);

constexpr const char* kBufferTypeNames[] = {
	"depth", "motionVectors", "hudLessColor", "scalingInputColor", "scalingOutputColor", "normals", "roughness",
	"albedo", "specularAlbedo", "indirectAlbedo", "specularMotionVectors", "disocclusionMask", "emissive", "exposure",
	"normalRoughness", "diffuseHitNoisy", "diffuseHitDenoised", "specularHitNoisy", "specularHitDenoised",
	"shadowNoisy", "shadowDenoised", "ambientOcclusionNoisy", "ambientOcclusionDenoised", "uiColorAndAlpha",
	"shadowHint", "reflectionHint", "particleHint", "transparencyHint", "animatedTextureHint", "biasCurrentColorHint",
	"raytracingDistance", "reflectionMotionVectors", "position", "invalidDepthMotionHint", "alpha", "opaqueColor",
	"reactiveMaskHint", "transparencyAndCompositionMaskHint", "reflectedAlbedo", "colorBeforeParticles",
	"colorBeforeTransparency", "colorBeforeFog", "specularHitDistance", "specularRayDirectionHitDistance",
	"specularRayDirection", "diffuseHitDistance", "diffuseRayDirectionHitDistance", "diffuseRayDirection",
	"hiResDepth", "linearDepth", "bidirectionalDistortionField", "transparencyLayer", "transparencyLayerOpacity",
	"backbuffer", "noWarpMask", "colorAfterParticles", "colorAfterTransparency", "colorAfterFog",
	"screenSpaceSubsurfaceScatteringGuide", "colorBeforeScreenSpaceSubsurfaceScattering",
	"colorAfterScreenSpaceSubsurfaceScattering", "screenSpaceRefractionGuide", "colorBeforeScreenSpaceRefraction",
	"colorAfterScreenSpaceRefraction", "depthOfFieldGuide", "colorBeforeDepthOfField", "colorAfterDepthOfField"
};
static_assert(std::size(kBufferTypeNames) == sl::kBufferTypeColorAfterDepthOfField + 1);

// A readable struct of at least aBytes, with aType's GUID and at least aVersion. aBytes covers the base too.
const sl::BaseStructure* Struct(ReadableMemory& aMemory, const void* aStruct, const sl::StructType& aType,
                                uint64_t aVersion, size_t aBytes)
{
	if (!aMemory.Range(aStruct, aBytes))
	{
		return nullptr;
	}
	const auto* base = static_cast<const sl::BaseStructure*>(aStruct);
	if (base->structType != aType || base->structVersion < aVersion)
	{
		return nullptr;
	}
	return base;
}

template<size_t N>
std::array<float, N> Copy(const float* aFrom)
{
	std::array<float, N> to{};
	std::memcpy(to.data(), aFrom, sizeof(float) * N);
	return to;
}

std::array<float, 16> CopyMatrix(const sl::float4x4& aMatrix)
{
	return Copy<16>(&aMatrix.row[0].x);
}
}

std::string BufferTypeName(uint32_t aType)
{
	if (aType < std::size(kBufferTypeNames))
	{
		return kBufferTypeNames[aType];
	}
	return std::format("type {}", aType);
}

std::optional<SlConstants> ReadConstants(ReadableMemory& aMemory, const void* aStruct)
{
	const auto* base = Struct(aMemory, aStruct, sl::Constants::s_structType, sl::kStructVersion1, kConstantsV1Bytes);
	if (!base)
	{
		return std::nullopt;
	}
	const auto& from = *static_cast<const sl::Constants*>(base);
	SlConstants to;
	to.structVersion = from.structVersion;
	to.viewToClip = CopyMatrix(from.cameraViewToClip);
	to.clipToView = CopyMatrix(from.clipToCameraView);
	to.clipToPrevClip = CopyMatrix(from.clipToPrevClip);
	to.prevClipToClip = CopyMatrix(from.prevClipToClip);
	to.jitter = { from.jitterOffset.x, from.jitterOffset.y };
	to.mvecScale = { from.mvecScale.x, from.mvecScale.y };
	to.pinholeOffset = { from.cameraPinholeOffset.x, from.cameraPinholeOffset.y };
	to.position = { from.cameraPos.x, from.cameraPos.y, from.cameraPos.z };
	to.up = { from.cameraUp.x, from.cameraUp.y, from.cameraUp.z };
	to.right = { from.cameraRight.x, from.cameraRight.y, from.cameraRight.z };
	to.forward = { from.cameraFwd.x, from.cameraFwd.y, from.cameraFwd.z };
	to.nearPlane = from.cameraNear;
	to.farPlane = from.cameraFar;
	to.fov = from.cameraFOV;
	to.aspect = from.cameraAspectRatio;
	to.motionVectorsInvalidValue = from.motionVectorsInvalidValue;
	to.depthInverted = static_cast<uint8_t>(from.depthInverted);
	to.cameraMotionIncluded = static_cast<uint8_t>(from.cameraMotionIncluded);
	to.motionVectors3D = static_cast<uint8_t>(from.motionVectors3D);
	to.reset = static_cast<uint8_t>(from.reset);
	to.orthographic = static_cast<uint8_t>(from.orthographicProjection);
	to.motionVectorsDilated = static_cast<uint8_t>(from.motionVectorsDilated);
	to.motionVectorsJittered = static_cast<uint8_t>(from.motionVectorsJittered);
	return to;
}

std::optional<SlTag> ReadTag(ReadableMemory& aMemory, const void* aStruct, bool aLocal)
{
	const auto* base = Struct(aMemory, aStruct, sl::ResourceTag::s_structType, sl::kStructVersion1, sizeof(sl::ResourceTag));
	if (!base)
	{
		return std::nullopt;
	}
	const auto& tag = *static_cast<const sl::ResourceTag*>(base);
	SlTag       to;
	to.type = tag.type;
	to.lifecycle = static_cast<uint32_t>(tag.lifecycle);
	to.extent = tag.extent;
	to.local = aLocal;
	if (tag.resource)
	{
		const auto* resource = Struct(aMemory, tag.resource, sl::Resource::s_structType, sl::kStructVersion1, kResourceBytes);
		if (!resource)
		{
			return std::nullopt;
		}
		const auto& native = *static_cast<const sl::Resource*>(resource);
		to.resource = native.native;
		to.state = native.state;
	}
	return to;
}

std::optional<uint32_t> ReadViewport(ReadableMemory& aMemory, const void* aStruct)
{
	const auto* base = Struct(aMemory, aStruct, sl::ViewportHandle::s_structType, sl::kStructVersion1, sizeof(sl::ViewportHandle));
	if (!base)
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(*static_cast<const sl::ViewportHandle*>(base));
}

std::vector<const sl::BaseStructure*> EvaluateInputs(ReadableMemory& aMemory, const sl::BaseStructure* const* aInputs,
                                                     uint32_t aCount)
{
	std::vector<const sl::BaseStructure*> structs;
	const uint32_t                        count = aCount < 32 ? aCount : 32;
	if (count == 0 || !aMemory.Range(aInputs, count * sizeof(aInputs[0])))
	{
		return structs;
	}
	for (uint32_t i = 0; i < count; ++i)
	{
		const sl::BaseStructure* at = aInputs[i];
		for (int depth = 0; at && depth < 16; ++depth)
		{
			if (!aMemory.Range(at, sizeof(sl::BaseStructure)))
			{
				break;
			}
			structs.push_back(at);
			at = at->next;
		}
	}
	return structs;
}

std::vector<SlTag> TagsForEvaluate(const std::vector<SlTag>& aGlobal, const std::vector<SlTag>& aLocal)
{
	std::vector<SlTag> tags;
	for (const SlTag& global : aGlobal)
	{
		bool replaced = false;
		for (const SlTag& local : aLocal)
		{
			replaced = replaced || local.type == global.type;
		}
		if (!replaced)
		{
			tags.push_back(global);
		}
	}
	tags.insert(tags.end(), aLocal.begin(), aLocal.end());
	return tags;
}

bool NewestFrame::Note(uint32_t aFrame)
{
	constexpr uint64_t kRestart = 1000;  // frames: far more than any thread works behind the main thread
	uint64_t           newest = newest_.load();
	while (newest == kNone || aFrame > newest || newest - aFrame > kRestart)
	{
		if (newest_.compare_exchange_weak(newest, aFrame))
		{
			return true;
		}
	}
	return false;
}
}
