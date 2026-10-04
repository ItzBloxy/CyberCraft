#!/usr/bin/env bash
# Build and test the scratch copy of spikes/block-render. Usage: b.sh [project dir] [configure]
S="$(cd "$(dirname "$0")" && pwd)/verify2"
B="${1:-C:/Users/BLOXY/AppData/Local/Temp/claude/D--Cyberpunk-Minecraft/e6822ed5-3da9-434f-9fd7-fdc2818e6d27/scratchpad/plans/verify2/spikes/block-render}"
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
CTEST="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe"
cd "$B" || exit 1
if [ "$2" = "configure" ] || [ ! -d build ]; then
	"$CMAKE" --preset default -DRED4EXT_SDK_DIR=D:/Cyberpunk-Minecraft/extern/RED4ext.SDK > "$S/../b6-configure.log" 2>&1 || { cat "$S/../b6-configure.log"; exit 1; }
fi
# Under %TEMP%, MSBuild doesn't track header changes (MSB8029), so "clean" rebuilds everything.
CLEAN=""; [ "$3" = "clean" ] && CLEAN="--clean-first"
"$CMAKE" --build --preset release $CLEAN -- -m -p:IgnoreWarnIntDirInTempDetected=true > "$S/../b6-build.log" 2>&1
status=$?
grep -E "warning|error" "$S/../b6-build.log" | grep -v MSB8029 | sed -E 's/\[[^]]*\]$//' | sort -u | head -60
echo "build exit: $status, warnings: $(grep -E "warning [A-Z]+[0-9]+" "$S/../b6-build.log" | grep -v MSB8029 | sort -u | wc -l)"
[ $status -eq 0 ] || exit $status
./build/RelWithDebInfo/render_tests.exe | grep -v "^PASS" | tail -40
