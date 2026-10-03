# Plan tooling, as spikes 3 and 4 used it

Spike 3 (block rendering) kept its plan verified: every code step was compiled and tested in a scratch copy first, then
the plan's tasks were replayed in a fresh clone exactly as written, checking every expected result, before they ran in
the real repo. These scripts did that, and spike 4 (the Minecraft link) did the same. They live in the session's
scratchpad while in use; this is a copy, so the next spike can start from them. Adapt before reuse.

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
