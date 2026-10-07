# Minecraft's entities in Night City: results

Spike 7 draws Minecraft's entities, block entities, particles and world entities in Night City
([the spec](../superpowers/specs/2026-10-06-minecraft-entities-design.md)). Step 0 read what SkyCraft really sends of
them, with `mclink`, without the game; later steps add their sections as they run, and the spike's answer goes at the
top once step A is in.

## Step 0: what Minecraft sends of its entities, with `mclink`, without the game

Date: 2026-10-07. No Cyberpunk: `mclink`, BlockRender's link code in a console program, ran against the user's
Minecraft with SkyCraft's unchanged mod. The versions:
- **Minecraft 26.3**, Fabric Loader 0.19.5, Fabric API 0.161.0+26.3 and Java 25, in the Modrinth App's `CyberCraft`
  instance, with CyberCraft Tweaks 1.3.0, its particles at All and its FOV 90°;
- **SkyCraft 0.1.2**, the release jar, protocol version 11;
- **BlockRender's link code and `mclink`** at `65d1723`: the plan's Tasks 1 to 4, built RelWithDebInfo, 465 unit tests
  passing.

The user started and stopped Minecraft. The agent ran `mclink` with step 0's script, `entities.txt`, read its output
and Minecraft's log, and saved every texture as DDS (`build/step0/`). Minecraft opened its world, SkyCraft's own
`SkyCraft`, by itself as the link came up, and the run took 43 s. Plan:
[2026-10-06-minecraft-entities.md](../superpowers/plans/2026-10-06-minecraft-entities.md), Task 5.

### Verdict

**Step 0: pass.** The link's new code works against SkyCraft's real mod as written: nothing in it changed after
running it, no message was rejected and the ring never resynced. Every test thing showed as the plan expected, and the
clear left nothing: its last scene was empty, and its world line said `nothing`. One expectation was wrong, and the
script missed one command:
- **The blast's scene peaked at 0.13 MB,** not near 1 MB: spike 4's 1 MB a frame was `tnt-cube`'s 125 TNT, not one
  TNT.
- **`/gamemode creative` wasn't typed:** it came 0.15 s after Minecraft said it was in its world, while its loading
  screen was still open, and `mclink`'s typer drops a command while a screen is open. Minecraft's player stayed in
  survival, its mode since spike 6, which changed nothing here.

### The checks

- **Linking:** `Minecraft is there` at 0.77 s, Minecraft in its world by 2.4 s, `clear all` and the atlas
  (2048 × 2576, 20.1 MB) at 2.56 s, and the city around region 110 0 250 all in the ring at 5.09 s (1,152 regions).
- **Textures:** 14, 3.64 MB in all, none from before the atlas, and every one readable (no `isn't available`).
  Minecraft's log names each id:

  | Id | Size | Texture |
  |---|---|---|
  | 1 | 64 × 64 | `textures/entity/player/slim/makena.png`, Minecraft's default slim skin |
  | 2 | 64 × 64 | `skins/8cf45343…`, the user's skin |
  | 3 | 64 × 32 | `capes/5d56c83d…`, the user's cape |
  | 4 | 64 × 64 | `textures/entity/pig/pig_temperate.png` |
  | 5 to 8 | 64 × 64 | the villager's `villager.png`, `type/plains.png`, `profession/farmer.png` and `profession_level/iron.png` |
  | 9 and 10 | 64 × 32 | `spider/spider.png` and `spider/spider_eyes.png` |
  | 11 | 64 × 32 | `blaze/blaze.png` |
  | 12 | 512 × 256 | `textures/atlas/particles.png`, 0.5 MB |
  | 13 | 512 × 512 | `textures/atlas/chest.png`, 1 MB |
  | 14 | 1024 × 512 | `textures/atlas/banner_patterns.png`, 2 MB |

  Ids 1 to 3 are the player's own, for the ragdoll SkyCraft sends about once a second (41 of them), not for the scene,
  which leaves the player out.
