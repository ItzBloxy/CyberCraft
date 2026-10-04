# Handoff: Spike 5: Entities (Part 3's last work, merged with entities)

Written 2026-10-04, at the end of the session that planned and ran spike 5's Part 2 (Night City's shape in Minecraft)
and closed it. The user asked for what's left of Part 3 to be merged with a new topic, entities, as "Spike 5:
Entities". Read this first.

## Start here

- **Repo and branch:** `D:\Cyberpunk-Minecraft`, branch `spike/collision-and-movement`. Spike work stays on the branch,
  never on `main`. The repo has no git remote.
- **Where spike 5 stands:**
  - **Part 1 is done** (`fc23bc3`): in build mode the user moves as Minecraft's player, and V follows every frame;
  - **Part 2 is done** (`4a599a9`): Minecraft gets Night City's own collision around V. Step 3 passed offline and in
    the game, after one fix;
  - **Part 3, V against blocks, isn't planned yet.** The plan stopped at Task 37, step 2 ("Plan Part 3").
- **What this handoff starts:** a brainstorm. Entities are new scope, and the user said the approach "needs more
  brainstorming", so nothing about them is decided. Part 3's own design and its two follow-ups are settled, below.
  The brainstorm settles how they fit together.
- **Process:** as spike 5's parts went:
  1. `superpowers:brainstorming` with the user, on the merged scope;
  2. the spec's changes: Part 3 kept or reshaped, and entities added. Whether that's a new part of
     `docs/superpowers/specs/2026-10-03-collision-and-movement-design.md` or a new spec is for the brainstorm;
  3. `superpowers:writing-plans`, every code step compiled and tested in a scratch clone first, then the tasks replayed
     in a fresh clone (see `tools/plans/README.md`);
  4. `superpowers:executing-plans`, inline;
  5. in-game verification with the user;
  6. results in `docs/spikes/2026-10-03-collision-and-movement-results.md`, or a new results doc if it's a new spec;
  7. `superpowers:finishing-a-development-branch`.

  **Expect the user to ask for a deep review** of the spec and the plan before each goes ahead.
- **No sub-agents:** the user wants everything done inline, plan execution included.
- **Read for background, in this order:**
  - this handoff;
  - **the results**, `docs/spikes/2026-10-03-collision-and-movement-results.md`: Part 1's verdict, then Part 2's three
    sections: "Step 3, offline", "Step 3, the opening check" and "Step 3, in the game";
  - **the plan**, `docs/superpowers/plans/2026-10-03-collision-and-movement.md`: "How Part 2 went" at its end, and
    Part 2's introduction, whose layout the next part should follow;
  - **the spec:** "Part 3: V against blocks", "Step 4" under "The steps", "Requests", "Safety", "Risks" and "Changes
    while planning";
  - **the protocol**, `extern/skycraft/skycraft_protocol.h`: the actor table, the event ring, `kInHurt`, the world
    entities, and the render ring's entity messages;
  - **SkyCraft's Minecraft side**, for how its entities work (see "SkyCraft's source" below):
    `fabric/src/main/java/dev/skycraft/combat/SkyrimActorEntity.java` and `SkyCombat.java`, and
    `fabric/src/client/java/dev/skycraft/client/ProxySync.java`.
- **A first message to start the next session with:**
  > Read docs/superpowers/handoffs/2026-10-04-spike-5-entities.md and start the brainstorm for Spike 5: Entities.

## What's left of spike 5

### Two follow-ups, ready to plan first

Part 2 left these, as Part 1 left four for Part 2. They're settled; plan them first, as Part 2 did.
1. **Remember the turn ratio between sessions.** Promised to the user.
   - **What happens now:** build mode turns the view by adding each frame's mouse counts times a ratio, measured from 5
     turns outside build mode each session (`link/Turn.h`). After every game start, build mode can't turn until V has
     turned 5 times; the user hit it as "turning keeps getting disabled".
   - **A design to start from:** `TurnRatio::Remember(ratio)`, used until this session has its own 5 turns. The plugin
     saves the measured ratio in its folder and loads it at startup. The status says whether it's remembered or
     measured. The user's ratio was −0.0500 degrees a count in both sessions where it was read.
