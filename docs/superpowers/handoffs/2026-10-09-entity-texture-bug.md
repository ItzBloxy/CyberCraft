# Handoff: Minecraft's mobs drawn with their textures wrong

Written 2026-10-09, at the end of spike 9 (Minecraft's mobs finding their way), which found it during run F. The user
asked for this handoff and calls it **critical: it affects all mobs.** Read this first.

## Start here

- **Repo and branch:** `D:\Cyberpunk-Minecraft`, `main` at `a84f683` (spike 9 merged). Make a branch for the fix,
  such as `fix/entity-textures`; nothing goes on `main` directly. The repo has no git remote.
- **What the user saw** (run F, 2026-10-09, in the game): "At times, the wolf looks very cursed. Texture appears to be
  wrapped/applied wrongly at certain angles/distances from the wolf", and then that it "affects all mobs". The
  screenshots: `docs/spikes/2026-10-08-minecraft-paths-results/run-f-spot-b-wolf-street.jpg` (the wolf on the street,
  5.8 m off) and `run-f-spot-b-wolf-near.jpg` (3.1 m off): a white blocky body with red and dark patches in places a
  wolf has none; `run-f-spot-b-sidewalk.jpg` has the overlay. Pathfinding and teleporting were fine; this is drawing.
- **Nothing is settled about the cause.** Spike 9's results record it under the wolf alone as "its collar's layer over
  its body fights at the same depth (spike 7's risk 16)": a guess from one mob, which "all mobs" contradicts. Start
  from the evidence below, not from that.
- **Process:** superpowers:systematic-debugging, root cause before any fix. If the fix turns out to be a design change
  (new shader paths, motion vectors per entity), brainstorm it first (superpowers:brainstorming), then a spec and a plan
  as the spikes have them. Inline, no sub-agents.
- **A first message to start the next session with:**
  > Read docs/superpowers/handoffs/2026-10-09-entity-texture-bug.md and let's debug the entity texture bug.

## Ask the user first

What only they saw, before any test:
- **Which mobs, and when:** every mob, or the layered ones (a wolf's collar, a villager's clothes)? Standing still too,
  or only walking? V still and the mob still: does it change frame to frame (flicker, crawl), or stay put?
- **With distance and angle:** does it grow with distance, or come and go at certain angles? Near the screen's edge?
- **Compared with Minecraft:** Minecraft's own window is hidden while linked (SkyCraft hides it; starting Minecraft
  with `-Dskycraft.showWindow=true` keeps it); does the same mob look right there?
- **Before spike 9:** did mobs look right in spikes 7 and 8's runs, as their results say, or was it there already?

## What changed since spike 7 passed its checks

Spike 7 (Minecraft's entities in the game, runs A to D) passed its checks, layers and smearing included
(`docs/spikes/2026-10-06-minecraft-entities-results.md`). Since then:
- **Entity Model Features and Entity Texture Features are enabled.** Spike 7 ran with them **disabled** (its results:
  "the user's Entity Model Features and Entity Texture Features disabled"). The mods folder now has
  `entity_model_features-3.3.11-26.3-fabric.jar` and `entity_texture_features-7.2.5-26.3-fabric.jar` (both dated
  2026-10-07), enabled by the user's choice, and since 2026-10-09 `entity_sound_features-0.8.2-26.1-fabric.jar` (built
  for 26.1). EMF replaces vanilla entity models with its own; ETF adds texture variants and emissive layers. SkyCraft
  captures what Minecraft draws; what it makes of EMF's models and ETF's textures is untested.
- **Mobs walk now.** Before spike 9 a mob on the city's ground stood still (no paths); spike 7's smearing check used a
  hopping slime and a blaze's turning rods, V standing still and turning. Spike 9's mobs walk all the time.
- **CyberCraft Tweaks went 1.6.0 to 1.7.1:** pathfinding only (`dev.cybercraft.tweaks.paths`, mixins on Minecraft's
  pathfinding, sight and pets). Its drawing mixins (`KeepDrawOrder`, `MarkEmissive`, `MarkBlended`) are unchanged.
- **BlockRender is 0.23.0** (spike 8's), unchanged by spike 9.

## Hypotheses, cheapest test first

1. **EMF and ETF.** The one big change in what SkyCraft captures. Test: with Minecraft stopped and the user's leave,
   rename both jars `.jar.disabled` (and Entity Sound Features, to be clean), start again, look at the same mobs.
   Gone means it's how SkyCraft's capture meets EMF's models or ETF's textures; then read SkyCraft's capture
   (`AvatarExporter`) against them, or keep them disabled for the game, as the user chooses.
2. **Motion vectors on moving mobs.** Spike 7 writes entities' motion vectors from the camera's movement alone ("as the
   blocks", `EntityVS`'s `prevClip = mul(clip, clipToPrevClip)` in `shaders/blocks.hlsl`): a walking or animated mob's
   own movement isn't in them, so DLSS's temporal accumulation reprojects last frame's texels to the wrong place.
   Spike 7 saw no smearing, but its mobs barely moved. Tests, no code needed:
   - a mob that doesn't move (the overlay's Mobs row's Spider has `NoAI:1b`; or `/summon` a cow with `NoAI:1b`) beside
     a walking one;
   - `set gbmotion 0` (leave the game's motion vectors) and back to 1;
   - `mode predlss` (draw before DLSS instead of into the G-buffer) and back to `mode gbuffer`;
   - the game's upscaler off or switched (its graphics settings), if the user agrees.

   Any of these clearing it points here; the fix is per-vertex motion (the previous frame's position for each entity
   vertex, which SkyCraft's scenes don't carry today) or DLSS's bias hint over the models (spike 7's planned
   `dlsshint 2`, never built since run A didn't need it).
3. **Layers at the same depth.** Spike 7's spec risk 16: a layer drawn over its own body (a wolf's collar, a villager's
   clothes, a horse's markings) passes the greater-or-equal depth test only if both get the same depth. Test: a
   single-layer mob (a pig, a cow) against layered ones; `set blend 0`. All mobs being hit argues against this alone.
4. **Texture bookkeeping.** Entity textures live in one `StructuredBuffer` (`src/render/EntityTextures.*`), each draw
   reading `textureStart`, `textureWidth`, `textureHeight`; `EntityTexel` samples the nearest texel with no mipmaps, the
   UV clamped. A wrong start or size after the buffer grows, or a texture replaced, would show another texture's texels.
   Tests: the `entities` request (BlockRender lists the scene's batches, textures and sizes in its log); `mclink`'s
   `save-textures <folder>` saves every entity texture SkyCraft sent, to compare with Minecraft's jar.

## Where things are

- **The drawing path** (spike 7): SkyCraft sends `kRenScene` (each frame's entities as batches of vertices) and
  `kRenTexture`; BlockRender's link parses them (`src/link/Messages`), `src/entities/Entities.*` makes a frame's
  geometry, `src/render/Snapshots.*` its upload ring, `src/render/EntityTextures.*` the textures' buffer, and
  `src/render/Renderer.cpp` draws them: into the G-buffer (`EntityPS`), their depth into the tagged depth
  (`EntityDepthPS`), blended layers and particles in the forward list before DLSS (`TranslucentPS`), the bias hint
  (`HintPS`). Shaders: `shaders/blocks.hlsl`, `EntityVS` from about line 215, `EntityTexel` about 236.
- **Settings and requests** (`src/render/Renderer.cpp`, `src/Requests.cpp`): `set gbmotion 0|1`, `set blend 0|1`,
  `set dlsshint 0|1` (particles only), `set entities 0|1`, `mode off|present|predlss|gbuffer`, `entities`, `dump
  depth|mvec|color|gbuffer|hint|all` (the frame's buffers as DDS, viewed with `tools/view_dump.py`).
- **CyberCraft Tweaks' drawing mixins:** `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/`
  (`KeepDrawOrder`, `MarkEmissive`, `MarkBlended`).
- **SkyCraft's source:** session `818fd1fd`'s scratchpad, `skycraft/` (SkyCraft 0.1.2, `bfcaf17`), its entity
  capture in `fabric/src/client/java/dev/skycraft/client/render/AvatarExporter.java`. The installed jar is
  `%APPDATA%\ModrinthApp\profiles\CyberCraft\mods\skycraft-fabric-0.1.2.jar`.
- **Spike 7's docs:** the spec `docs/superpowers/specs/2026-10-06-minecraft-entities-design.md` ("Step A", the
  motion vectors "from camera movement", risk 16), the plan `docs/superpowers/plans/2026-10-06-minecraft-entities.md`
  (Parts 1 to 5), the results (runs A to D).
- **Reproducing it:** the game linked, the overlay's Fun header: the Paths rows (Pig, Cow, Villager, Wolf, Bee, Husk)
  summon mobs that walk; the Mobs row has still ones. Clear removes them all.

## The game and Minecraft, right now

- **Cyberpunk:** closed. BlockRender 0.23.0 in `E:\Cyberpunk2077\red4ext\plugins\BlockRender\`, DRED on; the overlay
  (`bin\x64\plugins\cyber_engine_tweaks\mods\blockrender_hud\init.lua`) is the repo's, with spike 9's Paths rows and
  "Run F's spots". V was last in her apartment in Megabuilding H10.
- **Minecraft:** the Modrinth App's `CyberCraft` instance: Minecraft 26.3, Fabric Loader 0.19.5, Fabric API 0.161.0,
  SkyCraft 0.1.2, CyberCraft Tweaks 1.7.1, EMF, ETF and Entity Sound Features enabled. The player is in survival; its
  hotbar slots 8 and 9 hold wheat and a dandelion from run F (the overlay's "Your slots" gives the TNT and the flint
  and steel back). The world was autosaved after run F.

## Rules to keep

- **No sub-agents:** work inline.
- **The user does every in-game and Minecraft action:** launching and quitting, loading, moving, keys, clicks,
  screenshots. Ask, then wait. On-screen aids for positions and headings (the overlay).
- **Read-only requests are fine:** BlockRender's request file in the plugin's folder (`mc /data get …`, `entities`,
  `dump …`), atomically (`printf … > request.tmp && mv request.tmp blockrender.request`), as runs A to F did.
- **Installs and deploys:** BlockRender's DLL and shaders only with the game closed; Minecraft's mods only with
  Minecraft stopped, the old jar renamed `.jar.disabled`; each with the user's leave. The overlay alone was once copied
  with the game running at the user's choice; it's no general licence.
- **Minecraft saves by its autosave** (every 5 minutes): after the last change, wait for one before the user stops it.
- **`mclink`** runs only with the user's leave and the game closed.
- **Fixes in proportion:** an edge case gets the smallest checked fix; say what works and how big a problem is first.
- **Deep reviews** of any spec or plan, when asked: trace claims to the source, simulate the in-game steps, break new
  tests once.
- **Commits** end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Gotchas

- **`render_tests.exe` runs from `spikes/block-render`:** it opens the shader file and the `mclink` scripts relative to
  it; from the repo's root 37 tests fail for files not found.
- **Loading a save in a running game** moves the city into another PhysX scene, and BlockRender's exporter reads
  scene 0 only: no city, no build mode, until the game restarts (spike 9's results). Start the game fresh for tests.
- **SkyCraft waits on an open Minecraft screen** (the Options screen) before opening its world; close it.
- **Bash collapses `\\` to `\`,** even in quoted heredocs: write files with backslashes with the Write or Edit tools.
- **Line endings differ by file** (`core.autocrlf` is on); `sed -i` makes CRLF files LF. Edit with the Edit tool or
  Python that keeps the endings.
- **Other follow-ups from spike 9,** separate from this one (its results' "Left for later"): the exporter's PhysX scene
  after a save load; SkyCraft lifting Minecraft's player onto a doorway's header; mobs 0.9 wide at narrow doors. And
  the time-of-day sync the user asked for during spike 7.
