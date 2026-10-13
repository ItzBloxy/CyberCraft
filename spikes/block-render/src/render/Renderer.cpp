#include "render/Renderer.h"

#include "Measurements.h"
#include "hooks/D3D12Hooks.h"
#include "log/DdsWriter.h"
#include "log/FrameLogData.h"
#include "render/Pipeline.h"
#include "render/Predlss.h"

#include <dxgi1_4.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>
#include <utility>

namespace cpmc
{
namespace
{
constexpr uint64_t kConstantsStride = 512;  // a frame's constants, rounded up to a constant buffer view's alignment
static_assert(sizeof(FrameConstants) <= kConstantsStride && kConstantsStride % 256 == 0);
constexpr uint32_t kSkipsBeforeOff = 30;     // the spec's "Checks before every draw"
constexpr int64_t  kTrackedFramesAgo = 3;    // a DLSS list tracked longer ago than this was recorded again untracked

// Our descriptors, by their index in our CPU-only heaps.
constexpr uint32_t kBackBufferRtv = 0;
constexpr uint32_t kPredlssRtvs = 1;  // the colour, then the motion vectors
constexpr uint32_t kGbufferRtvs = 3;  // the base colour, normals, material values and motion vectors
constexpr uint32_t kForwardRtvs = 7;  // step B: the colour, then the bias hint
constexpr uint32_t kRtvs = 9;
constexpr uint32_t kOwnDepthDsv = 0;
constexpr uint32_t kPredlssDsv = 1;
constexpr uint32_t kGbufferDsv = 2;
constexpr uint32_t kTaggedDepthDsv = 3;
constexpr uint32_t kForwardDsv = 4;  // step B: the tagged depth, read-only
constexpr uint32_t kDsvs = 5;

// The G-buffer's render targets in order, as step 1 found them (MainGbuffer checks the game's views have them).
constexpr DXGI_FORMAT kGbufferFormats[4] = { DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM,
	                                         DXGI_FORMAT_R16G16_FLOAT };

// The slot of the list ListBefore handed to this thread's ExecuteCommandLists call, for OnSubmitted to signal: the
// same thread makes both calls, while the main thread may change the mode in between.
thread_local std::optional<uint32_t> t_insertedSlot;
// Step B: the forward list's slot, kept apart, as one call could hold both of a frame's lists of ours.
thread_local std::optional<uint32_t> t_insertedForward;

int64_t Now()
{
	LARGE_INTEGER now{};
	QueryPerformanceCounter(&now);
	return now.QuadPart;
}

double MsBetween(int64_t aFrom, int64_t aTo)
{
	LARGE_INTEGER frequency{};
	QueryPerformanceFrequency(&frequency);
	return double(aTo - aFrom) * 1000.0 / double(frequency.QuadPart);
}

std::optional<std::vector<uint8_t>> ReadWholeFile(const std::filesystem::path& aPath)
{
	std::ifstream in(aPath, std::ios::binary);
	if (!in)
	{
		return std::nullopt;
	}
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// False when all eight corners of the box lie beyond the same side of the view, or behind the camera.
bool Visible(const Mat4& aCameraToClip, const Vec3d& aLow, const Vec3d& aHigh)
{
	int outside[5] = {};
	for (int corner = 0; corner < 8; ++corner)
	{
		const std::array<double, 4> point{ corner & 1 ? aHigh.x : aLow.x, corner & 2 ? aHigh.y : aLow.y,
			                               corner & 4 ? aHigh.z : aLow.z, 1.0 };
		const std::array<double, 4> clip = Transform(point, aCameraToClip);
		outside[0] += clip[0] > clip[3] ? 1 : 0;
		outside[1] += clip[0] < -clip[3] ? 1 : 0;
		outside[2] += clip[1] > clip[3] ? 1 : 0;
		outside[3] += clip[1] < -clip[3] ? 1 : 0;
		outside[4] += clip[3] < 0.01 ? 1 : 0;
	}
	return std::none_of(std::begin(outside), std::end(outside), [](int aCount) { return aCount == 8; });
}

D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* aResource, D3D12_RESOURCE_STATES aBefore, D3D12_RESOURCE_STATES aAfter)
{
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = aResource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = aBefore;
	barrier.Transition.StateAfter = aAfter;
	return barrier;
}

D3D12_CPU_DESCRIPTOR_HANDLE Offset(D3D12_CPU_DESCRIPTOR_HANDLE aHandle, uint32_t aIndex, uint32_t aIncrement)
{
	return { aHandle.ptr + size_t(aIndex) * aIncrement };
}

// Where the tagged depth's image lies in its buffer: the tag's extent, or the whole buffer without one.
sl::Extent DepthExtent(const SlTag& aDepth, const D3D12_RESOURCE_DESC& aDesc)
{
	const bool wholeResource = aDepth.extent.width == 0 || aDepth.extent.height == 0;
	return wholeResource ? sl::Extent{ 0, 0, uint32_t(aDesc.Width), aDesc.Height } : aDepth.extent;
}
}

Renderer::Renderer(LogQueue& aLog, std::filesystem::path aFolder)
	: log_(aLog)
	, folder_(std::move(aFolder))
{
}

bool Renderer::SetMode(std::string_view aMode, std::string& aError)
{
	const bool drawing = aMode == "present" || aMode == "predlss" || aMode == "gbuffer";
	if (!drawing && aMode != "off")
	{
		aError = std::format("there's no mode called \"{}\"", aMode);
		return false;
	}
	if (drawing)
	{
		if (!AtlasData() && !LoadAtlas(aError))
		{
			return false;
		}
		bool haveShaders = false;
		{
			std::lock_guard lock(mutex_);
			haveShaders = shaders_.has_value();
		}
		if (!haveShaders && !ReloadShaders(aError))
		{
			return false;
		}
	}
	std::lock_guard lock(mutex_);
	recorded_ = {};
	recordedList_ = nullptr;
	submitted_ = {};
	predlss_ = {};
	predlssList_ = nullptr;
	latest_ = {};
	latestSubmitted_ = false;
	gbufferFrame_ = {};
	gbufferEnd_ = nullptr;
	DropForward();
	knownStates_.Forget();
	lastDrawnFrame_.reset();
	consecutiveSkips_ = 0;
	const Mode mode = aMode == "present"   ? Mode::Present
	                  : aMode == "predlss" ? Mode::Predlss
	                  : aMode == "gbuffer" ? Mode::Gbuffer
	                                       : Mode::Off;
	const bool tracks = mode == Mode::Predlss || mode == Mode::Gbuffer;  // both start their barriers from tracked lists
	if (tracks && !trackingRequested_)
	{
		d3d::RequestTracking();
		trackingRequested_ = true;
		trackedSince_ = d3d::CurrentFrame() + 1;  // lists reset from the next frame on are tracked
	}
	else if (!tracks)
	{
		ReleaseTracking();
	}
	mode_ = mode;
	return true;
}

std::string Renderer::ModeName() const
{
	switch (mode_.load())
	{
	case Mode::Present: return "present";
	case Mode::Predlss: return "predlss";
	case Mode::Gbuffer: return "gbuffer";
	default: return "off";
	}
}

bool Renderer::LoadAtlas(std::string& aError)
{
	const auto bytes = ReadWholeFile(folder_ / "atlas.bin");
	if (!bytes)
	{
		aError = "there's no atlas.bin in the plugin's folder: build it with tools/build_atlas.py";
		return false;
	}
	auto atlas = ParseAtlas(*bytes, aError);
	if (!atlas)
	{
		aError = "atlas.bin: " + aError;
		return false;
	}
	std::lock_guard lock(mutex_);
	atlas_ = std::move(*atlas);
	atlasUploaded_ = false;
	dirty_.Clear();  // a new atlas: its whole upload is due
	log_.Push(std::format("atlas: {} x {}, {} levels, {} blocks", atlas_->width, atlas_->height, atlas_->Levels(), atlas_->blocks.size()));
	return true;
}

void Renderer::SetAtlas(Atlas aAtlas)
{
	std::lock_guard lock(mutex_);
	atlas_ = std::move(aAtlas);
	atlasUploaded_ = false;
	dirty_.Clear();  // a new atlas, at its sprites' first frames: its whole upload is due
}

const Atlas* Renderer::AtlasData() const
{
	std::lock_guard lock(mutex_);
	return atlas_ ? &*atlas_ : nullptr;
}

bool Renderer::ReloadShaders(std::string& aError)
{
	const auto bytes = ReadWholeFile(folder_ / "shaders" / "blocks.hlsl");
	if (!bytes)
	{
		aError = "there's no shaders\\blocks.hlsl in the plugin's folder";
		return false;
	}
	auto shaders = CompileBlockShaders(std::string(bytes->begin(), bytes->end()), aError);
	if (!shaders)
	{
		return false;
	}
	std::lock_guard lock(mutex_);
	shaders_ = std::move(*shaders);
	pipeline_.Reset();  // rebuilt with the new shaders at the next draw
	predlssPipeline_.Reset();
	gbufferPipeline_.Reset();
	depthPipeline_.Reset();
	hudPipeline_.Reset();
	entityPipeline_.Reset();
	entityDepthPipeline_.Reset();
	crackPipeline_.Reset();
	entityPipelinesFailed_ = false;
	translucentPipeline_.Reset();
	outlinePipeline_.Reset();
	hintPipeline_.Reset();
	blendedPipeline_.Reset();
	additivePipeline_.Reset();
	blendedBlockPipeline_.Reset();
	forwardPipelinesFailed_ = false;
	patchPipeline_.Reset();
	patchFailed_ = false;
	hudShadersFailed_ = false;
	return true;
}

void Renderer::SetHud(bool aWanted)
{
	if (aWanted && !hudWanted_.load())
	{
		bool haveShaders = false;
		{
			std::lock_guard lock(mutex_);
			haveShaders = shaders_.has_value();
		}
		std::string error;
		if (!haveShaders && (hudShadersFailed_ || !ReloadShaders(error)))
		{
			if (!hudShadersFailed_)
			{
				hudShadersFailed_ = true;
				log_.Push("renderer: the HUD needs the shaders, which didn't compile: " + error);
			}
			return;
		}
	}
	hudWanted_ = aWanted;
}

bool Renderer::SetHudImage(uint32_t aWidth, uint32_t aHeight, bool aBottomUp, const uint8_t* aRgba)
{
	const uint64_t bytes = uint64_t(aWidth) * aHeight * 4;
	uint32_t       slot = 0;
	{
		std::lock_guard               lock(mutex_);
		const std::optional<uint32_t> writable = fence_ && bytes > 0 ? hudSlots_.BeginWrite(fence_->GetCompletedValue()) : std::nullopt;
		if (!writable)
		{
			++stats_.hudDropped;
			return false;
		}
		slot = *writable;
		if (hudUploadBytes_[slot] < bytes)
		{
			// The GPU has finished with this slot, so its buffer can go at once.
			if (hudMapped_[slot])
			{
				hudUploads_[slot]->Unmap(0, nullptr);
				hudMapped_[slot] = nullptr;
			}
			hudUploadBytes_[slot] = 0;
			hudUploads_[slot] = gpu::MakeBuffer(device_, bytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
			void* mapped = nullptr;
			if (!hudUploads_[slot] || FAILED(hudUploads_[slot]->Map(0, nullptr, &mapped)))
			{
				hudUploads_[slot].Reset();
				hudSlots_.Abandon(slot);
				++stats_.hudDropped;
				return false;
			}
			hudUploads_[slot]->SetName(L"BlockRender HUD upload");
			hudMapped_[slot] = static_cast<uint8_t*>(mapped);
			hudUploadBytes_[slot] = bytes;
		}
	}
	// Without the lock: the slot is ours until EndWrite.
	const auto start = std::chrono::steady_clock::now();
	std::memcpy(hudMapped_[slot], aRgba, size_t(bytes));
	const double    ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	std::lock_guard lock(mutex_);
	hudSlotImages_[slot] = { aWidth, aHeight, aBottomUp };
	hudSlots_.EndWrite(slot);
	++stats_.hudReceived;
	stats_.hudBytes += bytes;
	hudCopyMsSum_ += ms;
	++hudCopySamples_;
	return true;
}

bool Renderer::Set(std::string_view aName, double aValue, std::string& aError)
{
	std::lock_guard lock(mutex_);
	if (aName == "brightness" || aName == "tolerance" || aName == "exposure")
	{
		if (aValue < 0.0)
		{
			aError = std::format("{} can't be negative", aName);
			return false;
		}
		(aName == "brightness" ? brightness_ : aName == "tolerance" ? tolerance_ : exposure_) = float(aValue);
		return true;
	}
	if (aName == "jittersign" || aName == "mvsign")
	{
		if (aValue != 1.0 && aValue != -1.0 && aValue != 0.0)
		{
			aError = std::format("{} is 1, -1, or 0 for none", aName);
			return false;
		}
		(aName == "jittersign" ? jitterSign_ : mvSign_) = float(aValue);
		return true;
	}
	if (aName == "cull")
	{
		if (aValue != 0.0 && aValue != 1.0 && aValue != 2.0)
		{
			aError = "cull is 0 (none), 1 (back faces) or 2 (front faces)";
			return false;
		}
		cull_ = int(aValue);
		return true;
	}
	if (aName == "origin.x" || aName == "origin.y" || aName == "origin.z")
	{
		(aName == "origin.x" ? renderOrigin_.x : aName == "origin.y" ? renderOrigin_.y : renderOrigin_.z) = aValue;
		return true;
	}
	if (aName == "stencil")
	{
		if (aValue < 0.0 || aValue > 255.0 || aValue != std::floor(aValue))
		{
			aError = "stencil is a whole number from 0 to 255";
			return false;
		}
		stencil_ = uint8_t(aValue);
		return true;
	}
	if (aName == "metalness" || aName == "roughness" || aName == "materialb")
	{
		if (aValue < 0.0 || aValue > 1.0)
		{
			aError = std::format("{} is from 0 to 1", aName);
			return false;
		}
		(aName == "metalness" ? metalness_ : aName == "roughness" ? roughness_ : materialB_) = float(aValue);
		return true;
	}
	if (aName == "gbmotion" || aName == "gbdepth")
	{
		if (aValue != 0.0 && aValue != 1.0)
		{
			aError = std::format("{} is 1, or 0 to leave the game's", aName);
			return false;
		}
		(aName == "gbmotion" ? gbufferMotion_ : gbufferDepth_) = aValue == 1.0;
		return true;
	}
	if (aName == "entities")
	{
		if (aValue != 0.0 && aValue != 1.0)
		{
			aError = "entities is 1 to draw Minecraft's entities, or 0 to hide them";
			return false;
		}
		entitiesOn_ = aValue == 1.0;
		return true;
	}
	if (aName == "glow")
	{
		if (aValue < 0.0 || aValue > 1.0)
		{
			aError = "glow is from 0 to 1: what a glowing triangle writes into the G-buffer as emission";
			return false;
		}
		glow_ = float(aValue);
		return true;
	}
	if (aName == "glowlight")
	{
		if (aValue < 1.0 || aValue > 15.0 || aValue != std::floor(aValue))
		{
			aError = "glowlight is a whole number from 1 to 15: the block light from which a triangle glows";
			return false;
		}
		glowLight_ = uint32_t(aValue);
		return true;
	}
	if (aName == "dlsshint")
	{
		if (aValue != 0.0 && aValue != 1.0)
		{
			aError = "dlsshint is 1 to mark Minecraft's translucent particles for DLSS, or 0 for nothing";
			return false;
		}
		dlssHint_ = int(aValue);
		return true;
	}
	if (aName == "outline")
	{
		if (aValue != 0.0 && aValue != 1.0)
		{
			aError = "outline is 1 to draw the targeted block's outline in build mode, or 0 to hide it";
			return false;
		}
		outline_ = aValue == 1.0;
		return true;
	}
	if (aName == "blend")
	{
		if (aValue != 0.0 && aValue != 1.0)
		{
			aError = "blend is 1 to blend the entity layers Minecraft blends before DLSS, or 0 to draw them as cut-outs";
			return false;
		}
		blend_ = aValue == 1.0;
		return true;
	}
	if (aName == "animate")
	{
		if (aValue != 0.0 && aValue != 1.0)
		{
			aError = "animate is 1 to play Minecraft's animated textures, or 0 to leave the atlas as it is";
			return false;
		}
		animate_ = aValue == 1.0;
		return true;
	}
	if (aName == "plants")
	{
		if (aValue != 0.0 && aValue != 1.0)
		{
			aError = "plants is 1 to draw Minecraft's plants as the game's own small plants, or 0 to draw them as the blocks";
			return false;
		}
		plantFoliage_ = aValue == 1.0;
		return true;
	}
	if (aName == "plantstencil")
	{
		if (aValue < 0.0 || aValue > 255.0 || aValue != std::floor(aValue))
		{
			aError = "plantstencil is a whole number from 0 to 255: the stencil class plants are drawn with";
			return false;
		}
		plantStencil_ = uint8_t(aValue);
		return true;
	}
	if (aName == "plantb")
	{
		if (aValue < 0.0 || aValue > 1.0)
		{
			aError = "plantb is from 0 to 1: the material B plants are drawn with";
			return false;
		}
		plantB_ = float(aValue);
		return true;
	}
	aError = std::format("there's no setting called \"{}\"", aName);
	return false;
}

void Renderer::UploadSection(const SectionPos& aSection, std::vector<Vertex> aVertices)
{
	std::lock_guard lock(mutex_);
	pending_.Put(aSection, std::move(aVertices));
}

void Renderer::ClearSections()
{
	std::lock_guard lock(mutex_);
	for (auto& [section, gpuSection] : sections_)
	{
		Retire(gpuSection.buffer);
	}
	sections_.clear();
	pending_.Clear();
}

void Renderer::AddEntityTexture(uint32_t aId, uint32_t aWidth, uint32_t aHeight, std::span<const uint8_t> aRgba)
{
	std::lock_guard                            lock(mutex_);
	const std::optional<EntityTextures::Place> place = textures_.Add(aId, aWidth, aHeight);
	if (!place)
	{
		++texturesLeftOut_;
		if (!capLogged_)
		{
			capLogged_ = true;
			log_.Push(std::format("renderer: Minecraft's texture {} ({} x {}) is left out, past the entity textures' 64 MB: what uses it isn't drawn",
			                      aId, aWidth, aHeight));
		}
		return;
	}
	textureReady_.erase(aId);  // placed again: drawn once this upload is done
	pendingTextures_.push_back({ aId, *place, std::vector<uint8_t>(aRgba.begin(), aRgba.end()) });
}

void Renderer::ClearEntityTextures()
{
	std::lock_guard lock(mutex_);
	textures_.Clear();
	pendingTextures_.clear();
	textureReady_.clear();
	capLogged_ = false;
	if (entityTextures_)
	{
		// Lists on their way may still read it, so the next texture starts a fresh buffer, and this one waits for them.
		Retire(entityTextures_);
		entityTextures_.Reset();
		entityTexels_ = 0;
	}
}

void Renderer::SetEntities(entities::EntityFrame aFrame)
{
	std::lock_guard lock(mutex_);
	pendingEntities_ = std::move(aFrame);
}

void Renderer::ApplyAtlasPatch(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight, std::span<const uint8_t> aRgba)
{
	const auto      start = std::chrono::steady_clock::now();
	std::lock_guard lock(mutex_);
	if (!animate_)
	{
		++stats_.patchesDropped;  // set animate 0: the atlas stays as it is
		return;
	}
	const std::vector<AtlasArea> areas = atlas_ ? cpmc::PatchAtlas(*atlas_, aX, aY, aWidth, aHeight, aRgba) : std::vector<AtlasArea>();
	if (areas.empty())
	{
		++stats_.patchesRefused;  // no atlas, or one it doesn't fit
		return;
	}
	++stats_.patchesApplied;
	stats_.patchBytes += aRgba.size();
	if (atlasUploaded_)
	{
		if (dirty_.Waiting(areas.front()))
		{
			++stats_.patchesDropped;  // the sprite's last frame never reached the GPU: this one replaces it
		}
		dirty_.Add(areas);  // for the next patch pass; until then, the whole atlas's upload carries it
	}
	patchCpuMsSum_ += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	++patchCpuSamples_;
}

uint32_t Renderer::GlowLight() const
{
	std::lock_guard lock(mutex_);
	return glowLight_;
}

void Renderer::TimeUpload(const SectionPos& aSection)
{
	std::lock_guard lock(mutex_);
	watch_ = UploadWatch{ aSection };
}

std::optional<Renderer::TimedUpload> Renderer::TakeTimedUpload()
{
	std::lock_guard lock(mutex_);
	if (!watch_ || !watch_->submitted || !watch_->drawn)
	{
		return std::nullopt;
	}
	const TimedUpload timed{ *watch_->submitted, *watch_->drawn, watch_->drawnFrame };
	watch_.reset();
	return timed;
}

bool Renderer::EnsureObjects()
{
	if (fence_)
	{
		return true;
	}
	const d3d::Originals& original = d3d::Original();
	queue_ = d3d::MainQueue();
	device_ = queue_ ? d3d::Device() : nullptr;
	if (queue_ && queue_->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		Fail("DLSS runs on a compute queue, and this build draws only on a direct one");
		return false;
	}
	if (!device_ || !original.reset || !original.close || !original.executeCommandLists || !original.resourceBarrier ||
	    !original.omSetRenderTargets || !original.clearDsv || !original.drawInstanced || !original.createRtv || !original.createDsv)
	{
		return false;  // not ready yet
	}
	D3D12_ROOT_PARAMETER parameters[6]{};
	parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;  // b0: the frame
	parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	parameters[1].Constants = { 1, 0, 8 };  // b1: a section's, the HUD's, or an entity draw's (spike 7)
	for (UINT i = 2; i < 6; ++i)            // t0 vertices, t1 atlas, t2 depth copy, t3 entity textures (spike 7)
	{
		parameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		parameters[i].Descriptor = { i - 2, 0 };
	}
	const D3D12_ROOT_SIGNATURE_DESC rootDesc{ 6, parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE };
	gpu::ComPtr<ID3DBlob>           blob;
	gpu::ComPtr<ID3DBlob>           error;
	if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error)) ||
	    FAILED(device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_))))
	{
		Fail("couldn't create the root signature");
		return false;
	}
	const D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{ D3D12_DESCRIPTOR_HEAP_TYPE_RTV, kRtvs, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
	const D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{ D3D12_DESCRIPTOR_HEAP_TYPE_DSV, kDsvs, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
	const D3D12_QUERY_HEAP_DESC      queryDesc{ D3D12_QUERY_HEAP_TYPE_TIMESTAMP, kSlots * 2, 0 };
	constants_ = gpu::MakeBuffer(device_, kSlots * kConstantsStride, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	timings_ = gpu::MakeBuffer(device_, kSlots * 2 * sizeof(uint64_t), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
	entityTimings_ = gpu::MakeBuffer(device_, kSlots * 2 * sizeof(uint64_t), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
	if (FAILED(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_))) ||
	    FAILED(device_->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap_))) ||
	    FAILED(device_->CreateQueryHeap(&queryDesc, IID_PPV_ARGS(&queries_))) ||
	    FAILED(device_->CreateQueryHeap(&queryDesc, IID_PPV_ARGS(&entityQueries_))) || !constants_ || !timings_ || !entityTimings_ ||
	    FAILED(constants_->Map(0, nullptr, reinterpret_cast<void**>(&constantsMapped_))) ||
	    FAILED(timings_->Map(0, nullptr, reinterpret_cast<void**>(&timingsMapped_))) ||
	    FAILED(entityTimings_->Map(0, nullptr, reinterpret_cast<void**>(&entityTimingsMapped_))) ||
	    FAILED(queue_->GetTimestampFrequency(&timestampFrequency_)))
	{
		Fail("couldn't create its heaps and buffers");
		return false;
	}
	rtvIncrement_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	dsvIncrement_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	for (uint32_t slot = 0; slot < kSlots; ++slot)
	{
		if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocators_[slot]))) ||
		    FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocators_[slot].Get(), nullptr,
		                                      IID_PPV_ARGS(&lists_[slot]))))
		{
			Fail("couldn't create its command lists");
			return false;
		}
		original.close(lists_[slot].Get());
		lists_[slot]->SetName(L"BlockRender blocks");
	}
	gpu::ComPtr<ID3D12Fence> fence;
	if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAllocator_))) ||
	    FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, uploadAllocator_.Get(), nullptr, IID_PPV_ARGS(&uploadList_))) ||
	    FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
	{
		Fail("couldn't create its upload list and fence");
		return false;
	}
	original.close(uploadList_.Get());
	uploadList_->SetName(L"BlockRender uploads");
	rootSignature_->SetName(L"BlockRender root signature");
	constants_->SetName(L"BlockRender constants");
	timings_->SetName(L"BlockRender timings");
	entityTimings_->SetName(L"BlockRender entity timings");
	fence->SetName(L"BlockRender fence");
	fence_ = fence;  // last: a fence means everything is ready
	log_.Push("renderer: ready");
	return true;
}

