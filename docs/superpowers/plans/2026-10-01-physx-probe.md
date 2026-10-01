# PhysX Probe Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build PhysXProbe, a read-only RED4ext plugin that checks whether Cyberpunk 2077's PhysX 3.4.2 can be read
through NVIDIA's headers, corrected where the game's build differs, and dumps the collision around V (an OBJ plus a
JSON report). Then verify it in the running game.

**Status:** Tasks 1 to 14 built revision 1 of the spec. Task 15's first in-game runs led to the spec's revision 2,
which Tasks 16 to 22 build and verify (see "Revision 2" below).

**Architecture:** A C++20 RED4ext plugin, built with the header-only RED4ext SDK and NVIDIA's PhysX 3.4.2 headers,
and linked against no PhysX libraries.
- Everything that doesn't need the game lives in a static library, `probe_core`, with unit tests.
- The plugin adds the RED4ext glue: entry points, triggers and finding V.
- PhysX is reached through the exported `PxGetPhysics` and called only through the vtable slots the audit verified,
  on objects whose vtables are audited tables of the audited DLL build. Memory is checked before it is read, and
  nothing is caught: revision 1's crash guard is gone.
- A Python tool renders dumps to images for checking.

**Tech stack:** C++20, MSVC 14.44 (Visual Studio 2022), CMake 3.31 (bundled with Visual Studio), RED4ext.SDK
(header-only), NVIDIA PhysX 3.4.2 headers, Python 3.14 with uv (numpy, Pillow, pytest).

**Spec:** [docs/superpowers/specs/2026-10-01-physx-probe-design.md](../specs/2026-10-01-physx-probe-design.md)

---

## Before you start

- Work on a branch: in `D:\Cyberpunk-Minecraft`, run `git switch -c spike/physx-probe` from `main`.
- Commands are for Git Bash. These tools aren't on PATH; shell variables don't persist between tool calls, so every
  command block sets the ones it needs:
  ```bash
  CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
  CTEST="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe"
  DUMPBIN="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/dumpbin.exe"
  ```
- Pass MSBuild switches as `-m`, never `/m`: Git Bash rewrites `/m` into a file path.
- All the C++ and Python in this plan has already been compiled and tested once, in a scratch copy:
  - zero compiler warnings at `/W4`;
  - 39 C++ tests and 7 Python tests passing;
  - the plugin DLL exporting exactly `Main`, `Query` and `Supports`, with no PhysX imports.

  So a failure while following the plan most likely means a transcription slip; compare against the plan's code first.
- Code that talks to the live game (`PhysxSession`, `Probe`, `PlayerLocator`, `Plugin`) can't be unit-tested. What it
  relies on that can be tested offline is: the vtable slots, the struct layouts, the memory checks and the audited
  build (Tasks 16 to 18). Task 22's in-game run tests the rest.
- The test count grows with each task: 3, 7, 11, 15, 21, 28, 31, 33, 36, then 39; 42 after the version fix in
  `e3cbc0b`. Revision 2's counts are in its section.

## File map

| File | Responsibility |
|---|---|
| `.gitignore`, `.gitattributes`, `README.md` | Repo basics |
| `extern/RED4ext.SDK/` | Git submodule, pinned at `ad7277714ad30d6885d7050c5ba24fa0102f6920`, used header-only |
| `extern/physx-3.4.2/` | NVIDIA PhysX 3.4.2 public headers (vendored, BSD-3), with one local change in `PxShape.h` to match the game |
| `docs/spikes/2026-10-01-physx-abi-audit.md` | The audit of the game's PhysX DLLs that revision 2 relies on |
| `spikes/physx-probe/CMakeLists.txt`, `CMakePresets.json` | The build: `probe_core`, the `PhysXProbe` plugin, `probe_tests` |
| `src/Coords.h` | World (Z up) to Minecraft (Y up) axes; `Vec3d`, `Vec3f` |
| `src/Report.h` | Plain data for `report.json` |
| `src/Json.h`, `.cpp` | The small JSON writer |
| `src/physx/PxHeaders.h` | The PhysX headers the probe uses |
| `src/physx/ShapeTriangles.h`, `.cpp` | PhysX shapes as world triangles (pure geometry plus `AppendTriangles` for live shapes) |
| `src/physx/Checks.h`, `.cpp` | Pure parts of the compatibility check: ranges, geometry against mesh, categories, tallies |
| `src/physx/Readable.h`, `.cpp` | Readable-memory checks, with a region cache for one dump |
| `src/physx/GameAbi.h`, `.cpp` | The audited PhysX build: DLL identities, vtable addresses, `CheckObject` |
| `src/ObjWriter.h`, `.cpp` | `collision.obj` and `.mtl` text |
| `src/ReportJson.h`, `.cpp` | `report.json` text |
| `src/DumpWriter.h`, `.cpp` | Writes a dump's files on a background thread |
| `src/physx/PhysxSession.h`, `.cpp` | The only code that calls the game's PhysX: compatibility check, census, region, ray, every object checked first |
| `src/Probe.h`, `.cpp` | One dump, start to finish |
| `src/PlayerLocator.h`, `.cpp` | V's and the camera's pose, through the game's script functions (RED4ext) |
| `src/Plugin.cpp` | RED4ext entry points, triggers, hand-off to the writer |
| `tests/*` | The harness (`Harness.h`, `TestMain.cpp`) and one test file per unit; `TestAudit.cpp` checks vtable slots and struct layouts against the audit |
| `tools/render_dump.py`, `tools/test_render_dump.py` | Renders a dump to `top.png` and `eye.png`, plus its tests |

All paths under `src/`, `tests/` and `tools/` are relative to `spikes/physx-probe/`.

---

### Task 1: Repository basics and dependencies

**Files:**
- Create: `.gitignore`, `.gitattributes`, `README.md`
- Create: `extern/RED4ext.SDK` (submodule), `extern/physx-3.4.2/` (headers, `LICENSE.md`, `README.md`)

- [x] **Step 1: Create the branch**

```bash
cd /d/Cyberpunk-Minecraft && git switch -c spike/physx-probe && git branch --show-current
```
Expected: `spike/physx-probe`

- [x] **Step 2: Write `.gitignore`**

```gitignore
# CMake build folders
build/

# Python
__pycache__/
.pytest_cache/

# Renderer smoke-test output
spikes/physx-probe/smoke/
```

- [x] **Step 3: Write `.gitattributes`**

```gitattributes
* text=auto
```

- [x] **Step 4: Write `README.md`**

```markdown
# Cyberpunk-Minecraft

Exploring a SkyCraft-style mod for Cyberpunk 2077: play Night City as a Minecraft player, with Minecraft running
hidden next to the game and the two linked over shared memory. See
[chasmlol/SkyCraft](https://github.com/chasmlol/SkyCraft) for the Skyrim original.

Nothing is playable yet. The work so far is a set of spikes that de-risk the hard parts.

| Folder | |
|---|---|
| `docs/superpowers/specs/` | Designs |
| `docs/superpowers/plans/` | Implementation plans |
| `spikes/physx-probe/` | Spike 1: can Cyberpunk's PhysX be read through NVIDIA's public headers? |
| `extern/` | RED4ext.SDK (a submodule) and the PhysX 3.4.2 public headers |

To clone with the submodule, use `git clone --recursive`, or run `git submodule update --init` after cloning.
```

- [x] **Step 5: Add the RED4ext SDK as a submodule, pinned**

```bash
cd /d/Cyberpunk-Minecraft && git submodule add https://github.com/WopsS/RED4ext.SDK.git extern/RED4ext.SDK \
  && git -C extern/RED4ext.SDK checkout -q ad7277714ad30d6885d7050c5ba24fa0102f6920 \
  && git -C extern/RED4ext.SDK rev-parse HEAD
```
Expected: `ad7277714ad30d6885d7050c5ba24fa0102f6920`

- [x] **Step 6: Vendor the PhysX 3.4.2 headers**

`SCRATCH` is a scratch folder outside the repo; the command falls back to a fresh temp folder.
```bash
cd /d/Cyberpunk-Minecraft && SCRATCH="${SCRATCH:-$(mktemp -d)}" \
  && git clone -q --depth 1 --filter=blob:none --sparse https://github.com/NVIDIAGameWorks/PhysX-3.4.git "$SCRATCH/PhysX-3.4" \
  && git -C "$SCRATCH/PhysX-3.4" sparse-checkout set PhysX_3.4/Include PxShared/include \
  && git -C "$SCRATCH/PhysX-3.4" rev-parse HEAD \
  && mkdir -p extern/physx-3.4.2/PhysX_3.4 extern/physx-3.4.2/PxShared \
  && cp -r "$SCRATCH/PhysX-3.4/PhysX_3.4/Include" extern/physx-3.4.2/PhysX_3.4/ \
  && cp -r "$SCRATCH/PhysX-3.4/PxShared/include" extern/physx-3.4.2/PxShared/ \
  && sed -n '1,27p' "$SCRATCH/PhysX-3.4/README.md" > extern/physx-3.4.2/LICENSE.md \
  && grep -c 'PX_PHYSICS_VERSION_BUGFIX 2' extern/physx-3.4.2/PhysX_3.4/Include/PxPhysicsVersion.h \
  && find extern/physx-3.4.2 -type f | wc -l
```
Expected: the commit `5e42a5f112351a223c19c17bb331e6c55037b8eb`, then `1` (the header says 3.4.2), then `207` (206
headers plus `LICENSE.md`).

- [x] **Step 7: Write `extern/physx-3.4.2/README.md`**

```markdown
# NVIDIA PhysX 3.4.2 public headers

Copied unmodified from https://github.com/NVIDIAGameWorks/PhysX-3.4 at commit
5e42a5f112351a223c19c17bb331e6c55037b8eb ("PhysX 3.4, APEX 1.4 patch release @25354359"): `PhysX_3.4/Include`
and `PxShared/include` only. They match the `PhysX3_x64.dll` (file version 3.4.2.0) that Cyberpunk 2077 2.31
ships. The licence is BSD-3-Clause: see `LICENSE.md`, and the notice in each header.
```

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add .gitignore .gitattributes README.md .gitmodules extern \
  && git commit -q -m "Add repo basics, RED4ext.SDK submodule and PhysX 3.4.2 headers" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 2: Build skeleton, test harness and coordinates

**Files:**
- Create: `spikes/physx-probe/CMakeLists.txt`, `spikes/physx-probe/CMakePresets.json`
- Create: `tests/Harness.h`, `tests/TestMain.cpp`, `tests/TestCoords.cpp`
- Create: `src/Coords.h`

- [x] **Step 1: Write `spikes/physx-probe/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.25)

project(PhysXProbe VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(RED4EXT_SDK_DIR "${REPO_ROOT}/extern/RED4ext.SDK" CACHE PATH "RED4ext.SDK checkout")
set(PHYSX_DIR "${REPO_ROOT}/extern/physx-3.4.2" CACHE PATH "PhysX 3.4.2 headers (PhysX_3.4/Include, PxShared/include)")
set(CP2077_DIR "" CACHE PATH "Cyberpunk 2077 folder; when set, each build copies the plugin into red4ext/plugins/PhysXProbe")

# Shared settings. /EHa lets the crash guard (__try/__except) unwind C++ objects cleanly.
add_library(probe_settings INTERFACE)
target_compile_options(probe_settings INTERFACE /W4 /permissive- /utf-8 /EHa /Zc:__cplusplus)
target_compile_definitions(probe_settings INTERFACE WIN32_LEAN_AND_MEAN NOMINMAX PX_PHYSX_STATIC_LIB)
target_include_directories(probe_settings INTERFACE src)
target_include_directories(probe_settings SYSTEM INTERFACE "${PHYSX_DIR}/PhysX_3.4/Include" "${PHYSX_DIR}/PxShared/include")

# Unit tests: no game needed.
enable_testing()
add_executable(probe_tests
	tests/TestMain.cpp
	tests/TestCoords.cpp
)
target_link_libraries(probe_tests PRIVATE probe_settings)
add_test(NAME probe_tests COMMAND probe_tests)
```

- [x] **Step 2: Write `spikes/physx-probe/CMakePresets.json`**

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "default",
      "displayName": "Visual Studio 2022, x64",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "binaryDir": "${sourceDir}/build"
    }
  ],
  "buildPresets": [
    {
      "name": "release",
      "configurePreset": "default",
      "configuration": "RelWithDebInfo"
    }
  ],
  "testPresets": [
    {
      "name": "release",
      "configurePreset": "default",
      "configuration": "RelWithDebInfo",
      "output": { "outputOnFailure": true }
    }
  ]
}
```

- [x] **Step 3: Write the test harness, `tests/Harness.h`**

```cpp
#pragma once

// A tiny test harness: TEST(name) registers a test, CHECK* record failures, TestMain.cpp runs them all.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace th
{
struct Test
{
	const char* name;
	void (*fn)();
};

inline std::vector<Test>& Registry()
{
	static std::vector<Test> tests;
	return tests;
}

inline int& Failures()
{
	static int failures = 0;
	return failures;
}

struct Registrar
{
	Registrar(const char* aName, void (*aFn)()) { Registry().push_back({ aName, aFn }); }
};

inline void Fail(const char* aFile, int aLine, const std::string& aWhat)
{
	std::printf("  FAIL %s:%d: %s\n", aFile, aLine, aWhat.c_str());
	++Failures();
}
}

