# Block render: in-game results, steps 1 to 4

Date: 2026-10-01. Game: Cyberpunk 2077 2.31 (Epic), DLSS Quality at 2560×1440 (render 1707×960), frame generation,
ray tracing, HDR and VSync off, Reflex on, on a 239 Hz display. The builds:
- **BlockRender 0.1.0** for runs 1 and 2: the plan's Tasks 1 to 27, then Task 28's fixes from the first run;
- **0.2.0** for run 3: plus Task 31's fixes from the second;
- **0.3.0** for run 4: Part 2's Tasks 35 to 43, for step 3;
- **0.4.0** for run 5: plus Tasks 45 and 46, to find what the hooks cost;
- **0.5.0** for run 6: plus Task 48's fix;
- **0.6.0** for run 7: plus Task 50, which tracks every list again while drawing before DLSS;
- **0.7.0** for run 8: Part 3's Tasks 53 to 56, for step 4;
- **0.8.0** for run 9: plus Task 58, which lets menus be and gives the overlay buttons for the drawing modes.

The user played; the agent sent requests through `blockrender.request` and read the logs, `status.txt`, the frame
logs and the dumps. Plan: [2026-10-01-block-render.md](../superpowers/plans/2026-10-01-block-render.md).

## Verdict

**The spike's answer: yes, except for shadows.** Night City can show Minecraft's blocks at the scale a player builds,
at an acceptable frame rate and with a believable look. Drawn into the game's G-buffer, 100,000 blocks cost 0.06 ms
of GPU time and no frame time, and the game lights, hazes and shades them as its own geometry, steady under DLSS.
They cast no shadows yet, and block no light: they aren't in the game's shadow maps. The plan's "How Part 3 went"
says what a SkyCraft for Cyberpunk would build on, and what it still has to solve.

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
  steady standing still, leave no trails moving, and don't flicker turning fast (run 7). 100,000 of them cost
  0.03 ms of GPU time.
- **Step 4, blocks in the G-buffer: pass, except shadows.** The game lights the blocks as its own, in the sun, in a
  building's shadow, at sunset and at night, and its haze and ambient occlusion fall on them (runs 8 and 9). They're
  steady, leave no trails and don't flicker turning fast, writing their own motion vectors and tagged depth, which
  both turned out to be needed. 100,000 of them cost 0.06 ms of GPU time, and the frame time doesn't rise. But they
  cast no shadows, and block no light: they aren't in the game's shadow maps, so a house's floor is sunlit through
  its roof.
- **BlockRender costs nothing measurable, after a fix.**
  - **Run 4:** the hooks doubled the frame time with drawing off: 26.5 ms against 13.6 ms without BlockRender.
  - **Run 5 found the cause:** the Streamline hooks' readability checks, whose `VirtualQuery` walked gigabytes of the
    game's heaps.
  - **Run 6, after the fix:** 13.5 ms idle. Tracking every list and drawing before DLSS add about 0.1 to 0.2 ms.

## Runs

| Run | When | Build | What happened |
|---|---|---|---|
| 1 | 20:35 | Tasks 1 to 27 | Loading checks passed. The main menu's frame log found two problems; the game was closed |
| 2 | 20:50 | plus Task 28 | Step 1 at the main menu and in free roam, the tag dumps, then step 2 |
| 3 | 21:33 | 0.2.0: plus Task 31 | At run 2's corner: what tracking costs, a frame log, the G-buffer dump, the cube with the default culling |
| 4 | 23:29 | 0.3.0: Tasks 35 to 43 | Step 3 at the same corner and in the Badlands, then the game without the plugin |
| 5 | 00:18 | 0.4.0: plus Tasks 45 and 46 | At the same corner: the hook groups switched off in turn found the Streamline hooks' cost |
| 6 | 00:30 | 0.5.0: plus Task 48 | At the same corner: what BlockRender costs after the fix, step 3 again up to 100,000 blocks, and a flicker turning fast |
| 7 | 00:56 | 0.6.0: plus Task 50 | At the same corner: drawing before DLSS with every list tracked, turning fast, then four frame logs |
| 8 | 01:50 | 0.7.0: Tasks 53 to 56 | Step 4 at the same corner, by day and at night; the Skip Time screen switched drawing off |
| 9 | 02:15 | 0.8.0: plus Task 58 | Step 4 again: the overlay's buttons, the Skip Time screen, the side-by-side, the stencil, the motion and depth switches, fast turns, shadow and sun, the house and the timings |

