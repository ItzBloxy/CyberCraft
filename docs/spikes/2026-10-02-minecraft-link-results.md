# Minecraft link: results, steps 1 to 3

Step 1 ran BlockRender's link code against Minecraft without Cyberpunk; step 2, further down ("Step 2: in the game"),
ran it in the game; step 3, at the end ("Step 3: building from inside the game"), added keys and clicks.

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
V turned through north, because the yaw the link sent wrapped there. Task 28 (0.9.1) fixed it, checked in the
game the next day.
- **Loading:** BlockRender loaded hooked, and made the mapping at the game's first update, 15 s after load.
- **Minecraft linking:** Minecraft, started at the corner, linked at once and hid its window. It opened SkyCraft's
  world, and sent its atlas, which took 11.8 ms to build.
- **Linking:** Link pinned Minecraft's eye exactly at V's camera, switched drawing to the G-buffer, and sent the floor
  at V's feet. Step 1's gold block appeared on the street where step 1 left it. Walking, the eye followed V's camera
  within 0.07 m on average, at most 0.44 m at full speed, and the floor moved three times.
- **The HUD:** Minecraft's hotbar, crosshair and held item over Night City: sharp, the right way up, at the size
  planned. It was steady while turning, with frame generation on, except at north (the fix above).
- **Building by command:** `wall`, `tnt-ahead` and `field` built and blew up where they should, and the blocks were
  lit and shadowed as the game's own surfaces are. Minecraft's chat replies showed in the HUD. `wipe` missed, a block
  off: see "Surprises".
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
| `mc wipe` | Missed the field, a block below its layer, and the wall, its 32-block box centred where V stood by then: see "Surprises" | 8 sections; worst `Update` 8.5 ms, worst frame 21.4 ms |

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

### The yaw fix, checked

On 2026-10-03 at 00:11, BlockRender 0.9.1 (Task 28, `e1f1c06`) loaded with no error lines, and Minecraft linked
again. With the HUD always on, the user turned through heading 0 several times, both ways, slowly and quickly: the
held item now stays steady at north, with no jump.

### Surprises, and what they mean for Part 3

- **The held item jumped at north.** The yaw wrapped; Task 28 fixed it (see "The HUD" and "The yaw fix, checked").
- **V stood inside the field.** `field` fills the block that holds the feet, so its layer went in at y 6, from 6 to
  7, around V's shins: only 3 of the wall's 4 rows showed above it. Minecraft's player, held at V's feet, stood in it
  too:
  - **Minecraft's pose changes:** with its player inside blocks, Minecraft dropped to crawling for a second or so
    after V stopped walking. The crawling eye, 0.4 m up, was inside the grass, and the HUD showed Minecraft's in-block
    overlay, dirt for grass. As the user put it, that's Minecraft working as intended without collision, in a world
    where nothing is ever inside a block;
  - **a wrong guess, corrected:** at first this looked like Minecraft's player standing on our floor, above V's feet
    where the street dips. The experiment below ("Minecraft's eye among blocks") found commands work from the held
    feet, not from the floor.

  **For Part 3:** see the experiment below.
- **`field` and `wipe` pair up only at the same block height.** Each fills the block holding the feet, and the street
  crosses the boundary at y 6: the field went in with V's feet a little above 6.0, and `wipe` ran with them below it.
  The field went with two fills at its absolute coordinates.
- **Latency samples from sets of several commands are wrong.** Each Enter replaces the mark, so a section is timed
  from the last Enter before it, not from the command that made it: `tnt-ahead` gave 16 ms. Single commands measure
  it, as above.
- **Minecraft saves changed chunks at once,** so stopping it from the Modrinth App loses little. `/save-all` exists
  only on dedicated servers.
- **Explosions put up to 63 MB a second of particles through the render ring,** which the link steps over without a
  measurable cost.

## Minecraft's eye among blocks

Step 2 left one question that matters for Part 3's clicks: when Minecraft's eye leaves V's camera, and where a click
then lands. On 2026-10-03 at 00:21 the agent ran `mclink` with `tools/mclink/eye.txt` against Minecraft, without
Cyberpunk. For this run `mclink`'s status line also gives Minecraft's eye, and its height above the feet. The script
holds Minecraft's player at chosen feet, places blocks around it with commands, and reads the eye every 0.2 to 0.3 s:

| Phase | The eye above the feet | So |
|---|---|---|
| A: open space, feet at 6.14 | 1.62 throughout | standing: the eye at V's camera |
| B: a stone layer at y 6 around the feet, standing still | 1.62 for 3.5 s | being inside blocks alone changes nothing |
| B: then 6 teleports north through the layer, and still | 1.62, then 0.47 and 0.40 for about 0.75 s, then 1.17, 1.45 and 1.62 | step 2's drop: a short crawl after teleports inside blocks |
| C: on top of the layer, feet at 7.05 | 1.62 | standing |
| D: open air, one stone block above, at y 7 | 0.40 for as long as it's there | Minecraft's crawl pose: a standing or crouching player wouldn't fit |

