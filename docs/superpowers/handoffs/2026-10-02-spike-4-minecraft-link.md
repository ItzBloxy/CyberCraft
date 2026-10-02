# Handoff: start spike 4 (the Minecraft link)

Written 2026-10-02, at the end of the session that finished spike 3 (Minecraft blocks in Night City) and merged it to
`main`. Read this first.

## Start here

- **Repo and branch:** `D:\Cyberpunk-Minecraft`, branch `spike/minecraft-link`, which holds only this handoff and
  `tools/plans/` on top of `main` (`3c0bd8c`). Spike work stays on the branch, never on `main`. The repo has no git
  remote.
- **Process:** the one spikes 1 and 3 used:
  1. `superpowers:brainstorming` with the user, to agree what spike 4 tests and how;
  2. a spec in `docs/superpowers/specs/`;
  3. `superpowers:writing-plans`, giving a plan in `docs/superpowers/plans/` with verified code in each step (see
     `tools/plans/README.md`);
  4. `superpowers:executing-plans`, inline;
  5. in-game verification with the user;
  6. a results doc in `docs/spikes/`;
  7. `superpowers:finishing-a-development-branch`.
- **No sub-agents:** the user wants everything done inline, plan execution included.
- **Read for background, in this order:**
  - this handoff;
  - `docs/spikes/2026-10-01-block-render-results.md`, spike 3's results: the verdict, then runs 8 and 9;
  - "How Part 3 went", at the end of `docs/superpowers/plans/2026-10-01-block-render.md`, with what a SkyCraft for
    Cyberpunk builds on and still has to solve;
  - spike 2's handoff, `docs/superpowers/handoffs/2026-10-01-spike-2-player-puppeting.md` on branch
    `spike/player-puppeting`, for SkyCraft's protocol.
- **A first message to start the next session with:**
  > Read docs/superpowers/handoffs/2026-10-02-spike-4-minecraft-link.md and start spike 4.

## Why the link comes next

The project recreates [chasmlol/SkyCraft](https://github.com/chasmlol/SkyCraft) (Minecraft in Skyrim: an SKSE plugin
and a Fabric mod over shared memory, MIT) for Cyberpunk 2077. In SkyCraft, **Minecraft owns the world and the player**,
and the game only shows the result: Minecraft's blocks drawn into its world (spike 3, done), and the player puppeted to
Minecraft's (spike 2, waiting).

At the end of spike 3 the user weighed placing blocks (with a hotbar and inventory), TNT, collision and shadows. This
was the call, and why:
- **Placing, the hotbar, the inventory and TNT are Minecraft's own** once its world streams into Night City. Building
  them into the plugin instead (a crosshair raycast, a hotbar, an explosion) would rebuild Minecraft, only to be
  thrown away when the link arrives.
- **Collision is Minecraft's job, in SkyCraft's model.** The hidden Minecraft player moves and collides against
  Minecraft's blocks natively, and against Night City through the game's collision exported into Minecraft (spike 1
  read it). V just follows. So 10,000 or 100,000 blocks cost the game nothing for collision: Minecraft only ever checks
  the blocks around its player. Whether NPCs, cars and bullets should hit blocks is a later design question.
- **Shadows can wait:** the user found the G-buffer lighting good as it is.
- **It's testable at once, without input passthrough:** the user can build and set off TNT in Minecraft's own window
  and watch Night City change. Puppeting and input forwarding come next (spike 2's handoff, revived), and the
  collision exporter after that.

## What spike 4 is for

**Can SkyCraft's Fabric mod stream Minecraft's live world into BlockRender over shared memory, so that blocks placed,
broken or blown up in Minecraft appear in Night City within a frame or two, while both games run at acceptable frame
rates?**

### Questions to start the brainstorm with

This is a starting list, not a decision; agree the scope with the user.
1. **SkyCraft's mod, as is or trimmed:** can its Fabric mod (Minecraft 26.3) run without its SKSE side? What does it
   expect from the game side: a handshake, `SkyState`'s in-game, menu and loading flags, an input ring? Or is a minimal
   mod of our own, sending only sections, simpler? Getting SkyCraft is a download: ask the user first.
2. **The game side:** a link module in BlockRender reading the `RenderRing`, handing each section's mesh to
   `Renderer::UploadSection`. Which thread reads it, and how often? What happens when Minecraft starts, stops or
   restarts while the game runs?
3. **The atlas:** Minecraft's own (`RenAtlas`, every block, matching its meshes' UVs) instead of spike 3's
   `atlas.bin` from the user's jar (50 blocks).
