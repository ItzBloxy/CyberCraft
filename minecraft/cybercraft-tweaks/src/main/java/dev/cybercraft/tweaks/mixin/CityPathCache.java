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
