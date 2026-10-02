# Minecraft link: results, steps 1 and 2

Step 1 ran BlockRender's link code against Minecraft without Cyberpunk; step 2, further down ("Step 2: in the game"),
ran it in the game.

Date: 2026-10-02. No Cyberpunk: `mclink`, BlockRender's link code in a console program, ran against the user's
Minecraft with SkyCraft's unchanged mod. The versions:
- **Minecraft 26.3**, Fabric Loader 0.19.5, Fabric API 0.161.0+26.3 and Java 25, in the Modrinth App's `CyberCraft`
  instance, with `--enable-native-access=ALL-UNNAMED -Dskycraft.quitWithSkyrim=false`;
- **SkyCraft 0.1.2**, the release jar (`skycraft-fabric-0.1.2.jar`, sha256 `ea429c73…`), protocol version 11;
- **BlockRender's link code and `mclink`** at `e5d0d66`: the plan's Tasks 1 to 10, built RelWithDebInfo, 175 unit
  tests passing.

The user set up the instance, and started and stopped Minecraft. The agent ran `mclink` with step 1's scripts, and read
its output, Minecraft's log and the saved images. Plan: [2026-10-02-minecraft-link.md](../superpowers/plans/2026-10-02-minecraft-link.md),
Task 11.

## Verdict

**Step 1: pass.** BlockRender's link code works against SkyCraft's real mod as written: nothing in it changed after
running it. Every check passed. Two of the plan's expectations were wrong, and both are explained below: the atlas is
larger than estimated, and one pixel of the HUD image isn't premultiplied, at an edge of Minecraft's first-person
shield.

- **The link:** Minecraft linked 0.5 s after `mclink` started, hid its window, and created SkyCraft's world in 2 s.
- **The pinned player:** after the teleport, Minecraft's feet strayed 0.0000 blocks from the target over 10 s (602
  frames in game).
- **The atlas:** 2048 × 2576, 20.1 MB, Minecraft's block textures with its item textures below them. The plan expected
  6 to 13 MB.
- **Typed commands:** `/gamemode creative` and a `/setblock` were typed through chat. The stone's section arrived
  0.03 s after the chat closed, its block exactly where expected, its texel stone's grey.
- **A click:** a right click looking 60° down placed stone in the expected cell, at x −120, z 2006. Minecraft's log
  gives the hit at z 2006.5646, the point the plan computed.
- **The HUD image:** 1280 × 720, rows bottom-up, 60 a second, the right way up, with the hotbar, the crosshair and
  chat's replies. All of it is premultiplied but one pixel: see "Surprises".
- **TNT:** it landed on the stone and exploded 4.00 s later, at −117.5 7.1 2007.5, radius 4. The section came as
  removed: the stone and the clicked block were gone.
- **Coming and going:** unlinking and linking again teleported Minecraft's player back. Stopping `mclink` dropped
  Minecraft's link 8 s later. A second `mclink` got everything again: the clear, the atlas, the section that still had
  a block, and HUD images.
- **Traffic:** Minecraft paced to `mclink` at 60 frames a second. The render ring carried about 8 MB a second, nearly
  all of it animated textures. Nothing was refused, rejected or dropped.

## Runs

| Run | When | Script | What happened |
|---|---|---|---|
| 1 | 14:17:45 | `step1.txt` | Task 11's Step 4: every check, in 29 s. `exit 0` |
| 2 | 14:20:53 | `again.txt` | Step 6: a new game side, 3 s after it linked. `exit 0` |
| 3 | 14:21:18 | a diagnostic script | The HUD image's one bad pixel: the same view with the shield, then without it |

Minecraft started at 14:16:52 and ran throughout. Its log has no errors, and two warnings (see "Surprises").

## The checks

### The link and the pinned player

- `mclink` created the mapping. Minecraft, polling once a second, opened it 0.5 s later. Its log:
  `linked to Skyrim (pid 8984)`, `Skyrim link up`, `game window hidden`, `sizing overlay to Skyrim viewport
  1280x720`, then `creating mirror world`. The integrated server started at 14:17:46, and the player joined at
  14:17:47.
