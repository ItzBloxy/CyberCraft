# Plan tooling, as spike 3 used it

Spike 3 (block rendering) kept its plan verified: every code step was compiled and tested in a scratch copy first, then
the plan's tasks were replayed in a fresh clone exactly as written, checking every expected result, before they ran in
the real repo. These scripts did that. They live in the session's scratchpad while in use; this is a copy, so the next
spike can start from them. They're as spike 3 left them: adapt before reuse.

- **`planlib.py`:** reads a plan's tasks and steps, and applies a step whose title says `Write`, `Edit`, `Delete` or
  `Copy` a file: a Write step's first code block is the whole file; an Edit step's code blocks are (old, new) pairs,
  each old block found exactly once.
- **`gen_plan.py`:** builds the plan from skeleton files. A line `@@FILE <path>@@` becomes a code block holding the
  file, from the scratch copy (`verify/`), or from a commit after a `@@SOURCE <commit>@@` line, so a task that has run
  keeps showing its files as they were then. Pin each task to its commit once it has run.
- **`apply_step.py`:** applies one step, to the scratch copy or the repo.
- **`replay.py`:** replays tasks in a fresh clone, checking each build's expected failure or test count, the DLL's
  exports, and the commits. Under `%TEMP%`, MSBuild doesn't track header changes (MSB8029), so it rebuilds from clean
  after a step changes a header.
- **`execute.py`:** the same, in the real repo, ticking each task's boxes before its commit.
- **`tick.py`, `tick_steps.py`:** tick a whole task, or single steps.
- **`b.sh`:** builds and tests the scratch copy; `b.sh "" "" clean` rebuilds from clean.

**Before reuse:** the paths point at `spikes/block-render` (`planlib.target`, `gen_plan.repo_path`, `replay.project`,
`execute.project`) and at spike 3's scratchpad (`b.sh`), and `replay.py` knows a few step kinds only spike 3 had
(Task 1's header download, the atlas from the user's jar, the dump conversion).