#define TH_CAT2(a, b) a##b
#define TH_CAT(a, b) TH_CAT2(a, b)
#define TEST(name)                                                   \
	static void name();                                              \
	static th::Registrar TH_CAT(registrar_, name)(#name, &name);     \
	static void name()

#define CHECK(cond)                                   \
	do                                                \
	{                                                 \
		if (!(cond))                                  \
		{                                             \
			th::Fail(__FILE__, __LINE__, #cond);      \
		}                                             \
	} while (0)

#define CHECK_EQ(a, b)                                                                                            \
	do                                                                                                            \
	{                                                                                                             \
		const auto thA = (a);                                                                                     \
		const auto thB = (b);                                                                                     \
		if (!(thA == thB))                                                                                        \
		{                                                                                                         \
			th::Fail(__FILE__, __LINE__, std::string(#a " == " #b " (") + std::to_string(thA) + " vs " +          \
			                                 std::to_string(thB) + ")");                                          \
		}                                                                                                         \
	} while (0)

#define CHECK_NEAR(a, b, eps)                                                                                     \
	do                                                                                                            \
	{                                                                                                             \
		const double thA = double(a);                                                                             \
		const double thB = double(b);                                                                             \
		if (!(std::fabs(thA - thB) <= (eps)))                                                                     \
		{                                                                                                         \
			th::Fail(__FILE__, __LINE__, std::string(#a " ~= " #b " (") + std::to_string(thA) + " vs " +          \
			                                 std::to_string(thB) + ")");                                          \
		}                                                                                                         \
	} while (0)

#define CHECK_STR(a, b)                                                                                           \
	do                                                                                                            \
	{                                                                                                             \
		const std::string thA = (a);                                                                              \
		const std::string thB = (b);                                                                              \
		if (thA != thB)                                                                                           \
		{                                                                                                         \
			th::Fail(__FILE__, __LINE__, std::string(#a " == " #b "\n--- got:\n") + thA + "\n--- expected:\n" + thB); \
		}                                                                                                         \
	} while (0)
```

- [x] **Step 4: Write `tests/TestMain.cpp`**

```cpp
#include "Harness.h"

int main()
{
	for (const auto& test : th::Registry())
	{
		const int before = th::Failures();
		test.fn();
		std::printf("%s %s\n", th::Failures() == before ? "PASS" : "FAIL", test.name);
	}
	std::printf("%zu test(s), %d failure(s)\n", th::Registry().size(), th::Failures());
	return th::Failures() == 0 ? 0 : 1;
}
```

- [x] **Step 5: Write the failing test, `tests/TestCoords.cpp`**

```cpp
#include "Harness.h"

#include "Coords.h"

using cpmc::McFromWorld;
using cpmc::Vec3d;
using cpmc::WorldFromMc;

TEST(McFromWorldMapsAxes)
{
	// World east (+X) stays east; world up (+Z) becomes Minecraft up (+Y); world north (+Y) becomes Minecraft -Z.
	const Vec3d mc = McFromWorld({ 1.0, 2.0, 3.0 });
	CHECK_NEAR(mc.x, 1.0, 0.0);
	CHECK_NEAR(mc.y, 3.0, 0.0);
	CHECK_NEAR(mc.z, -2.0, 0.0);
}

TEST(WorldFromMcUndoesMcFromWorld)
{
	const Vec3d world{ -123.5, 4567.25, 89.125 };
	const Vec3d back = WorldFromMc(McFromWorld(world));
	CHECK_NEAR(back.x, world.x, 0.0);
	CHECK_NEAR(back.y, world.y, 0.0);
	CHECK_NEAR(back.z, world.z, 0.0);
}

TEST(McFromWorldIsAProperRotation)
{
	// Mapped X cross mapped Y must equal mapped Z (a determinant of +1), so winding is preserved.
	const Vec3d x = McFromWorld({ 1, 0, 0 });
	const Vec3d y = McFromWorld({ 0, 1, 0 });
	const Vec3d z = McFromWorld({ 0, 0, 1 });
	CHECK_NEAR(x.y * y.z - x.z * y.y, z.x, 0.0);
	CHECK_NEAR(x.z * y.x - x.x * y.z, z.y, 0.0);
	CHECK_NEAR(x.x * y.y - x.y * y.x, z.z, 0.0);
}
```

- [x] **Step 6: Configure and build; the build must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"
```
Expected: `error C1083: Cannot open include file: 'Coords.h': No such file or directory`

- [x] **Step 7: Write `src/Coords.h`**

```cpp
#pragma once

// Game world (REDengine: metres, Z up, Y north) <-> Minecraft axes (Y up, Z south).
// 1 world unit = 1 block = 1 metre. The mapping is a proper rotation, so triangle winding is kept.
namespace cpmc
{
struct Vec3d
{
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

struct Vec3f
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

constexpr Vec3d McFromWorld(const Vec3d& aWorld)
{
	return { aWorld.x, aWorld.z, -aWorld.y };
}

constexpr Vec3d WorldFromMc(const Vec3d& aMc)
{
	return { aMc.x, -aMc.z, aMc.y };
}
}
```

- [x] **Step 8: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | tail -1
```
Expected: no lines from the build filter, then `3 test(s), 0 failure(s)`

- [x] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/physx-probe \
  && git commit -q -m "physx-probe: build skeleton, test harness and coordinates" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 3: JSON writer

**Files:**
- Create: `tests/TestJson.cpp`, `src/Json.h`, `src/Json.cpp`
- Modify: `spikes/physx-probe/CMakeLists.txt` (add `probe_core`; the tests link it)

- [x] **Step 1: Write the failing test, `tests/TestJson.cpp`**

```cpp
#include "Harness.h"

#include "Json.h"

#include <limits>

using cpmc::EscapeJson;
using cpmc::JsonWriter;

TEST(EscapesQuotesBackslashesAndControlCharacters)
{
	CHECK_STR(EscapeJson("say \"hi\""), "say \\\"hi\\\"");
	CHECK_STR(EscapeJson("C:\\Games"), "C:\\\\Games");
	CHECK_STR(EscapeJson("a\nb\tc"), "a\\nb\\tc");
	CHECK_STR(EscapeJson(std::string("x\x01y", 3)), "x\\u0001y");
	CHECK_STR(EscapeJson("Ñight City"), "Ñight City");  // UTF-8 passes through
}

TEST(CompactOutputHasNoWhitespace)
{
	JsonWriter json(false);
	json.BeginObject();
	json.Key("a").Int(1);
	json.Key("b").BeginArray().Bool(true).Null().Number(1.5).EndArray();
	json.Key("c").BeginObject().EndObject();
	json.Key("d").String("x");
	json.EndObject();
	CHECK_STR(json.Str(), R"({"a":1,"b":[true,null,1.5],"c":{},"d":"x"})");
}

TEST(PrettyOutputIndentsByTwoSpaces)
{
	JsonWriter json;
	json.BeginObject();
	json.Key("a").Int(1);
	json.Key("b").BeginArray().UInt(2).EndArray();
	json.EndObject();
	CHECK_STR(json.Str(), "{\n  \"a\": 1,\n  \"b\": [\n    2\n  ]\n}");
}

TEST(NonFiniteNumbersBecomeNull)
{
	JsonWriter json(false);
	json.BeginArray().Number(std::numeric_limits<double>::quiet_NaN()).Number(std::numeric_limits<double>::infinity()).EndArray();
	CHECK_STR(json.Str(), "[null,null]");
}
```

- [x] **Step 2: Add `probe_core` and the new test to `CMakeLists.txt`**

Insert this block right after the `probe_settings` block (before `# Unit tests`):
```cmake
# Everything that doesn't need RED4ext: geometry, checks, writers, the PhysX session and one dump.
add_library(probe_core STATIC
	src/Json.cpp
)
target_link_libraries(probe_core PUBLIC probe_settings version)
```
Then replace the `probe_tests` block with:
```cmake
add_executable(probe_tests
	tests/TestMain.cpp
	tests/TestCoords.cpp
	tests/TestJson.cpp
)
target_link_libraries(probe_tests PRIVATE probe_core)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C" | head -3
```
Expected: `error C1083: Cannot open include file: 'Json.h'`, or a missing `src/Json.cpp` source error.

- [x] **Step 4: Write `src/Json.h`**

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cpmc
{
// Escapes quotes, backslashes and control characters; other bytes (UTF-8) pass through.
std::string EscapeJson(std::string_view aText);

// A small JSON writer. Pretty output indents by two spaces; compact output has no whitespace at all.
class JsonWriter
{
public:
	explicit JsonWriter(bool aPretty = true);

	JsonWriter& BeginObject();
	JsonWriter& EndObject();
	JsonWriter& BeginArray();
	JsonWriter& EndArray();
	JsonWriter& Key(std::string_view aKey);
	JsonWriter& String(std::string_view aValue);
	JsonWriter& Number(double aValue);  // NaN and infinities are written as null
	JsonWriter& Int(int64_t aValue);
	JsonWriter& UInt(uint64_t aValue);
	JsonWriter& Bool(bool aValue);
	JsonWriter& Null();

	const std::string& Str() const { return out_; }

private:
	void BeforeItem();
	void Open(char aBracket);
	void Close(char aBracket);
	void Newline();

	std::string       out_;
	std::vector<bool> scopeHasItems_;
	bool              afterKey_ = false;
	bool              pretty_;
};
}
```

- [x] **Step 5: Write `src/Json.cpp`**

```cpp
#include "Json.h"

#include <cmath>
#include <format>

namespace cpmc
{
std::string EscapeJson(std::string_view aText)
{
	std::string out;
	out.reserve(aText.size());
	for (const char ch : aText)
	{
		const auto c = static_cast<unsigned char>(ch);
		switch (c)
		{
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\b': out += "\\b"; break;
		case '\f': out += "\\f"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (c < 0x20)
			{
				out += std::format("\\u{:04x}", unsigned(c));
			}
			else
			{
				out.push_back(ch);
			}
			break;
		}
	}
	return out;
}

JsonWriter::JsonWriter(bool aPretty)
	: pretty_(aPretty)
{
}

void JsonWriter::Newline()
{
	if (pretty_)
	{
		out_ += '\n';
		out_.append(scopeHasItems_.size() * 2, ' ');
	}
}

// Separates this item from the previous one in the current object or array.
void JsonWriter::BeforeItem()
{
	if (afterKey_)
	{
		afterKey_ = false;
		return;
	}
	if (!scopeHasItems_.empty())
	{
		if (scopeHasItems_.back())
		{
			out_ += ',';
		}
		scopeHasItems_.back() = true;
		Newline();
	}
}

void JsonWriter::Open(char aBracket)
{
	BeforeItem();
	out_ += aBracket;
	scopeHasItems_.push_back(false);
}

void JsonWriter::Close(char aBracket)
{
	const bool hadItems = scopeHasItems_.back();
	scopeHasItems_.pop_back();
	if (hadItems)
	{
		Newline();
	}
	out_ += aBracket;
}

JsonWriter& JsonWriter::BeginObject()
{
	Open('{');
	return *this;
}

JsonWriter& JsonWriter::EndObject()
{
	Close('}');
	return *this;
}

JsonWriter& JsonWriter::BeginArray()
{
	Open('[');
	return *this;
}

JsonWriter& JsonWriter::EndArray()
{
	Close(']');
	return *this;
}

JsonWriter& JsonWriter::Key(std::string_view aKey)
{
	BeforeItem();
	out_ += '"';
	out_ += EscapeJson(aKey);
	out_ += pretty_ ? "\": " : "\":";
	afterKey_ = true;
	return *this;
}

JsonWriter& JsonWriter::String(std::string_view aValue)
{
	BeforeItem();
	out_ += '"';
	out_ += EscapeJson(aValue);
	out_ += '"';
	return *this;
}

JsonWriter& JsonWriter::Number(double aValue)
{
	if (!std::isfinite(aValue))
	{
		return Null();
	}
	BeforeItem();
	out_ += std::format("{}", aValue);
	return *this;
}

JsonWriter& JsonWriter::Int(int64_t aValue)
{
	BeforeItem();
	out_ += std::format("{}", aValue);
	return *this;
}

JsonWriter& JsonWriter::UInt(uint64_t aValue)
{
	BeforeItem();
	out_ += std::format("{}", aValue);
	return *this;
}

JsonWriter& JsonWriter::Bool(bool aValue)
{
	BeforeItem();
	out_ += aValue ? "true" : "false";
	return *this;
}

JsonWriter& JsonWriter::Null()
{
	BeforeItem();
	out_ += "null";
	return *this;
}
}
```

- [x] **Step 6: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | tail -1
```
Expected: `7 test(s), 0 failure(s)`

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/physx-probe \
  && git commit -q -m "physx-probe: JSON writer" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 4: PhysX headers and primitive shapes

**Files:**
- Create: `tests/TestPrimitives.cpp`, `src/physx/PxHeaders.h`, `src/physx/ShapeTriangles.h`, `src/physx/ShapeTriangles.cpp`
- Modify: `spikes/physx-probe/CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestPrimitives.cpp`**

```cpp
#include "Harness.h"

#include "physx/ShapeTriangles.h"

#include <algorithm>
#include <map>
#include <tuple>

using namespace physx;
using cpmc::px::Tri;

namespace
{
using Key = std::tuple<float, float, float>;
using Edge = std::pair<Key, Key>;

Key K(const PxVec3& aV)
{
	return { aV.x, aV.y, aV.z };
}

// Every edge is shared by exactly two triangles, once in each direction: a closed, consistently wound mesh.
bool IsClosed(const std::vector<Tri>& aTris)
{
	std::map<Edge, int> directed;
	for (const Tri& t : aTris)
	{
		for (int k = 0; k < 3; ++k)
		{
			++directed[{ K(t.v[k]), K(t.v[(k + 1) % 3]) }];
		}
	}
	for (const auto& [edge, count] : directed)
	{
		const auto reverse = directed.find({ edge.second, edge.first });
		if (count != 1 || reverse == directed.end() || reverse->second != 1)
		{
			return false;
		}
	}
	return true;
}

// Every face normal points away from aCenter.
bool FacesOutward(const std::vector<Tri>& aTris, const PxVec3& aCenter)
{
	for (const Tri& t : aTris)
	{
		const PxVec3 normal = (t.v[1] - t.v[0]).cross(t.v[2] - t.v[0]);
		const PxVec3 centroid = (t.v[0] + t.v[1] + t.v[2]) / 3.0f;
		if (normal.dot(centroid - aCenter) <= 0.0f)
		{
			return false;
		}
	}
	return true;
}
}

TEST(BoxHas12OutwardTrianglesAndIsClosed)
{
	std::vector<Tri> tris;
	const PxTransform pose(PxVec3(10.0f, 20.0f, 30.0f), PxQuat(0.7f, PxVec3(0.0f, 0.0f, 1.0f)));
	cpmc::px::BoxTriangles(PxVec3(1.0f, 2.0f, 3.0f), pose, tris);
	CHECK_EQ(tris.size(), size_t(12));
	CHECK(IsClosed(tris));
	CHECK(FacesOutward(tris, pose.p));
	for (const Tri& t : tris)
	{
		for (const PxVec3& v : t.v)
		{
			const PxVec3 local = pose.transformInv(v);
			CHECK_NEAR(std::fabs(local.x), 1.0, 1e-4);
			CHECK_NEAR(std::fabs(local.y), 2.0, 1e-4);
			CHECK_NEAR(std::fabs(local.z), 3.0, 1e-4);
		}
	}
}

TEST(SphereHas48OutwardTrianglesOnItsRadius)
{
	std::vector<Tri> tris;
	const PxTransform pose(PxVec3(1.0f, 2.0f, 3.0f));
	cpmc::px::SphereTriangles(0.5f, pose, tris);
	CHECK_EQ(tris.size(), size_t(48));
	CHECK(IsClosed(tris));
	CHECK(FacesOutward(tris, pose.p));
	for (const Tri& t : tris)
	{
		for (const PxVec3& v : t.v)
		{
			CHECK_NEAR((v - pose.p).magnitude(), 0.5, 1e-4);
		}
	}
}

TEST(CapsuleHas64OutwardTrianglesAroundItsXAxis)
{
	std::vector<Tri> tris;
	const PxTransform pose(PxVec3(0.0f), PxQuat(PxIdentity));
	cpmc::px::CapsuleTriangles(0.4f, 0.5f, pose, tris);
	CHECK_EQ(tris.size(), size_t(64));
	CHECK(IsClosed(tris));
	CHECK(FacesOutward(tris, PxVec3(0.0f)));
	for (const Tri& t : tris)
	{
		for (const PxVec3& v : t.v)
		{
			// Distance to the segment from (-0.5, 0, 0) to (0.5, 0, 0) equals the radius.
			const float  x = std::clamp(v.x, -0.5f, 0.5f);
			const PxVec3 nearest(x, 0.0f, 0.0f);
			CHECK_NEAR((v - nearest).magnitude(), 0.4, 1e-4);
		}
	}
}

TEST(KeepIfTouchesUsesTheTrianglesBounds)
{
	const PxBounds3 box(PxVec3(0.0f), PxVec3(1.0f));
	const Tri inside{ { PxVec3(0.2f), PxVec3(0.3f), PxVec3(0.4f, 0.2f, 0.2f) } };
	const Tri touching{ { PxVec3(1.0f, 0.5f, 0.5f), PxVec3(2.0f, 0.5f, 0.5f), PxVec3(2.0f, 0.6f, 0.5f) } };
	const Tri outside{ { PxVec3(1.1f, 0.5f, 0.5f), PxVec3(2.0f, 0.5f, 0.5f), PxVec3(2.0f, 0.6f, 0.5f) } };
	const Tri spanning{ { PxVec3(-1.0f, 0.5f, 0.5f), PxVec3(2.0f, 0.5f, 0.5f), PxVec3(2.0f, 0.6f, 0.5f) } };
	CHECK(cpmc::px::KeepIfTouches(inside, box));
	CHECK(cpmc::px::KeepIfTouches(touching, box));
	CHECK(!cpmc::px::KeepIfTouches(outside, box));
	CHECK(cpmc::px::KeepIfTouches(spanning, box));
}
```

- [x] **Step 2: Add the source and the test to `CMakeLists.txt`**

Replace the `probe_core` and `probe_tests` source lists with:
```cmake
add_library(probe_core STATIC
	src/Json.cpp
	src/physx/ShapeTriangles.cpp
)
```
```cmake
add_executable(probe_tests
	tests/TestMain.cpp
	tests/TestCoords.cpp
	tests/TestJson.cpp
	tests/TestPrimitives.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C" | head -3
```
Expected: `error C1083: Cannot open include file: 'physx/ShapeTriangles.h'`, or a missing source error.

- [x] **Step 4: Write `src/physx/PxHeaders.h`**

```cpp
#pragma once

// The NVIDIA PhysX 3.4.2 public headers the probe uses. CMake defines PX_PHYSX_STATIC_LIB, which empties the
// core API macro. The probe only calls PhysX through virtual tables or inline header code, so the built DLL
// imports nothing from PhysX3*.dll (check with dumpbin /imports).
#include <PxActor.h>
#include <PxAggregate.h>
#include <PxPhysics.h>
#include <PxQueryFiltering.h>
#include <PxQueryReport.h>
#include <PxRigidActor.h>
#include <PxRigidBody.h>
#include <PxRigidDynamic.h>
#include <PxRigidStatic.h>
#include <PxScene.h>
#include <PxShape.h>
#include <common/PxTolerancesScale.h>
#include <common/PxTypeInfo.h>
#include <foundation/PxBounds3.h>
#include <foundation/PxTransform.h>
#include <geometry/PxBoxGeometry.h>
#include <geometry/PxCapsuleGeometry.h>
#include <geometry/PxConvexMesh.h>
#include <geometry/PxConvexMeshGeometry.h>
#include <geometry/PxHeightField.h>
#include <geometry/PxHeightFieldGeometry.h>
#include <geometry/PxHeightFieldSample.h>
#include <geometry/PxMeshScale.h>
#include <geometry/PxPlaneGeometry.h>
#include <geometry/PxSphereGeometry.h>
#include <geometry/PxTriangleMesh.h>
#include <geometry/PxTriangleMeshGeometry.h>
```

- [x] **Step 5: Write `src/physx/ShapeTriangles.h`**

Tasks 5, 6 and 11 add more declarations to this header.
```cpp
#pragma once

#include "physx/PxHeaders.h"

#include <cstdint>
#include <functional>
#include <vector>

// Turns PhysX shapes into world-space triangles. Everything except AppendTriangles works on plain data and
// is unit-tested without PhysX; AppendTriangles reads a live shape from the game.
namespace cpmc::px
{
struct Tri
{
	physx::PxVec3 v[3];
};

// True if the triangle's bounding box touches the box (touching edges count).
bool KeepIfTouches(const Tri& aTri, const physx::PxBounds3& aBox);

// Closed primitives with outward-facing triangles, placed by aPose.
void BoxTriangles(const physx::PxVec3& aHalfExtents, const physx::PxTransform& aPose, std::vector<Tri>& aOut);
// A UV sphere with 8 segments and 4 rings: 48 triangles, poles on the local X axis.
void SphereTriangles(float aRadius, const physx::PxTransform& aPose, std::vector<Tri>& aOut);
// The same sphere split at its equator, with an 8-segment cylinder between the halves: 64 triangles along local X.
void CapsuleTriangles(float aRadius, float aHalfHeight, const physx::PxTransform& aPose, std::vector<Tri>& aOut);
}
```

- [x] **Step 6: Write `src/physx/ShapeTriangles.cpp`**

Tasks 5, 6 and 11 append more functions to this file.
```cpp
#include "physx/ShapeTriangles.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

using namespace physx;

namespace cpmc::px
{
namespace
{
constexpr uint32_t kSegments = 8;
constexpr uint32_t kRings = 4;
constexpr float    kPi = 3.14159265358979f;

using Ring = std::array<PxVec3, kSegments>;

void Push(std::vector<Tri>& aOut, const PxVec3& aA, const PxVec3& aB, const PxVec3& aC)
{
	aOut.push_back({ { aA, aB, aC } });
}

// Latitude ring aLatitude (1 .. kRings-1) of a sphere whose poles are on X, shifted along X.
Ring MakeRing(uint32_t aLatitude, float aRadius, float aShiftX)
{
	Ring ring{};
	const float theta = kPi * float(aLatitude) / float(kRings);
	for (uint32_t j = 0; j < kSegments; ++j)
	{
		const float phi = 2.0f * kPi * float(j) / float(kSegments);
		ring[j] = PxVec3(std::cos(theta) * aRadius + aShiftX, std::sin(theta) * std::cos(phi) * aRadius,
		                 std::sin(theta) * std::sin(phi) * aRadius);
	}
	return ring;
}

// Quads between ring aA and ring aB, where aA is nearer the +X pole.
void Band(const Ring& aA, const Ring& aB, std::vector<Tri>& aOut)
{
	for (uint32_t j = 0; j < kSegments; ++j)
	{
		const uint32_t k = (j + 1) % kSegments;
		Push(aOut, aA[j], aB[j], aB[k]);
		Push(aOut, aA[j], aB[k], aA[k]);
	}
}

void CapPositive(const PxVec3& aPole, const Ring& aRing, std::vector<Tri>& aOut)
{
	for (uint32_t j = 0; j < kSegments; ++j)
	{
		Push(aOut, aPole, aRing[j], aRing[(j + 1) % kSegments]);
	}
}

void CapNegative(const PxVec3& aPole, const Ring& aRing, std::vector<Tri>& aOut)
{
	for (uint32_t j = 0; j < kSegments; ++j)
	{
		Push(aOut, aPole, aRing[(j + 1) % kSegments], aRing[j]);
	}
}

// Two hemispheres at +-aHalfHeight on X, joined by a cylinder when aHalfHeight > 0.
void Rounded(float aRadius, float aHalfHeight, const PxTransform& aPose, std::vector<Tri>& aOut)
{
	const size_t first = aOut.size();
	const Ring   ring1 = MakeRing(1, aRadius, aHalfHeight);
	const Ring   equatorPositive = MakeRing(2, aRadius, aHalfHeight);
	const Ring   equatorNegative = MakeRing(2, aRadius, -aHalfHeight);
	const Ring   ring3 = MakeRing(3, aRadius, -aHalfHeight);
	CapPositive(PxVec3(aRadius + aHalfHeight, 0.0f, 0.0f), ring1, aOut);
	Band(ring1, equatorPositive, aOut);
	if (aHalfHeight > 0.0f)
	{
		Band(equatorPositive, equatorNegative, aOut);
	}
	Band(aHalfHeight > 0.0f ? equatorNegative : equatorPositive, ring3, aOut);
	CapNegative(PxVec3(-aRadius - aHalfHeight, 0.0f, 0.0f), ring3, aOut);
	for (size_t i = first; i < aOut.size(); ++i)
	{
		for (PxVec3& v : aOut[i].v)
		{
			v = aPose.transform(v);
		}
	}
}
}

bool KeepIfTouches(const Tri& aTri, const PxBounds3& aBox)
{
	for (unsigned axis = 0; axis < 3; ++axis)
	{
		const float lo = std::min({ aTri.v[0][axis], aTri.v[1][axis], aTri.v[2][axis] });
		const float hi = std::max({ aTri.v[0][axis], aTri.v[1][axis], aTri.v[2][axis] });
		if (hi < aBox.minimum[axis] || lo > aBox.maximum[axis])
		{
			return false;
		}
	}
	return true;
}

void BoxTriangles(const PxVec3& aHalfExtents, const PxTransform& aPose, std::vector<Tri>& aOut)
{
	// Corner i: bit 0 picks +x, bit 1 +y, bit 2 +z. Each face is listed counter-clockwise seen from outside.
	std::array<PxVec3, 8> c{};
	for (uint32_t i = 0; i < 8; ++i)
	{
		const PxVec3 local((i & 1) ? aHalfExtents.x : -aHalfExtents.x, (i & 2) ? aHalfExtents.y : -aHalfExtents.y,
		                   (i & 4) ? aHalfExtents.z : -aHalfExtents.z);
		c[i] = aPose.transform(local);
	}
	static constexpr uint8_t kFaces[36] = {
		0, 4, 6, 0, 6, 2,  // -x
		1, 3, 7, 1, 7, 5,  // +x
		0, 1, 5, 0, 5, 4,  // -y
		2, 6, 7, 2, 7, 3,  // +y
		0, 2, 3, 0, 3, 1,  // -z
		4, 5, 7, 4, 7, 6,  // +z
	};
	for (uint32_t i = 0; i < 36; i += 3)
	{
		Push(aOut, c[kFaces[i]], c[kFaces[i + 1]], c[kFaces[i + 2]]);
	}
}

void SphereTriangles(float aRadius, const PxTransform& aPose, std::vector<Tri>& aOut)
{
	Rounded(aRadius, 0.0f, aPose, aOut);
}

void CapsuleTriangles(float aRadius, float aHalfHeight, const PxTransform& aPose, std::vector<Tri>& aOut)
{
	Rounded(aRadius, std::max(aHalfHeight, 0.0f), aPose, aOut);
}
}
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | tail -1
```
Expected: `11 test(s), 0 failure(s)`. No warnings from the PhysX headers (they're included as `SYSTEM`).

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/physx-probe \
  && git commit -q -m "physx-probe: PhysX headers and primitive shape triangles" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 5: Triangle meshes and convex hulls

**Files:**
- Create: `tests/TestMeshes.cpp`
- Modify: `src/physx/ShapeTriangles.h`, `src/physx/ShapeTriangles.cpp`, `spikes/physx-probe/CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestMeshes.cpp`**

```cpp
#include "Harness.h"

#include "physx/ShapeTriangles.h"

using namespace physx;
using cpmc::px::Tri;

namespace
{
const PxBounds3 kEverywhere(PxVec3(-1e6f), PxVec3(1e6f));

bool Near(const PxVec3& aA, const PxVec3& aB, float aEps = 1e-4f)
{
	return (aA - aB).magnitude() <= aEps;
}
}

TEST(MeshScaleMatchesPhysXWithRotation)
{
	// Scale 2 along the axis rotated from X to Y: a vertex on Y doubles, one on X stays.
	const PxMeshScale scale(PxVec3(2.0f, 1.0f, 1.0f), PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f)));
	CHECK(Near(cpmc::px::ApplyMeshScale(scale, PxVec3(0.0f, 1.0f, 0.0f)), PxVec3(0.0f, 2.0f, 0.0f)));
	CHECK(Near(cpmc::px::ApplyMeshScale(scale, PxVec3(1.0f, 0.0f, 0.0f)), PxVec3(1.0f, 0.0f, 0.0f)));
	CHECK(!cpmc::px::FlipsWinding(scale));
	CHECK(cpmc::px::FlipsWinding(PxMeshScale(PxVec3(-1.0f, 1.0f, 1.0f), PxQuat(PxIdentity))));
}

TEST(MeshTrianglesReadsBothIndexSizesAndFlipsMirroredMeshes)
{
	const PxVec3   vertices[4] = { PxVec3(0, 0, 0), PxVec3(1, 0, 0), PxVec3(0, 1, 0), PxVec3(0, 0, 1) };
	const uint16_t indices16[6] = { 0, 1, 2, 0, 2, 3 };
	const uint32_t indices32[6] = { 0, 1, 2, 0, 2, 3 };
	const PxTransform pose(PxVec3(10.0f, 0.0f, 0.0f));
	uint32_t          bad = 0;

	std::vector<Tri> from16;
	cpmc::px::MeshTriangles(vertices, 4, indices16, true, 2, PxMeshScale(), pose, kEverywhere, from16, bad);
	std::vector<Tri> from32;
	cpmc::px::MeshTriangles(vertices, 4, indices32, false, 2, PxMeshScale(), pose, kEverywhere, from32, bad);
	CHECK_EQ(from16.size(), size_t(2));
	CHECK_EQ(from32.size(), size_t(2));
	CHECK(Near(from16[0].v[1], PxVec3(11.0f, 0.0f, 0.0f)));
	CHECK(Near(from32[1].v[2], PxVec3(10.0f, 0.0f, 1.0f)));

	std::vector<Tri> mirrored;
	cpmc::px::MeshTriangles(vertices, 4, indices16, true, 1, PxMeshScale(PxVec3(-1.0f, 1.0f, 1.0f), PxQuat(PxIdentity)),
	                        pose, kEverywhere, mirrored, bad);
	CHECK_EQ(mirrored.size(), size_t(1));
	CHECK(Near(mirrored[0].v[1], PxVec3(10.0f, 1.0f, 0.0f)));  // v1 and v2 swapped
	CHECK(Near(mirrored[0].v[2], PxVec3(9.0f, 0.0f, 0.0f)));
	CHECK_EQ(bad, uint32_t(0));
}

TEST(MeshTrianglesSkipsBadIndicesAndCullsByRegion)
{
	const PxVec3   vertices[3] = { PxVec3(0, 0, 0), PxVec3(1, 0, 0), PxVec3(0, 1, 0) };
	const uint32_t indices[6] = { 0, 1, 2, 0, 1, 7 };
	uint32_t       bad = 0;
	std::vector<Tri> tris;
	cpmc::px::MeshTriangles(vertices, 3, indices, false, 2, PxMeshScale(), PxTransform(PxIdentity), kEverywhere, tris, bad);
	CHECK_EQ(tris.size(), size_t(1));
	CHECK_EQ(bad, uint32_t(1));

	std::vector<Tri> culled;
	const PxBounds3  farAway(PxVec3(50.0f), PxVec3(60.0f));
	cpmc::px::MeshTriangles(vertices, 3, indices, false, 1, PxMeshScale(), PxTransform(PxIdentity), farAway, culled, bad);
	CHECK_EQ(culled.size(), size_t(0));
}

TEST(ConvexPolygonsAreFanTriangulated)
{
	// A square and a pentagon: 2 + 3 triangles.
	const PxVec3 vertices[9] = { PxVec3(0, 0, 0), PxVec3(1, 0, 0), PxVec3(1, 1, 0), PxVec3(0, 1, 0),
		                         PxVec3(0, 0, 2), PxVec3(1, 0, 2), PxVec3(1.5f, 1, 2), PxVec3(0.5f, 2, 2), PxVec3(-0.5f, 1, 2) };
	const uint8_t indexBuffer[9] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
	PxHullPolygon polygons[2]{};
	polygons[0].mNbVerts = 4;
	polygons[0].mIndexBase = 0;
	polygons[1].mNbVerts = 5;
	polygons[1].mIndexBase = 4;
	std::vector<Tri> tris;
	cpmc::px::ConvexPolygonTriangles(vertices, 9, indexBuffer, polygons, 2, PxMeshScale(), PxTransform(PxIdentity), tris);
	CHECK_EQ(tris.size(), size_t(5));
	CHECK(Near(tris[0].v[0], vertices[0]));
	CHECK(Near(tris[0].v[1], vertices[1]));
	CHECK(Near(tris[0].v[2], vertices[2]));
	CHECK(Near(tris[1].v[1], vertices[2]));
	CHECK(Near(tris[1].v[2], vertices[3]));
	CHECK(Near(tris[4].v[0], vertices[4]));

	// A polygon that points past the vertex array is skipped.
	polygons[1].mIndexBase = 6;  // uses indices 6..10; 9 and 10 are past the buffer's vertices
	const uint8_t badIndexBuffer[11] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	std::vector<Tri> skipped;
	cpmc::px::ConvexPolygonTriangles(vertices, 9, badIndexBuffer, polygons, 2, PxMeshScale(), PxTransform(PxIdentity), skipped);
	CHECK_EQ(skipped.size(), size_t(2));
}
```

- [x] **Step 2: Add the test to `CMakeLists.txt`**

Add `tests/TestMeshes.cpp` as the last entry of the `probe_tests` list.

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C" | head -3
```
Expected: `error C2039: 'ApplyMeshScale': is not a member of 'cpmc::px'` (or C3861 for the same name).

- [x] **Step 4: Declare the mesh functions in `src/physx/ShapeTriangles.h`**

Add these just before the namespace's closing `}`:
```cpp

// PhysX's mesh scale: rotation^-1 * scale * rotation (PxMeshScale::transform).
physx::PxVec3 ApplyMeshScale(const physx::PxMeshScale& aScale, const physx::PxVec3& aVertex);
// A negative scale determinant mirrors the mesh, which reverses triangle winding.
bool FlipsWinding(const physx::PxMeshScale& aScale);

// A triangle mesh from its raw arrays. Triangles that reference a missing vertex are skipped and counted.
void MeshTriangles(const physx::PxVec3* aVertices, uint32_t aVertexCount, const void* aIndices, bool aIndices16,
                   uint32_t aTriangleCount, const physx::PxMeshScale& aScale, const physx::PxTransform& aPose,
                   const physx::PxBounds3& aRegion, std::vector<Tri>& aOut, uint32_t& aBadTriangles);

// Convex hull polygons, fan-triangulated. Polygons that reference a missing vertex are skipped.
void ConvexPolygonTriangles(const physx::PxVec3* aVertices, uint32_t aVertexCount, const uint8_t* aIndexBuffer,
                            const physx::PxHullPolygon* aPolygons, uint32_t aPolygonCount,
                            const physx::PxMeshScale& aScale, const physx::PxTransform& aPose, std::vector<Tri>& aOut);
```

- [x] **Step 5: Implement them in `src/physx/ShapeTriangles.cpp`**

Add these just before the file's last closing `}` (the end of `namespace cpmc::px`):
```cpp

PxVec3 ApplyMeshScale(const PxMeshScale& aScale, const PxVec3& aVertex)
{
	return aScale.transform(aVertex);
}

bool FlipsWinding(const PxMeshScale& aScale)
{
	return aScale.scale.x * aScale.scale.y * aScale.scale.z < 0.0f;
}

void MeshTriangles(const PxVec3* aVertices, uint32_t aVertexCount, const void* aIndices, bool aIndices16,
                   uint32_t aTriangleCount, const PxMeshScale& aScale, const PxTransform& aPose,
                   const PxBounds3& aRegion, std::vector<Tri>& aOut, uint32_t& aBadTriangles)
{
	const bool  flip = FlipsWinding(aScale);
	const auto* indices16 = static_cast<const uint16_t*>(aIndices);
	const auto* indices32 = static_cast<const uint32_t*>(aIndices);
	for (uint32_t t = 0; t < aTriangleCount; ++t)
	{
		uint32_t index[3];
		for (uint32_t k = 0; k < 3; ++k)
		{
			index[k] = aIndices16 ? indices16[t * 3 + k] : indices32[t * 3 + k];
		}
		if (index[0] >= aVertexCount || index[1] >= aVertexCount || index[2] >= aVertexCount)
		{
			++aBadTriangles;
			continue;
		}
		Tri tri{};
		for (uint32_t k = 0; k < 3; ++k)
		{
			tri.v[k] = aPose.transform(ApplyMeshScale(aScale, aVertices[index[k]]));
		}
		if (flip)
		{
			std::swap(tri.v[1], tri.v[2]);
		}
		if (KeepIfTouches(tri, aRegion))
		{
			aOut.push_back(tri);
		}
	}
}

void ConvexPolygonTriangles(const PxVec3* aVertices, uint32_t aVertexCount, const uint8_t* aIndexBuffer,
                            const PxHullPolygon* aPolygons, uint32_t aPolygonCount, const PxMeshScale& aScale,
                            const PxTransform& aPose, std::vector<Tri>& aOut)
{
	const bool flip = FlipsWinding(aScale);
	for (uint32_t p = 0; p < aPolygonCount; ++p)
	{
		const PxHullPolygon& polygon = aPolygons[p];
		const uint8_t*       index = aIndexBuffer + polygon.mIndexBase;
		if (polygon.mNbVerts < 3 ||
		    std::any_of(index, index + polygon.mNbVerts, [&](uint8_t aIndex) { return aIndex >= aVertexCount; }))
		{
			continue;
		}
		const PxVec3 first = aPose.transform(ApplyMeshScale(aScale, aVertices[index[0]]));
		for (uint32_t k = 1; k + 1 < polygon.mNbVerts; ++k)
		{
			PxVec3 b = aPose.transform(ApplyMeshScale(aScale, aVertices[index[k]]));
			PxVec3 c = aPose.transform(ApplyMeshScale(aScale, aVertices[index[k + 1]]));
			if (flip)
			{
				std::swap(b, c);
			}
			Push(aOut, first, b, c);
		}
	}
}
```

- [x] **Step 6: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | tail -1
```
Expected: `15 test(s), 0 failure(s)`

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/physx-probe \
  && git commit -q -m "physx-probe: triangle mesh and convex hull triangles" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 6: Heightfields

**Files:**
- Create: `tests/TestHeightfield.cpp`
- Modify: `src/physx/ShapeTriangles.h`, `src/physx/ShapeTriangles.cpp`, `spikes/physx-probe/CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestHeightfield.cpp`**

The expected vertex orders are PhysX's own, from `Gu::HeightField::getTriangleVertexIndices` in the 3.4.2 source.
```cpp
#include "Harness.h"

#include "physx/ShapeTriangles.h"

using namespace physx;
using cpmc::px::CellRange;
using cpmc::px::HeightfieldView;
using cpmc::px::Tri;

namespace
{
const PxBounds3 kEverywhere(PxVec3(-1e6f), PxVec3(1e6f));

struct Grid
{
	uint32_t                          rows;
	uint32_t                          columns;
	std::vector<PxHeightFieldSample>  samples;

	Grid(uint32_t aRows, uint32_t aColumns)
		: rows(aRows)
		, columns(aColumns)
		, samples(size_t(aRows) * aColumns)
	{
		for (auto& sample : samples)
		{
			sample.height = 0;
			sample.materialIndex0 = PxBitAndByte(0);
			sample.materialIndex1 = PxBitAndByte(0);
		}
	}

	PxHeightFieldSample& At(uint32_t aRow, uint32_t aColumn) { return samples[size_t(aRow) * columns + aColumn]; }

	HeightfieldView View(float aHeightScale = 1.0f, float aRowScale = 1.0f, float aColumnScale = 1.0f)
	{
		HeightfieldView view;
		view.rows = rows;
		view.columns = columns;
		view.heightScale = aHeightScale;
		view.rowScale = aRowScale;
		view.columnScale = aColumnScale;
		view.sample = [this](uint32_t aRow, uint32_t aColumn) { return At(aRow, aColumn); };
		return view;
	}
};

bool Near(const PxVec3& aA, const PxVec3& aB)
{
	return (aA - aB).magnitude() <= 1e-4f;
}

std::vector<Tri> AllTriangles(Grid& aGrid, const HeightfieldView& aView)
{
	std::vector<Tri> tris;
	cpmc::px::HeightfieldTriangles(aView, CellRange{ 0, aGrid.rows - 1, 0, aGrid.columns - 1 }, PxTransform(PxIdentity),
	                               kEverywhere, tris);
	return tris;
}
}

TEST(A3x3GridHas8Triangles)
{
	Grid grid(3, 3);
	CHECK_EQ(AllTriangles(grid, grid.View()).size(), size_t(8));
}

TEST(HeightfieldScalesPlaceSamples)
{
	// Rows along local X, height along local Y, columns along local Z.
	Grid grid(2, 2);
	grid.At(1, 1).height = 4;
	const auto tris = AllTriangles(grid, grid.View(0.5f, 2.0f, 3.0f));
	CHECK_EQ(tris.size(), size_t(2));
	// With the tess flag clear, the second triangle starts at (r+1, c+1).
	CHECK(Near(tris[1].v[0], PxVec3(2.0f, 2.0f, 3.0f)));
}

TEST(TessFlagPicksTheDiagonalLikePhysX)
{
	Grid grid(2, 2);
	const PxVec3 v00(0, 0, 0), v01(0, 0, 1), v10(1, 0, 0), v11(1, 0, 1);

	auto clear = AllTriangles(grid, grid.View());
	CHECK(Near(clear[0].v[0], v00) && Near(clear[0].v[1], v01) && Near(clear[0].v[2], v10));
	CHECK(Near(clear[1].v[0], v11) && Near(clear[1].v[1], v10) && Near(clear[1].v[2], v01));

	grid.At(0, 0).setTessFlag();
	auto set = AllTriangles(grid, grid.View());
	CHECK(Near(set[0].v[0], v10) && Near(set[0].v[1], v00) && Near(set[0].v[2], v11));
	CHECK(Near(set[1].v[0], v01) && Near(set[1].v[1], v11) && Near(set[1].v[2], v00));
}

TEST(HeightfieldTrianglesFaceUp)
{
	Grid grid(2, 2);
	for (bool tess : { false, true })
	{
		if (tess)
		{
			grid.At(0, 0).setTessFlag();
		}
		for (const Tri& t : AllTriangles(grid, grid.View()))
		{
			CHECK((t.v[1] - t.v[0]).cross(t.v[2] - t.v[0]).y > 0.0f);
		}
	}
}

TEST(HoleMaterialRemovesOneTriangle)
{
	const PxU8 hole = PxU8(PxHeightFieldMaterial::eHOLE);
	Grid       grid(3, 3);
	grid.At(0, 0).materialIndex0 = PxBitAndByte(hole);
	CHECK_EQ(AllTriangles(grid, grid.View()).size(), size_t(7));

	Grid second(2, 2);
	second.At(0, 0).materialIndex1 = PxBitAndByte(hole);
	const auto tris = AllTriangles(second, second.View());
	CHECK_EQ(tris.size(), size_t(1));
	CHECK(Near(tris[0].v[0], PxVec3(0, 0, 0)));  // the first triangle survives

	// The tess bit shares materialIndex0's byte but isn't part of the material.
	Grid tessOnly(2, 2);
	tessOnly.At(0, 0).setTessFlag();
	CHECK_EQ(AllTriangles(tessOnly, tessOnly.View()).size(), size_t(2));
}

TEST(OnlyCellsUnderTheRegionAreVisited)
{
	Grid            grid(5, 5);  // 4 x 4 cells
	HeightfieldView view = grid.View();
	const PxBounds3 box(PxVec3(1.5f, -10.0f, 0.2f), PxVec3(2.5f, 10.0f, 0.8f));
	const CellRange cells = cpmc::px::HeightfieldCellsUnder(view, PxTransform(PxIdentity), box);
	CHECK_EQ(cells.r0, uint32_t(1));
	CHECK_EQ(cells.r1, uint32_t(3));
	CHECK_EQ(cells.c0, uint32_t(0));
	CHECK_EQ(cells.c1, uint32_t(1));
	std::vector<Tri> tris;
	cpmc::px::HeightfieldTriangles(view, cells, PxTransform(PxIdentity), kEverywhere, tris);
	CHECK_EQ(tris.size(), size_t(4));

	// A box off the grid gives no cells.
	const PxBounds3 off(PxVec3(10.0f, -1.0f, 10.0f), PxVec3(11.0f, 1.0f, 11.0f));
	CHECK(cpmc::px::HeightfieldCellsUnder(view, PxTransform(PxIdentity), off).Empty());
}
```

- [x] **Step 2: Add the test to `CMakeLists.txt`**

Add `tests/TestHeightfield.cpp` as the last entry of the `probe_tests` list.

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C" | head -3
```
Expected: `error C2039: 'CellRange': is not a member of 'cpmc::px'` (or similar for `HeightfieldView`).

- [x] **Step 4: Declare the heightfield types and functions in `src/physx/ShapeTriangles.h`**

Add these just before the namespace's closing `}`:
```cpp

// A heightfield's samples and scales. Sample (r, c) sits at local (r * rowScale, height * heightScale,
// c * columnScale): rows along local X, height along local Y, columns along local Z.
struct HeightfieldView
{
	uint32_t rows = 0;
	uint32_t columns = 0;
	float heightScale = 1.0f;
	float rowScale = 1.0f;
	float columnScale = 1.0f;
	std::function<physx::PxHeightFieldSample(uint32_t aRow, uint32_t aColumn)> sample;
};

// Cells [r0, r1) x [c0, c1). Cell (r, c) spans samples r..r+1 and c..c+1.
struct CellRange
{
	uint32_t r0 = 0;
	uint32_t r1 = 0;
	uint32_t c0 = 0;
	uint32_t c1 = 0;

	bool Empty() const { return r0 >= r1 || c0 >= c1; }
};

// The cells of a heightfield placed at aPose that lie under a world-space box.
CellRange HeightfieldCellsUnder(const HeightfieldView& aField, const physx::PxTransform& aPose, const physx::PxBounds3& aBox);

// The cells' triangles in PhysX's own vertex order (Gu::HeightField::getTriangleVertexIndices). A cell's first
// triangle uses materialIndex0 and its second materialIndex1; material 127 (a hole) leaves the triangle out.
void HeightfieldTriangles(const HeightfieldView& aField, const CellRange& aCells, const physx::PxTransform& aPose,
                          const physx::PxBounds3& aRegion, std::vector<Tri>& aOut);
```

- [x] **Step 5: Implement them in `src/physx/ShapeTriangles.cpp`**

Add these just before the file's last closing `}`:
```cpp

namespace
{
constexpr PxU8 kHoleMaterial = PxHeightFieldMaterial::eHOLE;
}

CellRange HeightfieldCellsUnder(const HeightfieldView& aField, const PxTransform& aPose, const PxBounds3& aBox)
{
	CellRange cells;
	if (aField.rows < 2 || aField.columns < 2 || aField.rowScale <= 0.0f || aField.columnScale <= 0.0f)
	{
		return cells;
	}
	PxBounds3 local = PxBounds3::empty();
	for (uint32_t i = 0; i < 8; ++i)
	{
		const PxVec3 corner((i & 1) ? aBox.maximum.x : aBox.minimum.x, (i & 2) ? aBox.maximum.y : aBox.minimum.y,
		                    (i & 4) ? aBox.maximum.z : aBox.minimum.z);
		local.include(aPose.transformInv(corner));
	}
	const auto clampTo = [](double aValue, uint32_t aMax) { return uint32_t(std::clamp(aValue, 0.0, double(aMax))); };
	const uint32_t rowEnd = aField.rows - 1;  // cells exist for rows 0 .. rows-2
	const uint32_t columnEnd = aField.columns - 1;
	cells.r0 = clampTo(std::floor(double(local.minimum.x) / aField.rowScale), rowEnd);
	cells.r1 = clampTo(std::floor(double(local.maximum.x) / aField.rowScale) + 1.0, rowEnd);
	cells.c0 = clampTo(std::floor(double(local.minimum.z) / aField.columnScale), columnEnd);
	cells.c1 = clampTo(std::floor(double(local.maximum.z) / aField.columnScale) + 1.0, columnEnd);
	return cells;
}

void HeightfieldTriangles(const HeightfieldView& aField, const CellRange& aCells, const PxTransform& aPose,
                          const PxBounds3& aRegion, std::vector<Tri>& aOut)
{
	const auto vertex = [&](uint32_t aRow, uint32_t aColumn) {
		const PxHeightFieldSample sample = aField.sample(aRow, aColumn);
		const PxVec3 local(float(aRow) * aField.rowScale, float(sample.height) * aField.heightScale,
		                   float(aColumn) * aField.columnScale);
		return aPose.transform(local);
	};
	for (uint32_t r = aCells.r0; r < aCells.r1; ++r)
	{
		for (uint32_t c = aCells.c0; c < aCells.c1; ++c)
		{
			const PxHeightFieldSample cell = aField.sample(r, c);
			const PxU8   material0 = cell.materialIndex0;  // the conversion drops the tess bit
			const PxU8   material1 = cell.materialIndex1;
			const PxVec3 v00 = vertex(r, c);
			const PxVec3 v01 = vertex(r, c + 1);
			const PxVec3 v10 = vertex(r + 1, c);
			const PxVec3 v11 = vertex(r + 1, c + 1);
			Tri first{};
			Tri second{};
			if (cell.tessFlag())
			{
				first = { { v10, v00, v11 } };  // diagonal (r, c) - (r+1, c+1)
				second = { { v01, v11, v00 } };
			}
			else
			{
				first = { { v00, v01, v10 } };  // diagonal (r, c+1) - (r+1, c)
				second = { { v11, v10, v01 } };
			}
			if (material0 != kHoleMaterial && KeepIfTouches(first, aRegion))
			{
				aOut.push_back(first);
			}
			if (material1 != kHoleMaterial && KeepIfTouches(second, aRegion))
			{
				aOut.push_back(second);
			}
		}
	}
}
```

- [x] **Step 6: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | tail -1
```
Expected: `21 test(s), 0 failure(s)`

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/physx-probe \
  && git commit -q -m "physx-probe: heightfield triangles, following PhysX's vertex order" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 7: Report data and the pure compatibility checks

**Files:**
- Create: `tests/TestChecks.cpp`, `src/Report.h`, `src/physx/Checks.h`, `src/physx/Checks.cpp`
- Modify: `spikes/physx-probe/CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestChecks.cpp`**

```cpp
#include "Harness.h"

#include "physx/Checks.h"

using namespace physx;
using namespace cpmc::px;

TEST(ConcreteTypeNamesMapToTheirIds)
{
	CHECK_EQ(ConcreteTypeForName("PxRigidStatic"), int(PxConcreteType::eRIGID_STATIC));
	CHECK_EQ(ConcreteTypeForName("PxRigidDynamic"), int(PxConcreteType::eRIGID_DYNAMIC));
	CHECK_EQ(ConcreteTypeForName("PxShape"), int(PxConcreteType::eSHAPE));
	CHECK_EQ(ConcreteTypeForName("PxBVH33TriangleMesh"), int(PxConcreteType::eTRIANGLE_MESH_BVH33));
	CHECK_EQ(ConcreteTypeForName("PxBVH34TriangleMesh"), int(PxConcreteType::eTRIANGLE_MESH_BVH34));
	CHECK_EQ(ConcreteTypeForName("PxHeightField"), int(PxConcreteType::eHEIGHTFIELD));
	CHECK_EQ(ConcreteTypeForName("PxConvexMesh"), int(PxConcreteType::eCONVEX_MESH));
	CHECK_EQ(ConcreteTypeForName("PxTriangleMesh"), -1);  // an interface name, never a concrete type
	CHECK_EQ(ConcreteTypeForName("garbage"), -1);
	CHECK_EQ(ConcreteTypeForName(nullptr), -1);
}

TEST(PlausibilityRanges)
{
	CHECK(TolerancesPlausible(1.0f, 10.0f));
	CHECK(!TolerancesPlausible(0.0f, 10.0f));
	CHECK(!TolerancesPlausible(1.0f, 5000.0f));
	CHECK(GravityFinite(PxVec3(0.0f, 0.0f, -9.81f)));
	CHECK(GravityFinite(PxVec3(0.0f)));
	CHECK(!GravityFinite(PxVec3(0.0f, 0.0f, std::nanf(""))));
	CHECK(!GravityFinite(PxVec3(0.0f, 0.0f, -1000.0f)));
	CHECK(GravityEarthLike(PxVec3(0.0f, 0.0f, -9.81f)));
	CHECK(!GravityEarthLike(PxVec3(0.0f)));
}

TEST(GeometryMustMatchItsMesh)
{
	CHECK(GeometryMatchesMesh(PxGeometryType::eTRIANGLEMESH, PxConcreteType::eTRIANGLE_MESH_BVH33));
	CHECK(GeometryMatchesMesh(PxGeometryType::eTRIANGLEMESH, PxConcreteType::eTRIANGLE_MESH_BVH34));
	CHECK(GeometryMatchesMesh(PxGeometryType::eCONVEXMESH, PxConcreteType::eCONVEX_MESH));
	CHECK(GeometryMatchesMesh(PxGeometryType::eHEIGHTFIELD, PxConcreteType::eHEIGHTFIELD));
	CHECK(!GeometryMatchesMesh(PxGeometryType::eTRIANGLEMESH, PxConcreteType::eCONVEX_MESH));
	CHECK(!GeometryMatchesMesh(PxGeometryType::eBOX, PxConcreteType::eCONVEX_MESH));
}

TEST(CategoriesFollowTheKindOrder)
{
	const PxShapeFlags trigger = PxShapeFlag::eTRIGGER_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE;
	const PxShapeFlags queryOnly = PxShapeFlag::eSCENE_QUERY_SHAPE;
	const PxShapeFlags solid = PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE;
	CHECK_STR(Category("static", trigger, PxGeometryType::eBOX), "trigger_box");
	CHECK_STR(Category("dynamic", queryOnly, PxGeometryType::eCONVEXMESH), "queryonly_convex");
	CHECK_STR(Category("static", solid, PxGeometryType::eTRIANGLEMESH), "static_trimesh");
	CHECK_STR(Category("kinematic", solid, PxGeometryType::eCAPSULE), "kinematic_capsule");
	CHECK_STR(Category("static", solid, PxGeometryType::eHEIGHTFIELD), "static_heightfield");
	CHECK_STR(GeometryName(PxGeometryType::eHEIGHTFIELD), "heightField");
}

TEST(FilterKeysAreHex)
{
	CHECK_STR(FilterKey(PxFilterData(1, 0xff, 0, 0x10)), "1:ff:0:10");
}

TEST(SanitizeNameKeepsPrintableAscii)
{
	CHECK_STR(SanitizeName("base\\world\\door_01"), "base\\world\\door_01");
	CHECK_STR(SanitizeName("a\tb\x01" "c"), "a?b?c");
	CHECK_STR(SanitizeName("abcdef", 3), "abc");
	CHECK_STR(SanitizeName(nullptr), "");
}

TEST(TrimTallyKeepsTheMostCommon)
{
	const cpmc::Tally tally{ { "a", 5 }, { "b", 1 }, { "c", 9 }, { "d", 1 } };
	const cpmc::Tally trimmed = TrimTally(tally, 2);
	CHECK_EQ(trimmed.size(), size_t(3));
	CHECK_EQ(trimmed.at("c"), uint64_t(9));
	CHECK_EQ(trimmed.at("a"), uint64_t(5));
	CHECK_EQ(trimmed.at("other"), uint64_t(2));
	CHECK_EQ(TrimTally(tally, 10).size(), size_t(4));  // nothing to fold, so no "other"
}
```

- [x] **Step 2: Add the source and test to `CMakeLists.txt`**

Add `src/physx/Checks.cpp` as the last entry of the `probe_core` list, and `tests/TestChecks.cpp` as the last entry of
the `probe_tests` list.

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C" | head -3
```
Expected: `error C1083: Cannot open include file: 'physx/Checks.h'`, or a missing source error.

- [x] **Step 4: Write `src/Report.h`**

```cpp
#pragma once

#include "Coords.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Plain data for one dump's report.json (see the spec's report schema).
namespace cpmc
{
using Tally = std::map<std::string, uint64_t>;

struct CameraPose
{
	Vec3d position;
	Vec3f forward;
	float fov = 0.0f;
};

struct PlayerPose
{
	Vec3d feet;
	Vec3f forward;
	std::optional<CameraPose> camera;
};

struct CompatStep
{
	int step = 0;
	std::string name;
	bool pass = false;
	std::string detail;
};

struct SceneCensus
{
	int index = 0;
	Vec3f gravity;
	uint32_t flags = 0;
	bool requiresRwLock = false;
	uint32_t staticActors = 0;
	uint32_t dynamicActors = 0;
	uint32_t aggregates = 0;
	uint32_t actorsInAggregates = 0;
	uint32_t articulations = 0;
	Tally byConcreteType;
	Tally byGeometryType;
	Tally shapeFlags;
	Tally queryFilter;
	Tally simulationFilter;
	uint32_t namedActors = 0;
	uint32_t actorsWithUserData = 0;
	std::vector<std::string> actorNameSamples;
	uint64_t mismatches = 0;
	double censusMs = 0.0;
};

struct RegionResult
{
	Vec3d center;
	Vec3f halfExtents;
	uint32_t boundsSelectedActors = 0;
	uint32_t overlapHits = 0;
	bool overlapOverflow = false;
	double selectMs = 0.0;
	double overlapMs = 0.0;
	double extractMs = 0.0;
	uint64_t triangles = 0;
	Tally trianglesByCategory;
	uint32_t skippedPlanes = 0;
	uint32_t unknownGeometry = 0;
	uint32_t badTriangles = 0;
	bool triangleCapHit = false;
};

struct NearestCapsule
{
	double distance = 0.0;
	float radius = 0.0f;
	float halfHeight = 0.0f;
	std::string kind;
};

struct CoordinateCheck
{
	bool rayHit = false;
	double hitDistance = 0.0;
	int scene = -1;
	bool pass = false;
	std::optional<NearestCapsule> nearestCapsule;
};

struct DumpReport
{
	std::string probeVersion = "0.1.0";
	std::string time;
	std::string trigger;
	std::string runtime;
	std::string physxFileVersion;
	std::optional<PlayerPose> player;
	bool compatibilityPass = false;
	std::vector<CompatStep> steps;
	uint64_t censusMismatches = 0;
	std::vector<SceneCensus> scenes;
	std::optional<RegionResult> region;
	std::optional<CoordinateCheck> coordinateCheck;
	double mainThreadPauseMs = 0.0;
	uint32_t faults = 0;
	bool objWritten = false;
};
}
```

- [x] **Step 5: Write `src/physx/Checks.h`**

```cpp
#pragma once

#include "Report.h"
#include "physx/PxHeaders.h"

#include <cstddef>
#include <string>

// The pure parts of the compatibility check and the census bookkeeping. No PhysX calls happen here.
namespace cpmc::px
{
// The PhysX 3.4.2 concrete type ID (PxConcreteType) for a type name, or -1 for a name 3.4.2 doesn't have.
int ConcreteTypeForName(const char* aName);

// PxTolerancesScale: length 0.01 .. 100, speed 0.1 .. 1000.
bool TolerancesPlausible(float aLength, float aSpeed);
// Finite, with a magnitude of at most 100.
bool GravityFinite(const physx::PxVec3& aGravity);
// A magnitude between 1 and 30.
bool GravityEarthLike(const physx::PxVec3& aGravity);

// A mesh-based geometry type references a mesh of the matching concrete type.
bool GeometryMatchesMesh(physx::PxGeometryType::Enum aGeometry, int aMeshConcreteType);

// "triangleMesh", "heightField", "convexMesh", "box", "sphere", "capsule", "plane" (report tallies).
const char* GeometryName(physx::PxGeometryType::Enum aGeometry);
// "trimesh", "heightfield", "convex", "box", "sphere", "capsule", "plane" (OBJ categories).
const char* GeometryShortName(physx::PxGeometryType::Enum aGeometry);

// The category of a shape: "trigger" if it has the trigger flag, otherwise "queryonly" if it has the
// scene-query flag but not the simulation flag, otherwise the actor's kind; then "_" and the short geometry name.
std::string Category(const char* aActorKind, physx::PxShapeFlags aShapeFlags, physx::PxGeometryType::Enum aGeometry);

// Filter data as "w0:w1:w2:w3" in hexadecimal.
std::string FilterKey(const physx::PxFilterData& aData);

// At most aMaxLength characters up to the first NUL; bytes outside printable ASCII become '?'.
std::string SanitizeName(const char* aName, size_t aMaxLength = 128);

// Keeps the aKeep most common entries (ties by name) and folds the rest into "other".
Tally TrimTally(const Tally& aTally, size_t aKeep = 64);
}
```

- [x] **Step 6: Write `src/physx/Checks.cpp`**

```cpp
#include "physx/Checks.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <utility>
#include <vector>

using namespace physx;

namespace cpmc::px
{
namespace
{
struct NamedType
{
	const char* name;
	int         type;
};

// Every concrete type name PhysX 3.4.2 returns from getConcreteTypeName(), with its PxConcreteType.
constexpr NamedType kTypes[] = {
	{ "PxHeightField", PxConcreteType::eHEIGHTFIELD },
	{ "PxConvexMesh", PxConcreteType::eCONVEX_MESH },
	{ "PxBVH33TriangleMesh", PxConcreteType::eTRIANGLE_MESH_BVH33 },
	{ "PxBVH34TriangleMesh", PxConcreteType::eTRIANGLE_MESH_BVH34 },
	{ "PxClothFabric", PxConcreteType::eCLOTH_FABRIC },
	{ "PxRigidDynamic", PxConcreteType::eRIGID_DYNAMIC },
	{ "PxRigidStatic", PxConcreteType::eRIGID_STATIC },
	{ "PxShape", PxConcreteType::eSHAPE },
	{ "PxMaterial", PxConcreteType::eMATERIAL },
	{ "PxConstraint", PxConcreteType::eCONSTRAINT },
	{ "PxCloth", PxConcreteType::eCLOTH },
	{ "PxParticleSystem", PxConcreteType::ePARTICLE_SYSTEM },
	{ "PxParticleFluid", PxConcreteType::ePARTICLE_FLUID },
	{ "PxAggregate", PxConcreteType::eAGGREGATE },
	{ "PxArticulation", PxConcreteType::eARTICULATION },
	{ "PxArticulationLink", PxConcreteType::eARTICULATION_LINK },
	{ "PxPruningStructure", PxConcreteType::ePRUNING_STRUCTURE },
};
}

int ConcreteTypeForName(const char* aName)
{
	if (!aName)
	{
		return -1;
	}
	for (const auto& entry : kTypes)
	{
		if (std::strcmp(entry.name, aName) == 0)
		{
			return entry.type;
		}
	}
	return -1;
}

bool TolerancesPlausible(float aLength, float aSpeed)
{
	return aLength >= 0.01f && aLength <= 100.0f && aSpeed >= 0.1f && aSpeed <= 1000.0f;
}

bool GravityFinite(const PxVec3& aGravity)
{
	return std::isfinite(aGravity.x) && std::isfinite(aGravity.y) && std::isfinite(aGravity.z) &&
	       aGravity.magnitude() <= 100.0f;
}

bool GravityEarthLike(const PxVec3& aGravity)
{
	const float magnitude = aGravity.magnitude();
	return magnitude >= 1.0f && magnitude <= 30.0f;
}

bool GeometryMatchesMesh(PxGeometryType::Enum aGeometry, int aMeshConcreteType)
{
	switch (aGeometry)
	{
	case PxGeometryType::eTRIANGLEMESH:
		return aMeshConcreteType == PxConcreteType::eTRIANGLE_MESH_BVH33 ||
		       aMeshConcreteType == PxConcreteType::eTRIANGLE_MESH_BVH34;
	case PxGeometryType::eCONVEXMESH:
		return aMeshConcreteType == PxConcreteType::eCONVEX_MESH;
	case PxGeometryType::eHEIGHTFIELD:
		return aMeshConcreteType == PxConcreteType::eHEIGHTFIELD;
	default:
		return false;
	}
}

const char* GeometryName(PxGeometryType::Enum aGeometry)
{
	switch (aGeometry)
	{
	case PxGeometryType::eSPHERE: return "sphere";
	case PxGeometryType::ePLANE: return "plane";
	case PxGeometryType::eCAPSULE: return "capsule";
	case PxGeometryType::eBOX: return "box";
	case PxGeometryType::eCONVEXMESH: return "convexMesh";
	case PxGeometryType::eTRIANGLEMESH: return "triangleMesh";
	case PxGeometryType::eHEIGHTFIELD: return "heightField";
	default: return "unknown";
	}
}

const char* GeometryShortName(PxGeometryType::Enum aGeometry)
{
	switch (aGeometry)
	{
	case PxGeometryType::eSPHERE: return "sphere";
	case PxGeometryType::ePLANE: return "plane";
	case PxGeometryType::eCAPSULE: return "capsule";
	case PxGeometryType::eBOX: return "box";
	case PxGeometryType::eCONVEXMESH: return "convex";
	case PxGeometryType::eTRIANGLEMESH: return "trimesh";
	case PxGeometryType::eHEIGHTFIELD: return "heightfield";
	default: return "unknown";
	}
}

std::string Category(const char* aActorKind, PxShapeFlags aShapeFlags, PxGeometryType::Enum aGeometry)
{
	const char* kind = aActorKind;
	if (aShapeFlags.isSet(PxShapeFlag::eTRIGGER_SHAPE))
	{
		kind = "trigger";
	}
	else if (aShapeFlags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE) && !aShapeFlags.isSet(PxShapeFlag::eSIMULATION_SHAPE))
	{
		kind = "queryonly";
	}
	return std::string(kind) + "_" + GeometryShortName(aGeometry);
}

std::string FilterKey(const PxFilterData& aData)
{
	return std::format("{:x}:{:x}:{:x}:{:x}", aData.word0, aData.word1, aData.word2, aData.word3);
}

std::string SanitizeName(const char* aName, size_t aMaxLength)
{
	std::string out;
	if (!aName)
	{
		return out;
	}
	for (size_t i = 0; i < aMaxLength && aName[i] != '\0'; ++i)
	{
		const auto c = static_cast<unsigned char>(aName[i]);
		out.push_back(c >= 0x20 && c <= 0x7E ? char(c) : '?');
	}
	return out;
}

Tally TrimTally(const Tally& aTally, size_t aKeep)
{
	std::vector<std::pair<std::string, uint64_t>> entries(aTally.begin(), aTally.end());
	std::stable_sort(entries.begin(), entries.end(), [](const auto& aA, const auto& aB) { return aA.second > aB.second; });
	Tally    kept;
	uint64_t other = 0;
	for (size_t i = 0; i < entries.size(); ++i)
	{
		if (i < aKeep)
		{
			kept[entries[i].first] = entries[i].second;
		}
		else
		{
			other += entries[i].second;
		}
	}
	if (other > 0)
	{
		kept["other"] += other;
	}
	return kept;
}
}
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | tail -1
```
Expected: `28 test(s), 0 failure(s)`

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/physx-probe \
  && git commit -q -m "physx-probe: report data and the pure compatibility checks" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 8: OBJ writer

**Files:**
- Create: `tests/TestObjWriter.cpp`, `src/ObjWriter.h`, `src/ObjWriter.cpp`
- Modify: `spikes/physx-probe/CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestObjWriter.cpp`**

```cpp
#include "Harness.h"

#include "ObjWriter.h"

#include <sstream>

using namespace physx;
using cpmc::ObjGroup;
using cpmc::px::Tri;

TEST(ObjIsInMinecraftAxesRelativeToTheOriginWithSharedVertices)
{
	// World origin (10, 20, 30). A quad in the world XY plane, as two triangles sharing an edge, and one more group.
	const cpmc::Vec3d origin{ 10.0, 20.0, 30.0 };
	ObjGroup          quad{ "static_trimesh", cpmc::CategoryColor("static_trimesh"), {} };
	quad.triangles.push_back({ { PxVec3(10, 20, 30), PxVec3(11, 20, 30), PxVec3(10, 21, 30) } });
	quad.triangles.push_back({ { PxVec3(11, 20, 30), PxVec3(11, 21, 30), PxVec3(10, 21, 30) } });
	ObjGroup box{ "dynamic_box", cpmc::CategoryColor("dynamic_box"), {} };
	box.triangles.push_back({ { PxVec3(10, 20, 31.5f), PxVec3(10.25f, 20, 31.5f), PxVec3(10, 20, 30) } });
	// A face that collapses at millimetre precision is dropped.
	box.triangles.push_back({ { PxVec3(10, 20, 30), PxVec3(10.0001f, 20, 30), PxVec3(10, 20, 31.5f) } });

	std::ostringstream out;
	cpmc::WriteObj(out, { quad, box }, origin, "collision.mtl", "test");
	CHECK_STR(out.str(),
	          "# PhysXProbe collision dump\n"
	          "# test\n"
	          "mtllib collision.mtl\n"
	          "o static_trimesh\n"
	          "usemtl static_trimesh\n"
	          "v 0.000 0.000 0.000\n"
	          "v 1.000 0.000 0.000\n"
	          "v 0.000 0.000 -1.000\n"
	          "v 1.000 0.000 -1.000\n"
	          "f 1 2 3\n"
	          "f 2 4 3\n"
	          "o dynamic_box\n"
	          "usemtl dynamic_box\n"
	          "v 0.000 1.500 0.000\n"
	          "v 0.250 1.500 0.000\n"
	          "v 0.000 0.000 0.000\n"
	          "f 5 6 7\n");
}

TEST(MtlHasOneMaterialPerGroup)
{
	const ObjGroup     group{ "static_box", { 0.55f, 0.6f, 0.7f }, {} };
	std::ostringstream out;
	cpmc::WriteMtl(out, { group });
	CHECK_STR(out.str(), "newmtl static_box\nKd 0.550 0.600 0.700\n");
}

TEST(CategoryColorsIgnoreTheScenePrefix)
{
	CHECK(cpmc::CategoryColor("s1_trigger_box") == cpmc::CategoryColor("trigger_box"));
	CHECK(cpmc::CategoryColor("dynamic_convex") != cpmc::CategoryColor("static_convex"));
}
```

- [x] **Step 2: Add the source and test to `CMakeLists.txt`**

Add `src/ObjWriter.cpp` as the last entry of the `probe_core` list, and `tests/TestObjWriter.cpp` as the last entry of
the `probe_tests` list.

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C" | head -3
```
Expected: `error C1083: Cannot open include file: 'ObjWriter.h'`, or a missing source error.

- [x] **Step 4: Write `src/ObjWriter.h`**

```cpp
#pragma once

#include "Coords.h"
#include "physx/ShapeTriangles.h"

#include <array>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace cpmc
{
// One OBJ object group: world-space triangles of one category.
struct ObjGroup
{
	std::string          name;
	std::array<float, 3> color{};
	std::vector<px::Tri> triangles;
};

// A fixed colour per category, by kind (static, kinematic, dynamic, trigger, queryonly) and geometry.
// A scene prefix such as "s1_" is ignored.
std::array<float, 3> CategoryColor(std::string_view aCategory);

// Writes the groups as OBJ text in Minecraft axes, relative to aOrigin (world), with millimetre precision.
// Identical vertices are shared within each group; faces that collapse to fewer than 3 vertices are dropped.
void WriteObj(std::ostream& aOut, const std::vector<ObjGroup>& aGroups, const Vec3d& aOrigin,
              std::string_view aMtlFileName, std::string_view aComment);

// One material per group, named after it, with the group's colour as Kd.
void WriteMtl(std::ostream& aOut, const std::vector<ObjGroup>& aGroups);
}
```

- [x] **Step 5: Write `src/ObjWriter.cpp`**

```cpp
#include "ObjWriter.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <unordered_map>

namespace cpmc
{
namespace
{
struct MmKey
{
	int64_t x, y, z;
	bool    operator==(const MmKey&) const = default;
};

struct MmKeyHash
{
	size_t operator()(const MmKey& aKey) const
	{
		return std::hash<int64_t>()(aKey.x) ^ (std::hash<int64_t>()(aKey.y) << 1) ^ (std::hash<int64_t>()(aKey.z) << 2);
	}
};

// Millimetres as "-12.345" without floating-point formatting (so no "-0.000").
std::string FormatMm(int64_t aMm)
{
	const bool    negative = aMm < 0;
	const int64_t magnitude = negative ? -aMm : aMm;
	return std::format("{}{}.{:03}", negative ? "-" : "", magnitude / 1000, magnitude % 1000);
}

std::string_view WithoutScenePrefix(std::string_view aCategory)
{
	// "s<digits>_" in front of a category name.
	if (aCategory.size() > 2 && aCategory[0] == 's')
	{
		size_t i = 1;
		while (i < aCategory.size() && aCategory[i] >= '0' && aCategory[i] <= '9')
		{
			++i;
		}
		if (i > 1 && i < aCategory.size() && aCategory[i] == '_')
		{
			return aCategory.substr(i + 1);
		}
	}
	return aCategory;
}
}

std::array<float, 3> CategoryColor(std::string_view aCategory)
{
	const std::string_view name = WithoutScenePrefix(aCategory);
	const auto             startsWith = [&](std::string_view aPrefix) { return name.substr(0, aPrefix.size()) == aPrefix; };
	if (startsWith("trigger_"))
	{
		return { 0.95f, 0.85f, 0.20f };
	}
	if (startsWith("queryonly_"))
	{
		return { 0.75f, 0.35f, 0.85f };
	}
	if (startsWith("kinematic_"))
	{
		return { 0.95f, 0.55f, 0.20f };
	}
	if (startsWith("dynamic_"))
	{
		return { 0.25f, 0.55f, 0.95f };
	}
	if (name == "static_heightfield")
	{
		return { 0.45f, 0.62f, 0.38f };
	}
	if (name == "static_convex")
	{
		return { 0.70f, 0.66f, 0.55f };
	}
	if (name == "static_box")
	{
		return { 0.55f, 0.60f, 0.70f };
	}
	if (name == "static_sphere" || name == "static_capsule")
	{
		return { 0.60f, 0.55f, 0.70f };
	}
	return { 0.62f, 0.62f, 0.62f };  // static_trimesh and anything else
}

void WriteObj(std::ostream& aOut, const std::vector<ObjGroup>& aGroups, const Vec3d& aOrigin,
              std::string_view aMtlFileName, std::string_view aComment)
{
	aOut << "# PhysXProbe collision dump\n";
	if (!aComment.empty())
	{
		aOut << "# " << aComment << '\n';
	}
	aOut << "mtllib " << aMtlFileName << '\n';
	uint64_t nextIndex = 1;  // OBJ indices are 1-based and count across the whole file
	for (const ObjGroup& group : aGroups)
	{
		aOut << "o " << group.name << '\n' << "usemtl " << group.name << '\n';
		std::unordered_map<MmKey, uint64_t, MmKeyHash> shared;
		std::string                                    faces;
		for (const px::Tri& tri : group.triangles)
		{
			uint64_t index[3];
			for (int k = 0; k < 3; ++k)
			{
				const Vec3d relative{ double(tri.v[k].x) - aOrigin.x, double(tri.v[k].y) - aOrigin.y,
				                      double(tri.v[k].z) - aOrigin.z };
				const Vec3d mc = McFromWorld(relative);
				const MmKey key{ std::llround(mc.x * 1000.0), std::llround(mc.y * 1000.0), std::llround(mc.z * 1000.0) };
				const auto  found = shared.find(key);
				if (found != shared.end())
				{
					index[k] = found->second;
					continue;
				}
				index[k] = nextIndex++;
				shared.emplace(key, index[k]);
				aOut << "v " << FormatMm(key.x) << ' ' << FormatMm(key.y) << ' ' << FormatMm(key.z) << '\n';
			}
			if (index[0] != index[1] && index[1] != index[2] && index[0] != index[2])
			{
				faces += std::format("f {} {} {}\n", index[0], index[1], index[2]);
			}
		}
		aOut << faces;
	}
}

void WriteMtl(std::ostream& aOut, const std::vector<ObjGroup>& aGroups)
{
	for (const ObjGroup& group : aGroups)
	{
		aOut << "newmtl " << group.name << '\n'
		     << std::format("Kd {:.3f} {:.3f} {:.3f}\n", group.color[0], group.color[1], group.color[2]);
	}
}
}
```

- [x] **Step 6: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | tail -1
```
Expected: `31 test(s), 0 failure(s)`

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/physx-probe \
  && git commit -q -m "physx-probe: OBJ and MTL writer" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 9: report.json

**Files:**
- Create: `tests/TestReportJson.cpp`, `src/ReportJson.h`, `src/ReportJson.cpp`
- Modify: `spikes/physx-probe/CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestReportJson.cpp`**

```cpp
#include "Harness.h"

#include "ReportJson.h"

namespace
{
bool Contains(const std::string& aText, const std::string& aPart)
{
	return aText.find(aPart) != std::string::npos;
}
}

TEST(ReportWithoutGeometryHasNulls)
{
	cpmc::DumpReport report;
	report.time = "2026-10-01T12:00:00";
	report.trigger = "request";
	report.runtime = "2.31.0";
	report.physxFileVersion = "3.4.2.0";
	report.steps.push_back({ 1, "module", false, "PhysX3_x64.dll is not loaded" });
	const std::string json = cpmc::ReportToJson(report);
	CHECK(Contains(json, "\"probeVersion\": \"0.1.0\""));
	CHECK(Contains(json, "\"trigger\": \"request\""));
	CHECK(Contains(json, "\"runtime\": \"2.31.0\""));
	CHECK(Contains(json, "\"player\": null"));
	CHECK(Contains(json, "\"pass\": false"));
	CHECK(Contains(json, "\"detail\": \"PhysX3_x64.dll is not loaded\""));
	CHECK(Contains(json, "\"region\": null"));
	CHECK(Contains(json, "\"coordinateCheck\": null"));
	CHECK(Contains(json, "\"files\": {}"));
}

TEST(ReportWithGeometryHasItsSections)
{
	cpmc::DumpReport report;
	cpmc::PlayerPose pose;
	pose.feet = { 1.0, 2.0, 3.0 };
	pose.forward = { 0.0f, 1.0f, 0.0f };
	pose.camera = cpmc::CameraPose{ { 1.0, 2.0, 4.7 }, { 0.0f, 1.0f, 0.0f }, 80.0f };
	report.player = pose;
	cpmc::SceneCensus scene;
	scene.gravity = { 0.0f, 0.0f, -9.81f };
	scene.flags = 0x1000;
	scene.requiresRwLock = true;
	scene.byConcreteType["PxRigidStatic"] = 7;
	scene.actorNameSamples = { "door \"A\"" };
	report.scenes.push_back(scene);
	cpmc::RegionResult region;
	region.triangles = 42;
	region.trianglesByCategory["static_trimesh"] = 42;
	report.region = region;
	cpmc::CoordinateCheck check;
	check.rayHit = true;
	check.hitDistance = 1.75;
	check.pass = true;
	check.scene = 0;
	report.coordinateCheck = check;
	report.objWritten = true;
	const std::string json = cpmc::ReportToJson(report);
	CHECK(Contains(json, "\"fov\": 80"));
	CHECK(Contains(json, "\"flags\": \"0x1000\""));
	CHECK(Contains(json, "\"requiresRwLock\": true"));
	CHECK(Contains(json, "\"PxRigidStatic\": 7"));
	CHECK(Contains(json, "\"door \\\"A\\\"\""));
	CHECK(Contains(json, "\"static_trimesh\": 42"));
	CHECK(Contains(json, "\"hitDistance\": 1.75"));
	CHECK(Contains(json, "\"nearestCapsule\": null"));
	CHECK(Contains(json, "\"obj\": \"collision.obj\""));
}
```

- [x] **Step 2: Add the source and test to `CMakeLists.txt`**

Add `src/ReportJson.cpp` as the last entry of the `probe_core` list, and `tests/TestReportJson.cpp` as the last entry
of the `probe_tests` list.

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C" | head -3
```
Expected: `error C1083: Cannot open include file: 'ReportJson.h'`, or a missing source error.

- [x] **Step 4: Write `src/ReportJson.h`**

```cpp
#pragma once

#include "Report.h"

#include <string>

namespace cpmc
{
// report.json for one dump, following the spec's report schema (pretty-printed).
std::string ReportToJson(const DumpReport& aReport);
}
```

- [x] **Step 5: Write `src/ReportJson.cpp`**

```cpp
#include "ReportJson.h"

#include "Json.h"

#include <format>

namespace cpmc
{
namespace
{
void Vec(JsonWriter& aJson, const Vec3d& aValue)
{
	aJson.BeginArray().Number(aValue.x).Number(aValue.y).Number(aValue.z).EndArray();
}

void Vec(JsonWriter& aJson, const Vec3f& aValue)
{
	aJson.BeginArray().Number(aValue.x).Number(aValue.y).Number(aValue.z).EndArray();
}

void Counts(JsonWriter& aJson, const Tally& aTally)
{
	aJson.BeginObject();
	for (const auto& [name, count] : aTally)
	{
		aJson.Key(name).UInt(count);
	}
	aJson.EndObject();
}

void Player(JsonWriter& aJson, const PlayerPose& aPose)
{
	aJson.BeginObject();
	aJson.Key("feet");
	Vec(aJson, aPose.feet);
	aJson.Key("forward");
	Vec(aJson, aPose.forward);
	aJson.Key("camera");
	if (aPose.camera)
	{
		aJson.BeginObject();
		aJson.Key("position");
		Vec(aJson, aPose.camera->position);
		aJson.Key("forward");
		Vec(aJson, aPose.camera->forward);
		aJson.Key("fov").Number(aPose.camera->fov);
		aJson.EndObject();
	}
	else
	{
		aJson.Null();
	}
	aJson.EndObject();
}

void Scene(JsonWriter& aJson, const SceneCensus& aScene)
{
	aJson.BeginObject();
	aJson.Key("index").Int(aScene.index);
	aJson.Key("gravity");
	Vec(aJson, aScene.gravity);
	aJson.Key("flags").String(std::format("0x{:x}", aScene.flags));
	aJson.Key("requiresRwLock").Bool(aScene.requiresRwLock);
	aJson.Key("staticActors").UInt(aScene.staticActors);
	aJson.Key("dynamicActors").UInt(aScene.dynamicActors);
	aJson.Key("aggregates").UInt(aScene.aggregates);
	aJson.Key("actorsInAggregates").UInt(aScene.actorsInAggregates);
	aJson.Key("articulations").UInt(aScene.articulations);
	aJson.Key("byConcreteType");
	Counts(aJson, aScene.byConcreteType);
	aJson.Key("byGeometryType");
	Counts(aJson, aScene.byGeometryType);
	aJson.Key("shapeFlags");
	Counts(aJson, aScene.shapeFlags);
	aJson.Key("queryFilter");
	Counts(aJson, aScene.queryFilter);
	aJson.Key("simulationFilter");
	Counts(aJson, aScene.simulationFilter);
	aJson.Key("namedActors").UInt(aScene.namedActors);
	aJson.Key("actorsWithUserData").UInt(aScene.actorsWithUserData);
	aJson.Key("actorNameSamples").BeginArray();
	for (const auto& name : aScene.actorNameSamples)
	{
		aJson.String(name);
	}
	aJson.EndArray();
	aJson.Key("mismatches").UInt(aScene.mismatches);
	aJson.Key("censusMs").Number(aScene.censusMs);
	aJson.EndObject();
}

void Region(JsonWriter& aJson, const RegionResult& aRegion)
{
	aJson.BeginObject();
	aJson.Key("center");
	Vec(aJson, aRegion.center);
	aJson.Key("halfExtents");
	Vec(aJson, aRegion.halfExtents);
	aJson.Key("boundsSelectedActors").UInt(aRegion.boundsSelectedActors);
	aJson.Key("overlapHits").UInt(aRegion.overlapHits);
	aJson.Key("overlapOverflow").Bool(aRegion.overlapOverflow);
	aJson.Key("selectMs").Number(aRegion.selectMs);
	aJson.Key("overlapMs").Number(aRegion.overlapMs);
	aJson.Key("extractMs").Number(aRegion.extractMs);
	aJson.Key("triangles").UInt(aRegion.triangles);
	aJson.Key("trianglesByCategory");
	Counts(aJson, aRegion.trianglesByCategory);
	aJson.Key("skipped").BeginObject().Key("plane").UInt(aRegion.skippedPlanes);
	aJson.Key("unknownGeometry").UInt(aRegion.unknownGeometry);
	aJson.Key("badTriangles").UInt(aRegion.badTriangles).EndObject();
	aJson.Key("triangleCapHit").Bool(aRegion.triangleCapHit);
	aJson.EndObject();
}

void Coordinates(JsonWriter& aJson, const CoordinateCheck& aCheck)
{
	aJson.BeginObject();
	aJson.Key("rayHit").Bool(aCheck.rayHit);
	aJson.Key("hitDistance");
	aCheck.rayHit ? aJson.Number(aCheck.hitDistance) : aJson.Null();
	aJson.Key("scene").Int(aCheck.scene);
	aJson.Key("pass").Bool(aCheck.pass);
	aJson.Key("nearestCapsule");
	if (aCheck.nearestCapsule)
	{
		aJson.BeginObject();
		aJson.Key("distance").Number(aCheck.nearestCapsule->distance);
		aJson.Key("radius").Number(aCheck.nearestCapsule->radius);
		aJson.Key("halfHeight").Number(aCheck.nearestCapsule->halfHeight);
		aJson.Key("kind").String(aCheck.nearestCapsule->kind);
		aJson.EndObject();
	}
	else
	{
		aJson.Null();
	}
	aJson.EndObject();
}
}

std::string ReportToJson(const DumpReport& aReport)
{
	JsonWriter json;
	json.BeginObject();
	json.Key("probeVersion").String(aReport.probeVersion);
	json.Key("time").String(aReport.time);
	json.Key("trigger").String(aReport.trigger);
	json.Key("game").BeginObject();
	json.Key("runtime").String(aReport.runtime);
	json.Key("physxFileVersion").String(aReport.physxFileVersion);
	json.EndObject();
	json.Key("player");
	aReport.player ? Player(json, *aReport.player) : void(json.Null());
	json.Key("compatibility").BeginObject();
	json.Key("pass").Bool(aReport.compatibilityPass);
	json.Key("steps").BeginArray();
	for (const CompatStep& step : aReport.steps)
	{
		json.BeginObject();
		json.Key("step").Int(step.step);
		json.Key("name").String(step.name);
		json.Key("pass").Bool(step.pass);
		json.Key("detail").String(step.detail);
		json.EndObject();
	}
	json.EndArray();
	json.Key("censusMismatches").UInt(aReport.censusMismatches);
	json.EndObject();
	json.Key("scenes").BeginArray();
	for (const SceneCensus& scene : aReport.scenes)
	{
		Scene(json, scene);
	}
	json.EndArray();
	json.Key("region");
	aReport.region ? Region(json, *aReport.region) : void(json.Null());
	json.Key("coordinateCheck");
	aReport.coordinateCheck ? Coordinates(json, *aReport.coordinateCheck) : void(json.Null());
	json.Key("mainThreadPauseMs").Number(aReport.mainThreadPauseMs);
	json.Key("faults").UInt(aReport.faults);
	json.Key("files").BeginObject();
	if (aReport.objWritten)
	{
		json.Key("obj").String("collision.obj");
		json.Key("mtl").String("collision.mtl");
	}
	json.EndObject();
	json.EndObject();
	return json.Str() + "\n";
}
}
```

- [x] **Step 6: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | tail -1
```
Expected: `33 test(s), 0 failure(s)`

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/physx-probe \
  && git commit -q -m "physx-probe: report.json" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 10: The background dump writer

**Files:**
- Create: `tests/TestDumpWriter.cpp`, `src/DumpWriter.h`, `src/DumpWriter.cpp`
- Modify: `spikes/physx-probe/CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestDumpWriter.cpp`**

```cpp
#include "Harness.h"

#include "DumpWriter.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

using namespace physx;

namespace
{
std::string ReadFile(const std::filesystem::path& aPath)
{
	std::ifstream      in(aPath, std::ios::binary);
	std::ostringstream text;
	text << in.rdbuf();
	return text.str();
}

cpmc::DumpJob MakeJob(const std::filesystem::path& aFolder, bool aWithGeometry)
{
	cpmc::DumpJob job;
	job.folder = aFolder;
	job.report.trigger = "request";
	job.origin = { 0.0, 0.0, 0.0 };
	if (aWithGeometry)
	{
		cpmc::ObjGroup group{ "static_box", cpmc::CategoryColor("static_box"), {} };
		group.triangles.push_back({ { PxVec3(0, 0, 0), PxVec3(1, 0, 0), PxVec3(0, 1, 0) } });
		job.groups.push_back(group);
		job.report.objWritten = true;
	}
	return job;
}
}

TEST(WriteDumpWritesReportAndObj)
{
	const auto folder = std::filesystem::temp_directory_path() / "cpmc-physx-probe-test" / "with-geometry";
	std::filesystem::remove_all(folder);
	const cpmc::WriteResult result = cpmc::WriteDump(MakeJob(folder, true));
	CHECK(result.ok);
	CHECK_EQ(result.triangles, uint64_t(1));
	CHECK(ReadFile(folder / "report.json").starts_with("{\n"));
	CHECK(ReadFile(folder / "collision.obj").find("o static_box\n") != std::string::npos);
	CHECK(ReadFile(folder / "collision.mtl").starts_with("newmtl static_box\n"));
	std::filesystem::remove_all(folder.parent_path());
}

TEST(WriteDumpWithoutGeometryWritesOnlyTheReport)
{
	const auto folder = std::filesystem::temp_directory_path() / "cpmc-physx-probe-test" / "no-geometry";
	std::filesystem::remove_all(folder);
	const cpmc::WriteResult result = cpmc::WriteDump(MakeJob(folder, false));
	CHECK(result.ok);
	CHECK(std::filesystem::exists(folder / "report.json"));
	CHECK(!std::filesystem::exists(folder / "collision.obj"));
	std::filesystem::remove_all(folder.parent_path());
}

TEST(DumpWriterRunsOneJobAtATime)
{
	const auto       folder = std::filesystem::temp_directory_path() / "cpmc-physx-probe-test" / "threaded";
	cpmc::DumpWriter writer;
	CHECK(writer.Start(MakeJob(folder, true)));
	writer.Join();
	CHECK(!writer.Busy());
	const auto result = writer.TakeResult();
	CHECK(result.has_value() && result->ok);
	CHECK(!writer.TakeResult().has_value());  // taken once
	std::filesystem::remove_all(folder.parent_path());
}
```

- [x] **Step 2: Add the source and test to `CMakeLists.txt`**

Add `src/DumpWriter.cpp` as the last entry of the `probe_core` list, and `tests/TestDumpWriter.cpp` as the last entry
of the `probe_tests` list.

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C" | head -3
```
Expected: `error C1083: Cannot open include file: 'DumpWriter.h'`, or a missing source error.

- [x] **Step 4: Write `src/DumpWriter.h`**

```cpp
#pragma once

#include "ObjWriter.h"
#include "Report.h"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace cpmc
{
// Everything one dump writes: the folder, the report, and the triangles (world space) by category.
struct DumpJob
{
	std::filesystem::path folder;
	DumpReport            report;
	std::vector<ObjGroup> groups;
	Vec3d                 origin;  // V's feet: OBJ coordinates are relative to this
};

struct WriteResult
{
	std::filesystem::path folder;
	bool                  ok = false;
	std::string           error;
	uint64_t              triangles = 0;
	double                writeMs = 0.0;
};

// Writes report.json, and collision.obj/.mtl when there are groups, into aJob.folder. Never throws.
WriteResult WriteDump(const DumpJob& aJob);

// Writes dumps on a background thread. That thread never calls PhysX, the game or RED4ext.
class DumpWriter
{
public:
	~DumpWriter();

	bool Busy() const { return busy_.load(); }
	// Starts writing aJob. Returns false, and does nothing, while the previous dump is still being written.
	bool Start(DumpJob aJob);
	// The result of the last finished write, once.
	std::optional<WriteResult> TakeResult();
	// Waits for a running write to finish.
	void Join();

private:
	std::thread                thread_;
	std::atomic<bool>          busy_{ false };
	std::mutex                 mutex_;
	std::optional<WriteResult> result_;
};
}
```

- [x] **Step 5: Write `src/DumpWriter.cpp`**

```cpp
#include "DumpWriter.h"

#include "ReportJson.h"

#include <chrono>
#include <fstream>
#include <format>
#include <stdexcept>
#include <utility>

namespace cpmc
{
WriteResult WriteDump(const DumpJob& aJob)
{
	WriteResult result;
	result.folder = aJob.folder;
	const auto start = std::chrono::steady_clock::now();
	try
	{
		std::filesystem::create_directories(aJob.folder);
		if (!aJob.groups.empty())
		{
			std::ofstream obj(aJob.folder / "collision.obj", std::ios::binary);
			const std::string comment = std::format("V's feet (world, metres, Z up): {:.3f} {:.3f} {:.3f}; axes: Minecraft (Y up)",
			                                        aJob.origin.x, aJob.origin.y, aJob.origin.z);
			WriteObj(obj, aJob.groups, aJob.origin, "collision.mtl", comment);
			obj.close();
			std::ofstream mtl(aJob.folder / "collision.mtl", std::ios::binary);
			WriteMtl(mtl, aJob.groups);
			mtl.close();
			if (obj.fail() || mtl.fail())
			{
				throw std::runtime_error("couldn't write collision.obj or collision.mtl");
			}
			for (const ObjGroup& group : aJob.groups)
			{
				result.triangles += group.triangles.size();
			}
		}
		std::ofstream report(aJob.folder / "report.json", std::ios::binary);
		report << ReportToJson(aJob.report);
		report.close();
		if (report.fail())
		{
			throw std::runtime_error("couldn't write report.json");
		}
		result.ok = true;
	}
	catch (const std::exception& e)
	{
		result.error = e.what();
	}
	result.writeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	return result;
}

DumpWriter::~DumpWriter()
{
	Join();
}

bool DumpWriter::Start(DumpJob aJob)
{
	if (busy_.load())
	{
		return false;
	}
	Join();  // the previous thread has finished; reclaim it
	busy_ = true;
	thread_ = std::thread([this, job = std::move(aJob)] {
		WriteResult result = WriteDump(job);
		{
			std::scoped_lock lock(mutex_);
			result_ = std::move(result);
		}
		busy_ = false;
	});
	return true;
}

std::optional<WriteResult> DumpWriter::TakeResult()
{
	std::scoped_lock lock(mutex_);
	return std::exchange(result_, std::nullopt);
}

void DumpWriter::Join()
{
	if (thread_.joinable())
	{
		thread_.join();
	}
}
}
```

- [x] **Step 6: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | tail -1
```
Expected: `36 test(s), 0 failure(s)`

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/physx-probe \
  && git commit -q -m "physx-probe: background dump writer" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 11: The PhysX session, live-shape triangles and the crash guard

`PhysxSession` is the only code that calls the game's PhysX. Only its crash guard can be tested without the game:
the guard must survive an access violation (what a mismatched virtual table would cause) and a thrown exception.
Everything else here is exercised by Task 15.

**Files:**
- Create: `tests/TestGuard.cpp`, `src/physx/PhysxSession.h`, `src/physx/PhysxSession.cpp`
- Modify: `src/physx/ShapeTriangles.h`, `src/physx/ShapeTriangles.cpp`, `spikes/physx-probe/CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestGuard.cpp`**

```cpp
#include "Harness.h"

#include "physx/PhysxSession.h"

#include <stdexcept>

TEST(GuardedRunsTheFunction)
{
	int value = 0;
	CHECK(cpmc::px::Guarded(value, [](int& aValue) { aValue = 42; }));
	CHECK_EQ(value, 42);
}

TEST(GuardedSurvivesAnAccessViolation)
{
	// The kind of fault a mismatched PhysX virtual table would cause: a write through a bad pointer.
	int* nowhere = nullptr;
	CHECK(!cpmc::px::Guarded(nowhere, [](int*& aPointer) { *static_cast<volatile int*>(aPointer) = 1; }));
}

TEST(GuardedSurvivesAThrownException)
{
	int value = 0;
	CHECK(!cpmc::px::Guarded(value, [](int&) { throw std::runtime_error("inside the guard"); }));
}
```

- [x] **Step 2: Add the source and test to `CMakeLists.txt`**

Add `src/physx/PhysxSession.cpp` as the last entry of the `probe_core` list, and `tests/TestGuard.cpp` as the last
entry of the `probe_tests` list.

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C" | head -3
```
Expected: `error C1083: Cannot open include file: 'physx/PhysxSession.h'`, or a missing source error.

- [x] **Step 4: Declare `AppendTriangles` in `src/physx/ShapeTriangles.h`**

Add these just before the namespace's closing `}`:
```cpp

struct AppendStats
{
	uint32_t planes = 0;
	uint32_t unknownGeometry = 0;
	uint32_t badTriangles = 0;
};

// World-space triangles of a live shape that touch aRegion. This calls into the game's PhysX, so run it under
// a crash guard (see RunGuarded in PhysxSession.h).
void AppendTriangles(const physx::PxShape& aShape, const physx::PxTransform& aActorPose, const physx::PxBounds3& aRegion,
                     std::vector<Tri>& aOut, AppendStats& aStats);
```

- [x] **Step 5: Implement `AppendTriangles` in `src/physx/ShapeTriangles.cpp`**

Add this just before the file's last closing `}`:
```cpp

void AppendTriangles(const PxShape& aShape, const PxTransform& aActorPose, const PxBounds3& aRegion,
                     std::vector<Tri>& aOut, AppendStats& aStats)
{
	const PxTransform pose = aActorPose * aShape.getLocalPose();
	std::vector<Tri>  small;
	switch (aShape.getGeometryType())
	{
	case PxGeometryType::eBOX:
	{
		PxBoxGeometry geometry;
		if (aShape.getBoxGeometry(geometry))
		{
			BoxTriangles(geometry.halfExtents, pose, small);
		}
		break;
	}
	case PxGeometryType::eSPHERE:
	{
		PxSphereGeometry geometry;
		if (aShape.getSphereGeometry(geometry))
		{
			SphereTriangles(geometry.radius, pose, small);
		}
		break;
	}
	case PxGeometryType::eCAPSULE:
	{
		PxCapsuleGeometry geometry;
		if (aShape.getCapsuleGeometry(geometry))
		{
			CapsuleTriangles(geometry.radius, geometry.halfHeight, pose, small);
		}
		break;
	}
	case PxGeometryType::ePLANE:
		++aStats.planes;
		break;
	case PxGeometryType::eCONVEXMESH:
	{
		PxConvexMeshGeometry geometry;
		if (aShape.getConvexMeshGeometry(geometry) && geometry.convexMesh)
		{
			const PxConvexMesh&        mesh = *geometry.convexMesh;
			std::vector<PxHullPolygon> polygons(mesh.getNbPolygons());
			for (PxU32 i = 0; i < PxU32(polygons.size()); ++i)
			{
				mesh.getPolygonData(i, polygons[i]);
			}
			ConvexPolygonTriangles(mesh.getVertices(), mesh.getNbVertices(), mesh.getIndexBuffer(), polygons.data(),
			                       PxU32(polygons.size()), geometry.scale, pose, small);
		}
		break;
	}
	case PxGeometryType::eTRIANGLEMESH:
	{
		PxTriangleMeshGeometry geometry;
		if (aShape.getTriangleMeshGeometry(geometry) && geometry.triangleMesh)
		{
			const PxTriangleMesh& mesh = *geometry.triangleMesh;
			MeshTriangles(mesh.getVertices(), mesh.getNbVertices(), mesh.getTriangles(),
			              mesh.getTriangleMeshFlags().isSet(PxTriangleMeshFlag::e16_BIT_INDICES), mesh.getNbTriangles(),
			              geometry.scale, pose, aRegion, aOut, aStats.badTriangles);
		}
		break;
	}
	case PxGeometryType::eHEIGHTFIELD:
	{
		PxHeightFieldGeometry geometry;
		if (aShape.getHeightFieldGeometry(geometry) && geometry.heightField)
		{
			const PxHeightField& field = *geometry.heightField;
			HeightfieldView      view;
			view.rows = field.getNbRows();
			view.columns = field.getNbColumns();
			view.heightScale = geometry.heightScale;
			view.rowScale = geometry.rowScale;
			view.columnScale = geometry.columnScale;
			view.sample = [&field](uint32_t aRow, uint32_t aColumn) { return field.getSample(aRow, aColumn); };
			const CellRange cells = HeightfieldCellsUnder(view, pose, aRegion);
			if (!cells.Empty())
			{
				HeightfieldTriangles(view, cells, pose, aRegion, aOut);
			}
		}
		break;
	}
	default:
		++aStats.unknownGeometry;
		break;
	}
	for (const Tri& tri : small)
	{
		if (KeepIfTouches(tri, aRegion))
		{
			aOut.push_back(tri);
		}
	}
}
```

- [x] **Step 6: Write `src/physx/PhysxSession.h`**

```cpp
#pragma once

#include "Report.h"
#include "physx/PxHeaders.h"
#include "physx/ShapeTriangles.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// The only code that calls the game's PhysX. Every call runs under a crash guard, so a bad read ends that
// operation (and counts a fault) instead of crashing the game.
namespace cpmc::px
{
using GuardedFn = void (*)(void* aContext);

// Runs aFn(aContext) under a structured-exception guard. False if it faulted (or threw).
bool RunGuarded(GuardedFn aFn, void* aContext) noexcept;

// Runs a captureless lambda taking a Context& under the guard.
template<class Context, class Fn>
bool Guarded(Context& aContext, Fn aFn)
{
	static_assert(std::is_convertible_v<Fn, void (*)(Context&)>, "use a captureless lambda");
	struct Thunk
	{
		static void Call(void* aRaw)
		{
			auto& pack = *static_cast<std::pair<Context*, void (*)(Context&)>*>(aRaw);
			pack.second(*pack.first);
		}
	};
	std::pair<Context*, void (*)(Context&)> pack{ &aContext, static_cast<void (*)(Context&)>(aFn) };
	return RunGuarded(&Thunk::Call, &pack);
}

struct OpenResult
{
	bool                    pass = false;
	std::vector<CompatStep> steps;
	std::string             physxFileVersion;
};

// Census and region results for one scene.
struct SceneDump
{
	SceneCensus                             census;
	uint32_t                                boundsSelected = 0;
	uint32_t                                overlapHits = 0;
	bool                                    overlapCapped = false;
	double                                  selectMs = 0.0;
	double                                  overlapMs = 0.0;
	double                                  extractMs = 0.0;
	std::map<std::string, std::vector<Tri>> triangles;  // by category
	AppendStats                             stats;
	std::optional<NearestCapsule>           nearestCapsule;
	bool                                    triangleCapHit = false;
};

class PhysxSession
{
public:
	// The first call finds PhysX and runs the compatibility check; later calls return the cached result.
	const OpenResult& Open();

	// The current scenes, fetched fresh. Empty unless Open() passed.
	std::vector<physx::PxScene*> FetchScenes(uint32_t& aFaults) const;

	// Census, then region selection, overlap query and triangle extraction, all under the scene's read lock.
	// aFeet is V's feet (world), used for the nearest capsule. At most aTriangleBudget triangles are kept.
	SceneDump DumpScene(physx::PxScene* aScene, int aIndex, const physx::PxBounds3& aRegion, const physx::PxVec3& aFeet,
	                    uint64_t aTriangleBudget, uint32_t& aFaults) const;

	// A ray straight down against static geometry. The distance to the nearest hit, if any.
	std::optional<float> RaycastDown(physx::PxScene* aScene, const physx::PxVec3& aFrom, float aMaxDistance,
	                                 uint32_t& aFaults) const;

private:
	physx::PxPhysics*         physics_ = nullptr;
	std::optional<OpenResult> open_;
};
}
```

- [x] **Step 7: Write `src/physx/PhysxSession.cpp`**

```cpp
#include "physx/PhysxSession.h"

#include "physx/Checks.h"

#include <Windows.h>

#include <algorithm>
#include <format>
#include <set>
#include <string_view>

using namespace physx;

namespace cpmc::px
{
bool RunGuarded(GuardedFn aFn, void* aContext) noexcept
{
	__try
	{
		aFn(aContext);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

namespace
{
constexpr uint32_t kMaxScenes = 16;
constexpr uint32_t kMaxActorsPerScene = 10'000'000;
constexpr size_t   kSampleActors = 64;
constexpr uint32_t kMaxSampledActors = 10'000;
constexpr size_t   kMaxNameSamples = 32;
constexpr size_t   kOverlapBuffer = 8192;
constexpr size_t   kOverlapCap = 1'000'000;
constexpr uint32_t kMaxFaultsPerScene = 100;
constexpr float    kCapsuleSearch = 1.5f;

const PxActorTypeFlags kRigidActors = PxActorTypeFlag::eRIGID_STATIC | PxActorTypeFlag::eRIGID_DYNAMIC;

double NowMs()
{
	LARGE_INTEGER counter;
	LARGE_INTEGER frequency;
	QueryPerformanceCounter(&counter);
	QueryPerformanceFrequency(&frequency);
	return double(counter.QuadPart) * 1000.0 / double(frequency.QuadPart);
}

std::string FileVersionOf(HMODULE aModule)
{
	wchar_t path[MAX_PATH]{};
	if (!GetModuleFileNameW(aModule, path, MAX_PATH))
	{
		return {};
	}
	DWORD       handle = 0;
	const DWORD size = GetFileVersionInfoSizeW(path, &handle);
	if (size == 0)
	{
		return {};
	}
	std::vector<uint8_t> data(size);
	VS_FIXEDFILEINFO*    info = nullptr;
	UINT                 length = 0;
	if (!GetFileVersionInfoW(path, 0, size, data.data()) ||
	    !VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &length) || !info)
	{
		return {};
	}
	return std::format("{}.{}.{}.{}", HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
	                   HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
}

bool IsReadable(const void* aAddress)
{
	MEMORY_BASIC_INFORMATION info{};
	if (!VirtualQuery(aAddress, &info, sizeof(info)) || info.State != MEM_COMMIT)
	{
		return false;
	}
	constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
	                            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
	return (info.Protect & kReadable) != 0 && (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0;
}

bool LockRead(PxScene* aScene)
{
	return Guarded(*aScene, [](PxScene& aSceneRef) { aSceneRef.lockRead(); });
}

void UnlockRead(PxScene* aScene)
{
	Guarded(*aScene, [](PxScene& aSceneRef) { aSceneRef.unlockRead(); });
}

// The object's concrete type name, read from the game's PhysX.
std::string TypeName(const PxBase& aObject)
{
	return SanitizeName(aObject.getConcreteTypeName(), 64);
}

// The name agrees with the object's type ID, which is a plain data member (not a virtual call).
bool TypeAgrees(const PxBase& aObject, const std::string& aName)
{
	const int expected = ConcreteTypeForName(aName.c_str());
	return expected >= 0 && expected == int(aObject.getConcreteType());
}

// The mesh object a mesh-based shape references; nullptr for primitives.
const PxBase* MeshOf(const PxShape& aShape)
{
	switch (aShape.getGeometryType())
	{
	case PxGeometryType::eTRIANGLEMESH:
	{
		PxTriangleMeshGeometry geometry;
		return aShape.getTriangleMeshGeometry(geometry) ? geometry.triangleMesh : nullptr;
	}
	case PxGeometryType::eCONVEXMESH:
	{
		PxConvexMeshGeometry geometry;
		return aShape.getConvexMeshGeometry(geometry) ? geometry.convexMesh : nullptr;
	}
	case PxGeometryType::eHEIGHTFIELD:
	{
		PxHeightFieldGeometry geometry;
		return aShape.getHeightFieldGeometry(geometry) ? geometry.heightField : nullptr;
	}
	default:
		return nullptr;
	}
}

const char* ActorKind(const PxActor& aActor)
{
	switch (aActor.getType())
	{
	case PxActorType::eRIGID_STATIC:
		return "static";
	case PxActorType::eRIGID_DYNAMIC:
		return static_cast<const PxRigidBody&>(aActor).getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC)
		           ? "kinematic"
		           : "dynamic";
	default:
		return "other";
	}
}

std::vector<PxShape*> ShapesOf(const PxRigidActor& aActor)
{
	std::vector<PxShape*> shapes(aActor.getNbShapes());
	shapes.resize(aActor.getShapes(shapes.data(), PxU32(shapes.size())));
	return shapes;
}

// What checking objects found: type-name mismatches (step 6) and geometry/mesh disagreements (step 7).
struct Findings
{
	uint64_t    objectsChecked = 0;
	uint64_t    typeMismatches = 0;
	std::string firstTypeMismatch;
	uint64_t    geometryMismatches = 0;
	std::string firstGeometryMismatch;
	bool        seenTriangleMesh = false;
	bool        seenHeightField = false;
	bool        seenConvex = false;
};

void CheckType(const PxBase& aObject, const char* aWhat, Findings& aFindings, std::string& aNameOut)
{
	aNameOut = TypeName(aObject);
	++aFindings.objectsChecked;
	if (!TypeAgrees(aObject, aNameOut))
	{
		++aFindings.typeMismatches;
		if (aFindings.firstTypeMismatch.empty())
		{
			aFindings.firstTypeMismatch =
			    std::format("{} reports type name \"{}\" with type ID {}", aWhat, aNameOut, int(aObject.getConcreteType()));
		}
	}
}

// Checks one actor, its shapes and their meshes. With aCensus set, it also fills the census tallies.
void VisitActor(const PxActor& aActor, Findings& aFindings, SceneCensus* aCensus, std::set<std::string>* aNames)
{
	std::string actorName;
	CheckType(aActor, "an actor", aFindings, actorName);
	if (aCensus)
	{
		++aCensus->byConcreteType[actorName];
		if (const char* name = aActor.getName())
		{
			++aCensus->namedActors;
			if (aNames && aNames->size() < kMaxNameSamples)
			{
				aNames->insert(SanitizeName(name));
			}
		}
		if (aActor.userData)
		{
			++aCensus->actorsWithUserData;
		}
	}
	const PxActorType::Enum type = aActor.getType();
	if (type != PxActorType::eRIGID_STATIC && type != PxActorType::eRIGID_DYNAMIC)
	{
		return;
	}
	for (const PxShape* shape : ShapesOf(static_cast<const PxRigidActor&>(aActor)))
	{
		if (!shape)
		{
			continue;
		}
		std::string shapeName;
		CheckType(*shape, "a shape", aFindings, shapeName);
		const PxGeometryType::Enum geometry = shape->getGeometryType();
		if (aCensus)
		{
			++aCensus->byGeometryType[GeometryName(geometry)];
			const PxShapeFlags flags = shape->getFlags();
			if (flags.isSet(PxShapeFlag::eSIMULATION_SHAPE))
			{
				++aCensus->shapeFlags["simulation"];
			}
			if (flags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE))
			{
				++aCensus->shapeFlags["sceneQuery"];
			}
			if (flags.isSet(PxShapeFlag::eTRIGGER_SHAPE))
			{
				++aCensus->shapeFlags["trigger"];
			}
			++aCensus->queryFilter[FilterKey(shape->getQueryFilterData())];
			++aCensus->simulationFilter[FilterKey(shape->getSimulationFilterData())];
		}
		const PxBase* mesh = MeshOf(*shape);
		if (!mesh)
		{
			continue;
		}
		std::string meshName;
		CheckType(*mesh, "a mesh", aFindings, meshName);
		if (!GeometryMatchesMesh(geometry, ConcreteTypeForName(meshName.c_str())))
		{
			++aFindings.geometryMismatches;
			if (aFindings.firstGeometryMismatch.empty())
			{
				aFindings.firstGeometryMismatch =
				    std::format("a {} shape references a \"{}\"", GeometryName(geometry), meshName);
			}
		}
		aFindings.seenTriangleMesh |= geometry == PxGeometryType::eTRIANGLEMESH;
		aFindings.seenHeightField |= geometry == PxGeometryType::eHEIGHTFIELD;
		aFindings.seenConvex |= geometry == PxGeometryType::eCONVEXMESH;
	}
}

// Collects every overlap hit: PhysX hands over each full buffer, and the last partial one, via processTouches.
struct CollectOverlaps final : PxHitCallback<PxOverlapHit>
{
	CollectOverlaps(PxOverlapHit* aBuffer, PxU32 aSize, std::vector<PxOverlapHit>& aOut)
		: PxHitCallback<PxOverlapHit>(aBuffer, aSize)
		, out(aOut)
	{
	}

	PxAgain processTouches(const PxOverlapHit* aHits, PxU32 aCount) override
	{
		out.insert(out.end(), aHits, aHits + aCount);
		if (out.size() >= kOverlapCap)
		{
			capped = true;
			return false;
		}
		return true;
	}

	std::vector<PxOverlapHit>& out;
	bool                       capped = false;
};

// Keeps the capsule nearest to V's feet (distance to its surface), if within kCapsuleSearch.
void NoteCapsule(const PxShape& aShape, const PxTransform& aActorPose, const PxVec3& aFeet, const char* aKind,
                 std::optional<NearestCapsule>& aNearest)
{
	PxCapsuleGeometry geometry;
	if (!aShape.getCapsuleGeometry(geometry))
	{
		return;
	}
	const PxTransform pose = aActorPose * aShape.getLocalPose();
	const PxVec3      axis = pose.q.rotate(PxVec3(geometry.halfHeight, 0.0f, 0.0f));  // capsules lie along local X
	const PxVec3      a = pose.p - axis;
	const PxVec3      ab = axis * 2.0f;
	const float       lengthSquared = ab.magnitudeSquared();
	const float       t = lengthSquared > 0.0f ? std::clamp((aFeet - a).dot(ab) / lengthSquared, 0.0f, 1.0f) : 0.0f;
	const float       distance = std::max(0.0f, (a + ab * t - aFeet).magnitude() - geometry.radius);
	if (distance <= kCapsuleSearch && (!aNearest || distance < aNearest->distance))
	{
		aNearest = NearestCapsule{ distance, geometry.radius, geometry.halfHeight, aKind };
	}
}

uint64_t CountTriangles(const std::map<std::string, std::vector<Tri>>& aTriangles)
{
	uint64_t count = 0;
	for (const auto& [category, triangles] : aTriangles)
	{
		count += triangles.size();
	}
	return count;
}

// Drops triangles beyond aBudget, starting from the last category.
void TrimToBudget(std::map<std::string, std::vector<Tri>>& aTriangles, uint64_t aBudget)
{
	uint64_t excess = CountTriangles(aTriangles);
	excess = excess > aBudget ? excess - aBudget : 0;
	for (auto it = aTriangles.rbegin(); it != aTriangles.rend() && excess > 0; ++it)
	{
		const uint64_t drop = std::min<uint64_t>(excess, it->second.size());
		it->second.resize(it->second.size() - drop);
		excess -= drop;
	}
}
}

const OpenResult& PhysxSession::Open()
{
	if (open_)
	{
		return *open_;
	}
	OpenResult result;
	const auto step = [&](int aNumber, const char* aName, bool aPass, std::string aDetail) {
		result.steps.push_back({ aNumber, aName, aPass, std::move(aDetail) });
		return aPass;
	};
	const auto finish = [&]() -> const OpenResult& {
		result.pass = result.steps.size() == 7 &&
		              std::all_of(result.steps.begin(), result.steps.end(), [](const CompatStep& aStep) { return aStep.pass; });
		open_ = std::move(result);
		return *open_;
	};

	// 1. The module and its version.
	const HMODULE module = GetModuleHandleW(L"PhysX3_x64.dll");
	result.physxFileVersion = module ? FileVersionOf(module) : std::string();
	if (!step(1, "module", module && result.physxFileVersion.starts_with("3.4.2."),
	          module ? "PhysX3_x64.dll " + result.physxFileVersion : std::string("PhysX3_x64.dll is not loaded")))
	{
		return finish();
	}

	// 2. PxGetPhysics.
	using GetPhysicsFn = PxPhysics& (*)();
	struct GetPhysicsCall
	{
		GetPhysicsFn fn;
		PxPhysics*   physics;
	};
	GetPhysicsCall getPhysics{ reinterpret_cast<GetPhysicsFn>(GetProcAddress(module, "PxGetPhysics")), nullptr };
	const bool     called = getPhysics.fn && Guarded(getPhysics, [](GetPhysicsCall& aCall) { aCall.physics = &aCall.fn(); });
	PxPhysics*     physics = called ? getPhysics.physics : nullptr;
	const bool     readable = physics && IsReadable(physics) && IsReadable(*reinterpret_cast<void* const*>(physics));
	if (!step(2, "PxGetPhysics", readable,
	          !getPhysics.fn ? std::string("PxGetPhysics isn't exported")
	          : readable     ? std::format("PxPhysics at {}", static_cast<const void*>(physics))
	                         : std::string("PxGetPhysics faulted or returned unreadable memory")))
	{
		return finish();
	}

	// 3. Tolerances.
	struct TolerancesCall
	{
		PxPhysics* physics;
		float      length;
		float      speed;
	};
	TolerancesCall tolerances{ physics, 0.0f, 0.0f };
	const bool     gotTolerances = Guarded(tolerances, [](TolerancesCall& aCall) {
        const PxTolerancesScale& scale = aCall.physics->getTolerancesScale();
        aCall.length = scale.length;
        aCall.speed = scale.speed;
    });
	if (!step(3, "tolerances", gotTolerances && TolerancesPlausible(tolerances.length, tolerances.speed),
	          gotTolerances ? std::format("length {}, speed {}", tolerances.length, tolerances.speed)
	                        : std::string("getTolerancesScale faulted")))
	{
		return finish();
	}

	// 4. Scenes.
	struct ScenesCall
	{
		PxPhysics*            physics;
		uint32_t              count;
		std::vector<PxScene*> scenes;
	};
	ScenesCall scenes{ physics, 0, {} };
	const bool gotScenes = Guarded(scenes, [](ScenesCall& aCall) {
		aCall.count = aCall.physics->getNbScenes();
		if (aCall.count >= 1 && aCall.count <= kMaxScenes)
		{
			aCall.scenes.resize(aCall.count);
			aCall.scenes.resize(aCall.physics->getScenes(aCall.scenes.data(), aCall.count));
		}
	});
	const bool scenesOk = gotScenes && scenes.count >= 1 && scenes.count <= kMaxScenes &&
	                      scenes.scenes.size() == scenes.count &&
	                      std::none_of(scenes.scenes.begin(), scenes.scenes.end(), [](PxScene* aScene) { return !aScene; });
	if (!step(4, "scenes", scenesOk,
	          gotScenes ? std::format("{} scene(s)", scenes.count) : std::string("getNbScenes/getScenes faulted")))
	{
		return finish();
	}

	// 5. Gravity and actor counts.
	struct SceneFacts
	{
		PxScene* scene;
		PxVec3   gravity;
		uint32_t actors;
	};
	std::vector<SceneFacts> facts;
	bool                    factsOk = true;
	for (PxScene* scene : scenes.scenes)
	{
		SceneFacts sceneFacts{ scene, PxVec3(0.0f), 0 };
		factsOk &= Guarded(sceneFacts, [](SceneFacts& aFacts) {
			aFacts.gravity = aFacts.scene->getGravity();
			aFacts.actors = aFacts.scene->getNbActors(kRigidActors);
		});
		facts.push_back(sceneFacts);
	}
	const bool allSane = factsOk && std::all_of(facts.begin(), facts.end(), [](const SceneFacts& aFacts) {
		return GravityFinite(aFacts.gravity) && aFacts.actors <= kMaxActorsPerScene;
	});
	const bool anyEarthLike =
	    std::any_of(facts.begin(), facts.end(), [](const SceneFacts& aFacts) { return GravityEarthLike(aFacts.gravity); });
	std::string factsDetail;
	for (const SceneFacts& sceneFacts : facts)
	{
		factsDetail += std::format("{}gravity ({:.2f}, {:.2f}, {:.2f}), {} actors", factsDetail.empty() ? "" : "; ",
		                           sceneFacts.gravity.x, sceneFacts.gravity.y, sceneFacts.gravity.z, sceneFacts.actors);
	}
	if (!step(5, "gravity and counts", allSane && anyEarthLike,
	          factsOk ? factsDetail : std::string("getGravity/getNbActors faulted")))
	{
		return finish();
	}

	// 6 and 7. Type names and IDs of sampled actors, shapes and meshes; geometry types against mesh types.
	Findings findings;
	bool     sampleFaulted = false;
	for (PxScene* scene : scenes.scenes)
	{
		if (!LockRead(scene))
		{
			sampleFaulted = true;
			continue;
		}
		struct SampleCall
		{
			PxScene*  scene;
			Findings* findings;
		};
		SampleCall sample{ scene, &findings };
		sampleFaulted |= !Guarded(sample, [](SampleCall& aCall) {
			std::vector<PxActor*> actors(std::min<PxU32>(aCall.scene->getNbActors(kRigidActors), kMaxSampledActors));
			actors.resize(aCall.scene->getActors(kRigidActors, actors.data(), PxU32(actors.size())));
			for (size_t i = 0; i < actors.size() && aCall.findings->typeMismatches == 0; ++i)
			{
				const Findings& seen = *aCall.findings;
				if (i >= kSampleActors && seen.seenTriangleMesh && seen.seenHeightField && seen.seenConvex)
				{
					break;
				}
				if (actors[i])
				{
					VisitActor(*actors[i], *aCall.findings, nullptr, nullptr);
				}
			}
		});
		UnlockRead(scene);
	}
	const std::string seenDetail =
	    std::format("checked {} objects; saw a triangle mesh: {}, a heightfield: {}, a convex mesh: {}", findings.objectsChecked,
	                findings.seenTriangleMesh, findings.seenHeightField, findings.seenConvex);
	if (!step(6, "type names", !sampleFaulted && findings.typeMismatches == 0 && findings.objectsChecked > 0,
	          sampleFaulted            ? "faulted while sampling; " + seenDetail
	          : findings.typeMismatches ? findings.firstTypeMismatch
	                                    : seenDetail))
	{
		return finish();
	}
	step(7, "geometry and mesh types", findings.geometryMismatches == 0,
	     findings.geometryMismatches ? findings.firstGeometryMismatch : std::string("every checked mesh matches its shape"));
	if (result.steps.back().pass)
	{
		physics_ = physics;
	}
	return finish();
}

std::vector<PxScene*> PhysxSession::FetchScenes(uint32_t& aFaults) const
{
	if (!physics_)
	{
		return {};
	}
	struct Call
	{
		PxPhysics*            physics;
		std::vector<PxScene*> scenes;
	};
	Call call{ physics_, {} };
	if (!Guarded(call, [](Call& aCall) {
		    aCall.scenes.resize(std::min<PxU32>(aCall.physics->getNbScenes(), kMaxScenes));
		    aCall.scenes.resize(aCall.physics->getScenes(aCall.scenes.data(), PxU32(aCall.scenes.size())));
	    }))
	{
		++aFaults;
		return {};
	}
	std::erase(call.scenes, nullptr);
	return call.scenes;
}

SceneDump PhysxSession::DumpScene(PxScene* aScene, int aIndex, const PxBounds3& aRegion, const PxVec3& aFeet,
                                  uint64_t aTriangleBudget, uint32_t& aFaults) const
{
	SceneDump dump;
	dump.census.index = aIndex;
	if (!LockRead(aScene))
	{
		++aFaults;
		return dump;
	}

	// Scene facts and the actor list.
	double t0 = NowMs();
	struct ListCall
	{
		PxScene*              scene;
		SceneCensus*          census;
		std::vector<PxActor*> actors;
	};
	ListCall list{ aScene, &dump.census, {} };
	if (!Guarded(list, [](ListCall& aCall) {
		    SceneCensus& census = *aCall.census;
		    const PxVec3 gravity = aCall.scene->getGravity();
		    census.gravity = { gravity.x, gravity.y, gravity.z };
		    const PxSceneFlags flags = aCall.scene->getFlags();
		    census.flags = static_cast<uint32_t>(flags);
		    census.requiresRwLock = flags.isSet(PxSceneFlag::eREQUIRE_RW_LOCK);
		    census.staticActors = aCall.scene->getNbActors(PxActorTypeFlag::eRIGID_STATIC);
		    census.dynamicActors = aCall.scene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC);
		    aCall.actors.resize(std::min<PxU32>(aCall.scene->getNbActors(kRigidActors), kMaxActorsPerScene));
		    aCall.actors.resize(aCall.scene->getActors(kRigidActors, aCall.actors.data(), PxU32(aCall.actors.size())));
		    std::vector<PxAggregate*> aggregates(aCall.scene->getNbAggregates());
		    aggregates.resize(aCall.scene->getAggregates(aggregates.data(), PxU32(aggregates.size())));
		    census.aggregates = uint32_t(aggregates.size());
		    for (const PxAggregate* aggregate : aggregates)
		    {
			    census.actorsInAggregates += aggregate ? aggregate->getNbActors() : 0;
		    }
		    census.articulations = aCall.scene->getNbArticulations();
	    }))
	{
		++aFaults;
	}

	// The census: every actor, shape and mesh, re-checking each one's type.
	Findings              findings;
	std::set<std::string> names;
	uint32_t              censusFaults = 0;
	for (PxActor* actor : list.actors)
	{
		struct VisitCall
		{
			PxActor*               actor;
			Findings*              findings;
			SceneCensus*           census;
			std::set<std::string>* names;
		};
		VisitCall visit{ actor, &findings, &dump.census, &names };
		if (actor && !Guarded(visit, [](VisitCall& aCall) { VisitActor(*aCall.actor, *aCall.findings, aCall.census, aCall.names); }))
		{
			++aFaults;
			if (++censusFaults >= kMaxFaultsPerScene)
			{
				break;
			}
		}
	}
	dump.census.mismatches = findings.typeMismatches + findings.geometryMismatches;
	dump.census.actorNameSamples.assign(names.begin(), names.end());
	dump.census.censusMs = NowMs() - t0;

	// Region selection by actor bounds.
	t0 = NowMs();
	std::vector<PxActor*> selected;
	for (PxActor* actor : list.actors)
	{
		struct BoundsCall
		{
			PxActor*  actor;
			PxBounds3 region;
			bool      touches;
		};
		BoundsCall bounds{ actor, aRegion, false };
		if (!actor)
		{
			continue;
		}
		if (!Guarded(bounds, [](BoundsCall& aCall) { aCall.touches = aCall.actor->getWorldBounds(1.0f).intersects(aCall.region); }))
		{
			++aFaults;
		}
		else if (bounds.touches)
		{
			selected.push_back(actor);
		}
	}
	dump.boundsSelected = uint32_t(selected.size());
	dump.selectMs = NowMs() - t0;

	// The same region through PhysX's own overlap query (scene-query shapes only), for comparison.
	t0 = NowMs();
	struct OverlapCall
	{
		PxScene*                  scene;
		PxBounds3                 region;
		std::vector<PxOverlapHit> hits;
		bool                      capped;
	};
	OverlapCall overlap{ aScene, aRegion, {}, false };
	if (!Guarded(overlap, [](OverlapCall& aCall) {
		    std::vector<PxOverlapHit> buffer(kOverlapBuffer);
		    CollectOverlaps           collect(buffer.data(), PxU32(buffer.size()), aCall.hits);
		    aCall.scene->overlap(PxBoxGeometry(aCall.region.getExtents()), PxTransform(aCall.region.getCenter()), collect,
		                         PxQueryFilterData(PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::eNO_BLOCK));
		    aCall.capped = collect.capped;
	    }))
	{
		++aFaults;
	}
	dump.overlapHits = uint32_t(overlap.hits.size());
	dump.overlapCapped = overlap.capped;
	dump.overlapMs = NowMs() - t0;

	// Triangles of every shape of the selected actors.
	t0 = NowMs();
	for (PxActor* actor : selected)
	{
		if (CountTriangles(dump.triangles) >= aTriangleBudget)
		{
			dump.triangleCapHit = true;
			break;
		}
		struct ExtractCall
		{
			PxActor*   actor;
			PxBounds3  region;
			PxVec3     feet;
			SceneDump* dump;
		};
		ExtractCall extract{ actor, aRegion, aFeet, &dump };
		if (!Guarded(extract, [](ExtractCall& aCall) {
			    const char*        kind = ActorKind(*aCall.actor);
			    const auto&        rigid = static_cast<const PxRigidActor&>(*aCall.actor);
			    const PxTransform  pose = rigid.getGlobalPose();
			    for (const PxShape* shape : ShapesOf(rigid))
			    {
				    if (!shape)
				    {
					    continue;
				    }
				    const PxGeometryType::Enum geometry = shape->getGeometryType();
				    AppendTriangles(*shape, pose, aCall.region,
				                    aCall.dump->triangles[Category(kind, shape->getFlags(), geometry)], aCall.dump->stats);
				    if (geometry == PxGeometryType::eCAPSULE && std::string_view(kind) != "static")
				    {
					    NoteCapsule(*shape, pose, aCall.feet, kind, aCall.dump->nearestCapsule);
				    }
			    }
		    }))
		{
			++aFaults;
		}
	}
	if (CountTriangles(dump.triangles) > aTriangleBudget)
	{
		TrimToBudget(dump.triangles, aTriangleBudget);
		dump.triangleCapHit = true;
	}
	std::erase_if(dump.triangles, [](const auto& aEntry) { return aEntry.second.empty(); });
	dump.extractMs = NowMs() - t0;

	UnlockRead(aScene);
	return dump;
}

std::optional<float> PhysxSession::RaycastDown(PxScene* aScene, const PxVec3& aFrom, float aMaxDistance, uint32_t& aFaults) const
{
	if (!LockRead(aScene))
	{
		++aFaults;
		return std::nullopt;
	}
	struct RayCall
	{
		PxScene* scene;
		PxVec3   from;
		float    maxDistance;
		bool     hit;
		float    distance;
	};
	RayCall    ray{ aScene, aFrom, aMaxDistance, false, 0.0f };
	const bool ok = Guarded(ray, [](RayCall& aCall) {
		PxRaycastBuffer buffer;
		aCall.scene->raycast(aCall.from, PxVec3(0.0f, 0.0f, -1.0f), aCall.maxDistance, buffer, PxHitFlag::eDEFAULT,
		                     PxQueryFilterData(PxQueryFlag::eSTATIC));
		aCall.hit = buffer.hasBlock;
		aCall.distance = buffer.hasBlock ? buffer.block.distance : 0.0f;
	});
	UnlockRead(aScene);
	if (!ok)
	{
		++aFaults;
		return std::nullopt;
	}
	return ray.hit ? std::optional<float>(ray.distance) : std::nullopt;
}
}
```

- [x] **Step 8: Build and run the tests; they must pass, with no warnings**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | tail -4
```
Expected: no build lines, then `PASS GuardedRunsTheFunction`, `PASS GuardedSurvivesAnAccessViolation`,
`PASS GuardedSurvivesAThrownException`, and `39 test(s), 0 failure(s)`.

- [x] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/physx-probe \
  && git commit -q -m "physx-probe: PhysX session, live-shape triangles and the crash guard" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 12: One dump, start to finish

`RunDump` needs a live `PhysxSession`, so there's no unit test; the check here is a clean build. Task 15 exercises it.

**Files:**
- Create: `src/Probe.h`, `src/Probe.cpp`
- Modify: `spikes/physx-probe/CMakeLists.txt`

- [x] **Step 1: Write `src/Probe.h`**

```cpp
#pragma once

#include "ObjWriter.h"
#include "Report.h"
#include "physx/PhysxSession.h"

#include <string>
#include <vector>

namespace cpmc
{
constexpr uint64_t kTriangleCap = 1'000'000;

struct DumpData
{
	DumpReport            report;
	std::vector<ObjGroup> groups;
};

// One dump on the main thread: the compatibility check, then per scene the census and region work under its read
// lock, then the coordinate check. The region box is V's feet + (0, 0, 8) with half-extents (32, 32, 24) m.
DumpData RunDump(px::PhysxSession& aSession, const PlayerPose& aPose, const std::string& aTrigger,
                 const std::string& aRuntime, const std::string& aTime);
}
```

- [x] **Step 2: Write `src/Probe.cpp`**

```cpp
#include "Probe.h"

#include "physx/Checks.h"

#include <cmath>
#include <format>
#include <map>

using namespace physx;

namespace cpmc
{
DumpData RunDump(px::PhysxSession& aSession, const PlayerPose& aPose, const std::string& aTrigger,
                 const std::string& aRuntime, const std::string& aTime)
{
	DumpData    data;
	DumpReport& report = data.report;
	report.time = aTime;
	report.trigger = aTrigger;
	report.runtime = aRuntime;
	report.player = aPose;

	const px::OpenResult& open = aSession.Open();
	report.physxFileVersion = open.physxFileVersion;
	report.steps = open.steps;
	report.compatibilityPass = open.pass;
	if (!open.pass)
	{
		return data;  // no geometry without a passing compatibility check
	}

	uint32_t                     faults = 0;
	const std::vector<PxScene*>  scenes = aSession.FetchScenes(faults);
	const PxVec3                 feet(float(aPose.feet.x), float(aPose.feet.y), float(aPose.feet.z));
	const PxVec3                 center = feet + PxVec3(0.0f, 0.0f, 8.0f);
	const PxVec3                 halfExtents(32.0f, 32.0f, 24.0f);
	const PxBounds3              regionBox = PxBounds3::centerExtents(center, halfExtents);
	RegionResult                 region;
	region.center = { center.x, center.y, center.z };
	region.halfExtents = { halfExtents.x, halfExtents.y, halfExtents.z };
	std::map<std::string, std::vector<px::Tri>> byCategory;
	std::optional<NearestCapsule>               nearest;
	uint64_t                                    kept = 0;

	for (size_t i = 0; i < scenes.size(); ++i)
	{
		px::SceneDump scene = aSession.DumpScene(scenes[i], int(i), regionBox, feet, kTriangleCap - kept, faults);
		report.censusMismatches += scene.census.mismatches;
		region.boundsSelectedActors += scene.boundsSelected;
		region.overlapHits += scene.overlapHits;
		region.overlapOverflow |= scene.overlapCapped;
		region.selectMs += scene.selectMs;
		region.overlapMs += scene.overlapMs;
		region.extractMs += scene.extractMs;
		region.skippedPlanes += scene.stats.planes;
		region.unknownGeometry += scene.stats.unknownGeometry;
		region.badTriangles += scene.stats.badTriangles;
		region.triangleCapHit |= scene.triangleCapHit;
		for (auto& [category, triangles] : scene.triangles)
		{
			const std::string name = scenes.size() > 1 ? std::format("s{}_{}", i, category) : category;
			region.trianglesByCategory[name] += triangles.size();
			region.triangles += triangles.size();
			kept += triangles.size();
			auto& destination = byCategory[name];
			destination.insert(destination.end(), triangles.begin(), triangles.end());
		}
		if (scene.nearestCapsule && (!nearest || scene.nearestCapsule->distance < nearest->distance))
		{
			nearest = scene.nearestCapsule;
		}
		scene.census.queryFilter = px::TrimTally(scene.census.queryFilter);
		scene.census.simulationFilter = px::TrimTally(scene.census.simulationFilter);
		report.scenes.push_back(std::move(scene.census));
	}

	// The coordinate check: a ray down from 1.8 m above V's feet should hit the ground about 1.8 m down.
	CoordinateCheck check;
	check.nearestCapsule = nearest;
	const PxVec3 from = feet + PxVec3(0.0f, 0.0f, 1.8f);
	for (size_t i = 0; i < scenes.size(); ++i)
	{
		const std::optional<float> distance = aSession.RaycastDown(scenes[i], from, 3.0f, faults);
		if (distance && (!check.rayHit || *distance < check.hitDistance))
		{
			check.rayHit = true;
			check.hitDistance = *distance;
			check.scene = int(i);
		}
	}
	check.pass = check.rayHit && std::fabs(check.hitDistance - 1.8) <= 0.3;

	report.region = region;
	report.coordinateCheck = check;
	report.faults = faults;
	report.compatibilityPass = open.pass && report.censusMismatches == 0;
	for (auto& [name, triangles] : byCategory)
	{
		data.groups.push_back({ name, CategoryColor(name), std::move(triangles) });
	}
	report.objWritten = !data.groups.empty();
	return data;
}
}
```

- [x] **Step 3: Add the source to `CMakeLists.txt`**

Add `src/Probe.cpp` as the last entry of the `probe_core` list.

- [x] **Step 4: Build and run the tests; they must still pass, with no warnings**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | tail -1
```
Expected: `39 test(s), 0 failure(s)`

- [x] **Step 5: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/physx-probe \
  && git commit -q -m "physx-probe: one dump, start to finish" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 13: The RED4ext plugin

**Files:**
- Create: `src/PlayerLocator.h`, `src/PlayerLocator.cpp`, `src/Plugin.cpp`
- Modify: `spikes/physx-probe/CMakeLists.txt`

- [x] **Step 1: Write `src/PlayerLocator.h`**

```cpp
#pragma once

#include "Report.h"

#include <optional>

namespace cpmc
{
// V's feet and facing, plus the active camera when it can be read, through the game's own script functions:
// PlayerSystem.GetLocalPlayerMainGameObject, then GetWorldPosition and GetWorldForward on V, and
// CameraSystem.GetActiveCameraWorldTransform / GetActiveCameraForward / GetActiveCameraFOV.
// Nothing when there is no player (main menu, loading). Main thread only.
std::optional<PlayerPose> LocatePlayer();
}
```

- [x] **Step 2: Write `src/PlayerLocator.cpp`**

```cpp
#include "PlayerLocator.h"

#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/Generated/Transform.hpp>
#include <RED4ext/Scripting/Natives/Generated/Vector4.hpp>

#include <cmath>

namespace cpmc
{
namespace
{
RED4ext::CClass* ClassByScriptName(const char* aName)
{
	auto* rtti = RED4ext::CRTTISystem::Get();
	return rtti ? rtti->GetClassByScriptName(aName) : nullptr;
}

bool GameReady()
{
	auto* engine = RED4ext::CGameEngine::Get();
	return engine && engine->framework && engine->framework->gameInstance;
}

// Calls a no-argument script function on an object, if the object's class (or a parent) has it.
template<class Out>
bool CallOn(RED4ext::IScriptable* aObject, const char* aFunction, Out& aOut)
{
	auto* function = aObject->GetType()->GetFunction(aFunction);
	return function && RED4ext::ExecuteFunction(aObject, function, &aOut);
}

float Length(const RED4ext::Vector4& aVector)
{
	return std::sqrt(aVector.X * aVector.X + aVector.Y * aVector.Y + aVector.Z * aVector.Z);
}

std::optional<CameraPose> LocateCamera(const Vec3d& aFeet)
{
	auto* cameraSystem = ClassByScriptName("CameraSystem");
	if (!cameraSystem)
	{
		return std::nullopt;
	}
	RED4ext::Transform transform{};
	bool               haveTransform = false;
	RED4ext::Vector4   forward{};
	float              fov = 0.0f;
	if (!RED4ext::ExecuteFunction(cameraSystem, "GetActiveCameraWorldTransform", &haveTransform, transform) || !haveTransform ||
	    !RED4ext::ExecuteFunction(cameraSystem, "GetActiveCameraForward", &forward) ||
	    !RED4ext::ExecuteFunction(cameraSystem, "GetActiveCameraFOV", &fov))
	{
		return std::nullopt;
	}
	const Vec3d position{ transform.position.X, transform.position.Y, transform.position.Z };
	const double dx = position.x - aFeet.x, dy = position.y - aFeet.y, dz = position.z - aFeet.z;
	// A camera far from V (a scene camera, a vehicle camera) or a bad read is not V's eye.
	if (std::sqrt(dx * dx + dy * dy + dz * dz) > 5.0 || Length(forward) < 0.5f || !(fov > 10.0f && fov < 170.0f))
	{
		return std::nullopt;
	}
	return CameraPose{ position, { forward.X, forward.Y, forward.Z }, fov };
}
}

std::optional<PlayerPose> LocatePlayer()
{
	if (!GameReady())
	{
		return std::nullopt;
	}
	auto* playerSystem = ClassByScriptName("PlayerSystem");
	if (!playerSystem)
	{
		return std::nullopt;
	}
	RED4ext::Handle<RED4ext::IScriptable> player;
	if (!RED4ext::ExecuteFunction(playerSystem, "GetLocalPlayerMainGameObject", &player) || !player)
	{
		return std::nullopt;
	}
	RED4ext::Vector4 position{};
	RED4ext::Vector4 forward{};
	if (!CallOn(player.GetPtr(), "GetWorldPosition", position) || !CallOn(player.GetPtr(), "GetWorldForward", forward))
	{
		return std::nullopt;
	}
	PlayerPose pose;
	pose.feet = { position.X, position.Y, position.Z };
	pose.forward = { forward.X, forward.Y, forward.Z };
	pose.camera = LocateCamera(pose.feet);
	return pose;
}
}
```

- [x] **Step 3: Write `src/Plugin.cpp`**

```cpp
// PhysXProbe: a read-only RED4ext plugin that checks whether Cyberpunk's PhysX 3.4.2 can be read through
// NVIDIA's public headers and dumps the collision around V. See docs/superpowers/specs/2026-10-01-physx-probe-design.md.
#include <RED4ext/RED4ext.hpp>

#include "DumpWriter.h"
#include "PlayerLocator.h"
#include "Probe.h"

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <optional>
#include <string>

namespace
{
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

RED4ext::v1::PluginHandle   g_handle = nullptr;
const RED4ext::v1::Sdk*     g_sdk = nullptr;
std::filesystem::path       g_pluginDir;
cpmc::px::PhysxSession      g_session;
cpmc::DumpWriter            g_writer;
std::string                 g_runtime;

bool                        g_f10WasDown = false;
Clock::time_point           g_lastDump{};
Clock::time_point           g_lastSecondlyCheck{};
std::optional<Clock::time_point> g_firstSeenPlayer;
bool                        g_autoDone = false;

template<class... Args>
void Log(std::format_string<Args...> aFormat, Args&&... aArgs)
{
	if (g_sdk)
	{
		g_sdk->logger->Info(g_handle, std::format(aFormat, std::forward<Args>(aArgs)...).c_str());
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

bool GameInForeground()
{
	DWORD processId = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &processId);
	return processId == GetCurrentProcessId();
}

// "2026-10-01T12:34:56" for the report, "2026-10-01_12-34-56" for the folder.
std::pair<std::string, std::string> Timestamps()
{
	SYSTEMTIME now{};
	GetLocalTime(&now);
	return { std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond),
	         std::format("{:04}-{:02}-{:02}_{:02}-{:02}-{:02}", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond) };
}

void RunOne(const std::string& aTrigger)
{
	const auto pose = cpmc::LocatePlayer();
	if (!pose)
	{
		Log("{} trigger: no player", aTrigger);
		return;
	}
	const auto [time, folderName] = Timestamps();
	LARGE_INTEGER start;
	LARGE_INTEGER end;
	LARGE_INTEGER frequency;
	QueryPerformanceCounter(&start);
	cpmc::DumpData data = cpmc::RunDump(g_session, *pose, aTrigger, g_runtime, time);
	QueryPerformanceCounter(&end);
	QueryPerformanceFrequency(&frequency);
	data.report.mainThreadPauseMs = double(end.QuadPart - start.QuadPart) * 1000.0 / double(frequency.QuadPart);

	const auto& report = data.report;
	Log("dump ({}): compatibility {}, {} scene(s), {} triangles, coordinate check {}, main-thread pause {:.1f} ms, {} fault(s)",
	    aTrigger, report.compatibilityPass ? "passed" : "FAILED", report.scenes.size(),
	    report.region ? report.region->triangles : 0,
	    report.coordinateCheck ? (report.coordinateCheck->pass ? "passed" : "FAILED") : "not run", report.mainThreadPauseMs,
	    report.faults);
	for (const auto& step : report.steps)
	{
		if (!step.pass)
		{
			Log("  compatibility step {} ({}) failed: {}", step.step, step.name, step.detail);
		}
	}
	cpmc::DumpJob job{ g_pluginDir / "dumps" / folderName, std::move(data.report), std::move(data.groups), pose->feet };
	g_writer.Start(std::move(job));
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
	if (auto written = g_writer.TakeResult())
	{
		if (written->ok)
		{
			Log("dump written to {} ({} triangles, {:.0f} ms)", written->folder.string(), written->triangles, written->writeMs);
		}
		else
		{
			Log("couldn't write the dump to {}: {}", written->folder.string(), written->error);
		}
	}

	const auto  now = Clock::now();
	std::string trigger;

	const bool f10Down = (GetAsyncKeyState(VK_F10) & 0x8000) != 0 && GameInForeground();
	if (f10Down && !g_f10WasDown)
	{
		trigger = "hotkey";
	}
	g_f10WasDown = f10Down;

	if (now - g_lastSecondlyCheck >= 1s)
	{
		g_lastSecondlyCheck = now;
		const auto      request = g_pluginDir / "dump.request";
		std::error_code error;
		if (trigger.empty() && std::filesystem::exists(request, error))
		{
			std::filesystem::remove(request, error);
			trigger = "request";
		}
		if (!g_autoDone)
		{
			if (!g_firstSeenPlayer && cpmc::LocatePlayer())
			{
				g_firstSeenPlayer = now;
			}
			if (trigger.empty() && g_firstSeenPlayer && now - *g_firstSeenPlayer >= 10s)
			{
				g_autoDone = true;
				trigger = "auto";
			}
		}
	}

	if (trigger.empty() || now - g_lastDump < 2s)
	{
		return false;
	}
	if (g_writer.Busy())
	{
		Log("{} trigger ignored: the previous dump is still being written", trigger);
		return false;
	}
	g_lastDump = now;
	RunOne(trigger);
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
		g_pluginDir = PluginFolder();
		g_runtime = std::format("{}.{}.{}", unsigned(aSdk->runtime->major), aSdk->runtime->minor, aSdk->runtime->patch);
		static RED4ext::v1::GameState running{ &OnEnter, &OnUpdate, &OnExit };
		aSdk->gameStates->Add(aHandle, RED4ext::EGameStateType::Running, &running);
		Log("PhysXProbe 0.1.0 loaded (game {}); F10 or {} triggers a dump", g_runtime, (g_pluginDir / "dump.request").string());
		break;
	}
	case RED4ext::v1::EMainReason::Unload:
		g_writer.Join();
		break;
	}
	return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* aInfo)
{
	aInfo->name = L"PhysXProbe";
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

- [x] **Step 4: Add the SDK and the plugin target to `CMakeLists.txt`**

Insert this block right after the `probe_core` block (before `# Unit tests`):
```cmake
# The RED4ext SDK, header-only. SYSTEM keeps its warnings out of ours.
set(RED4EXT_HEADER_ONLY ON CACHE BOOL "" FORCE)
add_subdirectory("${RED4EXT_SDK_DIR}" red4ext-sdk EXCLUDE_FROM_ALL SYSTEM)

# The RED4ext plugin.
add_library(PhysXProbe SHARED
	src/Plugin.cpp
	src/PlayerLocator.cpp
)
target_link_libraries(PhysXProbe PRIVATE probe_core RED4ext::SDK)

if(CP2077_DIR)
	add_custom_command(TARGET PhysXProbe POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CP2077_DIR}/red4ext/plugins/PhysXProbe"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:PhysXProbe>" "$<TARGET_PDB_FILE:PhysXProbe>" "${CP2077_DIR}/red4ext/plugins/PhysXProbe/"
		VERBATIM)
endif()
```

- [x] **Step 5: Build; there must be no errors or warnings, and the tests still pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | tail -1; ls -la build/RelWithDebInfo/PhysXProbe.dll
```
Expected: `39 test(s), 0 failure(s)`, and `PhysXProbe.dll` exists (about 800 KB).

- [x] **Step 6: Check the DLL's exports and imports**

```bash
DUMPBIN="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/dumpbin.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && echo "entry points: $("$DUMPBIN" -nologo -exports build/RelWithDebInfo/PhysXProbe.dll | grep -c -E ' (Main|Query|Supports) = ')" \
  && echo "PhysX3 imports: $("$DUMPBIN" -nologo -imports build/RelWithDebInfo/PhysXProbe.dll | grep -c PhysX3)" \
  && git -C /d/Cyberpunk-Minecraft/extern/RED4ext.SDK status --short | wc -l
```
Expected: `entry points: 3`, `PhysX3 imports: 0`, then `0`. The last number confirms the SDK submodule is still clean,
because configuring rewrites its `Version.hpp` with identical content.

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/physx-probe \
  && git commit -q -m "physx-probe: RED4ext plugin (entry points, triggers, player lookup)" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

- [x] **Step 8: Deploy into the game (the game must not be running)**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game first, because a running game locks the DLL.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --preset default -DCP2077_DIR=E:/Cyberpunk2077 > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ls -la /e/Cyberpunk2077/red4ext/plugins/PhysXProbe/
```
Expected: `PhysXProbe.dll` and `PhysXProbe.pdb` in `E:\Cyberpunk2077\red4ext\plugins\PhysXProbe\`. To uninstall, delete
that folder.

---

### Task 14: The dump renderer

**Files:**
- Create: `tools/test_render_dump.py`, `tools/render_dump.py`

- [x] **Step 1: Write the failing tests, `tools/test_render_dump.py`**

```python
"""Tests for render_dump.py:  uv run --no-project --with numpy --with pillow --with pytest pytest tools/test_render_dump.py -q"""

import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import render_dump as rd  # noqa: E402

OBJ = """# PhysXProbe collision dump
mtllib collision.mtl
o static_trimesh
usemtl static_trimesh
v 0.000 0.000 0.000
v 1.000 0.000 0.000
v 0.000 0.000 -1.000
f 1 2 3
o trigger_box
usemtl trigger_box
v 0.000 1.000 0.000
v 1.000 1.000 0.000
v 0.000 1.000 -1.000
f 4 5 6
"""


def test_load_obj_groups_faces(tmp_path):
    path = tmp_path / "collision.obj"
    path.write_text(OBJ)
    groups = rd.load_obj(path)
    assert sorted(groups) == ["static_trimesh", "trigger_box"]
    assert groups["static_trimesh"].shape == (1, 3, 3)
    assert groups["trigger_box"][0][2].tolist() == [0.0, 1.0, -1.0]


def test_hidden_groups():
    assert rd.is_hidden("trigger_box")
    assert rd.is_hidden("s1_queryonly_trimesh")
    assert not rd.is_hidden("static_trimesh")
    assert not rd.is_hidden("s0_static_trimesh")


def test_mc_from_world():
    assert rd.mc_from_world([1.0, 2.0, 3.0]).tolist() == [1.0, 3.0, -2.0]


def test_clip_near():
    front = np.array([[0, 0, 1.0], [1, 0, 1.0], [0, 1, 1.0]])
    behind = np.array([[0, 0, -1.0], [1, 0, -1.0], [0, 1, -1.0]])
    crossing = np.array([[0, 0, -1.0], [1, 0, 1.0], [0, 1, 1.0]])
    assert len(rd.clip_near(front)) == 3
    assert len(rd.clip_near(behind)) == 0
    clipped = rd.clip_near(crossing)
    assert len(clipped) == 4
    assert (clipped[:, 2] >= rd.NEAR - 1e-9).all()


def test_render_eye_occludes_the_floor_behind_a_wall():
    # A big floor and a wall 6 m ahead standing on it. Just above the wall's bottom edge, the wall must win.
    # (Sorting triangles far to near got this wrong: floor pieces behind the wall were drawn over its base.)
    floor = np.array([
        [[-20.0, 0.0, -20.0], [20.0, 0.0, -20.0], [20.0, 0.0, 20.0]],
        [[-20.0, 0.0, -20.0], [20.0, 0.0, 20.0], [-20.0, 0.0, 20.0]],
    ])
    wall = np.array([
        [[-2.0, 0.0, -6.0], [2.0, 0.0, -6.0], [2.0, 4.0, -6.0]],
        [[-2.0, 0.0, -6.0], [2.0, 4.0, -6.0], [-2.0, 4.0, -6.0]],
    ])
    groups = {"static_trimesh": floor, "static_box": wall}
    colors = {"static_trimesh": (0.0, 1.0, 0.0), "static_box": (1.0, 0.0, 0.0)}
    image = rd.render_eye(groups, colors, eye=[0.0, 1.7, 0.0], forward=[0.0, 0.0, -1.0], fov_deg=80.0, size=(320, 180))
    # The wall's base is at screen y ~ 120 (for a vertical fov of 80 degrees); check just above it.
    r, g, b = image.getpixel((160, 116))
    assert r > 100 and g == 0, (r, g, b)
    r, g, b = image.getpixel((160, 170))  # the floor in front of the wall
    assert g > 100 and r == 0, (r, g, b)


def test_render_eye_sees_a_wall_straight_ahead():
    # A red wall 3 m in front of the eye (Minecraft -Z is north, straight ahead here), a grey floor below.
    wall = np.array([
        [[-5.0, -2.0, -3.0], [5.0, -2.0, -3.0], [5.0, 8.0, -3.0]],
        [[-5.0, -2.0, -3.0], [5.0, 8.0, -3.0], [-5.0, 8.0, -3.0]],
    ])
    groups = {"static_box": wall}
    colors = {"static_box": (1.0, 0.0, 0.0)}
    image = rd.render_eye(groups, colors, eye=[0.0, 1.7, 0.0], forward=[0.0, 0.0, -1.0], fov_deg=80.0, size=(320, 180))
    r, g, b = image.getpixel((160, 90))
    assert r > 100 and g == 0 and b == 0


def test_render_top_draws_the_floor(tmp_path):
    floor = np.array([[[-10.0, 0.0, -10.0], [10.0, 0.0, -10.0], [10.0, 0.0, 10.0]]])
    image = rd.render_top({"static_trimesh": floor}, {"static_trimesh": (0.0, 1.0, 0.0)}, size=64)
    assert image.getpixel((40, 30))[1] > 100  # inside the triangle: green
    assert image.getpixel((5, 60)) == rd.SKY  # outside it: background
```

- [x] **Step 2: Run the tests; they must fail**

```bash
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && uv run --no-project -q --with numpy --with pillow --with pytest pytest tools/test_render_dump.py -q -p no:cacheprovider 2>&1 | tail -3
```
Expected: `ModuleNotFoundError: No module named 'render_dump'`

- [x] **Step 3: Write `tools/render_dump.py`**

```python
"""Render a PhysXProbe dump to images, so it can be checked without Blender.

    uv run --no-project --with numpy --with pillow tools/render_dump.py <dump folder> [--all] [--fov 80] [--fov-axis v|h]

Writes top.png (orthographic, from above, north up) and eye.png (from the camera recorded in report.json, or
1.7 m above V's feet looking along V's facing) into the dump folder. collision.obj is in Minecraft axes
(X east, Y up, Z south), relative to V's feet; report.json is in world axes (X east, Y north, Z up).
Both views use a z-buffer, so walls correctly hide what's behind them.
"""

from __future__ import annotations

import argparse
import json
import math
import re
from pathlib import Path

import numpy as np
from PIL import Image

HIDDEN_KINDS = ("trigger_", "queryonly_")
NEAR = 0.05
SKY = (22, 24, 30)


def is_hidden(group: str) -> bool:
    """trigger_* and queryonly_* groups, with or without a scene prefix such as s1_."""
    return re.sub(r"^s\d+_", "", group).startswith(HIDDEN_KINDS)


def load_mtl(path: Path) -> dict[str, tuple[float, float, float]]:
    colors: dict[str, tuple[float, float, float]] = {}
    name = None
    for line in path.read_text().splitlines():
        parts = line.split()
        if parts[:1] == ["newmtl"]:
            name = parts[1]
        elif parts[:1] == ["Kd"] and name:
            colors[name] = (float(parts[1]), float(parts[2]), float(parts[3]))
    return colors


def load_obj(path: Path) -> dict[str, np.ndarray]:
    """Triangles per group: {group: array of shape (n, 3, 3)}."""
    vertices: list[tuple[float, float, float]] = []
    faces: dict[str, list[tuple[int, int, int]]] = {}
    group = "default"
    for line in path.read_text().splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "v":
            vertices.append((float(parts[1]), float(parts[2]), float(parts[3])))
        elif parts[0] == "o":
            group = parts[1]
            faces.setdefault(group, [])
        elif parts[0] == "f":
            faces.setdefault(group, []).append(tuple(int(p.split("/")[0]) - 1 for p in parts[1:4]))
    table = np.array(vertices, dtype=np.float64).reshape(-1, 3)
    return {name: table[np.array(tris, dtype=np.int64)] for name, tris in faces.items() if tris}


def mc_from_world(v) -> np.ndarray:
    """World (X east, Y north, Z up) to Minecraft axes (X east, Y up, Z south)."""
    return np.array([v[0], v[2], -v[1]], dtype=np.float64)


def clip_near(points: np.ndarray, near: float = NEAR) -> np.ndarray:
    """Clip a camera-space polygon (depth in column 2) to depth >= near. Returns 0, 3 or 4+ points."""
    out = []
    count = len(points)
    for i in range(count):
        a, b = points[i], points[(i + 1) % count]
        a_in, b_in = a[2] >= near, b[2] >= near
        if a_in:
            out.append(a)
        if a_in != b_in:
            t = (near - a[2]) / (b[2] - a[2])
            out.append(a + (b - a) * t)
    return np.array(out) if len(out) >= 3 else np.zeros((0, 3))


def shade(color, normal: np.ndarray, light: np.ndarray, ambient: float) -> np.ndarray:
    """Two-sided Lambert shading of a flat colour, as 0-255 RGB."""
    length = np.linalg.norm(normal)
    lambert = abs(float(np.dot(normal / length, light))) if length > 0 else 0.0
    k = ambient + (1.0 - ambient) * lambert
    return np.clip(np.round(255.0 * np.asarray(color) * k), 0, 255).astype(np.uint8)


class Canvas:
    """An RGB image with a depth buffer. Nearer (smaller) depths win."""

    def __init__(self, width: int, height: int):
        self.width, self.height = width, height
        self.rgb = np.empty((height, width, 3), dtype=np.uint8)
        self.rgb[:] = SKY
        self.depth = np.full((height, width), np.inf)

    def triangle(self, xy: np.ndarray, depth: np.ndarray, color: np.ndarray, perspective: bool) -> None:
        """Fill a screen-space triangle (xy: 3x2 pixels, depth: 3 values) at pixel centres."""
        x_min = max(int(math.floor(xy[:, 0].min())), 0)
        x_max = min(int(math.ceil(xy[:, 0].max())), self.width - 1)
        y_min = max(int(math.floor(xy[:, 1].min())), 0)
        y_max = min(int(math.ceil(xy[:, 1].max())), self.height - 1)
        if x_min > x_max or y_min > y_max:
            return
        (x0, y0), (x1, y1), (x2, y2) = xy
        area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        if abs(area) < 1e-12:
            return
        xs, ys = np.meshgrid(np.arange(x_min, x_max + 1) + 0.5, np.arange(y_min, y_max + 1) + 0.5)
        w0 = ((x1 - xs) * (y2 - ys) - (x2 - xs) * (y1 - ys)) / area
        w1 = ((x2 - xs) * (y0 - ys) - (x0 - xs) * (y2 - ys)) / area
        w2 = 1.0 - w0 - w1
        inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        if perspective:  # 1/depth is linear in screen space
            z = 1.0 / (w0 / depth[0] + w1 / depth[1] + w2 / depth[2])
        else:
            z = w0 * depth[0] + w1 * depth[1] + w2 * depth[2]
        window = self.depth[y_min:y_max + 1, x_min:x_max + 1]
        nearer = inside & (z < window)
        window[nearer] = z[nearer]
        self.rgb[y_min:y_max + 1, x_min:x_max + 1][nearer] = color

    def image(self) -> Image.Image:
        return Image.fromarray(self.rgb, "RGB")


def render_top(groups, colors, size: int = 1024, extent: float = 32.0) -> Image.Image:
    """Looking straight down on +-extent metres around V; north (-Z) is up."""
    canvas = Canvas(size, size)
    scale = size / (2.0 * extent)
    light = np.array([0.4, 1.0, 0.3]) / np.linalg.norm([0.4, 1.0, 0.3])
    for name, tris in groups.items():
        color = colors.get(name, (0.6, 0.6, 0.6))
        for tri in tris:
            xy = np.stack([(tri[:, 0] + extent) * scale, (tri[:, 2] + extent) * scale], axis=1)
            fill = shade(color, np.cross(tri[1] - tri[0], tri[2] - tri[0]), light, 0.35)
            canvas.triangle(xy, -tri[:, 1], fill, perspective=False)
    return canvas.image()


def camera_basis(forward: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    f = forward / np.linalg.norm(forward)
    up = np.array([0.0, 1.0, 0.0])
    right = np.cross(f, up)
    if np.linalg.norm(right) < 1e-6:
        right = np.array([1.0, 0.0, 0.0])
    right /= np.linalg.norm(right)
    return right, np.cross(right, f), f


def render_eye(groups, colors, eye, forward, fov_deg: float, fov_axis: str = "v", size=(1280, 720)) -> Image.Image:
    """Perspective view from eye along forward (both in Minecraft axes, relative to V's feet)."""
    width, height = size
    aspect = width / height
    if fov_axis == "h":
        tan_h = math.tan(math.radians(fov_deg) / 2.0)
        tan_v = tan_h / aspect
    else:
        tan_v = math.tan(math.radians(fov_deg) / 2.0)
        tan_h = tan_v * aspect
    right, up, f = camera_basis(np.asarray(forward, dtype=np.float64))
    eye = np.asarray(eye, dtype=np.float64)
    light = -f + up * 0.5
    light /= np.linalg.norm(light)
    canvas = Canvas(width, height)
    for name, tris in groups.items():
        color = colors.get(name, (0.6, 0.6, 0.6))
        rel = tris - eye
        cams = np.stack([rel @ right, rel @ up, rel @ f], axis=2)  # (n, 3, 3) camera space
        for tri, cam in zip(tris, cams):
            if (cam[:, 2] < NEAR).all():
                continue  # entirely behind the camera
            polygon = cam if (cam[:, 2] >= NEAR).all() else clip_near(cam)
            if len(polygon) == 0:
                continue
            xy = np.stack([width / 2.0 * (1.0 + polygon[:, 0] / (polygon[:, 2] * tan_h)),
                           height / 2.0 * (1.0 - polygon[:, 1] / (polygon[:, 2] * tan_v))], axis=1)
            fill = shade(color, np.cross(tri[1] - tri[0], tri[2] - tri[0]), light, 0.3)
            for k in range(1, len(polygon) - 1):  # fan-triangulate the clipped polygon
                corners = [0, k, k + 1]
                canvas.triangle(xy[corners], polygon[corners, 2], fill, perspective=True)
    return canvas.image()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("dump", type=Path, help="a dumps/<timestamp> folder")
    parser.add_argument("--all", action="store_true", help="include trigger_* and queryonly_* groups")
    parser.add_argument("--fov", type=float, help="field of view in degrees (default: the report's camera, else 80)")
    parser.add_argument("--fov-axis", choices=("v", "h"), default="v", help="whether the field of view is vertical or horizontal")
    args = parser.parse_args()

    report = json.loads((args.dump / "report.json").read_text())
    groups = load_obj(args.dump / "collision.obj")
    colors = load_mtl(args.dump / "collision.mtl")
    if not args.all:
        groups = {name: tris for name, tris in groups.items() if not is_hidden(name)}

    player = report["player"]
    feet = np.array(player["feet"], dtype=np.float64)
    camera = player.get("camera")
    if camera:
        eye = mc_from_world(np.array(camera["position"]) - feet)
        forward = mc_from_world(camera["forward"])
        fov = args.fov or camera["fov"]
    else:
        eye = np.array([0.0, 1.7, 0.0])
        forward = mc_from_world(player["forward"])
        fov = args.fov or 80.0

    render_top(groups, colors).save(args.dump / "top.png")
    render_eye(groups, colors, eye, forward, fov, args.fov_axis).save(args.dump / "eye.png")
    total = sum(len(t) for t in groups.values())
    print(f"rendered {total} triangles in {len(groups)} group(s) to {args.dump / 'top.png'} and {args.dump / 'eye.png'}")


if __name__ == "__main__":
    main()
```

- [x] **Step 4: Run the tests; they must pass**

```bash
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && uv run --no-project -q --with numpy --with pillow --with pytest pytest tools/test_render_dump.py -q -p no:cacheprovider 2>&1 | tail -2
```
Expected: `7 passed`

- [x] **Step 5: Smoke-test the command line on a synthetic dump, and look at the result**

```bash
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && mkdir -p smoke/demo \
  && printf '# PhysXProbe collision dump\nmtllib collision.mtl\no static_trimesh\nusemtl static_trimesh\nv -20 0 -20\nv 20 0 -20\nv 20 0 20\nv -20 0 20\nf 1 2 3\nf 1 3 4\no static_box\nusemtl static_box\nv -2 0 -6\nv 2 0 -6\nv 2 4 -6\nv -2 4 -6\nf 5 6 7\nf 5 7 8\no trigger_box\nusemtl trigger_box\nv -1 0 -1\nv 1 0 -1\nv 0 3 -1\nf 9 10 11\n' > smoke/demo/collision.obj \
  && printf 'newmtl static_trimesh\nKd 0.620 0.620 0.620\nnewmtl static_box\nKd 0.550 0.600 0.700\nnewmtl trigger_box\nKd 0.950 0.850 0.200\n' > smoke/demo/collision.mtl \
  && printf '{"player": {"feet": [100.0, 200.0, 10.0], "forward": [0.0, 1.0, 0.0], "camera": {"position": [100.0, 200.0, 11.7], "forward": [0.0, 1.0, -0.1], "fov": 80}}}' > smoke/demo/report.json \
  && uv run --no-project -q --with numpy --with pillow python tools/render_dump.py smoke/demo
```
Expected: `rendered 4 triangles in 2 group(s) ...`, with the trigger group hidden. Open `smoke/demo/eye.png`. It should show
a grey floor, and a blue-grey panel ahead whose base is a straight horizontal edge, not a V.

- [x] **Step 6: Commit** (`smoke/` is ignored)

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/physx-probe/tools \
  && git commit -q -m "physx-probe: dump renderer with z-buffer, and its tests" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 15: Verify in the running game (superseded by Task 22)

Steps 1 and 2 ran on 2026-10-01, against revision 1:
- **The first run** stopped at compatibility step 1. NVIDIA's 3.4.2 release keeps 3.4.1.0 in the binary version field,
  so step 1 now reads the "FileVersion" string (`e3cbc0b`).
- **The second run** fired the automatic dump on the startup screen and passed all seven steps, then froze the game.
  - **The shift:** the game's `PxShape` has an extra virtual function, so the probe's `PxShape` calls ran the next
    function, several of them setters.
  - **The freeze:** the game's crash handler fired on the resulting fault before the crash guard could.
  - **The record:** `docs/spikes/2026-10-01-physx-abi-audit.md` has the evidence, and the spec's revision 2 the
    redesign.

Task 22 replaces the remaining steps.

---

## Revision 2

Task 15's first in-game runs found two things the spec hadn't foreseen. The game's PhysX is a modified 3.4.2 whose
`PxShape` has one more virtual function than NVIDIA's headers, so revision 1's `PxShape` calls landed one slot late,
several of them on setters. And the game's crash handler sees a fault before any `__try/__except`, so revision 1's
crash guard froze the game instead of protecting it. `docs/spikes/2026-10-01-physx-abi-audit.md` records the audit of
the game's DLLs, and the spec's revision 2 redesigns the compatibility check around it. Tasks 16 to 21 build revision
2, test-first where a test can run without the game; Task 22 verifies it in the game.

- All the code in Tasks 16 to 21 has been compiled and tested once in a scratch copy, like revision 1's: zero compiler
  warnings at `/W4`, the test counts below, and the plugin exporting exactly `Main`, `Query` and `Supports` with no
  PhysX imports.
- The test count goes 42 (after `e3cbc0b`), then 50, 54, 60, 60, 55 and 55. Task 20 deletes five tests whose code goes
  away: the crash guard's three, and two for functions revision 2 no longer has.

---

### Task 16: The game's `PxShape` layout, pinned by a test

**Files:**
- Create: `tests/TestAudit.cpp`
- Modify: `CMakeLists.txt` (the test list)
- Modify: `extern/physx-3.4.2/PhysX_3.4/Include/PxShape.h` (one declaration)
- Modify: `extern/physx-3.4.2/README.md`

- [ ] **Step 1: Stop builds from installing into the game**

The CMake cache still has `CP2077_DIR=E:/Cyberpunk2077` from Task 13, so every build would copy a half-finished plugin
into the game. Task 22 sets it again.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --preset default -DCP2077_DIR= > /dev/null \
  && grep "^CP2077_DIR" build/CMakeCache.txt
```
Expected: `CP2077_DIR:PATH=`

- [ ] **Step 2: Write the failing test `tests/TestAudit.cpp`**

It checks the vtable slot of every PhysX call the probe makes against the slot the audit verified in the game's DLLs,
and the layout of every struct the game reads from or writes into the probe's memory.
```cpp
#include "Harness.h"

#include "physx/PxHeaders.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

// The probe's side of the PhysX ABI, checked against what the audit verified in the game's DLLs
// (docs/spikes/2026-10-01-physx-abi-audit.md): the vtable slot of every call the probe makes, the layout of the hit
// callbacks the game fills in and calls back through, and the structs both sides share.
//
// Slots are measured with a fake object whose vtable entries each record their own index. Each entry returns its
// second argument, which is where a call that returns a struct passes the hidden return pointer. Calling through a
// fake object is outside standard C++, but it compiles to exactly what the probe does to the game's objects.
using namespace physx;

namespace
{
int g_slot = -1;

template<int I>
void* Record(void*, void* aSecond)
{
	g_slot = I;
	return aSecond;
}

using Entry = void* (*)(void*, void*);

template<int... I>
constexpr std::array<Entry, sizeof...(I)> MakeVtable(std::integer_sequence<int, I...>)
{
	return { &Record<I>... };
}

constexpr auto kVtable = MakeVtable(std::make_integer_sequence<int, 128>{});

// A stand-in object: a vtable pointer, then the object's data from offset 8.
struct Fake
{
	const Entry*  vtable = kVtable.data();
	unsigned char body[256] = {};
};

// The vtable slot a call on a T goes through.
template<class T, class Call>
int SlotOf(Call aCall)
{
	Fake fake;
	g_slot = -1;
	aCall(*reinterpret_cast<T*>(&fake));
	return g_slot;
}

void Expect(const char* aCall, int aSlot, int aGameSlot)
{
	if (aSlot != aGameSlot)
	{
		th::Fail(__FILE__, __LINE__,
		         std::string(aCall) + " goes through slot " + std::to_string(aSlot) + "; the game's is " + std::to_string(aGameSlot));
	}
}

// The byte offset of a member within an object.
template<class T, class M>
size_t OffsetOf(const T& aObject, const M& aMember)
{
	return size_t(reinterpret_cast<const char*>(&aMember) - reinterpret_cast<const char*>(&aObject));
}
}

// The sizes of the structs the game copies into the probe's memory, or reads from it.
static_assert(sizeof(PxOverlapHit) == 32);
static_assert(sizeof(PxRaycastHit) == 64);
static_assert(sizeof(PxBoxGeometry) == 16);
static_assert(sizeof(PxSphereGeometry) == 8);
static_assert(sizeof(PxCapsuleGeometry) == 12);
static_assert(sizeof(PxGeometryHolder) == 48);
static_assert(sizeof(PxConvexMeshGeometry) == 48);
static_assert(sizeof(PxTriangleMeshGeometry) == 48);
static_assert(sizeof(PxHeightFieldGeometry) == 32);
static_assert(sizeof(PxHullPolygon) == 20);
static_assert(sizeof(PxHeightFieldSample) == 4);
static_assert(sizeof(PxTransform) == 28);
static_assert(sizeof(PxFilterData) == 16);

TEST(PhysicsCallsUseTheGamesSlots)
{
	Expect("PxPhysics::getTolerancesScale", SlotOf<PxPhysics>([](PxPhysics& a) { (void)&a.getTolerancesScale(); }), 4);
	Expect("PxPhysics::getNbScenes", SlotOf<PxPhysics>([](PxPhysics& a) { (void)a.getNbScenes(); }), 19);
	Expect("PxPhysics::getScenes", SlotOf<PxPhysics>([](PxPhysics& a) { (void)a.getScenes(nullptr, 0); }), 20);
}

TEST(SceneCallsUseTheGamesSlots)
{
	const PxActorTypeFlags rigid = PxActorTypeFlag::eRIGID_STATIC | PxActorTypeFlag::eRIGID_DYNAMIC;
	const PxBoxGeometry    box(1.0f, 1.0f, 1.0f);
	const PxTransform      pose(PxIdentity);
	PxOverlapBuffer        overlaps;
	PxRaycastBuffer        ray;
	Expect("PxScene::getFlags", SlotOf<PxScene>([](PxScene& a) { (void)a.getFlags(); }), 3);
	Expect("PxScene::getNbActors", SlotOf<PxScene>([&](PxScene& a) { (void)a.getNbActors(rigid); }), 18);
	Expect("PxScene::getActors", SlotOf<PxScene>([&](PxScene& a) { (void)a.getActors(rigid, nullptr, 0); }), 19);
	Expect("PxScene::getNbArticulations", SlotOf<PxScene>([](PxScene& a) { (void)a.getNbArticulations(); }), 22);
	Expect("PxScene::getNbAggregates", SlotOf<PxScene>([](PxScene& a) { (void)a.getNbAggregates(); }), 26);
	Expect("PxScene::getAggregates", SlotOf<PxScene>([](PxScene& a) { (void)a.getAggregates(nullptr, 0); }), 27);
	Expect("PxScene::getGravity", SlotOf<PxScene>([](PxScene& a) { (void)a.getGravity(); }), 67);
	Expect("PxScene::raycast", SlotOf<PxScene>([&](PxScene& a) { (void)a.raycast(PxVec3(0.0f), PxVec3(0.0f, 0.0f, -1.0f), 1.0f, ray); }), 94);
	Expect("PxScene::overlap", SlotOf<PxScene>([&](PxScene& a) { (void)a.overlap(box, pose, overlaps); }), 96);
	Expect("PxScene::lockRead", SlotOf<PxScene>([](PxScene& a) { a.lockRead(); }), 105);
	Expect("PxScene::unlockRead", SlotOf<PxScene>([](PxScene& a) { a.unlockRead(); }), 106);
}

TEST(ActorCallsUseTheGamesSlots)
{
	Expect("PxActor::getName", SlotOf<PxActor>([](PxActor& a) { (void)a.getName(); }), 8);
	Expect("PxActor::getWorldBounds", SlotOf<PxActor>([](PxActor& a) { (void)a.getWorldBounds(1.0f); }), 9);
	Expect("PxRigidActor::getGlobalPose", SlotOf<PxRigidActor>([](PxRigidActor& a) { (void)a.getGlobalPose(); }), 20);
	Expect("PxRigidActor::getNbShapes", SlotOf<PxRigidActor>([](PxRigidActor& a) { (void)a.getNbShapes(); }), 25);
	Expect("PxRigidActor::getShapes", SlotOf<PxRigidActor>([](PxRigidActor& a) { (void)a.getShapes(nullptr, 0); }), 26);
	Expect("PxRigidBody::getRigidBodyFlags", SlotOf<PxRigidBody>([](PxRigidBody& a) { (void)a.getRigidBodyFlags(); }), 47);
	Expect("PxAggregate::getNbActors", SlotOf<PxAggregate>([](PxAggregate& a) { (void)a.getNbActors(); }), 9);
}

TEST(ShapeCallsUseTheGamesSlots)
{
	// The game's PxShape has a reference-count getter at slot 6 that NVIDIA's public headers lack (see PxShape.h).
	PxBoxGeometry          box;
	PxSphereGeometry       sphere;
	PxCapsuleGeometry      capsule;
	PxConvexMeshGeometry   convex;
	PxTriangleMeshGeometry mesh;
	PxHeightFieldGeometry  field;
	Expect("PxShape::getGeometryType", SlotOf<PxShape>([](PxShape& a) { (void)a.getGeometryType(); }), 7);
	Expect("PxShape::getBoxGeometry", SlotOf<PxShape>([&](PxShape& a) { (void)a.getBoxGeometry(box); }), 10);
	Expect("PxShape::getSphereGeometry", SlotOf<PxShape>([&](PxShape& a) { (void)a.getSphereGeometry(sphere); }), 11);
	Expect("PxShape::getCapsuleGeometry", SlotOf<PxShape>([&](PxShape& a) { (void)a.getCapsuleGeometry(capsule); }), 12);
	Expect("PxShape::getConvexMeshGeometry", SlotOf<PxShape>([&](PxShape& a) { (void)a.getConvexMeshGeometry(convex); }), 14);
	Expect("PxShape::getTriangleMeshGeometry", SlotOf<PxShape>([&](PxShape& a) { (void)a.getTriangleMeshGeometry(mesh); }), 15);
	Expect("PxShape::getHeightFieldGeometry", SlotOf<PxShape>([&](PxShape& a) { (void)a.getHeightFieldGeometry(field); }), 16);
	Expect("PxShape::getLocalPose", SlotOf<PxShape>([](PxShape& a) { (void)a.getLocalPose(); }), 19);
	Expect("PxShape::getSimulationFilterData", SlotOf<PxShape>([](PxShape& a) { (void)a.getSimulationFilterData(); }), 21);
	Expect("PxShape::getQueryFilterData", SlotOf<PxShape>([](PxShape& a) { (void)a.getQueryFilterData(); }), 23);
	Expect("PxShape::getFlags", SlotOf<PxShape>([](PxShape& a) { (void)a.getFlags(); }), 34);
}

TEST(MeshCallsUseTheGamesSlots)
{
	PxHullPolygon polygon;
	Expect("PxConvexMesh::getNbVertices", SlotOf<PxConvexMesh>([](PxConvexMesh& a) { (void)a.getNbVertices(); }), 5);
	Expect("PxConvexMesh::getVertices", SlotOf<PxConvexMesh>([](PxConvexMesh& a) { (void)a.getVertices(); }), 6);
	Expect("PxConvexMesh::getIndexBuffer", SlotOf<PxConvexMesh>([](PxConvexMesh& a) { (void)a.getIndexBuffer(); }), 7);
	Expect("PxConvexMesh::getNbPolygons", SlotOf<PxConvexMesh>([](PxConvexMesh& a) { (void)a.getNbPolygons(); }), 8);
	Expect("PxConvexMesh::getPolygonData", SlotOf<PxConvexMesh>([&](PxConvexMesh& a) { (void)a.getPolygonData(0, polygon); }), 9);
	Expect("PxTriangleMesh::getNbVertices", SlotOf<PxTriangleMesh>([](PxTriangleMesh& a) { (void)a.getNbVertices(); }), 5);
	Expect("PxTriangleMesh::getVertices", SlotOf<PxTriangleMesh>([](PxTriangleMesh& a) { (void)a.getVertices(); }), 6);
	Expect("PxTriangleMesh::getNbTriangles", SlotOf<PxTriangleMesh>([](PxTriangleMesh& a) { (void)a.getNbTriangles(); }), 9);
	Expect("PxTriangleMesh::getTriangles", SlotOf<PxTriangleMesh>([](PxTriangleMesh& a) { (void)a.getTriangles(); }), 10);
	Expect("PxTriangleMesh::getTriangleMeshFlags",
	       SlotOf<PxTriangleMesh>([](PxTriangleMesh& a) { (void)a.getTriangleMeshFlags(); }), 11);
	Expect("PxHeightField::getNbRows", SlotOf<PxHeightField>([](PxHeightField& a) { (void)a.getNbRows(); }), 7);
	Expect("PxHeightField::getNbColumns", SlotOf<PxHeightField>([](PxHeightField& a) { (void)a.getNbColumns(); }), 8);
	Expect("PxHeightField::getSample", SlotOf<PxHeightField>([](PxHeightField& a) { (void)&a.getSample(1, 2); }), 19);
}

TEST(HitCallbacksHaveTheGamesLayout)
{
	// The game writes hits into these objects and calls processTouches (slot 0) and finalizeQuery (slot 1) on them.
	struct Overlaps final : PxHitCallback<PxOverlapHit>
	{
		Overlaps()
			: PxHitCallback<PxOverlapHit>(nullptr, 0)
		{
		}
		PxAgain processTouches(const PxOverlapHit*, PxU32) override
		{
			g_slot = 0;
			return true;
		}
		void finalizeQuery() override { g_slot = 1; }
	};
	Overlaps   overlaps;
	auto*      vtable = *reinterpret_cast<void* const* const*>(&overlaps);
	const auto touches = reinterpret_cast<PxAgain (*)(void*, const PxOverlapHit*, PxU32)>(vtable[0]);
	const auto finalize = reinterpret_cast<void (*)(void*)>(vtable[1]);
	g_slot = -1;
	touches(&overlaps, nullptr, 0);
	CHECK_EQ(g_slot, 0);
	finalize(&overlaps);
	CHECK_EQ(g_slot, 1);

	CHECK_EQ(OffsetOf(overlaps, overlaps.block), size_t(8));
	CHECK_EQ(OffsetOf(overlaps, overlaps.hasBlock), size_t(40));
	CHECK_EQ(OffsetOf(overlaps, overlaps.touches), size_t(48));
	CHECK_EQ(OffsetOf(overlaps, overlaps.maxNbTouches), size_t(56));
	CHECK_EQ(OffsetOf(overlaps, overlaps.nbTouches), size_t(60));

	PxRaycastBuffer ray;
	CHECK_EQ(OffsetOf(ray, ray.block), size_t(8));
	CHECK_EQ(OffsetOf(ray.block, ray.block.distance), size_t(52));
	CHECK_EQ(OffsetOf(ray, ray.hasBlock), size_t(72));
	CHECK_EQ(OffsetOf(ray, ray.touches), size_t(80));
	CHECK_EQ(OffsetOf(ray, ray.maxNbTouches), size_t(88));
	CHECK_EQ(OffsetOf(ray, ray.nbTouches), size_t(92));
}

TEST(ObjectFieldsHaveTheGamesLayout)
{
	// The probe reads two fields straight from game objects: PxBase's type ID and PxActor::userData.
	Fake                 object;
	const std::uint16_t type = PxConcreteType::eSHAPE;
	std::memcpy(object.body, &type, sizeof(type));  // offset 8
	CHECK_EQ(int(reinterpret_cast<const PxBase&>(object).getConcreteType()), int(PxConcreteType::eSHAPE));
	const auto& actor = reinterpret_cast<const PxActor&>(object);
	CHECK_EQ(OffsetOf(actor, actor.userData), size_t(16));
}

TEST(SharedStructsHaveTheGamesLayout)
{
	// What the game's getters copy into the probe's structs, byte for byte.
	PxConvexMeshGeometry convex;
	CHECK_EQ(OffsetOf(convex, convex.convexMesh), size_t(32));
	CHECK_EQ(OffsetOf(convex, convex.maxMargin), size_t(40));
	CHECK_EQ(OffsetOf(convex, convex.meshFlags), size_t(44));
	PxTriangleMeshGeometry mesh;
	CHECK_EQ(OffsetOf(mesh, mesh.meshFlags), size_t(32));
	CHECK_EQ(OffsetOf(mesh, mesh.triangleMesh), size_t(40));
	PxHeightFieldGeometry field;
	CHECK_EQ(OffsetOf(field, field.heightField), size_t(8));
	CHECK_EQ(OffsetOf(field, field.heightScale), size_t(16));
	CHECK_EQ(OffsetOf(field, field.heightFieldFlags), size_t(28));
	PxHullPolygon polygon;
	CHECK_EQ(OffsetOf(polygon, polygon.mNbVerts), size_t(16));
	CHECK_EQ(OffsetOf(polygon, polygon.mIndexBase), size_t(18));
	PxQueryFilterData filter;
	CHECK_EQ(OffsetOf(filter, filter.flags), size_t(16));
	PxTolerancesScale scale;
	CHECK_EQ(OffsetOf(scale, scale.length), size_t(0));
	CHECK_EQ(OffsetOf(scale, scale.speed), size_t(8));
}
```

- [ ] **Step 3: Edit `CMakeLists.txt`: add the test**

Replace:
```cmake
	tests/TestFileVersions.cpp
)
```
with:
```cmake
	tests/TestFileVersions.cpp
	tests/TestAudit.cpp
)
```

- [ ] **Step 4: Run the tests; exactly the `PxShape` slots fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release --target probe_tests -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | grep -E "FAIL|test\(s\)"
```
Expected: no lines from the build filter. Then 11 lines like
`FAIL ...TestAudit.cpp:62: PxShape::getGeometryType goes through slot 6; the game's is 7`, one per `PxShape` call and
each one slot low, then `FAIL ShapeCallsUseTheGamesSlots` and `50 test(s), 11 failure(s)`. Every other class, the hit
callbacks and the struct layouts already match.

- [ ] **Step 5: Edit `extern/physx-3.4.2/PhysX_3.4/Include/PxShape.h`: declare the game's extra function**

The header uses tabs; keep them. Replace:
```cpp
	virtual		void					acquireReference() = 0;
```
with:
```cpp
	virtual		void					acquireReference() = 0;

	/**
	\brief Returns the reference count of the shape.

	LOCAL CHANGE for Cyberpunk 2077, not in NVIDIA's public 3.4.2 headers: the game's PhysX build has this virtual
	function at slot 6, right after acquireReference(). Declaring it here puts every later PxShape method on the
	game's slot. See docs/spikes/2026-10-01-physx-abi-audit.md.
	*/
	virtual		PxU32					getReferenceCount() const = 0;
```

- [ ] **Step 6: Run the tests; they all pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | tail -1
```
Expected: no lines from the build filter, then `50 test(s), 0 failure(s)`

- [ ] **Step 7: Write `extern/physx-3.4.2/README.md`**

```markdown
# NVIDIA PhysX 3.4.2 public headers

Copied from https://github.com/NVIDIAGameWorks/PhysX-3.4 at commit 5e42a5f112351a223c19c17bb331e6c55037b8eb
("PhysX 3.4, APEX 1.4 patch release @25354359"): `PhysX_3.4/Include` and `PxShared/include` only. The licence is
BSD-3-Clause: see `LICENSE.md`, and the notice in each header.

One local change makes them match the PhysX that Cyberpunk 2077 2.31 ships (`PhysX3_x64.dll`, FileVersion string
3.4.2.0). `PhysX_3.4/Include/PxShape.h` declares `getReferenceCount()` right after `acquireReference()`: the game's
build has that virtual function there, and NVIDIA's public headers don't. Everything else is unmodified. See
`docs/spikes/2026-10-01-physx-abi-audit.md`.
```

- [ ] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add extern/physx-3.4.2 spikes/physx-probe \
  && git commit -q -m "physx-probe: match the game's PxShape vtable, pinned by a slot and layout test" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 17: Readable-memory checks

**Files:**
- Create: `src/physx/Readable.h`, `src/physx/Readable.cpp`
- Create: `tests/TestReadable.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test `tests/TestReadable.cpp`**

It uses real pages: readable, no access, a guard page, reserved but never committed.
```cpp
#include "Harness.h"

#include "physx/Readable.h"

#include <Windows.h>

#include <cstdint>

using cpmc::px::ReadableMemory;

namespace
{
size_t PageSize()
{
	SYSTEM_INFO system{};
	GetSystemInfo(&system);
	return system.dwPageSize;
}
}

TEST(ReadableMemoryAcceptsOrdinaryMemory)
{
	ReadableMemory memory;
	const int      values[16] = {};
	CHECK(memory.Range(values, sizeof(values)));
	CHECK(memory.Range(values, 0));  // nothing to read at a real address
}

TEST(ReadableMemoryRejectsPagesThatWouldFault)
{
	// Four pages: readable, no access, a guard page, and reserved but never committed.
	const size_t page = PageSize();
	auto*        base = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4 * page, MEM_RESERVE, PAGE_NOACCESS));
	CHECK(base != nullptr);
	if (!base)
	{
		return;
	}
	VirtualAlloc(base, page, MEM_COMMIT, PAGE_READWRITE);
	VirtualAlloc(base + page, page, MEM_COMMIT, PAGE_NOACCESS);
	VirtualAlloc(base + 2 * page, page, MEM_COMMIT, PAGE_READWRITE | PAGE_GUARD);
	ReadableMemory memory;
	CHECK(memory.Range(base, page));
	CHECK(!memory.Range(base + page, 1));
	CHECK(!memory.Range(base + 2 * page, 1));
	CHECK(!memory.Range(base + 3 * page, 1));
	CHECK(memory.Range(base + page - 8, 8));
	CHECK(!memory.Range(base + page - 8, 16));  // starts readable, runs into the no-access page
	VirtualFree(base, 0, MEM_RELEASE);
}

TEST(ReadableMemoryRejectsNullAndWrappingRanges)
{
	ReadableMemory memory;
	CHECK(!memory.Range(nullptr, 1));
	CHECK(!memory.Range(nullptr, 0));
	const int value = 0;
	CHECK(!memory.Range(&value, SIZE_MAX));
}

TEST(ReadableMemoryRemembersRegionsUntilCleared)
{
	const size_t page = PageSize();
	auto*        base = static_cast<uint8_t*>(VirtualAlloc(nullptr, page, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
	CHECK(base != nullptr);
	if (!base)
	{
		return;
	}
	ReadableMemory memory;
	CHECK(!memory.Range(base, 1));
	DWORD old = 0;
	VirtualProtect(base, page, PAGE_READONLY, &old);
	CHECK(!memory.Range(base, 1));  // still the remembered answer
	memory.Clear();
	CHECK(memory.Range(base, 1));
	VirtualFree(base, 0, MEM_RELEASE);
}
```

- [ ] **Step 2: Edit `CMakeLists.txt`: add the test**

Replace:
```cmake
	tests/TestAudit.cpp
)
```
with:
```cmake
	tests/TestAudit.cpp
	tests/TestReadable.cpp
)
```

- [ ] **Step 3: Build; it fails**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release --target probe_tests -- -m -v:minimal 2>&1 | grep -E " error " | head -3
```
Expected: `fatal error C1083: Cannot open include file: 'physx/Readable.h'`

- [ ] **Step 4: Write `src/physx/Readable.h`**

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>

namespace cpmc::px
{
// Answers "can these bytes be read without faulting?" from VirtualQuery, and remembers each region it has asked
// about. The game's memory changes over time, so use one per dump and Clear() it before the next.
class ReadableMemory
{
public:
	// True when every byte of [aAddress, aAddress + aSize) is committed, readable and not a guard page. A null or
	// wrapping range never is; an empty range at a non-null address always is.
	bool Range(const void* aAddress, size_t aSize);

	void Clear();

private:
	struct Region
	{
		uintptr_t end = 0;
		bool      readable = false;
	};

	// The region holding aAddress, from memory or from VirtualQuery. False if VirtualQuery fails.
	bool Find(uintptr_t aAddress, Region& aOut);

	std::map<uintptr_t, Region> regions_;  // by start address
};
}
```

- [ ] **Step 5: Write `src/physx/Readable.cpp`**

```cpp
#include "physx/Readable.h"

#include <Windows.h>

#include <iterator>

namespace cpmc::px
{
namespace
{
constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                            PAGE_EXECUTE_WRITECOPY;
}

bool ReadableMemory::Range(const void* aAddress, size_t aSize)
{
	const auto start = reinterpret_cast<uintptr_t>(aAddress);
	if (start == 0 || start + aSize < start)
	{
		return false;
	}
	const uintptr_t end = start + aSize;
	for (uintptr_t at = start; at < end;)
	{
		Region region;
		if (!Find(at, region) || !region.readable)
		{
			return false;
		}
		at = region.end;
	}
	return true;
}

void ReadableMemory::Clear()
{
	regions_.clear();
}

bool ReadableMemory::Find(uintptr_t aAddress, Region& aOut)
{
	auto next = regions_.upper_bound(aAddress);
	if (next != regions_.begin())
	{
		const auto& [start, region] = *std::prev(next);
		if (aAddress < region.end)
		{
			aOut = region;
			return true;
		}
	}
	MEMORY_BASIC_INFORMATION info{};
	if (VirtualQuery(reinterpret_cast<const void*>(aAddress), &info, sizeof(info)) == 0)
	{
		return false;
	}
	const auto base = reinterpret_cast<uintptr_t>(info.BaseAddress);
	aOut.end = base + info.RegionSize;
	aOut.readable = info.State == MEM_COMMIT && (info.Protect & kReadable) != 0 &&
	                (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0;
	regions_[base] = aOut;
	return true;
}
}
```

- [ ] **Step 6: Edit `CMakeLists.txt`: add `Readable.cpp` to `probe_core`**

Replace:
```cmake
	src/physx/Checks.cpp
```
with:
```cmake
	src/physx/Checks.cpp
	src/physx/Readable.cpp
```

- [ ] **Step 7: Run the tests; they all pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | tail -1
```
Expected: no lines from the build filter, then `54 test(s), 0 failure(s)`

- [ ] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/physx-probe \
  && git commit -q -m "physx-probe: readable-memory checks" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" \
  && git log --oneline -1
```

---

### Task 18: The audited build (`GameAbi`)

**Files:**
- Create: `src/physx/GameAbi.h`, `src/physx/GameAbi.cpp`
- Create: `tests/TestGameAbi.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test `tests/TestGameAbi.cpp`**

```cpp
#include "Harness.h"

#include "physx/GameAbi.h"

#include <Windows.h>

#include <Psapi.h>

#include <cstdint>

using namespace physx;
using namespace cpmc::px;

namespace
{
// Where the crashed run had the two DLLs.
constexpr ModuleBases kBases{ 0x7ffcb6820000, 0x7ffcb6ab0000 };

// A PhysX object's leading fields: the vtable pointer, the type ID, the base flags and, for actors, userData.
struct FakeObject
{
	uintptr_t     vtable = 0;
	std::uint16_t type = 0;
	std::uint16_t baseFlags = 0;
	std::uint32_t padding = 0;
	void*         userData = nullptr;
};
}

TEST(ModuleIdentityReadsThePeHeader)
{
	alignas(8) unsigned char image[0x200] = {};
	auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
	dos->e_magic = IMAGE_DOS_SIGNATURE;
	dos->e_lfanew = 0x80;
	auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(image + 0x80);
	nt->Signature = IMAGE_NT_SIGNATURE;
	nt->FileHeader.TimeDateStamp = 0x64c978b4;
	nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
	nt->OptionalHeader.SizeOfImage = 0x281000;
	const auto id = ModuleIdentity(image);
	CHECK(id && *id == kAuditedModules[0].id);
	nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
	CHECK(!ModuleIdentity(image));
	nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
	nt->Signature = 0;
	CHECK(!ModuleIdentity(image));
	dos->e_magic = 0;
	CHECK(!ModuleIdentity(image));
	CHECK(!ModuleIdentity(nullptr));
}

TEST(ModuleIdentityOfALoadedDll)
{
	// kernel32's image size, checked against GetModuleInformation.
	const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
	MODULEINFO    info{};
	CHECK(GetModuleInformation(GetCurrentProcess(), kernel32, &info, sizeof(info)));
	const auto id = ModuleIdentity(kernel32);
	CHECK(id && id->imageSize == info.SizeOfImage);
	CHECK(id && id->timestamp != 0);
}

TEST(AuditedClassesAreFoundByVtable)
{
	CHECK(AuditedClassOf(kBases.physx3 + 0x20a310, kBases) == AuditedClass::Physics);
	CHECK(AuditedClassOf(kBases.physx3 + 0x20caf0, kBases) == AuditedClass::Scene);
	CHECK(AuditedClassOf(kBases.physx3 + 0x20b980, kBases) == AuditedClass::RigidStatic);
	CHECK(AuditedClassOf(kBases.physx3 + 0x20b2e8, kBases) == AuditedClass::RigidDynamic);
	CHECK(AuditedClassOf(kBases.physx3 + 0x206e68, kBases) == AuditedClass::Aggregate);
	CHECK(AuditedClassOf(kBases.physx3 + 0x20e9a8, kBases) == AuditedClass::Shape);
	CHECK(AuditedClassOf(kBases.common + 0x1b5200, kBases) == AuditedClass::ConvexMesh);
	CHECK(AuditedClassOf(kBases.common + 0x1b6978, kBases) == AuditedClass::TriangleMeshBv33);
	CHECK(AuditedClassOf(kBases.common + 0x1b6868, kBases) == AuditedClass::TriangleMeshBv34);
	CHECK(AuditedClassOf(kBases.common + 0x1b6b18, kBases) == AuditedClass::HeightField);
	CHECK(!AuditedClassOf(kBases.physx3 + 0x20e9a8 + 8, kBases));  // one slot into NpShape's table
	CHECK(!AuditedClassOf(kBases.physx3 + 0x20e878, kBases));      // the abstract PxShape table
	CHECK(!AuditedClassOf(kBases.common + 0x20e9a8, kBases));      // the right RVA in the wrong DLL
	const ModuleBases moved{ 0x180000000, 0x190000000 };
	CHECK(AuditedClassOf(moved.physx3 + 0x20e9a8, moved) == AuditedClass::Shape);
	CHECK(!AuditedClassOf(kBases.physx3 + 0x20e9a8, moved));
	CHECK(!AuditedClassOf(0x20e9a8, ModuleBases{}));  // the DLLs aren't loaded
}

TEST(AuditedClassesKnowTheirTypeIdsAndNames)
{
	CHECK_EQ(ConcreteTypeOf(AuditedClass::Shape), int(PxConcreteType::eSHAPE));
	CHECK_EQ(ConcreteTypeOf(AuditedClass::TriangleMeshBv34), int(PxConcreteType::eTRIANGLE_MESH_BVH34));
	CHECK_EQ(ConcreteTypeOf(AuditedClass::Scene), -1);
	CHECK_STR(InterfaceName(AuditedClass::RigidStatic), "PxRigidStatic");
	CHECK_STR(InterfaceName(AuditedClass::TriangleMeshBv33), "PxBVH33TriangleMesh");
}

TEST(CheckObjectNeedsReadableMemoryAnAuditedVtableAndTheRightType)
{
	ReadableMemory    memory;
	FakeObject        shape{ kBases.physx3 + 0x20e9a8, std::uint16_t(PxConcreteType::eSHAPE) };
	const ObjectCheck check = CheckObject(memory, kBases, &shape);
	CHECK(check.verdict == ObjectVerdict::Ok && check.cls == AuditedClass::Shape);
	shape.type = std::uint16_t(PxConcreteType::eCONVEX_MESH);
	CHECK(CheckObject(memory, kBases, &shape).verdict == ObjectVerdict::WrongType);
	shape.vtable += 8;
	CHECK(CheckObject(memory, kBases, &shape).verdict == ObjectVerdict::Unaudited);
	CHECK(CheckObject(memory, kBases, nullptr).verdict == ObjectVerdict::Unreadable);
	FakeObject scene{ kBases.physx3 + 0x20caf0, 0xffff };  // not a PxBase object: no type ID to check
	CHECK(CheckObject(memory, kBases, &scene).verdict == ObjectVerdict::Ok);
}

TEST(CheckObjectCoversTheBytesTheCallerReads)
{
	// An actor whose first 16 bytes end a readable page: its userData (bytes 16 to 24) would fault.
	SYSTEM_INFO system{};
	GetSystemInfo(&system);
	const size_t page = system.dwPageSize;
	auto*        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, 2 * page, MEM_RESERVE, PAGE_NOACCESS));
	CHECK(base != nullptr);
	if (!base)
	{
		return;
	}
	VirtualAlloc(base, page, MEM_COMMIT, PAGE_READWRITE);
	auto* actor = reinterpret_cast<uintptr_t*>(base + page - 16);
	actor[0] = kBases.physx3 + 0x20b980;
	actor[1] = PxConcreteType::eRIGID_STATIC;
	ReadableMemory memory;
	CHECK(CheckObject(memory, kBases, actor).verdict == ObjectVerdict::Ok);
	CHECK(CheckObject(memory, kBases, actor, 24).verdict == ObjectVerdict::Unreadable);
	VirtualFree(base, 0, MEM_RELEASE);
}
```

- [ ] **Step 2: Edit `CMakeLists.txt`: add the test**

Replace:
```cmake
	tests/TestReadable.cpp
)
```
with:
```cmake
	tests/TestReadable.cpp
	tests/TestGameAbi.cpp
)
```

- [ ] **Step 3: Build; it fails**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release --target probe_tests -- -m -v:minimal 2>&1 | grep -E " error " | head -3
```
Expected: `fatal error C1083: Cannot open include file: 'physx/GameAbi.h'`

