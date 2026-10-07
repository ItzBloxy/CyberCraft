# Minecraft's Screens Implementation Plan, Part 1 (step 0)

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

**Part 2** (BlockRender 0.23.0 and run E) is planned after step 0, from its results:
- build mode's screen state and its keys (`input/BuildMode`, `input/Router`);
- the cursor (`input/Cursor`) and its arrow in the HUD pass;
- typed text (`input/Text`, `input/MessageHook`);
- the status line;
- run E in the game.

Step 0 settles the pixels and the inputs Part 2 builds on.

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

- [ ] **Step 1: Record how Part 1 went, in this plan**

A section "How Part 1 went" at the plan's end: what ran as planned, what didn't and why, and what step 0's results
change for Part 2.

- [ ] **Step 2: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/superpowers/plans/2026-10-07-minecraft-screens.md \
  && git commit -q -m "minecraft-screens plan: how Part 1 went" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

- [ ] **Step 3: Record Part 2's plan**

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
