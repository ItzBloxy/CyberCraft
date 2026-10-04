# Collision and movement: results

Spike 5 makes Minecraft's player drive V in build mode, gives Minecraft Night City's shape, and stops V against
Minecraft's blocks. Step 1 ran the drive with `mclink`, without Cyberpunk; step 2, further down ("Step 2: in the game,
on the floor"), ran it in the game. Later steps add their sections as they run.

## Part 1: moving as Minecraft's player

Part 1 asked whether Minecraft's player can drive V in build mode: the movement keys go to Minecraft, and every frame V
is teleported so that its camera lands on Minecraft's eye, smoothly enough to play.

**Yes, with the frame's mouse turn added back (`set turn 1`).**
- **It drives:** V walks, sprints, sprint-jumps, sneaks and flies as Minecraft's player does, at Minecraft's own speeds,
  and lands on Minecraft's eye one frame later, within 4 to 7 mm. A steady walk's camera moves with 1.3% frame-to-frame
  jitter. It felt smooth to the user.
- **The look:** a teleport with V's own yaw locks left and right turning, so the plugin adds each frame's mouse turn,
  measured outside build mode, to the teleport's yaw. With that, turning is smooth in every direction.
- **It's safe for V:** falls from up to 47 m did no damage, a car hitting V played V's knockdown and V recovered, and
  build mode ends cleanly every way the spec lists.
- **It's cheap:** the teleport costs 0.002 ms a frame. Build mode costs about 0.3 ms a frame, all of it Minecraft's HUD.
- **No refused clicks:** build mode sends Minecraft no teleports, so clicks while moving all land, which ends spike 4's
  refusals.

What Part 1 leaves for later is in step 2's "What it means for Part 2".

## Step 1: the drive with `mclink`, without Cyberpunk

Date: 2026-10-03. No Cyberpunk: `mclink`, BlockRender's link code in a console program, ran against the user's
Minecraft with SkyCraft's unchanged mod. The versions:
- **Minecraft 26.3**, Fabric Loader 0.19.5, Fabric API 0.161.0+26.3 and Java 25, in the Modrinth App's `CyberCraft`
  instance, with `--enable-native-access=ALL-UNNAMED -Dskycraft.quitWithSkyrim=false`;
- **SkyCraft 0.1.2**, the release jar, protocol version 11;
- **BlockRender's link code and `mclink`** at `3dfcc71`: the plan's Tasks 1 to 7, built RelWithDebInfo, 270 unit tests
  passing.

The user started and stopped Minecraft. The agent ran `mclink` with step 1's scripts, `drive.txt` and `relink.txt`, and
read its output, Minecraft's log and the saved HUD images. Plan:
[2026-10-03-collision-and-movement.md](../superpowers/plans/2026-10-03-collision-and-movement.md), Task 8.

### Verdict

**Step 1: pass.** The drive works against SkyCraft's real mod as written: nothing in the code changed after running it.
Every check passed on its numbers. Two of the plan's expectations were worded too tightly, and both are explained below:
the gold block, which the script jumped onto and off again, and V's feet trailing Minecraft's while moving, which is
the render delay at work.

### The checks

- **Linking:** `Minecraft is there` 0.23 s after `mclink` started, and the floor (epoch 1) at 2.14 s, with its voxels.
  Outside build mode Minecraft let its player go after the link's teleport: by the first `track` line, 5.2 s in, the
  acknowledgement had caught up with the sequence (476360026 of 476360026).
- **Entering:** `build on` sent one teleport (476360027) and a fresh floor at the stand-in V's feet (epoch 2), and the
  drive waited. `Minecraft let its player go after 0.07 s: V follows it`, and the drive was driving.
- **The stand-in V follows:** standing still, V's feet matched Minecraft's to the millimetre in every `track` line. While
  moving, they trail Minecraft's newest position by about 11 to 14 ms of motion, which is the interpolator rendering in
  the past, as designed:

  | Moving | Speed (blocks a second) | V's feet behind Minecraft's |
  |---|---|---|
  | Walking | 4.31 | 4.7 to 5.6 cm |
  | Sprinting | 5.61 | 6.2 to 7.1 cm |
  | Flying up | about 7.4 | 8.8 to 9.4 cm |
  | Falling | faster | up to 12.8 cm |

  The gap closes once the player stops: a quarter of a second after the sprint ended, they matched again.
