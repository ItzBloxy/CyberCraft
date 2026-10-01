#pragma once

#include "LogQueue.h"
#include "hooks/HookEvents.h"
#include "render/Gpu.h"

#include <chrono>
#include <filesystem>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace cpmc
{
// Dumps (the spec's "FrameLog and dumps"). `dump <what>` arms one frame: from the next frame token, the tagged
// buffers are copied at the DLSS evaluate (depth, mvec, color) and the G-buffer candidates when their pass ends
// (gbuffer, at most 4 passes). The copies go into readback buffers inside the game's own lists; once a fence says the
// GPU has run them, the main thread writes each as a .dds file, with dump.json saying what each one is.
class Dumps : public HookEvents
{
public:
	Dumps(LogQueue& aLog, std::filesystem::path aFolder);

	// Main thread.
	bool        Arm(std::string_view aWhat, std::string& aError);
	std::string State() const;
	void        Update(const std::string& aFolderName);  // writes the copies the GPU has finished

	void OnFrameToken(uint32_t aFrame) override;
	void OnTags(uint32_t aFrame, uint32_t aViewport, const std::vector<SlTag>& aTags, void* aList) override;
	void OnEvaluate(const EvaluateCall& aCall) override;
	void OnPassEnd(ID3D12GraphicsCommandList* aList, ListState& aState, bool aClosing) override;
	void OnSubmit(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists) override;
	void OnSubmitted(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists) override;

private:
	enum class Phase
	{
		Idle,
		Armed,     // waiting for the next frame token
		Copying,   // one frame
		Waiting,   // for the GPU
	};

	struct Copy
	{
		std::string                  name;
		std::string                  note;  // what it is, for dump.json
		gpu::ComPtr<ID3D12Resource>  readback;
		gpu::Footprint               footprint;
		uint32_t                     format = 0;  // the format it's saved as
		uint32_t                     width = 0;
		uint32_t                     height = 0;
		ID3D12GraphicsCommandList*   list = nullptr;  // the game list it was recorded into
		ID3D12CommandQueue*          queue = nullptr;
		uint64_t                     fence = 0;  // 0 until the list is submitted and signalled
	};

	// Records copies of every plane of aResource (or of plane aOnlyPlane) into aList. Mutex held.
	void CopyResource(ID3D12GraphicsCommandList* aList, ID3D12Resource* aResource, uint32_t aState, const std::string& aName,
	                  const std::string& aNote, int aOnlyPlane = -1);

	LogQueue&                         log_;
	std::filesystem::path             folder_;
	mutable std::mutex                mutex_;
	Phase                             phase_ = Phase::Idle;
	std::set<std::string>             what_;
	uint32_t                          frame_ = 0;
	void*                             taggedDepth_ = nullptr;
	int                               candidates_ = 0;
	std::vector<Copy>                 copies_;
	std::set<ID3D12CommandList*>      lists_;  // game lists holding copies, not yet submitted
	gpu::ComPtr<ID3D12Fence>          fence_;
	uint64_t                          fenceValue_ = 0;
	std::vector<std::string>          errors_;
	std::chrono::steady_clock::time_point waitingSince_{};
};
}
