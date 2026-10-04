# Handoff: brainstorm Entities, Night City's NPCs and moving props with Minecraft

Written 2026-10-04, at the end of the session that planned and ran spike 5's Part 3, closed spike 5, and made two
follow-ups. The user's next step: **a brainstorm for NPCs and moving props.** Nothing about it is decided. Read this
first.

## Start here

- **Repo and branch:** `D:\Cyberpunk-Minecraft`, on `main`. Spike 5 (`ef324e0`) and its follow-ups (`6d8b71c`) are
  merged, by fast-forward; this handoff is committed on `main` after them. No spike branch is open: the brainstorm's
  spec starts one, as each spike has. The repo has no git remote.
- **What's next:**
  1. **`superpowers:brainstorming`** with the user, on "NPCs and moving props", their words. The brainstorm decides the
     scope, and whether it's spike 6. Start from "Questions to start with" below.
  2. **Then as before:** the spec, agreed with the user section by section, then a deep self-review; the plan
     (`superpowers:writing-plans`), every code step compiled and tested in a scratch clone, each new test broken once,
     the tasks replayed in a fresh clone exactly as written; then execution inline.

  **Expect the user to ask for a deep review** of the spec and the plan: trace every claim to its source, simulate the
  in-game steps, and make each new test fail once.
- **No sub-agents:** the user wants everything done inline.
- **Read for background, in this order:**
  - this handoff;
  - **the results**, `docs/spikes/2026-10-03-collision-and-movement-results.md`: the answers at its top, "Step 3, in the
    game" (the guard rail), "Step 4, in the game: V against blocks" (NPCs, cars and bullets), and "After spike 5";
  - **spike 5's spec**, `docs/superpowers/specs/2026-10-03-collision-and-movement-design.md`: "Part 2" (the exporter
    keeps static actors only), "Part 3", and its risks (risk 4: telling props from characters by their filters);
  - **SkyCraft's source** (below): `combat/SkyCombat.java`, `combat/SkyrimActorEntity.java`, the client's `ProxySync`,
    and the protocol header, `extern/skycraft/skycraft_protocol.h`: the actor table, the event ring, the world entities
    and the render ring;
  - **the PhysX audit**, `docs/spikes/2026-10-01-physx-abi-audit.md`, with its two addenda: which calls are audited;
  - `spikes/block-render/src/Puppet.*`: the game's script calls the plugin already makes (teleport, yaw, health).
- **A first message to start the next session with:**
  > Read docs/superpowers/handoffs/2026-10-04-entities-npcs-and-moving-props.md and let's brainstorm NPCs and moving
  > props.

## What spike 5 found about NPCs and moving props

- **NPCs against Minecraft's blocks** (step 4): with bodies on, NPCs feel the blocks' PhysX bodies, but their navigation
  wins: they walk through a wall of blocks, or are lifted on top of it. The bodies aren't in the game's navigation, so
  NPCs don't route around them.
- **Cars and bullets:** a car stops at a block's body, and so do ordinary bullets. Ricochet rounds went through the test
  box once, which didn't count.
- **The guard rail** (step 3): V and Minecraft's player walked through a breakable guard rail.
  - **What it is:** a chain of kinematic actors, each a 2.9 m section of capsules and boxes standing 1.1 m on the path,
    both simulation and scene-query shapes;
  - **its filters:** most shapes have query filter `0:0:4480008:0` and simulation filter `8:0:13cc:80000000`, and one
    box in each section `0:0:1c000:0` and `5c000:0:40000:80000000`. Spike 1's census at spot B counted 85 shapes with
    the rail's query filter;
  - **why Minecraft doesn't have it:** the exporter keeps static actors only, by design.
- **Characters' filters:** V's controller is a kinematic capsule of radius 0.32, query filter `0:0:2000:0`, simulation
  filter `1:0:41ce:0`; a second kinematic capsule on V, radius 1, has query filter `0:0:400001:0`. Both differ from the
  rail's, so filters could tell props from characters (spike 5's risk 4). NPCs' capsules weren't looked at.
