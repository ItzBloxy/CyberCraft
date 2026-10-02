"""Execute the block-render plan in the real repo, task by task, checking every expectation it states (replay.py,
plus ticking each task's checkboxes in the plan before its commit).

    uv run --no-project python replay.py <plan.md> <clone> [first task] [last task]

- File steps (Write, Edit, Delete, Copy) are applied exactly as the plan has them (planlib).
- Builds: a "must fail" build must fail with the plan's C1083 message; a passing build must show no errors or
  warnings, and the tests must report the plan's count.
- Python: a "must fail" run must fail to import the module; a passing run must report the plan's count.
- Commits use the plan's message. Task 1's download is replaced by a copy of the already-downloaded files, checked
  against the plan's hashes. Tasks 28 on (the game) are skipped.
"""
import hashlib
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import planlib  # noqa: E402

HERE = Path(__file__).parent
CMAKE = r"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
DUMPBIN = r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\dumpbin.exe"
SDK = "D:/Cyberpunk-Minecraft/extern/RED4ext.SDK"
VERIFIED = HERE / "verify"

plan_path = Path(sys.argv[1]).resolve()
clone = Path(sys.argv[2]).resolve()
first = int(sys.argv[3]) if len(sys.argv) > 3 else 1
last = int(sys.argv[4]) if len(sys.argv) > 4 else 27
first_step = int(sys.argv[5]) if len(sys.argv) > 5 else 1  # resume inside the first task
project = clone / "spikes" / "block-render"
log = open(HERE / "execute.log", "a", encoding="utf-8")


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


