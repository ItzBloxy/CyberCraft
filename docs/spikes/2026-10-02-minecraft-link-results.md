# Minecraft link: step 1's results

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
    close to what vanilla shows there. The spec's "no pixel has a colour above its alpha" gets corrected while
    planning Part 2.
- **The HUD image includes Minecraft's first-person view:** its hands and held items (the stone, the shield), not just
  the GUI. SkyCraft's `FrameExporter` reads back Minecraft's whole main target. Part 2 has to decide whether those
  are drawn over V's own first-person view.
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
