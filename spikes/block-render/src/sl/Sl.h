#pragma once

// Streamline 2.7.2's public types (extern/streamline-2.7.2, MIT), and the checks the plugin makes before it trusts
// what the game hands Streamline. The game's sl.interposer.dll is 2.7.1; the structs carry a type GUID and a version,
// so each one is checked before it's read.
#include <sl_core_api.h>

#include "Readable.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cpmc
{
// What the plugin keeps of one sl::Constants: plain copies, in Streamline's own row-major layout. The flags are
// sl::Boolean values: 0 false, 1 true, 2 not set.
struct SlConstants
{
	uint64_t              structVersion = 0;
	std::array<float, 16> viewToClip{};
	std::array<float, 16> clipToView{};
	std::array<float, 16> clipToPrevClip{};
	std::array<float, 16> prevClipToClip{};
	std::array<float, 2>  jitter{};
	std::array<float, 2>  mvecScale{};
	std::array<float, 2>  pinholeOffset{};
	std::array<float, 3>  position{};
	std::array<float, 3>  up{};
	std::array<float, 3>  right{};
	std::array<float, 3>  forward{};
	float                 nearPlane = 0.0f;
	float                 farPlane = 0.0f;
	float                 fov = 0.0f;  // radians
	float                 aspect = 0.0f;
	float                 motionVectorsInvalidValue = 0.0f;
	uint8_t               depthInverted = 2;
	uint8_t               cameraMotionIncluded = 2;
	uint8_t               motionVectors3D = 2;
	uint8_t               reset = 2;
	uint8_t               orthographic = 2;
	uint8_t               motionVectorsDilated = 2;
	uint8_t               motionVectorsJittered = 2;
};

// One buffer tag: global (from slSetTag) or local (among an slEvaluateFeature's inputs).
struct SlTag
{
	uint32_t   type = 0;
	void*      resource = nullptr;  // the native ID3D12Resource; not referenced
	uint32_t   state = 0xFFFFFFFF;  // its D3D12_RESOURCE_STATES at the evaluate; 0xFFFFFFFF if the game didn't say
	uint32_t   lifecycle = 0;       // sl::ResourceLifecycle
	sl::Extent extent{};            // all zero: the whole resource
	bool       local = false;
};

// "depth", "motionVectors", ... as Streamline names them, or "type <n>" past the ones 2.7.2 knows.
std::string BufferTypeName(uint32_t aType);

// The struct at aStruct as an sl::Constants, if it is one: readable, with Constants' type GUID and version 1 or
// later. Version 1's fields are all the plugin reads.
std::optional<SlConstants> ReadConstants(ReadableMemory& aMemory, const void* aStruct);

// The struct at aStruct as an sl::ResourceTag, with the sl::Resource it points to. A tag whose resource pointer is
// null is valid: the game sets one to drop a tag.
std::optional<SlTag> ReadTag(ReadableMemory& aMemory, const void* aStruct, bool aLocal);

// The value of an sl::ViewportHandle, if aStruct is one.
std::optional<uint32_t> ReadViewport(ReadableMemory& aMemory, const void* aStruct);

// The structures an slEvaluateFeature call was given: each input, then the structures chained to it through `next`.
// At most 32 inputs and 16 structures per chain; an unreadable pointer ends its chain.
std::vector<const sl::BaseStructure*> EvaluateInputs(ReadableMemory& aMemory, const sl::BaseStructure* const* aInputs,
                                                     uint32_t aCount);

// The tags an evaluate uses: the global ones, with each local tag replacing the global tag of its type.
std::vector<SlTag> TagsForEvaluate(const std::vector<SlTag>& aGlobal, const std::vector<SlTag>& aLocal);

// When a frame starts. The game asks Streamline for a frame token several times a frame, from several threads, and
// the main thread works a frame ahead of the render and present threads. So a frame starts when a token comes for a
// newer frame index than any before. A far older index means the count started again, and starts a frame too.
// Safe from any thread.
class NewestFrame
{
public:
	bool Note(uint32_t aFrame);  // true when aFrame starts a frame

private:
	static constexpr uint64_t kNone = ~0ull;
	std::atomic<uint64_t>     newest_{ kNone };
};
}
