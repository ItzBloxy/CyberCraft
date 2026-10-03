# Handoff: start spike 5 (block collisions, and moving as Minecraft's player in build mode)

Written 2026-10-03, at the end of the session that finished spike 4 (the Minecraft link) and merged it to `main`. Read
this first.

## Start here

- **Repo and branch:** `D:\Cyberpunk-Minecraft`, branch `spike/collision-and-movement`, which holds only this handoff
  on top of `main` (`55b2a85`). Spike work stays on the branch, never on `main`. The repo has no git remote.
- **Process:** the one spikes 1, 3 and 4 used:
  1. `superpowers:brainstorming` with the user, to agree what spike 5 tests and how;
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
  - `docs/spikes/2026-10-02-minecraft-link-results.md`: the spike's verdict at the top, then step 3's "Clicks refused
    after a teleport" and "Surprises";
  - `docs/superpowers/specs/2026-10-02-minecraft-link-design.md`: "The scope pass", parts 5 (puppeting) and 6
    (collision), and "Cyberpunk's input";
  - `docs/spikes/2026-10-01-physx-probe-results.md` and `docs/spikes/2026-10-01-physx-abi-audit.md`: the city's
    collision, the origin rule, and which PhysX calls are safe;
  - spike 2's handoff, `docs/superpowers/handoffs/2026-10-01-spike-2-player-puppeting.md` on branch
    `spike/player-puppeting`. Spike 5 takes over its question for build mode; its facts are folded in below.
- **A first message to start the next session with:**
  > Read docs/superpowers/handoffs/2026-10-03-spike-5-collision-and-movement.md and start spike 5.

## Why this comes next

Spike 4 answered yes: Night City shows Minecraft's live world, and the user builds from inside the game. It left two
problems that come from the same choice, V walking and Minecraft's player following by teleports:
- **V walks through Minecraft's blocks.** Nothing on Cyberpunk's side collides with them. Where V stands inside or
  under blocks, Minecraft's player crawls and its clicks land short (the results' "Minecraft's eye among blocks").
- **Clicks fail at random while V moves.** Minecraft's server refuses a click while it waits for a teleport to be
  confirmed, with a misleading "The height limit for building is 1023", and the link teleports up to 10 times a
  second.

SkyCraft's own model answers both: **Minecraft owns the player.** Its player moves with Minecraft's physics, against
Minecraft's blocks and the game's exported shape, and the game's player follows it. There are no teleports while it
drives, so no refused clicks. The user chose that for build mode: **in build mode the user moves as Minecraft's
player, and outside it V stays Cyberpunk's.** So V also needs to collide with blocks itself, or leaving build mode on
top of a build drops V through it.

## What spike 5 is for

A first wording, for the brainstorm to settle: **in build mode, can the user move as Minecraft's player (walking,
jumping, sneaking and flying with Minecraft's physics, against its blocks and Night City's shape) with V following
smoothly; and outside build mode, can V stand on Minecraft's blocks and bump into them?**

It has three parts, which spike 4's scope pass rated (its parts 5 and 6, and a new one):
1. **Moving as Minecraft's player in build mode:** movement keys to Minecraft, and V puppeted to Minecraft's player
   every frame. Rated medium to high, about 400 lines. Spike 2's open questions apply (see "Puppeting in Cyberpunk").
2. **Night City's shape in Minecraft:** without it, Minecraft's player has nothing to walk on, and walks through
   walls. A flat stand-in floor is low effort (about 150 lines); Night City's real collision is high (1,500 lines or
   more; SkyCraft's Havok exporter is 1,282).
3. **V against blocks outside build mode:** not in the scope pass. SkyCraft does it only for NPCs, since its player
   always moves as Minecraft's (see "Known facts"). In Cyberpunk the likely way is PhysX shapes in the city's scene,
   which is new ground.

### Questions to start the brainstorm with

This is a starting list, not a decision; agree the scope with the user.
1. **Scope:** all three parts in one spike, or the first two now (they make build mode whole) and V against blocks
   next? Part 3 also decides what leaving build mode on top of a build does.