bool Renderer::EnsurePipeline(DXGI_FORMAT aFormat, bool aReversed)
{
	if (pipeline_ && pipelineFormat_ == aFormat && pipelineCull_ == cull_ && pipelineReversed_ == aReversed)
	{
		return true;
	}
	if (!shaders_)
	{
		Fail("there are no shaders");
		return false;
	}
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.VS = { shaders_->vertex.data(), shaders_->vertex.size() };
	desc.PS = { shaders_->present.data(), shaders_->present.size() };
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = BlockRasterizer(cull_);
	desc.DepthStencilState.DepthEnable = TRUE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthStencilState.DepthFunc = aReversed ? D3D12_COMPARISON_FUNC_GREATER : D3D12_COMPARISON_FUNC_LESS;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = aFormat;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	desc.SampleDesc.Count = 1;
	gpu::ComPtr<ID3D12PipelineState> pipeline;
	if (FAILED(device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline))))
	{
		Fail(std::format("couldn't create a pipeline for {}", DxgiFormatName(aFormat)));
		return false;
	}
	pipeline->SetName(L"BlockRender present pipeline");
	pipeline_ = pipeline;
	pipelineFormat_ = aFormat;
	pipelineCull_ = cull_;
	pipelineReversed_ = aReversed;
	return true;
}

bool Renderer::EnsurePredlssPipeline(bool aReversed)
{
	if (predlssPipeline_ && predlssCull_ == cull_ && predlssReversed_ == aReversed)
	{
		return true;
	}
	if (!shaders_)
	{
		Fail("there are no shaders");
		return false;
	}
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.VS = { shaders_->vertex.data(), shaders_->vertex.size() };
	desc.PS = { shaders_->predlss.data(), shaders_->predlss.size() };
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;  // no independent blend: both
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = BlockRasterizer(cull_);
	desc.DepthStencilState.DepthEnable = TRUE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthStencilState.DepthFunc = aReversed ? D3D12_COMPARISON_FUNC_GREATER : D3D12_COMPARISON_FUNC_LESS;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 2;  // the scaling input colour, and the motion vectors
	desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.RTVFormats[1] = DXGI_FORMAT_R16G16_FLOAT;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	desc.SampleDesc.Count = 1;
	gpu::ComPtr<ID3D12PipelineState> pipeline;
	if (FAILED(device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline))))
	{
		Fail("couldn't create the pipeline for drawing before DLSS");
		return false;
	}
	pipeline->SetName(L"BlockRender predlss pipeline");
	predlssPipeline_ = pipeline;
	predlssCull_ = cull_;
	predlssReversed_ = aReversed;
	return true;
}

bool Renderer::EnsureGbufferPipeline(bool aReversed)
{
	if (gbufferPipeline_ && gbufferCull_ == cull_ && gbufferReversed_ == aReversed && gbufferMotionWrites_ == gbufferMotion_)
	{
		return true;
	}
	if (!shaders_)
	{
		Fail("there are no shaders");
		return false;
	}
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.VS = { shaders_->vertex.data(), shaders_->vertex.size() };
	desc.PS = { shaders_->gbuffer.data(), shaders_->gbuffer.size() };
	desc.BlendState.IndependentBlendEnable = TRUE;  // so the motion vectors alone can be left alone
	for (UINT i = 0; i < 4; ++i)
	{
		desc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		desc.RTVFormats[i] = kGbufferFormats[i];
	}
	if (!gbufferMotion_)
	{
		desc.BlendState.RenderTarget[3].RenderTargetWriteMask = 0;
	}
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = BlockRasterizer(cull_);
	desc.DepthStencilState.DepthEnable = TRUE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthStencilState.DepthFunc = aReversed ? D3D12_COMPARISON_FUNC_GREATER : D3D12_COMPARISON_FUNC_LESS;
	// Every pixel drawn takes the stencil class: OMSetStencilRef sets it.
	desc.DepthStencilState.StencilEnable = TRUE;
	desc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	desc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	const D3D12_DEPTH_STENCILOP_DESC replace{ D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_REPLACE,
		                                      D3D12_COMPARISON_FUNC_ALWAYS };
	desc.DepthStencilState.FrontFace = replace;
	desc.DepthStencilState.BackFace = replace;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 4;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	desc.SampleDesc.Count = 1;
	gpu::ComPtr<ID3D12PipelineState> pipeline;
	if (FAILED(device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline))))
	{
		Fail("couldn't create the pipeline for drawing into the G-buffer");
		return false;
	}
	pipeline->SetName(L"BlockRender gbuffer pipeline");
	gbufferPipeline_ = pipeline;
	gbufferCull_ = cull_;
	gbufferReversed_ = aReversed;
	gbufferMotionWrites_ = gbufferMotion_;
	return true;
}

bool Renderer::EnsureDepthPipeline(bool aReversed)
{
	if (depthPipeline_ && depthCull_ == cull_ && depthReversed_ == aReversed)
	{
		return true;
	}
	if (!shaders_)
	{
		Fail("there are no shaders");
		return false;
	}
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.VS = { shaders_->vertex.data(), shaders_->vertex.size() };
	desc.PS = { shaders_->depth.data(), shaders_->depth.size() };  // only to cut out what the atlas cuts out
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = BlockRasterizer(cull_);
	desc.DepthStencilState.DepthEnable = TRUE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthStencilState.DepthFunc = aReversed ? D3D12_COMPARISON_FUNC_GREATER : D3D12_COMPARISON_FUNC_LESS;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 0;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	desc.SampleDesc.Count = 1;
	gpu::ComPtr<ID3D12PipelineState> pipeline;
	if (FAILED(device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline))))
	{
		Fail("couldn't create the pipeline for the tagged depth");
		return false;
	}
	pipeline->SetName(L"BlockRender depth pipeline");
	depthPipeline_ = pipeline;
	depthCull_ = cull_;
	depthReversed_ = aReversed;
	return true;
}

bool Renderer::EnsureEntityPipelines(bool aReversed)
{
	if (entityPipeline_ && entityReversed_ == aReversed && entityMotionWrites_ == gbufferMotion_)
	{
		return true;
	}
	if (entityPipelinesFailed_ || !shaders_)
	{
		return false;
	}
	// Minecraft draws its entities two-sided (entity_cutout culls nothing) and with a greater-or-equal depth test, so a
	// later triangle on the same surface wins: a banner's patterns over its cloth, a villager's clothes over its body.
	const D3D12_COMPARISON_FUNC depthFunc = aReversed ? D3D12_COMPARISON_FUNC_GREATER_EQUAL : D3D12_COMPARISON_FUNC_LESS_EQUAL;
	// The models: into the G-buffer as the blocks are, their stencil class too.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC models{};
	models.pRootSignature = rootSignature_.Get();
	models.VS = { shaders_->entityVertex.data(), shaders_->entityVertex.size() };
	models.PS = { shaders_->entity.data(), shaders_->entity.size() };
	models.BlendState.IndependentBlendEnable = TRUE;  // so the motion vectors alone can be left alone
	for (UINT i = 0; i < 4; ++i)
	{
		models.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		models.RTVFormats[i] = kGbufferFormats[i];
	}
	if (!gbufferMotion_)
	{
		models.BlendState.RenderTarget[3].RenderTargetWriteMask = 0;
	}
	models.SampleMask = UINT_MAX;
	models.RasterizerState = BlockRasterizer(0);
	models.DepthStencilState.DepthEnable = TRUE;
	models.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	models.DepthStencilState.DepthFunc = depthFunc;
	models.DepthStencilState.StencilEnable = TRUE;
	models.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	models.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	const D3D12_DEPTH_STENCILOP_DESC replace{ D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_REPLACE,
		                                      D3D12_COMPARISON_FUNC_ALWAYS };
	models.DepthStencilState.FrontFace = replace;
	models.DepthStencilState.BackFace = replace;
	models.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	models.NumRenderTargets = 4;
	models.DSVFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	models.SampleDesc.Count = 1;
	// Their depth, into the tagged depth: the same tests, depth only.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC depth{};
	depth.pRootSignature = rootSignature_.Get();
	depth.VS = models.VS;
	depth.PS = { shaders_->entityDepth.data(), shaders_->entityDepth.size() };
	depth.SampleMask = UINT_MAX;
	depth.RasterizerState = BlockRasterizer(0);
	depth.DepthStencilState.DepthEnable = TRUE;
	depth.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depth.DepthStencilState.DepthFunc = depthFunc;
	depth.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	depth.NumRenderTargets = 0;
	depth.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	depth.SampleDesc.Count = 1;
	// The cracks: the base colour's RGB alone, blended DEST_COLOR, SRC_COLOR as Minecraft's crumbling, twice the crack's
	// colour times what's there; depth tested, not written; the stencil left alone.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC cracks{};
	cracks.pRootSignature = rootSignature_.Get();
	cracks.VS = models.VS;
	cracks.PS = { shaders_->crack.data(), shaders_->crack.size() };
	D3D12_RENDER_TARGET_BLEND_DESC& blend = cracks.BlendState.RenderTarget[0];
	blend.BlendEnable = TRUE;
	blend.SrcBlend = D3D12_BLEND_DEST_COLOR;
	blend.DestBlend = D3D12_BLEND_SRC_COLOR;
	blend.BlendOp = D3D12_BLEND_OP_ADD;
	blend.SrcBlendAlpha = D3D12_BLEND_ONE;
	blend.DestBlendAlpha = D3D12_BLEND_ZERO;
	blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blend.LogicOp = D3D12_LOGIC_OP_NOOP;
	blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
	cracks.SampleMask = UINT_MAX;
	cracks.RasterizerState = BlockRasterizer(0);
	cracks.DepthStencilState.DepthEnable = TRUE;
	cracks.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	cracks.DepthStencilState.DepthFunc = depthFunc;
	cracks.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	cracks.NumRenderTargets = 1;
	cracks.RTVFormats[0] = kGbufferFormats[0];
	cracks.DSVFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	cracks.SampleDesc.Count = 1;
	gpu::ComPtr<ID3D12PipelineState> modelPipeline;
	gpu::ComPtr<ID3D12PipelineState> depthPipeline;
	gpu::ComPtr<ID3D12PipelineState> crackPipeline;
	if (FAILED(device_->CreateGraphicsPipelineState(&models, IID_PPV_ARGS(&modelPipeline))) ||
	    FAILED(device_->CreateGraphicsPipelineState(&depth, IID_PPV_ARGS(&depthPipeline))) ||
	    FAILED(device_->CreateGraphicsPipelineState(&cracks, IID_PPV_ARGS(&crackPipeline))))
	{
		entityPipelinesFailed_ = true;
		log_.Push("renderer: couldn't create the pipelines for Minecraft's entities, so they aren't drawn; the blocks still are");
		return false;
	}
	modelPipeline->SetName(L"BlockRender entities pipeline");
	depthPipeline->SetName(L"BlockRender entities' depth pipeline");
	crackPipeline->SetName(L"BlockRender cracks pipeline");
	entityPipeline_ = modelPipeline;
	entityDepthPipeline_ = depthPipeline;
	crackPipeline_ = crackPipeline;
	entityReversed_ = aReversed;
	entityMotionWrites_ = gbufferMotion_;
	return true;
}

