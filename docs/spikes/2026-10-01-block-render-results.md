# Block render: in-game results, Part 1 (steps 1 and 2)

Date: 2026-10-01. Game: Cyberpunk 2077 2.31 (Epic), DLSS Quality at 2560×1440 (render 1707×960), frame generation,
ray tracing and HDR off. BlockRender 0.1.0: the plan's Tasks 1 to 27, then Task 28's fixes from the first run. The
user played; the agent sent requests through `blockrender.request` and read the logs, `status.txt`, the frame logs
and the dumps. Plan: [2026-10-01-block-render.md](../superpowers/plans/2026-10-01-block-render.md).

## Verdict

- **Step 1, the frame map: pass, except the G-buffer dump, which is still to come.**
  - The plugin sees the game's device and swap chain being made.
  - Streamline's camera is the game's camera, in world coordinates.
  - The tags are the render-resolution buffers DLSS reads.
  - The frame's order is clear, and every assumption step 2 makes held.
  - The G-buffer pass is found and its layout is known, but the candidate rule missed it: the tagged depth is not
    the G-buffer's depth. The dump of its contents waits for that fix.
- **Step 2, a cube on the finished frame: pass, once one setting is fixed.** The pipeline treated Minecraft's
  counter-clockwise front faces as back faces. With the culling flipped live (`set cull 2`), every criterion passed:
  - the cube stays on its spot of ground while V moves and turns, with no lag;
  - anything nearer hides it, from V's hand to a parked car;
  - all 50 blocks look like Minecraft's;
  - drawing costs about 0.01 ms of GPU time.

## Runs

| Run | When | Build | What happened |
|---|---|---|---|
| 1 | 20:35 | Tasks 1 to 27 | Loading checks passed. The main menu's frame log found two problems; the game was closed |
| 2 | 20:50 | plus Task 28 | Step 1 at the main menu and in free roam, the tag dumps, then step 2 |

**Run 1's two problems,** which Task 28 fixed:
- **Frame tokens:** the game calls `slGetNewFrameToken` about 8 times a frame (720 a second at 90 fps), from three
  threads, probably for Reflex's markers. The main thread asks for frame N+1 while the render and present threads
  still ask for N. Taking every call as a new frame shrank the frame log's two frames to 2 ms. A frame now starts
  only with a newer frame index: 29,355 calls made 3,594 frames at the menu.
- **The present queue:** the game's swap chain doesn't say which queue it presents on (`GetDevice` fails), so the
  hooks now note it when the swap chain is made.

## Loading

- **Load order:** RED4ext loaded BlockRender 13 s before the game made its D3D12 device (20:50:16 against 20:50:29),
  so the detours saw the device and both swap chains being made. Risk 1 doesn't happen.