- **Walking and sprinting:** with W held, 4.31 blocks a second north (Minecraft's walking speed is 4.317); after Left
  Shift toggled sprint, 5.61 (5.612), with `sprinting` in the flags.
- **Sneaking:** with Left Ctrl held, `eye 1.27 up` and `sneaking`, and the stand-in V's feet at 5.790, 0.35 below
  Minecraft's 6.140, so its camera stayed on Minecraft's eye. Releasing it brought the eye back to 1.62 within half a
  second.
- **The wall:** `/fill` put stone bricks 3 blocks north of the player's block, z 1984 to 1985. Walking north stopped at
  z 1985.300, the bricks' south face plus the player's half-width, 1.85 blocks on, and stayed there.
- **The gold block:** at x −117 to −116, the player's block row. With W and Space held for 1.5 s the player jumped onto
  it and off again, so no `track` sample caught it standing at 7.00 as the plan expected. One of its jumps reached at
  least 8.08, though, and only a jump from the block's top gets that high: from the floor at 6.14 a jump peaks at 7.39.
  It then landed on the floor beyond, at x −114.3.
- **Flying:** Space twice gave `flying`. Space held raised the player at about 7.4 blocks a second, Left Ctrl held
  lowered it as fast (the flags read `sneaking, flying`), and Space twice again dropped it back onto the floor,
  `on ground` at 6.140.
- **The interpolator:** the render delay started at 10 ms and, with every tick on time, shrank by 2 ms a second to its
  4 ms floor, which it reached 0.7 s into build mode, and stayed there. No frame waited for a late tick, and no target was skipped. Minecraft ran at 60 to 64
  frames a second, paced to `mclink`.
- **Clicks while moving:** twenty right clicks, 0.45 s apart, while strafing east looking north and down. The `/fill`
  that removed the stone said `Successfully filled 20 block(s)` (`count.png`): all twenty were placed. Minecraft's chat
  showed nothing after the clicks (`clicks.png`): none was refused. Build mode sends no teleports, which is what made
  Minecraft refuse clicks in spike 4.
- **Leaving:** `build off` gave `build mode ended: Minecraft's player follows V again`, the drive went off, and the
  link's teleport went at once (476360028, applied within a quarter of a second). The run made 3 teleports: the link's,
  build mode's, and leaving's.
- **The clock-started `teleportSeq`:** with Minecraft still running, two `relink.txt` runs 10 s apart started from
  1430752721 and 1722695733, and each run's first teleport was applied (…722, then …734). Minecraft's log has
  `Skyrim instance changed (pid …)`, `Skyrim link up` and `teleported to -119.5 6.14 2007.5` for each. Minecraft still
  held the drive run's last value, 476360028, when the first relink started, as the spec found.
- **No `build mode ends:` line,** and `exit: 0` for all three runs.

The render ring carried 31,271 atlas patches (290 MB, about 7.7 MB a second: Minecraft's animated textures), 31
sections and 2,231 HUD images (60 a second), with no resyncs and nothing rejected.

### What it means for step 2

- **Minecraft lets its player go at once:** within 0.07 s of build mode's teleport, on the floor's voxels. The drive's
  waiting state should pass in a frame or two in the game too.
- **V trails Minecraft's newest position by about 11 ms of motion,** the interpolator's render delay. In the game,
  V's camera goes to the interpolated eye, so the status's camera-to-eye distance shows the teleport's own lag on top.
  Step 2's drive log measures both.
- **No refused clicks while moving:** build mode ends spike 4's refusals, since Minecraft's player moves by itself.
- **The script's keys stayed held through the jump,** so step 2's own check of jumping onto a block, where the user
  stops on it, settles what step 1 showed only by the jump's height.

## Step 2: in the game, on the floor

Date: 2026-10-03. BlockRender 0.11.0 at `91b3480` (the plan's Tasks 1 to 16, 289 unit tests passing), deployed to
Cyberpunk 2077 2.31 with RED4ext 1.30.0, on an RTX 3070. Minecraft as in step 1, linked through the mapping. At spike
3's corner (world x −116.6, y −2005.2) with the same save, on spike 4's floor. Part 1 has no `ground` setting yet.

The user played: Minecraft, the game, F8 and the moving. The agent sent requests, recorded drive logs and the status,
and read the plugin's log. Plan: Task 17.

### Verdict

**Step 2: pass, with `set turn 1`.** Every check passed but one, the opening check's first try: with the teleport's yaw
V's own, the view wouldn't turn left or right. The plan's fallback, `set turn 1`, made it smooth. One message differs
from the plan, in a car (below), and a few small things are left for Part 2.

### The checks

- **The script calls:** on linking, the log found all eight, with the types `Puppet` checks for:
  - `PlayerSystem.GetLocalPlayerMainGameObject() -> handle:gameObject`;
  - `TeleportationFacility.Teleport(handle:gameObject, Vector4, EulerAngles)`;
  - V's `GetWorldYaw() -> Float`;
  - `GetMountedVehicle;GameObject(handle:gameObject) -> whandle:vehicleBaseObject`;
  - `WorkspotGameSystem.IsActorInWorkspot(handle:gameObject) -> Bool`;
  - V's `IsDead() -> Bool` and `GetEntityID() -> entEntityID`;
  - `StatPoolsSystem.GetStatPoolValue(gameStatsObjectID, gamedataStatPoolType, Bool) -> Float`.

  V's health read 100% through the stat pools. Nothing faulted in the whole run.
- **The turn ratio:** five or six turns of the mouse, outside build mode, measured −0.0500 degrees a count from 25
  turns. Moving the mouse right lowers the game's yaw.
- **The opening check:** F8, and `Minecraft let its player go after 0.09 s: V follows it`. Then:
  - **with `set turn 0`,** the default, the user could look up and down smoothly but not left or right: the teleport
    puts V's yaw back to where it was before the frame's mouse turn. The pitch is the camera's own, which the teleport
    doesn't touch;
  - **with `set turn 1`,** the frame's mouse counts times the ratio, added to the teleport's yaw, made turning "very
    smooth", in all directions. That settles risk 1's first question: the turn has to be added back.
- **Moving, with a 60 s drive log** (4,311 frames, at about 72 frames a second):
  - the user found walking, strafing, backing up, sprinting, jumping, sneaking and flying all smooth;
  - on a steady walk the target moved at 4.32 m/s with 0.3% frame-to-frame jitter, and V's camera at 4.31 m/s with
    1.3%. Each frame V's camera was within 3.6 to 6.7 mm of where the frame before put it: the teleport lands before
    the next frame's camera is taken, one frame of latency and no judder. That settles risk 1's second question. The
    blocks are drawn with the frame's own camera, so they stay locked to the city;
  - over the whole log, V's camera was a median 6.4 mm from the last frame's target, and 3.9 cm at the 90th
    percentile, which is V's head moving through the frame;
  - the render delay settled at 13.4 to 16.7 ms, against step 1's 4 ms: Minecraft's ticks reach the game later in its
    frame than they reached `mclink`. Only 3 of the 4,311 frames waited for a late tick;
  - the teleport call took a median 0.0014 ms, and at most 0.04 ms;
  - the floor grew around Minecraft's player at its height, never with a new epoch.
- **Hit by a car:** a car drove into V mid-test. V's camera first sat 10 cm high for half a second, then the game
  played V's knockdown: the camera dropped 1.35 m and swung up to 0.86 m sideways over 2 s, and rose back over the next
  2. The teleports kept V's feet under Minecraft's eye throughout, no target was skipped, build mode stayed on, and V's
  health stayed 100%.
- **Sprinting:** another drive log measured a straight sprint at 5.61 m/s, Minecraft's 5.612, and a sprint-jump
  climbing over a second to 7.16 to 7.27 m/s, Minecraft's own sprint-jump speed: the jump's forward boost is applied.
  The user still found sprinting and sprint-jumping slow compared with Minecraft itself. The speeds are Minecraft's, so
  it's the view: Minecraft widens its field of view by about 15% while sprinting and bobs the camera with each step,
  and V's camera does neither. Night City's scale makes the same speed read slower too.
- **Entering with W held:** V stayed where it was until the drive drove, and walked as Minecraft's player once W was
  pressed again: the held W stayed the game's, as spike 4's rule says.
- **Clicks while moving:** about ten right clicks while walking all placed stone, with no red message in Minecraft's
  chat.
- **Falls while placed:** with a save made first, four flights peaked at 18.9, 46.7, 11.9 and 26.5 m above the street
  before dropping back. V's health read 100% in all 90 readings, and V's camera stayed within 9 mm of where the last
  frame put it through every fall and landing, 5 mm on average while falling. V never falls in the game's own terms
  while placed, so risk 2 doesn't happen, and neither fallback is needed.
- **Leaving:**
  - F8 standing: `build mode off: F8 turned it off`, and V stayed where Minecraft's player was;
  - F8 a few metres up: V fell as in the game, from too low for fall damage;
  - Esc: `build mode off: Esc left it`, and the game's menu opened;
  - F8 again: Minecraft let its player go after 0.10 s, and the drive drove.

  After each, the link's teleports resumed from V's camera, up to 10 a second while V fell.
- **A vehicle:** getting into a car with build mode on gave `build mode off: V is in a vehicle`, from
  `GetMountedVehicle`. F8 in the car then kept build mode off, as it should, but said `build mode stays off: V isn't
  found` rather than repeating the vehicle, as the plan expected. In the car the camera isn't V's eye (`PlayerLocator`
  drops a camera more than 5 m from V's feet), so the link finds no V, and "V isn't found" comes before V's own reason.
  While V sat in the car, the eye height measure also learned the seat's camera height, 1.18 m: it averages over frames
  where V's feet stay level, and a car seat is level.
- **The cost,** standing still for half a minute each way:

  | Build mode | Frame time | Frames a second |
  |---|---|---|
  | On | 16.04 ms | 62.3 |
  | Off | 15.81 ms | 63.2 |
  | On again | 16.14 ms | 61.9 |

  The difference, 0.2 to 0.3 ms, is Minecraft's HUD, which shows only in build mode: its copy took 0.17 ms and its pass
  0.12 ms on the GPU. The teleport call took 0.002 ms a frame. Entering build mode gave one long frame each time, 53 and
  56 ms against a usual worst of about 20.
- **Once, right after the first F8,** V's camera stood 7.5 cm from Minecraft's eye, 7 cm of it upward. Later readings
  standing still were within 1 cm, and it didn't come back.

### What it means for Part 2

- **`set turn 1` should be the default:** without it, build mode can't turn.
- **V's reason in a car:** the plugin should ask `Puppet` why V can't be placed even when the link finds no camera, and
  say "V is in a vehicle" rather than "V isn't found". `Puppet` finds V itself, so it doesn't need the camera.
- **The eye height** should be measured only while V can be placed: a car seat, or any workspot, isn't V standing.
  Getting out of a car and pressing F8 within a second would place V by the seat's height until the average caught up.
- **The long frame on entering build mode:** the fresh floor, or the HUD's first frame. Part 2's floor and exporter
  send much more, so its cost needs measuring there anyway.
- **The sense of speed:** a field-of-view kick while Minecraft's player sprints would match Minecraft's own. It's a
  choice for later, not a fault in the drive.
- **The render delay** in the game is 13 to 17 ms, so V's view runs that far behind Minecraft's newest position.

## Step 3, offline: Night City's shape from spike 1's dumps

Date: 2026-10-04. No Cyberpunk: `mclink` sent Minecraft the static shapes of two of spike 1's saved dumps, through
Part 2's own filing, voxelizing and message code, and drove Minecraft's player over them. The versions:
- **Minecraft and SkyCraft** as in step 1: Minecraft 26.3 with Fabric in the `CyberCraft` instance, SkyCraft 0.1.2;
- **BlockRender's link code and `mclink`** at `7a387de`: the plan's Tasks 19 to 27, built RelWithDebInfo, 317 unit tests
  passing;
- **the dumps:** spot B (`2026-10-01_14-37-07`, 64,192 static triangles) at Minecraft 80.5 6.14 2007.5, and the ramp
  (`2026-10-01_14-46-55`, 21,242) 100 blocks farther east, each moved to the stand-in V's feet, east of spike 3's
  corner and clear of the blocks built there.

The user started and stopped Minecraft. The agent ran `mclink` with step 3's script, `city.txt`, and read its output,
Minecraft's log and the dumps' triangles. Plan: Task 28.

### Verdict

**Step 3's offline half: pass.** Minecraft's player walked real Night City geometry as V would: down a kerb, back up it,
into a barrier and up a ramp, and TNT landed on the street. Every check passed on its numbers, and nothing in the code
changed after running it. One expectation was worded loosely: `mclink` logs `explosion at 81.6 6.3 2007.8`, without the
plugin's word "Minecraft". One thing the plan didn't predict: pushed into a wall, the player slides slowly along it,
since the walls here aren't square to Minecraft's axes (below).

### The checks

- **The city, not the floor:** `Minecraft is there` 0.27 s after `mclink` started, and SkyCraft opened its world by
  itself. `the ground is the city now` and `the city starts over, as collision epoch 1` came at 1.87 s, then spot B's
  dump and epoch 2 at 1.95 s. No floor was sent until the script's switch at the ramp, and SkyCraft logged each epoch's
  clear.
- **All of spot B's area sent:** `the city around region 10 0 250 is all in the ring: 1152 regions`, 2.5 s after the
  dump. The ground line then read 145 batches (none unread), 1,152 regions with 107,965 triangles, and 23,888 voxel
  blocks from the worker in 852 ms, 0.74 ms a region; 6.01 MB through the ring, with nothing waiting or dropped.
- **Released at once:** `Minecraft let its player go after 0.02 s: V follows it`, its feet at 6.140, `on ground`.
- **The kerb:** walking west, the feet dropped 0.20, from 6.140 to 5.940, between x 79.6 and 78.5, about 2 blocks out,
  and stayed `on ground` on the road, to x 75.97. In the dump the road is 0.200 below the sidewalk, from about 1.7
  blocks out.
- **The barrier:** walking back east, the feet climbed the kerb at about x 79.0 and stopped at x 84.64, 4.1 blocks east
  of the start, where they stayed.
- **The TNT:** summoned 3 blocks west and 4 up, it went off at 81.6 6.3 2007.8, radius 4.0: on the sidewalk's voxels,
  whose top is the 1/8-block layer above 6.14. Fallen through them, it would have gone off tens of blocks lower.
- **The ramp:** after `ground floor`, the ramp's dump and `ground city`, `the city around region 22 0 250 is all in the
  ring: 1152 regions` came 0.54 s after the dump, and Minecraft let its player go after 0.05 s. Walking north for 4 s,
  the feet rose steadily from 6.145 to 6.470 over 17.3 blocks (+0.325), `on ground` throughout; in the dump the lane
  rises 0.329 over those 17.3 blocks. Walking east, x stayed at 180.62, the ramp's wall.
- **Sliding along walls:** pushed into the barrier with W held, the player slid 0.30 blocks south over 0.8 s, and 0.11
  along the ramp's wall, stopping when W was let go. In the dumps both faces point about 8° off Minecraft's −x, toward
  the south (normals (−0.989, 0, 0.145) and (−0.992, 0, 0.130)), so a push due east glides south along them, as along any
  angled wall. Axis-aligned voxels couldn't turn the push sideways, so SkyCraft collides its player with the triangles
  themselves.
- **No `build mode ends:` line,** and `exit: 0`.

By the end, after the area followed the player 2 regions north, the ground line read 464 batches, 2,496 regions with
146,225 triangles, 40,626 voxel blocks in 1,377 ms (0.55 ms a region) and 8.83 MB through the ring, with nothing
waiting, dropped or unread. Minecraft's log has only the warning it gives on every run as SkyCraft hides its window.

### What it means for the game

- **The ground streams fast enough:** a whole area, 1,152 regions, reached Minecraft within 2.5 s of reading spot B's
  triangles, and Minecraft let its player go within 0.05 s of build mode. In the game the reads are spread over frames
  within the 1.5 ms budget, and the area is read before build mode starts, so it should be ready as here.
- **Kerbs, barriers and ramps behave as Night City's own:** the feet's heights match the dumps' surfaces within 5 mm.
- **Walls not square to Minecraft's axes** make the player slide along them, which in the game V will follow. It's
  ordinary wall sliding, not a fault, but where Minecraft's player and the city disagree in step 3's game half,
  `shapes` will show the faces' filters and bounds.
