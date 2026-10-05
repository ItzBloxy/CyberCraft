# NPCs and combat: results

Spike 6 brings Night City's NPCs into Minecraft as SkyCraft's stand-ins (Part 1), then combat both ways (Part 2), then
Minecraft's explosions in the game (Part 3). Step 1 ran SkyCraft's actor table with `mclink`, without Cyberpunk; later
steps add their sections as they run, and the spike's answer goes at the top once step 2 is in.

## Step 1: made-up NPCs in SkyCraft's actor table, with `mclink`, without Cyberpunk

Date: 2026-10-05. No Cyberpunk: `mclink`, BlockRender's link code in a console program, ran against the user's
Minecraft with SkyCraft's unchanged mod. The versions:
- **Minecraft 26.3**, Fabric Loader 0.19.5, Fabric API 0.161.0+26.3 and Java 25, in the Modrinth App's `CyberCraft`
  instance, with `--enable-native-access=ALL-UNNAMED -Dskycraft.quitWithSkyrim=false`, and CyberCraft Tweaks 1.2.0;
- **SkyCraft 0.1.2**, the release jar, protocol version 11;
- **BlockRender's link code and `mclink`** at `984187c`: the plan's Tasks 1 to 3, built RelWithDebInfo, 408 unit tests
  passing.

The user started and stopped Minecraft. The agent ran `mclink` with step 1's script, `npcs.txt`, and read its output and
Minecraft's log. Plan: [2026-10-05-npcs-and-combat.md](../superpowers/plans/2026-10-05-npcs-and-combat.md), Task 4.

### Verdict

**Step 1: pass.** The actor table works against SkyCraft's real mod as written: nothing in the code changed after
running it, and every check passed. Two of the plan's expectations had the wrong words, not the wrong behaviour:
- Minecraft 26.3's reply to a passing `/execute if entity` reads `Test passed. Count: 3`, not `Test passed, count: 3`;
- SkyCraft logs its stand-in count only when stand-ins are added, never when they're removed, so the plan's expected
  `2` and `0 Skyrim actors mirrored as hittable stand-ins` lines don't exist. `SkyCombat.sync` removes stale stand-ins
  before it counts them (SkyCraft 0.1.2's code). The `/execute` counts showed both removals.

### The checks

- **Linking:** `Minecraft is there` 0.35 s after `mclink` started, then `floor 1 at height 6.14 around Minecraft 520.5
  2007.5` at 1.94 s. Minecraft was already in its world.
- **Stand-ins:** the three records went in at 5.05 to 5.08 s. SkyCraft logged `1` and then `3 Skyrim actors mirrored as
  hittable stand-ins` (its server tick saw the first record before the other two), and `/execute if entity
  @e[type=skycraft:skyrim_actor]` replied `Test passed. Count: 3`.
- **Hits:** in build mode, looking 180 20 at actor 1, 2 blocks north of the stand-in V:
  - a left click with an empty hand came back as `hit: actor 1 for 1.00, knockback 0.40 towards x -0.00 z -1.00;
    unarmed`, a second after the click;
  - with the diamond sword, its cooldown full after 2 s, `hit: actor 1 for 7.00, knockback 0.40 towards x -0.00 z
    -1.00; blade`;
  - SkyCraft logged `hit Ganger One for 1.0 (knockback 0.4)` and `hit Ganger One for 7.0 (knockback 0.4)`: the
    stand-in carries the record's name, and the knockback pushes it away from the player, north.
- **The plate:** the stone block, the stone pressure plate on it and the redstone lamp beside the plate were set by
  command, and Minecraft sent the section back after each. The lamp's three checks all passed: out at first; lit 2 s
  after actor 1 moved onto the plate, its feet on the block's top at y 7; out again 3 s after it moved back. SkyCraft's
  stand-in pressed the plate through its whole-block check, as the spec read.
- **Removal:** `actor-remove 2` left 2 records, and `/execute if entity` counted 2. `actor-remove all` emptied the table
  (`0 actors in the table`), and `/execute unless entity @e[type=skycraft:skyrim_actor]` passed: SkyCraft removed every
  stand-in once the table was empty.
- **The status:** `events: 0 explosions, 2 hits on actors, 0 deaths, 0 others; 0 actors in the table; 0 inputs dropped;
  …`.
- **No `Test failed`** in Minecraft's log, and `exit: 0`.

The render ring carried 21,264 atlas patches (197 MB, Minecraft's animated textures, as in spike 5), 7 sections, 1,519
HUD images and 26 `ragdoll` messages (SkyCraft's model of the player's body, sent about once a second while it's alive),
with no resyncs and nothing rejected.

### What it means for step 2

- **The table, the stand-ins and their hits work as SkyCraft's own game side's do,** so step 2 changes only where the
  records come from: the game's NPCs, through the module.
- **Minecraft's damage arrives exactly:** 1 for a fist and 7 for a diamond sword at full cooldown, so Part 2's amount,
  Minecraft's damage × V's punch, starts from Minecraft's own numbers.
- **Counting stand-ins:** SkyCraft's log shows stand-ins being added, not removed, so step 2 counts them with `mc
  /execute if entity @e[type=skycraft:skyrim_actor]` instead of waiting for a log line. SkyCraft reads typed input
  whenever BlockRender's heartbeat is fresh, which it stays while unlinked, so the count works then too. The plan's
  Task 11, steps 3, 6 and 11, now says so.
- **A plate in a stand-in's block lights its lamp:** on the pavement, a plate placed in build mode sits in the block the
  pavement's surface is in, which a stand-in's box overlaps from the NPC's feet up.