- **Clicks follow the eye.** The same right click, 30° down, placed stone 0.7 m ahead while crawling under the block
  (at x −291, z 1999), and 2.8 m ahead standing in the open (x −285, z 1997). Both are exactly where a ray from that
  eye meets the floor.
- **Commands work from the held feet, not from our floor.** With a fresh floor at 6.14 and the feet held 0.34 below it,
  at 5.80, `/setblock ~ ~-1 ~` changed the block at y 4 (Minecraft: `Changed the block at -271, 4, 2000`), from the
  feet. From the floor it would have been y 5.
- **The in-block overlay** shows only while the eye is inside a block, so in a one-block layer at the feet, only
  during the short crawl.

**What it means for Part 3:** building on the open street is unaffected. Minecraft's eye stays at V's camera, and
clicks land where V aims. Clicks go wrong only where V is inside or under Minecraft's blocks: for about a second after
walking through blocks, and for as long as a block overhangs 1 to 2 m above the feet. Cyberpunk doesn't collide with
Minecraft's blocks, and that's the project's PhysX work, not this spike's. So Part 3 builds on the open street, and
its results say where clicks stray.

## Step 3: building from inside the game

Date: 2026-10-03, 16:33 to 16:59. The versions:
- **BlockRender 0.10.0:** the plan's Tasks 30 to 38 (`129087d`), with 232 C++ and 25 Python tests passing;
- **Cyberpunk 2077 2.31:** spike 3's settings, unchanged;
- **Minecraft and SkyCraft:** as in steps 1 and 2, in the same instance and world.

The run was at spike 3's corner, in free roam, by day. The user played, pressed each key and button asked for, built,
took screenshots, and started and stopped Minecraft. The agent sent requests, recorded the status twice a second, and
read BlockRender's log and Minecraft's. With the game closed, `mclink` then settled what the user reported along the
way (see "Clicks refused after a teleport"). Plan: Task 39.

### Verdict

**Step 3: pass.** In build mode the mouse buttons, the wheel and 1 to 9 went to Minecraft, and none of them reached
the game: V never reacted. Every check passed. The user reported one problem, and it isn't BlockRender's: now and then
Minecraft refused a block with "The height limit for building is 1023". Its server refuses clicks while it waits for a
teleport to be confirmed, and SkyCraft teleports Minecraft's player up to 10 times a second while V moves.
- **Loading:** the patch on the game's `GetRawInputData` went in first, and 4,870 keyboard and mouse events had passed
  through it by the first status. With build mode off, the game took input as always.
- **F8:** without Minecraft, it linked and left build mode off, saying why. With Minecraft in its world, it turned
  build mode on and off, and the HUD with it.
- **Each input:** Minecraft took every key, button and wheel notch, and V did nothing at any of them.
- **Building:** blocks landed where the crosshair pointed, at the corner and 26 m west.
- **Menus:** Esc, I, M, J, P, K, H, T and Shift+F6 each left build mode and opened as usual.
- **Click latency:** placing a block, Minecraft's half averaged 41 ms, from the click to the section's arrival, and
  ours 17 ms, within a frame. The latency check's breaks weren't timed: see "Surprises".
- **Alt-Tab:** Minecraft let go of a held button the moment the game lost the front, and build mode stayed on.
- **Unlinking, and Minecraft going:** each ended build mode, and the HUD went.
- **The counts:** 754 inputs went to Minecraft, and 802 were hidden from the game: those 754, and F8's own 24 presses
  and their releases. None were dropped.

### Loading, and F8 without Minecraft

- **BlockRender:** `GetRawInputData hooked; the focus watch runs`, then `BlockRender 0.10.0 loaded (game 2.3.1),
  hooked` at 16:33:16.4, and `the mapping: created` at 16:33:31.8, at the game's first update.
- **The status:** `build=off: F8 turns it on; slot unknown until a number key; 0 inputs forwarded to Minecraft and 0
  hidden from the game, the last: nothing yet; 0 dropped with the input ring full; GetRawInputData hooked, 4870
  keyboard and mouse events read`. The patch was on the game's input path from the start.
- **Build mode off:** the user drew a weapon, aimed with the right button and put it away, as always.
- **F8, with Minecraft not running,** at 16:35:38: `F8: linking first, then build mode`, `request: F8: link on:
  Minecraft's player follows V; drawing is gbuffer`, `request: build on: it stays off: Minecraft isn't there`, and
  `build mode stays off: Minecraft isn't there`.

### Minecraft, and build mode on and off

