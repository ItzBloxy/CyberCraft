# Handoff: plan Part 3 of spike 6 (explosions)

Written 2026-10-06, at the end of the session that checked step 4's fixes in the game and closed Part 2 of spike 6,
NPCs and combat. The user chose to plan Part 3 in a new session. Read this first.

## Start here

- **Repo and branch:** `D:\Cyberpunk-Minecraft`, branch `spike/npcs-and-combat`. Spike work stays on the branch, never
  on `main`. The repo has no git remote.
- **Where spike 6 stands:**
  - **Part 1 is done:** Night City's NPCs are SkyCraft's stand-ins in Minecraft (steps 1 and 2 passed);
  - **Part 2 is done:** combat both ways. Steps 3 and 4 passed, and step 4's fixes passed their check in the game
    (Task 36, `1afacc4`). Task 37 closed Part 2 and wrote this handoff. BlockRender 0.18.1, with the module's version
    5, is installed in the game;
  - **two follow-ups** the user asked for wait (below).
- **Next: plan Part 3,** the spec's "Part 3: explosions" and its step 5. Its tasks start at **Task 38**, in a new
  section after Task 37 and before "Found while planning", laid out as Part 2's is: an introduction, a file map, then
  the tasks. Then commit `docs/superpowers` with `npcs-and-combat plan: Part 3 planned`.
  - **Ask the user first** whether the follow-ups come before Part 3, with it or after it: they didn't say.
- **Process:** as Part 2 was planned:
  1. `superpowers:writing-plans`;
  2. every code step compiled and tested in a scratch clone, each task committed and tagged there ("Planning, as it
     worked", below);
  3. each new or changed test broken once (`mutate.py`); the module compiled offline without and with Codeware's
     scripts, and a slip shown to fail the compile where it should;
  4. the tasks replayed in a fresh clone of the repo by `tools/plans/run_plan.py`, every expected result checked, and
     each task's tree compared with the scratch clone's tag;
  5. spec changes found while planning go in a new "Changes while planning Part 3" at the spec's end, and into the
     sections above it;
  6. then offer execution, inline: `run_plan.py execute`, a task at a time.

  **Expect the user to ask for a deep review of the plan** before executing it: trace every claim to its source (the
  decompiled scripts, SkyCraft's code, Codeware's scripts, the RED4ext SDK, Minecraft's jar), simulate step 5 as the
  user will play it, and break each new test once.
- **No sub-agents:** the user wants everything done inline, plan execution included.
- **Read for background, in this order:**
  - this handoff;
  - **the spec**, `docs/superpowers/specs/2026-10-05-npcs-and-combat-design.md`:
    - "Purpose" and "Decisions from the brainstorm";
    - under "Environment": "SkyCraft's Minecraft side" (`ServerExplosionMixin`), "SkyCraft's game side, as a
      reference" (`ApplyExplosion`), "The game's scripts" (the damage pipeline, and "Explosions": `ExplosiveDevice`,
      `CombatGadgetHelper`, `RPGManager.PrepareGameEffectAttack`) and "Minecraft 26.3" (`ServerExplosion.explode`);
    - "The protocol, as this spike uses it" and "The frame";
    - "Part 2: combat both ways", as built, above all "Minecraft's hits on NPCs";
    - "Part 3: explosions", "Step 5", `explode` in "Requests", and "Blasts" in "Measurements and the status";
    - risks 3, 8, 10, 13 and 14;
    - "Changes while planning" (items 7 and 8: the blast before its hits, and the push a quarter second after), the
      first "Not settled by reading" (under "Changes from the deep review"), and "Changes from step 4";
  - **the results**, `docs/spikes/2026-10-05-npcs-and-combat-results.md`: the answer at its top; step 4's "What it
    means"; and "Step 4's fixes", with its "Asked for, after Part 2" and "What it means";
  - **the plan**, `docs/superpowers/plans/2026-10-05-npcs-and-combat.md`: "Before you start"; Part 2's introduction
    and file map, whose layout Part 3 follows; Tasks 26 (the module) and 29 (the plugin's frame), the shape of a module
    task and a plugin task; Task 36, the shape of an in-game task; "Step 4's fixes" and "How Part 2 went";
  - `tools/plans/README.md`.
- **A first message to start the next session with:**
  > Read docs/superpowers/handoffs/2026-10-06-spike-6-part-3-planning.md and plan Part 3.

## What Part 3 builds

The spec's Part 3, in short:
- **The blast:** for each `kEvExplosion`, with combat on, its centre goes into world axes (`WorldFromMc`), and its reach
  is twice its radius, in metres, as SkyCraft's is. The module's `Explode` makes:
  - **the push:** the game effect `physicalImpulseSphere` at the centre, the reach as its radius, as
    `CombatGadgetHelper.SpawnPhysicalImpulse` runs it, a quarter of a second after the blast, so the bodies of NPCs the
    blast's hits killed are ragdolls by then;
  - **knockdowns:** an explosion attack at the centre, the reach as its radius, from V and flagged `DealNoDamage`, as
    `CombatGadgetHelper.SpawnAttack` makes one; its hit on V stops at `CheckForQuickExit`;
  - **the look:** an explosion effect at the centre.
- **Blast hits:** Minecraft's hits within 0.15 s after a blast, on NPCs within its reach by their last listed feet,
  land as hits of type `Explosion`, their direction from the blast's centre, so an NPC the blast kills dies of an
  explosion and the game throws its body.
- **The fallback:** if a hit that deals no damage doesn't knock NPCs down, the module knocks down each NPC within reach
  itself, as SkyCraft's `KnockExplosion` does.
- **`explode <metres> <radius>`:** a blast that far ahead of V, at the height of V's feet, as if it were Minecraft's,
  for step 5's opening check.
- **The status:** blasts, how many, and the last: its centre, reach, the NPCs within it and its blast hits.
- **BlockRender 0.19.0.**

**Step 5's checks** (the spec's "Step 5"): the opening check, `explode` beside an NPC (knocked down?) and beside a
parked car (pushed?), and the effect at the point; then TNT among NPCs on a pavement, by a parked car, bins and boxes:
NPCs at the edge knocked down, those in the middle killed and thrown; the car and the props pushed; the effect where the
TNT was; and V and Minecraft's player, in survival, hurt only by Minecraft's own blast.

**Left to planning** by the spec: which attack record and which effect; and whether a hit that deals no damage knocks
an NPC down, which step 5's opening check settles.

**New since the spec's Part 3 was written:**
- **Blast hits go through the game's whole damage pipeline,** as Minecraft's other hits have since the deep review of
  Part 2: they start where `Hit` marks them, so the game's own modifiers for explosions apply (the plan's Task 37 named
  `ExplosionResistance` and the distance curve: check how the pipeline applies them to a scripted hit).
