# Handoff: plan Part 2 of spike 9 (Minecraft's mobs in the game)

Written 2026-10-08, at the end of the session that brainstormed spike 9, planned and executed its Part 1, and closed
it. The user asked for this handoff to plan Part 2 in a new session. Read this first.

## Start here

- **Repo and branch:** `D:\Cyberpunk-Minecraft`, branch `spike/minecraft-paths`. Spike work stays on the branch, never
  on `main`. The repo has no git remote.
- **Where spike 9 stands:**
  - **Part 1 is done:** CyberCraft Tweaks, our Fabric mod, makes Minecraft's mobs find their way on Night City's
    ground, as SkyCraft holds it (1/8-block voxel shapes): they wander, go to targets (tempt, chase), keep up with their
    owner as pets, fly around walls, and don't see V through them. Step 0, run against SkyCraft's real mod with `mclink`
    and spike 1's dumped ground, passed with **CyberCraft Tweaks 1.7.1** (plan Task 14, `b052bef`), which is installed;
  - **Task 15** (close Part 1, plan Part 2): its first two steps are done, "How Part 1 went" at the plan's end and the
    spec's "Changes while executing Part 1", committed with this handoff.
- **Next: Task 15's step 3, plan Part 2:** the overlay's **Paths** row, **run F** in the game, and closing the spike.
  Its tasks start at **Task 16**, in a new section after Task 15 and before "Found while planning Part 1", laid out as
  Part 1's is: an introduction, a file map, then the tasks. Then Task 15's step 4 commits `docs/superpowers` (tick
  steps 3 and 4).
  - **Ask the user first** about what "New since the spec's run F was written" (below) leaves open: which hotbar slot
    the wheat and the dandelion go in, and whether to simulate V's apartment before planning its part of run F.
- **Process:** as Part 1 was planned ("Planning, as it worked", below):
  1. `superpowers:writing-plans`;
  2. every file step made and checked in a scratch clone first, each task committed and tagged there;
  3. each new or changed check broken once;
  4. the tasks replayed from the plan's text in a fresh clone, every expected result checked, each task's tree
     compared with the scratch clone's tag;
  5. spec changes found while planning go in a new "Changes while planning Part 2" at the spec's end, and into the
     sections above it;
  6. then offer execution, inline, a task range at a time.

  **Expect the user to ask for a deep review of the plan** before executing it: trace every claim to its source
  (Minecraft 26.3's jar, SkyCraft's code, the overlay, BlockRender's requests), simulate run F as the user will play
  it, and break each new check once.
- **No sub-agents:** the user wants everything done inline, plan execution included.
- **Read for background, in this order:**
  - this handoff;
  - **the spec**, `docs/superpowers/specs/2026-10-08-minecraft-paths-design.md`: the opening and "Decisions from the
    brainstorm"; section 1 (the city's cells, fences among them); sections 2 and 3 (walkers, pets, flyers, sight);
    section 4 (the minute line); "Versions and files"; "Run F, in the game"; "Risks"; and "Changes while executing Part
    1";
  - **the results**, `docs/spikes/2026-10-08-minecraft-paths-results.md`: the control, the 1.7.0 run with its
    diagnostics, and "Step 0 with CyberCraft Tweaks 1.7.1", with "The teleports" and "What it means for Part 2";
  - **the plan**, `docs/superpowers/plans/2026-10-08-minecraft-paths.md`: "Before you start"; Part 1's file map; Task 7
    (an `mclink` script task), Task 12 (a version task), Task 14 (an in-Minecraft task); "How Part 1 went";
  - **the overlay**, `spikes/block-render/tools/blockrender_hud/init.lua`: the Fun header's rows and spike 7's Spawn
    rows, which the Paths row joins;
  - `tools/plans/README.md`, "Spike 9's versions".
- **A first message to start the next session with:**
  > Read docs/superpowers/handoffs/2026-10-08-spike-9-part-2-planning.md and plan Part 2.

## What Part 2 builds

The spec's "Run F, in the game", in short:
- **The overlay's Paths row,** a Spawn row in the Fun header: buttons that summon, ahead of V, wanderers (a pig, a cow,
  a villager), a pet (a wolf owned by Minecraft's player), a flyer (a bee) and a husk with Resistance for the player,
  and give the player wheat and a dandelion. Everything is tagged `sp7`, so the row "Spawned:"'s Clear removes it. The
  overlay is one Lua file, deployed while the game is closed.
- **Run F,** with the user, at the pavement area by spike 3's corner first (step 0's ground: the user has V in her
  apartment in the save and will move V there), then V's apartment and its building's corridors (doorways, stairs,
  railings):
  1. the wanderers stroll the pavements and streets, step kerbs, and stay out of walls and off drops;
  2. the cow follows the wheat;
  3. the wolf, the only mob left, follows, and after V sprints away it lands beside V: the entities status line's
     "the nearest mob … m away" shows it;
  4. the bee follows the dandelion without sticking in walls;
  5. with combat off and Minecraft's player in survival, the husk chases V in view, and doesn't notice V behind a
     building it could walk around, until V steps into view;
  6. the frame rate is as before, and the mod's minute line in Minecraft's log stays modest.

  Clear ends it; the results doc gets screenshots, as earlier runs' did.
