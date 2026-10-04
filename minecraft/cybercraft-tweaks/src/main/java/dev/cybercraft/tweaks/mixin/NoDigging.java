package dev.cybercraft.tweaks.mixin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Nobody digs into the game's ground. Night City's ground can't be dug in the game, so Minecraft's copy of it isn't
 * either, or Minecraft's player and V would disagree about where the ground is. SkyCraft's {@code open} digs a cell of
 * Skyrim's ground that a player mines with a tool, and its {@code reveal} turns the ground behind a dug cell's exposed
 * walls into blocks; both are skipped, as {@link NoBlastBlocks} skips a blast's digging. Cells dug before this mod stay
 * dug, but nothing is revealed around them any more, so blocks broken there stay broken.
 *
 * <p>It targets SkyCraft 0.1.2's {@code dev.skycraft.world.SkyDig} by name, as {@link NoBlastBlocks} does.
 */
@Mixin(targets = "dev.skycraft.world.SkyDig", remap = false)
public abstract class NoDigging {
    @Inject(method = "open", at = @At("HEAD"), cancellable = true)
    private static void cybercraft_tweaks$noDigging(CallbackInfo info) {
        info.cancel();
    }

    @Inject(method = "reveal", at = @At("HEAD"), cancellable = true)
    private static void cybercraft_tweaks$noReveals(CallbackInfo info) {
        info.cancel();
    }
}