- **The hit power applies to them:** a hit starts at V's punch × Minecraft's damage to the power 0.5 (`kHitPower`,
  `set hitpower`), which the user found right for melee and arrows. A TNT blast is tens of Minecraft's damage near its
  centre: by Minecraft's explosion formula, (e² + e) / 2 × 7 × 2r + 1 with e from 0 to 1, up to 57 for TNT (radius 4)
  on an exposed entity at the centre; check it in the 26.3 jar. At the square root that starts at about 7.5 punches,
  near 800. Whether blasts keep 0.5 is a question for the user.
- **`Hit` today makes melee hits:** from V, with the attack record and weapon object of V's unarmed weapon, its
  direction from V. A blast hit needs an explosion attack record and type, its direction from the centre, and no weapon,
  but the same mark, so the pipeline's wraps start it and report its landing as they do Minecraft's other hits.
- **A testable step without the game:** `mclink` can make stand-ins (`actor`) and type commands
  (`type /summon tnt …`), so a step like steps 1 and 3 could read a real blast's `kEvExplosion` and its hits on
  stand-ins, their damage and order, before anything goes into the game.

## What's already there for it

- **Events:** `Link::Update` drains SkyCraft's event ring in its order. `kEvExplosion` goes to the sink: the plugin's
  `MinecraftSink::Explosion` logs `explosion at Minecraft x y z, radius r` and starts a burst measurement, and the
  link's status counts explosions. A blast's hits on stand-ins follow it at the end of Minecraft's tick.
- **`link/Combat`:** `AcceptHit` (which hits count, the cap of 1,000, the start), `NpcHit`, `HitDrop` and their words;
  the death window; the attack types (`kAttackTypes`, `Explosion` among them). The spec's tests for Part 3: the blast
  window (0.15 s, the reach) and the centre in world axes.
- **`link/Actors`:** NPCs' numbers (`ActorIds`) and the records with their feet, which the blast window needs.
- **`Coords.h`:** `WorldFromMc` and `McFromWorld`.
- **The module**, `scripts/BlockRender.reds`, version 5: `Hit` (above), marked `hitFlag.None` under `n"BlockRender
  <amount>"`; the `ProcessPipeline` wrap starts our hits at that amount (`StartAt`), and the `DealDamages` wrap reports
  their landings, `BlockRenderHitLanded`, or why they dealt nothing, `BlockRenderHitEnded`. Its comments on `Hit` and
  the wrap still say "Minecraft's damage x V's punch", where the start has been V's punch × damage to the hit power
  since Task 32: correct them when Part 3 changes them.
