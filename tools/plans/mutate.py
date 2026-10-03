"""Breaks Part 3's new code once per mutation, and checks that the test named for it fails. A build that fails, or a
test run that crashes before its summary line, counts as not caught: a test must fail, not crash.

    uv run --no-project python mutate.py [first index]
    uv run --no-project python mutate.py clean <index>...    (each from a clean build)

It works on the scratch copy (verify/ beside it), and puts each file back after its mutation.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).parent / "verify" / "spikes" / "block-render"
CMAKE = r"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

# (file, old, new, the test that must fail)
MUTATIONS = [
    ("src/input/BuildMode.cpp", "const bool press = !aKey.up && !f8Down_;", "const bool press = !aKey.up;", "BuildModeTogglesOnF8"),
    ("src/input/BuildMode.cpp", "MenuKey(aKey, leftShift_ || rightShift_)", "MenuKey(aKey, false)", "BuildModeLeavesOnTheMenuKeys"),
    ("src/input/BuildMode.cpp", "\tif (aKey.extended)\n\t{\n\t\treturn nullptr;\n\t}\n", "", "BuildModeLeavesOnTheMenuKeys"),
    ("src/input/BuildMode.cpp", "aKey.scanCode <= 0x0A", "aKey.scanCode <= 0x0B", "BuildModeTakesOnlyTheKeys1To9"),
    ("src/input/BuildMode.cpp", "if (aDown && holder == Holder::None)", "if (aDown)", "BuildModeSendsAReleaseWhereItsPressWent"),
    ("src/input/BuildMode.cpp", "holder = Holder::Neither;", "holder = Holder::None;", "BuildModeSendsAReleaseWhereItsPressWent"),
    ("src/input/BuildMode.cpp", "\theld_.fill(Holder::None);\n", "", "BuildModeLetsGoWhenTheGameLosesTheFront"),
    ("src/input/BuildMode.cpp", "\theld_.fill(Holder::None);\n\tf8Down_ = false;\n", "\theld_.fill(Holder::None);\n",
     "BuildModeLetsGoWhenTheGameLosesTheFront"),
    ("src/input/BuildMode.cpp", "\t\treturn decision;  // not the user at the game: nothing goes to Minecraft\n", "",
     "BuildModeForwardsNothingWhileTheGameIsntInFront"),
    ("src/input/BuildMode.cpp", "\tif (!aSituation.inFront)\n\t{\n\t\treturn decision;\n\t}\n\tfor", "\tfor",
     "BuildModeForwardsNothingWhileTheGameIsntInFront"),
    ("src/input/BuildMode.cpp", "(aMouse.buttons & RI_MOUSE_WHEEL) && on_", "(aMouse.buttons & RI_MOUSE_WHEEL)", "BuildModeStartsOffAndPassesEverything"),
    ("src/input/BuildMode.cpp", "*slot_ - 1 - Sign(whole) + 9", "*slot_ - 1 + Sign(whole) + 9", "BuildModeForwardsTheWheelAndFollowsTheSlot"),
    ("src/input/BuildMode.cpp", "\t\tscroll_ = 0.0;\n\t}\n\tscroll_ += notches;", "\t}\n\tscroll_ += notches;",
     "BuildModeForwardsTheWheelAndFollowsTheSlot"),
    ("src/input/BuildMode.cpp", "*slot_ - 1 - Sign(whole) + 9", "*slot_ - 1 - int(whole) + 9", "BuildModeForwardsTheWheelAndFollowsTheSlot"),
    ("src/input/BuildMode.cpp", "decision.press = decision.press || down;", "decision.press = true;", "BuildModeForwardsAndHidesTheButtons"),
    ("src/input/BuildMode.cpp", "RI_MOUSE_RIGHT_BUTTON_UP, 3,", "RI_MOUSE_RIGHT_BUTTON_UP, 2,", "BuildModeForwardsAndHidesTheButtons"),
    ("src/input/BuildMode.cpp", "\t\tdecision.hideKey = true;\n\t\t++hidden_;\n\t\tconst bool press", "\t\tdecision.hideKey = true;\n\t\tconst bool press",
     "BuildModeCountsWhatItForwardsAndHides"),
    ("src/input/BuildMode.cpp", "else if (!aSituation.linked)", "else if (false)", "BuildModeF8LinksFirst"),
    ("src/input/BuildMode.cpp", "if (!aSituation.minecraftInWorld)", "if (false)", "BuildModeGoesOnOnlyWithMinecraftInItsWorld"),
    ("src/input/BuildMode.cpp", "\tif (!on_)\n\t{\n\t\treturn false;\n\t}\n\ton_ = false;", "\ton_ = false;", "BuildModeCountsWhatItForwardsAndHides"),
    ("src/input/BuildMode.cpp", "decision.releaseAll = TurnOff(std::format(\"{} left it\", menu));", "TurnOff(std::format(\"{} left it\", menu));",
     "BuildModeLeavesOnTheMenuKeys"),
    ("src/input/BuildMode.cpp", "\t\t\tdecision.releaseAll = TurnOff(\"F8 turned it off\");", "\t\t\tTurnOff(\"F8 turned it off\");", "BuildModeTogglesOnF8"),
    ("src/input/BuildMode.cpp", "\t\t\tif (!aKey.up)\n\t\t\t{\n\t\t\t\tslot_ = digit;", "\t\t\tif (aKey.up)\n\t\t\t{\n\t\t\t\tslot_ = digit;",
     "BuildModeForwardsTheWheelAndFollowsTheSlot"),
    ("src/input/Router.cpp", "mouse.usButtonFlags = USHORT(mouse.usButtonFlags & ~decision.hideButtons);", "mouse.usButtonFlags = 0;",
     "RouterHidesAButtonAndKeepsItsMovement"),
    ("src/input/Router.cpp", "\t\t\tkey.MakeCode = 0;\n", "", "RouterMakesAKeyANoOp"),
    ("src/input/Router.cpp", "\t\t\tkey.VKey = 0xFF;\n", "", "RouterMakesAKeyANoOp"),
    ("src/input/Router.cpp", "aBytes >= sizeof header + sizeof(RAWMOUSE)", "aBytes >= sizeof header", "RouterLeavesEverythingElseAlone"),
    ("src/input/Router.cpp", "aBytes >= sizeof header + sizeof(RAWKEYBOARD)", "aBytes >= sizeof header", "RouterLeavesEverythingElseAlone"),
    ("src/input/Router.cpp", "\t\tif (why)\n\t\t{\n\t\t\tOff(why);", "\t\tif (false)\n\t\t{\n\t\t\tOff(why);", "RouterEndsBuildModeWhenMinecraftOrVGoes"),
    ("src/input/Router.cpp", "\tPush({ proto::kInReleaseAll, 0, 0, 0, 0 });\n\tlog_.Push(\"build mode off: \" + mode_.Why());",
     "\tlog_.Push(\"build mode off: \" + mode_.Why());", "RouterEndsBuildModeWhenMinecraftOrVGoes"),
    ("src/input/Router.cpp", "\t\tPush({ proto::kInReleaseAll, 0, 0, 0, 0 });\n\t\tlog_.Push(\"the game lost", "\t\tlog_.Push(\"the game lost",
     "RouterLetsGoWhenTheGameLosesTheFront"),
    ("src/input/Router.cpp", "\tlink_ = aLink;", "\tif (aLink)\n\t{\n\t\tlink_ = aLink;\n\t}", "RouterPushesNothingOnceTheLinkIsGone"),
    ("src/input/Router.cpp", "return std::exchange(press_, std::nullopt);", "return press_;", "RouterHidesAButtonAndKeepsItsMovement"),
    ("src/input/Router.cpp", "return std::exchange(linkRequested_, false);", "return linkRequested_;", "RouterTakesF8sLinkRequest"),
    ("src/input/Router.cpp", "\t\t++read_;\n\t\tRAWMOUSE mouse;", "\t\tRAWMOUSE mouse;", "RouterLeavesEverythingElseAlone"),
    ("src/input/Router.cpp", "\tif (aDecision.releaseAll)\n\t{", "\tif (false)\n\t{", "RouterReportsBuildMode"),
    ("src/input/Router.cpp", "\t\tlinkRequested_ = true;\n", "", "RouterTakesF8sLinkRequest"),
    ("src/input/Router.cpp", "\t\tpress_ = SteadySeconds();\n", "", "RouterHidesAButtonAndKeepsItsMovement"),
    ("src/input/Router.cpp", "\tsituation_.minecraftInWorld = aState.inWorld;\n", "", "RouterTakesF8sLinkRequest"),
    ("src/input/Router.cpp", "\t\tlog_.Push(mode_.On() ? \"build mode on\" : \"build mode off: \" + mode_.Why());",
     "\t\tlog_.Push(\"build mode changed\");", "RouterPassesEverythingOutsideBuildMode"),
    ("src/input/BuildMode.cpp", "\t\t\t\t\tslot_.reset();", "\t\t\t\t\t;", "BuildModeForwardsTheSideButtons"),
    ("src/input/BuildMode.cpp", "if (i < kSideButtons)", "if (true)", "BuildModeForwardsTheSideButtons"),
    ("src/input/BuildMode.cpp", "{ RI_MOUSE_BUTTON_4_DOWN, RI_MOUSE_BUTTON_4_UP, 4,", "{ RI_MOUSE_BUTTON_4_DOWN, RI_MOUSE_BUTTON_4_UP, 5,",
     "BuildModeForwardsTheSideButtons"),
    ("src/input/Router.cpp", "RI_MOUSE_BUTTON_5_DOWN | RI_MOUSE_BUTTON_5_UP | RI_MOUSE_WHEEL", "RI_MOUSE_BUTTON_5_UP | RI_MOUSE_WHEEL",
     "RouterHidesAButtonAndKeepsItsMovement"),
    ("src/Requests.cpp", "command == \"link\" ? Request::Kind::Link : Request::Kind::Build", "Request::Kind::Link", "RequestsForBuildMode"),
    ("src/input/Scancodes.cpp", "(aExtended ? 0x80u : 0u)", "0u", "ScancodesTellExtendedKeysApart"),
    ("src/input/Scancodes.cpp", "aScanCode < 0x80 ?", "aScanCode < 0x100 ?", "ScancodesTellExtendedKeysApart"),
    ("src/input/FocusWatch.cpp", "const bool lost = inFront_ && !aInFront;", "const bool lost = !aInFront;", "FocusWatchLetsGoOnceOnLosingTheFront"),
    ("src/Status.cpp", "aLink.slot ? std::format(\"slot {}\", *aLink.slot)", "aLink.slot ? std::format(\"slot {}\", *aLink.slot + 1)",
     "StatusWritesBuildMode"),
]


def run(args):
    result = subprocess.run(args, cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
    return result.returncode, result.stdout + result.stderr


# Either a first index, or "clean" and the indices to run, each from a clean build: under %TEMP%, MSBuild can miss a
# change (MSB8029).
clean = len(sys.argv) > 1 and sys.argv[1] == "clean"
only = {int(i) for i in sys.argv[2:]} if clean else None
first = 0 if clean else int(sys.argv[1]) if len(sys.argv) > 1 else 0
failed = 0
for index, (name, old, new, test) in enumerate(MUTATIONS):
    if index < first or (only is not None and index not in only):
        continue
    path = ROOT / name
    text = path.read_bytes().decode("utf-8")
    if text.count(old) != 1:
        print(f"{index}: {name}: the old text occurs {text.count(old)} times: {old!r}")
        failed += 1
        continue
    path.write_bytes(text.replace(old, new).encode("utf-8"))
    try:
        code, out = run([CMAKE, "--build", "--preset", "release"] + (["--clean-first"] if clean else []) +
                        ["--", "-m", "-v:minimal", "-p:IgnoreWarnIntDirInTempDetected=true"])
        if code != 0:
            errors = sorted({line.strip() for line in out.splitlines() if "error C" in line})
            print(f"{index}: {name}: the build failed: {errors[:2]}")
            failed += 1
            continue
        code, out = run([str(ROOT / "build" / "RelWithDebInfo" / "render_tests.exe")])
        failures = re.findall(r"^FAIL (\w+)$", out, re.M)
        if not re.search(r"^\d+ test\(s\), \d+ failure\(s\)$", out, re.M):
            print(f"{index}: {name}: the tests crashed (exit {code:#x}), which a test must not do")
            failed += 1
        elif test in failures:
            print(f"{index}: caught by {test}" + (f" (and {len(failures) - 1} more)" if len(failures) > 1 else ""))
        else:
            print(f"{index}: {name}: NOT caught by {test}; failures: {failures}")
            failed += 1
    finally:
        path.write_bytes(text.encode("utf-8"))
print(f"{len(MUTATIONS) - first} mutations, {failed} not caught as expected")
