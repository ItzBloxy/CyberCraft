"""Breaks Part 2's new code once per mutation, and checks that the test named for it fails. A build that fails, or a
test run that crashes before its summary line, counts as not caught: a test must fail, not crash.

    uv run --no-project python mutate6.py [first index [last index]]
    uv run --no-project python mutate6.py clean <index>...    (each from a clean build)

It works on the scratch copy (verify2/ beside it), and puts each file back after its mutation.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).parent / "verify2" / "spikes" / "block-render"
CMAKE = r"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

# (file, old, new, the test that must fail)
MUTATIONS = [
    # Task 20: V's reason first
    ("src/input/BuildMode.cpp", "\telse if (!aSituation.blocked.empty())\n\t{\n\t\twhy_ = aSituation.blocked;",
     "\telse if (!aSituation.blocked.empty() && aSituation.inGame)\n\t{\n\t\twhy_ = aSituation.blocked;", "BuildModeSaysWhyVCantBePlacedBeforeVIsntFound"),
    ("src/input/Router.cpp", "\t\t                  : !situation_.blocked.empty() ? situation_.blocked\n", "",
     "RouterSaysWhyVCantBePlacedWhenTheLinkFindsNoV"),
    # Task 21: the eye height while V can be placed
    ("src/link/Drive.cpp", "void EyeHeight::Break()\n{\n\tlastFeetZ_.reset();\n}", "void EyeHeight::Break()\n{\n}",
     "EyeHeightStartsAFreshRunAfterABreak"),
    ("src/link/Link.cpp", "if (aFrame.pose && !build && aFrame.placeable)", "if (aFrame.pose && !build)",
     "LinkMeasuresVsEyeHeightOnlyWhileVCanBePlaced"),
    ("src/link/Link.cpp", "\telse\n\t{\n\t\teyeHeight_.Break();\n\t}\n", "",
     "LinkMeasuresVsEyeHeightOnlyWhileVCanBePlaced"),
    # Task 22: the floor's voxels once a height
    ("src/link/Floor.cpp", "\t\tblock.x += aRegionX * kRegionBlocks;\n", "", "FloorVoxelsAreEachRegionsOwn"),
    ("src/link/Floor.cpp", "\t\tblock.z += aRegionZ * kRegionBlocks;\n", "", "FloorVoxelsAreEachRegionsOwn"),
    # Task 23: regions, batches, filing, the hash and the triangles message
    ("src/link/Ground.cpp", "return aValue >= 0 ? aValue / aBy : -((-aValue + aBy - 1) / aBy);", "return aValue / aBy;",
     "GroundFindsRegionsAndBatches"),
    ("src/link/Ground.cpp", "regions[i++] = { aBatch.x * 2 + x, aBatch.y * 2 + y, aBatch.z * 2 + z };",
     "regions[i++] = { aBatch.x * 2 + z, aBatch.y * 2 + y, aBatch.z * 2 + x };", "GroundFindsRegionsAndBatches"),
    ("src/link/Ground.cpp", "touches = high[k] >= corner[k] - kFileMargin && low[k] <= corner[k] + kRegionBlocks + kFileMargin;",
     "touches = high[k] >= corner[k] && low[k] <= corner[k] + kRegionBlocks;", "GroundFilesATriangleUnderEveryRegionItTouches"),
    ("src/link/Ground.cpp", "\t\tif (!Usable(triangle))\n", "\t\tif (false)\n", "GroundLeavesOutTrianglesThatArentUsable"),
    ("src/link/Ground.cpp", "\t\thash += Fnv1a(triangle.v, sizeof triangle.v);", "\t\thash = hash * 31 + Fnv1a(triangle.v, sizeof triangle.v);",
     "GroundHashesTrianglesWhateverTheirOrder"),
    ("src/link/Voxels.cpp", "\t\tproto::ColTri tri{};\n\t\tstd::memcpy(tri.v, aTriangles[i].v, sizeof tri.v);",
     "\t\tproto::ColTri tri{};\n\t\ttri.flags = 1;\n\t\tstd::memcpy(tri.v, aTriangles[i].v, sizeof tri.v);", "VoxelsTrisPayload"),
    # Task 24: what to read next
    ("src/link/Ground.cpp", "(wanted.nextToCentre && aNow - done->second.at >= kRefresh)",
     "(wanted.nextToCentre && aNow - done->second.at >= 10.0 * kRefresh)", "GroundAreasReadTheBatchesNextToTheCentreEverySecond"),
    ("src/link/Ground.cpp", "if (!centre_ || aNow < readyAt_)", "if (!centre_)", "GroundAreasWaitUntilTheyreReady"),
    ("src/link/Ground.cpp", "readyAt_ = std::max(readyAt_, aReadyAt);", "readyAt_ = aReadyAt;", "GroundAreasWaitUntilTheyreReady"),
    ("src/link/Ground.cpp", "if (first || done->second.hashes[i] != hash)", "if (first)", "GroundAreasSendARegionAgainOnlyWhenItChanges"),
    ("src/link/Ground.cpp", "return InArea(aRegion, aCentre, 1); });", "return InArea(aRegion, aCentre, 100); });",
     "GroundAreasForgetWhatsFarBehind"),
    ("src/link/Ground.cpp", "\tstd::stable_sort(ordered.begin(), ordered.end(), [](const Candidate& aA, const Candidate& aB) { return aA.distance < aB.distance; });\n",
     "", "GroundAreasReadEveryBatchOfTheAreaNearestFirst"),
    ("src/link/Ground.cpp", "return dx * dx + dz * dz + 2 * dy * dy;", "return dx * dx + dz * dz + dy * dy;",
     "GroundAreasReadEveryBatchOfTheAreaNearestFirst"),
    # Task 25: the worker
    ("src/link/GroundWorker.cpp",
     "\t\t\tmessages.push_back({ job.epoch, proto::kColTris, TrisPayload(at.x, at.y, at.z, job.epoch, job.triangles) });\n"
     "\t\t\tmessages.push_back({ job.epoch, proto::kColRegion, RegionPayload(at.x, at.y, at.z, job.epoch, voxels) });",
     "\t\t\tmessages.push_back({ job.epoch, proto::kColRegion, RegionPayload(at.x, at.y, at.z, job.epoch, voxels) });\n"
     "\t\t\tmessages.push_back({ job.epoch, proto::kColTris, TrisPayload(at.x, at.y, at.z, job.epoch, job.triangles) });",
     "GroundWorkerMakesEachRegionsMessages"),
    ("src/link/GroundWorker.cpp", "\t\tif (stop_)\n\t\t{\n\t\t\treturn;\n\t\t}", "\t\tif (stop_ && jobs_.empty())\n\t\t{\n\t\t\treturn;\n\t\t}",
     "GroundWorkerStopsWithJobsLeft"),
    ("src/link/GroundWorker.cpp", "\tinHand_ -= finished_;\n", "", "GroundWorkerMakesEachRegionsMessages"),
    # Task 26: the link sends the city
    ("src/link/Link.cpp", "\t\tcityClear_ = true;  // the city starts over, with a clear, once its collision has had time to stream in\n", "",
     "LinkStartsTheCityOverWhenMinecraftComesBack"),
    ("src/link/Link.cpp",
     "\t\tcityClear_ = true;  // the city starts over, with a clear, once its collision has had time to stream in\n"
     "\t\tareas_.Reset(aFrame.now + GroundAreas::kSettle);",
     "\t\tcityClear_ = true;  // the city starts over, with a clear, once its collision has had time to stream in\n"
     "\t\tareas_.Reset(aFrame.now);", "LinkSendsTheCityOnceMinecraftIsThereAndItsCollisionHasSettled"),
    ("src/link/Link.cpp", "\t\tstatus_.floorSent = false;\n\t\tareas_.Reset(aFrame.now + GroundAreas::kSettle);\n\t}\n\twasInGame_ = inGame;",
     "\t\tstatus_.floorSent = false;\n\t}\n\twasInGame_ = inGame;", "LinkReadsTheCityAgainWhenVIsFoundAgain"),
    ("src/link/Link.cpp", "\t\t\tif (message.epoch == status_.collisionEpoch)\n", "\t\t\tif (true)\n",
     "LinkStartsTheCityOverWhenMinecraftComesBack"),
    ("src/link/Link.cpp", "\t\telse if (!WriteByteRing(ring, proto::kColRingDataBytes, next.type, next.payload))\n\t\t{\n\t\t\tbreak;",
     "\t\telse if (!WriteByteRing(ring, proto::kColRingDataBytes, next.type, next.payload))\n\t\t{\n\t\t\t;",
     "LinkKeepsTheCitysMessagesInOrderUntilTheRingHasRoom"),
    ("src/link/Link.cpp", "\t\tif (next.payload.size() + 8 > proto::kColRingDataBytes / 2)\n", "\t\tif (false)\n",
     "LinkDropsAMessageTooBigForTheRing"),
    ("src/link/Link.cpp", "if (status_.ground != Ground::City || !status_.minecraft || !centred_ || cityClear_)",
     "if (status_.ground != Ground::City || !centred_ || cityClear_)", "LinkSendsTheCityOnceMinecraftIsThereAndItsCollisionHasSettled"),
    ("src/link/Link.cpp", "if (status_.ground != Ground::City || !status_.minecraft || !centred_ || cityClear_)",
     "if (status_.ground != Ground::City || !status_.minecraft || cityClear_)", "LinkReadsTheCityAgainWhenVIsFoundAgain"),
    ("src/link/Link.cpp", "\t\tconst std::optional<Vec3d> centre = driving       ? std::optional<Vec3d>(Vec3d{ mc->x, mc->y, mc->z })",
     "\t\tconst std::optional<Vec3d> centre = false         ? std::optional<Vec3d>(Vec3d{ mc->x, mc->y, mc->z })",
     "LinkCentresTheCityOnMinecraftsPlayerWhileItDrives"),
    ("src/link/Link.cpp", "\t\tif (there && !city)\n\t\t{\n\t\t\tSendFloor(status_.target, true, aSink);",
     "\t\tif (there)\n\t\t{\n\t\t\tSendFloor(status_.target, true, aSink);", "LinkCentresTheCityOnMinecraftsPlayerWhileItDrives"),
    ("src/link/Link.cpp", "\t\tcityClear_ = aSettings.ground == Ground::City;", "\t\tcityClear_ = false;",
     "LinkSwitchesBetweenTheFloorAndTheCity"),
    ("src/link/Link.cpp", "\t\t\tstatus_.lastHold = aFrame.now - since;\n", "", "LinkPlacesVUnderMinecraftsEyeOnceItLetsGo"),
    ("src/link/Link.cpp", "\t\tif (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() >= aBudgetMs)",
     "\t\tif (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() >= aBudgetMs + 1e9)",
     "LinkSendsTheCityOnceMinecraftIsThereAndItsCollisionHasSettled"),
    # Task 27: mclink's dump loader
    ("tools/mclink/DumpObj.cpp", "\t\t\tkeep = words[1].starts_with(aPrefix);", "\t\t\tkeep = true;", "DumpObjKeepsTheObjectsAskedFor"),
    ("tools/mclink/DumpObj.cpp", "return error == std::errc() && end == aWord.data() + aWord.size() && std::isfinite(aOut);",
     "return error == std::errc() && end == aWord.data() + aWord.size();", "DumpObjRefusesWhatItCantRead"),
    ("tools/mclink/McLinkScript.cpp", "if (words.size() != 2 || (words[1] != \"floor\" && words[1] != \"city\"))", "if (words.size() != 2)",
     "ScriptParsesTheGround"),
    # Task 30: the scene watch's record
    ("src/physx/SceneWatch.cpp", "\tif (aInUpdate && !locked)\n", "\tif (aInUpdate)\n", "SceneWatchCountsEachThreadsCalls"),
    ("src/physx/SceneWatch.cpp", "\treturn slots_[kThreads - 1];", "\treturn slots_[0];", "SceneWatchCountsThreadsPastItsTableTogether"),
    ("src/physx/SceneWatch.cpp", "for (uint64_t i = count > kShifts ? count - kShifts : 0; i < count; ++i)",
     "for (uint64_t i = count > kShifts ? 0 : 0; i < count; ++i)", "SceneWatchKeepsTheLatestShifts"),
    ("src/physx/SceneWatch.cpp", "\t{ \"lock\", { SceneCall::LockWrite } },", "\t{ \"lock\", { SceneCall::LockWrite, SceneCall::UnlockWrite } },",
     "SceneWatchSummarizesByThreadAndKind"),
    ("src/physx/SceneWatch.cpp", "\t{ \"addActors\", 12, 0x54a20 },", "\t{ \"addActors\", 11, 0x54a20 },", "SceneWatchHooksTheGamesSlots"),
    # Task 31: the scene watch's request, status and session
    ("src/Requests.cpp", "if (command == \"link\" || command == \"build\" || command == \"scenewatch\")",
     "if (command == \"link\" || command == \"build\")", "RequestsForTheSceneWatch"),
    ("src/Status.cpp", "\tif (!aReport.sceneWatch.empty())\n", "\tif (true)\n", "StatusWritesTheSceneWatch"),
    ("src/physx/Session.cpp", "aWhy = Narrow(audited.name) + \" isn't loaded\";", "aWhy = \"PhysX isn't loaded\";",
     "PhysxSessionSaysWhyWithoutTheGame"),
    # Task 34: the exporter
    ("src/physx/Export.cpp", "aFlags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE) && !aFlags.isSet(PxShapeFlag::eTRIGGER_SHAPE) &&",
     "!aFlags.isSet(PxShapeFlag::eTRIGGER_SHAPE) &&", "ExportKeepsStaticSceneQueryShapes"),
    ("src/physx/Export.cpp", "aFlags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE) && !aFlags.isSet(PxShapeFlag::eTRIGGER_SHAPE) &&",
     "aFlags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE) &&", "ExportKeepsStaticSceneQueryShapes"),
    ("src/physx/Export.cpp", "return aActor == AuditedClass::RigidStatic && aFlags", "return aFlags", "ExportKeepsStaticSceneQueryShapes"),
    ("src/physx/Export.cpp", "\t       aGeometry != PxGeometryType::ePLANE;", "\t       true;", "ExportKeepsStaticSceneQueryShapes"),
    ("src/physx/Export.cpp", "const Vec3d low = WorldFromMc({ aBox.low.x, aBox.low.y, aBox.high.z });",
     "const Vec3d low = WorldFromMc({ aBox.low.x, aBox.low.y, aBox.low.z });", "ExportTurnsPhysxIntoMinecraftWithTheOrigin"),
    ("src/physx/Export.cpp", "return McFromWorld({ aPoint.x + aOrigin.x, aPoint.y + aOrigin.y, aPoint.z + aOrigin.z });",
     "return McFromWorld({ aPoint.x + aOrigin.x, aPoint.y, aPoint.z + aOrigin.z });", "ExportTurnsPhysxIntoMinecraftWithTheOrigin"),
    ("src/physx/Export.cpp", "\t\treport_.why = \"V isn't found\";", "\t\treport_.why = \"no V\";", "ExportReadsNothingWithoutTheGame"),
    # Task 35: the city's request and status
    ("src/Requests.cpp", "return Request{ Request::Kind::Set, \"ground\", words[2] == \"city\" ? 1.0 : 0.0 };",
     "return Request{ Request::Kind::Set, \"ground\", 1.0 };", "RequestsForTheGround"),
    ("src/Status.cpp", "\tif (aLink.ground != \"city\")\n", "\tif (true)\n", "StatusWritesTheGround"),
    # Task 26, added: unlinked, nothing is read
    ("src/link/Link.cpp", "\t\t\tcentred_ = aFrame.linked;", "\t\t\tcentred_ = true;", "LinkSendsTheCityOnceMinecraftIsThereAndItsCollisionHasSettled"),
    # Tasks 25, 30 and 34, added: the tests no mutation broke yet
    ("src/link/GroundWorker.cpp", "\t\tconst Job job = std::move(jobs_.front());\n\t\tjobs_.pop_front();",
     "\t\tconst Job job = std::move(jobs_.back());\n\t\tjobs_.pop_back();", "GroundWorkerKeepsTheOrderOfItsJobs"),
    ("src/physx/SceneWatch.cpp", "\t\tif (id == aThread)\n", "\t\tif (id == aThread || i == 0)\n", "SceneWatchTakesCallsFromManyThreadsAtOnce"),
    ("src/physx/Export.cpp", "\tcase PxGeometryType::eHEIGHTFIELD: return \"heightField\";", "\tcase PxGeometryType::eHEIGHTFIELD: return \"heightfield\";",
     "ExportNamesShapesForTheShapesRequest"),
    # Task 30, added: the write lock followed before the count starts
    ("src/physx/SceneWatch.cpp", "depth.compare_exchange_weak(now, std::max(now + aBy, 0), std::memory_order_relaxed)",
     "depth.compare_exchange_weak(now, now + aBy, std::memory_order_relaxed)", "SceneWatchFollowsTheWriteLockBeforeItCounts"),
    ("src/physx/SceneWatch.cpp", "void SceneRecorder::Note(SceneCall aCall, uint32_t aThread, bool aInUpdate)\n{\n\tif (!started_.load(std::memory_order_relaxed))\n",
     "void SceneRecorder::Note(SceneCall aCall, uint32_t aThread, bool aInUpdate)\n{\n\tif (false)\n", "SceneWatchFollowsTheWriteLockBeforeItCounts"),
    ("src/physx/SceneWatch.cpp", "\tstarted_.store(false);\n\tfor (Slot& slot : slots_)", "\tfor (Slot& slot : slots_)",
     "SceneWatchFollowsTheWriteLockBeforeItCounts"),
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