2. **Entering and leaving build mode:** on entering, Minecraft's player is already at V, from the link's last
   teleport, and held there (see the hold below). The link stops teleporting and sends voxel regions, Minecraft
   releases its player, and from then on Minecraft drives. On leaving, V stays where Minecraft's player was, and the
   link's teleports resume. What does V do during the hold, and what if the user leaves build mode in mid-air?
3. **The look:** keep Cyberpunk's own mouse look, as spike 4 does (V's camera forward becomes `SkyState`'s yaw and
   pitch), or integrate the raw mouse deltas ourselves with Minecraft's formula, as SkyCraft does, and set the camera
   ourselves? The first needs nothing new if the camera still turns while V's movement is off; the second gives
   Minecraft's feel but needs the camera control spike 2 never found.
4. **Which keys go to Minecraft in build mode:** at least W, A, S, D, Space, Shift and Ctrl (Minecraft's defaults:
   Shift sneaks, Ctrl sprints, Space twice flies in creative). Maybe Q (drop), F (swap hands) and F5 (camera); not E
   (inventory), since Minecraft's screens aren't usable from the game yet. The forwarded key is the physical key, and
   Minecraft applies the user's own bindings. Spike 4's menu keys still leave build mode, and Shift+F6 must still open
   CET's overlay once Shift is Minecraft's (in spike 4, Shift was never forwarded).
5. **Puppeting V:** FreeFly's way, `TeleportationFacility.Teleport` every frame with the `NoMovement` status effect,
   from C++? Where in the frame, so the camera and BlockRender's drawing see the same position (BlockRender takes the
   camera from Streamline, about 2 ms before the G-buffer)? And how smooth: port SkyCraft's tick interpolation (about
   130 lines of `Game.cpp`) at once, or start simpler and measure?
6. **Night City's shape:** a voxel floor first (the stand-in, as voxels), then PhysX regions? Which shapes to leave
   out (spike 1: V's own capsules; maybe cars, NPCs and litter, which are dynamic), how far around Minecraft's player,
   how often, and on which thread (the probe's extraction took 2 to 6.5 ms on the main thread for ±32 m)?
7. **V against blocks:** static PhysX actors in the city's scene for the solid blocks near V (boxes, or one shape per
   section), or SkyCraft's push-out (V nudged out of blocks every frame, which can't hold V up on top), or a hook on
   V's controller (as SkyCraft's `DigPhysics.cpp` does in Havok)? Actors would also stop NPCs and cars, which may or may
   not be wanted.
