# PhysX probe: in-game results

Date: 2026-10-01. Game: Cyberpunk 2077 2.31 (Epic). Probe 0.2.0 (plan Tasks 16 to 21), 0.3.0 (a locator added
during Task 22, see "Coordinates") and 0.4.0 (an origin watch, see "The origin rule, measured"). The user played; the
agent triggered dumps and the watch through `dump.request` and read the reports and logs.

## Verdict

**Pass, with one finding that shapes the collision exporter:**
- **Reading the game's PhysX works.** The probe reads the city's physics world through NVIDIA's 3.4.2 headers,
  corrected by the audit, with no faults and nothing skipped.
- **PhysX coordinates are not world coordinates.** The game keeps PhysX's origin at the 1,024 m grid point nearest V,
  one frame behind V, so a reader must subtract that origin. V's own controller gives it exactly, every frame.
- **All four criteria pass with the origin applied,** at three spots in the city.

## Revision 1

Revision 1's first runs found three things. The audit, [2026-10-01-physx-abi-audit.md](2026-10-01-physx-abi-audit.md),
has the details.
- **The version field:** NVIDIA's 3.4.2 release keeps 3.4.1.0 in the binary FILEVERSION field. Only the
  "FileVersion" string says 3.4.2.0.
- **The `PxShape` slot shift:** the game's `PxShape` has one more virtual function than NVIDIA's headers, so every
  `PxShape` call from slot 6 on ran the next function, several of them setters.
- **The crash handler:** the game's handler sees a fault before any `__try/__except`. Revision 1's guard turned the
  resulting fault into a frozen game.

Revision 2 corrects the header, checks each object's vtable against the audited build before calling anything on it,
and catches nothing.

## Pass criteria

The startup world is the stand-in player's world at the main menu. The others are free roam in Downtown, along one
sidewalk and the raised walkway south of it.

| Criterion | Startup world | Free roam, first try | Spot B | Ramp | Past the line |
|---|---|---|---|---|---|
| Dump | `13-34-03` | `13-41-08` | `14-37-07` | `14-46-55` | `14-50-33` |
| V's feet (world) | (0.0, 3.6, 0.0) | (−191.7, 691.6, 77.5) | (−189.9, 616.8, 74.4) | (−178.1, 550.0, 74.1) | (−197.2, 505.8, 68.2) |
| Origin used | none | none | (0, 1024, 0) | (0, 1024, 0) | none (it is 0 here) |
| 1. Compatibility | ✅ 7/7, 0 skipped, 0 mismatches | ✅ 7/7, 0, 0 | ✅ 7/7, 0, 0 | ✅ 7/7, 0, 0 | ✅ 7/7, 0, 0 |
| 2. Coordinate check | ✅ 1.800 m | ❌ no hit | ✅ 1.800 m | ✅ 1.800 m | ✅ 1.808 m |
| 3. Render matches the screenshot | not checked (menu) | ❌ nothing to render | ✅ | ✅ | ✅ |
| 4. Timings present | ✅ | ✅ | ✅ | ✅ | ✅ |

The first free-roam try failed criteria 2 and 3 because the origin was not yet known. The same thing happens with
every plain dump north of y = 512: `14-34-55`, `14-46-37` and `14-49-29` all found no ground under V.

## Compatibility steps

From the first free-roam dump (`13-41-08`):

1. **Build:** `PhysX3_x64.dll` timestamp 0x64c978b4, image size 0x281000; `PhysX3Common_x64.dll` timestamp
   0x64c978a8, image size 0x1f9000; `PhysX3_x64.dll` FileVersion "3.4.2.0" (binary field 3.4.1.0).
