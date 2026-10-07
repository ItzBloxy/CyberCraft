# Minecraft's entities in Night City: design (spike 7)

Date: 2026-10-06. Status: agreed with the user section by section in the brainstorm, after a self-review of the three
approaches that the user asked for ("Decisions from the brainstorm"). "Found while writing", near the end, lists what
writing it out added to what was agreed. Then revised after a deep self-review, which checked each claim against its
source and walked through step 0 and run A as the user would play them: "Changes from the deep review", at the end,
lists what changed and what was checked. Planning Part 1 settled more, which "Changes while planning Part 1" lists,
and a read before executing it corrected step 0's script ("Changes before executing Part 1"). Step 0's results, and
planning Part 2 from them, settled more: "Changes while planning Part 2", and "Changes while executing Part 2". The
sections above already include all of those changes.

## Purpose

This is spike 7 of the project that recreates [SkyCraft](https://github.com/chasmlol/SkyCraft) (Minecraft in Skyrim)
for Cyberpunk 2077. Night City shows Minecraft's blocks (spike 3), its HUD (spike 4), and lets the user build as
Minecraft's player (spike 5) and fight with Minecraft's weapons (spike 6). Nothing else Minecraft draws is there:
- **entities:** lit TNT, falling blocks, minecarts, boats, mobs. In spike 6's step 5b, as the handoff tells it, a TNT
  block was drawn until lit, then vanished, and went off 4 s later where it was;
- **block entities,** which Minecraft draws apart from its section meshes: chests, banners, and pistons while they
  move. Beds and signs are block models in 26.3, in the section meshes, so they're drawn with the blocks already;
- **particles:** explosions, smoke, block debris, crits;
- **dropped and thrown items, arrows and tridents;**
- **the block-breaking cracks and the targeted block's outline.**

SkyCraft's unchanged mod already sends all of it whenever it's connected to the game's mapping, Link on or off, and
BlockRender steps over every message of it. This spike draws it:

> Can Minecraft's entities live in Night City: drawn where Minecraft has them, posed and animated as Minecraft poses
> them, lit by the game as its own geometry with Minecraft's glow kept, moving without smearing, at little cost?

It has two steps of code and an optional third, each tested against something real as soon as it's built (see "The
steps"):
1. **Step A, into the G-buffer:** everything opaque (mobs, lit TNT, falling blocks, block entities, opaque particles,
   which make a whole TNT explosion) and the world entities (items, arrows, tridents, cracks), lit by the game as the
   blocks are, with Minecraft's glow written as the G-buffer's emission.
2. **Step B, a forward pass before DLSS:** translucent particles blended, the targeted block's outline in build mode,
   and DLSS's bias hint under the particles, which have no motion vectors of their own. Run A found step A's emission
   works, so there's no unlit glow.
3. **Step C, where a step shows a need: the Minecraft side.** CyberCraft Tweaks keeps Minecraft's draw order for
   layered models (villagers' clothes, spider eyes) and marks emissive render types, which SkyCraft's capture drops.
   Step 0 showed the draw order is needed, so it comes before step A; the emissive mark waits for run B's look at the
   spider's eyes at night, which run A didn't take.

## Decisions from the brainstorm

- **Scope: SkyCraft's scene and its world entities.** The scene (`kRenScene`, with `kRenTexture` for its textures)
  carries entities, block entities and particles; the world-entity table carries items, arrows, tridents, cracks,
  contact shadows and the outline. The user left out animated textures (`kRenAtlasRegion`), translucent blocks (water,
  stained glass), the player's own body and Minecraft's lights.
- **Drawing only.** Nothing of Minecraft's becomes solid for V: no PhysX bodies for boats, minecarts or mobs.
  Minecraft's own collision still applies to Minecraft's player, since Minecraft runs it.
- **Lit by the game, with Minecraft's glow kept.** Opaque things go into the G-buffer as the blocks do, so Night City's
  sun, lamps, haze and ambient occlusion light them. What Minecraft draws full-bright (explosion puffs, lava particles,
  lit TNT's flash, blazes, magma cubes, allays, vexes) still glows at night. The user chose this over the game's light
  without glow, and over Minecraft's own lighting in a forward pass.
- **Approach 1, staged after a self-review.** Three approaches were proposed: the G-buffer plus a forward pass before
  DLSS (1); everything in the G-buffer, translucent things dithered (2); everything forward with our own light (3, ruled
  out by the lighting decision). The user asked for a concrete self-review of them, which checked each claim against
  BlockRender's code, SkyCraft's source and Minecraft 26.3's classes. It found five claims wrong or incomplete and eight
  things none of the approaches had. The changes, all in the sections below:
  - **what glows:** block light 15 marks explosion puffs, lava particles, lit TNT's flash and blazes, but not eyes,
    whose render type, not their light, makes them glow in 26.3 ("Minecraft 26.3");
  - **how the scene is drawn:** two-sided and with a greater-or-equal depth test, unlike the blocks, or banner patterns
    vanish; and the shader's direction 7 needs the triangle's own normal;
  - **cracks** multiply into the G-buffer's base colour as a decal, as Minecraft's crumbling blend does; the G-buffer's
    base colour is blendable and the game has its own decal pass into it;
  - **blended particles** are few: none of a TNT explosion's particles is blended;
  - **layered models** lose Minecraft's draw order in SkyCraft's capture (step C);
  - **DLSS's bias hint,** which the game tags, can stop smearing without touching Minecraft;
  - **contact shadows** may come from the game's ambient occlusion, checked before any are built;
  - **entity textures** must outlive Unlink and the Clear request, since SkyCraft sends each once;
  - **one snapshot a frame** for both passes; **the outline** may break up under DLSS.

  Approach 1 stayed, staged so its first half (step A) is approach 2 without the dithering, and can be tested alone.
- **The numbers are read from the sources.** The cut-out thresholds, depth test, crack blend, outline colour and width
  are Minecraft 26.3's own (checked in its jar); the glow rule is the light Minecraft gives a vertex; the bias hint is
  the game's own buffer. The one hand-set value is step B's `exposure` for blended particles: the game tags no exposure.
- **Testable steps first.** `mclink` reads what Minecraft really sends before anything is drawn (step 0); step A is
  checked in the game before step B is built; step C is built only where a step shows it's needed (its draw order, after
  step 0).

## Non-goals

- **Solid entities** for V, cars or NPCs.
- **Animated textures** (`kRenAtlasRegion`, drained and counted as now): entities and particles that use an animated
  sprite show its first frame, as blocks do.
- **Translucent blocks** (water, stained glass, ice): sections still drop their translucent triangles, unless
  `set translucent 1` keeps them as cut-outs, as now.
- **Minecraft's lights** (`kRenLights`) and **sun shadows** for anything of Minecraft's: entities aren't in the game's
  shadow maps, as the blocks aren't.
- **The player's body** (`kRenAvatar`, empty in first person, where build mode runs) and **its ragdoll**
  (`kRenRagdoll`).
- **What SkyCraft's capture skips:** shadows, name tags, text (signs' words), flames on burning mobs, leashes, outlines
  and custom geometry (beacon beams, guardian beams). SkyCraft's own contact shadows (`kWeShadow`) wait for run A's
  check of the game's ambient occlusion.
- **Arrows stuck in NPCs** (`kEvArrowStuck`, drained and counted as now): an arrow that hits an NPC's stand-in vanishes.
- **Translucent entity parts** (a slime's outer layer; allays and vexes, which Minecraft draws translucent): SkyCraft
  captures every model as a cut-out.
- **Changes to SkyCraft's mod.** Protocol 11 stays. CyberCraft Tweaks changes only in step C, and only where a step
  shows the need.
- **Only these versions:** game 2.31, SkyCraft 0.1.2, Minecraft 26.3, Fabric Loader 0.19.5.

## Environment (checked on 2026-10-06)

Each fact below was read in its source:
- SkyCraft's code at `bfcaf17` (the clone in session `818fd1fd`'s scratchpad) and the protocol header;
- the user's Minecraft 26.3 jar (`%APPDATA%\.minecraft\versions\26.3\26.3.jar`), its classes read with `javap -c`, its
  shaders and textures unpacked;
- the CyberCraft instance's `options.txt`;
- BlockRender's code at `a1db95d`, and spike 3's results (`docs/spikes/2026-10-01-block-render-results.md`).

### The game and BlockRender

- **BlockRender 0.19.1** (`bd7acca`, module version 7) is installed, with DRED on (`dred = 1` in `settings.txt`). Link
  switches drawing to `gbuffer` if it's off (`LinkRequest`); Unlink leaves drawing as it is.
- **Its frame** (`Plugin.cpp`, `OnUpdate`): requests once a second; `UpdateLink`, which runs `Link::Update` (the rings
  drained, the sink called); then the fight, the NPC list and V's hurts; then `g_renderer->Update()`, which uploads.
- **The link** (`Link::Take`) keeps `kRenAtlas`, `kRenSection`, `kRenClearAll` and `kRenSolids`; every other type is
  counted and stepped over (`DrainByteRing`'s visitor returns false: consumed, not counted against the ring budget).
  Sections are skipped until the atlas is seen (`sectionsBeforeAtlas`). `Link::Update` runs every frame once the
  mapping is open, Link on or off, writing the game's heartbeat, so SkyCraft exports all the while.
- **The mapping** (`Mapping::Open`) zeroes the world-entity table, and the render ring's head and tail, whenever it
  opens the mapping, created or reused. Nothing reads the table.
- **The ground** is the city's by default since spike 5 (`Plugin.cpp`), the floor if PhysX's audit fails: BlockRender
  sends Night City's shape as triangles and voxels (`kColTris`, `kColRegion`). SkyCraft adds the voxels to every
  entity's block-collision queries (`BlockCollisionsMixin`, `SkyCollision.readRegion`), so Minecraft's mobs, items and
  TNT stand on Night City's ground; on spike 4's floor, which is triangles only, they fall through (`Commands.cpp`'s
  `tnt-ahead`). The city's area (`GroundAreas`) reaches 5 regions of 8 blocks out from the centre's region
  horizontally, 3 below and 2 above; SkyCraft keeps what it was sent, so things stand wherever V has been, and fall
  beyond.
- **The renderer** (`render/Renderer.cpp`):
  - **in `gbuffer` mode** one list of ours goes before the list that ends the G-buffer (`ListBefore`, `RecordGbuffer`):
    the blocks into base colour, normals, material and motion vectors (`R10G10B10A2_UNORM`, `R10G10B10A2_UNORM`,
    `R8G8B8A8_UNORM`, `R16G16_FLOAT`) with the G-buffer's `D32_FLOAT_S8X24_UINT` depth and stencil, then a depth-only
    pass into the tagged depth. Its camera and jitter come from the frame's DLSS evaluate (`latest_`);
  - **in `predlss` mode** one list goes before the DLSS list (`RecordPredlss`), into the tagged colour
    (`R16G16B16A16_FLOAT`), motion vectors and depth, its barriers starting from the states the DLSS list found them in
    (`StateBeforeList`). The two modes exclude each other;
  - **its lists** come from a ring of 8 (`kSlots`, "more than Streamline's 6"), one per recording; the HUD pass has its
    own ring;
  - **the root signature:** b0 the frame's constants, b1 four 32-bit constants (a section's offset), t0 the vertices,
    t1 the atlas, t2 step 2's depth copy, all root descriptors, no descriptor tables;
  - **its pipelines** cull back faces (`set cull`, 1) and test depth with a strict greater (reversed depth) or less
    (`EnsureGbufferPipeline`);
  - **uploads** go through its upload list in `Update` (`Upload`), sections into committed buffers, retired with a fence
    (`Retired`).
- **The shader** (`shaders/blocks.hlsl`):
  - **the atlas** is a buffer of every level (`SampleAtlas`), the level picked from the UVs' screen derivatives;
  - **a texel** is discarded under alpha 0.5;
  - **a face's direction** is the flags' bits 4 to 7 into seven-entry tables (`kFaceNormal[min(i.face, 6)]`), so
    SkyCraft's direction 7 would read as east;
  - **`GbufferPS`** writes the material target as metalness, roughness, B and 0.
- **The game's settings,** as spike 4 recorded them on 2026-10-02: DLSS Quality at 2560 × 1440 (a render size of
  1707 × 960); frame generation, ray tracing, HDR and VSync off; Reflex on. Run A checks them again.

