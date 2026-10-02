"""Replay the minecraft-link plan in a fresh clone, or execute it in the real repo, task by task, checking every
expectation it states.

    uv run --no-project python run_plan.py replay|execute <plan.md> <repo> [first task] [last task] [first step]

- File steps (Write, Edit, Delete, Copy) are applied exactly as the plan has them (planlib).
- "Check the branch" is skipped.
- "Download the header and the licence": the files already downloaded into the verified clone are copied, and checked
  against the plan's hashes and byte count.
- Builds: a "must fail" build must fail with the plan's message; a passing build must show no errors or warnings, and
  the tests must report the plan's count. Under %TEMP%, MSBuild doesn't track headers (MSB8029), so a replay rebuilds
  from clean after a step changes a header.
- "Run mclink without Minecraft": it must open the mapping, find Minecraft absent, and exit 0.
- "Check the overlay's syntax": `luac -p` must accept the CET overlay.
- "Run the new tool's test; it must fail": pytest must stop on the plan's missing module. "Run the tools' tests; they
  must pass": all of `tools/`'s Python tests, with the plan's count.
- Commits use the plan's messages. Executing ticks the task's boxes first.
"""
import hashlib
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import planlib  # noqa: E402

HERE = Path(__file__).parent
CMAKE = r"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
SDK = "D:/Cyberpunk-Minecraft/extern/RED4ext.SDK"
VERIFIED = HERE / "verify"

mode = sys.argv[1]
plan_path = Path(sys.argv[2]).resolve()
repo = Path(sys.argv[3]).resolve()
first = int(sys.argv[4]) if len(sys.argv) > 4 else 1
last = int(sys.argv[5]) if len(sys.argv) > 5 else 10
first_step = int(sys.argv[6]) if len(sys.argv) > 6 else 1
project = repo / "spikes" / "block-render"
log = open(HERE / f"{mode}.log", "a", encoding="utf-8")


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


headers_changed = False


def build():
    global headers_changed
    run([CMAKE, "--preset", "default", f"-DRED4EXT_SDK_DIR={SDK}"], project, check=True)
    clean = ["--clean-first"] if headers_changed and mode == "replay" else []
    headers_changed = False
    code, out = run([CMAKE, "--build", "--preset", "release"] + clean + ["--", "-m", "-v:minimal",
                     "-p:IgnoreWarnIntDirInTempDetected=true"], project)
    lines = sorted({re.sub(r"\s*\[[^]]*\]$", "", line.strip()) for line in out.splitlines()
                    if re.search(r"error|warning [A-Z]+\d+", line) and "MSB8029" not in line})
    return code, lines


def tests():
    code, out = run([str(project / "build" / "RelWithDebInfo" / "render_tests.exe")], project)
    return out.strip().splitlines()[-1] if out.strip() else ""


def pytest(files):
    args = ["uv", "run", "--no-project", "-q", "--with", "numpy", "--with", "pillow", "--with", "pytest", "pytest", "-q",
            "-p", "no:cacheprovider"] + files
    return run(args, project / "tools")


def fail(step, why):
    raise SystemExit(f"Task {step.task} step {step.number} ({step.title}): {why}")


