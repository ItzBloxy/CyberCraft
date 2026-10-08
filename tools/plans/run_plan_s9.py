"""Replay spike 9's plan in a fresh clone, or execute it in the real repo, task by task, checking every expectation it
states.

    python run_plan_s9.py replay|execute <plan.md> <repo> [first task] [last task] [first step]

- File steps (Write, Edit, Delete, Copy) are applied exactly as the plan has them (planlib).
- "Build CyberCraft Tweaks", "Run CyberCraft Tweaks' unit tests", "Check the mixins" and "Check that mclink parses":
  the step's bash block runs, its /d/Cyberpunk-Minecraft made the repo's path, and every `text` of its Expected
  paragraph must be in what it printed. A replay first copies the real repo's mclink into the clone: this plan changes
  nothing of BlockRender, so the clone has none built.
- "Check that the game isn't running", "Ask the user", "In Minecraft", "Install" and "Record" steps are done by hand:
  the run stops at the first.
- Commits use the plan's messages. Executing ticks the task's boxes, up to the commit, first.
"""
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import planlib  # noqa: E402

HERE = Path(__file__).parent
REAL = Path("D:/Cyberpunk-Minecraft")

mode = sys.argv[1]
plan_path = Path(sys.argv[2]).resolve()
repo = Path(sys.argv[3]).resolve()
first = int(sys.argv[4]) if len(sys.argv) > 4 else 1
last = int(sys.argv[5]) if len(sys.argv) > 5 else 7
first_step = int(sys.argv[6]) if len(sys.argv) > 6 else 1
log = open(HERE / f"{mode}_s9p1.log", "a", encoding="utf-8")


def say(text):
    print(text, flush=True)
    log.write(text + "\n")
    log.flush()


def run(args, cwd, check=False):
    result = subprocess.run(args, cwd=cwd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    out = result.stdout + result.stderr
    if check and result.returncode != 0:
        raise SystemExit(f"command failed ({result.returncode}): {args}\n{out}")
    return result.returncode, out


def fail(step, why):
    raise SystemExit(f"Task {step.task} step {step.number} ({step.title}): {why}")


BASH_STEPS = ("Build CyberCraft Tweaks", "Run CyberCraft Tweaks' unit tests", "Check the mixins", "Check that mclink parses")
BY_HAND = ("Check that the game isn't running", "Ask the user", "In Minecraft", "Install", "Record")

for step in planlib.read_plan(plan_path):
    if step.task < first or step.task > last or (step.task == first and step.number < first_step):
        continue
    title = step.title
    label = f"Task {step.task} step {step.number}: {title}"
    if planlib.is_file_step(step):
        planlib.apply(step, repo)
        say(label)
    elif title.startswith(BY_HAND):
        say(label + " (by hand: the run stops here)")
        break
    elif title.startswith(BASH_STEPS):
        if title.startswith("Check that mclink parses") and repo != REAL:
            built = repo / "spikes/block-render/build/RelWithDebInfo"
            built.mkdir(parents=True, exist_ok=True)
            shutil.copy2(REAL / "spikes/block-render/build/RelWithDebInfo/mclink.exe", built / "mclink.exe")
        script = re.search(r"```bash\n(.*?)```", step.body, re.S).group(1).replace("/d/Cyberpunk-Minecraft", repo.as_posix())
        code, out = run([shutil.which("bash"), "-c", script], repo)
        expected = re.findall(r"`([^`]+)`", step.body.split("Expected", 1)[1].split("\n\n", 1)[0])
        if not expected or any(text not in out for text in expected):
            fail(step, f"expected {expected}, exit {code}:\n{out[-2000:]}")
        say(f"{label}: {', '.join(expected)}")
    elif title.startswith("Commit"):
        if mode == "execute":
            done = [str(number) for number in range(1, step.number + 1)]
            run([sys.executable, str(HERE / "tick_steps.py"), str(plan_path), str(step.task)] + done, repo, check=True)
        messages = re.findall(r'-m "([^"]+)"', step.body)
        body = step.body.replace("\\\n", " ")  # a command continued over lines
        paths = re.search(r"git add ([^&\n]+)", body).group(1).split()
        run(["git", "add", "--"] + paths, repo, check=True)
        run(["git", "commit", "-q"] + [arg for message in messages for arg in ("-m", message)], repo, check=True)
        _, head = run(["git", "log", "--oneline", "-1"], repo, check=True)
        say(f"{label}: {head.strip()}")
    else:
        fail(step, "this runner doesn't know this kind of step")
say(f"{mode} finished")
