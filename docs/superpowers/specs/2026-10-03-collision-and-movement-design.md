# Collision and movement: design (spike 5)

Date: 2026-10-03. Status: agreed with the user section by section in the brainstorm. "Found while writing", near the
end, lists what writing it out added to what was agreed. Then revised after a deep self-review, which checked each
claim against its source and walked through each step: "Changes from the deep review", at the end, lists what changed
and what was checked. The sections above it already include those changes.

## Purpose

This is spike 5 of the project that recreates [SkyCraft](https://github.com/chasmlol/SkyCraft) (Minecraft in Skyrim)
for Cyberpunk 2077. Spike 4 linked the real Minecraft: Night City shows Minecraft's live world, and in build mode (F8)
the user builds from inside the game. It left two problems, both from one choice, V walking and Minecraft's player
following by teleports:
- **V walks through Minecraft's blocks.** Where V stands inside or under them, Minecraft's player crawls, and its
  clicks land short.
- **Clicks fail at random while V moves.** Minecraft's server refuses a click while it waits for a teleport to be
  confirmed, and the link teleports up to 10 times a second.

SkyCraft's own model answers both: Minecraft owns the player. Its player moves with Minecraft's physics, against
Minecraft's blocks and the game's exported shape, and the game's player follows it, with no teleports while it drives.
The user chose that for build mode only: in build mode the user moves as Minecraft's player, and outside it V stays
Cyberpunk's. So V also has to collide with blocks itself, or leaving build mode on top of a build drops V through it.

> In build mode, can the user move as Minecraft's player (walking, sprinting, jumping, sneaking and flying with
> Minecraft's physics, against its blocks and Night City's real collision), with V following smoothly and clicks
> never refused? And outside build mode, do Minecraft's blocks stop V, and whatever else the city's walls stop, so
> that V can stand on a build after leaving build mode?

It has three parts:
1. **Moving as Minecraft's player in build mode:** the movement keys go to Minecraft, and V is placed at Minecraft's
   player every frame.
2. **Night City's shape in Minecraft:** a stand-in floor first, then the city's real collision, read from PhysX.
3. **V against blocks outside build mode:** PhysX static actors for the blocks near V.

Each is tested against something real as soon as it's built (see "The steps").

## Decisions from the brainstorm

- **Scope: all three parts,** in the handoff's order of testable steps. The user chose this over doing the first two
  now and V against blocks as spike 6.
- **The parts in order, each planned after the step before it,** as spike 4 was. Each in-game step opens by checking
  its own unknowns with the least code, before the rest of it is built.
- **Cyberpunk keeps the look.** The mouse still turns V's camera, and the link sends the camera's direction to
  Minecraft, as in spike 4. Minecraft's own mouse formula would need a way to set the camera's pitch, which nobody has
  found.
- **Build mode gives Minecraft the movement keys:** W, A, S, D, Space, Left Shift and Left Ctrl, on top of spike 4's
  buttons, wheel and 1 to 9. Cyberpunk's crouch keys are hidden from the game without being forwarded, so V can't
  crouch under the puppet. Everything else (F, Q, R, Tab and the rest) stays Cyberpunk's.
- **Leaving build mode in mid-air lets V fall,** as in the game, fall damage included.
- **Blocks stop whatever the city's walls stop:** V, and probably cars, NPCs and bullets. Step 4 records which.
- **SkyCraft's mod stays unchanged** (0.1.2, protocol 11). Everything this needs is in the protocol already.
- **Everything goes into BlockRender (0.11.0),** which owns the mapping, the input patch and the render ring. Spike 1's
  PhysX reader moves into it as `src/physx/`. A separate plugin would have to share the collision ring and the blocks'
  solids with it across DLLs.
- **Step 3 opens offline:** `mclink` sends the static shapes from spike 1's saved dumps through part 2's own code, so
  Minecraft's player walks real Night City geometry before the game is involved.

## Non-goals

- **NPCs routing around blocks.** Their navmesh doesn't know about blocks. SkyCraft needed its own path code for that
  (`PathAvoid.cpp`).
- **Minecraft's camera effects:** its walk bob, its sprint and flight field of view, and third person.
- **V's walking animation.** V is placed, not walked, so it slides.
- **Minecraft's screens** (inventory, chat) from the game, and any key beyond the movement keys.
- **Moving city shapes in Minecraft:** cars, NPCs, litter, and kinematic things such as doors and lifts. Minecraft's
  player walks through them.
- **Exact block shapes for V.** Slabs, stairs and fences are whole cubes, since `kRenSolids` says only whether a block
  is solid.
- **Water** (`WaterGrid`), **digging into Night City**, and **fighting** between Minecraft and the game.
- **Changes to SkyCraft's mod,** and gamepads.
- **Only these versions:** game 2.31, SkyCraft 0.1.2 (protocol 11), Minecraft 26.3, and the PhysX DLLs spike 1
  audited.

## Environment (checked on 2026-10-03)

Each fact below was read in its source: SkyCraft's code at `bfcaf17` (the clone in an earlier session's scratchpad),
the protocol header, the game's files, FreeFly's Lua, the RED4ext SDK, and BlockRender's and the probe's code.

### The game and BlockRender

- **The game:** Cyberpunk 2077 2.31, with spike 3's settings.
- **Installed:**
  - BlockRender 0.10.0, which does nothing until a link, a drawing mode or F8;
  - spike 1's probe 0.4.0, which acts only on F10 or its request file, and its overlay `physxprobe_hud`: position,
    heading, and the distance to each origin line;
  - FreeFly, on F7.
- **BlockRender's frame** (`Plugin.cpp`, `OnUpdate`), in order:
  1. requests, once a second;
  2. `UpdateLink`:
     - `LocatePlayer`: V's feet, camera and forward;
     - `Link::Update`: the heartbeat, `McState`, teleports, the floor, `SkyState`, typed commands, events, the render
       ring and the HUD;
     - the router's view of the link;
  3. the renderer's `Update`.
- **The link today:**
  - **Teleports:** each time V's camera moves 0.1 m, at most 10 a second. Each puts Minecraft's eye at V's camera, its
    feet 1.62 below.
  - **The floor:** two triangles a region, over 7 × 7 regions of 8 blocks at V's feet. It's sent again with a new
    epoch once V is 16 m away horizontally or 1 m vertically.
  - **`SkyState`'s menu flag** is set whenever build mode is off.
  - **`teleportSeq` starts at 0** each run.
  - **The render ring's `kRenSolids`** are stepped over.
- **V's camera** is 1.60 m above V's feet when standing. In spike 4's step 2, the camera was at Minecraft height 7.744
  and the floor at V's feet at 6.14.

### Cyberpunk's movement keys

From `r6\config\inputUserMappings.xml`. The user has rebound nothing.

| Key | Cyberpunk |
|---|---|
| W, A, S, D | Move |
| Space | Jump |
| Left Shift | Toggle sprint (Right Shift holds sprint) |
| Left Ctrl | Dodge |
| C | Toggle crouch, and dive |
| Right Ctrl | Hold crouch |
| G | Toggle walk |

### Minecraft's keys in the `CyberCraft` instance

From the instance's `options.txt`:
- **W, A, S, D and Space** are Minecraft's defaults.
- **Left Ctrl sneaks and Left Shift sprints:** `key.sneak:key.keyboard.left.control` and
  `key.sprint:key.keyboard.left.shift`, with toggle sprint on. This is the user's own swap.
- **Other keys:** F is third person, F5 swaps hands and Q drops. C and X save and load toolbars. Slots 8 and 9 are on
  the side buttons.

The link forwards physical keys, and Minecraft applies these bindings. So in build mode Left Shift sprints, and Left
Ctrl sneaks (and flies down).

