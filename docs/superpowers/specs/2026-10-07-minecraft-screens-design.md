# Minecraft's screens in build mode: design (spike 8)

Date: 2026-10-07. Status: agreed with the user section by section in the brainstorm. "Found while writing", near the
end, lists what writing it out added to what was agreed. Then revised after a deep self-review at the user's request,
which checked each claim against its source and walked through step 0 and run E as they would run: "Changes from the
deep review", at the end, lists what changed and what was checked. The sections above it include those changes.

## Purpose

This is spike 8 of the project that recreates [SkyCraft](https://github.com/chasmlol/SkyCraft) (Minecraft in Skyrim)
for Cyberpunk 2077. Night City shows Minecraft's blocks (spike 3), its HUD (spike 4) and its entities (spike 7), and in
build mode (F8) the user plays as Minecraft's player (spike 5) and fights with Minecraft's weapons (spike 6). But none
of Minecraft's screens can be worked from the game:
- **the inventory** (E), and the creative inventory;
- **what a right-click opens:** chests, crafting tables, furnaces and every other container, signs, books;
- **chat and commands** (T and /);
- **the pause menu** and its options;
- **the game mode switcher** (F3+F4) and the other F3 combinations.

Build mode takes only a fixed set of inputs (the mouse buttons, the wheel, 1 to 9, the movement keys), mouse movement
always turns V, nothing draws a cursor, and Esc always leaves build mode. So a crafting table right-clicked in build
mode opens its screen in the HUD image, and nothing in it can be clicked. Items given past the hotbar are out of reach,
which is why spike 6's overlay gives its Fun items into chosen hotbar slots.

The user asked for Minecraft's inventory and the F3+F4 switcher during spike 6 (its results' "Asked for, after Part
3"), and for this spike: "Inventory, and being able to interact with UI". The question:

> Can Minecraft's screens be worked from Cyberpunk in build mode as they are in Minecraft: opened by Minecraft's keys,
> clicked and dragged with a cursor, typed into, and closed, without V turning or the game acting on the input?

## Decisions from the brainstorm

- **Scope: Minecraft's screens only.** The inventory, containers, chat, the pause menu and the switcher, worked from
  Cyberpunk in build mode. The user turned down Cyberpunk's own menus opening without ending build mode.
- **Keys: Minecraft's UI keys.** With no Minecraft screen open, build mode also takes E, T, /, Q and F3, and every key
  while F3 is held. The user chose this over SkyCraft's way (every key Minecraft's but a few kept for the game), which
  would take F (interact) and Tab (scanner) from the game, and over only E and F3+F4. With a screen open, every input
  is Minecraft's.
- **The pause menu on O, as SkyCraft does,** through SkyCraft's own `kInOpenMenu`. Esc with no screen open keeps ending
  build mode and opening the game's menu. The user chose this over Esc opening Minecraft's menu, and over no pause
  menu.
- **Spectator stays in.** F3+F4 and F3+N can put Minecraft's player in spectator, and V, who follows it in build mode,
  then goes through walls. The user chose to keep it over taking it out of the switcher (a CyberCraft Tweaks change)
  and over ending build mode in spectator.
- **Approach 1: BlockRender draws the cursor.** Mouse movement, hidden from the game, moves a cursor that BlockRender
  keeps, sends to Minecraft as `kInCursor`, and draws in the HUD pass at the newest position each frame. The user chose
  it over Minecraft drawing the cursor into the HUD image (approach 2: a Tweaks mixin; the cursor and a carried item
  move together, but both lag Minecraft's frame and the HUD's transfer), and over Windows' own cursor freed from the
  game's clip (approach 3: the game clips and re-centres it, and it could wander onto the user's second monitor,
  where a click takes the focus).
- **Then five sections, each approved:** where each input goes; the cursor; typed text; edge cases, status and
  versions; testing.

## Non-goals

- **Cyberpunk's menus in build mode.** The game's menu keys (Esc with no screen open, I, M, J, P, K, H) still end build
  mode, as since spike 4.
- **Minecraft's other keys with no screen open:** F (swap hands), L (advancements), Tab (player list), F1 (hide the
  HUD), F2 (screenshot), F5 (third person, which BlockRender doesn't draw) and F11 (full screen, on Minecraft's hidden
  window). They stay the game's, or reach Minecraft only with F3 held. The user's rebinds of the keys taken apply, since
  Minecraft gets physical keys and applies its own bindings, but a rebind onto another key isn't followed.
- **Minecraft's Friends screen,** on O in 26.3 (`key.friends`): O is the pause menu's, as in SkyCraft.
- **Protecting V in spectator,** by the user's choice.
- **Minecraft's cursor shapes:** the pointing hand over buttons and the text beam over text fields. The cursor is an
  arrow.
