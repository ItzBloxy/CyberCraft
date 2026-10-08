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