### The game's frame (spike 3's results)

- **The G-buffer** is albedo, normals and material values, plus motion vectors for some objects; after it the game
  draws **decals** into the same three targets, its depth read-only, then a **forward and transparent** pass into the
  scaling input colour, a null target and an `R8_UNORM` target, against the tagged `D32_FLOAT`, mostly read-only.
- **The material target's A** was above 0 only on windows and signs, up to 0.93: likely emission. Nothing has written
  it yet.
- **The tags** at the DLSS evaluate: depth, motion vectors, the scaling input colour, `biasCurrentColorHint`
  (`R8_UNORM`, 1707 × 960, render target and UAV, in UAV state 0x8 at the evaluate) and the output colour. There is no
  exposure tag.
- **The game's ambient occlusion likely sees our G-buffer depth:** the road under step 4's cube darkened, "most likely
  the game's ambient occlusion" (run 8).

### SkyCraft's Minecraft side

- **Each Minecraft frame** (`WorldExporter.frame`, render thread, while `SkyLink.active()`): sections are meshed, the
  animated sprites patched, the world-entity table written (`exportEntities`), then `AvatarExporter.frame`: the avatar,
  the scene, and about once a second the ragdoll.
- **The scene** (`AvatarExporter.exportScene`):
  - **what's in it:** the entities within 64 blocks of Minecraft's camera, the first 48 in `entitiesForRendering()`'s
    order, not the nearest, leaving out the player, items, arrows, thrown items and our NPCs' stand-ins
    (`SkyrimActorEntity`); the block entities in the loaded chunks within 48 blocks, at most 256; all particles
    Minecraft extracted this frame;
  - **how it's caught:** Minecraft's renderers submit into SkyCraft's collector, posed and animated. Models, items,
    block models, moving blocks and particles are kept; shadows, name tags, text, flames, leashes, outlines, crumbling
    overlays and custom geometry are skipped;
  - **batches** are kept per texture, UV mode and flags in a `HashMap`, and sent in its iteration order, each batch's
    vertices in submission order;
  - **positions** are relative to the scene's origin, the whole block under Minecraft's camera (`Math.floor`);
  - **flags:** every model, item, block model and moving block is `SOLID` (`1 | 8 | 7 << 4`: cut-out, no mipmaps, lit
    by the triangle's own normal), its batch not translucent; particles are `PARTICLE` (`1 | 8`) or, for a layer
    Minecraft draws blended, `PARTICLE_BLENDED` (`2 | 8`), whose batch is flagged translucent (bit 0);
  - **overlays:** Minecraft's red hurt flash becomes a red tint in the vertex colour; its white flash (lit TNT about to
    go) becomes full light, `0xF000F0`;
  - **`order(int)` returns the collector itself,** so Minecraft's draw order between layers is lost.
- **Textures** (`kRenTexture`): a texture besides the combined atlas goes the first time a batch uses it, with an id
  from 1. A texture atlas other than the blocks' and items' (chests', banners', signs', particles') goes whole, as one
  texture. If the ring is full, the texture isn't counted as sent, and is tried again at its next use. All of them go
  again after `AvatarExporter.reset`, which `resendEverything` calls when SkyLink's generation changes: when Minecraft
  opens the mapping, or when the game's process id in it changes. Unlinking and linking again in one game session
  changes nothing.
- **The ring:** a scene goes with `tryWriteRender` (dropped if the ring is full; the next frame replaces it); a texture
  with `writeRender` (waits up to 1 s for room).
- **The world-entity table** (`WorldExporter.exportEntities`, `SkyLink.writeWorldEntities`): `seq` is made odd, the
  records written, then made even. Up to 160 records within 96 blocks of the player's eye:
  - **arrows** (`kWeArrow`): the position, yaw and pitch; `uv[0]` the side view and `uv[1]` the back plate of the
    arrow texture, which `SkyAtlas` puts in the combined atlas;
  - **tridents** (`kWeTrident`): `uv[0]`, the trident's item icon;
  - **dropped items and thrown items** (`kWeItem`): the item's icon, `scale` 0.5, turning about the vertical (`yaw`);
  - **dropped full blocks** (`kWeBlock`): a cube of side `scale` (0.25), `uv[0..2]` side, top and bottom, `tint` for the
    top (grass, leaves);
  - **cracks** (`kWeCrack`): the box at `x y z` of size `ext` over the block's shape, inflated by 0.004, `uv[0]` the
    stage's texture in the combined atlas;
  - **contact shadows** (`kWeShadow`): the feet and width of players and mobs;
  - **the outline:** `hasSelection`, `selMin`, `selMax`, the targeted block's shape, or the block a held block would
    fill on the game's ground; none while a Minecraft screen is open.
- **Arrows and tridents** hit the game's exact surfaces and stick there (`AbstractArrowMixin`, `SkyClip.refine`), so a
  `kWeArrow` record sits where it hit Night City. One that hits an NPC's stand-in is gone, as vanilla Minecraft
  discards an arrow that hits a creature, and SkyCraft reports `kEvArrowStuck`.
- **Minecraft's camera and options:** outside build mode the camera follows V's look (`SkyClient` sets the player's
  yaw and pitch from `SkyState`); in build mode Minecraft's look leads and V follows. The CyberCraft instance's FOV
  option is 90° (`fov:0.5` in `options.txt`, stored as (FOV − 70) / 40), against the game's 68.24°, so Minecraft's own
  particle culling (`Frustum.pointInFrustum` while particles are extracted) drops nothing the game shows. Its particles
  are at All (`particles:0`), and its render distance 8 chunks.

### SkyCraft's game side, as a reference

`skse/src/WorldRender.cpp`:
- **draws** the scene and the world entities with no culling (`CULL_NONE`) and a less-or-equal depth test, solid
  batches first, then blended ones, then the cracks (alpha-blended) and the outline (a line list, black at 45%);
- **builds** the world entities' geometry each frame (`BuildEntities`, `Box`, `BuildArrow`, `Quad`, `BuildOutline`),
  arrows at 0.55 of Minecraft's size, to suit Skyrim's people;
- **writes motion vectors from camera movement alone** ("Minecraft blocks don't move");
- **darkens the ground** under players and mobs from `kWeShadow` in its own shader (`BlobAO`) and a full-screen pass.

Its depth test with `HashMap` batch order would show layered models wrongly in Skyrim too (read, not seen), so its look
there would prove nothing about layers.

### Minecraft 26.3

