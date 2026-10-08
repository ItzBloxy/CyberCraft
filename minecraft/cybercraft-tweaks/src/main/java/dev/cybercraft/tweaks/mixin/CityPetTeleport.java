package dev.cybercraft.tweaks.mixin;

import dev.cybercraft.tweaks.paths.CityCells;
import dev.cybercraft.tweaks.paths.PathStats;

import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.TamableAnimal;
import net.minecraft.world.level.pathfinder.PathType;
import net.minecraft.world.level.pathfinder.WalkNodeEvaluator;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * A pet teleporting to its owner lands on Night City's floor. Vanilla checks a spot's room with the pet's box moved by
 * whole blocks, so its current fraction of a block carries over, and lands it at the cell's bottom: on the city that's
 * inside a floor's skin, and Minecraft's collision ignores voxels already inside a box, so the pet would fall through
 * the street. Where the city gives the cell a floor ({@link CityCells#floorAt}), the walkable check stays, and the room
 * check and the landing go to that floor's height; anywhere else vanilla's check and landing stand. A pet landed so has
 * its fall distance reset: one teleported while falling (as the world loads, before the city's ground arrives) would
 * otherwise take that whole fall's damage as it lands, as a test wolf did in spike 9's step 0.
 */
@Mixin(value = TamableAnimal.class, remap = false)
public abstract class CityPetTeleport {
    @Inject(method = "maybeTeleportTo", at = @At("HEAD"), cancellable = true)
    private void cybercraft_tweaks$ontoCityFloors(int x, int y, int z, CallbackInfoReturnable<Boolean> info) {
        TamableAnimal pet = (TamableAnimal) (Object) this;
        BlockPos pos = new BlockPos(x, y, z);
        double floor = CityCells.floorAt(pet.level(), pos);
        if (Double.isNaN(floor)) {
            return;
        }
        info.setReturnValue(false);
        if (WalkNodeEvaluator.getPathTypeStatic(pet, pos) != PathType.WALKABLE
            || !pet.level().noCollision(pet, pet.getBoundingBox().move(x + 0.5 - pet.getX(), floor - pet.getY(), z + 0.5 - pet.getZ()))) {
            return;
        }
        pet.snapTo(x + 0.5, floor, z + 0.5, pet.getYRot(), pet.getXRot());
        pet.resetFallDistance();
        pet.getNavigation().stop();
        PathStats.petLanded();
        info.setReturnValue(true);
    }
}
