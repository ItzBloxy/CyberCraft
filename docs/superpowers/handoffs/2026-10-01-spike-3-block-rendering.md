# Handoff: start spike 3 (Minecraft blocks in Night City)

Written 2026-10-01, at the end of the session that finished spike 1 (the PhysX probe) and measured PhysX's origin rule.
Read this first.

## Start here

- **Repo and branch:** `D:\Cyberpunk-Minecraft`, branch `spike/block-rendering`, which holds only this handoff on top
  of `main` (`c81b454`). Spike work stays on the branch, never on `main`. The repo has no git remote.
- **Process:** spike 1's, which worked well:
  1. `superpowers:brainstorming` with the user, to agree what spike 3 tests and how;
  2. a spec in `docs/superpowers/specs/`;
  3. `superpowers:writing-plans`, giving a plan in `docs/superpowers/plans/` with verified code in each step;
  4. `superpowers:executing-plans`, inline;
  5. an in-game verification task with the user;
  6. a results doc in `docs/spikes/`;
  7. `superpowers:finishing-a-development-branch`.
- **No sub-agents:** the user wants everything done inline, plan execution included.
- **Read for background, in this order:**
  - this handoff;
  - `docs/spikes/2026-10-01-physx-probe-results.md`, spike 1's results, especially "Surprises and next steps";
  - `docs/superpowers/specs/2026-10-01-physx-probe-design.md`, which shows how spike 1 was framed.
- **Spike 2 waits:** spike 2 (player puppeting) was deferred until a Minecraft side exists, because it has nothing to
  test against yet. Its handoff waits on branch `spike/player-puppeting`. Spike 3 was chosen because it can be tested
  with the game alone.
- **A first message to start the next session with:**
  > Read docs/superpowers/handoffs/2026-10-01-spike-3-block-rendering.md and start spike 3.

## What spike 3 is for

