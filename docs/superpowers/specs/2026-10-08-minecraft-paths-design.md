# Minecraft's mobs finding their way in Night City: design (spike 9)

Date: 2026-10-08. Status: agreed with the user section by section in the brainstorm. "Found while writing", near the
end, lists what writing it out added to what was agreed.

## Purpose

This is spike 9 of the project that recreates [SkyCraft](https://github.com/chasmlol/SkyCraft) (Minecraft in Skyrim)
for Cyberpunk 2077. Night City shows Minecraft's entities (spike 7), and they collide with the city's ground: SkyCraft's
`BlockCollisionsMixin` adds its voxel copy of Night City (spike 5's exporter) to every block-collision query, so mobs
stand on the streets, can be pushed onto a kerb, and fall where the city has no floor. But they don't find their way on
it. In spike 7's step 0 a pig and a villager, their AI on, never moved in 33 s: Minecraft 26.3's pathfinding decides
where a mob can walk from block states, and under the city's ground there is only air. A chase is worse off still:
`GroundPathNavigation` looks down from its target for ground, finds none, and moves the target to the top of the world.

SkyCraft never handled this in Skyrim either: it turns mob spawning off, and nothing in it touches pathfinding.

The user asked for "Spike 9: Minecraft entity pathfinding on Cyberpunk terrain". The question:

> Can Minecraft's mobs find their way over Night City's ground as they do over blocks: wander, go to what they're
> after, keep up with V as pets, and fly around its walls, without seeing V through them?

## Decisions from the brainstorm

- **Scope: wandering, going to targets, and pets keeping up with V.**
  - Wandering: animals' strolls (`RandomStrollGoal` and its kin) and villagers' brain strolls.
  - Targets: a mob chasing Minecraft's player (V), following food it's tempted by, fleeing, or led on a lead.
  - Pets: wolves, cats and parrots following their owner, and teleporting to them when left behind.

  The user left out keeping mobs from falling into the void while the ground is missing (at Minecraft's start and at
  each new link, before the city's regions arrive).
- **Walkers and flyers.** Ground mobs and climbers, and flyers (allays, bees, parrots), whose straight-line shortcuts
  also check against the city, so they don't cut through its walls.
- **The city's walls block sight.** A mob's line of sight casts against the city's triangles, as arrows already do, so
  hostiles notice V only in view and skeletons don't shoot through buildings.
- **Approach: Minecraft's pathfinding reads the city's voxels.** CyberCraft Tweaks 1.7.0 adds a cell classifier and
  small mixins at the points where pathfinding reads the world; Minecraft's A*, goals, brains and movement run as
  written, as SkyCraft's collision mixin lets vanilla movement run against the voxels. The user chose it over:
  - **invisible real blocks under the city:** thousands of block changes a second as V moves, saved into the world,
    meshed by SkyCraft and sent to the game, collided with on top of SkyCraft's own collision, in the way of clicks, and
    to be removed at every epoch clear;
  - **paths from Cyberpunk's navmesh:** a channel from the game to Minecraft outside SkyCraft's protocol, only where the
    game has streamed its navmesh in, and at odds with the voxel collision the mobs move by.
- **Cells by their middle** (section 1): a cell is blocked only by what's in its middle, so walls off a cell's centre
  don't narrow streets and close doorways.

## Non-goals

- **Surviving reloads:** mobs left in the city fall into the void whenever its ground is missing (Minecraft's start, a
  new link, an epoch clear), as now.
- **Natural spawning:** SkyCraft turns it off. Mobs come from commands, spawn eggs and the overlay.
- **Sunburn under roofs:** zombies and skeletons burn by day anywhere in the city, with no Minecraft block overhead.
- **The game's moving things:** cars, kinematic doors and NPCs aren't in the city's collision (the exporter keeps only
  static shapes), so mobs walk through them, as they already would. NPCs' stand-ins are entities, and mobs push them.
- **Water:** Night City's water isn't in the collision. Swimmers are untouched.
- **Mobs with no pathfinding** (slimes, ghasts, phantoms, a blaze's flight) and **vexes** (no collision, through walls
  by design) stay as they are.
- **A parrot on V's shoulder:** a tamed parrot riding Minecraft's player isn't drawn in the game ("Pets keep up with
  V").
- **Mobs on the city's exact triangles:** they keep standing on the voxels, up to 1/8 block above the true surface, as
  in spike 7's run A, where their feet were on the street.
- **Faster strolling:** strolls succeed less often on the city than on blocks ("Wandering"); fixed only if step 0 finds
  it sluggish.
- **No change** to SkyCraft, the protocol, BlockRender's C++ or `mclink`'s code.

## Environment (checked on 2026-10-08)

### SkyCraft's Minecraft side (0.1.2)

From its source (session 818fd1fd's clone) and the CyberCraft instance:
- **`SkyCollision`** keeps the city's voxels in static maps, so the integrated server reads what the client's thread
  writes (one JVM):
  - `SHAPES`, a `ConcurrentHashMap` from a block position's long to its `VoxelShape`. A cell arrives as 8 longs, one per
    layer from the bottom, bit `x + 8 × z`. A full cell becomes `Shapes.block()`, any other a `CubeVoxelShape` over an
    8 × 8 × 8 `BitSetDiscreteVoxelShape`.
  - `shapeAt(pos)` returns null at once while the map is empty, and the cell's shape or null otherwise.
  - Its fill info (`groundTop`, `supportsFromBelow`) describes the whole cell, not its middle, so the classifier reads
    the shape itself.
  - The "SkyCraft collision" thread fills it from the collision ring. A region builds its new shapes first, then puts or
    removes each of its cells. Everything is cleared at a new epoch. Regions stay until then: the exporter forgets
    regions beyond its area only on its own side.
- **`BlockCollisionsMixin`** adds `shapeAt`'s shape to every block-collision query (`BlockCollisions.computeNext`),
  except for entities on the smooth collider, which is Minecraft's player. So every other entity collides with the
  voxels.
- **`SkyClip.cast(Vec3, Vec3)`** returns the nearest of the city's triangles the segment hits, or null. It gathers the
  triangles of every region the segment's box (grown by 0.01) touches, skips Skyrim's stair helpers, and uses a
  thread-local list. Arrows and the crosshair use it.
- **Spawning is off** (`SPAWN_MOBS`, `SPAWN_MONSTERS` and the rest), and **nothing in SkyCraft touches pathfinding.**

### The city's ground (spike 5's exporter)

- **Skins:** triangles voxelized at 1/8 block with SkyCraft's triangle-box test, so a street or a floor is a skin one
  voxel thick, with nothing solid under it. A wall is a skin too.
- **The area:** regions of 8 blocks, 5 out horizontally, 3 below and 2 above (88 m square, 48 m tall), centred on
  Minecraft's player. Static scene-query shapes only.
- **Seen in spike 7's run A:** a pig at y 6.0 on the street, V's feet on the pavement at 6.14.

### Minecraft 26.3

From its jar (`26.3-0.19.5.jar`, read with `javap`):
- **`WalkNodeEvaluator.getPathTypeFromState(BlockGetter, BlockPos)`** (protected static): air is `OPEN`; a block that
  isn't pathfindable on land is `BLOCKED`; it never answers `WALKABLE`. `PathfindingContext.getPathTypeFromState` calls
  it directly, or through the server's `PathTypeCache.getOrCompute`: 4,096 slots, one per position's hash, whose
  `invalidate(pos)` only `ServerLevel`'s block changes call.
- **`WalkNodeEvaluator.getPathTypeStatic(PathfindingContext, MutableBlockPos)`:** a cell that isn't `OPEN` keeps its
  type. An `OPEN` cell above `OPEN`, `WATER`, `LAVA` or `WALKABLE` stays `OPEN`; above anything else (`BLOCKED` among
  them) it's `WALKABLE`, and then its neighbours' dangers apply. The walk and fly evaluators use it, and so does a pet's
  teleport, through `getPathTypeStatic(Mob, BlockPos)`.
- **`WalkNodeEvaluator.getFloorLevel(BlockGetter, BlockPos)`** (public static): the top of the collision shape of the
  cell below, or that cell's bottom (`y − 1`) if it has none. A node is refused if its floor is more than
  `getMobJumpHeight()`, max(1.125, the mob's step height), above the last.
- **`WalkNodeEvaluator.getStart()`:** a mob on the ground starts at floor(y + 0.5). One in the air searches down while
  the cell is air or pathfindable, as far as the world's bottom, and starts above where it stops.
- **`PathNavigation.isStableDestination(pos)`:** `getBlockState(pos.below()).isSolidRender()`.
  **`FlyingPathNavigation`'s:** `getBlockState(pos).entityCanStandOn(…)`.
- **`GroundPathNavigation.createPath(BlockPos, int)`** goes through `findSurfacePosition(LevelChunk, BlockPos, int)`:
  - from an air cell, down while air; a block found means the cell above it;
  - nothing found means up while air, to the top of the world;
  - from a solid block, up while solid.

  `createPath(Entity, int)` passes the entity's block position.
- **`GoalUtils.isSolid(PathfinderMob, BlockPos)`:** the state's `isSolid()`. `LandRandomPos` lifts a candidate out of
  solid cells with it (`RandomPos.moveUpOutOfSolid`), and `HoverRandomPos` puts one above them (`moveUpToAboveSolid`).
- **Strolling:** `WaterAvoidingRandomStrollGoal` picks spots with `LandRandomPos.getPos(mob, 10, 7)`, and
  `RandomStrollGoal` tries about once every 120 ticks.
- **`PathNavigation.isClearForMovementBetween(Mob, Vec3, Vec3, boolean)`** (protected static): one `Level.clip`
  (`COLLIDER`) to the target at half the mob's height. It's behind `FlyingPathNavigation.canMoveDirectly`.
- **`LivingEntity.hasLineOfSight(Entity)`** calls `hasLineOfSight(Entity, ClipContext.Block, ClipContext.Fluid,
  double)`, which clips once, eye to eye, within 128 blocks.
- **`MoveControl.tick`** jumps when the wanted y is more than the mob's step height above it and it's within
  max(1, its width) horizontally, or when its own block's collision rises above its feet.
- **`TamableAnimal`:**
  - `shouldTryTeleportToOwner`: 12 blocks away (distance² ≥ 144);
  - `teleportToAroundBlockPos`: 10 tries, x and z within ±3 but not both within ±1, y within ±1;
  - `canTeleportTo(pos)`: `getPathTypeStatic` is `WALKABLE`, no leaves below unless it can fly to its owner, and
    `noCollision` for its box moved by (pos − its own block position), so its current fraction of a block carries over;
  - `maybeTeleportTo` then `snapTo(x + 0.5, y, z + 0.5)`, at the cell's bottom.

  A wolf's `FollowOwnerGoal` starts at 10 blocks.
- **`Zombie`** targets players with `NearestAttackableTargetGoal(…, Player.class, true)`, so only those it sees.
  `Husk.isSunSensitive` is false.
- **`VoxelShape.max(Axis.Y, a, b)`** is the column's top at z = a, x = b (`AxisCycle`: forward from Y is Z, backward
  X), and −∞ for an empty column.

### The CyberCraft instance

- **Mods:** Fabric API 0.161.0, SkyCraft 0.1.2, CyberCraft Tweaks 1.6.0, Mouse Tweaks, e4mc, Entity Model Features,
  Entity Texture Features, and a datapack. Nothing that changes pathfinding (no Lithium).
- **EMF and ETF** have no `.disabled` on their jars, though spike 7 left them disabled. Step 0 asks before anything is
  installed.

### CyberCraft Tweaks today (1.6.0)

- **Environment `client`,** which takes in the integrated server: one JVM.
- **Mixins:**
  - common: `NoBlastBlocks`, `NoDigging`, `SetHealth`;
  - client: `KeepDrawOrder`, `MarkEmissive`, `MarkBlended`;
  - `defaultRequire: 1`.
- **Pinned to SkyCraft 0.1.2.**
- **Built by `build.sh`** with `javac` against the Modrinth App's jars. It has no tests.

## 1. The city's cells

**`CityCells`** classifies a block position where Minecraft has air (`isAir`) and SkyCraft has a shape
(`SkyCollision.shapeAt`). It reads the top of each of the shape's 8 × 8 voxel columns, `max(Axis.Y, z, x)` at the
column's centre, (i + 0.5) / 8, and goes by the cell's middle: the central 6 × 6 columns (0.125 to 0.875 on x and z),
where a mob up to 0.75 wide stands at the cell's centre. The cell's top is the highest of those 36, and the cell is:
- **none:** no column in the middle has anything, for example only a wall's face along the cell's edge. Pathfinding sees
  air there, as now.
- **floor:** the top is at most 0.5. The cell itself is walkable, its floor at y + top: a street's skin, a kerb, a
  stair's tread.
- **blocked:** the top is above 0.5. The cell counts as a full block, and the cell above stands on it, its floor at
  y + top.

Minecraft's own blocks always win: the city answers only where Minecraft's state is air. The answer depends on the
shape alone, so it's cached per shape object, held weakly so that old shapes go with their regions. SkyCraft builds new
shape objects whenever a region is sent again, and full cells share `Shapes.block()` (blocked, top 1).

**A cell's floor height** (`floorAt`), used by floor levels and pets' landings:
- a floor cell: its y + top;
- a cell above a blocked or floor city cell: that cell's y + top;
- otherwise none, and vanilla's answer stands.

**Why floor cells.** Vanilla's rule is "air above a solid block", and the city's skins have nothing solid under them.
Reporting a skin's own cell as walkable is what makes the rest of vanilla work: `getPathTypeStatic` makes the cell above
a `WALKABLE` cell `OPEN`, so mobs step from floor to floor and jump onto blocked cells as onto blocks.

**Why the middle.** Night City's walls rarely lie on cell boundaries. If a wall anywhere in a cell blocked it, every
street would lose a block on each side, and doorways of 1 to 2 m would often close.

**A known look:** mobs may hop onto railings and low walls up to about a block high, as vanilla mobs do onto iron bars
and glass panes, since a thin top can be stood on.

The rules on the 64 column tops (the middle, the kinds, the top, the floor height) live in a class with no Minecraft
types, `CellRule`, which `CityCells` calls and the unit tests check.

## 2. Walkers: wandering and going to targets

Each mixin asks `CityCells` only where Minecraft has air:
1. **Path types** (`WalkNodeEvaluator.getPathTypeFromState`): a floor cell answers `WALKABLE`, a blocked cell
   `BLOCKED`. The walk and fly evaluators read through it, and so does a pet's teleport check.
2. **Floor heights** (`WalkNodeEvaluator.getFloorLevel`): the cell's `floorAt`, where it has one. Without it a street's
   skin gives y − 1, and the jump check would refuse steps up onto kerbs and blocked cells.
3. **The shared path-type cache** (`PathTypeCache.getOrCompute`): a cell with a city shape skips the cache and is
   answered fresh. An entry made before its region arrived would stay as air otherwise: the cache is cleared only on
   Minecraft's block changes. Cells without a shape cache as now. A city answer is never cached, so when a shape goes,
   whatever the cache holds for its cell is vanilla's air.
4. **Stroll targets** (`PathNavigation.isStableDestination`): also stable is a floor cell, or a cell above a blocked
   city cell. `LandRandomPos`, `DefaultRandomPos` and villagers' brain strolls reject unstable spots.
5. **Path targets** (`GroundPathNavigation.findSurfacePosition`): searching down from a target in the air stops at the
   first floor cell (the target becomes it) or blocked cell (the cell above it). A target in a floor cell stays, as one
   in a carpet does, and a target in a blocked cell moves up out of it, as out of a solid block. Chasing, tempting and
   leads then aim at the ground, not the top of the world.
6. **Solid spots for random positions** (`GoalUtils.isSolid`): a blocked cell is solid, so a candidate inside one is
   lifted out of it.
7. **Airborne starts** (`WalkNodeEvaluator.getStart`): a walker in the air searches down for its start, and that search
   stops at a city cell: a floor cell is the start, and a blocked cell's start is the cell above. Today a mob that
   repaths mid-jump starts at the world's bottom and loses its path.

Nothing else changes: Minecraft's A*, goals, brains, and `MoveControl`'s jumps and step-ups. A mob on a floor cell aims
at the next node's integer y. `MoveControl` jumps when that's more than its step height (0.6) above its feet, and when
it isn't, the real rise is at most that, and a step-up takes it.

### Wandering

Strolls succeed less often on the city than on blocks. On blocks, a random candidate inside the ground is lifted to the
surface, so about half of them land. Under a street there's nothing solid, so only candidates in the floor's own cell
count: about 1 in 15 of them on flat ground (y within ±7), and so about half of a stroll's 10-candidate tries. Step 0
measures strolls a minute. If wandering looks sluggish, snapping each candidate's column to the nearest floor is the
fix, and isn't part of this design.

## 3. Pets, flyers and sight

### Pets keep up with V

Wolves, cats and parrots follow by path at 10 blocks with nothing beyond section 2. Teleporting at 12 blocks needs one
mixin on `TamableAnimal`:
- **The room check** (`canTeleportTo`) puts the pet's box exactly at the cell's `floorAt`, not moved by whole blocks.
- **The landing** (`maybeTeleportTo`) snaps it to the same height.

A cell with no `floorAt` keeps vanilla's check and landing. With V on a flat street, the tries at V's own height land
on the street's floor cells. If none of the 10 tries finds room, the pet tries again the next tick, as vanilla does.

A tamed parrot also rides its owner's shoulder once it's been off for 5 s (`LandOnOwnersShoulderGoal`, while the owner
isn't flying, in water or spectating). There it's part of Minecraft's player, which the game doesn't draw, so it
vanishes from Night City until Minecraft drops it: a fall of more than half a block, water, flying, sleeping, powder
snow, a hurt, a game-mode change or death (`ServerPlayer.handleShoulderEntities`, `hurtServer`, `setGameMode`, `die`).
That's left as it is ("Non-goals"), and the tests' flyer is a bee.

### Flyers

Allays, bees and parrots:
- **Walls:** their paths see the city's walls through section 2's path types.
- **Landing spots** (`FlyingPathNavigation.isStableDestination`): a blocked city cell is one a flyer can stand on.
- **Hovering** (`HoverRandomPos`, bees and allays): through section 2's `GoalUtils.isSolid`.
- **Shortcuts** (`PathNavigation.isClearForMovementBetween`, behind flyers' `canMoveDirectly`): when vanilla's clip is
  clear, the same segment is cast against the city ("Casting"), and a hit means no shortcut. A flyer then follows its
  path's nodes around the wall.

### Sight

`LivingEntity.hasLineOfSight(Entity, ClipContext.Block, ClipContext.Fluid, double)`: when vanilla's clip says clear,
the same eye-to-eye segment is cast against the city, and a hit means no sight. That covers what's built on it:
targeting (`TargetingConditions`, `Sensing`), skeletons and pillagers shooting, creepers swelling, and brains'
visible-entity sensors. Triangles, as arrows use, so a mob's sight matches what V sees.

### Casting

Both casts use `SkyClip.cast`, over the segment in pieces of at most 8 blocks, nearest first, stopping at the first
hit. A region holds 1,000 to 2,500 triangles (spike 5's Task 46), and one box around a long ray would gather tens of
thousands.

## 4. Cost and the log

Once a minute in which the mod answered anything from the city, it writes one line to Minecraft's log:
- **cells:** city answers given and shapes classified;
- **casts:** sight and shortcut casts, the hits among them, and the milliseconds they took;
- **teleports:** pets landed on the city's floor.

Step 0 and run F read the cost from it.

## 5. Edge cases

- **No city where a mob is:** outside the exporter's area, before its regions arrive, after an epoch clear. The cells
  answer none, so pathfinding behaves as now: nothing is walkable, mobs stand still, and paths fail. A path can't lead
  a mob off the known ground, since nothing beyond it is walkable; a knockback or a blast can, and the mob falls into
  the void, as now.
- **Outside the city:** in a plain Minecraft world, where SkyCraft has received no region, `shapeAt` returns null at
  once, and every read gives vanilla's answer. After the link ends, SkyCraft keeps the city's voxels until its next
  epoch, and mobs keep finding their way on them, as they keep colliding with them.
- **A Minecraft block on the city:** a block the player places on a street is real, and vanilla answers for its cell and
  the cell above it.
- **The ground changing under a path:** new shape objects come with each region sent again, so the per-shape cache
  never answers for an old one. A path already made stays until vanilla recomputes it; if the mob meets new geometry,
  vanilla's stuck detection repaths it.
- **Threads:** SkyCraft's collision thread writes its maps, and the server thread reads them. A region is swapped a cell
  at a time, so a path can see a region half old and half new for one computation: harmless, and SkyCraft's collision
  has the same window. Only the server thread asks for cells (mobs' AI runs there; sight uses the triangles, not the
  cells), and the shape cache is guarded anyway, at little cost on one thread.
- **Mobs wider than a block** (spiders, iron golems): vanilla checks every cell under their box, each by its middle, so
  such a mob may try a gap it can't fit; collision stops it, and vanilla's stuck detection repaths.
- **Versions:** every injection is required (`defaultRequire: 1`), so a Minecraft or SkyCraft update that moves a
  target fails at Minecraft's start, its log naming it. `fabric.mod.json` already pins SkyCraft 0.1.2; the mod's new
  calls into it are `SkyCollision.shapeAt` and `SkyClip.cast`.
- **Hostiles ignore creative players,** as vanilla does, so the chase tests run in survival.

## Versions and files

- **CyberCraft Tweaks 1.7.0** (`minecraft/cybercraft-tweaks`):
  - `CellRule` (new, no Minecraft types): the kinds and tops from 36 column tops, a cell's floor height, a segment's
    8-block pieces;
  - `CityCells` (new): `SkyCollision.shapeAt` to `CellRule`, the per-shape cache, `floorAt`;
  - `PathStats` (new): the counters and the minute line;
  - common mixins (new): `CityPathTypes` (`WalkNodeEvaluator`: path types, floor levels, airborne starts),
    `CityPathCache` (`PathTypeCache`), `CityDestinations` (both `isStableDestination`s, `findSurfacePosition`,
    `GoalUtils.isSolid`), `CityPetTeleport` (`TamableAnimal`), `CitySight` (`hasLineOfSight`,
    `isClearForMovementBetween`);
  - `fabric.mod.json`: 1.7.0, its description; the README: a section on finding the way;
  - `test.sh` (new) and `src/test/java`: the unit tests, compiled with `javac` and run with plain `java`.
- **`mclink`:** `tools/mclink/paths.txt`, step 0's script.
- **The overlay** (`blockrender_hud/init.lua`): a **Paths** Spawn row.
- **BlockRender's C++** and **SkyCraft** stay as they are.

## Testing

Each layer can fail, and each new test is broken once to prove it.

### Unit tests (`test.sh`, no Minecraft)

A small harness that counts failed checks, as the C++ tests' does, with no JUnit:
- **`CellRule`'s kinds:** nothing in the middle with walls along the edge columns (none), skins at layers 0 to 3
  (floor), 4 to 7 (blocked), a floor with a beam above it (blocked), a wall through the middle (blocked), a full cell
  (blocked, top 1);
- **floor heights:** a floor cell, above a blocked cell, above a floor cell, neither;
- **pieces:** short segments whole, long ones in 8-block pieces that meet end to end, zero length.

### Step 0: Minecraft's side, with `mclink`, before any game session

The user starts Minecraft, and `mclink` runs `tools/mclink/paths.txt` on spike 1's dumped ground at a fresh spot east
of spike 7's and 8's (x 880), with spots for each check picked from the dumps while planning (`view_dump.py`).
Everything summoned is tagged `sp9`, with no loot (`DeathLootTable:"minecraft:empty"`) and `PersistenceRequired`. The
results come from chat replies in Minecraft's log (`/data get entity … Pos`, `/execute if entity …,distance=..N`) and
`scene`'s shadow positions:
- **Wander:** a pig, a cow and a villager, positions every 5 s for 60 s. Each moves 3 or more blocks and strolls at
  least twice, stays on the ground (`OnGround:1b`, never below the street), and nobody walks off the raised level's
  6 to 9 m drop.
- **Tempt:** Minecraft's player holds wheat, and the cow follows as the stand-in V walks about 10 blocks.
- **Pet:** a wolf whose `Owner` is set from the player's UUID (`/data modify … Owner set from entity @p UUID`) follows
  by path as the stand-in V walks 8 blocks. Then `at` moves V 20 blocks: within 3 s the wolf is within 4 blocks of V, on
  the ground, its y at a floor and not inside a skin.
- **Chase and sight:** in survival, a husk in view 10 blocks off reaches the player; a husk behind a wall doesn't come
  within 10 s.
- **Flyer:** a bee, tempted by a dandelion in the player's off hand (`TemptGoal` on `BEE_FOOD`), follows the stand-in
  V over the drop and past a wall without sticking in it.
- **Cost:** the mod's minute line.
- **Clear:** `/kill @e[tag=sp9]`, then `scene`'s world says `nothing`. The player's game mode, health and held item
  are put back as they were.

It runs **twice**. First with 1.6.0, still installed, as the control: wander, tempt, chase and the wolf's teleport
should fail, which proves the checks can tell. Then with 1.7.0, everything passing. Installing 1.7.0 needs Minecraft
stopped and the user's leave; 1.6.0 is renamed `.jar.disabled`.

### Run F, in the game

After spike 8's run E. CyberCraft Tweaks 1.7.0 installed, and the overlay's Paths row deployed while the game is
closed (a Lua file). The row summons, ahead of V:
- **wanderers:** a pig, a cow, a villager;
- **a pet:** a wolf, owned by Minecraft's player;
- **a flyer:** a bee;
- **a husk;**
- **wheat and a dandelion,** given to the player.

They're tagged `sp7`, so the existing Clear removes them. With the user, at spots they pick (a sidewalk with kerbs,
stairs, a doorway or an alley, a railing):
1. The wanderers stroll the sidewalks and streets, step kerbs, and stay out of walls and off drops.
2. The cow follows the wheat.
3. The wolf follows, and after V sprints away it lands beside V: the overlay's nearest-mob distance shows it.
4. The bee follows the dandelion without sticking in walls.
5. With combat off (Minecraft's hits and deaths stay in Minecraft) and Minecraft's player in survival, the husk chases V
   in view, and doesn't notice V behind a building.
6. The frame rate is as before, and Minecraft's minute line stays modest.

Clear ends it, and the results doc gets screenshots, as earlier runs' did.

## Risks

1. **Skins on a cell boundary:** a surface lying exactly on one may fill the top layer of the cell below and the bottom
   layer of the cell above. The cell below is then blocked and the cell above a floor 1/8 block up, which is where
   collision puts a mob anyway.
2. **Slopes and stairs:** a ramp crosses cells, so its cells come out floor or blocked by where it is in each, and the
   path climbs in nodes, a step-up or a jump at a time. Night City's stairs (treads about 0.2 m) are step-ups.
3. **A wall just off a cell's middle** isn't seen there, so a mob passing off-centre scrapes it and slides; a gap
   narrower than the middle (0.75 block) is closed.
4. **Sight's cost:** casts against tens of thousands of triangles for long rays, in 8-block pieces. Step 0 and run F
   measure it in the minute line.
5. **Strolls less often** than on blocks ("Wandering").
6. **`getStart`'s search** is changed for walkers in the air only; a mob on the ground starts as before.
7. **Cells with a shape skip the shared cache:** a path reads each cell once (the evaluator keeps its own cache for the
   path), so the cost is a map lookup and a cached classification per cell.

## Found while writing

- **`VoxelShape.max(Axis.Y, a, b)` takes z first,** then x (`AxisCycle`), and gives −∞ for an empty column.
- **`PathTypeCache` computes through `WalkNodeEvaluator.getPathTypeFromState`,** so one mixin covers the cached and the
  uncached reads; the cache's own mixin only keeps city cells out of it.
- **`getPathTypeStatic` returns a cell's own type when it isn't `OPEN`,** so a floor cell answered `WALKABLE` skips the
  neighbours' danger check: the city has no fire or cactus, so nothing is lost.
- **`findSurfacePosition` from a solid target moves up while solid,** so a target inside a blocked city cell goes up
  out of it, as from a block.
- **The pets' room check carries the pet's current fraction of a block** (its box moved by whole blocks), which is why
  the check, not only the landing, moves to the floor's height.
- **Run F tags its mobs `sp7`** so spike 7's Clear covers them; step 0 tags `sp9`.
- **A tamed parrot would leave the tests:** it rides its owner's shoulder after 5 s, where `/kill @e[tag=…]` can't reach
  it and the game doesn't draw it. The brainstorm's tests had a parrot as the flyer; a bee tempted by a flower takes its
  place, and parrots on V's shoulder became a non-goal.
- **`CellRule` takes all 64 column tops,** not the middle 36, so the unit tests can check that walls along a cell's
  edge are left out.