4. **The world:** Minecraft's coordinates are Night City's (1 block = 1 m, `McFromWorld`), so puppeting can follow.
   A void or superflat world at the right height suits a test; spike 3's corner is Minecraft block (-119, 6, 2007),
   and the overlay shows the block at V's feet.
5. **Minecraft visible or hidden:** visible, to play in for this spike; hidden comes with puppeting. Does SkyCraft
   turn Minecraft's own rendering down or off?
6. **Both games at once:** each one's frame rate, CPU and GPU contention, and memory, on the user's RTX 3070.
7. **Latency:** from placing a block in Minecraft to seeing it in Night City, in frames.
8. **Bursts:** TNT changes many sections at once. Spike 3's renderer uploads on the main thread in `Update`; what does a
   burst cost, and does anything hitch?
9. **Beyond blocks,** probably later: entities and particles (`RenScene`, for primed TNT and items), the player model
   (`RenAvatar`), Minecraft's own lights such as torches, translucent blocks such as water (a G-buffer can't take them;
   step 3's drawing before DLSS could), and leaves in the trees' stencil class (182, which run 9 saw change their look).

## Known facts

### SkyCraft's protocol (from its source, read in the spike 2 session)

- `protocol/skycraft_protocol.h`, version 11, shared memory `Local\SkyCraft_v1`, with seqlocks throughout.
- **Minecraft → game:**
  - `McState`: the player's feet, yaw and pitch; eye position and height; camera mode and field of view; motion flags;
    the 20 Hz physics ticks with QPC timestamps, which the game side interpolates; a walk-bob phase and a frame counter.
  - `RenderRing`: `RenSection` (16×16×16 chunk-section meshes), `RenScene` (entities and particles), `RenAvatar` (the
    player model), `RenTexture` and `RenAtlas` (textures and the block atlas), and `RenBatch {first, count, flags,
    texture}` (draw ranges).
- **Game → Minecraft:** `SkyState` (the authoritative pose for teleports, with a `teleportSeq`/`teleportAck`
  handshake, and in-game, menu and loading flags) and the `InputRing` (keyboard and mouse, forwarded because Minecraft
  moves the player).
- **The SKSE side's render files:** `skse/src/WorldRender.cpp`, `BlockLights.cpp`, `DigMesh.cpp`, `NpcBlocks.cpp` and
  `Overlay.cpp`. SkyCraft captures Minecraft's own image only for the HUD and the hand.

All of this should be checked against the current source before the spec relies on it.

### BlockRender, from spike 3 (`spikes/block-render/`, version 0.8.0)

- **The vertex is SkyCraft's** 32-byte format (`src/world/Vertex.h`), and sections are 16×16×16, so `RenSection`
  meshes should hand straight to `Renderer::UploadSection(SectionPos, std::vector<Vertex>)`: the main thread hands a
  section over, and an empty mesh removes it. Today the plugin's own `BlockWorld`, `Mesher` and `Structures` make the
  sections.
- **The modes:** `gbuffer` (step 4, the look to keep), `predlss` (step 3) and `present` (step 2). In `gbuffer`:
  `stencil 0`, and both `gbmotion 1` and `gbdepth 1`, which run 9 showed are needed (trails and ghosting without them).
