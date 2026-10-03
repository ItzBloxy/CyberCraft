# Collision and movement: results

Spike 5 makes Minecraft's player drive V in build mode, gives Minecraft Night City's shape, and stops V against
Minecraft's blocks. Step 1 ran the drive with `mclink`, without Cyberpunk. Later steps add their sections as they run.

## Step 1: the drive with `mclink`, without Cyberpunk

Date: 2026-10-03. No Cyberpunk: `mclink`, BlockRender's link code in a console program, ran against the user's
Minecraft with SkyCraft's unchanged mod. The versions:
- **Minecraft 26.3**, Fabric Loader 0.19.5, Fabric API 0.161.0+26.3 and Java 25, in the Modrinth App's `CyberCraft`
  instance, with `--enable-native-access=ALL-UNNAMED -Dskycraft.quitWithSkyrim=false`;
- **SkyCraft 0.1.2**, the release jar, protocol version 11;
- **BlockRender's link code and `mclink`** at `3dfcc71`: the plan's Tasks 1 to 7, built RelWithDebInfo, 270 unit tests
  passing.

The user started and stopped Minecraft. The agent ran `mclink` with step 1's scripts, `drive.txt` and `relink.txt`, and
read its output, Minecraft's log and the saved HUD images. Plan:
[2026-10-03-collision-and-movement.md](../superpowers/plans/2026-10-03-collision-and-movement.md), Task 8.

### Verdict

**Step 1: pass.** The drive works against SkyCraft's real mod as written: nothing in the code changed after running it.
Every check passed on its numbers. Two of the plan's expectations were worded too tightly, and both are explained below:
the gold block, which the script jumped onto and off again, and V's feet trailing Minecraft's while moving, which is
the render delay at work.

### The checks

- **Linking:** `Minecraft is there` 0.23 s after `mclink` started, and the floor (epoch 1) at 2.14 s, with its voxels.
  Outside build mode Minecraft let its player go after the link's teleport: by the first `track` line, 5.2 s in, the
  acknowledgement had caught up with the sequence (476360026 of 476360026).
- **Entering:** `build on` sent one teleport (476360027) and a fresh floor at the stand-in V's feet (epoch 2), and the
  drive waited. `Minecraft let its player go after 0.07 s: V follows it`, and the drive was driving.
- **The stand-in V follows:** standing still, V's feet matched Minecraft's to the millimetre in every `track` line. While
  moving, they trail Minecraft's newest position by about 11 to 14 ms of motion, which is the interpolator rendering in
  the past, as designed:

  | Moving | Speed (blocks a second) | V's feet behind Minecraft's |
  |---|---|---|
  | Walking | 4.31 | 4.7 to 5.6 cm |
  | Sprinting | 5.61 | 6.2 to 7.1 cm |
  | Flying up | about 7.4 | 8.8 to 9.4 cm |
  | Falling | faster | up to 12.8 cm |

  The gap closes once the player stops: a quarter of a second after the sprint ended, they matched again.
- **Walking and sprinting:** with W held, 4.31 blocks a second north (Minecraft's walking speed is 4.317); after Left
  Shift toggled sprint, 5.61 (5.612), with `sprinting` in the flags.
- **Sneaking:** with Left Ctrl held, `eye 1.27 up` and `sneaking`, and the stand-in V's feet at 5.790, 0.35 below
  Minecraft's 6.140, so its camera stayed on Minecraft's eye. Releasing it brought the eye back to 1.62 within half a
  second.
- **The wall:** `/fill` put stone bricks 3 blocks north of the player's block, z 1984 to 1985. Walking north stopped at
  z 1985.300, the bricks' south face plus the player's half-width, 1.85 blocks on, and stayed there.
- **The gold block:** at x −117 to −116, the player's block row. With W and Space held for 1.5 s the player jumped onto
  it and off again, so no `track` sample caught it standing at 7.00 as the plan expected. One of its jumps reached at
  least 8.08, though, and only a jump from the block's top gets that high: from the floor at 6.14 a jump peaks at 7.39.
  It then landed on the floor beyond, at x −114.3.
- **Flying:** Space twice gave `flying`. Space held raised the player at about 7.4 blocks a second, Left Ctrl held
  lowered it as fast (the flags read `sneaking, flying`), and Space twice again dropped it back onto the floor,
  `on ground` at 6.140.
- **The interpolator:** the render delay started at 10 ms and, with every tick on time, shrank by 2 ms a second to its
  4 ms floor, which it reached 0.7 s into build mode, and stayed there. No frame waited for a late tick, and no target was skipped. Minecraft ran at 60 to 64
  frames a second, paced to `mclink`.
- **Clicks while moving:** twenty right clicks, 0.45 s apart, while strafing east looking north and down. The `/fill`
  that removed the stone said `Successfully filled 20 block(s)` (`count.png`): all twenty were placed. Minecraft's chat
  showed nothing after the clicks (`clicks.png`): none was refused. Build mode sends no teleports, which is what made
  Minecraft refuse clicks in spike 4.
- **Leaving:** `build off` gave `build mode ended: Minecraft's player follows V again`, the drive went off, and the
  link's teleport went at once (476360028, applied within a quarter of a second). The run made 3 teleports: the link's,
  build mode's, and leaving's.
- **The clock-started `teleportSeq`:** with Minecraft still running, two `relink.txt` runs 10 s apart started from
  1430752721 and 1722695733, and each run's first teleport was applied (…722, then …734). Minecraft's log has
  `Skyrim instance changed (pid …)`, `Skyrim link up` and `teleported to -119.5 6.14 2007.5` for each. Minecraft still
  held the drive run's last value, 476360028, when the first relink started, as the spec found.
- **No `build mode ends:` line,** and `exit: 0` for all three runs.

The render ring carried 31,271 atlas patches (290 MB, about 7.7 MB a second: Minecraft's animated textures), 31
sections and 2,231 HUD images (60 a second), with no resyncs and nothing rejected.

### What it means for step 2

- **Minecraft lets its player go at once:** within 0.07 s of build mode's teleport, on the floor's voxels. The drive's
  waiting state should pass in a frame or two in the game too.
- **V trails Minecraft's newest position by about 11 ms of motion,** the interpolator's render delay. In the game,
  V's camera goes to the interpolated eye, so the status's camera-to-eye distance shows the teleport's own lag on top.
  Step 2's drive log measures both.
- **No refused clicks while moving:** build mode ends spike 4's refusals, since Minecraft's player moves by itself.
- **The script's keys stayed held through the jump,** so step 2's own check of jumping onto a block, where the user
  stops on it, settles what step 1 showed only by the jump's height.
