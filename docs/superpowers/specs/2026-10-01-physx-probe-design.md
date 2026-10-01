# PhysX Probe: design (spike 1)

Date: 2026-10-01. Status: revision 2, awaiting review. Revision 1 was approved after a deep review against the
RED4ext SDK and the PhysX 3.4.2 sources. Revision 2 follows the first in-game runs:
- **The ABI audit:** `docs/spikes/2026-10-01-physx-abi-audit.md` found that the game's PhysX differs from NVIDIA's
  public headers in one class.
- **The crash:** a fault guard can't protect the game, because the game's crash handler fires first.

Revision 2.1 (0.3.0) came from the first city dumps, where nothing lay near V in PhysX space. The probe now says where
each scene's actors are and where every capsule on a dynamic actor is, and `dump.request` can give PhysX's origin. That
showed the game re-centres PhysX on a 1,024 m grid: see `docs/spikes/2026-10-01-physx-probe-results.md`.

## Purpose

This is the first of three spikes that de-risk a SkyCraft-style "play Cyberpunk 2077 as a Minecraft player" mod.
The mod's collision exporter plays the part of SkyCraft's `skse/src/Collision.cpp`, and it depends on reading
Cyberpunk's physics world. Cyberpunk ships NVIDIA PhysX 3.4.2 as separate DLLs, and `PhysX3_x64.dll` exports
`PxGetPhysics`. The probe finds out whether that physics world can be read through NVIDIA's 3.4.2 headers,
corrected where the game's build differs.

It answers three questions:

1. **Compatibility:** with the headers corrected by the audit, is the running game the audited PhysX build, and does
   every object the probe meets belong to a class whose functions the audit verified?
2. **Coordinates:** are PhysX scene coordinates the game's world coordinates (metres, Z up), with no origin shift?
3. **Content and cost:** what does the collision around V contain (shape types, collision-filter values,
   triangle counts), and how long does it take to extract?

A "no" to question 1 also counts as a result:
- **Unaudited classes** in the live world would need auditing too.
- **A world that can't be read without racing the game's own threads** would move the collision plan to ray casts
  through the game's script API, as in SkyCraft's own first stage.

## Non-goals

- No Minecraft side, shared memory, streaming, or per-frame export.
- No hooks into game functions, and no writes to game or physics state.
- No automatic dumps. A dump runs only when the user presses F10 or the agent creates the request file.
- No rendering inside the game.
- Only game version 2.31 is supported.

## Environment (checked on 2026-09-30 and 2026-10-01)

- Cyberpunk 2077 2.31 (Epic) at `E:\Cyberpunk2077`, with RED4ext 1.30.0. Mods are managed by Vortex.
- `bin\x64\PhysX3_x64.dll` has the "FileVersion" string 3.4.2.0. Its binary FILEVERSION field says 3.4.1.0, and so
  do those of the other three `PhysX3*` DLLs. NVIDIA's own 3.4.2 release has the same pair (see its `PhysX3.rc`),
  so only the string tells 3.4.1 and 3.4.2 apart. It exports `PxGetPhysics`, `PxRegisterCloth` and
  `PxRegisterParticles`. The last two show that `PX_USE_CLOTH_API` and `PX_USE_PARTICLE_SYSTEM_API` are 1. Those
  are also the header defaults, and they matter because they change `PxScene`'s virtual function table.
- The game's PhysX is a modified 3.4.2. `docs/spikes/2026-10-01-physx-abi-audit.md` compares every class, function
  and struct the probe uses with the game's DLLs.
  - **The mismatch:** `PxShape` has one virtual function more than NVIDIA's public headers, a reference-count getter
    at slot 6, so every later `PxShape` slot is one higher.
  - **Everything else matches** for the calls the probe makes.
  - **Scope:** the audit holds for exactly these builds:

  | DLL | PE timestamp | Image size |
  |---|---|---|
  | `PhysX3_x64.dll` | 0x64c978b4 | 0x281000 |
  | `PhysX3Common_x64.dll` | 0x64c978a8 | 0x1f9000 |

- The game's crash handler sees an access violation before any `__try/__except` in a plugin. It writes a crash
  dump, suspends every other thread and starts `REDEngineErrorReporter`. A plugin that catches the fault and carries
  on freezes the game; a fault that isn't caught ends in an ordinary crash with a report.
- `PlayerSystem.GetLocalPlayerMainGameObject` returns a stand-in player from the startup legal screen on, before any
  save is loaded.
  - **The stand-in:** at about (0, 3.6, 0), facing −Y, with a camera field of view of 60.
  - **The physics world then:** one scene with 8 actors.
  - **No usable "save loaded" signal:** no script function a plugin can reach says whether a save is loaded.
    `IsPreGame` belongs to `inkISystemRequestsHandler`, which UI game controllers hand out. No game system a plugin
    can call does. The executable registers `GetSystemRequestsHandler` among the controllers' methods.