- [ ] **Step 4: Write `src/physx/GameAbi.h`**

The RVAs and identities come from `docs/spikes/2026-10-01-physx-abi-audit.md`.
```cpp
#pragma once

#include "physx/PxHeaders.h"
#include "physx/Readable.h"

#include <cstddef>
#include <cstdint>
#include <optional>

// The game's PhysX build as audited in docs/spikes/2026-10-01-physx-abi-audit.md: the two DLLs' identities and the
// vtables of every class the probe calls into. The probe calls nothing on an object whose vtable isn't one of these.
namespace cpmc::px
{
// A module's PE timestamp and image size.
struct ModuleId
{
	uint32_t timestamp = 0;
	uint32_t imageSize = 0;

	bool operator==(const ModuleId&) const = default;
};

struct AuditedModule
{
	const wchar_t* name;
	ModuleId       id;
};

// PhysX3_x64.dll, then PhysX3Common_x64.dll, as audited.
inline constexpr AuditedModule kAuditedModules[2] = {
	{ L"PhysX3_x64.dll", { 0x64c978b4, 0x281000 } },
	{ L"PhysX3Common_x64.dll", { 0x64c978a8, 0x1f9000 } },
};

// The identity in the PE header of the module loaded at aBase; nothing unless it is a 64-bit PE image.
std::optional<ModuleId> ModuleIdentity(const void* aBase);

// Where the two DLLs are loaded.
struct ModuleBases
{
	uintptr_t physx3 = 0;
	uintptr_t common = 0;
};

enum class AuditedClass
{
	Physics,
	Scene,
	RigidStatic,
	RigidDynamic,
	Aggregate,
	Shape,
	ConvexMesh,
	TriangleMeshBv33,
	TriangleMeshBv34,
	HeightField,
};

// The audited class whose vtable starts at aVtable, if any.
std::optional<AuditedClass> AuditedClassOf(uintptr_t aVtable, const ModuleBases& aBases);

// The PxConcreteType ID the class's objects report, or -1 for the two classes that aren't PxBase objects.
int ConcreteTypeOf(AuditedClass aClass);

// The PhysX interface name, for reports: "PxRigidStatic", "PxShape", "PxBVH34TriangleMesh" and so on.
const char* InterfaceName(AuditedClass aClass);

enum class ObjectVerdict
{
	Ok,          // readable, an audited vtable, and (for PxBase objects) the type ID that vtable's class reports
	Unreadable,  // the bytes the caller reads aren't all readable
	Unaudited,   // the vtable isn't an audited table
	WrongType,   // an audited vtable, but the type ID disagrees with it
};

struct ObjectCheck
{
	ObjectVerdict verdict = ObjectVerdict::Unreadable;
	AuditedClass  cls = AuditedClass::Physics;  // meaningful for Ok and WrongType
};

// Checks a PhysX object before the probe touches it. At least its first 16 bytes must be readable: the vtable
// pointer and, for PxBase objects, the type ID at offset 8. Pass a larger aBytes to cover fields the caller reads
// itself, such as an actor's userData, which ends at 24.
ObjectCheck CheckObject(ReadableMemory& aMemory, const ModuleBases& aBases, const void* aObject, size_t aBytes = 16);
}
```

