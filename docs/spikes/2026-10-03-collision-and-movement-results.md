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

## Step 3, the opening check: the follow-ups and the scene watch

Date: 2026-10-04. BlockRender 0.12.0 at `f4d5405` (the plan's Tasks 19 to 32, 358 unit tests passing), deployed to
Cyberpunk 2077 2.31 as in step 2, with spike 1's PhysX probe still loaded. Minecraft as in step 1, linked through the
mapping. The ground was the floor throughout: 0.12.0 has no exporter yet.

The user played: Minecraft, the game, F8, the car and the walking and driving. The agent sent `scenewatch on` and
`off`, recorded a drive log, and read the plugin's log and status. Plan: Task 33.

### Verdict

**Pass, and the exporter reads in `OnUpdate`, as planned.** Part 1's follow-ups all work in the game. In 140 s of
watching, 37,770 calls to scene 0 came from 16 threads, and not one came while `OnUpdate` ran; no thread took the
write lock. So `OnUpdate` is a quiet time for scene 0, and Tasks 34 to 36 go ahead as planned.

### The checks

- **The turn by default:** with nothing set, the drive line said `turn on, ratio not measured yet` (1 of 5 turns, from
  the user's first moves). After a few turns it read −0.0500 degrees a count from 32 turns, as in step 2, and in
  build mode V turned left and right with the mouse at once.
- **The frame on entering build mode:** a 10 s drive log (688 frames) around one F8. The frame build mode started in
  took 13.8 ms, like the frames around it (13.5 to 14.1 ms), against step 2's 53 and 56 ms; Minecraft let its player
  go after 0.04 s. The log's worst frame, 19.0 ms, came 5 s after build mode had ended.
- **In a car:** F8 said `build mode stays off: V is in a vehicle`, not "V isn't found". After a short drive and two
  seconds standing, V's eye height still read 1.60 m, not the seat's 1.18. F8 then turned build mode on at once
  (released after 0.04 s): out of the car, V can be placed again.
- **The hooks:** at spot B (world −190.2, 617.6), `scenewatch on` hooked all 16 calls: every slot held the audited
  function, so nothing else had hooked scene 0. The probe patches nothing.
- **The watch,** 140 s: half a minute walking, a drive south across the y = 512 line and back north, then standing.
  Its summary, by thread:

  | Thread | add | remove | step | fetch | shift |
  |---|---|---|---|---|---|
  | 38768 | 634 | 640 | 604 | 579 | |
  | 39168 | 780 | 682 | 607 | 595 | |
  | 36500 | 572 | 676 | 608 | 629 | 1 |
  | 37668 | 674 | 561 | 611 | 594 | |
  | 38036 | 728 | 527 | 597 | 632 | |
  | 37876 | 479 | 475 | 609 | 622 | |
  | 7116 | 527 | 601 | 601 | 606 | |
  | 32192 | 736 | 573 | 624 | 626 | 2 |
  | 36416 | 615 | 648 | 655 | 641 | |
  | 38408 | 703 | 538 | 601 | 594 | |
  | 34212 | 624 | 715 | 620 | 642 | |
  | 32372 | 706 | 820 | 627 | 622 | |
  | 36028 | 648 | 539 | 581 | 622 | |
  | 37720, the main thread | 12 | 4 | 168 | 63 | |
  | 10428 | 623 | 699 | 621 | 652 | |
  | 38264 | 650 | 719 | 585 | 600 | 1 |
  | **All** | **9,711** | **9,417** | **9,319** | **9,319** | **4** |

  - **Who:** 15 worker threads share the work evenly, each adding, removing, stepping and fetching. The main thread
    makes a few of each.
  - **How often:** scene 0 is stepped and fetched about 67 times a second, once a frame. About 70 actors are added
    and 68 removed a second, while walking and driving: the city streaming in and out.
  - **When:** none of the 37,770 calls came while `OnUpdate` ran, for any thread or kind.
  - **The write lock:** never taken. The game doesn't use PhysX's write lock on scene 0, so our read lock keeps out
    nothing; reading is safe because of when it happens, not because of the lock.
  - **The origin:** 4 shifts while V drove across the line and back, each (0, ±1024, 0), the last (0, 1024, 0) going
    north. They came from worker threads, outside `OnUpdate`: the game moves the whole scene with `shiftOrigin`.
- **Unloading:** the plugin's log ends with its patches restored, and the game left no crash report.

### What it means for the exporter, and for Part 3

- **The exporter reads in `OnUpdate`,** under scene 0's read lock as spike 1 did. The game's own calls never
  overlapped it in 140 s and about 9,000 frames. The likely reason is that the game runs its physics jobs in another
  part of its frame from the one RED4ext's update runs in.
- **The watch covered walking and driving only,** not loads or fast travel. The exporter reads only while V is found,
  so loading screens don't read. Step 3's game half keeps the watch on while the exporter reads, to check the same
  holds with `OnUpdate` doing more work.
- **For Part 3's bodies:** the game shifts scene 0 itself when PhysX's origin moves, so PhysX moves our bodies with
  everything else. Part 3 only has to know the origin moved, not pose the bodies again. Adding and removing them in
  `OnUpdate` won't race the game's own adds and removes, which never come then.

## Step 3, in the game: Night City's shape

Date: 2026-10-04. BlockRender 0.13.0 at `dfc7d43` (the plan's Tasks 19 to 35, 364 unit tests), then with one fix at
`b5ac34f` (365 unit tests), deployed as in step 2, the ground the city from loading on. Minecraft as in step 1. At
spike 3's corner, then across the city, at spot B and at the ramp.

The user played: Minecraft, the game, F8, the walking, flying and building. The agent sent requests (`scenewatch`,
`shapes`, `set exportbudget`, `set ground`, `mc`), recorded the status a few times a second, and read the plugin's
log. Plan: Task 36.

### Verdict

**Pass, after one fix.** With the fix, Minecraft gets Night City's shape around V as the game has it:
- the streets, kerbs, ramps and walls hold, to the centimetre of spike 1's dumps;
- clicks place blocks against them, and TNT lands on the street;
- it costs nothing measurable in frame time.

What step 3 found:
- **The fix:** the first reads stalled about 9 ms once a second, in `VirtualQuery`. They no longer do.
- **Kinematic props aren't sent:** a breakable guard rail is a kinematic actor, which the exporter leaves out by
  design, so Minecraft's player walks through it.
- **The worker can fall behind:** in a fast drop into dense ground it queued 1,500 regions, and the player fell through
  the grass once.
- **The turn ratio isn't kept between sessions,** so after loading, build mode can't turn until V has turned 5 times.
- **Risk 11 happens:** entering build mode under an overhang lifts Minecraft's player, and V, on top of it.

### The checks

- **The city around V (step 2 of the task):** the log read `BlockRender 0.13.0 loaded`, `the ground is the city now`,
  `Minecraft is there`, `the city starts over, as collision epoch 1`, and epoch 2 when the link started; no floor. A few
  seconds after the link the ground line read 1,152 regions known around region −15 0 250, PhysX's origin
  (0, −2048, 0), and nothing unreadable. Standing, about 8 batches a second are read again: those next to V.
- **The stall, and its fix:** standing still, one batch read a second took 7.3 to 10.5 ms (0.33 to 0.49 ms a frame on
  average), and the frame's worst rose to 20 to 28 ms against a 15.5 ms average.
  - **Not the cause:** raising the budget to 20 ms changed nothing, since the batches next to V fall due at different
    times.
  - **Where the time went:** the slow batches' own log lines (`71b93fd`) put nearly all of each in `VirtualQuery`: 1 to
    4 calls a batch, 2.8 to 13.4 ms, about 3.5 ms a call, on regions of 582 to 764 MB. The calls were for heightfields'
    sample arrays, which run past the 16 pages `QueryWorkingSetEx` answered. The memory checks start afresh each frame,
    so each frame paid them again. Mesh triangles looked at were only 0 to 4,286 a batch.
  - **The fix (`b5ac34f`):** `QueryWorkingSetEx` now answers any range up to 256 MB, 256 pages a call, at a cost per
    page, and `VirtualQuery` only pages outside the working set. A new test checks a 300-page range takes no
    `VirtualQuery` and that a no-access page in it still fails.
  - **After it:** reads took 0.05 ms a frame on average and 1.4 to 2.2 ms at worst, the frame's worst 17 to 20 ms, and
    no batch reached 3 ms, the first area's included.
- **Building on the city:** F8 released Minecraft's player in 0.05 to 0.06 s. Walking the street, kerbs, stairs and
  ramps near the corner, V stayed on the visible ground and stopped at walls.
- **The guard rail:** V and Minecraft's player walked through a guard rail beside a path. `shapes` there:
  - **What it is:** a chain of kinematic actors, each a 2.9 m section of capsules and boxes, standing 1.1 m on the path
    (world z 6.05 to 7.14), simulation and scene-query shapes;
  - **Its filters:** most of its shapes have query filter `0:0:4480008:0` and simulation filter `8:0:13cc:80000000`;
    one box in each section has `0:0:1c000:0` and `5c000:0:40000:80000000`;
  - **Why it's out:** the exporter keeps only static actors, so it's left out by design, though in the game it's a
    solid simulation and scene-query shape. Whether V's own controller stops at it wasn't checked. It is most likely
    breakable, kinematic until something hits it;
  - **Telling props from characters:** V's controller, the radius 0.32 capsule, has query filter `0:0:2000:0` and
    simulation filter `1:0:41ce:0`, as spike 1 found. A second kinematic capsule on V, radius 1, has `0:0:400001:0` and
    no simulation filter. Both differ from the rail's, so filters could tell props from characters (risk 4). Spike 1's
    census of scene 0, taken at spot B, counted 85 shapes with the rail's query filter.
- **The scene watch while the exporter read:** 354 s of walking, 16 threads, 3,058 adds, 3,067 removes and 24,814 steps
  and fetches; none came while `OnUpdate` ran, and no thread took the write lock. `OnUpdate` stays quiet with the
  exporter reading in it.
- **Under an overhang:** leaving build mode under a structure and pressing F8 put Minecraft's player, and so V, on top
  of it: V's feet went to Minecraft y 13.3, where the streets nearby are at about 6.6. Minecraft let go after 0.06 s.
  Risk 11 happens, as the spec expected.
- **Blocks against the city:** stone placed against the street and against a wall both landed where the crosshair was.
- **Flying ahead of the regions:** over several minutes of sprint-flying up to y 230, the batches to read stayed at 0,
  but for 16 to 31 for a moment in steep drops. Dropping onto a street, the player landed on it.
  - **The fall-through:** dropping about 150 blocks in 3 s onto grass at world z −11.03, the player fell through it, to
    y −60. Every batch had been read (none waiting), but the worker had 857 and then 1,516 regions
    queued, at 3.4 to 15 ms a region, against 0.74 offline at spot B: one thread voxelizing the drop's dense ground
    took seconds.
  - **After the queue drained,** later drops landed on the grass. The grass there is static, and kept: flat meshes,
    boxes and the terrain's heightfield, filter `0:0:449c080:0`. With V under the game's own floor, the game put V
    back up by itself.
- **TNT on the street:** summoned 2 blocks east and 4 up, it went off at Minecraft −208.8 4.6 2175.9, with the
  player's feet at 4.55: on the street's voxels.
- **The cost,** standing in build mode, 10 s each:

  | Ground | Frame time | The second's worst frame, averaged | Reading a frame | Worst read |
  |---|---|---|---|---|
  | City | 15.87 ms | 21.9 ms | 0.036 ms | 1.22 ms |
  | Floor | 16.16 ms | 22.5 ms | none | none |
  | City again | 15.92 ms | 23.3 ms | 0.037 ms | 1.30 ms |

  Switching back to the city started it over (epoch 4), and the whole area was read again within 3 s.
- **Spot B and the ramp:** walked as `city.txt` walked them offline.
  - **Spot B:** the sidewalk at world z 74.43 (the dump's V at 74.432), and the road 0.20 lower beyond the kerb (74.23),
    as offline. V stepped down onto it and back up, and stopped at the barrier.
  - **The ramp:** walking north, the lane rose from 74.08 to 74.43 over 16.7 m, against offline's 0.325 over 17.3.
    Walking east at the dump's spot, V stopped at x −177.68.
- **The turn after loading:** after each restart of the game, build mode couldn't turn until the turn ratio had been
  measured again from 5 turns outside build mode. The plugin measures it each session and forgets it.
- **Quitting:** the plugin's log ended with its patches restored, and the game left no crash report in any of the
  session's four runs.

### What it means for Part 3, and the follow-ups

- **The exporter's shape is right where it reads:** heights match the dumps within a centimetre, and what's static is
  all there. What it misses is by design (kinematic props) or by speed (the worker on fast drops).
- **Before Part 3, the follow-ups:**
  - remember the turn ratio between sessions, so build mode turns at once after loading;
  - the worker: more threads, and the regions nearest Minecraft's player voxelized first, so a fast drop can't
    outrun it;
  - kinematic props like the rail: deferred by the user. Moving props may suit Minecraft entities better, as boats are
    solid to stand on, but that's beyond this spike and needs its own brainstorm.
- **Part 3's bodies can be added in `OnUpdate`:** the game's own calls never come then, with the exporter reading or
  not.

## Part 3, offline: the worker and the solids

Date: 2026-10-04. No Cyberpunk: `mclink` ran step 3's `city.txt` again with the new worker, and Part 3's `solids.txt`.
The versions:
- **Minecraft and SkyCraft** as in step 1: Minecraft 26.3 with Fabric in the `CyberCraft` instance, SkyCraft 0.1.2;
- **BlockRender's link code and `mclink`** at `7e86cad`: the plan's Tasks 38 to 42, built RelWithDebInfo, 376 unit tests
  passing;
- **the machine:** 16 logical processors, so the worker ran 4 threads.

The user started and stopped Minecraft. The agent ran `mclink` with both scripts, and read their output, Minecraft's
log, Task 28's output and SkyCraft's classes. Plan: Task 43.

### Verdict

**Pass, but for the TNT, which SkyCraft's digging explains.** The new worker gives Minecraft the same city as Task 28's
single thread, to the millimetre, and the link keeps Minecraft's solid blocks by section as SkyCraft sends them. The TNT
went off 3.8 blocks higher than in Task 28, on blocks SkyCraft had made from Task 28's own blast, not on the sidewalk.

### The checks

- **The city, as Task 28 found it:**
  - **Spot B's area:** `the city around region 10 0 250 is all in the ring: 1152 regions` 2.52 s after the dump (Task
    28: 2.5 s). Minecraft let its player go after 0.05 s (0.02), its feet at 6.140, `on ground`.
  - **The kerb:** walking west, the feet dropped 0.20, from 6.140 to 5.940, between x 79.6 and 78.6, and stayed `on
    ground` on the road, to x 75.76 (75.97).
  - **The barrier:** walking back east, the feet climbed the kerb between x 77.7 and 78.8 and stopped at x 84.635
    (84.64), sliding 0.28 blocks south along it (0.30).
  - **The ramp:** all in the ring 0.40 s after its dump (0.54 s), and let go after 0.05 s. Walking north, the feet rose
    from 6.145 to 6.471 over 17.5 blocks, +0.326 (+0.325 over 17.3). Walking east, x stayed at 180.61 (180.62).
  - **No `build mode ends:` line,** and `exit: 0`.
- **The TNT: on SkyCraft's blocks.** Summoned 3 blocks west and 4 up, at 81.6 10.14 2007.8, it went off at 81.6 10.1
  2007.8, radius 4.0, where Task 28's fell to 6.3. It hardly fell: it came to rest on a block's top at y 10.0, since
  Minecraft puts a TNT's explosion 0.06 above its feet.
  - **What it rested on:** as the world opened, SkyCraft sent sections 4 0 125 and 5 0 125, blocks from y 8 to 11 above
    spot B, at x 77 to 85 and z 2003 to 2012: 35 solid blocks, by the new solids count. In Task 28's output those two
    sections first came 10 ms after its own TNT went off; in the 13 s the player had stood there before, none had.
  - **Why they're there:** SkyCraft digs into Skyrim's ground. By the strings in its classes (nothing was decompiled),
    its explosion mixin hands each blast to `SkyDigBlast`, which marks cells of Skyrim's ground as dug (`markDug`), with
    buildings resisting (`BUILDING_RESISTANCE`), and sets real blocks in the world (`materialize`, `blockFor`,
    `setBlock`). SkyCraft's world, its "mirror world", is saved, so what Task 28's blast left stayed. This run's blast
    added 6 blocks: 41 in the two sections afterwards, section 5's now reaching y 12.
  - **Their meshes follow the city's voxels:** section 5's shrank from 6,222 vertices to 1,116 as spot B's voxels
    arrived, and in Task 28's run grew back when the ramp's city replaced spot B's. SkyCraft has its own code for
    drawing a dug hole's walls (`DigWalls`).
  - So `city.txt` can't be run twice in the same world as it stands: each run's TNT digs at spot B, and leaves blocks.
- **The worker:** 4 threads, a quarter of the processors. For spot B's area, 1,152 regions with 107,965 triangles
  became 23,888 voxel blocks, as in Task 28, in 1,092 ms of the threads' time: 0.95 ms a region, against 0.74 ms on one
  thread. Its threads were running 93% of that time, nothing waited, and `0 replaced`. By the end, 2,496 regions with
  146,225 triangles made 40,626 voxel blocks, as in Task 28, in 1,743 ms, 95% of it running.
- **How soon:** spot B's area was all in the ring 2.52 s after the dump, as with one thread. The threads' 1,092 ms is
  about 0.3 s over 4 of them, so here the reads set the pace: `mclink` reads at 60 Hz with the plugin's 1.5 ms budget a
  frame, about a batch a frame for spot B's 145.
- **The solids** (`solids.txt`, the stand-in V at 280.5 6.14 2007.5, in section 17 0 125): the 3 × 3 × 3 sections
  around it held
  - 0 sections with 0 blocks before any block was placed;
  - 1 section with 24 blocks after the stone box, `/fill 280 7 2004 282 8 2007 minecraft:stone`;
  - 26 after the glass, the oak slab and the torch: the glass and the slab count, the torch doesn't;
  - 0 sections with 0 blocks after the air fill: SkyCraft sent `section 17 0 125: removed`.

  Every `render ring:` line said `0 rejected`, the glass's translucent triangles were dropped, 12 each time the section
  came, as the link always drops them, and `exit: 0`. The totals were 41 solid blocks in 2 sections before and after,
  SkyCraft's blocks at spot B, sent at once with the player still at the ramp where `city.txt` left it, and 65 and 67
  with the box.

### What it means for the game, and Part 3

- **The worker makes the same city, faster:** the same voxels as Task 28's single thread, from 4 threads that Minecraft
  hardly kept waiting. Offline the reads were the limit, not the worker; Task 46's fast drop is its real test.
- **The solids are what Part 3's bodies need:** each section's blocks arrive as SkyCraft meshes it, and go when it
  empties.
- **Explosions make blocks:** where a blast digs into the city's ground, SkyCraft sets real blocks, kept in its saved
  world. They're solid, so with Part 3's bodies they'll stop V too, and Task 52's TNT step should expect them.

## Part 3, in the game: the follow-ups and the query watch

Date: 2026-10-04. BlockRender 0.14.0 at `247ff14` (the plan's Tasks 38 to 45, 378 unit tests), deployed as in step 2,
on game 2.3.1. Minecraft as in step 1. At spike 3's corner, over the city for the drops, and on an open street.

The user played: Minecraft, the game, the turns, F8, the flying, walking, driving, shooting and building. The agent sent
`scenewatch on` and `off`, recorded the status twice a second during the drops and the solids, and read the plugin's
log and `turn.txt`. Plan: Task 46.

### Verdict

**Pass.** Part 2's follow-ups work in the game, and the query watch clears the way for Part 3's bodies:
- build mode turns at once after a game start, with the last session's ratio;
- four fast drops all landed, the worker's queue draining within two seconds;
- a region costs what its triangles cost, with the worker's threads running 91 to 98% of the time: Part 2's slow
  regions were dense ones, not starved ones;
- no other thread called scene 0 while `OnUpdate` ran, queries included, so Tasks 47 to 52 add the bodies there as
  planned;
- Minecraft's solid blocks are counted one by one as they're placed and broken.

### The checks

- **The turn, measured and saved:** the log read `BlockRender 0.14.0 loaded` and `the turn: nothing remembered, so build
  mode turns once V has turned 5 times`. After the user turned V a few times, the drive line read `turn on, -0.0500
  degrees a count from 29 turns`, the log `the turn: -0.0500 degrees a count, measured, and saved in turn.txt for the
  next session`, and `turn.txt` its two comment lines and `ratio = -0.050000`.
- **The turn after a game start:** the game restarted, Minecraft left running, the log read `the turn: -0.0500 degrees a
  count, remembered from the last session, until this one has measured its own`. With F8 pressed before any turn, the
  drive line read `turn on, -0.0500 degrees a count, remembered from the last session (0 of 5 turns measured)`, and V
  turned with the mouse at once.
- **The drops** (eye heights, in Minecraft's y; the feet are 1.62 lower): every one landed, and none went below the
  ground.

  | Drop | From | Landed, feet at | Most regions waiting | The worker as it landed |
  |---|---|---|---|---|
  | 1 | 217.9 | −11.03 | 98, none 2 s later | 1.43 ms a region of 149 triangles, then 1.71 of 229, running 93 to 94% |
  | 2 | 227.8 | 10.96 | 50, none a second later | 1.67 ms of 192, 7.41 of 984, 10.99 of 1,437, running 92 to 95% |
  | 3 | 270.3 | 14.52 | 6 | 1.53 ms of 242, then 7.55 of 1,407, running 91 to 93% |
  | 4 | 139.8 | −10.27 | 16, none a second later | 1.72 ms of 208, 6.10 of 1,330, 14.97 of 2,142, running 94 to 98% |

  - **Drop 1** came down about 227 blocks, to the height of the grass Part 2's drop fell through, world z −11.03.
    Flying on afterwards, regions of 1,791 and 2,489 triangles took 14.85 and 19.08 ms.
  - **High up,** the regions are empty: up to 192 waited for a moment, at 0.02 ms each, and were gone within a second.
  - **Nothing was replaced:** `0 replaced` throughout.
- **Why a region costs more in the game:** at 4.6 to 9.6 µs a triangle, mostly 7 to 8, a region costs what its
  triangles cost, as offline, where spot B's cost 7.9 µs a triangle on one thread (Task 28) and 10 on four (Task 43).
  The regions under the drops hold about 1,000 to 2,500 triangles, 10 to 25 times spot B's 94, and the threads were
  running 91 to 98% of the time, so the game's own threads weren't keeping them waiting. Part 2's 3.4 to 15 ms a region
  came from that density, and its fall-through from one thread working through 1,516 such regions in turn.
- **The query watch:** `scenewatch on: 19 of scene 0's calls hooked; from a second on, the status sums them by thread`.
  The user walked for half a minute, drove for half a minute, fired a few shots at a wall and stood still. Then
  `scenewatch off`, and the log's summary, for 132 s:
  - **the main thread:** 4 adds, 5 removes, 256 steps, 56 fetches and 40,991 queries, 1,044 of them in `OnUpdate`:
    about 8 a second, BlockRender's own reads of the city;
  - **15 other threads:** each about 330 to 470 adds, 280 to 510 removes, 555 to 615 steps, 556 to 647 fetches, and
    58,300 to 60,500 queries; 892,876 queries in all, about 6,800 a second;
  - `no other thread called while OnUpdate ran; no shifts`.
- **The decision:** no other thread called scene 0 while `OnUpdate` ran, neither the writes Task 33 watched nor the
  queries, so the bodies are added, removed and posed in `OnUpdate`, as Tasks 47 to 52 plan. PhysX's batched queries
  don't go through the scene's vtable; step 4's box checks what's left.
- **The solids:** before building, `solids=Minecraft's solid blocks: 136 in 11 sections`. Four blocks placed a second or
  two apart took it to 137 in 12 sections, then 138, 139 and 140; broken again, to 139, 138, 137 and 136 in 11
  sections. Each change came with one solids message, within the status's second, and every ring line said `0
  rejected`.
- **Quitting:** both runs' logs ended with their 25 vtable entries restored, without a warning, and the game left no
  crash report.

### What it means for Part 3

- **The follow-ups are done:** the turn is kept between sessions, and the worker keeps up with fast drops into the
  densest ground found so far.
- **Bodies in `OnUpdate`:** the game never calls scene 0 from another thread then, so adding and removing bodies there
  can't race the game's own queries or writes.
- **The solids arrive as blocks change,** one message a section, ready for the bodies.

## Step 4, in the game: V against blocks

Date: 2026-10-04. BlockRender 0.15.0 at `a377189` (the plan's Tasks 47 to 51, 392 unit tests), deployed as in step 2,
on game 2.3.1; then, after a GPU crash in the first run, with the renderer's fix at `ead8772` (396 unit tests) and a
smaller overlay at `87adb6c`, and from the third run with DRED on (`settings.txt`: `dred = 1`). Minecraft as in step 1.
At the corner and around it, with a box test near the x = 512 origin line.

The user played: Minecraft, the game, the saves and loads, F8, the walking, jumping, driving, shooting and building.
The agent sent requests (`mc` among them), recorded the status a few times a second, and read the plugin's log, the
game's crash report and the Windows system log. Plan: Task 52.

### Verdict

**Part 3's question is answered: yes.** With bodies, Minecraft's blocks stop V as the city's walls do:
- V walks into them and stops, stands and jumps on them, and stays on a tower when build mode ends;
- a car stops at them, and so do ordinary bullets; NPCs feel them, but their navigation walks them through or lifts
  them on top;
- the bodies follow the blocks as they're placed and broken, within the second, each made in at most 0.08 ms, and cost
  nothing measurable in frame time;
- PhysX's origin moving never needed a body posed again, and a load lost the box without anything touched.

One GPU crash happened, in the first run, while blocks were changed quickly. A race in BlockRender's renderer, there
since spike 4, could explain it and is fixed, but whether it was the cause isn't settled (below). Nothing crashed in
the two runs after the fix, 571 block changes in one of them.

### The checks

- **Loading (step 2 of the task):** `BlockRender 0.15.0 loaded`, the turn remembered from the last session, and
  `bodies=off: 0 sections with bodies, 0 boxes; 0 made, 0 removed, 0 failed and 0 lost since the start, the slowest
  making 0.00 ms; made of what V stands on, once copied; 0 origin changes, 0 bodies posed again; the test box off`.
- **The box:** `box on: 2 x 2 x 1 m, centred at world 236.95 -2171.95 18.25, made of a static heightField's: query
  filter 0:0:449c080:0, simulation filter 1c080:0:104a:0, flags 0xb`: what V stood on was terrain, with the grass's
  filter of step 3, not the street's. Nobody can see the box; the overlay put its nearest face 1.59 m away, as V faced
  heading 310, across the world's axes.
  - **Walked into,** V stopped 0.40 m from its face: the controller's 0.32 m radius and PhysX's contact gap. Nothing
    faulted.
  - **On it,** V's feet were at 18.74 against its top at 18.75, 1.05 m above the ground; Minecraft's player stayed on
    its own ground, as the box is the game's alone.
  - **Shot,** the bullets went through, but they were ricochet rounds, which don't count; with ordinary rounds, bullets
    stop at blocks (below).
  - **Driven into,** the car stopped at it.
- **The origin:** the box was made again near the x = 512 line, at world 523.51 −1823.51 29.29. By then PhysX's origin
  had changed 3 times with the first box standing, as V drove across the line. Walking across the line and back, then
  into the box, took the count to 7, still with `0 bodies posed again`: the game moved our actor with its scene each
  time, as step 3's watch suggested. V stopped at the box again, and stood on it.
- **A load with the box standing:** nothing faulted, and once V was found again the line read `1 lost` and `the test
  box off`: forgotten, not touched. PhysX's origin was back to (0, −2048, 0) at the corner.
- **Blocks on:** `set blocks 1: Minecraft's blocks around V get bodies`, and at once 2 sections with bodies and 3
  boxes, for earlier builds near the corner.
- **A wall and a tower:** a wall 5 long and 3 high and a tower 5 high took the solids from 135 to 155. Each block
  placed or broken made its section's body again: 4 made became 36, in 3 to 6 boxes over 2 sections, the slowest in
  0.02 ms. Leaving build mode on top, V stood on the tower, feet at 11.00.
- **Leaving build mode sneaking:** V's controller started about 0.33 m inside the top block, and was pushed up onto it.
- **The wall and a step:** the wall stopped V, and V jumped onto a one-block step, feet at 7.00.
- **Blocks placed and broken by command:** the plan's relative coordinates follow Minecraft's player, which moves with
  V, so the agent placed and broke them by their block coordinates instead.
  - **Beside V:** V stood on a block boundary, so two stones went in, at −134 6 2004 and 2005. They stopped V 0.47 m
    short; broken again, V walked through where they were.
  - **At V's feet:** a stone at −111 6 2008 left V's controller still, its feet 0.93 m inside the block, while
    Minecraft pushed its own player out sideways at once. Any movement input then put V on top of the block, feet at
    7.00. Broken again, V was back on the street.
- **TNT under V,** in the third run: a platform 3 by 3 and one high merged into one box. Minecraft's player had dropped
  into the platform's middle block (its eye 0.82 m above the feet it was teleported to), so the TNT went in by its
  coordinates, on the platform beside V. It went off 4 s later at Minecraft −57.5 6.1 2024.5, radius 4.0.
  - **The platform** was gone, and its body with it.
  - **SkyCraft's blocks:** the solids went from 165 to 194, as SkyCraft dug into the street and set about 38 blocks of
    stone, copper ore and coal ore, in the air above the blast, as at spot B in Task 43. Their section's body was made
    again with 8 boxes.
  - **V** dropped about 0.9 m to the street, nothing under V, and walked away freely.
- **NPCs, a car and bullets,** at a wall across a pavement: NPCs walked through it or were lifted on top of it, as
  their navigation wins over collision; a car stopped at it; ordinary bullets stopped at it.
- **The cost,** standing by a build, 10 s each:

  | Bodies | With bodies | Frame time |
  |---|---|---|
  | On | 4 sections, 10 boxes | 15.84 ms |
  | Off | none | 15.87 ms |
  | On again | 3 sections, 8 boxes | 15.86 ms |

  The slowest making since the start was 0.08 ms.
- **What the bodies were made of:** in the first run, terrain's heightfield, where V stood for the box; in the later
  runs, the street's triangle mesh, `query filter 0:0:449c004:0, simulation filter 5c004:0:13ce:0`, as step 3 found it.
- **Quitting:** the runs after the fix ended with their 25 vtable entries restored, no warning, and no crash report.

### The GPU crash

- **When:** in the first run, at step 14's platform. After eight block changes in 2.4 s, the game froze for 11.3 s;
  then BlockRender logged `the D3D12 device was removed (0x887a0006), so drawing is off`, and the game's crash reporter
  `Gpu Crash for unknown reasons!`: `DXGI_ERROR_DEVICE_HUNG`. Windows logged three NVIDIA driver errors (event 153) from
  20:22:18 and a driver reset at 20:22:31.
- **Where:** the game's breadcrumbs had every pass up to `PostGBufferOverrides` finished and `RenderDistantShadows` not
  started. BlockRender inserts its G-buffer list about there, but it isn't in the game's breadcrumbs, so whether it ran
  before or after `PostGBufferOverrides` isn't known.
- **A race that could explain it:** a render thread records our list, which draws each section's buffer, and the fence
  after it is signalled once the game's `ExecuteCommandLists` returns. The main thread's upload, replacing a section in
  between, stamped the old buffer with the fence before our list, so it could be freed while our list still waited on
  the GPU. Rapid block changes make that likelier. `ead8772` keeps such a buffer until the fence after the list
  (`render/Retired`, 4 tests, each broken once), and logs each time it does.
- **Not settled:** in the next run, 571 block changes in about a minute of fast building, with bodies on, the fix never
  had to keep a buffer: the race's window is a submission's length, and may rarely or never open. The crash may have
  been that race, or something else. DRED stays on, so a device removal would log the list that stopped and any page
  fault, with the resource allocated or freed there by name. There was none in the runs after.
- **Not the bodies:** they're PhysX's, on the CPU, and the crash was the GPU's.

### What it means for Part 3, and spike 5

- **Part 3 works:** V walks, stands and jumps on Minecraft's blocks in Night City, cars and bullets stop at them, and
  the bodies follow the blocks at no measurable cost. Spike 5's three parts are answered.
- **What's left:**
  - **the GPU crash's cause,** if it comes back, from DRED's report;
  - **SkyCraft's blocks from blasts:** the user finds blocks in the air above a blast odd, and would rather not have
    them. SkyCraft has no setting for it (`skycraft.properties` holds only `join=`), so it means a small Fabric mixin of
    our own, or a change to SkyCraft;
  - **NPCs and moving props:** NPCs' navigation walks them through blocks, and kinematic props aren't sent; both wait
    for the Entities brainstorm.
