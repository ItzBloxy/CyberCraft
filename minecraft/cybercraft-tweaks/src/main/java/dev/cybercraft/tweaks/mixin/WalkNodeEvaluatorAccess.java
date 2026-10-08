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
