package dev.cybercraft.tweaks.mixin;

import dev.cybercraft.tweaks.CyberCraftTweaks;
import net.minecraft.server.level.ServerPlayer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * V's health, carried into Minecraft as build mode starts with combat on. BlockRender sends it as SkyCraft's kInHurt with
 * no damage, which SkyCraft's own {@code hurtPlayer} leaves alone, with {@link CyberCraftTweaks#SET_HEALTH} among its
 * flags and V's share of its health, times 10,000, where an attacker's form ID would be. This sets Minecraft's player's
 * health to the same share of its own max health. Nothing else of SkyCraft's changes: a hurt with damage goes on as
 * SkyCraft's, and without this mod the health input does nothing.
 *
 * <p>It targets SkyCraft 0.1.2's {@code dev.skycraft.combat.SkyCombat} by name, as {@link NoDigging} does.
 */
@Mixin(targets = "dev.skycraft.combat.SkyCombat", remap = false)
public abstract class SetHealth {
    @Inject(method = "hurtPlayer", at = @At("HEAD"), cancellable = true)
    private static void cybercraft_tweaks$setHealth(ServerPlayer player, int kind, float skyrimDamage, int share, int flags, CallbackInfo info) {
        if (skyrimDamage != 0.0F || (flags & CyberCraftTweaks.SET_HEALTH) == 0) {
            return;
        }
        info.cancel();
        if (!player.isAlive() || share <= 0) {
            return;
        }
        float before = player.getHealth();
        player.setHealth(Math.min(share, 10000) / 10000.0F * player.getMaxHealth());
        CyberCraftTweaks.LOG.info("CyberCraft Tweaks: Minecraft's player's health set to V's share {}: {} -> {}", share / 10000.0F, before,
            player.getHealth());
    }
}
