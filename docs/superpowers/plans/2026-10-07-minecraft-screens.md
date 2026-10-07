# Minecraft's Screens Implementation Plan, Parts 1 and 2 (step 0, then BlockRender 0.23.0 and run E)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> In this project, plans run inline with superpowers:executing-plans, never through sub-agents.

**Goal:** Part 1 of spike 8, step 0 of the spec: check Minecraft's side of its screens worked from the game before
BlockRender sends anything. `mclink` sends what BlockRender will (a cursor in the HUD image's pixels, clicks, keys,
typed text and the pause menu's `kInOpenMenu`) to SkyCraft's unchanged mod in the user's Minecraft, without the game,
and reads back what Minecraft did: which slot it highlights, where items land, what chat got, which game mode it set.

**Architecture:** all of it is plain C++, tested without the game or Minecraft.
- **`mclink`'s new verbs** (`tools/mclink/McLinkScript.*`, `tools/mclink/Main.cpp`):
  - `cursor <x> <y>` sends `kInCursor`;
  - `text <text>` sends each character as `kInText`, from UTF-8 (`CodePoints`);
  - `pause` sends `kInOpenMenu`;
  - `wait-screen open|closed [seconds]` waits for McState's screen flag;
  - `hud-box <x0> <y0> <x1> <y1>` prints a box's mean colour in the next HUD image (`tools/mclink/HudBox.*`), to
    tell from the log where Minecraft drew a slot's hover highlight.
- **The link** (`src/link/Link.*`) keeps McState's GUI scale, which `mclink`'s status prints: step 0's slot pixels
  depend on it.
- **Step 0's script,** `tools/mclink/screens.txt`, works a test chest, chat, the F3+F4 switcher, the pause menu and the
  inventory. It touches nothing of the user's inventory, and leaves the game mode as it was.

Nothing changes in the game: BlockRender's own code is untouched but for the link's new field, and nothing is
deployed.

**Tech stack:**
- C++20, MSVC 14.44 (Visual Studio 2022), CMake 3.31 (bundled with Visual Studio);
- SkyCraft's protocol (version 11) and its Fabric mod 0.1.2, unchanged, in the user's `CyberCraft` instance (Minecraft
  26.3, Fabric Loader 0.19.5, CyberCraft Tweaks 1.6.0).

**Spec:** [docs/superpowers/specs/2026-10-07-minecraft-screens-design.md](../specs/2026-10-07-minecraft-screens-design.md)

**Part 2** (Tasks 6 to 15) builds BlockRender 0.23.0, which works Minecraft's screens from the game in build mode (build
mode's keys, the cursor and its arrow, typed text, the status line), then runs run E in the game. It was planned after
step 0, from its results, and its own section below says how it's built.

---

## Before you start

- **The branch:** work on `spike/minecraft-screens`, which holds the spec and this plan.
- **The shell:** commands are for Git Bash.
  - CMake isn't on PATH, and shell variables don't persist between tool calls, so every command block sets it:
    ```bash
    CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
    ```
  - Pass MSBuild switches as `-m`, never `/m`: Git Bash rewrites `/m` into a file path.
- **Edit steps:** an Edit step's code blocks come in pairs, applied in order: the text to find, which occurs exactly
  once in the file as the pairs before it left it, then the text that replaces it. A Write step's block is the whole
  file.
- **Already verified:** all the code in this plan has been compiled and tested once already, in a scratch clone.
  - Tasks 1 to 3 were then replayed in a fresh clone by a script that applied every file step exactly as written, and
    checked every build, test and `mclink` step's expected result. The results:
    - zero compiler warnings at `/W4`;
    - 526 C++ tests passing;
    - the replayed files identical to the scratch clone's.
  - Each new test was broken once, by a mutation of the code it covers, and failed as it should: 23 mutations.
  - Step 0's script was checked against Minecraft 26.3's own classes and the user's settings ("Found while planning
    Part 1"): the chest screen's size and its slots' places, the slot highlight's sprites, the commands' syntax, the
    switcher's rounds, the user's GUI scale and bindings.

  So a failure while following the plan most likely means a transcription slip: compare against the plan's code first.
- **Test counts:** 520 at the start, then 525, 526 and 526 after Tasks 1 to 3.
- **What only Minecraft can test:** how Minecraft answers the inputs, in Task 4, with the user.
- **The user:**
  - **does everything in Minecraft and the Modrinth App:** starting and stopping it, and anything on its screen. Never
    drive their mouse or keyboard: ask, then wait for them to say they're done;
  - **approves downloads first.** Nothing in Part 1 downloads anything;
  - **approves changes to their Minecraft world:** Task 4's script places a test chest at 882 7 2007 in the `SkyCraft`
    world, SkyCraft's own in the `CyberCraft` instance, and removes it with what's in it at its end; and it switches
    the game mode twice with F3+F4, which leaves it as it was.
- **The game:** Part 1 deploys nothing. `mclink` needs the game closed, since the game would hold the mapping.
- **No faults:** shared memory is written by another process: every count, size and offset from it is checked before
  use.
- **Commits** go on `spike/minecraft-screens`, and end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## File map (Part 1)

All paths under `src/`, `tests/` and `tools/` are relative to `spikes/block-render/`.

| File | Responsibility |
|---|---|
| `CMakeLists.txt` | The new source and test |
| `tools/mclink/McLinkScript.*` | The five new verbs parsed; `CodePoints`, UTF-8 to code points |
| `tools/mclink/HudBox.*` | `MeanOfBox`: a HUD image box's mean colour |
| `tools/mclink/Main.cpp` | The verbs run; the GUI scale in the status |
| `tools/mclink/screens.txt` | Step 0's script |
| `src/link/Link.*` | The status's `guiScale`, from McState |
| `tests/TestMcLinkScript.cpp`, `tests/TestLink.cpp` | Their units' new tests |
| `tests/TestHudBox.cpp` | `HudBox`'s tests |

---

### Task 1: `mclink` parses the screens' verbs

Five verbs for step 0 (the spec's "Step 0", and "Changes while planning Part 1"):
- **`cursor <x> <y>`:** whole pixels of the HUD image, which are Minecraft's window coordinates, at most an overlay
  slot's largest image (3,840 by 2,160): x from 0 to 3,839, y from 0 to 2,159.
- **`text <text>`:** the rest of the line, which must be well-formed UTF-8 of printable characters (none below 0x20,
  nor 0x7F), as BlockRender will send only those. `CodePoints` decodes it, refusing overlong forms, surrogates, code
  points past U+10FFFF, and any byte out of place.
- **`pause`:** nothing after it.
- **`wait-screen open|closed [seconds]`:** 5 seconds unless given, over 0.
- **`hud-box <x0> <y0> <x1> <y1>`:** a box of whole pixels, the far edges left out, so x0 is below x1 (at most 3,840)
  and y0 below y1 (at most 2,160). `MeanOfBox` takes the HUD image as `mclink` keeps it, its rows top-down or bottom-up
  as Minecraft sent them, and counts the box from the top left; it refuses a box that isn't all inside the image, or
  an image whose bytes don't match its size.

**Files:**
- Create: `tools/mclink/HudBox.h`, `tools/mclink/HudBox.cpp`, `tests/TestHudBox.cpp`
- Modify: `tools/mclink/McLinkScript.h`, `tools/mclink/McLinkScript.cpp`, `tests/TestMcLinkScript.cpp`, `CMakeLists.txt`

- [x] **Step 1: Edit `tests/TestMcLinkScript.cpp` to add the failing tests**

```cpp
TEST(ScriptParsesTheGround)
```
```cpp
TEST(ScriptParsesScreenActions)
{
	std::string error;
	const auto  actions = ParseScript("cursor 496 244\ntext /say step0 \xC3\xA6\xC3\xB8\xC3\xA5 @  \npause\nwait-screen open\n"
	                                  "wait-screen closed 3\nhud-box 484 268 508 292\n",
	                                  error);
	CHECK(actions.has_value());
	CHECK_EQ(actions ? actions->size() : 0, size_t(6));
	if (!actions || actions->size() != 6)
	{
		return;
	}
	const auto& a = *actions;
	CHECK(a[0].kind == Kind::Cursor);
	CHECK_EQ(a[0].numbers[0], 496.0);
	CHECK_EQ(a[0].numbers[1], 244.0);
	CHECK(a[1].kind == Kind::Text);
	CHECK_STR(a[1].text, "/say step0 \xC3\xA6\xC3\xB8\xC3\xA5 @");  // as written, but for the line's ends
	CHECK(a[2].kind == Kind::Pause);
	CHECK(a[3].kind == Kind::WaitScreen);
	CHECK(a[3].on);
	CHECK_EQ(a[3].numbers[0], 5.0);  // seconds, unless given
	CHECK(a[4].kind == Kind::WaitScreen);
	CHECK(!a[4].on);
	CHECK_EQ(a[4].numbers[0], 3.0);
	CHECK(a[5].kind == Kind::HudBox);
	CHECK_EQ(a[5].numbers[0], 484.0);
	CHECK_EQ(a[5].numbers[1], 268.0);
	CHECK_EQ(a[5].numbers[2], 508.0);
	CHECK_EQ(a[5].numbers[3], 292.0);
}

TEST(ScriptRejectsBadScreenActions)
{
	std::string error;
	CHECK(ParseScript("cursor 0 0\ncursor 3839 2159\n", error).has_value());  // the corners of the largest HUD image
	CHECK(!ParseScript("cursor 3840 0", error).has_value());
	CHECK(!ParseScript("cursor 0 2160", error).has_value());
	CHECK(!ParseScript("cursor -1 0", error).has_value());
	CHECK(!ParseScript("cursor 2.5 0", error).has_value());
	CHECK(!ParseScript("cursor 1", error).has_value());
	CHECK_STR(error, "line 1: cursor takes whole HUD pixels, x from 0 to 3839 and y from 0 to 2159: \"cursor 1\"");
	CHECK(!ParseScript("text", error).has_value());
	CHECK(!ParseScript("text \xC3\x28", error).has_value());  // not UTF-8
	CHECK(!ParseScript("text a\x7F", error).has_value());     // a control character
	CHECK_STR(error, "line 1: text takes printable UTF-8: \"text a\x7F\"");
	CHECK(!ParseScript("pause now", error).has_value());
	CHECK(!ParseScript("wait-screen", error).has_value());
	CHECK(!ParseScript("wait-screen ajar", error).has_value());
	CHECK(!ParseScript("wait-screen open 0", error).has_value());
	CHECK_STR(error, "line 1: wait-screen takes open or closed, then an optional number of seconds: \"wait-screen open 0\"");
	CHECK(ParseScript("hud-box 0 0 3840 2160", error).has_value());
	CHECK(!ParseScript("hud-box 10 10 10 20", error).has_value());  // no width
	CHECK(!ParseScript("hud-box 10 20 20 10", error).has_value());
	CHECK(!ParseScript("hud-box 0 0 3841 10", error).has_value());
	CHECK(!ParseScript("hud-box 0 0 10", error).has_value());
	CHECK_STR(error, "line 1: hud-box takes whole HUD pixels x0 y0 x1 y1, x0 below x1 up to 3840, y0 below y1 up to 2160: \"hud-box 0 0 10\"");
}

TEST(CodePointsDecodeUtf8)
{
	// a, æ (2 bytes), € (3 bytes), 🙂 (4 bytes)
	const auto points = CodePoints("a\xC3\xA6\xE2\x82\xAC\xF0\x9F\x99\x82");
	CHECK(points.has_value());
	CHECK_EQ(points ? points->size() : 0, size_t(4));
	if (points && points->size() == 4)
	{
		CHECK_EQ((*points)[0], uint32_t(0x61));
		CHECK_EQ((*points)[1], uint32_t(0xE6));
		CHECK_EQ((*points)[2], uint32_t(0x20AC));
		CHECK_EQ((*points)[3], uint32_t(0x1F642));
	}
	CHECK(!CodePoints("\xC3").has_value());              // cut short
	CHECK(!CodePoints("\xC3\x28").has_value());          // a bad continuation
	CHECK(!CodePoints("\xC3\xC3").has_value());          // a lead where a continuation must be
	CHECK(!CodePoints("\x80").has_value());              // a continuation first
	CHECK(!CodePoints("\xC0\xAF").has_value());          // overlong
	CHECK(!CodePoints("\xE0\x80\xAF").has_value());      // overlong
	CHECK(!CodePoints("\xED\xA0\x80").has_value());      // a surrogate
	CHECK(!CodePoints("\xF4\x90\x80\x80").has_value());  // past U+10FFFF
	CHECK(!CodePoints("\xF8\x88\x80\x80\x80").has_value());
	CHECK(CodePoints("").has_value() && CodePoints("")->empty());
}

TEST(ScriptParsesTheGround)
```

- [x] **Step 2: Write the failing test, `tests/TestHudBox.cpp`**

```cpp
#include "Harness.h"

#include "HudBox.h"

using namespace cpmc::mclink;

namespace
{
// A 3 x 2 image: each pixel's red is its x, green its y counted from the top, blue 10, alpha 255. As stored top row
// first, or bottom row first.
std::vector<uint8_t> Image(bool aBottomUp)
{
	std::vector<uint8_t> rgba;
	for (uint32_t row = 0; row < 2; ++row)
	{
		const uint32_t y = aBottomUp ? 1 - row : row;
		for (uint32_t x = 0; x < 3; ++x)
		{
			rgba.insert(rgba.end(), { uint8_t(x), uint8_t(y), 10, 255 });
		}
	}
	return rgba;
}
}

TEST(MeanOfBoxCountsFromTheTopLeft)
{
	for (const bool bottomUp : { false, true })
	{
		const auto top = MeanOfBox(Image(bottomUp), 3, 2, bottomUp, 1, 0, 3, 1);  // x 1 and 2 of the top row
		CHECK(top.has_value());
		if (top)
		{
			CHECK_EQ(top->r, 1.5);
			CHECK_EQ(top->g, 0.0);
			CHECK_EQ(top->b, 10.0);
			CHECK_EQ(top->a, 255.0);
			CHECK_EQ(top->pixels, uint64_t(2));
		}
		const auto all = MeanOfBox(Image(bottomUp), 3, 2, bottomUp, 0, 0, 3, 2);
		CHECK(all.has_value());
		if (all)
		{
			CHECK_EQ(all->r, 1.0);
			CHECK_EQ(all->g, 0.5);
			CHECK_EQ(all->pixels, uint64_t(6));
		}
		const auto bottom = MeanOfBox(Image(bottomUp), 3, 2, bottomUp, 0, 1, 1, 2);  // the bottom left pixel
		CHECK(bottom.has_value() && bottom->r == 0.0 && bottom->g == 1.0);
	}
}

TEST(MeanOfBoxRefusesWhatIsOutside)
{
	const std::vector<uint8_t> image = Image(false);
	CHECK(!MeanOfBox(image, 3, 2, false, 0, 0, 4, 2).has_value());  // past the right edge
	CHECK(!MeanOfBox(image, 3, 2, false, 0, 0, 3, 3).has_value());  // past the bottom
	CHECK(!MeanOfBox(image, 3, 2, false, 2, 0, 2, 2).has_value());  // no width
	CHECK(!MeanOfBox(image, 3, 2, false, 0, 1, 3, 1).has_value());  // no height
	CHECK(!MeanOfBox(image, 4, 2, false, 0, 0, 1, 1).has_value());  // the bytes aren't a 4 x 2 image
	CHECK(!MeanOfBox({}, 0, 0, false, 0, 0, 1, 1).has_value());
}
```

- [x] **Step 3: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestSnapshots.cpp
	tools/mclink/DumpObj.cpp
```
```cmake
	tests/TestSnapshots.cpp
	tests/TestHudBox.cpp
	tools/mclink/DumpObj.cpp
```

- [x] **Step 4: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -o -E "error C[0-9]+: [^[]*" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'HudBox.h'`, and `error C2065: 'Cursor': undeclared identifier`.

- [x] **Step 5: Write `tools/mclink/HudBox.h`**

```cpp
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

// mclink's hud-box (spike 8's step 0): the mean colour of a box of Minecraft's HUD image, to tell from the log whether
// Minecraft drew a slot's hover highlight where the cursor was sent.
namespace cpmc::mclink
{
struct BoxMean
{
	double   r = 0.0;  // each channel's mean, 0 to 255, as stored (premultiplied by alpha)
	double   g = 0.0;
	double   b = 0.0;
	double   a = 0.0;
	uint64_t pixels = 0;
};

// The mean of each channel over the box from (aX0, aY0) to (aX1, aY1), the far edges left out, counted in pixels from
// the image's top left, of an RGBA8 image aWidth by aHeight whose rows are stored bottom row first if aBottomUp. Nothing
// if the box is empty or not all inside the image, or aRgba isn't aWidth * aHeight * 4 bytes.
std::optional<BoxMean> MeanOfBox(const std::vector<uint8_t>& aRgba, uint32_t aWidth, uint32_t aHeight, bool aBottomUp, uint32_t aX0,
                                 uint32_t aY0, uint32_t aX1, uint32_t aY1);
}
```

- [x] **Step 6: Write `tools/mclink/HudBox.cpp`**

```cpp
#include "HudBox.h"

namespace cpmc::mclink
{
std::optional<BoxMean> MeanOfBox(const std::vector<uint8_t>& aRgba, uint32_t aWidth, uint32_t aHeight, bool aBottomUp, uint32_t aX0,
                                 uint32_t aY0, uint32_t aX1, uint32_t aY1)
{
	if (aRgba.size() != uint64_t(aWidth) * aHeight * 4 || aX0 >= aX1 || aY0 >= aY1 || aX1 > aWidth || aY1 > aHeight)
	{
		return std::nullopt;
	}
	uint64_t sums[4]{};
	for (uint32_t y = aY0; y < aY1; ++y)
	{
		const uint32_t row = aBottomUp ? aHeight - 1 - y : y;
		for (uint32_t x = aX0; x < aX1; ++x)
		{
			const uint8_t* pixel = aRgba.data() + (uint64_t(row) * aWidth + x) * 4;
			for (int c = 0; c < 4; ++c)
			{
				sums[c] += pixel[c];
			}
		}
	}
	BoxMean mean;
	mean.pixels = uint64_t(aX1 - aX0) * (aY1 - aY0);
	const double count = double(mean.pixels);
	mean.r = double(sums[0]) / count;
	mean.g = double(sums[1]) / count;
	mean.b = double(sums[2]) / count;
	mean.a = double(sums[3]) / count;
	return mean;
}
}
```

- [x] **Step 7: Edit `tools/mclink/McLinkScript.h` to add the verbs and `CodePoints`**

```cpp
#include <optional>
```
```cpp
#include <cstdint>
#include <optional>
```
```cpp
//   save-textures <folder>         save every entity texture received so far as <folder>\texture-<id>.dds
```
```cpp
//   save-textures <folder>         save every entity texture received so far as <folder>\texture-<id>.dds
//   cursor <x> <y>                 Minecraft's cursor to that pixel of its HUD image, its window's (spike 8's step 0)
//   text <text>                    each character to the line's end typed into Minecraft's open screen, as kInText
//   pause                          open Minecraft's pause menu (kInOpenMenu)
//   wait-screen open|closed [seconds]
//                                  until Minecraft says a screen is open, or that none is (5 s unless given)
//   hud-box <x0> <y0> <x1> <y1>    wait for the next HUD image (5 s at most), and print the box's mean colour, the far
//                                  edges left out
```
```cpp
		SaveTextures,
```
```cpp
		SaveTextures,
		Cursor,
		Text,
		Pause,
		WaitScreen,
		HudBox,
```
```cpp
	                      // kind (proto::HurtKind), share and attacker; the health's share
	bool        on = false;  // link, build and hud; a hurt's power attack
	std::string text;        // the command, the file, the folder, the ground, or an actor's name
```
```cpp
	                      // kind (proto::HurtKind), share and attacker; the health's share; the cursor's x and y; a
	                      // box's x0, y0, x1 and y1
	bool        on = false;  // link, build and hud; a hurt's power attack; wait-screen open
	std::string text;        // the command, the file, the folder, the ground, an actor's name, or the text to type
```
```cpp
std::optional<std::vector<Action>> ParseScript(std::string_view aText, std::string& aError);
```
```cpp
std::optional<std::vector<Action>> ParseScript(std::string_view aText, std::string& aError);

// UTF-8 text's code points, or nothing if it isn't well-formed UTF-8: no overlong forms, surrogates, or code points
// past U+10FFFF.
std::optional<std::vector<uint32_t>> CodePoints(std::string_view aUtf8);
```

- [x] **Step 8: Edit `tools/mclink/McLinkScript.cpp` to parse them, and decode UTF-8**

```cpp
#include "McLinkScript.h"
```
```cpp
#include "McLinkScript.h"

#include <skycraft_protocol.h>
```
```cpp
	return aValue >= 1.0 && aValue <= 4294967295.0 && aValue == std::floor(aValue);
}
}
```
```cpp
	return aValue >= 1.0 && aValue <= 4294967295.0 && aValue == std::floor(aValue);
}

// A whole number from 0 to aMost.
bool WholeUpTo(double aValue, double aMost)
{
	return aValue >= 0.0 && aValue <= aMost && aValue == std::floor(aValue);
}

namespace proto = skycraft::proto;
}

std::optional<std::vector<uint32_t>> CodePoints(std::string_view aUtf8)
{
	constexpr uint32_t    kSmallest[5] = { 0, 0, 0x80, 0x800, 0x10000 };  // by length: anything below is overlong
	std::vector<uint32_t> points;
	size_t                at = 0;
	while (at < aUtf8.size())
	{
		const uint8_t lead = uint8_t(aUtf8[at]);
		const size_t  length = lead < 0x80 ? 1 : (lead & 0xE0) == 0xC0 ? 2 : (lead & 0xF0) == 0xE0 ? 3 : (lead & 0xF8) == 0xF0 ? 4 : 0;
		if (length == 0 || at + length > aUtf8.size())
		{
			return std::nullopt;
		}
		uint32_t point = length == 1 ? lead : lead & (0x7Fu >> length);
		for (size_t i = 1; i < length; ++i)
		{
			const uint8_t next = uint8_t(aUtf8[at + i]);
			if ((next & 0xC0) != 0x80)
			{
				return std::nullopt;
			}
			point = point << 6 | (next & 0x3Fu);
		}
		if (point < kSmallest[length] || point > 0x10FFFF || (point >= 0xD800 && point <= 0xDFFF))
		{
			return std::nullopt;
		}
		points.push_back(point);
		at += length;
	}
	return points;
}
```
```cpp
		else if (verb == "floor" || verb == "scene" || verb == "status" || verb == "quit")
		{
			action.kind = verb == "floor"    ? Action::Kind::Floor
			              : verb == "scene"  ? Action::Kind::Scene
```
```cpp
		else if (verb == "cursor")
		{
			action.kind = Action::Kind::Cursor;
			if (!numbers(2) || !WholeUpTo(action.numbers[0], proto::kMaxOverlayW - 1) || !WholeUpTo(action.numbers[1], proto::kMaxOverlayH - 1))
			{
				return fail("cursor takes whole HUD pixels, x from 0 to 3839 and y from 0 to 2159");
			}
		}
		else if (verb == "text")
		{
			action.kind = Action::Kind::Text;
			action.text = std::string(rest);
			const auto points = CodePoints(action.text);
			if (action.text.empty() || !points ||
			    std::any_of(points->begin(), points->end(), [](uint32_t aPoint) { return aPoint < 0x20 || aPoint == 0x7F; }))
			{
				return fail("text takes printable UTF-8");
			}
		}
		else if (verb == "wait-screen")
		{
			action.kind = Action::Kind::WaitScreen;
			action.numbers[0] = 5;
			action.on = words.size() >= 2 && words[1] == "open";
			bool ok = (words.size() == 2 || words.size() == 3) && (action.on || words[1] == "closed");
			if (ok && words.size() == 3)
			{
				ok = Number(words[2], action.numbers[0]) && action.numbers[0] > 0;
			}
			if (!ok)
			{
				return fail("wait-screen takes open or closed, then an optional number of seconds");
			}
		}
		else if (verb == "hud-box")
		{
			action.kind = Action::Kind::HudBox;
			if (!numbers(4) || !WholeUpTo(action.numbers[0], proto::kMaxOverlayW) || !WholeUpTo(action.numbers[1], proto::kMaxOverlayH) ||
			    !WholeUpTo(action.numbers[2], proto::kMaxOverlayW) || !WholeUpTo(action.numbers[3], proto::kMaxOverlayH) ||
			    action.numbers[0] >= action.numbers[2] || action.numbers[1] >= action.numbers[3])
			{
				return fail("hud-box takes whole HUD pixels x0 y0 x1 y1, x0 below x1 up to 3840, y0 below y1 up to 2160");
			}
		}
		else if (verb == "floor" || verb == "scene" || verb == "pause" || verb == "status" || verb == "quit")
		{
			action.kind = verb == "floor"    ? Action::Kind::Floor
			              : verb == "scene"  ? Action::Kind::Scene
			              : verb == "pause"  ? Action::Kind::Pause
```

- [x] **Step 9: Edit `CMakeLists.txt` to add the source**

`mclink` builds it too, for Task 3.
```cmake
	tools/mclink/Main.cpp
	tools/mclink/DumpObj.cpp
	tools/mclink/McLinkScript.cpp
```
```cmake
	tools/mclink/Main.cpp
	tools/mclink/DumpObj.cpp
	tools/mclink/HudBox.cpp
	tools/mclink/McLinkScript.cpp
```
```cmake
	tests/TestHudBox.cpp
	tools/mclink/DumpObj.cpp
	tools/mclink/McLinkScript.cpp
```
```cmake
	tests/TestHudBox.cpp
	tools/mclink/DumpObj.cpp
	tools/mclink/HudBox.cpp
	tools/mclink/McLinkScript.cpp
```

- [x] **Step 10: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `525 test(s), 0 failure(s)`.

- [x] **Step 11: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-screens: mclink parses cursor, text, pause, wait-screen and hud-box; a HUD box's mean, and UTF-8's code points" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 2: The link keeps Minecraft's GUI scale

Step 0's slot pixels hold only at the user's GUI scale 2 (`guiScale:2` in the instance's `options.txt`), so its
script checks the scale Minecraft really uses, which SkyCraft writes into McState (`guiScale`, from
`Window.getGuiScale`). The link keeps it in its status beside the screen flag, read each update as the rest of McState
is; `mclink`'s status prints it (Task 3).

**Files:**
- Modify: `src/link/Link.h`, `src/link/Link.cpp`, `tests/TestLink.cpp`

- [x] **Step 1: Edit `tests/TestLink.cpp` to add the failing test**

The fake Minecraft's `State` gains the GUI scale, 0 unless given, as SkyCraft's McState starts.

```cpp
	void State(uint32_t aFlags)
```
```cpp
	void State(uint32_t aFlags, uint32_t aGuiScale = 0)
```
```cpp
		state.flags = aFlags;
```
```cpp
		state.flags = aFlags;
		state.guiScale = aGuiScale;
```
```cpp
TEST(LinkPassesOnExplosionsAndHudImages)
```
```cpp
TEST(LinkKeepsMinecraftsScreenAndGuiScale)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld | proto::kMcScreenOpen, 2);
	link.Update(At(1.0), kSettings, sink);
	CHECK(link.State().screenOpen);
	CHECK_EQ(link.State().guiScale, uint32_t(2));
	minecraft.Beat(77, 1100);
	minecraft.State(proto::kMcInWorld, 3);
	link.Update(At(1.1), kSettings, sink);
	CHECK(!link.State().screenOpen);
	CHECK_EQ(link.State().guiScale, uint32_t(3));
}

TEST(LinkPassesOnExplosionsAndHudImages)
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -o -E "error C[0-9]+: [^[]*" | sort -u | head -3
```
Expected: `error C2039: 'guiScale': is not a member of 'cpmc::link::Link::Status'`.

- [x] **Step 3: Edit `src/link/Link.h` to add the field**

```cpp
		uint32_t              minecraftFlags = 0;  // McState's: on ground, sneaking, sprinting, flying and the rest
```
```cpp
		uint32_t              minecraftFlags = 0;  // McState's: on ground, sneaking, sprinting, flying and the rest
		uint32_t              guiScale = 0;  // McState's: window pixels to a GUI pixel, each way (spike 8's step 0)
```

- [x] **Step 4: Edit `src/link/Link.cpp` to read it**

```cpp
		status_.minecraftFlags = mc->flags;
```
```cpp
		status_.minecraftFlags = mc->flags;
		status_.guiScale = mc->guiScale;
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `526 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-screens: the link keeps Minecraft's GUI scale from McState" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 3: `mclink` runs the verbs, and step 0's script

`mclink` runs each verb as BlockRender will send its input:
- **`cursor`** pushes `kInCursor`, its a and b the pixel;
- **`text`** pushes one `kInText` a code point, and says how many it typed and how many a full ring dropped;
- **`pause`** pushes `kInOpenMenu`;
- **`wait-screen`** waits until McState's screen flag says what it asks, then says how long that took; past its time
  it says so and the run's exit code becomes 1, but the script goes on, so its end still clears the test chest;
- **`hud-box`** asks for the next HUD image, as `save-hud` does, and prints the box's mean colour and alpha, or why it
  can't.

Its status line for Minecraft gains the GUI scale.

Step 0's script, `tools/mclink/screens.txt` (the spec's "Step 0", with "Changes while planning Part 1"):
- **Its spot** is spike 7's: the stand-in V at 880.5 6.14 2007.5 on spike 1's dumped ground, with the city's ground.
  It looks east and 9 degrees down, at a chest placed two blocks east at its eye's height (882 7 2007), facing it.
- **Build mode is on** for the screens: with it off, SkyState says a menu is open, and SkyCraft lets go of every key
  and button each frame, so F3 couldn't stay held.
- **The chest:** a stick goes in its slot 0 and a stone in slot 1, with `/item replace`, so nothing of the user's
  inventory is touched. A right-click opens it. At the user's GUI scale 2 the 1,280 by 720 HUD image is 640 by 360 GUI
  pixels, and the chest's screen, 176 by 168, is centred at GUI 232 96: chest slot n (row n / 9, column n % 9) has its
  centre at window pixel 496 + 36 x column, 244 + 36 x row.
- **The hover highlight:** the cursor on empty slot 9, then the boxes of slots 9 and 10 inside their 32 by 32 pixels.
- **The moves:** the stick from slot 0 to slot 4 and the stone from slot 1 to slot 5, a click to pick each up and one
  to put it down; Esc closes the chest; `/execute if items` says in chat where they are.
- **Typing:** T opens chat, `text` types a line with Danish letters and AltGr's characters, and Enter sends it as a
  chat line: `/say` would read the `@` as the start of an entity selector.
- **The switcher:** F3 down, F4, F3 up, twice. It opens on the last game mode, so the first round switches to it and
  the second back.
- **The pause menu** opened by `pause`, and **the inventory** by E; Esc closes each.
- **The end:** `setblock … air strict` removes the chest with what's in it, dropping nothing, and a `/kill` clears any
  item dropped near it, though none should be.

**Files:**
- Create: `tools/mclink/screens.txt`
- Modify: `tools/mclink/Main.cpp`, `tests/TestMcLinkScript.cpp`

- [x] **Step 1: Edit `tests/TestMcLinkScript.cpp` to check the new script**

```cpp
	                          "entities.txt", "spawn.txt", "emissive.txt", "blended.txt" })
```
```cpp
	                          "entities.txt", "spawn.txt", "emissive.txt", "blended.txt", "screens.txt" })
```

- [x] **Step 2: Build and run the tests; the script check must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | grep -v "^PASS" | tail -2
```
Expected: `FAIL ScriptsInTheRepoParse` and `526 test(s), 2 failure(s)`: the new script can't be read yet.

- [x] **Step 3: Edit `tools/mclink/Main.cpp` to run the verbs, and print the GUI scale**

```cpp
#include "DumpObj.h"
```
```cpp
#include "DumpObj.h"
#include "HudBox.h"
```
```cpp
	Say("Minecraft: {}{}{}, feet {:.3f} {:.3f} {:.3f}, eye {:.3f} ({:.2f} above the feet), frame {}",
	    s.minecraft ? std::format("process {}", s.minecraftPid) : "absent", s.inWorld ? ", in its world" : "",
	    s.screenOpen ? ", a screen open" : "", s.minecraftFeet.x, s.minecraftFeet.y, s.minecraftFeet.z, s.minecraftEye.y,
```
```cpp
	Say("Minecraft: {}{}{}, GUI scale {}, feet {:.3f} {:.3f} {:.3f}, eye {:.3f} ({:.2f} above the feet), frame {}",
	    s.minecraft ? std::format("process {}", s.minecraftPid) : "absent", s.inWorld ? ", in its world" : "",
	    s.screenOpen ? ", a screen open" : "", s.guiScale, s.minecraftFeet.x, s.minecraftFeet.y, s.minecraftFeet.z, s.minecraftEye.y,
```
```cpp
			case Action::Kind::SaveHud:
				report.wantHud = true;
				report.hudReady = false;
				break;
```
```cpp
			case Action::Kind::SaveHud:
			case Action::Kind::HudBox:
				report.wantHud = true;
				report.hudReady = false;
				break;
			case Action::Kind::Cursor:
				link.Push({ proto::kInCursor, 0, int32_t(action.numbers[0]), int32_t(action.numbers[1]), 0 });
				break;
			case Action::Kind::Text:
			{
				// The parser checked the text, so it decodes.
				const std::vector<uint32_t> points = mclink::CodePoints(action.text).value_or(std::vector<uint32_t>{});
				size_t                      dropped = 0;
				for (const uint32_t point : points)
				{
					dropped += link.Push({ proto::kInText, 0, int32_t(point), 0, 0 }) ? 0 : 1;
				}
				Say("text: {} characters typed{}", points.size(), dropped ? std::format("; {} DROPPED, the input ring is full", dropped) : "");
				break;
			}
			case Action::Kind::Pause: link.Push({ proto::kInOpenMenu, 0, 0, 0, 0 }); break;
```
```cpp
			break;
		case Action::Kind::HoldCheck:
```
```cpp
			break;
		case Action::Kind::WaitScreen:
			done = state.screenOpen == action.on;
			if (done)
			{
				Say("{} after {:.2f} s", action.on ? "a screen is open" : "no screen is open", elapsed);
			}
			else if (elapsed > action.numbers[0])
			{
				Say("line {}: {} within {} s", action.line, action.on ? "no screen opened" : "the screen didn't close", action.numbers[0]);
				exitCode = 1;
				done = true;
			}
			break;
		case Action::Kind::HudBox:
			done = report.hudReady;
			if (done)
			{
				const uint32_t x0 = uint32_t(action.numbers[0]);
				const uint32_t y0 = uint32_t(action.numbers[1]);
				const uint32_t x1 = uint32_t(action.numbers[2]);
				const uint32_t y1 = uint32_t(action.numbers[3]);
				if (const auto mean = mclink::MeanOfBox(report.hud, report.hudWidth, report.hudHeight, report.hudBottomUp, x0, y0, x1, y1))
				{
					Say("HUD box {} {} to {} {}: mean colour {:.1f} {:.1f} {:.1f}, alpha {:.1f}, over {} pixels", x0, y0, x1, y1, mean->r, mean->g,
					    mean->b, mean->a, mean->pixels);
				}
				else
				{
					Say("line {}: the box {} {} to {} {} isn't inside the {} x {} HUD image", action.line, x0, y0, x1, y1, report.hudWidth,
					    report.hudHeight);
					exitCode = 1;
				}
			}
			else if (elapsed > 5.0)
			{
				Say("line {}: no HUD image within 5 s", action.line);
				exitCode = 1;
				done = true;
			}
			break;
		case Action::Kind::HoldCheck:
```

- [x] **Step 4: Write step 0's script, `tools/mclink/screens.txt`**

```text
# Spike 8's step 0: Minecraft's side of its screens worked from the game, against SkyCraft's real mod, without the
# game. mclink sends what BlockRender will: a cursor in the HUD image's pixels, clicks, keys, typed text, and the pause
# menu's kInOpenMenu. It asks Minecraft for a 1280 x 720 HUD image, which at the user's GUI scale 2 is 640 by 360 GUI
# pixels: a chest's screen, 176 by 168, is centred at GUI 232 96, so chest slot n (row n / 9, column n % 9) has its
# centre at window pixel 496 + 36 x column, 244 + 36 x row. Build mode is on throughout: with it off, SkyState says a
# menu is open, and SkyCraft lets go of every key and button each frame, so F3 couldn't stay held for the switcher.
# The test chest stands at 882 7 2007, two blocks east of the stand-in V at its eye's height, with a stick and a stone
# in it: nothing of the user's inventory is touched, and the switcher's two rounds leave the game mode as it was.
# Start Minecraft first; this waits for its world.
status
wait-world 180
ground city
at 880.5 6.14 2007.5
look 270 9
dump E:\Cyberpunk2077\red4ext\plugins\PhysXProbe\dumps\2026-10-01_14-37-07\collision.obj
link on
wait-ground 30
wait 2
# The chest, facing the stand-in V, a stick in its slot 0 and a stone in slot 1.
type /setblock 882 7 2007 minecraft:chest[facing=west]
type /item replace block 882 7 2007 container.0 with minecraft:stick
type /item replace block 882 7 2007 container.1 with minecraft:stone
build on
wait 2
# A right-click opens it: the stand-in V looks east and 9 degrees down, at the chest's middle.
click right
wait-screen open
status
save-hud build\step0\chest.dds
# The hover highlight: the cursor on empty slot 9 (row 1, column 0), beside empty slot 10, which isn't hovered.
cursor 496 280
wait 0.5
hud-box 484 268 508 292
hud-box 520 268 544 292
# The stick from slot 0 to slot 4, and the stone from slot 1 to slot 5: a click picks each up, a click puts it down.
cursor 496 244
wait 0.3
click left
wait 0.3
cursor 640 244
wait 0.3
click left
wait 0.3
cursor 532 244
wait 0.3
click left
wait 0.3
cursor 676 244
wait 0.3
click left
wait 0.5
save-hud build\step0\moved.dds
# Esc closes the chest; the typer types only with no screen open.
key 41
wait-screen closed
type /execute if items block 882 7 2007 container.4 minecraft:stick run say step0: the stick is in slot 4
type /execute if items block 882 7 2007 container.5 minecraft:stone run say step0: the stone is in slot 5
type /execute unless items block 882 7 2007 container.0 * unless items block 882 7 2007 container.1 * run say step0: slots 0 and 1 are empty
# Typing: T opens chat, and text types into it, Danish letters and AltGr's characters included; Enter sends it as a
# chat line, not a command, since /say would read the @ as the start of an entity selector.
key 23
wait-screen open
text step0: typed æøå é @ ~
key 40
wait-screen closed
# The switcher: F3 held, F4 opens it on the last game mode, and letting go of F3 switches to that. Twice: back again.
down 60
wait 0.3
key 61
wait-screen open
save-hud build\step0\switcher.dds
up 60
wait-screen closed
wait 1
down 60
wait 0.3
key 61
wait-screen open
up 60
wait-screen closed
wait 1
# The pause menu, opened as O will open it, and closed by Esc.
pause
wait-screen open
save-hud build\step0\pause.dds
key 41
wait-screen closed
# The inventory: E opens it, Esc closes it.
key 8
wait-screen open
save-hud build\step0\inventory.dds
key 41
wait-screen closed
# The end: the chest goes with what's in it, since strict drops nothing, and any item dropped near it is cleared.
build off
type /setblock 882 7 2007 minecraft:air strict
type /kill @e[type=minecraft:item,x=882,y=7,z=2007,distance=..8]
wait 2
scene
status
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `526 test(s), 0 failure(s)`.

- [x] **Step 6: Run mclink without Minecraft**

Minecraft and the game must not be running.
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && printf 'status\nquit\n' > build/selfcheck.txt \
  && ./build/RelWithDebInfo/mclink.exe build/selfcheck.txt; echo "exit: $?"
```
Expected: `the mapping: created (process …); waiting for Minecraft`, then the status, with `Minecraft: absent, GUI
scale 0`. Then `exit: 0`.

- [x] **Step 7: Run the new verbs without Minecraft**

With nothing to answer them, each verb must still run to its end: the text is pushed, the waits and the box give up.
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render \
  && printf 'cursor 10 10\ntext abc \xc3\xa6\npause\nwait-screen open 0.5\nwait-screen closed 0.5\nhud-box 0 0 4 4\nquit\n' > build/verbcheck.txt \
  && ./build/RelWithDebInfo/mclink.exe build/verbcheck.txt | grep -E "text:|screen|HUD"; echo "exit: ${PIPESTATUS[0]}"
```
Expected: `text: 5 characters typed`, `line 4: no screen opened within 0.5 s`, `no screen is open after 0.00 s`,
`line 6: no HUD image within 5 s`, then `exit: 1`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-screens: mclink runs the screens' verbs and shows Minecraft's GUI scale; step 0's script" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 4: In Minecraft: step 0

Step 0 of the spec's "Testing", with the user: Minecraft's side of its screens worked from the game, from `mclink`,
against SkyCraft's unchanged mod in the `CyberCraft` instance, without the game.
- **The instance** has SkyCraft 0.1.2, Fabric API and CyberCraft Tweaks 1.6.0, and the Java arguments
  `--enable-native-access=ALL-UNNAMED -Dskycraft.quitWithSkyrim=false`, so Minecraft keeps running between `mclink`
  runs. The world allows commands, which the switcher needs.
- **What Minecraft logs:** the chat's lines go into Minecraft's log after `[CHAT]`: `/execute`'s `say` lines, the chat
  line typed, the switcher's `Set own game mode to … Mode`, and any command's error.
- **If Minecraft doesn't link,** its log says why, in lines starting `SkyCraft:`.
- **If a check fails,** stop and find the cause with superpowers:systematic-debugging before going on. Fixes become new
  tasks.
  - **SkyCraft logs its first 20 clicks** of a Minecraft session: `SkyCraft: click <button> -> <what it hit> …
    (grabbed …, screen …)`. If the chest doesn't open, the right-click's line says what it hit: `MISS` means the look
    missed the chest.
  - **If a `wait-screen` gives up,** the script goes on to its end, but a screen left open would stop the typer's last
    commands (`not typed: a Minecraft screen is open`): then ask the user to close it in Minecraft's window, and run
    the end's commands again with a script of their own.

**Files:**
- Create: `docs/spikes/2026-10-07-minecraft-screens-results.md`, `docs/spikes/2026-10-07-minecraft-screens-results/`
  (the HUD images, as PNG)

- [x] **Step 1: Check that the game isn't running**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game: it would hold the mapping.

- [x] **Step 2: Ask the user to start Minecraft**

With their leave to place a test chest at 882 7 2007 in the `SkyCraft` world, removed with what's in it at the
script's end, and to switch the game mode twice with F3+F4, which leaves it as it was, the user starts the `CyberCraft`
instance from the Modrinth App, and says when its title screen shows. Then:
```bash
grep -E "SkyCraft:" "/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log" | tail -3
```
Expected: `SkyCraft: can't open Skyrim's shared memory yet (Windows error 2: …)`: waiting for a game side.

- [x] **Step 3: Run step 0's script**

```bash
L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"; before=$(wc -l < "$L"); \
  cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/step0 \
  && ./build/RelWithDebInfo/mclink.exe tools/mclink/screens.txt | tee build/step0/screens.log; echo "exit: ${PIPESTATUS[0]}"; \
  tail -n +$((before + 1)) "$L" | grep -E "\[CHAT\]|SkyCraft: |WARN" | tee build/step0/minecraft.log
```
It takes about a minute once Minecraft is in its world, and up to four if it opens the world first: give the command 6
minutes. Expected, against the spec's "Step 0":
- **Linking:** `Minecraft is there (process …)`, `clear all`, the atlas, `HUD images: 1280 x 720`, then `the city
  around region … is all in the ring`.
- **The chest:**
  - `a screen is open after …` within a second of the right-click;
  - in Minecraft's log, SkyCraft's `click 3 -> BLOCK` line for the right-click, with `grabbed true, screen null`;
  - then the status says `a screen open, GUI scale 2`;
  - the HUD image is saved with nearly all of its 921,600 pixels not transparent: an open screen darkens the whole
    window behind it, with a gradient at alpha 192 to 208. Spike 4 found one pixel with a colour above its alpha, at
    an edge of the hand; a few such are no fault.
- **The hover highlight:** `HUD box 484 268 to 508 292` (slot 9, hovered) with a mean colour of about 192 in each
  channel (the slot's grey 139 under the highlight's white at alpha 96, then at alpha 32), and `HUD box 520 268 to 544
  292` (slot 10) with about 139; both with alpha 255, over 576 pixels.
- **The moves:**
  - SkyCraft's four `click 1` lines, each with `grabbed false` and the chest's `ContainerScreen`;
  - after Esc, `no screen is open after …`;
  - in Minecraft's chat, `step0: the stick is in slot 4`, `step0: the stone is in slot 5` and `step0: slots 0 and 1
    are empty`.
- **Typing:** `text: 22 characters typed`, the chat opening and closing, and in Minecraft's chat the line `<SirBloxy>
  step0: typed æøå é @ ~`, its letters whole.
- **The switcher:** a screen open and closed twice, and two `Set own game mode to … Mode` lines in Minecraft's chat,
  the second naming the game mode the player had before.
- **The pause menu and the inventory:** each opens a screen, and Esc closes it.
- **The end:** in Minecraft's chat `No entity was found` (nothing was dropped); the last scene with no batches, and
  `world: nothing`.
- **No other `[CHAT]` line with an error, no `WARN` line from a command,** and `exit: 0`.

- [x] **Step 4: Ask the user to stop Minecraft**

Its window is hidden while `mclink` drives it, so the user stops it from the Modrinth App.

- [x] **Step 5: Save the HUD images as PNG, and look at them**

```bash
cd /d/Cyberpunk-Minecraft && mkdir -p docs/spikes/2026-10-07-minecraft-screens-results \
  && python -c "
from PIL import Image
for name in ('chest', 'moved', 'switcher', 'pause', 'inventory'):
    Image.open(f'spikes/block-render/build/step0/{name}.dds').save(f'docs/spikes/2026-10-07-minecraft-screens-results/{name}.png')
    print(name, 'saved')"
```
Expected: `chest saved` and the four others. Look at each: the chest's screen with the stick in slot 0 and the stone
in slot 1; after the moves, the stick in slot 4 and the stone in slot 5; the switcher's four icons; the pause menu;
the inventory (the creative one, in creative).

- [x] **Step 6: Write step 0's results, `docs/spikes/2026-10-07-minecraft-screens-results.md`**

From `build/step0/screens.log`, `build/step0/minecraft.log` and the PNG images, as spike 7's results doc does: the
versions; each check above with its outcome and numbers (the boxes' means, how long each screen took to open and
close, the chat's lines); anything that surprised, and what it means for Part 2. The spike's answer goes at the top
once run E is in.

- [x] **Step 7: Commit**

With the plan, its steps ticked up to this one: running Tasks 1 to 3 ticked theirs.
```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-07-minecraft-screens-results.md docs/spikes/2026-10-07-minecraft-screens-results docs/superpowers/plans/2026-10-07-minecraft-screens.md \
  && git commit -q -m "minecraft-screens: step 0's results, and Part 1's steps ticked" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 5: Close Part 1, and plan Part 2

- [x] **Step 1: Record how Part 1 went, in this plan**

A section "How Part 1 went" at the plan's end: what ran as planned, what didn't and why, and what step 0's results
change for Part 2.

- [x] **Step 2: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/superpowers/plans/2026-10-07-minecraft-screens.md \
  && git commit -q -m "minecraft-screens plan: how Part 1 went" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

- [x] **Step 3: Record Part 2's plan**

With superpowers:writing-plans, from the spec and step 0's results: BlockRender 0.23.0 (build mode's screen state and
keys, the cursor and its arrow, typed text, the status line), then run E with the user. Part 2's tasks follow this
one, and the spec gets "Changes while planning Part 2". The user reviews it before it runs.

---

## Found while planning Part 1

The spec's "Changes while planning Part 1" lists what planning settled in the design. In the plan itself:
- **Two more verbs than the spec named:** `wait-screen`, so the script waits for each screen rather than for a guess at
  Minecraft's time, and `hud-box`, so the hover check is a number in the log, not a look at an image.
- **The link keeps the GUI scale** (Task 2): step 0's pixels depend on it, and nothing read it.
- **A chest, not the inventory, for the cursor and the moves.** The spec had the inventory, with test items in slots
  found empty first and the game mode read and set back. A chest needs neither: its slots are its own, filled by
  `/item replace block`, and the switcher's two rounds leave the game mode as it was. It also tests what a right-click
  opens. E still opens the inventory, without moves.
- **The chest screen's pixels,** from 26.3's classes: `ContainerScreen` is 176 by 114 + 18 a row (168 for a chest),
  `AbstractContainerScreen.init` centres it, and `ChestMenu.addChestGrid` puts slot n at GUI 8 + 18 x column, 18 + 18 x
  row from its corner. The slot's grey is 139 (`generic_54.png`), and the hover highlight is two 24 by 24 sprites of
  white, at alpha 96 (`slot_highlight_back`) and 32 (`slot_highlight_front`), which make about 192.
- **The typed line is chat, not `/say`:** `/say` reads `@` as the start of an entity selector.
- **The user's player is `SirBloxy`,** and the switcher's feedback is `Set own game mode to … Mode`, from earlier logs.
- **`/execute if items` and `unless items`** take a block's `container.<n>` slots, and `*` for any item, in 26.3.
- **A test that caught too little:** in the first run of the mutations, `CodePoints` accepting a lead byte where a
  continuation belongs went uncaught; `CodePointsDecodeUtf8` now checks `C3 C3`, and all 23 are caught.

## Part 1's deep review

The user asked for a deep self-review of the plan once it was written. It traced each claim step 0 rests on to 26.3's
classes, SkyCraft's source, `mclink`'s and the link's code, the user's settings and earlier scripts, and walked through
`screens.txt` line by line as Minecraft would take it.

### What changed

- **Task 4's checks name SkyCraft's own click lines.** SkyCraft logs its first 20 clicks of a session with what each
  hit and whether a screen was open; the right-click must hit the chest (`BLOCK`, `screen null`), and the four left
  clicks land in its `ContainerScreen`. If the chest doesn't open, that line says why (`MISS`: the look missed it).
- **The chest image's check is a number:** nearly all of its 921,600 pixels not transparent, since an open screen
  darkens the whole window. It was "most of its middle", which `mclink` doesn't measure; and a few pixels with a colour
  above their alpha are no fault, as spike 4 found one.

### What was checked

- **Minecraft 26.3:**
  - `Minecraft.isWindowActive` is `Window.isFocused`, which SkyCraft makes true while linked. So `MouseHandler` grabs
    the mouse after a screen closes, and handles movement each frame, drags included (Part 2's run E needs them);
  - `MouseHandler.onMove` keeps the position whatever the focus, and a click uses the position at once, so a `cursor`
    just before a `click` is where it lands;
  - the switcher opens only for a player allowed `/gamemode` (`GameModeCommand.PERMISSION_CHECK`), which the user's
    world gives: its logs have `Set own game mode to Creative Mode` from earlier sessions;
  - it switches in `keyReleased`, on the debug modifier's key, so F3's release must reach Minecraft as a key (Part 2's
    holders send it where the press went);
  - the two highlight sprites cover exactly the slot's 16 by 16 inside (`slot.x - 4`, 24 by 24, clear but for the
    middle), so slot 10's box sees none of slot 9's highlight;
  - a screen in game draws a full-window gradient, `0xC0101010` to `0xD0101010`, or the blurred menu background;
  - a right-click on a chest opens it whatever the player holds, since the block's use comes before the item's.
- **SkyCraft:** `InputBridge` logs clicks as above, takes `kInCursor` as `onMove`'s window position, and refuses
  `kInText` with no screen open; `SkyClient` sets the player's look from SkyState only with no screen open.
- **The link and `mclink`:**
  - SkyState's look comes from `mclink`'s `look` every frame, build mode or not;
  - right-clicks in build mode placed blocks in spike 5's `drive.txt`, so `click right` there works;
  - `mclink`'s DDS images open in PIL (one of spike 7's textures, as `DDS RGBA`);
  - the run's lines quoted in Task 4 are the ones `mclink` prints.
- **The user's instance:** Minecraft 26.3, Fabric Loader 0.19.5, SkyCraft 0.1.2 and CyberCraft Tweaks 1.6.0 in its
  latest log, and SkyCraft's waiting line as Task 4 quotes it.
- **The scenarios where a step fails:** a chest that doesn't open leaves the left clicks to the world; in creative
  one could break the chest, whose stick and stone would drop, and the end's `/kill` clears them. An Esc with no
  screen opens the pause menu, which stops the typer's last commands; Task 4 says what to do then.

## How Part 1 went

Part 1 ran on 2026-10-07, after the plan's deep review, as the user said to start.
- **Tasks 1 to 3** ran in the real repo through the plan runner, task range 1 to 3: every step's expected result held
  (each build's failure, 525, 526 and 526 tests with no warnings, `mclink`'s self-check and its new verbs' dry run).
  - **Commits:** `793d8c4`, `53f5c0d` and `1d946dc`, each tree identical to the scratch clone's `p1-t1` to `p1-t3`.
  - **The tags:** comparing the trees fetched the clone's tags into the repo; they were deleted again.
- **Task 4, step 0,** passed every check (`f60210b`, results in
  [2026-10-07-minecraft-screens-results.md](../../spikes/2026-10-07-minecraft-screens-results.md)):
  - the right-click hit the chest at height 7.51, where the review worked out the look would, and its screen opened
    in 0.10 s;
  - the hover check gave exactly 192.0 on the hovered slot and 139.0 beside it;
  - the moves, the typed line (`æøå é @ ~` whole), the switcher's two rounds (Survival, then Creative again), the pause
    menu and the inventory all went as planned;
  - the end left nothing.
- **What didn't go as planned:**
  - **One expectation was wrong:** the instance's player is `ItzBloxy`, not `SirBloxy`, from older logs.
  - **Steps 5 and 6** (the images as PNG, the results) ran while the user stopped Minecraft, before step 4 was ticked;
    they don't need Minecraft.
- **For Part 2,** step 0 settles:
  - the cursor's pixels are the HUD image's, at the user's GUI scale 2;
  - screens open 0.00 to 0.10 s after their input, as McState reports them;
  - a container screen darkens the whole window, the pause menu blurs it and lays the menu background over it, and
    the switcher draws nothing behind itself;
  - `kInText` delivers any code point whole.

  Nothing in the spec's design for Part 2 changes.

---

## Part 2: BlockRender 0.23.0, and run E

**Goal:** Part 2 of spike 8: Minecraft's screens worked from the game in build mode, as the spec's "Where each input
goes", "The cursor" and "Typed text" have them, then run E in the game with the user. Step 0 settled Minecraft's side:
the cursor's pixels are the HUD image's, screens open 0.00 to 0.10 s after their input, and `kInText` delivers any code
point whole ("How Part 1 went").

**Architecture:** the pure parts are plain C++ in `render_core`, tested without the game; the patch and the wiring are
in `render_runtime` and `Plugin.cpp`, which only the game tests.
- **`input/Cursor`:** Windows' pointer speed table; the cursor's position in back-buffer pixels, centred as a screen
  opens, kept in the HUD image, and its image pixel, for `kInCursor`, only when that pixel changes.
- **`input/Text`:** WM_CHAR's UTF-16 code units made into code points, lone surrogates refused, and `Printable`.
- **`render/Hud`:** the arrow's height and shape (`CursorHeight`, `CursorAt`, which `HudPS` mirrors), and `HudCursor`,
  the spot the router writes and the HUD pass reads, in one atomic.
- **`input/BuildMode`:** holders for every key; E, T, /, Q and F3 Minecraft's; every key while F3's press is
  Minecraft's; O sends `kInOpenMenu`; with a Minecraft screen open, every input Minecraft's; no latency mark, and the
  slot readout kept, in a screen.
- **`input/Router`:** while a screen is build mode's, the mouse's movement moves the cursor and is hidden from the game,
  typed characters go as `kInText`, and `TakeMouseX` gives 0; build mode ending with a screen open sends Esc, if
  Minecraft is linked, in its world and alive; Windows' speed is read again as a screen opens.
- **The HUD pass:** `HudConstants` grows to 8 values, and `HudPS` draws the arrow before the image.
- **`input/MessageHook`:** the executable's `PeekMessageW` import patched, each WM_CHAR taken off the queue to the
  router.
- **BlockRender 0.23.0:** the hook installed and removed, the HUD image's size and the cursor's spot shared, and a
  `screens=` status line.

---

## Before you start (Part 2)

As Part 1's "Before you start", with these:
- **Already verified:** Part 2's code was compiled and tested in the scratch clone, Tasks 6 to 13 replayed in a fresh
  clone exactly as written (every build, test and failure checked, the files identical to the clone's), and each new
  test broken once by a mutation of the code it covers: 52 mutations, all caught.
- **Test counts:** 526 at the start, then 531, 534, 537, 543, 550, 550, 550 and 551 after Tasks 6 to 13.
- **The game:** builds deploy only with `CP2077_DIR` set, which only Task 14's deploy step sets, with the game closed.
- **What only the game can test:** the patch, the arrow, and how it all feels, in Task 14's run E, with the user.
- **The user's world:** run E places a crafting table and a chest at markers tagged `sp8b`, removed at its end without
  updates (`strict`), so they drop nothing, and clears any item dropped near V.

## File map (Part 2)

All paths are relative to `spikes/block-render/`.

| File | Responsibility |
|---|---|
| `CMakeLists.txt` | The new sources and tests |
| `src/input/Cursor.*` | `PointerFactor`; `Cursor`: the position, its area, centring, and the image pixel |
| `src/input/Text.*` | `Utf16Text`, `Printable` |
| `src/render/Hud.*` | `CursorHeight`, `CursorAt`, `HudCursor` |
| `src/input/BuildMode.*` | The screen state's routing, holders for every key, the new keys, the F3 rule, O |
| `src/input/Router.*` | The cursor, typed text, the Esc as build mode ends, `ReadPointerSpeed`, the report |
| `src/render/FrameConstants.h`, `shaders/blocks.hlsl`, `src/render/Renderer.*` | The arrow in the HUD pass |
| `src/input/MessageHook.*` | The `PeekMessageW` patch |
| `src/Status.*`, `src/Plugin.cpp` | The `screens=` line; the wiring; 0.23.0 |
| `tests/TestCursor.cpp`, `tests/TestText.cpp` | The new units' tests |
| `tests/TestHud.cpp`, `tests/TestBuildMode.cpp`, `tests/TestRouter.cpp`, `tests/TestStatus.cpp` | Their new tests |

---

### Task 6: The cursor

The spec's "Moving it" and "Where it starts":
- **`PointerFactor`:** back-buffer pixels a raw count, by Windows' table with "Enhance pointer precision" off (1/32 and
  1/16 at speeds 1 and 2, eighths up to 1 at 10, quarters up to 3.5 at 20); a speed out of range counts as 10's.
- **`Cursor`:** its position in back-buffer pixels, fractions kept, clamped inside the area the HUD image covers (its
  size times its scale) and never on the far edge. As Minecraft's flag goes from clear to set it goes to the image's
  centre pixel, as `MouseHandler.releaseMouse` puts Minecraft's position (the window's size over 2, rounded down);
  from one screen straight to another it stays. `Move` says the image pixel only when it changed. With no image yet it
  goes nowhere.

**Files:**
- Create: `src/input/Cursor.h`, `src/input/Cursor.cpp`, `tests/TestCursor.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestCursor.cpp`**

```cpp
#include "Harness.h"

#include "input/Cursor.h"

using namespace cpmc::input;

namespace
{
// The user's: a 1280 by 720 HUD image drawn at twice its size on a 2560 by 1440 back buffer.
Cursor OnTheUsersScreen()
{
	Cursor cursor;
	cursor.SetArea(1280, 720, 2);
	return cursor;
}

bool At(const Pixel& aPixel, uint32_t aX, uint32_t aY)
{
	return aPixel.x == aX && aPixel.y == aY;
}
}

TEST(CursorFollowsWindowsPointerSpeed)
{
	// Windows' factors with "Enhance pointer precision" off, by speed 1 to 20.
	const double factors[] = { 1.0 / 32, 1.0 / 16, 1.0 / 8, 2.0 / 8, 3.0 / 8, 4.0 / 8, 5.0 / 8, 6.0 / 8, 7.0 / 8, 1.0,
		                       1.25,     1.5,      1.75,    2.0,     2.25,    2.5,     2.75,    3.0,     3.25,    3.5 };
	for (int speed = 1; speed <= 20; ++speed)
	{
		CHECK_EQ(PointerFactor(speed), factors[speed - 1]);
	}
	CHECK_EQ(PointerFactor(0), 1.0);  // out of range: the default's
	CHECK_EQ(PointerFactor(21), 1.0);
}

TEST(CursorStartsAtTheCentreAsAScreenOpens)
{
	Cursor cursor = OnTheUsersScreen();
	cursor.SetScreen(true);
	CHECK(At(cursor.Image(), 640, 360));  // as Minecraft puts its own position: the window's size over 2
	CHECK(At(cursor.Back(), 1280, 720));
	cursor.Move(100, 50, 1.0);
	cursor.SetScreen(true);  // still open, or one screen straight after another: it stays
	CHECK(At(cursor.Image(), 690, 385));
	cursor.SetScreen(false);
	CHECK(At(cursor.Image(), 690, 385));  // closed: it stays where it was, unseen
	cursor.SetScreen(true);
	CHECK(At(cursor.Image(), 640, 360));
	Cursor odd;
	odd.SetArea(1281, 721, 1);
	odd.SetScreen(true);
	CHECK(At(odd.Image(), 640, 360));  // Minecraft's 640.5 and 360.5, in whole pixels
}

TEST(CursorMovesInBackBufferPixelsAndCarriesTheFractions)
{
	Cursor cursor = OnTheUsersScreen();
	cursor.SetScreen(true);
	CHECK(!cursor.Move(1, 0, 0.75).has_value());  // 1280.75: still image pixel 640
	CHECK(!cursor.Move(1, 0, 0.75).has_value());  // 1281.5
	const auto moved = cursor.Move(1, 0, 0.75);   // 1282.25: pixel 641
	CHECK(moved.has_value() && At(*moved, 641, 360));
	CHECK(At(cursor.Back(), 1282, 720));
	const auto down = cursor.Move(-3, 4, 0.75);  // back to 1280, and 723
	CHECK(down.has_value() && At(*down, 640, 361));
	CHECK(!cursor.Move(0, 0, 0.75).has_value());
}

TEST(CursorStaysInTheImage)
{
	Cursor cursor = OnTheUsersScreen();
	cursor.SetScreen(true);
	const auto corner = cursor.Move(-100000, -100000, 1.0);
	CHECK(corner.has_value() && At(*corner, 0, 0));
	const auto far = cursor.Move(1000000, 1000000, 1.0);
	CHECK(far.has_value() && At(*far, 1279, 719));
	CHECK(cursor.Back().x < 2560 && cursor.Back().y < 1440);
	cursor.SetArea(640, 360, 4);  // hudscale 4: the same screen, a smaller image
	CHECK(At(cursor.Image(), 639, 359));
	cursor.SetArea(320, 180, 4);  // and a smaller screen
	CHECK(At(cursor.Image(), 319, 179));
	CHECK(At(cursor.Back(), 1279, 719));
}

TEST(CursorWithoutAnImageGoesNowhere)
{
	Cursor cursor;
	CHECK(!cursor.HasArea());
	cursor.SetScreen(true);
	CHECK(!cursor.Move(10, 10, 1.0).has_value());
	CHECK(At(cursor.Image(), 0, 0));
	cursor.SetArea(0, 0, 2);  // Minecraft has sent no HUD image yet
	CHECK(!cursor.HasArea());
	cursor.SetArea(1280, 720, 0);  // a scale of 0 counts as 1
	CHECK(cursor.HasArea());
	const auto moved = cursor.Move(5, 5, 1.0);
	CHECK(moved.has_value() && At(*moved, 5, 5));
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestRouter.cpp
```
```cmake
	tests/TestRouter.cpp
	tests/TestCursor.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -o -E "error C[0-9]+: [^[]*" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'input/Cursor.h'`.

- [x] **Step 4: Write `src/input/Cursor.h`**

```cpp
#pragma once

#include <cstdint>
#include <optional>

// The cursor BlockRender keeps while a Minecraft screen is open in build mode (spike 8's "The cursor"): moved by the
// mouse's raw counts at Windows' pointer speed, kept in back-buffer pixels with their fractions, clamped to the area the
// HUD image covers, and given to Minecraft in the image's pixels, which are its window's. Pure, and not thread-safe: the
// router holds its lock around every call.
namespace cpmc::input
{
// Windows' pointer speed (SPI_GETMOUSESPEED, 1 to 20, 10 by default), and whether "Enhance pointer precision" is on.
struct PointerSpeed
{
	int  speed = 10;
	bool enhanced = false;
};

// Back-buffer pixels a raw count, by Windows' table with "Enhance pointer precision" off: 1/32 and 1/16 at 1 and 2, then
// eighths up to 1 at 10, then quarters up to 3.5 at 20. Its curve, when it's on, isn't followed. A speed out of range
// counts as the default's.
double PointerFactor(int aSpeed);

// A pixel, whole, from the top left.
struct Pixel
{
	uint32_t x = 0;
	uint32_t y = 0;

	bool operator==(const Pixel&) const = default;
};

class Cursor
{
public:
	// The HUD image's size, and the back-buffer pixels each of its texels covers each way: the area the cursor stays in.
	// The position is clamped into a new area. A size of 0 leaves no area, and the cursor nowhere; a scale of 0 counts
	// as 1.
	void SetArea(uint32_t aImageWidth, uint32_t aImageHeight, uint32_t aScale);
	// Minecraft's screen flag, each frame. Going from clear to set puts the cursor at the image's centre, as Minecraft's
	// own MouseHandler.releaseMouse puts its position as a screen opens; from one screen straight to another it stays.
	void SetScreen(bool aOpen);
	// A mouse event's raw counts, at aFactor back-buffer pixels a count: the image pixel it moved to, if that changed.
	std::optional<Pixel> Move(int32_t aDx, int32_t aDy, double aFactor);

	bool     HasArea() const { return width_ > 0 && height_ > 0; }
	Pixel    Image() const;  // in the image's pixels, for Minecraft
	Pixel    Back() const;   // in back-buffer pixels, for the arrow
	uint32_t ImageWidth() const { return width_; }
	uint32_t ImageHeight() const { return height_; }

private:
	void Clamp();

	uint32_t width_ = 0;  // the image's
	uint32_t height_ = 0;
	uint32_t scale_ = 1;
	double   x_ = 0.0;  // back-buffer pixels
	double   y_ = 0.0;
	bool     screen_ = false;
};
}
```

- [x] **Step 5: Write `src/input/Cursor.cpp`**

```cpp
#include "input/Cursor.h"

#include <algorithm>
#include <cmath>

namespace cpmc::input
{
double PointerFactor(int aSpeed)
{
	if (aSpeed < 1 || aSpeed > 20)
	{
		return 1.0;
	}
	if (aSpeed <= 2)
	{
		return aSpeed == 1 ? 1.0 / 32.0 : 1.0 / 16.0;
	}
	if (aSpeed <= 10)
	{
		return (aSpeed - 2) / 8.0;
	}
	return 1.0 + (aSpeed - 10) / 4.0;
}

void Cursor::SetArea(uint32_t aImageWidth, uint32_t aImageHeight, uint32_t aScale)
{
	width_ = aImageWidth;
	height_ = aImageHeight;
	scale_ = std::max(aScale, 1u);
	Clamp();
}

void Cursor::SetScreen(bool aOpen)
{
	if (aOpen && !screen_ && HasArea())
	{
		x_ = double(width_ / 2) * scale_;
		y_ = double(height_ / 2) * scale_;
	}
	screen_ = aOpen;
}

std::optional<Pixel> Cursor::Move(int32_t aDx, int32_t aDy, double aFactor)
{
	if (!HasArea())
	{
		return std::nullopt;
	}
	const Pixel before = Image();
	x_ += aDx * aFactor;
	y_ += aDy * aFactor;
	Clamp();
	const Pixel after = Image();
	if (after == before)
	{
		return std::nullopt;
	}
	return after;
}

Pixel Cursor::Image() const
{
	if (!HasArea())
	{
		return {};
	}
	return { std::min(uint32_t(x_ / scale_), width_ - 1), std::min(uint32_t(y_ / scale_), height_ - 1) };
}

Pixel Cursor::Back() const
{
	return { uint32_t(x_), uint32_t(y_) };
}

void Cursor::Clamp()
{
	if (!HasArea())
	{
		x_ = 0.0;
		y_ = 0.0;
		return;
	}
	// Inside the area's last pixel, never on its far edge.
	x_ = std::clamp(x_, 0.0, std::nextafter(double(width_) * scale_, 0.0));
	y_ = std::clamp(y_, 0.0, std::nextafter(double(height_) * scale_, 0.0));
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/input/Router.cpp
```
```cmake
	src/input/Router.cpp
	src/input/Cursor.cpp
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `531 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-screens: the cursor, at Windows' pointer speed, centred as a screen opens and kept in the HUD image" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 7: Typed text

The spec's "Typed text": `Utf16Text` takes WM_CHAR's code units one at a time and gives a code point once it's whole,
joining a surrogate pair and refusing a lone half (a second half with no first, or a first half followed by anything but
a second), which it counts. `Printable` says what Minecraft gets as text: nothing below 0x20, and not 0x7F; Backspace,
Enter, Tab, Esc and Ctrl's letters reach Minecraft as keys.

**Files:**
- Create: `src/input/Text.h`, `src/input/Text.cpp`, `tests/TestText.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestText.cpp`**

```cpp
#include "Harness.h"

#include "input/Text.h"

using namespace cpmc::input;

TEST(TextTakesWholeCodePoints)
{
	Utf16Text text;
	CHECK_EQ(text.Take(u'a').value_or(0), uint32_t('a'));
	CHECK_EQ(text.Take(0x00E6).value_or(0), uint32_t(0xE6));  // æ, typed on the user's Danish layout
	CHECK_EQ(text.Take(0x20AC).value_or(0), uint32_t(0x20AC));  // €, with AltGr
	CHECK(!text.Take(0xD83D).has_value());                     // 🙂's first half waits for its second
	CHECK_EQ(text.Take(0xDE42).value_or(0), uint32_t(0x1F642));
	CHECK_EQ(text.Refused(), 0ull);
}

TEST(TextRefusesLoneSurrogates)
{
	Utf16Text text;
	CHECK(!text.Take(0xDE42).has_value());  // a second half with no first
	CHECK_EQ(text.Refused(), 1ull);
	CHECK(!text.Take(0xD83D).has_value());
	CHECK_EQ(text.Take(u'b').value_or(0), uint32_t('b'));  // a first half with no second: it's refused, and b goes on
	CHECK_EQ(text.Refused(), 2ull);
	CHECK(!text.Take(0xD83D).has_value());
	CHECK(!text.Take(0xD83D).has_value());  // a first half after a first half: the earlier is refused
	CHECK_EQ(text.Refused(), 3ull);
	CHECK_EQ(text.Take(0xDE00).value_or(0), uint32_t(0x1F600));
}

TEST(TextKnowsWhatMinecraftTypes)
{
	CHECK(Printable(' '));
	CHECK(Printable('a'));
	CHECK(Printable(0xE6));
	CHECK(Printable(0x1F642));
	CHECK(!Printable(0x08));  // Backspace, a key to Minecraft
	CHECK(!Printable(0x0D));  // Enter
	CHECK(!Printable(0x16));  // Ctrl+V
	CHECK(!Printable(0x1B));  // Esc
	CHECK(!Printable(0x1F));
	CHECK(!Printable(0x7F));  // Ctrl+Backspace
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestCursor.cpp
```
```cmake
	tests/TestCursor.cpp
	tests/TestText.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -o -E "error C[0-9]+: [^[]*" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'input/Text.h'`.

- [x] **Step 4: Write `src/input/Text.h`**

```cpp
#pragma once

#include <cstdint>
#include <optional>

// Typed text (spike 8's "Typed text"): WM_CHAR's UTF-16 code units made into code points, and which of those Minecraft
// gets as text. Pure, and not thread-safe: the router holds its lock around every call.
namespace cpmc::input
{
// Whether Minecraft gets aPoint as text: nothing below 0x20, and not 0x7F. Backspace, Enter, Tab, Esc and Ctrl's letters
// reach Minecraft as keys, as SDL gives them.
bool Printable(uint32_t aPoint);

class Utf16Text
{
public:
	// One code unit: the code point it completes, or nothing while a surrogate pair's first half waits for its second.
	// A lone half is refused: a second half with no first, or a first half followed by anything but a second.
	std::optional<uint32_t> Take(uint16_t aUnit);
	uint64_t                Refused() const { return refused_; }

private:
	std::optional<uint16_t> high_;  // a first half, waiting
	uint64_t                refused_ = 0;
};
}
```

- [x] **Step 5: Write `src/input/Text.cpp`**

```cpp
#include "input/Text.h"

namespace cpmc::input
{
bool Printable(uint32_t aPoint)
{
	return aPoint >= 0x20 && aPoint != 0x7F;
}

std::optional<uint32_t> Utf16Text::Take(uint16_t aUnit)
{
	const bool first = aUnit >= 0xD800 && aUnit <= 0xDBFF;
	const bool second = aUnit >= 0xDC00 && aUnit <= 0xDFFF;
	if (high_ && second)
	{
		const uint32_t point = 0x10000 + ((uint32_t(*high_) - 0xD800) << 10) + (aUnit - 0xDC00u);
		high_.reset();
		return point;
	}
	if (high_)
	{
		++refused_;  // a first half with no second
		high_.reset();
	}
	if (first)
	{
		high_ = aUnit;
		return std::nullopt;
	}
	if (second)
	{
		++refused_;
		return std::nullopt;
	}
	return aUnit;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/input/Cursor.cpp
```
```cmake
	src/input/Cursor.cpp
	src/input/Text.cpp
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `534 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-screens: typed text, WM_CHAR's code units made into code points, and what Minecraft types" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 8: The arrow, and its spot for the HUD pass

The spec's "Drawing it":
- **`CursorHeight`:** 18 back-buffer pixels at 1,080 lines, scaled with the height and rounded, at least 1: 24 on the
  user's 1,440.
- **`CursorAt`:** SkyCraft's arrow, judged at each pixel's centre as `SV_Position` is: inside where the slope 0.6 a
  pixel down leaves it, below its tip and above its height; its edges (left, slanted and bottom) 1.5 pixels wide at 18
  tall, black, and the rest white. `HudPS` does the same arithmetic in Task 11.
- **`HudCursor`:** the spot the router writes as the cursor moves and screens open and close, and the HUD pass reads at
  Present: x, y and whether it's shown, packed into one 64-bit atomic so they never tear.

**Files:**
- Modify: `src/render/Hud.h`, `src/render/Hud.cpp`, `tests/TestHud.cpp`

- [x] **Step 1: Edit `tests/TestHud.cpp` to add the failing tests**

```cpp
	CHECK_EQ(*slots.TakeNewest(), 2u);
}
```
```cpp
	CHECK_EQ(*slots.TakeNewest(), 2u);
}

TEST(HudCursorScalesWithTheScreen)
{
	CHECK_EQ(CursorHeight(1080), 18u);  // SkyCraft's
	CHECK_EQ(CursorHeight(1440), 24u);  // the user's
	CHECK_EQ(CursorHeight(2160), 36u);
	CHECK_EQ(CursorHeight(720), 12u);
	CHECK_EQ(CursorHeight(768), 13u);  // rounded
	CHECK_EQ(CursorHeight(0), 1u);
}

TEST(HudCursorIsSkyCraftsArrow)
{
	// 18 pixels tall, its tip at 100 200, each pixel judged at its centre, as HudPS sees SV_Position.
	CHECK(CursorAt(100, 200, 100, 200, 18) == CursorPart::None);  // the tip's own pixel: its centre is outside the slope
	CHECK(CursorAt(100, 205, 100, 200, 18) == CursorPart::Edge);  // the left edge
	CHECK(CursorAt(102, 210, 100, 200, 18) == CursorPart::Fill);
	CHECK(CursorAt(105, 210, 100, 200, 18) == CursorPart::Edge);  // the slanted edge
	CHECK(CursorAt(106, 210, 100, 200, 18) == CursorPart::None);  // past it
	CHECK(CursorAt(102, 217, 100, 200, 18) == CursorPart::Edge);  // the bottom edge
	CHECK(CursorAt(100, 218, 100, 200, 18) == CursorPart::None);  // below it
	CHECK(CursorAt(99, 210, 100, 200, 18) == CursorPart::None);   // left of the tip
	// Twice as tall, the edges twice as wide.
	CHECK(CursorAt(5, 20, 0, 0, 36) == CursorPart::Fill);
	CHECK(CursorAt(2, 20, 0, 0, 36) == CursorPart::Edge);
	CHECK(CursorAt(5, 20, 0, 0, 0) == CursorPart::None);  // no cursor
}

TEST(HudCursorKeepsItsSpotWhole)
{
	HudCursor cursor;
	CHECK(!cursor.Get().shown);
	cursor.Set({ 2559, 1439, true });
	HudCursor::Spot spot = cursor.Get();
	CHECK(spot.shown);
	CHECK_EQ(spot.x, 2559u);
	CHECK_EQ(spot.y, 1439u);
	cursor.Set({ 7679, 4319, false });
	spot = cursor.Get();
	CHECK(!spot.shown);
	CHECK_EQ(spot.x, 7679u);
	CHECK_EQ(spot.y, 4319u);
	cursor.Set({ 0x7FFFFFFF, 0x7FFFFFFF, false });  // the largest spot: its parts stay apart
	spot = cursor.Get();
	CHECK(!spot.shown);
	CHECK_EQ(spot.x, 0x7FFFFFFFu);
	CHECK_EQ(spot.y, 0x7FFFFFFFu);
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -o -E "error C[0-9]+: [^[]*" | sort -u | head -3
```
Expected: `error C2065: 'HudCursor': undeclared identifier`, with `CursorPart`'s parts undeclared too.

- [x] **Step 3: Edit `src/render/Hud.h` to declare them**

```cpp
#include <array>
```
```cpp
#include <array>
#include <atomic>
```
```cpp
// Which of the three upload buffers each image goes through. The main thread copies an image into a slot the GPU has
```
```cpp
// Spike 8: the cursor's arrow, SkyCraft's, which the HUD pass draws over the image while a Minecraft screen is open in
// build mode (its spec's "Drawing it"). Its height in back-buffer pixels: 18 at 1,080 lines, scaled with the back
// buffer's height and rounded, at least 1.
uint32_t CursorHeight(uint32_t aBackHeight);

enum class CursorPart
{
	None,
	Edge,  // black
	Fill,  // white
};

// The part of the arrow, its tip at (aTipX, aTipY) and aHeight pixels tall (0: no cursor), at back-buffer pixel (aX, aY),
// judged at the pixel's centre: inside where the slope 0.6 a pixel down leaves it, its edges 1.5 pixels wide at 18
// pixels tall and as much wider as it's taller. HudPS does the same arithmetic.
CursorPart CursorAt(uint32_t aX, uint32_t aY, uint32_t aTipX, uint32_t aTipY, uint32_t aHeight);

// Where the cursor is, for the HUD pass: the router writes it as it moves and as screens open and close, and the pass reads
// it at Present. Its spot is one atomic, so x and y never tear.
class HudCursor
{
public:
	struct Spot
	{
		uint32_t x = 0;  // back-buffer pixels, below 2^31 each
		uint32_t y = 0;
		bool     shown = false;
	};

	void Set(const Spot& aSpot);
	Spot Get() const;

private:
	std::atomic<uint64_t> packed_{ 0 };  // x in bits 0 to 30, y in 31 to 61, shown in 62
};

// Which of the three upload buffers each image goes through. The main thread copies an image into a slot the GPU has
```

- [x] **Step 4: Edit `src/render/Hud.cpp` to add them**

```cpp
#include <algorithm>
```
```cpp
#include <algorithm>
#include <cmath>
```
```cpp
	return (aBottomUp ? aHeight - 1 - y : y) * aWidth + x;
```
```cpp
	return (aBottomUp ? aHeight - 1 - y : y) * aWidth + x;
}

uint32_t CursorHeight(uint32_t aBackHeight)
{
	return std::max(1u, uint32_t(std::lround(18.0 * aBackHeight / 1080.0)));
}

CursorPart CursorAt(uint32_t aX, uint32_t aY, uint32_t aTipX, uint32_t aTipY, uint32_t aHeight)
{
	if (aHeight == 0)
	{
		return CursorPart::None;
	}
	const float height = float(aHeight);
	const float edge = 1.5f * height / 18.0f;
	const float x = float(aX) + 0.5f - float(aTipX);
	const float y = float(aY) + 0.5f - float(aTipY);
	if (x < 0.0f || y < 0.0f || y >= height || x > y * 0.6f)
	{
		return CursorPart::None;
	}
	return x < edge || x > y * 0.6f - edge || y > height - edge ? CursorPart::Edge : CursorPart::Fill;
}

void HudCursor::Set(const Spot& aSpot)
{
	packed_.store(uint64_t(aSpot.x & 0x7FFFFFFFu) | (uint64_t(aSpot.y & 0x7FFFFFFFu) << 31) | (aSpot.shown ? 1ull << 62 : 0ull));
}

HudCursor::Spot HudCursor::Get() const
{
	const uint64_t packed = packed_.load();
	return { uint32_t(packed & 0x7FFFFFFFu), uint32_t((packed >> 31) & 0x7FFFFFFFu), (packed >> 62) != 0 };
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `537 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-screens: the cursor's arrow, SkyCraft's, scaled with the screen, and its spot for the HUD pass" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 9: Build mode: Minecraft's screens' keys

The spec's "Where each input goes", in `BuildMode`, which stays pure:
- **Holders for every key,** by scan code and E0 prefix (512, after the five buttons'), so any key's repeats and
  release go where its press went. A key's new press is placed by `PlaceKey`, in this order:
  1. build mode off or paused: the game's;
  2. a Minecraft screen open (`Situation::screenOpen`, the link's flag): Minecraft's, the menu keys and O too;
  3. F3's press Minecraft's: Minecraft's, counted (`F3Taken`), so F3+F4, F3+N, F3+F6 and F3+Esc work;
  4. a menu key (Esc, I, M, J, P, K, H; no longer T): build mode ends, and the game opens its menu;
  5. O: `kInOpenMenu`, and its repeats and release go nowhere;
  6. 1 to 9, and the taken keys: W, A, S, D, Space, Left Shift, Left Ctrl, and now E, T, /, Q and F3, Minecraft's; C and
     Right Ctrl only hidden;
  7. anything else: the game's.
- **F8 and Shift+F6** stay build mode's before any of that. An F6 without Shift now goes on as any key, so it reaches
  Minecraft with a screen open or F3 held.
- **In a screen,** 1 to 9 and the wheel don't move the slot readout, a side button doesn't clear it, and a click makes
  no latency mark.

Two of Task 9's older checks change with it: E is no longer the game's (F stands in for it), and T is no longer a menu
key.

**Files:**
- Modify: `src/input/BuildMode.h`, `src/input/BuildMode.cpp`, `tests/TestBuildMode.cpp`

- [x] **Step 1: Edit `tests/TestBuildMode.cpp` to add the failing tests**

```cpp
	CHECK(Passes(mode.Key(Press(0x12), Ready())));        // E: the game's iconic weapon, still the game's
```
```cpp
	CHECK(Passes(mode.Key(Press(0x21), Ready())));        // F: the game's interact, still the game's
```
```cpp
	for (const int key : { 0x01, 0x17, 0x32, 0x24, 0x19, 0x25, 0x23, 0x14 })  // Esc I M J P K H T
```
```cpp
	for (const int key : { 0x01, 0x17, 0x32, 0x24, 0x19, 0x25, 0x23 })  // Esc I M J P K H: T is Minecraft's chat
```
```cpp
TEST(BuildModeTakesTheOverlaysOwnWord)
```
```cpp
TEST(BuildModeGivesMinecraftItsScreensKeys)
{
	// E, T, / and Q (spike 8's "No Minecraft screen open"), as set-1 scan codes, and the SDL scancodes Minecraft gets.
	const std::pair<uint16_t, uint16_t> keys[] = { { 0x12, 8 }, { 0x14, 23 }, { 0x35, 56 }, { 0x10, 20 } };
	BuildMode                           mode = On();
	for (const auto& [scanCode, sdl] : keys)
	{
		const Decision down = mode.Key(Press(scanCode), Ready());
		CHECK(down.hideKey && down.forwarded == 1 && Is(down.forward[0], proto::kInKey, sdl, 1));
		const Decision up = mode.Key(Release(scanCode), Ready());
		CHECK(up.hideKey && up.forwarded == 1 && Is(up.forward[0], proto::kInKey, sdl, 0));
	}
	CHECK(mode.On());  // T opens Minecraft's chat, and no longer leaves build mode
	CHECK_STR(mode.Last(), "Q up");
	CHECK(Passes(mode.Key(Press(0x35, true), Ready())));  // the keypad's /
	BuildMode off;
	for (const auto& [scanCode, sdl] : keys)
	{
		CHECK(Passes(off.Key(Press(scanCode), Ready())));
	}
}

TEST(BuildModeGivesMinecraftEveryKeyWhileF3IsHeld)
{
	BuildMode      mode = On();
	const Decision f3 = mode.Key(Press(0x3D), Ready());
	CHECK(f3.hideKey && Is(f3.forward[0], proto::kInKey, 60, 1));
	const Decision f4 = mode.Key(Press(0x3E), Ready());  // F3+F4: the game mode switcher
	CHECK(f4.hideKey && Is(f4.forward[0], proto::kInKey, 61, 1));
	CHECK(Is(mode.Key(Release(0x3E), Ready()).forward[0], proto::kInKey, 61, 0));
	CHECK(Is(mode.Key(Press(0x31), Ready()).forward[0], proto::kInKey, 17, 1));  // F3+N: spectator
	mode.Key(Release(0x31), Ready());
	CHECK(Is(mode.Key(Press(0x40), Ready()).forward[0], proto::kInKey, 63, 1));  // F3+F6: debug options
	mode.Key(Release(0x40), Ready());
	const Decision esc = mode.Key(Press(0x01), Ready());  // F3+Esc: Minecraft's, not a menu key
	CHECK(esc.hideKey && Is(esc.forward[0], proto::kInKey, 41, 1));
	CHECK(mode.On());
	mode.Key(Release(0x01), Ready());
	CHECK_EQ(mode.F3Taken(), 4ull);
	CHECK(Is(mode.Key(Release(0x3D), Ready()).forward[0], proto::kInKey, 60, 0));
	CHECK(Passes(mode.Key(Press(0x3E), Ready())));  // a plain F4 is the game's again
	mode.Key(Release(0x3E), Ready());
	// F8 stays build mode's, F3 held or not.
	mode.Key(Press(0x3D), Ready());
	CHECK(mode.Key(Press(kF8), Ready()).releaseAll);
	CHECK(!mode.On());
	const Decision f3Up = mode.Key(Release(0x3D), Ready());  // Minecraft let go of everything: F3's release goes nowhere
	CHECK(f3Up.hideKey && f3Up.forwarded == 0);
}

TEST(BuildModeOpensMinecraftsPauseMenuOnO)
{
	BuildMode      mode = On();
	const Decision o = mode.Key(Press(0x18), Ready());
	CHECK(o.hideKey && o.forwarded == 1 && Is(o.forward[0], proto::kInOpenMenu, 0, 0));
	CHECK_STR(mode.Last(), "O, Minecraft's pause menu");
	const Decision repeat = mode.Key(Press(0x18), Ready());  // one pause menu a press: its repeats and release go nowhere
	CHECK(repeat.hideKey && repeat.forwarded == 0);
	const Decision up = mode.Key(Release(0x18), Ready());
	CHECK(up.hideKey && up.forwarded == 0);
	BuildMode off;
	CHECK(Passes(off.Key(Press(0x18), Ready())));
}

TEST(BuildModeGivesMinecraftEveryInputWithAScreenOpen)
{
	BuildMode mode = On();
	mode.Key(Press(0x04), Ready());  // 3, before any screen
	mode.Key(Release(0x04), Ready());
	Situation screen = Ready();
	screen.screenOpen = true;
	// Every key, the game's menu keys too: typing, and Esc and E closing the screen.
	const std::pair<RawKey, uint16_t> keys[] = { { Press(0x21), 9 },  { Press(0x01), 41 },     { Press(0x17), 12 }, { Press(0x18), 18 },
		                                         { Press(0x2E), 6 },  { Press(0x1D, true), 228 }, { Press(0x40), 63 }, { Press(0x12), 8 } };
	for (const auto& [key, sdl] : keys)
	{
		const Decision down = mode.Key(key, screen);
		CHECK(down.hideKey && down.forwarded == 1 && Is(down.forward[0], proto::kInKey, sdl, 1));
		const Decision up = mode.Key({ key.scanCode, key.extended, true }, screen);
		CHECK(up.hideKey && Is(up.forward[0], proto::kInKey, sdl, 0));
	}
	CHECK(mode.On());
	// 1 to 9 swap items there, and the wheel scrolls: the slot stays as it was.
	CHECK(Is(mode.Key(Press(0x06), screen).forward[0], proto::kInKey, 34, 1));  // 5
	mode.Key(Release(0x06), screen);
	const Decision wheel = mode.Mouse({ RI_MOUSE_WHEEL, -120 }, screen);
	CHECK(wheel.hideButtons == RI_MOUSE_WHEEL && Is(wheel.forward[0], proto::kInScroll, 0, -120));
	mode.Mouse({ RI_MOUSE_BUTTON_4_DOWN, 0 }, screen);
	mode.Mouse({ RI_MOUSE_BUTTON_4_UP, 0 }, screen);
	CHECK_EQ(mode.Slot().value_or(0), 3);
	// A click there changes no block: no latency mark.
	const Decision click = mode.Mouse({ RI_MOUSE_LEFT_BUTTON_DOWN, 0 }, screen);
	CHECK(Is(click.forward[0], proto::kInMouseButton, 1, 1));
	CHECK(!click.press);
	mode.Mouse({ RI_MOUSE_LEFT_BUTTON_UP, 0 }, screen);
	// F8 stays build mode's.
	CHECK(mode.Key(Press(kF8), screen).releaseAll);
	CHECK(!mode.On());
}

TEST(BuildModeKeepsShiftF6WithAScreenOpen)
{
	BuildMode mode = On();
	Situation screen = Ready();
	screen.screenOpen = true;
	CHECK(mode.Key(Press(0x2A), screen).forwarded == 1);  // Shift: Minecraft's
	const Decision f6 = mode.Key(Press(0x40), screen);
	CHECK(f6.releaseAll && !f6.hideKey && f6.forwarded == 0);  // CET's overlay, which pauses build mode
	CHECK(mode.Paused());
	CHECK(Passes(mode.Key(Press(0x21), screen)));  // paused: every input is the overlay's and the game's
	CHECK(Passes(mode.Mouse({ RI_MOUSE_LEFT_BUTTON_DOWN, 0 }, screen)));
	BuildMode off;
	CHECK(Passes(off.Key(Press(0x21), screen)));  // a screen open with build mode off isn't build mode's
	CHECK(Passes(off.Key(Press(0x01), screen)));
}

TEST(BuildModeSendsAKeysReleaseWhereItsPressWentAcrossAScreen)
{
	BuildMode mode = On();
	Situation screen = Ready();
	screen.screenOpen = true;
	CHECK(Passes(mode.Key(Press(0x21), Ready())));  // F, the game's, held as a screen opens
	CHECK(Passes(mode.Key(Press(0x21), screen)));   // its repeat is still the game's
	CHECK(Passes(mode.Key(Release(0x21), screen)));
	CHECK(mode.Key(Press(0x22), screen).forwarded == 1);  // G, pressed in the screen, is let go of after it closes
	const Decision release = mode.Key(Release(0x22), Ready());
	CHECK(release.hideKey && Is(release.forward[0], proto::kInKey, 10, 0));
	CHECK(Passes(mode.Key(Press(0x22), Ready())));  // and pressed again, it's the game's
}

TEST(BuildModeTakesTheOverlaysOwnWord)
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -o -E "error C[0-9]+: [^[]*" | sort -u | head -3
```
Expected: `error C2039: 'screenOpen': is not a member of 'cpmc::input::Situation'`, and `F3Taken` not a member of
`BuildMode`.

- [x] **Step 3: Edit `src/input/BuildMode.h` to add the screen state and the holders**

```cpp
// every input is the overlay's and the game's, and build mode goes on when it closes. A press's repeats and its release
// go where the press went, so neither side is left holding an input that only the other saw let go. Pure, and not
// thread-safe: the router (input/Router.h) holds its lock around every call.
```
```cpp
// every input is the overlay's and the game's, and build mode goes on when it closes. Since spike 8 (its spec's "Where
// each input goes"), E, T, /, Q and F3 are Minecraft's too, every key while F3's press is Minecraft's, and O opens
// Minecraft's pause menu; with a Minecraft screen open, every input but F8 and Shift+F6 is Minecraft's. A press's repeats
// and its release go where the press went, for every key and button, so neither side is left holding an input that only
// the other saw let go. Pure, and not thread-safe: the router (input/Router.h) holds its lock around every call.
```
```cpp
	                      // Known even when the link finds no V, as in a car, where the camera isn't V's eye
```
```cpp
	                      // Known even when the link finds no V, as in a car, where the camera isn't V's eye
	bool screenOpen = false;  // Minecraft says a screen is open (spike 8): with build mode on and not paused, every input
	                          // is Minecraft's
```
```cpp
	uint64_t           Hidden() const { return hidden_; }        // inputs hidden from the game
```
```cpp
	uint64_t           Hidden() const { return hidden_; }        // inputs hidden from the game
	uint64_t           F3Taken() const { return f3Taken_; }      // keys' presses Minecraft got only because F3 was held
```
```cpp
	static constexpr size_t kInputs = 23;  // the five mouse buttons, the keys 1 to 9, then nine other keys it takes

	Holder Route(size_t aInput, bool aDown);
```
```cpp
	static constexpr size_t kInputs = 5 + 512;  // the five mouse buttons, then every key by its scan code, then with E0

	Holder Route(size_t aInput, bool aDown);
	// Where a key's new press goes. A menu key's ends build mode first, and O's sends kInOpenMenu.
	Holder PlaceKey(const RawKey& aKey, const Situation& aSituation, Decision& aDecision);
```
```cpp
	uint64_t                    hidden_ = 0;
```
```cpp
	uint64_t                    hidden_ = 0;
	uint64_t                    f3Taken_ = 0;
```

- [x] **Step 4: Write `src/input/BuildMode.cpp`**

The whole file: its key handling is rewritten around `PlaceKey`.

```cpp
#include "input/BuildMode.h"

#include "input/Scancodes.h"

#include <Windows.h>

#include <cmath>
#include <format>
#include <initializer_list>
#include <iterator>
#include <utility>

namespace cpmc::input
{
namespace
{
constexpr uint16_t kF3 = 0x3D;
constexpr uint16_t kF6 = 0x40;
constexpr uint16_t kF8 = 0x42;
constexpr uint16_t kO = 0x18;
constexpr uint16_t kLeftShift = 0x2A;
constexpr uint16_t kRightShift = 0x36;

// The keys that open the game's menus (Esc, I, M, J, P, K and H): its name, or null. T, the game's phone, is Minecraft's
// chat since spike 8.
const char* MenuKey(const RawKey& aKey)
{
	if (aKey.extended)
	{
		return nullptr;
	}
	switch (aKey.scanCode)
	{
	case 0x01: return "Esc";
	case 0x17: return "I";
	case 0x32: return "M";
	case 0x24: return "J";
	case 0x19: return "P";
	case 0x25: return "K";
	case 0x23: return "H";
	default: return nullptr;
	}
}

// The mouse buttons build mode takes, as Raw Input flags them and SDL numbers them. The side buttons are Minecraft's
// too: Cyberpunk binds neither, and the user's Minecraft picks hotbar slots with them.
struct Button
{
	uint16_t    down;
	uint16_t    up;
	uint16_t    sdl;
	const char* name;
};

constexpr Button kButtons[] = {
	{ RI_MOUSE_LEFT_BUTTON_DOWN, RI_MOUSE_LEFT_BUTTON_UP, 1, "the left button" },
	{ RI_MOUSE_RIGHT_BUTTON_DOWN, RI_MOUSE_RIGHT_BUTTON_UP, 3, "the right button" },
	{ RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP, 2, "the middle button" },
	{ RI_MOUSE_BUTTON_4_DOWN, RI_MOUSE_BUTTON_4_UP, 4, "mouse button 4" },
	{ RI_MOUSE_BUTTON_5_DOWN, RI_MOUSE_BUTTON_5_UP, 5, "mouse button 5" },
};
constexpr size_t kButtonCount = std::size(kButtons);  // their holders come first, then the keys'
constexpr size_t kSideButtons = 3;                    // the first side button's index

// The other keys build mode takes with no Minecraft screen open: Minecraft's movement keys (spike 5's "Keys") and the
// keys of its screens (spike 8), forwarded, and the game's crouch keys, only hidden, so V can't crouch under the puppet.
// C is Minecraft's toolbar-saving key, so it isn't forwarded either. The forwarded keys are physical keys: Minecraft
// applies its own bindings to them. The keys 1 to 9 are taken too.
struct TakenKey
{
	uint16_t    scanCode;  // set 1, without its E0 prefix
	bool        extended;
	bool        forward;  // to Minecraft, as well as hidden from the game
	const char* name;
};

constexpr TakenKey kTakenKeys[] = {
	{ 0x11, false, true, "W" },        { 0x1E, false, true, "A" },         { 0x1F, false, true, "S" },
	{ 0x20, false, true, "D" },        { 0x39, false, true, "Space" },     { 0x2A, false, true, "Left Shift" },
	{ 0x1D, false, true, "Left Ctrl" }, { 0x12, false, true, "E" },        { 0x14, false, true, "T" },
	{ 0x35, false, true, "/" },        { 0x10, false, true, "Q" },         { kF3, false, true, "F3" },
	{ 0x2E, false, false, "C" },       { 0x1D, true, false, "Right Ctrl" },
};

const TakenKey* Taken(const RawKey& aKey)
{
	for (const TakenKey& taken : kTakenKeys)
	{
		if (aKey.scanCode == taken.scanCode && aKey.extended == taken.extended)
		{
			return &taken;
		}
	}
	return nullptr;
}

// The keys 1 to 9: the digit, or 0.
int Digit(const RawKey& aKey)
{
	return !aKey.extended && aKey.scanCode >= 0x02 && aKey.scanCode <= 0x0A ? aKey.scanCode - 0x01 : 0;
}

std::string KeyName(const RawKey& aKey)
{
	if (const int digit = Digit(aKey))
	{
		return std::format("the key {}", digit);
	}
	if (const TakenKey* taken = Taken(aKey))
	{
		return taken->name;
	}
	if (const char* menu = MenuKey(aKey))
	{
		return menu;
	}
	if (!aKey.extended && aKey.scanCode == kO)
	{
		return "O";
	}
	return std::format("the key of scan code {:#04x}{}", aKey.scanCode, aKey.extended ? " E0" : "");
}

// A key's holder: after the buttons', by its scan code, and those with the E0 prefix 256 further on. None past 0xFF.
std::optional<size_t> KeyIndex(const RawKey& aKey)
{
	if (aKey.scanCode > 0xFF)
	{
		return std::nullopt;
	}
	return kButtonCount + (aKey.extended ? 256u : 0u) + aKey.scanCode;
}

constexpr size_t kF3Index = kButtonCount + kF3;

int Sign(double aValue)
{
	return aValue > 0.0 ? 1 : aValue < 0.0 ? -1 : 0;
}
}

Decision BuildMode::Key(const RawKey& aKey, const Situation& aSituation)
{
	Decision decision;
	if (!aKey.extended && (aKey.scanCode == kLeftShift || aKey.scanCode == kRightShift))
	{
		(aKey.scanCode == kLeftShift ? leftShift_ : rightShift_) = !aKey.up;
	}
	if (!aSituation.inFront)
	{
		return decision;  // not the user at the game: nothing goes to Minecraft
	}
	if (!aKey.extended && aKey.scanCode == kF8)
	{
		// F8 is build mode's alone, and its repeats do nothing.
		decision.hideKey = true;
		++hidden_;
		const bool press = !aKey.up && !f8Down_;
		f8Down_ = !aKey.up;
		if (!press)
		{
			return decision;
		}
		if (on_)
		{
			decision.releaseAll = TurnOff("F8 turned it off");
		}
		else if (!aSituation.linked)
		{
			decision.requestLink = true;
		}
		else
		{
			TurnOn(aSituation);
		}
		return decision;
	}
	if (!aKey.extended && aKey.scanCode == kF6)
	{
		// Shift+F6 opens and closes CET's overlay, which pauses build mode while it's open, so its clicks don't reach
		// Minecraft. The overlay sees the key, and a repeat changes nothing. An F6 alone goes on as any other key: with a
		// Minecraft screen open, or F3 held, it's Minecraft's.
		const bool press = !aKey.up && !f6Down_;
		f6Down_ = !aKey.up;
		if (leftShift_ || rightShift_)
		{
			if (press)
			{
				decision.releaseAll = SetOverlay(!overlay_);
			}
			return decision;
		}
	}
	const std::optional<size_t> index = KeyIndex(aKey);
	if (!index)
	{
		return decision;
	}
	Holder& holder = held_[*index];
	if (!aKey.up && holder == Holder::None)
	{
		holder = PlaceKey(aKey, aSituation, decision);
	}
	const Holder to = holder;  // a repeat or a release goes where its press went
	if (aKey.up)
	{
		holder = Holder::None;
	}
	if (to == Holder::Minecraft || to == Holder::Neither)
	{
		decision.hideKey = true;
		++hidden_;
	}
	if (to == Holder::Minecraft)
	{
		Forward(decision, { proto::kInKey, SdlScancode(aKey.scanCode, aKey.extended), aKey.up ? 0 : 1, 0, 0 },
		        std::format("{} {}", KeyName(aKey), aKey.up ? "up" : "down"));
		const int digit = Digit(aKey);
		if (digit != 0 && !aKey.up && !aSituation.screenOpen)
		{
			slot_ = digit;  // in a Minecraft screen, 1 to 9 swap items instead
		}
	}
	return decision;
}

BuildMode::Holder BuildMode::PlaceKey(const RawKey& aKey, const Situation& aSituation, Decision& aDecision)
{
	if (!on_ || overlay_)
	{
		return Holder::Game;
	}
	if (aSituation.screenOpen)
	{
		return Holder::Minecraft;  // a Minecraft screen is open: every key is Minecraft's, typing and Esc included
	}
	if ((aKey.extended || aKey.scanCode != kF3) && held_[kF3Index] == Holder::Minecraft)
	{
		++f3Taken_;
		return Holder::Minecraft;  // F3's combinations: F3+F4, F3+N, F3+F6 and the rest
	}
	if (const char* menu = MenuKey(aKey))
	{
		aDecision.releaseAll = TurnOff(std::format("{} left it", menu));
		return Holder::Game;  // and the game opens its menu
	}
	if (!aKey.extended && aKey.scanCode == kO)
	{
		Forward(aDecision, { proto::kInOpenMenu, 0, 0, 0, 0 }, "O, Minecraft's pause menu");
		return Holder::Neither;  // one pause menu a press: its repeats and release go nowhere
	}
	if (Digit(aKey) != 0)
	{
		return Holder::Minecraft;
	}
	if (const TakenKey* taken = Taken(aKey))
	{
		return taken->forward ? Holder::Minecraft : Holder::Neither;
	}
	return Holder::Game;
}

Decision BuildMode::Mouse(const RawMouse& aMouse, const Situation& aSituation)
{
	Decision decision;
	if (!aSituation.inFront)
	{
		return decision;
	}
	const bool screen = on_ && !overlay_ && aSituation.screenOpen;
	for (size_t i = 0; i < kButtonCount; ++i)
	{
		for (const bool down : { true, false })
		{
			const uint16_t flag = down ? kButtons[i].down : kButtons[i].up;
			if (!(aMouse.buttons & flag))
			{
				continue;
			}
			const Holder to = Route(i, down);
			if (to == Holder::Minecraft || to == Holder::Neither)
			{
				decision.hideButtons |= flag;
				++hidden_;
			}
			if (to == Holder::Minecraft)
			{
				Forward(decision, { proto::kInMouseButton, kButtons[i].sdl, down ? 1 : 0, 0, 0 },
				        std::format("{} {}", kButtons[i].name, down ? "down" : "up"));
				if (screen)
				{
					// In a Minecraft screen a click changes no block, and a side button picks no slot.
				}
				else if (i < kSideButtons)
				{
					decision.press = decision.press || down;  // a side button changes no block: no latency to time
				}
				else if (down)
				{
					slot_.reset();  // which slot it picks, if any, is Minecraft's own binding
				}
			}
		}
	}
	if ((aMouse.buttons & RI_MOUSE_WHEEL) && on_ && !overlay_)
	{
		decision.hideButtons |= RI_MOUSE_WHEEL;
		++hidden_;
		Forward(decision, { proto::kInScroll, 0, aMouse.wheel, 0, 0 }, std::format("the wheel, {}", aMouse.wheel));
		if (!screen)
		{
			Scroll(aMouse.wheel);  // in a Minecraft screen, the wheel scrolls it instead
		}
	}
	return decision;
}

bool BuildMode::TurnOn(const Situation& aSituation)
{
	if (on_)
	{
		return true;
	}
	if (!aSituation.minecraftInWorld)
	{
		why_ = aSituation.minecraft ? "Minecraft isn't in its world" : "Minecraft isn't there";
	}
	else if (!aSituation.linked)
	{
		why_ = "it isn't linked";
	}
	else if (!aSituation.blocked.empty())
	{
		why_ = aSituation.blocked;  // first: in a car the camera isn't V's eye, so the link finds no V
	}
	else if (!aSituation.inGame)
	{
		why_ = "V isn't found";
	}
	else
	{
		on_ = true;
		why_.clear();
	}
	return on_;
}

bool BuildMode::TurnOff(std::string aWhy)
{
	if (!on_)
	{
		return false;
	}
	on_ = false;
	why_ = std::move(aWhy);
	LetGo();
	return true;
}

bool BuildMode::LostFront()
{
	held_.fill(Holder::None);
	f8Down_ = false;
	f6Down_ = false;
	leftShift_ = false;
	rightShift_ = false;
	return on_;
}

bool BuildMode::SetOverlay(bool aOpen)
{
	const bool pauses = aOpen && !overlay_ && on_;
	overlay_ = aOpen;
	if (pauses)
	{
		LetGo();
	}
	return pauses;
}

void BuildMode::LetGo()
{
	for (Holder& holder : held_)
	{
		if (holder == Holder::Minecraft)
		{
			holder = Holder::Neither;  // Minecraft lets go of it now: its repeats and release go nowhere
		}
	}
}

BuildMode::Holder BuildMode::Route(size_t aInput, bool aDown)
{
	Holder& holder = held_[aInput];
	if (aDown && holder == Holder::None)
	{
		holder = on_ && !overlay_ ? Holder::Minecraft : Holder::Game;  // a press goes where build mode says
	}
	const Holder to = holder;  // a repeat or a release goes where its press went
	if (!aDown)
	{
		holder = Holder::None;
	}
	return to;  // None: a release whose press was never seen, which the game ignores unless it holds the input
}

void BuildMode::Forward(Decision& aDecision, const proto::InputEvent& aEvent, std::string aWhat)
{
	if (aDecision.forwarded < aDecision.forward.size())
	{
		aDecision.forward[aDecision.forwarded++] = aEvent;
		++forwarded_;
		last_ = std::move(aWhat);
	}
}

void BuildMode::Scroll(int16_t aWheel)
{
	// As Minecraft's ScrollWheelHandler counts it: turning the other way starts over, and each event moves the slot one
	// step at most, by its whole notches, keeping the fraction for the next. Up is the slot to the left.
	const double notches = aWheel / 120.0;
	if (scroll_ != 0.0 && Sign(notches) != Sign(scroll_))
	{
		scroll_ = 0.0;
	}
	scroll_ += notches;
	const double whole = std::trunc(scroll_);
	scroll_ -= whole;
	if (whole != 0.0 && slot_)
	{
		slot_ = (*slot_ - 1 - Sign(whole) + 9) % 9 + 1;
	}
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
Expected: no error or warning lines, then `543 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-screens: build mode gives Minecraft its screens' keys, every key while F3 is held and with a screen open, and O its pause menu" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 10: The router: the cursor, typed text, and the Esc as build mode ends

The spec's "The cursor", "Typed text" and "Edge cases", in `Router`:
- **The screen state,** `ScreenOn`: build mode on, not paused, and the link's screen flag; kept in an atomic, so mouse
  movement alone still skips the lock while it's off. Each change is logged, and published to the HUD's cursor.
- **Mouse movement,** while it's on and the game in front: a relative event's counts move the cursor at Windows' speed
  (`factor_`), a changed image pixel goes as `kInCursor`, and the event's movement is zeroed, so V doesn't turn. An
  absolute event (a remote desktop's) moves nothing, and is left as it is. Buttons and the wheel go through build mode
  as before.
- **`Char`:** one WM_CHAR code unit; a whole, printable code point goes as `kInText` while the state is on.
- **`Update`:** the screen flag into `Situation`; Windows' speed read again as it goes from clear to set; the cursor
  told (centring); and `closeable_`: linked, Minecraft there, in its world, its player alive and a screen open.
- **The Esc:** build mode ending, by F8 (`Apply`) or any other way (`Off`), sends Esc's press and release after
  Minecraft lets go of everything, if `closeable_`.
- **`TakeMouseX`** gives 0 while the state is on, as while paused.
- **`SetHudArea`, `SetHudCursor`,** and the report's new fields. `ReadPointerSpeed` reads Windows' (`SPI_GETMOUSESPEED`,
  and `SPI_GETMOUSE`'s third value for "Enhance pointer precision"); the tests give their own.

**Files:**
- Modify: `src/input/Router.h`, `src/input/Router.cpp`, `tests/TestRouter.cpp`

- [x] **Step 1: Edit `tests/TestRouter.cpp` to add the failing tests**

The test router gets the user's speed, 8, a HUD cursor, and the user's HUD image, 1280 by 720 at twice its size.

```cpp
bool g_inFront = true;
```
```cpp
bool g_inFront = true;
int  g_speed = 8;  // the user's
```
```cpp
	return g_inFront;
```
```cpp
	return g_inFront;
}

PointerSpeed FakeSpeed()
{
	return { g_speed, false };
```
```cpp
// A router on a link, in build mode.
struct Building
{
	Building() : router(log, &FakeFront)
	{
		g_inFront = true;
```
```cpp
// What the link says with a Minecraft screen open.
link::Link::Status WithAScreen()
{
	link::Link::Status state = Linked();
	state.screenOpen = true;
	return state;
}

// A router on a link, in build mode, with the user's 1280 by 720 HUD image drawn twice its size.
struct Building
{
	Building() : router(log, &FakeFront, &FakeSpeed)
	{
		g_inFront = true;
		g_speed = 8;
		router.SetHudCursor(&hud);
		router.SetHudArea(1280, 720, 2);
```
```cpp
	LogQueue log;
	Mapping  mapping;
	Router   router;
```
```cpp
	LogQueue  log;
	Mapping   mapping;
	HudCursor hud;
	Router    router;
```
```cpp
	CHECK(!building.router.TurnOn());
}
```
```cpp
	CHECK(!building.router.TurnOn());
}

TEST(RouterMovesMinecraftsCursorWhileAScreenIsOpen)
{
	Building building;
	building.router.Update(true, WithAScreen());
	CHECK(Logged(building.log, "a Minecraft screen is build mode's: the mouse moves Minecraft's cursor, and every key is Minecraft's"));
	CHECK(building.hud.Get().shown);
	CHECK(building.hud.Get().x == 1280 && building.hud.Get().y == 720);  // the centre, as Minecraft puts its own position
	auto move = Mouse(0, 0, 10, -4);  // at speed 8, 7.5 and -3 back-buffer pixels: image pixel 643 358
	Inspect(building.router, move);
	CHECK_EQ(MouseOf(move).lLastX, LONG(0));  // V doesn't turn
	CHECK_EQ(MouseOf(move).lLastY, LONG(0));
	auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].type == proto::kInCursor && inputs[0].a == 643 && inputs[0].b == 358);
	CHECK(building.hud.Get().x == 1287 && building.hud.Get().y == 717);
	auto click = Mouse(RI_MOUSE_LEFT_BUTTON_DOWN, 0, 2, 0);  // a click and a move in one event: both Minecraft's
	Inspect(building.router, click);
	CHECK_EQ(MouseOf(click).usButtonFlags, USHORT(0));
	CHECK_EQ(MouseOf(click).lLastX, LONG(0));
	inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 2 && inputs[0].type == proto::kInCursor && inputs[0].a == 644 && inputs[1].type == proto::kInMouseButton);
	CHECK(!building.router.TakePress());               // a click in a screen is no latency mark
	CHECK_EQ(building.router.TakeMouseX(), 0ll);        // and the moves turn nothing
	building.router.Update(true, Linked());  // the screen closed
	CHECK(Logged(building.log, "no Minecraft screen is build mode's: the mouse turns V again"));
	CHECK(!building.hud.Get().shown);
	auto turn = Mouse(0, 0, 10, -4);
	Inspect(building.router, turn);
	CHECK_EQ(MouseOf(turn).lLastX, LONG(10));
	CHECK(building.mapping.Inputs().empty());
	CHECK_EQ(building.router.TakeMouseX(), 10ll);
}

TEST(RouterSendsTheCursorOnlyAsItsPixelChanges)
{
	Building building;
	building.router.Update(true, WithAScreen());
	building.mapping.Inputs();
	for (int i = 0; i < 2; ++i)
	{
		auto small = Mouse(0, 0, 1, 0);  // 0.75 back-buffer pixels each: still image pixel 640
		Inspect(building.router, small);
	}
	CHECK(building.mapping.Inputs().empty());
	auto third = Mouse(0, 0, 1, 0);  // 2.25 in all: pixel 641
	Inspect(building.router, third);
	const auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].a == 641 && inputs[0].b == 360);
	CHECK_EQ(building.router.Report().cursorEvents, 1ull);
	auto     absolute = Mouse(0, 0, 30000, 30000);  // a remote desktop's position, not a movement: it moves nothing
	RAWMOUSE mouse = MouseOf(absolute);
	mouse.usFlags = MOUSE_MOVE_ABSOLUTE;
	std::memcpy(absolute.data() + sizeof(RAWINPUTHEADER), &mouse, sizeof mouse);
	Inspect(building.router, absolute);
	CHECK_EQ(MouseOf(absolute).lLastX, LONG(30000));
	CHECK(building.mapping.Inputs().empty());
}

TEST(RouterTypesTextIntoAScreen)
{
	Building building;
	building.router.Char(u'a');  // no screen: nothing to type into
	CHECK(building.mapping.Inputs().empty());
	building.router.Update(true, WithAScreen());
	building.mapping.Inputs();
	building.router.Char(0x00E6);  // æ
	building.router.Char(0x000D);  // Enter's character: Enter reaches Minecraft as a key
	building.router.Char(0xD83D);
	building.router.Char(0xDE42);  // 🙂
	building.router.Char(0xDE42);  // a lone second half
	const auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 2 && inputs[0].type == proto::kInText && inputs[0].a == 0xE6 && inputs[1].a == 0x1F642);
	const InputReport report = building.router.Report();
	CHECK_EQ(report.charsTyped, 2ull);
	CHECK_EQ(report.charsRefused, 1ull);
	building.router.SetOverlay(true);  // paused: what's typed is CET's overlay's
	building.mapping.Inputs();
	building.router.Char(u'b');
	CHECK(building.mapping.Inputs().empty());
}

TEST(RouterClosesMinecraftsScreenAsBuildModeEnds)
{
	{
		Building building;
		building.router.Update(true, WithAScreen());
		building.mapping.Inputs();
		auto f8 = Key(0x42, VK_F8);
		Inspect(building.router, f8);
		const auto inputs = building.mapping.Inputs();
		CHECK(inputs.size() == 3 && inputs[0].type == proto::kInReleaseAll);  // Minecraft lets go of everything, then Esc
		CHECK(inputs.size() == 3 && inputs[1].type == proto::kInKey && inputs[1].code == 41 && inputs[1].a == 1);
		CHECK(inputs.size() == 3 && inputs[2].type == proto::kInKey && inputs[2].code == 41 && inputs[2].a == 0);
		CHECK(Logged(building.log, "build mode ended with a Minecraft screen open: Esc closes it"));
	}
	{
		Building building;
		building.router.Update(true, WithAScreen(), "V is in a workspot");
		const auto inputs = building.mapping.Inputs();
		CHECK(inputs.size() == 3 && inputs[1].code == 41);
	}
	for (const bool dead : { false, true })
	{
		// Minecraft leaving its world, or its player dying and respawning, changes its screens itself: an Esc could find
		// none, and open the pause menu.
		Building           building;
		link::Link::Status state = WithAScreen();
		if (dead)
		{
			state.minecraftFlags = proto::kMcInWorld | proto::kMcDead;
		}
		else
		{
			state.inWorld = false;
		}
		building.router.Update(true, state);
		CHECK(!building.router.On());
		const auto inputs = building.mapping.Inputs();
		CHECK(inputs.size() == 1 && inputs[0].type == proto::kInReleaseAll);
	}
	Building building;
	building.router.Update(false, WithAScreen());  // unlinked
	CHECK(!building.router.On());
	CHECK_EQ(building.mapping.Inputs().size(), size_t(1));
}

TEST(RouterLeavesTheMouseToTheGameWhileAScreenIsntBuildModes)
{
	Building building;
	building.router.SetOverlay(true);  // paused behind CET's overlay
	building.router.Update(true, WithAScreen());
	building.mapping.Inputs();
	auto move = Mouse(0, 0, 10, 0);
	Inspect(building.router, move);
	CHECK_EQ(MouseOf(move).lLastX, LONG(10));
	CHECK(building.mapping.Inputs().empty());
	CHECK(!building.hud.Get().shown);
	building.router.SetOverlay(false);  // the overlay closed: the screen is build mode's again
	CHECK(building.hud.Get().shown);
	building.router.TurnOff("a build off request");
	CHECK(!building.hud.Get().shown);
	auto off = Mouse(0, 0, 10, 0);
	Inspect(building.router, off);
	CHECK_EQ(MouseOf(off).lLastX, LONG(10));
}

TEST(RouterRereadsWindowsPointerSpeedAsAScreenOpens)
{
	Building building;
	CHECK(Logged(building.log, "Windows' pointer speed is 8 of 20: Minecraft's cursor moves 0.750 back-buffer pixels a count"));
	g_speed = 12;  // changed in Windows while the game runs
	building.router.Update(true, Linked());
	CHECK_EQ(building.router.Report().speed.speed, 8);  // read again only as a screen opens
	building.router.Update(true, WithAScreen());
	CHECK_EQ(building.router.Report().speed.speed, 12);
	CHECK(Logged(building.log, "Windows' pointer speed is 12 of 20: Minecraft's cursor moves 1.500 back-buffer pixels a count"));
	building.mapping.Inputs();
	auto move = Mouse(0, 0, 2, 0);  // 3 back-buffer pixels: from 1280 to 1283, image pixel 641
	Inspect(building.router, move);
	const auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].a == 641);
	g_speed = 8;
}

TEST(RouterReportsTheScreen)
{
	Building building;
	auto     f3 = Key(0x3D, VK_F3);
	auto     f4 = Key(0x3E, VK_F4);
	Inspect(building.router, f3);
	Inspect(building.router, f4);  // F3+F4, before the switcher's screen is known
	building.router.Update(true, WithAScreen());
	auto move = Mouse(0, 0, 4, 0);  // 3 back-buffer pixels: image pixel 641
	Inspect(building.router, move);
	building.router.Char(u'x');
	const InputReport report = building.router.Report();
	CHECK(report.screenOpen && report.screen);
	CHECK(report.cursor.x == 641 && report.cursor.y == 360);
	CHECK(report.imageWidth == 1280 && report.imageHeight == 720);
	CHECK_EQ(report.speed.speed, 8);
	CHECK(!report.speed.enhanced);
	CHECK_EQ(report.cursorEvents, 1ull);
	CHECK_EQ(report.f3Taken, 1ull);
	CHECK_EQ(report.charsTyped, 1ull);
	building.router.SetOverlay(true);
	const InputReport paused = building.router.Report();
	CHECK(paused.screenOpen && !paused.screen);
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -o -E "error C[0-9]+: [^[]*" | sort -u | head -3
```
Expected: `error C2039: 'Char': is not a member of 'cpmc::input::Router'`, with `SetHudArea` and `SetHudCursor` too.

- [x] **Step 3: Write `src/input/Router.h`**

```cpp
#pragma once

#include "LogQueue.h"
#include "input/BuildMode.h"
#include "input/Cursor.h"
#include "input/Text.h"
#include "link/Link.h"
#include "render/Hud.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

// Where the game's keyboard and mouse events go (the spec's "Input"), between four threads: the game's input thread,
// through the GetRawInputData patch (input/RawInputHook.h) and, for typed text, the PeekMessageW patch
// (input/MessageHook.h); the focus watch's; and the main thread, which keeps the link and what it says about Minecraft
// and V current, and turns build mode on and off. One lock guards build mode and the link, so once the main thread takes
// the link away, nothing pushes into the mapping. Since spike 8, while a Minecraft screen is build mode's (on, not
// paused, and Minecraft saying a screen is open), the mouse's movement moves Minecraft's cursor instead of turning V,
// typed characters go to Minecraft, and build mode ending closes the screen with Esc.
namespace cpmc::input
{
// Whether the foreground window belongs to this process.
bool GameInFront();
// Windows' pointer speed, and whether "Enhance pointer precision" is on, as SystemParametersInfo says.
PointerSpeed ReadPointerSpeed();

// Build mode and the hook, for the status.
struct InputReport
{
	bool               on = false;
	std::string        why;            // why it's off
	std::optional<int> slot;           // Minecraft's hotbar slot, 1 to 9, once a number key has set it
	uint64_t           read = 0;       // keyboard and mouse events read
	uint64_t           forwarded = 0;  // events for Minecraft's input ring
	uint64_t           hidden = 0;     // inputs hidden from the game
	std::string        last;           // the last input forwarded
	bool               paused = false;  // on, behind CET's overlay
	// Spike 8
	bool         screenOpen = false;  // Minecraft says a screen is open
	bool         screen = false;      // and it's build mode's: the mouse moves Minecraft's cursor, and every key is Minecraft's
	Pixel        cursor;              // in the HUD image's pixels
	uint32_t     imageWidth = 0;      // the HUD image the cursor stays in; 0 before one has come
	uint32_t     imageHeight = 0;
	PointerSpeed speed;               // Windows', as last read
	uint64_t     cursorEvents = 0;    // kInCursor sent
	uint64_t     f3Taken = 0;         // keys Minecraft got because F3 was held
	uint64_t     charsTyped = 0;      // kInText sent
	uint64_t     charsRefused = 0;    // lone surrogates
};

class Router
{
public:
	using FrontCheck = bool (*)();
	using SpeedCheck = PointerSpeed (*)();
	explicit Router(LogQueue& aLog, FrontCheck aInFront = &GameInFront, SpeedCheck aSpeed = &ReadPointerSpeed);

	// From the patch, once GetRawInputData has copied an event into aData, aBytes of it. A keyboard or mouse event that
	// build mode takes is hidden from the game in place, and forwarded. Anything else, or too short, is left alone.
	void Inspect(void* aData, uint32_t aBytes);
	// From the PeekMessageW patch: one of WM_CHAR's UTF-16 code units. While a Minecraft screen is build mode's, each whole,
	// printable code point goes to Minecraft as kInText.
	void Char(uint16_t aUnit);

	// From the main thread. The link: once the mapping opens, and null again at unload.
	void SetLink(link::Link* aLink);
	// Once a frame, after the link's update: whether the user linked, what the link says, and why V can't be placed, if
	// it can't (spike 5), even when the link finds no V. Build mode ends if Minecraft or V is gone, or the link, or V
	// can't be placed, or Minecraft's player dies; why V can't be placed is said before "V isn't found". A Minecraft
	// screen opening re-reads Windows' pointer speed, and puts the cursor at the image's centre.
	void Update(bool aLinked, const link::Link::Status& aState, const std::string& aVBlocked = {});
	// Once a frame, before Update: the HUD image's size and the back-buffer pixels each of its texels covers, which the
	// cursor stays in.
	void SetHudArea(uint32_t aImageWidth, uint32_t aImageHeight, uint32_t aScale);
	// Where the HUD pass finds the cursor, which the router keeps current; null for none.
	void SetHudCursor(HudCursor* aCursor);
	// The mouse's movement along x since the last call, in raw counts: for the turn a teleport adds back (spike 5). None
	// while build mode is paused, when the mouse is CET's overlay's, or while a Minecraft screen is build mode's.
	int64_t TakeMouseX();
	// Build mode on: nothing, or why it stays off.
	std::optional<std::string> TurnOn();
	// Build mode off, saying why: true if it was on.
	bool                  TurnOff(std::string aWhy);
	bool                  On() const;
	bool                  TakeLinkRequest();  // F8 asked for the link, since the last call
	std::optional<double> TakePress();        // when a mouse button's press last went to Minecraft (SteadySeconds), once
	InputReport           Report() const;

	// From the focus watch: the game has just stopped being in front.
	void FrontLost();
	// From the main thread: CET's overlay opened or closed, as the overlay itself says (spike 7's `overlay` request).
	void SetOverlay(bool aOpen);

private:
	// With the lock held.
	void Apply(const Decision& aDecision, bool aWasOn, bool aWasPaused, const std::string& aWhyBefore);
	void LogPause(bool aWasPaused);  // build mode paused, or going on again, since aWasPaused
	bool Off(std::string aWhy);
	void Push(const proto::InputEvent& aEvent);
	bool ScreenOn() const;  // a Minecraft screen is build mode's
	void Refresh();         // the screen state, and the HUD's cursor, after anything that may change them
	void Publish();         // the HUD's cursor
	void CloseScreen();     // build mode ended: Esc closes Minecraft's screen, if it's there to close
	void ReadSpeed();

	LogQueue&             log_;
	FrontCheck            inFront_;
	SpeedCheck            speedCheck_;
	mutable std::mutex    mutex_;
	BuildMode             mode_;
	link::Link*           link_ = nullptr;
	Situation             situation_;  // the main thread's part: the link, Minecraft and V
	bool                  linkRequested_ = false;
	std::optional<double> press_;
	std::atomic<uint64_t> read_{ 0 };
	std::atomic<int64_t>  mouseX_{ 0 };
	// Spike 8
	std::atomic<bool>           screen_{ false };  // ScreenOn, for the movement's way in without the lock
	bool                        closeable_ = false;  // linked, Minecraft in its world, its player alive, a screen open
	Cursor                      cursor_;
	Utf16Text                   text_;
	HudCursor*                  hudCursor_ = nullptr;
	PointerSpeed                speed_;
	std::optional<PointerSpeed> speedLogged_;
	double                      factor_ = 1.0;
	uint64_t                    cursorEvents_ = 0;
	uint64_t                    charsTyped_ = 0;
};
}
```

- [x] **Step 4: Write `src/input/Router.cpp`**

```cpp
#include "input/Router.h"

#include "Measurements.h"

#include <Windows.h>

#include <cstring>
#include <format>
#include <utility>

namespace cpmc::input
{
namespace
{
// The mouse flags build mode may take: the five buttons' presses and releases, and the wheel. An event without them is
// movement, or the horizontal wheel, and stays the game's without taking the lock, unless a Minecraft screen is build
// mode's.
constexpr USHORT kTaken = RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_LEFT_BUTTON_UP | RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_UP |
                          RI_MOUSE_MIDDLE_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_UP | RI_MOUSE_BUTTON_4_DOWN | RI_MOUSE_BUTTON_4_UP |
                          RI_MOUSE_BUTTON_5_DOWN | RI_MOUSE_BUTTON_5_UP | RI_MOUSE_WHEEL;

constexpr uint16_t kSdlEscape = 41;
}

bool GameInFront()
{
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	return process == GetCurrentProcessId();
}

PointerSpeed ReadPointerSpeed()
{
	PointerSpeed speed;
	int          value = 0;
	if (SystemParametersInfoW(SPI_GETMOUSESPEED, 0, &value, 0))
	{
		speed.speed = value;
	}
	int mouse[3]{};  // two thresholds, and the acceleration: "Enhance pointer precision"
	if (SystemParametersInfoW(SPI_GETMOUSE, 0, mouse, 0))
	{
		speed.enhanced = mouse[2] != 0;
	}
	return speed;
}

Router::Router(LogQueue& aLog, FrontCheck aInFront, SpeedCheck aSpeed) : log_(aLog), inFront_(aInFront), speedCheck_(aSpeed)
{
	ReadSpeed();
}

void Router::Inspect(void* aData, uint32_t aBytes)
{
	if (!aData || aBytes < sizeof(RAWINPUTHEADER))
	{
		return;
	}
	RAWINPUTHEADER header;
	std::memcpy(&header, aData, sizeof header);
	uint8_t* const body = static_cast<uint8_t*>(aData) + sizeof header;
	if (header.dwType == RIM_TYPEMOUSE && aBytes >= sizeof header + sizeof(RAWMOUSE))
	{
		++read_;
		RAWMOUSE mouse;
		std::memcpy(&mouse, body, sizeof mouse);
		const bool relative = !(mouse.usFlags & MOUSE_MOVE_ABSOLUTE);
		if (relative)
		{
			mouseX_ += mouse.lLastX;
		}
		if ((mouse.usButtonFlags & kTaken) == 0 && !screen_.load())
		{
			return;
		}
		const bool      inFront = inFront_();
		std::lock_guard lock(mutex_);
		Situation       situation = situation_;
		situation.inFront = inFront;
		const bool        wasOn = mode_.On();
		const bool        wasPaused = mode_.Paused();
		const std::string whyBefore = mode_.Why();
		bool              changed = false;
		if (ScreenOn() && inFront && relative)
		{
			// A Minecraft screen is build mode's: the movement moves Minecraft's cursor, and turns nothing in the game.
			if (mouse.lLastX != 0 || mouse.lLastY != 0)
			{
				if (const std::optional<Pixel> pixel = cursor_.Move(mouse.lLastX, mouse.lLastY, factor_))
				{
					Push({ proto::kInCursor, 0, int32_t(pixel->x), int32_t(pixel->y), 0 });
					++cursorEvents_;
				}
				Publish();
			}
			mouse.lLastX = 0;
			mouse.lLastY = 0;
			changed = true;
		}
		const Decision decision = mode_.Mouse({ mouse.usButtonFlags, int16_t(mouse.usButtonData) }, situation);
		if (decision.hideButtons != 0)
		{
			// The game reads buttons and the wheel only from these flags.
			mouse.usButtonFlags = USHORT(mouse.usButtonFlags & ~decision.hideButtons);
			changed = true;
		}
		if (changed)
		{
			std::memcpy(body, &mouse, sizeof mouse);
		}
		Apply(decision, wasOn, wasPaused, whyBefore);
	}
	else if (header.dwType == RIM_TYPEKEYBOARD && aBytes >= sizeof header + sizeof(RAWKEYBOARD))
	{
		++read_;
		RAWKEYBOARD key;
		std::memcpy(&key, body, sizeof key);
		const bool      inFront = inFront_();
		std::lock_guard lock(mutex_);
		Situation       situation = situation_;
		situation.inFront = inFront;
		const bool        wasOn = mode_.On();
		const bool        wasPaused = mode_.Paused();
		const std::string whyBefore = mode_.Why();
		const Decision    decision = mode_.Key({ key.MakeCode, (key.Flags & RI_KEY_E0) != 0, (key.Flags & RI_KEY_BREAK) != 0 }, situation);
		if (decision.hideKey)
		{
			// A no-op: the game skips a key whose virtual key is 0xFF.
			key.MakeCode = 0;
			key.VKey = 0xFF;
			std::memcpy(body, &key, sizeof key);
		}
		Apply(decision, wasOn, wasPaused, whyBefore);
	}
}

void Router::Char(uint16_t aUnit)
{
	std::lock_guard               lock(mutex_);
	const std::optional<uint32_t> point = text_.Take(aUnit);
	if (point && Printable(*point) && ScreenOn())
	{
		Push({ proto::kInText, 0, int32_t(*point), 0, 0 });
		++charsTyped_;
	}
}

void Router::SetLink(link::Link* aLink)
{
	std::lock_guard lock(mutex_);
	link_ = aLink;
}

void Router::Update(bool aLinked, const link::Link::Status& aState, const std::string& aVBlocked)
{
	std::lock_guard lock(mutex_);
	situation_.linked = aLinked;
	situation_.inGame = aState.inGame;
	situation_.minecraft = aState.minecraft;
	situation_.minecraftInWorld = aState.inWorld;
	situation_.blocked = (aState.minecraftFlags & proto::kMcDead) ? "Minecraft's player is dead" : aVBlocked;
	if (aState.screenOpen && !situation_.screenOpen)
	{
		ReadSpeed();  // so a change in Windows applies without a restart
	}
	situation_.screenOpen = aState.screenOpen;
	cursor_.SetScreen(aState.screenOpen);
	closeable_ = aLinked && aState.minecraft && aState.inWorld && !(aState.minecraftFlags & proto::kMcDead) && aState.screenOpen;
	if (mode_.On())
	{
		std::string why = !aState.minecraft             ? "Minecraft has gone"
		                  : !aState.inWorld             ? "Minecraft left its world"
		                  : !aLinked                    ? "unlinked"
		                  : !situation_.blocked.empty() ? situation_.blocked
		                  : !aState.inGame              ? "V isn't found"
		                                                : std::string();
		if (!why.empty())
		{
			Off(std::move(why));
		}
	}
	Refresh();
}

void Router::SetHudArea(uint32_t aImageWidth, uint32_t aImageHeight, uint32_t aScale)
{
	std::lock_guard lock(mutex_);
	cursor_.SetArea(aImageWidth, aImageHeight, aScale);
	Publish();
}

void Router::SetHudCursor(HudCursor* aCursor)
{
	std::lock_guard lock(mutex_);
	hudCursor_ = aCursor;
	Publish();
}

int64_t Router::TakeMouseX()
{
	const int64_t   counts = mouseX_.exchange(0);
	std::lock_guard lock(mutex_);
	return mode_.Paused() || ScreenOn() ? 0 : counts;  // CET's overlay's cursor, or Minecraft's, which turn nothing
}

std::optional<std::string> Router::TurnOn()
{
	std::lock_guard lock(mutex_);
	if (mode_.On())
	{
		return std::nullopt;
	}
	if (!mode_.TurnOn(situation_))
	{
		log_.Push("build mode stays off: " + mode_.Why());
		return mode_.Why();
	}
	log_.Push("build mode on");
	LogPause(false);
	Refresh();
	return std::nullopt;
}

bool Router::TurnOff(std::string aWhy)
{
	std::lock_guard lock(mutex_);
	return Off(std::move(aWhy));
}

bool Router::On() const
{
	std::lock_guard lock(mutex_);
	return mode_.On();
}

bool Router::TakeLinkRequest()
{
	std::lock_guard lock(mutex_);
	return std::exchange(linkRequested_, false);
}

std::optional<double> Router::TakePress()
{
	std::lock_guard lock(mutex_);
	return std::exchange(press_, std::nullopt);
}

InputReport Router::Report() const
{
	std::lock_guard lock(mutex_);
	InputReport     report{ mode_.On(), mode_.Why(), mode_.Slot(), read_.load(), mode_.Forwarded(), mode_.Hidden(), mode_.Last(), mode_.Paused() };
	report.screenOpen = situation_.screenOpen;
	report.screen = ScreenOn();
	report.cursor = cursor_.Image();
	report.imageWidth = cursor_.ImageWidth();
	report.imageHeight = cursor_.ImageHeight();
	report.speed = speed_;
	report.cursorEvents = cursorEvents_;
	report.f3Taken = mode_.F3Taken();
	report.charsTyped = charsTyped_;
	report.charsRefused = text_.Refused();
	return report;
}

void Router::FrontLost()
{
	std::lock_guard lock(mutex_);
	if (mode_.LostFront())
	{
		Push({ proto::kInReleaseAll, 0, 0, 0, 0 });
		log_.Push("the game lost the front in build mode: Minecraft lets go of everything");
	}
	Refresh();
}

void Router::SetOverlay(bool aOpen)
{
	std::lock_guard lock(mutex_);
	const bool      wasPaused = mode_.Paused();
	if (mode_.SetOverlay(aOpen))
	{
		Push({ proto::kInReleaseAll, 0, 0, 0, 0 });
	}
	LogPause(wasPaused);
	Refresh();
}

void Router::Apply(const Decision& aDecision, bool aWasOn, bool aWasPaused, const std::string& aWhyBefore)
{
	if (aDecision.releaseAll)
	{
		Push({ proto::kInReleaseAll, 0, 0, 0, 0 });
	}
	if (aWasOn && !mode_.On())
	{
		CloseScreen();  // F8, with Minecraft's screen open
	}
	for (size_t i = 0; i < aDecision.forwarded; ++i)
	{
		Push(aDecision.forward[i]);
	}
	if (aDecision.press)
	{
		press_ = SteadySeconds();
	}
	if (aDecision.requestLink)
	{
		linkRequested_ = true;
		log_.Push("F8: linking first, then build mode");
	}
	if (mode_.On() != aWasOn)
	{
		log_.Push(mode_.On() ? "build mode on" : "build mode off: " + mode_.Why());
	}
	else if (!mode_.On() && mode_.Why() != aWhyBefore)
	{
		log_.Push("build mode stays off: " + mode_.Why());
	}
	LogPause(aWasPaused);
	Refresh();
}

void Router::LogPause(bool aWasPaused)
{
	if (mode_.Paused() && !aWasPaused)
	{
		log_.Push("build mode paused: CET's overlay is open");
	}
	else if (!mode_.Paused() && aWasPaused && mode_.On())
	{
		log_.Push("build mode goes on: CET's overlay closed");
	}
}

bool Router::Off(std::string aWhy)
{
	if (!mode_.TurnOff(std::move(aWhy)))
	{
		return false;
	}
	Push({ proto::kInReleaseAll, 0, 0, 0, 0 });
	CloseScreen();
	log_.Push("build mode off: " + mode_.Why());
	Refresh();
	return true;
}

void Router::Push(const proto::InputEvent& aEvent)
{
	if (link_)
	{
		link_->Push(aEvent);  // a full ring drops it, and the link counts it
	}
}

bool Router::ScreenOn() const
{
	return mode_.On() && !mode_.Paused() && situation_.screenOpen;
}

void Router::Refresh()
{
	const bool on = ScreenOn();
	if (on != screen_.load())
	{
		screen_ = on;
		log_.Push(on ? "a Minecraft screen is build mode's: the mouse moves Minecraft's cursor, and every key is Minecraft's"
		             : "no Minecraft screen is build mode's: the mouse turns V again");
	}
	Publish();
}

void Router::Publish()
{
	if (hudCursor_)
	{
		const Pixel back = cursor_.Back();
		hudCursor_->Set({ back.x, back.y, screen_.load() && cursor_.HasArea() });
	}
}

void Router::CloseScreen()
{
	if (closeable_)
	{
		Push({ proto::kInKey, kSdlEscape, 1, 0, 0 });
		Push({ proto::kInKey, kSdlEscape, 0, 0, 0 });
		log_.Push("build mode ended with a Minecraft screen open: Esc closes it");
	}
}

void Router::ReadSpeed()
{
	speed_ = speedCheck_();
	factor_ = PointerFactor(speed_.speed);
	if (!speedLogged_ || speedLogged_->speed != speed_.speed || speedLogged_->enhanced != speed_.enhanced)
	{
		speedLogged_ = speed_;
		log_.Push(std::format("Windows' pointer speed is {} of 20: Minecraft's cursor moves {:.3f} back-buffer pixels a count{}", speed_.speed,
		                      factor_, speed_.enhanced ? ", without \"Enhance pointer precision\"'s curve" : ""));
	}
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
Expected: no error or warning lines, then `550 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-screens: the router moves Minecraft's cursor and types its text while a screen is build mode's, and closes it as build mode ends" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 11: The HUD pass draws the arrow

`HudConstants` grows from 4 values to 8, which root parameter 1 already has: the cursor's tip in back-buffer pixels,
and its arrow's height, 0 for none. `HudPS` tests the arrow first (`CursorPart`, `CursorAt`'s arithmetic), since the
image pass returns nothing outside the image. The renderer reads the router's spot as it records the pass at Present,
from `HudCursor`'s atomic, so Present never waits on the router's lock.

**Files:**
- Modify: `src/render/FrameConstants.h`, `shaders/blocks.hlsl`, `src/render/Renderer.h`, `src/render/Renderer.cpp`

- [x] **Step 1: Edit `src/render/FrameConstants.h` to grow `HudConstants`**

```cpp
	uint32_t width = 0;     // the image's
	uint32_t height = 0;
	uint32_t scale = 1;     // back-buffer pixels per image texel, each way
	uint32_t bottomUp = 0;  // 1 if its rows are stored bottom-up
};
static_assert(sizeof(HudConstants) == 16);
```
```cpp
	uint32_t width = 0;         // the image's
	uint32_t height = 0;
	uint32_t scale = 1;         // back-buffer pixels per image texel, each way
	uint32_t bottomUp = 0;      // 1 if its rows are stored bottom-up
	uint32_t cursorX = 0;       // spike 8: the cursor's tip, in back-buffer pixels
	uint32_t cursorY = 0;
	uint32_t cursorHeight = 0;  // its arrow's height in back-buffer pixels; 0: no cursor
	uint32_t unused = 0;
};
static_assert(sizeof(HudConstants) == 32);  // root parameter 1 has 8 values
```

- [x] **Step 2: Build and run the tests; the shader check must fail**

The shader's `Hud` buffer is still 16 bytes.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | grep -v "^PASS" | tail -2
```
Expected: `FAIL ShadersCompileFromTheShaderFile` and `550 test(s), 1 failure(s)`.

- [x] **Step 3: Edit `shaders/blocks.hlsl` to draw the arrow**

```hlsl
    uint4 hudInfo;  // the image's width and height; back-buffer pixels per texel; 1 if its rows are stored bottom-up
```
```hlsl
    uint4 hudInfo;    // the image's width and height; back-buffer pixels per texel; 1 if its rows are stored bottom-up
    uint4 hudCursor;  // spike 8: the cursor's tip in back-buffer pixels; its arrow's height, 0 for none; unused
```
```hlsl
float4 HudPS(float4 position : SV_Position) : SV_Target0
{
```
```hlsl
// Spike 8: the part of the cursor's arrow at a pixel's centre, SkyCraft's arrow: 0 none, 1 its black edge, 2 its white
// fill. src/render/Hud.cpp's CursorAt does the same arithmetic.
uint CursorPart(float2 position)
{
    if (hudCursor.z == 0)
    {
        return 0;
    }
    const float  height = (float)hudCursor.z;
    const float  edge = 1.5 * height / 18.0;
    const float2 p = position - (float2)hudCursor.xy;
    if (p.x < 0.0 || p.y < 0.0 || p.y >= height || p.x > p.y * 0.6)
    {
        return 0;
    }
    return p.x < edge || p.x > p.y * 0.6 - edge || p.y > height - edge ? 1 : 2;
}

float4 HudPS(float4 position : SV_Position) : SV_Target0
{
    const uint part = CursorPart(position.xy);
    if (part != 0)
    {
        return part == 1 ? float4(0.0, 0.0, 0.0, 1.0) : float4(1.0, 1.0, 1.0, 1.0);  // opaque, over the image
    }
```

- [x] **Step 4: Edit `src/render/Renderer.h` to take the cursor's spot**

```cpp
	HudSize BackBufferSize() const { return { backWidth_.load(), backHeight_.load() }; }  // as the last Present saw it
```
```cpp
	HudSize BackBufferSize() const { return { backWidth_.load(), backHeight_.load() }; }  // as the last Present saw it
	// Spike 8: where the HUD pass finds the cursor, which it draws over the image while it's shown; null for none.
	void SetHudCursor(const HudCursor* aCursor) { hudCursor_.store(aCursor); }
```
```cpp
	std::atomic<bool>                                          hudWanted_{ false };
```
```cpp
	std::atomic<bool>                                          hudWanted_{ false };
	std::atomic<const HudCursor*>                              hudCursor_{ nullptr };
```

- [x] **Step 5: Edit `src/render/Renderer.cpp` to give it to the HUD pass**

```cpp
	const HudConstants hud{ hudShown_.width, hudShown_.height, scale, hudShown_.bottomUp ? 1u : 0u };
	list->SetGraphicsRoot32BitConstants(1, 4, &hud, 0);
```
```cpp
	// Spike 8: the cursor's newest spot, as the input thread left it, so the arrow moves in the frame the mouse does.
	const HudCursor*      cursor = hudCursor_.load();
	const HudCursor::Spot spot = cursor ? cursor->Get() : HudCursor::Spot{};
	const HudConstants    hud{ hudShown_.width, hudShown_.height, scale, hudShown_.bottomUp ? 1u : 0u, spot.x, spot.y,
                            spot.shown ? CursorHeight(uint32_t(target.Height)) : 0u, 0u };
	list->SetGraphicsRoot32BitConstants(1, 8, &hud, 0);
```

- [x] **Step 6: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `550 test(s), 0 failure(s)`.

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-screens: the HUD pass draws the cursor's arrow at its newest spot" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 12: The `PeekMessageW` patch

As `GetRawInputData`'s (`input/RawInputHook`): the executable's import, found by name and pointed at the replacement
with one atomic write once the router and the original are stored. The replacement calls the original, and for a
message it took off the queue (`PM_REMOVE`, and a true return) that is a `WM_CHAR`, hands its code unit to
`Router::Char`. The game's pump gets every message as before. Only the game can test it; here it must build.

**Files:**
- Create: `src/input/MessageHook.h`, `src/input/MessageHook.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write `src/input/MessageHook.h`**

```cpp
#pragma once

#include "LogQueue.h"
#include "input/Router.h"

// The patch on Cyberpunk2077.exe's import of PeekMessageW (spike 8's "Typed text"). The game's one message pump takes
// every message through it with PM_REMOVE, then calls TranslateMessage, which makes each key's WM_CHAR, and
// DispatchMessageW; its other call only peeks for sent messages. The replacement calls the original, then hands each
// WM_CHAR it took off the queue to the router, which types it into Minecraft's screen while one is build mode's. The
// game gets every message unchanged. Only the game can test this file.
namespace cpmc::input
{
// Patches the import. False, with nothing patched, if the exe doesn't import it by name.
bool InstallMessageHook(Router& aRouter, LogQueue& aLog);

// Puts the import back. A call already in the replacement finishes through the router, which outlives it.
void RemoveMessageHook();
}
```

- [x] **Step 2: Write `src/input/MessageHook.cpp`**

```cpp
#include "input/MessageHook.h"

#include "hooks/ImportPatch.h"

#include <Windows.h>

#include <optional>

namespace cpmc::input
{
namespace
{
using PeekMessageWFn = BOOL(WINAPI*)(LPMSG, HWND, UINT, UINT, UINT);

PeekMessageWFn             g_original = nullptr;
Router*                    g_router = nullptr;
std::optional<ImportPatch> g_patch;

BOOL WINAPI PeekMessageHook(LPMSG aMessage, HWND aWindow, UINT aFirst, UINT aLast, UINT aRemove)
{
	const BOOL taken = g_original(aMessage, aWindow, aFirst, aLast, aRemove);
	// Only a message taken off the queue: a peek that leaves it there would see it again.
	if (taken && aMessage && (aRemove & PM_REMOVE) && aMessage->message == WM_CHAR)
	{
		g_router->Char(uint16_t(aMessage->wParam));  // a UTF-16 code unit, from the W function
	}
	return taken;
}
}

bool InstallMessageHook(Router& aRouter, LogQueue& aLog)
{
	std::optional<ImportPatch> patch = FindImport(GetModuleHandleW(nullptr), "user32.dll", "PeekMessageW");
	if (!patch)
	{
		aLog.Push("the game doesn't import PeekMessageW by name: typing in Minecraft's screens won't work");
		return false;
	}
	// Both before the patch, so no call reaches the replacement without them.
	g_router = &aRouter;
	g_original = reinterpret_cast<PeekMessageWFn>(patch->original);
	if (!ApplyImport(*patch, reinterpret_cast<void*>(&PeekMessageHook)))
	{
		aLog.Push("PeekMessageW's import couldn't be patched: typing in Minecraft's screens won't work");
		return false;
	}
	g_patch = *patch;
	aLog.Push("PeekMessageW hooked: what's typed reaches Minecraft's screens");
	return true;
}

void RemoveMessageHook()
{
	if (g_patch)
	{
		RestoreImport(*g_patch);
		g_patch.reset();
	}
}
}
```

- [x] **Step 3: Edit `CMakeLists.txt` to add the source**

```cmake
	src/input/RawInputHook.cpp
```
```cmake
	src/input/RawInputHook.cpp
	src/input/MessageHook.cpp
```

- [x] **Step 4: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `550 test(s), 0 failure(s)`.

- [x] **Step 5: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-screens: the PeekMessageW patch, which hands each WM_CHAR the game's pump takes to the router" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 13: BlockRender 0.23.0: the wiring and the status

- **The status's `screens=` line,** after `build=` (the spec's "Status and log"): whether a Minecraft screen is open and
  build mode's, the cursor in the image's pixels, Windows' speed and factor (and whether its precision curve is left
  out), cursor events sent, keys taken by the F3 rule, characters typed and lone surrogates dropped, and whether
  `PeekMessageW` is hooked.
- **The plugin:**
  - one `HudCursor`, given to the router and the renderer at load;
  - the `PeekMessageW` patch installed beside `GetRawInputData`'s, and removed at unload;
  - the newest HUD image's size kept from the sink, and given to the router each frame before its update, with the
    scale the HUD pass draws it at (`HudScaleFor`);
  - the report's new fields;
  - `build on`'s reply naming the new keys;
  - version 0.23.0.

**Files:**
- Modify: `src/Status.h`, `src/Status.cpp`, `src/Plugin.cpp`, `tests/TestStatus.cpp`

- [x] **Step 1: Edit `tests/TestStatus.cpp` to add the failing test**

```cpp
	CHECK(status.find("link=") < status.find("build=") && status.find("build=") < status.find("positions="));
```
```cpp
	CHECK(status.find("link=") < status.find("build=") && status.find("build=") < status.find("positions="));
}

TEST(StatusWritesTheScreens)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	link.screenOpen = true;
	link.screenOurs = true;
	link.cursorX = 640;
	link.cursorY = 360;
	link.cursorImageWidth = 1280;
	link.cursorImageHeight = 720;
	link.pointerSpeed = 8;
	link.pointerFactor = 0.75;
	link.cursorEvents = 152;
	link.f3Taken = 2;
	link.charsTyped = 37;
	link.charsRefused = 1;
	link.messageHooked = true;
	report.link = link;
	std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "screens=a Minecraft screen open, the cursor at 640, 360 of 1280 x 720; Windows speed 8 of 20: 0.750 px a "
	                  "count; 152 cursor events sent; 2 keys taken by the F3 rule; 37 characters typed, 1 lone surrogate "
	                  "dropped; PeekMessageW hooked"));
	CHECK(status.find("build=") < status.find("screens=") && status.find("screens=") < status.find("drive="));
	link.cursorImageWidth = 0;  // no HUD image has come
	report.link = link;
	CHECK(cpmc::FormatStatus(report).find("\nscreens=a Minecraft screen open, and no HUD image for the cursor yet;") != std::string::npos);
	link.screenOurs = false;
	report.link = link;
	CHECK(cpmc::FormatStatus(report).find("\nscreens=a Minecraft screen open, but build mode is off or paused;") != std::string::npos);
	link.screenOpen = false;
	link.pointerEnhanced = true;
	link.messageHooked = false;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "screens=no Minecraft screen open; Windows speed 8 of 20: 0.750 px a count, without "
	                                      "its precision curve; 152 cursor events sent; 2 keys taken by the F3 rule; 37 "
	                                      "characters typed, 1 lone surrogate dropped; PeekMessageW not hooked (see the log)"));
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -o -E "error C[0-9]+: [^[]*" | sort -u | head -3
```
Expected: `error C2039: 'screenOurs': is not a member of 'cpmc::LinkReport'`.

- [x] **Step 3: Edit `src/Status.h` to add the report's fields**

```cpp
	std::string        lastInput;            // the last forwarded
```
```cpp
	std::string        lastInput;            // the last forwarded
	// Spike 8: Minecraft's screens, the cursor and typed text; screenOpen is Minecraft's own flag
	bool     screenOurs = false;     // a Minecraft screen is build mode's
	uint32_t cursorX = 0;            // in the HUD image's pixels
	uint32_t cursorY = 0;
	uint32_t cursorImageWidth = 0;   // the HUD image the cursor stays in; 0 before one has come
	uint32_t cursorImageHeight = 0;
	int      pointerSpeed = 10;      // Windows', 1 to 20
	double   pointerFactor = 1.0;    // back-buffer pixels a count
	bool     pointerEnhanced = false;  // "Enhance pointer precision", whose curve the cursor leaves out
	uint64_t cursorEvents = 0;       // kInCursor sent
	uint64_t f3Taken = 0;            // keys Minecraft got because F3 was held
	uint64_t charsTyped = 0;         // kInText sent
	uint64_t charsRefused = 0;       // lone surrogates
	bool     messageHooked = false;  // PeekMessageW's import is patched
```

- [x] **Step 4: Edit `src/Status.cpp` to write the line**

```cpp
	                                     : std::string("GetRawInputData not hooked (see the log)"));
```
```cpp
	                                     : std::string("GetRawInputData not hooked (see the log)"));
	const std::string screen = !aLink.screenOpen               ? std::string("no Minecraft screen open")
	                           : !aLink.screenOurs             ? std::string("a Minecraft screen open, but build mode is off or paused")
	                           : aLink.cursorImageWidth == 0 ? std::string("a Minecraft screen open, and no HUD image for the cursor yet")
	                                                           : std::format("a Minecraft screen open, the cursor at {}, {} of {} x {}", aLink.cursorX,
	                                                                         aLink.cursorY, aLink.cursorImageWidth, aLink.cursorImageHeight);
	out += std::format("screens={}; Windows speed {} of 20: {:.3f} px a count{}; {} cursor events sent; {} keys taken by the F3 "
	                   "rule; {} characters typed, {} lone surrogate{} dropped; {}\n",
	                   screen, aLink.pointerSpeed, aLink.pointerFactor, aLink.pointerEnhanced ? ", without its precision curve" : "",
	                   aLink.cursorEvents, aLink.f3Taken, aLink.charsTyped, aLink.charsRefused, aLink.charsRefused == 1 ? "" : "s",
	                   aLink.messageHooked ? "PeekMessageW hooked" : "PeekMessageW not hooked (see the log)");
```

- [x] **Step 5: Edit `src/Plugin.cpp` to wire it all in, as 0.23.0**

```cpp
#include "hooks/StreamlineHooks.h"
```
```cpp
#include "hooks/StreamlineHooks.h"
#include "input/MessageHook.h"
```
```cpp
constexpr const char* kVersion = "0.22.0";
```
```cpp
constexpr const char* kVersion = "0.23.0";
```
```cpp
bool                      g_inputHooked = false;  // GetRawInputData's import is patched
```
```cpp
bool                      g_inputHooked = false;  // GetRawInputData's import is patched
bool                      g_messageHooked = false;  // PeekMessageW's import is patched (spike 8)
```
```cpp
cpmc::input::Router* g_router = nullptr;  // where keys and clicks go (the spec's "Input")
```
```cpp
cpmc::input::Router* g_router = nullptr;  // where keys and clicks go (the spec's "Input")
cpmc::HudCursor      g_hudCursor;         // spike 8: Minecraft's cursor, which the router keeps and the HUD pass draws
cpmc::HudSize        g_hudImage;          // the newest HUD image's size, which the cursor stays in
```
```cpp
	{
		g_renderer->SetHudImage(aFrame.width, aFrame.height, aFrame.bottomUp, aFrame.pixels);
```
```cpp
	{
		g_hudImage = { aFrame.width, aFrame.height };
		g_renderer->SetHudImage(aFrame.width, aFrame.height, aFrame.bottomUp, aFrame.pixels);
```
```cpp
	const cpmc::HudSize viewport = cpmc::HudViewport(back.width, back.height, g_hudScale);
```
```cpp
	const cpmc::HudSize viewport = cpmc::HudViewport(back.width, back.height, g_hudScale);
	// Spike 8: the cursor stays in the area the newest HUD image covers, drawn as the HUD pass draws it.
	g_router->SetHudArea(g_hudImage.width, g_hudImage.height, cpmc::HudScaleFor(back.width, back.height, g_hudImage.width, g_hudImage.height));
```
```cpp
		           : "build on: Minecraft's player drives V; the movement keys, the mouse buttons, the wheel and 1 to 9 are Minecraft's");
```
```cpp
		           : "build on: Minecraft's player drives V; the movement keys, the mouse buttons, the wheel, 1 to 9, E, T, /, Q, F3 "
		             "and O are Minecraft's, and with a Minecraft screen open every key");
```
```cpp
	report.lastInput = input.last;
```
```cpp
	report.lastInput = input.last;
	report.screenOurs = input.screen;
	report.cursorX = input.cursor.x;
	report.cursorY = input.cursor.y;
	report.cursorImageWidth = input.imageWidth;
	report.cursorImageHeight = input.imageHeight;
	report.pointerSpeed = input.speed.speed;
	report.pointerFactor = cpmc::input::PointerFactor(input.speed.speed);
	report.pointerEnhanced = input.speed.enhanced;
	report.cursorEvents = input.cursorEvents;
	report.f3Taken = input.f3Taken;
	report.charsTyped = input.charsTyped;
	report.charsRefused = input.charsRefused;
	report.messageHooked = g_messageHooked;
```
```cpp
		g_router = new cpmc::input::Router(g_log);
		g_inputHooked = cpmc::input::InstallRawInputHook(*g_router, g_log);
```
```cpp
		g_router = new cpmc::input::Router(g_log);
		g_router->SetHudCursor(&g_hudCursor);
		g_renderer->SetHudCursor(&g_hudCursor);
		g_inputHooked = cpmc::input::InstallRawInputHook(*g_router, g_log);
		g_messageHooked = cpmc::input::InstallMessageHook(*g_router, g_log);
```
```cpp
			cpmc::input::RemoveRawInputHook();
```
```cpp
			cpmc::input::RemoveRawInputHook();
			cpmc::input::RemoveMessageHook();
```

- [x] **Step 6: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `551 test(s), 0 failure(s)`.

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-screens: BlockRender 0.23.0 wires the screens in: the PeekMessageW patch, the cursor's area and spot, and the screens status line" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 14: In the game: run E

Run E of the spec's "Testing", with the user, on BlockRender 0.23.0, linked to SkyCraft's Minecraft in the `CyberCraft`
instance, Minecraft's window hidden as always. V somewhere open, by day; build mode on with F8.

**What the agent sends,** each a request, as `mc` and Minecraft's command, typed into Minecraft's chat, and only while
no Minecraft screen is open (the typer refuses then):
- **the test blocks,** 3 blocks ahead of Minecraft's player, a crafting table to the right and a chest to the left,
  each at a marker tagged `sp8b`, set without updates:
  ```text
  mc /execute rotated ~ 0 positioned ^1 ^ ^3 run summon minecraft:marker ~ ~ ~ {Tags:["sp8b","sp8table"]}
  mc /execute at @e[tag=sp8table] run setblock ~ ~ ~ minecraft:crafting_table strict
  mc /execute rotated ~ 0 positioned ^-1 ^ ^3 run summon minecraft:marker ~ ~ ~ {Tags:["sp8b","sp8chest"]}
  mc /execute at @e[tag=sp8chest] run setblock ~ ~ ~ minecraft:chest[facing=north] strict
  ```
- **the clear,** at the end: the blocks at their markers to air, without updates, so the chest's contents vanish with
  it, then the markers, then any item dropped within 10 blocks:
  ```text
  mc /execute at @e[tag=sp8b] run setblock ~ ~ ~ minecraft:air strict
  mc /kill @e[tag=sp8b]
  mc /kill @e[type=minecraft:item,distance=..10]
  ```

These commands come back, as in spike 7's runs:
- **Send a request,** atomically: the plugin runs it within a second. Replace `REQUEST` with the request, with each `"`
  in it written `\"`, and send the next only once the status's `request=` line shows the last.
  ```bash
  P=/e/Cyberpunk2077/red4ext/plugins/BlockRender; printf '%s\n' "REQUEST" > "$P/request.tmp" && mv "$P/request.tmp" "$P/blockrender.request"
  ```
- **The log's newest lines:**
  ```bash
  L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); tail -30 "/e/Cyberpunk2077/red4ext/logs/$L"
  ```
- **The status:**
  ```bash
  cat /e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud/status.txt
  ```
- **Minecraft's log, for the chat:**
  ```bash
  grep -E "\[CHAT\]" "/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log" | tail -10
  ```

**If the game freezes,** read the logs and the newest folder in `%LOCALAPPDATA%\REDEngine\ReportQueue`, and force-close
it only when the user asks. If a check fails, stop and find the cause with superpowers:systematic-debugging before
going on. Fixes become new tasks.

**Files:**
- Modify: `docs/spikes/2026-10-07-minecraft-screens-results.md`

- [x] **Step 1: Deploy (the game must not be running)**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game first, because a running game locks the DLL.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
P=/e/Cyberpunk2077/red4ext/plugins/BlockRender
H=/e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default -DCP2077_DIR=E:/Cyberpunk2077 > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C|error MSB"; \
  "$CMAKE" --preset default -DCP2077_DIR= > /dev/null; \
  cmp build/RelWithDebInfo/BlockRender.dll "$P/BlockRender.dll" && cmp shaders/blocks.hlsl "$P/shaders/blocks.hlsl" \
  && cmp tools/blockrender_hud/init.lua "$H/init.lua" && echo installed
```
Expected: `installed`. The second configure clears `CP2077_DIR` again.

- [x] **Step 2: In the game: BlockRender 0.23.0 loads**

Ask the user to start the `CyberCraft` instance, then the game, and to wait at the game's main menu. Then:
```bash
L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); grep -E "loaded|hooked|pointer speed|ERROR|error" "/e/Cyberpunk2077/red4ext/logs/$L" | head -8
```
Expected: `BlockRender 0.23.0 loaded (game 2.31…)`; `GetRawInputData hooked; the focus watch runs`; `PeekMessageW
hooked: what's typed reaches Minecraft's screens`; `Windows' pointer speed is 8 of 20: Minecraft's cursor moves 0.750
back-buffer pixels a count`; and no error lines.

- [x] **Step 3: In the game: link, and build mode**

Ask the user to load a save somewhere open by day, click Link in the overlay, close it, and press F8. Expected: in the
log, `build mode on`; in the status, `build=on`, then `screens=no Minecraft screen open; Windows speed 8 of 20: 0.750 px
a count; 0 cursor events sent; …; PeekMessageW hooked`.

- [x] **Step 4: In the game: the inventory (check 1)**

Ask the user to press E, move the mouse around, then press Esc. Expected:
- the inventory in the HUD, the arrow at the screen's middle, following the mouse at the desktop's speed, sharp, white
  with a black edge, 24 pixels tall;
- V's view not turning while it's open;
- in the log, `a Minecraft screen is build mode's: …`, then `no Minecraft screen is build mode's: …` after Esc;
- in the status while it's open, `screens=a Minecraft screen open, the cursor at …` with the cursor's pixel changing;
- after Esc, the inventory gone, and the game's own menu NOT opened (risk 8).

- [x] **Step 5: In the game: moving items in survival (check 2)**

Send `mc /gamemode survival`. Ask the user, in the inventory, to pick up and put down an item, shift-click one between
the hotbar and the inventory, drag half the golden apples across a few slots and put them back in slot 6 as 64, press
a number key over a slot, and press Q over one; then close it with E. Expected: each as in Minecraft, the hover
highlight following the arrow, a carried item trailing it a little; then send the clear's last request (`/kill` of the
items within 10 blocks) for what Q dropped: Minecraft's chat says `Killed …`.

- [x] **Step 6: In the game: the creative inventory (check 3)**

Send `mc /gamemode creative`. Ask the user to press E, click through a few tabs, scroll one with the wheel, open the
search tab and type `stone`, then close it with Esc. Expected: as in Minecraft; the typed letters in the search box;
the status's `slot` unchanged by the wheel and number keys while it was open.

- [x] **Step 7: In the game: chat (check 4)**

Ask the user to press T, type `Hej æøå é @ ~` (capitals with Shift, @ with AltGr+2, é with ´ then e), try Backspace,
the arrow keys, Ctrl+A and Ctrl+C, then Ctrl+V to paste it twice, and send it with Enter; then to press Minecraft's
command key (`-` on their keyboard), type `time query daytime` and press Enter. Expected: in Minecraft's chat, the line
`<ItzBloxy> Hej æøå é @ ~…` with the letters whole; then `The time is …`; T opening no phone in the game.

- [x] **Step 8: In the game: F3 (check 5)**

Ask the user to tap F3 (the debug screen shows; again, it goes), then hold F3 and press F4 a few times (the switcher
cycles), let go of F3 on Survival, then do it again back to Creative; then F3+N into spectator, look around without
moving, and F3+N back. Expected: Minecraft's chat says `Set own game mode to …` each time; V stays where it stood.

- [x] **Step 9: In the game: the pause menu (check 6)**

Ask the user to press O, open Options, drag a slider and put it back where it was, click Done, then press Esc.
Expected: Minecraft's pause menu, its blurred background over Night City, the slider dragging with the arrow; Esc
closing it and no game menu opening.

- [x] **Step 10: In the game: a crafting table and a chest (check 7)**

Send the test blocks' four requests. Ask the user to look at the crafting table and right-click it, craft something
(planks from a log, say, if they have one; or just move an item in and out), close it with Esc; then right-click the
chest, put an item in, and with the chest still open press F8. Expected: the chest closes and build mode ends (log:
`build mode ended with a Minecraft screen open: Esc closes it`). Then ask them to press F8 again, open the chest,
and press Shift+F6 to open CET's overlay, then Shift+F6 again. Expected: build mode paused while the overlay is open,
the arrow gone; back with the chest still open and the arrow where it was.

- [x] **Step 11: In the game: Alt-Tab (check 8)**

With the chest open, ask the user to Alt-Tab to Claude Code and back. Expected: the log's `the game lost the front in
build mode: Minecraft lets go of everything`; back in the game, the chest still open, and the arrow working. Then Esc.

- [x] **Step 12: In the game: the keys left to the game (check 9)**

Ask the user to press Esc with no Minecraft screen open. Expected: build mode ends (`Esc left it`) and the game's menu
opens, as before. Then send the clear's three requests. Expected: in Minecraft's chat, `Changed the block at …` twice,
`Killed …` for the markers, and for the items, `Killed …` or `No entity was found`; the two blocks gone.

- [x] **Step 13: Ask the user to quit the game and stop Minecraft**

- [x] **Step 14: Read the log once more**

```bash
L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); grep -E "build mode|screen|pointer|hooked|request" "/e/Cyberpunk2077/red4ext/logs/$L" | head -200
```
Everything the steps above logged, for the results.

- [x] **Step 15: Record run E's results in `docs/spikes/2026-10-07-minecraft-screens-results.md`**

A "Run E" section: the versions; each check above with its outcome; how the cursor felt; anything that surprised.
Then the spike's answer at the top.

- [x] **Step 16: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes docs/superpowers/plans/2026-10-07-minecraft-screens.md \
  && git commit -q -m "minecraft-screens: run E's results" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 15: Close the spike

- [x] **Step 1: Record how Part 2 went, in this plan**

At the end, after "Found while planning Part 2": what ran as written, what changed, and what run E found.

- [x] **Step 2: Record the spec's changes while executing Part 2**

In its "Changes while executing Part 2": anything Part 2 changed in the design, and what's left for later.

- [x] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/superpowers \
  && git commit -q -m "minecraft-screens plan: how Part 2 went" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

- [x] **Step 4: Record the branch's finish**

Use superpowers:finishing-a-development-branch: the tests pass (551), and the user chooses whether
`spike/minecraft-screens` merges into `main`, as spikes 1 to 7 did.

## Found while planning Part 2

The spec's "Changes while planning Part 2" lists what planning changed in the design. In the plan itself:
- **One way through `PlaceKey`.** Build mode's keys used to be a fixed list of 23 holders checked one by one. With every
  key Minecraft's in a screen, every key needs a holder, so a key's new press is placed once, in one order (Task 9),
  and its repeats and release follow the holder.
- **F6 without Shift** used to be passed to the game at once. It now goes on as any key, so F3+F6 reaches Minecraft,
  and an F6 typed in a screen does too.
- **The Esc lives in the router,** in `CloseScreen`, which both of build mode's endings call: `Apply` after a key's
  decision ended it (F8), and `Off` for the rest. `closeable_` is worked out in each `Update`, from the frame's state,
  so an ending found in that same update (Minecraft leaving its world, its player dying) sends none.
- **The cursor's area comes from the newest HUD image,** which the plugin keeps from the link's sink, at the scale the
  HUD pass draws it (`HudScaleFor`), given to the router each frame before its update.
- **`HudCursor` packs x, y and shown into one atomic,** so the HUD pass at Present never reads an x from one move and a
  y from another.
- **The status's typed-text count** is of characters typed and lone surrogates dropped: control characters are expected
  on every Enter and Backspace, so they aren't counted as dropped.
- **An absolute mouse event** in a screen is left as it is, and moves nothing; the first mutation list found no test for
  it, so `RouterSendsTheCursorOnlyAsItsPixelChanges` checks one.
- **Run E's test blocks are placed by request,** at markers, as spike 7's run D placed its blocks, and cleared at them
  with `strict`, which drops nothing; the user doesn't place them. A `/kill` of items within 10 blocks clears what Q
  drops.
- **Risk 8, found while planning:** a key hidden from the game's Raw Input still reaches its window as `WM_KEYDOWN`.
  Spike 4 found that the game reads keys only through Raw Input, and the game imports no `GetKeyState` or
  `GetAsyncKeyState`, but Esc was never hidden before: run E checks that closing a Minecraft screen with Esc opens no
  game menu.

## How Part 2 went

Part 2 ran on 2026-10-07, as the user said to start.
- **Tasks 6 to 13** ran in the real repo through the plan runner, task range 6 to 13: every step's expected result held
  (each build's failure and the shader check's, then 531, 534, 537, 543, 550, 550, 550 and 551 tests, with no warnings).
  - **Commits:** `f13d76e` to `fd97e0e`, each tree identical to the scratch clone's `p2-t6` to `p2-t13`.
  - **The tags:** comparing them fetched the clone's tags into the repo again; they were deleted.
- **Task 14, run E,** passed every check (`1a6f432`, results in
  [2026-10-07-minecraft-screens-results.md](../../spikes/2026-10-07-minecraft-screens-results.md)). Risk 8 is settled:
  Esc closing a Minecraft screen opened no game menu. The user found the cursor smooth.
- **What didn't go as planned:**
  - **The slot check** needed a second try: the first 5 was pressed after the inventory closed, as the status's last
    input (`the key 5 up`, not `Esc up`) showed.
  - **The é** first arrived as `'e`: the apostrophe key pressed for the dead key. The ´ dead key, tried after, gave é.
- **Seen, and left as they are:**
  - the Windows key goes to Minecraft while a screen is open, as every key does, harmlessly;
  - the typer's own chat makes a screen build mode's for a moment, the spec's edge case.
