#pragma once

#include "Coords.h"
#include "LogQueue.h"
#include "hooks/HookEvents.h"
#include "render/Atlas.h"
#include "render/Camera.h"
#include "render/Gpu.h"
#include "render/Shaders.h"
#include "world/BlockWorld.h"
#include "world/Vertex.h"

#include <array>
#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cpmc
{
// The plugin's own drawing (the spec's "Renderer"). This build has the modes off and present (step 2): at each DLSS
// evaluate the tagged depth's depth plane is copied into a buffer of ours, inside the game's list. Once that list is
// submitted, the next Present draws the blocks into the back buffer with our own list, on the same queue, hidden
// where the copied depth says the game drew something nearer.
class Renderer : public HookEvents
{
public:
	Renderer(LogQueue& aLog, std::filesystem::path aFolder);

	// Main thread.
	bool         SetMode(std::string_view aMode, std::string& aError);
	std::string  ModeName() const;
	bool         LoadAtlas(std::string& aError);  // atlas.bin, from the plugin's folder
	const Atlas* AtlasData() const;               // null until loaded; it stays valid, since the atlas is loaded once
	bool         ReloadShaders(std::string& aError);
	bool         Set(std::string_view aName, double aValue, std::string& aError);  // brightness, tolerance, cull, origin.x/y/z
	void         UploadSection(const SectionPos& aSection, std::vector<Vertex> aVertices);  // no vertices: remove it
	void         Update();  // uploads, and frees what the GPU has finished with

	struct Stats
	{
		uint64_t    drawn = 0;
		uint64_t    skipped = 0;
		uint64_t    lateCamera = 0;  // drawn with the same DLSS frame as the draw before
		std::string lastSkip;
		double      gpuMsAverage = -1.0;
		double      gpuMsWorst = -1.0;
		double      frameMsAverage = -1.0;
		double      frameMsWorst = -1.0;
		size_t      sections = 0;
		size_t      vertices = 0;
	};
	Stats TakeStats();  // since the last call
	void  Shutdown();   // waits for the GPU, then releases everything

	void OnEvaluate(const EvaluateCall& aCall) override;
	void OnSubmit(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists) override;
	void OnPresent(IDXGISwapChain* aSwapChain) override;

private:
	enum class Mode
	{
		Off,
		Present,
	};

	static constexpr uint32_t kSlots = 8;  // frames in flight: more than Streamline's 6

	struct SectionGpu
	{
		gpu::ComPtr<ID3D12Resource> buffer;
		uint32_t                    vertices = 0;
		uint64_t                    ready = 0;  // the fence value its upload signals
	};

	struct Frame  // one DLSS evaluate's
	{
		const void* list = nullptr;  // the game's list holding the depth copy; compared, never used
		uint32_t    frame = 0;
		bool        valid = false;  // its constants were found
		SlConstants constants;
		bool        depth = false;  // the depth copy holds its depth
		uint32_t    rowFloats = 0;
		sl::Extent  extent{};
	};

	// All with the mutex held.
	bool     EnsureObjects();  // false until the main queue is known, or if creating them failed
	bool     EnsurePipeline(DXGI_FORMAT aFormat, bool aReversed);
	bool     EnsureDepthTarget(uint32_t aWidth, uint32_t aHeight);
	void     Skip(const std::string& aWhy);
	void     Fail(const std::string& aWhy);  // logs, and switches drawing off
	uint64_t Signal();
	void     Retire(gpu::ComPtr<ID3D12Resource> aResource);
	void     ReadTimings(uint32_t aSlot);

	LogQueue&             log_;
	std::filesystem::path folder_;
	mutable std::mutex    mutex_;
	std::atomic<Mode>     mode_{ Mode::Off };

	float brightness_ = 1.0f;
	float tolerance_ = 0.0001f;
	int   cull_ = 1;  // 0 none, 1 back faces, 2 front faces
	Vec3d renderOrigin_;

	ID3D12Device*                                              device_ = nullptr;  // the game's
	ID3D12CommandQueue*                                        queue_ = nullptr;   // the game's main queue
	gpu::ComPtr<ID3D12RootSignature>                           rootSignature_;
	std::optional<BlockShaders>                                shaders_;
	gpu::ComPtr<ID3D12PipelineState>                           pipeline_;
	DXGI_FORMAT                                                pipelineFormat_ = DXGI_FORMAT_UNKNOWN;
	int                                                        pipelineCull_ = -1;
	bool                                                       pipelineReversed_ = false;
	gpu::ComPtr<ID3D12DescriptorHeap>                          rtvHeap_;
	gpu::ComPtr<ID3D12DescriptorHeap>                          dsvHeap_;
	gpu::ComPtr<ID3D12Resource>                                depthTarget_;
	uint32_t                                                   depthWidth_ = 0;
	uint32_t                                                   depthHeight_ = 0;
	gpu::ComPtr<ID3D12Resource>                                constants_;  // upload: kSlots x 256 bytes
	uint8_t*                                                   constantsMapped_ = nullptr;
	std::array<gpu::ComPtr<ID3D12CommandAllocator>, kSlots>    allocators_;
	std::array<gpu::ComPtr<ID3D12GraphicsCommandList>, kSlots> lists_;
	std::array<uint64_t, kSlots>                               slotFence_{};
	uint64_t                                                   presents_ = 0;
	gpu::ComPtr<ID3D12CommandAllocator>                        uploadAllocator_;
	gpu::ComPtr<ID3D12GraphicsCommandList>                     uploadList_;
	uint64_t                                                   uploadFence_ = 0;
	gpu::ComPtr<ID3D12Fence>                                   fence_;
	uint64_t                                                   fenceValue_ = 0;
	gpu::ComPtr<ID3D12QueryHeap>                               queries_;
	gpu::ComPtr<ID3D12Resource>                                timings_;  // readback: kSlots x 2 timestamps
	uint64_t*                                                  timingsMapped_ = nullptr;
	uint64_t                                                   timestampFrequency_ = 0;

	std::optional<Atlas>                                           atlas_;
	gpu::ComPtr<ID3D12Resource>                                    atlasBuffer_;
	uint64_t                                                       atlasReady_ = 0;
	bool                                                           atlasUploaded_ = false;
	std::map<SectionPos, SectionGpu>                               sections_;
	std::vector<std::pair<SectionPos, std::vector<Vertex>>>        pending_;
	std::vector<std::pair<gpu::ComPtr<ID3D12Resource>, uint64_t>> retired_;

	gpu::ComPtr<ID3D12Resource>              depthCopy_;
	uint64_t                                 depthCopyBytes_ = 0;
	std::vector<gpu::ComPtr<ID3D12Resource>> oldDepthCopies_;  // a game list not yet submitted may still use them
	Frame                                    recorded_;        // the latest evaluate, its list not yet submitted
	std::atomic<const void*>                 recordedList_{ nullptr };
	Frame                                    submitted_;       // the latest evaluate whose list was submitted
	std::optional<uint32_t>                  lastDrawnFrame_;
	const void*                              checkedSwapChain_ = nullptr;  // compared, never used
	bool                                     swapChainOnMainQueue_ = false;
	uint32_t                                 consecutiveSkips_ = 0;

	Stats    stats_;
	double   gpuMsSum_ = 0.0;
	uint64_t gpuSamples_ = 0;
	double   frameMsSum_ = 0.0;
	uint64_t frameSamples_ = 0;
	int64_t  lastPresent_ = 0;
};
}