bool Renderer::EnsureForwardObjects()
{
	if (forwardTimingsMapped_)
	{
		return true;
	}
	const D3D12_QUERY_HEAP_DESC queryDesc{ D3D12_QUERY_HEAP_TYPE_TIMESTAMP, kSlots * 2, 0 };
	if (!forwardConstants_)
	{
		forwardConstants_ = gpu::MakeBuffer(device_, kSlots * kConstantsStride, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	}
	if (!forwardTimings_)
	{
		forwardTimings_ = gpu::MakeBuffer(device_, kSlots * 2 * sizeof(uint64_t), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
	}
	if (!forwardConstants_ || !forwardTimings_ ||
	    (!forwardQueries_ && FAILED(device_->CreateQueryHeap(&queryDesc, IID_PPV_ARGS(&forwardQueries_)))))
	{
		return false;
	}
	for (uint32_t slot = 0; slot < kSlots; ++slot)
	{
		if (forwardLists_[slot])
		{
			continue;
		}
		if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&forwardAllocators_[slot]))) ||
		    FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, forwardAllocators_[slot].Get(), nullptr,
		                                      IID_PPV_ARGS(&forwardLists_[slot]))))
		{
			forwardLists_[slot].Reset();
			return false;
		}
		d3d::Original().close(forwardLists_[slot].Get());
		forwardLists_[slot]->SetName(L"BlockRender forward");
	}
	if (!forwardConstantsMapped_)
	{
		void* mapped = nullptr;
		if (FAILED(forwardConstants_->Map(0, nullptr, &mapped)))
		{
			return false;
		}
		forwardConstants_->SetName(L"BlockRender forward constants");
		forwardConstantsMapped_ = static_cast<uint8_t*>(mapped);
	}
	void* mapped = nullptr;
	if (FAILED(forwardTimings_->Map(0, nullptr, &mapped)))
	{
		return false;
	}
	forwardTimings_->SetName(L"BlockRender forward timings");
	forwardTimingsMapped_ = static_cast<uint64_t*>(mapped);  // last: mapped timings mean everything is ready
	return true;
}

bool Renderer::EnsureForwardPipelines(bool aReversed)
{
	if (translucentPipeline_ && forwardReversed_ == aReversed && forwardCull_ == cull_)
	{
		return true;
	}
	if (forwardPipelinesFailed_ || !shaders_)
	{
		return false;
	}
	// The translucent particles: blended over the scene's colour as Minecraft blends them, its alpha left alone; tested
	// against the tagged depth as the entities are, greater-or-equal, and never written; two-sided.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC particles{};
	particles.pRootSignature = rootSignature_.Get();
	particles.VS = { shaders_->entityVertex.data(), shaders_->entityVertex.size() };
	particles.PS = { shaders_->translucent.data(), shaders_->translucent.size() };
	D3D12_RENDER_TARGET_BLEND_DESC& blend = particles.BlendState.RenderTarget[0];
	blend.BlendEnable = TRUE;
	blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blend.BlendOp = D3D12_BLEND_OP_ADD;
	blend.SrcBlendAlpha = D3D12_BLEND_ONE;
	blend.DestBlendAlpha = D3D12_BLEND_ZERO;
	blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blend.LogicOp = D3D12_LOGIC_OP_NOOP;
	blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
	particles.SampleMask = UINT_MAX;
	particles.RasterizerState = BlockRasterizer(0);
	particles.DepthStencilState.DepthEnable = TRUE;
	particles.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	particles.DepthStencilState.DepthFunc = aReversed ? D3D12_COMPARISON_FUNC_GREATER_EQUAL : D3D12_COMPARISON_FUNC_LESS_EQUAL;
	particles.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	particles.NumRenderTargets = 1;
	particles.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	particles.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	particles.SampleDesc.Count = 1;
	// The outline: the same, its strips made from the edges by OutlineVS.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC outline = particles;
	outline.VS = { shaders_->outlineVertex.data(), shaders_->outlineVertex.size() };
	outline.PS = { shaders_->outline.data(), shaders_->outline.size() };
	// DLSS's bias hint: 1 under the particles, unblended, into the R8 hint.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC hint = particles;
	hint.PS = { shaders_->hint.data(), shaders_->hint.size() };
	hint.BlendState.RenderTarget[0] = {};
	hint.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED;
	hint.RTVFormats[0] = DXGI_FORMAT_R8_UNORM;
	// Part 5: the entity layers Minecraft blends, blended as the particles with their own shader; and those it adds, their
	// colour added, ONE to ONE, as Minecraft's ADDITIVE.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC layers = particles;
	layers.PS = { shaders_->blended.data(), shaders_->blended.size() };
	D3D12_GRAPHICS_PIPELINE_STATE_DESC added = particles;
	added.PS = { shaders_->additive.data(), shaders_->additive.size() };
	added.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	added.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	// The polish pass's translucent blocks: blended as the particles, but with the blocks' vertex shader, culling and
	// depth test, and their depth written, as Minecraft's translucent_terrain writes it.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC blocks = particles;
	blocks.VS = { shaders_->vertex.data(), shaders_->vertex.size() };
	blocks.PS = { shaders_->blendedBlock.data(), shaders_->blendedBlock.size() };
	blocks.RasterizerState = BlockRasterizer(cull_);
	blocks.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	blocks.DepthStencilState.DepthFunc = aReversed ? D3D12_COMPARISON_FUNC_GREATER : D3D12_COMPARISON_FUNC_LESS;
	gpu::ComPtr<ID3D12PipelineState> particlePipeline;
	gpu::ComPtr<ID3D12PipelineState> outlinePipeline;
	gpu::ComPtr<ID3D12PipelineState> hintPipeline;
	gpu::ComPtr<ID3D12PipelineState> blendedPipeline;
	gpu::ComPtr<ID3D12PipelineState> additivePipeline;
	gpu::ComPtr<ID3D12PipelineState> blendedBlockPipeline;
	if (FAILED(device_->CreateGraphicsPipelineState(&particles, IID_PPV_ARGS(&particlePipeline))) ||
	    FAILED(device_->CreateGraphicsPipelineState(&outline, IID_PPV_ARGS(&outlinePipeline))) ||
	    FAILED(device_->CreateGraphicsPipelineState(&hint, IID_PPV_ARGS(&hintPipeline))) ||
	    FAILED(device_->CreateGraphicsPipelineState(&layers, IID_PPV_ARGS(&blendedPipeline))) ||
	    FAILED(device_->CreateGraphicsPipelineState(&added, IID_PPV_ARGS(&additivePipeline))) ||
	    FAILED(device_->CreateGraphicsPipelineState(&blocks, IID_PPV_ARGS(&blendedBlockPipeline))))
	{
		forwardPipelinesFailed_ = true;
		log_.Push("renderer: couldn't create the forward list's pipelines, so Minecraft's translucent blocks and particles, its blended "
		          "layers, the outline and the plants' depth aren't drawn; everything else still is");
		return false;
	}
	particlePipeline->SetName(L"BlockRender translucent particles pipeline");
	outlinePipeline->SetName(L"BlockRender outline pipeline");
	hintPipeline->SetName(L"BlockRender bias hint pipeline");
	blendedPipeline->SetName(L"BlockRender blended layers pipeline");
	additivePipeline->SetName(L"BlockRender additive layers pipeline");
	blendedBlockPipeline->SetName(L"BlockRender translucent blocks pipeline");
	translucentPipeline_ = particlePipeline;
	outlinePipeline_ = outlinePipeline;
	hintPipeline_ = hintPipeline;
	blendedPipeline_ = blendedPipeline;
	additivePipeline_ = additivePipeline;
	blendedBlockPipeline_ = blendedBlockPipeline;
	forwardReversed_ = aReversed;
	forwardCull_ = cull_;
	return true;
}

bool Renderer::EnsureDepthTarget(uint32_t aWidth, uint32_t aHeight)
{
	if (depthTarget_ && depthWidth_ == aWidth && depthHeight_ == aHeight)
	{
		return true;
	}
	if (depthTarget_)
	{
		Retire(depthTarget_);
		depthTarget_.Reset();
	}
	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = aWidth;
	desc.Height = aHeight;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_D32_FLOAT;
	desc.SampleDesc.Count = 1;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE clear{};
	clear.Format = DXGI_FORMAT_D32_FLOAT;
	if (FAILED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear,
	                                            IID_PPV_ARGS(&depthTarget_))))
	{
		Fail("couldn't create its depth buffer");
		return false;
	}
	depthTarget_->SetName(L"BlockRender depth");
	d3d::Original().createDsv(device_, depthTarget_.Get(), nullptr, Offset(dsvHeap_->GetCPUDescriptorHandleForHeapStart(), kOwnDepthDsv, dsvIncrement_));
	depthWidth_ = aWidth;
	depthHeight_ = aHeight;
	return true;
}

void Renderer::Skip(const std::string& aWhy, bool aCounts)
{
	++stats_.skipped;
	stats_.lastSkip = aWhy;
	if (aCounts && ++consecutiveSkips_ == kSkipsBeforeOff)
	{
		Fail(std::format("{} frames in a row were skipped ({})", kSkipsBeforeOff, aWhy));
	}
}

void Renderer::ForwardSkip(const std::string& aWhy)
{
	++stats_.forwardSkipped;
	stats_.forwardLastSkip = aWhy;
}

void Renderer::DropForward()
{
	if (forward_ && forward_->snapshot)
	{
		snapshotRing_.Abandon(*forward_->snapshot);
	}
	forward_.reset();
	forwardList_ = nullptr;
}

void Renderer::Fail(const std::string& aWhy)
{
	log_.Push("renderer: " + aWhy + ", so drawing is off");
	stats_.lastSkip = aWhy;
	mode_ = Mode::Off;
	ReleaseTracking();
}

void Renderer::ReleaseTracking()
{
	if (trackingRequested_)
	{
		d3d::ReleaseTracking();
		trackingRequested_ = false;
	}
}

uint64_t Renderer::Signal()
{
	queue_->Signal(fence_.Get(), ++fenceValue_);
	return fenceValue_;
}

void Renderer::Retire(gpu::ComPtr<ID3D12Resource> aResource)
{
	// Every list of ours that used it has been submitted, and the fence was signalled after each, unless one is on its way
	// to the GPU from a render thread: then it waits for the fence after that one.
	if (retired_.Retire(std::move(aResource), fenceValue_))
	{
		log_.Push(std::format("renderer: an old buffer waits for a list of ours on its way to the GPU ({} since the start)",
		                      retired_.Waited()));
	}
}

void Renderer::ReadTimings(uint32_t aSlot)
{
	const uint64_t begin = timingsMapped_[aSlot * 2];
	const uint64_t end = timingsMapped_[aSlot * 2 + 1];
	if (end > begin && timestampFrequency_ > 0)
	{
		const double ms = double(end - begin) * 1000.0 / double(timestampFrequency_);
		gpuMsSum_ += ms;
		++gpuSamples_;
		stats_.gpuMsWorst = std::max(stats_.gpuMsWorst, ms);
	}
	if (std::exchange(slotEntityTimed_[aSlot], false))
	{
		const uint64_t entityBegin = entityTimingsMapped_[aSlot * 2];
		const uint64_t entityEnd = entityTimingsMapped_[aSlot * 2 + 1];
		if (entityEnd > entityBegin && timestampFrequency_ > 0)
		{
			entityGpuMsSum_ += double(entityEnd - entityBegin) * 1000.0 / double(timestampFrequency_);
			++entityGpuSamples_;
		}
	}
}

void Renderer::ReadForwardTimings(uint32_t aSlot)
{
	const uint64_t begin = forwardTimingsMapped_[aSlot * 2];
	const uint64_t end = forwardTimingsMapped_[aSlot * 2 + 1];
	if (end > begin && timestampFrequency_ > 0)
	{
		forwardGpuMsSum_ += double(end - begin) * 1000.0 / double(timestampFrequency_);
		++forwardGpuSamples_;
	}
}

void Renderer::Update()
{
	const auto      start = std::chrono::steady_clock::now();
	std::lock_guard lock(mutex_);
	Upload();
	CopyEntities();
	const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	updateMsSum_ += ms;
	++updateSamples_;
	stats_.updateMsWorst = std::max(stats_.updateMsWorst, ms);
}

