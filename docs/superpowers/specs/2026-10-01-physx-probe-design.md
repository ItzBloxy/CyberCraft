# PhysX Probe: design (spike 1)

Date: 2026-10-01. Status: design approved. The spec was revised after a deep review against the RED4ext SDK and
the PhysX 3.4.2 sources.

## Purpose

This is the first of three spikes that de-risk a SkyCraft-style "play Cyberpunk 2077 as a Minecraft player" mod.
The mod's collision exporter plays the part of SkyCraft's `skse/src/Collision.cpp`, and it depends on reading
Cyberpunk's physics world. Cyberpunk ships NVIDIA PhysX 3.4.2 as separate DLLs, and `PhysX3_x64.dll` exports
`PxGetPhysics`. The probe finds out whether that physics world can be read through NVIDIA's public 3.4.2 headers.

It answers three questions:

1. **Compatibility:** do calls made through the public 3.4.2 headers reach the right functions in the game's
   PhysX build?
2. **Coordinates:** are PhysX scene coordinates the game's world coordinates (metres, Z up), with no origin shift?
3. **Content and cost:** what does the collision around V contain (shape types, collision-filter values,
   triangle counts), and how long does it take to extract?

A clear "no" to question 1 also counts as a result. The collision plan would then switch to ray casts through the
game's script API, as in SkyCraft's own first stage.

## Non-goals

- No Minecraft side, shared memory, streaming, or per-frame export.
- No hooks into game functions, and no writes to game or physics state.
- No rendering inside the game.
- Only game version 2.31 is supported.

## Environment (checked on 2026-09-30 and 2026-10-01)

- Cyberpunk 2077 2.31 (Epic) at `E:\Cyberpunk2077`, with RED4ext 1.30.0. Mods are managed by Vortex.
- `bin\x64\PhysX3_x64.dll` has file version 3.4.2.0. It exports `PxGetPhysics`, `PxRegisterCloth` and
  `PxRegisterParticles`. The last two show that `PX_USE_CLOTH_API` and `PX_USE_PARTICLE_SYSTEM_API` are 1. Those
  are also the header defaults, and they matter because they change `PxScene`'s virtual function table.
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
  extern/RED4ext.SDK/        git submodule (WopsS/RED4ext.SDK), used header-only
  extern/physx-3.4.2/        NVIDIA PhysX 3.4.2 public headers, BSD-3 licence text kept
                             (PhysX_3.4/Include and PxShared/include only)
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
- There are three triggers:
  - **F10:** edge-detected with `GetAsyncKeyState`, and only while the foreground window belongs to the game's process.
  - **Request file:** `dump.request` in the plugin folder, checked once a second and deleted when the dump starts.
  - **Automatic:** one dump 10 seconds after V is first found, once per game launch.
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
- It returns nothing when the engine, the game instance, the player system or V isn't available, as on the main
  menu or during loading. The camera part is optional; without it, the render falls back to a point 1.7 m above V's
  feet, looking along V's forward.
- `PlayerPose { Vec3d feet; Vec3f forward; std::optional<CameraPose> camera; }`, with
  `CameraPose { Vec3d position; Vec3f forward; float fov; }`. All of these are in world coordinates (metres, Z up).

### `physx/PhysxSession` (reusable)

This is the only code that touches PhysX. Every PhysX call goes through a small `__try`/`__except` helper, kept in
functions that own no C++ objects needing unwinding. A fault ends the current operation with an error and never
reaches the game. All scene reads during a dump happen between `lockRead` and `unlockRead` on that scene.

- `Open()`:
  1. Calls `GetModuleHandleW(L"PhysX3_x64.dll")`.
  2. Reads the module's version resource.
  3. Calls `GetProcAddress("PxGetPhysics")`.
  4. Runs the compatibility check below.

  The result is cached for the session.
- `Scenes()` returns the scene pointers.
- `Census(scene)` walks `getActors(eRIGID_STATIC | eRIGID_DYNAMIC)` and each actor's shapes. It returns counts and
  tallies without reading any geometry:
  - actors by concrete type and by kind, and shapes by geometry type and by flags;
  - tallies of query and simulation filter data, keeping the 64 most common combinations and an "other" count;
  - aggregates (`getNbAggregates`/`getAggregates`), the number of actors inside them, and the number of
    articulations (`getNbArticulations`), since `getActors` never returns articulation links;
  - how many actors have a name and how many have user data, plus up to 32 distinct actor names. Names are
    read under the crash guard, cut to 128 characters, and have non-printable bytes replaced.
  - It also re-checks the concrete type (name and ID) of every object it visits and counts mismatches.
- `SelectByBounds(scene, box)` keeps the actors whose `PxActor::getWorldBounds()` overlaps the region box.
- `QueryBox(scene, box)` runs `PxScene::overlap` with a box geometry and the flags
  `eSTATIC | eDYNAMIC | eNO_BLOCK`. It uses a heap-allocated touch buffer of 8192 hits and reports when that buffer
  overflows. The report compares its result with `SelectByBounds`, because the overlap query only sees scene-query
  shapes.
- `RaycastDown(scene, from, maxDistance)` runs `PxScene::raycast` against static geometry only.

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
2. `PhysxSession::Open()`. This runs the compatibility check on the first dump only. On a failure it writes the
   report and stops, without reading any geometry.
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

1. `PhysX3_x64.dll` is loaded, and its version resource is 3.4.2.x.
2. The `PxGetPhysics` export exists. It returns a non-null pointer to committed, readable memory, checked with
   `VirtualQuery`.
