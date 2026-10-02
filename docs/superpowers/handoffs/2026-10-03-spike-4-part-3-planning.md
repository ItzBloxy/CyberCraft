# Handoff: plan Part 3 of spike 4 (input)

Written 2026-10-03, at the end of the session that ran Part 2 of spike 4 (the Minecraft link in the game) and settled
the one question it left for Part 3. Read this first.

## Start here

- **Repo and branch:** `D:\Cyberpunk-Minecraft`, branch `spike/minecraft-link`. Spike work stays on the branch, never on
  `main`. The repo has no git remote.
- **Where spike 4 stands:**
  - **Part 1 is done:** step 1, `mclink` against Minecraft without Cyberpunk, passed;
  - **Part 2 is done:** step 2, the link, blocks and the HUD in the game, passed. BlockRender 0.9.1 is installed;
  - **an `mclink` experiment** then settled where Minecraft's eye goes among blocks (see below).
- **Next: the plan's Task 29, step 2:** plan Part 3, which is the spec's step 3, input. Its tasks start at Task 30 and
  are appended to the plan. Then step 3 commits `docs/superpowers`. Part 2's close is already committed (`796ec82`),
  so the message is `minecraft-link plan: Part 3 planned` rather than the step's `Part 2 closed, Part 3 planned`.
- **Process:** as Parts 1 and 2 were planned:
  1. `superpowers:writing-plans`;
  2. every code step compiled and tested in a scratch clone (see "Planning, as it worked" below);
  3. the tasks replayed in a fresh clone by `tools/plans/run_plan.py`, with the replayed files compared to the scratch
     copy's;
  4. spec changes found while planning go in the spec's "Changes while planning";
  5. then offer execution, inline.

  **Expect the user to ask for a deep review of the plan** before executing it: trace every claim to its source
  (SkyCraft's code, the game's files), simulate the in-game steps end to end, and make each new test fail once.
- **No sub-agents:** the user wants everything done inline, plan execution included.
- **Read for background, in this order:**
  - this handoff;
  - **the spec**, `docs/superpowers/specs/2026-10-02-minecraft-link-design.md`:
    - "Purpose" and "Decisions from the brainstorm";
    - "Cyberpunk's input", under "Environment";
    - "`input/`", under "Components";
    - "Input", which includes "Typed commands";
    - "Step 3", and `build on|off` in "Requests";
    - in "Safety", "Nothing stays held" and "Off by default";
    - in "Testing", `Scancodes`, `BuildMode`, `RawInputHook` and `FocusWatch`;
    - risks 1, 2, 6 and 7;
    - "Changes from step 2";
  - **the results**, `docs/spikes/2026-10-02-minecraft-link-results.md`: step 2's "Verdict", its "Surprises, and what
    they mean for Part 3", and "Minecraft's eye among blocks";
  - **the plan**, `docs/superpowers/plans/2026-10-02-minecraft-link.md`: Part 2's introduction, whose layout Part 3
    should follow; "How Part 2 went"; and Task 27, the shape of an in-game task;
  - `tools/plans/README.md`.
- **A first message to start the next session with:**
  > Read docs/superpowers/handoffs/2026-10-03-spike-4-part-3-planning.md and plan Part 3.

## What Part 3 builds

The spec's step 3, in short:
- **F8 build mode:** Minecraft takes the mouse buttons, the wheel and keys 1 to 9 from Cyberpunk, and V's own actions
  with them stop.
- **The hook:** a patch of `Cyberpunk2077.exe`'s import of `USER32!GetRawInputData`. Cyberpunk reads keyboard and mouse
  only through Raw Input. The hook calls the original, then applies `BuildMode`'s decision to the event: pass, forward
  and hide, toggle, or leave and pass.
- **Forwarding:** as SkyCraft input events through the link's input ring:
  - keys as SDL scancodes, through `Scancodes`;
  - buttons 1, 2 and 3 for left, middle and right;
  - the wheel as scroll, in units of 120.
- **Hiding:** a cleared button event keeps its mouse movement, and a cleared key becomes a no-op. Risk 1's fallbacks
  apply per input if Cyberpunk still reacts.
- **Leaving build mode** releases everything. The menu keys leave it and pass to the game, and so does Minecraft going.
  Build mode turns on only while Minecraft is in its world.
- **`FocusWatch`:** a thread that releases held inputs once when the game stops being in front, 10 times a second.
- **The rest:**
  - the overlay gets a line with build mode and the hotbar slot;
  - a `build on|off` request;
  - `hud 1`, already the default, shows the HUD in build mode;
  - click latency, with each click a mark.

**Step 3's checks** (the spec's "Step 3"):
- build mode on and off;
- each input blocked from Cyberpunk and acted on by Minecraft;
- building by clicking on the street, walking, and building elsewhere;
- leaving through each menu key;
- click latency;
- Alt-Tab with a button held.

## What's already there for it

- **`link::Link::Push(const proto::InputEvent&)`:** callable from any thread, behind a mutex. It counts what it drops.
  `CommandTyper` already sends keys and text through it.
