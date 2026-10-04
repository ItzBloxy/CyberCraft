"""Breaks Part 3's new code once per mutation, and checks that the test named for it fails. A build that fails, or a
test run that crashes before its summary line, counts as not caught: a test must fail, not crash.

    uv run --no-project python mutate.py [first index [last index]]
    uv run --no-project python mutate.py clean <index>...    (each from a clean build)

It works on the scratch copy (verify3/ beside it), and puts each file back after its mutation.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).parent / "verify3" / "spikes" / "block-render"
CMAKE = r"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

# (file, old, new, the test that must fail)
MUTATIONS = [
    # Task 38: the remembered turn
    ("src/link/Turn.cpp", "\treturn measured ? measured : remembered_;", "\treturn measured;",
     "TurnRatioUsesARememberedRatioUntilItsOwnIsKnown"),
    ("src/link/Turn.cpp", "\treturn remembered_.has_value() && ratios_.size() < kKnown;", "\treturn remembered_.has_value();",
     "TurnRatioUsesARememberedRatioUntilItsOwnIsKnown"),
    ("src/link/Turn.cpp", "!std::isfinite(ratio) || std::abs(ratio) < 1e-4 ||", "!std::isfinite(ratio) ||",
     "TurnFileKeepsTheRatioForTheNextSession"),
    ("src/link/Turn.cpp", "Trim(line.substr(0, equals)) != \"ratio\"", "line.substr(0, equals).find(\"ratio\") == std::string_view::npos",
     "TurnFileKeepsTheRatioForTheNextSession"),
    ("src/Status.cpp", "\treturn aLink.turnRemembered ? std::format(", "\treturn false ? std::format(", "StatusWritesTheDrive"),
    # Task 39: the worker's threads, focus and order
    ("src/link/GroundWorker.cpp", "\t\t\ttriangles_ += job.triangles.size();\n", "", "GroundWorkerMakesEachRegionsMessages"),
    ("src/link/GroundWorker.cpp", "\t\tcycles_ += cycles;\n", "", "GroundWorkerMakesEachRegionsMessages"),
    ("src/link/GroundWorker.cpp", "\t\tif (best == jobs_.end() || distance < nearest)", "\t\tif (best == jobs_.end())",
     "GroundWorkerStartsTheJobNearestItsFocus"),
    ("src/link/GroundWorker.cpp", "\t\t\tif (older != jobs_.end())", "\t\t\tif (false)", "GroundWorkerReplacesAJobNotStarted"),
    ("src/link/GroundWorker.cpp", "\t\tif (std::find(underWay_.begin(), underWay_.end(), job->region) != underWay_.end())",
     "\t\tif (false)", "GroundWorkerKeepsARegionsJobsInOrder"),
    ("src/link/GroundWorker.cpp", "\tfor (unsigned i = 0; i < std::max(aThreads, 1u); ++i)", "\tfor (unsigned i = 0; i < 1u; ++i)",
     "GroundWorkerRunsSeveralThreads"),
    # Every job finished before the thread stops: the destructor waits for them all.
    ("src/link/GroundWorker.cpp",
     "\t\twake_.wait(lock, [&] { return stop_ || (next = Next()) != jobs_.end(); });\n\t\tif (stop_)\n",
     "\t\twake_.wait(lock, [&] { return (next = Next()) != jobs_.end() || stop_; });\n\t\tif (next == jobs_.end())\n",
     "GroundWorkerStopsWithJobsLeft"),
    # Task 40: the link's focus, and the worker's figures
    ("src/link/Link.cpp", "\t\tworker_->Focus(*focus_);\n", "", "LinkGivesTheWorkerTheCitysCentreFirst"),
    ("src/link/Link.cpp", "\tstatus_.workerTriangles = worker_ ? worker_->Triangles() : 0;\n", "",
     "LinkGivesTheWorkerTheCitysCentreFirst"),
    ("src/Status.cpp", "aLink.workerTrianglesPerRegion, aLink.workerRunning * 100.0);", "aLink.workerTrianglesPerRegion, aLink.workerRunning);",
     "StatusWritesTheGround"),
    # Task 41: the queries watched
    ("src/physx/SceneWatch.cpp", "SceneCall::Overlap }, false },", "SceneCall::Overlap } },", "SceneWatchCountsQueries"),
    ("src/physx/SceneWatch.cpp", "\t\t\tothersInUpdate += thread.id != aMainThread ? inUpdate : 0;", "\t\t\tothersInUpdate += inUpdate;",
     "SceneWatchCountsQueries"),
    ("src/physx/SceneWatch.cpp", "\t{ \"raycast\", 94, 0x60890 },", "\t{ \"raycast\", 95, 0x60890 },", "SceneWatchHooksTheGamesSlots"),
    # Task 42: the solids
    ("src/link/Solids.cpp", "\tSolidsMessage solids{ { header.sx, header.sy, header.sz }, header.count };",
     "\tSolidsMessage solids{ { header.sz, header.sy, header.sx }, header.count };", "SolidsParseSkyCraftsMessage"),
    ("src/link/Solids.cpp", "\tif (set != header.count)", "\tif (false)", "SolidsRejectWhatIsntSkyCraftsMessage"),
    ("src/link/Solids.cpp", "\t\tblocks_ -= old->second.count;\n", "", "SolidsKeepEachSectionsLatest"),
    ("src/link/Solids.cpp", "aMessage.count, ++version_ };", "aMessage.count, version_ };", "SolidsKeepEachSectionsLatest"),
    ("src/link/Link.cpp", "\t\tstatus_.atlasSeen = false;  // Minecraft sends its atlas next\n\t\tsolids_.Clear();\n",
     "\t\tstatus_.atlasSeen = false;  // Minecraft sends its atlas next\n", "LinkKeepsMinecraftsSolids"),
    # Task 44: the solids' status line
    ("src/Status.cpp", "aLink.solidBlocks, aLink.solidSections);", "aLink.solidSections, aLink.solidBlocks);",
     "StatusWritesMinecraftsSolids"),
    # Task 45: the bodies' slots, our side: without the game's extra PxShape slot, every later one moves
    ("../../extern/physx-3.4.2/PhysX_3.4/Include/PxShape.h", "\tvirtual\t\tPxU32\t\t\t\t\tgetReferenceCount() const = 0;\n", "",
     "BodyCallsUseTheGamesSlots"),
    # Task 47: the box merge
    ("src/physx/BoxMerge.cpp", "\t\t\t\twhile (y1 < 16 && left.Layer(x, x1, z, z1, y1))", "\t\t\t\twhile (false)",
     "BoxMergeMakesWholeLayersAndSectionsOneBox"),
    ("src/physx/BoxMerge.cpp", "\t\t\t\twhile (z1 < 16 && left.Row(x, x1, y, z1))", "\t\t\t\twhile (false)",
     "BoxMergeGoesAlongXThenZThenY"),
    ("src/physx/BoxMerge.cpp", "\t\t\t\t\t\t\tleft.Take(bx, by, bz);\n", "", "BoxMergeCoversAnyBlocksExactly"),
    # Task 48: the bodies' plan
    ("src/physx/BodyPlan.cpp", "\tfor (int32_t dx = -1; dx <= 1; ++dx)", "\tfor (int32_t dx = -1; dx <= 2; ++dx)",
     "BodyPlanMakesTheSectionsAroundV"),
    ("src/physx/BodyPlan.cpp", "if (!aOn || !solids || Apart(", "if (!solids || Apart(", "BodyPlanRemovesEveryBodyWhileBodiesAreOff"),
    ("src/physx/BodyPlan.cpp", "\tif (!aOn)\n\t{\n\t\treturn changes;\n\t}\n", "", "BodyPlanRemovesEveryBodyWhileBodiesAreOff"),
    ("src/physx/BodyPlan.cpp", "Apart(section, aV) > 2)", "Apart(section, aV) > 1)",
     "BodyPlanKeepsBodiesUntilTheyAreTwoSectionsAway"),
    ("src/physx/BodyPlan.cpp", "\t\telse if (solids->version != entry.version)", "\t\telse if (false)",
     "BodyPlanRebuildsABodyWhenItsSolidsChange"),
    ("src/physx/BodyPlan.cpp", "\tentries_[aSection] = { aVersion, false };", "\tentries_.erase(aSection);",
     "BodyPlanTriesAFailedBodyAgainOnlyWhenItsSolidsChange"),
    ("src/physx/BodyPlan.cpp", "void BodyPlan::Clear()\n{\n\tentries_.clear();\n}", "void BodyPlan::Clear()\n{\n}",
     "BodyPlanStartsOverWhenCleared"),
    ("src/physx/BodyPlan.cpp", "{ (aBox.x1 - aBox.x0) / 2.0, (aBox.z1 - aBox.z0) / 2.0, (aBox.y1 - aBox.y0) / 2.0 }",
     "{ (aBox.x1 - aBox.x0) / 2.0, (aBox.y1 - aBox.y0) / 2.0, (aBox.z1 - aBox.z0) / 2.0 }", "BodyPlanPutsBoxesInTheWorld"),
    ("src/physx/BodyPlan.cpp", "int32_t(std::floor(aMc.x / 16.0))", "int32_t(std::trunc(aMc.x / 16.0))", "BodyPlanPutsBoxesInTheWorld"),
    ("src/physx/BodyPlan.cpp", "aPhysx.z + aOrigin.z - aWorld.z) < 0.01;", "aPhysx.z + aOrigin.z - aWorld.z) < 0.1;",
     "BodyPlanTellsWhetherABodyMovedWithTheOrigin"),
    # Task 49: our bodies left out of the export
    ("src/physx/Export.cpp", "aGeometry != PxGeometryType::ePLANE && !aOurs;", "aGeometry != PxGeometryType::ePLANE;",
     "ExportLeavesOutBlockRendersOwnBodies"),
    # Task 51: the box request and the bodies' line
    ("src/Requests.cpp", " || command == \"scenewatch\" || command == \"box\")", " || command == \"scenewatch\")",
     "RequestsForTheBodies"),
    ("src/Status.cpp", "\t                   aLink.bodiesOn ? \"on\" : \"off\",", "\t                   \"on\",", "StatusWritesTheBodies"),
    ("src/Status.cpp", "aLink.bodiesWhy.empty() ? \"\" : \"; waiting: \" + OneLine(aLink.bodiesWhy));",
     "std::string());", "StatusWritesTheBodies"),
]


def run(args):
    result = subprocess.run(args, cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
    return result.returncode, result.stdout + result.stderr


# Either a first index, or "clean" and the indices to run, each from a clean build: under %TEMP%, MSBuild can miss a
# change (MSB8029).
clean = len(sys.argv) > 1 and sys.argv[1] == "clean"
only = {int(i) for i in sys.argv[2:]} if clean else None
first = 0 if clean else int(sys.argv[1]) if len(sys.argv) > 1 else 0
last = int(sys.argv[2]) if not clean and len(sys.argv) > 2 else len(MUTATIONS) - 1
failed = 0
ran = 0
for index, (name, old, new, test) in enumerate(MUTATIONS):
    if index < first or index > last or (only is not None and index not in only):
        continue
    ran += 1
    path = ROOT / name
    text = path.read_bytes().decode("utf-8")
    if "\r\n" in text:
        # git checks files out with CRLF here: a mutation's line breaks are the file's.
        old, new = old.replace("\n", "\r\n"), new.replace("\n", "\r\n")
    if text.count(old) != 1:
        print(f"{index}: {name}: the old text occurs {text.count(old)} times: {old!r}", flush=True)
        failed += 1
        continue
    path.write_bytes(text.replace(old, new).encode("utf-8"))
    try:
        code, out = run([CMAKE, "--build", "--preset", "release"] + (["--clean-first"] if clean else []) +
                        ["--", "-m", "-v:minimal", "-p:IgnoreWarnIntDirInTempDetected=true"])
        if code != 0:
            errors = sorted({line.strip() for line in out.splitlines() if "error C" in line})
            print(f"{index}: {name}: the build failed: {errors[:2]}", flush=True)
            failed += 1
            continue
        code, out = run([str(ROOT / "build" / "RelWithDebInfo" / "render_tests.exe")])
        failures = re.findall(r"^FAIL (\w+)$", out, re.M)
        if not re.search(r"^\d+ test\(s\), \d+ failure\(s\)$", out, re.M):
            print(f"{index}: {name}: the tests crashed (exit {code:#x}), which a test must not do", flush=True)
            failed += 1
        elif test in failures:
            print(f"{index}: caught by {test}" + (f" (and {len(failures) - 1} more)" if len(failures) > 1 else ""), flush=True)
        else:
            print(f"{index}: {name}: NOT caught by {test}; failures: {failures}", flush=True)
            failed += 1
    finally:
        path.write_bytes(text.encode("utf-8"))
print(f"{ran} mutations, {failed} not caught as expected")