def build():
    run([CMAKE, "--preset", "default", f"-DRED4EXT_SDK_DIR={SDK}"], project, check=True)
    code, out = run([CMAKE, "--build", "--preset", "release", "--", "-m", "-v:minimal",
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


def expected_count(step, pattern):
    match = re.search(pattern, step.body)
    if not match:
        fail(step, f"no expected count matching {pattern}")
    return int(match.group(1))


for step in planlib.read_plan(plan_path):
    if step.task < first or step.task > last or (step.task == first and step.number < first_step):
        continue
    title = step.title
    label = f"Task {step.task} step {step.number}: {title}"
    if planlib.is_file_step(step):
        planlib.apply(step, clone)
        say(label)
    elif title.startswith("Check the branch"):
        say(label + " (skipped in the replay)")
    elif title.startswith("Download the headers"):
        shutil.copytree(VERIFIED / "extern" / "streamline-2.7.2" / "include", clone / "extern" / "streamline-2.7.2" / "include")
        shutil.copy2(VERIFIED / "extern" / "streamline-2.7.2" / "license.txt", clone / "extern" / "streamline-2.7.2")
        expected = dict((name, digest) for digest, name in re.findall(r"^([0-9a-f]{64}) \*(\S+)$", step.body, re.M))
        root = clone / "extern" / "streamline-2.7.2"
        for name, digest in expected.items():
            actual = hashlib.sha256((root / name).read_bytes()).hexdigest()
            if actual != digest:
                fail(step, f"{name} hashes to {actual}")
        total = sum((root / name).stat().st_size for name in expected)
        if f"\n{total}\n" not in step.body:
            fail(step, f"{total} bytes in all")
        say(f"{label}: copied, {len(expected)} hashes and {total} bytes match")
    elif "must fail" in title and "uild" in title:
        code, lines = build()
        expected = re.search(r"Expected: `([^`]+)`", step.body).group(1)
        if code == 0 or not any(expected in line for line in lines):
            fail(step, f"expected a failure with {expected!r}, got exit {code}:\n" + "\n".join(lines[:10]))
        say(f"{label}: fails with {expected}, as expected")
    elif title.startswith(("Build and run the tests", "Build; it must compile")):
        code, lines = build()
        if code != 0 or lines:
            fail(step, f"exit {code}:\n" + "\n".join(lines[:30]))
        count = expected_count(step, r"`(\d+) test\(s\), 0 failure\(s\)`")
        result = tests()
        if result != f"{count} test(s), 0 failure(s)":
            fail(step, f"tests said: {result}")
        say(f"{label}: clean, {result}")
    elif title.startswith(("Run it; it must fail", "Run both; the summary's must fail")):
        files = re.findall(r"(test_\w+\.py)", step.body.split("Expected")[0])
        code, out = pytest(files)
        module = re.search(r"No module named '(\w+)'", step.body).group(1)
        if code == 0 or f"No module named '{module}'" not in out:
            fail(step, f"expected a missing {module}:\n{out[-800:]}")
        say(f"{label}: fails on {module}, as expected; tail: {out.strip().splitlines()[-1]}")
    elif title.startswith("Run it; the new test must fail"):
        files = re.findall(r"(test_\w+\.py)", step.body.split("Expected")[0])
        code, out = pytest(files)
        expected = re.findall(r"`([^`]+)`", step.body.split("Expected", 1)[1].split("\n", 1)[0])
        if code == 0 or not expected or any(text not in out for text in expected):
            fail(step, f"expected a failure showing {expected}:\n{out[-1500:]}")
        say(f"{label}: fails with {expected}, as expected")
    elif title.startswith("Convert Task 32's dump"):
        folder = re.search(r"tools/view_dump\.py (\S+)", step.body).group(1)
        folder = re.sub(r"^/([a-z])/", lambda m: m.group(1).upper() + ":/", folder)
        code, out = run(["uv", "run", "--no-project", "-q", "--with", "numpy", "--with", "pillow", "python",
                         "tools/view_dump.py", folder], project)
        count = expected_count(step, r"Expected: `(\d+)`")
        converted = sum(1 for line in out.splitlines() if ".dds: " in line)
        if code != 0 or converted != count:
            fail(step, f"exit {code}, {converted} files converted:\n{out[-1500:]}")
        say(f"{label}: {converted} files converted")
    elif title.startswith(("Run the tests; they must pass", "Run all the tools' tests")):
        files = re.findall(r"(test_\w+\.py)", step.body.split("Expected")[0])
        code, out = pytest(files)
        count = expected_count(step, r"`(\d+) passed`")
        if code != 0 or not re.search(rf"\b{count} passed\b", out):
            fail(step, f"expected {count} passed:\n{out[-1500:]}")
        say(f"{label}: {count} passed")
    elif title.startswith("Try it on the user's jar"):
        out_dir = Path(tempfile.mkdtemp())
        jar = Path.home() / "AppData" / "Roaming" / ".minecraft" / "versions" / "26.3" / "26.3.jar"
        code, out = run(["uv", "run", "--no-project", "-q", "--with", "pillow", "python", "tools/build_atlas.py", str(jar),
                         str(out_dir)], project)
        size = (out_dir / "atlas.bin").stat().st_size if (out_dir / "atlas.bin").exists() else -1
        if code != 0 or "atlas: 256 x 64, 62 sprites, 50 blocks, written to" not in out or size != 91520:
            fail(step, f"exit {code}, atlas.bin {size} bytes:\n{out}")
        say(f"{label}: {out.strip()} ({size} bytes)")
        shutil.rmtree(out_dir)
    elif title.startswith("Check the DLL's exports"):
        dll = project / "build" / "RelWithDebInfo" / "BlockRender.dll"
        _, exports = run([DUMPBIN, "-nologo", "-exports", str(dll)], project, check=True)
        _, imports = run([DUMPBIN, "-nologo", "-dependents", str(dll)], project, check=True)
        names = re.findall(r" (\w+) = ", exports)
        dlls = [line.strip() for line in imports.splitlines() if re.search(r"d3d|dxgi|sl\.", line, re.I)]
        if names != ["Main", "Query", "Supports"] or set(dlls) != {"D3DCOMPILER_47.dll", "d3d12.dll"}:
            fail(step, f"exports {names}, imports {dlls}")
        say(f"{label}: exports {names}, imports {dlls}")
    elif title.startswith("Check the overlay's Lua"):
        code, out = run(["luac", "-p", str(project / "tools" / "blockrender_hud" / "init.lua")], project)
        if code != 0:
            fail(step, out)
        say(f"{label}: syntax ok")
    elif title.startswith("Commit"):
        run(["uv", "run", "--no-project", "-q", "python", str(HERE / "tick.py"), str(plan_path), str(step.task)], clone, check=True)
        messages = re.findall(r'-m "([^"]+)"', step.body)
        run(["git", "add", "-A", "."], clone, check=True)
        run(["git", "commit", "-q"] + [arg for message in messages for arg in ("-m", message)], clone, check=True)
        _, head = run(["git", "log", "--oneline", "-1"], clone, check=True)
        say(f"{label}: {head.strip()}")
    else:
        fail(step, "the replay doesn't know this kind of step")
say("replay finished")
