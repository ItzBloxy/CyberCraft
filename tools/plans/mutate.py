"""Breaks step 4's fixes' new code once per mutation, an exact text swap in one file, rebuilds the tests, and checks that
the test named for it fails. The file is put back after each. The clone is under %TEMP%, where MSBuild doesn't track
headers, so a header's mutation names a source to touch too.

    uv run --no-project python mutate_fix.py
"""
import os
import pathlib
import subprocess
import time

ROOT = pathlib.Path(r"C:/Users/BLOXY/AppData/Local/Temp/claude/D--Cyberpunk-Minecraft/4a6539a8-db56-4530-9f07-df4565c9cfab"
                    r"/scratchpad/verify/spikes/block-render")
CMAKE = r"C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"

# (file, old, new, the test that must fail, a source to touch for a header's mutation)
MUTATIONS = [
    ("src/link/Combat.cpp", "hit.amount = aPunch * std::pow(hit.damage, aPower);", "hit.amount = aPunch * hit.damage;",
     "CombatStartsHitsAtVsPunchTimesDamageToTheHitPower", None),
    ("src/link/Combat.cpp", "hit.amount = aPunch * std::pow(hit.damage, aPower);", "hit.amount = aPunch * std::sqrt(hit.damage);",
     "CombatStartsHitsAtVsPunchTimesDamageToTheHitPower", None),
    ("src/link/Combat.cpp", "hit.amount = aPunch * std::pow(hit.damage, aPower);", "hit.amount = aPunch * std::pow(damage, aPower);",
     "CombatCountsHitsOnListedNpcsWithDamage", None),
    ("src/link/Combat.h", "constexpr double kHitPower = 0.5;", "constexpr double kHitPower = 1.0;",
     "CombatStartsHitsAtVsPunchTimesDamageToTheHitPower", "tests/TestCombat.cpp"),
    ("src/Status.cpp", "V's punch {:.2f}{}{}; hit power {:.2f}; hits", "V's punch {:.2f}{}{}; hit power {:.1f}; hits", "StatusWritesCombat",
     None),
    ("src/Status.cpp", "aLink.hitPower, aLink.hitsQueued, aLink.hitsLanded,", "0.5, aLink.hitsQueued, aLink.hitsLanded,", "StatusWritesCombat",
     None),
    ("src/Status.cpp", "aLink.vMeshes, aLink.vEffects,", "aLink.vMeshes, 0,", "StatusWritesWhetherVIsHidden", None),
    ("src/Status.cpp", "aLink.vCrosshair > 0   ?", "aLink.vCrosshair >= 0  ?", "StatusWritesWhetherVIsHidden", None),
    ("src/Status.cpp", '", the crosshair not found"', '", the crosshair hidden"', "StatusWritesWhetherVIsHidden", None),
]


def build() -> str:
    run = subprocess.run([CMAKE, "--build", "--preset", "release", "--target", "render_tests", "--", "-m", "-v:minimal",
                          "-p:IgnoreWarnIntDirInTempDetected=true"], cwd=ROOT, capture_output=True, text=True)
    errors = [line for line in run.stdout.splitlines() if " error C" in line or " error LNK" in line]
    return errors[0] if errors else ""


def touch(path: str) -> None:
    now = time.time() + 2
    os.utime(ROOT / path, (now, now))


def mutate(path: str, old: str, new: str, test: str, also: str | None) -> str:
    file = ROOT / path
    original = file.read_bytes()
    text = original.decode("utf-8")
    if text.count(old) != 1:
        return f"NOT FOUND ONCE ({text.count(old)} times)"
    file.write_bytes(text.replace(old, new).encode("utf-8"))
    if also:
        touch(also)
    try:
        error = build()
        if error:
            return "build failed: " + error.strip()[:120]
        run = subprocess.run([str(ROOT / "build/RelWithDebInfo/render_tests.exe")], cwd=ROOT, capture_output=True, text=True, timeout=300)
        if f"FAIL {test}\n" in run.stdout:
            return "caught"
        if "failure(s)" not in run.stdout:
            return f"crashed (exit {run.returncode})"
        return "NOT CAUGHT"
    finally:
        file.write_bytes(original)
        if also:
            touch(also)


if __name__ == "__main__":
    caught = 0
    import sys
    only = {int(a) for a in sys.argv[1:]}
    for i, (path, old, new, test, also) in enumerate(MUTATIONS, 1):
        if only and i not in only:
            continue
        result = mutate(path, old, new, test, also)
        caught += result.startswith("caught")
        print(f"{i:2}. {path}: {old.strip()[:56]!r} -> {test}: {result}", flush=True)
    print(f"{caught} of {len(MUTATIONS)} caught")
    print("final build:", build() or "clean")