void Renderer::Upload()
{
	if ((!fence_ && mode_.load() == Mode::Off) || !EnsureObjects())
	{
		return;  // nothing is created until drawing is first switched on
	}
	const uint64_t completed = fence_->GetCompletedValue();
	if (completed == UINT64_MAX)
	{
		return;  // the device was removed: the plugin reports it
	}
	retired_.Free(completed);
	if (patchTimed_ && completed >= uploadFence_)
	{
		// Part 5: the last upload list's patch pass, timed around its dispatch.
		patchTimed_ = false;
		if (patchTimingsMapped_[1] > patchTimingsMapped_[0] && timestampFrequency_ > 0)
		{
			patchGpuMsSum_ += double(patchTimingsMapped_[1] - patchTimingsMapped_[0]) * 1000.0 / double(timestampFrequency_);
			++patchGpuSamples_;
		}
	}
	const bool atlasDue = atlas_ && !atlasUploaded_;
	if ((pending_.Size() == 0 && !atlasDue && pendingTextures_.empty() && dirty_.Empty()) || completed < uploadFence_)
	{
		return;
	}
	const d3d::Originals& original = d3d::Original();
	if (FAILED(uploadAllocator_->Reset()) || FAILED(original.reset(uploadList_.Get(), uploadAllocator_.Get(), nullptr)))
	{
		Fail("couldn't reset its upload list");
		return;
	}
	const uint64_t                           ready = fenceValue_ + 1;  // the value the Signal below sets
	std::vector<gpu::ComPtr<ID3D12Resource>> staging;
	bool                                     failed = false;
	const auto upload = [&](const void* aData, uint64_t aBytes, D3D12_RESOURCE_FLAGS aFlags = D3D12_RESOURCE_FLAG_NONE) -> gpu::ComPtr<ID3D12Resource> {
		auto  target = gpu::MakeBuffer(device_, aBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, aFlags);
		auto  source = gpu::MakeBuffer(device_, aBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
		void* mapped = nullptr;
		if (!target || !source || FAILED(source->Map(0, nullptr, &mapped)))
		{
			failed = true;
			return nullptr;
		}
		std::memcpy(mapped, aData, aBytes);
		source->Unmap(0, nullptr);
		uploadList_->CopyBufferRegion(target.Get(), 0, source.Get(), 0, aBytes);
		staging.push_back(source);
		return target;
	};
	if (atlasDue)
	{
		if (atlasBuffer_)
		{
			Retire(atlasBuffer_);
		}
		// With unordered access, for Part 5's patch pass; the CPU's atlas holds every patch so far.
		atlasBuffer_ = upload(atlas_->texels.data(), atlas_->texels.size() * sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		if (atlasBuffer_)
		{
			atlasBuffer_->SetName(L"BlockRender atlas");
		}
		atlasReady_ = ready;
		atlasUploaded_ = true;
		dirty_.Clear();
	}
	bool watched = false;
	for (auto& [section, vertices] : pending_.Take())
	{
		if (const auto found = sections_.find(section); found != sections_.end())
		{
			Retire(found->second.buffer);
			sections_.erase(found);
		}
		if (watch_ && !watch_->submitted && watch_->section == section)
		{
			watch_->ready = ready;
			watched = true;
		}
		SectionGpu gpuSection;
		gpuSection.parts = SplitSection(vertices, atlas_ ? &*atlas_ : nullptr);  // the polish pass's parts
		if (vertices.empty())
		{
			continue;
		}
		gpuSection.buffer = upload(vertices.data(), vertices.size() * sizeof(Vertex));
		gpuSection.vertices = uint32_t(vertices.size());
		gpuSection.ready = ready;
		if (gpuSection.buffer)
		{
			gpuSection.buffer->SetName(L"BlockRender section");
			sections_[section] = std::move(gpuSection);
		}
	}
	UploadEntityTextures(ready, staging, failed);
	RecordPatches();
	original.close(uploadList_.Get());
	ID3D12CommandList* lists[] = { uploadList_.Get() };
	original.executeCommandLists(queue_, 1, lists);
	uploadFence_ = Signal();
	if (watched)
	{
		watch_->submitted = SteadySeconds();
	}
	for (auto& buffer : staging)
	{
		retired_.RetireAfter(std::move(buffer), uploadFence_);  // only the upload list reads them
	}
	if (failed)
	{
		log_.Push("renderer: couldn't create some upload buffers; the sections or entity textures they held aren't drawn");
	}
}

void Renderer::UploadEntityTextures(uint64_t aReady, std::vector<gpu::ComPtr<ID3D12Resource>>& aStaging, bool& aFailed)
{
	if (pendingTextures_.empty())
	{
		return;
	}
	// The one buffer, grown first if the textures don't fit: the old texels copied over on the GPU, and every texture in
	// it drawn again once the copy is done.
	const uint64_t capacity = textures_.Capacity(entityTexels_);
	if (capacity > entityTexels_)
	{
		gpu::ComPtr<ID3D12Resource> grown = gpu::MakeBuffer(device_, capacity * 4, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
		if (!grown)
		{
			aFailed = true;
			return;  // they wait for the next upload
		}
		grown->SetName(L"BlockRender entity textures");
		if (entityTextures_)
		{
			// Only the texels uploaded before, which the waiting textures' places follow: copies in one list aren't
			// ordered, so a copy over their places could land after theirs.
			const uint64_t uploaded = std::min<uint64_t>(pendingTextures_.front().place.start, entityTexels_);
			if (uploaded > 0)
			{
				uploadList_->CopyBufferRegion(grown.Get(), 0, entityTextures_.Get(), 0, uploaded * 4);
			}
			// This upload reads the old buffer, so it's kept until the upload is done, not only the lists before it, or
			// until the list of ours on its way to the GPU is, which comes later.
			retired_.Retire(std::move(entityTextures_), aReady);
			for (auto& [id, ready] : textureReady_)
			{
				ready = aReady;
			}
		}
		entityTextures_ = grown;
		entityTexels_ = capacity;
	}
	for (const PendingTexture& texture : pendingTextures_)
	{
		const uint64_t bytes = texture.rgba.size();
		auto           source = gpu::MakeBuffer(device_, bytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
		void*          mapped = nullptr;
		if (!source || FAILED(source->Map(0, nullptr, &mapped)))
		{
			aFailed = true;
			continue;  // never ready, so what uses it is skipped
		}
		std::memcpy(mapped, texture.rgba.data(), bytes);
		source->Unmap(0, nullptr);
		uploadList_->CopyBufferRegion(entityTextures_.Get(), uint64_t(texture.place.start) * 4, source.Get(), 0, bytes);
		aStaging.push_back(source);
		textureReady_[texture.id] = aReady;
	}
	pendingTextures_.clear();
}

void Renderer::RecordPatches()
{
	if (dirty_.Empty())
	{
		return;
	}
	if (!atlas_ || !atlasBuffer_ || !EnsurePatchObjects())
	{
		// No atlas on the GPU, its upload failed, or no patch pass: the GPU's atlas keeps the frames it has, and the
		// CPU's stays current for its next whole upload.
		dirty_.Clear();
		return;
	}
	const auto               start = std::chrono::steady_clock::now();
	const AtlasDirty::Packed packed = dirty_.Take(*atlas_, D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION);
	if (packed.areas.empty())
	{
		return;
	}
	// One buffer: the area table, then the texels from the next 256 bytes on. The last upload list that read it is done
	// (Upload records a list only then), so it's written in place, or replaced at once when it's too small.
	const uint64_t tableBytes = packed.areas.size() * sizeof(PatchAreaGpu);
	const uint64_t texelsAt = (tableBytes + 255) / 256 * 256;
	const uint64_t bytes = texelsAt + packed.texels.size() * sizeof(uint32_t);
	if (bytes > patchUploadBytes_)
	{
		if (patchUploadMapped_)
		{
			patchUpload_->Unmap(0, nullptr);
		}
		patchUpload_.Reset();
		patchUploadMapped_ = nullptr;
		patchUploadBytes_ = 0;
		const uint64_t size = std::max<uint64_t>(std::bit_ceil(bytes), 64ull << 10);
		auto           buffer = gpu::MakeBuffer(device_, size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
		void*          mapped = nullptr;
		if (!buffer || FAILED(buffer->Map(0, nullptr, &mapped)))
		{
			patchFailed_ = true;
			patchPipeline_.Reset();
			log_.Push("renderer: couldn't create the atlas patches' upload buffer, so Minecraft's animated textures stay as they are on the GPU");
			return;
		}
		buffer->SetName(L"BlockRender atlas patches");
		patchUpload_ = buffer;
		patchUploadMapped_ = static_cast<uint8_t*>(mapped);
		patchUploadBytes_ = size;
	}
	std::memcpy(patchUploadMapped_, packed.areas.data(), size_t(tableBytes));
	std::memcpy(patchUploadMapped_ + texelsAt, packed.texels.data(), packed.texels.size() * sizeof(uint32_t));
	PatchConstants constants;
	constants.areas = uint32_t(packed.areas.size());
	constants.texels = uint32_t(packed.texels.size());
	constants.atlasTexels = uint32_t(atlas_->texels.size());
	// The atlas, in the common state between lists, takes the writes, then goes back for the draws to read.
	const d3d::Originals&        original = d3d::Original();
	const D3D12_RESOURCE_BARRIER into = Transition(atlasBuffer_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	const D3D12_RESOURCE_BARRIER back = Transition(atlasBuffer_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
	original.resourceBarrier(uploadList_.Get(), 1, &into);
	uploadList_->SetComputeRootSignature(patchRootSignature_.Get());
	uploadList_->SetPipelineState(patchPipeline_.Get());
	uploadList_->SetComputeRoot32BitConstants(0, 4, &constants, 0);
	uploadList_->SetComputeRootShaderResourceView(1, patchUpload_->GetGPUVirtualAddress());
	uploadList_->SetComputeRootShaderResourceView(2, patchUpload_->GetGPUVirtualAddress() + texelsAt);
	uploadList_->SetComputeRootUnorderedAccessView(3, atlasBuffer_->GetGPUVirtualAddress());
	uploadList_->EndQuery(patchQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
	original.dispatch(uploadList_.Get(), constants.areas, 1, 1);  // a group an area; past the hook, as every call of ours
	uploadList_->EndQuery(patchQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
	original.resourceBarrier(uploadList_.Get(), 1, &back);
	uploadList_->ResolveQueryData(patchQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, patchTimings_.Get(), 0);
	patchTimed_ = true;
	++stats_.patchPasses;
	patchCpuMsSum_ += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

bool Renderer::EnsurePatchObjects()
{
	if (patchPipeline_)
	{
		return true;
	}
	if (patchFailed_ || !shaders_ || !d3d::Original().dispatch)
	{
		return false;
	}
	const auto fail = [this](const char* aWhat) {
		patchFailed_ = true;
		log_.Push(std::format("renderer: couldn't create the atlas patch pass's {}, so Minecraft's animated textures stay as they are on the "
		                      "GPU; everything else is drawn",
		                      aWhat));
		return false;
	};
	if (!patchRootSignature_)
	{
		// b0 the pass's constants, t0 the areas, t1 their texels, u0 the atlas: root descriptors, as the draws'.
		D3D12_ROOT_PARAMETER parameters[4]{};
		parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		parameters[0].Constants = { 0, 0, 4 };
		parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		parameters[1].Descriptor = { 0, 0 };
		parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		parameters[2].Descriptor = { 1, 0 };
		parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		parameters[3].Descriptor = { 0, 0 };
		const D3D12_ROOT_SIGNATURE_DESC rootDesc{ 4, parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE };
		gpu::ComPtr<ID3DBlob>           blob;
		gpu::ComPtr<ID3DBlob>           error;
		if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error)) ||
		    FAILED(device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&patchRootSignature_))))
		{
			return fail("root signature");
		}
		patchRootSignature_->SetName(L"BlockRender patch root signature");
	}
	if (!patchQueries_)
	{
		const D3D12_QUERY_HEAP_DESC queryDesc{ D3D12_QUERY_HEAP_TYPE_TIMESTAMP, 2, 0 };
		auto                        timings = gpu::MakeBuffer(device_, 2 * sizeof(uint64_t), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
		void*                       mapped = nullptr;
		if (!timings || FAILED(timings->Map(0, nullptr, &mapped)) || FAILED(device_->CreateQueryHeap(&queryDesc, IID_PPV_ARGS(&patchQueries_))))
		{
			patchQueries_.Reset();
			return fail("timing queries");
		}
		timings->SetName(L"BlockRender patch timings");
		patchTimings_ = timings;
		patchTimingsMapped_ = static_cast<uint64_t*>(mapped);
	}
	D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = patchRootSignature_.Get();
	desc.CS = { shaders_->patch.data(), shaders_->patch.size() };
	gpu::ComPtr<ID3D12PipelineState> pipeline;
	if (FAILED(device_->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pipeline))))
	{
		return fail("pipeline");
	}
	pipeline->SetName(L"BlockRender patch pipeline");
	patchPipeline_ = pipeline;
	return true;
}

void Renderer::CopyEntities()
{
	if (!pendingEntities_)
	{
		return;
	}
	if (!fence_ || mode_.load() != Mode::Gbuffer || !entitiesOn_)
	{
		pendingEntities_.reset();  // drawn only into the G-buffer
		return;
	}
	const std::optional<uint32_t> index = snapshotRing_.Writable(fence_->GetCompletedValue());
	if (!index)
	{
		++stats_.snapshotWaits;  // every snapshot is held: the next frame's geometry replaces this one's
		return;
	}
	const entities::EntityFrame& frame = *pendingEntities_;
	SnapshotGpu&                 snapshot = snapshots_[*index];
	const uint64_t               bytes = std::min<uint64_t>(frame.vertices.size() * sizeof(Vertex), kSnapshotMaxBytes);
	if (!snapshot.buffer || snapshot.bytes < bytes)
	{
		// No list of ours reads this one (Writable), so it can go at once.
		if (snapshot.mapped)
		{
			snapshot.buffer->Unmap(0, nullptr);
		}
		snapshot = {};
		const uint64_t size = std::max<uint64_t>(std::bit_ceil(bytes), 64ull << 10);
		snapshot.buffer = gpu::MakeBuffer(device_, size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
		void* mapped = nullptr;
		if (!snapshot.buffer || FAILED(snapshot.buffer->Map(0, nullptr, &mapped)))
		{
			snapshot = {};
			pendingEntities_.reset();
			if (!snapshotLogged_)
			{
				snapshotLogged_ = true;
				log_.Push("renderer: couldn't create a snapshot buffer for Minecraft's entities, so they're skipped until one can be");
			}
			return;
		}
		snapshot.buffer->SetName(L"BlockRender entity snapshot");
		snapshot.mapped = static_cast<uint8_t*>(mapped);
		snapshot.bytes = size;
	}
	if (bytes > 0)
	{
		std::memcpy(snapshot.mapped, frame.vertices.data(), size_t(bytes));
	}
	// A frame over 8 MB keeps its draws in order while they fit; the rest are counted.
	const uint64_t fit = bytes / sizeof(Vertex);
	snapshot.origin = frame.origin;
	snapshot.draws.clear();
	for (const entities::EntityDraw& draw : frame.draws)
	{
		if (uint64_t(draw.first) + draw.count <= fit)
		{
			snapshot.draws.push_back(draw);
		}
		else
		{
			++stats_.snapshotCut;
		}
	}
	if (!blend_)
	{
		entities::UnblendDraws(snapshot.draws);  // set blend 0: every entity layer a cut-out in the G-buffer, as before Part 5
	}
	snapshotRing_.Written(*index);
	pendingEntities_.reset();
}

Renderer::Stats Renderer::TakeStats()
{
	std::lock_guard lock(mutex_);
	Stats stats = stats_;
	stats.gpuMsAverage = gpuSamples_ ? gpuMsSum_ / double(gpuSamples_) : -1.0;
	stats.frameMsAverage = frameSamples_ ? frameMsSum_ / double(frameSamples_) : -1.0;
	stats.updateMsAverage = updateSamples_ ? updateMsSum_ / double(updateSamples_) : -1.0;
	stats.hudCopyMsAverage = hudCopySamples_ ? hudCopyMsSum_ / double(hudCopySamples_) : -1.0;
	stats.hudGpuMsAverage = hudGpuSamples_ ? hudGpuMsSum_ / double(hudGpuSamples_) : -1.0;
	stats.hudWidth = hudShown_.width;
	stats.hudHeight = hudShown_.height;
	stats.hudBottomUp = hudShown_.bottomUp;
	stats.pending = pending_.Size();
	if (atlas_)
	{
		stats.atlasWidth = atlas_->width;
		stats.atlasHeight = atlas_->height;
		stats.atlasLevels = atlas_->Levels();
	}
	stats.sections = sections_.size();
	for (const auto& [section, gpuSection] : sections_)
	{
		stats.vertices += gpuSection.vertices;
		stats.plantVertices += gpuSection.parts.plants;
		stats.translucentVertices += gpuSection.parts.translucent;
	}
	stats.plantFoliage = plantFoliage_;
	stats.entitiesOn = entitiesOn_;
	stats.entityTextures = textures_.Count();
	stats.entityTextureMb = double(textures_.Used()) * 4.0 / 1048576.0;
	stats.entityTexturesLeftOut = texturesLeftOut_;
	stats.entityGpuMsAverage = entityGpuSamples_ ? entityGpuMsSum_ / double(entityGpuSamples_) : -1.0;
	stats.outlineOn = outline_;
	stats.dlssHint = dlssHint_;
	stats.blendOn = blend_;
	stats.forwardGpuMsAverage = forwardGpuSamples_ ? forwardGpuMsSum_ / double(forwardGpuSamples_) : -1.0;
	stats.animateOn = animate_;
	stats.patchAreasWaiting = dirty_.Size();
	stats.patchCpuMsAverage = patchCpuSamples_ ? patchCpuMsSum_ / double(patchCpuSamples_) : -1.0;
	stats.patchGpuMsAverage = patchGpuSamples_ ? patchGpuMsSum_ / double(patchGpuSamples_) : -1.0;
	stats_ = {};
	gpuMsSum_ = frameMsSum_ = updateMsSum_ = hudCopyMsSum_ = hudGpuMsSum_ = entityGpuMsSum_ = forwardGpuMsSum_ = 0.0;
	gpuSamples_ = frameSamples_ = updateSamples_ = hudCopySamples_ = hudGpuSamples_ = entityGpuSamples_ = forwardGpuSamples_ = 0;
	patchCpuMsSum_ = patchGpuMsSum_ = 0.0;
	patchCpuSamples_ = patchGpuSamples_ = 0;
	return stats;
}

FrameConstants Renderer::BaseConstants(const CameraFrame& aCamera) const
{
	FrameConstants constants{};
	for (size_t i = 0; i < 16; ++i)
	{
		constants.cameraToClip[i] = float(aCamera.cameraToClip[i]);
	}
	constants.atlasInfo[0] = atlas_->width;
	constants.atlasInfo[1] = atlas_->height;
	constants.atlasInfo[2] = atlas_->Levels();
	for (size_t i = 0; i < atlas_->levelOffsets.size() && i < 8; ++i)
	{
		constants.atlasLevels[i] = atlas_->levelOffsets[i];
	}
	constants.tuning[0] = brightness_;
	constants.tuning[1] = tolerance_;
	constants.tuning[2] = exposure_;
	constants.tuning[3] = mvSign_;
	constants.material[0] = metalness_;
	constants.material[1] = roughness_;
	constants.material[2] = materialB_;
	constants.material[3] = plantFoliage_ ? plantB_ : materialB_;  // the polish pass's plants
	constants.entity[0] = glow_;
	constants.entity[1] = float(glowLight_);
	return constants;
}

FrameConstants Renderer::JitteredConstants(const CameraFrame& aCamera, const SlConstants& aGame, const sl::Extent& aExtent) const
{
	FrameConstants constants = BaseConstants(aCamera);
	constants.target[0] = float(aExtent.width);
	constants.target[1] = float(aExtent.height);
	constants.target[2] = 1.0f / float(aExtent.width);
	constants.target[3] = 1.0f / float(aExtent.height);
	for (size_t i = 0; i < 16; ++i)
	{
		constants.clipToPrevClip[i] = aGame.clipToPrevClip[i];
	}
	const std::array<double, 2> jitter = ClipJitter(aGame.jitter[0], aGame.jitter[1], aExtent.width, aExtent.height, jitterSign_);
	constants.jitter[0] = float(jitter[0]);
	constants.jitter[1] = float(jitter[1]);
	constants.entity[2] = OutlinePixels(backWidth_.load(), aExtent.width);  // step B's, as Minecraft draws its own
	return constants;
}

namespace
{
// A section's minimum corner, camera-relative, in world axes.
Vec3d SectionOffset(const SectionPos& aSection, const CameraFrame& aCamera)
{
	const Vec3d corner = WorldFromMc({ aSection.x * 16.0, aSection.y * 16.0, aSection.z * 16.0 });
	return { corner.x - aCamera.origin.x, corner.y - aCamera.origin.y, corner.z - aCamera.origin.z };
}

// Whether any of a section is in view, from its camera-relative corner. The section fills world x..x+16, y-16..y and
// z..z+16 from that corner, since Minecraft z runs along world -y.
bool SectionVisible(const CameraFrame& aCamera, const Vec3d& aOffset)
{
	return Visible(aCamera.cameraToClip, { aOffset.x, aOffset.y - 16.0, aOffset.z }, { aOffset.x + 16.0, aOffset.y, aOffset.z + 16.0 });
}

void DrawPart(ID3D12GraphicsCommandList* aList, ID3D12Resource* aBuffer, const Vec3d& aOffset, uint32_t aFirst, uint32_t aCount)
{
	SectionConstants constants{};
	constants.offset[0] = float(aOffset.x);
	constants.offset[1] = float(aOffset.y);
	constants.offset[2] = float(aOffset.z);
	constants.first = aFirst;
	aList->SetGraphicsRoot32BitConstants(1, 4, &constants, 0);
	aList->SetGraphicsRootShaderResourceView(2, aBuffer->GetGPUVirtualAddress());
	d3d::Original().drawInstanced(aList, aCount, 1, 0, 0);
}
}

void Renderer::DrawSections(ID3D12GraphicsCommandList* aList, const CameraFrame& aCamera, uint64_t aCompleted, SectionPart aFrom,
                            SectionPart aTo)
{
	for (const auto& [section, gpuSection] : sections_)
	{
		if (gpuSection.ready > aCompleted)
		{
			continue;  // still uploading
		}
		const uint32_t first = gpuSection.parts.First(aFrom);
		const uint32_t count = gpuSection.parts.First(aTo) + gpuSection.parts.Count(aTo) - first;
		const Vec3d    offset = SectionOffset(section, aCamera);
		if (count == 0 || !SectionVisible(aCamera, offset))
		{
			continue;
		}
		if (watch_ && watch_->submitted && !watch_->drawn && watch_->section == section && watch_->ready == gpuSection.ready)
		{
			watch_->drawn = SteadySeconds();  // the first list that draws the timed upload
			watch_->drawnFrame = presents_.load();
		}
		DrawPart(aList, gpuSection.buffer.Get(), offset, first, count);
	}
}

void Renderer::DrawTranslucentSections(ID3D12GraphicsCommandList* aList, const CameraFrame& aCamera, uint64_t aCompleted)
{
	struct InView
	{
		double            distance = 0.0;  // squared, from the camera to the section's centre
		const SectionGpu* section = nullptr;
		Vec3d             offset;
	};
	std::vector<InView> inView;
	for (const auto& [section, gpuSection] : sections_)
	{
		if (gpuSection.ready > aCompleted || gpuSection.parts.translucent == 0)
		{
			continue;
		}
		const Vec3d offset = SectionOffset(section, aCamera);
		if (!SectionVisible(aCamera, offset))
		{
			continue;
		}
		const Vec3d centre{ offset.x + 8.0, offset.y - 8.0, offset.z + 8.0 };
		inView.push_back({ centre.x * centre.x + centre.y * centre.y + centre.z * centre.z, &gpuSection, offset });
	}
	std::sort(inView.begin(), inView.end(), [](const InView& aA, const InView& aB) { return aA.distance > aB.distance; });
	for (const InView& section : inView)
	{
		const SectionParts& parts = section.section->parts;
		DrawPart(aList, section.section->buffer.Get(), section.offset, parts.First(SectionPart::Translucent), parts.translucent);
	}
}

bool Renderer::ForwardSections() const
{
	return std::any_of(sections_.begin(), sections_.end(), [](const auto& aEntry) { return aEntry.second.parts.translucent > 0; });
}

void Renderer::DrawEntities(ID3D12GraphicsCommandList* aList, const CameraFrame& aCamera, uint64_t aCompleted, const SnapshotGpu& aSnapshot,
                            entities::EntityDraw::Kind aKind, bool aCount)
{
	const KindPipeline kinds[] = { { aKind, nullptr } };
	DrawEntities(aList, aCamera, aCompleted, aSnapshot, kinds, aCount);
}

void Renderer::DrawEntities(ID3D12GraphicsCommandList* aList, const CameraFrame& aCamera, uint64_t aCompleted, const SnapshotGpu& aSnapshot,
                            std::span<const KindPipeline> aKinds, bool aCount)
{
	// The snapshot's positions are relative to its origin, a whole block: camera-relative, as a section's corner is.
	const Vec3d     corner = WorldFromMc(aSnapshot.origin);
	EntityConstants constants{};
	constants.offset[0] = float(corner.x - aCamera.origin.x);
	constants.offset[1] = float(corner.y - aCamera.origin.y);
	constants.offset[2] = float(corner.z - aCamera.origin.z);
	aList->SetGraphicsRootShaderResourceView(2, aSnapshot.buffer->GetGPUVirtualAddress());
	aList->SetGraphicsRootShaderResourceView(5, (entityTextures_ ? entityTextures_ : atlasBuffer_)->GetGPUVirtualAddress());
	ID3D12PipelineState* bound = nullptr;  // the pipeline this call set last
	for (const entities::EntityDraw& draw : aSnapshot.draws)
	{
		const auto kind = std::find_if(aKinds.begin(), aKinds.end(), [&draw](const KindPipeline& aEntry) { return aEntry.kind == draw.kind; });
		if (kind == aKinds.end())
		{
			continue;
		}
		constants.first = draw.first;
		constants.textureStart = 0;  // Minecraft's combined atlas
		constants.textureWidth = 0;
		constants.textureHeight = 0;
		if (draw.texture != 0)
		{
			const EntityTextures::Place* place = textures_.Find(draw.texture);
			const auto                   ready = textureReady_.find(draw.texture);
			if (!place || !entityTextures_ || ready == textureReady_.end() || ready->second > aCompleted)
			{
				stats_.entityDrawsSkipped += aCount ? 1 : 0;  // not on the GPU yet, or left out at the cap
				continue;
			}
			constants.textureStart = place->start;
			constants.textureWidth = place->width;
			constants.textureHeight = place->height;
		}
		if (kind->pipeline && kind->pipeline != bound)
		{
			aList->SetPipelineState(kind->pipeline);
			bound = kind->pipeline;
		}
		aList->SetGraphicsRoot32BitConstants(1, 8, &constants, 0);
		// An outline's vertices are its edges' ends, a pair an edge, from each of which OutlineVS makes six.
		const uint32_t vertices = draw.kind == entities::EntityDraw::Kind::Outline ? draw.count / 2 * 6 : draw.count;
		d3d::Original().drawInstanced(aList, vertices, 1, 0, 0);
	}
}

void Renderer::OnEvaluate(const EvaluateCall& aCall)
{
	const Mode mode = mode_.load();
	if (mode == Mode::Predlss || mode == Mode::Gbuffer)
	{
		Evaluate evaluate;
		evaluate.list = aCall.list;
		evaluate.constants = aCall.constants;
		for (const SlTag& tag : aCall.tags)
		{
			if (tag.type == sl::kBufferTypeScalingInputColor)
			{
				evaluate.colour = tag;
			}
			else if (tag.type == sl::kBufferTypeMotionVectors)
			{
				evaluate.motion = tag;
			}
			else if (tag.type == sl::kBufferTypeDepth)
			{
				evaluate.depth = tag;
			}
			else if (tag.type == sl::kBufferTypeBiasCurrentColorHint)
			{
				evaluate.hint = tag;
			}
		}
		std::lock_guard lock(mutex_);
		if (mode_.load() == Mode::Predlss)
		{
			predlss_ = evaluate;
			predlssList_ = aCall.list;
		}
		else if (mode_.load() == Mode::Gbuffer)
		{
			latest_ = evaluate;
			latestSubmitted_ = false;
			if (auto* depth = static_cast<ID3D12Resource*>(evaluate.depth.resource))
			{
				const D3D12_RESOURCE_DESC desc = depth->GetDesc();
				renderWidth_ = desc.Width;
				renderHeight_ = desc.Height;
			}
			WatchGbuffer();
		}
		return;
	}
	if (mode != Mode::Present)
	{
		return;
	}
	std::lock_guard lock(mutex_);
	if (mode_.load() != Mode::Present || !aCall.list || !EnsureObjects())
	{
		return;
	}
	recorded_ = {};
	recorded_.list = aCall.list;
	recorded_.frame = aCall.frame;
	recorded_.valid = aCall.constants.has_value();
	if (aCall.constants)
	{
		recorded_.constants = *aCall.constants;
	}
	recordedList_ = aCall.list;
	const SlTag* depth = nullptr;
	for (const SlTag& tag : aCall.tags)
	{
		if (tag.type == sl::kBufferTypeDepth && tag.resource && tag.state != 0xFFFFFFFF)
		{
			depth = &tag;
		}
	}
	if (!depth)
	{
		return;  // drawn without the game's depth
	}
	auto*                     resource = static_cast<ID3D12Resource*>(depth->resource);
	const D3D12_RESOURCE_DESC desc = resource->GetDesc();
	if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || DumpFormat(desc.Format, 0) != uint32_t(DXGI_FORMAT_R32_FLOAT))
	{
		return;
	}
	const uint32_t       subresource = gpu::PlaneSubresource(desc, 0);
	const gpu::Footprint footprint = gpu::FootprintOf(device_, desc, subresource);
	if (!depthCopy_ || depthCopyBytes_ < footprint.totalBytes)
	{
		if (depthCopy_)
		{
			oldDepthCopies_.push_back(depthCopy_);
		}
		depthCopy_ = gpu::MakeBuffer(device_, footprint.totalBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
		depthCopyBytes_ = depthCopy_ ? footprint.totalBytes : 0;
		if (!depthCopy_)
		{
			Fail("couldn't create the depth copy");
			return;
		}
		depthCopy_->SetName(L"BlockRender depth copy");
	}
	if (!gpu::RecordCopy(aCall.list, resource, subresource, depth->state, depthCopy_.Get(), footprint))
	{
		return;  // drawn without the game's depth
	}
	recorded_.depth = true;
	recorded_.rowFloats = footprint.layout.Footprint.RowPitch / 4;
	recorded_.extent = DepthExtent(*depth, desc);
}

void Renderer::OnSubmit(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists)
{
	if (mode_.load() == Mode::Gbuffer)
	{
		if (aQueue == d3d::MainQueue())
		{
			std::lock_guard lock(mutex_);
			if (mode_.load() == Mode::Gbuffer)
			{
				SeeGbufferSubmission(aCount, aLists);
			}
		}
		return;
	}
	const void* recorded = recordedList_.load();
	if (!recorded)
	{
		return;
	}
	for (UINT i = 0; i < aCount; ++i)
	{
		if (static_cast<const void*>(aLists[i]) == recorded)
		{
			std::lock_guard lock(mutex_);
			if (recorded_.list == recorded)
			{
				submitted_ = recorded_;
				recorded_ = {};
				recordedList_ = nullptr;
			}
			return;
		}
	}
}

void Renderer::WatchGbuffer()
{
	std::vector<void*> watched;
	if (gbuffer_)
	{
		watched = { gbuffer_->base, gbuffer_->normals, gbuffer_->material, gbuffer_->depth, latest_.motion.resource,
			        latest_.depth.resource };
	}
	if (watched != watched_)
	{
		watched_ = watched;
		knownStates_.Watch(watched_);  // all unknown until the lists show them again
	}
}

void Renderer::SeeGbufferSubmission(UINT aCount, ID3D12CommandList* const* aLists)
{
	const uint32_t frame = d3d::CurrentFrame();
	if (frame != gbufferFrame_.frame)
	{
		gbufferFrame_ = {};
		gbufferFrame_.frame = frame;
		gbufferEnd_ = nullptr;
		knownStates_.Forget();  // only lists submitted in this frame say where things are
	}
	for (UINT i = 0; i < aCount; ++i)
	{
		const void* list = aLists[i];
		if (list == latest_.list && !latestSubmitted_)
		{
			// The frame's DLSS list: the G-buffer is long done, and the next frame's evaluate is the next camera.
			latestSubmitted_ = true;
			if (!gbufferFrame_.decided)
			{
				gbufferFrame_.decided = true;
				// The game draws some frames differently, as a menu opens: they're skipped, but don't switch drawing off.
				Skip(gbufferFrame_.seen ? "the G-buffer's end wasn't found" : "no G-buffer list was seen in this frame", false);
			}
			continue;
		}
		const ListState* state = d3d::Tracker().Find(aLists[i]);
		if (!state || state->Recording() || state->Frame() < trackedSince_)
		{
			knownStates_.Forget();  // a list we can't see into could move anything
			continue;
		}
		for (const Targets& targets : state->WideTargets())
		{
			const std::optional<GbufferTargets> found = MainGbuffer(targets, renderWidth_, renderHeight_);
			if (found)
			{
				gbufferFrame_.seen = true;
				if (found != gbuffer_)
				{
					gbuffer_ = found;
					WatchGbuffer();
				}
			}
		}
		if (gbufferFrame_.seen && !gbufferFrame_.decided && !gbufferFrame_.end && EndsGbuffer(*state, *gbuffer_))
		{
			gbufferFrame_.end = list;
			for (size_t k = 0; k < watched_.size() && k < kWatched; ++k)
			{
				gbufferFrame_.before[k] = knownStates_.Before(*state, watched_[k]);
			}
			gbufferEnd_ = list;
		}
		knownStates_.Apply(*state);
	}
}

ID3D12CommandList* Renderer::ListBefore(ID3D12CommandQueue* aQueue, ID3D12CommandList* aList)
{
	const void* list = aList;
	if (list == predlssList_.load())
	{
		std::lock_guard lock(mutex_);
		if (mode_.load() != Mode::Predlss || predlss_.list != aList)
		{
			return nullptr;
		}
		ID3D12CommandList* ours = RecordPredlss(aQueue, static_cast<ID3D12GraphicsCommandList*>(aList));
		predlss_ = {};  // each evaluate draws once
		predlssList_ = nullptr;
		return ours;
	}
	if (list == forwardList_.load())
	{
		// Step B: the DLSS list of a frame whose G-buffer list left something for the forward list.
		std::lock_guard lock(mutex_);
		if (mode_.load() != Mode::Gbuffer || !forward_ || forward_->list != list)
		{
			return nullptr;
		}
		return RecordForward(aQueue, static_cast<ID3D12GraphicsCommandList*>(aList));
	}
	if (list != gbufferEnd_.load())
	{
		return nullptr;  // most lists: no lock taken
	}
	std::lock_guard lock(mutex_);
	if (mode_.load() != Mode::Gbuffer || gbufferFrame_.end != list || gbufferFrame_.decided)
	{
		return nullptr;
	}
	gbufferFrame_.end = nullptr;
	gbufferFrame_.decided = true;  // each frame draws once
	gbufferEnd_ = nullptr;
	return RecordGbuffer(aQueue);
}

ID3D12CommandList* Renderer::RecordPredlss(ID3D12CommandQueue* aQueue, ID3D12GraphicsCommandList* aDlssList)
{
	if (!EnsureObjects())
	{
		Skip("not ready: the main queue isn't known yet");
		return nullptr;
	}
	const uint64_t completed = fence_->GetCompletedValue();
	if (completed == UINT64_MAX)
	{
		Fail(std::format("the device was removed (0x{:08x})", uint32_t(device_->GetDeviceRemovedReason())));
		return nullptr;
	}
	if (aQueue != queue_)
	{
		Skip("the DLSS list went to another queue than the main one");
		return nullptr;
	}
	if (!predlss_.constants)
	{
		Skip("the DLSS frame came without constants");
		return nullptr;
	}
	auto* colour = static_cast<ID3D12Resource*>(predlss_.colour.resource);
	auto* motion = static_cast<ID3D12Resource*>(predlss_.motion.resource);
	auto* depth = static_cast<ID3D12Resource*>(predlss_.depth.resource);
	if (!colour || !motion || !depth)
	{
		Skip("the scaling input colour, motion vectors and depth aren't all tagged");
		return nullptr;
	}
	const D3D12_RESOURCE_DESC colourDesc = colour->GetDesc();
	const D3D12_RESOURCE_DESC motionDesc = motion->GetDesc();
	const D3D12_RESOURCE_DESC depthDesc = depth->GetDesc();
	std::string               why;
	if (!CheckPredlssTargets(colourDesc, motionDesc, depthDesc, why))
	{
		Skip(why);
		return nullptr;
	}
	const ListState* dlss = d3d::Tracker().Find(aDlssList);
	if (!dlss || dlss->Recording() || int64_t(d3d::CurrentFrame()) - int64_t(dlss->Frame()) > kTrackedFramesAgo)
	{
		Skip("the DLSS list wasn't tracked from its Reset");
		return nullptr;
	}
	const std::optional<uint32_t> colourState = StateBeforeList(*dlss, colour, predlss_.colour.state);
	const std::optional<uint32_t> motionState = StateBeforeList(*dlss, motion, predlss_.motion.state);
	const std::optional<uint32_t> depthState = StateBeforeList(*dlss, depth, predlss_.depth.state);
	if (!colourState || !motionState || !depthState)
	{
		Skip("a tagged buffer's state before the DLSS list isn't known");
		return nullptr;
	}
	if (!atlasBuffer_ || completed < atlasReady_)
	{
		Skip("the atlas isn't on the GPU yet");
		return nullptr;
	}
	const std::optional<CameraFrame> camera = MakeCameraFrame(*predlss_.constants, renderOrigin_);
	if (!camera)
	{
		Skip("the camera isn't understood");
		return nullptr;
	}
	if (!EnsurePredlssPipeline(camera->reversedDepth))
	{
		return nullptr;
	}
	const uint32_t slot = uint32_t(recordings_ % kSlots);
	if (completed < slotFence_[slot])
	{
		Skip("the GPU is still busy with an earlier frame");
		return nullptr;
	}
	if (slotFence_[slot] != 0)
	{
		ReadTimings(slot);
	}

	const sl::Extent     extent = DepthExtent(predlss_.depth, depthDesc);
	const FrameConstants constants = JitteredConstants(*camera, *predlss_.constants, extent);
	std::memcpy(constantsMapped_ + slot * kConstantsStride, &constants, sizeof(constants));

	// Our own list: calls the hooks watch go through their originals, so nothing of ours reaches the tracker.
	const d3d::Originals&      original = d3d::Original();
	ID3D12GraphicsCommandList* list = lists_[slot].Get();
	if (FAILED(allocators_[slot]->Reset()) || FAILED(original.reset(list, allocators_[slot].Get(), predlssPipeline_.Get())))
	{
		Fail("couldn't reset a command list");
		return nullptr;
	}
	list->EndQuery(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2);
	const std::vector<Move> into = MovesInto({ { colour, *colourState, D3D12_RESOURCE_STATE_RENDER_TARGET },
	                                           { motion, *motionState, D3D12_RESOURCE_STATE_RENDER_TARGET },
	                                           { depth, *depthState, D3D12_RESOURCE_STATE_DEPTH_WRITE } });
	const std::vector<D3D12_RESOURCE_BARRIER> toTargets = Barriers(into);
	if (!toTargets.empty())
	{
		original.resourceBarrier(list, UINT(toTargets.size()), toTargets.data());
	}
	const D3D12_CPU_DESCRIPTOR_HANDLE rtvs[2] = { Offset(rtvHeap_->GetCPUDescriptorHandleForHeapStart(), kPredlssRtvs, rtvIncrement_),
		                                          Offset(rtvHeap_->GetCPUDescriptorHandleForHeapStart(), kPredlssRtvs + 1, rtvIncrement_) };
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv = Offset(dsvHeap_->GetCPUDescriptorHandleForHeapStart(), kPredlssDsv, dsvIncrement_);
	D3D12_RENDER_TARGET_VIEW_DESC     colourView{};
	colourView.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	colourView.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	D3D12_RENDER_TARGET_VIEW_DESC motionView = colourView;
	motionView.Format = DXGI_FORMAT_R16G16_FLOAT;
	D3D12_DEPTH_STENCIL_VIEW_DESC depthView{};
	depthView.Format = DepthViewFormat(depthDesc.Format);
	depthView.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	original.createRtv(device_, colour, &colourView, rtvs[0]);
	original.createRtv(device_, motion, &motionView, rtvs[1]);
	original.createDsv(device_, depth, &depthView, dsv);
	original.omSetRenderTargets(list, 2, rtvs, FALSE, &dsv);
	const D3D12_VIEWPORT viewport{ float(extent.left), float(extent.top), float(extent.width), float(extent.height), 0.0f, 1.0f };
	const D3D12_RECT     scissor{ LONG(extent.left), LONG(extent.top), LONG(extent.left + extent.width), LONG(extent.top + extent.height) };
	list->RSSetViewports(1, &viewport);
	list->RSSetScissorRects(1, &scissor);
	list->SetGraphicsRootSignature(rootSignature_.Get());
	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	list->SetGraphicsRootConstantBufferView(0, constants_->GetGPUVirtualAddress() + slot * kConstantsStride);
	list->SetGraphicsRootShaderResourceView(3, atlasBuffer_->GetGPUVirtualAddress());
	list->SetGraphicsRootShaderResourceView(4, atlasBuffer_->GetGPUVirtualAddress());  // t2: only step 2's shader reads it
	list->SetGraphicsRootShaderResourceView(5, atlasBuffer_->GetGPUVirtualAddress());  // t3: only the entities' read it
	DrawSections(list, *camera, completed, SectionPart::Solid, SectionPart::Plant);  // translucent blocks: gbuffer's alone
	const std::vector<D3D12_RESOURCE_BARRIER> back = Barriers(MovesBack(into));
	if (!back.empty())
	{
		original.resourceBarrier(list, UINT(back.size()), back.data());
	}
	list->EndQuery(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2 + 1);
	list->ResolveQueryData(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2, 2, timings_.Get(), slot * 2 * sizeof(uint64_t));
	if (FAILED(original.close(list)))
	{
		Fail("couldn't close a command list");
		return nullptr;
	}
	++recordings_;
	t_insertedSlot = slot;
	retired_.ListRecorded();  // until OnSubmitted signals after it, what it draws isn't freed
	return list;
}

ID3D12CommandList* Renderer::RecordGbuffer(ID3D12CommandQueue* aQueue)
{
	if (!EnsureObjects())
	{
		Skip("not ready: the main queue isn't known yet");
		return nullptr;
	}
	const uint64_t completed = fence_->GetCompletedValue();
	if (completed == UINT64_MAX)
	{
		Fail(std::format("the device was removed (0x{:08x})", uint32_t(device_->GetDeviceRemovedReason())));
		return nullptr;
	}
	if (aQueue != queue_)
	{
		Skip("the G-buffer ended on another queue than the main one");
		return nullptr;
	}
	if (!latest_.list || latestSubmitted_)
	{
		++stats_.lateCamera;  // this frame's evaluate, which brings its camera, hasn't come yet
		Skip("the frame's camera came after its G-buffer", false);
		return nullptr;
	}
	if (!latest_.constants)
	{
		Skip("the DLSS frame came without constants");
		return nullptr;
	}
	auto* colour = static_cast<ID3D12Resource*>(latest_.colour.resource);
	auto* motion = static_cast<ID3D12Resource*>(latest_.motion.resource);
	auto* taggedDepth = static_cast<ID3D12Resource*>(latest_.depth.resource);
	if (!colour || !motion || !taggedDepth)
	{
		Skip("the scaling input colour, motion vectors and depth aren't all tagged");
		return nullptr;
	}
	const D3D12_RESOURCE_DESC depthDesc = taggedDepth->GetDesc();
	std::string               why;
	if (!CheckPredlssTargets(colour->GetDesc(), motion->GetDesc(), depthDesc, why))
	{
		Skip(why);
		return nullptr;
	}
	if (!gbuffer_ || watched_.size() != kWatched || watched_[kMotion] != motion || watched_[kTaggedDepth] != taggedDepth)
	{
		Skip("the G-buffer and the tagged buffers aren't the ones watched");
		return nullptr;
	}
	static constexpr const char* kNames[kTaggedDepth] = { "base colour", "normals", "material values", "G-buffer's depth",
		                                                  "motion vectors" };
	for (size_t k = 0; k < kTaggedDepth; ++k)
	{
		if (!gbufferFrame_.before[k])
		{
			Skip(std::format("the state of the {} before the G-buffer's end isn't known", kNames[k]));
			return nullptr;
		}
	}
	// The game copies the G-buffer's depth into the tagged depth before the end, except in a few frames, such as while
	// the Skip Time screen opens. Those frames get the blocks in the G-buffer only.
	const bool intoTaggedDepth = gbufferDepth_ && gbufferFrame_.before[kTaggedDepth].has_value();
	if (!atlasBuffer_ || completed < atlasReady_)
	{
		Skip("the atlas isn't on the GPU yet");
		return nullptr;
	}
	const std::optional<CameraFrame> camera = MakeCameraFrame(*latest_.constants, renderOrigin_);
	if (!camera)
	{
		Skip("the camera isn't understood");
		return nullptr;
	}
	if (!EnsureGbufferPipeline(camera->reversedDepth) || (intoTaggedDepth && !EnsureDepthPipeline(camera->reversedDepth)))
	{
		return nullptr;
	}
	const uint32_t slot = uint32_t(recordings_ % kSlots);
	if (completed < slotFence_[slot])
	{
		Skip("the GPU is still busy with an earlier frame");
		return nullptr;
	}
	if (slotFence_[slot] != 0)
	{
		ReadTimings(slot);
	}

	const sl::Extent     extent = DepthExtent(latest_.depth, depthDesc);
	const FrameConstants constants = JitteredConstants(*camera, *latest_.constants, extent);
	std::memcpy(constantsMapped_ + slot * kConstantsStride, &constants, sizeof(constants));

	// Our own list: calls the hooks watch go through their originals, so nothing of ours reaches the tracker.
	const d3d::Originals&      original = d3d::Original();
	ID3D12GraphicsCommandList* list = lists_[slot].Get();
	if (FAILED(allocators_[slot]->Reset()) || FAILED(original.reset(list, allocators_[slot].Get(), gbufferPipeline_.Get())))
	{
		Fail("couldn't reset a command list");
		return nullptr;
	}
	list->EndQuery(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2);
	ID3D12Resource* targets[4] = { static_cast<ID3D12Resource*>(gbuffer_->base), static_cast<ID3D12Resource*>(gbuffer_->normals),
		                           static_cast<ID3D12Resource*>(gbuffer_->material), motion };
	auto* gbufferDepth = static_cast<ID3D12Resource*>(gbuffer_->depth);
	std::vector<Move> wanted = { { targets[0], *gbufferFrame_.before[kBase], D3D12_RESOURCE_STATE_RENDER_TARGET },
		                         { targets[1], *gbufferFrame_.before[kNormals], D3D12_RESOURCE_STATE_RENDER_TARGET },
		                         { targets[2], *gbufferFrame_.before[kMaterial], D3D12_RESOURCE_STATE_RENDER_TARGET },
		                         { motion, *gbufferFrame_.before[kMotion], D3D12_RESOURCE_STATE_RENDER_TARGET },
		                         { gbufferDepth, *gbufferFrame_.before[kGbufferDepth], D3D12_RESOURCE_STATE_DEPTH_WRITE } };
	if (intoTaggedDepth)
	{
		wanted.push_back({ taggedDepth, *gbufferFrame_.before[kTaggedDepth], D3D12_RESOURCE_STATE_DEPTH_WRITE });
	}
	const std::vector<Move>                   into = MovesInto(wanted);
	const std::vector<D3D12_RESOURCE_BARRIER> toTargets = Barriers(into);
	if (!toTargets.empty())
	{
		original.resourceBarrier(list, UINT(toTargets.size()), toTargets.data());
	}
	D3D12_CPU_DESCRIPTOR_HANDLE rtvs[4]{};
	for (uint32_t i = 0; i < 4; ++i)
	{
		rtvs[i] = Offset(rtvHeap_->GetCPUDescriptorHandleForHeapStart(), kGbufferRtvs + i, rtvIncrement_);
		D3D12_RENDER_TARGET_VIEW_DESC view{};
		view.Format = kGbufferFormats[i];
		view.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;  // mip 0, as the game's views
		original.createRtv(device_, targets[i], &view, rtvs[i]);
	}
	const D3D12_CPU_DESCRIPTOR_HANDLE gbufferDsv = Offset(dsvHeap_->GetCPUDescriptorHandleForHeapStart(), kGbufferDsv, dsvIncrement_);
	D3D12_DEPTH_STENCIL_VIEW_DESC     gbufferDepthView{};
	gbufferDepthView.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	gbufferDepthView.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	original.createDsv(device_, gbufferDepth, &gbufferDepthView, gbufferDsv);
	original.omSetRenderTargets(list, 4, rtvs, FALSE, &gbufferDsv);
	list->OMSetStencilRef(stencil_);
	const D3D12_VIEWPORT viewport{ float(extent.left), float(extent.top), float(extent.width), float(extent.height), 0.0f, 1.0f };
	const D3D12_RECT     scissor{ LONG(extent.left), LONG(extent.top), LONG(extent.left + extent.width), LONG(extent.top + extent.height) };
	list->RSSetViewports(1, &viewport);
	list->RSSetScissorRects(1, &scissor);
	list->SetGraphicsRootSignature(rootSignature_.Get());
	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	list->SetGraphicsRootConstantBufferView(0, constants_->GetGPUVirtualAddress() + slot * kConstantsStride);
	list->SetGraphicsRootShaderResourceView(3, atlasBuffer_->GetGPUVirtualAddress());
	list->SetGraphicsRootShaderResourceView(4, atlasBuffer_->GetGPUVirtualAddress());  // t2: only step 2's shader reads it
	list->SetGraphicsRootShaderResourceView(5, atlasBuffer_->GetGPUVirtualAddress());  // t3: until the entities' own
	DrawSections(list, *camera, completed, SectionPart::Solid, SectionPart::Solid);  // translucent blocks: the forward list's
	// The polish pass: plants with the game's class for its own small plants (set plants 1), whose lighting doesn't
	// shadow them dark as the blocks' class does a field of grass; their material B comes from the constants.
	list->OMSetStencilRef(plantFoliage_ ? plantStencil_ : stencil_);
	DrawSections(list, *camera, completed, SectionPart::Plant, SectionPart::Plant);
	list->OMSetStencilRef(stencil_);
	// Spike 7: Minecraft's entities after the blocks, from the newest snapshot, if it has any: its models, then their
	// cracks over the base colour alone. The snapshot is held until the fence after this list.
	const std::optional<uint32_t> newest = snapshotRing_.Newest();
	const std::optional<uint32_t> snapshot = entitiesOn_ && newest && !snapshots_[*newest].draws.empty() && EnsureEntityPipelines(camera->reversedDepth)
	                                             ? snapshotRing_.TakeNewest()
	                                             : std::nullopt;
	if (snapshot)
	{
		const SnapshotGpu& shot = snapshots_[*snapshot];
		list->EndQuery(entityQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2);
		list->SetPipelineState(entityPipeline_.Get());
		DrawEntities(list, *camera, completed, shot, entities::EntityDraw::Kind::Model, true);
		original.omSetRenderTargets(list, 1, rtvs, FALSE, &gbufferDsv);
		list->SetPipelineState(crackPipeline_.Get());
		DrawEntities(list, *camera, completed, shot, entities::EntityDraw::Kind::Crack, true);
		list->EndQuery(entityQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2 + 1);
		list->ResolveQueryData(entityQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2, 2, entityTimings_.Get(), slot * 2 * sizeof(uint64_t));
	}
	if (intoTaggedDepth)
	{
		// The game copied the G-buffer's depth into the tagged depth before our list, so the blocks go in there too.
		const D3D12_CPU_DESCRIPTOR_HANDLE taggedDsv = Offset(dsvHeap_->GetCPUDescriptorHandleForHeapStart(), kTaggedDepthDsv, dsvIncrement_);
		D3D12_DEPTH_STENCIL_VIEW_DESC     taggedView{};
		taggedView.Format = DepthViewFormat(depthDesc.Format);
		taggedView.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		original.createDsv(device_, taggedDepth, &taggedView, taggedDsv);
		list->SetPipelineState(depthPipeline_.Get());
		original.omSetRenderTargets(list, 0, nullptr, FALSE, &taggedDsv);
		// Plants too: the game lights each pixel where this depth puts it, so one missing from it is lit as what's behind.
		DrawSections(list, *camera, completed, SectionPart::Solid, SectionPart::Plant);
		if (snapshot)
		{
			list->SetPipelineState(entityDepthPipeline_.Get());
			DrawEntities(list, *camera, completed, snapshots_[*snapshot], entities::EntityDraw::Kind::Model, false);
		}
	}
	const std::vector<D3D12_RESOURCE_BARRIER> back = Barriers(MovesBack(into));
	if (!back.empty())
	{
		original.resourceBarrier(list, UINT(back.size()), back.data());
	}
	list->EndQuery(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2 + 1);
	list->ResolveQueryData(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2, 2, timings_.Get(), slot * 2 * sizeof(uint64_t));
	if (FAILED(original.close(list)))
	{
		if (snapshot)
		{
			snapshotRing_.Abandon(*snapshot);
		}
		Fail("couldn't close a command list");
		return nullptr;
	}
	++recordings_;
	if (gbufferDepth_ && !intoTaggedDepth)
	{
		++stats_.partial;
	}
	slotSnapshot_[slot] = snapshot;
	slotEntityTimed_[slot] = snapshot.has_value();
	stats_.entityFrames += snapshot ? 1 : 0;
	// Step B: the frame's forward list, before its DLSS list, if the snapshot has anything for it, with the snapshot held
	// for it, or if the sections have (the polish pass). A claim an earlier frame left, whose forward list never came, is
	// let go.
	DropForward();
	const bool forwardSnapshot = snapshot && entities::ForwardDraws(snapshots_[*snapshot].draws, outline_) && snapshotRing_.Take(*snapshot);
	if (forwardSnapshot || ForwardSections())
	{
		forward_ = Forward{ latest_.list, latest_, *camera, constants, extent, forwardSnapshot ? snapshot : std::nullopt };
		forwardList_ = latest_.list;
	}
	t_insertedSlot = slot;
	retired_.ListRecorded();  // until OnSubmitted signals after it, what it draws isn't freed
	return list;
}

ID3D12CommandList* Renderer::RecordForward(ID3D12CommandQueue* aQueue, ID3D12GraphicsCommandList* aDlssList)
{
	// The frame's hold on its snapshot, if it has one, moves to our list, or is let go if the list can't be drawn.
	const Forward forward = *std::exchange(forward_, std::nullopt);
	forwardList_ = nullptr;
	const auto letGo = [&] {
		if (forward.snapshot)
		{
			snapshotRing_.Abandon(*forward.snapshot);
		}
	};
	const auto skip = [&](const std::string& aWhy) -> ID3D12CommandList* {
		letGo();
		ForwardSkip(aWhy);
		return nullptr;
	};
	const uint64_t completed = fence_->GetCompletedValue();
	if (completed == UINT64_MAX)
	{
		letGo();
		Fail(std::format("the device was removed (0x{:08x})", uint32_t(device_->GetDeviceRemovedReason())));
		return nullptr;
	}
	if (aQueue != queue_)
	{
		return skip("the DLSS list went to another queue than the main one");
	}
	if (!atlasBuffer_ || completed < atlasReady_)
	{
		return skip("the atlas isn't on the GPU");
	}
	// The colour and the depth passed the G-buffer list's checks for this same frame (CheckPredlssTargets).
	auto*            colour = static_cast<ID3D12Resource*>(forward.evaluate.colour.resource);
	auto*            depth = static_cast<ID3D12Resource*>(forward.evaluate.depth.resource);
	auto*            hint = static_cast<ID3D12Resource*>(forward.evaluate.hint.resource);
	const ListState* dlss = d3d::Tracker().Find(aDlssList);
	if (!dlss || dlss->Recording() || int64_t(d3d::CurrentFrame()) - int64_t(dlss->Frame()) > kTrackedFramesAgo)
	{
		return skip("the DLSS list wasn't tracked from its Reset");
	}
	const std::optional<uint32_t> colourState = StateBeforeList(*dlss, colour, forward.evaluate.colour.state);
	const std::optional<uint32_t> depthState = StateBeforeList(*dlss, depth, forward.evaluate.depth.state);
	if (!colourState || !depthState)
	{
		return skip("the colour's or the depth's state before the DLSS list isn't known");
	}
	// DLSS's bias hint, while set dlsshint is 1, if the game tags one as step 1 found it and its state is known.
	std::string             why;
	std::optional<uint32_t> hintState;
	if (dlssHint_ == 1 && hint && CheckHintTarget(hint->GetDesc(), colour->GetDesc(), why))
	{
		hintState = StateBeforeList(*dlss, hint, forward.evaluate.hint.state);
	}
	if (!EnsureForwardObjects())
	{
		return skip("couldn't create the forward list's objects");
	}
	if (!EnsureForwardPipelines(forward.camera.reversedDepth))
	{
		return skip("the forward list's pipelines couldn't be made");
	}
	const bool     sections = ForwardSections();  // the polish pass's translucent blocks, which write the depth
	const uint32_t slot = uint32_t(forwardRecordings_ % kSlots);
	if (completed < forwardSlotFence_[slot])
	{
		return skip("the GPU is still busy with an earlier frame");
	}
	if (forwardSlotFence_[slot] != 0)
	{
		ReadForwardTimings(slot);
	}
	std::memcpy(forwardConstantsMapped_ + slot * kConstantsStride, &forward.constants, sizeof(forward.constants));

	// Our own list: calls the hooks watch go through their originals, so nothing of ours reaches the tracker.
	const d3d::Originals&      original = d3d::Original();
	ID3D12GraphicsCommandList* list = forwardLists_[slot].Get();
	if (FAILED(forwardAllocators_[slot]->Reset()) || FAILED(original.reset(list, forwardAllocators_[slot].Get(), translucentPipeline_.Get())))
	{
		letGo();
		Fail("couldn't reset a command list");
		return nullptr;
	}
	list->EndQuery(forwardQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2);
	std::vector<Move> wanted = { { colour, *colourState, D3D12_RESOURCE_STATE_RENDER_TARGET },
		                         { depth, *depthState, uint32_t(sections ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_DEPTH_READ) } };
	if (hintState)
	{
		wanted.push_back({ hint, *hintState, D3D12_RESOURCE_STATE_RENDER_TARGET });
	}
	const std::vector<Move>                   into = MovesInto(wanted);
	const std::vector<D3D12_RESOURCE_BARRIER> toTargets = Barriers(into);
	if (!toTargets.empty())
	{
		original.resourceBarrier(list, UINT(toTargets.size()), toTargets.data());
	}
	const D3D12_CPU_DESCRIPTOR_HANDLE colourRtv = Offset(rtvHeap_->GetCPUDescriptorHandleForHeapStart(), kForwardRtvs, rtvIncrement_);
	const D3D12_CPU_DESCRIPTOR_HANDLE hintRtv = Offset(rtvHeap_->GetCPUDescriptorHandleForHeapStart(), kForwardRtvs + 1, rtvIncrement_);
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv = Offset(dsvHeap_->GetCPUDescriptorHandleForHeapStart(), kForwardDsv, dsvIncrement_);
	D3D12_RENDER_TARGET_VIEW_DESC     colourView{};
	colourView.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	colourView.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	original.createRtv(device_, colour, &colourView, colourRtv);
	if (hintState)
	{
		D3D12_RENDER_TARGET_VIEW_DESC hintView = colourView;
		hintView.Format = DXGI_FORMAT_R8_UNORM;
		original.createRtv(device_, hint, &hintView, hintRtv);
	}
	D3D12_DEPTH_STENCIL_VIEW_DESC depthView{};
	depthView.Format = DepthViewFormat(depth->GetDesc().Format);
	depthView.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	// Tested, and written only by the translucent blocks; the particles' and the layers' pipelines never write it.
	depthView.Flags = sections ? D3D12_DSV_FLAG_NONE : D3D12_DSV_FLAG_READ_ONLY_DEPTH;
	original.createDsv(device_, depth, &depthView, dsv);
	const sl::Extent&    extent = forward.extent;
	const D3D12_VIEWPORT viewport{ float(extent.left), float(extent.top), float(extent.width), float(extent.height), 0.0f, 1.0f };
	const D3D12_RECT     scissor{ LONG(extent.left), LONG(extent.top), LONG(extent.left + extent.width), LONG(extent.top + extent.height) };
	list->RSSetViewports(1, &viewport);
	list->RSSetScissorRects(1, &scissor);
	list->SetGraphicsRootSignature(rootSignature_.Get());
	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	list->SetGraphicsRootConstantBufferView(0, forwardConstants_->GetGPUVirtualAddress() + slot * kConstantsStride);
	list->SetGraphicsRootShaderResourceView(3, atlasBuffer_->GetGPUVirtualAddress());
	list->SetGraphicsRootShaderResourceView(4, atlasBuffer_->GetGPUVirtualAddress());  // t2: only step 2's shader reads it
	list->SetGraphicsRootShaderResourceView(5, atlasBuffer_->GetGPUVirtualAddress());  // t3: until the entities' own
	if (sections)
	{
		// The polish pass: the translucent blocks, the farthest section first, before the particles, as Minecraft draws them.
		list->SetPipelineState(blendedBlockPipeline_.Get());
		original.omSetRenderTargets(list, 1, &colourRtv, FALSE, &dsv);
		DrawTranslucentSections(list, forward.camera, completed);
	}
	// The translucent particles and Part 5's blended and additive layers, in the scene's order, each kind with its pipeline;
	// then the outline, blended into the colour; then the bias hint under the particles and the layers.
	if (forward.snapshot)
	{
		using Kind = entities::EntityDraw::Kind;
		const SnapshotGpu& shot = snapshots_[*forward.snapshot];
		original.omSetRenderTargets(list, 1, &colourRtv, FALSE, &dsv);
		const KindPipeline layers[] = { { Kind::Translucent, translucentPipeline_.Get() }, { Kind::Blended, blendedPipeline_.Get() },
			                            { Kind::Additive, additivePipeline_.Get() } };
		DrawEntities(list, forward.camera, completed, shot, layers, true);
		if (outline_)
		{
			list->SetPipelineState(outlinePipeline_.Get());
			DrawEntities(list, forward.camera, completed, shot, Kind::Outline, false);
		}
		if (hintState)
		{
			list->SetPipelineState(hintPipeline_.Get());
			original.omSetRenderTargets(list, 1, &hintRtv, FALSE, &dsv);
			const KindPipeline hinted[] = { { Kind::Translucent, nullptr }, { Kind::Blended, nullptr }, { Kind::Additive, nullptr } };
			DrawEntities(list, forward.camera, completed, shot, hinted, false);
		}
	}
	const std::vector<D3D12_RESOURCE_BARRIER> back = Barriers(MovesBack(into));
	if (!back.empty())
	{
		original.resourceBarrier(list, UINT(back.size()), back.data());
	}
	list->EndQuery(forwardQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2 + 1);
	list->ResolveQueryData(forwardQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2, 2, forwardTimings_.Get(), slot * 2 * sizeof(uint64_t));
	if (FAILED(original.close(list)))
	{
		letGo();
		Fail("couldn't close a command list");
		return nullptr;
	}
	++forwardRecordings_;
	forwardSlotSnapshot_[slot] = forward.snapshot;
	++stats_.forwardFrames;
	stats_.forwardHinted += hintState ? 1 : 0;
	t_insertedForward = slot;
	retired_.ListRecorded();  // until OnSubmitted signals after it, what it draws isn't freed
	return list;
}

void Renderer::OnSubmitted(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*)
{
	if (!t_insertedSlot && !t_insertedForward)
	{
		return;
	}
	const std::optional<uint32_t> slot = std::exchange(t_insertedSlot, std::nullopt);
	const std::optional<uint32_t> forward = std::exchange(t_insertedForward, std::nullopt);
	std::lock_guard               lock(mutex_);
	const uint64_t                fence = Signal();  // signalled whatever the mode is now: our lists went to the GPU
	if (slot)
	{
		slotFence_[*slot] = fence;
		retired_.ListSubmitted(fence);
		if (const std::optional<uint32_t> snapshot = std::exchange(slotSnapshot_[*slot], std::nullopt))
		{
			snapshotRing_.Submitted(*snapshot, fence);
		}
		++stats_.drawn;
		consecutiveSkips_ = 0;
	}
	if (forward)
	{
		forwardSlotFence_[*forward] = fence;
		retired_.ListSubmitted(fence);
		if (const std::optional<uint32_t> snapshot = std::exchange(forwardSlotSnapshot_[*forward], std::nullopt))
		{
			snapshotRing_.Submitted(*snapshot, fence);
		}
	}
}

void Renderer::OnPresent(IDXGISwapChain* aSwapChain)
{
	const int64_t now = Now();
	++presents_;
	DXGI_SWAP_CHAIN_DESC swapChainDesc{};
	if (SUCCEEDED(aSwapChain->GetDesc(&swapChainDesc)))
	{
		backWidth_ = swapChainDesc.BufferDesc.Width;
		backHeight_ = swapChainDesc.BufferDesc.Height;
	}
	std::lock_guard lock(mutex_);
	if (lastPresent_ != 0)
	{
		const double ms = MsBetween(lastPresent_, now);
		frameMsSum_ += ms;
		++frameSamples_;
		stats_.frameMsWorst = std::max(stats_.frameMsWorst, ms);
	}
	lastPresent_ = now;
	if (mode_.load() == Mode::Present)
	{
		PresentBlocks(aSwapChain);
	}
	if (hudWanted_.load())
	{
		PresentHud(aSwapChain);
	}
}

bool Renderer::OnMainQueue(IDXGISwapChain* aSwapChain)
{
	if (aSwapChain != checkedSwapChain_)
	{
		checkedSwapChain_ = aSwapChain;
		swapChainOnMainQueue_ = d3d::PresentQueue(aSwapChain) == queue_;
		log_.Push(std::format("renderer: swap chain 0x{:x} presents on {}", reinterpret_cast<uintptr_t>(aSwapChain),
		                      swapChainOnMainQueue_ ? "the main queue" : "another queue"));
	}
	return swapChainOnMainQueue_;
}

void Renderer::PresentBlocks(IDXGISwapChain* aSwapChain)
{
	if (!EnsureObjects())
	{
		return Skip("not ready: the main queue isn't known yet");
	}
	const uint64_t completed = fence_->GetCompletedValue();
	if (completed == UINT64_MAX)
	{
		return Fail(std::format("the device was removed (0x{:08x})", uint32_t(device_->GetDeviceRemovedReason())));
	}
	if (!OnMainQueue(aSwapChain))
	{
		return Skip("the swap chain presents on another queue than DLSS's");
	}
	if (!submitted_.list)
	{
		return Skip("no DLSS frame has been submitted yet");
	}
	if (!submitted_.valid)
	{
		return Skip("the DLSS frame came without constants");
	}
	if (!atlasBuffer_ || completed < atlasReady_)
	{
		return Skip("the atlas isn't on the GPU yet");
	}
	const std::optional<CameraFrame> camera = MakeCameraFrame(submitted_.constants, renderOrigin_);
	if (!camera)
	{
		return Skip("the camera isn't understood");
	}
	gpu::ComPtr<IDXGISwapChain3> swapChain;
	gpu::ComPtr<ID3D12Resource>  backBuffer;
	if (FAILED(aSwapChain->QueryInterface(IID_PPV_ARGS(&swapChain))) ||
	    FAILED(swapChain->GetBuffer(swapChain->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&backBuffer))))
	{
		return Skip("no back buffer");
	}
	const D3D12_RESOURCE_DESC target = backBuffer->GetDesc();
	if (!EnsurePipeline(target.Format, camera->reversedDepth) || !EnsureDepthTarget(uint32_t(target.Width), target.Height))
	{
		return;
	}
	const uint32_t slot = uint32_t(recordings_ % kSlots);
	if (completed < slotFence_[slot])
	{
		return Skip("the GPU is still busy with an earlier frame");
	}
	if (slotFence_[slot] != 0)
	{
		ReadTimings(slot);
	}

	FrameConstants constants = BaseConstants(*camera);  // no jitter: the image is already resolved
	constants.target[0] = float(target.Width);
	constants.target[1] = float(target.Height);
	constants.target[2] = 1.0f / float(target.Width);
	constants.target[3] = 1.0f / float(target.Height);
	constants.depthExtent[0] = float(submitted_.extent.left);
	constants.depthExtent[1] = float(submitted_.extent.top);
	constants.depthExtent[2] = float(submitted_.extent.width);
	constants.depthExtent[3] = float(submitted_.extent.height);
	constants.depthInfo[0] = submitted_.rowFloats;
	constants.depthInfo[1] = submitted_.depth ? 1u : 0u;
	constants.depthInfo[2] = camera->reversedDepth ? 1u : 0u;
	std::memcpy(constantsMapped_ + slot * kConstantsStride, &constants, sizeof(constants));

	// Our own list: calls the hooks watch go through their originals, so nothing of ours reaches the tracker.
	const d3d::Originals&      original = d3d::Original();
	ID3D12GraphicsCommandList* list = lists_[slot].Get();
	if (FAILED(allocators_[slot]->Reset()) || FAILED(original.reset(list, allocators_[slot].Get(), pipeline_.Get())))
	{
		return Fail("couldn't reset a command list");
	}
	list->EndQuery(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2);
	const D3D12_RESOURCE_BARRIER toTarget = Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	original.resourceBarrier(list, 1, &toTarget);
	const D3D12_CPU_DESCRIPTOR_HANDLE rtv = Offset(rtvHeap_->GetCPUDescriptorHandleForHeapStart(), kBackBufferRtv, rtvIncrement_);
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv = Offset(dsvHeap_->GetCPUDescriptorHandleForHeapStart(), kOwnDepthDsv, dsvIncrement_);
	original.createRtv(device_, backBuffer.Get(), nullptr, rtv);
	original.clearDsv(list, dsv, D3D12_CLEAR_FLAG_DEPTH, camera->reversedDepth ? 0.0f : 1.0f, 0, 0, nullptr);
	original.omSetRenderTargets(list, 1, &rtv, FALSE, &dsv);
	const D3D12_VIEWPORT viewport{ 0.0f, 0.0f, float(target.Width), float(target.Height), 0.0f, 1.0f };
	const D3D12_RECT     scissor{ 0, 0, LONG(target.Width), LONG(target.Height) };
	list->RSSetViewports(1, &viewport);
	list->RSSetScissorRects(1, &scissor);
	list->SetGraphicsRootSignature(rootSignature_.Get());
	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	list->SetGraphicsRootConstantBufferView(0, constants_->GetGPUVirtualAddress() + slot * kConstantsStride);
	list->SetGraphicsRootShaderResourceView(3, atlasBuffer_->GetGPUVirtualAddress());
	// Without a depth copy the shader never reads t2, but the root argument still needs a valid buffer.
	list->SetGraphicsRootShaderResourceView(4, (submitted_.depth ? depthCopy_ : atlasBuffer_)->GetGPUVirtualAddress());
	list->SetGraphicsRootShaderResourceView(5, atlasBuffer_->GetGPUVirtualAddress());  // t3: only the entities' read it
	DrawSections(list, *camera, completed, SectionPart::Solid, SectionPart::Plant);  // translucent blocks: gbuffer's alone
	const D3D12_RESOURCE_BARRIER toPresent = Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	original.resourceBarrier(list, 1, &toPresent);
	list->EndQuery(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2 + 1);
	list->ResolveQueryData(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2, 2, timings_.Get(), slot * 2 * sizeof(uint64_t));
	if (FAILED(original.close(list)))
	{
		return Fail("couldn't close a command list");
	}
	ID3D12CommandList* lists[] = { list };
	original.executeCommandLists(queue_, 1, lists);
	slotFence_[slot] = Signal();
	++recordings_;
	++stats_.drawn;
	if (lastDrawnFrame_ == submitted_.frame)
	{
		++stats_.lateCamera;
	}
	lastDrawnFrame_ = submitted_.frame;
	consecutiveSkips_ = 0;
}

bool Renderer::EnsureHudObjects()
{
	if (hudTimingsMapped_)
	{
		return true;
	}
	const D3D12_QUERY_HEAP_DESC queryDesc{ D3D12_QUERY_HEAP_TYPE_TIMESTAMP, kSlots * 2, 0 };
	if (!hudTimings_)
	{
		hudTimings_ = gpu::MakeBuffer(device_, kSlots * 2 * sizeof(uint64_t), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
	}
	if (!hudTimings_ || (!hudQueries_ && FAILED(device_->CreateQueryHeap(&queryDesc, IID_PPV_ARGS(&hudQueries_)))))
	{
		return false;
	}
	for (uint32_t slot = 0; slot < kSlots; ++slot)
	{
		if (hudLists_[slot])
		{
			continue;
		}
		if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&hudAllocators_[slot]))) ||
		    FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, hudAllocators_[slot].Get(), nullptr,
		                                      IID_PPV_ARGS(&hudLists_[slot]))))
		{
			hudLists_[slot].Reset();
			return false;
		}
		d3d::Original().close(hudLists_[slot].Get());
		hudLists_[slot]->SetName(L"BlockRender HUD");
	}
	void* mapped = nullptr;
	if (FAILED(hudTimings_->Map(0, nullptr, &mapped)))
	{
		return false;
	}
	hudTimings_->SetName(L"BlockRender HUD timings");
	hudTimingsMapped_ = static_cast<uint64_t*>(mapped);  // last: mapped timings mean everything is ready
	return true;
}