- [ ] **Step 5: Write `src/physx/GameAbi.cpp`**

```cpp
#include "physx/GameAbi.h"

#include <Windows.h>

#include <algorithm>
#include <iterator>

using namespace physx;

namespace cpmc::px
{
namespace
{
struct AuditedVtable
{
	AuditedClass cls;
	bool         common;  // in PhysX3Common_x64.dll rather than PhysX3_x64.dll
	uintptr_t    rva;
	int          concreteType;
	const char*  name;
};

// Each audited class's primary vtable, from the audit.
constexpr AuditedVtable kVtables[] = {
	{ AuditedClass::Physics, false, 0x20a310, -1, "PxPhysics" },
	{ AuditedClass::Scene, false, 0x20caf0, -1, "PxScene" },
	{ AuditedClass::RigidStatic, false, 0x20b980, PxConcreteType::eRIGID_STATIC, "PxRigidStatic" },
	{ AuditedClass::RigidDynamic, false, 0x20b2e8, PxConcreteType::eRIGID_DYNAMIC, "PxRigidDynamic" },
	{ AuditedClass::Aggregate, false, 0x206e68, PxConcreteType::eAGGREGATE, "PxAggregate" },
	{ AuditedClass::Shape, false, 0x20e9a8, PxConcreteType::eSHAPE, "PxShape" },
	{ AuditedClass::ConvexMesh, true, 0x1b5200, PxConcreteType::eCONVEX_MESH, "PxConvexMesh" },
	{ AuditedClass::TriangleMeshBv33, true, 0x1b6978, PxConcreteType::eTRIANGLE_MESH_BVH33, "PxBVH33TriangleMesh" },
	{ AuditedClass::TriangleMeshBv34, true, 0x1b6868, PxConcreteType::eTRIANGLE_MESH_BVH34, "PxBVH34TriangleMesh" },
	{ AuditedClass::HeightField, true, 0x1b6b18, PxConcreteType::eHEIGHTFIELD, "PxHeightField" },
};

const AuditedVtable& Entry(AuditedClass aClass)
{
	return *std::find_if(std::begin(kVtables), std::end(kVtables), [&](const AuditedVtable& aEntry) { return aEntry.cls == aClass; });
}
}

std::optional<ModuleId> ModuleIdentity(const void* aBase)
{
	if (!aBase)
	{
		return std::nullopt;
	}
	const auto* dos = static_cast<const IMAGE_DOS_HEADER*>(aBase);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000 - LONG(sizeof(IMAGE_NT_HEADERS64)))
	{
		return std::nullopt;
	}
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(static_cast<const uint8_t*>(aBase) + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
	{
		return std::nullopt;
	}
	return ModuleId{ nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage };
}

std::optional<AuditedClass> AuditedClassOf(uintptr_t aVtable, const ModuleBases& aBases)
{
	for (const AuditedVtable& entry : kVtables)
	{
		const uintptr_t base = entry.common ? aBases.common : aBases.physx3;
		if (base != 0 && aVtable == base + entry.rva)
		{
			return entry.cls;
		}
	}
	return std::nullopt;
}

int ConcreteTypeOf(AuditedClass aClass)
{
	return Entry(aClass).concreteType;
}

const char* InterfaceName(AuditedClass aClass)
{
	return Entry(aClass).name;
}

ObjectCheck CheckObject(ReadableMemory& aMemory, const ModuleBases& aBases, const void* aObject, size_t aBytes)
{
	if (!aMemory.Range(aObject, std::max<size_t>(aBytes, 16)))
	{
		return { ObjectVerdict::Unreadable };
	}
	const std::optional<AuditedClass> cls = AuditedClassOf(*static_cast<const uintptr_t*>(aObject), aBases);
	if (!cls)
	{
		return { ObjectVerdict::Unaudited };
	}
	const int type = ConcreteTypeOf(*cls);
	if (type >= 0 && int(static_cast<const PxBase*>(aObject)->getConcreteType()) != type)
	{
		return { ObjectVerdict::WrongType, *cls };
	}
	return { ObjectVerdict::Ok, *cls };
}
}
```

