# Handoff: execute PhysX probe revision 2 (Tasks 16 to 22)

Written 2026-10-01, at the end of the session that audited the game's PhysX, revised the spec and wrote the plan.
Read this first, then the plan.

## Start here

- **Repo and branch:** `D:\Cyberpunk-Minecraft`, branch `spike/physx-probe`. Work stays on this branch, never on `main`.
- **What to execute:** Tasks 16 to 22 of the plan, `docs/superpowers/plans/2026-10-01-physx-probe.md`, from its
  "Revision 2" section on. Run them in order with `superpowers:executing-plans`, inline in the session, committing at
  the end of each task as the plan says.
- **No sub-agents:** the user asked for none, and chose inline execution. The plan's header recommends
  `subagent-driven-development`; don't use it.
- **Background, if needed:**
  - `docs/superpowers/specs/2026-10-01-physx-probe-design.md`: revision 2, approved;
  - `docs/spikes/2026-10-01-physx-abi-audit.md`: why the code is shaped the way it is.
- **A first message to start the next session with:**
  > Read docs/superpowers/handoffs/2026-10-01-physx-probe-rev2.md and execute Tasks 16 to 22 of the plan.

## Where things stand

- **Done:**
  - Tasks 1 to 14 (revision 1);
  - the version-check fix `e3cbc0b`;
  - the audit `d83b534`;
  - spec revision 2 `6c56c16`;
  - the plan's revision 2 `64d8132`;
  - this handoff.
- **Next:** Task 16, Step 1. Tasks 16 to 21 need no game. Task 22 needs the user at the keyboard and the game running.
- **The tests:** 42 pass today. The plan expects 50, 54, 60, 60, 55 and 55 after Tasks 16 to 21.
- **The plan's code is verified.** In a scratch copy, every Write, Edit and Delete step of Tasks 16 to 21 was applied to
  this branch. The result built with no compiler warnings, passed 55 tests, exported exactly `Main`, `Query` and
  `Supports`, and imported nothing from PhysX. So a failure while following the plan most likely means a transcription
  slip: apply the code blocks verbatim (see below) rather than retyping them.

## The game install, right now

- **The plugin folder,** `E:\Cyberpunk2077\red4ext\plugins\PhysXProbe\`, holds three things:
  - `PhysXProbe.dll.disabled`: revision 1, renamed after it froze the game, so RED4ext won't load it. The game is safe
    to play as it is.
  - `PhysXProbe.pdb`.
  - `dumps\`: the three revision 1 dumps.
- **The build would install into the game.** The repo's CMake cache still has `CP2077_DIR=E:/Cyberpunk2077`, so every
  build would copy a half-finished plugin into the game. Task 16, Step 1 clears it before any build, and Task 22,
  Step 1 sets it again and removes the `.disabled` file. **Don't build anything before Task 16, Step 1.**
- **A leftover crash report.** The freeze left one in
  `%LOCALAPPDATA%\REDEngine\ReportQueue\Cyberpunk2077-20261001-020952-8188-15048`. The user was told they may delete it
  rather than send it to CD Projekt, so leave it to them.

## Rules to keep

- **No sub-agents:** work inline.
- **The user does every in-game action:** launching, loading, moving, F10 and screenshots. Never drive their mouse or
  keyboard. Ask, then wait for them to say they're done.
- **Never build or deploy while Cyberpunk is running,** because the running game locks the DLL.
  `tasklist | grep -i -c "cyberpunk2077.exe"` must print `0`.
- **If the game freezes, force-close it only when the user asks.** Capture the evidence first (see "If Task 22 goes
  wrong").
- **Commits:** end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. The plan's commit commands already
  do.
- **After Task 22:** use `superpowers:finishing-a-development-branch`.

## Gotchas from the last session

- **Backslashes:** the Bash tool turns `\\` inside a command into `\`. A file written through a shell heredoc or a
  `python -c` string loses backslashes; one header's `\brief` turned into a backspace that way. Write files with the
  Write tool, or build backslashes in Python with `chr(92)`.
- **Tool paths:** `cmake`, `ctest` and `dumpbin` aren't on `PATH`; the plan's command blocks set them. MSBuild switches
  must be `-m`, not `/m`, because Git Bash rewrites `/m` into a path.
- **Python:** run it with `uv run --no-project`. An inline Python script gets Windows paths (`E:/...`), not
  `/e/...`; only command-line arguments are converted.
- **Batch files:** `cmd //c` needs a batch file's full Windows path (`cygpath -w`).
- **Scratch builds under `%TEMP%`:** MSBuild doesn't track header dependencies for a build tree under `%TEMP%`
  (warning MSB8029), so incremental builds there miss header changes. Use `--clean-first` there. The repo's own
  `build/` folder doesn't have this problem.

## Applying the plan's code verbatim

