#include "log/FrameLog.h"

#include "hooks/D3D12Hooks.h"
#include "hooks/StreamlineHooks.h"

#include <Windows.h>

#include <format>

namespace cpmc
{
namespace
{
int64_t Now()
{
	LARGE_INTEGER now{};
	QueryPerformanceCounter(&now);
	return now.QuadPart;
}

std::string Hex(const void* aPointer)
{
	return std::format("0x{:x}", reinterpret_cast<uintptr_t>(aPointer));
}

const char* TypeName(D3D12_COMMAND_LIST_TYPE aType)
{
	switch (aType)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT: return "direct";
	case D3D12_COMMAND_LIST_TYPE_COMPUTE: return "compute";
	case D3D12_COMMAND_LIST_TYPE_COPY: return "copy";
	default: return "other";
	}
}

std::string TagNames(const std::vector<SlTag>& aTags)
{
	std::string names;
	for (const SlTag& tag : aTags)
	{
		names += (names.empty() ? "" : ", ") + BufferTypeName(tag.type) + (tag.local ? " (local)" : "");
	}
	return names;
}
}

ResourceInfo DescribeResource(void* aResource)
{
	ResourceInfo info;
	if (!aResource)
	{
		return info;
	}
	const D3D12_RESOURCE_DESC desc = static_cast<ID3D12Resource*>(aResource)->GetDesc();
	info.known = true;
	info.dimension = desc.Dimension;
	info.format = desc.Format;
	info.width = desc.Width;
	info.height = desc.Height;
	info.depthOrArraySize = desc.DepthOrArraySize;
	info.mips = desc.MipLevels;
	info.flags = desc.Flags;
	return info;
}

FrameLog::FrameLog(LogQueue& aLog)
	: log_(aLog)
{
}

bool FrameLog::Arm()
{
	std::lock_guard lock(mutex_);
	if (phase_ != Phase::Idle)
	{
		return false;
	}
	data_ = {};
	tokens_ = 0;
	names_ = std::make_unique<gpuapi::ListNames>();
	phase_ = Phase::Armed;
	d3d::RequestTracking();
	return true;
}

bool FrameLog::Capturing() const
{
	std::lock_guard lock(mutex_);
	return phase_ == Phase::Capturing || phase_ == Phase::Draining;
}

std::string FrameLog::State() const
{
	std::lock_guard lock(mutex_);
	switch (phase_)
	{
	case Phase::Idle: return "idle";
	case Phase::Armed: return "armed, waiting for Streamline's next frame";
	case Phase::Capturing: return "capturing";
	case Phase::Draining: return "finishing";
	case Phase::Done: return "done, being written";
	}
	return "";
}

void FrameLog::NoteScriptCamera(const LogScriptCamera& aCamera)
{
	std::lock_guard lock(mutex_);
	if (phase_ == Phase::Capturing)
	{
		data_.scriptCamera = aCamera;
	}
}

bool FrameLog::Recording() const
{
	return phase_ == Phase::Capturing || phase_ == Phase::Draining;
}

double FrameLog::Ms() const
{
	LARGE_INTEGER frequency{};
	QueryPerformanceFrequency(&frequency);
	return double(Now() - start_) * 1000.0 / double(frequency.QuadPart);
}

void FrameLog::Event(std::string aWhat, std::string aDetail)
{
	data_.events.push_back({ Ms(), d3d::CurrentFrame(), GetCurrentThreadId(), std::move(aWhat), std::move(aDetail) });
}

void FrameLog::OnFrameToken(uint32_t aFrame)
{
	std::lock_guard lock(mutex_);
	switch (phase_)
	{
	case Phase::Armed:
		phase_ = Phase::Capturing;
		start_ = Now();
		data_.firstFrame = aFrame;
		d3d::SetLogging(true);
		Event("frameToken", std::format("frame {}: the log begins", aFrame));
		break;
	case Phase::Capturing:
		Event("frameToken", std::format("frame {}", aFrame));
		if (++tokens_ == 2)
		{
			phase_ = Phase::Draining;
			data_.lastFrame = aFrame - 1;
			d3d::SetLogging(false);
		}
		break;
	case Phase::Draining:
		Event("frameToken", std::format("frame {}: the log ends", aFrame));
		phase_ = Phase::Done;
		break;
	default: break;
	}
}

void FrameLog::OnConstants(uint32_t aFrame, uint32_t aViewport, const SlConstants& aConstants)
{
	std::lock_guard lock(mutex_);
	if (Recording())
	{
		data_.constants.push_back({ aFrame, aViewport, aConstants });
		Event("setConstants", std::format("frame {}, viewport {}", aFrame, aViewport));
	}
}

void FrameLog::OnTags(uint32_t aFrame, uint32_t aViewport, const std::vector<SlTag>& aTags, void* aList)
{
	std::lock_guard lock(mutex_);
	if (!Recording())
	{
		return;
	}
	for (const SlTag& tag : aTags)
	{
		data_.tags.push_back({ aFrame, aViewport, tag, DescribeResource(tag.resource), reinterpret_cast<uintptr_t>(aList) });
	}
	Event("setTag", std::format("viewport {}: {}", aViewport, TagNames(aTags)));
}