- [ ] **Step 6: Edit `CMakeLists.txt`: add `GameAbi.cpp` to `probe_core`**

Replace:
```cmake
	src/physx/Readable.cpp
```
with:
```cmake
	src/physx/Readable.cpp
	src/physx/GameAbi.cpp
```

- [ ] **Step 7: Run the tests; they all pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | tail -1
```
Expected: no lines from the build filter, then `60 test(s), 0 failure(s)`

- [ ] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/physx-probe \
  && git commit -q -m "physx-probe: the audited PhysX build and per-object checks" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 19: The report for revision 2

**Files:**
- Modify: `tests/TestReportJson.cpp`, `src/Report.h`, `src/ReportJson.cpp`
- Modify: `src/Probe.cpp`, `src/Plugin.cpp` (one line each, so the build stays green until Tasks 20 and 21)

- [ ] **Step 1: Write the failing test `tests/TestReportJson.cpp`**

It expects version 0.2.0, the DLL identities, the skip counts, and no `faults`.
```cpp
#include "Harness.h"

#include "ReportJson.h"

namespace
{
bool Contains(const std::string& aText, const std::string& aPart)
{
	return aText.find(aPart) != std::string::npos;
}
}

TEST(ReportWithoutGeometryHasNulls)
{
	cpmc::DumpReport report;
	report.time = "2026-10-01T12:00:00";
	report.trigger = "request";
	report.runtime = "2.3.1";
	report.physxFileVersion = "3.4.2.0";
	report.modules.push_back({ "PhysX3_x64.dll", 0x64c978b4, 0x281000, true });
	report.modules.push_back({ "PhysX3Common_x64.dll", 0, 0, false });
	report.steps.push_back({ 1, "build", false, "PhysX3Common_x64.dll is not loaded" });
	const std::string json = cpmc::ReportToJson(report);
	CHECK(Contains(json, "\"probeVersion\": \"0.2.0\""));
	CHECK(Contains(json, "\"trigger\": \"request\""));
	CHECK(Contains(json, "\"runtime\": \"2.3.1\""));
	CHECK(Contains(json, "\"name\": \"PhysX3_x64.dll\""));
	CHECK(Contains(json, "\"timestamp\": \"0x64c978b4\""));
	CHECK(Contains(json, "\"imageSize\": \"0x281000\""));
	CHECK(Contains(json, "\"audited\": true"));
	CHECK(Contains(json, "\"audited\": false"));
	CHECK(Contains(json, "\"player\": null"));
	CHECK(Contains(json, "\"pass\": false"));
	CHECK(Contains(json, "\"detail\": \"PhysX3Common_x64.dll is not loaded\""));
	CHECK(Contains(json, "\"censusSkipped\": 0"));
	CHECK(Contains(json, "\"region\": null"));
	CHECK(Contains(json, "\"coordinateCheck\": null"));
	CHECK(Contains(json, "\"files\": {}"));
	CHECK(!Contains(json, "faults"));
}

