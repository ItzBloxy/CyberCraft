# Block Render Implementation Plan, Parts 1 and 2 (steps 1 to 3)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> In this project, plans run inline with superpowers:executing-plans, never through sub-agents.

**Goal:** Build BlockRender, a RED4ext plugin that maps Cyberpunk 2077's frame through its Streamline calls and draws
Minecraft blocks on the finished frame. Then run spike 3's first two steps in the game with the user: the frame log
(step 1) and a cube on the finished frame (step 2). Part 2 then draws the blocks into the frame before DLSS (step 3).

**Architecture:** A C++20 RED4ext plugin, built like spike 1's.
- **`render_core`** holds everything that can be tested without the game, with unit tests:
  - request parsing, and Streamline's struct offsets;
  - the patchers, the view registry and the state tracker;
  - the camera maths, the atlas, the block world and the mesher;
  - the DDS and JSON writers, the shaders and the status file.
- **`render_runtime`** holds what only the running game can test:
  - the Streamline import hooks and the D3D12 vtable hooks;
  - GpuApi reads;
  - the frame log, the dumps and the renderer.
- **The plugin** (`BlockRender.dll`) adds the entry points, the request file, the status file and finding V.
- **Python tools** build the block atlas from the user's Minecraft jar, and turn the dumps and the frame log into
  something to read.
- **A CET overlay** shows the status in the game.

**Tech stack:**
- C++20, MSVC 14.44 (Visual Studio 2022), and CMake 3.31 (bundled with Visual Studio).
- RED4ext.SDK (header-only) and NVIDIA Streamline 2.7.2's headers.
- D3D12 and D3DCompiler, from the Windows SDK.
- Python 3.14 with uv (numpy, Pillow and pytest), and Lua for CET.

**Spec:** [docs/superpowers/specs/2026-10-01-block-render-design.md](../specs/2026-10-01-block-render-design.md)

**Parts 2 and 3:** Part 2 is step 3 (blocks before DLSS): Tasks 35 to 48, which Task 34 planned from step 1's
findings. Part 3 is step 4 (blocks in the G-buffer), which Task 48 plans once step 3 has run in the game.

---

## Before you start

- **The branch:** work on `spike/block-rendering`, which holds the spec and the handoff.
- **The shell:** commands are for Git Bash.
  - These tools aren't on PATH, and shell variables don't persist between tool calls, so every command block sets
    the ones it needs:
    ```bash
    CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
    DUMPBIN="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/dumpbin.exe"
    ```
  - Pass MSBuild switches as `-m`, never `/m`: Git Bash rewrites `/m` into a file path.
- **Already verified:** all the C++, HLSL, Lua and Python in this plan has been compiled and tested once already, in a
  scratch clone. Tasks 1 to 28, 31 and 33 were then replayed in a fresh clone by a script that applied every file
  step exactly as written, and checked every build and test step's expected result. The results:
  - zero compiler warnings at `/W4`;
  - 85 C++ tests and 21 Python tests passing;
  - the plugin DLL exporting exactly `Main`, `Query` and `Supports`;
  - the replayed files identical to the scratch clone's.

  So a failure while following the plan most likely means a transcription slip: compare against the plan's code first.
- **Test counts:** the C++ test count grows with each task: 7, 11, 16, 19, 25, 28, 31, 36, 41, 48, 51, 56, 64, 69, 72, 75,
  79, 80, then 83 from Task 20 on, 84 from Task 28 on, and 85 from Task 31 on.
- **What only the game can test:** the code that runs inside the game, which is the hooks, GpuApi, the frame log, the
  dumps, the renderer and the plugin (Tasks 23 to 27). Tasks 3 to 20 test, offline, the pieces it relies on. Tasks 29
  and 30 test the rest in the game, with the user, and Task 32 finishes. Tasks 28, 31 and 33 fix what the runs
  in the game found.
- **The game and the user:**
  - **The user does everything in the game:** launching it, loading saves, moving, pressing keys and taking
    screenshots. Never drive their mouse or keyboard: ask, then wait for them to say they're done.
  - **Deploying:** never build with `CP2077_DIR` set, and never deploy, while the game runs.
    `tasklist | grep -i -c "cyberpunk2077.exe"` must print `0`. Set `CP2077_DIR` only to deploy, and clear it
    straight after.
  - **No faults:** plugin code must never fault, because the game's crash handler turns any fault into a crash or a
    frozen game. If the game freezes, first read BlockRender's log and the newest folder in
    `%LOCALAPPDATA%\REDEngine\ReportQueue\`. Force-close the game only when the user asks.
- **Commits** go on `spike/block-rendering`, and end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## File map

All paths under `src/`, `tests/`, `shaders/` and `tools/` are relative to `spikes/block-render/`.

| File | Responsibility |
|---|---|
| `extern/streamline-2.7.2/` | NVIDIA Streamline 2.7.2's public headers and MIT licence, vendored unchanged |
| `CMakeLists.txt`, `CMakePresets.json` | The build: `render_core`, `render_runtime`, `BlockRender`, `render_tests`, and the deploy step |
| `src/Coords.h`, `src/Json.*`, `src/Readable.*`, `src/PlayerLocator.cpp` | Copied from spike 1: axes, the JSON writer, readable-memory checks, finding V |
| `src/PlayerLocator.h` | V's pose and the active camera |
| `src/Requests.*` | `blockrender.request`'s commands, and `settings.txt` |
| `src/LogQueue.*` | Log lines from any thread, written by the main thread |
| `src/Status.*` | `status.txt`, which the overlay shows |
| `src/sl/Sl.*` | Streamline's structs, checked and copied |
| `src/hooks/ImportPatch.*`, `src/hooks/VtablePatch.*`, `src/hooks/Slots.h` | Import-table and vtable patching, and the patched slots |
| `src/hooks/HookEvents.h` | What the hooks tell the features |
| `src/hooks/StreamlineHooks.*` | The four Streamline imports |
| `src/hooks/D3D12Hooks.*` | The device and factory detours, and the vtable patches on the device, queue, lists and swap chain |
| `src/track/ViewRegistry.*` | RTV and DSV handles to their resources |
| `src/track/StateTracker.*` | Each list's passes, render targets and resource states |
| `src/track/GpuApi.*` | The game's own GPU layer, read with care, and the game's names for its command lists |
| `src/render/Camera.*` | Streamline's camera as matrices for drawing |
| `src/render/Atlas.*` | `atlas.bin` |
| `src/render/Gpu.*` | Small D3D12 helpers: buffers, footprints and copies |
| `src/render/Shaders.*`, `src/render/FrameConstants.h`, `shaders/blocks.hlsl` | The block shaders, compiled in the game |
| `src/render/Renderer.*` | Our D3D12 objects and drawing at `Present` |
| `src/world/Vertex.h`, `src/world/BlockWorld.*`, `src/world/Mesher.*`, `src/world/Structures.*` | Blocks, SkyCraft's vertex, meshing, and the test structures |
| `src/log/DdsWriter.*` | `.dds` files for dumps |
| `src/log/FrameLogData.h`, `src/log/FrameLogJson.cpp`, `src/log/FrameLog.*` | The frame log, and its `frame.json` |
| `src/log/EventNames.*` | PIX event names from `BeginEvent` and `SetMarker` |
| `src/log/Dumps.*` | Buffer dumps |
| `src/Plugin.cpp` | RED4ext entry points, requests, status, and the main-thread update |
| `tests/*` | Spike 1's harness (`Harness.h`, `TestMain.cpp`), one test file per unit, `AtlasFixture.h` and `SlotsC.c` |
| `tools/build_atlas.py` | The block atlas, from the user's own Minecraft jar |
| `tools/view_dump.py` | Dumps to PNGs |
| `tools/frame_summary.py` | `frame.json`, summarised for step 1 |
| `tools/test_*.py` | The tools' tests |
| `tools/blockrender_hud/init.lua` | The CET overlay |

---

### Task 1: Streamline's headers

The user approved this download on 2026-10-01: nine files from NVIDIA's public repository at tag v2.7.2, 70,269 bytes
in all, under the MIT licence.

**Files:**
- Create: `extern/streamline-2.7.2/include/` (eight headers) and `extern/streamline-2.7.2/license.txt`
- Create: `extern/streamline-2.7.2/README.md`

- [x] **Step 1: Check the branch**

```bash
cd /d/Cyberpunk-Minecraft && git switch spike/block-rendering && git status --short && git log --oneline -1
```
Expected: no changed files, and this plan's own commit at the top.

- [x] **Step 2: Download the headers and the licence**

```bash
cd /d/Cyberpunk-Minecraft && mkdir -p extern/streamline-2.7.2/include \
  && for f in sl_appidentity.h sl_consts.h sl_core_api.h sl_core_types.h sl_device_wrappers.h sl_result.h sl_struct.h sl_version.h; do \
       curl -fsSL -o "extern/streamline-2.7.2/include/$f" "https://raw.githubusercontent.com/NVIDIA-RTX/Streamline/v2.7.2/include/$f"; done \
  && curl -fsSL -o extern/streamline-2.7.2/license.txt https://raw.githubusercontent.com/NVIDIA-RTX/Streamline/v2.7.2/license.txt \
  && cd extern/streamline-2.7.2 && sha256sum license.txt include/*.h && cat license.txt include/*.h | wc -c
```
Expected:
```
7b6f23e7d6f3ad6292f9308d2b42cdc3d82ae4e9b2abb55f230279d83bedd43d *license.txt
388d5c18c213e85e87dcc057fb8223b02dee9572675ed55337a4afe9b8d93531 *include/sl_appidentity.h
69a9f35aeecd7fbf68a997872ba2e29f27af7da4675f76b94628393d14474abf *include/sl_consts.h
7a2a36ae27c09991b0bccdb28b1a34e24aa688fbead21dfaaa41e97513abf83a *include/sl_core_api.h
29d0184a91dc24aebc0f1b09217de7b12b6d8186fd8c8c2d021e6e38e0aadf9d *include/sl_core_types.h
55f71095da4da57659752506d120dfb9ccaa360e313cbce6086a6ac4c1475967 *include/sl_device_wrappers.h
cfd0a3bad8886a10e311d3f93a52b18e55b721b020a059b00fb0584a4e843c11 *include/sl_result.h
fa28dacbb8bc0574d6dd2403ec8b01b2d811facd7884251f3834b6e9dbf4d73f *include/sl_struct.h
dc9bf43ae1003d4d94a18e901043724a25dce73ff64ebc5b1d015dd84ef0e4a2 *include/sl_version.h
70269
```
The files keep NVIDIA's CRLF line endings, which is what the hashes are of.

- [x] **Step 3: Write `extern/streamline-2.7.2/README.md`**

```markdown
# NVIDIA Streamline 2.7.2 public headers

Copied unmodified from https://github.com/NVIDIA-RTX/Streamline at tag v2.7.2: the eight headers in `include/` that
`sl_core_api.h` needs, and `license.txt` (MIT). Cyberpunk 2077 2.31 ships Streamline 2.7.1 (`sl.interposer.dll`,
FileVersion 2.7.1.0), and 2.7.2 is the nearest public tag.

BlockRender uses only their types and links nothing from Streamline. `spikes/block-render/tests/TestSl.cpp` pins the
offset of every field the plugin reads, and the plugin checks each struct's type GUID and version before reading it.
```

- [x] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add extern/streamline-2.7.2 \
  && git commit -q -m "block-render: Streamline 2.7.2 headers" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" \
  && git log --oneline -1
```

---

### Task 2: Build skeleton, with spike 1's harness, axes and JSON writer

Spike 1's test harness, `Coords.h` and JSON writer come over unchanged.

**Files:**
- Create: `spikes/block-render/CMakeLists.txt`
- Copy from `spikes/physx-probe/`: `CMakePresets.json`, `tests/Harness.h`, `tests/TestMain.cpp`, `tests/TestCoords.cpp`,
  `tests/TestJson.cpp`, `src/Json.h`, `src/Json.cpp`, `src/Coords.h`

- [x] **Step 1: Copy `../physx-probe/CMakePresets.json` to `CMakePresets.json`**

```bash
mkdir -p /d/Cyberpunk-Minecraft/spikes/block-render/src /d/Cyberpunk-Minecraft/spikes/block-render/tests \
  && cp /d/Cyberpunk-Minecraft/spikes/physx-probe/CMakePresets.json /d/Cyberpunk-Minecraft/spikes/block-render/
```

- [x] **Step 2: Copy `../physx-probe/tests/Harness.h` to `tests/Harness.h`**

```bash
cp /d/Cyberpunk-Minecraft/spikes/physx-probe/tests/Harness.h /d/Cyberpunk-Minecraft/spikes/block-render/tests/
```

- [x] **Step 3: Copy `../physx-probe/tests/TestMain.cpp` to `tests/TestMain.cpp`**

```bash
cp /d/Cyberpunk-Minecraft/spikes/physx-probe/tests/TestMain.cpp /d/Cyberpunk-Minecraft/spikes/block-render/tests/
```

- [x] **Step 4: Copy `../physx-probe/tests/TestCoords.cpp` to `tests/TestCoords.cpp`**

```bash
cp /d/Cyberpunk-Minecraft/spikes/physx-probe/tests/TestCoords.cpp /d/Cyberpunk-Minecraft/spikes/block-render/tests/
```

- [x] **Step 5: Copy `../physx-probe/tests/TestJson.cpp` to `tests/TestJson.cpp`**

```bash
cp /d/Cyberpunk-Minecraft/spikes/physx-probe/tests/TestJson.cpp /d/Cyberpunk-Minecraft/spikes/block-render/tests/
```

- [x] **Step 6: Copy `../physx-probe/src/Json.h` to `src/Json.h`**

```bash
cp /d/Cyberpunk-Minecraft/spikes/physx-probe/src/Json.h /d/Cyberpunk-Minecraft/spikes/block-render/src/
```

- [x] **Step 7: Copy `../physx-probe/src/Json.cpp` to `src/Json.cpp`**

```bash
cp /d/Cyberpunk-Minecraft/spikes/physx-probe/src/Json.cpp /d/Cyberpunk-Minecraft/spikes/block-render/src/
```

- [x] **Step 8: Write `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.25)

project(BlockRender VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(RED4EXT_SDK_DIR "${REPO_ROOT}/extern/RED4ext.SDK" CACHE PATH "RED4ext.SDK checkout")
set(STREAMLINE_DIR "${REPO_ROOT}/extern/streamline-2.7.2" CACHE PATH "Streamline 2.7.2 headers")
set(CP2077_DIR "" CACHE PATH "Cyberpunk 2077 folder; when set, each build copies the plugin into red4ext/plugins/BlockRender")

# Shared settings. Streamline's headers are SYSTEM, which keeps their warnings out of ours.
add_library(render_settings INTERFACE)
target_compile_options(render_settings INTERFACE
	$<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /utf-8 /EHsc /Zc:__cplusplus>
	$<$<COMPILE_LANGUAGE:C>:/W4 /utf-8>)
target_compile_definitions(render_settings INTERFACE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(render_settings INTERFACE src)
target_include_directories(render_settings SYSTEM INTERFACE "${STREAMLINE_DIR}/include")

# Everything that doesn't need RED4ext or the running game.
add_library(render_core STATIC
	src/Json.cpp
)
target_link_libraries(render_core PUBLIC render_settings d3dcompiler dxguid)

# Unit tests: no game needed. They run in this folder, so tests can find shaders/.
enable_testing()
add_executable(render_tests
	tests/TestMain.cpp
	tests/TestCoords.cpp
	tests/TestJson.cpp
)
target_link_libraries(render_tests PRIVATE render_core)
add_test(NAME render_tests COMMAND render_tests WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
```

- [x] **Step 9: Configure and build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'Coords.h'`.

- [x] **Step 10: Copy `../physx-probe/src/Coords.h` to `src/Coords.h`**

```bash
cp /d/Cyberpunk-Minecraft/spikes/physx-probe/src/Coords.h /d/Cyberpunk-Minecraft/spikes/block-render/src/
```

- [x] **Step 11: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `7 test(s), 0 failure(s)`.

- [x] **Step 12: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: build skeleton, with spike 1's harness, axes and JSON writer" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 3: Readable-memory checks

Spike 1's `ReadableMemory` checks that memory can be read before anything reads it, and caches the regions it has
checked. The game's crash handler means a fault is never safe, so every read of game or Streamline memory goes through
it. It comes over with one change: the namespace is `cpmc`, not `cpmc::px`.

**Files:**
- Copy from `spikes/physx-probe/`, then edit: `src/Readable.h`, `src/Readable.cpp`, `tests/TestReadable.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Copy `../physx-probe/tests/TestReadable.cpp` to `tests/TestReadable.cpp`**

```bash
cp /d/Cyberpunk-Minecraft/spikes/physx-probe/tests/TestReadable.cpp /d/Cyberpunk-Minecraft/spikes/block-render/tests/
```

- [x] **Step 2: Edit `tests/TestReadable.cpp` for the new path and namespace**

```cpp
#include "physx/Readable.h"
```
```cpp
#include "Readable.h"
```
```cpp
using cpmc::px::ReadableMemory;
```
```cpp
using cpmc::ReadableMemory;
```

- [x] **Step 3: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestJson.cpp
)
```
```cmake
	tests/TestJson.cpp
	tests/TestReadable.cpp
)
```

- [x] **Step 4: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'Readable.h'`.

- [x] **Step 5: Copy `../physx-probe/src/physx/Readable.h` to `src/Readable.h`**

```bash
cp /d/Cyberpunk-Minecraft/spikes/physx-probe/src/physx/Readable.h /d/Cyberpunk-Minecraft/spikes/block-render/src/
```

- [x] **Step 6: Edit `src/Readable.h` for the namespace**

```cpp
namespace cpmc::px
```
```cpp
namespace cpmc
```

- [x] **Step 7: Copy `../physx-probe/src/physx/Readable.cpp` to `src/Readable.cpp`**

```bash
cp /d/Cyberpunk-Minecraft/spikes/physx-probe/src/physx/Readable.cpp /d/Cyberpunk-Minecraft/spikes/block-render/src/
```

- [x] **Step 8: Edit `src/Readable.cpp` for the path and namespace**

```cpp
#include "physx/Readable.h"
```
```cpp
#include "Readable.h"
```
```cpp
namespace cpmc::px
```
```cpp
namespace cpmc
```

- [x] **Step 9: Edit `CMakeLists.txt` to add the source**

```cmake
	src/Json.cpp
)
```
```cmake
	src/Json.cpp
	src/Readable.cpp
)
```

- [x] **Step 10: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `11 test(s), 0 failure(s)`.

- [x] **Step 11: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: readable-memory checks, from spike 1" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 4: Requests and settings

`blockrender.request` holds one command, which the plugin reads once a second (the spec's "Requests"). `settings.txt`
turns on DRED or the D3D12 debug layer at load (the spec's "Safety").

**Files:**
- Create: `src/Requests.h`, `src/Requests.cpp`, `tests/TestRequests.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestRequests.cpp`**

```cpp
#include "Harness.h"

#include "Requests.h"

using cpmc::ParseRequest;
using cpmc::Request;

namespace
{
std::optional<Request> Parse(std::string_view aText)
{
	std::string error;
	return ParseRequest(aText, error);
}

std::string ErrorOf(std::string_view aText)
{
	std::string error;
	ParseRequest(aText, error);
	return error;
}
}

TEST(RequestsParseEachCommand)
{
	CHECK(Parse("log")->kind == Request::Kind::Log);
	CHECK(Parse("clear")->kind == Request::Kind::Clear);
	CHECK(Parse("reload")->kind == Request::Kind::Reload);

	const auto dump = Parse("dump gbuffer");
	CHECK(dump->kind == Request::Kind::Dump);
	CHECK_STR(dump->what, "gbuffer");

	const auto mode = Parse("mode present");
	CHECK(mode->kind == Request::Kind::Mode);
	CHECK_STR(mode->what, "present");

	const auto cube = Parse("place cube");
	CHECK(cube->kind == Request::Kind::Place);
	CHECK_STR(cube->what, "cube");

	const auto field = Parse("place field 100000");
	CHECK(field->kind == Request::Kind::Place);
	CHECK_STR(field->what, "field");
	CHECK_EQ(field->number, 100000.0);

	const auto set = Parse("set exposure 0.25");
	CHECK(set->kind == Request::Kind::Set);
	CHECK_STR(set->what, "exposure");
	CHECK_EQ(set->number, 0.25);
}

TEST(RequestsIgnoreSurroundingWhitespace)
{
	const auto mode = Parse("  mode\toff \r\n");
	CHECK(mode.has_value());
	CHECK_STR(mode->what, "off");
}

TEST(RequestsRejectBadInput)
{
	CHECK(!Parse(""));
	CHECK(!Parse("   \n"));
	CHECK(!Parse("log now"));
	CHECK(!Parse("dump everything"));
	CHECK(!Parse("mode on"));
	CHECK(!Parse("place tower"));
	CHECK(!Parse("place field"));
	CHECK(!Parse("place field 0"));
	CHECK(!Parse("place field 2.5"));
	CHECK(!Parse("place field 1000001"));
	CHECK(!Parse("set Exposure 1"));
	CHECK(!Parse("set exposure"));
	CHECK(!Parse("set exposure nan"));
	CHECK(!Parse("set exposure 1x"));
	CHECK(!Parse("Log"));
}

TEST(RequestsSayWhatWentWrong)
{
	CHECK_STR(ErrorOf(""), "the request is empty");
	CHECK_STR(ErrorOf("fly"), "unknown command \"fly\"");
	CHECK_STR(ErrorOf("mode on"), "mode takes one of: off, present, predlss, gbuffer");
	CHECK_STR(ErrorOf("place field 0"), "place field takes a whole number of blocks from 1 to 1000000");
}

TEST(SettingsReadDredAndDebugLayer)
{
	const cpmc::PluginSettings none = cpmc::ParseSettings("");
	CHECK(!none.dred && !none.debugLayer);

	const cpmc::PluginSettings both = cpmc::ParseSettings("# chasing a GPU crash\r\ndred = 1\r\ndebuglayer=1  # slow\r\n");
	CHECK(both.dred && both.debugLayer);

	const cpmc::PluginSettings off = cpmc::ParseSettings("dred = 0\ndebuglayer = yes\nother = 1\ndred\n= 1\n");
	CHECK(!off.dred && !off.debugLayer);
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestReadable.cpp
)
```
```cmake
	tests/TestReadable.cpp
	tests/TestRequests.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'Requests.h'`.

- [x] **Step 4: Write `src/Requests.h`**

```cpp
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace cpmc
{
// One command from blockrender.request (see the spec's "Requests"):
//   log
//   dump depth|mvec|color|gbuffer|all
//   mode off|present|predlss|gbuffer
//   place cube|sampler|house
//   place field <n>            (n: 1 to 1,000,000 blocks)
//   clear
//   reload
//   set <name> <value>         (name: lower-case letters, digits, '.' and '_'; value: a finite number)
struct Request
{
	enum class Kind
	{
		Log,
		Dump,
		Mode,
		Place,
		Clear,
		Reload,
		Set,
	};

	Kind        kind = Kind::Log;
	std::string what;          // the dump target, mode, structure or setting name
	double      number = 0.0;  // the field's block count, or the setting's value
};

// The command in aText (one line; surrounding whitespace is ignored). Nothing, with aError saying why, if the text
// isn't exactly one valid command.
std::optional<Request> ParseRequest(std::string_view aText, std::string& aError);

// settings.txt, read once at load (see the spec's "Safety"): lines of "name = value", where "#" starts a comment. Two
// settings, both off unless set to 1. Other names and values are ignored.
struct PluginSettings
{
	bool dred = false;        // DRED's breadcrumbs and page faults
	bool debugLayer = false;  // the D3D12 debug layer
};

PluginSettings ParseSettings(std::string_view aText);
}
```

- [x] **Step 5: Write `src/Requests.cpp`**

```cpp
#include "Requests.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <initializer_list>
#include <vector>

namespace cpmc
{
namespace
{
bool IsSpace(char aChar)
{
	return aChar == ' ' || aChar == '\t' || aChar == '\r' || aChar == '\n';
}

std::vector<std::string_view> Words(std::string_view aText)
{
	std::vector<std::string_view> words;
	size_t                        i = 0;
	while (i < aText.size())
	{
		while (i < aText.size() && IsSpace(aText[i]))
		{
			++i;
		}
		const size_t start = i;
		while (i < aText.size() && !IsSpace(aText[i]))
		{
			++i;
		}
		if (i > start)
		{
			words.push_back(aText.substr(start, i - start));
		}
	}
	return words;
}

// A finite number that is the whole word.
std::optional<double> Number(std::string_view aWord)
{
	double     value = 0.0;
	const auto end = aWord.data() + aWord.size();
	const auto [stop, error] = std::from_chars(aWord.data(), end, value);
	if (error != std::errc() || stop != end || !std::isfinite(value))
	{
		return std::nullopt;
	}
	return value;
}

bool OneOf(std::string_view aWord, std::initializer_list<std::string_view> aChoices)
{
	return std::find(aChoices.begin(), aChoices.end(), aWord) != aChoices.end();
}

bool IsSettingName(std::string_view aWord)
{
	return !aWord.empty() && aWord.size() <= 40 && std::all_of(aWord.begin(), aWord.end(), [](char aChar) {
		return (aChar >= 'a' && aChar <= 'z') || (aChar >= '0' && aChar <= '9') || aChar == '.' || aChar == '_';
	});
}

std::optional<Request> Fail(std::string& aError, std::string aWhy)
{
	aError = std::move(aWhy);
	return std::nullopt;
}
}

std::optional<Request> ParseRequest(std::string_view aText, std::string& aError)
{
	const std::vector<std::string_view> words = Words(aText);
	if (words.empty())
	{
		return Fail(aError, "the request is empty");
	}
	const std::string_view command = words[0];
	const size_t           arguments = words.size() - 1;
	if (command == "log" || command == "clear" || command == "reload")
	{
		if (arguments != 0)
		{
			return Fail(aError, std::string(command) + " takes nothing after it");
		}
		const Request::Kind kind = command == "log" ? Request::Kind::Log : command == "clear" ? Request::Kind::Clear : Request::Kind::Reload;
		return Request{ kind };
	}
	if (command == "dump")
	{
		if (arguments != 1 || !OneOf(words[1], { "depth", "mvec", "color", "gbuffer", "all" }))
		{
			return Fail(aError, "dump takes one of: depth, mvec, color, gbuffer, all");
		}
		return Request{ Request::Kind::Dump, std::string(words[1]) };
	}
	if (command == "mode")
	{
		if (arguments != 1 || !OneOf(words[1], { "off", "present", "predlss", "gbuffer" }))
		{
			return Fail(aError, "mode takes one of: off, present, predlss, gbuffer");
		}
		return Request{ Request::Kind::Mode, std::string(words[1]) };
	}
	if (command == "place")
	{
		if (arguments == 1 && OneOf(words[1], { "cube", "sampler", "house" }))
		{
			return Request{ Request::Kind::Place, std::string(words[1]) };
		}
		if (arguments == 2 && words[1] == "field")
		{
			const std::optional<double> blocks = Number(words[2]);
			if (!blocks || *blocks < 1.0 || *blocks > 1'000'000.0 || *blocks != std::floor(*blocks))
			{
				return Fail(aError, "place field takes a whole number of blocks from 1 to 1000000");
			}
			return Request{ Request::Kind::Place, "field", *blocks };
		}
		return Fail(aError, "place takes cube, sampler, house, or field <blocks>");
	}
	if (command == "set")
	{
		if (arguments != 2 || !IsSettingName(words[1]))
		{
			return Fail(aError, "set takes a name (a-z, 0-9, '.', '_') and a number");
		}
		const std::optional<double> value = Number(words[2]);
		if (!value)
		{
			return Fail(aError, "set takes a finite number as its value");
		}
		return Request{ Request::Kind::Set, std::string(words[1]), *value };
	}
	return Fail(aError, "unknown command \"" + std::string(command.substr(0, 40)) + "\"");
}

PluginSettings ParseSettings(std::string_view aText)
{
	PluginSettings settings;
	for (size_t start = 0; start <= aText.size();)
	{
		const size_t     end = std::min(aText.find('\n', start), aText.size());
		std::string_view line = aText.substr(start, end - start);
		start = end + 1;
		line = line.substr(0, line.find('#'));
		const size_t equals = line.find('=');
		if (equals == std::string_view::npos)
		{
			continue;
		}
		const std::vector<std::string_view> name = Words(line.substr(0, equals));
		const std::vector<std::string_view> value = Words(line.substr(equals + 1));
		if (name.size() != 1 || value.size() != 1)
		{
			continue;
		}
		if (name[0] == "dred")
		{
			settings.dred = value[0] == "1";
		}
		else if (name[0] == "debuglayer")
		{
			settings.debugLayer = value[0] == "1";
		}
	}
	return settings;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/Readable.cpp
)
```
```cmake
	src/Readable.cpp
	src/Requests.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `16 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: request and settings parsing" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 5: The log queue

The game's render threads call the hooks, but only the main thread writes to RED4ext's log, as in spike 1. Lines
wait in a queue of at most 2,000, and lines beyond that are counted.

**Files:**
- Create: `src/LogQueue.h`, `src/LogQueue.cpp`, `tests/TestLogQueue.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestLogQueue.cpp`**

```cpp
#include "Harness.h"

#include "LogQueue.h"

#include <thread>

TEST(LogQueueDrainsInOrder)
{
	cpmc::LogQueue queue;
	queue.Push("one");
	queue.Push("two");
	const auto lines = queue.Drain();
	CHECK_EQ(lines.size(), size_t(2));
	CHECK_STR(lines[0], "one");
	CHECK_STR(lines[1], "two");
	CHECK(queue.Drain().empty());
}

TEST(LogQueueCountsWhatItDrops)
{
	cpmc::LogQueue queue(2);
	queue.Push("a");
	queue.Push("b");
	queue.Push("c");
	queue.Push("d");
	const auto lines = queue.Drain();
	CHECK_EQ(lines.size(), size_t(3));
	CHECK_STR(lines[2], "(2 more log lines were dropped)");
	queue.Push("e");
	CHECK_EQ(queue.Drain().size(), size_t(1));
}

TEST(LogQueueTakesLinesFromManyThreads)
{
	cpmc::LogQueue           queue;
	std::vector<std::thread> threads;
	for (int t = 0; t < 4; ++t)
	{
		threads.emplace_back([&queue] {
			for (int i = 0; i < 100; ++i)
			{
				queue.Push("line");
			}
		});
	}
	for (auto& thread : threads)
	{
		thread.join();
	}
	CHECK_EQ(queue.Drain().size(), size_t(400));
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestRequests.cpp
)
```
```cmake
	tests/TestRequests.cpp
	tests/TestLogQueue.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'LogQueue.h'`.

- [x] **Step 4: Write `src/LogQueue.h`**

```cpp
#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace cpmc
{
// Log lines from any thread, written later by the main thread (only the main thread writes to RED4ext's log). Holds
// at most aCapacity lines; further lines are counted and dropped until the queue is drained.
class LogQueue
{
public:
	explicit LogQueue(size_t aCapacity = 2000);

	void Push(std::string aLine);

	// Every queued line, oldest first, then a line saying how many were dropped, if any were.
	std::vector<std::string> Drain();

private:
	std::mutex               mutex_;
	std::vector<std::string> lines_;
	size_t                   capacity_;
	size_t                   dropped_ = 0;
};
}
```

- [x] **Step 5: Write `src/LogQueue.cpp`**

```cpp
#include "LogQueue.h"

#include <format>

namespace cpmc
{
LogQueue::LogQueue(size_t aCapacity)
	: capacity_(aCapacity)
{
}

void LogQueue::Push(std::string aLine)
{
	std::lock_guard lock(mutex_);
	if (lines_.size() >= capacity_)
	{
		++dropped_;
		return;
	}
	lines_.push_back(std::move(aLine));
}

std::vector<std::string> LogQueue::Drain()
{
	std::lock_guard          lock(mutex_);
	std::vector<std::string> lines;
	lines.swap(lines_);
	if (dropped_ > 0)
	{
		lines.push_back(std::format("({} more log lines were dropped)", dropped_));
		dropped_ = 0;
	}
	return lines;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/Requests.cpp
)
```
```cmake
	src/Requests.cpp
	src/LogQueue.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `19 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: log queue" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" \
  && git log --oneline -1
```

---

### Task 6: Streamline's structs

What the game hands Streamline is checked before it's read:
- **Readable:** the memory is readable.
- **Type:** the struct carries the type GUID it should, and a version the plugin knows.

Then the fields the plugin uses are copied into plain structs. The tests pin every field's offset against the 2.7.2
headers, and check that local tags replace global tags of the same type for one evaluate. The buffer type names follow
`sl_consts.h`.

**Files:**
- Create: `src/sl/Sl.h`, `src/sl/Sl.cpp`, `tests/TestSl.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestSl.cpp`**

```cpp
#include "Harness.h"

#include "sl/Sl.h"

#include <cstddef>

using cpmc::ReadableMemory;

// Streamline 2.7.2's layout, as the plugin reads it from the game's 2.7.1 calls. A wrong offset fails the build.
static_assert(sizeof(sl::BaseStructure) == 32);
static_assert(offsetof(sl::BaseStructure, structType) == 8);
static_assert(offsetof(sl::BaseStructure, structVersion) == 24);
static_assert(offsetof(sl::Constants, cameraViewToClip) == 32);
static_assert(offsetof(sl::Constants, clipToCameraView) == 96);
static_assert(offsetof(sl::Constants, clipToPrevClip) == 224);
static_assert(offsetof(sl::Constants, prevClipToClip) == 288);
static_assert(offsetof(sl::Constants, jitterOffset) == 352);
static_assert(offsetof(sl::Constants, mvecScale) == 360);
static_assert(offsetof(sl::Constants, cameraPinholeOffset) == 368);
static_assert(offsetof(sl::Constants, cameraPos) == 376);
static_assert(offsetof(sl::Constants, cameraUp) == 388);
static_assert(offsetof(sl::Constants, cameraRight) == 400);
static_assert(offsetof(sl::Constants, cameraFwd) == 412);
static_assert(offsetof(sl::Constants, cameraNear) == 424);
static_assert(offsetof(sl::Constants, cameraFar) == 428);
static_assert(offsetof(sl::Constants, cameraFOV) == 432);
static_assert(offsetof(sl::Constants, cameraAspectRatio) == 436);
static_assert(offsetof(sl::Constants, motionVectorsInvalidValue) == 440);
static_assert(offsetof(sl::Constants, depthInverted) == 444);
static_assert(offsetof(sl::Constants, motionVectorsJittered) == 450);
static_assert(offsetof(sl::Resource, native) == 40);
static_assert(offsetof(sl::Resource, state) == 64);
static_assert(offsetof(sl::ResourceTag, resource) == 32);
static_assert(offsetof(sl::ResourceTag, type) == 40);
static_assert(offsetof(sl::ResourceTag, lifecycle) == 44);
static_assert(offsetof(sl::ResourceTag, extent) == 48);
static_assert(sizeof(sl::ViewportHandle) == 40);

TEST(SlReadsConstants)
{
	sl::Constants constants;
	constants.cameraViewToClip.row[3] = { 0.0f, 0.0f, 0.1f, 0.0f };
	constants.jitterOffset = { 0.25f, -0.5f };
	constants.cameraPos = { 1.0f, 2.0f, 3.0f };
	constants.cameraFwd = { 0.0f, 1.0f, 0.0f };
	constants.cameraNear = 0.1f;
	constants.depthInverted = sl::Boolean::eTrue;
	ReadableMemory memory;
	const auto     read = cpmc::ReadConstants(memory, &constants);
	CHECK(read.has_value());
	CHECK_EQ(read->structVersion, uint64_t(2));
	CHECK_EQ(read->viewToClip[14], 0.1f);
	CHECK_EQ(read->jitter[1], -0.5f);
	CHECK_EQ(read->position[2], 3.0f);
	CHECK_EQ(read->forward[1], 1.0f);
	CHECK_EQ(read->nearPlane, 0.1f);
	CHECK_EQ(int(read->depthInverted), 1);
	CHECK_EQ(int(read->cameraMotionIncluded), 2);  // not set
}

TEST(SlRefusesOtherStructsAndBadPointers)
{
	sl::ViewportHandle viewport(3u);
	ReadableMemory     memory;
	CHECK(!cpmc::ReadConstants(memory, &viewport));
	CHECK(!cpmc::ReadConstants(memory, nullptr));
	CHECK(!cpmc::ReadTag(memory, &viewport, false));
	sl::Constants old;
	old.structVersion = 0;
	CHECK(!cpmc::ReadConstants(memory, &old));
}

TEST(SlReadsTagsAndViewports)
{
	int             fakeTexture = 0;
	sl::Resource    resource(sl::ResourceType::eTex2d, &fakeTexture, 0x80u);
	const sl::Extent extent{ 0, 0, 1707, 960 };
	sl::ResourceTag tag(&resource, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, &extent);
	ReadableMemory  memory;
	const auto      read = cpmc::ReadTag(memory, &tag, true);
	CHECK(read.has_value());
	CHECK_EQ(read->type, sl::kBufferTypeDepth);
	CHECK(read->resource == &fakeTexture);
	CHECK_EQ(read->state, 0x80u);
	CHECK_EQ(read->lifecycle, uint32_t(sl::ResourceLifecycle::eValidUntilPresent));
	CHECK_EQ(read->extent.width, 1707u);
	CHECK(read->local);

	sl::ResourceTag dropped(nullptr, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent);
	const auto      none = cpmc::ReadTag(memory, &dropped, false);
	CHECK(none.has_value());
	CHECK(none->resource == nullptr);

	sl::ViewportHandle viewport(7u);
	CHECK_EQ(*cpmc::ReadViewport(memory, &viewport), 7u);
	CHECK(!cpmc::ReadViewport(memory, &tag));
}

TEST(SlWalksEvaluateInputsAndTheirChains)
{
	sl::ViewportHandle viewport(0u);
	int                fake = 0;
	sl::Resource       resource(sl::ResourceType::eTex2d, &fake, 0u);
	sl::ResourceTag    depth(&resource, sl::kBufferTypeDepth, sl::ResourceLifecycle::eOnlyValidNow);
	sl::ResourceTag    mvec(&resource, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eOnlyValidNow);
	depth.next = &mvec;
	const sl::BaseStructure* inputs[] = { &viewport, &depth };
	ReadableMemory           memory;
	const auto               structs = cpmc::EvaluateInputs(memory, inputs, 2);
	CHECK_EQ(structs.size(), size_t(3));
	CHECK(structs[0] == &viewport);
	CHECK(structs[1] == &depth);
	CHECK(structs[2] == &mvec);
	CHECK(cpmc::EvaluateInputs(memory, nullptr, 2).empty());
}

TEST(SlLocalTagsReplaceGlobalOnesOfTheirType)
{
	cpmc::SlTag globalDepth{ sl::kBufferTypeDepth, reinterpret_cast<void*>(1) };
	cpmc::SlTag globalColor{ sl::kBufferTypeScalingInputColor, reinterpret_cast<void*>(2) };
	cpmc::SlTag localDepth{ sl::kBufferTypeDepth, reinterpret_cast<void*>(3) };
	localDepth.local = true;
	const auto tags = cpmc::TagsForEvaluate({ globalDepth, globalColor }, { localDepth });
	CHECK_EQ(tags.size(), size_t(2));
	CHECK(tags[0].resource == reinterpret_cast<void*>(2));
	CHECK(tags[1].resource == reinterpret_cast<void*>(3));
}

TEST(SlNamesBufferTypes)
{
	CHECK_STR(cpmc::BufferTypeName(sl::kBufferTypeDepth), "depth");
	CHECK_STR(cpmc::BufferTypeName(sl::kBufferTypeScalingInputColor), "scalingInputColor");
	CHECK_STR(cpmc::BufferTypeName(sl::kBufferTypeExposure), "exposure");
	CHECK_STR(cpmc::BufferTypeName(sl::kBufferTypeUIColorAndAlpha), "uiColorAndAlpha");
	CHECK_STR(cpmc::BufferTypeName(sl::kBufferTypeColorAfterDepthOfField), "colorAfterDepthOfField");
	CHECK_STR(cpmc::BufferTypeName(500), "type 500");
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestLogQueue.cpp
)
```
```cmake
	tests/TestLogQueue.cpp
	tests/TestSl.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'sl/Sl.h'`.

- [x] **Step 4: Write `src/sl/Sl.h`**

```cpp
#pragma once

// Streamline 2.7.2's public types (extern/streamline-2.7.2, MIT), and the checks the plugin makes before it trusts
// what the game hands Streamline. The game's sl.interposer.dll is 2.7.1; the structs carry a type GUID and a version,
// so each one is checked before it's read.
#include <sl_core_api.h>

#include "Readable.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cpmc
{
// What the plugin keeps of one sl::Constants: plain copies, in Streamline's own row-major layout. The flags are
// sl::Boolean values: 0 false, 1 true, 2 not set.
struct SlConstants
{
	uint64_t              structVersion = 0;
	std::array<float, 16> viewToClip{};
	std::array<float, 16> clipToView{};
	std::array<float, 16> clipToPrevClip{};
	std::array<float, 16> prevClipToClip{};
	std::array<float, 2>  jitter{};
	std::array<float, 2>  mvecScale{};
	std::array<float, 2>  pinholeOffset{};
	std::array<float, 3>  position{};
	std::array<float, 3>  up{};
	std::array<float, 3>  right{};
	std::array<float, 3>  forward{};
	float                 nearPlane = 0.0f;
	float                 farPlane = 0.0f;
	float                 fov = 0.0f;  // radians
	float                 aspect = 0.0f;
	float                 motionVectorsInvalidValue = 0.0f;
	uint8_t               depthInverted = 2;
	uint8_t               cameraMotionIncluded = 2;
	uint8_t               motionVectors3D = 2;
	uint8_t               reset = 2;
	uint8_t               orthographic = 2;
	uint8_t               motionVectorsDilated = 2;
	uint8_t               motionVectorsJittered = 2;
};

// One buffer tag: global (from slSetTag) or local (among an slEvaluateFeature's inputs).
struct SlTag
{
	uint32_t   type = 0;
	void*      resource = nullptr;  // the native ID3D12Resource; not referenced
	uint32_t   state = 0xFFFFFFFF;  // its D3D12_RESOURCE_STATES at the evaluate; 0xFFFFFFFF if the game didn't say
	uint32_t   lifecycle = 0;       // sl::ResourceLifecycle
	sl::Extent extent{};            // all zero: the whole resource
	bool       local = false;
};

// "depth", "motionVectors", ... as Streamline names them, or "type <n>" past the ones 2.7.2 knows.
std::string BufferTypeName(uint32_t aType);

// The struct at aStruct as an sl::Constants, if it is one: readable, with Constants' type GUID and version 1 or
// later. Version 1's fields are all the plugin reads.
std::optional<SlConstants> ReadConstants(ReadableMemory& aMemory, const void* aStruct);

// The struct at aStruct as an sl::ResourceTag, with the sl::Resource it points to. A tag whose resource pointer is
// null is valid: the game sets one to drop a tag.
std::optional<SlTag> ReadTag(ReadableMemory& aMemory, const void* aStruct, bool aLocal);

// The value of an sl::ViewportHandle, if aStruct is one.
std::optional<uint32_t> ReadViewport(ReadableMemory& aMemory, const void* aStruct);

// The structures an slEvaluateFeature call was given: each input, then the structures chained to it through `next`.
// At most 32 inputs and 16 structures per chain; an unreadable pointer ends its chain.
std::vector<const sl::BaseStructure*> EvaluateInputs(ReadableMemory& aMemory, const sl::BaseStructure* const* aInputs,
                                                     uint32_t aCount);

// The tags an evaluate uses: the global ones, with each local tag replacing the global tag of its type.
std::vector<SlTag> TagsForEvaluate(const std::vector<SlTag>& aGlobal, const std::vector<SlTag>& aLocal);
}
```

- [x] **Step 5: Write `src/sl/Sl.cpp`**

```cpp
#include "sl/Sl.h"

#include <cstddef>
#include <cstring>
#include <format>
#include <string>

namespace cpmc
{
// The plugin reads these offsets; their values are pinned by tests/TestSl.cpp.
static_assert(sizeof(sl::BaseStructure) == 32);
static_assert(offsetof(sl::Constants, motionVectorsJittered) == 450);
static_assert(offsetof(sl::ResourceTag, extent) == 48);
static_assert(offsetof(sl::Resource, state) == 64);

namespace
{
// Every version-1 field of sl::Constants ends before this many bytes.
constexpr size_t kConstantsV1Bytes = offsetof(sl::Constants, motionVectorsJittered) + 1;
// Of sl::Resource the plugin reads up to its state.
constexpr size_t kResourceBytes = offsetof(sl::Resource, state) + sizeof(uint32_t);

constexpr const char* kBufferTypeNames[] = {
	"depth", "motionVectors", "hudLessColor", "scalingInputColor", "scalingOutputColor", "normals", "roughness",
	"albedo", "specularAlbedo", "indirectAlbedo", "specularMotionVectors", "disocclusionMask", "emissive", "exposure",
	"normalRoughness", "diffuseHitNoisy", "diffuseHitDenoised", "specularHitNoisy", "specularHitDenoised",
	"shadowNoisy", "shadowDenoised", "ambientOcclusionNoisy", "ambientOcclusionDenoised", "uiColorAndAlpha",
	"shadowHint", "reflectionHint", "particleHint", "transparencyHint", "animatedTextureHint", "biasCurrentColorHint",
	"raytracingDistance", "reflectionMotionVectors", "position", "invalidDepthMotionHint", "alpha", "opaqueColor",
	"reactiveMaskHint", "transparencyAndCompositionMaskHint", "reflectedAlbedo", "colorBeforeParticles",
	"colorBeforeTransparency", "colorBeforeFog", "specularHitDistance", "specularRayDirectionHitDistance",
	"specularRayDirection", "diffuseHitDistance", "diffuseRayDirectionHitDistance", "diffuseRayDirection",
	"hiResDepth", "linearDepth", "bidirectionalDistortionField", "transparencyLayer", "transparencyLayerOpacity",
	"backbuffer", "noWarpMask", "colorAfterParticles", "colorAfterTransparency", "colorAfterFog",
	"screenSpaceSubsurfaceScatteringGuide", "colorBeforeScreenSpaceSubsurfaceScattering",
	"colorAfterScreenSpaceSubsurfaceScattering", "screenSpaceRefractionGuide", "colorBeforeScreenSpaceRefraction",
	"colorAfterScreenSpaceRefraction", "depthOfFieldGuide", "colorBeforeDepthOfField", "colorAfterDepthOfField"
};
static_assert(std::size(kBufferTypeNames) == sl::kBufferTypeColorAfterDepthOfField + 1);

// A readable struct of at least aBytes, with aType's GUID and at least aVersion. aBytes covers the base too.
const sl::BaseStructure* Struct(ReadableMemory& aMemory, const void* aStruct, const sl::StructType& aType,
                                uint64_t aVersion, size_t aBytes)
{
	if (!aMemory.Range(aStruct, aBytes))
	{
		return nullptr;
	}
	const auto* base = static_cast<const sl::BaseStructure*>(aStruct);
	if (base->structType != aType || base->structVersion < aVersion)
	{
		return nullptr;
	}
	return base;
}

template<size_t N>
std::array<float, N> Copy(const float* aFrom)
{
	std::array<float, N> to{};
	std::memcpy(to.data(), aFrom, sizeof(float) * N);
	return to;
}

std::array<float, 16> CopyMatrix(const sl::float4x4& aMatrix)
{
	return Copy<16>(&aMatrix.row[0].x);
}
}

std::string BufferTypeName(uint32_t aType)
{
	if (aType < std::size(kBufferTypeNames))
	{
		return kBufferTypeNames[aType];
	}
	return std::format("type {}", aType);
}

std::optional<SlConstants> ReadConstants(ReadableMemory& aMemory, const void* aStruct)
{
	const auto* base = Struct(aMemory, aStruct, sl::Constants::s_structType, sl::kStructVersion1, kConstantsV1Bytes);
	if (!base)
	{
		return std::nullopt;
	}
	const auto& from = *static_cast<const sl::Constants*>(base);
	SlConstants to;
	to.structVersion = from.structVersion;
	to.viewToClip = CopyMatrix(from.cameraViewToClip);
	to.clipToView = CopyMatrix(from.clipToCameraView);
	to.clipToPrevClip = CopyMatrix(from.clipToPrevClip);
	to.prevClipToClip = CopyMatrix(from.prevClipToClip);
	to.jitter = { from.jitterOffset.x, from.jitterOffset.y };
	to.mvecScale = { from.mvecScale.x, from.mvecScale.y };
	to.pinholeOffset = { from.cameraPinholeOffset.x, from.cameraPinholeOffset.y };
	to.position = { from.cameraPos.x, from.cameraPos.y, from.cameraPos.z };
	to.up = { from.cameraUp.x, from.cameraUp.y, from.cameraUp.z };
	to.right = { from.cameraRight.x, from.cameraRight.y, from.cameraRight.z };
	to.forward = { from.cameraFwd.x, from.cameraFwd.y, from.cameraFwd.z };
	to.nearPlane = from.cameraNear;
	to.farPlane = from.cameraFar;
	to.fov = from.cameraFOV;
	to.aspect = from.cameraAspectRatio;
	to.motionVectorsInvalidValue = from.motionVectorsInvalidValue;
	to.depthInverted = static_cast<uint8_t>(from.depthInverted);
	to.cameraMotionIncluded = static_cast<uint8_t>(from.cameraMotionIncluded);
	to.motionVectors3D = static_cast<uint8_t>(from.motionVectors3D);
	to.reset = static_cast<uint8_t>(from.reset);
	to.orthographic = static_cast<uint8_t>(from.orthographicProjection);
	to.motionVectorsDilated = static_cast<uint8_t>(from.motionVectorsDilated);
	to.motionVectorsJittered = static_cast<uint8_t>(from.motionVectorsJittered);
	return to;
}

std::optional<SlTag> ReadTag(ReadableMemory& aMemory, const void* aStruct, bool aLocal)
{
	const auto* base = Struct(aMemory, aStruct, sl::ResourceTag::s_structType, sl::kStructVersion1, sizeof(sl::ResourceTag));
	if (!base)
	{
		return std::nullopt;
	}
	const auto& tag = *static_cast<const sl::ResourceTag*>(base);
	SlTag       to;
	to.type = tag.type;
	to.lifecycle = static_cast<uint32_t>(tag.lifecycle);
	to.extent = tag.extent;
	to.local = aLocal;
	if (tag.resource)
	{
		const auto* resource = Struct(aMemory, tag.resource, sl::Resource::s_structType, sl::kStructVersion1, kResourceBytes);
		if (!resource)
		{
			return std::nullopt;
		}
		const auto& native = *static_cast<const sl::Resource*>(resource);
		to.resource = native.native;
		to.state = native.state;
	}
	return to;
}

std::optional<uint32_t> ReadViewport(ReadableMemory& aMemory, const void* aStruct)
{
	const auto* base = Struct(aMemory, aStruct, sl::ViewportHandle::s_structType, sl::kStructVersion1, sizeof(sl::ViewportHandle));
	if (!base)
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(*static_cast<const sl::ViewportHandle*>(base));
}

std::vector<const sl::BaseStructure*> EvaluateInputs(ReadableMemory& aMemory, const sl::BaseStructure* const* aInputs,
                                                     uint32_t aCount)
{
	std::vector<const sl::BaseStructure*> structs;
	const uint32_t                        count = aCount < 32 ? aCount : 32;
	if (count == 0 || !aMemory.Range(aInputs, count * sizeof(aInputs[0])))
	{
		return structs;
	}
	for (uint32_t i = 0; i < count; ++i)
	{
		const sl::BaseStructure* at = aInputs[i];
		for (int depth = 0; at && depth < 16; ++depth)
		{
			if (!aMemory.Range(at, sizeof(sl::BaseStructure)))
			{
				break;
			}
			structs.push_back(at);
			at = at->next;
		}
	}
	return structs;
}

std::vector<SlTag> TagsForEvaluate(const std::vector<SlTag>& aGlobal, const std::vector<SlTag>& aLocal)
{
	std::vector<SlTag> tags;
	for (const SlTag& global : aGlobal)
	{
		bool replaced = false;
		for (const SlTag& local : aLocal)
		{
			replaced = replaced || local.type == global.type;
		}
		if (!replaced)
		{
			tags.push_back(global);
		}
	}
	tags.insert(tags.end(), aLocal.begin(), aLocal.end());
	return tags;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/LogQueue.cpp
)
```
```cmake
	src/LogQueue.cpp
	src/sl/Sl.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `25 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: Streamline structs, checked and copied" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 7: Import and vtable patching

Both patchers keep the original first, then swap one pointer atomically, so no thread can reach a hook before its
original is known:
- **`ImportPatch`:** finds an import by DLL and function name in a module's import directory, then swaps the entry.
- **`VtablePatch`:** swaps a vtable entry, and leaves alone an entry that already holds ours. When it restores, it puts
  the original back only if the entry still holds ours, and only once `Readable` shows the vtable is still there.

The tests patch the test program's own import of `GetCurrentProcessId`, and a fake COM object's vtable, and put both
back.

**Files:**
- Create: `src/hooks/ImportPatch.h`, `src/hooks/ImportPatch.cpp`, `src/hooks/VtablePatch.h`, `src/hooks/VtablePatch.cpp`,
  `tests/TestPatches.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestPatches.cpp`**

```cpp
#include "Harness.h"

#include "hooks/ImportPatch.h"
#include "hooks/VtablePatch.h"

#include <Windows.h>

namespace
{
DWORD WINAPI FakeProcessId()
{
	return 4242;
}

// Not inlined, so each call reads the import table afresh.
__declspec(noinline) DWORD ProcessIdThroughImport()
{
	return GetCurrentProcessId();
}

// A stand-in COM object: a pointer to a table of three functions.
int First(void*)
{
	return 1;
}

int Second(void*)
{
	return 2;
}

int Third(void*)
{
	return 3;
}

int Replacement(void*)
{
	return 20;
}

using Method = int (*)(void*);
Method g_table[] = { &First, &Second, &Third };

struct FakeObject
{
	Method* vtable = g_table;
};

int CallSlot(FakeObject& aObject, size_t aIndex)
{
	return aObject.vtable[aIndex](&aObject);
}
}

TEST(ImportPatchRedirectsAndRestoresAnImport)
{
	const DWORD real = ProcessIdThroughImport();
	auto        patch = cpmc::FindImport(GetModuleHandleW(nullptr), "kernel32.dll", "GetCurrentProcessId");
	CHECK(patch.has_value());
	CHECK(patch->original == reinterpret_cast<void*>(&GetCurrentProcessId));
	CHECK_EQ(ProcessIdThroughImport(), real);  // finding changes nothing
	CHECK(cpmc::ApplyImport(*patch, reinterpret_cast<void*>(&FakeProcessId)));
	CHECK_EQ(ProcessIdThroughImport(), DWORD(4242));
	CHECK(cpmc::RestoreImport(*patch));
	CHECK_EQ(ProcessIdThroughImport(), real);
	CHECK(!cpmc::RestoreImport(*patch));  // already restored
}

TEST(ImportPatchFindsNothingThatIsntImported)
{
	CHECK(!cpmc::FindImport(GetModuleHandleW(nullptr), "kernel32.dll", "NoSuchFunction"));
	CHECK(!cpmc::FindImport(GetModuleHandleW(nullptr), "nosuch.dll", "GetCurrentProcessId"));
}

TEST(VtablePatchSwapsOneEntryAndRestoresIt)
{
	FakeObject          object;
	cpmc::VtablePatcher patcher;
	void* original = patcher.Patch(&object, 1, reinterpret_cast<void*>(&Replacement));
	CHECK(original == reinterpret_cast<void*>(&Second));
	CHECK_EQ(CallSlot(object, 0), 1);
	CHECK_EQ(CallSlot(object, 1), 20);
	CHECK_EQ(CallSlot(object, 2), 3);

	// Patching the same entry again changes nothing and still hands back the true original.
	CHECK(patcher.Patch(&object, 1, reinterpret_cast<void*>(&Replacement)) == reinterpret_cast<void*>(&Second));
	CHECK_EQ(patcher.Count(), size_t(1));

	CHECK_EQ(patcher.RestoreAll(), size_t(1));
	CHECK_EQ(CallSlot(object, 1), 2);
	CHECK_EQ(patcher.Count(), size_t(0));
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestSl.cpp
)
```
```cmake
	tests/TestSl.cpp
	tests/TestPatches.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'hooks/ImportPatch.h'`.

- [x] **Step 4: Write `src/hooks/ImportPatch.h`**

```cpp
#pragma once

#include <Windows.h>

#include <optional>

namespace cpmc
{
// One import-table entry of a loaded module. Once applied, calls the module makes to that import reach the
// replacement; `original` is where they went before.
struct ImportPatch
{
	void** slot = nullptr;
	void*  original = nullptr;
	void*  replacement = nullptr;
};

// Finds aModule's import of aFunction (imported by name) from aDll, which is compared without regard to case, and
// what the entry holds, without changing it. Nothing if the module doesn't import it that way.
std::optional<ImportPatch> FindImport(HMODULE aModule, const char* aDll, const char* aFunction);

// Points the entry at aReplacement, with one atomic pointer write. Store aPatch.original where the replacement will
// look for it first, so no call can reach the replacement before it can call on.
bool ApplyImport(ImportPatch& aPatch, void* aReplacement);

// Puts the original back, if the entry still holds the replacement. False otherwise.
bool RestoreImport(const ImportPatch& aPatch);
}
```

- [x] **Step 5: Write `src/hooks/ImportPatch.cpp`**

```cpp
#include "hooks/ImportPatch.h"

#include <cstring>

namespace cpmc
{
namespace
{
// Writes one pointer-sized entry, making its page writable just for the write.
bool WriteEntry(void** aSlot, void* aValue)
{
	DWORD oldProtect = 0;
	if (!VirtualProtect(aSlot, sizeof(void*), PAGE_READWRITE, &oldProtect))
	{
		return false;
	}
	InterlockedExchangePointer(aSlot, aValue);
	DWORD unused = 0;
	VirtualProtect(aSlot, sizeof(void*), oldProtect, &unused);
	return true;
}
}

std::optional<ImportPatch> FindImport(HMODULE aModule, const char* aDll, const char* aFunction)
{
	if (!aModule || !aDll || !aFunction)
	{
		return std::nullopt;
	}
	auto*       base = reinterpret_cast<uint8_t*>(aModule);
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
	{
		return std::nullopt;
	}
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
	{
		return std::nullopt;
	}
	const IMAGE_DATA_DIRECTORY& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (directory.VirtualAddress == 0)
	{
		return std::nullopt;
	}
	for (auto* import = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress); import->Name != 0;
	     ++import)
	{
		if (_stricmp(reinterpret_cast<const char*>(base + import->Name), aDll) != 0 || import->OriginalFirstThunk == 0)
		{
			continue;
		}
		const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA64*>(base + import->OriginalFirstThunk);
		auto*       entries = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + import->FirstThunk);
		for (size_t i = 0; names[i].u1.AddressOfData != 0; ++i)
		{
			if (IMAGE_SNAP_BY_ORDINAL64(names[i].u1.Ordinal))
			{
				continue;
			}
			const auto* byName = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names[i].u1.AddressOfData);
			if (std::strcmp(reinterpret_cast<const char*>(byName->Name), aFunction) != 0)
			{
				continue;
			}
			ImportPatch patch;
			patch.slot = reinterpret_cast<void**>(&entries[i].u1.Function);
			patch.original = *patch.slot;
			return patch;
		}
	}
	return std::nullopt;
}

bool ApplyImport(ImportPatch& aPatch, void* aReplacement)
{
	if (!aPatch.slot || !aReplacement || !WriteEntry(aPatch.slot, aReplacement))
	{
		return false;
	}
	aPatch.replacement = aReplacement;
	return true;
}

bool RestoreImport(const ImportPatch& aPatch)
{
	if (!aPatch.slot || *aPatch.slot != aPatch.replacement)
	{
		return false;
	}
	return WriteEntry(aPatch.slot, aPatch.original);
}
}
```

- [x] **Step 6: Write `src/hooks/VtablePatch.h`**

```cpp
#pragma once

#include "Readable.h"

#include <cstddef>
#include <mutex>
#include <vector>

namespace cpmc
{
// Points entries of COM objects' vtables at replacements. Every object sharing the vtable then calls the
// replacement, which must call the original itself. Swapping one entry is a single atomic pointer write, so it's safe
// while other threads are calling through the vtable.
class VtablePatcher
{
public:
	// Patches entry aIndex of aObject's vtable. Returns the function the entry held before, which the replacement
	// must call: the original from the first patch if this entry already holds aReplacement (patching twice changes
	// nothing). Null if the entry couldn't be written.
	void* Patch(void* aObject, size_t aIndex, void* aReplacement);

	// Puts every original back where the entry still holds the replacement and its memory is still there. Returns how
	// many entries it restored.
	size_t RestoreAll();

	size_t Count() const;

private:
	struct Entry
	{
		void** slot;
		void*  original;
		void*  replacement;
	};

	mutable std::mutex mutex_;
	std::vector<Entry> entries_;
};
}
```

- [x] **Step 7: Write `src/hooks/VtablePatch.cpp`**

```cpp
#include "hooks/VtablePatch.h"

#include <Windows.h>

namespace cpmc
{
namespace
{
bool WriteEntry(void** aSlot, void* aValue)
{
	DWORD oldProtect = 0;
	if (!VirtualProtect(aSlot, sizeof(void*), PAGE_READWRITE, &oldProtect))
	{
		return false;
	}
	InterlockedExchangePointer(aSlot, aValue);
	DWORD unused = 0;
	VirtualProtect(aSlot, sizeof(void*), oldProtect, &unused);
	return true;
}
}

void* VtablePatcher::Patch(void* aObject, size_t aIndex, void* aReplacement)
{
	if (!aObject || !aReplacement)
	{
		return nullptr;
	}
	std::lock_guard lock(mutex_);
	void** vtable = *static_cast<void***>(aObject);
	void** slot = vtable + aIndex;
	for (const Entry& entry : entries_)
	{
		if (entry.slot == slot && *slot == aReplacement)
		{
			return entry.original;
		}
	}
	void* original = *slot;
	if (!WriteEntry(slot, aReplacement))
	{
		return nullptr;
	}
	entries_.push_back({ slot, original, aReplacement });
	return original;
}

size_t VtablePatcher::RestoreAll()
{
	std::lock_guard lock(mutex_);
	ReadableMemory  memory;
	size_t          restored = 0;
	for (const Entry& entry : entries_)
	{
		if (memory.Range(entry.slot, sizeof(void*)) && *entry.slot == entry.replacement && WriteEntry(entry.slot, entry.original))
		{
			++restored;
		}
	}
	entries_.clear();
	return restored;
}

size_t VtablePatcher::Count() const
{
	std::lock_guard lock(mutex_);
	return entries_.size();
}
}
```

- [x] **Step 8: Edit `CMakeLists.txt` to add the sources**

```cmake
	src/sl/Sl.cpp
)
```
```cmake
	src/sl/Sl.cpp
	src/hooks/ImportPatch.cpp
	src/hooks/VtablePatch.cpp
)
```

- [x] **Step 9: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `28 test(s), 0 failure(s)`.

- [x] **Step 10: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: import-table and vtable patching" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 8: The patched vtable slots

`Slots.h` lists the vtable slot of every method the hooks patch. `SlotsC.c` reads the same slots from the Windows
SDK's C interface layouts (the `...Vtbl` structs), and `TestSlots.cpp` compares the two.

**Files:**
- Create: `src/hooks/Slots.h`, `tests/SlotsC.c`, `tests/TestSlots.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write `tests/SlotsC.c`**

```c
/* Compiled as C, where the Windows SDK declares each COM interface as a struct of function pointers: the vtable slot
   of a method is its offset in that struct over the size of a pointer. tests/TestSlots.cpp compares these numbers with
   src/hooks/Slots.h. */
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_2.h>

#include <stddef.h>

#define SLOT(vtbl, method) (offsetof(vtbl, method) / sizeof(void*))

size_t SlotsC_Device(int aWhich)
{
	switch (aWhich)
	{
	case 0: return SLOT(ID3D12DeviceVtbl, CreateRenderTargetView);
	case 1: return SLOT(ID3D12DeviceVtbl, CreateDepthStencilView);
	case 2: return SLOT(ID3D12DeviceVtbl, CopyDescriptors);
	case 3: return SLOT(ID3D12DeviceVtbl, CopyDescriptorsSimple);
	default: return 0;
	}
}

size_t SlotsC_Queue(void)
{
	return SLOT(ID3D12CommandQueueVtbl, ExecuteCommandLists);
}

size_t SlotsC_List(int aWhich)
{
	switch (aWhich)
	{
	case 0: return SLOT(ID3D12GraphicsCommandList7Vtbl, Close);
	case 1: return SLOT(ID3D12GraphicsCommandList7Vtbl, Reset);
	case 2: return SLOT(ID3D12GraphicsCommandList7Vtbl, DrawInstanced);
	case 3: return SLOT(ID3D12GraphicsCommandList7Vtbl, DrawIndexedInstanced);
	case 4: return SLOT(ID3D12GraphicsCommandList7Vtbl, Dispatch);
	case 5: return SLOT(ID3D12GraphicsCommandList7Vtbl, ResourceBarrier);
	case 6: return SLOT(ID3D12GraphicsCommandList7Vtbl, ExecuteBundle);
	case 7: return SLOT(ID3D12GraphicsCommandList7Vtbl, OMSetRenderTargets);
	case 8: return SLOT(ID3D12GraphicsCommandList7Vtbl, ClearDepthStencilView);
	case 9: return SLOT(ID3D12GraphicsCommandList7Vtbl, ClearRenderTargetView);
	case 10: return SLOT(ID3D12GraphicsCommandList7Vtbl, SetMarker);
	case 11: return SLOT(ID3D12GraphicsCommandList7Vtbl, BeginEvent);
	case 12: return SLOT(ID3D12GraphicsCommandList7Vtbl, EndEvent);
	case 13: return SLOT(ID3D12GraphicsCommandList7Vtbl, ExecuteIndirect);
	case 14: return SLOT(ID3D12GraphicsCommandList7Vtbl, BeginRenderPass);
	case 15: return SLOT(ID3D12GraphicsCommandList7Vtbl, EndRenderPass);
	case 16: return SLOT(ID3D12GraphicsCommandList7Vtbl, Barrier);
	default: return 0;
	}
}

size_t SlotsC_Dxgi(int aWhich)
{
	switch (aWhich)
	{
	case 0: return SLOT(IDXGIFactory2Vtbl, CreateSwapChain);
	case 1: return SLOT(IDXGIFactory2Vtbl, CreateSwapChainForHwnd);
	case 2: return SLOT(IDXGISwapChain1Vtbl, Present);
	case 3: return SLOT(IDXGISwapChain1Vtbl, Present1);
	default: return 0;
	}
}
```

- [x] **Step 2: Write the failing test, `tests/TestSlots.cpp`**

```cpp
#include "Harness.h"

#include "hooks/Slots.h"

namespace slots = cpmc::slots;

extern "C"
{
size_t SlotsC_Device(int aWhich);
size_t SlotsC_Queue(void);
size_t SlotsC_List(int aWhich);
size_t SlotsC_Dxgi(int aWhich);
}

TEST(SlotsMatchTheSdksDeviceAndQueueLayouts)
{
	CHECK_EQ(SlotsC_Device(0), slots::kCreateRenderTargetView);
	CHECK_EQ(SlotsC_Device(1), slots::kCreateDepthStencilView);
	CHECK_EQ(SlotsC_Device(2), slots::kCopyDescriptors);
	CHECK_EQ(SlotsC_Device(3), slots::kCopyDescriptorsSimple);
	CHECK_EQ(SlotsC_Queue(), slots::kExecuteCommandLists);
}

TEST(SlotsMatchTheSdksCommandListLayout)
{
	const size_t expected[] = { slots::kClose,
		                        slots::kReset,
		                        slots::kDrawInstanced,
		                        slots::kDrawIndexedInstanced,
		                        slots::kDispatch,
		                        slots::kResourceBarrier,
		                        slots::kExecuteBundle,
		                        slots::kOMSetRenderTargets,
		                        slots::kClearDepthStencilView,
		                        slots::kClearRenderTargetView,
		                        slots::kSetMarker,
		                        slots::kBeginEvent,
		                        slots::kEndEvent,
		                        slots::kExecuteIndirect,
		                        slots::kBeginRenderPass,
		                        slots::kEndRenderPass,
		                        slots::kBarrier };
	for (int i = 0; i < int(std::size(expected)); ++i)
	{
		CHECK_EQ(SlotsC_List(i), expected[i]);
	}
}

TEST(SlotsMatchTheSdksDxgiLayouts)
{
	CHECK_EQ(SlotsC_Dxgi(0), slots::kCreateSwapChain);
	CHECK_EQ(SlotsC_Dxgi(1), slots::kCreateSwapChainForHwnd);
	CHECK_EQ(SlotsC_Dxgi(2), slots::kPresent);
	CHECK_EQ(SlotsC_Dxgi(3), slots::kPresent1);
}
```

- [x] **Step 3: Edit `CMakeLists.txt` to add both**

```cmake
	tests/TestPatches.cpp
)
```
```cmake
	tests/TestPatches.cpp
	tests/TestSlots.cpp
	tests/SlotsC.c
)
```

- [x] **Step 4: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'hooks/Slots.h'`.

- [x] **Step 5: Write `src/hooks/Slots.h`**

```cpp
#pragma once

#include <cstddef>

// The vtable slot of every method the plugin patches, counted from 0 (IUnknown::QueryInterface). The interfaces
// derive in a straight line, so each method keeps its slot in every later version of its interface.
// tests/SlotsC.c reads the same numbers from the Windows SDK's C layouts, and tests/TestSlots.cpp compares them.
namespace cpmc::slots
{
// ID3D12Device
constexpr size_t kCreateRenderTargetView = 20;
constexpr size_t kCreateDepthStencilView = 21;
constexpr size_t kCopyDescriptors = 23;
constexpr size_t kCopyDescriptorsSimple = 24;

// ID3D12CommandQueue
constexpr size_t kExecuteCommandLists = 10;

// ID3D12GraphicsCommandList, then ...List4 and ...List7
constexpr size_t kClose = 9;
constexpr size_t kReset = 10;
constexpr size_t kDrawInstanced = 12;
constexpr size_t kDrawIndexedInstanced = 13;
constexpr size_t kDispatch = 14;
constexpr size_t kResourceBarrier = 26;
constexpr size_t kExecuteBundle = 27;
constexpr size_t kOMSetRenderTargets = 46;
constexpr size_t kClearDepthStencilView = 47;
constexpr size_t kClearRenderTargetView = 48;
constexpr size_t kSetMarker = 56;
constexpr size_t kBeginEvent = 57;
constexpr size_t kEndEvent = 58;
constexpr size_t kExecuteIndirect = 59;
constexpr size_t kBeginRenderPass = 68;
constexpr size_t kEndRenderPass = 69;
constexpr size_t kBarrier = 80;

// IDXGIFactory and IDXGIFactory2
constexpr size_t kCreateSwapChain = 10;
constexpr size_t kCreateSwapChainForHwnd = 15;

// IDXGISwapChain and IDXGISwapChain1
constexpr size_t kPresent = 8;
constexpr size_t kPresent1 = 22;
}
```

- [x] **Step 6: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `31 test(s), 0 failure(s)`.

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: the patched vtable slots, checked against the SDK's C layouts" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 9: The view registry

The registry maps RTV and DSV descriptor handles to the resource and view they were made with:
- it follows `CreateRenderTargetView`, `CreateDepthStencilView`, and copies of RTV and DSV descriptors, singly and in
  ranges;
- copying from a handle it doesn't know erases the destination, so a stale entry never survives a copy.

**Files:**
- Create: `src/track/ViewRegistry.h`, `src/track/ViewRegistry.cpp`, `tests/TestViewRegistry.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestViewRegistry.cpp`**

```cpp
#include "Harness.h"

#include "track/ViewRegistry.h"

using cpmc::CopyRuns;
using cpmc::ViewInfo;
using cpmc::ViewRegistry;

namespace
{
ViewInfo Rtv(uintptr_t aResource, uint32_t aFormat)
{
	ViewInfo view;
	view.kind = ViewInfo::Kind::Rtv;
	view.resource = reinterpret_cast<void*>(aResource);
	view.viewFormat = aFormat;
	return view;
}
}

TEST(ViewRegistryRemembersViews)
{
	ViewRegistry registry;
	registry.SetIncrements(32, 8);
	registry.Record(1000, Rtv(0xA, 24));
	ViewInfo depth;
	depth.kind = ViewInfo::Kind::Dsv;
	depth.resource = reinterpret_cast<void*>(0xB);
	registry.Record(5000, depth);
	CHECK(registry.Find(1000)->resource == reinterpret_cast<void*>(0xA));
	CHECK_EQ(registry.Find(1000)->viewFormat, 24u);
	CHECK(registry.Find(5000)->kind == ViewInfo::Kind::Dsv);
	CHECK(!registry.Find(1032));
	CHECK_EQ(registry.Count(ViewInfo::Kind::Rtv), size_t(1));
	CHECK_EQ(registry.Count(ViewInfo::Kind::Dsv), size_t(1));
	CHECK_EQ(registry.Increment(ViewInfo::Kind::Dsv), 8u);
}

TEST(ViewRegistryFollowsCopiedDescriptors)
{
	ViewRegistry registry;
	registry.SetIncrements(32, 8);
	registry.Record(1000, Rtv(0xA, 24));
	registry.Record(1032, Rtv(0xB, 10));
	registry.Record(2064, Rtv(0xC, 2));  // the source at 1064 has no known view, so this one goes
	registry.Copy(ViewInfo::Kind::Rtv, 2000, 1000, 3);
	CHECK(registry.Find(2000)->resource == reinterpret_cast<void*>(0xA));
	CHECK(registry.Find(2032)->resource == reinterpret_cast<void*>(0xB));
	CHECK(!registry.Find(2064));
	CHECK(registry.Find(1000).has_value());  // the source keeps its view
}

TEST(ViewRegistryCopiesOverlappingRanges)
{
	ViewRegistry registry;
	registry.SetIncrements(32, 8);
	registry.Record(1000, Rtv(0xA, 1));
	registry.Record(1032, Rtv(0xB, 1));
	registry.Copy(ViewInfo::Kind::Rtv, 1032, 1000, 2);
	CHECK(registry.Find(1032)->resource == reinterpret_cast<void*>(0xA));
	CHECK(registry.Find(1064)->resource == reinterpret_cast<void*>(0xB));
}

TEST(CopyRunsLinesUpUnevenRanges)
{
	// Destinations: 3 at 100, 1 at 500. Sources: 1 at 900, 3 at 1300. Step 10.
	const uint64_t destStarts[] = { 100, 500 };
	const uint32_t destSizes[] = { 3, 1 };
	const uint64_t sourceStarts[] = { 900, 1300 };
	const uint32_t sourceSizes[] = { 1, 3 };
	const auto     runs = CopyRuns(2, destStarts, destSizes, 2, sourceStarts, sourceSizes, 10);
	CHECK_EQ(runs.size(), size_t(3));
	CHECK_EQ(runs[0].dest, uint64_t(100));
	CHECK_EQ(runs[0].source, uint64_t(900));
	CHECK_EQ(runs[0].count, 1u);
	CHECK_EQ(runs[1].dest, uint64_t(110));
	CHECK_EQ(runs[1].source, uint64_t(1300));
	CHECK_EQ(runs[1].count, 2u);
	CHECK_EQ(runs[2].dest, uint64_t(500));
	CHECK_EQ(runs[2].source, uint64_t(1320));
	CHECK_EQ(runs[2].count, 1u);
}

TEST(CopyRunsTakesNullSizesAsOnes)
{
	const uint64_t destStarts[] = { 100, 200 };
	const uint64_t sourceStarts[] = { 300, 400 };
	const auto     runs = CopyRuns(2, destStarts, nullptr, 2, sourceStarts, nullptr, 10);
	CHECK_EQ(runs.size(), size_t(2));
	CHECK_EQ(runs[1].dest, uint64_t(200));
	CHECK_EQ(runs[1].source, uint64_t(400));
	CHECK(CopyRuns(1, nullptr, nullptr, 1, sourceStarts, nullptr, 10).empty());
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/SlotsC.c
)
```
```cmake
	tests/SlotsC.c
	tests/TestViewRegistry.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'track/ViewRegistry.h'`.

- [x] **Step 4: Write `src/track/ViewRegistry.h`**

```cpp
#pragma once

#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace cpmc
{
// What a render-target or depth-stencil view was made from.
struct ViewInfo
{
	enum class Kind : uint8_t
	{
		Rtv,
		Dsv,
	};

	Kind     kind = Kind::Rtv;
	void*    resource = nullptr;  // the ID3D12Resource; not referenced
	uint32_t viewFormat = 0;      // the view's DXGI_FORMAT, or the resource's own when the view gave none
	uint32_t resourceFormat = 0;
	uint64_t width = 0;  // the resource's size
	uint32_t height = 0;
	uint32_t resourceFlags = 0;  // D3D12_RESOURCE_FLAGS
	uint32_t mip = 0;
	uint32_t plane = 0;
	uint32_t firstSlice = 0;
	bool     readOnlyDepth = false;
	bool     readOnlyStencil = false;
};

// aCount descriptors copied from aSource onwards to aDest onwards.
struct DescriptorRun
{
	uint64_t dest = 0;
	uint64_t source = 0;
	uint32_t count = 0;
};

// Splits one CopyDescriptors call into runs that are contiguous on both sides. Destination and source ranges needn't
// line up; null sizes mean ranges of one descriptor each, as in D3D12. aIncrement is the heap type's handle step.
std::vector<DescriptorRun> CopyRuns(uint32_t aDestRanges, const uint64_t* aDestStarts, const uint32_t* aDestSizes,
                                    uint32_t aSourceRanges, const uint64_t* aSourceStarts, const uint32_t* aSourceSizes,
                                    uint32_t aIncrement);

// RTV and DSV descriptor handles (D3D12_CPU_DESCRIPTOR_HANDLE::ptr) mapped to what their views were made from, as the
// device creates and copies them. Thread-safe.
class ViewRegistry
{
public:
	void     SetIncrements(uint32_t aRtv, uint32_t aDsv);
	uint32_t Increment(ViewInfo::Kind aKind) const;

	void Record(uint64_t aHandle, const ViewInfo& aView);

	// Copies the views of aCount descriptors from aSource onwards to aDest onwards. A source with no known view
	// leaves its destination unknown too.
	void Copy(ViewInfo::Kind aKind, uint64_t aDest, uint64_t aSource, uint32_t aCount);

	std::optional<ViewInfo> Find(uint64_t aHandle) const;
	size_t                  Count(ViewInfo::Kind aKind) const;

private:
	mutable std::shared_mutex              mutex_;
	std::unordered_map<uint64_t, ViewInfo> views_;
	uint32_t                               rtvIncrement_ = 0;
	uint32_t                               dsvIncrement_ = 0;
};
}
```

- [x] **Step 5: Write `src/track/ViewRegistry.cpp`**

```cpp
#include "track/ViewRegistry.h"

#include <algorithm>
#include <mutex>

namespace cpmc
{
std::vector<DescriptorRun> CopyRuns(uint32_t aDestRanges, const uint64_t* aDestStarts, const uint32_t* aDestSizes,
                                    uint32_t aSourceRanges, const uint64_t* aSourceStarts, const uint32_t* aSourceSizes,
                                    uint32_t aIncrement)
{
	std::vector<DescriptorRun> runs;
	if (!aDestStarts || !aSourceStarts)
	{
		return runs;
	}
	uint32_t destRange = 0, destOffset = 0;
	uint32_t sourceRange = 0, sourceOffset = 0;
	while (destRange < aDestRanges && sourceRange < aSourceRanges)
	{
		const uint32_t destSize = aDestSizes ? aDestSizes[destRange] : 1;
		const uint32_t sourceSize = aSourceSizes ? aSourceSizes[sourceRange] : 1;
		if (destOffset >= destSize)
		{
			++destRange;
			destOffset = 0;
			continue;
		}
		if (sourceOffset >= sourceSize)
		{
			++sourceRange;
			sourceOffset = 0;
			continue;
		}
		const uint32_t count = std::min(destSize - destOffset, sourceSize - sourceOffset);
		runs.push_back({ aDestStarts[destRange] + uint64_t(destOffset) * aIncrement,
		                 aSourceStarts[sourceRange] + uint64_t(sourceOffset) * aIncrement, count });
		destOffset += count;
		sourceOffset += count;
	}
	return runs;
}

void ViewRegistry::SetIncrements(uint32_t aRtv, uint32_t aDsv)
{
	std::unique_lock lock(mutex_);
	rtvIncrement_ = aRtv;
	dsvIncrement_ = aDsv;
}

uint32_t ViewRegistry::Increment(ViewInfo::Kind aKind) const
{
	std::shared_lock lock(mutex_);
	return aKind == ViewInfo::Kind::Rtv ? rtvIncrement_ : dsvIncrement_;
}

void ViewRegistry::Record(uint64_t aHandle, const ViewInfo& aView)
{
	std::unique_lock lock(mutex_);
	views_[aHandle] = aView;
}

void ViewRegistry::Copy(ViewInfo::Kind aKind, uint64_t aDest, uint64_t aSource, uint32_t aCount)
{
	std::unique_lock lock(mutex_);
	const uint64_t   step = aKind == ViewInfo::Kind::Rtv ? rtvIncrement_ : dsvIncrement_;
	if (step == 0)
	{
		return;
	}
	// Read every source first: the ranges may overlap.
	std::vector<std::optional<ViewInfo>> copied(aCount);
	for (uint32_t i = 0; i < aCount; ++i)
	{
		const auto found = views_.find(aSource + i * step);
		if (found != views_.end() && found->second.kind == aKind)
		{
			copied[i] = found->second;
		}
	}
	for (uint32_t i = 0; i < aCount; ++i)
	{
		if (copied[i])
		{
			views_[aDest + i * step] = *copied[i];
		}
		else
		{
			views_.erase(aDest + i * step);
		}
	}
}

std::optional<ViewInfo> ViewRegistry::Find(uint64_t aHandle) const
{
	std::shared_lock lock(mutex_);
	const auto       found = views_.find(aHandle);
	if (found == views_.end())
	{
		return std::nullopt;
	}
	return found->second;
}

size_t ViewRegistry::Count(ViewInfo::Kind aKind) const
{
	std::shared_lock lock(mutex_);
	return size_t(std::count_if(views_.begin(), views_.end(), [aKind](const auto& aEntry) { return aEntry.second.kind == aKind; }));
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/hooks/VtablePatch.cpp
)
```
```cmake
	src/hooks/VtablePatch.cpp
	src/track/ViewRegistry.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `36 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: view registry" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" \
  && git log --oneline -1
```

---

### Task 10: The state tracker

The tracker follows each command list from `Reset` to `Close`, and keeps:
- **for the frame log:** the list's passes, each with its render targets, counts and event names;
- **for the dumps:** each resource's state after the last barrier in the list. A split barrier, or a barrier on one
  subresource, makes that state unknown.

`IsGbufferCandidate` picks the passes that might be the G-buffer: three or more known targets the size of the tagged
depth, with the tagged depth bound.

Part 1 doesn't need the restore the spec describes. It comes with step 3, when blocks are drawn into the game's own
lists.

**Files:**
- Create: `src/track/StateTracker.h`, `src/track/StateTracker.cpp`, `tests/TestStateTracker.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestStateTracker.cpp`**

```cpp
#include "Harness.h"

#include "track/StateTracker.h"

#include <thread>

using cpmc::BoundTarget;
using cpmc::ListState;
using cpmc::StateTracker;
using cpmc::Targets;
using cpmc::ViewInfo;

namespace
{
BoundTarget Target(uintptr_t aResource, uint64_t aWidth, uint32_t aHeight, ViewInfo::Kind aKind = ViewInfo::Kind::Rtv)
{
	ViewInfo view;
	view.kind = aKind;
	view.resource = reinterpret_cast<void*>(aResource);
	view.width = aWidth;
	view.height = aHeight;
	return { aResource * 100, view };
}

Targets Gbuffer(uintptr_t aDepth)
{
	Targets targets;
	targets.rtvs = { Target(1, 1707, 960), Target(2, 1707, 960), Target(3, 1707, 960) };
	targets.dsv = Target(aDepth, 1707, 960, ViewInfo::Kind::Dsv);
	return targets;
}
}

TEST(StateTrackerFindsOnlyListsItSawReset)
{
	StateTracker tracker;
	int          listA = 0, listB = 0;
	CHECK(tracker.Find(&listA) == nullptr);
	ListState& a = tracker.ForReset(&listA);
	a.Begin(7, false);
	CHECK(tracker.Find(&listA) == &a);
	CHECK(tracker.Find(&listB) == nullptr);
	CHECK(tracker.Find(&listA)->Recording());
	CHECK_EQ(tracker.Find(&listA)->Frame(), 7u);
	a.End();
	CHECK(!a.Recording());
	CHECK(&tracker.ForReset(&listA) == &a);  // reused
}

TEST(StateTrackerFindsListsFromOtherThreads)
{
	StateTracker tracker;
	int          list = 0;
	tracker.ForReset(&list).Begin(1, false);
	ListState* seen = nullptr;
	std::thread other([&] { seen = tracker.Find(&list); });
	other.join();
	CHECK(seen == tracker.Find(&list));
}

TEST(ListStateRecordsPassesOnlyWhileLogging)
{
	ListState quiet;
	quiet.Begin(1, false);
	quiet.SetTargets(Gbuffer(9));
	quiet.Count(ListState::Counter::Draw);
	CHECK(quiet.Passes().empty());
	CHECK_EQ(quiet.CurrentTargets().rtvs.size(), size_t(3));  // targets are kept regardless

	ListState logged;
	logged.Begin(1, true);
	logged.Count(ListState::Counter::Dispatch);  // before any targets: a pass of its own
	logged.SetTargets(Gbuffer(9));
	logged.Count(ListState::Counter::IndexedDraw);
	logged.Count(ListState::Counter::IndexedDraw);
	logged.Count(ListState::Counter::Clear);
	logged.Event("GBuffer");
	logged.SetTargets({}, true);
	logged.Count(ListState::Counter::Indirect);
	CHECK_EQ(logged.Passes().size(), size_t(3));
	CHECK_EQ(logged.Passes()[0].dispatches, 1u);
	CHECK_EQ(logged.Passes()[1].indexedDraws, 2u);
	CHECK_EQ(logged.Passes()[1].clears, 1u);
	CHECK_STR(logged.Passes()[1].events[0], "GBuffer");
	CHECK(logged.Passes()[2].renderPass);
	CHECK_EQ(logged.Passes()[2].indirect, 1u);
}

TEST(ListStateFollowsTransitionsAndSplitBarriers)
{
	ListState state;
	state.Begin(1, false);
	int resource = 0;
	CHECK(!state.StateOf(&resource));
	state.Transition(&resource, 0x4, false);  // render target
	CHECK_EQ(*state.StateOf(&resource), 0x4u);
	state.Transition(&resource, 0x800, true);  // begun, not ended
	CHECK(!state.StateOf(&resource));
	state.Transition(&resource, 0x800, false);  // ended
	CHECK_EQ(*state.StateOf(&resource), 0x800u);
	state.Begin(2, false);
	CHECK(!state.StateOf(&resource));  // a new recording forgets
}

TEST(GbufferCandidatesNeedTheTaggedDepthAndMatchingTargets)
{
	CHECK(cpmc::IsGbufferCandidate(Gbuffer(9), reinterpret_cast<void*>(9)));
	CHECK(!cpmc::IsGbufferCandidate(Gbuffer(9), reinterpret_cast<void*>(8)));  // another depth buffer (a mirror)
	CHECK(!cpmc::IsGbufferCandidate(Gbuffer(9), nullptr));

	Targets two = Gbuffer(9);
	two.rtvs.pop_back();
	CHECK(!cpmc::IsGbufferCandidate(two, reinterpret_cast<void*>(9)));

	Targets small = Gbuffer(9);
	small.rtvs[1] = Target(2, 853, 480);
	CHECK(!cpmc::IsGbufferCandidate(small, reinterpret_cast<void*>(9)));

	Targets unknown = Gbuffer(9);
	unknown.rtvs[2].view.reset();
	CHECK(!cpmc::IsGbufferCandidate(unknown, reinterpret_cast<void*>(9)));
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestViewRegistry.cpp
)
```
```cmake
	tests/TestViewRegistry.cpp
	tests/TestStateTracker.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'track/StateTracker.h'`.

- [x] **Step 4: Write `src/track/StateTracker.h`**

```cpp
#pragma once

#include "track/ViewRegistry.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace cpmc
{
// One bound render target or depth buffer: its descriptor handle, and the view the registry knew for it then.
struct BoundTarget
{
	uint64_t                handle = 0;
	std::optional<ViewInfo> view;
};

struct Targets
{
	std::vector<BoundTarget>   rtvs;
	std::optional<BoundTarget> dsv;
};

// What a command list did between two changes of render targets, recorded while a frame log runs.
struct PassRecord
{
	Targets                  targets;
	bool                     renderPass = false;  // begun with BeginRenderPass
	uint32_t                 draws = 0;           // DrawInstanced
	uint32_t                 indexedDraws = 0;    // DrawIndexedInstanced
	uint32_t                 indirect = 0;        // ExecuteIndirect
	uint32_t                 bundles = 0;         // ExecuteBundle
	uint32_t                 dispatches = 0;
	uint32_t                 clears = 0;
	uint32_t                 barriers = 0;          // ResourceBarrier calls
	uint32_t                 enhancedBarriers = 0;  // Barrier calls
	std::vector<std::string> events;              // event and marker names, at most 16
};

// One command list's state from its Reset to its Close. Only the thread recording the list touches it.
class ListState
{
public:
	enum class Counter
	{
		Draw,
		IndexedDraw,
		Indirect,
		Bundle,
		Dispatch,
		Clear,
		Barrier,
		EnhancedBarrier,
	};

	static constexpr uint32_t kUnknownState = 0xFFFFFFFF;

	void Begin(uint32_t aFrame, bool aLogging);
	void End();
	bool Recording() const { return recording_; }
	bool Logging() const { return logging_; }
	uint32_t Frame() const { return frame_; }

	void SetTargets(Targets aTargets, bool aRenderPass = false);
	const Targets& CurrentTargets() const { return targets_; }

	void Count(Counter aCounter);
	void Event(std::string aName);

	// A transition barrier. A begin-only half leaves the state unknown until its end-only half.
	void Transition(void* aResource, uint32_t aAfter, bool aBeginOnly);
	std::optional<uint32_t> StateOf(void* aResource) const;

	const std::vector<PassRecord>& Passes() const { return passes_; }

private:
	PassRecord* CurrentPass();

	bool                                   recording_ = false;
	bool                                   logging_ = false;
	uint32_t                               frame_ = 0;
	Targets                                targets_;
	std::unordered_map<void*, uint32_t>    states_;
	std::vector<PassRecord>                passes_;
};

// Every command list's state, found by the list's address. A list's state lives as long as the tracker, so a pointer
// to it stays valid; a new list at an old list's address reuses it from its first Reset.
class StateTracker
{
public:
	StateTracker();

	ListState& ForReset(void* aList);  // creates the state on a list's first Reset
	ListState* Find(void* aList);      // null for a list never reset since tracking began

private:
	uint64_t                                              id_;  // tells trackers apart in each thread's cache
	std::shared_mutex                                     mutex_;
	std::unordered_map<void*, std::unique_ptr<ListState>> lists_;
};

// True when aTargets look like a G-buffer: three or more known render targets the size of the tagged depth, with
// the tagged depth bound as the depth buffer.
bool IsGbufferCandidate(const Targets& aTargets, const void* aTaggedDepth);
}
```

- [x] **Step 5: Write `src/track/StateTracker.cpp`**

```cpp
#include "track/StateTracker.h"

#include <atomic>
#include <mutex>

namespace cpmc
{
namespace
{
constexpr size_t kMaxEvents = 16;

// The last list each thread looked up. A thread records one list at a time, so this saves taking the lock on almost
// every call.
struct Cached
{
	uint64_t   tracker = 0;
	void*      list = nullptr;
	ListState* state = nullptr;
};
thread_local Cached t_cached;

std::atomic<uint64_t> g_nextTrackerId{ 1 };
}

void ListState::Begin(uint32_t aFrame, bool aLogging)
{
	recording_ = true;
	logging_ = aLogging;
	frame_ = aFrame;
	targets_ = {};
	states_.clear();
	passes_.clear();
}

void ListState::End()
{
	recording_ = false;
}

PassRecord* ListState::CurrentPass()
{
	if (!logging_)
	{
		return nullptr;
	}
	if (passes_.empty())
	{
		passes_.emplace_back();  // work before the first render-target change
	}
	return &passes_.back();
}

void ListState::SetTargets(Targets aTargets, bool aRenderPass)
{
	targets_ = std::move(aTargets);
	if (logging_)
	{
		PassRecord pass;
		pass.targets = targets_;
		pass.renderPass = aRenderPass;
		passes_.push_back(std::move(pass));
	}
}

void ListState::Count(Counter aCounter)
{
	PassRecord* pass = CurrentPass();
	if (!pass)
	{
		return;
	}
	switch (aCounter)
	{
	case Counter::Draw: ++pass->draws; break;
	case Counter::IndexedDraw: ++pass->indexedDraws; break;
	case Counter::Indirect: ++pass->indirect; break;
	case Counter::Bundle: ++pass->bundles; break;
	case Counter::Dispatch: ++pass->dispatches; break;
	case Counter::Clear: ++pass->clears; break;
	case Counter::Barrier: ++pass->barriers; break;
	case Counter::EnhancedBarrier: ++pass->enhancedBarriers; break;
	}
}

void ListState::Event(std::string aName)
{
	PassRecord* pass = CurrentPass();
	if (pass && pass->events.size() < kMaxEvents)
	{
		pass->events.push_back(std::move(aName));
	}
}

void ListState::Transition(void* aResource, uint32_t aAfter, bool aBeginOnly)
{
	states_[aResource] = aBeginOnly ? kUnknownState : aAfter;
}

std::optional<uint32_t> ListState::StateOf(void* aResource) const
{
	const auto found = states_.find(aResource);
	if (found == states_.end() || found->second == kUnknownState)
	{
		return std::nullopt;
	}
	return found->second;
}

StateTracker::StateTracker()
	: id_(g_nextTrackerId++)
{
}

ListState& StateTracker::ForReset(void* aList)
{
	std::unique_lock lock(mutex_);
	auto&            state = lists_[aList];
	if (!state)
	{
		state = std::make_unique<ListState>();
	}
	t_cached = { id_, aList, state.get() };
	return *state;
}

ListState* StateTracker::Find(void* aList)
{
	if (t_cached.tracker == id_ && t_cached.list == aList)
	{
		return t_cached.state;
	}
	std::shared_lock lock(mutex_);
	const auto       found = lists_.find(aList);
	if (found == lists_.end())
	{
		return nullptr;
	}
	t_cached = { id_, aList, found->second.get() };
	return found->second.get();
}

bool IsGbufferCandidate(const Targets& aTargets, const void* aTaggedDepth)
{
	if (!aTaggedDepth || !aTargets.dsv || !aTargets.dsv->view || aTargets.dsv->view->resource != aTaggedDepth ||
	    aTargets.rtvs.size() < 3)
	{
		return false;
	}
	const ViewInfo& depth = *aTargets.dsv->view;
	for (const BoundTarget& target : aTargets.rtvs)
	{
		if (!target.view || target.view->width != depth.width || target.view->height != depth.height)
		{
			return false;
		}
	}
	return true;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/track/ViewRegistry.cpp
)
```
```cmake
	src/track/ViewRegistry.cpp
	src/track/StateTracker.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `41 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: state tracker" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" \
  && git log --oneline -1
```

---

### Task 11: The camera

`MakeCameraFrame` turns Streamline's constants into one matrix, from camera-relative world positions to clip space,
and it reads the layout of `cameraViewToClip` from the matrix itself:
- whether it's stored for row or column vectors;
- which view axis is forward, right and up.

So the camera works whichever convention the game turns out to use. The view part comes from the camera's right, up
and forward vectors. A render origin, if step 1 finds one, is added to `cameraPos`. `AddJitter` and `DepthTexelFor`
serve step 3 and the present pass's depth test.

**Files:**
- Create: `src/render/Camera.h`, `src/render/Camera.cpp`, `tests/TestCamera.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestCamera.cpp`**

```cpp
#include "Harness.h"

#include "render/Camera.h"

#include <cmath>

using cpmc::CameraFrame;
using cpmc::Mat4;
using cpmc::SlConstants;

namespace
{
constexpr double kXs = 0.8;   // 1 / (tan(fov / 2) * aspect)
constexpr double kYs = 1.4;   // 1 / tan(fov / 2)
constexpr double kNear = 0.1;

// A camera at (100, 200, 10) in the world, looking north (+y), with +z up and +x to its right.
SlConstants Camera(const std::array<float, 16>& aViewToClip)
{
	SlConstants constants;
	constants.viewToClip = aViewToClip;
	constants.position = { 100.0f, 200.0f, 10.0f };
	constants.forward = { 0.0f, 1.0f, 0.0f };
	constants.right = { 1.0f, 0.0f, 0.0f };
	constants.up = { 0.0f, 0.0f, 1.0f };
	constants.depthInverted = 1;
	return constants;
}

// D3D's usual left-handed view space (x right, y up, z forward), reversed infinite depth, row vectors.
std::array<float, 16> LeftHanded()
{
	return { float(kXs), 0, 0, 0, 0, float(kYs), 0, 0, 0, 0, 0, 1, 0, 0, float(kNear), 0 };
}

// A right-handed view space (z backward) stored for column vectors, as OpenGL-style code writes it.
std::array<float, 16> RightHandedColumns()
{
	return { float(kXs), 0, 0, 0, 0, float(kYs), 0, 0, 0, 0, 0, float(kNear), 0, 0, -1, 0 };
}

// A Z-up view space (x right, y forward, z up), row vectors.
std::array<float, 16> ZUp()
{
	return { float(kXs), 0, 0, 0, 0, 0, 0, 1, 0, float(kYs), 0, 0, 0, 0, float(kNear), 0 };
}

// Where a world point lands in normalised device coordinates, plus its depth.
std::array<double, 3> Ndc(const CameraFrame& aFrame, double aX, double aY, double aZ)
{
	const auto clip = cpmc::Transform({ aX - aFrame.origin.x, aY - aFrame.origin.y, aZ - aFrame.origin.z, 1.0 }, aFrame.cameraToClip);
	return { clip[0] / clip[3], clip[1] / clip[3], clip[2] / clip[3] };
}

void ChecksOut(const std::array<float, 16>& aViewToClip)
{
	const auto frame = cpmc::MakeCameraFrame(Camera(aViewToClip), {});
	CHECK(frame.has_value());
	if (!frame)
	{
		return;
	}
	// 10 m ahead: the centre of the screen, depth near/10 (reversed).
	const auto ahead = Ndc(*frame, 100.0, 210.0, 10.0);
	CHECK_NEAR(ahead[0], 0.0, 1e-9);
	CHECK_NEAR(ahead[1], 0.0, 1e-9);
	// 10 m ahead, 1 m right and 2 m up.
	const auto offset = Ndc(*frame, 101.0, 210.0, 12.0);
	CHECK_NEAR(offset[0], kXs * 1.0 / 10.0, 1e-6);
	CHECK_NEAR(offset[1], kYs * 2.0 / 10.0, 1e-6);
	CHECK(frame->reversedDepth);
}
}

TEST(CameraHandlesALeftHandedRowVectorProjection)
{
	const auto layout = cpmc::FindProjectionLayout(LeftHanded());
	CHECK(layout.has_value());
	CHECK(!layout->columnVectors);
	CHECK_EQ(layout->forwardAxis, 2);
	CHECK_EQ(layout->forwardSign, 1.0);
	ChecksOut(LeftHanded());
	const auto frame = cpmc::MakeCameraFrame(Camera(LeftHanded()), {});
	CHECK_NEAR(Ndc(*frame, 100.0, 210.0, 10.0)[2], kNear / 10.0, 1e-9);
}

TEST(CameraHandlesARightHandedColumnVectorProjection)
{
	const auto layout = cpmc::FindProjectionLayout(RightHandedColumns());
	CHECK(layout.has_value());
	CHECK(layout->columnVectors);
	CHECK_EQ(layout->forwardAxis, 2);
	CHECK_EQ(layout->forwardSign, -1.0);
	ChecksOut(RightHandedColumns());
}

TEST(CameraHandlesAZUpViewSpace)
{
	const auto layout = cpmc::FindProjectionLayout(ZUp());
	CHECK(layout.has_value());
	CHECK_EQ(layout->forwardAxis, 1);
	CHECK_EQ(layout->upAxis, 2);
	ChecksOut(ZUp());
}

TEST(CameraRefusesWhatIsntAPerspective)
{
	const std::array<float, 16> identity = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	CHECK(!cpmc::FindProjectionLayout(identity));
	SlConstants noAxes = Camera(LeftHanded());
	noAxes.forward = { 0.0f, 0.0f, 0.0f };
	CHECK(!cpmc::MakeCameraFrame(noAxes, {}));
}

TEST(CameraAddsTheRenderOrigin)
{
	const auto frame = cpmc::MakeCameraFrame(Camera(LeftHanded()), { 0.0, 1024.0, 0.0 });
	CHECK_NEAR(frame->origin.y, 1224.0, 1e-9);
	// A point 10 m ahead of the camera's true world position is still the screen's centre.
	CHECK_NEAR(Ndc(*frame, 100.0, 1234.0, 10.0)[0], 0.0, 1e-9);
}

TEST(CameraJitterMovesAPointByThatManyPixels)
{
	const auto frame = cpmc::MakeCameraFrame(Camera(LeftHanded()), {});
	for (const double sign : { 1.0, -1.0 })
	{
		CameraFrame jittered = *frame;
		jittered.cameraToClip = cpmc::AddJitter(frame->cameraToClip, 0.5, 0.25, 1000, 500, sign);
		const auto ndc = Ndc(jittered, 100.0, 210.0, 10.0);
		// NDC spans 2 units over the image, so a pixel is 2 / 1000 across and 2 / 500 down (y runs up).
		CHECK_NEAR(ndc[0] * 1000.0 / 2.0, sign * 0.5, 1e-9);
		CHECK_NEAR(-ndc[1] * 500.0 / 2.0, sign * 0.25, 1e-9);
	}
}

TEST(CameraMapsOutputPixelsIntoTheDepthExtent)
{
	const sl::Extent whole{ 0, 0, 1707, 960 };
	CHECK(cpmc::DepthTexelFor(0, 0, 2560, 1440, whole) == std::make_pair(0u, 0u));
	CHECK(cpmc::DepthTexelFor(2559, 1439, 2560, 1440, whole) == std::make_pair(1706u, 959u));
	CHECK(cpmc::DepthTexelFor(1280, 720, 2560, 1440, whole) == std::make_pair(853u, 480u));
	const sl::Extent part{ 10, 20, 100, 50 };  // top, left, width, height
	CHECK(cpmc::DepthTexelFor(0, 0, 200, 100, part) == std::make_pair(20u, 10u));
	CHECK(cpmc::DepthTexelFor(199, 99, 200, 100, part) == std::make_pair(119u, 59u));
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestStateTracker.cpp
)
```
```cmake
	tests/TestStateTracker.cpp
	tests/TestCamera.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'render/Camera.h'`.

- [x] **Step 4: Write `src/render/Camera.h`**

```cpp
#pragma once

#include "Coords.h"
#include "sl/Sl.h"

#include <array>
#include <cstdint>
#include <optional>
#include <utility>

namespace cpmc
{
// A 4x4 matrix used with row vectors (p' = p * M), stored row by row, as Streamline and HLSL's mul(vector, matrix)
// use them.
using Mat4 = std::array<double, 16>;

Mat4                  Multiply(const Mat4& aFirst, const Mat4& aThen);  // p * aFirst * aThen
std::array<double, 4> Transform(const std::array<double, 4>& aPoint, const Mat4& aMatrix);

// How cameraViewToClip maps the game's view space to clip space, read from the matrix itself: whether it's stored for
// column vectors (and so needs transposing), and which view axis (0, 1 or 2) points forward, right and up, with which
// sign. Clip space is D3D's: x right, y up, w the distance in front.
struct ProjectionLayout
{
	bool   columnVectors = false;
	int    forwardAxis = 2;
	double forwardSign = 1.0;
	int    rightAxis = 0;
	double rightSign = 1.0;
	int    upAxis = 1;
	double upSign = 1.0;
};

// Nothing if aViewToClip isn't a perspective projection: one view axis must feed clip w with a factor of +-1, and
// clip w must not depend on anything else.
std::optional<ProjectionLayout> FindProjectionLayout(const std::array<float, 16>& aViewToClip);

// One frame's camera, ready for drawing.
struct CameraFrame
{
	Mat4  cameraToClip{};  // a camera-relative world position (metres, world axes) to clip space
	Vec3d origin;          // the camera's world position: cameraPos plus the render origin
	bool  reversedDepth = false;
};

// The frame's camera from Streamline's constants, without jitter. aRenderOrigin is added to cameraPos: zero unless
// step 1 finds the renderer works relative to an origin. Nothing if the projection isn't understood or the camera's
// axes are missing.
std::optional<CameraFrame> MakeCameraFrame(const SlConstants& aConstants, const Vec3d& aRenderOrigin);

// aCameraToClip with a jitter of (aJitterX, aJitterY) pixels added, for a render of aWidth x aHeight pixels: clip x
// moves by aSign * 2 * jitterX / width and clip y by -aSign * 2 * jitterY / height, times clip w.
Mat4 AddJitter(const Mat4& aCameraToClip, double aJitterX, double aJitterY, uint32_t aWidth, uint32_t aHeight, double aSign);

// The texel of a depth buffer under output pixel (aX, aY) of an aWidth x aHeight image, when the depth image fills
// aExtent of its buffer.
std::pair<uint32_t, uint32_t> DepthTexelFor(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight, const sl::Extent& aExtent);
}
```

- [x] **Step 5: Write `src/render/Camera.cpp`**

```cpp
#include "render/Camera.h"

#include <cmath>

namespace cpmc
{
namespace
{
double At(const Mat4& aMatrix, int aRow, int aColumn)
{
	return aMatrix[size_t(aRow * 4 + aColumn)];
}

bool Near(double aValue, double aTarget, double aTolerance)
{
	return std::fabs(aValue - aTarget) <= aTolerance;
}

// True when, for row vectors, exactly one view axis feeds clip w, with a factor of +-1, and nothing else does.
bool PerspectiveForRowVectors(const Mat4& aMatrix)
{
	int ones = 0;
	for (int axis = 0; axis < 3; ++axis)
	{
		const double w = std::fabs(At(aMatrix, axis, 3));
		if (Near(w, 1.0, 1e-3))
		{
			++ones;
		}
		else if (w > 1e-3)
		{
			return false;
		}
	}
	return ones == 1 && Near(At(aMatrix, 3, 3), 0.0, 1e-3);
}

Mat4 Transposed(const Mat4& aMatrix)
{
	Mat4 out{};
	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 4; ++column)
		{
			out[size_t(row * 4 + column)] = At(aMatrix, column, row);
		}
	}
	return out;
}

double Sign(double aValue)
{
	return aValue < 0.0 ? -1.0 : 1.0;
}

double Length(const std::array<float, 3>& aVector)
{
	return std::sqrt(double(aVector[0]) * aVector[0] + double(aVector[1]) * aVector[1] + double(aVector[2]) * aVector[2]);
}
}

Mat4 Multiply(const Mat4& aFirst, const Mat4& aThen)
{
	Mat4 out{};
	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 4; ++column)
		{
			double sum = 0.0;
			for (int k = 0; k < 4; ++k)
			{
				sum += At(aFirst, row, k) * At(aThen, k, column);
			}
			out[size_t(row * 4 + column)] = sum;
		}
	}
	return out;
}

std::array<double, 4> Transform(const std::array<double, 4>& aPoint, const Mat4& aMatrix)
{
	std::array<double, 4> out{};
	for (int column = 0; column < 4; ++column)
	{
		for (int k = 0; k < 4; ++k)
		{
			out[size_t(column)] += aPoint[size_t(k)] * At(aMatrix, k, column);
		}
	}
	return out;
}

std::optional<ProjectionLayout> FindProjectionLayout(const std::array<float, 16>& aViewToClip)
{
	Mat4 matrix{};
	for (size_t i = 0; i < 16; ++i)
	{
		matrix[i] = aViewToClip[i];
	}
	ProjectionLayout layout;
	if (!PerspectiveForRowVectors(matrix))
	{
		matrix = Transposed(matrix);
		if (!PerspectiveForRowVectors(matrix))
		{
			return std::nullopt;
		}
		layout.columnVectors = true;
	}
	for (int axis = 0; axis < 3; ++axis)
	{
		if (Near(std::fabs(At(matrix, axis, 3)), 1.0, 1e-3))
		{
			layout.forwardAxis = axis;
			layout.forwardSign = Sign(At(matrix, axis, 3));
		}
	}
	const int first = (layout.forwardAxis + 1) % 3;
	const int second = (layout.forwardAxis + 2) % 3;
	layout.rightAxis = std::fabs(At(matrix, first, 0)) >= std::fabs(At(matrix, second, 0)) ? first : second;
	layout.upAxis = layout.rightAxis == first ? second : first;
	layout.rightSign = Sign(At(matrix, layout.rightAxis, 0));
	layout.upSign = Sign(At(matrix, layout.upAxis, 1));
	if (std::fabs(At(matrix, layout.rightAxis, 0)) < 1e-3 || std::fabs(At(matrix, layout.upAxis, 1)) < 1e-3)
	{
		return std::nullopt;
	}
	return layout;
}

std::optional<CameraFrame> MakeCameraFrame(const SlConstants& aConstants, const Vec3d& aRenderOrigin)
{
	const std::optional<ProjectionLayout> layout = FindProjectionLayout(aConstants.viewToClip);
	if (!layout || !Near(Length(aConstants.forward), 1.0, 0.01) || !Near(Length(aConstants.right), 1.0, 0.01) ||
	    !Near(Length(aConstants.up), 1.0, 0.01))
	{
		return std::nullopt;
	}
	// World (camera-relative) to the game's view space: each view axis is the matching camera vector, signed.
	Mat4 view{};
	for (int i = 0; i < 3; ++i)
	{
		view[size_t(i * 4 + layout->forwardAxis)] = layout->forwardSign * aConstants.forward[size_t(i)];
		view[size_t(i * 4 + layout->rightAxis)] = layout->rightSign * aConstants.right[size_t(i)];
		view[size_t(i * 4 + layout->upAxis)] = layout->upSign * aConstants.up[size_t(i)];
	}
	view[15] = 1.0;
	Mat4 projection{};
	for (size_t i = 0; i < 16; ++i)
	{
		projection[i] = aConstants.viewToClip[i];
	}
	if (layout->columnVectors)
	{
		projection = Transposed(projection);
	}
	CameraFrame frame;
	frame.cameraToClip = Multiply(view, projection);
	frame.origin = { aConstants.position[0] + aRenderOrigin.x, aConstants.position[1] + aRenderOrigin.y,
		             aConstants.position[2] + aRenderOrigin.z };
	frame.reversedDepth = aConstants.depthInverted == 1;
	return frame;
}

Mat4 AddJitter(const Mat4& aCameraToClip, double aJitterX, double aJitterY, uint32_t aWidth, uint32_t aHeight, double aSign)
{
	Mat4 jitter{};
	jitter[0] = jitter[5] = jitter[10] = jitter[15] = 1.0;
	jitter[12] = aSign * 2.0 * aJitterX / aWidth;   // clip x += this * clip w
	jitter[13] = -aSign * 2.0 * aJitterY / aHeight;  // clip y: pixel rows run down, clip y up
	return Multiply(aCameraToClip, jitter);
}

std::pair<uint32_t, uint32_t> DepthTexelFor(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight, const sl::Extent& aExtent)
{
	const auto scale = [](uint32_t aPixel, uint32_t aSize, uint32_t aStart, uint32_t aSpan) {
		const auto texel = uint32_t((double(aPixel) + 0.5) * aSpan / aSize);
		return aStart + (texel < aSpan ? texel : aSpan - 1);
	};
	return { scale(aX, aWidth, aExtent.left, aExtent.width), scale(aY, aHeight, aExtent.top, aExtent.height) };
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/track/StateTracker.cpp
)
```
```cmake
	src/track/StateTracker.cpp
	src/render/Camera.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `48 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: camera from Streamline's constants" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 12: The atlas

`ParseAtlas` reads the `atlas.bin` that `tools/build_atlas.py` writes (Task 21). That file holds:
- the sprites, each with its mip levels;
- the blocks: each block's sprite per face, its tint, which faces are tinted, and whether it's cut out.

`tests/AtlasFixture.h` builds a small atlas in memory (stone, grass, glass, oak leaves and an oak log) for this test and
the mesher's.

**Files:**
- Create: `src/render/Atlas.h`, `src/render/Atlas.cpp`, `tests/AtlasFixture.h`, `tests/TestAtlas.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write `tests/AtlasFixture.h`**

```cpp
#pragma once

// A small atlas.bin, laid out as tools/build_atlas.py writes it: 8 sprites in a row of 16 x 16 tiles, 5 levels, and
// the blocks stone, grass_block, glass, oak_leaves and oak_log.
#include "render/Atlas.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace fixture
{
inline void Put32(std::vector<uint8_t>& aOut, uint32_t aValue)
{
	for (int i = 0; i < 4; ++i)
	{
		aOut.push_back(uint8_t(aValue >> (8 * i)));
	}
}

inline void PutBlock(std::vector<uint8_t>& aOut, const char* aName, std::initializer_list<uint16_t> aFaces,
                     std::initializer_list<uint8_t> aTint, uint8_t aTintedFaces, bool aCutout)
{
	char name[32]{};
	std::memcpy(name, aName, std::min<size_t>(std::strlen(aName), 31));
	aOut.insert(aOut.end(), name, name + 32);
	for (const uint16_t face : aFaces)
	{
		aOut.push_back(uint8_t(face));
		aOut.push_back(uint8_t(face >> 8));
	}
	aOut.insert(aOut.end(), aTint);
	aOut.push_back(aTintedFaces);
	aOut.push_back(aCutout ? 1 : 0);
	aOut.insert(aOut.end(), 15, 0);
}

inline std::vector<uint8_t> AtlasBytes()
{
	std::vector<uint8_t> out;
	const char           magic[] = "CPMCATL1";
	out.insert(out.end(), magic, magic + 8);
	const uint32_t width = 128, height = 16, levels = 5, sprites = 8, blocks = 5;
	for (const uint32_t value : { width, height, levels, sprites, blocks, 0u })
	{
		Put32(out, value);
	}
	for (uint32_t i = 0; i < sprites; ++i)
	{
		for (const uint32_t value : { i * 16, 0u, 16u, 16u })
		{
			Put32(out, value);
		}
	}
	// Sprites: 0 stone, 1 dirt, 2 grass top, 3 grass side, 4 glass, 5 leaves, 6 log top, 7 log side.
	PutBlock(out, "stone", { 0, 0, 0, 0, 0, 0 }, { 255, 255, 255 }, 0, false);
	PutBlock(out, "grass_block", { 1, 2, 3, 3, 3, 3 }, { 0x91, 0xBD, 0x59 }, 0b000010, false);
	PutBlock(out, "glass", { 4, 4, 4, 4, 4, 4 }, { 255, 255, 255 }, 0, true);
	PutBlock(out, "oak_leaves", { 5, 5, 5, 5, 5, 5 }, { 0x77, 0xAB, 0x2F }, 0b111111, true);
	PutBlock(out, "oak_log", { 6, 6, 7, 7, 7, 7 }, { 255, 255, 255 }, 0, false);
	uint32_t texels = 0;
	for (uint32_t level = 0; level < levels; ++level)
	{
		texels += (width >> level) * (height >> level);
	}
	for (uint32_t i = 0; i < texels; ++i)
	{
		Put32(out, 0xFF000000u | i);  // opaque, each texel different
	}
	return out;
}

inline cpmc::Atlas Atlas()
{
	std::string error;
	return *cpmc::ParseAtlas(AtlasBytes(), error);
}
}
```

- [x] **Step 2: Write the failing test, `tests/TestAtlas.cpp`**

```cpp
#include "Harness.h"

#include "AtlasFixture.h"

TEST(AtlasReadsWhatBuildAtlasWrites)
{
	std::string error;
	const auto  atlas = cpmc::ParseAtlas(fixture::AtlasBytes(), error);
	CHECK(atlas.has_value());
	CHECK_STR(error, "");
	CHECK_EQ(atlas->width, 128u);
	CHECK_EQ(atlas->height, 16u);
	CHECK_EQ(atlas->Levels(), 5u);
	CHECK_EQ(atlas->levelOffsets[1], 128u * 16u);
	CHECK_EQ(atlas->levelOffsets[4], 2048u + 512u + 128u + 32u);
	CHECK_EQ(atlas->texels.size(), size_t(2048 + 512 + 128 + 32 + 8));
	CHECK_EQ(atlas->texels[5], 0xFF000005u);
	CHECK_EQ(atlas->sprites[3].x, 48u);
	CHECK_EQ(atlas->blocks.size(), size_t(5));
	const cpmc::AtlasBlock& grass = atlas->blocks[1];
	CHECK_STR(grass.name, "grass_block");
	CHECK_EQ(grass.faces[1], uint16_t(2));
	CHECK_EQ(grass.tint[0], uint8_t(0x91));
	CHECK_EQ(grass.tintedFaces, uint8_t(0b000010));
	CHECK(atlas->blocks[2].cutout);
	CHECK(!atlas->blocks[0].cutout);
}

TEST(AtlasNumbersBlocksFromOne)
{
	const cpmc::Atlas atlas = fixture::Atlas();
	CHECK_EQ(*atlas.BlockId("stone"), uint16_t(1));
	CHECK_EQ(*atlas.BlockId("oak_log"), uint16_t(5));
	CHECK(!atlas.BlockId("bedrock"));
	CHECK(atlas.Block(0) == nullptr);  // air
	CHECK_STR(atlas.Block(3)->name, "glass");
	CHECK(atlas.Block(6) == nullptr);
}

TEST(AtlasRefusesBrokenFiles)
{
	std::string error;
	std::vector<uint8_t> bytes = fixture::AtlasBytes();
	bytes.pop_back();
	CHECK(!cpmc::ParseAtlas(bytes, error));
	CHECK(error.find("its header says") != std::string::npos);

	std::vector<uint8_t> wrongMagic = fixture::AtlasBytes();
	wrongMagic[0] = 'X';
	CHECK(!cpmc::ParseAtlas(wrongMagic, error));

	std::vector<uint8_t> badFace = fixture::AtlasBytes();
	badFace[32 + 8 * 16 + 32] = 9;  // stone's down face: sprite 9 of 8
	CHECK(!cpmc::ParseAtlas(badFace, error));
	CHECK(error.find("past the 8 sprites") != std::string::npos);

	CHECK(!cpmc::ParseAtlas({}, error));
}
```

- [x] **Step 3: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestCamera.cpp
)
```
```cmake
	tests/TestCamera.cpp
	tests/TestAtlas.cpp
)
```

- [x] **Step 4: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'render/Atlas.h'`.

- [x] **Step 5: Write `src/render/Atlas.h`**

```cpp
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cpmc
{
// One block of tools/build_atlas.py's atlas.bin (its docstring gives the layout).
struct AtlasBlock
{
	std::string              name;
	std::array<uint16_t, 6>  faces{};  // sprite per face: down, up, north, south, west, east
	std::array<uint8_t, 3>   tint{};   // RGB multiplier for the tinted faces
	uint8_t                  tintedFaces = 0;  // bit per face, same order
	bool                     cutout = false;
};

struct AtlasSprite
{
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t w = 0;
	uint32_t h = 0;
};

struct Atlas
{
	uint32_t                 width = 0;   // level 0, in texels
	uint32_t                 height = 0;
	std::vector<AtlasSprite> sprites;
	std::vector<AtlasBlock>  blocks;
	std::vector<uint32_t>    levelOffsets;  // where each level starts in `texels`
	std::vector<uint32_t>    texels;        // RGBA8, one per uint32 (red in the low byte), every level in turn

	uint32_t Levels() const { return uint32_t(levelOffsets.size()); }

	// The block ID for a block name: its index in `blocks` plus one, since 0 is air. Nothing for an unknown name.
	std::optional<uint16_t> BlockId(std::string_view aName) const;
	const AtlasBlock*       Block(uint16_t aId) const;  // null for air or an unknown ID
};

// Reads atlas.bin. Nothing, with aError saying why, if it isn't a complete atlas of the expected layout.
std::optional<Atlas> ParseAtlas(std::span<const uint8_t> aBytes, std::string& aError);
}
```

- [x] **Step 6: Write `src/render/Atlas.cpp`**

```cpp
#include "render/Atlas.h"

#include <algorithm>
#include <cstring>
#include <format>

namespace cpmc
{
namespace
{
constexpr size_t   kHeaderBytes = 32;
constexpr size_t   kSpriteBytes = 16;
constexpr size_t   kBlockBytes = 64;
constexpr uint32_t kMaxLevels = 8;

uint32_t U32(std::span<const uint8_t> aBytes, size_t aAt)
{
	uint32_t value = 0;
	std::memcpy(&value, aBytes.data() + aAt, sizeof(value));
	return value;
}

uint16_t U16(std::span<const uint8_t> aBytes, size_t aAt)
{
	uint16_t value = 0;
	std::memcpy(&value, aBytes.data() + aAt, sizeof(value));
	return value;
}

std::optional<Atlas> Fail(std::string& aError, std::string aWhy)
{
	aError = std::move(aWhy);
	return std::nullopt;
}
}

std::optional<uint16_t> Atlas::BlockId(std::string_view aName) const
{
	for (size_t i = 0; i < blocks.size(); ++i)
	{
		if (blocks[i].name == aName)
		{
			return uint16_t(i + 1);
		}
	}
	return std::nullopt;
}

const AtlasBlock* Atlas::Block(uint16_t aId) const
{
	return aId >= 1 && aId <= blocks.size() ? &blocks[aId - 1] : nullptr;
}

std::optional<Atlas> ParseAtlas(std::span<const uint8_t> aBytes, std::string& aError)
{
	if (aBytes.size() < kHeaderBytes || std::memcmp(aBytes.data(), "CPMCATL1", 8) != 0)
	{
		return Fail(aError, "not an atlas: no CPMCATL1 header");
	}
	Atlas atlas;
	atlas.width = U32(aBytes, 8);
	atlas.height = U32(aBytes, 12);
	const uint32_t levels = U32(aBytes, 16);
	const uint32_t spriteCount = U32(aBytes, 20);
	const uint32_t blockCount = U32(aBytes, 24);
	if (atlas.width == 0 || atlas.height == 0 || atlas.width > 4096 || atlas.height > 4096 || levels == 0 ||
	    levels > kMaxLevels || spriteCount == 0 || spriteCount > 4096 || blockCount == 0 || blockCount > 4096)
	{
		return Fail(aError, std::format("atlas header out of range: {} x {}, {} levels, {} sprites, {} blocks", atlas.width,
		                                atlas.height, levels, spriteCount, blockCount));
	}
	size_t texelCount = 0;
	for (uint32_t level = 0; level < levels; ++level)
	{
		atlas.levelOffsets.push_back(uint32_t(texelCount));
		texelCount += size_t(std::max(1u, atlas.width >> level)) * std::max(1u, atlas.height >> level);
	}
	const size_t expected = kHeaderBytes + spriteCount * kSpriteBytes + blockCount * kBlockBytes + texelCount * 4;
	if (aBytes.size() != expected)
	{
		return Fail(aError, std::format("atlas is {} bytes; its header says {}", aBytes.size(), expected));
	}
	size_t at = kHeaderBytes;
	for (uint32_t i = 0; i < spriteCount; ++i, at += kSpriteBytes)
	{
		AtlasSprite sprite{ U32(aBytes, at), U32(aBytes, at + 4), U32(aBytes, at + 8), U32(aBytes, at + 12) };
		if (sprite.w == 0 || sprite.h == 0 || sprite.x + sprite.w > atlas.width || sprite.y + sprite.h > atlas.height)
		{
			return Fail(aError, std::format("sprite {} lies outside the atlas", i));
		}
		atlas.sprites.push_back(sprite);
	}
	for (uint32_t i = 0; i < blockCount; ++i, at += kBlockBytes)
	{
		AtlasBlock block;
		const char* name = reinterpret_cast<const char*>(aBytes.data() + at);
		block.name.assign(name, strnlen(name, 32));
		for (size_t face = 0; face < 6; ++face)
		{
			block.faces[face] = U16(aBytes, at + 32 + face * 2);
			if (block.faces[face] >= spriteCount)
			{
				return Fail(aError, std::format("block {} uses sprite {}, past the {} sprites", block.name, block.faces[face], spriteCount));
			}
		}
		block.tint = { aBytes[at + 44], aBytes[at + 45], aBytes[at + 46] };
		block.tintedFaces = aBytes[at + 47];
		block.cutout = aBytes[at + 48] != 0;
		atlas.blocks.push_back(std::move(block));
	}
	atlas.texels.resize(texelCount);
	std::memcpy(atlas.texels.data(), aBytes.data() + at, texelCount * 4);
	return atlas;
}
}
```

- [x] **Step 7: Edit `CMakeLists.txt` to add the source**

```cmake
	src/render/Camera.cpp
)
```
```cmake
	src/render/Camera.cpp
	src/render/Atlas.cpp
)
```

- [x] **Step 8: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `51 test(s), 0 failure(s)`.

- [x] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: atlas reader" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" \
  && git log --oneline -1
```

---

### Task 13: The block world

Blocks are kept in sparse 16×16×16 sections, keyed by section coordinates (block >> 4, so negative coordinates split
by floor). Changing a block marks its section for meshing, and also a neighbouring section when the block lies on
their border. Only the main thread touches the world.

**Files:**
- Create: `src/world/BlockWorld.h`, `src/world/BlockWorld.cpp`, `tests/TestBlockWorld.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestBlockWorld.cpp`**

```cpp
#include "Harness.h"

#include "world/BlockWorld.h"

using cpmc::BlockPos;
using cpmc::BlockWorld;
using cpmc::SectionPos;

TEST(BlockWorldStoresBlocksInSections)
{
	BlockWorld world;
	CHECK_EQ(world.Get({ 3, 4, 5 }), uint16_t(0));
	world.Set({ 3, 4, 5 }, 7);
	world.Set({ -1, -1, -1 }, 2);
	CHECK_EQ(world.Get({ 3, 4, 5 }), uint16_t(7));
	CHECK_EQ(world.Get({ -1, -1, -1 }), uint16_t(2));
	CHECK_EQ(world.BlockCount(), size_t(2));
	CHECK_EQ(world.SectionCount(), size_t(2));
	CHECK(world.Find({ -1, -1, -1 }) != nullptr);
	CHECK(world.Find({ 1, 0, 0 }) == nullptr);
	world.Set({ 3, 4, 5 }, 0);
	CHECK_EQ(world.BlockCount(), size_t(1));
	world.Set({ -1, -1, -1 }, 3);  // replacing a block doesn't add one
	CHECK_EQ(world.BlockCount(), size_t(1));
}

TEST(BlockWorldSplitsNegativeCoordinatesByFloor)
{
	CHECK(cpmc::SectionOf({ -1, 0, 15 }) == (SectionPos{ -1, 0, 0 }));
	CHECK(cpmc::SectionOf({ -16, -17, 16 }) == (SectionPos{ -1, -2, 1 }));
}

TEST(BlockWorldMarksChangedSectionsAndBorderNeighbours)
{
	BlockWorld world;
	world.Set({ 5, 5, 5 }, 1);
	auto dirty = world.TakeDirty();
	CHECK_EQ(dirty.size(), size_t(1));
	CHECK(dirty[0] == (SectionPos{ 0, 0, 0 }));
	CHECK(world.TakeDirty().empty());

	world.Set({ 0, 5, 15 }, 1);  // on the west and south borders
	dirty = world.TakeDirty();
	CHECK_EQ(dirty.size(), size_t(3));
	CHECK(dirty[0] == (SectionPos{ -1, 0, 0 }));
	CHECK(dirty[1] == (SectionPos{ 0, 0, 0 }));
	CHECK(dirty[2] == (SectionPos{ 0, 0, 1 }));

	world.Set({ 5, 5, 5 }, 1);  // no change, nothing dirty
	CHECK(world.TakeDirty().empty());
}

TEST(BlockWorldCanMarkEverySectionDirty)
{
	BlockWorld world;
	world.Set({ 5, 5, 5 }, 1);
	world.Set({ 40, 5, 5 }, 1);
	world.TakeDirty();
	world.MarkAllDirty();
	CHECK_EQ(world.TakeDirty().size(), size_t(2));
	CHECK_EQ(world.BlockCount(), size_t(2));
}

TEST(BlockWorldClearEmptiesEverySection)
{
	BlockWorld world;
	world.Set({ 5, 5, 5 }, 1);
	world.Set({ 40, 5, 5 }, 1);
	world.TakeDirty();
	world.Clear();
	CHECK_EQ(world.BlockCount(), size_t(0));
	CHECK_EQ(world.SectionCount(), size_t(0));
	CHECK_EQ(world.TakeDirty().size(), size_t(2));
	CHECK_EQ(world.Get({ 5, 5, 5 }), uint16_t(0));
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestAtlas.cpp
)
```
```cmake
	tests/TestAtlas.cpp
	tests/TestBlockWorld.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'world/BlockWorld.h'`.

- [x] **Step 4: Write `src/world/BlockWorld.h`**

```cpp
#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace cpmc
{
// Minecraft block coordinates: x east, y up, z south.
struct BlockPos
{
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;

	auto operator<=>(const BlockPos&) const = default;
};

// A 16 x 16 x 16 section: block (x, y, z) lies in section (x >> 4, y >> 4, z >> 4).
struct SectionPos
{
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;

	auto operator<=>(const SectionPos&) const = default;
};

SectionPos SectionOf(const BlockPos& aBlock);

// Blocks by position, in sparse sections. Block IDs are the atlas's (0 is air). Main thread only.
class BlockWorld
{
public:
	using Section = std::array<uint16_t, 4096>;  // index: x + 16 * (z + 16 * y), local coordinates

	uint16_t Get(const BlockPos& aBlock) const;

	// Changes one block. Its section needs meshing again, and so does a neighbouring section when the block lies on
	// the border with it.
	void Set(const BlockPos& aBlock, uint16_t aId);

	// Removes every block. Every section that had blocks needs meshing again (to nothing).
	void Clear();

	// Every section needs meshing again, as when the meshing options change.
	void MarkAllDirty();

	// The sections changed since the last call, in order.
	std::vector<SectionPos> TakeDirty();

	const Section* Find(const SectionPos& aSection) const;  // null if it has never held a block
	size_t         BlockCount() const { return blocks_; }
	size_t         SectionCount() const { return sections_.size(); }

private:
	std::map<SectionPos, std::unique_ptr<Section>> sections_;
	std::set<SectionPos>                           dirty_;
	size_t                                         blocks_ = 0;
};
}
```

- [x] **Step 5: Write `src/world/BlockWorld.cpp`**

```cpp
#include "world/BlockWorld.h"

namespace cpmc
{
namespace
{
size_t IndexOf(const BlockPos& aBlock)
{
	return size_t(aBlock.x & 15) + 16 * (size_t(aBlock.z & 15) + 16 * size_t(aBlock.y & 15));
}
}

SectionPos SectionOf(const BlockPos& aBlock)
{
	// An arithmetic shift is a floor division, negative coordinates included.
	return { aBlock.x >> 4, aBlock.y >> 4, aBlock.z >> 4 };
}

uint16_t BlockWorld::Get(const BlockPos& aBlock) const
{
	const Section* section = Find(SectionOf(aBlock));
	return section ? (*section)[IndexOf(aBlock)] : 0;
}

void BlockWorld::Set(const BlockPos& aBlock, uint16_t aId)
{
	const SectionPos at = SectionOf(aBlock);
	auto&            section = sections_[at];
	if (!section)
	{
		section = std::make_unique<Section>();
		section->fill(0);
	}
	uint16_t& block = (*section)[IndexOf(aBlock)];
	if (block == aId)
	{
		return;
	}
	blocks_ = blocks_ - (block != 0 ? 1 : 0) + (aId != 0 ? 1 : 0);
	block = aId;
	dirty_.insert(at);
	const int32_t local[3] = { aBlock.x & 15, aBlock.y & 15, aBlock.z & 15 };
	for (int axis = 0; axis < 3; ++axis)
	{
		for (const int32_t side : { -1, 1 })
		{
			if (local[axis] == (side < 0 ? 0 : 15))
			{
				SectionPos neighbour = at;
				(axis == 0 ? neighbour.x : axis == 1 ? neighbour.y : neighbour.z) += side;
				dirty_.insert(neighbour);
			}
		}
	}
}

void BlockWorld::Clear()
{
	for (const auto& [at, section] : sections_)
	{
		dirty_.insert(at);
	}
	sections_.clear();
	blocks_ = 0;
}

void BlockWorld::MarkAllDirty()
{
	for (const auto& [at, section] : sections_)
	{
		dirty_.insert(at);
	}
}

std::vector<SectionPos> BlockWorld::TakeDirty()
{
	std::vector<SectionPos> dirty(dirty_.begin(), dirty_.end());
	dirty_.clear();
	return dirty;
}

const BlockWorld::Section* BlockWorld::Find(const SectionPos& aSection) const
{
	const auto found = sections_.find(aSection);
	return found == sections_.end() ? nullptr : found->second.get();
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/render/Atlas.cpp
)
```
```cmake
	src/render/Atlas.cpp
	src/world/BlockWorld.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `56 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: block world" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" \
  && git log --oneline -1
```

---

### Task 14: SkyCraft's vertex and the mesher

`Vertex.h` is SkyCraft's 32-byte section vertex (the spec's "Vertex format"). `MeshSection` writes a section's
visible faces in that format:
- **Faces:** a face is dropped against an opaque neighbour, across section borders too. A cut-out block hides a face
  only against the same block, as glass does against glass.
- **Colour:** each face gets its tint, and Minecraft's corner ambient occlusion.
- **Splitting:** each quad is split along its darker diagonal, as Minecraft does.

Minecraft's per-face brightness is left to the shader.

**Files:**
- Create: `src/world/Vertex.h`, `src/world/Mesher.h`, `src/world/Mesher.cpp`, `tests/TestMesher.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestMesher.cpp`**

```cpp
#include "Harness.h"

#include "AtlasFixture.h"
#include "world/Mesher.h"

#include <cstddef>

using cpmc::BlockWorld;
using cpmc::MeshSection;
using cpmc::Vertex;

// SkyCraft's byte layout (the spec's "Vertex format").
static_assert(offsetof(Vertex, x) == 0);
static_assert(offsetof(Vertex, u) == 12);
static_assert(offsetof(Vertex, r) == 20);
static_assert(offsetof(Vertex, a) == 23);
static_assert(offsetof(Vertex, light) == 24);
static_assert(offsetof(Vertex, flags) == 28);

namespace
{
const cpmc::Atlas& TestAtlas()
{
	static const cpmc::Atlas atlas = fixture::Atlas();
	return atlas;
}

uint16_t Id(const char* aName)
{
	return *TestAtlas().BlockId(aName);
}

std::vector<Vertex> Mesh(const BlockWorld& aWorld, bool aOcclusion = true, cpmc::SectionPos aSection = {})
{
	return MeshSection(aWorld, aSection, TestAtlas(), { aOcclusion });
}

size_t Faces(const std::vector<Vertex>& aVertices)
{
	return aVertices.size() / 6;
}
}

TEST(MesherGivesALoneBlockSixFaces)
{
	BlockWorld world;
	world.Set({ 2, 3, 4 }, Id("stone"));
	const auto vertices = Mesh(world);
	CHECK_EQ(vertices.size(), size_t(36));
	int perFace[7] = {};
	for (const Vertex& v : vertices)
	{
		CHECK(v.x >= 2.0f && v.x <= 3.0f && v.y >= 3.0f && v.y <= 4.0f && v.z >= 4.0f && v.z <= 5.0f);
		CHECK_EQ(v.flags & 1u, 1u);  // alpha-tested
		CHECK_EQ(v.light, 15u << 8);
		CHECK_EQ(int(v.r), 255);  // no tint, nothing nearby to occlude
		++perFace[(v.flags >> 4) & 15];
	}
	for (int face = 1; face <= 6; ++face)
	{
		CHECK_EQ(perFace[face], 6);
	}
	CHECK(MeshSection(world, { 1, 0, 0 }, TestAtlas(), {}).empty());
}

TEST(MesherHidesFacesBetweenOpaqueNeighbours)
{
	BlockWorld world;
	world.Set({ 0, 0, 0 }, Id("stone"));
	world.Set({ 1, 0, 0 }, Id("stone"));
	CHECK_EQ(Faces(Mesh(world)), size_t(10));
}

TEST(MesherHidesFacesAgainstTheNextSection)
{
	BlockWorld world;
	world.Set({ 15, 0, 0 }, Id("stone"));
	world.Set({ 16, 0, 0 }, Id("stone"));
	CHECK_EQ(Faces(Mesh(world, true, { 0, 0, 0 })), size_t(5));
	CHECK_EQ(Faces(Mesh(world, true, { 1, 0, 0 })), size_t(5));
}

TEST(MesherLetsCutoutBlocksShowWhatsBehindThem)
{
	BlockWorld glassAndStone;
	glassAndStone.Set({ 0, 0, 0 }, Id("glass"));
	glassAndStone.Set({ 1, 0, 0 }, Id("stone"));
	CHECK_EQ(Faces(Mesh(glassAndStone)), size_t(11));  // the stone's face shows through the glass

	BlockWorld twoGlass;
	twoGlass.Set({ 0, 0, 0 }, Id("glass"));
	twoGlass.Set({ 1, 0, 0 }, Id("glass"));
	CHECK_EQ(Faces(Mesh(twoGlass)), size_t(10));  // glass hides glass

	BlockWorld glassAndLeaves;
	glassAndLeaves.Set({ 0, 0, 0 }, Id("glass"));
	glassAndLeaves.Set({ 1, 0, 0 }, Id("oak_leaves"));
	CHECK_EQ(Faces(Mesh(glassAndLeaves)), size_t(12));  // different cut-out blocks hide nothing
}

TEST(MesherTintsOnlyTheTintedFaces)
{
	BlockWorld world;
	world.Set({ 0, 0, 0 }, Id("grass_block"));
	for (const Vertex& v : Mesh(world))
	{
		const uint32_t face = ((v.flags >> 4) & 15) - 1;
		if (face == uint32_t(cpmc::Face::Up))
		{
			CHECK_EQ(int(v.r), 0x91);
			CHECK_EQ(int(v.g), 0xBD);
			CHECK_EQ(int(v.b), 0x59);
		}
		else
		{
			CHECK_EQ(int(v.g), 255);
		}
	}
}

TEST(MesherMapsEachFaceToItsSprite)
{
	BlockWorld world;
	world.Set({ 0, 0, 0 }, Id("oak_log"));
	for (const Vertex& v : Mesh(world))
	{
		const uint32_t face = ((v.flags >> 4) & 15) - 1;
		const float    left = (face <= 1 ? 6.0f : 7.0f) * 16.0f / 128.0f;  // log top is sprite 6, log side sprite 7
		CHECK(v.u >= left - 1e-6f && v.u <= left + 16.0f / 128.0f + 1e-6f);
		CHECK(v.v >= 0.0f && v.v <= 1.0f);
	}
}

TEST(MesherShadesCornersLikeMinecraft)
{
	BlockWorld world;
	world.Set({ 0, 0, 0 }, Id("stone"));
	world.Set({ 1, 1, 0 }, Id("stone"));  // above the east half of the top face
	const auto vertices = Mesh(world);
	int        dark = 0;
	for (size_t i = 6; i < 12; ++i)  // the first block's top face
	{
		const Vertex& v = vertices[i];
		CHECK_EQ(int(v.r), v.x == 1.0f ? 204 : 255);  // (1 + 0.2 + 1 + 1) / 4 = 0.8
		dark += v.x == 1.0f ? 1 : 0;
	}
	CHECK_EQ(dark, 3);  // two corners, one of them in both triangles
	for (const Vertex& v : Mesh(world, false))
	{
		CHECK_EQ(int(v.r), 255);
	}
}

TEST(MesherSplitsQuadsAlongTheDarkerDiagonal)
{
	BlockWorld world;
	world.Set({ 0, 0, 0 }, Id("stone"));
	world.Set({ 1, 1, 1 }, Id("stone"));  // diagonal to the top face's corner (1, 1, 1) only
	const auto vertices = Mesh(world);
	// That corner is corner 1, the only shaded one, so the quad is split along 1-3 and starts there.
	CHECK_EQ(vertices[6].x, 1.0f);
	CHECK_EQ(vertices[6].z, 1.0f);
	CHECK_EQ(int(vertices[6].r), 204);
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestBlockWorld.cpp
)
```
```cmake
	tests/TestBlockWorld.cpp
	tests/TestMesher.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'world/Mesher.h'`.

- [x] **Step 4: Write `src/world/Vertex.h`**

```cpp
#pragma once

#include <cstdint>

namespace cpmc
{
// SkyCraft's section vertex (the spec's "Vertex format"). Quads are two triangles, six vertices, with no index buffer.
struct Vertex
{
	float    x = 0.0f;  // from the section's minimum corner, in blocks, in Minecraft axes (x east, y up, z south)
	float    y = 0.0f;
	float    z = 0.0f;
	float    u = 0.0f;  // atlas coordinates, 0 to 1
	float    v = 0.0f;
	uint8_t  r = 255;  // tint and ambient occlusion, without Minecraft's per-face brightness
	uint8_t  g = 255;
	uint8_t  b = 255;
	uint8_t  a = 255;
	uint32_t light = 0;  // block light (0-15) in bits 0-7, sky light (0-15) in bits 8-15
	uint32_t flags = 0;  // bit 0 alpha-tested (opaque or cut-out), bit 1 translucent, bits 4-7 the face's direction + 1
};
static_assert(sizeof(Vertex) == 32);

// Minecraft's Direction order, which the vertex flags and the atlas's faces use.
enum class Face : uint8_t
{
	Down,
	Up,
	North,
	South,
	West,
	East,
};

constexpr uint32_t kFlagAlphaTested = 1u << 0;
constexpr uint32_t kFlagTranslucent = 1u << 1;

constexpr uint32_t FaceFlags(Face aFace)
{
	return (uint32_t(aFace) + 1u) << 4;
}
}
```

- [x] **Step 5: Write `src/world/Mesher.h`**

```cpp
#pragma once

#include "render/Atlas.h"
#include "world/BlockWorld.h"
#include "world/Vertex.h"

#include <vector>

namespace cpmc
{
struct MeshOptions
{
	bool ambientOcclusion = true;  // Minecraft-style corner shading in the vertex colour
};

// The visible faces of one section's blocks as SkyCraft vertices, positions relative to the section's minimum corner.
// - A face is hidden by an opaque neighbour, in this section or the next. A cut-out block (glass, leaves) hides a
//   face only of the same block.
// - Colour: the face's tint (if the atlas tints it) times the corner's ambient occlusion: (1 + side1 + side2 +
//   corner) / 4, where an opaque block counts 0.2 and anything else 1, and two opaque sides make the corner count
//   0.2 too. Minecraft's per-face brightness is left to the shader.
// - Light: sky light 15, block light 0. Flags: alpha-tested, plus the face's direction.
// - Each quad is split along its darker diagonal, so a shaded corner's gradient spreads over both triangles alike.
std::vector<Vertex> MeshSection(const BlockWorld& aWorld, const SectionPos& aSection, const Atlas& aAtlas,
                                const MeshOptions& aOptions);
}
```

- [x] **Step 6: Write `src/world/Mesher.cpp`**

```cpp
#include "world/Mesher.h"

#include <array>
#include <cmath>

namespace cpmc
{
namespace
{
struct FaceShape
{
	std::array<int, 3>                normal;
	std::array<std::array<int, 3>, 4> corners;  // unit-cube corners, counter-clockwise seen from outside
};

// Corners per face, in Face order. Seen from outside, corner 0 is bottom-left, then bottom-right, top-right and
// top-left (for up and down: the texture's bottom-left onwards), so every face takes the same UVs.
constexpr std::array<FaceShape, 6> kFaces = { {
	{ { 0, -1, 0 }, { { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 1 }, { 0, 0, 1 } } } },  // down
	{ { 0, 1, 0 }, { { { 0, 1, 1 }, { 1, 1, 1 }, { 1, 1, 0 }, { 0, 1, 0 } } } },   // up
	{ { 0, 0, -1 }, { { { 1, 0, 0 }, { 0, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 } } } },  // north
	{ { 0, 0, 1 }, { { { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 } } } },   // south
	{ { -1, 0, 0 }, { { { 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 1 }, { 0, 1, 0 } } } },  // west
	{ { 1, 0, 0 }, { { { 1, 0, 1 }, { 1, 0, 0 }, { 1, 1, 0 }, { 1, 1, 1 } } } },   // east
} };

// Texture coordinates of the corners: bottom-left, bottom-right, top-right, top-left.
constexpr std::array<std::array<float, 2>, 4> kCornerUv = { { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f } } };

bool Opaque(const Atlas& aAtlas, uint16_t aId)
{
	const AtlasBlock* block = aAtlas.Block(aId);
	return block && !block->cutout;
}

bool FaceHidden(const Atlas& aAtlas, uint16_t aId, uint16_t aNeighbour)
{
	const AtlasBlock* neighbour = aAtlas.Block(aNeighbour);
	return neighbour && (!neighbour->cutout || aNeighbour == aId);
}

uint8_t Scale(uint8_t aValue, float aFactor)
{
	return uint8_t(std::lround(aValue * aFactor));
}
}

std::vector<Vertex> MeshSection(const BlockWorld& aWorld, const SectionPos& aSection, const Atlas& aAtlas,
                                const MeshOptions& aOptions)
{
	std::vector<Vertex>       vertices;
	const BlockWorld::Section* section = aWorld.Find(aSection);
	if (!section)
	{
		return vertices;
	}
	const BlockPos base{ aSection.x * 16, aSection.y * 16, aSection.z * 16 };
	for (int y = 0; y < 16; ++y)
	{
		for (int z = 0; z < 16; ++z)
		{
			for (int x = 0; x < 16; ++x)
			{
				const uint16_t    id = (*section)[size_t(x + 16 * (z + 16 * y))];
				const AtlasBlock* block = aAtlas.Block(id);
				if (!block)
				{
					continue;
				}
				const BlockPos at{ base.x + x, base.y + y, base.z + z };
				for (size_t face = 0; face < 6; ++face)
				{
					const FaceShape& shape = kFaces[face];
					const BlockPos   front{ at.x + shape.normal[0], at.y + shape.normal[1], at.z + shape.normal[2] };
					if (FaceHidden(aAtlas, id, aWorld.Get(front)))
					{
						continue;
					}
					const AtlasSprite& sprite = aAtlas.sprites[block->faces[face]];
					const bool         tinted = (block->tintedFaces >> face) & 1;
					std::array<float, 4> shade{ 1.0f, 1.0f, 1.0f, 1.0f };
					if (aOptions.ambientOcclusion)
					{
						for (size_t c = 0; c < 4; ++c)
						{
							// The two blocks beside this corner, and the one diagonal to it, in the layer the face looks into.
							BlockPos side1 = front, side2 = front, corner = front;
							int      tangent = 0;
							for (int axis = 0; axis < 3; ++axis)
							{
								if (shape.normal[size_t(axis)] != 0)
								{
									continue;
								}
								const int step = shape.corners[c][size_t(axis)] == 0 ? -1 : 1;
								BlockPos& side = tangent++ == 0 ? side1 : side2;
								(axis == 0 ? side.x : axis == 1 ? side.y : side.z) += step;
								(axis == 0 ? corner.x : axis == 1 ? corner.y : corner.z) += step;
							}
							const bool s1 = Opaque(aAtlas, aWorld.Get(side1));
							const bool s2 = Opaque(aAtlas, aWorld.Get(side2));
							const bool c3 = (s1 && s2) || Opaque(aAtlas, aWorld.Get(corner));
							shade[c] = (1.0f + (s1 ? 0.2f : 1.0f) + (s2 ? 0.2f : 1.0f) + (c3 ? 0.2f : 1.0f)) / 4.0f;
						}
					}
					Vertex corners[4];
					for (size_t c = 0; c < 4; ++c)
					{
						Vertex& v = corners[c];
						v.x = float(x + shape.corners[c][0]);
						v.y = float(y + shape.corners[c][1]);
						v.z = float(z + shape.corners[c][2]);
						v.u = (sprite.x + kCornerUv[c][0] * sprite.w) / float(aAtlas.width);
						v.v = (sprite.y + kCornerUv[c][1] * sprite.h) / float(aAtlas.height);
						v.r = Scale(tinted ? block->tint[0] : 255, shade[c]);
						v.g = Scale(tinted ? block->tint[1] : 255, shade[c]);
						v.b = Scale(tinted ? block->tint[2] : 255, shade[c]);
						v.a = 255;
						v.light = 15u << 8;
						v.flags = kFlagAlphaTested | FaceFlags(Face(face));
					}
					// Split along the darker diagonal, so a shaded corner's gradient spreads over both triangles alike.
					const bool flip = shade[0] + shade[2] > shade[1] + shade[3];
					const int  order[6] = { 0, 1, 2, 0, 2, 3 };
					const int  flipped[6] = { 1, 2, 3, 1, 3, 0 };
					for (const int c : flip ? flipped : order)
					{
						vertices.push_back(corners[c]);
					}
				}
			}
		}
	}
	return vertices;
}
}
```

- [x] **Step 7: Edit `CMakeLists.txt` to add the source**

```cmake
	src/world/BlockWorld.cpp
)
```
```cmake
	src/world/BlockWorld.cpp
	src/world/Mesher.cpp
)
```

- [x] **Step 8: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `64 test(s), 0 failure(s)`.

- [x] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: SkyCraft's vertex and the section mesher" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 15: The test structures

A structure's minimum corner goes 3 blocks ahead of V, in the block layer that holds V's feet. Part 1 has two
structures:
- **`cube`:** one stone block;
- **`sampler`:** every block in the atlas, in a row, with a gap between each, on a stone base.

`house` and `field` come with step 3, in Part 2.

**Files:**
- Create: `src/world/Structures.h`, `src/world/Structures.cpp`, `tests/TestStructures.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestStructures.cpp`**

```cpp
#include "Harness.h"

#include "AtlasFixture.h"
#include "world/Structures.h"

using cpmc::BlockPos;
using cpmc::BlockWorld;

TEST(StructuresGoThreeBlocksAheadInTheLayerOfVsFeet)
{
	// V at (10.4, 20.2, 7.9) facing north (+y): 3 m north is world (10.4, 23.2, 7.9), Minecraft (10.4, 7.9, -23.2).
	CHECK(cpmc::PlacementOrigin({ 10.4, 20.2, 7.9 }, { 0.0f, 1.0f, 0.0f }) == (BlockPos{ 10, 7, -24 }));
	// Facing east (+x) and a little down: only the horizontal direction counts.
	CHECK(cpmc::PlacementOrigin({ 0.5, 0.5, -0.5 }, { 0.8f, 0.0f, -0.6f }) == (BlockPos{ 3, -1, -1 }));
	// Looking straight down: north.
	CHECK(cpmc::PlacementOrigin({ 0.5, 0.5, 0.0 }, { 0.0f, 0.0f, -1.0f }) == (BlockPos{ 0, 0, -4 }));
}

TEST(StructuresPlaceACube)
{
	const cpmc::Atlas atlas = fixture::Atlas();
	BlockWorld        world;
	std::string       error;
	const auto        placed = cpmc::PlaceStructure(world, atlas, "cube", { 1, 2, 3 }, error);
	CHECK(placed.has_value());
	CHECK_EQ(placed->blocks, size_t(1));
	CHECK_EQ(world.BlockCount(), size_t(1));
	CHECK_EQ(world.Get({ 1, 2, 3 }), *atlas.BlockId("stone"));
}

TEST(StructuresPlaceASamplerOfEveryBlock)
{
	const cpmc::Atlas atlas = fixture::Atlas();
	BlockWorld        world;
	std::string       error;
	const auto        placed = cpmc::PlaceStructure(world, atlas, "sampler", { 0, 10, 0 }, error);
	CHECK(placed.has_value());
	CHECK_EQ(placed->blocks, size_t(15));  // 5 blocks on 10 base blocks
	CHECK(placed->max == (BlockPos{ 9, 11, 0 }));
	CHECK_EQ(world.Get({ 0, 11, 0 }), uint16_t(1));
	CHECK_EQ(world.Get({ 8, 11, 0 }), uint16_t(5));
	CHECK_EQ(world.Get({ 9, 11, 0 }), uint16_t(0));  // the gap
	CHECK_EQ(world.Get({ 9, 10, 0 }), *atlas.BlockId("stone"));
}

TEST(StructuresKnowTheirWorldBox)
{
	cpmc::Placed placed;
	placed.min = { 10, 7, -24 };
	placed.max = { 11, 8, -23 };
	const auto [low, high] = cpmc::WorldBox(placed);
	CHECK_EQ(low.x, 10.0);
	CHECK_EQ(low.y, 22.0);
	CHECK_EQ(low.z, 7.0);
	CHECK_EQ(high.x, 12.0);
	CHECK_EQ(high.y, 24.0);
	CHECK_EQ(high.z, 9.0);
}

TEST(StructuresRefuseUnknownNames)
{
	const cpmc::Atlas atlas = fixture::Atlas();
	BlockWorld        world;
	std::string       error;
	CHECK(!cpmc::PlaceStructure(world, atlas, "castle", {}, error));
	CHECK_STR(error, "no structure called \"castle\" in this build");
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestMesher.cpp
)
```
```cmake
	tests/TestMesher.cpp
	tests/TestStructures.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'world/Structures.h'`.

- [x] **Step 4: Write `src/world/Structures.h`**

```cpp
#pragma once

#include "Coords.h"
#include "render/Atlas.h"
#include "world/BlockWorld.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace cpmc
{
// Where a structure goes, in Minecraft block coordinates: its minimum corner 3 blocks ahead of V (along V's forward,
// flattened), in the block layer that holds V's feet. V's feet and forward are in world coordinates.
BlockPos PlacementOrigin(const Vec3d& aFeet, const Vec3f& aForward);

struct Placed
{
	size_t   blocks = 0;
	BlockPos min;  // the corners of the box it fills, inclusive
	BlockPos max;
};

// The world box a structure fills: Minecraft blocks aPlaced.min to aPlaced.max, inclusive, in world coordinates.
std::pair<Vec3d, Vec3d> WorldBox(const Placed& aPlaced);

// Adds a structure with its minimum corner at aOrigin (see the spec's "Structures"):
// - "cube": one stone block;
// - "sampler": every block of the atlas, in a row along +x with a gap between each, on a stone base.
// Nothing, with aError saying why, for another name or when the atlas lacks stone.
std::optional<Placed> PlaceStructure(BlockWorld& aWorld, const Atlas& aAtlas, std::string_view aName, const BlockPos& aOrigin,
                                     std::string& aError);
}
```

- [x] **Step 5: Write `src/world/Structures.cpp`**

```cpp
#include "world/Structures.h"

#include <cmath>

namespace cpmc
{
BlockPos PlacementOrigin(const Vec3d& aFeet, const Vec3f& aForward)
{
	double       x = aForward.x, y = aForward.y;
	const double length = std::sqrt(x * x + y * y);
	if (length > 1e-6)
	{
		x /= length;
		y /= length;
	}
	else
	{
		x = 0.0;
		y = 1.0;  // looking straight up or down: place it to the north
	}
	const Vec3d ahead = McFromWorld({ aFeet.x + 3.0 * x, aFeet.y + 3.0 * y, aFeet.z });
	return { int32_t(std::floor(ahead.x)), int32_t(std::floor(ahead.y)), int32_t(std::floor(ahead.z)) };
}

std::pair<Vec3d, Vec3d> WorldBox(const Placed& aPlaced)
{
	// Minecraft block (x, y, z) fills world x..x+1, -z-1..-z, y..y+1.
	return { { double(aPlaced.min.x), double(-aPlaced.max.z - 1), double(aPlaced.min.y) },
	         { double(aPlaced.max.x + 1), double(-aPlaced.min.z), double(aPlaced.max.y + 1) } };
}

std::optional<Placed> PlaceStructure(BlockWorld& aWorld, const Atlas& aAtlas, std::string_view aName, const BlockPos& aOrigin,
                                     std::string& aError)
{
	const std::optional<uint16_t> stone = aAtlas.BlockId("stone");
	if (!stone)
	{
		aError = "the atlas has no stone";
		return std::nullopt;
	}
	Placed placed;
	placed.min = aOrigin;
	if (aName == "cube")
	{
		aWorld.Set(aOrigin, *stone);
		placed.blocks = 1;
		placed.max = aOrigin;
		return placed;
	}
	if (aName == "sampler")
	{
		const auto count = int32_t(aAtlas.blocks.size());
		for (int32_t i = 0; i < count; ++i)
		{
			aWorld.Set({ aOrigin.x + 2 * i, aOrigin.y, aOrigin.z }, *stone);
			aWorld.Set({ aOrigin.x + 2 * i + 1, aOrigin.y, aOrigin.z }, *stone);
			aWorld.Set({ aOrigin.x + 2 * i, aOrigin.y + 1, aOrigin.z }, uint16_t(i + 1));
		}
		placed.blocks = size_t(3 * count);
		placed.max = { aOrigin.x + 2 * count - 1, aOrigin.y + 1, aOrigin.z };
		return placed;
	}
	aError = "no structure called \"" + std::string(aName) + "\" in this build";
	return std::nullopt;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/world/Mesher.cpp
)
```
```cmake
	src/world/Mesher.cpp
	src/world/Structures.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `69 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: test structures" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" \
  && git log --oneline -1
```

---

### Task 16: The DDS writer

Dumps are saved as `.dds` files with a DX10 header, in a typed form of the resource's own format:
- a depth plane is saved as `R32_FLOAT`, and a stencil plane as `R8_UINT`;
- other typeless formats are saved as their `UNORM` or `FLOAT` form.

**Files:**
- Create: `src/log/DdsWriter.h`, `src/log/DdsWriter.cpp`, `tests/TestDds.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestDds.cpp`**

```cpp
#include "Harness.h"

#include "log/DdsWriter.h"

#include <dxgiformat.h>

#include <cstring>

namespace
{
uint32_t Read32(const std::vector<uint8_t>& aBytes, size_t aAt)
{
	uint32_t value = 0;
	std::memcpy(&value, aBytes.data() + aAt, 4);
	return value;
}
}

TEST(DdsWritesTheHeaderAndPacksRows)
{
	// 3 x 2 texels of RGBA8, rows 16 bytes apart in the source.
	uint8_t source[32];
	for (uint8_t i = 0; i < 32; ++i)
	{
		source[i] = i;
	}
	const auto dds = cpmc::MakeDds(DXGI_FORMAT_R8G8B8A8_UNORM, 3, 2, source, 16);
	CHECK_EQ(dds.size(), size_t(4 + 124 + 20 + 24));
	CHECK(std::memcmp(dds.data(), "DDS ", 4) == 0);
	CHECK_EQ(Read32(dds, 4), 124u);
	CHECK_EQ(Read32(dds, 12), 2u);   // height
	CHECK_EQ(Read32(dds, 16), 3u);   // width
	CHECK_EQ(Read32(dds, 20), 12u);  // bytes per row
	CHECK(std::memcmp(dds.data() + 84, "DX10", 4) == 0);
	CHECK_EQ(Read32(dds, 128), uint32_t(DXGI_FORMAT_R8G8B8A8_UNORM));
	CHECK_EQ(Read32(dds, 132), 3u);  // 2D
	CHECK_EQ(Read32(dds, 140), 1u);  // one slice
	CHECK_EQ(int(dds[148]), 0);
	CHECK_EQ(int(dds[148 + 11]), 11);
	CHECK_EQ(int(dds[148 + 12]), 16);  // the second row starts at the source's pitch
}

TEST(DdsKnowsTexelSizes)
{
	CHECK_EQ(cpmc::BytesPerTexel(DXGI_FORMAT_R10G10B10A2_UNORM), 4u);
	CHECK_EQ(cpmc::BytesPerTexel(DXGI_FORMAT_R16G16B16A16_FLOAT), 8u);
	CHECK_EQ(cpmc::BytesPerTexel(DXGI_FORMAT_R8_UINT), 1u);
	CHECK_EQ(cpmc::BytesPerTexel(DXGI_FORMAT_R16G16_FLOAT), 4u);
	CHECK_EQ(cpmc::BytesPerTexel(DXGI_FORMAT_BC1_UNORM), 0u);
}

TEST(DdsTypesTypelessAndDepthPlanes)
{
	CHECK_EQ(cpmc::DumpFormat(DXGI_FORMAT_R32G8X24_TYPELESS, 0), uint32_t(DXGI_FORMAT_R32_FLOAT));
	CHECK_EQ(cpmc::DumpFormat(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 1), uint32_t(DXGI_FORMAT_R8_UINT));
	CHECK_EQ(cpmc::DumpFormat(DXGI_FORMAT_R24G8_TYPELESS, 0), uint32_t(DXGI_FORMAT_R24_UNORM_X8_TYPELESS));
	CHECK_EQ(cpmc::DumpFormat(DXGI_FORMAT_R10G10B10A2_TYPELESS, 0), uint32_t(DXGI_FORMAT_R10G10B10A2_UNORM));
	CHECK_EQ(cpmc::DumpFormat(DXGI_FORMAT_R16G16_TYPELESS, 0), uint32_t(DXGI_FORMAT_R16G16_FLOAT));
	CHECK_EQ(cpmc::DumpFormat(DXGI_FORMAT_R11G11B10_FLOAT, 0), uint32_t(DXGI_FORMAT_R11G11B10_FLOAT));
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestStructures.cpp
)
```
```cmake
	tests/TestStructures.cpp
	tests/TestDds.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'log/DdsWriter.h'`.

- [x] **Step 4: Write `src/log/DdsWriter.h`**

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cpmc
{
// Bytes per texel of the uncompressed DXGI formats a dump saves; 0 for any other format.
uint32_t BytesPerTexel(uint32_t aDxgiFormat);

// The typed format a dump saves for one plane of a resource whose format may be typeless or a depth format: a depth
// plane becomes R32_FLOAT (or the 24-bit depth's R24_UNORM_X8_TYPELESS), a stencil plane R8_UINT, other typeless
// formats their UNORM or FLOAT form (R16G16 as FLOAT, as motion vectors are). Typed formats stay as they are.
uint32_t DumpFormat(uint32_t aResourceFormat, uint32_t aPlane);

// A DDS file (with the DX10 header) for one 2D image of aFormat, from aHeight rows of aWidth texels, each row
// starting aPitch bytes after the last in aData. The file's rows are packed.
std::vector<uint8_t> MakeDds(uint32_t aFormat, uint32_t aWidth, uint32_t aHeight, const uint8_t* aData, size_t aPitch);
}
```

- [x] **Step 5: Write `src/log/DdsWriter.cpp`**

```cpp
#include "log/DdsWriter.h"

#include <dxgiformat.h>

#include <cstring>

namespace cpmc
{
namespace
{
void Put32(std::vector<uint8_t>& aOut, uint32_t aValue)
{
	for (int i = 0; i < 4; ++i)
	{
		aOut.push_back(uint8_t(aValue >> (8 * i)));
	}
}
}

uint32_t BytesPerTexel(uint32_t aDxgiFormat)
{
	switch (DXGI_FORMAT(aDxgiFormat))
	{
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		return 16;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
		return 8;
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R11G11B10_FLOAT:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
		return 4;
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
		return 2;
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
	case DXGI_FORMAT_A8_UNORM:
		return 1;
	default:
		return 0;
	}
}

uint32_t DumpFormat(uint32_t aResourceFormat, uint32_t aPlane)
{
	switch (DXGI_FORMAT(aResourceFormat))
	{
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		return aPlane == 0 ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R8_UINT;
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return aPlane == 0 ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : DXGI_FORMAT_R8_UINT;
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_D16_UNORM:
		return DXGI_FORMAT_R16_UNORM;
	case DXGI_FORMAT_R32G32B32A32_TYPELESS: return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case DXGI_FORMAT_R32G32_TYPELESS: return DXGI_FORMAT_R32G32_FLOAT;
	case DXGI_FORMAT_R10G10B10A2_TYPELESS: return DXGI_FORMAT_R10G10B10A2_UNORM;
	case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
	case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
	case DXGI_FORMAT_R16G16_TYPELESS: return DXGI_FORMAT_R16G16_FLOAT;
	case DXGI_FORMAT_R8G8_TYPELESS: return DXGI_FORMAT_R8G8_UNORM;
	case DXGI_FORMAT_R8_TYPELESS: return DXGI_FORMAT_R8_UNORM;
	default: return aResourceFormat;
	}
}

std::vector<uint8_t> MakeDds(uint32_t aFormat, uint32_t aWidth, uint32_t aHeight, const uint8_t* aData, size_t aPitch)
{
	const uint32_t       rowBytes = aWidth * BytesPerTexel(aFormat);
	std::vector<uint8_t> out;
	out.reserve(148 + size_t(rowBytes) * aHeight);
	const char magic[] = "DDS ";
	out.insert(out.end(), magic, magic + 4);
	Put32(out, 124);                      // header size
	Put32(out, 0x1 | 0x2 | 0x4 | 0x8 | 0x1000);  // caps, height, width, pitch, pixel format
	Put32(out, aHeight);
	Put32(out, aWidth);
	Put32(out, rowBytes);
	Put32(out, 0);  // depth
	Put32(out, 0);  // mip count
	for (int i = 0; i < 11; ++i)
	{
		Put32(out, 0);
	}
	Put32(out, 32);   // pixel format size
	Put32(out, 0x4);  // a four-character code follows
	out.insert(out.end(), { 'D', 'X', '1', '0' });
	for (int i = 0; i < 5; ++i)
	{
		Put32(out, 0);
	}
	Put32(out, 0x1000);  // a texture
	for (int i = 0; i < 4; ++i)
	{
		Put32(out, 0);
	}
	Put32(out, aFormat);
	Put32(out, 3);  // a 2D texture
	Put32(out, 0);
	Put32(out, 1);  // one array slice
	Put32(out, 0);
	for (uint32_t row = 0; row < aHeight; ++row)
	{
		const uint8_t* from = aData + row * aPitch;
		out.insert(out.end(), from, from + rowBytes);
	}
	return out;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/world/Structures.cpp
)
```
```cmake
	src/world/Structures.cpp
	src/log/DdsWriter.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `72 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: DDS writer" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" \
  && git log --oneline -1
```

---

### Task 17: The frame log's data and `frame.json`

`FrameLogData` holds what a frame log records:
- the device report and the script camera;
- every constant, tag and evaluate Streamline was given;
- the events in order, and every submission;
- each tracked list's passes, and the G-buffer candidates.

`FrameLogJson` writes it as `frame.json` (the spec's "The frame log"), with DXGI format names spelled out.

**Files:**
- Create: `src/log/FrameLogData.h`, `src/log/FrameLogJson.cpp`, `tests/TestFrameLogJson.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestFrameLogJson.cpp`**

```cpp
#include "Harness.h"

#include "log/FrameLogData.h"

#include <dxgiformat.h>

using cpmc::FrameLogData;

namespace
{
bool Has(const std::string& aText, const std::string& aPart)
{
	const bool found = aText.find(aPart) != std::string::npos;
	if (!found)
	{
		std::printf("  missing: %s\n", aPart.c_str());
	}
	return found;
}

FrameLogData SampleLog()
{
	FrameLogData log;
	log.pluginVersion = "0.1.0";
	log.time = "2026-10-01T18:00:00";
	log.firstFrame = 100;
	log.lastFrame = 101;
	log.device.seenAtCreation = true;
	log.device.device = 0xABC;
	log.device.gpuApiDevice = "proxy";
	log.device.gpuApiMatches = true;
	log.device.streamline = "proxies";
	log.device.rtvs = 12;
	log.scriptCamera = cpmc::LogScriptCamera{ { 1.0, 2.0, 3.0 }, { 0.0f, 1.0f, 0.0f }, 68.25f };

	cpmc::LogConstants constants;
	constants.frame = 100;
	constants.constants.depthInverted = 1;
	constants.constants.cameraMotionIncluded = 0;
	constants.constants.fov = 1.0f;
	log.constants.push_back(constants);

	cpmc::LogTag depth;
	depth.frame = 100;
	depth.tag.type = 0;
	depth.tag.resource = reinterpret_cast<void*>(0x5000);
	depth.tag.state = 0x80;
	depth.tag.lifecycle = 1;
	depth.tag.extent = { 0, 0, 1707, 960 };
	depth.resource = { true, 3, DXGI_FORMAT_R32G8X24_TYPELESS, 2560, 1440, 1, 1, 0x2 };
	log.tags.push_back(depth);

	log.evaluates.push_back({ 100, 0, 0, 0x7000, 0x7100 });
	log.events.push_back({ 1.23456, 100, 42, "evaluate", "list 0x7000" });
	log.submissions.push_back({ 2.0, 100, 0x9000, { 0x7100, 0x7200 }, 1 });

	cpmc::PassRecord gbuffer;
	for (uintptr_t i = 1; i <= 3; ++i)
	{
		cpmc::ViewInfo view;
		view.resource = reinterpret_cast<void*>(i);
		view.viewFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
		view.width = 2560;
		view.height = 1440;
		gbuffer.targets.rtvs.push_back({ i * 16, view });
	}
	cpmc::ViewInfo dsv;
	dsv.kind = cpmc::ViewInfo::Kind::Dsv;
	dsv.resource = reinterpret_cast<void*>(0x5000);
	dsv.viewFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	gbuffer.targets.dsv = cpmc::BoundTarget{ 0x900, dsv };
	gbuffer.indexedDraws = 812;
	gbuffer.events.push_back("GBuffer");
	cpmc::PassRecord unknown;
	unknown.targets.rtvs.push_back({ 0x123, std::nullopt });
	log.lists.push_back({ 0x7100, "Scene", 100, { gbuffer, unknown } });
	log.gbufferCandidates.push_back({ 0x7100, 0 });
	return log;
}
}

TEST(FrameLogNamesFormats)
{
	CHECK_STR(cpmc::DxgiFormatName(DXGI_FORMAT_R10G10B10A2_UNORM), "R10G10B10A2_UNORM");
	CHECK_STR(cpmc::DxgiFormatName(DXGI_FORMAT_D32_FLOAT_S8X24_UINT), "D32_FLOAT_S8X24_UINT");
	CHECK_STR(cpmc::DxgiFormatName(DXGI_FORMAT_B4G4R4A4_UNORM), "B4G4R4A4_UNORM");
	CHECK_STR(cpmc::DxgiFormatName(500), "format 500");
}

TEST(FrameLogWritesEverySection)
{
	const std::string json = cpmc::FrameLogJson(SampleLog());
	CHECK(Has(json, "\"pluginVersion\": \"0.1.0\""));
	CHECK(Has(json, "\"frames\": [\n    100,\n    101\n  ]"));
	CHECK(Has(json, "\"seenAtCreation\": true"));
	CHECK(Has(json, "\"device\": \"proxy\""));
	CHECK(Has(json, "\"streamline\": \"proxies\""));
	CHECK(Has(json, "\"rtvs\": 12"));
	CHECK(Has(json, "\"fov\": 68.25"));
	CHECK(Has(json, "\"depthInverted\": \"true\""));
	CHECK(Has(json, "\"cameraMotionIncluded\": \"false\""));
	CHECK(Has(json, "\"motionVectors3D\": \"unset\""));
	CHECK(Has(json, "\"fovDegrees\": 57.29577951308232"));
	CHECK(Has(json, "\"name\": \"depth\""));
	CHECK(Has(json, "\"format\": \"R32G8X24_TYPELESS\""));
	CHECK(Has(json, "\"state\": \"0x80\""));
	CHECK(Has(json, "\"lifecycle\": \"validUntilPresent\""));
	CHECK(Has(json, "\"nativeList\": \"0x7100\""));
	CHECK(Has(json, "\"ms\": 1.235"));
	CHECK(Has(json, "\"what\": \"evaluate\""));
	CHECK(Has(json, "\"untracked\": 1"));
	CHECK(Has(json, "\"name\": \"Scene\""));
	CHECK(Has(json, "\"format\": \"R10G10B10A2_UNORM\""));
	CHECK(Has(json, "\"depthIsTagged\": true"));
	CHECK(Has(json, "\"indexedDraws\": 812"));
	CHECK(Has(json, "\"unknown\": true"));
	CHECK(Has(json, "\"gbufferCandidates\": [\n    {\n      \"list\": \"0x7100\",\n      \"pass\": 0\n    }\n  ]"));
	CHECK_EQ(json.back(), '\n');
}

TEST(FrameLogWritesNullWithoutAScriptCamera)
{
	FrameLogData log = SampleLog();
	log.scriptCamera.reset();
	CHECK(Has(cpmc::FrameLogJson(log), "\"scriptCamera\": null"));
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestDds.cpp
)
```
```cmake
	tests/TestDds.cpp
	tests/TestFrameLogJson.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'log/FrameLogData.h'`.

- [x] **Step 4: Write `src/log/FrameLogData.h`**

```cpp
#pragma once

#include "Coords.h"
#include "sl/Sl.h"
#include "track/StateTracker.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cpmc
{
// What a frame log holds (the spec's "The frame log"). The plugin fills it in while it watches two frames; the main
// thread then writes it out as frame.json.

// A resource's description, from ID3D12Resource::GetDesc.
struct ResourceInfo
{
	bool     known = false;
	uint32_t dimension = 0;  // D3D12_RESOURCE_DIMENSION
	uint32_t format = 0;     // DXGI_FORMAT
	uint64_t width = 0;
	uint32_t height = 0;
	uint32_t depthOrArraySize = 0;
	uint32_t mips = 0;
	uint32_t flags = 0;  // D3D12_RESOURCE_FLAGS
};

struct LogEvent
{
	double      ms = 0.0;  // since the log began
	uint32_t    frame = 0;
	uint32_t    thread = 0;
	std::string what;    // frameToken, setConstants, setTag, evaluate, submit or present
	std::string detail;
};

struct LogConstants
{
	uint32_t    frame = 0;
	uint32_t    viewport = 0;
	SlConstants constants;
};

struct LogTag
{
	uint32_t     frame = 0;
	uint32_t     viewport = 0;
	SlTag        tag;
	ResourceInfo resource;
	uint64_t     list = 0;  // the command list passed with it, if any
};

struct LogEvaluate
{
	uint32_t frame = 0;
	uint32_t feature = 0;
	uint32_t viewport = 0;
	uint64_t list = 0;        // as the game passed it
	uint64_t nativeList = 0;  // the D3D12 list underneath, if it was a Streamline proxy
};

struct LogList
{
	uint64_t                list = 0;
	std::string             name;  // the game's name for it, if found
	uint32_t                frame = 0;
	std::vector<PassRecord> passes;
};

struct LogSubmission
{
	double                ms = 0.0;
	uint32_t              frame = 0;
	uint64_t              queue = 0;
	std::vector<uint64_t> lists;
	uint32_t              untracked = 0;  // lists the tracker hadn't followed from their Reset
};

struct LogDevice
{
	bool        seenAtCreation = false;
	uint64_t    device = 0;
	std::string gpuApiDevice;  // native, proxy, other, or missing
	bool        gpuApiMatches = false;
	std::string gpuApiQueue;
	std::string gpuApiSwapChain;
	std::string streamline;  // proxies, manual or unknown
	size_t      rtvs = 0;
	size_t      dsvs = 0;
	size_t      unknownHandles = 0;
	size_t      vtableEntries = 0;
	size_t      imports = 0;
};

struct LogScriptCamera
{
	Vec3d position;
	Vec3f forward;
	float fov = 0.0f;
};

struct LogCandidate
{
	uint64_t list = 0;
	size_t   pass = 0;
};

struct FrameLogData
{
	std::string                    pluginVersion;
	std::string                    time;
	uint32_t                       firstFrame = 0;
	uint32_t                       lastFrame = 0;
	LogDevice                      device;
	std::optional<LogScriptCamera> scriptCamera;
	std::vector<LogConstants>      constants;
	std::vector<LogTag>            tags;  // global (slSetTag) and local (slEvaluateFeature) tags
	std::vector<LogEvaluate>       evaluates;
	std::vector<LogEvent>          events;
	std::vector<LogSubmission>     submissions;
	std::vector<LogList>           lists;
	std::vector<LogCandidate>      gbufferCandidates;
};

// The name dxgiformat.h gives a format, without the DXGI_FORMAT_ prefix, or "format <n>".
std::string DxgiFormatName(uint32_t aFormat);

// frame.json, pretty-printed.
std::string FrameLogJson(const FrameLogData& aLog);
}
```

- [x] **Step 5: Write `src/log/FrameLogJson.cpp`**

```cpp
#include "log/FrameLogData.h"

#include "Json.h"

#include <cmath>
#include <format>
#include <iterator>
#include <numbers>

namespace cpmc
{
namespace
{
// dxgiformat.h's names for formats 0 to 115, which covers every format a game renders with.
constexpr const char* kDxgiNames[] = {
	"UNKNOWN", "R32G32B32A32_TYPELESS", "R32G32B32A32_FLOAT", "R32G32B32A32_UINT", "R32G32B32A32_SINT",
	"R32G32B32_TYPELESS", "R32G32B32_FLOAT", "R32G32B32_UINT", "R32G32B32_SINT", "R16G16B16A16_TYPELESS",
	"R16G16B16A16_FLOAT", "R16G16B16A16_UNORM", "R16G16B16A16_UINT", "R16G16B16A16_SNORM", "R16G16B16A16_SINT",
	"R32G32_TYPELESS", "R32G32_FLOAT", "R32G32_UINT", "R32G32_SINT", "R32G8X24_TYPELESS", "D32_FLOAT_S8X24_UINT",
	"R32_FLOAT_X8X24_TYPELESS", "X32_TYPELESS_G8X24_UINT", "R10G10B10A2_TYPELESS", "R10G10B10A2_UNORM",
	"R10G10B10A2_UINT", "R11G11B10_FLOAT", "R8G8B8A8_TYPELESS", "R8G8B8A8_UNORM", "R8G8B8A8_UNORM_SRGB",
	"R8G8B8A8_UINT", "R8G8B8A8_SNORM", "R8G8B8A8_SINT", "R16G16_TYPELESS", "R16G16_FLOAT", "R16G16_UNORM",
	"R16G16_UINT", "R16G16_SNORM", "R16G16_SINT", "R32_TYPELESS", "D32_FLOAT", "R32_FLOAT", "R32_UINT", "R32_SINT",
	"R24G8_TYPELESS", "D24_UNORM_S8_UINT", "R24_UNORM_X8_TYPELESS", "X24_TYPELESS_G8_UINT", "R8G8_TYPELESS",
	"R8G8_UNORM", "R8G8_UINT", "R8G8_SNORM", "R8G8_SINT", "R16_TYPELESS", "R16_FLOAT", "D16_UNORM", "R16_UNORM",
	"R16_UINT", "R16_SNORM", "R16_SINT", "R8_TYPELESS", "R8_UNORM", "R8_UINT", "R8_SNORM", "R8_SINT", "A8_UNORM",
	"R1_UNORM", "R9G9B9E5_SHAREDEXP", "R8G8_B8G8_UNORM", "G8R8_G8B8_UNORM", "BC1_TYPELESS", "BC1_UNORM",
	"BC1_UNORM_SRGB", "BC2_TYPELESS", "BC2_UNORM", "BC2_UNORM_SRGB", "BC3_TYPELESS", "BC3_UNORM", "BC3_UNORM_SRGB",
	"BC4_TYPELESS", "BC4_UNORM", "BC4_SNORM", "BC5_TYPELESS", "BC5_UNORM", "BC5_SNORM", "B5G6R5_UNORM",
	"B5G5R5A1_UNORM", "B8G8R8A8_UNORM", "B8G8R8X8_UNORM", "R10G10B10_XR_BIAS_A2_UNORM", "B8G8R8A8_TYPELESS",
	"B8G8R8A8_UNORM_SRGB", "B8G8R8X8_TYPELESS", "B8G8R8X8_UNORM_SRGB", "BC6H_TYPELESS", "BC6H_UF16", "BC6H_SF16",
	"BC7_TYPELESS", "BC7_UNORM", "BC7_UNORM_SRGB", "AYUV", "Y410", "Y416", "NV12", "P010", "P016", "420_OPAQUE",
	"YUY2", "Y210", "Y216", "NV11", "AI44", "IA44", "P8", "A8P8", "B4G4R4A4_UNORM"
};

std::string Hex(uint64_t aValue)
{
	return std::format("0x{:x}", aValue);
}

template<class T>
void Numbers(JsonWriter& aJson, const T& aValues)
{
	aJson.BeginArray();
	for (const auto value : aValues)
	{
		aJson.Number(double(value));
	}
	aJson.EndArray();
}

const char* Boolean(uint8_t aValue)
{
	return aValue == 0 ? "false" : aValue == 1 ? "true" : "unset";
}

const char* Lifecycle(uint32_t aValue)
{
	switch (aValue)
	{
	case 0: return "onlyValidNow";
	case 1: return "validUntilPresent";
	case 2: return "validUntilEvaluate";
	default: return "unknown";
	}
}

void Target(JsonWriter& aJson, const BoundTarget& aTarget)
{
	aJson.BeginObject().Key("handle").String(Hex(aTarget.handle));
	if (aTarget.view)
	{
		const ViewInfo& view = *aTarget.view;
		aJson.Key("resource").String(Hex(reinterpret_cast<uintptr_t>(view.resource)));
		aJson.Key("format").String(DxgiFormatName(view.viewFormat));
		aJson.Key("size").BeginArray().UInt(view.width).UInt(view.height).EndArray();
		if (view.plane != 0 || view.mip != 0 || view.firstSlice != 0)
		{
			aJson.Key("subresource").BeginArray().UInt(view.mip).UInt(view.firstSlice).UInt(view.plane).EndArray();
		}
		if (view.readOnlyDepth || view.readOnlyStencil)
		{
			aJson.Key("readOnly").String(view.readOnlyDepth ? (view.readOnlyStencil ? "depth, stencil" : "depth") : "stencil");
		}
	}
	else
	{
		aJson.Key("unknown").Bool(true);
	}
	aJson.EndObject();
}

void Pass(JsonWriter& aJson, const PassRecord& aPass, const void* aTaggedDepth)
{
	aJson.BeginObject();
	aJson.Key("targets").BeginArray();
	for (const BoundTarget& target : aPass.targets.rtvs)
	{
		Target(aJson, target);
	}
	aJson.EndArray();
	aJson.Key("depth");
	if (aPass.targets.dsv)
	{
		Target(aJson, *aPass.targets.dsv);
	}
	else
	{
		aJson.Null();
	}
	const bool tagged = aPass.targets.dsv && aPass.targets.dsv->view && aTaggedDepth &&
	                    aPass.targets.dsv->view->resource == aTaggedDepth;
	aJson.Key("depthIsTagged").Bool(tagged);
	if (aPass.renderPass)
	{
		aJson.Key("renderPass").Bool(true);
	}
	aJson.Key("draws").UInt(aPass.draws);
	aJson.Key("indexedDraws").UInt(aPass.indexedDraws);
	aJson.Key("indirect").UInt(aPass.indirect);
	aJson.Key("bundles").UInt(aPass.bundles);
	aJson.Key("dispatches").UInt(aPass.dispatches);
	aJson.Key("clears").UInt(aPass.clears);
	aJson.Key("barriers").UInt(aPass.barriers);
	aJson.Key("enhancedBarriers").UInt(aPass.enhancedBarriers);
	aJson.Key("events").BeginArray();
	for (const std::string& event : aPass.events)
	{
		aJson.String(event);
	}
	aJson.EndArray();
	aJson.EndObject();
}

// The tagged depth buffer, from the log's latest depth tag.
const void* TaggedDepth(const FrameLogData& aLog)
{
	const void* depth = nullptr;
	for (const LogTag& tag : aLog.tags)
	{
		if (tag.tag.type == sl::kBufferTypeDepth && tag.tag.resource)
		{
			depth = tag.tag.resource;
		}
	}
	return depth;
}

void Device(JsonWriter& aJson, const LogDevice& aDevice)
{
	aJson.BeginObject();
	aJson.Key("seenAtCreation").Bool(aDevice.seenAtCreation);
	aJson.Key("device").String(Hex(aDevice.device));
	aJson.Key("gpuApi").BeginObject();
	aJson.Key("device").String(aDevice.gpuApiDevice);
	aJson.Key("matchesDetour").Bool(aDevice.gpuApiMatches);
	aJson.Key("queue").String(aDevice.gpuApiQueue);
	aJson.Key("swapChain").String(aDevice.gpuApiSwapChain);
	aJson.EndObject();
	aJson.Key("streamline").String(aDevice.streamline);
	aJson.Key("views").BeginObject();
	aJson.Key("rtvs").UInt(aDevice.rtvs).Key("dsvs").UInt(aDevice.dsvs).Key("unknownHandles").UInt(aDevice.unknownHandles);
	aJson.EndObject();
	aJson.Key("patches").BeginObject().Key("vtableEntries").UInt(aDevice.vtableEntries).Key("imports").UInt(aDevice.imports).EndObject();
	aJson.EndObject();
}

void Constants(JsonWriter& aJson, const LogConstants& aEntry)
{
	const SlConstants& c = aEntry.constants;
	aJson.BeginObject();
	aJson.Key("frame").UInt(aEntry.frame);
	aJson.Key("viewport").UInt(aEntry.viewport);
	aJson.Key("version").UInt(c.structVersion);
	aJson.Key("viewToClip");
	Numbers(aJson, c.viewToClip);
	aJson.Key("clipToView");
	Numbers(aJson, c.clipToView);
	aJson.Key("clipToPrevClip");
	Numbers(aJson, c.clipToPrevClip);
	aJson.Key("prevClipToClip");
	Numbers(aJson, c.prevClipToClip);
	aJson.Key("jitter");
	Numbers(aJson, c.jitter);
	aJson.Key("mvecScale");
	Numbers(aJson, c.mvecScale);
	aJson.Key("pinholeOffset");
	Numbers(aJson, c.pinholeOffset);
	aJson.Key("position");
	Numbers(aJson, c.position);
	aJson.Key("up");
	Numbers(aJson, c.up);
	aJson.Key("right");
	Numbers(aJson, c.right);
	aJson.Key("forward");
	Numbers(aJson, c.forward);
	aJson.Key("near").Number(c.nearPlane);
	aJson.Key("far").Number(c.farPlane);
	aJson.Key("fov").Number(c.fov);
	aJson.Key("fovDegrees").Number(c.fov * 180.0 / std::numbers::pi);
	aJson.Key("aspect").Number(c.aspect);
	aJson.Key("motionVectorsInvalidValue").Number(c.motionVectorsInvalidValue);
	aJson.Key("depthInverted").String(Boolean(c.depthInverted));
	aJson.Key("cameraMotionIncluded").String(Boolean(c.cameraMotionIncluded));
	aJson.Key("motionVectors3D").String(Boolean(c.motionVectors3D));
	aJson.Key("reset").String(Boolean(c.reset));
	aJson.Key("orthographic").String(Boolean(c.orthographic));
	aJson.Key("motionVectorsDilated").String(Boolean(c.motionVectorsDilated));
	aJson.Key("motionVectorsJittered").String(Boolean(c.motionVectorsJittered));
	aJson.EndObject();
}

void Tag(JsonWriter& aJson, const LogTag& aEntry)
{
	const SlTag& tag = aEntry.tag;
	aJson.BeginObject();
	aJson.Key("frame").UInt(aEntry.frame);
	aJson.Key("viewport").UInt(aEntry.viewport);
	aJson.Key("local").Bool(tag.local);
	aJson.Key("type").UInt(tag.type);
	aJson.Key("name").String(BufferTypeName(tag.type));
	aJson.Key("resource").String(Hex(reinterpret_cast<uintptr_t>(tag.resource)));
	if (aEntry.resource.known)
	{
		aJson.Key("format").String(DxgiFormatName(aEntry.resource.format));
		aJson.Key("size").BeginArray().UInt(aEntry.resource.width).UInt(aEntry.resource.height).EndArray();
		aJson.Key("flags").String(Hex(aEntry.resource.flags));
		aJson.Key("mips").UInt(aEntry.resource.mips);
		aJson.Key("depthOrArraySize").UInt(aEntry.resource.depthOrArraySize);
	}
	aJson.Key("state").String(Hex(tag.state));
	aJson.Key("lifecycle").String(Lifecycle(tag.lifecycle));
	aJson.Key("extent").BeginArray().UInt(tag.extent.top).UInt(tag.extent.left).UInt(tag.extent.width).UInt(tag.extent.height).EndArray();
	aJson.Key("list").String(Hex(aEntry.list));
	aJson.EndObject();
}

double Round3(double aValue)
{
	return std::round(aValue * 1000.0) / 1000.0;
}
}

std::string DxgiFormatName(uint32_t aFormat)
{
	if (aFormat < std::size(kDxgiNames))
	{
		return kDxgiNames[aFormat];
	}
	return std::format("format {}", aFormat);
}

std::string FrameLogJson(const FrameLogData& aLog)
{
	JsonWriter json;
	json.BeginObject();
	json.Key("pluginVersion").String(aLog.pluginVersion);
	json.Key("time").String(aLog.time);
	json.Key("frames").BeginArray().UInt(aLog.firstFrame).UInt(aLog.lastFrame).EndArray();
	json.Key("device");
	Device(json, aLog.device);

	json.Key("scriptCamera");
	if (aLog.scriptCamera)
	{
		const LogScriptCamera& camera = *aLog.scriptCamera;
		json.BeginObject();
		json.Key("position").BeginArray().Number(camera.position.x).Number(camera.position.y).Number(camera.position.z).EndArray();
		json.Key("forward").BeginArray().Number(camera.forward.x).Number(camera.forward.y).Number(camera.forward.z).EndArray();
		json.Key("fov").Number(camera.fov);
		json.EndObject();
	}
	else
	{
		json.Null();
	}

	json.Key("constants").BeginArray();
	for (const LogConstants& entry : aLog.constants)
	{
		Constants(json, entry);
	}
	json.EndArray();

	json.Key("tags").BeginArray();
	for (const LogTag& entry : aLog.tags)
	{
		Tag(json, entry);
	}
	json.EndArray();

	json.Key("evaluates").BeginArray();
	for (const LogEvaluate& evaluate : aLog.evaluates)
	{
		json.BeginObject();
		json.Key("frame").UInt(evaluate.frame);
		json.Key("feature").UInt(evaluate.feature);
		json.Key("viewport").UInt(evaluate.viewport);
		json.Key("list").String(Hex(evaluate.list));
		json.Key("nativeList").String(Hex(evaluate.nativeList));
		json.EndObject();
	}
	json.EndArray();

	json.Key("events").BeginArray();
	for (const LogEvent& event : aLog.events)
	{
		json.BeginObject();
		json.Key("ms").Number(Round3(event.ms));
		json.Key("frame").UInt(event.frame);
		json.Key("thread").UInt(event.thread);
		json.Key("what").String(event.what);
		json.Key("detail").String(event.detail);
		json.EndObject();
	}
	json.EndArray();

	json.Key("submissions").BeginArray();
	for (const LogSubmission& submission : aLog.submissions)
	{
		json.BeginObject();
		json.Key("ms").Number(Round3(submission.ms));
		json.Key("frame").UInt(submission.frame);
		json.Key("queue").String(Hex(submission.queue));
		json.Key("lists").BeginArray();
		for (const uint64_t list : submission.lists)
		{
			json.String(Hex(list));
		}
		json.EndArray();
		json.Key("untracked").UInt(submission.untracked);
		json.EndObject();
	}
	json.EndArray();

	const void* taggedDepth = TaggedDepth(aLog);
	json.Key("lists").BeginArray();
	for (const LogList& list : aLog.lists)
	{
		json.BeginObject();
		json.Key("list").String(Hex(list.list));
		json.Key("name").String(list.name);
		json.Key("frame").UInt(list.frame);
		json.Key("passes").BeginArray();
		for (const PassRecord& pass : list.passes)
		{
			Pass(json, pass, taggedDepth);
		}
		json.EndArray();
		json.EndObject();
	}
	json.EndArray();

	json.Key("gbufferCandidates").BeginArray();
	for (const LogCandidate& candidate : aLog.gbufferCandidates)
	{
		json.BeginObject().Key("list").String(Hex(candidate.list)).Key("pass").UInt(candidate.pass).EndObject();
	}
	json.EndArray();
	json.EndObject();
	return json.Str() + "\n";
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/log/DdsWriter.cpp
)
```
```cmake
	src/log/DdsWriter.cpp
	src/log/FrameLogJson.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `75 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: frame log data and frame.json" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 18: The shaders

`shaders/blocks.hlsl` holds the block shaders for step 2, which the plugin compiles in the game with D3DCompile
(shader model 5.1). They read everything through root buffer views, so they need no descriptor heap:
- **the vertex shader** reads SkyCraft's vertices from a buffer;
- **the pixel shader** samples the atlas (the nearest texel of the nearest mip level), applies Minecraft's face
  brightness, and discards where the game's copied depth is nearer.

`FrameConstants.h` mirrors the constant buffers, and the tests check its sizes against the compiled shader's
reflection. `TestWinding` checks that faces toward the camera wind counter-clockwise in the projections Task 11
handles, so back-face culling keeps the right faces.

**Files:**
- Create: `src/render/FrameConstants.h`, `src/render/Shaders.h`, `src/render/Shaders.cpp`, `shaders/blocks.hlsl`,
  `tests/TestShaders.cpp`, `tests/TestWinding.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestShaders.cpp`**

```cpp
#include "Harness.h"

#include "render/FrameConstants.h"
#include "render/Shaders.h"

#include <Windows.h>
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <fstream>
#include <sstream>

namespace
{
// The tests run in spikes/block-render, so the real shader file is here.
std::string ShaderSource()
{
	std::ifstream     file("shaders/blocks.hlsl", std::ios::binary);
	std::stringstream text;
	text << file.rdbuf();
	return text.str();
}

UINT ConstantBufferSize(const std::vector<uint8_t>& aCode, const char* aName)
{
	Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection;
	if (FAILED(D3DReflect(aCode.data(), aCode.size(), IID_PPV_ARGS(&reflection))))
	{
		return 0;
	}
	D3D12_SHADER_BUFFER_DESC desc{};
	if (FAILED(reflection->GetConstantBufferByName(aName)->GetDesc(&desc)))
	{
		return 0;
	}
	return desc.Size;
}
}

TEST(ShadersCompileFromTheShaderFile)
{
	const std::string source = ShaderSource();
	CHECK(!source.empty());
	std::string error;
	const auto  shaders = cpmc::CompileBlockShaders(source, error);
	CHECK_STR(error, "");
	CHECK(shaders.has_value());
	if (shaders)
	{
		CHECK(!shaders->vertex.empty());
		CHECK(!shaders->present.empty());
		// The constant buffers match the C++ structs byte for byte.
		CHECK_EQ(ConstantBufferSize(shaders->vertex, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->vertex, "Section"), UINT(sizeof(cpmc::SectionConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->present, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
	}
}

TEST(ShadersReportCompileErrors)
{
	std::string error;
	CHECK(!cpmc::CompileShader("float4 Broken() : SV_Target0 { return nope; }", "broken.hlsl", "Broken", "ps_5_1", error));
	CHECK(error.find("broken.hlsl(1") != std::string::npos);
	CHECK(error.find("nope") != std::string::npos);
}
```

- [x] **Step 2: Write `tests/TestWinding.cpp`**

```cpp
#include "Harness.h"

#include "AtlasFixture.h"
#include "render/Camera.h"
#include "world/Mesher.h"

// The mesher's faces, through the camera, must wind counter-clockwise in clip space (x right, y up) when they face
// the camera. On screen, with y down, that's clockwise: Direct3D's default front face, so back-face culling keeps them.
namespace
{
double SignedArea(const std::array<double, 3>& aA, const std::array<double, 3>& aB, const std::array<double, 3>& aC)
{
	return (aB[0] - aA[0]) * (aC[1] - aA[1]) - (aC[0] - aA[0]) * (aB[1] - aA[1]);
}

void CheckWinding(const std::array<float, 16>& aViewToClip)
{
	// The camera at world (0.5, 0, 0.5), looking north (+y), +z up.
	cpmc::SlConstants constants;
	constants.viewToClip = aViewToClip;
	constants.position = { 0.5f, 0.0f, 0.5f };
	constants.forward = { 0.0f, 1.0f, 0.0f };
	constants.right = { 1.0f, 0.0f, 0.0f };
	constants.up = { 0.0f, 0.0f, 1.0f };
	const auto frame = cpmc::MakeCameraFrame(constants, {});
	CHECK(frame.has_value());
	if (!frame)
	{
		return;
	}
	// Minecraft block (0, 0, -10) fills world x 0..1, y 9..10, z 0..1: ahead of the camera.
	cpmc::BlockWorld world;
	const cpmc::Atlas atlas = fixture::Atlas();
	world.Set({ 0, 0, -10 }, *atlas.BlockId("stone"));
	const cpmc::SectionPos section{ 0, 0, -1 };
	const auto             vertices = cpmc::MeshSection(world, section, atlas, {});
	for (size_t first = 0; first < vertices.size(); first += 3)
	{
		std::array<std::array<double, 3>, 3> ndc{};
		for (size_t k = 0; k < 3; ++k)
		{
			const cpmc::Vertex& v = vertices[first + k];
			const cpmc::Vec3d   worldPos = cpmc::WorldFromMc({ section.x * 16.0 + v.x, section.y * 16.0 + v.y, section.z * 16.0 + v.z });
			const auto          clip = cpmc::Transform({ worldPos.x - frame->origin.x, worldPos.y - frame->origin.y, worldPos.z - frame->origin.z, 1.0 }, frame->cameraToClip);
			ndc[k] = { clip[0] / clip[3], clip[1] / clip[3], clip[2] / clip[3] };
		}
		const auto face = cpmc::Face(((vertices[first].flags >> 4) & 15) - 1);
		const double area = SignedArea(ndc[0], ndc[1], ndc[2]);
		if (face == cpmc::Face::South)  // world -y: towards the camera
		{
			CHECK(area > 0.0);
		}
		if (face == cpmc::Face::North)  // away from the camera
		{
			CHECK(area < 0.0);
		}
	}
}
}

TEST(WindingFacesTheCameraWithALeftHandedProjection)
{
	CheckWinding({ 0.8f, 0, 0, 0, 0, 1.4f, 0, 0, 0, 0, 0, 1, 0, 0, 0.1f, 0 });
}

TEST(WindingFacesTheCameraWithARightHandedColumnProjection)
{
	CheckWinding({ 0.8f, 0, 0, 0, 0, 1.4f, 0, 0, 0, 0, 0, 0.1f, 0, 0, -1, 0 });
}
```

- [x] **Step 3: Edit `CMakeLists.txt` to add both**

```cmake
	tests/TestFrameLogJson.cpp
)
```
```cmake
	tests/TestFrameLogJson.cpp
	tests/TestShaders.cpp
	tests/TestWinding.cpp
)
```

- [x] **Step 4: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'render/FrameConstants.h'`.

- [x] **Step 5: Write `src/render/FrameConstants.h`**

```cpp
#pragma once

#include <cstdint>

namespace cpmc
{
// shaders/blocks.hlsl's constant buffers, byte for byte. tests/TestShaders.cpp checks the sizes against the compiled
// shader's own reflection.
struct FrameConstants
{
	float    cameraToClip[16];  // camera-relative world position to clip, for row vectors
	float    target[4];         // width, height, 1 / width, 1 / height of the image drawn into
	float    depthExtent[4];    // left, top, width, height of the depth image within its buffer
	uint32_t depthInfo[4];      // floats per row of the depth copy; 1 if there is a copy; 1 if depth is reversed; 0
	uint32_t atlasInfo[4];      // width, height, levels; 0
	uint32_t atlasLevels[8];    // where each level's texels start
	float    tuning[4];         // brightness; depth tolerance; 0; 0
};
static_assert(sizeof(FrameConstants) == 176);

struct SectionConstants
{
	float offset[3];  // the section's minimum corner: camera-relative, world axes
	float unused;
};
static_assert(sizeof(SectionConstants) == 16);
}
```

- [x] **Step 6: Write `src/render/Shaders.h`**

```cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cpmc
{
// Compiles entry point aEntry of HLSL source aSource for aProfile (for example "vs_5_1") with D3DCompile, which
// System32's D3DCompiler_47.dll provides. aName labels the source in messages. Nothing, with the compiler's messages
// in aError, if it doesn't compile.
std::optional<std::vector<uint8_t>> CompileShader(std::string_view aSource, const char* aName, const char* aEntry,
                                                  const char* aProfile, std::string& aError);

// The shaders the plugin draws with.
struct BlockShaders
{
	std::vector<uint8_t> vertex;   // BlocksVS
	std::vector<uint8_t> present;  // PresentPS
};

// Every entry point of shaders/blocks.hlsl, from its text. Nothing, with the messages in aError, if any fails.
std::optional<BlockShaders> CompileBlockShaders(std::string_view aSource, std::string& aError);
}
```

- [x] **Step 7: Write `src/render/Shaders.cpp`**

```cpp
#include "render/Shaders.h"

#include <Windows.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

namespace cpmc
{
std::optional<std::vector<uint8_t>> CompileShader(std::string_view aSource, const char* aName, const char* aEntry,
                                                  const char* aProfile, std::string& aError)
{
	Microsoft::WRL::ComPtr<ID3DBlob> code;
	Microsoft::WRL::ComPtr<ID3DBlob> messages;
	const HRESULT result = D3DCompile(aSource.data(), aSource.size(), aName, nullptr, nullptr, aEntry, aProfile,
	                                  D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, &messages);
	if (FAILED(result) || !code)
	{
		aError = messages ? std::string(static_cast<const char*>(messages->GetBufferPointer()), messages->GetBufferSize())
		                  : "D3DCompile failed without saying why";
		while (!aError.empty() && (aError.back() == '\0' || aError.back() == '\n'))
		{
			aError.pop_back();
		}
		return std::nullopt;
	}
	const auto* bytes = static_cast<const uint8_t*>(code->GetBufferPointer());
	return std::vector<uint8_t>(bytes, bytes + code->GetBufferSize());
}

std::optional<BlockShaders> CompileBlockShaders(std::string_view aSource, std::string& aError)
{
	auto vertex = CompileShader(aSource, "blocks.hlsl", "BlocksVS", "vs_5_1", aError);
	if (!vertex)
	{
		return std::nullopt;
	}
	auto present = CompileShader(aSource, "blocks.hlsl", "PresentPS", "ps_5_1", aError);
	if (!present)
	{
		return std::nullopt;
	}
	return BlockShaders{ std::move(*vertex), std::move(*present) };
}
}
```

- [x] **Step 8: Write `shaders/blocks.hlsl`**

```hlsl
// BlockRender's shaders: Minecraft section vertices (SkyCraft's 32-byte format), read from a buffer and drawn into
// the game's frame. The plugin compiles this file in the game with D3DCompile (shader model 5.1); "reload" in
// blockrender.request compiles it again. src/render/FrameConstants.h mirrors the constant buffers.

struct Vertex
{
    float3 position;  // from the section's minimum corner, in Minecraft axes (x east, y up, z south)
    float2 uv;        // atlas coordinates
    uint   colour;    // RGBA8, red in the low byte: tint and ambient occlusion
    uint   light;     // block light in bits 0-7, sky light in bits 8-15
    uint   flags;     // bit 0 alpha-tested, bit 1 translucent, bits 4-7 the face's direction + 1
};

cbuffer Frame : register(b0)
{
    row_major float4x4 cameraToClip;  // camera-relative world position to clip
    float4 target;                    // width, height, 1 / width, 1 / height of the image drawn into
    float4 depthExtent;               // left, top, width, height of the depth image within its buffer
    uint4  depthInfo;                 // floats per row of the depth copy; 1 if there is a copy; 1 if depth is reversed
    uint4  atlasInfo;                 // width, height, levels
    uint4  atlasLevels[2];            // where each level's texels start
    float4 tuning;                    // brightness; depth tolerance
};

cbuffer Section : register(b1)
{
    float3 sectionOffset;  // the section's minimum corner: camera-relative, world axes
    float  sectionUnused;
};

StructuredBuffer<Vertex> vertices : register(t0);
StructuredBuffer<uint>   atlas    : register(t1);
StructuredBuffer<float>  depthCopy : register(t2);

struct VSOut
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 colour   : COLOR0;
    nointerpolation uint face : TEXCOORD1;
};

VSOut BlocksVS(uint id : SV_VertexID)
{
    const Vertex v = vertices[id];
    VSOut o;
    // Minecraft (x, y, z) is world (x, -z, y).
    const float3 world = sectionOffset + float3(v.position.x, -v.position.z, v.position.y);
    o.position = mul(float4(world, 1.0), cameraToClip);
    o.uv = v.uv;
    o.colour = float4(v.colour & 0xFF, (v.colour >> 8) & 0xFF, (v.colour >> 16) & 0xFF, v.colour >> 24) / 255.0;
    o.face = (v.flags >> 4) & 15;
    return o;
}

// The atlas texel nearest uv, in the mip level that uv's rate of change across the screen picks.
float4 SampleAtlas(float2 uv)
{
    const float2 size = float2(atlasInfo.x, atlasInfo.y);
    const float2 dx = ddx(uv * size);
    const float2 dy = ddy(uv * size);
    const float  lod = 0.5 * log2(max(max(dot(dx, dx), dot(dy, dy)), 1e-8));
    const uint   level = (uint)clamp(floor(lod + 0.5), 0.0, (float)atlasInfo.z - 1.0);
    const uint2  levelSize = max(uint2(atlasInfo.x >> level, atlasInfo.y >> level), uint2(1, 1));
    const uint2  texel = min((uint2)(uv * float2(levelSize)), levelSize - 1);
    const uint   start = atlasLevels[level / 4][level % 4];
    const uint   packed = atlas[start + texel.y * levelSize.x + texel.x];
    return float4(packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF, packed >> 24) / 255.0;
}

// Minecraft's brightness per face, by the flags' direction + 1: none, down, up, north, south, west, east.
static const float kFaceShade[7] = { 1.0, 0.5, 1.0, 0.8, 0.8, 0.6, 0.6 };

// Step 2: on the finished frame, before Present. Hidden where the game's depth (copied at the DLSS evaluate) says
// something nearer is there; blocks sort among themselves through a depth buffer of the plugin's own.
float4 PresentPS(VSOut i) : SV_Target0
{
    const float4 texel = SampleAtlas(i.uv);
    if (texel.a < 0.5)
    {
        discard;
    }
    if (depthInfo.y != 0)
    {
        const float2 at = depthExtent.xy + i.position.xy * target.zw * depthExtent.zw;
        const uint2  last = (uint2)(depthExtent.xy + depthExtent.zw) - 1;
        const uint2  where = min((uint2)at, last);
        const float  scene = depthCopy[where.y * depthInfo.x + where.x];
        const bool   hidden = depthInfo.z != 0 ? scene > i.position.z + tuning.y : scene < i.position.z - tuning.y;
        if (hidden)
        {
            discard;
        }
    }
    return float4(texel.rgb * i.colour.rgb * kFaceShade[min(i.face, 6)] * tuning.x, 1.0);
}
```

- [x] **Step 9: Edit `CMakeLists.txt` to add the source**

```cmake
	src/log/FrameLogJson.cpp
)
```
```cmake
	src/log/FrameLogJson.cpp
	src/render/Shaders.cpp
)
```

- [x] **Step 10: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `79 test(s), 0 failure(s)`.

- [x] **Step 11: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: block shaders, checked by reflection and winding tests" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 19: The status file

The plugin writes `status.txt` once a second into the overlay's folder (the spec's "Status and the overlay"), as
`key=value` lines. Task 27's overlay shows them.

**Files:**
- Create: `src/Status.h`, `src/Status.cpp`, `tests/TestStatus.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestStatus.cpp`**

```cpp
#include "Harness.h"

#include "Status.h"

TEST(StatusWritesOneLinePerItem)
{
	cpmc::StatusReport report;
	report.mode = "present";
	report.framesDrawn = 58;
	report.framesSkipped = 2;
	report.lastSkip = "no depth tag\nyet";
	report.sections = 1;
	report.blocks = 1;
	report.vertices = 36;
	report.gpuMsAverage = 0.05;
	report.gpuMsWorst = 0.08;
	report.streamlineFrames = 60;
	report.hooks = "device seen at creation";
	report.lastRequest = "place cube: placed at block (10, 7, -24)";
	report.structures.push_back({ "cube", { 10.0, 23.0, 7.0 }, { 11.0, 24.0, 8.0 } });
	CHECK_STR(cpmc::FormatStatus(report),
	          "mode=present\n"
	          "frames=58 drawn, 2 skipped (no depth tag yet), 0 with a late camera (last second)\n"
	          "blocks=1 in 1 sections, 36 vertices\n"
	          "gpu=0.050 ms average, 0.080 ms worst\n"
	          "frame=not measured\n"
	          "streamline=60 frames in the last second\n"
	          "hooks=device seen at creation\n"
	          "request=place cube: placed at block (10, 7, -24)\n"
	          "structure=cube 10.00 23.00 7.00 11.00 24.00 8.00\n");
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestWinding.cpp
)
```
```cmake
	tests/TestWinding.cpp
	tests/TestStatus.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'Status.h'`.

- [x] **Step 4: Write `src/Status.h`**

```cpp
#pragma once

#include "Coords.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cpmc
{
// What status.txt says (the spec's "Status and the overlay"). The plugin writes it once a second into the overlay's
// folder; tools/blockrender_hud/init.lua shows it.
struct StatusReport
{
	struct Structure
	{
		std::string name;
		Vec3d       worldMin;  // the box it fills, in world coordinates
		Vec3d       worldMax;
	};

	std::string            mode = "off";
	uint64_t               framesDrawn = 0;    // in the last second
	uint64_t               framesSkipped = 0;  // in the last second
	std::string            lastSkip;           // why the latest skipped frame was skipped
	uint64_t               lateCamera = 0;     // frames drawn with the previous frame's camera, in the last second
	size_t                 sections = 0;
	size_t                 blocks = 0;
	size_t                 vertices = 0;
	double                 gpuMsAverage = -1.0;  // below 0: not measured
	double                 gpuMsWorst = -1.0;
	double                 frameMsAverage = -1.0;
	double                 frameMsWorst = -1.0;
	uint32_t               streamlineFrames = 0;  // frames Streamline started in the last second
	std::string            hooks;
	std::string            lastRequest;
	std::vector<Structure> structures;
};

// Lines of "key=value": mode, frames, blocks, gpu, frame, streamline, hooks, request, then one structure line per
// structure: "structure=<name> <min x> <min y> <min z> <max x> <max y> <max z>".
std::string FormatStatus(const StatusReport& aReport);
}
```

- [x] **Step 5: Write `src/Status.cpp`**

```cpp
#include "Status.h"

#include <format>

namespace cpmc
{
namespace
{
std::string Ms(double aAverage, double aWorst)
{
	if (aAverage < 0.0)
	{
		return "not measured";
	}
	return std::format("{:.3f} ms average, {:.3f} ms worst", aAverage, aWorst);
}

std::string OneLine(std::string aText)
{
	for (char& ch : aText)
	{
		if (ch == '\n' || ch == '\r')
		{
			ch = ' ';
		}
	}
	return aText;
}
}

std::string FormatStatus(const StatusReport& aReport)
{
	std::string out;
	out += std::format("mode={}\n", aReport.mode);
	out += std::format("frames={} drawn, {} skipped{}, {} with a late camera (last second)\n", aReport.framesDrawn,
	                   aReport.framesSkipped, aReport.lastSkip.empty() ? "" : " (" + OneLine(aReport.lastSkip) + ")",
	                   aReport.lateCamera);
	out += std::format("blocks={} in {} sections, {} vertices\n", aReport.blocks, aReport.sections, aReport.vertices);
	out += std::format("gpu={}\n", Ms(aReport.gpuMsAverage, aReport.gpuMsWorst));
	out += std::format("frame={}\n", Ms(aReport.frameMsAverage, aReport.frameMsWorst));
	out += std::format("streamline={} frames in the last second\n", aReport.streamlineFrames);
	out += std::format("hooks={}\n", OneLine(aReport.hooks));
	out += std::format("request={}\n", OneLine(aReport.lastRequest));
	for (const StatusReport::Structure& structure : aReport.structures)
	{
		out += std::format("structure={} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f}\n", structure.name, structure.worldMin.x,
		                   structure.worldMin.y, structure.worldMin.z, structure.worldMax.x, structure.worldMax.y,
		                   structure.worldMax.z);
	}
	return out;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/render/Shaders.cpp
)
```
```cmake
	src/render/Shaders.cpp
	src/Status.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `80 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: status file" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" \
  && git log --oneline -1
```

---

### Task 20: Event names

The game names its GPU work with PIX events (`BeginEvent` and `SetMarker`). The frame log keeps the names, so its
passes say what they are. Names can be UTF-16 (metadata 0), ANSI (metadata 1), or a PIX blob, which is searched for
printable text.

**Files:**
- Create: `src/log/EventNames.h`, `src/log/EventNames.cpp`, `tests/TestEventNames.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestEventNames.cpp`**

```cpp
#include "Harness.h"

#include "log/EventNames.h"

#include <cstdint>
#include <cstring>
#include <string>

using cpmc::EventName;

TEST(EventNamesReadUnicodeAndAnsi)
{
	const wchar_t wide[] = L"GBuffer";
	CHECK_STR(EventName(0, wide, sizeof(wide)), "GBuffer");
	const char ansi[] = "Lighting";
	CHECK_STR(EventName(1, ansi, sizeof(ansi)), "Lighting");
	const wchar_t accented[] = L"Café";
	CHECK_STR(EventName(0, accented, sizeof(accented)), "Caf?");
}

TEST(EventNamesFindTextInBlobs)
{
	// A PIX 3 blob: 64-bit header and colour words, then the name packed into the next words.
	uint8_t blob[48]{};
	blob[0] = 0x01;
	blob[8] = 0xFF;
	std::memcpy(blob + 16, "DepthPrepass", 12);
	CHECK_STR(EventName(2, blob, sizeof(blob)), "DepthPrepass");

	uint8_t wide[32]{};
	const char name[] = "SSAO";
	for (size_t i = 0; i < 4; ++i)
	{
		wide[8 + 2 * i] = uint8_t(name[i]);
	}
	CHECK_STR(EventName(2, wide, sizeof(wide)), "SSAO");
}

TEST(EventNamesHandleNothingAndLongNames)
{
	CHECK_STR(EventName(0, nullptr, 10), "<event>");
	const uint8_t zeros[16]{};
	CHECK_STR(EventName(2, zeros, sizeof(zeros)), "<event>");
	const std::string longName(100, 'x');
	CHECK_EQ(EventName(1, longName.c_str(), uint32_t(longName.size() + 1)).size(), size_t(64));
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestStatus.cpp
)
```
```cmake
	tests/TestStatus.cpp
	tests/TestEventNames.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'log/EventNames.h'`.

- [x] **Step 4: Write `src/log/EventNames.h`**

```cpp
#pragma once

#include <cstdint>
#include <string>

namespace cpmc
{
// The text of a PIX event or marker (ID3D12GraphicsCommandList::BeginEvent and SetMarker): metadata 0 is a UTF-16
// string, 1 an ANSI string, and anything else (PIX 3's packed blobs) is searched for runs of printable text. Non-ASCII
// characters become '?'. At most 64 characters; "<event>" when there's no text to find.
std::string EventName(uint32_t aMetadata, const void* aData, uint32_t aSize);
}
```

- [x] **Step 5: Write `src/log/EventNames.cpp`**

```cpp
#include "log/EventNames.h"

#include <cstring>

namespace cpmc
{
namespace
{
constexpr size_t   kMaxBytes = 512;
constexpr size_t   kMaxChars = 64;
constexpr uint32_t kUnicode = 0;
constexpr uint32_t kAnsi = 1;

bool Printable(uint32_t aChar)
{
	return aChar >= 0x20 && aChar < 0x7F;
}

void Append(std::string& aOut, uint32_t aChar)
{
	if (aOut.size() < kMaxChars)
	{
		aOut.push_back(Printable(aChar) ? char(aChar) : '?');
	}
}

// Runs of at least three printable characters, read with aStride bytes per character (1, or 2 for UTF-16 whose
// high bytes are zero), joined by spaces.
std::string Runs(const uint8_t* aBytes, size_t aSize, size_t aStride)
{
	std::string out;
	std::string run;
	const auto  flush = [&] {
		if (run.size() >= 3)
		{
			if (!out.empty())
			{
				out.push_back(' ');
			}
			out += run;
		}
		run.clear();
	};
	for (size_t i = 0; i + aStride <= aSize; i += aStride)
	{
		const bool wide = aStride == 2;
		if (Printable(aBytes[i]) && (!wide || aBytes[i + 1] == 0))
		{
			run.push_back(char(aBytes[i]));
		}
		else
		{
			flush();
		}
	}
	flush();
	return out.substr(0, kMaxChars);
}
}

std::string EventName(uint32_t aMetadata, const void* aData, uint32_t aSize)
{
	if (!aData || aSize == 0)
	{
		return "<event>";
	}
	const auto*  bytes = static_cast<const uint8_t*>(aData);
	const size_t size = aSize < kMaxBytes ? aSize : kMaxBytes;
	std::string  out;
	if (aMetadata == kUnicode)
	{
		for (size_t i = 0; i + 1 < size; i += 2)
		{
			const uint32_t ch = uint32_t(bytes[i]) | (uint32_t(bytes[i + 1]) << 8);
			if (ch == 0)
			{
				break;
			}
			Append(out, ch);
		}
	}
	else if (aMetadata == kAnsi)
	{
		for (size_t i = 0; i < size && bytes[i] != 0; ++i)
		{
			Append(out, bytes[i]);
		}
	}
	else
	{
		out = Runs(bytes, size, 1);
		if (out.empty())
		{
			out = Runs(bytes, size, 2);
		}
	}
	return out.empty() ? "<event>" : out;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/Status.cpp
)
```
```cmake
	src/Status.cpp
	src/log/EventNames.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `83 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: PIX event names" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" \
  && git log --oneline -1
```

---

### Task 21: The atlas builder

`build_atlas.py` reads about 50 common blocks' textures from the user's own Minecraft 26.3 jar:
- it builds 5 mip levels for each sprite, packed 16 to a row;
- it writes `atlas.bin`, which the plugin reads (Task 12), and `atlas.json`, the same for people.

Grass tops and leaves get Minecraft's plains tints. The textures belong to Mojang, so the output goes into the game's
plugin folder and never into the repo. The tests build small jars of their own.

**Files:**
- Create: `tools/build_atlas.py`, `tools/test_build_atlas.py`

- [x] **Step 1: Write the failing test, `tools/test_build_atlas.py`**

```python
"""Tests for build_atlas.py, against a small fake jar.

    uv run --no-project --with pillow --with pytest pytest tools/test_build_atlas.py
"""
import io
import struct
import zipfile

import pytest
from PIL import Image

import build_atlas


def png(colour, size=(16, 16)):
    buffer = io.BytesIO()
    Image.new("RGBA", size, colour).save(buffer, "PNG")
    return buffer.getvalue()


def every_texture():
    names = set()
    for _, textures, _, _, _ in build_atlas.BLOCKS:
        names.update(build_atlas.face_textures(textures))
    return sorted(names)


def fake_jar(path, skip=(), overrides=None):
    overrides = overrides or {}
    with zipfile.ZipFile(path, "w") as jar:
        for index, name in enumerate(every_texture()):
            if name in skip:
                continue
            data = overrides.get(name) or png((index % 256, 100, 200, 255))
            jar.writestr(f"assets/minecraft/textures/block/{name}.png", data)
    return path


def read_atlas(path):
    data = (path / "atlas.bin").read_bytes()
    assert data[:8] == b"CPMCATL1"
    width, height, levels, sprite_count, block_count, zero = struct.unpack_from("<6I", data, 8)
    at = 32
    sprites = [struct.unpack_from("<4I", data, at + 16 * i) for i in range(sprite_count)]
    at += 16 * sprite_count
    blocks = []
    for i in range(block_count):
        record = data[at + 64 * i: at + 64 * (i + 1)]
        name = record[:32].rstrip(b"\0").decode()
        faces = struct.unpack_from("<6H", record, 32)
        blocks.append((name, faces, tuple(record[44:47]), record[47], record[48]))
    at += 64 * block_count
    return {"size": (width, height), "levels": levels, "zero": zero, "sprites": sprites, "blocks": blocks,
            "pixels": data[at:]}


def test_face_textures():
    assert build_atlas.face_textures("stone") == ["stone"] * 6
    assert build_atlas.face_textures({"end": "top", "side": "side"}) == ["top", "top", "side", "side", "side", "side"]
    assert build_atlas.face_textures({"end": "a", "side": "b", "north": "c"}) == ["a", "a", "c", "b", "b", "b"]
    with pytest.raises(ValueError):
        build_atlas.face_textures({"side": "b"})


def test_atlas_layout(tmp_path):
    jar = fake_jar(tmp_path / "fake.jar")
    summary = build_atlas.build(jar, tmp_path / "out")
    atlas = read_atlas(tmp_path / "out")
    assert atlas["zero"] == 0
    assert atlas["levels"] == 5
    assert atlas["size"] == (256, 16 * ((len(atlas["sprites"]) + 15) // 16))
    assert len(atlas["blocks"]) == len(build_atlas.BLOCKS)
    assert atlas["sprites"][1] == (16, 0, 16, 16)
    # Every level's texels follow the tables, each half the size of the one before.
    width, height = atlas["size"]
    expected = sum(max(1, width >> level) * max(1, height >> level) * 4 for level in range(5))
    assert len(atlas["pixels"]) == expected
    assert summary["blocks"][0]["name"] == "stone"


def test_blocks_name_their_faces_tints_and_cutouts(tmp_path):
    atlas = read_atlas_after_build(tmp_path)
    blocks = {name: (faces, tint, tinted, cutout) for name, faces, tint, tinted, cutout in atlas["blocks"]}
    names = [name for name, _, _, _, _ in build_atlas.BLOCKS]
    sprite_names = []
    for _, textures, _, _, _ in build_atlas.BLOCKS:
        for name in build_atlas.face_textures(textures):
            if name not in sprite_names:
                sprite_names.append(name)
    log_faces = blocks["oak_log"][0]
    assert sprite_names[log_faces[1]] == "oak_log_top"  # up
    assert sprite_names[log_faces[2]] == "oak_log"  # north
    grass = blocks["grass_block"]
    assert sprite_names[grass[0][0]] == "dirt"
    assert grass[1] == build_atlas.GRASS
    assert grass[2] == 0b000010  # only the top is tinted
    assert blocks["oak_leaves"][2] == 0b111111
    assert blocks["glass"][3] == 1 and blocks["stone"][3] == 0
    assert blocks["stone"][1] == (255, 255, 255)
    table = blocks["crafting_table"][0]
    assert [sprite_names[i] for i in table] == ["oak_planks", "crafting_table_top", "crafting_table_front",
                                                "crafting_table_side", "crafting_table_side", "crafting_table_front"]
    assert len(names) == len(set(names))


def read_atlas_after_build(tmp_path):
    jar = fake_jar(tmp_path / "fake.jar")
    build_atlas.build(jar, tmp_path / "out")
    return read_atlas(tmp_path / "out")


def test_mip_levels_average_each_sprite(tmp_path):
    # Stone: a 16 x 16 checkerboard of black and white averages to grey at level 1 and below.
    checker = Image.new("RGBA", (16, 16))
    checker.putdata([(255, 255, 255, 255) if (x + y) % 2 else (0, 0, 0, 255) for y in range(16) for x in range(16)])
    buffer = io.BytesIO()
    checker.save(buffer, "PNG")
    jar = fake_jar(tmp_path / "fake.jar", overrides={"stone": buffer.getvalue()})
    build_atlas.build(jar, tmp_path / "out")
    atlas = read_atlas(tmp_path / "out")
    width, height = atlas["size"]
    level0 = width * height * 4
    level1 = atlas["pixels"][level0:level0 + 4]  # texel (0, 0) of level 1: stone's top-left 2 x 2
    assert abs(level1[0] - 127) <= 1 and level1[3] == 255


def test_animated_textures_give_their_first_frame(tmp_path):
    frames = Image.new("RGBA", (16, 48), (10, 20, 30, 255))
    frames.paste(Image.new("RGBA", (16, 16), (200, 0, 0, 255)), (0, 0))
    buffer = io.BytesIO()
    frames.save(buffer, "PNG")
    jar = fake_jar(tmp_path / "fake.jar", overrides={"magma": buffer.getvalue()})
    summary = build_atlas.build(jar, tmp_path / "out")
    sprite = next(s for s in summary["sprites"] if s["name"] == "magma")
    atlas = read_atlas(tmp_path / "out")
    width = atlas["size"][0]
    at = (sprite["y"] * width + sprite["x"]) * 4
    assert tuple(atlas["pixels"][at:at + 4]) == (200, 0, 0, 255)


def test_a_missing_texture_is_named(tmp_path):
    jar = fake_jar(tmp_path / "fake.jar", skip=("glass",))
    with pytest.raises(SystemExit) as failure:
        build_atlas.build(jar, tmp_path / "out")
    assert "glass.png" in str(failure.value)
```

- [x] **Step 2: Run it; it must fail**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render/tools && uv run --no-project -q --with numpy --with pillow --with pytest pytest -q test_build_atlas.py 2>&1 | grep -E "^E |error in"
```
Expected: `E   ModuleNotFoundError: No module named 'build_atlas'`, then `1 error in ...`.

- [x] **Step 3: Write `tools/build_atlas.py`**

```python
"""Builds BlockRender's block atlas from your own Minecraft jar.

    uv run --no-project --with pillow tools/build_atlas.py <minecraft.jar> <output folder>

Writes atlas.bin, which the plugin reads, and atlas.json, the same for people. The textures belong to Mojang, so the
output goes into the game's plugin folder and never into the repo.

atlas.bin is little-endian:
- header, 32 bytes: magic "CPMCATL1", then uint32 width, height, levels, sprite count, block count, 0;
- sprites, 16 bytes each: uint32 x, y, w, h, in level-0 texels;
- blocks, 64 bytes each: char name[32] (zero-padded), uint16 sprite per face (down, up, north, south, west, east),
  uint8 tint red, green, blue, uint8 tinted faces (bit per face, same order), uint8 cut-out (1: has see-through
  texels), 15 zero bytes;
- the texels, RGBA8 row by row, level 0 first; level l is max(1, width >> l) by max(1, height >> l).
"""
import io
import json
import struct
import sys
import zipfile
from pathlib import Path

from PIL import Image

TILE = 16
COLUMNS = 16
LEVELS = 5  # 16, 8, 4, 2 and 1 texels per sprite
FACES = ("down", "up", "north", "south", "west", "east")
GRASS = (0x91, 0xBD, 0x59)  # Minecraft's plains grass colour
FOLIAGE = (0x77, 0xAB, 0x2F)  # Minecraft's plains foliage colour

# name, textures, tint, tinted faces, cut-out. Textures: one name for every face, or a dict with "end" (down and up),
# "side" (the four sides) and any single face, later keys winning.
BLOCKS = [
    ("stone", "stone", None, (), False),
    ("cobblestone", "cobblestone", None, (), False),
    ("mossy_cobblestone", "mossy_cobblestone", None, (), False),
    ("dirt", "dirt", None, (), False),
    ("grass_block", {"down": "dirt", "up": "grass_block_top", "side": "grass_block_side"}, GRASS, ("up",), False),
    ("sand", "sand", None, (), False),
    ("gravel", "gravel", None, (), False),
    ("clay", "clay", None, (), False),
    ("oak_planks", "oak_planks", None, (), False),
    ("spruce_planks", "spruce_planks", None, (), False),
    ("birch_planks", "birch_planks", None, (), False),
    ("oak_log", {"end": "oak_log_top", "side": "oak_log"}, None, (), False),
    ("oak_leaves", "oak_leaves", FOLIAGE, FACES, True),
    ("glass", "glass", None, (), True),
    ("bricks", "bricks", None, (), False),
    ("stone_bricks", "stone_bricks", None, (), False),
    ("white_wool", "white_wool", None, (), False),
    ("red_wool", "red_wool", None, (), False),
    ("blue_wool", "blue_wool", None, (), False),
    ("yellow_wool", "yellow_wool", None, (), False),
    ("lime_wool", "lime_wool", None, (), False),
    ("black_wool", "black_wool", None, (), False),
    ("iron_block", "iron_block", None, (), False),
    ("gold_block", "gold_block", None, (), False),
    ("diamond_block", "diamond_block", None, (), False),
    ("emerald_block", "emerald_block", None, (), False),
    ("lapis_block", "lapis_block", None, (), False),
    ("redstone_block", "redstone_block", None, (), False),
    ("coal_ore", "coal_ore", None, (), False),
    ("iron_ore", "iron_ore", None, (), False),
    ("gold_ore", "gold_ore", None, (), False),
    ("diamond_ore", "diamond_ore", None, (), False),
    ("deepslate", {"end": "deepslate_top", "side": "deepslate"}, None, (), False),
    ("granite", "granite", None, (), False),
    ("diorite", "diorite", None, (), False),
    ("andesite", "andesite", None, (), False),
    ("sandstone", {"down": "sandstone_bottom", "up": "sandstone_top", "side": "sandstone"}, None, (), False),
    ("obsidian", "obsidian", None, (), False),
    ("netherrack", "netherrack", None, (), False),
    ("glowstone", "glowstone", None, (), False),
    ("magma_block", "magma", None, (), False),
    ("snow_block", "snow", None, (), False),
    ("quartz_block", {"down": "quartz_block_bottom", "up": "quartz_block_top", "side": "quartz_block_side"}, None, (), False),
    ("bookshelf", {"end": "oak_planks", "side": "bookshelf"}, None, (), False),
    ("pumpkin", {"end": "pumpkin_top", "side": "pumpkin_side"}, None, (), False),
    ("tnt", {"down": "tnt_bottom", "up": "tnt_top", "side": "tnt_side"}, None, (), False),
    ("crafting_table", {"down": "oak_planks", "up": "crafting_table_top", "side": "crafting_table_side",
                        "north": "crafting_table_front", "east": "crafting_table_front"}, None, (), False),
    ("terracotta", "terracotta", None, (), False),
    ("white_concrete", "white_concrete", None, (), False),
    ("cyan_concrete", "cyan_concrete", None, (), False),
]


def face_textures(textures):
    """The texture name of each face, in FACES order."""
    if isinstance(textures, str):
        return [textures] * 6
    faces = {}
    for key, name in textures.items():
        targets = {"end": ("down", "up"), "side": ("north", "south", "west", "east")}.get(key, (key,))
        for face in targets:
            faces[face] = name
    missing = [face for face in FACES if face not in faces]
    if missing:
        raise ValueError(f"no texture for faces {missing} in {textures}")
    return [faces[face] for face in FACES]


def load_texture(jar, name):
    """A block texture as a 16 x 16 RGBA image; animated textures give their first frame."""
    path = f"assets/minecraft/textures/block/{name}.png"
    try:
        data = jar.read(path)
    except KeyError:
        raise SystemExit(f"error: {path} isn't in the jar") from None
    image = Image.open(io.BytesIO(data)).convert("RGBA")
    if image.width != TILE or image.height < TILE:
        raise SystemExit(f"error: {path} is {image.width} x {image.height}, not {TILE} wide and at least {TILE} high")
    return image.crop((0, 0, TILE, TILE))


def mip_levels(atlas):
    """Level 0 and its box-filtered halvings. Sprites sit on a 16-texel grid, so no level mixes two sprites."""
    levels = [atlas]
    for _ in range(1, LEVELS):
        previous = levels[-1]
        levels.append(previous.resize((max(1, previous.width // 2), max(1, previous.height // 2)), Image.Resampling.BOX))
    return levels


def build(jar_path, out_dir):
    out_dir = Path(out_dir)
    with zipfile.ZipFile(jar_path) as jar:
        sprite_names = []
        for _, textures, _, _, _ in BLOCKS:
            for name in face_textures(textures):
                if name not in sprite_names:
                    sprite_names.append(name)
        rows = (len(sprite_names) + COLUMNS - 1) // COLUMNS
        atlas = Image.new("RGBA", (COLUMNS * TILE, rows * TILE), (0, 0, 0, 0))
        sprites = []
        for index, name in enumerate(sprite_names):
            x, y = (index % COLUMNS) * TILE, (index // COLUMNS) * TILE
            atlas.paste(load_texture(jar, name), (x, y))
            sprites.append({"name": name, "x": x, "y": y, "w": TILE, "h": TILE})

    blocks = []
    for name, textures, tint, tinted, cutout in BLOCKS:
        faces = [sprite_names.index(texture) for texture in face_textures(textures)]
        blocks.append({
            "name": name,
            "faces": dict(zip(FACES, faces)),
            "tint": list(tint or (255, 255, 255)),
            "tintFaces": list(tinted),
            "cutout": cutout,
        })

    levels = mip_levels(atlas)
    out = bytearray(b"CPMCATL1")
    out += struct.pack("<6I", atlas.width, atlas.height, LEVELS, len(sprites), len(blocks), 0)
    for sprite in sprites:
        out += struct.pack("<4I", sprite["x"], sprite["y"], sprite["w"], sprite["h"])
    for block in blocks:
        tinted_bits = sum(1 << FACES.index(face) for face in block["tintFaces"])
        out += block["name"].encode("ascii").ljust(32, b"\0")
        out += struct.pack("<6H", *(block["faces"][face] for face in FACES))
        out += bytes(block["tint"]) + bytes([tinted_bits, 1 if block["cutout"] else 0]) + bytes(15)
    for level in levels:
        out += level.tobytes()

    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "atlas.bin").write_bytes(bytes(out))
    summary = {"width": atlas.width, "height": atlas.height, "levels": LEVELS, "sprites": sprites, "blocks": blocks}
    (out_dir / "atlas.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    return summary


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    summary = build(sys.argv[1], sys.argv[2])
    print(f"atlas: {summary['width']} x {summary['height']}, {len(summary['sprites'])} sprites, "
          f"{len(summary['blocks'])} blocks, written to {sys.argv[2]}")


if __name__ == "__main__":
    main()
```

- [x] **Step 4: Run the tests; they must pass**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render/tools && uv run --no-project -q --with numpy --with pillow --with pytest pytest -q test_build_atlas.py 2>&1 | tail -1
```
Expected: `6 passed`.

- [x] **Step 5: Try it on the user's jar, into a scratch folder**

```bash
OUT=$(mktemp -d) && cd /d/Cyberpunk-Minecraft/spikes/block-render \
  && uv run --no-project -q --with pillow python tools/build_atlas.py "$APPDATA/.minecraft/versions/26.3/26.3.jar" "$OUT" \
  && ls -l "$OUT"
```
Expected: `atlas: 256 x 64, 62 sprites, 50 blocks, written to ...`, then `atlas.bin` (91,520 bytes) and `atlas.json`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render/tools \
  && git commit -q -m "block-render: atlas builder" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" \
  && git log --oneline -1
```

---

### Task 22: The dump viewer and the frame log summary

- **`view_dump.py`** turns each dumped `.dds` into PNGs:
  - colour as it is, and HDR tone mapped;
  - depth stretched over its range;
  - motion vectors as a flow image;
  - stencil as a palette, with a count of each value.
- **`frame_summary.py`** prints what step 1 asks of a `frame.json`:
  - the device report;
  - Streamline's camera against the script camera, and the projection's layout;
  - each distinct tag;
  - the order of events, and every command list;
  - the G-buffer candidates.

**Files:**
- Create: `tools/view_dump.py`, `tools/test_view_dump.py`, `tools/frame_summary.py`, `tools/test_frame_summary.py`

- [x] **Step 1: Write the failing test, `tools/test_view_dump.py`**

```python
"""Tests for view_dump.py, with DDS files laid out as the plugin's DdsWriter writes them.

    uv run --no-project --with numpy --with pillow --with pytest pytest tools/test_view_dump.py
"""
import struct

import numpy as np
import pytest
from PIL import Image

import view_dump as vd


def dds(path, dxgi_format, width, height, texels):
    """A DDS file as src/log/DdsWriter.cpp writes one: packed rows after the DX10 header."""
    texels = bytes(texels)
    row_bytes = len(texels) // height
    header = bytearray(b"DDS ")
    header += struct.pack("<7I", 124, 0x100F, height, width, row_bytes, 0, 0) + bytes(44)
    header += struct.pack("<2I", 32, 4) + b"DX10" + bytes(20)
    header += struct.pack("<5I", 0x1000, 0, 0, 0, 0)
    header += struct.pack("<5I", dxgi_format, 3, 0, 1, 0)
    path.write_bytes(bytes(header) + texels)
    return path


def decoded(path):
    dxgi_format, width, height, rows = vd.read_dds(path)
    return vd.decode(dxgi_format, width, height, rows)


def test_rgba8_and_bgra8(tmp_path):
    values, kind = decoded(dds(tmp_path / "a.dds", vd.R8G8B8A8_UNORM, 2, 1, [255, 0, 0, 255, 0, 0, 255, 128]))
    assert kind == "colour"
    assert values[0, 0].tolist() == [1.0, 0.0, 0.0, 1.0]
    assert values[0, 1, 3] == pytest.approx(128 / 255)
    values, _ = decoded(dds(tmp_path / "b.dds", vd.B8G8R8A8_UNORM, 1, 1, [255, 0, 0, 255]))
    assert values[0, 0, :3].tolist() == [0.0, 0.0, 1.0]  # blue came first


def test_r10g10b10a2(tmp_path):
    word = 1023 | (512 << 10) | (0 << 20) | (3 << 30)
    values, _ = decoded(dds(tmp_path / "n.dds", vd.R10G10B10A2_UNORM, 1, 1, struct.pack("<I", word)))
    assert values[0, 0, 0] == pytest.approx(1.0)
    assert values[0, 0, 1] == pytest.approx(512 / 1023)
    assert values[0, 0, 2] == 0.0
    assert values[0, 0, 3] == 1.0


def test_r11g11b10_float(tmp_path):
    # 1.0 is exponent 15, mantissa 0 in both widths; 0.5 is exponent 14.
    one11 = 15 << 6
    half11 = 14 << 6
    two10 = 16 << 5
    word = one11 | (half11 << 11) | (two10 << 22)
    values, kind = decoded(dds(tmp_path / "h.dds", vd.R11G11B10_FLOAT, 1, 1, struct.pack("<I", word)))
    assert kind == "hdr"
    assert values[0, 0].tolist() == [1.0, 0.5, 2.0]


def test_half_floats(tmp_path):
    texels = np.array([[1.5, -2.0]], dtype="<f2").tobytes()
    values, kind = decoded(dds(tmp_path / "m.dds", vd.R16G16_FLOAT, 1, 1, texels))
    assert kind == "motion"
    assert values[0, 0].tolist() == [1.5, -2.0]
    hdr = np.array([[4.0, 0.0, 1.0, 1.0]], dtype="<f2").tobytes()
    values, kind = decoded(dds(tmp_path / "c.dds", vd.R16G16B16A16_FLOAT, 1, 1, hdr))
    assert kind == "hdr" and values[0, 0, 0] == 4.0


def test_depth_formats(tmp_path):
    values, kind = decoded(dds(tmp_path / "d.dds", vd.R32_FLOAT, 2, 1, np.array([0.25, 1.0], dtype="<f4").tobytes()))
    assert kind == "depth" and values[0, :, 0].tolist() == [0.25, 1.0]
    values, _ = decoded(dds(tmp_path / "e.dds", vd.R24_UNORM_X8_TYPELESS, 1, 1, struct.pack("<I", 0xAB000000 | 0xFFFFFF)))
    assert values[0, 0, 0] == pytest.approx(1.0)  # the stencil byte above the depth is ignored
    values, _ = decoded(dds(tmp_path / "f.dds", vd.R16_UNORM, 1, 1, struct.pack("<H", 65535)))
    assert values[0, 0, 0] == 1.0


def test_stencil_counts_each_value(tmp_path):
    path = dds(tmp_path / "s.dds", vd.R8_UINT, 4, 1, [0x15, 0x15, 0xA0, 0])
    line = vd.convert(path)
    assert "R8_UINT" in line and "0x15: 2" in line and "0xa0: 1" in line
    assert Image.open(path.with_suffix(".png")).size == (4, 1)


def test_convert_writes_pngs(tmp_path):
    path = dds(tmp_path / "albedo.dds", vd.R8G8B8A8_UNORM, 2, 2, [10, 20, 30, 40] * 4)
    line = vd.convert(path)
    assert "2 x 2" in line
    assert np.asarray(Image.open(tmp_path / "albedo.png"))[0, 0].tolist() == [10, 20, 30]
    assert np.asarray(Image.open(tmp_path / "albedo.alpha.png"))[0, 0] == 40
    depth = dds(tmp_path / "depth.dds", vd.R32_FLOAT, 2, 1, np.array([0.0, 1.0], dtype="<f4").tobytes())
    vd.convert(depth)
    assert np.asarray(Image.open(tmp_path / "depth.png"))[0, :, 0].tolist() == [0, 255]


def test_unknown_formats_are_named(tmp_path):
    with pytest.raises(ValueError, match="71"):
        decoded(dds(tmp_path / "x.dds", 71, 4, 1, bytes(8)))  # BC1
    junk = tmp_path / "junk.dds"
    junk.write_bytes(b"not a dds file" * 20)
    with pytest.raises(ValueError, match="DX10"):
        vd.read_dds(junk)
```

- [x] **Step 2: Run it; it must fail**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render/tools && uv run --no-project -q --with numpy --with pillow --with pytest pytest -q test_view_dump.py 2>&1 | grep -E "^E |error in"
```
Expected: `E   ModuleNotFoundError: No module named 'view_dump'`, then `1 error in ...`.

- [x] **Step 3: Write `tools/view_dump.py`**

```python
"""Turns BlockRender's dumped buffers (.dds files) into PNGs you can look at.

    uv run --no-project --with numpy --with pillow tools/view_dump.py <dump folder>

For each name.dds it writes name.png, plus name.alpha.png when the format has an alpha channel, and prints the
format, the size and each channel's range:
- colour targets as they are (8-bit and 10-bit), HDR ones tone mapped;
- depth stretched between its smallest and largest values, the largest white (the nearest, with reversed depth);
- motion vectors as a flow image: red right, green down, both scaled to the largest motion;
- stencil as a palette, with a count of each value.
"""
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image

# DXGI format numbers (dxgiformat.h).
R32G32B32A32_FLOAT = 2
R16G16B16A16_FLOAT = 10
R16G16B16A16_UNORM = 11
R32G32_FLOAT = 16
R10G10B10A2_UNORM = 24
R11G11B10_FLOAT = 26
R8G8B8A8_UNORM = 28
R8G8B8A8_UNORM_SRGB = 29
R16G16_FLOAT = 34
R16G16_UNORM = 35
R16G16_SNORM = 37
R32_FLOAT = 41
R32_UINT = 42
R24_UNORM_X8_TYPELESS = 46
R8G8_UNORM = 49
R16_FLOAT = 54
R16_UNORM = 56
R8_UNORM = 61
R8_UINT = 62
B8G8R8A8_UNORM = 87
B8G8R8A8_UNORM_SRGB = 91

NAMES = {value: name for name, value in globals().items() if name.isupper() and isinstance(value, int)}


def read_dds(path):
    """(format, texels as a 2D numpy array of rows, raw bytes) from a DDS file with a DX10 header."""
    data = Path(path).read_bytes()
    if data[:4] != b"DDS " or data[84:88] != b"DX10":
        raise ValueError(f"{path} isn't a DDS file with a DX10 header")
    height, width, row_bytes = struct.unpack_from("<3I", data, 12)
    dxgi_format = struct.unpack_from("<I", data, 128)[0]
    pixels = data[148:148 + row_bytes * height]
    return dxgi_format, width, height, np.frombuffer(pixels, dtype=np.uint8).reshape(height, row_bytes)


def float11(bits, mantissa_bits):
    """The unsigned small floats of R11G11B10_FLOAT: 5 exponent bits over 6 (or 5) mantissa bits."""
    exponent = (bits >> mantissa_bits) & 31
    mantissa = bits & ((1 << mantissa_bits) - 1)
    scale = float(1 << mantissa_bits)
    normal = np.ldexp(1.0 + mantissa / scale, exponent.astype(np.int32) - 15)
    denormal = np.ldexp(mantissa / scale, -14)
    return np.where(exponent == 0, denormal, np.where(exponent == 31, np.inf, normal))


def decode(dxgi_format, width, height, rows):
    """A float array (height, width, channels) and what kind of image it is: colour, hdr, depth, motion or stencil."""
    raw = rows.reshape(height, -1)
    if dxgi_format in (R8G8B8A8_UNORM, R8G8B8A8_UNORM_SRGB):
        return raw.reshape(height, width, 4) / 255.0, "colour"
    if dxgi_format in (B8G8R8A8_UNORM, B8G8R8A8_UNORM_SRGB):
        return raw.reshape(height, width, 4)[:, :, [2, 1, 0, 3]] / 255.0, "colour"
    if dxgi_format == R10G10B10A2_UNORM:
        words = raw.view("<u4").reshape(height, width)
        channels = [(words >> shift) & mask for shift, mask in ((0, 1023), (10, 1023), (20, 1023), (30, 3))]
        return np.stack([channels[0] / 1023.0, channels[1] / 1023.0, channels[2] / 1023.0, channels[3] / 3.0], axis=-1), "colour"
    if dxgi_format == R11G11B10_FLOAT:
        words = raw.view("<u4").reshape(height, width)
        return np.stack([float11(words & 2047, 6), float11((words >> 11) & 2047, 6), float11((words >> 22) & 1023, 5)], axis=-1), "hdr"
    if dxgi_format == R16G16B16A16_FLOAT:
        return raw.view("<f2").reshape(height, width, 4).astype(np.float64), "hdr"
    if dxgi_format == R16G16B16A16_UNORM:
        return raw.view("<u2").reshape(height, width, 4) / 65535.0, "colour"
    if dxgi_format == R32G32B32A32_FLOAT:
        return raw.view("<f4").reshape(height, width, 4).astype(np.float64), "hdr"
    if dxgi_format == R16G16_FLOAT:
        return raw.view("<f2").reshape(height, width, 2).astype(np.float64), "motion"
    if dxgi_format == R16G16_SNORM:
        return np.maximum(raw.view("<i2").reshape(height, width, 2) / 32767.0, -1.0), "motion"
    if dxgi_format == R16G16_UNORM:
        return raw.view("<u2").reshape(height, width, 2) / 65535.0, "motion"
    if dxgi_format == R32G32_FLOAT:
        return raw.view("<f4").reshape(height, width, 2).astype(np.float64), "motion"
    if dxgi_format == R32_FLOAT:
        return raw.view("<f4").reshape(height, width, 1).astype(np.float64), "depth"
    if dxgi_format == R24_UNORM_X8_TYPELESS:
        return ((raw.view("<u4").reshape(height, width, 1) & 0xFFFFFF) / float(0xFFFFFF)), "depth"
    if dxgi_format == R16_UNORM:
        return raw.view("<u2").reshape(height, width, 1) / 65535.0, "depth"
    if dxgi_format == R16_FLOAT:
        return raw.view("<f2").reshape(height, width, 1).astype(np.float64), "depth"
    if dxgi_format == R32_UINT:
        return raw.view("<u4").reshape(height, width, 1).astype(np.float64), "stencil"
    if dxgi_format == R8_UINT:
        return raw.reshape(height, width, 1).astype(np.float64), "stencil"
    if dxgi_format == R8_UNORM:
        return raw.reshape(height, width, 1) / 255.0, "colour"
    if dxgi_format == R8G8_UNORM:
        return raw.reshape(height, width, 2) / 255.0, "colour"
    raise ValueError(f"format {NAMES.get(dxgi_format, dxgi_format)} isn't supported")


def to_rgb(values, kind):
    """8-bit RGB for an image of each kind."""
    if kind == "colour":
        rgb = values[:, :, :3] if values.shape[2] >= 3 else np.concatenate([values, np.zeros_like(values[:, :, :1])], axis=-1)
        if rgb.shape[2] == 1:
            rgb = np.repeat(rgb, 3, axis=-1)
        return np.clip(rgb * 255.0 + 0.5, 0, 255).astype(np.uint8)
    if kind == "hdr":
        rgb = np.nan_to_num(values[:, :, :3], nan=0.0, posinf=1e4)
        rgb = np.clip(rgb, 0.0, None)
        mapped = (rgb / (1.0 + rgb)) ** (1.0 / 2.2)  # Reinhard, then a display gamma
        return np.clip(mapped * 255.0 + 0.5, 0, 255).astype(np.uint8)
    if kind == "depth":
        depth = values[:, :, 0]
        finite = depth[np.isfinite(depth)]
        low, high = (finite.min(), finite.max()) if finite.size else (0.0, 1.0)
        scaled = (depth - low) / (high - low) if high > low else np.zeros_like(depth)
        grey = np.clip(scaled * 255.0 + 0.5, 0, 255).astype(np.uint8)
        return np.repeat(grey[:, :, None], 3, axis=-1)
    if kind == "motion":
        motion = np.nan_to_num(values[:, :, :2])
        largest = np.abs(motion).max() or 1.0
        rgb = np.zeros(motion.shape[:2] + (3,))
        rgb[:, :, 0] = 0.5 + 0.5 * motion[:, :, 0] / largest
        rgb[:, :, 1] = 0.5 + 0.5 * motion[:, :, 1] / largest
        rgb[:, :, 2] = 0.5
        return np.clip(rgb * 255.0 + 0.5, 0, 255).astype(np.uint8)
    if kind == "stencil":
        value = values[:, :, 0].astype(np.int64) & 0xFF
        rgb = np.stack([(value * 97) % 256, (value * 57) % 256, (value * 151) % 256], axis=-1)
        rgb[value == 0] = 0
        return rgb.astype(np.uint8)
    raise ValueError(kind)


def describe(path, dxgi_format, width, height, values, kind):
    ranges = ", ".join(f"{np.nanmin(values[:, :, c]):.4g}..{np.nanmax(values[:, :, c]):.4g}" for c in range(values.shape[2]))
    line = f"{Path(path).name}: {NAMES.get(dxgi_format, dxgi_format)}, {width} x {height}, {kind}, channels {ranges}"
    if kind == "stencil":
        unique, counts = np.unique(values[:, :, 0].astype(np.int64), return_counts=True)
        line += "; values " + ", ".join(f"0x{u:02x}: {c}" for u, c in zip(unique, counts))
    return line


def convert(path):
    """Writes the PNG(s) for one .dds file and returns its summary line."""
    path = Path(path)
    dxgi_format, width, height, rows = read_dds(path)
    values, kind = decode(dxgi_format, width, height, rows)
    Image.fromarray(to_rgb(values, kind), "RGB").save(path.with_suffix(".png"))
    if kind == "colour" and values.shape[2] == 4:
        alpha = np.clip(values[:, :, 3] * 255.0 + 0.5, 0, 255).astype(np.uint8)
        Image.fromarray(alpha, "L").save(path.with_suffix(".alpha.png"))
    return describe(path, dxgi_format, width, height, values, kind)


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    files = sorted(Path(sys.argv[1]).glob("*.dds"))
    if not files:
        raise SystemExit(f"no .dds files in {sys.argv[1]}")
    for path in files:
        print(convert(path))


if __name__ == "__main__":
    main()
```

- [x] **Step 4: Write the failing test, `tools/test_frame_summary.py`**

```python
import copy

from frame_summary import projection_layout, summarise

# A reversed, infinite D3D projection for row vectors: view x right, y up, z forward (near 0.2).
F = 1.0 / 0.6745  # 1 / tan(34.0 deg), a vertical field of view of 68 deg
PROJECTION = [F / 1.7778, 0, 0, 0,
              0, F, 0, 0,
              0, 0, 0, 1,
              0, 0, 0.2, 0]


def transposed(matrix):
    return [matrix[column * 4 + row] for row in range(4) for column in range(4)]


def target(resource, fmt):
    return {"handle": "0x10", "resource": resource, "format": fmt, "size": [2560, 1440]}


LOG = {
    "pluginVersion": "0.1.0",
    "time": "2026-10-01T18:00:00",
    "frames": [100, 101],
    "device": {
        "seenAtCreation": True,
        "device": "0xabc",
        "gpuApi": {"device": "proxy", "matchesDetour": True, "queue": "proxy", "swapChain": "proxy"},
        "streamline": "proxies",
        "views": {"rtvs": 12, "dsvs": 3, "unknownHandles": 0},
        "patches": {"vtableEntries": 29, "imports": 4},
    },
    "scriptCamera": {"position": [1.0, 2.0, 3.0], "forward": [0.0, 1.0, 0.0], "fov": 68.24},
    "constants": [{
        "frame": 100, "viewport": 0, "version": 2,
        "viewToClip": PROJECTION, "clipToView": [0] * 16, "clipToPrevClip": [0] * 16, "prevClipToClip": [0] * 16,
        "jitter": [0.25, -0.25], "mvecScale": [1, 1], "pinholeOffset": [0, 0],
        "position": [11.0, 2.0, 3.0], "up": [0, 0, 1], "right": [1, 0, 0], "forward": [0.0, 1.0, 0.0],
        "near": 0.2, "far": 10000.0, "fov": 1.19, "fovDegrees": 68.18, "aspect": 1.7778,
        "motionVectorsInvalidValue": 0, "depthInverted": "true", "cameraMotionIncluded": "true",
        "motionVectors3D": "false", "reset": "false", "orthographic": "false", "motionVectorsDilated": "false",
        "motionVectorsJittered": "false",
    }],
    "tags": [
        {"frame": 100, "viewport": 0, "local": False, "type": 0, "name": "depth", "resource": "0x5000",
         "format": "R32G8X24_TYPELESS", "size": [2560, 1440], "flags": "0x2", "mips": 1, "depthOrArraySize": 1,
         "state": "0x80", "lifecycle": "validUntilPresent", "extent": [0, 0, 1707, 960], "list": "0x0"},
        {"frame": 101, "viewport": 0, "local": False, "type": 0, "name": "depth", "resource": "0x5000",
         "format": "R32G8X24_TYPELESS", "size": [2560, 1440], "flags": "0x2", "mips": 1, "depthOrArraySize": 1,
         "state": "0x80", "lifecycle": "validUntilPresent", "extent": [0, 0, 1707, 960], "list": "0x0"},
    ],
    "evaluates": [{"frame": 100, "feature": 0, "viewport": 0, "list": "0x7000", "nativeList": "0x7100"}],
    "events": [
        {"ms": 0.0, "frame": 100, "thread": 7, "what": "frameToken", "detail": "frame 100: the log begins"},
        {"ms": 1.0, "frame": 100, "thread": 9, "what": "submit", "detail": "queue 0x9000 (direct), 1 lists"},
        {"ms": 2.0, "frame": 100, "thread": 9, "what": "evaluate", "detail": "DLSS, frame 100"},
        {"ms": 3.0, "frame": 100, "thread": 9, "what": "submit", "detail": "queue 0x9000 (direct), 2 lists"},
        {"ms": 4.0, "frame": 100, "thread": 9, "what": "present", "detail": "swap chain 0x1, on queue 0x9000"},
    ],
    "submissions": [
        {"ms": 1.0, "frame": 100, "queue": "0x9000", "lists": ["0x6000"], "untracked": 0},
        {"ms": 3.0, "frame": 100, "queue": "0x9000", "lists": ["0x7100", "0x7200"], "untracked": 1},
    ],
    "lists": [{
        "list": "0x7100", "name": "Scene", "frame": 100,
        "passes": [{
            "targets": [target("0x1", "R10G10B10A2_UNORM"), target("0x2", "R10G10B10A2_UNORM"),
                        target("0x3", "R8G8B8A8_UNORM")],
            "depth": target("0x5000", "D32_FLOAT_S8X24_UINT"), "depthIsTagged": True,
            "draws": 0, "indexedDraws": 812, "indirect": 3, "bundles": 0, "dispatches": 0, "clears": 0,
            "barriers": 4, "enhancedBarriers": 0, "events": ["GBuffer"],
        }],
    }],
    "gbufferCandidates": [{"list": "0x7100", "pass": 0}],
}


def text(log=LOG):
    return "\n".join(summarise(log))


def test_projection_layout_reads_row_and_column_vectors():
    layout = projection_layout(PROJECTION)
    assert layout["vectors"] == "row"
    assert layout["forward"] == (2, 1) and layout["right"] == (0, 1) and layout["up"] == (1, 1)
    transposed_layout = projection_layout(transposed(PROJECTION))
    assert transposed_layout["vectors"] == "column"
    assert transposed_layout["forward"] == (2, 1) and transposed_layout["right"] == (0, 1)


def test_projection_layout_rejects_other_matrices():
    identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    assert projection_layout(identity) is None


def test_summary_compares_the_cameras():
    summary = text()
    assert "Streamline minus script position: [10.0, 0.0, 0.0] (10.0000 m)" in summary
    assert "angle between the forwards: 0.0000 deg" in summary
    assert "viewToClip: row vectors; view +z is forward, +x right, +y up" in summary


def test_summary_lists_each_distinct_tag_once():
    summary = text()
    assert summary.count("depth (global): R32G8X24_TYPELESS 2560x1440") == 1


def test_summary_marks_the_dlss_and_gbuffer_lists():
    summary = text()
    assert "submit: queue 0x9000 (direct), 2 lists, holding the DLSS list and a G-buffer list" in summary
    assert "submit: queue 0x9000 (direct), 1 lists" not in summary
    assert "0x7100 'Scene' frame 100: 1 passes, 815 draws, 0 dispatches [DLSS, G-buffer candidate]" in summary
    assert "target 2: R8G8B8A8_UNORM 2560x1440 (0x3)" in summary
    assert "depth: D32_FLOAT_S8X24_UINT 2560x1440 (0x5000), tagged: True" in summary


def test_summary_without_a_script_camera_or_candidates():
    log = copy.deepcopy(LOG)
    log["scriptCamera"] = None
    log["gbufferCandidates"] = []
    summary = text(log)
    assert "no script camera was noted (no player?)" in summary
    assert summary.endswith("== G-buffer candidates\nnone")
```

- [x] **Step 5: Run both; the summary's must fail**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render/tools && uv run --no-project -q --with numpy --with pillow --with pytest pytest -q test_view_dump.py test_frame_summary.py 2>&1 | grep -E "^E |error in"
```
Expected: `E   ModuleNotFoundError: No module named 'frame_summary'`, then `1 error in ...`.

- [x] **Step 6: Write `tools/frame_summary.py`**

```python
"""Summarise a BlockRender frame log (frame.json) against step 1 of spike 3: the device, the camera, the tags, the
order of events, the command lists and the G-buffer candidates (see the spec's "Step 1: the frame log").

    uv run --no-project python tools/frame_summary.py <frame.json>
"""
import json
import math
import sys


def sign(value):
    return -1 if value < 0 else 1


def rows_of(matrix):
    return [list(matrix[i * 4:(i + 1) * 4]) for i in range(4)]


def projection_layout(matrix):
    """How viewToClip maps view space to clip space, read as Camera.cpp's FindProjectionLayout reads it: row or column
    vectors, and which view axis is forward, right and up, with signs. None if it isn't a perspective projection."""

    def perspective(rows):
        ones = 0
        for axis in range(3):
            w = abs(rows[axis][3])
            if abs(w - 1.0) <= 1e-3:
                ones += 1
            elif w > 1e-3:
                return False
        return ones == 1 and abs(rows[3][3]) <= 1e-3

    rows = rows_of(matrix)
    vectors = "row"
    if not perspective(rows):
        rows = [list(column) for column in zip(*rows)]
        if not perspective(rows):
            return None
        vectors = "column"
    forward = next(axis for axis in range(3) if abs(abs(rows[axis][3]) - 1.0) <= 1e-3)
    first, second = (forward + 1) % 3, (forward + 2) % 3
    right = first if abs(rows[first][0]) >= abs(rows[second][0]) else second
    up = second if right == first else first
    return {
        "vectors": vectors,
        "forward": (forward, sign(rows[forward][3])),
        "right": (right, sign(rows[right][0])),
        "up": (up, sign(rows[up][1])),
        "rows": rows,
    }


def angle_degrees(a, b):
    length = math.sqrt(sum(x * x for x in a)) * math.sqrt(sum(x * x for x in b))
    if length == 0:
        return float("nan")
    cosine = max(-1.0, min(1.0, sum(x * y for x, y in zip(a, b)) / length))
    return math.degrees(math.acos(cosine))


def axis_name(axis_and_sign):
    axis, direction = axis_and_sign
    return f"{'+' if direction > 0 else '-'}{'xyz'[axis]}"


def target_text(target):
    if target is None:
        return "none"
    if target.get("unknown"):
        return f"unknown handle {target['handle']}"
    text = f"{target['format']} {target['size'][0]}x{target['size'][1]} ({target['resource']})"
    if "readOnly" in target:
        text += f", read-only {target['readOnly']}"
    return text


def summarise(log):
    out = []
    add = out.append
    add(f"BlockRender {log['pluginVersion']}, logged {log['time']}, frames {log['frames'][0]} to {log['frames'][1]}")

    device = log["device"]
    gpu_api = device["gpuApi"]
    add("")
    add("== Device")
    add(f"seen at creation: {device['seenAtCreation']}; device {device['device']}")
    add(f"GpuApi: device {gpu_api['device']} (matches the detour's: {gpu_api['matchesDetour']}), queue {gpu_api['queue']}, "
        f"swap chain {gpu_api['swapChain']}")
    add(f"Streamline: {device['streamline']}; patches: {device['patches']['vtableEntries']} vtable entries, "
        f"{device['patches']['imports']} imports")
    add(f"views known: {device['views']['rtvs']} RTVs, {device['views']['dsvs']} DSVs; "
        f"{device['views']['unknownHandles']} bound handles unknown")

    add("")
    add("== Camera")
    evaluated = {(e["frame"], e["viewport"]) for e in log["evaluates"]}
    constants = [c for c in log["constants"] if (c["frame"], c["viewport"]) in evaluated] or log["constants"]
    if not constants:
        add("no constants were logged")
    for c in constants[:2]:
        add(f"frame {c['frame']}, viewport {c['viewport']}, struct version {c['version']}")
        add(f"  position {c['position']}, forward {c['forward']}, right {c['right']}, up {c['up']}")
        add(f"  near {c['near']}, far {c['far']}, fov {c['fovDegrees']:.3f} deg, aspect {c['aspect']:.4f}, "
            f"depthInverted {c['depthInverted']}, jitter {c['jitter']}, mvecScale {c['mvecScale']}")
        add(f"  cameraMotionIncluded {c['cameraMotionIncluded']}, motionVectors3D {c['motionVectors3D']}, "
            f"motionVectorsDilated {c['motionVectorsDilated']}, motionVectorsJittered {c['motionVectorsJittered']}, "
            f"reset {c['reset']}, orthographic {c['orthographic']}")
        layout = projection_layout(c["viewToClip"])
        if layout is None:
            add(f"  viewToClip isn't a perspective projection Camera.cpp understands: {c['viewToClip']}")
        else:
            rows = layout["rows"]
            forward = layout["forward"][0]
            add(f"  viewToClip: {layout['vectors']} vectors; view {axis_name(layout['forward'])} is forward, "
                f"{axis_name(layout['right'])} right, {axis_name(layout['up'])} up")
            add(f"  rows (as row vectors): {rows}")
            add(f"  terms that would carry jitter: {rows[forward][0]}, {rows[forward][1]}; depth terms: "
                f"{rows[forward][2]} (forward), {rows[3][2]} (constant)")
    script = log.get("scriptCamera")
    if script and constants:
        c = constants[0]
        offset = [p - s for p, s in zip(c["position"], script["position"])]
        add(f"script camera: position {script['position']}, forward {script['forward']}, fov {script['fov']} deg")
        add(f"  Streamline minus script position: {[round(x, 4) for x in offset]} "
            f"({math.sqrt(sum(x * x for x in offset)):.4f} m)")
        add(f"  angle between the forwards: {angle_degrees(c['forward'], script['forward']):.4f} deg")
    elif not script:
        add("no script camera was noted (no player?)")

    add("")
    add("== Tags (each distinct one)")
    seen = set()
    for tag in log["tags"]:
        size = f"{tag['size'][0]}x{tag['size'][1]}" if "size" in tag else "?"
        key = (tag["name"], tag["local"], tag.get("format", "?"), size, tuple(tag["extent"]), tag["state"], tag["lifecycle"])
        if key in seen:
            continue
        seen.add(key)
        add(f"{tag['name']} ({'local' if tag['local'] else 'global'}): {tag.get('format', '?')} {size}, flags "
            f"{tag.get('flags', '?')}, extent {tag['extent']} (top, left, width, height), state {tag['state']}, "
            f"{tag['lifecycle']}, resource {tag['resource']}")

    add("")
    add("== Evaluates")
    for e in log["evaluates"]:
        add(f"frame {e['frame']}, feature {e['feature']}, viewport {e['viewport']}, list {e['list']}, native {e['nativeList']}")

    lists = {entry["list"]: entry for entry in log["lists"]}
    candidates = {(c["list"], c["pass"]) for c in log["gbufferCandidates"]}
    candidate_lists = {c["list"] for c in log["gbufferCandidates"]}
    native_lists = {e["nativeList"] for e in log["evaluates"]}

    add("")
    add("== Order of events (Streamline calls, presents, and the submissions holding the DLSS or G-buffer lists)")
    submissions = iter(log["submissions"])  # the log adds each submit event and its submission together, in order
    for event in log["events"]:
        if event["what"] == "submit":
            submission = next(submissions, None)
            holds = []
            if submission:
                if native_lists & set(submission["lists"]):
                    holds.append("the DLSS list")
                if candidate_lists & set(submission["lists"]):
                    holds.append("a G-buffer list")
            if not holds:
                continue
            add(f"{event['ms']:9.3f} ms  frame {event['frame']}  thread {event['thread']}  submit: {event['detail']}, "
                f"holding {' and '.join(holds)}")
            continue
        add(f"{event['ms']:9.3f} ms  frame {event['frame']}  thread {event['thread']}  {event['what']}: {event['detail']}")

    add("")
    add("== Submissions")
    queues = {}
    for s in log["submissions"]:
        queues.setdefault(s["queue"], [0, 0, 0])
        queues[s["queue"]][0] += 1
        queues[s["queue"]][1] += len(s["lists"])
        queues[s["queue"]][2] += s["untracked"]
    for queue, (count, total, untracked) in queues.items():
        add(f"queue {queue}: {count} submissions, {total} lists, {untracked} untracked")

    add("")
    add("== Command lists")
    totals = {"draws": 0, "indexedDraws": 0, "indirect": 0, "dispatches": 0, "barriers": 0, "enhancedBarriers": 0}
    render_passes = 0
    for entry in log["lists"]:
        passes = entry["passes"]
        for p in passes:
            for key in totals:
                totals[key] += p[key]
            render_passes += 1 if p.get("renderPass") else 0
        draws = sum(p["draws"] + p["indexedDraws"] + p["indirect"] for p in passes)
        dispatches = sum(p["dispatches"] for p in passes)
        marks = []
        if entry["list"] in native_lists:
            marks.append("DLSS")
        if entry["list"] in candidate_lists:
            marks.append("G-buffer candidate")
        add(f"{entry['list']} '{entry['name']}' frame {entry['frame']}: {len(passes)} passes, {draws} draws, {dispatches} "
            f"dispatches{(' [' + ', '.join(marks) + ']') if marks else ''}")
    add(f"in all: {len(log['lists'])} lists; {totals}; {render_passes} render passes")

    add("")
    add("== G-buffer candidates")
    if not candidates:
        add("none")
    for list_address, index in sorted(candidates):
        entry = lists.get(list_address)
        if entry is None:
            add(f"{list_address} pass {index}: the list isn't in the log")
            continue
        p = entry["passes"][index]
        add(f"{list_address} '{entry['name']}' pass {index}: {p['draws']} draws, {p['indexedDraws']} indexed, "
            f"{p['indirect']} indirect, {p['clears']} clears, events {p['events']}")
        for i, target in enumerate(p["targets"]):
            add(f"  target {i}: {target_text(target)}")
        add(f"  depth: {target_text(p['depth'])}, tagged: {p['depthIsTagged']}")
    return out


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    with open(argv[1], encoding="utf-8") as file:
        log = json.load(file)
    print("\n".join(summarise(log)))


if __name__ == "__main__":
    main(sys.argv)
```

- [x] **Step 7: Run all the tools' tests; they must pass**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render/tools && uv run --no-project -q --with numpy --with pillow --with pytest pytest -q 2>&1 | tail -1
```
Expected: `20 passed`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render/tools \
  && git commit -q -m "block-render: dump viewer and frame log summary" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 23: The hooks

From here on, the code runs inside the game, so it's built and checked for warnings here, and tested in Tasks 29
and 30.

- **`StreamlineHooks`** swaps the game exe's imports of `slGetNewFrameToken`, `slSetConstants`, `slSetTag` and
  `slEvaluateFeature` for hooks. Each hook checks and records what it's given, tells the listeners, and calls
  Streamline. At a DLSS evaluate:
  - it reads the local tags among the inputs, and lets them replace the global tags of the same type;
  - it finds the native command list under a Streamline proxy, through `slGetNativeInterface`.
- **`D3D12Hooks`** detours `D3D12CreateDevice` and `CreateDXGIFactory`, `CreateDXGIFactory1` and
  `CreateDXGIFactory2` at load, and patches vtables from then on:
  - **the device:** its descriptor calls, for the view registry;
  - **one queue and one list of its own:** these reach the vtables that every queue and list shares (submissions,
    `Reset` and `Close`, render targets, barriers, draws and events);
  - **the factory:** swap-chain creation, which then patches each new swap chain's `Present` and `Present1`.

  The main queue is the one the DLSS evaluate's list is submitted on. `Device()` is that queue's device once it's
  known. Every device and queue the hooks keep is held for the session, so no pointer they hand out dangles.
- **`HookEvents`** is what the hooks tell the features: the frame log, the dumps and the renderer.

**Files:**
- Create: `src/hooks/HookEvents.h`, `src/hooks/StreamlineHooks.h`, `src/hooks/StreamlineHooks.cpp`,
  `src/hooks/D3D12Hooks.h`, `src/hooks/D3D12Hooks.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write `src/hooks/HookEvents.h`**

```cpp
#pragma once

#include "sl/Sl.h"
#include "track/StateTracker.h"

#include <d3d12.h>
#include <dxgi1_2.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace cpmc
{
// One DLSS super-resolution evaluate, just before Streamline runs it.
struct EvaluateCall
{
	uint32_t                   frame = 0;
	uint32_t                   viewport = 0;
	void*                      gameList = nullptr;  // as the game passed it: possibly a Streamline proxy
	ID3D12GraphicsCommandList* list = nullptr;      // the native list underneath; null if it couldn't be found
	std::vector<SlTag>         tags;                // the global tags, with local ones in their place
	std::optional<SlConstants> constants;           // this frame's, for this viewport
};

// What the hooks tell the plugin's features: the frame log, the dumps and the renderer. Every call comes from one of
// the game's threads, inside a D3D12 or Streamline call, so keep them short, and never fault. A feature that records
// commands into a game list uses d3d::Original(), so its commands never reach the tracker.
class HookEvents
{
public:
	virtual ~HookEvents() = default;

	virtual void OnFrameToken(uint32_t /*aFrame*/) {}
	virtual void OnConstants(uint32_t /*aFrame*/, uint32_t /*aViewport*/, const SlConstants& /*aConstants*/) {}
	virtual void OnTags(uint32_t /*aFrame*/, uint32_t /*aViewport*/, const std::vector<SlTag>& /*aTags*/, void* /*aList*/) {}
	virtual void OnEvaluate(const EvaluateCall& /*aCall*/) {}

	// A pass of a tracked list ends: the list is about to change render targets, close (aClosing), or has just ended
	// a render pass. aState.CurrentTargets() are still the pass's targets.
	virtual void OnPassEnd(ID3D12GraphicsCommandList* /*aList*/, ListState& /*aState*/, bool /*aClosing*/) {}
	virtual void OnClose(ID3D12GraphicsCommandList* /*aList*/, ListState& /*aState*/) {}

	virtual void OnSubmit(ID3D12CommandQueue* /*aQueue*/, UINT /*aCount*/, ID3D12CommandList* const* /*aLists*/) {}
	virtual void OnSubmitted(ID3D12CommandQueue* /*aQueue*/, UINT /*aCount*/, ID3D12CommandList* const* /*aLists*/) {}
	virtual void OnPresent(IDXGISwapChain* /*aSwapChain*/) {}
};
}
```

- [x] **Step 2: Write `src/hooks/StreamlineHooks.h`**

```cpp
#pragma once

#include "LogQueue.h"
#include "hooks/HookEvents.h"
#include "sl/Sl.h"

#include <d3d12.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The Streamline side of the hooks (the spec's "Hooks"): the game exe's imports of slGetNewFrameToken, slSetConstants,
// slSetTag and slEvaluateFeature from sl.interposer.dll point at these, which record what they're given, tell the
// listeners, and call Streamline.
namespace cpmc::slhooks
{
// Patches the four imports. False, with nothing patched, if the exe doesn't import them all by name.
bool Install(LogQueue& aLog);
void Remove();

void AddListener(HookEvents* aListener);

uint64_t FrameTokens();  // how many frames Streamline has started

// The native object under a Streamline proxy (a device, queue, list or swap chain), through slGetNativeInterface,
// with one reference the caller owns. Null if aObject's vtable isn't in sl.interposer.dll.
void* NativeInterface(void* aObject);

// The native D3D12 list under a list the game handed Streamline (itself, or the list inside a Streamline proxy).
// Null if it's neither. When it isn't null, the caller owns one reference to it.
ID3D12GraphicsCommandList* NativeList(void* aGameList);

std::string Summary();
}
```

- [x] **Step 3: Write `src/hooks/StreamlineHooks.cpp`**

```cpp
#include "hooks/StreamlineHooks.h"

#include "hooks/D3D12Hooks.h"
#include "hooks/ImportPatch.h"

#include <Windows.h>
#include <psapi.h>

#include <array>
#include <atomic>
#include <format>
#include <map>
#include <mutex>

namespace cpmc::slhooks
{
namespace
{
using GetNewFrameTokenFn = sl::Result (*)(sl::FrameToken*&, const uint32_t*);
using SetConstantsFn = sl::Result (*)(const sl::Constants&, const sl::FrameToken&, const sl::ViewportHandle&);
using SetTagFn = sl::Result (*)(const sl::ViewportHandle&, const sl::ResourceTag*, uint32_t, sl::CommandBuffer*);
using EvaluateFeatureFn = sl::Result (*)(sl::Feature, const sl::FrameToken&, const sl::BaseStructure**, uint32_t, sl::CommandBuffer*);
using GetNativeInterfaceFn = sl::Result (*)(void*, void**);

constexpr const char* kDll = "sl.interposer.dll";
constexpr uint32_t    kMaxTags = 64;
constexpr size_t      kConstantsKept = 8;

LogQueue*                  g_log = nullptr;
std::array<HookEvents*, 4> g_listeners{};
size_t                     g_listenerCount = 0;
std::vector<ImportPatch>   g_patches;
GetNewFrameTokenFn         g_getNewFrameToken = nullptr;
SetConstantsFn             g_setConstants = nullptr;
SetTagFn                   g_setTag = nullptr;
EvaluateFeatureFn          g_evaluateFeature = nullptr;
GetNativeInterfaceFn       g_getNativeInterface = nullptr;
uintptr_t                  g_slBase = 0;
uintptr_t                  g_slEnd = 0;
std::atomic<uint64_t>      g_frameTokens{ 0 };
std::atomic<uint64_t>      g_evaluates{ 0 };

// What Streamline has been told, by viewport.
struct Viewport
{
	std::map<uint32_t, SlTag>                          tags;       // the latest global tag of each type
	std::vector<std::pair<uint32_t, SlConstants>>      constants;  // the latest few frames' constants
};
std::mutex                   g_mutex;
std::map<uint32_t, Viewport> g_viewports;

template<class Fn>
void Each(Fn&& aFn)
{
	for (size_t i = 0; i < g_listenerCount; ++i)
	{
		aFn(*g_listeners[i]);
	}
}

// A frame token's index, through its virtual operator: the token is Streamline's own object.
uint32_t FrameOf(const sl::FrameToken& aToken)
{
	return static_cast<uint32_t>(aToken);
}

sl::Result GetNewFrameToken(sl::FrameToken*& aToken, const uint32_t* aFrameIndex)
{
	const sl::Result result = g_getNewFrameToken(aToken, aFrameIndex);
	if (result == sl::Result::eOk && aToken)
	{
		const uint32_t frame = FrameOf(*aToken);
		d3d::SetCurrentFrame(frame);
		++g_frameTokens;
		Each([&](HookEvents& aListener) { aListener.OnFrameToken(frame); });
	}
	return result;
}

sl::Result SetConstants(const sl::Constants& aValues, const sl::FrameToken& aFrame, const sl::ViewportHandle& aViewport)
{
	ReadableMemory memory;
	const auto     constants = ReadConstants(memory, &aValues);
	const auto     viewport = ReadViewport(memory, &aViewport);
	if (constants && viewport)
	{
		const uint32_t frame = FrameOf(aFrame);
		{
			std::lock_guard lock(g_mutex);
			auto&           kept = g_viewports[*viewport].constants;
			kept.emplace_back(frame, *constants);
			if (kept.size() > kConstantsKept)
			{
				kept.erase(kept.begin());
			}
		}
		Each([&](HookEvents& aListener) { aListener.OnConstants(frame, *viewport, *constants); });
	}
	return g_setConstants(aValues, aFrame, aViewport);
}

sl::Result SetTag(const sl::ViewportHandle& aViewport, const sl::ResourceTag* aTags, uint32_t aCount, sl::CommandBuffer* aList)
{
	ReadableMemory     memory;
	const auto         viewport = ReadViewport(memory, &aViewport);
	std::vector<SlTag> tags;
	for (uint32_t i = 0; viewport && aTags && i < aCount && i < kMaxTags; ++i)
	{
		if (const auto tag = ReadTag(memory, &aTags[i], false))
		{
			tags.push_back(*tag);
		}
	}
	if (viewport && !tags.empty())
	{
		{
			std::lock_guard lock(g_mutex);
			auto&           kept = g_viewports[*viewport].tags;
			for (const SlTag& tag : tags)
			{
				if (tag.resource)
				{
					kept[tag.type] = tag;
				}
				else
				{
					kept.erase(tag.type);  // the game dropped this tag
				}
			}
		}
		const uint32_t frame = d3d::CurrentFrame();
		Each([&](HookEvents& aListener) { aListener.OnTags(frame, *viewport, tags, aList); });
	}
	return g_setTag(aViewport, aTags, aCount, aList);
}

sl::Result EvaluateFeature(sl::Feature aFeature, const sl::FrameToken& aFrame, const sl::BaseStructure** aInputs, uint32_t aCount,
                           sl::CommandBuffer* aList)
{
	if (aFeature == sl::kFeatureDLSS)
	{
		++g_evaluates;
		ReadableMemory     memory;
		EvaluateCall       call;
		std::vector<SlTag> local;
		call.frame = FrameOf(aFrame);
		call.gameList = aList;
		for (const sl::BaseStructure* input : EvaluateInputs(memory, aInputs, aCount))
		{
			if (const auto viewport = ReadViewport(memory, input))
			{
				call.viewport = *viewport;
			}
			else if (const auto tag = ReadTag(memory, input, true))
			{
				local.push_back(*tag);
			}
		}
		{
			std::lock_guard    lock(g_mutex);
			Viewport&          viewport = g_viewports[call.viewport];
			std::vector<SlTag> global;
			for (const auto& [type, tag] : viewport.tags)
			{
				global.push_back(tag);
			}
			call.tags = TagsForEvaluate(global, local);
			for (const auto& [frame, constants] : viewport.constants)
			{
				if (frame == call.frame)
				{
					call.constants = constants;
				}
			}
		}
		call.list = NativeList(aList);
		d3d::NoteEvaluateList(call.list);
		Each([&](HookEvents& aListener) { aListener.OnEvaluate(call); });
		if (call.list)
		{
			call.list->Release();
		}
	}
	return g_evaluateFeature(aFeature, aFrame, aInputs, aCount, aList);
}

// Stores the original before the entry changes, so a call arriving mid-way still reaches Streamline.
template<class Fn>
bool PatchOne(HMODULE aExe, const char* aFunction, void* aReplacement, Fn& aOriginal)
{
	auto patch = FindImport(aExe, kDll, aFunction);
	if (!patch)
	{
		return false;
	}
	aOriginal = reinterpret_cast<Fn>(patch->original);
	if (!ApplyImport(*patch, aReplacement))
	{
		return false;
	}
	g_patches.push_back(*patch);
	return true;
}
}

bool Install(LogQueue& aLog)
{
	g_log = &aLog;
	const HMODULE interposer = GetModuleHandleA(kDll);
	MODULEINFO    info{};
	if (!interposer || !GetModuleInformation(GetCurrentProcess(), interposer, &info, sizeof(info)))
	{
		aLog.Push("sl.interposer.dll isn't loaded: nothing hooked");
		return false;
	}
	g_slBase = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
	g_slEnd = g_slBase + info.SizeOfImage;
	g_getNativeInterface = reinterpret_cast<GetNativeInterfaceFn>(GetProcAddress(interposer, "slGetNativeInterface"));
	const HMODULE exe = GetModuleHandleW(nullptr);
	const bool    ok = PatchOne(exe, "slGetNewFrameToken", reinterpret_cast<void*>(&GetNewFrameToken), g_getNewFrameToken) &&
	                PatchOne(exe, "slSetConstants", reinterpret_cast<void*>(&SetConstants), g_setConstants) &&
	                PatchOne(exe, "slSetTag", reinterpret_cast<void*>(&SetTag), g_setTag) &&
	                PatchOne(exe, "slEvaluateFeature", reinterpret_cast<void*>(&EvaluateFeature), g_evaluateFeature);
	if (!ok)
	{
		Remove();
		aLog.Push("the game doesn't import all four Streamline functions by name: nothing hooked");
		return false;
	}
	aLog.Push(std::format("Streamline hooked: {} imports patched", g_patches.size()));
	return true;
}

void Remove()
{
	for (const ImportPatch& patch : g_patches)
	{
		RestoreImport(patch);
	}
	g_patches.clear();
}

void AddListener(HookEvents* aListener)
{
	if (g_listenerCount < g_listeners.size())
	{
		g_listeners[g_listenerCount++] = aListener;
	}
}

uint64_t FrameTokens()
{
	return g_frameTokens.load();
}

void* NativeInterface(void* aObject)
{
	ReadableMemory memory;
	if (!aObject || !g_getNativeInterface || !memory.Range(aObject, sizeof(void*)))
	{
		return nullptr;
	}
	const auto vtable = *static_cast<uintptr_t*>(aObject);
	void*      native = nullptr;
	if (vtable < g_slBase || vtable >= g_slEnd || g_getNativeInterface(aObject, &native) != sl::Result::eOk)
	{
		return nullptr;
	}
	return native;
}

ID3D12GraphicsCommandList* NativeList(void* aGameList)
{
	void* const listVtable = d3d::ListVtable();
	ReadableMemory memory;
	if (!aGameList || !listVtable || !memory.Range(aGameList, sizeof(void*)))
	{
		return nullptr;
	}
	const auto vtable = *static_cast<uintptr_t*>(aGameList);
	if (vtable == reinterpret_cast<uintptr_t>(listVtable))
	{
		auto* list = static_cast<ID3D12GraphicsCommandList*>(aGameList);
		list->AddRef();
		return list;
	}
	// A Streamline proxy's vtable lies in sl.interposer.dll. Streamline hands back the native list with a reference.
	if (void* native = NativeInterface(aGameList))
	{
		if (memory.Range(native, sizeof(void*)) && *static_cast<uintptr_t*>(native) == reinterpret_cast<uintptr_t>(listVtable))
		{
			return static_cast<ID3D12GraphicsCommandList*>(native);
		}
		static_cast<IUnknown*>(native)->Release();
	}
	return nullptr;
}

std::string Summary()
{
	return std::format("{} Streamline imports patched, {} frames, {} DLSS evaluates", g_patches.size(), g_frameTokens.load(),
	                   g_evaluates.load());
}
}
```

- [x] **Step 4: Write `src/hooks/D3D12Hooks.h`**

```cpp
#pragma once

#include "LogQueue.h"
#include "hooks/HookEvents.h"
#include "track/StateTracker.h"
#include "track/ViewRegistry.h"

#include <RED4ext/RED4ext.hpp>

#include <d3d12.h>
#include <dxgi1_2.h>

#include <cstdint>
#include <string>
#include <vector>

// The D3D12 and DXGI side of the hooks (the spec's "Hooks"): detours on D3D12CreateDevice and CreateDXGIFactory*,
// set at load, then vtable patches on the native device, command queue, command lists, factory and swap chain.
namespace cpmc::d3d
{
struct Settings
{
	bool dred = false;        // turn on DRED's breadcrumbs and page faults when the device is created
	bool debugLayer = false;  // turn on the D3D12 debug layer when the device is created
};

// The functions behind the patched vtable entries. The plugin calls these when it records into a game list, so its
// commands never reach the tracker or the features.
struct Originals
{
	void(STDMETHODCALLTYPE* createRtv)(ID3D12Device*, ID3D12Resource*, const D3D12_RENDER_TARGET_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE) = nullptr;
	void(STDMETHODCALLTYPE* createDsv)(ID3D12Device*, ID3D12Resource*, const D3D12_DEPTH_STENCIL_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE) = nullptr;
	void(STDMETHODCALLTYPE* copyDescriptors)(ID3D12Device*, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE*, const UINT*, UINT,
	                                         const D3D12_CPU_DESCRIPTOR_HANDLE*, const UINT*, D3D12_DESCRIPTOR_HEAP_TYPE) = nullptr;
	void(STDMETHODCALLTYPE* copyDescriptorsSimple)(ID3D12Device*, UINT, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_CPU_DESCRIPTOR_HANDLE,
	                                               D3D12_DESCRIPTOR_HEAP_TYPE) = nullptr;
	void(STDMETHODCALLTYPE* executeCommandLists)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*) = nullptr;
	HRESULT(STDMETHODCALLTYPE* close)(ID3D12GraphicsCommandList*) = nullptr;
	HRESULT(STDMETHODCALLTYPE* reset)(ID3D12GraphicsCommandList*, ID3D12CommandAllocator*, ID3D12PipelineState*) = nullptr;
	void(STDMETHODCALLTYPE* drawInstanced)(ID3D12GraphicsCommandList*, UINT, UINT, UINT, UINT) = nullptr;
	void(STDMETHODCALLTYPE* drawIndexedInstanced)(ID3D12GraphicsCommandList*, UINT, UINT, UINT, INT, UINT) = nullptr;
	void(STDMETHODCALLTYPE* dispatch)(ID3D12GraphicsCommandList*, UINT, UINT, UINT) = nullptr;
	void(STDMETHODCALLTYPE* resourceBarrier)(ID3D12GraphicsCommandList*, UINT, const D3D12_RESOURCE_BARRIER*) = nullptr;
	void(STDMETHODCALLTYPE* executeBundle)(ID3D12GraphicsCommandList*, ID3D12GraphicsCommandList*) = nullptr;
	void(STDMETHODCALLTYPE* omSetRenderTargets)(ID3D12GraphicsCommandList*, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE*, BOOL,
	                                            const D3D12_CPU_DESCRIPTOR_HANDLE*) = nullptr;
	void(STDMETHODCALLTYPE* clearDsv)(ID3D12GraphicsCommandList*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_CLEAR_FLAGS, FLOAT, UINT8, UINT,
	                                  const D3D12_RECT*) = nullptr;
	void(STDMETHODCALLTYPE* clearRtv)(ID3D12GraphicsCommandList*, D3D12_CPU_DESCRIPTOR_HANDLE, const FLOAT*, UINT, const D3D12_RECT*) = nullptr;
	void(STDMETHODCALLTYPE* setMarker)(ID3D12GraphicsCommandList*, UINT, const void*, UINT) = nullptr;
	void(STDMETHODCALLTYPE* beginEvent)(ID3D12GraphicsCommandList*, UINT, const void*, UINT) = nullptr;
	void(STDMETHODCALLTYPE* executeIndirect)(ID3D12GraphicsCommandList*, ID3D12CommandSignature*, UINT, ID3D12Resource*, UINT64,
	                                         ID3D12Resource*, UINT64) = nullptr;
	void(STDMETHODCALLTYPE* beginRenderPass)(ID3D12GraphicsCommandList4*, UINT, const D3D12_RENDER_PASS_RENDER_TARGET_DESC*,
	                                         const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC*, D3D12_RENDER_PASS_FLAGS) = nullptr;
	void(STDMETHODCALLTYPE* endRenderPass)(ID3D12GraphicsCommandList4*) = nullptr;
	void(STDMETHODCALLTYPE* barrier)(ID3D12GraphicsCommandList7*, UINT32, const D3D12_BARRIER_GROUP*) = nullptr;
	HRESULT(STDMETHODCALLTYPE* createSwapChain)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**) = nullptr;
	HRESULT(STDMETHODCALLTYPE* createSwapChainForHwnd)(IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*,
	                                                   const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**) = nullptr;
	HRESULT(STDMETHODCALLTYPE* present)(IDXGISwapChain*, UINT, UINT) = nullptr;
	HRESULT(STDMETHODCALLTYPE* present1)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*) = nullptr;
};

const Originals& Original();

// Installs the detours on d3d12.dll's D3D12CreateDevice and dxgi.dll's CreateDXGIFactory, CreateDXGIFactory1 and
// CreateDXGIFactory2, loading both DLLs to find them. Call once, from Main at load, before any listener is needed.
bool InstallDetours(const RED4ext::v1::Sdk* aSdk, RED4ext::v1::PluginHandle aHandle, LogQueue& aLog, const Settings& aSettings);

// Listeners, in the order they're called. At most 4; add them all at load.
void AddListener(HookEvents* aListener);

// From now on, command lists are tracked from their next Reset. Off until a feature first needs it.
void EnableTracking();
bool Tracking();

// Lists reset while this is on record their passes for the frame log.
void SetLogging(bool aLogging);

// Patches a device the detour missed (it was created before the plugin loaded), once its vtable has been checked.
void PatchLateDevice(ID3D12Device* aDevice);
// Patches a swap chain the factory detour missed.
void PatchLateSwapChain(IDXGISwapChain* aSwapChain);

ViewRegistry& Views();
StateTracker& Tracker();

// The game's device: once the main queue is known, the device it belongs to; until then, the latest device the
// detour or a late patch saw (D3D12 gives one device per adapter, so that's the game's unless it made devices on two
// adapters). Every device stored here is held for the session, so the pointer never dangles.
ID3D12Device*      Device();
bool               SeenAtCreation();
void*              ListVtable();  // native command lists' vtable, once seen
ID3D12CommandQueue* MainQueue();  // the queue the DLSS evaluate's list goes to, once seen; held for the session
// The native queue a swap chain presents on, under any Streamline proxy; null if it can't be found.
ID3D12CommandQueue* PresentQueue(IDXGISwapChain* aSwapChain);
void               NoteEvaluateList(ID3D12GraphicsCommandList* aList);
uint32_t           CurrentFrame();
void               SetCurrentFrame(uint32_t aFrame);

// After the device is removed: DRED's unfinished command lists and its page fault, when settings.txt turned DRED on.
std::vector<std::string> DredReport();

// The debug layer's errors and warnings that name BlockRender's objects, since the last call, plus a count of the
// others, when settings.txt turned the debug layer on. Main thread.
std::vector<std::string> DebugMessages();

// A one-line summary for the status file.
std::string Summary();
size_t      PatchedEntries();

// Restores every vtable entry that still holds a patch. The detours stay: RED4ext removes them.
void RemovePatches();
}
```

- [x] **Step 5: Write `src/hooks/D3D12Hooks.cpp`**

```cpp
#include "hooks/D3D12Hooks.h"

#include "hooks/Slots.h"
#include "hooks/StreamlineHooks.h"
#include "hooks/VtablePatch.h"
#include "log/EventNames.h"

#include <d3d12sdklayers.h>
#include <wrl/client.h>

#include <array>
#include <atomic>
#include <cstring>
#include <format>
#include <mutex>

namespace cpmc::d3d
{
namespace
{
using Microsoft::WRL::ComPtr;

using D3D12CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
using D3D12GetDebugInterfaceFn = HRESULT(WINAPI*)(REFIID, void**);
using CreateFactoryFn = HRESULT(WINAPI*)(REFIID, void**);
using CreateFactory2Fn = HRESULT(WINAPI*)(UINT, REFIID, void**);

LogQueue*                    g_log = nullptr;
Settings                     g_settings;
Originals                    g_original;
VtablePatcher                g_patcher;
ViewRegistry                 g_views;
StateTracker                 g_tracker;
std::array<HookEvents*, 4>   g_listeners{};
size_t                       g_listenerCount = 0;
std::atomic<bool>            g_tracking{ false };
std::atomic<bool>            g_logging{ false };
std::atomic<uint32_t>        g_frame{ 0 };
std::atomic<ID3D12Device*>   g_device{ nullptr };      // the latest device the detour or a late patch saw
std::atomic<ID3D12Device*>   g_mainDevice{ nullptr };  // the main queue's device
std::atomic<bool>            g_devicePatched{ false };
std::atomic<bool>            g_seenAtCreation{ false };
std::atomic<void*>           g_listVtable{ nullptr };
std::atomic<ID3D12CommandQueue*>        g_mainQueue{ nullptr };
std::atomic<ID3D12GraphicsCommandList*> g_evaluateList{ nullptr };
std::mutex                   g_patchMutex;  // device, factory and swap-chain patching

D3D12CreateDeviceFn      g_createDevice = nullptr;
D3D12GetDebugInterfaceFn g_getDebugInterface = nullptr;
CreateFactoryFn          g_createFactory = nullptr;
CreateFactoryFn          g_createFactory1 = nullptr;
CreateFactory2Fn         g_createFactory2 = nullptr;

void Log(std::string aLine)
{
	if (g_log)
	{
		g_log->Push(std::move(aLine));
	}
}

template<class Fn>
void Each(Fn&& aFn)
{
	for (size_t i = 0; i < g_listenerCount; ++i)
	{
		aFn(*g_listeners[i]);
	}
}

// The list's state, if it's being tracked and is recording.
ListState* Recording(void* aList)
{
	ListState* state = g_tracker.Find(aList);
	return state && state->Recording() ? state : nullptr;
}

ViewInfo MakeView(ViewInfo::Kind aKind, ID3D12Resource* aResource, uint32_t aViewFormat, uint32_t aMip, uint32_t aPlane,
                  uint32_t aFirstSlice, bool aReadOnlyDepth, bool aReadOnlyStencil)
{
	ViewInfo view;
	view.kind = aKind;
	view.resource = aResource;
	if (aResource)
	{
		const D3D12_RESOURCE_DESC desc = aResource->GetDesc();
		view.resourceFormat = desc.Format;
		view.width = desc.Width;
		view.height = desc.Height;
		view.resourceFlags = desc.Flags;
	}
	view.viewFormat = aViewFormat != DXGI_FORMAT_UNKNOWN ? aViewFormat : view.resourceFormat;
	view.mip = aMip;
	view.plane = aPlane;
	view.firstSlice = aFirstSlice;
	view.readOnlyDepth = aReadOnlyDepth;
	view.readOnlyStencil = aReadOnlyStencil;
	return view;
}

// ---- device -------------------------------------------------------------------------------------------------------

void STDMETHODCALLTYPE CreateRtv(ID3D12Device* aDevice, ID3D12Resource* aResource, const D3D12_RENDER_TARGET_VIEW_DESC* aDesc,
                                 D3D12_CPU_DESCRIPTOR_HANDLE aHandle)
{
	g_original.createRtv(aDevice, aResource, aDesc, aHandle);
	uint32_t mip = 0, plane = 0, slice = 0;
	if (aDesc)
	{
		switch (aDesc->ViewDimension)
		{
		case D3D12_RTV_DIMENSION_TEXTURE2D:
			mip = aDesc->Texture2D.MipSlice;
			plane = aDesc->Texture2D.PlaneSlice;
			break;
		case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
			mip = aDesc->Texture2DArray.MipSlice;
			plane = aDesc->Texture2DArray.PlaneSlice;
			slice = aDesc->Texture2DArray.FirstArraySlice;
			break;
		default: break;
		}
	}
	g_views.Record(aHandle.ptr, MakeView(ViewInfo::Kind::Rtv, aResource, aDesc ? aDesc->Format : DXGI_FORMAT_UNKNOWN, mip,
	                                      plane, slice, false, false));
}

void STDMETHODCALLTYPE CreateDsv(ID3D12Device* aDevice, ID3D12Resource* aResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* aDesc,
                                 D3D12_CPU_DESCRIPTOR_HANDLE aHandle)
{
	g_original.createDsv(aDevice, aResource, aDesc, aHandle);
	uint32_t mip = 0, slice = 0;
	if (aDesc)
	{
		switch (aDesc->ViewDimension)
		{
		case D3D12_DSV_DIMENSION_TEXTURE2D: mip = aDesc->Texture2D.MipSlice; break;
		case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
			mip = aDesc->Texture2DArray.MipSlice;
			slice = aDesc->Texture2DArray.FirstArraySlice;
			break;
		default: break;
		}
	}
	const bool readOnlyDepth = aDesc && (aDesc->Flags & D3D12_DSV_FLAG_READ_ONLY_DEPTH);
	const bool readOnlyStencil = aDesc && (aDesc->Flags & D3D12_DSV_FLAG_READ_ONLY_STENCIL);
	g_views.Record(aHandle.ptr, MakeView(ViewInfo::Kind::Dsv, aResource, aDesc ? aDesc->Format : DXGI_FORMAT_UNKNOWN, mip, 0,
	                                      slice, readOnlyDepth, readOnlyStencil));
}

std::optional<ViewInfo::Kind> KindOf(D3D12_DESCRIPTOR_HEAP_TYPE aType)
{
	if (aType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
	{
		return ViewInfo::Kind::Rtv;
	}
	if (aType == D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
	{
		return ViewInfo::Kind::Dsv;
	}
	return std::nullopt;
}

void STDMETHODCALLTYPE CopyDescriptors(ID3D12Device* aDevice, UINT aDestRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* aDestStarts,
                                       const UINT* aDestSizes, UINT aSourceRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* aSourceStarts,
                                       const UINT* aSourceSizes, D3D12_DESCRIPTOR_HEAP_TYPE aType)
{
	g_original.copyDescriptors(aDevice, aDestRanges, aDestStarts, aDestSizes, aSourceRanges, aSourceStarts, aSourceSizes, aType);
	const auto kind = KindOf(aType);
	if (!kind)
	{
		return;
	}
	static_assert(sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) == sizeof(uint64_t));
	const auto runs = CopyRuns(aDestRanges, reinterpret_cast<const uint64_t*>(aDestStarts), aDestSizes, aSourceRanges,
	                           reinterpret_cast<const uint64_t*>(aSourceStarts), aSourceSizes, g_views.Increment(*kind));
	for (const DescriptorRun& run : runs)
	{
		g_views.Copy(*kind, run.dest, run.source, run.count);
	}
}

void STDMETHODCALLTYPE CopyDescriptorsSimple(ID3D12Device* aDevice, UINT aCount, D3D12_CPU_DESCRIPTOR_HANDLE aDest,
                                             D3D12_CPU_DESCRIPTOR_HANDLE aSource, D3D12_DESCRIPTOR_HEAP_TYPE aType)
{
	g_original.copyDescriptorsSimple(aDevice, aCount, aDest, aSource, aType);
	if (const auto kind = KindOf(aType))
	{
		g_views.Copy(*kind, aDest.ptr, aSource.ptr, aCount);
	}
}

// ---- command queue ------------------------------------------------------------------------------------------------

void STDMETHODCALLTYPE ExecuteCommandLists(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists)
{
	if (ID3D12GraphicsCommandList* evaluate = g_evaluateList.load())
	{
		for (UINT i = 0; i < aCount; ++i)
		{
			if (aLists[i] == evaluate && g_evaluateList.compare_exchange_strong(evaluate, nullptr))
			{
				// Both are held for the session: GetDevice's reference, and one more on the queue.
				ID3D12Device* device = nullptr;
				if (SUCCEEDED(aQueue->GetDevice(IID_PPV_ARGS(&device))))
				{
					g_mainDevice = device;
				}
				aQueue->AddRef();
				g_mainQueue = aQueue;
				Log(std::format("main queue 0x{:x}, on device 0x{:x}{}", reinterpret_cast<uintptr_t>(aQueue),
				                reinterpret_cast<uintptr_t>(device),
				                device == g_device.load() ? ", the one the detour saw" : ", NOT the one the detour saw"));
				break;
			}
		}
	}
	Each([&](HookEvents& aListener) { aListener.OnSubmit(aQueue, aCount, aLists); });
	g_original.executeCommandLists(aQueue, aCount, aLists);
	Each([&](HookEvents& aListener) { aListener.OnSubmitted(aQueue, aCount, aLists); });
}

// ---- command lists ------------------------------------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE Reset(ID3D12GraphicsCommandList* aList, ID3D12CommandAllocator* aAllocator, ID3D12PipelineState* aState)
{
	const HRESULT result = g_original.reset(aList, aAllocator, aState);
	if (SUCCEEDED(result) && g_tracking.load(std::memory_order_relaxed))
	{
		g_tracker.ForReset(aList).Begin(g_frame.load(std::memory_order_relaxed), g_logging.load(std::memory_order_relaxed));
	}
	return result;
}

HRESULT STDMETHODCALLTYPE Close(ID3D12GraphicsCommandList* aList)
{
	if (ListState* state = Recording(aList))
	{
		Each([&](HookEvents& aListener) { aListener.OnPassEnd(aList, *state, true); });
		Each([&](HookEvents& aListener) { aListener.OnClose(aList, *state); });
		state->End();
	}
	return g_original.close(aList);
}

void STDMETHODCALLTYPE DrawInstanced(ID3D12GraphicsCommandList* aList, UINT aVertices, UINT aInstances, UINT aFirstVertex, UINT aFirstInstance)
{
	if (ListState* state = Recording(aList))
	{
		state->Count(ListState::Counter::Draw);
	}
	g_original.drawInstanced(aList, aVertices, aInstances, aFirstVertex, aFirstInstance);
}

void STDMETHODCALLTYPE DrawIndexedInstanced(ID3D12GraphicsCommandList* aList, UINT aIndices, UINT aInstances, UINT aFirstIndex,
                                            INT aBaseVertex, UINT aFirstInstance)
{
	if (ListState* state = Recording(aList))
	{
		state->Count(ListState::Counter::IndexedDraw);
	}
	g_original.drawIndexedInstanced(aList, aIndices, aInstances, aFirstIndex, aBaseVertex, aFirstInstance);
}

void STDMETHODCALLTYPE Dispatch(ID3D12GraphicsCommandList* aList, UINT aX, UINT aY, UINT aZ)
{
	if (ListState* state = Recording(aList))
	{
		state->Count(ListState::Counter::Dispatch);
	}
	g_original.dispatch(aList, aX, aY, aZ);
}

void STDMETHODCALLTYPE ResourceBarrier(ID3D12GraphicsCommandList* aList, UINT aCount, const D3D12_RESOURCE_BARRIER* aBarriers)
{
	if (ListState* state = Recording(aList))
	{
		state->Count(ListState::Counter::Barrier);
		for (UINT i = 0; i < aCount; ++i)
		{
			const D3D12_RESOURCE_BARRIER& barrier = aBarriers[i];
			if (barrier.Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
			{
				continue;
			}
			// Only whole-resource transitions say what state the whole resource is in.
			const bool whole = barrier.Transition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			const bool beginOnly = (barrier.Flags & D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY) != 0;
			state->Transition(barrier.Transition.pResource, barrier.Transition.StateAfter, beginOnly || !whole);
		}
	}
	g_original.resourceBarrier(aList, aCount, aBarriers);
}

void STDMETHODCALLTYPE ExecuteBundle(ID3D12GraphicsCommandList* aList, ID3D12GraphicsCommandList* aBundle)
{
	if (ListState* state = Recording(aList))
	{
		state->Count(ListState::Counter::Bundle);
	}
	g_original.executeBundle(aList, aBundle);
}

BoundTarget Bound(uint64_t aHandle)
{
	return { aHandle, g_views.Find(aHandle) };
}

void STDMETHODCALLTYPE OMSetRenderTargets(ID3D12GraphicsCommandList* aList, UINT aCount, const D3D12_CPU_DESCRIPTOR_HANDLE* aRtvs,
                                          BOOL aSingleRange, const D3D12_CPU_DESCRIPTOR_HANDLE* aDsv)
{
	if (ListState* state = Recording(aList))
	{
		Each([&](HookEvents& aListener) { aListener.OnPassEnd(aList, *state, false); });
		Targets  targets;
		const uint64_t step = g_views.Increment(ViewInfo::Kind::Rtv);
		for (UINT i = 0; aRtvs && i < aCount && i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		{
			targets.rtvs.push_back(Bound(aSingleRange ? aRtvs[0].ptr + i * step : aRtvs[i].ptr));
		}
		if (aDsv)
		{
			targets.dsv = Bound(aDsv->ptr);
		}
		state->SetTargets(std::move(targets));
	}
	g_original.omSetRenderTargets(aList, aCount, aRtvs, aSingleRange, aDsv);
}

void STDMETHODCALLTYPE ClearDsv(ID3D12GraphicsCommandList* aList, D3D12_CPU_DESCRIPTOR_HANDLE aDsv, D3D12_CLEAR_FLAGS aFlags,
                                FLOAT aDepth, UINT8 aStencil, UINT aRects, const D3D12_RECT* aRectList)
{
	if (ListState* state = Recording(aList))
	{
		state->Count(ListState::Counter::Clear);
	}
	g_original.clearDsv(aList, aDsv, aFlags, aDepth, aStencil, aRects, aRectList);
}

void STDMETHODCALLTYPE ClearRtv(ID3D12GraphicsCommandList* aList, D3D12_CPU_DESCRIPTOR_HANDLE aRtv, const FLOAT* aColour, UINT aRects,
                                const D3D12_RECT* aRectList)
{
	if (ListState* state = Recording(aList))
	{
		state->Count(ListState::Counter::Clear);
	}
	g_original.clearRtv(aList, aRtv, aColour, aRects, aRectList);
}

void STDMETHODCALLTYPE SetMarker(ID3D12GraphicsCommandList* aList, UINT aMetadata, const void* aData, UINT aSize)
{
	if (ListState* state = Recording(aList); state && state->Logging())
	{
		state->Event(EventName(aMetadata, aData, aSize));
	}
	g_original.setMarker(aList, aMetadata, aData, aSize);
}

void STDMETHODCALLTYPE BeginEvent(ID3D12GraphicsCommandList* aList, UINT aMetadata, const void* aData, UINT aSize)
{
	if (ListState* state = Recording(aList); state && state->Logging())
	{
		state->Event(EventName(aMetadata, aData, aSize));
	}
	g_original.beginEvent(aList, aMetadata, aData, aSize);
}

void STDMETHODCALLTYPE ExecuteIndirect(ID3D12GraphicsCommandList* aList, ID3D12CommandSignature* aSignature, UINT aMaxCommands,
                                       ID3D12Resource* aArguments, UINT64 aArgumentOffset, ID3D12Resource* aCount, UINT64 aCountOffset)
{
	if (ListState* state = Recording(aList))
	{
		state->Count(ListState::Counter::Indirect);
	}
	g_original.executeIndirect(aList, aSignature, aMaxCommands, aArguments, aArgumentOffset, aCount, aCountOffset);
}

void STDMETHODCALLTYPE BeginRenderPass(ID3D12GraphicsCommandList4* aList, UINT aCount, const D3D12_RENDER_PASS_RENDER_TARGET_DESC* aTargets,
                                       const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* aDepth, D3D12_RENDER_PASS_FLAGS aFlags)
{
	if (ListState* state = Recording(aList))
	{
		Each([&](HookEvents& aListener) { aListener.OnPassEnd(aList, *state, false); });
		Targets targets;
		for (UINT i = 0; aTargets && i < aCount && i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		{
			targets.rtvs.push_back(Bound(aTargets[i].cpuDescriptor.ptr));
		}
		if (aDepth)
		{
			targets.dsv = Bound(aDepth->cpuDescriptor.ptr);
		}
		state->SetTargets(std::move(targets), true);
	}
	g_original.beginRenderPass(aList, aCount, aTargets, aDepth, aFlags);
}

void STDMETHODCALLTYPE EndRenderPass(ID3D12GraphicsCommandList4* aList)
{
	g_original.endRenderPass(aList);
	if (ListState* state = Recording(aList))
	{
		// Outside the render pass now, so a listener may record into the list.
		Each([&](HookEvents& aListener) { aListener.OnPassEnd(aList, *state, false); });
		state->SetTargets({});
	}
}

void STDMETHODCALLTYPE Barrier(ID3D12GraphicsCommandList7* aList, UINT32 aGroups, const D3D12_BARRIER_GROUP* aBarrierGroups)
{
	if (ListState* state = Recording(aList))
	{
		state->Count(ListState::Counter::EnhancedBarrier);
	}
	g_original.barrier(aList, aGroups, aBarrierGroups);
}

// ---- DXGI ---------------------------------------------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE Present(IDXGISwapChain* aSwapChain, UINT aSyncInterval, UINT aFlags)
{
	if ((aFlags & DXGI_PRESENT_TEST) == 0)
	{
		Each([&](HookEvents& aListener) { aListener.OnPresent(aSwapChain); });
	}
	return g_original.present(aSwapChain, aSyncInterval, aFlags);
}

HRESULT STDMETHODCALLTYPE Present1(IDXGISwapChain1* aSwapChain, UINT aSyncInterval, UINT aFlags, const DXGI_PRESENT_PARAMETERS* aParameters)
{
	if ((aFlags & DXGI_PRESENT_TEST) == 0)
	{
		Each([&](HookEvents& aListener) { aListener.OnPresent(aSwapChain); });
	}
	return g_original.present1(aSwapChain, aSyncInterval, aFlags, aParameters);
}

// Patches one entry, once. The original is stored before the entry points at the hook, so no thread can reach the
// hook first. A second vtable whose entry holds a different function is left alone: each hook calls one original.
template<class Fn>
void PatchEntry(void* aObject, size_t aSlot, void* aHook, Fn& aOriginal, const char* aName)
{
	void* current = (*static_cast<void***>(aObject))[aSlot];
	if (current == aHook)
	{
		return;
	}
	if (aOriginal && current != reinterpret_cast<void*>(aOriginal))
	{
		Log(std::format("{}: a second vtable, left unpatched", aName));
		return;
	}
	aOriginal = reinterpret_cast<Fn>(current);
	if (!g_patcher.Patch(aObject, aSlot, aHook))
	{
		Log(std::format("couldn't patch {}", aName));
	}
}

void PatchSwapChain(IDXGISwapChain* aSwapChain)
{
	std::lock_guard lock(g_patchMutex);
	PatchEntry(aSwapChain, slots::kPresent, reinterpret_cast<void*>(&Present), g_original.present, "IDXGISwapChain::Present");
	ComPtr<IDXGISwapChain1> swapChain1;
	if (SUCCEEDED(aSwapChain->QueryInterface(IID_PPV_ARGS(&swapChain1))))
	{
		PatchEntry(swapChain1.Get(), slots::kPresent1, reinterpret_cast<void*>(&Present1), g_original.present1, "IDXGISwapChain1::Present1");
	}
}

HRESULT STDMETHODCALLTYPE CreateSwapChain(IDXGIFactory* aFactory, IUnknown* aDevice, DXGI_SWAP_CHAIN_DESC* aDesc, IDXGISwapChain** aSwapChain)
{
	const HRESULT result = g_original.createSwapChain(aFactory, aDevice, aDesc, aSwapChain);
	if (SUCCEEDED(result) && aSwapChain && *aSwapChain)
	{
		PatchSwapChain(*aSwapChain);
		Log("swap chain created (CreateSwapChain): Present patched");
	}
	return result;
}

HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd(IDXGIFactory2* aFactory, IUnknown* aDevice, HWND aWindow, const DXGI_SWAP_CHAIN_DESC1* aDesc,
                                                 const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* aFullscreen, IDXGIOutput* aOutput, IDXGISwapChain1** aSwapChain)
{
	const HRESULT result = g_original.createSwapChainForHwnd(aFactory, aDevice, aWindow, aDesc, aFullscreen, aOutput, aSwapChain);
	if (SUCCEEDED(result) && aSwapChain && *aSwapChain)
	{
		PatchSwapChain(*aSwapChain);
		Log("swap chain created (CreateSwapChainForHwnd): Present patched");
	}
	return result;
}

void PatchFactory(void* aFactory)
{
	auto* unknown = static_cast<IUnknown*>(aFactory);
	ComPtr<IDXGIFactory2> factory;
	if (FAILED(unknown->QueryInterface(IID_PPV_ARGS(&factory))))
	{
		return;
	}
	std::lock_guard lock(g_patchMutex);
	PatchEntry(factory.Get(), slots::kCreateSwapChain, reinterpret_cast<void*>(&CreateSwapChain), g_original.createSwapChain,
	           "IDXGIFactory::CreateSwapChain");
	PatchEntry(factory.Get(), slots::kCreateSwapChainForHwnd, reinterpret_cast<void*>(&CreateSwapChainForHwnd),
	           g_original.createSwapChainForHwnd, "IDXGIFactory2::CreateSwapChainForHwnd");
}

HRESULT WINAPI CreateFactoryDetour(REFIID aId, void** aFactory)
{
	const HRESULT result = g_createFactory(aId, aFactory);
	if (SUCCEEDED(result) && aFactory && *aFactory)
	{
		PatchFactory(*aFactory);
	}
	return result;
}

HRESULT WINAPI CreateFactory1Detour(REFIID aId, void** aFactory)
{
	const HRESULT result = g_createFactory1(aId, aFactory);
	if (SUCCEEDED(result) && aFactory && *aFactory)
	{
		PatchFactory(*aFactory);
	}
	return result;
}

HRESULT WINAPI CreateFactory2Detour(UINT aFlags, REFIID aId, void** aFactory)
{
	const HRESULT result = g_createFactory2(aFlags, aId, aFactory);
	if (SUCCEEDED(result) && aFactory && *aFactory)
	{
		PatchFactory(*aFactory);
	}
	return result;
}

// ---- the device ---------------------------------------------------------------------------------------------------

void PatchDevice(ID3D12Device* aDevice)
{
	std::lock_guard lock(g_patchMutex);
	if (g_device.load() == aDevice)
	{
		return;
	}
	aDevice->AddRef();  // held for the session, so Device() never hands out a released device
	g_device = aDevice;
	if (g_devicePatched.exchange(true))
	{
		Log(std::format("another device 0x{:x}: the vtables it shares are already patched", reinterpret_cast<uintptr_t>(aDevice)));
		return;
	}
	g_views.SetIncrements(aDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV),
	                      aDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
	PatchEntry(aDevice, slots::kCreateRenderTargetView, reinterpret_cast<void*>(&CreateRtv), g_original.createRtv, "CreateRenderTargetView");
	PatchEntry(aDevice, slots::kCreateDepthStencilView, reinterpret_cast<void*>(&CreateDsv), g_original.createDsv, "CreateDepthStencilView");
	PatchEntry(aDevice, slots::kCopyDescriptors, reinterpret_cast<void*>(&CopyDescriptors), g_original.copyDescriptors, "CopyDescriptors");
	PatchEntry(aDevice, slots::kCopyDescriptorsSimple, reinterpret_cast<void*>(&CopyDescriptorsSimple), g_original.copyDescriptorsSimple,
	           "CopyDescriptorsSimple");

	// A queue, an allocator and a list of our own, only to reach the vtables every queue and list shares.
	D3D12_COMMAND_QUEUE_DESC queueDesc{};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ComPtr<ID3D12CommandQueue>        queue;
	ComPtr<ID3D12CommandAllocator>    allocator;
	ComPtr<ID3D12GraphicsCommandList> list;
	if (FAILED(aDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue))) ||
	    FAILED(aDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) ||
	    FAILED(aDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list))))
	{
		Log("couldn't create a queue and list to reach their vtables: command lists aren't tracked");
		return;
	}
	list->Close();
	PatchEntry(queue.Get(), slots::kExecuteCommandLists, reinterpret_cast<void*>(&ExecuteCommandLists), g_original.executeCommandLists,
	           "ExecuteCommandLists");
	void* l = list.Get();
	PatchEntry(l, slots::kClose, reinterpret_cast<void*>(&Close), g_original.close, "Close");
	PatchEntry(l, slots::kReset, reinterpret_cast<void*>(&Reset), g_original.reset, "Reset");
	PatchEntry(l, slots::kDrawInstanced, reinterpret_cast<void*>(&DrawInstanced), g_original.drawInstanced, "DrawInstanced");
	PatchEntry(l, slots::kDrawIndexedInstanced, reinterpret_cast<void*>(&DrawIndexedInstanced), g_original.drawIndexedInstanced,
	           "DrawIndexedInstanced");
	PatchEntry(l, slots::kDispatch, reinterpret_cast<void*>(&Dispatch), g_original.dispatch, "Dispatch");
	PatchEntry(l, slots::kResourceBarrier, reinterpret_cast<void*>(&ResourceBarrier), g_original.resourceBarrier, "ResourceBarrier");
	PatchEntry(l, slots::kExecuteBundle, reinterpret_cast<void*>(&ExecuteBundle), g_original.executeBundle, "ExecuteBundle");
	PatchEntry(l, slots::kOMSetRenderTargets, reinterpret_cast<void*>(&OMSetRenderTargets), g_original.omSetRenderTargets,
	           "OMSetRenderTargets");
	PatchEntry(l, slots::kClearDepthStencilView, reinterpret_cast<void*>(&ClearDsv), g_original.clearDsv, "ClearDepthStencilView");
	PatchEntry(l, slots::kClearRenderTargetView, reinterpret_cast<void*>(&ClearRtv), g_original.clearRtv, "ClearRenderTargetView");
	PatchEntry(l, slots::kSetMarker, reinterpret_cast<void*>(&SetMarker), g_original.setMarker, "SetMarker");
	PatchEntry(l, slots::kBeginEvent, reinterpret_cast<void*>(&BeginEvent), g_original.beginEvent, "BeginEvent");
	PatchEntry(l, slots::kExecuteIndirect, reinterpret_cast<void*>(&ExecuteIndirect), g_original.executeIndirect, "ExecuteIndirect");
	// The later entries exist only if the list is a version-4 and version-7 list.
	ComPtr<ID3D12GraphicsCommandList4> list4;
	if (SUCCEEDED(list.As(&list4)))
	{
		PatchEntry(l, slots::kBeginRenderPass, reinterpret_cast<void*>(&BeginRenderPass), g_original.beginRenderPass, "BeginRenderPass");
		PatchEntry(l, slots::kEndRenderPass, reinterpret_cast<void*>(&EndRenderPass), g_original.endRenderPass, "EndRenderPass");
	}
	ComPtr<ID3D12GraphicsCommandList7> list7;
	if (SUCCEEDED(list.As(&list7)))
	{
		PatchEntry(l, slots::kBarrier, reinterpret_cast<void*>(&Barrier), g_original.barrier, "Barrier");
	}
	g_listVtable = *static_cast<void**>(l);
	Log(std::format("device 0x{:x} patched: {} vtable entries", reinterpret_cast<uintptr_t>(aDevice), g_patcher.Count()));
}

void EnableDebugTools()
{
	if (!g_getDebugInterface || (!g_settings.dred && !g_settings.debugLayer))
	{
		return;
	}
	if (g_settings.debugLayer)
	{
		ComPtr<ID3D12Debug> debug;
		if (SUCCEEDED(g_getDebugInterface(IID_PPV_ARGS(&debug))))
		{
			debug->EnableDebugLayer();
			Log("D3D12 debug layer on");
		}
		else
		{
			Log("couldn't turn the D3D12 debug layer on");
		}
	}
	if (g_settings.dred)
	{
		ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dred;
		if (SUCCEEDED(g_getDebugInterface(IID_PPV_ARGS(&dred))))
		{
			dred->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
			dred->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
			Log("DRED on");
		}
		else
		{
			Log("couldn't turn DRED on");
		}
	}
}

// The adapter's name, for the log.
std::string AdapterName(IUnknown* aAdapter)
{
	if (!aAdapter)
	{
		return "the default adapter";
	}
	ComPtr<IDXGIAdapter> adapter;
	DXGI_ADAPTER_DESC    desc{};
	if (FAILED(aAdapter->QueryInterface(IID_PPV_ARGS(&adapter))) || FAILED(adapter->GetDesc(&desc)))
	{
		return "an adapter that doesn't say what it is";
	}
	std::string name;
	for (size_t i = 0; i < std::size(desc.Description) && desc.Description[i] != 0; ++i)
	{
		const wchar_t ch = desc.Description[i];
		name += ch >= 32 && ch < 127 ? char(ch) : '?';
	}
	return name;
}

HRESULT WINAPI CreateDeviceDetour(IUnknown* aAdapter, D3D_FEATURE_LEVEL aLevel, REFIID aId, void** aDevice)
{
	if (aDevice && !g_device.load())
	{
		EnableDebugTools();
	}
	const HRESULT result = g_createDevice(aAdapter, aLevel, aId, aDevice);
	if (SUCCEEDED(result) && aDevice && *aDevice)
	{
		ComPtr<ID3D12Device> device;
		if (SUCCEEDED(static_cast<IUnknown*>(*aDevice)->QueryInterface(IID_PPV_ARGS(&device))))
		{
			g_seenAtCreation = true;
			Log(std::format("device 0x{:x} created on {}", reinterpret_cast<uintptr_t>(device.Get()), AdapterName(aAdapter)));
			PatchDevice(device.Get());
		}
	}
	return result;
}

template<class Fn>
bool Detour(const RED4ext::v1::Sdk* aSdk, RED4ext::v1::PluginHandle aHandle, HMODULE aModule, const char* aName, void* aDetour, Fn& aOriginal)
{
	void* target = aModule ? reinterpret_cast<void*>(GetProcAddress(aModule, aName)) : nullptr;
	if (!target || !aSdk->hooking->Attach(aHandle, target, aDetour, reinterpret_cast<void**>(&aOriginal)))
	{
		Log(std::format("couldn't detour {}", aName));
		return false;
	}
	return true;
}
}

const Originals& Original()
{
	return g_original;
}

bool InstallDetours(const RED4ext::v1::Sdk* aSdk, RED4ext::v1::PluginHandle aHandle, LogQueue& aLog, const Settings& aSettings)
{
	g_log = &aLog;
	g_settings = aSettings;
	const HMODULE d3d12 = LoadLibraryW(L"d3d12.dll");
	const HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
	g_getDebugInterface = d3d12 ? reinterpret_cast<D3D12GetDebugInterfaceFn>(GetProcAddress(d3d12, "D3D12GetDebugInterface")) : nullptr;
	bool ok = Detour(aSdk, aHandle, d3d12, "D3D12CreateDevice", reinterpret_cast<void*>(&CreateDeviceDetour), g_createDevice);
	ok = Detour(aSdk, aHandle, dxgi, "CreateDXGIFactory", reinterpret_cast<void*>(&CreateFactoryDetour), g_createFactory) && ok;
	ok = Detour(aSdk, aHandle, dxgi, "CreateDXGIFactory1", reinterpret_cast<void*>(&CreateFactory1Detour), g_createFactory1) && ok;
	ok = Detour(aSdk, aHandle, dxgi, "CreateDXGIFactory2", reinterpret_cast<void*>(&CreateFactory2Detour), g_createFactory2) && ok;
	return ok;
}

void AddListener(HookEvents* aListener)
{
	if (g_listenerCount < g_listeners.size())
	{
		g_listeners[g_listenerCount++] = aListener;
	}
}

void EnableTracking()
{
	g_tracking = true;
}

bool Tracking()
{
	return g_tracking.load();
}

void SetLogging(bool aLogging)
{
	g_logging = aLogging;
}

void PatchLateDevice(ID3D12Device* aDevice)
{
	if (aDevice && !g_device.load())
	{
		PatchDevice(aDevice);
	}
}

void PatchLateSwapChain(IDXGISwapChain* aSwapChain)
{
	if (aSwapChain)
	{
		PatchSwapChain(aSwapChain);
	}
}

ViewRegistry& Views()
{
	return g_views;
}

StateTracker& Tracker()
{
	return g_tracker;
}

ID3D12Device* Device()
{
	if (ID3D12Device* main = g_mainDevice.load())
	{
		return main;
	}
	return g_device.load();
}

bool SeenAtCreation()
{
	return g_seenAtCreation.load();
}

void* ListVtable()
{
	return g_listVtable.load();
}

ID3D12CommandQueue* MainQueue()
{
	return g_mainQueue.load();
}

ID3D12CommandQueue* PresentQueue(IDXGISwapChain* aSwapChain)
{
	ComPtr<ID3D12CommandQueue> queue;
	if (!aSwapChain || FAILED(aSwapChain->GetDevice(IID_PPV_ARGS(&queue))))
	{
		return nullptr;
	}
	if (void* native = slhooks::NativeInterface(queue.Get()))
	{
		static_cast<IUnknown*>(native)->Release();  // the proxy holds it
		return static_cast<ID3D12CommandQueue*>(native);
	}
	return queue.Get();  // the swap chain holds it
}

void NoteEvaluateList(ID3D12GraphicsCommandList* aList)
{
	if (aList && !g_mainQueue.load())
	{
		g_evaluateList = aList;
	}
}

uint32_t CurrentFrame()
{
	return g_frame.load(std::memory_order_relaxed);
}

void SetCurrentFrame(uint32_t aFrame)
{
	g_frame.store(aFrame, std::memory_order_relaxed);
}

namespace
{
std::string NameOf(const char* aNarrow, const wchar_t* aWide)
{
	if (aNarrow)
	{
		return aNarrow;
	}
	std::string name;
	for (size_t i = 0; aWide && aWide[i] != 0 && i < 128; ++i)
	{
		name += aWide[i] >= 32 && aWide[i] < 127 ? char(aWide[i]) : '?';
	}
	return name.empty() ? "(no name)" : name;
}
}

std::vector<std::string> DredReport()
{
	std::vector<std::string>                       lines;
	ID3D12Device* const                            device = Device();
	ComPtr<ID3D12DeviceRemovedExtendedData>        dred;
	if (!g_settings.dred || !device || FAILED(device->QueryInterface(IID_PPV_ARGS(&dred))))
	{
		return lines;
	}
	D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbs{};
	if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput(&breadcrumbs)))
	{
		for (const D3D12_AUTO_BREADCRUMB_NODE* node = breadcrumbs.pHeadAutoBreadcrumbNode; node && lines.size() < 32; node = node->pNext)
		{
			const UINT32 done = node->pLastBreadcrumbValue ? *node->pLastBreadcrumbValue : 0;
			if (done >= node->BreadcrumbCount)
			{
				continue;  // it finished
			}
			lines.push_back(std::format("DRED: list {} on queue {} stopped after {} of {} operations; the next was operation {}",
			                            NameOf(node->pCommandListDebugNameA, node->pCommandListDebugNameW),
			                            NameOf(node->pCommandQueueDebugNameA, node->pCommandQueueDebugNameW), done, node->BreadcrumbCount,
			                            node->pCommandHistory ? int(node->pCommandHistory[done]) : -1));
		}
	}
	D3D12_DRED_PAGE_FAULT_OUTPUT fault{};
	if (SUCCEEDED(dred->GetPageFaultAllocationOutput(&fault)) && fault.PageFaultVA != 0)
	{
		lines.push_back(std::format("DRED: page fault at GPU address 0x{:x}", fault.PageFaultVA));
		size_t shown = 0;
		for (const D3D12_DRED_ALLOCATION_NODE* node = fault.pHeadExistingAllocationNode; node && shown < 16; node = node->pNext, ++shown)
		{
			lines.push_back("DRED: allocation there: " + NameOf(node->ObjectNameA, node->ObjectNameW));
		}
		for (const D3D12_DRED_ALLOCATION_NODE* node = fault.pHeadRecentFreedAllocationNode; node && shown < 32; node = node->pNext, ++shown)
		{
			lines.push_back("DRED: recently freed there: " + NameOf(node->ObjectNameA, node->ObjectNameW));
		}
	}
	if (lines.empty())
	{
		lines.push_back("DRED: no unfinished lists and no page fault recorded");
	}
	return lines;
}

std::vector<std::string> DebugMessages()
{
	std::vector<std::string> lines;
	ID3D12Device* const      device = Device();
	ComPtr<ID3D12InfoQueue>  queue;
	if (!g_settings.debugLayer || !device || FAILED(device->QueryInterface(IID_PPV_ARGS(&queue))))
	{
		return lines;
	}
	size_t        others = 0;
	const UINT64  count = queue->GetNumStoredMessages();
	std::vector<uint8_t> buffer;
	for (UINT64 i = 0; i < count; ++i)
	{
		SIZE_T size = 0;
		if (FAILED(queue->GetMessage(i, nullptr, &size)) || size < sizeof(D3D12_MESSAGE))
		{
			continue;
		}
		buffer.resize(size);
		auto* message = reinterpret_cast<D3D12_MESSAGE*>(buffer.data());
		if (FAILED(queue->GetMessage(i, message, &size)) || message->Severity > D3D12_MESSAGE_SEVERITY_WARNING || !message->pDescription)
		{
			continue;
		}
		const std::string text(message->pDescription, strnlen(message->pDescription, message->DescriptionByteLength));
		if (text.find("BlockRender") == std::string::npos || lines.size() >= 20)
		{
			++others;
			continue;
		}
		lines.push_back(std::format("D3D12 {}: {}", message->Severity == D3D12_MESSAGE_SEVERITY_WARNING ? "warning" : "error", text));
	}
	queue->ClearStoredMessages();
	if (others > 0)
	{
		lines.push_back(std::format("D3D12: {} more errors and warnings, about the game's own objects or past the first 20", others));
	}
	return lines;
}

std::string Summary()
{
	return std::format("device {}, {} vtable entries, present {}, main queue {}, tracking {}",
	                   g_device.load() ? (g_seenAtCreation.load() ? "seen at creation" : "patched late") : "not seen",
	                   g_patcher.Count(), g_original.present ? "patched" : "not patched", g_mainQueue.load() ? "known" : "unknown",
	                   g_tracking.load() ? "on" : "off");
}

size_t PatchedEntries()
{
	return g_patcher.Count();
}

void RemovePatches()
{
	const size_t restored = g_patcher.RestoreAll();
	Log(std::format("{} vtable entries restored", restored));
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the RED4ext SDK and the runtime library**

```cmake
target_link_libraries(render_core PUBLIC render_settings d3dcompiler dxguid)

# Unit tests: no game needed. They run in this folder, so tests can find shaders/.
```
```cmake
target_link_libraries(render_core PUBLIC render_settings d3dcompiler dxguid)

# The RED4ext SDK, header-only. SYSTEM keeps its warnings out of ours.
set(RED4EXT_HEADER_ONLY ON CACHE BOOL "" FORCE)
add_subdirectory("${RED4EXT_SDK_DIR}" red4ext-sdk EXCLUDE_FROM_ALL SYSTEM)

# What runs inside the game: the hooks and the features they feed. Only the running game can test it.
add_library(render_runtime STATIC
	src/hooks/D3D12Hooks.cpp
	src/hooks/StreamlineHooks.cpp
)
target_link_libraries(render_runtime PUBLIC render_core RED4ext::SDK d3d12)

# Unit tests: no game needed. They run in this folder, so tests can find shaders/.
```

- [x] **Step 7: Build; it must compile without warnings, and the tests still pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ls build/RelWithDebInfo/render_runtime.lib && ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, the library, then `83 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: Streamline import hooks and D3D12 vtable hooks" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 24: GpuApi, and the GPU helpers

- **`GpuApi`** reads the game's own GPU layer (`RED4ext::GpuApi::SDeviceData`) with care:
  - **the address:** it calls RED4ext's resolver itself, because the SDK's own lookup ends the game when an address
    is missing;
  - **the reads:** every one is range-checked;
  - **the objects:** each is trusted only once its vtable lies in the right DLL.

  `Check` compares GpuApi's device with the hooks' device. That settles, in step 1, whether the SDK's offsets hold for
  2.31. `ListNames` finds the game's names for its command lists.
- **`Gpu`** holds small D3D12 helpers for the dumps and the renderer: buffers, copy footprints, and a copy recorded
  into a list with its barriers. The copy refuses states that a compute list can't leave.

**Files:**
- Create: `src/track/GpuApi.h`, `src/track/GpuApi.cpp`, `src/render/Gpu.h`, `src/render/Gpu.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write `src/track/GpuApi.h`**

```cpp
#pragma once

#include "Readable.h"

#include <d3d12.h>
#include <dxgi.h>

#include <map>
#include <string>

// What the game's own GPU layer (REDengine's GpuApi, through RED4ext.SDK's GpuApi::SDeviceData) holds, read with
// care: the address comes from RED4ext's resolver called directly (the SDK's own lookup ends the game when an address
// is missing), every read is range-checked, and an object is trusted only once its vtable lies in the right DLL.
// Nothing confirms the SDK's offsets for 2.31; step 1 checks them by comparing the device found here with the one the
// D3D12CreateDevice detour saw.
namespace cpmc::gpuapi
{
struct Report
{
	bool            found = false;  // the device data's address resolved and is readable
	std::string     device;         // native, proxy, debug layer, other, or missing
	bool            deviceMatches = false;  // it's the detour's device, or Streamline's proxy for it
	std::string     queue;
	std::string     swapChain;
	IDXGISwapChain* swapChainObject = nullptr;  // the first swap chain, if its vtable lies in dxgi.dll or sl.interposer.dll
	ID3D12Device*   nativeDevice = nullptr;     // the device found here, under any proxy, if its vtable is D3D12Core's
};

Report Check(ID3D12Device* aDetourDevice);

// The game's names for its command lists (CommandListContext::debugName). Use one per frame log: it remembers the
// memory it checked and the native list under each Streamline proxy.
class ListNames
{
public:
	~ListNames();

	// The name of the context recording aNativeList now, or empty.
	std::string Find(ID3D12GraphicsCommandList* aNativeList);

private:
	ReadableMemory                      memory_;
	std::map<void*, void*>              nativeOf_;  // proxy list -> native list (each holding one reference)
};
}
```

- [x] **Step 2: Write `src/track/GpuApi.cpp`**

```cpp
#include "track/GpuApi.h"

#include "hooks/StreamlineHooks.h"

#include <RED4ext/GpuApi/DeviceData.hpp>

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <memory>

namespace cpmc::gpuapi
{
namespace
{
using ResolveFn = uintptr_t (*)(uint32_t);
constexpr uint32_t kDeviceDataHash = 1239944840u;  // RED4ext.SDK's Detail::AddressHashes::g_DeviceData

// The file name of the module holding aAddress, lower case; empty if none.
std::string ModuleOf(const void* aAddress)
{
	HMODULE module = nullptr;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                        static_cast<LPCWSTR>(aAddress), &module))
	{
		return {};
	}
	char path[MAX_PATH]{};
	const DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
	std::string name(path, length);
	name = name.substr(name.find_last_of("\\/") + 1);
	std::transform(name.begin(), name.end(), name.begin(), [](unsigned char aChar) { return char(std::tolower(aChar)); });
	return name;
}

// The module an object's vtable lies in, or empty if the object can't be read.
std::string VtableModule(ReadableMemory& aMemory, const void* aObject)
{
	if (!aObject || !aMemory.Range(aObject, sizeof(void*)))
	{
		return {};
	}
	return ModuleOf(*static_cast<void* const*>(aObject));
}

std::string Describe(const std::string& aModule)
{
	if (aModule.empty())
	{
		return "missing";
	}
	if (aModule == "d3d12core.dll" || aModule == "dxgi.dll")
	{
		return "native";
	}
	if (aModule == "sl.interposer.dll")
	{
		return "proxy";
	}
	if (aModule == "d3d12sdklayers.dll")
	{
		return "debug layer";
	}
	return "other (" + aModule + ")";
}

RED4ext::GpuApi::SDeviceData* DeviceData(ReadableMemory& aMemory)
{
	const HMODULE red4ext = GetModuleHandleW(L"RED4ext.dll");
	const auto    resolve = red4ext ? reinterpret_cast<ResolveFn>(GetProcAddress(red4ext, "RED4ext_ResolveAddress")) : nullptr;
	const auto    address = resolve ? resolve(kDeviceDataHash) : 0;
	if (address == 0 || !aMemory.Range(reinterpret_cast<const void*>(address), sizeof(void*)))
	{
		return nullptr;
	}
	// The struct is 27 MB; check the fields read straight from it. Container entries are checked as they're read.
	auto* data = *reinterpret_cast<RED4ext::GpuApi::SDeviceData**>(address);
	if (!data || !aMemory.Range(std::addressof(data->device), sizeof(data->device)) ||
	    !aMemory.Range(std::addressof(data->directCommandQueue), sizeof(data->directCommandQueue)))
	{
		return nullptr;
	}
	return data;
}

// The CString's text, if every byte of it can be read.
std::string Text(ReadableMemory& aMemory, const RED4ext::CString& aString)
{
	const uint32_t length = aString.length & 0x3FFFFFFF;
	if (length == 0 || length > 256)
	{
		return {};
	}
	const char* text = aString.length < 0x40000000 ? aString.text.inline_str : aString.text.str.ptr;
	if (aString.length < 0x40000000 && length >= sizeof(aString.text.inline_str))
	{
		return {};
	}
	if (!aMemory.Range(text, length))
	{
		return {};
	}
	return std::string(text, length);
}
}

Report Check(ID3D12Device* aDetourDevice)
{
	ReadableMemory                memory;
	Report                        report;
	RED4ext::GpuApi::SDeviceData* data = DeviceData(memory);
	if (!data)
	{
		report.device = report.queue = report.swapChain = "not found";
		return report;
	}
	report.found = true;
	ID3D12Device* device = data->device.Get();
	const std::string deviceModule = VtableModule(memory, device);
	report.device = Describe(deviceModule);
	if (deviceModule == "d3d12core.dll" || deviceModule == "d3d12sdklayers.dll")
	{
		report.nativeDevice = device;
		report.deviceMatches = device == aDetourDevice;
	}
	else if (deviceModule == "sl.interposer.dll")
	{
		if (void* native = slhooks::NativeInterface(device))
		{
			report.nativeDevice = static_cast<ID3D12Device*>(native);
			report.deviceMatches = native == aDetourDevice;
			static_cast<IUnknown*>(native)->Release();
		}
	}
	report.queue = Describe(VtableModule(memory, data->directCommandQueue.Get()));
	report.swapChain = "missing";
	auto& swapChains = data->swapChains;
	for (auto& handle : swapChains.resources)
	{
		if (!memory.Range(&handle, sizeof(handle)) || !handle.IsUsed())
		{
			continue;
		}
		IDXGISwapChain4*  swapChain = handle.instance.swapChain.Get();
		const std::string module = VtableModule(memory, swapChain);
		report.swapChain = Describe(module);
		if (module == "dxgi.dll" || module == "sl.interposer.dll")
		{
			report.swapChainObject = swapChain;
		}
		break;
	}
	return report;
}

ListNames::~ListNames()
{
	for (const auto& [proxy, native] : nativeOf_)
	{
		if (native)
		{
			static_cast<IUnknown*>(native)->Release();
		}
	}
}

std::string ListNames::Find(ID3D12GraphicsCommandList* aNativeList)
{
	RED4ext::GpuApi::SDeviceData* data = DeviceData(memory_);
	if (!data || !aNativeList)
	{
		return {};
	}
	void* const listVtable = *reinterpret_cast<void**>(aNativeList);
	for (auto& handle : data->commandLists.resources)
	{
		if (!memory_.Range(&handle, sizeof(handle)) || !handle.IsUsed())
		{
			continue;
		}
		RED4ext::GpuApi::CommandListContext* context = handle.instance.GetPtr();
		if (!context || !memory_.Range(context, sizeof(*context)))
		{
			continue;
		}
		void* list = context->commandList.Get();
		if (!list || !memory_.Range(list, sizeof(void*)))
		{
			continue;
		}
		if (*static_cast<void**>(list) != listVtable)
		{
			// A Streamline proxy: compare the list underneath, looked up once per log.
			auto found = nativeOf_.find(list);
			if (found == nativeOf_.end())
			{
				found = nativeOf_.emplace(list, slhooks::NativeInterface(list)).first;
			}
			list = found->second;
		}
		if (list == aNativeList)
		{
			return Text(memory_, context->debugName);
		}
	}
	return {};
}
}
```

- [x] **Step 3: Write `src/render/Gpu.h`**

```cpp
#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>

// Small D3D12 helpers the features share.
namespace cpmc::gpu
{
using Microsoft::WRL::ComPtr;

// Where one subresource of a texture lands in a buffer: the footprint D3D12 gives for a copy at offset 0.
struct Footprint
{
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
	uint32_t                           rows = 0;
	uint64_t                           rowBytes = 0;
	uint64_t                           totalBytes = 0;
};

// The plane index a D3D12 subresource number needs: mip 0 and array slice 0 of plane aPlane.
uint32_t PlaneSubresource(const D3D12_RESOURCE_DESC& aDesc, uint32_t aPlane);

// How many planes the resource's format has (2 for depth-stencil formats, else 1).
uint32_t PlaneCount(ID3D12Device* aDevice, DXGI_FORMAT aFormat);

Footprint FootprintOf(ID3D12Device* aDevice, const D3D12_RESOURCE_DESC& aDesc, uint32_t aSubresource);

// A buffer in aHeap (default or readback), in aState.
ComPtr<ID3D12Resource> MakeBuffer(ID3D12Device* aDevice, uint64_t aBytes, D3D12_HEAP_TYPE aHeap, D3D12_RESOURCE_STATES aState);

// Records into aList, which may be the game's: a copy of subresource aSubresource of aSource into aDest at offset 0,
// laid out as aFootprint. Unless aState already allows copying, aSource moves from aState to the copy state and back
// (all subresources), through the original ResourceBarrier so the tracker doesn't see it. False, with nothing
// recorded, if aList isn't a direct list and aState includes a state only direct lists may use.
bool RecordCopy(ID3D12GraphicsCommandList* aList, ID3D12Resource* aSource, uint32_t aSubresource, uint32_t aState,
                ID3D12Resource* aDest, const Footprint& aFootprint);
}
```

- [x] **Step 4: Write `src/render/Gpu.cpp`**

```cpp
#include "render/Gpu.h"

#include "hooks/D3D12Hooks.h"

#include <utility>

namespace cpmc::gpu
{
uint32_t PlaneSubresource(const D3D12_RESOURCE_DESC& aDesc, uint32_t aPlane)
{
	const uint32_t arraySize = aDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1u : aDesc.DepthOrArraySize;
	return aPlane * aDesc.MipLevels * arraySize;
}

uint32_t PlaneCount(ID3D12Device* aDevice, DXGI_FORMAT aFormat)
{
	D3D12_FEATURE_DATA_FORMAT_INFO info{ aFormat, 0 };
	if (FAILED(aDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &info, sizeof(info))) || info.PlaneCount == 0)
	{
		return 1;
	}
	return info.PlaneCount;
}

Footprint FootprintOf(ID3D12Device* aDevice, const D3D12_RESOURCE_DESC& aDesc, uint32_t aSubresource)
{
	Footprint footprint;
	UINT      rows = 0;
	UINT64    rowBytes = 0;
	UINT64    total = 0;
	aDevice->GetCopyableFootprints(&aDesc, aSubresource, 1, 0, &footprint.layout, &rows, &rowBytes, &total);
	footprint.rows = rows;
	footprint.rowBytes = rowBytes;
	footprint.totalBytes = total;
	return footprint;
}

ComPtr<ID3D12Resource> MakeBuffer(ID3D12Device* aDevice, uint64_t aBytes, D3D12_HEAP_TYPE aHeap, D3D12_RESOURCE_STATES aState)
{
	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = aHeap;
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = aBytes;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ComPtr<ID3D12Resource> buffer;
	if (FAILED(aDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, aState, nullptr, IID_PPV_ARGS(&buffer))))
	{
		return nullptr;
	}
	return buffer;
}

bool RecordCopy(ID3D12GraphicsCommandList* aList, ID3D12Resource* aSource, uint32_t aSubresource, uint32_t aState,
                ID3D12Resource* aDest, const Footprint& aFootprint)
{
	constexpr uint32_t kDirectOnly = D3D12_RESOURCE_STATE_INDEX_BUFFER | D3D12_RESOURCE_STATE_RENDER_TARGET | D3D12_RESOURCE_STATE_DEPTH_WRITE |
	                                 D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
	                                 D3D12_RESOURCE_STATE_STREAM_OUT | D3D12_RESOURCE_STATE_RESOLVE_DEST | D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
	const bool transition = (aState & D3D12_RESOURCE_STATE_COPY_SOURCE) == 0;
	if (transition && (aState & kDirectOnly) != 0 && aList->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		return false;
	}
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = aSource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATES(aState);
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	if (transition)
	{
		d3d::Original().resourceBarrier(aList, 1, &barrier);
	}
	D3D12_TEXTURE_COPY_LOCATION to{};
	to.pResource = aDest;
	to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	to.PlacedFootprint = aFootprint.layout;
	D3D12_TEXTURE_COPY_LOCATION from{};
	from.pResource = aSource;
	from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	from.SubresourceIndex = aSubresource;
	aList->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
	if (transition)
	{
		std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
		d3d::Original().resourceBarrier(aList, 1, &barrier);
	}
	return true;
}
}
```

- [x] **Step 5: Edit `CMakeLists.txt` to add the sources**

```cmake
	src/hooks/StreamlineHooks.cpp
)
```
```cmake
	src/hooks/StreamlineHooks.cpp
	src/track/GpuApi.cpp
	src/render/Gpu.cpp
)
```

- [x] **Step 6: Build; it must compile without warnings, and the tests still pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `83 test(s), 0 failure(s)`.

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: GpuApi reads, and GPU helpers" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 25: The frame log and the dumps

- **`FrameLog`** (`log`) records two whole frames, from the frame token after the request:
  - what Streamline is told;
  - every submission and present, each with its thread;
  - every tracked list's passes, with the game's names for the lists.

  It then waits one more frame for lists still closing. The main thread writes the result as `frame.json`.
- **`Dumps`** (`dump <what>`) copies buffers into readback memory, inside the game's own lists, at the moment each is
  complete:
  - the tagged depth, motion vectors and colour at the DLSS evaluate;
  - the G-buffer candidates as their pass ends.

  Once a fence shows the GPU has run the copies, the main thread writes them as `.dds` files, with `dump.json`
  saying what each one is.

**Files:**
- Create: `src/log/FrameLog.h`, `src/log/FrameLog.cpp`, `src/log/Dumps.h`, `src/log/Dumps.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write `src/log/FrameLog.h`**

```cpp
#pragma once

#include "LogQueue.h"
#include "hooks/HookEvents.h"
#include "log/FrameLogData.h"
#include "track/GpuApi.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace cpmc
{
// The frame log (the spec's step 1). `log` arms it. From the next frame token it records two whole frames: what
// Streamline is told, every submission and present, and every tracked list's passes. It then waits one more frame
// for lists still closing, and hands the result to the main thread.
class FrameLog : public HookEvents
{
public:
	explicit FrameLog(LogQueue& aLog);

	// Main thread.
	bool                        Arm();  // false if a log is already running
	bool                        Capturing() const;
	std::string                 State() const;  // for the status file
	void                        NoteScriptCamera(const LogScriptCamera& aCamera);
	std::optional<FrameLogData> TakeFinished(const std::string& aVersion, const std::string& aTime);

	void OnFrameToken(uint32_t aFrame) override;
	void OnConstants(uint32_t aFrame, uint32_t aViewport, const SlConstants& aConstants) override;
	void OnTags(uint32_t aFrame, uint32_t aViewport, const std::vector<SlTag>& aTags, void* aList) override;
	void OnEvaluate(const EvaluateCall& aCall) override;
	void OnClose(ID3D12GraphicsCommandList* aList, ListState& aState) override;
	void OnSubmit(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists) override;
	void OnPresent(IDXGISwapChain* aSwapChain) override;

private:
	enum class Phase
	{
		Idle,
		Armed,      // waiting for the next frame token
		Capturing,  // two frames
		Draining,   // one more frame: lists still closing are kept
		Done,
	};

	bool   Recording() const;  // mutex held
	double Ms() const;
	void   Event(std::string aWhat, std::string aDetail);  // mutex held

	LogQueue&                          log_;
	mutable std::mutex                 mutex_;
	Phase                              phase_ = Phase::Idle;
	uint32_t                           tokens_ = 0;
	int64_t                            start_ = 0;
	FrameLogData                       data_;
	std::unique_ptr<gpuapi::ListNames> names_;
};

// What a resource is, from ID3D12Resource::GetDesc. Unknown for null.
ResourceInfo DescribeResource(void* aResource);
}
```

- [x] **Step 2: Write `src/log/FrameLog.cpp`**

```cpp
#include "log/FrameLog.h"

#include "hooks/D3D12Hooks.h"
#include "hooks/StreamlineHooks.h"

#include <Windows.h>

#include <format>

namespace cpmc
{
namespace
{
int64_t Now()
{
	LARGE_INTEGER now{};
	QueryPerformanceCounter(&now);
	return now.QuadPart;
}

std::string Hex(const void* aPointer)
{
	return std::format("0x{:x}", reinterpret_cast<uintptr_t>(aPointer));
}

const char* TypeName(D3D12_COMMAND_LIST_TYPE aType)
{
	switch (aType)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT: return "direct";
	case D3D12_COMMAND_LIST_TYPE_COMPUTE: return "compute";
	case D3D12_COMMAND_LIST_TYPE_COPY: return "copy";
	default: return "other";
	}
}

std::string TagNames(const std::vector<SlTag>& aTags)
{
	std::string names;
	for (const SlTag& tag : aTags)
	{
		names += (names.empty() ? "" : ", ") + BufferTypeName(tag.type) + (tag.local ? " (local)" : "");
	}
	return names;
}
}

ResourceInfo DescribeResource(void* aResource)
{
	ResourceInfo info;
	if (!aResource)
	{
		return info;
	}
	const D3D12_RESOURCE_DESC desc = static_cast<ID3D12Resource*>(aResource)->GetDesc();
	info.known = true;
	info.dimension = desc.Dimension;
	info.format = desc.Format;
	info.width = desc.Width;
	info.height = desc.Height;
	info.depthOrArraySize = desc.DepthOrArraySize;
	info.mips = desc.MipLevels;
	info.flags = desc.Flags;
	return info;
}

FrameLog::FrameLog(LogQueue& aLog)
	: log_(aLog)
{
}

bool FrameLog::Arm()
{
	std::lock_guard lock(mutex_);
	if (phase_ != Phase::Idle)
	{
		return false;
	}
	data_ = {};
	tokens_ = 0;
	names_ = std::make_unique<gpuapi::ListNames>();
	phase_ = Phase::Armed;
	d3d::EnableTracking();
	return true;
}

bool FrameLog::Capturing() const
{
	std::lock_guard lock(mutex_);
	return phase_ == Phase::Capturing || phase_ == Phase::Draining;
}

std::string FrameLog::State() const
{
	std::lock_guard lock(mutex_);
	switch (phase_)
	{
	case Phase::Idle: return "idle";
	case Phase::Armed: return "armed, waiting for Streamline's next frame";
	case Phase::Capturing: return "capturing";
	case Phase::Draining: return "finishing";
	case Phase::Done: return "done, being written";
	}
	return "";
}

void FrameLog::NoteScriptCamera(const LogScriptCamera& aCamera)
{
	std::lock_guard lock(mutex_);
	if (phase_ == Phase::Capturing)
	{
		data_.scriptCamera = aCamera;
	}
}

bool FrameLog::Recording() const
{
	return phase_ == Phase::Capturing || phase_ == Phase::Draining;
}

double FrameLog::Ms() const
{
	LARGE_INTEGER frequency{};
	QueryPerformanceFrequency(&frequency);
	return double(Now() - start_) * 1000.0 / double(frequency.QuadPart);
}

void FrameLog::Event(std::string aWhat, std::string aDetail)
{
	data_.events.push_back({ Ms(), d3d::CurrentFrame(), GetCurrentThreadId(), std::move(aWhat), std::move(aDetail) });
}

void FrameLog::OnFrameToken(uint32_t aFrame)
{
	std::lock_guard lock(mutex_);
	switch (phase_)
	{
	case Phase::Armed:
		phase_ = Phase::Capturing;
		start_ = Now();
		data_.firstFrame = aFrame;
		d3d::SetLogging(true);
		Event("frameToken", std::format("frame {}: the log begins", aFrame));
		break;
	case Phase::Capturing:
		Event("frameToken", std::format("frame {}", aFrame));
		if (++tokens_ == 2)
		{
			phase_ = Phase::Draining;
			data_.lastFrame = aFrame - 1;
			d3d::SetLogging(false);
		}
		break;
	case Phase::Draining:
		Event("frameToken", std::format("frame {}: the log ends", aFrame));
		phase_ = Phase::Done;
		break;
	default: break;
	}
}

void FrameLog::OnConstants(uint32_t aFrame, uint32_t aViewport, const SlConstants& aConstants)
{
	std::lock_guard lock(mutex_);
	if (Recording())
	{
		data_.constants.push_back({ aFrame, aViewport, aConstants });
		Event("setConstants", std::format("frame {}, viewport {}", aFrame, aViewport));
	}
}

void FrameLog::OnTags(uint32_t aFrame, uint32_t aViewport, const std::vector<SlTag>& aTags, void* aList)
{
	std::lock_guard lock(mutex_);
	if (!Recording())
	{
		return;
	}
	for (const SlTag& tag : aTags)
	{
		data_.tags.push_back({ aFrame, aViewport, tag, DescribeResource(tag.resource), reinterpret_cast<uintptr_t>(aList) });
	}
	Event("setTag", std::format("viewport {}: {}", aViewport, TagNames(aTags)));
}

void FrameLog::OnEvaluate(const EvaluateCall& aCall)
{
	std::lock_guard lock(mutex_);
	if (!Recording())
	{
		return;
	}
	data_.evaluates.push_back({ aCall.frame, sl::kFeatureDLSS, aCall.viewport, reinterpret_cast<uintptr_t>(aCall.gameList),
	                            reinterpret_cast<uintptr_t>(aCall.list) });
	for (const SlTag& tag : aCall.tags)
	{
		if (tag.local)
		{
			data_.tags.push_back({ aCall.frame, aCall.viewport, tag, DescribeResource(tag.resource), reinterpret_cast<uintptr_t>(aCall.list) });
		}
	}
	Event("evaluate", std::format("DLSS, frame {}, viewport {}, list {} (native {}, {}), constants {}", aCall.frame, aCall.viewport,
	                              Hex(aCall.gameList), Hex(aCall.list), aCall.list ? TypeName(aCall.list->GetType()) : "unknown",
	                              aCall.constants ? "found" : "missing"));
}

void FrameLog::OnClose(ID3D12GraphicsCommandList* aList, ListState& aState)
{
	if (!aState.Logging())
	{
		return;
	}
	std::lock_guard lock(mutex_);
	if (Recording())
	{
		data_.lists.push_back({ reinterpret_cast<uintptr_t>(aList), names_ ? names_->Find(aList) : std::string(), aState.Frame(),
		                        aState.Passes() });
	}
}

void FrameLog::OnSubmit(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists)
{
	std::lock_guard lock(mutex_);
	if (!Recording())
	{
		return;
	}
	LogSubmission submission;
	submission.ms = Ms();
	submission.frame = d3d::CurrentFrame();
	submission.queue = reinterpret_cast<uintptr_t>(aQueue);
	for (UINT i = 0; i < aCount; ++i)
	{
		submission.lists.push_back(reinterpret_cast<uintptr_t>(aLists[i]));
		if (!d3d::Tracker().Find(aLists[i]))
		{
			++submission.untracked;
		}
	}
	Event("submit", std::format("queue {} ({}{}), {} lists", Hex(aQueue), TypeName(aQueue->GetDesc().Type),
	                            aQueue == d3d::MainQueue() ? ", the main queue" : "", aCount));
	data_.submissions.push_back(std::move(submission));
}

void FrameLog::OnPresent(IDXGISwapChain* aSwapChain)
{
	std::lock_guard lock(mutex_);
	if (Recording())
	{
		const ID3D12CommandQueue* queue = d3d::PresentQueue(aSwapChain);
		Event("present", std::format("swap chain {}, on queue {}{}", Hex(aSwapChain), Hex(queue),
		                             queue && queue == d3d::MainQueue() ? " (the main queue)" : ""));
	}
}

std::optional<FrameLogData> FrameLog::TakeFinished(const std::string& aVersion, const std::string& aTime)
{
	std::lock_guard lock(mutex_);
	if (phase_ != Phase::Done)
	{
		return std::nullopt;
	}
	FrameLogData data = std::move(data_);
	data_ = {};
	names_.reset();
	phase_ = Phase::Idle;

	data.pluginVersion = aVersion;
	data.time = aTime;
	LogDevice& device = data.device;
	device.seenAtCreation = d3d::SeenAtCreation();
	device.device = reinterpret_cast<uintptr_t>(d3d::Device());
	const gpuapi::Report report = gpuapi::Check(d3d::Device());
	device.gpuApiDevice = report.device;
	device.gpuApiMatches = report.deviceMatches;
	device.gpuApiQueue = report.queue;
	device.gpuApiSwapChain = report.swapChain;
	device.streamline = data.evaluates.empty() ? "unknown" : data.evaluates[0].list == data.evaluates[0].nativeList ? "manual" : "proxies";
	device.rtvs = d3d::Views().Count(ViewInfo::Kind::Rtv);
	device.dsvs = d3d::Views().Count(ViewInfo::Kind::Dsv);
	device.vtableEntries = d3d::PatchedEntries();
	device.imports = 4;

	// The G-buffer candidates, against the log's latest depth tag.
	const void* depth = nullptr;
	for (const LogTag& tag : data.tags)
	{
		if (tag.tag.type == sl::kBufferTypeDepth && tag.tag.resource)
		{
			depth = tag.tag.resource;
		}
	}
	for (const LogList& list : data.lists)
	{
		for (size_t pass = 0; pass < list.passes.size(); ++pass)
		{
			const Targets& targets = list.passes[pass].targets;
			for (const BoundTarget& target : targets.rtvs)
			{
				device.unknownHandles += target.view ? 0 : 1;
			}
			device.unknownHandles += targets.dsv && !targets.dsv->view ? 1 : 0;
			if (IsGbufferCandidate(targets, depth))
			{
				data.gbufferCandidates.push_back({ list.list, pass });
			}
		}
	}
	return data;
}
}
```

- [x] **Step 3: Write `src/log/Dumps.h`**

```cpp
#pragma once

#include "LogQueue.h"
#include "hooks/HookEvents.h"
#include "render/Gpu.h"

#include <chrono>
#include <filesystem>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace cpmc
{
// Dumps (the spec's "FrameLog and dumps"). `dump <what>` arms one frame: from the next frame token, the tagged
// buffers are copied at the DLSS evaluate (depth, mvec, color) and the G-buffer candidates when their pass ends
// (gbuffer, at most 4 passes). The copies go into readback buffers inside the game's own lists; once a fence says the
// GPU has run them, the main thread writes each as a .dds file, with dump.json saying what each one is.
class Dumps : public HookEvents
{
public:
	Dumps(LogQueue& aLog, std::filesystem::path aFolder);

	// Main thread.
	bool        Arm(std::string_view aWhat, std::string& aError);
	std::string State() const;
	void        Update(const std::string& aFolderName);  // writes the copies the GPU has finished

	void OnFrameToken(uint32_t aFrame) override;
	void OnTags(uint32_t aFrame, uint32_t aViewport, const std::vector<SlTag>& aTags, void* aList) override;
	void OnEvaluate(const EvaluateCall& aCall) override;
	void OnPassEnd(ID3D12GraphicsCommandList* aList, ListState& aState, bool aClosing) override;
	void OnSubmit(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists) override;
	void OnSubmitted(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists) override;

private:
	enum class Phase
	{
		Idle,
		Armed,     // waiting for the next frame token
		Copying,   // one frame
		Waiting,   // for the GPU
	};

	struct Copy
	{
		std::string                  name;
		std::string                  note;  // what it is, for dump.json
		gpu::ComPtr<ID3D12Resource>  readback;
		gpu::Footprint               footprint;
		uint32_t                     format = 0;  // the format it's saved as
		uint32_t                     width = 0;
		uint32_t                     height = 0;
		ID3D12GraphicsCommandList*   list = nullptr;  // the game list it was recorded into
		ID3D12CommandQueue*          queue = nullptr;
		uint64_t                     fence = 0;  // 0 until the list is submitted and signalled
	};

	// Records copies of every plane of aResource (or of plane aOnlyPlane) into aList. Mutex held.
	void CopyResource(ID3D12GraphicsCommandList* aList, ID3D12Resource* aResource, uint32_t aState, const std::string& aName,
	                  const std::string& aNote, int aOnlyPlane = -1);

	LogQueue&                         log_;
	std::filesystem::path             folder_;
	mutable std::mutex                mutex_;
	Phase                             phase_ = Phase::Idle;
	std::set<std::string>             what_;
	uint32_t                          frame_ = 0;
	void*                             taggedDepth_ = nullptr;
	int                               candidates_ = 0;
	std::vector<Copy>                 copies_;
	std::set<ID3D12CommandList*>      lists_;  // game lists holding copies, not yet submitted
	gpu::ComPtr<ID3D12Fence>          fence_;
	uint64_t                          fenceValue_ = 0;
	std::vector<std::string>          errors_;
	std::chrono::steady_clock::time_point waitingSince_{};
};
}
```

- [x] **Step 4: Write `src/log/Dumps.cpp`**

```cpp
#include "log/Dumps.h"

#include "Json.h"
#include "hooks/D3D12Hooks.h"
#include "log/DdsWriter.h"
#include "log/FrameLogData.h"

#include <format>
#include <fstream>

namespace cpmc
{
namespace
{
constexpr int kMaxCandidates = 4;

bool WriteFile(const std::filesystem::path& aPath, const void* aData, size_t aSize)
{
	std::ofstream out(aPath, std::ios::binary);
	out.write(static_cast<const char*>(aData), std::streamsize(aSize));
	return bool(out);
}
}

Dumps::Dumps(LogQueue& aLog, std::filesystem::path aFolder)
	: log_(aLog)
	, folder_(std::move(aFolder))
{
}

bool Dumps::Arm(std::string_view aWhat, std::string& aError)
{
	std::lock_guard lock(mutex_);
	if (phase_ != Phase::Idle)
	{
		aError = "a dump is already under way";
		return false;
	}
	what_.clear();
	if (aWhat == "all")
	{
		what_ = { "depth", "mvec", "color", "gbuffer" };
	}
	else
	{
		what_.insert(std::string(aWhat));
	}
	copies_.clear();
	lists_.clear();
	errors_.clear();
	candidates_ = 0;
	phase_ = Phase::Armed;
	d3d::EnableTracking();
	return true;
}

std::string Dumps::State() const
{
	std::lock_guard lock(mutex_);
	switch (phase_)
	{
	case Phase::Idle: return "idle";
	case Phase::Armed: return "armed";
	case Phase::Copying: return "copying";
	case Phase::Waiting: return std::format("waiting for the GPU ({} copies)", copies_.size());
	}
	return "";
}

void Dumps::OnFrameToken(uint32_t aFrame)
{
	std::lock_guard lock(mutex_);
	if (phase_ == Phase::Armed)
	{
		phase_ = Phase::Copying;
		frame_ = aFrame;
	}
	else if (phase_ == Phase::Copying)
	{
		phase_ = Phase::Waiting;
		waitingSince_ = std::chrono::steady_clock::now();
	}
}

void Dumps::OnTags(uint32_t, uint32_t, const std::vector<SlTag>& aTags, void*)
{
	std::lock_guard lock(mutex_);
	for (const SlTag& tag : aTags)
	{
		if (tag.type == sl::kBufferTypeDepth && tag.resource)
		{
			taggedDepth_ = tag.resource;
		}
	}
}

void Dumps::CopyResource(ID3D12GraphicsCommandList* aList, ID3D12Resource* aResource, uint32_t aState, const std::string& aName,
                         const std::string& aNote, int aOnlyPlane)
{
	gpu::ComPtr<ID3D12Device> owner;
	if (!aResource || FAILED(aResource->GetDevice(IID_PPV_ARGS(&owner))))
	{
		return;
	}
	ID3D12Device* const       device = owner.Get();
	const D3D12_RESOURCE_DESC desc = aResource->GetDesc();
	if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
	{
		errors_.push_back(aName + ": not a 2D texture");
		return;
	}
	const uint32_t planes = gpu::PlaneCount(device, desc.Format);
	for (uint32_t plane = 0; plane < planes; ++plane)
	{
		if (aOnlyPlane >= 0 && plane != uint32_t(aOnlyPlane))
		{
			continue;
		}
		const uint32_t format = DumpFormat(desc.Format, plane);
		if (BytesPerTexel(format) == 0)
		{
			errors_.push_back(std::format("{}: format {} can't be saved", aName, DxgiFormatName(desc.Format)));
			continue;
		}
		Copy copy;
		copy.name = planes > 1 ? std::format("{}-plane{}", aName, plane) : aName;
		copy.note = aNote;
		copy.footprint = gpu::FootprintOf(device, desc, gpu::PlaneSubresource(desc, plane));
		copy.readback = gpu::MakeBuffer(device, copy.footprint.totalBytes, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
		if (!copy.readback)
		{
			errors_.push_back(aName + ": couldn't create a readback buffer");
			continue;
		}
		copy.format = format;
		copy.width = copy.footprint.layout.Footprint.Width;
		copy.height = copy.footprint.layout.Footprint.Height;
		copy.list = aList;
		if (!gpu::RecordCopy(aList, aResource, gpu::PlaneSubresource(desc, plane), aState, copy.readback.Get(), copy.footprint))
		{
			errors_.push_back(std::format("{}: its list isn't a direct list, which can't move it out of state 0x{:x}", copy.name, aState));
			continue;
		}
		copies_.push_back(std::move(copy));
	}
	lists_.insert(aList);
}

void Dumps::OnEvaluate(const EvaluateCall& aCall)
{
	std::lock_guard lock(mutex_);
	if (phase_ != Phase::Copying || !aCall.list)
	{
		return;
	}
	const std::pair<uint32_t, const char*> wanted[] = { { sl::kBufferTypeDepth, "depth" },
		                                                { sl::kBufferTypeMotionVectors, "mvec" },
		                                                { sl::kBufferTypeScalingInputColor, "color" } };
	for (const auto& [type, what] : wanted)
	{
		if (!what_.contains(what))
		{
			continue;
		}
		for (const SlTag& tag : aCall.tags)
		{
			if (tag.type != type || !tag.resource)
			{
				continue;
			}
			if (tag.state == 0xFFFFFFFF)
			{
				errors_.push_back(BufferTypeName(type) + ": the tag gives no state");
				continue;
			}
			const std::string note = std::format("tag {} at the DLSS evaluate, frame {}, state 0x{:x}, extent {} {} {} {}",
			                                     BufferTypeName(type), aCall.frame, tag.state, tag.extent.top, tag.extent.left,
			                                     tag.extent.width, tag.extent.height);
			CopyResource(aCall.list, static_cast<ID3D12Resource*>(tag.resource), tag.state, "tag-" + BufferTypeName(type), note);
		}
	}
}

void Dumps::OnPassEnd(ID3D12GraphicsCommandList* aList, ListState& aState, bool)
{
	std::lock_guard lock(mutex_);
	if (phase_ != Phase::Copying || !what_.contains("gbuffer") || candidates_ >= kMaxCandidates ||
	    !IsGbufferCandidate(aState.CurrentTargets(), taggedDepth_))
	{
		return;
	}
	const int      index = candidates_++;
	const Targets& targets = aState.CurrentTargets();
	for (size_t i = 0; i < targets.rtvs.size(); ++i)
	{
		const ViewInfo& view = *targets.rtvs[i].view;
		auto*           resource = static_cast<ID3D12Resource*>(view.resource);
		const uint32_t  state = aState.StateOf(resource).value_or(D3D12_RESOURCE_STATE_RENDER_TARGET);
		const std::string note = std::format("G-buffer candidate {}, target {} ({}), list 0x{:x}, frame {}, state 0x{:x}", index, i,
		                                     DxgiFormatName(view.viewFormat), reinterpret_cast<uintptr_t>(aList), frame_, state);
		CopyResource(aList, resource, state, std::format("gbuffer{}-target{}", index, i), note, int(view.plane));
	}
	const ViewInfo& depth = *targets.dsv->view;
	const auto      known = aState.StateOf(depth.resource);
	if (!known && depth.readOnlyDepth)
	{
		errors_.push_back(std::format("gbuffer{}-depth: read-only and its state is unknown, so it isn't copied", index));
		return;
	}
	const uint32_t    state = known.value_or(D3D12_RESOURCE_STATE_DEPTH_WRITE);
	const std::string note = std::format("G-buffer candidate {}, depth ({}), list 0x{:x}, frame {}, state 0x{:x}", index,
	                                     DxgiFormatName(depth.viewFormat), reinterpret_cast<uintptr_t>(aList), frame_, state);
	CopyResource(aList, static_cast<ID3D12Resource*>(depth.resource), state, std::format("gbuffer{}-depth", index), note);
}

void Dumps::OnSubmit(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists)
{
	std::lock_guard lock(mutex_);
	if (lists_.empty())
	{
		return;
	}
	for (UINT i = 0; i < aCount; ++i)
	{
		if (lists_.erase(aLists[i]) == 0)
		{
			continue;
		}
		for (Copy& copy : copies_)
		{
			if (copy.list == aLists[i])
			{
				copy.queue = aQueue;
			}
		}
	}
}

void Dumps::OnSubmitted(ID3D12CommandQueue* aQueue, UINT, ID3D12CommandList* const*)
{
	std::lock_guard lock(mutex_);
	bool            signal = false;
	for (const Copy& copy : copies_)
	{
		signal = signal || (copy.queue == aQueue && copy.fence == 0);
	}
	if (!signal)
	{
		return;
	}
	gpu::ComPtr<ID3D12Device> device;
	if (!fence_ && (FAILED(aQueue->GetDevice(IID_PPV_ARGS(&device))) ||
	                FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)))))
	{
		errors_.push_back("couldn't create a fence");
		return;
	}
	aQueue->Signal(fence_.Get(), ++fenceValue_);
	for (Copy& copy : copies_)
	{
		if (copy.queue == aQueue && copy.fence == 0)
		{
			copy.fence = fenceValue_;
		}
	}
}

void Dumps::Update(const std::string& aFolderName)
{
	std::lock_guard lock(mutex_);
	if (phase_ != Phase::Waiting)
	{
		return;
	}
	// Wait until the GPU has run every copy, or 10 seconds: a list the game never submitted won't finish.
	const uint64_t completed = fence_ ? fence_->GetCompletedValue() : 0;
	const bool     timedOut = std::chrono::steady_clock::now() - waitingSince_ > std::chrono::seconds(10);
	for (const Copy& copy : copies_)
	{
		if ((copy.fence == 0 || completed < copy.fence) && !timedOut)
		{
			return;
		}
	}
	const std::filesystem::path folder = folder_ / "dumps" / aFolderName;
	std::error_code             error;
	std::filesystem::create_directories(folder, error);
	JsonWriter json;
	json.BeginObject().Key("frame").UInt(frame_).Key("files").BeginArray();
	size_t written = 0;
	for (Copy& copy : copies_)
	{
		if (copy.fence == 0 || completed < copy.fence)
		{
			errors_.push_back(copy.name + ": the GPU never ran its copy");
			continue;
		}
		void*             mapped = nullptr;
		const D3D12_RANGE range{ 0, size_t(copy.footprint.totalBytes) };
		if (FAILED(copy.readback->Map(0, &range, &mapped)))
		{
			errors_.push_back(copy.name + ": couldn't map its readback buffer");
			continue;
		}
		const auto* base = static_cast<const uint8_t*>(mapped) + copy.footprint.layout.Offset;
		const auto  dds = MakeDds(copy.format, copy.width, copy.height, base, copy.footprint.layout.Footprint.RowPitch);
		const D3D12_RANGE nothing{ 0, 0 };
		copy.readback->Unmap(0, &nothing);
		if (!WriteFile(folder / (copy.name + ".dds"), dds.data(), dds.size()))
		{
			errors_.push_back(copy.name + ": couldn't write the file");
			continue;
		}
		++written;
		json.BeginObject();
		json.Key("file").String(copy.name + ".dds");
		json.Key("format").String(DxgiFormatName(copy.format));
		json.Key("size").BeginArray().UInt(copy.width).UInt(copy.height).EndArray();
		json.Key("note").String(copy.note);
		json.EndObject();
	}
	json.EndArray().Key("errors").BeginArray();
	for (const std::string& line : errors_)
	{
		json.String(line);
	}
	json.EndArray().EndObject();
	const std::string text = json.Str() + "\n";
	WriteFile(folder / "dump.json", text.data(), text.size());
	log_.Push(std::format("dump: {} files written to {}{}", written, folder.string(),
	                      errors_.empty() ? "" : std::format(", {} problems (see dump.json)", errors_.size())));
	copies_.clear();
	errors_.clear();
	phase_ = Phase::Idle;
}
}
```

- [x] **Step 5: Edit `CMakeLists.txt` to add the sources**

```cmake
	src/render/Gpu.cpp
)
```
```cmake
	src/render/Gpu.cpp
	src/log/FrameLog.cpp
	src/log/Dumps.cpp
)
```

- [x] **Step 6: Build; it must compile without warnings, and the tests still pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `83 test(s), 0 failure(s)`.

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: frame log and dumps" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" \
  && git log --oneline -1
```

---

### Task 26: The renderer

`Renderer` is step 2's drawing, mode `present`:
- **At each DLSS evaluate,** the tagged depth's depth plane is copied into a buffer of ours, inside the game's list.
- **Once that list is submitted,** the next `Present` draws the blocks with our own list:
  - on the same queue, so the copy is done before our draw runs;
  - into the back buffer, before the original `Present` runs;
  - hidden wherever the copied depth says the game drew something nearer.

Uploads go on the main queue from the main thread, and a buffer is freed only once the fence shows the GPU has
finished with it.

The renderer never waits on the GPU. It skips a frame, and counts it, when anything it needs isn't there:
- after 30 skips in a row, or if one of our D3D12 calls fails, it switches drawing off and logs why;
- it creates nothing until drawing is first switched on.

**Files:**
- Create: `src/render/Renderer.h`, `src/render/Renderer.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write `src/render/Renderer.h`**

```cpp
#pragma once

#include "Coords.h"
#include "LogQueue.h"
#include "hooks/HookEvents.h"
#include "render/Atlas.h"
#include "render/Camera.h"
#include "render/Gpu.h"
#include "render/Shaders.h"
#include "world/BlockWorld.h"
#include "world/Vertex.h"

#include <array>
#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cpmc
{
// The plugin's own drawing (the spec's "Renderer"). This build has the modes off and present (step 2): at each DLSS
// evaluate the tagged depth's depth plane is copied into a buffer of ours, inside the game's list. Once that list is
// submitted, the next Present draws the blocks into the back buffer with our own list, on the same queue, hidden
// where the copied depth says the game drew something nearer.
class Renderer : public HookEvents
{
public:
	Renderer(LogQueue& aLog, std::filesystem::path aFolder);

	// Main thread.
	bool         SetMode(std::string_view aMode, std::string& aError);
	std::string  ModeName() const;
	bool         LoadAtlas(std::string& aError);  // atlas.bin, from the plugin's folder
	const Atlas* AtlasData() const;               // null until loaded; it stays valid, since the atlas is loaded once
	bool         ReloadShaders(std::string& aError);
	bool         Set(std::string_view aName, double aValue, std::string& aError);  // brightness, tolerance, cull, origin.x/y/z
	void         UploadSection(const SectionPos& aSection, std::vector<Vertex> aVertices);  // no vertices: remove it
	void         Update();  // uploads, and frees what the GPU has finished with

	struct Stats
	{
		uint64_t    drawn = 0;
		uint64_t    skipped = 0;
		uint64_t    lateCamera = 0;  // drawn with the same DLSS frame as the draw before
		std::string lastSkip;
		double      gpuMsAverage = -1.0;
		double      gpuMsWorst = -1.0;
		double      frameMsAverage = -1.0;
		double      frameMsWorst = -1.0;
		size_t      sections = 0;
		size_t      vertices = 0;
	};
	Stats TakeStats();  // since the last call
	void  Shutdown();   // waits for the GPU, then releases everything

	void OnEvaluate(const EvaluateCall& aCall) override;
	void OnSubmit(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists) override;
	void OnPresent(IDXGISwapChain* aSwapChain) override;

private:
	enum class Mode
	{
		Off,
		Present,
	};

	static constexpr uint32_t kSlots = 8;  // frames in flight: more than Streamline's 6

	struct SectionGpu
	{
		gpu::ComPtr<ID3D12Resource> buffer;
		uint32_t                    vertices = 0;
		uint64_t                    ready = 0;  // the fence value its upload signals
	};

	struct Frame  // one DLSS evaluate's
	{
		const void* list = nullptr;  // the game's list holding the depth copy; compared, never used
		uint32_t    frame = 0;
		bool        valid = false;  // its constants were found
		SlConstants constants;
		bool        depth = false;  // the depth copy holds its depth
		uint32_t    rowFloats = 0;
		sl::Extent  extent{};
	};

	// All with the mutex held.
	bool     EnsureObjects();  // false until the main queue is known, or if creating them failed
	bool     EnsurePipeline(DXGI_FORMAT aFormat, bool aReversed);
	bool     EnsureDepthTarget(uint32_t aWidth, uint32_t aHeight);
	void     Skip(const std::string& aWhy);
	void     Fail(const std::string& aWhy);  // logs, and switches drawing off
	uint64_t Signal();
	void     Retire(gpu::ComPtr<ID3D12Resource> aResource);
	void     ReadTimings(uint32_t aSlot);

	LogQueue&             log_;
	std::filesystem::path folder_;
	mutable std::mutex    mutex_;
	std::atomic<Mode>     mode_{ Mode::Off };

	float brightness_ = 1.0f;
	float tolerance_ = 0.0001f;
	int   cull_ = 1;  // 0 none, 1 back faces, 2 front faces
	Vec3d renderOrigin_;

	ID3D12Device*                                              device_ = nullptr;  // the game's
	ID3D12CommandQueue*                                        queue_ = nullptr;   // the game's main queue
	gpu::ComPtr<ID3D12RootSignature>                           rootSignature_;
	std::optional<BlockShaders>                                shaders_;
	gpu::ComPtr<ID3D12PipelineState>                           pipeline_;
	DXGI_FORMAT                                                pipelineFormat_ = DXGI_FORMAT_UNKNOWN;
	int                                                        pipelineCull_ = -1;
	bool                                                       pipelineReversed_ = false;
	gpu::ComPtr<ID3D12DescriptorHeap>                          rtvHeap_;
	gpu::ComPtr<ID3D12DescriptorHeap>                          dsvHeap_;
	gpu::ComPtr<ID3D12Resource>                                depthTarget_;
	uint32_t                                                   depthWidth_ = 0;
	uint32_t                                                   depthHeight_ = 0;
	gpu::ComPtr<ID3D12Resource>                                constants_;  // upload: kSlots x 256 bytes
	uint8_t*                                                   constantsMapped_ = nullptr;
	std::array<gpu::ComPtr<ID3D12CommandAllocator>, kSlots>    allocators_;
	std::array<gpu::ComPtr<ID3D12GraphicsCommandList>, kSlots> lists_;
	std::array<uint64_t, kSlots>                               slotFence_{};
	uint64_t                                                   presents_ = 0;
	gpu::ComPtr<ID3D12CommandAllocator>                        uploadAllocator_;
	gpu::ComPtr<ID3D12GraphicsCommandList>                     uploadList_;
	uint64_t                                                   uploadFence_ = 0;
	gpu::ComPtr<ID3D12Fence>                                   fence_;
	uint64_t                                                   fenceValue_ = 0;
	gpu::ComPtr<ID3D12QueryHeap>                               queries_;
	gpu::ComPtr<ID3D12Resource>                                timings_;  // readback: kSlots x 2 timestamps
	uint64_t*                                                  timingsMapped_ = nullptr;
	uint64_t                                                   timestampFrequency_ = 0;

	std::optional<Atlas>                                           atlas_;
	gpu::ComPtr<ID3D12Resource>                                    atlasBuffer_;
	uint64_t                                                       atlasReady_ = 0;
	bool                                                           atlasUploaded_ = false;
	std::map<SectionPos, SectionGpu>                               sections_;
	std::vector<std::pair<SectionPos, std::vector<Vertex>>>        pending_;
	std::vector<std::pair<gpu::ComPtr<ID3D12Resource>, uint64_t>> retired_;

	gpu::ComPtr<ID3D12Resource>              depthCopy_;
	uint64_t                                 depthCopyBytes_ = 0;
	std::vector<gpu::ComPtr<ID3D12Resource>> oldDepthCopies_;  // a game list not yet submitted may still use them
	Frame                                    recorded_;        // the latest evaluate, its list not yet submitted
	std::atomic<const void*>                 recordedList_{ nullptr };
	Frame                                    submitted_;       // the latest evaluate whose list was submitted
	std::optional<uint32_t>                  lastDrawnFrame_;
	const void*                              checkedSwapChain_ = nullptr;  // compared, never used
	bool                                     swapChainOnMainQueue_ = false;
	uint32_t                                 consecutiveSkips_ = 0;

	Stats    stats_;
	double   gpuMsSum_ = 0.0;
	uint64_t gpuSamples_ = 0;
	double   frameMsSum_ = 0.0;
	uint64_t frameSamples_ = 0;
	int64_t  lastPresent_ = 0;
};
}
```

- [x] **Step 2: Write `src/render/Renderer.cpp`**

```cpp
#include "render/Renderer.h"

#include "hooks/D3D12Hooks.h"
#include "log/DdsWriter.h"
#include "log/FrameLogData.h"
#include "render/FrameConstants.h"

#include <dxgi1_4.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>

namespace cpmc
{
namespace
{
constexpr uint64_t kConstantsStride = 256;  // a constant buffer view's alignment
constexpr uint32_t kSkipsBeforeOff = 30;     // the spec's "Checks before every draw"

int64_t Now()
{
	LARGE_INTEGER now{};
	QueryPerformanceCounter(&now);
	return now.QuadPart;
}

double MsBetween(int64_t aFrom, int64_t aTo)
{
	LARGE_INTEGER frequency{};
	QueryPerformanceFrequency(&frequency);
	return double(aTo - aFrom) * 1000.0 / double(frequency.QuadPart);
}

std::optional<std::vector<uint8_t>> ReadWholeFile(const std::filesystem::path& aPath)
{
	std::ifstream in(aPath, std::ios::binary);
	if (!in)
	{
		return std::nullopt;
	}
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// False when all eight corners of the box lie beyond the same side of the view, or behind the camera.
bool Visible(const Mat4& aCameraToClip, const Vec3d& aLow, const Vec3d& aHigh)
{
	int outside[5] = {};
	for (int corner = 0; corner < 8; ++corner)
	{
		const std::array<double, 4> point{ corner & 1 ? aHigh.x : aLow.x, corner & 2 ? aHigh.y : aLow.y,
			                               corner & 4 ? aHigh.z : aLow.z, 1.0 };
		const std::array<double, 4> clip = Transform(point, aCameraToClip);
		outside[0] += clip[0] > clip[3] ? 1 : 0;
		outside[1] += clip[0] < -clip[3] ? 1 : 0;
		outside[2] += clip[1] > clip[3] ? 1 : 0;
		outside[3] += clip[1] < -clip[3] ? 1 : 0;
		outside[4] += clip[3] < 0.01 ? 1 : 0;
	}
	return std::none_of(std::begin(outside), std::end(outside), [](int aCount) { return aCount == 8; });
}

D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* aResource, D3D12_RESOURCE_STATES aBefore, D3D12_RESOURCE_STATES aAfter)
{
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = aResource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = aBefore;
	barrier.Transition.StateAfter = aAfter;
	return barrier;
}
}

Renderer::Renderer(LogQueue& aLog, std::filesystem::path aFolder)
	: log_(aLog)
	, folder_(std::move(aFolder))
{
}

bool Renderer::SetMode(std::string_view aMode, std::string& aError)
{
	if (aMode == "present")
	{
		if (!AtlasData() && !LoadAtlas(aError))
		{
			return false;
		}
		bool haveShaders = false;
		{
			std::lock_guard lock(mutex_);
			haveShaders = shaders_.has_value();
		}
		if (!haveShaders && !ReloadShaders(aError))
		{
			return false;
		}
	}
	else if (aMode != "off")
	{
		aError = std::format("mode {} isn't in this build: it comes with steps 3 and 4", aMode);
		return false;
	}
	std::lock_guard lock(mutex_);
	recorded_ = {};
	recordedList_ = nullptr;
	submitted_ = {};
	lastDrawnFrame_.reset();
	consecutiveSkips_ = 0;
	mode_ = aMode == "present" ? Mode::Present : Mode::Off;
	return true;
}

std::string Renderer::ModeName() const
{
	return mode_.load() == Mode::Present ? "present" : "off";
}

bool Renderer::LoadAtlas(std::string& aError)
{
	const auto bytes = ReadWholeFile(folder_ / "atlas.bin");
	if (!bytes)
	{
		aError = "there's no atlas.bin in the plugin's folder: build it with tools/build_atlas.py";
		return false;
	}
	auto atlas = ParseAtlas(*bytes, aError);
	if (!atlas)
	{
		aError = "atlas.bin: " + aError;
		return false;
	}
	std::lock_guard lock(mutex_);
	atlas_ = std::move(*atlas);
	atlasUploaded_ = false;
	log_.Push(std::format("atlas: {} x {}, {} levels, {} blocks", atlas_->width, atlas_->height, atlas_->Levels(), atlas_->blocks.size()));
	return true;
}

const Atlas* Renderer::AtlasData() const
{
	std::lock_guard lock(mutex_);
	return atlas_ ? &*atlas_ : nullptr;
}

bool Renderer::ReloadShaders(std::string& aError)
{
	const auto bytes = ReadWholeFile(folder_ / "shaders" / "blocks.hlsl");
	if (!bytes)
	{
		aError = "there's no shaders\\blocks.hlsl in the plugin's folder";
		return false;
	}
	auto shaders = CompileBlockShaders(std::string(bytes->begin(), bytes->end()), aError);
	if (!shaders)
	{
		return false;
	}
	std::lock_guard lock(mutex_);
	shaders_ = std::move(*shaders);
	pipeline_.Reset();  // rebuilt with the new shaders at the next draw
	return true;
}

bool Renderer::Set(std::string_view aName, double aValue, std::string& aError)
{
	std::lock_guard lock(mutex_);
	if (aName == "brightness" || aName == "tolerance")
	{
		if (aValue < 0.0)
		{
			aError = std::format("{} can't be negative", aName);
			return false;
		}
		(aName == "brightness" ? brightness_ : tolerance_) = float(aValue);
		return true;
	}
	if (aName == "cull")
	{
		if (aValue != 0.0 && aValue != 1.0 && aValue != 2.0)
		{
			aError = "cull is 0 (none), 1 (back faces) or 2 (front faces)";
			return false;
		}
		cull_ = int(aValue);
		return true;
	}
	if (aName == "origin.x" || aName == "origin.y" || aName == "origin.z")
	{
		(aName == "origin.x" ? renderOrigin_.x : aName == "origin.y" ? renderOrigin_.y : renderOrigin_.z) = aValue;
		return true;
	}
	aError = std::format("there's no setting called \"{}\"", aName);
	return false;
}

void Renderer::UploadSection(const SectionPos& aSection, std::vector<Vertex> aVertices)
{
	std::lock_guard lock(mutex_);
	pending_.emplace_back(aSection, std::move(aVertices));
}

bool Renderer::EnsureObjects()
{
	if (fence_)
	{
		return true;
	}
	const d3d::Originals& original = d3d::Original();
	queue_ = d3d::MainQueue();
	device_ = queue_ ? d3d::Device() : nullptr;
	if (queue_ && queue_->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		Fail("DLSS runs on a compute queue, and this build draws only on a direct one");
		return false;
	}
	if (!device_ || !original.reset || !original.close || !original.executeCommandLists || !original.resourceBarrier ||
	    !original.omSetRenderTargets || !original.clearDsv || !original.drawInstanced || !original.createRtv || !original.createDsv)
	{
		return false;  // not ready yet
	}
	D3D12_ROOT_PARAMETER parameters[5]{};
	parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;  // b0: the frame
	parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	parameters[1].Constants = { 1, 0, 4 };  // b1: the section
	for (UINT i = 2; i < 5; ++i)            // t0 vertices, t1 atlas, t2 depth copy
	{
		parameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		parameters[i].Descriptor = { i - 2, 0 };
	}
	const D3D12_ROOT_SIGNATURE_DESC rootDesc{ 5, parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE };
	gpu::ComPtr<ID3DBlob>           blob;
	gpu::ComPtr<ID3DBlob>           error;
	if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error)) ||
	    FAILED(device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_))))
	{
		Fail("couldn't create the root signature");
		return false;
	}
	const D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{ D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
	const D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{ D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
	const D3D12_QUERY_HEAP_DESC      queryDesc{ D3D12_QUERY_HEAP_TYPE_TIMESTAMP, kSlots * 2, 0 };
	constants_ = gpu::MakeBuffer(device_, kSlots * kConstantsStride, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	timings_ = gpu::MakeBuffer(device_, kSlots * 2 * sizeof(uint64_t), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
	if (FAILED(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_))) ||
	    FAILED(device_->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap_))) ||
	    FAILED(device_->CreateQueryHeap(&queryDesc, IID_PPV_ARGS(&queries_))) || !constants_ || !timings_ ||
	    FAILED(constants_->Map(0, nullptr, reinterpret_cast<void**>(&constantsMapped_))) ||
	    FAILED(timings_->Map(0, nullptr, reinterpret_cast<void**>(&timingsMapped_))) ||
	    FAILED(queue_->GetTimestampFrequency(&timestampFrequency_)))
	{
		Fail("couldn't create its heaps and buffers");
		return false;
	}
	for (uint32_t slot = 0; slot < kSlots; ++slot)
	{
		if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocators_[slot]))) ||
		    FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocators_[slot].Get(), nullptr,
		                                      IID_PPV_ARGS(&lists_[slot]))))
		{
			Fail("couldn't create its command lists");
			return false;
		}
		original.close(lists_[slot].Get());
		lists_[slot]->SetName(L"BlockRender present");
	}
	gpu::ComPtr<ID3D12Fence> fence;
	if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAllocator_))) ||
	    FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, uploadAllocator_.Get(), nullptr, IID_PPV_ARGS(&uploadList_))) ||
	    FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
	{
		Fail("couldn't create its upload list and fence");
		return false;
	}
	original.close(uploadList_.Get());
	uploadList_->SetName(L"BlockRender uploads");
	rootSignature_->SetName(L"BlockRender root signature");
	constants_->SetName(L"BlockRender constants");
	timings_->SetName(L"BlockRender timings");
	fence->SetName(L"BlockRender fence");
	fence_ = fence;  // last: a fence means everything is ready
	log_.Push("renderer: ready");
	return true;
}

bool Renderer::EnsurePipeline(DXGI_FORMAT aFormat, bool aReversed)
{
	if (pipeline_ && pipelineFormat_ == aFormat && pipelineCull_ == cull_ && pipelineReversed_ == aReversed)
	{
		return true;
	}
	if (!shaders_)
	{
		Fail("there are no shaders");
		return false;
	}
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.VS = { shaders_->vertex.data(), shaders_->vertex.size() };
	desc.PS = { shaders_->present.data(), shaders_->present.size() };
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = cull_ == 0 ? D3D12_CULL_MODE_NONE : cull_ == 1 ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_FRONT;
	desc.RasterizerState.DepthClipEnable = TRUE;
	desc.DepthStencilState.DepthEnable = TRUE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthStencilState.DepthFunc = aReversed ? D3D12_COMPARISON_FUNC_GREATER : D3D12_COMPARISON_FUNC_LESS;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = aFormat;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	desc.SampleDesc.Count = 1;
	gpu::ComPtr<ID3D12PipelineState> pipeline;
	if (FAILED(device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline))))
	{
		Fail(std::format("couldn't create a pipeline for {}", DxgiFormatName(aFormat)));
		return false;
	}
	pipeline->SetName(L"BlockRender present pipeline");
	pipeline_ = pipeline;
	pipelineFormat_ = aFormat;
	pipelineCull_ = cull_;
	pipelineReversed_ = aReversed;
	return true;
}

bool Renderer::EnsureDepthTarget(uint32_t aWidth, uint32_t aHeight)
{
	if (depthTarget_ && depthWidth_ == aWidth && depthHeight_ == aHeight)
	{
		return true;
	}
	if (depthTarget_)
	{
		Retire(depthTarget_);
		depthTarget_.Reset();
	}
	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = aWidth;
	desc.Height = aHeight;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_D32_FLOAT;
	desc.SampleDesc.Count = 1;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE clear{};
	clear.Format = DXGI_FORMAT_D32_FLOAT;
	if (FAILED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear,
	                                            IID_PPV_ARGS(&depthTarget_))))
	{
		Fail("couldn't create its depth buffer");
		return false;
	}
	depthTarget_->SetName(L"BlockRender depth");
	d3d::Original().createDsv(device_, depthTarget_.Get(), nullptr, dsvHeap_->GetCPUDescriptorHandleForHeapStart());
	depthWidth_ = aWidth;
	depthHeight_ = aHeight;
	return true;
}

void Renderer::Skip(const std::string& aWhy)
{
	++stats_.skipped;
	stats_.lastSkip = aWhy;
	if (++consecutiveSkips_ == kSkipsBeforeOff)
	{
		Fail(std::format("{} frames in a row were skipped ({})", kSkipsBeforeOff, aWhy));
	}
}

void Renderer::Fail(const std::string& aWhy)
{
	log_.Push("renderer: " + aWhy + ", so drawing is off");
	stats_.lastSkip = aWhy;
	mode_ = Mode::Off;
}

uint64_t Renderer::Signal()
{
	queue_->Signal(fence_.Get(), ++fenceValue_);
	return fenceValue_;
}

void Renderer::Retire(gpu::ComPtr<ID3D12Resource> aResource)
{
	// Every list of ours that used it has been submitted, and the fence was signalled after each.
	retired_.emplace_back(std::move(aResource), fenceValue_);
}

void Renderer::ReadTimings(uint32_t aSlot)
{
	const uint64_t begin = timingsMapped_[aSlot * 2];
	const uint64_t end = timingsMapped_[aSlot * 2 + 1];
	if (end > begin && timestampFrequency_ > 0)
	{
		const double ms = double(end - begin) * 1000.0 / double(timestampFrequency_);
		gpuMsSum_ += ms;
		++gpuSamples_;
		stats_.gpuMsWorst = std::max(stats_.gpuMsWorst, ms);
	}
}

void Renderer::Update()
{
	std::lock_guard lock(mutex_);
	if ((!fence_ && mode_.load() == Mode::Off) || !EnsureObjects())
	{
		return;  // nothing is created until drawing is first switched on
	}
	const uint64_t completed = fence_->GetCompletedValue();
	if (completed == UINT64_MAX)
	{
		return;  // the device was removed: the plugin reports it
	}
	std::erase_if(retired_, [completed](const auto& aEntry) { return aEntry.second <= completed; });
	const bool atlasDue = atlas_ && !atlasUploaded_;
	if ((pending_.empty() && !atlasDue) || completed < uploadFence_)
	{
		return;
	}
	const d3d::Originals& original = d3d::Original();
	if (FAILED(uploadAllocator_->Reset()) || FAILED(original.reset(uploadList_.Get(), uploadAllocator_.Get(), nullptr)))
	{
		Fail("couldn't reset its upload list");
		return;
	}
	const uint64_t                           ready = fenceValue_ + 1;  // the value the Signal below sets
	std::vector<gpu::ComPtr<ID3D12Resource>> staging;
	bool                                     failed = false;
	const auto upload = [&](const void* aData, uint64_t aBytes) -> gpu::ComPtr<ID3D12Resource> {
		auto  target = gpu::MakeBuffer(device_, aBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
		auto  source = gpu::MakeBuffer(device_, aBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
		void* mapped = nullptr;
		if (!target || !source || FAILED(source->Map(0, nullptr, &mapped)))
		{
			failed = true;
			return nullptr;
		}
		std::memcpy(mapped, aData, aBytes);
		source->Unmap(0, nullptr);
		uploadList_->CopyBufferRegion(target.Get(), 0, source.Get(), 0, aBytes);
		staging.push_back(source);
		return target;
	};
	if (atlasDue)
	{
		if (atlasBuffer_)
		{
			Retire(atlasBuffer_);
		}
		atlasBuffer_ = upload(atlas_->texels.data(), atlas_->texels.size() * sizeof(uint32_t));
		if (atlasBuffer_)
		{
			atlasBuffer_->SetName(L"BlockRender atlas");
		}
		atlasReady_ = ready;
		atlasUploaded_ = true;
	}
	for (auto& [section, vertices] : pending_)
	{
		if (const auto found = sections_.find(section); found != sections_.end())
		{
			Retire(found->second.buffer);
			sections_.erase(found);
		}
		if (vertices.empty())
		{
			continue;
		}
		SectionGpu gpuSection;
		gpuSection.buffer = upload(vertices.data(), vertices.size() * sizeof(Vertex));
		gpuSection.vertices = uint32_t(vertices.size());
		gpuSection.ready = ready;
		if (gpuSection.buffer)
		{
			gpuSection.buffer->SetName(L"BlockRender section");
			sections_[section] = std::move(gpuSection);
		}
	}
	pending_.clear();
	original.close(uploadList_.Get());
	ID3D12CommandList* lists[] = { uploadList_.Get() };
	original.executeCommandLists(queue_, 1, lists);
	uploadFence_ = Signal();
	for (auto& buffer : staging)
	{
		retired_.emplace_back(std::move(buffer), uploadFence_);
	}
	if (failed)
	{
		log_.Push("renderer: couldn't create some upload buffers; those sections aren't drawn");
	}
}

Renderer::Stats Renderer::TakeStats()
{
	std::lock_guard lock(mutex_);
	Stats stats = stats_;
	stats.gpuMsAverage = gpuSamples_ ? gpuMsSum_ / double(gpuSamples_) : -1.0;
	stats.frameMsAverage = frameSamples_ ? frameMsSum_ / double(frameSamples_) : -1.0;
	stats.sections = sections_.size();
	for (const auto& [section, gpuSection] : sections_)
	{
		stats.vertices += gpuSection.vertices;
	}
	stats_ = {};
	gpuMsSum_ = frameMsSum_ = 0.0;
	gpuSamples_ = frameSamples_ = 0;
	return stats;
}

void Renderer::OnEvaluate(const EvaluateCall& aCall)
{
	if (mode_.load() != Mode::Present)
	{
		return;
	}
	std::lock_guard lock(mutex_);
	if (mode_.load() != Mode::Present || !aCall.list || !EnsureObjects())
	{
		return;
	}
	recorded_ = {};
	recorded_.list = aCall.list;
	recorded_.frame = aCall.frame;
	recorded_.valid = aCall.constants.has_value();
	if (aCall.constants)
	{
		recorded_.constants = *aCall.constants;
	}
	recordedList_ = aCall.list;
	const SlTag* depth = nullptr;
	for (const SlTag& tag : aCall.tags)
	{
		if (tag.type == sl::kBufferTypeDepth && tag.resource && tag.state != 0xFFFFFFFF)
		{
			depth = &tag;
		}
	}
	if (!depth)
	{
		return;  // drawn without the game's depth
	}
	auto*                     resource = static_cast<ID3D12Resource*>(depth->resource);
	const D3D12_RESOURCE_DESC desc = resource->GetDesc();
	if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || DumpFormat(desc.Format, 0) != uint32_t(DXGI_FORMAT_R32_FLOAT))
	{
		return;
	}
	const uint32_t       subresource = gpu::PlaneSubresource(desc, 0);
	const gpu::Footprint footprint = gpu::FootprintOf(device_, desc, subresource);
	if (!depthCopy_ || depthCopyBytes_ < footprint.totalBytes)
	{
		if (depthCopy_)
		{
			oldDepthCopies_.push_back(depthCopy_);
		}
		depthCopy_ = gpu::MakeBuffer(device_, footprint.totalBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
		depthCopyBytes_ = depthCopy_ ? footprint.totalBytes : 0;
		if (!depthCopy_)
		{
			Fail("couldn't create the depth copy");
			return;
		}
		depthCopy_->SetName(L"BlockRender depth copy");
	}
	if (!gpu::RecordCopy(aCall.list, resource, subresource, depth->state, depthCopy_.Get(), footprint))
	{
		return;  // drawn without the game's depth
	}
	recorded_.depth = true;
	recorded_.rowFloats = footprint.layout.Footprint.RowPitch / 4;
	const bool wholeResource = depth->extent.width == 0 || depth->extent.height == 0;
	recorded_.extent = wholeResource ? sl::Extent{ 0, 0, uint32_t(desc.Width), desc.Height } : depth->extent;
}

void Renderer::OnSubmit(ID3D12CommandQueue*, UINT aCount, ID3D12CommandList* const* aLists)
{
	const void* recorded = recordedList_.load();
	if (!recorded)
	{
		return;
	}
	for (UINT i = 0; i < aCount; ++i)
	{
		if (static_cast<const void*>(aLists[i]) == recorded)
		{
			std::lock_guard lock(mutex_);
			if (recorded_.list == recorded)
			{
				submitted_ = recorded_;
				recorded_ = {};
				recordedList_ = nullptr;
			}
			return;
		}
	}
}

void Renderer::OnPresent(IDXGISwapChain* aSwapChain)
{
	const int64_t   now = Now();
	std::lock_guard lock(mutex_);
	if (lastPresent_ != 0)
	{
		const double ms = MsBetween(lastPresent_, now);
		frameMsSum_ += ms;
		++frameSamples_;
		stats_.frameMsWorst = std::max(stats_.frameMsWorst, ms);
	}
	lastPresent_ = now;
	if (mode_.load() != Mode::Present)
	{
		return;
	}
	if (!EnsureObjects())
	{
		return Skip("not ready: the main queue isn't known yet");
	}
	const uint64_t completed = fence_->GetCompletedValue();
	if (completed == UINT64_MAX)
	{
		return Fail(std::format("the device was removed (0x{:08x})", uint32_t(device_->GetDeviceRemovedReason())));
	}
	if (aSwapChain != checkedSwapChain_)
	{
		checkedSwapChain_ = aSwapChain;
		swapChainOnMainQueue_ = d3d::PresentQueue(aSwapChain) == queue_;
		log_.Push(std::format("renderer: swap chain 0x{:x} presents on {}", reinterpret_cast<uintptr_t>(aSwapChain),
		                      swapChainOnMainQueue_ ? "the main queue" : "another queue"));
	}
	if (!swapChainOnMainQueue_)
	{
		return Skip("the swap chain presents on another queue than DLSS's");
	}
	if (!submitted_.list)
	{
		return Skip("no DLSS frame has been submitted yet");
	}
	if (!submitted_.valid)
	{
		return Skip("the DLSS frame came without constants");
	}
	if (!atlasBuffer_ || completed < atlasReady_)
	{
		return Skip("the atlas isn't on the GPU yet");
	}
	const std::optional<CameraFrame> camera = MakeCameraFrame(submitted_.constants, renderOrigin_);
	if (!camera)
	{
		return Skip("the camera isn't understood");
	}
	gpu::ComPtr<IDXGISwapChain3> swapChain;
	gpu::ComPtr<ID3D12Resource>  backBuffer;
	if (FAILED(aSwapChain->QueryInterface(IID_PPV_ARGS(&swapChain))) ||
	    FAILED(swapChain->GetBuffer(swapChain->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&backBuffer))))
	{
		return Skip("no back buffer");
	}
	const D3D12_RESOURCE_DESC target = backBuffer->GetDesc();
	if (!EnsurePipeline(target.Format, camera->reversedDepth) || !EnsureDepthTarget(uint32_t(target.Width), target.Height))
	{
		return;
	}
	const uint32_t slot = uint32_t(presents_ % kSlots);
	if (completed < slotFence_[slot])
	{
		return Skip("the GPU is still busy with an earlier frame");
	}
	if (slotFence_[slot] != 0)
	{
		ReadTimings(slot);
	}

	FrameConstants constants{};
	for (size_t i = 0; i < 16; ++i)
	{
		constants.cameraToClip[i] = float(camera->cameraToClip[i]);
	}
	constants.target[0] = float(target.Width);
	constants.target[1] = float(target.Height);
	constants.target[2] = 1.0f / float(target.Width);
	constants.target[3] = 1.0f / float(target.Height);
	constants.depthExtent[0] = float(submitted_.extent.left);
	constants.depthExtent[1] = float(submitted_.extent.top);
	constants.depthExtent[2] = float(submitted_.extent.width);
	constants.depthExtent[3] = float(submitted_.extent.height);
	constants.depthInfo[0] = submitted_.rowFloats;
	constants.depthInfo[1] = submitted_.depth ? 1u : 0u;
	constants.depthInfo[2] = camera->reversedDepth ? 1u : 0u;
	constants.atlasInfo[0] = atlas_->width;
	constants.atlasInfo[1] = atlas_->height;
	constants.atlasInfo[2] = atlas_->Levels();
	for (size_t i = 0; i < atlas_->levelOffsets.size() && i < 8; ++i)
	{
		constants.atlasLevels[i] = atlas_->levelOffsets[i];
	}
	constants.tuning[0] = brightness_;
	constants.tuning[1] = tolerance_;
	std::memcpy(constantsMapped_ + slot * kConstantsStride, &constants, sizeof(constants));

	// Our own list: calls the hooks watch go through their originals, so nothing of ours reaches the tracker.
	const d3d::Originals&      original = d3d::Original();
	ID3D12GraphicsCommandList* list = lists_[slot].Get();
	if (FAILED(allocators_[slot]->Reset()) || FAILED(original.reset(list, allocators_[slot].Get(), pipeline_.Get())))
	{
		return Fail("couldn't reset a command list");
	}
	list->EndQuery(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2);
	const D3D12_RESOURCE_BARRIER toTarget = Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	original.resourceBarrier(list, 1, &toTarget);
	const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
	original.createRtv(device_, backBuffer.Get(), nullptr, rtv);
	original.clearDsv(list, dsv, D3D12_CLEAR_FLAG_DEPTH, camera->reversedDepth ? 0.0f : 1.0f, 0, 0, nullptr);
	original.omSetRenderTargets(list, 1, &rtv, FALSE, &dsv);
	const D3D12_VIEWPORT viewport{ 0.0f, 0.0f, float(target.Width), float(target.Height), 0.0f, 1.0f };
	const D3D12_RECT     scissor{ 0, 0, LONG(target.Width), LONG(target.Height) };
	list->RSSetViewports(1, &viewport);
	list->RSSetScissorRects(1, &scissor);
	list->SetGraphicsRootSignature(rootSignature_.Get());
	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	list->SetGraphicsRootConstantBufferView(0, constants_->GetGPUVirtualAddress() + slot * kConstantsStride);
	list->SetGraphicsRootShaderResourceView(3, atlasBuffer_->GetGPUVirtualAddress());
	// Without a depth copy the shader never reads t2, but the root argument still needs a valid buffer.
	list->SetGraphicsRootShaderResourceView(4, (submitted_.depth ? depthCopy_ : atlasBuffer_)->GetGPUVirtualAddress());
	for (const auto& [section, gpuSection] : sections_)
	{
		if (gpuSection.ready > completed)
		{
			continue;  // still uploading
		}
		const Vec3d corner = WorldFromMc({ section.x * 16.0, section.y * 16.0, section.z * 16.0 });
		const Vec3d offset{ corner.x - camera->origin.x, corner.y - camera->origin.y, corner.z - camera->origin.z };
		// The section fills world x..x+16, y-16..y and z..z+16 from that corner, since Minecraft z runs along world -y.
		if (!Visible(camera->cameraToClip, { offset.x, offset.y - 16.0, offset.z }, { offset.x + 16.0, offset.y, offset.z + 16.0 }))
		{
			continue;
		}
		const float sectionConstants[4] = { float(offset.x), float(offset.y), float(offset.z), 0.0f };
		list->SetGraphicsRoot32BitConstants(1, 4, sectionConstants, 0);
		list->SetGraphicsRootShaderResourceView(2, gpuSection.buffer->GetGPUVirtualAddress());
		original.drawInstanced(list, gpuSection.vertices, 1, 0, 0);
	}
	const D3D12_RESOURCE_BARRIER toPresent = Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	original.resourceBarrier(list, 1, &toPresent);
	list->EndQuery(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2 + 1);
	list->ResolveQueryData(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2, 2, timings_.Get(), slot * 2 * sizeof(uint64_t));
	if (FAILED(original.close(list)))
	{
		return Fail("couldn't close a command list");
	}
	ID3D12CommandList* lists[] = { list };
	original.executeCommandLists(queue_, 1, lists);
	slotFence_[slot] = Signal();
	++presents_;
	++stats_.drawn;
	if (lastDrawnFrame_ == submitted_.frame)
	{
		++stats_.lateCamera;
	}
	lastDrawnFrame_ = submitted_.frame;
	consecutiveSkips_ = 0;
}

void Renderer::Shutdown()
{
	mode_ = Mode::Off;
	std::lock_guard lock(mutex_);
	if (fence_ && fence_->GetCompletedValue() < fenceValue_)
	{
		const HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (event && SUCCEEDED(fence_->SetEventOnCompletion(fenceValue_, event)))
		{
			WaitForSingleObject(event, 2000);
		}
		if (event)
		{
			CloseHandle(event);
		}
	}
	if (constantsMapped_)
	{
		constants_->Unmap(0, nullptr);
		constantsMapped_ = nullptr;
	}
	if (timingsMapped_)
	{
		timings_->Unmap(0, nullptr);
		timingsMapped_ = nullptr;
	}
	sections_.clear();
	pending_.clear();
	retired_.clear();
	oldDepthCopies_.clear();
	lists_ = {};
	allocators_ = {};
	depthCopy_.Reset();
	atlasBuffer_.Reset();
	depthTarget_.Reset();
	pipeline_.Reset();
	rootSignature_.Reset();
	rtvHeap_.Reset();
	dsvHeap_.Reset();
	constants_.Reset();
	timings_.Reset();
	queries_.Reset();
	uploadList_.Reset();
	uploadAllocator_.Reset();
	fence_.Reset();
}
}
```

- [x] **Step 3: Edit `CMakeLists.txt` to add the source**

```cmake
	src/log/Dumps.cpp
)
```
```cmake
	src/log/Dumps.cpp
	src/render/Renderer.cpp
)
```

- [x] **Step 4: Build; it must compile without warnings, and the tests still pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `83 test(s), 0 failure(s)`.

- [x] **Step 5: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: renderer, drawing at Present (step 2)" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 27: The plugin and the overlay

**`Plugin.cpp`** holds the RED4ext entry points.
- **At load,** it:
  - reads `settings.txt`;
  - makes the frame log, the dumps and the renderer, and adds each as a listener;
  - installs the Streamline hooks, then the D3D12 detours.

  If Streamline's imports are missing, nothing is hooked.
- **Every frame, on the main thread,** it:
  - runs `blockrender.request` once a second;
  - meshes changed sections, and runs the renderer's uploads;
  - writes the frame log and the dumps when they're done, and the queued log lines;
  - writes `status.txt` once a second.
- **Once a second,** it also:
  - patches the device and the swap chain through GpuApi, if the detours missed them (risk 1);
  - checks whether the device has been removed, and if so, logs DRED's findings.
- **The module is pinned,** because the hooks point into it.

**The overlay** (`blockrender_hud`, CET Lua) shows `status.txt`, plus V's position, heading and Minecraft block, and
the distance to the nearest structure. The deploy step copies the DLL, its PDB and the shaders into the plugin's
folder, and the overlay into CET's mods.

**Files:**
- Create: `src/PlayerLocator.h`, `src/Plugin.cpp`, `tools/blockrender_hud/init.lua`
- Copy from `spikes/physx-probe/`: `src/PlayerLocator.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write `src/PlayerLocator.h`**

```cpp
#pragma once

#include "Coords.h"

#include <optional>

namespace cpmc
{
struct CameraPose
{
	Vec3d position;
	Vec3f forward;
	float fov = 0.0f;  // vertical, degrees
};

struct PlayerPose
{
	Vec3d                     feet;
	Vec3f                     forward;
	std::optional<CameraPose> camera;
};

// V's feet and facing, plus the active camera when it can be read, through the game's own script functions:
// PlayerSystem.GetLocalPlayerMainGameObject, then GetWorldPosition and GetWorldForward on V, and
// CameraSystem.GetActiveCameraWorldTransform, GetActiveCameraForward and GetActiveCameraFOV. Copied from spike 1.
// Nothing when there is no player (main menu, loading). Main thread only.
std::optional<PlayerPose> LocatePlayer();
}
```

- [x] **Step 2: Copy `../physx-probe/src/PlayerLocator.cpp` to `src/PlayerLocator.cpp`**

```bash
cp /d/Cyberpunk-Minecraft/spikes/physx-probe/src/PlayerLocator.cpp /d/Cyberpunk-Minecraft/spikes/block-render/src/
```

- [x] **Step 3: Write `src/Plugin.cpp`**

```cpp
// BlockRender: spike 3's RED4ext plugin. It hooks the game's Streamline calls and D3D12 to map Cyberpunk's frame
// (step 1: the frame log and the dumps), and draws Minecraft blocks into it (step 2: on the finished frame). Commands
// come through blockrender.request in its folder, and status.txt in the overlay's folder says what it's doing. See
// docs/superpowers/specs/2026-10-01-block-render-design.md.
#include <RED4ext/RED4ext.hpp>

#include "LogQueue.h"
#include "PlayerLocator.h"
#include "Requests.h"
#include "Status.h"
#include "hooks/D3D12Hooks.h"
#include "hooks/StreamlineHooks.h"
#include "log/Dumps.h"
#include "log/FrameLog.h"
#include "log/FrameLogData.h"
#include "render/Renderer.h"
#include "track/GpuApi.h"
#include "world/BlockWorld.h"
#include "world/Mesher.h"
#include "world/Structures.h"

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr const char* kVersion = "0.1.0";

RED4ext::v1::PluginHandle g_handle = nullptr;
const RED4ext::v1::Sdk*   g_sdk = nullptr;
std::filesystem::path     g_pluginDir;
std::filesystem::path     g_hudDir;  // the CET overlay's folder, where status.txt goes
cpmc::LogQueue            g_log;
bool                      g_hooked = false;  // Streamline's imports are patched

// Made at load and never destroyed: the hooks may call them until the process ends.
cpmc::FrameLog* g_frameLog = nullptr;
cpmc::Dumps*    g_dumps = nullptr;
cpmc::Renderer* g_renderer = nullptr;

cpmc::BlockWorld                           g_world;
cpmc::MeshOptions                          g_meshOptions;
std::vector<cpmc::StatusReport::Structure> g_structures;
std::string                                g_lastRequest = "none yet";
Clock::time_point                          g_lastSecond{};
uint64_t                                   g_lastFrameTokens = 0;
bool                                       g_removalReported = false;
bool                                       g_hudMissingReported = false;

template<class... Args>
void Log(std::format_string<Args...> aFormat, Args&&... aArgs)
{
	if (g_sdk)
	{
		g_sdk->logger->Info(g_handle, std::format(aFormat, std::forward<Args>(aArgs)...).c_str());
	}
}

void DrainLog()
{
	for (const std::string& line : g_log.Drain())
	{
		Log("{}", line);
	}
}

std::filesystem::path PluginFolder()
{
	HMODULE module = nullptr;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                   reinterpret_cast<LPCWSTR>(&PluginFolder), &module);
	wchar_t path[MAX_PATH]{};
	GetModuleFileNameW(module, path, MAX_PATH);
	return std::filesystem::path(path).parent_path();
}

// "2026-10-01T12:34:56" for the logs, "2026-10-01_12-34-56" for folder names.
std::pair<std::string, std::string> Timestamps()
{
	SYSTEMTIME now{};
	GetLocalTime(&now);
	return { std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond),
	         std::format("{:04}-{:02}-{:02}_{:02}-{:02}-{:02}", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond) };
}

// At most the first 4 KB of a small text file; empty if it can't be read.
std::string ReadSmallFile(const std::filesystem::path& aPath)
{
	std::ifstream in(aPath, std::ios::binary);
	std::string   text(4096, '\0');
	in.read(text.data(), std::streamsize(text.size()));
	text.resize(size_t(in.gcount()));
	return text;
}

// aText on one line for the log: control characters and anything past ASCII become '?', at most 200 characters.
std::string Printable(const std::string& aText)
{
	std::string out;
	for (const char ch : aText.substr(0, 200))
	{
		out += ch >= 32 && ch < 127 ? ch : '?';
	}
	return out;
}

void Report(std::string aResult)
{
	Log("request: {}", aResult);
	g_lastRequest = std::move(aResult);
}

void Place(const cpmc::Request& aRequest)
{
	std::string error;
	if (!g_renderer->AtlasData() && !g_renderer->LoadAtlas(error))
	{
		return Report("place " + aRequest.what + ": " + error);
	}
	const std::optional<cpmc::PlayerPose> pose = cpmc::LocatePlayer();
	if (!pose)
	{
		return Report("place " + aRequest.what + ": there's no player");
	}
	const cpmc::BlockPos                origin = cpmc::PlacementOrigin(pose->feet, pose->forward);
	const std::optional<cpmc::Placed>   placed = cpmc::PlaceStructure(g_world, *g_renderer->AtlasData(), aRequest.what, origin, error);
	if (!placed)
	{
		return Report("place " + aRequest.what + ": " + error);
	}
	const auto [low, high] = cpmc::WorldBox(*placed);
	g_structures.push_back({ aRequest.what, low, high });
	Report(std::format("placed {}: {} blocks, Minecraft {} {} {} to {} {} {}", aRequest.what, placed->blocks, placed->min.x, placed->min.y,
	                   placed->min.z, placed->max.x, placed->max.y, placed->max.z));
}

void Set(const cpmc::Request& aRequest)
{
	if (aRequest.what == "ao")
	{
		g_meshOptions.ambientOcclusion = aRequest.number != 0.0;
		g_world.MarkAllDirty();
		return Report(std::format("set ao {}", g_meshOptions.ambientOcclusion ? 1 : 0));
	}
	std::string error;
	Report(g_renderer->Set(aRequest.what, aRequest.number, error) ? std::format("set {} {}", aRequest.what, aRequest.number)
	                                                              : "set: " + error);
}

void RunRequest()
{
	const std::filesystem::path path = g_pluginDir / "blockrender.request";
	std::error_code             error;
	if (!std::filesystem::exists(path, error))
	{
		return;
	}
	const std::string text = ReadSmallFile(path);
	std::filesystem::remove(path, error);
	std::string                         why;
	const std::optional<cpmc::Request> request = cpmc::ParseRequest(text, why);
	if (!request)
	{
		return Report(std::format("\"{}\" ignored: {}", Printable(text), why));
	}
	switch (request->kind)
	{
	case cpmc::Request::Kind::Log: Report(g_frameLog->Arm() ? "log: armed for the next two frames" : "log: one is already running"); break;
	case cpmc::Request::Kind::Dump:
		Report(g_dumps->Arm(request->what, why) ? "dump " + request->what + ": armed for the next frame" : "dump: " + why);
		break;
	case cpmc::Request::Kind::Mode:
		Report(g_renderer->SetMode(request->what, why) ? "mode " + request->what : "mode " + request->what + ": " + why);
		break;
	case cpmc::Request::Kind::Place: Place(*request); break;
	case cpmc::Request::Kind::Clear:
		g_world.Clear();
		g_structures.clear();
		Report("cleared");
		break;
	case cpmc::Request::Kind::Reload: Report(g_renderer->ReloadShaders(why) ? "reload: shaders compiled" : "reload: " + why); break;
	case cpmc::Request::Kind::Set: Set(*request); break;
	}
}

// If the device or the swap chain was made before BlockRender loaded, the detours never saw it: patch it through
// GpuApi instead (risk 1), once its vtable checks out.
void PatchLate()
{
	if (cpmc::d3d::Device() && cpmc::d3d::Original().present)
	{
		return;
	}
	const cpmc::gpuapi::Report report = cpmc::gpuapi::Check(cpmc::d3d::Device());
	if (!cpmc::d3d::Device() && report.nativeDevice)
	{
		cpmc::d3d::PatchLateDevice(report.nativeDevice);
		Log("the device was made before BlockRender loaded: patched it through GpuApi ({})", report.device);
	}
	if (!cpmc::d3d::Original().present && report.swapChainObject)
	{
		void* native = cpmc::slhooks::NativeInterface(report.swapChainObject);  // null unless it's a Streamline proxy
		cpmc::d3d::PatchLateSwapChain(native ? static_cast<IDXGISwapChain*>(native) : report.swapChainObject);
		if (native)
		{
			static_cast<IUnknown*>(native)->Release();
		}
		Log("the swap chain was made before BlockRender loaded: patched it through GpuApi ({})", report.swapChain);
	}
}

void CheckDevice()
{
	ID3D12Device* const device = cpmc::d3d::Device();
	if (!device || g_removalReported)
	{
		return;
	}
	const HRESULT reason = device->GetDeviceRemovedReason();
	if (reason == S_OK)
	{
		return;
	}
	g_removalReported = true;
	std::string error;
	g_renderer->SetMode("off", error);
	Log("the D3D12 device was removed (0x{:08x}), so drawing is off", uint32_t(reason));
	for (const std::string& line : cpmc::d3d::DredReport())
	{
		Log("{}", line);
	}
}

void WriteFrameLog(const cpmc::FrameLogData& aLog, const std::string& aFolderName)
{
	const std::filesystem::path folder = g_pluginDir / "logs" / aFolderName;
	std::error_code             error;
	std::filesystem::create_directories(folder, error);
	std::ofstream out(folder / "frame.json", std::ios::binary);
	out << cpmc::FrameLogJson(aLog);
	if (!out)
	{
		return Report("log: couldn't write " + (folder / "frame.json").string());
	}
	Report(std::format("log: written to logs\\{}\\frame.json ({} evaluates, {} submissions, {} lists, {} G-buffer candidates)",
	                   aFolderName, aLog.evaluates.size(), aLog.submissions.size(), aLog.lists.size(), aLog.gbufferCandidates.size()));
}

void WriteStatus()
{
	const cpmc::Renderer::Stats stats = g_renderer->TakeStats();
	cpmc::StatusReport          report;
	report.mode = g_renderer->ModeName();
	report.framesDrawn = stats.drawn;
	report.framesSkipped = stats.skipped;
	report.lastSkip = stats.lastSkip;
	report.lateCamera = stats.lateCamera;
	report.sections = stats.sections;
	report.blocks = g_world.BlockCount();
	report.vertices = stats.vertices;
	report.gpuMsAverage = stats.gpuMsAverage;
	report.gpuMsWorst = stats.gpuMsWorst;
	report.frameMsAverage = stats.frameMsAverage;
	report.frameMsWorst = stats.frameMsWorst;
	const uint64_t tokens = cpmc::slhooks::FrameTokens();
	report.streamlineFrames = uint32_t(tokens - g_lastFrameTokens);
	g_lastFrameTokens = tokens;
	report.hooks = g_hooked ? std::format("{}; {}; log {}; dump {}", cpmc::slhooks::Summary(), cpmc::d3d::Summary(), g_frameLog->State(),
	                                      g_dumps->State())
	                        : "not installed (see red4ext\\logs)";
	report.lastRequest = g_lastRequest;
	report.structures = g_structures;
	std::error_code error;
	if (!std::filesystem::is_directory(g_hudDir, error))
	{
		if (!g_hudMissingReported)
		{
			g_hudMissingReported = true;
			Log("the overlay's folder {} is missing, so there's no status file", g_hudDir.string());
		}
		return;
	}
	const std::filesystem::path temporary = g_hudDir / "status.tmp";
	{
		std::ofstream out(temporary, std::ios::binary);
		out << cpmc::FormatStatus(report);
	}
	// While the overlay is reading status.txt this fails; the next second's file replaces it.
	std::filesystem::rename(temporary, g_hudDir / "status.txt", error);
}

bool OnEnter(RED4ext::CGameApplication*)
{
	return true;
}

bool OnExit(RED4ext::CGameApplication*)
{
	return true;
}

// Every frame on the main thread while the game runs (RED4ext ignores the return value in this state).
bool OnUpdate(RED4ext::CGameApplication*)
{
	const auto now = Clock::now();
	const bool newSecond = now - g_lastSecond >= 1s;
	if (newSecond)
	{
		g_lastSecond = now;
		if (g_hooked)
		{
			PatchLate();
			CheckDevice();
		}
		RunRequest();
	}
	const auto [time, folderName] = Timestamps();
	if (g_frameLog->Capturing())
	{
		if (const std::optional<cpmc::PlayerPose> pose = cpmc::LocatePlayer(); pose && pose->camera)
		{
			g_frameLog->NoteScriptCamera({ pose->camera->position, pose->camera->forward, pose->camera->fov });
		}
	}
	if (const std::optional<cpmc::FrameLogData> finished = g_frameLog->TakeFinished(kVersion, time))
	{
		WriteFrameLog(*finished, folderName);
	}
	g_dumps->Update(folderName);
	if (const cpmc::Atlas* atlas = g_renderer->AtlasData())
	{
		for (const cpmc::SectionPos& section : g_world.TakeDirty())
		{
			g_renderer->UploadSection(section, cpmc::MeshSection(g_world, section, *atlas, g_meshOptions));
		}
	}
	g_renderer->Update();
	DrainLog();
	if (newSecond)
	{
		for (const std::string& line : cpmc::d3d::DebugMessages())
		{
			Log("{}", line);
		}
		WriteStatus();
	}
	return false;
}
}

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::v1::PluginHandle aHandle, RED4ext::v1::EMainReason aReason,
                                        const RED4ext::v1::Sdk* aSdk)
{
	switch (aReason)
	{
	case RED4ext::v1::EMainReason::Load:
	{
		g_handle = aHandle;
		g_sdk = aSdk;
		// Pinned: the hooks point into this DLL, and threads may be inside them when the game unloads plugins.
		HMODULE self = nullptr;
		GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&PluginFolder),
		                   &self);
		g_pluginDir = PluginFolder();
		g_hudDir = g_pluginDir.parent_path().parent_path().parent_path() / "bin" / "x64" / "plugins" / "cyber_engine_tweaks" / "mods" /
		           "blockrender_hud";
		const cpmc::PluginSettings settings = cpmc::ParseSettings(ReadSmallFile(g_pluginDir / "settings.txt"));
		g_frameLog = new cpmc::FrameLog(g_log);
		g_dumps = new cpmc::Dumps(g_log, g_pluginDir);
		g_renderer = new cpmc::Renderer(g_log, g_pluginDir);
		for (cpmc::HookEvents* listener : std::initializer_list<cpmc::HookEvents*>{ g_frameLog, g_dumps, g_renderer })
		{
			cpmc::d3d::AddListener(listener);
			cpmc::slhooks::AddListener(listener);
		}
		g_hooked = cpmc::slhooks::Install(g_log);
		if (g_hooked && !cpmc::d3d::InstallDetours(aSdk, aHandle, g_log, { settings.dred, settings.debugLayer }))
		{
			g_log.Push("some D3D12 or DXGI detours are missing (see above)");
		}
		static RED4ext::v1::GameState running{ &OnEnter, &OnUpdate, &OnExit };
		aSdk->gameStates->Add(aHandle, RED4ext::EGameStateType::Running, &running);
		DrainLog();
		Log("BlockRender {} loaded (game {}.{}.{}), {}; dred {}, debug layer {}; requests go in {}", kVersion,
		    unsigned(aSdk->runtime->major), aSdk->runtime->minor, aSdk->runtime->patch, g_hooked ? "hooked" : "NOT hooked",
		    settings.dred ? "on" : "off", settings.debugLayer ? "on" : "off", (g_pluginDir / "blockrender.request").string());
		break;
	}
	case RED4ext::v1::EMainReason::Unload:
		if (g_renderer)
		{
			g_renderer->Shutdown();
		}
		cpmc::d3d::RemovePatches();
		cpmc::slhooks::Remove();
		DrainLog();
		break;
	}
	return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* aInfo)
{
	aInfo->name = L"BlockRender";
	aInfo->author = L"Cyberpunk-Minecraft";
	aInfo->version = RED4EXT_V1_SEMVER(0, 1, 0);
	aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
	aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
	return RED4EXT_API_VERSION_1;
}
```

- [x] **Step 4: Write `tools/blockrender_hud/init.lua`**

```lua
-- BlockRender HUD: what BlockRender (spike 3 of D:\Cyberpunk-Minecraft) is doing, from the status.txt it writes into
-- this folder once a second, plus V's position, heading and Minecraft block, and the nearest structure. Read-only.
-- Delete this folder to remove it.
local status = nil       -- status.txt's lines, except the structures
local structures = {}    -- { name, min = { x, y, z }, max = { x, y, z } }, in world coordinates
local player = nil       -- the lines about V
local sinceRead = 1.0

local atan2 = math.atan2 or function(y, x) return math.atan(y, x) end

local function readStatus()
    local file = io.open("status.txt", "r")
    if file == nil then
        status = nil
        structures = {}
        return
    end
    status = {}
    structures = {}
    for line in file:lines() do
        local name, a, b, c, d, e, f = line:match("^structure=(%S+) (%S+) (%S+) (%S+) (%S+) (%S+) (%S+)$")
        if name ~= nil then
            table.insert(structures, {
                name = name,
                min = { tonumber(a), tonumber(b), tonumber(c) },
                max = { tonumber(d), tonumber(e), tonumber(f) },
            })
        else
            table.insert(status, line)
        end
    end
    file:close()
end

-- How far a point is from a box, 0 inside it.
local function distanceToBox(p, box)
    local function gap(value, low, high)
        if value < low then return low - value end
        if value > high then return value - high end
        return 0
    end
    local dx = gap(p.x, box.min[1], box.max[1])
    local dy = gap(p.y, box.min[2], box.max[2])
    local dz = gap(p.z, box.min[3], box.max[3])
    return math.sqrt(dx * dx + dy * dy + dz * dz)
end

registerForEvent("onUpdate", function(delta)
    sinceRead = sinceRead + delta
    if sinceRead >= 1.0 then
        sinceRead = 0.0
        readStatus()
    end
    local v = Game.GetPlayer()
    if v == nil then
        player = nil
        return
    end
    local p = v:GetWorldPosition()
    local f = v:GetWorldForward()
    local heading = math.deg(atan2(f.x, f.y)) % 360  -- 0 = north (+y), 90 = east (+x)
    -- Minecraft's axes: x is world x, y is world z (up), z is world -y (south).
    local text = string.format(
        "V: x %.2f  y %.2f  z %.2f, heading %d (0 north, 90 east)\nMinecraft block at V's feet: %d %d %d",
        p.x, p.y, p.z, math.floor(heading + 0.5), math.floor(p.x), math.floor(p.z), math.floor(-p.y))
    local nearest, nearestName = nil, nil
    for _, box in ipairs(structures) do
        local distance = distanceToBox(p, box)
        if nearest == nil or distance < nearest then
            nearest, nearestName = distance, box.name
        end
    end
    if nearest ~= nil then
        text = text .. string.format("\nnearest structure: %s, %.2f m away", nearestName, nearest)
    end
    player = text
end)

registerForEvent("onDraw", function()
    ImGui.SetNextWindowPos(30, 420, ImGuiCond.FirstUseEver)
    if ImGui.Begin("BlockRender", ImGuiWindowFlags.AlwaysAutoResize) then
        if status == nil then
            ImGui.Text("no status.txt yet: is BlockRender loaded?")
        else
            for _, line in ipairs(status) do
                ImGui.Text(line)
            end
        end
        if player ~= nil then
            ImGui.Separator()
            ImGui.Text(player)
        end
    end
    ImGui.End()
end)
```

- [x] **Step 5: Edit `CMakeLists.txt` to add the plugin and its deploy step**

```cmake
target_link_libraries(render_runtime PUBLIC render_core RED4ext::SDK d3d12)
```
```cmake
target_link_libraries(render_runtime PUBLIC render_core RED4ext::SDK d3d12)

# The RED4ext plugin: entry points, requests, the status file and finding V.
add_library(BlockRender SHARED
	src/Plugin.cpp
	src/PlayerLocator.cpp
)
target_link_libraries(BlockRender PRIVATE render_runtime)

if(CP2077_DIR)
	set(PLUGIN_OUT "${CP2077_DIR}/red4ext/plugins/BlockRender")
	set(HUD_OUT "${CP2077_DIR}/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud")
	add_custom_command(TARGET BlockRender POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${PLUGIN_OUT}/shaders" "${HUD_OUT}"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:BlockRender>" "$<TARGET_PDB_FILE:BlockRender>" "${PLUGIN_OUT}/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/shaders/blocks.hlsl" "${PLUGIN_OUT}/shaders/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/tools/blockrender_hud/init.lua" "${HUD_OUT}/"
		VERBATIM)
endif()
```

- [x] **Step 6: Build; it must compile without warnings, and the tests still pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C|error LNK" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `83 test(s), 0 failure(s)`.

- [x] **Step 7: Check the DLL's exports and imports**

```bash
DUMPBIN="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/dumpbin.exe"
B=/d/Cyberpunk-Minecraft/spikes/block-render/build/RelWithDebInfo/BlockRender.dll
"$DUMPBIN" -nologo -exports "$B" | grep -E " (Main|Query|Supports) = "; \
  "$DUMPBIN" -nologo -dependents "$B" | grep -iE "d3d|dxgi|sl\."
```
Expected: exactly `Main`, `Query` and `Supports`, then `D3DCOMPILER_47.dll` and `d3d12.dll`. There's no `dxgi.dll`
and no Streamline DLL: the hooks find those at runtime.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: RED4ext plugin, overlay and deploy step (0.1.0)" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 28: Fixes from the first run in the game: frame tokens and the present queue

Task 29's Steps 1 to 4 first ran on 2026-10-01, before this task existed. The plugin loaded 13 seconds before the
game made its device, so the hooks saw it, and GpuApi's device matched theirs, which confirms the SDK's offsets for
2.31. But the main menu's frame log found two problems:
- **Frame tokens.** The game asks Streamline for a frame token about 8 times a frame, from three threads, probably for
  Reflex's markers. The main thread works a frame ahead: it asked for frame 8878 while the render and present threads
  still asked for 8877. The plugin took every call as a new frame, so the log's "two frames" lasted 2 ms, and a dump
  would have missed the DLSS evaluate.
- **The present queue.** Asking the swap chain for its queue (`GetDevice`) fails, so the present event said
  `on queue 0x0`, and step 2's renderer would skip every frame.

The log also showed:
- the projection is for row vectors, with +z forward, and reversed depth (near 0.02, far 16000);
- the game gives `cameraFOV` in degrees (68.24), where Streamline documents radians;
- DLSS doesn't run at the main menu.

The fixes:
- `NewestFrame` starts a frame only when a token comes for a newer frame index than any before, so the listeners hear
  each frame once. The status counts frames and token calls apart.
- The factory hooks note the queue each swap chain is made with, and `PresentQueue` looks it up.
- `frame_summary.py` prints the field of view as given.
- The overlay draws smaller, and splits long lines at "; ", because the user found it a little big.

**Files:**
- Modify: `src/sl/Sl.h`, `src/sl/Sl.cpp`, `tests/TestSl.cpp`
- Modify: `src/hooks/HookEvents.h`, `src/hooks/StreamlineHooks.cpp`, `src/hooks/D3D12Hooks.h`, `src/hooks/D3D12Hooks.cpp`
- Modify: `tools/frame_summary.py`, `tools/blockrender_hud/init.lua`

- [x] **Step 1: Edit `tests/TestSl.cpp` to add the failing test**

```cpp
	CHECK_STR(cpmc::BufferTypeName(sl::kBufferTypeColorAfterDepthOfField), "colorAfterDepthOfField");
	CHECK_STR(cpmc::BufferTypeName(500), "type 500");
}
```
```cpp
	CHECK_STR(cpmc::BufferTypeName(sl::kBufferTypeColorAfterDepthOfField), "colorAfterDepthOfField");
	CHECK_STR(cpmc::BufferTypeName(500), "type 500");
}

TEST(SlNewestFrameStartsEachFrameOnce)
{
	// As the game's main menu logged it: three threads ask for tokens, the main thread a frame ahead of the others.
	cpmc::NewestFrame frames;
	CHECK(frames.Note(8877));
	CHECK(!frames.Note(8877));
	CHECK(frames.Note(8878));
	CHECK(!frames.Note(8877));
	CHECK(!frames.Note(8878));
	CHECK(frames.Note(8880));
	// A far older index: the count started again.
	CHECK(frames.Note(12));
	CHECK(!frames.Note(12));
	CHECK(!frames.Note(11));
	CHECK(frames.Note(13));
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `'NewestFrame': is not a member of 'cpmc'`.

- [x] **Step 3: Edit `src/sl/Sl.h` to declare `NewestFrame`**

```cpp
#include <array>
#include <cstdint>
```
```cpp
#include <array>
#include <atomic>
#include <cstdint>
```
```cpp
std::vector<SlTag> TagsForEvaluate(const std::vector<SlTag>& aGlobal, const std::vector<SlTag>& aLocal);
}
```
```cpp
std::vector<SlTag> TagsForEvaluate(const std::vector<SlTag>& aGlobal, const std::vector<SlTag>& aLocal);

// When a frame starts. The game asks Streamline for a frame token several times a frame, from several threads, and
// the main thread works a frame ahead of the render and present threads. So a frame starts when a token comes for a
// newer frame index than any before. A far older index means the count started again, and starts a frame too.
// Safe from any thread.
class NewestFrame
{
public:
	bool Note(uint32_t aFrame);  // true when aFrame starts a frame

private:
	static constexpr uint64_t kNone = ~0ull;
	std::atomic<uint64_t>     newest_{ kNone };
};
}
```

- [x] **Step 4: Edit `src/sl/Sl.cpp` to implement it**

```cpp
	tags.insert(tags.end(), aLocal.begin(), aLocal.end());
	return tags;
}
}
```
```cpp
	tags.insert(tags.end(), aLocal.begin(), aLocal.end());
	return tags;
}

bool NewestFrame::Note(uint32_t aFrame)
{
	constexpr uint64_t kRestart = 1000;  // frames: far more than any thread works behind the main thread
	uint64_t           newest = newest_.load();
	while (newest == kNone || aFrame > newest || newest - aFrame > kRestart)
	{
		if (newest_.compare_exchange_weak(newest, aFrame))
		{
			return true;
		}
	}
	return false;
}
}
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `84 test(s), 0 failure(s)`.

- [x] **Step 6: Edit `src/hooks/HookEvents.h` to say when a frame starts**

```cpp
	virtual void OnFrameToken(uint32_t /*aFrame*/) {}
```
```cpp
	// A frame starts: Streamline gave a token for a newer frame index than any before (see NewestFrame).
	virtual void OnFrameToken(uint32_t /*aFrame*/) {}
```

- [x] **Step 7: Edit `src/hooks/StreamlineHooks.cpp` to start each frame once**

```cpp
std::atomic<uint64_t>      g_frameTokens{ 0 };
std::atomic<uint64_t>      g_evaluates{ 0 };
```
```cpp
std::atomic<uint64_t>      g_frameTokens{ 0 };  // frames started
std::atomic<uint64_t>      g_tokenCalls{ 0 };   // every slGetNewFrameToken
std::atomic<uint64_t>      g_evaluates{ 0 };
NewestFrame                g_newestFrame;
```
```cpp
	const sl::Result result = g_getNewFrameToken(aToken, aFrameIndex);
	if (result == sl::Result::eOk && aToken)
	{
		const uint32_t frame = FrameOf(*aToken);
		d3d::SetCurrentFrame(frame);
		++g_frameTokens;
		Each([&](HookEvents& aListener) { aListener.OnFrameToken(frame); });
	}
	return result;
```
```cpp
	const sl::Result result = g_getNewFrameToken(aToken, aFrameIndex);
	if (result == sl::Result::eOk && aToken)
	{
		++g_tokenCalls;
		const uint32_t frame = FrameOf(*aToken);
		if (g_newestFrame.Note(frame))
		{
			d3d::SetCurrentFrame(frame);
			++g_frameTokens;
			Each([&](HookEvents& aListener) { aListener.OnFrameToken(frame); });
		}
	}
	return result;
```
```cpp
	return std::format("{} Streamline imports patched, {} frames, {} DLSS evaluates", g_patches.size(), g_frameTokens.load(),
	                   g_evaluates.load());
```
```cpp
	return std::format("{} Streamline imports patched, {} frames ({} token calls), {} DLSS evaluates", g_patches.size(),
	                   g_frameTokens.load(), g_tokenCalls.load(), g_evaluates.load());
```

- [x] **Step 8: Edit `src/hooks/D3D12Hooks.h` to say where the present queue comes from**

```cpp
// The native queue a swap chain presents on, under any Streamline proxy; null if it can't be found.
```
```cpp
// The native queue a swap chain presents on, under any Streamline proxy, as noted when the swap chain was made; null if
// it can't be found.
```

- [x] **Step 9: Edit `src/hooks/D3D12Hooks.cpp` to note each swap chain's queue**

```cpp
#include <format>
#include <mutex>
```
```cpp
#include <format>
#include <map>
#include <mutex>
```
```cpp
std::mutex                   g_patchMutex;  // device, factory and swap-chain patching
```
```cpp
std::mutex                   g_patchMutex;  // device, factory and swap-chain patching
std::mutex                   g_swapChainMutex;
std::map<const void*, ID3D12CommandQueue*> g_swapChainQueues;  // the native queue each swap chain was made with
```
```cpp
void PatchSwapChain(IDXGISwapChain* aSwapChain)
{
```
```cpp
// The native queue under a swap chain's "device", which for D3D12 is the queue it presents on. Null for anything else,
// such as a D3D11 device. No reference is kept: the pointer is only compared.
ID3D12CommandQueue* NativeQueueOf(IUnknown* aDevice)
{
	ComPtr<ID3D12CommandQueue> queue;
	if (!aDevice || FAILED(aDevice->QueryInterface(IID_PPV_ARGS(&queue))))
	{
		return nullptr;
	}
	if (void* native = slhooks::NativeInterface(queue.Get()))
	{
		static_cast<IUnknown*>(native)->Release();  // the proxy holds it
		return static_cast<ID3D12CommandQueue*>(native);
	}
	return queue.Get();  // the swap chain holds it
}

// The game's swap chain doesn't say which queue it presents on (GetDevice fails), so each swap chain's queue is noted
// when it's made.
void NoteSwapChain(IDXGISwapChain* aSwapChain, IUnknown* aDevice, const char* aHow)
{
	ID3D12CommandQueue* const queue = NativeQueueOf(aDevice);
	{
		std::lock_guard lock(g_swapChainMutex);
		g_swapChainQueues[aSwapChain] = queue;
	}
	Log(std::format("swap chain 0x{:x} created ({}), on queue 0x{:x}: Present patched", reinterpret_cast<uintptr_t>(aSwapChain), aHow,
	                reinterpret_cast<uintptr_t>(queue)));
}

void PatchSwapChain(IDXGISwapChain* aSwapChain)
{
```
```cpp
		PatchSwapChain(*aSwapChain);
		Log("swap chain created (CreateSwapChain): Present patched");
```
```cpp
		PatchSwapChain(*aSwapChain);
		NoteSwapChain(*aSwapChain, aDevice, "CreateSwapChain");
```
```cpp
		PatchSwapChain(*aSwapChain);
		Log("swap chain created (CreateSwapChainForHwnd): Present patched");
```
```cpp
		PatchSwapChain(*aSwapChain);
		NoteSwapChain(*aSwapChain, aDevice, "CreateSwapChainForHwnd");
```
```cpp
ID3D12CommandQueue* PresentQueue(IDXGISwapChain* aSwapChain)
{
	ComPtr<ID3D12CommandQueue> queue;
	if (!aSwapChain || FAILED(aSwapChain->GetDevice(IID_PPV_ARGS(&queue))))
	{
		return nullptr;
	}
	if (void* native = slhooks::NativeInterface(queue.Get()))
	{
		static_cast<IUnknown*>(native)->Release();  // the proxy holds it
		return static_cast<ID3D12CommandQueue*>(native);
	}
	return queue.Get();  // the swap chain holds it
}
```
```cpp
ID3D12CommandQueue* PresentQueue(IDXGISwapChain* aSwapChain)
{
	{
		std::lock_guard lock(g_swapChainMutex);
		if (const auto found = g_swapChainQueues.find(aSwapChain); found != g_swapChainQueues.end())
		{
			return found->second;
		}
	}
	// A swap chain made before BlockRender loaded: ask it, though the game's may not say.
	ComPtr<IUnknown> device;
	if (!aSwapChain || FAILED(aSwapChain->GetDevice(IID_PPV_ARGS(&device))))
	{
		return nullptr;
	}
	return NativeQueueOf(device.Get());
}
```

- [x] **Step 10: Build; it must compile without warnings, and the tests still pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C|error LNK" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `84 test(s), 0 failure(s)`.

- [x] **Step 11: Edit `tools/frame_summary.py` to print the field of view as given**

```python
        add(f"  near {c['near']}, far {c['far']}, fov {c['fovDegrees']:.3f} deg, aspect {c['aspect']:.4f}, "
```
```python
        # Streamline documents cameraFOV in radians, but the game gives degrees (68.24 at the main menu): print it as is.
        add(f"  near {c['near']}, far {c['far']}, fov {c['fov']} as given, aspect {c['aspect']:.4f}, "
```

- [x] **Step 12: Edit `tools/blockrender_hud/init.lua` to draw smaller, and split long lines**

```lua
local sinceRead = 1.0

local atan2 = math.atan2 or function(y, x) return math.atan(y, x) end
```
```lua
local sinceRead = 1.0
local fontScale = 0.8    -- the window's text, against CET's own size

local atan2 = math.atan2 or function(y, x) return math.atan(y, x) end

-- A status line as the window shows it: a long line splits at each "; ", with the later parts indented.
local function displayLines(line)
    local out = {}
    for part in string.gmatch(line .. "; ", "(.-); ") do
        table.insert(out, (#out == 0 and "" or "    ") .. part)
    end
    return out
end
```
```lua
    if ImGui.Begin("BlockRender", ImGuiWindowFlags.AlwaysAutoResize) then
        if status == nil then
            ImGui.Text("no status.txt yet: is BlockRender loaded?")
        else
            for _, line in ipairs(status) do
                ImGui.Text(line)
            end
        end
```
```lua
    if ImGui.Begin("BlockRender", ImGuiWindowFlags.AlwaysAutoResize) then
        if ImGui.SetWindowFontScale then
            ImGui.SetWindowFontScale(fontScale)
        end
        if status == nil then
            ImGui.Text("no status.txt yet: is BlockRender loaded?")
        else
            for _, line in ipairs(status) do
                for _, part in ipairs(displayLines(line)) do
                    ImGui.Text(part)
                end
            end
        end
```

- [x] **Step 13: Run all the tools' tests; they must pass**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render/tools && uv run --no-project -q --with numpy --with pillow --with pytest pytest -q 2>&1 | tail -1
```
Expected: `20 passed`.

- [x] **Step 14: Check the overlay's Lua**

```bash
luac -p /d/Cyberpunk-Minecraft/spikes/block-render/tools/blockrender_hud/init.lua && echo "syntax ok"
```
Expected: `syntax ok`. (`luac` is Lua 5.4's; CET runs LuaJIT, and the overlay uses nothing that differs between them.)

- [x] **Step 15: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: start each frame once, and note each swap chain's queue (from the first game run)" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 29: In the game: loading, and step 1 (the frame log)

This tests everything that talks to the game. Steps 1 to 4 first ran before Task 28, which fixed what they found:
after Task 28, run every step again, from Step 1.

The user plays, and the agent sends requests and reads what comes back. The agent reads:
- BlockRender's log, in `E:\Cyberpunk2077\red4ext\logs\blockrender-<time>.log`;
- `status.txt`;
- the frame logs and the dumps.

Note what you see as you go: Step 9 records it. These commands come back again and again:
- **Send a request,** atomically:
  ```bash
  P=/e/Cyberpunk2077/red4ext/plugins/BlockRender; printf '%s\n' "log" > "$P/request.tmp" && mv "$P/request.tmp" "$P/blockrender.request"
  ```
  The plugin runs it within a second. Replace `log` with the command.
- **The log's newest lines:**
  ```bash
  L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); tail -20 "/e/Cyberpunk2077/red4ext/logs/$L"
  ```
- **The status:**
  ```bash
  cat /e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud/status.txt
  ```
- **The newest frame log, summarised:**
  ```bash
  F=$(ls -td /e/Cyberpunk2077/red4ext/plugins/BlockRender/logs/*/ | head -1)
  cd /d/Cyberpunk-Minecraft/spikes/block-render && uv run --no-project -q python tools/frame_summary.py "${F}frame.json"
  ```

**Files:**
- Create: `docs/spikes/2026-10-01-block-render-results.md`

- [x] **Step 1: Deploy (the game must not be running)**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game first, because a running game locks the DLL.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default -DCP2077_DIR=E:/Cyberpunk2077 > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C|error MSB"; \
  "$CMAKE" --preset default -DCP2077_DIR= > /dev/null; \
  ls /e/Cyberpunk2077/red4ext/plugins/BlockRender /e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud \
  && cmp build/RelWithDebInfo/BlockRender.dll /e/Cyberpunk2077/red4ext/plugins/BlockRender/BlockRender.dll && echo installed
```
Expected:
- the plugin's folder holds `BlockRender.dll`, `BlockRender.pdb` and `shaders`;
- the overlay's folder holds `init.lua`;
- then `installed`.

The second configure clears `CP2077_DIR` again.

- [x] **Step 2: Build the atlas into the plugin's folder**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render \
  && uv run --no-project -q --with pillow python tools/build_atlas.py "$APPDATA/.minecraft/versions/26.3/26.3.jar" /e/Cyberpunk2077/red4ext/plugins/BlockRender
```
Expected: `atlas: 256 x 64, 62 sprites, 50 blocks, written to E:/Cyberpunk2077/red4ext/plugins/BlockRender`.

- [x] **Step 3: Loading, at the main menu**

Ask the user to launch Cyberpunk 2077 from Epic, wait at the main menu without loading a save, and say when they're
there. Then read the whole log, and the status:
```bash
L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); cat "/e/Cyberpunk2077/red4ext/logs/$L"; echo ---; \
  cat /e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud/status.txt
```
Expected in the log:
- `Streamline hooked: 4 imports patched`;
- `BlockRender 0.1.0 loaded (game 2.3.1), hooked; dred off, debug layer off; requests go in ...`;
- the device, either way:
  - seen at creation: `device 0x... created on NVIDIA GeForce RTX 3070`, then `device 0x... patched: N vtable entries`;
  - or made before BlockRender loaded (risk 1): `the device was made before BlockRender loaded: patched it through
    GpuApi (...)`;
- the swap chain: `swap chain 0x... created (CreateSwapChainForHwnd), on queue 0x...: Present patched`, where the
  queue is the one the `main queue` line names; or its GpuApi line;
- once DLSS has run: `main queue 0x..., on device 0x..., the one the detour saw`.

In the status:
- `mode=off`;
- `streamline=N frames in the last second`, about the frame rate;
- `hooks=...`, saying `present patched` and `main queue known` once DLSS has run. Its `DLSS evaluates` count grows
  only while DLSS runs, which on the first run it didn't at the menu.

Ask the user two things:
- whether the BlockRender window shows in CET's overlay, and the PhysX probe's still shows;
- whether CET's console still opens.

**If the game crashed or froze,** stop here. Read the log and the newest folder in
`%LOCALAPPDATA%\REDEngine\ReportQueue\`, and record what happened in Step 9.

- [x] **Step 4: A frame log at the main menu**

Send `log`, wait 3 seconds, and read the log's newest lines. Expected: `request: log: armed for the next two frames`,
then `request: log: written to logs\<time>\frame.json (...)`. Then run the summary. Note:
- the device section, especially `matches the detour's`;
- each `present` event's queue, which should end with `(the main queue)`;
- which threads start frames, and how many lists the menu's frames have.

- [x] **Step 5: A frame log in free roam, standing still**

Ask the user four things:
1. Load a save that's in free roam.
2. Stand outside, at a quiet spot.
3. Stand still, and leave the mouse alone.
4. Say when they're there.

Then send `log`, wait 3 seconds, and run the summary. Step 1's pass criteria, from the spec:
- **Patches:** all four Streamline patches are in place, and the hooks saw the game's device.
- **GpuApi:** its device `matches the detour's`. That confirms the SDK's GpuApi offsets for 2.31.
- **The camera:**
  - `angle between the forwards` is under 0.1°;
  - `Streamline minus script position` is under 0.01 m, or an offset (note it).
- **The tags:**
  - depth, motion vectors and scaling input colour are tagged, with extents (or sizes) of the render resolution,
    about 1707×960 at DLSS Quality;
  - the scaling output colour is 2560×1440.
- **The G-buffer:** its pass is among the candidates: three or more targets the size of the tagged depth, with the
  tagged depth bound. Where several share those targets (decals after the G-buffer, say), their order and draw counts
  tell them apart.

Note everything else the spec's "Also recorded" lists:
- the order of the Streamline calls, the G-buffer list's submission, the evaluate and the present, with their threads;
- whether the G-buffer pass and the DLSS evaluate sit at command-list boundaries;
- whether Streamline runs as `proxies` or `manual`;
- the depth convention (`depthInverted`, near and far) and the projection's layout;
- whether `viewToClip` holds the jitter (the jitter terms aren't 0);
- `fov` and `aspect`, against the script camera's vertical 68.24°;
- each tag's flags, lifecycle, and whether it's global or local;
- the barrier API (`barriers` and `enhancedBarriers`) and any render passes;
- every command list, with its name.

Then check the gates for step 2. The renderer relies on each of them:
- **the present queue:** each `present` event ends with `(the main queue)`;
- **the evaluate's list:** each DLSS `evaluate` event says `direct` and `constants found`;
- **the depth tag:** its format is a 32-bit depth (`R32G8X24_TYPELESS`, `D32_FLOAT_S8X24_UINT`, `R32_TYPELESS` or
  `D32_FLOAT`), and it gives a state;
- **legacy barriers only:** `enhancedBarriers` is 0 in the totals.

- [x] **Step 6: The dumps**

**If `enhancedBarriers` wasn't 0,** don't dump: the dumps move resources with legacy barriers. Note it, and go to
Step 7.

Otherwise, with the user still standing still, send `dump all`. Wait 5 seconds, then:
```bash
L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); tail -3 "/e/Cyberpunk2077/red4ext/logs/$L"; \
  D=$(ls -td /e/Cyberpunk2077/red4ext/plugins/BlockRender/dumps/*/ | head -1); cat "${D}dump.json"; \
  cd /d/Cyberpunk-Minecraft/spikes/block-render && uv run --no-project -q --with numpy --with pillow python tools/view_dump.py "$D"
```
Expected:
- `dump: N files written to ...`;
- `dump.json`, listing each file with what it is, and an empty `errors` list (or what went wrong);
- one line per file from the viewer.

Look at each PNG (the Read tool shows images), and ask the user for a screenshot of the same view. Note what each
G-buffer target holds:
- albedo, normals (0.5n + 0.5), material values, motion vectors, or something else;
- for each channel, its range.

Note the stencil values too.

- [x] **Step 7: A frame log while turning**

Ask the user to turn slowly with the mouse, and keep turning until you say. Send `log`, wait 3 seconds, tell the user
they can stop, and run the summary. Note:
- whether each frame's constants follow the turn;
- how far Streamline's camera and the script camera now differ. The script camera is read on the main thread, so a
  small difference is expected while turning;
- whether the order of events matches Step 5's.

- [x] **Step 8: Across a grid line, if Streamline's position was offset**

If Step 5 found Streamline's camera position offset from the script camera's:
1. Ask the user to walk across one of the 1,024 m grid lines. The PhysX probe's overlay shows how far each line is.
2. Ask them to stand still on the far side.
3. Send `log`, and compare the offset with Step 5's. It's either fixed, or follows a grid near V, like PhysX's origin.

If there was no offset, skip this step.

- [x] **Step 9: Record step 1's results, and commit**

Write `docs/spikes/2026-10-01-block-render-results.md` with these sections, filled from the logs, the summaries, the
dumps and the user's screenshots:
- **Verdict for step 1:** pass or fail, in one sentence.
- **Loading:**
  - when BlockRender loaded, against the device;
  - how the device and the swap chain were patched;
  - whether GpuApi's offsets hold for 2.31.
- **The camera:**
  - the projection's layout, and whether it holds the jitter;
  - the depth convention;
  - `fov` and `aspect` against the script camera;
  - any position offset, and whether it's fixed or follows a grid.
- **The tags:** a table of type, format, size, extent, state, lifecycle, and global or local.
- **The frame:**
  - the order of events, and their threads;
  - Streamline's mode;
  - the barrier API and any render passes;
  - the command lists, with their names;
  - whether the G-buffer pass and the evaluate sit at list boundaries.
- **The G-buffer:**
  - the candidates, and which one is the G-buffer pass;
  - what each target holds, from the dumps, with the PNGs or links to their folder;
  - the stencil values.
- **The gates for step 2:** each one, met or not.
- **Surprises,** and what they mean for steps 3 and 4.

Then commit:
```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes \
  && git commit -q -m "block-render: step 1's results" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" \
  && git log --oneline -1
```

---

### Task 30: In the game: step 2 (a cube on the finished frame)

The same game session can carry straight on from Task 29.

**If a gate failed,** stop before this task. The renderer would skip every frame, then switch itself off and say
why. Instead, plan the fix with the user as an amendment to this plan, and come back here afterwards.

The commands are Task 29's.

**Files:**
- Modify: `docs/spikes/2026-10-01-block-render-results.md`

- [x] **Step 1: Set the render origin, if step 1 found an offset**

If Streamline's camera position was offset from the script camera's by a fixed amount, the renderer's render origin
is the script camera's position minus Streamline's. Send each of these, a value at a time, two seconds apart:
`set origin.x <x>`, `set origin.y <y>` and `set origin.z <z>`. The status's `request=` line then says `set origin.x
...` for each.

If there was no offset, skip this step.

- [x] **Step 2: Switch drawing on**

Send `mode present`, wait 3 seconds, then read the log and the status. Expected:
- in the log: `request: mode present`, then `atlas: 256 x 64, 5 levels, 50 blocks`, `renderer: ready` and
  `renderer: swap chain 0x... presents on the main queue`;
- in the status: `mode=present`, and frames drawn. There's nothing to draw yet, but each frame still runs our list.
  A skip or two in the first second is normal, while the first DLSS frame arrives.

If frames go on being skipped, the status says why.

- [x] **Step 3: Place a cube**

Ask the user to face an open patch of ground, a few metres away, stand still, and say when they're there. Send
`place cube`, and read the log's newest lines. Expected: `request: placed cube: 1 blocks, Minecraft x y z to x y z`.
The overlay then shows `nearest structure: cube`, 2 to 3 m away.

Ask the user what they see, and for a screenshot. The cube should be:
- a stone block, 3 m ahead of where V stood;
- sunk partly into the ground (its bottom layer holds V's feet);
- textured like Minecraft's stone;
- with its top brightest and its sides darker.

- [x] **Step 4: Step 2's pass criteria**

Ask the user to:
- walk around the cube;
- turn fast;
- walk so that a wall, a car or a pole is between V and the cube;

then report what they see, with screenshots. Check the spec's criteria:
- **It stays put.** The cube stays on the same spot of ground while V moves and turns. Compare the screenshots with
  the overlay's coordinates.
- **It's hidden correctly.** Anything nearer hides it. Edges are clean, apart from some softness: the depth is at
  render resolution, and the image at output resolution.
- **No lag.** Turning shows no lag.

Read the status while the user moves. Note:
- `frames=` (drawn, skipped, and the last reason);
- `late camera`;
- `gpu=` and `frame=`.

- [x] **Step 5: The sampler, to check the textures**

Ask the user to face open ground again, and say when they're there. Send `place sampler`. Then ask for a
screenshot, close enough to see the textures. Check:
- every block looks like Minecraft's, with grass tops and leaves tinted green;
- glass and leaves are cut out, showing what's behind them;
- each face's brightness matches Minecraft's: top 1.0, north and south 0.8, east and west 0.6.

- [x] **Step 6: The cost**

With the blocks in view, note the status's `gpu=` and `frame=` lines. Then send `mode off`, wait 3 seconds, and note
`frame=` again.

- [x] **Step 7: Clean up**

Send `clear`, then `mode off`, two seconds apart. Expected: `request: cleared`, then `request: mode off`.

- [x] **Step 8: Record step 2's results, and commit**

Add a section to `docs/spikes/2026-10-01-block-render-results.md`:
- **Verdict for step 2:** pass or fail, in one sentence.
- **Each pass criterion,** with what was seen and the screenshots.
- **The sampler:** textures, tints, cut-outs and shading.
- **The numbers:**
  - frames drawn and skipped, and the skip reasons;
  - late-camera counts;
  - our GPU time;
  - the frame time with drawing on and off.
- **Surprises.**

Then commit:
```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes \
  && git commit -q -m "block-render: step 2's results" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" \
  && git log --oneline -1
```

---

### Task 31: Fixes from the second run in the game: front faces, the G-buffer dump, and tracking

Tasks 29 and 30 ran on 2026-10-01. `docs/spikes/2026-10-01-block-render-results.md` has their results. Three things
need fixing before Part 1 can close:
- **Front faces.**
  - **What happened:** the cube showed its inside. The mesher winds faces counter-clockwise seen from outside, as
    Minecraft does, and Direct3D's default takes clockwise triangles as the front faces.
  - **The mistake:** `TestWinding` assumed that flipping y for the render target turns one into the other, but
    flipping a coordinate axis doesn't change how a triangle winds on screen.
  - **The proof:** `set cull 2` showed the solid cube in the game.

  Now `BlockRasterizer` gives the pipeline `FrontCounterClockwise`, and `TestWinding` checks Direct3D's actual rule
  against it. With Direct3D's default put back, 9 checks fail.
- **The G-buffer dump.**
  - **The rule:** the tagged depth is a copy of the game's own, not the G-buffer's depth (`D32_FLOAT_S8X24_UINT`), so
    the candidate rule found nothing. A candidate is now three or more targets and a depth buffer, all at the tagged
    depth's size.
  - **The parallel lists:** the game records the G-buffer pass as 12 lists. Each set of candidate targets now keeps one
    copy, which every pass with those targets records again, so the last in submission order wins: the whole G-buffer.
- **Tracking.** Command list tracking stayed on after the first log, and free roam ran at 30 to 38 fps. Now the frame
  log and the dumps request tracking only while they run. `set tracking 1` holds it on, so its cost can be measured.

The version goes to 0.2.0.

**Files:**
- Create: `src/render/Pipeline.h`, `src/render/Pipeline.cpp`
- Modify: `tests/TestWinding.cpp`, `tests/TestStateTracker.cpp`, `CMakeLists.txt`, `src/render/Renderer.cpp`
- Modify: `src/track/StateTracker.h`, `src/track/StateTracker.cpp`, `src/hooks/D3D12Hooks.h`, `src/hooks/D3D12Hooks.cpp`
- Modify: `src/log/FrameLog.cpp`, `src/Plugin.cpp`
- Rewrite: `src/log/Dumps.h`, `src/log/Dumps.cpp`

- [x] **Step 1: Edit `tests/TestWinding.cpp` to check Direct3D's front-face rule**

```cpp
#include "render/Camera.h"
#include "world/Mesher.h"

// The mesher's faces, through the camera, must wind counter-clockwise in clip space (x right, y up) when they face
// the camera. On screen, with y down, that's clockwise: Direct3D's default front face, so back-face culling keeps them.
```
```cpp
#include "render/Camera.h"
#include "render/Pipeline.h"
#include "world/Mesher.h"

// The mesher's faces, through the camera, wind counter-clockwise in clip space (x right, y up) when they face the
// camera, as Minecraft's do. Flipping y for the render target doesn't change how a triangle winds on screen, so
// Direct3D takes such a triangle as a front face only when the rasterizer says FrontCounterClockwise. Its default is
// the opposite: the first run in the game showed a cube seen from the inside.
```
```cpp
		const double area = SignedArea(ndc[0], ndc[1], ndc[2]);
		if (face == cpmc::Face::South)  // world -y: towards the camera
		{
			CHECK(area > 0.0);
		}
		if (face == cpmc::Face::North)  // away from the camera
		{
			CHECK(area < 0.0);
		}
```
```cpp
		const double area = SignedArea(ndc[0], ndc[1], ndc[2]);
		// Direct3D's rule: a front face winds clockwise on screen, or counter-clockwise with FrontCounterClockwise.
		const bool front = cpmc::BlockRasterizer(1).FrontCounterClockwise ? area > 0.0 : area < 0.0;
		if (face == cpmc::Face::South)  // world -y: towards the camera
		{
			CHECK(area > 0.0);
			CHECK(front);
		}
		if (face == cpmc::Face::North)  // away from the camera
		{
			CHECK(area < 0.0);
			CHECK(!front);
		}
```
```cpp
TEST(WindingFacesTheCameraWithARightHandedColumnProjection)
{
	CheckWinding({ 0.8f, 0, 0, 0, 0, 1.4f, 0, 0, 0, 0, 0, 0.1f, 0, 0, -1, 0 });
}
```
```cpp
TEST(WindingFacesTheCameraWithARightHandedColumnProjection)
{
	CheckWinding({ 0.8f, 0, 0, 0, 0, 1.4f, 0, 0, 0, 0, 0, 0.1f, 0, 0, -1, 0 });
}

TEST(WindingCullSettingsPickTheirFaces)
{
	CHECK(cpmc::BlockRasterizer(0).CullMode == D3D12_CULL_MODE_NONE);
	CHECK(cpmc::BlockRasterizer(1).CullMode == D3D12_CULL_MODE_BACK);
	CHECK(cpmc::BlockRasterizer(2).CullMode == D3D12_CULL_MODE_FRONT);
	CHECK(cpmc::BlockRasterizer(1).FrontCounterClockwise == TRUE);
	CHECK(cpmc::BlockRasterizer(1).DepthClipEnable == TRUE);
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `Cannot open include file: 'render/Pipeline.h'`.

- [x] **Step 3: Write `src/render/Pipeline.h`**

```cpp
#pragma once

#include <d3d12.h>

namespace cpmc
{
// The rasterizer state the blocks draw with. The mesher winds faces counter-clockwise seen from the front, as
// Minecraft does, so counter-clockwise triangles are the front faces: Direct3D's default is the opposite. aCull: 0
// culls nothing, 1 back faces, 2 front faces.
D3D12_RASTERIZER_DESC BlockRasterizer(int aCull);
}
```

- [x] **Step 4: Write `src/render/Pipeline.cpp`**

```cpp
#include "render/Pipeline.h"

namespace cpmc
{
D3D12_RASTERIZER_DESC BlockRasterizer(int aCull)
{
	D3D12_RASTERIZER_DESC desc{};
	desc.FillMode = D3D12_FILL_MODE_SOLID;
	desc.CullMode = aCull == 0 ? D3D12_CULL_MODE_NONE : aCull == 1 ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_FRONT;
	desc.FrontCounterClockwise = TRUE;
	desc.DepthClipEnable = TRUE;
	return desc;
}
}
```

- [x] **Step 5: Edit `CMakeLists.txt` to add the source, and the version**

```cmake
project(BlockRender VERSION 0.1.0 LANGUAGES C CXX)
```
```cmake
project(BlockRender VERSION 0.2.0 LANGUAGES C CXX)
```
```cmake
	src/log/EventNames.cpp
)
```
```cmake
	src/log/EventNames.cpp
	src/render/Pipeline.cpp
)
```

- [x] **Step 6: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `85 test(s), 0 failure(s)`.

- [x] **Step 7: Edit `src/render/Renderer.cpp` to draw with it**

```cpp
#include "render/FrameConstants.h"

#include <dxgi1_4.h>
```
```cpp
#include "render/FrameConstants.h"
#include "render/Pipeline.h"

#include <dxgi1_4.h>
```
```cpp
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = cull_ == 0 ? D3D12_CULL_MODE_NONE : cull_ == 1 ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_FRONT;
	desc.RasterizerState.DepthClipEnable = TRUE;
```
```cpp
	desc.RasterizerState = BlockRasterizer(cull_);
```

- [x] **Step 8: Edit `tests/TestStateTracker.cpp` for the new candidate rule**

```cpp
TEST(GbufferCandidatesNeedTheTaggedDepthAndMatchingTargets)
{
	CHECK(cpmc::IsGbufferCandidate(Gbuffer(9), reinterpret_cast<void*>(9)));
	CHECK(!cpmc::IsGbufferCandidate(Gbuffer(9), reinterpret_cast<void*>(8)));  // another depth buffer (a mirror)
	CHECK(!cpmc::IsGbufferCandidate(Gbuffer(9), nullptr));

	Targets two = Gbuffer(9);
	two.rtvs.pop_back();
	CHECK(!cpmc::IsGbufferCandidate(two, reinterpret_cast<void*>(9)));

	Targets small = Gbuffer(9);
	small.rtvs[1] = Target(2, 853, 480);
	CHECK(!cpmc::IsGbufferCandidate(small, reinterpret_cast<void*>(9)));

	Targets unknown = Gbuffer(9);
	unknown.rtvs[2].view.reset();
	CHECK(!cpmc::IsGbufferCandidate(unknown, reinterpret_cast<void*>(9)));
}
```
```cpp
TEST(GbufferCandidatesNeedThreeTargetsAndADepthAtRenderSize)
{
	CHECK(cpmc::IsGbufferCandidate(Gbuffer(9), 1707, 960));
	CHECK(!cpmc::IsGbufferCandidate(Gbuffer(9), 2560, 1440));  // not the render size
	CHECK(!cpmc::IsGbufferCandidate(Gbuffer(9), 0, 0));        // no depth tag yet

	Targets two = Gbuffer(9);
	two.rtvs.pop_back();
	CHECK(!cpmc::IsGbufferCandidate(two, 1707, 960));

	Targets small = Gbuffer(9);
	small.rtvs[1] = Target(2, 853, 480);
	CHECK(!cpmc::IsGbufferCandidate(small, 1707, 960));

	Targets unknown = Gbuffer(9);
	unknown.rtvs[2].view.reset();
	CHECK(!cpmc::IsGbufferCandidate(unknown, 1707, 960));

	Targets noDepth = Gbuffer(9);
	noDepth.dsv.reset();
	CHECK(!cpmc::IsGbufferCandidate(noDepth, 1707, 960));

	Targets nullTarget = Gbuffer(9);  // the game's forward passes bind a null target, which has no size
	nullTarget.rtvs[1] = Target(0, 0, 0);
	CHECK(!cpmc::IsGbufferCandidate(nullTarget, 1707, 960));
}
```

- [x] **Step 9: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `'cpmc::IsGbufferCandidate': function does not take 3 arguments`.

- [x] **Step 10: Edit `src/track/StateTracker.h` for the new rule**

```cpp
// True when aTargets look like a G-buffer: three or more known render targets the size of the tagged depth, with
// the tagged depth bound as the depth buffer.
bool IsGbufferCandidate(const Targets& aTargets, const void* aTaggedDepth);
```
```cpp
// True when aTargets look like a G-buffer: three or more known render targets and a known depth buffer, all
// aWidth x aHeight, the render size (the tagged depth's). The depth buffer isn't the tagged depth: the game tags a
// copy of its own.
bool IsGbufferCandidate(const Targets& aTargets, uint64_t aWidth, uint32_t aHeight);
```

- [x] **Step 11: Edit `src/track/StateTracker.cpp` for the new rule**

```cpp
bool IsGbufferCandidate(const Targets& aTargets, const void* aTaggedDepth)
{
	if (!aTaggedDepth || !aTargets.dsv || !aTargets.dsv->view || aTargets.dsv->view->resource != aTaggedDepth ||
	    aTargets.rtvs.size() < 3)
	{
		return false;
	}
	const ViewInfo& depth = *aTargets.dsv->view;
	for (const BoundTarget& target : aTargets.rtvs)
	{
		if (!target.view || target.view->width != depth.width || target.view->height != depth.height)
		{
			return false;
		}
	}
	return true;
}
```
```cpp
bool IsGbufferCandidate(const Targets& aTargets, uint64_t aWidth, uint32_t aHeight)
{
	const auto renderSized = [&](const std::optional<ViewInfo>& aView) {
		return aView && aView->width == aWidth && aView->height == aHeight;
	};
	if (aWidth == 0 || aHeight == 0 || !aTargets.dsv || !renderSized(aTargets.dsv->view) || aTargets.rtvs.size() < 3)
	{
		return false;
	}
	for (const BoundTarget& target : aTargets.rtvs)
	{
		if (!renderSized(target.view))
		{
			return false;
		}
	}
	return true;
}
```

- [x] **Step 12: Edit `src/hooks/D3D12Hooks.h` to track only on request**

```cpp
// From now on, command lists are tracked from their next Reset. Off until a feature first needs it.
void EnableTracking();
```
```cpp
// Command lists are tracked from their next Reset while any feature asks for it: a frame log or a dump. Tracking costs
// frame time, so each feature releases its request when it's done.
void RequestTracking();
void ReleaseTracking();
```

- [x] **Step 13: Edit `src/hooks/D3D12Hooks.cpp` to count the requests**

```cpp
std::atomic<bool>            g_tracking{ false };
```
```cpp
std::atomic<bool>            g_tracking{ false };
std::atomic<int>             g_trackingRequests{ 0 };
```
```cpp
void EnableTracking()
{
	g_tracking = true;
}
```
```cpp
void RequestTracking()
{
	g_tracking = ++g_trackingRequests > 0;
}

void ReleaseTracking()
{
	g_tracking = --g_trackingRequests > 0;
}
```

- [x] **Step 14: Edit `src/log/FrameLog.cpp` for tracking and the new rule**

```cpp
	d3d::EnableTracking();
```
```cpp
	d3d::RequestTracking();
```
```cpp
	names_.reset();
	phase_ = Phase::Idle;
```
```cpp
	names_.reset();
	phase_ = Phase::Idle;
	d3d::ReleaseTracking();
```
```cpp
	// The G-buffer candidates, against the log's latest depth tag.
	const void* depth = nullptr;
	for (const LogTag& tag : data.tags)
	{
		if (tag.tag.type == sl::kBufferTypeDepth && tag.tag.resource)
		{
			depth = tag.tag.resource;
		}
	}
```
```cpp
	// The G-buffer candidates, at the render size: the log's latest depth tag's.
	uint64_t width = 0;
	uint32_t height = 0;
	for (const LogTag& tag : data.tags)
	{
		if (tag.tag.type == sl::kBufferTypeDepth && tag.resource.known)
		{
			width = tag.resource.width;
			height = tag.resource.height;
		}
	}
```
```cpp
			if (IsGbufferCandidate(targets, depth))
```
```cpp
			if (IsGbufferCandidate(targets, width, height))
```

- [x] **Step 15: Write `src/log/Dumps.h`**

```cpp
#pragma once

#include "LogQueue.h"
#include "hooks/HookEvents.h"
#include "render/Gpu.h"

#include <chrono>
#include <filesystem>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace cpmc
{
// Dumps (the spec's "FrameLog and dumps"). `dump <what>` arms one frame: from the next frame token, the tagged
// buffers are copied at the DLSS evaluate (depth, mvec, color), and the G-buffer candidates as their passes end
// (gbuffer). The game records its G-buffer pass over many lists, so each set of candidate targets keeps one copy,
// which every pass with those targets records again: the last in submission order wins, which is the whole G-buffer.
// The copies go into readback buffers inside the game's own lists. Once a fence says the GPU has run them, the main
// thread writes each as a .dds file, with dump.json saying what each one is.
class Dumps : public HookEvents
{
public:
	Dumps(LogQueue& aLog, std::filesystem::path aFolder);

	// Main thread.
	bool        Arm(std::string_view aWhat, std::string& aError);
	std::string State() const;
	void        Update(const std::string& aFolderName);  // writes the copies the GPU has finished

	void OnFrameToken(uint32_t aFrame) override;
	void OnTags(uint32_t aFrame, uint32_t aViewport, const std::vector<SlTag>& aTags, void* aList) override;
	void OnEvaluate(const EvaluateCall& aCall) override;
	void OnPassEnd(ID3D12GraphicsCommandList* aList, ListState& aState, bool aClosing) override;
	void OnSubmit(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists) override;
	void OnSubmitted(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists) override;

private:
	enum class Phase
	{
		Idle,
		Armed,     // waiting for the next frame token
		Copying,   // one frame
		Waiting,   // for the GPU
	};

	struct Copy
	{
		std::string                  name;
		std::string                  note;  // what it is, for dump.json
		gpu::ComPtr<ID3D12Resource>  readback;
		gpu::Footprint               footprint;
		uint32_t                     subresource = 0;
		uint32_t                     format = 0;  // the format it's saved as
		uint32_t                     width = 0;
		uint32_t                     height = 0;
		int                          recordings = 0;
		std::set<ID3D12CommandList*> unsubmitted;  // game lists holding a recording of it, not yet submitted
		ID3D12CommandQueue*          queue = nullptr;  // where its latest recording was submitted
		bool                         signalDue = false;  // submitted since the fence was last signalled for it
		uint64_t                     fence = 0;  // signalled after its latest submission
	};

	// One set of G-buffer candidate targets and depth, by resource, and the copies of each resource.
	struct Group
	{
		std::vector<void*>               resources;  // the render targets, then the depth buffer
		std::vector<std::vector<size_t>> copies;     // per resource, its planes' copies (indices into copies_)
		int                              passes = 0;
	};

	// Records copies of every plane of aResource (or of plane aOnlyPlane) into aList, into new readback buffers named
	// aName, and returns their indices in copies_. Mutex held.
	std::vector<size_t> CopyResource(ID3D12GraphicsCommandList* aList, ID3D12Resource* aResource, uint32_t aState,
	                                 const std::string& aName, const std::string& aNote, int aOnlyPlane = -1);
	// Records aCopy's copy of aResource again, into aList. Mutex held.
	void CopyAgain(ID3D12GraphicsCommandList* aList, ID3D12Resource* aResource, uint32_t aState, Copy& aCopy);

	LogQueue&                             log_;
	std::filesystem::path                 folder_;
	mutable std::mutex                    mutex_;
	Phase                                 phase_ = Phase::Idle;
	std::set<std::string>                 what_;
	uint32_t                              frame_ = 0;
	uint64_t                              renderWidth_ = 0;  // the tagged depth's size
	uint32_t                              renderHeight_ = 0;
	std::vector<Group>                    groups_;
	std::vector<Copy>                     copies_;
	gpu::ComPtr<ID3D12Fence>              fence_;
	uint64_t                              fenceValue_ = 0;
	std::vector<std::string>              errors_;
	std::chrono::steady_clock::time_point waitingSince_{};
};
}
```

- [x] **Step 16: Write `src/log/Dumps.cpp`**

```cpp
#include "log/Dumps.h"

#include "Json.h"
#include "hooks/D3D12Hooks.h"
#include "log/DdsWriter.h"
#include "log/FrameLogData.h"

#include <algorithm>
#include <format>
#include <fstream>

namespace cpmc
{
namespace
{
constexpr size_t kMaxGroups = 4;

bool WriteFile(const std::filesystem::path& aPath, const void* aData, size_t aSize)
{
	std::ofstream out(aPath, std::ios::binary);
	out.write(static_cast<const char*>(aData), std::streamsize(aSize));
	return bool(out);
}
}

Dumps::Dumps(LogQueue& aLog, std::filesystem::path aFolder)
	: log_(aLog)
	, folder_(std::move(aFolder))
{
}

bool Dumps::Arm(std::string_view aWhat, std::string& aError)
{
	std::lock_guard lock(mutex_);
	if (phase_ != Phase::Idle)
	{
		aError = "a dump is already under way";
		return false;
	}
	what_.clear();
	if (aWhat == "all")
	{
		what_ = { "depth", "mvec", "color", "gbuffer" };
	}
	else
	{
		what_.insert(std::string(aWhat));
	}
	groups_.clear();
	copies_.clear();
	errors_.clear();
	phase_ = Phase::Armed;
	d3d::RequestTracking();
	return true;
}

std::string Dumps::State() const
{
	std::lock_guard lock(mutex_);
	switch (phase_)
	{
	case Phase::Idle: return "idle";
	case Phase::Armed: return "armed";
	case Phase::Copying: return "copying";
	case Phase::Waiting: return std::format("waiting for the GPU ({} copies)", copies_.size());
	}
	return "";
}

void Dumps::OnFrameToken(uint32_t aFrame)
{
	std::lock_guard lock(mutex_);
	if (phase_ == Phase::Armed)
	{
		phase_ = Phase::Copying;
		frame_ = aFrame;
	}
	else if (phase_ == Phase::Copying)
	{
		phase_ = Phase::Waiting;
		waitingSince_ = std::chrono::steady_clock::now();
	}
}

void Dumps::OnTags(uint32_t, uint32_t, const std::vector<SlTag>& aTags, void*)
{
	for (const SlTag& tag : aTags)
	{
		if (tag.type == sl::kBufferTypeDepth && tag.resource)
		{
			// Kept from every frame: a frame's tags come after the G-buffer passes they give the size of.
			const D3D12_RESOURCE_DESC desc = static_cast<ID3D12Resource*>(tag.resource)->GetDesc();
			std::lock_guard lock(mutex_);
			renderWidth_ = desc.Width;
			renderHeight_ = desc.Height;
		}
	}
}

std::vector<size_t> Dumps::CopyResource(ID3D12GraphicsCommandList* aList, ID3D12Resource* aResource, uint32_t aState,
                                        const std::string& aName, const std::string& aNote, int aOnlyPlane)
{
	std::vector<size_t>       made;
	gpu::ComPtr<ID3D12Device> owner;
	if (!aResource || FAILED(aResource->GetDevice(IID_PPV_ARGS(&owner))))
	{
		return made;
	}
	ID3D12Device* const       device = owner.Get();
	const D3D12_RESOURCE_DESC desc = aResource->GetDesc();
	if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
	{
		errors_.push_back(aName + ": not a 2D texture");
		return made;
	}
	const uint32_t planes = gpu::PlaneCount(device, desc.Format);
	for (uint32_t plane = 0; plane < planes; ++plane)
	{
		if (aOnlyPlane >= 0 && plane != uint32_t(aOnlyPlane))
		{
			continue;
		}
		const uint32_t format = DumpFormat(desc.Format, plane);
		if (BytesPerTexel(format) == 0)
		{
			errors_.push_back(std::format("{}: format {} can't be saved", aName, DxgiFormatName(desc.Format)));
			continue;
		}
		Copy copy;
		copy.name = planes > 1 ? std::format("{}-plane{}", aName, plane) : aName;
		copy.note = aNote;
		copy.subresource = gpu::PlaneSubresource(desc, plane);
		copy.footprint = gpu::FootprintOf(device, desc, copy.subresource);
		copy.readback = gpu::MakeBuffer(device, copy.footprint.totalBytes, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
		if (!copy.readback)
		{
			errors_.push_back(aName + ": couldn't create a readback buffer");
			continue;
		}
		copy.format = format;
		copy.width = copy.footprint.layout.Footprint.Width;
		copy.height = copy.footprint.layout.Footprint.Height;
		copies_.push_back(std::move(copy));
		made.push_back(copies_.size() - 1);
		CopyAgain(aList, aResource, aState, copies_.back());
	}
	return made;
}

void Dumps::CopyAgain(ID3D12GraphicsCommandList* aList, ID3D12Resource* aResource, uint32_t aState, Copy& aCopy)
{
	if (!gpu::RecordCopy(aList, aResource, aCopy.subresource, aState, aCopy.readback.Get(), aCopy.footprint))
	{
		errors_.push_back(std::format("{}: its list isn't a direct list, which can't move it out of state 0x{:x}", aCopy.name, aState));
		return;
	}
	++aCopy.recordings;
	aCopy.unsubmitted.insert(aList);
}

void Dumps::OnEvaluate(const EvaluateCall& aCall)
{
	std::lock_guard lock(mutex_);
	if (phase_ != Phase::Copying || !aCall.list)
	{
		return;
	}
	const std::pair<uint32_t, const char*> wanted[] = { { sl::kBufferTypeDepth, "depth" },
		                                                { sl::kBufferTypeMotionVectors, "mvec" },
		                                                { sl::kBufferTypeScalingInputColor, "color" } };
	for (const auto& [type, what] : wanted)
	{
		if (!what_.contains(what))
		{
			continue;
		}
		for (const SlTag& tag : aCall.tags)
		{
			if (tag.type != type || !tag.resource)
			{
				continue;
			}
			if (tag.state == 0xFFFFFFFF)
			{
				errors_.push_back(BufferTypeName(type) + ": the tag gives no state");
				continue;
			}
			const std::string note = std::format("tag {} at the DLSS evaluate, frame {}, state 0x{:x}, extent {} {} {} {}",
			                                     BufferTypeName(type), aCall.frame, tag.state, tag.extent.top, tag.extent.left,
			                                     tag.extent.width, tag.extent.height);
			CopyResource(aCall.list, static_cast<ID3D12Resource*>(tag.resource), tag.state, "tag-" + BufferTypeName(type), note);
		}
	}
}

void Dumps::OnPassEnd(ID3D12GraphicsCommandList* aList, ListState& aState, bool)
{
	std::lock_guard lock(mutex_);
	const Targets& targets = aState.CurrentTargets();
	if (phase_ != Phase::Copying || !what_.contains("gbuffer") || !IsGbufferCandidate(targets, renderWidth_, renderHeight_))
	{
		return;
	}
	std::vector<void*> resources;
	for (const BoundTarget& target : targets.rtvs)
	{
		resources.push_back(target.view->resource);
	}
	resources.push_back(targets.dsv->view->resource);
	auto group = std::find_if(groups_.begin(), groups_.end(), [&](const Group& aGroup) { return aGroup.resources == resources; });
	if (group == groups_.end())
	{
		if (groups_.size() >= kMaxGroups)
		{
			return;
		}
		groups_.push_back({ resources, std::vector<std::vector<size_t>>(resources.size()), 0 });
		group = groups_.end() - 1;
	}
	const size_t index = size_t(group - groups_.begin());
	++group->passes;
	for (size_t i = 0; i < resources.size(); ++i)
	{
		const bool      isDepth = i == targets.rtvs.size();
		const ViewInfo& view = isDepth ? *targets.dsv->view : *targets.rtvs[i].view;
		auto*           resource = static_cast<ID3D12Resource*>(view.resource);
		const auto      known = aState.StateOf(resource);
		if (isDepth && !known && view.readOnlyDepth)
		{
			continue;  // a read-only depth in an unknown state: its earlier copy stands
		}
		const uint32_t state = known.value_or(isDepth ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET);
		if (group->copies[i].empty())
		{
			const std::string name = isDepth ? std::format("gbuffer{}-depth", index) : std::format("gbuffer{}-target{}", index, i);
			const std::string note = std::format("G-buffer candidate {}, {} ({}), first copied in list 0x{:x}, frame {}, state 0x{:x}",
			                                     index, isDepth ? std::string("depth") : std::format("target {}", i),
			                                     DxgiFormatName(view.viewFormat), reinterpret_cast<uintptr_t>(aList), frame_, state);
			group->copies[i] = CopyResource(aList, resource, state, name, note, isDepth ? -1 : int(view.plane));
		}
		else
		{
			for (const size_t copy : group->copies[i])
			{
				CopyAgain(aList, resource, state, copies_[copy]);
			}
		}
	}
}

void Dumps::OnSubmit(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists)
{
	std::lock_guard lock(mutex_);
	if (phase_ != Phase::Copying && phase_ != Phase::Waiting)
	{
		return;
	}
	for (Copy& copy : copies_)
	{
		for (UINT i = 0; i < aCount; ++i)
		{
			if (copy.unsubmitted.erase(aLists[i]) > 0)
			{
				copy.queue = aQueue;
				copy.signalDue = true;
			}
		}
	}
}

void Dumps::OnSubmitted(ID3D12CommandQueue* aQueue, UINT, ID3D12CommandList* const*)
{
	std::lock_guard lock(mutex_);
	bool            due = false;
	for (const Copy& copy : copies_)
	{
		due = due || (copy.signalDue && copy.queue == aQueue);
	}
	if (!due)
	{
		return;
	}
	gpu::ComPtr<ID3D12Device> device;
	if (!fence_ && (FAILED(aQueue->GetDevice(IID_PPV_ARGS(&device))) ||
	                FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)))))
	{
		errors_.push_back("couldn't create a fence");
		return;
	}
	aQueue->Signal(fence_.Get(), ++fenceValue_);
	for (Copy& copy : copies_)
	{
		if (copy.signalDue && copy.queue == aQueue)
		{
			copy.fence = fenceValue_;
			copy.signalDue = false;
		}
	}
}

void Dumps::Update(const std::string& aFolderName)
{
	std::lock_guard lock(mutex_);
	if (phase_ != Phase::Waiting)
	{
		return;
	}
	// Wait until the GPU has run every copy, or 10 seconds: a list the game never submitted won't finish.
	const uint64_t completed = fence_ ? fence_->GetCompletedValue() : 0;
	const bool     timedOut = std::chrono::steady_clock::now() - waitingSince_ > std::chrono::seconds(10);
	const auto     done = [&](const Copy& aCopy) { return aCopy.unsubmitted.empty() && aCopy.fence != 0 && completed >= aCopy.fence; };
	for (const Copy& copy : copies_)
	{
		if (copy.recordings > 0 && !done(copy) && !timedOut)
		{
			return;
		}
	}
	const std::filesystem::path folder = folder_ / "dumps" / aFolderName;
	std::error_code             error;
	std::filesystem::create_directories(folder, error);
	JsonWriter json;
	json.BeginObject().Key("frame").UInt(frame_).Key("files").BeginArray();
	size_t written = 0;
	for (Copy& copy : copies_)
	{
		if (copy.recordings == 0)
		{
			continue;  // errors_ says why
		}
		if (!done(copy))
		{
			errors_.push_back(copy.name + ": the GPU never ran its copy");
			continue;
		}
		void*             mapped = nullptr;
		const D3D12_RANGE range{ 0, size_t(copy.footprint.totalBytes) };
		if (FAILED(copy.readback->Map(0, &range, &mapped)))
		{
			errors_.push_back(copy.name + ": couldn't map its readback buffer");
			continue;
		}
		const auto* base = static_cast<const uint8_t*>(mapped) + copy.footprint.layout.Offset;
		const auto  dds = MakeDds(copy.format, copy.width, copy.height, base, copy.footprint.layout.Footprint.RowPitch);
		const D3D12_RANGE nothing{ 0, 0 };
		copy.readback->Unmap(0, &nothing);
		if (!WriteFile(folder / (copy.name + ".dds"), dds.data(), dds.size()))
		{
			errors_.push_back(copy.name + ": couldn't write the file");
			continue;
		}
		++written;
		json.BeginObject();
		json.Key("file").String(copy.name + ".dds");
		json.Key("format").String(DxgiFormatName(copy.format));
		json.Key("size").BeginArray().UInt(copy.width).UInt(copy.height).EndArray();
		json.Key("note").String(copy.note);
		json.Key("recordings").UInt(uint64_t(copy.recordings));
		json.EndObject();
	}
	json.EndArray().Key("errors").BeginArray();
	for (const std::string& line : errors_)
	{
		json.String(line);
	}
	json.EndArray().EndObject();
	const std::string text = json.Str() + "\n";
	WriteFile(folder / "dump.json", text.data(), text.size());
	log_.Push(std::format("dump: {} files written to {}{}", written, folder.string(),
	                      errors_.empty() ? "" : std::format(", {} problems (see dump.json)", errors_.size())));
	groups_.clear();
	copies_.clear();
	errors_.clear();
	phase_ = Phase::Idle;
	d3d::ReleaseTracking();
}
}
```

- [x] **Step 17: Edit `src/Plugin.cpp` for `set tracking`, and the version**

```cpp
constexpr const char* kVersion = "0.1.0";
```
```cpp
constexpr const char* kVersion = "0.2.0";
```
```cpp
bool                                       g_hudMissingReported = false;
```
```cpp
bool                                       g_hudMissingReported = false;
bool                                       g_trackingHeld = false;  // set tracking 1: to measure what tracking costs
```
```cpp
		g_world.MarkAllDirty();
		return Report(std::format("set ao {}", g_meshOptions.ambientOcclusion ? 1 : 0));
	}
```
```cpp
		g_world.MarkAllDirty();
		return Report(std::format("set ao {}", g_meshOptions.ambientOcclusion ? 1 : 0));
	}
	if (aRequest.what == "tracking")
	{
		const bool hold = aRequest.number != 0.0;
		if (hold != g_trackingHeld)
		{
			hold ? cpmc::d3d::RequestTracking() : cpmc::d3d::ReleaseTracking();
			g_trackingHeld = hold;
		}
		return Report(std::format("set tracking {}: command lists are {}tracked", hold ? 1 : 0, cpmc::d3d::Tracking() ? "" : "not "));
	}
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 1, 0);
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 2, 0);
```

- [x] **Step 18: Build; it must compile without warnings, and the tests still pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C|error LNK" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `85 test(s), 0 failure(s)`.

- [x] **Step 19: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: front faces wind counter-clockwise, G-buffer dumps by render size, tracking on request (0.2.0)" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 32: In the game: the culling fix, what tracking costs, and the G-buffer dump

A short session. The commands are Task 29's. Stand at the same spot as in Task 29, so the numbers compare.

**Files:**
- Modify: `docs/spikes/2026-10-01-block-render-results.md`

- [x] **Step 1: Deploy (the game must not be running)**

Run Task 29 Step 1's two commands. Expected: `0`, then the plugin's folder with `BlockRender.dll`, `BlockRender.pdb`,
`shaders`, `atlas.bin` and `atlas.json`, the overlay's folder with `init.lua`, and `installed`.

- [x] **Step 2: Load, at the same spot**

Ask the user to launch the game, load the free-roam save, stand where Task 29's logs were taken, and stand still.
Then read the log's first lines and the status. Expected:
- `BlockRender 0.2.0 loaded`;
- in the status, `tracking off`.

- [x] **Step 3: What tracking costs**

With the user standing still and the mouse left alone, read the status's `frame=` line five times, a second apart.
Then send `set tracking 1` and read it five times more. Then send `set tracking 0` and read it five times more:
```bash
ST=/e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud/status.txt; for i in 1 2 3 4 5; do sed -n '5p' $ST; sleep 1.1; done
```
Note the three averages. Their difference is what tracking every command list costs. Step 3 of the spec weighs that
when deciding how much to track.

- [x] **Step 4: A frame log**

Send `log`, wait 3 seconds, and run the summary. Expected:
- the G-buffer candidates now list the G-buffer pass's lists (12 in Task 29's log), the passes that also write
  motion, and the decals;
- the status says `tracking off` again once the log is written.

- [x] **Step 5: The G-buffer dump**

With the user still standing still, send `dump gbuffer`. Wait 5 seconds, then read `dump.json` and view the dump, as
in Task 29 Step 6. Expected:
- `gbuffer0-target0` to `gbuffer0-target2` and `gbuffer0-depth-plane0` and `-plane1`, each with `recordings` about
  the number of G-buffer lists;
- the sets with motion and the decals as `gbuffer1` and `gbuffer2`;
- no errors.

Look at each PNG, and note:
- what each target holds: albedo, normals, material values, motion;
- each channel's range;
- the stencil values.

If a target looks only partly drawn, note which parts: that would mean the copies of different lists overlapped.

- [x] **Step 6: The cube, with the default culling**

Ask the user to face open ground and stand still. Send `mode present`, then `place cube`. With `cull` at its default
(1), the cube must look solid. Ask for a screenshot.

- [x] **Step 7: Clean up**

Send `clear`, then `mode off`, two seconds apart.

- [x] **Step 8: Record the results, and commit**

In `docs/spikes/2026-10-01-block-render-results.md`, add what this run found:
- the culling fix confirmed;
- the tracking costs;
- the G-buffer's encoding, from the dumps.

Then commit:
```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes \
  && git commit -q -m "block-render: the G-buffer dump, and what tracking costs" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 33: The dump viewer's one-channel targets

Task 32's G-buffer dump crashed `view_dump.py` on the second decal set's `R8_UNORM` targets. `to_rgb` pads a
one-channel colour image to two channels instead of three, and Pillow refuses it: `ValueError: not enough image
data`. One channel now shows as grey. Task 32 converted its dump with this fix already made in the scratch clone.

**Files:**
- Modify: `tools/test_view_dump.py`, `tools/view_dump.py`

- [x] **Step 1: Edit `tools/test_view_dump.py` to add a one-channel test**

```python
    vd.convert(depth)
    assert np.asarray(Image.open(tmp_path / "depth.png"))[0, :, 0].tolist() == [0, 255]
```
```python
    vd.convert(depth)
    assert np.asarray(Image.open(tmp_path / "depth.png"))[0, :, 0].tolist() == [0, 255]


def test_one_channel_colour_is_grey(tmp_path):
    # The second decal set's R8_UNORM targets: the third run in the game found them crashing the viewer.
    path = dds(tmp_path / "mask.dds", vd.R8_UNORM, 2, 1, [0, 255])
    assert "R8_UNORM" in vd.convert(path)
    assert np.asarray(Image.open(tmp_path / "mask.png")).tolist() == [[[0, 0, 0], [255, 255, 255]]]
```

- [x] **Step 2: Run it; the new test must fail**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render/tools && uv run --no-project -q --with numpy --with pillow --with pytest pytest -q test_view_dump.py 2>&1 | grep -E "^E |passed|failed"
```
Expected: `ValueError: not enough image data`, then `1 failed, 8 passed`.

- [x] **Step 3: Edit `tools/view_dump.py` to show one channel as grey**

```python
- colour targets as they are (8-bit and 10-bit), HDR ones tone mapped;
```
```python
- colour targets as they are (8-bit and 10-bit, one channel as grey), HDR ones tone mapped;
```
```python
    if kind == "colour":
        rgb = values[:, :, :3] if values.shape[2] >= 3 else np.concatenate([values, np.zeros_like(values[:, :, :1])], axis=-1)
        if rgb.shape[2] == 1:
            rgb = np.repeat(rgb, 3, axis=-1)
        return np.clip(rgb * 255.0 + 0.5, 0, 255).astype(np.uint8)
```
```python
    if kind == "colour":
        channels = values.shape[2]
        if channels >= 3:
            rgb = values[:, :, :3]
        elif channels == 2:
            rgb = np.concatenate([values, np.zeros_like(values[:, :, :1])], axis=-1)
        else:
            rgb = np.repeat(values, 3, axis=-1)  # one channel: grey
        return np.clip(rgb * 255.0 + 0.5, 0, 255).astype(np.uint8)
```

- [x] **Step 4: Run all the tools' tests; they must pass**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render/tools && uv run --no-project -q --with numpy --with pillow --with pytest pytest -q 2>&1 | tail -1
```
Expected: `21 passed`.

- [x] **Step 5: Convert Task 32's dump**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && uv run --no-project -q --with numpy --with pillow python tools/view_dump.py /e/Cyberpunk2077/red4ext/plugins/BlockRender/dumps/2026-10-01_21-37-20 | grep -c "dds: "
```
Expected: `14`. Every file converts, `gbuffer1-target1.dds` and `gbuffer1-target2.dds` (`R8_UNORM`, all zero in that
view) included.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render/tools \
  && git commit -q -m "block-render: the dump viewer shows one-channel targets as grey" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 34: Close Part 1, and plan Part 2

**Files:**
- Modify: `docs/superpowers/plans/2026-10-01-block-render.md` (this plan)

- [x] **Step 1: Add "How Part 1 went" to this plan**

Add a section after Task 34, as spike 1's plan did after its in-game task. Cover:
- what Tasks 29, 30 and 32 found, and Tasks 28, 31 and 33's fixes;
- any amendments made along the way;
- what step 1's findings decide for steps 3 and 4:
  - where blocks can be drawn before DLSS;
  - whether the tagged buffers can be render targets;
  - where the G-buffer pass is, and its encoding;
  - the barrier API.

- [x] **Step 2: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/superpowers/plans \
  && git commit -q -m "block-render plan: how Part 1 went" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" \
  && git log --oneline -1
```

- [x] **Step 3: Plan Part 2**

Use superpowers:writing-plans to write Part 2's tasks (spec steps 3 and 4) from what step 1 found, as Tasks 35 on in
this plan. As in Part 1, compile and test their code in a scratch clone first, and replay the tasks before committing.

### How Part 1 went (2026-10-01)

`docs/spikes/2026-10-01-block-render-results.md` has the results. In short:
- **Tasks 1 to 27** ran as written, offline: 83 C++ tests and 20 Python tests, no warnings, and a DLL exporting
  exactly `Main`, `Query` and `Supports`.
- **Run 1, at the main menu,** found two problems, which Task 28 fixed:
  - the game asks for frame tokens about 8 times a frame, from three threads, so a frame now starts only with a newer
    frame index;
  - the swap chain doesn't say which queue it presents on, so the hooks note it when the swap chain is made.
- **Run 2 (Tasks 29 and 30)** passed step 1, except the G-buffer dump, and passed step 2 once the culling was
  flipped live. Task 31 (0.2.0) fixed:
  - **front faces:** the pipeline takes counter-clockwise triangles as front faces, as Minecraft's meshes wind them;
  - **the G-buffer candidates:** the tagged depth isn't the G-buffer's depth, so they are found by the render size,
    and each set of targets keeps one copy, recorded again after every pass that draws into it;
  - **tracking:** it runs only while a log or dump runs, or on request.
- **Run 3 (Task 32):**
  - it confirmed the culling fix;
  - tracking every command list costs nothing measurable;
  - the G-buffer was dumped and decoded. The viewer crashed on the dump's one-channel targets, which Task 33 fixed.

**Amendments along the way:**
- **New tasks:** Tasks 28 and 31 to 33 were added after the runs, as above.
- **The file blocks:** from Task 28 on, the plan's generator put each file's latest version into every task. So Tasks 1
  to 27 showed code from Tasks 28 and 31 until Task 33 was added. Each task now shows its files as they were when it
  ran, and the whole plan was replayed again, from scratch.
- **The expected build failures** in Tasks 2 to 20 said `fatal error C1083`. MSBuild prints `error C1083`, so they now
  say that. The replay had checked only the header's name until Task 31.

**What step 1's findings decide for steps 3 and 4:**
- **Where blocks can be drawn before DLSS (step 3):** the DLSS evaluate has a short list of its own, the 4th of 9 in
  the frame's last main-queue submission. A list of ours can go in just before it, in the same
  `ExecuteCommandLists`, with nothing recorded into the game's lists. The open question is the state each tagged
  buffer is in at that point. The frame log counts barriers but doesn't keep them, and the DLSS list records 33 to
  45 of its own. Part 2 settles it before drawing there. The fallback is drawing inside the DLSS list, at the
  evaluate, where the tags give the states, followed by `StateTracker::Restore`.
- **Whether the tagged buffers can be render targets:** yes. All three are 1707×960 and can be bound directly:
  - the scaling input colour, `R16G16B16A16_FLOAT`, is a render target and a UAV (state 0x8 at the evaluate);
  - the motion vectors, `R16G16_FLOAT`, are a render target (0xC0);
  - the depth, `R32_TYPELESS` with a D32 view, allows depth-stencil (0xC0).
- **Where the G-buffer pass is (step 4):**
  - **The targets:** `R10G10B10A2_UNORM`, `R10G10B10A2_UNORM` and `R8G8B8A8_UNORM`, with the
    `D32_FLOAT_S8X24_UINT` depth and its stencil.
  - **The lists:** the pass is recorded as 12 parallel lists, sent in 3 main-queue submissions (10, 7 and 2 lists)
    within about 1 ms, long before the DLSS submission. Some passes also draw a fourth target, `R16G16_FLOAT` motion.
  - **The same open question:** what state these targets are in at a list boundary after the pass.
- **Its encoding:**
  - **target 0:** base colour, which looks gamma-encoded;
  - **target 1:** world-space normals as n × 0.5 + 0.5, with z up;
  - **target 2:** material values, likely metalness, roughness, a translucency amount and emission;
  - **the stencil:** a class for each kind of surface, with buildings at 0x00;
  - **the alphas:** 0 on ordinary surfaces in all three targets. Screens, signs and windows set some of them.

  Run 3's results have the details.
- **The barrier API:** legacy `ResourceBarrier` only, and no render passes. So our lists use legacy barriers, and the
  spec's rule for render passes isn't needed.

---

## Part 2: step 3, blocks before DLSS

Tasks 35 to 48, planned after Part 1 closed (see "How Part 1 went"). Run 4 found that the hooks cost a lot even
with drawing off, so Tasks 45 to 47 were added to find and fix that before Part 2 closes.

**Goal:** draw the blocks into the frame before DLSS. DLSS then upscales them, and the game tone maps, blooms and
grades them with the scene, under the HUD and the menus. Then run step 3 in the game with the user.

**How it works:**
- **Our list goes into the game's submission,** just before the DLSS evaluate's list (Task 36). So the GPU runs it
  after everything the scene draws before DLSS, and nothing is recorded into the game's lists. The spec's
  `StateTracker::Restore` isn't needed.
- **Its barriers start where the game left each tagged buffer:** at the state before the DLSS list's first
  transition of it, which the tracker now keeps (Tasks 35 and 37). The frame log records the same states, so the game
  run shows them before anything is drawn (Task 41).
- **It draws into the tagged colour, motion vectors and depth** (Tasks 38, 39 and 42):
  - the colour is the atlas's, made linear, times `exposure`;
  - the vertices move with the frame's jitter;
  - the motion vectors come from Streamline's `clipToPrevClip`.
- **The game run settles three values live** (Task 44), as Part 1 settled the culling: `exposure`, `jittersign` and
  `mvsign`.
- **Part 3 (step 4, the G-buffer) is planned after the run,** because it reuses this part's machinery, which the game
  should test first.

**Already verified:** as in Part 1, all the code was compiled and tested in the scratch clone first. Tasks 35 to 43
were then replayed in a fresh clone from the end of Part 1, and Tasks 45 and 46 from the end of Task 44, applying
every step as written and checking every expected result:
- zero compiler warnings;
- 104 C++ tests and 22 Python tests passing;
- the DLL exporting exactly `Main`, `Query` and `Supports`;
- the replayed files identical to the scratch clone's.

**Test counts:** the C++ count goes 86, 88, 93, 96, 97, 100, 101 from Task 41, 103 from Task 45, then 104 from Task
46 on. The Python count is 22 from Task 41 on.

**The commands and rules** in "Before you start" apply here too: the game and the user, deploying, and no faults.

---

### Task 35: The tracker keeps each resource's first transition in a list

Step 3 puts a list of ours just before the game's DLSS list. Our barriers must start from the state each tagged buffer
is in at that point: the state before the DLSS list's first transition of it. So `ListState` now keeps each
resource's first transition, with its state before, its subresource and its flags. The hook passes the whole barrier.

**Files:**
- Modify: `tests/TestStateTracker.cpp`, `src/track/StateTracker.h`, `src/track/StateTracker.cpp`,
  `src/hooks/D3D12Hooks.cpp`

- [x] **Step 1: Edit `tests/TestStateTracker.cpp` to test first transitions**

```cpp
TEST(ListStateFollowsTransitionsAndSplitBarriers)
{
	ListState state;
	state.Begin(1, false);
	int resource = 0;
	CHECK(!state.StateOf(&resource));
	state.Transition(&resource, 0x4, false);  // render target
	CHECK_EQ(*state.StateOf(&resource), 0x4u);
	state.Transition(&resource, 0x800, true);  // begun, not ended
	CHECK(!state.StateOf(&resource));
	state.Transition(&resource, 0x800, false);  // ended
	CHECK_EQ(*state.StateOf(&resource), 0x800u);
	state.Begin(2, false);
	CHECK(!state.StateOf(&resource));  // a new recording forgets
}
```
```cpp
TEST(ListStateFollowsTransitionsAndSplitBarriers)
{
	constexpr uint32_t all = ListState::kAllSubresources;
	ListState state;
	state.Begin(1, false);
	int resource = 0;
	CHECK(!state.StateOf(&resource));
	state.Transition(&resource, 0x0, 0x4, all, 0);  // common to render target
	CHECK_EQ(*state.StateOf(&resource), 0x4u);
	state.Transition(&resource, 0x4, 0x800, all, ListState::kBeginOnly);  // begun, not ended
	CHECK(!state.StateOf(&resource));
	state.Transition(&resource, 0x4, 0x800, all, ListState::kEndOnly);  // ended
	CHECK_EQ(*state.StateOf(&resource), 0x800u);
	state.Transition(&resource, 0x800, 0x8, 0, 0);  // one subresource: the whole resource's state is unknown
	CHECK(!state.StateOf(&resource));
	state.Begin(2, false);
	CHECK(!state.StateOf(&resource));  // a new recording forgets
}

TEST(ListStateKeepsEachResourcesFirstTransition)
{
	constexpr uint32_t all = ListState::kAllSubresources;
	ListState state;
	state.Begin(1, false);
	int colour = 0, depth = 0, untouched = 0;
	state.Transition(&colour, 0x4, 0x8, all, 0);   // render target to unordered access
	state.Transition(&colour, 0x8, 0xC0, all, 0);  // a later transition doesn't change the first
	state.Transition(&depth, 0x10, 0xC0, 0, ListState::kBeginOnly);
	const auto colourFirst = state.FirstTransitionOf(&colour);
	CHECK(colourFirst.has_value());
	CHECK_EQ(colourFirst->before, 0x4u);
	CHECK_EQ(colourFirst->subresource, all);
	CHECK_EQ(colourFirst->flags, 0u);
	const auto depthFirst = state.FirstTransitionOf(&depth);
	CHECK(depthFirst.has_value());
	CHECK_EQ(depthFirst->before, 0x10u);
	CHECK_EQ(depthFirst->subresource, 0u);
	CHECK_EQ(depthFirst->flags, ListState::kBeginOnly);
	CHECK(!state.FirstTransitionOf(&untouched));
	CHECK_EQ(state.FirstTransitions().size(), size_t(2));
	state.Begin(2, false);
	CHECK(!state.FirstTransitionOf(&colour));  // a new recording forgets
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `'kAllSubresources': is not a member of 'cpmc::ListState'`.

- [x] **Step 3: Edit `src/track/StateTracker.h` for first transitions**

```cpp
	// A transition barrier. A begin-only half leaves the state unknown until its end-only half.
	void Transition(void* aResource, uint32_t aAfter, bool aBeginOnly);
	std::optional<uint32_t> StateOf(void* aResource) const;
```
```cpp
	// D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, and the two D3D12_RESOURCE_BARRIER_FLAGS.
	static constexpr uint32_t kAllSubresources = 0xFFFFFFFF;
	static constexpr uint32_t kBeginOnly = 0x1;
	static constexpr uint32_t kEndOnly = 0x2;

	// A resource's first transition in the list. Its state before is the state the list found the resource in.
	struct FirstTransition
	{
		uint32_t before = 0;
		uint32_t subresource = kAllSubresources;
		uint32_t flags = 0;
	};

	// A transition barrier. Afterwards the resource's state is known, unless the barrier is a begin-only half (until
	// its end-only half) or covers a single subresource.
	void Transition(void* aResource, uint32_t aBefore, uint32_t aAfter, uint32_t aSubresource, uint32_t aFlags);
	std::optional<uint32_t> StateOf(void* aResource) const;
	std::optional<FirstTransition> FirstTransitionOf(void* aResource) const;
	const std::unordered_map<void*, FirstTransition>& FirstTransitions() const { return firsts_; }
```
```cpp
	bool                                   recording_ = false;
	bool                                   logging_ = false;
	uint32_t                               frame_ = 0;
	Targets                                targets_;
	std::unordered_map<void*, uint32_t>    states_;
	std::vector<PassRecord>                passes_;
```
```cpp
	bool                                       recording_ = false;
	bool                                       logging_ = false;
	uint32_t                                   frame_ = 0;
	Targets                                    targets_;
	std::unordered_map<void*, uint32_t>        states_;
	std::unordered_map<void*, FirstTransition> firsts_;
	std::vector<PassRecord>                    passes_;
```

- [x] **Step 4: Edit `src/track/StateTracker.cpp` for first transitions**

```cpp
	states_.clear();
	passes_.clear();
}
```
```cpp
	states_.clear();
	firsts_.clear();
	passes_.clear();
}
```
```cpp
void ListState::Transition(void* aResource, uint32_t aAfter, bool aBeginOnly)
{
	states_[aResource] = aBeginOnly ? kUnknownState : aAfter;
}
```
```cpp
void ListState::Transition(void* aResource, uint32_t aBefore, uint32_t aAfter, uint32_t aSubresource, uint32_t aFlags)
{
	firsts_.try_emplace(aResource, FirstTransition{ aBefore, aSubresource, aFlags });
	const bool known = aSubresource == kAllSubresources && (aFlags & kBeginOnly) == 0;
	states_[aResource] = known ? aAfter : kUnknownState;
}
```
```cpp
	return found->second;
}

StateTracker::StateTracker()
```
```cpp
	return found->second;
}

std::optional<ListState::FirstTransition> ListState::FirstTransitionOf(void* aResource) const
{
	const auto found = firsts_.find(aResource);
	if (found == firsts_.end())
	{
		return std::nullopt;
	}
	return found->second;
}

StateTracker::StateTracker()
```

- [x] **Step 5: Edit `src/hooks/D3D12Hooks.cpp` to pass the whole barrier**

```cpp
			// Only whole-resource transitions say what state the whole resource is in.
			const bool whole = barrier.Transition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			const bool beginOnly = (barrier.Flags & D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY) != 0;
			state->Transition(barrier.Transition.pResource, barrier.Transition.StateAfter, beginOnly || !whole);
```
```cpp
			state->Transition(barrier.Transition.pResource, barrier.Transition.StateBefore, barrier.Transition.StateAfter,
			                  barrier.Transition.Subresource, barrier.Flags);
```

- [x] **Step 6: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `86 test(s), 0 failure(s)`.

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: the tracker keeps each resource's first transition in a list" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 36: Lists of ours in the game's submissions

Step 3's list goes into the game's own `ExecuteCommandLists` call, just before the DLSS list, so the GPU runs it after
everything that list's submission puts ahead of it, and nothing of ours is recorded into the game's lists. The hook now
asks each listener, for each list submitted, whether it has a list to put before that one.

**Files:**
- Create: `src/hooks/Submission.h`, `src/hooks/Submission.cpp`, `tests/TestSubmission.cpp`
- Modify: `CMakeLists.txt`, `src/hooks/HookEvents.h`, `src/hooks/D3D12Hooks.cpp`

- [x] **Step 1: Write the failing test, `tests/TestSubmission.cpp`**

```cpp
#include "Harness.h"

#include "hooks/Submission.h"

namespace
{
// Lists are only compared here, never called, so any address will do.
ID3D12CommandList* Fake(uintptr_t aAddress)
{
	return reinterpret_cast<ID3D12CommandList*>(aAddress);
}
}

TEST(SubmissionsGoThroughUntouchedWithNothingToPutIn)
{
	ID3D12CommandList* lists[] = { Fake(0x10), Fake(0x20), Fake(0x30) };
	CHECK(cpmc::WithListsBefore(3, lists, [](ID3D12CommandList*) -> ID3D12CommandList* { return nullptr; }).empty());
	CHECK(cpmc::WithListsBefore(0, nullptr, [](ID3D12CommandList*) { return Fake(0x99); }).empty());
}

TEST(SubmissionsGetAListJustBeforeTheOneItGoesBefore)
{
	ID3D12CommandList* lists[] = { Fake(0x10), Fake(0x20), Fake(0x30), Fake(0x40) };
	// Ours goes before the third, as step 3's goes before the DLSS evaluate's list.
	const auto withOurs = cpmc::WithListsBefore(4, lists, [](ID3D12CommandList* aList) {
		return aList == Fake(0x30) ? Fake(0x99) : nullptr;
	});
	CHECK(withOurs == (std::vector<ID3D12CommandList*>{ Fake(0x10), Fake(0x20), Fake(0x99), Fake(0x30), Fake(0x40) }));
	// Before the first, and before more than one.
	const auto twice = cpmc::WithListsBefore(4, lists, [](ID3D12CommandList* aList) {
		return aList == Fake(0x10) || aList == Fake(0x40) ? Fake(0x99) : nullptr;
	});
	CHECK(twice == (std::vector<ID3D12CommandList*>{ Fake(0x99), Fake(0x10), Fake(0x20), Fake(0x30), Fake(0x99), Fake(0x40) }));
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestEventNames.cpp
)
```
```cmake
	tests/TestEventNames.cpp
	tests/TestSubmission.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'hooks/Submission.h'`.

- [x] **Step 4: Write `src/hooks/Submission.h`**

```cpp
#pragma once

#include <d3d12.h>

#include <functional>
#include <vector>

namespace cpmc
{
// A submission's command lists with lists of ours put in: aBefore(list) names one to go just before that list, or
// gives null. Empty when nothing is put in, so the submission can go through untouched.
std::vector<ID3D12CommandList*> WithListsBefore(UINT aCount, ID3D12CommandList* const* aLists,
                                                const std::function<ID3D12CommandList*(ID3D12CommandList*)>& aBefore);
}
```

- [x] **Step 5: Write `src/hooks/Submission.cpp`**

```cpp
#include "hooks/Submission.h"

namespace cpmc
{
std::vector<ID3D12CommandList*> WithListsBefore(UINT aCount, ID3D12CommandList* const* aLists,
                                                const std::function<ID3D12CommandList*(ID3D12CommandList*)>& aBefore)
{
	std::vector<ID3D12CommandList*> lists;
	for (UINT i = 0; i < aCount; ++i)
	{
		if (ID3D12CommandList* before = aBefore(aLists[i]))
		{
			if (lists.empty())
			{
				lists.assign(aLists, aLists + i);  // the lists ahead of it, as they were
			}
			lists.push_back(before);
		}
		if (!lists.empty())
		{
			lists.push_back(aLists[i]);
		}
	}
	return lists;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/render/Pipeline.cpp
)
```
```cmake
	src/render/Pipeline.cpp
	src/hooks/Submission.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `88 test(s), 0 failure(s)`.

- [x] **Step 8: Edit `src/hooks/HookEvents.h` to ask for lists to put in**

```cpp
	virtual void OnSubmit(ID3D12CommandQueue* /*aQueue*/, UINT /*aCount*/, ID3D12CommandList* const* /*aLists*/) {}
```
```cpp
	virtual void OnSubmit(ID3D12CommandQueue* /*aQueue*/, UINT /*aCount*/, ID3D12CommandList* const* /*aLists*/) {}
	// Asked for each list of a submission, after OnSubmit: a list of the feature's own to submit just before aList, or
	// null. The first listener to name one wins. OnSubmitted sees only the game's lists, as OnSubmit does.
	virtual ID3D12CommandList* ListBefore(ID3D12CommandQueue* /*aQueue*/, ID3D12CommandList* /*aList*/) { return nullptr; }
```

- [x] **Step 9: Edit `src/hooks/D3D12Hooks.cpp` to put them in**

```cpp
#include "hooks/StreamlineHooks.h"
#include "hooks/VtablePatch.h"
```
```cpp
#include "hooks/StreamlineHooks.h"
#include "hooks/Submission.h"
#include "hooks/VtablePatch.h"
```
```cpp
	Each([&](HookEvents& aListener) { aListener.OnSubmit(aQueue, aCount, aLists); });
	g_original.executeCommandLists(aQueue, aCount, aLists);
	Each([&](HookEvents& aListener) { aListener.OnSubmitted(aQueue, aCount, aLists); });
```
```cpp
	Each([&](HookEvents& aListener) { aListener.OnSubmit(aQueue, aCount, aLists); });
	const std::vector<ID3D12CommandList*> lists = WithListsBefore(aCount, aLists, [&](ID3D12CommandList* aList) {
		ID3D12CommandList* before = nullptr;
		Each([&](HookEvents& aListener) {
			if (!before)
			{
				before = aListener.ListBefore(aQueue, aList);
			}
		});
		return before;
	});
	if (lists.empty())
	{
		g_original.executeCommandLists(aQueue, aCount, aLists);
	}
	else
	{
		g_original.executeCommandLists(aQueue, UINT(lists.size()), lists.data());
	}
	Each([&](HookEvents& aListener) { aListener.OnSubmitted(aQueue, aCount, aLists); });
```

- [x] **Step 10: Build; it must compile without warnings, and the tests still pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `88 test(s), 0 failure(s)`.

- [x] **Step 11: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: lists of ours in the game's submissions" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 37: Drawing before DLSS: the checks, the starting states and the barriers

The parts of step 3's drawing that need no GPU:
- **The checks before every draw:** the tagged buffers must be as step 1 found them.
- **The starting states:** the state each tagged buffer is in just before the DLSS list, from that list's first
  transition of it (Task 35), or its tagged state if the list never moves it.
- **The barriers:** the moves into render target and depth write, leaving out a buffer already there, and back.

**Files:**
- Create: `src/render/Predlss.h`, `src/render/Predlss.cpp`, `tests/TestPredlss.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestPredlss.cpp`**

```cpp
#include "Harness.h"

#include "render/Predlss.h"

#include <string>

using cpmc::ListState;
using cpmc::Move;

namespace
{
D3D12_RESOURCE_DESC Texture(DXGI_FORMAT aFormat, D3D12_RESOURCE_FLAGS aFlags)
{
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = 1707;
	desc.Height = 960;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = aFormat;
	desc.SampleDesc.Count = 1;
	desc.Flags = aFlags;
	return desc;
}

// The tags step 1 found.
const D3D12_RESOURCE_DESC kColour = Texture(DXGI_FORMAT_R16G16B16A16_FLOAT,
                                            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
const D3D12_RESOURCE_DESC kMotion = Texture(DXGI_FORMAT_R16G16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
const D3D12_RESOURCE_DESC kDepth = Texture(DXGI_FORMAT_R32_TYPELESS, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

bool Refused(const D3D12_RESOURCE_DESC& aColour, const D3D12_RESOURCE_DESC& aMotion, const D3D12_RESOURCE_DESC& aDepth,
             const char* aWhy)
{
	std::string why;
	return !cpmc::CheckPredlssTargets(aColour, aMotion, aDepth, why) && why.find(aWhy) != std::string::npos;
}
}

TEST(PredlssTakesTheTagsStep1Found)
{
	std::string why;
	CHECK(cpmc::CheckPredlssTargets(kColour, kMotion, kDepth, why));
	CHECK_STR(why, "");
	D3D12_RESOURCE_DESC typedDepth = kDepth;
	typedDepth.Format = DXGI_FORMAT_D32_FLOAT;
	CHECK(cpmc::CheckPredlssTargets(kColour, kMotion, typedDepth, why));
}

TEST(PredlssRefusesOtherTargets)
{
	D3D12_RESOURCE_DESC computeOnly = kColour;
	computeOnly.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	CHECK(Refused(computeOnly, kMotion, kDepth, "the colour isn't a render target"));
	D3D12_RESOURCE_DESC otherFormat = kMotion;
	otherFormat.Format = DXGI_FORMAT_R32G32_FLOAT;
	CHECK(Refused(kColour, otherFormat, kDepth, "the motion vectors are R32G32_FLOAT"));
	D3D12_RESOURCE_DESC noDepth = kDepth;
	noDepth.Flags = D3D12_RESOURCE_FLAG_NONE;
	CHECK(Refused(kColour, kMotion, noDepth, "the depth isn't a depth buffer"));
	D3D12_RESOURCE_DESC smaller = kDepth;
	smaller.Width = 853;
	CHECK(Refused(kColour, kMotion, smaller, "sizes differ"));
	D3D12_RESOURCE_DESC mipped = kColour;
	mipped.MipLevels = 2;
	CHECK(Refused(mipped, kMotion, kDepth, "the colour has more than one subresource"));
}

TEST(PredlssViewsTheDepthAsD32)
{
	CHECK(cpmc::DepthViewFormat(DXGI_FORMAT_R32_TYPELESS) == DXGI_FORMAT_D32_FLOAT);
	CHECK(cpmc::DepthViewFormat(DXGI_FORMAT_D32_FLOAT) == DXGI_FORMAT_D32_FLOAT);
	CHECK(cpmc::DepthViewFormat(DXGI_FORMAT_R32G8X24_TYPELESS) == DXGI_FORMAT_UNKNOWN);
}

TEST(PredlssFindsEachBuffersStateBeforeTheDlssList)
{
	constexpr uint32_t all = ListState::kAllSubresources;
	ListState list;
	list.Begin(1, false);
	int colour = 0, depth = 0, motion = 0, part = 0, split = 0;
	list.Transition(&colour, 0x4, 0x8, all, 0);  // render target, to unordered access for the evaluate
	list.Transition(&depth, 0x10, 0xC0, 0, 0);   // subresource 0, the only one a tagged buffer has
	list.Transition(&part, 0x4, 0x8, 1, 0);      // another subresource only
	list.Transition(&split, 0x4, 0x8, all, ListState::kEndOnly);  // ends a split begun in an earlier list
	CHECK_EQ(cpmc::StateBeforeList(list, &colour, 0x8).value_or(0), 0x4u);
	CHECK_EQ(cpmc::StateBeforeList(list, &depth, 0xC0).value_or(0), 0x10u);
	CHECK_EQ(cpmc::StateBeforeList(list, &motion, 0xC0).value_or(0), 0xC0u);  // never moved: as tagged
	CHECK(!cpmc::StateBeforeList(list, &motion, 0xFFFFFFFF));               // never moved, and the tag didn't say
	CHECK(!cpmc::StateBeforeList(list, &part, 0x8));
	CHECK(!cpmc::StateBeforeList(list, &split, 0x8));
}

TEST(PredlssMovesOnlyWhatIsntThereAndPutsItBack)
{
	int colour = 0, motion = 0, depth = 0;
	const std::vector<Move> into = cpmc::MovesInto({ { &colour, 0x8, 0x4 }, { &motion, 0x4, 0x4 }, { &depth, 0xC0, 0x10 } });
	CHECK_EQ(into.size(), size_t(2));  // the motion vectors are already a render target
	CHECK(into[0].resource == &colour && into[0].before == 0x8 && into[0].after == 0x4);
	CHECK(into[1].resource == &depth && into[1].before == 0xC0 && into[1].after == 0x10);
	const std::vector<Move> back = cpmc::MovesBack(into);
	CHECK_EQ(back.size(), size_t(2));
	CHECK(back[0].resource == &depth && back[0].before == 0x10 && back[0].after == 0xC0);
	CHECK(back[1].resource == &colour && back[1].before == 0x4 && back[1].after == 0x8);
	const std::vector<D3D12_RESOURCE_BARRIER> barriers = cpmc::Barriers(into);
	CHECK_EQ(barriers.size(), size_t(2));
	CHECK(barriers[1].Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);
	CHECK(barriers[1].Transition.pResource == reinterpret_cast<ID3D12Resource*>(&depth));
	CHECK(barriers[1].Transition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	CHECK(barriers[1].Transition.StateBefore == D3D12_RESOURCE_STATES(0xC0));
	CHECK(barriers[1].Transition.StateAfter == D3D12_RESOURCE_STATE_DEPTH_WRITE);
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestSubmission.cpp
)
```
```cmake
	tests/TestSubmission.cpp
	tests/TestPredlss.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'render/Predlss.h'`.

- [x] **Step 4: Write `src/render/Predlss.h`**

```cpp
#pragma once

#include "track/StateTracker.h"

#include <d3d12.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Drawing before DLSS (step 3): the parts that need no GPU. Our list goes just before the game's DLSS list, and draws
// into the tagged scaling input colour, motion vectors and depth.
namespace cpmc
{
// The checks before every draw (the spec's "Safety"): the colour must be an R16G16B16A16_FLOAT render target, the
// motion vectors an R16G16_FLOAT render target, and the depth a 32-bit depth buffer, as step 1 found them. All three
// must be the same size, and plain 2D textures with one subresource each. aWhy says what failed.
bool CheckPredlssTargets(const D3D12_RESOURCE_DESC& aColour, const D3D12_RESOURCE_DESC& aMotion, const D3D12_RESOURCE_DESC& aDepth,
                         std::string& aWhy);

// The format of a depth-stencil view of a depth buffer in aFormat: D32_FLOAT for R32_TYPELESS or D32_FLOAT, UNKNOWN for
// anything else.
DXGI_FORMAT DepthViewFormat(DXGI_FORMAT aFormat);

// A tagged buffer's state just before aList, the DLSS list: the state its first transition in aList starts from, or,
// if aList never moves it, its state at the evaluate (aTaggedState). Nothing if that state is unknown, or if the
// first transition ends a split barrier or covers another subresource than the buffer's only one.
std::optional<uint32_t> StateBeforeList(const ListState& aList, void* aResource, uint32_t aTaggedState);

// A resource moving from one state to another.
struct Move
{
	void*    resource = nullptr;
	uint32_t before = 0;
	uint32_t after = 0;
};

// The moves that take each resource to the state wanted, leaving out those already in it: D3D12 refuses a transition
// to the state a resource is in.
std::vector<Move> MovesInto(const std::vector<Move>& aWanted);
// The moves that put them back, in reverse order.
std::vector<Move> MovesBack(const std::vector<Move>& aMoves);
// Whole-resource transition barriers for aMoves.
std::vector<D3D12_RESOURCE_BARRIER> Barriers(const std::vector<Move>& aMoves);
}
```

- [x] **Step 5: Write `src/render/Predlss.cpp`**

```cpp
#include "render/Predlss.h"

#include "log/FrameLogData.h"

#include <format>

namespace cpmc
{
namespace
{
// Why aDesc isn't a plain 2D texture in aFormat that allows aFlag, or nothing if it is. aName takes aVerb: "is" or
// "are".
std::string Problem(const char* aName, const char* aVerb, const D3D12_RESOURCE_DESC& aDesc, DXGI_FORMAT aFormat,
                    D3D12_RESOURCE_FLAGS aFlag, const char* aRole)
{
	const std::string_view verb = aVerb;
	if (aDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
	{
		return std::format("the {} {}n't a 2D texture", aName, verb);
	}
	if (aDesc.Format != aFormat && !(aFormat == DXGI_FORMAT_R32_TYPELESS && aDesc.Format == DXGI_FORMAT_D32_FLOAT))
	{
		return std::format("the {} {} {}", aName, verb, DxgiFormatName(aDesc.Format));
	}
	if ((aDesc.Flags & aFlag) == 0)
	{
		return std::format("the {} {}n't {}", aName, verb, aRole);
	}
	if (aDesc.MipLevels != 1 || aDesc.DepthOrArraySize != 1 || aDesc.SampleDesc.Count != 1)
	{
		return std::format("the {} {} more than one subresource", aName, verb == "is" ? "has" : "have");
	}
	return {};
}
}

bool CheckPredlssTargets(const D3D12_RESOURCE_DESC& aColour, const D3D12_RESOURCE_DESC& aMotion, const D3D12_RESOURCE_DESC& aDepth,
                         std::string& aWhy)
{
	aWhy = Problem("colour", "is", aColour, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, "a render target");
	if (aWhy.empty())
	{
		aWhy = Problem("motion vectors", "are", aMotion, DXGI_FORMAT_R16G16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		               "a render target");
	}
	if (aWhy.empty())
	{
		aWhy = Problem("depth", "is", aDepth, DXGI_FORMAT_R32_TYPELESS, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, "a depth buffer");
	}
	if (aWhy.empty() && (aMotion.Width != aColour.Width || aMotion.Height != aColour.Height || aDepth.Width != aColour.Width ||
	                     aDepth.Height != aColour.Height))
	{
		aWhy = std::format("the tagged buffers' sizes differ: colour {} x {}, motion vectors {} x {}, depth {} x {}", aColour.Width,
		                   aColour.Height, aMotion.Width, aMotion.Height, aDepth.Width, aDepth.Height);
	}
	return aWhy.empty();
}

DXGI_FORMAT DepthViewFormat(DXGI_FORMAT aFormat)
{
	return aFormat == DXGI_FORMAT_R32_TYPELESS || aFormat == DXGI_FORMAT_D32_FLOAT ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_UNKNOWN;
}

std::optional<uint32_t> StateBeforeList(const ListState& aList, void* aResource, uint32_t aTaggedState)
{
	const std::optional<ListState::FirstTransition> first = aList.FirstTransitionOf(aResource);
	if (!first)
	{
		return aTaggedState == ListState::kUnknownState ? std::nullopt : std::optional<uint32_t>(aTaggedState);
	}
	const bool wholeBuffer = first->subresource == ListState::kAllSubresources || first->subresource == 0;
	if (!wholeBuffer || (first->flags & ListState::kEndOnly) != 0)
	{
		return std::nullopt;
	}
	return first->before;
}

std::vector<Move> MovesInto(const std::vector<Move>& aWanted)
{
	std::vector<Move> moves;
	for (const Move& move : aWanted)
	{
		if (move.before != move.after)
		{
			moves.push_back(move);
		}
	}
	return moves;
}

std::vector<Move> MovesBack(const std::vector<Move>& aMoves)
{
	std::vector<Move> back;
	for (auto move = aMoves.rbegin(); move != aMoves.rend(); ++move)
	{
		back.push_back({ move->resource, move->after, move->before });
	}
	return back;
}

std::vector<D3D12_RESOURCE_BARRIER> Barriers(const std::vector<Move>& aMoves)
{
	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	for (const Move& move : aMoves)
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = static_cast<ID3D12Resource*>(move.resource);
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATES(move.before);
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATES(move.after);
		barriers.push_back(barrier);
	}
	return barriers;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/hooks/Submission.cpp
)
```
```cmake
	src/hooks/Submission.cpp
	src/render/Predlss.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `93 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: drawing before DLSS: the checks, the starting states and the barriers" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 38: The camera's jitter and motion vectors for drawing before DLSS

Before DLSS, our blocks must move with the game's jitter and give DLSS motion vectors, or they shimmer and leave
trails:
- **`ClipJitter`:** the jitter `AddJitter` applies, as an offset per unit of clip w, which the vertex shader adds.
- **`MotionVector`:** where a static point was on screen a frame before, from Streamline's `clipToPrevClip`, in the
  game's units (`mvecScale` is 1, 1: a fraction of the screen).

`jittersign` and `mvsign` settle both signs in the game.

**Files:**
- Modify: `tests/TestCamera.cpp`, `src/render/Camera.h`, `src/render/Camera.cpp`

- [x] **Step 1: Edit `tests/TestCamera.cpp` to test the jitter offset and motion vectors**

```cpp
	CHECK(frame->reversedDepth);
}
}
```
```cpp
	CHECK(frame->reversedDepth);
}

Mat4 Inverse(const Mat4& aMatrix)  // Gauss-Jordan, for the well-conditioned matrices here
{
	double work[4][8]{};
	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 4; ++column)
		{
			work[row][column] = aMatrix[size_t(row * 4 + column)];
		}
		work[row][4 + row] = 1.0;
	}
	for (int column = 0; column < 4; ++column)
	{
		int pivot = column;
		for (int row = column + 1; row < 4; ++row)
		{
			pivot = std::fabs(work[row][column]) > std::fabs(work[pivot][column]) ? row : pivot;
		}
		std::swap(work[column], work[pivot]);
		const double scale = work[column][column];
		for (double& value : work[column])
		{
			value /= scale;
		}
		for (int row = 0; row < 4; ++row)
		{
			const double factor = row == column ? 0.0 : work[row][column];
			for (int k = 0; k < 8; ++k)
			{
				work[row][k] -= factor * work[column][k];
			}
		}
	}
	Mat4 inverse{};
	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 4; ++column)
		{
			inverse[size_t(row * 4 + column)] = work[row][4 + column];
		}
	}
	return inverse;
}

Mat4 Translation(double aX, double aY, double aZ)  // for row vectors
{
	return { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, aX, aY, aZ, 1 };
}
}
```
```cpp
	CHECK(cpmc::DepthTexelFor(199, 99, 200, 100, part) == std::make_pair(119u, 59u));
}
```
```cpp
	CHECK(cpmc::DepthTexelFor(199, 99, 200, 100, part) == std::make_pair(119u, 59u));
}

TEST(CameraClipJitterIsWhatAddJitterAdds)
{
	const auto frame = cpmc::MakeCameraFrame(Camera(LeftHanded()), {});
	const Mat4 jittered = cpmc::AddJitter(frame->cameraToClip, 0.5, 0.25, 1000, 500, -1.0);
	const auto offset = cpmc::ClipJitter(0.5, 0.25, 1000, 500, -1.0);
	CHECK_NEAR(offset[0], -0.001, 1e-12);
	CHECK_NEAR(offset[1], 0.001, 1e-12);
	const std::array<double, 4> point{ 1.0, 10.0, 2.0, 1.0 };
	const auto plain = cpmc::Transform(point, frame->cameraToClip);
	const auto moved = cpmc::Transform(point, jittered);
	CHECK_NEAR(moved[0], plain[0] + offset[0] * plain[3], 1e-12);
	CHECK_NEAR(moved[1], plain[1] + offset[1] * plain[3], 1e-12);
}

TEST(CameraMotionVectorsAreZeroForAStillCamera)
{
	const auto motion = cpmc::MotionVector({ 0.3, -0.2, 0.01, 5.0 }, Translation(0.0, 0.0, 0.0), 1.0);
	CHECK_NEAR(motion[0], 0.0, 1e-12);
	CHECK_NEAR(motion[1], 0.0, 1e-12);
}

TEST(CameraMotionVectorsPointToWhereThePointWas)
{
	const auto now = cpmc::MakeCameraFrame(Camera(LeftHanded()), {});
	const auto clip = cpmc::Transform({ 0.0, 10.0, 0.0, 1.0 }, now->cameraToClip);  // 10 m ahead
	// Streamline's clipToPrevClip after the camera moved by (aX, 0, aZ) since the frame before: from clip space back to
	// the camera-relative world, across to where the earlier camera was, then into its clip space.
	const auto clipToPrevClip = [&](double aX, double aZ) {
		SlConstants earlier = Camera(LeftHanded());
		earlier.position[0] -= float(aX);
		earlier.position[2] -= float(aZ);
		const auto then = cpmc::MakeCameraFrame(earlier, {});
		return cpmc::Multiply(cpmc::Multiply(Inverse(now->cameraToClip), Translation(aX, 0.0, aZ)), then->cameraToClip);
	};
	// The camera moved 1 m to its right (+x, since it faces north). The point was then 1 m to the camera's right:
	// further right on screen, by half its normalised x of kXs / 10, and that's where its motion vector points. (The
	// projection holds floats, hence 1e-6.)
	const auto right = cpmc::MotionVector(clip, clipToPrevClip(1.0, 0.0), 1.0);
	CHECK_NEAR(right[0], kXs / 20.0, 1e-6);
	CHECK_NEAR(right[1], 0.0, 1e-9);
	CHECK_NEAR(cpmc::MotionVector(clip, clipToPrevClip(1.0, 0.0), -1.0)[0], -kXs / 20.0, 1e-6);
	// The camera rose 1 m. The point was then 1 m above the camera: higher on screen, which is negative in screen
	// units, since they run down.
	const auto up = cpmc::MotionVector(clip, clipToPrevClip(0.0, 1.0), 1.0);
	CHECK_NEAR(up[0], 0.0, 1e-9);
	CHECK_NEAR(up[1], -kYs / 20.0, 1e-6);
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `'ClipJitter': is not a member of 'cpmc'`.

- [x] **Step 3: Edit `src/render/Camera.h` to declare them**

```cpp
Mat4 AddJitter(const Mat4& aCameraToClip, double aJitterX, double aJitterY, uint32_t aWidth, uint32_t aHeight, double aSign);
```
```cpp
Mat4 AddJitter(const Mat4& aCameraToClip, double aJitterX, double aJitterY, uint32_t aWidth, uint32_t aHeight, double aSign);
// The same jitter as an offset per unit of clip w: (aSign * 2 * jitterX / width, -aSign * 2 * jitterY / height). The
// vertex shader adds it to clip x and y, times clip w.
std::array<double, 2> ClipJitter(double aJitterX, double aJitterY, uint32_t aWidth, uint32_t aHeight, double aSign);

// A static point's motion vector, in the game's units (mvecScale 1, 1): from where the point is on screen to where it
// was a frame before, as a fraction of the screen (x right, y down), times aSign. aClip is the point's clip position
// without jitter, and aClipToPrevClip is Streamline's clipToPrevClip, for row vectors. The pixel shader does the same.
std::array<double, 2> MotionVector(const std::array<double, 4>& aClip, const Mat4& aClipToPrevClip, double aSign);
```

- [x] **Step 4: Edit `src/render/Camera.cpp` to define them**

```cpp
Mat4 AddJitter(const Mat4& aCameraToClip, double aJitterX, double aJitterY, uint32_t aWidth, uint32_t aHeight, double aSign)
{
	Mat4 jitter{};
	jitter[0] = jitter[5] = jitter[10] = jitter[15] = 1.0;
	jitter[12] = aSign * 2.0 * aJitterX / aWidth;   // clip x += this * clip w
	jitter[13] = -aSign * 2.0 * aJitterY / aHeight;  // clip y: pixel rows run down, clip y up
	return Multiply(aCameraToClip, jitter);
}
```
```cpp
Mat4 AddJitter(const Mat4& aCameraToClip, double aJitterX, double aJitterY, uint32_t aWidth, uint32_t aHeight, double aSign)
{
	const std::array<double, 2> offset = ClipJitter(aJitterX, aJitterY, aWidth, aHeight, aSign);
	Mat4 jitter{};
	jitter[0] = jitter[5] = jitter[10] = jitter[15] = 1.0;
	jitter[12] = offset[0];  // clip x += this * clip w
	jitter[13] = offset[1];
	return Multiply(aCameraToClip, jitter);
}

std::array<double, 2> ClipJitter(double aJitterX, double aJitterY, uint32_t aWidth, uint32_t aHeight, double aSign)
{
	return { aSign * 2.0 * aJitterX / aWidth, -aSign * 2.0 * aJitterY / aHeight };  // pixel rows run down, clip y up
}

std::array<double, 2> MotionVector(const std::array<double, 4>& aClip, const Mat4& aClipToPrevClip, double aSign)
{
	const std::array<double, 4> before = Transform(aClip, aClipToPrevClip);
	// Normalised device coordinates span 2 across the screen, and run up where the screen's y runs down.
	return { aSign * (before[0] / before[3] - aClip[0] / aClip[3]) * 0.5,
	         -aSign * (before[1] / before[3] - aClip[1] / aClip[3]) * 0.5 };
}
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `96 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: the camera's jitter offset and motion vectors" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 39: The shaders for drawing before DLSS

- **The vertex shader** adds the jitter (`ClipJitter`, as an offset per unit of clip w), and passes on the clip position
  without jitter and where it was a frame before (`clipToPrevClip`), for the motion vectors. Drawing at `Present`
  passes no jitter.
- **`PredlssPS`** writes the scene's colour before DLSS and its motion vectors:
  - **the colour:** the atlas's colour, linear (the scene before DLSS is linear HDR), times the face's shade and
    `exposure`. Run 2's dump puts sunlit ground at about 0.6 to 1.0 there;
  - **the motion vector:** as `MotionVector` computes it, times `mvsign`.
- **`FrameConstants`** gains `clipToPrevClip` and the jitter, and `tuning` gains `exposure` and `mvsign`. That makes
  it 256 bytes, the stride of the constant buffer ring.

**Files:**
- Modify: `tests/TestShaders.cpp`, `src/render/FrameConstants.h`, `shaders/blocks.hlsl`, `src/render/Shaders.h`,
  `src/render/Shaders.cpp`

- [x] **Step 1: Edit `tests/TestShaders.cpp` to test `PredlssPS`**

```cpp
	return desc.Size;
}
}
```
```cpp
	return desc.Size;
}

UINT Outputs(const std::vector<uint8_t>& aCode)
{
	Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection;
	D3D12_SHADER_DESC                              desc{};
	if (FAILED(D3DReflect(aCode.data(), aCode.size(), IID_PPV_ARGS(&reflection))) || FAILED(reflection->GetDesc(&desc)))
	{
		return 0;
	}
	return desc.OutputParameters;
}
}
```
```cpp
		CHECK(!shaders->vertex.empty());
		CHECK(!shaders->present.empty());
```
```cpp
		CHECK(!shaders->vertex.empty());
		CHECK(!shaders->present.empty());
		CHECK(!shaders->predlss.empty());
```
```cpp
		CHECK_EQ(ConstantBufferSize(shaders->present, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
	}
}
```
```cpp
		CHECK_EQ(ConstantBufferSize(shaders->present, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
		CHECK_EQ(ConstantBufferSize(shaders->predlss, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
	}
}

TEST(ShadersWriteColourAndMotionBeforeDlss)
{
	std::string error;
	const auto  shaders = cpmc::CompileBlockShaders(ShaderSource(), error);
	CHECK(shaders.has_value());
	if (shaders)
	{
		CHECK_EQ(Outputs(shaders->present), 1u);
		CHECK_EQ(Outputs(shaders->predlss), 2u);  // the colour, and the motion vectors
	}
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `'predlss': is not a member of 'cpmc::BlockShaders'`.

- [x] **Step 3: Edit `src/render/FrameConstants.h` to add the motion vectors' matrix and the jitter**

```cpp
struct FrameConstants
{
	float    cameraToClip[16];  // camera-relative world position to clip, for row vectors
	float    target[4];         // width, height, 1 / width, 1 / height of the image drawn into
	float    depthExtent[4];    // left, top, width, height of the depth image within its buffer
	uint32_t depthInfo[4];      // floats per row of the depth copy; 1 if there is a copy; 1 if depth is reversed; 0
	uint32_t atlasInfo[4];      // width, height, levels; 0
	uint32_t atlasLevels[8];    // where each level's texels start
	float    tuning[4];         // brightness; depth tolerance; 0; 0
};
static_assert(sizeof(FrameConstants) == 176);
```
```cpp
struct FrameConstants
{
	float    cameraToClip[16];    // camera-relative world position to clip, for row vectors
	float    target[4];           // width, height, 1 / width, 1 / height of the image drawn into
	float    depthExtent[4];      // left, top, width, height of the depth image within its buffer
	uint32_t depthInfo[4];        // floats per row of the depth copy; 1 if there is a copy; 1 if depth is reversed; 0
	uint32_t atlasInfo[4];        // width, height, levels; 0
	uint32_t atlasLevels[8];      // where each level's texels start
	float    tuning[4];           // brightness; depth tolerance; exposure; the motion vectors' sign
	float    clipToPrevClip[16];  // Streamline's, for row vectors: a clip position to where it was a frame before
	float    jitter[4];           // the jitter as an offset per unit of clip w (ClipJitter): x, y; 0; 0
};
static_assert(sizeof(FrameConstants) == 256);
```

- [x] **Step 4: Edit `shaders/blocks.hlsl` for the jitter, the motion vectors and `PredlssPS`**

```hlsl
    uint4  atlasLevels[2];            // where each level's texels start
    float4 tuning;                    // brightness; depth tolerance
};
```
```hlsl
    uint4  atlasLevels[2];            // where each level's texels start
    float4 tuning;                    // brightness; depth tolerance; exposure; the motion vectors' sign
    row_major float4x4 clipToPrevClip;  // Streamline's: a clip position to where it was a frame before
    float4 jitter;                    // the jitter as an offset per unit of clip w: x, y
};
```
```hlsl
    nointerpolation uint face : TEXCOORD1;
};
```
```hlsl
    nointerpolation uint face : TEXCOORD1;
    float4 clip     : TEXCOORD2;  // without jitter
    float4 prevClip : TEXCOORD3;  // where it was a frame before
};
```
```hlsl
    o.position = mul(float4(world, 1.0), cameraToClip);
```
```hlsl
    const float4 clip = mul(float4(world, 1.0), cameraToClip);
    o.position = float4(clip.xy + jitter.xy * clip.w, clip.zw);
    o.clip = clip;
    o.prevClip = mul(clip, clipToPrevClip);
```
```hlsl
    return float4(texel.rgb * i.colour.rgb * kFaceShade[min(i.face, 6)] * tuning.x, 1.0);
}
```
```hlsl
    return float4(texel.rgb * i.colour.rgb * kFaceShade[min(i.face, 6)] * tuning.x, 1.0);
}

// The sRGB curve, undone: the atlas holds Minecraft's colours as they look, and the scene before DLSS is linear.
float3 Linear(float3 aColour)
{
    return aColour <= 0.04045 ? aColour / 12.92 : pow(abs((aColour + 0.055) / 1.055), 2.4);
}

struct PredlssOut
{
    float4 colour : SV_Target0;
    float2 motion : SV_Target1;
};

// Step 3: into the scene's colour before DLSS, which then tone maps, blooms and grades the blocks with the rest. The
// game's own depth hides what's behind, and the motion vectors let DLSS follow the blocks as the camera moves.
PredlssOut PredlssPS(VSOut i)
{
    const float4 texel = SampleAtlas(i.uv);
    if (texel.a < 0.5)
    {
        discard;
    }
    PredlssOut o;
    o.colour = float4(Linear(texel.rgb * i.colour.rgb) * kFaceShade[min(i.face, 6)] * tuning.z, 1.0);
    // As Camera.cpp's MotionVector: a fraction of the screen, from where the point is to where it was.
    const float2 now = i.clip.xy / i.clip.w;
    const float2 before = i.prevClip.xy / i.prevClip.w;
    o.motion = (before - now) * float2(0.5, -0.5) * tuning.w;
    return o;
}
```

- [x] **Step 5: Edit `src/render/Shaders.h` to add `PredlssPS`**

```cpp
	std::vector<uint8_t> present;  // PresentPS
```
```cpp
	std::vector<uint8_t> present;  // PresentPS
	std::vector<uint8_t> predlss;  // PredlssPS
```

- [x] **Step 6: Edit `src/render/Shaders.cpp` to compile it**

```cpp
	return BlockShaders{ std::move(*vertex), std::move(*present) };
```
```cpp
	auto predlss = CompileShader(aSource, "blocks.hlsl", "PredlssPS", "ps_5_1", aError);
	if (!predlss)
	{
		return std::nullopt;
	}
	return BlockShaders{ std::move(*vertex), std::move(*present), std::move(*predlss) };
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `97 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: shaders for drawing before DLSS: jitter, motion vectors and linear colour" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 40: The house and the field

Step 3 places a `house` and fields of 10,000 and 100,000 blocks, for the timings (the spec's "Structures"):
- **`house`:** 7 × 7 and 5 high. A cobblestone floor, oak plank walls with oak log corners, a glass window in each
  wall, a doorway, a plank roof, and an oak leaves bush beside it.
- **`field <n>`:** rolling ground of grass over three of dirt over stone, in rows of whole columns until it holds n
  blocks.

The test atlas lacks some of their blocks, so the tests build one with all of them.

**Files:**
- Modify: `tests/TestStructures.cpp`, `src/world/Structures.h`, `src/Plugin.cpp`
- Rewrite: `src/world/Structures.cpp`

- [x] **Step 1: Edit `tests/TestStructures.cpp` to test the house and the field**

```cpp
using cpmc::BlockPos;
using cpmc::BlockWorld;
```
```cpp
using cpmc::BlockPos;
using cpmc::BlockWorld;

namespace
{
// An atlas with every block the structures use, each drawn with the same plain sprite.
cpmc::Atlas BuildingAtlas()
{
	std::vector<uint8_t> out;
	const char           magic[] = "CPMCATL1";
	out.insert(out.end(), magic, magic + 8);
	const char* names[] = { "stone", "cobblestone", "oak_planks", "oak_log", "glass", "oak_leaves", "grass_block", "dirt" };
	for (const uint32_t value : { 16u, 16u, 1u, 1u, uint32_t(std::size(names)), 0u })  // size, levels, sprites, blocks
	{
		fixture::Put32(out, value);
	}
	for (const uint32_t value : { 0u, 0u, 16u, 16u })
	{
		fixture::Put32(out, value);
	}
	for (const char* name : names)
	{
		fixture::PutBlock(out, name, { 0, 0, 0, 0, 0, 0 }, { 255, 255, 255 }, 0, false);
	}
	for (int i = 0; i < 16 * 16; ++i)
	{
		fixture::Put32(out, 0xFF808080u);
	}
	std::string error;
	return *cpmc::ParseAtlas(out, error);
}

bool Is(const BlockWorld& aWorld, const cpmc::Atlas& aAtlas, const BlockPos& aAt, const char* aName)
{
	return aWorld.Get(aAt) == aAtlas.BlockId(aName);
}
}
```
```cpp
	CHECK(!cpmc::PlaceStructure(world, atlas, "castle", {}, error));
	CHECK_STR(error, "no structure called \"castle\" in this build");
}
```
```cpp
	CHECK(!cpmc::PlaceStructure(world, atlas, "castle", {}, error));
	CHECK_STR(error, "no structure called \"castle\" in this build");
}

TEST(StructuresPlaceAHouse)
{
	const cpmc::Atlas atlas = BuildingAtlas();
	BlockWorld        world;
	std::string       error;
	const auto        placed = cpmc::PlaceStructure(world, atlas, "house", { 0, 64, 0 }, error);
	CHECK(placed.has_value());
	CHECK_EQ(placed->blocks, size_t(176));  // floor 49, roof 49, walls 72 less the doorway's 2, bush 8
	CHECK_EQ(world.BlockCount(), size_t(176));
	CHECK(placed->max == (BlockPos{ 9, 68, 6 }));
	CHECK(Is(world, atlas, { 0, 64, 0 }, "cobblestone") && Is(world, atlas, { 6, 64, 6 }, "cobblestone"));  // the floor
	CHECK(Is(world, atlas, { 0, 65, 0 }, "oak_log") && Is(world, atlas, { 6, 67, 6 }, "oak_log"));          // corners
	CHECK(Is(world, atlas, { 1, 65, 0 }, "oak_planks"));                                                    // a wall
	CHECK(Is(world, atlas, { 5, 66, 0 }, "glass") && Is(world, atlas, { 3, 66, 6 }, "glass"));              // windows
	CHECK(Is(world, atlas, { 0, 66, 3 }, "glass") && Is(world, atlas, { 6, 66, 3 }, "glass"));
	CHECK_EQ(world.Get({ 3, 65, 0 }), uint16_t(0));  // the doorway
	CHECK_EQ(world.Get({ 3, 66, 0 }), uint16_t(0));
	CHECK(Is(world, atlas, { 3, 67, 0 }, "oak_planks"));  // above it
	CHECK_EQ(world.Get({ 3, 65, 3 }), uint16_t(0));       // inside
	CHECK(Is(world, atlas, { 3, 68, 3 }, "oak_planks"));  // the roof
	CHECK(Is(world, atlas, { 8, 64, 2 }, "oak_leaves") && Is(world, atlas, { 9, 65, 3 }, "oak_leaves"));  // the bush
}

TEST(StructuresPlaceAFieldOfAboutTheBlocksAskedFor)
{
	const cpmc::Atlas atlas = BuildingAtlas();
	for (const size_t blocks : { size_t(1), size_t(1000), size_t(10000) })
	{
		BlockWorld  world;
		std::string error;
		const auto  placed = cpmc::PlaceField(world, atlas, { 0, 0, 0 }, blocks, error);
		CHECK(placed.has_value());
		CHECK(placed->blocks >= blocks && placed->blocks < blocks + 10);  // whole columns, of at most 10 blocks
		CHECK_EQ(world.BlockCount(), placed->blocks);
	}
	BlockWorld  world;
	std::string error;
	cpmc::PlaceField(world, atlas, { 0, 0, 0 }, 1000, error);
	// The first column is 6 + round(2 sin 0 + 2 cos 0) = 8 high: grass, three of dirt, then stone.
	CHECK(Is(world, atlas, { 0, 7, 0 }, "grass_block"));
	CHECK(Is(world, atlas, { 0, 6, 0 }, "dirt") && Is(world, atlas, { 0, 4, 0 }, "dirt"));
	CHECK(Is(world, atlas, { 0, 3, 0 }, "stone") && Is(world, atlas, { 0, 0, 0 }, "stone"));
	CHECK_EQ(world.Get({ 0, 8, 0 }), uint16_t(0));
}

TEST(StructuresNeedTheirBlocks)
{
	const cpmc::Atlas atlas = fixture::Atlas();  // no cobblestone, planks or dirt
	BlockWorld        world;
	std::string       error;
	CHECK(!cpmc::PlaceStructure(world, atlas, "house", {}, error));
	CHECK_STR(error, "the atlas has no cobblestone");
	CHECK(!cpmc::PlaceField(world, atlas, {}, 100, error));
	CHECK_STR(error, "the atlas has no dirt");
	CHECK(!cpmc::PlaceField(world, BuildingAtlas(), {}, 0, error));
	CHECK_STR(error, "a field needs at least one block");
	CHECK_EQ(world.BlockCount(), size_t(0));
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `'PlaceField': is not a member of 'cpmc'`.

- [x] **Step 3: Edit `src/world/Structures.h` to add them**

```cpp
// Adds a structure with its minimum corner at aOrigin (see the spec's "Structures"):
// - "cube": one stone block;
// - "sampler": every block of the atlas, in a row along +x with a gap between each, on a stone base.
// Nothing, with aError saying why, for another name or when the atlas lacks stone.
std::optional<Placed> PlaceStructure(BlockWorld& aWorld, const Atlas& aAtlas, std::string_view aName, const BlockPos& aOrigin,
                                     std::string& aError);
```
```cpp
// Adds a structure with its minimum corner at aOrigin (see the spec's "Structures"):
// - "cube": one stone block;
// - "sampler": every block of the atlas, in a row along +x with a gap between each, on a stone base;
// - "house": 7 x 7 and 5 high: a cobblestone floor, oak plank walls with oak log corners, a glass window in each wall,
//   a doorway, a plank roof, and an oak leaves bush beside it.
// Nothing, with aError saying why, for another name or when the atlas lacks a block it needs.
std::optional<Placed> PlaceStructure(BlockWorld& aWorld, const Atlas& aAtlas, std::string_view aName, const BlockPos& aOrigin,
                                     std::string& aError);

// Adds rolling ground of grass over dirt over stone with its minimum corner at aOrigin, holding aBlocks blocks, or up to
// 9 more, since it's made of whole columns. Nothing, with aError saying why, for no blocks or when the atlas lacks a
// block it needs.
std::optional<Placed> PlaceField(BlockWorld& aWorld, const Atlas& aAtlas, const BlockPos& aOrigin, size_t aBlocks, std::string& aError);
```

- [x] **Step 4: Write `src/world/Structures.cpp`**

```cpp
#include "world/Structures.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

namespace cpmc
{
namespace
{
// The IDs of aNames in the atlas, in order. Nothing, with aError naming the first one missing.
std::optional<std::vector<uint16_t>> BlockIds(const Atlas& aAtlas, std::initializer_list<const char*> aNames, std::string& aError)
{
	std::vector<uint16_t> ids;
	for (const char* name : aNames)
	{
		const std::optional<uint16_t> id = aAtlas.BlockId(name);
		if (!id)
		{
			aError = std::string("the atlas has no ") + name;
			return std::nullopt;
		}
		ids.push_back(*id);
	}
	return ids;
}

// 7 x 7 and 5 high, with its doorway in the wall at the lowest z, and the bush beside the wall at the highest x.
std::optional<Placed> PlaceHouse(BlockWorld& aWorld, const Atlas& aAtlas, const BlockPos& aOrigin, std::string& aError)
{
	const auto ids = BlockIds(aAtlas, { "cobblestone", "oak_planks", "oak_log", "glass", "oak_leaves" }, aError);
	if (!ids)
	{
		return std::nullopt;
	}
	const uint16_t cobblestone = (*ids)[0], planks = (*ids)[1], log = (*ids)[2], glass = (*ids)[3], leaves = (*ids)[4];
	Placed placed;
	placed.min = aOrigin;
	placed.max = { aOrigin.x + 9, aOrigin.y + 4, aOrigin.z + 6 };
	const auto set = [&](int32_t aX, int32_t aY, int32_t aZ, uint16_t aBlock) {
		aWorld.Set({ aOrigin.x + aX, aOrigin.y + aY, aOrigin.z + aZ }, aBlock);
		++placed.blocks;
	};
	for (int32_t x = 0; x < 7; ++x)
	{
		for (int32_t z = 0; z < 7; ++z)
		{
			set(x, 0, z, cobblestone);  // the floor
			set(x, 4, z, planks);       // the roof
			const bool edgeX = x == 0 || x == 6;
			const bool edgeZ = z == 0 || z == 6;
			if (!edgeX && !edgeZ)
			{
				continue;  // inside
			}
			for (int32_t y = 1; y <= 3; ++y)
			{
				const bool doorway = z == 0 && x == 3 && y <= 2;
				const bool window = y == 2 && ((z == 0 && x == 5) || (z == 6 && x == 3) || (edgeX && z == 3));
				if (!doorway)
				{
					set(x, y, z, edgeX && edgeZ ? log : window ? glass : planks);
				}
			}
		}
	}
	for (int32_t x = 8; x <= 9; ++x)
	{
		for (int32_t y = 0; y <= 1; ++y)
		{
			for (int32_t z = 2; z <= 3; ++z)
			{
				set(x, y, z, leaves);
			}
		}
	}
	return placed;
}
}

BlockPos PlacementOrigin(const Vec3d& aFeet, const Vec3f& aForward)
{
	double       x = aForward.x, y = aForward.y;
	const double length = std::sqrt(x * x + y * y);
	if (length > 1e-6)
	{
		x /= length;
		y /= length;
	}
	else
	{
		x = 0.0;
		y = 1.0;  // looking straight up or down: place it to the north
	}
	const Vec3d ahead = McFromWorld({ aFeet.x + 3.0 * x, aFeet.y + 3.0 * y, aFeet.z });
	return { int32_t(std::floor(ahead.x)), int32_t(std::floor(ahead.y)), int32_t(std::floor(ahead.z)) };
}

std::pair<Vec3d, Vec3d> WorldBox(const Placed& aPlaced)
{
	// Minecraft block (x, y, z) fills world x..x+1, -z-1..-z, y..y+1.
	return { { double(aPlaced.min.x), double(-aPlaced.max.z - 1), double(aPlaced.min.y) },
	         { double(aPlaced.max.x + 1), double(-aPlaced.min.z), double(aPlaced.max.y + 1) } };
}

std::optional<Placed> PlaceStructure(BlockWorld& aWorld, const Atlas& aAtlas, std::string_view aName, const BlockPos& aOrigin,
                                     std::string& aError)
{
	const std::optional<uint16_t> stone = aAtlas.BlockId("stone");
	if (!stone)
	{
		aError = "the atlas has no stone";
		return std::nullopt;
	}
	Placed placed;
	placed.min = aOrigin;
	if (aName == "cube")
	{
		aWorld.Set(aOrigin, *stone);
		placed.blocks = 1;
		placed.max = aOrigin;
		return placed;
	}
	if (aName == "sampler")
	{
		const auto count = int32_t(aAtlas.blocks.size());
		for (int32_t i = 0; i < count; ++i)
		{
			aWorld.Set({ aOrigin.x + 2 * i, aOrigin.y, aOrigin.z }, *stone);
			aWorld.Set({ aOrigin.x + 2 * i + 1, aOrigin.y, aOrigin.z }, *stone);
			aWorld.Set({ aOrigin.x + 2 * i, aOrigin.y + 1, aOrigin.z }, uint16_t(i + 1));
		}
		placed.blocks = size_t(3 * count);
		placed.max = { aOrigin.x + 2 * count - 1, aOrigin.y + 1, aOrigin.z };
		return placed;
	}
	if (aName == "house")
	{
		return PlaceHouse(aWorld, aAtlas, aOrigin, aError);
	}
	aError = "no structure called \"" + std::string(aName) + "\" in this build";
	return std::nullopt;
}

std::optional<Placed> PlaceField(BlockWorld& aWorld, const Atlas& aAtlas, const BlockPos& aOrigin, size_t aBlocks, std::string& aError)
{
	const auto ids = BlockIds(aAtlas, { "grass_block", "dirt", "stone" }, aError);
	if (!ids)
	{
		return std::nullopt;
	}
	if (aBlocks == 0)
	{
		aError = "a field needs at least one block";
		return std::nullopt;
	}
	const uint16_t grass = (*ids)[0], dirt = (*ids)[1], stone = (*ids)[2];
	// Rows of columns along x, as many columns to a row as there would be rows if each column held its average of 6
	// blocks, until the field holds aBlocks.
	const auto width = int32_t(std::ceil(std::sqrt(double(aBlocks) / 6.0)));
	Placed     placed;
	placed.min = aOrigin;
	placed.max = aOrigin;
	for (int32_t z = 0; placed.blocks < aBlocks; ++z)
	{
		for (int32_t x = 0; x < width && placed.blocks < aBlocks; ++x)
		{
			// 2 to 10 blocks high, rolling: grass on top, then three of dirt, then stone.
			const auto height = int32_t(6 + std::lround(2.0 * std::sin(x / 5.0) + 2.0 * std::cos(z / 7.0)));
			for (int32_t y = 0; y < height; ++y)
			{
				aWorld.Set({ aOrigin.x + x, aOrigin.y + y, aOrigin.z + z }, y == height - 1 ? grass : y >= height - 4 ? dirt : stone);
			}
			placed.blocks += size_t(height);
			placed.max = { std::max(placed.max.x, aOrigin.x + x), std::max(placed.max.y, aOrigin.y + height - 1), aOrigin.z + z };
		}
	}
	return placed;
}
}
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `100 test(s), 0 failure(s)`.

- [x] **Step 6: Edit `src/Plugin.cpp` to place fields**

```cpp
	const cpmc::BlockPos                origin = cpmc::PlacementOrigin(pose->feet, pose->forward);
	const std::optional<cpmc::Placed>   placed = cpmc::PlaceStructure(g_world, *g_renderer->AtlasData(), aRequest.what, origin, error);
```
```cpp
	const cpmc::BlockPos              origin = cpmc::PlacementOrigin(pose->feet, pose->forward);
	const cpmc::Atlas&                atlas = *g_renderer->AtlasData();
	const std::optional<cpmc::Placed> placed = aRequest.what == "field"
	                                               ? cpmc::PlaceField(g_world, atlas, origin, size_t(aRequest.number), error)
	                                               : cpmc::PlaceStructure(g_world, atlas, aRequest.what, origin, error);
```

- [x] **Step 7: Build; it must compile without warnings, and the tests still pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `100 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: the house and the field" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 41: The frame log keeps each list's transitions

Part 1 couldn't say what state the tagged buffers are in just before the DLSS list: the frame log counted barriers but
didn't keep them. Now each logged list keeps, for every resource it moved, the first transition's state before (with
its subresource and flags) and the state after the last. `frame_summary.py` prints them for the DLSS lists and the
G-buffer lists, so step 3's first log shows the states our list starts from, and step 4 sees where the G-buffer lists
leave their targets.

**Files:**
- Modify: `tests/TestFrameLogJson.cpp`, `src/log/FrameLogData.h`, `src/log/FrameLogJson.cpp`, `src/log/FrameLog.cpp`,
  `tools/test_frame_summary.py`, `tools/frame_summary.py`

- [x] **Step 1: Edit `tests/TestFrameLogJson.cpp` to test the transitions**

```cpp
	log.lists.push_back({ 0x7100, "Scene", 100, { gbuffer, unknown } });
```
```cpp
	log.lists.push_back({ 0x7100, "Scene", 100, { gbuffer, unknown },
	                      { { 0x1, 0x0, 0, cpmc::ListState::kBeginOnly, std::nullopt }, { 0x5000, 0x10, 0xFFFFFFFF, 0, 0x80 } } });
```
```cpp
	CHECK(Has(json, "\"unknown\": true"));
```
```cpp
	CHECK(Has(json, "\"unknown\": true"));
	CHECK(Has(json, "\"resource\": \"0x1\",\n          \"firstBefore\": \"0x0\",\n          \"firstSubresource\": 0,\n"
	                "          \"firstFlags\": 1,\n          \"after\": null"));
	CHECK(Has(json, "\"firstSubresource\": \"all\""));
	CHECK(Has(json, "\"after\": \"0x80\""));
```
```cpp
TEST(FrameLogWritesNullWithoutAScriptCamera)
```
```cpp
TEST(FrameLogTakesEachListsTransitionsByAddress)
{
	cpmc::ListState state;
	state.Begin(1, true);
	state.Transition(reinterpret_cast<void*>(0x20), 0x4, 0x8, cpmc::ListState::kAllSubresources, 0);
	state.Transition(reinterpret_cast<void*>(0x10), 0x10, 0xC0, 0, cpmc::ListState::kBeginOnly);
	const std::vector<cpmc::LogTransition> transitions = cpmc::TransitionsOf(state);
	CHECK_EQ(transitions.size(), size_t(2));
	CHECK_EQ(transitions[0].resource, uint64_t(0x10));
	CHECK_EQ(transitions[0].firstFlags, cpmc::ListState::kBeginOnly);
	CHECK(!transitions[0].after);  // begun, not ended
	CHECK_EQ(transitions[1].firstBefore, 0x4u);
	CHECK_EQ(transitions[1].after.value_or(0), 0x8u);
}

TEST(FrameLogWritesNullWithoutAScriptCamera)
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `'LogTransition': is not a member of 'cpmc'`.

- [x] **Step 3: Edit `src/log/FrameLogData.h` to add the transitions**

```cpp
struct LogList
{
	uint64_t                list = 0;
	std::string             name;  // the game's name for it, if found
	uint32_t                frame = 0;
	std::vector<PassRecord> passes;
};
```
```cpp
// A resource's transitions in one list: the first one's state before, subresource and flags (see
// ListState::FirstTransition), and the state after the last, if that's known.
struct LogTransition
{
	uint64_t                resource = 0;
	uint32_t                firstBefore = 0;
	uint32_t                firstSubresource = 0;
	uint32_t                firstFlags = 0;
	std::optional<uint32_t> after;
};

struct LogList
{
	uint64_t                   list = 0;
	std::string                name;  // the game's name for it, if found
	uint32_t                   frame = 0;
	std::vector<PassRecord>    passes;
	std::vector<LogTransition> transitions;  // by resource address
};
```
```cpp
// The name dxgiformat.h gives a format, without the DXGI_FORMAT_ prefix, or "format <n>".
std::string DxgiFormatName(uint32_t aFormat);
```
```cpp
// The name dxgiformat.h gives a format, without the DXGI_FORMAT_ prefix, or "format <n>".
std::string DxgiFormatName(uint32_t aFormat);

// The transitions a list recorded, by resource address.
std::vector<LogTransition> TransitionsOf(const ListState& aState);
```

- [x] **Step 4: Edit `src/log/FrameLogJson.cpp` to write them**

```cpp
#include "Json.h"

#include <cmath>
```
```cpp
#include "Json.h"

#include <algorithm>
#include <cmath>
```
```cpp
std::string FrameLogJson(const FrameLogData& aLog)
{
```
```cpp
std::vector<LogTransition> TransitionsOf(const ListState& aState)
{
	std::vector<LogTransition> transitions;
	for (const auto& [resource, first] : aState.FirstTransitions())
	{
		transitions.push_back({ reinterpret_cast<uintptr_t>(resource), first.before, first.subresource, first.flags,
		                        aState.StateOf(resource) });
	}
	std::sort(transitions.begin(), transitions.end(),
	          [](const LogTransition& aA, const LogTransition& aB) { return aA.resource < aB.resource; });
	return transitions;
}

std::string FrameLogJson(const FrameLogData& aLog)
{
```
```cpp
		for (const PassRecord& pass : list.passes)
		{
			Pass(json, pass, taggedDepth);
		}
		json.EndArray();
```
```cpp
		for (const PassRecord& pass : list.passes)
		{
			Pass(json, pass, taggedDepth);
		}
		json.EndArray();
		json.Key("transitions").BeginArray();
		for (const LogTransition& transition : list.transitions)
		{
			json.BeginObject();
			json.Key("resource").String(Hex(transition.resource));
			json.Key("firstBefore").String(Hex(transition.firstBefore));
			json.Key("firstSubresource");
			transition.firstSubresource == ListState::kAllSubresources ? json.String("all") : json.UInt(transition.firstSubresource);
			json.Key("firstFlags").UInt(transition.firstFlags);
			json.Key("after");
			transition.after ? json.String(Hex(*transition.after)) : json.Null();
			json.EndObject();
		}
		json.EndArray();
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `101 test(s), 0 failure(s)`.

- [x] **Step 6: Edit `src/log/FrameLog.cpp` to keep each logged list's transitions**

```cpp
		data_.lists.push_back({ reinterpret_cast<uintptr_t>(aList), names_ ? names_->Find(aList) : std::string(), aState.Frame(),
		                        aState.Passes() });
```
```cpp
		data_.lists.push_back({ reinterpret_cast<uintptr_t>(aList), names_ ? names_->Find(aList) : std::string(), aState.Frame(),
		                        aState.Passes(), TransitionsOf(aState) });
```

- [x] **Step 7: Build; it must compile without warnings, and the tests still pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `101 test(s), 0 failure(s)`.

- [x] **Step 8: Edit `tools/test_frame_summary.py` to test the states**

```python
            "barriers": 4, "enhancedBarriers": 0, "events": ["GBuffer"],
        }],
    }],
```
```python
            "barriers": 4, "enhancedBarriers": 0, "events": ["GBuffer"],
        }],
        "transitions": [
            {"resource": "0x1", "firstBefore": "0x0", "firstSubresource": 0, "firstFlags": 1, "after": None},
            {"resource": "0x5000", "firstBefore": "0x10", "firstSubresource": "all", "firstFlags": 0, "after": "0x80"},
        ],
    }],
```
```python
def test_summary_without_a_script_camera_or_candidates():
```
```python
def test_summary_shows_the_states_in_the_dlss_and_gbuffer_lists():
    summary = text()
    assert "0x7100 frame 100 [DLSS]\n  depth 0x5000: 0x10 -> 0x80; tagged 0x80" in summary
    assert ("0x7100 frame 100 [G-buffer]: 0x1: 0x0 (subresource 0, flags 1) -> unknown, 0x2: not moved, "
            "0x3: not moved, 0x5000: 0x10 -> 0x80") in summary
    older = copy.deepcopy(LOG)
    del older["lists"][0]["transitions"]  # logs from 0.2.0 and before
    assert "  depth 0x5000: not logged; tagged 0x80" in text(older)


def test_summary_without_a_script_camera_or_candidates():
```

- [x] **Step 9: Run it; the new test must fail**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render/tools && uv run --no-project -q --with numpy --with pillow --with pytest pytest -q test_frame_summary.py 2>&1 | grep -E "^E |passed|failed"
```
Expected: `AssertionError`, then `1 failed, 6 passed`.

- [x] **Step 10: Edit `tools/frame_summary.py` to print the states**

```python
def summarise(log):
```
```python
def state_text(entry, resource):
    """A resource's states in a logged list: its first transition's state before, and the state after its last."""
    if "transitions" not in entry:
        return "not logged"
    for transition in entry["transitions"]:
        if transition["resource"] == resource:
            first = transition["firstBefore"]
            notes = []
            if transition["firstSubresource"] != "all":
                notes.append(f"subresource {transition['firstSubresource']}")
            if transition["firstFlags"]:
                notes.append(f"flags {transition['firstFlags']}")
            if notes:
                first += f" ({', '.join(notes)})"
            return f"{first} -> {transition['after'] or 'unknown'}"
    return "not moved"


def summarise(log):
```
```python
    add(f"in all: {len(log['lists'])} lists; {totals}; {render_passes} render passes")
```
```python
    add(f"in all: {len(log['lists'])} lists; {totals}; {render_passes} render passes")

    add("")
    add("== States in the DLSS and G-buffer lists (before the first transition -> after the last)")
    tagged = {}
    for tag in log["tags"]:
        if tag["resource"] != "0x0":
            tagged.setdefault(tag["resource"], (tag["name"], tag["state"]))
    for entry in log["lists"]:
        if entry["list"] in native_lists:
            add(f"{entry['list']} frame {entry['frame']} [DLSS]")
            for resource, (name, state) in tagged.items():
                add(f"  {name} {resource}: {state_text(entry, resource)}; tagged {state}")
        if entry["list"] in candidate_lists:
            resources = []
            for index, p in enumerate(entry["passes"]):
                if (entry["list"], index) in candidates:
                    for t in p["targets"] + [p["depth"]]:
                        if t and not t.get("unknown") and t["resource"] not in resources:
                            resources.append(t["resource"])
            states = ", ".join(f"{resource}: {state_text(entry, resource)}" for resource in resources)
            add(f"{entry['list']} frame {entry['frame']} [G-buffer]: {states}")
```

- [x] **Step 11: Run all the tools' tests; they must pass**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render/tools && uv run --no-project -q --with numpy --with pillow --with pytest pytest -q 2>&1 | tail -1
```
Expected: `22 passed`.

- [x] **Step 12: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: the frame log keeps each list's transitions" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 42: The renderer draws before DLSS

`mode predlss` (step 3):
- **At the evaluate:** the renderer keeps the frame's constants and its colour, motion vector and depth tags.
- **Our list:** when the game submits the DLSS list, `ListBefore` (Task 36) records a list of ours and puts it just
  before. It:
  - checks the tagged buffers (Task 37);
  - finds the state each was in, from the DLSS list's first transitions (Tasks 35 and 37);
  - moves them to render target and depth write;
  - draws every section with `PredlssPS` (Task 39), the jitter and `clipToPrevClip`;
  - moves them back.
- **Tracking:** `predlss` asks for command list tracking while it's on, since the first transitions come from the
  tracker. A DLSS list that wasn't tracked from its `Reset` is skipped.
- **The fence:** `OnSubmitted` signals the fence for the slot it used. The slot passes from `ListBefore` to
  `OnSubmitted` through a thread-local, since both run on the same thread inside one `ExecuteCommandLists`, while the
  main thread may change the mode in between. So the signal is never lost.
- **Settings:** `exposure` (3 to start), `jittersign` and `mvsign` (1, -1, or 0 for none).
- **Present mode** is unchanged. It now shares the constants and the section drawing with `predlss`.

Streamline's `clipToPrevClip` is for row vectors, as the vertex shader uses it: rebuilt from the cameras of two frames
in run 2's log taken while turning, it matches to within 3e-8, and its transpose doesn't.

**Files:**
- Rewrite: `src/render/Renderer.h`, `src/render/Renderer.cpp`

- [x] **Step 1: Write `src/render/Renderer.h`**

```cpp
#pragma once

#include "Coords.h"
#include "LogQueue.h"
#include "hooks/HookEvents.h"
#include "render/Atlas.h"
#include "render/Camera.h"
#include "render/FrameConstants.h"
#include "render/Gpu.h"
#include "render/Shaders.h"
#include "world/BlockWorld.h"
#include "world/Vertex.h"

#include <array>
#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cpmc
{
// The plugin's own drawing (the spec's "Renderer"). The modes:
// - off;
// - present (step 2): at each DLSS evaluate the tagged depth's depth plane is copied into a buffer of ours, inside the
//   game's list. Once that list is submitted, the next Present draws the blocks into the back buffer with our own list,
//   on the same queue, hidden where the copied depth says the game drew something nearer;
// - predlss (step 3): a list of ours goes into the submission holding the DLSS evaluate's list, just before it. It draws
//   into the tagged colour, motion vectors and depth, with the frame's jitter, so DLSS takes the blocks for the game's
//   own. Its barriers start from the state the DLSS list found each buffer in (its first transitions), so command lists
//   are tracked while this mode is on.
class Renderer : public HookEvents
{
public:
	Renderer(LogQueue& aLog, std::filesystem::path aFolder);

	// Main thread.
	bool         SetMode(std::string_view aMode, std::string& aError);
	std::string  ModeName() const;
	bool         LoadAtlas(std::string& aError);  // atlas.bin, from the plugin's folder
	const Atlas* AtlasData() const;               // null until loaded; it stays valid, since the atlas is loaded once
	bool         ReloadShaders(std::string& aError);
	// brightness, tolerance, cull, origin.x/y/z; for predlss, exposure, jittersign and mvsign
	bool         Set(std::string_view aName, double aValue, std::string& aError);
	void         UploadSection(const SectionPos& aSection, std::vector<Vertex> aVertices);  // no vertices: remove it
	void         Update();  // uploads, and frees what the GPU has finished with

	struct Stats
	{
		uint64_t    drawn = 0;
		uint64_t    skipped = 0;
		uint64_t    lateCamera = 0;  // present: drawn with the same DLSS frame as the draw before
		std::string lastSkip;
		double      gpuMsAverage = -1.0;
		double      gpuMsWorst = -1.0;
		double      frameMsAverage = -1.0;
		double      frameMsWorst = -1.0;
		size_t      sections = 0;
		size_t      vertices = 0;
	};
	Stats TakeStats();  // since the last call
	void  Shutdown();   // waits for the GPU, then releases everything

	void               OnEvaluate(const EvaluateCall& aCall) override;
	void               OnSubmit(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists) override;
	ID3D12CommandList* ListBefore(ID3D12CommandQueue* aQueue, ID3D12CommandList* aList) override;
	void               OnSubmitted(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists) override;
	void               OnPresent(IDXGISwapChain* aSwapChain) override;

private:
	enum class Mode
	{
		Off,
		Present,
		Predlss,
	};

	static constexpr uint32_t kSlots = 8;  // frames in flight: more than Streamline's 6

	struct SectionGpu
	{
		gpu::ComPtr<ID3D12Resource> buffer;
		uint32_t                    vertices = 0;
		uint64_t                    ready = 0;  // the fence value its upload signals
	};

	struct Frame  // present: one DLSS evaluate's
	{
		const void* list = nullptr;  // the game's list holding the depth copy; compared, never used
		uint32_t    frame = 0;
		bool        valid = false;  // its constants were found
		SlConstants constants;
		bool        depth = false;  // the depth copy holds its depth
		uint32_t    rowFloats = 0;
		sl::Extent  extent{};
	};

	struct PredlssFrame  // predlss: one DLSS evaluate's
	{
		const void*                list = nullptr;  // the DLSS list; compared, never used
		std::optional<SlConstants> constants;
		SlTag                      colour;  // the scaling input colour
		SlTag                      motion;
		SlTag                      depth;
	};

	// All with the mutex held.
	bool               EnsureObjects();  // false until the main queue is known, or if creating them failed
	bool               EnsurePipeline(DXGI_FORMAT aFormat, bool aReversed);
	bool               EnsurePredlssPipeline(bool aReversed);
	bool               EnsureDepthTarget(uint32_t aWidth, uint32_t aHeight);
	ID3D12CommandList* RecordPredlss(ID3D12CommandQueue* aQueue, ID3D12GraphicsCommandList* aDlssList);  // null: skipped
	FrameConstants     BaseConstants(const CameraFrame& aCamera) const;  // the camera, the atlas and the tuning values
	void               DrawSections(ID3D12GraphicsCommandList* aList, const CameraFrame& aCamera, uint64_t aCompleted);
	void               Skip(const std::string& aWhy);
	void               Fail(const std::string& aWhy);  // logs, and switches drawing off
	void               ReleaseTracking();               // if predlss asked for it
	uint64_t           Signal();
	void               Retire(gpu::ComPtr<ID3D12Resource> aResource);
	void               ReadTimings(uint32_t aSlot);

	LogQueue&             log_;
	std::filesystem::path folder_;
	mutable std::mutex    mutex_;
	std::atomic<Mode>     mode_{ Mode::Off };
	bool                  trackingRequested_ = false;

	float brightness_ = 1.0f;
	float tolerance_ = 0.0001f;
	int   cull_ = 1;  // 0 none, 1 back faces, 2 front faces
	Vec3d renderOrigin_;
	float exposure_ = 3.0f;    // predlss: what the linear colour is multiplied by; run 2 put sunlit ground at 0.6 to 1
	float jitterSign_ = 1.0f;  // predlss: 1 or -1 as the jitter's sign, 0 for none
	float mvSign_ = 1.0f;      // predlss: 1 or -1 as the motion vectors' sign, 0 for none

	ID3D12Device*                                              device_ = nullptr;  // the game's
	ID3D12CommandQueue*                                        queue_ = nullptr;   // the game's main queue
	gpu::ComPtr<ID3D12RootSignature>                           rootSignature_;
	std::optional<BlockShaders>                                shaders_;
	gpu::ComPtr<ID3D12PipelineState>                           pipeline_;
	DXGI_FORMAT                                                pipelineFormat_ = DXGI_FORMAT_UNKNOWN;
	int                                                        pipelineCull_ = -1;
	bool                                                       pipelineReversed_ = false;
	gpu::ComPtr<ID3D12PipelineState>                           predlssPipeline_;
	int                                                        predlssCull_ = -1;
	bool                                                       predlssReversed_ = false;
	gpu::ComPtr<ID3D12DescriptorHeap>                          rtvHeap_;  // the back buffer; predlss's colour and motion vectors
	gpu::ComPtr<ID3D12DescriptorHeap>                          dsvHeap_;  // our depth buffer; predlss's depth
	uint32_t                                                   rtvIncrement_ = 0;
	uint32_t                                                   dsvIncrement_ = 0;
	gpu::ComPtr<ID3D12Resource>                                depthTarget_;
	uint32_t                                                   depthWidth_ = 0;
	uint32_t                                                   depthHeight_ = 0;
	gpu::ComPtr<ID3D12Resource>                                constants_;  // upload: kSlots x 256 bytes
	uint8_t*                                                   constantsMapped_ = nullptr;
	std::array<gpu::ComPtr<ID3D12CommandAllocator>, kSlots>    allocators_;
	std::array<gpu::ComPtr<ID3D12GraphicsCommandList>, kSlots> lists_;
	std::array<uint64_t, kSlots>                               slotFence_{};
	uint64_t                                                   recordings_ = 0;  // lists of ours recorded to draw
	gpu::ComPtr<ID3D12CommandAllocator>                        uploadAllocator_;
	gpu::ComPtr<ID3D12GraphicsCommandList>                     uploadList_;
	uint64_t                                                   uploadFence_ = 0;
	gpu::ComPtr<ID3D12Fence>                                   fence_;
	uint64_t                                                   fenceValue_ = 0;
	gpu::ComPtr<ID3D12QueryHeap>                               queries_;
	gpu::ComPtr<ID3D12Resource>                                timings_;  // readback: kSlots x 2 timestamps
	uint64_t*                                                  timingsMapped_ = nullptr;
	uint64_t                                                   timestampFrequency_ = 0;

	std::optional<Atlas>                                           atlas_;
	gpu::ComPtr<ID3D12Resource>                                    atlasBuffer_;
	uint64_t                                                       atlasReady_ = 0;
	bool                                                           atlasUploaded_ = false;
	std::map<SectionPos, SectionGpu>                               sections_;
	std::vector<std::pair<SectionPos, std::vector<Vertex>>>        pending_;
	std::vector<std::pair<gpu::ComPtr<ID3D12Resource>, uint64_t>> retired_;

	gpu::ComPtr<ID3D12Resource>              depthCopy_;
	uint64_t                                 depthCopyBytes_ = 0;
	std::vector<gpu::ComPtr<ID3D12Resource>> oldDepthCopies_;  // a game list not yet submitted may still use them
	Frame                                    recorded_;        // the latest evaluate, its list not yet submitted
	std::atomic<const void*>                 recordedList_{ nullptr };
	Frame                                    submitted_;       // the latest evaluate whose list was submitted
	std::optional<uint32_t>                  lastDrawnFrame_;
	const void*                              checkedSwapChain_ = nullptr;  // compared, never used
	bool                                     swapChainOnMainQueue_ = false;
	uint32_t                                 consecutiveSkips_ = 0;

	PredlssFrame             predlss_;  // the latest evaluate, until its list is submitted
	std::atomic<const void*> predlssList_{ nullptr };

	Stats    stats_;
	double   gpuMsSum_ = 0.0;
	uint64_t gpuSamples_ = 0;
	double   frameMsSum_ = 0.0;
	uint64_t frameSamples_ = 0;
	int64_t  lastPresent_ = 0;
};
}
```

- [x] **Step 2: Write `src/render/Renderer.cpp`**

```cpp
#include "render/Renderer.h"

#include "hooks/D3D12Hooks.h"
#include "log/DdsWriter.h"
#include "log/FrameLogData.h"
#include "render/Pipeline.h"
#include "render/Predlss.h"

#include <dxgi1_4.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>

namespace cpmc
{
namespace
{
constexpr uint64_t kConstantsStride = 256;  // a constant buffer view's alignment
constexpr uint32_t kSkipsBeforeOff = 30;     // the spec's "Checks before every draw"
constexpr int64_t  kTrackedFramesAgo = 3;    // a DLSS list tracked longer ago than this was recorded again untracked

// The slot of the list ListBefore handed to this thread's ExecuteCommandLists call, for OnSubmitted to signal: the
// same thread makes both calls, while the main thread may change the mode in between.
thread_local std::optional<uint32_t> t_insertedSlot;

int64_t Now()
{
	LARGE_INTEGER now{};
	QueryPerformanceCounter(&now);
	return now.QuadPart;
}

double MsBetween(int64_t aFrom, int64_t aTo)
{
	LARGE_INTEGER frequency{};
	QueryPerformanceFrequency(&frequency);
	return double(aTo - aFrom) * 1000.0 / double(frequency.QuadPart);
}

std::optional<std::vector<uint8_t>> ReadWholeFile(const std::filesystem::path& aPath)
{
	std::ifstream in(aPath, std::ios::binary);
	if (!in)
	{
		return std::nullopt;
	}
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// False when all eight corners of the box lie beyond the same side of the view, or behind the camera.
bool Visible(const Mat4& aCameraToClip, const Vec3d& aLow, const Vec3d& aHigh)
{
	int outside[5] = {};
	for (int corner = 0; corner < 8; ++corner)
	{
		const std::array<double, 4> point{ corner & 1 ? aHigh.x : aLow.x, corner & 2 ? aHigh.y : aLow.y,
			                               corner & 4 ? aHigh.z : aLow.z, 1.0 };
		const std::array<double, 4> clip = Transform(point, aCameraToClip);
		outside[0] += clip[0] > clip[3] ? 1 : 0;
		outside[1] += clip[0] < -clip[3] ? 1 : 0;
		outside[2] += clip[1] > clip[3] ? 1 : 0;
		outside[3] += clip[1] < -clip[3] ? 1 : 0;
		outside[4] += clip[3] < 0.01 ? 1 : 0;
	}
	return std::none_of(std::begin(outside), std::end(outside), [](int aCount) { return aCount == 8; });
}

D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* aResource, D3D12_RESOURCE_STATES aBefore, D3D12_RESOURCE_STATES aAfter)
{
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = aResource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = aBefore;
	barrier.Transition.StateAfter = aAfter;
	return barrier;
}

D3D12_CPU_DESCRIPTOR_HANDLE Offset(D3D12_CPU_DESCRIPTOR_HANDLE aHandle, uint32_t aIndex, uint32_t aIncrement)
{
	return { aHandle.ptr + size_t(aIndex) * aIncrement };
}
}

Renderer::Renderer(LogQueue& aLog, std::filesystem::path aFolder)
	: log_(aLog)
	, folder_(std::move(aFolder))
{
}

bool Renderer::SetMode(std::string_view aMode, std::string& aError)
{
	if (aMode == "present" || aMode == "predlss")
	{
		if (!AtlasData() && !LoadAtlas(aError))
		{
			return false;
		}
		bool haveShaders = false;
		{
			std::lock_guard lock(mutex_);
			haveShaders = shaders_.has_value();
		}
		if (!haveShaders && !ReloadShaders(aError))
		{
			return false;
		}
	}
	else if (aMode != "off")
	{
		aError = std::format("mode {} isn't in this build: it comes with step 4", aMode);
		return false;
	}
	std::lock_guard lock(mutex_);
	recorded_ = {};
	recordedList_ = nullptr;
	submitted_ = {};
	predlss_ = {};
	predlssList_ = nullptr;
	lastDrawnFrame_.reset();
	consecutiveSkips_ = 0;
	const Mode mode = aMode == "present" ? Mode::Present : aMode == "predlss" ? Mode::Predlss : Mode::Off;
	if (mode == Mode::Predlss && !trackingRequested_)
	{
		d3d::RequestTracking();  // our barriers start from the DLSS list's first transitions
		trackingRequested_ = true;
	}
	else if (mode != Mode::Predlss)
	{
		ReleaseTracking();
	}
	mode_ = mode;
	return true;
}

std::string Renderer::ModeName() const
{
	switch (mode_.load())
	{
	case Mode::Present: return "present";
	case Mode::Predlss: return "predlss";
	default: return "off";
	}
}

bool Renderer::LoadAtlas(std::string& aError)
{
	const auto bytes = ReadWholeFile(folder_ / "atlas.bin");
	if (!bytes)
	{
		aError = "there's no atlas.bin in the plugin's folder: build it with tools/build_atlas.py";
		return false;
	}
	auto atlas = ParseAtlas(*bytes, aError);
	if (!atlas)
	{
		aError = "atlas.bin: " + aError;
		return false;
	}
	std::lock_guard lock(mutex_);
	atlas_ = std::move(*atlas);
	atlasUploaded_ = false;
	log_.Push(std::format("atlas: {} x {}, {} levels, {} blocks", atlas_->width, atlas_->height, atlas_->Levels(), atlas_->blocks.size()));
	return true;
}

const Atlas* Renderer::AtlasData() const
{
	std::lock_guard lock(mutex_);
	return atlas_ ? &*atlas_ : nullptr;
}

bool Renderer::ReloadShaders(std::string& aError)
{
	const auto bytes = ReadWholeFile(folder_ / "shaders" / "blocks.hlsl");
	if (!bytes)
	{
		aError = "there's no shaders\\blocks.hlsl in the plugin's folder";
		return false;
	}
	auto shaders = CompileBlockShaders(std::string(bytes->begin(), bytes->end()), aError);
	if (!shaders)
	{
		return false;
	}
	std::lock_guard lock(mutex_);
	shaders_ = std::move(*shaders);
	pipeline_.Reset();  // rebuilt with the new shaders at the next draw
	predlssPipeline_.Reset();
	return true;
}

bool Renderer::Set(std::string_view aName, double aValue, std::string& aError)
{
	std::lock_guard lock(mutex_);
	if (aName == "brightness" || aName == "tolerance" || aName == "exposure")
	{
		if (aValue < 0.0)
		{
			aError = std::format("{} can't be negative", aName);
			return false;
		}
		(aName == "brightness" ? brightness_ : aName == "tolerance" ? tolerance_ : exposure_) = float(aValue);
		return true;
	}
	if (aName == "jittersign" || aName == "mvsign")
	{
		if (aValue != 1.0 && aValue != -1.0 && aValue != 0.0)
		{
			aError = std::format("{} is 1, -1, or 0 for none", aName);
			return false;
		}
		(aName == "jittersign" ? jitterSign_ : mvSign_) = float(aValue);
		return true;
	}
	if (aName == "cull")
	{
		if (aValue != 0.0 && aValue != 1.0 && aValue != 2.0)
		{
			aError = "cull is 0 (none), 1 (back faces) or 2 (front faces)";
			return false;
		}
		cull_ = int(aValue);
		return true;
	}
	if (aName == "origin.x" || aName == "origin.y" || aName == "origin.z")
	{
		(aName == "origin.x" ? renderOrigin_.x : aName == "origin.y" ? renderOrigin_.y : renderOrigin_.z) = aValue;
		return true;
	}
	aError = std::format("there's no setting called \"{}\"", aName);
	return false;
}

void Renderer::UploadSection(const SectionPos& aSection, std::vector<Vertex> aVertices)
{
	std::lock_guard lock(mutex_);
	pending_.emplace_back(aSection, std::move(aVertices));
}

bool Renderer::EnsureObjects()
{
	if (fence_)
	{
		return true;
	}
	const d3d::Originals& original = d3d::Original();
	queue_ = d3d::MainQueue();
	device_ = queue_ ? d3d::Device() : nullptr;
	if (queue_ && queue_->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		Fail("DLSS runs on a compute queue, and this build draws only on a direct one");
		return false;
	}
	if (!device_ || !original.reset || !original.close || !original.executeCommandLists || !original.resourceBarrier ||
	    !original.omSetRenderTargets || !original.clearDsv || !original.drawInstanced || !original.createRtv || !original.createDsv)
	{
		return false;  // not ready yet
	}
	D3D12_ROOT_PARAMETER parameters[5]{};
	parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;  // b0: the frame
	parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	parameters[1].Constants = { 1, 0, 4 };  // b1: the section
	for (UINT i = 2; i < 5; ++i)            // t0 vertices, t1 atlas, t2 depth copy
	{
		parameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		parameters[i].Descriptor = { i - 2, 0 };
	}
	const D3D12_ROOT_SIGNATURE_DESC rootDesc{ 5, parameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE };
	gpu::ComPtr<ID3DBlob>           blob;
	gpu::ComPtr<ID3DBlob>           error;
	if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error)) ||
	    FAILED(device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_))))
	{
		Fail("couldn't create the root signature");
		return false;
	}
	const D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{ D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 3, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
	const D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{ D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 2, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
	const D3D12_QUERY_HEAP_DESC      queryDesc{ D3D12_QUERY_HEAP_TYPE_TIMESTAMP, kSlots * 2, 0 };
	constants_ = gpu::MakeBuffer(device_, kSlots * kConstantsStride, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	timings_ = gpu::MakeBuffer(device_, kSlots * 2 * sizeof(uint64_t), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
	if (FAILED(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_))) ||
	    FAILED(device_->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap_))) ||
	    FAILED(device_->CreateQueryHeap(&queryDesc, IID_PPV_ARGS(&queries_))) || !constants_ || !timings_ ||
	    FAILED(constants_->Map(0, nullptr, reinterpret_cast<void**>(&constantsMapped_))) ||
	    FAILED(timings_->Map(0, nullptr, reinterpret_cast<void**>(&timingsMapped_))) ||
	    FAILED(queue_->GetTimestampFrequency(&timestampFrequency_)))
	{
		Fail("couldn't create its heaps and buffers");
		return false;
	}
	rtvIncrement_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	dsvIncrement_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	for (uint32_t slot = 0; slot < kSlots; ++slot)
	{
		if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocators_[slot]))) ||
		    FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocators_[slot].Get(), nullptr,
		                                      IID_PPV_ARGS(&lists_[slot]))))
		{
			Fail("couldn't create its command lists");
			return false;
		}
		original.close(lists_[slot].Get());
		lists_[slot]->SetName(L"BlockRender blocks");
	}
	gpu::ComPtr<ID3D12Fence> fence;
	if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAllocator_))) ||
	    FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, uploadAllocator_.Get(), nullptr, IID_PPV_ARGS(&uploadList_))) ||
	    FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
	{
		Fail("couldn't create its upload list and fence");
		return false;
	}
	original.close(uploadList_.Get());
	uploadList_->SetName(L"BlockRender uploads");
	rootSignature_->SetName(L"BlockRender root signature");
	constants_->SetName(L"BlockRender constants");
	timings_->SetName(L"BlockRender timings");
	fence->SetName(L"BlockRender fence");
	fence_ = fence;  // last: a fence means everything is ready
	log_.Push("renderer: ready");
	return true;
}

bool Renderer::EnsurePipeline(DXGI_FORMAT aFormat, bool aReversed)
{
	if (pipeline_ && pipelineFormat_ == aFormat && pipelineCull_ == cull_ && pipelineReversed_ == aReversed)
	{
		return true;
	}
	if (!shaders_)
	{
		Fail("there are no shaders");
		return false;
	}
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.VS = { shaders_->vertex.data(), shaders_->vertex.size() };
	desc.PS = { shaders_->present.data(), shaders_->present.size() };
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = BlockRasterizer(cull_);
	desc.DepthStencilState.DepthEnable = TRUE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthStencilState.DepthFunc = aReversed ? D3D12_COMPARISON_FUNC_GREATER : D3D12_COMPARISON_FUNC_LESS;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = aFormat;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	desc.SampleDesc.Count = 1;
	gpu::ComPtr<ID3D12PipelineState> pipeline;
	if (FAILED(device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline))))
	{
		Fail(std::format("couldn't create a pipeline for {}", DxgiFormatName(aFormat)));
		return false;
	}
	pipeline->SetName(L"BlockRender present pipeline");
	pipeline_ = pipeline;
	pipelineFormat_ = aFormat;
	pipelineCull_ = cull_;
	pipelineReversed_ = aReversed;
	return true;
}

bool Renderer::EnsurePredlssPipeline(bool aReversed)
{
	if (predlssPipeline_ && predlssCull_ == cull_ && predlssReversed_ == aReversed)
	{
		return true;
	}
	if (!shaders_)
	{
		Fail("there are no shaders");
		return false;
	}
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.VS = { shaders_->vertex.data(), shaders_->vertex.size() };
	desc.PS = { shaders_->predlss.data(), shaders_->predlss.size() };
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;  // no independent blend: both
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = BlockRasterizer(cull_);
	desc.DepthStencilState.DepthEnable = TRUE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthStencilState.DepthFunc = aReversed ? D3D12_COMPARISON_FUNC_GREATER : D3D12_COMPARISON_FUNC_LESS;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 2;  // the scaling input colour, and the motion vectors
	desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.RTVFormats[1] = DXGI_FORMAT_R16G16_FLOAT;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	desc.SampleDesc.Count = 1;
	gpu::ComPtr<ID3D12PipelineState> pipeline;
	if (FAILED(device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline))))
	{
		Fail("couldn't create the pipeline for drawing before DLSS");
		return false;
	}
	pipeline->SetName(L"BlockRender predlss pipeline");
	predlssPipeline_ = pipeline;
	predlssCull_ = cull_;
	predlssReversed_ = aReversed;
	return true;
}

bool Renderer::EnsureDepthTarget(uint32_t aWidth, uint32_t aHeight)
{
	if (depthTarget_ && depthWidth_ == aWidth && depthHeight_ == aHeight)
	{
		return true;
	}
	if (depthTarget_)
	{
		Retire(depthTarget_);
		depthTarget_.Reset();
	}
	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = aWidth;
	desc.Height = aHeight;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_D32_FLOAT;
	desc.SampleDesc.Count = 1;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE clear{};
	clear.Format = DXGI_FORMAT_D32_FLOAT;
	if (FAILED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear,
	                                            IID_PPV_ARGS(&depthTarget_))))
	{
		Fail("couldn't create its depth buffer");
		return false;
	}
	depthTarget_->SetName(L"BlockRender depth");
	d3d::Original().createDsv(device_, depthTarget_.Get(), nullptr, dsvHeap_->GetCPUDescriptorHandleForHeapStart());
	depthWidth_ = aWidth;
	depthHeight_ = aHeight;
	return true;
}

void Renderer::Skip(const std::string& aWhy)
{
	++stats_.skipped;
	stats_.lastSkip = aWhy;
	if (++consecutiveSkips_ == kSkipsBeforeOff)
	{
		Fail(std::format("{} frames in a row were skipped ({})", kSkipsBeforeOff, aWhy));
	}
}

void Renderer::Fail(const std::string& aWhy)
{
	log_.Push("renderer: " + aWhy + ", so drawing is off");
	stats_.lastSkip = aWhy;
	mode_ = Mode::Off;
	ReleaseTracking();
}

void Renderer::ReleaseTracking()
{
	if (trackingRequested_)
	{
		d3d::ReleaseTracking();
		trackingRequested_ = false;
	}
}

uint64_t Renderer::Signal()
{
	queue_->Signal(fence_.Get(), ++fenceValue_);
	return fenceValue_;
}

void Renderer::Retire(gpu::ComPtr<ID3D12Resource> aResource)
{
	// Every list of ours that used it has been submitted, and the fence was signalled after each.
	retired_.emplace_back(std::move(aResource), fenceValue_);
}

void Renderer::ReadTimings(uint32_t aSlot)
{
	const uint64_t begin = timingsMapped_[aSlot * 2];
	const uint64_t end = timingsMapped_[aSlot * 2 + 1];
	if (end > begin && timestampFrequency_ > 0)
	{
		const double ms = double(end - begin) * 1000.0 / double(timestampFrequency_);
		gpuMsSum_ += ms;
		++gpuSamples_;
		stats_.gpuMsWorst = std::max(stats_.gpuMsWorst, ms);
	}
}

void Renderer::Update()
{
	std::lock_guard lock(mutex_);
	if ((!fence_ && mode_.load() == Mode::Off) || !EnsureObjects())
	{
		return;  // nothing is created until drawing is first switched on
	}
	const uint64_t completed = fence_->GetCompletedValue();
	if (completed == UINT64_MAX)
	{
		return;  // the device was removed: the plugin reports it
	}
	std::erase_if(retired_, [completed](const auto& aEntry) { return aEntry.second <= completed; });
	const bool atlasDue = atlas_ && !atlasUploaded_;
	if ((pending_.empty() && !atlasDue) || completed < uploadFence_)
	{
		return;
	}
	const d3d::Originals& original = d3d::Original();
	if (FAILED(uploadAllocator_->Reset()) || FAILED(original.reset(uploadList_.Get(), uploadAllocator_.Get(), nullptr)))
	{
		Fail("couldn't reset its upload list");
		return;
	}
	const uint64_t                           ready = fenceValue_ + 1;  // the value the Signal below sets
	std::vector<gpu::ComPtr<ID3D12Resource>> staging;
	bool                                     failed = false;
	const auto upload = [&](const void* aData, uint64_t aBytes) -> gpu::ComPtr<ID3D12Resource> {
		auto  target = gpu::MakeBuffer(device_, aBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
		auto  source = gpu::MakeBuffer(device_, aBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
		void* mapped = nullptr;
		if (!target || !source || FAILED(source->Map(0, nullptr, &mapped)))
		{
			failed = true;
			return nullptr;
		}
		std::memcpy(mapped, aData, aBytes);
		source->Unmap(0, nullptr);
		uploadList_->CopyBufferRegion(target.Get(), 0, source.Get(), 0, aBytes);
		staging.push_back(source);
		return target;
	};
	if (atlasDue)
	{
		if (atlasBuffer_)
		{
			Retire(atlasBuffer_);
		}
		atlasBuffer_ = upload(atlas_->texels.data(), atlas_->texels.size() * sizeof(uint32_t));
		if (atlasBuffer_)
		{
			atlasBuffer_->SetName(L"BlockRender atlas");
		}
		atlasReady_ = ready;
		atlasUploaded_ = true;
	}
	for (auto& [section, vertices] : pending_)
	{
		if (const auto found = sections_.find(section); found != sections_.end())
		{
			Retire(found->second.buffer);
			sections_.erase(found);
		}
		if (vertices.empty())
		{
			continue;
		}
		SectionGpu gpuSection;
		gpuSection.buffer = upload(vertices.data(), vertices.size() * sizeof(Vertex));
		gpuSection.vertices = uint32_t(vertices.size());
		gpuSection.ready = ready;
		if (gpuSection.buffer)
		{
			gpuSection.buffer->SetName(L"BlockRender section");
			sections_[section] = std::move(gpuSection);
		}
	}
	pending_.clear();
	original.close(uploadList_.Get());
	ID3D12CommandList* lists[] = { uploadList_.Get() };
	original.executeCommandLists(queue_, 1, lists);
	uploadFence_ = Signal();
	for (auto& buffer : staging)
	{
		retired_.emplace_back(std::move(buffer), uploadFence_);
	}
	if (failed)
	{
		log_.Push("renderer: couldn't create some upload buffers; those sections aren't drawn");
	}
}

Renderer::Stats Renderer::TakeStats()
{
	std::lock_guard lock(mutex_);
	Stats stats = stats_;
	stats.gpuMsAverage = gpuSamples_ ? gpuMsSum_ / double(gpuSamples_) : -1.0;
	stats.frameMsAverage = frameSamples_ ? frameMsSum_ / double(frameSamples_) : -1.0;
	stats.sections = sections_.size();
	for (const auto& [section, gpuSection] : sections_)
	{
		stats.vertices += gpuSection.vertices;
	}
	stats_ = {};
	gpuMsSum_ = frameMsSum_ = 0.0;
	gpuSamples_ = frameSamples_ = 0;
	return stats;
}

FrameConstants Renderer::BaseConstants(const CameraFrame& aCamera) const
{
	FrameConstants constants{};
	for (size_t i = 0; i < 16; ++i)
	{
		constants.cameraToClip[i] = float(aCamera.cameraToClip[i]);
	}
	constants.atlasInfo[0] = atlas_->width;
	constants.atlasInfo[1] = atlas_->height;
	constants.atlasInfo[2] = atlas_->Levels();
	for (size_t i = 0; i < atlas_->levelOffsets.size() && i < 8; ++i)
	{
		constants.atlasLevels[i] = atlas_->levelOffsets[i];
	}
	constants.tuning[0] = brightness_;
	constants.tuning[1] = tolerance_;
	constants.tuning[2] = exposure_;
	constants.tuning[3] = mvSign_;
	return constants;
}

void Renderer::DrawSections(ID3D12GraphicsCommandList* aList, const CameraFrame& aCamera, uint64_t aCompleted)
{
	for (const auto& [section, gpuSection] : sections_)
	{
		if (gpuSection.ready > aCompleted)
		{
			continue;  // still uploading
		}
		const Vec3d corner = WorldFromMc({ section.x * 16.0, section.y * 16.0, section.z * 16.0 });
		const Vec3d offset{ corner.x - aCamera.origin.x, corner.y - aCamera.origin.y, corner.z - aCamera.origin.z };
		// The section fills world x..x+16, y-16..y and z..z+16 from that corner, since Minecraft z runs along world -y.
		if (!Visible(aCamera.cameraToClip, { offset.x, offset.y - 16.0, offset.z }, { offset.x + 16.0, offset.y, offset.z + 16.0 }))
		{
			continue;
		}
		const float sectionConstants[4] = { float(offset.x), float(offset.y), float(offset.z), 0.0f };
		aList->SetGraphicsRoot32BitConstants(1, 4, sectionConstants, 0);
		aList->SetGraphicsRootShaderResourceView(2, gpuSection.buffer->GetGPUVirtualAddress());
		d3d::Original().drawInstanced(aList, gpuSection.vertices, 1, 0, 0);
	}
}

void Renderer::OnEvaluate(const EvaluateCall& aCall)
{
	const Mode mode = mode_.load();
	if (mode == Mode::Predlss)
	{
		std::lock_guard lock(mutex_);
		predlss_ = {};
		predlss_.list = aCall.list;
		predlss_.constants = aCall.constants;
		for (const SlTag& tag : aCall.tags)
		{
			if (tag.type == sl::kBufferTypeScalingInputColor)
			{
				predlss_.colour = tag;
			}
			else if (tag.type == sl::kBufferTypeMotionVectors)
			{
				predlss_.motion = tag;
			}
			else if (tag.type == sl::kBufferTypeDepth)
			{
				predlss_.depth = tag;
			}
		}
		predlssList_ = aCall.list;
		return;
	}
	if (mode != Mode::Present)
	{
		return;
	}
	std::lock_guard lock(mutex_);
	if (mode_.load() != Mode::Present || !aCall.list || !EnsureObjects())
	{
		return;
	}
	recorded_ = {};
	recorded_.list = aCall.list;
	recorded_.frame = aCall.frame;
	recorded_.valid = aCall.constants.has_value();
	if (aCall.constants)
	{
		recorded_.constants = *aCall.constants;
	}
	recordedList_ = aCall.list;
	const SlTag* depth = nullptr;
	for (const SlTag& tag : aCall.tags)
	{
		if (tag.type == sl::kBufferTypeDepth && tag.resource && tag.state != 0xFFFFFFFF)
		{
			depth = &tag;
		}
	}
	if (!depth)
	{
		return;  // drawn without the game's depth
	}
	auto*                     resource = static_cast<ID3D12Resource*>(depth->resource);
	const D3D12_RESOURCE_DESC desc = resource->GetDesc();
	if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || DumpFormat(desc.Format, 0) != uint32_t(DXGI_FORMAT_R32_FLOAT))
	{
		return;
	}
	const uint32_t       subresource = gpu::PlaneSubresource(desc, 0);
	const gpu::Footprint footprint = gpu::FootprintOf(device_, desc, subresource);
	if (!depthCopy_ || depthCopyBytes_ < footprint.totalBytes)
	{
		if (depthCopy_)
		{
			oldDepthCopies_.push_back(depthCopy_);
		}
		depthCopy_ = gpu::MakeBuffer(device_, footprint.totalBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
		depthCopyBytes_ = depthCopy_ ? footprint.totalBytes : 0;
		if (!depthCopy_)
		{
			Fail("couldn't create the depth copy");
			return;
		}
		depthCopy_->SetName(L"BlockRender depth copy");
	}
	if (!gpu::RecordCopy(aCall.list, resource, subresource, depth->state, depthCopy_.Get(), footprint))
	{
		return;  // drawn without the game's depth
	}
	recorded_.depth = true;
	recorded_.rowFloats = footprint.layout.Footprint.RowPitch / 4;
	const bool wholeResource = depth->extent.width == 0 || depth->extent.height == 0;
	recorded_.extent = wholeResource ? sl::Extent{ 0, 0, uint32_t(desc.Width), desc.Height } : depth->extent;
}

void Renderer::OnSubmit(ID3D12CommandQueue*, UINT aCount, ID3D12CommandList* const* aLists)
{
	const void* recorded = recordedList_.load();
	if (!recorded)
	{
		return;
	}
	for (UINT i = 0; i < aCount; ++i)
	{
		if (static_cast<const void*>(aLists[i]) == recorded)
		{
			std::lock_guard lock(mutex_);
			if (recorded_.list == recorded)
			{
				submitted_ = recorded_;
				recorded_ = {};
				recordedList_ = nullptr;
			}
			return;
		}
	}
}

ID3D12CommandList* Renderer::ListBefore(ID3D12CommandQueue* aQueue, ID3D12CommandList* aList)
{
	if (static_cast<const void*>(aList) != predlssList_.load())
	{
		return nullptr;  // most lists: no lock taken
	}
	std::lock_guard lock(mutex_);
	if (mode_.load() != Mode::Predlss || predlss_.list != aList)
	{
		return nullptr;
	}
	ID3D12CommandList* ours = RecordPredlss(aQueue, static_cast<ID3D12GraphicsCommandList*>(aList));
	predlss_ = {};  // each evaluate draws once
	predlssList_ = nullptr;
	return ours;
}

ID3D12CommandList* Renderer::RecordPredlss(ID3D12CommandQueue* aQueue, ID3D12GraphicsCommandList* aDlssList)
{
	if (!EnsureObjects())
	{
		Skip("not ready: the main queue isn't known yet");
		return nullptr;
	}
	const uint64_t completed = fence_->GetCompletedValue();
	if (completed == UINT64_MAX)
	{
		Fail(std::format("the device was removed (0x{:08x})", uint32_t(device_->GetDeviceRemovedReason())));
		return nullptr;
	}
	if (aQueue != queue_)
	{
		Skip("the DLSS list went to another queue than the main one");
		return nullptr;
	}
	if (!predlss_.constants)
	{
		Skip("the DLSS frame came without constants");
		return nullptr;
	}
	auto* colour = static_cast<ID3D12Resource*>(predlss_.colour.resource);
	auto* motion = static_cast<ID3D12Resource*>(predlss_.motion.resource);
	auto* depth = static_cast<ID3D12Resource*>(predlss_.depth.resource);
	if (!colour || !motion || !depth)
	{
		Skip("the scaling input colour, motion vectors and depth aren't all tagged");
		return nullptr;
	}
	const D3D12_RESOURCE_DESC colourDesc = colour->GetDesc();
	const D3D12_RESOURCE_DESC motionDesc = motion->GetDesc();
	const D3D12_RESOURCE_DESC depthDesc = depth->GetDesc();
	std::string               why;
	if (!CheckPredlssTargets(colourDesc, motionDesc, depthDesc, why))
	{
		Skip(why);
		return nullptr;
	}
	const ListState* dlss = d3d::Tracker().Find(aDlssList);
	if (!dlss || dlss->Recording() || int64_t(d3d::CurrentFrame()) - int64_t(dlss->Frame()) > kTrackedFramesAgo)
	{
		Skip("the DLSS list wasn't tracked from its Reset");
		return nullptr;
	}
	const std::optional<uint32_t> colourState = StateBeforeList(*dlss, colour, predlss_.colour.state);
	const std::optional<uint32_t> motionState = StateBeforeList(*dlss, motion, predlss_.motion.state);
	const std::optional<uint32_t> depthState = StateBeforeList(*dlss, depth, predlss_.depth.state);
	if (!colourState || !motionState || !depthState)
	{
		Skip("a tagged buffer's state before the DLSS list isn't known");
		return nullptr;
	}
	if (!atlasBuffer_ || completed < atlasReady_)
	{
		Skip("the atlas isn't on the GPU yet");
		return nullptr;
	}
	const std::optional<CameraFrame> camera = MakeCameraFrame(*predlss_.constants, renderOrigin_);
	if (!camera)
	{
		Skip("the camera isn't understood");
		return nullptr;
	}
	if (!EnsurePredlssPipeline(camera->reversedDepth))
	{
		return nullptr;
	}
	const uint32_t slot = uint32_t(recordings_ % kSlots);
	if (completed < slotFence_[slot])
	{
		Skip("the GPU is still busy with an earlier frame");
		return nullptr;
	}
	if (slotFence_[slot] != 0)
	{
		ReadTimings(slot);
	}

	const bool        wholeResource = predlss_.depth.extent.width == 0 || predlss_.depth.extent.height == 0;
	const sl::Extent  extent = wholeResource ? sl::Extent{ 0, 0, uint32_t(depthDesc.Width), depthDesc.Height } : predlss_.depth.extent;
	const SlConstants& game = *predlss_.constants;
	FrameConstants     constants = BaseConstants(*camera);
	constants.target[0] = float(extent.width);
	constants.target[1] = float(extent.height);
	constants.target[2] = 1.0f / float(extent.width);
	constants.target[3] = 1.0f / float(extent.height);
	for (size_t i = 0; i < 16; ++i)
	{
		constants.clipToPrevClip[i] = game.clipToPrevClip[i];
	}
	const std::array<double, 2> jitter = ClipJitter(game.jitter[0], game.jitter[1], extent.width, extent.height, jitterSign_);
	constants.jitter[0] = float(jitter[0]);
	constants.jitter[1] = float(jitter[1]);
	std::memcpy(constantsMapped_ + slot * kConstantsStride, &constants, sizeof(constants));

	// Our own list: calls the hooks watch go through their originals, so nothing of ours reaches the tracker.
	const d3d::Originals&      original = d3d::Original();
	ID3D12GraphicsCommandList* list = lists_[slot].Get();
	if (FAILED(allocators_[slot]->Reset()) || FAILED(original.reset(list, allocators_[slot].Get(), predlssPipeline_.Get())))
	{
		Fail("couldn't reset a command list");
		return nullptr;
	}
	list->EndQuery(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2);
	const std::vector<Move> into = MovesInto({ { colour, *colourState, D3D12_RESOURCE_STATE_RENDER_TARGET },
	                                           { motion, *motionState, D3D12_RESOURCE_STATE_RENDER_TARGET },
	                                           { depth, *depthState, D3D12_RESOURCE_STATE_DEPTH_WRITE } });
	const std::vector<D3D12_RESOURCE_BARRIER> toTargets = Barriers(into);
	if (!toTargets.empty())
	{
		original.resourceBarrier(list, UINT(toTargets.size()), toTargets.data());
	}
	const D3D12_CPU_DESCRIPTOR_HANDLE rtvs[2] = { Offset(rtvHeap_->GetCPUDescriptorHandleForHeapStart(), 1, rtvIncrement_),
		                                          Offset(rtvHeap_->GetCPUDescriptorHandleForHeapStart(), 2, rtvIncrement_) };
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv = Offset(dsvHeap_->GetCPUDescriptorHandleForHeapStart(), 1, dsvIncrement_);
	D3D12_RENDER_TARGET_VIEW_DESC     colourView{};
	colourView.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	colourView.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	D3D12_RENDER_TARGET_VIEW_DESC motionView = colourView;
	motionView.Format = DXGI_FORMAT_R16G16_FLOAT;
	D3D12_DEPTH_STENCIL_VIEW_DESC depthView{};
	depthView.Format = DepthViewFormat(depthDesc.Format);
	depthView.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	original.createRtv(device_, colour, &colourView, rtvs[0]);
	original.createRtv(device_, motion, &motionView, rtvs[1]);
	original.createDsv(device_, depth, &depthView, dsv);
	original.omSetRenderTargets(list, 2, rtvs, FALSE, &dsv);
	const D3D12_VIEWPORT viewport{ float(extent.left), float(extent.top), float(extent.width), float(extent.height), 0.0f, 1.0f };
	const D3D12_RECT     scissor{ LONG(extent.left), LONG(extent.top), LONG(extent.left + extent.width), LONG(extent.top + extent.height) };
	list->RSSetViewports(1, &viewport);
	list->RSSetScissorRects(1, &scissor);
	list->SetGraphicsRootSignature(rootSignature_.Get());
	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	list->SetGraphicsRootConstantBufferView(0, constants_->GetGPUVirtualAddress() + slot * kConstantsStride);
	list->SetGraphicsRootShaderResourceView(3, atlasBuffer_->GetGPUVirtualAddress());
	list->SetGraphicsRootShaderResourceView(4, atlasBuffer_->GetGPUVirtualAddress());  // t2: only step 2's shader reads it
	DrawSections(list, *camera, completed);
	const std::vector<D3D12_RESOURCE_BARRIER> back = Barriers(MovesBack(into));
	if (!back.empty())
	{
		original.resourceBarrier(list, UINT(back.size()), back.data());
	}
	list->EndQuery(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2 + 1);
	list->ResolveQueryData(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2, 2, timings_.Get(), slot * 2 * sizeof(uint64_t));
	if (FAILED(original.close(list)))
	{
		Fail("couldn't close a command list");
		return nullptr;
	}
	++recordings_;
	t_insertedSlot = slot;
	return list;
}

void Renderer::OnSubmitted(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*)
{
	if (!t_insertedSlot)
	{
		return;
	}
	const uint32_t  slot = *t_insertedSlot;
	t_insertedSlot.reset();
	std::lock_guard lock(mutex_);
	slotFence_[slot] = Signal();  // signalled whatever the mode is now: our list went to the GPU
	++stats_.drawn;
	consecutiveSkips_ = 0;
}

void Renderer::OnPresent(IDXGISwapChain* aSwapChain)
{
	const int64_t   now = Now();
	std::lock_guard lock(mutex_);
	if (lastPresent_ != 0)
	{
		const double ms = MsBetween(lastPresent_, now);
		frameMsSum_ += ms;
		++frameSamples_;
		stats_.frameMsWorst = std::max(stats_.frameMsWorst, ms);
	}
	lastPresent_ = now;
	if (mode_.load() != Mode::Present)
	{
		return;
	}
	if (!EnsureObjects())
	{
		return Skip("not ready: the main queue isn't known yet");
	}
	const uint64_t completed = fence_->GetCompletedValue();
	if (completed == UINT64_MAX)
	{
		return Fail(std::format("the device was removed (0x{:08x})", uint32_t(device_->GetDeviceRemovedReason())));
	}
	if (aSwapChain != checkedSwapChain_)
	{
		checkedSwapChain_ = aSwapChain;
		swapChainOnMainQueue_ = d3d::PresentQueue(aSwapChain) == queue_;
		log_.Push(std::format("renderer: swap chain 0x{:x} presents on {}", reinterpret_cast<uintptr_t>(aSwapChain),
		                      swapChainOnMainQueue_ ? "the main queue" : "another queue"));
	}
	if (!swapChainOnMainQueue_)
	{
		return Skip("the swap chain presents on another queue than DLSS's");
	}
	if (!submitted_.list)
	{
		return Skip("no DLSS frame has been submitted yet");
	}
	if (!submitted_.valid)
	{
		return Skip("the DLSS frame came without constants");
	}
	if (!atlasBuffer_ || completed < atlasReady_)
	{
		return Skip("the atlas isn't on the GPU yet");
	}
	const std::optional<CameraFrame> camera = MakeCameraFrame(submitted_.constants, renderOrigin_);
	if (!camera)
	{
		return Skip("the camera isn't understood");
	}
	gpu::ComPtr<IDXGISwapChain3> swapChain;
	gpu::ComPtr<ID3D12Resource>  backBuffer;
	if (FAILED(aSwapChain->QueryInterface(IID_PPV_ARGS(&swapChain))) ||
	    FAILED(swapChain->GetBuffer(swapChain->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&backBuffer))))
	{
		return Skip("no back buffer");
	}
	const D3D12_RESOURCE_DESC target = backBuffer->GetDesc();
	if (!EnsurePipeline(target.Format, camera->reversedDepth) || !EnsureDepthTarget(uint32_t(target.Width), target.Height))
	{
		return;
	}
	const uint32_t slot = uint32_t(recordings_ % kSlots);
	if (completed < slotFence_[slot])
	{
		return Skip("the GPU is still busy with an earlier frame");
	}
	if (slotFence_[slot] != 0)
	{
		ReadTimings(slot);
	}

	FrameConstants constants = BaseConstants(*camera);  // no jitter: the image is already resolved
	constants.target[0] = float(target.Width);
	constants.target[1] = float(target.Height);
	constants.target[2] = 1.0f / float(target.Width);
	constants.target[3] = 1.0f / float(target.Height);
	constants.depthExtent[0] = float(submitted_.extent.left);
	constants.depthExtent[1] = float(submitted_.extent.top);
	constants.depthExtent[2] = float(submitted_.extent.width);
	constants.depthExtent[3] = float(submitted_.extent.height);
	constants.depthInfo[0] = submitted_.rowFloats;
	constants.depthInfo[1] = submitted_.depth ? 1u : 0u;
	constants.depthInfo[2] = camera->reversedDepth ? 1u : 0u;
	std::memcpy(constantsMapped_ + slot * kConstantsStride, &constants, sizeof(constants));

	// Our own list: calls the hooks watch go through their originals, so nothing of ours reaches the tracker.
	const d3d::Originals&      original = d3d::Original();
	ID3D12GraphicsCommandList* list = lists_[slot].Get();
	if (FAILED(allocators_[slot]->Reset()) || FAILED(original.reset(list, allocators_[slot].Get(), pipeline_.Get())))
	{
		return Fail("couldn't reset a command list");
	}
	list->EndQuery(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2);
	const D3D12_RESOURCE_BARRIER toTarget = Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	original.resourceBarrier(list, 1, &toTarget);
	const D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
	original.createRtv(device_, backBuffer.Get(), nullptr, rtv);
	original.clearDsv(list, dsv, D3D12_CLEAR_FLAG_DEPTH, camera->reversedDepth ? 0.0f : 1.0f, 0, 0, nullptr);
	original.omSetRenderTargets(list, 1, &rtv, FALSE, &dsv);
	const D3D12_VIEWPORT viewport{ 0.0f, 0.0f, float(target.Width), float(target.Height), 0.0f, 1.0f };
	const D3D12_RECT     scissor{ 0, 0, LONG(target.Width), LONG(target.Height) };
	list->RSSetViewports(1, &viewport);
	list->RSSetScissorRects(1, &scissor);
	list->SetGraphicsRootSignature(rootSignature_.Get());
	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	list->SetGraphicsRootConstantBufferView(0, constants_->GetGPUVirtualAddress() + slot * kConstantsStride);
	list->SetGraphicsRootShaderResourceView(3, atlasBuffer_->GetGPUVirtualAddress());
	// Without a depth copy the shader never reads t2, but the root argument still needs a valid buffer.
	list->SetGraphicsRootShaderResourceView(4, (submitted_.depth ? depthCopy_ : atlasBuffer_)->GetGPUVirtualAddress());
	DrawSections(list, *camera, completed);
	const D3D12_RESOURCE_BARRIER toPresent = Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	original.resourceBarrier(list, 1, &toPresent);
	list->EndQuery(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2 + 1);
	list->ResolveQueryData(queries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2, 2, timings_.Get(), slot * 2 * sizeof(uint64_t));
	if (FAILED(original.close(list)))
	{
		return Fail("couldn't close a command list");
	}
	ID3D12CommandList* lists[] = { list };
	original.executeCommandLists(queue_, 1, lists);
	slotFence_[slot] = Signal();
	++recordings_;
	++stats_.drawn;
	if (lastDrawnFrame_ == submitted_.frame)
	{
		++stats_.lateCamera;
	}
	lastDrawnFrame_ = submitted_.frame;
	consecutiveSkips_ = 0;
}

void Renderer::Shutdown()
{
	mode_ = Mode::Off;
	std::lock_guard lock(mutex_);
	ReleaseTracking();
	if (fence_ && fence_->GetCompletedValue() < fenceValue_)
	{
		const HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (event && SUCCEEDED(fence_->SetEventOnCompletion(fenceValue_, event)))
		{
			WaitForSingleObject(event, 2000);
		}
		if (event)
		{
			CloseHandle(event);
		}
	}
	if (constantsMapped_)
	{
		constants_->Unmap(0, nullptr);
		constantsMapped_ = nullptr;
	}
	if (timingsMapped_)
	{
		timings_->Unmap(0, nullptr);
		timingsMapped_ = nullptr;
	}
	sections_.clear();
	pending_.clear();
	retired_.clear();
	oldDepthCopies_.clear();
	lists_ = {};
	allocators_ = {};
	depthCopy_.Reset();
	atlasBuffer_.Reset();
	depthTarget_.Reset();
	pipeline_.Reset();
	predlssPipeline_.Reset();
	rootSignature_.Reset();
	rtvHeap_.Reset();
	dsvHeap_.Reset();
	constants_.Reset();
	timings_.Reset();
	queries_.Reset();
	uploadList_.Reset();
	uploadAllocator_.Reset();
	fence_.Reset();
}
}
```

- [x] **Step 3: Build; it must compile without warnings, and the tests still pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `101 test(s), 0 failure(s)`.

- [x] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: the renderer draws before DLSS (mode predlss)" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 43: Version 0.3.0, and the overlay's own frame time

The plugin needs nothing more for step 3: `mode predlss` and the `exposure`, `jittersign` and `mvsign` settings go
straight to the renderer, and `place house` and `place field` came with Task 40. Two last things:
- **The version:** 0.3.0.
- **The overlay's own frame time:** step 3 measures what the hooks cost against the game without BlockRender, which
  then writes no status file. So the overlay times frames itself, from CET's `onUpdate`. Once a second it shows the
  average and the worst, with or without the plugin, and writes them into `frames.txt` in its folder, for the agent
  to read.

**Files:**
- Modify: `CMakeLists.txt`, `src/Plugin.cpp`, `tools/blockrender_hud/init.lua`

- [x] **Step 1: Edit `CMakeLists.txt` for the version**

```cmake
project(BlockRender VERSION 0.2.0 LANGUAGES C CXX)
```
```cmake
project(BlockRender VERSION 0.3.0 LANGUAGES C CXX)
```

- [x] **Step 2: Edit `src/Plugin.cpp` for the version**

```cpp
// (step 1: the frame log and the dumps), and draws Minecraft blocks into it (step 2: on the finished frame). Commands
// come through blockrender.request in its folder, and status.txt in the overlay's folder says what it's doing. See
// docs/superpowers/specs/2026-10-01-block-render-design.md.
```
```cpp
// (step 1: the frame log and the dumps), and draws Minecraft blocks into it (step 2: on the finished frame; step 3:
// before DLSS). Commands come through blockrender.request in its folder, and status.txt in the overlay's folder says
// what it's doing. See docs/superpowers/specs/2026-10-01-block-render-design.md.
```
```cpp
constexpr const char* kVersion = "0.2.0";
```
```cpp
constexpr const char* kVersion = "0.3.0";
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 2, 0);
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 3, 0);
```

- [x] **Step 3: Edit `tools/blockrender_hud/init.lua` to time frames**

```lua
-- this folder once a second, plus V's position, heading and Minecraft block, and the nearest structure. Read-only.
-- Delete this folder to remove it.
```
```lua
-- this folder once a second, plus V's position, heading and Minecraft block, and the nearest structure, and the frame
-- time as CET sees it, which works without BlockRender too and goes into frames.txt here as well. Delete this folder
-- to remove it.
```
```lua
local sinceRead = 1.0
```
```lua
local sinceRead = 1.0
local frames = { sum = 0.0, count = 0, worst = 0.0 }  -- this second's frame times, from onUpdate
local frameLine = nil                                  -- the last whole second's
```
```lua
registerForEvent("onUpdate", function(delta)
    sinceRead = sinceRead + delta
    if sinceRead >= 1.0 then
        sinceRead = 0.0
        readStatus()
    end
```
```lua
registerForEvent("onUpdate", function(delta)
    frames.sum = frames.sum + delta
    frames.count = frames.count + 1
    frames.worst = math.max(frames.worst, delta)
    sinceRead = sinceRead + delta
    if sinceRead >= 1.0 then
        sinceRead = 0.0
        readStatus()
        local average = frames.sum / frames.count
        frameLine = string.format("CET's frame time: %.1f ms average (%.1f fps), %.1f ms worst", average * 1000.0,
            1.0 / average, frames.worst * 1000.0)
        frames = { sum = 0.0, count = 0, worst = 0.0 }
        local out = io.open("frames.txt", "w")
        if out ~= nil then
            out:write(frameLine, "\n")
            out:close()
        end
    end
```
```lua
        if player ~= nil then
            ImGui.Separator()
            ImGui.Text(player)
        end
```
```lua
        if frameLine ~= nil then
            ImGui.Text(frameLine)
        end
        if player ~= nil then
            ImGui.Separator()
            ImGui.Text(player)
        end
```

- [x] **Step 4: Build; it must compile without warnings, and the tests still pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `101 test(s), 0 failure(s)`.

- [x] **Step 5: Check the DLL's exports and imports**

```bash
DUMPBIN="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/dumpbin.exe"
B=/d/Cyberpunk-Minecraft/spikes/block-render/build/RelWithDebInfo/BlockRender.dll
"$DUMPBIN" -nologo -exports "$B" | grep -E " (Main|Query|Supports) = "; \
  "$DUMPBIN" -nologo -dependents "$B" | grep -iE "d3d|dxgi|sl\."
```
Expected: exactly `Main`, `Query` and `Supports`, then `D3DCOMPILER_47.dll` and `d3d12.dll`.

- [x] **Step 6: Check the overlay's Lua**

```bash
luac -p /d/Cyberpunk-Minecraft/spikes/block-render/tools/blockrender_hud/init.lua && echo "syntax ok"
```
Expected: `syntax ok`.

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: version 0.3.0, and the overlay times frames itself" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 44: In the game: step 3 (blocks before DLSS)

Run 4, at run 3's corner (x −116.6, y −2005.2, facing south-west, heading about 220). The user plays, and the agent
sends requests and reads what comes back, with the commands from Task 29, plus CET's own frame time:
```bash
cat /e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud/frames.txt
```
Each of the step's unknowns has a setting that changes it live, as `set cull` did in Task 30:
- **The states before the DLSS list:** the first `log` shows them, before anything is drawn.
- **`exposure`:** how bright the blocks are against the scene.
- **`jittersign` and `mvsign`:** the signs of the jitter and of the motion vectors.

If drawing skips frames, the status says why: stop there, and fix it in a new task before going on.

**Files:**
- Modify: `docs/spikes/2026-10-01-block-render-results.md`

- [x] **Step 1: Deploy (the game must not be running)**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game first, because a running game locks the DLL.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default -DCP2077_DIR=E:/Cyberpunk2077 > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C|error MSB"; \
  "$CMAKE" --preset default -DCP2077_DIR= > /dev/null; \
  ls /e/Cyberpunk2077/red4ext/plugins/BlockRender /e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud \
  && cmp build/RelWithDebInfo/BlockRender.dll /e/Cyberpunk2077/red4ext/plugins/BlockRender/BlockRender.dll && echo installed
```
Expected: the plugin's folder holds `BlockRender.dll`, `BlockRender.pdb`, `shaders`, `atlas.bin` and `atlas.json`, the
overlay's folder holds `init.lua`, then `installed`.

- [x] **Step 2: Load, at the corner**

Ask the user to launch the game, load the free-roam save, go to run 3's corner, face south-west, and stand still.
Then read the log's first lines, the status and `frames.txt`. Expected:
- `BlockRender 0.3.0 loaded`;
- in the status, `mode=off` and `tracking off`;
- in `frames.txt`, CET's frame time.

- [x] **Step 3: What the hooks cost, with the plugin**

With the user standing still and the mouse left alone, read the status's `frame=` line and `frames.txt` five times
each, a second apart:
```bash
H=/e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud
for i in 1 2 3 4 5; do sed -n '5p' $H/status.txt; cat $H/frames.txt; sleep 1.1; done
```
Note both averages. Step 13 repeats CET's without the plugin.

- [x] **Step 4: The states before the DLSS list**

Send `log`, wait 3 seconds, and summarise the newest frame log. In "States in the DLSS and G-buffer lists", note each
tagged buffer's states in the DLSS lists. Expected:
- the scaling input colour, the motion vectors and the depth each show `<before> -> <after>; tagged <state>` or `not
  moved; tagged <state>`;
- each `<before>` is a plain state: a note of `flags 2` (the end of a split barrier begun earlier) or of a
  subresource other than 0 would make `predlss` skip. Only the state before matters to `predlss`, so an
  `unknown` after the arrow doesn't.

Note the G-buffer lists' states too, for Part 3.

- [x] **Step 5: Everything in one dump**

Send `dump all`, wait 5 seconds, then read `dump.json` and convert the dump, as in Task 32 Step 5:
```bash
D=$(ls -td /e/Cyberpunk2077/red4ext/plugins/BlockRender/dumps/*/ | head -1); cat "$D/dump.json" | grep -E '"file"|errors'; \
  cd /d/Cyberpunk-Minecraft/spikes/block-render && uv run --no-project -q --with numpy --with pillow python tools/view_dump.py "$D"
```
Expected: the three tags and the G-buffer's groups, from one frame, and no errors. Part 3 compares the colour with the
G-buffer's base colour, pixel by pixel, to settle whether the base colour is gamma-encoded.

- [x] **Step 6: The cube, before DLSS**

Ask the user to face open ground and stand still. Send `mode predlss`, then `place cube`, and read the status twice, a
second apart. Expected:
- `mode=predlss`;
- frames drawn at about the frame rate, and none skipped after the first second;
- a GPU time.

Ask for a screenshot. Expected: the cube sits under the HUD and CET's windows, with its faces' shading, and tone mapped
with the scene.

- [x] **Step 7: The exposure**

Ask the user to compare the cube's top with the sunlit pavement. Try `set exposure` at 1.5, 3, 6 and 12, two seconds
apart, and ask which looks closest. Narrow it down, and note the value.

- [x] **Step 8: The jitter's sign**

Ask the user to stand still and watch the cube's edges for a few seconds at each of `set jittersign 1`, `-1` and `0`.
Expected:
- the right sign: steady, smooth edges;
- the wrong sign: edges that crawl or blur;
- `0`: steady but jagged edges.

Note which is right, and leave it set.

- [x] **Step 9: The motion vectors' sign**

Ask the user to strafe left and right past the cube, at each of `set mvsign 1`, `-1` and `0`. Expected:
- the right sign: no trails or smears behind its edges;
- the others: trails.

Note which is right, and leave it set.

- [x] **Step 10: More blocks, and the UI**

Ask the user to move to an open spot for each, and to say when they're ready. Then:
- `place sampler`: leaves and glass are cut out;
- `place house`.

Ask the user to walk around them, then open the map and the pause menu, with screenshots. Expected:
- the blocks are under every menu;
- they hide what's behind them, and nearer things hide them.

- [x] **Step 11: The timings**

Back at the corner, with the same view and `predlss` still on, send `clear` and read the status five times, a
second apart: drawing with no blocks. Then `place field 10000` and read it five times, then `clear`, `place field
100000`, and five times again. Note the blocks, the sections, the vertices, the GPU time and the frame time of
each.

- [x] **Step 12: Clean up**

Send `clear`, then `mode off`, two seconds apart. Expected in the status: `mode=off`, and `tracking off`.

- [x] **Step 13: The game without the plugin**

Ask the user to quit the game. Once `tasklist | grep -i -c "cyberpunk2077.exe"` prints `0`, take the plugin out, and
remove its stale status file:
```bash
P=/e/Cyberpunk2077/red4ext/plugins/BlockRender; mv "$P/BlockRender.dll" "$P/BlockRender.dll.off" \
  && rm -f /e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud/status.txt && ls "$P"
```
Ask the user to launch the game, load the same save, stand at the corner facing the same way, and stand still. Read
`frames.txt` five times, a second apart, as in Step 3. Ask the user to quit, then, once the game is gone, put the
plugin back:
```bash
P=/e/Cyberpunk2077/red4ext/plugins/BlockRender; mv "$P/BlockRender.dll.off" "$P/BlockRender.dll" && ls "$P"
```
The difference from Step 3's CET time is what the hooks cost.

- [x] **Step 14: Record the results, and commit**

In `docs/spikes/2026-10-01-block-render-results.md`, add step 3:
- the states before the DLSS list;
- the exposure, and the two signs;
- the screenshots' findings: the HUD and menus, the cut-outs, hiding both ways;
- the timings at 10,000 and 100,000 blocks;
- what the hooks cost.

Then commit:
```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes \
  && git commit -q -m "block-render: step 3's results" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 45: Switches that show what each group of hooks costs

Run 4 found that BlockRender doubles the frame time with drawing off: 26.5 ms at run 3's corner, against 13.6 ms
without it. To find out where that goes, `set hooks.<group> 0` makes a group of hooks call their originals and do
nothing else, and `1` turns it back on. The groups:
- **`lists`:** the command list calls the tracker follows;
- **`views`:** the device's view creation and descriptor copies;
- **`submit`:** `ExecuteCommandLists`;
- **`present`:** `Present` and `Present1`;
- **`sl`:** the four Streamline imports.

The status's hooks line says which are off. A group switched off loses track while it's off: the views registry
misses views made meanwhile, and drawing skips frames without the submissions or Streamline's calls. So the switches
are for measuring, not for leaving off.

**Files:**
- Create: `src/hooks/HookGroups.h`, `src/hooks/HookGroups.cpp`, `tests/TestHookGroups.cpp`
- Modify: `CMakeLists.txt`, `src/hooks/D3D12Hooks.cpp`, `src/hooks/StreamlineHooks.cpp`, `src/Plugin.cpp`

- [ ] **Step 1: Write the failing test, `tests/TestHookGroups.cpp`**

```cpp
#include "Harness.h"

#include "hooks/HookGroups.h"

using cpmc::HookGroup;

TEST(HookGroupsGoByTheirNames)
{
	CHECK(cpmc::HookGroupNamed("lists") == HookGroup::Lists);
	CHECK(cpmc::HookGroupNamed("views") == HookGroup::Views);
	CHECK(cpmc::HookGroupNamed("submit") == HookGroup::Submit);
	CHECK(cpmc::HookGroupNamed("present") == HookGroup::Present);
	CHECK(cpmc::HookGroupNamed("sl") == HookGroup::Streamline);
	CHECK(!cpmc::HookGroupNamed("everything"));
}

TEST(HookGroupsSwitchOffAndOnAgain)
{
	CHECK_STR(cpmc::HookGroupsSummary(), "all hooks on");
	cpmc::SetHookGroup(HookGroup::Views, false);
	cpmc::SetHookGroup(HookGroup::Submit, false);
	CHECK(!cpmc::HookGroupOn(HookGroup::Views));
	CHECK(cpmc::HookGroupOn(HookGroup::Lists));
	CHECK_STR(cpmc::HookGroupsSummary(), "hooks off: views, submit");
	cpmc::SetHookGroup(HookGroup::Views, true);
	cpmc::SetHookGroup(HookGroup::Submit, true);
	CHECK_STR(cpmc::HookGroupsSummary(), "all hooks on");
}
```

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestPredlss.cpp
)
```
```cmake
	tests/TestPredlss.cpp
	tests/TestHookGroups.cpp
)
```

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'hooks/HookGroups.h'`.

- [ ] **Step 4: Write `src/hooks/HookGroups.h`**

```cpp
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace cpmc
{
// Groups of hooks. `set hooks.<name> 0` makes a group's hooks call their originals and do nothing else, so the game
// can show what each group costs: run 4 found the plugin doubled the frame time with drawing off.
enum class HookGroup
{
	Lists,       // "lists": the command list calls the tracker follows
	Views,       // "views": the device's view creation and descriptor copies
	Submit,      // "submit": ExecuteCommandLists
	Present,     // "present": Present and Present1
	Streamline,  // "sl": the four Streamline imports
};

// The group called aName, if there is one.
std::optional<HookGroup> HookGroupNamed(std::string_view aName);
void                     SetHookGroup(HookGroup aGroup, bool aOn);
bool                     HookGroupOn(HookGroup aGroup);  // one relaxed atomic read: cheap enough for every call
std::string              HookGroupsSummary();             // "all hooks on", or "hooks off: lists, views"
}
```

- [ ] **Step 5: Write `src/hooks/HookGroups.cpp`**

```cpp
#include "hooks/HookGroups.h"

#include <array>
#include <atomic>

namespace cpmc
{
namespace
{
constexpr std::array<std::string_view, 5> kNames = { "lists", "views", "submit", "present", "sl" };
std::array<std::atomic<bool>, 5>          g_on = { true, true, true, true, true };
}

std::optional<HookGroup> HookGroupNamed(std::string_view aName)
{
	for (size_t i = 0; i < kNames.size(); ++i)
	{
		if (kNames[i] == aName)
		{
			return HookGroup(i);
		}
	}
	return std::nullopt;
}

void SetHookGroup(HookGroup aGroup, bool aOn)
{
	g_on[size_t(aGroup)].store(aOn, std::memory_order_relaxed);
}

bool HookGroupOn(HookGroup aGroup)
{
	return g_on[size_t(aGroup)].load(std::memory_order_relaxed);
}

std::string HookGroupsSummary()
{
	std::string off;
	for (size_t i = 0; i < kNames.size(); ++i)
	{
		if (!g_on[i].load(std::memory_order_relaxed))
		{
			off += (off.empty() ? "" : ", ") + std::string(kNames[i]);
		}
	}
	return off.empty() ? "all hooks on" : "hooks off: " + off;
}
}
```

- [ ] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/render/Predlss.cpp
)
```
```cmake
	src/render/Predlss.cpp
	src/hooks/HookGroups.cpp
)
```

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `103 test(s), 0 failure(s)`.

- [ ] **Step 8: Edit `src/hooks/D3D12Hooks.cpp` to let each group pass straight through**

```cpp
#include "hooks/D3D12Hooks.h"

#include "hooks/Slots.h"
```
```cpp
#include "hooks/D3D12Hooks.h"

#include "hooks/HookGroups.h"
#include "hooks/Slots.h"
```
```cpp
// The list's state, if it's being tracked and is recording.
ListState* Recording(void* aList)
{
	ListState* state = g_tracker.Find(aList);
```
```cpp
// The list's state, if it's being tracked and is recording, and the list hooks are on.
ListState* Recording(void* aList)
{
	if (!HookGroupOn(HookGroup::Lists))
	{
		return nullptr;
	}
	ListState* state = g_tracker.Find(aList);
```
```cpp
	g_original.createRtv(aDevice, aResource, aDesc, aHandle);
	uint32_t mip = 0, plane = 0, slice = 0;
```
```cpp
	g_original.createRtv(aDevice, aResource, aDesc, aHandle);
	if (!HookGroupOn(HookGroup::Views))
	{
		return;
	}
	uint32_t mip = 0, plane = 0, slice = 0;
```
```cpp
	g_original.createDsv(aDevice, aResource, aDesc, aHandle);
	uint32_t mip = 0, slice = 0;
```
```cpp
	g_original.createDsv(aDevice, aResource, aDesc, aHandle);
	if (!HookGroupOn(HookGroup::Views))
	{
		return;
	}
	uint32_t mip = 0, slice = 0;
```
```cpp
	const auto kind = KindOf(aType);
	if (!kind)
```
```cpp
	const auto kind = KindOf(aType);
	if (!kind || !HookGroupOn(HookGroup::Views))
```
```cpp
	if (const auto kind = KindOf(aType))
	{
		g_views.Copy(*kind, aDest.ptr, aSource.ptr, aCount);
```
```cpp
	if (const auto kind = KindOf(aType); kind && HookGroupOn(HookGroup::Views))
	{
		g_views.Copy(*kind, aDest.ptr, aSource.ptr, aCount);
```
```cpp
void STDMETHODCALLTYPE ExecuteCommandLists(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists)
{
	if (ID3D12GraphicsCommandList* evaluate = g_evaluateList.load())
```
```cpp
void STDMETHODCALLTYPE ExecuteCommandLists(ID3D12CommandQueue* aQueue, UINT aCount, ID3D12CommandList* const* aLists)
{
	if (!HookGroupOn(HookGroup::Submit))
	{
		return g_original.executeCommandLists(aQueue, aCount, aLists);
	}
	if (ID3D12GraphicsCommandList* evaluate = g_evaluateList.load())
```
```cpp
	const HRESULT result = g_original.reset(aList, aAllocator, aState);
	if (SUCCEEDED(result) && g_tracking.load(std::memory_order_relaxed))
```
```cpp
	const HRESULT result = g_original.reset(aList, aAllocator, aState);
	if (SUCCEEDED(result) && g_tracking.load(std::memory_order_relaxed) && HookGroupOn(HookGroup::Lists))
```
```cpp
HRESULT STDMETHODCALLTYPE Present(IDXGISwapChain* aSwapChain, UINT aSyncInterval, UINT aFlags)
{
	if ((aFlags & DXGI_PRESENT_TEST) == 0)
```
```cpp
HRESULT STDMETHODCALLTYPE Present(IDXGISwapChain* aSwapChain, UINT aSyncInterval, UINT aFlags)
{
	if ((aFlags & DXGI_PRESENT_TEST) == 0 && HookGroupOn(HookGroup::Present))
```
```cpp
HRESULT STDMETHODCALLTYPE Present1(IDXGISwapChain1* aSwapChain, UINT aSyncInterval, UINT aFlags, const DXGI_PRESENT_PARAMETERS* aParameters)
{
	if ((aFlags & DXGI_PRESENT_TEST) == 0)
```
```cpp
HRESULT STDMETHODCALLTYPE Present1(IDXGISwapChain1* aSwapChain, UINT aSyncInterval, UINT aFlags, const DXGI_PRESENT_PARAMETERS* aParameters)
{
	if ((aFlags & DXGI_PRESENT_TEST) == 0 && HookGroupOn(HookGroup::Present))
```

- [ ] **Step 9: Edit `src/hooks/StreamlineHooks.cpp` the same way**

```cpp
#include "hooks/D3D12Hooks.h"
#include "hooks/ImportPatch.h"
```
```cpp
#include "hooks/D3D12Hooks.h"
#include "hooks/HookGroups.h"
#include "hooks/ImportPatch.h"
```
```cpp
	const sl::Result result = g_getNewFrameToken(aToken, aFrameIndex);
	if (result == sl::Result::eOk && aToken)
```
```cpp
	const sl::Result result = g_getNewFrameToken(aToken, aFrameIndex);
	if (result == sl::Result::eOk && aToken && HookGroupOn(HookGroup::Streamline))
```
```cpp
sl::Result SetConstants(const sl::Constants& aValues, const sl::FrameToken& aFrame, const sl::ViewportHandle& aViewport)
{
	ReadableMemory memory;
```
```cpp
sl::Result SetConstants(const sl::Constants& aValues, const sl::FrameToken& aFrame, const sl::ViewportHandle& aViewport)
{
	if (!HookGroupOn(HookGroup::Streamline))
	{
		return g_setConstants(aValues, aFrame, aViewport);
	}
	ReadableMemory memory;
```
```cpp
sl::Result SetTag(const sl::ViewportHandle& aViewport, const sl::ResourceTag* aTags, uint32_t aCount, sl::CommandBuffer* aList)
{
	ReadableMemory     memory;
```
```cpp
sl::Result SetTag(const sl::ViewportHandle& aViewport, const sl::ResourceTag* aTags, uint32_t aCount, sl::CommandBuffer* aList)
{
	if (!HookGroupOn(HookGroup::Streamline))
	{
		return g_setTag(aViewport, aTags, aCount, aList);
	}
	ReadableMemory     memory;
```
```cpp
	if (aFeature == sl::kFeatureDLSS)
	{
		++g_evaluates;
```
```cpp
	if (aFeature == sl::kFeatureDLSS && HookGroupOn(HookGroup::Streamline))
	{
		++g_evaluates;
```

- [ ] **Step 10: Edit `src/Plugin.cpp` for `set hooks.<group>`, and the status**

```cpp
#include "hooks/D3D12Hooks.h"
#include "hooks/StreamlineHooks.h"
```
```cpp
#include "hooks/D3D12Hooks.h"
#include "hooks/HookGroups.h"
#include "hooks/StreamlineHooks.h"
```
```cpp
	if (aRequest.what == "tracking")
	{
```
```cpp
	if (aRequest.what.starts_with("hooks."))
	{
		const std::string_view               name = std::string_view(aRequest.what).substr(6);
		const std::optional<cpmc::HookGroup> group = cpmc::HookGroupNamed(name);
		if (!group)
		{
			return Report(std::format("set: there's no hook group called \"{}\": they're lists, views, submit, present and sl", name));
		}
		cpmc::SetHookGroup(*group, aRequest.number != 0.0);
		return Report(std::format("set {} {}: {}", aRequest.what, aRequest.number != 0.0 ? 1 : 0, cpmc::HookGroupsSummary()));
	}
	if (aRequest.what == "tracking")
	{
```
```cpp
	report.hooks = g_hooked ? std::format("{}; {}; log {}; dump {}", cpmc::slhooks::Summary(), cpmc::d3d::Summary(), g_frameLog->State(),
	                                      g_dumps->State())
	                        : "not installed (see red4ext\\logs)";
```
```cpp
	report.hooks = g_hooked ? std::format("{}; {}; {}; log {}; dump {}", cpmc::slhooks::Summary(), cpmc::d3d::Summary(),
	                                      cpmc::HookGroupsSummary(), g_frameLog->State(), g_dumps->State())
	                        : "not installed (see red4ext\\logs)";
```

- [ ] **Step 11: Build; it must compile without warnings, and the tests still pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `103 test(s), 0 failure(s)`.

- [ ] **Step 12: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: switches that show what each group of hooks costs" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 46: Tracking that costs nothing while it's off, and drawing before DLSS that tracks two lists

Every hooked command list call asked the tracker for its list, under the tracker's lock, even with tracking off: about
10,000 calls a frame, from up to 12 threads, each missing the per-thread cache. That's the likeliest cause of the
hooks' cost that run 4 measured. Two changes:
- **Off costs nothing:** the hooks count the tracked lists between their `Reset` and `Close`. While none is recording,
  a hooked call costs one atomic read, not a lookup.
- **Drawing before DLSS tracks only the DLSS lists:** it no longer turns tracking on for every list. Each evaluate's
  list is watched instead, tracked from its next `Reset` while everything else goes untracked (`ListWatch`). The game
  alternates between two DLSS lists, so drawing starts within a few frames, as before.

Run 4 settled `exposure` at 2.5, which becomes the default. The version goes to 0.4.0.

**Files:**
- Create: `src/track/ListWatch.h`, `src/track/ListWatch.cpp`, `tests/TestListWatch.cpp`
- Modify: `CMakeLists.txt`, `src/hooks/D3D12Hooks.h`, `src/hooks/D3D12Hooks.cpp`, `src/render/Renderer.h`,
  `src/render/Renderer.cpp`, `src/Plugin.cpp`

- [ ] **Step 1: Write the failing test, `tests/TestListWatch.cpp`**

```cpp
#include "Harness.h"

#include "track/ListWatch.h"

namespace
{
const void* List(uintptr_t aAddress)
{
	return reinterpret_cast<const void*>(aAddress);
}
}

TEST(ListWatchKeepsTheLastFewListsNamed)
{
	cpmc::ListWatch watch;
	CHECK_EQ(watch.Count(), size_t(0));
	CHECK(!watch.Contains(List(0x10)));
	CHECK(!watch.Contains(nullptr));
	watch.Add(List(0x10));
	watch.Add(List(0x20));
	watch.Add(List(0x10));  // already watched: nothing moves
	CHECK_EQ(watch.Count(), size_t(2));
	CHECK(watch.Contains(List(0x10)) && watch.Contains(List(0x20)));
	CHECK(!watch.Contains(nullptr));  // empty slots aren't lists
	watch.Add(List(0x30));
	watch.Add(List(0x40));
	watch.Add(List(0x50));  // a fifth: the oldest goes
	CHECK_EQ(watch.Count(), cpmc::ListWatch::kSize);
	CHECK(!watch.Contains(List(0x10)));
	CHECK(watch.Contains(List(0x50)));
	watch.Add(nullptr);  // ignored
	CHECK(watch.Contains(List(0x20)));
	watch.Clear();
	CHECK_EQ(watch.Count(), size_t(0));
	CHECK(!watch.Contains(List(0x50)));
}
```

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestHookGroups.cpp
)
```
```cmake
	tests/TestHookGroups.cpp
	tests/TestListWatch.cpp
)
```

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'track/ListWatch.h'`.

- [ ] **Step 4: Write `src/track/ListWatch.h`**

```cpp
#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace cpmc
{
// The command lists tracked even while tracking is off: drawing before DLSS needs the DLSS evaluate's lists' first
// transitions, and nothing else. The game alternates between two such lists, so the last few named are kept. One
// thread names them (the evaluate's); any thread may ask, with a few atomic reads and no lock.
class ListWatch
{
public:
	static constexpr size_t kSize = 4;

	void   Add(const void* aList);  // a list already watched stays where it is
	void   Clear();
	bool   Contains(const void* aList) const;
	size_t Count() const;

private:
	std::array<std::atomic<const void*>, kSize> lists_{};
	std::atomic<size_t>                         next_{ 0 };
};
}
```

- [ ] **Step 5: Write `src/track/ListWatch.cpp`**

```cpp
#include "track/ListWatch.h"

namespace cpmc
{
void ListWatch::Add(const void* aList)
{
	if (!aList || Contains(aList))
	{
		return;
	}
	lists_[next_.fetch_add(1, std::memory_order_relaxed) % kSize].store(aList, std::memory_order_release);
}

void ListWatch::Clear()
{
	for (auto& list : lists_)
	{
		list.store(nullptr, std::memory_order_release);
	}
}

bool ListWatch::Contains(const void* aList) const
{
	for (const auto& list : lists_)
	{
		if (list.load(std::memory_order_acquire) == aList)
		{
			return aList != nullptr;
		}
	}
	return false;
}

size_t ListWatch::Count() const
{
	size_t count = 0;
	for (const auto& list : lists_)
	{
		count += list.load(std::memory_order_acquire) ? 1 : 0;
	}
	return count;
}
}
```

- [ ] **Step 6: Edit `CMakeLists.txt` to add the source, and the version**

```cmake
project(BlockRender VERSION 0.3.0 LANGUAGES C CXX)
```
```cmake
project(BlockRender VERSION 0.4.0 LANGUAGES C CXX)
```
```cmake
	src/hooks/HookGroups.cpp
)
```
```cmake
	src/hooks/HookGroups.cpp
	src/track/ListWatch.cpp
)
```

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `104 test(s), 0 failure(s)`.

- [ ] **Step 8: Edit `src/hooks/D3D12Hooks.h` to watch lists**

```cpp
void RequestTracking();
void ReleaseTracking();
bool Tracking();
```
```cpp
void RequestTracking();
void ReleaseTracking();
bool Tracking();

// A list to track from its next Reset even while tracking is off: drawing before DLSS names each evaluate's list, the
// only lists it needs. The last few named stay watched until UnwatchLists.
void WatchList(const void* aList);
void UnwatchLists();
```

- [ ] **Step 9: Edit `src/hooks/D3D12Hooks.cpp` to count the lists recording, and watch lists**

```cpp
#include "log/EventNames.h"
```
```cpp
#include "log/EventNames.h"
#include "track/ListWatch.h"
```
```cpp
StateTracker                 g_tracker;
```
```cpp
StateTracker                 g_tracker;
ListWatch                    g_watch;                // lists tracked even while tracking is off
std::atomic<int>             g_listsRecording{ 0 };  // tracked lists between their Reset and their Close
```
```cpp
// The list's state, if it's being tracked and is recording, and the list hooks are on.
ListState* Recording(void* aList)
{
	if (!HookGroupOn(HookGroup::Lists))
	{
		return nullptr;
	}
	ListState* state = g_tracker.Find(aList);
	return state && state->Recording() ? state : nullptr;
}
```
```cpp
// The list's state, if it's tracked and recording. While no list is, this costs one atomic read, not a lookup under
// the tracker's lock: the likeliest cause of the hooks' cost that run 4 measured.
ListState* Tracked(void* aList)
{
	if (g_listsRecording.load(std::memory_order_relaxed) == 0)
	{
		return nullptr;
	}
	ListState* state = g_tracker.Find(aList);
	return state && state->Recording() ? state : nullptr;
}

// The list's state, if the list hooks are on and the list is tracked: every list while tracking is on, and the
// watched ones while it's off.
ListState* Recording(void* aList)
{
	if (!HookGroupOn(HookGroup::Lists) || (!g_tracking.load(std::memory_order_relaxed) && !g_watch.Contains(aList)))
	{
		return nullptr;
	}
	return Tracked(aList);
}
```
```cpp
	if (SUCCEEDED(result) && g_tracking.load(std::memory_order_relaxed) && HookGroupOn(HookGroup::Lists))
	{
		g_tracker.ForReset(aList).Begin(g_frame.load(std::memory_order_relaxed), g_logging.load(std::memory_order_relaxed));
	}
```
```cpp
	if (SUCCEEDED(result) && HookGroupOn(HookGroup::Lists) && (g_tracking.load(std::memory_order_relaxed) || g_watch.Contains(aList)))
	{
		ListState& state = g_tracker.ForReset(aList);
		if (!state.Recording())
		{
			g_listsRecording.fetch_add(1, std::memory_order_relaxed);
		}
		state.Begin(g_frame.load(std::memory_order_relaxed), g_logging.load(std::memory_order_relaxed));
	}
```
```cpp
	if (ListState* state = Recording(aList))
	{
		Each([&](HookEvents& aListener) { aListener.OnPassEnd(aList, *state, true); });
		Each([&](HookEvents& aListener) { aListener.OnClose(aList, *state); });
		state->End();
	}
	return g_original.close(aList);
```
```cpp
	// Every tracked list ends here, even one that tracking or the list hooks have let go of since its Reset, so the
	// count of lists recording stays right.
	if (ListState* state = Tracked(aList))
	{
		if (Recording(aList))
		{
			Each([&](HookEvents& aListener) { aListener.OnPassEnd(aList, *state, true); });
			Each([&](HookEvents& aListener) { aListener.OnClose(aList, *state); });
		}
		state->End();
		g_listsRecording.fetch_sub(1, std::memory_order_relaxed);
	}
	return g_original.close(aList);
```
```cpp
void SetLogging(bool aLogging)
{
	g_logging = aLogging;
}
```
```cpp
void SetLogging(bool aLogging)
{
	g_logging = aLogging;
}

void WatchList(const void* aList)
{
	g_watch.Add(aList);
}

void UnwatchLists()
{
	g_watch.Clear();
}
```
```cpp
	return std::format("device {}, {} vtable entries, present {}, main queue {}, tracking {}",
	                   g_device.load() ? (g_seenAtCreation.load() ? "seen at creation" : "patched late") : "not seen",
	                   g_patcher.Count(), g_original.present ? "patched" : "not patched", g_mainQueue.load() ? "known" : "unknown",
	                   g_tracking.load() ? "on" : "off");
```
```cpp
	return std::format("device {}, {} vtable entries, present {}, main queue {}, tracking {}, {} lists watched",
	                   g_device.load() ? (g_seenAtCreation.load() ? "seen at creation" : "patched late") : "not seen",
	                   g_patcher.Count(), g_original.present ? "patched" : "not patched", g_mainQueue.load() ? "known" : "unknown",
	                   g_tracking.load() ? "on" : "off", g_watch.Count());
```

- [ ] **Step 10: Edit `src/render/Renderer.h` for watched lists, and the exposure**

```cpp
//   own. Its barriers start from the state the DLSS list found each buffer in (its first transitions), so command lists
//   are tracked while this mode is on.
```
```cpp
//   own. Its barriers start from the state the DLSS list found each buffer in (its first transitions), so the DLSS
//   lists are watched: tracked from their Reset, while every other list goes untracked.
```
```cpp
	void               Fail(const std::string& aWhy);  // logs, and switches drawing off
	void               ReleaseTracking();               // if predlss asked for it
```
```cpp
	void               Fail(const std::string& aWhy);  // logs, and switches drawing off
```
```cpp
	std::atomic<Mode>     mode_{ Mode::Off };
	bool                  trackingRequested_ = false;
```
```cpp
	std::atomic<Mode>     mode_{ Mode::Off };
```
```cpp
	float exposure_ = 3.0f;    // predlss: what the linear colour is multiplied by; run 2 put sunlit ground at 0.6 to 1
```
```cpp
	float exposure_ = 2.5f;    // predlss: what the linear colour is multiplied by; run 4 settled on 2.5 by eye
```

- [ ] **Step 11: Edit `src/render/Renderer.cpp` to watch the DLSS lists**

```cpp
	const Mode mode = aMode == "present" ? Mode::Present : aMode == "predlss" ? Mode::Predlss : Mode::Off;
	if (mode == Mode::Predlss && !trackingRequested_)
	{
		d3d::RequestTracking();  // our barriers start from the DLSS list's first transitions
		trackingRequested_ = true;
	}
	else if (mode != Mode::Predlss)
	{
		ReleaseTracking();
	}
	mode_ = mode;
```
```cpp
	const Mode mode = aMode == "present" ? Mode::Present : aMode == "predlss" ? Mode::Predlss : Mode::Off;
	if (mode != Mode::Predlss)
	{
		d3d::UnwatchLists();  // predlss watches the DLSS lists as their evaluates come
	}
	mode_ = mode;
```
```cpp
	mode_ = Mode::Off;
	ReleaseTracking();
}

void Renderer::ReleaseTracking()
{
	if (trackingRequested_)
	{
		d3d::ReleaseTracking();
		trackingRequested_ = false;
	}
}
```
```cpp
	mode_ = Mode::Off;
	d3d::UnwatchLists();
}
```
```cpp
		predlssList_ = aCall.list;
		return;
```
```cpp
		predlssList_ = aCall.list;
		d3d::WatchList(aCall.list);  // tracked from its next Reset: its first transitions are where our barriers start
		return;
```
```cpp
	std::lock_guard lock(mutex_);
	ReleaseTracking();
```
```cpp
	std::lock_guard lock(mutex_);
	d3d::UnwatchLists();
```

- [ ] **Step 12: Edit `src/Plugin.cpp` for the version**

```cpp
constexpr const char* kVersion = "0.3.0";
```
```cpp
constexpr const char* kVersion = "0.4.0";
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 3, 0);
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 4, 0);
```

- [ ] **Step 13: Build; it must compile without warnings, and the tests still pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `104 test(s), 0 failure(s)`.

- [ ] **Step 14: Check the DLL's exports and imports**

```bash
DUMPBIN="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/dumpbin.exe"
B=/d/Cyberpunk-Minecraft/spikes/block-render/build/RelWithDebInfo/BlockRender.dll
"$DUMPBIN" -nologo -exports "$B" | grep -E " (Main|Query|Supports) = "; \
  "$DUMPBIN" -nologo -dependents "$B" | grep -iE "d3d|dxgi|sl\."
```
Expected: exactly `Main`, `Query` and `Supports`, then `D3DCOMPILER_47.dll` and `d3d12.dll`.

- [ ] **Step 15: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "block-render: tracking that costs nothing while it's off, and predlss tracks only the DLSS lists (0.4.0)" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 47: In the game: what the hooks cost now

Run 5, at run 3's corner, with the same save. Run 4 measured 13.6 ms there without BlockRender, and 26.5 ms with it.
The commands are Task 44's.

**Files:**
- Modify: `docs/spikes/2026-10-01-block-render-results.md`

- [ ] **Step 1: Deploy (the game must not be running)**

Run Task 44 Step 1's two commands. Expected: `0`, then the plugin's folder with `BlockRender.dll`, `BlockRender.pdb`,
`shaders`, `atlas.bin` and `atlas.json`, the overlay's folder with `init.lua`, and `installed`.

- [ ] **Step 2: Load, at the corner**

Ask the user to launch the game, load the free-roam save, go to the corner, face south-west, and stand still. Then
read the log's first lines, the status and `frames.txt`. Expected:
- `BlockRender 0.4.0 loaded`;
- in the status, `mode=off`, `tracking off, 0 lists watched` and `all hooks on`.

- [ ] **Step 3: What the hooks cost, idle**

Read the status's `frame=` line and `frames.txt` five times each, a second apart, as in Task 44 Step 3. Expected:
about the 13.6 ms run 4 measured without BlockRender. If so, the lookups were the cost.

- [ ] **Step 4: If it isn't, which group costs it**

Only if Step 3 is well above 13.6 ms: switch each group off in turn, with `set hooks.lists 0`, then `views`,
`submit`, `present` and `sl`, five readings each, and back on with `1` before the next. The group whose switch brings
the frame time down holds the cost. Note each group's readings.

- [ ] **Step 5: Tracking on**

Send `set tracking 1`, read five times, then `set tracking 0`. That's what tracking every list costs, which frame logs
and dumps still do while they run.

- [ ] **Step 6: Drawing before DLSS**

Ask the user to face open ground. Send `mode predlss` and `place cube`, wait three seconds, and read the status. Expected:
- `2 lists watched`, and `tracking off`;
- frames drawn at the frame rate, and none skipped after the first second.

Read five times. Then ask the user to check the cube is steady standing still and leaves no trails strafing, with a
screenshot.

- [ ] **Step 7: 10,000 blocks**

Send `clear`, then `place field 10000`, and read five times.

- [ ] **Step 8: Clean up**

Send `clear`, then `mode off`, two seconds apart. Expected: `mode=off`, `tracking off, 0 lists watched`.

- [ ] **Step 9: Record the results, and commit**

In `docs/spikes/2026-10-01-block-render-results.md`, add what run 5 found:
- the hooks' cost, idle;
- each group's, if Step 4 ran;
- tracking on, drawing before DLSS, and 10,000 blocks.

Then commit:
```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes \
  && git commit -q -m "block-render: what the hooks cost after the fixes" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 48: Close Part 2, and plan Part 3

**Files:**
- Modify: `docs/superpowers/plans/2026-10-01-block-render.md` (this plan)

- [ ] **Step 1: Add "How Part 2 went" to this plan**

Add a section after Task 48, as Part 1's closed it. Cover:
- what Tasks 44 and 47 found, and Tasks 45 and 46's fixes;
- any amendments made along the way;
- what step 3's run decides for step 4:
  - where blocks can go into the G-buffer: the G-buffer lists' last submission, and the states they leave their
    targets in;
  - whether the base colour is gamma-encoded, from Task 44's dump;
  - the stencil value, and whether blocks must write motion vectors;
  - what the hooks cost now, and what tracking costs, for the tracking step 4 needs.

- [ ] **Step 2: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/superpowers/plans \
  && git commit -q -m "block-render plan: how Part 2 went" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" \
  && git log --oneline -1
```

- [ ] **Step 3: Plan Part 3**

Use superpowers:writing-plans to write Part 3's tasks (spec step 4) from what steps 1 to 3 found, as Tasks 49 on in
this plan. As in Parts 1 and 2, compile and test their code in a scratch clone first, and replay the tasks before
committing.
