# Handoff: spike 7, Minecraft's entities in Cyberpunk

Written 2026-10-06, at the end of the session that executed spike 6's Part 3, closed spike 6 and merged it. The user's
next step: **spike 7, Minecraft's entities in Cyberpunk.** Night City shows Minecraft's blocks and its HUD, but nothing
else Minecraft draws: lit TNT, mobs, chests and other block entities, dropped items, arrows, particles. Nothing about
the spike is decided beyond that topic. Read this first.

(The user called it "spike 6"; spike 6 was NPCs and combat, so this is spike 7.)

## Start here

- **Repo and branch:** `D:\Cyberpunk-Minecraft`, on `main` at `a1db95d`: spike 6 merged by fast-forward, its branch
  deleted. No spike branch is open: the brainstorm's spec starts one, as each spike has. The repo has no git remote.
- **What's next:**
  1. **`superpowers:brainstorming`** with the user, on "Minecraft entities in Cyberpunk". The brainstorm decides the
     scope. Start from "Questions to start with" below.
  2. **Then as before:** the spec, agreed with the user section by section, then a deep self-review; the plan
     (`superpowers:writing-plans`), every code step compiled and tested in a scratch clone, each new test broken once,
     the tasks replayed in a fresh clone exactly as written; then execution inline, a task at a time, with
     `tools/plans/run_plan.py execute`.

  **Expect the user to ask for a deep review** of the spec and the plan: trace every claim to its source, simulate the
  in-game steps, and make each new test fail once.
- **No sub-agents:** the user wants everything done inline.
- **Read for background, in this order:**
  - this handoff;
  - **the minecraft-link spec**, `docs/superpowers/specs/2026-10-02-minecraft-link-design.md`: "Non-goals" (no
    entities, items, particles, the player's avatar or the block outline; no animated textures, translucent blocks or
    Minecraft's lights), "SkyCraft's mod, once linked" (its exporting, step 3), and "The protocol, as this spike uses
    it";
  - **spike 3's results**, `docs/spikes/2026-10-01-block-render-results.md`, and the block-render plan's "How Part 3
    went": the two places BlockRender draws, the G-buffer and before DLSS, and what they cost;
  - **spike 6's results**, `docs/spikes/2026-10-05-npcs-and-combat-results.md`: the answer and "What is left" at the
    top;
  - **SkyCraft's source** (below): `fabric/src/client/java/dev/skycraft/client/render/AvatarExporter.java` (the scene
    and avatar captures), `render/WorldExporter.java` (the world entities), `skse/src/WorldRender.cpp` (how SkyCraft's
    Skyrim side draws them), and the protocol header, `extern/skycraft/skycraft_protocol.h`;
  - **BlockRender:** `spikes/block-render/src/link/Link.cpp` (`Link::Take`, what the render ring's drain keeps),
    `src/render/` (`Renderer`, `Gbuffer`, `Predlss`, `Pipeline`, `Atlas`) and `shaders/blocks.hlsl`.
- **A first message to start the next session with:**
  > Read docs/superpowers/handoffs/2026-10-06-spike-7-minecraft-entities.md and let's brainstorm Minecraft's entities in
  > Cyberpunk.

## What Night City doesn't show yet

BlockRender draws Minecraft's block sections (`kRenSection`, with its atlas) and the HUD image (the hand, the HUD and
screens). Everything else Minecraft draws is missing:
- **entities:** lit TNT, falling blocks, minecarts, boats, mobs. In spike 6's step 5b a TNT block was drawn until lit,
  then vanished, and went off 4 s later where it was;
- **block entities,** which Minecraft draws apart from its section meshes: chests, beds, signs, banners, and pistons
  while they move;
- **particles:** explosions, smoke, block debris, crits;
- **dropped and thrown items, arrows and tridents;**
- **the block-breaking cracks, the targeted block's outline, and contact shadows;**
- **the player's own body.** Cyberpunk's camera is V's eye, so this matters little, but V is hidden in build mode.

Also left out so far, and close to this spike: animated textures (blocks show their first frame), translucent blocks
(dropped, or cut-outs with `set translucent 1`), and Minecraft's lights.

## SkyCraft's entity channels (protocol version 11)

