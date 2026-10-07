# CyberCraft Tweaks

A Fabric mod for the `CyberCraft` Minecraft instance, beside SkyCraft, that adjusts SkyCraft for Night City.

Night City's ground can't be dug in the game, so Minecraft's copy of it isn't either: otherwise Minecraft's player and V
would disagree about where the ground is.
- **Blasts leave the game's ground as it is** (`NoBlastBlocks`). SkyCraft digs into Skyrim's ground where an explosion
  reaches it, turning the ground in the blast into stone and ore blocks and lining the hole with more. Night City isn't
  Skyrim's terrain: the cells SkyCraft takes for underground there can be open air, so the blocks were left floating
  above a blast (spike 5's step 4, in `docs/spikes/2026-10-03-collision-and-movement-results.md`). The mixin skips
  SkyCraft's `SkyDigBlast.materialize` and `finish`. An explosion still breaks Minecraft's blocks, the game's ground
  still shields them, and the game still hears of the explosion.
- **Nobody digs into it** (`NoDigging`). SkyCraft's `SkyDig.open` digs a cell a player mines with a tool, and its
  `reveal` turns the ground behind a dug cell's exposed walls into blocks: both are skipped.
- **What was dug before is undug** (`CyberCraftTweaks`). SkyCraft keeps its dug cells as a `skycraft:dug` attachment on
  each chunk: holes in Minecraft's copy of the ground, whose walls SkyCraft draws, and the game shows floating. Each
  chunk drops it as the server loads it, and SkyCraft's sync takes that to clients.
- **V's health carries into Minecraft as build mode starts** (`SetHealth`, from 1.3.0). With combat on, BlockRender
  sends V's share of its health when build mode starts, as SkyCraft's `kInHurt` with no damage, `0x100` among its
  flags, and the share × 10,000 where an attacker's form ID would be. SkyCraft's own `hurtPlayer` leaves a hurt with no
  damage alone, so without this mod nothing happens; with it, Minecraft's player's health becomes the same share of its
  max health, and Minecraft's log says so.

What SkyCraft sends of Minecraft's entities keeps Minecraft's draw order and its glow:
- **An entity's layers come in order** (`KeepDrawOrder`, from 1.4.0). Minecraft draws an entity's layers each over the
  one before at the same depth: a villager's body, then its biome's clothes, its profession's and its level's badge
  (`order` 0 to 3), a spider's eyes over its body, a banner's patterns over its cloth. SkyCraft's `AvatarExporter`
  answers `order(n)` with itself and keeps its batches in a `HashMap`, so they go out in the textures' ids' order:
  spike 7's step 0 had a villager's badge before its body. With the mod, `order(n)` hands back a stand-in that submits
  with n as the order, each batch is kept by its order, and the batches go out lowest order first.
- **Emissive layers glow** (`MarkEmissive`, from 1.5.0). Minecraft draws some layers without its light map: those of
  its `eyes` render type (a spider's, an enderman's and a phantom's eyes), `entity_translucent_emissive` (the warden's,
  the creaking's, the copper golem's and the breeze's glowing parts) and `energy_swirl` (a charged creeper's and the
  wither's armour). SkyCraft keeps the light Minecraft passes them, the entity's own, so the game lit them as the rest:
  spike 7's run B saw a spider's eyes at night as dark as its body. With the mod, such a layer goes with full light, as
  SkyCraft sends lit TNT's white flash, and BlockRender's glow rule makes it glow.

It targets SkyCraft 0.1.2's classes by name, so `fabric.mod.json` asks for exactly that version: Fabric won't start with
another SkyCraft until this mod is checked against it.

SkyCraft's own "Skyrim destruction" button, at the top left of Minecraft's pause menu (`destruction=` in
`config/skycraft.properties`), turns its digging off too, with two differences: blasts then lose SkyCraft's explosion
resistance, so the city's ground no longer shields blocks behind it; and holes already dug stay. The mod works whichever
way the button is set.

## Build

```bash
bash build.sh
```

Needs a JDK 21 or later on `PATH`. No Gradle and nothing downloaded: it compiles against the jars the Modrinth App
already keeps for the instance (Minecraft 26.3, Fabric Loader, the mixin library, Fabric API and SkyCraft), and packs
`build/cybercraft-tweaks-<version>.jar`.

## Install

Copy the jar into the instance's mods folder, `%APPDATA%\ModrinthApp\profiles\CyberCraft\mods`, with Minecraft stopped.
Delete it to remove the mod. Blocks earlier blasts left stay in the world: break them, or `/fill` them with air; with
the mod, nothing is revealed behind them.

## Test

`spikes/block-render/tools/mclink/blast.txt` blows up TNT on Night City's ground at a fresh spot: with the mod, the solid
blocks around it are the same afterwards as before. `spikes/block-render/tools/mclink/blasts.txt` sets Minecraft's
player's health to half, all and a twentieth of its max, and reads each back with `/data`.
`spikes/block-render/tools/mclink/emissive.txt` summons a spider, an enderman, a warden and a charged creeper: with the
mod, every vertex of their emissive layers' batches glows (block light 15), and their bodies' don't.
`spikes/block-render/tools/mclink/entities.txt` prints each scene's batches in the order they come: with the mod, a
villager's body comes before its biome's clothes, those before its profession's, and those before its level's badge.
