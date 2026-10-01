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
// buffers are copied at the DLSS evaluate (depth, mvec, color), and the G-buffer candidates as their passes end
// (gbuffer). The game records its G-buffer pass over many lists, so each set of candidate targets keeps one copy,
// which every pass with those targets records again: the last in submission order wins, which is the whole G-buffer.
// The copies go into readback buffers inside the game's own lists. Once a fence says the GPU has run them, the main
// thread writes each as a .dds file, with dump.json saying what each one is.
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
		uint32_t                     subresource = 0;
		uint32_t                     format = 0;  // the format it's saved as
		uint32_t                     width = 0;
		uint32_t                     height = 0;
		int                          recordings = 0;
		std::set<ID3D12CommandList*> unsubmitted;  // game lists holding a recording of it, not yet submitted
		ID3D12CommandQueue*          queue = nullptr;  // where its latest recording was submitted
		bool                         signalDue = false;  // submitted since the fence was last signalled for it
		uint64_t                     fence = 0;  // signalled after its latest submission
	};

	// One set of G-buffer candidate targets and depth, by resource, and the copies of each resource.
	struct Group
	{
		std::vector<void*>               resources;  // the render targets, then the depth buffer
		std::vector<std::vector<size_t>> copies;     // per resource, its planes' copies (indices into copies_)
		int                              passes = 0;
	};

	// Records copies of every plane of aResource (or of plane aOnlyPlane) into aList, into new readback buffers named
	// aName, and returns their indices in copies_. Mutex held.
	std::vector<size_t> CopyResource(ID3D12GraphicsCommandList* aList, ID3D12Resource* aResource, uint32_t aState,
	                                 const std::string& aName, const std::string& aNote, int aOnlyPlane = -1);
	// Records aCopy's copy of aResource again, into aList. Mutex held.
	void CopyAgain(ID3D12GraphicsCommandList* aList, ID3D12Resource* aResource, uint32_t aState, Copy& aCopy);

	LogQueue&                             log_;
	std::filesystem::path                 folder_;
	mutable std::mutex                    mutex_;
	Phase                                 phase_ = Phase::Idle;
	std::set<std::string>                 what_;
	uint32_t                              frame_ = 0;
	uint64_t                              renderWidth_ = 0;  // the tagged depth's size
	uint32_t                              renderHeight_ = 0;
	std::vector<Group>                    groups_;
	std::vector<Copy>                     copies_;
	gpu::ComPtr<ID3D12Fence>              fence_;
	uint64_t                              fenceValue_ = 0;
	std::vector<std::string>              errors_;
	std::chrono::steady_clock::time_point waitingSince_{};
};
}
