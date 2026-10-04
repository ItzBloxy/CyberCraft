# PhysX ABI audit: the probe's headers against the game's DLLs

Date: 2026-10-01. Game: Cyberpunk 2077 2.31 (Epic). Done offline, from the game's DLLs and one crash dump, without
running the game.

## Why

The first probe run that got past the version check froze the game on the startup legal screen. The cause:

1. The automatic dump fired while the game was still starting up. `GetLocalPlayerMainGameObject` already returns a
   stand-in player at about (0, 3.6, 0) then.
2. The compatibility check passed, but the game's `PxShape` has one more virtual function than NVIDIA's public 3.4.2
   headers. From slot 6 on, each of the probe's `PxShape` calls ran the game's next function, and several of those
   are setters.
3. One of them returned garbage, and the probe read address 0x210. The game's crash handler sees access violations
   before any `__try/__except`. It wrote a crash dump, suspended every other thread and started the error reporter.
   The probe's guard let the main thread carry on, and it spun forever waiting for the suspended threads.

Evidence: the crash dump in `%LOCALAPPDATA%\REDEngine\ReportQueue\Cyberpunk2077-20261001-020952-8188-15048`. Its
exception record points at `PhysXProbe.dll+0x4adce`, reading `[rdx]` with rdx = 0x210, inside `FilterKey`. The caller
used the value returned by `call [rax+0B0h]`, which is `PxShape` slot 22, as the address of a `PxFilterData`.

## Method

- **Our side:** the compiler's own layout report (`cl /d1reportAllClassLayout`). The input was a file that includes the
  probe's `PxHeaders.h`, built with the probe's defines (`PX_PHYSX_STATIC_LIB`, `NDEBUG`). This gives every vtable
  slot and every field offset the probe was built with.
- **The game's side:** `PhysX3_x64.dll` and `PhysX3Common_x64.dll`, read with `dumpbin /disasm` and short Python
  scripts.
  - A vtable is an `.rdata` address that code loads with a rip-relative `lea` and that holds code pointers.
  - Class names come from the inline `getConcreteTypeName` (returns the name) and `isKindOf` (compares with it).
  - Abstract interface tables are mostly `_purecall` (RVA 0x14e330 in `PhysX3_x64.dll`, 0x165564 in
    `PhysX3Common_x64.dll`), so they give each interface's exact slot count.
- **Matching:** for every slot the probe calls, the game's function was disassembled and checked against what the
  header says it does. Examples:
  - a hidden-pointer getter copies into `[rdx]` and returns rdx;
  - a type-checked geometry getter compares the geometry type with the right enum value;
  - a count getter reads a field.
- **Liveness:** the crash dump's captured memory was searched for vtable pointers. It holds 6 objects whose vtable is
  `NpShape`'s, 2 `NpRigidStatic` and 1 `NpScene`, so these are the tables live objects use.

## Results: virtual functions

| Interface | Our slots | Game's slots | Concrete table (RVA) | Probe's slots | Verdict |
|---|---|---|---|---|---|
| `PxBase` | 5 | 5 | n/a | 1 | match |
| `PxPhysics` | 40 | 40 | `NpPhysics` 0x20a310 | 4, 19, 20 | match |
| `PxScene` | 118 | 118 | `NpScene` 0x20caf0 | 3, 18, 19, 22, 26, 27, 67, 94, 96, 105, 106 | match |
| `PxRigidStatic` | 29 | 29 | `NpRigidStatic` 0x20b980 | 1, 5, 8, 9, 20, 25, 26 | match |
| `PxRigidDynamic` | 78 | 78 | `NpRigidDynamic` 0x20b2e8 | the same, plus 47 | match |
| `PxAggregate` | 14 | 14 | `NpAggregate` 0x206e68 | 9 | match |
| **`PxShape`** | **37** | **38** | `NpShape` 0x20e9a8 | 1, 6, 9–11, 13–15, 18, 20, 22, 33 | **mismatch from slot 6** |
| `PxConvexMesh` | 15 | 16 | `Gu::ConvexMesh` 0x1b5200 (Common) | 5–9 | match (extra slot at the end) |
| `PxTriangleMesh` | 17 | 18 | BV33 0x1b6978, BV34 0x1b6868 (Common) | 5, 6, 9, 10, 11 | match (extra slot at the end) |
| `PxHeightField` | 21 | 21 | `Gu::HeightField` 0x1b6b18 (Common) | 7, 8, 19 | match |
| `PxHitCallback` (the probe's object) | 3 | game calls 0 and 1 | n/a | n/a | match |

Some cross-checks that held:

- **Scene queries:** the abstract scene class between `PxScene` and `NpScene` implements exactly slots 0, 94, 95 and
  96 (destructor, `raycast`, `sweep`, `overlap`), which is where our headers put them.
- **Abstract interfaces:** `PxScene` and `PxPhysics` implement only slot 0, their destructor, as ours do.
- **Triangle meshes:** BV33 and BV34 share every slot the probe uses.

### `PxShape` in detail

The game's slot 5, `acquireReference`, does an atomic operation on `this+0x20`. Slot 6 is the extra function:
`mov eax,[rcx+20h]; ret`, a reference-count getter. NVIDIA's final public 3.4 release (GitHub PhysX-3.4 `5e42a5f`,
"patch release @25354359") doesn't have it, so the game's PhysX is a modified or non-public 3.4.2 build. Every slot from
6 on is one higher in the game:

