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