3. `PxPhysics::getTolerancesScale()` returns a length between 0.01 and 100 and a speed between 0.1 and 1000.
4. `getNbScenes()` is between 1 and 16, and `getScenes()` fills that many non-null pointers.
5. Every scene's gravity is finite with a magnitude of at most 100, and every scene has at most 10,000,000 actors
   (`getNbActors(static | dynamic)`). At least one scene has a gravity magnitude between 1 and 30, and −Z is the
   expected direction. Gravity is recorded for every scene.
6. This is the decisive step. In each scene, it checks the first 64 actors. It then keeps going, up to 10,000 actors
   per scene, until it has also checked at least one triangle mesh, one heightfield and one convex mesh, or runs
   out of actors. For every actor, shape and referenced mesh it checks, two things must hold:
   - `getConcreteTypeName()` returns one of the 3.4.2 names: `PxRigidStatic`, `PxRigidDynamic`, `PxArticulation`,
     `PxArticulationLink`, `PxShape`, `PxMaterial`, `PxConvexMesh`, `PxHeightField`, `PxBVH33TriangleMesh`,
     `PxBVH34TriangleMesh`, `PxAggregate`, `PxConstraint`, `PxCloth`, `PxClothFabric`, `PxParticleSystem`,
     `PxParticleFluid` or `PxPruningStructure`.
   - `getConcreteType()` agrees with the name. That is a non-virtual read of a data member, so it checks the object
     layout as well as the virtual table.
7. For the shapes checked in step 6, each shape's `getGeometryType()` matches the concrete type of the mesh it
   references. For example, a triangle-mesh geometry must reference a `PxBVH33TriangleMesh` or
   `PxBVH34TriangleMesh`.

After that, the census re-checks every object it visits (see `Census`). Any mismatch there also fails compatibility.

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
  "probeVersion": "0.1.0",
  "time": "2026-10-01T12:34:56",
  "trigger": "auto | hotkey | request",
  "game": { "runtime": "2.31.0", "physxFileVersion": "3.4.2.0" },
  "player": {
    "feet": [0.0, 0.0, 0.0],
    "forward": [0.0, 1.0, 0.0],
    "camera": { "position": [0.0, 0.0, 1.7], "forward": [0.0, 1.0, 0.0], "fov": 80.0 }
  },
  "compatibility": {
    "pass": true,
    "steps": [ { "step": 1, "name": "module", "pass": true, "detail": "..." } ],
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
    "censusMs": 0.0
  } ],
  "region": {
    "center": [0.0, 0.0, 0.0], "halfExtents": [32.0, 32.0, 24.0],
    "boundsSelectedActors": 0, "overlapHits": 0, "overlapOverflow": false,
    "selectMs": 0.0, "overlapMs": 0.0, "extractMs": 0.0,
    "triangles": 0, "trianglesByCategory": { "static_trimesh": 0 },
    "skipped": { "plane": 0 }, "triangleCapHit": false
  },
  "coordinateCheck": { "rayHit": true, "hitDistance": 1.8, "pass": true, "nearestCapsule": null },
  "mainThreadPauseMs": 0.0,
  "faults": 0,
  "files": { "obj": "collision.obj", "mtl": "collision.mtl" }
}
```

`player.camera` is `null` when the camera couldn't be read.

## Error handling

- **No player:** log and skip.
- **Compatibility failure:** the report is written with the failing step. No geometry is read and no OBJ is written.
- **Fault during the census or extraction:** that operation stops, partial results are kept, the fault is counted,
  and the report is still written.
- **Overlap buffer overflow:** recorded. Extraction uses the bounds selection either way.
- **Triangle cap reached:** recorded in the report.
- **Previous dump still being written:** the trigger is ignored and logged.
- **Output folder or file errors:** logged.

## Safety

- The probe is read-only: it never writes to PhysX or game state.
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
- the crash guard, which must survive an access violation and a thrown exception.

**Renderer tests** (`pytest`): OBJ loading, hidden groups, near-plane clipping, and a wall correctly hiding the floor
behind it.

**In-game pass criteria:**
1. All seven compatibility steps pass, and the census finds no mismatches.
2. The coordinate check passes.
3. `eye.png` visibly matches a screenshot taken from the same camera pose.
4. The report contains the census, overlap, extraction and main-thread pause timings.

Triggering on the main menu must log "no player" and do nothing.

**How it's run:**
1. The user launches the game and loads a save, standing in free roam on the ground somewhere with varied geometry.
2. The user stands still and leaves the mouse alone, then presses F10 or waits for the automatic dump 10 seconds
   after loading. Right after the dump, still without moving, the user takes a screenshot.
3. Further dumps are triggered by creating `dump.request` from the development shell, or with F10.

## Risks

- **CDPR may have modified PhysX:** the check would stop at one of its steps, most likely 6 or 7. That would be a
  decisive negative result, and collision would move to ray casts through the game's `SpatialQueriesSystem`.
- **The world may be split:** it could span several scenes, or keep static collision in aggregates. The census
  reports both.
- **Threading:** reads happen on the main thread while physics may be simulating on worker threads. None of the
  `PxScene` methods the probe uses is marked "not during simulation" in the 3.4.2 headers.
  - If the game doesn't use the read-write lock, a streaming thread could change the actor list mid-read. The
    crash guards contain this, and the fault and mismatch counts show whether it happens.
  - If the game does use the lock, and ever holds a write lock while waiting on the main thread, `lockRead` would
    freeze the game. That's unlikely. If a dump ever freezes, the next version reads without the lock.
- **Streaming:** right after a save loads, collision near V may still be streaming in. Repeated dumps show whether
  the counts settle.
- **Hotkey clashes:** F10 may be bound by another mod. The request file and the automatic dump don't depend on it.