- **When the scene can be touched** (Task 46's query watch, 132 s of walking, driving and shooting): 16 threads made
  about 6,000 adds, 5,800 removes, 9,100 steps and fetches and 930,000 queries, and none came while `OnUpdate` ran but
  BlockRender's own. So reading moving actors' poses in `OnUpdate` is as safe as the exporter's reads, and Part 3's
  bodies already write there.
- **What exists to build on:**
  - **the exporter** (`physx/Export`): reads scene 0's static actors around V in batches, within 1.5 ms a frame,
    checking every object against the audited vtables. Moving props would need poses of a few actors each frame
    instead;
  - **the bodies** (`physx/Bodies`): adds PhysX actors of ours, made with audited calls, checked as ours before each
    touch, and moved with the scene by the game at origin shifts. The template if something of Minecraft's must be
    solid in the game;
  - **the audit:** the actors' and shapes' getters the exporter uses are audited, and so is
    `PxRigidBody::getRigidBodyFlags` (slot 47), which tells kinematic actors apart; `PxRigidDynamic`'s vtable is known
    (`0x20b2e8`). A kinematic actor's target pose (`getKinematicTarget`) isn't audited yet.

## SkyCraft's entity channels (protocol version 11)

Checked against SkyCraft's source on 2026-10-04. All exist in the protocol and in SkyCraft's unchanged mod; BlockRender
uses almost none of them.
- **The actor table** (game → Minecraft, seqlock, at `0x12000`): up to 256 records of 64 bytes: `formId`, flags
  (hostile, dead, essential, in combat), feet (Minecraft axes), yaw, width and height in blocks, health fraction, level
  and a 24-byte name.
  - **On Minecraft's side** (`SkyCombat`), each record becomes a `SkyrimActorEntity`, an invisible `LivingEntity`
    stand-in placed from the table every tick, and on the client every frame (`ProxySync`), so the crosshair and reach
    line up.
  - **What it does:** Minecraft's own combat hits it (swords, crits, sweeps, bows, tridents), and `SkyCombat` forwards
    one hit a tick as `kEvHitActor`. It also presses pressure plates and trips tripwires.
  - **It isn't solid:** no gravity, `noPhysics`, `isPushable` false and `canBeCollidedWith` false.
- **The event ring** (Minecraft → game, at `0x17000`): `kEvHitActor` (form ID, damage, knockback, weapon class, flags),
  `kEvPlayerDied`, `kEvExplosion`, `kEvArrowStuck`, and `kEvSkillUse` (Skyrim's skills).
- **`kInHurt`** in the input ring (game → Minecraft): the game hit Minecraft's player, with the kind (melee, projectile,
  magic, other), damage × 100, the attacker's form ID, and blocked or power-attack flags.
- **The world entities** (Minecraft → game, seqlock, at `0x1C000`): up to 160 things the game draws itself: arrows,
  dropped items, tridents, dropped blocks, block-breaking cracks and contact shadows, each with a position, rotation,
  scale and atlas rectangles; the table also carries the targeted block's outline.
- **The render ring's entity messages** (Minecraft → game): `kRenTexture` (skins, armour), `kRenAvatar` (the player's
  model each frame, none in first person), `kRenScene` (every other entity and all particles each frame), and
  `kRenRagdoll` (the player's body parts, about once a second).

### What BlockRender does with them now

- It clears the actor table, the event ring and the world entities when it creates the mapping, and acts on one event,
  `kEvExplosion`, which it logs.
- Its drain counts textures, scenes and ragdolls, but draws only sections. Offline, `mclink` saw 289 scene messages in
  24 s.
- Nothing is written to the actor table, and `kInHurt` is never sent.

## New since spike 5: a mod of ours on Minecraft's side

`minecraft/cybercraft-tweaks`, CyberCraft Tweaks 1.2.0, installed in the `CyberCraft` instance beside SkyCraft. It's
built with plain `javac` against jars the Modrinth App already keeps (Minecraft 26.3, Fabric Loader, the mixin library,
Fabric API's modules, SkyCraft): no Gradle and nothing downloaded. It has mixins into SkyCraft's classes, a Fabric
entrypoint, and Fabric API events and attachments, and it asks for SkyCraft 0.1.2 exactly.

So Minecraft-side changes no longer mean changing SkyCraft: a mixin could make `SkyrimActorEntity` solid, as a boat is,
or the mod could register an entity type of its own. Registering one needs more of Minecraft's API, still on those
jars. What it does now: blasts, tools and reveals don't dig into Night City's ground, and old dug cells are dropped as
chunks load.

## Questions to start with

A starting list, not a decision; agree the scope with the user.
1. **Which direction first:**
   - **the game's NPCs and moving props in Minecraft:** so that Minecraft's player, and V in build mode, collide with
     them, and Minecraft can hit them. NPCs through the actor table; props as solid stand-ins, as blocks, or as the
     exporter's voxels refreshed;
   - **NPCs against Minecraft's blocks in the game:** their navigation ignores our bodies. Does the game have dynamic
     navigation obstacles? Or does that stay a non-goal, as in spike 5?
   - **combat between the two:** `kEvHitActor` applied to NPCs through the game's scripts, as `Puppet` calls them;
     NPCs hurting Minecraft's player through `kInHurt`; deaths;
   - **Minecraft's own entities in Night City** (`kRenScene`, `kRenAvatar`): drawing them, and perhaps bodies so V
     collides with them. Perhaps later.
2. **Finding NPCs and props in the game:** through the game's systems, from RED4ext or script calls (NPC entities'
   positions, health, a stable ID for `formId`), or through PhysX (characters' capsules, kinematic and dynamic actors,
   told apart by filters)? The actor table wants a feet position, yaw, width and height, health and flags.
3. **Solid stand-ins:** Minecraft's entities collide as axis-aligned boxes, so a rail running at an angle needs several
   small ones, or one box blocks far more than the rail. Boats show the way: entities you can stand on. Who places them:
   SkyCraft's table every tick, or the mod's own?
4. **Which props, and how many:** rails and barriers are kinematic until hit; cars and crates are dynamic and fast. NPCs
   are at most 256 records. How often are they read and sent: every frame, every tick?
5. **What can be tested without the game:** `mclink` can write an actor table and read events against SkyCraft's real
   mod, so stand-ins and hits can be checked offline first, as spike 5's steps 1 and 3 were.

## SkyCraft's source

SkyCraft 0.1.2 at `bfcaf17`, cloned in an earlier session to:

`C:\Users\BLOXY\AppData\Local\Temp\claude\D--Cyberpunk-Minecraft\818fd1fd-489f-482d-9873-88e6752bad04\scratchpad\skycraft`

It was still there on 2026-10-04. It's a temporary folder; if it's gone, cloning again is a download, so ask the user
first. The repo keeps only the protocol header (`extern/skycraft/`).

**Search the whole of SkyCraft.** Spike 5's follow-ups first concluded SkyCraft had no switch for its digging, from a
partial unpacking of its jar without the client classes. It has one, "Skyrim destruction" in the pause menu. Read the
source, or unpack the whole jar.

## The game and Minecraft, right now

- **BlockRender 0.16.0** (`d5ed7e0`) is in `E:\Cyberpunk2077\red4ext\plugins\BlockRender\`:
  - **bodies are on by default:** Minecraft's blocks around V are solid for V (`set blocks 0` turns them off);
  - **requests:** `set blocks`, `box on|off` (a test box), `scenewatch on|off` (now counting queries too), `shapes`,
    `set ground floor|city`, `set exportbudget`, `set turn` and `drivelog <seconds>`, besides spike 4's;
  - **the overlay** shows one row of buttons (Link, Unlink, Blocks, Box, Turn) and five lines, with "More controls" and
    "Full status" folded;
  - **`turn.txt`** keeps the turn ratio, −0.0500 degrees a count, between sessions;
  - **DRED is on:** `settings.txt` there has `dred = 1`, since a GPU crash in step 4 whose cause isn't settled (the
    results' "The GPU crash"). On a device removal, the log gets DRED's report. Delete the file to turn it off.
- **Spike 1's probe 0.4.0** (`PhysXProbe`) and its `physxprobe_hud` overlay are installed: it shows V's distance to
  PhysX's origin lines. **FreeFly** is installed too.
- **Minecraft:** the Modrinth App's `CyberCraft` instance: Minecraft 26.3, Fabric Loader 0.19.5, Fabric API 0.161.0,
  SkyCraft 0.1.2, CyberCraft Tweaks 1.2.0. SkyCraft's "Skyrim destruction" button is untouched; the mod makes it moot.
  The world keeps the user's builds near spike 3's corner; the blasts' leftover blocks were cleared.
- **The repo's CMake cache** has `CP2077_DIR` empty, so builds don't install into the game.

## Rules to keep

- **No sub-agents:** work inline.
- **The user does every in-game and Minecraft action:** launching and quitting both games, loading, moving, keys,
  clicks and screenshots. Never drive their mouse or keyboard; ask, then wait for them to say they're done.
  - **On-screen aids:** give them when a test needs positions, headings or counts. The overlays show V's position,
    heading, the Minecraft block at V's feet, the nearest structure, and the origin lines.
  - **Name what's invisible:** the test box and anything else BlockRender doesn't draw. Say so before the user looks.
- **Deep reviews:** trace every claim to its source (SkyCraft's code, the protocol, the game's binaries), simulate the
  in-game steps, and break each new test once to prove it can fail.
- **Settle important unknowns** before moving on, rather than leaving them as next steps. Offer that.
- **Testable steps first:** sequence the work so every step can be checked right then.
- **Builds and the game:**
  - never build with `CP2077_DIR` set, or deploy, while Cyberpunk runs. `tasklist | grep -i -c "cyberpunk2077.exe"` must
    print `0`; the process can take a few seconds to go after the user quits;
  - set `CP2077_DIR` only to deploy, and clear it straight after;
  - `mclink` needs Cyberpunk closed, and a check that runs it "without Minecraft" needs Minecraft closed too;
  - the mod's jar can't be replaced while Minecraft runs with it loaded: ask the user to stop Minecraft first.
- **No faults:** the game's crash handler sees a fault before any `__try/__except`. Check every pointer, size and
  vtable. If the game freezes, read the logs, the newest `ReportQueue` folder and its `gpucrash-*.log`, and force-close
  it only when the user asks.
- **Downloads** need the user's explicit permission first.
- **Commits** end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Spike work goes on the spike's branch;
  handoffs between spikes have gone on `main`.
- **Ask before deleting branch `spike/player-puppeting`** (spike 2's handoff).
- **After the spike:** `superpowers:finishing-a-development-branch`. The user has chosen a local fast-forward merge to
  `main` every time so far; ask again.

## Gotchas

- **Bash's working directory:** a `cd` in a Bash call moves it for later calls. Wrap it in a subshell,
  `(cd ... && ...)`.
- **Backslashes:** the Bash tool turns `\\` into `\`. Write files with the Write or Edit tools when they hold
  backslashes.
- **Tool paths:** `cmake`, `ctest` and `dumpbin` aren't on `PATH`; the plans' build steps have their full paths. `luac`,
  `javac`, `javap` and `jar` are.
- **Python:** `uv run --no-project`. Give it a script file: a heredoc and a `< /dev/null` together hang it.
- **Tests:** run `render_tests.exe` from `spikes/block-render`; from elsewhere its fixture files aren't found.
- **The plan tooling** is in `tools/plans/`, now with Part 3's versions, and the PhysX slot audit in
  `tools/plans/physx-audit/`. Their paths point at that session's scratchpad (`verify3/`), so adapt them first.
  `mutate.py` runs its whole list when imported.
- **Waiting on the game:** a foreground `sleep` is blocked. Record the status in the background with
  `spikes/block-render/tools/status_watch.py <seconds> <interval> "<keys>"`, the keys a regular expression such as
  `"bodies|solids"`, and read the recording afterwards. The status is written once a second.
- **Request files:** write them atomically (a temporary name, then renamed), and wait for the status's `request=` line
  before the next. `mc` commands with relative coordinates (`~`) follow Minecraft's player, which moves with V: use
  block coordinates for anything placed and later removed.
- **Minecraft:**
  - **running it:** the user starts and stops it from the Modrinth App; its window hides while linked. Its log is
    `%APPDATA%\ModrinthApp\profiles\CyberCraft\logs\latest.log`, and system chat lands there as `[CHAT]` lines. A
    swapchain warning on every link is normal: SkyCraft hiding the window;
  - **its world is saved:** whatever a test places or blows up stays. Clear it afterwards, by block coordinates.
- **Logs:** each plugin writes one per launch in `E:\Cyberpunk2077\red4ext\logs\`.
- **Git:** for a fast-forward merge without touching the working tree, `git fetch . <branch>:main`, then
  `git checkout main`.