bool Renderer::EnsureHudPipeline(DXGI_FORMAT aFormat)
{
	if (hudPipeline_ && hudPipelineFormat_ == aFormat)
	{
		return true;
	}
	if (!shaders_)
	{
		return false;
	}
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.VS = { shaders_->hudVertex.data(), shaders_->hudVertex.size() };
	desc.PS = { shaders_->hud.data(), shaders_->hud.size() };
	D3D12_RENDER_TARGET_BLEND_DESC& blend = desc.BlendState.RenderTarget[0];
	blend.BlendEnable = TRUE;  // premultiplied alpha, as SkyCraft composites the image
	blend.SrcBlend = D3D12_BLEND_ONE;
	blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blend.BlendOp = D3D12_BLEND_OP_ADD;
	blend.SrcBlendAlpha = D3D12_BLEND_ONE;
	blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blend.LogicOp = D3D12_LOGIC_OP_NOOP;
	blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = BlockRasterizer(0);  // one triangle: nothing to cull
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = aFormat;
	desc.SampleDesc.Count = 1;
	gpu::ComPtr<ID3D12PipelineState> pipeline;
	if (FAILED(device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline))))
	{
		return false;
	}
	pipeline->SetName(L"BlockRender HUD pipeline");
	hudPipeline_ = pipeline;
	hudPipelineFormat_ = aFormat;
	return true;
}

