package dev.cybercraft.tweaks.mixin;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Proxy;
import java.util.HashMap;
import java.util.Map;
import java.util.TreeMap;

import net.minecraft.client.renderer.OrderedSubmitNodeCollector;

import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Mutable;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.Unique;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.ModifyVariable;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Minecraft's draw order kept in what SkyCraft sends the game. Minecraft draws an entity's layers in order, each over
 * the one before at the same depth: a villager's body, then its biome's clothes, its profession's and its level's badge
 * (orders 0 to 3); a spider's eyes over its body; a banner's patterns over its cloth. SkyCraft's {@code AvatarExporter}
 * takes every layer as Minecraft submits it, but its {@code order(n)} hands back the collector itself, and it keeps its
 * batches in a HashMap by texture, UV mode and flags, which sends them by the textures' ids: spike 7's step 0 had a
 * villager's badge before its body. Here {@code order(n)} hands back a stand-in that submits through the collector with
 * n as the order, each batch is kept by its order too, and the batches are kept sorted, lowest order first, so the game
 * draws each layer after those under it.
 *
 * <p>It targets SkyCraft 0.1.2's {@code dev.skycraft.client.render.AvatarExporter} by name, as {@link NoBlastBlocks}
 * targets {@code SkyDigBlast}.
 */
@Mixin(targets = "dev.skycraft.client.render.AvatarExporter", remap = false)
public abstract class KeepDrawOrder {
    @Shadow @Final @Mutable private Map<Long, ?> batches;
    /** The order of what's being submitted: 0 unless it comes through an order's stand-in. */
    @Unique private int cybercraft_tweaks$order;
    /** The stand-ins, one for each order asked for. */
    @Unique private Map<Integer, OrderedSubmitNodeCollector> cybercraft_tweaks$standIns;

    @Inject(method = "<init>", at = @At("RETURN"))
    private void cybercraft_tweaks$sortBatches(CallbackInfo info) {
        this.batches = new TreeMap<Long, Object>();  // in key order, and the key starts with the order
        this.cybercraft_tweaks$standIns = new HashMap<>();
    }

    @Inject(method = "order", at = @At("HEAD"), cancellable = true)
    private void cybercraft_tweaks$order(int order, CallbackInfoReturnable<OrderedSubmitNodeCollector> info) {
        info.setReturnValue(this.cybercraft_tweaks$standIns.computeIfAbsent(order, this::cybercraft_tweaks$standIn));
    }

    // SkyCraft's key is the texture's id << 16 | the UV mode << 8 | the flags: the order goes above it, signed, so a
    // layer below 0 comes first.
    @ModifyVariable(method = "batch", at = @At("STORE"), ordinal = 0)
    private long cybercraft_tweaks$keyByOrder(long key) {
        return ((long) this.cybercraft_tweaks$order << 40) + key;
    }

    /** Submits through the collector itself, with aOrder as the order while each call runs. */
    @Unique
    private OrderedSubmitNodeCollector cybercraft_tweaks$standIn(int aOrder) {
        Object collector = this;
        return (OrderedSubmitNodeCollector) Proxy.newProxyInstance(OrderedSubmitNodeCollector.class.getClassLoader(),
            new Class<?>[] { OrderedSubmitNodeCollector.class }, (proxy, method, args) -> {
                int before = this.cybercraft_tweaks$order;
                this.cybercraft_tweaks$order = aOrder;
                try {
                    return method.invoke(collector, args);
                } catch (InvocationTargetException e) {
                    throw e.getCause();
                } finally {
                    this.cybercraft_tweaks$order = before;
                }
            });
    }
}
