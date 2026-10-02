#pragma once

#include "Coords.h"
#include "LogQueue.h"
#include "hooks/HookEvents.h"
#include "render/Atlas.h"
#include "render/Camera.h"
#include "render/FrameConstants.h"
#include "render/Gbuffer.h"
#include "render/Gpu.h"
#include "render/PendingSections.h"
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
// The plugin's own drawing (the spec's "Renderer"). The modes:
// - off;
// - present (step 2): at each DLSS evaluate the tagged depth's depth plane is copied into a buffer of ours, inside the
//   game's list. Once that list is submitted, the next Present draws the blocks into the back buffer with our own list,
//   on the same queue, hidden where the copied depth says the game drew something nearer;
// - predlss (step 3): a list of ours goes into the submission holding the DLSS evaluate's list, just before it. It draws
//   into the tagged colour, motion vectors and depth, with the frame's jitter, so DLSS takes the blocks for the game's
//   own. Its barriers start from the state the DLSS list found each buffer in (its first transitions), so every
//   command list is tracked while this mode is on: the game rotates through too many DLSS lists to watch only those;
// - gbuffer (step 4): a list of ours goes into the submission holding the list that ends the G-buffer, just before it.
//   It draws into the G-buffer's base colour, normals, material values and motion vectors, testing and writing its
//   depth and stencil, then into the tagged depth, the copy of that depth which the game has made by then. The game
//   then lights, shadows and fogs the blocks as its own. Every list is tracked: the G-buffer's lists are known by their
//   targets, its end by its transitions, and the states our barriers start from by both.
class Renderer : public HookEvents
{
public:
	Renderer(LogQueue& aLog, std::filesystem::path aFolder);

	// Main thread.
	bool         SetMode(std::string_view aMode, std::string& aError);
	std::string  ModeName() const;
	bool         LoadAtlas(std::string& aError);  // atlas.bin, from the plugin's folder
	void         SetAtlas(Atlas aAtlas);          // Minecraft's, say: replaces any atlas, and is uploaded at the next Update
	// Null until loaded. Valid until the next LoadAtlas or SetAtlas, so fetch it again each time it's needed.
	const Atlas* AtlasData() const;
	bool         ReloadShaders(std::string& aError);
	// brightness, tolerance, cull, origin.x/y/z; for predlss, exposure, jittersign and mvsign; for gbuffer, stencil,
	// metalness, roughness, materialb, gbmotion and gbdepth
	bool         Set(std::string_view aName, double aValue, std::string& aError);
	void         UploadSection(const SectionPos& aSection, std::vector<Vertex> aVertices);  // no vertices: remove it
	void         ClearSections();  // every section, and every upload waiting
	void         Update();         // uploads, and frees what the GPU has finished with

	// Latency (the spec's "Measurements"): the next upload of aSection is timed, from its submission to the first list
	// recorded that draws it. It replaces any section still being timed.
	struct TimedUpload
	{
		double   submitted = 0.0;  // SteadySeconds()
		double   drawn = 0.0;
		uint64_t drawnFrame = 0;  // Presents() then
	};
	void                       TimeUpload(const SectionPos& aSection);
	std::optional<TimedUpload> TakeTimedUpload();  // once it has been drawn; then it's done
	uint64_t                   Presents() const { return presents_.load(); }  // the game's frames so far

	struct Stats
	{
		uint64_t    drawn = 0;
		uint64_t    skipped = 0;
		uint64_t    lateCamera = 0;  // present: drawn with the same DLSS frame as the draw before; gbuffer: skipped,
		                             // because the frame's camera came after its G-buffer
		uint64_t    partial = 0;     // gbuffer: drawn into the G-buffer only, as the tagged depth's state wasn't known
		std::string lastSkip;
		double      gpuMsAverage = -1.0;
		double      gpuMsWorst = -1.0;
		double      frameMsAverage = -1.0;
		double      frameMsWorst = -1.0;
		size_t      sections = 0;
		size_t      vertices = 0;
		double      updateMsAverage = -1.0;  // Update's CPU time
		double      updateMsWorst = -1.0;
		size_t      pending = 0;  // uploads waiting
		uint32_t    atlasWidth = 0;  // 0 before there's an atlas
		uint32_t    atlasHeight = 0;
		uint32_t    atlasLevels = 0;
	};
	Stats TakeStats();  // since the last call
	void  Shutdown();   // waits for the GPU, then releases everything

	void               OnEvaluate(const EvaluateCall& aCall) override;
	void               OnSubmit(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists) override;
	ID3D12CommandList* ListBefore(ID3D12CommandQueue* aQueue, ID3D12CommandList* aList) override;
	void               OnSubmitted(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists) override;
	void               OnPresent(IDXGISwapChain* aSwapChain) override;

private:
	enum class Mode
	{
		Off,
		Present,
		Predlss,
		Gbuffer,
	};