Checked against SkyCraft's source on 2026-10-06. All are in SkyCraft's unchanged mod, sent whenever linked.
- **`kRenScene`** (render ring, every Minecraft frame): every other entity and all particles.
  - **Its layout:** `RenScene` (an origin, a Minecraft block near the camera, as doubles; `batchCount`; `vertexCount`),
    then `RenBatch[batchCount]`, then `RenVertex[vertexCount]`, triangle lists. A batch has a `texture` (0: the
    combined block and item atlas, else a `kRenTexture` id), its `first` vertex and `count`, and `flags` (bit 0:
    translucent, drawn blended after the solid pass and casting no shadow).
  - **What's in it** (`AvatarExporter.exportScene`): the entities within 64 blocks of Minecraft's camera, at most 48,
    leaving out the player, dropped items, arrows, thrown items and our NPCs' stand-ins (`SkyrimActorEntity`); the
    block entities within 48 blocks, at most 256; and all particles.
  - **How it's caught:** Minecraft's own renderers submit into SkyCraft's collector, posed and animated, instead of
    a GPU buffer. Models, items, block models, moving blocks and particles are kept. Shadows, name tags, text (signs'
    words), flames, leashes, outlines and custom geometry are skipped.
- **`kRenTexture`:** a texture besides the atlas (`RenTexture`: an id from 1, the size, then RGBA8 pixels): skins,
  armour and other entities' textures. Each goes once, the first time a batch uses it, and all of them again after a
  new link, world or atlas (`AvatarExporter.reset`). If the ring is full, it's tried again later.
- **`kRenAvatar`:** the player's body in third person (F5), relative to its feet; no batches in first person, which is
  how build mode runs.
- **`kRenRagdoll`:** the player's standing body split into its parts (batch flags bits 8 to 11), about once a second
  while alive, for Skyrim's ragdoll when the player dies.
- **`kRenAtlasRegion`:** an animated sprite's current frame, a patch into the combined atlas. In spike 6's last session
  the status counted 825 patches a second, 7.62 MB.
- **The world entities** (Minecraft → game, a seqlocked table at `0x1C000`, every frame; `WorldExporter`): up to 160
  `WorldEntity` records of 96 bytes within 96 blocks, the light path for small things. Each has a kind, Minecraft's
  entity id, a position interpolated to Minecraft's render time, yaw, pitch, scale, a box size, three atlas rectangles
  and a tint. The kinds: arrows, dropped items (flat sprites turning about the vertical), tridents, dropped blocks
  (spinning cubes, with side, top and bottom), block-breaking cracks over a box, and contact shadows under players and
  mobs. The table also has the targeted block's outline.
- **Vertex flags beyond the header's comment.** The header documents bit 0 (cut-out), bit 1 (translucent) and bits 4 to
  6 (a face's direction + 1). SkyCraft's entity and particle vertices also use bit 3 (no mipmaps: "entity textures:
  always full detail, like Minecraft") and the direction 7 ("lit by the triangle's own normal"); particles have no
  direction. Its Skyrim side adds bit 2 (untextured) for geometry it builds itself. See `AvatarExporter`'s `SOLID` and
  `PARTICLE` flags, and `WorldRender.cpp`'s `kFlag…` constants and pixel shader.

### How SkyCraft's Skyrim side draws them

`skse/src/WorldRender.cpp`, as the reference:
- `OnMesh` (about line 880) puts the avatar's and the scene's batches and vertices into dynamic vertex buffers.
  `kRenTexture` makes a texture by id.
- Each frame it reads the world entities (`ReadWorldEntities`, about line 2907) and builds their geometry (from about
  line 1280): shadows, block cubes, cracks, arrows and tridents (`BuildArrow`), and item sprites.
- `DrawMesh` (about line 1480) draws the avatar at Skyrim's interpolated feet and the scene at its origin, the
  translucent batches in a blended pass after the rest.
- It draws everything forward, right after Skyrim's 3D scene and before its post-processing, into the HDR scene and
  Skyrim's depth, lit by its own shader: the sun, shadowed by Minecraft's blocks and by Skyrim's shadow maps. Skyrim's
  fog, tone mapping and bloom then apply.

## What BlockRender does with them now, and what exists to build on

- **The drain:** `Link::Take` keeps `kRenAtlas`, `kRenSection`, `kRenClearAll` and `kRenSolids`. Everything else is
  counted in the status's ring line and stepped over: atlas patches, lights, digging, textures, avatar, scene and
  ragdoll. The world entities are zeroed when the mapping is made (`Mapping.cpp`), and never read.
- **The vertices:** `RenVertex` is byte for byte BlockRender's `Vertex`, so the scene's vertices could go to the GPU as
  they come.
- **Drawing:** sections only, in committed buffers (`Renderer::UploadSection`), in one of two places, set by `mode` or
  the overlay's Draw buttons:
  - **`gbuffer`,** the default since spike 3's step 4: into the game's G-buffer, just before the list that ends it. The
    game lights, hazes and shades the blocks as its own; they cast no shadows (they aren't in its shadow maps), and
    nothing can blend there;
  - **`predlss`:** forward, into the tagged colour before DLSS (`R16G16B16A16_FLOAT`), with the motion vectors and
    depth, under the HUD.
- **The shader** (`shaders/blocks.hlsl`) reads the atlas as a buffer, texel by texel (`SampleAtlas`), always discards
  texels with alpha under 0.5, and reads flag bits 4 to 7 as a direction into seven-entry tables. Entity textures,
  blending, and SkyCraft's direction 7 and bits 2 and 3 are all new to it.
- **The bodies** (`physx/Bodies`, spike 5): PhysX actors of ours, made with audited calls. They're the template if
  something of Minecraft's should be solid for V.
- **`mclink`** (`tools/mclink`): the game's side as a console program, linked to the real Minecraft with Cyberpunk
  closed. It already drains the render ring, so it could show what scenes, textures and world entities carry before
  anything is drawn.
- **The overlay's Fun controls** (spike 6's Task 53) switch Minecraft's game mode and fill the hotbar, without the
  agent. Spawn eggs, `/summon` or a placed chest go through `mc` requests, and the overlay now queues its requests.

## Questions to start with

A starting list, not a decision; agree the scope with the user.
1. **Which entities, in what order:** lit TNT and falling blocks; block entities (chests, signs, beds); mobs; particles
   (explosions, smoke, debris); dropped items and arrows (the world entities); cracks, the outline and contact shadows;
   the player's body where V is hidden. Mobs don't spawn in SkyCraft's world (`configureServer` stops it), so they come
   from spawn eggs or `/summon`.
2. **Where to draw them:** opaque models into the G-buffer, as the blocks are, so the game lights them; and particles
   and translucent batches in a forward pass, blended. Before DLSS, as `predlss` draws, depth-tested against the game's
   depth? What do DLSS's motion vectors need for moving entities and particles, so they don't smear?
3. **Textures:** the atlas, and `kRenTexture`'s, a batch at a time. How the shader samples them (the atlas is a buffer
   now), and what happens at a new link, when they all come again.
4. **Per-frame geometry:** the scene comes every Minecraft frame. How many vertices does an explosion's particles make,
   what does uploading them cost, and how does a scene line up with Cyberpunk's frame (the HUD image arrives about a
   frame late)?
