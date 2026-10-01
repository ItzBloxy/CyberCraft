# Block render: in-game results, steps 1 to 3

Date: 2026-10-01. Game: Cyberpunk 2077 2.31 (Epic), DLSS Quality at 2560×1440 (render 1707×960), frame generation,
ray tracing, HDR and VSync off, Reflex on, on a 239 Hz display. The builds:
- **BlockRender 0.1.0** for runs 1 and 2: the plan's Tasks 1 to 27, then Task 28's fixes from the first run;
- **0.2.0** for run 3: plus Task 31's fixes from the second;
- **0.3.0** for run 4: Part 2's Tasks 35 to 43, for step 3.

The user played; the agent sent requests through `blockrender.request` and read the logs, `status.txt`, the frame
logs and the dumps. Plan: [2026-10-01-block-render.md](../superpowers/plans/2026-10-01-block-render.md).

## Verdict

- **Step 1, the frame map: pass.**
  - The plugin sees the game's device and swap chain being made.
  - Streamline's camera is the game's camera, in world coordinates.
  - The tags are the render-resolution buffers DLSS reads.
  - The frame's order is clear, and every assumption step 2 makes held.
  - The G-buffer pass is found, and its contents are dumped: base colour, world-space normals and material values,
    with a stencil that sorts the scene into classes. Its depth is not the tagged depth.
- **Step 2, a cube on the finished frame: pass.** The pipeline first treated Minecraft's counter-clockwise front
  faces as back faces. With the culling flipped live (`set cull 2`), every criterion passed, and run 3 confirmed the
  fix with the default culling:
  - the cube stays on its spot of ground while V moves and turns, with no lag;
  - anything nearer hides it, from V's hand to a parked car;
  - all 50 blocks look like Minecraft's;
  - drawing costs about 0.01 ms of GPU time.
- **Step 3, blocks before DLSS: pass.** They sit under the HUD and the menus, tone mapped with the scene. They're
  steady standing still and leave no trails moving, and 100,000 of them cost 0.03 ms of GPU time.
- **But BlockRender's hooks double the frame time, even with drawing off:** 26.5 ms with the plugin against 13.6 ms
  without it, at run 3's corner (run 4). Run 3's finding that tracking costs nothing compared two states that both
  paid this. The hooks have to be fixed before the spike can say its frame rate is acceptable.

## Runs

| Run | When | Build | What happened |
|---|---|---|---|
| 1 | 20:35 | Tasks 1 to 27 | Loading checks passed. The main menu's frame log found two problems; the game was closed |
| 2 | 20:50 | plus Task 28 | Step 1 at the main menu and in free roam, the tag dumps, then step 2 |
| 3 | 21:33 | 0.2.0: plus Task 31 | At run 2's corner: what tracking costs, a frame log, the G-buffer dump, the cube with the default culling |
| 4 | 23:29 | 0.3.0: Tasks 35 to 43 | Step 3 at the same corner and in the Badlands, then the game without the plugin |

**Run 1's two problems,** which Task 28 fixed:
- **Frame tokens:** the game calls `slGetNewFrameToken` about 8 times a frame (720 a second at 90 fps), from three
  threads, probably for Reflex's markers. The main thread asks for frame N+1 while the render and present threads
  still ask for N. Taking every call as a new frame shrank the frame log's two frames to 2 ms. A frame now starts
  only with a newer frame index: 29,355 calls made 3,594 frames at the menu.
- **The present queue:** the game's swap chain doesn't say which queue it presents on (`GetDevice` fails), so the
  hooks now note it when the swap chain is made.

**Run 2's fixes,** made by Task 31:
- the pipeline's culling (see step 2);
- the G-buffer candidate rule (see the G-buffer);
- tracking runs only while a log or dump runs, or on request.

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

Task 31 changed the rule: it finds the pass by its targets and depth at render resolution. The dump now keeps one copy
per set of targets, recorded again after each piece, so the last copy holds the complete G-buffer.

## The G-buffer's contents

**Run 3's frame log** (`logs\2026-10-01_21-37-08`) found 36 candidate lists over two frames. Each frame sends them to
the main queue in 3 submissions (10, 7 and 2 lists), within about 1 ms, long before the DLSS submission.

**The dump** (`dumps\2026-10-01_21-37-20`): 14 files, no errors, V standing still.
- **The copies:** the dumped frame recorded them after 13 passes that draw the three targets, 4 that also draw a
  motion target, and 1 that draws the second decal set.
- **Complete:** the three-target set and the four-target set are the same resources, and their last copies came out
  byte for byte the same. So the copies ran after the last write, and no target is partly drawn.