TEST(ReportWithGeometryHasItsSections)
{
	cpmc::DumpReport report;
	cpmc::PlayerPose pose;
	pose.feet = { 1.0, 2.0, 3.0 };
	pose.forward = { 0.0f, 1.0f, 0.0f };
	pose.camera = cpmc::CameraPose{ { 1.0, 2.0, 4.7 }, { 0.0f, 1.0f, 0.0f }, 80.0f };
	report.player = pose;
	report.censusSkipped = 5;
	cpmc::SceneCensus scene;
	scene.gravity = { 0.0f, 0.0f, -9.81f };
	scene.flags = 0x1000;
	scene.requiresRwLock = true;
	scene.byConcreteType["PxRigidStatic"] = 7;
	scene.actorNameSamples = { "door \"A\"" };
	scene.skippedUnreadable = 2;
	scene.skippedUnaudited = 3;
	scene.mismatches = 1;
	report.scenes.push_back(scene);
	cpmc::RegionResult region;
	region.triangles = 42;
	region.trianglesByCategory["static_trimesh"] = 42;
	region.unreadable = 4;
	report.region = region;
	cpmc::CoordinateCheck check;
	check.rayHit = true;
	check.hitDistance = 1.75;
	check.pass = true;
	check.scene = 0;
	report.coordinateCheck = check;
	report.objWritten = true;
	const std::string json = cpmc::ReportToJson(report);
	CHECK(Contains(json, "\"fov\": 80"));
	CHECK(Contains(json, "\"censusSkipped\": 5"));
	CHECK(Contains(json, "\"flags\": \"0x1000\""));
	CHECK(Contains(json, "\"requiresRwLock\": true"));
	CHECK(Contains(json, "\"PxRigidStatic\": 7"));
	CHECK(Contains(json, "\"door \\\"A\\\"\""));
	CHECK(Contains(json, "\"unreadable\": 2"));
	CHECK(Contains(json, "\"unauditedVtable\": 3"));
	CHECK(Contains(json, "\"mismatches\": 1"));
	CHECK(Contains(json, "\"static_trimesh\": 42"));
	CHECK(Contains(json, "\"unreadable\": 4"));
	CHECK(Contains(json, "\"hitDistance\": 1.75"));
	CHECK(Contains(json, "\"nearestCapsule\": null"));
	CHECK(Contains(json, "\"obj\": \"collision.obj\""));
}
```

- [ ] **Step 2: Build; it fails**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release --target probe_tests -- -m -v:minimal 2>&1 | grep -E " error " | head -3
```
Expected: `error C2039: 'modules': is not a member of 'cpmc::DumpReport'` (and more like it)

- [ ] **Step 3: Write `src/Report.h`**

```cpp
#pragma once

#include "Coords.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Plain data for one dump's report.json (see the spec's report schema).
namespace cpmc
{
using Tally = std::map<std::string, uint64_t>;

struct CameraPose
{
	Vec3d position;
	Vec3f forward;
	float fov = 0.0f;
};

struct PlayerPose
{
	Vec3d feet;
	Vec3f forward;
	std::optional<CameraPose> camera;
};

// One of the game's PhysX DLLs, and whether it is the build the audit covers.
struct ModuleReport
{
	std::string name;
	uint32_t timestamp = 0;
	uint32_t imageSize = 0;
	bool audited = false;
};

struct CompatStep
{
	int step = 0;
	std::string name;
	bool pass = false;
	std::string detail;
};

struct SceneCensus
{
	int index = 0;
	Vec3f gravity;
	uint32_t flags = 0;
	bool requiresRwLock = false;
	uint32_t staticActors = 0;
	uint32_t dynamicActors = 0;
	uint32_t aggregates = 0;
	uint32_t actorsInAggregates = 0;
	uint32_t articulations = 0;
	Tally byConcreteType;
	Tally byGeometryType;
	Tally shapeFlags;
	Tally queryFilter;
	Tally simulationFilter;
	uint32_t namedActors = 0;
	uint32_t actorsWithUserData = 0;
	std::vector<std::string> actorNameSamples;
	uint64_t skippedUnreadable = 0;  // objects whose memory couldn't be read
	uint64_t skippedUnaudited = 0;   // objects whose vtable isn't an audited table
	uint64_t mismatches = 0;         // type IDs that disagree with the vtable, meshes that don't fit their shape
	double censusMs = 0.0;
};

struct RegionResult
{
	Vec3d center;
	Vec3f halfExtents;
	uint32_t boundsSelectedActors = 0;
	uint32_t overlapHits = 0;
	bool overlapOverflow = false;
	double selectMs = 0.0;
	double overlapMs = 0.0;
	double extractMs = 0.0;
	uint64_t triangles = 0;
	Tally trianglesByCategory;
	uint32_t skippedPlanes = 0;
	uint32_t unknownGeometry = 0;
	uint32_t badTriangles = 0;
	uint32_t unreadable = 0;  // shapes and meshes that failed their checks during extraction
	bool triangleCapHit = false;
};

struct NearestCapsule
{
	double distance = 0.0;
	float radius = 0.0f;
	float halfHeight = 0.0f;
	std::string kind;
};

struct CoordinateCheck
{
	bool rayHit = false;
	double hitDistance = 0.0;
	int scene = -1;
	bool pass = false;
	std::optional<NearestCapsule> nearestCapsule;
};

struct DumpReport
{
	std::string probeVersion = "0.2.0";
	std::string time;
	std::string trigger;
	std::string runtime;
	std::string physxFileVersion;
	std::vector<ModuleReport> modules;
	std::optional<PlayerPose> player;
	bool compatibilityPass = false;
	std::vector<CompatStep> steps;
	uint64_t censusSkipped = 0;
	uint64_t censusMismatches = 0;
	std::vector<SceneCensus> scenes;
	std::optional<RegionResult> region;
	std::optional<CoordinateCheck> coordinateCheck;
	double mainThreadPauseMs = 0.0;
	bool objWritten = false;
};
}
```

- [ ] **Step 4: Write `src/ReportJson.cpp`**

