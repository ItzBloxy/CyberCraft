"""Reading and applying the block-render plan's steps (docs/superpowers/plans/2026-10-01-block-render.md).

A step's title says what it does:
- Write `path`: the step's first code block becomes the whole file.
- Edit `path`: the step's code blocks are (old, new) pairs, applied in order; each old block must occur exactly once.
- Delete `path`: the file is deleted.
- Copy `from` to `to`: the file is copied unchanged.
Paths are relative to spikes/block-render/, except extern/..., docs/... and minecraft/..., which are relative to the
repo root.
Files are written with LF line endings (the repo's text=auto takes care of the rest).
"""
import re
import shutil
from dataclasses import dataclass
from pathlib import Path

FENCE = re.compile(r"^(`{3,})[a-z]*\n(.*?)^\1[ \t]*$", re.M | re.S)
PATH = re.compile(r"`([^`]+\.(?:h|c|cpp|txt|md|py|json|hlsl|lua|reds|java|sh))`")
TASK = re.compile(r"^### Task (\d+):(.*)$", re.M)
STEP = re.compile(r"^- \[[ x]\] \*\*Step (\d+): (.+?)\*\*\s*$", re.M)


@dataclass
class Step:
    task: int
    number: int
    title: str
    body: str  # the text after the title, up to the next step or task

    def blocks(self):
        return [m.group(2) for m in FENCE.finditer(self.body)]

    def paths(self):
        return PATH.findall(self.title)


def read_plan(plan_path):
    plan = Path(plan_path).read_text(encoding="utf-8").replace("\r\n", "\n")
    tasks = list(TASK.finditer(plan))
    steps = []
    for i, task in enumerate(tasks):
        end = tasks[i + 1].start() if i + 1 < len(tasks) else len(plan)
        found = list(STEP.finditer(plan, task.end(), end))
        for k, step in enumerate(found):
            stop = found[k + 1].start() if k + 1 < len(found) else end
            steps.append(Step(int(task.group(1)), int(step.group(1)), step.group(2), plan[step.end():stop]))
    return steps


def target(repo, path):
    repo = Path(repo)
    return repo / path if path.startswith(("extern/", "docs/", "minecraft/")) else repo / "spikes" / "block-render" / path


def is_file_step(step):
    return step.title.split(" ", 1)[0] in ("Write", "Edit", "Delete", "Copy")


def apply(step, repo):
    verb = step.title.split(" ", 1)[0]
    paths = step.paths()
    if not paths:
        raise SystemExit(f"Task {step.task} step {step.number} names no file: {step.title}")
    path = target(repo, paths[-1] if verb == "Copy" else paths[0])
    blocks = step.blocks()
    if verb == "Write":
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(blocks[0].encode("utf-8"))
    elif verb == "Edit":
        text = path.read_bytes().decode("utf-8").replace("\r\n", "\n")
        if len(blocks) % 2:
            raise SystemExit(f"Task {step.task} step {step.number} has an odd number of code blocks")
        for old, new in zip(blocks[0::2], blocks[1::2]):
            count = text.count(old)
            if count != 1:
                raise SystemExit(f"Task {step.task} step {step.number}: the old text occurs {count} times in {path}:\n{old}")
            text = text.replace(old, new)
        path.write_bytes(text.encode("utf-8"))
    elif verb == "Delete":
        path.unlink()
    elif verb == "Copy":
        if len(paths) != 2:
            raise SystemExit(f"Task {step.task} step {step.number}: a copy names two files")
        path.parent.mkdir(parents=True, exist_ok=True)
        text = target(repo, paths[0]).read_bytes().decode("utf-8").replace("\r\n", "\n")
        path.write_bytes(text.encode("utf-8"))
    else:
        raise SystemExit(f"Task {step.task} step {step.number} isn't a file step: {step.title}")
