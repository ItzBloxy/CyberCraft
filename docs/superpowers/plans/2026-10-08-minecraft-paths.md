# Minecraft's Mobs Finding Their Way Implementation Plan, Parts 1 and 2 (CyberCraft Tweaks 1.7.0 and 1.7.1, step 0, run F)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> In this project, plans run inline with superpowers:executing-plans, never through sub-agents.

**Goal:** Part 1 of spike 9: CyberCraft Tweaks 1.7.0, with which Minecraft's mobs find their way on Night City's
ground (wander, go to targets, keep up with their owner as pets, fly around its walls, and don't see through them),
then step 0 of the spec: the same `mclink` script on spike 1's dumped ground twice, first with 1.6.0 as the control,
then with 1.7.0. Tasks 10 to 14, added after step 0's first run with 1.7.0, make CyberCraft Tweaks 1.7.1 (thin city
cells are fences; a pet's landing resets its fall distance) and run step 0 again with it, with a pet's teleports.
Part 2, Tasks 16 to 21, gives the overlay its Paths rows and the way to run F's spots, types the rows' commands into
Minecraft with `mclink` on run F's own ground, then has run F in the game, at spot B and in V's apartment, and
closes the spike.

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

**Part 2** (the overlay's Paths rows, run F in the game, and closing the spike) was planned after step 0, from its
results and a simulation of run F's ground: it's Tasks 16 to 21, after Task 15, with its own "Before you start" and
file map.

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
  - Tasks 10 to 13, added while executing, were verified the same way: replayed from this plan's text in a fresh
    clone, with 56 unit checks and 12 mixin checks passing, zero warnings, both scripts parsing, and the trees
    identical to the scratch clone's. 18 mutations of `CellRule` (the fence rule's 8, and 10 of Part 1's rules
    again) each failed the check that guards it.
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

- [x] **Step 5: Ask the user to stop Minecraft**

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

### Task 10: Thin city cells are fences

Added while executing Part 1, after step 0's first run with 1.7.0 and its diagnostics (Task 9; the results doc has
them). Mobs walked onto the road's barrier, a skin 1/8 block thick and about 1.25 m tall, whose cells counted as a full
block with a floor on top (its last half block of skin was even a floor cell), and off its far side onto the ledge
behind and the levels below. The user chose to make such cells fences (2026-10-08). `CellRule` gains a fourth kind,
`THIN`: a cell whose middle has fewer than half (18) of its 36 columns covered, a column counting as covered if the
cell has anything in it or the cell below reaches into its upper half there (a kerb's cell, part pavement and part
the street's top below, stays a floor). A fence is impassable (`BLOCKED`), solid to random positions, never a stroll
target, and the cell above it stands half a block higher than its top (`FENCE_RISE`), as on a Minecraft fence's
1.5-block collision, so the jump check refuses it. `CityCells` keeps each shape's 64 column tops instead of its class,
since a class now takes the cell below too; `PathStats` counts shapes read.

On the deep review's simulation of the dump, the rule takes the barrier's top (64 places) and the ledge behind it (54)
out of reach, and nothing of the street and pavement; 1,597 of the 20,162 cells near V are fences.

**Files:**
- Modify: `minecraft/cybercraft-tweaks/src/test/java/dev/cybercraft/tweaks/paths/CellRuleTest.java`,
  `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/paths/CellRule.java`,
  `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/paths/CityCells.java`,
  `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/paths/PathStats.java`

- [x] **Step 1: Edit `minecraft/cybercraft-tweaks/src/test/java/dev/cybercraft/tweaks/paths/CellRuleTest.java` for fences**

```java
        check(CellRule.kind(CellRule.classify(edges)) == CellRule.NONE, "walls along the edges leave the middle empty");
        int[] corner = new int[64];
        corner[63] = 6;
        check(CellRule.classify(corner) == packed(CellRule.NONE, 0), "a sliver in a corner column is no foothold");
        for (int layer = 0; layer < 4; layer++) {
            int cell = CellRule.classify(all(layer + 1));
```
```java
        check(CellRule.kind(CellRule.classify(edges, null)) == CellRule.NONE, "walls along the edges leave the middle empty");
        int[] corner = new int[64];
        corner[63] = 6;
        check(CellRule.classify(corner, null) == packed(CellRule.NONE, 0), "a sliver in a corner column is no foothold");
        for (int layer = 0; layer < 4; layer++) {
            int cell = CellRule.classify(all(layer + 1), null);
```
```java
            int cell = CellRule.classify(all(layer + 1));
```
```java
            int cell = CellRule.classify(all(layer + 1), null);
```
```java
        check(CellRule.classify(beam) == packed(CellRule.BLOCKED, 7), "a floor with a beam above it is blocked");
```
```java
        check(CellRule.classify(beam, null) == packed(CellRule.BLOCKED, 7), "a floor with a beam above it is blocked");
        check(CellRule.classify(all(8), null) == packed(CellRule.BLOCKED, 8), "a full cell is blocked, its top 1");
        check(CellRule.classify(new int[64], null) == packed(CellRule.NONE, 0), "an empty cell is none");
        check(CellRule.classify(new int[64], all(8)) == packed(CellRule.NONE, 0), "an empty cell on a full one is none");
        thin();
    }

    /** Fences: a cell whose middle is mostly open, counting the cell below's upper half as covering a column. */
    private static void thin() {
```
```java
        check(CellRule.classify(wall) == packed(CellRule.BLOCKED, 8), "a wall through the middle is blocked");
        check(CellRule.classify(all(8)) == packed(CellRule.BLOCKED, 8), "a full cell is blocked, its top 1");
        check(CellRule.classify(new int[64]) == packed(CellRule.NONE, 0), "an empty cell is none");
```
```java
        check(CellRule.classify(wall, null) == packed(CellRule.THIN, 8), "a wall through the middle is a fence");
        int[] railing = new int[64];
        for (int z = 0; z < 8; z++) {
            railing[z * 8 + 3] = 4;
        }
        check(CellRule.classify(railing, null) == packed(CellRule.THIN, 4),
            "a railing's last half block of skin is a fence, not a floor");
        int[] ledge = new int[64];
        for (int z = 0; z < 8; z++) {
            for (int x = 0; x < 3; x++) {
                ledge[z * 8 + x] = 2;
            }
        }
        check(CellRule.classify(ledge, null) == packed(CellRule.THIN, 2), "a floor's last sliver over a drop is a fence");
        int[] floorOnHalf = new int[64];
        int[] streetBelow = new int[64];
        for (int z = 0; z < 8; z++) {
            for (int x = 0; x < 8; x++) {
                if (x < 3) {
                    floorOnHalf[z * 8 + x] = 2;
                } else {
                    streetBelow[z * 8 + x] = 8;
                }
            }
        }
        check(CellRule.classify(floorOnHalf, streetBelow) == packed(CellRule.FLOOR, 2),
            "a kerb's cell, part pavement and part the street's top below, is a floor");
        int[] lowBelow = new int[64];
        for (int i = 0; i < 64; i++) {
            lowBelow[i] = streetBelow[i] > 0 ? 4 : 0;
        }
        check(CellRule.classify(floorOnHalf, lowBelow) == packed(CellRule.THIN, 2),
            "the cell below's lower half covers nothing");
        int[] poleOnStreet = new int[64];
        poleOnStreet[3 * 8 + 3] = 8;
        check(CellRule.classify(poleOnStreet, all(8)) == packed(CellRule.BLOCKED, 8),
            "a pole standing on the street's top is a block");
        check(CellRule.classify(poleOnStreet, null) == packed(CellRule.THIN, 8), "a pole above nothing is a fence");
        int[] eighteen = new int[64];
        for (int z = 1; z < 4; z++) {
            for (int x = 1; x < 7; x++) {
                eighteen[z * 8 + x] = 2;
            }
        }
        check(CellRule.kind(CellRule.classify(eighteen, null)) == CellRule.FLOOR, "half the middle covered is a floor");
        eighteen[3 * 8 + 6] = 0;
        check(CellRule.kind(CellRule.classify(eighteen, null)) == CellRule.THIN, "one column short of half is a fence");
```
```java
        check(Double.isNaN(CellRule.floorAt(6, blocked, -1)), "inside a blocked cell: no floor");
        check(CellRule.stable(floor, -1), "a floor cell is a stroll target");
        check(CellRule.stable(none, blocked), "just above a blocked cell is a stroll target");
```
```java
        check(Double.isNaN(CellRule.floorAt(6, blocked, -1)), "inside a blocked cell: no floor");
        int fence = packed(CellRule.THIN, 8);
        check(near(CellRule.floorAt(7, none, fence), 7.5), "above a fence, half a block over its top");
        check(Double.isNaN(CellRule.floorAt(6, fence, -1)), "inside a fence: no floor");
        check(CellRule.stable(floor, -1), "a floor cell is a stroll target");
        check(CellRule.stable(none, blocked), "just above a blocked cell is a stroll target");
        check(!CellRule.stable(none, fence), "above a fence isn't a stroll target");
        check(!CellRule.stable(fence, -1), "a fence isn't a stroll target");
```

- [x] **Step 2: Run CyberCraft Tweaks' unit tests; they must fail, `classify` taking no cell below yet**

```bash
cd /d/Cyberpunk-Minecraft/minecraft/cybercraft-tweaks && bash test.sh 2>&1 | grep -m 2 "error"; echo "exit: ${PIPESTATUS[0]}"
```
Expected: `method classify in class CellRule cannot be applied to given types` and `exit: 1`.

- [x] **Step 3: Edit `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/paths/CellRule.java` for fences**

```java
 * nothing solid under them; a top in the upper half is {@link #BLOCKED}, a full block the cell above stands on.
```
```java
 * nothing solid under them; a top in the upper half is {@link #BLOCKED}, a full block the cell above stands on.
 *
 * <p>A cell whose middle is mostly open is a {@link #THIN} one, a fence: a railing, a barrier, a pole, a ledge's last
 * sliver over a drop. A middle column counts as covered if the cell has anything in it, or the cell below reaches into
 * its upper half there, so a kerb's cell, half pavement and half the street's top below, isn't one. Spike 9's step 0
 * saw mobs walk onto the road's thin barrier, which as a full block had a floor on top, and fall off its far side.
```
```java
    public static final int BLOCKED = 2;
```
```java
    public static final int BLOCKED = 2;
    /** Fewer than half the middle's columns covered: a fence, impassable, its top half a block higher than it is. */
    public static final int THIN = 3;

    /** How much higher than its top a fence's top counts, as a Minecraft fence's 1.5-block collision does. */
    public static final double FENCE_RISE = 0.5;
```
```java
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
```
```java
     * tops and those of the cell below ({@code null} for none): index {@code z * 8 + x}, each 0 for an empty column, or
     * 1 to 8 for one whose highest voxel is in layer 0 to 7. Only the middle 6 × 6 columns count.
     */
    public static int classify(int[] tops, int[] below) {
        int top = 0, covered = 0;
        for (int z = 1; z < 7; z++) {
            for (int x = 1; x < 7; x++) {
                int i = z * 8 + x;
                top = Math.max(top, tops[i]);
                if (tops[i] > 0 || below != null && below[i] > 4) {
                    covered++;
                }
            }
        }
        int kind = top == 0 ? NONE : covered < 18 ? THIN : top <= 4 ? FLOOR : BLOCKED;
        return kind | top << 2;
    }

    /** A packed cell's kind: {@link #NONE}, {@link #FLOOR}, {@link #BLOCKED} or {@link #THIN}. */
```
```java
     * Where a mob in cell {@code y} stands: on a floor in the cell itself, or on the top of a floor or blocked cell
     * below; {@code NaN} if neither. Each of {@code here} and {@code below} is a packed cell, or -1 where the city
     * doesn't count (no shape, or Minecraft's own block there).
```
```java
     * Where a mob in cell {@code y} stands: on a floor in the cell itself, or on the top of a floor, blocked or thin
     * cell below, a thin one's {@link #FENCE_RISE} higher; {@code NaN} if neither. Each of {@code here} and
     * {@code below} is a packed cell, or -1 where the city doesn't count (no shape, or Minecraft's own block there).
```
```java
            return y - 1 + top(below);
```
```java
            return y - 1 + top(below) + (kind(below) == THIN ? FENCE_RISE : 0.0);
```
```java
    /** Whether a mob may stroll to cell {@code here}: a floor itself, or just above a blocked cell. */
```
```java
    /** Whether a mob may stroll to cell {@code here}: a floor itself, or just above a blocked cell, not a thin one. */
```

- [x] **Step 4: Run CyberCraft Tweaks' unit tests; they must pass**

```bash
cd /d/Cyberpunk-Minecraft/minecraft/cybercraft-tweaks && bash test.sh 2>&1 | tail -3; echo "exit: ${PIPESTATUS[0]}"
```
Expected: `56 check(s), 0 failure(s)` and `exit: 0`, with no line starting FAIL.

- [x] **Step 5: Edit `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/paths/CityCells.java` to read the cell below, and fences**

```java
 * <p>A cell's class depends on its shape alone, so it's kept per shape object, weakly: SkyCraft builds new shapes
 * whenever a region is sent again, and the old ones go with their region. {@code VoxelShape} keeps
 * {@code Object.equals}, so the map goes by identity.
```
```java
 * <p>A cell's class depends on its shape and the shape of the cell below it (a thin cell's columns can be covered from
 * below), so what's kept is each shape's 64 column tops, per shape object, weakly: SkyCraft builds new shapes whenever
 * a region is sent again, and the old ones go with their region. {@code VoxelShape} keeps {@code Object.equals}, so
 * the map goes by identity.
```
```java
    private static final Map<VoxelShape, Integer> CLASSES = Collections.synchronizedMap(new WeakHashMap<>());
```
```java
    private static final Map<VoxelShape, int[]> TOPS = Collections.synchronizedMap(new WeakHashMap<>());
```
```java
    /** The city's cell at {@code pos}, packed as {@link CellRule#classify} packs it, or -1 where SkyCraft has none. */
    public static int cell(BlockPos pos) {
        VoxelShape shape = SkyCollision.shapeAt(pos);
        if (shape == null) {
            return -1;
        }
        Integer known = CLASSES.get(shape);
```
```java
    /** The 64 column tops of SkyCraft's shape at {@code pos}, as {@link CellRule#classify} takes them, or null. */
    private static int[] tops(BlockPos pos) {
        VoxelShape shape = SkyCollision.shapeAt(pos);
        if (shape == null) {
            return null;
        }
        int[] known = TOPS.get(shape);
```
```java
        int cell = CellRule.classify(tops);
        CLASSES.put(shape, cell);
        PathStats.classified();
        return cell;
```
```java
        TOPS.put(shape, tops);
        PathStats.shapeRead();
        return tops;
    }

    /** The city's cell at {@code pos}, packed as {@link CellRule#classify} packs it, or -1 where SkyCraft has none. */
    public static int cell(BlockPos pos) {
        int[] tops = tops(pos);
        return tops == null ? -1 : CellRule.classify(tops, tops(pos.below()));
```
```java
    /** The path type for a cell vanilla calls {@code OPEN}: {@code WALKABLE} for a floor, {@code BLOCKED}, or null. */
```
```java
    /** The path type for a cell vanilla calls {@code OPEN}: {@code WALKABLE} for a floor, else {@code BLOCKED}, or null. */
```
```java
    /** Whether this cell is a blocked city cell: solid, for random positions. */
    public static boolean blockedAt(BlockGetter level, BlockPos pos) {
        int cell = open(level, pos);
        boolean blocked = cell >= 0 && CellRule.kind(cell) == CellRule.BLOCKED;
```
```java
    /** Whether this cell is a blocked or thin city cell: solid, for random positions. */
    public static boolean blockedAt(BlockGetter level, BlockPos pos) {
        int cell = open(level, pos);
        boolean blocked = cell >= 0 && (CellRule.kind(cell) == CellRule.BLOCKED || CellRule.kind(cell) == CellRule.THIN);
```

- [x] **Step 6: Edit `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/paths/PathStats.java` to count shapes read**

```java
 * answers, the shapes classified, the sight and shortcut casts with their hits and time, and the pets landed on the
```
```java
 * answers, the shapes read, the sight and shortcut casts with their hits and time, and the pets landed on the
```
```java
    private static final AtomicLong CLASSIFIED = new AtomicLong();
```
```java
    private static final AtomicLong SHAPES = new AtomicLong();
```
```java
    public static void classified() {
        CLASSIFIED.incrementAndGet();
```
```java
    public static void shapeRead() {
        SHAPES.incrementAndGet();
```
```java
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
```
```java
        long answers = ANSWERS.getAndSet(0), shapes = SHAPES.getAndSet(0);
        long sight = SIGHT_CASTS.getAndSet(0), sightHits = SIGHT_HITS.getAndSet(0);
        long shortcuts = SHORTCUT_CASTS.getAndSet(0), shortcutHits = SHORTCUT_HITS.getAndSet(0);
        long nanos = CAST_NANOS.getAndSet(0), pets = PETS.getAndSet(0);
        if (answers + shapes + sight + shortcuts + pets == 0) {
            return;
        }
        CyberCraftTweaks.LOG.info("CyberCraft Tweaks: paths in the last minute: {} city answers, {} shapes read; "
            + "{} sight casts ({} hit), {} shortcut casts ({} hit), {} ms casting; {} pets landed on the city's floor",
            answers, shapes, sight, sightHits, shortcuts, shortcutHits, String.format("%.1f", nanos / 1e6), pets);
```

- [x] **Step 7: Build CyberCraft Tweaks, and check the mixins**

```bash
cd /d/Cyberpunk-Minecraft/minecraft/cybercraft-tweaks && out=$(bash build.sh 2>&1); echo "$out" | tail -1; \
  echo "warnings: $(echo "$out" | grep -c -i warning)"; bash mixins.sh 2>&1 | tail -1
```
Expected: `built build/cybercraft-tweaks-1.7.0.jar against 26.3-`, `warnings: 0` and `12 mixin check(s), 0 problem(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add minecraft/cybercraft-tweaks/src \
  && git commit -q -m "minecraft-paths: thin city cells are fences: mobs don't path through them or onto them" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 11: A pet's landing resets its fall distance

Also added while executing: a pet teleported to its owner while falling kept its fall distance, and took that whole
fall's damage as it landed (the control's test wolf, Task 9). `Entity.snapTo` doesn't reset it; `resetFallDistance`
does.

**Files:**
- Modify: `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/CityPetTeleport.java`

- [x] **Step 1: Edit `minecraft/cybercraft-tweaks/src/main/java/dev/cybercraft/tweaks/mixin/CityPetTeleport.java` to reset the fall distance**

```java
 * check and the landing go to that floor's height; anywhere else vanilla's check and landing stand.
```
```java
 * check and the landing go to that floor's height; anywhere else vanilla's check and landing stand. A pet landed so has
 * its fall distance reset: one teleported while falling (as the world loads, before the city's ground arrives) would
 * otherwise take that whole fall's damage as it lands, as a test wolf did in spike 9's step 0.
```
```java
        pet.snapTo(x + 0.5, floor, z + 0.5, pet.getYRot(), pet.getXRot());
```
```java
        pet.snapTo(x + 0.5, floor, z + 0.5, pet.getYRot(), pet.getXRot());
        pet.resetFallDistance();
```

- [x] **Step 2: Build CyberCraft Tweaks, and check the mixins**

```bash
cd /d/Cyberpunk-Minecraft/minecraft/cybercraft-tweaks && out=$(bash build.sh 2>&1); echo "$out" | tail -1; \
  echo "warnings: $(echo "$out" | grep -c -i warning)"; bash mixins.sh 2>&1 | tail -1
```
Expected: `built build/cybercraft-tweaks-1.7.0.jar against 26.3-`, `warnings: 0` and `12 mixin check(s), 0 problem(s)`.

- [x] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add minecraft/cybercraft-tweaks/src \
  && git commit -q -m "minecraft-paths: a pet's landing on the city's floor resets its fall distance" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 12: CyberCraft Tweaks 1.7.1, and step 0's scripts

- **1.7.1**, and the README says what's new.
- **Step 0's script:** every summon goes half a block above its floor, to drop onto it (one inside the city's skin
  falls through it: the pavement's skin spans 6.125 to 6.25, and the first runs summoned at 6.2); a second wandering
  villager starts on the pavement beside the barrier; the wander check's heights go to 7.0, where the road climbs at
  its north end; and the world is saved last (`/save-all flush`).
- **`tools/mclink/teleports.txt`:** a wolf following the stand-in V as it jumps 22 blocks along the street, five times,
  each landing watched every quarter second, with the dump placed as step 0 places it (a dump goes around V's feet as
  `dump` runs, which diagnostics 1 to 4 forgot).

**Files:**
- Modify: `minecraft/cybercraft-tweaks/src/main/resources/fabric.mod.json`, `minecraft/cybercraft-tweaks/README.md`,
  `spikes/block-render/tools/mclink/paths.txt`
- Create: `spikes/block-render/tools/mclink/teleports.txt`

- [x] **Step 1: Edit `minecraft/cybercraft-tweaks/src/main/resources/fabric.mod.json` for 1.7.1**

```json
  "version": "1.7.0",
```
```json
  "version": "1.7.1",
```

- [x] **Step 2: Edit `minecraft/cybercraft-tweaks/README.md` to say what 1.7.1 adds**

```markdown
  above stands on.
```
```markdown
  above stands on. A cell whose middle is mostly open, counting the cell below's upper half as covering a column, is a
  fence (from 1.7.1): a railing, a barrier, a pole, a ledge's last sliver over a drop. Mobs don't path through it or
  onto it, since its top counts half a block higher than it is, as a Minecraft fence's does; spike 9's step 0 saw them
  walk onto a road's barrier and fall off its far side.
```
```markdown
  it would fall through.
```
```markdown
  it would fall through, and with its fall distance reset (from 1.7.1), or one teleported while falling would take the
  whole fall's damage as it lands.
```
```markdown
face; without it, none of that happens but the skeleton's shooting, through the rock.
```
```markdown
face; without it, none of that happens but the skeleton's shooting, through the rock.
`spikes/block-render/tools/mclink/teleports.txt` makes a pet wolf follow its owner as the stand-in V jumps 22 blocks
along the same road five times: with the mod, it lands beside V on the street each time, on the ground.
```

- [x] **Step 3: Edit `tools/mclink/paths.txt` to drop summons onto their floors, watch the barrier, and save**

```text
# husks. Everything summoned is tagged sp9, keeps no loot and drops nothing. Results come from the chat replies in
# Minecraft's log: positions (/data get), "Test passed" or "Test failed" (/execute if entity), and the scenes' world
# records (arrows). The player's off hand is used only if it's empty; its game mode is noted at the start and put back
# at the end, and its effects are cleared. Start Minecraft first; this waits for its world. W (26) walks.
```
```text
# husks. Everything summoned is tagged sp9, keeps no loot and drops nothing, and goes half a block above its floor, to
# drop onto it: a mob summoned inside the city's 1/8-block skin falls through it. Results come from the chat replies in
# Minecraft's log: positions (/data get), "Test passed" or "Test failed" (/execute if entity), and the scenes' world
# records (arrows). The player's off hand is used only if it's empty; its game mode is noted at the start and put back
# at the end, and its effects are cleared. Last, the world is saved, so stopping Minecraft can't lose any of this. Start
# Minecraft first; this waits for its world. W (26) walks.
```
```text
# 1. Wander: a pig, a cow and a villager on the street west of V, their positions every 5 s for 60 s. Each should
# stroll at least twice and move 3 blocks or more, and stay on the road, at y 6.0 to 7.5.
type /summon minecraft:pig 1272.5 6.0 2007.5 {Tags:["sp9","sp9w"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
type /summon minecraft:cow 1272.5 6.0 2012.5 {Tags:["sp9","sp9w"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
type /summon minecraft:villager 1272.5 6.0 2002.5 {Tags:["sp9","sp9w"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
```
```text
# 1. Wander: a pig, a cow and a villager on the street west of V, and a villager on the pavement beside the barrier,
# their positions every 5 s for 60 s. Each should stroll at least twice and move 3 blocks or more, and stay on the road,
# at y 6.0 to 7.0 (it climbs to 7.0 at its north end): none on the barrier's top (7.5), a fence, nor below the road.
type /summon minecraft:pig 1272.5 6.5 2007.5 {Tags:["sp9","sp9w"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
type /summon minecraft:cow 1272.5 6.5 2012.5 {Tags:["sp9","sp9w"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
type /summon minecraft:villager 1272.5 6.5 2002.5 {Tags:["sp9","sp9w"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
type /summon minecraft:villager 1281.5 6.75 1995.5 {Tags:["sp9","sp9w"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
```
```text
type /summon minecraft:cow 1278.5 6.2 2009.5 {Tags:["sp9","sp9cow"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
```
```text
type /summon minecraft:cow 1278.5 6.75 2009.5 {Tags:["sp9","sp9cow"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
```
```text
type /summon minecraft:wolf 1274.5 6.0 2007.5 {Tags:["sp9","sp9wolf"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
```
```text
type /summon minecraft:wolf 1274.5 6.5 2007.5 {Tags:["sp9","sp9wolf"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
```
```text
type /summon minecraft:husk 1276.5 6.0 2017.5 {Tags:["sp9","sp9husk"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
```
```text
type /summon minecraft:husk 1276.5 6.5 2017.5 {Tags:["sp9","sp9husk"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
```
```text
type /summon skeleton 1290.5 -1.25 2007.5 {Tags:["sp9","sp9skel"],PersistenceRequired:1b,DeathLootTable:"empty",equipment:{mainhand:{id:bow},head:{id:iron_helmet,components:{unbreakable:{}}}},drop_chances:{mainhand:0f,head:0f}}
```
```text
type /summon skeleton 1290.5 -0.75 2007.5 {Tags:["sp9","sp9skel"],PersistenceRequired:1b,DeathLootTable:"empty",equipment:{mainhand:{id:bow},head:{id:iron_helmet,components:{unbreakable:{}}}},drop_chances:{mainhand:0f,head:0f}}
```
```text
scene
status
```
```text
scene
type /save-all flush
wait 3
status
```

- [x] **Step 4: Write `tools/mclink/teleports.txt`**

```text
# Spike 9's teleports: a pet following V as V jumps 22 blocks along the street, five times, each landing watched every
# quarter second for 4 s (Pos, OnGround), on spike 1's raised-level dump placed as step 0 places it (around 1280.5 6.14
# 2007.5: a dump goes around the stand-in V's feet as it's loaded). Step 0's first run with 1.7.0 once found its wolf
# under the road 4 s after such a jump. The wolf is tagged sp9 and killed at the end; nothing else is summoned; the
# world is saved last. Start Minecraft first; this waits for its world.
status
wait-world 180
ground city
at 1280.5 6.14 2007.5
look 0 0
dump E:\Cyberpunk2077\red4ext\plugins\PhysXProbe\dumps\2026-10-01_14-37-07\collision.obj
link on
wait-ground 30
wait 2
at 1276.5 6.0 2022.5
wait 2
type /summon minecraft:wolf 1274.5 6.5 2022.5 {Tags:["sp9","sp9wolf"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
type /data modify entity @e[tag=sp9wolf,limit=1] Owner set from entity @p UUID
wait 3
at 1276.5 6.0 2000.5
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute if entity @e[tag=sp9wolf,distance=..4]
wait 2
at 1276.5 6.0 2022.5
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute if entity @e[tag=sp9wolf,distance=..4]
wait 2
at 1276.5 6.0 2000.5
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute if entity @e[tag=sp9wolf,distance=..4]
wait 2
at 1276.5 6.0 2022.5
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute if entity @e[tag=sp9wolf,distance=..4]
wait 2
at 1276.5 6.0 2000.5
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute as @e[tag=sp9wolf] run data get entity @s Pos
type /execute as @e[tag=sp9wolf] run data get entity @s OnGround
wait 0.1
type /execute if entity @e[tag=sp9wolf,distance=..4]
wait 2
type /kill @e[tag=sp9]
wait 3
scene
type /save-all flush
wait 3
status
quit
```

- [x] **Step 5: Build CyberCraft Tweaks, run its unit tests, and check the mixins**

```bash
cd /d/Cyberpunk-Minecraft/minecraft/cybercraft-tweaks && out=$(bash build.sh 2>&1); echo "$out" | tail -1; \
  echo "warnings: $(echo "$out" | grep -c -i warning)"; bash test.sh 2>&1 | tail -1; bash mixins.sh 2>&1 | tail -1
```
Expected: `built build/cybercraft-tweaks-1.7.1.jar against 26.3-`, `warnings: 0`, `56 check(s), 0 failure(s)` and
`12 mixin check(s), 0 problem(s)`.

- [x] **Step 6: Check that mclink parses step 0's scripts**

As in Task 7: each with a line `mclink` doesn't know at its end, so nothing runs, even with Minecraft open.
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build && for s in paths teleports; do \
  { cat tools/mclink/$s.txt; echo "parse-check-stop here"; } > build/$s-parse.txt; \
  ./build/RelWithDebInfo/mclink.exe build/$s-parse.txt; echo "exit: $?"; done
```
Expected: `paths-parse.txt: line 161: unknown action: "parse-check-stop here"`, `teleports-parse.txt: line 282: unknown action: "parse-check-stop here"`
and `exit: 2`.

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add minecraft/cybercraft-tweaks/src minecraft/cybercraft-tweaks/README.md \
       spikes/block-render/tools/mclink/paths.txt spikes/block-render/tools/mclink/teleports.txt \
  && git commit -q -m "minecraft-paths: CyberCraft Tweaks 1.7.1, and step 0's summons dropped onto their floors" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 13: Step 0's scripts don't save

Added while executing, at the next task's first step, which saved the world through the link first: singleplayer has
no `/save-all` (26.3 registers it, with `/save-on`, `/save-off` and `/stop`, on a dedicated server only), and Minecraft
answered `Unknown or incomplete command`. Nor did the pause menu save the linked game: opened by `mclink`'s `pause` and
closed by Esc, it left no `Saving and pausing game...` in the log and `level.dat` untouched, though it did in earlier
sessions' unlinked play. What saves the world is Minecraft's autosave, every 5 minutes, which writes `level.dat` and
the player's file in `players/data/` (22:32:51 and 22:37:51 that evening): before Minecraft is stopped, one autosave
must pass after the last script. So the two scripts lose their `/save-all flush`, and their headers say so.

**Files:**
- Modify: `spikes/block-render/tools/mclink/paths.txt`, `spikes/block-render/tools/mclink/teleports.txt`

- [x] **Step 1: Edit `tools/mclink/paths.txt` to leave the saving to Minecraft's autosave**

```text
# at the end, and its effects are cleared. Last, the world is saved, so stopping Minecraft can't lose any of this. Start
# Minecraft first; this waits for its world. W (26) walks.
```
```text
# at the end, and its effects are cleared. Singleplayer has no /save-all, and a linked game doesn't pause: Minecraft's
# autosave, every 5 minutes, saves all this, so let one pass before stopping Minecraft. Start Minecraft first; this
# waits for its world. W (26) walks.
```
```text
type /save-all flush
wait 3
```
```text
```

- [x] **Step 2: Edit `tools/mclink/teleports.txt` to leave the saving to Minecraft's autosave**

```text
# under the road 4 s after such a jump. The wolf is tagged sp9 and killed at the end; nothing else is summoned; the
# world is saved last. Start Minecraft first; this waits for its world.
```
```text
# under the road 4 s after such a jump. The wolf is tagged sp9 and killed at the end; nothing else is summoned. Let
# Minecraft's autosave (every 5 minutes) pass before stopping it. Start Minecraft first; this waits for its world.
```
```text
type /save-all flush
wait 3
```
```text
```

- [x] **Step 3: Check that mclink parses step 0's scripts**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build && for s in paths teleports; do \
  { cat tools/mclink/$s.txt; echo "parse-check-stop here"; } > build/$s-parse.txt; \
  ./build/RelWithDebInfo/mclink.exe build/$s-parse.txt; echo "exit: $?"; done
```
Expected: `paths-parse.txt: line 160: unknown action: "parse-check-stop here"`, `teleports-parse.txt: line 280: unknown action: "parse-check-stop here"`
and `exit: 2`.

- [x] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render/tools/mclink/paths.txt spikes/block-render/tools/mclink/teleports.txt \
  && git commit -q -m "minecraft-paths: step 0's scripts don't save: singleplayer has no /save-all" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 14: In Minecraft: CyberCraft Tweaks 1.7.1, step 0 and the teleports

Minecraft is still running from Task 9's diagnostics, in its world, its window hidden. The user's player is in
creative (from the stale save) with the shield in inventory slot 15 (Task 8): both go back at the end. No command
saves the world (Task 13): before the user stops Minecraft, which kills it unsaved, an autosave must pass after the
last script. This waits for one, run in the background for its `sleep` loop, and prints the world files' times:
```bash
W="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/saves/SkyCraft"; \
  M=/d/Cyberpunk-Minecraft/spikes/block-render/build/step0/saved-after; touch "$M"; \
  until [ "$W/level.dat" -nt "$M" ] && [ -n "$(find "$W/players/data" -name "*.dat" -newer "$M")" ]; do sleep 2; done; \
  date +%H:%M:%S; ls -la --time-style=+%H:%M:%S "$W/level.dat" "$W"/players/data/*.dat
```
Expected within 5 minutes: `level.dat` and the player's `.dat` both written after the mark.

**Files:**
- Modify: `docs/spikes/2026-10-08-minecraft-paths-results.md`

- [x] **Step 1: Wait for Minecraft's autosave, then ask the user to stop Minecraft**

Task 9's diagnostics were the last scripts, each ending with `world: nothing`. Wait for an autosave as above, then
the user stops Minecraft from the Modrinth App (this is also Task 9's step 5), and:
```bash
tasklist | grep -i -c "javaw.exe"
```
Expected: `0`.

- [x] **Step 2: Install CyberCraft Tweaks 1.7.1, with the user's leave**

```bash
M="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/mods"; \
  cd /d/Cyberpunk-Minecraft/minecraft/cybercraft-tweaks && bash build.sh 2>&1 | tail -1 \
  && mv "$M/cybercraft-tweaks-1.7.0.jar" "$M/cybercraft-tweaks-1.7.0.jar.disabled" \
  && cp build/cybercraft-tweaks-1.7.1.jar "$M/" && ls "$M" | grep cybercraft
```
Expected: `built build/cybercraft-tweaks-1.7.1.jar`, then `cybercraft-tweaks-1.7.1.jar` beside 1.2.0 to 1.7.0, each
`.jar.disabled`.

- [x] **Step 3: Ask the user to start Minecraft, and check that the mod loaded**

The user starts the instance and says when its title screen shows. Then:
```bash
L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"; \
  grep -E "cybercraft_tweaks [0-9.]+" "$L" | head -1; \
  echo "mixin errors: $(grep -c -E "Mixin.*(rror|ailed)|InvalidInjection|MixinApplyError|cybercraft_tweaks.*(rror|xception)" "$L")"
```
Expected: `- cybercraft_tweaks 1.7.1` and `mixin errors: 0`.

- [x] **Step 4: In Minecraft: run step 0's script with 1.7.1**

The user's off hand is empty (the shield is in slot 15), so the wheat and the dandelion go in.
```bash
L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"; before=$(wc -l < "$L"); \
  cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/step0 \
  && ./build/RelWithDebInfo/mclink.exe tools/mclink/paths.txt > build/step0/paths-1.7.1.log 2>&1; echo "exit: $?"; \
  tail -n +$((before + 1)) "$L" | grep -E "\[CHAT\]|SkyCraft: |CyberCraft Tweaks: |WARN|rror|xception" > build/step0/paths-1.7.1-minecraft.log
```
Give the command 9 minutes. Expected, against Task 9's run (26.3 words the tests `Test passed. Count: 1` and `Test
failed`):
- **Linking:** as before, and no error or exception from a mixin in Minecraft's log.
- **Wander:** 13 positions for each of the four mobs, each moving 3 blocks or more and strolling at least twice, every
  y 6.0 to 7.0: none at 7.5 on the barrier's top, none below the road; the four `OnGround` lines `1b`.
- **Tempt, pet, chase, flyer:** `Test passed. Count: 1` each (the pet twice), the wolf's last position within 4 blocks
  of 1276.5 6.0 2000.5, on the street's floor, `OnGround` `1b`.
- **Sight:** no arrow in the first scene, an arrow in the second.
- **Cost:** the minute lines, with `shapes read`.
- **The end:** the last scene `world: nothing`, no errors, `exit: 0`.

- [x] **Step 5: In Minecraft: run the teleports**

```bash
L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"; before=$(wc -l < "$L"); \
  cd /d/Cyberpunk-Minecraft/spikes/block-render \
  && ./build/RelWithDebInfo/mclink.exe tools/mclink/teleports.txt > build/step0/teleports.log 2>&1; echo "exit: $?"; \
  tail -n +$((before + 1)) "$L" | grep -E "\[CHAT\]|CyberCraft Tweaks: |WARN" > build/step0/teleports-minecraft.log; \
  grep -c "Test passed" build/step0/teleports-minecraft.log
```
Expected: `exit: 0` and `5`: after each of the five jumps the wolf is within 4 blocks of V. And, read from the log,
every one of its 80 samples on the ground (`1b`) at the street's floor (y 6.0 to 6.25), none under the road, and the
last scene `world: nothing`.

- [x] **Step 6: In Minecraft: the user's shield back in their off hand, and survival back**

The reverse of Task 8's move, guarded the same way: the shield copied into the off hand only if it's empty, slot 15
emptied only once the off hand holds it. The user's game mode is survival (their saved player data before step 0, and
what the control put back). Write `spikes/block-render/build/step0/restore.txt` (scratch, not committed) with the
Write tool (the Bash tool collapses backslashes):
```text
# The user's shield back from inventory slot 15 into the off hand after step 0, and their game mode back to survival.
# Guarded: the copy only into an empty off hand, slot 15 emptied only once the off hand holds the shield. Linked as
# step 0 is, on the same dumped ground. Minecraft's autosave saves it.
status
wait-world 60
ground city
at 1280.5 6.14 2007.5
look 0 0
dump E:\Cyberpunk2077\red4ext\plugins\PhysXProbe\dumps\2026-10-01_14-37-07\collision.obj
link on
wait-ground 30
wait 2
type /execute unless items entity @p weapon.offhand * run item replace entity @p weapon.offhand from entity @p container.15
type /execute if items entity @p weapon.offhand minecraft:shield run item replace entity @p container.15 with minecraft:air
type /gamemode survival
wait 1
type /execute if items entity @p weapon.offhand minecraft:shield
type /execute if items entity @p container.15 *
wait 2
status
quit
```
Then:
```bash
L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"; before=$(wc -l < "$L"); \
  cd /d/Cyberpunk-Minecraft/spikes/block-render \
  && ./build/RelWithDebInfo/mclink.exe build/step0/restore.txt > build/step0/restore.log 2>&1; echo "exit: $?"; \
  tail -n +$((before + 1)) "$L" | grep -E "\[CHAT\]" | sed 's/.*\[CHAT\] //'
```
Expected: `exit: 0`, `Replaced 1 slot(s) on ItzBloxy` (26.3 names no item for a copy), `Replaced 1 slot(s) on
ItzBloxy with [Air]`, `Set own game mode to Survival Mode`, `Test passed. Count: 1` (the shield in the off hand) and
`Test failed` (slot 15 empty).

- [x] **Step 7: Wait for Minecraft's autosave, then ask the user to stop Minecraft**

As in step 1. Run F, in Part 2, needs the game; Minecraft starts again then.

- [x] **Step 8: Record the results in `docs/spikes/2026-10-08-minecraft-paths-results.md`**

Step 0 with 1.7.1 beside the earlier runs, the teleports, and the restore: each check's outcome and numbers, the
minute lines, and what it means for Part 2.

- [x] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-08-minecraft-paths-results.md docs/superpowers/plans/2026-10-08-minecraft-paths.md \
  && git commit -q -m "minecraft-paths: step 0 with CyberCraft Tweaks 1.7.1, and the teleports" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 15: Close Part 1, and plan Part 2

- [x] **Step 1: Record "How Part 1 went" at the end of this plan**

What ran as written, what didn't and why, the fixes (Tasks 10 to 13 among them), and the tests' and checks' final
counts.

- [x] **Step 2: Record the spec's "Changes while executing Part 1"**

Beyond what this part's fix tasks already recorded there.

- [x] **Step 3: Plan Part 2 with superpowers:writing-plans**

From step 0's results: the overlay's Paths Spawn row (`spikes/block-render/tools/blockrender_hud/init.lua`), deployed
with the game closed; run F in the game, at the pavement area by spike 3's corner (step 0's ground) and then V's
apartment and its building's corridors (doorways, stairs, railings); and closing the spike. Verified as this part was,
and appended to this plan.

- [x] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/superpowers \
  && git commit -q -m "minecraft-paths plan: how Part 1 went, and Part 2 planned" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

## Part 2: the overlay's Paths rows, run F, and the spike's close

Part 2 takes spike 9 into the game (the spec's "Run F, in the game"). The overlay gets the Paths rows, which summon the
spec's test mobs ahead of V and fill slots 8 and 9 with what they follow, and a readout of the way to run F's spots; an
`mclink` script types the rows' commands into the user's Minecraft on run F's own ground first; then run F, with the
user, at spot B and in V's apartment; then the spike closes. It was planned after step 0 (Tasks 8 to 14), from its
results and a simulation of run F's ground (below). The spec's "Changes while planning Part 2" lists what planning
settled in the design, and "Found while planning Part 2", at the end of this plan, what it settled in the plan.
- **Task 16:** the overlay's Paths rows, its food row, and run F's spots, checked offline by running the overlay
  under Lua.
- **Task 17:** `tools/mclink/pathsrow.txt`, the rows' commands exactly as the overlay types them, on spot B's and the
  apartment's dumps placed where the game itself puts that ground.
- **Task 18:** that script in Minecraft, with the user, before the game.
- **Tasks 19 and 20:** run F in the game: at spot B, then in V's apartment, in Megabuilding H10.
- **Task 21:** the spike's close.

Nothing changes in CyberCraft Tweaks (1.7.1 stays installed), BlockRender's C++, `mclink`'s code or SkyCraft.

### What step 0 and planning found

From the results ("Step 0 with CyberCraft Tweaks 1.7.1") and the simulation of run F's ground, what Part 2 builds on:
- **Step 0 passed with 1.7.1:** wander, tempt, the pet twice, chase, sight and the flyer, at 2 to 3 ms of casting a
  minute, and 26 pets' landings of 26 on the road.
- **Summons go half a block up:** a mob summoned inside the city's 1/8-block skin falls through it, and V's feet can be
  inside one. The overlay's summons go at `~ ~0.5 ~`, the bee's at `~ ~1 ~`.
- **The user's choices** (2026-10-09): the wheat in slot 8 and the dandelion in slot 9, in place of the TNT and the
  flint and steel, which the "Your slots" row gives back; a simulation of V's apartment before planning its part of run
  F; and spot B first, then the apartment, though the apartment turned out to be 1.4 km from spot B and the save
  starts there.
- **The game doesn't put spot B where step 0 did.** BlockRender gives Minecraft the city where it is: V's feet at spot
  B were Minecraft −189.898 74.432 −616.754, while step 0's dump went around 1280.5 6.14 2007.5, 0.398, 0.708 and
  0.254 of a block off the game's grid. Voxelized where the game puts it, the road is as in step 0 (the street at
  74.25, the pavement at 74.5), but the barrier's wide north end (x −188 to −187, z −638 to −634, solid up to 75.75)
  is a wall top, not a fence: a mob steps onto it from the pavement's raised edge (1.125, its highest step) and drops
  2.6 blocks off its far side onto the ledge, then down the rock face in drops of 3 or less. Within 30 blocks of V
  that's 1,500 places below the road, which step 0's placement had none of. It's the spec's "wide wall tops stay
  walkable" and vanilla's 3-block drops, not a fault; run F's wanderers start south of V, 20 blocks from it, and the
  runs record any mob that takes it.
- **The dump called V's apartment wasn't:** `2026-10-08_01-44-52` (world −484 389, 132 m up) is a furnished room
  elsewhere, whose furniture's sloped faces become whole blocks (the voxelizer's 50 to 84° coarsening), splitting its
  floor into pockets that no mob two cells tall leaves. V's apartment is in Megabuilding H10 (world −1383 1272 123).
  The probe's F10 dumps find nothing there, since they don't pass the PhysX origin and away from the map's origin read
  the wrong place; with the user's V in its main room, a dump through the probe's request file, `origin -1024 1024 0`,
  read it (`2026-10-09_12-22-29`, 140,451 triangles, its coordinate check passed).
- **V's apartment, simulated where the game puts it:** V's room (75 places, a stroll finding a spot 89% of the time, a
  pet's landing having room 91% of the time in one tick) opens through its front door, a block wide at x −1389, z
  −1272, with a threshold at 123.375, onto the building's corridor (x −1395 to −1390). Through it a mob reaches 5,204
  places on five storeys 4 blocks apart (floors 111.125 to 127.125), joined by a stairwell at x −1403: six treads of
  about 0.65 block down, from z −1284 to −1290, and six up, from z −1293 to −1287. So the doorways and stairs the user
  asked run F for are there.
- **The husk's sight check needs a wall with a way around it:** spot B's road has nothing tall to hide V that the husk
  could walk around (its best cover is the barrier, from a ledge that has no way back up), so the hidden half is in V's
  apartment. V 3 blocks in from the front door, facing heading 300, has the husk summoned 7 ahead in the corridor,
  behind the room's wall: hidden from V's eye from each of the 81 pairs of neighbouring places, with a way around
  through the door 9 steps long; from the doorway, V is in its view.
- **How tall a mob is to a path:** 26.3 sizes a node by floor(height + 1) cells (`NodeEvaluator.prepare`): the pig and
  the wolf one, the cow, the villager and the husk two.
- **`mclink` can't pick the hotbar slot:** the user's slots 8 and 9 are on mouse buttons 5 and 4, and `mclink` clicks
  left, middle and right only. Unheld, the wheat and the dandelion tempt nothing (`TemptGoal` looks at both hands), so
  Task 18 checks that they go in, and leaves the tempt and the bee's follow to run F.

## Before you start (Part 2)

- **Part 1's "Before you start" holds,** its shell, Edit steps and commits included.
- **Already verified:** all of Part 2's files have been written and checked once already, in a scratch clone.
  - Tasks 16 and 17 were then replayed in a fresh clone by `tools/plans/run_plan_s9.py`, which applied every file step
    exactly as written and checked every expected result: the overlay's syntax, its requests and spots run offline
    under Lua 5.4, the script's parse, and that the script types each of the overlay's commands. The replayed
    `spikes/block-render` trees are identical to the scratch clone's, task by task.
  - Each new check was broken once, by a change to the code it guards, and failed as it should ("Found while planning
    Part 2").
  - The spots and every expected position come from the simulation of the two dumps where the game puts them
    (`tools/plans/paths-sim/`, "Found while planning Part 2").
- **What only Minecraft and the game can test:** the rows' commands against Minecraft 26.3 and SkyCraft's real mod
  (Task 18), and everything in run F (Tasks 19 and 20).
- **The user:**
  - **does everything in Minecraft, the Modrinth App and the game,** as in Part 1: never drive their mouse or keyboard;
    ask, then wait for them to say they're done;
  - **approves Task 18's script:** it summons test mobs at spot B's and the apartment's Minecraft places in the
    `SkyCraft` world, all killed as it goes, puts the wheat and the dandelion in slots 8 and 9 and then 64 TNT and a
    flint and steel back, gives Resistance and clears it, and notes the game mode and puts it back;
  - **approves the overlay's deploy,** with the game closed (Task 19).
- **Saving:** Minecraft's autosave, every 5 minutes, is what saves its world; after the last change, wait for one
  before the user stops Minecraft (Part 1's Task 14 has the wait).
- **During run F the agent reads, and asks:** it sends BlockRender read-only `mc` requests (positions, `/execute if`
  tests) through the plugin's own request file, and reads Minecraft's log, BlockRender's status and the overlay's
  frame times. Everything the user does goes through the overlay's buttons and the game.

## File map (Part 2)

| File | Task | What |
|---|---|---|
| `spikes/block-render/tools/blockrender_hud/init.lua` | 16, 18 | the Paths rows, the food row, run F's spots |
| `spikes/block-render/tools/mclink/pathsrow.txt` | 17, 18 | the rows' commands on run F's ground, as the game places it |
| `docs/spikes/2026-10-08-minecraft-paths-results.md` | 18 to 21 | the runs, and the spike's answer |
| `tools/plans/fun_check.lua`, `tools/plans/run_plan_s9.py`, `tools/plans/paths-sim/` | planning | a stand-in V for the overlay offline; Part 2's step kinds; the simulation's new scripts |

---

### Task 16: The overlay's Paths rows, and the way to run F's spots

The Fun header gets two rows and a readout (the spec's "Run F, in the game"):
- **Paths, ahead:** Pig, Cow and Villager, 4 blocks ahead (2 to the left, ahead, 2 to the right); Wolf, 2 ahead and 2
  to the left, its `Owner` then set from the player's UUID, which tames it; Bee, 2 ahead and 2 to the right, a block
  up; and Husk, Resistance at its highest for 300 s first, then the husk 7 ahead. Each mob goes half a block above the
  player's feet and is tagged `sp7`, as the Mobs row's are, so Clear removes it. The Pig's and the Villager's labels
  end `##paths`: ImGui takes buttons with the same label in one window for one button, and the Mobs row has a Pig and a
  Villager.
- **Paths' food:** Wheat (8) and Dandelion (9), into the user's slots 8 and 9; "Your slots" gives the TNT and the flint
  and steel back.
- **Run F's spots:** each spot's distance from V, the heading to walk it, how far up or down, and the heading to face
  there: spot B, V's room, the husk's two spots, and the stairs.

Each has its own place among the Spawn rows': the wolf and the bee 2 ahead, the wanderers 4, the husk 7, where no other
row puts anything.

**Files:**
- Modify: `spikes/block-render/tools/blockrender_hud/init.lua`

- [x] **Step 1: Edit `tools/blockrender_hud/init.lua`**

```lua
-- spawn ahead and clear again, typed into Minecraft's chat) and "Full status" (every status line, and the frame time).
```
```lua
-- spawn ahead and clear again, typed into Minecraft's chat; and the way to spike 9's run F's spots) and "Full status"
-- (every status line, and the frame time).
```
```lua
-- lands on another: the dropped items and the sand 3 blocks ahead, the potion and the campfire 3 ahead to either side,
-- the mobs 5, the firework 6, the block entities 8 and the herd 11 and 13. The mobs have no loot and the sand vanishes
```
```lua
-- lands on another: the Paths row's wolf and bee 2 blocks ahead to either side, the dropped items and the sand 3
-- ahead, the potion and the campfire 3 ahead to either side, the Paths row's wanderers 4, the mobs 5, the firework 6,
-- the Paths row's husk 7, the block entities 8 and the herd 11 and 13. The mobs have no loot and the sand vanishes
```
```lua
    } },
    -- Strict: no block updates, so neither half of the bed breaks the other with a drop.
```
```lua
    } },
    -- Spike 9's run F: Minecraft's mobs finding their way on the city's ground. Each goes half a block above the player's
    -- feet, which can be inside the city's 1/8-block skin, and drops onto the floor: one summoned inside the skin would
    -- fall through it. The pig, the cow and the villager wander; the wolf is the player's, its Owner set from the
    -- player's UUID; the bee, a block up, follows a dandelion; and the husk chases the player in survival, Resistance at
    -- its highest first so that nothing hurts. "##paths" keeps the Pig and the Villager apart from the Mobs row's, which
    -- ImGui would otherwise take for the same buttons.
    { label = "Paths, ahead:", buttons = {
        { label = "Pig##paths", commands = { mob("^2 ^ ^4", "pig", nil, "~ ~0.5 ~") } },
        { label = "Cow", commands = { mob("^ ^ ^4", "cow", nil, "~ ~0.5 ~") } },
        { label = "Villager##paths", commands = { mob("^-2 ^ ^4", "villager", nil, "~ ~0.5 ~") } },
        { label = "Wolf", commands = { mob("^2 ^ ^2", "wolf", nil, "~ ~0.5 ~"),
            "/data modify entity @e[type=minecraft:wolf,tag=sp7,limit=1,sort=nearest] Owner set from entity @s UUID" } },
        { label = "Bee", commands = { mob("^-2 ^ ^2", "bee", nil, "~ ~1 ~") } },
        { label = "Husk", commands = { "/effect give @s minecraft:resistance 300 4 true", mob("^ ^ ^7", "husk", nil, "~ ~0.5 ~") } },
    } },
    -- What the cow and the bee follow, in the user's slots 8 and 9 in place of the TNT and the flint and steel, which
    -- "Your slots" gives back: a tempted mob looks at both hands, and the off hand keeps its shield. The slot is picked in
    -- build mode.
    { label = "Paths' food:", buttons = {
        { label = "Wheat (8)", commands = { "/item replace entity @s hotbar.7 with minecraft:wheat" } },
        { label = "Dandelion (9)", commands = { "/item replace entity @s hotbar.8 with minecraft:dandelion" } },
    } },
    -- Strict: no block updates, so neither half of the bed breaks the other with a drop.
```
```lua
    } },
}

-- Requests not yet sent, oldest first. BlockRender reads blockrender.request in this folder once a second, once its line
```
```lua
    } },
}

-- Run F's spots (spike 9's Part 2), where its checks stand V, picked on a simulation of the city's dumps there: the
-- world position the V line shows (z up), and the heading to face. The Fun header shows how far each one is, the
-- heading to walk it, and how far up or down. V's room is in Megabuilding H10, 2 blocks in from its front door; the
-- husk can't see V hidden 1 block further in, but can from the doorway; the stairs go down to the storey below.
local spots = {
    { name = "Spot B", x = -189.90, y = 616.75, z = 74.43, face = 180 },
    { name = "V's room", x = -1385.5, y = 1271.5, z = 123.06, face = 90 },
    { name = "Hidden from the husk", x = -1384.5, y = 1271.5, z = 123.06, face = 300 },
    { name = "In the husk's view", x = -1388.5, y = 1271.5, z = 123.3, face = 300 },
    { name = "The stairs", x = -1402.5, y = 1283.5, z = 123.06, face = 0 },
}
local spotLines = nil    -- each spot's distance, heading and height from V, from onUpdate

-- Requests not yet sent, oldest first. BlockRender reads blockrender.request in this folder once a second, once its line
```
```lua
        player = nil
```
```lua
        player = nil
        spotLines = nil
```
```lua
    player = text
```
```lua
    player = text
    spotLines = {}
    for _, spot in ipairs(spots) do
        local dx, dy, dz = spot.x - p.x, spot.y - p.y, spot.z - p.z
        table.insert(spotLines, string.format("%s: %.1f m, heading %d, %.1f m %s; face %d there", spot.name,
            math.sqrt(dx * dx + dy * dy), math.floor(math.deg(atan2(dx, dy)) % 360 + 0.5) % 360, math.abs(dz),
            dz < 0 and "down" or "up", spot.face))
    end
```
```lua
                    end
                end
            end
        end

        if ImGui.CollapsingHeader("Full status") then
```
```lua
                    end
                end
            end
            if spotLines ~= nil then
                ImGui.Text("Run F's spots:")
                for _, line in ipairs(spotLines) do
                    ImGui.Text("    " .. line)
                end
            end
        end

        if ImGui.CollapsingHeader("Full status") then
```

- [x] **Step 2: Check the overlay's syntax**

```bash
luac -p /d/Cyberpunk-Minecraft/spikes/block-render/tools/blockrender_hud/init.lua && echo "syntax ok"
```
Expected: `syntax ok`.

- [x] **Step 3: Run the overlay offline: the Paths rows' requests**

`tools/plans/fun_check.lua` runs the overlay under Lua 5.4 with CET's functions stubbed, clicks the buttons it's given,
one a frame, and plays BlockRender's side of `blockrender.request`, printing each request in turn.
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/fun && cd build/fun \
  && lua /d/Cyberpunk-Minecraft/tools/plans/fun_check.lua ../../tools/blockrender_hud/init.lua click "Pig##paths" Cow \
       "Villager##paths" Wolf Bee Husk "Wheat (8)" "Dandelion (9)" | tr -d '\r' | tee requests.txt \
  && awk '/^request: mc /{n = length($0) - 12; if (n > m) m = n} END {print "longest command:", m}' requests.txt
```
Expected, one request a command, in order, the longest within Minecraft's 256-character chat line:
- `request: mc /execute rotated ~ 0 positioned ^2 ^ ^4 run summon minecraft:pig ~ ~0.5 ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}`
- `request: mc /execute rotated ~ 0 positioned ^ ^ ^4 run summon minecraft:cow ~ ~0.5 ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}`
- `request: mc /execute rotated ~ 0 positioned ^-2 ^ ^4 run summon minecraft:villager ~ ~0.5 ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}`
- `request: mc /execute rotated ~ 0 positioned ^2 ^ ^2 run summon minecraft:wolf ~ ~0.5 ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}`
- `request: mc /data modify entity @e[type=minecraft:wolf,tag=sp7,limit=1,sort=nearest] Owner set from entity @s UUID`
- `request: mc /execute rotated ~ 0 positioned ^-2 ^ ^2 run summon minecraft:bee ~ ~1 ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}`
- `request: mc /effect give @s minecraft:resistance 300 4 true`
- `request: mc /execute rotated ~ 0 positioned ^ ^ ^7 run summon minecraft:husk ~ ~0.5 ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}`
- `request: mc /item replace entity @s hotbar.7 with minecraft:wheat`
- `request: mc /item replace entity @s hotbar.8 with minecraft:dandelion`
- `longest command: 150`

- [x] **Step 4: Run the overlay offline: run F's spots**

`FUN_V` stands V where the apartment's dump found her (`2026-10-09_12-22-29`: her feet, in the game's world
coordinates, and her facing), and `show` prints what the overlay shows, the Fun header open.
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/fun && cd build/fun \
  && FUN_V="-1383.343 1271.700 123.065 -0.944 -0.330" lua /d/Cyberpunk-Minecraft/tools/plans/fun_check.lua \
       ../../tools/blockrender_hud/init.lua show | tr -d '\r' | grep -E "^text: (V:|Run F|    )"
```
Expected, V 1.4 km from spot B and beside the others:
- `V: x -1383.34  y 1271.70  z 123.06, heading 251 (0 north, 90 east)`
- `Spot B: 1361.3 m, heading 119, 48.6 m down; face 180 there`
- `V's room: 2.2 m, heading 265, 0.0 m down; face 90 there`
- `Hidden from the husk: 1.2 m, heading 260, 0.0 m down; face 300 there`
- `In the husk's view: 5.2 m, heading 268, 0.2 m up; face 300 there`
- `The stairs: 22.5 m, heading 302, 0.0 m down; face 0 there`

- [x] **Step 5: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render/tools/blockrender_hud/init.lua \
  && git commit -q -m "minecraft-paths: the overlay's Paths rows, and the way to run F's spots" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 17: An `mclink` script of the Paths rows, on run F's ground

`tools/mclink/pathsrow.txt` types the Paths rows' commands, and Clear, Survival and "Your slots"' slots 8 and 9,
exactly as the overlay types them, into the user's Minecraft, without the game, on run F's ground where the game itself
puts it: spot B's dump around V's own feet there, then the apartment's around hers (a dump goes around the stand-in V's
feet as `dump` runs). It checks each command against Minecraft 26.3 and SkyCraft's real mod before run F, and rehearses
run F's checks but the tempt and the bee's follow, which need the slot held ("What step 0 and planning found"). Each
check runs alone, cleared before the next. The player's game mode is noted in a scoreboard and put back, the
Resistance cleared, and slots 8 and 9 left as "Your slots" fills them.

**Files:**
- Create: `spikes/block-render/tools/mclink/pathsrow.txt`

- [x] **Step 1: Write `tools/mclink/pathsrow.txt`**

```text
# Spike 9's Part 2: the overlay's Paths rows, typed as its buttons type them, against SkyCraft's real mod, without
# the game, on run F's ground where the game itself puts it: spot B (spike 1's raised-level dump, 2026-10-01_14-37-07)
# around V's own feet there, Minecraft -189.898 74.432 -616.754, then V's apartment in Megabuilding H10
# (2026-10-09_12-22-29) around -1383.343 123.065 -1271.700. Step 0 placed spot B's dump at 1280.5 6.14 2007.5, a
# fraction of a block off the game's grid, so its cells weren't quite these
# (docs/superpowers/plans/2026-10-08-minecraft-paths.md, "Part 2").
#
# Each check runs alone and is cleared before the next, with the overlay's Clear. Results come from the chat replies
# in Minecraft's log: positions (/data get), "Test passed" or "Test failed" (/execute if entity). The player's game
# mode is noted at the start and put back at the end; its slots 8 and 9 get the wheat and the dandelion, and then the
# TNT and the flint and steel the overlay's "Your slots" gives; the husk's Resistance is cleared. Unselected, the wheat
# and the dandelion tempt nothing (the user picks the slot in build mode in run F), so the tempt and the bee's follow
# are left to run F. Start Minecraft first; this waits for its world. W (26) walks.
status
wait-world 180
ground city
at -189.898 74.432 -616.754
look 0 0
dump E:\Cyberpunk2077\red4ext\plugins\PhysXProbe\dumps\2026-10-01_14-37-07\collision.obj
link on
wait-ground 30
wait 2
type /scoreboard objectives add sp9mode dummy
type /execute store success score #creative sp9mode if entity @s[gamemode=creative]
type /execute store success score #adventure sp9mode if entity @s[gamemode=adventure]
type /gamemode survival
scene
# Spot B, V on the pavement facing south (heading 180), the barrier 3.5 blocks east (left).
# 1. Wander: the pig, the cow and the villager 4 blocks south of V, their positions every 5 s for 60 s. Each should
# stroll at least twice and move 3 blocks or more, and stay on the road: the street at 74.25 and the pavement at 74.5,
# none past the barrier nor below the road.
type /execute rotated ~ 0 positioned ^2 ^ ^4 run summon minecraft:pig ~ ~0.5 ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
type /execute rotated ~ 0 positioned ^ ^ ^4 run summon minecraft:cow ~ ~0.5 ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
type /execute rotated ~ 0 positioned ^-2 ^ ^4 run summon minecraft:villager ~ ~0.5 ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
wait 1
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
type /execute as @e[tag=sp7] run data get entity @s OnGround
type /execute at @e[tag=sp7b] run setblock ~ ~ ~ minecraft:air strict
type /kill @e[tag=sp7]
type /kill @e[tag=sp7b]
wait 3
# 2. Pet: the wolf 2 ahead and 2 to the left of V on the street, its owner the player. V walks about 15 blocks south,
# past the 10 a wolf waits for: it should follow by path to within 4 blocks. Then V jumps 22 blocks north, past the 12
# a pet teleports at: it should land beside V at once, on the road's floor and on the ground.
at -193.5 74.3 -616.5
wait 2
type /execute rotated ~ 0 positioned ^2 ^ ^2 run summon minecraft:wolf ~ ~0.5 ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
type /data modify entity @e[type=minecraft:wolf,tag=sp7,limit=1,sort=nearest] Owner set from entity @s UUID
wait 2
build on
down 26
track 3.5
up 26
wait 4
type /execute if entity @e[type=minecraft:wolf,tag=sp7,distance=..4]
type /execute as @e[tag=sp7] run data get entity @s Pos
build off
at -193.5 74.3 -623.5
wait 0.5
type /execute as @e[tag=sp7] run data get entity @s Pos
type /execute as @e[tag=sp7] run data get entity @s OnGround
type /execute if entity @e[type=minecraft:wolf,tag=sp7,distance=..4.5]
wait 1.5
type /execute as @e[tag=sp7] run data get entity @s Pos
type /execute at @e[tag=sp7b] run setblock ~ ~ ~ minecraft:air strict
type /kill @e[tag=sp7]
type /kill @e[tag=sp7b]
wait 3
# 3. Chase: the husk 7 blocks south of V on the pavement, in view, Resistance first. It should reach the player.
at -189.898 74.432 -616.754
wait 2
type /effect give @s minecraft:resistance 300 4 true
type /execute rotated ~ 0 positioned ^ ^ ^7 run summon minecraft:husk ~ ~0.5 ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
wait 10
type /execute if entity @e[type=minecraft:husk,tag=sp7,distance=..2]
type /execute as @e[tag=sp7] run data get entity @s Pos
type /execute at @e[tag=sp7b] run setblock ~ ~ ~ minecraft:air strict
type /kill @e[tag=sp7]
type /kill @e[tag=sp7b]
wait 3
# 4. Flyer: the dandelion into slot 9, and the bee 2 ahead and 2 to the right of V, a block up. With the dandelion not
# held, it wanders: in the air above the road, not inside the city's cells.
type /item replace entity @s hotbar.8 with minecraft:dandelion
type /execute rotated ~ 0 positioned ^-2 ^ ^2 run summon minecraft:bee ~ ~1 ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
wait 1
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
type /execute at @e[tag=sp7b] run setblock ~ ~ ~ minecraft:air strict
type /kill @e[tag=sp7]
type /kill @e[tag=sp7b]
wait 3
# V's apartment: spot B's city goes, and the apartment's comes around V's feet when it was dumped. Run F's checks
# there start in V's room 2 blocks in from its front door (a doorway a block wide at x -1389, onto the building's
# corridor), V facing into the room (heading 90). The building's storeys are 4 blocks apart, with stairs between them.
type /item replace entity @s hotbar.7 with minecraft:wheat
ground floor
at -1383.343 123.065 -1271.700
dump E:\Cyberpunk2077\red4ext\plugins\PhysXProbe\dumps\2026-10-09_12-22-29\collision.obj
ground city
wait-ground 30
at -1385.5 123.06 -1271.5
look 270 0
wait-ground 30
wait 2
# 5. Wander in V's room: the pig, the cow and the villager 4 blocks east of V, their positions every 5 s for 60 s.
# Each should stroll at least twice, among the furniture and not through it, on the room's floor at 123.125 (up to
# about 124 on raised parts); any of them may go out of the front door into the corridor, and down or up the stairs:
# floors 4 blocks apart, at 111.125 to 127.125.
type /execute rotated ~ 0 positioned ^2 ^ ^4 run summon minecraft:pig ~ ~0.5 ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
type /execute rotated ~ 0 positioned ^ ^ ^4 run summon minecraft:cow ~ ~0.5 ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
type /execute rotated ~ 0 positioned ^-2 ^ ^4 run summon minecraft:villager ~ ~0.5 ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
wait 1
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
wait 5
type /execute as @e[tag=sp7] run data get entity @s Pos
type /execute as @e[tag=sp7] run data get entity @s OnGround
type /execute at @e[tag=sp7b] run setblock ~ ~ ~ minecraft:air strict
type /kill @e[tag=sp7]
type /kill @e[tag=sp7b]
wait 3
# 6. Pet: the wolf beside V in the room, then V jumps 19 blocks out along the building's corridor, and then 18 blocks
# across and 4 down, to the storey below: each time the wolf should land beside V at once, on V's storey's floor
# (123.125, then 119.125; or on the far side of a wall, within 3 blocks across, as a vanilla pet may).
type /execute rotated ~ 0 positioned ^2 ^ ^2 run summon minecraft:wolf ~ ~0.5 ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
type /data modify entity @e[type=minecraft:wolf,tag=sp7,limit=1,sort=nearest] Owner set from entity @s UUID
wait 3
at -1400.5 123.06 -1259.5
wait 0.5
type /execute as @e[tag=sp7] run data get entity @s Pos
type /execute as @e[tag=sp7] run data get entity @s OnGround
type /execute if entity @e[type=minecraft:wolf,tag=sp7,distance=..4.5]
wait 1.5
type /execute as @e[tag=sp7] run data get entity @s Pos
at -1385.5 119.06 -1289.5
wait 0.5
type /execute as @e[tag=sp7] run data get entity @s Pos
type /execute as @e[tag=sp7] run data get entity @s OnGround
type /execute if entity @e[type=minecraft:wolf,tag=sp7,distance=..4.5]
wait 1.5
type /execute as @e[tag=sp7] run data get entity @s Pos
type /execute at @e[tag=sp7b] run setblock ~ ~ ~ minecraft:air strict
type /kill @e[tag=sp7]
type /kill @e[tag=sp7b]
wait 3
# 7. Sight: V in the room 3 blocks in from the front door, facing heading 300, the husk 7 ahead in the corridor, the
# room's wall between them, and its way around through the door 9 steps long. For 4 s it shouldn't come (unless a
# stroll brings V into its view); once V steps into the doorway, in its view, it should reach the player.
at -1384.5 123.06 -1271.5
look 120 0
wait 2
type /effect give @s minecraft:resistance 300 4 true
type /execute rotated ~ 0 positioned ^ ^ ^7 run summon minecraft:husk ~ ~0.5 ~ {Tags:["sp7"],PersistenceRequired:1b,DeathLootTable:"minecraft:empty"}
wait 4
type /execute if entity @e[type=minecraft:husk,tag=sp7,distance=..3]
type /execute as @e[tag=sp7] run data get entity @s Pos
at -1388.5 123.3 -1271.5
wait 8
type /execute if entity @e[type=minecraft:husk,tag=sp7,distance=..2]
type /execute as @e[tag=sp7] run data get entity @s Pos
# The clear: everything tagged, the Resistance, slots 8 and 9 as "Your slots" fills them, the game mode as it was.
# A killed mob's puffs last up to 5 s: the last scene, 7 s on, should say nothing is left.
type /execute at @e[tag=sp7b] run setblock ~ ~ ~ minecraft:air strict
type /kill @e[tag=sp7]
type /kill @e[tag=sp7b]
type /effect clear @s minecraft:resistance
type /item replace entity @s hotbar.7 with minecraft:tnt 64
type /item replace entity @s hotbar.8 with minecraft:flint_and_steel
type /execute if score #creative sp9mode matches 1 run gamemode creative
type /execute if score #adventure sp9mode matches 1 run gamemode adventure
type /scoreboard objectives remove sp9mode
wait 7
scene
status
quit
```

- [x] **Step 2: Check that mclink parses the script**

A copy with a line `mclink` doesn't know at its end: `mclink` reads the whole script before it opens the link, so it
stops at that line, having parsed every other, and nothing runs, even with Minecraft open.
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build \
  && { cat tools/mclink/pathsrow.txt; echo "parse-check-stop here"; } > build/pathsrow-parse.txt \
  && ./build/RelWithDebInfo/mclink.exe build/pathsrow-parse.txt; echo "exit: $?"
```
Expected: `line 224: unknown action: "parse-check-stop here"` and `exit: 2`.

- [x] **Step 3: Check that the script types the overlay's commands**

The overlay run offline, clicking every button the script stands in for, and each request it writes looked for in the
script as a whole `type` line.
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/fun && cd build/fun \
  && lua /d/Cyberpunk-Minecraft/tools/plans/fun_check.lua ../../tools/blockrender_hud/init.lua click "Pig##paths" Cow \
       "Villager##paths" Wolf Bee Husk "Wheat (8)" "Dandelion (9)" Clear Survival "TNT (8)" "Flint and steel (9)" \
  | tr -d '\r' | sed -n 's/^request: mc //p' > commands.txt && tr -d '\r' < ../../tools/mclink/pathsrow.txt > script.txt \
  && typed=0 && while IFS= read -r command; do \
       if grep -q -x -F "type $command" script.txt; then typed=$((typed + 1)); else echo "not typed: $command"; fi; \
     done < commands.txt; echo "the script types $typed of the overlay's $(wc -l < commands.txt) commands"
```
Expected: `the script types 16 of the overlay's 16 commands`.

- [x] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render/tools/mclink/pathsrow.txt \
  && git commit -q -m "minecraft-paths: an mclink script of the Paths rows, on run F's ground" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 18: In Minecraft: the Paths rows at spot B and in V's apartment

Task 17's script, with the user, against SkyCraft's mod and CyberCraft Tweaks 1.7.1 in the `CyberCraft` instance,
without the game.
- **What Minecraft logs:** as in Part 1: `/data get`'s `… has the following entity data: …`, `/execute if`'s `Test
  passed. Count: 1` or `Test failed`, `Replaced 1 slot(s) on ItzBloxy with [Wheat]` for `item replace`, `Summoned new
  …`, the minute lines (`CyberCraft Tweaks: paths`), and any command's error. Clear's first command finds no marker
  each time and answers `No entity was found`; that's expected.
- **If a check misbehaves,** let the script finish (it clears what it makes as it goes), then find the cause with
  superpowers:systematic-debugging before going on. A command that needs changing changes in the overlay and the
  script together, as a new task.

**Files:**
- Modify: `docs/spikes/2026-10-08-minecraft-paths-results.md`, and (steps 5 to 10, added after the run)
  `spikes/block-render/tools/blockrender_hud/init.lua` and `spikes/block-render/tools/mclink/pathsrow.txt`

- [x] **Step 1: Check that the game isn't running**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game: it would hold the mapping.

- [x] **Step 2: Ask the user for their leave, and to start Minecraft**

Ask for their leave for what the script does ("Before you start (Part 2)"), naming slots 8 and 9: their TNT and flint
and steel are replaced, and come back as 64 TNT and a flint and steel. Then the user starts the `CyberCraft` instance
from the Modrinth App and says when its title screen shows. Then:
```bash
L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"; \
  grep -E "cybercraft_tweaks [0-9.]+" "$L" | head -1; \
  echo "mixin errors: $(grep -c -E "Mixin.*(rror|ailed)|InvalidInjection|MixinApplyError|cybercraft_tweaks.*(rror|xception)" "$L")"
```
Expected: `- cybercraft_tweaks 1.7.1` and `mixin errors: 0`.

- [x] **Step 3: In Minecraft: run the Paths rows' script**

```bash
L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"; before=$(wc -l < "$L"); \
  cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/pathsrow \
  && ./build/RelWithDebInfo/mclink.exe tools/mclink/pathsrow.txt > build/pathsrow/pathsrow.log 2>&1; echo "exit: $?"; \
  tail -n +$((before + 1)) "$L" | grep -E "\[CHAT\]|SkyCraft: |CyberCraft Tweaks: |WARN|rror|xception" > build/pathsrow/minecraft.log
```
It takes about 5 minutes once Minecraft is in its world, and up to 3 more if it opens the world first: give the command
9 minutes. Expected, from the simulation of the two dumps where the game puts them:
- **Linking:** `Minecraft is there (process …)`, `the city around region … is all in the ring` for spot B, and again
  after the switch to the apartment's dump; no error or exception from a mixin.
- **Spot B, wander:** 13 positions each for the pig, the cow and the villager, summoned at about −187.9, −189.9 and
  −191.9 by 74.9 by −612.8, and dropping onto the pavement's 74.5 (the villager the street's 74.25): each moves 3 blocks
  or more and strolls at least twice; every y 74.25 to 75.0, the road's (73 or less is the ledge or the slope behind
  the barrier, 75.75 its north end's top); the three `OnGround` `1b`. If one climbs the barrier's wide north end (x −188 to −187, z −638 to −634) and drops off
  its far side, that's the route "What step 0 and planning found" describes: record it.
- **Spot B, pet:** `Test passed. Count: 1` after the walk; after the jump north, `Test passed. Count: 1` with the wolf
  within 3 blocks across of V (−193.5 −623.5), at y 74.25 or 74.5 and `1b`, and still near V at 2 s.
- **Spot B, chase:** `Test passed. Count: 1`.
- **Spot B, flyer:** `Replaced 1 slot(s) on ItzBloxy with [Dandelion]`; the bee's three positions in the air over the
  road, from about 75 up.
- **The apartment, wander:** `Replaced 1 slot(s) on ItzBloxy with [Wheat]`; 13 positions each, summoned at about
  −1381.5 by 123.6 by −1273.5, −1271.5 and −1269.5: each strolls, among the furniture and not through it, on the room's
  floor (122.5 to 124.25), or out of the front door into the corridor (x −1395 to −1390, 123.125), or on another storey
  (111.125, 115.125, 119.125 or 127.125) if it took the stairs; the three `1b`.
- **The apartment, pet:** after each jump `Test passed. Count: 1`, the wolf within 3 blocks across of V and `1b`: at
  123.125 by −1400.5 −1259.5, then at 119.125 by −1385.5 −1289.5, the storey below.
- **The apartment, sight:** after 4 s `Test failed`, the husk still about 7 blocks off in the corridor, near −1390.6
  123.6 −1275.0 or a stroll from there (if it passed, its position says what brought V into its view); after V steps
  into the doorway, `Test passed. Count: 1`.
- **Cost:** minute lines with city answers, shapes read, sight casts with hits (the husk's 4 s), a few milliseconds of
  casting a minute, and `pets landed on the city's floor` counting the three jumps.
- **The end:** `Removed effect Resistance`, `Replaced 1 slot(s) on ItzBloxy with [TNT]` and `… with [Flint and Steel]`,
  the game mode as it was (no reply if it's still survival), the last scene `world: nothing`, no error but Clear's `No
  entity was found`, and `exit: 0`.

- [x] **Step 4: Ask the user to stop Minecraft, after an autosave**

The last change was the script's end. Wait for Minecraft's autosave, run in the background for its `sleep` loop:
```bash
W="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/saves/SkyCraft"; \
  M=/d/Cyberpunk-Minecraft/spikes/block-render/build/pathsrow/saved-after; touch "$M"; \
  until [ "$W/level.dat" -nt "$M" ] && [ -n "$(find "$W/players/data" -name "*.dat" -newer "$M")" ]; do sleep 2; done; \
  date +%H:%M:%S; ls -la --time-style=+%H:%M:%S "$W/level.dat" "$W"/players/data/*.dat
```
Expected within 5 minutes: `level.dat` and the player's `.dat` both written after the mark. Then the user stops
Minecraft from the Modrinth App, and:
```bash
tasklist | grep -i -c "javaw.exe"
```
Expected: `0`.

- [x] **Step 5: Edit `tools/blockrender_hud/init.lua`: the husk's view outside the front door**

Added after the run (2026-10-09). All its checks passed but the sight check's last: once the stand-in V stepped into
the doorway, `SkyCraft: lifted player 2,275 blocks out of the ground`, and the husk didn't come (`Test failed`, 4.7
blocks off). After every teleport SkyCraft lifts Minecraft's player onto the highest surface within 2.5 blocks above its
feet (`SkyClient.liftOutOfGeometry`, `TriCollider.floor` over every triangle, walls and ceilings included), for feet a
sliver inside the ground; the doorway's outer cell has its header 2.26 blocks above the threshold, so the player went
onto the door frame, out of the husk's reach. Recomputed on the dump the same way, the lift is 2.275 there and 0.000 to
0.023 at every other spot the script stood V, and on the walk to the stairs and down them nowhere but that cell. In the
game BlockRender teleports Minecraft's player to V whenever V moves 0.1 m (10 a second at most), so V stopped in the
doorway would do the same. The husk's view moves to the corridor just outside the door (Minecraft block −1390 123
−1272): in view from the husk's eye at all 9 points around it, a lift of 0.017, and the husk's walk 4 steps.

```lua
-- husk can't see V hidden 1 block further in, but can from the doorway; the stairs go down to the storey below.
```
```lua
-- husk can't see V hidden 1 block further in, but can once V is out of the door in the corridor (not in the doorway,
-- whose header SkyCraft lifts Minecraft's player onto after a teleport); the stairs go down to the storey below.
```
```lua
    { name = "In the husk's view", x = -1388.5, y = 1271.5, z = 123.3, face = 300 },
```
```lua
    { name = "In the husk's view", x = -1389.5, y = 1271.5, z = 123.06, face = 345 },
```

- [x] **Step 6: Check the overlay's syntax**

```bash
luac -p /d/Cyberpunk-Minecraft/spikes/block-render/tools/blockrender_hud/init.lua && echo "syntax ok"
```
Expected: `syntax ok`.

- [x] **Step 7: Run the overlay offline: run F's spots**

As Task 16's step 4.
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/fun && cd build/fun \
  && FUN_V="-1383.343 1271.700 123.065 -0.944 -0.330" lua /d/Cyberpunk-Minecraft/tools/plans/fun_check.lua \
       ../../tools/blockrender_hud/init.lua show | tr -d '\r' | grep -E "^text: (V:|Run F|    )"
```
Expected, the husk's view now 6.2 m off:
- `Spot B: 1361.3 m, heading 119, 48.6 m down; face 180 there`
- `V's room: 2.2 m, heading 265, 0.0 m down; face 90 there`
- `Hidden from the husk: 1.2 m, heading 260, 0.0 m down; face 300 there`
- `In the husk's view: 6.2 m, heading 268, 0.0 m down; face 345 there`
- `The stairs: 22.5 m, heading 302, 0.0 m down; face 0 there`

- [x] **Step 8: Edit `tools/mclink/pathsrow.txt`: the husk's view outside the front door**

```text
# stroll brings V into its view); once V steps into the doorway, in its view, it should reach the player.
```
```text
# stroll brings V into its view); once V steps out of the door into the corridor, in its view, it should reach the
# player. Not in the doorway: SkyCraft lifts Minecraft's player onto its header, 2.26 blocks up, after a teleport.
```
```text
at -1388.5 123.3 -1271.5
```
```text
at -1389.5 123.06 -1271.5
```

- [x] **Step 9: Check that mclink parses the script**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build \
  && { cat tools/mclink/pathsrow.txt; echo "parse-check-stop here"; } > build/pathsrow-parse.txt \
  && ./build/RelWithDebInfo/mclink.exe build/pathsrow-parse.txt; echo "exit: $?"
```
Expected: `line 225: unknown action: "parse-check-stop here"` and `exit: 2`.

- [x] **Step 10: Check that the script types the overlay's commands**

As Task 17's step 3.
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/fun && cd build/fun \
  && lua /d/Cyberpunk-Minecraft/tools/plans/fun_check.lua ../../tools/blockrender_hud/init.lua click "Pig##paths" Cow \
       "Villager##paths" Wolf Bee Husk "Wheat (8)" "Dandelion (9)" Clear Survival "TNT (8)" "Flint and steel (9)" \
  | tr -d '\r' | sed -n 's/^request: mc //p' > commands.txt && tr -d '\r' < ../../tools/mclink/pathsrow.txt > script.txt \
  && typed=0 && while IFS= read -r command; do \
       if grep -q -x -F "type $command" script.txt; then typed=$((typed + 1)); else echo "not typed: $command"; fi; \
     done < commands.txt; echo "the script types $typed of the overlay's $(wc -l < commands.txt) commands"
```
Expected: `the script types 16 of the overlay's 16 commands`. The rehearsal isn't run again for this: run F's apartment
half steps into the corridor in the game, and spot B's chase in view passed here.

- [x] **Step 11: Record the run in `docs/spikes/2026-10-08-minecraft-paths-results.md`**

A "The Paths rows with `mclink`" section after step 0's: the versions, the two dumps and where they went, and each
check with its numbers (every mob's positions, the tests' replies, the minute lines), against the simulation's, and
anything that surprised: the doorway's lift among them, and its fix.

- [x] **Step 12: Commit**

With the plan, its steps ticked up to this one.
```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render/tools/blockrender_hud/init.lua spikes/block-render/tools/mclink/pathsrow.txt \
  docs/spikes/2026-10-08-minecraft-paths-results.md docs/superpowers/plans/2026-10-08-minecraft-paths.md \
  && git commit -q -m "minecraft-paths: the Paths rows with mclink, before run F, and the husk's view outside the door" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 19: In the game: run F at spot B

Run F of the spec, with the user, its first half: spot B, on BlockRender 0.23.0 (unchanged), linked to SkyCraft's
Minecraft in the `CyberCraft` instance with CyberCraft Tweaks 1.7.1. Combat off (the overlay's Combat button: Minecraft's
hits stay in Minecraft), Minecraft's player in survival. DRED stays on.

**Before the user looks,** name what can't be seen, so it isn't taken for a fault:
- the cells the mobs walk on are the city's collision voxels, up to 1/8 block above the surface drawn, so a mob's feet
  may hover a little or sink a little;
- the city's collision has no cars, NPCs or kinematic doors: mobs walk through them, as they always did;
- a wall top 2 blocks wide is walkable, as on a Minecraft wall: the barrier's north end is one (Minecraft block −188 75
  −636), with drops of 3 blocks or less down the rock face behind it;
- under anything within 2.5 blocks above V's feet, as V's front door's header is, SkyCraft lifts Minecraft's player on
  top of it after a teleport (Task 18's step 5): walking V through the doorway lifts it for a moment; don't stop V
  there;
- the minute line and every reply are in Minecraft's log, which the agent reads.

**The order matters:** Clear before each check, as Task 18 did, and before V leaves a spot, since mobs left where the
city's ground goes fall. If the game freezes, read the logs and the newest folder in
`%LOCALAPPDATA%\REDEngine\ReportQueue`, and force-close it only when the user asks. If a check fails, stop and find the
cause with superpowers:systematic-debugging before going on. Fixes become new tasks.

These commands come back, here and in Task 20:
- **Send BlockRender a request,** atomically, from the plugin's own folder: it runs one within a second. Replace the
  command; send the next only once the file has gone.
  ```bash
  P=/e/Cyberpunk2077/red4ext/plugins/BlockRender; printf '%s\n' "mc /execute as @e[tag=sp7] run data get entity @s Pos" > "$P/request.tmp" && mv "$P/request.tmp" "$P/blockrender.request"
  ```
- **Positions every 5 s for 60 s,** run in the background for its `sleep` loop:
  ```bash
  P=/e/Cyberpunk2077/red4ext/plugins/BlockRender; for i in $(seq 1 13); do \
    while [ -e "$P/blockrender.request" ]; do sleep 0.2; done; \
    printf '%s\n' "mc /execute as @e[tag=sp7] run data get entity @s Pos" > "$P/request.tmp" && mv "$P/request.tmp" "$P/blockrender.request"; \
    sleep 5; done; echo polled
  ```
- **The wolf's and V's positions in turn,** one a second for 30 s, in the background, to catch a landing:
  ```bash
  P=/e/Cyberpunk2077/red4ext/plugins/BlockRender; for i in $(seq 1 15); do \n    for c in "mc /execute as @e[tag=sp7] run data get entity @s Pos" "mc /data get entity @p Pos"; do \n      while [ -e "$P/blockrender.request" ]; do sleep 0.2; done; \n      printf '%s
' "$c" > "$P/request.tmp" && mv "$P/request.tmp" "$P/blockrender.request"; sleep 1; done; done; echo polled
  ```
- **Minecraft's chat and minute lines since a mark** (note `wc -l < "$L"` as the mark first):
  ```bash
  L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"; tail -n +$((mark + 1)) "$L" | grep -E "\[CHAT\]|CyberCraft Tweaks: |WARN"
  ```
- **CET's frame time,** five readings 2 s apart, in the background:
  ```bash
  H=/e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud; for i in 1 2 3 4 5; do cat "$H/frames.txt"; sleep 2; done
  ```
- **The status, and BlockRender's log:**
  ```bash
  cat /e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud/status.txt; \
    L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); tail -20 "/e/Cyberpunk2077/red4ext/logs/$L"
  ```

**Files:**
- Modify: `docs/spikes/2026-10-08-minecraft-paths-results.md`

- [x] **Step 1: Deploy the overlay, with the user's leave (the game must not be running)**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game first. Then:
```bash
H=/e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud; \
  cp /d/Cyberpunk-Minecraft/spikes/block-render/tools/blockrender_hud/init.lua "$H/init.lua" \
  && cmp /d/Cyberpunk-Minecraft/spikes/block-render/tools/blockrender_hud/init.lua "$H/init.lua" && echo installed
```
Expected: `installed`.

- [x] **Step 2: In the game: start, load and link**

Ask the user to start the `CyberCraft` instance, then the game, to load their save (V in her apartment), and to click
Link. Then:
```bash
L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); grep -E "loaded|link on|ERROR|error" "/e/Cyberpunk2077/red4ext/logs/$L" | head -5; \
  grep -E "^(combat|entities)=" /e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud/status.txt | cut -c1-120; \
  grep -E "cybercraft_tweaks [0-9.]+" "/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log" | head -1
```
Expected: `BlockRender 0.23.0 loaded (game 2.3.1)`, `link on: Minecraft's player follows V; drawing is gbuffer`, no error
lines, `combat=off: Minecraft's hits are logged, not applied`, `entities=on`, and `- cybercraft_tweaks 1.7.1`. Ask the
user to open the overlay's Fun header: under the rows, "Run F's spots" with five lines, "V's room" a few metres away.
Then name what can't be seen ("Before the user looks", above).

- [x] **Step 3: In the game: go to spot B**

Ask the user to take V to spot B by the overlay's "Spot B" line (about 1.4 km at heading 119 from the apartment, 49 m
down): Downtown's raised sidewalk, a concrete barrier about 3.5 m east of V, a drop of 6 m beyond it, the road to the
west. There, V stands at Minecraft block −190 74 −617 (the overlay's "Minecraft block" line; "Spot B" under 1 m),
facing heading 180, south along the sidewalk; a block about 2 m square stands on the sidewalk 11 m ahead, at Minecraft
−190 −606. Then read the status's `ground=` line until it says the city around V is known (`… regions known, 0
batches to read`), and take the frame time with nothing summoned, as the baseline.

- [x] **Step 4: In the game: spot B's wanderers (check 1)**

Ask the user to click Clear, then the Paths row's Pig, Cow and Villager, and to watch for a minute, V standing still.
Note the log's mark, then run the 60 s positions in the background, and the frame time after it. Expected, as in Task
18: each strolls at least twice and moves 3 blocks or more, every y 74.25 to 75.0, the road's; to the user,
they wander the sidewalk and the street, step down and up the kerb, and stay out of the barrier and the block ahead.
If one goes to the barrier's north end, 20 blocks north, and over it, record it: the route the simulation found. Then
send `mc /execute as @e[tag=sp7] run data get entity @s OnGround`: three `1b`.

- [x] **Step 5: In the game: spot B's cow and the wheat (check 2)**

Ask the user to click Clear, then Cow and Wheat (8); then F8 (build mode), mouse button 5 (slot 8, the wheat in
Minecraft's hand), F8 again; then to walk V about 10 m south, passing the block ahead on its street side, and stop.
Then send `mc /execute if entity @e[type=minecraft:cow,tag=sp7,distance=..4]` and the positions. Expected: `Test
passed. Count: 1`, the cow beside V, having followed down the kerb if V took the street.

- [x] **Step 6: In the game: spot B's wolf (check 3)**

Ask the user to click Clear, then Wolf (two requests: the wolf, then its owner). Start the wolf's and V's positions in
turn in the background, then ask the user to walk V about 15 m south on the street side, past the 10 m a wolf waits for,
and stop; then to sprint about 25 m north along the street, past the 12 m a pet teleports at, stop, and read the
overlay's Entities line at once: "the nearest mob … m away". Expected: the wolf follows the walk to within 4 blocks
(`mc /execute if entity @e[type=minecraft:wolf,tag=sp7,distance=..4]` after it); during the sprint, within a second of
V passing 12 m, a jump in its positions to within 3 blocks across of V, at y 74.25 or 74.5, never below the road; the
nearest mob under about 4.5 m when V stops; and the next minute line counting `1 pets landed` or more. A wolf that
strolls off a few seconds after landing is vanilla's ("How Part 1 went").

- [x] **Step 7: In the game: spot B's bee and the dandelion (check 4)**

Ask the user to click Clear, then Dandelion (9); F8, mouse button 4 (slot 9), F8; then Bee; then to walk V about 10 m
south along the sidewalk and then about 15 m west across the street, and stop. Then send `mc /execute if entity
@e[type=minecraft:bee,tag=sp7,distance=..4]` and the positions. Expected: `Test passed. Count: 1`; to the user, the bee
follows around the block and over the kerb without sticking against the barrier or the block.

- [x] **Step 8: In the game: spot B's husk (check 5, in view)**

Ask the user to click Clear, then Survival (the Mode row) and Husk (two requests: Resistance, then the husk 7 m ahead),
and to stand still, facing heading 180 again. After 10 s send `mc /execute if entity
@e[type=minecraft:husk,tag=sp7,distance=..2]`. Expected: `Test passed. Count: 1`: the husk walked to V and swings at
her; nothing hurts V (combat off) or Minecraft's player (Resistance). The hidden half of check 5 is in V's apartment
(Task 20).

- [x] **Step 9: In the game: spot B's cost (check 6)**

Ask the user to click Clear, then Pig, Cow, Villager, Wolf and Bee at once, and to stand for 30 s; take the frame time
meanwhile, and read the minute lines since the step 4 mark. Expected: the frame time within the baseline's spread; the
minute lines' casting a few milliseconds a minute, as in step 0. Then ask the user to click Clear.

- [x] **Step 10: Record spot B's run in `docs/spikes/2026-10-08-minecraft-paths-results.md`**

A "Run F" section: the versions; at spot B each check with its outcome and numbers (positions, tests, the wolf's
landing and distance, frame times, minute lines), the user's observations, and screenshots if the user takes some, as
earlier runs' results have.

- [x] **Step 11: Commit**

With the plan, its steps ticked up to this one. The user can drive V back to the apartment meanwhile.
```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-08-minecraft-paths-results.md docs/superpowers/plans/2026-10-08-minecraft-paths.md \
  && git commit -q -m "minecraft-paths: run F at spot B" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 20: In the game: run F in V's apartment

Run F's second half, in the same game session, in V's apartment in Megabuilding H10 and its building: the doorway, the
corridor and the stairs. The commands Task 19 names come back. Expected results come from the simulation of the
apartment's dump (`2026-10-09_12-22-29`, "What step 0 and planning found"); the city around V in the game reaches
further than the dump (88 m square against 64), so mobs may go beyond what it shows.

**Files:**
- Modify: `docs/spikes/2026-10-08-minecraft-paths-results.md`

- [ ] **Step 1: In the game: go to V's room**

With everything cleared at spot B, ask the user to take V back by the overlay's "V's room" line, and stand at
Minecraft block −1386 123 −1272, 2 blocks in from the front door, facing heading 90, into the room. Read the status's
`ground=` line until the city around V is known.

- [ ] **Step 2: In the game: the room's wanderers (check 1)**

Ask the user to click Pig, Cow and Villager, and to watch for a minute; run the 60 s positions in the background, then
the `OnGround` request. Expected: each strolls at least twice, among the furniture and not through it, at y 122.5 to
124.25 in the room; any may go out of the door into the corridor (123.125) and down or up the stairs (119.125,
127.125); none inside a wall or the furniture, none falling between storeys; three `1b`. Then Clear.

- [ ] **Step 3: In the game: the cow through the doorway and down the stairs (check 2)**

Ask the user to click Cow and Wheat (8), and select slot 8 (F8, mouse button 5, F8); then to walk V out of the front
door, along the corridor to the stairs (the overlay's "The stairs" line, Minecraft block −1403 123 −1284), down them to
the storey below, and stop. Expected: the cow follows through the doorway (a block wide, over its raised threshold),
along the corridor and down the stairs: `mc /execute if entity @e[type=minecraft:cow,tag=sp7,distance=..4]` passes, its
position at about 119.125. Then Clear.

- [ ] **Step 4: In the game: the wolf through the doorway, down the stairs, and its landing (check 3)**

Ask the user to go back to V's room (the "V's room" line), click Wolf, and walk V out of the door, along the corridor
and down the stairs to the storey below, then sprint along that storey's corridor about 25 m and stop, reading the
Entities line's nearest mob at once. Start the wolf's and V's positions in turn in the background before the sprint.
Expected: the wolf follows by path through the door and down the stairs; during the sprint it lands within 3 blocks
across of V on V's storey (119.125), not on a storey above or below, never falling; the nearest mob under about 4.5 m
when V stops; a minute line counting the landing. Then Clear.

- [ ] **Step 5: In the game: the bee through the doorway (check 4)**

Ask the user to go back to V's room, click Dandelion (9), select slot 9 (F8, mouse button 4, F8), click Bee, and walk
V out of the door, along the corridor and down the stairs. Expected: the bee follows through the doorway and down the
stairwell without sticking against a wall, the door's frame or the stairs: `mc /execute if entity
@e[type=minecraft:bee,tag=sp7,distance=..4]` passes when V stops. Then Clear.

- [ ] **Step 6: In the game: the husk behind the wall (check 5, hidden, then in view)**

Ask the user to go back to the room, click Survival, and stand at "Hidden from the husk" (Minecraft block −1385 123
−1272, 3 blocks in from the door), facing heading 300, toward the wall beside the door; then to click Husk, which puts
the husk 7 m ahead, in the corridor behind the wall, and to stay there. Note the log's mark. After about 5 s send
`mc /execute if entity @e[type=minecraft:husk,tag=sp7,distance=..3]` and the positions; then ask the user to walk V out
of the door into the corridor ("In the husk's view", Minecraft block −1390 123 −1272, just outside it, not in the
doorway) and stand, and after 10 s send the same test with `distance=..2`. Expected: first `Test failed`, the husk still in the corridor about 7 blocks off (unless a stroll took
it into the doorway's view, which its position shows), and the minute line's sight casts with hits while V was
hidden; then `Test passed. Count: 1`: once V was in view, it came along the corridor. Then Clear.

- [ ] **Step 7: In the game: the apartment's cost (check 6)**

As at spot B: Pig, Cow, Villager, Wolf and Bee at once in V's room for 30 s, the frame time meanwhile, and the minute
lines. Expected: the frame time within the baseline's spread, a few milliseconds of casting a minute. Then Clear.

- [ ] **Step 8: In the game: the end**

Ask the user to click TNT (8) and Flint and steel (9) ("Your slots") to have them back, then to quit the game. Wait for
Minecraft's autosave as Task 18's step 4 does (mark `build/pathsrow/saved-after-run-f`), then ask the user to stop
Minecraft. Then read BlockRender's log for errors and Minecraft's for every minute line of the session:
```bash
L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); grep -c -E "ERROR|error" "/e/Cyberpunk2077/red4ext/logs/$L"; \
  grep -E "CyberCraft Tweaks: paths" "/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"
```
Expected: `0` errors, and the minute lines.

- [ ] **Step 9: Record the apartment's run in `docs/spikes/2026-10-08-minecraft-paths-results.md`**

In "Run F", after spot B: each check with its outcome and numbers, the user's observations, the screenshots, and what
the runs mean for the spike's answer.

- [ ] **Step 10: Commit**

With the plan, its steps ticked up to this one.
```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-08-minecraft-paths-results.md docs/superpowers/plans/2026-10-08-minecraft-paths.md \
  && git commit -q -m "minecraft-paths: run F in V's apartment" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 21: Close the spike

- [ ] **Step 1: Record the spike's answer at the top of the results**

In place of "The spike's answer goes here once run F is in": the spec's question, answered from step 0 and run F, with
what's left for later.

- [ ] **Step 2: Record "How Part 2 went" at the end of this plan**

After "Found while planning Part 2": what ran as written, what changed and why, and what Task 18 and run F found.

- [ ] **Step 3: Record the spec's "Changes while executing Part 2"**

Anything Task 18 or run F changed in the design, or left for later.

- [ ] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs \
  && git commit -q -m "minecraft-paths: the spike's answer, and how Part 2 went" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

- [ ] **Step 5: Ask the user how to finish the branch**

With superpowers:finishing-a-development-branch. The user has chosen a local fast-forward merge of the spike's branch
into `main` every time so far; ask again. The time-of-day sync the user asked for during spike 7 comes next.

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

## How Part 1 went

Part 1 ran from 14:58 on 2026-10-08, when it was planned, to 23:23 the same day. The results:
[2026-10-08-minecraft-paths-results.md](../../spikes/2026-10-08-minecraft-paths-results.md), step 0's control, its
first run with 1.7.0, and its run with 1.7.1.
- **As written:** `run_plan_s9.py` executed Tasks 1 to 7 (`c5c6412` to `0097e05`) and the fixes' Tasks 10 to 13
  (`7a69f51` to `625546e`), checking every expected result, and each commit holds exactly the code tested in the
  scratch clones (`verify/`, tags `s9-t1` to `s9-t7`; `verify2/`, `s9-t10` to `s9-t13`). Every build had no warning;
  the unit checks went from 43 to 56, the mixin checks stayed 12.
- **What changed:**
  - **Task 7's skeleton summon was 298 characters,** past the 256 of Minecraft's chat line, which `mclink`'s typer
    refuses only as it types, so the parse check passed it. Fixed outside the tasks, `5ff4610`: Task 7 shows the script
    as it first ran.
  - **Task 8 found a shield in the user's off hand,** so the guarded wheat never went in. With the user's leave, a
    guarded `mclink` script moved it to inventory slot 15 (Minecraft's window stays hidden once linked), and the
    control ran again (15:16); Task 14 put it back.
  - **Task 9 loaded a stale autosave:** stopping Minecraft from the Modrinth App kills it unsaved, so the control's
    test wolf, the player's creative mode and its wheat came back. Step 0 with 1.7.0 passed tempt, the pet's follow,
    chase, sight and the flyer, but mobs walked onto the road's thin barrier and off its far side. Four diagnostic
    scripts followed, with the user's leave; each placed its dump around a moved V, so their cities were shifted.
  - **Tasks 10 to 14 were added:** thin cells as fences (the user's choice), a pet's landing resetting its fall
    distance, summons dropped onto their floors, the teleports script, CyberCraft Tweaks 1.7.1 (Tasks 10 to 12), and
    step 0 again (Task 14). Task 14's first step found that singleplayer has no `/save-all` and that the pause menu
    doesn't save a linked game: Task 13 took the saving out of the scripts, and Minecraft's own autosave, every 5
    minutes, was waited for before each stop. The close became Task 15.
- **Step 0 with 1.7.1 (Task 14) passed:** wander (four mobs, none on the barrier or below the road), tempt, the pet
  twice, chase, sight (69 sight casts hitting the city in the skeleton's minute) and the flyer, at 2.1 to 2.8 ms of
  casting a minute. The teleports script's own test passed 3 times of 5: after two landings beside V the wolf strolled
  off, as a pet within 10 blocks of its owner does, before the test 4 s later. With a fifth diagnostic's 20 jumps,
  26 landings of 26 were on the road, on the ground.
- **Not as expected:**
  - **the barrier runs on a diagonal** (x 1284 at z 2006 to 1286 at z 2015), not along x 1284;
  - **the dump has hollows** under the pavement (floors at 3.25 to 4.5) and under the ledge (1.375), which the
    skeleton reached from the slope; no path reaches the pavement's from the road;
  - **the 1.7.0 run's wolf ended in the pavement's hollow** after a landing, and how stays unexplained; 1.7.1 never
    repeated it.
- **Tooling:** `run_plan_s9.py` now reads a commit's `git add` continued over lines; spike 9's tools and the city
  simulation are in `tools/plans/` (its README's "Spike 9's versions").
- **For Part 2:**
  - CyberCraft Tweaks 1.7.1 is installed in the CyberCraft instance; the overlay's Paths row and run F remain;
  - a pet's check should read its landing right after V moves, not seconds later, when a stroll may have taken it;
  - watch pets' landings in the game, where the city has more levels and hollows than step 0's road;
  - before Minecraft is stopped, wait for an autosave after the last change.

## Found while planning Part 2

The spec's "Changes while planning Part 2" lists what planning settled in the design. In the plan itself:
- **The simulation, where the game puts the ground:** `voxdump` placed each dump around V's own feet in Minecraft's
  axes, (x, z, −y) of the world, so its cells are the game's. `tools/plans/paths-sim/` gained `simlib.py` (the walk
  rules with 1.7.1's cells, as `fences.py` had them) and the scripts that answered run F's questions:
  - maps of a level: `areamap.py`, `ascii.py` and `ascii2.py`, `voxmap.py` and `voxmap2.py`;
  - `components.py`, the areas a level splits into, and `areas.py`, an area's places and its stroll and landing odds;
  - `summons.py`, where the Paths rows' summons land from a spot facing a heading;
  - `raylib.py`, `hidden.py`, `hidden7.py` and `robust.py`: the husk's hiding places, V's eye hidden from the husk's by
    the dump's triangles as `SkyClip.cast` casts, with a way around, graded over the places around both;
  - `jumps.py`, a pet's landing odds at the jump targets.
- **The spots:**
  - spot B, V's own dumped feet, facing heading 180: the summons go south, away from the barrier's north end;
  - V's room, 2 blocks in from the front door, facing heading 90, where every summon lands in the room;
  - hidden from the husk, 3 blocks in, facing heading 300, and the doorway, in its view;
  - the stairs' top, on V's storey.
- **A landing's test is 4.5 blocks:** a pet teleports to a cell up to 3 blocks off on each axis, as far as 4.24 blocks
  away, so a test of 4 would fail about one landing in ten for nothing. The walks' tests keep step 0's 4.
- **The husk goes 7 blocks ahead,** not 8, where the block entities' sign goes.
- **ImGui's IDs:** a Paths row Pig or Villager labelled as the Mobs row's are would be the same button to ImGui;
  `##paths` keeps them apart, and is the label `fun_check.lua` clicks.
- **The slot is picked in build mode:** mouse buttons 5 and 4, the user's slots 8 and 9, go to Minecraft in build
  mode (`BuildMode`: the mouse buttons, side buttons included, the wheel and the keys 1 to 9); out of it, the slot
  stays.
- **The probe's origin:** F10 dumps pass no PhysX origin, and away from the map's origin find nothing; `dump.request`'s
  `origin <x> <y> <z>` passes one: the 1,024 m grid point nearest V on x and y, −1024 1024 0 at world −1383 1272.
- **Read from Minecraft 26.3's jar:** `TemptGoal.shouldFollow` looks at the main hand, then the off hand, and its
  targeting ignores line of sight; the `cow_food` tag is wheat and `bee_food` has the dandelion; a zombie targets players
  it sees (`NearestAttackableTargetGoal(…, Player.class, true)`), forgets one unseen for 60 ticks
  (`TargetGoal.unseenMemoryTicks`), and its attack goal (`ZombieAttackGoal(…, 1.0, false)`) repaths only while it sees
  its target; `NodeEvaluator.prepare` sizes a node floor(width + 1) by floor(height + 1).
- **The plan's tools:** `fun_check.lua` takes `FUN_V` for a V standing in the game; `run_plan_s9.py` knows Part 2's
  steps ("Check the overlay's syntax", "Run the overlay offline", "Check that the script types the overlay's commands";
  "Deploy" and "In the game" by hand).
- **The mutations:** 17, each failing the step that guards it (`tools/plans/mutate_s9_overlay.py`): in Task 16's requests, the
  cow summoned at the feet, the wheat and the dandelion a slot off, any wolf owned, Resistance short of its highest,
  the husk on the sign's place, the bee half a block up, the Pig's label the Mobs row's, and the wolf on the right;
  in its spots, the heading from east, up and down swapped, the distance across the height, V's room at Minecraft's
  z, and the spots' lines not under their heading; in Task 17, an action `mclink` doesn't know, the script's
  dandelion a slot off, and the script without Survival.
