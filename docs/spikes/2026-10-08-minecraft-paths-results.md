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