The project recreates [chasmlol/SkyCraft](https://github.com/chasmlol/SkyCraft) (Minecraft in Skyrim) for
Cyberpunk 2077:
- **Minecraft runs hidden next to the game,** and simulates the player against the game's collision, exported into
  Minecraft.
- **The game shows the result:** V, puppeted to Minecraft's player (spike 2), and **Minecraft's world drawn into the
  game's world**. That means the blocks the player places and breaks, dropped items, arrows and so on.

Spike 1 settled reading the game's collision. Spike 3 de-risks the drawing side: **can Night City show Minecraft's
blocks, at the scale a player builds, with an acceptable frame rate and a believable look?** It needs no Minecraft: the
spike can place blocks itself.

### Two ways to draw blocks: the brainstorm's first decision

The project's notes called spike 3 "spawning block entities through Codeware". This session found that **SkyCraft
doesn't do that**, so both routes deserve a look.

1. **Engine entities, the original idea.** Codeware's `DynamicEntitySystem` spawns entities whose mesh is a block, or a
   group of blocks.
   - **For:**
     - The game treats them like any prop: its own lighting, shadows, ray and path tracing, upscaling and frame
       generation all work.
     - No renderer hooks.
     - Quick to try: from CET Lua it needs no build and no restart (see "Iteration speed").
   - **Against:**
     - One entity per block may get expensive at thousands of blocks.
     - Meshes must exist as resources, and no way is known to build meshes at runtime.
     - Real textures need assets: WolvenKit plus ArchiveXL.
2. **Direct rendering, SkyCraft's way.** The plugin draws Minecraft's chunk meshes with its own shaders into the
   game's frame, depth-tested against the game's depth buffer. SkyCraft's `skse/src/WorldRender.cpp` does this in
   Direct3D 11:
   - **The meshes:** one vertex buffer per 16×16×16 chunk section, built by Minecraft and sent over the `RenderRing`.
   - **The textures:** a block texture atlas.
   - **The cost:** about 25 to 45 draw calls a frame, wrapped in a full save and restore of the render state.
   - **The lighting:** Skyrim's sun, its ambient light and its 16 nearest point lights.
   - **Its own extras:** a sun shadow map and contact ambient occlusion.

   It scales, but in Cyberpunk it's much harder:
   - The renderer is Direct3D 12 behind NVIDIA Streamline (DLSS super resolution, ray reconstruction and frame
     generation), plus FSR3 and XeSS frame interpolation.
   - So the injection point, the jittered projection, motion vectors for TAA, DLSS and frame generation, HDR
     tonemapping and path-traced lighting all have to be handled.
   - That's a large, high-risk engineering effort.

**Possible middle ground,** unverified: `MeshComponent` has a 64-bit `chunkMask` that shows or hides a mesh's
sub-meshes, and a `numInstances` field.
- **The idea:** a pre-built mesh whose sub-meshes are block slots, toggled per block, could put many blocks in one
  entity without building meshes at runtime.
- **To check:** whether either field works like that at runtime.

**This session's suggestion,** for the user to decide in the brainstorm:
- Start with the entity route, because the game alone can test it, and measure where it breaks.
- Weigh direct rendering only if entities can't reach the scale a player builds.
- The user sequences work so each step can be tested against something real at the time.

### Questions to start the brainstorm with

This is a starting list, not a decision; agree the scope with the user.
1. **Placing a block:** can code place a 1 m cube on the Minecraft grid near V, and remove it again, quickly and
   without a custom `.ent` file?
   - **Candidates:** a minimal entity plus a `MeshComponent` added at runtime (Codeware's `AddComponent`), or a vanilla
     entity template.
2. **Scale:** how many blocks before the frame rate drops: 100, 1,000, 10,000?
   - Is the limit CPU (entity count) or GPU (draw calls)?
   - How fast can blocks be spawned and removed, per frame?
3. **Looks:** lighting, shadows (the `castShadows` family), ray tracing and path tracing, level of detail and pop-in,
   and how blocks sit beside Night City's own geometry.
4. **Textures:**
   - **The tools:** Minecraft's look needs Minecraft's block textures on the blocks. That means custom meshes or
     materials through ArchiveXL, and those need WolvenKit, which isn't installed.
   - **Licensing:** the textures belong to Mojang. The mod would read them from the user's own Minecraft at runtime,
     not ship them.
   - **For a spike,** plain colours are enough.
5. **Persistence and streaming:**
   - What happens on save and load (`persistState`, `persistSpawn`)?
   - What happens when V walks away (`alwaysSpawned`, `spawnInView`) or fast-travels?
6. **Collision:**
   - **Do blocks need it?** In SkyCraft's model, Minecraft owns the player's collision, so game-side collision is
     optional. NPCs and cars walking through a player's walls is a design question.
   - **The exporter:** if blocks do get collision, they'll show up in PhysX, and the collision exporter must not send
     them back to Minecraft.

## Known facts

### Codeware 1.19.0 (installed)

From its declarations in `E:\Cyberpunk2077\red4ext\plugins\Codeware\Scripts\Codeware.Global.reds`:
- **`DynamicEntitySpec`:** `recordID` (TweakDBID) or `templatePath` (ResRef), `appearanceName`, `position`
  (Vector4), `orientation` (Quaternion), `persistState`, `persistSpawn`, `alwaysSpawned`, `spawnInView`, `active`, and
  `tags` (array of CName).
- **`DynamicEntitySystem`,** reached through `GameInstance.GetDynamicEntitySystem()`:
  - **Lifecycle:** `CreateEntity(spec) -> EntityID`, `DeleteEntity`, `EnableEntity`, `DisableEntity`.
  - **State:** `IsReady`, `IsRestored`, `IsManaged`, `IsSpawned`, `IsSpawning`, `GetEntity(id) -> ref<Entity>`.
  - **Tags:** `GetTags`, `AssignTag`, `UnassignTag`, `IsTagged`, `IsPopulated(tag)`, `GetTagged`, `GetTaggedID(s)`,
    `DeleteTagged`, `EnableTagged`, `DisableTagged`. Tags allow bulk work on a whole build.
  - **Listeners:** `RegisterListener(tag, target, function)`, with events `Created`, `Deleted`, `Spawned`, `Despawned`
    and `Dead`.
- **Entities:** `Entity.AddComponent(component)`, `SetWorldTransform`, `FindComponentByType`, `GetComponents`,
  `GetTemplatePath`.
  - The `EntityBuilder` wrappers let components be added while an entity is being built.
- **No local example:** no installed mod uses `DynamicEntitySystem`. AMM's main code isn't installed (only its
  `Collabs` folder), so start from Codeware's documentation and declarations.

### Meshes and assets

- **`MeshComponent`** (`entMeshComponent`; RED4ext SDK `ent/MeshComponent.hpp`) has these fields:
  - `mesh`, `meshAppearance` and `visualScale`;
  - `chunkMask` (u64) and `numInstances`;
  - `castShadows`, `castLocalShadows`, `castRayTracedGlobalShadows` and `castRayTracedLocalShadows`;
  - `LODMode`, `forcedLodDistance` and `navigationImpact`.

  `PhysicalMeshComponent` exists too, for meshes with collision.
- **Vanilla cube meshes** in the game's resource list are possible placeholders. Which are visible, and their size and
  pivot, are unchecked:
  - `base\lighting\emissive_cube.mesh`
  - `base\lighting\emissive_boxes\emissive_cube.mesh`
  - `ep1\lighting\emissive_cube.mesh`
  - `base\open_world\mini_world_stories\badlands\se5\mws_se5_03\assets\mws_se5_03_cube.mesh`
  - `base\fx\meshes\cyberparticles\fake_occluder_cube.mesh` (probably invisible)

  The list is Codeware's `Data\KnownHashes.txt`: 129,628 known resource paths, and good for finding more.
- **WolvenKit isn't installed:** it's neither in the usual places nor on `PATH`. Custom meshes, materials and textures
  need it, and installing it is a download, so ask the user first.
- **Custom resources are proven here:** ArchiveXL 1.26.3 and TweakXL 1.11.3 are installed, and `archive\pc\mod` holds
  about a hundred archive mods and their `.xl` files, which load fine.

### Iteration speed

- **CET Lua mods reload in game** ("Reload All Mods" in the CET overlay), with no restart. From CET, Codeware's system
  should be `Game.GetDynamicEntitySystem()`; check that first. This is the fastest way to try spawning calls and
  measure scale.
- **Everything else needs a restart:** redscript, ArchiveXL resources and RED4ext plugins load only when the game
  starts.
- **The final mod is a RED4ext plugin,** spawning what Minecraft sends, so whatever the spike proves in Lua should be
  callable from C++. The probe's `PlayerLocator` shows how, with `ExecuteFunction` on a system's class.

### The renderer and performance

- **The renderer:** D3D12 behind NVIDIA Streamline (DLSS SR, RR and FG), plus FSR3 and XeSS frame interpolation,
  checked on 2026-09-30.
- **The user's frame rate:** 40 to 80 fps in the city. Note it before and after spawning, at the same spot, with the
  same view.

### Coordinates

- **Units and axes:** 1 block = 1 m. World axes are X east, Y north, Z up, and Minecraft's are X east, Y up, Z south:
  `McFromWorld(x, y, z) = (x, z, -y)`, in `spikes/physx-probe/src/Coords.h`.
- **Where a block goes:** block (bx, by, bz) occupies world x from bx to bx+1, y from −bz−1 to −bz, and z from by to
  by+1. Mind the sign flip.
- **PhysX's origin rule** doesn't affect spawning, which uses world coordinates. It matters only if blocks get collision
  and something reads them back through PhysX.

### SkyCraft, for reference

- **The protocol's `RenderRing` (Minecraft → game):**
  - `RenSection`: 16×16×16 chunk-section meshes;
  - `RenScene`: entities and particles;
  - `RenAvatar`: the player model;
  - `RenTexture` and `RenAtlas`: textures and the block texture atlas;
  - `RenBatch {first, count, flags, texture}`: draw ranges.
- **`skse/src` render-related files:** `WorldRender.cpp`, `BlockLights.cpp`, `DigMesh.cpp`, `NpcBlocks.cpp` and
  `Overlay.cpp`.

### Reusable from spike 1 (`spikes/physx-probe/`)

| Piece | What it gives spike 3 |
|---|---|
| `tools/physxprobe_hud/init.lua` | A working CET Lua mod: `onUpdate` and `onDraw` events, `ImGui` windows and V's position. A good starting point for a CET spike, with a readout of block count and frame rate |
| `src/Plugin.cpp` | RED4ext entry points; a per-frame `OnUpdate`; F10 (only while the game is in the foreground) and `dump.request` triggers; logging |
| `src/DumpRequest.*` | A tested little command language in a request file, so the agent can drive the game from the shell |
| `src/PlayerLocator.*` | V's feet and facing, plus the camera, through script calls from C++ |
| `CMakeLists.txt`, `CMakePresets.json`, `tests/Harness.h` | The build (RED4ext SDK header-only, `/W4`, the `CP2077_DIR` install step) and the tiny test harness |

## The game install, right now

- **The probe, 0.4.0,** is in `E:\Cyberpunk2077\red4ext\plugins\PhysXProbe\`. It acts only on F10 or `dump.request`,
  and its origin watch is off. The user chose to keep it.
  - If spike 3 adds a plugin or mod with a hotkey, don't use F10, or remove the probe first.
- **The CET overlay** is in `bin\x64\plugins\cyber_engine_tweaks\mods\physxprobe_hud\`. It shows position, heading and
  grid lines whenever the user is in game, and the user chose to keep it. Vortex manages that folder and may list
  the overlay as an external file. The same goes for anything new there.
- **The repo's CMake cache** has `CP2077_DIR` empty, so builds don't install into the game.
- **An old crash report** from the probe's revision 1 is still in `%LOCALAPPDATA%\REDEngine\ReportQueue\`. It's the
  user's to delete.

## Rules to keep

- **No sub-agents:** work inline.
- **The user does every in-game action:** launching, loading, moving, keys and screenshots. Never drive their mouse or
  keyboard; ask, then wait for them to say they're done.
  - Give them on-screen aids, such as the CET overlay, whenever a test needs positions, headings or counts.
- **Settle important unknowns.** The user would rather test an important unknown before moving on than leave it open.
  Offer that.
- **Testable steps first:** sequence the spike so every step can be checked in the game right then.
- **Builds and the game:**
  - Never build with `CP2077_DIR` set, or deploy a plugin, while Cyberpunk runs, because the running game locks the DLL.
    `tasklist | grep -i -c "cyberpunk2077.exe"` must print `0`.
  - Set `CP2077_DIR` only to deploy, and clear it again straight after.
- **No faults:** the game's crash handler sees a fault before any `__try/__except`, and catching one freezes the game.
  Plugin code must never fault. If the game freezes, force-close it only when the user asks, after reading the logs and
  the newest `ReportQueue` folder.
- **Commits** end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`, and go on the spike branch.
- **After the spike:** use `superpowers:finishing-a-development-branch`. The user has chosen a local fast-forward
  merge to `main` both times so far.

## Gotchas from the last two sessions

- **Backslashes:** the Bash tool turns `\\` inside a command into `\`. Write files, game paths and Lua with the Write
  tool, not with heredocs or `python -c`.
- **Tool paths:** `cmake`, `ctest` and `dumpbin` aren't on `PATH`; the spike 1 plan's command blocks have their full
  paths. Pass MSBuild switches as `-m`, never `/m`.
- **Python:** run it with `uv run --no-project`. Only command-line arguments get Windows paths; paths inside an inline
  script don't.
- **Waiting:** the Bash tool blocks a foreground `sleep`. To wait for the game, poll from PowerShell with
  `Start-Sleep -Milliseconds 500` in a bounded loop that checks a log. Spike 1 did this for every request.
- **Request files:**
  - **Write them atomically:** to a temporary name, then renamed into place. A plugin polling the file would otherwise
    see half of it.
  - **The probe drops close requests:** it deletes a request that arrives within 2 seconds of its last dump without
    acting on it.
- **The error reporter:** `REDEngineErrorReporter.exe` starts with the game every time. Seeing it running isn't a
  crash; check `ReportQueue` instead.
- **Logs:**
  - RED4ext plugins write one log per launch in `E:\Cyberpunk2077\red4ext\logs\`, and `red4ext-*.log` shows load
    errors.
  - CET's are `bin\x64\plugins\cyber_engine_tweaks\cyber_engine_tweaks.log` and `scripting.log`, plus each mod's own
    log in its folder.
  - Codeware logs to `red4ext\plugins\Codeware\Codeware-*.log`.
- **Git:**
  - `git rev-parse --short` takes one revision at a time.
  - For a fast-forward merge without checking the old tree out in between, run `git fetch . <branch>:main`, then
    `git switch main`.
- **The scratchpad:** it is per session, so earlier helper scripts are gone. The spike 1 revision 2 handoff has
  `apply_step.py`, for replaying a verified plan step by step.
