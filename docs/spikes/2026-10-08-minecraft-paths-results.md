# Minecraft's mobs finding their way in Night City: results

Spike 9 makes Minecraft's mobs find their way on Night City's ground
([the spec](../superpowers/specs/2026-10-08-minecraft-paths-design.md)): wander, go to targets, keep up with their
owner as pets, fly around the city's walls, and not see V through them, from CyberCraft Tweaks 1.7.0. Step 0 checks
Minecraft's side with `mclink`, without the game, twice: first with 1.6.0 as the control, then with 1.7.0. Run F checks
it in the game.

The spike's answer goes here once run F is in.

## Step 0's control: CyberCraft Tweaks 1.6.0

Date: 2026-10-08. No Cyberpunk: `mclink` placed spike 1's raised-level dump (`2026-10-01_14-37-07`, 64,192 static
triangles) around 1280.5 6.14 2007.5 as Night City's ground, linked to the user's Minecraft with SkyCraft's unchanged
mod, and ran step 0's script, `spikes/block-render/tools/mclink/paths.txt`. The versions:
- **Minecraft 26.3**, Fabric Loader 0.19.5, Fabric API 0.161.0, in the Modrinth App's `CyberCraft` instance, with
  CyberCraft Tweaks **1.6.0** and the user's Entity Model Features and Entity Texture Features (left enabled, by the
  user's choice: they change how entities are drawn, not where they walk);
- **SkyCraft 0.1.2**, the release jar, protocol version 11;
- **`mclink`** from BlockRender's build on `main`, unchanged by this spike.

Plan: [2026-10-08-minecraft-paths.md](../superpowers/plans/2026-10-08-minecraft-paths.md), Task 8. The user started
Minecraft; the agent ran the script and read its output and Minecraft's log. Minecraft opened its world, `SkyCraft`,
by itself as the link came up, and the script took 168 s.

### Two things fixed before the run that counts

The first run (15:02) went to its end, cleared everything and put the game mode back, but two of its checks didn't
test anything:
- **The skeleton was never summoned.** Its command, with its bow, unbreakable helmet and drop chances, was 298
  characters, and `mclink`'s typer refuses more than Minecraft's 256-character chat line (`line 115: a command is at
  most 256 characters`). Written with Minecraft's default namespace and count it's 222 (`5ff4610`). The script's parse
  check couldn't see it: the typer checks a command's length only as it types it.