- **`Link::Frame::buildMode`:** off sets `SkyState`'s menu flag, so Minecraft releases held keys every frame. Step 1
  checked that this leaves typing working (SkyCraft's `InputBridge.releaseAll` lifts only held keys).
- **`mclink`'s `key` and `click` actions:** step 1's click placed a block exactly where predicted. SkyCraft's `SkyRay`
  hits the floor's triangles, and the block goes in the cell at `floor(hit + normal × 0.4)`. `mclink`'s status line
  now also gives Minecraft's eye (`15b3ae8`).
- **`hooks/ImportPatch.*`:** `FindImport(module, dll, function)` finds an import's slot. It's spike 3's patcher for
  Streamline's imports, ready for `GetRawInputData`.
- **`Plugin.cpp`:**
  - build mode is hard-wired off for now: `HudWanted(g_hudSetting, false, …)`, commented "build mode comes with
    input, in Part 3", and `frame.buildMode = false` in `UpdateLink`;
  - the latency tracker (`Measurements.h`) and bursts are ready for click marks.
- **The overlay** (`tools/blockrender_hud/init.lua`) has three rows: drawing, Minecraft (Link, Unlink and the HUD
  button) and Type.
- **`tools/status_watch.py`** takes readings of the status over time, for the game run.
- **Counts at Part 3's start:** 204 C++ tests and 25 Python ones. BlockRender 0.9.1 (Part 3 makes it 0.10.0).

## What step 2 and the eye experiment found, for Part 3

- **Clicks follow Minecraft's eye.** In the open, the eye stays exactly at V's camera, and a click lands where V aims.
  Inside or under Minecraft's blocks, Minecraft crawls, its eye 0.4 above its feet:
  - for about a second after teleports inside blocks;
  - for as long as a block overhangs 1 to 2 m above the feet.

  A click then lands short: 0.7 m ahead instead of 2.8 m, at 30° down. V doesn't collide with Minecraft's blocks;
  that's the project's PhysX work, not this spike's. So the agreed approach is that Part 3 builds on the open street,
  and its results say where clicks stray (the spec's risk 6).
- **Commands work from the held feet,** not from our floor. Step 2's first guess otherwise was wrong, and the docs are
  corrected.
- **`field` fills the block that holds the feet,** so V stands inside its layer, and `field` and `wipe` pair up only
  at the same block height. Part 3 could make `field` a floor under the feet (`~-1`), with `wipe` to match, if it
  builds on fields.
- **Latency marks:** each Enter, or click, replaces the waiting mark. Samples from several commands in a row were
  wrong. Measure click latency with single clicks while V stands still.
- **The HUD** draws over Cyberpunk's own HUD, steady with frame generation, at 0.25 ms a frame. Minecraft running costs
  Cyberpunk about 2 ms a frame. Cyberpunk keeps running behind other windows, so risk 2 doesn't happen. `FocusWatch`
  is still needed, for inputs held while Alt-Tabbing.
- **Dropped HUD images** happened only around a screenshot.

## The game install, right now

- **BlockRender 0.9.1** is in `E:\Cyberpunk2077\red4ext\plugins\BlockRender\`, with its `shaders`, spike 3's `atlas.bin`
  and `atlas.json`, logs and dumps. Drawing is off, and it's unlinked, unless a request or a button changes that. It
  makes the shared mapping at the game's first update, and a running Minecraft links to it.
- **The overlay** is in `bin\x64\plugins\cyber_engine_tweaks\mods\blockrender_hud\`.
- **Spike 1's probe 0.4.0** and its overlay are still installed; the probe acts only on F10 or its request file.
- **The repo's CMake caches** have `CP2077_DIR` empty, so builds don't install into the game.
- **Minecraft** is the Modrinth App's `CyberCraft` instance, in
  `C:\Users\BLOXY\AppData\Roaming\ModrinthApp\profiles\CyberCraft`:
  - Minecraft 26.3, Fabric Loader 0.19.5, Fabric API 0.161.0+26.3, and `skycraft-fabric-0.1.2.jar`;
  - the Java arguments `--enable-native-access=ALL-UNNAMED -Dskycraft.quitWithSkyrim=false`;
  - its GUI scale fixed at 3, and chat on T;
  - SkyCraft's world in `saves\SkyCraft`: a void world, empty but for the eye experiment's blocks, between x −305
    and −271, z 1995 and 2005.
- **SkyCraft's source** (commit `bfcaf17`) is in an earlier session's scratchpad,
  `C:\Users\BLOXY\AppData\Local\Temp\claude\D--Cyberpunk-Minecraft\818fd1fd-489f-482d-9873-88e6752bad04\scratchpad\skycraft`.
  If it's gone, ask before downloading it again. Its input side is `fabric/src/client/java/dev/skycraft/client/`
  (`InputBridge.java`, `SkyClient.java`), and clicks' rays are `fabric/src/main/java/dev/skycraft/world/SkyRay.java`.

## Planning, as it worked

- **The scratch copy:** clone the branch into the session's scratchpad as `verify/`, and copy `tools/plans/`'s scripts
  next to it. Point `b.sh` at the new scratchpad. Build and test there with `b.sh`, and `b.sh "" "" clean` after a
  header changes, since builds under `%TEMP%` miss header changes (MSB8029).
- **The skeleton:** markdown with the prose, where `@@FILE <path>@@` becomes a whole file and
  `@@EDIT <path> <from> <to>@@` an Edit step's (old, new) pairs. `<from>` and `<to>` are a commit, `verify`, or
  `snap:<name>`: a copy of a file between two tasks, for files more than one task changes. `gen_plan.py` turns the
  skeleton into the plan's text.
- **The replay:** `uv run --no-project python run_plan.py replay <plan or part.md> <fresh clone> <first> <last>`. Pass
  the tasks: the last task defaults to Part 1's 10. A stopped run resumes with a sixth argument, the step. Execute mode
  runs the same steps in the repo, ticking each task's boxes up to its commit.
- **The runner's step kinds:** a step is a file step, a build, a Python test, a check or a stop, by its title:
  - **files:** Write, Edit, Delete or Copy `path`;
  - **builds:** "Build; it must fail" (a compile error), "Build and run the tests; the new one must fail" (a test that
    fails when run) and "Build and run the tests; they must pass" (a count, no warnings);
  - **Python:** "Run the new tool's test; it must fail" and "Run the tools' tests; they must pass";
  - **checks:** "Run mclink without Minecraft" (needs Minecraft closed), "Check the overlay's syntax" and "Commit";
  - **stops:** steps titled "Deploy", "In the game" or "Record", which the user does by hand.

  Anything else stops the run: add a step kind first.
- **Expected failures:** MSVC reports a missing enum value as `C2065 'X': undeclared identifier`, a missing member as
  `C2039`, and a missing header as `C1083`. Guess them, and let the replay correct the guess.

## Rules to keep

- **No sub-agents:** work inline.
- **The user does every in-game action:** launching and quitting both games, loading, moving, keys, clicks and
  screenshots. Never drive their mouse or keyboard; ask, then wait for them to say they're done.
  - Give on-screen aids for positions and headings: the overlay shows V's position, heading and the block at V's feet,
    and the status's `positions=` line.
- **Settle important unknowns** before moving on, rather than leaving them as next steps. Offer that.
- **Testable steps first:** sequence the work so each step can be checked right then.
- **Builds and the game:**
  - Never build with `CP2077_DIR` set, or deploy, while Cyberpunk runs. `tasklist | grep -i -c "cyberpunk2077.exe"`
    must print `0`.
  - Set `CP2077_DIR` only to deploy, and clear it straight after.
- **`mclink` needs Cyberpunk closed:** only one game side may use the mapping.
- **No faults:** plugin code must never fault, because the game's crash handler turns any fault into a crash or a
  freeze. The input hook runs on Cyberpunk's input thread: it must check every size before reading or writing, and
  pass through anything it doesn't fully understand. If the game freezes, read the logs and the newest `ReportQueue`
  folder, and force-close it only when the user asks.
- **Downloads** need the user's explicit permission first.
- **Commits** end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`, and go on the spike branch.
- **After the spike** (Part 3 and the final results): `superpowers:finishing-a-development-branch`. The user has chosen
  a local fast-forward merge to `main` each time so far.

## Gotchas

- **Bash and `cd`:** a `cd` changes the session's working directory for later commands. Wrap it in a subshell:
  `(cd … && …)`.
- **Backslashes and long scripts:** heredocs mangle `\\` and fail on long content. Write scripts with the Write tool.
- **Python on Windows:** printing non-ASCII needs `PYTHONIOENCODING=utf-8`. Run Python with `uv run --no-project`.
- **The unit tests read `shaders/` from the working directory:** run `render_tests.exe` from `spikes/block-render`, as
  `b.sh` does.
- **Waiting:** a bare foreground `sleep` is blocked, but `until <condition>; do sleep 1; done` works. For readings in
  the game, use `tools/status_watch.py`. Its "status written … ago" shows whether the game is still updating.
- **Requests:** write `blockrender.request` atomically (a temporary name, then renamed), and send the next once the
  status's `request=` line shows the last.
- **Minecraft:**
  - its window is hidden while linked, so the user stops it from the Modrinth App;
  - `/save-all` doesn't exist in singleplayer, but Minecraft writes changed chunks to disk at once anyway;
  - its chat replies are in `logs\latest.log`, as `[CHAT]` lines.
- **Tool paths:** `cmake`, `ctest` and `dumpbin` aren't on `PATH`; the plan has their full paths. `luac` (Lua 5.4) is.
  Pass MSBuild switches as `-m`, never `/m`.
- **The scratchpad is per session:** Part 2's scratch copy, skeletons and notes stay in the earlier session's
  scratchpad (`d6098eb0-744d-48fa-a026-8d85ea782f54`). Start a fresh clone from the branch.
