package dev.cybercraft.tweaks;

import dev.cybercraft.tweaks.paths.PathStats;
import dev.skycraft.world.SkyDig;
import net.fabricmc.api.ModInitializer;
import net.fabricmc.fabric.api.attachment.v1.AttachmentTarget;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerChunkEvents;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerTickEvents;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Drops what SkyCraft dug before this mod. Nothing digs into the game's ground any more ({@code NoBlastBlocks},
 * {@code NoDigging}), so SkyCraft's record of dug cells, the {@code skycraft:dug} attachment on each chunk, holds only
 * what earlier blasts left: holes in Minecraft's copy of the ground, whose walls SkyCraft draws and the game shows
 * floating. Each chunk drops it as the server loads it; SkyCraft syncs the attachment, so clients drop it with it.
 *
 * <p>It also writes the pathfinding mixins' minute line ({@link PathStats}) from the server's tick.
 */
public final class CyberCraftTweaks implements ModInitializer {
    public static final Logger LOG = LoggerFactory.getLogger("cybercraft_tweaks");
    /** In a kInHurt's flags, beside SkyCraft's blocked (1) and power attack (2): set the health instead ({@code SetHealth}). */
    public static final int SET_HEALTH = 0x100;

    @Override
    public void onInitialize() {
        ServerChunkEvents.CHUNK_LOAD.register((level, chunk, generated) -> {
            // LevelChunk is an attachment target at runtime, through Fabric API; the vanilla jar this compiles against
            // doesn't say so.
            AttachmentTarget target = (AttachmentTarget) chunk;
            if (target.hasAttached(SkyDig.DUG)) {
                target.removeAttached(SkyDig.DUG);
            }
        });
        ServerTickEvents.END_SERVER_TICK.register(server -> PathStats.tick());
    }
}
