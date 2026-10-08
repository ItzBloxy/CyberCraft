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
