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
blocks around it are the same afterwards as before.