void FrameLog::OnEvaluate(const EvaluateCall& aCall)
{
	std::lock_guard lock(mutex_);
	if (!Recording())
	{
		return;
	}
	data_.evaluates.push_back({ aCall.frame, sl::kFeatureDLSS, aCall.viewport, reinterpret_cast<uintptr_t>(aCall.gameList),
	                            reinterpret_cast<uintptr_t>(aCall.list) });
	for (const SlTag& tag : aCall.tags)
	{
		if (tag.local)
		{
			data_.tags.push_back({ aCall.frame, aCall.viewport, tag, DescribeResource(tag.resource), reinterpret_cast<uintptr_t>(aCall.list) });
		}
	}
	Event("evaluate", std::format("DLSS, frame {}, viewport {}, list {} (native {}, {}), constants {}", aCall.frame, aCall.viewport,
	                              Hex(aCall.gameList), Hex(aCall.list), aCall.list ? TypeName(aCall.list->GetType()) : "unknown",
	                              aCall.constants ? "found" : "missing"));
}

void FrameLog::OnClose(ID3D12GraphicsCommandList* aList, ListState& aState)
{
	if (!aState.Logging())
	{
		return;
	}
	std::lock_guard lock(mutex_);
	if (Recording())
	{
		data_.lists.push_back({ reinterpret_cast<uintptr_t>(aList), names_ ? names_->Find(aList) : std::string(), aState.Frame(),
		                        aState.Passes(), TransitionsOf(aState) });
	}
}

void FrameLog::OnSubmit(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists)
{
	std::lock_guard lock(mutex_);
	if (!Recording())
	{
		return;
	}
	LogSubmission submission;
	submission.ms = Ms();
	submission.frame = d3d::CurrentFrame();
	submission.queue = reinterpret_cast<uintptr_t>(aQueue);
	for (UINT i = 0; i < aCount; ++i)
	{
		submission.lists.push_back(reinterpret_cast<uintptr_t>(aLists[i]));
		if (!d3d::Tracker().Find(aLists[i]))
		{
			++submission.untracked;
		}
	}
	Event("submit", std::format("queue {} ({}{}), {} lists", Hex(aQueue), TypeName(aQueue->GetDesc().Type),
	                            aQueue == d3d::MainQueue() ? ", the main queue" : "", aCount));
	data_.submissions.push_back(std::move(submission));
}

void FrameLog::OnPresent(IDXGISwapChain* aSwapChain)
{
	std::lock_guard lock(mutex_);
	if (Recording())
	{
		const ID3D12CommandQueue* queue = d3d::PresentQueue(aSwapChain);
		Event("present", std::format("swap chain {}, on queue {}{}", Hex(aSwapChain), Hex(queue),
		                             queue && queue == d3d::MainQueue() ? " (the main queue)" : ""));
	}
}

std::optional<FrameLogData> FrameLog::TakeFinished(const std::string& aVersion, const std::string& aTime)
{
	std::lock_guard lock(mutex_);
	if (phase_ != Phase::Done)
	{
		return std::nullopt;
	}
	FrameLogData data = std::move(data_);
	data_ = {};
	names_.reset();
	phase_ = Phase::Idle;
	d3d::ReleaseTracking();

	data.pluginVersion = aVersion;
	data.time = aTime;
	LogDevice& device = data.device;
	device.seenAtCreation = d3d::SeenAtCreation();
	device.device = reinterpret_cast<uintptr_t>(d3d::Device());
	const gpuapi::Report report = gpuapi::Check(d3d::Device());
	device.gpuApiDevice = report.device;
	device.gpuApiMatches = report.deviceMatches;
	device.gpuApiQueue = report.queue;
	device.gpuApiSwapChain = report.swapChain;
	device.streamline = data.evaluates.empty() ? "unknown" : data.evaluates[0].list == data.evaluates[0].nativeList ? "manual" : "proxies";
	device.rtvs = d3d::Views().Count(ViewInfo::Kind::Rtv);
	device.dsvs = d3d::Views().Count(ViewInfo::Kind::Dsv);
	device.vtableEntries = d3d::PatchedEntries();
	device.imports = 4;

	// The G-buffer candidates, at the render size: the log's latest depth tag's.
	uint64_t width = 0;
	uint32_t height = 0;
	for (const LogTag& tag : data.tags)
	{
		if (tag.tag.type == sl::kBufferTypeDepth && tag.resource.known)
		{
			width = tag.resource.width;
			height = tag.resource.height;
		}
	}
	for (const LogList& list : data.lists)
	{
		for (size_t pass = 0; pass < list.passes.size(); ++pass)
		{
			const Targets& targets = list.passes[pass].targets;
			for (const BoundTarget& target : targets.rtvs)
			{
				device.unknownHandles += target.view ? 0 : 1;
			}
			device.unknownHandles += targets.dsv && !targets.dsv->view ? 1 : 0;
			if (IsGbufferCandidate(targets, width, height))
			{
				data.gbufferCandidates.push_back({ list.list, pass });
			}
		}
	}
	return data;
}
}
