#include "render/Renderer.h"

#include "Measurements.h"
#include "hooks/D3D12Hooks.h"
#include "log/DdsWriter.h"
#include "log/FrameLogData.h"
#include "render/Pipeline.h"
#include "render/Predlss.h"

#include <dxgi1_4.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>

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
constexpr uint32_t kRtvs = 7;
constexpr uint32_t kOwnDepthDsv = 0;
constexpr uint32_t kPredlssDsv = 1;
constexpr uint32_t kGbufferDsv = 2;
constexpr uint32_t kTaggedDepthDsv = 3;
constexpr uint32_t kDsvs = 4;

// The G-buffer's render targets in order, as step 1 found them (MainGbuffer checks the game's views have them).
constexpr DXGI_FORMAT kGbufferFormats[4] = { DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM,
	                                         DXGI_FORMAT_R16G16_FLOAT };

// The slot of the list ListBefore handed to this thread's ExecuteCommandLists call, for OnSubmitted to signal: the
// same thread makes both calls, while the main thread may change the mode in between.
thread_local std::optional<uint32_t> t_insertedSlot;

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
	log_.Push(std::format("atlas: {} x {}, {} levels, {} blocks", atlas_->width, atlas_->height, atlas_->Levels(), atlas_->blocks.size()));
	return true;
}

void Renderer::SetAtlas(Atlas aAtlas)
{
	std::lock_guard lock(mutex_);
	atlas_ = std::move(aAtlas);
	atlasUploaded_ = false;
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
	D3D12_ROOT_PARAMETER parameters[5]{};
	parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;  // b0: the frame
	parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	parameters[1].Constants = { 1, 0, 4 };  // b1: the section
	for (UINT i = 2; i < 5; ++i)            // t0 vertices, t1 atlas, t2 depth copy
	{
		parameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		parameters[i].Descriptor = { i - 2, 0 };
	}
	const D3D12_ROOT_SIGNATURE_DESC rootDesc{ 5, parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE };
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
	if (FAILED(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_))) ||
	    FAILED(device_->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap_))) ||
	    FAILED(device_->CreateQueryHeap(&queryDesc, IID_PPV_ARGS(&queries_))) || !constants_ || !timings_ ||
	    FAILED(constants_->Map(0, nullptr, reinterpret_cast<void**>(&constantsMapped_))) ||
	    FAILED(timings_->Map(0, nullptr, reinterpret_cast<void**>(&timingsMapped_))) ||
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
	// Every list of ours that used it has been submitted, and the fence was signalled after each.
	retired_.emplace_back(std::move(aResource), fenceValue_);
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
}

void Renderer::Update()
{
	const auto      start = std::chrono::steady_clock::now();
	std::lock_guard lock(mutex_);
	Upload();
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
	std::erase_if(retired_, [completed](const auto& aEntry) { return aEntry.second <= completed; });
	const bool atlasDue = atlas_ && !atlasUploaded_;
	if ((pending_.Size() == 0 && !atlasDue) || completed < uploadFence_)
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
	const auto upload = [&](const void* aData, uint64_t aBytes) -> gpu::ComPtr<ID3D12Resource> {
		auto  target = gpu::MakeBuffer(device_, aBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
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
		atlasBuffer_ = upload(atlas_->texels.data(), atlas_->texels.size() * sizeof(uint32_t));
		if (atlasBuffer_)
		{
			atlasBuffer_->SetName(L"BlockRender atlas");
		}
		atlasReady_ = ready;
		atlasUploaded_ = true;
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
		if (vertices.empty())
		{
			continue;
		}
		SectionGpu gpuSection;
		gpuSection.buffer = upload(vertices.data(), vertices.size() * sizeof(Vertex));
		gpuSection.vertices = uint32_t(vertices.size());
		gpuSection.ready = ready;
		if (gpuSection.buffer)
		{
			gpuSection.buffer->SetName(L"BlockRender section");
			sections_[section] = std::move(gpuSection);
		}
	}
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
		retired_.emplace_back(std::move(buffer), uploadFence_);
	}
	if (failed)
	{
		log_.Push("renderer: couldn't create some upload buffers; those sections aren't drawn");
	}
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
	}
	stats_ = {};
	gpuMsSum_ = frameMsSum_ = updateMsSum_ = hudCopyMsSum_ = hudGpuMsSum_ = 0.0;
	gpuSamples_ = frameSamples_ = updateSamples_ = hudCopySamples_ = hudGpuSamples_ = 0;
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
	return constants;
}

void Renderer::DrawSections(ID3D12GraphicsCommandList* aList, const CameraFrame& aCamera, uint64_t aCompleted)
{
	for (const auto& [section, gpuSection] : sections_)
	{
		if (gpuSection.ready > aCompleted)
		{
			continue;  // still uploading
		}
		const Vec3d corner = WorldFromMc({ section.x * 16.0, section.y * 16.0, section.z * 16.0 });
		const Vec3d offset{ corner.x - aCamera.origin.x, corner.y - aCamera.origin.y, corner.z - aCamera.origin.z };
		// The section fills world x..x+16, y-16..y and z..z+16 from that corner, since Minecraft z runs along world -y.
		if (!Visible(aCamera.cameraToClip, { offset.x, offset.y - 16.0, offset.z }, { offset.x + 16.0, offset.y, offset.z + 16.0 }))
		{
			continue;
		}
		if (watch_ && watch_->submitted && !watch_->drawn && watch_->section == section && watch_->ready == gpuSection.ready)
		{
			watch_->drawn = SteadySeconds();  // the first list that draws the timed upload
			watch_->drawnFrame = presents_.load();
		}
		const float sectionConstants[4] = { float(offset.x), float(offset.y), float(offset.z), 0.0f };
		aList->SetGraphicsRoot32BitConstants(1, 4, sectionConstants, 0);
		aList->SetGraphicsRootShaderResourceView(2, gpuSection.buffer->GetGPUVirtualAddress());
		d3d::Original().drawInstanced(aList, gpuSection.vertices, 1, 0, 0);
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
	DrawSections(list, *camera, completed);
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
	DrawSections(list, *camera, completed);
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
		DrawSections(list, *camera, completed);
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
		Fail("couldn't close a command list");
		return nullptr;
	}
	++recordings_;
	if (gbufferDepth_ && !intoTaggedDepth)
	{
		++stats_.partial;
	}
	t_insertedSlot = slot;
	return list;
}

void Renderer::OnSubmitted(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*)
{
	if (!t_insertedSlot)
	{
		return;
	}
	const uint32_t  slot = *t_insertedSlot;
	t_insertedSlot.reset();
	std::lock_guard lock(mutex_);
	slotFence_[slot] = Signal();  // signalled whatever the mode is now: our list went to the GPU
	++stats_.drawn;
	consecutiveSkips_ = 0;
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
	DrawSections(list, *camera, completed);
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
	const HudConstants hud{ hudShown_.width, hudShown_.height, scale, hudShown_.bottomUp ? 1u : 0u };
	list->SetGraphicsRoot32BitConstants(1, 4, &hud, 0);
	for (UINT parameter = 2; parameter < 5; ++parameter)  // t0 to t2: the image, which HudPS reads as t1
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
	retired_.clear();
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
