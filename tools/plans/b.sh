#!/usr/bin/env bash
# Build and test the scratch copy of spikes/block-render. Usage: b.sh [project dir] [configure]
B="${1:-/c/Users/BLOXY/AppData/Local/Temp/claude/D--Cyberpunk-Minecraft/406ecbf4-756f-4341-9425-42e5c95a5693/scratchpad/verify/spikes/block-render}"
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
CTEST="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe"
cd "$B" || exit 1
if [ "$2" = "configure" ] || [ ! -d build ]; then
	"$CMAKE" --preset default -DRED4EXT_SDK_DIR=D:/Cyberpunk-Minecraft/extern/RED4ext.SDK > build-configure.log 2>&1 || { cat build-configure.log; exit 1; }
fi
# Under %TEMP%, MSBuild doesn't track header changes (MSB8029), so "clean" rebuilds everything.
CLEAN=""; [ "$3" = "clean" ] && CLEAN="--clean-first"
"$CMAKE" --build --preset release $CLEAN -- -m -p:IgnoreWarnIntDirInTempDetected=true > build.log 2>&1
status=$?
grep -E "warning|error" build.log | grep -v MSB8029 | sed -E 's/\[[^]]*\]$//' | sort -u | head -60
echo "build exit: $status, warnings: $(grep -E 'warning [A-Z]+[0-9]+' build.log | grep -v MSB8029 | sort -u | wc -l)"
[ $status -eq 0 ] || exit $status
./build/RelWithDebInfo/render_tests.exe | grep -v "^PASS" | tail -40
