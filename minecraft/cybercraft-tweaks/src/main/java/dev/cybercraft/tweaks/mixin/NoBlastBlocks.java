package dev.cybercraft.tweaks.mixin;

import java.util.List;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Blasts leave the game's ground as it is. SkyCraft digs into Skyrim's ground where an explosion reaches it: its
 * {@code materialize} turns the ground in the blast into stone and ore blocks just before the explosion breaks them,
 * marking it dug, and its {@code finish} lines the hole with more blocks. Night City isn't Skyrim's terrain: the cells
 * SkyCraft takes for underground there can be open air, so the blocks stay floating above the blast. Both are skipped.
 * An explosion still breaks Minecraft's blocks, the game's ground still shields them, as SkyCraft's resistance has it,
 * and the game still hears of the explosion.
 *
 * <p>It targets SkyCraft 0.1.2's {@code dev.skycraft.world.SkyDigBlast} by name: nothing is remapped, as Minecraft 26.3
 * isn't obfuscated, and fabric.mod.json asks for that SkyCraft version.
 */
@Mixin(targets = "dev.skycraft.world.SkyDigBlast", remap = false)
public abstract class NoBlastBlocks {
    @Inject(method = "materialize", at = @At("HEAD"), cancellable = true)
    private void cybercraft_tweaks$keepTheGround(List<?> blownUp, boolean destroys, CallbackInfo info) {
        info.cancel();
    }

    @Inject(method = "finish", at = @At("HEAD"), cancellable = true)
    private void cybercraft_tweaks$noWalls(CallbackInfo info) {
        info.cancel();
    }
}
