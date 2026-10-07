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
 * What Minecraft blends goes to the game blended. SkyCraft's {@code AvatarExporter} sends every layer of an entity model
 * as {@code SOLID} (a cut-out, no mipmaps, lit by the triangle's own normal), so spike 7's run C saw the warden's faint
 * glowing layers and a charged creeper's armour drawn solid, and the game drew a slime's outer layer, allays and vexes
 * as cut-outs. Here a layer of a render type Minecraft 26.3 blends by alpha, {@code entity_translucent},
 * {@code entity_translucent_emissive}, {@code breeze_wind} or {@code wolf_armor_cracks}, goes with SkyCraft's
 * translucent flag and its own normal ({@code 2 | 8 | 7 << 4}): SkyCraft sends its batch translucent, keyed apart from
 * the solid ones. {@code energy_swirl}, which Minecraft adds rather than blends, goes with bit 7 as well, which
 * BlockRender reads as additive. BlockRender draws those that are really see-through before DLSS, and the rest as
 * before. {@code eyes} and {@code banner_pattern} stay solid: every eyes texture is opaque or clear, and banner patterns
 * lie on cloth the game lights.
 *
 * <p>It targets SkyCraft 0.1.2's {@code dev.skycraft.client.render.AvatarExporter} by name, as {@link MarkEmissive}
 * does: {@code submitModel} alone picks its batch by render type, through {@code batchFor}, which it calls first.
 */
@Mixin(targets = "dev.skycraft.client.render.AvatarExporter", remap = false)
public abstract class MarkBlended {
    /** SkyCraft's flags for a translucent particle, with the triangle's own normal: translucent, no mipmaps, direction 7. */
    @Unique private static final int BLENDED = 2 | 8 | 7 << 4;
    /** Bit 7 of an entity vertex's flags, free within SkyCraft's 8-bit key: BlockRender adds such a layer. */
    @Unique private static final int ADDITIVE = 1 << 7;
    /** Minecraft 26.3's render types whose pipelines blend by alpha, two-sided, and that BlockRender blends too. */
    @Unique private static final Set<String> BLENDED_TYPES = Set.of("entity_translucent", "entity_translucent_emissive", "breeze_wind",
        "wolf_armor_cracks");
    /** The one whose pipeline adds. */
    @Unique private static final String ADDITIVE_TYPE = "energy_swirl";

    /** The flags of the layer being submitted: 0 keeps SkyCraft's. */
    @Unique private int cybercraft_tweaks$flags;

    @Inject(method = "submitModel", at = @At("HEAD"))
    private void cybercraft_tweaks$seeBlend(Model<?> model, Object state, PoseStack poseStack, RenderType renderType, int lightCoords,
        int overlayCoords, int tintedColor, UvMapping uvMapping, int outlineColor, CallbackInfo info) {
        String name = ((RenderTypeAccessor) renderType).skycraft$name();
        this.cybercraft_tweaks$flags = BLENDED_TYPES.contains(name) ? BLENDED : ADDITIVE_TYPE.equals(name) ? BLENDED | ADDITIVE : 0;
    }

    // The flags batchFor picks the layer's batch with: SkyCraft's SOLID, or the blend noted at submitModel's head.
    @ModifyArg(method = "batchFor", at = @At(value = "INVOKE",
        target = "Ldev/skycraft/client/render/AvatarExporter;batch(III)Ldev/skycraft/client/render/AvatarExporter$Batch;"),
        index = 2)
    private int cybercraft_tweaks$blendFlags(int flags) {
        return this.cybercraft_tweaks$flags != 0 ? this.cybercraft_tweaks$flags : flags;
    }
}
