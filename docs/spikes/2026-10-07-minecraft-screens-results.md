# Minecraft's screens in build mode: results

Spike 8 makes Minecraft's screens workable from Cyberpunk in build mode
([the spec](../superpowers/specs/2026-10-07-minecraft-screens-design.md)): the inventory, containers, chat, the pause
menu and the F3+F4 switcher, with a cursor and typed text. Step 0 checked Minecraft's side with `mclink`, without the
game; run E checked it all in the game, on BlockRender 0.23.0.

**The answer, from step 0 and run E: yes.** In build mode, Minecraft's screens are worked from Cyberpunk as in
Minecraft:
- **opening them:** E opens the inventory (the creative one in creative), T and Minecraft's command key open chat, O the
  pause menu, F3+F4 the switcher, and a right-click a crafting table or a chest;
- **the cursor:** an arrow drawn over Night City, moved at the user's Windows pointer speed, which "felt smooth". V
  doesn't turn while a screen is open;
- **items:** clicks, shift-clicks, drag-splitting, the number keys and Q work on Minecraft's slots;
- **typing:** Danish letters, AltGr's characters, a dead key's é, Backspace, the arrows and Ctrl+A, C and V;
- **F3:** its combinations reach Minecraft, the switcher and F3+N's spectator included;
- **closing them:** Esc or E close a screen without the game's menu opening, F8 closes one as it ends build mode, and
  Shift+F6 pauses with the screen kept.

Outside a screen, the game keeps its menu keys and F, R, Tab and F5.

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
- **Minecraft's log** had no command error. Its only two warnings came as the world opened, before any command:
  `Couldn't acquire next surface texture` and `Requested post effect does not exist: minecraft:end_of_frame`, from its
  hidden window.

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

## Run E: Minecraft's screens in the game

Date: 2026-10-07, 22:28 to 22:50. BlockRender 0.23.0 (the plan's Tasks 6 to 13, `fd97e0e`, 551 unit tests passing),
deployed with the game closed; Minecraft 26.3 with SkyCraft 0.1.2 and CyberCraft Tweaks 1.6.0 in the `CyberCraft`
instance, its window hidden; the game on the user's 2560 × 1440 screen, the HUD image 1280 × 720 at `hudscale 2`.
The user played; the agent sent requests and read the log, the status and Minecraft's log. Plan:
[2026-10-07-minecraft-screens.md](../superpowers/plans/2026-10-07-minecraft-screens.md), Task 14.

### Verdict

**Run E: pass,** every check. Risk 8 is settled: Esc, hidden from the game's Raw Input, closes a Minecraft screen
without opening the game's menu. The user found the cursor smooth. One step needed a second try: the first 5 of the slot
check was pressed after the inventory closed, as the status's last input showed, and the second, inside it, left the
slot as it was.

### The checks

- **Loading:** `BlockRender 0.23.0 loaded`, with `GetRawInputData hooked`, `PeekMessageW hooked: what's typed reaches
  Minecraft's screens` and `Windows' pointer speed is 8 of 20: Minecraft's cursor moves 0.750 back-buffer pixels a
  count`; no error in the log all run. After Link and F8, `screens=no Minecraft screen open; Windows speed 8 of 20:
  0.750 px a count; …; PeekMessageW hooked`.
- **Check 1, the inventory:**
  - E opened it with the arrow in the middle;
  - the arrow followed the mouse, the hover highlight followed the arrow, and V didn't turn;
  - the status said `a Minecraft screen open, the cursor at 795, 241 of 1280 x 720`, 4,886 cursor events sent by then;
  - Esc closed it, and the game's menu didn't open (risk 8).
- **Check 2, items in survival** (`Set own game mode to Survival Mode`): picking up and putting down, shift-click,
  drag-splitting the golden apples and gathering them back, a number key over a slot, and Q over an item all worked;
  the user picked the dropped item up again, and the `/kill` of items found none. The number keys, Q and E also make
  characters, which went to the inventory as text, which ignores them, as in Minecraft's own window.
- **Check 3, the creative inventory** (`Set own game mode to Creative Mode`): tabs, the wheel scrolling, and `stone`
  typed into the search box worked. With slot 3 set outside, 5 and the wheel inside the inventory left the readout at
  `slot 3`, the last input `Esc up`.
- **Check 4, chat:**
  - T opened Minecraft's chat, not the phone;
  - `<ItzBloxy> Hej æøå 'e @ ~` arrived with æ, ø, å, AltGr's @ and ~ whole. Its `'` is a plain apostrophe (U+0027), so
    the apostrophe key was pressed there, not the ´ dead key; tried afterwards, ´ then e arrived as `<ItzBloxy> é`;
  - Backspace, the arrows, Ctrl+A, Ctrl+C and Ctrl+V worked;
  - Minecraft's command key (`-` on the user's keyboard) opened chat with `/`, and `time query daytime` answered `Clock
    minecraft:overworld is at 6000 tick(s)`;
  - 175 characters typed in all by then, no lone surrogates.
- **Check 5, F3:** the debug screen toggled; the switcher cycled with F4 and set the mode as F3 was let go, twice each
  way (`Set own game mode to Survival Mode`, then `Creative Mode`); F3+N went into spectator and back; 6 keys taken by
  the F3 rule; V stayed where it stood.
- **Check 6, the pause menu:** O opened it, a slider in Options dragged with the arrow, and Esc closed it; build mode
  stayed on.
- **Check 7, a crafting table and a chest,** placed by request at -119 6 2007 and -120 6 2006:
  - both worked with the arrow;
  - F8 with the chest open logged `build mode ended with a Minecraft screen open: Esc closes it`, and the chest closed;
  - with build mode back on and the chest open, Shift+F6 logged `build mode paused: CET's overlay is open` and the arrow
    went; closing the overlay brought the chest back with the arrow where it was.
- **Check 8, Alt-Tab:** with the chest open, `the game lost the front in build mode: Minecraft lets go of everything`;
  back in the game, the chest still open and the arrow working; Esc closed it.
- **Check 9, the keys left to the game:** Esc with no screen open logged `build mode off: Esc left it`, and the game's
  menu opened.
- **The clear:** `Changed the block at -119, 6, 2007` and `-120, 6, 2006`, `Killed 2 entities` (the markers), and `No
  entity was found` for items. The chest's contents went with it.

### What else showed

- **The Windows key goes to Minecraft while a screen is open,** as every key does: the status's last input once read
  `the key of scan code 0x5b E0 down`, the key the user switched windows with. It's harmless: Minecraft lets go of
  everything as the game loses the front.
- **The typer's own chat** made a screen build mode's for 17 ms as `/gamemode creative` was typed, the spec's edge case.
- **Frames** averaged 15.4 ms at the run's end, linked, with the HUD drawn.