### SkyCraft's Minecraft side, for a player it drives

- **Teleports** (`SkyClient.beginFrame`):
  - a new `teleportSeq`, or a new player object, teleports the player to `SkyState`'s position, on the client and the
    integrated server. It happens only while the in-game flag is set and loading isn't;
  - while not in game, the player is parked where it is, and teleported when in game returns;
  - **the last `teleportSeq` is never reset when the game side restarts:** `SkyLink.poll` only notes the new process.
- **The hold** (`holdUntilReady`, every client tick):
  - after a teleport, the player is held at the target until three regions are known: those holding the feet's block,
    the block below, and the block 8 below;
  - it also needs voxel ground in the 3 × 3 column within 12 blocks below. Once the regions are known, 6 s since the
    hold began does instead;
  - then `liftOutOfGeometry` lifts the player onto the highest triangle surface up to 2.5 blocks above its feet, and
    lets go. Any surface counts (`TriCollider.groundAt` with walkable surfaces not required), so the underside of an
    overhang lower than that counts too.
- **`teleportAck`** (`afterRender`): `McState` reports the last `teleportSeq` applied, less 1 while the player is
  held. So `teleportAck == teleportSeq` means Minecraft has its player there, and lets it move.
- **The look** is set from `SkyState` every frame while no screen is open.
- **Held keys:**
  - `InputBridge` keeps a virtual keyboard, so a key stays down until its release arrives, and `isKeyDown` reads it;
  - a repeat is replayed as a repeat;
  - `SkyState`'s menu flag (or loading) releases everything, every frame.
- **The player collides only with triangles.** While linked, players use the "smooth collider" (`SkyCraftClient` calls
  `SkyCollision.setSmoothCollider`):
  - `BlockCollisionsMixin` leaves the game's voxels out of their block collisions;
  - `EntityCollideMixin` passes the local player's movement through `TriCollider`. It walks on triangles up to about
    45° (`|ny| ≥ 0.7`), steps up to the step height, slides along walls, and stops at ceilings.

  So voxel regions do two things: they release the hold, and they're what other entities (TNT, items, mobs) collide
  with.
- **Sneaking doesn't stop at edges** while linked (`PlayerEdgeMixin`), and the server allows flight
  (`MinecraftServerFlightMixin`).
- **The collision ring's reader** (`SkyCollision`) is its own thread, which looks for messages every 2 ms:
  - a clear empties everything and sets the epoch, and a message of another epoch is dropped;
  - a voxel region replaces everything in its box, and marks every region it overlaps as known;
  - a triangle message is stored under the region that holds its box's minimum corner.
- **The ticks** (`publishTick`, at the end of every client tick): the previous and current feet, the eye heights
  (smoothed as `Camera` smooths them), walk distances and bob, `tickMs`, and the tick's `QueryPerformanceCounter`
  time, corrected for the partial tick already gone.
- **Solids** (`WorldExporter`): after each section, `kRenSolids` lists its blocks that have any collision shape, as a
  4,096-bit set (bit x + 16z + 256y). Its count is 0 when the section has none left.

### Minecraft 26.3, for eye heights and clicks