for step in planlib.read_plan(plan_path):
    if step.task < first or step.task > last or (step.task == first and step.number < first_step):
        continue
    title = step.title
    label = f"Task {step.task} step {step.number}: {title}"
    if planlib.is_file_step(step):
        planlib.apply(step, repo)
        headers_changed = headers_changed or any(path.endswith(".h") for path in step.paths())
        say(label)
    elif title.startswith("Check the branch"):
        say(label + " (skipped)")
    elif title.startswith("Download the header and the licence"):
        target = repo / "extern" / "skycraft"
        target.mkdir(parents=True, exist_ok=True)
        downloads = HERE / "dl"
        for name in ("skycraft_protocol.h", "LICENSE"):
            shutil.copy2(downloads / name, target / name)
        expected = dict((name, digest) for digest, name in re.findall(r"^([0-9a-f]{64}) \*(\S+)$", step.body, re.M))
        for name, digest in expected.items():
            actual = hashlib.sha256((target / name).read_bytes()).hexdigest()
            if actual != digest:
                fail(step, f"{name} hashes to {actual}")
        total = sum((target / name).stat().st_size for name in expected)
        if f"\n{total}\n" not in step.body:
            fail(step, f"{total} bytes in all")
        say(f"{label}: copied, {len(expected)} hashes and {total} bytes match")
    elif "must fail" in title and "uild" in title:
        code, lines = build()
        expected = re.search(r"Expected: `([^`]+)`", step.body).group(1)
        if code == 0 or not any(expected in line for line in lines):
            fail(step, f"expected a failure with {expected!r}, got exit {code}:\n" + "\n".join(lines[:10]))
        say(f"{label}: fails with {expected}, as expected")
    elif title.startswith("Build and run the tests"):
        code, lines = build()
        if code != 0 or lines:
            fail(step, f"exit {code}:\n" + "\n".join(lines[:30]))
        match = re.search(r"`(\d+) test\(s\), 0 failure\(s\)`", step.body)
        if not match:
            fail(step, "no expected test count")
        result = tests()
        if result != f"{match.group(1)} test(s), 0 failure(s)":
            fail(step, f"tests said: {result}")
        say(f"{label}: clean, {result}")
    elif title.startswith("Run the new tool's test; it must fail"):
        files = re.findall(r"(test_\w+\.py)", step.body.split("Expected")[0])
        code, out = pytest(files)
        module = re.search(r"No module named '(\w+)'", step.body).group(1)
        if code == 0 or f"No module named '{module}'" not in out:
            fail(step, f"expected a missing {module}:\n{out[-800:]}")
        say(f"{label}: fails on {module}, as expected")
    elif title.startswith("Run the tools' tests; they must pass"):
        code, out = pytest([])
        count = re.search(r"`(\d+) passed`", step.body).group(1)
        if code != 0 or not re.search(rf"\b{count} passed\b", out):
            fail(step, f"expected {count} passed:\n{out[-1500:]}")
        say(f"{label}: {count} passed")
    elif title.startswith("Check the overlay's syntax"):
        code, out = run(["luac", "-p", str(project / "tools" / "blockrender_hud" / "init.lua")], project)
        if code != 0:
            fail(step, f"luac said:\n{out}")
        say(f"{label}: syntax ok")
    elif title.startswith("Run mclink without Minecraft"):
        script = project / "build" / "selfcheck.txt"
        script.write_text("status\nquit\n", encoding="utf-8")
        code, out = run([str(project / "build" / "RelWithDebInfo" / "mclink.exe"), str(script)], project)
        lines = out.splitlines()
        if code != 0 or len(lines) < 2 or not re.search(r"the mapping: (created|reused) \(process \d+\); waiting for Minecraft", lines[0]) \
                or "Minecraft: absent" not in lines[1]:
            fail(step, f"exit {code}:\n{out[:800]}")
        say(f"{label}: {lines[0].strip()} / {lines[1].strip()[:40]}")
    elif title.startswith("Commit"):
        if mode == "execute":
            run(["uv", "run", "--no-project", "-q", "python", str(HERE / "tick.py"), str(plan_path), str(step.task)], repo, check=True)
        messages = re.findall(r'-m "([^"]+)"', step.body)
        paths = [path for path in re.search(r"git add ([^&\n]+)", step.body).group(1).split() if path != "\\"]
        run(["git", "add", "--"] + paths, repo, check=True)
        run(["git", "commit", "-q"] + [arg for message in messages for arg in ("-m", message)], repo, check=True)
        _, head = run(["git", "log", "--oneline", "-1"], repo, check=True)
        say(f"{label}: {head.strip()}")
    else:
        fail(step, "this runner doesn't know this kind of step")
say(f"{mode} finished")
