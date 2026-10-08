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