- **Closing the spike:** the results doc's answer at its top, "How Part 2 went", and
  `superpowers:finishing-a-development-branch`.

**New since the spec's run F was written:**
- **Summon half a block up.** A mob summoned inside the city's skin falls through it (`VoxelShape.collideX` ignores
  voxels already inside the box), and V's feet can be inside one: on step 0's road the stand-in V's feet were at 5.94
  over a skin topped at 6.0. The overlay's `ahead()` places things at the player's feet height (`/execute rotated ~ 0
  positioned ^ ^ ^5 run summon … ~ ~ ~`), so the row's summons need `~ ~0.5 ~` or `^ ^0.5 ^…`. Step 0's script drops
  every summon half a block onto its floor.
- **The wheat and the dandelion:** the user keeps a **shield in the off hand**, and step 0 had to move it (Task 8). A
  tempting mob looks at both hands (`TemptGoal`), so the main hand will do: the Fun rows' "In hand" replaces the
  selected hotbar slot's item. Ask the user which slot to give up (their hotbar 8 and 9 are on mouse buttons 5 and 4).
- **The wolf's owner:** step 0 set it from the chat with `/data modify entity @e[tag=sp9wolf,limit=1] Owner set from
  entity @p UUID`. The overlay's commands run as the player, through the link's chat; the wolf needs a tag of its own
  besides `sp7` to be picked out, and `limit=1,sort=nearest`.
- **Resistance:** step 0's `/effect give @p minecraft:resistance 300 4 true`, with combat off (Minecraft's hits stay in
  Minecraft).
- **Commands are at most 256 characters,** Minecraft's chat line. BlockRender's `mc` request refuses a longer one as
  it reads the request (`link::ValidCommandOrSet`, `src/link/Commands.cpp`), so the overlay shows an error reply
  instead of a summon; Task 7's skeleton summon was 298 and had to be shortened.
- **Read a pet's landing right after V moves:** a pet that lands near its owner strolls, as in vanilla, so the
  teleports script's test 4 s after a jump failed twice of five on strolls while all 26 landings were on the road.
  The mod's minute line counts landings (`… pets landed on the city's floor`).
- **Watch pets in many-levelled places:** in the 1.7.0 run one wolf ended in a hollow under the pavement that no path
  reaches, after landing near V; how is unexplained, and 1.7.1 never repeated it in 26 landings. V's apartment
  building has more levels than step 0's road.
- **Railings are fences now:** a cell whose middle is mostly open is impassable and solid to random positions, and its
  top counts half a block higher, so no mob jumps onto a railing; wide wall tops stay walkable.