8. **What can be tested without the game:** `mclink` can test the voxel floor, held movement keys and the ticks
   (parts 1 and 2's Minecraft side). Puppeting and PhysX need the game.

### A possible order of testable steps

A suggestion only, in the spirit of "testable steps first":
1. **`mclink`, without Cyberpunk:** a voxel floor releases Minecraft's held player; held movement keys walk it, jump
   it onto a block and stop it at a wall of blocks; `McState`'s ticks show the motion; clicks made while it walks are
   never refused.
2. **In the game, on the voxel floor:** build mode drives V from Minecraft's player, and leaving build mode hands V
   back where it stands.
3. **In the game, Night City's shape:** PhysX regions replace the floor, so Minecraft's player walks the streets and
   stairs and stops at walls.
4. **In the game, V against blocks:** V stands on blocks and bumps into them outside build mode, including just after
   leaving build mode on top of a build.

## Known facts

### What SkyCraft's Skyrim side does (from its source)

SkyCraft 0.1.2 at `bfcaf17`: the version spike 4 runs. Its design doc (`docs/DESIGN.md`) is an older draft, and some
of it differs from what was built (it planned Havok statics for NPCs to collide with blocks): the code is the
reference. The source was cloned in an earlier session to:

`C:\Users\BLOXY\AppData\Local\Temp\claude\D--Cyberpunk-Minecraft\818fd1fd-489f-482d-9873-88e6752bad04\scratchpad\skycraft`

That's a temporary folder; if it's gone, cloning again is a download, so ask the user first. The repo keeps only the
protocol header (`extern/skycraft/`).

- **Minecraft holds its player after every teleport** until the game's shape around it has arrived
  (`SkyClient.holdUntilReady`, Fabric side): the voxel regions holding the feet's block, the one below it and the one
  8 blocks below must be known, and there must be solid voxel ground within 12 blocks below in a 3 × 3 column (or,
  once they're known, the hold must be 6 s old). Only voxel messages (`kColRegion`) make a region known; triangles
  (`kColTris`) never do (`SkyCollision.readRegion`). While held, `McState.teleportAck` stays one behind
  `SkyState.teleportSeq`. Spike 4's floor sends triangles only, which is why its player stayed pinned.
- **Teleports are rare in SkyCraft.** Minecraft teleports its player only when `SkyState.teleportSeq` changes. The
  Skyrim side bumps it when Minecraft connects or restarts, when the world changes, when Skyrim moves the player itself
  (load doors, fast travel, more than 300 units in a frame), when Skyrim hands the player back after furniture or a
  scene, and when Minecraft has held its player more than 8 blocks from Skyrim's for a second. Otherwise Minecraft
  drives: the puppet is on while Minecraft is in its world and has acknowledged the last teleport, Skyrim isn't
  loading, and the player is neither dead nor in Skyrim's hands (`Game.cpp`, `PerFrame`).
- **The look stays on the game's side.** Skyrim integrates the raw mouse deltas with Minecraft's own formula
  (`s = sensitivity × 0.6 + 0.2`, degrees = delta × s³ × 8 × 0.15, with `McState.sensitivity` from Minecraft's
  options), sends yaw and pitch in `SkyState`, and turns its own camera from them, so looking adds no latency.
  Minecraft sets its player's look from `SkyState` every frame.
- **The puppet, every frame** (`Game.cpp`, about 130 lines of it):
  - Minecraft's 20 Hz ticks (`McState`'s previous and current feet, `tickQpc`, `tickMs`) are interpolated on the
    game's own clock, rendering a little in the past, never extrapolating. The delay adapts between 4 and 30 ms to how
    late ticks arrive, so a tick has always arrived before it's needed. Sampling `McState`'s per-frame position
    instead judders, because the two games' frames aren't in phase.
  - Then `SetPosition` on the player, the character controller's velocity zeroed and its fall height reset, its
    on-ground or in-air state set from `McState`'s flags, and its gravity left on (Skyrim's NPCs won't melee a target
    they think is in the air).
  - The camera goes to Minecraft's eye, with Minecraft's walk bob added.
