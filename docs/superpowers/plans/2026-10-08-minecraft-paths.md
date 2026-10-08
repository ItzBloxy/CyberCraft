# Minecraft's Mobs Finding Their Way Implementation Plan, Part 1 (CyberCraft Tweaks 1.7.0, and step 0)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> In this project, plans run inline with superpowers:executing-plans, never through sub-agents.

**Goal:** Part 1 of spike 9: CyberCraft Tweaks 1.7.0, with which Minecraft's mobs find their way on Night City's
ground (wander, go to targets, keep up with their owner as pets, fly around its walls, and don't see through them),
then step 0 of the spec: the same `mclink` script on spike 1's dumped ground twice, first with 1.6.0 as the control,
then with 1.7.0.

**Architecture:** all of it is on Minecraft's side, in CyberCraft Tweaks (`minecraft/cybercraft-tweaks`):
- **`dev.cybercraft.tweaks.paths`,** plain classes:
  - `CellRule`, the rules with no Minecraft types, which the unit tests check: a cell's kind and top from its 64
    voxel columns (by its middle 6 × 6), a mob's floor in a cell, stroll targets, the surface search, a segment's
    8-block pieces;
  - `CityCells`, SkyCraft's shapes (`SkyCollision.shapeAt`) through `CellRule`, cached per shape object, with
    Minecraft's own blocks first;
  - `CityRays`, casts against the city's triangles (`SkyClip.cast`) in 8-block pieces;
  - `PathStats`, the counters and the minute line, written from the server's tick.
- **Eight mixins on Minecraft's own classes,** one per class: `CityWalkNodes` (the walk evaluator's path types, floor
  heights and airborne starts), `WalkNodeEvaluatorAccess` (an invoker), `CityPathCache`, `CityNavigation` (stroll
  targets, flyers' shortcuts), `CitySurface`, `CityGoalUtils`, `CityPetTeleport` and `CitySight`.
- **Two checks without Minecraft:** `test.sh` runs the unit tests with plain `java`; `mixins.sh` checks the mixins'
  targets against Minecraft 26.3's jar, as Mixin would at load (`tools/MixinCheck.java`), since a wrong target would
  otherwise show only as a crash at Minecraft's start.
- **Step 0's script,** `spikes/block-render/tools/mclink/paths.txt`.

Nothing changes in the game or in BlockRender, and nothing is deployed to the game.

**Tech stack:**
- Java 21 (compiled with the JDK 25 on `PATH`, `--release 21`), Fabric's mixin library 0.8.7 (Sponge `@Inject`,
  `@Redirect`, `@Invoker`, `@Shadow`), ASM 9.10 for the mixin check, no Gradle: `build.sh` compiles against the jars
  the Modrinth App keeps;
- the user's `CyberCraft` instance: Minecraft 26.3, Fabric Loader 0.19.5, Fabric API 0.161.0, SkyCraft 0.1.2,
  CyberCraft Tweaks 1.6.0;
- `mclink`, unchanged, from BlockRender's build on `main`.

**Spec:** [docs/superpowers/specs/2026-10-08-minecraft-paths-design.md](../specs/2026-10-08-minecraft-paths-design.md)