- **Minecraft,** started from the Modrinth App: `linked to Skyrim (pid 31924)`, `Skyrim link up`, `game window
  hidden` and `opening mirror world`, from 16:36:38 to 16:36:42. The link was on, so its player went straight to V.
  BlockRender logged `link: Minecraft is there (process 25044)` and a floor at V's feet, then Minecraft's atlas, built
  in 14.0 ms. Minecraft's eye was at V's camera to the hundredth.
- **The status** still said `build=off: Minecraft isn't there`, the reason F8 last gave, with 2 inputs hidden: F8's
  press and release.
- **The hotbar:** `mc creative` and three `/item replace` commands put stone, gold and diamond in its first three
  slots, and Minecraft's chat confirmed each.
- **F8, three times:** `build mode on` at 16:38:29.7, `build mode off: F8 turned it off` at 16:38:35.8, and `build mode
  on` at 16:38:37.6. Minecraft's HUD showed with the first and the third, and went with the second.

### Each input, Minecraft's and not the game's

With a weapon drawn and build mode on, the user went through every input, and more besides, with the status recorded
twice a second:

| Input | Minecraft | The `build=` line |
|---|---|---|
| 1 to 7 | selected each slot | the slot followed; the last `the key 5 up` and so on |
| 8 and 9 | nothing: this instance binds slots 8 and 9 to the side buttons | `slot 8`, `slot 9` |
| the side buttons | selected slots 8 and 9 | `slot unknown until a number key`; the last `mouse button 5 up` or `mouse button 4 up` |
| the wheel, a notch down and a notch up, from slot 2 | slot 3, then 2 | `slot 3`, then `slot 2`; the last `the wheel, -120`, then `the wheel, 120` |
| a right click on the street | stone, gold or diamond where the crosshair pointed | the last `the right button up` |
| a left click on a block | broke it | the last `the left button up` |
| a middle click on the diamond, from slot 1 | picked it: the selection jumped to the diamond's slot | still `slot 1`, as the spec says: a pick is Minecraft's own |

- **V did nothing** at any of them: no weapon switched, no aim, no swing, nothing thrown.
- **The counts:** 376 inputs forwarded and 392 hidden when the list was done. All through the 10-minute recording,
  hidden minus forwarded went up only with F8, by 2 a press, from 8 to 42 over 17 presses: every forwarded input was
  hidden too. None were dropped.

### Building at two spots

- **The corner, then 26 m west** (x −141.9): the user placed and broke blocks at both, and they landed where the
  crosshair pointed, on the street. Screenshots at each spot show them.
- **The positions,** recorded twice a second for 8 minutes (959 readings): Minecraft's eye was 0.06 m from V's camera
  on average, within the link's 0.1 m teleport step, and 0.39 m at most. The 17 readings over 0.25 m all came while V
  moved. The floor moved twice on the walk west, at 16 m (epochs 48 and 49).
- **The log:** no `render ring:` lines, and no errors.

### Leaving through each menu key

From 16:45:21 to 16:46:02, Esc, I, M, J, P, K, H and T, then Shift+F6, each logged `build mode off: <key> left it`.
The HUD went, the menu, the phone or CET's overlay opened and took clicks as usual, and F8 turned build mode on again
after each. T and H, pressed again, did the same.

### Click latency

Five right clicks placing stone and five left clicks breaking it, alternately, a second or two apart, with V standing
still:

| | Samples | Average |
|---|---|---|
| Minecraft's half: the click to the section's arrival | 30, 43, 74, 29, 31 ms | 41 ms |
| Ours: arrival to the first list that draws it | 9.6, 22.0, 22.5, 9.4, 23.0 ms; 0 or 1 frames | 17 ms |

- **Minecraft's half is shorter than for step 2's commands,** 41 ms against 68: a click needs no chat, and Minecraft
  acts on it at its next tick, at most 50 ms away. Ours is close to step 2's 20 ms: within a frame.
- **The breaks gave no samples:** see "Surprises".
- **Over the session,** the status's average of 192 samples was 49 ms and 21.7 ms (0.8 frames). It mixes in clicks in
  quick succession, where each click replaces the last one's mark, as step 2's sets of commands did.

### Alt-Tab with a button held

- **The first try** found no shield: step 1's starter kit gave one, but this player had none by now. `mc /item replace
  entity @s weapon.offhand with shield` gave it one.
- **Holding the right button,** looking at the sky, raised the shield in the HUD. The user Alt-Tabbed to Claude Code
  with the button still held, and the shield came down at once, before the button was let go in the other window. The
  log has `the game lost the front in build mode: Minecraft lets go of everything` for each Alt-Tab, four from
  16:50:18.9.
- **Back in the game:** still `build=on`, and a right click placed a block at about 16:50:55.
- **The focus watch** let go 14 times in the session, each time the game lost the front in build mode, and build mode
  stayed on each time.

### Unlinking, and Minecraft going

- **Clearing first:** instead of `mc wipe`, which clears one layer at V's feet, one `mc /fill -160 5 1985 -95 12 2025
  air` cleared the built area: 41 blocks.
- **`link off`** with build mode on: `build mode off: unlinked` at 16:58:10.9, `link=not linked`, and the HUD gone.
- **F8:** `F8: linking first, then build mode`, `request: F8: link on: …`, `request: build on: the mouse buttons, the
  wheel and 1 to 9 are Minecraft's` and `build mode on`, all at 16:58:28.0.