- **The scenes,** each the newest Minecraft had sent, about 60 a second:

  | Scene | Batches | Vertices | Glowing | Translucent | What's new in it |
  |---|---|---|---|---|---|
  | the pig and the villager, 3 s on | 5 | 1,692 | 0 | 0 | the villager's four layers (396, 396, 396 and 252 vertices) and the pig (252), each its own texture |
  | the spider and the blaze, 3 s on | 9 | 3,396 | 468 | 0 | the blaze, 468 vertices, all glowing; the spider's eyes, the whole spider model (396) with its eye texture, none glowing; the blaze's smoke in the particles' atlas (444), none glowing |
  | the falling sand, 0.55 s on | 10 | 3,486 | 468 | 0 | the sand in the air, an atlas batch of 36 vertices |
  | the chest and the banner, 2 s on | 11 | 3,744 | 468 | 0 | the chest's atlas, 108 vertices (3 boxes); the banner's atlas, 216 (its pole, bar and cloth, then the pattern base and both patterns, in one batch) |
  | the items and the arrow, 2 s on | 11 | 3,750 | 468 | 0 | nothing: dropped items and arrows are only in the world table |
  | the firework, 1.55 s on, looking up | 12 | 6,204 | 2,856 | 2,388 | its burst's 398 sparks, a translucent batch in the particles' atlas, all glowing |
  | the potion, 1.03 s on | 13 | 5,568 | 1,746 | 1,824 | its 91 spell particles (546 vertices), translucent and not glowing, beside the firework's last sparks (1,278, glowing); its 3 glass shards, an atlas batch of 18 |
  | the TNT, 2.03 s on | 13 | 4,008 | 504 | 102 | the TNT, an atlas batch of 36 vertices, all glowing: its white flash |
  | the TNT, 2.34 s on | 13 | 4,026 | 504 | 126 | the TNT, all 36 glowing |
  | the TNT, 2.63 s on | 13 | 4,020 | 468 | 120 | the TNT, none glowing |
  | 0.39 s after the blast | 12 | 4,152 | 720 | 78 | the TNT gone; 798 particle vertices, 252 of them glowing: the explosion's 42 puffs, and its smoke |
  | 7 s after the clear | 0 | 0 | 0 | 0 | nothing: SkyCraft's one empty scene |

  From the potion on, a translucent batch of 102 to 126 vertices, none glowing, stayed until the clear: most likely
  the swirls of the Regeneration the potion gave the mobs near it.
- **The villager's layers come in the wrong order:** every scene had its level badge (iron) first, then its body, the
  pig, its profession (farmer) and its biome (plains); Minecraft draws the body, the biome, the profession and the
  level, in that order. SkyCraft keys its batches by texture, UV mode and flags in a `HashMap`, so they come out by the
  texture's id XOR the flags, modulo the table's size: this run gave the badge id 8 and the body id 5, so the badge came
  first. With step A's rule that a later triangle wins at equal depth, the body would cover the badge, and the biome's
  clothes the profession's. The order follows the ids, which follow what Minecraft drew first in the session, so a
  layered model can be right in one session and wrong in the next. The spider's eyes came after its body (ids 10 and 9):
  right, by chance. The banner is right: its cloth and patterns are one batch, in Minecraft's order.
- **The world table:** 2 shadows with the pig (0.9 wide) and the villager (0.6); 4 with the spider (1.4) and the blaze
  (0.6, floating a block up, its AI off); then `1 arrow, 1 block, 1 item, 4 shadows`:
  - the diamond, an `item` 0.5 wide, bobbing and turning;
  - the grass block, a `block` 0.25 wide;
  - the arrow, which flew off the raised ground and stuck in the ground below, 17.7 blocks east and 9 m down, its pitch
    −60.5.

  The splash potion in flight made one 5-second line `2 items`, and the world line said `nothing` after the clear. Of
  about 2,400 updates, one found the table mid-write all 64 times and kept the last good one; no record was left out.
- **The blast:** 4.00 s after its command, at 898.50 6.06 2007.50, on the obsidian, which held: the clear changed it
  back to air. The largest scene in the blast's 5 seconds was 0.13 MB (4,182 vertices).