- **Input while Minecraft drives:** every key is Minecraft's except an allow-list for Skyrim's menus (Esc, the
  console, G to activate, H, J, M, O for Minecraft's own pause, F9). Skyrim's `PlayerControls` handler is hooked to
  get nothing; disabling the player's controls instead would also stop NPCs fighting the player (`Input.cpp`).
- **The collision exporter** (`Collision.h` and `.cpp`): regions of 8 × 8 × 8 blocks around Minecraft's player. The
  main thread walks Havok's world under its read lock and copies the triangles and convex primitives touching each
  region. A worker thread voxelizes them at 1/8 block into `ColBlock` masks (`kColRegion`). It sends the triangles
  first (`kColTris`), which Minecraft uses for exact ray casts (`SkyClip`) and, as the protocol puts it, the player's
  smooth collider. A new epoch (`kColClear`) clears Minecraft's store when Minecraft reconnects or the world changes.
- **Blocks against the game's actors:** Minecraft sends each section's solid blocks as a bitset (`kRenSolids`).
  - `NpcBlocks.cpp` pushes NPCs out of solid blocks sideways every frame (a circle against block squares, sliding
    along walls);
  - `PathAvoid.cpp` routes NPC paths around blocks;
  - `DigPhysics.cpp` adds the blocks near each NPC as extra planes in Havok's character proxy solver, through
    reverse-engineered hooks, so NPCs stand on blocks and bump into them;
  - the player's own body takes only the blocks it stands on, since Minecraft does the rest. SkyCraft creates no
    physics bodies for blocks.

### The protocol pieces this spike uses (`extern/skycraft/skycraft_protocol.h`, version 11)

- **`McState`** (Minecraft → game, seqlock): the interpolated feet, yaw and pitch, eye height and eye position; flags
  for in world, screen open, on ground, sneaking, sprinting, dead, swimming and flying; `teleportAck`; the mouse
  `sensitivity`, the FOV, the walk bob and the camera mode; and the raw ticks: `tickQpc`, previous and current feet,
  eye heights, walk distances, bob, and `tickMs`.
- **`SkyState`** (game → Minecraft, seqlock): flags for in game, menu open and loading; `worldId`; `collisionEpoch`;
  the position (used only when `teleportSeq` changes); the look (yaw and pitch); `teleportSeq`; the viewport; the hour.
- **The collision ring** (game → Minecraft, at 0x20000, 32 MB), messages 8-byte aligned, each `{u32 type, u32
  payloadBytes}`:
  - `kColClear {u32 epoch}`;
  - `kColTris`: a `ColRegion` (count = triangles) and `ColTri[]` (9 floats and flags: stair helper, diggable with a
    material, ghost, terrain), sent before its region;
  - `kColRegion`: a `ColRegion` (an inclusive block box, the epoch, the count) and `ColBlock[]`, each a block's
    `x, y, z` and 8 × u64: `bits[y]` bit `z × 8 + x` is the 1/8-block sub-voxel (x, y, z), in Minecraft axes.
    A region message replaces all collision in its box, so an empty one marks a region known and empty.
- **`kRenSolids`** in the render ring (Minecraft → game): per section, `RenSolids {sx, sy, sz, count}` and, when
  count isn't 0, a 512-byte bitset, bit `x + 16z + 256y`. BlockRender's link already receives these and steps over
  them (`src/link/Link.cpp`, the drain's switch).
- **The input ring:** `kInKey` (SDL scancodes, press and release), `kInMouseButton`, `kInScroll` and
  `kInReleaseAll`, as spike 4 forwards them.
- **`WaterGrid`:** water surfaces around the player, for swimming. Later.

### BlockRender after spike 4 (`spikes/block-render/`, 0.10.0, on `main`)

- **`src/link/`:** the mapping and `SkyState`; teleports from `Link::Update` (each time V's camera moves 0.1 m, at
  most 10 a second, settings `teleportstep` and `teleportrate`); the floor (`Floor.cpp`: triangles only, 7 × 7 regions
  of 8 blocks at V's feet, sent again when V is 16 m away horizontally or 1 m vertically); the render ring's drain
  (sections, the atlas, the HUD).
- **`src/input/`:** the patch on the game's `GetRawInputData` import, which hides an input by rewriting the event the
  game reads (button and wheel flags cleared, or a key's virtual key set to 0xFF); `BuildMode`, which decides each key
  and button; the focus watch, which releases everything in Minecraft when the game loses the front; `Scancodes`
  (set-1 scan codes to SDL's, SkyCraft's table). Mouse movement is never touched; the game reads it from `lLastX` and
  `lLastY`.
- **`tools/mclink/`:** the offline harness, BlockRender's link code in a console program against the real Minecraft.
  It runs one action a frame at 60 Hz. Its `key` presses and releases at once, so holding a movement key needs new
  actions. It prints only the first 20 sections it receives, then every 100th.
- **Measured:** our half of the latency within a frame; the HUD 0.25 ms a frame; Minecraft running costs Cyberpunk
  about 2 ms a frame.

### The game's physics (spike 1, and the game's imports)

- **PhysX 3.4.2,** a modified build, in separate DLLs. `PxGetPhysics` is exported. Scene 0 is the city's world
  (4,000 to 6,000 actors); the other scenes are copies of a five-plane scene that come and go. `eREQUIRE_RW_LOCK` is
  off. Gravity is (0, 0, −9.81): Z is up.
- **V's controller is very likely PhysX's own.** The executable imports `PxCreateControllerManager` from
  `PhysX3CharacterKinematic_x64.dll`, and `PxCreateCooking` (meshes built at run time), and V's collider is a
  kinematic capsule: radius 0.32, half-height 0.4, centre exactly 1 m above V's feet, query filter 0:0:2000:0,
  simulation filter 1:0:41ce:0. If it's a PhysX controller, static actors added to scene 0 with the right filter data
  should stop V. **Not tested.**
- **The ABI:** `PxPhysics`, `PxScene` and the actor interfaces match NVIDIA's public 3.4.2 headers. `PxShape` has one
  more virtual (slot 6), so every later slot is one higher, setters included: `setQueryFilterData` and
  `setSimulationFilterData`, which actors made by us would need, are exactly among the shifted ones. The audit
  checked only the slots the probe calls. Creating objects (`createRigidStatic`, `createShape`, `createMaterial`,
  `attachShape`, `addActor`, `removeActor`) needs those slots audited first, the same way.
- **The origin:** PhysX coordinates = world − origin, the 1,024 m grid point nearest V, one frame behind V. Read it
  from V's controller. **Unknown:** whether the game moves the whole scene when the origin changes
  (`PxScene::shiftOrigin`, which would move our actors too) or only its own actors. Actors we add must stay right
  across a line either way.
- **Unknown:** when, and from which thread, it's safe to add or remove actors in scene 0. PhysX 3.4 buffers some
  writes made during a simulation step; the game's own threads and locks are unread.
- **The city's collision** (spike 1's census): boxes, triangle meshes, convex meshes, capsules, spheres, five
  heightfields and a plane. The commonest static filters are 0:0:449c004:0 (query) and 5c004:0:13ce:0 (simulation).
  Around V, ±32 m holds 21,000 to 77,000 triangles. Character controllers aren't world collision: leave kinematic
  capsules out.
- **The probe's `spikes/physx-probe/src/physx/`** is the base to build on: `PhysxSession` calls only audited methods,
  on objects whose vtables are audited tables, and checks every array before reading it. It does the region work
  (overlap, then triangles for every geometry type, in `ShapeTriangles`) under the scene's read lock, and
  `ReadController` reads V's controller every frame.

### Puppeting in Cyberpunk (spike 2's facts)

- **FreeFly** (installed: `bin\x64\plugins\cyber_engine_tweaks\mods\freefly\modules\utils\logic.lua`) puppets V from
  Lua every frame:
  - `Game.GetTeleportationFacility():Teleport(player, pos, EulerAngles.new(0, 0, yaw))`, with the mouse's yaw added
    back in, since a teleport sets the body's yaw; it doesn't set pitch;
  - the status effects `GameplayRestriction.NoMovement`, `NoZooming` and optionally `NoCombat`, applied and removed
    through `Game.GetStatusEffectSystem()`;
  - no teleport while V is in a vehicle; health set to 100 every frame, presumably against fall damage.
- **The C++ route:** RED4ext's `TeleportationFacility` and `StatusEffectSystem`, found by script name and called with
  `RED4ext::ExecuteFunction`, as the probe's `PlayerLocator` calls the camera and player systems every frame.
- **Unknown:** how smooth a per-frame teleport looks, whether the mouse still turns the camera under `NoMovement`,
  pitch, the order within the frame (the teleport, the camera, BlockRender's drawing), and fall damage.
- **The stand-in player:** `GetLocalPlayerMainGameObject` returns a stand-in V from the startup screen on, so nothing
  should start by itself. Build mode's F8 is already an explicit trigger.

### Coordinates

- World: metres, X east, Y north, Z up. Minecraft: X east, Y up, Z south. `McFromWorld(x, y, z) = (x, z, −y)`, in
  `spikes/block-render/src/Coords.h`.
- Blocks sit on whole metres, and the game's ground doesn't: blocks float a few centimetres or sink up to 0.6 m. V on
  top of a block and V on the street differ by that much.

## After spike 5

NPCs, cars and bullets against blocks (if spike 5 leaves them out); blocks in the game's shadow maps; translucent
blocks; entities, items and particles; Minecraft's lights; water through `WaterGrid`; Minecraft's screens (inventory,
chat) from the game; timing removals for the latency line; a request that lists the renderer's sections.

## The game install, right now

- **BlockRender 0.10.0** is in `E:\Cyberpunk2077\red4ext\plugins\BlockRender\`. Nothing shows until the user links,
  picks a drawing mode or presses F8. Its overlay, `blockrender_hud`, shows the status, the build-mode line and the
  buttons.
- **Spike 1's probe 0.4.0** (`PhysXProbe`, acting only on F10 or its request file) and its `physxprobe_hud` overlay
  are still installed. **FreeFly** is installed too.
- **Minecraft:** the Modrinth App's `CyberCraft` instance, Minecraft 26.3 with Fabric Loader 0.19.5, Fabric API
  0.161.0+26.3 and `skycraft-fabric-0.1.2.jar`, run with `-Dskycraft.quitWithSkyrim=false`. Its `SkyCraft` world was
  emptied of every test block on 2026-10-03.
- **The repo's CMake caches** have `CP2077_DIR` empty, so builds don't install into the game.

## Rules to keep

- **No sub-agents:** work inline.
- **The user does every in-game action:** launching and quitting both games, loading, moving, keys, clicks and
  screenshots. Never drive their mouse or keyboard; ask, then wait for them to say they're done.
  - Give on-screen aids when a test needs positions, headings or counts. The overlays show V's position and the
    status.
- **Deep reviews:** expect the user to ask for a deep review of the spec and of the plan before each goes ahead. Trace
  every claim to its source (SkyCraft's code, the protocol header, the game's binaries), simulate the in-game steps,
  and break each new test once to prove it can fail.
- **Settle important unknowns** before moving on, rather than leaving them as next steps. Offer that.
- **Testable steps first:** sequence the spike so every step can be checked right then.
- **Builds and the game:**
  - Never build with `CP2077_DIR` set, or deploy, while Cyberpunk runs, because the running game locks the DLL.
    `tasklist | grep -i -c "cyberpunk2077.exe"` must print `0`.
  - Set `CP2077_DIR` only to deploy, and clear it again straight after.
  - `mclink` needs Cyberpunk closed: Minecraft links to one game side at a time.
- **No faults:** the game's crash handler sees a fault before any `__try/__except`, and catching one freezes the game.
  Plugin code must never fault: check every pointer, size and vtable before using it. If the game freezes, read the
  logs and the newest `ReportQueue` folder, and force-close it only when the user asks.
- **Downloads** (SkyCraft's source again, anything else) need the user's explicit permission first.
- **Commits** end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`, and go on the spike branch.
- **After the spike:** use `superpowers:finishing-a-development-branch`. The user has chosen a local fast-forward
  merge to `main` all four times so far; ask again.

## Gotchas

- **Bash's working directory:** a `cd` in a Bash call changes it for later calls. Wrap it in a subshell, `(cd ... &&
  ...)`.
- **Backslashes:** the Bash tool turns `\\` inside a command into `\`. Write files with the Write or Edit tools, not
  heredocs or `python -c`, when they hold backslashes.
- **Tool paths:** `cmake`, `ctest` and `dumpbin` aren't on `PATH`; spike 4's plan has their full paths. Pass MSBuild
  switches as `-m`, never `/m`. `luac` (Lua 5.4) is on `PATH`, for checking CET Lua.
- **Python:** run it with `uv run --no-project`. The tools' tests: `uv run --offline --no-project --with pytest --with
  numpy --with pillow pytest tools -q`, in `spikes/block-render`.
- **Scratch builds under `%TEMP%`** miss header changes (MSB8029): rebuild from clean after editing a header.
- **Waiting on the game:** a foreground `sleep` is blocked. Record the status in the background with
  `spikes/block-render/tools/status_watch.py <seconds> <interval> "<keys>"`, and read the recording afterwards.
- **Request files:** write them atomically (a temporary name, then renamed), and wait for the status's `request=` line
  to answer before writing the next.
- **Minecraft:** the user starts and stops it from the Modrinth App. Its log is
  `%APPDATA%\ModrinthApp\profiles\CyberCraft\logs\latest.log`; SkyCraft's lines start with `SkyCraft:`. It logs every
  teleport, its first 10 section meshes and every 200th, and its first 20 clicks. It saves changed chunks at once.
- **Checking Minecraft's world:** a fresh `mclink` side gets every section with blocks near Minecraft's player, so a
  short script that links and waits lists what's there.
- **The error reporter:** `REDEngineErrorReporter.exe` starts with the game every time. Seeing it running isn't a
  crash; check `ReportQueue` instead.
- **Logs:** each plugin writes one per launch in `E:\Cyberpunk2077\red4ext\logs\`.
- **Git:** for a fast-forward merge, `git switch main` then `git merge --ff-only <branch>`.
- **The scratchpad** is per session. The plan tooling's copies are in `tools/plans/`; spike 4's in-game helpers
  (request sender, recording parsers) are gone.
