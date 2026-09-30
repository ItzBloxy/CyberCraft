# PhysX Probe: design (spike 1)

Date: 2026-10-01. Status: design approved; spec under review.

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

## Environment (checked on 2026-09-30)

- Cyberpunk 2077 2.31 (Epic) at `E:\Cyberpunk2077`, with RED4ext 1.30.0. Mods are managed by Vortex.
- `bin\x64\PhysX3_x64.dll` has file version 3.4.2.0. It exports `PxGetPhysics`, `PxRegisterCloth` and
  `PxRegisterParticles`. The last two show that `PX_USE_CLOTH_API` and `PX_USE_PARTICLE_SYSTEM_API` are 1. Those
  are also the header defaults, and they matter because they change `PxScene`'s virtual function table.
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
    src/Plugin.cpp
    src/PlayerLocator.h, .cpp
    src/Coords.h
    src/DumpWriter.h, .cpp
    src/physx/PhysxSession.h, .cpp
    src/physx/ShapeTriangles.h, .cpp
    tests/                   unit tests (a small test program run by ctest)
    tools/render_dump.py
```

`extern/` sits at the repo root so the real plugin can reuse it later.

## Build

- CMake with the "Visual Studio 17 2022" generator on x64, in C++20. The presets are `default`, and then `release` for
  building and testing.
- The RED4ext SDK is used in header-only mode (`RED4EXT_HEADER_ONLY=ON`).
- The PhysX headers are used without any import library. The probe makes only virtual calls on objects the game
  hands it, plus inline code from the headers. `PX_PHYSX_STATIC_LIB` is defined so the core API macro is empty.
  The built DLL must not import anything from `PhysX3*.dll`, and `dumpbin /imports` is the check for this.
- After each build, a post-build step copies `PhysXProbe.dll` to
  `<CP2077_DIR>\red4ext\plugins\PhysXProbe\` when the CMake variable `CP2077_DIR` is set. The preset sets it to
  `E:\Cyberpunk2077`.

## Components

### `Plugin.cpp`

- Provides the RED4ext entry points:
  - `Query` gives the name `PhysXProbe` and runtime `RED4EXT_V1_RUNTIME_VERSION_LATEST`;
  - `Supports`;
  - `Main`.
- On load, it registers a `GameState` for `EGameStateType::Running`. That state's `OnUpdate` runs every frame on
  the game's main thread.
- `OnUpdate` checks the triggers and runs at most one dump per frame, with dumps at least 2 seconds apart.
- There are three triggers:
  - **F10:** edge-detected with `GetAsyncKeyState`, and only while the foreground window belongs to the game's process.
  - **Request file:** `dump.request` in the plugin folder, checked once a second and deleted when the dump starts.
  - **Automatic:** one dump 10 seconds after V is first found, once per game launch.
- Log lines go through RED4ext's logger.

### `PlayerLocator`

- `std::optional<PlayerPose> Locate()` calls `cpPlayerSystem.GetLocalPlayerControlledGameObject()` through
  `RED4ext::ExecuteFunction`. It then calls `GetWorldPosition()` and `GetWorldYaw()` on the returned object.
- It returns nothing when there is no player, as on the main menu or during loading.
- `PlayerPose { double x, y, z; float yaw; }` is in world coordinates (metres, Z up). The yaw is stored exactly as
  the game reports it, and the renders confirm its units (degrees are expected).

### `physx/PhysxSession` (reusable)

This is the only code that touches PhysX. Every PhysX call goes through a small `__try`/`__except` helper, kept in
functions that own no C++ objects needing unwinding. A fault ends the current operation with an error and never
reaches the game.

- `Open()`:
  1. Calls `GetModuleHandleW(L"PhysX3_x64.dll")`.
  2. Reads the module's version resource.
  3. Calls `GetProcAddress("PxGetPhysics")`.
  4. Runs the compatibility check below.

  The result is cached for the session.
- `Scenes()` returns the scene pointers.
- `Census(scene)` runs under `lockRead`/`unlockRead`. It walks `getActors(eRIGID_STATIC | eRIGID_DYNAMIC)` and each
  actor's shapes, and returns counts and tallies without reading any geometry. It also reports:
  - the aggregate count (`getNbAggregates`/`getAggregates`), and the number of actors inside aggregates;
  - how many actors have a name, and how many have user data.
- `SelectByBounds(scene, box)` keeps the actors whose `PxActor::getWorldBounds()` overlaps the region box.
- `QueryBox(scene, box)` runs `PxScene::overlap` with a box geometry and the flags
  `eSTATIC | eDYNAMIC | eNO_BLOCK`. It uses a touch buffer of 8192 hits and reports when that buffer overflows. The
  report compares its result with `SelectByBounds`, because the overlap query only sees scene-query shapes.
- `RaycastDown(scene, from, maxDistance)` runs `PxScene::raycast` against static geometry only.

### `physx/ShapeTriangles` (reusable)

- `AppendTriangles(shape, actorPose, regionBox, out)` adds world-space triangles for one shape. It keeps only the
  triangles whose bounds touch the region box.
- It handles each geometry type as follows:
  - **Triangle mesh:** applies `PxMeshScale` the way PhysX does, as rotation⁻¹ · scale · rotation
    (`PxMeshScale::transform`, which is inline in the header). It handles 16- or 32-bit indices, and winding flips
    when the scale's determinant is negative.
  - **Heightfield:** sample (row r, column c) sits at local (r × rowScale, height × heightScale, c × columnScale), so
    rows run along local X, height along local Y and columns along local Z. Triangulation follows PhysX's own
    `Gu::HeightField::getTriangleVertexIndices` from the 3.4.2 source: the tess flag picks the cell's diagonal, and
    material 127 (`eHOLE`) removes a triangle. The material is `materialIndex & 0x7F`.
  - **Convex mesh:** its polygons are fan-triangulated, with the mesh scale applied the same way.
  - **Box:** 12 triangles.
  - **Capsule and sphere:** low-poly, with 8 segments. PhysX capsules lie along the shape's local X axis.
  - **Plane:** skipped and counted.

  Every shape's local pose is applied first, then its actor's global pose.
- The geometry maths lives in plain functions that take plain data, so it can be unit-tested without PhysX objects:
  `BoxTriangles`, `CapsuleTriangles`, `SphereTriangles`, `HeightfieldTriangles`, `ConvexPolygonTriangles`,
  `ApplyMeshScale` and `KeepIfTouches`.

### `Coords.h`

- `McFromWorld(x, y, z)` returns `(x, z, -y)`, and `WorldFromMc(x, y, z)` returns `(x, -z, y)`.
- 1 world unit = 1 block = 1 metre. Positions are doubles.
- The mapping is a proper rotation (determinant +1), so triangle winding is preserved.

### `DumpWriter`

- Creates `dumps\<YYYY-MM-DD_HH-MM-SS>\` in the plugin folder.
- `collision.obj` and `collision.mtl`:
  - Vertices are in Minecraft axes, relative to V's feet, with millimetre precision. V's world position is written
    as a comment.
  - There is one object group per category, and each category has its own colour. A category is
    `<kind>_<geometry>`, for example `static_trimesh`. The kind is picked in this order:
    1. `trigger` if the shape has the trigger flag;
    2. otherwise `queryonly` if it has the scene-query flag but not the simulation flag;
    3. otherwise the actor's kind: `static`, `kinematic` (a rigid dynamic with the kinematic flag), `dynamic` or
       `articulation`.
  - With more than one scene, group names are prefixed with the scene index.
  - Output is capped at 2,000,000 triangles per dump, and the report records it if the cap is hit.
- `report.json` is written by a small JSON writer in `DumpWriter`, with no third-party JSON library. It follows the
  schema below.

### `tools/render_dump.py`

- It is run with `uv run --with numpy --with pillow tools/render_dump.py <dump folder>`.
- It writes `top.png`, an orthographic top-down view coloured by category.
- It also writes `eye.png`, a perspective view from V's eye with pitch 0 and an 80° horizontal field of view. The
  eye is 1.7 m above the feet, looking along V's yaw.
- Rendering sorts triangles far to near and draws them with Pillow, using flat shading from the face normal.
- It skips `trigger` and `queryonly` by default, and `--all` includes them.

## One dump, step by step

1. `PlayerLocator::Locate()`. If there's no player, it logs "no player" and stops.
2. `PhysxSession::Open()`. This runs the compatibility check on the first dump only. On a failure it writes the
   report and stops, without reading any geometry.
3. `Census` for each scene.
4. The region box is centred on V's feet plus (0, 0, 8), with half-extents (32, 32, 24) m. That covers ±32 m
   horizontally and −16 to +32 m vertically. For every scene, the probe runs `SelectByBounds` and `QueryBox`, then
   `AppendTriangles` for each shape of the bounds-selected actors.
5. The coordinate check.
6. The files are written, and a summary line goes to the log.

The census, region selection, overlap query and triangle extraction are each timed with `QueryPerformanceCounter`.

## Compatibility check

The steps run in order and stop at the first failure, which the report names.

1. `PhysX3_x64.dll` is loaded, and its version resource is 3.4.2.x.
2. The `PxGetPhysics` export exists. It returns a non-null pointer to committed, readable memory, checked with
   `VirtualQuery`.
3. `PxPhysics::getTolerancesScale()` returns a length between 0.01 and 100 and a speed between 0.1 and 1000.
4. `getNbScenes()` is between 1 and 16, and `getScenes()` fills that many non-null pointers.
5. For each scene, the magnitude of `getGravity()` is between 1 and 30, and `getNbActors(static | dynamic)` is at
   most 10,000,000. The gravity direction is recorded; −Z is expected.
6. This is the decisive step. For the first 64 actors, their shapes, and the meshes those shapes reference, two things
   must hold:
   - `getConcreteTypeName()` returns one of the 3.4.2 names: `PxRigidStatic`, `PxRigidDynamic`, `PxArticulation`,
     `PxArticulationLink`, `PxShape`, `PxMaterial`, `PxConvexMesh`, `PxHeightField`, `PxBVH33TriangleMesh`,
     `PxBVH34TriangleMesh`, `PxAggregate`, `PxConstraint`, `PxCloth`, `PxClothFabric`, `PxParticleSystem`,
     `PxParticleFluid` or `PxPruningStructure`.
   - `getConcreteType()` agrees with the name. That is a non-virtual read of a data member, so it checks the object
     layout as well as the virtual table.
7. For the shapes checked in step 6, each shape's `getGeometryType()` matches the concrete type of the mesh it
   references. For example, a triangle-mesh geometry must reference a `PxBVH33TriangleMesh` or
   `PxBVH34TriangleMesh`.

## Coordinate check

- **Pass/fail:** `RaycastDown` from V's feet + (0, 0, 1.8), over 3.0 m, against static geometry. It passes when the
  ray hits at 1.8 ± 0.3 m.
- **Informational:** the nearest capsule shape on a dynamic or kinematic actor within 1.5 m of V's feet (possibly
  V's character controller), with its radius and half-height.

## Report schema (`report.json`)

```json
{
  "probeVersion": "0.1.0",
  "time": "2026-10-01T12:34:56",
  "trigger": "auto | hotkey | request",
  "game": { "runtime": "2.31 (from the RED4ext SDK struct)", "physxFileVersion": "3.4.2.0" },
  "player": { "world": [0.0, 0.0, 0.0], "yaw": 0.0 },
  "compatibility": [ { "step": 1, "name": "module", "pass": true, "detail": "..." } ],
  "scenes": [ {
    "index": 0,
    "gravity": [0.0, 0.0, -9.81],
    "flags": "0x...",
    "requiresRwLock": false,
    "staticActors": 0, "dynamicActors": 0,
    "aggregates": 0, "actorsInAggregates": 0,
    "byConcreteType": { "PxRigidStatic": 0 },
    "byGeometryType": { "triangleMesh": 0 },
    "shapeFlags": { "simulation": 0, "sceneQuery": 0, "trigger": 0 },
    "queryFilter": { "w0:w1:w2:w3": 0 },
    "simulationFilter": { "w0:w1:w2:w3": 0 },
    "namedActors": 0, "actorsWithUserData": 0,
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
  "faults": 0,
  "files": { "obj": "collision.obj", "mtl": "collision.mtl" }
}
```

## Error handling

- **No player:** log and skip.
- **Compatibility failure:** the report is written with the failing step. No geometry is read and no OBJ is written.
- **Fault during the census or extraction:** that operation stops, partial results are kept, the fault is counted,
  and the report is still written.
- **Overlap buffer overflow:** recorded. Extraction uses the bounds selection either way.
- **Triangle cap reached:** recorded in the report.
- **File errors:** logged.

## Safety

- The probe is read-only: it never writes to PhysX or game state.
- Between dumps it costs one key check per frame and one file check per second.
- Installing it means one folder, `E:\Cyberpunk2077\red4ext\plugins\PhysXProbe\`, and deleting that folder removes
  it. Nothing else in the game folder or the saves is touched.

## Testing

**Unit tests** (`ctest`, no game needed):
- the coordinate mapping, with known points and a round trip;
- box, capsule and sphere triangles: counts, distances from the centre or axis, and closed meshes (every edge
  shared by exactly two triangles);
- heightfield triangles:
  - a 3×3 grid gives 8 triangles;
  - scales are applied;
  - the tess flag switches the diagonal, following PhysX's rule;
  - material 127 removes that triangle;
- mesh scale with rotation, and flipped winding for a negative determinant;
- fan triangulation of an n-gon into n−2 triangles;
- `KeepIfTouches` region culling;
- the OBJ writer's exact output for a small input.

**In-game pass criteria:**
1. All seven compatibility steps pass.
2. The coordinate check passes.
3. `eye.png` visibly matches a screenshot taken from the same spot.
4. The report contains census, overlap and extraction timings.

Triggering on the main menu must log "no player" and do nothing.

**How it's run:**
1. The user launches the game and loads a save, standing in free roam somewhere with varied geometry.
2. The automatic dump fires after 10 seconds, and the user takes a screenshot at the same time.
3. Further dumps are triggered by creating `dump.request` from the development shell, or with F10.

## Risks

- **CDPR may have modified PhysX:** the check would stop at step 6 or 7. That would be a decisive negative
  result, and collision would move to ray casts through the game's `SpatialQueriesSystem`.
- **The world may be split:** it could span several scenes, or keep static collision in aggregates. The census
  reports both.
- **Threading:** reads happen on the main thread while physics may be simulating on worker threads. If the game
  doesn't use the read-write lock, a streaming thread could change the actor list mid-read. The crash guards
  contain this, and fault counts show whether it happens. Moving the probe to just after the physics step is left
  for later spikes.
- **Hotkey clashes:** F10 may be bound by another mod. The request file and the automatic dump don't depend on it.
