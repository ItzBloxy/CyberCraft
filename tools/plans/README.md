# Plan tooling, as spikes 3 to 6 used it

Spike 3 (block rendering) kept its plan verified: every code step was compiled and tested in a scratch copy first, then
the plan's tasks were replayed in a fresh clone exactly as written, checking every expected result, before they ran in
the real repo. These scripts did that, and spikes 4 (the Minecraft link), 5 (collision and movement) and 6 (NPCs and
combat) did the same. They live in the session's scratchpad while in use; this is a copy, so the next spike can start
from them. Adapt before reuse.

- **`planlib.py`:** reads a plan's tasks and steps, and applies a step whose title says `Write`, `Edit`, `Delete` or
  `Copy` a file: a Write step's first code block is the whole file; an Edit step's code blocks are (old, new) pairs,
  each old block found exactly once.
- **`gen_plan.py`:** builds the plan from skeleton files. A line `@@FILE <path>@@` becomes a code block holding the
  file, from the scratch copy (`verify/`), or from a commit after a `@@SOURCE <commit>@@` line, so a task that has run
  keeps showing its files as they were then. Pin each task to its commit once it has run. A line
  `@@EDIT <path> <from> <to>@@` becomes an Edit step's pairs, between two versions of a file: the scratch copy, a
  commit, or a snapshot between two tasks (`snap:<name>`, in `snap/<name>/`), for a file more than one task changes.
