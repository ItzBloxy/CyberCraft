"""Breaks Part 3's new code once per mutation, an exact text swap in one file, rebuilds the tests, and checks that the test
named for it fails. The file is put back after each. The clone is under %TEMP%, where MSBuild doesn't track headers, so a
header's mutation names a source to touch too.

    uv run --no-project python mutate.py [numbers of the mutations to run]
"""
import os
import pathlib
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent / "verify" / "spikes" / "block-render"
CMAKE = r"C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"

# (file, old, new, the test that must fail, a source to touch for a header's mutation)
MUTATIONS = [
    # link/Combat: blasts and the health input (Task 38)
    ("src/link/Combat.cpp", "blast.centre = WorldFromMc({ aEvent.a, aEvent.b, aEvent.c });",
     "blast.centre = McFromWorld({ aEvent.a, aEvent.b, aEvent.c });", "CombatTakesMinecraftsBlastsIntoTheGamesAxes", None),
    ("src/link/Combat.cpp", "blast.reach = 2.0 * std::min(double(aEvent.d), kBlastRadiusCap);",
     "blast.reach = std::min(double(aEvent.d), kBlastRadiusCap);", "CombatTakesMinecraftsBlastsIntoTheGamesAxes", None),
    ("src/link/Combat.cpp", "blast.reach = 2.0 * std::min(double(aEvent.d), kBlastRadiusCap);", "blast.reach = 2.0 * double(aEvent.d);",
     "CombatTakesMinecraftsBlastsIntoTheGamesAxes", None),
    ("src/link/Combat.cpp", "!std::isfinite(aEvent.d) || aEvent.d <= 0.0f)", "!std::isfinite(aEvent.d) || aEvent.d < 0.0f)",
     "CombatTakesMinecraftsBlastsIntoTheGamesAxes", None),
    ("src/link/Combat.cpp", "after >= 0.0 && after <= kBlastWindow", "after >= 0.0 && after <= kPushDelay",
     "CombatCountsHitsWithinABlastsWindowAndReach", None),
    ("src/link/Combat.cpp", "after >= 0.0 && after <= kBlastWindow", "after >= -1.0 && after <= kBlastWindow",
     "CombatCountsHitsWithinABlastsWindowAndReach", None),
    ("src/link/Combat.cpp", "&& distance <= blast->reach)", "&& distance < blast->reach)", "CombatCountsHitsWithinABlastsWindowAndReach",
     None),
    ("src/link/Combat.cpp",
     "std::hypot(aFeet.x - blast->centre.x, aFeet.y - blast->centre.y, aFeet.z - blast->centre.z);",
     "std::hypot(aFeet.x - blast->centre.x, aFeet.y - blast->centre.y);", "CombatCountsHitsWithinABlastsWindowAndReach", None),
    ("src/link/Combat.cpp", "for (auto blast = recent_.rbegin(); blast != recent_.rend(); ++blast)",
     "for (auto blast = recent_.begin(); blast != recent_.end(); ++blast)", "CombatCountsHitsWithinABlastsWindowAndReach", None),
    ("src/link/Combat.cpp", "\t\t\t++blast->hits;\n", "", "CombatPushesEachBlastOnceAQuarterSecondAfter", None),
    ("src/link/Combat.cpp", "if (aNow - aBlast.at < kPushDelay)", "if (aNow - aBlast.at < kBlastWindow)",
     "CombatPushesEachBlastOnceAQuarterSecondAfter", None),
    ("src/link/Combat.cpp", "\t\tdue.push_back(aBlast);\n\t\treturn true;", "\t\tdue.push_back(aBlast);\n\t\treturn false;",
     "CombatPushesEachBlastOnceAQuarterSecondAfter", None),
    ("src/link/Combat.cpp", "\tinput.a = 0;\n\tinput.b = std::isfinite", "\tinput.a = 1;\n\tinput.b = std::isfinite",
     "CombatSetsMinecraftsHealthThroughCyberCraftTweaks", None),
    ("src/link/Combat.cpp", "std::clamp(aShare, 0.0, 1.0) * 10000.0", "std::clamp(aShare, 0.0, 1.0) * 1000.0",
     "CombatSetsMinecraftsHealthThroughCyberCraftTweaks", None),
    ("src/link/Combat.cpp", "std::clamp(aShare, 0.0, 1.0) * 10000.0", "aShare * 10000.0", "CombatSetsMinecraftsHealthThroughCyberCraftTweaks",
     None),
    ("src/link/Combat.cpp", "input.c = int32_t(kHurtSetHealth);", "input.c = 0;", "CombatSetsMinecraftsHealthThroughCyberCraftTweaks", None),
    ("src/link/Combat.h", "constexpr double kBlastWindow = 0.15;", "constexpr double kBlastWindow = 0.2;",
     "CombatCountsHitsWithinABlastsWindowAndReach", "tests/TestCombat.cpp"),
    # physx/Export and link/Link: the bush rule, each shape's words, the city started over (Task 39)
    ("src/physx/Export.cpp", "aQuery.word2 == kSightLayer && aQuery.word3 == 0", "(aQuery.word2 & kSightLayer) != 0 && aQuery.word3 == 0",
     "ExportLeavesOutWhatTheBushRuleSays", None),
    ("src/physx/Export.cpp", "return aQuery.word0 == 0 && aQuery.word1 == 0 &&", "return aQuery.word1 == 0 &&",
     "ExportLeavesOutWhatTheBushRuleSays", None),
    ("src/physx/Export.cpp", "return aSimulation.word0 == 0 && aSimulation.word1 == 0 && aSimulation.word2 == 0 && aSimulation.word3 == 0;",
     "return aSimulation.word1 == 0 && aSimulation.word3 == 0;", "ExportLeavesOutWhatTheBushRuleSays", None),
    ("src/physx/Export.h", "LeaveOut                 rule_ = LeaveOut::SightOnly;", "LeaveOut                 rule_ = LeaveOut::Nothing;",
     "ExportLeavesOutWhatTheBushRuleSays", "tests/TestExport.cpp"),
    ("src/physx/Export.cpp", "\t\treturn \"sight-only shapes\";", "\t\treturn \"sight-only\";", "ExportLeavesOutWhatTheBushRuleSays", None),
    ("src/physx/Export.cpp", "aPose.q.rotate(PxVec3(aHalfHeight, 0.0f, 0.0f))", "aPose.q.rotate(PxVec3(0.0f, 0.0f, aHalfHeight))",
     "ExportDescribesEachPrimitiveShape", None),
    ("src/physx/Export.cpp", "2.0f * aHalfExtents.x, 2.0f * aHalfExtents.y,", "aHalfExtents.x, 2.0f * aHalfExtents.y,",
     "ExportDescribesEachPrimitiveShape", None),
    ("src/physx/Export.cpp", "aRadius, aPose.p.x + aOrigin.x, aPose.p.y + aOrigin.y,", "aRadius, aPose.p.x + aOrigin.x, aPose.p.y,",
     "ExportDescribesEachPrimitiveShape", None),
    ("src/link/Link.cpp", "\t\tcityClear_ = true;\n\t\tareas_.Reset(aFrame.now);\n\t\toutbox_.clear();\n\t}\n\tconst bool city",
     "\t\tareas_.Reset(aFrame.now);\n\t\toutbox_.clear();\n\t}\n\tconst bool city", "LinkStartsTheCityOverWhenWhatItKeepsChanges", None),
    ("src/link/Link.cpp", "\t\tcityClear_ = true;\n\t\tareas_.Reset(aFrame.now);\n\t\toutbox_.clear();\n\t}\n\tconst bool city",
     "\t\tcityClear_ = true;\n\t\toutbox_.clear();\n\t}\n\tconst bool city", "LinkStartsTheCityOverWhenWhatItKeepsChanges", None),
    ("src/link/Link.cpp", "if (std::exchange(cityRestart_, false))", "if (cityRestart_)", "LinkStartsTheCityOverWhenWhatItKeepsChanges", None),
    # mclink's health (Task 40)
    ("tools/mclink/McLinkScript.cpp", "action.numbers[0] <= 0.0 || action.numbers[0] > 1.0", "action.numbers[0] < 0.0 || action.numbers[0] > 1.0",
     "ScriptParsesHealth", None),
    ("tools/mclink/McLinkScript.cpp", "action.numbers[0] <= 0.0 || action.numbers[0] > 1.0", "action.numbers[0] <= 0.0 || action.numbers[0] > 2.0",
     "ScriptParsesHealth", None),
    # The explode request and the status (Task 45)
    ("src/Requests.cpp", "*metres < 0.0 || *metres > 100.0", "*metres < 0.0 || *metres > 1000.0", "RequestsForBlasts", None),
    ("src/Requests.cpp", "*radius <= 0.0 || *radius > 16.0", "*radius <= 0.0 || *radius > 17.0", "RequestsForBlasts", None),
    ("src/Requests.cpp", "\t\trequest.radius = *radius;\n", "", "RequestsForBlasts", None),
    ("src/Status.cpp", "return \"blasts=off: Minecraft's blasts are logged, not applied\\n\";", "return \"blasts=off\\n\";", "StatusWritesBlasts",
     None),
    ("src/Status.cpp", "aLink.blasts, aLink.blastPower,", "aLink.blasts, 0.5,", "StatusWritesBlasts", None),
    ("src/Status.cpp", "out += \"; Minecraft's health \" + OneLine(aLink.lastHealth);", "out += \"; health \" + OneLine(aLink.lastHealth);",
     "StatusWritesBlasts", None),
    ("src/Status.cpp", "OneLine(aLink.bushRule), aLink.shapesBush);", "OneLine(aLink.bushRule), aLink.shapesLeft);", "StatusWritesBlasts", None),
    ("src/Status.cpp", "\tout += FormatCombat(aLink);\n\tout += FormatBlasts(aLink);", "\tout += FormatBlasts(aLink);\n\tout += FormatCombat(aLink);",
     "StatusWritesBlasts", None),
    # Status: the forced knockdowns, on by default (step 5b's fix, Task 49)
    ("src/Status.cpp", "aLink.blastKnock ? \"\" : \", the forced knockdowns off\"", "aLink.blastKnock ? \", the forced knockdowns off\" : \"\"",
     "StatusWritesBlasts", None),
    ("src/Status.cpp", "\", the forced knockdowns off\"", "\", forced knockdowns off\"", "StatusWritesBlasts", None),
    ("src/Status.h", "bool        blastKnock = true;", "bool        blastKnock = false;", "StatusWritesBlasts", "tests/TestStatus.cpp"),
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
    crlf = "\r\n" in text
    if crlf:
        text = text.replace("\r\n", "\n")
    if text.count(old) != 1:
        return f"NOT FOUND ONCE ({text.count(old)} times)"
    changed = text.replace(old, new)
    file.write_bytes((changed.replace("\n", "\r\n") if crlf else changed).encode("utf-8"))
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
    only = {int(a) for a in sys.argv[1:]}
    ran = 0
    for i, (path, old, new, test, also) in enumerate(MUTATIONS, 1):
        if only and i not in only:
            continue
        ran += 1
        result = mutate(path, old, new, test, also)
        caught += result.startswith("caught")
        print(f"{i:2}. {path}: {old.strip()[:56]!r} -> {test}: {result}", flush=True)
    print(f"{caught} of {ran} caught")
    print("final build:", build() or "clean")
