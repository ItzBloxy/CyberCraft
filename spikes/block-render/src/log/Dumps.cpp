#include "log/Dumps.h"

#include "Json.h"
#include "hooks/D3D12Hooks.h"
#include "log/DdsWriter.h"
#include "log/FrameLogData.h"

#include <format>
#include <fstream>

namespace cpmc
{
namespace
{
constexpr int kMaxCandidates = 4;

bool WriteFile(const std::filesystem::path& aPath, const void* aData, size_t aSize)
{
	std::ofstream out(aPath, std::ios::binary);
	out.write(static_cast<const char*>(aData), std::streamsize(aSize));
	return bool(out);
}
}

Dumps::Dumps(LogQueue& aLog, std::filesystem::path aFolder)
	: log_(aLog)
	, folder_(std::move(aFolder))
{
}

bool Dumps::Arm(std::string_view aWhat, std::string& aError)
{
	std::lock_guard lock(mutex_);
	if (phase_ != Phase::Idle)
	{
		aError = "a dump is already under way";
		return false;
	}
	what_.clear();
	if (aWhat == "all")
	{
		what_ = { "depth", "mvec", "color", "gbuffer" };
	}
	else
	{
		what_.insert(std::string(aWhat));
	}
	copies_.clear();
	lists_.clear();
	errors_.clear();
	candidates_ = 0;
	phase_ = Phase::Armed;
	d3d::EnableTracking();
	return true;
}

std::string Dumps::State() const
{
	std::lock_guard lock(mutex_);
	switch (phase_)
	{
	case Phase::Idle: return "idle";
	case Phase::Armed: return "armed";
	case Phase::Copying: return "copying";
	case Phase::Waiting: return std::format("waiting for the GPU ({} copies)", copies_.size());
	}
	return "";
}

void Dumps::OnFrameToken(uint32_t aFrame)
{
	std::lock_guard lock(mutex_);
	if (phase_ == Phase::Armed)
	{
		phase_ = Phase::Copying;
		frame_ = aFrame;
	}
	else if (phase_ == Phase::Copying)
	{
		phase_ = Phase::Waiting;
		waitingSince_ = std::chrono::steady_clock::now();
	}
}

void Dumps::OnTags(uint32_t, uint32_t, const std::vector<SlTag>& aTags, void*)
{
	std::lock_guard lock(mutex_);
	for (const SlTag& tag : aTags)
	{
		if (tag.type == sl::kBufferTypeDepth && tag.resource)
		{
			taggedDepth_ = tag.resource;
		}
	}
}

void Dumps::CopyResource(ID3D12GraphicsCommandList* aList, ID3D12Resource* aResource, uint32_t aState, const std::string& aName,
                         const std::string& aNote, int aOnlyPlane)
{
	gpu::ComPtr<ID3D12Device> owner;
	if (!aResource || FAILED(aResource->GetDevice(IID_PPV_ARGS(&owner))))
	{
		return;
	}
	ID3D12Device* const       device = owner.Get();
	const D3D12_RESOURCE_DESC desc = aResource->GetDesc();
	if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
	{
		errors_.push_back(aName + ": not a 2D texture");
		return;
	}
	const uint32_t planes = gpu::PlaneCount(device, desc.Format);
	for (uint32_t plane = 0; plane < planes; ++plane)
	{
		if (aOnlyPlane >= 0 && plane != uint32_t(aOnlyPlane))
		{
			continue;
		}
		const uint32_t format = DumpFormat(desc.Format, plane);
		if (BytesPerTexel(format) == 0)
		{
			errors_.push_back(std::format("{}: format {} can't be saved", aName, DxgiFormatName(desc.Format)));
			continue;
		}
		Copy copy;
		copy.name = planes > 1 ? std::format("{}-plane{}", aName, plane) : aName;
		copy.note = aNote;
		copy.footprint = gpu::FootprintOf(device, desc, gpu::PlaneSubresource(desc, plane));
		copy.readback = gpu::MakeBuffer(device, copy.footprint.totalBytes, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
		if (!copy.readback)
		{
			errors_.push_back(aName + ": couldn't create a readback buffer");
			continue;
		}
		copy.format = format;
		copy.width = copy.footprint.layout.Footprint.Width;
		copy.height = copy.footprint.layout.Footprint.Height;
		copy.list = aList;
		if (!gpu::RecordCopy(aList, aResource, gpu::PlaneSubresource(desc, plane), aState, copy.readback.Get(), copy.footprint))
		{
			errors_.push_back(std::format("{}: its list isn't a direct list, which can't move it out of state 0x{:x}", copy.name, aState));
			continue;
		}
		copies_.push_back(std::move(copy));
	}
	lists_.insert(aList);
}

void Dumps::OnEvaluate(const EvaluateCall& aCall)
{
	std::lock_guard lock(mutex_);
	if (phase_ != Phase::Copying || !aCall.list)
	{
		return;
	}
	const std::pair<uint32_t, const char*> wanted[] = { { sl::kBufferTypeDepth, "depth" },
		                                                { sl::kBufferTypeMotionVectors, "mvec" },
		                                                { sl::kBufferTypeScalingInputColor, "color" } };
	for (const auto& [type, what] : wanted)
	{
		if (!what_.contains(what))
		{
			continue;
		}
		for (const SlTag& tag : aCall.tags)
		{
			if (tag.type != type || !tag.resource)
			{
				continue;
			}
			if (tag.state == 0xFFFFFFFF)
			{
				errors_.push_back(BufferTypeName(type) + ": the tag gives no state");
				continue;
			}
			const std::string note = std::format("tag {} at the DLSS evaluate, frame {}, state 0x{:x}, extent {} {} {} {}",
			                                     BufferTypeName(type), aCall.frame, tag.state, tag.extent.top, tag.extent.left,
			                                     tag.extent.width, tag.extent.height);
			CopyResource(aCall.list, static_cast<ID3D12Resource*>(tag.resource), tag.state, "tag-" + BufferTypeName(type), note);
		}
	}
}

void Dumps::OnPassEnd(ID3D12GraphicsCommandList* aList, ListState& aState, bool)
{
	std::lock_guard lock(mutex_);
	if (phase_ != Phase::Copying || !what_.contains("gbuffer") || candidates_ >= kMaxCandidates ||
	    !IsGbufferCandidate(aState.CurrentTargets(), taggedDepth_))
	{
		return;
	}
	const int      index = candidates_++;
	const Targets& targets = aState.CurrentTargets();
	for (size_t i = 0; i < targets.rtvs.size(); ++i)
	{
		const ViewInfo& view = *targets.rtvs[i].view;
		auto*           resource = static_cast<ID3D12Resource*>(view.resource);
		const uint32_t  state = aState.StateOf(resource).value_or(D3D12_RESOURCE_STATE_RENDER_TARGET);
		const std::string note = std::format("G-buffer candidate {}, target {} ({}), list 0x{:x}, frame {}, state 0x{:x}", index, i,
		                                     DxgiFormatName(view.viewFormat), reinterpret_cast<uintptr_t>(aList), frame_, state);
		CopyResource(aList, resource, state, std::format("gbuffer{}-target{}", index, i), note, int(view.plane));
	}
	const ViewInfo& depth = *targets.dsv->view;
	const auto      known = aState.StateOf(depth.resource);
	if (!known && depth.readOnlyDepth)
	{
		errors_.push_back(std::format("gbuffer{}-depth: read-only and its state is unknown, so it isn't copied", index));
		return;
	}
	const uint32_t    state = known.value_or(D3D12_RESOURCE_STATE_DEPTH_WRITE);
	const std::string note = std::format("G-buffer candidate {}, depth ({}), list 0x{:x}, frame {}, state 0x{:x}", index,
	                                     DxgiFormatName(depth.viewFormat), reinterpret_cast<uintptr_t>(aList), frame_, state);
	CopyResource(aList, static_cast<ID3D12Resource*>(depth.resource), state, std::format("gbuffer{}-depth", index), note);
}

void Dumps::OnSubmit(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists)
{
	std::lock_guard lock(mutex_);
	if (lists_.empty())
	{
		return;
	}
	for (UINT i = 0; i < aCount; ++i)
	{
		if (lists_.erase(aLists[i]) == 0)
		{
			continue;
		}
		for (Copy& copy : copies_)
		{
			if (copy.list == aLists[i])
			{
				copy.queue = aQueue;
			}
		}
	}
}

void Dumps::OnSubmitted(ID3D12CommandQueue* aQueue, UINT, ID3D12CommandList* const*)
{
	std::lock_guard lock(mutex_);
	bool            signal = false;
	for (const Copy& copy : copies_)
	{
		signal = signal || (copy.queue == aQueue && copy.fence == 0);
	}
	if (!signal)
	{
		return;
	}
	gpu::ComPtr<ID3D12Device> device;
	if (!fence_ && (FAILED(aQueue->GetDevice(IID_PPV_ARGS(&device))) ||
	                FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)))))
	{
		errors_.push_back("couldn't create a fence");
		return;
	}
	aQueue->Signal(fence_.Get(), ++fenceValue_);
	for (Copy& copy : copies_)
	{
		if (copy.queue == aQueue && copy.fence == 0)
		{
			copy.fence = fenceValue_;
		}
	}
}