5. **Animated textures** (`kRenAtlasRegion`, stepped over now): entities and particles use the atlas too. Fold them in?
6. **Solid or not:** should V collide with boats, minecarts or mobs, through bodies like spike 5's? Or is this spike
   only drawing?
7. **What to test without the game first:** `mclink` against Minecraft, logging what a lit TNT, a summoned mob, a placed
   chest and an explosion send, sizes and textures included, as earlier spikes' first steps did.

## SkyCraft's source

SkyCraft 0.1.2 at `bfcaf17`, cloned in an earlier session to:

`C:\Users\BLOXY\AppData\Local\Temp\claude\D--Cyberpunk-Minecraft\818fd1fd-489f-482d-9873-88e6752bad04\scratchpad\skycraft`

It was still there on 2026-10-06. It's a temporary folder; if it's gone, cloning again is a download, so ask the user
first. The repo keeps only the protocol header (`extern/skycraft/`). The mod's jar is in the instance's `mods` folder,
and `javap` reads it without unpacking. Read the whole source before concluding SkyCraft lacks something.

## The game and Minecraft, right now

- **BlockRender 0.19.1** (`bd7acca`, module version 7) is in `E:\Cyberpunk2077\red4ext\plugins\BlockRender\`, with the
  overlay of `30bd3e2` in CET's `blockrender_hud`.
  - **Defaults at each launch:** drawing off until Link, which draws in the G-buffer; bodies on, NPCs on, combat off
    (`set combat 1` turns it on), the forced knockdowns on.
  - **The overlay:** one row of buttons (Link, Unlink, Blocks, Box, Turn, NPCs, Combat), a line each for the status,
    and three folded sections: More controls, Fun, and Full status.
  - **DRED is on:** `settings.txt` there has `dred = 1`, since spike 5's GPU crash.
- **Also installed:** spike 1's probe (`PhysXProbe`) and its overlay; FreeFly (F7); Immersive First Person, Appearance
  Menu Mod and Smoother Dodge and Dash, among others; Codeware, ArchiveXL, TweakXL, Input Loader and Mod Settings.
- **Minecraft:** the Modrinth App's `CyberCraft` instance: Minecraft 26.3, Fabric Loader 0.19.5, Fabric API 0.161.0,
  SkyCraft 0.1.2, CyberCraft Tweaks 1.3.0 (1.2.0 kept as `cybercraft-tweaks-1.2.0.jar.disabled`). The player is in
  survival, with golden apples, golden carrots, TNT and a flint and steel in hotbar slots 6 to 9; the Fun buttons
  restock them. The world has `keep_inventory` and `immediate_respawn` on.
- **The repo's CMake cache** has `CP2077_DIR` empty, so builds don't install into the game.
- **Also asked for, not this spike:** Minecraft's inventory and its game mode switcher in build mode. Spike 6's results
  keep it under "Asked for, after Part 3", and its spec's "Asked for during step 5b's fix" has what was verified.
  Unless the user folds it in, it waits for a spike of its own.

## Rules to keep

- **No sub-agents:** work inline.
- **The user does every in-game and Minecraft action:** launching and quitting both games, loading, moving, keys,
  clicks, CET's console and screenshots. Never drive their mouse or keyboard; ask, then wait for them to say they're
  done.
  - **On-screen aids:** give them when a test needs positions, headings or counts; the overlays show V's position and
    heading, and the Minecraft block at V's feet.
  - **Name what's invisible:** anything BlockRender doesn't draw. Say so before the user looks.
- **Deep reviews:** trace every claim to its source, simulate the in-game steps, and break each new test once.
- **Settle important unknowns** before moving on, rather than leaving them as next steps. Offer that.
- **Testable steps first:** sequence the work so each step can be checked against something real right then.
- **Read values from the game:** the user wants the game's and Minecraft's real numbers, fed into the game's own
  systems, not tables tuned by hand.
- **Builds and the game:**
  - never build with `CP2077_DIR` set, or deploy, while Cyberpunk runs: `tasklist | grep -i -c "cyberpunk2077.exe"`
    must print `0`. The one exception so far was spike 6's Task 53, at the user's choice: the overlay's Lua file alone,
    which nothing holds;
  - set `CP2077_DIR` only to deploy, and clear it straight after;
  - `mclink` needs Cyberpunk closed;
  - CyberCraft Tweaks' jar can't be replaced while Minecraft runs: ask the user to stop it first.
- **No faults:** the game's crash handler sees a fault before any `__try`. Check every pointer, size and vtable; the
  module checks every object with `IsDefined`. If the game freezes, read the logs and the newest folder in
  `%LOCALAPPDATA%\REDEngine\ReportQueue`, and force-close it only when the user asks.
- **Downloads** need the user's explicit permission first.
- **Changes to the user's Minecraft world or instance** need their leave; clear what tests place or break.
- **Commits** end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`, and go on the spike's branch.
- **After the spike:** `superpowers:finishing-a-development-branch`. The user has chosen a local fast-forward merge to
  `main` every time so far; ask again.

