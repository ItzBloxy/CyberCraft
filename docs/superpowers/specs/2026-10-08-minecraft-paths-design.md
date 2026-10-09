# Minecraft's mobs finding their way in Night City: design (spike 9)

Date: 2026-10-08. Status: agreed with the user section by section in the brainstorm. "Found while writing", near the
end, lists what writing it out added to what was agreed. Then revised after a deep self-review at the user's request,
which checked each claim against its source (Minecraft 26.3's bytecode, SkyCraft's source, BlockRender's voxelizer,
the installed mods and the world's files) and simulated step 0's ground on spike 1's dumps with BlockRender's own
voxelizer: "Changes from the deep review", at the end, lists what changed and what was checked. Planning Part 1 (the
mod and step 0) settled more: "Changes while planning Part 1". The sections above them include those changes.

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
- **Amphibians and striders' strolls:** frogs, turtles and axolotls (`AmphibiousPathNavigation`,
  `Turtle$TurtlePathNavigation`) and striders (`Strider$StriderPathNavigation`) have their own stable-spot checks, which
  read Minecraft's blocks. They get the city's path types (a frog's long jumps among them) but not the stroll fix.
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
    removes each of its cells. Only a `COL_CLEAR` message (a new epoch) empties it; the exporter forgets regions beyond
    its area only on its own side.
- **`BlockCollisionsMixin`** adds `shapeAt`'s shape to every block-collision query (`BlockCollisions.computeNext`),
  except for entities on the smooth collider, which is Minecraft's player. So every other entity collides with the
  voxels.
- **`SkyClip.cast(Vec3, Vec3)`** returns the nearest of the city's triangles the segment hits, or null. It gathers the
  triangles of every region the segment's box (grown by 0.01) touches, skips Skyrim's stair helpers, and uses a
  thread-local list. Arrows and the crosshair use it.
- **Spawning is off** (`SPAWN_MOBS`, `SPAWN_MONSTERS` and the rest), and **nothing in SkyCraft touches pathfinding,**
  sight or pets.

### The city's ground (spike 5's exporter, `link/Voxels.cpp`)

- **Skins:** each triangle fills the 1/8-block sub-voxels it passes through (Akenine-Möller's triangle-box test), so a
  street, a floor or a wall is a skin a voxel or two thick, with nothing solid under or behind it.
- **Floors on whole blocks fill both voxels they touch** (Night City's geometry is often on whole metres). Such a street
  makes the cell below it blocked (its top layer) and leaves its own cell either empty or a thin floor.
- **Surfaces 50 to 84° from flat** (`|n.y|` from 0.1 to 0.643) are coarsened to whole-block footprints between their
  lowest and highest layers, so their risers exceed Minecraft's 0.6 step height.
- **The area:** regions of 8 blocks, 5 out horizontally, 3 below and 2 above (88 m square, 48 m tall), centred on
  Minecraft's player. Static scene-query shapes only.
- **Seen in spike 7's run A:** a pig at y 6.0 on the street, V's feet on the pavement at 6.14.

### Minecraft 26.3

From its jar (`26.3-0.19.5.jar`, read with `javap`):
- **`WalkNodeEvaluator.getPathTypeFromState(BlockGetter, BlockPos)`** (protected static): air is `OPEN` before
  anything else; a block that isn't pathfindable on land is `BLOCKED`; it never answers `WALKABLE`.
  `PathfindingContext.getPathTypeFromState` calls it directly, or through the server's `PathTypeCache.getOrCompute`:
  4,096 slots, one per position's hash, whose `invalidate(pos)` only `ServerLevel.sendBlockUpdated` calls.
- **`WalkNodeEvaluator.getPathTypeStatic(PathfindingContext, MutableBlockPos)`:** a cell that isn't `OPEN` keeps its
  type. An `OPEN` cell above `OPEN`, `WATER`, `LAVA` or `WALKABLE` stays `OPEN`; above anything else (`BLOCKED` among
  them) it's `WALKABLE`, and then its neighbours' dangers apply. The walk evaluator uses it; so do, through
  `getPathTypeStatic(Mob, BlockPos)`, a pet's teleport, `GoalUtils.hasMalus`, goats' rams and frogs' and goats' long
  jumps. **`FlyNodeEvaluator.getPathType`** has its own copy of the rule, with the same outcome for `OPEN`, `WALKABLE`
  and `BLOCKED`.
- **`WalkNodeEvaluator.getFloorLevel(BlockGetter, BlockPos)`** (public static): the top of the collision shape of the
  cell below, or that cell's bottom (`y − 1`) if it has none. A node is refused if its floor is more than
  `getMobJumpHeight()`, max(1.125, the mob's step height), above the last. Only `WalkNodeEvaluator`,
  `AmphibiousNodeEvaluator` and `PathNavigation` call it.
- **`WalkNodeEvaluator.getStart()`:** a mob on the ground starts at floor(y + 0.5). One in the air searches down from
  the cell above its feet while the cell below is air or pathfindable, as far as the world's bottom, and starts above
  where it stops. `canStartAt` refuses an `OPEN` start, but `getStart` then tries the box's corners and falls back to
  the start anyway, and A* drops from an open node to the ground below it.
- **`PathNavigation.isStableDestination(pos)`:** `getBlockState(pos.below()).isSolidRender()`. Only
  `GoalUtils.isNotStable` (from `LandRandomPos` and `DefaultRandomPos`) and goats' `PrepareRamNearestTarget` call it.
  `FlyingPathNavigation`, a bee's own navigation (`Bee$1`), amphibians', turtles' and striders' override it.
- **`GroundPathNavigation.createPath(BlockPos, int)`** goes through `findSurfacePosition(LevelChunk, BlockPos, int)`:
  - from an air cell, down while air; a block found means the cell above it;
  - nothing found means up while air, to the top of the world;
  - from a solid block, up while solid.

  `createPath(Entity, int)` passes the entity's block position.
- **`GoalUtils.isSolid(PathfinderMob, BlockPos)`:** the state's `isSolid()`. `LandRandomPos` checks a candidate's
  stability (`isNotStable`) **before** lifting it out of solid cells (`movePosUpOutOfSolid`), then refuses it if it has
  a malus (`hasMalus`). `HoverRandomPos` puts a candidate above solid cells (`moveUpToAboveSolid`).
- **Strolling:** `WaterAvoidingRandomStrollGoal` picks spots with `LandRandomPos.getPos(mob, 10, 7)`, and
  `RandomStrollGoal` tries about once every 120 ticks. Villagers' brain strolls (`RandomStroll`) use `LandRandomPos`.
  Flyers wander with `HoverRandomPos` and `AirAndWaterRandomPos` (`WaterAvoidingRandomFlyingGoal`, `BeeWanderGoal`,
  `RandomStroll.fly`), which never ask for a stable spot.
- **`PathNavigation.isClearForMovementBetween(Mob, Vec3, Vec3, boolean)`** (protected static): one `Level.clip`
  (`COLLIDER`) to the target at half the mob's height. It's behind `FlyingPathNavigation.canMoveDirectly`.
- **`LivingEntity.hasLineOfSight(Entity)`** calls `hasLineOfSight(Entity, ClipContext.Block, ClipContext.Fluid,
  double)`, which clips once, eye to eye, within 128 blocks. `Sensing.hasLineOfSight` calls it, and
  `TargetingConditions.test` calls `Sensing` whenever it checks line of sight; so do the melee, bow, crossbow and
  ranged attack goals, `SwellGoal`, `TargetGoal`'s keeping of a target, and `NearestItemSensor`.
- **`MoveControl.tick`** jumps when the wanted y is more than the mob's step height above it and it's within
  max(1, its width) horizontally, or when its own block's collision rises above its feet.
- **`VoxelShape.collideX`** stops a moving box only at voxels beyond its face in the direction of travel
  (`findIndex(min + 1e-7)` and below, or `max − 1e-7` and above); voxels already inside the box are ignored.
- **`TamableAnimal`:**
  - `shouldTryTeleportToOwner`: 12 blocks away (distance² ≥ 144);
  - `teleportToAroundBlockPos`: 10 tries, x and z within ±3 but not both within ±1, y within ±1;
  - `canTeleportTo(pos)`: `getPathTypeStatic` is `WALKABLE`, no leaves below unless it can fly to its owner, and
    `noCollision` for its box moved by (pos − its own block position), so its current fraction of a block carries over;
  - `maybeTeleportTo` then `snapTo(x + 0.5, y, z + 0.5)`, at the cell's bottom;
  - `readAdditionalSaveData` reads `Owner` and makes the animal tame.

  `FollowOwnerGoal` starts at 10 blocks for wolves (stopping at 2) and cats (stopping at 5), at 5 for parrots.
- **`Zombie`** targets players with `NearestAttackableTargetGoal(…, Player.class, true)`, so only those it sees.
  `Husk.isSunSensitive` is false. Pillagers target players they see (and villagers unseen), and shoot with
  `RangedCrossbowAttackGoal` only when they see their target.
- **`VoxelShape.max(Axis.Y, a, b)`** is the column's top at z = a, x = b (`AxisCycle`: forward from Y is Z, backward
  X), and −∞ for an empty column.

### The CyberCraft instance and its world

- **Mods:** Fabric API 0.161.0, SkyCraft 0.1.2, CyberCraft Tweaks 1.6.0, Mouse Tweaks, e4mc, Entity Model Features,
  Entity Texture Features, and a datapack. Nothing that changes pathfinding (no Lithium); none mixes into sight, pets or
  navigation.
- **Fabric API's content registries** mix into two of the same methods, for blocks registered with a path type:
  - `WalkNodeEvaluator.getPathTypeFromState`, at its `BlockState.getBlock()` calls, which come after air's `OPEN`;
  - `PathfindingContext.getPathTypeFromState`, which answers a registered block before the cache.

  Neither acts on air or on the city's cells, so the two compose.
- **EMF and ETF** have no `.disabled` on their jars, though spike 7 left them disabled. Step 0 asks before anything is
  installed.
- **The `SkyCraft` world's clock is fixed at noon** (`world_clocks.dat`: 6,000 ticks; `advance_time` off), with no rain
  (`weather.dat`), and `spawn_mobs`, `spawn_monsters` and `advance_weather` off (`game_rules.dat`). At noon a villager
  without a job site stays idle and strolls, bees are out of their hives, and zombies and skeletons burn.

### CyberCraft Tweaks today (1.6.0)

- **Environment `client`,** which takes in the integrated server: one JVM.
- **Mixins:**
  - common: `NoBlastBlocks`, `NoDigging`, `SetHealth`;
  - client: `KeepDrawOrder`, `MarkEmissive`, `MarkBlended`;
  - `defaultRequire: 1`.
- **Pinned to SkyCraft 0.1.2.**
- **Built by `build.sh`** with `javac` against the Modrinth App's jars. It has no tests.

## 1. The city's cells

**`CityCells`** classifies a block position where SkyCraft has a shape (`SkyCollision.shapeAt`). It reads the top of
each of the shape's 8 × 8 voxel columns, `max(Axis.Y, z, x)` at the column's centre, (i + 0.5) / 8, and goes by the
cell's middle: the central 6 × 6 columns (0.125 to 0.875 on x and z), where a mob up to 0.75 wide stands at the cell's
centre. The cell's top is the highest of those 36, and the cell is:
- **none:** no column in the middle has anything, for example only a wall's face along the cell's edge, or a sliver of
  rock in a corner. Pathfinding sees air there, as now.
- **floor:** the top is at most 0.5. The cell itself is walkable, its floor at y + top: a street's skin, a kerb, a
  stair's tread.
- **blocked:** the top is above 0.5. The cell counts as a full block, and the cell above stands on it, its floor at
  y + top.
- **fence** (from 1.7.1, "Changes while executing Part 1"): fewer than half (18) of the middle's 36 columns are
  covered, a column counting as covered if the cell has anything in it or the cell below reaches into its upper half
  there: a railing, a barrier, a pole, a floor's last sliver over a drop. It's impassable, solid for random positions
  and never a stroll target, and the cell above stands half a block above its top, as on a Minecraft fence, so no mob
  jumps onto it. A kerb's cell, part pavement and part the street's top in the cell below, stays a floor.

The answer depends on the shape and the one below it, so each shape's 64 column tops are cached per shape object, held
weakly so that old shapes go with their regions. SkyCraft builds new shape objects whenever a region is sent again, and
full cells share `Shapes.block()` (blocked, top 1).

**Minecraft's blocks win.** The city answers only where Minecraft's own answer is open air:
- for path types, where vanilla answers `OPEN`: air, or a block mobs pass through, such as a flower or a torch placed on
  a street, which would otherwise hide the floor under it;
- for everything else (floor heights, stable spots, solid spots, path targets, starts), in a cell whose Minecraft block
  has no collision.

**A cell's floor height** (`floorAt`), used by floor levels and pets' landings:
- a floor cell: its y + top;
- a cell above a blocked or floor city cell: that cell's y + top; above a fence, half a block higher;
- otherwise none, and vanilla's answer stands.

**Why floor cells.** Vanilla's rule is "air above a solid block", and the city's skins have nothing solid under them.
Reporting a skin's own cell as walkable is what makes the rest of vanilla work: `getPathTypeStatic` makes the cell above
a `WALKABLE` cell `OPEN`, so mobs step from floor to floor and jump onto blocked cells as onto blocks. Where a floor
lies on a whole block, the voxelizer makes the cell below it blocked, and vanilla's own rule makes the cell above
walkable: on spike 1's raised level, two thirds of the walkable places are of that kind ("Changes from the deep
review").

**Why the middle.** Night City's walls rarely lie on cell boundaries. If a wall anywhere in a cell blocked it, every
street would lose a block on each side, and doorways of 1 to 2 m would often close. And a cell holding only a sliver
at its edge or corner would become a foothold no mob at the cell's centre has: on spike 1's raised level, such slivers
let a whole-cell rule's mobs climb down the rock face beside the road in falls of up to 3 blocks, which the middle rule
refuses.

**A known look, until 1.7.1:** mobs could hop onto railings and low walls up to about a block high, as vanilla mobs do
onto iron bars and glass panes, since a thin top could be stood on. The raised level's barrier is one; step 0 saw mobs
walk along it and drop off its far side, and from 1.7.1 such cells are fences. Wide wall tops stay walkable.

The rules on the 64 column tops (the middle, the kinds, the top, the floor height) live in a class with no Minecraft
types, `CellRule`, which `CityCells` calls and the unit tests check.

## 2. Walkers: wandering and going to targets

Each mixin asks `CityCells` only where Minecraft's own answer is open air (section 1):
1. **Path types** (`WalkNodeEvaluator.getPathTypeFromState`): a floor cell answers `WALKABLE`, a blocked cell
   `BLOCKED`. The walk and fly evaluators read through it, and so do a pet's teleport check and `GoalUtils.hasMalus`.
2. **Floor heights** (`WalkNodeEvaluator.getFloorLevel`): the cell's `floorAt`, where it has one. Without it any node on
   the city gives y − 1, since Minecraft has air below it, and the jump check would refuse steps up onto kerbs and
   blocked cells.
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
6. **Solid spots for random positions** (`GoalUtils.isSolid`): a blocked cell is solid, so a hovering flyer's spot goes
   above it, and a walker's candidate inside thick geometry is lifted out of it.
7. **Airborne starts** (`WalkNodeEvaluator.getStart`): a walker in the air searches down for its start, and that search
   stops at the first city cell, floor or blocked, and starts above it, as above a block. Above a floor cell that start
   is open, which `getStart` falls back to and A* drops from (as for a mob standing on a floor whose top is exactly
   0.5). Today a mob that repaths mid-jump starts at the world's bottom and loses its path.

Nothing else changes: Minecraft's A*, goals, brains, and `MoveControl`'s jumps and step-ups. A mob on a floor cell aims
at the next node's integer y. `MoveControl` jumps when that's more than its step height (0.6) above its feet, and when
it isn't, the real rise is at most that, and a step-up takes it.

### Wandering

Strolls succeed less often on the city than on blocks. `LandRandomPos` asks that a random candidate be stable where it
falls, before lifting it out of anything solid. On flat blocks 8 of its 15 heights are stable (inside the ground, or
just above it), so a stroll's 10 candidates nearly always find one. On the city only a floor cell, or a cell just above
a blocked one, is stable, and the cells under a street's skin aren't. Simulated on spike 1's dumps, a stroll found a
spot 72% of the time on the raised level and 66% on the ramp, so mobs stroll about a third less often. Step 0 measures
strolls a minute; if wandering looks sluggish, snapping each candidate's column to the nearest floor is the fix, and
isn't part of this design.

## 3. Pets, flyers and sight

### Pets keep up with V

Wolves and cats follow by path at 10 blocks, parrots at 5, with nothing beyond section 2. Teleporting at 12 blocks
needs one mixin on `TamableAnimal`:
- **The room check** (`canTeleportTo`) puts the pet's box exactly at the cell's `floorAt`, not moved by whole blocks.
- **The landing** (`maybeTeleportTo`) snaps it to the same height, and resets its fall distance (from 1.7.1): a pet
  teleported while falling would otherwise take the whole fall's damage as it lands.

A cell with no `floorAt` keeps vanilla's check and landing. Vanilla's landing at a cell's bottom would put a pet inside
a floor cell's skin, and Minecraft's collision ignores voxels already inside a box (`VoxelShape.collideX`), so the pet
would fall through the street; a third of the walkable places on spike 1's raised level are floor cells. Simulated
there, one tick's 10 tries found room for 97% of V's positions on the road (95% on the ramp); the pet tries again each
tick, as vanilla does.

A tamed parrot also rides its owner's shoulder once it's been off for 5 s (`LandOnOwnersShoulderGoal`, while the owner
isn't flying, in water or spectating). There it's part of Minecraft's player, which the game doesn't draw, so it
vanishes from Night City until Minecraft drops it: a fall of more than half a block, water, flying, sleeping, powder
snow, a hurt, a game-mode change or death (`ServerPlayer.handleShoulderEntities`, `hurtServer`, `setGameMode`, `die`).
That's left as it is ("Non-goals"), and the tests' flyer is a bee.

### Flyers

Allays, bees and parrots:
- **Walls:** their paths see the city's walls through section 2's path types (`FlyNodeEvaluator`'s copy of the rule
  reads the same answers).
- **Wandering:** their spots come from `HoverRandomPos`, which goes above solid cells (section 2's `GoalUtils.isSolid`),
  and `AirAndWaterRandomPos`, which refuses spots with a malus (the path types). Neither asks for a stable spot, so the
  flyers' own `isStableDestination` (`FlyingPathNavigation`'s, a bee's) is left alone.
- **Shortcuts** (`PathNavigation.isClearForMovementBetween`, behind flyers' `canMoveDirectly`): when vanilla's clip is
  clear, the same segment is cast against the city ("Casting"), and a hit means no shortcut. A flyer then follows its
  path's nodes around the wall.

### Sight

`LivingEntity.hasLineOfSight(Entity, ClipContext.Block, ClipContext.Fluid, double)`: when vanilla's clip says clear,
the same eye-to-eye segment is cast against the city, and a hit means no sight. That covers what's built on it:
targeting (`TargetingConditions`, `Sensing`), keeping a target (`TargetGoal`), skeletons and pillagers shooting,
creepers swelling, a mob looking at a player, and brains' visible-entity and item sensors. Triangles, as arrows use, so
a mob's sight follows the city's collision, which is close to what V sees.

### Casting

Both casts use `SkyClip.cast`, over the segment in pieces of at most 8 blocks, nearest first, stopping at the first
hit. A region holds 1,000 to 2,500 triangles (spike 5's Task 46), and one box around a long ray would gather tens of
thousands. If the minute line shows sight costing too much, a walk through the cells' voxels is the cheaper fallback,
at the voxels' coarseness.

## 4. Cost and the log

Once a minute in which the mod answered anything from the city, it writes one line to Minecraft's log:
- **cells:** city answers given and shapes classified;
- **casts:** sight and shortcut casts, the hits among them, and the milliseconds they took;
- **teleports:** pets landed on the city's floor.

The counters are safe to bump from any thread. Step 0 and run F read the cost from the line.

## 5. Edge cases

- **No city where a mob is:** outside the exporter's area, before its regions arrive, after an epoch clear. The cells
  answer none, so pathfinding behaves as now: nothing is walkable, mobs stand still, and paths fail. A path can't lead
  a mob off the known ground, since nothing beyond it is walkable; a knockback or a blast can, and the mob falls into
  the void, as now.
- **Outside the city:** in a plain Minecraft world, where SkyCraft has received no region, `shapeAt` returns null at
  once, and every read gives vanilla's answer. After the link ends, SkyCraft keeps the city's voxels until its next
  epoch, and mobs keep finding their way on them, as they keep colliding with them.
- **A Minecraft block on the city:** a block the player places on a street is real, and vanilla answers for its cell.
  The cell above it stands on it as on any block, unless the city has a floor or a wall there too.
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
  calls into it are `SkyCollision.shapeAt` and `SkyCollision.active`, and `SkyClip.cast`.
- **Hostiles ignore creative players,** as vanilla does, so the chase tests run in survival.

## Versions and files

- **CyberCraft Tweaks 1.7.0** (`minecraft/cybercraft-tweaks`):
  - `paths/CellRule` (new, no Minecraft types): the kinds and tops from 64 column tops, a cell's floor height, stroll
    targets, the surface search, a segment's 8-block pieces;
  - `paths/CityCells` (new): `SkyCollision.shapeAt` to `CellRule`, the per-shape cache, `floorAt`, and Minecraft's
    blocks first;
  - `paths/CityRays` (new): the casts against the city's triangles, in pieces;
  - `paths/PathStats` (new): the counters and the minute line, which `CyberCraftTweaks` writes from the server's tick;
  - common mixins (new), one per Minecraft class: `CityWalkNodes` (`WalkNodeEvaluator`: path types, floor levels,
    airborne starts), `WalkNodeEvaluatorAccess` (an invoker for `getPathTypeFromState`), `CityPathCache`
    (`PathTypeCache`), `CityNavigation` (`PathNavigation`: stroll targets, flyers' shortcuts), `CitySurface`
    (`GroundPathNavigation.findSurfacePosition`), `CityGoalUtils` (`GoalUtils.isSolid`), `CityPetTeleport`
    (`TamableAnimal`), `CitySight` (`LivingEntity.hasLineOfSight`);
  - `fabric.mod.json`: 1.7.0, its description; the README: a section on finding the way;
  - `test.sh` (new) and `src/test/java`: the unit tests, compiled with `javac` and run with plain `java`;
  - `mixins.sh` and `tools/MixinCheck.java` (new): the mixins on Minecraft's classes checked against 26.3's jar, as
    Mixin would at load.
- **`mclink`:** `tools/mclink/paths.txt`, step 0's script; `teleports.txt`, a pet's landings (Part 1); and
  `pathsrow.txt`, the overlay's Paths rows typed into Minecraft on run F's ground (Part 2).
- **The overlay** (`blockrender_hud/init.lua`): the **Paths** rows, Spawn rows of the mobs and of the wheat and the
  dandelion, and the way to run F's spots.
- **BlockRender's C++** and **SkyCraft** stay as they are.

## Testing

Each layer can fail, and each new test is broken once to prove it.

### Unit tests (`test.sh`, no Minecraft)

A small harness that counts failed checks, as the C++ tests' does, with no JUnit:
- **`CellRule`'s kinds:** nothing in the middle with walls along the edge columns (none), a sliver in a corner column
  (none), skins at layers 0 to 3 (floor), 4 to 7 (blocked), a floor with a beam above it (blocked), a full cell
  (blocked, top 1); fences: a wall through the middle, a railing's last half block, a floor's last sliver, a pole above
  nothing, the 18-column boundary, and not a kerb's cell or a pole on the street's top;
- **floor heights:** a floor cell, above a blocked cell, above a floor cell, above a fence, neither;
- **stroll targets:** a floor cell, above a blocked cell, but not above a floor cell or a fence, or in the air;
- **the surface search:** vanilla's columns (onto the block below, up out of something solid, above the world when
  there's nothing, a flower staying), and the city's (into a floor cell below, staying in one, up out of a blocked
  cell);
- **pieces:** short segments whole, long ones in 8-block pieces that meet end to end, zero length.

The mixins' targets are checked without Minecraft too (`mixins.sh`), each check broken once.

### Step 0: Minecraft's side, with `mclink`, before any game session

The user starts Minecraft, and `mclink` runs `tools/mclink/paths.txt` on spike 1's raised-level dump
(`2026-10-01_14-37-07`), placed around 1280.5 6.14 2007.5, 400 blocks east of spike 7's spot. The deep review's
simulation (BlockRender's own voxelizer on the dump, the walk rules, and sight rays against its triangles) picked each
check's spots and gives its expected numbers: a road along z, its street at 6.0 and its pavement at 6.2, a barrier at
x 1284 (top 7.5), a ledge behind it at 5.5, then a 6 m drop onto a rock slope falling east from −0.5. Everything
summoned is tagged `sp9`, with no loot (`DeathLootTable:"minecraft:empty"`), `PersistenceRequired`, and nothing it
carries dropping. The results come from chat replies in Minecraft's log (`/data get entity … Pos`, `/execute if
entity …,distance=..N`) and `scene`'s world records.

The checks run one at a time, each cleared before the next, since wolves attack what hurts their owner, villagers flee
husks, and skeletons flee wolves. The player's off hand is filled only if it's empty, with the wheat and then the
dandelion, and only those are cleared from it at the end. In survival the player has Resistance at its highest, so
nothing hurts it, and a husk's hunger never lands.
- **Wander:** a pig, a cow and a villager on the street, positions every 5 s for 60 s. Each moves 3 or more blocks and
  strolls at least twice, and stays on the ground (`OnGround:1b`) on the road: the simulation reaches about 2,200
  places within 30 blocks there, none of them on the levels below it.
- **Tempt:** the cow follows the wheat as the stand-in V walks about 10 blocks south along the pavement.
- **Pet:** a wolf whose `Owner` is set from the player's UUID (`/data modify … Owner set from entity @p UUID`) follows
  by path as the stand-in V walks about 15 blocks south, past the 10 a wolf waits for. Then `at` moves V 21 blocks back
  north along the street: the wolf lands within 4 blocks of V, on the ground, at the street's floor and not inside it.
- **Chase:** in survival, a husk in view 10 blocks off on the street reaches the player.
- **Sight:** a skeleton on the rock slope, with a bow and an unbreakable helmet (noon's sun) given in its `equipment`,
  shoots no arrows while V is on the road, which hides it; once `at` takes V onto the ledge behind the barrier, 7.7
  blocks from it and in view, it shoots. `scene`'s world records list the arrows.
- **Flyer:** a bee, tempted by the dandelion (`TemptGoal` on `BEE_FOOD`), follows as `at` takes the stand-in V onto the
  ledge and then down onto the rock slope, past the barrier and the rock face, without sticking in either.
- **Cost:** the mod's minute line.
- **Clear:** `/kill @e[tag=sp9]` and any arrow, then `scene`'s world says `nothing`. The player's game mode (noted in a
  scoreboard at the start) is put back, and its effects cleared.

It runs **twice**. First with 1.6.0, still installed, as the control: wander, tempt, chase and the wolf's teleport
should fail, and the skeleton should shoot through the rock (its arrows sticking in it), which proves the checks can
tell. Then with 1.7.0, everything passing. Installing 1.7.0 needs Minecraft stopped and the user's leave; 1.6.0 is
renamed `.jar.disabled`. The run with 1.7.0 found what 1.7.1 fixes ("Changes while executing Part 1"), so step 0 runs a
third time, with 1.7.1, beside `tools/mclink/teleports.txt`: a pet wolf following the stand-in V through five 22-block
jumps along the street, each landing sampled every quarter second.

### Run F, in the game

After spike 8's run E. CyberCraft Tweaks 1.7.1 installed, and the overlay's Paths rows deployed while the game is
closed (a Lua file). The rows summon, ahead of V, each half a block above V's feet so that it drops onto the floor:
- **wanderers:** a pig, a cow and a villager, 4 blocks ahead;
- **a pet:** a wolf, 2 ahead and to the left, owned by Minecraft's player;
- **a flyer:** a bee, 2 ahead and to the right, a block up;
- **a husk,** 7 ahead, with Resistance for Minecraft's player;
- **wheat and a dandelion,** into the user's slots 8 and 9, picked in build mode: a tempted mob looks at both hands,
  and the off hand keeps its shield.

They're tagged `sp7`, so the existing Clear removes them. The overlay also shows the way to run F's spots: each one's
distance, heading and height from V. First `mclink` types the rows' commands into Minecraft on the two places' dumps,
placed where the game puts them, and rehearses the checks but the tempt and the bee's follow
(`tools/mclink/pathsrow.txt`). Then, with the user, at spot B (spike 1's raised-level dump, V on the pavement facing
south), then in V's apartment in Megabuilding H10, through its front door into the building's corridor and down its
stairs:
1. The wanderers stroll the sidewalks and streets, step kerbs, and stay out of walls and off drops; in the apartment,
   among the furniture, and out of the door if they go.
2. The cow follows the wheat: along the pavement, then through the apartment's door and down the stairs.
3. The wolf, the only mob left, follows, through the door and down the stairs too, and after V sprints away it lands
   beside V: the entities status line's nearest-mob distance shows it, read at once.
4. The bee follows the dandelion without sticking in walls, through the doorway too.
5. With combat off (Minecraft's hits and deaths stay in Minecraft) and Minecraft's player in survival, the husk chases V
   in view at spot B; in the apartment, summoned in the corridor behind the room's wall, it doesn't notice V until V
   steps into the doorway, then comes round through the door.
6. The frame rate is as before, and Minecraft's minute line stays modest.

Clear ends each check, and the results doc gets screenshots, as earlier runs' did.

## Risks

1. **Sloped skins straddle the half:** a gently sloped road's cells come out a floor at y or blocked with the walkable
   node at y + 1, by a voxel either way; both rise less than a jump, so paths work either way.
2. **Slopes and stairs:** a ramp crosses cells, so the path climbs in nodes, a step-up or a jump at a time. Night City's
   stairs (treads about 0.2 m) are step-ups; surfaces steeper than 50° are whole-block risers, as the exporter means.
3. **A wall just off a cell's middle** isn't seen there, so a mob passing off-centre scrapes it and slides; a gap
   narrower than the middle (0.75 block) is closed.
4. **Mobs 0.75 to 1 block wide** (pigs, cows and sheep are 0.9) may path through a gap between 0.75 and their width,
   which they can't fit; collision stops them, and vanilla's stuck detection repaths.
5. **Head room:** a floor cell at head height doesn't stop a path, as a carpet there wouldn't in vanilla; collision
   does. A jump's head room is checked against the real collision (`tryJumpOn`), which sees the city.
6. **Sight's cost:** casts against tens of thousands of triangles for long rays, in 8-block pieces. Step 0 and run F
   measure it in the minute line; the voxel walk is the fallback.
7. **Strolls less often** than on blocks ("Wandering").
8. **Cells with a shape skip the shared cache:** a path reads each cell once (the evaluator keeps its own cache for the
   path), so the cost is a map lookup and a cached classification per cell.
9. **The game's grid isn't step 0's:** a cell can come out a fence or not a fraction of a block over, so where the game
   puts spot B, mobs can climb the barrier's wide north end and go down the rock face behind it, which step 0's
   placement didn't let them ("Changes while planning Part 2").
10. **Furnished rooms split into pockets:** the voxelizer's whole-block footprints for steep surfaces make furniture's
    sloped faces blocks, which can close the gaps between pieces and a room's ways out, as in the room of the dump
    first taken for V's apartment; and a doorway about 2 m high closes to mobs two cells tall where the floor's skin
    rounds up and the lintel's down.

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

## Changes from the deep review

The user asked for a deep self-review of the spec. It traced each claim to its source and simulated step 0's ground.

### What changed

1. **The voxelizer's facts** joined the environment (`link/Voxels.cpp`): a floor on a whole block fills both voxels it
   touches, and surfaces 50 to 84° from flat are coarsened to whole-block footprints. The spec's first risk ("a surface
   on a boundary may fill both") is a fact. On the raised level two thirds of the walkable places stand above a blocked
   cell that way, not on a floor cell, so both of section 1's ways of standing matter.
2. **Minecraft's blocks win by each read's own terms** (section 1): path types take the city's answer where vanilla
   answers `OPEN`, the other reads in a cell whose Minecraft block has no collision. The spec said "where Minecraft has
   air", which would let a flower or a torch on a street hide its floor.
3. **Airborne starts start above the first city cell** (section 2, item 7), as above a block, instead of starting in a
   floor cell: `getStart` falls back to an open start and A* drops from it, which also covers a mob standing on a floor
   whose top is exactly 0.5.
4. **The flyers' `isStableDestination` hook is gone** (section 3): only `LandRandomPos` and `DefaultRandomPos` ask for
   stable spots, and flyers wander with `HoverRandomPos` and `AirAndWaterRandomPos`, which use `GoalUtils.isSolid` and
   the path types. A bee's own navigation overrides that method anyway (`Bee$1`).
5. **Wandering's odds were wrong:** `LandRandomPos` checks stability before lifting a candidate, so the city's lost
   candidates are those under a skin. Simulated, a stroll finds a spot 72% and 66% of the time on spike 1's dumps, not
   about half; vanilla on flat blocks nearly always does.
6. **The pets' landing fix has a stronger reason:** Minecraft's collision ignores voxels inside a box, so a pet landed
   inside a skin falls through the street. Parrots follow at 5 blocks, not 10; cats stop at 5.
7. **Step 0's sight check is a pillager below the rock face,** not a husk behind a wall: the husk couldn't tell blocked
   sight from no path, since with 1.6.0 it can't come either, and the dumps have no tall wall with a way around. With
   1.6.0 the pillager shoots through the rock; with 1.7.0 not until V is in view.
8. **Step 0 is laid out to run clean:** one check at a time, each cleared before the next; the off hand, checked empty,
   for the wheat and the dandelion; Resistance in survival; the bee following V down by `at`; the spots and expected
   numbers from the simulation.
9. **Run F:** the wolf alone for the distance readout, Resistance for the chase, and the husk behind a building it could
   walk around.
10. **The environment gained:** the fly evaluator's own copy of the below rule; `getPathTypeStatic`'s other users;
    `isStableDestination`'s two callers and its overrides; Fabric API's two content-registry mixins and why they
    compose; the world's clock fixed at noon; `Owner` making an animal tame; `sendBlockUpdated` as the cache's only
    invalidation; collision ignoring voxels inside a box.
11. **Non-goals gained** amphibians' and striders' strolls. **Risks gained** mobs 0.75 to 1 block wide, head room under
    a floor cell, and sight's fallback; the boundary risk became the sloped-skin one.

### What was checked

- **Minecraft 26.3, with `javap`:** `getPathTypeFromState`'s returns (air first, `BLOCKED` for unpathfindable, never
  `WALKABLE`), `getPathTypeStatic`'s switch (`OPEN`, `WATER`, `LAVA` and `WALKABLE` below give `OPEN`),
  `FlyNodeEvaluator.getPathType`, `getFloorLevel`, `getMobJumpHeight` (max(1.125, step)), `getStart` and
  `canStartAt`, `findSurfacePosition` in full, `isStableDestination` and every caller and override of it,
  `LandRandomPos`'s order, the flyers' wander goals' helpers, `GoalUtils.isSolid`, `PathTypeCache` (4,096 slots,
  `compute` through `getPathTypeFromState`, `invalidate` only from `ServerLevel.sendBlockUpdated`),
  `isClearForMovementBetween`, `hasLineOfSight` (128 blocks) and its callers, `TargetingConditions.test`,
  `Sensing.hasLineOfSight`, `MoveControl.tick`, `VoxelShape.collideX` and `max(Axis, double, double)`,
  `TamableAnimal`'s teleport (10 tries, distance² 144, the box moved by whole blocks, `snapTo` at the cell's bottom)
  and `Owner`, the wolf's, cat's and parrot's `FollowOwnerGoal`, `Zombie`'s target goal, `Husk.isSunSensitive`, the
  parrot's shoulder rules, and every class in the jar naming the hooked methods: all pathfinding and AI.
- **SkyCraft 0.1.2's source:** `SkyCollision`'s cell layout, `shapeAt`, regions swapped a cell at a time, emptied only
  by `COL_CLEAR`; `BlockCollisionsMixin`; `SkyClip.cast`; nothing touching pathfinding, sight or pets.
- **BlockRender:** `link/Voxels.cpp` (skins, whole-block floors, steep coarsening), the exporter's area and filing, and
  the entities status line's "the nearest mob … m away" (from Minecraft's feet).
- **The instance:** every mod's mixins (Fabric API's 44 modules, Mouse Tweaks, e4mc, EMF, ETF); only Fabric's content
  registries touch these methods, and not for air. The world's `world_clocks.dat`, `weather.dat` and `game_rules.dat`.
- **Step 0's ground, simulated:** a scratch tool, `voxdump`, loads a dump as `mclink`'s `dump` places it, files its
  triangles as the exporter does, and runs BlockRender's `VoxelizeRegion` unchanged; a Python model applies `CellRule`,
  the path types, floor levels, jump limits, falls of up to 3 blocks, `LandRandomPos`'s order and the teleport's tries
  and room. From the stand-in V, within 30 blocks:

  | dump | reachable places | their floors | on a floor cell | strolls found | teleports found |
  |---|---|---|---|---|---|
  | raised level (`14-37-07`) | 2,182 | 5.25 to 8.38 | 737 | 72% | 97% |
  | ramp (`14-46-55`) | 1,846 | 1.50 to 7.88 | 1,275 | 66% | 95% |
  | `2026-10-08_01-44-52` | 45 | 6.12 to 7.12 | 44 | 97%, most out of reach | not run |

  The third is a dump from early on 2026-10-08 (179,645 triangles, V 132 m up), where V stands on a strip two or three
  blocks wide between a tall structure and a gap. A whole-cell rule reached 3,697 places on the raised level, its extra
  ground mostly through corner and edge slivers down the rock face, and some the same places a cell higher on the
  sloped road. The scratch tools are in this session's scratchpad (`voxdump/`, `sim/`) for planning.

  The user took the third dump while playtesting the campaign. It isn't V's apartment, which is in Megabuilding H10
  ("Changes while planning Part 2").

## Changes while planning Part 1

Part 1 is the mod (CyberCraft Tweaks 1.7.0) and step 0; its plan is
`docs/superpowers/plans/2026-10-08-minecraft-paths.md`, whose "Found while planning Part 1" has the detail.
- **Mixins go one per Minecraft class:** a mixin's handlers apply to every class it targets, so the five groups became
  eight classes (`CityWalkNodes`, `WalkNodeEvaluatorAccess`, `CityPathCache`, `CityNavigation`, `CitySurface`,
  `CityGoalUtils`, `CityPetTeleport`, `CitySight`), each with `remap = false`: 26.3 is unobfuscated, and the mod ships
  no refmap. The casts moved from `CityCells` to their own class, `CityRays`.
- **The mixins' targets are checked before Minecraft starts** (`mixins.sh`, `tools/MixinCheck.java`): `javac` checks
  none of it, Mixin's annotation processor checks targets only when it remaps, and a wrong target with
  `defaultRequire: 1` would crash Minecraft at load.
- **Step 0's sight check is a skeleton,** not a pillager: a pillager's crossbow reaches 8 blocks, and the rock slope is
  9 to 16 blocks from V on the road; a skeleton's bow reaches 15. A summon with NBT skips `finalizeSpawn`, so its bow
  comes in its `equipment`, with an unbreakable helmet against noon's sun and `drop_chances` of 0.
- **Step 0's spots and moves** come from the simulation: the walks go south along the road, the wolf's jump 21 blocks
  back north, the bee over the barrier onto the ledge and then down onto the slope, each hop within its 10-block tempt
  range; V walks about 15 blocks for the wolf, which follows only from 10.
- **The off hand is used only if it's empty** (`execute unless items`), and the game mode is noted in a scoreboard and
  put back, rather than "checked first" and "put back as it was" by hand.
- **The unit tests also cover stroll targets and the surface search,** which `CellRule` holds.
- **`PathStats`'s line is written from the server's tick** (Fabric API's `ServerTickEvents`, already on the build's
  classpath).

## Changes while executing Part 1

Step 0's first run with 1.7.0 and four diagnostic scripts after it (the results doc, "Step 0 with CyberCraft Tweaks
1.7.0: the first run") found three things; Tasks 10 to 14 of the plan fix them in CyberCraft Tweaks 1.7.1 and run
step 0 again.
- **Thin cells are fences** (section 1), the user's choice (2026-10-08). The road's barrier, a skin 1/8 block thick and
  about 1.25 m tall, counted as a full block with a floor on top, and its last half block of skin as a floor cell, so
  mobs walked onto it and off its far side, onto the ledge behind it and down the rock face. The other choice was
  section 1's known look, mobs on railings as on iron bars. On the deep review's simulation of the dump, the fence
  rule takes the barrier's top (64 places) and the ledge behind it (54) out of reach, keeps the street, the pavement
  and the kerbs, and leaves a stroll's odds of a target at 0.73. A class now needs the cell below, so `CityCells`
  caches each shape's column tops instead of its class.
- **A pet's landing resets its fall distance** (section 3): the control's test wolf, left in a stale autosave, fell as
  the world loaded before the city's ground came, was teleported to its owner while falling, and died of the fall.
- **Step 0's script:** summons go half a block above their floors (a mob summoned inside a skin falls through it: the
  pavement's skin spans 6.125 to 6.25, and the first runs summoned at 6.2); a second wandering villager starts on the
  pavement beside the barrier; and the wander heights go to 7.0, where the road climbs.
- **Minecraft's autosave saves the world:** stopping Minecraft from the Modrinth App kills it unsaved, so the control's
  test wolf had stayed in the autosave the next session loaded. Singleplayer has no `/save-all` (a dedicated server's
  command in 26.3), and the pause menu doesn't save a linked game, so before Minecraft is stopped an autosave (every 5
  minutes) must pass after the last script; `level.dat`'s time shows it.
- **Diagnostics place the dump where step 0 does:** `mclink`'s `dump` goes around the stand-in V's feet as it runs, so
  a script must stand V at 1280.5 6.14 2007.5 first. The four diagnostics hadn't, and built shifted copies of the city.
- **A pet's check reads its landing, not where it is later:** a pet that lands near its owner strolls, as in vanilla,
  so the teleports script's test 4 s after a jump failed twice of five for a stroll, while all 26 landings of 1.7.1's
  runs were on the road. Run F's pet check reads the nearest-mob distance right after V moves.

## Changes while planning Part 2

Part 2 is the overlay's Paths rows, an `mclink` rehearsal, run F and the spike's close; its plan is Tasks 16 to 21 of
`docs/superpowers/plans/2026-10-08-minecraft-paths.md`, whose "Found while planning Part 2" has the detail. The user
chose (2026-10-09) slots 8 and 9 for the wheat and the dandelion, a simulation of V's apartment before planning its
part of run F, and spot B first. "Run F, in the game" above holds the result.
- **Run F's ground, simulated where the game puts it:** step 0 placed spot B's dump a fraction of a block off the
  game's grid. Where the game puts it, the road is the same, but the barrier's wide north end is a wall top, 2 blocks
  wide and 1.25 above the pavement, that a mob can step onto and drop off behind, down the rock face in drops of 3 or
  less: section 1's "wide wall tops stay walkable", now risk 9.
- **V's apartment is in Megabuilding H10:** the dump taken for it (`2026-10-08_01-44-52`) was a furnished room
  elsewhere, where furniture's whole blocks split the floor into pockets: risk 10. The probe's F10 dumps find nothing
  away from the map's origin, since they pass no PhysX origin; its request file's `origin -1024 1024 0`, with V in her
  main room, dumped the apartment (`2026-10-09_12-22-29`). Its room opens through a front door a block wide onto the
  building's corridor, and five storeys 4 blocks apart are joined by stairs, so run F can try the doorways and stairs
  the brainstorm asked for.
- **The husk's sight check moved indoors:** spot B has nothing tall that hides V and has a way around. In the apartment
  the husk is summoned in the corridor behind the room's wall, and should reach V through the door only once V is in
  the doorway. At spot B the husk chases in view.
- **The rows' places:** wanderers 4 ahead, the wolf and the bee 2 ahead to either side, the husk 7, so none lands on
  another Spawn row's things; each summon half a block above V's feet; and a readout of the way to run F's spots.
- **An `mclink` rehearsal before the game:** the rows' exact commands on both places' dumps, placed as the game places
  them. The tempt and the bee's follow are left to run F: `mclink` can't pick the hotbar slot (the user's 8 and 9 are
  on mouse buttons 5 and 4), and an unheld item tempts nothing.
- **Pets' landings are read at once,** and tested within 4.5 blocks, since a teleport lands up to 3 blocks off on each
  axis.
