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