## Gotchas

- **Bash's working directory:** a `cd` in a Bash call moves it for later calls. Wrap it in a subshell, `(cd … && …)`,
  or use absolute paths.
- **Backslashes:** the Bash tool turns `\\` into `\`. Write files that hold backslashes with the Write or Edit tools.
- **Tool paths:** `cmake` and `dumpbin` aren't on `PATH`; the plans' build steps have their full paths. `lua` and `luac`
  (5.4), `javac`, `javap` and `jar` are.
- **Python:** `uv run --no-project`.
- **Tests:** run `render_tests.exe` from `spikes/block-render`, or its fixture files aren't found. 450 pass at
  `a1db95d`.
- **Waiting on the game:** a short `sleep` inside a command works; a `timeout … bash -c` loop was refused once. Send a
  request, then read the status's `request=` line.
- **Requests:** write `blockrender.request` atomically (a temporary name, then renamed), and send the next only once the
  status's `request=` line shows the last. The overlay's buttons queue their own.
- **The plan tools** (`tools/plans/`, see its README): a step titled "Write", "Edit", "Delete" or "Copy" is a file
  step to them, so a step done by hand is titled "Record …" or "In the game: …". Their paths point at spike 6's
  scratchpad: adapt them. `fun_check.lua` runs the overlay with CET stubbed and clicks its buttons.
- **Line endings:** the repo's working files are LF and the scratch clones' CRLF; compare trees by blob (`git
  rev-parse <commit>:<path>`), not by `cmp`.
- **Minecraft:**
  - the user starts and stops it from the Modrinth App; its window hides while linked;
  - its log is `%APPDATA%\ModrinthApp\profiles\CyberCraft\logs\latest.log`, with chat replies as `[CHAT]` lines;
  - its world is saved: whatever a test places or breaks stays. Clear it afterwards, by block coordinates;
  - in survival, starting build mode high up means Minecraft's player falls, and its death kills V.
- **Logs:** BlockRender writes one per launch in `E:\Cyberpunk2077\red4ext\logs\`; redscript's is
  `E:\Cyberpunk2077\r6\logs\redscript_rCURRENT.log`.
- **The scratchpad is per session:** spike 6's scratch clone (`verify/`, tags `p3-t38` to `p3-t53`), its notes and the
  Fun controls' checks are in session `a0bb7ab3-58f9-4d19-a7f8-25f4fd8a9dde`'s scratchpad. Start a fresh clone from
  the new branch.