Read from the jar:
- **Depth:** the default depth test is greater-or-equal (`DepthStencilState.DEFAULT`, reversed depth).
- **Culling:** `EntityModel`'s default render type is `entityCutout`, whose pipeline (`entity_cutout`) culls nothing,
  as `entity_translucent` and `entity_translucent_emissive` don't: Minecraft draws most entity models two-sided itself.
- **Bright mobs:** blazes, magma cubes, allays and vexes return block light 15 (`getBlockLightLevel`); allays and vexes
  are drawn translucent (`entityTranslucent`), as a slime's outer layer is.
- **Cut-outs:** `ALPHA_CUTOUT` is 0.5 for cut-out blocks and terrain, 0.1 for entities, items and armour; the particle
  and crumbling shaders discard where texel × vertex colour has alpha under 0.1.
- **Cracks:** the crumbling pipeline blends `DST_COLOR, SRC_COLOR`, which is 2 × crack × what's there. The
  `destroy_stage` textures checked (stages 0, 5 and 9) are 16 × 16, alpha 1 or 255; their kept texels are grey 61
  (darkening to 48%) or 155 (brightening by 22%).
- **Particles' layers:** `HugeExplosionParticle`, `ExplodeParticle`, smoke (`BaseAshSmokeParticle`), flames, lava,
  crits, drips, dust and most others are opaque; `SpellParticle` (potions), `CampfireSmokeParticle`,
  `SimpleAnimatedParticle` (firework sparks, end rods, totems, squid ink), the firework flash, sculk, shriek,
  vibration, sonic boom, fireflies, player clouds and `EmissiveRisingParticle` are translucent. Block debris takes its
  layer from its sprite (`Layer.bySprite`).
- **Particles' light:** `HugeExplosionParticle` returns `0xF000F0` (block and sky light 15); lava particles and glowing
  drips set block light 15; glow particles rise toward it over their life. Flames don't override their light: they
  take the world's, 14 beside a torch.
- **Eyes:** `EyesLayer` passes the entity's own light and draws at `order(1)`; the glow is the render type's
  (`EMISSIVE` in `entity.fsh`, no light map), which the capture can't see. `spider_eyes.png` has 20 opaque texels and
  `enderman_eyes.png` 6; the rest is transparent.
- **Layers by order:** `renderColoredCutoutModel` takes an order; `VillagerProfessionLayer` draws the biome, profession
  and level textures at orders 1, 2 and 3 over the body. Banners draw their cloth, then `BANNER_PATTERN_BASE` at
  `order(1)` and each pattern at `order(i + 2)`, all in one atlas (`textures/atlas/banner_patterns.png`).
- **The outline:** black at alpha 102 (40%), `max(2.5, window width / 1920 × 2.5)` pixels wide: 3.3 at 2560 wide, about
  2.2 at the render width of 1707.

## The protocol, as this spike uses it

SkyCraft's header (`extern/skycraft/skycraft_protocol.h`, version 11) stays unchanged.
- **`kRenTexture` (4):** `RenTexture` (`id`, `width`, `height`, `pad`), then `width × height` RGBA8 pixels, top row
  first.
- **`kRenScene` (6):** `RenScene` (the origin as three doubles, `batchCount`, `vertexCount`), then
  `RenBatch[batchCount]` (`texture`: 0 the combined atlas, else a texture id; `first`; `count`; `flags`, bit 0
  translucent), then `RenVertex[vertexCount]`, triangle lists. A scene of no batches says nothing is there.
- **`RenVertex`** is byte for byte BlockRender's `Vertex`. Its flags: bit 0 cut-out, bit 1 translucent, bit 3 no
  mipmaps, bits 4 to 6 a face's direction + 1, where 7 means the triangle's own normal and 0 none. BlockRender reads
  bits 4 to 7 as the direction now; this spike reads 4 to 6, leaving bit 7 and up free. The light: block light in bits
  0 to 7, sky light in 8 to 15.
- **`kRenClearAll` (3)** now also drops the textures and the scene.
- **The world-entity table** at `0x1C000`, read under its sequence lock: `seq`, `count`, `hasSelection`, `selMin`,
  `selMax`, then up to 160 `WorldEntity` records of 96 bytes (`kind`, `id`, `x y z`, `yaw`, `pitch`, `scale`, `ext`,
  `uv[3][4]`, `tint`).
- **Step C** adds meaning to free vertex-flag bits (bit 8 up), which SkyCraft's own game side ignores; it changes no
  layout.

## Repository layout

```
docs/superpowers/specs/2026-10-06-minecraft-entities-design.md   this file
docs/superpowers/plans/2026-10-06-minecraft-entities.md          the plan
docs/spikes/2026-10-06-minecraft-entities-results.md             the results
spikes/block-render/
  src/link/Messages.*          + kRenTexture and kRenScene, checked against their layouts (pure)
  src/link/Rings.*             + the world-entity table, copied under its sequence lock (pure)
  src/link/WorldTable.*        the world-entity table's records and outline, checked (pure)
  src/entities/Entities.*      the textures, the newest scene and world entities, and each frame's geometry (pure)
  src/entities/WorldShapes.*   items, cubes, arrows, tridents, cracks and the outline as triangles (pure)
  src/render/EntityTextures.*  where each texture goes in the one buffer, and when it grows (pure)
  src/render/Snapshots.*       the snapshot ring: which buffer a frame writes, and when one is free again (pure)
  src/render/Renderer.*        + the entity textures and snapshots on the GPU, step A's draws, step B's list
  shaders/blocks.hlsl          + the entity, crack, forward and outline shaders
  src/link/Link.*              + textures, scenes and the world-entity table to the sink
  src/Plugin.cpp, Requests.*, Status.*   + the settings, the entities request and the entities status line
  tools/mclink/                + scenes, textures and the world-entity table reported, and entities.txt
  tools/blockrender_hud/init.lua         + the entities line, the Spawn row and Clear
  tests/                       + the new unit tests
minecraft/cybercraft-tweaks/   (step C only) + the capture's draw order and emissive flag (1.4.0)
```

The names are proposals; the plan settles them.

## Build

- **The libraries:** `render_core` gains the pure parts (`link/WorldTable`, `entities/`, `render/EntityTextures`,
  `render/Snapshots`, and the additions to `link/Messages` and `link/Rings`), so `mclink` and the tests use them;
  `render_runtime` keeps the renderer.
- **The versions:** step A deploys 0.20.0 and step B 0.21.0, in the log line and in `Query`. Module version 7 stays: no
  redscript changes.
- **CyberCraft Tweaks 1.4.0,** step C only, built by its own `build.sh` with the JDK alone, as in spikes 5 and 6, and
  copied into the CyberCraft instance's mods in place of 1.3.0, with Minecraft stopped.

## The frame

On the game's main thread, in `OnUpdate`:
1. `Link::Update` drains the render ring: textures and scenes go to the sink, which hands them to `Entities`. Then it
   reads the world-entity table, which also goes to `Entities`.
2. After the fight, the NPC list and V's hurts, `Entities` builds the frame's geometry, and the renderer takes it
   (`SetEntities`).
3. `g_renderer->Update()` uploads new textures, and copies the frame's geometry into a free snapshot buffer.

On the game's render threads:
4. **At the G-buffer's end,** our list draws the blocks, then the newest snapshot's opaque things, then the cracks, then
   writes the blocks and the opaque things into the tagged depth. The snapshot it used is kept for the frame.
5. **Before the DLSS list** (step B), our second list draws that same snapshot's forward things.

## The link: what comes in

### Textures

`ParseTexture` takes a `kRenTexture` payload: the id at least 1, each side from 1 to 4,096 (SkyCraft's own game side's
limit), and the payload at least 16 bytes plus 4 a pixel. A good texture goes to the sink as `Texture(id, width, height,
pixels)`; a bad one is rejected and counted, as a bad section is.

### The scene

