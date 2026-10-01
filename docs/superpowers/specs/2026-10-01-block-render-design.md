# Block render: design (spike 3)

Date: 2026-10-01. Status: agreed with the user section by section in the brainstorm. This document awaits their
review.

## Purpose

This is spike 3 of the project that recreates [SkyCraft](https://github.com/chasmlol/SkyCraft) (Minecraft in Skyrim)
for Cyberpunk 2077. Minecraft runs hidden next to the game, and the game shows Minecraft's world: the blocks a player
places and breaks, and later items, arrows and so on. Spike 1 settled reading the game's collision. Spike 3 de-risks
the drawing side:

> Can Night City show Minecraft's blocks, at the scale a player builds, with an acceptable frame rate and a believable
> look?

It answers that the way SkyCraft does: the plugin draws block meshes itself, inside the game's own frame. There are
four questions, one per step:

1. **The map:** where in Cyberpunk's frame can a plugin draw, with what camera, and into which buffers?
2. **The renderer:** can our own D3D12 renderer draw textured blocks that stay fixed in the world and that the game's
   geometry hides correctly?
3. **Inside the frame:** can blocks go into the image before DLSS, so that they sit under the HUD, are tone mapped
   with the scene and stay steady under DLSS? What do they cost?
4. **The look:** can blocks be written into the game's G-buffer, so that the game's own lighting, shadows, fog,
   reflections and DLSS treat them as its own geometry?

A "no" at step 4 also counts as a result. The spike then ends at step 3, and the results say what stopped it.

## Decisions from the brainstorm

- **Direct rendering, not engine entities.** The handoff weighed two routes: spawning blocks as game entities through
  Codeware, or drawing them ourselves. It suggested entities first. The user chose direct rendering, SkyCraft's way.
- **Scope: into the G-buffer,** in four steps, each tested in the game (see "The four steps").
- **Streamline is the way in.** The game's DLSS goes through NVIDIA Streamline. Its calls carry the frame's camera,
  and the depth, motion vectors and colour that DLSS reads. Hooking them replaces most of the renderer map that
  SkyCraft gets for Skyrim from CommonLibSSE.
- **Our own frame logger, not a capture tool.** The plugin logs and dumps the frame itself, because it needs that code
  anyway to find the passes while the game runs. NVIDIA Nsight Graphics or Microsoft PIX comes in only if the dumps
  leave the G-buffer's encoding unclear.
- **Minecraft's textures, from the user's own Minecraft.** The user's Minecraft 26.3 jar holds its 1,300 block
  textures. A tool builds some of them into an atlas in the plugin's folder, never in the repo.
- **SkyCraft's vertex format.** The test structures are meshed into SkyCraft's 32-byte vertices. Later, Minecraft's own
  meshes can replace them: SkyCraft's Fabric mod (MIT, Minecraft 26.3) already builds them.
- **What Minecraft's de-obfuscation changes.** Minecraft has shipped unobfuscated since 26.1. That makes the Minecraft
  side easy, and SkyCraft already relies on it. It does nothing for mapping Cyberpunk's renderer.
- **Minecraft doesn't draw its own world.** SkyCraft's first design had Minecraft render its world from the game's
  camera, and shared the image between the two games. SkyCraft moved to drawing the meshes itself, to light them with
  Skyrim's sun, shadows, fog and weather. It kept a captured Minecraft image only for the HUD and the hand. Spike 3
  follows the same split.

## Non-goals

- **No Minecraft:** no link, no shared memory and no RenderRing. The plugin makes its own test structures.
- **No translucent blocks** (stained glass, water, ice), because a G-buffer only takes opaque surfaces. Cut-out blocks,
  whose pixels are either opaque or skipped, are in: glass and leaves.
- **No other Minecraft drawing:** no entities, items, particles, block entities, animated textures or the player's hand.
- **No light exchange:** blocks cast no shadows on the city, and Minecraft's lights (torches and so on) don't light it.
- **No collision:** NPCs, cars and bullets pass through blocks.
- **DLSS must be on.** There's no path for FSR, XeSS, TAA alone or native resolution, and none for frame generation,
  ray tracing or path tracing.
- **Nothing persists.** Blocks live in the plugin's memory, and nothing goes into saves.
- **Only these versions are supported:** game 2.31, Streamline 2.7.1 and the D3D12 Agility SDK 615.

## Environment (checked on 2026-10-01)

### The machine and its settings

- **Hardware:** NVIDIA GeForce RTX 3070 (driver 32.0.16.1714), AMD Ryzen 7 9800X3D, Windows 10 Pro 19045.
- **Display:** 2560×1440 in a borderless window, VSync off, Reflex on.
- **Upscaling:** DLSS super resolution, Quality preset, transformer model. Quality renders at two thirds of the output
  on each axis, about 1707×960.
- **Off:** frame generation and ray tracing.
- **Frame rate:** 40 to 80 fps in the city.

### Streamline and DLSS

- **Versions:** `bin\x64\sl.interposer.dll` is Streamline 2.7.1 (FileVersion 2.7.1.0). Next to it are `sl.common`,
  `sl.dlss`, `sl.dlss_g`, `sl.dlss_d`, `sl.reflex`, `sl.pcl` and `sl.nis`. `nvngx_dlss.dll` is DLSS 310.1.0.0.
- **The game's imports:** `Cyberpunk2077.exe` imports 12 functions from `sl.interposer.dll`:
  - `slInit`, `slShutdown`, `slIsFeatureSupported`, `slIsFeatureLoaded`, `slSetFeatureLoaded` and
    `slGetFeatureFunction`;
  - `slUpgradeInterface` and `slSetD3DDevice`;
  - `slGetNewFrameToken`, `slSetConstants`, `slSetTag` and `slEvaluateFeature`.
- **The headers:** Streamline's public headers are MIT-licensed. The nearest public tag to 2.7.1 is v2.7.2. Its
  signatures:
  - `slSetConstants(const Constants&, const FrameToken&, const ViewportHandle&)`;
  - `slSetTag(const ViewportHandle&, const ResourceTag* tags, uint32_t numTags, CommandBuffer*)`;
  - `slEvaluateFeature(Feature, const FrameToken&, const BaseStructure** inputs, uint32_t numInputs, CommandBuffer*)`;
  - `slGetNewFrameToken(FrameToken*&, const uint32_t* frameIndex)`.
- **`sl::Constants`** (struct version 2) holds:
  - five row-major matrices **without jitter**: `cameraViewToClip`, `clipToCameraView`, `clipToLensClip`,
    `clipToPrevClip` and `prevClipToClip`;
  - `jitterOffset` (in pixels), `mvecScale` and `cameraPinholeOffset`;
  - `cameraPos`, `cameraUp`, `cameraRight` and `cameraFwd`, in world space;
  - `cameraNear`, `cameraFar`, `cameraFOV` (radians), `cameraAspectRatio` and `motionVectorsInvalidValue`;
  - the flags `depthInverted`, `cameraMotionIncluded`, `motionVectors3D`, `reset`, `orthographicProjection`,
    `motionVectorsDilated` and `motionVectorsJittered`.
- **Buffer types** include depth (0), motion vectors (1), HUD-less colour (2), scaling input colour (3), scaling
  output colour (4), exposure (13), and UI colour and alpha (23). Each tag's `sl::Resource` carries the native resource
  and its D3D12 state.
- **The approach works in this game:** [OptiScaler](https://github.com/optiscaler/OptiScaler/wiki/Cyberpunk-2077), an
  open-source upscaler swapper, hooks Streamline to read Cyberpunk's DLSS inputs.

### D3D12

- **Loaded at runtime:** the game has no static import of `d3d12.dll` or `dxgi.dll`. It calls `D3D12CreateDevice`,
  `CreateDXGIFactory2` and `D3D12GetDebugInterface` by name.
- **Its own runtime:** the game ships the D3D12 Agility SDK. The exe exports `D3D12SDKVersion` = 615, and
  `bin\x64\D3D12\` holds `D3D12Core.dll` and `d3d12SDKLayers.dll`, both 1.615.0. So the vtables of the game's D3D12
  objects live in that `D3D12Core.dll`, and the debug layer is there if needed.
- **The shader compiler:** `C:\Windows\System32\D3DCompiler_47.dll` is 10.0.19041.7663. It compiles shader model 5.1.

### RED4ext and its SDK

- **Load time:** RED4ext 1.30.0 loads plugins about a second after the game starts, before scripts compile. Step 1
  checks whether that's before the game creates its D3D12 device.
- **`RED4ext::GpuApi`:** the SDK's `GetDeviceData()` gives:
  - the D3D12 device (offset 0x13BC4A8) and the direct command queue (0x13BC4D0);
  - the swap chains (0xC97F20). Each `SSwapChainData` holds the swap chain, its back buffer's texture ID and the
    back buffers' render-target views;
  - the game's 128 command list contexts (0xD1AD80). Each `CommandListContext` holds a `debugName` (0x00), its
    `ID3D12GraphicsCommandList` (0x30) and a type (0x68).
- **Hooking:** the SDK's `hooking->Attach` detours a function. BlockRender uses it once, at load, on
  `D3D12CreateDevice`.

### Other mods

- **CET** hooks the game's present with MinHook (`CRenderNode_Present_DoInternal`, `CRenderGlobal_Resize` and
  `CRenderGlobal_Shutdown`), and draws its overlay into the back buffer. BlockRender hooks none of those three.
- **The PhysX probe** (0.4.0) and its overlay stay installed. The probe owns F10, so BlockRender uses no hotkey.

### Cyberpunk's frame, as published

Two analyses of the game's first versions, [zhangdoa.com](https://zhangdoa.com/rendering-analysis-cyberpunk-2077/) and
[c0de517e](http://c0de517e.blogspot.com/2020/12/hallucinations-re-rendering-of.html), describe a deferred renderer.
They captured at 1080p, without ray tracing or DLSS. In order:
1. **A partial depth pre-pass.**
2. **A G-buffer** of four render targets:
   - `R10G10B10A2_UNORM`: albedo, with an object mask in alpha;
   - `R10G10B10A2_UNORM`: the world-space normal, stored as 0.5n + 0.5;
   - `R8G8B8A8_UNORM`: metalness, roughness and other attributes;
   - `R16G16_FLOAT`: motion vectors.

   Depth and stencil are `D32_FLOAT_S8X24_UINT`. The stencil marks bodies (0x15), faces (0x35), hair (0x95) and
   trees (0xA0).
3. **Decals,** then four shadow cascades, tiled lighting in compute, screen-space reflections and volumetric fog.
4. **TAA,** bloom and colour grading.
5. **The UI,** last.

Version 2.31 has changed since then. Here DLSS replaces TAA, and there's path tracing. So the frame log re-checks all
of it.

### Minecraft and SkyCraft

- **Minecraft 26.3** is installed (`%APPDATA%\.minecraft\versions\26.3\26.3.jar`):
  - it's unobfuscated, with a new renderer (`com.mojang.renderpearl`) that has OpenGL and Vulkan back ends;
  - it has 1,300 block textures under `assets/minecraft/textures/block/`;
  - its terrain shader multiplies the texture (nearest sampling) by the vertex colour and a lightmap, then applies fog.
- **SkyCraft's Fabric mod** targets 26.3, and builds section meshes with Minecraft's own block renderer. Its vertex
  is 32 bytes (see "Vertex format").

## Vertex format

SkyCraft's section vertex, which the test structures use too:

| Bytes | Field | Meaning |
|---|---|---|
| 0–11 | `float x, y, z` | Position from the section's minimum corner, in blocks, in Minecraft axes (x east, y up, z south) |
| 12–19 | `float u, v` | Atlas coordinates |
| 20–23 | `uint8 r, g, b, a` | Tint and ambient occlusion, without Minecraft's per-face brightness |
| 24–27 | `uint32 light` | Block light (0–15) in bits 0–7, sky light (0–15) in bits 8–15 |
| 28–31 | `uint32 flags` | Bit 0 cut-out, bit 1 translucent. Bits 4–7: the face's direction + 1, in Minecraft's order (down, up, north, south, west, east), or 0 for none |

- **Quads:** each is two triangles, so six vertices (corners 0, 1, 2, 0, 2, 3). There's no index buffer.
- **World position:** the section's origin plus `WorldFromMc(x, y, z)`, which is `(x, −z, y)` (spike 1's `Coords.h`).

## Repository layout

```
docs/superpowers/specs/2026-10-01-block-render-design.md   this file
extern/streamline-2.7.2/          the Streamline headers the plugin needs, with NVIDIA's MIT licence
spikes/block-render/
  CMakeLists.txt, CMakePresets.json
  src/Plugin.cpp                  RED4ext entry points, the main-thread update
  src/Requests.h, .cpp            the request file's commands
  src/Status.h, .cpp              the status file the overlay reads
  src/PlayerLocator.*, Readable.*, Coords.h, Json.*   copied from spike 1
  src/hooks/ImportPatch.*         import-table patching
  src/hooks/VtablePatch.*         vtable patching
  src/hooks/Slots.h               the vtable slots that are patched
  src/hooks/StreamlineHooks.*
  src/hooks/D3D12Hooks.*
  src/track/ViewRegistry.*        descriptor handle to resource
  src/track/StateTracker.*        per-command-list state, and resource states
  src/track/ListNames.*           the game's names for its command lists
  src/log/FrameLog.*              one frame's log
  src/log/Dumps.*, DdsWriter.*
  src/render/Renderer.*           our D3D12 objects and the drawing modes
  src/render/Camera.*             matrices, jitter and motion from Streamline's constants
  src/render/Shaders.*            compiling HLSL in the game
  src/render/Atlas.*
  src/render/GpuTimer.*           timestamp queries
  src/world/Vertex.h, BlockWorld.*, Mesher.*, Structures.*
  shaders/blocks.hlsl
  tests/                          unit tests, with spike 1's harness
  tools/build_atlas.py, view_dump.py, and their tests
  tools/blockrender_hud/init.lua  the CET overlay
```

## Build

- **As in spike 1:** CMake with the Visual Studio 2022 generator, x64, C++20 and `/W4`. The RED4ext SDK is used
  header-only. The presets are `default` and `release`, and the tests use the same harness.
- **Libraries:** it links `d3d12.lib` (for `D3D12SerializeRootSignature`), `d3dcompiler.lib` and `dxguid.lib`.
  Nothing from Streamline is linked. The plugin uses only the types from Streamline's headers, and calls the originals
  through the pointers it replaced.
- **Streamline's headers:** `extern/streamline-2.7.2/` holds the headers the plugin needs, unchanged. Static checks
  pin the offset of every field the plugin reads.
- **Deploying:** `CP2077_DIR` works as in spike 1. When it's set, a post-build step copies `BlockRender.dll` and
  `shaders\` to `<CP2077_DIR>\red4ext\plugins\BlockRender\`, and the overlay to CET's `mods\blockrender_hud\`. It's set
  only to deploy, with the game closed, and cleared straight after.

## Components

### `Plugin.cpp`

- **Entry points:** as in spike 1, with the name `BlockRender`.
- **On load:**
  - reads `settings.txt` from its folder, if there is one. Two settings, both off unless set to 1: `dred` and
    `debuglayer` (see "Safety");
  - installs the Streamline import patches and the `D3D12CreateDevice` detour.
- **Every frame, on the main thread,** a game state for `Running` runs `OnUpdate`. It:
  - checks `blockrender.request` once a second, and runs the command;
  - places structures, meshes changed sections, and hands the meshes to the renderer;
  - writes the log lines the render threads queued;
  - once a second, writes the status file.
- **On unload:** switches drawing off, puts back every patch that still holds our pointer, and releases our D3D12
  objects.

### Hooks

- **`ImportPatch`:** finds `sl.interposer.dll` in the game exe's import directory. It swaps the entries for
  `slGetNewFrameToken`, `slSetConstants`, `slSetTag` and `slEvaluateFeature`, with one atomic pointer write each, and
  keeps the originals.
- **`StreamlineHooks`:** each hook records what it's given, then calls the original and returns its result unchanged.
  - `slGetNewFrameToken`: notes the index of the frame the game is starting.
  - `slSetConstants`: checks the struct's type GUID and version, then keeps viewport 0's constants in a small ring,
    by frame index.
  - `slSetTag`: checks each tag, then records its type, resource, state, extent and lifecycle for the frame. It reads
    each resource's description (format and size).
  - `slEvaluateFeature`: notes which command list evaluates DLSS super resolution. If that list is a Streamline proxy,
    `slGetNativeInterface` gives the native one. The game doesn't import that function, so the plugin finds it among
    `sl.interposer.dll`'s exports. Depending on the step, this hook:
    - records dumps of the tagged buffers (step 1);
    - copies the depth (step 2);
    - draws, or marks the list for our own list to follow (step 3).
- **`D3D12Hooks`:**
  - **The device:** the detour on `D3D12CreateDevice` (the plugin loads `d3d12.dll` at load to find it) turns on DRED
    or the debug layer if the settings ask for them, then calls the original. When a device comes back, it:
    - patches the device's vtable for `CreateRenderTargetView`, `CreateDepthStencilView`, `CopyDescriptors` and
      `CopyDescriptorsSimple`;
    - creates a command queue, a command allocator and a command list of its own, patches their vtables, then
      releases them;
    - logs that it saw the device.
  - **The command queue:** `ExecuteCommandLists` puts our command lists into the game's submissions: uploads at the
    front, and drawing straight after the list it follows. It also records the submissions for the frame log.
  - **The command lists:** the calls that the tracker and the log need:
    - `Reset` and `Close`;
    - `OMSetRenderTargets`, `BeginRenderPass` and `EndRenderPass`;
    - `ClearRenderTargetView` and `ClearDepthStencilView`;
    - `ResourceBarrier` and `Barrier`;
    - `SetPipelineState`, `SetGraphicsRootSignature` and the calls that set graphics root arguments;
    - `RSSetViewports`, `RSSetScissorRects`, `IASetPrimitiveTopology` and `OMSetStencilRef`;
    - the draw and dispatch calls, `ExecuteIndirect` and `ExecuteBundle`;
    - `BeginEvent`, `EndEvent` and `SetMarker`.
  - **The swap chain:** `Present` and `Present1`, patched on the swap chain the game uses (from `GetDeviceData()`) the
    first time Streamline's hooks see a frame.
  - **Why vtable patches:** everything after load is patched in vtables, because swapping one pointer is safe while
    the game's render threads are running. Rewriting the start of a heavily called function isn't.
- **`VtablePatch`:** makes a vtable entry writable, swaps it atomically, restores the protection and keeps the
  original. Restoring only puts the original back if the entry still holds ours.
- **`Slots.h`:** the vtable slot of every patched method. A unit test checks each one against the C interface layouts
  in the Windows SDK's headers.

### `ViewRegistry`

- **The map:** RTV and DSV descriptor handles to the resource and view description they were made with. It follows
  `CreateRenderTargetView`, `CreateDepthStencilView`, and copies of RTV and DSV descriptors, under a reader-writer
  lock.
- **The fallback:** if step 1 shows the device was created before the plugin loaded, the map is filled from GpuApi's
  own texture table instead (see "Risks").

### `StateTracker`

- **Which lists:** it follows each command list from `Reset` to `Close`. A list whose `Reset` came before tracking
  started is marked untracked, and nothing is ever drawn into it.
- **What it keeps:**
  - everything our drawing may change: the bound render targets and depth (with their resources), the pipeline
    state, the graphics root signature and every root argument set since it, the viewports, scissors, topology and
    stencil reference;
  - each resource's state after the last barrier recorded in the list.
- **`Restore(list)`** sets all of that on the list again, after our draws.
- **For the frame log,** when a log is armed, it splits each list into passes, with each change of render targets
  starting one. It counts each pass's draws, indirect draws, dispatches and clears, and keeps any event names.

### `ListNames`

Finds a native command list's `CommandListContext` in `GetDeviceData()->commandLists`, and reads its `debugName`.
Every read is range-checked with `Readable`. Only the frame log uses it.

### `FrameLog` and dumps

- **`log`** arms the next whole frame, from one `slGetNewFrameToken` to the next. It writes
  `logs\<time>\frame.json` (see "The frame log").
- **`dump <what>`** copies buffers into readback memory at the moment each is complete:
  - the tagged buffers at the DLSS evaluate;
  - the G-buffer candidates when their pass ends.

  Barriers move each buffer into the copy state and back. Each buffer is written as a `.dds` file (with a DX10 header
  and the resource's own format), and the depth buffer's stencil plane as a file of its own.
- **`view_dump.py`** turns the dumps into PNGs: albedo as it is, normals as colours, depth linearised, motion vectors
  as a flow image, and stencil as a palette.

### `Renderer`

- **Our D3D12 objects:**
  - one root signature: a constant buffer view, two buffer views (vertices, and the atlas) and root constants;
  - pipeline states for each mode, and CPU-only RTV and DSV descriptor heaps of our own;
  - default-heap buffers for the section meshes and the atlas, an upload ring and a per-frame constant-buffer ring;
  - a command allocator and command list per frame in flight, a fence, and the GPU timer.
- **No descriptor heaps:** the shaders read vertices and the atlas through root buffer views, so they need no
  shader-visible descriptor heap, and the game's heaps are never changed. The one exception is step 2's Present pass:
  it runs in our own list, and reads our depth copy through a heap of our own.
- **The modes:**
  - **`off`.**
  - **`present` (step 2):**
    - At the DLSS evaluate, the tagged depth's depth plane is copied into our own `R32_FLOAT` texture. That's only
      barriers and a copy, recorded in the game's list.
    - At `Present`, our own list draws the blocks into the back buffer, before the original `Present` runs. The pixel
      shader discards any pixel where the depth copy is nearer, mapping each output pixel to the render-resolution
      depth.
  - **`predlss` (step 3):** at the DLSS evaluate, draws into the tagged scaling-input colour. It tests and writes the
    tagged depth, and writes the tagged motion vectors. Shading is forward: the atlas, times the vertex colour, times
    Minecraft's face brightness (up 1.0, down 0.5, north and south 0.8, east and west 0.6), times an exposure.
    - **The exposure:** if the game tags an exposure texture, it's copied into a small buffer of ours each frame, and
      the shader reads it. Otherwise it's `set exposure`.
  - **`gbuffer` (step 4):** draws into the G-buffer targets and depth that the frame log found, writing albedo,
    normal, material values and motion vectors in the encoding the dumps show.
    - **Recognising the pass:** every frame, by its targets' formats and sizes, and by its depth being the main depth
      buffer that Streamline's latest tags name. The frame log shows whether the game alternates two depth buffers.
    - **Drawing once per frame:** our draws go in when the list leaves the pass, at its next change of render targets
      or at `Close`, and only once per frame. If the G-buffer pass is spread over several lists, one of them is enough,
      because depth testing sorts out which is in front.
- **Where the commands go:** our own command list, in `ExecuteCommandLists`, when the frame log shows a list boundary
  at the right point. Otherwise they're recorded into the game's list, followed by `StateTracker::Restore`.
- **The camera:** written into the constant buffer as late as possible, just before the list that holds our draws is
  submitted, from Streamline's constants for that frame. That's in `ExecuteCommandLists` in steps 3 and 4, and in
  `Present` in step 2, where the frame is the one whose DLSS evaluate came last. If the frame's constants haven't
  arrived yet, the previous frame's are used, and the frame is counted as "late camera" (see "Risks").
- **Drawing:** one draw per section, skipping sections outside the view frustum. A cut-out pixel with alpha under 0.5
  is discarded.
- **GPU time:** timestamps before and after our draws, resolved into a readback buffer and read three frames later.
  Timestamps and their resolves change no pipeline state, so they're safe in the game's list.

### `Camera`

- **The matrices:**
  - the world-to-view matrix is built from `cameraPos`, `cameraRight`, `cameraUp` and `cameraFwd`;
  - the projection is `cameraViewToClip`;
  - the jitter is added as a clip-space offset of 2 × jitter / size on each axis. Its sign is settled in step 3
    (`set jittersign`).
- **Motion vectors** for static points come from `clipToPrevClip`, in the units that `mvecScale` implies. Their sign
  is settled in step 3, against the game's own motion vectors (`set mvsign`).
- **Positions are camera-relative.** Section origins are subtracted from the camera's position in doubles, so the GPU
  only sees small numbers.

### `Shaders`

`shaders\blocks.hlsl` lives in the plugin's folder. `D3DCompile` compiles it (shader model 5.1) when the renderer
starts, and again on `reload`. A compile error is logged, and the previous pipelines stay.

### `Atlas`

- **Loading:** reads `atlas.bin` (RGBA8 pixels, with mip levels for each sprite) and `atlas.json` (each block's
  sprites per face, its tint and its cut-out flag) from the plugin's folder, and uploads the pixels into a buffer.
- **Sampling:** the shader samples the nearest texel, picking a mip level from the UV derivatives, as Minecraft does
  with mipmaps on.

### `BlockWorld`, `Mesher` and `Structures`

- **`BlockWorld`:** sparse 16×16×16 sections, keyed by section coordinates. Block IDs index the block list in
  `atlas.json`. Only the main thread touches it.
- **`Mesher`:**
  - drops faces against opaque neighbours, across section borders too. A cut-out block hides a face only against the
    same block, as glass does against glass;
  - writes the vertex format above, with sky light 15 and block light 0;
  - puts Minecraft-style corner ambient occlusion into the colour, switchable with `set ao`.
- **`Structures`:** each is placed on the block grid at the height of V's feet, with its minimum corner 3 blocks ahead
  of V.
  - `cube`: one stone block.
  - `sampler`: every block in the atlas list, in a row with a gap between each, on a stone base.
  - `house`: 7×7 and 5 high. A cobblestone floor, oak plank walls, oak log corners, a glass window in each side, a
    doorway, a plank roof, and an oak-leaves bush beside it.
  - `field <n>`: rolling terrain of grass over dirt over stone, sized to hold about n blocks.
- **Uploads:** new and changed sections are meshed on the main thread. They're uploaded by our own list at the front
  of the next submission, and old buffers are freed once the fence passes.

### Status and the overlay

- **`status.txt`:** once a second, the plugin writes it into the overlay's folder, with:
  - the mode, and the frames drawn and skipped, with the last reason;
  - the sections, blocks and vertices;
  - our GPU time and the frame time, as the average and the worst over that second;
  - the last request's result, and where each structure is.
- **`blockrender_hud`** (CET Lua) shows that, plus V's position and heading, the block V stands on in Minecraft
  coordinates, and the distance to the nearest structure.

### Tools

- **`build_atlas.py <jar> <folder>`** reads the textures of about 40 common blocks from the jar (stone, dirt, the grass
  block, planks, logs, leaves, glass, cobblestone, bricks, sand, wool, ores and so on). It builds mip levels for each
  sprite, packs them, and writes `atlas.bin` and `atlas.json` into the folder, normally the plugin's. Grass tops and
  leaves get Minecraft's default plains tint.
- **`view_dump.py <folder>`** is the dump viewer.
- Both run with `uv run --with numpy --with pillow`.

## The four steps

Each step starts at a quiet spot, after a save. The user plays. The agent sends requests and reads the logs and
dumps.

### Step 1: the frame log

**How it's run:**
1. At the main menu: `log`.
2. In free roam, standing still with the mouse left alone: `log`, then `dump all`.
3. One more `log` while turning slowly.

**Pass:**
- All four Streamline patches are in place, and the device detour saw the game's device. If it didn't, that's
  recorded, and the fallback is needed.
- Standing still, Streamline's `cameraPos` is within 0.01 m of the script camera's position, and `cameraFwd` within
  0.1° of its forward.
- Depth, motion vectors and the scaling input colour are tagged at the render resolution, and the scaling output
  colour at 2560×1440.
- The G-buffer pass is found among the candidates: the passes that draw into three or more render-resolution targets
  with the tagged depth. Where several share those targets, such as decals after the G-buffer, their order and draw
  counts tell them apart. The dumps show what each target holds. If the pass isn't found, the log shows why.
- CET's overlay still works.

**Also recorded:**
- the order of `slGetNewFrameToken`, `slSetConstants`, `slSetTag`, the G-buffer list's submission and
  `slEvaluateFeature`;
- whether the G-buffer pass and the DLSS evaluate sit at command-list boundaries;
- the depth convention: `depthInverted`, near and far;
- how `cameraFOV` and `cameraAspectRatio` relate to the script camera's vertical 68.24°;
- which barrier API the game uses;
- every command list of the frame, with its name.

### Step 2: a cube on the finished frame

**How it's run:** `mode present`, then `place cube`. Walk around the cube, turn fast, and walk behind a wall or a car.

**Pass:**
- The cube stays on the same spot of ground while you move and turn. Screenshots are checked against the overlay's
  coordinates.
- Anything nearer hides it. Edges are clean apart from some expected softness, because the depth is at render
  resolution and the image at output resolution.
- Turning shows no lag.

### Step 3: blocks before DLSS

**How it's run:**
1. `mode predlss`, then `place sampler` and `place house`.
2. Move and turn, stand still, and open the map and the pause menu.
3. At one spot and view, `place field 10000`, then `place field 100000`, for the timings.

**Pass:**
- The blocks sit under the HUD and the menus, and are tone mapped with the scene: bloom and colour grading apply to
  them.
- Standing still they don't shimmer, and moving they leave no trails, which shows the jitter and motion vectors are
  right.
- They hide, and are hidden, correctly.

**Recorded:**
- our GPU time and the frame time with no blocks, about 10,000 and about 100,000, at the same spot and view;
- the cost of the hooks alone: every mode off, against the game without the plugin.

### Step 4: blocks in the G-buffer

**How it's run:** `mode gbuffer`, at the same spots:
- by day, in sun and in a building's shadow;
- at night, under street lights, using the game's Skip Time;
- in fog or rain, if the weather allows.

**Pass:**
- The blocks are lit by the sun and darkened by the city's shadows, and lit by street lights at night.
- Fog thickens on them with distance, and their ambient occlusion and reflections look plausible.
- DLSS keeps them as steady as in step 3.
- No other pass shows artifacts: decals, materials picked by stencil, reflections or ambient occlusion.

**Recorded:** our GPU time at step 3's block counts, and screenshots from step 3 and step 4, side by side.

If the G-buffer pass can't be found, or can't be written safely, the spike ends after step 3, and the results doc
records why.

## Requests

`blockrender.request` sits in the plugin's folder. It's written atomically (to a temporary name, then renamed) and
checked once a second. Each file holds one command. Unknown commands are logged and ignored.

| Command | Does |
|---|---|
| `log` | Logs the next whole frame |
| `dump depth\|mvec\|color\|gbuffer\|all` | Dumps on the next frame |
| `mode off\|present\|predlss\|gbuffer` | Switches drawing, from the next frame |
| `place cube\|sampler\|house` | Adds that structure 3 blocks ahead of V |
| `place field <n>` | Adds rolling terrain of about n blocks |
| `clear` | Removes all blocks |
| `reload` | Recompiles the shaders |
| `set <name> <value>` | Changes a tuning value: `exposure`, `ao`, `jittersign`, `mvsign`, and the G-buffer encoding values step 1 finds |

## The frame log

`frame.json` holds:
- **`pluginVersion`, `time` and `frameIndex`.**
- **`device`:** whether the detour saw the device being created, and how many RTVs and DSVs the registry knows, plus
  how many handles came up that it didn't know.
- **`scriptCamera`:** the position, forward and field of view from the game's scripts (`PlayerLocator`), read on the
  main thread during the logged frame.
- **`streamline`:**
  - `constants`: every field of `sl::Constants`;
  - `tags`: for each tag, the buffer type and its name, the resource, its format and size, its state, lifecycle and
    extent;
  - `evaluates`: the feature and the command list.
- **`events`:** the frame's Streamline calls, submissions and present, in order, each with its thread.
- **`submissions`:** for each `ExecuteCommandLists` call, the queue and its lists. Each list has its address, its
  game name, and its passes. Each pass has:
  - its render targets and depth, as resource, format and size, with whether the depth is the tagged one;
  - its counts of draws, indirect draws, dispatches and clears;
  - its event names.
- **`gbufferCandidates`:** each pass that draws into three or more render-resolution targets with the tagged depth.

## Safety

- **Off by default.** The hooks only pass calls through until a request switches a mode on, and the frame log runs
  only on request.
  - A new mode takes effect at the start of a frame.
  - Nothing is drawn into a command list the tracker hasn't followed from its `Reset`.
- **No faults.** The game's crash handler turns any fault into a crash or a frozen game, so the plugin must never
  fault:
  - every read of game memory is range-checked with `Readable`;
  - Streamline structs are checked by type GUID, version and size before they're read;
  - COM calls go only to objects that D3D12 or Streamline handed to the plugin.
- **Checks before every draw.** The tagged buffers and the G-buffer pass must match what the frame log recorded:
  formats, sizes, and the main depth buffer. If they don't, that frame is skipped and counted. 30 skipped frames in a
  row switch the mode off, and log why.
- **Barriers.** We only move resources whose current state we know, and put each back exactly as it was.
- **If the GPU crashes.**
  - With `dred = 1`, Microsoft's DRED is switched on when the device is created. After a device-removed crash, the
    plugin logs DRED's breadcrumbs and page fault.
  - With `debuglayer = 1`, the D3D12 debug layer that the game ships is switched on. It's slow, so it's only for
    chasing an error in our own commands; our objects carry names, so its messages are easy to pick out.
- **Logging.** Render threads queue log lines, and the main thread writes them, as in spike 1.
- **Footprint.** Two folders, the plugin's and the overlay's. Deleting them removes BlockRender. Nothing goes into
  saves.
- **Builds.** A build that deploys runs only when `tasklist` shows no `Cyberpunk2077.exe`.

## Error handling

- **Streamline's imports missing at load:** logged, and nothing is installed.
- **A struct of an unexpected type or version:** ignored and counted. Drawing needs valid constants and tags for the
  frame, so without them the frame is skipped.
- **Expected buffers or the G-buffer pass missing or different:** the frame is skipped and counted, with the reason in
  the status.
- **A shader compile error:** logged, and the previous pipelines stay.
- **One of our D3D12 calls fails** (creating a resource or a pipeline, for example): logged, and the mode switches
  off.
- **The device is removed:** the plugin stops drawing, and logs `GetDeviceRemovedReason`, plus DRED's findings when
  DRED is on.
- **A bad request:** logged.
- **A file error** (logs, dumps or status): logged.

## Testing

### Unit tests (ctest, no game)

- **The mesher:**
  - one block gives 6 faces, and two neighbours give 10;
  - a face against a block in the next section is dropped;
  - glass hides faces only against glass;
  - the byte layout matches SkyCraft's.
- **The block-to-world mapping,** with the sign flip on y.
- **The camera maths:**
  - the view and projection from known Streamline values;
  - the jitter's sign and scale;
  - a static point's motion vector between two cameras.
- **Streamline's struct layouts:** the offset of every field read, against the 2.7.2 headers.
- **The patchers:** an import-table patch on the test program's own imports, and a vtable patch on a fake COM object,
  each put back afterwards.
- **`Slots.h`** against the C interface layouts in the Windows SDK.
- **The state tracker:** its restore sequence, on a fake command list that records the calls.
- **The rest:** the request parser, the DDS writer's headers, and the frame log's JSON.

### pytest

- **The atlas builder:** packing, UVs, mip levels and a missing texture.
- **The dump viewer:** decoding of each format the dumps use.

### In the game

See "The four steps". Every step also checks that CET's overlay still works, and that the game's frame time with every
mode off matches the game without the plugin.

## Risks

1. **The plugin loads after the game creates its device,** so the registry misses render targets made before then.
   **Fallback:** fill it from GpuApi's own texture table. The swap chain's back-buffer texture ID, with the back buffer
   that `GetBuffer` returns, locates that table's entries.
2. **Streamline's constants arrive after the G-buffer list is submitted,** so our blocks would be one frame behind.
   **Fallback:** read the game's own camera constants, which the G-buffer pass's draws bind as a root constant buffer.
3. **The G-buffer encoding has changed since the published analyses.** The dumps and `set` experiments settle it.
   Anything that can't be decoded is copied from a static surface of the game's next to the blocks.
4. **Mirrors and other views** render their own G-buffer passes. We only draw into passes with the main depth buffer.
5. **The game uses enhanced barriers** (`Barrier`, which Agility SDK 615 supports), or render passes. Our barriers and
   draws then use the same API. The frame log shows which.
6. **A GPU crash while developing.** Each mode is off by default, steps go one at a time, and DRED or the debug layer
   shows where it happened.
7. **Clashes with CET, Streamline, Reflex or launcher overlays.** Our patches call whatever they replaced, and every
   step checks CET's overlay.
8. **Tracking costs frame time.** The hooks run on every command list call. Step 3 measures the hooks alone, and if
   they cost noticeably, tracking is limited to the lists we draw into.
9. **A game patch** changes the renderer, the Streamline version or the Agility SDK. The frame log would show it, and
   only 2.31 is supported.
