"""Apply a plan's file steps, task by task, to a clone, and compare each task's tree with a verified clone's tag.

    uv run --no-project python trees_match.py <plan.md> <clone> <verified clone> <first task> <last task> <tag prefix>

No builds: this checks only that the plan's text makes exactly the files the verified clone built and tested.
"""
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import planlib  # noqa: E402

plan_path, clone, verified = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
first, last, prefix = int(sys.argv[4]), int(sys.argv[5]), sys.argv[6]


def git(repo, *args):
    return subprocess.run(["git", "-C", str(repo), *args], capture_output=True, text=True, check=True).stdout.strip()


steps = planlib.read_plan(plan_path)
git(clone, "fetch", "-q", str(verified), f"refs/tags/{prefix}*:refs/tags/{prefix}*")
verified = clone
ok = True
for task in range(first, last + 1):
    applied = 0
    for step in steps:
        if step.task == task and planlib.is_file_step(step):
            planlib.apply(step, clone)
            applied += 1
    git(clone, "add", "-A")
    tree = git(clone, "write-tree")
    want = git(verified, "rev-parse", f"{prefix}{task}^{{tree}}")
    same = tree == want
    ok = ok and same
    print(f"Task {task}: {applied} file steps; tree {tree[:10]} vs {prefix}{task} {want[:10]}: {'identical' if same else 'DIFFERENT'}")
    if not same:
        print(git(clone, "diff", "--cached", "--stat", want))
sys.exit(0 if ok else 1)