```cpp
#include "ReportJson.h"

#include "Json.h"

#include <format>

namespace cpmc
{
namespace
{
void Vec(JsonWriter& aJson, const Vec3d& aValue)
{
	aJson.BeginArray().Number(aValue.x).Number(aValue.y).Number(aValue.z).EndArray();
}

void Vec(JsonWriter& aJson, const Vec3f& aValue)
{
	aJson.BeginArray().Number(aValue.x).Number(aValue.y).Number(aValue.z).EndArray();
}

void Counts(JsonWriter& aJson, const Tally& aTally)
{
	aJson.BeginObject();
	for (const auto& [name, count] : aTally)
	{
		aJson.Key(name).UInt(count);
	}
	aJson.EndObject();
}

void Player(JsonWriter& aJson, const PlayerPose& aPose)
{
	aJson.BeginObject();
	aJson.Key("feet");
	Vec(aJson, aPose.feet);
	aJson.Key("forward");
	Vec(aJson, aPose.forward);
	aJson.Key("camera");
	if (aPose.camera)
	{
		aJson.BeginObject();
		aJson.Key("position");
		Vec(aJson, aPose.camera->position);
		aJson.Key("forward");
		Vec(aJson, aPose.camera->forward);
		aJson.Key("fov").Number(aPose.camera->fov);
		aJson.EndObject();
	}
	else
	{
		aJson.Null();
	}
	aJson.EndObject();
}

void Scene(JsonWriter& aJson, const SceneCensus& aScene)
{
	aJson.BeginObject();
	aJson.Key("index").Int(aScene.index);
	aJson.Key("gravity");
	Vec(aJson, aScene.gravity);
	aJson.Key("flags").String(std::format("0x{:x}", aScene.flags));
	aJson.Key("requiresRwLock").Bool(aScene.requiresRwLock);
	aJson.Key("staticActors").UInt(aScene.staticActors);
	aJson.Key("dynamicActors").UInt(aScene.dynamicActors);
	aJson.Key("aggregates").UInt(aScene.aggregates);
	aJson.Key("actorsInAggregates").UInt(aScene.actorsInAggregates);
	aJson.Key("articulations").UInt(aScene.articulations);
	aJson.Key("byConcreteType");
	Counts(aJson, aScene.byConcreteType);
	aJson.Key("byGeometryType");
	Counts(aJson, aScene.byGeometryType);
	aJson.Key("shapeFlags");
	Counts(aJson, aScene.shapeFlags);
	aJson.Key("queryFilter");
	Counts(aJson, aScene.queryFilter);
	aJson.Key("simulationFilter");
	Counts(aJson, aScene.simulationFilter);
	aJson.Key("namedActors").UInt(aScene.namedActors);
	aJson.Key("actorsWithUserData").UInt(aScene.actorsWithUserData);
	aJson.Key("actorNameSamples").BeginArray();
	for (const auto& name : aScene.actorNameSamples)
	{
		aJson.String(name);
	}
	aJson.EndArray();
	aJson.Key("skipped").BeginObject().Key("unreadable").UInt(aScene.skippedUnreadable);
	aJson.Key("unauditedVtable").UInt(aScene.skippedUnaudited).EndObject();
	aJson.Key("mismatches").UInt(aScene.mismatches);
	aJson.Key("censusMs").Number(aScene.censusMs);
	aJson.EndObject();
}

void Region(JsonWriter& aJson, const RegionResult& aRegion)
{
	aJson.BeginObject();
	aJson.Key("center");
	Vec(aJson, aRegion.center);
	aJson.Key("halfExtents");
	Vec(aJson, aRegion.halfExtents);
	aJson.Key("boundsSelectedActors").UInt(aRegion.boundsSelectedActors);
	aJson.Key("overlapHits").UInt(aRegion.overlapHits);
	aJson.Key("overlapOverflow").Bool(aRegion.overlapOverflow);
	aJson.Key("selectMs").Number(aRegion.selectMs);
	aJson.Key("overlapMs").Number(aRegion.overlapMs);
	aJson.Key("extractMs").Number(aRegion.extractMs);
	aJson.Key("triangles").UInt(aRegion.triangles);
	aJson.Key("trianglesByCategory");
	Counts(aJson, aRegion.trianglesByCategory);
	aJson.Key("skipped").BeginObject().Key("plane").UInt(aRegion.skippedPlanes);
	aJson.Key("unknownGeometry").UInt(aRegion.unknownGeometry);
	aJson.Key("badTriangles").UInt(aRegion.badTriangles);
	aJson.Key("unreadable").UInt(aRegion.unreadable).EndObject();
	aJson.Key("triangleCapHit").Bool(aRegion.triangleCapHit);
	aJson.EndObject();
}

void Coordinates(JsonWriter& aJson, const CoordinateCheck& aCheck)
{
	aJson.BeginObject();
	aJson.Key("rayHit").Bool(aCheck.rayHit);
	aJson.Key("hitDistance");
	aCheck.rayHit ? aJson.Number(aCheck.hitDistance) : aJson.Null();
	aJson.Key("scene").Int(aCheck.scene);
	aJson.Key("pass").Bool(aCheck.pass);
	aJson.Key("nearestCapsule");
	if (aCheck.nearestCapsule)
	{
		aJson.BeginObject();
		aJson.Key("distance").Number(aCheck.nearestCapsule->distance);
		aJson.Key("radius").Number(aCheck.nearestCapsule->radius);
		aJson.Key("halfHeight").Number(aCheck.nearestCapsule->halfHeight);
		aJson.Key("kind").String(aCheck.nearestCapsule->kind);
		aJson.EndObject();
	}
	else
	{
		aJson.Null();
	}
	aJson.EndObject();
}
}

std::string ReportToJson(const DumpReport& aReport)
{
	JsonWriter json;
	json.BeginObject();
	json.Key("probeVersion").String(aReport.probeVersion);
	json.Key("time").String(aReport.time);
	json.Key("trigger").String(aReport.trigger);
	json.Key("game").BeginObject();
	json.Key("runtime").String(aReport.runtime);
	json.Key("physxFileVersion").String(aReport.physxFileVersion);
	json.Key("modules").BeginArray();
	for (const ModuleReport& module : aReport.modules)
	{
		json.BeginObject();
		json.Key("name").String(module.name);
		json.Key("timestamp").String(std::format("0x{:x}", module.timestamp));
		json.Key("imageSize").String(std::format("0x{:x}", module.imageSize));
		json.Key("audited").Bool(module.audited);
		json.EndObject();
	}
	json.EndArray();
	json.EndObject();
	json.Key("player");
	aReport.player ? Player(json, *aReport.player) : void(json.Null());
	json.Key("compatibility").BeginObject();
	json.Key("pass").Bool(aReport.compatibilityPass);
	json.Key("steps").BeginArray();
	for (const CompatStep& step : aReport.steps)
	{
		json.BeginObject();
		json.Key("step").Int(step.step);
		json.Key("name").String(step.name);
		json.Key("pass").Bool(step.pass);
		json.Key("detail").String(step.detail);
		json.EndObject();
	}
	json.EndArray();
	json.Key("censusSkipped").UInt(aReport.censusSkipped);
	json.Key("censusMismatches").UInt(aReport.censusMismatches);
	json.EndObject();
	json.Key("scenes").BeginArray();
	for (const SceneCensus& scene : aReport.scenes)
	{
		Scene(json, scene);
	}
	json.EndArray();
	json.Key("region");
	aReport.region ? Region(json, *aReport.region) : void(json.Null());
	json.Key("coordinateCheck");
	aReport.coordinateCheck ? Coordinates(json, *aReport.coordinateCheck) : void(json.Null());
	json.Key("mainThreadPauseMs").Number(aReport.mainThreadPauseMs);
	json.Key("files").BeginObject();
	if (aReport.objWritten)
	{
		json.Key("obj").String("collision.obj");
		json.Key("mtl").String("collision.mtl");
	}
	json.EndObject();
	json.EndObject();
	return json.Str() + "\n";
}
}
```

- [ ] **Step 5: Edit `src/Probe.cpp`: drop the removed `faults` field**

Task 20 rewrites this file; this keeps it compiling until then. Replace:
```cpp
	report.coordinateCheck = check;
	report.faults = faults;
```
with:
```cpp
	report.coordinateCheck = check;
```

- [ ] **Step 6: Edit `src/Plugin.cpp`: log skipped objects instead of faults**

Replace:
```cpp
	Log("dump ({}): compatibility {}, {} scene(s), {} triangles, coordinate check {}, main-thread pause {:.1f} ms, {} fault(s)",
```
with:
```cpp
	Log("dump ({}): compatibility {}, {} scene(s), {} triangles, coordinate check {}, main-thread pause {:.1f} ms, {} object(s) skipped",
```
Then replace:
```cpp
	    report.faults);
```
with:
```cpp
	    report.censusSkipped);
```

- [ ] **Step 7: Run the tests; they all pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | tail -1
```
Expected: no lines from the build filter, then `60 test(s), 0 failure(s)`

- [ ] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/physx-probe \
  && git commit -q -m "physx-probe: report DLL identities and skipped objects, version 0.2.0" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 20: The PhysX session without a crash guard

Every object is checked before anything is called on it, every raw array before it is read, and nothing is caught.
This code runs only against the game, so it has no new unit tests. Tasks 16 to 18 test everything it relies on that can
be tested offline, and Task 22 tests the rest.

**Files:**
- Modify: `src/physx/PhysxSession.h`, `src/physx/PhysxSession.cpp`, `src/Probe.cpp` (rewritten)
- Modify: `src/physx/ShapeTriangles.h`, `src/physx/ShapeTriangles.cpp` (`AppendTriangles`)
- Modify: `src/physx/Checks.h`, `src/physx/Checks.cpp`, `tests/TestChecks.cpp` (two functions go)
- Modify: `CMakeLists.txt` (`/EHsc`, no `TestGuard.cpp`)
- Delete: `tests/TestGuard.cpp`

- [ ] **Step 1: Write `src/physx/PhysxSession.h`**

```cpp
#pragma once

#include "Report.h"
#include "physx/GameAbi.h"
#include "physx/PxHeaders.h"
#include "physx/Readable.h"
#include "physx/ShapeTriangles.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

// The only code that calls the game's PhysX. It catches no faults: the game's crash handler sees a fault before any
// handler of ours could, and freezes the game if we carry on. So it checks before it touches anything. It calls only
// the methods the audit verified, only on objects whose vtables are audited tables of the audited build, and it
// checks every raw array before reading it.
namespace cpmc::px
{
// A module's version resource: the binary FILEVERSION field and the "FileVersion" string. They can disagree:
// NVIDIA's PhysX 3.4.2 release says 3.4.1.0 in the binary field and 3.4.2.0 in the string. Empty if unreadable.
struct FileVersions
{
	std::string binary;
	std::string text;
};

// aModule is an HMODULE.
FileVersions ReadFileVersions(void* aModule);

// The compatibility check's result for one dump.
struct CheckResult
{
	bool                         pass = false;
	std::vector<CompatStep>      steps;
	std::string                  physxFileVersion;
	std::vector<ModuleReport>    modules;
	std::vector<physx::PxScene*> scenes;  // checked, once step 4 has passed
};

// Census and region results for one scene.
struct SceneDump
{
	SceneCensus                             census;
	uint32_t                                boundsSelected = 0;
	uint32_t                                overlapHits = 0;
	bool                                    overlapCapped = false;
	double                                  selectMs = 0.0;
	double                                  overlapMs = 0.0;
	double                                  extractMs = 0.0;
	std::map<std::string, std::vector<Tri>> triangles;  // by category
	AppendStats                             stats;
	std::optional<NearestCapsule>           nearestCapsule;
	bool                                    triangleCapHit = false;
};

class PhysxSession
{
public:
	// Starts a dump: forgets remembered memory regions, then runs the compatibility check. Steps 1-3 describe the
	// build and run once per session; steps 4-7 describe the world and run every time.
	CheckResult Check();

	// Census, region selection, overlap query and triangle extraction, all under the scene's read lock. aScene must
	// come from this dump's Check(). aFeet is V's feet (world), used for the nearest capsule. At most aTriangleBudget
	// triangles are kept. After 100 skipped objects the census stops and the region work is left out.
	SceneDump DumpScene(physx::PxScene* aScene, int aIndex, const physx::PxBounds3& aRegion, const physx::PxVec3& aFeet,
	                    uint64_t aTriangleBudget);

	// A ray straight down against static geometry. The distance to the nearest hit, if any.
	std::optional<float> RaycastDown(physx::PxScene* aScene, const physx::PxVec3& aFrom, float aMaxDistance);

private:
	struct Build
	{
		bool                      pass = false;
		std::vector<CompatStep>   steps;
		std::string               physxFileVersion;
		std::vector<ModuleReport> modules;
		ModuleBases               bases;
		physx::PxPhysics*         physics = nullptr;
	};

	// Steps 1-3.
	Build CheckBuild();

	std::optional<Build> build_;
	ReadableMemory       memory_;
};
}
```

- [ ] **Step 2: Write `src/physx/PhysxSession.cpp`**

```cpp
#include "physx/PhysxSession.h"

#include "physx/Checks.h"

#include <Windows.h>

#include <algorithm>
#include <format>
#include <initializer_list>
#include <set>
#include <string_view>
#include <utility>

using namespace physx;

namespace cpmc::px
{
FileVersions ReadFileVersions(void* aModule)
{
	FileVersions versions;
	wchar_t      path[MAX_PATH]{};
	if (!GetModuleFileNameW(static_cast<HMODULE>(aModule), path, MAX_PATH))
	{
		return versions;
	}
	DWORD       handle = 0;
	const DWORD size = GetFileVersionInfoSizeW(path, &handle);
	if (size == 0)
	{
		return versions;
	}
	std::vector<uint8_t> data(size);
	if (!GetFileVersionInfoW(path, 0, size, data.data()))
	{
		return versions;
	}
	VS_FIXEDFILEINFO* info = nullptr;
	UINT              length = 0;
	if (VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &length) && info)
	{
		versions.binary = std::format("{}.{}.{}.{}", HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
		                              HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
	}
	// The string table is keyed by the resource's first language and code page.
	struct Translation
	{
		WORD language;
		WORD codePage;
	};
	Translation* translations = nullptr;
	if (VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&translations), &length) &&
	    translations && length >= sizeof(Translation))
	{
		const std::wstring key =
		    std::format(L"\\StringFileInfo\\{:04x}{:04x}\\FileVersion", translations[0].language, translations[0].codePage);
		wchar_t* text = nullptr;
		UINT     textLength = 0;
		if (VerQueryValueW(data.data(), key.c_str(), reinterpret_cast<void**>(&text), &textLength) && text)
		{
			for (size_t i = 0; i < textLength && text[i] != L'\0'; ++i)
			{
				versions.text.push_back(text[i] < 0x80 ? char(text[i]) : '?');  // version strings are ASCII
			}
		}
	}
	return versions;
}

namespace
{
constexpr uint32_t kMaxScenes = 16;
constexpr uint32_t kMaxActorsPerScene = 10'000'000;
constexpr size_t   kSampleActors = 64;
constexpr uint32_t kMaxSampledActors = 10'000;
constexpr size_t   kMaxNameSamples = 32;
constexpr size_t   kMaxNameLength = 128;
constexpr size_t   kOverlapBuffer = 8192;
constexpr size_t   kOverlapCap = 1'000'000;
constexpr uint64_t kMaxSkipsPerScene = 100;
constexpr float    kCapsuleSearch = 1.5f;
constexpr size_t   kActorBytes = 24;  // the vtable pointer, the type ID and base flags, and userData

const PxActorTypeFlags kRigidActors = PxActorTypeFlag::eRIGID_STATIC | PxActorTypeFlag::eRIGID_DYNAMIC;

double NowMs()
{
	LARGE_INTEGER counter;
	LARGE_INTEGER frequency;
	QueryPerformanceCounter(&counter);
	QueryPerformanceFrequency(&frequency);
	return double(counter.QuadPart) * 1000.0 / double(frequency.QuadPart);
}

std::string Narrow(const wchar_t* aText)
{
	std::string out;
	for (; *aText; ++aText)
	{
		out.push_back(char(*aText));  // module names are ASCII
	}
	return out;
}

std::string Describe(const ObjectCheck& aCheck)
{
	switch (aCheck.verdict)
	{
	case ObjectVerdict::Ok:
		return std::format("a {} vtable", InterfaceName(aCheck.cls));
	case ObjectVerdict::Unreadable:
		return "not readable";
	case ObjectVerdict::Unaudited:
		return "a vtable the audit doesn't cover";
	case ObjectVerdict::WrongType:
		return std::format("a {} vtable but a different type ID", InterfaceName(aCheck.cls));
	}
	return {};
}

// What checking objects found, in a census or in the compatibility check's sample.
struct Findings
{
	uint64_t    objectsChecked = 0;
	uint64_t    unreadable = 0;
	uint64_t    unaudited = 0;
	uint64_t    typeMismatches = 0;      // a type ID that disagrees with the vtable, or the wrong class for the place
	uint64_t    geometryMismatches = 0;  // a shape whose mesh doesn't fit its geometry type
	std::string firstProblem;
	std::string firstGeometryMismatch;
	bool        seenTriangleMesh = false;
	bool        seenHeightField = false;
	bool        seenConvex = false;

	uint64_t Skipped() const { return unreadable + unaudited; }
	uint64_t Mismatches() const { return typeMismatches + geometryMismatches; }
};

// Checks game objects and memory for one dump.
struct Checker
{
	ReadableMemory&    memory;
	const ModuleBases& bases;

	// The object's audited class, if it passes CheckObject and is one of aAccepted. Counts what it finds either way.
	std::optional<AuditedClass> Accept(const void* aObject, std::initializer_list<AuditedClass> aAccepted, const char* aWhat,
	                                   Findings& aFindings, size_t aBytes = 16) const
	{
		++aFindings.objectsChecked;
		const ObjectCheck check = CheckObject(memory, bases, aObject, aBytes);
		const bool accepted = check.verdict == ObjectVerdict::Ok &&
		                      std::find(aAccepted.begin(), aAccepted.end(), check.cls) != aAccepted.end();
		if (accepted)
		{
			return check.cls;
		}
		switch (check.verdict)
		{
		case ObjectVerdict::Unreadable:
			++aFindings.unreadable;
			break;
		case ObjectVerdict::Unaudited:
			++aFindings.unaudited;
			break;
		default:
			++aFindings.typeMismatches;
			break;
		}
		if (aFindings.firstProblem.empty())
		{
			aFindings.firstProblem = std::format("{} at {} is {}", aWhat, aObject, Describe(check));
		}
		return std::nullopt;
	}

	// How many bytes from aText on can be read, up to aMax.
	size_t ReadableLength(const char* aText, size_t aMax) const
	{
		size_t length = 0;
		while (length < aMax && memory.Range(aText + length, 1))
		{
			++length;
		}
		return length;
	}
};

std::vector<PxShape*> ShapesOf(const PxRigidActor& aActor)
{
	std::vector<PxShape*> shapes(aActor.getNbShapes());
	shapes.resize(aActor.getShapes(shapes.data(), PxU32(shapes.size())));
	return shapes;
}

// The mesh object a mesh-based shape references; nullptr for primitives.
const PxBase* MeshOf(const PxShape& aShape, PxGeometryType::Enum aGeometry)
{
	switch (aGeometry)
	{
	case PxGeometryType::eTRIANGLEMESH:
	{
		PxTriangleMeshGeometry geometry;
		return aShape.getTriangleMeshGeometry(geometry) ? geometry.triangleMesh : nullptr;
	}
	case PxGeometryType::eCONVEXMESH:
	{
		PxConvexMeshGeometry geometry;
		return aShape.getConvexMeshGeometry(geometry) ? geometry.convexMesh : nullptr;
	}
	case PxGeometryType::eHEIGHTFIELD:
	{
		PxHeightFieldGeometry geometry;
		return aShape.getHeightFieldGeometry(geometry) ? geometry.heightField : nullptr;
	}
	default:
		return nullptr;
	}
}

// "static", "kinematic" or "dynamic". The audited class tells a static actor from a dynamic one.
const char* ActorKind(const PxActor& aActor, AuditedClass aClass)
{
	if (aClass == AuditedClass::RigidStatic)
	{
		return "static";
	}
	return static_cast<const PxRigidBody&>(aActor).getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC) ? "kinematic"
	                                                                                                       : "dynamic";
}

// Checks one actor, its shapes and their meshes; with aCensus set, it also fills the census tallies. Returns the
// actor's class if the actor itself passed its check.
std::optional<AuditedClass> VisitActor(const Checker& aChecker, const PxActor* aActor, Findings& aFindings,
                                       SceneCensus* aCensus, std::set<std::string>* aNames)
{
	const std::optional<AuditedClass> actorClass =
	    aChecker.Accept(aActor, { AuditedClass::RigidStatic, AuditedClass::RigidDynamic }, "an actor", aFindings, kActorBytes);
	if (!actorClass)
	{
		return std::nullopt;
	}
	if (aCensus)
	{
		++aCensus->byConcreteType[InterfaceName(*actorClass)];
		if (const char* name = aActor->getName())
		{
			++aCensus->namedActors;
			if (aNames && aNames->size() < kMaxNameSamples)
			{
				aNames->insert(SanitizeName(name, aChecker.ReadableLength(name, kMaxNameLength)));
			}
		}
		if (aActor->userData)
		{
			++aCensus->actorsWithUserData;
		}
	}
	for (const PxShape* shape : ShapesOf(static_cast<const PxRigidActor&>(*aActor)))
	{
		if (!aChecker.Accept(shape, { AuditedClass::Shape }, "a shape", aFindings))
		{
			continue;
		}
		const PxGeometryType::Enum geometry = shape->getGeometryType();
		if (aCensus)
		{
			++aCensus->byGeometryType[GeometryName(geometry)];
			const PxShapeFlags flags = shape->getFlags();
			if (flags.isSet(PxShapeFlag::eSIMULATION_SHAPE))
			{
				++aCensus->shapeFlags["simulation"];
			}
			if (flags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE))
			{
				++aCensus->shapeFlags["sceneQuery"];
			}
			if (flags.isSet(PxShapeFlag::eTRIGGER_SHAPE))
			{
				++aCensus->shapeFlags["trigger"];
			}
			++aCensus->queryFilter[FilterKey(shape->getQueryFilterData())];
			++aCensus->simulationFilter[FilterKey(shape->getSimulationFilterData())];
		}
		const PxBase* mesh = MeshOf(*shape, geometry);
		if (!mesh)
		{
			continue;
		}
		const std::optional<AuditedClass> meshClass =
		    aChecker.Accept(mesh,
		                    { AuditedClass::ConvexMesh, AuditedClass::TriangleMeshBv33, AuditedClass::TriangleMeshBv34,
		                      AuditedClass::HeightField },
		                    "a mesh", aFindings);
		if (!meshClass)
		{
			continue;
		}
		if (!GeometryMatchesMesh(geometry, ConcreteTypeOf(*meshClass)))
		{
			++aFindings.geometryMismatches;
			if (aFindings.firstGeometryMismatch.empty())
			{
				aFindings.firstGeometryMismatch =
				    std::format("a {} shape references a {}", GeometryName(geometry), InterfaceName(*meshClass));
			}
		}
		aFindings.seenTriangleMesh |= geometry == PxGeometryType::eTRIANGLEMESH;
		aFindings.seenHeightField |= geometry == PxGeometryType::eHEIGHTFIELD;
		aFindings.seenConvex |= geometry == PxGeometryType::eCONVEXMESH;
	}
	return actorClass;
}

// Collects every overlap hit: PhysX hands over each full buffer, and the last partial one, via processTouches.
struct CollectOverlaps final : PxHitCallback<PxOverlapHit>
{
	CollectOverlaps(PxOverlapHit* aBuffer, PxU32 aSize, std::vector<PxOverlapHit>& aOut)
		: PxHitCallback<PxOverlapHit>(aBuffer, aSize)
		, out(aOut)
	{
	}

	PxAgain processTouches(const PxOverlapHit* aHits, PxU32 aCount) override
	{
		out.insert(out.end(), aHits, aHits + aCount);
		if (out.size() >= kOverlapCap)
		{
			capped = true;
			return false;
		}
		return true;
	}

	std::vector<PxOverlapHit>& out;
	bool                       capped = false;
};

// Keeps the capsule nearest to V's feet (distance to its surface), if within kCapsuleSearch.
void NoteCapsule(const PxShape& aShape, const PxTransform& aActorPose, const PxVec3& aFeet, const char* aKind,
                 std::optional<NearestCapsule>& aNearest)
{
	PxCapsuleGeometry geometry;
	if (!aShape.getCapsuleGeometry(geometry))
	{
		return;
	}
	const PxTransform pose = aActorPose * aShape.getLocalPose();
	const PxVec3      axis = pose.q.rotate(PxVec3(geometry.halfHeight, 0.0f, 0.0f));  // capsules lie along local X
	const PxVec3      a = pose.p - axis;
	const PxVec3      ab = axis * 2.0f;
	const float       lengthSquared = ab.magnitudeSquared();
	const float       t = lengthSquared > 0.0f ? std::clamp((aFeet - a).dot(ab) / lengthSquared, 0.0f, 1.0f) : 0.0f;
	const float       distance = std::max(0.0f, (a + ab * t - aFeet).magnitude() - geometry.radius);
	if (distance <= kCapsuleSearch && (!aNearest || distance < aNearest->distance))
	{
		aNearest = NearestCapsule{ distance, geometry.radius, geometry.halfHeight, aKind };
	}
}

uint64_t CountTriangles(const std::map<std::string, std::vector<Tri>>& aTriangles)
{
	uint64_t count = 0;
	for (const auto& [category, triangles] : aTriangles)
	{
		count += triangles.size();
	}
	return count;
}

// Drops triangles beyond aBudget, starting from the last category.
void TrimToBudget(std::map<std::string, std::vector<Tri>>& aTriangles, uint64_t aBudget)
{
	uint64_t excess = CountTriangles(aTriangles);
	excess = excess > aBudget ? excess - aBudget : 0;
	for (auto it = aTriangles.rbegin(); it != aTriangles.rend() && excess > 0; ++it)
	{
		const uint64_t drop = std::min<uint64_t>(excess, it->second.size());
		it->second.resize(it->second.size() - drop);
		excess -= drop;
	}
}
}

PhysxSession::Build PhysxSession::CheckBuild()
{
	Build      build;
	const auto step = [&](int aNumber, const char* aName, bool aPass, std::string aDetail) {
		build.steps.push_back({ aNumber, aName, aPass, std::move(aDetail) });
		return aPass;
	};

	// 1. The build: both DLLs loaded, with the audited timestamps and image sizes.
	HMODULE     modules[2] = {};
	bool        audited = true;
	std::string detail;
	for (size_t i = 0; i < 2; ++i)
	{
		const AuditedModule& expected = kAuditedModules[i];
		modules[i] = GetModuleHandleW(expected.name);
		ModuleReport report;
		report.name = Narrow(expected.name);
		const std::optional<ModuleId> id = modules[i] ? ModuleIdentity(modules[i]) : std::nullopt;
		if (id)
		{
			report.timestamp = id->timestamp;
			report.imageSize = id->imageSize;
			report.audited = *id == expected.id;
		}
		audited &= report.audited;
		detail += std::format("{}{} ", detail.empty() ? "" : "; ", report.name);
		detail += !modules[i] ? std::string("is not loaded")
		          : !id       ? std::string("has no readable PE header")
		                      : std::format("timestamp 0x{:x}, image size 0x{:x}{}", report.timestamp, report.imageSize,
		                                    report.audited ? "" : ": not the audited build");
		build.modules.push_back(std::move(report));
	}
	const FileVersions versions = modules[0] ? ReadFileVersions(modules[0]) : FileVersions{};
	build.physxFileVersion = versions.text;
	if (modules[0])
	{
		detail += std::format("; PhysX3_x64.dll FileVersion \"{}\" (binary field {})", versions.text, versions.binary);
	}
	if (!step(1, "build", audited, detail))
	{
		return build;
	}
	build.bases = { reinterpret_cast<uintptr_t>(modules[0]), reinterpret_cast<uintptr_t>(modules[1]) };

	// 2. The physics object. PxGetPhysics is the only function called before this check.
	using GetPhysicsFn = PxPhysics& (*)();
	const auto        getPhysics = reinterpret_cast<GetPhysicsFn>(GetProcAddress(modules[0], "PxGetPhysics"));
	PxPhysics*        physics = getPhysics ? &getPhysics() : nullptr;
	const ObjectCheck check = CheckObject(memory_, build.bases, physics);
	if (!step(2, "physics", check.verdict == ObjectVerdict::Ok && check.cls == AuditedClass::Physics,
	          getPhysics ? std::format("PxPhysics at {}: {}", static_cast<const void*>(physics), Describe(check))
	                     : std::string("PxGetPhysics isn't exported")))
	{
		return build;
	}

	// 3. Tolerances.
	const PxTolerancesScale& scale = physics->getTolerancesScale();
	const bool               readable = memory_.Range(&scale, sizeof(scale));
	if (!step(3, "tolerances", readable && TolerancesPlausible(scale.length, scale.speed),
	          readable ? std::format("length {}, speed {}", scale.length, scale.speed)
	                   : std::string("the tolerances aren't readable")))
	{
		return build;
	}
	build.physics = physics;
	build.pass = true;
	return build;
}

CheckResult PhysxSession::Check()
{
	memory_.Clear();
	if (!build_)
	{
		build_ = CheckBuild();
	}
	CheckResult result;
	result.steps = build_->steps;
	result.physxFileVersion = build_->physxFileVersion;
	result.modules = build_->modules;
	if (!build_->pass)
	{
		return result;
	}
	const auto step = [&](int aNumber, const char* aName, bool aPass, std::string aDetail) {
		result.steps.push_back({ aNumber, aName, aPass, std::move(aDetail) });
		return aPass;
	};
	PxPhysics&    physics = *build_->physics;
	const Checker checker{ memory_, build_->bases };

	// 4. Scenes.
	const PxU32           count = physics.getNbScenes();
	std::vector<PxScene*> scenes;
	if (count >= 1 && count <= kMaxScenes)
	{
		scenes.resize(count);
		scenes.resize(physics.getScenes(scenes.data(), count));
	}
	std::string sceneProblem;
	for (PxScene* scene : scenes)
	{
		const ObjectCheck check = CheckObject(memory_, build_->bases, scene);
		if (sceneProblem.empty() && (check.verdict != ObjectVerdict::Ok || check.cls != AuditedClass::Scene))
		{
			sceneProblem = std::format("; the scene at {} is {}", static_cast<const void*>(scene), Describe(check));
		}
	}
	const bool scenesOk = count >= 1 && count <= kMaxScenes && scenes.size() == count && sceneProblem.empty();
	if (!step(4, "scenes", scenesOk, std::format("{} scene(s){}", count, sceneProblem)))
	{
		return result;
	}
	result.scenes = scenes;

	// 5. Gravity and counts.
	bool        allSane = true;
	bool        anyEarthLike = false;
	std::string factsDetail;
	for (PxScene* scene : scenes)
	{
		const PxVec3 gravity = scene->getGravity();
		const PxU32  actors = scene->getNbActors(kRigidActors);
		allSane &= GravityFinite(gravity) && actors <= kMaxActorsPerScene;
		anyEarthLike |= GravityEarthLike(gravity);
		factsDetail += std::format("{}gravity ({:.2f}, {:.2f}, {:.2f}), {} actors", factsDetail.empty() ? "" : "; ", gravity.x,
		                           gravity.y, gravity.z, actors);
	}
	if (!step(5, "gravity and counts", allSane && anyEarthLike, factsDetail))
	{
		return result;
	}

	// 6 and 7. Sampled actors, their shapes and their meshes; then each shape's geometry against its mesh.
	Findings findings;
	for (PxScene* scene : scenes)
	{
		scene->lockRead();
		std::vector<PxActor*> actors(std::min<PxU32>(scene->getNbActors(kRigidActors), kMaxSampledActors));
		actors.resize(scene->getActors(kRigidActors, actors.data(), PxU32(actors.size())));
		for (size_t i = 0; i < actors.size(); ++i)
		{
			const bool seenAll = findings.seenTriangleMesh && findings.seenHeightField && findings.seenConvex;
			if ((i >= kSampleActors && seenAll) || findings.Skipped() + findings.typeMismatches > 0)
			{
				break;
			}
			VisitActor(checker, actors[i], findings, nullptr, nullptr);
		}
		scene->unlockRead();
	}
	const bool objectsOk = findings.objectsChecked > 0 && findings.Skipped() + findings.typeMismatches == 0;
	if (!step(6, "objects", objectsOk,
	          objectsOk ? std::format("checked {} objects; saw a triangle mesh: {}, a heightfield: {}, a convex mesh: {}",
	                                  findings.objectsChecked, findings.seenTriangleMesh, findings.seenHeightField,
	                                  findings.seenConvex)
	                    : findings.firstProblem))
	{
		return result;
	}
	step(7, "geometry and meshes", findings.geometryMismatches == 0,
	     findings.geometryMismatches ? findings.firstGeometryMismatch : std::string("every checked mesh matches its shape"));
	result.pass = result.steps.size() == 7 &&
	              std::all_of(result.steps.begin(), result.steps.end(), [](const CompatStep& aStep) { return aStep.pass; });
	return result;
}

SceneDump PhysxSession::DumpScene(PxScene* aScene, int aIndex, const PxBounds3& aRegion, const PxVec3& aFeet,
                                  uint64_t aTriangleBudget)
{
	SceneDump     dump;
	SceneCensus&  census = dump.census;
	const Checker checker{ memory_, build_->bases };
	census.index = aIndex;
	aScene->lockRead();

	// Scene facts and the actor list.
	double       t0 = NowMs();
	const PxVec3 gravity = aScene->getGravity();
	census.gravity = { gravity.x, gravity.y, gravity.z };
	const PxSceneFlags flags = aScene->getFlags();
	census.flags = static_cast<uint32_t>(flags);
	census.requiresRwLock = flags.isSet(PxSceneFlag::eREQUIRE_RW_LOCK);
	census.staticActors = aScene->getNbActors(PxActorTypeFlag::eRIGID_STATIC);
	census.dynamicActors = aScene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC);
	census.articulations = aScene->getNbArticulations();
	std::vector<PxActor*> actors(std::min<PxU32>(aScene->getNbActors(kRigidActors), kMaxActorsPerScene));
	actors.resize(aScene->getActors(kRigidActors, actors.data(), PxU32(actors.size())));
	std::vector<PxAggregate*> aggregates(aScene->getNbAggregates());
	aggregates.resize(aScene->getAggregates(aggregates.data(), PxU32(aggregates.size())));
	census.aggregates = uint32_t(aggregates.size());

	// The census: every aggregate, actor, shape and mesh, each checked before anything is called on it.
	Findings findings;
	for (const PxAggregate* aggregate : aggregates)
	{
		if (checker.Accept(aggregate, { AuditedClass::Aggregate }, "an aggregate", findings))
		{
			census.actorsInAggregates += aggregate->getNbActors();
		}
	}
	std::set<std::string>                                names;
	std::vector<std::pair<const PxActor*, AuditedClass>> usable;
	bool                                                 abandoned = false;
	for (const PxActor* actor : actors)
	{
		if (findings.Skipped() >= kMaxSkipsPerScene)
		{
			abandoned = true;
			break;
		}
		if (const std::optional<AuditedClass> cls = VisitActor(checker, actor, findings, &census, &names))
		{
			usable.emplace_back(actor, *cls);
		}
	}
	census.skippedUnreadable = findings.unreadable;
	census.skippedUnaudited = findings.unaudited;
	census.mismatches = findings.Mismatches();
	census.actorNameSamples.assign(names.begin(), names.end());
	census.censusMs = NowMs() - t0;
	if (abandoned)
	{
		aScene->unlockRead();
		return dump;
	}

	// Region selection by actor bounds.
	t0 = NowMs();
	std::vector<std::pair<const PxActor*, AuditedClass>> selected;
	for (const auto& [actor, cls] : usable)
	{
		if (actor->getWorldBounds(1.0f).intersects(aRegion))
		{
			selected.emplace_back(actor, cls);
		}
	}
	dump.boundsSelected = uint32_t(selected.size());
	dump.selectMs = NowMs() - t0;

	// The same region through PhysX's own overlap query (scene-query shapes only), for comparison.
	t0 = NowMs();
	std::vector<PxOverlapHit> buffer(kOverlapBuffer);
	std::vector<PxOverlapHit> hits;
	CollectOverlaps           collect(buffer.data(), PxU32(buffer.size()), hits);
	aScene->overlap(PxBoxGeometry(aRegion.getExtents()), PxTransform(aRegion.getCenter()), collect,
	                PxQueryFilterData(PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::eNO_BLOCK));
	dump.overlapHits = uint32_t(hits.size());
	dump.overlapCapped = collect.capped;
	dump.overlapMs = NowMs() - t0;

	// Triangles of every shape of the selected actors.
	t0 = NowMs();
	for (const auto& [actor, cls] : selected)
	{
		if (CountTriangles(dump.triangles) >= aTriangleBudget)
		{
			dump.triangleCapHit = true;
			break;
		}
		const char*        kind = ActorKind(*actor, cls);
		const auto&        rigid = static_cast<const PxRigidActor&>(*actor);
		const PxTransform  pose = rigid.getGlobalPose();
		for (const PxShape* shape : ShapesOf(rigid))
		{
			const ObjectCheck check = CheckObject(memory_, build_->bases, shape);
			if (check.verdict != ObjectVerdict::Ok || check.cls != AuditedClass::Shape)
			{
				++dump.stats.unreadable;
				continue;
			}
			const PxGeometryType::Enum geometry = shape->getGeometryType();
			AppendTriangles(*shape, pose, aRegion, memory_, build_->bases,
			                dump.triangles[Category(kind, shape->getFlags(), geometry)], dump.stats);
			if (geometry == PxGeometryType::eCAPSULE && std::string_view(kind) != "static")
			{
				NoteCapsule(*shape, pose, aFeet, kind, dump.nearestCapsule);
			}
		}
	}
	if (CountTriangles(dump.triangles) > aTriangleBudget)
	{
		TrimToBudget(dump.triangles, aTriangleBudget);
		dump.triangleCapHit = true;
	}
	std::erase_if(dump.triangles, [](const auto& aEntry) { return aEntry.second.empty(); });
	dump.extractMs = NowMs() - t0;

	aScene->unlockRead();
	return dump;
}