Runs 5 to 9 were after midnight, on 2026-10-02.

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

The likeliest cause looked like this: every hooked command list call, about 10,000 a frame from up to 12 recording
threads, looks its list up under a shared lock, even with tracking off. A list that was never tracked misses every
time, so the lookup never uses its per-thread cache. That alone might not add up to 13 ms, and the device's view hooks
run often too, so Tasks 45 and 46 added a switch for each group of hooks, and removed the lookup while nothing is
tracked.

### Run 5: where the cost was

BlockRender 0.4.0, at the same corner, with the same save. Idle, it still took 26.1 ms, so the lookup wasn't the cost.
Switching each group off in turn, five readings each (CET's frame time):

| Group switched off | Frame time |
|---|---|
| `lists` (command lists) | 25.1 to 26.4 ms |
| `views` (view creation and copies) | 25.3 to 26.3 ms |
| `submit` (`ExecuteCommandLists`) | 26.2 to 35.3 ms |
| `present` | 25.8 to 26.9 ms |
| **`sl` (the four Streamline imports)** | **14.2 to 14.6 ms** |

**The cause is the Streamline hooks' readability checks.**
- **What they do:** before reading a struct the game hands Streamline, `ReadableMemory` asks `VirtualQuery` whether
  its memory can be read.
- **Why that's slow:** `VirtualQuery` reports the size of the region an address lies in, and finds it by walking the
  region's pages. Measured here, an address in the middle of a committed, touched region takes 0.27 ms in 256 MB,
  and 1.54 ms in 1 GB. `QueryWorkingSetEx` answers for one page in 0.0013 ms, whatever the region's size.
- **Where it adds up:** the game's `sl::Resource` structs live in heaps of gigabytes. The frame logs show its five
  `slSetTag` calls a frame about 2.3 ms apart, on the thread that records the DLSS evaluate. That's about 12 ms on
  the frame's critical path.

**Tracking's own cost, measured with `sl` off:**
- tracking off: 14.1 ms;
- on: 14.4 ms;
- off again: 14.6 ms.

So tracking every list costs about 0.3 ms, within the noise.

Run 5 skipped its measurements of drawing before DLSS and of 10,000 blocks, since the Streamline hooks' cost would
have swamped them. The next task fixes the checks, and run 6 measures again.

### Run 6: after the fix

BlockRender 0.5.0 (Task 48: `QueryWorkingSetEx` for pages in the working set), at the same corner, with the same save.

| State | Frame time (CET's) |
|---|---|
| idle, all hooks on | **13.5 ms (74 fps)**, against 13.6 ms without BlockRender and 26.5 ms before the fix |
| tracking every list | 13.6 ms |
| drawing the cube before DLSS | 13.5 to 13.6 ms, 74 frames drawn a second |

**A flicker, from the watched lists.**
- **What the user saw:** standing still, the cube was steady. When spinning or moving fast, it vanished for single
  frames.
- **What the status showed:** sampled several times a second, 0 to 2 frames a second were skipped, as "the DLSS list
  wasn't tracked from its Reset", with the watch full at 4 lists.
- **Why:** the game rotates through more DLSS command lists than the 4 watched. A list that drops out of the watch is
  recorded untracked the next time round, so that frame has no starting states and is skipped.
- **Tried live:** with `set tracking 1`, every list tracked, 15 seconds of fast spinning skipped none, and the cube
  didn't flicker.

So drawing before DLSS goes back to tracking every list (the next task). In the same conditions, back to back, the
game's own time had by then drifted up to 14.6 ms:

| State | Frame time (CET's) |
|---|---|
| drawing off, tracking off | 14.6 ms |
| drawing off, tracking on | 14.6 ms |
| the cube before DLSS, tracking on | 14.7 ms |
| the cube before DLSS, the watch only | 15.1 ms |
| the cube before DLSS, tracking on again | 14.8 ms |

Drawing before DLSS with every list tracked costs about 0.1 to 0.2 ms in all.

**Blocks before DLSS,** with every list tracked: the GPU time is 0.009 ms for the cube, 0.028 ms for 10,003 blocks,
and 0.030 ms for 100,001 blocks (81 sections, 267,432 vertices). The frame time stays at 14.5 to 15.0 ms throughout.
Every frame was drawn, and none skipped.

### Run 7: every list tracked

BlockRender 0.6.0 (Task 50: drawing before DLSS tracks every list again, and the watch is gone), at the same corner,
with the same save.

| State | Frame time (CET's) |
|---|---|
| idle | 14.5 to 15.1 ms |
| the cube before DLSS, every list tracked | 14.4 to 14.8 ms, 68 to 70 frames drawn a second, none skipped |

The game's own time was already at run 6's later level, so drawing is compared with this run's idle: it costs
nothing measurable. The cube's GPU time is 0.007 ms.

**Turning fast, no flicker.**
- **What the status showed:** recorded twice a second for three minutes (269 readings) while the user turned fast,
  sprinted and strafed. No frame was skipped, and none had a late camera. Frames took 11 to 16 ms averaged over a
  second, and single frames up to 31 ms.
- **What the user saw:** no flicker at all. The cube was steady with no trails, its edges sharp and clean, and the
  pistol in V's hand drew over it.

**Four frame logs** followed, with drawing off: one taken during a fight with the police, then three standing at the
corner. "For step 4" has what they show.

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
- **The camera is known before the G-buffer is submitted.** In all eight frames of run 7's frame logs, Streamline's
  constants and the DLSS evaluate came 2.0 to 3.1 ms before the G-buffer's first submission. In run 4's log, with
  the slow Streamline hooks, they came 5.3 ms after it in one of its two frames. The game doesn't wait for them, so
  it's a race the camera wins by about 2 ms: step 4 must count the frames it loses.
- **Where the G-buffer ends,** the same in every frame logged, on the main queue:
  1. a 10-list submission: its first list makes the targets render targets (0x4) and the depth writable (0x10),
     then 9 lists draw the G-buffer;
  2. a 7-list submission draws the rest, and leaves the depth writable;
  3. a 1-list submission moves the normals, material values, motion and depth to 0x40 (non-pixel shader
     resource). The base colour stays a render target.

  Just before that 1-list submission, every target is a render target and the depth is writable, so a list of ours
  there needs no barriers.
- **Who reads it next:**
  - **the second queue** submits a list 0.08 to 0.12 ms after the move, in every frame logged: likely async compute
    reading the G-buffer;
  - **an 11-list submission** draws without touching the G-buffer: likely shadow maps;
  - **a transition list** moves every target to 0xC0 and the depth to 0xE0. The list after it, with 47 to 57
    passes and 36 to 42 dispatches, is likely the lighting.

  So blocks must be in the G-buffer before the 1-list submission, for the second queue's reads too.

## Step 4: blocks in the G-buffer (run 8)

BlockRender 0.7.0 (Tasks 53 to 56), at run 3's corner, with the same save. `gbuffer` puts a list of ours just before
the list that ends the G-buffer. It draws the blocks into the G-buffer's targets, with their stencil class, then into
the tagged depth.

| State | Frame time (CET's) |
|---|---|
| idle | 13.3 to 13.6 ms |
| the cube in the G-buffer, by day | 12.8 to 13.4 ms, 75 to 79 frames drawn a second, none skipped, no late camera |
| the same, at night | 69 to 70 frames drawn a second, none skipped |

Our GPU time is 0.014 ms by day and 0.024 ms at night, for both passes.

**What the user saw:**
- **The game lights the cube.** Its top is bright in the sun and its sides are in shade, like the scene around it,
  where step 3's cube was flat and evenly bright. At night the user found it lit as properly.
- **It sits a few centimetres above the road.** Blocks sit on Minecraft's whole-metre grid, so the cube's bottom is at
  z 6.0, while the road beside the curb, where V stood at z 6.14, is lower. The ground under the cube darkens: most
  likely the game's ambient occlusion, which then sees the cube's depth.

**The Skip Time screen switched drawing off,** twice.
- **While it opens,** the game still draws the world for a moment, but without its copy into the tagged depth before
  the G-buffer's end, so the tagged depth's state there isn't known. Each such frame was skipped, and 30 in a row
  switch drawing off.
- **With the screen open,** a frame log shows no DLSS evaluate and no G-buffer: nothing to draw into.

Run 8 stopped there for the next task's fix:
- with only the tagged depth's state unknown, the blocks still go into the G-buffer, leaving the tagged depth;
- frames that the game draws without a G-buffer, its end or the frame's camera are skipped, but don't count toward
  the 30 that switch drawing off;
- the overlay gets buttons for the drawing modes.

### Run 9: step 4, with menus that don't stop it

BlockRender 0.8.0 (plus Task 58), at the same corner, with the same save.

**The fix.**
- **The overlay's buttons:** the user clicked G-buffer, and drawing was on within a second.
- **The Skip Time screen:** 91 readings over a minute, while the user opened and closed it and skipped an hour.
  Drawing stayed on, and the cube was there and lit after the skip. As the screen opened and closed, 1 or 2 frames a
  second were skipped, because the base colour's state before the G-buffer's end wasn't known in them. Those were
  scattered among drawn frames, so they never added up to the 30 that switch drawing off.

**Step 3 and step 4, side by side,** from the same view by day. Before DLSS, the cube has Minecraft's flat face shading
and looks pasted on. In the G-buffer, the game's sun lights its top, its sides fall into soft shade, and the ground
darkens where it meets the curb. The user's word: "holy improvement".

**The settings settled live:**
- **`stencil`:** 0, 1, 2, 20 and 24 looked the same. 182, the trees' class, changed the cube at some angles: likely
  foliage's light through leaves, which might suit leaf blocks later. Blocks stay at 0, the buildings' class.
- **`gbmotion`:** strafing past the cube, with everything on: no trails. With `gbmotion 0`, subtle trails: the
  camera's motion that the game fills in before our list is the ground's, not the blocks'.
- **`gbdepth`:** with `gbdepth 0`, heavy ghosting: a car driving behind the cube showed through it, since DLSS's depth
  didn't have the cube. Both stay on.

**Turning fast:** 183 readings over two minutes of fast turns, sprinting and strafing. No frame was skipped, none had a
late camera, and none went without the tagged depth. Frames took 11 to 17 ms averaged over a second.

**The light:**
- **In a building's shadow,** the cube takes the shade's cool sky light, like the ground and the wall beside it.
- **In the low sun,** its top catches the warm light like the road, and the faces turned away take the sky's blue.
- **No shadows:** the blocks cast none, and block no light. Inside the house the floor is sunlit through the roof,
  while the walls' inner faces are dim only because they face away from the sun. The game's shadow maps are drawn from
  its own geometry, likely in the 11-list submission after the G-buffer, and the blocks aren't in them.
- **The grid:** blocks sit on Minecraft's whole metres, so on the game's ground they float a few centimetres or sink up
  to 0.6 m, and the ground hides what's sunk.

**The house:** 176 blocks, lit like the street: planks, logs and cobblestone, glass frames to see through, and the
bush's leaves cut out cleanly.

**The timings** (at the corner, the same view as step 3's, five readings each):

| Blocks | Sections | Vertices | Our GPU time | Frame time (the plugin's / CET's) |
|---|---|---|---|---|
| none | 0 | 0 | 0.011 ms | 14.7 / 14.7 to 14.8 ms |
| 10,003 | 12 | 31,116 | 0.056 to 0.094 ms | 12.3 to 12.9 ms |
| 100,001 | 81 | 267,432 | 0.059 to 0.060 ms | 11.8 ms (85 fps) |

Two passes cost about twice step 3's one. The frame got faster with the field, which fills much of the view: its depth
likely spares the game's later passes the scene behind it.

**At this scale,** the user found that the 100,000-block field, surprisingly, at times looks like it belongs. Its
terraces take the evening sun and haze like the city around them, and the game's rooftops and machinery pass
through the terrain where the depths cross.

Clean up: `mode=off`, tracking off, no errors in the log, and no safety stop in the whole run.