- RED4ext reports the runtime version 2.31 as the SemVer 2.3.1.
- The game's scripts include every function the probe calls:
  - `PlayerSystem.GetLocalPlayerMainGameObject`;
  - `GetWorldPosition` and `GetWorldForward`;
  - `CameraSystem.GetActiveCameraWorldTransform`, `GetActiveCameraForward` and `GetActiveCameraFOV`.
- Toolchain: Visual Studio 2022 (MSVC 14.44) with its bundled CMake, Windows SDK 10.0.26100, Python 3.14 and uv.
- RED4ext.SDK is at master (2026-07-30), where `RED4EXT_V1_RUNTIME_VERSION_LATEST` is 2.31.

## Repository layout

```
D:\Cyberpunk-Minecraft\
  docs/superpowers/specs/2026-10-01-physx-probe-design.md   this file
  docs/spikes/2026-10-01-physx-abi-audit.md                 the audit of the game's PhysX DLLs
  extern/RED4ext.SDK/        git submodule (WopsS/RED4ext.SDK), used header-only
  extern/physx-3.4.2/        NVIDIA PhysX 3.4.2 public headers, BSD-3 licence text kept
                             (PhysX_3.4/Include and PxShared/include only), with one local change
                             to match the game (see Build)
  spikes/physx-probe/
    CMakeLists.txt
    CMakePresets.json
    src/Plugin.cpp           RED4ext entry points and triggers
    src/PlayerLocator.h, .cpp
    src/Probe.h, .cpp        one dump, start to finish
    src/Coords.h
    src/Report.h             the report's plain data
    src/Json.h, .cpp         the small JSON writer
    src/ReportJson.h, .cpp   report.json
    src/ObjWriter.h, .cpp    collision.obj and .mtl
    src/DumpWriter.h, .cpp   the background writer
    src/physx/PxHeaders.h    the PhysX headers in use
    src/physx/Checks.h, .cpp the pure parts of the compatibility check
    src/physx/GameAbi.h, .cpp the audited build: DLL identities and vtable addresses
    src/physx/Readable.h, .cpp readable-memory checks
    src/physx/PhysxSession.h, .cpp
    src/physx/ShapeTriangles.h, .cpp
    tests/                   unit tests (a small test program run by ctest)
    tools/render_dump.py, test_render_dump.py
```

The pieces the component list below describes as part of `DumpWriter` and `PhysxSession` live in their own
small files, each with one job.

`extern/` sits at the repo root so the real plugin can reuse it later.

## Build

- CMake with the "Visual Studio 17 2022" generator on x64, in C++20. `CMakePresets.json` has a configure preset
  named `default`, and build and test presets named `release`.
- The RED4ext SDK is used in header-only mode (`RED4EXT_HEADER_ONLY=ON`).
- The PhysX headers are used without any import library. The probe makes only virtual calls on objects the game
  hands it, plus inline code from the headers.
  - `PX_PHYSX_STATIC_LIB` is defined, which empties the core API macro.
  - The common API macro stays `dllimport`. It marks the methods of `PxHeightField`, `PxTriangleMesh`,
    `PxConvexMesh` and other classes, but the probe only calls those through their virtual tables, which needs no
    import. `PX_PHYSX_COMMON_EXPORTS` must not be defined.
  - The built DLL must not import anything from `PhysX3*.dll`, and `dumpbin /imports` is the check for this.
- `extern/physx-3.4.2/PhysX_3.4/Include/PxShape.h` carries one local change, marked with a comment that cites the
  audit: `virtual PxU32 getReferenceCount() const = 0;` right after `acquireReference()`. It puts every later
  `PxShape` method on the game's slot. Any new PhysX call the probe makes must first be checked against the game's
  DLLs, recorded in the audit, and added to the slot test (see Testing).