std::optional<float> PhysxSession::RaycastDown(PxScene* aScene, const PxVec3& aFrom, float aMaxDistance)
{
	aScene->lockRead();
	PxRaycastBuffer buffer;
	aScene->raycast(aFrom, PxVec3(0.0f, 0.0f, -1.0f), aMaxDistance, buffer, PxHitFlag::eDEFAULT,
	                PxQueryFilterData(PxQueryFlag::eSTATIC));
	aScene->unlockRead();
	return buffer.hasBlock ? std::optional<float>(buffer.block.distance) : std::nullopt;
}
}
```

- [ ] **Step 3: Edit `src/physx/ShapeTriangles.h`: `AppendTriangles` takes the checks**

Replace:
```cpp
#include "physx/PxHeaders.h"
```
with:
```cpp
#include "physx/GameAbi.h"
#include "physx/PxHeaders.h"
#include "physx/Readable.h"
```
Then replace:
```cpp
	uint32_t badTriangles = 0;
};
```
with:
```cpp
	uint32_t badTriangles = 0;
	uint32_t unreadable = 0;  // meshes that failed their checks
};
```
Then replace:
```cpp
// World-space triangles of a live shape that touch aRegion. This calls into the game's PhysX, so run it under
// a crash guard (see RunGuarded in PhysxSession.h).
void AppendTriangles(const physx::PxShape& aShape, const physx::PxTransform& aActorPose, const physx::PxBounds3& aRegion,
                     std::vector<Tri>& aOut, AppendStats& aStats);
```
with:
```cpp
// World-space triangles of a live shape that touch aRegion. The caller has checked the shape (see CheckObject);
// this checks the shape's mesh the same way, and checks every array it reads for readability first. A mesh that fails
// is skipped and counted in aStats.unreadable.
void AppendTriangles(const physx::PxShape& aShape, const physx::PxTransform& aActorPose, const physx::PxBounds3& aRegion,
                     ReadableMemory& aMemory, const ModuleBases& aBases, std::vector<Tri>& aOut, AppendStats& aStats);
```

- [ ] **Step 4: Edit `src/physx/ShapeTriangles.cpp`: check meshes and arrays in `AppendTriangles`**

Replace the whole function, which is the last one in the file:
```cpp
void AppendTriangles(const PxShape& aShape, const PxTransform& aActorPose, const PxBounds3& aRegion,
                     std::vector<Tri>& aOut, AppendStats& aStats)
{
	const PxTransform pose = aActorPose * aShape.getLocalPose();
	std::vector<Tri>  small;
	switch (aShape.getGeometryType())
	{
	case PxGeometryType::eBOX:
	{
		PxBoxGeometry geometry;
		if (aShape.getBoxGeometry(geometry))
		{
			BoxTriangles(geometry.halfExtents, pose, small);
		}
		break;
	}
	case PxGeometryType::eSPHERE:
	{
		PxSphereGeometry geometry;
		if (aShape.getSphereGeometry(geometry))
		{
			SphereTriangles(geometry.radius, pose, small);
		}
		break;
	}
	case PxGeometryType::eCAPSULE:
	{
		PxCapsuleGeometry geometry;
		if (aShape.getCapsuleGeometry(geometry))
		{
			CapsuleTriangles(geometry.radius, geometry.halfHeight, pose, small);
		}
		break;
	}
	case PxGeometryType::ePLANE:
		++aStats.planes;
		break;
	case PxGeometryType::eCONVEXMESH:
	{
		PxConvexMeshGeometry geometry;
		if (aShape.getConvexMeshGeometry(geometry) && geometry.convexMesh)
		{
			const PxConvexMesh&        mesh = *geometry.convexMesh;
			std::vector<PxHullPolygon> polygons(mesh.getNbPolygons());
			for (PxU32 i = 0; i < PxU32(polygons.size()); ++i)
			{
				mesh.getPolygonData(i, polygons[i]);
			}
			ConvexPolygonTriangles(mesh.getVertices(), mesh.getNbVertices(), mesh.getIndexBuffer(), polygons.data(),
			                       PxU32(polygons.size()), geometry.scale, pose, small);
		}
		break;
	}
	case PxGeometryType::eTRIANGLEMESH:
	{
		PxTriangleMeshGeometry geometry;
		if (aShape.getTriangleMeshGeometry(geometry) && geometry.triangleMesh)
		{
			const PxTriangleMesh& mesh = *geometry.triangleMesh;
			MeshTriangles(mesh.getVertices(), mesh.getNbVertices(), mesh.getTriangles(),
			              mesh.getTriangleMeshFlags().isSet(PxTriangleMeshFlag::e16_BIT_INDICES), mesh.getNbTriangles(),
			              geometry.scale, pose, aRegion, aOut, aStats.badTriangles);
		}
		break;
	}
	case PxGeometryType::eHEIGHTFIELD:
	{
		PxHeightFieldGeometry geometry;
		if (aShape.getHeightFieldGeometry(geometry) && geometry.heightField)
		{
			const PxHeightField& field = *geometry.heightField;
			HeightfieldView      view;
			view.rows = field.getNbRows();
			view.columns = field.getNbColumns();
			view.heightScale = geometry.heightScale;
			view.rowScale = geometry.rowScale;
			view.columnScale = geometry.columnScale;
			view.sample = [&field](uint32_t aRow, uint32_t aColumn) { return field.getSample(aRow, aColumn); };
			const CellRange cells = HeightfieldCellsUnder(view, pose, aRegion);
			if (!cells.Empty())
			{
				HeightfieldTriangles(view, cells, pose, aRegion, aOut);
			}
		}
		break;
	}
	default:
		++aStats.unknownGeometry;
		break;
	}
	for (const Tri& tri : small)
	{
		if (KeepIfTouches(tri, aRegion))
		{
			aOut.push_back(tri);
		}
	}
}
```
with:
```cpp
void AppendTriangles(const PxShape& aShape, const PxTransform& aActorPose, const PxBounds3& aRegion,
                     ReadableMemory& aMemory, const ModuleBases& aBases, std::vector<Tri>& aOut, AppendStats& aStats)
{
	// A mesh object passes when it is readable and its vtable is the audited table of aClass or aOther.
	const auto meshOk = [&](const void* aMesh, AuditedClass aClass, AuditedClass aOther) {
		const ObjectCheck check = CheckObject(aMemory, aBases, aMesh);
		return check.verdict == ObjectVerdict::Ok && (check.cls == aClass || check.cls == aOther);
	};
	const PxTransform pose = aActorPose * aShape.getLocalPose();
	std::vector<Tri>  small;
	switch (aShape.getGeometryType())
	{
	case PxGeometryType::eBOX:
	{
		PxBoxGeometry geometry;
		if (aShape.getBoxGeometry(geometry))
		{
			BoxTriangles(geometry.halfExtents, pose, small);
		}
		break;
	}
	case PxGeometryType::eSPHERE:
	{
		PxSphereGeometry geometry;
		if (aShape.getSphereGeometry(geometry))
		{
			SphereTriangles(geometry.radius, pose, small);
		}
		break;
	}
	case PxGeometryType::eCAPSULE:
	{
		PxCapsuleGeometry geometry;
		if (aShape.getCapsuleGeometry(geometry))
		{
			CapsuleTriangles(geometry.radius, geometry.halfHeight, pose, small);
		}
		break;
	}
	case PxGeometryType::ePLANE:
		++aStats.planes;
		break;
	case PxGeometryType::eCONVEXMESH:
	{
		PxConvexMeshGeometry geometry;
		if (!aShape.getConvexMeshGeometry(geometry) || !geometry.convexMesh)
		{
			break;
		}
		if (!meshOk(geometry.convexMesh, AuditedClass::ConvexMesh, AuditedClass::ConvexMesh))
		{
			++aStats.unreadable;
			break;
		}
		const PxConvexMesh&        mesh = *geometry.convexMesh;
		std::vector<PxHullPolygon> polygons(mesh.getNbPolygons());
		uint32_t                   indexCount = 0;
		for (PxU32 i = 0; i < PxU32(polygons.size()); ++i)
		{
			mesh.getPolygonData(i, polygons[i]);
			indexCount = std::max<uint32_t>(indexCount, uint32_t(polygons[i].mIndexBase) + polygons[i].mNbVerts);
		}
		const PxVec3*  vertices = mesh.getVertices();
		const uint32_t vertexCount = mesh.getNbVertices();
		const PxU8*    indices = mesh.getIndexBuffer();
		if (vertexCount == 0 || indexCount == 0)
		{
			break;
		}
		if (!aMemory.Range(vertices, size_t(vertexCount) * sizeof(PxVec3)) || !aMemory.Range(indices, indexCount))
		{
			++aStats.unreadable;
			break;
		}
		ConvexPolygonTriangles(vertices, vertexCount, indices, polygons.data(), PxU32(polygons.size()), geometry.scale, pose, small);
		break;
	}
	case PxGeometryType::eTRIANGLEMESH:
	{
		PxTriangleMeshGeometry geometry;
		if (!aShape.getTriangleMeshGeometry(geometry) || !geometry.triangleMesh)
		{
			break;
		}
		if (!meshOk(geometry.triangleMesh, AuditedClass::TriangleMeshBv33, AuditedClass::TriangleMeshBv34))
		{
			++aStats.unreadable;
			break;
		}
		const PxTriangleMesh& mesh = *geometry.triangleMesh;
		const PxVec3*         vertices = mesh.getVertices();
		const uint32_t        vertexCount = mesh.getNbVertices();
		const void*           triangles = mesh.getTriangles();
		const bool            indices16 = mesh.getTriangleMeshFlags().isSet(PxTriangleMeshFlag::e16_BIT_INDICES);
		const uint32_t        triangleCount = mesh.getNbTriangles();
		if (vertexCount == 0 || triangleCount == 0)
		{
			break;
		}
		if (!aMemory.Range(vertices, size_t(vertexCount) * sizeof(PxVec3)) ||
		    !aMemory.Range(triangles, size_t(triangleCount) * 3 * (indices16 ? 2 : 4)))
		{
			++aStats.unreadable;
			break;
		}
		MeshTriangles(vertices, vertexCount, triangles, indices16, triangleCount, geometry.scale, pose, aRegion, aOut,
		              aStats.badTriangles);
		break;
	}
	case PxGeometryType::eHEIGHTFIELD:
	{
		PxHeightFieldGeometry geometry;
		if (!aShape.getHeightFieldGeometry(geometry) || !geometry.heightField)
		{
			break;
		}
		if (!meshOk(geometry.heightField, AuditedClass::HeightField, AuditedClass::HeightField))
		{
			++aStats.unreadable;
			break;
		}
		const PxHeightField& field = *geometry.heightField;
		HeightfieldView      view;
		view.rows = field.getNbRows();
		view.columns = field.getNbColumns();
		if (view.rows == 0 || view.columns == 0)
		{
			break;
		}
		// The samples are one array of rows x columns; getSample returns a reference into it.
		if (!aMemory.Range(&field.getSample(0, 0), size_t(view.rows) * view.columns * sizeof(PxHeightFieldSample)))
		{
			++aStats.unreadable;
			break;
		}
		view.heightScale = geometry.heightScale;
		view.rowScale = geometry.rowScale;
		view.columnScale = geometry.columnScale;
		view.sample = [&field](uint32_t aRow, uint32_t aColumn) { return field.getSample(aRow, aColumn); };
		const CellRange cells = HeightfieldCellsUnder(view, pose, aRegion);
		if (!cells.Empty())
		{
			HeightfieldTriangles(view, cells, pose, aRegion, aOut);
		}
		break;
	}
	default:
		++aStats.unknownGeometry;
		break;
	}
	for (const Tri& tri : small)
	{
		if (KeepIfTouches(tri, aRegion))
		{
			aOut.push_back(tri);
		}
	}
}
```

- [ ] **Step 5: Write `src/Probe.cpp`**

```cpp
#include "Probe.h"

#include "physx/Checks.h"

#include <cmath>
#include <format>
#include <map>

using namespace physx;

namespace cpmc
{
DumpData RunDump(px::PhysxSession& aSession, const PlayerPose& aPose, const std::string& aTrigger,
                 const std::string& aRuntime, const std::string& aTime)
{
	DumpData    data;
	DumpReport& report = data.report;
	report.time = aTime;
	report.trigger = aTrigger;
	report.runtime = aRuntime;
	report.player = aPose;

	const px::CheckResult compat = aSession.Check();
	report.physxFileVersion = compat.physxFileVersion;
	report.modules = compat.modules;
	report.steps = compat.steps;
	report.compatibilityPass = compat.pass;
	if (!compat.pass)
	{
		return data;  // no geometry without a passing compatibility check
	}

	const std::vector<PxScene*>&                scenes = compat.scenes;
	const PxVec3                                feet(float(aPose.feet.x), float(aPose.feet.y), float(aPose.feet.z));
	const PxVec3                                center = feet + PxVec3(0.0f, 0.0f, 8.0f);
	const PxVec3                                halfExtents(32.0f, 32.0f, 24.0f);
	const PxBounds3                             regionBox = PxBounds3::centerExtents(center, halfExtents);
	RegionResult                                region;
	region.center = { center.x, center.y, center.z };
	region.halfExtents = { halfExtents.x, halfExtents.y, halfExtents.z };
	std::map<std::string, std::vector<px::Tri>> byCategory;
	std::optional<NearestCapsule>               nearest;
	uint64_t                                    kept = 0;

	for (size_t i = 0; i < scenes.size(); ++i)
	{
		px::SceneDump scene = aSession.DumpScene(scenes[i], int(i), regionBox, feet, kTriangleCap - kept);
		report.censusSkipped += scene.census.skippedUnreadable + scene.census.skippedUnaudited;
		report.censusMismatches += scene.census.mismatches;
		region.boundsSelectedActors += scene.boundsSelected;
		region.overlapHits += scene.overlapHits;
		region.overlapOverflow |= scene.overlapCapped;
		region.selectMs += scene.selectMs;
		region.overlapMs += scene.overlapMs;
		region.extractMs += scene.extractMs;
		region.skippedPlanes += scene.stats.planes;
		region.unknownGeometry += scene.stats.unknownGeometry;
		region.badTriangles += scene.stats.badTriangles;
		region.unreadable += scene.stats.unreadable;
		region.triangleCapHit |= scene.triangleCapHit;
		for (auto& [category, triangles] : scene.triangles)
		{
			const std::string name = scenes.size() > 1 ? std::format("s{}_{}", i, category) : category;
			region.trianglesByCategory[name] += triangles.size();
			region.triangles += triangles.size();
			kept += triangles.size();
			auto& destination = byCategory[name];
			destination.insert(destination.end(), triangles.begin(), triangles.end());
		}
		if (scene.nearestCapsule && (!nearest || scene.nearestCapsule->distance < nearest->distance))
		{
			nearest = scene.nearestCapsule;
		}
		scene.census.queryFilter = px::TrimTally(scene.census.queryFilter);
		scene.census.simulationFilter = px::TrimTally(scene.census.simulationFilter);
		report.scenes.push_back(std::move(scene.census));
	}

	// The coordinate check: a ray down from 1.8 m above V's feet should hit the ground about 1.8 m down.
	CoordinateCheck check;
	check.nearestCapsule = nearest;
	const PxVec3 from = feet + PxVec3(0.0f, 0.0f, 1.8f);
	for (size_t i = 0; i < scenes.size(); ++i)
	{
		const std::optional<float> distance = aSession.RaycastDown(scenes[i], from, 3.0f);
		if (distance && (!check.rayHit || *distance < check.hitDistance))
		{
			check.rayHit = true;
			check.hitDistance = *distance;
			check.scene = int(i);
		}
	}
	check.pass = check.rayHit && std::fabs(check.hitDistance - 1.8) <= 0.3;

	report.region = region;
	report.coordinateCheck = check;
	report.compatibilityPass = compat.pass && report.censusMismatches == 0 && report.censusSkipped == 0;
	for (auto& [name, triangles] : byCategory)
	{
		data.groups.push_back({ name, CategoryColor(name), std::move(triangles) });
	}
	report.objWritten = !data.groups.empty();
	return data;
}
}
```

- [ ] **Step 6: Write `src/physx/Checks.h`**

`ConcreteTypeForName` and `IsPhysX342` go: `GameAbi` now gives each class's type ID, and step 1 checks the build's identity.
```cpp
#pragma once

#include "Report.h"
#include "physx/PxHeaders.h"

#include <cstddef>
#include <string>

// The pure parts of the compatibility check and the census bookkeeping. No PhysX calls happen here.
namespace cpmc::px
{
// PxTolerancesScale: length 0.01 .. 100, speed 0.1 .. 1000.
bool TolerancesPlausible(float aLength, float aSpeed);
// Finite, with a magnitude of at most 100.
bool GravityFinite(const physx::PxVec3& aGravity);
// A magnitude between 1 and 30.
bool GravityEarthLike(const physx::PxVec3& aGravity);

// A mesh-based geometry type references a mesh of the matching concrete type.
bool GeometryMatchesMesh(physx::PxGeometryType::Enum aGeometry, int aMeshConcreteType);

// "triangleMesh", "heightField", "convexMesh", "box", "sphere", "capsule", "plane" (report tallies).
const char* GeometryName(physx::PxGeometryType::Enum aGeometry);
// "trimesh", "heightfield", "convex", "box", "sphere", "capsule", "plane" (OBJ categories).
const char* GeometryShortName(physx::PxGeometryType::Enum aGeometry);

// The category of a shape: "trigger" if it has the trigger flag, otherwise "queryonly" if it has the
// scene-query flag but not the simulation flag, otherwise the actor's kind; then "_" and the short geometry name.
std::string Category(const char* aActorKind, physx::PxShapeFlags aShapeFlags, physx::PxGeometryType::Enum aGeometry);

// Filter data as "w0:w1:w2:w3" in hexadecimal.
std::string FilterKey(const physx::PxFilterData& aData);

// At most aMaxLength characters up to the first NUL; bytes outside printable ASCII become '?'.
std::string SanitizeName(const char* aName, size_t aMaxLength = 128);

// Keeps the aKeep most common entries (ties by name) and folds the rest into "other".
Tally TrimTally(const Tally& aTally, size_t aKeep = 64);
}
```

- [ ] **Step 7: Edit `src/physx/Checks.cpp`: remove the two functions**

Replace:
```cpp
#include <cmath>
#include <cstring>
#include <format>
```
with:
```cpp
#include <cmath>
#include <format>
```
Then replace:
```cpp
namespace cpmc::px
{
namespace
{
struct NamedType
{
	const char* name;
	int         type;
};

// Every concrete type name PhysX 3.4.2 returns from getConcreteTypeName(), with its PxConcreteType.
constexpr NamedType kTypes[] = {
	{ "PxHeightField", PxConcreteType::eHEIGHTFIELD },
	{ "PxConvexMesh", PxConcreteType::eCONVEX_MESH },
	{ "PxBVH33TriangleMesh", PxConcreteType::eTRIANGLE_MESH_BVH33 },
	{ "PxBVH34TriangleMesh", PxConcreteType::eTRIANGLE_MESH_BVH34 },
	{ "PxClothFabric", PxConcreteType::eCLOTH_FABRIC },
	{ "PxRigidDynamic", PxConcreteType::eRIGID_DYNAMIC },
	{ "PxRigidStatic", PxConcreteType::eRIGID_STATIC },
	{ "PxShape", PxConcreteType::eSHAPE },
	{ "PxMaterial", PxConcreteType::eMATERIAL },
	{ "PxConstraint", PxConcreteType::eCONSTRAINT },
	{ "PxCloth", PxConcreteType::eCLOTH },
	{ "PxParticleSystem", PxConcreteType::ePARTICLE_SYSTEM },
	{ "PxParticleFluid", PxConcreteType::ePARTICLE_FLUID },
	{ "PxAggregate", PxConcreteType::eAGGREGATE },
	{ "PxArticulation", PxConcreteType::eARTICULATION },
	{ "PxArticulationLink", PxConcreteType::eARTICULATION_LINK },
	{ "PxPruningStructure", PxConcreteType::ePRUNING_STRUCTURE },
};
}

int ConcreteTypeForName(const char* aName)
{
	if (!aName)
	{
		return -1;
	}
	for (const auto& entry : kTypes)
	{
		if (std::strcmp(entry.name, aName) == 0)
		{
			return entry.type;
		}
	}
	return -1;
}

bool IsPhysX342(const std::string& aFileVersionString)
{
	return aFileVersionString.starts_with("3.4.2.");
}

bool TolerancesPlausible(float aLength, float aSpeed)
```
with:
```cpp
namespace cpmc::px
{
bool TolerancesPlausible(float aLength, float aSpeed)
```

- [ ] **Step 8: Edit `tests/TestChecks.cpp`: remove their tests**

Replace:
```cpp
TEST(ConcreteTypeNamesMapToTheirIds)
{
	CHECK_EQ(ConcreteTypeForName("PxRigidStatic"), int(PxConcreteType::eRIGID_STATIC));
	CHECK_EQ(ConcreteTypeForName("PxRigidDynamic"), int(PxConcreteType::eRIGID_DYNAMIC));
	CHECK_EQ(ConcreteTypeForName("PxShape"), int(PxConcreteType::eSHAPE));
	CHECK_EQ(ConcreteTypeForName("PxBVH33TriangleMesh"), int(PxConcreteType::eTRIANGLE_MESH_BVH33));
	CHECK_EQ(ConcreteTypeForName("PxBVH34TriangleMesh"), int(PxConcreteType::eTRIANGLE_MESH_BVH34));
	CHECK_EQ(ConcreteTypeForName("PxHeightField"), int(PxConcreteType::eHEIGHTFIELD));
	CHECK_EQ(ConcreteTypeForName("PxConvexMesh"), int(PxConcreteType::eCONVEX_MESH));
	CHECK_EQ(ConcreteTypeForName("PxTriangleMesh"), -1);  // an interface name, never a concrete type
	CHECK_EQ(ConcreteTypeForName("garbage"), -1);
	CHECK_EQ(ConcreteTypeForName(nullptr), -1);
}

TEST(PhysXVersionComesFromTheFileVersionString)
{
	// NVIDIA's 3.4.2 release says 3.4.2.0 in the FileVersion string but 3.4.1.0 in the binary field.
	CHECK(IsPhysX342("3.4.2.0"));
	CHECK(!IsPhysX342("3.4.1.0"));
	CHECK(!IsPhysX342("13.4.2.0"));
	CHECK(!IsPhysX342(""));
}

TEST(PlausibilityRanges)
```
with:
```cpp
TEST(PlausibilityRanges)
```

- [ ] **Step 9: Delete `tests/TestGuard.cpp`**

```bash
cd /d/Cyberpunk-Minecraft && git rm -q spikes/physx-probe/tests/TestGuard.cpp
```

- [ ] **Step 10: Edit `CMakeLists.txt`: no guard, so standard C++ exceptions only**

Replace:
```cmake
# Shared settings. /EHa lets the crash guard (__try/__except) unwind C++ objects cleanly.
add_library(probe_settings INTERFACE)
target_compile_options(probe_settings INTERFACE /W4 /permissive- /utf-8 /EHa /Zc:__cplusplus)
```
with:
```cmake
# Shared settings.
add_library(probe_settings INTERFACE)
target_compile_options(probe_settings INTERFACE /W4 /permissive- /utf-8 /EHsc /Zc:__cplusplus)
```
Then replace:
```cmake
	tests/TestGuard.cpp
```
with nothing (delete the line):
```cmake
```

- [ ] **Step 11: Build; no warnings, and the tests pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | tail -1
```
Expected: no lines from the build filter, then `55 test(s), 0 failure(s)`

- [ ] **Step 12: Check the DLL's exports and imports**

```bash
DUMPBIN="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/dumpbin.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && echo "entry points: $("$DUMPBIN" -nologo -exports build/RelWithDebInfo/PhysXProbe.dll | grep -c -E ' (Main|Query|Supports) = ')" \
  && echo "PhysX3 imports: $("$DUMPBIN" -nologo -imports build/RelWithDebInfo/PhysXProbe.dll | grep -c PhysX3)" \
  && grep -c -E "Guarded|__try|ConcreteTypeForName|IsPhysX342" src/physx/*.cpp src/physx/*.h src/*.cpp | grep -v ":0" | wc -l
```
Expected: `entry points: 3`, `PhysX3 imports: 0`, then `0` (nothing left of the guard or the name checks).

- [ ] **Step 13: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add -A spikes/physx-probe \
  && git commit -q -m "physx-probe: check every object and array instead of catching faults" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 21: The plugin: no automatic dump, version 0.2.0

**Files:**
- Modify: `src/Plugin.cpp` (rewritten), `CMakeLists.txt` (the project version)

- [ ] **Step 1: Write `src/Plugin.cpp`**

The automatic trigger goes (see the spec's `Plugin.cpp` section), and the version becomes 0.2.0.
```cpp
// PhysXProbe: a read-only RED4ext plugin that checks whether Cyberpunk's PhysX 3.4.2 can be read through NVIDIA's
// headers, corrected where the game's build differs, and dumps the collision around V. F10 or a request file starts a
// dump; there is no automatic one. See docs/superpowers/specs/2026-10-01-physx-probe-design.md.
#include <RED4ext/RED4ext.hpp>

#include "DumpWriter.h"
#include "PlayerLocator.h"
#include "Probe.h"

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <string>

namespace
{
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

RED4ext::v1::PluginHandle   g_handle = nullptr;
const RED4ext::v1::Sdk*     g_sdk = nullptr;
std::filesystem::path       g_pluginDir;
cpmc::px::PhysxSession      g_session;
cpmc::DumpWriter            g_writer;
std::string                 g_runtime;

bool                        g_f10WasDown = false;
Clock::time_point           g_lastDump{};
Clock::time_point           g_lastSecondlyCheck{};

template<class... Args>
void Log(std::format_string<Args...> aFormat, Args&&... aArgs)
{
	if (g_sdk)
	{
		g_sdk->logger->Info(g_handle, std::format(aFormat, std::forward<Args>(aArgs)...).c_str());
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

bool GameInForeground()
{
	DWORD processId = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &processId);
	return processId == GetCurrentProcessId();
}

// "2026-10-01T12:34:56" for the report, "2026-10-01_12-34-56" for the folder.
std::pair<std::string, std::string> Timestamps()
{
	SYSTEMTIME now{};
	GetLocalTime(&now);
	return { std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond),
	         std::format("{:04}-{:02}-{:02}_{:02}-{:02}-{:02}", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond) };
}

void RunOne(const std::string& aTrigger)
{
	const auto pose = cpmc::LocatePlayer();
	if (!pose)
	{
		Log("{} trigger: no player", aTrigger);
		return;
	}
	const auto [time, folderName] = Timestamps();
	LARGE_INTEGER start;
	LARGE_INTEGER end;
	LARGE_INTEGER frequency;
	QueryPerformanceCounter(&start);
	cpmc::DumpData data = cpmc::RunDump(g_session, *pose, aTrigger, g_runtime, time);
	QueryPerformanceCounter(&end);
	QueryPerformanceFrequency(&frequency);
	data.report.mainThreadPauseMs = double(end.QuadPart - start.QuadPart) * 1000.0 / double(frequency.QuadPart);

	const auto& report = data.report;
	Log("dump ({}): compatibility {}, {} scene(s), {} triangles, coordinate check {}, main-thread pause {:.1f} ms, {} object(s) skipped",
	    aTrigger, report.compatibilityPass ? "passed" : "FAILED", report.scenes.size(),
	    report.region ? report.region->triangles : 0,
	    report.coordinateCheck ? (report.coordinateCheck->pass ? "passed" : "FAILED") : "not run", report.mainThreadPauseMs,
	    report.censusSkipped);
	for (const auto& step : report.steps)
	{
		if (!step.pass)
		{
			Log("  compatibility step {} ({}) failed: {}", step.step, step.name, step.detail);
		}
	}
	cpmc::DumpJob job{ g_pluginDir / "dumps" / folderName, std::move(data.report), std::move(data.groups), pose->feet };
	g_writer.Start(std::move(job));
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
	if (auto written = g_writer.TakeResult())
	{
		if (written->ok)
		{
			Log("dump written to {} ({} triangles, {:.0f} ms)", written->folder.string(), written->triangles, written->writeMs);
		}
		else
		{
			Log("couldn't write the dump to {}: {}", written->folder.string(), written->error);
		}
	}

	const auto  now = Clock::now();
	std::string trigger;

	const bool f10Down = (GetAsyncKeyState(VK_F10) & 0x8000) != 0 && GameInForeground();
	if (f10Down && !g_f10WasDown)
	{
		trigger = "hotkey";
	}
	g_f10WasDown = f10Down;

	if (now - g_lastSecondlyCheck >= 1s)
	{
		g_lastSecondlyCheck = now;
		const auto      request = g_pluginDir / "dump.request";
		std::error_code error;
		if (trigger.empty() && std::filesystem::exists(request, error))
		{
			std::filesystem::remove(request, error);
			trigger = "request";
		}
	}

	if (trigger.empty() || now - g_lastDump < 2s)
	{
		return false;
	}
	if (g_writer.Busy())
	{
		Log("{} trigger ignored: the previous dump is still being written", trigger);
		return false;
	}
	g_lastDump = now;
	RunOne(trigger);
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
		g_pluginDir = PluginFolder();
		g_runtime = std::format("{}.{}.{}", unsigned(aSdk->runtime->major), aSdk->runtime->minor, aSdk->runtime->patch);
		static RED4ext::v1::GameState running{ &OnEnter, &OnUpdate, &OnExit };
		aSdk->gameStates->Add(aHandle, RED4ext::EGameStateType::Running, &running);
		Log("PhysXProbe 0.2.0 loaded (game {}); F10 or {} triggers a dump", g_runtime, (g_pluginDir / "dump.request").string());
		break;
	}
	case RED4ext::v1::EMainReason::Unload:
		g_writer.Join();
		break;
	}
	return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* aInfo)
{
	aInfo->name = L"PhysXProbe";
	aInfo->author = L"Cyberpunk-Minecraft";
	aInfo->version = RED4EXT_V1_SEMVER(0, 2, 0);
	aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
	aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
	return RED4EXT_API_VERSION_1;
}
```

- [ ] **Step 2: Edit `CMakeLists.txt`: the project version**

Replace:
```cmake
project(PhysXProbe VERSION 0.1.0 LANGUAGES CXX)
```
with:
```cmake
project(PhysXProbe VERSION 0.2.0 LANGUAGES CXX)
```

- [ ] **Step 3: Build; no warnings, the tests pass, and the DLL says 0.2.0**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ./build/RelWithDebInfo/probe_tests.exe | tail -1; grep -a -c "PhysXProbe 0.2.0 loaded" build/RelWithDebInfo/PhysXProbe.dll
```
Expected: no lines from the build filter, then `55 test(s), 0 failure(s)`, then `1`

- [ ] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/physx-probe \
  && git commit -q -m "physx-probe: drop the automatic dump; version 0.2.0" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 22: Verify revision 2 in the running game

This is the test for everything that talks to the game. It needs the user at the keyboard, and the agent never drives
their mouse or keyboard. Write down the numbers as you go; Step 6 records them.

**Files:**
- Create: `docs/spikes/2026-10-01-physx-probe-results.md`

- [ ] **Step 1: Deploy (the game must not be running)**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game first, because a running game locks the DLL.
`PhysXProbe.dll.disabled` is revision 1, renamed after the freeze so the game couldn't load it; it goes.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && rm -f /e/Cyberpunk2077/red4ext/plugins/PhysXProbe/PhysXProbe.dll.disabled \
  && "$CMAKE" --preset default -DCP2077_DIR=E:/Cyberpunk2077 > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E " error |warning C"; \
  ls /e/Cyberpunk2077/red4ext/plugins/PhysXProbe/ \
  && cmp build/RelWithDebInfo/PhysXProbe.dll /e/Cyberpunk2077/red4ext/plugins/PhysXProbe/PhysXProbe.dll && echo installed
```
Expected: `PhysXProbe.dll`, `PhysXProbe.pdb` and `dumps` in the folder, then `installed`.

- [ ] **Step 2: The startup world, at the main menu**

Ask the user to launch Cyberpunk 2077 from Epic and wait at the main menu, without loading a save, and to say when
they're there. Then run:
```bash
touch /e/Cyberpunk2077/red4ext/plugins/PhysXProbe/dump.request; sleep 5; \
  L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i physx | head -1); tail -4 "/e/Cyberpunk2077/red4ext/logs/$L"
```
Expected: `PhysXProbe 0.2.0 loaded`, then `dump (request): compatibility passed, 1 scene(s), ... 0 object(s) skipped`,
then `dump written to ...`. If the game froze or crashed instead, stop: read the newest folder under
`%LOCALAPPDATA%\REDEngine\ReportQueue\` and the probe log, and record what happened in Step 6.

Then check the report:
```bash
D=/e/Cyberpunk2077/red4ext/plugins/PhysXProbe/dumps/$(ls -t /e/Cyberpunk2077/red4ext/plugins/PhysXProbe/dumps | head -1)
uv run --no-project -q python -c "
import json,sys; r=json.load(open(sys.argv[1]+'/report.json'))
print('modules:', [(m['name'], m['audited']) for m in r['game']['modules']])
c=r['compatibility']; print('compatibility:', c['pass'], '| skipped:', c['censusSkipped'], '| mismatches:', c['censusMismatches'])
for s in c['steps']: print('  step', s['step'], s['name'], 'PASS' if s['pass'] else 'FAIL', '-', s['detail'])
for s in r['scenes']: print('scene', s['index'], s['byConcreteType'], s['byGeometryType'], s['shapeFlags'], len(s['queryFilter']), 'filter keys', s['skipped'])
" "$D"
```
Pass criteria for the startup world:
- every module `audited`, and all seven steps `PASS`;
- skipped `0` and mismatches `0`;
- `shapeFlags` and the filter tallies filled in. Revision 1's report had them empty, because the census faulted on
  every shape.

- [ ] **Step 3: Free roam**

Send the user these instructions:
1. Load a save that's in free roam, not a scene.
2. Stand on solid ground (pavement, stairs or a walkway, not a car, and not right under anything low) with varied
   geometry around: buildings, stairs or kerbs, a parked car.
3. Stand still, leave the mouse alone, and press F10.
4. Without moving, take a screenshot (Win+Shift+S works) and paste it into the chat.

Then check the pass criteria:
```bash
D=/e/Cyberpunk2077/red4ext/plugins/PhysXProbe/dumps/$(ls -t /e/Cyberpunk2077/red4ext/plugins/PhysXProbe/dumps | head -1)
uv run --no-project -q python -c "
import json,sys; r=json.load(open(sys.argv[1]+'/report.json'))
c=r['compatibility']; print('trigger:', r['trigger'], '| compatibility:', c['pass'], '| skipped:', c['censusSkipped'], '| mismatches:', c['censusMismatches'])
for s in c['steps']: print('  step', s['step'], s['name'], 'PASS' if s['pass'] else 'FAIL', '-', s['detail'])
cc=r['coordinateCheck']; print('coordinate check:', cc and cc['pass'], '| hit distance:', cc and cc['hitDistance'], '| capsule:', cc and cc['nearestCapsule'])
g=r['region']; print('region:', g and {k: g[k] for k in ('boundsSelectedActors','overlapHits','triangles','selectMs','overlapMs','extractMs','triangleCapHit','skipped')})
print('scenes:', [(s['index'], s['staticActors'], s['dynamicActors'], s['requiresRwLock'], round(s['censusMs'],1), s['skipped']) for s in r['scenes']])
print('main-thread pause ms:', r['mainThreadPauseMs'], '| camera:', r['player'] and r['player']['camera'])
" "$D"
```
Pass criteria (the spec's):
1. `compatibility: True` with all seven steps `PASS`, skipped `0` and mismatches `0`.
2. The coordinate check is `True`, with a hit distance of 1.8 ± 0.3.
3. Checked in Step 4 below.
4. Every timing is present.

**If compatibility failed,** stop here and record the failing step and its detail in Step 6:
- **Skipped objects with unaudited vtables:** the live world has a class or build the audit didn't cover.
- **Unreadable objects:** the world changed under the read, which means a race.

- [ ] **Step 4: Render and compare with the screenshot**

```bash
D=/e/Cyberpunk2077/red4ext/plugins/PhysXProbe/dumps/$(ls -t /e/Cyberpunk2077/red4ext/plugins/PhysXProbe/dumps | head -1)
cd /d/Cyberpunk-Minecraft/spikes/physx-probe && uv run --no-project -q --with numpy --with pillow python tools/render_dump.py "$D"
```
Look at `eye.png` next to the user's screenshot, and at `top.png` too. Criterion 3 passes when the main shapes line up:
- the ground;
- the building edges;
- steps or kerbs;
- a car's outline, when there's one.

If everything lines up but looks too wide or too narrow, rerender with `--fov-axis h`, then note which axis the game's
field of view is.

- [ ] **Step 5: A second dump somewhere different, triggered from here**

Ask the user to walk somewhere different (indoors, or up some stairs), stand still, screenshot again, and say when
they're there. Then run:
```bash
touch /e/Cyberpunk2077/red4ext/plugins/PhysXProbe/dump.request; sleep 5; ls -t /e/Cyberpunk2077/red4ext/plugins/PhysXProbe/dumps | head -2
```
Expected: a new dump folder, and `dump (request)` in the log. Repeat Steps 3 and 4 for it.

- [ ] **Step 6: Record the results and commit**

Write `docs/spikes/2026-10-01-physx-probe-results.md` with these sections. Fill every value from the reports and your
observations:
- **Verdict:** pass or fail, in one sentence.
- **Revision 1:** what the first runs found, briefly. That's the version-field quirk, the `PxShape` slot shift, and the
  crash handler; link `2026-10-01-physx-abi-audit.md`.
- **Pass criteria:** a table with one row per criterion (1–4), and one column per dump holding the observed value. Put
  the startup world in its own column.
- **Compatibility steps:** each step's detail line from the first free-roam dump.
- **Scenes:**
  - the scene count;
  - static and dynamic actors per scene;
  - `requiresRwLock`;
  - aggregates and articulations;
  - census time.
- **What's in the collision:**
  - `byGeometryType` and `shapeFlags`;
  - the ten most common `queryFilter` and `simulationFilter` keys;
  - a sample of actor names.
- **Region cost:**
  - bounds-selected actors against overlap hits;
  - triangles by category;
  - select, overlap and extract milliseconds;
  - the main-thread pause.
- **Coordinates:** the ray's hit distance, the nearest capsule, and which axis the field of view uses.
- **Surprises and next steps**, for spike 2 and the collision exporter.

Include the screenshots and renders side by side, or link to their dump folders. Then commit:
```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes \
  && git commit -q -m "physx-probe: in-game results" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---
## Afterwards

Use superpowers:finishing-a-development-branch to decide how `spike/physx-probe` goes back into `main`.
