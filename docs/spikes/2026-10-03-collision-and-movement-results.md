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
  - **Its filters:** the capsules have query filter `0:0:4480008:0` and simulation filter `8:0:13cc:80000000`, the boxes
    `0:0:1c000:0` and `5c000:0:40000:80000000`;
  - **Why it's out:** the exporter keeps only static actors, so it's left out by design, though V's controller collides
    with it in the game. It is most likely breakable, kinematic until something hits it;
  - **Telling props from characters:** V's controller's capsule has query filter `0:0:400001:0` and an NPC-style
    hitbox capsule `0:0:2000:0`, so filters could tell props from characters (risk 4). Spike 1's census of scene 0,
    taken at spot B, counted 85 shapes with the rail's query filter.
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
  - a choice: whether Minecraft gets kinematic props like the rail, kept by their filters, and leaving out characters'
    capsules.
- **Part 3's bodies can be added in `OnUpdate`:** the game's own calls never come then, with the exporter reading or
  not.
