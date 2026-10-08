package dev.cybercraft.tweaks.mixin;

import dev.cybercraft.tweaks.paths.CityRays;

import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.level.ClipContext;
import net.minecraft.world.phys.Vec3;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Night City's walls block sight. {@code hasLineOfSight} clips eye to eye against Minecraft's blocks alone, so a husk
 * noticed V through a building and a skeleton shot at V through it. When vanilla sees clear, the same segment is cast
 * against the city's triangles ({@link CityRays}), as arrows are, and a hit means no sight: targeting, keeping a
 * target, shooting, swelling, looking at a player and brains' visible-entity and item sensors all go through here.
 */
@Mixin(value = LivingEntity.class, remap = false)
public abstract class CitySight {
    @Inject(method = "hasLineOfSight(Lnet/minecraft/world/entity/Entity;Lnet/minecraft/world/level/ClipContext$Block;Lnet/minecraft/world/level/ClipContext$Fluid;D)Z",
        at = @At("RETURN"), cancellable = true)
    private void cybercraft_tweaks$cityWalls(Entity target, ClipContext.Block block, ClipContext.Fluid fluid, double eyeY,
        CallbackInfoReturnable<Boolean> info) {
        if (!info.getReturnValueZ()) {
            return;
        }
        LivingEntity self = (LivingEntity) (Object) this;
        // The same segment vanilla clips: from this entity's eyes to the target at the eye height it was given.
        if (CityRays.blocked(new Vec3(self.getX(), self.getEyeY(), self.getZ()), new Vec3(target.getX(), eyeY, target.getZ()), true)) {
            info.setReturnValue(false);
        }
    }
}
