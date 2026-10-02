"""Apply one Write, Edit, Delete or Copy step of the block-render plan, exactly as the plan has it.

    uv run --no-project python apply_step.py <plan.md> <task> <step> [repo]

The repo defaults to the one holding the plan (docs/superpowers/plans/<plan>.md). See planlib.py for the step kinds.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import planlib  # noqa: E402

plan_path = Path(sys.argv[1]).resolve()
task_no, step_no = int(sys.argv[2]), int(sys.argv[3])
repo = Path(sys.argv[4]) if len(sys.argv) > 4 else plan_path.parents[3]
step = next((s for s in planlib.read_plan(plan_path) if s.task == task_no and s.number == step_no), None)
if step is None:
    sys.exit(f"There's no Task {task_no} step {step_no}")
planlib.apply(step, repo)
print(f"Task {task_no} step {step_no}: {step.title}")
