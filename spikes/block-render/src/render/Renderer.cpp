#include "render/Renderer.h"

#include "hooks/D3D12Hooks.h"
#include "log/DdsWriter.h"
#include "log/FrameLogData.h"
#include "render/FrameConstants.h"
#include "render/Pipeline.h"

#include <dxgi1_4.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>

namespace cpmc
{
namespace
{
constexpr uint64_t kConstantsStride = 256;  // a constant buffer view's alignment
constexpr uint32_t kSkipsBeforeOff = 30;     // the spec's "Checks before every draw"

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
}

Renderer::Renderer(LogQueue& aLog, std::filesystem::path aFolder)
	: log_(aLog)
	, folder_(std::move(aFolder))
{
}

bool Renderer::SetMode(std::string_view aMode, std::string& aError)
{
	if (aMode == "present")
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
	else if (aMode != "off")
	{
		aError = std::format("mode {} isn't in this build: it comes with steps 3 and 4", aMode);
		return false;
	}
	std::lock_guard lock(mutex_);
	recorded_ = {};
	recordedList_ = nullptr;
	submitted_ = {};
	lastDrawnFrame_.reset();
	consecutiveSkips_ = 0;
	mode_ = aMode == "present" ? Mode::Present : Mode::Off;
	return true;
}

std::string Renderer::ModeName() const
{
	return mode_.load() == Mode::Present ? "present" : "off";
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
	return true;
}

bool Renderer::Set(std::string_view aName, double aValue, std::string& aError)
{
	std::lock_guard lock(mutex_);
	if (aName == "brightness" || aName == "tolerance")
	{
		if (aValue < 0.0)
		{
			aError = std::format("{} can't be negative", aName);
			return false;
		}
		(aName == "brightness" ? brightness_ : tolerance_) = float(aValue);
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
	aError = std::format("there's no setting called \"{}\"", aName);
	return false;
}

void Renderer::UploadSection(const SectionPos& aSection, std::vector<Vertex> aVertices)
{
	std::lock_guard lock(mutex_);
	pending_.emplace_back(aSection, std::move(aVertices));
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
	const D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{ D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
	const D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{ D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
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
		lists_[slot]->SetName(L"BlockRender present");
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
	d3d::Original().createDsv(device_, depthTarget_.Get(), nullptr, dsvHeap_->GetCPUDescriptorHandleForHeapStart());
	depthWidth_ = aWidth;
	depthHeight_ = aHeight;
	return true;
}

void Renderer::Skip(const std::string& aWhy)
{
	++stats_.skipped;
	stats_.lastSkip = aWhy;
	if (++consecutiveSkips_ == kSkipsBeforeOff)
	{
		Fail(std::format("{} frames in a row were skipped ({})", kSkipsBeforeOff, aWhy));
	}
}

void Renderer::Fail(const std::string& aWhy)
{
	log_.Push("renderer: " + aWhy + ", so drawing is off");
	stats_.lastSkip = aWhy;
	mode_ = Mode::Off;
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
	std::lock_guard lock(mutex_);
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
	if ((pending_.empty() && !atlasDue) || completed < uploadFence_)
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
	for (auto& [section, vertices] : pending_)
	{
		if (const auto found = sections_.find(section); found != sections_.end())
		{
			Retire(found->second.buffer);
			sections_.erase(found);
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
	pending_.clear();
	original.close(uploadList_.Get());
	ID3D12CommandList* lists[] = { uploadList_.Get() };
	original.executeCommandLists(queue_, 1, lists);
	uploadFence_ = Signal();
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
	stats.sections = sections_.size();
	for (const auto& [section, gpuSection] : sections_)
	{
		stats.vertices += gpuSection.vertices;
	}
	stats_ = {};
	gpuMsSum_ = frameMsSum_ = 0.0;
	gpuSamples_ = frameSamples_ = 0;
	return stats;
}

void Renderer::OnEvaluate(const EvaluateCall& aCall)
{
	if (mode_.load() != Mode::Present)
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
	const bool wholeResource = depth->extent.width == 0 || depth->extent.height == 0;
	recorded_.extent = wholeResource ? sl::Extent{ 0, 0, uint32_t(desc.Width), desc.Height } : depth->extent;
}

void Renderer::OnSubmit(ID3D12CommandQueue*, UINT aCount, ID3D12CommandList* const* aLists)
{
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

void Renderer::OnPresent(IDXGISwapChain* aSwapChain)
{
	const int64_t   now = Now();
	std::lock_guard lock(mutex_);
	if (lastPresent_ != 0)
	{
		const double ms = MsBetween(lastPresent_, now);
		frameMsSum_ += ms;
		++frameSamples_;
		stats_.frameMsWorst = std::max(stats_.frameMsWorst, ms);
	}
	lastPresent_ = now;
	if (mode_.load() != Mode::Present)
	{
		return;
	}
	if (!EnsureObjects())
	{
		return Skip("not ready: the main queue isn't known yet");
	}
	const uint64_t completed = fence_->GetCompletedValue();
	if (completed == UINT64_MAX)
	{
		return Fail(std::format("the device was removed (0x{:08x})", uint32_t(device_->GetDeviceRemovedReason())));
	}
	if (aSwapChain != checkedSwapChain_)
	{
		checkedSwapChain_ = aSwapChain;
		swapChainOnMainQueue_ = d3d::PresentQueue(aSwapChain) == queue_;
		log_.Push(std::format("renderer: swap chain 0x{:x} presents on {}", reinterpret_cast<uintptr_t>(aSwapChain),
		                      swapChainOnMainQueue_ ? "the main queue" : "another queue"));
	}
	if (!swapChainOnMainQueue_)
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
	const uint32_t slot = uint32_t(presents_ % kSlots);
	if (completed < slotFence_[slot])
	{
		return Skip("the GPU is still busy with an earlier frame");
	}
	if (slotFence_[slot] != 0)
	{
		ReadTimings(slot);
	}

	FrameConstants constants{};
	for (size_t i = 0; i < 16; ++i)
	{
		constants.cameraToClip[i] = float(camera->cameraToClip[i]);
	}
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
	constants.atlasInfo[0] = atlas_->width;
	constants.atlasInfo[1] = atlas_->height;
	constants.atlasInfo[2] = atlas_->Levels();
	for (size_t i = 0; i < atlas_->levelOffsets.size() && i < 8; ++i)
	{
		constants.atlasLevels[i] = atlas_->levelOffsets[i];
	}
	constants.tuning[0] = brightness_;
	constants.tuning[1] = tolerance_;
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
	const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
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
	for (const auto& [section, gpuSection] : sections_)
	{
		if (gpuSection.ready > completed)
		{
			continue;  // still uploading
		}
		const Vec3d corner = WorldFromMc({ section.x * 16.0, section.y * 16.0, section.z * 16.0 });
		const Vec3d offset{ corner.x - camera->origin.x, corner.y - camera->origin.y, corner.z - camera->origin.z };
		// The section fills world x..x+16, y-16..y and z..z+16 from that corner, since Minecraft z runs along world -y.
		if (!Visible(camera->cameraToClip, { offset.x, offset.y - 16.0, offset.z }, { offset.x + 16.0, offset.y, offset.z + 16.0 }))
		{
			continue;
		}
		const float sectionConstants[4] = { float(offset.x), float(offset.y), float(offset.z), 0.0f };
		list->SetGraphicsRoot32BitConstants(1, 4, sectionConstants, 0);
		list->SetGraphicsRootShaderResourceView(2, gpuSection.buffer->GetGPUVirtualAddress());
		original.drawInstanced(list, gpuSection.vertices, 1, 0, 0);
	}
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
	++presents_;
	++stats_.drawn;
	if (lastDrawnFrame_ == submitted_.frame)
	{
		++stats_.lateCamera;
	}
	lastDrawnFrame_ = submitted_.frame;
	consecutiveSkips_ = 0;
}

void Renderer::Shutdown()
{
	mode_ = Mode::Off;
	std::lock_guard lock(mutex_);
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
	sections_.clear();
	pending_.clear();
	retired_.clear();
	oldDepthCopies_.clear();
	lists_ = {};
	allocators_ = {};
	depthCopy_.Reset();
	atlasBuffer_.Reset();
	depthTarget_.Reset();
	pipeline_.Reset();
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