2. **Physics:** `PxPhysics` at 0x208c0181380, an audited `PxPhysics` vtable.
3. **Tolerances:** length 1, speed 10 (PhysX's defaults).
4. **Scenes:** 15.
5. **Gravity and counts:** gravity (0, 0, −9.81) in every scene; 5,781 actors in scene 0 and 5 in each of the others.
6. **Objects:** 2,090 objects checked, including at least one triangle mesh, one heightfield and one convex mesh.
7. **Geometry and meshes:** every checked mesh matches its shape.

Every live object the probe met in 13 city dumps, meshes included, had an audited vtable and the matching type ID.

## Scenes

- **Count:** 15 in the first session, 7 or 8 in the second. Scene 0 is the world. The others are copies of a small
  scene, each holding 5 static planes and nothing else (flags 0x1, active actors only), and their number changes over
  time.
- **Scene 0's flags:** 0x2021d. That's active actors, CCD (without resweep), adaptive force, PCM, and kinematics left
  out of the active actors.
- **The read-write lock:** `eREQUIRE_RW_LOCK` is off in every scene, in the startup world and in the city.

| Scene 0 | `13-41-08` | `14-37-07` |
|---|---|---|
| Static actors | 4,865 | 3,749 |
| Dynamic actors | 916 | 875 |
| Aggregates (actors in them) | 9 (112) | 14 (121) |
| Articulations | 0 | 0 |
| Census time | 7.8 ms | 11.2 ms |

## What's in the collision

Scene 0 at `13-41-08`:
- **By geometry:**

  | Geometry | Shapes |
  |---|---|
  | Box | 3,102 |
  | Triangle mesh | 3,046 |
  | Convex mesh | 814 |
  | Capsule | 467 |
  | Sphere | 161 |
  | Heightfield | 5 |
  | Plane | 1 |
- **By flags:** 7,397 scene-query shapes, 7,270 simulation shapes and 7 triggers, out of 7,596 shapes.
- **Query filters** (`w0:w1:w2:w3`, 31 distinct): 0:0:449c004:0 (3,848), 0:0:405c004:0 (966), 0:0:480000:0 (963),
  0:0:42dc108:0 (342), 0:0:4000:0 (236), 0:0:4498008:0 (184), 0:0:400000:0 (172), 0:0:200000:0 (128),
  0:0:600010:0 (119), 0:0:4680000:0 (110).
- **Simulation filters** (33 distinct): 5c004:0:13ce:0 (3,848), 0:0:0:0 (1,338), 5c004:0:1e48:0 (966),
  1c008:0:5c4b:0 (340), 4000:0:0:0 (235), 1c008:0:13cf:80000000 (184), 10:0:8ce0:80000000 (128), 0:0:144a:0 (110),
  4040:0:2fd:80000000 (85), 18000:0:11fe:80000000 (70).
- **Names:** no actor has a PhysX name. `userData` is set on 5,780 of the 5,781 actors (pointing at game objects).
- **Capsules on dynamic actors** (`14-37-07`): 366, of which 245 are kinematic.

## Region cost

The region is ±32 m around V horizontally, and from 16 m below V's feet to 32 m above.

| | Spot B | Ramp | Past the line |
|---|---|---|---|
| Actors selected by bounds | 504 | 200 | 232 |
| Overlap hits (shapes) | 773 | 301 | 381 |
| Triangles | 76,654 | 23,426 | 21,080 |
| Select / overlap / extract | 0.52 / 0.23 / 6.55 ms | 0.35 / 0.15 / 2.64 ms | 0.51 / 0.17 / 2.05 ms |
| Main-thread pause | 45.0 ms | 39.0 ms | 37.1 ms |

Spot B's triangles by category:

| Category | Triangles |
|---|---|
| Static triangle mesh | 41,618 |
| Static heightfield | 19,785 |
| Kinematic capsule | 3,648 |
| Dynamic convex | 3,120 |
| Dynamic capsule | 2,048 |
| Static convex | 1,713 |
| Dynamic box | 1,428 |
| Kinematic box | 1,068 |
| Static box | 992 |
| Kinematic convex | 902 |
| Query-only capsule | 128 |
| Kinematic triangle mesh | 72 |
| Static capsule | 64 |
| Query-only sphere | 48 |
| Static sphere | 20 |

The census and region work account for 8 to 19 ms of each 30 to 55 ms pause in the city. The report doesn't break
down the rest: the compatibility check's sampling, and the locks and rays of 7 to 15 scenes.

## Coordinates

- **The ray:** where the origin was right, the ray down from 1.8 m above V's feet hit at 1.800, 1.800, 1.800 and
  1.808 m.
- **The origin:** the 0.3.0 locator recorded every capsule on a dynamic actor. Between two dumps 52 m apart, exactly two
  capsules moved as V did, both with an error of 0.000 m and with no axis swap or mirror.
  - **V's controller** is a kinematic capsule with radius 0.32 and half-height 0.4, query filter 0:0:2000:0 and
    simulation filter 1:0:41ce:0. Its centre is exactly 1 m above V's feet.
  - **The other one,** with radius 1.0 and query filter 0:0:400001:0, encloses V. The startup world has it too.
  - **The offset:** in all ten 0.3.0 dumps between y = 520 and y = 685, V's feet minus the controller's centre was
    exactly (0, 1024, −1). At y = 505.8 it was (0, 0, −1).
- **The rule:** PhysX coordinates = world − origin. The dumps showed only one switch, near y = 512; the origin watch
  then measured the rule frame by frame (see "The origin rule, measured").
- **The field of view:** the camera's reported 68.24° is the vertical field of view. Renders with the default
  `--fov-axis v` match the screenshots; `h` is visibly too narrow.

Spot B, the screenshot beside the render, then blended 50/50:

![Spot B: screenshot and render](2026-10-01-physx-probe-results/spot-b.jpg)
![Spot B: overlay](2026-10-01-physx-probe-results/spot-b-overlay.jpg)

The ramp:

![Ramp: screenshot and render](2026-10-01-physx-probe-results/ramp.jpg)
![Ramp: overlay](2026-10-01-physx-probe-results/ramp-overlay.jpg)

Past the line, where a plain dump needs no origin:

![Past the line: screenshot and render](2026-10-01-physx-probe-results/past-the-line.jpg)
![Past the line: overlay](2026-10-01-physx-probe-results/past-the-line-overlay.jpg)

In the renders, grey is static, blue dynamic and orange kinematic. Traffic moved between the spot B screenshot and its
re-check dump, so its car doesn't line up. The dumps are in `E:\Cyberpunk2077\red4ext\plugins\PhysXProbe\dumps\`,
under the names in the tables.

## The origin rule, measured

Probe 0.4.0 added an origin watch (`287d6a2`). While it runs, it reads V's controller every frame, logs each origin
change with V's position in the frames before and after, and writes a status line every 2 seconds. The user walked
back and forth across y = 512, then x = −512, then drove across both and across y = −512. A small Cyber Engine Tweaks
overlay showed position and heading, so the lines could be found
(`spikes/physx-probe/tools/physxprobe_hud/init.lua`; copy its folder into CET's `mods` folder). The watch's log is
[2026-10-01-physx-probe-results/origin-watch.log](2026-10-01-physx-probe-results/origin-watch.log).

| | On foot | Driving |
|---|---|---|
| Frames watched | about 48,000, standing included | about 6,200, getting in and out included |
| Origin changes | 15: y = 512 seven times, x = −512 eight times | 7: x = −512, y = 512, y = −512 |
| V past the line in the last frame with the old origin | 0.001 to 0.150 m | 0.042 to 0.575 m |
| V past the line in the first frame with the new one | 0.055 to 0.302 m | 0.583 to 1.279 m |
| Controller lost | never | never, including getting in and out |
| V's feet minus the controller, across | 0.000 m | 0.000 m; up to 0.041 m while getting in or out |

What that settles:
- **The origin is the 1,024 m grid point nearest V,** with x and y alike. Crossings went from (0, 0, 0) to
  (0, 1024, 0), (−1024, 1024, 0), (−1024, 0, 0) and (−1024, −1024, 0), always switching where V passes 512 + k × 1,024.
  Z stayed at 0, with V between 8 and 78 m up. No place in the city reaches the z = 512 line, so that one is untested.
- **There's no hysteresis.** Each line switches at the same place in both directions.
- **It runs exactly one frame behind V.** In all 22 changes, the first frame with V past the line still had the old
  origin, and the next frame had the new one. A distance threshold can't explain that: V was once 0.15 m past a line
  without a switch, and another switch came at 0.095 m.
- **It never tears.** V's position and the controller agreed to 0.000 m in every frame of every switch, so a reader that
  takes the origin from the controller always gets a consistent frame.
- **Vehicles change nothing.** V's controller stays in the scene and rides along with V, so the origin can be read in a
  car exactly as on foot.

## Surprises and next steps

1. **The origin moves with V,** to the 1,024 m grid point nearest V, one frame late. The exporter has to read PhysX at
   world − origin and know the origin at every export.
   - **Where to get it:** V's own controller, every frame: feet minus controller centre, plus 1 m up, rounded to
     multiples of 1,024 m. That needs no game internals, and works on foot and in vehicles.
   - **What not to do:** computing the origin from V's *current* position is 1,024 m wrong for one frame at every
     crossing. V's position in the previous frame matches the rule too, but the controller is the direct reading.
2. **Character controllers aren't world collision.**
   - **For the exporter:** leave kinematic capsules out. V's own two enclose the camera.
   - **For the renderer:** it now hides them by default (`9d7a558`).
3. **Only scene 0 matters.** The small five-plane scenes come and go.
4. **No threading trouble showed:** 0 skipped objects in 13 city dumps, without the read-write lock.
5. **The city has heightfields.** At spot B, 19,785 of 76,654 triangles were heightfield, from the planted area beside
   the walkway.
6. **Collision is coarse in places.** A tree is one convex shape around its canopy, and litter is dynamic capsules.
7. **A dump pauses the main thread for 30 to 55 ms.** That's fine for a probe, but not every frame. The census of all
   actors (8 to 15 ms) costs more than the region work (under 8 ms). An exporter should walk only the region,
   and spread the work over frames.
8. **The error reporter isn't a crash sign.** `REDEngineErrorReporter.exe` starts with the game, 3 seconds after it,
   so seeing it running doesn't mean the game crashed.
