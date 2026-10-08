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

What SkyCraft sends of Minecraft's entities keeps Minecraft's draw order, its glow and its blending:
- **An entity's layers come in order** (`KeepDrawOrder`, from 1.4.0). Minecraft draws an entity's layers each over the
  one before at the same depth: a villager's body, then its biome's clothes, its profession's and its level's badge
  (`order` 0 to 3), a spider's eyes over its body, a banner's patterns over its cloth. SkyCraft's `AvatarExporter`
  answers `order(n)` with itself and keeps its batches in a `HashMap`, so they go out in the textures' ids' order:
  spike 7's step 0 had a villager's badge before its body. With the mod, `order(n)` hands back a stand-in that submits
  with n as the order, each batch is kept by its order, and the batches go out lowest order first.
- **Emissive layers glow** (`MarkEmissive`, from 1.5.0). Minecraft draws some layers without its light map: those of
  its `eyes` render type (a spider's, an enderman's, a phantom's, a creaking's and a copper golem's eyes),
  `entity_translucent_emissive` (the warden's glowing layers and a breeze's eyes) and `energy_swirl` (a charged
  creeper's and the wither's armour). SkyCraft keeps the light Minecraft passes them, the entity's own, so the game lit
  them as the rest: spike 7's run B saw a spider's eyes at night as dark as its body. With the mod, such a layer goes
  with full light, as SkyCraft sends lit TNT's white flash, and BlockRender's glow rule makes it glow.
- **Blended layers go blended** (`MarkBlended`, from 1.6.0). SkyCraft sends every layer of an entity model solid, so
  the game drew the warden's faint glowing layers and a charged creeper's armour solid (spike 7's run C), and a slime's
  outer layer, allays and vexes as cut-outs. With the mod, a layer of a render type Minecraft blends by alpha
  (`entity_translucent`, `entity_translucent_emissive`, `breeze_wind` and `wolf_armor_cracks`) goes with SkyCraft's
  translucent flag and its own normal, so its batch goes translucent; `energy_swirl`, which Minecraft adds, goes with
  bit 7 as well, for additive. BlockRender 0.22.0 draws those that are really see-through blended, before DLSS, and
  the rest as before. `eyes` and `banner_pattern` stay solid: every eyes texture is opaque or clear, and banner
  patterns lie on cloth the game lights.

Minecraft's mobs find their way on Night City's ground (from 1.7.0). SkyCraft's copy of the city isn't blocks: its
collision mixin adds the city's voxels to every collision query, so mobs stand on the streets, but Minecraft 26.3's
pathfinding decides where a mob can walk from block states, and under the city there's only air. Spike 7's mobs never
moved, and a chase aimed at the top of the world. With the mod, where Minecraft has nothing of its own, the city's cells
answer pathfinding's questions (`dev.cybercraft.tweaks.paths`):
- **A cell goes by its middle** (`CellRule`, `CityCells`): the central 6 × 6 of its 8 × 8 voxel columns, where a mob up
  to 0.75 wide stands at its centre. Nothing there is air, as before; a top in the lower half is a floor, walkable
  itself, since the city's skins have nothing solid under them; a top in the upper half is a full block, which the cell
  above stands on. A cell whose middle is mostly open, counting the cell below's upper half as covering a column, is a
  fence (from 1.7.1): a railing, a barrier, a pole, a ledge's last sliver over a drop. Mobs don't path through it or
  onto it, since its top counts half a block higher than it is, as a Minecraft fence's does; spike 9's step 0 saw them
  walk onto a road's barrier and fall off its far side.
- **Walking** (`CityWalkNodes`, `CityPathCache`, `CityNavigation`, `CitySurface`, `CityGoalUtils`): the walk
  evaluator's path types, floor heights and airborne starts; the shared path-type cache skipped for the city's cells;
  strolls ending on the city's floors; paths aimed at its ground, not the top of the world; and its blocked cells solid
  to random positions.
- **Pets keep up** (`CityPetTeleport`): a pet teleporting to its owner lands on the city's floor, not inside it, which
  it would fall through, and with its fall distance reset (from 1.7.1), or one teleported while falling would take the
  whole fall's damage as it lands.
- **Walls block sight and flyers' shortcuts** (`CitySight`, `CityNavigation`): a mob's line of sight and a flyer's
  straight-line shortcut also cast against the city's triangles (`CityRays`), as arrows do.

Each minute it did any of this, Minecraft's log gets one line: the city's answers, the shapes classified, the casts with
their hits and time, and the pets landed (`PathStats`).

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

```bash
bash test.sh
bash mixins.sh
```

`test.sh` runs the unit tests of the pathfinding's rules (`CellRule`), with plain `java` and no Minecraft. `mixins.sh`
checks the mixins on Minecraft's own classes against Minecraft 26.3's jar, as Mixin would when Minecraft loads them, so
a wrong target fails there rather than at Minecraft's start; it reads the classes `build.sh` compiles.

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
`spikes/block-render/tools/mclink/blended.txt` summons a slime, an allay, a vex, a breeze, a warden, a charged creeper
and a piglin: with the mod, their blended layers' batches come translucent, the armour's additive too, and BlockRender
draws the slime's outer layer, the allay, the vex, the breeze's wind and eyes and the warden's layers blended, and the
piglin, opaque, as a model.
`spikes/block-render/tools/mclink/entities.txt` prints each scene's batches in the order they come: with the mod, a
villager's body comes before its biome's clothes, those before its profession's, and those before its level's badge.
`spikes/block-render/tools/mclink/paths.txt` puts mobs on spike 1's dumped ground: with the mod, a pig, a cow and a
villager stroll the road, a cow follows wheat, a wolf follows its owner and teleports to them, a husk chases the player,
a skeleton below the road's edge doesn't shoot until it sees the player, and a bee follows a dandelion down the rock
face; without it, none of that happens but the skeleton's shooting, through the rock.
`spikes/block-render/tools/mclink/teleports.txt` makes a pet wolf follow its owner as the stand-in V jumps 22 blocks
along the same road five times: with the mod, it lands beside V on the street each time, on the ground.