2. **The ground worker falls behind fast drops.**
   - **What happened:** a creative drop of about 150 blocks in 3 s into dense ground. Every batch had been read, but the
     one worker thread had 857, then 1,516 regions queued, at 3.4 to 15 ms a region in the game against 0.74 offline,
     and the player fell through the grass once. Walking and flying, it keeps up.
   - **Likely fixes:** more worker threads, and the queue taken nearest Minecraft's player first rather than in reading
     order. Settle first why a region costs 5 to 20 times more in the game: the game's threads taking the cores, or
     denser regions.

### Part 3: V against blocks, as specified

The spec's "Part 3" and "Step 4", in short:
- **Solids:** the link keeps `kRenSolids` by section (it steps over them now), checked like the other render messages.
- **Bodies:** one PhysX static actor per section with solid blocks, for the 3 × 3 × 3 sections around V, made within one
  section and removed beyond two. Its blocks are merged into few boxes (`BoxMerge`), and it's rebuilt when the section
  changes. They stay in build mode, so leaving build mode on top of a build leaves V standing on it. Off by default
  (`set blocks 1`).
- **What they're made of:** the filter data, first material and shape flags of the static shape under V's feet, found
  with a ray down.
- **The audit first,** as spike 1's: `PxPhysics::createRigidStatic` and `createShape`, `PxRigidActor::attachShape` and
  `setGlobalPose`, `PxShape::setQueryFilterData`, `setSimulationFilterData` and `getMaterials` (each one slot later than
  NVIDIA's headers, since the game's `PxShape` has one more virtual), `PxActor::getScene`, and `PxBase::release`.
  `addActor` and `removeActor` were audited for the scene watch (Task 32's addendum).
- **The exporter skips our own actors,** so Minecraft never gets its blocks back as city shape.
- **Step 4:** `box on` (a test box 3 m ahead of V) for every interaction first; then `set blocks 1`: building a wall
  and a tower, leaving build mode on top, walking into blocks, TNT under V, and what NPCs, cars and bullets do.

### What Part 2 settled for Part 3

- **`OnUpdate` is a quiet time for scene 0.** Over 140 s at spot B and 354 s across the city with the exporter reading,
  about 93,000 of the game's calls (adds, removes, steps, fetches) came from 16 threads, and none while `OnUpdate` ran.
  No thread ever took the write lock. So bodies can be added and removed in `OnUpdate`, the spec's starting point. Our
  read lock keeps nothing out; the timing does.
- **The game shifts the whole scene** (`PxScene::shiftOrigin`, (0, ±1024, 0), on worker threads, outside `OnUpdate`)
  when PhysX's origin moves. PhysX moves our bodies with it, so the spec's "pose again after an origin change" should
  find nothing to do. Keep the check, and expect no re-poses.
- **V's two kinematic capsules:** V's controller is the radius 0.32 capsule (query filter `0:0:2000:0`, simulation
  filter `1:0:41ce:0`), as spike 1 found, and as `OriginWatch` picks it (radius 0.6 at most). A second capsule, radius
  1, has query filter `0:0:400001:0` and no simulation filter. The game's street filters are `0:0:449c004:0` and
  `5c004:0:13ce:0`, and terrain's `0:0:449c080:0`.
- **Readability checks:** `ReadableMemory` now answers large ranges with `QueryWorkingSetEx` (`b5ac34f`). `VirtualQuery`
  costs about 3.5 ms a call in the game's 600 MB heap regions, so anything new that checks large arrays must not fall
  back to it every frame.
- **Risk 11 happens:** F8 under an overhang puts Minecraft's player, and V, on top of it.

## Entities: what there is to build on

### Why entities came up

V and Minecraft's player walked through a breakable guard rail. It's a chain of kinematic actors, 2.9 m sections of
capsules and boxes, mostly query filter `0:0:4480008:0` and simulation filter `8:0:13cc:80000000`. The exporter keeps
only static actors, so it's left out by design. The user's view: moving props may be better as Minecraft entities than
as blocks, "like how boats are solid to walk on", but implementing entities was scope creep for Part 2. So the choice
was deferred to this brainstorm, rather than adding a filter rule then.

### SkyCraft's entity channels (protocol version 11)

All of these exist in the protocol and in SkyCraft's unchanged mod; BlockRender uses almost none of them.
- **The actor table** (game → Minecraft, seqlock, at `0x12000`): up to 256 records, each `formId`, flags (hostile, dead,
  essential, in combat), feet (Minecraft axes), yaw, width and height in blocks, health fraction, level and a 24-byte
  name.
  - **On Minecraft's side** (`SkyCombat`), each record becomes a `SkyrimActorEntity`, an invisible `LivingEntity`
    stand-in placed from the table every tick, and on the client every frame (`ProxySync`), so the crosshair and reach
    line up.
  - **What it does:** Minecraft's own combat hits it (swords, crits, sweeps, bows, tridents), and `SkyCombat` forwards
    one hit a tick as `kEvHitActor`. It also presses pressure plates and trips tripwires.
  - **It isn't solid:** `canBeCollidedWith` returns false, `isPushable` false, `noPhysics` and no gravity. A stand-in
    you can stand on, as on a boat, needs a change on Minecraft's side.
- **The event ring** (Minecraft → game, at `0x17000`): `kEvHitActor` (form ID, damage, knockback, weapon class, flags),
  `kEvPlayerDied`, `kEvExplosion`, `kEvArrowStuck`, and `kEvSkillUse` (Skyrim's skills).
- **`kInHurt`** in the input ring (game → Minecraft): the game hit Minecraft's player, with the kind (melee, projectile,
  magic, other), damage × 100, the attacker's form ID, and blocked or power-attack flags.
- **The world entities** (Minecraft → game, seqlock, at `0x1C000`): up to 160 things the game draws itself: arrows,
  dropped items, tridents, dropped blocks, block-breaking cracks and contact shadows. Each has a position, a rotation, a
  scale and atlas rectangles; the table also carries the targeted block's outline.
- **The render ring's entity messages** (Minecraft → game):
  - `kRenTexture`: entity textures, such as skins and armour;
  - `kRenAvatar`: the player's model each frame, none in first person;
  - `kRenScene`: every other entity and all particles each frame;
  - `kRenRagdoll`: the player's body parts, about once a second, for a death ragdoll;
  - also `kRenAtlasRegion` (animated sprites), `kRenLights` and `kRenDug` (blocks dug out of the game's world).

### What BlockRender does with them now

- **The tables and ring:** it clears the actor table, the event ring and the world entities when it creates the
  mapping, and acts on one event, `kEvExplosion`, which it logs.
- **The render ring:** its drain counts textures, scenes and ragdolls, but draws only sections. Offline, `mclink` saw
  306 scene messages in 23 s.
- **Nothing is written** to the actor table, and `kInHurt` is never sent.

### Questions to start the brainstorm with

A starting list, not a decision; agree the scope with the user.
1. **What "Entities" covers here:**
   - the game's things in Minecraft: NPCs as hittable stand-ins (the actor table), and moving props as solid entities;
   - Minecraft's things in the game: drawing its entities and particles (`kRenScene`, `kRenAvatar`), the world entities
     and the block outline;
   - combat between the two: hits, hurts and deaths.

   Which come first, and where does Part 3 go: before, beside or after them?
2. **Solid stand-ins:** SkyCraft's aren't solid. Ways to get there, each with its cost:
   - change SkyCraft's mod, which the project has kept unchanged so far;
   - a companion Fabric mod with its own solid stand-in type, perhaps driven by a new table;
   - keep props as blocks, kept by their filters and refreshed every second, as the exporter could.

   Minecraft entities collide as axis-aligned boxes, so a rail running at an angle needs several small ones, or one
   box blocks far more than the rail.
3. **Which of the game's things,** and how many: NPCs (256 records at most), cars, props like rails and barriers. Cars
   are dynamic and fast; props are kinematic until hit.
4. **Which way damage goes:**
   - Minecraft hitting NPCs, `kEvHitActor` applied through the game's scripts as `Puppet` calls them;
   - NPCs hitting Minecraft's player, through `kInHurt`;
   - one side's death killing the other (`kEvPlayerDied`).
5. **Drawing Minecraft's entities in the game:** a new draw path beside sections, rebuilt every frame, and the player's
   own avatar in third person.
6. **What can be tested without the game:** `mclink` can write an actor table and read events against SkyCraft's real
   mod, so the stand-ins and hits can be checked offline first, as steps 1 and 3 were.

## SkyCraft's source

SkyCraft 0.1.2 at `bfcaf17`, cloned in an earlier session to:

`C:\Users\BLOXY\AppData\Local\Temp\claude\D--Cyberpunk-Minecraft\818fd1fd-489f-482d-9873-88e6752bad04\scratchpad\skycraft`

It was still there on 2026-10-04. It's a temporary folder; if it's gone, cloning again is a download, so ask the user
first. The repo keeps only the protocol header (`extern/skycraft/`).

## The game install, right now

- **BlockRender 0.13.0** (`b5ac34f`) is in `E:\Cyberpunk2077\red4ext\plugins\BlockRender\`.
  - **The ground:** the city by default: once linked, Minecraft gets Night City's collision around V.
  - **Requests:** `scenewatch on|off`, `shapes`, `set ground floor|city`, `set exportbudget`, `set turn` (on by
    default) and `drivelog <seconds>`, besides spike 4's.
  - **The overlay** (`blockrender_hud`) shows the ground line, with its buttons.
  - **The log** gets a `slow batch` line for any read over 3 ms, 120 at most.
- **Spike 1's probe 0.4.0** (`PhysXProbe`) and its `physxprobe_hud` overlay are still installed: it shows V's distance
  to PhysX's origin lines. **FreeFly** is installed too.
- **Minecraft:** the Modrinth App's `CyberCraft` instance, Minecraft 26.3, Fabric, SkyCraft 0.1.2. Its world wasn't
  cleared after step 3: at least the two stone blocks placed against the street and a wall are still there.
- **The repo's CMake cache** has `CP2077_DIR` empty, so builds don't install into the game.

## Rules to keep

- **No sub-agents:** work inline.
- **The user does every in-game and Minecraft action:** launching and quitting both games, loading, moving, keys,
  clicks and screenshots. Never drive their mouse or keyboard; ask, then wait for them to say they're done.
  - **On-screen aids:** give them when a test needs positions, headings or counts. The overlays show V's position,
    heading, the Minecraft block at V's feet, and the origin lines.
  - **Turning after a game start:** until the turn ratio is remembered, ask the user to turn V a few times before
    build mode.
- **Deep reviews:** trace every claim to its source (SkyCraft's code, the protocol, the game's binaries), simulate the
  in-game steps, and break each new test once to prove it can fail.
- **Settle important unknowns** before moving on, rather than leaving them as next steps. Offer that.
- **Testable steps first:** sequence the work so every step can be checked right then.
- **Builds and the game:**
  - never build with `CP2077_DIR` set, or deploy, while Cyberpunk runs. `tasklist | grep -i -c "cyberpunk2077.exe"` must
    print `0`; the process can take a few seconds to go after the user quits;
  - set `CP2077_DIR` only to deploy, and clear it straight after;
  - `mclink` needs Cyberpunk closed, and a check that runs it "without Minecraft" needs Minecraft closed too.
- **No faults:** the game's crash handler sees a fault before any `__try/__except`. Check every pointer, size and
  vtable. If the game freezes, read the logs and the newest `ReportQueue` folder, and force-close it only when the user
  asks.
- **Downloads** need the user's explicit permission first.
- **Commits** end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`, and go on the spike branch.
- **Ask before deleting branch `spike/player-puppeting`** (spike 2's handoff).
- **After the spike:** `superpowers:finishing-a-development-branch`. The user has chosen a local fast-forward merge to
  `main` every time so far; ask again.

## Gotchas

- **Bash's working directory:** a `cd` in a Bash call moves it for later calls. Wrap it in a subshell,
  `(cd ... && ...)`.
- **Backslashes:** the Bash tool turns `\\` into `\`. Write files with the Write or Edit tools when they hold
  backslashes.
- **Tool paths:** `cmake`, `ctest` and `dumpbin` aren't on `PATH`; the plan's build steps have their full paths. `luac`
  is on `PATH`.
- **Python:** `uv run --no-project`.
- **Scratch builds under `%TEMP%`** miss header changes (MSB8029): rebuild from clean after editing a header.
- **The plan tooling** is in `tools/plans/`, updated with Part 2's versions; their paths point at the old scratchpad,
  so adapt them. `mutate.py` runs its whole list when imported.
- **Waiting on the game:** a foreground `sleep` is blocked. Record the status in the background (a loop over
  `status.txt`, or `spikes/block-render/tools/status_watch.py`) and read the recording afterwards. The status's "worst"
  figures are the last second's.
- **Request files:** write them atomically (a temporary name, then renamed), and wait for the status's `request=` line
  before the next.
- **Minecraft:** the user starts and stops it from the Modrinth App; its window hides while linked. Its log is
  `%APPDATA%\ModrinthApp\profiles\CyberCraft\logs\latest.log`. A swapchain warning on every link is normal: SkyCraft
  hiding the window.
- **Logs:** each plugin writes one per launch in `E:\Cyberpunk2077\red4ext\logs\`.
- **Git:** for a fast-forward merge, `git switch main` then `git merge --ff-only <branch>`.