void Renderer::HudSkip(const std::string& aWhy)
{
	++stats_.hudSkipped;
	stats_.hudLastSkip = aWhy;
}

void Renderer::ReadHudTimings(uint32_t aSlot)
{
	const uint64_t begin = hudTimingsMapped_[aSlot * 2];
	const uint64_t end = hudTimingsMapped_[aSlot * 2 + 1];
	if (end > begin && timestampFrequency_ > 0)
	{
		hudGpuMsSum_ += double(end - begin) * 1000.0 / double(timestampFrequency_);
		++hudGpuSamples_;
	}
}

void Renderer::PresentHud(IDXGISwapChain* aSwapChain)
{
	if (!EnsureObjects() || !EnsureHudObjects())
	{
		return HudSkip("not ready: the main queue isn't known yet");
	}
	const uint64_t completed = fence_->GetCompletedValue();
	if (completed == UINT64_MAX)
	{
		return HudSkip("the device was removed");
	}
	if (!OnMainQueue(aSwapChain))
	{
		return HudSkip("the swap chain presents on another queue than the main one");
	}
	gpu::ComPtr<IDXGISwapChain3> swapChain;
	gpu::ComPtr<ID3D12Resource>  backBuffer;
	if (FAILED(aSwapChain->QueryInterface(IID_PPV_ARGS(&swapChain))) ||
	    FAILED(swapChain->GetBuffer(swapChain->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&backBuffer))))
	{
		return HudSkip("no back buffer");
	}
	const D3D12_RESOURCE_DESC target = backBuffer->GetDesc();
	if (!EnsureHudPipeline(target.Format))
	{
		return HudSkip(std::format("no HUD pipeline for {}", DxgiFormatName(target.Format)));
	}
	const uint32_t slot = uint32_t(hudRecordings_ % kSlots);
	if (completed < hudSlotFence_[slot])
	{
		return HudSkip("the GPU is still busy with an earlier frame");
	}
	if (hudSlotFence_[slot] != 0)
	{
		ReadHudTimings(slot);
	}
	const std::optional<uint32_t> taken = hudSlots_.TakeNewest();
	if (!taken && !hudImageReadable_)
	{
		return HudSkip("no image yet");
	}
	uint64_t copyBytes = 0;
	if (taken)
	{
		const HudImage& image = hudSlotImages_[*taken];
		copyBytes = uint64_t(image.width) * image.height * 4;
		if (hudImageBytes_ < copyBytes)
		{
			if (hudImage_)
			{
				Retire(hudImage_);
			}
			hudImage_ = gpu::MakeBuffer(device_, copyBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
			hudImageBytes_ = hudImage_ ? copyBytes : 0;
			hudImageReadable_ = false;
			if (!hudImage_)
			{
				hudSlots_.Reading(*taken, 0);  // never read: free again
				return HudSkip("couldn't create the HUD image's buffer");
			}
			hudImage_->SetName(L"BlockRender HUD image");
		}
	}

	// Our own list: calls the hooks watch go through their originals, so nothing of ours reaches the tracker.
	const bool                 wasReadable = hudImageReadable_;  // as the buffer is now, grown or not
	const HudImage             wasShown = hudShown_;
	const d3d::Originals&      original = d3d::Original();
	ID3D12GraphicsCommandList* list = hudLists_[slot].Get();
	if (FAILED(hudAllocators_[slot]->Reset()) || FAILED(original.reset(list, hudAllocators_[slot].Get(), hudPipeline_.Get())))
	{
		if (taken)
		{
			hudSlots_.Reading(*taken, 0);
		}
		return HudSkip("couldn't reset its command list");
	}
	list->EndQuery(hudQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2);
	if (taken)
	{
		const D3D12_RESOURCE_STATES  before = hudImageReadable_ ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON;
		const D3D12_RESOURCE_BARRIER toCopy = Transition(hudImage_.Get(), before, D3D12_RESOURCE_STATE_COPY_DEST);
		original.resourceBarrier(list, 1, &toCopy);
		list->CopyBufferRegion(hudImage_.Get(), 0, hudUploads_[*taken].Get(), 0, copyBytes);
		const D3D12_RESOURCE_BARRIER toRead =
		    Transition(hudImage_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		original.resourceBarrier(list, 1, &toRead);
		hudImageReadable_ = true;
		hudShown_ = hudSlotImages_[*taken];
	}
	const D3D12_RESOURCE_BARRIER toTarget = Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	original.resourceBarrier(list, 1, &toTarget);
	const D3D12_CPU_DESCRIPTOR_HANDLE rtv = Offset(rtvHeap_->GetCPUDescriptorHandleForHeapStart(), kBackBufferRtv, rtvIncrement_);
	original.createRtv(device_, backBuffer.Get(), nullptr, rtv);
	original.omSetRenderTargets(list, 1, &rtv, FALSE, nullptr);
	const D3D12_VIEWPORT viewport{ 0.0f, 0.0f, float(target.Width), float(target.Height), 0.0f, 1.0f };
	const D3D12_RECT     scissor{ 0, 0, LONG(target.Width), LONG(target.Height) };
	list->RSSetViewports(1, &viewport);
	list->RSSetScissorRects(1, &scissor);
	list->SetGraphicsRootSignature(rootSignature_.Get());
	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	list->SetGraphicsRootConstantBufferView(0, constants_->GetGPUVirtualAddress());  // b0: the HUD's shaders don't read it
	const uint32_t     scale = HudScaleFor(uint32_t(target.Width), target.Height, hudShown_.width, hudShown_.height);
	// Spike 8: the cursor's newest spot, as the input thread left it, so the arrow moves in the frame the mouse does.
	const HudCursor*      cursor = hudCursor_.load();
	const HudCursor::Spot spot = cursor ? cursor->Get() : HudCursor::Spot{};
	const HudConstants    hud{ hudShown_.width, hudShown_.height, scale, hudShown_.bottomUp ? 1u : 0u, spot.x, spot.y,
                            spot.shown ? CursorHeight(uint32_t(target.Height)) : 0u, 0u };
	list->SetGraphicsRoot32BitConstants(1, 8, &hud, 0);
	for (UINT parameter = 2; parameter < 6; ++parameter)  // t0 to t3: the image, which HudPS reads as t1
	{
		list->SetGraphicsRootShaderResourceView(parameter, hudImage_->GetGPUVirtualAddress());
	}
	original.drawInstanced(list, 3, 1, 0, 0);
	const D3D12_RESOURCE_BARRIER toPresent = Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	original.resourceBarrier(list, 1, &toPresent);
	list->EndQuery(hudQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2 + 1);
	list->ResolveQueryData(hudQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2, 2, hudTimings_.Get(), slot * 2 * sizeof(uint64_t));
	if (FAILED(original.close(list)))
	{
		hudImageReadable_ = wasReadable;  // nothing of the list ran
		hudShown_ = wasShown;
		if (taken)
		{
			hudSlots_.Reading(*taken, 0);
		}
		return HudSkip("couldn't close its command list");
	}
	ID3D12CommandList* lists[] = { list };
	original.executeCommandLists(queue_, 1, lists);
	const uint64_t fence = Signal();
	hudSlotFence_[slot] = fence;
	if (taken)
	{
		hudSlots_.Reading(*taken, fence);
	}
	++hudRecordings_;
	++stats_.hudDrawn;
}

void Renderer::Shutdown()
{
	mode_ = Mode::Off;
	std::lock_guard lock(mutex_);
	ReleaseTracking();
	if (fence_ && fence_->GetCompletedValue() < fenceValue_)
	{
		const HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (event && SUCCEEDED(fence_->SetEventOnCompletion(fenceValue_, event)))
		{
			WaitForSingleObject(event, 2000);
		}
		if (event)
		{
			CloseHandle(event);
		}
	}
	if (constantsMapped_)
	{
		constants_->Unmap(0, nullptr);
		constantsMapped_ = nullptr;
	}
	if (timingsMapped_)
	{
		timings_->Unmap(0, nullptr);
		timingsMapped_ = nullptr;
	}
	hudWanted_ = false;
	for (uint32_t slot = 0; slot < HudSlots::kSlots; ++slot)
	{
		if (hudMapped_[slot])
		{
			hudUploads_[slot]->Unmap(0, nullptr);
			hudMapped_[slot] = nullptr;
		}
		hudUploads_[slot].Reset();
		hudUploadBytes_[slot] = 0;
	}
	if (hudTimingsMapped_)
	{
		hudTimings_->Unmap(0, nullptr);
		hudTimingsMapped_ = nullptr;
	}
	hudImage_.Reset();
	hudPipeline_.Reset();
	hudLists_ = {};
	hudAllocators_ = {};
	hudQueries_.Reset();
	hudTimings_.Reset();
	sections_.clear();
	pending_.Clear();
	watch_.reset();
	for (SnapshotGpu& snapshot : snapshots_)
	{
		if (snapshot.mapped)
		{
			snapshot.buffer->Unmap(0, nullptr);
		}
		snapshot = {};
	}
	snapshotRing_ = Snapshots(kSnapshots);
	slotSnapshot_ = {};
	forward_.reset();
	forwardList_ = nullptr;
	forwardSlotSnapshot_ = {};
	if (forwardConstantsMapped_)
	{
		forwardConstants_->Unmap(0, nullptr);
		forwardConstantsMapped_ = nullptr;
	}
	if (forwardTimingsMapped_)
	{
		forwardTimings_->Unmap(0, nullptr);
		forwardTimingsMapped_ = nullptr;
	}
	forwardConstants_.Reset();
	forwardTimings_.Reset();
	forwardQueries_.Reset();
	forwardLists_ = {};
	forwardAllocators_ = {};
	translucentPipeline_.Reset();
	outlinePipeline_.Reset();
	hintPipeline_.Reset();
	blendedBlockPipeline_.Reset();
	pendingEntities_.reset();
	pendingTextures_.clear();
	textureReady_.clear();
	textures_.Clear();
	entityTextures_.Reset();
	entityTexels_ = 0;
	entityPipeline_.Reset();
	entityDepthPipeline_.Reset();
	crackPipeline_.Reset();
	if (entityTimingsMapped_)
	{
		entityTimings_->Unmap(0, nullptr);
		entityTimingsMapped_ = nullptr;
	}
	entityTimings_.Reset();
	entityQueries_.Reset();
	retired_.Clear();
	oldDepthCopies_.clear();
	lists_ = {};
	allocators_ = {};
	depthCopy_.Reset();
	atlasBuffer_.Reset();
	depthTarget_.Reset();
	pipeline_.Reset();
	predlssPipeline_.Reset();
	gbufferPipeline_.Reset();
	depthPipeline_.Reset();
	rootSignature_.Reset();
	rtvHeap_.Reset();
	dsvHeap_.Reset();
	constants_.Reset();
	timings_.Reset();
	queries_.Reset();
	uploadList_.Reset();
	uploadAllocator_.Reset();
	fence_.Reset();
}
}