- `link on` teleported the player to −119.5 6.14 2007.5 (Minecraft's log: `teleported to -119.5 6.14 2007.5`), and the
  link sent the floor: 49 regions of triangles at height 6.14, epoch 1.
- Over the next 10 s, `McState`'s feet stayed exactly at the target: SkyCraft held the player, since no voxel regions
  ever arrive.

### Typed commands, and a click

| Time (s) | `mclink` |
|---|---|
| 14.78 | `typed: /gamemode creative` |
| 14.90 | `typed: /setblock ~2 ~ ~ minecraft:stone` |
| 14.93 | `section -8 0 125`: 36 vertices, blocks x −118 to −117, y 6 to 7, z 2007 to 2008; its first triangle's texel (1701, 1333) is rgba(127, 127, 127, 255) |
| 16.05 | `typed: /item replace entity @s hotbar.0 with minecraft:stone`, then key 1, a look 60° down, and a right click |
| 16.67 | `section -8 0 125`: 72 vertices, blocks x −120 to −117, y 6 to 7, z 2006 to 2008 |

Typing one command takes about 0.1 s: T, a client tick for chat to open, the text and Enter, and a tick to close.
Latency from Enter to the frame that draws a block is step 2's measurement, in the game.

### The HUD image

`hud.dds`, saved at 17.69 s, 1280 × 720:
- the right way up once flipped (its rows come bottom-up, as SkyCraft's flag says);
- the hotbar, with stone selected in the first slot. SkyCraft's own starter and builder kits filled the other slots;
- the crosshair in white, and "Stone" above the hotbar;
- chat with the three replies: `Set own game mode to Creative Mode`, `Changed the block at -118, 6, 2007`,
  `Replaced 1 slot(s) on ItzBloxy with [Stone]`;
- Minecraft's first-person view as well: the held stone on the right, and the kit's shield on the left;
- a "New Recipe(s) Unlocked!" toast, from the kits;
- 262,656 pixels not transparent, and 1 with a colour above its alpha, where the spec expected none.

### TNT, and a block that stays

| Time (s) | `mclink` |
|---|---|
| 17.78 | `typed: /summon tnt ~2 ~1 ~` |
| 21.78 | `explosion at -117.5 7.1 2007.5, radius 4.0`: 80 ticks later, 0.06 above the stone it landed on |
| 21.84 | `section -8 0 125: removed` |
| 23.90 | `typed: /setblock ~-6 ~ ~ minecraft:gold_block` |
| 23.93 | `section -8 0 125`: 36 vertices, blocks x −126 to −125; its texel is rgba(255, 236, 79, 255), gold |

### Coming and going

- **Unlinking and linking again:** the second `link on` teleported the player back (a second
  `teleported to -119.5 6.14 2007.5` in Minecraft's log), and sent the floor again with epoch 2.
- **Stopping `mclink`:** it quit at about 14:18:14. Minecraft logged `Skyrim link down` at 14:18:22, kept running
  (`quitWithSkyrim=false`), and kept its world.
- **A new game side (run 2):** `mclink` reused the mapping Minecraft held. Minecraft logged
  `Skyrim instance changed (pid 32108)` and `Skyrim link up`, then sent `clear all` and the atlas within 0.31 s, and
  the gold block's section 0.01 s after. HUD images resumed at once: 166 in the 3 s run. SkyCraft resends only
  sections with blocks in them, so the gold block was what made this checkable.

### Traffic

Minecraft's frames, the render ring and HUD images, in `mclink`'s lines every 5 s:

| Window | Frames/s | Render ring | HUD images/s |
|---|---|---|---|
| 0 to 5 s | 196 | 37.5 MB/s, the 20 MB atlas included | 114 |
| 5 to 25 s | 60 | 7.7 to 8.6 MB/s | 60 |

Run 1's render ring, by message type, over its 29 s:

| Type | Messages | MB |
|---|---|---|
| atlas patch (animated textures) | 21,838 | 202.25 |
| atlas | 1 | 20.13 |
| scene (entities and particles) | 488 | 4.86 |
| ragdoll | 26 | 0.65 |
| texture (skins, armour, the particle atlas) | 6 | 0.55 |
| section, solids, clear-all | 4, 4, 1 | 0.00 |

No resyncs, rejected messages, sections before the atlas, dropped translucent triangles, dropped inputs or refused HUD
images.

## Surprises, and what they mean for Part 2

- **The atlas is 20.1 MB, not 6 to 13.** SkyCraft's `SkyAtlas` stacks Minecraft's block atlas over its item atlas,
  plus a 16-row strip of crack and arrow textures: 2048 × (2048 + 512 + 16). Minecraft 26.3's block atlas is a full
  2048 × 2048, though its sprites fill only about 1,600 rows. The plan's estimate assumed a smaller one.
  - **For Part 2:** the renderer takes a 20 MB atlas once per link. It fits SkyCraft's limit for one render message,
    half the 64 MB ring, so a resource pack that doubled it wouldn't.
- **One pixel of the HUD image isn't premultiplied.** It's at (561, 483), rgba(44, 45, 48, 0): colour with zero alpha,
  between two opaque faces of the first-person shield. Run 3 settled it: the same view gave the same pixel with the
  shield, and none without it, and nowhere else in either image does a colour exceed its alpha. Minecraft seems to
  draw the shield with a solid entity pipeline, which keeps the texture's alpha and has no alpha test. At a face edge,
  nearest-texel sampling then picks a transparent texel just outside the face's UVs, which keeps its colour. On
  Minecraft's opaque screen that can't be seen.
  - **For Part 2:** the HUD pass blends as premultiplied, as planned. Such a pixel adds its colour over Night City,
    close to what vanilla shows there. The spec's "no pixel has a colour above its alpha", and risk 13's fallback
    (below), get corrected while planning Part 2.
- **The HUD image includes Minecraft's first-person view:** its hands and held items (the stone, the shield), not just
  the GUI, since SkyCraft's `FrameExporter` reads back Minecraft's whole main target. The spec's "The HUD" expects
  that ("the hand and the held item"), though Task 11 didn't list it. Step 2 shows how they look over V's own view.
- **Risk 13's fallback doesn't apply.** The spec says that if step 1 finds any pixel with a colour above its alpha,
  the blend changes to straight alpha. That rule assumed an image that isn't premultiplied at all. This one is, but
  for one texel, and straight alpha would darken all of its translucent edges, so the HUD pass stays premultiplied.
- **Animated textures fill the render ring.** SkyCraft plays 56 animated sprites (water, lava, fire and others), and
  sends each one's frame as an atlas patch: about 8 MB a second, every second. The link steps over them, and
  draining costs nothing measurable. Applying them would animate water and lava in Night City: later work, not this
  spike's.
- **SkyCraft does more than the protocol:** on a new world it gives the player a starter kit and a builder kit (tools,
  food, torches, arrows, blocks and a shield), which bring recipe toasts into the HUD. It also connects to Discord
  for Rich Presence.
- **Minecraft ran unpaced for its first seconds:** 196 frames a second while it created its world, then 60, paced to
  `mclink`.
- **Minecraft's log has two warnings,** both harmless here: one failed acquire of its window's surface at 14:17:46,
  right after SkyCraft hid the window and resized it to 1280 × 720, and a request for a post effect that doesn't
  exist (`minecraft:end_of_frame`). HUD images were unaffected.
- **`view_dump.py`'s PNGs show colour without alpha.** 17% of the atlas's texels (919,778) are fully transparent but
  keep a colour, as Minecraft's straight-alpha textures do, so `atlas.png` looks as if sprites bleed into their
  surroundings. The PNGs are pixel-identical to the DDS files. `*.alpha.png`, or a viewer that reads the DDS's alpha,
  shows them as they are.

## Step 2: in the game

Date: 2026-10-02, 16:00 to 16:41. The versions:
- **BlockRender 0.9.0:** the plan's Tasks 13 to 26 (`a95b990`), with 203 C++ and 25 Python tests passing;
- **Cyberpunk 2077 2.31:** spike 3's settings (DLSS Quality at 2560 × 1440, frame generation, ray tracing, Reflex);
- **Minecraft and SkyCraft:** as in step 1, in the same instance and world.

The run was at spike 3's corner (world x −116.6, y −2005.2), in free roam, by day. The user played, clicked the
overlay's buttons, and started and stopped Minecraft. The agent sent requests, and read the status, BlockRender's log
and Minecraft's. Plan: Task 27, and Task 28 for the one fix.

### Verdict

**Step 2: pass.** Every check passed. One defect was found and fixed: the held item in Minecraft's HUD jumped whenever
V turned through north, because the yaw the link sent wrapped there. Task 28 (0.9.1) fixes it.
- **Loading:** BlockRender loaded hooked, and made the mapping at the game's first update, 15 s after load.
- **Minecraft linking:** Minecraft, started at the corner, linked at once and hid its window. It opened SkyCraft's
  world, and sent its atlas, which took 11.8 ms to build.
- **Linking:** Link pinned Minecraft's eye exactly at V's camera, switched drawing to the G-buffer, and sent the floor
  at V's feet. Step 1's gold block appeared on the street where step 1 left it. Walking, the eye followed V's camera
  within 0.07 m on average, at most 0.44 m at full speed, and the floor moved three times.
- **The HUD:** Minecraft's hotbar, crosshair and held item over Night City: sharp, the right way up, at the size
  planned. It was steady while turning, with frame generation on, except at north (the fix above).
- **Building by command:** `wall`, `tnt-ahead`, `field` and `wipe` built and blew up where they should, and the
  blocks were lit and shadowed as the game's own surfaces are. Minecraft's chat replies showed in the HUD.
- **Latency:** Minecraft's half averaged 68 ms, from a command's Enter to its section arriving. Ours averaged 20 ms,
  0.8 frames, from arrival to the first list that drew it.
- **Bursts:** neither the field (81 sections at once) nor 125 TNT hitched a frame.
- **Frame rates:** Minecraft running costs Cyberpunk about 2 ms a frame. Linking, drawing and the HUD add little on
  top: the HUD 0.25 ms at `hudscale 2`, and 1.6 ms at `hudscale 1`.
- **Focus:** with Claude Code in front, Cyberpunk kept running at full rate, and the link stayed up.
- **Minecraft closed and reopened:** the blocks stayed while it was gone. A new Minecraft process linked again, and
  sent its atlas and the saved wall. The HUD came back.

### Loading, and Minecraft

- **BlockRender:** `BlockRender 0.9.0 loaded (game 2.3.1), hooked` at 16:00:06.3, and `the mapping: created` at
  16:00:21.5, at the game's first update. The status, without Minecraft: drawing off, not linked, Minecraft absent,
  V's camera at Minecraft −116.60 7.74 2005.22.
- **Minecraft:** started from the Modrinth App, it logged `linked to Skyrim (pid 3248)` (Cyberpunk's), `Skyrim link
  up`, `game window hidden` and `opening mirror world` at 16:03:16 to 16:03:19. Then `sent 2048x2576 texture atlas
  to Skyrim (ok)` at 16:03:21, and step 1's gold block's section.
- **BlockRender:** `Minecraft is there (process 31420)`, `Minecraft cleared its sections` and `Minecraft's atlas: 2048
  x 2576, 5 levels, built in 11.8 ms`. Risk 5, a long frame for the atlas's levels, doesn't happen.
- **The status:** Minecraft in its world at 68 frames a second, Cyberpunk's 68, since it paces to `SkyState`. Atlas
  patches at 7.7 MB a second. The gold block waited: the renderer uploads nothing until drawing is first switched on.

### Linking, and walking

- **Link** (the overlay's button) at 16:05:10:
  - **the request:** `link on: Minecraft's player follows V; drawing is gbuffer`, and the floor at height 6.14;
  - **Minecraft's side:** `collision cleared (epoch 1)` and `teleported to -116.596 6.124 2005.221`;
  - **the status:** 66 frames drawn into the G-buffer a second, none skipped, none with a late camera. The gold
    block's section was uploaded (36 vertices, 0.009 ms of GPU time), and Minecraft's eye was at V's camera to the
    hundredth.
- **The gold block** stood on the street about 9 m from the corner, in Minecraft's texture, lit by the sun like the
  road. It cast no shadow, as spike 3 found for all blocks.
- **The walk:** 33 m along the street and back, recorded twice a second:
  - **the eye:** 0.07 m from V's camera on average. The most, 0.44 m, came at full speed with 10 teleports a second,
    the cap;
  - **the floor:** moved at 16 m (epochs 2, 3 and 4), following the street down to 5.94 m.

### The HUD

- **`HUD: always`** (the overlay's button) showed Minecraft's hotbar, its white crosshair and the held stone.
  - **The status:** 64 images a second, received and drawn, matching the game's 64 frames, 1280 × 720, rows
    bottom-up. Copying one took 0.17 ms on the main thread, and the pass 0.13 ms on the GPU.
  - **The size:** the hotbar spans about 1,090 screen pixels: Minecraft's 182 GUI pixels × 6, its GUI scale 3,
    doubled.
  - **No health or hunger bars:** the player is in creative.
- **Turning:** steady, with frame generation on, except at north. Turning through heading 0 (350 to 10), the held
  item jumped, and only the held item: the hotbar and crosshair stayed put, and heading 180 was fine all round.
  - **The cause:** the link's yaw was in (−180°, 180°], so it wrapped from 180 to −180 facing north. SkyCraft sets
    the player's yaw to it every frame, where Minecraft's own yaw never wraps. Minecraft sways the first-person hand
    by the yaw's change, so the hand swung a whole turn's worth and settled.
  - **The fix:** Task 28 keeps the yaw within 180° of the last, as Minecraft's own is. SkyCraft's own Skyrim side
    wraps the same way, at Skyrim's north.
- **Dropped images:** 9 of 62 HUD images were dropped in the second the user took a screenshot, and none in any
  other reading.

### Building by command

| Command | What happened | Burst |
|---|---|---|
| `wall` (the button) | Chat: `Successfully filled 20 block(s)`. 5 × 4 stone bricks, 4 m ahead, sharp, sunlit and shaded like the street | 1 section, worst `Update` 0.74 ms, worst frame 14.3 ms |
| `mc tnt-ahead` | 3 commands typed in 0.1 s; the TNT went off 4.0 s after the summon, at −118.4 6.1 2006.4, and chipped the wall's near end (stone bricks resist TNT) | 1 section each; worst frames 16.2 and 17.3 ms |
| `mc field` | A grass layer 128 blocks square, the game's palm shadows falling on it. 81 sections, 202,968 vertices, 0.065 ms of GPU time | 81 sections, 13.6 MB; worst drain 0.12 ms, worst `Update` 9.6 ms, worst frame 19.9 ms |
| `mc wipe` | The wall went, but not the field: see "Surprises" | 8 sections; worst `Update` 8.5 ms, worst frame 21.4 ms |

`wall` always runs east to west, so facing about west it stood edge-on beside V at first.

### Latency

Ten single commands with V standing still, putting a gold or a diamond block 3 m ahead in turn, each sent once the
last one's sample was in the log:

| | Samples | Average |
|---|---|---|
| Minecraft's half: Enter to the section's arrival | 60, 75, 61, 75, 75, 60, 75, 75, 61, 61 ms | 68 ms |
| Ours: arrival to the first list that draws it | 2, 24, 23, 23, 23, 23, 23, 24, 8, 23 ms; 1 frame, or 0 twice | 20 ms, 0.8 frames |

Minecraft's half falls in two clusters, about 60 and 75 ms, a Minecraft frame apart. Ours is within a frame, inside
the spike question's "a frame or two". Enter to the drawn frame is about 90 ms; the game shows a frame a frame or two
after drawing it, as always. The status's own average mixes in the sets' samples, which are wrong (see "Surprises").

### Bursts

- **`field`:** 81 sections and 13.6 MB within a second. Its worst frame, 19.9 ms, is within the frame-to-frame
  variation at rest (worst frames up to 20 ms), and the worst `Update` was 9.6 ms on the main thread.
- **`tnt-cube`:** 125 TNT went off over 2 s, flung up to 70 blocks in the empty world, so only 4 sections changed.
  The explosions' particles filled the per-frame scene messages: 34 MB in one second and 63 MB in the next. The link
  stepped over them, the worst drain 0.007 ms, and the worst frame was 16.1 ms in both seconds.

Neither hitches a frame, so the spec's fix for bursts (sections suballocated from a few large buffers, and an upload
budget a frame) isn't needed.

### Frame rates, CPU and memory

Five readings a second apart for each, at different spots, so the comparisons are approximate:

| Condition | Frame time | CET's fps | Cyberpunk's CPU | Minecraft's CPU, memory |
|---|---|---|---|---|
| Minecraft closed | 12.7 to 13.3 ms | 76 to 79 | 3.4 to 4.1 cores | |
| Minecraft running, not linked | 14.7 to 15.5 ms | 64 to 70 | 2.3 to 4.6 cores | 0.3 to 0.9 cores, 2.1 GB |
| Linked and idle (one block drawn) | 14.6 to 15.0 ms | 67 to 68 | 2.3 to 4.1 cores | 0.4 to 0.6 cores, 2.3 GB |
| The HUD off | 14.3 to 14.5 ms | 69 to 70 | 2.7 to 3.8 cores | 0.6 cores, 2.1 GB |
| The HUD at `hudscale 2` | 14.5 to 14.7 ms | 68 to 69 | 3.3 to 4.0 cores | 0.6 to 0.7 cores, 2.1 GB |
| The HUD at `hudscale 1` | 15.6 to 16.1 ms | 62 to 64 | 1.7 to 4.3 cores | 0.6 to 0.9 cores, 2.4 GB |

- **Minecraft running costs Cyberpunk about 2 ms a frame,** linked or not. Linking and drawing in the G-buffer add
  nothing measurable on top.
- **The HUD costs about 0.25 ms a frame at `hudscale 2`:** a 0.17 ms copy and a 0.12 ms pass. At `hudscale 1` it's
  about 1.6 ms: 14 MB images, a 0.72 ms copy and a 0.61 ms pass, nearly 900 MB a second through the triple buffer.
  Minecraft reads its frame back whether the HUD is taken or not, so its side doesn't change.
- **`hudscale 1` halves the HUD on screen:** the instance's GUI scale is fixed at 3, not automatic. The held item gets
  sharper.
- **Cyberpunk's working set** was 7.5 to 7.8 GB, then fell to 4.1 GB while Minecraft grew to 2.4 GB at `hudscale 1`.
  That's Windows trimming it, so the working set isn't a steady measure of memory.

### Focus

Recorded twice a second for a minute while the user spent 20 s in Claude Code on the second monitor: the status stayed
under 1.0 s old, frames held at 14 to 15.5 ms, the link stayed in game with Minecraft's heartbeat fresh, and
Minecraft's log has no `link down`. Cyberpunk keeps running at full rate behind another window, so risk 2 doesn't
happen.

### Minecraft closed and reopened

- **Saving:** `mc wall` put up a wall to come back. `/save-all` isn't a singleplayer command (`Unknown or incomplete
  command`), but Minecraft wrote the wall's region file 0.08 s after the fill. The 5-minute autosave at 16:38:21 found
  nothing left to write.
- **Stopped** from the Modrinth App: `link: Minecraft has gone` at 16:38:43, 3 s after its last heartbeat. The wall
  stayed, the HUD went, and the link stayed in game.
- **Started again:**
  - **BlockRender:** `Minecraft is there (process 15660)` and a fresh floor at V's feet at 16:39:18. Then
    `Minecraft cleared its sections`, and its atlas again, built in 18.9 ms;
  - **Minecraft's side:** it linked to the same Cyberpunk, teleported its player to V, opened its world, and sent the
    wall's section: the same 348 vertices, from disk;
  - **the HUD:** back at 75 images a second, none dropped, so the triple buffer started over cleanly for the new
    process.

### Clean up

`mc wipe`, `set hud 1`, `link off` and `mode off`: drawing off, unlinked, and no error lines in BlockRender's log for
the whole session.

### Surprises, and what they mean for Part 3

- **The held item jumped at north.** The yaw wrapped; Task 28 fixes it (see "The HUD").
- **Minecraft's player stands on our floor, not at V's feet.** The floor is flat at the height it was sent with, and
  moves only at 16 m or a metre's change. Where the street dips below it, Minecraft's player stands on the floor:
  - **commands work from there:** `field` filled at y 6, a block higher than V's feet (5.92). So V stood in the
    grass, and only 3 of the wall's 4 rows showed above it;
  - **Minecraft's pose changes:** with its player inside a block, Minecraft drops to crouching or crawling for a
    second or two after V stops, and the crawling eye, 0.4 m up, is inside the block. The HUD then shows Minecraft's
    in-block overlay, dirt for grass. As the user put it, that's Minecraft working as intended without collision, in
    a world where nothing is ever inside a block.

  **For Part 3:** clicks cast from Minecraft's eye, which is at V's camera only while Minecraft's player stands. And
  V can walk into any block it places, since Cyberpunk doesn't collide with Minecraft's blocks, which will bring the
  overlay back. Part 3 should measure how often, and how far off, a click is.
- **`field` and `wipe` pair up only at the same block height.** `wipe` clears the layer at Minecraft's feet, and V had
  moved to ground a block lower. The field went with two fills at its absolute coordinates.
- **Latency samples from sets of several commands are wrong.** Each Enter replaces the mark, so a section is timed
  from the last Enter before it, not from the command that made it: `tnt-ahead` gave 16 ms. Single commands measure
  it, as above.
- **Minecraft saves changed chunks at once,** so stopping it from the Modrinth App loses little. `/save-all` exists
  only on dedicated servers.
- **Explosions put up to 63 MB a second of particles through the render ring,** which the link steps over without a
  measurable cost.