- **The stream:** while something was there, 60 scenes a second of 0.05 to 0.19 MB, 3 to 8.5 MB a second.
  - In all: 2,121 scenes (204 MB).
  - Beside them: 33,688 atlas patches (312 MB, Minecraft's animated textures, as in spikes 5 and 6), 41 ragdoll
    messages, and 5 sections (the placed blocks).
  - No resyncs, and nothing rejected.
- **Minecraft's log:** a `[CHAT]` reply to every command typed, and no error. One warning came as SkyCraft hid the
  window and opened its world: Minecraft's Vulkan backend's `Couldn't acquire next surface texture … swapchain out of
  date`, after which it ran at 60 frames a second.
- **The clear:**
  - the three placed blocks went back to air;
  - `Killed 7 entities`: the four mobs, the two items and the arrow (the sand had vanished as it landed);
  - `Killed 3 entities`: the markers;
  - the potion had given Minecraft's player, 3.6 blocks away, a few seconds of Regeneration; nothing else of the user's
    world changed;
  - `exit: 0`.

### Surprises

1. **SkyCraft sends a scene only while something is there.** None came before the pig. After the clear, one empty scene
   came once the last thing had gone (origin 0 0 0, no batches; `AvatarExporter.sendEmpty`, which waits for room in the
   ring), then none.
2. **Mobs don't walk on Night City's ground.** The pig and the villager, their AI on, never moved in 33 s. A ground mob
   strolls only to a spot with a solid Minecraft block under it (`PathNavigation.isStableDestination` in 26.3), and
   under the city's ground there's air.
3. **Firework sparks glow, spell particles don't.** Every spark of the burst had block light 15; the potion's particles
   had the world's light. Both are translucent.
4. **The player's own textures come first,** for the ragdoll: Minecraft's default skin, then the user's skin and cape.
5. **`/gamemode creative` came too soon** ("Verdict").

### What it means for Part 2

- **Sizes:** the scenes here were at most 6,222 vertices (0.19 MB), and `tnt-cube`'s about 1 MB: the spec's snapshot
  buffers, up to 8 MB each, hold them with room to spare. The textures came to 3.64 MB, 3.5 MB of it three atlases, far
  below the 64 MB cap; signs' and beds' atlases will add a few MB.
- **The glow rule (block light 15) takes what it should:** the blaze, the TNT's flash, the explosion's puffs and the
  firework's sparks. It leaves the spider's eyes, as the spec read, and the smoke and the spell particles. For step B,
  the sparks are translucent and glowing, so they're drawn unlit; the spell particles translucent and lit.
- **`Entities` keeps the last scene until an empty one comes:** SkyCraft sends nothing while nothing is there.
- **Layered models are left to chance:** with this session's ids the villager's layers would show wrong. Run A's check
  7 will show it, and step C's draw order is the spec's fix.
- **Run A's smearing check needs something that moves:** a pig on the city's ground stands still, so Part 2's plan picks
  something else, such as a falling block, or a mob given `Motion`.
- **The script:** `type /gamemode creative` goes after `wait-ground`, once the loading screen has closed.
- **The vertex flags weren't printed:** step A takes SkyCraft's constants for them (`SOLID`, `PARTICLE` and
  `PARTICLE_BLENDED`). Each batch's translucent flag matched its particles' layer.

## Step C's draw order, with `mclink`

Date: 2026-10-07. Step 0's script again, `entities.txt` with Task 6's fix (creative typed after `wait-ground`), against
SkyCraft 0.1.2 with CyberCraft Tweaks 1.4.0, without the game. The versions are step 0's but for CyberCraft Tweaks
1.4.0 (1.3.0 kept beside it, disabled) and BlockRender's link code and `mclink` at `85d643c`. The user started and
stopped Minecraft; the agent installed the mod with the user's leave, with Minecraft stopped. Plan: Task 7.

### Verdict

**Pass.** Every scene's batches came in Minecraft's draw order, a villager's layers and a spider's eyes after their
bodies, and nothing else changed from step 0. Minecraft loaded the mod, with no mixin error before or after the run.

### The checks

- **As step 0:** linking (`clear all` at 2.05 s, the city in the ring at 4.77 s), the same 14 textures with the same
  ids, every scene's batches and vertices within a few of step 0's (what moves), the world table, the blast (3.97 s
  after its command, at 898.50 6.06 2007.50), the stream (2,097 scenes), and the clear: `Killed 7 entities` and
  `Killed 3 entities`, the last scene empty and its world line `nothing`. No resyncs, nothing rejected, `exit: 0`.
- **The batch order,** by texture id (step 0's run gave the same ids):

  | Scene | Step 0 | With CyberCraft Tweaks 1.4.0 |
  |---|---|---|
  | the pig and the villager | 8, 5, 4, 7, 6 | 4, 5, 6, 7, 8 |
  | with the spider and the blaze | 9, 8, 11, 10, 12, 5, 4, 7, 6 | 4, 5, 9, 11, 12; 6, 10; 7; 8 |

  Minecraft's order first, then the texture's id: at order 0 each model's base (the pig 4, the villager's body 5, the
  spider 9, the blaze 11, the particles' atlas 12, and Minecraft's combined atlas first of all); at order 1 the
  villager's biome 6 and the spider's eyes 10; at order 2 its profession 7; at order 3 its level 8.
- **The banner:** step 0's one batch of 216 vertices (texture 14) is now four, in Minecraft's order: its pole, bar and
  cloth (108 vertices) at order 0, and its pattern base and two patterns (36 each) at orders 1 to 3.
- **Creative:** `[CHAT] Set own game mode to Creative Mode`, once the loading screen had closed.
- **A warning in Minecraft's log,** most likely in step 0's too, whose log was kept only in part: `Failed to decode
  value '{Name:"minecraft:sand"}' from field 'BlockState'`. 26.3 reads a falling block's `BlockState` as a block id
  (`BlockState:"minecraft:sand"`) or a map keyed `id` (`BlockState.CODEC`: `Codec.either(BLOCK.byNameCodec(),
  FULL_CODEC)`). The sand came out right only because sand is the falling block's default. Step 0's script and the
  overlay now give the id.

### The overlay's Spawn rows, with `mclink`

With the user's leave, while Minecraft was still up: every command of the overlay's Spawn rows (with the sand fixed) and
of Clear, typed as its buttons type them, at step 0's spot, by a scratch script (not committed), to check each against
26.3 before run A.
- **Every command was answered** (`Summoned new …`, `Changed the block at …`), with no warning or error in Minecraft's
  log.
- **The slime** came as two batches: its inner body (144 vertices) at order 0 and its outer layer (36) at order 1.
- **The bed, the sign and the torch are block models in 26.3,** in SkyCraft's section mesh, not entities: the section's
  mesh went from 0 vertices to 90 with the bed's foot, 180 with its head and 246 with the sign, while the chest and the
  banner added nothing there and came as entities, with their atlases (textures 13 and 14). No bed or sign atlas was
  sent. So the bed, the sign and the torch are drawn with the blocks, and step A draws the chest and the banner.
- **Clear left nothing:** it set the six placed blocks back to air, killed 17 entities (the five mobs, the ten pigs
  and the two items) and the six markers; the last scene was empty, its world line `nothing`, and the status's
  `solids` 0 blocks, after 3 before it (the chest and the bed's halves). The falling sand had vanished where it landed.
- **Small batches of Minecraft's combined atlas** (6 and 24 vertices) came and went with the slime's hops, most likely
  its slime-ball particles, from the items' atlas.

### What it means for Part 2

- **Run A's check 7** should see the villager's clothes and the spider's eyes over their bodies in the game.
- **Run A's check 4:** the bed, the sign and the torch come with the blocks; the chest and the banner are step A's.
- **The Spawn rows and Clear are checked against 26.3** before the game. `tnt-ahead`'s obsidian and marker weren't
  typed here: they're step 0's TNT block and marker in another form, which run A types.
