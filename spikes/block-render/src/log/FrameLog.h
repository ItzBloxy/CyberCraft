#pragma once

#include "LogQueue.h"
#include "hooks/HookEvents.h"
#include "log/FrameLogData.h"
#include "track/GpuApi.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace cpmc
{
// The frame log (the spec's step 1). `log` arms it. From the next frame token it records two whole frames: what
// Streamline is told, every submission and present, and every tracked list's passes. It then waits one more frame
// for lists still closing, and hands the result to the main thread.
class FrameLog : public HookEvents
{
public:
	explicit FrameLog(LogQueue& aLog);

	// Main thread.
	bool                        Arm();  // false if a log is already running
	bool                        Capturing() const;
	std::string                 State() const;  // for the status file
	void                        NoteScriptCamera(const LogScriptCamera& aCamera);
	std::optional<FrameLogData> TakeFinished(const std::string& aVersion, const std::string& aTime);

	void OnFrameToken(uint32_t aFrame) override;
	void OnConstants(uint32_t aFrame, uint32_t aViewport, const SlConstants& aConstants) override;
	void OnTags(uint32_t aFrame, uint32_t aViewport, const std::vector<SlTag>& aTags, void* aList) override;
	void OnEvaluate(const EvaluateCall& aCall) override;
	void OnClose(ID3D12GraphicsCommandList* aList, ListState& aState) override;
	void OnSubmit(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists) override;
	void OnPresent(IDXGISwapChain* aSwapChain) override;

private:
	enum class Phase
	{
		Idle,
		Armed,      // waiting for the next frame token
		Capturing,  // two frames
		Draining,   // one more frame: lists still closing are kept
		Done,
	};

	bool   Recording() const;  // mutex held
	double Ms() const;
	void   Event(std::string aWhat, std::string aDetail);  // mutex held

	LogQueue&                          log_;
	mutable std::mutex                 mutex_;
	Phase                              phase_ = Phase::Idle;
	uint32_t                           tokens_ = 0;
	int64_t                            start_ = 0;
	FrameLogData                       data_;
	std::unique_ptr<gpuapi::ListNames> names_;
};

// What a resource is, from ID3D12Resource::GetDesc. Unknown for null.
ResourceInfo DescribeResource(void* aResource);
}