**Part 2** (the overlay's Paths row, run F in the game, and closing the spike) is planned after step 0, from its
results, as earlier spikes' second parts were.

---

## Before you start

- **The branch:** work on `spike/minecraft-paths`, which holds the spec and this plan.
- **The shell:** commands are for Git Bash, with a JDK 21 or later on `PATH` (the user's is 25). Shell variables don't
  persist between tool calls, so each block sets what it needs.
- **Edit steps:** an Edit step's code blocks come in pairs, applied in order: the text to find, which occurs exactly
  once in the file as the pairs before it left it, then the text that replaces it. A Write step's block is the whole
  file.
- **Already verified:** all the code in this plan has been compiled and tested once already, in a scratch clone.
  - Tasks 1 to 7 were then replayed in a fresh clone by a script that applied every file step exactly as written and
    checked every build, test, mixin check and parse check's expected result. The results:
    - zero `javac` warnings with `-Xlint:all`;
    - 43 unit checks passing, and 12 mixin checks;
    - the replayed trees identical to the scratch clone's, task by task.
  - Each unit check that guards a rule was broken once, by a mutation of `CellRule`, and failed as it should: 12
    mutations. The mixin check was broken five ways (a misspelled method, an ordinal past the calls, a wrong argument
    type, a wrong static-ness, a missing field), and caught each.
  - Step 0's script parses in `mclink`, and its spots come from the spec's deep review's simulation of spike 1's
    dump: BlockRender's own voxelizer, and the walk rules, sight rays against the dump's triangles ("Found while
    planning Part 1").
- **Minecraft:** stopped for an install (its jars are locked while it runs); started by the user for step 0. The game
  stays closed throughout: `mclink` takes its place on the link.

## File map (Part 1)

All in `minecraft/cybercraft-tweaks/` but the last:

| File | Task | What |
|---|---|---|
| `src/main/java/dev/cybercraft/tweaks/paths/CellRule.java` | 1 | the rules, no Minecraft types |
| `src/test/java/dev/cybercraft/tweaks/paths/CellRuleTest.java` | 1 | its checks, plain `java` |
| `test.sh` | 1 | compiles and runs the checks |
| `src/main/java/dev/cybercraft/tweaks/paths/CityCells.java` | 2 | the city's cells, Minecraft's blocks first |
| `src/main/java/dev/cybercraft/tweaks/paths/CityRays.java` | 2 | casts against the city's triangles |
| `src/main/java/dev/cybercraft/tweaks/paths/PathStats.java` | 2 | the counters and the minute line |
| `src/main/java/dev/cybercraft/tweaks/CyberCraftTweaks.java` | 2 | the minute line from the server's tick |
| `tools/MixinCheck.java`, `mixins.sh` | 3 | the mixins' targets checked against 26.3's jar |
| `src/main/java/dev/cybercraft/tweaks/mixin/WalkNodeEvaluatorAccess.java` | 4 | `getPathTypeFromState`'s invoker |
| `src/main/java/dev/cybercraft/tweaks/mixin/CityWalkNodes.java` | 4 | path types, floor heights, airborne starts |
| `src/main/java/dev/cybercraft/tweaks/mixin/CityPathCache.java` | 4 | the city's cells skip the shared cache |
| `src/main/java/dev/cybercraft/tweaks/mixin/CityNavigation.java` | 4 | stroll targets, flyers' shortcuts |
| `src/main/java/dev/cybercraft/tweaks/mixin/CitySurface.java` | 4 | where a path aims |
| `src/main/java/dev/cybercraft/tweaks/mixin/CityGoalUtils.java` | 4 | blocked cells solid to random positions |
| `src/main/java/dev/cybercraft/tweaks/mixin/CityPetTeleport.java` | 5 | pets land on the city's floor |
| `src/main/java/dev/cybercraft/tweaks/mixin/CitySight.java` | 5 | the city's walls block sight |
| `src/main/resources/cybercraft_tweaks.mixins.json` | 4, 5 | the mixins listed |
| `src/main/resources/fabric.mod.json`, `README.md` | 6 | 1.7.0 |
| `spikes/block-render/tools/mclink/paths.txt` | 7 | step 0's script |

---

### Task 1: `CellRule`, and the mod's unit tests

The rules Minecraft's pathfinding follows on a cell of Night City's ground, as plain arithmetic: the spec's section 1
(a cell by its middle: none, floor or blocked, and its top), a mob's floor in a cell, stroll targets,
`findSurfacePosition`'s search with the city's cells, and a cast's 8-block pieces. The mod had no tests; `test.sh`
compiles `CellRule` and its checks with `javac` alone and runs them with plain `java`, counting failed checks as
BlockRender's C++ tests do.

**Files:**
- Create: `minecraft/cybercraft-tweaks/src/test/java/dev/cybercraft/tweaks/paths/CellRuleTest.java`,
  `minecraft/cybercraft-tweaks/test.sh`,
  `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/paths/CellRule.java`

- [x] **Step 1: Write `minecraft/cybercraft-tweaks/src/test/java/dev/cybercraft/tweaks/paths/CellRuleTest.java`**

```java
package dev.cybercraft.tweaks.paths;

import java.util.Arrays;
import java.util.function.IntUnaryOperator;

/**
 * {@link CellRule}'s checks, run by {@code test.sh} with plain {@code java}: no JUnit and no Minecraft. Each failed
 * check prints its name; the last line counts them, as BlockRender's C++ tests do.
 */
public final class CellRuleTest {
    private static int checks;
    private static int failures;

    private CellRuleTest() {
    }

    private static void check(boolean ok, String name) {
        checks++;
        if (!ok) {
            failures++;
            System.out.println("FAIL: " + name);
        }
    }

    private static boolean near(double a, double b) {
        return Math.abs(a - b) < 1e-9;
    }

    /** 64 column tops, every one {@code top}. */
    private static int[] all(int top) {
        int[] tops = new int[64];
        Arrays.fill(tops, top);
        return tops;
    }

    private static int packed(int kind, int topEighths) {
        return kind | topEighths << 2;
    }

    private static void classify() {
        int[] edges = new int[64];
        for (int i = 0; i < 8; i++) {
            edges[i] = 8;           // z 0: a wall along the cell's north edge
            edges[56 + i] = 8;      // z 7
            edges[i * 8] = 8;       // x 0
            edges[i * 8 + 7] = 8;   // x 7
        }
        check(CellRule.kind(CellRule.classify(edges)) == CellRule.NONE, "walls along the edges leave the middle empty");
        int[] corner = new int[64];
        corner[63] = 6;
        check(CellRule.classify(corner) == packed(CellRule.NONE, 0), "a sliver in a corner column is no foothold");
        for (int layer = 0; layer < 4; layer++) {
            int cell = CellRule.classify(all(layer + 1));
            check(CellRule.kind(cell) == CellRule.FLOOR && near(CellRule.top(cell), (layer + 1) / 8.0),
                "a skin at layer " + layer + " is a floor at its top");
        }
        for (int layer = 4; layer < 8; layer++) {
            int cell = CellRule.classify(all(layer + 1));
            check(CellRule.kind(cell) == CellRule.BLOCKED && near(CellRule.top(cell), (layer + 1) / 8.0),
                "a skin at layer " + layer + " is blocked at its top");
        }
        int[] beam = all(2);
        beam[3 * 8 + 4] = 7;
        check(CellRule.classify(beam) == packed(CellRule.BLOCKED, 7), "a floor with a beam above it is blocked");
        int[] wall = new int[64];
        for (int z = 0; z < 8; z++) {
            wall[z * 8 + 3] = 8;
        }
        check(CellRule.classify(wall) == packed(CellRule.BLOCKED, 8), "a wall through the middle is blocked");
        check(CellRule.classify(all(8)) == packed(CellRule.BLOCKED, 8), "a full cell is blocked, its top 1");
        check(CellRule.classify(new int[64]) == packed(CellRule.NONE, 0), "an empty cell is none");
    }

    private static void floors() {
        int floor = packed(CellRule.FLOOR, 2);
        int blocked = packed(CellRule.BLOCKED, 7);
        int none = packed(CellRule.NONE, 0);
        check(near(CellRule.floorAt(6, floor, blocked), 6.25), "a floor cell stands on its own floor");
        check(near(CellRule.floorAt(6, none, blocked), 5.875), "above a blocked cell, on its top");
        check(near(CellRule.floorAt(6, -1, blocked), 5.875), "above a blocked cell, the cell not the city's");
        check(near(CellRule.floorAt(6, none, floor), 5.25), "above a floor cell, on its floor");
        check(Double.isNaN(CellRule.floorAt(6, none, none)), "nothing here or below: no floor");
        check(Double.isNaN(CellRule.floorAt(6, -1, -1)), "no city here or below: no floor");
        check(Double.isNaN(CellRule.floorAt(6, blocked, -1)), "inside a blocked cell: no floor");
        check(CellRule.stable(floor, -1), "a floor cell is a stroll target");
        check(CellRule.stable(none, blocked), "just above a blocked cell is a stroll target");
        check(!CellRule.stable(none, floor), "above a floor cell isn't a stroll target");
        check(!CellRule.stable(none, none), "in the air isn't a stroll target");
        check(!CellRule.stable(-1, -1), "no city isn't a stroll target");
    }

    /** A column of search kinds, from y 0 up; air everywhere else. */
    private static IntUnaryOperator column(int... kinds) {
        return y -> y >= 0 && y < kinds.length ? kinds[y] : CellRule.SEARCH_AIR;
    }

    private static void surfaces() {
        final int a = CellRule.SEARCH_AIR, f = CellRule.SEARCH_FLOOR, s = CellRule.SEARCH_SOLID, o = CellRule.SEARCH_OTHER;
        check(CellRule.surface(6, 0, 20, column(s, s, s, a, a, a, a, a)) == 3, "from air, onto the block below");
        check(CellRule.surface(6, 0, 20, column(s, a, a, a, a, a, a)) == 1, "from air, down past every air cell");
        check(CellRule.surface(6, 0, 20, column(a, a, a, a, a, a, a, a, s, s, a)) == 10,
            "nothing below: up while air, then up out of the solid");
        check(CellRule.surface(6, 0, 20, column()) == 21, "nothing anywhere: above the world");
        check(CellRule.surface(3, 0, 20, column(s, s, s, s, s, a)) == 5, "inside something solid: up out of it");
        check(CellRule.surface(3, 0, 20, column(s, s, s, o)) == 3, "in a flower: stays");
        check(CellRule.surface(3, 0, 20, column(s, s, a, a, o)) == 2, "from air above a flower: onto its block");
        check(CellRule.surface(6, 0, 20, column(a, a, a, f, a, a, a)) == 3, "from air, into the floor cell below");
        check(CellRule.surface(3, 0, 20, column(a, a, a, f, a)) == 3, "in a floor cell: stays");
        check(CellRule.surface(4, 0, 20, column(a, a, a, s, s, a)) == 5, "in a blocked cell: up out of it");
    }

    private static void pieces() {
        double[] from = { 1, 2, 3 };
        double[][] still = CellRule.pieces(from, from, 8);
        check(still.length == 2 && Arrays.equals(still[0], from) && Arrays.equals(still[1], from), "zero length: one piece");
        double[][] five = CellRule.pieces(from, new double[] { 4, 6, 3 }, 8);
        check(five.length == 2, "5 blocks: one piece");
        double[][] eight = CellRule.pieces(new double[] { 0, 0, 0 }, new double[] { 8, 0, 0 }, 8);
        check(eight.length == 2, "8 blocks exactly: one piece");
        double[] to = { 30, -2, 17 };
        double[][] long_ = CellRule.pieces(from, to, 8);
        boolean short_ = true;
        for (int i = 0; i + 1 < long_.length; i++) {
            double dx = long_[i + 1][0] - long_[i][0], dy = long_[i + 1][1] - long_[i][1], dz = long_[i + 1][2] - long_[i][2];
            short_ &= Math.sqrt(dx * dx + dy * dy + dz * dz) <= 8 + 1e-9;
        }
        check(long_.length == 6, "about 32.4 blocks: five pieces");
        check(Arrays.equals(long_[0], from) && Arrays.equals(long_[long_.length - 1], to), "the pieces start and end at the ends");
        check(short_, "every piece at most 8 blocks long");
        double[][] over = CellRule.pieces(new double[] { 0, 0, 0 }, new double[] { 8.5, 0, 0 }, 8);
        check(over.length == 3 && near(over[1][0], 4.25), "8.5 blocks: two pieces meeting halfway");
    }

    public static void main(String[] args) {
        classify();
        floors();
        surfaces();
        pieces();
        System.out.println(checks + " check(s), " + failures + " failure(s)");
        if (failures > 0) {
            System.exit(1);
        }
    }
}
```

- [x] **Step 2: Write `minecraft/cybercraft-tweaks/test.sh`**

```bash
#!/usr/bin/env bash
# Runs CyberCraft Tweaks' unit tests: the pure rules (CellRule), compiled with the JDK alone and run with plain java, no
# JUnit and no Minecraft. The last line counts the checks and the failures, and a failure makes it exit 1.
set -euo pipefail
cd "$(dirname "$0")"
rm -rf build/test
mkdir -p build/test
javac --release 21 -Xlint:all -d build/test src/main/java/dev/cybercraft/tweaks/paths/CellRule.java \
	$(find src/test/java -name '*.java')
java -cp build/test dev.cybercraft.tweaks.paths.CellRuleTest
```

- [x] **Step 3: Run CyberCraft Tweaks' unit tests; they must fail, with no `CellRule` yet**

```bash
cd /d/Cyberpunk-Minecraft/minecraft/cybercraft-tweaks && bash test.sh 2>&1 | head -3; echo "exit: ${PIPESTATUS[0]}"
```
Expected: `error: file not found:` naming `CellRule.java`, and `exit: 2`.

- [x] **Step 4: Write `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/paths/CellRule.java`**

```java
package dev.cybercraft.tweaks.paths;

import java.util.function.IntUnaryOperator;

/**
 * The rules Minecraft's pathfinding follows on a cell of Night City's ground, as SkyCraft holds it: 8 × 8 columns of
 * 1/8-block voxels a cell. Plain arithmetic with no Minecraft types, so the unit tests check it without Minecraft.
 *
 * <p>A cell goes by its middle, the central 6 × 6 columns (0.125 to 0.875 across), where a mob up to 0.75 wide stands
 * at the cell's centre: a wall's face along the cell's edge, or a sliver of rock in a corner, is no foothold and no
 * obstacle there. Its top is the highest of those columns. Nothing in the middle is {@link #NONE}; a top in the lower
 * half is a {@link #FLOOR}, walkable itself (a street's skin, a kerb, a stair's tread), since the city's skins have
 * nothing solid under them; a top in the upper half is {@link #BLOCKED}, a full block the cell above stands on.
 */
public final class CellRule {
    /** Nothing in the cell's middle: pathfinding sees air, as without the city. */
    public static final int NONE = 0;
    /** Everything in the middle in the lower half: the cell itself is walkable, its floor at its top. */
    public static final int FLOOR = 1;
    /** Something in the middle in the upper half: a full block, which the cell above stands on. */
    public static final int BLOCKED = 2;

    /** The surface search's kinds of cell: air, a city floor, something solid, or another block (a flower). */
    public static final int SEARCH_AIR = 0;
    public static final int SEARCH_FLOOR = 1;
    public static final int SEARCH_SOLID = 2;
    public static final int SEARCH_OTHER = 3;

    private CellRule() {
    }

    /**
     * A cell's kind and top, packed as {@code kind | top << 2} with the top in eighths of a block, from its 64 columns'
     * tops: index {@code z * 8 + x}, each 0 for an empty column, or 1 to 8 for one whose highest voxel is in layer 0 to
     * 7. Only the middle 6 × 6 columns count.
     */
    public static int classify(int[] tops) {
        int top = 0;
        for (int z = 1; z < 7; z++) {
            for (int x = 1; x < 7; x++) {
                top = Math.max(top, tops[z * 8 + x]);
            }
        }
        int kind = top == 0 ? NONE : top <= 4 ? FLOOR : BLOCKED;
        return kind | top << 2;
    }

    /** A packed cell's kind: {@link #NONE}, {@link #FLOOR} or {@link #BLOCKED}. */
    public static int kind(int cell) {
        return cell & 3;
    }

    /** A packed cell's top, in blocks above its bottom (0 to 1). */
    public static double top(int cell) {
        return (cell >> 2) / 8.0;
    }

    /**
     * Where a mob in cell {@code y} stands: on a floor in the cell itself, or on the top of a floor or blocked cell
     * below; {@code NaN} if neither. Each of {@code here} and {@code below} is a packed cell, or -1 where the city
     * doesn't count (no shape, or Minecraft's own block there).
     */
    public static double floorAt(int y, int here, int below) {
        if (here >= 0 && kind(here) == FLOOR) {
            return y + top(here);
        }
        if (below >= 0 && kind(below) != NONE) {
            return y - 1 + top(below);
        }
        return Double.NaN;
    }

    /** Whether a mob may stroll to cell {@code here}: a floor itself, or just above a blocked cell. */
    public static boolean stable(int here, int below) {
        return here >= 0 && kind(here) == FLOOR || below >= 0 && kind(below) == BLOCKED;
    }

    /**
     * Where {@code GroundPathNavigation.findSurfacePosition} aims a path at a target in column cell {@code y}, as
     * Minecraft 26.3 does, with the city's cells: from air it searches down to the first cell that isn't air, a floor
     * becoming the target and anything else putting it on top; finding nothing it searches up while air, to above the
     * world; a target in something solid moves up out of it; a floor or another block (a flower) stays.
     * {@code kindAt} gives each cell's search kind, and air beyond the world's height.
     */
    public static int surface(int y, int minY, int maxY, IntUnaryOperator kindAt) {
        if (kindAt.applyAsInt(y) == SEARCH_AIR) {
            int below = y - 1;
            while (below >= minY && kindAt.applyAsInt(below) == SEARCH_AIR) {
                below--;
            }
            if (below >= minY) {
                return kindAt.applyAsInt(below) == SEARCH_FLOOR ? below : below + 1;
            }
            int above = y + 1;
            while (above <= maxY && kindAt.applyAsInt(above) == SEARCH_AIR) {
                above++;
            }
            y = above;
        }
        if (kindAt.applyAsInt(y) == SEARCH_SOLID) {
            int above = y + 1;
            while (above <= maxY && kindAt.applyAsInt(above) == SEARCH_SOLID) {
                above++;
            }
            return above;
        }
        return y;
    }

    /**
     * The segment from {@code from} to {@code to} as points a cast goes between, each piece at most {@code longest}
     * long: the first point is {@code from}, the last {@code to}, and each piece ends where the next starts. A zero
     * length gives one piece.
     */
    public static double[][] pieces(double[] from, double[] to, double longest) {
        double dx = to[0] - from[0], dy = to[1] - from[1], dz = to[2] - from[2];
        int count = Math.max(1, (int) Math.ceil(Math.sqrt(dx * dx + dy * dy + dz * dz) / longest));
        double[][] points = new double[count + 1][];
        for (int i = 0; i < count; i++) {
            double t = (double) i / count;
            points[i] = new double[] { from[0] + dx * t, from[1] + dy * t, from[2] + dz * t };
        }
        points[count] = to.clone();
        return points;
    }
}
```

- [x] **Step 5: Run CyberCraft Tweaks' unit tests; they must pass**

```bash
cd /d/Cyberpunk-Minecraft/minecraft/cybercraft-tweaks && bash test.sh 2>&1 | tail -3; echo "exit: ${PIPESTATUS[0]}"
```
Expected: `43 check(s), 0 failure(s)` and `exit: 0`, with no line starting FAIL.

- [x] **Step 6: Build CyberCraft Tweaks**

```bash
cd /d/Cyberpunk-Minecraft/minecraft/cybercraft-tweaks && out=$(bash build.sh 2>&1); echo "$out" | tail -1; \
  echo "warnings: $(echo "$out" | grep -c -i warning)"
```
Expected: `built build/cybercraft-tweaks-1.6.0.jar against 26.3-` and `warnings: 0`.

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add minecraft/cybercraft-tweaks/src minecraft/cybercraft-tweaks/test.sh \
  && git commit -q -m "minecraft-paths: CellRule, Night City's cells by their middle, and the mod's unit tests" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 2: `CityCells`, `CityRays` and `PathStats`

- **`CityCells`** reads each column's top of a SkyCraft shape with `VoxelShape.max(Axis.Y, z, x)` (z first, as
  `AxisCycle` has it, and −∞ for an empty column), classifies it with `CellRule`, and keeps the class per shape object
  in a synchronized `WeakHashMap` (`VoxelShape` keeps `Object.equals`, so it goes by identity). Its answers count the
  city only where Minecraft's own block lets it: path types where vanilla said `OPEN`, the rest where Minecraft's
  block has no collision. `searchKind` reads `isSolid`, deprecated in 26.3, because `findSurfacePosition` does.
- **`CityRays`** returns at once while SkyCraft has no city (`SkyCollision.active()`), and otherwise casts in pieces of
  at most a region (8 blocks), nearest first.
- **`PathStats`** counts with atomics, since sight can be asked on any thread; the server's tick
  (`ServerTickEvents.END_SERVER_TICK`, Fabric API's lifecycle events, already on `build.sh`'s classpath) writes the line
  once a minute in which anything was counted.

**Files:**
- Create: `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/paths/CityCells.java`,
  `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/paths/CityRays.java`,
  `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/paths/PathStats.java`
- Modify: `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/CyberCraftTweaks.java`

- [x] **Step 1: Write `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/paths/CityCells.java`**

```java
package dev.cybercraft.tweaks.paths;

import java.util.Collections;
import java.util.Map;
import java.util.WeakHashMap;

import dev.skycraft.world.SkyCollision;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.pathfinder.PathType;
import net.minecraft.world.phys.shapes.VoxelShape;

/**
 * Night City's ground as Minecraft's pathfinding sees it: each cell of SkyCraft's voxel copy (its
 * {@code SkyCollision} shapes) classified by {@link CellRule}, and the answers the pathfinding mixins give from it.
 *
 * <p>Minecraft's own blocks win. Path types take the city's answer only where vanilla answers {@code OPEN} (air, or a
 * block mobs pass through, such as a flower or a torch on a street); the other reads count the city only in a cell
 * whose Minecraft block has no collision.
 *
 * <p>A cell's class depends on its shape alone, so it's kept per shape object, weakly: SkyCraft builds new shapes
 * whenever a region is sent again, and the old ones go with their region. {@code VoxelShape} keeps
 * {@code Object.equals}, so the map goes by identity.
 */
public final class CityCells {
    /** What an airborne start's search meets in a city cell: a block, so the start goes above it. */
    public static final BlockState STOP = Blocks.STONE.defaultBlockState();

    private static final Map<VoxelShape, Integer> CLASSES = Collections.synchronizedMap(new WeakHashMap<>());

    private CityCells() {
    }

    /** The city's cell at {@code pos}, packed as {@link CellRule#classify} packs it, or -1 where SkyCraft has none. */
    public static int cell(BlockPos pos) {
        VoxelShape shape = SkyCollision.shapeAt(pos);
        if (shape == null) {
            return -1;
        }
        Integer known = CLASSES.get(shape);
        if (known != null) {
            return known;
        }
        int[] tops = new int[64];
        for (int z = 0; z < 8; z++) {
            for (int x = 0; x < 8; x++) {
                // VoxelShape.max(Y, a, b) is the column's top at z = a, x = b, and -infinity for an empty column.
                double top = shape.max(Direction.Axis.Y, (z + 0.5) / 8.0, (x + 0.5) / 8.0);
                tops[z * 8 + x] = top == Double.NEGATIVE_INFINITY ? 0 : (int) Math.round(top * 8.0);
            }
        }
        int cell = CellRule.classify(tops);
        CLASSES.put(shape, cell);
        PathStats.classified();
        return cell;
    }

    /** The city's cell at {@code pos} where Minecraft's own block there has no collision, or -1. */
    private static int open(BlockGetter level, BlockPos pos) {
        int cell = cell(pos);
        return cell >= 0 && level.getBlockState(pos).getCollisionShape(level, pos).isEmpty() ? cell : -1;
    }

    /** The path type for a cell vanilla calls {@code OPEN}: {@code WALKABLE} for a floor, {@code BLOCKED}, or null. */
    public static PathType pathType(BlockPos pos) {
        int cell = cell(pos);
        if (cell < 0 || CellRule.kind(cell) == CellRule.NONE) {
            return null;
        }
        PathStats.answered();
        return CellRule.kind(cell) == CellRule.FLOOR ? PathType.WALKABLE : PathType.BLOCKED;
    }

    /** Where a mob in this cell stands on the city ({@link CellRule#floorAt}), or {@code NaN}. */
    public static double floorAt(BlockGetter level, BlockPos pos) {
        double floor = CellRule.floorAt(pos.getY(), open(level, pos), open(level, pos.below()));
        if (!Double.isNaN(floor)) {
            PathStats.answered();
        }
        return floor;
    }

    /** Whether a mob may stroll to this cell on the city ({@link CellRule#stable}). */
    public static boolean stableAt(BlockGetter level, BlockPos pos) {
        boolean stable = CellRule.stable(open(level, pos), open(level, pos.below()));
        if (stable) {
            PathStats.answered();
        }
        return stable;
    }

    /** Whether this cell is a blocked city cell: solid, for random positions. */
    public static boolean blockedAt(BlockGetter level, BlockPos pos) {
        int cell = open(level, pos);
        boolean blocked = cell >= 0 && CellRule.kind(cell) == CellRule.BLOCKED;
        if (blocked) {
            PathStats.answered();
        }
        return blocked;
    }

    /** Whether an airborne start's search stops at this cell: a floor or a blocked city cell. */
    public static boolean stopsStart(BlockGetter level, BlockPos pos) {
        int cell = open(level, pos);
        boolean stops = cell >= 0 && CellRule.kind(cell) != CellRule.NONE;
        if (stops) {
            PathStats.answered();
        }
        return stops;
    }

    /**
     * This cell's kind for the surface search ({@link CellRule#surface}), the city's cells with Minecraft's. It reads
     * {@code isSolid}, deprecated in 26.3, because {@code findSurfacePosition} itself does.
     */
    @SuppressWarnings("deprecation")
    public static int searchKind(LevelChunk chunk, BlockPos pos) {
        BlockState state = chunk.getBlockState(pos);
        if (state.getCollisionShape(chunk, pos).isEmpty()) {
            int cell = cell(pos);
            if (cell >= 0 && CellRule.kind(cell) != CellRule.NONE) {
                PathStats.answered();
                return CellRule.kind(cell) == CellRule.FLOOR ? CellRule.SEARCH_FLOOR : CellRule.SEARCH_SOLID;
            }
        }
        if (state.isAir()) {
            return CellRule.SEARCH_AIR;
        }
        return state.isSolid() ? CellRule.SEARCH_SOLID : CellRule.SEARCH_OTHER;
    }
}
```

- [x] **Step 2: Write `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/paths/CityRays.java`**

```java
package dev.cybercraft.tweaks.paths;

import dev.skycraft.world.SkyClip;
import dev.skycraft.world.SkyCollision;

import net.minecraft.world.phys.Vec3;

/**
 * Whether Night City's ground stands between two points: a mob's sight, or a flyer's straight-line shortcut. It casts
 * against the city's triangles with SkyCraft's {@code SkyClip.cast}, as arrows do, so it follows the city's collision
 * rather than its coarser voxels.
 *
 * <p>{@code SkyClip.cast} gathers the triangles of every region the segment's box touches, and a region holds 1,000 to
 * 2,500 of them, so a long segment goes in pieces of at most {@link #PIECE} blocks, nearest first, stopping at the
 * first hit.
 */
public final class CityRays {
    /** The longest piece a segment is cast in: a region's size. */
    public static final double PIECE = 8.0;

    private CityRays() {
    }

    /** Whether the city's triangles cross the segment; {@code sight} says what it's for, in the minute line. */
    public static boolean blocked(Vec3 from, Vec3 to, boolean sight) {
        if (!SkyCollision.active()) {
            return false;
        }
        long start = System.nanoTime();
        double[][] points = CellRule.pieces(new double[] { from.x, from.y, from.z }, new double[] { to.x, to.y, to.z }, PIECE);
        boolean hit = false;
        for (int i = 0; i + 1 < points.length && !hit; i++) {
            hit = SkyClip.cast(new Vec3(points[i][0], points[i][1], points[i][2]),
                new Vec3(points[i + 1][0], points[i + 1][1], points[i + 1][2])) != null;
        }
        PathStats.cast(sight, hit, System.nanoTime() - start);
        return hit;
    }
}
```

- [x] **Step 3: Write `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/paths/PathStats.java`**

```java
package dev.cybercraft.tweaks.paths;

import java.util.concurrent.atomic.AtomicLong;

import dev.cybercraft.tweaks.CyberCraftTweaks;

/**
 * What the pathfinding mixins did, for one line in Minecraft's log each minute in which they did anything: the city's
 * answers, the shapes classified, the sight and shortcut casts with their hits and time, and the pets landed on the
 * city's floor. The counters are atomic, so any thread may count; only the server's tick writes the line.
 */
public final class PathStats {
    private static final long MINUTE = 60_000_000_000L;

    private static final AtomicLong ANSWERS = new AtomicLong();
    private static final AtomicLong CLASSIFIED = new AtomicLong();
    private static final AtomicLong SIGHT_CASTS = new AtomicLong();
    private static final AtomicLong SIGHT_HITS = new AtomicLong();
    private static final AtomicLong SHORTCUT_CASTS = new AtomicLong();
    private static final AtomicLong SHORTCUT_HITS = new AtomicLong();
    private static final AtomicLong CAST_NANOS = new AtomicLong();
    private static final AtomicLong PETS = new AtomicLong();

    private static long nextLine;  // the server thread's alone

    private PathStats() {
    }

    public static void answered() {
        ANSWERS.incrementAndGet();
    }

    public static void classified() {
        CLASSIFIED.incrementAndGet();
    }

    public static void cast(boolean sight, boolean hit, long nanos) {
        (sight ? SIGHT_CASTS : SHORTCUT_CASTS).incrementAndGet();
        if (hit) {
            (sight ? SIGHT_HITS : SHORTCUT_HITS).incrementAndGet();
        }
        CAST_NANOS.addAndGet(nanos);
    }

    public static void petLanded() {
        PETS.incrementAndGet();
    }

    /** Each server tick: once a minute, the line, if anything was counted. */
    public static void tick() {
        long now = System.nanoTime();
        if (nextLine == 0) {
            nextLine = now + MINUTE;
            return;
        }
        if (now < nextLine) {
            return;
        }
        nextLine = now + MINUTE;
        long answers = ANSWERS.getAndSet(0), classified = CLASSIFIED.getAndSet(0);
        long sight = SIGHT_CASTS.getAndSet(0), sightHits = SIGHT_HITS.getAndSet(0);
        long shortcuts = SHORTCUT_CASTS.getAndSet(0), shortcutHits = SHORTCUT_HITS.getAndSet(0);
        long nanos = CAST_NANOS.getAndSet(0), pets = PETS.getAndSet(0);
        if (answers + classified + sight + shortcuts + pets == 0) {
            return;
        }
        CyberCraftTweaks.LOG.info("CyberCraft Tweaks: paths in the last minute: {} city answers, {} shapes classified; "
            + "{} sight casts ({} hit), {} shortcut casts ({} hit), {} ms casting; {} pets landed on the city's floor",
            answers, classified, sight, sightHits, shortcuts, shortcutHits, String.format("%.1f", nanos / 1e6), pets);
    }
}
```

- [x] **Step 4: Edit `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/CyberCraftTweaks.java` to write the minute line**

```java
import dev.skycraft.world.SkyDig;
```
```java
import dev.cybercraft.tweaks.paths.PathStats;
import dev.skycraft.world.SkyDig;
```
```java
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerChunkEvents;
```
```java
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerChunkEvents;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerTickEvents;
```
```java
 * floating. Each chunk drops it as the server loads it; SkyCraft syncs the attachment, so clients drop it with it.
```
```java
 * floating. Each chunk drops it as the server loads it; SkyCraft syncs the attachment, so clients drop it with it.
 *
 * <p>It also writes the pathfinding mixins' minute line ({@link PathStats}) from the server's tick.
```
```java
                target.removeAttached(SkyDig.DUG);
            }
        });
    }
}
```
```java
                target.removeAttached(SkyDig.DUG);
            }
        });
        ServerTickEvents.END_SERVER_TICK.register(server -> PathStats.tick());
    }
}
```

- [x] **Step 5: Build CyberCraft Tweaks**

```bash
cd /d/Cyberpunk-Minecraft/minecraft/cybercraft-tweaks && out=$(bash build.sh 2>&1); echo "$out" | tail -1; \
  echo "warnings: $(echo "$out" | grep -c -i warning)"; \
  unzip -l build/cybercraft-tweaks-1.6.0.jar | grep -o "dev/cybercraft/tweaks/paths/[A-Za-z]*\.class" | sort | tr '\n' ' '
```
Expected: `built build/cybercraft-tweaks-1.6.0.jar against 26.3-`, `warnings: 0`, and
`dev/cybercraft/tweaks/paths/CellRule.class dev/cybercraft/tweaks/paths/CityCells.class dev/cybercraft/tweaks/paths/CityRays.class dev/cybercraft/tweaks/paths/PathStats.class`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add minecraft/cybercraft-tweaks/src \
  && git commit -q -m "minecraft-paths: CityCells, CityRays and PathStats: the city's answers, its casts and the minute line" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 3: The mixin check

Mixin applies a mixin when Minecraft loads its target class, and with `defaultRequire: 1` a wrong target crashes
Minecraft then; `javac` checks none of it, and Mixin's own annotation processor checks targets only when it remaps,
which 26.3, unobfuscated, doesn't (with `remap = false` it let a misspelled method and an ordinal past the calls
through). `tools/MixinCheck.java` reads the compiled mixins' annotations with ASM and checks them against the
Minecraft jar the build compiles against: each `@Inject`'s and `@Redirect`'s target method (once, when named without a
descriptor), its handler's arguments, return and static-ness, a redirected call at its ordinal, each `@Invoker`'s
method and `@Shadow` field. `mixins.sh` runs it on the mixins that name their target class (`@Mixin(value = …)`);
the SkyCraft ones, by name, aren't Minecraft's. With none yet, it counts none.

**Files:**
- Create: `minecraft/cybercraft-tweaks/tools/MixinCheck.java`, `minecraft/cybercraft-tweaks/mixins.sh`

- [x] **Step 1: Write `minecraft/cybercraft-tweaks/tools/MixinCheck.java`**

```java
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.zip.ZipFile;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.Type;
import org.objectweb.asm.tree.AbstractInsnNode;
import org.objectweb.asm.tree.AnnotationNode;
import org.objectweb.asm.tree.ClassNode;
import org.objectweb.asm.tree.FieldNode;
import org.objectweb.asm.tree.MethodInsnNode;
import org.objectweb.asm.tree.MethodNode;

/**
 * Checks CyberCraft Tweaks' mixins on Minecraft's own classes against Minecraft's jar, as Mixin would when Minecraft
 * loads them, so a wrong target fails here rather than at Minecraft's start ({@code mixins.sh} runs it): each
 * {@code @Inject}'s and {@code @Redirect}'s target method is there (once, when it's named without a descriptor), its
 * handler's arguments, return and static-ness fit, a redirected call is there at its ordinal, and each
 * {@code @Invoker}'s method and {@code @Shadow} field is there. It knows the kinds the mod uses: {@code @Inject} at
 * {@code HEAD} or {@code RETURN}, {@code @Redirect} of an {@code INVOKE}, and anything else is reported unchecked.
 *
 * <p>Usage: {@code java MixinCheck <minecraft.jar> <mixin class file>...}; the last line counts checks and problems.
 */
public final class MixinCheck {
    static ZipFile jar;
    static final Map<String, ClassNode> CLASSES = new HashMap<>();
    static int problems;
    static int checks;

    static ClassNode load(String internal) throws Exception {
        ClassNode cached = CLASSES.get(internal);
        if (cached != null) {
            return cached;
        }
        var entry = jar.getEntry(internal + ".class");
        if (entry == null) {
            return null;
        }
        ClassNode node = new ClassNode();
        try (InputStream in = jar.getInputStream(entry)) {
            new ClassReader(in).accept(node, 0);
        }
        CLASSES.put(internal, node);
        return node;
    }

    static Object value(AnnotationNode a, String key) {
        if (a == null || a.values == null) {
            return null;
        }
        for (int i = 0; i < a.values.size(); i += 2) {
            if (a.values.get(i).equals(key)) {
                return a.values.get(i + 1);
            }
        }
        return null;
    }

    static AnnotationNode find(List<AnnotationNode> list, String desc) {
        if (list != null) {
            for (AnnotationNode a : list) {
                if (a.desc.equals(desc)) {
                    return a;
                }
            }
        }
        return null;
    }

    static void problem(String where, String what) {
        problems++;
        System.out.println("PROBLEM " + where + ": " + what);
    }

    static void ok(String where, String what) {
        checks++;
        System.out.println("ok " + where + ": " + what);
    }

    static List<MethodNode> targets(ClassNode target, String spec) {
        String name = spec, desc = null;
        int paren = spec.indexOf('(');
        if (paren >= 0) {
            name = spec.substring(0, paren);
            desc = spec.substring(paren);
        }
        List<MethodNode> found = new ArrayList<>();
        for (MethodNode m : target.methods) {
            if (m.name.equals(name) && (desc == null || m.desc.equals(desc))) {
                found.add(m);
            }
        }
        return found;
    }

    /** The calls in {@code m} to {@code ref}, written as Mixin's targets are: {@code Lowner;name(args)return}. */
    static List<MethodInsnNode> calls(MethodNode m, String ref) {
        List<MethodInsnNode> found = new ArrayList<>();
        for (AbstractInsnNode insn : m.instructions) {
            if (insn instanceof MethodInsnNode call && ("L" + call.owner + ";" + call.name + call.desc).equals(ref)) {
                found.add(call);
            }
        }
        return found;
    }

    static FieldNode field(String owner, String name) throws Exception {
        for (String at = owner; at != null; ) {
            ClassNode node = load(at);
            if (node == null) {
                return null;
            }
            for (FieldNode f : node.fields) {
                if (f.name.equals(name)) {
                    return f;
                }
            }
            at = node.superName;
        }
        return null;
    }

    public static void main(String[] args) throws Exception {
        jar = new ZipFile(args[0]);
        for (int i = 1; i < args.length; i++) {
            ClassNode mixin = new ClassNode();
            new ClassReader(Files.readAllBytes(Path.of(args[i]))).accept(mixin, 0);
            String who = mixin.name.substring(mixin.name.lastIndexOf('/') + 1);
            AnnotationNode mix = find(mixin.invisibleAnnotations, "Lorg/spongepowered/asm/mixin/Mixin;");
            @SuppressWarnings("unchecked")
            List<Type> values = (List<Type>) value(mix, "value");
            if (values == null || values.size() != 1) {
                problem(who, "no single @Mixin value");
                continue;
            }
            ClassNode target = load(values.get(0).getInternalName());
            if (target == null) {
                problem(who, "no class " + values.get(0));
                continue;
            }
            for (FieldNode f : mixin.fields) {
                if (find(f.visibleAnnotations, "Lorg/spongepowered/asm/mixin/Shadow;") != null
                    || find(f.invisibleAnnotations, "Lorg/spongepowered/asm/mixin/Shadow;") != null) {
                    FieldNode real = field(target.name, f.name);
                    if (real == null || !real.desc.equals(f.desc)) {
                        problem(who, "@Shadow " + f.name + " " + f.desc + " not in " + target.name);
                    } else {
                        ok(who, "@Shadow " + f.name);
                    }
                }
            }
            for (MethodNode h : mixin.methods) {
                List<AnnotationNode> all = new ArrayList<>();
                if (h.visibleAnnotations != null) {
                    all.addAll(h.visibleAnnotations);
                }
                if (h.invisibleAnnotations != null) {
                    all.addAll(h.invisibleAnnotations);
                }
                for (AnnotationNode a : all) {
                    String kind = a.desc.substring(a.desc.lastIndexOf('/') + 1, a.desc.length() - 1);
                    String where = who + "." + h.name;
                    boolean handlerStatic = (h.access & Opcodes.ACC_STATIC) != 0;
                    if (kind.equals("Invoker")) {
                        String name = (String) value(a, "value");
                        List<MethodNode> found = targets(target, name + h.desc);
                        if (found.size() != 1) {
                            problem(where, "invoker target " + name + h.desc + " found " + found.size());
                        } else if (((found.get(0).access & Opcodes.ACC_STATIC) != 0) != handlerStatic) {
                            problem(where, "invoker static-ness differs");
                        } else {
                            ok(where, "@Invoker " + name + h.desc);
                        }
                        continue;
                    }
                    if (!kind.equals("Inject") && !kind.equals("Redirect")) {
                        if (a.desc.startsWith("Lorg/spongepowered/asm/mixin/injection/") || kind.equals("Accessor")) {
                            problem(where, "@" + kind + " isn't one this check knows");
                        }
                        continue;
                    }
                    @SuppressWarnings("unchecked")
                    List<String> methods = (List<String>) value(a, "method");
                    Object atValue = value(a, "at");
                    AnnotationNode at = atValue instanceof List<?> list ? (AnnotationNode) list.get(0) : (AnnotationNode) atValue;
                    String atKind = (String) value(at, "value");
                    String atTarget = (String) value(at, "target");
                    Integer ordinal = (Integer) value(at, "ordinal");
                    for (String spec : methods) {
                        List<MethodNode> found = targets(target, spec);
                        if (found.size() != 1) {
                            problem(where, "target " + spec + " found " + found.size() + " times in " + target.name);
                            continue;
                        }
                        MethodNode m = found.get(0);
                        boolean targetStatic = (m.access & Opcodes.ACC_STATIC) != 0;
                        if (targetStatic != handlerStatic) {
                            problem(where, "static-ness: target " + targetStatic + ", handler " + handlerStatic);
                        }
                        Type[] targetArgs = Type.getArgumentTypes(m.desc);
                        Type[] handlerArgs = Type.getArgumentTypes(h.desc);
                        if (kind.equals("Inject")) {
                            Type ret = Type.getReturnType(m.desc);
                            String info = ret.getSort() == Type.VOID ? "org/spongepowered/asm/mixin/injection/callback/CallbackInfo"
                                : "org/spongepowered/asm/mixin/injection/callback/CallbackInfoReturnable";
                            boolean fits = handlerArgs.length == targetArgs.length + 1
                                && handlerArgs[targetArgs.length].getInternalName().equals(info)
                                && Type.getReturnType(h.desc).getSort() == Type.VOID;
                            for (int k = 0; fits && k < targetArgs.length; k++) {
                                fits = handlerArgs[k].equals(targetArgs[k]);
                            }
                            if (!fits) {
                                problem(where, "handler " + h.desc + " doesn't fit " + m.name + m.desc);
                            } else if (!atKind.equals("HEAD") && !atKind.equals("RETURN")) {
                                problem(where, "unchecked @At " + atKind);
                            } else {
                                ok(where, "@Inject " + m.name + m.desc + " at " + atKind);
                            }
                        } else {
                            List<MethodInsnNode> calls = atTarget == null ? List.of() : calls(m, atTarget);
                            int need = ordinal == null ? 1 : ordinal + 1;
                            if (!"INVOKE".equals(atKind) || calls.size() < need) {
                                problem(where, atTarget + " occurs " + calls.size() + " times in " + m.name + ", ordinal " + ordinal);
                                continue;
                            }
                            String owner = atTarget.substring(1, atTarget.indexOf(';'));
                            String callDesc = atTarget.substring(atTarget.indexOf('('));
                            Type[] callArgs = Type.getArgumentTypes(callDesc);
                            List<Type> expected = new ArrayList<>();
                            if (calls.get(need - 1).getOpcode() != Opcodes.INVOKESTATIC) {
                                expected.add(Type.getObjectType(owner));  // an instance call's handler takes its receiver first
                            }
                            for (Type t : callArgs) {
                                expected.add(t);
                            }
                            boolean fits = Type.getReturnType(h.desc).equals(Type.getReturnType(callDesc))
                                && handlerArgs.length == expected.size();
                            for (int k = 0; fits && k < expected.size(); k++) {
                                fits = handlerArgs[k].equals(expected.get(k));
                            }
                            if (!fits) {
                                problem(where, "redirect handler " + h.desc + " doesn't fit " + atTarget);
                            } else {
                                ok(where, "@Redirect in " + m.name + " of " + atTarget + " (" + calls.size() + " calls, ordinal " + ordinal + ")");
                            }
                        }
                    }
                }
            }
        }
        System.out.println(checks + " mixin check(s), " + problems + " problem(s)");
        System.exit(problems == 0 ? 0 : 1);
    }
}
```

- [x] **Step 2: Write `minecraft/cybercraft-tweaks/mixins.sh`**

```bash
#!/usr/bin/env bash
# Checks the mod's mixins on Minecraft's own classes (those naming their target class, @Mixin(value = ...)) against
# Minecraft 26.3's jar, as Mixin would when Minecraft loads them, so a wrong target fails here rather than at
# Minecraft's start (tools/MixinCheck.java says what it checks). It reads the classes build.sh compiles, so run that
# first. It needs the JDK alone, and the ASM jars the Modrinth App keeps; the last line counts checks and problems.
set -euo pipefail
cd "$(dirname "$0")"
modrinth="$(cygpath -u "$APPDATA")/ModrinthApp"
newest() { ls $1 | sort -V | tail -1; }  # the newest of the files a pattern names
minecraft=$(newest "$modrinth/meta/versions/26.3-*/26.3-*.jar")
asm=""
for name in asm asm-tree; do
	asm+="$(cygpath -w "$(newest "$modrinth/meta/libraries/org/ow2/asm/$name/*/$name-*.jar")");"
done
rm -rf build/mixincheck
mkdir -p build/mixincheck
javac --release 21 -Xlint:all -cp "$asm" -d build/mixincheck tools/MixinCheck.java
classes=()
for source in $(grep -l "@Mixin(value = " src/main/java/dev/cybercraft/tweaks/mixin/*.java); do
	classes+=("$(cygpath -w "build/classes/dev/cybercraft/tweaks/mixin/$(basename "$source" .java).class")")
done
java -cp "$asm$(cygpath -w build/mixincheck)" MixinCheck "$(cygpath -w "$minecraft")" "${classes[@]}"
```

- [x] **Step 3: Build CyberCraft Tweaks, and check the mixins**

```bash
cd /d/Cyberpunk-Minecraft/minecraft/cybercraft-tweaks && out=$(bash build.sh 2>&1); echo "$out" | tail -1; \
  echo "warnings: $(echo "$out" | grep -c -i warning)"; bash mixins.sh 2>&1 | tail -3
```
Expected: `built build/cybercraft-tweaks-1.6.0.jar against 26.3-`, `warnings: 0`, and `0 mixin check(s), 0 problem(s)`.

- [x] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add minecraft/cybercraft-tweaks/tools minecraft/cybercraft-tweaks/mixins.sh \
  && git commit -q -m "minecraft-paths: mixins.sh checks the mod's mixins on Minecraft's classes against 26.3's jar" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 4: Mobs walk Night City's ground

The spec's section 2, and flyers' shortcuts from section 3. Each mixin names its class (`@Mixin(value = …, remap =
false)`: 26.3 is unobfuscated, with no refmap), and asks `CityCells` only where Minecraft's own answer is open air:
- **`CityWalkNodes`** on `WalkNodeEvaluator`: at the return of `getPathTypeFromState`, an `OPEN` answer becomes the
  city's (`WALKABLE` for a floor, `BLOCKED`); at the return of the static `getFloorLevel(BlockGetter, BlockPos)`, the
  city's floor when it's higher; and `getStart`'s fourth `PathfindingContext.getBlockState` call, its airborne
  search's (ordinal 3 of 4), answers a stand-in stone at a city cell, so the start goes above it.
- **`WalkNodeEvaluatorAccess`** invokes the protected static `getPathTypeFromState` for `CityPathCache`.
- **`CityPathCache`** on `PathTypeCache`: at the head of `getOrCompute`, a cell with a city shape is answered fresh,
  and never cached.
- **`CityNavigation`** on `PathNavigation`: at the return of `isStableDestination`, a floor cell or a cell above a
  blocked one is stable too; at the return of the static `isClearForMovementBetween`, a clear segment to the target at
  half the mob's height is refused where the city crosses it.
- **`CitySurface`** on `GroundPathNavigation`: at the head of the private `findSurfacePosition`, while SkyCraft has a
  city, the target comes from `CellRule.surface` with the city's cells among Minecraft's.
- **`CityGoalUtils`** on `GoalUtils`: at the return of the static `isSolid`, a blocked city cell is solid.

**Files:**
- Create: `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/WalkNodeEvaluatorAccess.java`,
  `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/CityWalkNodes.java`,
  `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/CityPathCache.java`,
  `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/CityNavigation.java`,
  `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/CitySurface.java`,
  `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/CityGoalUtils.java`
- Modify: `minecraft/cybercraft-tweaks/src/main/resources/cybercraft_tweaks.mixins.json`

- [x] **Step 1: Write `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/WalkNodeEvaluatorAccess.java`**

```java
package dev.cybercraft.tweaks.mixin;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.pathfinder.PathType;
import net.minecraft.world.level.pathfinder.WalkNodeEvaluator;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Invoker;

/** {@code WalkNodeEvaluator.getPathTypeFromState}, protected, for {@link CityPathCache}'s fresh answers. */
@Mixin(value = WalkNodeEvaluator.class, remap = false)
public interface WalkNodeEvaluatorAccess {
    @Invoker("getPathTypeFromState")
    static PathType cybercraft_tweaks$getPathTypeFromState(BlockGetter level, BlockPos pos) {
        throw new AssertionError("the invoker wasn't applied");
    }
}
```

- [x] **Step 2: Write `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/CityWalkNodes.java`**

```java
package dev.cybercraft.tweaks.mixin;

import dev.cybercraft.tweaks.paths.CityCells;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.pathfinder.PathType;
import net.minecraft.world.level.pathfinder.PathfindingContext;
import net.minecraft.world.level.pathfinder.WalkNodeEvaluator;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.Redirect;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Minecraft's walk evaluator on Night City's ground. Minecraft 26.3 decides where a mob can walk from block states,
 * and SkyCraft's copy of the city isn't blocks: under a street there's only air, so spike 7's mobs never moved. Here,
 * where Minecraft has nothing of its own, the city's cells answer ({@link CityCells}):
 * <ul>
 * <li>a cell's path type: a floor is {@code WALKABLE} itself, a blocked cell {@code BLOCKED}; the walk and fly
 * evaluators, a pet's teleport and {@code GoalUtils.hasMalus} read it, and vanilla's own rule makes the cell above a
 * blocked one walkable;
 * <li>a node's floor height, which the jump check compares: without it every node on the city stood at y - 1;
 * <li>an airborne mob's start: its search down stops at the first city cell and starts above it, as above a block.
 * Otherwise a mob that repaths mid-jump starts at the world's bottom and loses its path.
 * </ul>
 */
@Mixin(value = WalkNodeEvaluator.class, remap = false)
public abstract class CityWalkNodes {
    @Inject(method = "getPathTypeFromState", at = @At("RETURN"), cancellable = true)
    private static void cybercraft_tweaks$cityTypes(BlockGetter level, BlockPos pos, CallbackInfoReturnable<PathType> info) {
        if (info.getReturnValue() == PathType.OPEN) {
            PathType city = CityCells.pathType(pos);
            if (city != null) {
                info.setReturnValue(city);
            }
        }
    }

    @Inject(method = "getFloorLevel(Lnet/minecraft/world/level/BlockGetter;Lnet/minecraft/core/BlockPos;)D", at = @At("RETURN"),
        cancellable = true)
    private static void cybercraft_tweaks$cityFloors(BlockGetter level, BlockPos pos, CallbackInfoReturnable<Double> info) {
        double city = CityCells.floorAt(level, pos);
        if (city > info.getReturnValueD()) {  // false for NaN: no city floor here
            info.setReturnValue(city);
        }
    }

    // getStart's fourth getBlockState is its airborne search's, the cell below the one it's at.
    @Redirect(method = "getStart", at = @At(value = "INVOKE", ordinal = 3,
        target = "Lnet/minecraft/world/level/pathfinder/PathfindingContext;getBlockState(Lnet/minecraft/core/BlockPos;)Lnet/minecraft/world/level/block/state/BlockState;"))
    private BlockState cybercraft_tweaks$cityStarts(PathfindingContext context, BlockPos pos) {
        return CityCells.stopsStart(context.level(), pos) ? CityCells.STOP : context.getBlockState(pos);
    }
}
```

- [x] **Step 3: Write `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/CityPathCache.java`**

```java
package dev.cybercraft.tweaks.mixin;

import dev.skycraft.world.SkyCollision;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.pathfinder.PathType;
import net.minecraft.world.level.pathfinder.PathTypeCache;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * The city's cells skip the server's shared path-type cache. Its 4,096 slots are cleared only when a Minecraft block
 * changes, so a cell cached as air before its region arrived would stay air. A cell with a city shape is answered
 * fresh, and never cached, so when its shape goes whatever the cache holds for it is vanilla's air.
 */
@Mixin(value = PathTypeCache.class, remap = false)
public abstract class CityPathCache {
    @Inject(method = "getOrCompute", at = @At("HEAD"), cancellable = true)
    private void cybercraft_tweaks$cityFresh(BlockGetter level, BlockPos pos, CallbackInfoReturnable<PathType> info) {
        if (SkyCollision.shapeAt(pos) != null) {
            info.setReturnValue(WalkNodeEvaluatorAccess.cybercraft_tweaks$getPathTypeFromState(level, pos));
        }
    }
}
```

- [x] **Step 4: Write `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/CityNavigation.java`**

```java
package dev.cybercraft.tweaks.mixin;

import dev.cybercraft.tweaks.paths.CityCells;
import dev.cybercraft.tweaks.paths.CityRays;

import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.Vec3;

import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * A ground mob's navigation on Night City's ground:
 * <ul>
 * <li>a stroll may end on a city floor, or just above a blocked city cell ({@code isStableDestination}, which
 * {@code LandRandomPos} and {@code DefaultRandomPos} ask of each spot); flyers', amphibians' and striders' navigations
 * have their own, left as they are;
 * <li>a flyer's straight-line shortcut ({@code isClearForMovementBetween}, behind {@code canMoveDirectly}) is refused
 * where the city stands in the way, so the flyer follows its path around the wall instead of into it.
 * </ul>
 */
@Mixin(value = PathNavigation.class, remap = false)
public abstract class CityNavigation {
    @Shadow
    @Final
    protected Level level;

    @Inject(method = "isStableDestination", at = @At("RETURN"), cancellable = true)
    private void cybercraft_tweaks$cityFloors(BlockPos pos, CallbackInfoReturnable<Boolean> info) {
        if (!info.getReturnValueZ() && CityCells.stableAt(level, pos)) {
            info.setReturnValue(true);
        }
    }

    @Inject(method = "isClearForMovementBetween", at = @At("RETURN"), cancellable = true)
    private static void cybercraft_tweaks$cityInTheWay(Mob mob, Vec3 from, Vec3 to, boolean swim, CallbackInfoReturnable<Boolean> info) {
        // The same segment vanilla clips: to the target at half the mob's height.
        if (info.getReturnValueZ() && CityRays.blocked(from, new Vec3(to.x, to.y + mob.getBbHeight() * 0.5, to.z), false)) {
            info.setReturnValue(false);
        }
    }
}
```

- [x] **Step 5: Write `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/CitySurface.java`**

```java
package dev.cybercraft.tweaks.mixin;

import dev.cybercraft.tweaks.paths.CellRule;
import dev.cybercraft.tweaks.paths.CityCells;
import dev.skycraft.world.SkyCollision;

import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.ai.navigation.GroundPathNavigation;
import net.minecraft.world.level.chunk.LevelChunk;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Where a ground mob's path aims, on Night City's ground. {@code findSurfacePosition} searches down from a target in
 * the air for a block to stand on; under the city there's only air, so a chase, a tempt or a lead aimed at the top of
 * the world. While SkyCraft has the city, the search runs as {@link CellRule#surface} has it, with the city's cells
 * among Minecraft's: a floor below becomes the target, a blocked cell puts it on top. Where it meets no city cell it
 * gives what vanilla gives.
 */
@Mixin(value = GroundPathNavigation.class, remap = false)
public abstract class CitySurface {
    @Inject(method = "findSurfacePosition", at = @At("HEAD"), cancellable = true)
    private void cybercraft_tweaks$citySurface(LevelChunk chunk, BlockPos pos, int reach, CallbackInfoReturnable<BlockPos> info) {
        if (SkyCollision.active()) {
            int y = CellRule.surface(pos.getY(), chunk.getMinY(), chunk.getMaxY(), at -> CityCells.searchKind(chunk, pos.atY(at)));
            info.setReturnValue(pos.atY(y));
        }
    }
}
```

- [x] **Step 6: Write `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/CityGoalUtils.java`**

```java
package dev.cybercraft.tweaks.mixin;

import dev.cybercraft.tweaks.paths.CityCells;

import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.util.GoalUtils;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * A blocked city cell is solid to random positions ({@code GoalUtils.isSolid}): a hovering flyer's spot goes above it
 * ({@code HoverRandomPos}), and a walker's candidate inside thick geometry is lifted out of it ({@code LandRandomPos}).
 */
@Mixin(value = GoalUtils.class, remap = false)
public abstract class CityGoalUtils {
    @Inject(method = "isSolid", at = @At("RETURN"), cancellable = true)
    private static void cybercraft_tweaks$cityBlocked(PathfinderMob mob, BlockPos pos, CallbackInfoReturnable<Boolean> info) {
        if (!info.getReturnValueZ() && CityCells.blockedAt(mob.level(), pos)) {
            info.setReturnValue(true);
        }
    }
}
```

- [x] **Step 7: Edit `minecraft/cybercraft-tweaks/src/main/resources/cybercraft_tweaks.mixins.json` to list them, for both sides**

```json
  "mixins": ["NoBlastBlocks", "NoDigging", "SetHealth"],
```
```json
  "mixins": ["NoBlastBlocks", "NoDigging", "SetHealth", "WalkNodeEvaluatorAccess", "CityWalkNodes", "CityPathCache",
    "CityNavigation", "CitySurface", "CityGoalUtils"],
```

- [x] **Step 8: Build CyberCraft Tweaks, and check the mixins**

```bash
cd /d/Cyberpunk-Minecraft/minecraft/cybercraft-tweaks && out=$(bash build.sh 2>&1); echo "$out" | tail -1; \
  echo "warnings: $(echo "$out" | grep -c -i warning)"; bash mixins.sh 2>&1 | grep -v "^ok "
```
Expected: `built build/cybercraft-tweaks-1.6.0.jar against 26.3-`, `warnings: 0`, and `10 mixin check(s), 0 problem(s)`,
with no PROBLEM line.

- [x] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add minecraft/cybercraft-tweaks/src \
  && git commit -q -m "minecraft-paths: mobs walk Night City's ground: path types, floors, starts, the cache, strolls, targets, and flyers' shortcuts" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 5: Pets land on the city's floor, and its walls block sight

The spec's section 3:
- **`CityPetTeleport`** on `TamableAnimal`: at the head of the private `maybeTeleportTo(int, int, int)`, where the
  city gives the cell a floor, it keeps vanilla's walkable check (`getPathTypeStatic`), checks room with the pet's box
  at that floor's height, and lands it there; anywhere else vanilla's check and landing run as they are.
- **`CitySight`** on `LivingEntity`: at the return of `hasLineOfSight(Entity, ClipContext.Block, ClipContext.Fluid,
  double)`, a clear sight is cast against the city, the same eye-to-eye segment vanilla clipped.

**Files:**
- Create: `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/CityPetTeleport.java`,
  `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/CitySight.java`
- Modify: `minecraft/cybercraft-tweaks/src/main/resources/cybercraft_tweaks.mixins.json`

- [x] **Step 1: Write `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/CityPetTeleport.java`**

```java
package dev.cybercraft.tweaks.mixin;

import dev.cybercraft.tweaks.paths.CityCells;
import dev.cybercraft.tweaks.paths.PathStats;

import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.TamableAnimal;
import net.minecraft.world.level.pathfinder.PathType;
import net.minecraft.world.level.pathfinder.WalkNodeEvaluator;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * A pet teleporting to its owner lands on Night City's floor. Vanilla checks a spot's room with the pet's box moved by
 * whole blocks, so its current fraction of a block carries over, and lands it at the cell's bottom: on the city that's
 * inside a floor's skin, and Minecraft's collision ignores voxels already inside a box, so the pet would fall through
 * the street. Where the city gives the cell a floor ({@link CityCells#floorAt}), the walkable check stays, and the room
 * check and the landing go to that floor's height; anywhere else vanilla's check and landing stand.
 */
@Mixin(value = TamableAnimal.class, remap = false)
public abstract class CityPetTeleport {
    @Inject(method = "maybeTeleportTo", at = @At("HEAD"), cancellable = true)
    private void cybercraft_tweaks$ontoCityFloors(int x, int y, int z, CallbackInfoReturnable<Boolean> info) {
        TamableAnimal pet = (TamableAnimal) (Object) this;
        BlockPos pos = new BlockPos(x, y, z);
        double floor = CityCells.floorAt(pet.level(), pos);
        if (Double.isNaN(floor)) {
            return;
        }
        info.setReturnValue(false);
        if (WalkNodeEvaluator.getPathTypeStatic(pet, pos) != PathType.WALKABLE
            || !pet.level().noCollision(pet, pet.getBoundingBox().move(x + 0.5 - pet.getX(), floor - pet.getY(), z + 0.5 - pet.getZ()))) {
            return;
        }
        pet.snapTo(x + 0.5, floor, z + 0.5, pet.getYRot(), pet.getXRot());
        pet.getNavigation().stop();
        PathStats.petLanded();
        info.setReturnValue(true);
    }
}
```

- [x] **Step 2: Write `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/CitySight.java`**

```java
package dev.cybercraft.tweaks.mixin;

import dev.cybercraft.tweaks.paths.CityRays;

import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.level.ClipContext;
import net.minecraft.world.phys.Vec3;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Night City's walls block sight. {@code hasLineOfSight} clips eye to eye against Minecraft's blocks alone, so a husk
 * noticed V through a building and a skeleton shot at V through it. When vanilla sees clear, the same segment is cast
 * against the city's triangles ({@link CityRays}), as arrows are, and a hit means no sight: targeting, keeping a
 * target, shooting, swelling, looking at a player and brains' visible-entity and item sensors all go through here.
 */
@Mixin(value = LivingEntity.class, remap = false)
public abstract class CitySight {
    @Inject(method = "hasLineOfSight(Lnet/minecraft/world/entity/Entity;Lnet/minecraft/world/level/ClipContext$Block;Lnet/minecraft/world/level/ClipContext$Fluid;D)Z",
        at = @At("RETURN"), cancellable = true)
    private void cybercraft_tweaks$cityWalls(Entity target, ClipContext.Block block, ClipContext.Fluid fluid, double eyeY,
        CallbackInfoReturnable<Boolean> info) {
        if (!info.getReturnValueZ()) {
            return;
        }
        LivingEntity self = (LivingEntity) (Object) this;
        // The same segment vanilla clips: from this entity's eyes to the target at the eye height it was given.
        if (CityRays.blocked(new Vec3(self.getX(), self.getEyeY(), self.getZ()), new Vec3(target.getX(), eyeY, target.getZ()), true)) {
            info.setReturnValue(false);
        }
    }
}
```

- [x] **Step 3: Edit `minecraft/cybercraft-tweaks/src/main/resources/cybercraft_tweaks.mixins.json` to list them**

```json
    "CityNavigation", "CitySurface", "CityGoalUtils"],
```
```json
    "CityNavigation", "CitySurface", "CityGoalUtils", "CityPetTeleport", "CitySight"],
```

- [x] **Step 4: Build CyberCraft Tweaks, and check the mixins**

```bash
cd /d/Cyberpunk-Minecraft/minecraft/cybercraft-tweaks && out=$(bash build.sh 2>&1); echo "$out" | tail -1; \
  echo "warnings: $(echo "$out" | grep -c -i warning)"; bash mixins.sh 2>&1 | grep -v "^ok "
```
Expected: `built build/cybercraft-tweaks-1.6.0.jar against 26.3-`, `warnings: 0`, and `12 mixin check(s), 0 problem(s)`,
with no PROBLEM line.

- [x] **Step 5: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add minecraft/cybercraft-tweaks/src \
  && git commit -q -m "minecraft-paths: pets land on the city's floor, and its walls block sight" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 6: CyberCraft Tweaks 1.7.0

The version goes to 1.7.0, the description and the README say what it does, and the README's Build and Test say how
to run `test.sh`, `mixins.sh` and step 0's script.

**Files:**
- Modify: `minecraft/cybercraft-tweaks/src/main/resources/fabric.mod.json`, `minecraft/cybercraft-tweaks/README.md`

- [x] **Step 1: Edit `minecraft/cybercraft-tweaks/src/main/resources/fabric.mod.json` for 1.7.0**

```json
  "version": "1.6.0",
  "name": "CyberCraft Tweaks",
  "description": "SkyCraft adjusted for Cyberpunk 2077's Night City: nothing digs into the game's ground, so blasts leave no blocks floating above it; V's health share carries into Minecraft's player as build mode starts; and what SkyCraft sends of Minecraft's entities keeps their layers in Minecraft's draw order, with the layers Minecraft draws emissive marked to glow, and those it blends marked blended.",
```
```json
  "version": "1.7.0",
  "name": "CyberCraft Tweaks",
  "description": "SkyCraft adjusted for Cyberpunk 2077's Night City: nothing digs into the game's ground, so blasts leave no blocks floating above it; V's health share carries into Minecraft's player as build mode starts; and what SkyCraft sends of Minecraft's entities keeps their layers in Minecraft's draw order, with the layers Minecraft draws emissive marked to glow, and those it blends marked blended; and Minecraft's mobs find their way on the city's ground, its walls blocking their sight.",
```

- [x] **Step 2: Edit `minecraft/cybercraft-tweaks/README.md` to say what it does, and how to test it**

```markdown
It targets SkyCraft 0.1.2's classes by name, so `fabric.mod.json` asks for exactly that version: Fabric won't start with
```
```markdown
Minecraft's mobs find their way on Night City's ground (from 1.7.0). SkyCraft's copy of the city isn't blocks: its
collision mixin adds the city's voxels to every collision query, so mobs stand on the streets, but Minecraft 26.3's
pathfinding decides where a mob can walk from block states, and under the city there's only air. Spike 7's mobs never
moved, and a chase aimed at the top of the world. With the mod, where Minecraft has nothing of its own, the city's cells
answer pathfinding's questions (`dev.cybercraft.tweaks.paths`):
- **A cell goes by its middle** (`CellRule`, `CityCells`): the central 6 × 6 of its 8 × 8 voxel columns, where a mob up
  to 0.75 wide stands at its centre. Nothing there is air, as before; a top in the lower half is a floor, walkable
  itself, since the city's skins have nothing solid under them; a top in the upper half is a full block, which the cell
  above stands on.
- **Walking** (`CityWalkNodes`, `CityPathCache`, `CityNavigation`, `CitySurface`, `CityGoalUtils`): the walk
  evaluator's path types, floor heights and airborne starts; the shared path-type cache skipped for the city's cells;
  strolls ending on the city's floors; paths aimed at its ground, not the top of the world; and its blocked cells solid
  to random positions.
- **Pets keep up** (`CityPetTeleport`): a pet teleporting to its owner lands on the city's floor, not inside it, which
  it would fall through.
- **Walls block sight and flyers' shortcuts** (`CitySight`, `CityNavigation`): a mob's line of sight and a flyer's
  straight-line shortcut also cast against the city's triangles (`CityRays`), as arrows do.

Each minute it did any of this, Minecraft's log gets one line: the city's answers, the shapes classified, the casts with
their hits and time, and the pets landed (`PathStats`).

It targets SkyCraft 0.1.2's classes by name, so `fabric.mod.json` asks for exactly that version: Fabric won't start with
```
```markdown
`build/cybercraft-tweaks-<version>.jar`.
```
````markdown
`build/cybercraft-tweaks-<version>.jar`.

```bash
bash test.sh
bash mixins.sh
```

`test.sh` runs the unit tests of the pathfinding's rules (`CellRule`), with plain `java` and no Minecraft. `mixins.sh`
checks the mixins on Minecraft's own classes against Minecraft 26.3's jar, as Mixin would when Minecraft loads them, so
a wrong target fails there rather than at Minecraft's start; it reads the classes `build.sh` compiles.
````
```markdown
villager's body comes before its biome's clothes, those before its profession's, and those before its level's badge.
```
```markdown
villager's body comes before its biome's clothes, those before its profession's, and those before its level's badge.
`spikes/block-render/tools/mclink/paths.txt` puts mobs on spike 1's dumped ground: with the mod, a pig, a cow and a
villager stroll the road, a cow follows wheat, a wolf follows its owner and teleports to them, a husk chases the player,
a skeleton below the road's edge doesn't shoot until it sees the player, and a bee follows a dandelion down the rock
face; without it, none of that happens but the skeleton's shooting, through the rock.
```

- [x] **Step 3: Build CyberCraft Tweaks, run its unit tests, and check the mixins**

```bash
cd /d/Cyberpunk-Minecraft/minecraft/cybercraft-tweaks && out=$(bash build.sh 2>&1); echo "$out" | tail -1; \
  echo "warnings: $(echo "$out" | grep -c -i warning)"; bash test.sh 2>&1 | tail -1; bash mixins.sh 2>&1 | tail -1
```
Expected: `built build/cybercraft-tweaks-1.7.0.jar against 26.3-`, `warnings: 0`, `43 check(s), 0 failure(s)` and
`12 mixin check(s), 0 problem(s)`.

- [x] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add minecraft/cybercraft-tweaks/src minecraft/cybercraft-tweaks/README.md \
  && git commit -q -m "minecraft-paths: CyberCraft Tweaks 1.7.0" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 7: Step 0's script

`tools/mclink/paths.txt` puts the checks of the spec's "Step 0" on spike 1's raised-level dump, at a fresh spot 400
blocks east of spike 7's (the dump around 1280.5 6.14 2007.5), each check alone and cleared before the next. Its
spots come from the deep review's simulation of that ground ("Found while planning Part 1"). It touches the player's
off hand only if it's empty, notes the game mode in a scoreboard and puts it back, and clears the effects it gives.

**Files:**
- Create: `spikes/block-render/tools/mclink/paths.txt`

- [x] **Step 1: Write `tools/mclink/paths.txt`**

```text
# Spike 9's step 0: Minecraft's mobs finding their way on Night City's ground, against SkyCraft's real mod, without the
# game (docs/superpowers/specs/2026-10-08-minecraft-paths-design.md, "Step 0"). Spike 1's raised-level dump goes around
# the stand-in V at a fresh spot, 400 blocks east of spike 7's: a road along z, its street at 6.0 and its pavement at
# 6.2 from x 1278, a barrier at x 1284 (top 7.5), a ledge behind it at 5.5, then a 6 m drop onto a rock slope falling
# east from -0.5 to about -6. The deep review's simulation of this ground picked each spot.
#
# The checks run one at a time, each cleared before the next: wolves attack what hurts their owner, villagers flee
# husks. Everything summoned is tagged sp9, keeps no loot and drops nothing. Results come from the chat replies in
# Minecraft's log: positions (/data get), "Test passed" or "Test failed" (/execute if entity), and the scenes' world
# records (arrows). The player's off hand is used only if it's empty; its game mode is noted at the start and put back
# at the end, and its effects are cleared. Start Minecraft first; this waits for its world. W (26) walks.
status
wait-world 180
ground city
at 1280.5 6.14 2007.5
look 0 0
dump E:\Cyberpunk2077\red4ext\plugins\PhysXProbe\dumps\2026-10-01_14-37-07\collision.obj
link on
wait-ground 30
wait 2
type /scoreboard objectives add sp9mode dummy
type /execute store success score #survival sp9mode if entity @p[gamemode=survival]
type /execute store success score #adventure sp9mode if entity @p[gamemode=adventure]
type /gamemode creative
scene
# 1. Wander: a pig, a cow and a villager on the street west of V, their positions every 5 s for 60 s. Each should
# stroll at least twice and move 3 blocks or more, and stay on the road, at y 6.0 to 7.5.
type /summon minecraft:pig 1272.5 6.0 2007.5 {Tags:["sp9","sp9w"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
type /summon minecraft:cow 1272.5 6.0 2012.5 {Tags:["sp9","sp9w"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
type /summon minecraft:villager 1272.5 6.0 2002.5 {Tags:["sp9","sp9w"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
wait 1
type /execute as @e[tag=sp9w] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp9w] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp9w] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp9w] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp9w] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp9w] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp9w] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp9w] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp9w] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp9w] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp9w] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp9w] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp9w] run data get entity @s Pos
type /execute as @e[tag=sp9w] run data get entity @s OnGround
type /kill @e[tag=sp9]
wait 3
# 2. Tempt: a cow beside V, wheat in the player's off hand (only if it's empty), and the stand-in V walks south about 10
# blocks along the pavement. The cow should follow to within 4 blocks.
type /summon minecraft:cow 1278.5 6.2 2009.5 {Tags:["sp9","sp9cow"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
type /execute unless items entity @p weapon.offhand * run item replace entity @p weapon.offhand with minecraft:wheat
wait 2
build on
down 26
track 2.5
up 26
wait 4
type /execute if entity @e[tag=sp9cow,distance=..4]
type /execute as @e[tag=sp9cow] run data get entity @s Pos
build off
type /kill @e[tag=sp9]
wait 3
# 3. Pet: a wolf 2 blocks west of V, its owner the player. V walks south about 15 blocks, past the 10 a wolf waits for:
# it should follow by path to within 4 blocks. Then V jumps 21 blocks north along the street, past the 12 a pet
# teleports at: it should land beside V, on the street's floor (y 6.0) and on the ground.
at 1276.5 6.0 2007.5
wait 2
type /summon minecraft:wolf 1274.5 6.0 2007.5 {Tags:["sp9","sp9wolf"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
type /data modify entity @e[tag=sp9wolf,limit=1] Owner set from entity @p UUID
wait 2
build on
down 26
track 3.5
up 26
wait 4
type /execute if entity @e[tag=sp9wolf,distance=..4]
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
build off
at 1276.5 6.0 2000.5
wait 4
type /execute if entity @e[tag=sp9wolf,distance=..4]
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
type /kill @e[tag=sp9]
wait 3
# 4. Chase: survival, with Resistance at its highest so nothing hurts the player (and a husk's hunger never lands), and
# a husk 10 blocks south on the street, in view. It should reach the player.
at 1276.5 6.0 2007.5
wait 2
type /effect give @p minecraft:resistance 300 4 true
type /gamemode survival
type /summon minecraft:husk 1276.5 6.0 2017.5 {Tags:["sp9","sp9husk"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
wait 10
type /execute if entity @e[tag=sp9husk,distance=..2]
type /execute as @e[tag=sp9husk] run data get entity @s Pos
type /kill @e[tag=sp9]
wait 3
# 5. Sight: a skeleton on the rock slope below the road, with its bow and an unbreakable helmet (noon's sun), dropping
# neither. From V on the road the barrier and the road's edge hide it: it shouldn't shoot. On the ledge behind the
# barrier, 7.7 blocks from it and in view, V should draw its arrows, which the scenes' world records list.
at 1280.5 6.14 2007.5
wait 2
type /summon minecraft:skeleton 1290.5 -1.25 2007.5 {Tags:["sp9","sp9skel"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty",equipment:{mainhand:{id:"minecraft:bow",count:1},head:{id:"minecraft:iron_helmet",count:1,components:{"minecraft:unbreakable":{}}}},drop_chances:{mainhand:0.0f,head:0.0f}}
wait 10
scene
at 1286.5 5.5 2007.5
wait 8
scene
type /kill @e[tag=sp9]
type /kill @e[type=minecraft:arrow,distance=..60]
wait 3
# 6. Flyer: a bee beside V on the road, a dandelion in the off hand (in place of the wheat), then V hops onto the ledge
# behind the barrier and down onto the rock slope. The bee should follow over the barrier and down the rock face to
# within 4 blocks.
at 1280.5 6.14 2007.5
wait 2
type /execute if items entity @p weapon.offhand minecraft:wheat run item replace entity @p weapon.offhand with minecraft:dandelion
type /summon minecraft:bee 1281.5 7.0 2009.5 {Tags:["sp9","sp9bee"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
wait 3
at 1286.5 5.5 2007.5
wait 5
type /execute as @e[tag=sp9bee] run data get entity @s Pos
at 1290.5 -1.25 2007.5
wait 8
type /execute if entity @e[tag=sp9bee,distance=..4]
type /execute as @e[tag=sp9bee] run data get entity @s Pos
# The clear: everything tagged and any arrow left, the off hand's dandelion, the effects, the game mode as it was. A
# killed mob takes a second to die and its puffs up to 5 s more: the last scene, 7 s on, should say nothing is left.
type /kill @e[tag=sp9]
type /kill @e[type=minecraft:arrow,distance=..60]
type /execute if items entity @p weapon.offhand minecraft:dandelion run item replace entity @p weapon.offhand with minecraft:air
type /effect clear @p
type /gamemode creative
type /execute if score #survival sp9mode matches 1 run gamemode survival
type /execute if score #adventure sp9mode matches 1 run gamemode adventure
type /scoreboard objectives remove sp9mode
at 1280.5 6.14 2007.5
wait 7
scene
status
quit
```

- [x] **Step 2: Check that mclink parses step 0's script**

A copy of the script with a line `mclink` doesn't know at its end: `mclink` reads the whole script before it opens the
link, so it stops at that line, the last, having parsed every other, and nothing runs, even with Minecraft open.
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build \
  && { cat tools/mclink/paths.txt; echo "parse-check-stop here"; } > build/paths-parse.txt \
  && ./build/RelWithDebInfo/mclink.exe build/paths-parse.txt; echo "exit: $?"
```
Expected: `line 154: unknown action: "parse-check-stop here"` and `exit: 2`.

- [x] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render/tools/mclink/paths.txt \
  && git commit -q -m "minecraft-paths: step 0's script, mobs on spike 1's raised-level dump" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 8: In Minecraft: step 0's control, with 1.6.0

Step 0 of the spec's "Testing", first as its control: the script with CyberCraft Tweaks 1.6.0, still installed, which
shows the checks can tell. With the user, from `mclink`, against SkyCraft's unchanged mod in the `CyberCraft` instance,
without the game.
- **The instance** has the Java arguments `--enable-native-access=ALL-UNNAMED -Dskycraft.quitWithSkyrim=false`, so
  Minecraft keeps running between `mclink` runs, and the `SkyCraft` world allows commands.
- **What Minecraft logs:** the chat's lines go into its log after `[CHAT]`: `/data get`'s `… has the following entity
  data: …`, `/execute if entity`'s `Test passed, count: 1` or `Test failed`, the game mode's `Set own game mode to …`,
  and any command's error. SkyCraft's lines start `SkyCraft:`, and 1.7.0's minute line `CyberCraft Tweaks: paths`.
- **If a check misbehaves,** let the script finish (it clears everything whatever happens), then find the cause with
  superpowers:systematic-debugging before going on. Fixes become new tasks.

**Files:**
- Create: `docs/spikes/2026-10-08-minecraft-paths-results.md`

- [x] **Step 1: Check that the game isn't running**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game: it would hold the mapping.

- [x] **Step 2: Ask the user about Entity Model Features and Entity Texture Features**

The mods folder has them without `.disabled`, though spike 7 left them disabled. They change how entities are drawn,
not where they walk: ask whether to leave them as they are. Then check that 1.6.0 is the CyberCraft Tweaks installed:
```bash
ls "/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/mods" | grep -i -E "cybercraft|entity_"
```
Expected: `cybercraft-tweaks-1.6.0.jar`, the earlier versions with `.jar.disabled`, and EMF and ETF as the user chose.

- [x] **Step 3: Ask the user to start Minecraft**

With their leave to summon test mobs on the dump's ground around x 1272 to 1296, z 2000 to 2022 in the `SkyCraft`
world, all killed at the script's end, to switch the game mode to survival and back, and to put wheat, then a
dandelion, in the player's off hand if it's empty, the user starts the `CyberCraft` instance from the Modrinth App,
and says when its title screen shows (or, if it's running already, that it's at the title screen or in the world).
Then:
```bash
grep -E "SkyCraft:" "/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log" | tail -3
```
Expected: `SkyCraft: can't open Skyrim's shared memory yet (Windows error 2: …)`: waiting for a game side.

- [x] **Step 4: In Minecraft: run step 0's script, the control**

```bash
L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"; before=$(wc -l < "$L"); \
  cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/step0 \
  && ./build/RelWithDebInfo/mclink.exe tools/mclink/paths.txt | tee build/step0/control.log; echo "exit: ${PIPESTATUS[0]}"; \
  tail -n +$((before + 1)) "$L" | grep -E "\[CHAT\]|SkyCraft: |CyberCraft Tweaks: |WARN" | tee build/step0/control-minecraft.log
```
It takes about 4 minutes once Minecraft is in its world, and up to 4 more if it opens the world first: give the command
9 minutes. Expected, with 1.6.0:
- **Linking:** `Minecraft is there (process …)`, `clear all`, then `the city around region … is all in the ring`.
- **Wander:** 13 position lines for each of the pig, the cow and the villager, each mob's within a block of where it
  was summoned (1272.5 6.0 2007.5, 1272.5 6.0 2012.5, 1272.5 6.0 2002.5): they stand still, as in spike 7.
- **Tempt:** `Test failed`, the cow where it was summoned, about 9 blocks back.
- **Pet:** `Test failed` after the walk, and again after the jump north: the wolf never moves, and never teleports
  (`canTeleportTo` finds no `WALKABLE` cell on the city).
- **Chase:** `Test failed`, the husk where it was summoned.
- **Sight:** arrows in both scenes' world records: the skeleton sees V through the rock, and its arrows stick in it.
- **Flyer:** recorded as it comes: a bee's paths see every cell as open air, so it may follow or stick against the
  barrier or the rock.
- **No `CyberCraft Tweaks: paths` line** (1.6.0 has none).
- **The end:** the game mode set back as it was, the last scene with `world: nothing`, no `[CHAT]` line with an error,
  no `WARN` line from a command, and `exit: 0`.

- [x] **Step 5: Record the control's results in `docs/spikes/2026-10-08-minecraft-paths-results.md`**

From `build/step0/control.log` and `build/step0/control-minecraft.log`, as spike 7's results doc does: the versions
(Minecraft 26.3, Fabric Loader, SkyCraft 0.1.2, CyberCraft Tweaks 1.6.0, the dump), and each check with its outcome and
numbers: every mob's positions, the tests' replies, the arrows. The spike's answer goes at the top once run F is in.

- [x] **Step 6: Commit**

With the plan, its steps ticked up to this one: running Tasks 1 to 7 ticked theirs.
```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-08-minecraft-paths-results.md docs/superpowers/plans/2026-10-08-minecraft-paths.md \
  && git commit -q -m "minecraft-paths: step 0's control, with CyberCraft Tweaks 1.6.0" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 9: In Minecraft: CyberCraft Tweaks 1.7.0, and step 0

**Files:**
- Modify: `docs/spikes/2026-10-08-minecraft-paths-results.md`

- [x] **Step 1: Ask the user to stop Minecraft**

Its window is hidden while `mclink` drives it, so the user stops it from the Modrinth App. Then:
```bash
tasklist | grep -i -c "javaw.exe"
```
Expected: `0`, or the user says the Java processes left are their own (a Gradle daemon, an IDE).

- [x] **Step 2: Install CyberCraft Tweaks 1.7.0, with the user's leave**

1.6.0 goes `.jar.disabled`, as each earlier version did:
```bash
M="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/mods"; \
  cd /d/Cyberpunk-Minecraft/minecraft/cybercraft-tweaks && bash build.sh 2>&1 | tail -1 \
  && mv "$M/cybercraft-tweaks-1.6.0.jar" "$M/cybercraft-tweaks-1.6.0.jar.disabled" \
  && cp build/cybercraft-tweaks-1.7.0.jar "$M/" && ls "$M" | grep cybercraft
```
Expected: `built build/cybercraft-tweaks-1.7.0.jar`, then `cybercraft-tweaks-1.7.0.jar` beside 1.2.0 to 1.6.0, each
`.jar.disabled`.

- [x] **Step 3: Ask the user to start Minecraft, and check that the mod loaded**

The user starts the instance and says when its title screen shows. Then:
```bash
L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"; \
  grep -E "cybercraft_tweaks [0-9.]+" "$L" | head -1; \
  echo "mixin errors: $(grep -c -E "Mixin.*(rror|ailed)|InvalidInjection|MixinApplyError|cybercraft_tweaks.*(rror|xception)" "$L")"
```
Expected: `- cybercraft_tweaks 1.7.0` and `mixin errors: 0`. The mixins on Minecraft's pathfinding apply when their
classes first load, in the world: the next step checks the log again.

- [x] **Step 4: In Minecraft: run step 0's script with 1.7.0**

```bash
L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"; before=$(wc -l < "$L"); \
  cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/step0 \
  && ./build/RelWithDebInfo/mclink.exe tools/mclink/paths.txt | tee build/step0/paths.log; echo "exit: ${PIPESTATUS[0]}"; \
  tail -n +$((before + 1)) "$L" | grep -E "\[CHAT\]|SkyCraft: |CyberCraft Tweaks: |WARN|rror|xception" | tee build/step0/paths-minecraft.log
```
Give the command 9 minutes. Expected, with 1.7.0, against the spec's "Step 0" and the simulation's numbers:
- **Linking:** as in the control, and no error or exception from a mixin in Minecraft's log.
- **Wander:** each mob's 13 positions change at least twice (two strolls), each reaches 3 blocks or more from where it
  was summoned, and every y is 6.0 to 7.5, on the road (the simulation reaches nothing lower within 30 blocks); the
  three `OnGround` lines say `1b`.
- **Tempt:** `Test passed, count: 1`, the cow beside V on the pavement.
- **Pet:** `Test passed, count: 1` after the walk, and again after the jump north, the wolf's position within 4 blocks
  of 1276.5 6.0 2000.5 at y 6.0, the street's floor, not inside it, and `OnGround` `1b`.
- **Chase:** `Test passed, count: 1`.
- **Sight:** no arrow in the first scene's world records, V on the road hidden from the skeleton; arrows in the second,
  V on the ledge in its view.
- **Flyer:** `Test passed, count: 1`, the bee within 4 blocks of 1290.5 -1.25 2007.5 on the slope.
- **Cost:** `CyberCraft Tweaks: paths in the last minute:` lines, with city answers, shapes classified, sight casts
  (and hits, while the skeleton was hidden) and a pet landed; the casting time a few milliseconds a minute.
- **The end:** as in the control: the game mode back, `world: nothing`, no errors, `exit: 0`.

- [ ] **Step 5: Ask the user to stop Minecraft**

Run F, in Part 2, needs the game; Minecraft starts again then.

- [x] **Step 6: Record step 0's results in `docs/spikes/2026-10-08-minecraft-paths-results.md`**

Beside the control's, from `build/step0/paths.log` and `build/step0/paths-minecraft.log`: each check with 1.7.0, its
outcome and numbers (the strolls a minute, how far each mob went, how long the cow, the wolf, the husk and the bee
took, the arrows), the minute lines, and what it means for Part 2.

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-08-minecraft-paths-results.md docs/superpowers/plans/2026-10-08-minecraft-paths.md \
  && git commit -q -m "minecraft-paths: step 0's results with CyberCraft Tweaks 1.7.0" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 10: Close Part 1, and plan Part 2

- [ ] **Step 1: Record "How Part 1 went" at the end of this plan**

What ran as written, what didn't and why, the fixes, and the tests' and checks' final counts.

- [ ] **Step 2: Record the spec's "Changes while executing Part 1"**

Only if executing changed the design.

- [ ] **Step 3: Plan Part 2 with superpowers:writing-plans**

From step 0's results: the overlay's Paths Spawn row (`spikes/block-render/tools/blockrender_hud/init.lua`), deployed
with the game closed; run F in the game, at the pavement area by spike 3's corner (step 0's ground) and then V's
apartment and its building's corridors (doorways, stairs, railings); and closing the spike. Verified as this part was,
and appended to this plan.