- **The wheat and the dandelion never reached the player:** its off hand held a shield, and the script fills the off
  hand only if it's empty, so no `Replaced 1 slot(s)` line came. The tempt and flyer checks failed for that alone. The
  user chose to move the shield themselves; Minecraft's window stays hidden after a link (SkyCraft hides it and doesn't
  show it again), and opening the world by hand would load the player over the void with no city's ground, so a short
  guarded `mclink` script moved it to empty inventory slot 15 instead, with the user's leave: copied only into an empty
  slot, the off hand emptied only once the slot held the shield (`Test passed. Count: 1`, then the off hand's `Test
  failed`). A reverse script puts it back after the 1.7.0 run.

Both runs' wander, pet and chase checks agreed.

### The run (15:16)

`exit: 0`; no `WARN` line and no command error in Minecraft's log. 26.3 words `/execute if`'s replies `Test passed.
Count: 1` and `Test failed`.

| Check | With 1.6.0 |
|---|---|
| Wander | the pig, the cow and the villager never moved: all 39 positions (13 each, every 5 s for 60 s) exactly where they were summoned (1272.5 6.0 2007.5, 2012.5, 2002.5); all three `OnGround` |
| Tempt | wheat in the off hand (`Replaced 1 slot(s) on ItzBloxy with [Wheat]`); `Test failed`, the cow still at 1278.5 6.0 2009.5 after V walked 10.5 blocks south |
| Pet | `Test failed` after V walked about 15 blocks south, and again after V's jump 21 blocks north: the wolf never left 1274.5 6.0 2007.5, never teleported; `OnGround` |
| Chase | `Test failed`, the husk still at 1276.5 6.0 2017.5 after 10 s in survival |
| Sight | the skeleton, hidden from V on the road behind the barrier and the road's edge, **shot anyway**: 3 arrows in the first scene's world records, 4 in the second (V on the ledge, in view); `/kill` took the 4 left |
| Flyer | the bee flew toward V on the ledge and **stuck against the barrier**, at 1284.6 6.25 2007.65, where it still was 8 s after V went down onto the slope; `Test failed` |
| Clear | the last scene `world: nothing`; the dandelion taken from the off hand, the effects cleared, survival set back (the user's mode, from the scoreboard) |

So without the mod nothing walks, follows, teleports or chases on Night City's ground, a mob sees through it, and a
flyer flies into it: every check can tell.

## Step 0 with CyberCraft Tweaks 1.7.0: the first run

Date: 2026-10-08, 22:02. The same script and ground, CyberCraft Tweaks **1.7.0** installed (Minecraft stopped, 1.6.0
renamed `.jar.disabled`), `- cybercraft_tweaks 1.7.0` in Minecraft's log with no mixin error before or after the run.
`exit: 0`. Plan: Task 9.

### A stale save first

The control's session had ended with no "Saving" line: stopping Minecraft from the Modrinth App had killed it unsaved,
so this session loaded its last autosave, taken during the control's pet check (15:17:52 to 15:18:12). It held the
control's scoreboard objective (`An objective already exists by that name`), the player in creative (so the script
noted creative, and set creative at the end), wheat already in the off hand (so the script's guarded `item replace`
didn't run, and the tempt still had its wheat), and **the control's own test wolf**, tamed by its `Owner`, standing on
the city's ground. As the world loaded, before the link brought the ground, that wolf fell; 1.7.0 teleported it to
its owner onto the city's floor (the first minute line's `1 pets landed`) while it was falling, and it died of the
fall (`Wolf fell from a high place`, 22:02:53): a pet's landing keeps its fall distance. No other test entity was in
the autosave. From here on, Minecraft is saved (`/save-all flush`) before it's stopped.

### The checks

| Check | With 1.7.0 |
|---|---|
| Wander | the pig strolled 6 times and went 18.6 blocks, at y 6.0 to 6.25; the cow strolled once, 7.3 blocks, and then stood 55 s; the villager went 26.5 blocks, **up onto the barrier's top** (1283.1 7.5 1989.5) and **off its far side** to y 1.75, below the road, where it stayed |
| Tempt | `Test passed. Count: 1`, the cow at 1279.5 6.0 2016.1 beside V |
| Pet | `Test passed. Count: 1` after the walk (the wolf at 1276.2 6.0 2021.2, V at 2022.7); after the jump north, `Test failed`: the wolf at 1281.3 **3.795** 1997.6, in the air, under the road deck |
| Chase | `Test passed. Count: 1` |
| Sight | the first scene, V on the road: no arrow; the second, V on the ledge in view: an arrow. The minute lines count 41 and 20 sight casts hitting the city |
| Flyer | `Test passed. Count: 1`, the bee at 1289.5 −0.875 2007.1 on the slope, past the barrier and down the rock face |
| Cost | three minute lines: 74,993, 16,465 and 8,429 city answers; 2,948, 238 and 140 shapes classified; 19, 277 and 179 sight casts; 12 shortcut casts (the bee's, none hit); **0.6, 2.3 and 1.9 ms casting a minute** |
| Clear | `world: nothing`; the dandelion taken back; the game mode set to creative, as the stale save had it |

### Diagnostics

With the user's leave, four short `mclink` scripts (`build/step0/diag1` to `diag4`) watched pets and mobs every tenth
of a second to a second. Each moved the stand-in V before its `dump`, and a dump goes around V's feet as `dump` runs,
so each built **a shifted copy of the city** (diagnostic 2's by −4, −0.14, −7): their positions are read through that
shift.
- **Teleports** (diagnostic 1): three jumps of V, 22 blocks each way; the wolf landed on the road every time
  (1273.5 6.125 2000.5, 1279.5 6.125 2022.5, 1275.3 6.125 2002.6), on the ground, and stayed or strolled there. Step 0's
  one wolf under the road stays unexplained.
- **The barrier** (diagnostic 2): of two villagers and a pig, one villager climbed onto the barrier's top (its 7.375
  is the barrier's 7.5 through the shift) and fell off its far side to −0.4; the other, summoned onto the narrow ledge
  behind the barrier, walked off its edge down the rock face; the pig stayed on the road.
- **Kerbs and slopes** (diagnostics 3 and 4): cows tempted across a kerb and up the sloped pavement, sampled every
  tenth of a second (804 samples), never left the ground's surface.

### What it means

- **Thin tops are walkways.** The barrier, a skin 1/8 block thick and about 1.25 m tall, counts as a full block with
  a floor on top (its top part is even a floor cell, its lower half-block of skin), so mobs path onto it, along it and
  off its far side, and the narrow ledge behind it drops them further. The user chose to make such cells **fences**.
- **A pet's landing must reset its fall distance.**
- **The script summons inside a skin** where it gives the pavement's y 6.2 (its skin spans 6.125 to 6.25): such a mob
  falls through the skin it's inside. Summons go half a block above the floor, to drop onto it.