- **Minecraft stopped** from the Modrinth App: `link: Minecraft has gone` and `build mode off: Minecraft has gone` at
  16:58:41.4, and the HUD went.
- **F8 again:** `build mode stays off: Minecraft isn't there`.

### Clean up

`link off` and `mode off`: drawing off, unlinked, and no error lines in BlockRender's log for the session. By then 754
inputs had gone to Minecraft and 802 had been hidden from the game, none dropped, of 88,800 keyboard and mouse events
through the patch. BlockRender 0.10.0 stays installed: nothing changes until the user links, picks a drawing mode or
presses F8.

### Clicks refused after a teleport

The user reported that now and then a block wouldn't place, with Minecraft's red message "The height limit for
building is 1023", all through the testing. Nothing was near that height: fills from y 1,000 to 1,023 over the area
found no blocks, and Minecraft's player went above y 15 only while V flew up to about 1,000 m, late in the session.
- **The cause, in Minecraft 26.3's server:** `ServerGamePacketListenerImpl.handleUseItemOn` refuses a click on a block
  while `awaitingPositionFromClient` is set, which is while the server waits for the client to confirm a teleport. It
  then calls `sendBuildLimitMessage`, the height message with the world's top (1,023 in SkyCraft's world), and sends
  the block back as it was. A vanilla player is rarely teleported, so the misleading message rarely shows.
- **Why it happens here:** the link asks for a teleport each time V's camera moves 0.1 m, up to 10 times a second
  (`teleportstep` and `teleportrate`), and SkyCraft moves Minecraft's player on its client and on its server
  (`ServerPlayer.teleportTo`). Each teleport leaves the server waiting until Minecraft's client confirms it, a frame
  or so later. A click that reaches the server in that window is refused.
- **Settled with `mclink`,** without Cyberpunk: 24 right clicks along a row 2 m apart, with nothing in the way, each
  after a teleport to its spot. Of the 18 clicks sent in the same frame as their teleport, 3 were refused, each with
  the message in the HUD image 0.3 s later. Of the 6 sent a second after their teleport, none were. The row then held
  21 blocks, missing exactly the 3 refused ones. The row was removed afterwards.
- **For what comes next:** standing still, every click places; while V moves, some fail, at random. The project's own
  Minecraft side should hold a click until Minecraft's client has confirmed the last teleport, or not teleport
  Minecraft's player at all: with spike 2's puppeting, Minecraft's player moves and V follows it.

### Surprises, and what they mean for what comes next

- **Clicks refused after a teleport** (above): Minecraft's and SkyCraft's doing, not BlockRender's.
- **A break that empties a section isn't timed.** A section is timed from its arrival to the first list that draws it,
  and a section a break empties is never drawn again, so it gives no sample. In the latency check each block was alone
  in its section, so the five breaks gave none. The renderer drops such a section at its next update, on the same path as an
  upload, so the break should show as quickly as a placement. Timing it means timing a removal to the first list
  without it: a small follow-up.
- **The floor follows V up and down.** The link sends a fresh floor whenever V's feet are more than 1 m above or below
  the last one's centre. V's jumps, about 3 m high, sent it four to six times each, and V's flight to about 1,000 m
  sent it 1,668 times in a minute, epoch 1,743 by the end. A click in mid-jump lands on the raised floor: diamond
  blocks were left floating at y 9, beside where a jump had lifted the floor to 9.41 at 16:57:59, just before the
  last input, a right click with the diamond slot. Night City's own collision, the project's PhysX work, replaces the
  floor.
- **Two sections stayed unexplained.** At 16:43:55, as V walked west, two sections with 276 vertices arrived, and they
  stayed drawn to the end. None of the clearing fills touched them, Minecraft's world has nothing there now (a new
  `mclink` side at that spot got only the 4 known sections), and SkyCraft has no dig records saved. BlockRender's log
  doesn't list sections, so which they were is lost. A request that lists the renderer's sections, as `mclink` lists
  Minecraft's, would name them.
- **Blocks left in Minecraft's world:** the floating diamond blocks, stone from the `mclink` runs at x −120, and the
  eye experiment's blocks 150 to 190 m west. The first two will show at the corner in the next run unless cleared.
