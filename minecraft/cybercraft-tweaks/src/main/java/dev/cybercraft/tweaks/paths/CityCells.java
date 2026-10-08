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
 * <p>A cell's class depends on its shape and the shape of the cell below it (a thin cell's columns can be covered from
 * below), so what's kept is each shape's 64 column tops, per shape object, weakly: SkyCraft builds new shapes whenever
 * a region is sent again, and the old ones go with their region. {@code VoxelShape} keeps {@code Object.equals}, so
 * the map goes by identity.
 */
public final class CityCells {
    /** What an airborne start's search meets in a city cell: a block, so the start goes above it. */
    public static final BlockState STOP = Blocks.STONE.defaultBlockState();

    private static final Map<VoxelShape, int[]> TOPS = Collections.synchronizedMap(new WeakHashMap<>());

    private CityCells() {
    }

    /** The 64 column tops of SkyCraft's shape at {@code pos}, as {@link CellRule#classify} takes them, or null. */
    private static int[] tops(BlockPos pos) {
        VoxelShape shape = SkyCollision.shapeAt(pos);
        if (shape == null) {
            return null;
        }
        int[] known = TOPS.get(shape);
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
        TOPS.put(shape, tops);
        PathStats.shapeRead();
        return tops;
    }

    /** The city's cell at {@code pos}, packed as {@link CellRule#classify} packs it, or -1 where SkyCraft has none. */
    public static int cell(BlockPos pos) {
        int[] tops = tops(pos);
        return tops == null ? -1 : CellRule.classify(tops, tops(pos.below()));
    }

    /** The city's cell at {@code pos} where Minecraft's own block there has no collision, or -1. */
    private static int open(BlockGetter level, BlockPos pos) {
        int cell = cell(pos);
        return cell >= 0 && level.getBlockState(pos).getCollisionShape(level, pos).isEmpty() ? cell : -1;
    }

    /** The path type for a cell vanilla calls {@code OPEN}: {@code WALKABLE} for a floor, else {@code BLOCKED}, or null. */
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

    /** Whether this cell is a blocked or thin city cell: solid, for random positions. */
    public static boolean blockedAt(BlockGetter level, BlockPos pos) {
        int cell = open(level, pos);
        boolean blocked = cell >= 0 && (CellRule.kind(cell) == CellRule.BLOCKED || CellRule.kind(cell) == CellRule.THIN);
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
