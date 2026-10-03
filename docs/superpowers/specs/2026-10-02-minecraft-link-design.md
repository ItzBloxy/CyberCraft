# Minecraft link: design (spike 4)

Date: 2026-10-02. Status: agreed with the user section by section in the brainstorm, after a scope pass the user asked
for, with concrete evidence for each part. Then revised after a deep self-review, which checked its claims against
SkyCraft's source, the user's Minecraft 26.3 jar, Cyberpunk's files and BlockRender's code. "Changes from the deep
review", at the end, lists what changed. Then the user asked why not use the HUD image Minecraft reads back anyway, and
it was added. "Added after the review: Minecraft's HUD", at the very end, lists what that changed.

## Purpose

This is spike 4 of the project that recreates [SkyCraft](https://github.com/chasmlol/SkyCraft) (Minecraft in Skyrim)
for Cyberpunk 2077. In SkyCraft, Minecraft owns the world and the player, and the game only shows the result. Spike 1
read the game's collision, and spike 3 drew Minecraft's blocks into Night City's G-buffer from the plugin's own test
structures. Spike 4 connects the real Minecraft:

> Can Night City show Minecraft's live world, streamed by SkyCraft's unchanged Fabric mod over shared memory, so that
> blocks placed, broken or blown up appear a frame or two after Minecraft changes them? With the building done from
> inside Cyberpunk, and both games at acceptable frame rates?

"After Minecraft changes them" matters. Minecraft acts on a click only in its next 20 Hz client tick (`startAttack`
and `startUseItem` run from `handleKeybinds`, inside `tick()`), and on a typed command a tick or two later still. So
latency is measured in two halves: Minecraft's own, and ours (see "Measurements").

Minecraft's HUD (the hotbar, the held item, chat) is drawn over Night City as well, from the image the mod reads back
every frame anyway.

It answers that in three steps, each tested against something real (see "The steps"):
1. BlockRender's link code, run as a console stand-in, against the real Minecraft, without Cyberpunk.
2. In the game: the link, the blocks and the HUD, with Minecraft driven by commands typed from the overlay.
3. In the game: input. In build mode, clicks place and break blocks where V looks, and Cyberpunk doesn't react to
   them.

## Decisions from the brainstorm

- **SkyCraft's mod, unchanged.** The user chose the released `skycraft-fabric-0.1.2.jar` over a trimmed fork or a mod
  of our own. Cyberpunk's side therefore plays Skyrim's part of the protocol. Once linked, the mod takes Minecraft
  over (see "SkyCraft's mod, once linked"), so the building is done from inside Cyberpunk, not in Minecraft's window.
- **Scope: parts 0 to 4 and 7 of the scope pass:** running Minecraft, the link, the look, blocks, input, and
  Minecraft's HUD. The user picked parts 0 to 4 after the scope pass. Part 7 was added after the deep review, when the
  user asked why not use the HUD image Minecraft reads back anyway. Most of its plumbing turned out to exist already in
  spike 3's `present` path. Puppeting and Night City's real collision come later.
- **V walks, and Minecraft follows.** Until puppeting, Cyberpunk keeps movement and mouse look. BlockRender keeps
  Minecraft's player pinned at V's camera with teleports. A flat floor of collision triangles at V's feet gives clicks
  something to hit, since SkyCraft's world is an empty void.
- **Build mode on F8.** In build mode the mouse buttons, the wheel and the keys 1 to 9 go only to Minecraft, so V
  doesn't shoot, aim or switch weapons. Everything else stays Cyberpunk's. Outside build mode nothing is forwarded.
- **A Modrinth instance.** The user runs Minecraft from their own Modrinth App, with their own account, rather than
  SkyCraft's Prism bundle or a build from source. It runs with `-Dskycraft.quitWithSkyrim=false`. Otherwise
  Minecraft saves and quits 5 s after the game side's process ends: when `mclink` stops, and whenever Cyberpunk closes.
- **Minecraft's HUD drawn over Night City, and its window hidden.**
  - **The first plan:** both games share the main monitor (Cyberpunk borderless fullscreen), and the second monitor
    shows Claude Code. So the user would keep Minecraft's window minimized, and bring it up to see the hotbar or chat.
  - **What changed it:** with the HUD drawn in the game, the window has nothing left to show. It stays hidden,
    SkyCraft's own default, which also avoids an untested path: a minimized window that SkyCraft tells Minecraft isn't
    minimized (see "Minecraft").
- **Approach A: BlockRender grows two modules.** `link/` and `input/` go into BlockRender (0.9.0), which already has
  the renderer, import patching, requests, the status file and the overlay. A second plugin would need a cross-DLL
  interface for the renderer's sections and atlas. The link code also builds into `mclink.exe`, a console stand-in
  for Cyberpunk, so the protocol is tested against the real Minecraft before the game.

## The scope pass

The user asked how hard each part of Skyrim's role would be, with evidence. The evidence came from SkyCraft at
`bfcaf17` (0.1.2, 2026-10-01), Cyberpunk's executable and address file, the RED4ext SDK, BlockRender 0.8.0, FreeFly,
and the user's 26.3 jar. SkyCraft's Skyrim side is 11,850 lines of C++.

| # | Part | Rating | Rough size |
|---|---|---|---|
| 0 | Running Minecraft with the mod | Low | No code |
| 1 | The link | Low | ~250 lines |
| 2 | The look | Low | ~40 lines |
| 3 | Blocks, plus typed commands | Low to medium | ~400, plus ~200 |
| 4 | Input | Medium | ~350 |
| 5 | Puppeting | Medium to high | ~400 |
| 6 | Collision: a stand-in floor / Night City's | Low / high | ~150 / 1,500+ |
| 7 | The HUD | Medium | ~400 |

0. **Running Minecraft.** The release has the jar. The README's "your own launcher" path needs Minecraft 26.3, Fabric
   Loader 0.19.5 or newer, Fabric API and Java 25.
1. **The link.** SkyCraft's `skse/src/Link.cpp` (363 lines) creates, resets and heartbeats the mapping, and writes the
   seqlocked `SkyState`. BlockRender's `OnUpdate` already runs once a frame on the main thread. "In game" has no
   reliable signal: spike 2 found a stand-in V at the startup screen. The address file names
   `cp::PlayerSystem::OnPlayerMainObjectSpawned`, but an explicit trigger is safer. Minecraft busy-waits up to 25 ms a
   frame for `SkyState` to change (`SkyClient.paceFrame`), so one CPU core spins.
2. **The look.** `SkyClient.beginFrame` sets the player's yaw and pitch from `SkyState` every frame, unless a
   Minecraft screen is open. BlockRender's `PlayerLocator` already reads the camera's forward vector.
3. **Blocks.** `RenVertex` matches BlockRender's `Vertex` byte for byte, so sections go straight to
   `Renderer::UploadSection`. The jar has 1,300 block textures (105 with animation files) and 831 item textures, so the combined
   atlas is about 6 to 13 MB, under the ring's 32 MB message limit. Minecraft sends only the atlas's full-size level.
   If the ring fills, `SkyLink.writeRender` waits up to a second per message on Minecraft's render thread, so the ring
   must always be drained. Bursts are unmeasured: `Renderer::Update` makes two committed buffers per section on the
   main thread, and nobody timed spike 3's 81-section field.
   - **Typed commands:**
     - `MirrorWorld` creates its world with `allowCommands` on (the jar's `LevelSettings` constructor);
     - T opens chat on the next client tick (`Gui.handleKeybinds`);
     - `InputBridge` takes text only while a screen is open.
4. **Input.** Cyberpunk's executable reads the keyboard and mouse only through Raw Input. It imports
   `RegisterRawInputDevices`, `GetRawInputData`, `PeekMessageW`, `ClipCursor` and `SetCursorPos`, with XInput for
   pads. It imports no DirectInput and no key-state functions. So one import-table hook on `GetRawInputData` sees every
   key and click the game reads, and BlockRender's `ImportPatch` already patches the executable's imports. SkyCraft's `Input.cpp` maps
   set-1 scan codes to SDL scancodes. Unproven: that rewriting the returned event stops Cyberpunk acting on it.
5. **Puppeting.** FreeFly, which is installed, teleports V every frame from Lua with the `NoMovement` status effect. It
   adds the mouse's yaw back in every frame, so a per-frame teleport overrides V's turning; pitch is untouched.
   SkyCraft's puppet loop interpolates Minecraft's 20 Hz ticks with an adaptive delay (about 130 lines of
   `Game.cpp`). Smoothness is spike 2's open question.
6. **Collision.** `tools/fake_skyrim.py` builds a stand-in floor in about 40 lines. Minecraft releases its held player
   once the regions at and below the feet are known (`SkyCollision.isKnown`, from voxel messages only) and there's
   ground within 12 blocks. Night City's real collision builds on spike 1's PhysX reading. SkyCraft's Havok version,
   `Collision.cpp`, is 1,282 lines.
7. **The HUD.** `FrameExporter` reads back Minecraft's whole frame (hand, HUD and screens on a transparent background)
   every frame: 2560×1440×4 = 14.7 MB, about 1.2 GB/s at 80 fps on each side. Spike 3's `present` mode already draws
   on the back buffer. SkyCraft's `Overlay.cpp` is 410 lines (D3D11). Added to this spike later (see "Added after the
   review: Minecraft's HUD"), at half size: 3.7 MB a frame.

## Non-goals

- **No puppeting:** V moves with Cyberpunk's own movement, and Minecraft's player follows by teleports.
- **No using Minecraft's screens from Cyberpunk:** the HUD shows the inventory or chat if one opens, but working them
  needs a forwarded cursor and more keys. No mouse cursor is drawn.
- **No real collision:** only the flat floor of triangles. Minecraft's player never walks.
- **No other Minecraft drawing:**
  - no entities, items, particles, the player's avatar, or the targeted block's outline;
  - no animated textures: their first frame only;
  - no translucent blocks (water, stained glass, ice);
  - no Minecraft lights, and no shadows.
- **No chat or screens from Cyberpunk's keyboard:** only the build-mode keys are forwarded, and commands come from the
  overlay's buttons and requests. Their replies show in the HUD's chat.
- **No exact copy of Minecraft's crosshair:** Minecraft draws it with an invert blend, which the transparent capture
  can't keep, so it comes out plain white. SkyCraft redraws that patch with the invert blend; this spike doesn't.
- **No changes to SkyCraft's mod,** and no gamepad.
- **Only these versions:** game 2.31 (as spike 3), SkyCraft 0.1.2 with protocol version 11, Minecraft 26.3.

## Environment (checked on 2026-10-02)

### The game and BlockRender

- **The game and its settings are as in spike 3:** Cyberpunk 2077 2.31 (Epic), DLSS Quality at 2560×1440, frame
  generation, ray tracing, HDR and VSync off, Reflex on.
- **BlockRender 0.8.0** is installed in `E:\Cyberpunk2077\red4ext\plugins\BlockRender\`, with its overlay in CET's
  `mods\blockrender_hud\`. Drawing is off unless a request or a button switches it on. Spike 1's probe 0.4.0 and its
  overlay are installed too.
- **Keys already taken:** CET's overlay is Shift+F6 (`bindings.json`), FreeFly is F7, and the probe dumps on F10.
  Cyberpunk binds no F2, F6, F7, F8, F10, F11 or F12 (`r6\config\inputUserMappings.xml`), so F8 is free. The user
  hasn't rebound any key (`UserSettings.json`), so the defaults below hold.

### Cyberpunk's input

- **Raw Input only,** from the executable's imports (`dumpbin /imports`):
  - `USER32`'s `RegisterRawInputDevices`, `GetRawInputData`, `PeekMessageW`, `ClipCursor`, `SetCursorPos` and
    `GetMessagePos`;
  - `XINPUT9_1_0`'s `XInputGetState`;
  - `HID.DLL`.

  It imports no DirectInput, no `GetRawInputBuffer`, and no key-state function (`GetAsyncKeyState`, `GetKeyState`,
  `GetKeyboardState`).
- **How it reads them** (its code, read while planning Part 3; see "Changes while planning"):
  - it registers the keyboard and the mouse with no flags, so Raw Input reaches it only while its window has the
    focus (no `RIDEV_INPUTSINK`), and Windows still sends the legacy key and mouse messages (no `RIDEV_NOLEGACY`);
  - one window-message handler reads every `WM_INPUT`, into a 48-byte buffer, and ignores what `GetRawInputData`
    returns. It acts on the buffer's type: mouse, keyboard, or anything else skipped;
  - **the mouse:** the movement from `lLastX` and `lLastY`, the wheel from `RI_MOUSE_WHEEL` in whole notches, and
    the buttons from `usButtonFlags`' presses and releases;
  - **the keyboard:** by virtual key. One of 0xFF or 0 is skipped, and so is a press of a key already down;
  - no legacy key or mouse message (`WM_KEYDOWN`, `WM_LBUTTONDOWN`) reaches its input system.
- **The bindings that clash with Minecraft's building keys** (`inputUserMappings.xml`):
  - **mouse:** left is attack, right is aim, middle is the combat gadget or tag;
  - **the wheel** is next and previous weapon;
  - **keys:** 1 to 4 are weapon slots, Q is quick melee, E is the iconic weapon, F is interact, T is the phone,
    X is a consumable.
- **The keys that open menus:** Esc (pause), I (inventory), M (map), J (journal), P (perks), K (crafting), H (hub) and
  T (phone).
- **The address file** (`bin\x64\cyberpunk2077_addresses.json`) names 84 of its 987,479 functions. They include
  `input::InputSystemWin32Base::Update` and `cp::PlayerSystem::OnPlayerMainObjectSpawned`, neither used by this spike.

### Minecraft

- **Minecraft 26.3** is installed in `%APPDATA%\.minecraft`. Its jar has 1,300 block textures (105 with animation
  files, 48 wider than 16 pixels) and 831 item textures.
  - It acts on clicks in its 20 Hz client tick: `startAttack`, `startUseItem` and `continueAttack` are called from
    `handleKeybinds`, which `tick()` calls.
  - `renderFrame` calls `GameRenderer.render()` every frame. Being minimized only stops it reconfiguring the GPU
    surface.
  - **Minimizing:** `Window.onFramebufferResize` ignores a 0×0 size, so a minimized window keeps its render target.
    But while linked, SkyCraft reports the window as not minimized, so Minecraft would try to reconfigure its surface
    at the minimized window's size, a path nobody has tried. That's one reason the window stays hidden (`SDL_HideWindow`
    keeps its size).
  - `/fill` is capped by the game rule `max_block_modifications`, 32,768 by default.
  - **The GUI scale it picks automatically** is the largest that still leaves at least 320×240 GUI pixels: 3 for
    1280×720, and 6 for 2560×1440.
- **The Modrinth App** is installed, with the user's account. The user makes the instance:
  - Minecraft 26.3, Fabric Loader 0.19.5, and Fabric API 0.161.0+26.3;
  - `skycraft-fabric-0.1.2.jar` in its `mods` folder;
  - JVM arguments `--enable-native-access=ALL-UNNAMED -Dskycraft.quitWithSkyrim=false`. Without the second, Minecraft
    saves and quits 5 s after the process in the header's game process ID ends (`SkyClient.quitWithSkyrim`):
    `mclink`'s or Cyberpunk's. Without `-Dskycraft.showWindow=true`, the window hides itself once linked. It's on screen
    only until then, at the title screen.
- **Java:** Minecraft 26.3 needs Java 25, which the Modrinth App provides. Temurin 25.0.2 is also on `PATH`.

### SkyCraft

- **The source:** [chasmlol/SkyCraft](https://github.com/chasmlol/SkyCraft) at `bfcaf178524b92c2cdeb88e4ce0f13ef9ded6f32`
  ("SkyCraft 0.1.2: fix explosion lag, add a destruction toggle"), MIT. The user allowed the download. It was cloned
  for reading into the session's scratchpad, so only the protocol header comes into the repo.
- **The release** `v0.1.2` has `skycraft-fabric-0.1.2.jar` (242 KB), `SkyCraft-0.1.2.zip` (the Skyrim plugin with a
  Prism bundle, 24.5 MB) and the plugin's symbols.
- **Its build:** Minecraft 26.3, Fabric Loader 0.19.5, Fabric API 0.161.0+26.3, Loom 1.18-SNAPSHOT, Gradle 9.7.1,
  Java 25.

#### SkyCraft's mod, once linked

The game side creates the mapping, and Minecraft opens it, checking the magic and version. The link counts as up while
the game's heartbeat is less than 8 s old (`SkyLink.active`). Once it's up:
- **The window:** hidden with `SDL_HideWindow`, unless `-Dskycraft.showWindow=true`. Minecraft is told it's focused
  and not minimized (`WindowMixin`). `SkyState`'s viewport, when it isn't 0, resizes the window to that size
  (`applyViewportSize`), and the HUD image follows the window's size.
- **Rendering:** Minecraft's own level rendering is cancelled (`LevelRendererMixin`), so its window shows only the
  hand, the HUD and screens.
- **Input:**
  - key states come only from the input ring (`InputConstantsMixin`), and the mouse is never grabbed;
  - `InputBridge` replays the ring's events into Minecraft's keyboard and mouse handlers;
  - the menu or loading flag releases every held key, each frame.
- **The look:** the player's yaw and pitch are set from `SkyState` every frame while no Minecraft screen is open.
- **Frame pacing:** Minecraft waits, spinning, up to 25 ms a frame for `SkyState`'s sequence to change.
- **Teleports:** with the in-game flag set and not loading, a new `teleportSeq` (or a new player object) teleports the
  player to `SkyState`'s position, on the client and the integrated server.
- **Holding:** after a teleport, or while not in game, the player is held still at that spot (`holdUntilReady`). It's
  released only once the regions around it are known from voxel messages, and there's ground below or 6 s have passed.
- **Its world:** `MirrorWorld` opens the "SkyCraft" world, or creates it: the `skycraft:mirror` preset, an empty void
  with a dimension from y −1024 to 1024, survival, normal difficulty, cheats on.
  - `SkyCraft.configureServer` stops time, weather and mob spawning, and turns the movement check off;
  - each player gets a starter kit and a builder kit;
  - linking sets the instance's options: render and simulation distance 8, VSync off, a 260 fps limit, music off.
- **Exporting** (`SkyClient.afterRender`, right after `GameRenderer.render()`, while its player is in a world), in
  order:
  1. on a new link, a new game process, a new world or a resource reload: a clear-all, the combined atlas, then every
     loaded section that isn't air;
  2. each frame, dirty sections: the player's own edits first, up to 12 a frame within 3 ms;
  3. animated-sprite patches, the world-entity table, the avatar, and the scene (other entities and particles). The
     per-frame ones are written only if the ring has room, and dropped otherwise;
  4. its HUD image (`FrameExporter`): the hand, HUD and screens on a transparent background. It's read back from the
     GPU every frame at the window's size, asynchronously, so it's shipped about a frame later. The rows are bottom-up
     (slot flag bit 0), RGBA8 with premultiplied alpha (as SkyCraft's `Overlay.cpp` notes). It's published through the
     overlay's triple buffer (see "The protocol").
- **Its own window still takes real input.** Only key states and the mouse grab are taken over, so keys and clicks
  inside Minecraft's window reach its handlers. A click in it can act in the world: Minecraft treats the first click
  as grabbing the mouse, and later ones as attack or use. Hidden, as in this spike, nothing can click it.
- **Clicks on the game's ground:**
  - the crosshair's pick goes through `SkyClip` (`EntityPickMixin`) with no other condition, so triangles alone are
    enough. Without triangles, a click in the void hits nothing;
  - the cast is two-sided, with the normal turned to face the ray, so a triangle's winding doesn't matter;
  - a block placed against a triangle goes in the cell at `floor(hit + normal × 0.4)`;
  - attacking a triangle digs only if its flags say diggable (`SkyDigClient.attack`). Ours never do, so an attack
    there does nothing;
  - while linked, the server allows flight (`MinecraftServerFlightMixin`), so a player held in mid-air isn't kicked.
- **Fields of `SkyState` it doesn't use:** `gameHour`, and `collisionEpoch` (the collision ring carries its own
  epochs). A viewport of 0 would leave Minecraft's window size alone, but this spike sends the HUD's size.

## The protocol, as this spike uses it

SkyCraft's `protocol/skycraft_protocol.h` (version 11) is copied unchanged into `extern/skycraft/`, with its MIT
licence and a note of the commit. Its static checks pin the layout. Everything is little-endian, and coordinates are
Minecraft's (blocks, y up, z south). The mapping is `Local\SkyCraft_v1`, 200,327,168 bytes (191 MiB):

| Offset | Region | Direction | This spike |
|---|---|---|---|
| `0x0` | `Header`: magic, version, both process IDs, both heartbeats | both | written and read |
| `0x100` | `SkyState`, seqlocked | game → MC | written every frame |
| `0x200` | `McState`, seqlocked | MC → game | read every frame |
| `0x300`, `0x340` | the overlay's triple buffer: its state, and 3 slot headers | MC → game | read: the HUD |
| `0x400` | `WaterGrid` | game → MC | unused (never written, so Minecraft sees none) |
| `0x1000` | the input ring: 4,096 events of 16 bytes | game → MC | written |
| `0x12000` | the actor table | game → MC | unused |
| `0x17000` | the event ring: 512 events of 32 bytes | MC → game | drained, explosions logged |
| `0x1C000` | the world entities | MC → game | unused |
| `0x20000` | the collision ring, 32 MiB | game → MC | the floor's messages |
| `0x2020000` | the overlay's pixels, 3 slots of 3840×2160×4 | MC → game | read: the HUD |
| `0x7F0C000` | the render ring, 64 MiB | MC → game | drained every frame |

- **The rings:** a head at `+0x0` written by the producer, a tail at `+0x40` written by the consumer, and data at
  `+0x80`. Heads and tails count bytes (or events) ever written.
  - The byte rings' messages start 8-byte aligned with `{u32 type, u32 payload bytes}`. A message never wraps: type 0
    (pad) means "skip to the ring's start".
  - A render message is at most half the ring, 33,554,368 bytes.
- **`SkyState`, as written:**
  - `flags`:
    - in game while linked and V is found;
    - menu open whenever build mode is off;
    - never loading.
  - `worldId` 1, and `collisionEpoch` the floor's epoch;
  - the position: the teleport target (see "The link");
  - yaw and pitch from V's camera;
  - `teleportSeq`;
  - the viewport: the size the HUD image should be, the back buffer's size divided by `hudscale` (2): 1280×720 here;
  - `gameHour` 0.
- **`McState`, as read:**
  - the flags: in world, a screen open;
  - the position and eye;
  - `frameCounter`, for Minecraft's frame rate;
  - the header's `mcPid` and `mcHeartbeatMs`, for whether Minecraft is there (heartbeat under 3 s, as SkyCraft's
    `kMcTimeoutMs`).
- **Input events:** `{u16 type, u16 code, i32 a, b, c}`:
  - key: code is the SDL scancode, a is 1 for down and 0 for up;
  - mouse button: code is 1 left, 2 middle, 3 right, and 4 and 5 the side buttons;
  - scroll: a is notches × 120, positive up;
  - text: a is the code point, taken only while a screen is open;
  - release all.
- **The overlay's triple buffer, as read** (SkyCraft's `Link::AcquireOverlayFrame`):
  - its state at `0x300`: bits 0 to 1 hold the middle slot, and bit 2 says the middle slot holds a frame not yet read.
    Minecraft writes into its own back slot, then swaps it into the middle with the bit set;
  - the reader keeps a front slot. When the bit is set, it swaps its front slot into the middle (bit clear) and takes
    the slot it gets back as its new front, which Minecraft won't touch until it's swapped out again;
  - each slot has a 64-byte header at `0x340`: width, height, flags (bit 0: rows bottom-up) and a frame number. Its
    pixels are at `0x2020000` plus the slot × 33,177,600;
  - for a new Minecraft process, the state goes to 0 and the reader's front to slot 2, since its writer starts again
    at slot 1 (see "Changes while planning").
- **Collision messages, as sent:**
  - clear: payload is the epoch;
  - triangles: a `ColRegion` header (the region's inclusive block box, the epoch, the count), then `ColTri`s (9 floats
    and flags, 0 here).

  Voxel regions (`kColRegion`) are never sent.
- **Render messages handled:**
  - atlas: `{u32 w, h}`, then w×h RGBA8, top row first;
  - section: `{i32 sx, sy, sz; u32 vertices}`, then 32-byte vertices, a triangle list relative to the section's corner;
  - clear-all.

  The rest (atlas patches, lights, solids, digging, textures, avatar, scene, ragdoll) are counted and skipped.
- **The vertex** is BlockRender's `Vertex`:
  - position, then UV;
  - RGBA8 colour, which is tint × ambient occlusion, without Minecraft's face shading;
  - light: block light in bits 0 to 7, sky light in bits 8 to 15;
  - flags: bit 0 alpha-tested, bit 1 translucent, bits 4 to 6 the face's direction + 1.

## Repository layout

```
docs/superpowers/specs/2026-10-02-minecraft-link-design.md   this file
docs/superpowers/plans/2026-10-02-minecraft-link.md          the plan
extern/skycraft/                  SkyCraft's protocol header, unchanged, with its MIT licence and source commit
spikes/block-render/
  src/link/Mapping.*              the shared mapping: create or reuse, reset, heartbeat, Minecraft's process
  src/link/Rings.*                the rings and seqlocked blocks, read and written on a span of bytes
  src/link/Messages.*             render messages, checked, into sections and an atlas
  src/link/Overlay.*              the overlay's triple buffer: the newest HUD image, checked
  src/link/Floor.*                the floor's collision messages
  src/link/Look.*                 yaw, pitch and the teleport target, from V's camera
  src/link/Commands.*             typing commands into Minecraft's chat
  src/link/Link.*                 the link each frame: in game, teleports, the floor, events, Minecraft coming and going
  src/input/Scancodes.*           set-1 scan codes to SDL scancodes (SkyCraft's table, credited)
  src/input/BuildMode.*           what each raw event does, in and out of build mode
  src/input/FocusWatch.*          whether the game has just lost the front
  src/input/Router.*              each RAWINPUT event read, hidden and forwarded, for the hook's and two other threads
  src/input/RawInputHook.*        the GetRawInputData import patch, and the focus watch's thread
  src/render/Atlas.*              + an atlas from Minecraft's RGBA, with mip levels
  src/render/PendingSections.*    the renderer's waiting uploads, the latest mesh per section
  src/render/Hud.*                the HUD pass's rules: when it shows, and where each pixel samples the image
  src/render/Renderer.*           + SetAtlas, ClearSections, upload timing, when an upload is first drawn, the HUD pass
  shaders/blocks.hlsl             + the HUD's vertex and pixel shaders
  src/Requests.*, Status.*, Plugin.cpp   + the new commands, the link's status, the update order
  tools/mclink/Main.cpp           the console stand-in
  tools/blockrender_hud/init.lua  + Link, Unlink and the command buttons, and the link's status
  tests/                          + the new unit tests
```

## Build

- **As in spike 3,** with three changes:
  - `render_core` gains `link/`, `input/Scancodes`, `input/BuildMode`, `input/FocusWatch`, `input/Router`,
    `render/PendingSections` and `render/Hud`, so the unit tests reach them;
  - `render_runtime` gains `input/RawInputHook`: the patch, and the focus watch's thread;
  - a new console target, `mclink`, links `render_core`.
- **The protocol header** comes from `extern/skycraft/`.
- **Versions:** BlockRender's version becomes 0.9.0, in the log line and in `Query`.
- **Deploying** is unchanged: `CP2077_DIR` is set only to deploy, with the game closed, and cleared straight after.
  `mclink.exe` isn't deployed. It runs from the build folder.

## Components

### `Plugin.cpp`

- **On load,** before the existing hooks:
  - patches `GetRawInputData`;
  - starts the focus watch.

  Neither does anything visible until the user links or presses F8.
- **Every frame, in `OnUpdate`,** in this order:
  1. once a second: the mapping, until it's open (see "The link"), and requests;
  2. `Link::Update`, with V's pose from `LocatePlayer` (see "The link"). Spike 1 called the same script functions
     every frame without trouble. It also hands the newest HUD image, if there is one, to the renderer;
  3. the frame log and the dumps;
  4. meshing spike 3's own structures, as before, and the renderer's `Update`;
  5. the log;
  6. the status, once a second.
- **On unload:** build mode off, the focus watch stopped, the import patch put back, and the mapping unmapped.
  Minecraft sees the heartbeat stop.

### `link/`

- **`Mapping`:**
  - creates the named mapping with SkyCraft's access rules: this user, the system and administrators, at medium
    integrity, so an elevated game doesn't lock Minecraft out;
  - **finding it already there** (Minecraft holds it open between game sides):
    - refuses if the view is smaller than the layout, checked with `VirtualQuery`, since it would belong to something
      else;
    - refuses if another game side is still running: the header's game process ID is a live process other than this
      one, and its heartbeat is under 3 s old;
    - otherwise resets the regions the game side owns, as SkyCraft's `Link::Create` does;
  - writes the header, and the heartbeat;
  - reports Minecraft's process ID and heartbeat.

  It takes the name as a parameter, so tests use their own. A refusal is retried once a second, and the status says
  why.
- **`Rings`:** the seqlocked writes and reads, and both directions of the byte and event rings. These are pure
  functions on a span of bytes with acquire and release ordering, so they're tested on ordinary memory.
- **`Messages`:** turns render messages into a section (coordinates and vertices) or an atlas, after checking each
  against its layout (see "Safety").
- **`Overlay`:** the triple buffer's reader. It takes the newest HUD image when there is one, as in "The protocol",
  and checks it before use:
  - a slot index of 3 is refused;
  - width and height from 1 to 3840 and 2160.

  Link resets it only for a new Minecraft process (see "Changes while planning").
- **`Floor`:** the clear message, and one triangle message per region, from a centre, a height and an epoch.
- **`Look`:**
  - yaw = atan2(−x, z) and pitch = −asin(y), in degrees, from V's camera's forward vector in Minecraft's axes. Looking
    straight up or down, where the forward vector's horizontal length is under 0.001, the last yaw is kept;
  - the yaw is the one within 180° of the last, in no fixed range, as Minecraft's own is (see "Changes from step 2");
  - the teleport target: the camera's position in Minecraft's axes, less 1.62 blocks of height, Minecraft's standing
    eye height, so Minecraft's eye is at V's camera;
  - if the camera can't be read, V counts as not found: the in-game flag clears, the last look is kept, and there's no
    teleport (see "Changes while planning").
- **`Commands`:** the typing state machine (see "Typed commands"), driven by `McState`'s flags.
- **`Link`:** the per-frame logic, given the mapping, V's pose and the time. Its decisions (teleport or not, floor or
  not, what to type next) are testable with a fake pose and a mapping in memory.

### `input/`

- **`Scancodes`:** SkyCraft's set-1 to SDL table. An extended key (E0) is code + 0x80, as in DirectInput.
- **`BuildMode`:** what each raw event does: pass, forward and hide, toggle, or leave and pass. It goes by whether
  build mode is on, where the event's press went, whether the game is in front, and what the link last said. It also
  keeps the hotbar slot. Pure.
- **`Router`:** reads each event `GetRawInputData` copied out, after checking its size, asks `BuildMode`, hides in
  place what Minecraft takes, and forwards it through the link. The hook's thread, the focus watch's and the main
  thread share it, behind one lock. It's tested on synthetic `RAWINPUT` events, with a link in memory.
- **`RawInputHook`:** the import patch. It calls the original, then hands the event it copied to the router (see
  "Input").
- **`FocusWatch`:** whether the game has just stopped being in front. A small thread checks it 10 times a second, and
  tells the router. It's a thread so that held inputs are released even if the game stops updating in the
  background, which step 2 found it doesn't.

### The renderer and the atlas

- **`Atlas`:** a new constructor from width, height and RGBA bytes. It builds 5 levels, each half the last
  (rounding down, at least 1 texel), by averaging 2×2 texels: an odd side's last row or column is left out, and a
  side of 1 is used twice. The shader already finds levels this way.
- **`Renderer`:**
  - **`SetAtlas`** replaces whatever atlas it has, from `atlas.bin` or Minecraft, under the renderer's lock, and
    uploads it on the next `Update`. Until now the atlas was loaded once, and `AtlasData()`'s pointer stayed valid for
    good. Now it can change between frames, so the main thread fetches it again each time it needs it;
  - **`UploadSection`** keeps only the latest mesh for each section while it waits (`PendingSections`). Until drawing
    is first switched on, `Update` uploads nothing, and today every update queues another copy, which a linked
    Minecraft would pile up;
  - **`ClearSections`** removes every section, and every waiting upload;
  - **`Update`** reports its CPU time and its upload's fence;
  - **drawing** notes when it first records a G-buffer list that draws an upload, for latency;
  - **`SetHudImage`** copies a HUD image into one of 3 upload buffers, one the GPU has finished with, and marks it as
    the newest. It's called on the main thread;
  - **the HUD pass** runs at Present, in any drawing mode, when `Hud` says the HUD shows (see "The HUD"). It records
    one list onto the main queue just before Present, through spike 3's `present` path: the back buffer, its
    barriers, and a pipeline for its format. The list copies the newest image into a buffer in GPU memory, if a new
    one came, then draws one full-screen triangle with the HUD's shaders and premultiplied-alpha blending, without
    depth. It has its own skip count, and never switches block drawing off.
- **`Hud`:** the pass's pure parts:
  - whether the HUD shows: the `hud` setting, build mode, and Minecraft in its world. Until an image has come, the
    pass skips its frames;
  - the viewport to ask for;
  - which image texel a back-buffer pixel takes: scaled by as many whole back-buffer pixels a texel as fit, which is
    `hudscale` once Minecraft has resized, and flipped when the rows are bottom-up. Pixels outside the image show
    nothing (see "Changes while planning");
  - which of the 3 upload buffers each image goes through.
- **The HUD's shaders,** in `blocks.hlsl`, with the existing root signature:
  - the vertex shader makes one full-screen triangle from the vertex index;
  - the pixel shader reads the image as a structured buffer of RGBA8 texels, the way the atlas is read, with no
    texture or descriptor heap. It returns the texel as it is, since its alpha is premultiplied;
  - the sizes and the flip come in as root constants.

### Status and the overlay

- **`status.txt`** gains the link's lines (see "Measurements").
- **The overlay** gains:
  - Link and Unlink buttons;
  - the command buttons;
  - a HUD button that cycles never, in build mode, and always;
  - a line showing build mode and the hotbar slot.

  The mode buttons stay.

### `mclink.exe`

A console program around the same `link/` code, standing in for Cyberpunk:
- **It takes:**
  - a position for Minecraft's player's feet and a look. The default is where V stood at spike 3's corner: Minecraft
    (−119.5, 6.14, 2007.5), facing north;
  - how long to run;
  - commands to type;
  - clicks or keys to send.
- **It writes `SkyState`** at 60 Hz with that position and a 1280×720 viewport. It sends the floor, types its
  commands, and sends its inputs.
- **It reports:**
  - the link's state;
  - Minecraft's position and flags;
  - every message type, with counts and bytes;
  - the atlas's size, saved as DDS with spike 3's writer for `view_dump.py` to turn into PNG;
  - each section, with a few vertices and the atlas texels their UVs land on;
  - explosions;
  - HUD images: how many a second, their size and flags. The latest one is saved as DDS too, unflipped, with its
    alpha.
- **It refuses to start** on the same rules as `Mapping`: if another game side (Cyberpunk, or another `mclink`) is
  running, or the mapping found has the wrong size. Minecraft holding the mapping open isn't a reason. That's the
  normal case when `mclink` starts again.

## The link

- **The mapping** is created at the game's first update, at its main menu, rather than at load: the heartbeat comes
  from `Link::Update`, once a frame, and the game doesn't update until it has loaded (see "Changes while planning").
  Minecraft polls for it once a second, so it links whichever starts first, and opens its world straight away. The
  in-game flag stays clear until the user links.
- **Linking:**
  - the overlay's Link button, `link on`, or the first F8 links. F8 arrives on the hook's thread, so it only asks, and
    `Link::Update` links on the main thread, which `LocatePlayer` needs;
  - the in-game flag is set while linked and V is found. It clears by itself when V isn't found (loading), and build
    mode ends with it. At the main menu the stand-in V counts as found, which is harmless: Minecraft's player just
    waits near the origin;
  - Link also switches drawing to `gbuffer` if it's off;
  - Unlink (`link off`) clears it and ends build mode. Minecraft then holds its player where it is, and the blocks
    stay drawn.
- **`SkyState` once a frame,** so Minecraft paces to Cyberpunk's frames: yaw and pitch from V's camera every frame,
  and the other fields as in "The protocol". The viewport comes from the back buffer's size, which the Present hook
  sees, divided by `hudscale`. Minecraft resizes its hidden window to it.
- **Teleports:**
  - the target: Minecraft's eye at V's camera;
  - `teleportSeq` goes up on linking, when Minecraft comes back, and when the camera is `teleportstep` (0.1 m) from
    the last target;
  - at most `teleportrate` (10) times a second.

  Each sends Minecraft's player there. No voxel regions are ever sent, so it stays held: pinned at the target, never
  falling.
- **The floor:**
  - flat triangles, two per region, at V's feet height, over 7 by 7 regions of 8 blocks centred on V (56 m square);
  - sent on linking, when Minecraft comes back, and on a `floor` request, but only while Minecraft is there. Until
    then, it would only queue in a ring nobody reads;
  - sent again, with a new epoch whose clear message drops the old one, once V's feet are 16 m from the floor's centre
    or 1 m off its height.

  It's what clicks hit. A block placed on it goes in the cell at `floor(hit + normal × 0.4)`, so its bottom sits on a
  whole metre between 0.6 m below V's feet and 0.4 m above them. At spike 3's corner (feet at 6.14) that's 6.0. Blocks
  stay at V's height even where the street rises or dips.
- **Minecraft coming and going:**
  - **gone:** its heartbeat stops for 3 s, and the status says so. Build mode ends, typing stops, and the blocks stay;
  - **back:** a new heartbeat or process ID. Minecraft sends its clear, atlas and sections again by itself. The link
    sends the floor and a teleport. It resets the overlay's triple buffer only for a new Minecraft process, whose
    writer starts over (see "Changes while planning");
  - **a restarted Cyberpunk:** it finds the mapping still there if Minecraft holds it, resets its own regions and
    writes its new process ID. Minecraft sees the change and sends everything again.
- **Events:** the event ring is drained every frame. Explosions (`kEvExplosion`, centre and radius) are logged with
  their time. Other events are counted.

## Blocks

- **Draining:**
  - every frame in `Link::Update`, before the renderer's `Update`, whether drawing is on or off. Minecraft must never
    find the ring full;
  - up to `ringmb` (16) MB of sections and atlases a frame, checked between messages, so a big message is still taken
    whole;
  - the payloads taken are copied out before the tail moves on. Skipped messages are only stepped over, without
    copying, and don't count toward `ringmb`. Explosions make the per-frame scene message large with particles.
- **The atlas:** built into an `Atlas` with mip levels, and handed to `SetAtlas`.
  - Spike 3's own structures are cleared then, since their UVs belong to `atlas.bin`.
  - While Minecraft's atlas is loaded, spike 3's `place` commands are refused, `place field <n>` included. `clear`
    clears everything, but Minecraft's atlas stays.
- **Sections:**
  - handed to `UploadSection` by their Minecraft section coordinates. Spike 3's renderer already places section
    (x, y, z) at Minecraft block (16x, 16y, 16z), which is world (16x, −16z, 16y);
  - a vertex count that isn't a multiple of 3 is cut down to one;
  - no vertices removes the section, and clear-all removes every section;
  - a section that arrives before Minecraft's atlas is dropped and counted. Minecraft always sends its clear-all and
    atlas first when it links, so one can only be left over from before the plugin started reading.
- **Translucent triangles** (flag bit 1) are dropped and counted, since a G-buffer takes only opaque surfaces.
  `set translucent 1` draws them as cut-outs instead, to see where they are.
- **What's ignored:** Minecraft's light values (the game lights the blocks) and its face shading (already removed by
  the mod). Its ambient occlusion stays in the vertex colour, as in spike 3.
- **Bursts are measured, not assumed** (see "Measurements"). If a TNT burst hitches the frame, the fix is part of this
  spike:
  - sections suballocated from a few large buffers, instead of two committed buffers each;
  - an upload budget per frame.

## Input

- **The hook:**
  - patches the import of `GetRawInputData` from `USER32.dll` in `Cyberpunk2077.exe`. If something else patched it
    first, ours calls whatever was there;
  - calls the original, and looks only at calls that return an event: `RID_INPUT`, a buffer, a positive size, and a
    keyboard or mouse event of the expected size. Everything else passes through untouched.
- **Build mode, as `BuildMode` decides:**

  | Event | Outside build mode | In build mode |
  |---|---|---|
  | F8 (scan code 0x42) | Toggles build mode on, hidden from the game | Toggles it off, hidden |
  | Left, right, middle and side buttons | Passes | Forwarded as SDL button 1, 3, 2, 4 or 5, and cleared from the event |
  | Wheel | Passes | Forwarded as a scroll, and cleared from the event |
  | 1 to 9 (0x02 to 0x0A) | Passes | Forwarded as SDL 30 to 38, and the event made a no-op |
  | Esc, I, M, J, P, K, H, T, or F6 with Shift held | Passes | Leaves build mode, and passes |
  | Anything else, mouse movement included | Passes | Passes |

  - A cleared mouse event keeps its movement, so the look keeps working. A no-op key has scan code 0 and virtual key
    0xFF. The game reads the buttons and the wheel only from the cleared flags, and skips a key whose virtual key is
    0xFF (see "Cyberpunk's input").
  - Key repeats are forwarded as repeats, and F8's own repeats are ignored.
- **A press's repeats and release go where the press went** (see "Changes while planning"). A button, or a key 1 to 9,
  pressed outside build mode is the game's until it's released, even if build mode goes on meanwhile: V doesn't go on
  aiming. One pressed in build mode is Minecraft's. When build mode ends, Minecraft lets go of everything at once, so
  its presses' repeats and releases then go nowhere. When the game loses the front, every press is forgotten, since
  it's released in another window.
- **Build mode turns on only while Minecraft is in its world, and V is found.** F8 while unlinked links first, as
  `link on` does, and turns build mode on after the link's next update. Otherwise F8 says why on the overlay, and
  nothing goes into the ring, so no stale clicks wait for Minecraft. Minecraft going or leaving its world, V not found
  (loading), or Unlink ends build mode.
- **Leaving build mode** sends a release-all, and `SkyState`'s menu flag repeats it every frame while build mode is
  off. Nothing stays held down in Minecraft.
- **Focus:**
  - **The hook forwards nothing unless the foreground window belongs to the game's process.** It checks before
    forwarding each event. Cyberpunk doesn't ask for Raw Input in the background (no `RIDEV_INPUTSINK`; see
    "Cyberpunk's input"), so this only guards against an event that arrives anyway: a click meant for another
    window would otherwise reach Minecraft through the ring.
  - **`FocusWatch` releases held inputs once** when the game stops being in front. It doesn't rely on `OnUpdate`, so
    a button held while Alt-Tabbing is let go even if Cyberpunk stops updating in the background.
  - **Build mode stays on,** so it's still on when the user comes back from another window.
- **The hotbar slot:** tracked from the forwarded keys and the wheel (up is the slot to the left, down the slot to the
  right, wrapping at 1 and 9), and shown on the overlay. It's unknown until a number key sets it. The wheel moves it as
  Minecraft 26.3's `ScrollWheelHandler` does: by whole notches, keeping the fraction, one slot an event at most, and
  starting over when it turns the other way. A side button makes it unknown again, since which slot it picks, if any,
  is Minecraft's own binding. It's approximate: Minecraft can change the slot by itself, for example when the middle
  button picks a block that's in another slot.
- **What build mode takes from Cyberpunk:** every use of the mouse buttons and the wheel, not only combat. That
  includes scrolling through dialogue choices, scanning and quickhacking, and the radial menu. F8 out of build mode
  first. Interact (F) still works.
- **Threads:** the hook runs on whichever thread Cyberpunk reads input on, and `FocusWatch` on its own.
  - Build mode's state, what the link last said about Minecraft and V, F8's link request, the last click's time and
    the link itself are behind the router's one lock. The main thread keeps the link's view current, once a frame.
  - The link's own mutex guards writing into the input ring, which `Commands` and the router share.
  - At unload, the main thread takes the link from the router before unmapping, so no thread pushes into it after.

### Typed commands

- **The sources:** `mc <command>` requests, and the overlay's buttons, which write named requests.
- **Typing one:**
  1. press and release T;
  2. wait for `McState`'s screen-open flag, for up to 2 s;
  3. send the text;
  4. press and release Enter;
  5. wait for the screen to close, for up to 2 s, before the next one.
- **Refused if** Minecraft isn't in its world, or a Minecraft screen is already open. A raw command must start with
  `/`, be printable ASCII, and be at most 256 characters, Minecraft's limit.
- **The named sets,** typed in order. Positions are relative to Minecraft's player, whose feet are about V's, so they
  follow V. The TNT sets switch to creative first, so the explosions can't kill Minecraft's player.

  | Name | Commands | Does |
  |---|---|---|
  | `creative` | `/gamemode creative` | Creative mode |
  | `tnt-kit` | `/give @s tnt 64`, `/give @s flint_and_steel` | TNT and a lighter in the inventory |
  | `tnt-ahead` | `/gamemode creative`, `/execute rotated ~ 0 positioned ^ ^ ^4 run setblock ~ ~-1 ~ stone`, `/execute rotated ~ 0 positioned ^ ^ ^4 run summon tnt` | Primed TNT on a stone block, 4 blocks ahead |
  | `wall` | `/execute rotated ~ 0 positioned ^ ^ ^4 run fill ~-2 ~ ~ ~2 ~3 ~ stone_bricks` | A stone-brick wall 5 wide and 4 tall, 4 blocks ahead, running east to west |
  | `tnt-cube` | `/gamemode creative`, `/execute rotated ~ 0 positioned ^ ^ ^6 run fill ~-2 ~ ~-2 ~2 ~4 ~2 tnt`, `/execute rotated ~ 0 positioned ^ ^ ^6 run summon tnt ~ ~5 ~` | 125 TNT, 5×5×5, 6 blocks ahead, set off by primed TNT on top: the worst burst |
  | `field` | `/fill ~-63 ~ ~-63 ~64 ~ ~64 grass_block` | A 128×128 layer at V's feet, 16,384 blocks in one fill, 64 to 81 sections at once |
  | `wipe` | `/fill ~-63 ~ ~-63 ~64 ~ ~64 air`, `/fill ~-16 ~ ~-16 ~15 ~31 ~15 air` | The field's layer, then 32×32×32 around V, the largest fill one command allows |

## The HUD

- **What shows:** Minecraft's frame without its world:
  - the hotbar with its selected slot, the hand and the held item;
  - health, hunger and experience;
  - chat, which includes the replies to typed commands;
  - any open screen;
  - its crosshair, in white (see "Non-goals").

  No mouse cursor is drawn.
- **When:** the `hud` setting.
  - `0` never;
  - `1` in build mode, the default;
  - `2` always, while Minecraft is in its world.

  Build mode is the default so the HUD never covers Cyberpunk's menus: the keys that open them leave build mode. The
  overlay's HUD button cycles the three. Step 2 uses `2`, since build mode comes in step 3.
- **Size:** the viewport asks for the back buffer's size divided by `hudscale` (2), 1280×720 here. The pass scales it
  back up with whole texels. At 1280×720 Minecraft's automatic GUI scale is 3, so each GUI pixel lands on 6×6 screen
  pixels, exactly as its automatic scale of 6 would at 2560×1440. `hudscale 1` asks for the full size, and 4 for a
  quarter.
- **The path:**
  1. Minecraft reads back its frame, about a frame late;
  2. the triple buffer;
  3. while the HUD shows, `Link::Update` takes the newest on the main thread, and `SetHudImage` copies it into an upload
     buffer: 3.7 MB at half size. While it doesn't, images are left unread in the triple buffer, where Minecraft keeps
     replacing the middle slot, at no cost to us;
  4. the HUD pass at Present copies it into GPU memory and draws it over the finished frame, over Cyberpunk's own HUD.
- **Blending:** premultiplied alpha (one, and one minus the source's alpha), as SkyCraft composites it. The rows are
  flipped when the slot says bottom-up.

## Measurements

- **The status, once a second:**
  - **the link:**
    - the mapping, created or reused, or refused with the reason;
    - Minecraft's process ID and heartbeat age;
    - in world, and screen open;
    - Minecraft's eye position, and V's camera, to compare;
    - its frame rate, from `frameCounter`;
    - linked;
    - teleports this second and the last target;
    - the floor's epoch, centre and height;
  - **build mode:** on or off with the reason, the slot, and since the start: inputs forwarded and hidden, the last
    one forwarded, input events dropped because the ring was full, and the keyboard and mouse events the hook has
    read, which shows it's on the game's path before build mode is on;
  - **the ring:** messages and MB this second by type, the most in one frame, drain time (average and worst), and
    rejected messages;
  - **blocks:**
    - the atlas's size;
    - sections and vertices;
    - translucent triangles dropped;
    - `Update`'s CPU time (average and worst);
    - the renderer's existing figures: frames drawn and skipped, GPU time, frame time;
  - **the HUD:**
    - the `hud` setting and whether it's showing;
    - images received and drawn this second, their size and MB a second;
    - `SetHudImage`'s copy time;
    - the pass's GPU time, and its skips with the reason;
  - **latency:** the last and the average (see below);
  - **processes:** CPU (as a share of one core) and working set, for Cyberpunk and for Minecraft (from `mcPid`).
- **Latency:**
  - each forwarded button press, and each typed command's Enter, is a mark with its time;
  - the first section to arrive after a mark gives its arrival time;
  - that section's upload gives the submit time;
  - the first G-buffer list recorded that draws that upload gives the drawn time. The game shows that frame a frame or
    two later, as it does all its frames.

  It's reported in two halves, in milliseconds and in Cyberpunk frames:
  - **Minecraft's:** mark to arrival. That includes waiting for its next 20 Hz tick, and for a command, the chat screen
    and the server's tick too;
  - **ours:** arrival to drawn, the part the question's "a frame or two" is about.

  Each mark goes in the log, and the status shows the last and the average. A mark with no section within a second (a
  click that changes nothing, or the number keys, which only change the slot) is dropped. Marks are only meaningful
  when nothing else is changing sections at the same time, so they're taken while V stands still.
- **Bursts:** `tnt-cube` and `field`.
  - An explosion event or the command's Enter starts the window.
  - Then over the next second: sections, MB, drain time, `Update`'s time and the worst frame.
  - The frame log can be armed alongside.
- **Frame rates:** Cyberpunk without Minecraft, then linked and idle, building, and through bursts. Minecraft's frame
  rate and CPU throughout, since it spins waiting for `SkyState`.
- **The HUD's cost:** both games' frame rates and our copy time with the HUD off, at `hudscale 2`, and at
  `hudscale 1`.

## The steps

Each step ends with what the user saw, and the agent's reading of the status, logs and measurements.

### Step 1: `mclink` against Minecraft, without Cyberpunk

The user makes the Modrinth instance, starts Minecraft, and says when it's at its title screen. The agent runs
`mclink`. Its typed commands and clicks act only in SkyCraft's world. Checks:
- **The link:** the link comes up, and Minecraft creates or opens the SkyCraft world.
- **The pinned player:** after the teleport, `McState`'s position stays at the target, to within 0.01 blocks, for 10 s.
- **The atlas** arrives. Its size is as expected, and the PNG looks like Minecraft's blocks and items.
- **Typed commands:** `/gamemode creative` and a `/setblock` send a section whose vertices' UVs land on that block's
  texels.
- **A click:**
  1. a typed command puts stone in the first hotbar slot;
  2. key 1 selects it;
  3. a right click, looking down at the floor, places a block in the expected cell.
- **TNT:** a `/summon tnt` sends an explosion event and changed sections.
- **The HUD image:**
  - images arrive at 1280×720, the viewport asked for, with their bottom-up flag;
  - the saved image shows the hotbar, the crosshair, and chat with the replies to the typed commands;
  - its alpha is premultiplied: across the whole image, no pixel has a colour channel above its alpha. `mclink`
    counts any that do. Step 1 found one (see risk 13).
- **Coming and going:**
  - unlinking and linking again;
  - stopping `mclink`: Minecraft's link drops within 8 s;
  - starting it again: Minecraft sends everything again, and HUD images resume.
- **Traffic:** bytes and messages by type, HUD images a second, and Minecraft's frame rate paced at 60 Hz.

### Step 2: in the game, the link, blocks and the HUD

BlockRender 0.9.0, deployed. At spike 3's corner, with the same save:
- **Linking:** Link pins Minecraft's player at V's camera. The overlay shows both positions.
- **The HUD,** with `set hud 2`: Minecraft's hotbar, crosshair and chat drawn over Night City at the expected size,
  sharp, and the right way up. The typed commands' replies show in its chat.
- **Building by command:** `wall`, `tnt-ahead`, `field` and `wipe` change Night City where the commands put them,
  and the blocks are lit as in spike 3.
- **Latency** from a command's Enter to the frame that draws it.
- **Bursts:** `tnt-cube` and `field`: the worst frame and `Update`'s time. If they hitch, the fix (see "Blocks"),
  then measure again.
- **Frame rates, CPU and memory:**
  - Cyberpunk with Minecraft closed, linked and idle, building, and through bursts;
  - the HUD off, at `hudscale 2`, and at `hudscale 1`.
- **Focus:** the user switches to another window (Claude Code, on the second monitor) for 20 s, then returns. They
  do that between tests anyway. A background recorder of `status.txt` shows whether Cyberpunk kept running and the
  link survived. If Cyberpunk pauses, Minecraft drops the link after 8 s and links again on return.
- **Minecraft closed and reopened** while linked: its blocks come back, and so does the HUD.

### Step 3: in the game, input

- **Build mode:** F8 turns it on, the overlay shows it, and Minecraft's HUD appears (`hud 1`, the default).
- **Blocking, input by input:** the left, right, middle and side buttons, the wheel, and each of 1 to 9. Does
  Minecraft act (a block broken or placed, the HUD's slot changed), and does V stay still (no attack, aim, gadget, or
  weapon switch)? An input Cyberpunk still reacts to gets the fallback (see "Risks"), and is checked again. The
  overlay's slot line should match the HUD's.
- **Building:** placing and breaking blocks on the street by clicking. Walking (Minecraft's player follows), and
  building elsewhere. The floor moves to follow V.
- **Leaving:** each menu key leaves build mode, the HUD goes, and the menu takes clicks normally. F8 turns it back on.
- **Click latency.**
- **Held inputs:** Alt-Tab with a button held releases it in Minecraft.

### Step 4: the results

A results doc in `docs/spikes/`, then `superpowers:finishing-a-development-branch`.

## Requests

As in spike 3, one command a file in `blockrender.request`, atomically written, checked once a second, from the
plugin's folder or the overlay's. New and changed commands:

| Command | Does |
|---|---|
| `link on\|off` | Sets or clears the in-game flag, as the Link and Unlink buttons |
| `build on\|off` | Build mode, as F8: on links first, if it isn't linked |
| `floor` | Sends a fresh floor around V |
| `mc <name>` | Types a named set: `creative`, `tnt-kit`, `tnt-ahead`, `wall`, `tnt-cube`, `field` or `wipe` |
| `mc /<command>` | Types that command, taken verbatim to the end of the line |
| `place ...` | As before, but refused while Minecraft's atlas is loaded |
| `clear` | As before, and also removes Minecraft's sections, which come back as they change, or when Minecraft next sends everything |
| `set <name> <value>` | Adds `translucent` (0 or 1), `teleportstep` (metres), `teleportrate` (per second), `ringmb`, `hud` (0 never, 1 in build mode, 2 always) and `hudscale` (1, 2 or 4) |

## Safety

- **No faults.** As in spike 3, the plugin must never fault. Shared memory is written by another process, so nothing
  in it is trusted:
  - a ring whose head is behind its tail, or more than a ring ahead, is resynchronised to its head and counted;
  - every message must fit the data area from its position, and its payload must fit its type's layout:
    - a section's header (16 bytes) plus vertices × 32;
    - an atlas's header (8 bytes) plus w × h × 4, with w and h from 1 to 16,384;
    - section coordinates within ±2,000,000 across and ±256 up;
  - a message whose framing doesn't fit (its header, or its length) skips the ring to its head, since nothing after
    it can be trusted. One whose payload fails its own checks is dropped by itself, as the framing around it is
    still sound. Both are counted, and logged once a second;
  - a HUD image needs a slot index from 0 to 2, and a width and height from 1 to 3840 and 2160, so it fits its
    slot. The pixels are copied out of the slot before use.
- **The HUD pass never switches block drawing off.** It skips a frame it can't draw (no back buffer, another queue,
  no image yet) and counts it separately. Its draw reads only within the image: a pixel whose texel would fall outside
  it shows nothing.
- **The hook passes through** anything it doesn't fully understand, and checks every size before reading or writing.
- **The ring is always drained,** so Minecraft never stalls. If the mapping can't be created, the link is off, and the
  status and log say why.
- **Nothing stays held.** Leaving build mode, focus loss (`FocusWatch`, on its own thread) and Minecraft going all
  release inputs. Menu keys always reach the game.
- **Off by default.** Until the user links or presses F8, the plugin keeps the mapping's heartbeat and takes what
  Minecraft sends (see "Blocks"), and the hook passes everything through. Drawing stays as spike 3 left it, off until
  switched on, so nothing shows (see "Changes while planning").
- **Two game sides never share the mapping:**
  - the plugin refuses while another live game side holds it, and retries once a second. `mclink` refuses to start,
    and says why;
  - a mapping of the wrong size is refused too, so nothing is read past its end.
- **Memory:** the mapping commits 191 MB of the pagefile when it's made. Only the pages written are ever touched: the
  headers, the rings in use, and the overlay slots Minecraft fills.
- **Builds:** a build that deploys runs only when `tasklist` shows no `Cyberpunk2077.exe`.

## Error handling

- **The mapping can't be created:** logged, and the link is off.
- **The mapping is refused** (another game side running, or the wrong size): retried once a second, with the reason
  in the status.
- **Minecraft's magic or version is wrong:** Minecraft refuses, and logs it, in its own log. The status shows Minecraft
  absent.
- **A rejected message:** skipped as in "Safety", and counted.
- **The atlas can't be built** (bad size): logged, and spike 3's atlas stays.
- **A HUD image is refused:** counted, and the last good one stays on screen.
- **A typed command times out:** the queue drops it, logs which step timed out, and goes on.
- **The input ring is full:** the event is dropped and counted.
- **A bad request:** logged.

## Testing

### Unit tests (ctest, no game)

- **`Rings`:**
  - the seqlocked writes, against the header's offsets;
  - the byte ring reader: wrapping, the pad message, a full ring, and a consumer behind or ahead;
  - the input and event rings, including dropping when full.
- **`Messages`:**
  - a section and an atlas parsed from bytes laid out as `WorldExporter` writes them;
  - each rejection (short payload, oversized counts, a bad size or coordinates);
  - a vertex count cut down to a multiple of 3;
  - translucent triangles dropped, or kept with `translucent 1`.
- **`Atlas`:** the levels' sizes for odd and non-square atlases, and averaged texels.
- **`Overlay`:**
  - taking a frame only when the unread bit is set, and swapping the front slot as SkyCraft's reader does;
  - the reset;
  - refusing slot index 3, and sizes out of range.
- **`Hud`:**
  - whether it shows, for each `hud` setting, with and without build mode and Minecraft;
  - the viewport, and the scale taken from an image's size;
  - the texel for a pixel at each `hudscale`, flipped and not, and none outside the image;
  - the upload buffers: an image written, taken once and read until its fence, a failed write, and the waiting image
    replaced while the GPU reads the rest.
- **`Floor`:** the messages byte for byte against `fake_skyrim.py`'s `floor_tris_payload`, and the regions covered.
- **`Look`:**
  - north, east, up and down to yaw and pitch;
  - the last yaw kept looking straight up or down;
  - the teleport target.
- **`Link`:**
  - teleports by step and rate;
  - the floor moving with V, sent only while Minecraft is there;
  - linking only with V found, and F8's link request;
  - Minecraft coming and going;
  - the menu flag following build mode;
  - a section before Minecraft's atlas dropped, and skipped messages not counting toward `ringmb`.
- **`Commands`:** a command typed against a fake `McState`, a screen already open, both timeouts, and rejected text.
- **`Scancodes`:** the table, including extended keys.
- **`BuildMode`:**
  - every row of the table above;
  - Shift with F6;
  - F8's repeats, and F8 linking first;
  - no forwarding without Minecraft in its world, or while the game isn't in front;
  - a press's repeats and release going where the press went, and what's forgotten when the game loses the front;
  - the side buttons;
  - the hotbar slot, from the keys and the wheel.
- **The router's rewrites,** on synthetic `RAWINPUT` events, with a link in memory: a cleared button keeps its
  movement, a no-op key, and events too short or of another type left alone. Also build mode ending when Minecraft or
  V goes, the focus watch's release, and nothing pushed once the link is taken away.
- **`FocusWatch`'s decision:** a release once on losing the front, and none while it stays lost.
- **`Mapping`:**
  - created under a test name, and its header;
  - reused after a game side that has gone;
  - refused while another live game side's heartbeat is fresh, and when the view is too small.
- **The renderer's waiting uploads,** factored out of `Renderer` so they can be tested: one per section, the latest,
  and none after `ClearSections`.
- **The shaders:** the HUD's compile, and a reflection check of their root constants' layout, as spike 3's tests do
  for the others.
- **The rest:** the new requests, and the status lines.

### `mclink`

Step 1, with the user's Minecraft. Its checks are those of "Step 1".

### In the game

Steps 2 and 3. Every step also checks that CET's overlay and spike 3's mode buttons still work.

## Risks

1. **Rewriting the event doesn't stop Cyberpunk acting on it.** Its code says it does (see "Cyberpunk's input"), but
   only the game can show it.
   **Fallbacks, in order:**
   - change its type to HID (`RIM_TYPEHID`), which Cyberpunk's handler skips, the event's movement with it;
   - for the combat buttons, the gameplay restrictions FreeFly applies as status effects (`GameplayRestriction.NoCombat`
     among them).

   Returning the event as no data, once the first fallback, can't work: the game reads the buffer whatever the call
   returns. Step 3 checks each input, so the fallback is chosen per input.
2. **Cyberpunk stops running while another window is in front,** such as Claude Code on the second monitor, which the
   user answers in between tests. The heartbeat stops, Minecraft drops the link after 8 s, and links again on return.
   Teleports and typed commands pause too. Held inputs are still released, because `FocusWatch` runs on its own
   thread. That's acceptable for this spike, but step 2 says which it is.
3. **Minecraft's spinning costs Cyberpunk frame time.** Step 2 measures it. If it does, Minecraft runs at below-normal
   priority. That's a Task Manager or launcher setting, since the mod is unchanged.
4. **Bursts hitch.** Suballocation and an upload budget, inside the spike (see "Blocks").
5. **The atlas's mip levels take a long frame at link time.** That's once per link and acceptable. If it isn't, they're
   built on a worker thread.
6. **Placement is off when V crouches or moves.** Minecraft stands at V's camera, less its standing eye height, and
   teleports follow at most 10 times a second. The settings tune both, and step 3 says whether it's noticeable. It's
   also off inside or under Minecraft's blocks, where Minecraft crawls, its eye 0.4 above its feet (see "Changes from
   step 2"). Step 3 builds on the open street.
7. **Cyberpunk's menus read keys some other way** (window messages). Menu keys leave build mode before a menu opens,
   so forwarded keys never reach a menu.
8. **The user's Minecraft binds chat to another key.** Typed commands then time out, and the status says so. The
   instance should keep T for chat.
9. **SkyCraft or the game updates.** A new protocol version is refused by Minecraft and shown in the status. The jar
   is pinned at 0.1.2, and the game at 2.31.
10. **The flat floor.** Blocks land at V's feet height, not the street's. That's known and accepted until Night City's
    collision comes.
11. **A click inside Minecraft's own window acts in its world.** Its window still takes real input (see "SkyCraft's
    mod, once linked"). Hidden, as in this spike, it can't be clicked. If it's ever shown again
    (`-Dskycraft.showWindow=true`), it's for looking, not clicking.
12. **The HUD costs both games frame time.** Minecraft reads its frame back every frame at the viewport's size, and we
    copy and upload each new image. At `hudscale 2` that's 3.7 MB an image. Step 2 measures both games with the HUD
    off, at `hudscale 2` and at `hudscale 1`. `hudscale 4` quarters it again if needed, at the cost of a coarser HUD.
13. **The image's alpha isn't premultiplied.** Its edges would then look wrong over Night City: too bright where it's
    translucent. Step 1 counts any pixel with a colour above its alpha. If there are any, the blend changes to straight
    alpha. Step 1 found one, at an edge of Minecraft's first-person shield, and the rest premultiplied, so the blend
    stays premultiplied (see "Changes while planning").
14. **Minecraft's HUD covers Cyberpunk's.** Both draw at the bottom of the screen, and ours goes on top. It shows only
    in build mode by default, and `hud 0` hides it.

## Changes from the deep review

The user asked for a deep self-review of the first draft. Each claim was checked against its source: SkyCraft's code
at `bfcaf17`, the user's 26.3 jar (with `javap`), Cyberpunk's files, and BlockRender 0.8.0's code. The sections above
already include these changes.

**Corrected:**
- **The question's latency.** Minecraft acts on clicks only in its 20 Hz client tick, so "a frame or two" can only be
  promised after Minecraft changes a block. Latency is now reported in two halves, Minecraft's and ours.
- **`-Dskycraft.quitWithSkyrim=false` is required, not optional.** Without it Minecraft quits 5 s after the game
  side's process ends, which would end step 1's restart check and every Cyberpunk restart.
- **Reusing the mapping.** `mclink` refused whenever the mapping existed, but Minecraft keeps it open between game
  sides, so `mclink` could never start twice. Both game sides now refuse only a live other game side, or a mapping of
  the wrong size, and otherwise reset and reuse it.
- **The renderer's waiting uploads.** While drawing is off, `Update` uploads nothing, and `UploadSection` queued every
  update, so a linked Minecraft would have grown that list without limit. Now it keeps the latest mesh per section,
  and `ClearSections` empties it.
- **The atlas can change.** `AtlasData()` promised a pointer valid for good, since the atlas was loaded once.
  `SetAtlas` now replaces it under the renderer's lock.
- **Focus.** The check ran in `OnUpdate`, which may stop while Cyberpunk is in the background. The hook now checks the
  foreground before forwarding each event, and `FocusWatch` releases held inputs from its own thread.
- **F8 and threads.** F8 arrives on the hook's thread, but linking needs `LocatePlayer` on the main thread. F8 now
  only asks, and the main thread links.
- **Where a block lands on the floor:** the cell at `floor(hit + normal × 0.4)`, between 0.6 m below V's feet and
  0.4 m above them, not "at V's feet height".
- **Skipped messages.** The per-frame scene message carries every particle, so it grows during explosions. Skipped
  messages are stepped over without copying, and don't count toward `ringmb`.
- **Smaller fixes:**
  - the floor goes out only while Minecraft is there;
  - a section before Minecraft's atlas is dropped;
  - looking straight up or down keeps the last yaw;
  - the TNT command set `clear` became `wipe`, apart from the plugin's own `clear`;
  - "place and field refused" now names spike 3's `place` commands;
  - "no DirectInput" became "imports no DirectInput".

**Added:**
- **What else the mod does,** in "SkyCraft's mod, once linked":
  - it reads back its HUD every frame at its window's size, which this spike didn't use at the time. It does now (see
    "Added after the review: Minecraft's HUD");
  - its window still takes real input, so a click in it can act in the world (risk 11);
  - the per-frame scene message.
- **What build mode takes from Cyberpunk:** every use of the mouse buttons and the wheel, dialogue choices included.
- **The mapping's pagefile commit, 191 MB.**
- **The status:** Minecraft's eye beside V's camera, and why a mapping was refused.

**Checked, and found as written:**
- **Clicks on the floor need only triangles:** the crosshair pick goes through `SkyClip` with no other condition.
  The cast is two-sided, so winding doesn't matter, and only voxel messages mark regions known, so the player stays
  held.
- **A floating, held player isn't kicked:** the server allows flight while linked.
- **Attacking our floor does nothing:** it digs only for diggable triangles.
- **Minecraft keeps rendering, and so exporting, while minimized:** `renderFrame` calls `GameRenderer.render()` every
  frame.
- **The command sets fit:** `/fill` allows 32,768 blocks by default (`max_block_modifications`), and SkyCraft's world
  is created with commands allowed.
- **Chat opens one client tick after T:** `Gui.handleKeybinds`.
- **The protocol's offsets and the mapping's size** were computed from the header.
- **The vertex matches BlockRender's,** byte for byte.
- **The user has rebound no Cyberpunk key,** so the default bindings hold.
- **`mclink`'s atlas dump:** spike 3's DDS writer takes RGBA8, and `view_dump.py` decodes it.

## Added after the review: Minecraft's HUD

**Why:** the user asked why not use the HUD image the mod reads back every frame anyway, or whether that would grow
the scope too much. The evidence said it wouldn't, by much:
- **Minecraft's side** already does all of it (`FrameExporter`).
- **SkyCraft's Skyrim side** shows what drawing it takes (`Overlay.cpp`): one full-screen triangle at Present, point
  sampling, premultiplied alpha, rows flipped.
- **Spike 3's `present` path** already has the hard D3D12 parts: the back buffer, its barriers, a pipeline per format,
  and our own list on the main queue just before Present.
- **The shader** reads the image as a buffer, as it already reads the atlas, so the root signature doesn't change and
  no textures or descriptor heaps come in.
- **Size:** about 300 to 400 lines with tests, which is part 7 of the scope pass.

**What it changed** (the sections above already include it):
- **Decisions:** the scope is parts 0 to 4 and 7. Minecraft's window stays hidden, SkyCraft's default, instead of
  minimized: the HUD shows what the window was for. That also avoids a path nobody has tried: a minimized window
  SkyCraft reports as not minimized. `-Dskycraft.showWindow=true` is gone from the JVM arguments.
- **The protocol:** the overlay's triple buffer is read, as SkyCraft's `Link::AcquireOverlayFrame` reads it, and reset
  for a new Minecraft process (see "Changes while planning"). `SkyState`'s viewport asks for the back buffer's size
  over `hudscale` (2), 1280×720, instead of 0. The GUI then looks as Minecraft's automatic scale would make it at
  2560×1440, with a quarter of the bandwidth.
- **Components:**
  - `link/Overlay`, the triple buffer's reader;
  - `render/Hud`, the pass's rules;
  - the renderer's `SetHudImage` and HUD pass;
  - the HUD's shaders;
  - the overlay's HUD button.
- **A new section, "The HUD":** what shows, when (`hud`: never, in build mode by default, or always), the size, the
  path and the blending.
- **The steps:**
  - step 1 checks the image: its size, its flag, how it looks, and that its alpha is premultiplied;
  - step 2 shows it in game, and measures its cost on both games;
  - step 3 uses it to see the hotbar while building;
  - step 2's focus check now switches to another window (Claude Code), since Minecraft's window is hidden.
- **Requests:** `set hud` and `set hudscale`.
- **Safety and error handling:** the image's checks. The HUD pass skips its own frames, and never switches block
  drawing off.
- **Risks:**
  - the HUD's cost (12);
  - premultiplied alpha (13);
  - covering Cyberpunk's HUD (14);
  - focus (2), now about Claude Code's window.
- **Non-goals:** using Minecraft's screens from Cyberpunk, a mouse cursor, and an exact inverted crosshair stay out.

**Checked:**
- **The triple buffer's rules:** in SkyCraft's `Link.cpp` (`AcquireOverlayFrame`, `ResetOverlay`) and `SkyLink.java`
  (`publishOverlay`, and the writer restarting at slot 1).
- **The image:** RGBA8, uploaded unconverted, premultiplied alpha, and a bottom-up flag, all from SkyCraft's
  `Overlay.cpp`.
- **The GUI scale:** Minecraft 26.3's automatic rule (`Window.calculateScale`) is the largest scale leaving at least
  320×240 GUI pixels.
- **Minimizing:** `Window.onFramebufferResize` ignores a 0×0 size, and `renderFrame` reconfigures the GPU surface only
  when the window isn't minimized, which SkyCraft's mixin denies while linked.

## Changes while planning

Writing and verifying Part 1's code corrected these details. The sections above already include them.
- **Rejected messages:** only a message whose framing doesn't fit the ring moves the tail to the head. One whose
  payload fails its checks (a section claiming more vertices than it carries, say) is dropped by itself.
- **TNT needs a block under it:** in SkyCraft, only the local player collides with the floor's triangles. Every other
  entity collides only with voxel regions (`BlockCollisionsMixin`), and this spike never sends any. So TNT summoned in
  the air falls through the floor into the void, and explodes far below. `tnt-ahead` puts a stone block under its TNT,
  and step 1's TNT lands on a block. Dropped items fall through the floor too.
- **"Ahead" is level:** `^ ^ ^4` follows the look's pitch as well as its yaw, so whenever V looked down, `tnt-ahead`,
  `wall` and `tnt-cube` would have built underground. Their commands start with `rotated ~ 0`, which keeps the yaw and
  levels the pitch.
- **The overlay's reset:** SkyCraft's `Game.cpp` resets the overlay's triple buffer whenever Minecraft connects or
  comes back. The link resets it only for a new Minecraft process, whose writer starts over at slot 1 and publishes
  only once it's in its world. The first Minecraft the link sees needs no reset: `Mapping::Open` reset the buffer
  before Minecraft could see the new game side, and Minecraft starts its writer over when it does. One coming back
  kept its slots. A reset in either case could undo an image Minecraft had just published (it ships one left from
  before as soon as it relinks), leaving its writer and the link's reader on one slot, with torn images from then on.
- **`mclink` refuses rather than retries,** as its own section says; "Safety" had it retrying like the plugin.
- **`mclink`'s input:** a script, one action a line (`tools/mclink/McLinkScript.h`), so step 1's runs can change
  without a rebuild.

Planning Part 2, after step 1, corrected these:
- **The mapping is created at the first update, not at load.** The heartbeat comes from `Link::Update`, once a frame,
  and the game doesn't update until it has loaded: in spike 3's runs, the D3D12 device alone came 13 s after load. A
  mapping made at load would let a running Minecraft link at once and send its atlas and sections into a render ring
  nothing drains yet, then drop the link 8 s later, only to link again.
- **The overlay's reset,** in "The protocol", still said the triple buffer starts over whenever Minecraft connects or
  comes back. It starts over only for a new Minecraft process, as above.
- **Pixels outside the HUD image show nothing,** instead of the nearest texel. An image smaller than the back buffer
  at its scale (one Minecraft drew before it resized, or at a `hudscale` that doesn't divide the back buffer) then
  leaves the rest of the screen alone, rather than smearing its last row and column across it. The draw still reads
  only within the image.
- **The HUD's scale comes from each image:** as many whole back-buffer pixels a texel as fit. An image Minecraft drew
  at the old size, before it caught up with a new `hudscale`, is still drawn whole. Once it has, that's `hudscale`.
- **Premultiplied blending stays** (risk 13). Step 1 found one texel with colour above its alpha in a whole image, at
  an edge of the first-person shield's model, and the rest premultiplied. Straight alpha would darken every
  translucent pixel of the HUD to fix one texel, which premultiplied blending only brightens a little.
- **V found means V's camera found.** `LocatePlayer` can find V without the camera, but the teleport target and the
  look need it. Without it the plugin passes the link no pose: the in-game flag clears, the last look is kept, and
  there's no teleport. "Look" had only the last two.
- **Before linking, Minecraft's blocks are taken,** as "Blocks" has it: the ring is drained, and the atlas and the
  sections are kept as they come, so they're there as soon as the user links. Drawing stays as spike 3 left it, so
  nothing shows until then, and the renderer uploads nothing until drawing is first switched on. "Safety" said the
  plugin only kept the heartbeat.

Planning Part 3, after step 2, found or changed these:
- **How the game reads input, from its code.** Before writing the hook, the executable's callers of the Raw Input
  imports were disassembled (`Cyberpunk2077.exe` 2.31, with capstone; the addresses are the image's, based at
  0x140000000):
  - **the registration** (at 0x14016c7f2): the keyboard (usage page 1, usage 6) and the mouse (1, 2), each with flags 0
    and no target window. So there's no `RIDEV_INPUTSINK`, and no Raw Input in the background, which "Focus" had left
    unchecked; and no `RIDEV_NOLEGACY`;
  - **the one reader** (at 0x1407cb893, in the input system's window-message handler): for each `WM_INPUT`,
    `GetRawInputData` with `RID_INPUT` into a 48-byte buffer on the stack, without asking the size first. It ignores
    the call's result, and goes by the buffer's type: mouse, keyboard, or anything else skipped;
  - **the mouse** (0x1407cbd90): the movement from `lLastX` and `lLastY`; the wheel from `RI_MOUSE_WHEEL`, with
    `usButtonData` / 120 in whole notches; and the five buttons from `usButtonFlags`, each press or release acted on
    only if it changes that button's state;
  - **the keyboard** (0x1407cc508): by virtual key. A virtual key of 0xFF or 0 is skipped at once, and so is a press
    of a key already down. The scan code only tells the left and right Shift apart;
  - **other messages:** the same handler takes `WM_ACTIVATE`, `WM_SETFOCUS`, `WM_KILLFOCUS`, `WM_SETCURSOR`,
    `WM_MOUSELEAVE`, `WM_WTSSESSION_CHANGE` and Alt's `WM_SYSCOMMAND`, but no legacy key or mouse message.

  So the spec's rewrites hide each input as intended: the cleared button or wheel flag with the movement kept, and the
  key with virtual key 0xFF. Risk 1's first fallback, returning no data, couldn't work, since the result is ignored.
  Changing the type to HID would, as any other type is skipped. Step 3 still checks each input in the game.
- **A press's repeats and release go where the press went.** "Input"'s table decided each event by build mode alone. A
  button held when build mode went on (V aiming, say) would then have had its release hidden, and the game would
  have kept it held until build mode ended. Build mode now remembers where each press of its inputs went.
- **The side buttons go to Minecraft too.** The user's Minecraft picks hotbar slots 8 and 9 with them (`key.mouse.5`
  and `key.mouse.4` in the `CyberCraft` instance's options, as in nearly all their instances), and Cyberpunk binds
  neither: its mappings use only the left, right and middle buttons and the wheel, and the user rebound nothing. So
  in build mode they're forwarded as SDL's buttons 4 and 5, as Minecraft 26.3 numbers them (`InputConstants`), and
  hidden from the game. The user chose this while Part 3 was planned. The keys 8 and 9 are still forwarded, though
  this instance binds no slot to them. A side button isn't a latency mark, and makes the overlay's slot unknown.
- **F8 while unlinked links, then turns build mode on:** the hook only asks; the main thread links, and turns build
  mode on after the link's next update, once it knows whether Minecraft and V are there. `build on` does the same.
  Build mode also needs V found, and ends when Minecraft leaves its world.
- **The router.** `BuildMode` stays a pure class. What the three threads share is a second, tested class,
  `input/Router`: the `RAWINPUT` reads and rewrites, forwarding through the link, and one lock. Only the patch and the
  focus watch's thread are in `render_runtime` (`input/RawInputHook`). "Threads" said atomics; one lock is simpler,
  and the hook takes it only for buttons, the wheel and keys, never for mouse movement.
- **The hotbar slot** is unknown until a number key sets it, and the wheel moves it as Minecraft 26.3's
  `ScrollWheelHandler` does (its bytecode, in the user's jar).
- **The status's build line** counts since the start, and adds the last input forwarded and the events the hook has
  read.
- **At unload,** build mode goes off, the patch is put back, and the focus watch is told to stop. Then the router
  loses the link, so no thread can push into the mapping once it's unmapped. Nothing waits for the focus watch's
  thread.

## Changes from step 2

Step 2's game run, in Part 2 of the plan, found these. The sections above already include them.
- **The yaw never wraps.** It was in (−180°, 180°], so it jumped from 180 to −180 whenever V turned through north.
  SkyCraft puts it straight into the player's yaw, where Minecraft's own never wraps, and Minecraft sways the
  first-person hand by the yaw's change. So the held item in the HUD swung and settled, at north only. Each yaw
  is now the one within 180° of the last.
- **Minecraft's eye leaves V's camera among blocks.** V doesn't collide with Minecraft's blocks, so Minecraft's
  player, held at V's feet, can be inside them, as in `field`'s layer. Minecraft then crawls, its eye 0.4 above its
  feet: for about a second after teleports inside blocks, and for as long as a block overhangs 1 to 2 m above the
  feet. A click then casts from that eye, and lands short. An `mclink` run measured it (`tools/mclink/eye.txt`, and
  the results' "Minecraft's eye among blocks"). In the open, the eye stays at V's camera. Commands work from the
  held feet, not from the floor.
