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