| Our slot: the probe meant | Game's same slot: what actually ran |
|---|---|
| 6 `getGeometryType` | the reference-count getter (read-only) |
| 9 `getBoxGeometry` | `getGeometry`: writes 48 bytes into the probe's 16-byte struct |
| 10 `getSphereGeometry` | `getBoxGeometry`: writes 16 bytes into an 8-byte struct |
| 11 `getCapsuleGeometry` | `getSphereGeometry` |
| 13 `getConvexMeshGeometry` | `getPlaneGeometry` |
| 14 `getTriangleMeshGeometry` | `getConvexMeshGeometry` |
| 15 `getHeightFieldGeometry` | `getTriangleMeshGeometry`: writes 48 bytes into a 32-byte struct |
| 18 `getLocalPose` | **`setLocalPose`**, from the probe's uninitialized buffer |
| 20 `getSimulationFilterData` | **`setSimulationFilterData`**, likewise |
| 22 `getQueryFilterData` | **`setQueryFilterData`**, likewise |
| 33 `getFlags` | **`setFlags`**, likewise |

In the crashed run, the census ran `setFlags` and `setQueryFilterData` on all 8 shapes of the startup world before
faulting. The region step ran `setFlags` and `setLocalPose` on the shapes of the 4 actors it selected. All of that
was memory-only state in a process that was then killed, and nothing was saved. The report's "5 boxes, 3 capsules" are
reference counts (3 and 2), not geometry types.

The fix on our side is one placeholder after `acquireReference()` in our copy of `PxShape.h`:
`virtual PxU32 getReferenceCount() const = 0;`. That puts every later declaration back on the game's slot.

## Results: data the game and the probe share

All of these match the compiler's layout of our headers:

- **Geometry structs:** the game's getters copy exactly our sizes and field offsets:
  - box 16, sphere 8, capsule 12;
  - convex mesh 48, with the mesh pointer at 0x20;
  - triangle mesh 48, with the mesh pointer at 0x28;
  - heightfield 32, with the pointer at 8.
- **Small structs:** `getLocalPose` copies 28 bytes and the filter-data getters 16. `PxGeometryHolder` is 48 on both
  sides.
- **`PxHullPolygon`:** `getPolygonData` writes the plane at 0, the vertex count at 16 and the index base at 18,
  20 bytes in all.
- **`PxHeightFieldSample`:** the stride is 4. `getSample` returns `&samples[row * columns + column]`, a reference, as
  declared.
- **`PxQueryFilterData`:** the game reads the query flags at offset 16.
- **Hit callbacks:** the game's overlap and raycast code use the probe's object at our offsets:
  - **overlap:** `hasBlock` 40, `touches` 48, `maxNbTouches` 56, `nbTouches` 60, and hits appended with a 32-byte
    stride (`PxOverlapHit` is 32);
  - **raycast:** `hasBlock` 72, `touches` 80, `maxNbTouches` 88, `nbTouches` 92, and `block.distance` at 60;
  - **virtual calls:** `processTouches` through slot 0 and `finalizeQuery` through slot 1.
- **`PxActor::userData` at 16:** the game's actors keep their name at `+0x18`, right after the 24-byte interface part.
- **`PxBase` type ID at 8 and `PxTolerancesScale::speed` at 8:** confirmed at runtime. The crashed run's IDs agreed
  with the names for 16 objects, and speed read 10, PhysX's default.

## The DLLs this applies to

| DLL | PE timestamp | Image size | FileVersion string / binary |
|---|---|---|---|
| `PhysX3_x64.dll` | 0x64c978b4 | 0x281000 | 3.4.2.0 / 3.4.1.0 |
| `PhysX3Common_x64.dll` | 0x64c978a8 | 0x1f9000 | 3.4.2.0 / 3.4.1.0 |

The RVAs above hold only for these exact files. A game patch that touches either DLL needs a fresh audit.

## What this means for the probe

1. **Headers:** add the `PxShape` placeholder, and keep it documented as the game's layout, not NVIDIA's.
2. **Compatibility check:** checking type names can't catch a vtable shift. That check passed while the probe was
   calling setters. The probe should instead:
   - check that each DLL's PE timestamp and size match the audited build;
   - before using any object, check that its vtable pointer is one of the audited tables (module base + RVA from the
     table above), and leave the object alone otherwise.