	static constexpr uint32_t kSlots = 8;  // frames in flight: more than Streamline's 6

	struct SectionGpu
	{
		gpu::ComPtr<ID3D12Resource> buffer;
		uint32_t                    vertices = 0;
		uint64_t                    ready = 0;  // the fence value its upload signals
	};

	struct Frame  // present: one DLSS evaluate's
	{
		const void* list = nullptr;  // the game's list holding the depth copy; compared, never used
		uint32_t    frame = 0;
		bool        valid = false;  // its constants were found
		SlConstants constants;
		bool        depth = false;  // the depth copy holds its depth
		uint32_t    rowFloats = 0;
		sl::Extent  extent{};
	};

	struct Evaluate  // predlss and gbuffer: one DLSS evaluate's
	{
		const void*                list = nullptr;  // the DLSS list; compared, never used
		std::optional<SlConstants> constants;
		SlTag                      colour;  // the scaling input colour
		SlTag                      motion;
		SlTag                      depth;
	};

	// gbuffer: the resources whose states the main queue's lists show, in watched_'s order
	enum Watched : size_t
	{
		kBase,
		kNormals,
		kMaterial,
		kGbufferDepth,
		kMotion,
		kTaggedDepth,
		kWatched,
	};

	struct GbufferFrame  // gbuffer: what this frame's submissions have shown so far
	{
		uint32_t                                     frame = 0;        // the frame it is
		bool                                         seen = false;     // a G-buffer list has been submitted
		bool                                         decided = false;  // drawn, or counted as skipped
		const void*                                  end = nullptr;    // the list that ends the G-buffer, once found
		std::array<std::optional<uint32_t>, kWatched> before{};        // each watched resource's state just before it
	};

	// All with the mutex held.
	void               Upload();         // Update's work
	bool               EnsureObjects();  // false until the main queue is known, or if creating them failed
	bool               EnsurePipeline(DXGI_FORMAT aFormat, bool aReversed);
	bool               EnsurePredlssPipeline(bool aReversed);
	bool               EnsureGbufferPipeline(bool aReversed);
	bool               EnsureDepthPipeline(bool aReversed);
	bool               EnsureDepthTarget(uint32_t aWidth, uint32_t aHeight);
	ID3D12CommandList* RecordPredlss(ID3D12CommandQueue* aQueue, ID3D12GraphicsCommandList* aDlssList);  // null: skipped
	void               SeeGbufferSubmission(UINT aCount, ID3D12CommandList* const* aLists);
	void               WatchGbuffer();  // the G-buffer's targets and the tagged buffers, if they changed
	ID3D12CommandList* RecordGbuffer(ID3D12CommandQueue* aQueue);  // null: skipped
	FrameConstants     BaseConstants(const CameraFrame& aCamera) const;  // the camera, the atlas and the tuning values
	// BaseConstants, plus the image's size, the jitter and the motion: predlss and gbuffer draw at the render size
	FrameConstants     JitteredConstants(const CameraFrame& aCamera, const SlConstants& aGame, const sl::Extent& aExtent) const;
	void               DrawSections(ID3D12GraphicsCommandList* aList, const CameraFrame& aCamera, uint64_t aCompleted);
	void               Skip(const std::string& aWhy, bool aCounts = true);  // aCounts: toward the skips in a row
	void               Fail(const std::string& aWhy);  // logs, and switches drawing off
	void               ReleaseTracking();               // if predlss or gbuffer asked for it
	uint64_t           Signal();
	void               Retire(gpu::ComPtr<ID3D12Resource> aResource);
	void               ReadTimings(uint32_t aSlot);

	LogQueue&             log_;
	std::filesystem::path folder_;
	mutable std::mutex    mutex_;
	std::atomic<Mode>     mode_{ Mode::Off };
	bool                  trackingRequested_ = false;
	uint32_t              trackedSince_ = 0;  // lists reset from this frame on are tracked

	float   brightness_ = 1.0f;
	float   tolerance_ = 0.0001f;
	int     cull_ = 1;  // 0 none, 1 back faces, 2 front faces
	Vec3d   renderOrigin_;
	float   exposure_ = 2.5f;      // predlss: what the linear colour is multiplied by; run 4 settled on 2.5 by eye
	float   jitterSign_ = 1.0f;    // predlss and gbuffer: 1 or -1 as the jitter's sign, 0 for none
	float   mvSign_ = 1.0f;        // predlss and gbuffer: 1 or -1 as the motion vectors' sign, 0 for none
	uint8_t stencil_ = 0;          // gbuffer: the stencil class; buildings carry 0
	float   metalness_ = 0.0f;     // gbuffer: the material values run 3's dump found for dirt
	float   roughness_ = 0.85f;
	float   materialB_ = 0.33f;
	bool    gbufferMotion_ = true;  // gbuffer: write the motion vectors, as the game wrote them before our list
	bool    gbufferDepth_ = true;   // gbuffer: write the tagged depth, which the game copied before our list

