#include "log/Dumps.h"

#include "Json.h"
#include "hooks/D3D12Hooks.h"
#include "log/DdsWriter.h"
#include "log/FrameLogData.h"

#include <algorithm>
#include <format>
#include <fstream>

namespace cpmc
{
namespace
{
constexpr size_t kMaxGroups = 4;

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
	groups_.clear();
	copies_.clear();
	errors_.clear();
	phase_ = Phase::Armed;
	d3d::RequestTracking();
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
	for (const SlTag& tag : aTags)
	{
		if (tag.type == sl::kBufferTypeDepth && tag.resource)
		{
			// Kept from every frame: a frame's tags come after the G-buffer passes they give the size of.
			const D3D12_RESOURCE_DESC desc = static_cast<ID3D12Resource*>(tag.resource)->GetDesc();
			std::lock_guard lock(mutex_);
			renderWidth_ = desc.Width;
			renderHeight_ = desc.Height;
		}
	}
}

std::vector<size_t> Dumps::CopyResource(ID3D12GraphicsCommandList* aList, ID3D12Resource* aResource, uint32_t aState,
                                        const std::string& aName, const std::string& aNote, int aOnlyPlane)
{
	std::vector<size_t>       made;
	gpu::ComPtr<ID3D12Device> owner;
	if (!aResource || FAILED(aResource->GetDevice(IID_PPV_ARGS(&owner))))
	{
		return made;
	}
	ID3D12Device* const       device = owner.Get();
	const D3D12_RESOURCE_DESC desc = aResource->GetDesc();
	if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
	{
		errors_.push_back(aName + ": not a 2D texture");
		return made;
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
		copy.subresource = gpu::PlaneSubresource(desc, plane);
		copy.footprint = gpu::FootprintOf(device, desc, copy.subresource);
		copy.readback = gpu::MakeBuffer(device, copy.footprint.totalBytes, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
		if (!copy.readback)
		{
			errors_.push_back(aName + ": couldn't create a readback buffer");
			continue;
		}
		copy.format = format;
		copy.width = copy.footprint.layout.Footprint.Width;
		copy.height = copy.footprint.layout.Footprint.Height;
		copies_.push_back(std::move(copy));
		made.push_back(copies_.size() - 1);
		CopyAgain(aList, aResource, aState, copies_.back());
	}
	return made;
}

void Dumps::CopyAgain(ID3D12GraphicsCommandList* aList, ID3D12Resource* aResource, uint32_t aState, Copy& aCopy)
{
	if (!gpu::RecordCopy(aList, aResource, aCopy.subresource, aState, aCopy.readback.Get(), aCopy.footprint))
	{
		errors_.push_back(std::format("{}: its list isn't a direct list, which can't move it out of state 0x{:x}", aCopy.name, aState));
		return;
	}
	++aCopy.recordings;
	aCopy.unsubmitted.insert(aList);
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
	const Targets& targets = aState.CurrentTargets();
	if (phase_ != Phase::Copying || !what_.contains("gbuffer") || !IsGbufferCandidate(targets, renderWidth_, renderHeight_))
	{
		return;
	}
	std::vector<void*> resources;
	for (const BoundTarget& target : targets.rtvs)
	{
		resources.push_back(target.view->resource);
	}
	resources.push_back(targets.dsv->view->resource);
	auto group = std::find_if(groups_.begin(), groups_.end(), [&](const Group& aGroup) { return aGroup.resources == resources; });
	if (group == groups_.end())
	{
		if (groups_.size() >= kMaxGroups)
		{
			return;
		}
		groups_.push_back({ resources, std::vector<std::vector<size_t>>(resources.size()), 0 });
		group = groups_.end() - 1;
	}
	const size_t index = size_t(group - groups_.begin());
	++group->passes;
	for (size_t i = 0; i < resources.size(); ++i)
	{
		const bool      isDepth = i == targets.rtvs.size();
		const ViewInfo& view = isDepth ? *targets.dsv->view : *targets.rtvs[i].view;
		auto*           resource = static_cast<ID3D12Resource*>(view.resource);
		const auto      known = aState.StateOf(resource);
		if (isDepth && !known && view.readOnlyDepth)
		{
			continue;  // a read-only depth in an unknown state: its earlier copy stands
		}
		const uint32_t state = known.value_or(isDepth ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET);
		if (group->copies[i].empty())
		{
			const std::string name = isDepth ? std::format("gbuffer{}-depth", index) : std::format("gbuffer{}-target{}", index, i);
			const std::string note = std::format("G-buffer candidate {}, {} ({}), first copied in list 0x{:x}, frame {}, state 0x{:x}",
			                                     index, isDepth ? std::string("depth") : std::format("target {}", i),
			                                     DxgiFormatName(view.viewFormat), reinterpret_cast<uintptr_t>(aList), frame_, state);
			group->copies[i] = CopyResource(aList, resource, state, name, note, isDepth ? -1 : int(view.plane));
		}
		else
		{
			for (const size_t copy : group->copies[i])
			{
				CopyAgain(aList, resource, state, copies_[copy]);
			}
		}
	}
}

void Dumps::OnSubmit(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists)
{
	std::lock_guard lock(mutex_);
	if (phase_ != Phase::Copying && phase_ != Phase::Waiting)
	{
		return;
	}
	for (Copy& copy : copies_)
	{
		for (UINT i = 0; i < aCount; ++i)
		{
			if (copy.unsubmitted.erase(aLists[i]) > 0)
			{
				copy.queue = aQueue;
				copy.signalDue = true;
			}
		}
	}
}

void Dumps::OnSubmitted(ID3D12CommandQueue* aQueue, UINT, ID3D12CommandList* const*)
{
	std::lock_guard lock(mutex_);
	bool            due = false;
	for (const Copy& copy : copies_)
	{
		due = due || (copy.signalDue && copy.queue == aQueue);
	}
	if (!due)
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
		if (copy.signalDue && copy.queue == aQueue)
		{
			copy.fence = fenceValue_;
			copy.signalDue = false;
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
	const auto     done = [&](const Copy& aCopy) { return aCopy.unsubmitted.empty() && aCopy.fence != 0 && completed >= aCopy.fence; };
	for (const Copy& copy : copies_)
	{
		if (copy.recordings > 0 && !done(copy) && !timedOut)
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
		if (copy.recordings == 0)
		{
			continue;  // errors_ says why
		}
		if (!done(copy))
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
		json.Key("recordings").UInt(uint64_t(copy.recordings));
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
	groups_.clear();
	copies_.clear();
	errors_.clear();
	phase_ = Phase::Idle;
	d3d::ReleaseTracking();
}
}