- `CP2077_DIR` is a CMake cache variable, set when configuring (for example `-DCP2077_DIR=E:/Cyberpunk2077`) and
  never committed. When it is set, a post-build step copies `PhysXProbe.dll` to
  `<CP2077_DIR>\red4ext\plugins\PhysXProbe\`.

## Components

### `Plugin.cpp`

- Provides the RED4ext entry points:
  - `Query` gives the name `PhysXProbe` and runtime `RED4EXT_V1_RUNTIME_VERSION_LATEST`;
  - `Supports`;
  - `Main`.
- On load, it registers a `GameState` for `EGameStateType::Running`. RED4ext calls its `OnUpdate` every frame on the
  game's main thread, and ignores its return value in the Running state.
- `OnUpdate` checks the triggers and runs at most one dump per frame, with dumps at least 2 seconds apart.
- There are two triggers:
  - **F10:** edge-detected with `GetAsyncKeyState`, and only while the foreground window belongs to the game's process.
  - **Request file:** `dump.request` in the plugin folder, checked once a second and deleted when the dump starts.
    It may be empty, or hold one line `origin <x> <y> <z>`: where PhysX's origin is in world coordinates (metres).
    With an origin, the dump reads PhysX at world − origin for the region box, the ray and the nearest capsule, and
    moves the triangles back into world coordinates. Any other text is logged and ignored.
- There is no automatic trigger. Revision 1 had one, 10 seconds after V was first found, but it fired on the
  stand-in player during the startup legal screen. No signal a plugin can reach would hold it back until a save is
  loaded (see Environment), and a timed dump could also land mid-load, while the world is changing fastest.
- A trigger that fires while the previous dump's files are still being written is ignored, and a log line says so.
- The plugin finds its own folder from its module handle.
- Only the main thread writes log lines, through RED4ext's logger.
- On unload, it waits for the writer thread to finish.

### `PlayerLocator`

- Finds `PlayerSystem` and `CameraSystem` by their script names with `CRTTISystem::GetClassByScriptName`, and
  calls their functions through `RED4ext::ExecuteFunction`. `CClass::GetFunction` searches parent classes, so
  `GetWorldPosition` resolves from V's class up to `Entity`.
- `std::optional<PlayerPose> Locate()` reads two things:
  - **V:** `PlayerSystem.GetLocalPlayerMainGameObject()`, then `GetWorldPosition()` and `GetWorldForward()` on it.
    The main object is always V, whereas the "controlled" object can be a car or a hacked camera.
  - **The camera:** `CameraSystem.GetActiveCameraWorldTransform()` gives the position,
    `GetActiveCameraForward()` the view direction, and `GetActiveCameraFOV()` the field of view.
- It returns nothing when the engine, the game instance, the player system or V isn't available, which happens
  only very early in startup. From the legal screen on, it finds the stand-in player (see Environment), and the probe
  doesn't try to tell the stand-in from V: a dump at the main menu reads the small startup world, which is harmless
  and makes a good first test. The camera part is optional; without it, the render falls back to a point 1.7 m above
  V's feet, looking along V's forward.
- `PlayerPose { Vec3d feet; Vec3f forward; std::optional<CameraPose> camera; }`, with
  `CameraPose { Vec3d position; Vec3f forward; float fov; }`. All of these are in world coordinates (metres, Z up).

### `physx/PhysxSession` (reusable)

This is the only code that touches PhysX. It doesn't catch faults, because in this game catching one freezes the
game (see Environment). It avoids them instead, with three rules:

1. **Check before use:** before it calls anything on a PhysX object, it checks two things:
   - The bytes it reads straight from the object are readable (`Readable`). That's the vtable pointer, plus for
     `PxBase` objects the type ID, and for actors `userData`: at most the first 24 bytes.
   - The vtable pointer is one of the audited tables for the class it expects (`GameAbi`).

   An object that fails either check is skipped and counted, and nothing is ever called on it.
2. **Audited calls only:** it calls only the methods listed in the audit. Because the object's table is known,
   each call reaches the function the audit verified.
3. **Range-check raw memory:** every raw array it reads is checked for readability over its whole length first:
   mesh vertices and indices, convex index buffers, heightfield samples and actor names.

All scene reads during a dump happen between `lockRead` and `unlockRead` on that scene.

- `Open()`:
  1. Finds `PhysX3_x64.dll` and `PhysX3Common_x64.dll` with `GetModuleHandleW`.
  2. Reads each one's PE identity (`GameAbi::ModuleIdentity`) and version resource (both the binary FILEVERSION
     field and the "FileVersion" string).
  3. Calls `GetProcAddress("PxGetPhysics")`.
  4. Runs the compatibility check below.

  Steps 1–3 of the check describe the build and are cached for the session. Steps 4–7 describe the world, so they
  run again on every dump: the first dump may read the startup world and a later one the city.
- `Scenes()` returns the scene pointers.
- `Census(scene)` walks `getActors(eRIGID_STATIC | eRIGID_DYNAMIC)`, each actor's shapes, and the mesh each shape
  references. It returns counts and tallies without reading any vertices:
  - actors by concrete type and by kind, and shapes by geometry type and by flags;
  - tallies of query and simulation filter data, keeping the 64 most common combinations and an "other" count;
  - aggregates (`getNbAggregates`/`getAggregates`), the number of actors inside them, and the number of
    articulations (`getNbArticulations`), since `getActors` never returns articulation links;
  - how many actors have a name and how many have user data, plus up to 32 distinct actor names. Names are copied
    only as far as readable memory goes, cut to 128 characters, and have non-printable bytes replaced.
  - It checks every object it visits as above, and counts the objects it skips: unreadable ones, and ones whose
    vtable isn't an audited table. It also checks that each object's type ID, a plain data member, agrees with its
    audited class, and counts any disagreements as mismatches. After 100 skipped objects it stops reading that scene.
- `SelectByBounds(scene, box)` keeps the actors whose `PxActor::getWorldBounds()` overlaps the region box.
- `QueryBox(scene, box)` runs `PxScene::overlap` with a box geometry and the flags
  `eSTATIC | eDYNAMIC | eNO_BLOCK`. It uses a heap-allocated touch buffer of 8192 hits and reports when that buffer
  overflows. The report compares its result with `SelectByBounds`, because the overlap query only sees scene-query
  shapes.
- `RaycastDown(scene, from, maxDistance)` runs `PxScene::raycast` against static geometry only.

### `physx/GameAbi` (reusable)

- Holds the audited build as data:
  - for `PhysX3_x64.dll` and `PhysX3Common_x64.dll`, the PE timestamp and image size the audit covers;
  - the audited concrete vtables, each as a DLL, an RVA, an audited class and, for `PxBase` objects, the
    `PxConcreteType` ID that class must report:

  | Class | DLL | Vtable RVA | Type ID |
  |---|---|---|---|
  | `NpPhysics` | `PhysX3_x64.dll` | 0x20a310 | none |
  | `NpScene` | `PhysX3_x64.dll` | 0x20caf0 | none |
  | `NpRigidStatic` | `PhysX3_x64.dll` | 0x20b980 | `eRIGID_STATIC` |
  | `NpRigidDynamic` | `PhysX3_x64.dll` | 0x20b2e8 | `eRIGID_DYNAMIC` |
  | `NpAggregate` | `PhysX3_x64.dll` | 0x206e68 | `eAGGREGATE` |
  | `NpShape` | `PhysX3_x64.dll` | 0x20e9a8 | `eSHAPE` |
  | `Gu::ConvexMesh` | `PhysX3Common_x64.dll` | 0x1b5200 | `eCONVEX_MESH` |
  | BV33 triangle mesh | `PhysX3Common_x64.dll` | 0x1b6978 | `eTRIANGLE_MESH_BVH33` |
  | BV34 triangle mesh | `PhysX3Common_x64.dll` | 0x1b6868 | `eTRIANGLE_MESH_BVH34` |
  | `Gu::HeightField` | `PhysX3Common_x64.dll` | 0x1b6b18 | `eHEIGHTFIELD` |

- `ModuleIdentity(module)` reads the timestamp and image size from a loaded module's PE header.
- `AuditedClassOf(vtablePointer, moduleBases)` returns the audited class a vtable pointer belongs to, or nothing. It
  only compares numbers, so it's unit-tested without the game.
- `CheckObject(memory, bases, object, bytes)` checks an object before the probe uses it, and returns a verdict and
  the class:
  - **Readable:** the bytes the caller reads from it are readable (at least 16).
  - **Audited:** its vtable is an audited table.
  - **Typed:** for `PxBase` objects, its type ID agrees with that table's class.
  - **The verdicts:** ok, unreadable, unaudited, or wrong type.
- Each place in the probe that receives an object says which classes it accepts:
  - **the physics object:** `NpPhysics`;
  - **a scene:** `NpScene`;
  - **an actor:** `NpRigidStatic` or `NpRigidDynamic`;
  - **an aggregate:** `NpAggregate`;
  - **a shape:** `NpShape`;
  - **a mesh:** one of the four mesh classes, and one that matches the shape's geometry type: a BV33 or BV34 mesh
    for a triangle mesh, `Gu::ConvexMesh` for a convex mesh, `Gu::HeightField` for a heightfield.

### `physx/Readable` (reusable)

- `ReadableMemory::Range(address, size)` is true when every byte of the range is committed memory whose protection
  allows reading and that isn't a guard page. It asks `VirtualQuery` region by region, so a range may span regions. A
  null or wrapping range is never readable.
- The regions it has seen are cached for one dump. That keeps checking hundreds of thousands of objects cheap, and
  the cache is cleared at the start of every dump.

### `physx/ShapeTriangles` (reusable)

- `AppendTriangles(shape, actorPose, regionBox, out)` adds world-space triangles for one shape. It keeps only the
  triangles whose bounds touch the region box.
- Every shape's local pose is applied first, then its actor's global pose.
- It handles each geometry type as follows:
  - **Triangle mesh:** applies `PxMeshScale` the way PhysX does, as rotation⁻¹ · scale · rotation
    (`PxMeshScale::transform`, which is inline in the header). It handles 16- or 32-bit indices, and winding flips
    when the scale's determinant is negative.
  - **Heightfield:** visits only the cells under the region box. The box's corners are taken into the
    heightfield's local frame, and the covered row and column range is computed from them. Samples are read with
    `PxHeightField::getSample(row, column)`, and sample (r, c) sits at local
    (r × rowScale, height × heightScale, c × columnScale). A cell (r, c) exists for r < rows − 1 and
    c < columns − 1 and holds two triangles. Their vertex order follows PhysX's
    `Gu::HeightField::getTriangleVertexIndices`:
    - **Tess flag set:** the diagonal runs from (r, c) to (r+1, c+1). The triangles are
      [(r+1, c), (r, c), (r+1, c+1)] and [(r, c+1), (r+1, c+1), (r, c)].
    - **Tess flag clear:** the diagonal runs from (r, c+1) to (r+1, c). The triangles are
      [(r, c), (r, c+1), (r+1, c)] and [(r+1, c+1), (r+1, c), (r, c+1)].
    - The first triangle uses the cell sample's `materialIndex0` and the second its `materialIndex1`, in both cases
      without the tess bit. Material 127 (`eHOLE`) removes that triangle.
  - **Convex mesh:** each `PxHullPolygon` (`mNbVerts` indices starting at `mIndexBase` in the 8-bit index buffer) is
    fan-triangulated. Hull polygons face outward, and the mesh scale is applied the same way as for triangle
    meshes.
  - **Box:** 12 triangles.
  - **Sphere:** a UV sphere with 8 segments and 4 rings, which is 48 triangles.
  - **Capsule:** the same sphere split at its equator, with an 8-segment cylinder between the halves, which is 64
    triangles. PhysX capsules lie along the shape's local X axis.
  - **Plane:** skipped and counted.
- The geometry maths lives in plain functions that take plain data, so it can be unit-tested without PhysX objects:
  `BoxTriangles`, `SphereTriangles`, `CapsuleTriangles`, `HeightfieldTriangles`, `ConvexPolygonTriangles`,
  `ApplyMeshScale` and `KeepIfTouches`.

### `Coords.h`

- `McFromWorld(x, y, z)` returns `(x, z, -y)`, and `WorldFromMc(x, y, z)` returns `(x, -z, y)`.
- 1 world unit = 1 block = 1 metre. Positions are doubles.
- The mapping is a proper rotation (determinant +1), so triangle winding is preserved.

### `DumpWriter`

- When the main thread finishes reading PhysX, it hands the collected triangles and report data to a background
  writer thread. That thread formats and writes the files, and never calls PhysX, the game or RED4ext. The main
  thread logs the outcome on a later frame.
- Each dump goes into `dumps\<YYYY-MM-DD_HH-MM-SS>\` in the plugin folder.
- `collision.obj` and `collision.mtl`:
  - Vertices are in Minecraft axes, relative to V's feet, with millimetre precision. Identical vertices are shared
    within each group, which keeps the file small. V's world position is written as a comment.
  - There is one object group per category, and each category has its own colour. A category is
    `<kind>_<geometry>`, for example `static_trimesh`. The kind is picked in this order:
    1. `trigger` if the shape has the trigger flag;
    2. otherwise `queryonly` if it has the scene-query flag but not the simulation flag;
    3. otherwise the actor's kind: `static`, `kinematic` (a rigid dynamic with the kinematic flag) or `dynamic`.
  - With more than one scene, group names are prefixed with the scene index.
  - Output is capped at 1,000,000 triangles per dump, and the report records it if the cap is hit.
- `report.json` is written by a small JSON writer in `DumpWriter`, with no third-party JSON library. It escapes
  strings, and it follows the schema below.

### `tools/render_dump.py`

- It is run with `uv run --with numpy --with pillow tools/render_dump.py <dump folder>`.
- It writes `top.png`, an orthographic top-down view coloured by category.
- It also writes `eye.png`, a perspective view from the camera pose in the report. Without a camera pose, it uses
  a point 1.7 m above V's feet, looking along V's forward.
  - The reported field of view is treated as vertical; `--fov-axis h` treats it as horizontal instead, and `--fov`
    overrides the value.
  - Triangles are clipped against a near plane 0.05 m from the camera, then drawn with a z-buffer and flat shading
    from the face normal. Both views use the z-buffer: a first version that sorted triangles far to near drew floors
    over the bases of walls.
- It skips `trigger` and `queryonly` by default, and `--all` includes them.

## One dump, step by step

1. `PlayerLocator::Locate()`. If there's no player, it logs "no player" and stops.
2. The compatibility check: steps 1–3 on the first dump only, steps 4–7 on every dump (see `Open()`). On a failure
   it writes the report and stops, without reading any geometry.
3. For each scene, under its read lock:
   1. `Census`.
   2. The region work. The region box is centred on V's feet plus (0, 0, 8), with half-extents (32, 32, 24) m,
      which covers ±32 m horizontally and −16 to +32 m vertically. The probe runs `SelectByBounds` and `QueryBox`,
      then `AppendTriangles` for each shape of the bounds-selected actors.
4. The coordinate check.
5. The collected data goes to the writer thread. This ends the main-thread pause, which is recorded.
6. The writer writes the files, and on a later frame the main thread logs a summary line.

The census, region selection, overlap query, triangle extraction and the whole main-thread pause are each timed with
`QueryPerformanceCounter`.

## Compatibility check

The steps run in order and stop at the first failure, which the report names.

Revision 1 checked type names instead. That check passed while, because of the `PxShape` slot shift, the probe was
calling setters. Names can't reveal a shifted vtable; the identity of the build and of each object's vtable can.

1. **The build.** `PhysX3_x64.dll` and `PhysX3Common_x64.dll` are both loaded, and each one's PE timestamp and image
   size match the audited build (see Environment). Both version-resource fields are recorded but not checked.
2. **The physics object.** The `PxGetPhysics` export exists. It returns a pointer whose first 8 bytes are readable,
   and whose vtable is the audited `NpPhysics` table. `PxGetPhysics` itself is the only function the probe calls
   before this check.
3. **Tolerances.** `PxPhysics::getTolerancesScale()` returns a length between 0.01 and 100 and a speed between 0.1
   and 1000.
4. **Scenes.** `getNbScenes()` is between 1 and 16, and `getScenes()` fills that many pointers, each readable and
   with the audited `NpScene` vtable.
5. **Gravity and counts.** Every scene's gravity is finite with a magnitude of at most 100, and every scene has at
   most 10,000,000 actors (`getNbActors(static | dynamic)`). At least one scene has a gravity magnitude between 1 and
   30, and −Z is the expected direction. Gravity is recorded for every scene.
6. **Objects.** In each scene, it checks the first 64 actors. It then keeps going, up to 10,000 actors per scene,
   until it has also checked at least one triangle mesh, one heightfield and one convex mesh, or runs out of actors.
   For every actor, shape and referenced mesh, two things must hold:
   - the object is readable and its vtable is an audited table of an accepted class (see `GameAbi`);
   - `getConcreteType()`, a non-virtual read of a data member, returns that class's type ID. This checks the object's
     data layout as well as its vtable.
7. **Geometry and meshes.** For the shapes checked in step 6, each shape's `getGeometryType()` matches the class of
   the mesh it references. For example, a triangle-mesh geometry must reference a BV33 or BV34 triangle mesh.

After that, the census checks every object it visits (see `Census`). Every skip and every mismatch there is counted
and reported, and the in-game pass criteria require both counts to be zero.

## Coordinate check

- **Pass/fail:** `RaycastDown` from V's feet + (0, 0, 1.8), over 3.0 m, against static geometry, in every scene.
  The nearest hit counts, and the report records which scene it came from. The check passes when that hit is at
  1.8 ± 0.3 m.
- **Reading a failure:** a steady offset suggests V's world position isn't at the feet, which is a convention to
  note. A miss, or a wildly different distance, suggests PhysX and game coordinates don't line up, for example
  because of an origin shift.
- **Informational:** the nearest capsule shape on a dynamic or kinematic actor within 1.5 m of V's feet (possibly
  V's character controller), with its radius and half-height.

## Report schema (`report.json`)

```json
{
  "probeVersion": "0.3.0",
  "time": "2026-10-01T12:34:56",
  "trigger": "hotkey | request",
  "game": {
    "runtime": "2.3.1",
    "physxFileVersion": "3.4.2.0",
    "modules": [
      { "name": "PhysX3_x64.dll", "timestamp": "0x64c978b4", "imageSize": "0x281000", "audited": true },
      { "name": "PhysX3Common_x64.dll", "timestamp": "0x64c978a8", "imageSize": "0x1f9000", "audited": true }
    ]
  },
  "player": {
    "feet": [0.0, 0.0, 0.0],
    "forward": [0.0, 1.0, 0.0],
    "camera": { "position": [0.0, 0.0, 1.7], "forward": [0.0, 1.0, 0.0], "fov": 80.0 }
  },
  "physxOrigin": null,
  "compatibility": {
    "pass": true,
    "steps": [ { "step": 1, "name": "build", "pass": true, "detail": "..." } ],
    "censusSkipped": 0,
    "censusMismatches": 0
  },
  "scenes": [ {
    "index": 0,
    "gravity": [0.0, 0.0, -9.81],
    "flags": "0x...",
    "requiresRwLock": false,
    "staticActors": 0, "dynamicActors": 0,
    "aggregates": 0, "actorsInAggregates": 0, "articulations": 0,
    "byConcreteType": { "PxRigidStatic": 0 },
    "byGeometryType": { "triangleMesh": 0 },
    "shapeFlags": { "simulation": 0, "sceneQuery": 0, "trigger": 0 },
    "queryFilter": { "w0:w1:w2:w3": 0, "other": 0 },
    "simulationFilter": { "w0:w1:w2:w3": 0, "other": 0 },
    "namedActors": 0, "actorsWithUserData": 0, "actorNameSamples": [ "..." ],
    "skipped": { "unreadable": 0, "unauditedVtable": 0 },
    "mismatches": 0,
    "censusMs": 0.0,
    "staticCenters": { "count": 0, "min": null, "median": null, "max": null },
    "dynamicCenters": { "count": 0, "min": null, "median": null, "max": null },
    "capsules": [ { "actor": "0x...", "center": [0.0, 0.0, 0.0], "axis": [0.0, 0.0, 1.0], "radius": 0.32,
                    "halfHeight": 0.4, "kind": "kinematic", "queryFilter": "...", "simulationFilter": "..." } ]
  } ],
  "region": {
    "center": [0.0, 0.0, 0.0], "halfExtents": [32.0, 32.0, 24.0],
    "boundsSelectedActors": 0, "overlapHits": 0, "overlapOverflow": false,
    "selectMs": 0.0, "overlapMs": 0.0, "extractMs": 0.0,
    "triangles": 0, "trianglesByCategory": { "static_trimesh": 0 },
    "skipped": { "plane": 0, "unknownGeometry": 0, "badTriangles": 0, "unreadable": 0 }, "triangleCapHit": false
  },
  "coordinateCheck": { "rayHit": true, "hitDistance": 1.8, "pass": true, "nearestCapsule": null },
  "mainThreadPauseMs": 0.0,
  "files": { "obj": "collision.obj", "mtl": "collision.mtl" }
}
```

- `player.camera` is `null` when the camera couldn't be read.
- `physxOrigin` is the origin `dump.request` gave, or `null`. `player`, `region.center` and the OBJ are in world
  coordinates either way.
- `staticCenters` and `dynamicCenters` give the minimum, median and maximum of the centres of the scene's actor bounds,
  in raw PhysX coordinates. Actors with unbounded shapes (planes) are left out, and the three are `null` when there
  are none.
- `capsules` lists the capsules on dynamic actors in raw PhysX coordinates (at most 4,096 per scene). `actor` is the
  actor's address, which matches the same actor across dumps. Character controllers are among them.
- The census's skips are split into objects whose memory wasn't readable and objects whose vtable isn't an audited
  table; `compatibility.censusSkipped` and `censusMismatches` sum them over the scenes.
- In the region, `skipped.unreadable` counts shapes whose mesh data failed the range check.
- Revision 1's `faults` count is gone, because nothing is caught any more. `probeVersion` is 0.2.0 for revision 2
  and 0.3.0 from revision 2.1 on, and the load log line shows the same version.

## Error handling

- **No player:** log and skip.
- **Compatibility failure:** the report is written with the failing step. No geometry is read and no OBJ is written.
- **An object fails its check** (unreadable, or a vtable that isn't audited): the object is skipped and counted,
  nothing is called on it, and the dump carries on. After 100 skipped objects in a scene, the probe stops reading
  that scene.
- **Mesh data fails its range check:** that shape's triangles are skipped and counted.
- **A fault anyway:** the probe has no guard, so a fault would crash the game with the game's own crash report. That
  is better than revision 1's guard, which turned a fault into a frozen game.
- **Overlap buffer overflow:** recorded. Extraction uses the bounds selection either way.
- **Triangle cap reached:** recorded in the report.
- **Previous dump still being written:** the trigger is ignored and logged.
- **Output folder or file errors:** logged.

## Safety

- **The probe is read-only, and the design makes that hold:**
  - it calls only getters whose slots the audit verified in the game's DLLs;
  - it calls them only on objects whose vtables are audited tables;
  - it makes no calls at all unless the running DLLs are the audited build.

  Revision 1 broke this without knowing. Its `PxShape` calls landed one slot late, so four of its "getters" were
  setters: `setLocalPose`, `setSimulationFilterData`, `setQueryFilterData` and `setFlags`.
- **New calls:** a PhysX call is added only after it has been checked against the game's DLLs, recorded in the
  audit, and added to the slot test.
- **No guard:** the probe doesn't catch faults, because the game's crash handler makes catching them worse than
  crashing (see Environment).
- **When it runs:** a dump runs only when the user presses F10 or the agent creates the request file. Neither should
  happen during a loading screen, while the world is changing fastest.
- Between dumps it costs one key check per frame and one file check per second.
- During a dump, holding a scene's read lock briefly pauses any game thread that wants to change that scene.
- Installing it means one folder, `E:\Cyberpunk2077\red4ext\plugins\PhysXProbe\`, and deleting that folder removes
  it. Nothing else in the game folder or the saves is touched.

## Testing

**Unit tests** (`ctest`, no game needed):
- the coordinate mapping, with known points and a round trip;
- box, sphere and capsule triangles:
  - exact counts: 12, 48 and 64;
  - vertices at the right distance from the centre or axis;
  - closed meshes, with every edge shared by exactly two triangles;
- heightfield triangles:
  - a 3×3 grid gives 8 triangles;
  - scales are applied;
  - with the tess flag set, and then clear, one cell gives exactly the triangles listed under `ShapeTriangles`;
  - `materialIndex0` controls the first triangle and `materialIndex1` the second;
  - material 127 removes a triangle;
  - a region covering part of the grid visits only the cells under it;
- mesh scale with rotation, and flipped winding for a negative determinant;
- fan triangulation of an n-gon into n − 2 triangles;
- `KeepIfTouches` region culling;
- the OBJ writer's exact output for a small input, including shared vertices;
- string escaping in the JSON writer;
- **the slot test:** for every PhysX call the probe makes, the build calls the slot the audit verified in the game's
  DLLs. For example, `PxShape::getGeometryType` must go through slot 7 and `getQueryFilterData` through slot 23.
  - **How it works:** the test builds a fake object whose vtable entries each record their own slot number. It then
    makes each call through the real headers and checks the recorded number.
  - **What it catches:** a header change, or a revert of the local `PxShape.h` fix, fails it.
- `GameAbi`:
  - the timestamp and image size read from a hand-built PE header;
  - `AuditedClassOf` for every audited table, for an address one slot off, and for a module loaded at a different base;
- `Readable`, on real memory:
  - a readable buffer;
  - a `PAGE_NOACCESS` page and a reserved but uncommitted page;
  - a range that starts readable and runs into an unreadable page;
  - null and wrapping ranges.

**Renderer tests** (`pytest`): OBJ loading, hidden groups, near-plane clipping, and a wall correctly hiding the floor
behind it.

**In-game pass criteria:**
1. All seven compatibility steps pass, and the census skips nothing and finds no mismatches.
2. The coordinate check passes.
3. `eye.png` visibly matches a screenshot taken from the same camera pose.
4. The report contains the census, overlap, extraction and main-thread pause timings.

**How it's run:**
1. **The startup world first.** The user launches the game and waits at the main menu. The agent creates
   `dump.request`. The dump reads the stand-in's small world. It must meet three checks:
   - every compatibility step passes;
   - the census skips nothing and finds no mismatches;
   - the shape flag and filter tallies, empty in revision 1's report, are filled in.

   This tests every new check on a small, stable world before the city.
2. **Free roam.** The user loads a save and stands in free roam on the ground somewhere with varied geometry. Standing
   still with the mouse left alone, the user presses F10, then takes a screenshot without moving.
3. **More dumps.** Further dumps are triggered by creating `dump.request` from the development shell, or with F10,
   never during a loading screen.

## Risks

- **CDPR modified PhysX:** this has happened. The audit found one changed class and corrected the headers for it.
  Any other build, such as a game patch that touches either PhysX DLL, fails compatibility step 1. The probe then
  reads nothing until the audit is redone for the new DLLs.
- **The world may be split:** it could span several scenes, or keep static collision in aggregates. The census
  reports both.
- **Threading:** reads happen on the main thread while physics may be simulating on worker threads. None of the
  `PxScene` methods the probe uses is marked "not during simulation" in the 3.4.2 headers.
  - If the game doesn't use the read-write lock, a streaming thread could change the actor list mid-read. The
    startup scene doesn't use it (its flags lack `eREQUIRE_RW_LOCK`). The per-object checks catch an object that
    has been freed and reused, and the skip counts show whether that happens. A fault would now crash the game.
  - If the game does use the lock, and ever holds a write lock while waiting on the main thread, `lockRead` would
    freeze the game. That's unlikely. If a dump ever freezes, the next version reads without the lock.
- **Streaming:** right after a save loads, collision near V may still be streaming in. Repeated dumps show whether
  the counts settle.
- **Hotkey clashes:** F10 may be bound by another mod. The request file doesn't depend on it.