- **`GameScript`:** the module's 10 functions and 16 fields, and its seven natives, each checked by its types once;
  `kModuleVersion` is 5. A new function, such as `Explode`, means version 6 and a new entry in its tables.
- **`tools/check_reds.py`:** the module's offline compile against the game's own scripts, with Codeware's as an extra
  folder. Never run the game's `scc.exe` by hand: a failed compile shows a message box on the user's screen.
- **Requests and the status:** `Requests.*` parses `hit`, `punch`, `set hitpower` and the rest, and `Plugin.cpp` runs
  them; `Status.*` writes each line, the combat line `combat=on: V's punch …; hit power 0.50; hits …`.
- **The overlay**, `tools/blockrender_hud/init.lua`: the build, NPC and combat lines, and the Combat button.
- **Counts at Part 3's start:** 440 C++ tests; BlockRender 0.18.1 (Part 3 makes it 0.19.0); module version 5.

## What Parts 1 and 2 found, for Part 3

- **The game runs script callbacks and the damage pipeline on worker threads.** The wraps read only the hit and call
  BlockRender's natives, behind a lock, and the module's state is touched only on the main thread (the spec's risk 8).
- **Our hits are V's in all but the swing:** the start at `ProcessPipeline`'s entry holds; V's crits roll on top; NPCs
  flinch, now and then stagger, take cover and fight back; the hits carry V's Mantis Blades and burn NPCs as the
  Thermal blades do, and earn V skill XP.
- **An NPC's health changes only after `DealDamages`:** a landing's "after" is its "before"; the next hit's "before"
  shows the change.
- **Hits sometimes land late:** one strong hit landed 1.4 s after it was queued. The blast window counts Minecraft's
  events, not the game's landings, so it isn't affected.
- **No NPC blocked, parried, dodged or cut a hit with its armour** in 53 hits on 6th Street gangers.
- **V's hurts from a blast** reach the wrap as `other (Explosion)`, with no attacker: two came in step 4's fixes.
- **The user's V can't be made Immortal:** three Mortal god modes that quests left behind outrank it, and the game
  refuses the override. In build mode the damage wrap is V's only guard; it held for all 84 hits on V there.
- **In the game:** CET's overlay key ends build mode, as Esc does, so checks in CET's console happen outside it; the
  game puts V's blades away after a while idle, about every 68 s in build mode, and `Arm` draws them again within a
  second.

## The follow-ups the user asked for

Not planned yet; the user decides when (see "Start here").
- **Minecraft's health from V's as build mode starts:** "Entering build mode should set your Minecraft health to what
  V's health is." Today the two are apart: in build mode only Minecraft's hearts take hits, outside it only V's health.
  SkyCraft's protocol has no input that sets health, only `kInHurt`: a hurt of the difference could lower it, though
  Minecraft's Protection may cut it; raising it needs a heal, through a command or CyberCraft Tweaks, our own mod.
- **Bushes out of the city's collision for Minecraft:** the exporter's `KeptShape` (`src/physx/Export.cpp`) keeps every
  rigid static scene-query shape that isn't a trigger or a plane. A bush is spheres of about 2 m with query layer
  `0x80000` alone and an all-zero simulation filter (the results' "Asked for, after Part 2"); one bush also had a
  capsule, which `shapes` couldn't size, since it prints each actor's bounds, not each shape's. Print each shape's own
  bounds, pick the rule, and check it in the game on a bush and a wall.
- **Smaller ones, from Task 36:** blades V held before build mode come back without their heat until drawn again (the
  heat isn't an effect that starts by itself, so it isn't restarted); and the game's idle timer that puts the blades
  away could be reset while `Arm` holds them.

## The game and Minecraft, right now

- **BlockRender 0.18.1** (`0e23764`) is in `E:\Cyberpunk2077\red4ext\plugins\BlockRender\`, the module in its
  `Scripts\BlockRender.reds`:
  - combat is off at each start (`set combat 1`, or the overlay's Combat button); the hit power is 0.5 and `hidev` 1;
  - NPCs and bodies are on by default; DRED is on (`settings.txt` has `dred = 1`); `turn.txt` keeps the turn ratio.
- **The overlay** is in `bin\x64\plugins\cyber_engine_tweaks\mods\blockrender_hud\`, with the status in `status.txt`.
- **Codeware 1.19, RED4ext 1.30.0 and CET** are installed; redscript compiles the module with the game's scripts at
  each start. Spike 1's probe and its overlay, and FreeFly (F7, kept off), are installed too.
- **The user's V:** the Thermal, Legendary Mantis Blades as its unarmed weapon (V's punch 104.87), 456 max health, no
  Second Heart. Saves used: behind a car at a 6th Street gang spot (level-47 gangers of 594 to 891 health), and step 2's
  busy pavement.
- **Minecraft:** the Modrinth App's `CyberCraft` instance, in `%APPDATA%\ModrinthApp\profiles\CyberCraft`: Minecraft
  26.3, Fabric Loader 0.19.5, Fabric API 0.161.0, SkyCraft 0.1.2 and CyberCraft Tweaks 1.2.0. Its world has
  `keep_inventory` and `immediate_respawn` on; the player is in survival, wearing 4 points of armour; the hotbar holds a
  diamond sword, a diamond axe, a bow, arrows, oak planks, golden apples and golden carrots in slots 1 to 7. Ask before
  replacing anything in it: in step 4 the user allowed slots 1 to 4.
- **The repo's CMake cache** has `CP2077_DIR` empty, so builds don't install into the game.
- **SkyCraft's source,** `bfcaf17`, is in an earlier session's scratchpad, there on 2026-10-06:
  `C:\Users\BLOXY\AppData\Local\Temp\claude\D--Cyberpunk-Minecraft\818fd1fd-489f-482d-9873-88e6752bad04\scratchpad\skycraft`.
  Its explosions: `fabric/src/main/java/dev/skycraft/mixin/ServerExplosionMixin.java` (the event) and
  `skse/src/Combat.cpp` (`ApplyExplosion`, `KnockExplosion`). If it's gone, cloning it again is a download: ask first.
- **The game's scripts,** decompiled for 2.31: [adamsmasher/cyberpunk](https://codeberg.org/adamsmasher/cyberpunk) on
  Codeberg. Search with `…/search?q=<words>` in the built-in browser; read raw files
  (`…/raw/branch/master/<path>`) with its JavaScript tool, and quote them; `orphans.swift` holds the natives. Don't
  trust a summary of them.

## Planning, as it worked

- **The scratch clone:** clone the branch into the session's scratchpad as `verify/`, and copy `tools/plans/` beside
  it as `plans/`; point `b.sh`, `gen_plan.py`, `run_plan.py` and `mutate.py` at it (their paths are an earlier
  scratchpad's). Build and test with `plans/b.sh`, and `b.sh "" "" clean` after a header changes: under `%TEMP%`,
  MSBuild misses header changes (MSB8029). Commit each task there and tag it: step 4's fixes were `fix-t32` to
  `fix-t35`.
- **The module:** a "Compile the module …" step compiles it with `tools/check_reds.py E:/Cyberpunk2077 scripts`, then
  again with `E:/Cyberpunk2077/red4ext/plugins/Codeware/Scripts` added; a "Check that the compile catches a slip …"
  step `sed`s a slip into a copy under `build/reds-slip` and shows the compile failing (Task 33's Steps 2 and 3).
- **The skeleton:** markdown with the prose, where `@@FILE <path>@@` becomes a whole file and
  `@@EDIT <path> <from> <to>@@` an Edit step's (old, new) pairs, `<from>` and `<to>` each a commit, `verify` or
  `snap:<name>`. `gen_plan.py`, with `GEN_CLONE` set to a worktree of the clone, turns it into the plan's text, which
  goes into the plan after Task 37.
- **Before committing the plan:**
  - `mutate.py` breaks each new or changed test once;
  - if the overlay changes, `luac -p` checks its syntax, and `hud_check.lua` shows its lines against sample status
    lines;
  - `run_plan.py replay <plan> <fresh clone of the repo> 38 <last task>` applies every file step exactly and checks
    every build, test count and compile;
  - after any later edit to the plan's text, `trees_match.py` checks it still makes the tagged trees.
- **The runner's step kinds:** a step's title decides what `run_plan.py` does with it:
  - **files:** Write, Edit, Delete or Copy `path`; paths are under `spikes/block-render/` unless they start with
    `extern/` or `docs/`;
  - **builds:** "Build; it must fail" (a compile error), "Build and run the tests; the new one must fail" (or "ones")
    and "Build and run the tests; they must pass" (a count, no warnings);
  - **the module:** "Compile the module …" and "Check that the compile catches a slip …";
  - **checks:** "Check that BlockRender imports nothing from PhysX", "Check the overlay's syntax", "Run mclink without
    Minecraft", the tools' Python tests, and "Commit";
  - **stops:** steps titled "Deploy", "In the game" or "Record", done by hand.

  Anything else stops the run: add a step kind first.
- **Expected failures:** MSVC reports a missing name as `C2065`, a missing member as `C2039` and a wrong number of
  arguments as `C2660`; redscript, a method that isn't there as `[UNRESOLVED_METHOD]`. Guess them, and let the replay
  correct the guess.

## Rules to keep

- **No sub-agents:** work inline.
- **The user does every in-game and Minecraft action:** launching and quitting both games, loading, moving, keys,
  clicks, CET's console and screenshots. Never drive their mouse or keyboard; ask, then wait for them to say they're
  done.
  - **On-screen aids:** give them when a test needs positions, headings or counts; the overlays show V's position and
    heading, and the Minecraft block at V's feet.
  - **Name what's invisible:** anything BlockRender doesn't draw, such as the stand-ins. Say so before the user looks.
- **Deep reviews:** trace every claim to its source, simulate the in-game steps, and break each new test once.
- **Settle important unknowns** before moving on, rather than leaving them as next steps. Offer that.
- **Testable steps first:** sequence the work so each step can be checked against something real right then.
- **Read values from the game:** the user wants the game's real numbers, fed into the game's own systems, not tables
  tuned by hand.
- **Builds and the game:**
  - never build with `CP2077_DIR` set, or deploy, while Cyberpunk runs: `tasklist | grep -i -c "cyberpunk2077.exe"`
    must print `0`;
  - set `CP2077_DIR` only to deploy, and clear it straight after;
  - `mclink` needs Cyberpunk closed.
- **No faults:** the game's crash handler sees a fault before any `__try`. `GameScript` calls only functions whose
  types it has checked; the module checks every object with `IsDefined`; the natives, on any thread, only take a lock.
  If the game freezes, read the logs and the newest folder in `%LOCALAPPDATA%\REDEngine\ReportQueue`, and force-close
  it only when the user asks.
- **Downloads** need the user's explicit permission first.
- **Commits** end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`, and go on the spike branch.
- **Ask before deleting branch `spike/player-puppeting`** (spike 2's handoff).
- **After the spike:** `superpowers:finishing-a-development-branch`. The user has chosen a local fast-forward merge to
  `main` every time so far; ask again.

## Gotchas

- **Bash's working directory:** a `cd` in a Bash call moves it for later calls. Wrap it in a subshell,
  `(cd … && …)`.
- **Backslashes:** the Bash tool turns `\\` into `\`. Write files that hold backslashes with the Write or Edit tools.
- **Tool paths:** `cmake` and `dumpbin` aren't on `PATH`; the plan's build steps have their full paths. `lua` and
  `luac` (5.4) are.
- **Python:** `uv run --no-project`.
- **Tests:** run `render_tests.exe` from `spikes/block-render`, or its fixture files aren't found.
- **Waiting on the game:** a foreground `sleep` is blocked. Poll a condition instead, such as the status's `request=`
  line changing: `timeout 15 bash -c 'until <condition>; do :; done'`.
- **Requests:** write `blockrender.request` atomically (a temporary name, then renamed), and send the next only once
  the status's `request=` line shows the last.
- **Minecraft:**
  - the user starts and stops it from the Modrinth App; its window hides while linked;
  - its log is `%APPDATA%\ModrinthApp\profiles\CyberCraft\logs\latest.log`, with chat replies as `[CHAT]` lines;
    `mc /execute if entity @s[gamemode=survival]` answers whether the player is in survival;
  - its world is saved: whatever a test places, or a blast breaks, stays so. Clear it afterwards, by block coordinates.
- **Logs:** BlockRender writes one per launch in `E:\Cyberpunk2077\red4ext\logs\`; redscript's is
  `E:\Cyberpunk2077\r6\logs\redscript_rCURRENT.log`.
- **The scratchpad is per session:** Part 2's scratch clone (`verify/`, tags `p2r-t23` to `p2r-t30` and `fix-t32` to
  `fix-t35`) and notes are in earlier sessions' scratchpads (`4a6539a8-db56-4530-9f07-df4565c9cfab`,
  `cb5a531f-b206-42f2-8657-e547d454294d` and `7f5762d7-4df1-4461-91ea-2d5ca236c2d1`). Start a fresh clone from the
  branch.