`ParseScene` takes a `kRenScene` payload: the origin finite, at most 65,536 batches and 1,048,576 vertices (32 MB, the
size SkyCraft's own ring allows a message, half of 64 MB), each batch's `first + count` inside the vertices and its
count a multiple of 3, the payload long enough for both. A good scene goes to the sink as `Scene(view)`, its payload
valid only during the call; a bad one is rejected whole and counted. Vertices aren't checked one by one: the shader
treats any position, UV or flag as data, and a NaN only drops its triangle.

### The world-entity table

`ReadWorldEntities` copies the table when `seq` is even, then reads `seq` again: the copy stands if it hasn't changed,
64 tries at most, as `ReadMcState` copies Minecraft's state. If none stands, the link keeps the last good table.
`CheckWorldTable` reads at most 160 records, whatever the count says, keeps those whose kind is 1 to 6 and whose
numbers are finite, and counts the rest as left out; it takes the outline when `hasSelection` is set and its numbers are
finite. The link reads the table each update while Minecraft is in its world, and hands the sink the table every
update.

### Coming and going

- **Before the atlas,** textures and scenes are stepped over, as sections are. The mapping's opening empties the ring,
  but Minecraft may write for up to a frame before it sees a new game process, with the old generation's ids, which
  SkyCraft reuses after its clear. They're counted (`beforeAtlas`).
- **`kRenClearAll`** drops the textures (in `Entities` and on the GPU) and the scene.
- **Minecraft gone or out of its world** (its heartbeat, or `kMcInWorld` clear): the scene and world entities are
  emptied, so nothing stale is drawn. The link empties the table it hands on; `Entities` empties the scene, which
  Minecraft stops sending when it leaves its world. The textures stay; a new Minecraft process starts with its own
  clear.
- **Unlink, the Clear request, and drawing switched off** keep the textures, since SkyCraft wouldn't send them again.
  Only a clear from Minecraft drops them.

## Entities: one frame's geometry

`Entities` runs on the main thread and calls no GPU, so every rule below is unit-tested without the game.

- **What it keeps:** the textures by id (their pixels, until the renderer has uploaded them), the newest scene (copied
  from the payload), and the newest world-entity table.
- **What it builds each frame,** an `EntityFrame`:
  - one vertex list: the scene's vertices as they came, then the world entities' triangles;
  - one draw list, each draw a first vertex, a count, a texture (0 the atlas, else an id) and a kind: `model` (the
    scene's batches not flagged translucent, and the world entities' solid shapes), `translucent` (the scene's
    translucent batches), `crack` or `outline`;
  - the origin: the scene's, or, with no scene, the whole block under Minecraft's eye (`McState`).
- **The world entities' shapes** (`WorldShapes`), relative to the origin, as `WorldRender.cpp` builds them:
  - a **dropped item:** one quad of its icon, `scale` wide (0.5), turned by `yaw` about the vertical, two-sided;
  - a **dropped full block:** a cube of side `scale` (0.25) about its centre, turned by `yaw`, side, top and bottom
    textures, the top tinted, every face lit by its own normal;
  - an **arrow:** Minecraft's arrow model, two fins crossed at 45° and a back plate, along `(sin yaw cos pitch, sin
    pitch, cos yaw cos pitch)`, at Minecraft's size, not SkyCraft's 0.55: Night City's people are Minecraft's height;
  - a **trident:** its icon on two crossed quads, the diagonal along its flight;
  - a **crack:** the box's six faces with the stage's texture, as a `crack` draw;
  - the **outline:** the box's 12 edges, inflated by 0.002 as SkyCraft does, as an `outline` draw (step B);
  - **contact shadows:** not built in step A ("Run A");
  - every vertex carries the cut-out and no-mipmap flags and sky light 15, as SkyCraft's do, so none of them glows.

## The renderer

### Entity textures on the GPU

- **One buffer** (default heap, one `uint` per texel as the atlas), and a table from id to start, width and height,
  passed to the shader per draw.
- **New textures** are placed as they come and appended by the upload list in `Update`, as sections are. When the
  buffer is full, a bigger one is made, and the textures already uploaded copied over on the GPU: only those, since
  copies in one list aren't ordered and the new textures' copies follow. The old buffer is kept until that upload is
  done, since the upload reads it, and every texture is drawn again once it is.
- **A texture with a reused id** (only after a clear, which empties the table) goes in afresh.
- **A batch whose texture isn't on the GPU yet** is skipped that frame and counted. SkyCraft always sends a texture
  before the scene that uses it, so this lasts a frame.
- **The total is capped at 64 MB,** counting the pixels `Entities` keeps for upload, which pile up while drawing is
  off. A texture past it is left out, logged once, and its batches are skipped.

### The snapshot ring

- **A ring of 8 upload buffers** (CPU-written, GPU-read), each grown as needed up to 8 MB (262,144 vertices; a big
  explosion's scene is about 1 MB). `Update` copies the frame's geometry into a buffer no recorded list still reads, the
  newest one becoming the frame's snapshot. If none is free, the copy waits for the next frame. A frame over 8 MB keeps
  its draws in order while they fit, and the rest are counted as skipped.
- **A frame's snapshot is held** from its G-buffer list's recording until the GPU passes the fence after the last list
  of ours that reads it. The G-buffer list takes the newest (`TakeNewest`); when the snapshot has anything for the
  frame's forward list (step B), the G-buffer list takes it a second time for it (`Take`), and the forward list holds
  it to the fence after itself. If the forward list doesn't come, or is skipped, that hold is given back (`Abandon`),
  and the snapshot is held to the G-buffer list's fence alone. Holding it only to the G-buffer list's own fence would
  let `Update` rewrite it before the forward list reads it, mixing two frames' vertices under one frame's draws. The
  fence after a list, not before it, as `Retired` learnt from Task 52.

### Step A: into the G-buffer

Only in `gbuffer` mode: `predlss` and `present` keep drawing the blocks alone. In `RecordGbuffer`, after
`DrawSections`, while `set entities` is 1:
1. **The scene's `model` draws, in the order SkyCraft sent them,** then the world entities' `model` draws. Each draw
   sets its root constants: the origin's offset from the camera (as a section's corner is computed, `WorldFromMc` minus
   the camera's origin) and its texture's start and size.
2. **The `crack` draws.**
3. **The depth pass** into the tagged depth, as for the blocks: the blocks, then every `model` draw.

**Three new pipelines**, all with the blocks' root signature, grown to eight 32-bit constants in b1 and a fourth root
SRV (t3, the entity textures):
- **entities:** no culling, as Minecraft's own `entity_cutout`; depth test greater-or-equal (less-or-equal when the
  depth isn't reversed), so a later triangle on the same surface wins, as in Minecraft, and banner patterns show over
  their cloth; depth written; the four G-buffer targets, the motion vectors' write mask following `gbmotion`; the
  stencil class as the blocks' (`stencil`, 0);
- **entities' depth:** the same tests, depth only, into the tagged depth;
- **cracks:** no culling; depth tested greater-or-equal, not written; the base colour's RGB blended `DEST_COLOR,
  SRC_COLOR` (2 × crack × base colour, Minecraft's crumbling blend), its A and the normals, material and motion targets
  masked off; the stencil left alone. The base colour is sRGB-encoded in a UNORM target, as Minecraft's framebuffer
  is, so the blend's numbers are Minecraft's.

**The shader** (new entry points beside the blocks'):
- **texture:** texture 0 reads the atlas at level 0 (the flags' bit 3: no mipmaps, as SkyCraft marks every entity
  vertex); any other reads the entity buffer's texel at `uv × size`, clamped, nearest;
- **cut-out:** discarded where texel alpha × vertex alpha is under 0.1, Minecraft 26.3's threshold for entities,
  particles and cracks;
- **normal:** direction 7, the triangle's own normal, `normalize(cross(ddy(p), ddx(p)))` of the camera-relative
  position, turned toward the camera; direction 0 (particles), straight up, as the blocks' plants are now; 1 to 6, the
  table;
- **glow:** a triangle whose first vertex has block light of at least `glowlight` (a setting, 15 at first) writes
  `glow` (a setting, 0 to 1, 0.9 at first, near the 0.93 spike 3 found on the game's signs) into the material target's
  A, which spike 3 found only on windows and signs; every other triangle writes 0, as the blocks do. At 15 the rule
  takes explosion puffs, lava particles, lit TNT's flash, blazes, magma cubes, allays and vexes; 14 would add flames,
  which take the world's light, and anything standing beside a block of light 15;
- **colour:** texel × vertex colour (Minecraft's tint, the red hurt flash) × `brightness`, alpha 0, as the blocks;
- **material:** `metalness`, `roughness` and `materialb`, as the blocks;
- **motion:** from camera movement, as the blocks (`MotionVector`).

### Step B: before DLSS

A second list of ours, the forward list, put before the DLSS list (the DLSS evaluate's list, `latest_.list`) while in
`gbuffer` mode:
- **when:** only in a frame whose G-buffer list drew a snapshot with anything for it: translucent draws, or the outline
  while `set outline` is 1. That list leaves it the frame's DLSS list, tags, camera, constants (the jitter with them)
  and extent, and holds the snapshot for it ("The snapshot ring"); the forward list is recorded with those when that
  DLSS list is submitted, and reads what the G-buffer list wrote into the tagged depth. A claim whose forward list
  never came is let go at the next G-buffer list, and at a change of mode;
- **its lists:** a ring of 8 of its own, with its own constants and timing queries, as the HUD pass has; and its own
  thread-local record, since one submission could hold both of a frame's lists of ours;
- **its targets:** the tagged colour and `biasCurrentColorHint` as render targets, and the tagged depth read-only, each
  moved from the state the DLSS list finds it in and back, as `RecordPredlss` does. The motion vectors aren't touched;
- **skipped,** it lets the snapshot go, counts the skip and keeps its reason, and never switches drawing off.

What it draws, in order, each tested against the tagged depth, greater-or-equal as the entities, writing no depth and
culling nothing:
1. **The translucent particles:** blended (source alpha), the colour's alpha left alone. A particle with block light
   of at least `glowlight` is unlit (`Linear(texel × colour) × exposure`); any other takes Minecraft's light curve,
   `l / (4 − 3l)` of its brighter light level, times that. Under 0.1 of alpha is left out, as Minecraft's particle
   shader leaves it. `exposure` is `predlss`'s setting, 2.5, chosen by eye in spike 3's run 4: the one hand-set value,
   since the game tags no exposure.
2. **The outline,** while build mode is on, paused or not, with `set outline 1`: each edge widened in the vertex shader
   into a strip facing the camera, as Minecraft's lines shader widens its own, `max(2.5, output width / 1920 × 2.5)`
   output pixels scaled to the render width (about 2.2 pixels at 2560 and 1707); an end behind the camera is first
   brought along the edge to just in front of it. Black at Minecraft's 40%, blended so it darkens the scene's linear
   colour to `Linear(0.6)`, about a third, as Minecraft's darkens its sRGB framebuffer's colours to 0.6. Outside
   build mode Minecraft's player aims where V looks, but V's clicks don't reach Minecraft, so an outline there would
   mislead; paused, it shows what build mode will aim at.
3. **DLSS's bias hint,** `set dlsshint` 1 (the default): 1 under the translucent particles, which have no motion
   vectors of their own, by drawing them again into the hint; at 0, nothing. The hint must be an `R8_UNORM` render
   target the colour's size, as step 1 found it, or the forward list draws without it. It's the game's own mechanism
   for what its motion vectors can't follow. Spike 3's frame log shows the DLSS list starting with five barriers and no
   draw, dispatch or clear, its two dispatches after a change of targets, most likely DLSS's own. Run B's `dump hint`,
   copied as DLSS gets it, shows whether these marks reach DLSS, and whether the game clears the hint each frame. If
   the game writes it after the forward list, the hint's draw moves into the DLSS list at the evaluate, where the dumps'
   copies go.

### Costs measured

A timestamp between the blocks and our entity draws, and around the forward list, give the GPU time of each; the
status shows them. Main-thread costs (the scene's copy and `Update`'s) go into the existing update timing.

## Step C: the Minecraft side (where a step shows a need)

CyberCraft Tweaks mixes into SkyCraft's `AvatarExporter`:
- **draw order** (1.4.0, before step A, since step 0 showed the villager's layers out of order): `order(n)` returns a
  collector that marks what's submitted through it with `n`; batches are kept per order too, and sent lowest order
  first, so a layer drawn later in Minecraft is drawn later here and wins at equal depth. The collector is a `Proxy`
  that calls the exporter itself with `n` as the order while each call runs; SkyCraft makes its batch inside each call;
  the batches are a `TreeMap`, keyed by the order above SkyCraft's own key (`order << 40`);
- **emissive render types** (only if run A shows the need): a batch of an emissive render type (eyes, the warden's
  glowing layers) carries a free vertex-flag bit, which the shader takes as glow;
- **per-entity motion** isn't designed here: if `dlsshint 2` isn't enough, it gets a design of its own before any
  code.

Each is checked with `mclink` (the batch order and flags) before the game. The batch key widens so the new bit can't
collide with SkyCraft's UV mode.

## The steps

### Step 0: `mclink`, without the game

With Cyberpunk closed, the user starts Minecraft from the Modrinth App, its particles left at All. `mclink`'s report
learns the new sink calls, and a new script, `entities.txt`, as `blast.txt` does, puts the stand-in V on spike 1's
dumped ground (`ground city`, so things stand on it) in creative, then makes each test thing there, tagged `sp7`
(`PersistenceRequired`, so nothing despawns), the stand-in V looking at each as it's made, since Minecraft extracts
only the particles in its view:
- a pig and a villager with a profession and a level;
- a spider and a blaze, AI off;
- a falling sand, dropped from 12 blocks up so a scene catches it in the air;
- a chest and a banner with patterns, each with a marker entity tagged `sp7b`;
- a dropped item, a dropped grass block and an arrow;
- a firework and a splash potion, for translucent particles;
- last, a lit TNT 18 blocks ahead, at least 11 blocks from the rest: a TNT's power is 4, and its blast hurts up to
  twice that, so it spares them. Spike 1's raised ground ends 6 blocks ahead, so an obsidian block with a marker holds
  the TNT at the stand-in V's level. It flashes white every quarter second, so the scene is printed thrice a quarter
  second apart, then just after its blast.

Nothing it makes leaves a drop that a clear by tag would miss, since Night City's ground is only collision to
Minecraft ("Changes before executing Part 1"): the mobs' loot tables are empty (`DeathLootTable`), the sand drops no
item where it can't land as a block (`DropItem`), and TNT can't break obsidian.

It records:
- **the scenes:** batches (texture, flags, count, first), vertices, those with block light 15, translucent batches, MB a
  frame, and the explosion's peak;
- **the textures:** id, size and MB, each saved as DDS so each id can be named;
- **the villager's batches,** in the order they came, which shows whether its layers come out wrong;
- **the world entities** by kind, and the outline.

Then `/execute at @e[tag=sp7b] run setblock ~ ~ ~ air`, `/kill @e[tag=sp7]` and `/kill @e[tag=sp7b]` clear it, sparing
the NPCs' stand-ins, and a last scene 7 s later, once the mobs' death puffs and the blaze's smoke are gone, shows
nothing left. This settles, from Minecraft's own data, the sizes and flags step A builds on.

### Step C's draw order, before step A

Step 0 showed the villager's layers out of order, so step C's draw order comes first: CyberCraft Tweaks 1.4.0,
installed with the user's leave and Minecraft stopped, its 1.3.0 kept disabled to put back; then step 0's script again
with `mclink`, whose scenes should be step 0's with their batches in Minecraft's draw order. Run A's check 7 then sees
it in the game.

### Step A's code

The parsers, the table reader, `Entities`, `WorldShapes`, `EntityTextures` and `Snapshots`, unit-tested; the renderer's
draws and shaders; the settings, the `entities` request, the status line, the overlay's Spawn row and Clear. Built and
deployed with the game closed.

### Run A: in the game

In creative (the Fun row), with combat off (the default), so Minecraft's blasts stay in Minecraft and Minecraft's death
can't kill V. Before the user looks, the agent names what isn't drawn yet: translucent particles, the outline, signs'
words, name tags, contact shadows, and a slime's outer layer, drawn solid since SkyCraft captures every model as a
cut-out. A TNT's blast breaks the placed blocks and drops them, so Clear comes before any TNT.
1. **Things in place:** a pig, a villager, a spider, a blaze and a slime (the Spawn row, 5 m ahead), a lit TNT
   (`tnt-ahead`) and a falling sand stand with their feet on the ground, or fall to it, neither floating nor sunk,
   animate, are lit like the blocks by day, and stay put while V walks and turns. The `entities` line shows the nearest
   mob's distance from V (from the world table's shadow records), to compare with the 5 m.
2. **Glow:** at night (Skip Time, as spike 3 did), the TNT's flash, the blaze and the explosion's puffs glow. A few
   `set glow` values are tried, and `set glowlight 14` beside a torch's flame; if nothing glows at any, step B's
   `glowpass` is the way.
3. **The explosion:** its puffs, smoke and debris come and go as in Minecraft.
4. **Block entities:** a chest shows, and the banner's patterns over its cloth; the bed and the sign, block models in
   26.3, show with the blocks.
5. **World entities:** a dropped item and a dropped block (the Spawn row: build mode doesn't forward Q), arrows (the Fun
   row's bow) and a thrown trident (its trident), stuck where they hit Night City, and cracks while mining (briefly in
   survival, since creative breaks blocks at once).
6. **Contact darkening:** whether the game's ambient occlusion darkens the ground under a pig. If not, `kWeShadow` waits
   for a later step.
7. **Layers:** the villager's clothes and the spider's eyes over their bodies, with step C's draw order.
8. **Smearing:** the slime hopping and the blaze's rods turning, while V stands, and while V turns: mobs don't walk on
   the city's ground (step 0). Visible trails decide `dlsshint 2` for step B.
9. **Cost:** the status's GPU times and the frame time, with the herd, then, last, during `tnt-cube`'s blast (125 TNT,
   whose chain may prime more than the scene's 48 entities at once, so some may not be drawn).
10. **No crash or freeze.** DRED stays on.

Then the Clear button clears it all.

### Step B's code, and run B

The forward list, its shaders and settings, with run A's fixes (Shift+F6's pause; the overlay's block entities at
their markers, and its still rows), built and deployed with the game closed. First the overlay's Spawn rows are
typed into Minecraft with `mclink` (`tools/mclink/spawn.txt`), without the game. Then in the game:
- fireworks, a splash potion and campfire smoke, by day: blended, hidden behind nearer things, and as bright as
  Minecraft's, side by side;
- DLSS's bias hint: `dump hint` with the smoke in view shows its marks, and again once the smoke has gone shows no
  trace of it, so the game clears the hint each frame; `dlsshint` 0 and 1 compared, still and turning;
- the outline in build mode: steady and unbroken while V moves, about as thick as Minecraft's; if DLSS breaks it up,
  it moves to Present, drawn over the finished image against a copy of the depth, as spike 3's step 2 did;
- Shift+F6 pausing build mode, and build mode going on as the overlay closes; the overlay's block entities each at its
  marker while V walks, and Clear leaving nothing;
- at night: the particles beside Minecraft's (if too bright, a night factor, from the game's hour, gets a design of
  its own), and the spider's eyes, which decide step C's emissive mark;
- the forward list's cost, and no crash or freeze.

### Step C's emissive mark, if run B needs it

CyberCraft Tweaks again, installed with the user's leave and Minecraft stopped; `mclink` shows the batches' flags; then
run A's check 2 again in the game.

### The results

`docs/spikes/2026-10-06-minecraft-entities-results.md`, with the answer at the top.

## Requests and settings

- **`set entities 0|1`** (1 by default): draws or hides all of Minecraft's entities; the first thing to try if anything
  looks wrong.
- **`set glow <0 to 1>`** (0.9): what a glowing triangle writes into the material target's A.
- **`set glowlight <1 to 15>`** (15): the block light from which a triangle glows.
- **`set dlsshint 0|1`** (1; step B): DLSS's bias hint under the translucent particles, or nowhere.
- **`set outline 0|1`** (1; step B): the outline in build mode.
- **`exposure`** stays `predlss`'s setting, shared with step B's unlit colours.
- **`entities`** (a request): the newest scene's batches (texture, size, flags, count), the textures (id, size) and the
  world entities by kind, into the log.
- **`dump hint`** (step B; a request): DLSS's bias hint as DLSS gets it, saved with the other tagged buffers at the
  evaluate; `dump all` takes it too.
- **`overlay open|closed`** (Part 3; a request): CET's overlay opened or closed, as the overlay says from CET's
  events, which pauses build mode while it's open.
- **The overlay's Spawn rows** (Fun): each button types its commands as `mc` requests, each test thing ahead of
  Minecraft's player, level (`/execute rotated ~ 0 positioned ^ ^ ^N`, as the named sets do), tagged as in step 0. Each
  kind has its own distance, so none lands on another:
  - 5 m ahead: a pig, a villager (with a profession and a level), a spider and a blaze (AI off), and a slime of the
    smallest size, which hops with no path and doesn't split when it dies, each with an empty loot table, as in step 0;
  - a herd of 10 pigs, 11 and 13 m ahead (10 requests, about 10 s);
  - a falling sand from 12 m up, 3 m ahead, which vanishes wherever it lands (`CancelDrop`), even on a placed block, and
    drops no item if it falls out of the world (`DropItem`);
  - block entities 8 m ahead, 3 m apart: a chest, a bed (its foot and its head, two blocks, each with a marker), a sign
    and a banner with patterns, and a torch, for its flame (the bed, the sign and the torch are block models in 26.3,
    drawn with the blocks). Each marker goes first, tagged with its kind, and its block at it, so V moving between
    the requests can't part them (Part 3);
  - items 3 m ahead: a dropped item and a dropped grass block (its top tinted);
  - for step B, the Particles row: a firework 6 m ahead, which bursts overhead; a splash potion that breaks 3 m ahead
    and 3 m to the left; and a lit campfire 3 m ahead and 3 m to the right, placed at its marker as the block entities
    are.

  **Clear** clears what they made, by tag, leaving no drops: the placed blocks set to air `strict`, without block
  updates, so neither half of the bed breaks the other with a drop. A lit TNT and a big blast come from the named sets
  already there (`tnt-ahead`, whose TNT stands on obsidian with a marker, and `tnt-cube`).

## Measurements and the status

A new status line, `entities`, shown by the overlay:
- scenes a second and MB a second;
- the newest scene's batches and vertices, how many glow and how many are translucent;
- batches skipped (texture not yet on the GPU, or capped) and rejected messages;
- textures: count and MB;
- world entities by kind, and whether there's an outline;
- the nearest mob's distance from V, from the world table's shadow records (`kWeShadow`: players' and mobs' feet);
- the GPU time of our entity draws, and of the forward list (step B);
- frames the G-buffer list drew entities in, how many the forward list drew (step B) and how many of those with the
  bias hint, its skips with the last one's reason, and the `outline` and `dlsshint` settings.

## Safety

- **No faults:** every count, size and offset from the mapping is checked before use; the scene's payload is copied
  before the call returns; the table is copied before it's read. The game's crash handler sees a fault before any
  guard, so nothing relies on one.
- **No reads past a buffer on the GPU:** root descriptors carry no bounds, so the shader clamps every index it works out
  from vertex data (a UV to a texel within its texture, a direction to its table); a bad UV reads an edge texel. Draws
  read only the vertices the CPU checked into the snapshot.
- **GPU memory is bounded:** entity textures at 64 MB; snapshots at 8 buffers of at most 8 MB.
- **A failure never stops the blocks:** a texture, snapshot or pipeline that can't be made skips the entities (logged
  once), and drawing goes on. A removed device is handled as now.
- **Builds and deploys** only with the game closed (`tasklist` shows no `Cyberpunk2077.exe`), `CP2077_DIR` set only for
  the deploy and cleared after.
- **Minecraft's world:** every test thing is tagged and cleared after each step; placed blocks are removed by their
  markers, without block updates; and nothing a test makes leaves an untagged drop: the placed blocks are cleared
  before any TNT, whose blast would break and drop them. Test runs are in creative.

## Error handling

- **A malformed message** is rejected and counted (the status's rejected count, as now), and logged at most once a
  second with the rest.
- **A torn table** keeps the last good copy.
- **A batch that can't be drawn** (texture missing or capped, range bad) is skipped and counted.
- **Shader compile failures** (`reload`) leave the last shaders; the entity pipelines are made at their first use and,
  if they fail, entities are skipped with drawing kept on, logged once.

## Testing

### Unit tests (ctest, no game)

- **`ParseTexture` and `ParseScene`:** good payloads; a short header, short pixels, a zero or oversized side, a batch
  past the vertices, a count not a multiple of 3, a non-finite origin, too many batches or vertices, each rejected with
  its reason.
- **`ReadWorldEntities` and `CheckWorldTable`:** a good table; a copy mid-write refused, and the link keeping the last;
  a bad kind, count or NaN left out.
- **`Entities`:** the frame's draws from a scene and a table, in order and by kind; the origin with no scene; textures
  and the scene dropped at a clear but kept at Unlink and the Clear request; Minecraft gone empties the scene and table.
- **`WorldShapes`:** each kind's triangles against hand-worked corners (an item at yaw 90, a cube's top tint, an arrow
  flying north and up, a crack's box, the outline's 24 vertices).
- **`EntityTextures`:** appending, growth, the cap, and a clear.
- **`Snapshots`:** a buffer never reused while a recorded list's fence is ahead; a frame's snapshot held past its
  G-buffer list's fence until its forward list's, or given back when no forward list comes.
- **The forward list's pure parts:** the bias hint's checks, the outline's width, and whether a snapshot has anything
  for the forward list.
- **Build mode** (Part 3): Shift+F6 and the overlay's word pause it, and it goes on when the overlay closes.
- **The shader's constants:** the grown b1 checked against the compiled shader's reflection, as `TestShaders` does now.
- **`Requests`:** the `entities`, `overlay` and `dump hint` requests, and the new settings parsed. Their ranges
  (`entities`, `glow`, `glowlight`, `dlsshint`, `outline`) are the renderer's, as every drawing setting's are, which
  no unit test reaches: run A sends values out of range.

Each new test is broken once to prove it can fail.

### `mclink`

Step 0's script against the real Minecraft, and the same script again with step C's draw order.

### In the game

Runs A and B, as "The steps" has them.

## Risks

1. **The material target's A isn't emission:** then nothing glows in step A. Run A found it is, so step B has no glow
   fallback.
2. **Two lists of ours in one frame** have never run together: the forward list depends on the G-buffer list's frame,
   snapshot and tagged depth. Run B checks they pair up (the status counts frames drawn by one and not the other).
3. **Smearing:** motion vectors from camera movement alone may leave trails behind moving mobs and fast particles. Run A
   saw none behind the models; the bias hint goes under the translucent particles, and step C's motion is the next
   answer.
4. **Layer order:** villagers' clothes and spider eyes may show wrong (step C).
5. **The scene is a frame old** (as the HUD's images), and a Minecraft hitch freezes it for a frame or two. Something
   moving at a sprinting player's 5.6 m/s is drawn about 8 cm behind at 68 frames a second.
6. **The 48-entity cap** isn't by distance: in a big TNT chain, some near TNT may be missing. SkyCraft's choice.
7. **Texture memory:** whole atlases come as textures (chests', banners', signs', particles'); the 64 MB cap may need
   raising, which run A's status shows.
8. **Per-frame copies:** about 1 MB a frame during a big explosion is copied twice on the main thread (payload to
   `Entities`, frame to the snapshot); the update timing shows it.
9. **Blended particles at night:** `exposure` was chosen by day; night may look too bright (step B).
10. **The outline under DLSS** may break up; Present is the fallback.
11. **Contact darkening** may not come from the game's ambient occlusion; then `kWeShadow` waits.
12. **Mobs attacking Minecraft's player** could hurt V through spike 6's combat: runs keep combat off (the default) and
    Minecraft in creative, and the spider and blaze have AI off.
13. **Flames don't glow** at `glowlight` 15: Minecraft 26.3 gives them the world's light. 14 takes them, but also
    anything standing beside a block of light 15; run A tries it.
14. **Things beyond the city's area fall:** SkyCraft holds the city's voxels only where V has been (5 regions of 8
    blocks around it), so a mob wandering farther, or something summoned there, drops into the void.

## Found while writing

- **Things stand on Night City's ground only on the city's ground.** SkyCraft adds the city's voxels to every entity's
  collisions, but spike 4's floor is triangles only, for the player: step 0's script uses `ground city` with spike 1's
  dump, as `blast.txt` does.
- **Tags for cleanup:** every test thing is tagged, and placed blocks carry a marker, so one command clears them and the
  NPCs' stand-ins are spared.
- **The TNT goes last and away** from the other test things in step 0, so its blast doesn't destroy them before
  they're recorded.
- **The forward list has its own ring:** the G-buffer list's 8 slots are sized for one list a frame.
- **World entities without a scene** need an origin: the whole block under Minecraft's eye.
- **BlockRender reads bits 4 to 7 as the direction;** SkyCraft's are 4 to 6, so this spike reads 4 to 6 and leaves bit
  7 free.
- **Minecraft extracts only the particles in its view,** so step 0's stand-in V looks at each thing as it's made. In
  the game Minecraft's FOV (90°) is wider than the game's, so nothing on screen is lost.
- **A TNT hurts up to 8 blocks away** (twice its power of 4), so step 0's goes at least 11 blocks from the rest.
- **Build mode doesn't forward Q,** Minecraft's drop key (only WASD, Space, Left Shift, Left Ctrl, 1 to 9 and the mouse
  buttons): run A's dropped items come from the Spawn row.
- **Combat off keeps Minecraft's blasts and death out of the game** (spike 6's `set combat`), so runs need no more
  than creative.
- **Flames take the world's light in 26.3,** not full light: `glowlight` lets run A try 14.
- **Snapshots are capped at 8 MB each,** so 8 of them can't hold more than 64 MB of upload memory.

## Changes from the deep review

The user asked for a deep self-review of the spec before planning. It traced each claim to its source and walked
through step 0 and run A as the user would play them. One design fault turned up, and a set of corrections.

### What changed

1. **A frame's snapshot is held until its forward list is done** ("The snapshot ring"). Holding it only to the
   G-buffer list's own fence would have let `Update` rewrite it before step B's list read it: two frames' vertices under
   one frame's draws. It's now held to the fence after the frame's forward list, or after the next G-buffer list when no
   forward list comes, and a unit test covers it.
2. **SkyCraft sends whenever it's connected to the mapping,** not "whenever linked": `Link::Update` writes the game's
   heartbeat every frame once the mapping is open, Link on or off ("Purpose", "The game and BlockRender").
3. **Bright and translucent mobs:** blazes, magma cubes, allays and vexes return block light 15, so the glow rule takes
   all four. Allays and vexes are drawn translucent, as a slime's outer layer is. The non-goal's "a ghast's glow" was
   wrong: nothing of the ghast's is translucent or bright.
4. **Two-sided entities follow Minecraft itself:** `EntityModel`'s default render type, `entityCutout`, culls nothing in
   26.3, so drawing the scene two-sided matches Minecraft rather than only making up for the capture.
5. **The mapping empties the render ring whenever it opens,** created or reused, so the reason textures and scenes wait
   for the atlas is the frame Minecraft may write before it sees a new game process ("Coming and going").
6. **Where things stand:** the city's ground falls back to the floor if PhysX's audit fails, and the city's area reaches
   5 regions of 8 blocks around V. Things farther than V has been fall into the void (risk 14).
7. **Arrows and tridents stick in the game's exact surfaces** (`AbstractArrowMixin`), so run A's check 5 looks for
   them stuck in Night City, and throws a trident too. An arrow that hits a stand-in is discarded, as vanilla does.
8. **The camera follows V's look outside build mode only;** in build mode Minecraft's look leads.
9. **What was read, not seen, now says so:** the game's ambient occlusion under spike 3's cube ("most likely", in run
   8's words), SkyCraft's own layered models in Skyrim, and spike 6's TNT, which comes from the handoff.
10. **The crack decal writes RGB only,** leaving the base colour's A.
11. **Entities are drawn only in `gbuffer` mode,** stated.
12. **The texture cap counts the pixels kept for upload,** which pile up while drawing is off.
13. **The Spawn row and run A's checks:** a herd of 10 pigs for the cost check; a marker where the falling sand lands,
    since it lands as a block; a torch, for `glowlight 14`; a dropped grass block, for its tint. Check 1 now names
    what the user can see (feet on the ground, 5 m ahead) and the `entities` line shows the nearest mob's distance from
    V. `tnt-cube` goes last, since it destroys what's near it.
14. **Step 0** keeps Minecraft's particles at All, which they are now (`particles:0`).

### What was checked

- **BlockRender at `a1db95d`:** `DrainByteRing`'s visitor contract (`Rings.h`); `Mapping::Open`'s resets; `UpdateLink`
  running `Link::Update` every frame; the ground's default and its PhysX fallback; `GroundAreas`' reach; `set
  translucent`; `Retired`'s rule; `CreateCommittedResource` in `Gpu.cpp`; `ListBefore`, `OnEvaluate`, `RecordGbuffer`
  and `RecordPredlss`; the root signature, the pipelines and `kGbufferFormats`; build mode's forwarded keys
  (`BuildMode.cpp`); `LinkRequest`; the named sets (`Commands.cpp`); `mclink`'s scripts and actions.
- **The installed game:** BlockRender 0.19.1's log line (`dred on`), `settings.txt`'s `dred = 1`, and spike 1's dump
  (`collision.obj`) still in place.
- **SkyCraft at `bfcaf17`:**
  - `AvatarExporter`: the capture, flags, batches, `order`, textures and overlays;
  - `WorldExporter`: the frame's order, the world table, cracks and the outline;
  - `SkyLink`: the generation, the sequence lock and ring writes;
  - `SkyAtlas`: arrows' and cracks' rectangles;
  - `BlockCollisionsMixin` and `SkyCollision.readRegion`: voxels for every entity;
  - `AbstractArrowMixin`: exact surfaces and arrows stuck in stand-ins;
  - `SkyClient`: the look;
  - `WorldRender.cpp`: states, shapes, motion and contact shadows.
- **Minecraft 26.3's jar:**
  - `DepthStencilState.DEFAULT`;
  - `RenderPipelines`: every cut-out value, the entity pipelines' culling, the crumbling blend;
  - every particle class's layer and light;
  - `EyesLayer`, `RenderLayer.renderColoredCutoutModel`, and `VillagerProfessionLayer`'s orders and textures (`type`,
    `profession`, `profession_level`);
  - `BannerRenderer` and `Sheets` (one banner atlas);
  - `EntityModel`'s default render type, and `SlimeOuterLayer`, `AllayModel` and `VexModel`;
  - the bright mobs' `getBlockLightLevel`;
  - the outline's colour and `Window.getAppropriateLineWidth`;
  - `Options`' FOV encoding;
  - the core shaders' discards, the eye textures and the crack stages.
- **The CyberCraft instance's `options.txt`:** FOV 0.5 (90°), particles 0 (All), render distance 8.
- **Earlier results:** spike 3's tags, passes, material values and run 8; spike 4's settings; spike 6's combat switch.
- **Step 0, walked through:**
  - `mclink` creates the mapping, and Minecraft opens it and sends everything again;
  - `ground city` with the dump; each thing summoned by `type`, the stand-in V turned toward it;
  - the TNT last; then the clear by tag.

  Found: the falling sand lands as a block, and particles need Minecraft's view.
- **Run A, walked through as played:**
  - Link switches drawing to `gbuffer`; creative from the Fun row;
  - the Spawn row puts things 5 m ahead, level, on the city's voxels within the area;
  - night by Skip Time;
  - Q isn't forwarded, the bow and the trident are (the right button), and mining (the left button);
  - `tnt-cube` last; then Clear.

### Not settled by reading

- Whether the material target's A is emission, and how strong `glow` needs to be (run A).
- Whether DLSS smears moving mobs and particles, and how much the bias hint helps (runs A and B).
- Whether the game's ambient occlusion darkens the ground under mobs (run A).
- How large the textures are in all (chests', signs', banners' and particles' atlases), and the scenes' sizes (step
  0).
- The villager's batch order with real texture ids (step 0).
- 26.3's data for villagers, fireworks and potions in `/summon` (the plan checks each against the jar).
- How the game takes two lists of ours in one frame (run B).

## Changes while planning Part 1

The plan is written part by part, as spike 6's was. Part 1 is step 0 alone: what the link takes, and `mclink`'s
report. Parts 2 and 3 (steps A and B, and C if run A needs it) are planned from step 0's results, which settle the
sizes, flags and textures they build on. Planning Part 1 settled these:
1. **The parsers live in `link/Messages`,** beside the section and atlas parsers, as `ParseTexture`, `ParseScene` and
   `SceneVertices`, not in a new `link/SceneMessages`.
2. **The table's read lives in `link/Rings`,** as `ReadWorldEntities` beside `ReadMcState`, with its 64 tries rather
   than 3; its checks live in `link/WorldTable` (`CheckWorldTable`, `WorldKindName`).
3. **The link empties the table,** not `Entities`: it reads the table each update while Minecraft is in its world, keeps
   the last good one when it finds it mid-write, and hands the sink the table every update, empty otherwise. The
   scene's emptying stays with `Entities` (Part 2), since the link has no scene to keep.
4. **Textures and scenes count toward the ring's budget,** as the messages the link takes do; the budget test now steps
   over atlas patches instead of scenes.
5. **`mclink`** reports each texture, sums up the scenes every five seconds with the largest among them, and gains
   `scene` (the newest scene batch by batch, with each batch's glowing vertices, and the table) and `save-textures`.
6. **Step 0's script is `tools/mclink/entities.txt`,** at x 880 on spike 1's dump, whose ground reaches 36 blocks east:
   - the falling sand drops from 12 blocks up, so a scene catches it in the air;
   - the TNT goes 18 blocks ahead on a stone block, with a marker, in case the street dips there;
   - its scenes come thrice a quarter second apart, since it flashes white every quarter second, then just after its
     blast.
7. **Step 0's commands follow 26.3's data,** checked in its classes: the entity ids (`splash_potion`, `firework_rocket`,
   `falling_block`, `marker`), `VillagerData` (`type`, `profession`, `level`), the banner's `patterns` (`pattern`,
   `color`), `Item` and `PickupDelay`, `FireworksItem`, `LifeTime`, the `fireworks` component's `explosions` (`shape`,
   `colors`), `potion_contents` (`potion`), `BlockState`, `NoAI` and `PersistenceRequired`.

## Changes before executing Part 1

Before Part 1 ran, the user asked for a read of the plan, this spec and their context. Step 0's script, played through
against SkyCraft's source, Minecraft 26.3's classes and spike 1's dump, would have left untagged items in the user's
world past its clear. Night City's ground is only collision to Minecraft, in SkyCraft's void world `SkyCraft`, so:
1. **Spike 1's raised ground ends 6 blocks ahead** of the stand-in V, 6 to 9 m above the ground beyond, which reaches
   about 30 blocks east. "Changes while planning Part 1" had it reaching 36 blocks: that was the dump's extent, not its
   ground at the stand-in V's level. The TNT, 18 ahead, stands only on the block under it.
2. **That block is obsidian,** not stone: a TNT blast drops every block it breaks (`tnt_explosion_drop_decay` is false
   by default), and TNT can't break obsidian.
3. **A falling sand can't land as a block on the city's ground:** Minecraft has no block under it
   (`FallingBlock.isFree`), so it drops as a sand item. `DropItem:0b` stops that, and the sand needs no marker.
4. **`/kill` runs a mob's loot table:** the pig would drop porkchops, the spider string. The mobs have
   `DeathLootTable:"minecraft:empty"`, which `Mob.getLootTable` honours.
5. **Timing:** the potion breaks on the ground about half a second after it's summoned, so its scene comes after 1 s.
   The last scene comes 7 s after the clear, once the killed mobs' death puffs (up to 4 s) and the blaze's smoke (up to
   5 s; it smokes even with its AI off) are gone, so its world line saying `nothing` shows that nothing was left.
6. **A dropped full block is a `block` record** in the world table (`WorldExporter.addItem`), so the items' scene shows
   `1 arrow, 1 block, 1 item, 4 shadows`.

The Spawn row (Part 2) follows step 0: its mobs have empty loot tables and its falling sand drops no item, so Clear
leaves nothing behind.

## Changes while planning Part 2

Part 2 was planned from step 0's results (`docs/spikes/2026-10-06-minecraft-entities-results.md`): step A, and step
C's draw order before it. Its code was written and tested in a scratch clone first, and reviewed against BlockRender's
code, SkyCraft 0.1.2's bytecode and Minecraft 26.3's classes before the plan was written from it. These changed the
design; the sections above include them:
1. **Step C's draw order comes before step A,** in Part 2: step 0 showed a villager's badge sent before its body, by the
   textures' ids. It's checked with `mclink` first, then by run A's check 7. Its emissive mark still waits for run A.
   The mixin's collector is a `Proxy` that calls the exporter itself with the order set while each call runs, which
   works because SkyCraft makes its batch inside each submit call; the batches are a `TreeMap` keyed by the order above
   SkyCraft's own key ("Step C").
2. **Run A's smearing check uses a slime,** hopping, and the blaze's rods turning: mobs don't walk on the city's ground
   (step 0). The Spawn row gains a slime of the smallest size, which hops with no path (26.3's `AbstractCubeMob`) and
   doesn't split when it dies. It's drawn solid, as the non-goals have it.
3. **The Spawn rows' layout and their clear,** played through against Minecraft 26.3's classes and SkyCraft's:
   - every entity but Minecraft's player stands on SkyCraft's blocky copy of the city's ground, so mobs and items stay
     up, as in step 0;
   - each kind has its own distance (3, 5, 8, 11 and 13 m), and the block entities stand 3 m apart;
   - a bed is two blocks, its foot and its head, each with a marker; Clear sets the placed blocks to air `strict`,
     without block updates, since a bed's half breaks with a drop when the other goes;
   - the falling sand has `CancelDrop`, so it vanishes even where it lands on a placed block;
   - Clear comes before any TNT, whose blast would break the placed blocks and drop them.
4. **`tnt-ahead`'s TNT stands on obsidian, with a marker,** not on stone, which its blast would break and drop; Clear
   removes the obsidian.
5. **Entity textures are placed as they come,** so the cap counts them at once, and uploaded by `Update`. The buffer's
   growth copies only the textures already uploaded, since copies in one list aren't ordered, and keeps the old buffer
   until that upload is done, since it reads it ("Entity textures on the GPU").
6. **Until step B, a snapshot is held to the fence after the G-buffer list that took it,** the only list that reads
   it; an empty snapshot isn't taken; a snapshot buffer that can't be made skips the entities, logged once ("The
   snapshot ring").
7. **`WorldWords` moves from `mclink` into `link/WorldTable`,** for the status's `entities` line and the `entities`
   request.
8. **The nearest mob is measured from Minecraft's player's feet,** which follow V outside build mode, and which V
   follows in it.
9. **The settings' ranges are the renderer's** (`Renderer::Set`), as every drawing setting's are; run A checks them
   with values out of range ("Testing").
10. **Link turns drawing to `gbuffer` only from off;** if it was `predlss` or `present`, run A switches it from the
    overlay, since entities are drawn only in `gbuffer` mode.
11. **The outline comes with step B,** which draws it: Part 2's `WorldShapes` and `Entities` have no outline kind yet,
    and the outline's test (its 24 vertices) comes with it.
12. **BlockRender 0.20.0,** and CyberCraft Tweaks 1.4.0.

## Changes while executing Part 2

Step C's `mclink` run (Task 7) and a check of the overlay's Spawn rows typed into Minecraft after it (the results'
"Step C's draw order, with `mclink`") found two things the sections above now include:
1. **A falling block's `BlockState` is a block id in 26.3** (`"minecraft:sand"`), or a map keyed `id`, not
   `{Name:...}`, which 26.3 refused with a warning, the sand coming only from the falling block's default. Step 0's script
   and the Spawn row now give the id. "Changes while planning Part 1" had `BlockState` among the data checked against
   26.3's classes: its form wasn't.
2. **Beds, signs and torches are block models in 26.3,** in the section meshes SkyCraft sends, so BlockRender draws
   them with the blocks already; of the Spawn row's block entities, step A draws the chest and the banner ("Purpose",
   run A's check 4).

## Changes while planning Part 3

Part 3 was planned from run A's results (`docs/spikes/2026-10-06-minecraft-entities-results.md`) and the user's choices
on its problems. Its code was written and tested in a scratch clone first, then reviewed against BlockRender's code,
SkyCraft's and spike 3's frame log, before the plan was written from it. These changed the design; the sections above
include them:
1. **Step B is trimmed to what run A left:** the material target's A is emission, so there's no glow fallback (`set
   glowpass` is gone); nothing smeared, so the bias hint goes under the translucent particles alone (`set dlsshint` is 0
   or 1, never 2); and the game's ambient occlusion darkens the ground under mobs, so there are no contact shadows. The
   forward list's targets lose the motion vectors, which only the glow fallback wrote.
2. **Shift+F6 pauses build mode while CET's overlay is open,** the user's choice over leaving it, as spike 4 had it:
   while the overlay is open every input is its and the game's, the mouse's movement adds no turn to V's teleports,
   Minecraft lets go of everything as build mode pauses, and build mode goes on when the overlay closes. The overlay
   tells BlockRender when it opens and closes (`overlay open|closed`, from CET's events), which keeps the pause right if
   it was toggled another way.
3. **The overlay places each block entity at its own marker,** the marker first and tagged with its kind, so V moving
   between the requests can't part a block from its marker, as in run A; and its top lines are always drawn, so its
   rows don't move under a click.
4. **Unlink keeps Minecraft's entities drawn,** as it keeps its blocks: the user's choice, so nothing changes.
5. **The forward list pairs with its frame** through what the G-buffer list leaves it: the frame's DLSS list, tags,
   camera, constants and extent, and the snapshot, taken a second time for it (`Snapshots::Take`). A claim whose
   forward list never came is let go at the next G-buffer list or a change of mode, and a skipped forward list lets it
   go at once. The forward list has its own ring, constants and timing queries, and its own thread-local record, so a
   submission holding both lists signals and submits each.
6. **The outline shows while build mode is on, paused or not:** paused, it shows what build mode will aim at.
   `Entities::Frame` takes build mode's state, and the renderer's `set outline` hides it.
7. **The outline darkens the scene's linear colour to `Linear(0.6)`,** about a third, which is what Minecraft's black at
   40% does to its sRGB framebuffer's colours; 40% in linear would darken it far less on screen. Its width is
   Minecraft's in output pixels, scaled to DLSS's render width, from the back buffer's width as the last Present saw
   it (2.5 pixels before one).
8. **The bias hint is drawn by drawing the translucent particles again into it,** only where the game tags an
   `R8_UNORM` render target the colour's size, as step 1 found it. Spike 3's frame log shows the DLSS list starting
   with barriers alone; `dump hint`, a new dump target, shows in run B whether the marks reach DLSS and whether the game
   clears the hint each frame.
9. **The Particles row** makes step B's test things: a firework, a splash potion, and a campfire placed at its marker.
   An `mclink` script of every Spawn row (`tools/mclink/spawn.txt`) checks them all against Minecraft before run B.
10. **The status's `entities` line** shows the forward list: its frames, those with the hint, its GPU time and skips,
    and the two settings; the overlay's `Entities` line, its frames.
11. **BlockRender 0.21.0.**