From the user's 26.3 jar, with `javap`:
- **Eye heights** (`Avatar`'s pose table): 1.62 standing, 1.27 crouching, and 0.4 swimming, crawling and gliding.
- **The server corrects a move by teleporting the player back** (`ServerGamePacketListenerImpl.handleMovePlayer`), and
  so refuses clicks until the client confirms, in two cases:
  - the move ends colliding with something the old position didn't;
  - outside creative and spectator, the server's replay of the move lands more than a quarter block from the client's
    ("moved wrongly").

  The game rule SkyCraft turns off, `PLAYER_MOVEMENT_CHECK`, only removes "moved too quickly"
  (`shouldCheckPlayerMovement`). Players skip the game's voxels on the server too, so walking on the triangles
  shouldn't trigger either case. Step 1's clicks check it.

### SkyCraft's game side, as a reference

- **The puppet** (`Game.cpp`, `PerFrame`) runs while all of these hold:
  - Minecraft is in its world and has acknowledged the last teleport;
  - nothing is loading;
  - the player is alive, and not in Skyrim's hands.
- **Its `teleportSeq` starts at a value from `QueryPerformanceCounter`** each run (lines 23 to 29), so that a Minecraft
  still acknowledging the last run's teleport can't be taken for one that has arrived.
- **The interpolation** (lines 662 to 786):
  - it keeps up to 8 ticks, with their times locked to the `tickMs` rhythm (the stamps' noise filtered by 1/16);
  - two ticks seen in one frame are split, and a missed tick is bridged;
  - the render delay stays between 4 and 30 ms, set from how late the last 40 ticks (2 s) turned up. It grows by up to
    20 ms a second and shrinks by 2;
  - the feet and the eye height are interpolated within a tick, never extrapolated.
- **The exporter** (`Collision.cpp`):
  - regions of 8 blocks, 5 out horizontally, 3 below and 2 above, nearest first (height weighted double);
  - the nearest regions (±1 across, −1 to 0 up) are read again every second;
  - at most 3 regions and 2.5 ms a frame on the main thread, under Havok's read lock;
  - a worker voxelizes at 1/8 block with a triangle-box test;
  - after a load or a reconnect, it waits 1.5 s before reading, since bodies stream in over a few frames.
- **Blocks against NPCs:** a push-out, path avoidance, and planes added to Havok's character proxy. SkyCraft makes no
  physics bodies for blocks.

### FreeFly

`modules/utils/logic.lua`, every frame while flying:
- **The teleport:** `Game.GetTeleportationFacility():Teleport(GetPlayer(), newPos,
  EulerAngles.new(0, 0, GetPlayer():GetWorldYaw() + angle + logic.yaw))`. Its `logic.yaw` is the frame's
  `CameraMouseX` turn, scaled by the game's mouse sensitivity setting.
- **Health** is set to 100 every frame.
- **In a vehicle,** the teleport is skipped.
- **Status effects:** `GameplayRestriction.NoMovement` and `NoZooming` are applied while flying.

### Calling the game from C++

- `RED4ext::ExecuteFunction` on a class (`CClass*`) takes the instance from `GameInstance::GetSystem`. That's how
  `PlayerLocator` calls `PlayerSystem` and `CameraSystem` every frame.
- The game instance's system map is keyed by implementation types (`GameEngine.hpp`). `TeleportationFacility`'s script
  name names its implementation class, `gameTeleportationFacility`, a game system (`ITeleportationFacility :
  IGameSystem`), as `CameraSystem` names `gameCameraSystem`.
- `CClass::GetFunction` searches parent classes too, so a function declared on a base class is found.
- `EulerAngles` is `{Roll, Pitch, Yaw}`.
- FreeFly passes a pitch of 0 to `Teleport`, and still flies where the camera looks, up and down, so a teleport
  doesn't seem to reset the camera's pitch.

### PhysX (spike 1 and its audit)

- **The build:** PhysX 3.4.2, a modified one, in separate DLLs, with `PxGetPhysics` exported. Scene 0 is the city
  (4,000 to 6,000 actors), with `eREQUIRE_RW_LOCK` off. Z is up.
- **The origin:** PhysX = world − origin, where the origin is the 1,024 m grid point nearest V, one frame behind V. It's
  read from V's controller: a kinematic capsule (radius 0.32, half-height 0.4) whose centre is 1 m above V's feet.
  Spike 1's `ReadController` finds that actor once, then each frame checks the cached actor is still among scene 0's
  dynamic actors; its origin watch did that for 48,000 frames.
- **V's controller, if it's PhysX's.** It very likely is: the game imports `PxCreateControllerManager`, and V's
  collider is the kinematic capsule a controller makes. Then, from `PxController.h` and `PxControllerManager.h`:
  - it moves by sweeping against scene-query shapes, so a shape that's only a simulation shape doesn't stop it;
  - its geometry cache is flushed by itself whenever a static shape is added, changed or removed;
  - its overlap recovery, on unless the game turned it off, pushes it out of static objects it starts inside.
- **The ABI:** `PxPhysics`, `PxScene` and the actors match NVIDIA's 3.4.2 headers. `PxShape` has one more virtual, at
  slot 6, so every later slot is one higher; the repo's copy of the headers (`extern/physx-3.4.2`) declares it. The
  audit covered only the calls the probe makes.
- **The census:**
  - 7,596 shapes in scene 0 at one spot;
  - the commonest static filters are query 0:0:449c004:0 and simulation 5c004:0:13ce:0;
  - `userData` is set on every actor but one: 5,780 of 5,781, and 4,623 of 4,624 in another dump.
- **The region's cost:** for ±32 m across and from 16 m below V to 32 m above:
  - 21,000 to 77,000 triangles;
  - 2 to 6.5 ms to extract them on the main thread, plus under 1 ms to select and overlap.
- **Adding actors:** `PxScene.h` says `addActors` "does not support buffering. It may not be called during
  simulation", and `addActor` carries no such note. Neither is safe against another thread writing to the scene at the
  same time, and which threads the game writes from is unknown.

### Spike 1's saved collision

`E:\Cyberpunk2077\red4ext\plugins\PhysXProbe\dumps\<time>\collision.obj` holds each dump's region as triangles:
- in Minecraft's axes, relative to V's feet, to the millimetre;
- with one OBJ object per category (`s0_static_trimesh`, `s0_static_heightfield`, `s0_dynamic_box` and so on).

Spot B (`2026-10-01_14-37-07`) has 64,192 static triangles. The ramp (`14-46-55`) and "past the line" (`14-50-33`) have
fewer.

## The protocol, as this spike adds to it

SkyCraft's header, `extern/skycraft/skycraft_protocol.h` (version 11), stays unchanged.

- **`SkyState`:**
  - `teleportSeq` starts each run at a value from the clock, as SkyCraft's does. It goes up on linking, when Minecraft
    comes back, for each teleport outside build mode, and once on entering build mode;
  - the position is V's camera less 1.62 outside build mode, as now. In build mode it's V's feet, every frame, so that
    whenever Minecraft teleports its player by itself (a new player object, or the in-game flag coming back), it puts
    it where V is;
  - the menu flag stays clear in build mode, as now.
- **`McState`, newly read:**
  - `teleportAck`;
  - the flags on ground, sneaking, sprinting, dead, swimming and flying;
  - the raw ticks: `tickQpc`, the previous and current feet, `tickEyeO` and `tickEye`, and `tickMs`.
- **Input:** key events for W (SDL 26), A (4), S (22), D (7), Space (44), Left Shift (225) and Left Ctrl (224).
- **The collision ring, newly sent:** voxel regions (`kColRegion`).
  - Each is a `ColRegion` (an inclusive block box, the epoch, the count), then the `ColBlock`s.
  - A `ColBlock` is a block's x, y and z and 8 × u64: `bits[y]` bit `z × 8 + x` is the 1/8-block sub-voxel (x, y, z).
  - Each region's triangles (`kColTris`) go first, then its voxels.
- **The render ring, newly kept:** `kRenSolids`: a `RenSolids {sx, sy, sz, count}`, then 512 bytes when the count
  isn't 0.

## Repository layout

```
docs/superpowers/specs/2026-10-03-collision-and-movement-design.md   this file
docs/superpowers/plans/2026-10-03-collision-and-movement.md          the plan, written part by part
docs/spikes/2026-10-01-physx-abi-audit.md          + an addendum: the new PhysX calls
spikes/block-render/
  src/link/Ticks.*        Minecraft's ticks, interpolated on our clock (SkyCraft's Game.cpp, credited)
  src/link/Drive.*        build mode's drive: entering, waiting, driving, leaving, and V's target
  src/link/Voxels.*       triangles to 1/8-block voxels and ColBlocks (SkyCraft's triangle-box test, credited)
  src/link/Ground.*       the floor's and the city's regions: what to read next, filing, hashes, messages, the worker
  src/link/Solids.*       Minecraft's solid blocks by section, from kRenSolids
  src/link/Floor.*        + the floor's voxel regions
  src/link/Link.*         + the clock-started teleportSeq, no teleports while driving, the ground's messages, solids
  src/input/BuildMode.*   + the movement keys, and the crouch keys hidden
  src/physx/              spike 1's reader, moved in and trimmed: its headers, the audited tables, the checks,
                          ShapeTriangles, and the session (scene 0, the read lock, overlap, V's controller)
  src/physx/Export.*      the city's batches, read on the main thread within the budget
  src/physx/BoxMerge.*    a section's solid blocks as few boxes (pure)
  src/physx/Bodies.*      the blocks' static actors: made, posed, rebuilt and removed
  src/physx/SceneWatch.*  step 3's log of who writes to scene 0, and when (hooks, on request)
  src/Puppet.*            the game's script calls: Teleport, V's yaw, in a vehicle, dead, health
  src/Plugin.cpp, Requests.*, Status.*   + the new requests, the status lines and the update order
  tools/mclink/           + held keys, build mode's drive, and the dump loader
  tools/blockrender_hud/init.lua         + the new lines and buttons
  tests/                  + the new unit tests
```

The names are proposals; the plan settles them.

## Build

- **As in spike 4,** with these changes:
  - `render_core` gains the pure parts: `link/Ticks`, `Drive`, `Voxels`, `Ground` and `Solids`, `physx/BoxMerge`, and
    the plain-data half of `ShapeTriangles`;
  - `render_runtime` gains the parts that touch the game: the PhysX session, `Export`, `Bodies`, `SceneWatch` and
    `Puppet`;
  - the PhysX headers come from `extern/physx-3.4.2` as system includes, with `PX_PHYSX_STATIC_LIB`, as in spike 1.
    PhysX is called only through its virtual tables and inline header code, and `PxGetPhysics` through
    `GetProcAddress`, so the DLL imports nothing from PhysX;
  - `mclink` gains the dump loader and held keys, and still links only `render_core`.
- **The version** becomes 0.11.0, in the log line and in `Query`.
- **Deploying is unchanged:** `CP2077_DIR` is set only to deploy, with the game closed, and cleared straight after.

## Part 1: build mode while Minecraft drives

### Entering

Build mode turns on (F8 or `build on`) only when Minecraft is in its world, and V is found, alive and on foot: not in
a vehicle, and not in a workspot (sitting, or held by a scene), through the game's script calls, chosen while
planning. Otherwise it says why, as spike 4's does. Then:
1. **The ground:**
   - in `floor` mode, a fresh floor at V's feet, with its voxels (see "Part 2");
   - in `city` mode, the exporter's regions, which it keeps around V anyway.
2. **One teleport:** `teleportSeq` goes up once, with `SkyState`'s position at V's feet. Minecraft puts its player
   there and holds it.
3. **The keys:**
   - from this moment the movement keys are Minecraft's;
   - a key already held when build mode starts stays the game's until it's released, as spike 4's buttons do.
4. **Waiting:** until Minecraft's `teleportAck` equals our `teleportSeq`, Minecraft is still holding its player. V is
   held where it is, placed there every frame, so a movement key that was already down when build mode started can't
   walk V away. The status shows how long it has waited. There's no time limit: F8 leaves.
5. **Driving:** from then on, Minecraft drives. The link sends no more teleports while build mode is on, so no click
   is refused. If `teleportAck` falls behind again, because Minecraft teleported its player by itself (see "The
   protocol"), the drive goes back to waiting, and V is held where it is.

### Each frame while Minecraft drives

In `OnUpdate`, after `Link::Update` has read `McState`:
1. **The ticks** go into the interpolator (`Ticks`). It's ported from SkyCraft's `Game.cpp` with SkyCraft's constants
   (see "SkyCraft's game side"), and runs on `QueryPerformanceCounter`, the clock Minecraft stamps them with. It's fed
   every frame while linked, so it has settled by the time Minecraft drives. It gives Minecraft's feet and eye height
   at the render time.
2. **V's target** is Minecraft's eye (the interpolated feet plus the interpolated eye height), in world axes, less V's
   own eye height.
   - **V's eye height** is the camera's height above V's feet, averaged over the frames before build mode starts
     while V's feet aren't moving up or down (1.60 m standing in spike 4). It isn't read during build mode: a camera
     and feet read in the same frame may straddle a teleport, and V can't crouch in build mode, so the height doesn't
     change. If V was crouched when build mode started, it stays crouched, and its eye height is measured as it is.
   - **Horizontally,** V's feet go straight under Minecraft's eye. If V's camera sits off V's feet horizontally, the
     status's camera-to-eye distance shows by how much.
   - **So V's camera lands on Minecraft's eye.** Standing, V's feet are 2 cm from Minecraft's. Sneaking, Minecraft's
     eye is 1.27 above its feet and V can't crouch, so V sinks about 0.33 m: V's legs go into the floor, and the
     crosshair stays on Minecraft's pick ray.
   - **A target that isn't finite, or is more than 20 m from V,** isn't used: the frame is skipped and counted. If the
     targets stay bad for a second, build mode ends and says why.
3. **The teleport:** `TeleportationFacility.Teleport(V, target, EulerAngles(0, 0, yaw))`, found by its script name and
   called the way `PlayerLocator` calls the camera system.
   - The yaw is V's current one, from `GetWorldYaw` as FreeFly reads it.
   - If that fights Cyberpunk's turning, `set turn 1` adds the frame's turn from the raw mouse movement the input patch
     sees. Counts become degrees by a ratio measured while V turns outside build mode.
   - Which of the two works is step 2's opening check.
4. **The rest of the link,** as now:
   - the look from V's camera into `SkyState`, and V's feet as its position;
   - the ground around Minecraft's player. In `floor` mode the floor stays at the height it had when build mode
     started, and grows as the player moves, never with a new epoch, so nothing is cleared from under it.

**BlockRender's drawing** takes its camera from the frame the game renders (from Streamline), so the blocks stay locked
to the city wherever in the frame the teleport lands. The teleport's place in the frame only adds latency, which the
status's camera-to-eye distance shows.

There are no status effects at first: hiding the keys already stops V walking. If V's own movement fights the
teleports (falling between them, landing, fall damage), FreeFly's `GameplayRestriction.NoMovement` is the fallback,
and health set every frame is the last resort.

### Leaving

Build mode ends on F8, a menu key or Shift+F6, as now. It also ends when:
- the link goes off, or Minecraft goes or leaves its world (as now), or Minecraft's player dies;
- V is lost (loading), dies, or gets into a vehicle or a workspot. Cyberpunk's F (interact) still works in build mode;
- the targets stay bad for a second.

Then:
- V isn't placed any more, so it stays where Minecraft's player was. In mid-air it falls, as in the game;
- Minecraft lets go of everything, and `SkyState`'s menu flag repeats that every frame, as now;
- the link's teleports resume from V's camera, starting at once.

### Keys

`BuildMode` gains these, beside the buttons, the wheel and 1 to 9:

| Key (set-1 scan code) | In build mode |
|---|---|
| W (0x11), A (0x1E), S (0x1F), D (0x20), Space (0x39), Left Shift (0x2A), Left Ctrl (0x1D) | Forwarded to Minecraft, and hidden from the game |
| C (0x2E), Right Ctrl (0x1D with E0) | Hidden from the game, not forwarded |

- **Where a press goes:** each key follows spike 4's rule, that a press's repeats and its release go where the press
  went.
- **Shift+F6** still leaves build mode with Left Shift forwarded, since `BuildMode` notes both Shift keys before it
  decides anything else. Left Shift reaches Minecraft first, though, so in the user's instance it also toggles
  Minecraft's sprint on the way out.

### Status

A drive line:
- off, waiting (and for how long), or driving, with the reason when it's off;
- V's camera's distance from Minecraft's eye: the last, and this second's average and worst;
- the render delay, and the frames that waited for a late tick;
- frames skipped for a bad target;
- the teleport call's time;
- V's health, read through the game's stat pools, so fall damage shows.

The frame log (`log`) gains each frame's camera, target, render delay, and whether the frame waited for a tick, so
judder can be measured.

## Part 2: Night City's shape in Minecraft

### The ground setting

`set ground floor` or `set ground city` picks what Minecraft gets. Switching sends a clear with a new epoch, then the
other one's regions. Step 2 runs on `floor`. From step 3 on, `city` is the default, and `floor` stays as a fallback.

### The floor's voxels

The floor (spike 4's triangles at one height, over 7 × 7 regions) gains voxel regions:
- **each floor region's voxels,** made by the city's voxelizer from the floor's own triangles, so the floor tests the
  same code;
- **the regions just above and below it,** sent empty, so they count as known.

That releases Minecraft's player at once. The hold needs the regions at the feet, a block below and 8 blocks below,
and voxel ground within 12 blocks.

The floor's epochs:
- outside build mode, the floor still follows V with a new epoch each time, as in spike 4;
- entering build mode sends a fresh one at V's feet;
- while Minecraft drives, the floor grows around its player at that height, and never changes epoch.

### The exporter

- **What it reads:** scene 0, through spike 1's audited reader, moved into `src/physx/` and trimmed to what this
  needs:
  - the DLL and vtable checks;
  - `PxGetPhysics` and scene 0;
  - the scene's read lock;
  - an overlap query;
  - each shape's triangles (`ShapeTriangles`);
  - V's controller, for the origin.
- **What it keeps:** the scene-query shapes of static actors. That's what V's controller sweeps against, if it's a
  PhysX controller (see "PhysX"); a shape that's only a simulation shape doesn't stop V. From spike 1's census, about
  7,078 of 7,596 shapes are both, about 319 query-only and about 192 simulation-only.
- **What it leaves out:**
  - dynamic and kinematic actors: cars, litter and bodies, V's and NPCs' controllers, and perhaps doors and lifts;
  - triggers;
  - planes, which spike 1 skips too;
  - our own bodies from part 3.
- **The area** is SkyCraft's: regions of 8 blocks, 5 out horizontally, 3 below and 2 above (88 m square, 48 m tall).
  - It's centred on Minecraft's player while Minecraft drives, and on V otherwise.
  - It runs whenever the link is on, Minecraft is there and the ground is `city`, so the regions are ready when build
    mode starts. Without Minecraft its messages would only queue in a ring nobody reads, as spike 4's floor avoids.
- **Batches:** the main thread reads 2 × 2 × 2 regions at a time:
  - one overlap query for the batch's box, with a quarter-block margin as SkyCraft's has;
  - each shape's triangles culled to that box;
  - each triangle filed under every region its bounds touch.

  Batches go nearest first, in SkyCraft's order (height weighted double).
- **The budget:** batches are read in `OnUpdate`, under scene 0's read lock as spike 1 read it, until 1.5 ms have
  gone (`set exportbudget`). A batch is always read whole. Whether `OnUpdate` is a safe time to read every frame is
  what the scene watch (below) settles first.
- **The worker:** one thread takes each batch's triangles and, for each region:
  - voxelizes it at 1/8 block with SkyCraft's triangle-box test;
  - builds its messages: the triangles (`kColTris`), then the voxels (`kColRegion`).

  The main thread writes the finished messages into the collision ring, in order. If the ring has no room, they wait
  for the next frame.
- **Refreshing:**
  - the city streams its collision in as V moves, so the batches next to the centre are read again every second;
  - a region is sent again only if its triangles' hash has changed;
  - regions more than one beyond the area are forgotten, so they're read again on return.
- **Settling:** the exporter waits 1.5 s (SkyCraft's settle time) before reading, after the link starts, after
  Minecraft comes back, and after V is found again following a load.
- **Epochs:** a clear only when the link starts, when Minecraft comes back, or when the ground setting changes. Never
  while walking.
- **Coordinates:**
  - each triangle goes from PhysX's frame to world (plus the origin read from V's controller in the same frame), then
    to Minecraft's axes (`McFromWorld`);
  - each batch's box goes the other way, for its query.

### The scene watch

Spike 1 read scene 0's regions on the main thread about twenty times (its dumps), and V's controller every frame for
48,000 frames (its origin watch). The exporter reads far more, every frame, and part 3 writes to the scene, so both
need to know which threads the game writes to scene 0 from, and when. PhysX's read-write lock works whether or not the
scene requires it (`eREQUIRE_RW_LOCK` only makes its use compulsory), so our read lock keeps out the game's writes
only if the game takes the write lock. So step 3 opens with a watch:
- **Hooks** on scene 0's vtable (`NpScene`'s, audited first) record each call's thread and time, without doing
  anything else. The small five-plane scenes share that vtable, so only calls on scene 0 are kept. The calls:
  - adding and removing actors and aggregates, one or several at a time;
  - stepping the simulation (`simulate`, or `collide` and `advance`) and fetching its results (`fetchResults`, or
    `fetchResultsStart` and `fetchResultsFinish`);
  - the write lock (`lockWrite`, `unlockWrite`);
  - `shiftOrigin`, with its shift, which settles whether the game moves the whole scene when PhysX's origin moves.
- **The status** sums them by thread, and says which came while `OnUpdate` was running.
- **What it decides:**
  - if the game never writes to scene 0 during `OnUpdate`, or always takes the write lock to do it (which our read
    lock then excludes), the reads and part 3's writes stay in `OnUpdate`;
  - otherwise they move to where the watch shows the scene is quiet, such as just after the game fetches the
    simulation's results.
- **When:** on request (`scenewatch on`), and the hooks come out on `scenewatch off` and at unload.

### `mclink`'s dump loader

`mclink` loads a dump's `collision.obj`:
- it keeps the `s0_static_*` objects. These are the static actors' simulation shapes, with or without the scene-query
  flag, since the dumps put every query-only shape, of any actor, under `s0_queryonly_*` (the probe's `Category`).
  Nearly all of them are scene-query shapes too, so this stands in for the exporter's choice;
- it moves them to Minecraft's player's feet (the dump's origin is V's feet);
- it hands them to the same filing, voxelizing and message code.

Minecraft's player then walks real Night City geometry without the game, within the dump's ±32 m.

## Part 3: V against blocks

### Solids

The link keeps `kRenSolids` by section instead of stepping over it.
- **Checks,** like the other render messages: coordinates within spike 4's ranges, a count up to 4,096, and 512 bytes
  of bits when the count isn't 0.
- **Changes:** a count of 0 removes the section, and clear-all clears them all.
- **When:** solids are kept whether drawing is on or not, as sections are.

### Bodies

- **Where:**
  - the 3 × 3 × 3 sections around V's own;
  - a section's body is made when it comes within one section of V's, and removed when it's more than two away, so
    walking along a boundary doesn't churn bodies.
- **Shape:**
  - one PhysX static actor per section that has solid blocks;
  - its blocks merged into as few boxes as possible (`BoxMerge`: greedy along x, then z, then y), so a 16 × 16 layer is
    one box;
  - each box a `PxBoxGeometry` shape, in world axes.
- **Changes:** when a section's solids change, its body is rebuilt in that frame: the old actor removed and released,
  and the new one added. So a placed block stops V about when it's first drawn.
- **In build mode too:** bodies stay while V is placed by teleports, so leaving build mode on top of a build leaves V
  standing on them. Leaving while sneaking starts V's controller about a third of a metre inside the top block, and
  step 4 checks what the controller does then.
- **When:** while `blocks` is on and drawing is on, so blocks nobody can see never leave invisible walls. They stay
  while unlinked, or with Minecraft gone, as the drawn blocks do.
- **Off by default:** `set blocks 1` turns them on. It stays off by default until step 4 passes.

### What they're made of

- **Filters and material,** copied from the static shape under V's feet:
  - found with a ray down when the first body is made;
  - what's copied: the query and simulation filter data, the first material, and the shape flags. So the blocks are
    made of whatever V already stands on;
  - bodies wait until V stands on a static shape to copy from;
  - the status shows what was copied.
- **`userData`** stays empty. One actor in scene 0 already has none.

### The origin

Each body keeps its world position. After every origin change (seen on V's controller):
- each body's PhysX pose is read, and compared with world − origin;
- one that's off is posed again (`setGlobalPose`).

That works whether the game shifts the whole scene or moves only its own actors, and the status records which it was.

### Kept out of the export

The exporter skips our actors by their pointers, so Minecraft never gets its own blocks back as city shape.

### The audit

Offline, as spike 1's audit was done: each new slot is checked against the game's DLLs, disassembled. The results go
into `docs/spikes/2026-10-01-physx-abi-audit.md` as an addendum.
- **Before step 3,** the slots the scene watch hooks, on `PxScene`: `addActor`, both `addActors`, `removeActor`,
  `removeActors`, `addAggregate`, `removeAggregate`, `simulate`, `collide`, `advance`, `fetchResults`,
  `fetchResultsStart`, `fetchResultsFinish`, `lockWrite`, `unlockWrite` and `shiftOrigin`. NVIDIA's header puts them at
  slots 10 to 16, 56 to 58, 61, 62, 64, 107, 108 and 116 of 118. MSVC may order overloads differently, so each is
  confirmed in the disassembly.
- **Before step 4,** the calls that make and manage bodies:
  - `PxPhysics::createRigidStatic` and `createShape`;
  - `PxRigidActor::attachShape` and `setGlobalPose`;
  - `PxShape::setQueryFilterData`, `setSimulationFilterData` and `getMaterials`, each one slot later than in NVIDIA's
    headers;
  - `PxActor::getScene`, and `PxBase::release` for actors and shapes. `addActor` and `removeActor` are already done
    for the watch.
- **Already audited:** the probe's own calls, listed as such: `PxScene::overlap`, `raycast`, `lockRead` and
  `unlockRead`, the actors' `getGlobalPose`, `getNbShapes` and `getShapes`, and the shapes' getters (their filter data
  and flags among them).

### Threads

Step 3's scene watch decides where the adds, removes and poses run, as it does for the exporter's reads:
- the starting point is `OnUpdate` on the main thread;
- they move if the watch finds the game writing to scene 0 without its write lock while `OnUpdate` runs. If the game
  does take the write lock, our writes take it too.

### Loads

When V is lost (a load):
- the bodies are removed from scene 0 if PhysX still lists that scene;
- otherwise they're forgotten without being touched.

Step 4 loads a save with bodies present.

## The steps

Each step ends with what the user saw, and the agent's reading of the status, the logs and the measurements. Each
in-game step also checks that CET's overlay and spike 4's build mode still work.

### Step 1: `mclink`, without the game

The user starts Minecraft, and the agent runs `mclink` scripts. Checks:
- **The release:** with the floor and its voxels, `teleportAck` catches up within a second of the teleport.
- **Moving,** with keys held:
  - W walks; Left Shift sprints;
  - Space jumps onto a block placed by a command;
  - a wall of blocks stops the player;
  - Left Ctrl sneaks, and `McState`'s eye height drops to 1.27;
  - Space twice flies; Space and Left Ctrl move it up and down.

  `McState`'s flags and ticks record each.
- **Clicks while walking:** 20 right clicks with W held, along a row. None are refused: 20 blocks, and no red message
  in the HUD image.
- **Entering and leaving:** `mclink`'s script plays the game side's drive: build mode on (one teleport, waiting,
  driving), then off (teleports resume). The moving and clicking checks above run with build mode on, so `SkyState`'s
  menu flag is clear and held keys stay held.
- **The interpolator** runs on `mclink`'s own 60 Hz frames, as the game will run it: the render delay it settles on,
  and the frames that waited for a late tick.
- **The clock-started `teleportSeq`:** a second `mclink` run, with Minecraft still running, teleports Minecraft's
  player on its first link.

### Step 2: in the game, on the floor

Part 1 in BlockRender 0.11.0, deployed. At spike 3's corner, with `ground floor`:
- **The opening check:**
  - F8, then the drive line says driving within a second;
  - standing still in Minecraft, the user turns the mouse and looks up and down. Does the view move as it does outside
    build mode? If not, `set turn 1`, and again.
- **Moving:** walking, strafing, backing up, sprinting, jumping, sneaking (the view drops to Minecraft's eye), and
  flying up and down. The user judges how smooth it is. The status and a frame log record V's camera against
  Minecraft's eye, the render delay and late ticks.
- **Entering with W held:** V stays where it is until Minecraft drives, then walks as Minecraft's player once W is
  pressed again.
- **Clicks while moving:** a row of blocks placed while walking. None are refused.
- **Fall damage while placed:** fly up 5 m and drop, then 20 m. Minecraft's creative player takes none; the status
  shows V's health. A save first, in case V dies.
- **Leaving:** F8 standing (V stays), F8 while flying a few metres up (V falls, with the game's fall damage), Esc, and
  entering again.
- **A vehicle:** with build mode on, the user gets into a car with F, and build mode ends.
- **Cost:** frame times while driving and not, and the teleport call's time.

### Step 3: Night City's shape

- **Offline first:** `mclink` with the dumps from spot B and the ramp. Minecraft's player walks the sidewalk, the
  kerbs, the walkway and the ramp, and stops at walls. TNT lands on the street, on its voxels.
- **In the game, the opening check:** with `ground floor`, so the exporter isn't running yet, `scenewatch on` for a
  minute of walking and driving, across an origin line. It shows which threads write to scene 0, and when, whether the
  game takes the write lock, and whether it calls `shiftOrigin` at the line. That decides where the exporter reads
  (see "The scene watch").
- **In the game,** with `ground city`, at the corner, then at spot B and the ramp:
  - the release when build mode starts;
  - walking streets, kerbs, stairs and ramps, and into walls. Where Minecraft's player and V disagree, `shapes` logs
    the shapes there with their filters;
  - entering build mode under something less than 2.5 m above V's feet, an awning or a stair, say. Minecraft's
    `liftOutOfGeometry` may put its player, and so V, on top of it;
  - a block placed against the street or a wall lands on it, since clicks now hit the city's triangles;
  - sprint-flying across the city, then dropping: the regions keep ahead, and the player lands on the street;
  - TNT on the street, with no stone under it;
  - the cost: reading time a frame (average and worst), the worker's time, and ring bytes.

### Step 4: V against blocks

- **The opening check:** `box on`, with a save made just before, one interaction at a time:
  - walk into the box, stand on it, jump onto it, shoot it, and drive into it;
  - with the box placed just short of an origin line (the probe's overlay shows the distance), cross the line and
    come back. The box still stops V;
  - load a save with the box present.
- **Then `set blocks 1`:**
  - build a wall and a tower in build mode, and leave build mode on top, standing and then sneaking. V stands on them;
  - outside build mode: walk into the wall, jump onto blocks, stand by while blocks are placed and broken, and TNT a
    block under V;
  - record what an NPC, a car and bullets do.
- **Cost:** bodies and boxes, rebuild times, and frame times.

### Step 5: the results

A results doc in `docs/spikes/`, then `superpowers:finishing-a-development-branch`.

## Requests

As in spike 4: one command per file in `blockrender.request`, written atomically and checked once a second, from the
plugin's folder or the overlay's. The new commands:

| Command | Does |
|---|---|
| `set ground floor\|city` | What Minecraft gets as the game's shape: the flat floor, or Night City's collision |
| `set exportbudget <ms>` | The exporter's main-thread budget a frame (1.5) |
| `set turn 0\|1` | Adds the frame's mouse turn to the teleport's yaw (0) |
| `set blocks 0\|1` | Bodies for Minecraft's blocks around V (0) |
| `shapes` | Logs the static shapes within 2 m of Minecraft's player (or V, outside build mode): geometry, actor, filters |
| `box on\|off` | Step 4's test box, 3 m ahead of V, made as the bodies are |
| `scenewatch on\|off` | Step 3's watch of scene 0: which threads add, remove, step, fetch, lock and shift it, and when |

The overlay gains buttons for the ground, blocks, the box and `shapes`, and the drive, ground and bodies lines.

## Measurements and the status

- **Drive:** see "Part 1", "Status".
- **Ground:**
  - the setting, and the regions known around the centre;
  - reading time a frame (average and worst), and batches a second;
  - triangles and voxel blocks a second;
  - the worker's time a region, and the batches waiting;
  - ring bytes a second, and messages waiting for room;
  - how long Minecraft held its player when build mode last started.
- **Bodies:**
  - on or off;
  - sections with bodies, and their boxes;
  - adds, removes and rebuilds a second, and the worst rebuild's time;
  - the copied filters and material;
  - origin changes, and whether the game moved our bodies or we posed them again.
- **Scene watch,** while on: each thread's calls to add, remove, step, fetch, lock and shift scene 0, which of them
  came while `OnUpdate` was running, and each `shiftOrigin`'s shift.
- **Frame rates:** with the drive, the exporter and the bodies each on and off.

## Safety

- **V is placed only while Minecraft drives,** which only starts from F8 or `build on`. FreeFly (F7) must stay off
  meanwhile, since it teleports V too.
- **No faults.** As in spikes 1, 3 and 4, the plugin must never fault, since the game's crash handler freezes the game
  on any fault:
  - every PhysX object is checked against the audited vtables before any call, and the DLLs' timestamps and sizes
    against the audited build. A failed check keeps parts 2 and 3 off, and the status says why;
  - every array is checked for readability before it's read, as spike 1's reader does;
  - our own actors are kept by pointer, and checked before each use;
  - a teleport's target is checked (finite, within 20 m of V) before the call.
- **Shared memory is untrusted:** `kRenSolids` is checked like every render message, and `McState`'s ticks must be
  finite, with a `tickMs` from 1 to 1,000, before the interpolator takes them.
- **The rings:** the collision ring is never written past its tail. A message that doesn't fit waits for the next
  frame, and the render ring is still drained every frame.
- **Off by default:** nothing changes until the user links or presses F8. The exporter runs only while linked, with
  Minecraft there and `ground city`. Bodies stay off until `set blocks 1`, and the scene hooks until `scenewatch on`.
- **The hooks** only record and call on: they run on the game's own threads, so they take no lock and allocate
  nothing, and write into a fixed buffer.
- **At unload:** the hooks are put back, and the bodies are left alone, since the scene may already be going.
- **Builds:** a build that deploys runs only when `tasklist` shows no `Cyberpunk2077.exe`.

## Error handling

- **`Teleport` isn't found, or the call fails:** build mode turns off and says why, so Minecraft never walks away from
  a V that can't follow.
- **A bad target:** the frame is skipped and counted. A second of them ends build mode, and says why.
- **Minecraft never lets go:** the status shows the wait, and F8 leaves.
- **PhysX checks fail:** the ground falls back to `floor`, bodies stay off, and the status says why.
- **A shape or mesh fails its checks while being read:** it's skipped and counted, as in spike 1.
- **The collision ring is full:** messages wait for the next frame, and the count shows.
- **A bad `kRenSolids`:** dropped and counted, like other rejected render messages.
- **Creating an actor or a shape returns nothing:** counted, and that section is tried again when its solids next
  change.
- **Scene 0 is gone:** bodies are forgotten, without being touched.

## Testing

### Unit tests (ctest, no game)

- **Part 1:**
  - the interpolator, with SkyCraft's cases: the rhythm, late ticks, two ticks in a frame, a missed tick, a restarted
    Minecraft, and the delay growing and shrinking;
  - V's eye height: averaged only over frames where V's feet don't move up or down, and the fallback;
  - V's target, from Minecraft's eye and V's eye height, the 20 m check, and a second of bad targets ending build
    mode;
  - the drive: entering, waiting with V held where it is, driving, waiting again when `teleportAck` falls behind, and
    each way of leaving;
  - `teleportSeq` starting from the clock; no teleports or new floor epochs while driving; and `SkyState`'s position
    at V's feet every frame in build mode;
  - the new keys, the crouch keys hidden, and Shift+F6 with Left Shift forwarded;
  - the floor's voxel messages, byte for byte.
- **Part 2:**
  - the voxelizer, with flat, sloped and vertical triangles;
  - filing a batch's triangles into regions, and the margin;
  - nearest-first order, the budget, and sending again only on a changed hash;
  - the messages' layout, `ColBlock`'s bit order included;
  - PhysX to world to Minecraft coordinates, with an origin;
  - which shapes are kept: static scene-query shapes, not simulation-only ones, triggers, or our own bodies;
  - the exporter idle without Minecraft;
  - the scene watch's summary of recorded calls: by thread, and inside `OnUpdate` or not;
  - the dump loader, on a small OBJ.
- **Part 3:**
  - solids from `kRenSolids`, and their checks;
  - the box merge: the boxes cover exactly the solid blocks, with no overlaps;
  - which sections get bodies around V, and the two-section hysteresis;
  - bodies only while `blocks` and drawing are both on;
  - a box from Minecraft's axes to a world box shape;
  - posing again after an origin change.

### `mclink`

Steps 1 and 3's offline half, with the user's Minecraft.

### In the game

Steps 2 to 4.

## Risks

1. **The per-frame teleport fights the look.** `set turn 1` adds the turn back. If it judders, the frame log shows
   where in the frame the camera is taken. If teleporting can't be made smooth at all, the spike reports it, with what
   it found.
2. **V's own movement states while placed:** falling between teleports, landing, fall damage. `NoMovement` is the
   fallback, and health set every frame the last resort.
3. **A teleport a frame costs too much.** The status times it, and step 2 measures the frame rate with it.
4. **The city's statics aren't what stops V.** `shapes` records the filters where Minecraft's player and V disagree,
   and those filters are left out.
5. **The exporter costs too much, or reads regions before their collision has streamed in.** The budget, the settle
   time, and the refresh.
6. **Bodies fault the game.** If the game's own code expects a game object behind whatever it hits, the empty
   `userData` faults, and the game freezes. Hence step 4's box first, with a save made just before, one interaction at
   a time.
   The fallbacks:
   - leave out the filter bits behind the fault (bullets', say);
   - if statics don't stop V at all, SkyCraft's push-out, which nudges V out of blocks sideways. It can't hold V up
     on top, so that would be a partial answer.
7. **The exporter's reads, or the bodies' writes, race the game's threads.** Step 3's scene watch picks where they
   run, before the exporter reads every frame.
8. **A load with bodies in the scene.** Step 4 loads one, and "Loads" says what the plugin does.
9. **V's controller starts inside a block:** after leaving build mode while sneaking, or when a block appears where V
   stands. A PhysX controller's overlap recovery, if the game left it on, pushes it out of static objects; step 4 shows
   what happens. Leaving build mode standing avoids the first.
10. **Minecraft's player now moves between teleports outside build mode.** Once regions are known, Minecraft lets its
    player go after each teleport, so its eye can leave V's camera, for example falling to the street while V stands
    on a car. That's harmless: outside build mode nothing is forwarded.
11. **Entering build mode under a low overhang lifts V onto it.** Minecraft's `liftOutOfGeometry` takes any surface up
    to 2.5 blocks above where its player is held, so under an awning, a stair or a low ceiling it puts its player, and
    then V, on top. With SkyCraft's mod unchanged there's no way round it but entering build mode somewhere else. Step 3
    tries it, to see how often it matters.
12. **Scenes and workspots.** Build mode refuses a V in a workspot or a vehicle, but other scripted moments (a
    dialogue standing up, say) aren't detected, and teleporting V there could upset them. Build mode is for free roam.
13. **Accepted limits:**
    - NPCs don't route around blocks;
    - slabs, stairs and fences are whole cubes for V;
    - sneaking doesn't stop at edges;
    - there's no walk bob, and V slides rather than walks;
    - Minecraft's player walks through cars and NPCs.

## Found while writing

Writing the spec out against the sources added these to what the brainstorm agreed:
- **`teleportSeq` starts from the clock.** Minecraft never resets its last `teleportSeq` when the game side restarts,
  and BlockRender's starts at 0 every run. A restarted game could repeat a value Minecraft has already applied: no
  teleport would happen, and the drive would take Minecraft's stale `teleportAck` for an arrival and place V wherever
  Minecraft's player was. SkyCraft starts its own from `QueryPerformanceCounter` for this reason.
- **Right Ctrl is hidden too.** Cyberpunk crouches on Right Ctrl (held) as well as C (toggled).
- **The user's Minecraft swaps sneak and sprint** (sneak on Left Ctrl, sprint on Left Shift). Forwarding physical keys
  already gives Minecraft its own bindings, so nothing changes.
- **The exporter waits 1.5 s** after the link starts or a load, as SkyCraft's does, so it doesn't read regions before
  the city's collision has streamed in.
- **The floor's voxels come from the city's voxelizer,** so step 2 already tests it.
- **The exporter reads 2 × 2 × 2 regions at a time,** so a large mesh's triangles are culled once for eight regions,
  not eight times.
- **A teleport's target is checked:** a target that isn't finite, or is more than 20 m from V, is skipped.
- **V's health is in the status,** so fall damage while placed shows.
- **Bodies copy their filters and material** from the static shape under V, and wait until there is one.
- **Loads:** bodies are removed if scene 0 is still listed, otherwise forgotten, and step 4 tries a load with bodies
  present.

## Changes from the deep review

The user asked for a deep self-review of the spec. Each claim was traced to its source: SkyCraft's code at `bfcaf17`,
the user's Minecraft 26.3 jar (with `javap`), the protocol header, NVIDIA's PhysX 3.4.2 headers in `extern/`, the
RED4ext SDK, the game's files and FreeFly's Lua, BlockRender's code, and spike 1's probe, audit and dumps. Each step was
then walked through as it would run. The sections above already include these changes.

**Corrected:**
- **V's eye height is measured before build mode, not read each frame.** The draft placed V by "V's camera offset from
  its feet, as `LocatePlayer` read it this frame". A camera and feet read in one frame can straddle a teleport, and
  then the offset is wrong by however far V moved that frame, up to about 0.17 m a frame when flying up. V can't crouch
  in build mode, so the eye height measured while V stands beforehand stays right. Horizontally, V's feet now go
  straight under Minecraft's eye, and the status shows any offset of the camera.
- **V is held where it is while waiting,** instead of not being placed. A movement key already down when build mode
  starts stays the game's, so V would have walked away from Minecraft's held player, then jumped back when driving
  began.
- **`SkyState`'s position is V's feet every frame in build mode.** Minecraft also teleports its player by itself (a new
  player object, or the in-game flag coming back), to wherever the position says. The draft left it at V's feet when
  build mode started, which would have pulled Minecraft's player, and V with it, back there.
- **The drive waits again when `teleportAck` falls behind** while driving (one of Minecraft's own teleports, as
  above), instead of placing V at a player Minecraft is holding.
- **The exporter keeps only scene-query shapes.** A PhysX character controller moves by sweeping against scene-query
  shapes, so a simulation-only shape (about 192 of spike 1's 7,596) doesn't stop V, and the draft's "scene-query or
  simulation" would have given Minecraft walls V walks through.
- **The exporter runs only while Minecraft is there,** as spike 4's floor does. Otherwise its messages would queue in
  a ring nobody reads.
- **The scene watch opens step 3, not step 4.** The exporter reads scene 0 every frame, so which threads write to it,
  and when, matters for part 2's reads as much as for part 3's writes. Its slots are audited before step 3, and the
  calls that make bodies before step 4.
- **The scene watch covers every way the game might write:** batch adds and removes, aggregates, the split simulation
  and fetch, the write lock, and `shiftOrigin`. The draft hooked only single adds and removes, `simulate` and
  `fetchResults`. The write lock says whether our read lock already keeps the game's writes out, and `shiftOrigin`
  settles directly whether the game moves the whole scene when the origin moves.
- **Step 2's falls start low:** 5 m before 20 m, and leaving build mode in flight a few metres up, with a save first.
  The draft's 20 m drops could kill V if the game applies fall damage while V is placed, and leaving at 20 m certainly
  would.
- **A second of bad targets ends build mode,** rather than leaving V frozen while Minecraft walks on.
- **The overhang risk's fix was wrong,** in the first draft of this review: teleporting Minecraft's player higher only
  moves `liftOutOfGeometry`'s 2.5-block window up. With the mod unchanged, the only way round it is entering build
  mode elsewhere.

**Added:**
- **Minecraft 26.3's server corrections,** from the jar: a move into something new, or outside creative a replay more
  than a quarter block off, teleports the player back and refuses clicks until confirmed. SkyCraft's
  `PLAYER_MOVEMENT_CHECK` only removes "moved too quickly". Players skip the game's voxels on the server too, so
  walking on the triangles shouldn't trigger either case, and step 1's clicks check it.
- **`liftOutOfGeometry` lifts onto any surface up to 2.5 blocks above,** an overhang's underside included. Entering
  build mode under an awning, a stair or a low ceiling puts Minecraft's player, and then V, on top. A risk, and a check
  in step 3.
- **Workspots:** build mode needs V outside workspots (sitting, scenes) as well as vehicles. Other scripted moments
  aren't detected, so build mode is for free roam (a risk).
- **The PhysX controller's behaviour,** from NVIDIA's headers: its cache flushes itself when a static shape is added,
  changed or removed, so a new body stops V with nothing else needed; and its overlap recovery pushes it out of static
  objects it starts inside, unless the game turned it off. That's part of risk 9's check.
- **BlockRender's drawing and the teleport:** blocks are drawn with the camera of the frame the game renders, so they
  stay locked to the city whatever the order. Only latency depends on where the teleport lands.
- **The interpolator is fed every frame while linked,** so it has settled when Minecraft starts driving.
- **Step 1 checks the clock-started `teleportSeq`:** a second `mclink` run, with Minecraft still running, teleports its
  player on its first link. Spike 4 never hit the bug, since a new run only collides when the last one ended on the
  same value as the new one's first. And step 1's moving checks run with build mode on, so the menu flag doesn't
  release the held keys.
- **Step 2 enters with W held, and looks up and down** as well as turning.
- **Step 3 places blocks against the street and walls,** which land on them now that clicks hit the city's triangles,
  and **enters build mode under an overhang.**
- **Shift+F6 toggles Minecraft's sprint** on its way out, since Left Shift reaches Minecraft first.
- **Bodies exist only while drawing is on,** so blocks nobody can see never leave invisible walls.
- **The hooks** only record and call on, on the game's threads, and come out at unload. Bodies are left alone at
  unload.
- **The dump loader's categories:** `s0_static_*` holds static actors' simulation shapes, with or without the
  scene-query flag, since the probe files every query-only shape under `s0_queryonly_*`. Nearly all are scene-query
  shapes too.

**Checked, and found as written:**
- **SkyCraft's Minecraft side:**
  - teleports on a new `teleportSeq` or player object, and the parking while not in game (`SkyClient.beginFrame`,
    `holdUntilReady`);
  - the hold's three known regions, its 3 × 3 column 12 blocks deep, and the 6 s fallback;
  - `teleportAck` less 1 while held (`afterRender`);
  - the last `teleportSeq` never reset for a new game side (`SkyLink.poll`);
  - the look while no screen is open;
  - `InputBridge`'s virtual keyboard and repeats, and the menu flag's release every frame;
  - the smooth collider for players on client and server (`SkyCraftClient`, `BlockCollisionsMixin`,
    `EntityCollideMixin`), and `TriCollider`'s walkable limit (`SkyTri.WALKABLE_NY`, 0.7);
  - `PlayerEdgeMixin` and `MinecraftServerFlightMixin`;
  - the collision reader: its 2 ms poll, epochs, regions replacing their box and marking regions known, and
    triangles stored under their box's minimum corner (`SkyCollision`);
  - the ticks (`publishTick`), and the solids (`WorldExporter`).
- **SkyCraft's game side:** the puppet's conditions, the clock-started `teleportSeq` (`Game.cpp`, lines 23 to 29), the
  interpolation's constants (lines 662 to 786, and `tickDue`'s 40 entries), and the exporter's area, refresh, budget
  and settle time (`Collision.cpp`, `Game.cpp`).
- **Minecraft 26.3:** the eye heights (`Avatar`'s pose table), and the user's bindings (`options.txt`).
- **Cyberpunk:** the movement bindings (`inputUserMappings.xml`), and FreeFly's teleport, status effects and health.
- **RED4ext:** `ExecuteFunction` on a class goes through `GetSystem`, whose map is keyed by implementation types;
  `TeleportationFacility`'s script name names its implementation; `CClass::GetFunction` searches parent classes;
  `EulerAngles` is roll, pitch, yaw.
- **PhysX:** the census and region costs; the origin rule, and how spike 1 reads V's controller each frame; the ABI
  shift; the `addActors` note; the read-write lock's semantics; `PxScene`'s 118 slots, of which the probe's audited
  ones include `lockRead` and `unlockRead` (105 and 106).
- **The protocol and BlockRender:** `ColRegion`, `ColBlock` and `RenSolids`; the scan codes and their SDL codes
  (`Scancodes.cpp`); the floor's 7 × 7 regions and its 16 m and 1 m triggers (`Floor.h`, `Link.cpp`); the menu flag
  outside build mode; `teleportSeq` starting at 0; `kRenSolids` stepped over; `BuildMode` noting the Shift keys
  first.
- **Spike 1's dumps:** objects by category, Minecraft's axes relative to V's feet, and spot B's 64,192 static
  triangles.