- **The device:** one, on the NVIDIA GeForce RTX 3070. The main queue (the queue DLSS's list goes to) belongs to it.
- **Swap chains:**
  - the game's, made with `CreateSwapChainForHwnd`, presents on the main queue;
  - one more, made with `CreateSwapChain`, has no D3D12 queue (likely a D3D11 device's), and never presented in the
    logs.
- **GpuApi:** `GetDeviceData()`'s device is the hooks' device, so RED4ext.SDK's GpuApi offsets hold for 2.31. Its
  device and queue are native, and its swap chain is Streamline's proxy.
- **Streamline's mode:** the game hands Streamline native command lists, so Streamline runs in manual mode, while the
  swap chain is a Streamline proxy. After an evaluate, the game itself restores the list's state.
- **CET:** its console and both overlays (BlockRender's and the PhysX probe's) work.

## The camera

| What | Found |
|---|---|
| `cameraViewToClip` | for row vectors; view +z forward, +x right, +y up; no jitter in it |
| Depth | reversed (`depthInverted` true), near 0.02 m, far 16,000 m: P[2][2] = −1.19e−6, P[3][2] = 0.02 |
| Field of view | `cameraFOV` = 68.238, in **degrees**, though Streamline documents radians. It matches the script camera's vertical 68.238°, and the matrix's P[1][1] = 1.4759 |
| Aspect | 1.7778 |
| Jitter | about ±0.5 px a frame, given apart from the matrix, as Streamline asks |
| Motion vectors | `mvecScale` (1, 1); camera motion included; not 3D, dilated or jittered |
| Position | world coordinates: at (−114.8, −2005.5, 7.7), Streamline's camera and the script camera differ by 0.1 mm and 0.0045°. So no render origin, and step 1's grid-line check wasn't needed |
| Timing | a frame's constants equal the script camera the main thread read for the same frame index |

## The tags

All global, all `validUntilPresent`, with extents covering the whole resource. There is no exposure tag.

| Type | Format | Size | Flags | State at the evaluate |
|---|---|---|---|---|
| depth | `R32_TYPELESS` (D32 depth, no stencil) | 1707×960 | depth-stencil | 0xC0 (shader resource) |
| motionVectors | `R16G16_FLOAT` | 1707×960 | render target | 0xC0 |
| scalingInputColor | `R16G16B16A16_FLOAT` | 1707×960 | render target, UAV | 0x8 (UAV) |
| biasCurrentColorHint | `R8_UNORM` | 1707×960 | render target, UAV | 0x8 |
| scalingOutputColor | `R16G16B16A16_FLOAT` | 2560×1440 | render target, UAV | 0x8 |

**The tag dumps** (free roam, standing still, `dumps\2026-10-01_20-56-08`):
- the input colour is the HDR scene before DLSS, with no HUD;
- the depth is reversed, with the nearest ground about 2 m away;
- the motion vectors are tiny, because V stood still.

The copies, recorded into the game's own list at the evaluate, went through cleanly.

## The frame

- **Threads:** the main thread starts frame N. A worker thread then sets the constants and tags for N−1, and
  evaluates DLSS on a direct list. That list goes to the main queue about 0.5 ms later, and `Present` follows about
  0.2 ms after that. The main thread starts N+1 right after `Present`. The worker threads change from frame to frame.
- **Queues:**
  - the main (direct) queue: 14 submissions a frame;
  - a compute queue: 3 a frame;
  - a copy queue: now and then.
- **Lists:** about 67 a frame, all tracked. The game's names for them came out empty. The game emits no PIX event
  names.
- **Barriers:** legacy only (`ResourceBarrier`), and no render passes.
- **The DLSS evaluate** has a short list of its own (2 passes, 2 dispatches, 33 to 45 barriers). It is the 4th of 9
  lists in the frame's last main-queue submission.

## The G-buffer

Over two frames of free roam:

| Pass | Targets (all 1707×960) | Depth | Draws |
|---|---|---|---|
| Depth pre-pass | none | `D32_FLOAT_S8X24_UINT` | 514 |
| **G-buffer** | `R10G10B10A2_UNORM`, `R10G10B10A2_UNORM`, `R8G8B8A8_UNORM` | `D32_FLOAT_S8X24_UINT`, written | 3,617 |
| G-buffer with motion | the same three, plus `R16G16_FLOAT` | the same | 863 |
| Decals | the same three | the same, read-only | 810 |
| More decals | `R16G16_SNORM`, `R8_UNORM`, `R8_UNORM` | the same, read-only | 282 |
| Forward and transparent | the scaling input colour, a null target, `R8_UNORM` | **the tagged `D32_FLOAT`**, mostly read-only | 1,114 |
| Shadow cascades | none | `D16_UNORM` 4096×4096 | 3,433 |

- **The layout** is the published one: albedo, normals and material values in the first three targets, plus motion
  vectors for some objects.
- **Spread over lists:** the G-buffer pass is recorded in parallel, as 12 lists, submitted in two batches (8 lists,
  then 4).
- **Two depth buffers:** the G-buffer's depth (`D32_FLOAT_S8X24_UINT`) is not the tagged depth. The tagged
  `D32_FLOAT` serves the forward passes into the pre-DLSS colour, and DLSS.
- **Why no candidates:** the candidate rule required the tagged depth, so it found none.
- **What the dump needs:** a dump that keeps the first 4 pieces would also miss most of the pass.

The next fix finds the pass by its targets and depth at render resolution, and keeps one copy per set of targets,
overwritten by each piece, so the dump holds the complete G-buffer.

## The gates for step 2

| Gate | Met |
|---|---|
| The swap chain presents on the main queue | ✅ |
| The DLSS evaluate's list is a direct list, and its constants arrive | ✅ |
| The tagged depth is a 32-bit depth with a state | ✅ `R32_TYPELESS`, 0xC0 |
| Legacy barriers only | ✅ |
| No render origin needed | ✅ |

## Step 2: a cube on the finished frame

**The setup:** `mode present` (atlas 256×64 with 5 levels and 50 blocks; the renderer ready; the swap chain on the
main queue), then `place cube` at Minecraft (−119, 6, 2008).

**The culling bug.** The first look showed the inside of the box: the faces toward the camera were culled, the far
ones drawn.
- **The mesh was right:** the mesher winds faces counter-clockwise seen from outside, like Minecraft, and the winding
  test confirms they come out counter-clockwise on screen.
- **The assumption was wrong:** the test assumed the render target's flipped y turns that into Direct3D's default
  front face (clockwise). Flipping a coordinate axis doesn't change how a triangle winds on screen.
- **The proof:** `set cull 2` culls the other side, and showed a solid block at once.
- **The fix:** the pipeline needs `FrontCounterClockwise = TRUE`.

| Criterion | Seen |
|---|---|
| It stays on its spot while V moves and turns | ✅ the same spot from every side, in third person from a car too; "stays consistent" while moving and turning fast |
| Anything nearer hides it | ✅ V's hand and pistol, a parked car's body, bushes and power lines; it hides the people behind it |
| Clean edges | ✅ |
| No lag when turning | ✅ none seen; no late cameras in play |

**The sampler** (`place sampler`, all 50 blocks on a stone base):
- the textures look right: grass tops and leaves tinted, dirt-sided grass blocks, distinct log ends, wool, planks and
  ores;
- glass and leaves are cut out;
- each face's brightness looks right.

**The numbers:**
- **Draws:** one per `Present`, 31 to 39 a second, none skipped.
- **Late cameras:** 0 in play. In one second, most likely while the game was in the background, it presented 76
  times with only 7 new DLSS frames (69 late cameras). The world was paused then, so the old camera was the right one.
- **GPU time:** 0.004 ms with nothing to draw, 0.01 ms for the cube, and 0.01 to 0.02 ms for 151 blocks (8 sections,
  3,660 vertices).
- **Frame time:** 26 to 33 ms with drawing on, and 27 to 31 ms with it off, so our drawing costs nothing measurable.
- **The hooks' tracking** has been on since the first frame log, and free roam sat at 30 to 38 fps, below the usual
  40 to 80 in the city. The next fix turns tracking on only while a log or dump runs, so the next run can
  measure what it costs.

**As step 2 expects,** blocks drawn at `Present` cover the HUD, the menus, the weapon wheel and CET's windows. Step 3
draws them before DLSS, under the UI.

## Surprises, and what they mean for steps 3 and 4

- **Frame tokens arrive about 8 times a frame, from three threads.** Anything that keys on frames must use the newest
  frame index.
- **`cameraFOV` is in degrees.** Nothing reads it: the camera comes from the matrices.
- **DLSS doesn't run at the main menu,** though the menu's 3D scene is rendered.
- **The tagged depth is a copy, not the G-buffer's depth.** Step 4 draws into the G-buffer's own
  `D32_FLOAT_S8X24_UINT`, with its stencil. Step 3 draws before DLSS, and must test and write the tagged `D32_FLOAT`,
  which the forward passes use too.
- **The G-buffer pass is spread over 12 lists.** Drawing once at the end of the pass means after its last list.
  Step 3's planning settles that point, from the submission order.
- **Streamline runs in manual mode,** so after an evaluate the game restores the list's state itself. Anything step 3
  records at the evaluate must restore what it changes.
- **The DLSS evaluate has a short list of its own,** fourth in the frame's last submission, so a list of ours can
  slot in just before it.
- **The scaling input colour and the motion vectors are render targets** (the colour is also a UAV), at the same
  1707×960 as the tagged depth. Step 3 can draw into them directly, so risk 11 doesn't happen.