3. **No faults:** a structured-exception guard is not a safety net in this game, because the crash handler fires
   first. The probe must avoid faulting, not rely on catching faults.
4. **When to dump:** the automatic dump must wait until a save is loaded, not just until a player object exists.

## Addendum: spike 5's scene watch

Date: 2026-10-04. The same DLLs and the same method: our layout from `cl /d1reportAllClassLayout` on `PxScene.h`, and
the game's `PhysX3_x64.dll` read with Python and Capstone. Spike 5's step 3 hooks 16 of `PxScene`'s slots to watch which
threads write to scene 0, and when (`spikes/block-render/src/physx/SceneHooks.cpp`). Each hook calls the original
through the slot's own signature, so each slot's function had to be the one NVIDIA's header puts there.

| Slot | Call | Function's RVA | How it was told |
|---|---|---|---|
| 10 | `addActor(PxActor&)` | 0x520d0 | "PxScene::addActor(): actor is in a pruning structure …" |
| 11 | `addActors(const PxPruningStructure&)` | 0x54a30 | tests the structure's valid flag at +0x50, then "PxScene::addActors(): Provided pruning structure is not valid." |
| 12 | `addActors(PxActor* const*, PxU32)` | 0x54a20 | `xor r9d, r9d; jmp` to the internal add: no pruning structure, as `NpScene::addActors` passes `NULL` |
| 13 | `removeActor(PxActor&, bool)` | 0x521e0 | "PxScene::removeActor(): Actor … not assigned to scene …" |
| 14 | `removeActors(PxActor* const*, PxU32, bool)` | 0x54a90 | "PxScene::removeActors(): Actor … not assigned to scene …" |
| 15 | `addAggregate(PxAggregate&)` | 0x524e0 | "PxScene::addAggregate(): Aggregate already assigned to a scene." |
| 16 | `removeAggregate(PxAggregate&, bool)` | 0x525d0 | "PxScene::removeAggregate(): Aggregate … not assigned to scene …" |
| 56 | `simulate(PxReal, PxBaseTask*, void*, PxU32, bool)` | 0x53760 | "PxScene::simulate: Simulation is still processing last simulate call …"; calls the shared step with 3 |
| 57 | `advance(PxBaseTask*)` | 0x537a0 | "PxScene::advance: advance() called illegally! …" |
| 58 | `collide(PxReal, PxBaseTask*, void*, PxU32, bool)` | 0x53880 | the same shared step as `simulate`, called with 1 |
| 61 | `fetchResults(bool, PxU32*)` | 0x53980 | tests the simulation's stage at +0x1f54, then "PxScene::fetchResults: fetchResults() called illegally! …" |
| 62 | `fetchResultsStart(const PxContactPairHeader*&, PxU32&, bool)` | 0x53a50 | "PXScene::fetchResultsStart: fetchResultsStart() called illegally! …" |
| 64 | `fetchResultsFinish(PxU32*)` | 0x53d30 | calls only the fetch's last part (0x59730), which `fetchResults` (61) ends with; `fetchResultsStart` (62) shares its first part (0x59590) |
| 107 | `lockWrite(const char*, PxU32)` | 0x55180 | "PxScene::lockWrite() detected after a PxScene::lockRead(), lock upgrading is not supported …" |
| 108 | `unlockWrite()` | 0x55280 | "PxScene::unlockWrite() called without matching call to PxScene::lockWrite() …" |
| 116 | `shiftOrigin(const PxVec3&)` | 0x55360 | "PxScene::shiftOrigin() not allowed while simulation is running. …" |

What that settles:
- **Every slot is where NVIDIA's header puts it,** as our compiler lays it out: `TestAudit`'s `SceneWatchHooksTheGamesSlots`
  checks our side. The two `addActors` overloads sit in reverse order of declaration, as MSVC orders overloads, in the game
  and in our layout alike.
- **The calling convention:** on x64 a member function takes its object first, as a free function takes its first
  parameter, so a hook declared as a free function with the object first, then the declared parameters, calls the
  original correctly. `simulate` and `collide` take their time step in XMM1, which such a hook declares the same way. None
  of the 16 returns a struct, the one case where a member function passes its hidden return pointer differently.
- **The hooks check before they patch:** each slot must still hold the RVA above, plus where `PhysX3_x64.dll` is loaded.
  If another hook got there first, nothing is patched, and `scenewatch on` says which slot differs.

Still to audit before step 4: `PxPhysics::createRigidStatic` and `createShape`, `PxRigidActor::attachShape` and
`setGlobalPose`, `PxShape::setQueryFilterData`, `setSimulationFilterData` and `getMaterials`, `PxActor::getScene`, and
`PxBase::release` for actors and shapes.
