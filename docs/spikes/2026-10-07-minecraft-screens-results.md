# Minecraft's screens in build mode: results

Spike 8 makes Minecraft's screens workable from Cyberpunk in build mode
([the spec](../superpowers/specs/2026-10-07-minecraft-screens-design.md)): the inventory, containers, chat, the pause
menu and the F3+F4 switcher, with a cursor and typed text. Step 0 checked Minecraft's side with `mclink`, without the
game; run E adds its section once BlockRender 0.23.0 is in, and the spike's answer goes at the top then.

## Step 0: Minecraft's side of its screens, with `mclink`, without the game

Date: 2026-10-07. No Cyberpunk: `mclink` sent what BlockRender will send (a cursor in the HUD image's pixels, clicks,
keys, typed text and the pause menu's `kInOpenMenu`) to the user's Minecraft with SkyCraft's unchanged mod. The
versions:
- **Minecraft 26.3**, Fabric Loader 0.19.5, in the Modrinth App's `CyberCraft` instance, with CyberCraft Tweaks 1.6.0
  and the user's GUI scale 2;
- **SkyCraft 0.1.2**, the release jar, protocol version 11;
- **BlockRender's link code and `mclink`** at `1d946dc`: the plan's Tasks 1 to 3, built RelWithDebInfo, 526 unit tests
  passing.

The user started and stopped Minecraft. The agent ran `mclink` with step 0's script, `screens.txt`, read its output
and Minecraft's log, and saved five HUD images, here as PNG in
[2026-10-07-minecraft-screens-results/](2026-10-07-minecraft-screens-results/). Minecraft opened its world, SkyCraft's
own `SkyCraft`, by itself as the link came up, and the run took 19 s. Plan:
[2026-10-07-minecraft-screens.md](../superpowers/plans/2026-10-07-minecraft-screens.md), Task 4.

### Verdict

**Step 0: pass.** SkyCraft's mod works Minecraft's screens from the inputs BlockRender will send, as the spec has it:
- the cursor's pixels are Minecraft's window coordinates, to the pixel;
- clicks land where the cursor is, and move items between a chest's slots;
- typed text arrives whole, Danish letters and AltGr's characters included;
- F3+F4 switches the game mode;
- `kInOpenMenu` opens the pause menu;
- E and Esc open and close screens.

Every check passed, and the end left nothing: the chest went with what was in it, nothing dropped, and the last
scene's world line said `nothing`. One expectation was wrong: the player in this instance is `ItzBloxy`, not
`SirBloxy`, whom the plan took from older logs.

### The checks

- **Linking:**
  - `Minecraft is there` at 0.27 s;
  - `clear all` and the atlas (2048 × 2576, 20.1 MB) at 2.1 s;
  - HUD images of 1280 × 720, rows bottom-up, at 2.24 s;
  - the city around region 110 0 250 all in the ring at 4.58 s (1,152 regions).
- **The chest,** placed at 882 7 2007 with a stick in slot 0 and a stone in slot 1 (`Changed the block`, two
  `Replaced 1 slot(s)` in the chat):
  - the right-click hit it where the plan's look aimed, at height 7.51 (SkyCraft: `click 3 -> BLOCK (882.0625,
    7.512585610790914, 2007.5) (grabbed true, screen null)`);
  - its screen was open 0.10 s later;
  - the status said `in its world, a screen open, GUI scale 2`, Minecraft's player at 880.500 6.140 2007.500;
  - its HUD image had all 921,600 pixels not transparent, and none with a colour above its alpha
    ([chest.png](2026-10-07-minecraft-screens-results/chest.png)).
- **The hover highlight,** with the cursor on empty slot 9 (window pixel 496 280):
  - slot 9's box: a mean colour of exactly 192.0, alpha 255.0;
  - slot 10's beside it: exactly 139.0, the slot's grey;
  - each over 576 pixels.

  The plan's arithmetic (the slot's 139 under white at alpha 96, then 32) made 192: the pixel mapping and the
  highlight are as worked out from 26.3's classes.
- **The moves:**
  - SkyCraft's four `click 1` lines, each `grabbed false` with the chest's `ContainerScreen`;
  - the image after them shows the stick in slot 4 and the stone in slot 5, with the stone's tooltip at the cursor
    ([moved.png](2026-10-07-minecraft-screens-results/moved.png));
  - Esc closed the chest at once;
  - the chat said `step0: the stick is in slot 4`, `step0: the stone is in slot 5` and `step0: slots 0 and 1 are
    empty`.
- **Typing:**
  - T opened chat within a frame;
  - `text: 22 characters typed`;
  - Enter sent it and closed chat 0.02 s later;
  - the chat line is `<ItzBloxy> step0: typed æøå é @ ~`, every letter whole.
- **The switcher:**
  - F3 down, F4 opened it in 0.02 s, on Survival Mode, the player's last game mode
    ([switcher.png](2026-10-07-minecraft-screens-results/switcher.png)), and letting go of F3 closed it with `Set own
    game mode to Survival Mode`;
  - the second round set `Creative Mode`, the player's mode before;
  - its image has only 177,983 pixels not transparent: the switcher draws no background.
- **The pause menu:**
  - `pause` opened it in 0.02 s, with SkyCraft's own `Skyrim destruction` button
    ([pause.png](2026-10-07-minecraft-screens-results/pause.png));
  - Esc closed it.
- **The inventory:**
  - E opened it in 0.08 s: the creative inventory, the player being in creative
    ([inventory.png](2026-10-07-minecraft-screens-results/inventory.png));
  - Esc closed it.
- **The end:**
  - `Changed the block at 882, 7, 2007` as the chest went;
  - `No entity was found` for the `/kill` of dropped items;
  - the last scene had 0 batches, and its world line said `nothing`;
  - the user's hotbar is as before (golden apples in slot 6, golden carrots in 7, TNT in 8, flint and steel in 9,
    as the images show);
  - `exit: 0`.
- **Minecraft's log** had no command error. Its only two warnings came as the world opened, before any command: `Couldn't
  acquire next surface texture` and `Requested post effect does not exist: minecraft:end_of_frame`, from its hidden
  window.

### What it means for Part 2

- **The cursor needs nothing more than the spec has:** BlockRender's position in back-buffer pixels, over the HUD's
  scale, is the pixel Minecraft uses, and Minecraft draws its hover highlight there.
- **Screens open 0.00 to 0.10 s after their key or click,** as McState reports them: chat and the switcher within a
  frame or two, the inventory 0.08 s (E is handled in Minecraft's tick), and a chest 0.10 s (its tick and the
  server's). Until BlockRender sees the flag, its inputs follow the no-screen rules, as the spec has it.
- **Screens look different behind them.** A container screen darkens the whole window with a gradient, the pause menu
  blurs the HUD image and lays Minecraft's menu background over all of it, and the switcher draws nothing behind
  itself. In the game each will show over Night City as it does over Minecraft's world.
- **Typed text is code points:** whatever reaches `kInText` arrives whole, so Part 2's `WM_CHAR` path only has to
  deliver the right characters.