- **The second decal set** (`R16G16_SNORM`, `R8_UNORM`, `R8_UNORM`) was all zero in this view.

| Target | Format | Holds | Where nothing is drawn (the sky) |
|---|---|---|---|
| 0 | `R10G10B10A2_UNORM` | Base colour in RGB. It looks gamma-encoded: shown as it is, it looks right, and the asphalt reads 0.34 (0.09 in linear terms, about real asphalt's albedo). A is 0 everywhere drawn | RGB 0, A 1 |
| 1 | `R10G10B10A2_UNORM` | The normal, in world space, as n × 0.5 + 0.5: x east, y north, z up. The ground reads (−0.02, 0.00, 1.00), and the median length is 1.00. A is 1 on screens and signs (3,540 px), 0 elsewhere | (0.5, 0.5, 1): straight up; A 0 |
| 2 | `R8G8B8A8_UNORM` | Material values. R is likely metalness: 0 on ground, plants and people, 0.26 on the car. G is likely roughness: 0.85 on dirt, 0.72 on the road, 0.37 on car paint. B is 0.33 on most surfaces, but higher on plants (0.63 to 0.86) and skin (1.0): likely a translucency or subsurface amount. A is above 0 only on windows and signs (up to 0.93): likely emission | all 0 |
| 3 (four-target set only) | `R16G16_FLOAT` | Motion: the walking pedestrian's own, and a small, smooth motion everywhere else, sky included (under 6e−5) | |
| depth | `D32_FLOAT_S8X24_UINT` | Reversed: 1.5e−6 to 0.009 where drawn, so 13.5 km to 2.2 m away | 0 |

**The stencil** sorts the scene into classes:

| Value | Pixels | What it covers |
|---|---|---|
| 0x00 | 378,884, plus the sky | buildings, billboards and most props |
| 0x01 | 29,234 | bare dirt and sand beside the roads |
| 0x02 | 674,947 | the roads, and the overpass |
| 0x14 | 21,098 | poles, street lamps and bollards |
| 0x15 | 1,867 | people |
| 0x18 | 1,766 | vehicles |
| 0x95 | 37 | small parts of people, with material B at 1: likely skin |
| 0xA0 | 189 | small plants |
| 0xB6 | 23,844 | trees and bushes |

The three values with the top bit set (0x95, 0xA0, 0xB6) are the ones with a high material B: skin and plants. So
the top bit likely marks translucent or subsurface shading.

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
- **Confirmed in run 3:** with the fix and the default culling (`cull 1`), the cube looked solid from two sides
  (headings 220° and 137°), at 0.008 to 0.010 ms of GPU time.

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
- **The hooks' tracking** was on from the first frame log to the end of run 2, and free roam sat at 30 to 38 fps,
  below the usual 40 to 80 in the city. Run 3 measured it at the same corner, five readings a second apart each:
  - tracking off: 26.5 ms;
  - on: 26.4 ms;
  - off again: 25.9 ms.

  That is about 38 fps throughout, so turning tracking on adds nothing measurable on top. But run 4 found that it
  isn't the whole story: without BlockRender, the same spot runs at 13.6 ms (73.5 fps). The hooks cost about 13 ms
  even with tracking off, and that is why this spot ran slowly (see "What the hooks cost").

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
- **The G-buffer's normals are in world space, z up.** Step 4's blocks can write their face normals as they are, with
  no view transform.
- **The base colour is sRGB-encoded,** which run 4's dump confirmed (see step 3's results). Step 4 can write
  Minecraft's texture colours as they are.
- **The stencil sorts the scene into classes,** which the lighting likely reads. Step 4 picks a class for blocks,
  starting with 0x00, the buildings' class.
- **Motion seems filled in for the whole screen,** sky included, so step 4's static blocks may need no motion of
  their own. Run 4 found that the G-buffer passes with a fourth target draw into the tagged motion vector buffer
  itself. Step 4 checks.

## Step 3: blocks before DLSS (run 4)

BlockRender 0.3.0, at run 3's corner (x −116.6, y −2005.2, facing south-west), then in the Badlands, by day and at
night. `predlss` puts a list of ours into the game's submission just before the DLSS list. It draws into the tagged
colour, motion vectors and depth, with the frame's jitter.

**The states before the DLSS list,** from the first frame log, in both frames' DLSS lists:

