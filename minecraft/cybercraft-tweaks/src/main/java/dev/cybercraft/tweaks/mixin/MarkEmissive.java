package dev.cybercraft.tweaks.mixin;

import java.util.Set;

import com.mojang.blaze3d.vertex.PoseStack;

import dev.skycraft.client.mixin.RenderTypeAccessor;

import net.minecraft.client.model.Model;
import net.minecraft.client.renderer.rendertype.RenderType;
import net.minecraft.client.renderer.texture.UvMapping;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Unique;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.ModifyArg;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * What Minecraft draws emissive glows in the game too. Minecraft lights its entities with its light map, but for the
 * layers its emissive render types draw, whose pipelines skip it (the {@code EMISSIVE} define): {@code eyes}, a spider's,
 * an enderman's and a phantom's eyes; {@code entity_translucent_emissive}, the warden's, the creaking's, the copper
 * golem's and the breeze's glowing parts; and {@code energy_swirl}, a charged creeper's and the wither's armour.
 * SkyCraft's {@code AvatarExporter} keeps the light Minecraft passes those layers, the entity's own, so the game lit them
 * as the rest: spike 7's run B saw a spider's eyes at night as dark as its body. Here such a layer is captured with full
 * light, as SkyCraft already captures lit TNT's white flash, and BlockRender's glow rule (block light 15) makes it glow.
 *
 * <p>It targets SkyCraft 0.1.2's {@code dev.skycraft.client.render.AvatarExporter} by name, as {@link KeepDrawOrder}
 * does, and reads a render type's name through SkyCraft's own accessor.
 */
@Mixin(targets = "dev.skycraft.client.render.AvatarExporter", remap = false)
public abstract class MarkEmissive {
    /** Minecraft 26.3's render types whose pipelines draw without the light map. */
    @Unique private static final Set<String> EMISSIVE = Set.of("eyes", "entity_translucent_emissive", "energy_swirl");

    /** Whether the layer being submitted is one of them. */
    @Unique private boolean cybercraft_tweaks$emissive;

    @Inject(method = "submitModel", at = @At("HEAD"))
    private void cybercraft_tweaks$seeRenderType(Model<?> model, Object state, PoseStack poseStack, RenderType renderType, int lightCoords,
        int overlayCoords, int tintedColor, UvMapping uvMapping, int outlineColor, CallbackInfo info) {
        this.cybercraft_tweaks$emissive = EMISSIVE.contains(((RenderTypeAccessor) renderType).skycraft$name());
    }

    // The light the model is drawn with: full block and sky light for an emissive layer, which BlockRender makes glow.
    @ModifyArg(method = "submitModel", at = @At(value = "INVOKE",
        target = "Lnet/minecraft/client/model/Model;renderToBuffer(Lcom/mojang/blaze3d/vertex/PoseStack;Lcom/mojang/blaze3d/vertex/VertexConsumer;III)V"),
        index = 2)
    private int cybercraft_tweaks$fullLight(int lightCoords) {
        return this.cybercraft_tweaks$emissive ? 0xF000F0 : lightCoords;
    }
}
