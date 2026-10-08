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