| Tagged buffer | Before the DLSS list | Where that comes from |
|---|---|---|
| Scaling input colour | 0x8 (unordered access) | the DLSS list's first transition starts there |
| Motion vectors | 0xC0 (shader resource) | the list never moves it: its tagged state |
| Depth | 0xC0 (shader resource) | the first transition starts there; the list ends it in 0x40 |
| Bias colour hint | 0x8 | the first transition starts there |

All are plain states, with no subresource or split barriers, so `predlss` always knew where to start. It drew every
frame DLSS ran, and skipped none.

**What the game settled live:**
- **`exposure` 2.5:** the user compared 1.5, 2, 2.5, 3, 6 and 12 against the sunlit pavement. 3 was close, and 2.5
  looked the most natural.
- **`jittersign` 1,** as built: its edges were the smoothest, standing still.
- **`mvsign` 1,** as built: no trails while strafing past the cube. The other signs left them.

| Criterion | Seen |
|---|---|
| Under the HUD and the menus | ✅ the HUD's icons and the minimap draw over the blocks; the weapon wheel and the pause menu sit on top, and the pause menu's blur blurs the blocks with the scene |
| Tone mapped with the scene | ✅ at `exposure` 2.5 |
| Steady standing still, no trails moving | ✅ with both signs at 1 |
| Hiding and hidden | ✅ grass and bushes hide the base row, V hides it in third person, and a car driven over the row hides the blocks under it |
| Cut-outs | ✅ glass shows the desert through its frame, and the leaves have clean holes |
| The house | ✅ log corners, plank walls, windows on every side, the doorway, the floor, the roof and the bush; placed onto the sampler row, it replaced the blocks it overlapped |
| Full-screen menus | the game runs no DLSS behind them, so nothing is drawn, and nothing counts as skipped |

**Not part of step 3:**
- **No lighting:** the blocks have no shadows, and at night they keep their daytime brightness.
- **No fog:** they're drawn after it.

Step 4's G-buffer drawing is what lets the game light them.

**The timings** (`predlss` on, the same view, five readings a second apart each):

| Blocks | Sections | Vertices | Our GPU time | Frame time (the plugin's / CET's) |
|---|---|---|---|---|
| none | 0 | 0 | about 0 ms | 26.6 / 26.5 ms |
| 10,003 | 12 | 31,116 | 0.028 to 0.029 ms | 26.1 / 26.1 ms |
| 100,001 | 81 | 267,432 | 0.031 to 0.033 ms | 26.1 / 25.9 ms |

The measured view had a stone face of the field filling most of the screen, a near worst case for fill. Drawing even
100,000 blocks costs nothing measurable in frame time: the mesher drops hidden faces, and whole sections outside the
view are culled.

### What the hooks cost

CET's own frame time, at the corner, standing still, five readings each:

| BlockRender | Frame time |
|---|---|
| loaded, drawing off, tracking off | 26.5 ms (37.7 fps) |
| removed (its DLL renamed) | **13.6 ms (73.5 fps)** |

- **Not an artefact:** VSync is off, and the display runs at 239 Hz, so this is no frame-rate quantisation.
- **It roughly doubles the frame time,** even with drawing off.
- **It explains earlier runs:** run 2's free roam at 30 to 38 fps, below the usual 40 to 80. It also means run 3's
  comparison of tracking on and off compared two states that both paid it, so tracking's own cost is unknown.

The likeliest cause: every hooked command list call, about 10,000 a frame from up to 12 recording threads, looks its
list up under a shared lock, even with tracking off. A list that was never tracked misses every time, so the lookup
never uses its per-thread cache. That alone may not add up to 13 ms, and the device's view hooks run often too, so
the next step measures each group of hooks before fixing.

### For step 4

- **The base colour is sRGB-encoded.** Sunlit ground facing up gets the same light in every pixel. Its pre-DLSS colour
  divided by its base colour varies least when the base colour is sRGB-decoded:

  | Decoding | Spread (std of log) | Slope of colour against base colour |
  |---|---|---|
  | as stored | 0.156 | 1.44 |
  | sRGB-decoded | 0.099 | 0.91 |
  | gamma 2.2 | 0.107 | 0.86 |

- **The game's light on that ground is about 9 times its linear base colour** (through its exposure). Step 3's blocks
  use 2.5, so blocks the game lights in step 4 will be brighter than step 3's, as Minecraft's colours are bright for
  real surfaces.
- **The G-buffer lists' states:**
  - most lists don't move their targets at all;
  - the G-buffer depth goes between 0xE0 (read) and 0x10 (write);
  - the second target (normals) moves one subresource at a time, so it likely has mip levels;
  - the motion vector buffer goes from 0x40 to a render target in the list that first draws into it.