void Dumps::Update(const std::string& aFolderName)
{
	std::lock_guard lock(mutex_);
	if (phase_ != Phase::Waiting)
	{
		return;
	}
	// Wait until the GPU has run every copy, or 10 seconds: a list the game never submitted won't finish.
	const uint64_t completed = fence_ ? fence_->GetCompletedValue() : 0;
	const bool     timedOut = std::chrono::steady_clock::now() - waitingSince_ > std::chrono::seconds(10);
	for (const Copy& copy : copies_)
	{
		if ((copy.fence == 0 || completed < copy.fence) && !timedOut)
		{
			return;
		}
	}
	const std::filesystem::path folder = folder_ / "dumps" / aFolderName;
	std::error_code             error;
	std::filesystem::create_directories(folder, error);
	JsonWriter json;
	json.BeginObject().Key("frame").UInt(frame_).Key("files").BeginArray();
	size_t written = 0;
	for (Copy& copy : copies_)
	{
		if (copy.fence == 0 || completed < copy.fence)
		{
			errors_.push_back(copy.name + ": the GPU never ran its copy");
			continue;
		}
		void*             mapped = nullptr;
		const D3D12_RANGE range{ 0, size_t(copy.footprint.totalBytes) };
		if (FAILED(copy.readback->Map(0, &range, &mapped)))
		{
			errors_.push_back(copy.name + ": couldn't map its readback buffer");
			continue;
		}
		const auto* base = static_cast<const uint8_t*>(mapped) + copy.footprint.layout.Offset;
		const auto  dds = MakeDds(copy.format, copy.width, copy.height, base, copy.footprint.layout.Footprint.RowPitch);
		const D3D12_RANGE nothing{ 0, 0 };
		copy.readback->Unmap(0, &nothing);
		if (!WriteFile(folder / (copy.name + ".dds"), dds.data(), dds.size()))
		{
			errors_.push_back(copy.name + ": couldn't write the file");
			continue;
		}
		++written;
		json.BeginObject();
		json.Key("file").String(copy.name + ".dds");
		json.Key("format").String(DxgiFormatName(copy.format));
		json.Key("size").BeginArray().UInt(copy.width).UInt(copy.height).EndArray();
		json.Key("note").String(copy.note);
		json.EndObject();
	}
	json.EndArray().Key("errors").BeginArray();
	for (const std::string& line : errors_)
	{
		json.String(line);
	}
	json.EndArray().EndObject();
	const std::string text = json.Str() + "\n";
	WriteFile(folder / "dump.json", text.data(), text.size());
	log_.Push(std::format("dump: {} files written to {}{}", written, folder.string(),
	                      errors_.empty() ? "" : std::format(", {} problems (see dump.json)", errors_.size())));
	copies_.clear();
	errors_.clear();
	phase_ = Phase::Idle;
}
}