	ID3D12Device*                                              device_ = nullptr;  // the game's
	ID3D12CommandQueue*                                        queue_ = nullptr;   // the game's main queue
	gpu::ComPtr<ID3D12RootSignature>                           rootSignature_;
	std::optional<BlockShaders>                                shaders_;
	gpu::ComPtr<ID3D12PipelineState>                           pipeline_;
	DXGI_FORMAT                                                pipelineFormat_ = DXGI_FORMAT_UNKNOWN;
	int                                                        pipelineCull_ = -1;
	bool                                                       pipelineReversed_ = false;
	gpu::ComPtr<ID3D12PipelineState>                           predlssPipeline_;
	int                                                        predlssCull_ = -1;
	bool                                                       predlssReversed_ = false;
	gpu::ComPtr<ID3D12PipelineState>                           gbufferPipeline_;
	int                                                        gbufferCull_ = -1;
	bool                                                       gbufferReversed_ = false;
	bool                                                       gbufferMotionWrites_ = false;
	gpu::ComPtr<ID3D12PipelineState>                           depthPipeline_;
	int                                                        depthCull_ = -1;
	bool                                                       depthReversed_ = false;
	// RTVs: the back buffer; predlss's colour and motion vectors; gbuffer's base colour, normals, material values and
	// motion vectors. DSVs: our depth buffer; predlss's depth; gbuffer's G-buffer depth and tagged depth.
	gpu::ComPtr<ID3D12DescriptorHeap>                          rtvHeap_;
	gpu::ComPtr<ID3D12DescriptorHeap>                          dsvHeap_;
	uint32_t                                                   rtvIncrement_ = 0;
	uint32_t                                                   dsvIncrement_ = 0;
	gpu::ComPtr<ID3D12Resource>                                depthTarget_;
	uint32_t                                                   depthWidth_ = 0;
	uint32_t                                                   depthHeight_ = 0;
	gpu::ComPtr<ID3D12Resource>                                constants_;  // upload: kSlots x kConstantsStride bytes
	uint8_t*                                                   constantsMapped_ = nullptr;
	std::array<gpu::ComPtr<ID3D12CommandAllocator>, kSlots>    allocators_;
	std::array<gpu::ComPtr<ID3D12GraphicsCommandList>, kSlots> lists_;
	std::array<uint64_t, kSlots>                               slotFence_{};
	uint64_t                                                   recordings_ = 0;  // lists of ours recorded to draw
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
	PendingSections                                                pending_;
	std::vector<std::pair<gpu::ComPtr<ID3D12Resource>, uint64_t>> retired_;

	struct UploadWatch  // the section TimeUpload times
	{
		SectionPos            section;
		uint64_t              ready = 0;  // its upload's fence value, once submitted
		std::optional<double> submitted;
		std::optional<double> drawn;
		uint64_t              drawnFrame = 0;
	};
	std::optional<UploadWatch> watch_;
	std::atomic<uint64_t>      presents_{ 0 };

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

	Evaluate                 predlss_;  // predlss: the latest evaluate, until its list is submitted
	std::atomic<const void*> predlssList_{ nullptr };

	Evaluate                      latest_;                   // gbuffer: the latest evaluate
	bool                          latestSubmitted_ = false;  // its DLSS list has been submitted: the next is late
	uint64_t                      renderWidth_ = 0;          // the tagged depth's size, the G-buffer's too
	uint32_t                      renderHeight_ = 0;
	std::optional<GbufferTargets> gbuffer_;  // as last seen
	std::vector<void*>            watched_;  // kWatched resources once the G-buffer has been seen; empty until then
	KnownStates                   knownStates_;
	GbufferFrame                  gbufferFrame_;
	std::atomic<const void*>      gbufferEnd_{ nullptr };  // gbufferFrame_.end, read without the lock

	Stats    stats_;
	double   gpuMsSum_ = 0.0;
	uint64_t gpuSamples_ = 0;
	double   frameMsSum_ = 0.0;
	uint64_t frameSamples_ = 0;
	int64_t  lastPresent_ = 0;
	double   updateMsSum_ = 0.0;
	uint64_t updateSamples_ = 0;
};
}
