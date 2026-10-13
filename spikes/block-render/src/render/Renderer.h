#pragma once

#include "Coords.h"
#include "LogQueue.h"
#include "entities/Entities.h"
#include "hooks/HookEvents.h"
#include "render/Atlas.h"
#include "render/Camera.h"
#include "render/EntityTextures.h"
#include "render/FrameConstants.h"
#include "render/Gbuffer.h"
#include "render/Gpu.h"
#include "render/Hud.h"
#include "render/PendingSections.h"
#include "render/Retired.h"
#include "render/SectionParts.h"
#include "render/Shaders.h"
#include "render/Snapshots.h"
#include "world/BlockWorld.h"
#include "world/Vertex.h"

#include <array>
#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <span>
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
//   targets, its end by its transitions, and the states our barriers start from by both. Spike 7 draws Minecraft's
//   entities in the same list, after the blocks: the newest snapshot's models, then their cracks, then their depth.
//   Its step B puts a second list of ours, the forward list, just before that frame's DLSS list, with the same
//   snapshot, camera and jitter: the translucent particles, Part 5's blended and additive entity layers and the outline,
//   blended into the tagged colour, then DLSS's bias hint under the particles and the layers, each tested against the
//   tagged depth. The polish pass splits each section into parts (SectionParts): plants go into the G-buffer with the
//   game's own class for small plants, whose lighting doesn't shadow them dark; and translucent blocks are blended in
//   the forward list, sections farthest first, before the particles.
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
	// metalness, roughness, materialb, gbmotion and gbdepth; spike 7's entities, glow and glowlight, step B's dlsshint
	// and outline, Part 5's blend and animate, and the polish pass's plants, plantstencil and plantb
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

	// The HUD (the spec's "The HUD"), from the main thread. SetHud says whether the HUD pass draws at Present, in any
	// drawing mode; the first time it's wanted, the shaders are compiled if drawing hasn't. SetHudImage copies an image
	// (RGBA8, premultiplied alpha) into an upload buffer the GPU has finished with, for the next pass: false, and
	// counted as dropped, if none is free, or before the renderer has made its objects.
	void    SetHud(bool aWanted);
	bool    SetHudImage(uint32_t aWidth, uint32_t aHeight, bool aBottomUp, const uint8_t* aRgba);
	HudSize BackBufferSize() const { return { backWidth_.load(), backHeight_.load() }; }  // as the last Present saw it
	// Spike 8: where the HUD pass finds the cursor, which it draws over the image while it's shown; null for none.
	void SetHudCursor(const HudCursor* aCursor) { hudCursor_.store(aCursor); }

	// Spike 7: Minecraft's entities (the spec's "The renderer"), from the main thread. AddEntityTexture copies a texture
	// and places it in the one buffer at once, for the next Update to upload: past the 64 MB cap it's left out, logged
	// once. ClearEntityTextures drops them all, at Minecraft's clear: Unlink, the Clear request and drawing switched off
	// keep them, since SkyCraft sends each once. SetEntities takes this frame's geometry, which the next Update copies
	// into a free snapshot for the next G-buffer list to draw.
	void     AddEntityTexture(uint32_t aId, uint32_t aWidth, uint32_t aHeight, std::span<const uint8_t> aRgba);
	void     ClearEntityTextures();
	void     SetEntities(entities::EntityFrame aFrame);
	uint32_t GlowLight() const;  // set glowlight

	// Spike 7's Part 5: an animated sprite's frame, aWidth x aHeight RGBA8 pixels at aX aY of the atlas's top level,
	// patched into the atlas at once (PatchAtlas), its levels under it rebuilt. The areas it changed wait for the next
	// Update's upload list, whose patch pass writes them into the GPU's atlas; a patch to an area still waiting replaces
	// the one before, which counts as dropped. Until the atlas is on the GPU, its whole upload carries them. With set
	// animate 0, or no atlas it fits, nothing changes, and it's counted.
	void ApplyAtlasPatch(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight, std::span<const uint8_t> aRgba);

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
		size_t      plantVertices = 0;        // the polish pass: of those, the plants'
		size_t      translucentVertices = 0;  // and the translucent blocks'
		bool        plantFoliage = true;      // set plants
		double      updateMsAverage = -1.0;  // Update's CPU time
		double      updateMsWorst = -1.0;
		size_t      pending = 0;  // uploads waiting
		uint32_t    atlasWidth = 0;  // 0 before there's an atlas
		uint32_t    atlasHeight = 0;
		uint32_t    atlasLevels = 0;
		uint64_t    hudReceived = 0;  // images SetHudImage took
		uint64_t    hudDropped = 0;   // and couldn't
		uint64_t    hudBytes = 0;     // of the images taken
		uint64_t    hudDrawn = 0;     // HUD passes
		uint64_t    hudSkipped = 0;
		std::string hudLastSkip;
		double      hudCopyMsAverage = -1.0;  // SetHudImage's copy
		double      hudGpuMsAverage = -1.0;   // the pass, on the GPU
		uint32_t    hudWidth = 0;  // the image drawn; 0 before one
		uint32_t    hudHeight = 0;
		bool        hudBottomUp = false;
		// Spike 7's entities
		bool        entitiesOn = true;          // set entities
		uint64_t    entityFrames = 0;           // G-buffer lists that drew them, in the last second
		uint64_t    entityDrawsSkipped = 0;     // their texture not on the GPU yet, or left out at the cap; in the last second
		uint64_t    snapshotWaits = 0;          // frames whose geometry waited for a free snapshot, in the last second
		uint64_t    snapshotCut = 0;            // draws past a snapshot's 8 MB, in the last second
		size_t      entityTextures = 0;         // placed: uploaded, or waiting to be
		double      entityTextureMb = 0.0;
		uint64_t    entityTexturesLeftOut = 0;  // at the cap, since the start
		double      entityGpuMsAverage = -1.0;  // the entities' draws in the G-buffer, on the GPU; below 0: none drawn
		// Step B's forward list
		bool        outlineOn = true;            // set outline
		int         dlssHint = 1;                // set dlsshint
		uint64_t    forwardFrames = 0;           // forward lists drawn, in the last second
		uint64_t    forwardHinted = 0;           // of those, the ones that drew DLSS's bias hint
		uint64_t    forwardSkipped = 0;          // frames whose forward list couldn't be drawn, in the last second
		std::string forwardLastSkip;             // why, the last time
		double      forwardGpuMsAverage = -1.0;  // the forward list, on the GPU; below 0: none drawn
		bool        blendOn = true;              // Part 5's set blend
		// Part 5's animated textures, in the last second
		bool        animateOn = true;          // set animate
		uint64_t    patchesApplied = 0;        // into the CPU's atlas
		uint64_t    patchesDropped = 0;        // replaced by a newer one before the upload list wrote it, or with set animate 0
		uint64_t    patchesRefused = 0;        // with no atlas they fit
		uint64_t    patchBytes = 0;            // of the patches applied
		size_t      patchAreasWaiting = 0;     // for the next upload list, now
		uint64_t    patchPasses = 0;           // upload lists that wrote patches into the GPU's atlas
		double      patchCpuMsAverage = -1.0;  // a patch's, rebuilding its levels, with the packing; below 0: none
		double      patchGpuMsAverage = -1.0;  // a patch pass's, on the GPU; below 0: none timed
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
		SectionParts                parts;      // the polish pass: where each part lies in the buffer
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
		SlTag                      hint;  // DLSS's bias hint (step B)
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
	void               PresentBlocks(IDXGISwapChain* aSwapChain);  // mode present: the blocks, over the finished frame
	void               PresentHud(IDXGISwapChain* aSwapChain);     // Minecraft's HUD, over that
	bool               OnMainQueue(IDXGISwapChain* aSwapChain);    // whether the swap chain presents on the main queue
	bool               EnsureHudObjects();                         // the HUD pass's lists and timing queries
	bool               EnsureHudPipeline(DXGI_FORMAT aFormat);
	void               HudSkip(const std::string& aWhy);  // counted apart: the HUD never switches drawing off
	void               ReadHudTimings(uint32_t aSlot);
	void               Upload();         // Update's work
	bool               EnsureObjects();  // false until the main queue is known, or if creating them failed
	bool               EnsurePipeline(DXGI_FORMAT aFormat, bool aReversed);
	bool               EnsurePredlssPipeline(bool aReversed);
	bool               EnsureGbufferPipeline(bool aReversed);
	bool               EnsureDepthPipeline(bool aReversed);
	bool               EnsureDepthTarget(uint32_t aWidth, uint32_t aHeight);
	bool               EnsureEntityPipelines(bool aReversed);  // spike 7's; false, logged once, if they can't be made
	ID3D12CommandList* RecordPredlss(ID3D12CommandQueue* aQueue, ID3D12GraphicsCommandList* aDlssList);  // null: skipped
	void               SeeGbufferSubmission(UINT aCount, ID3D12CommandList* const* aLists);
	void               WatchGbuffer();  // the G-buffer's targets and the tagged buffers, if they changed
	ID3D12CommandList* RecordGbuffer(ID3D12CommandQueue* aQueue);  // null: skipped
	FrameConstants     BaseConstants(const CameraFrame& aCamera) const;  // the camera, the atlas and the tuning values
	// BaseConstants, plus the image's size, the jitter and the motion: predlss and gbuffer draw at the render size
	FrameConstants     JitteredConstants(const CameraFrame& aCamera, const SlConstants& aGame, const sl::Extent& aExtent) const;
	// The sections' parts aFrom to aTo, which lie one after another in each section's buffer.
	void DrawSections(ID3D12GraphicsCommandList* aList, const CameraFrame& aCamera, uint64_t aCompleted, SectionPart aFrom, SectionPart aTo);
	// The polish pass: the sections' translucent parts, the farthest section first, as Minecraft orders them.
	void DrawTranslucentSections(ID3D12GraphicsCommandList* aList, const CameraFrame& aCamera, uint64_t aCompleted);
	// Whether the forward list has translucent blocks to draw. Sections still uploading count, as they'll be ready soon.
	bool ForwardSections() const;
	// Spike 7: a snapshot's draws of aKind; those whose texture isn't on the GPU are skipped, and counted if aCount.
	struct SnapshotGpu;
	void DrawEntities(ID3D12GraphicsCommandList* aList, const CameraFrame& aCamera, uint64_t aCompleted, const SnapshotGpu& aSnapshot,
	                  entities::EntityDraw::Kind aKind, bool aCount);
	// Part 5: or those of the kinds aKinds has, in the snapshot's order, each with its kind's pipeline, set only when it
	// changes (null: the one set already).
	struct KindPipeline
	{
		entities::EntityDraw::Kind kind = entities::EntityDraw::Kind::Model;
		ID3D12PipelineState*       pipeline = nullptr;
	};
	void DrawEntities(ID3D12GraphicsCommandList* aList, const CameraFrame& aCamera, uint64_t aCompleted, const SnapshotGpu& aSnapshot,
	                  std::span<const KindPipeline> aKinds, bool aCount);
	// Spike 7: the textures waiting, into the one buffer, grown first if they don't fit, recorded into Upload's list.
	void UploadEntityTextures(uint64_t aReady, std::vector<gpu::ComPtr<ID3D12Resource>>& aStaging, bool& aFailed);
	// Part 5: the atlas's waiting areas, packed and written into the GPU's atlas by the patch pass, in Upload's list.
	void RecordPatches();
	bool EnsurePatchObjects();  // the patch pass's root signature, pipeline and timing; false, logged once, if not
	void CopyEntities();  // spike 7: Update's, the frame's geometry into a free snapshot
	// Step B's forward list, before the DLSS list its frame's G-buffer list left it (forward_): null if it's skipped,
	// which lets go of the frame's snapshot and never switches drawing off.
	ID3D12CommandList* RecordForward(ID3D12CommandQueue* aQueue, ID3D12GraphicsCommandList* aDlssList);
	bool               EnsureForwardObjects();  // its lists, constants and timing queries
	bool               EnsureForwardPipelines(bool aReversed);  // false, logged once, if they can't be made
	void               ForwardSkip(const std::string& aWhy);
	void               DropForward();  // a forward list that won't come: its frame's snapshot let go
	void               ReadForwardTimings(uint32_t aSlot);
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
	bool    plantFoliage_ = true;   // gbuffer: plants drawn with the game's class for its own small plants (set plants)
	uint8_t plantStencil_ = 0xA0;   // that class: step 1's dump found it on the game's small plants
	float   plantB_ = 0.75f;        // and their material B, from 0.63 to 0.86 on the game's plants

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
	// motion vectors; the forward list's colour and bias hint. DSVs: our depth buffer; predlss's depth; gbuffer's
	// G-buffer depth and tagged depth; the forward list's tagged depth, read-only.
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
	gpu::ComPtr<ID3D12Resource>                                    atlasBuffer_;  // with unordered access, for the patch pass
	uint64_t                                                       atlasReady_ = 0;
	bool                                                           atlasUploaded_ = false;
	// Part 5's animated textures: the areas waiting for the patch pass, and its objects. Its upload buffer is reused: Upload
	// records a list only once the last one is done.
	bool                                                           animate_ = true;  // set animate
	AtlasDirty                                                     dirty_;
	gpu::ComPtr<ID3D12RootSignature>                               patchRootSignature_;
	gpu::ComPtr<ID3D12PipelineState>                               patchPipeline_;
	bool                                                           patchFailed_ = false;  // logged; until the shaders are reloaded
	gpu::ComPtr<ID3D12Resource>                                    patchUpload_;
	uint8_t*                                                       patchUploadMapped_ = nullptr;
	uint64_t                                                       patchUploadBytes_ = 0;
	gpu::ComPtr<ID3D12QueryHeap>                                   patchQueries_;  // 2 timestamps, around the pass
	gpu::ComPtr<ID3D12Resource>                                    patchTimings_;  // readback
	uint64_t*                                                      patchTimingsMapped_ = nullptr;
	bool                                                           patchTimed_ = false;  // the last upload list timed a pass
	double                                                         patchCpuMsSum_ = 0.0;
	uint64_t                                                       patchCpuSamples_ = 0;
	double                                                         patchGpuMsSum_ = 0.0;
	uint64_t                                                       patchGpuSamples_ = 0;
	std::map<SectionPos, SectionGpu>                               sections_;
	PendingSections                                                pending_;
	Retired<gpu::ComPtr<ID3D12Resource>>                           retired_;

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

	// Spike 7's entities. The snapshots: upload buffers, written by Update into one no list may still read (Snapshots),
	// each with the draws that fit in it.
	struct SnapshotGpu
	{
		gpu::ComPtr<ID3D12Resource>       buffer;  // upload heap, mapped while it lives
		uint8_t*                          mapped = nullptr;
		uint64_t                          bytes = 0;
		Vec3d                             origin;  // the frame's, Minecraft axes
		std::vector<entities::EntityDraw> draws;
	};
	static constexpr uint32_t kSnapshots = 8;
	static constexpr uint64_t kSnapshotMaxBytes = 8ull << 20;  // 262,144 vertices: a big blast's scene is about 1 MB
	struct PendingTexture
	{
		uint32_t              id = 0;
		EntityTextures::Place place;
		std::vector<uint8_t>  rgba;
	};
	bool                                        entitiesOn_ = true;  // set entities
	float                                       glow_ = 0.9f;        // set glow: near the 0.93 spike 3 found on the game's signs
	uint32_t                                    glowLight_ = 15;     // set glowlight
	EntityTextures                              textures_;
	std::vector<PendingTexture>                 pendingTextures_;
	std::map<uint32_t, uint64_t>                textureReady_;     // each uploaded texture's fence
	gpu::ComPtr<ID3D12Resource>                 entityTextures_;   // default heap, entityTexels_ texels
	uint64_t                                    entityTexels_ = 0;
	bool                                        capLogged_ = false;
	uint64_t                                    texturesLeftOut_ = 0;
	std::optional<entities::EntityFrame>        pendingEntities_;  // SetEntities's, until Update copies it
	std::array<SnapshotGpu, kSnapshots>         snapshots_;
	Snapshots                                   snapshotRing_{ kSnapshots };
	std::array<std::optional<uint32_t>, kSlots> slotSnapshot_{};  // the snapshot each slot's list took
	gpu::ComPtr<ID3D12PipelineState>            entityPipeline_;
	gpu::ComPtr<ID3D12PipelineState>            entityDepthPipeline_;
	gpu::ComPtr<ID3D12PipelineState>            crackPipeline_;
	bool                                        entityReversed_ = false;
	bool                                        entityMotionWrites_ = false;
	bool                                        entityPipelinesFailed_ = false;  // until the shaders are reloaded
	bool                                        snapshotLogged_ = false;  // a snapshot buffer couldn't be made: logged once
	gpu::ComPtr<ID3D12QueryHeap>                entityQueries_;  // kSlots x 2 timestamps, around the entities' draws
	gpu::ComPtr<ID3D12Resource>                 entityTimings_;  // readback
	uint64_t*                                   entityTimingsMapped_ = nullptr;
	std::array<bool, kSlots>                    slotEntityTimed_{};
	double                                      entityGpuMsSum_ = 0.0;
	uint64_t                                    entityGpuSamples_ = 0;

	// Step B's forward list. A G-buffer list that drew a snapshot with something for it, or whose sections have (the
	// polish pass), leaves its frame's DLSS list, its tags, camera and constants, and any such snapshot, held for the
	// forward list (Snapshots::Take) until that's recorded, skipped or dropped.
	struct Forward
	{
		const void*             list = nullptr;  // the frame's DLSS list; compared, never used
		Evaluate                evaluate;
		CameraFrame             camera;
		FrameConstants          constants{};
		sl::Extent              extent{};
		std::optional<uint32_t> snapshot;  // none: only the sections' parts
	};
	bool                                                       outline_ = true;  // set outline: the outline in build mode
	int                                                        dlssHint_ = 1;    // set dlsshint: 1 under the particles, 0 nowhere
	bool                                                       blend_ = true;    // Part 5's set blend: blended layers before DLSS
	std::optional<Forward>                                     forward_;
	std::atomic<const void*>                                   forwardList_{ nullptr };  // forward_->list, read without the lock
	gpu::ComPtr<ID3D12PipelineState>                           translucentPipeline_;
	gpu::ComPtr<ID3D12PipelineState>                           outlinePipeline_;
	gpu::ComPtr<ID3D12PipelineState>                           hintPipeline_;
	gpu::ComPtr<ID3D12PipelineState>                           blendedPipeline_;   // Part 5
	gpu::ComPtr<ID3D12PipelineState>                           additivePipeline_;
	gpu::ComPtr<ID3D12PipelineState>                           blendedBlockPipeline_;  // the polish pass's translucent blocks
	int                                                        forwardCull_ = -1;      // as the blocks: set cull
	bool                                                       forwardReversed_ = false;
	bool                                                       forwardPipelinesFailed_ = false;  // until the shaders are reloaded
	std::array<gpu::ComPtr<ID3D12CommandAllocator>, kSlots>    forwardAllocators_;
	std::array<gpu::ComPtr<ID3D12GraphicsCommandList>, kSlots> forwardLists_;
	std::array<uint64_t, kSlots>                               forwardSlotFence_{};
	std::array<std::optional<uint32_t>, kSlots>                forwardSlotSnapshot_{};  // the snapshot each slot's list holds
	uint64_t                                                   forwardRecordings_ = 0;
	gpu::ComPtr<ID3D12Resource>                                forwardConstants_;  // upload: kSlots x kConstantsStride bytes
	uint8_t*                                                   forwardConstantsMapped_ = nullptr;
	gpu::ComPtr<ID3D12QueryHeap>                               forwardQueries_;
	gpu::ComPtr<ID3D12Resource>                                forwardTimings_;  // readback: kSlots x 2 timestamps
	uint64_t*                                                  forwardTimingsMapped_ = nullptr;
	double                                                     forwardGpuMsSum_ = 0.0;
	uint64_t                                                   forwardGpuSamples_ = 0;

	Stats    stats_;
	double   gpuMsSum_ = 0.0;
	uint64_t gpuSamples_ = 0;
	double   frameMsSum_ = 0.0;
	uint64_t frameSamples_ = 0;
	int64_t  lastPresent_ = 0;
	double   updateMsSum_ = 0.0;
	uint64_t updateSamples_ = 0;

	// The HUD (SetHud, SetHudImage and the HUD pass)
	struct HudImage
	{
		uint32_t width = 0;
		uint32_t height = 0;
		bool     bottomUp = false;
	};
	std::atomic<bool>                                          hudWanted_{ false };
	std::atomic<const HudCursor*>                              hudCursor_{ nullptr };
	bool                                                       hudShadersFailed_ = false;  // main thread: not again until a reload
	std::atomic<uint32_t>                                      backWidth_{ 0 };
	std::atomic<uint32_t>                                      backHeight_{ 0 };
	HudSlots                                                   hudSlots_;
	std::array<gpu::ComPtr<ID3D12Resource>, HudSlots::kSlots> hudUploads_;
	std::array<uint8_t*, HudSlots::kSlots>                     hudMapped_{};
	std::array<uint64_t, HudSlots::kSlots>                     hudUploadBytes_{};
	std::array<HudImage, HudSlots::kSlots>                     hudSlotImages_{};
	gpu::ComPtr<ID3D12Resource>                                hudImage_;  // in GPU memory: what the pass draws
	uint64_t                                                   hudImageBytes_ = 0;
	bool                                                       hudImageReadable_ = false;  // copied into, and a shader resource
	HudImage                                                   hudShown_;
	gpu::ComPtr<ID3D12PipelineState>                           hudPipeline_;
	DXGI_FORMAT                                                hudPipelineFormat_ = DXGI_FORMAT_UNKNOWN;
	std::array<gpu::ComPtr<ID3D12CommandAllocator>, kSlots>    hudAllocators_;
	std::array<gpu::ComPtr<ID3D12GraphicsCommandList>, kSlots> hudLists_;
	std::array<uint64_t, kSlots>                               hudSlotFence_{};
	uint64_t                                                   hudRecordings_ = 0;
	gpu::ComPtr<ID3D12QueryHeap>                               hudQueries_;
	gpu::ComPtr<ID3D12Resource>                                hudTimings_;  // readback: kSlots x 2 timestamps
	uint64_t*                                                  hudTimingsMapped_ = nullptr;
	double                                                     hudCopyMsSum_ = 0.0;
	uint64_t                                                   hudCopySamples_ = 0;
	double                                                     hudGpuMsSum_ = 0.0;
	uint64_t                                                   hudGpuSamples_ = 0;
};
}