- **Testable without the game first:** an `mclink` script can type the row's exact commands into Minecraft on step 0's
  dump, as step 0 did, proving them against SkyCraft's real mod before the game; `tools/plans/fun_check.lua` clicks
  the overlay's buttons offline and prints the requests they write. The simulation (`tools/plans/paths-sim/`) can
  voxelize V's apartment's dump, `E:\Cyberpunk2077\red4ext\plugins\PhysXProbe\dumps\2026-10-08_01-44-52` (V 132 m up),
  and flood what a mob reaches there, to pick run F's spots and expected results.

## What's already there for it

- **CyberCraft Tweaks 1.7.1,** `minecraft/cybercraft-tweaks`:
  - `paths/CellRule` (no Minecraft types: the kinds none, floor, blocked and fence, a cell's floor, stroll targets, the
    surface search, a segment's pieces), `CityCells` (SkyCraft's shapes through `CellRule`, each shape's column tops
    cached weakly, Minecraft's own blocks first), `CityRays` (casts against the city's triangles, `SkyClip.cast`, in
    8-block pieces), `PathStats` (the counters and the minute line, from the server's tick);
  - eight mixins, one per Minecraft class, `remap = false`: `CityWalkNodes`, `WalkNodeEvaluatorAccess`,
    `CityPathCache`, `CityNavigation`, `CitySurface`, `CityGoalUtils`, `CityPetTeleport`, `CitySight`;
  - `build.sh` (`javac` against the Modrinth App's jars, no Gradle), `test.sh` (56 unit checks with plain `java`),
    `mixins.sh` (12 checks of the mixins' targets against 26.3's jar, `tools/MixinCheck.java`).
- **The minute line,** in Minecraft's log: `CyberCraft Tweaks: paths in the last minute: <n> city answers, <n> shapes
  read; <n> sight casts (<n> hit), <n> shortcut casts (<n> hit), <x> ms casting; <n> pets landed on the city's floor`,
  written only in a minute with something to count. Step 0's ran 2 to 3 ms of casting a minute.
- **The overlay:** the Fun header's rows, whose buttons' commands are typed into Minecraft's chat, one `mc` request
  each, in order; spike 7's Spawn rows (`ahead()`, `mob()`, tag `sp7`, blocks with `sp7b` markers) and "Spawned:"'s
  Clear. The deployed `init.lua` is the repo's.
- **BlockRender 0.23.0** (spike 8's), unchanged by spike 9: the entities status line ends `; the nearest mob <x> m
  away`. Spike 9 changes nothing in BlockRender's C++ or in SkyCraft.
- **`mclink`'s scripts:** `tools/mclink/paths.txt` (step 0) and `teleports.txt` (five jumps with a pet), with the
  actions `status`, `wait-world`, `ground city`, `at`, `look`, `dump`, `link on`, `wait-ground`, `type`, `down` and `up`
  (keys: 26 is W), `pause`, `key`, `wait-screen`, `scene` and `quit`.
- **Plan tools** in `tools/plans/` (its README's "Spike 9's versions"): `run_plan_s9.py`, `mutate_s9p1.py`,
  `mutate_s9p2.py`, `paths-sim/` (`voxdump` and the walk simulation), with `gen_plan.py`, `pairs.py`, `planlib.py`,
  `tick_steps.py`, `trees_match.py`, `rewrap.py`, `hud_check.lua` and `fun_check.lua`.

## What Part 1 found, for Part 2

- **The city's skins have nothing under them:** a street is a skin one or two voxels thick, often with a second level
  or a hollow below. A cell whose skin is in its lower half is a floor; in its upper half, a full block; mostly open,
  a fence.
- **A mob inside a skin falls through it;** so does anything summoned at a floor's exact height if it's inside the
  skin. Pets' landings are placed exactly on the floor, and reset their fall distance (from 1.7.1).
- **Minecraft's window hides once linked** and stays hidden; the user can't click in it. Commands go through the chat,
  from `mclink` or BlockRender's `mc`.
- **Saving:** singleplayer has no `/save-all` (26.3 registers it on dedicated servers only), and the pause menu doesn't
  save a linked game. Minecraft autosaves every 5 minutes (`saves/SkyCraft/level.dat` and `players/data/*.dat` get
  new times). Stopping Minecraft from the Modrinth App kills it unsaved, and a stale autosave once brought back a test
  wolf, the player's creative mode and wheat in its off hand: **after the last change, wait for an autosave before the
  user stops Minecraft** (a background `until` loop on the files' times, as plan Task 14 does).
- **26.3's replies:** `Test passed. Count: 1` and `Test failed` for `/execute if`; `Replaced 1 slot(s) on ItzBloxy with
  [Wheat]` for `item replace … with`, and no item named for `item replace … from`.
- **A summon with NBT skips `finalizeSpawn`:** no default gear. A skeleton's bow and an unbreakable helmet (against the
  noon sun) go in `equipment`, with `drop_chances` of 0.
- **The world:** `SkyCraft`, its clock fixed at noon, no weather, natural spawning off.
- **Step 0's dump** (`2026-10-01_14-37-07`, at 1280.5 6.14 2007.5): a road along z, its street at 6.0 and pavement at
  6.25, kerbs between; a barrier on a diagonal (x 1284 at z 2006 to 1286 at z 2015), top 7.5; a ledge at 5.5 behind
  it, then a rock slope from −0.5; hollows under the pavement (3.25 to 4.5) and under the ledge (1.375). `mclink`'s
  `dump` places a dump around the stand-in V's feet as it runs: stand V at 1280.5 6.14 2007.5 before it.

## The game and Minecraft, right now

- **Cyberpunk:** closed. BlockRender 0.23.0 is in `E:\Cyberpunk2077\red4ext\plugins\BlockRender\` (2026-10-07), and
  the overlay in `bin\x64\plugins\cyber_engine_tweaks\mods\blockrender_hud\`, the same as the repo's. V is in her
  apartment in the user's save.
- **Minecraft:** the Modrinth App's `CyberCraft` instance (`%APPDATA%\ModrinthApp\profiles\CyberCraft`): Minecraft
  26.3, Fabric Loader 0.19.5, Fabric API 0.161.0, SkyCraft 0.1.2, **CyberCraft Tweaks 1.7.1** (1.2.0 to 1.7.0 renamed
  `.jar.disabled`), and the user's Entity Model Features and Entity Texture Features, left enabled by the user's
  choice. Its world's player is in survival with the shield in its off hand, as before step 0; no test entity is left.
  When this was written Minecraft was still running, its world autosaved at 23:03:36 after the last change, and the
  user had been asked to stop it.
- **The scratchpad is per session:** Part 1's scratch clones and skeletons are in session
  `55a52b36-da7d-4d82-a71c-38b004a4f4fc`'s scratchpad (`verify/`, tags `s9-t0` to `s9-t7`; `verify2/`, `s9-t9` to
  `s9-t13`; `replay2/`; `plans/skel_s9p1*.md`; `sim/*.bin`, the voxelized dumps). Start a fresh clone from the branch.

## Planning, as it worked

- **The scratch clone:** clone the branch into the session's scratchpad as `verify/`, copy `tools/plans/` beside it as
  `plans/`, make each task's change there, check it, commit it and tag it (`s9-t16`, …).
- **The skeleton:** markdown with the prose, where `@@FILE <path>@@` becomes a whole file and `@@EDIT <path> <from>
  <to>@@` an Edit step's (old, new) pairs, `<from>` and `<to>` each a commit or tag, `verify` or `snap:<name>`;
  `@@SOURCE <commit>@@` pins the files after it to a commit. `gen_plan.py <skeleton> <out>`, with `GEN_CLONE` set to
  the clone, makes the plan's text; paths are under `spikes/block-render/` unless they start with `extern/`, `docs/` or
  `minecraft/`. A small Python script splices it into the plan, keeping the file's line endings.
- **Before committing the plan:** replay the new tasks in a fresh clone of the repo with `run_plan_s9.py replay <plan>
  <clone> 16 <last>` and compare each commit's tree with the clone's tag (`git rev-parse <commit>^{tree}`); after any
  later edit to the plan's text, `trees_match.py` checks it still makes them.
- **The runner's step kinds** (`run_plan_s9.py`): file steps titled Write, Edit, Delete or Copy; bash steps titled
  "Build CyberCraft Tweaks", "Run CyberCraft Tweaks' unit tests", "Check the mixins" and "Check that mclink parses",
  each passing only if every quoted text of its Expected paragraph is in what it printed; "Commit"; and stops at
  "Check that the game isn't running", "Ask the user", "In Minecraft", "Install" and "Record". **Part 2 needs more:**
  the overlay's syntax (`luac -p`), `fun_check.lua`'s clicks, a deploy and "In the game" steps; spike 6's `run_plan.py`
  has the first two. Add a step kind before writing a step the runner doesn't know, or it stops there.
- **Expected failures:** `javac` says `method <m> in class <C> cannot be applied to given types` for a changed
  signature; a unit check prints `FAIL: <name>` and `test.sh` exits 1. Guess them, and let the replay correct the
  guess.

## Rules to keep

- **No sub-agents:** work inline.
- **The user does every in-game and Minecraft action:** launching and quitting both games, loading, moving, keys,
  clicks and screenshots. Never drive their mouse or keyboard; ask, then wait for them to say they're done.
  - **On-screen aids:** give them when a test needs positions, headings or counts; the overlay shows V's position and
    heading, the Minecraft block at V's feet, and the nearest mob's distance.
  - **Name what's invisible** before the user looks.
- **`mclink` runs only with the user's knowledge,** with Cyberpunk closed: it takes the game's place on the link.
  Running a script in the user's Minecraft (step 0, diagnostics) needs their leave; it did each time in Part 1.
- **Minecraft's player:** ask before touching the inventory, the off hand or the game mode; guard every change
  (`execute unless items …`), and put everything back.
- **Deep reviews:** trace every claim to its source, simulate the in-game steps, and break each new check once.
- **Settle important unknowns** before moving on, rather than leaving them as next steps. Offer that.
- **Testable steps first:** sequence the work so each step can be checked against something real right then.
- **Read values from the game:** the user wants the game's real numbers, fed into the game's own systems.
- **Installs and deploys:** CyberCraft Tweaks only with Minecraft stopped (its jars are locked while it runs), the old
  jar renamed `.jar.disabled`; the overlay only with Cyberpunk closed (`tasklist | grep -i -c "cyberpunk2077.exe"`
  prints `0`); each with the user's leave.
- **Downloads** need the user's explicit permission first.
- **Commits** end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`, and go on the spike branch.
- **After the spike:** `superpowers:finishing-a-development-branch`. The user has chosen a local fast-forward merge to
  `main` every time so far; ask again. The time-of-day sync the user asked for waits until after spike 9.

## Gotchas

- **Bash's working directory:** a `cd` in a Bash call moves it for later calls. Wrap it in a subshell,
  `(cd … && …)`.
- **Backslashes:** the Bash tool turns `\\` into `\`, even in quoted heredocs. Write files that hold backslashes (an
  `mclink` script's `dump` path) with the Write or Edit tools.
- **Line endings** differ from file to file in the working copy (`core.autocrlf` is on; the spec and the results doc
  are CRLF, the plan LF, as of this writing), and `sed -i` makes a CRLF file LF. Check with `file` first, and edit with
  the Edit tool or Python that keeps the file's endings.
- **Python:** `uv run --no-project`; the simulation needs `--with pillow --with numpy`.
- **Waiting:** a foreground `sleep` is blocked. Run a polling `until` loop in the background, and wait for its
  notification.
- **Minecraft's log** is `%APPDATA%\ModrinthApp\profiles\CyberCraft\logs\latest.log`, with chat replies as `[CHAT]`
  lines; note its line count before a run and read what comes after.
- **`mclink`** is `spikes/block-render/build/RelWithDebInfo/mclink.exe`; a script's parse check appends a line it
  doesn't know (`parse-check-stop here`), so nothing runs even with Minecraft open, but the 256-character limit is
  checked only as a command is typed.