- [ ] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/superpowers \
  && git commit -q -m "minecraft-paths plan: how Part 1 went, and Part 2 planned" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

## Found while planning Part 1

- **Mixins name their target class, one per class:** a mixin's handlers apply to every class it targets, so the spec's
  five groups became eight classes (Task 4 and 5's), and `CityRays` holds the casts the spec gave to `CityCells`.
  `remap = false` on each: 26.3 is unobfuscated and the mod ships no refmap.
- **Nothing checked the mixins' targets before Minecraft's start:** `javac` doesn't, and Mixin's annotation processor,
  in the mixin jar, runs only when asked (JDK 23 and later no longer run classpath processors by themselves) and then
  checks targets only for remapping, which `remap = false` turns off. Hence `mixins.sh` (Task 3).
- **MixinExtras is in Fabric Loader** (0.5.5, as a nested jar), but not on `build.sh`'s classpath; Sponge's own
  `@Inject`, `@Redirect` and `@Invoker` do all this needs, so `build.sh` is unchanged.
- **`isSolid` is deprecated in 26.3,** and `findSurfacePosition` still reads it: `CityCells.searchKind` does the same,
  with the deprecation suppressed and said why.
- **A summon with an NBT argument skips `finalizeSpawn`** (`SummonCommand` passes false), so a summoned skeleton has no
  bow unless its `equipment` gives one. Its helmet is unbreakable, since at noon a leather helmet's damage would wear
  through in about 5 s and the skeleton would burn; `drop_chances` of 0 keep its gear from dropping when it's killed.
- **Step 0's sight check is a skeleton, not the spec's pillager:** a pillager's crossbow reaches 8 blocks
  (`RangedCrossbowAttackGoal`'s radius), and the rock slope below the road is 9 to 16 blocks from V there; a skeleton's
  bow reaches 15. Rays against the dump's 64,192 static triangles, as `SkyClip.cast` casts, found every slope spot
  from x 1288 to 1300 hidden from V's eye anywhere on the road (x 1276 to 1282), and the skeleton's spot (1290.5
  -1.25 2007.5) in view from the ledge behind the barrier (7.7 blocks) and from the slope (4 to 10).
- **The dump's ground, from the simulation:** the road runs along z, its street at 6.0 and its pavement at 6.2 from x
  1278, flat to z 2038 and rising north to 7.0 by z 1975; a barrier at x 1284 with its top at 7.5; a ledge behind it at
  5.5; then 6 m down to a rock slope falling east from −0.5 (x 1288) to about −6 (x 1308). Below the road, other
  levels (2.1, −2), which no stroll reaches. So the walks go south, the wolf's jump goes back north along the street,
  and the bee goes over the barrier onto the ledge, then down onto the slope, each hop within its 10-block tempt range.
- **A wolf follows only from 10 blocks,** so V walks about 15 (3.5 s at a walk); the wolf starts 2 blocks west of V,
  out of V's walking line.
- **The off hand only if it's empty:** `/execute unless items entity @p weapon.offhand *` guards the wheat, `if items`
  the swap to the dandelion and the clearing, so an item the user keeps there is never touched; the tempt and flyer
  checks would then fail, and say so.
- **The game mode is noted and put back** with a scoreboard (`/execute store success score … if entity
  @p[gamemode=…]`), since the script needs creative and then survival.