Save this as `apply_step.py` in the session's scratchpad (with the Write tool, because of the backslashes). It applies
one Write, Edit or Delete step exactly as the plan has it; it was checked against the verified replay, and produces
the same 23 files. For Task 20, Step 9 (deleting `TestGuard.cpp`), run the plan's `git rm` instead.
```python
"""Apply one Write, Edit or Delete step of the PhysX probe plan, exactly as the plan has it.

    uv run --no-project python apply_step.py <plan.md> <task> <step>

Write: the step's first code block becomes the whole file.
Edit: the step's code blocks are (old, new) pairs, applied in order; each old block must occur exactly once.
Delete: the file is deleted.
Paths in step titles are relative to spikes/physx-probe/, except extern/... and docs/..., which are relative to the
repo root. Files are written with LF line endings (the repo's text=auto takes care of the rest).
"""
import re
import sys
from pathlib import Path

plan_path = Path(sys.argv[1]).resolve()
task_no, step_no = int(sys.argv[2]), int(sys.argv[3])
repo = plan_path.parents[3]  # docs/superpowers/plans/<plan>.md
plan = plan_path.read_text(encoding="utf-8").replace("\r\n", "\n")

tasks = list(re.finditer(r"^### Task (\d+):", plan, re.M))
index = next(i for i, m in enumerate(tasks) if int(m.group(1)) == task_no)
task_start = tasks[index].end()
task_end = tasks[index + 1].start() if index + 1 < len(tasks) else len(plan)
steps = list(re.compile(r"^- \[[ x]\] \*\*Step (\d+): (.+?)\*\*\s*$", re.M).finditer(plan, task_start, task_end))
k = next(i for i, s in enumerate(steps) if int(s.group(1)) == step_no)
title = steps[k].group(2)
stop = steps[k + 1].start() if k + 1 < len(steps) else task_end
blocks = [m.group(1) for m in re.compile(r"^```[a-z]*\n(.*?)^```\s*$", re.M | re.S).finditer(plan, steps[k].end(), stop)]
match = re.search(r"`([^`]+\.(?:h|cpp|txt|md|py|json))`", title)
if not match:
    sys.exit(f"Task {task_no} step {step_no} names no file: {title}")
path = match.group(1)
target = repo / (path if path.startswith(("extern/", "docs/")) else "spikes/physx-probe/" + path)

if title.startswith("Write"):
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(blocks[0].encode("utf-8"))
elif title.startswith("Edit"):
    text = target.read_bytes().decode("utf-8").replace("\r\n", "\n")
    if len(blocks) % 2:
        sys.exit(f"Task {task_no} step {step_no} has an odd number of code blocks")
    for old, new in zip(blocks[0::2], blocks[1::2]):
        count = text.count(old)
        if count != 1:
            sys.exit(f"Task {task_no} step {step_no}: the old text occurs {count} times in {target}:\n{old}")
        text = text.replace(old, new)
    target.write_bytes(text.encode("utf-8"))
elif title.startswith("Delete"):
    target.unlink()
else:
    sys.exit(f"Task {task_no} step {step_no} isn't a Write, Edit or Delete step: {title}")
print(f"Task {task_no} step {step_no}: {title}")
```
For example, `uv run --no-project python apply_step.py /d/Cyberpunk-Minecraft/docs/superpowers/plans/2026-10-01-physx-probe.md 16 2`
writes `tests/TestAudit.cpp`. The steps that change files:

| Task | Steps |
|---|---|
| 16 | 2, 3, 5, 7 |
| 17 | 1, 2, 4, 5, 6 |
| 18 | 1, 2, 4, 5, 6 |
| 19 | 1, 3, 4, 5, 6 |
| 20 | 1, 2, 3, 4, 5, 6, 7, 8, 10 (and 9 by `git rm`) |
| 21 | 1, 2 |

The others are commands to run, with their expected output. Run them, and stop to investigate when the output
differs.

## If Task 22 goes wrong

- **The game freezes or crashes:**
  1. Ask the user before force-closing it.
  2. Read the probe log, `E:\Cyberpunk2077\red4ext\logs\physxprobe-*.log` (the newest).
  3. Read the newest folder under `%LOCALAPPDATA%\REDEngine\ReportQueue\`: `stacktrace.txt`, `report.txt` and
     `Cyberpunk2077.dmp`.
  4. If needed, decode the dump. Last time a small standard-library Python minidump reader got the exception record and
     scanned the faulting thread's stack for return addresses. `dumpbin /disasm` on `build/RelWithDebInfo/PhysXProbe.dll`
     then named the probe's functions, using its PDB. That build must be byte-identical to the installed DLL.
- **Compatibility step 1 fails:** the game's PhysX DLLs have changed, for example after a game patch. Redo the audit
  before anything else, using the method in the audit doc.
- **The census skips objects with unaudited vtables:** the live world has a class the audit didn't cover. Audit it
  before widening `GameAbi`. Mesh tables have never been seen in a live dump.
- **The census skips unreadable objects:** the world changed under the read, which is a race. Record it; it's the
  spec's threading risk.

## What Task 22 should find out

- Whether live meshes use the audited tables (BV33 and BV34 triangle meshes, convex meshes, heightfields). The crash
  dump only showed live shapes, static actors and the scene.
- Whether the city's scene uses `eREQUIRE_RW_LOCK`. The startup scene doesn't.
- Which axis the game's field of view is (`render_dump.py --fov-axis`).
- Whether the coordinate check passes in the city. It passed on the stand-in's world at 1.805 m.