- **Mouse acceleration:** with Windows' "Enhance pointer precision" on, the cursor moves without its curve.
- **IME composition** (`WM_IME_CHAR`) and controllers.
- **Screens outside build mode:** Minecraft's title screen, or a screen opened while build mode is off, is worked in
  Minecraft's own window, as now.
- **The overlay's typed commands with a screen open:** they're refused while one is open, as now.

## Environment (checked on 2026-10-07)

### The game

- **Cyberpunk 2077 2.31** at `E:\Cyberpunk2077`, BlockRender 0.22.0 installed (spike 7), CET with its overlay.
- **Its back buffer** is 2560 × 1440 (spike 4's results); the HUD image is 1280 × 720 at `hudscale 2`, the default.
- **Its bindings** (`r6\config\inputUserMappings.xml`) for the keys this spike takes:
  - E: the iconic cyberware (`IconicCyberware_Button`), lean right, and the free camera's step up;
  - T: the phone (`PhoneInteract_Button`), notifications, cycling objectives;
  - Q: quick melee, dropping a carried body;
  - F3 and F4: the free camera (`ActivateFreeCam`, `SwitchFreeCamControl`);
  - O: only the free camera's field of view (`CameraIncreaseFOV`, in the "FREE CAMERA CUSTOMS" block);
  - /: nothing.
- **Its message pump** (from the executable's disassembly with `tools/plans/find_calls.py`): one loop at
  `0x1409f3230` calls `PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)`, then for every message but `WM_QUIT` calls
  `TranslateMessage` and `DispatchMessageW`. The only other `PeekMessageW` call (`0x14014acda`) passes
  `PM_NOYIELD | PM_QS_SENDMESSAGE`, so it handles only sent messages. So Windows makes `WM_CHAR` for every key the
  window gets, hidden from the game's Raw Input or not: hiding a key changes only the `RAWINPUT` the game reads.
- **Its cursor:** it imports `ClipCursor`, `SetCursorPos`, `SetCursor` and `GetCursorPos`, so it manages Windows'
  cursor itself.
- **Its Raw Input** (spike 4's "Cyberpunk's input"): one `GetRawInputData` call site, in the input system's
  `WM_INPUT` handler; mouse movement from `lLastX`/`lLastY`, buttons and the wheel from `usButtonFlags`, keys by
  `VKey` (0xFF skipped). It imports no other way to read keys or the mouse: no `GetKeyState`, `GetAsyncKeyState`,
  `GetKeyboardState`, `GetRawInputBuffer` or DirectInput; XInput is for controllers. The `WM_INPUT` handler runs from
  the pump's `DispatchMessageW`, so a key's Raw Input and its `WM_CHAR` are seen on the same thread.

### Windows

- **Pointer speed** (`SPI_GETMOUSESPEED`, the registry's `MouseSensitivity`): 8 of 20 on the user's machine, with
  "Enhance pointer precision" off (`MouseSpeed` 0). Windows' factors with it off, by speed 1 to 20 (Liquipedia's
  "Mouse Settings", seen through yuuza.net's translation): 1/32, 1/16, 1/8, 2/8, 3/8, 4/8, 5/8, 6/8, 7/8, 1, 1.25, 1.5,
  1.75, 2, 2.25, 2.5, 2.75, 3, 3.25, 3.5 pixels a count, the fractions carried over. So 0.75 for the user.
- **Display scaling** 100% (`AppliedDPI` 96), so Minecraft's window coordinates are its pixels.
- **Keyboard layouts:** Danish first (`00000406`), then US. On the Danish layout the key where US has "/" types "-", "/"
  is Shift+7, and AltGr makes @ (AltGr+2), £, $, €, the brackets and braces; ´, ¨, ^, ` and ~ are dead keys. AltGr
  reaches programs as Left Ctrl and Right Alt.

### SkyCraft's Minecraft side (0.1.2)

- **`InputBridge`** replays the input ring into Minecraft's own handlers, as if its hidden window had the focus:
  - `kInKey` → `KeyboardHandler.keyPress`, with a repeat (action -1) for a press of a key already down, and SDL's key
    code for the scancode;
  - `kInMouseButton` → `MouseHandler.onButton`; `kInScroll` → `onScroll`;
  - `kInCursor` → `MouseHandler.onMove(handle, a, b, dx, dy)`, in the window's coordinates;
  - `kInText` → `KeyboardHandler.textInput`, only while a screen is open;
  - `kInOpenMenu` → if no screen is open, release everything and `gui.setScreen(new PauseScreen(true))`;
  - `kInReleaseAll` lifts every key and button it holds.
- **`ScreenMixin`:** no screen pauses Minecraft while linked, so the world goes on behind an inventory.
- **`InputConstantsMixin`:** once SkyCraft has taken over Minecraft's window, the real mouse is never grabbed or
  released, and keys read as down come from the bridge's own table.
- **`kMcScreenOpen`** is set in McState while `gui.screen()` isn't null, and `guiScale` is the window's GUI scale.
- **The look:** each frame, only while no screen is open, it sets the player's yaw and pitch to the game's (SkyState).
  While a screen is open the player keeps its look.
- **Each frame, in order:** if SkyState says a menu is open (`kSkyMenuOpen`) or a load, `InputBridge.releaseAll`; then
  the input ring is drained; then the look. BlockRender and mclink set `kSkyMenuOpen` whenever build mode is off
  (`Link.cpp`; BlockRender's frame takes `Router::On()`, so not while paused). So with build mode off, nothing stays
  held from one frame to the next.
- **Its window** is sized to SkyState's viewport (`Window.setWindowed(w, h)`), the HUD image's size.
- **Its Skyrim side, as a reference:** while a Minecraft screen is open, every key is Minecraft's and the mouse moves a
  cursor (raw counts as overlay pixels), started at the centre when a screen opens; its overlay shader draws an
  18-pixel arrow, white with a black edge; O sends `kInOpenMenu`; characters from Skyrim's char events go as `kInText`.

### Minecraft 26.3

- **Default keys** (`Options`, SDL scancodes, so physical keys): inventory E (8), drop Q (20), chat T (23), command /
  (56), swap hands F (9), friends O (18), debug overlay and debug modifier F3 (60), switch game mode F4 (61) and
  spectate N (17) with the modifier, debug options F6 (63) with it. F4 alone toggles spectator shader effects; F1
  toggles the HUD.
- **The user's CyberCraft instance** (`ModrinthApp\profiles\CyberCraft\options.txt`) keeps those defaults for every key
  this spike takes. Its own changes: sneak on Left Ctrl and sprint on Left Shift (toggled), swap hands on F5, the
  perspective on F, hotbar 8 and 9 on mouse buttons 5 and 4; GUI scale 2.
- **`KeyboardHandler.keyPress`:** action 0 is a release, 1 a press, and anything else a repeat, so SkyCraft's -1
  repeats.
- **`Gui.setScreen`** (not `Minecraft.setScreen` in 26.3), given a screen, calls `MouseHandler.releaseMouse` and
  `KeyMapping.releaseAll`; given none, it stops text input and restores toggled keys. `releaseMouse`, if the mouse was
  grabbed, puts its position at the window's centre (`getScreenWidth() / 2`, `getScreenHeight() / 2`). Closing it
  (`grabMouse`) grabs the mouse again, centres the position too, and sets every key mapping from the keys held.
- **`MouseHandler.onMove`** only keeps the position and adds up the movement. Minecraft acts on it once a frame
  (`handleAccumulatedMovement`): with a screen open, `Screen.mouseMoved` at the frame's last position, and with a button
  held `mouseDragged` there too; with the mouse grabbed and no screen, `turnPlayer`. So a drag faster than a slot a
  frame skips slots in Minecraft's own window as well.
- **The switcher** (`GameModeSwitcherScreen`) cycles creative, survival, adventure and spectator
  (`GameModeIcon.getNext`).
- **The game rule `immediate_respawn`** is on in the user's world (spike 6), so there's no death screen.
- **`setblock … strict`** places with flags 2 | 816, which include `UPDATE_SUPPRESS_DROPS` and
  `UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS`: a chest removed so drops nothing, its contents included.

### BlockRender today (0.22.0)

- **`input/BuildMode`** decides each event: F8 toggles; Shift+F6 pauses while CET's overlay is open (spike 7); the menu
  keys leave; holders for 23 inputs (5 buttons, 1 to 9, 9 other keys) send a press's repeats and release where the press
  went; the hotbar slot is followed from 1 to 9 and the wheel.
- **`input/Router`** holds the lock around build mode, patches events in place (a hidden key's `VKey` becomes 0xFF, a
  hidden button's flag is cleared), pushes into the input ring, counts the mouse's x counts for `set turn`, and returns
  without the lock for an event of movement alone.
- **`input/RawInputHook`** patches the executable's `GetRawInputData` import (`hooks/ImportPatch`).
- **The HUD pass** (`Renderer::PresentHud`, `shaders/blocks.hlsl`'s `HudPS`) draws Minecraft's newest image over the
  finished frame at Present, `HudConstants` in root parameter 1, which has 8 values (an entity draw uses all 8).
- **`link/Commands`' typer** types the overlay's commands (T, the text as `kInText`, Enter), and refuses one while a
  Minecraft screen is open.
- **The link's status** already has `screenOpen` from McState.

## Where each input goes

Build mode gets a third state beside on and paused: **a Minecraft screen is open**. It comes from the link's
`kMcScreenOpen`, which the main thread passes to the router once a frame, and it counts only while build mode is on and
not paused.

### No Minecraft screen open

As now, plus:
- **E, T, / and Q** go to Minecraft and are hidden from the game: the inventory, chat, commands, and drop (Ctrl+Q drops
  the stack, Left Ctrl being Minecraft's already). T is no longer one of the keys that leave build mode; to reach the
  phone, leave with F8 first.
- **F3** goes to Minecraft, and **while F3's press is Minecraft's, every key pressed goes to Minecraft**, so that every
  F3 combination works: F3+F4 (the switcher), F3+N (spectator), F3+F6 (debug options), F3+Esc (pause without a menu)
  and the rest. F8 and Shift+F6 stay ours even then. A plain F4 or N still reaches the game.
- **O** sends `kInOpenMenu`, which opens Minecraft's pause menu, and is hidden from the game: one `kInOpenMenu` a
  press, its repeats and release hidden and sent nowhere.
- **Esc and the game's other menu keys** (I, M, J, P, K, H) end build mode and open the game's menu, as now.
- **F, R, Tab, F5** and every other key stay the game's.

### A Minecraft screen open

**Every key, button and wheel notch goes to Minecraft** and is hidden from the game, the menu keys and Esc included.
So typing works, Esc and E close screens, Shift and Ctrl modify clicks, Q drops what's under the cursor, 1 to 9 swap
with the hovered slot, and the user's F5 (swap hands) swaps it with the offhand. A click in a screen makes no latency
mark (`TakePress`), since it changes no block. Mouse movement moves the cursor ("The cursor") instead of turning V,
and characters go as text ("Typed text"). Two inputs stay ours:
- **F8** ends build mode. With a screen open it also closes it: after Minecraft lets go of everything, Esc's press and
  release go to Minecraft. Otherwise an inventory would stay open out of sight, and the overlay's Type and Fun buttons
  would refuse to run. The Esc goes even though SkyState then says a menu is open: SkyCraft lets go of everything at
  the frame's start, then drains the ring, Esc's press and release included.
- **Shift+F6** pauses build mode, as now. The screen stays open in Minecraft; when CET's overlay closes, the screen is
  worked again with the cursor where it was.

### Always

- **A press's repeats and its release go where the press went,** now for every key: holders by scan code and E0
  prefix, and the five buttons. So a key held as a screen opens or closes is let go on the side that saw its press.
- **The hotbar slot** isn't followed while a screen is open: there, 1 to 9 swap items and the wheel scrolls.
- **Leaving the game's window** keeps build mode and the screen, and Minecraft lets go of everything, as now.
- **Timing:** our side learns of a screen a frame or two after Minecraft opens it (about 30 ms). Until then, inputs go
  by the no-screen rules. That's also why the T or / that opens chat is never typed into it.

## The cursor

### Moving it

While a Minecraft screen is open (build mode on, not paused), the Raw Input patch zeroes each mouse event's movement
(`lLastX`, `lLastY`) so V doesn't turn, and the movement moves the cursor:
- **at Windows' pointer speed,** by Windows' table (0.75 back-buffer pixels a count for the user), read with
  `SystemParametersInfo(SPI_GETMOUSESPEED)` at load and again each time a screen opens. With "Enhance pointer
  precision" on, the table's factor is used without the curve, and the log says so;
- **kept in back-buffer pixels,** fractions carried over, clamped to the area the HUD image covers (its size times the
  scale it's drawn at);
- **sent to Minecraft as `kInCursor`** in the image's pixels (the back-buffer position over the scale, rounded down),
  which are Minecraft's window coordinates, from the input thread **whenever that pixel changes,** so the ring always
  ends with the newest position when Minecraft drains it. Minecraft acts on the frame's last position, as in its own
  window, so a drag splitting a stack covers the slots it would cover there;
- **an event with absolute movement** (`MOUSE_MOVE_ABSOLUTE`, as from remote desktops) moves nothing.

`TakeMouseX` gives 0 while a screen is open, as while paused, so `set turn` adds nothing to the teleports.

### Where it starts

At the centre of the image when Minecraft's screen flag goes from clear to set, whatever build mode's state: exactly
what Minecraft's own `releaseMouse` does with its position. Going from one screen straight to another (the pause menu
to Options) keeps it, as in Minecraft, and so does a pause behind CET's overlay. A changed `hudscale` or back buffer
clamps it into the new area. If one screen closes and another opens within a frame our side doesn't see (a gap of
one Minecraft frame, when Minecraft runs faster than the game), Minecraft centres its position and ours stays: the
arrow and Minecraft's hover disagree until the mouse moves, whose first event puts Minecraft's position at the arrow.

### Drawing it

The HUD pass draws SkyCraft's arrow (white, with a black edge) over Minecraft's image, at back-buffer resolution, 18
pixels tall at 1080 lines and scaled with the back buffer's height (24 on the user's 1440), its edge scaled alike.
`HudConstants` grows from 4 values to 8: the cursor's x and y in back-buffer pixels, its height, and whether it's
drawn. The pass reads the router's newest position as it's recorded at Present, from atomics the router publishes, so
Present never waits on the router's lock, and the arrow moves in the frame the mouse does. It's drawn while the
screen state is on and the HUD image is drawn.

Minecraft's hover highlight, and an item carried in an inventory, follow Minecraft's own position: they trail the arrow
by Minecraft's frame and the HUD image's transfer, about 2 to 3 frames, as the user accepted with approach 1.

## Typed text

- **From `WM_CHAR`:** Windows' characters for each key press, with the keyboard layout, Shift, Caps Lock, AltGr and dead
  keys applied, as Minecraft's own window (SDL) would get them. On the user's Danish layout that's æ, ø and å, "/" as
  Shift+7, @ as AltGr+2, é as ´ then e. AltGr's keys reach Minecraft too, as Left Ctrl, Right Alt and the key; none
  of the Danish AltGr characters sits on A, C, V or X, Minecraft's Ctrl shortcuts in text fields, so only the
  character shows.
- **Seen by patching the executable's `PeekMessageW` import,** as `GetRawInputData`'s is (`hooks/ImportPatch`): after a
  call with `PM_REMOVE` that returned a message, a `WM_CHAR` goes to the router. The other call site only peeks for sent
  messages, so it never returns one. The game gets every message unchanged; nothing of its own takes text in build
  mode.
- **Sent as `kInText` only while the screen state is on,** so the key that opened chat, and anything typed outside
  build mode or behind CET's overlay, never reaches Minecraft as text.
- **One code point each:** a UTF-16 surrogate pair is joined into one; a lone surrogate is dropped.
- **Control characters** (below 0x20, and 0x7F) are dropped. Minecraft gets Backspace, Enter, Tab, the arrows and
  Ctrl+A, C and V as keys, as SDL gives them; Ctrl+V reads the Windows clipboard on Minecraft's side.
- **A held key** repeats its key and its character, as SDL gives Minecraft.
- **The order:** both come through the game's one pump, on its thread. A key's `WM_INPUT` (the Raw Input patch, which
  pushes the key) is expected before `TranslateMessage` makes its `WM_CHAR` from the legacy `WM_KEYDOWN`, so Minecraft
  gets the key, then its character, as SDL orders them (risk 7: unverified, and harmless the other way round).
  `Link::Push` may be called from any thread.
- **If the patch fails,** the log says that typing in Minecraft's screens won't work; the rest does.

## Edge cases

- **Build mode ending any other way with a screen open** closes it as F8 does, with Esc to Minecraft, if Minecraft
  said last frame that it's in its world, its player alive, and a screen open. That's a scene or a car blocking V, a
  teleport of V failing, a `build off` request, or the plugin unloading. Not when Minecraft's player dies, whose
  respawn changes screens by itself, so an Esc could find none; nor when the link or Minecraft is gone, or Minecraft
  has left its world (its title screen ignores Esc anyway). Then nothing is sent, and the screen state goes off. The
  menu keys can't end build mode with a screen open, since they're Minecraft's then. All of it is in one place, the
  router's ending, which F8's decision goes through too.
- **Esc that finds no screen:** if the screen closed by itself in the frame build mode ended, the Esc opens Minecraft's
  pause menu instead. It stays out of sight until build mode is back, where Esc closes it; meanwhile the overlay's Type
  and Fun buttons are refused, as with any screen open.
- **The overlay's Type and Fun buttons** are refused while a Minecraft screen is open ("a Minecraft screen is open"),
  as now: with the inventory open behind CET's overlay, close it first.
- **The typer's own chat:** a command typed by an `mc` request while build mode is on and not paused (from mclink or
  the console) opens chat, so the screen state is on for a moment: the cursor shows, and anything typed then goes into
  that chat line. Rare, and left so.
- **`hud 0`** hides Minecraft's screens and the cursor, but the routing stays, so Esc still closes a screen out of
  sight.
- **Saving and quitting to Minecraft's title screen** ends build mode, as Minecraft leaving its world does now.
- **A full input ring** drops the event and counts it, as now.

## Status and log

- **A `screens=` status line,** after `build=`: whether a Minecraft screen is open and whose the cursor is, its position
  in the image's pixels (`cursor 640, 360 of 1280 × 720`), the pointer speed used (`Windows speed 8 of 20: 0.75 px a
  count`), cursor events sent, keys taken by the F3 rule, characters typed and dropped, and whether `PeekMessageW` is
  hooked.
- **The log:** a screen opening and closing (with build mode's state), the `PeekMessageW` patch, the pointer speed read,
  and the Esc that closes a screen as build mode ends.

## Versions and files

- **BlockRender 0.23.0:**
  - `input/BuildMode`: the screen state, holders for every key, the taken keys, the F3 rule, O, F8's Esc;
  - `input/Router`: the screen flag (read without the lock for movement), the cursor, text, the Esc on other endings;
  - `input/Cursor` (new, pure): Windows' speed table, the cursor's position, the image's pixels, the centring;
  - `input/Text` (new, pure): UTF-16 to code points, the control filter;
  - `input/MessageHook` (new): the `PeekMessageW` patch;
  - `render/Hud` (`CursorCovers`, the arrow's test, mirrored by `HudPS`), `render/FrameConstants` (`HudConstants`),
    `render/Renderer` (the cursor's constants at Present), `shaders/blocks.hlsl` (`HudPS` draws the arrow);
  - `Status`, `Plugin` (the hook, the pointer speed, the version).
- **mclink:** three verbs for checking Minecraft's side: `cursor <x> <y>` (`kInCursor`), `text <string>` (each
  character as `kInText`), `pause` (`kInOpenMenu`); and `tools/mclink/screens.txt`.
- **CyberCraft Tweaks and SkyCraft** stay as they are: SkyCraft's mod does all Minecraft needs.

## Testing

Each step checks against something real before the next one depends on it.

### Step 0: Minecraft's side, with mclink, before any game session

The user starts Minecraft; mclink takes the game's place on the link and runs `tools/mclink/screens.txt`, saving HUD
images. mclink always asks for a 1280 × 720 HUD. The script turns `build on`: with build mode off, SkyState says a menu
is open and SkyCraft lets go of every key each frame, so F3 couldn't stay held for the switcher, nor a button for a
drag. Before it runs, a `/data get entity @s Inventory` and the player's game mode, read from Minecraft's log, show
which main-inventory slots are empty and what to set back.
- **the cursor's pixels:** in survival, whose inventory has a fixed layout (the creative one has tabs), E opens the
  inventory. Its 176 by 166 GUI pixels are centred in the window at the user's GUI scale 2, which the script checks
  against McState's `guiScale`. The cursor goes to a slot's centre, and the image shows the slot's hover highlight
  under it;
- **moving an item:** a stick and a stone go into two of the empty slots with `/item replace`, so none of the user's
  items is replaced. They're moved by `cursor` and `click`, then Esc closes the inventory (the typer refuses while one
  is open), and `/execute if items …` reports in chat that they landed where intended;
- **typing:** T opens chat, then `text /say screens` and Enter; Minecraft's log has the line;
- **the switcher:** F3 down, F4, F3 up; the chat says the game mode changed, and a second round sets it back;
- **the pause menu:** `pause` opens it, Esc closes it;
- **the end:** the two test slots are emptied with `/item replace … with air`, the game mode read before is set back,
  `build off`, and the user's hotbar is as they keep it (golden apples in slot 6, golden carrots in 7, TNT in 8, flint
  and steel in 9).

### Unit tests (ctest, no game)

Each new test is broken once to prove it can fail.
- **Build mode:** routing with and without a screen; F8 and Shift+F6 kept ours, F8's Esc after the release; the F3 rule;
  O's one `kInOpenMenu`, its repeats and release sent nowhere; Esc both ways; the menu keys going to Minecraft with a
  screen open; holders for any key across a screen opening and closing; the slot readout kept, and no latency mark,
  while a screen is open.
- **The router's ending:** Esc only with Minecraft in its world, its player alive and a screen open.
- **The cursor:** Windows' table, 1 to 20; fractions carried; clamping; the image's pixels at scales 1, 2 and 4;
  centring when the flag sets, not between screens; sending only when the pixel changes.
- **Text:** surrogate pairs, lone surrogates, the control filter, the screen state's gate.
- **The arrow:** `CursorCovers`, the CPU mirror of `HudPS`'s arrow, as `HudTexel` pins the image's arithmetic.

### Run E, in the game

After spike 7's runs A to D. BlockRender 0.23.0 deployed while the game is closed, then with the user:
1. E opens the inventory with the cursor in the middle; moving the mouse doesn't turn V.
2. In survival (the overlay's Survival button): picking up and placing, shift-click, a drag splitting the golden
   apples (put back in slot 6 after), 1 to 9 over a slot, Q; E and Esc close it. What Q drops is picked up again, or
   cleared with `/kill @e[type=item,distance=..8]`.
3. In creative: the creative inventory's tabs, the wheel scrolling, typing in its search box.
4. Chat: T, typing capitals, æ, ø and å, @ (AltGr+2), é (´ then e), "/" (Shift+7), Backspace, the arrows, Ctrl+A,
   Ctrl+C and Ctrl+V. Minecraft's command key, the physical key where US has "/" ("-" on the user's Danish keyboard),
   opens chat with the slash; a command runs.
5. F3 toggles the debug screen; F3+F4 cycles the modes; F3+N into spectator briefly, then back.
6. O opens the pause menu; an Options slider drags.
7. A crafting table and a chest placed in the city and worked; F8 with one open closes it and ends build mode; Shift+F6
   with one open pauses, and the cursor is where it was after.
8. Alt-Tab with a screen open, and back.
9. T no longer opens the phone in build mode; Esc with no screen open still opens the game's menu.

Only test items go in the chest: the placed blocks are cleared at the end with `setblock … air strict`, which drops
nothing, the chest's contents included. The results doc gets screenshots, as earlier runs' did.

## Risks

1. **`WM_CHAR` for hidden keys:** expected from the pump's disassembly and from Raw Input registered without
   `RIDEV_NOLEGACY` (spike 4), which keeps the legacy key messages coming; run E's typing shows it.
2. **The cursor's pixels:** if Minecraft's window coordinates weren't the image's pixels, clicks would land off the
   arrow. Step 0 checks them before the game; display scaling is 100%.
3. **The 30 ms after a screen closes,** before our side learns of it: the mouse's movement is still hidden from the
   game, so V doesn't turn, and goes to Minecraft as `kInCursor`, which, the mouse grabbed again, turns Minecraft's
   player. SkyCraft sets the player's look from the game's again the next frame, so it's at most a one-frame twitch of
   Minecraft's hand and a few degrees of V's turn lost.
4. **F8's Esc and the typer:** an Esc sent while the typer's chat is open closes it before its Enter, and the command
   is lost while the typer logs it typed. Rare: the typer types while CET's overlay is open.
5. **Spectator** takes V through walls, and V could be left inside the city's geometry when it ends. The user chose to
   keep it.
6. **Many cursor events:** a fast mouse at 1,000 events a second sends one each time the image's pixel changes, about
   17 a frame at 60 frames a second; the input ring holds 4,096, and Minecraft drains it every frame.
7. **A key's `WM_INPUT` before its `WM_CHAR`** is expected (the raw event comes with the key, `WM_CHAR` only from
   `TranslateMessage` of the legacy `WM_KEYDOWN`), not verified. The other order would do no harm: a text field types
   on characters and ignores printable keys, a screen without one ignores characters, and SkyCraft drops a character
   when no screen is open.

## Found while writing

- **O is the free camera's key, not photo mode's,** as the brainstorm said: `CameraIncreaseFOV` sits in the
  mappings' "FREE CAMERA CUSTOMS" block. It changes nothing: O is taken in build mode only.
- **F4 alone is bound in 26.3** (spectator shader effects) and stays the game's, as agreed: it matters only in
  spectator, spectating a creeper, an enderman or a spider.
- **26.3's screen changes go through `Gui.setScreen`,** which releases the mouse and every key mapping, and centres the
  mouse's position: the cursor's centring copies it.
- **SkyCraft sets the player's look only while no screen is open,** so Minecraft's player keeps its look behind a
  screen, and V's, which doesn't turn then, matches it.
- **`HudConstants` fits:** root parameter 1 has 8 values, which an entity draw already uses.
- **Every way build mode can end with a screen open closes it,** not only F8 (the brainstorm's section 4 agreed this).
- **A drag doesn't see every slot in Minecraft either.** The brainstorm's section 2 said sending the cursor at each
  pixel change lets a drag pass over every slot. In 26.3, `MouseHandler` acts on movement once a frame, at the frame's
  last position, so a fast drag skips slots in Minecraft's own window too. The sending stays as agreed, for the newest
  position; only its reason changed.
- **T's other bindings** are notifications and cycling objectives, not photo mode.

## Changes from the deep review

The user asked for a deep self-review of the spec once it was written. It traced each claim to its source (the game's
executable and bindings, Minecraft 26.3's classes with `javap`, SkyCraft 0.1.2's source, BlockRender's and mclink's
code, the user's Windows and Minecraft settings), and walked through step 0's script and run E's nine steps under the
routing rules.

### What changed

- **The ending's Esc is narrower:** it goes only while Minecraft is in its world, its player alive, and a screen open.
  A death changes screens by itself (respawn), so an Esc then could open the pause menu out of sight; out of its world
  there's nothing to close. It lives in the router's one ending, which F8 goes through too ("Edge cases").
- **O's release:** one `kInOpenMenu` a press, its repeats and release hidden and sent nowhere. The spec said only that
  O is hidden.
- **No latency mark from a click in a screen.** A mark times the next block change within a second, and a click in an
  inventory changes none, so an unrelated change (a furnace lighting) would have made a false sample.
- **Step 0 runs with `build on`.** SkyCraft lets go of every key at each frame's start while SkyState says a menu is
  open, which mclink and BlockRender say whenever build mode is off. F3 held for the switcher, or a button held for a
  drag, would be let go between frames.
- **Step 0's inventory test** uses survival's inventory, whose layout is fixed, at the user's GUI scale 2 (checked
  against McState). Its test items go only into slots found empty first, and the game mode read first is set back. The
  first version would have used `/item replace` on slots that might hold the user's items.
- **Run E** switches to survival for the item moves, and types the user's Danish characters (æ, ø, å, AltGr's @, the
  dead key's é, "/" as Shift+7). It names Minecraft's command key as the user's "-" key, and clears what Q drops. Only
  test items go in the chest, whose removal with `strict` drops nothing.
- **The renderer reads the cursor from atomics,** so Present never waits on the router's lock; the arrow's edge scales
  with it.
- **Two edge cases added:** a screen replacing another within a frame our side misses (the arrow and Minecraft's hover
  disagree until the mouse moves), and the 30 ms after a screen closes (risk 3, extended).
- **Risk 7 added:** the order of a key's `WM_INPUT` and its `WM_CHAR` is expected, not verified, and harmless either
  way.
- **Non-goal added:** Minecraft's Friends screen, on O in 26.3.

### What was checked

- **The game:**
  - its imports: input is read only through `GetRawInputData`, with XInput for controllers;
  - its pump's disassembly, with the other `PeekMessageW` call (sent messages only);
  - its bindings for E, T, Q, F3, F4 and O (`CameraIncreaseFOV` in the free camera's block);
  - that the `WM_INPUT` handler runs from the pump's dispatch.
- **Minecraft 26.3:**
  - every default key this spike uses, and `key.friends` on O;
  - `Gui.setScreen`'s two branches;
  - `MouseHandler.releaseMouse`, `grabMouse` and `handleAccumulatedMovement`;
  - `KeyboardHandler.keyPress`'s actions;
  - `GameModeIcon.getNext`;
  - `SetBlockCommand`'s strict flags against `Block`'s `UPDATE_*` constants.
- **SkyCraft:**
  - `InputBridge`'s handling of each input type;
  - `SkyClient`'s frame order (release while a menu is open, drain, look only without a screen) and its viewport;
  - `ScreenMixin` and `InputConstantsMixin`.
- **The user's settings:**
  - the CyberCraft instance's `options.txt`: every key this spike takes at Minecraft's default, sneak and sprint
    swapped, swap hands on F5, the perspective on F, GUI scale 2;
  - Windows: pointer speed 8 with "Enhance pointer precision" off, 96 DPI, Danish then US layouts;
  - Windows' pointer speed table, from Liquipedia's figures.
- **BlockRender and mclink:**
  - every way build mode ends (`Router::Update`, the drive's `TurnOff`s, `build off`, unload, F8);
  - `frame.buildMode` taking `Router::On()`;
  - the latency marks;
  - root parameter 1's 8 values;
  - the typer's refusal;
  - mclink's `build` and `hud` verbs and its 1280 × 720 viewport.
- **Simulated:**
  - step 0's script, step by step, which found the `build on` and empty-slot changes;
  - run E's steps under the routing rules, among them E's character dropped by the gate, the inventory closed by E or
    Esc, the F3+F4 switcher closing as F3 is let go, O's pause menu, F8's Esc, Shift+F6 and Alt-Tab with a screen
    open, and Danish typing through AltGr and dead keys.