- **`pairs.py`** (spike 4's Part 2): the pairs for `@@EDIT`. Each is one change with as little context as keeps its
  old block unique, readable (a line with words in it, not only braces), and free of blank lines at its edges.
- **`apply_step.py`:** applies one step, to the scratch copy or the repo.
- **`run_plan.py`** (spike 4): replays tasks in a fresh clone, or executes them in the real repo, ticking each task's
  boxes before its commit. It checks each build's expected failure or test count, with no warnings, `mclink`'s
  self-check, the overlay's Lua syntax (`luac -p`), and the tools' Python tests, failing or passing. Under `%TEMP%`,
  MSBuild doesn't track header changes (MSB8029), so a replay rebuilds from clean after a step changes a header.
  `replay|execute <plan> <repo> [first task] [last task] [first step]` resumes a stopped run at its step. A unit test
  can be made to fail when run (not only to fail to compile), and a run stops at the first step done by hand (a
  deploy, the game, a record). Executing ticks a task only up to its commit, with `tick_steps.py`.
- **`replay.py`, `execute.py`** (spike 3): the same, in two scripts, with spike 3's step kinds; they also check the
  DLL's exports.
- **`tick.py`, `tick_steps.py`:** tick a whole task, or single steps.
- **`b.sh`:** builds and tests the scratch copy; `b.sh "" "" clean` rebuilds from clean.
- **`mutate.py`** (spike 4's Part 3): breaks the new code once per mutation, an exact text swap in one file, rebuilds,
  and checks that the test named for it fails. A crash counts as not caught, since it hides every later test. The list
  is Part 3's; under `%TEMP%` an incremental build can miss a change, so `clean` reruns chosen ones from clean.
- **`find_calls.py`, `disasm.py`** (spike 4's Part 3): find the game executable's calls through chosen USER32 imports
  and disassemble the functions that make them, or any address, with capstone. They showed how Cyberpunk reads Raw
  Input (the minecraft-link spec's "Cyberpunk's input").

**Before reuse:** the paths point at `spikes/block-render` (`planlib.target`, `gen_plan.repo_path`,
`run_plan.project`) and at spike 4's scratchpad (`b.sh`). The runners know a few step kinds only their spike had:
`run_plan.py` copies SkyCraft's header from the scratchpad's `dl/` and checks the plan's hashes, and runs `mclink`
without Minecraft; `replay.py` has spike 3's header download, the atlas from the user's jar and the dump conversion.
`run_plan.py`'s last task defaults to Part 1's 10: pass the tasks to run.

**Spike 5's Part 2 versions** replaced `run_plan.py`, `gen_plan.py`, `mutate.py` and `b.sh`, and added three scripts.
Their paths point at that session's scratchpad (`verify2/`, `snap6/`, `replay6/`): adapt before reuse.
- **`run_plan.py`** also checks that BlockRender imports nothing from PhysX (`dumpbin /imports`), and in `execute` mode
  ran Part 2 in the real repo, task range by task range, stopping at each step done by hand.
- **`gen_plan.py`** reads the scratch copy from `GEN_CLONE` when set: a worktree at the clone's head, so a plan can be
  generated while `mutate.py` is changing the clone's files.
- **`mutate.py`** runs its whole list when imported, so don't import it to inspect the list: read the file.
- **`fold.sh`** folds fixup commits into the tasks they belong to, rebuilding the clone's history from the tag before the
  first one named, and moves the tags only if the rebuilt tree is the same.
- **`replay_tasks.sh`** replays task ranges in a fresh clone of the real repo and compares each task's commit with the
  clone's tag for it; **`replay_from.sh`** replays again from a task whose commit already matched, after a fold.

**Spike 5's Part 3 versions** replaced `run_plan.py`, `gen_plan.py`, `mutate.py`, `fold.sh`, `replay_tasks.sh` and
`b.sh`, and added three scripts. Their paths point at that session's scratchpad (`verify3/`, `skel7/`, `replay7/`), and
`replay_tasks.sh` names Part 3's task ranges: adapt before reuse.
- **`run_plan.py`** in `execute` mode ran Part 3 in the real repo, task range by task range, ticking each task's boxes up
  to its commit, and stopping at deploys and the game's steps.
- **`assemble.py`** puts a part generated from skeletons into the plan file, after the parts already there.
- **`physx-audit/slots.py`** disassembles the functions at chosen slots of the game's PhysX vtables (`physics`,
  `scene`, `static`, `dynamic`, `shape`, `aggregate`, or an RVA), listing each one's strings, calls and vtables, to
  match it to PhysX 3.4's source; **`physx-audit/tree.py`** follows a function's calls a few levels down. They made the
  audit's Part 3 addendum. They need capstone, `uv run --no-project --with capstone python slots.py static 21`, a
  download the first time.

**Spike 6's versions** (NPCs and combat, Parts 1 and 2) replaced `planlib.py`, `gen_plan.py`, `run_plan.py`,
`mutate.py` and `b.sh`, and added `trees_match.py`, `rewrap.py` and `hud_check.lua`. Their paths point at that session's
scratchpad, where the scratch copy was `verify/`, beside `plans/`, and the snapshots `snap8/`: adapt before reuse.
- **`planlib.py` and `gen_plan.py`** know the redscript module: a `.reds` path in a step's title, shown as `swift`.
- **`run_plan.py`** runs the module's offline compile: a step titled "Compile the module …" or "Check that the compile
  catches a slip …" runs its bash block, in the clone or the repo, and every quoted text of its Expected paragraph
  must be in what it printed. In `execute` mode it ran Part 2 in the real repo a task at a time (`execute <plan>
  D:/Cyberpunk-Minecraft 32 32`), stopping at deploys and the game's steps.
- **`mutate.py`** has step 4's fixes' list (Tasks 32 to 35, 9 mutations). Each mutation names the test that must fail,
  and a header's names a source to touch too, since under `%TEMP%` MSBuild doesn't see a header change. Running it runs
  the whole list.
- **`trees_match.py`** applies a plan's file steps to a clone, task by task, without building, and compares each
  task's tree with a verified clone's tag, `<prefix><task>`:
  `trees_match.py <plan> <clone> <verified clone> <first> <last> <prefix>`, the clone checked out at the commit the
  tasks start from. A quick check, after editing a plan, that its text still makes the code tested.
- **`rewrap.py`** rewraps the Markdown paragraphs or list items that start with given text to a width, with LF line
  endings: `rewrap.py <file> 120 "<start>" …`.
- **`hud_check.lua`** runs the CET overlay's `init.lua` under Lua 5.4 with CET's functions stubbed, against the
  `status.txt` in the current folder, and prints the lines and buttons the overlay shows: `lua hud_check.lua
  <path to init.lua>`.

**Spike 6's Part 3 versions** replaced `planlib.py`, `gen_plan.py`, `run_plan.py`, `mutate.py`, `b.sh` and `fold.sh`,
and added `assemble3.py`. Their paths point at that session's scratchpad, where the scratch copy was `verify/` beside
`plans/`, with Part 3's tasks tagged `p3-t38` to `p3-t47`: adapt before reuse.
- **`planlib.py` and `gen_plan.py`** take `minecraft/` paths from the repo's root, as they take `extern/` and `docs/`,
  and Java and shell files.
- **`run_plan.py`** runs a step titled "Build CyberCraft Tweaks" as it runs the module's compile: its bash block, and
  every quoted text of its Expected paragraph must be in what it printed. Its log is `<mode>_p3.log`.
- **`mutate.py`** has Part 3's list (Tasks 38 to 40 and 45, 38 mutations), then step 5b's fix's (Task 49, 3 more). A
  header's mutation names the source to touch: the one that compiles the changed declaration in, which for a default
  member initializer is the test that constructs the struct.
- **`fold.sh`** works on `verify/` and the tags `p3-t<n>`. A task with no code (one with the user) needs a tag of its
  own on the task before's commit first, as `p3-t42` was put on `p3-t41`'s.
- **`assemble3.py`** puts a generated part after Task 37 and before "Found while planning", brings the plan's header and
  "Before you start" up to Part 3, and adds "Found while planning Part 3" at the end. **`assemble_fix.py`** does the
  same for step 5b's fix: its tasks after Task 48, the close renumbered, and "Step 5b's fix" at the end; and
  **`assemble_fun.py`** for the overlay's Fun controls: Task 53 before the close, renumbered 54, and "The Fun controls"
  at the end.
- **`fun_check.lua`** runs the overlay's `init.lua` under Lua 5.4 with CET's functions stubbed, in a folder holding a
  `status.txt`, and plays BlockRender's side of `blockrender.request`, reading and deleting it each frame:
  `lua fun_check.lua <init.lua> show` prints what the overlay shows, with "More controls" and "Fun" open;
  `click <label>…` clicks those buttons and prints each request written, in order; `hold <label>…` first leaves the
  file unread for 5 frames, and prints it and the waiting line.

A step whose title starts with "Write", "Edit", "Delete" or "Copy" is a file step to all of these tools: a step that
records results by hand is titled "Record …".

The module's offline compile itself is `spikes/block-render/tools/check_reds.py`, with Codeware's scripts as an extra
folder for the parts that need Codeware.

**Spike 9's versions** (Minecraft's mobs finding their way, Part 1) added `run_plan_s9.py`, `mutate_s9p1.py`,
`mutate_s9p2.py` and `paths-sim/`, and used `planlib.py`, `gen_plan.py`, `pairs.py`, `tick_steps.py`,
`trees_match.py` and `rewrap.py` as they are here. That session's scratchpad held the scratch clones, `verify/` (tags
`s9-t0` to `s9-t7`) and `verify2/` (`s9-t9` to `s9-t13`), beside `plans/`, `sim/` and `voxdump/`: adapt before reuse.
- **`run_plan_s9.py`** knows CyberCraft Tweaks' steps: "Build CyberCraft Tweaks", "Run CyberCraft Tweaks' unit
  tests", "Check the mixins" and "Check that mclink parses" run their bash block, and every quoted text of the
  Expected paragraph must be in what it printed (a replay copies the real repo's `mclink.exe` into the clone first).
  Steps titled "Check that the game isn't running", "Ask the user", "In Minecraft", "Install" or "Record" stop the run.
  A commit's `git add` may be continued over lines. `replay|execute <plan> <repo> [first] [last] [first step]`.
- **`mutate_s9p1.py`, `mutate_s9p2.py`:** `CellRule`'s mutations, Part 1's 12 and the fence rule's 18, each naming the
  unit check that must fail: `mutate_s9p2.py <clone>/minecraft/cybercraft-tweaks`.
- **`paths-sim/`:** the city simulated as the mod sees it. `voxdump` (`voxdump.cpp`, built with its `CMakeLists.txt`
  and VS's cmake) places a dump's static triangles around a stand-in V's feet as `mclink`'s `dump` does, voxelizes them
  with BlockRender's own `VoxelizeRegion`, and writes SkyCraft's `ColBlock`s: `voxdump <collision.obj> <feet x y z>
  <radius> <below> <above> <out.bin>`; step 0's ground was `2026-10-01_14-37-07` at 1280.5 6.14 2007.5, 40 32 16.
  `analyze.py` applies 1.7.0's cell rule and 26.3's walk rules to the cells, floods what a mob reaches from V, and
  estimates a stroll's odds (it needs `uv run --no-project --with pillow --with numpy`); `fences.py` does the same with
  1.7.1's fences. The others probe one question each: sight rays (`sight.py`), pets' landings (`teleport.py`), the
  road and its hollows (`road.py`, `hollow_s9.py`, `gaps_s9.py`, `probe_s9r3.py`, `probe_s9r4.py`).

**Spike 9's Part 2** (the overlay's Paths rows and run F) changed `fun_check.lua` and `run_plan_s9.py`, and added
`mutate_s9_overlay.py` and more of `paths-sim/`. Planning used the session's scratchpad: the scratch clone `verify/`
(tags `s9-t15` to `s9-t17`), `plans/skel_s9p2.md` and `plans/found_s9p2.md`, and `sim/` (the dumps voxelized).
- **`fun_check.lua`** takes `FUN_V="<x> <y> <z> <forward x> <forward y>"`: V stands there, in the game's world
  coordinates, so `show` prints the lines that need her, run F's spots among them. Without it there's no V, as before.
- **`run_plan_s9.py`** also runs "Check the overlay's syntax", "Run the overlay offline" and "Check that the script
  types the overlay's commands" as bash steps, and stops at "Deploy" and "In the game".
- **`mutate_s9_overlay.py`** breaks Part 2's checks once each, 17 mutations, in a clone at Task 17's commit with these
  tools and `mclink.exe`: `mutate_s9_overlay.py <plan> <clone>`.
- **`paths-sim/`** gained `simlib.py`, the walk rules with 1.7.1's cells as a library, and the scripts on it, each
  run on `voxdump`'s cells of a dump placed where the game puts it (V's own feet in Minecraft's axes, (x, z, -y) of
  the world): level maps (`areamap.py`, `ascii.py`, `ascii2.py`, `voxmap.py`, `voxmap2.py`), the areas a level
  splits into (`components.py`) and an area's odds (`areas.py`), where the Paths rows' summons land (`summons.py`),
  the husk's hiding places (`raylib.py`, `hidden.py`, `hidden7.py`, `robust.py`), and pets' jump targets (`jumps.py`,
  on V's apartment's dump), each reading its arguments from `sys.argv` near its top.