- **Requests:** `blockrender.request`, one command a file, read once a second, from the plugin's folder (written
  atomically) or the overlay's (its buttons write it). Commands: `log`, `dump`, `mode`, `place cube|sampler|house|field
  <n>`, `clear`, `reload`, `set <name> <value>`. `status.txt` in the overlay's folder says what it's doing, once a
  second; the overlay shows it, with Off, Before DLSS and G-buffer buttons.
- **The cost:** 100,000 blocks take 0.06 ms of GPU time in `gbuffer` mode, and the hooks cost nothing measurable.
  Tracking every command list costs about 0.1 ms a frame.
- **The frame's order,** from run 7's frame logs: Streamline's camera arrives about 2 ms before the G-buffer is
  submitted; our list goes just before the one-list submission that ends the G-buffer.
- **Blocks sit on whole metres,** and the game's ground doesn't: they float a few centimetres or sink up to 0.6 m.
- **No shadows:** blocks aren't in the game's shadow maps, likely the 11-list submission after the G-buffer.

### Coordinates

- World: metres, X east, Y north, Z up. Minecraft: X east, Y up, Z south. `McFromWorld(x, y, z) = (x, z, -y)`, in
  `spikes/block-render/src/Coords.h`.
- Block (bx, by, bz) fills world x from bx to bx+1, y from −bz−1 to −bz, and z from by to by+1.

### The user's Minecraft

- Minecraft 26.3 is installed in `%APPDATA%\.minecraft` (spike 3 built its atlas from `versions\26.3\26.3.jar`),
  alongside many other versions, a `fabric` folder and Fabric mods for 1.21.1. The Modrinth App is installed too.
- Java: Temurin 25.0.2 is on `PATH`.
- The user is a Minecraft modder (Fabric, NeoForge, shaders): talk Minecraft internals at that level.

## After spike 4

1. **Puppeting and input (spike 2's handoff, revived):** V follows the Minecraft player every frame, the game's input
   goes to Minecraft, and Minecraft's HUD (the hotbar) is shown in the game. From then on the user plays from inside
   Cyberpunk, and walking on blocks comes from Minecraft's physics.
2. **The collision exporter:** Night City's collision, read through spike 1's PhysX access, voxelized into Minecraft,
   so the Minecraft player walks Night City's streets. An offline first step: voxelize one of spike 1's dumps into a
   Minecraft structure.
3. **Later:** blocks in the game's shadow maps; translucent blocks; entities, items and particles; Minecraft's lights.

## The game install, right now

- **BlockRender 0.8.0** is in `E:\Cyberpunk2077\red4ext\plugins\BlockRender\`, with its `shaders`, `atlas.bin`, logs and
  dumps. Drawing is off unless a request or a button switches it on.
- **Its overlay** is in `bin\x64\plugins\cyber_engine_tweaks\mods\blockrender_hud\`: the status, V's position and the
  block at V's feet, and the mode buttons.
- **Spike 1's probe 0.4.0** and its `physxprobe_hud` overlay are still installed; the probe acts only on F10 or its
  request file.
- Vortex manages the CET mods folder and may list both overlays as external files.
- **The repo's CMake caches** have `CP2077_DIR` empty, so builds don't install into the game.

## Rules to keep

- **No sub-agents:** work inline.
- **The user does every in-game action:** launching, loading, moving, keys and screenshots, and now playing Minecraft
  too. Never drive their mouse or keyboard; ask, then wait for them to say they're done.
  - Give on-screen aids when a test needs positions, headings or counts: the overlay shows the block at V's feet.
- **Settle important unknowns** before moving on, rather than leaving them as next steps. Offer that.
- **Testable steps first:** sequence the spike so every step can be checked right then.
- **Builds and the game:**
  - Never build with `CP2077_DIR` set, or deploy, while Cyberpunk runs, because the running game locks the DLL.
    `tasklist | grep -i -c "cyberpunk2077.exe"` must print `0`.
  - Set `CP2077_DIR` only to deploy, and clear it again straight after.
- **No faults:** the game's crash handler sees a fault before any `__try/__except`, and catching one freezes the game.
  Plugin code must never fault. If the game freezes, read the logs and the newest `ReportQueue` folder, and force-close
  it only when the user asks.
- **Downloads** (SkyCraft's source, Fabric tooling, anything else) need the user's explicit permission first.
- **Commits** end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`, and go on the spike branch.
- **After the spike:** use `superpowers:finishing-a-development-branch`. The user has chosen a local fast-forward
  merge to `main` all three times so far.

## Gotchas

- **Backslashes:** the Bash tool turns `\\` inside a command into `\`. Write files with the Write or Edit tools, not
  heredocs or `python -c`, when they hold backslashes.
- **Tool paths:** `cmake`, `ctest` and `dumpbin` aren't on `PATH`; spike 3's plan has their full paths. Pass MSBuild
  switches as `-m`, never `/m`. `luac` (Lua 5.4) is on `PATH`, for checking CET Lua.
- **Python:** run it with `uv run --no-project`.
- **Scratch builds under `%TEMP%`** miss header changes (MSB8029): rebuild from clean after editing a header. A stale
  object crashed spike 3's tests once.
- **Waiting on the game:** a foreground `sleep` is blocked. Run a recorder in the background instead: a bounded loop
  that reads `status.txt` twice a second into a file, with a stop file to end it. Spike 3 did this for every timing.
- **Request files:** write them atomically (a temporary name, then renamed), and wait for the plugin to take one before
  writing the next.
- **The error reporter:** `REDEngineErrorReporter.exe` starts with the game every time. Seeing it running isn't a
  crash; check `ReportQueue` instead.
- **Logs:** each plugin writes one per launch in `E:\Cyberpunk2077\red4ext\logs\`. BlockRender's frame logs go into its
  `logs\<time>\frame.json`, and `spikes/block-render/tools/frame_summary.py` summarises them.
- **Git:** for a fast-forward merge, `git switch main` then `git merge --ff-only <branch>`.
- **The scratchpad** is per session, so spike 3's helpers are gone, apart from the copies in `tools/plans/`.
