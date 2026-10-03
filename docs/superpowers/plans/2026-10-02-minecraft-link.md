# Minecraft Link Implementation Plan, Parts 1 to 3 (steps 1 to 3)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> In this project, plans run inline with superpowers:executing-plans, never through sub-agents.

**Goal:** Build BlockRender's side of SkyCraft's shared-memory protocol, and `mclink`, a console stand-in for
Cyberpunk that runs it against the user's real Minecraft with SkyCraft's unchanged mod. Then run spike 4's step 1
with the user: the link, the pinned player, the floor, typed commands, a click, TNT, the atlas and the HUD image, all
without Cyberpunk.

**Architecture:** the link is plain C++ in `render_core`, working on the mapping's memory, so it's tested without the
game or Minecraft.
- **`link/Rings`:** SkyCraft's seqlocked blocks and its four rings.
- **`link/Messages` and `link/Overlay`:** what Minecraft sends, checked before use: sections, the atlas, HUD images.
- **`link/Floor`, `link/Look` and `link/Commands`:** the stand-in floor, Minecraft's eye at V's camera, and commands
  typed into Minecraft's chat.
- **`link/Mapping`:** the named mapping, made as SkyCraft's Skyrim side makes it.
- **`link/Link`:** all of it once a frame, handing what arrives to a sink: Part 2's renderer, or `mclink`'s report.
- **`mclink`** (`tools/mclink/`) runs the link at 60 Hz from a script.

The unit tests play Minecraft's side in plain memory. Step 1 then checks the link against the real Minecraft.

**Tech stack:**
- C++20, MSVC 14.44 (Visual Studio 2022), CMake 3.31 (bundled with Visual Studio);
- SkyCraft's protocol header (version 11, MIT), and Win32 shared memory;
- Minecraft 26.3, Fabric Loader 0.19.5, Fabric API for 26.3 (SkyCraft takes any version) and
  `skycraft-fabric-0.1.2.jar`, in the user's Modrinth instance.

**Spec:** [docs/superpowers/specs/2026-10-02-minecraft-link-design.md](../specs/2026-10-02-minecraft-link-design.md)

**Parts 2 and 3:** each planned from what the step before found, as spike 3 planned its parts.
- **Part 2** is step 2 in the game: Tasks 13 to 29, which Task 12 planned from step 1's findings, all but Task 28,
  which step 2's run added. The plugin's link, the renderer's changes, the HUD pass, the status and the overlay, then
  the game run and what it found.
- **Part 3** is step 3 in the game: input. Tasks 30 to 40, which Task 29 planned from step 2's findings: build mode,
  the patch on the game's `GetRawInputData`, the router between the threads, the plugin and the overlay, then the game
  run and the spike's close.

Task 12 closes Part 1 and plans Part 2.

---

## Before you start

- **The branch:** work on `spike/minecraft-link`, which holds the handoff and the spec.
- **The shell:** commands are for Git Bash.
  - CMake isn't on PATH, and shell variables don't persist between tool calls, so every command block sets it:
    ```bash
    CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
    ```
  - Pass MSBuild switches as `-m`, never `/m`: Git Bash rewrites `/m` into a file path.
- **Already verified:** all the C++ in this plan has been compiled and tested once already, in a scratch clone.
  - Tasks 1 to 10 were then replayed in a fresh clone by a script that applied every file step exactly as written,
    and checked every build, test and `mclink` step's expected result. The results:
    - zero compiler warnings at `/W4`;
    - 175 C++ tests passing;
    - the replayed files identical to the scratch clone's.
  - Before this plan was written, `mclink` also ran against a stand-in for SkyCraft's Java side. That covered the
    commands' T, chat, text and Enter handshake, the floor, sections, an explosion and a HUD image.
  - A deep review then checked every task against the spec, SkyCraft's protocol header and its Java and Skyrim
    sides. "Changes from the deep review", at the end, lists what it changed.

  So a failure while following the plan most likely means a transcription slip: compare against the plan's code first.
- **Test counts:** 112 at the start. Then 123, 130, 134, 138, 143, 151, 155, 171 and 175 after Tasks 2 to 10.
- **What only Minecraft can test:** the link against SkyCraft's real mod, in Task 11, with the user.
- **The user and Minecraft:**
  - **The user does everything in Minecraft and the Modrinth App:** adding Fabric API, setting the instance's Java
    arguments, starting and stopping Minecraft. Never drive their mouse or keyboard: ask, then wait for them to say
    they're done.
  - **`mclink` acts only in SkyCraft's own world:** its typed commands and clicks go into the world SkyCraft creates.
- **The game:** nothing in Part 1 goes into Cyberpunk. The repo's CMake cache has `CP2077_DIR` empty, so builds don't
  deploy.
- **Commits** go on `spike/minecraft-link`, and end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## File map (Part 1)

All paths under `src/`, `tests/` and `tools/` are relative to `spikes/block-render/`.

| File | Responsibility |
|---|---|
| `extern/skycraft/` | SkyCraft's protocol header and licence, unchanged, and a README naming their commit |
| `CMakeLists.txt` | SkyCraft's include folder, the link's sources, `mclink`, and the new tests |
| `src/link/Rings.*` | `SkyState` and `McState`'s seqlocks, and the byte, input and event rings |
| `src/link/Messages.*` | Render messages into section meshes and the atlas, checked |
| `src/link/Overlay.*` | The reader's side of the HUD image's triple buffer |
| `src/link/Floor.*` | The stand-in floor's collision messages |
| `src/link/Look.*` | Minecraft's yaw and pitch from V's camera, and the teleport target |
| `src/link/Commands.*` | The command sets, and typing commands into Minecraft's chat |
| `src/link/Mapping.*` | The named mapping: create or reuse, refuse, reset, the heartbeat |
| `src/link/Link.*` | All of it once a frame, and the link's status |
| `tools/mclink/McLinkScript.*` | `mclink`'s script language |
| `tools/mclink/Main.cpp` | `mclink`: the runner and its report |
| `tools/mclink/step1.txt`, `tools/mclink/again.txt` | Step 1's scripts |
| `tests/TestRings.cpp` to `tests/TestMcLinkScript.cpp` | One test file per unit; `TestLink.cpp` has a fake Minecraft |

---

### Task 1: SkyCraft's protocol header

The user approved downloading SkyCraft on 2026-10-02. Two files come from its repository at commit
`bfcaf178524b92c2cdeb88e4ce0f13ef9ded6f32` (SkyCraft 0.1.2): the protocol header and the MIT licence, 23,667 bytes in
all.

**Files:**
- Create: `extern/skycraft/skycraft_protocol.h`, `extern/skycraft/LICENSE`, `extern/skycraft/README.md`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Check the branch**

```bash
cd /d/Cyberpunk-Minecraft && git switch spike/minecraft-link && git status --short && git log --oneline -1
```
Expected: no changed files, and this plan's commits (`minecraft-link plan: …`) at the top.

- [x] **Step 2: Download the header and the licence**

```bash
cd /d/Cyberpunk-Minecraft && mkdir -p extern/skycraft && cd extern/skycraft \
  && curl -fsSL -O https://raw.githubusercontent.com/chasmlol/SkyCraft/bfcaf178524b92c2cdeb88e4ce0f13ef9ded6f32/protocol/skycraft_protocol.h \
  && curl -fsSL -O https://raw.githubusercontent.com/chasmlol/SkyCraft/bfcaf178524b92c2cdeb88e4ce0f13ef9ded6f32/LICENSE \
  && sha256sum skycraft_protocol.h LICENSE && cat skycraft_protocol.h LICENSE | wc -c
```
Expected:
```
d7dd4da8201fabd34f97225b73cb2b2d9b1f70e7290978de33fa3cbad195a11e *skycraft_protocol.h
adcf5b9966dd6f78b54bd6872d8fccfe4bd8e5d51186aa982dde7cb628087dd1 *LICENSE
23667
```

- [x] **Step 3: Write `extern/skycraft/README.md`**

```markdown
# SkyCraft's shared-memory protocol

`skycraft_protocol.h` and `LICENSE` (MIT) copied unmodified from https://github.com/chasmlol/SkyCraft at commit
`bfcaf178524b92c2cdeb88e4ce0f13ef9ded6f32` (SkyCraft 0.1.2): `protocol/skycraft_protocol.h` and `LICENSE`.

It's the layout of the mapping `Local\SkyCraft_v1`, protocol version 11, that SkyCraft's Fabric mod
(`skycraft-fabric-0.1.2.jar`) opens. BlockRender plays the game side, as SkyCraft's Skyrim plugin does: see
`docs/superpowers/specs/2026-10-02-minecraft-link-design.md`. The header's own static checks pin the layout.
```

- [x] **Step 4: Edit `CMakeLists.txt` to add SkyCraft's include folder**

```cmake
set(STREAMLINE_DIR "${REPO_ROOT}/extern/streamline-2.7.2" CACHE PATH "Streamline 2.7.2 headers")
```
```cmake
set(STREAMLINE_DIR "${REPO_ROOT}/extern/streamline-2.7.2" CACHE PATH "Streamline 2.7.2 headers")
set(SKYCRAFT_DIR "${REPO_ROOT}/extern/skycraft" CACHE PATH "SkyCraft's protocol header")
```
```cmake
# Shared settings. Streamline's headers are SYSTEM, which keeps their warnings out of ours.
```
```cmake
# Shared settings. Streamline's and SkyCraft's headers are SYSTEM, which keeps their warnings out of ours.
```
```cmake
target_include_directories(render_settings SYSTEM INTERFACE "${STREAMLINE_DIR}/include")
```
```cmake
target_include_directories(render_settings SYSTEM INTERFACE "${STREAMLINE_DIR}/include" "${SKYCRAFT_DIR}")
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `112 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add extern/skycraft spikes/block-render \
  && git commit -q -m "minecraft-link: SkyCraft's protocol header" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 2: The rings

SkyCraft's seqlocked state blocks and its rings, on raw memory, with the memory orders of SkyCraft's Skyrim side (the
spec's "The protocol"): `SkyState` written, `McState` read, the render ring drained, the collision ring written, the
input ring pushed, the event ring drained.

**Files:**
- Create: `src/link/Rings.h`, `src/link/Rings.cpp`, `tests/TestRings.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestRings.cpp`**

```cpp
#include "Harness.h"

#include "link/Rings.h"

#include <cstring>
#include <vector>

using namespace cpmc::link;

namespace
{
// 8-byte aligned memory, as the mapping is: a ring's header (0x80 bytes) and aDataBytes of data.
struct Memory
{
	explicit Memory(size_t aBytes) : words((aBytes + 7) / 8) {}
	uint8_t*              Bytes() { return reinterpret_cast<uint8_t*>(words.data()); }
	uint64_t&             Head() { return *reinterpret_cast<uint64_t*>(Bytes()); }
	uint64_t&             Tail() { return *reinterpret_cast<uint64_t*>(Bytes() + 0x40); }
	std::vector<uint64_t> words;
};

std::vector<uint8_t> Payload(size_t aBytes, uint8_t aFirst)
{
	std::vector<uint8_t> bytes(aBytes);
	for (size_t i = 0; i < aBytes; ++i)
	{
		bytes[i] = uint8_t(aFirst + i);
	}
	return bytes;
}

struct Seen
{
	uint32_t             type;
	std::vector<uint8_t> payload;
};

DrainResult Drain(Memory& aRing, uint64_t aDataBytes, std::vector<Seen>& aSeen, uint64_t aBudget = ~0ull, uint32_t aSkipType = 99)
{
	return DrainByteRing(aRing.Bytes(), aDataBytes, aBudget, [&](const RingMessage& aMessage) {
		aSeen.push_back({ aMessage.type, { aMessage.payload.begin(), aMessage.payload.end() } });
		return aMessage.type != aSkipType;
	});
}
}

TEST(RingsSkyStateCountsUpByTwo)
{
	Memory           memory(0x1000);
	proto::SkyState state{};
	state.flags = proto::kSkyInGame;
	state.posX = 1.5;
	state.teleportSeq = 7;
	WriteSkyState(memory.Bytes(), state);
	state.posY = -2.25;
	WriteSkyState(memory.Bytes(), state);
	proto::SkyState written;
	std::memcpy(&written, memory.Bytes() + proto::kOffSkyState, sizeof written);
	CHECK_EQ(written.seq, 4u);
	CHECK_EQ(written.flags, uint32_t(proto::kSkyInGame));
	CHECK_EQ(written.posX, 1.5);
	CHECK_EQ(written.posY, -2.25);
	CHECK_EQ(written.teleportSeq, 7u);
}

TEST(RingsMcStateReadsWholeStates)
{
	Memory         memory(0x1000);
	proto::McState state{};
	state.seq = 6;
	state.flags = proto::kMcInWorld | proto::kMcScreenOpen;
	state.x = -119.5;
	state.frameCounter = 42;
	std::memcpy(memory.Bytes() + proto::kOffMcState, &state, sizeof state);
	const auto read = ReadMcState(memory.Bytes());
	CHECK(read.has_value());
	CHECK_EQ(read->flags, uint32_t(proto::kMcInWorld | proto::kMcScreenOpen));
	CHECK_EQ(read->x, -119.5);
	CHECK_EQ(read->frameCounter, 42ull);
	const uint32_t writing = 7;  // odd: Minecraft is mid-write
	std::memcpy(memory.Bytes() + proto::kOffMcState, &writing, sizeof writing);
	CHECK(!ReadMcState(memory.Bytes()).has_value());
}

TEST(RingsByteRingRoundTrip)
{
	Memory     ring(0x80 + 128);
	const auto five = Payload(5, 10);
	CHECK(WriteByteRing(ring.Bytes(), 128, 2, five));
	CHECK(WriteByteRing(ring.Bytes(), 128, 3, {}));
	CHECK_EQ(ring.Head(), 24ull);  // 8 + 5 rounded up to 16, then a bare header
	std::vector<Seen> seen;
	const auto        result = Drain(ring, 128, seen);
	CHECK_EQ(seen.size(), size_t(2));
	CHECK_EQ(seen[0].type, 2u);
	CHECK(seen[0].payload == five);
	CHECK_EQ(seen[1].type, 3u);
	CHECK(seen[1].payload.empty());
	CHECK_EQ(result.messages, 2u);
	CHECK_EQ(result.bytes, 24ull);
	CHECK(!result.resynced);
	CHECK_EQ(ring.Tail(), 24ull);
}

TEST(RingsByteRingPadsAtTheEnd)
{
	Memory ring(0x80 + 64);
	for (int i = 0; i < 3; ++i)
	{
		CHECK(WriteByteRing(ring.Bytes(), 64, 2, Payload(8, 0)));  // 16 bytes each
	}
	std::vector<Seen> seen;
	Drain(ring, 64, seen);
	CHECK_EQ(ring.Tail(), 48ull);
	CHECK(WriteByteRing(ring.Bytes(), 64, 4, Payload(16, 1)));  // 24 bytes: no room before the end, so a pad first
	CHECK_EQ(ring.Head(), 88ull);
	seen.clear();
	const auto result = Drain(ring, 64, seen);
	CHECK_EQ(seen.size(), size_t(1));
	CHECK_EQ(seen[0].type, 4u);
	CHECK(seen[0].payload == Payload(16, 1));
	CHECK_EQ(result.bytes, 40ull);  // the 16-byte pad, then the message
	CHECK_EQ(ring.Tail(), 88ull);
}

TEST(RingsByteRingRefusesWhatDoesntFit)
{
	Memory ring(0x80 + 64);
	CHECK(!WriteByteRing(ring.Bytes(), 64, 2, Payload(32, 0)));  // 40 bytes: over half the ring
	for (int i = 0; i < 4; ++i)
	{
		CHECK(WriteByteRing(ring.Bytes(), 64, 2, Payload(8, 0)));
	}
	CHECK(!WriteByteRing(ring.Bytes(), 64, 2, Payload(8, 0)));  // full
	CHECK_EQ(ring.Head(), 64ull);
}

TEST(RingsDrainStopsAtTheBudget)
{
	Memory ring(0x80 + 128);
	for (int i = 0; i < 3; ++i)
	{
		CHECK(WriteByteRing(ring.Bytes(), 128, 2, Payload(8, uint8_t(i))));
	}
	std::vector<Seen> seen;
	auto              result = Drain(ring, 128, seen, 20);
	CHECK_EQ(result.messages, 2u);  // 16 is under 20, so a second is taken; 32 isn't
	CHECK_EQ(result.takenBytes, 32ull);
	result = Drain(ring, 128, seen, 20);
	CHECK_EQ(result.messages, 1u);
	CHECK_EQ(seen.size(), size_t(3));
	CHECK_EQ(int(seen[2].payload[0]), 2);
}

TEST(RingsSkippedMessagesDontCountTowardTheBudget)
{
	Memory ring(0x80 + 128);
	for (int i = 0; i < 4; ++i)
	{
		CHECK(WriteByteRing(ring.Bytes(), 128, 7, Payload(8, 0)));
	}
	CHECK(WriteByteRing(ring.Bytes(), 128, 2, Payload(8, 0)));
	std::vector<Seen> seen;
	const auto        result = Drain(ring, 128, seen, 16, 7);
	CHECK_EQ(result.messages, 5u);
	CHECK_EQ(result.takenBytes, 16ull);
}

TEST(RingsDrainResyncsOnABadMessage)
{
	Memory                    ring(0x80 + 64);
	const proto::ColMsgHeader liar{ 2, 1000 };
	std::memcpy(ring.Bytes() + 0x80, &liar, sizeof liar);
	ring.Head() = 16;
	std::vector<Seen> seen;
	const auto        result = Drain(ring, 64, seen);
	CHECK(result.resynced);
	CHECK(seen.empty());
	CHECK_EQ(ring.Tail(), 16ull);
}

TEST(RingsDrainResyncsWhenTheTailIsAhead)
{
	Memory ring(0x80 + 64);
	ring.Head() = 8;
	ring.Tail() = 16;
	std::vector<Seen> seen;
	const auto        result = Drain(ring, 64, seen);
	CHECK(result.resynced);
	CHECK_EQ(ring.Tail(), 8ull);
}

TEST(RingsInputRingFillsUp)
{
	Memory                  ring(0x80 + proto::kInputRingEntries * sizeof(proto::InputEvent));
	const proto::InputEvent key{ proto::kInKey, 23, 1, 0, 0 };
	for (uint32_t i = 0; i < proto::kInputRingEntries; ++i)
	{
		CHECK(PushInput(ring.Bytes(), key));
	}
	CHECK(!PushInput(ring.Bytes(), key));
	ring.Tail() = 1;
	const proto::InputEvent up{ proto::kInKey, 23, 0, 0, 0 };
	CHECK(PushInput(ring.Bytes(), up));
	proto::InputEvent stored;
	std::memcpy(&stored, ring.Bytes() + 0x80, sizeof stored);  // entry 4096 wraps to slot 0
	CHECK_EQ(int(stored.a), 0);
	CHECK_EQ(ring.Head(), uint64_t(proto::kInputRingEntries) + 1);
}

TEST(RingsEventsDrainOldestFirst)
{
	Memory ring(0x80 + proto::kEventRingEntries * sizeof(proto::McEvent));
	for (uint32_t i = 0; i < 3; ++i)
	{
		proto::McEvent event{};
		event.type = proto::kEvExplosion;
		event.a = float(i);
		std::memcpy(ring.Bytes() + 0x80 + i * sizeof event, &event, sizeof event);
	}
	ring.Head() = 3;
	std::vector<float> seen;
	CHECK_EQ(DrainEvents(ring.Bytes(), [&](const proto::McEvent& aEvent) { seen.push_back(aEvent.a); }), 3u);
	CHECK(seen == std::vector<float>({ 0.0f, 1.0f, 2.0f }));
	CHECK_EQ(ring.Tail(), 3ull);
	ring.Head() = 3 + 600;  // lapped: only the newest 512 are still there
	CHECK_EQ(DrainEvents(ring.Bytes(), [](const proto::McEvent&) {}), proto::kEventRingEntries);
	CHECK_EQ(ring.Tail(), 603ull);
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestGbuffer.cpp
)
```
```cmake
	tests/TestGbuffer.cpp
	tests/TestRings.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Rings.h'`.

- [x] **Step 4: Write `src/link/Rings.h`**

```cpp
#pragma once

#include <skycraft_protocol.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>

// SkyCraft's seqlocked blocks and rings, read and written on raw memory: the mapping in the game and in mclink, plain
// buffers in the tests. The memory orders are those of SkyCraft's Skyrim side (skse/src/Link.cpp, MIT), which its
// Java side matches. Every ring has its head at +0x00, its tail at +0x40 and its data at +0x80.
namespace cpmc::link
{
namespace proto = skycraft::proto;

// The game side's state, at aBase + kOffSkyState. The sequence goes odd while writing, and up by 2 each write.
void WriteSkyState(uint8_t* aBase, const proto::SkyState& aState);

// Minecraft's state, at aBase + kOffMcState. Nothing if it was mid-write all 64 times we looked.
std::optional<proto::McState> ReadMcState(const uint8_t* aBase);

// One byte-ring message: its type, and its payload, which points into the ring.
struct RingMessage
{
	uint32_t                 type = 0;
	std::span<const uint8_t> payload;
};

struct DrainResult
{
	uint32_t    messages = 0;    // handed to the visitor
	uint64_t    bytes = 0;       // ring bytes consumed, pads included
	uint64_t    takenBytes = 0;  // ring bytes of the messages the visitor took
	bool        resynced = false;
	std::string why;  // why the tail jumped to the head
};

// Consumes a byte ring (SkyCraft's render ring) from its tail towards its head, handing each message to aVisit, which
// returns whether it took it. Stops once taken messages reach aBudget bytes, checked between messages. A head and tail
// that make no sense, or a message that doesn't fit, move the tail to the head. Nothing outside the data is read.
DrainResult DrainByteRing(uint8_t* aRing, uint64_t aDataBytes, uint64_t aBudget, const std::function<bool(const RingMessage&)>& aVisit);

// Appends one message to a byte ring (SkyCraft's collision ring), with a pad to the start when it wouldn't fit before
// the end. False, writing nothing, if there's no room or the message is over half the ring.
bool WriteByteRing(uint8_t* aRing, uint64_t aDataBytes, uint32_t aType, std::span<const uint8_t> aPayload);

// The input ring (game to Minecraft). False, writing nothing, if Minecraft is a whole ring behind.
bool PushInput(uint8_t* aRing, const proto::InputEvent& aEvent);

// The event ring (Minecraft to game): every waiting event, oldest first. If Minecraft lapped us, the oldest are gone.
uint32_t DrainEvents(uint8_t* aRing, const std::function<void(const proto::McEvent&)>& aVisit);
}
```

- [x] **Step 5: Write `src/link/Rings.cpp`**

```cpp
#include "link/Rings.h"

#include <atomic>
#include <cstring>
#include <format>
#include <intrin.h>

namespace cpmc::link
{
namespace
{
constexpr uint64_t kHead = 0x00;
constexpr uint64_t kTail = 0x40;
constexpr uint64_t kData = 0x80;
static_assert(proto::kRenRingHeadOff == kHead && proto::kRenRingTailOff == kTail && proto::kRenRingDataOff == kData);
static_assert(proto::kColRingHeadOff == kHead && proto::kColRingTailOff == kTail && proto::kColRingDataOff == kData);
static_assert(proto::kInputRingHeadOff == kHead && proto::kInputRingTailOff == kTail && proto::kInputRingDataOff == kData);
static_assert(proto::kEventRingHeadOff == kHead && proto::kEventRingTailOff == kTail && proto::kEventRingDataOff == kData);
static_assert(proto::kRenPad == 0 && proto::kColPad == 0);

template<class T>
std::atomic_ref<T> At(uint8_t* aBase, uint64_t aOffset)
{
	return std::atomic_ref<T>(*reinterpret_cast<T*>(aBase + aOffset));
}

// A whole message, header and padding included: messages start 8-byte aligned.
uint64_t MessageBytes(uint64_t aPayloadBytes)
{
	return (sizeof(proto::ColMsgHeader) + aPayloadBytes + 7) & ~7ull;
}
}

void WriteSkyState(uint8_t* aBase, const proto::SkyState& aState)
{
	auto           seq = At<uint32_t>(aBase, proto::kOffSkyState);
	const uint32_t count = seq.load(std::memory_order_relaxed);
	seq.store(count + 1, std::memory_order_relaxed);
	std::atomic_thread_fence(std::memory_order_release);
	std::memcpy(aBase + proto::kOffSkyState + 4, reinterpret_cast<const uint8_t*>(&aState) + 4, sizeof(proto::SkyState) - 4);
	seq.store(count + 2, std::memory_order_release);
}

std::optional<proto::McState> ReadMcState(const uint8_t* aBase)
{
	uint8_t* const source = const_cast<uint8_t*>(aBase) + proto::kOffMcState;  // only loaded from
	auto           seq = std::atomic_ref<uint32_t>(*reinterpret_cast<uint32_t*>(source));
	for (int attempt = 0; attempt < 64; ++attempt)
	{
		const uint32_t before = seq.load(std::memory_order_acquire);
		if (before & 1)
		{
			_mm_pause();
			continue;
		}
		proto::McState state;
		std::memcpy(&state, source, sizeof state);
		std::atomic_thread_fence(std::memory_order_acquire);
		if (seq.load(std::memory_order_relaxed) == before)
		{
			return state;
		}
	}
	return std::nullopt;
}

DrainResult DrainByteRing(uint8_t* aRing, uint64_t aDataBytes, uint64_t aBudget, const std::function<bool(const RingMessage&)>& aVisit)
{
	DrainResult    result;
	auto           headRef = At<uint64_t>(aRing, kHead);
	auto           tailRef = At<uint64_t>(aRing, kTail);
	const uint8_t* data = aRing + kData;
	const uint64_t head = headRef.load(std::memory_order_acquire);
	uint64_t       tail = tailRef.load(std::memory_order_relaxed);
	const auto     resync = [&](std::string aWhy) {
        result.resynced = true;
        result.why = std::move(aWhy);
        tail = head;
	};
	if (head < tail || head - tail > aDataBytes || tail % 8 != 0)
	{
		resync(std::format("head {} and tail {} don't fit a ring of {} bytes", head, tail, aDataBytes));
	}
	while (tail < head && result.takenBytes < aBudget)
	{
		const uint64_t position = tail % aDataBytes;
		const uint64_t waiting = head - tail;
		if (waiting < sizeof(proto::ColMsgHeader) || aDataBytes - position < sizeof(proto::ColMsgHeader))
		{
			resync(std::format("no room for a message header at {}", tail));
			break;
		}
		proto::ColMsgHeader header;
		std::memcpy(&header, data + position, sizeof header);
		if (header.type == proto::kRenPad)
		{
			const uint64_t skip = aDataBytes - position;
			if (skip > waiting)
			{
				resync(std::format("a pad at {} goes past the head", tail));
				break;
			}
			tail += skip;
			result.bytes += skip;
			continue;
		}
		const uint64_t bytes = MessageBytes(header.payloadBytes);
		if (bytes > aDataBytes - position || bytes > waiting)
		{
			resync(std::format("a message of type {} at {} claims {} payload bytes", header.type, tail, header.payloadBytes));
			break;
		}
		const bool taken = aVisit({ header.type, { data + position + sizeof header, header.payloadBytes } });
		++result.messages;
		tail += bytes;
		result.bytes += bytes;
		if (taken)
		{
			result.takenBytes += bytes;
		}
	}
	tailRef.store(tail, std::memory_order_release);
	return result;
}

bool WriteByteRing(uint8_t* aRing, uint64_t aDataBytes, uint32_t aType, std::span<const uint8_t> aPayload)
{
	const uint64_t bytes = MessageBytes(aPayload.size());
	if (bytes > aDataBytes / 2)
	{
		return false;
	}
	auto           headRef = At<uint64_t>(aRing, kHead);
	auto           tailRef = At<uint64_t>(aRing, kTail);
	uint8_t*       data = aRing + kData;
	uint64_t       head = headRef.load(std::memory_order_relaxed);
	const uint64_t tail = tailRef.load(std::memory_order_acquire);
	uint64_t       position = head % aDataBytes;
	const uint64_t pad = position + bytes > aDataBytes ? aDataBytes - position : 0;
	if (head - tail > aDataBytes || aDataBytes - (head - tail) < bytes + pad)
	{
		return false;
	}
	if (pad)
	{
		const proto::ColMsgHeader padHeader{ proto::kColPad, 0 };
		std::memcpy(data + position, &padHeader, sizeof padHeader);
		head += pad;
		position = 0;
	}
	const proto::ColMsgHeader header{ aType, uint32_t(aPayload.size()) };
	std::memcpy(data + position, &header, sizeof header);
	if (!aPayload.empty())
	{
		std::memcpy(data + position + sizeof header, aPayload.data(), aPayload.size());
	}
	headRef.store(head + bytes, std::memory_order_release);
	return true;
}

bool PushInput(uint8_t* aRing, const proto::InputEvent& aEvent)
{
	auto           headRef = At<uint64_t>(aRing, kHead);
	auto           tailRef = At<uint64_t>(aRing, kTail);
	const uint64_t head = headRef.load(std::memory_order_relaxed);
	const uint64_t tail = tailRef.load(std::memory_order_acquire);
	if (head - tail >= proto::kInputRingEntries)
	{
		return false;
	}
	std::memcpy(aRing + kData + (head & (proto::kInputRingEntries - 1)) * sizeof(proto::InputEvent), &aEvent, sizeof aEvent);
	headRef.store(head + 1, std::memory_order_release);
	return true;
}

uint32_t DrainEvents(uint8_t* aRing, const std::function<void(const proto::McEvent&)>& aVisit)
{
	auto           headRef = At<uint64_t>(aRing, kHead);
	auto           tailRef = At<uint64_t>(aRing, kTail);
	const uint64_t head = headRef.load(std::memory_order_acquire);
	uint64_t       tail = tailRef.load(std::memory_order_relaxed);
	if (tail > head)
	{
		tail = head;
	}
	if (head - tail > proto::kEventRingEntries)
	{
		tail = head - proto::kEventRingEntries;
	}
	uint32_t count = 0;
	for (; tail < head; ++tail, ++count)
	{
		proto::McEvent event;
		std::memcpy(&event, aRing + kData + (tail & (proto::kEventRingEntries - 1)) * sizeof(proto::McEvent), sizeof event);
		aVisit(event);
	}
	tailRef.store(tail, std::memory_order_release);
	return count;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/render/Gbuffer.cpp
)
```
```cmake
	src/render/Gbuffer.cpp
	src/link/Rings.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `123 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: SkyCraft's seqlocks and rings" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 3: Render messages

Sections and the atlas, as SkyCraft's `WorldExporter` writes them, checked against their layouts (the spec's "Blocks"
and "Safety"). Translucent triangles are dropped unless asked for.

**Files:**
- Create: `src/link/Messages.h`, `src/link/Messages.cpp`, `tests/TestMessages.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestMessages.cpp`**

```cpp
#include "Harness.h"

#include "link/Messages.h"

#include <cstring>

using namespace cpmc;
using namespace cpmc::link;

namespace
{
// Little-endian bytes, as SkyCraft's WorldExporter writes them with a little-endian ByteBuffer.
struct Bytes
{
	template<class T>
	Bytes& Put(T aValue)
	{
		uint8_t raw[sizeof(T)];
		std::memcpy(raw, &aValue, sizeof raw);
		data.insert(data.end(), raw, raw + sizeof raw);
		return *this;
	}
	std::vector<uint8_t> data;
};

// MeshBuilder.vertex: x, y, z, u, v as floats, then r, g, b, a, then the light and the flags.
void PutVertex(Bytes& aBytes, float aX, uint32_t aFlags)
{
	aBytes.Put(aX).Put(2.0f).Put(3.0f).Put(0.25f).Put(0.5f);
	aBytes.Put(uint8_t(200)).Put(uint8_t(150)).Put(uint8_t(100)).Put(uint8_t(255));
	aBytes.Put(uint32_t(15 << 8)).Put(aFlags);
}

std::vector<uint8_t> Section(int32_t aX, int32_t aY, int32_t aZ, uint32_t aCount, const std::vector<uint32_t>& aFlags)
{
	Bytes bytes;
	bytes.Put(aX).Put(aY).Put(aZ).Put(aCount);
	for (size_t i = 0; i < aFlags.size(); ++i)
	{
		PutVertex(bytes, float(i), aFlags[i]);
	}
	return bytes.data;
}

constexpr uint32_t kOpaqueUp = 1 | (2 << 4);    // cut-out, facing up (Direction.UP + 1)
constexpr uint32_t kWater = 2 | (2 << 4);       // translucent
}

TEST(MessagesSectionAsWorldExporterWritesIt)
{
	const auto  payload = Section(-8, 0, 125, 6, std::vector<uint32_t>(6, kOpaqueUp));
	std::string error;
	const auto  mesh = ParseSection(payload, false, error);
	CHECK(mesh.has_value());
	CHECK_EQ(mesh->x, -8);
	CHECK_EQ(mesh->y, 0);
	CHECK_EQ(mesh->z, 125);
	CHECK_EQ(mesh->vertices.size(), size_t(6));
	const Vertex& v = mesh->vertices[1];
	CHECK_EQ(v.x, 1.0f);
	CHECK_EQ(v.y, 2.0f);
	CHECK_EQ(v.u, 0.25f);
	CHECK_EQ(v.v, 0.5f);
	CHECK_EQ(int(v.r), 200);
	CHECK_EQ(int(v.a), 255);
	CHECK_EQ(v.light, uint32_t(15 << 8));
	CHECK_EQ(v.flags, kOpaqueUp);
	CHECK_EQ(mesh->translucentDropped, 0u);
}

TEST(MessagesSectionKeepsWholeTriangles)
{
	const auto  payload = Section(0, 0, 0, 7, std::vector<uint32_t>(7, kOpaqueUp));
	std::string error;
	const auto  mesh = ParseSection(payload, false, error);
	CHECK(mesh.has_value());
	CHECK_EQ(mesh->vertices.size(), size_t(6));
}

TEST(MessagesTranslucentTrianglesAreDropped)
{
	const auto  payload = Section(0, 0, 0, 6, { kOpaqueUp, kOpaqueUp, kOpaqueUp, kWater, kWater, kWater });
	std::string error;
	const auto  dropped = ParseSection(payload, false, error);
	CHECK(dropped.has_value());
	CHECK_EQ(dropped->vertices.size(), size_t(3));
	CHECK_EQ(dropped->translucentDropped, 1u);
	const auto kept = ParseSection(payload, true, error);
	CHECK_EQ(kept->vertices.size(), size_t(6));
	CHECK_EQ(kept->translucentDropped, 0u);
}

TEST(MessagesEmptySectionIsARemoval)
{
	const auto  payload = Section(3, -4, 5, 0, {});
	std::string error;
	const auto  mesh = ParseSection(payload, false, error);
	CHECK(mesh.has_value());
	CHECK(mesh->vertices.empty());
}

TEST(MessagesSectionRejections)
{
	std::string error;
	CHECK(!ParseSection(std::vector<uint8_t>(15), false, error).has_value());
	CHECK(!ParseSection(Section(0, 0, 0, 10, std::vector<uint32_t>(9, kOpaqueUp)), false, error).has_value());
	CHECK(error.find("claims 10 vertices") != std::string::npos);
	CHECK(!ParseSection(Section(3'000'000, 0, 0, 0, {}), false, error).has_value());
	CHECK(!ParseSection(Section(0, 300, 0, 0, {}), false, error).has_value());
	CHECK(error.find("out of range") != std::string::npos);
}

TEST(MessagesAtlas)
{
	Bytes bytes;
	bytes.Put(uint32_t(2)).Put(uint32_t(1));
	for (uint8_t i = 0; i < 8; ++i)
	{
		bytes.Put(uint8_t(i + 1));
	}
	std::string error;
	const auto  atlas = ParseAtlas(bytes.data, error);
	CHECK(atlas.has_value());
	CHECK_EQ(atlas->width, 2u);
	CHECK_EQ(atlas->height, 1u);
	CHECK_EQ(atlas->rgba.size(), size_t(8));
	CHECK_EQ(int(atlas->rgba[0]), 1);
	CHECK_EQ(int(atlas->rgba[7]), 8);
}

TEST(MessagesAtlasRejections)
{
	std::string error;
	Bytes       zero;
	zero.Put(uint32_t(0)).Put(uint32_t(4));
	CHECK(!ParseAtlas(zero.data, error).has_value());
	Bytes huge;
	huge.Put(uint32_t(20000)).Put(uint32_t(1));
	CHECK(!ParseAtlas(huge.data, error).has_value());
	Bytes shortOne;
	shortOne.Put(uint32_t(2)).Put(uint32_t(2)).Put(uint32_t(0));
	CHECK(!ParseAtlas(shortOne.data, error).has_value());
	CHECK(error.find("needs 16 bytes") != std::string::npos);
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestRings.cpp
)
```
```cmake
	tests/TestRings.cpp
	tests/TestMessages.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Messages.h'`.

- [x] **Step 4: Write `src/link/Messages.h`**

```cpp
#pragma once

#include "world/Vertex.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// The render ring's messages this spike takes (the spec's "Blocks"), checked against their layouts before use: shared
// memory is written by another process, so nothing in it is trusted.
namespace cpmc::link
{
constexpr int32_t  kMaxSectionXZ = 2'000'000;  // the world border is 30 million blocks, 1.9 million sections, out
constexpr int32_t  kMaxSectionY = 256;
constexpr uint32_t kMaxAtlasSide = 16384;

// A section's mesh (kRenSection): whole triangles in SkyCraft's vertex, which is BlockRender's Vertex.
struct SectionMesh
{
	int32_t             x = 0;  // Minecraft section coordinates
	int32_t             y = 0;
	int32_t             z = 0;
	std::vector<Vertex> vertices;  // none: the section is gone
	uint32_t            translucentDropped = 0;  // triangles left out
};

// A kRenSection payload: {i32 x, y, z; u32 vertices}, then the vertices. A count that isn't a multiple of 3 is cut down
// to one. Translucent triangles (flag bit 1) are left out unless aKeepTranslucent. Nothing, with aError saying why, if
// the payload is too short for its count or the coordinates are out of range.
std::optional<SectionMesh> ParseSection(std::span<const uint8_t> aPayload, bool aKeepTranslucent, std::string& aError);

// Minecraft's combined atlas (kRenAtlas): RGBA8, top row first.
struct AtlasImage
{
	uint32_t                 width = 0;
	uint32_t                 height = 0;
	std::span<const uint8_t> rgba;  // width * height * 4 bytes, inside the payload
};

// A kRenAtlas payload: {u32 width, height}, then the pixels. Nothing, with aError saying why, if a side is 0 or over
// 16,384, or the payload is too short.
std::optional<AtlasImage> ParseAtlas(std::span<const uint8_t> aPayload, std::string& aError);
}
```

- [x] **Step 5: Write `src/link/Messages.cpp`**

```cpp
#include "link/Messages.h"

#include <skycraft_protocol.h>

#include <cstdlib>
#include <cstring>
#include <format>

namespace cpmc::link
{
namespace proto = skycraft::proto;
static_assert(sizeof(proto::RenVertex) == sizeof(Vertex));
static_assert(sizeof(proto::RenSection) == 16 && sizeof(proto::RenAtlas) == 8);

std::optional<SectionMesh> ParseSection(std::span<const uint8_t> aPayload, bool aKeepTranslucent, std::string& aError)
{
	proto::RenSection header;
	if (aPayload.size() < sizeof header)
	{
		aError = std::format("a section message of {} bytes has no room for its header", aPayload.size());
		return std::nullopt;
	}
	std::memcpy(&header, aPayload.data(), sizeof header);
	const uint64_t room = (aPayload.size() - sizeof header) / sizeof(Vertex);
	if (header.vertexCount > room)
	{
		aError = std::format("section {} {} {} claims {} vertices, with room for {}", header.sx, header.sy, header.sz,
		                     header.vertexCount, room);
		return std::nullopt;
	}
	if (std::abs(int64_t(header.sx)) > kMaxSectionXZ || std::abs(int64_t(header.sz)) > kMaxSectionXZ ||
	    std::abs(int64_t(header.sy)) > kMaxSectionY)
	{
		aError = std::format("section {} {} {} is out of range", header.sx, header.sy, header.sz);
		return std::nullopt;
	}
	SectionMesh mesh{ header.sx, header.sy, header.sz };
	const uint32_t triangles = header.vertexCount / 3;
	mesh.vertices.reserve(size_t(triangles) * 3);
	const uint8_t* source = aPayload.data() + sizeof header;
	for (uint32_t t = 0; t < triangles; ++t)
	{
		Vertex corners[3];
		std::memcpy(corners, source + size_t(t) * sizeof corners, sizeof corners);
		if ((corners[0].flags & kFlagTranslucent) && !aKeepTranslucent)
		{
			++mesh.translucentDropped;
			continue;
		}
		mesh.vertices.insert(mesh.vertices.end(), corners, corners + 3);
	}
	return mesh;
}

std::optional<AtlasImage> ParseAtlas(std::span<const uint8_t> aPayload, std::string& aError)
{
	proto::RenAtlas header;
	if (aPayload.size() < sizeof header)
	{
		aError = std::format("an atlas message of {} bytes has no room for its header", aPayload.size());
		return std::nullopt;
	}
	std::memcpy(&header, aPayload.data(), sizeof header);
	if (header.width == 0 || header.height == 0 || header.width > kMaxAtlasSide || header.height > kMaxAtlasSide)
	{
		aError = std::format("an atlas of {} x {} is out of range", header.width, header.height);
		return std::nullopt;
	}
	const uint64_t bytes = uint64_t(header.width) * header.height * 4;
	if (aPayload.size() - sizeof header < bytes)
	{
		aError = std::format("a {} x {} atlas needs {} bytes, and has {}", header.width, header.height, bytes, aPayload.size() - sizeof header);
		return std::nullopt;
	}
	return AtlasImage{ header.width, header.height, aPayload.subspan(sizeof header, size_t(bytes)) };
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Rings.cpp
)
```
```cmake
	src/link/Rings.cpp
	src/link/Messages.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `130 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: render messages, checked" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 4: The overlay reader

The reader's side of the triple buffer where Minecraft publishes its HUD image (the spec's "The protocol"), with
SkyCraft's swap and reset rules. The tests play Minecraft's writer as SkyCraft's `SkyLink.publishOverlay` does.

**Files:**
- Create: `src/link/Overlay.h`, `src/link/Overlay.cpp`, `tests/TestOverlay.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestOverlay.cpp`**

```cpp
#include "Harness.h"

#include "link/Overlay.h"

#include <skycraft_protocol.h>

#include <cstring>
#include <vector>

using namespace cpmc::link;
namespace proto = skycraft::proto;

namespace
{
constexpr uint64_t kSlotBytes = 64;  // 4 x 4 pixels, small for the tests

// The overlay's control word, three slot headers and three small slots of pixels, with Minecraft's writer as
// SkyCraft's SkyLink.publishOverlay has it: its back slot starts at 1.
struct Triple
{
	Triple() : words(512) {}
	uint8_t*  Control() { return reinterpret_cast<uint8_t*>(words.data()); }
	uint8_t*  Headers() { return Control() + 64; }
	uint8_t*  Pixels() { return Control() + 64 + 3 * sizeof(proto::OverlaySlotHdr); }
	uint32_t& State() { return *reinterpret_cast<uint32_t*>(Control()); }

	void Publish(uint32_t aWidth, uint32_t aHeight, uint64_t aFrameId, uint8_t aFill)
	{
		proto::OverlaySlotHdr header{};
		header.width = aWidth;
		header.height = aHeight;
		header.flags = 1;  // bottom-up, as Minecraft's readback is
		header.frameId = aFrameId;
		std::memcpy(Headers() + back * sizeof header, &header, sizeof header);
		std::memset(Pixels() + back * kSlotBytes, aFill, kSlotBytes);
		const uint32_t old = State();
		State() = back | proto::kOverlayDirty;
		back = old & 3;
	}

	std::vector<uint64_t> words;
	uint32_t              back = 1;
};
}

TEST(OverlayNothingUntilPublished)
{
	Triple        triple;
	OverlayReader reader(triple.Control(), triple.Headers(), triple.Pixels(), kSlotBytes);
	CHECK(!reader.Acquire().has_value());
}

TEST(OverlayTakesTheNewestFrame)
{
	Triple        triple;
	OverlayReader reader(triple.Control(), triple.Headers(), triple.Pixels(), kSlotBytes);
	triple.Publish(4, 4, 5, 0xAA);
	const auto first = reader.Acquire();
	CHECK(first.has_value());
	CHECK_EQ(first->width, 4u);
	CHECK_EQ(first->height, 4u);
	CHECK(first->bottomUp);
	CHECK_EQ(first->frameId, 5ull);
	CHECK(first->pixels == triple.Pixels() + 1 * kSlotBytes);  // Minecraft's first back slot
	CHECK_EQ(int(first->pixels[0]), 0xAA);
	CHECK_EQ(triple.State(), 2u);  // our old front went into the middle, unread bit clear
	CHECK(!reader.Acquire().has_value());
	triple.Publish(2, 2, 6, 0xBB);  // into slot 0, the back slot Minecraft got from its first swap
	triple.Publish(4, 2, 7, 0xCC);  // and again, into slot 2: only the newest is read
	const auto newest = reader.Acquire();
	CHECK(newest.has_value());
	CHECK_EQ(newest->frameId, 7ull);
	CHECK(newest->pixels == triple.Pixels() + 2 * kSlotBytes);
	CHECK_EQ(int(newest->pixels[0]), 0xCC);
}

TEST(OverlayResetStartsOver)
{
	Triple        triple;
	OverlayReader reader(triple.Control(), triple.Headers(), triple.Pixels(), kSlotBytes);
	triple.Publish(4, 4, 1, 1);
	reader.Acquire();
	reader.Reset();
	CHECK_EQ(triple.State(), 0u);
	triple.back = 1;  // Minecraft's writer starts over too
	triple.Publish(4, 4, 2, 2);
	const auto frame = reader.Acquire();
	CHECK(frame.has_value());
	CHECK(frame->pixels == triple.Pixels() + 1 * kSlotBytes);
	CHECK_EQ(triple.State(), 2u);
}

TEST(OverlayRefusesBadSlotsAndSizes)
{
	Triple        triple;
	OverlayReader reader(triple.Control(), triple.Headers(), triple.Pixels(), kSlotBytes);
	triple.State() = 3 | proto::kOverlayDirty;
	CHECK(!reader.Acquire().has_value());
	CHECK_EQ(reader.Refused(), 1ull);
	CHECK_EQ(triple.State(), 0u);  // reset
	triple.back = 1;
	triple.Publish(5000, 4, 1, 0);
	CHECK(!reader.Acquire().has_value());
	triple.Publish(8, 8, 2, 0);  // 256 bytes: more than its slot
	CHECK(!reader.Acquire().has_value());
	CHECK_EQ(reader.Refused(), 3ull);
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestMessages.cpp
)
```
```cmake
	tests/TestMessages.cpp
	tests/TestOverlay.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Overlay.h'`.

- [x] **Step 4: Write `src/link/Overlay.h`**

```cpp
#pragma once

#include <cstdint>
#include <optional>

// The reader's side of SkyCraft's overlay triple buffer, where Minecraft publishes its HUD image (the spec's "The
// protocol"), as SkyCraft's Skyrim side reads it (skse/src/Link.cpp, MIT). Minecraft draws into its own back slot,
// then swaps it into the middle with the unread bit set. The reader swaps its front slot into the middle when the bit
// is set, and takes the slot it gets back, which Minecraft won't touch until it's swapped out again.
namespace cpmc::link
{
struct HudFrame
{
	uint32_t       width = 0;
	uint32_t       height = 0;
	bool           bottomUp = false;
	uint64_t       frameId = 0;
	const uint8_t* pixels = nullptr;  // width * height * 4 bytes, RGBA8 with premultiplied alpha; ours until the next Acquire
};

class OverlayReader
{
public:
	// aControl: the OverlayCtl; aHeaders: the 3 slot headers; aPixels: slot 0's pixels; aSlotBytes: bytes per slot.
	OverlayReader(uint8_t* aControl, uint8_t* aHeaders, uint8_t* aPixels, uint64_t aSlotBytes);

	// When Minecraft connects or comes back: the state to 0, and our front to slot 2. Minecraft's writer starts at 1.
	void Reset();

	// The newest image, if Minecraft has published one since the last call. A slot index of 3, or a size out of range
	// (1 to 3840 by 2160, within its slot), is refused and counted.
	std::optional<HudFrame> Acquire();

	uint64_t Refused() const { return refused_; }

private:
	uint8_t* control_;
	uint8_t* headers_;
	uint8_t* pixels_;
	uint64_t slotBytes_;
	uint32_t front_ = 2;
	uint64_t refused_ = 0;
};
}
```

- [x] **Step 5: Write `src/link/Overlay.cpp`**

```cpp
#include "link/Overlay.h"

#include <skycraft_protocol.h>

#include <atomic>
#include <cstring>

namespace cpmc::link
{
namespace proto = skycraft::proto;

OverlayReader::OverlayReader(uint8_t* aControl, uint8_t* aHeaders, uint8_t* aPixels, uint64_t aSlotBytes)
    : control_(aControl), headers_(aHeaders), pixels_(aPixels), slotBytes_(aSlotBytes)
{
}

void OverlayReader::Reset()
{
	std::atomic_ref<uint32_t>(*reinterpret_cast<uint32_t*>(control_)).store(0, std::memory_order_release);
	front_ = 2;
}

std::optional<HudFrame> OverlayReader::Acquire()
{
	auto state = std::atomic_ref<uint32_t>(*reinterpret_cast<uint32_t*>(control_));
	if (!(state.load(std::memory_order_acquire) & proto::kOverlayDirty))
	{
		return std::nullopt;
	}
	const uint32_t slot = state.exchange(front_, std::memory_order_acq_rel) & 3;
	if (slot > 2)
	{
		++refused_;
		Reset();
		return std::nullopt;
	}
	front_ = slot;
	proto::OverlaySlotHdr header;
	std::memcpy(&header, headers_ + slot * sizeof header, sizeof header);
	if (header.width == 0 || header.height == 0 || header.width > proto::kMaxOverlayW || header.height > proto::kMaxOverlayH ||
	    uint64_t(header.width) * header.height * 4 > slotBytes_)
	{
		++refused_;
		return std::nullopt;
	}
	return HudFrame{ header.width, header.height, (header.flags & 1) != 0, header.frameId, pixels_ + slot * slotBytes_ };
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Messages.cpp
)
```
```cmake
	src/link/Messages.cpp
	src/link/Overlay.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `134 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: the HUD image's triple buffer, read" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 5: The floor

The stand-in floor's collision messages (the spec's "The link"): a clear with the new epoch, then two flat triangles
for each of 7 by 7 regions. The first test checks one region byte for byte against SkyCraft's own
`tools/fake_skyrim.py`.

**Files:**
- Create: `src/link/Floor.h`, `src/link/Floor.cpp`, `tests/TestFloor.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestFloor.cpp`**

The expected bytes in `FloorMatchesFakeSkyrim` came from SkyCraft's `fake_skyrim.floor_tris_payload(12500, 12, -1, 1)`
at commit `bfcaf17`, run with Python.

```cpp
#include "Harness.h"

#include "link/Floor.h"

#include <skycraft_protocol.h>

#include <algorithm>
#include <cstring>
#include <string>

using namespace cpmc::link;
namespace proto = skycraft::proto;

namespace
{
using Region = std::pair<int32_t, int32_t>;

std::vector<uint8_t> FromHex(const std::string& aHex)
{
	std::vector<uint8_t> bytes;
	for (size_t i = 0; i + 1 < aHex.size(); i += 2)
	{
		bytes.push_back(uint8_t(std::stoi(aHex.substr(i, 2), nullptr, 16)));
	}
	return bytes;
}
}

// SkyCraft's tools/fake_skyrim.py at bfcaf17: floor_tris_payload(12500, 12, -1, 1), its floor being at y = 100.
TEST(FloorMatchesFakeSkyrim)
{
	const auto expected = FromHex(
	    "a086010060000000f8ffffffa786010067000000ffffffff01000000020000000050c3470000c842000000c10054c3470000c842000000c1"
	    "0054c3470000c84200000000000000000050c3470000c842000000c10054c3470000c842000000000050c3470000c8420000000000000000");
	CHECK_EQ(expected.size(), size_t(112));
	CHECK(FloorRegion(12500, -1, 100.0, 1) == expected);
}

TEST(FloorRegionRowHoldsTheHeight)
{
	const auto       bytes = FloorRegion(-15, 250, 6.14, 3);
	proto::ColRegion region;
	std::memcpy(&region, bytes.data(), sizeof region);
	CHECK_EQ(region.minX, -120);
	CHECK_EQ(region.maxX, -113);
	CHECK_EQ(region.minY, 0);
	CHECK_EQ(region.maxY, 7);
	CHECK_EQ(region.minZ, 2000);
	CHECK_EQ(region.epoch, 3u);
	CHECK_EQ(region.count, 2u);
	proto::ColTri triangle;
	std::memcpy(&triangle, bytes.data() + sizeof region, sizeof triangle);
	CHECK_EQ(triangle.v[1], 6.14f);
	CHECK_EQ(triangle.flags, 0u);  // not diggable: attacking the floor does nothing
	std::memcpy(&region, FloorRegion(0, 0, -0.5, 1).data(), sizeof region);
	CHECK_EQ(region.minY, -8);
}

TEST(FloorCoversSevenBySevenRegions)
{
	const auto regions = FloorRegions(-119.5, 2007.5);  // where V stood at spike 3's corner
	CHECK_EQ(regions.size(), size_t(49));
	CHECK(std::find(regions.begin(), regions.end(), Region(-15, 250)) != regions.end());
	CHECK(regions.front() == Region(-18, 247));
	CHECK(regions.back() == Region(-12, 253));
}

TEST(FloorClearHoldsTheEpoch)
{
	CHECK(FloorClear(0x01020304) == std::vector<uint8_t>({ 4, 3, 2, 1 }));
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestOverlay.cpp
)
```
```cmake
	tests/TestOverlay.cpp
	tests/TestFloor.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Floor.h'`.

- [x] **Step 4: Write `src/link/Floor.h`**

```cpp
#pragma once

#include <cstdint>
#include <utility>
#include <vector>

// The stand-in floor (the spec's "The link"): flat collision triangles at V's feet, which Minecraft's crosshair hits,
// so clicks have something to place blocks on. Triangles only: SkyCraft releases its held player only once voxel
// regions are known, so Minecraft's player stays pinned where it was teleported.
namespace cpmc::link
{
constexpr int32_t kRegionBlocks = 8;  // SkyCraft's collision regions are 8-block cubes
constexpr int32_t kFloorRadius = 3;   // regions each side of the centre's: 7 by 7, 56 blocks across

// The collision clear message's payload (kColClear): the new epoch.
std::vector<uint8_t> FloorClear(uint32_t aEpoch);

// One region's triangles message payload (kColTris): two triangles covering its 8 by 8 blocks at height aY, filed in
// the region row holding aY. The same bytes as SkyCraft's tools/fake_skyrim.py floor_tris_payload for its floor.
std::vector<uint8_t> FloorRegion(int32_t aRegionX, int32_t aRegionZ, double aY, uint32_t aEpoch);

// The regions of a floor centred on Minecraft block column aX, aZ: (region x, region z) pairs, 7 by 7.
std::vector<std::pair<int32_t, int32_t>> FloorRegions(double aX, double aZ);
}
```

- [x] **Step 5: Write `src/link/Floor.cpp`**

```cpp
#include "link/Floor.h"

#include <skycraft_protocol.h>

#include <cmath>
#include <cstring>

namespace cpmc::link
{
namespace proto = skycraft::proto;

namespace
{
template<class T>
void Append(std::vector<uint8_t>& aBytes, const T& aValue)
{
	const auto* raw = reinterpret_cast<const uint8_t*>(&aValue);
	aBytes.insert(aBytes.end(), raw, raw + sizeof aValue);
}

int32_t RegionOf(double aBlocks)
{
	return int32_t(std::floor(aBlocks / kRegionBlocks));
}
}

std::vector<uint8_t> FloorClear(uint32_t aEpoch)
{
	std::vector<uint8_t> bytes;
	Append(bytes, aEpoch);
	return bytes;
}

std::vector<uint8_t> FloorRegion(int32_t aRegionX, int32_t aRegionZ, double aY, uint32_t aEpoch)
{
	const int32_t     regionY = RegionOf(aY);
	proto::ColRegion region{ aRegionX * kRegionBlocks, regionY * kRegionBlocks, aRegionZ * kRegionBlocks,
		                     aRegionX * kRegionBlocks + kRegionBlocks - 1, regionY * kRegionBlocks + kRegionBlocks - 1,
		                     aRegionZ * kRegionBlocks + kRegionBlocks - 1, aEpoch, 2 };
	const float x0 = float(aRegionX * kRegionBlocks);
	const float x1 = x0 + kRegionBlocks;
	const float z0 = float(aRegionZ * kRegionBlocks);
	const float z1 = z0 + kRegionBlocks;
	const float y = float(aY);
	const proto::ColTri first{ { x0, y, z0, x1, y, z0, x1, y, z1 }, 0 };
	const proto::ColTri second{ { x0, y, z0, x1, y, z1, x0, y, z1 }, 0 };
	std::vector<uint8_t> bytes;
	Append(bytes, region);
	Append(bytes, first);
	Append(bytes, second);
	return bytes;
}

std::vector<std::pair<int32_t, int32_t>> FloorRegions(double aX, double aZ)
{
	const int32_t centreX = RegionOf(aX);
	const int32_t centreZ = RegionOf(aZ);
	std::vector<std::pair<int32_t, int32_t>> regions;
	for (int32_t x = centreX - kFloorRadius; x <= centreX + kFloorRadius; ++x)
	{
		for (int32_t z = centreZ - kFloorRadius; z <= centreZ + kFloorRadius; ++z)
		{
			regions.emplace_back(x, z);
		}
	}
	return regions;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Overlay.cpp
)
```
```cmake
	src/link/Overlay.cpp
	src/link/Floor.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `138 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: the stand-in floor's collision messages" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 6: The look

Minecraft's yaw and pitch from V's camera, and the teleport target that puts Minecraft's eye at the camera (the
spec's "The link"). Minecraft's yaw is 0 facing south, 90 west, 180 north and -90 east.

**Files:**
- Create: `src/link/Look.h`, `src/link/Look.cpp`, `tests/TestLook.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestLook.cpp`**

```cpp
#include "Harness.h"

#include "link/Look.h"

using namespace cpmc;
using namespace cpmc::link;

TEST(LookCompassDirections)
{
	// World axes: x east, y north, z up. Minecraft's yaw: 0 south, 90 west, 180 north, -90 east.
	CHECK_NEAR(LookFromForward({ 0, 1, 0 }, 0)->yaw, 180.0, 1e-4);
	CHECK_NEAR(LookFromForward({ 1, 0, 0 }, 0)->yaw, -90.0, 1e-4);
	CHECK_NEAR(LookFromForward({ 0, -1, 0 }, 0)->yaw, 0.0, 1e-4);
	CHECK_NEAR(LookFromForward({ -1, 0, 0 }, 0)->yaw, 90.0, 1e-4);
	CHECK_NEAR(LookFromForward({ 0, 1, 0 }, 0)->pitch, 0.0, 1e-4);
}

TEST(LookPitch)
{
	CHECK_NEAR(LookFromForward({ 0, 0.7071068f, -0.7071068f }, 0)->pitch, 45.0, 1e-3);  // down, toward the north
	CHECK_NEAR(LookFromForward({ 0, 0.7071068f, -0.7071068f }, 0)->yaw, 180.0, 1e-3);
	CHECK_NEAR(LookFromForward({ 0, 0, 2 }, 0)->pitch, -90.0, 1e-4);  // any length
}

TEST(LookStraightUpOrDownKeepsTheYaw)
{
	CHECK_NEAR(LookFromForward({ 0, 0, 1 }, 37.0f)->yaw, 37.0, 1e-6);
	CHECK_NEAR(LookFromForward({ 0, 0, -1 }, -12.0f)->yaw, -12.0, 1e-6);
	CHECK_NEAR(LookFromForward({ 0, 0, -1 }, -12.0f)->pitch, 90.0, 1e-4);
	CHECK(!LookFromForward({ 0, 0, 0 }, 0).has_value());
}

TEST(LookTeleportTargetPutsTheEyeAtTheCamera)
{
	// V's camera at spike 3's corner, 1.62 m above the road at z 6.14.
	const Vec3d feet = TeleportTarget({ -119.5, -2007.5, 7.76 });
	CHECK_NEAR(feet.x, -119.5, 1e-9);
	CHECK_NEAR(feet.y, 6.14, 1e-9);
	CHECK_NEAR(feet.z, 2007.5, 1e-9);
}

TEST(LookForwardRoundTrip)
{
	const auto look = LookFromForward(ForwardFromLook({ 30.0f, -20.0f }), 0);
	CHECK_NEAR(look->yaw, 30.0, 1e-3);
	CHECK_NEAR(look->pitch, -20.0, 1e-3);
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestFloor.cpp
)
```
```cmake
	tests/TestFloor.cpp
	tests/TestLook.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Look.h'`.

- [x] **Step 4: Write `src/link/Look.h`**

```cpp
#pragma once

#include "Coords.h"

#include <optional>

// Where Minecraft's player stands and looks, from V's camera (the spec's "The link"): its eye at the camera, looking
// the camera's way.
namespace cpmc::link
{
constexpr double kEyeHeight = 1.62;  // Minecraft's standing eye height, in blocks

// Minecraft's degrees: yaw 0 faces south (+z), 90 west, 180 north, -90 east; pitch is positive looking down.
struct McLook
{
	float yaw = 0.0f;
	float pitch = 0.0f;
};

// From a forward vector in the game's world axes, any length. Looking straight up or down (a horizontal part under
// 0.001 of its length), the yaw is aLastYaw. Nothing for a zero vector. The yaw comes out in (-180, 180].
std::optional<McLook> LookFromForward(const Vec3f& aForward, float aLastYaw);

// Where Minecraft's player's feet go (Minecraft axes) so that its eye is at aCamera (world axes).
Vec3d TeleportTarget(const Vec3d& aCamera);

// The forward vector, in the game's world axes, of a Minecraft look.
Vec3f ForwardFromLook(const McLook& aLook);
}
```

- [x] **Step 5: Write `src/link/Look.cpp`**

```cpp
#include "link/Look.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace cpmc::link
{
namespace
{
constexpr double kDegrees = 180.0 / std::numbers::pi;
}

std::optional<McLook> LookFromForward(const Vec3f& aForward, float aLastYaw)
{
	const Vec3d  mc = McFromWorld({ aForward.x, aForward.y, aForward.z });
	const double length = std::sqrt(mc.x * mc.x + mc.y * mc.y + mc.z * mc.z);
	if (length < 1e-9)
	{
		return std::nullopt;
	}
	const double x = mc.x / length;
	const double y = mc.y / length;
	const double z = mc.z / length;
	float        yaw = aLastYaw;
	if (std::sqrt(x * x + z * z) >= 0.001)
	{
		yaw = float(std::atan2(-x, z) * kDegrees);
		if (yaw <= -180.0f)
		{
			yaw += 360.0f;
		}
	}
	return McLook{ yaw, float(-std::asin(std::clamp(y, -1.0, 1.0)) * kDegrees) };
}

Vec3d TeleportTarget(const Vec3d& aCamera)
{
	const Vec3d eye = McFromWorld(aCamera);
	return { eye.x, eye.y - kEyeHeight, eye.z };
}

Vec3f ForwardFromLook(const McLook& aLook)
{
	const double yaw = aLook.yaw / kDegrees;
	const double pitch = aLook.pitch / kDegrees;
	const Vec3d  world = WorldFromMc({ -std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch) });
	return { float(world.x), float(world.y), float(world.z) };
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Floor.cpp
)
```
```cmake
	src/link/Floor.cpp
	src/link/Look.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `143 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: Minecraft's look and teleport target from V's camera" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 7: Typed commands

The overlay's command sets, and typing commands into Minecraft's chat through the input ring (the spec's "Typed
commands"): T, wait for the chat screen, the text, Enter, wait for the screen to close. Minecraft opens chat on its
next client tick, and takes text only while a screen is open. The sets that build ahead of V level the look first
(`rotated ~ 0`): `^ ^ ^4` follows the look's pitch too, so looking down would put them underground.

**Files:**
- Create: `src/link/Commands.h`, `src/link/Commands.cpp`, `tests/TestCommands.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestCommands.cpp`**

```cpp
#include "Harness.h"

#include "link/Commands.h"

using namespace cpmc::link;

namespace
{
const CommandTyper::Minecraft kReady{ true, false };
const CommandTyper::Minecraft kChatOpen{ true, true };

bool IsKey(const proto::InputEvent& aEvent, uint16_t aScancode, int32_t aDown)
{
	return aEvent.type == proto::kInKey && aEvent.code == aScancode && aEvent.a == aDown;
}
}

TEST(CommandsNamedSetsAreValidCommands)
{
	for (const char* name : { "creative", "tnt-kit", "tnt-ahead", "wall", "tnt-cube", "field", "wipe" })
	{
		const auto commands = NamedCommands(name);
		CHECK(commands.has_value());
		for (const auto& command : *commands)
		{
			std::string error;
			CHECK(ValidCommand(command, error));
			// "Ahead" is level, whatever the camera's pitch.
			CHECK(!command.starts_with("/execute") || command.starts_with("/execute rotated ~ 0 positioned ^ ^ ^"));
		}
	}
	CHECK_EQ(NamedCommands("tnt-cube")->size(), size_t(3));
	CHECK_STR(NamedCommands("tnt-cube")->front(), "/gamemode creative");
	CHECK_STR(NamedCommands("tnt-ahead")->at(1), "/execute rotated ~ 0 positioned ^ ^ ^4 run setblock ~ ~-1 ~ stone");  // TNT needs a block under it
	CHECK(!NamedCommands("explode").has_value());
}

TEST(CommandsValidation)
{
	std::string error;
	CHECK(ValidCommand("/gamemode creative", error));
	CHECK(!ValidCommand("", error));
	CHECK(!ValidCommand("gamemode creative", error));
	CHECK(!ValidCommand("/" + std::string(256, 'a'), error));
	CHECK(error.find("257") != std::string::npos);
	CHECK(!ValidCommand("/say caf\xc3\xa9", error));
	CHECK(!ValidCommand("/say a\nb", error));
}

TEST(CommandsTypesACommand)
{
	CommandTyper typer;
	std::string  error;
	CHECK(typer.Queue("/gamemode creative", error));
	auto step = typer.Advance(0.0, kReady);
	CHECK_EQ(step.events.size(), size_t(2));
	CHECK(IsKey(step.events[0], kSdlT, 1));
	CHECK(IsKey(step.events[1], kSdlT, 0));
	CHECK(typer.Advance(0.1, kReady).events.empty());  // chat opens on Minecraft's next tick
	step = typer.Advance(0.2, kChatOpen);
	CHECK(step.entered);
	CHECK_EQ(step.events.size(), size_t(18 + 2));
	CHECK(step.events[0].type == proto::kInText && step.events[0].a == '/');
	CHECK(step.events[17].type == proto::kInText && step.events[17].a == 'e');
	CHECK(IsKey(step.events[18], kSdlReturn, 1));
	CHECK(IsKey(step.events[19], kSdlReturn, 0));
	CHECK(typer.Advance(0.3, kChatOpen).events.empty());
	step = typer.Advance(0.4, kReady);
	CHECK_EQ(step.log.size(), size_t(1));
	CHECK_STR(step.log[0], "typed: /gamemode creative");
	CHECK_EQ(typer.Waiting(), size_t(0));
}

TEST(CommandsSetsAreTypedInOrder)
{
	CommandTyper typer;
	std::string  error;
	CHECK(typer.Queue("tnt-kit", error));
	CHECK_EQ(typer.Waiting(), size_t(2));
	std::vector<std::string> log;
	for (int round = 0; round < 2; ++round)
	{
		typer.Advance(round * 1.0, kReady);
		typer.Advance(round * 1.0 + 0.1, kChatOpen);
		for (auto& line : typer.Advance(round * 1.0 + 0.2, kReady).log)
		{
			log.push_back(line);
		}
	}
	CHECK_EQ(log.size(), size_t(2));
	CHECK_STR(log[0], "typed: /give @s tnt 64");
	CHECK_STR(log[1], "typed: /give @s flint_and_steel");
}

TEST(CommandsAreDroppedWhenMinecraftIsntReady)
{
	CommandTyper typer;
	std::string  error;
	typer.Queue("creative", error);
	auto step = typer.Advance(0.0, { false, false });
	CHECK(step.events.empty());
	CHECK_STR(step.log[0], "not typed: Minecraft isn't in its world: /gamemode creative");
	typer.Queue("creative", error);
	step = typer.Advance(0.0, kChatOpen);
	CHECK(step.events.empty());
	CHECK_STR(step.log[0], "not typed: a Minecraft screen is open: /gamemode creative");
	CHECK_EQ(typer.Waiting(), size_t(0));
}

TEST(CommandsTimeOut)
{
	CommandTyper typer;
	std::string  error;
	typer.Queue("creative", error);
	typer.Advance(0.0, kReady);
	auto step = typer.Advance(2.5, kReady);
	CHECK_STR(step.log[0], "not typed: chat didn't open within 2 s: /gamemode creative");
	typer.Queue("creative", error);
	typer.Advance(10.0, kReady);
	typer.Advance(10.1, kChatOpen);
	step = typer.Advance(12.5, kChatOpen);
	CHECK_STR(step.log[0], "typed, but chat didn't close within 2 s: /gamemode creative");
	CHECK_EQ(typer.Waiting(), size_t(0));
}

TEST(CommandsClearDropsEverything)
{
	CommandTyper typer;
	std::string  error;
	typer.Queue("tnt-cube", error);
	typer.Advance(0.0, kReady);
	typer.Clear();
	CHECK_EQ(typer.Waiting(), size_t(0));
	CHECK(typer.Advance(0.1, kChatOpen).events.empty());
}

TEST(CommandsQueueRefusesNonsense)
{
	CommandTyper typer;
	std::string  error;
	CHECK(!typer.Queue("explode", error));
	CHECK(error.find("neither a command nor a set") != std::string::npos);
	CHECK(!typer.Queue("/" + std::string(300, 'x'), error));
	CHECK(error.find("at most 256") != std::string::npos);
	CHECK_EQ(typer.Waiting(), size_t(0));
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestLook.cpp
)
```
```cmake
	tests/TestLook.cpp
	tests/TestCommands.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Commands.h'`.

- [x] **Step 4: Write `src/link/Commands.h`**

```cpp
#pragma once

#include <skycraft_protocol.h>

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Typing commands into Minecraft's chat through the input ring (the spec's "Typed commands"): press T, wait for the
// chat screen, send the text, press Enter, and wait for the screen to close before the next one.
namespace cpmc::link
{
namespace proto = skycraft::proto;

constexpr uint16_t kSdlT = 23;       // SDL scancodes (USB HID usages)
constexpr uint16_t kSdlReturn = 40;
constexpr double   kTypingTimeout = 2.0;  // seconds to wait for the chat screen to open, and then to close

// The overlay's command sets, by name: creative, tnt-kit, tnt-ahead, wall, tnt-cube, field and wipe.
std::optional<std::vector<std::string>> NamedCommands(std::string_view aName);

// A command starts with '/', is printable ASCII, and is at most 256 characters (Minecraft's chat limit).
bool ValidCommand(std::string_view aCommand, std::string& aError);

class CommandTyper
{
public:
	// A command, or the name of a set, which queues all of its commands. False, with aError, for anything else.
	bool Queue(std::string_view aCommandOrName, std::string& aError);

	// What Minecraft's state says, read from McState each frame.
	struct Minecraft
	{
		bool inWorld = false;
		bool screenOpen = false;
	};

	struct Step
	{
		std::vector<proto::InputEvent> events;  // to push into the input ring this frame, in order
		std::vector<std::string>       log;
		bool                           entered = false;  // Enter went this frame: a latency mark
	};

	// Once a frame. A command whose turn comes while Minecraft isn't in its world, or has a screen open, is dropped.
	Step Advance(double aNow, const Minecraft& aMinecraft);

	size_t Waiting() const { return queue_.size(); }  // the one being typed included
	void   Clear();                                   // drops everything: Minecraft has gone

private:
	enum class State
	{
		Idle,
		WaitingToOpen,
		WaitingToClose,
	};

	std::deque<std::string> queue_;
	State                   state_ = State::Idle;
	double                  deadline_ = 0.0;
};
}
```

- [x] **Step 5: Write `src/link/Commands.cpp`**

```cpp
#include "link/Commands.h"

#include <format>

namespace cpmc::link
{
namespace
{
proto::InputEvent Key(uint16_t aScancode, bool aDown)
{
	return { proto::kInKey, aScancode, aDown ? 1 : 0, 0, 0 };
}
}

std::optional<std::vector<std::string>> NamedCommands(std::string_view aName)
{
	if (aName == "creative")
	{
		return std::vector<std::string>{ "/gamemode creative" };
	}
	if (aName == "tnt-kit")
	{
		return std::vector<std::string>{ "/give @s tnt 64", "/give @s flint_and_steel" };
	}
	// "Ahead" is level: ^ ^ ^N follows the look's pitch too, which would put these underground whenever V looks down.
	if (aName == "tnt-ahead")
	{
		// On a stone block: only the player collides with the floor's triangles, so a TNT in the air falls into the void.
		return std::vector<std::string>{ "/gamemode creative", "/execute rotated ~ 0 positioned ^ ^ ^4 run setblock ~ ~-1 ~ stone",
			                             "/execute rotated ~ 0 positioned ^ ^ ^4 run summon tnt" };
	}
	if (aName == "wall")
	{
		return std::vector<std::string>{ "/execute rotated ~ 0 positioned ^ ^ ^4 run fill ~-2 ~ ~ ~2 ~3 ~ stone_bricks" };
	}
	if (aName == "tnt-cube")
	{
		return std::vector<std::string>{ "/gamemode creative", "/execute rotated ~ 0 positioned ^ ^ ^6 run fill ~-2 ~ ~-2 ~2 ~4 ~2 tnt",
			                             "/execute rotated ~ 0 positioned ^ ^ ^6 run summon tnt ~ ~5 ~" };
	}
	if (aName == "field")
	{
		return std::vector<std::string>{ "/fill ~-63 ~ ~-63 ~64 ~ ~64 grass_block" };
	}
	if (aName == "wipe")
	{
		return std::vector<std::string>{ "/fill ~-63 ~ ~-63 ~64 ~ ~64 air", "/fill ~-16 ~ ~-16 ~15 ~31 ~15 air" };
	}
	return std::nullopt;
}

bool ValidCommand(std::string_view aCommand, std::string& aError)
{
	if (aCommand.empty() || aCommand.front() != '/')
	{
		aError = "a command starts with /";
		return false;
	}
	if (aCommand.size() > 256)
	{
		aError = std::format("a command is at most 256 characters, and this is {}", aCommand.size());
		return false;
	}
	for (const char ch : aCommand)
	{
		if (ch < 32 || ch > 126)
		{
			aError = "a command is printable ASCII";
			return false;
		}
	}
	return true;
}

bool CommandTyper::Queue(std::string_view aCommandOrName, std::string& aError)
{
	if (const auto named = NamedCommands(aCommandOrName))
	{
		queue_.insert(queue_.end(), named->begin(), named->end());
		return true;
	}
	if (!ValidCommand(aCommandOrName, aError))
	{
		if (aCommandOrName.empty() || aCommandOrName.front() != '/')
		{
			aError = std::format("\"{}\" is neither a command nor a set (creative, tnt-kit, tnt-ahead, wall, tnt-cube, field, wipe)",
			                     aCommandOrName);
		}
		return false;
	}
	queue_.emplace_back(aCommandOrName);
	return true;
}

CommandTyper::Step CommandTyper::Advance(double aNow, const Minecraft& aMinecraft)
{
	Step step;
	switch (state_)
	{
	case State::Idle:
		if (queue_.empty())
		{
			break;
		}
		if (!aMinecraft.inWorld || aMinecraft.screenOpen)
		{
			step.log.push_back(std::format("not typed: {}: {}", aMinecraft.inWorld ? "a Minecraft screen is open" : "Minecraft isn't in its world",
			                               queue_.front()));
			queue_.pop_front();
			break;
		}
		step.events = { Key(kSdlT, true), Key(kSdlT, false) };
		state_ = State::WaitingToOpen;
		deadline_ = aNow + kTypingTimeout;
		break;
	case State::WaitingToOpen:
		if (aMinecraft.screenOpen)
		{
			for (const char ch : queue_.front())
			{
				step.events.push_back({ proto::kInText, 0, int32_t(ch), 0, 0 });
			}
			step.events.push_back(Key(kSdlReturn, true));
			step.events.push_back(Key(kSdlReturn, false));
			step.entered = true;
			state_ = State::WaitingToClose;
			deadline_ = aNow + kTypingTimeout;
		}
		else if (aNow > deadline_)
		{
			step.log.push_back("not typed: chat didn't open within 2 s: " + queue_.front());
			queue_.pop_front();
			state_ = State::Idle;
		}
		break;
	case State::WaitingToClose:
		if (!aMinecraft.screenOpen)
		{
			step.log.push_back("typed: " + queue_.front());
			queue_.pop_front();
			state_ = State::Idle;
		}
		else if (aNow > deadline_)
		{
			step.log.push_back("typed, but chat didn't close within 2 s: " + queue_.front());
			queue_.pop_front();
			state_ = State::Idle;
		}
		break;
	}
	return step;
}

void CommandTyper::Clear()
{
	queue_.clear();
	state_ = State::Idle;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Look.cpp
)
```
```cmake
	src/link/Look.cpp
	src/link/Commands.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `151 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: commands typed into Minecraft's chat" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 8: The mapping

The named mapping, made as SkyCraft's Skyrim side makes it, with the deep review's reuse rules (the spec's "link/"):
refuse one too small, refuse one another live game side still uses, and otherwise reset what the game side owns. The
tests make real mappings under names of their own, and `advapi32` provides the security descriptor's functions.

**Files:**
- Create: `src/link/Mapping.h`, `src/link/Mapping.cpp`, `tests/TestMapping.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestMapping.cpp`**

```cpp
#include "Harness.h"

#include "link/Mapping.h"

#include <skycraft_protocol.h>

#include <cstring>
#include <format>

using namespace cpmc::link;
namespace proto = skycraft::proto;

namespace
{
// A name of the tests' own, so they never touch SkyCraft's real mapping.
std::wstring TestName()
{
	static int count = 0;
	return std::format(L"Local\\BlockRenderTest_{}_{}", GetCurrentProcessId(), count++);
}

constexpr uint32_t kGonePid = 0xFFFFFFF0;  // no process has it

proto::Header HeaderOf(const Mapping& aMapping)
{
	proto::Header header;
	std::memcpy(&header, aMapping.Base(), sizeof header);
	return header;
}
}

TEST(MappingCreatesAndWritesTheHeader)
{
	Mapping    mapping;
	const auto opened = mapping.Open(TestName().c_str(), 1234, 5000);
	CHECK(opened.outcome == Mapping::Outcome::Created);
	CHECK(mapping.Base() != nullptr);
	auto header = HeaderOf(mapping);
	CHECK_EQ(header.magic, proto::kMagic);
	CHECK_EQ(header.version, proto::kVersion);
	CHECK_EQ(header.skyrimPid, 1234u);
	CHECK_EQ(header.skyrimHeartbeatMs, 5000ull);
	mapping.Heartbeat(6000);
	CHECK_EQ(HeaderOf(mapping).skyrimHeartbeatMs, 6000ull);
	CHECK_EQ(mapping.McPid(), 0u);
	CHECK_EQ(mapping.McHeartbeatMs(), 0ull);
}

TEST(MappingIsReusedAfterAGameSideHasGone)
{
	const auto name = TestName();
	Mapping    minecraft;  // stands for Minecraft, holding the mapping open after the game side has gone
	CHECK(minecraft.Open(name.c_str(), kGonePid, 1000).outcome == Mapping::Outcome::Created);
	const uint64_t staleTail = 77;
	std::memcpy(minecraft.Base() + proto::kOffInputRing + proto::kInputRingTailOff, &staleTail, sizeof staleTail);
	Mapping    game;
	const auto opened = game.Open(name.c_str(), GetCurrentProcessId(), 1500);
	CHECK(opened.outcome == Mapping::Outcome::Reused);
	CHECK_EQ(HeaderOf(game).skyrimPid, uint32_t(GetCurrentProcessId()));
	uint64_t tail = 1;
	std::memcpy(&tail, game.Base() + proto::kOffInputRing + proto::kInputRingTailOff, sizeof tail);
	CHECK_EQ(tail, 0ull);  // reset, as SkyCraft does
}

TEST(MappingRefusesALiveGameSide)
{
	const auto name = TestName();
	Mapping    first;
	CHECK(first.Open(name.c_str(), GetCurrentProcessId(), 10'000).outcome == Mapping::Outcome::Created);
	Mapping    second;
	const auto refused = second.Open(name.c_str(), 12, 10'100);  // as if we were process 12: this one is another, alive
	CHECK(refused.outcome == Mapping::Outcome::Refused);
	CHECK(refused.why.find("another game side") != std::string::npos);
	CHECK(second.Base() == nullptr);
	CHECK(second.Open(name.c_str(), 12, 20'000).outcome == Mapping::Outcome::Reused);  // its heartbeat has gone stale
}

TEST(MappingRefusesOneTooSmall)
{
	const auto name = TestName();
	HANDLE     small = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 64 * 1024, name.c_str());
	CHECK(small != nullptr);
	Mapping    mapping;
	const auto refused = mapping.Open(name.c_str(), GetCurrentProcessId(), 1000);
	CHECK(refused.outcome == Mapping::Outcome::Refused);
	CHECK(refused.why.find("smaller") != std::string::npos);
	CloseHandle(small);
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestCommands.cpp
)
```
```cmake
	tests/TestCommands.cpp
	tests/TestMapping.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Mapping.h'`.

- [x] **Step 4: Write `src/link/Mapping.h`**

```cpp
#pragma once

#include <Windows.h>

#include <cstdint>
#include <string>

// The shared mapping, made the way SkyCraft's Skyrim side makes it (skse/src/Link.cpp, MIT): the game side creates it,
// and Minecraft opens it. It allows this user, the system and administrators at medium integrity, so a game run
// elevated doesn't lock Minecraft out.
namespace cpmc::link
{
constexpr uint64_t kAliveMs = 3000;  // a heartbeat younger than this means that side is there (SkyCraft's kMcTimeoutMs)

class Mapping
{
public:
	enum class Outcome
	{
		Created,
		Reused,
		Refused,
		Failed,
	};

	struct Opened
	{
		Outcome     outcome = Outcome::Failed;
		std::string why;
	};

	Mapping() = default;
	Mapping(const Mapping&) = delete;
	Mapping& operator=(const Mapping&) = delete;
	~Mapping();

	// Creates aName at the protocol's size, or opens the one already there: Minecraft keeps it open between game
	// sides. A mapping smaller than the protocol's layout is refused, and so is one another game side still uses (the
	// header's game process ID is a live process other than aOurPid, and its heartbeat is under 3 s old). Otherwise
	// the regions the game side owns are reset, and the header is written for aOurPid.
	Opened Open(const wchar_t* aName, uint32_t aOurPid, uint64_t aNowMs);
	void   Close();

	uint8_t* Base() const { return base_; }  // null until opened
	void     Heartbeat(uint64_t aNowMs);
	uint32_t McPid() const;
	uint64_t McHeartbeatMs() const;

private:
	HANDLE   handle_ = nullptr;
	uint8_t* base_ = nullptr;
};
}
```

- [x] **Step 5: Write `src/link/Mapping.cpp`**

```cpp
#include "link/Mapping.h"

#include <skycraft_protocol.h>

#include <sddl.h>

#include <atomic>
#include <cstring>
#include <format>
#include <vector>

namespace cpmc::link
{
namespace proto = skycraft::proto;

namespace
{
template<class T>
std::atomic_ref<T> Atomic(T& aValue)
{
	return std::atomic_ref<T>(aValue);
}

// SkyCraft's SharedWithThisUser: this user, the system and administrators, at medium integrity. Null (the default
// security) if it can't be built. Free with LocalFree.
PSECURITY_DESCRIPTOR SharedWithThisUser()
{
	std::wstring sddl = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)";
	HANDLE       token = nullptr;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
	{
		DWORD size = 0;
		GetTokenInformation(token, TokenUser, nullptr, 0, &size);
		std::vector<uint8_t> buffer(size);
		if (size && GetTokenInformation(token, TokenUser, buffer.data(), size, &size))
		{
			LPWSTR sid = nullptr;
			if (ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid, &sid))
			{
				sddl += std::wstring(L"(A;;GA;;;") + sid + L")";
				LocalFree(sid);
			}
		}
		CloseHandle(token);
	}
	sddl += L"S:(ML;;NW;;;ME)";
	PSECURITY_DESCRIPTOR descriptor = nullptr;
	if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr))
	{
		return nullptr;
	}
	return descriptor;
}

bool ProcessAlive(uint32_t aPid)
{
	HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, aPid);
	if (!process)
	{
		return false;
	}
	const bool alive = WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
	CloseHandle(process);
	return alive;
}
}

Mapping::~Mapping()
{
	Close();
}

Mapping::Opened Mapping::Open(const wchar_t* aName, uint32_t aOurPid, uint64_t aNowMs)
{
	Close();
	constexpr uint64_t  size = proto::kMappingBytes;
	SECURITY_ATTRIBUTES access{ sizeof access, SharedWithThisUser(), FALSE };
	handle_ = CreateFileMappingW(INVALID_HANDLE_VALUE, access.lpSecurityDescriptor ? &access : nullptr, PAGE_READWRITE, DWORD(size >> 32),
	                             DWORD(size & 0xFFFFFFFF), aName);
	const DWORD created = GetLastError();
	if (access.lpSecurityDescriptor)
	{
		LocalFree(access.lpSecurityDescriptor);
	}
	if (!handle_)
	{
		return { Outcome::Failed, std::format("CreateFileMapping failed ({})", created) };
	}
	const bool existed = created == ERROR_ALREADY_EXISTS;
	base_ = static_cast<uint8_t*>(MapViewOfFile(handle_, FILE_MAP_ALL_ACCESS, 0, 0, 0));
	if (!base_)
	{
		const DWORD error = GetLastError();
		Close();
		return { Outcome::Failed, std::format("MapViewOfFile failed ({})", error) };
	}
	MEMORY_BASIC_INFORMATION region{};
	if (VirtualQuery(base_, &region, sizeof region) == 0 || region.RegionSize < size)
	{
		const uint64_t found = region.RegionSize;
		Close();
		return { Outcome::Refused, std::format("the mapping already there is {} bytes, smaller than the protocol's {}", found, size) };
	}
	auto* header = reinterpret_cast<proto::Header*>(base_);
	if (existed)
	{
		const uint32_t pid = Atomic(header->skyrimPid).load(std::memory_order_acquire);
		const uint64_t beat = Atomic(header->skyrimHeartbeatMs).load(std::memory_order_acquire);
		const bool     fresh = beat >= aNowMs || aNowMs - beat < kAliveMs;
		if (pid != 0 && pid != aOurPid && fresh && ProcessAlive(pid))
		{
			Close();
			return { Outcome::Refused, std::format("another game side (process {}) is using it", pid) };
		}
	}
	// As SkyCraft does: everything the game side owns starts from a known state, rings and the overlay's swap included.
	std::memset(base_ + proto::kOffSkyState, 0, sizeof(proto::SkyState));
	std::memset(base_ + proto::kOffOverlayCtl, 0, 0x100);
	std::memset(base_ + proto::kOffInputRing, 0, proto::kInputRingDataOff);
	std::memset(base_ + proto::kOffCollisionRing, 0, proto::kColRingDataOff);
	std::memset(base_ + proto::kOffActorTable, 0, sizeof(proto::ActorTable));
	std::memset(base_ + proto::kOffEventRing, 0, proto::kEventRingDataOff);
	std::memset(base_ + proto::kOffWorldEntities, 0, sizeof(proto::WorldEntities));
	std::memset(base_ + proto::kOffRenderRing, 0, proto::kRenRingDataOff);
	Atomic(header->version).store(proto::kVersion, std::memory_order_relaxed);
	Atomic(header->skyrimPid).store(aOurPid, std::memory_order_relaxed);
	Atomic(header->skyrimHeartbeatMs).store(aNowMs, std::memory_order_relaxed);
	Atomic(header->magic).store(proto::kMagic, std::memory_order_release);
	return { existed ? Outcome::Reused : Outcome::Created, existed ? "reused" : "created" };
}

void Mapping::Close()
{
	if (base_)
	{
		UnmapViewOfFile(base_);
		base_ = nullptr;
	}
	if (handle_)
	{
		CloseHandle(handle_);
		handle_ = nullptr;
	}
}

void Mapping::Heartbeat(uint64_t aNowMs)
{
	if (base_)
	{
		Atomic(reinterpret_cast<proto::Header*>(base_)->skyrimHeartbeatMs).store(aNowMs, std::memory_order_release);
	}
}

uint32_t Mapping::McPid() const
{
	return base_ ? Atomic(reinterpret_cast<proto::Header*>(base_)->mcPid).load(std::memory_order_acquire) : 0;
}

uint64_t Mapping::McHeartbeatMs() const
{
	return base_ ? Atomic(reinterpret_cast<proto::Header*>(base_)->mcHeartbeatMs).load(std::memory_order_acquire) : 0;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source and `advapi32`**

```cmake
	src/link/Commands.cpp
)
target_link_libraries(render_core PUBLIC render_settings d3dcompiler dxguid)
```
```cmake
	src/link/Commands.cpp
	src/link/Mapping.cpp
)
target_link_libraries(render_core PUBLIC render_settings d3dcompiler dxguid advapi32)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `155 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: the shared mapping, made and reused as SkyCraft does" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 9: The link

All of it once a frame (the spec's "The link" and "Blocks"): the heartbeat, Minecraft coming and going, `SkyState`,
teleports by step and rate, the floor once Minecraft is there, typed commands, explosions, the render ring within its
budget, and HUD images on request. The tests give it a fake Minecraft in plain memory, laid out as the mapping.

Writing it settled one detail the spec left loose: what a bad message does to the ring. The deep review changed one
rule SkyCraft's own game side follows: the overlay's swap starts over only for a new Minecraft process, since starting
it over for Minecraft coming back can leave both sides on one slot. The last step records both in the spec's "Changes
while planning", with the other corrections planning Part 1 found.

**Files:**
- Create: `src/link/Link.h`, `src/link/Link.cpp`, `tests/TestLink.cpp`
- Modify: `CMakeLists.txt`, `docs/superpowers/specs/2026-10-02-minecraft-link-design.md`

- [x] **Step 1: Write the failing test, `tests/TestLink.cpp`**

```cpp
#include "Harness.h"

#include "link/Link.h"

#include <Windows.h>

#include <cstring>

using namespace cpmc;
using namespace cpmc::link;

namespace
{
// The mapping's layout in plain memory, with Minecraft's side played the way SkyCraft's SkyLink plays it.
struct FakeMinecraft
{
	FakeMinecraft()
	{
		base = static_cast<uint8_t*>(VirtualAlloc(nullptr, proto::kMappingBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	}
	~FakeMinecraft() { VirtualFree(base, 0, MEM_RELEASE); }

	proto::Header* Header() { return reinterpret_cast<proto::Header*>(base); }

	void Beat(uint32_t aPid, uint64_t aTickMs)
	{
		Header()->mcPid = aPid;
		Header()->mcHeartbeatMs = aTickMs;
	}

	void State(uint32_t aFlags)
	{
		proto::McState state{};
		std::memcpy(&state, base + proto::kOffMcState, sizeof state);
		state.seq += 2;
		state.flags = aFlags;
		state.x = -119.5;
		state.frameCounter += 1;
		std::memcpy(base + proto::kOffMcState, &state, sizeof state);
	}

	void Send(uint32_t aType, const std::vector<uint8_t>& aPayload)
	{
		CHECK(WriteByteRing(base + proto::kOffRenderRing, proto::kRenRingDataBytes, aType, aPayload));
	}

	proto::SkyState Sky()
	{
		proto::SkyState state;
		std::memcpy(&state, base + proto::kOffSkyState, sizeof state);
		return state;
	}

	std::vector<RingMessage> Collision(std::vector<std::vector<uint8_t>>& aPayloads)
	{
		std::vector<RingMessage> messages;
		DrainByteRing(base + proto::kOffCollisionRing, proto::kColRingDataBytes, ~0ull, [&](const RingMessage& aMessage) {
			aPayloads.emplace_back(aMessage.payload.begin(), aMessage.payload.end());
			messages.push_back(aMessage);
			return true;
		});
		return messages;
	}

	std::vector<proto::InputEvent> Inputs()
	{
		auto*                          ring = base + proto::kOffInputRing;
		uint64_t&                      head = *reinterpret_cast<uint64_t*>(ring);
		uint64_t&                      tail = *reinterpret_cast<uint64_t*>(ring + 0x40);
		std::vector<proto::InputEvent> events;
		for (; tail < head; ++tail)
		{
			proto::InputEvent event;
			std::memcpy(&event, ring + 0x80 + (tail % proto::kInputRingEntries) * sizeof event, sizeof event);
			events.push_back(event);
		}
		return events;
	}

	void Explode(float aX, float aY, float aZ, float aRadius)
	{
		auto*          ring = base + proto::kOffEventRing;
		uint64_t&      head = *reinterpret_cast<uint64_t*>(ring);
		proto::McEvent event{ proto::kEvExplosion, 0, aX, aY, aZ, aRadius, 0, 0 };
		std::memcpy(ring + 0x80 + (head % proto::kEventRingEntries) * sizeof event, &event, sizeof event);
		++head;
	}

	void PublishHud(uint32_t aWidth, uint32_t aHeight)
	{
		proto::OverlaySlotHdr slot{};
		slot.width = aWidth;
		slot.height = aHeight;
		slot.flags = 1;
		std::memcpy(base + proto::kOffOverlaySlotHdr + hudBack * sizeof slot, &slot, sizeof slot);
		uint32_t&      state = *reinterpret_cast<uint32_t*>(base + proto::kOffOverlayCtl);
		const uint32_t old = state;
		state = hudBack | proto::kOverlayDirty;
		hudBack = old & 3;
	}

	// The overlay's middle slot, and the slot a HUD image's pixels are in.
	uint32_t Middle() { return *reinterpret_cast<uint32_t*>(base + proto::kOffOverlayCtl) & 3; }
	uint32_t SlotOf(const uint8_t* aPixels) { return uint32_t((aPixels - (base + proto::kOffOverlayPixels)) / proto::kOverlaySlotBytes); }

	uint8_t* base = nullptr;
	uint32_t hudBack = 1;
};

struct Sink : LinkSink
{
	void Atlas(const AtlasImage& aAtlas) override { atlases.push_back({ aAtlas.width, aAtlas.height }); }
	void Section(SectionMesh&& aMesh) override { sections.push_back(std::move(aMesh)); }
	void ClearAll() override { ++clears; }
	void Explosion(const proto::McEvent& aEvent) override { explosions.push_back(aEvent); }
	void Hud(const HudFrame& aFrame) override
	{
		huds.push_back({ aFrame.width, aFrame.height });
		hudPixels = aFrame.pixels;
	}
	void Log(const std::string& aLine) override { log.push_back(aLine); }

	int Count(const std::string& aStart) const  // log lines starting with aStart
	{
		int count = 0;
		for (const auto& line : log)
		{
			if (line.rfind(aStart, 0) == 0)
			{
				++count;
			}
		}
		return count;
	}

	bool Logged(const std::string& aStart) const { return Count(aStart) > 0; }

	std::vector<std::pair<uint32_t, uint32_t>> atlases;
	std::vector<SectionMesh>                   sections;
	int                                        clears = 0;
	std::vector<proto::McEvent>                explosions;
	std::vector<std::pair<uint32_t, uint32_t>> huds;
	const uint8_t*                             hudPixels = nullptr;  // the last HUD image's
	std::vector<std::string>                   log;
};

// The overlay's three slots stay apart: Minecraft's back slot, the middle, and the one the link took last.
bool SlotsApart(FakeMinecraft& aMinecraft, const Sink& aSink)
{
	const uint32_t taken = aMinecraft.SlotOf(aSink.hudPixels);
	const uint32_t middle = aMinecraft.Middle();
	return taken != middle && taken != aMinecraft.hudBack && middle != aMinecraft.hudBack;
}

// V at spike 3's corner, looking north: feet on the road at z 6.14, the camera 1.62 m above.
GamePose Corner(double aEast = 0.0, double aUp = 0.0)
{
	return { { -119.5 + aEast, -2007.5, 6.14 + aUp }, { -119.5 + aEast, -2007.5, 7.76 + aUp }, { 0.0f, 1.0f, 0.0f } };
}

Link::Frame At(double aNow, std::optional<GamePose> aPose = Corner(), bool aLinked = true)
{
	Link::Frame frame;
	frame.now = aNow;
	frame.tickMs = uint64_t(aNow * 1000.0);
	frame.pose = aPose;
	frame.linked = aLinked;
	frame.buildMode = true;
	return frame;
}

Link::Frame HudAt(double aNow)  // a frame that takes HUD images
{
	Link::Frame frame = At(aNow);
	frame.takeHud = true;
	return frame;
}

std::vector<uint8_t> Atlas2x2()
{
	std::vector<uint8_t> payload(8 + 16, 0x7F);
	const uint32_t       side = 2;
	std::memcpy(payload.data(), &side, 4);
	std::memcpy(payload.data() + 4, &side, 4);
	return payload;
}

std::vector<uint8_t> SectionPayload(int32_t aX, uint32_t aVertices, uint32_t aClaimed)
{
	std::vector<uint8_t> payload(16 + aVertices * sizeof(Vertex));
	const int32_t        zero = 0;
	std::memcpy(payload.data(), &aX, 4);
	std::memcpy(payload.data() + 4, &zero, 4);
	std::memcpy(payload.data() + 8, &zero, 4);
	std::memcpy(payload.data() + 12, &aClaimed, 4);
	return payload;
}

const LinkSettings kSettings;
}

TEST(LinkWritesTheHeartbeatAndSkyState)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	LinkSettings  settings;
	settings.viewportWidth = 1280;
	settings.viewportHeight = 720;
	auto frame = At(1.0, std::nullopt, false);
	frame.buildMode = false;
	link.Update(frame, settings, sink);
	CHECK_EQ(minecraft.Header()->skyrimHeartbeatMs, 1000ull);
	const auto sky = minecraft.Sky();
	CHECK_EQ(sky.seq, 2u);
	CHECK_EQ(sky.flags, uint32_t(proto::kSkyMenuOpen));  // not in game, and build mode is off
	CHECK_EQ(sky.viewportW, 1280u);
	CHECK_EQ(sky.viewportH, 720u);
	CHECK_EQ(sky.worldId, 1u);
	frame.linked = true;  // linked, but V isn't there: not in game, and no teleport
	link.Update(frame, settings, sink);
	CHECK_EQ(minecraft.Sky().flags, uint32_t(proto::kSkyMenuOpen));
	CHECK_EQ(minecraft.Sky().teleportSeq, 0u);
}

TEST(LinkSeesMinecraftComeAndGo)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), kSettings, sink);
	CHECK(link.State().minecraft);
	CHECK_EQ(link.State().minecraftPid, 77u);
	CHECK(link.State().inWorld);
	CHECK_NEAR(link.State().minecraftFeet.x, -119.5, 1e-9);
	CHECK(sink.Logged("Minecraft is there (process 77)"));
	link.Update(At(5.0), kSettings, sink);  // no heartbeat for 4 s
	CHECK(!link.State().minecraft);
	CHECK(!link.State().inWorld);
	CHECK(sink.Logged("Minecraft has gone"));
}

TEST(LinkTeleportsMinecraftsEyeToTheCamera)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	link.Update(At(1.0), kSettings, sink);
	const auto sky = minecraft.Sky();
	CHECK_EQ(sky.flags, uint32_t(proto::kSkyInGame));  // build mode on: no menu flag
	CHECK_EQ(sky.teleportSeq, 1u);
	CHECK_NEAR(sky.posX, -119.5, 1e-9);
	CHECK_NEAR(sky.posY, 6.14, 1e-9);
	CHECK_NEAR(sky.posZ, 2007.5, 1e-9);
	CHECK_NEAR(sky.yaw, 180.0, 1e-4);
	CHECK_NEAR(sky.pitch, 0.0, 1e-4);
	link.Update(At(2.0, Corner(), false), kSettings, sink);
	CHECK_EQ(minecraft.Sky().flags, 0u);  // unlinked
}

TEST(LinkTeleportsByStepAndRate)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	link.Update(At(1.00), kSettings, sink);
	link.Update(At(1.50, Corner(0.05)), kSettings, sink);
	CHECK_EQ(minecraft.Sky().teleportSeq, 1u);  // under the 0.1 m step
	link.Update(At(1.55, Corner(0.25)), kSettings, sink);
	CHECK_EQ(minecraft.Sky().teleportSeq, 2u);
	link.Update(At(1.60, Corner(0.50)), kSettings, sink);
	CHECK_EQ(minecraft.Sky().teleportSeq, 2u);  // only 0.05 s since the last: 10 a second at most
	link.Update(At(1.66, Corner(0.50)), kSettings, sink);
	CHECK_EQ(minecraft.Sky().teleportSeq, 3u);
	CHECK_NEAR(minecraft.Sky().posX, -119.0, 1e-9);
}

TEST(LinkSendsTheFloorOnceMinecraftIsThere)
{
	FakeMinecraft                     minecraft;
	Link                              link(minecraft.base);
	Sink                              sink;
	std::vector<std::vector<uint8_t>> payloads;
	link.Update(At(1.0), kSettings, sink);
	CHECK(minecraft.Collision(payloads).empty());  // nobody reads it yet
	minecraft.Beat(77, 2000);
	link.Update(At(2.0), kSettings, sink);
	auto messages = minecraft.Collision(payloads);
	CHECK_EQ(messages.size(), size_t(50));
	CHECK_EQ(messages[0].type, uint32_t(proto::kColClear));
	CHECK(payloads[0] == std::vector<uint8_t>({ 1, 0, 0, 0 }));
	CHECK_EQ(messages[1].type, uint32_t(proto::kColTris));
	CHECK_EQ(minecraft.Sky().collisionEpoch, 1u);
	minecraft.Beat(77, 2100);
	link.Update(At(2.1), kSettings, sink);
	CHECK(minecraft.Collision(payloads).empty());
	minecraft.Beat(77, 2200);
	link.Update(At(2.2, Corner(20.0)), kSettings, sink);  // 20 m along: a new floor
	CHECK_EQ(minecraft.Collision(payloads).size(), size_t(50));
	CHECK_EQ(link.State().floorEpoch, 2u);
	minecraft.Beat(77, 2300);
	link.Update(At(2.3, Corner(20.0, 1.5)), kSettings, sink);  // 1.5 m up
	CHECK_EQ(link.State().floorEpoch, 3u);
	link.RequestFloor();
	minecraft.Beat(77, 2400);
	link.Update(At(2.4, Corner(20.0, 1.5)), kSettings, sink);
	CHECK_EQ(link.State().floorEpoch, 4u);
	CHECK_NEAR(link.State().floorCentre.y, 7.64, 1e-9);
}

TEST(LinkDropsSectionsBeforeTheAtlas)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Send(proto::kRenSection, SectionPayload(1, 3, 3));
	link.Update(At(1.0), kSettings, sink);
	CHECK(sink.sections.empty());
	CHECK_EQ(link.State().sectionsBeforeAtlas, 1ull);
	minecraft.Send(proto::kRenClearAll, {});
	minecraft.Send(proto::kRenAtlas, Atlas2x2());
	minecraft.Send(proto::kRenSection, SectionPayload(2, 6, 6));
	link.Update(At(1.1), kSettings, sink);
	CHECK_EQ(sink.clears, 1);
	CHECK_EQ(sink.atlases.size(), size_t(1));
	CHECK_EQ(sink.atlases[0].first, 2u);
	CHECK_EQ(sink.sections.size(), size_t(1));
	CHECK_EQ(sink.sections[0].x, 2);
	CHECK_EQ(sink.sections[0].vertices.size(), size_t(6));
	CHECK(link.State().atlasSeen);
}

TEST(LinkStepsOverOtherMessagesAndKeepsToTheBudget)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Send(proto::kRenAtlas, Atlas2x2());
	for (int i = 0; i < 3; ++i)
	{
		minecraft.Send(proto::kRenScene, std::vector<uint8_t>(4000));
		minecraft.Send(proto::kRenSection, SectionPayload(i, 3, 3));
	}
	LinkSettings settings;
	settings.ringBudget = 100;  // the atlas (32 bytes) and one section (120 bytes) reach it
	link.Update(At(1.0), settings, sink);
	CHECK_EQ(sink.sections.size(), size_t(1));
	CHECK_EQ(link.State().messages[proto::kRenScene], 1ull);  // stepped over, without counting toward the budget
	link.Update(At(1.1), kSettings, sink);
	CHECK_EQ(sink.sections.size(), size_t(3));
	CHECK_EQ(link.State().messages[proto::kRenScene], 3ull);
	CHECK_EQ(link.State().messages[proto::kRenSection], 3ull);
}

TEST(LinkRejectsABadPayloadAndCarriesOn)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Send(proto::kRenAtlas, Atlas2x2());
	minecraft.Send(proto::kRenSection, SectionPayload(1, 3, 100));  // claims 100 vertices, carries 3
	minecraft.Send(proto::kRenSection, SectionPayload(2, 3, 3));
	link.Update(At(1.0), kSettings, sink);
	CHECK_EQ(link.State().rejected, 1ull);
	CHECK(link.State().lastRejected.find("claims 100 vertices") != std::string::npos);
	CHECK_EQ(sink.sections.size(), size_t(1));
	CHECK_EQ(sink.sections[0].x, 2);
	CHECK_EQ(link.State().resyncs, 0ull);
}

TEST(LinkTypesCommandsThroughTheInputRing)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	std::string   error;
	CHECK(link.Type("/gamemode creative", error));
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), kSettings, sink);
	auto inputs = minecraft.Inputs();
	CHECK_EQ(inputs.size(), size_t(2));
	CHECK(inputs[0].type == proto::kInKey && inputs[0].code == kSdlT && inputs[0].a == 1);
	minecraft.Beat(77, 1100);
	minecraft.State(proto::kMcInWorld | proto::kMcScreenOpen);
	link.Update(At(1.1), kSettings, sink);
	inputs = minecraft.Inputs();
	CHECK_EQ(inputs.size(), size_t(18 + 2));
	CHECK(link.State().lastEnter.has_value());
	CHECK_NEAR(*link.State().lastEnter, 1.1, 1e-9);
	minecraft.Beat(77, 1200);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.2), kSettings, sink);
	CHECK(sink.Logged("typed: /gamemode creative"));
	CHECK_EQ(link.State().commandsWaiting, size_t(0));
}

TEST(LinkPassesOnExplosionsAndHudImages)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.Explode(-117.0f, 6.0f, 2010.0f, 4.0f);
	link.Update(At(1.0), kSettings, sink);  // Minecraft connects
	CHECK_EQ(sink.explosions.size(), size_t(1));
	CHECK_EQ(sink.explosions[0].a, -117.0f);
	CHECK_EQ(sink.explosions[0].d, 4.0f);
	minecraft.PublishHud(1280, 720);
	minecraft.Beat(77, 1100);
	link.Update(At(1.1), kSettings, sink);
	CHECK(sink.huds.empty());  // not asked for, so it waits
	auto frame = At(1.2);
	frame.takeHud = true;
	minecraft.Beat(77, 1200);
	link.Update(frame, kSettings, sink);
	CHECK_EQ(sink.huds.size(), size_t(1));
	CHECK_EQ(sink.huds[0].first, 1280u);
	CHECK_EQ(link.State().hudImages, 1ull);
}

TEST(LinkKeepsAnImagePublishedBeforeItSawMinecraft)
{
	// SkyCraft ships an image left from before as soon as it relinks, which can be before the link first sees it.
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.PublishHud(1280, 720);
	link.Update(HudAt(1.0), kSettings, sink);
	CHECK_EQ(sink.huds.size(), size_t(1));
	CHECK(SlotsApart(minecraft, sink));
	minecraft.PublishHud(1280, 720);
	minecraft.Beat(77, 1100);
	link.Update(HudAt(1.1), kSettings, sink);
	CHECK_EQ(sink.huds.size(), size_t(2));
	CHECK(SlotsApart(minecraft, sink));
}

TEST(LinkKeepsTheSwapWhenMinecraftComesBack)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Beat(77, 1000);
	link.Update(HudAt(1.0), kSettings, sink);
	minecraft.PublishHud(1280, 720);
	minecraft.Beat(77, 1100);
	link.Update(HudAt(1.1), kSettings, sink);
	CHECK_EQ(sink.huds.size(), size_t(1));
	link.Update(HudAt(5.0), kSettings, sink);  // no heartbeat for 3.9 s: gone
	CHECK(!link.State().minecraft);
	minecraft.Beat(77, 5100);  // back: the same process, its writer's slot where it was
	link.Update(HudAt(5.1), kSettings, sink);
	minecraft.PublishHud(1280, 720);
	minecraft.Beat(77, 5200);
	link.Update(HudAt(5.2), kSettings, sink);
	CHECK_EQ(sink.huds.size(), size_t(2));
	CHECK(SlotsApart(minecraft, sink));
}

TEST(LinkStartsTheSwapOverForANewMinecraft)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Beat(77, 1000);
	link.Update(HudAt(1.0), kSettings, sink);
	minecraft.PublishHud(1280, 720);
	minecraft.Beat(77, 1100);
	link.Update(HudAt(1.1), kSettings, sink);
	CHECK_EQ(sink.huds.size(), size_t(1));
	minecraft.hudBack = 1;  // Minecraft restarted: a new process, its writer starting over
	minecraft.Beat(78, 2000);
	link.Update(HudAt(2.0), kSettings, sink);
	CHECK(sink.Logged("Minecraft is there (process 78)"));
	minecraft.PublishHud(1280, 720);
	minecraft.Beat(78, 2100);
	link.Update(HudAt(2.1), kSettings, sink);
	CHECK_EQ(sink.huds.size(), size_t(2));
	CHECK(SlotsApart(minecraft, sink));
}

TEST(LinkLogsRingProblemsOnceASecond)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Send(proto::kRenAtlas, Atlas2x2());
	for (int i = 0; i < 3; ++i)
	{
		minecraft.Send(proto::kRenSection, SectionPayload(i, 3, 100));  // each claims 100 vertices, and carries 3
	}
	link.Update(At(1.0), kSettings, sink);
	CHECK_EQ(link.State().rejected, 3ull);
	CHECK_EQ(sink.Count("render ring:"), 1);
	CHECK(sink.Logged("render ring: 3 rejected messages or resyncs since the last report; the last: section 2 0 0"));
	minecraft.Send(proto::kRenSection, SectionPayload(5, 3, 100));
	link.Update(At(1.5), kSettings, sink);
	CHECK_EQ(link.State().rejected, 4ull);
	CHECK_EQ(sink.Count("render ring:"), 1);  // within the second: counted, not logged
	link.Update(At(2.1), kSettings, sink);
	CHECK_EQ(sink.Count("render ring:"), 2);  // logged once the second has passed
	CHECK(sink.Logged("render ring: section 5 0 0 claims 100 vertices"));
}

TEST(LinkMenuFlagFollowsBuildMode)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	auto          frame = At(1.0);
	frame.buildMode = false;
	link.Update(frame, kSettings, sink);
	CHECK_EQ(minecraft.Sky().flags, uint32_t(proto::kSkyInGame | proto::kSkyMenuOpen));
	frame.buildMode = true;
	link.Update(frame, kSettings, sink);
	CHECK_EQ(minecraft.Sky().flags, uint32_t(proto::kSkyInGame));
}

TEST(LinkCountsDroppedInputs)
{
	FakeMinecraft           minecraft;
	Link                    link(minecraft.base);
	Sink                    sink;
	const proto::InputEvent click{ proto::kInMouseButton, 3, 1, 0, 0 };
	for (uint32_t i = 0; i < proto::kInputRingEntries; ++i)
	{
		CHECK(link.Push(click));
	}
	CHECK(!link.Push(click));
	link.Update(At(1.0), kSettings, sink);
	CHECK_EQ(link.State().inputsDropped, 1ull);
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestMapping.cpp
)
```
```cmake
	tests/TestMapping.cpp
	tests/TestLink.cpp
)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Link.h'`.

- [x] **Step 4: Write `src/link/Link.h`**

```cpp
#pragma once

#include "Coords.h"
#include "link/Commands.h"
#include "link/Look.h"
#include "link/Messages.h"
#include "link/Overlay.h"
#include "link/Rings.h"

#include <array>
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

// The game side of SkyCraft's protocol, once a frame (the spec's "The link" and "Blocks"): the heartbeat and SkyState,
// teleports and the floor, typed commands, Minecraft's events, the render ring, and HUD images. It works on the
// mapping's memory, so the tests give it plain memory with a fake Minecraft.
namespace cpmc::link
{
// V's pose in the game's world axes (metres: x east, y north, z up), as PlayerLocator finds it.
struct GamePose
{
	Vec3d feet;
	Vec3d camera;
	Vec3f forward;  // the camera's
};

struct LinkSettings
{
	double   teleportStep = 0.1;        // metres the camera moves before Minecraft's player follows
	double   teleportRate = 10.0;       // teleports a second, at most
	uint64_t ringBudget = 16ull << 20;  // ring bytes of atlases and sections taken each frame, checked between messages
	bool     keepTranslucent = false;   // draw translucent triangles as cut-outs instead of dropping them
	uint32_t viewportWidth = 0;         // the HUD image's size to ask for; 0 leaves Minecraft's window alone
	uint32_t viewportHeight = 0;
};

// Where the link hands on what Minecraft sends: the renderer in the game, a report in mclink. Called inside Update,
// with payloads that point into the mapping and stay valid only during the call.
class LinkSink
{
public:
	virtual ~LinkSink() = default;
	virtual void Atlas(const AtlasImage& aAtlas) = 0;
	virtual void Section(SectionMesh&& aMesh) = 0;
	virtual void ClearAll() = 0;
	virtual void Explosion(const proto::McEvent&) {}
	virtual void Hud(const HudFrame&) {}
	virtual void Log(const std::string&) {}
};

class Link
{
public:
	explicit Link(uint8_t* aBase);  // the mapping's memory, laid out as skycraft_protocol.h has it

	struct Frame
	{
		double                  now = 0.0;          // seconds, on any steady clock
		uint64_t                tickMs = 0;         // GetTickCount64(), the clock both heartbeats use
		std::optional<GamePose> pose;               // nothing: V isn't there
		bool                    linked = false;     // the user's Link or Unlink
		bool                    buildMode = false;  // off sets SkyState's menu flag: Minecraft releases held keys
		bool                    takeHud = false;    // hand on HUD images this frame
	};

	void Update(const Frame& aFrame, const LinkSettings& aSettings, LinkSink& aSink);

	// Push may be called from any thread. Everything else, from the thread that calls Update.
	bool Type(std::string_view aCommandOrName, std::string& aError);  // queues a command, or a named set
	bool Push(const proto::InputEvent& aEvent);                        // the input ring; false if dropped
	void RequestFloor();                                               // a fresh floor at the next update

	static constexpr size_t kTypes = 13;  // the render ring's message types, 0 to 11, and 12 for any other

	struct Status
	{
		// Minecraft, as its heartbeat and McState say
		bool     minecraft = false;  // its heartbeat is under 3 s old
		uint32_t minecraftPid = 0;
		uint64_t heartbeatAgeMs = 0;
		bool     inWorld = false;
		bool     screenOpen = false;
		Vec3d    minecraftFeet;        // Minecraft axes
		uint64_t minecraftFrames = 0;  // its frame counter
		// The game side
		bool     inGame = false;
		uint32_t teleportSeq = 0;
		uint64_t teleports = 0;
		Vec3d    target;  // the last teleport's, Minecraft axes
		McLook   look;
		uint32_t floorEpoch = 0;
		bool     floorSent = false;
		Vec3d    floorCentre;  // Minecraft axes; y is the floor's height
		// The render ring, since the start
		std::array<uint64_t, kTypes> messages{};
		std::array<uint64_t, kTypes> bytes{};
		uint64_t                     drainedBytes = 0;
		uint64_t                     resyncs = 0;
		std::string                  lastResync;
		uint64_t                     rejected = 0;  // messages whose payload failed its layout's checks
		std::string                  lastRejected;
		bool                         atlasSeen = false;
		uint64_t                     sectionsBeforeAtlas = 0;
		uint64_t                     translucentDropped = 0;
		// Events, input, commands and the HUD
		uint64_t              explosions = 0;
		uint64_t              otherEvents = 0;
		uint64_t              inputsDropped = 0;
		size_t                commandsWaiting = 0;
		std::optional<double> lastEnter;  // when a typed command's Enter last went, on Frame::now's clock
		uint64_t              hudImages = 0;
		uint64_t              hudRefused = 0;
	};

	const Status& State() const { return status_; }

private:
	void SendFloor(const Vec3d& aFeet, LinkSink& aSink);
	bool Take(const RingMessage& aMessage, const LinkSettings& aSettings, LinkSink& aSink);

	uint8_t*              base_;
	OverlayReader         overlay_;
	CommandTyper          typer_;
	std::mutex            inputMutex_;
	std::atomic<uint64_t> inputsDropped_{ 0 };
	Status                status_;
	bool                  wasThere_ = false;
	uint32_t              lastPid_ = 0;
	bool                  wasInGame_ = false;
	bool                  teleportDue_ = true;
	double                lastTeleportAt_ = -1e9;
	bool                  floorRequested_ = false;
	uint64_t              problemsLogged_ = 0;  // resyncs and rejected messages already logged
	double                problemLoggedAt_ = -1e9;
	std::string           lastProblem_;
};
}
```

- [x] **Step 5: Write `src/link/Link.cpp`**

```cpp
#include "link/Link.h"

#include "link/Floor.h"
#include "link/Mapping.h"

#include <cmath>
#include <format>

namespace cpmc::link
{
namespace
{
template<class T>
std::atomic_ref<T> Atomic(T& aValue)
{
	return std::atomic_ref<T>(aValue);
}

double Distance(const Vec3d& aA, const Vec3d& aB)
{
	return std::sqrt((aA.x - aB.x) * (aA.x - aB.x) + (aA.y - aB.y) * (aA.y - aB.y) + (aA.z - aB.z) * (aA.z - aB.z));
}
}

Link::Link(uint8_t* aBase)
    : base_(aBase), overlay_(aBase + proto::kOffOverlayCtl, aBase + proto::kOffOverlaySlotHdr, aBase + proto::kOffOverlayPixels,
                             proto::kOverlaySlotBytes)
{
}

void Link::Update(const Frame& aFrame, const LinkSettings& aSettings, LinkSink& aSink)
{
	// The heartbeat, then whether Minecraft is there: its heartbeat under 3 s old.
	auto* header = reinterpret_cast<proto::Header*>(base_);
	Atomic(header->skyrimHeartbeatMs).store(aFrame.tickMs, std::memory_order_release);
	const uint32_t pid = Atomic(header->mcPid).load(std::memory_order_acquire);
	const uint64_t beat = Atomic(header->mcHeartbeatMs).load(std::memory_order_acquire);
	const bool     there = pid != 0 && beat != 0 && (beat >= aFrame.tickMs || aFrame.tickMs - beat < kAliveMs);
	status_.minecraft = there;
	status_.minecraftPid = pid;
	status_.heartbeatAgeMs = beat != 0 && aFrame.tickMs > beat ? aFrame.tickMs - beat : 0;
	if (there && (!wasThere_ || pid != lastPid_))
	{
		// Minecraft (re)connected. A new Minecraft sends its clear, atlas and sections by itself; the game side sends
		// the floor and a teleport.
		if (lastPid_ != 0 && pid != lastPid_)
		{
			// A new Minecraft process: its overlay writer starts over at slot 1, so the swap does too. Its first image
			// comes once it's in its world, long after this. SkyCraft's Game.cpp also resets for the first Minecraft
			// it sees, and for one coming back, but neither needs it: Mapping::Open reset the swap before Minecraft
			// could see this game side, and Minecraft starts its writer over when it does; one coming back kept its
			// slots. A reset then could undo an image Minecraft has just published (it ships one left from before as
			// soon as it relinks), leaving its writer and our reader on one slot.
			overlay_.Reset();
		}
		status_.floorSent = false;
		teleportDue_ = true;
		aSink.Log(std::format("Minecraft is there (process {})", pid));
	}
	if (!there && wasThere_)
	{
		typer_.Clear();
		aSink.Log("Minecraft has gone");
	}
	wasThere_ = there;
	if (there)
	{
		lastPid_ = pid;
	}
	const auto mc = there ? ReadMcState(base_) : std::nullopt;
	status_.inWorld = mc && (mc->flags & proto::kMcInWorld);
	status_.screenOpen = mc && (mc->flags & proto::kMcScreenOpen);
	if (mc)
	{
		status_.minecraftFeet = { mc->x, mc->y, mc->z };
		status_.minecraftFrames = mc->frameCounter;
	}

	// In game while linked with V there. Linking teleports Minecraft's player and sends a fresh floor.
	const bool inGame = aFrame.linked && aFrame.pose.has_value();
	if (inGame && !wasInGame_)
	{
		teleportDue_ = true;
		status_.floorSent = false;
	}
	wasInGame_ = inGame;
	status_.inGame = inGame;
	if (aFrame.pose)
	{
		if (const auto look = LookFromForward(aFrame.pose->forward, status_.look.yaw))
		{
			status_.look = *look;
		}
	}
	if (inGame)
	{
		const Vec3d target = TeleportTarget(aFrame.pose->camera);
		const bool  due = teleportDue_ || Distance(target, status_.target) >= aSettings.teleportStep;
		if (due && aFrame.now - lastTeleportAt_ >= 1.0 / aSettings.teleportRate)
		{
			++status_.teleportSeq;
			++status_.teleports;
			status_.target = target;
			lastTeleportAt_ = aFrame.now;
			teleportDue_ = false;
		}
		const Vec3d feet = McFromWorld(aFrame.pose->feet);
		if (there && (!status_.floorSent || floorRequested_ || std::hypot(feet.x - status_.floorCentre.x, feet.z - status_.floorCentre.z) > 16.0 ||
		              std::abs(feet.y - status_.floorCentre.y) > 1.0))
		{
			SendFloor(feet, aSink);
			floorRequested_ = false;
		}
	}

	proto::SkyState state{};
	state.flags = (inGame ? uint32_t(proto::kSkyInGame) : 0u) | (aFrame.buildMode ? 0u : uint32_t(proto::kSkyMenuOpen));
	state.worldId = 1;
	state.collisionEpoch = status_.floorEpoch;
	state.posX = status_.target.x;
	state.posY = status_.target.y;
	state.posZ = status_.target.z;
	state.yaw = status_.look.yaw;
	state.pitch = status_.look.pitch;
	state.teleportSeq = status_.teleportSeq;
	state.viewportW = aSettings.viewportWidth;
	state.viewportH = aSettings.viewportHeight;
	WriteSkyState(base_, state);

	const auto step = typer_.Advance(aFrame.now, { status_.inWorld, status_.screenOpen });
	for (const auto& event : step.events)
	{
		Push(event);
	}
	for (const auto& line : step.log)
	{
		aSink.Log(line);
	}
	if (step.entered)
	{
		status_.lastEnter = aFrame.now;
	}
	status_.commandsWaiting = typer_.Waiting();
	status_.inputsDropped = inputsDropped_.load();

	DrainEvents(base_ + proto::kOffEventRing, [&](const proto::McEvent& aEvent) {
		if (aEvent.type == proto::kEvExplosion)
		{
			++status_.explosions;
			aSink.Explosion(aEvent);
		}
		else
		{
			++status_.otherEvents;
		}
	});

	const auto drained = DrainByteRing(base_ + proto::kOffRenderRing, proto::kRenRingDataBytes, aSettings.ringBudget,
	                                   [&](const RingMessage& aMessage) { return Take(aMessage, aSettings, aSink); });
	status_.drainedBytes += drained.bytes;
	if (drained.resynced)
	{
		++status_.resyncs;
		status_.lastResync = drained.why;
		lastProblem_ = drained.why;
	}
	// Resyncs and rejected messages: counted each time, logged at most once a second, so a broken stream can't flood
	// the log.
	const uint64_t problems = status_.resyncs + status_.rejected;
	if (problems > problemsLogged_ && aFrame.now - problemLoggedAt_ >= 1.0)
	{
		const uint64_t fresh = problems - problemsLogged_;
		aSink.Log(fresh == 1 ? "render ring: " + lastProblem_
		                     : std::format("render ring: {} rejected messages or resyncs since the last report; the last: {}", fresh, lastProblem_));
		problemsLogged_ = problems;
		problemLoggedAt_ = aFrame.now;
	}

	if (aFrame.takeHud && there)
	{
		if (const auto frame = overlay_.Acquire())
		{
			++status_.hudImages;
			aSink.Hud(*frame);
		}
		status_.hudRefused = overlay_.Refused();
	}
}

bool Link::Take(const RingMessage& aMessage, const LinkSettings& aSettings, LinkSink& aSink)
{
	const size_t type = aMessage.type < kTypes - 1 ? aMessage.type : kTypes - 1;
	++status_.messages[type];
	status_.bytes[type] += aMessage.payload.size();
	std::string error;
	const auto  reject = [&] {
        ++status_.rejected;
        status_.lastRejected = error;
        lastProblem_ = error;
	};
	switch (aMessage.type)
	{
	case proto::kRenAtlas:
		if (const auto atlas = ParseAtlas(aMessage.payload, error))
		{
			status_.atlasSeen = true;
			aSink.Atlas(*atlas);
		}
		else
		{
			reject();
		}
		return true;
	case proto::kRenSection:
		if (!status_.atlasSeen)
		{
			++status_.sectionsBeforeAtlas;  // left over from before we read: its UVs belong to an atlas we never saw
			return false;
		}
		if (auto mesh = ParseSection(aMessage.payload, aSettings.keepTranslucent, error))
		{
			status_.translucentDropped += mesh->translucentDropped;
			aSink.Section(std::move(*mesh));
		}
		else
		{
			reject();
		}
		return true;
	case proto::kRenClearAll:
		status_.atlasSeen = false;  // Minecraft sends its atlas next
		aSink.ClearAll();
		return true;
	default:
		return false;  // atlas patches, lights, solids, digging, textures, avatar, scene, ragdoll: stepped over
	}
}

void Link::SendFloor(const Vec3d& aFeet, LinkSink& aSink)
{
	// A new epoch each time: its clear message drops the last floor, and anything half-sent.
	const uint32_t epoch = status_.floorEpoch + 1;
	uint8_t* const ring = base_ + proto::kOffCollisionRing;
	bool           sent = WriteByteRing(ring, proto::kColRingDataBytes, proto::kColClear, FloorClear(epoch));
	for (const auto& [x, z] : FloorRegions(aFeet.x, aFeet.z))
	{
		sent = sent && WriteByteRing(ring, proto::kColRingDataBytes, proto::kColTris, FloorRegion(x, z, aFeet.y, epoch));
	}
	status_.floorEpoch = epoch;
	status_.floorSent = sent;
	if (sent)
	{
		status_.floorCentre = aFeet;
		aSink.Log(std::format("floor {} at height {:.2f} around Minecraft {:.1f} {:.1f}", epoch, aFeet.y, aFeet.x, aFeet.z));
	}
	else
	{
		aSink.Log("the floor didn't fit in the collision ring; trying again next frame");
	}
}

bool Link::Type(std::string_view aCommandOrName, std::string& aError)
{
	return typer_.Queue(aCommandOrName, aError);
}

bool Link::Push(const proto::InputEvent& aEvent)
{
	std::lock_guard lock(inputMutex_);
	if (PushInput(base_ + proto::kOffInputRing, aEvent))
	{
		return true;
	}
	++inputsDropped_;
	return false;
}

void Link::RequestFloor()
{
	floorRequested_ = true;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Mapping.cpp
)
```
```cmake
	src/link/Mapping.cpp
	src/link/Link.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `171 test(s), 0 failure(s)`.

- [x] **Step 8: Edit `docs/superpowers/specs/2026-10-02-minecraft-link-design.md` for the changes while planning**

```markdown
  | `tnt-ahead` | `/gamemode creative`, `/execute anchored eyes positioned ^ ^ ^4 run summon tnt` | Primed TNT 4 blocks ahead of the eye |
  | `wall` | `/execute positioned ^ ^ ^4 run fill ~-2 ~ ~ ~2 ~3 ~ stone_bricks` | A stone-brick wall 5 wide and 4 tall, 4 blocks ahead, running east to west |
  | `tnt-cube` | `/gamemode creative`, `/execute positioned ^ ^ ^6 run fill ~-2 ~ ~-2 ~2 ~4 ~2 tnt`, `/execute positioned ^ ^ ^6 run summon tnt ~ ~5 ~` | 125 TNT, 5×5×5, 6 blocks ahead, set off by primed TNT on top: the worst burst |
```
```markdown
  | `tnt-ahead` | `/gamemode creative`, `/execute rotated ~ 0 positioned ^ ^ ^4 run setblock ~ ~-1 ~ stone`, `/execute rotated ~ 0 positioned ^ ^ ^4 run summon tnt` | Primed TNT on a stone block, 4 blocks ahead |
  | `wall` | `/execute rotated ~ 0 positioned ^ ^ ^4 run fill ~-2 ~ ~ ~2 ~3 ~ stone_bricks` | A stone-brick wall 5 wide and 4 tall, 4 blocks ahead, running east to west |
  | `tnt-cube` | `/gamemode creative`, `/execute rotated ~ 0 positioned ^ ^ ^6 run fill ~-2 ~ ~-2 ~2 ~4 ~2 tnt`, `/execute rotated ~ 0 positioned ^ ^ ^6 run summon tnt ~ ~5 ~` | 125 TNT, 5×5×5, 6 blocks ahead, set off by primed TNT on top: the worst burst |
```
```markdown
  - a message that fails skips the ring to its head, counted and logged once a second;
```
```markdown
  - a message whose framing doesn't fit (its header, or its length) skips the ring to its head, since nothing after
    it can be trusted. One whose payload fails its own checks is dropped by itself, as the framing around it is
    still sound. Both are counted, and logged once a second;
```
```markdown
  - **back:** a new heartbeat or process ID. Minecraft sends its clear, atlas and sections again by itself. The link
    sends the floor and a teleport, and resets the overlay's triple buffer, as SkyCraft's `Game.cpp` does;
```
```markdown
  - **back:** a new heartbeat or process ID. Minecraft sends its clear, atlas and sections again by itself. The link
    sends the floor and a teleport. It resets the overlay's triple buffer only for a new Minecraft process, whose
    writer starts over (see "Changes while planning");
```
```markdown
  - the plugin and `mclink` each refuse while another live game side holds it, and retry once a second;
```
```markdown
  - the plugin refuses while another live game side holds it, and retries once a second. `mclink` refuses to start,
    and says why;
```
```markdown
  It resets when Minecraft connects or comes back.
```
```markdown
  Link resets it only for a new Minecraft process (see "Changes while planning").
```
```markdown
- **The protocol:** the overlay's triple buffer is read, as SkyCraft's `Link::AcquireOverlayFrame` reads it, and reset
  when Minecraft comes back. `SkyState`'s viewport asks for the back buffer's size over `hudscale` (2), 1280×720,
  instead of 0. The GUI then looks as Minecraft's automatic scale would make it at 2560×1440, with a quarter of the
  bandwidth.
```
```markdown
- **The protocol:** the overlay's triple buffer is read, as SkyCraft's `Link::AcquireOverlayFrame` reads it, and reset
  for a new Minecraft process (see "Changes while planning"). `SkyState`'s viewport asks for the back buffer's size
  over `hudscale` (2), 1280×720, instead of 0. The GUI then looks as Minecraft's automatic scale would make it at
  2560×1440, with a quarter of the bandwidth.
```
```markdown
- **Minimizing:** `Window.onFramebufferResize` ignores a 0×0 size, and `renderFrame` reconfigures the GPU surface only
  when the window isn't minimized, which SkyCraft's mixin denies while linked.
```
```markdown
- **Minimizing:** `Window.onFramebufferResize` ignores a 0×0 size, and `renderFrame` reconfigures the GPU surface only
  when the window isn't minimized, which SkyCraft's mixin denies while linked.

## Changes while planning

Writing and verifying Part 1's code corrected these details. The sections above already include them.
- **Rejected messages:** only a message whose framing doesn't fit the ring moves the tail to the head. One whose
  payload fails its checks (a section claiming more vertices than it carries, say) is dropped by itself.
- **TNT needs a block under it:** in SkyCraft, only the local player collides with the floor's triangles. Every other
  entity collides only with voxel regions (`BlockCollisionsMixin`), and this spike never sends any. So TNT summoned in
  the air falls through the floor into the void, and explodes far below. `tnt-ahead` puts a stone block under its TNT,
  and step 1's TNT lands on a block. Dropped items fall through the floor too.
- **"Ahead" is level:** `^ ^ ^4` follows the look's pitch as well as its yaw, so whenever V looked down, `tnt-ahead`,
  `wall` and `tnt-cube` would have built underground. Their commands start with `rotated ~ 0`, which keeps the yaw and
  levels the pitch.
- **The overlay's reset:** SkyCraft's `Game.cpp` resets the overlay's triple buffer whenever Minecraft connects or
  comes back. The link resets it only for a new Minecraft process, whose writer starts over at slot 1 and publishes
  only once it's in its world. The first Minecraft the link sees needs no reset: `Mapping::Open` reset the buffer
  before Minecraft could see the new game side, and Minecraft starts its writer over when it does. One coming back
  kept its slots. A reset in either case could undo an image Minecraft had just published (it ships one left from
  before as soon as it relinks), leaving its writer and the link's reader on one slot, with torn images from then on.
- **`mclink` refuses rather than retries,** as its own section says; "Safety" had it retrying like the plugin.
- **`mclink`'s input:** a script, one action a line (`tools/mclink/McLinkScript.h`), so step 1's runs can change
  without a rebuild.
```

- [x] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render docs/superpowers/specs \
  && git commit -q -m "minecraft-link: the link, once a frame" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 10: mclink

A console stand-in for Cyberpunk's side (the spec's "`mclink.exe`"). It opens the mapping, runs the link at 60 Hz with
Minecraft's player pinned where its script says, follows the script, and reports what Minecraft sends. Step 1's two
scripts come with it.

**Files:**
- Create: `tools/mclink/McLinkScript.h`, `tools/mclink/McLinkScript.cpp`, `tools/mclink/Main.cpp`,
  `tools/mclink/step1.txt`, `tools/mclink/again.txt`, `tests/TestMcLinkScript.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestMcLinkScript.cpp`**

```cpp
#include "Harness.h"

#include "McLinkScript.h"

using namespace cpmc::mclink;
using Kind = Action::Kind;

TEST(ScriptParsesEveryAction)
{
	const char* script = R"(# step 1
status
wait-world 180

link on
build off
hud on
at -119.5 6.14 2007.5
look 180 60
wait 0.5
type creative
key 30
click right
floor
save-atlas atlas.dds
save-hud hud.dds
hold-check 10
quit
)";
	std::string error;
	const auto  actions = ParseScript(script, error);
	CHECK(actions.has_value());
	CHECK_EQ(actions->size(), size_t(16));
	const auto& a = *actions;
	CHECK(a[0].kind == Kind::Status);
	CHECK_EQ(a[0].line, 2);
	CHECK(a[1].kind == Kind::WaitWorld);
	CHECK_EQ(a[1].numbers[0], 180.0);
	CHECK(a[2].kind == Kind::Link && a[2].on);
	CHECK_EQ(a[2].line, 5);
	CHECK(a[3].kind == Kind::Build && !a[3].on);
	CHECK(a[4].kind == Kind::Hud && a[4].on);
	CHECK(a[5].kind == Kind::At);
	CHECK_EQ(a[5].numbers[0], -119.5);
	CHECK_EQ(a[5].numbers[1], 6.14);
	CHECK_EQ(a[5].numbers[2], 2007.5);
	CHECK(a[6].kind == Kind::Look);
	CHECK_EQ(a[6].numbers[1], 60.0);
	CHECK(a[7].kind == Kind::Wait);
	CHECK_EQ(a[7].numbers[0], 0.5);
	CHECK(a[8].kind == Kind::Type);
	CHECK_STR(a[8].text, "creative");
	CHECK(a[9].kind == Kind::Key);
	CHECK_EQ(a[9].numbers[0], 30.0);
	CHECK(a[10].kind == Kind::Click);
	CHECK_EQ(a[10].numbers[0], 3.0);
	CHECK(a[11].kind == Kind::Floor);
	CHECK(a[12].kind == Kind::SaveAtlas);
	CHECK_STR(a[12].text, "atlas.dds");
	CHECK(a[13].kind == Kind::SaveHud);
	CHECK(a[14].kind == Kind::HoldCheck);
	CHECK_EQ(a[14].numbers[0], 10.0);
	CHECK(a[15].kind == Kind::Quit);
}

TEST(ScriptTypeKeepsTheWholeCommand)
{
	std::string error;
	const auto  actions = ParseScript("  type /setblock ~2 ~ ~ minecraft:stone  \r\n", error);
	CHECK(actions.has_value());
	CHECK_STR(actions->at(0).text, "/setblock ~2 ~ ~ minecraft:stone");
}

TEST(ScriptWaitWorldDefaultsToTwoMinutes)
{
	std::string error;
	CHECK_EQ(ParseScript("wait-world", error)->at(0).numbers[0], 120.0);
}

TEST(ScriptRejectsBadLines)
{
	std::string error;
	CHECK(!ParseScript("status\nfly away\n", error).has_value());
	CHECK_STR(error, "line 2: unknown action: \"fly away\"");
	CHECK(!ParseScript("wait soon", error).has_value());
	CHECK(!ParseScript("look 0 95", error).has_value());
	CHECK(!ParseScript("key 0", error).has_value());
	CHECK(!ParseScript("key 2.5", error).has_value());
	CHECK(!ParseScript("click sideways", error).has_value());
	CHECK(!ParseScript("at 1 2", error).has_value());
	CHECK(!ParseScript("link maybe", error).has_value());
	CHECK(!ParseScript("type", error).has_value());
	CHECK(!ParseScript("status now", error).has_value());
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test and its include folder**

```cmake
	tests/TestLink.cpp
)
target_link_libraries(render_tests PRIVATE render_core)
```
```cmake
	tests/TestLink.cpp
	tests/TestMcLinkScript.cpp
)
target_include_directories(render_tests PRIVATE tools/mclink)
target_link_libraries(render_tests PRIVATE render_core)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'McLinkScript.h'`.

- [x] **Step 4: Write `tools/mclink/McLinkScript.h`**

```cpp
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// mclink's scripts: one action a line, run in order at 60 Hz. Blank lines and lines starting with # are skipped.
//   wait <seconds>                 pause
//   wait-world [seconds]           until Minecraft is in its world (120 s unless given), or give up
//   link on|off                    the in-game flag
//   build on|off                   build mode: off sets SkyState's menu flag (on by default)
//   hud on|off                     take HUD images (on by default)
//   at <x> <y> <z>                 Minecraft's player's feet, in Minecraft blocks
//   look <yaw> <pitch>             Minecraft degrees
//   type <command or set>          type it, and wait until it's typed (10 s at most)
//   key <scancode>                 press and release an SDL scancode
//   click left|middle|right        press and release a mouse button
//   floor                          send a fresh floor
//   save-atlas <file.dds>          wait for Minecraft's atlas (30 s at most), and save it
//   save-hud <file.dds>            wait for the next HUD image (5 s at most), save it top row first, and check its alpha
//   hold-check <seconds>           how far Minecraft's feet stray from the last teleport target meanwhile
//   status                         print the link's state
//   quit                           stop here
namespace cpmc::mclink
{
struct Action
{
	enum class Kind
	{
		Wait,
		WaitWorld,
		Link,
		Build,
		Hud,
		At,
		Look,
		Type,
		Key,
		Click,
		Floor,
		SaveAtlas,
		SaveHud,
		HoldCheck,
		Status,
		Quit,
	};

	Kind        kind = Kind::Quit;
	double      numbers[3]{};  // seconds; x, y, z; yaw and pitch; the scancode; the button (1 left, 2 middle, 3 right)
	bool        on = false;    // link, build and hud
	std::string text;          // the command, or the file
	int         line = 0;
};

// The script's actions, or nothing with aError naming the first bad line.
std::optional<std::vector<Action>> ParseScript(std::string_view aText, std::string& aError);
}
```

- [x] **Step 5: Write `tools/mclink/McLinkScript.cpp`**

```cpp
#include "McLinkScript.h"

#include <charconv>
#include <cmath>
#include <format>

namespace cpmc::mclink
{
namespace
{
std::string_view Trim(std::string_view aText)
{
	const size_t first = aText.find_first_not_of(" \t\r");
	if (first == std::string_view::npos)
	{
		return {};
	}
	return aText.substr(first, aText.find_last_not_of(" \t\r") - first + 1);
}

std::vector<std::string_view> Words(std::string_view aText)
{
	std::vector<std::string_view> words;
	size_t                        at = 0;
	while (at < aText.size())
	{
		const size_t start = aText.find_first_not_of(" \t", at);
		if (start == std::string_view::npos)
		{
			break;
		}
		const size_t end = aText.find_first_of(" \t", start);
		words.push_back(aText.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
		at = end == std::string_view::npos ? aText.size() : end;
	}
	return words;
}

bool Number(std::string_view aWord, double& aOut)
{
	const auto [end, error] = std::from_chars(aWord.data(), aWord.data() + aWord.size(), aOut);
	return error == std::errc() && end == aWord.data() + aWord.size() && std::isfinite(aOut);
}
}

std::optional<std::vector<Action>> ParseScript(std::string_view aText, std::string& aError)
{
	std::vector<Action> actions;
	int                 lineNumber = 0;
	size_t              at = 0;
	while (at <= aText.size())
	{
		const size_t           end = aText.find('\n', at);
		const std::string_view line = Trim(aText.substr(at, end == std::string_view::npos ? std::string_view::npos : end - at));
		at = end == std::string_view::npos ? aText.size() + 1 : end + 1;
		++lineNumber;
		if (line.empty() || line.front() == '#')
		{
			continue;
		}
		const auto       words = Words(line);
		const auto       verb = words[0];
		const auto       rest = Trim(line.substr(verb.size()));
		Action           action;
		action.line = lineNumber;
		const auto fail = [&](std::string_view aWhy) {
			aError = std::format("line {}: {}: \"{}\"", lineNumber, aWhy, line);
			return std::nullopt;
		};
		const auto numbers = [&](size_t aCount) {
			if (words.size() != aCount + 1)
			{
				return false;
			}
			for (size_t i = 0; i < aCount; ++i)
			{
				if (!Number(words[i + 1], action.numbers[i]))
				{
					return false;
				}
			}
			return true;
		};
		const auto onOff = [&] {
			if (words.size() == 2 && (words[1] == "on" || words[1] == "off"))
			{
				action.on = words[1] == "on";
				return true;
			}
			return false;
		};
		if (verb == "wait")
		{
			action.kind = Action::Kind::Wait;
			if (!numbers(1) || action.numbers[0] < 0)
			{
				return fail("wait takes a number of seconds");
			}
		}
		else if (verb == "wait-world")
		{
			action.kind = Action::Kind::WaitWorld;
			action.numbers[0] = 120;
			if (words.size() > 1 && (!numbers(1) || action.numbers[0] <= 0))
			{
				return fail("wait-world takes an optional number of seconds");
			}
		}
		else if (verb == "link" || verb == "build" || verb == "hud")
		{
			action.kind = verb == "link" ? Action::Kind::Link : verb == "build" ? Action::Kind::Build : Action::Kind::Hud;
			if (!onOff())
			{
				return fail(std::format("{} takes on or off", verb));
			}
		}
		else if (verb == "at")
		{
			action.kind = Action::Kind::At;
			if (!numbers(3))
			{
				return fail("at takes x, y and z");
			}
		}
		else if (verb == "look")
		{
			action.kind = Action::Kind::Look;
			if (!numbers(2) || std::abs(action.numbers[1]) > 90)
			{
				return fail("look takes a yaw and a pitch from -90 to 90");
			}
		}
		else if (verb == "type")
		{
			action.kind = Action::Kind::Type;
			action.text = std::string(rest);
			if (action.text.empty())
			{
				return fail("type takes a command or a set");
			}
		}
		else if (verb == "key")
		{
			action.kind = Action::Kind::Key;
			if (!numbers(1) || action.numbers[0] < 1 || action.numbers[0] > 511 || action.numbers[0] != std::floor(action.numbers[0]))
			{
				return fail("key takes an SDL scancode, 1 to 511");
			}
		}
		else if (verb == "click")
		{
			action.kind = Action::Kind::Click;
			if (words.size() != 2 || (words[1] != "left" && words[1] != "middle" && words[1] != "right"))
			{
				return fail("click takes left, middle or right");
			}
			action.numbers[0] = words[1] == "left" ? 1 : words[1] == "middle" ? 2 : 3;
		}
		else if (verb == "save-atlas" || verb == "save-hud")
		{
			action.kind = verb == "save-atlas" ? Action::Kind::SaveAtlas : Action::Kind::SaveHud;
			action.text = std::string(rest);
			if (action.text.empty())
			{
				return fail(std::format("{} takes a file", verb));
			}
		}
		else if (verb == "hold-check")
		{
			action.kind = Action::Kind::HoldCheck;
			if (!numbers(1) || action.numbers[0] <= 0)
			{
				return fail("hold-check takes a number of seconds");
			}
		}
		else if (verb == "floor" || verb == "status" || verb == "quit")
		{
			action.kind = verb == "floor" ? Action::Kind::Floor : verb == "status" ? Action::Kind::Status : Action::Kind::Quit;
			if (words.size() != 1)
			{
				return fail(std::format("{} takes nothing", verb));
			}
		}
		else
		{
			return fail("unknown action");
		}
		actions.push_back(std::move(action));
	}
	return actions;
}
}
```

- [x] **Step 6: Write `tools/mclink/Main.cpp`**

```cpp
// mclink: a console stand-in for Cyberpunk's side of SkyCraft's protocol, for spike 4's step 1. It runs BlockRender's
// link code against the real Minecraft, without the game (docs/superpowers/specs/2026-10-02-minecraft-link-design.md).
//
//     mclink.exe <script.txt>
//
// It runs the script (McLinkScript.h) at 60 Hz with Minecraft's player pinned where the script says, and reports
// what Minecraft sends. It refuses to start while another game side uses the mapping.
#include "McLinkScript.h"

#include "Coords.h"
#include "link/Link.h"
#include "link/Mapping.h"
#include "log/DdsWriter.h"

#include <Windows.h>
#include <timeapi.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <format>
#include <fstream>
#include <sstream>
#include <thread>

using namespace cpmc;
using namespace cpmc::link;
using cpmc::mclink::Action;

namespace
{
using Clock = std::chrono::steady_clock;
constexpr double kFrame = 1.0 / 60.0;
constexpr double kLongest = 600.0;  // seconds: no run goes longer

const Clock::time_point g_start = Clock::now();

double Now()
{
	return std::chrono::duration<double>(Clock::now() - g_start).count();
}

template<class... Args>
void Say(std::format_string<Args...> aFormat, Args&&... aArgs)
{
	std::printf("[%7.2f] %s\n", Now(), std::format(aFormat, std::forward<Args>(aArgs)...).c_str());
	std::fflush(stdout);
}

bool WriteDds(const std::string& aPath, uint32_t aWidth, uint32_t aHeight, const uint8_t* aRgba)
{
	constexpr uint32_t kRgba8 = 28;  // DXGI_FORMAT_R8G8B8A8_UNORM
	const auto         bytes = MakeDds(kRgba8, aWidth, aHeight, aRgba, size_t(aWidth) * 4);
	std::ofstream      out(aPath, std::ios::binary);
	out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
	return bool(out);
}

const char* TypeName(size_t aType)
{
	static const char* names[Link::kTypes] = { "pad",    "atlas",  "section", "clear-all", "texture", "avatar", "scene",
		                                       "atlas patch", "lights", "ragdoll", "solids",    "dug",     "other" };
	return names[std::min(aType, Link::kTypes - 1)];
}

// What Minecraft sends, reported as it comes.
struct Report : LinkSink
{
	void Atlas(const AtlasImage& aAtlas) override
	{
		atlasWidth = aAtlas.width;
		atlasHeight = aAtlas.height;
		atlas.assign(aAtlas.rgba.begin(), aAtlas.rgba.end());
		Say("atlas: {} x {}, {:.1f} MB", aAtlas.width, aAtlas.height, double(aAtlas.rgba.size()) / 1048576.0);
	}

	void Section(SectionMesh&& aMesh) override
	{
		++sections;
		if (sections <= 20 || sections % 100 == 0)
		{
			Describe(aMesh);
		}
	}

	void ClearAll() override { Say("clear all"); }

	void Explosion(const proto::McEvent& aEvent) override
	{
		Say("explosion at {:.1f} {:.1f} {:.1f}, radius {:.1f}", aEvent.a, aEvent.b, aEvent.c, aEvent.d);
	}

	void Hud(const HudFrame& aFrame) override
	{
		++huds;
		if (aFrame.width != hudWidth || aFrame.height != hudHeight || aFrame.bottomUp != hudBottomUp)
		{
			Say("HUD images: {} x {}, rows {}", aFrame.width, aFrame.height, aFrame.bottomUp ? "bottom-up" : "top-down");
		}
		hudWidth = aFrame.width;
		hudHeight = aFrame.height;
		hudBottomUp = aFrame.bottomUp;
		if (wantHud)
		{
			hud.assign(aFrame.pixels, aFrame.pixels + size_t(aFrame.width) * aFrame.height * 4);
			wantHud = false;
			hudReady = true;
		}
	}

	void Log(const std::string& aLine) override { Say("{}", aLine); }

	// The section's blocks, as the extent of its vertices, and the atlas texel at the centre of its first triangle's UVs:
	// inside that face's sprite, where a corner's UV can land on the next sprite over.
	void Describe(const SectionMesh& aMesh) const
	{
		if (aMesh.vertices.empty())
		{
			Say("section {} {} {}: removed", aMesh.x, aMesh.y, aMesh.z);
			return;
		}
		float low[3] = { 1e9f, 1e9f, 1e9f };
		float high[3] = { -1e9f, -1e9f, -1e9f };
		for (const Vertex& v : aMesh.vertices)
		{
			const float at[3] = { v.x, v.y, v.z };
			for (int i = 0; i < 3; ++i)
			{
				low[i] = std::min(low[i], at[i]);
				high[i] = std::max(high[i], at[i]);
			}
		}
		std::string sample;
		if (!atlas.empty())
		{
			const float    u = (aMesh.vertices[0].u + aMesh.vertices[1].u + aMesh.vertices[2].u) / 3.0f;
			const float    v = (aMesh.vertices[0].v + aMesh.vertices[1].v + aMesh.vertices[2].v) / 3.0f;
			const uint32_t x = std::min(uint32_t(std::clamp(u, 0.0f, 1.0f) * atlasWidth), atlasWidth - 1);
			const uint32_t y = std::min(uint32_t(std::clamp(v, 0.0f, 1.0f) * atlasHeight), atlasHeight - 1);
			const uint8_t* texel = &atlas[(size_t(y) * atlasWidth + x) * 4];
			sample = std::format("; its first triangle's texel ({}, {}) is rgba({}, {}, {}, {})", x, y, texel[0], texel[1], texel[2], texel[3]);
		}
		const std::string dropped = aMesh.translucentDropped ? std::format(" ({} translucent triangles dropped)", aMesh.translucentDropped) : "";
		Say("section {} {} {}: {} vertices{}, blocks x {:.2f} to {:.2f}, y {:.2f} to {:.2f}, z {:.2f} to {:.2f}{}", aMesh.x, aMesh.y, aMesh.z,
		    aMesh.vertices.size(), dropped, aMesh.x * 16.0 + low[0], aMesh.x * 16.0 + high[0], aMesh.y * 16.0 + low[1],
		    aMesh.y * 16.0 + high[1], aMesh.z * 16.0 + low[2], aMesh.z * 16.0 + high[2], sample);
	}

	std::vector<uint8_t> atlas;
	uint32_t             atlasWidth = 0;
	uint32_t             atlasHeight = 0;
	uint64_t             sections = 0;
	uint64_t             huds = 0;
	uint32_t             hudWidth = 0;
	uint32_t             hudHeight = 0;
	bool                 hudBottomUp = false;
	bool                 wantHud = false;
	bool                 hudReady = false;
	std::vector<uint8_t> hud;
};

void PrintStatus(const Link::Status& aStatus, const Report& aReport)
{
	const auto& s = aStatus;
	Say("Minecraft: {}{}{}, feet {:.3f} {:.3f} {:.3f}, frame {}", s.minecraft ? std::format("process {}", s.minecraftPid) : "absent",
	    s.inWorld ? ", in its world" : "", s.screenOpen ? ", a screen open" : "", s.minecraftFeet.x, s.minecraftFeet.y, s.minecraftFeet.z,
	    s.minecraftFrames);
	Say("game side: {}, {} teleports (sequence {}) to {:.3f} {:.3f} {:.3f}, look {:.1f} {:.1f}, floor {} {}", s.inGame ? "in game" : "not in game",
	    s.teleports, s.teleportSeq, s.target.x, s.target.y, s.target.z, s.look.yaw, s.look.pitch, s.floorEpoch,
	    s.floorSent ? std::format("at height {:.2f}", s.floorCentre.y) : "not sent");
	std::string types;
	for (size_t type = 0; type < Link::kTypes; ++type)
	{
		if (s.messages[type])
		{
			types += std::format("{}{} {} ({:.2f} MB)", types.empty() ? "" : ", ", s.messages[type], TypeName(type), double(s.bytes[type]) / 1048576.0);
		}
	}
	Say("render ring: {:.2f} MB; {}", double(s.drainedBytes) / 1048576.0, types.empty() ? "no messages" : types);
	Say("render ring: {} resyncs{}, {} rejected{}, {} sections before the atlas, {} translucent triangles dropped, {} sections reported",
	    s.resyncs, s.resyncs ? " (" + s.lastResync + ")" : "", s.rejected, s.rejected ? " (" + s.lastRejected + ")" : "", s.sectionsBeforeAtlas,
	    s.translucentDropped, aReport.sections);
	Say("events: {} explosions, {} others; {} inputs dropped; {} commands waiting; {} HUD images, {} refused", s.explosions, s.otherEvents,
	    s.inputsDropped, s.commandsWaiting, s.hudImages, s.hudRefused);
}
}

int main(int aArgc, char** aArgv)
{
	if (aArgc != 2)
	{
		std::puts("usage: mclink <script.txt>");
		return 2;
	}
	std::ifstream in(aArgv[1], std::ios::binary);
	if (!in)
	{
		std::printf("can't read %s\n", aArgv[1]);
		return 2;
	}
	std::stringstream text;
	text << in.rdbuf();
	std::string error;
	const auto  script = mclink::ParseScript(text.str(), error);
	if (!script)
	{
		std::printf("%s: %s\n", aArgv[1], error.c_str());
		return 2;
	}

	Mapping    mapping;
	const auto opened = mapping.Open(proto::kMappingName, GetCurrentProcessId(), GetTickCount64());
	if (opened.outcome != Mapping::Outcome::Created && opened.outcome != Mapping::Outcome::Reused)
	{
		Say("the mapping: {}", opened.why);
		return 1;
	}
	Say("the mapping: {} (process {}); waiting for Minecraft", opened.why, GetCurrentProcessId());

	timeBeginPeriod(1);  // 1 ms sleeps, for an even 60 Hz
	Link         link(mapping.Base());
	Report       report;
	LinkSettings settings;
	settings.viewportWidth = 1280;
	settings.viewportHeight = 720;
	Vec3d  feet{ -119.5, 6.14, 2007.5 };  // where V stood at spike 3's corner
	McLook look{ 180.0f, 0.0f };          // north
	bool   linked = false;
	bool   build = true;
	bool   hud = true;

	size_t   next = 0;
	bool     begun = false;
	double   begunAt = 0.0;
	double   stray = 0.0;
	uint64_t holdFrames = 0;
	int      exitCode = 0;
	double   lastReport = 0.0;
	uint64_t reportedFrames = 0;
	uint64_t reportedBytes = 0;
	uint64_t reportedHuds = 0;
	for (double tick = Now();; tick += kFrame)
	{
		const double now = Now();
		if (now > kLongest)
		{
			Say("stopping: no run goes over {} s", kLongest);
			exitCode = 1;
			break;
		}
		Link::Frame frame;
		frame.now = now;
		frame.tickMs = GetTickCount64();
		frame.linked = linked;
		frame.buildMode = build;
		frame.takeHud = hud;
		frame.pose = GamePose{ WorldFromMc(feet), WorldFromMc({ feet.x, feet.y + kEyeHeight, feet.z }), ForwardFromLook(look) };
		link.Update(frame, settings, report);
		const auto& state = link.State();

		if (now - lastReport >= 5.0)
		{
			const double seconds = lastReport == 0.0 ? 1.0 : now - lastReport;
			Say("Minecraft {}: {:.0f} frames/s; the render ring {:.2f} MB/s; {} sections so far; {:.0f} HUD images/s",
			    state.minecraft ? "there" : "absent", double(state.minecraftFrames - reportedFrames) / seconds,
			    double(state.drainedBytes - reportedBytes) / 1048576.0 / seconds, report.sections, double(report.huds - reportedHuds) / seconds);
			lastReport = now;
			reportedFrames = state.minecraftFrames;
			reportedBytes = state.drainedBytes;
			reportedHuds = report.huds;
		}

		if (next >= script->size())
		{
			break;
		}
		const Action& action = (*script)[next];
		const double  elapsed = begun ? now - begunAt : 0.0;
		bool          done = false;
		if (!begun)
		{
			begun = true;
			begunAt = now;
			switch (action.kind)
			{
			case Action::Kind::Link: linked = action.on; break;
			case Action::Kind::Build: build = action.on; break;
			case Action::Kind::Hud: hud = action.on; break;
			case Action::Kind::At: feet = { action.numbers[0], action.numbers[1], action.numbers[2] }; break;
			case Action::Kind::Look: look = { float(action.numbers[0]), float(action.numbers[1]) }; break;
			case Action::Kind::Floor: link.RequestFloor(); break;
			case Action::Kind::Status: PrintStatus(state, report); break;
			case Action::Kind::Type:
				if (!link.Type(action.text, error))
				{
					Say("line {}: {}", action.line, error);
					exitCode = 1;
				}
				break;
			case Action::Kind::Key:
				link.Push({ proto::kInKey, uint16_t(action.numbers[0]), 1, 0, 0 });
				link.Push({ proto::kInKey, uint16_t(action.numbers[0]), 0, 0, 0 });
				break;
			case Action::Kind::Click:
				link.Push({ proto::kInMouseButton, uint16_t(action.numbers[0]), 1, 0, 0 });
				link.Push({ proto::kInMouseButton, uint16_t(action.numbers[0]), 0, 0, 0 });
				break;
			case Action::Kind::SaveHud:
				report.wantHud = true;
				report.hudReady = false;
				break;
			case Action::Kind::HoldCheck:
				stray = 0.0;
				holdFrames = 0;
				break;
			default: break;
			}
		}
		switch (action.kind)
		{
		case Action::Kind::Wait: done = elapsed >= action.numbers[0]; break;
		case Action::Kind::WaitWorld:
			done = state.inWorld;
			if (!done && elapsed > action.numbers[0])
			{
				Say("line {}: Minecraft wasn't in its world within {} s", action.line, action.numbers[0]);
				exitCode = 1;
				next = script->size();
			}
			break;
		case Action::Kind::Type:
			done = elapsed > 0.0 && state.commandsWaiting == 0;
			if (!done && elapsed > 10.0)
			{
				Say("line {}: still typing after 10 s", action.line);
				exitCode = 1;
				done = true;
			}
			break;
		case Action::Kind::SaveAtlas:
			done = !report.atlas.empty();
			if (done)
			{
				Say("atlas saved to {}: {}", action.text,
				    WriteDds(action.text, report.atlasWidth, report.atlasHeight, report.atlas.data()) ? "ok" : "FAILED");
			}
			else if (elapsed > 30.0)
			{
				Say("line {}: no atlas within 30 s", action.line);
				exitCode = 1;
				done = true;
			}
			break;
		case Action::Kind::SaveHud:
			done = report.hudReady;
			if (done)
			{
				const uint32_t       w = report.hudWidth;
				const uint32_t       h = report.hudHeight;
				std::vector<uint8_t> rows(report.hud.size());
				for (uint32_t y = 0; y < h; ++y)
				{
					const uint32_t from = report.hudBottomUp ? h - 1 - y : y;
					std::copy_n(report.hud.data() + size_t(from) * w * 4, size_t(w) * 4, rows.data() + size_t(y) * w * 4);
				}
				uint64_t above = 0;
				uint64_t covered = 0;
				for (size_t i = 0; i < rows.size(); i += 4)
				{
					above += std::max({ rows[i], rows[i + 1], rows[i + 2] }) > rows[i + 3];
					covered += rows[i + 3] != 0;
				}
				Say("HUD image saved to {} ({} x {}, top row first): {}; {} pixels not transparent, {} with a colour above their alpha", action.text,
				    w, h, WriteDds(action.text, w, h, rows.data()) ? "ok" : "FAILED", covered, above);
			}
			else if (elapsed > 5.0)
			{
				Say("line {}: no HUD image within 5 s", action.line);
				exitCode = 1;
				done = true;
			}
			break;
		case Action::Kind::HoldCheck:
			if (state.inGame && state.inWorld)
			{
				const Vec3d& a = state.minecraftFeet;
				const Vec3d& b = state.target;
				stray = std::max(stray, std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)));
				++holdFrames;
			}
			done = elapsed >= action.numbers[0];
			if (done)
			{
				Say("over {} s ({} frames in game), Minecraft's feet strayed at most {:.4f} blocks from the teleport target", action.numbers[0],
				    holdFrames, stray);
			}
			break;
		case Action::Kind::Quit: next = script->size(); break;
		default: done = true; break;
		}
		if (done)
		{
			++next;
			begun = false;
		}
		std::this_thread::sleep_until(g_start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(tick + kFrame)));
	}
	PrintStatus(link.State(), report);
	timeEndPeriod(1);
	return exitCode;
}
```

- [x] **Step 7: Write `tools/mclink/step1.txt`**

```text
# Spike 4's step 1 (the spec's "Step 1"): BlockRender's link code against SkyCraft's mod, without Cyberpunk.
# Start Minecraft first; it links within a second of this starting, then opens (or creates) its SkyCraft world.
status
wait-world 180
link on
wait 2
# The pinned player: after the teleport, Minecraft's feet stay at the target.
hold-check 10
save-atlas atlas.dds
# Typed commands: a stone block 2 east of the feet, in section -8 0 125.
type creative
type /setblock ~2 ~ ~ minecraft:stone
wait 1
# A click on the floor: stone in the first slot, slot 1 picked, then a right click looking 60 degrees down.
type /item replace entity @s hotbar.0 with minecraft:stone
key 30
look 180 60
wait 0.5
click right
wait 1
save-hud hud.dds
# TNT on the stone (only the player collides with the floor, so it needs a block under it): an explosion event, and
# the section again, now empty.
type /summon tnt ~2 ~1 ~
wait 6
# A block that stays, for the second run to find: gold, 6 west of the feet, in the same section.
type /setblock ~-6 ~ ~ minecraft:gold_block
wait 1
# Unlinking holds Minecraft's player; linking again teleports it back.
link off
wait 2
link on
wait 2
status
quit
```

- [x] **Step 8: Write `tools/mclink/again.txt`**

```text
# Step 1, again: a new game side. Minecraft must send everything again: its clear, its atlas, and its sections.
wait-world 30
link on
wait 3
status
quit
```

- [x] **Step 9: Edit `CMakeLists.txt` to add the script parser to the tests, and mclink**

```cmake
	tests/TestMcLinkScript.cpp
)
```
```cmake
	tests/TestMcLinkScript.cpp
	tools/mclink/McLinkScript.cpp
)
```
```cmake
# Unit tests: no game needed. They run in this folder, so tests can find shaders/.
```
```cmake
# mclink: a console stand-in for the game's side of SkyCraft's protocol, run against the real Minecraft.
add_executable(mclink
	tools/mclink/Main.cpp
	tools/mclink/McLinkScript.cpp
)
target_include_directories(mclink PRIVATE tools/mclink)
target_link_libraries(mclink PRIVATE render_core winmm)

# Unit tests: no game needed. They run in this folder, so tests can find shaders/.
```

- [x] **Step 10: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `175 test(s), 0 failure(s)`.

- [x] **Step 11: Run mclink without Minecraft; it must start and stop cleanly**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && printf 'status\nquit\n' > build/selfcheck.txt \
  && ./build/RelWithDebInfo/mclink.exe build/selfcheck.txt > build/selfcheck.log; echo "exit: $?"; head -2 build/selfcheck.log
```
Expected: `exit: 0`, then lines like these (the process ID varies):
```
[   0.00] the mapping: created (process 12345); waiting for Minecraft
[   0.00] Minecraft: absent, feet 0.000 0.000 0.000, frame 0
```

- [x] **Step 12: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: mclink, a stand-in for the game's side" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 11: In Minecraft: step 1

Step 1 of the spec's "The steps", with the user: BlockRender's link code, run by `mclink`, against SkyCraft's unchanged
mod in the user's Modrinth instance, with no Cyberpunk.
- **The instance:** `CyberCraft`, at `C:\Users\BLOXY\AppData\Roaming\ModrinthApp\profiles\CyberCraft`. On 2026-10-02 it
  had Minecraft 26.3 and Fabric Loader 0.19.5, no mods, chat on T, and a GUI scale of 3.
- **The download:** the user approved downloading SkyCraft on 2026-10-02.
- **If Minecraft doesn't link,** its log says why, in lines starting `SkyCraft:`: a protocol mismatch, or access
  denied.
- **If a check fails,** stop and find the cause with superpowers:systematic-debugging before going on. Fixes become new
  tasks, as in spike 3.

**Files:**
- Create: `docs/spikes/2026-10-02-minecraft-link-results.md`

- [x] **Step 1: Put SkyCraft's jar in the instance**

```bash
M="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/mods" \
  && curl -fsSL -o "$M/skycraft-fabric-0.1.2.jar" https://github.com/chasmlol/SkyCraft/releases/download/v0.1.2/skycraft-fabric-0.1.2.jar \
  && sha256sum "$M/skycraft-fabric-0.1.2.jar" && wc -c < "$M/skycraft-fabric-0.1.2.jar"
```
Expected: `ea429c73dcec49d39b6a0bb9b17ad08a6b0290a39a96ccf6f6c07d581cf992d9`, then `247869`.

- [x] **Step 2: Ask the user to finish the instance and start Minecraft**

In the Modrinth App, the user:
1. adds Fabric API to the instance (its Content tab): the newest for 26.3, which was 0.161.0+26.3 on 2026-10-02.
   SkyCraft takes any version;
2. sets the instance's Java arguments to `--enable-native-access=ALL-UNNAMED -Dskycraft.quitWithSkyrim=false` (its
   settings, Java and memory);
3. starts the instance, and says when Minecraft reaches its title screen.

Its window shows until the link comes up, then SkyCraft hides it.

- [x] **Step 3: Check that SkyCraft loaded**

```bash
L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log" \
  && grep -E "Loading [0-9]+ mods|- (skycraft|fabric-api) |SkyCraft:" "$L" | head -12
```
Expected:
- the mods loaded, with `skycraft 0.1.2` and `fabric-api …+26.3` among them;
- `SkyCraft: can't open Skyrim's shared memory yet (Windows error 2: Skyrim hasn't created it yet)`: waiting for a
  game side.

- [x] **Step 4: Run step 1's script**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/step1 && cd build/step1 \
  && ../RelWithDebInfo/mclink.exe ../../tools/mclink/step1.txt | tee step1.log; echo "exit: ${PIPESTATUS[0]}"
```
It takes about a minute once Minecraft is in its world, and up to four if Minecraft creates its world first: give the
command 5 minutes, more than the shell's default timeout of 2.

Expected, against the spec's "Step 1":
- **The link:** `Minecraft is there (process …)`. Minecraft's log shows `SkyCraft: linked to Skyrim (pid …)`,
  `SkyCraft: game window hidden …`, and `SkyCraft: creating mirror world` the first time, or
  `SkyCraft: opening mirror world` after.
- **The pinned player:** `Minecraft's feet strayed at most …` no more than 0.01 blocks.
- **The atlas:** `atlas: W x H, … MB`, about 6 to 13 MB, saved to `atlas.dds`.
- **Typed commands:**
  - `typed:` for `/gamemode creative` and the `/setblock`;
  - then `section -8 0 125` with blocks from x -118.00 to -117.00, y 6.00 to 7.00 and z 2007.00 to 2008.00: the stone,
    2 east of the feet;
  - its first triangle's texel is stone's: opaque (alpha 255) and grey, with r, g and b equal or nearly so.
- **The click:** after it, `section -8 0 125` again, now also covering x -120 to -119, y 6 to 7 and z 2006 to 2007:
  the block placed on the floor where the crosshair met it.
- **The HUD image:** `HUD images: 1280 x 720, rows bottom-up`. It's saved to `hud.dds`, with `0 with a colour above
  their alpha`.
- **TNT:** it lands on the stone, then `explosion at -117.5 7.1 2007.5, radius 4.0` (TNT explodes 0.06 above its
  feet), and `section -8 0 125: removed`: the stone and the clicked block are gone, and nothing else was in it.
- **A block that stays:** `typed:` for the gold block's `/setblock`, then `section -8 0 125` with blocks from
  x -126.00 to -125.00, y 6.00 to 7.00 and z 2007.00 to 2008.00.
- **Unlinking and linking again:** the last status is in game, with at least 2 teleports.
- **Traffic:** the lines every 5 seconds show Minecraft's frames a second near 60 (paced to `mclink`), the render
  ring's MB a second, and HUD images a second.
- `exit: 0`.

- [x] **Step 5: Look at the atlas and the HUD image**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && uv run --no-project -q --with numpy --with pillow python tools/view_dump.py build/step1
```
Expected: `atlas.dds` and `hud.dds` converted, both `R8G8B8A8_UNORM`. Read `build/step1/atlas.png` and
`build/step1/hud.png`, and check:
- **`atlas.png`:** Minecraft's block textures, with its item textures below them;
- **`hud.png`:** the right way up, with the hotbar (stone in the first slot, which is selected), the crosshair in
  white, and chat showing the commands' replies.

- [x] **Step 6: Run it again, as a new game side**

Wait 10 seconds after Step 4. Minecraft drops the link after 8 s, and keeps running, since it was told not to quit
with the game side. Then:
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render/build/step1 \
  && ../RelWithDebInfo/mclink.exe ../../tools/mclink/again.txt | tee again.log; echo "exit: ${PIPESTATUS[0]}"
```
Expected:
- `Minecraft is there`, then `clear all`, `atlas: …`, and `section -8 0 125` with the gold block again (x -126.00 to
  -125.00): Minecraft sends everything again to a new game side, but only sections with blocks in them;
- HUD images again;
- `exit: 0`.

Then check Minecraft's side of it:
```bash
grep -E "SkyCraft: (Skyrim link|linked to|Skyrim instance changed)" "/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"
```
Expected, in this order:
- `linked to Skyrim (pid …)` and `Skyrim link up`, for Step 4;
- `Skyrim link down`, about 8 s after Step 4 ended;
- `Skyrim instance changed (pid …)` with this run's process, and `Skyrim link up`.

- [x] **Step 7: Ask the user to stop Minecraft**

Its window is hidden, so the user stops it from the Modrinth App.

- [x] **Step 8: Write step 1's results, `docs/spikes/2026-10-02-minecraft-link-results.md`**

From `build/step1/step1.log`, `build/step1/again.log`, the two PNGs and Minecraft's log, as spike 3's results doc
does:
- the versions;
- each check above with its outcome and numbers: the stray, the atlas's size, Minecraft's frames a second, the render
  ring's MB a second and HUD images a second. Latency is step 2's, measured in the game;
- anything that surprised, and what it means for Part 2.

- [x] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-02-minecraft-link-results.md \
  && git commit -q -m "minecraft-link: step 1's results" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 12: Close Part 1, and plan Part 2

- [x] **Step 1: Add "How Part 1 went" to this plan**

At the end, after "Changes from the deep review": what ran as written, what changed, and what step 1 found.

- [x] **Step 2: Plan Part 2**

Use superpowers:writing-plans to add Part 2's tasks, from Task 13 on, to this plan: step 2 in the game. Verify them as
Part 1's were. Changes to the spec found while planning go in its "Changes while planning".

- [x] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/superpowers \
  && git commit -q -m "minecraft-link plan: Part 1 closed, Part 2 planned" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

## Changes from the deep review

On 2026-10-02 the user asked for a deep review of this plan. It read every task against the spec, SkyCraft's protocol
header, and SkyCraft's Java and Skyrim sides at `bfcaf17`, then replayed the changed plan in a fresh clone. It
changed:
- **The overlay's reset (Task 9):** the link started the HUD image's triple buffer over whenever Minecraft appeared,
  as SkyCraft's `Game.cpp` does. But SkyCraft ships an image left from before as soon as it relinks, which can be
  before the link first sees it. And a Minecraft coming back after a stall keeps its slots. A reset then leaves
  Minecraft's writer and the link's reader on one slot, with torn images from then on. Task 11's second run
  (`again.txt`) could have hit it. The link now resets only for a new Minecraft process. Three tests pin it: an image
  published before the link saw Minecraft, Minecraft coming back, and a new Minecraft.
- **Ring problems logged once a second (Task 9):** the spec's "Safety" asks for it. The link logged each resync at
  once, which a broken stream would repeat every frame, and never logged rejected messages. Both are now logged at
  most once a second, with a count, and a test pins it.
- **Sampling the atlas (Task 10):** `mclink` reported the texel at a section's first vertex's UV. That's a sprite's
  corner, and the far corners land on the next sprite over. It now samples the centre of the first triangle's UVs,
  so Task 11 can check that the stone's texel is stone's: opaque grey.
- **Step 1's second run (Tasks 10 and 11):** Step 4's TNT destroys both of its blocks, and SkyCraft resends only
  sections with blocks in them (`resendEverything`). So the second run would have found no section at all, where
  Step 6 expected Step 4's blocks. `step1.txt` now places a gold block after the explosion, out of its reach, for the
  second run to find, and Step 4 expects the TNT's section to come as removed.
- **Task 11:** step 1's run can take up to four minutes, over the shell's default timeout, so it says to allow five.
  Fabric API isn't pinned to one version, since SkyCraft's jar takes any. The explosion prints at y 7.1, not 7.0:
  TNT explodes 0.06 above its feet.
- **Smaller:**
  - Task 1's branch check expects the plan's commits at the top;
  - `Link.h` says which calls may come from other threads: only `Push`;
  - the spec's "Safety" now agrees with its `mclink` section: `mclink` refuses to start, rather than retrying.

It also checked these against SkyCraft's code, and found them right:
- **The floor:** SkyCraft stores a region's triangles as they arrive (`readTris`). Only voxel regions mark a region
  known (`readRegion`). So the triangle-only floor is clickable, the player stays pinned (`holdUntilReady`), and
  explosions stay vanilla, since `SkyDigBlast` needs known regions.
- **The rings:** SkyCraft's render ring writer pads and limits its messages as the reader expects (`writeRender`). Its
  event writer drops events rather than lapping the reader (`pushEvent`).
- **Sections:** `WorldExporter`'s vertex is x, y, z, u, v, then r, g, b, a, the light and the flags, as the tests
  write it.
- **Task 11's expectations:**
  - SkyCraft's log lines: `linked to Skyrim`, `Skyrim link up` and `down`, `Skyrim instance changed`, `creating` or
    `opening mirror world`, and `game window hidden`;
  - SkyCraft opens its world while the game side isn't in game (`MirrorWorld`), so `wait-world` before `link on`
    works;
  - its teleports wait for the in-game flag, so nothing moves before linking;
  - a click's hit normal faces the ray (`SkyRay`), so the block goes on top of the floor, at y 6;
  - vanilla TNT breaks the stone under it, and the clicked block;
  - `fabric.mod.json` needs Fabric Loader 0.19.5 or newer and Java 25, which the instance has.

---

## How Part 1 went

On 2026-10-02, inline, as the user asked.
- **Tasks 1 to 10 ran as written,** in the repo, by the same runner that replayed them (`tools/plans/run_plan.py`,
  execute mode). Task 1's header and licence were downloaded with the plan's `curl`; the runner did the rest. Every
  build that had to fail failed with its missing header. Every test count matched, from 112 to 175, with no warnings,
  and `mclink`'s self-check found Minecraft absent. The ten commits run from `5e8fde3` to `e5d0d66`, and their 35
  files are identical to the verified scratch copy's.
- **Task 11 ran as written,** with the user: [step 1's results](../../spikes/2026-10-02-minecraft-link-results.md).
  Every check passed, and nothing in the code changed. Two expectations were wrong, and the plan said to stop and find
  why, which a third, diagnostic run settled:
  - **the atlas** is 2048 × 2576, 20.1 MB, not 6 to 13 MB: Minecraft 26.3's block atlas is a full 2048 × 2048;
  - **the HUD image** has one pixel with colour and no alpha, at an edge of Minecraft's first-person shield. The same
    view without the shield has none, and nothing else in the image breaks the premultiplied rule.
- **What step 1 found, for Part 2** (the results doc has the numbers):
  - BlockRender's link code works against SkyCraft's real mod as written. Its latency, frame cost and the HUD over
    Night City are step 2's to measure, in the game;
  - the HUD image includes Minecraft's first-person view, its hands and held items, as the spec's "The HUD" expects.
    Step 2 shows how they look over V's own view;
  - the HUD image is premultiplied but for one texel, so the HUD pass keeps premultiplied blending, and risk 13's
    fallback to straight alpha doesn't apply;
  - the atlas Part 2 uploads is 20 MB, once per link;
  - animated textures fill the render ring with about 8 MB a second of atlas patches, which the link steps over;
  - SkyCraft gives a new world's player starter and builder kits, with recipe toasts in the HUD, and connects to
    Discord for Rich Presence.

---

## Part 2: step 2, the link in the game

Tasks 13 to 29, planned after Part 1 closed (see "How Part 1 went"), from step 1's findings. Task 28 came later,
from what step 2's run found.

**Goal:** BlockRender 0.9.0, linked in the game to SkyCraft's Minecraft. Minecraft's player follows V, Minecraft's
blocks are drawn into Night City's G-buffer with Minecraft's own atlas, and Minecraft's HUD is drawn over the finished
frame. Then run the spec's step 2 with the user: linking, the HUD, building by command, latency, bursts, frame rates,
focus, and Minecraft closed and reopened.

**How it works:**
- **The plugin runs Part 1's link once a frame** (Task 24), and hands what arrives to the renderer through the link's
  sink, on the main thread:
  - **the atlas** becomes an `Atlas` with 5 levels (Task 13), and replaces `atlas.bin`'s (Task 22). Spike 3's own
    world is cleared then, and isn't meshed again while Minecraft's atlas is loaded, so its removals can't undo
    Minecraft's sections;
  - **sections** wait by position, only the latest of each (Tasks 14 and 22), for the renderer's `Update` to upload
    them, as spike 3's did;
  - **HUD images** go into one of 3 upload buffers (Tasks 15 and 23). At Present, the HUD pass copies the newest into
    GPU memory and draws it over the finished frame, with premultiplied blending (Tasks 16 and 23).
- **Measurements** (Tasks 18 to 22): latency from a typed command's Enter to the first list that draws its section,
  bursts after an explosion or a command, `Update`'s time, the render ring's traffic and drain time, the HUD's copy
  and pass, and both processes' CPU and memory. The status gets 8 lines for them (Task 21).
- **Requests** (Task 17): `link on|off`, `floor`, `mc <set or command>`, and the new settings, which the plugin
  applies (Task 24). The overlay gets buttons for them (Task 25).
- **The game run** (Task 27) follows the spec's "Step 2", with a recorder for the status (Task 26).

**Already verified:** as in Part 1, all the code was compiled and tested in the scratch clone first. Tasks 13 to 26
were then replayed in a fresh clone from the end of Part 1 (`c42e122`), applying every step as written and checking
every expected result:
- zero compiler warnings at `/W4`;
- 203 C++ tests and 25 Python tests passing;
- `luac` accepting the overlay;
- the replayed files identical to the scratch clone's.

Task 28 was replayed the same way from the end of Task 26 (`a95b990`).

Tasks 22 to 24, the renderer's and the plugin's code, have no unit tests: only the game can test them, in Task 27.
The overlay's buttons were run once with CET stubbed out, writing the requests they should.

**The spec:** planning Part 2 corrected it in seven places, which its "Changes while planning" lists after Part 1's.
Among them: the mapping is made at the game's first update, not at load, and HUD pixels outside the image show
nothing.

**Test counts:** 175 at the start. Then 178, 181, 189, 190, 192, 196, 199, 201 and 203 after Tasks 13 to 21, 203 until
Task 28, and 204 after it. The Python count is spike 3's 22 until Task 26, then 25.

**The commands and rules** in "Before you start" apply here too. Part 2 also goes into the game:
- **The user does everything in the game,** as in Minecraft: launching it, loading saves, moving, pressing keys,
  clicking the overlay's buttons and taking screenshots. Never drive their mouse or keyboard: ask, then wait for them
  to say they're done.
- **Deploying:** never build with `CP2077_DIR` set, and never deploy, while the game runs.
  `tasklist | grep -i -c "cyberpunk2077.exe"` must print `0`. Set `CP2077_DIR` only to deploy, and clear it straight
  after.
- **No faults:** plugin code must never fault, because the game's crash handler turns any fault into a crash or a
  frozen game. If the game freezes, first read BlockRender's log and the newest folder in
  `%LOCALAPPDATA%\REDEngine\ReportQueue\`. Force-close the game only when the user asks.

## File map (Part 2)

| File | Responsibility |
|---|---|
| `src/render/Atlas.*` | Minecraft's atlas from its pixels, with 5 levels |
| `src/render/PendingSections.*` | The uploads waiting: the latest mesh for each section |
| `src/render/Hud.*` | The HUD's rules: when it shows, the size to ask for, the texel each pixel takes, and the 3 upload buffers |
| `shaders/blocks.hlsl`, `src/render/FrameConstants.h`, `src/render/Shaders.*` | The HUD's shaders and their constants |
| `src/Requests.*`, `src/link/Commands.*` | The `link`, `floor` and `mc` requests |
| `src/Measurements.*` | Latency and bursts, and the clock they share |
| `src/ProcessUsage.*` | CPU and memory, for Cyberpunk and for Minecraft |
| `src/link/Link.*`, `src/link/Overlay.h`, `tools/mclink/Main.cpp` | Minecraft's eye, the drain's time, and the render ring's type names |
| `src/Status.*` | The link's lines in `status.txt` |
| `src/render/Renderer.*` | Minecraft's atlas and sections, `Update`'s time, the timed upload, and the HUD pass |
| `src/Plugin.cpp` | The link once a frame, what it hands on, the requests and settings, and the status's link lines |
| `tools/blockrender_hud/init.lua` | The overlay's Link, Unlink, HUD and command buttons |
| `tools/status_watch.py`, `tools/test_status_watch.py` | Readings of the status over time, for the game run |
| `src/link/Look.*` | Minecraft's yaw, kept within 180° of the last (Task 28, from step 2's run) |
| `CMakeLists.txt` | The new sources and tests, and version 0.9.0 |
| `tests/TestPendingSections.cpp`, `tests/TestHud.cpp`, `tests/TestMeasurements.cpp`, `tests/TestProcessUsage.cpp` | One test file per new unit; the changed units' tests grow in their own files |

---

### Task 13: Minecraft's atlas, with its levels

Minecraft sends its combined atlas once a link, as RGBA8 rows, top first: 2048 × 2576 and 20.1 MB in step 1.
`AtlasFromRgba` makes it an `Atlas` with the 5 levels the shader finds (the spec's "The renderer and the atlas"). Each
level halves the last, rounding down to at least 1 texel, and each texel is the rounded average of the 2 × 2 above
it. An odd side's last row or column is left out, and a side of 1 is used twice. The atlas has no sprites or blocks,
since Minecraft's vertices carry their own UVs. The last test makes one of step 1's size, which pins the levels'
offsets the renderer uploads.

**Files:**
- Modify: `tests/TestAtlas.cpp`, `src/render/Atlas.h`, `src/render/Atlas.cpp`

- [x] **Step 1: Edit `tests/TestAtlas.cpp` to test atlases from Minecraft's pixels**

```cpp
	CHECK(!cpmc::ParseAtlas({}, error));
}
```
```cpp
	CHECK(!cpmc::ParseAtlas({}, error));
}

TEST(AtlasFromMinecraftsPixelsHalvesFiveTimes)
{
	// 5 x 3 texels, each with its own red: level 1 is 2 x 1, its last column and row of the level above unused, then
	// 1 x 1 three times, each the one row of the level above used twice.
	std::vector<uint8_t> rgba;
	for (uint8_t i = 0; i < 15; ++i)
	{
		rgba.insert(rgba.end(), { uint8_t(i * 10), 0, 0, 255 });
	}
	std::string error;
	const auto  atlas = cpmc::AtlasFromRgba(5, 3, rgba, error);
	CHECK(atlas.has_value());
	CHECK_EQ(atlas->width, 5u);
	CHECK_EQ(atlas->height, 3u);
	CHECK_EQ(atlas->Levels(), 5u);
	CHECK(atlas->levelOffsets == std::vector<uint32_t>({ 0, 15, 17, 18, 19 }));
	CHECK_EQ(atlas->texels.size(), size_t(20));
	CHECK_EQ(atlas->texels[6], 0xFF00003Cu);   // level 0 is the pixels: red 60 in the low byte
	CHECK_EQ(atlas->texels[15], 0xFF00001Eu);  // (0 + 10 + 50 + 60) / 4
	CHECK_EQ(atlas->texels[16], 0xFF000032u);  // (20 + 30 + 70 + 80) / 4
	CHECK_EQ(atlas->texels[17], 0xFF000028u);  // (30 + 50 + 30 + 50) / 4
	CHECK_EQ(atlas->texels[19], 0xFF000028u);
	CHECK(atlas->sprites.empty());
	CHECK(atlas->blocks.empty());
}

TEST(AtlasFromMinecraftsPixelsAveragesEachChannel)
{
	// One opaque white texel and three transparent black ones: a quarter of each, rounded.
	const std::vector<uint8_t> rgba = { 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	std::string                error;
	const auto                 atlas = cpmc::AtlasFromRgba(2, 2, rgba, error);
	CHECK_EQ(atlas->texels[4], 0x40404040u);
}

TEST(AtlasFromMinecraftsPixelsAtStepOnesSize)
{
	// Step 1's atlas: 2048 x 2576, five levels as the shader finds them.
	const std::vector<uint8_t> rgba(size_t(2048) * 2576 * 4, 0);
	std::string                error;
	const auto                 atlas = cpmc::AtlasFromRgba(2048, 2576, rgba, error);
	CHECK(atlas.has_value());
	CHECK(atlas->levelOffsets == std::vector<uint32_t>({ 0, 5'275'648, 6'594'560, 6'924'288, 7'006'720 }));
	CHECK_EQ(atlas->texels.size(), size_t(7'027'328));
	CHECK(!cpmc::AtlasFromRgba(2048, 2576, std::span(rgba).first(100), error));
	CHECK(error.find("needs 21102592 bytes") != std::string::npos);
	CHECK(!cpmc::AtlasFromRgba(0, 4, rgba, error));
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'AtlasFromRgba': is not a member of 'cpmc'`.

- [x] **Step 3: Edit `src/render/Atlas.h` to declare `AtlasFromRgba`**

```cpp
std::optional<Atlas> ParseAtlas(std::span<const uint8_t> aBytes, std::string& aError);
```
```cpp
std::optional<Atlas> ParseAtlas(std::span<const uint8_t> aBytes, std::string& aError);

constexpr uint32_t kMinecraftAtlasLevels = 5;

// Minecraft's atlas, as SkyCraft sends it (spike 4): RGBA8 rows, top first. Level 0 is the pixels as they are. Each
// further level halves the last, rounding down to at least 1 texel, each texel the rounded average of the 2 x 2 above
// it: an odd side's last row or column is left out, and a side of 1 is used twice. It has no sprites or blocks:
// Minecraft's vertices carry their own UVs. Nothing, with aError saying why, if a side is 0 or aRgba is short.
std::optional<Atlas> AtlasFromRgba(uint32_t aWidth, uint32_t aHeight, std::span<const uint8_t> aRgba, std::string& aError);
```

- [x] **Step 4: Edit `src/render/Atlas.cpp` to build the levels**

```cpp
	aError = std::move(aWhy);
	return std::nullopt;
}
```
```cpp
	aError = std::move(aWhy);
	return std::nullopt;
}

// Four RGBA8 texels averaged channel by channel, rounded to nearest.
uint32_t Average(uint32_t aA, uint32_t aB, uint32_t aC, uint32_t aD)
{
	uint32_t out = 0;
	for (uint32_t shift = 0; shift < 32; shift += 8)
	{
		const uint32_t sum = ((aA >> shift) & 0xFF) + ((aB >> shift) & 0xFF) + ((aC >> shift) & 0xFF) + ((aD >> shift) & 0xFF);
		out |= ((sum + 2) / 4) << shift;
	}
	return out;
}
```
```cpp
	return atlas;
}
}
```
```cpp
	return atlas;
}

std::optional<Atlas> AtlasFromRgba(uint32_t aWidth, uint32_t aHeight, std::span<const uint8_t> aRgba, std::string& aError)
{
	const uint64_t bytes = uint64_t(aWidth) * aHeight * 4;
	if (aWidth == 0 || aHeight == 0 || aRgba.size() < bytes)
	{
		return Fail(aError, std::format("a {} x {} atlas needs {} bytes of pixels, and has {}", aWidth, aHeight, bytes, aRgba.size()));
	}
	Atlas  atlas;
	size_t texelCount = 0;
	atlas.width = aWidth;
	atlas.height = aHeight;
	for (uint32_t level = 0; level < kMinecraftAtlasLevels; ++level)
	{
		atlas.levelOffsets.push_back(uint32_t(texelCount));
		texelCount += size_t(std::max(1u, aWidth >> level)) * std::max(1u, aHeight >> level);
	}
	atlas.texels.resize(texelCount);
	std::memcpy(atlas.texels.data(), aRgba.data(), size_t(bytes));
	for (uint32_t level = 1; level < kMinecraftAtlasLevels; ++level)
	{
		const uint32_t  aboveWidth = std::max(1u, aWidth >> (level - 1));
		const uint32_t  aboveHeight = std::max(1u, aHeight >> (level - 1));
		const uint32_t  width = std::max(1u, aWidth >> level);
		const uint32_t  height = std::max(1u, aHeight >> level);
		const uint32_t* above = atlas.texels.data() + atlas.levelOffsets[level - 1];
		uint32_t*       out = atlas.texels.data() + atlas.levelOffsets[level];
		for (uint32_t y = 0; y < height; ++y)
		{
			const size_t row0 = size_t(y * 2) * aboveWidth;
			const size_t row1 = size_t(std::min(y * 2 + 1, aboveHeight - 1)) * aboveWidth;
			for (uint32_t x = 0; x < width; ++x)
			{
				const uint32_t x0 = x * 2;
				const uint32_t x1 = std::min(x0 + 1, aboveWidth - 1);
				out[size_t(y) * width + x] = Average(above[row0 + x0], above[row0 + x1], above[row1 + x0], above[row1 + x1]);
			}
		}
	}
	return atlas;
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
Expected: no error or warning lines, then `178 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: Minecraft's atlas, with its levels" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 14: Waiting uploads keep only each section's latest

`Renderer::UploadSection` queues every mesh it's given, and `Update` uploads the queue. Until drawing is first
switched on, `Update` uploads nothing, so a linked Minecraft would pile up a copy of every change. `PendingSections`
keeps one mesh for each section, the latest, removals included (the spec's "The renderer and the atlas"). Task 22
puts it in the renderer.

**Files:**
- Create: `src/render/PendingSections.h`, `src/render/PendingSections.cpp`, `tests/TestPendingSections.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestPendingSections.cpp`**

```cpp
#include "Harness.h"

#include "render/PendingSections.h"

using namespace cpmc;

namespace
{
const SectionPos kNear{ -3, 4, 0 };
const SectionPos kFar{ 1, 0, 2 };

std::vector<Vertex> Mesh(size_t aVertices, float aX)
{
	std::vector<Vertex> vertices(aVertices);
	for (Vertex& vertex : vertices)
	{
		vertex.x = aX;
	}
	return vertices;
}
}

TEST(PendingSectionsKeepTheLatestMesh)
{
	PendingSections pending;
	pending.Put(kFar, Mesh(6, 1.0f));
	pending.Put(kFar, Mesh(12, 2.0f));  // Minecraft changed it again before the upload
	pending.Put(kNear, Mesh(3, 3.0f));
	CHECK_EQ(pending.Size(), size_t(2));
	const auto taken = pending.Take();
	CHECK_EQ(taken.size(), size_t(2));
	CHECK(taken[0].first == kNear);  // in section order
	CHECK(taken[1].first == kFar);
	CHECK_EQ(taken[1].second.size(), size_t(12));
	CHECK_EQ(taken[1].second[0].x, 2.0f);
	CHECK_EQ(pending.Size(), size_t(0));
	CHECK(pending.Take().empty());
}

TEST(PendingSectionsKeepARemoval)
{
	PendingSections pending;
	pending.Put(kFar, Mesh(6, 1.0f));
	pending.Put(kFar, {});  // then Minecraft removed it: the removal waits instead
	const auto taken = pending.Take();
	CHECK_EQ(taken.size(), size_t(1));
	CHECK(taken[0].second.empty());
}

TEST(PendingSectionsClear)
{
	PendingSections pending;
	pending.Put(kNear, Mesh(6, 1.0f));
	pending.Put(kFar, Mesh(6, 1.0f));
	pending.Clear();
	CHECK_EQ(pending.Size(), size_t(0));
	CHECK(pending.Take().empty());
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestMcLinkScript.cpp
	tools/mclink/McLinkScript.cpp
```
```cmake
	tests/TestMcLinkScript.cpp
	tests/TestPendingSections.cpp
	tools/mclink/McLinkScript.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'render/PendingSections.h'`.

- [x] **Step 4: Write `src/render/PendingSections.h`**

```cpp
#pragma once

#include "world/BlockWorld.h"
#include "world/Vertex.h"

#include <map>
#include <utility>
#include <vector>

// The renderer's waiting uploads (the spec's "The renderer and the atlas"): the latest mesh for each section. Until
// drawing is first switched on nothing is uploaded, and Minecraft may send a section many times before the next upload,
// so each section waits once, with its latest mesh. Not thread-safe: the renderer holds its lock.
namespace cpmc
{
class PendingSections
{
public:
	void Put(const SectionPos& aSection, std::vector<Vertex> aVertices);  // replaces whatever waits for it; none: remove it
	std::vector<std::pair<SectionPos, std::vector<Vertex>>> Take();        // everything waiting, in section order
	void   Clear();
	size_t Size() const { return waiting_.size(); }

private:
	std::map<SectionPos, std::vector<Vertex>> waiting_;
};
}
```

- [x] **Step 5: Write `src/render/PendingSections.cpp`**

```cpp
#include "render/PendingSections.h"

namespace cpmc
{
void PendingSections::Put(const SectionPos& aSection, std::vector<Vertex> aVertices)
{
	waiting_[aSection] = std::move(aVertices);
}

std::vector<std::pair<SectionPos, std::vector<Vertex>>> PendingSections::Take()
{
	std::vector<std::pair<SectionPos, std::vector<Vertex>>> taken;
	taken.reserve(waiting_.size());
	for (auto& [section, vertices] : waiting_)
	{
		taken.emplace_back(section, std::move(vertices));
	}
	waiting_.clear();
	return taken;
}

void PendingSections::Clear()
{
	waiting_.clear();
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Link.cpp
)
```
```cmake
	src/link/Link.cpp
	src/render/PendingSections.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `181 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: waiting uploads keep only each section's latest" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 15: The HUD's rules

The HUD pass's pure parts (the spec's "The HUD"), so the tests pin them:
- **`HudWanted`:** whether to take Minecraft's HUD images and draw the newest: the `hud` setting (0 never, 1 in build
  mode, 2 always), and only while Minecraft is in its world.
- **`HudViewport`:** the size to ask Minecraft for, `SkyState`'s viewport: the back buffer's size over `hudscale`
  (1, 2 or 4), at most an overlay slot's 3840 × 2160. 0 × 0 before the back buffer is known, which leaves Minecraft's
  window alone.
- **`HudScaleFor`:** the scale an image is drawn at, from its own size: as many whole back-buffer pixels a texel as fit.
  So an image Minecraft drew at the old size, before it caught up with a new `hudscale`, is still drawn whole.
- **`HudTexel`:** which texel a back-buffer pixel shows: scaled from the top left, and flipped when the rows are stored
  bottom-up. Outside the image it shows nothing, where the spec had the edge texel repeated (see the spec's "Changes
  while planning"). Task 16's pixel shader does the same arithmetic.
- **`HudSlots`:** which of the 3 upload buffers each image goes through. The main thread copies an image into a slot
  the GPU has finished with, where it waits as the newest. The HUD pass takes the newest, once, and the GPU reads it
  until its fence. An image never taken is replaced by the next, as in Minecraft's own triple buffer. If the GPU is
  still reading the other two, the next image replaces the one waiting.

**Files:**
- Create: `src/render/Hud.h`, `src/render/Hud.cpp`, `tests/TestHud.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestHud.cpp`**

```cpp
#include "Harness.h"

#include "render/Hud.h"

using namespace cpmc;

TEST(HudWantedFollowsTheSetting)
{
	CHECK(!HudWanted(0, true, true));  // never
	CHECK(HudWanted(1, true, true));   // in build mode, the default
	CHECK(!HudWanted(1, false, true));
	CHECK(HudWanted(2, false, true));  // always
	CHECK(!HudWanted(2, true, false));  // but only with Minecraft in its world
	CHECK(!HudWanted(3, true, true));
}

TEST(HudViewportIsTheBackBufferOverTheScale)
{
	const HudSize half = HudViewport(2560, 1440, 2);
	CHECK_EQ(half.width, 1280u);
	CHECK_EQ(half.height, 720u);
	CHECK_EQ(HudViewport(2560, 1440, 1).width, 2560u);
	CHECK_EQ(HudViewport(2560, 1440, 4).height, 360u);
	CHECK_EQ(HudViewport(1366, 768, 4).width, 341u);  // rounded down: the last 2 columns show nothing
	CHECK_EQ(HudViewport(7680, 4320, 1).width, 3840u);  // at most a slot's size
	CHECK_EQ(HudViewport(7680, 4320, 1).height, 2160u);
	CHECK_EQ(HudViewport(0, 0, 2).width, 0u);  // the back buffer isn't known yet: Minecraft's window is left alone
	CHECK_EQ(HudViewport(2560, 1440, 3).width, 0u);  // 1, 2 or 4 only
}

TEST(HudScaleComesFromTheImage)
{
	CHECK_EQ(HudScaleFor(2560, 1440, 1280, 720), 2u);
	CHECK_EQ(HudScaleFor(1366, 768, 341, 192), 4u);
	CHECK_EQ(HudScaleFor(2560, 1440, 2560, 1440), 1u);
	CHECK_EQ(HudScaleFor(2560, 1440, 1280, 1440), 1u);  // the smaller of the two ways
	CHECK_EQ(HudScaleFor(1280, 720, 2560, 1440), 1u);   // an image bigger than the screen: cut off, never shrunk
	CHECK_EQ(HudScaleFor(2560, 1440, 0, 0), 1u);
}

TEST(HudTexelScalesAndFlips)
{
	// A 4 x 2 image at scale 2, rows bottom-up: the top left screen pixels show the image's last row.
	CHECK_EQ(*HudTexel(0, 0, 4, 2, 2, true), 4u);
	CHECK_EQ(*HudTexel(1, 1, 4, 2, 2, true), 4u);
	CHECK_EQ(*HudTexel(2, 0, 4, 2, 2, true), 5u);
	CHECK_EQ(*HudTexel(7, 3, 4, 2, 2, true), 3u);
	CHECK_EQ(*HudTexel(7, 3, 4, 2, 2, false), 7u);  // top first: no flip
	CHECK_EQ(*HudTexel(3, 1, 4, 2, 1, false), 7u);  // scale 1
}

TEST(HudTexelShowsNothingOutsideTheImage)
{
	CHECK(!HudTexel(8, 0, 4, 2, 2, true).has_value());
	CHECK(!HudTexel(0, 4, 4, 2, 2, false).has_value());
	CHECK(!HudTexel(0, 0, 0, 0, 2, false).has_value());
	CHECK(!HudTexel(0, 0, 4, 2, 0, false).has_value());
}

TEST(HudSlotsWriteTakeAndRead)
{
	HudSlots slots;
	CHECK(!slots.TakeNewest().has_value());  // nothing written yet
	CHECK_EQ(*slots.BeginWrite(0), 0u);
	slots.EndWrite(0);
	CHECK_EQ(*slots.TakeNewest(), 0u);
	CHECK(!slots.TakeNewest().has_value());  // each image once
	slots.Reading(0, 5);
	CHECK_EQ(*slots.BeginWrite(4), 1u);  // slot 0 is the GPU's until fence 5
	slots.EndWrite(1);
	CHECK_EQ(*slots.BeginWrite(4), 2u);
	slots.EndWrite(2);  // replaces slot 1, never taken, which is free again
	CHECK_EQ(*slots.TakeNewest(), 2u);
	CHECK_EQ(*slots.BeginWrite(4), 1u);
	slots.EndWrite(1);
	CHECK_EQ(*slots.BeginWrite(5), 0u);  // fence 5 has passed
}

TEST(HudSlotsAbandonAFailedWrite)
{
	HudSlots slots;
	CHECK_EQ(*slots.BeginWrite(0), 0u);
	slots.Abandon(0);  // its upload buffer couldn't be made
	CHECK(!slots.TakeNewest().has_value());
	CHECK_EQ(*slots.BeginWrite(0), 0u);  // free again
}

TEST(HudSlotsReplaceTheWaitingImageWhenTheGpuHasTheRest)
{
	HudSlots slots;
	for (uint32_t i = 0; i < 2; ++i)
	{
		const uint32_t slot = *slots.BeginWrite(0);
		slots.EndWrite(slot);
		slots.Reading(*slots.TakeNewest(), 10 + i);
	}
	CHECK_EQ(*slots.BeginWrite(0), 2u);
	slots.EndWrite(2);
	CHECK_EQ(*slots.BeginWrite(0), 2u);  // only the waiting image's slot is left: the newer one replaces it
	CHECK(!slots.TakeNewest().has_value());  // not while it's being written
	CHECK(!slots.BeginWrite(0).has_value());  // and nothing else is free
	slots.EndWrite(2);
	CHECK_EQ(*slots.TakeNewest(), 2u);
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestPendingSections.cpp
```
```cmake
	tests/TestPendingSections.cpp
	tests/TestHud.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'render/Hud.h'`.

- [x] **Step 4: Write `src/render/Hud.h`**

```cpp
#pragma once

#include <array>
#include <cstdint>
#include <optional>

// The HUD pass's rules (the spec's "The HUD"): when Minecraft's HUD image is wanted, the size to ask Minecraft for,
// which texel of the image each back-buffer pixel shows, and how images pass through the three upload buffers.
// shaders/blocks.hlsl's HudPS does HudTexel's arithmetic, so the tests here pin the shader's too.
namespace cpmc
{
// Whether to take Minecraft's HUD images, and draw the newest. aSetting is the `hud` setting: 0 never, 1 in build mode
// (the default), 2 always. Either way only while Minecraft is in its world.
bool HudWanted(int aSetting, bool aBuildMode, bool aMinecraftInWorld);

struct HudSize
{
	uint32_t width = 0;
	uint32_t height = 0;
};

// The image to ask Minecraft for (SkyState's viewport): the back buffer's size over aScale (1, 2 or 4), rounded down, and
// at most an overlay slot's 3840 by 2160. 0 by 0, which leaves Minecraft's window alone, before the back buffer is known
// or for another scale.
HudSize HudViewport(uint32_t aBackWidth, uint32_t aBackHeight, uint32_t aScale);

// The scale an image is drawn at: back-buffer pixels per image texel, each way, as many as fit (at least 1). From the
// image itself, so an image Minecraft drew at the size asked for before `hudscale` changed is still drawn right.
uint32_t HudScaleFor(uint32_t aBackWidth, uint32_t aBackHeight, uint32_t aWidth, uint32_t aHeight);

// The image texel back-buffer pixel (aX, aY) shows, as an index into the image's rows as they're stored: aScale pixels a
// texel from the top left, and rows flipped when they're stored bottom-up. Nothing outside the image, where the pass
// shows nothing.
std::optional<uint32_t> HudTexel(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight, uint32_t aScale, bool aBottomUp);

// Which of the three upload buffers each image goes through. The main thread copies an image into a slot the GPU has
// finished with, and it waits there as the newest. The HUD pass takes the newest, once, and the GPU reads it until its
// fence. An image never taken is replaced by the next, as in Minecraft's own triple buffer. Not thread-safe: the
// renderer holds its lock around each call, though not while the main thread copies.
class HudSlots
{
public:
	static constexpr uint32_t kSlots = 3;

	// A slot to copy the next image into: a free one, or else the waiting newest, which the next image replaces. Nothing
	// if the GPU still reads the rest. aCompleted is the renderer's completed fence value.
	std::optional<uint32_t> BeginWrite(uint64_t aCompleted);
	void                    EndWrite(uint32_t aSlot);  // the slot is the newest now
	void                    Abandon(uint32_t aSlot);   // a write that failed: the slot is free again
	std::optional<uint32_t> TakeNewest();              // for the pass: the newest, once
	void                    Reading(uint32_t aSlot, uint64_t aFence);  // the GPU reads the slot taken until aFence

private:
	enum class State
	{
		Free,
		Writing,
		Newest,
		Reading,
	};

	std::array<State, kSlots>    state_{};
	std::array<uint64_t, kSlots> fence_{};
};
}
```

- [x] **Step 5: Write `src/render/Hud.cpp`**

```cpp
#include "render/Hud.h"

#include <algorithm>

namespace cpmc
{
bool HudWanted(int aSetting, bool aBuildMode, bool aMinecraftInWorld)
{
	return aMinecraftInWorld && (aSetting == 2 || (aSetting == 1 && aBuildMode));
}

HudSize HudViewport(uint32_t aBackWidth, uint32_t aBackHeight, uint32_t aScale)
{
	if (aBackWidth == 0 || aBackHeight == 0 || (aScale != 1 && aScale != 2 && aScale != 4))
	{
		return {};
	}
	return { std::clamp(aBackWidth / aScale, 1u, 3840u), std::clamp(aBackHeight / aScale, 1u, 2160u) };
}

uint32_t HudScaleFor(uint32_t aBackWidth, uint32_t aBackHeight, uint32_t aWidth, uint32_t aHeight)
{
	if (aWidth == 0 || aHeight == 0)
	{
		return 1;
	}
	return std::max(1u, std::min(aBackWidth / aWidth, aBackHeight / aHeight));
}

std::optional<uint32_t> HudTexel(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight, uint32_t aScale, bool aBottomUp)
{
	if (aScale == 0)
	{
		return std::nullopt;
	}
	const uint32_t x = aX / aScale;
	const uint32_t y = aY / aScale;
	if (x >= aWidth || y >= aHeight)
	{
		return std::nullopt;
	}
	return (aBottomUp ? aHeight - 1 - y : y) * aWidth + x;
}

std::optional<uint32_t> HudSlots::BeginWrite(uint64_t aCompleted)
{
	std::optional<uint32_t> newest;
	for (uint32_t slot = 0; slot < kSlots; ++slot)
	{
		if (state_[slot] == State::Reading && fence_[slot] <= aCompleted)
		{
			state_[slot] = State::Free;
		}
		if (state_[slot] == State::Free)
		{
			state_[slot] = State::Writing;
			return slot;
		}
		if (state_[slot] == State::Newest)
		{
			newest = slot;
		}
	}
	if (newest)
	{
		state_[*newest] = State::Writing;
	}
	return newest;
}

void HudSlots::EndWrite(uint32_t aSlot)
{
	for (State& state : state_)
	{
		if (state == State::Newest)
		{
			state = State::Free;  // never taken
		}
	}
	state_[aSlot] = State::Newest;
}

void HudSlots::Abandon(uint32_t aSlot)
{
	state_[aSlot] = State::Free;
}

std::optional<uint32_t> HudSlots::TakeNewest()
{
	for (uint32_t slot = 0; slot < kSlots; ++slot)
	{
		if (state_[slot] == State::Newest)
		{
			state_[slot] = State::Reading;
			fence_[slot] = UINT64_MAX;  // until Reading says when the GPU is done with it
			return slot;
		}
	}
	return std::nullopt;
}

void HudSlots::Reading(uint32_t aSlot, uint64_t aFence)
{
	fence_[aSlot] = aFence;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/render/PendingSections.cpp
)
```
```cmake
	src/render/PendingSections.cpp
	src/render/Hud.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `189 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: the HUD's rules" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 16: The HUD's shaders

Two entry points in `blocks.hlsl`, with the existing root signature (the spec's "The HUD's shaders"):
- **`HudVS`** makes one triangle covering the screen from the vertex index alone, so the pass needs no vertex buffer.
- **`HudPS`** reads the image as a structured buffer of RGBA8 texels in `t1`, the way the blocks read the atlas, so no
  texture or descriptor heap comes in. Its constants in `b1` give the image's size, the scale and the flip. It returns
  the texel as it is, since its alpha is premultiplied, and nothing outside the image, as `HudTexel` has it.

The tests check that both compile, that `Hud` matches `HudConstants`, and that `HudPS` writes one target.

**Files:**
- Modify: `tests/TestShaders.cpp`, `shaders/blocks.hlsl`, `src/render/FrameConstants.h`, `src/render/Shaders.h`,
  `src/render/Shaders.cpp`

- [x] **Step 1: Edit `tests/TestShaders.cpp` to test the HUD's shaders**

```cpp
		CHECK_EQ(ConstantBufferSize(shaders->depth, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
```
```cpp
		CHECK_EQ(ConstantBufferSize(shaders->depth, "Frame"), UINT(sizeof(cpmc::FrameConstants)));
		CHECK(!shaders->hudVertex.empty());
		CHECK(!shaders->hud.empty());
		CHECK_EQ(ConstantBufferSize(shaders->hud, "Hud"), UINT(sizeof(cpmc::HudConstants)));
```
```cpp
TEST(ShadersReportCompileErrors)
```
```cpp
TEST(ShadersDrawTheHud)
{
	std::string error;
	const auto  shaders = cpmc::CompileBlockShaders(ShaderSource(), error);
	CHECK(shaders.has_value());
	if (shaders)
	{
		CHECK_EQ(Outputs(shaders->hud), 1u);  // the back buffer
	}
}

TEST(ShadersReportCompileErrors)
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'hudVertex': is not a member of 'cpmc::BlockShaders'`.

- [x] **Step 3: Edit `shaders/blocks.hlsl` to add the HUD's shaders**

```hlsl
        discard;
    }
}
```
```hlsl
        discard;
    }
}

// Spike 4: Minecraft's HUD image (its hotbar, crosshair, chat, hand and held item), drawn over the finished frame at
// Present with premultiplied-alpha blending. The image is RGBA8 with premultiplied alpha, one texel per uint (red in the
// low byte), its rows stored as Minecraft sent them. Each texel covers hudInfo.z by hudInfo.z back-buffer pixels from
// the top left, and nothing shows outside the image. src/render/Hud.cpp's HudTexel does the same arithmetic.
cbuffer Hud : register(b1)
{
    uint4 hudInfo;  // the image's width and height; back-buffer pixels per texel; 1 if its rows are stored bottom-up
};

StructuredBuffer<uint> hudImage : register(t1);

// One triangle covering the screen, from the vertex index alone.
float4 HudVS(uint id : SV_VertexID) : SV_Position
{
    const float2 corner = float2((id << 1) & 2, id & 2);  // (0, 0), (2, 0), (0, 2)
    return float4(corner * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

float4 HudPS(float4 position : SV_Position) : SV_Target0
{
    const uint2 texel = (uint2)position.xy / max(hudInfo.z, 1u);
    if (texel.x >= hudInfo.x || texel.y >= hudInfo.y)
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }
    const uint row = hudInfo.w != 0 ? hudInfo.y - 1 - texel.y : texel.y;
    const uint packed = hudImage[row * hudInfo.x + texel.x];
    return float4(packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF, packed >> 24) / 255.0;
}
```

- [x] **Step 4: Edit `src/render/FrameConstants.h` to add the HUD's constants**

```cpp
static_assert(sizeof(SectionConstants) == 16);
```
```cpp
static_assert(sizeof(SectionConstants) == 16);

// The HUD pass's root constants, in b1 where the blocks have SectionConstants.
struct HudConstants
{
	uint32_t width = 0;     // the image's
	uint32_t height = 0;
	uint32_t scale = 1;     // back-buffer pixels per image texel, each way
	uint32_t bottomUp = 0;  // 1 if its rows are stored bottom-up
};
static_assert(sizeof(HudConstants) == 16);
```

- [x] **Step 5: Edit `src/render/Shaders.h` to hold the HUD's shaders**

```cpp
	std::vector<uint8_t> vertex;   // BlocksVS
	std::vector<uint8_t> present;  // PresentPS
	std::vector<uint8_t> predlss;  // PredlssPS
	std::vector<uint8_t> gbuffer;  // GbufferPS
	std::vector<uint8_t> depth;    // DepthPS
```
```cpp
	std::vector<uint8_t> vertex;     // BlocksVS
	std::vector<uint8_t> present;    // PresentPS
	std::vector<uint8_t> predlss;    // PredlssPS
	std::vector<uint8_t> gbuffer;    // GbufferPS
	std::vector<uint8_t> depth;      // DepthPS
	std::vector<uint8_t> hudVertex;  // HudVS
	std::vector<uint8_t> hud;        // HudPS
```

- [x] **Step 6: Edit `src/render/Shaders.cpp` to compile them**

```cpp
	return BlockShaders{ std::move(*vertex), std::move(*present), std::move(*predlss), std::move(*gbuffer), std::move(*depth) };
```
```cpp
	auto hudVertex = CompileShader(aSource, "blocks.hlsl", "HudVS", "vs_5_1", aError);
	if (!hudVertex)
	{
		return std::nullopt;
	}
	auto hud = CompileShader(aSource, "blocks.hlsl", "HudPS", "ps_5_1", aError);
	if (!hud)
	{
		return std::nullopt;
	}
	return BlockShaders{ std::move(*vertex), std::move(*present), std::move(*predlss), std::move(*gbuffer),
		                 std::move(*depth), std::move(*hudVertex), std::move(*hud) };
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `190 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: the HUD's shaders" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 17: Requests for the link

The spec's "Requests": `link on|off`, `floor`, and `mc` followed by a set's name or a command. A command runs verbatim
to the end of the line, since a Minecraft command keeps its own spacing. The request is checked as the link's
`CommandTyper` will check it, through a new `ValidCommandOrSet`, so a bad one is refused when it's read, with the
reason in the status. The new settings (`translucent`, `teleportstep`, `teleportrate`, `ringmb`, `hud` and
`hudscale`) are plain `set` requests, which the plugin checks in Task 24.

**Files:**
- Modify: `tests/TestRequests.cpp`, `src/link/Commands.h`, `src/link/Commands.cpp`, `src/Requests.h`,
  `src/Requests.cpp`

- [x] **Step 1: Edit `tests/TestRequests.cpp` to test the link's requests**

```cpp
	CHECK(!off.dred && !off.debugLayer);
}
```
```cpp
	CHECK(!off.dred && !off.debugLayer);
}

TEST(RequestsForTheLink)
{
	std::string error;
	const auto  on = cpmc::ParseRequest("link on\n", error);
	CHECK(on.has_value());
	CHECK(on->kind == cpmc::Request::Kind::Link);
	CHECK_STR(on->what, "on");
	CHECK_STR(cpmc::ParseRequest("link off", error)->what, "off");
	CHECK(!cpmc::ParseRequest("link", error));
	CHECK(!cpmc::ParseRequest("link maybe", error));
	CHECK(cpmc::ParseRequest("floor", error)->kind == cpmc::Request::Kind::Floor);
	CHECK(!cpmc::ParseRequest("floor now", error));
}

TEST(RequestsTypeIntoMinecraft)
{
	std::string error;
	const auto  set = cpmc::ParseRequest("mc tnt-cube\n", error);
	CHECK(set.has_value());
	CHECK(set->kind == cpmc::Request::Kind::Mc);
	CHECK_STR(set->what, "tnt-cube");
	// A command, verbatim to the end of the line, but for the spaces around it.
	const auto command = cpmc::ParseRequest("  mc /execute rotated ~ 0 positioned ^ ^ ^4 run setblock ~ ~ ~ stone \r\n", error);
	CHECK_STR(command->what, "/execute rotated ~ 0 positioned ^ ^ ^4 run setblock ~ ~ ~ stone");
	CHECK(!cpmc::ParseRequest("mc", error));
	CHECK(!cpmc::ParseRequest("mc explode", error));
	CHECK(error.find("neither a command nor a set") != std::string::npos);
	CHECK(!cpmc::ParseRequest("mc /say a\nb", error));  // one line only
	CHECK(error.find("printable ASCII") != std::string::npos);
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2065: 'Link': undeclared identifier`.

- [x] **Step 3: Edit `src/link/Commands.h` to declare `ValidCommandOrSet`**

```cpp
bool ValidCommand(std::string_view aCommand, std::string& aError);
```
```cpp
bool ValidCommand(std::string_view aCommand, std::string& aError);

// A valid command, or the name of a set. False, with aError, for anything else.
bool ValidCommandOrSet(std::string_view aCommandOrName, std::string& aError);
```

- [x] **Step 4: Edit `src/link/Commands.cpp` to define it, and queue through it**

```cpp
bool CommandTyper::Queue(std::string_view aCommandOrName, std::string& aError)
{
	if (const auto named = NamedCommands(aCommandOrName))
	{
		queue_.insert(queue_.end(), named->begin(), named->end());
		return true;
	}
	if (!ValidCommand(aCommandOrName, aError))
	{
		if (aCommandOrName.empty() || aCommandOrName.front() != '/')
		{
			aError = std::format("\"{}\" is neither a command nor a set (creative, tnt-kit, tnt-ahead, wall, tnt-cube, field, wipe)",
			                     aCommandOrName);
		}
		return false;
	}
	queue_.emplace_back(aCommandOrName);
```
```cpp
bool ValidCommandOrSet(std::string_view aCommandOrName, std::string& aError)
{
	if (NamedCommands(aCommandOrName))
	{
		return true;
	}
	if (aCommandOrName.empty() || aCommandOrName.front() != '/')
	{
		aError = std::format("\"{}\" is neither a command nor a set (creative, tnt-kit, tnt-ahead, wall, tnt-cube, field, wipe)",
		                     aCommandOrName);
		return false;
	}
	return ValidCommand(aCommandOrName, aError);
}

bool CommandTyper::Queue(std::string_view aCommandOrName, std::string& aError)
{
	if (!ValidCommandOrSet(aCommandOrName, aError))
	{
		return false;
	}
	if (const auto named = NamedCommands(aCommandOrName))
	{
		queue_.insert(queue_.end(), named->begin(), named->end());
	}
	else
	{
		queue_.emplace_back(aCommandOrName);
	}
```

- [x] **Step 5: Edit `src/Requests.h` to add the requests**

```cpp
//   set <name> <value>         (name: lower-case letters, digits, '.' and '_'; value: a finite number)
```
```cpp
//   set <name> <value>         (name: lower-case letters, digits, '.' and '_'; value: a finite number)
//   link on|off                (spike 4: the in-game flag)
//   floor                      (a fresh floor around V)
//   mc <set>|<command>         (a set: creative, tnt-kit, tnt-ahead, wall, tnt-cube, field or wipe; a command starts with
//                               '/' and runs verbatim to the end of the line)
```
```cpp
		Set,
	};

	Kind        kind = Kind::Log;
	std::string what;          // the dump target, mode, structure or setting name
```
```cpp
		Set,
		Link,
		Floor,
		Mc,
	};

	Kind        kind = Kind::Log;
	std::string what;          // the dump target, mode, structure, setting name, on or off, or what to type
```

- [x] **Step 6: Edit `src/Requests.cpp` to parse them**

```cpp
#include "Requests.h"
```
```cpp
#include "Requests.h"

#include "link/Commands.h"
```
```cpp
	return std::nullopt;
}
}
```
```cpp
	return std::nullopt;
}

std::string_view Trim(std::string_view aText)
{
	while (!aText.empty() && IsSpace(aText.front()))
	{
		aText.remove_prefix(1);
	}
	while (!aText.empty() && IsSpace(aText.back()))
	{
		aText.remove_suffix(1);
	}
	return aText;
}
}
```
```cpp
	if (command == "log" || command == "clear" || command == "reload")
```
```cpp
	if (command == "log" || command == "clear" || command == "reload" || command == "floor")
```
```cpp
		const Request::Kind kind = command == "log" ? Request::Kind::Log : command == "clear" ? Request::Kind::Clear : Request::Kind::Reload;
		return Request{ kind };
```
```cpp
		const Request::Kind kind = command == "log"     ? Request::Kind::Log
		                           : command == "clear" ? Request::Kind::Clear
		                           : command == "floor" ? Request::Kind::Floor
		                                                : Request::Kind::Reload;
		return Request{ kind };
	}
	if (command == "link")
	{
		if (arguments != 1 || !OneOf(words[1], { "on", "off" }))
		{
			return Fail(aError, "link takes on or off");
		}
		return Request{ Request::Kind::Link, std::string(words[1]) };
	}
	if (command == "mc")
	{
		// The rest of the line, verbatim: a Minecraft command keeps its own spacing.
		const std::string_view rest = Trim(aText.substr(size_t(command.data() - aText.data()) + command.size()));
		if (rest.empty())
		{
			return Fail(aError, "mc takes a set's name, or a command starting with /");
		}
		std::string why;
		if (!link::ValidCommandOrSet(rest, why))
		{
			return Fail(aError, "mc: " + why);
		}
		return Request{ Request::Kind::Mc, std::string(rest) };
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `192 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: requests for the link" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 18: Latency and bursts

The spec's "Measurements", as pure classes on times in seconds:
- **`LatencyTracker`:** a mark (a typed command's Enter, in Part 3 a click too) times the first section to arrive
  within a second after it, to the first list that draws its upload. The sample has two halves: Minecraft's, from the
  mark to the arrival, and ours, from the arrival to the drawing, in milliseconds and in the game's frames. A mark
  with no section within a second is dropped.
- **`BurstWindow`:** an explosion, or a typed command's Enter, opens a window of a second. Each frame in it adds its
  sections, bytes, drain time, `Update`'s time and frame time, and the first frame after it closes the window with its
  report. A start while one is open is ignored.
- **`SteadySeconds`:** the clock the plugin and the renderer share, so their times compare.

**Files:**
- Create: `src/Measurements.h`, `src/Measurements.cpp`, `tests/TestMeasurements.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestMeasurements.cpp`**

```cpp
#include "Harness.h"

#include "Measurements.h"

using namespace cpmc;

TEST(LatencyTimesTheFirstSectionAfterAMark)
{
	LatencyTracker latency;
	CHECK(!latency.SectionArrived(1.0, 10));  // no mark: not timed
	latency.Mark(2.0);
	CHECK(latency.SectionArrived(2.12, 20));
	CHECK(!latency.SectionArrived(2.13, 20));  // only the first
	const auto sample = latency.Drawn(2.15, 22);
	CHECK(sample.has_value());
	CHECK_NEAR(sample->minecraftMs, 120.0, 1e-6);
	CHECK_NEAR(sample->oursMs, 30.0, 1e-6);
	CHECK_NEAR(sample->oursFrames, 2.0, 1e-9);
	CHECK(!latency.Drawn(2.2, 23).has_value());  // once
	CHECK_EQ(latency.Samples(), size_t(1));
}

TEST(LatencyDropsAMarkWithNoSectionWithinASecond)
{
	LatencyTracker latency;
	latency.Mark(1.0);
	CHECK(!latency.SectionArrived(2.5, 30));  // a click that changed nothing, or a number key
	CHECK(!latency.Drawn(2.6, 31).has_value());
	CHECK_EQ(latency.Samples(), size_t(0));
	CHECK(!latency.Last().has_value());
}

TEST(LatencyKeepsTheLastAndTheAverage)
{
	LatencyTracker latency;
	latency.Mark(1.0);
	latency.SectionArrived(1.1, 0);
	latency.Drawn(1.12, 1);
	latency.Mark(5.0);
	latency.SectionArrived(5.3, 10);
	latency.Drawn(5.34, 13);
	CHECK_NEAR(latency.Last()->minecraftMs, 300.0, 1e-6);
	CHECK_NEAR(latency.Average()->minecraftMs, 200.0, 1e-6);
	CHECK_NEAR(latency.Average()->oursMs, 30.0, 1e-6);
	CHECK_NEAR(latency.Average()->oursFrames, 2.0, 1e-9);
}

TEST(BurstsCoverTheSecondAfterTheirStart)
{
	BurstWindow     burst;
	const BurstFrame quiet{ 5, 1'000'000, 0.5, 2.0, 16.0 };
	const BurstFrame busy{ 20, 4'000'000, 1.5, 6.0, 30.0 };
	const BurstFrame after{ 1, 0, 0.1, 0.2, 15.0 };
	CHECK(!burst.Open());
	CHECK(!burst.Frame(9.0, busy).has_value());  // no burst: nothing counted
	burst.Start(10.0, "explosion");
	CHECK(burst.Open());
	burst.Start(10.2, "typed: /fill");  // one at a time
	CHECK(!burst.Frame(10.1, quiet).has_value());
	CHECK(!burst.Frame(10.5, busy).has_value());
	const auto report = burst.Frame(11.05, after);  // its second is up: this frame isn't counted
	CHECK(report.has_value());
	CHECK_STR(report->what, "explosion");
	CHECK_EQ(report->sections, 25ull);
	CHECK_EQ(report->bytes, 5'000'000ull);
	CHECK_EQ(report->frames, 2ull);
	CHECK_NEAR(report->drainMsWorst, 1.5, 1e-9);
	CHECK_NEAR(report->updateMsWorst, 6.0, 1e-9);
	CHECK_NEAR(report->frameMsWorst, 30.0, 1e-9);
	CHECK(!burst.Open());
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestHud.cpp
```
```cmake
	tests/TestHud.cpp
	tests/TestMeasurements.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'Measurements.h'`.

- [x] **Step 4: Write `src/Measurements.h`**

```cpp
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

// What step 2 measures beyond the renderer's own figures (the spec's "Measurements"): latency, and bursts. The classes are
// pure, on times in seconds from any steady clock, and main thread only. The plugin and the renderer use SteadySeconds,
// which any thread may call.
namespace cpmc
{
// Seconds on the steady clock: the time base the plugin's measurements and the renderer's share.
inline double SteadySeconds()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// One typed command, or one click, timed.
struct LatencySample
{
	double minecraftMs = 0.0;  // the mark to the section's arrival: Minecraft's next tick, and for a command, its chat and
	                           // its server too
	double oursMs = 0.0;       // the section's arrival to the first list recorded that draws its upload
	double oursFrames = 0.0;   // the same, in the game's frames
};

// The spec's latency. A mark (a typed command's Enter, or a forwarded click) times the first section to arrive within a
// second after it, to the first list that draws its upload. A mark with no section within a second is dropped: a click
// that changed nothing, or a number key. Marks mean something only while nothing else changes sections, so they're
// taken while V stands still.
class LatencyTracker
{
public:
	void Mark(double aNow);  // replaces any mark still waiting for its section
	// A section arrived, aFrame being the game's frame count: true if it's the first within a second of a mark, and so
	// the one to time.
	bool SectionArrived(double aNow, uint64_t aFrame);
	// The section being timed was first drawn: its sample, kept as the last and in the average.
	std::optional<LatencySample> Drawn(double aNow, uint64_t aFrame);

	std::optional<LatencySample> Last() const { return last_; }
	std::optional<LatencySample> Average() const;
	size_t                       Samples() const { return samples_; }

private:
	std::optional<double>        mark_;
	std::optional<double>        timedMark_;  // the mark of the section on its way to being drawn
	double                       arrival_ = 0.0;
	uint64_t                     arrivalFrame_ = 0;
	std::optional<LatencySample> last_;
	LatencySample                sum_;
	size_t                       samples_ = 0;
};

// One frame's share of a burst.
struct BurstFrame
{
	uint64_t sections = 0;    // arrived this frame
	uint64_t bytes = 0;       // of the render ring, drained this frame
	double   drainMs = 0.0;
	double   updateMs = 0.0;  // the renderer's Update
	double   frameMs = 0.0;   // since the frame before
};

struct BurstReport
{
	std::string what;  // what started it
	uint64_t    frames = 0;
	uint64_t    sections = 0;
	uint64_t    bytes = 0;
	double      drainMsWorst = 0.0;
	double      updateMsWorst = 0.0;
	double      frameMsWorst = 0.0;
};

// The spec's bursts: an explosion, or a typed command's Enter, starts one, and the second after it is summed up. One at
// a time: a start while one is open is ignored.
class BurstWindow
{
public:
	void Start(double aNow, std::string aWhat);
	bool Open() const { return start_.has_value(); }
	// Each frame: counted while the burst's second lasts. The first frame after it closes the burst, and gets its report.
	std::optional<BurstReport> Frame(double aNow, const BurstFrame& aFrame);

private:
	std::optional<double> start_;
	BurstReport           report_;
};
}
```

- [x] **Step 5: Write `src/Measurements.cpp`**

```cpp
#include "Measurements.h"

#include <algorithm>

namespace cpmc
{
void LatencyTracker::Mark(double aNow)
{
	mark_ = aNow;
}

bool LatencyTracker::SectionArrived(double aNow, uint64_t aFrame)
{
	if (!mark_)
	{
		return false;
	}
	const double mark = *mark_;
	mark_.reset();
	if (aNow - mark > 1.0)
	{
		return false;
	}
	timedMark_ = mark;
	arrival_ = aNow;
	arrivalFrame_ = aFrame;
	return true;
}

std::optional<LatencySample> LatencyTracker::Drawn(double aNow, uint64_t aFrame)
{
	if (!timedMark_)
	{
		return std::nullopt;
	}
	const LatencySample sample{ (arrival_ - *timedMark_) * 1000.0, (aNow - arrival_) * 1000.0,
		                        aFrame >= arrivalFrame_ ? double(aFrame - arrivalFrame_) : 0.0 };
	timedMark_.reset();
	last_ = sample;
	sum_.minecraftMs += sample.minecraftMs;
	sum_.oursMs += sample.oursMs;
	sum_.oursFrames += sample.oursFrames;
	++samples_;
	return sample;
}

std::optional<LatencySample> LatencyTracker::Average() const
{
	if (samples_ == 0)
	{
		return std::nullopt;
	}
	const double count = double(samples_);
	return LatencySample{ sum_.minecraftMs / count, sum_.oursMs / count, sum_.oursFrames / count };
}

void BurstWindow::Start(double aNow, std::string aWhat)
{
	if (start_)
	{
		return;
	}
	start_ = aNow;
	report_ = BurstReport{ std::move(aWhat) };
}

std::optional<BurstReport> BurstWindow::Frame(double aNow, const BurstFrame& aFrame)
{
	if (!start_)
	{
		return std::nullopt;
	}
	if (aNow - *start_ > 1.0)
	{
		start_.reset();
		return report_;
	}
	++report_.frames;
	report_.sections += aFrame.sections;
	report_.bytes += aFrame.bytes;
	report_.drainMsWorst = std::max(report_.drainMsWorst, aFrame.drainMs);
	report_.updateMsWorst = std::max(report_.updateMsWorst, aFrame.updateMs);
	report_.frameMsWorst = std::max(report_.frameMsWorst, aFrame.frameMs);
	return std::nullopt;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/render/Hud.cpp
)
```
```cmake
	src/render/Hud.cpp
	src/Measurements.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `196 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: latency and bursts" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 19: Both processes' CPU and memory

The spec's "Measurements" asks for CPU, as a share of one core, and the working set, for Cyberpunk and for Minecraft
(from the header's `mcPid`). `SampleProcess` reads a process's times and working set, and `CpuShare` turns two
samples into a share. `ProcessWatch` follows one process by ID: it opens it with the least access that reads both,
tries again each second until that works, and starts over when the ID changes. The tests sample this process.

**Files:**
- Create: `src/ProcessUsage.h`, `src/ProcessUsage.cpp`, `tests/TestProcessUsage.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestProcessUsage.cpp`**

```cpp
#include "Harness.h"

#include "ProcessUsage.h"

TEST(ProcessUsageSamplesThisProcess)
{
	const auto sample = cpmc::SampleProcess(GetCurrentProcess(), 1.0);
	CHECK(sample.has_value());
	CHECK(sample->workingSet > 1'000'000);  // the test program takes more than a megabyte
	CHECK_EQ(sample->at, 1.0);
}

TEST(ProcessUsageCpuIsAShareOfOneCore)
{
	const cpmc::ProcessSample before{ 10.0, 0, 0 };
	const cpmc::ProcessSample after{ 12.0, 10'000'000, 0 };  // 1 s of CPU in 2 s
	CHECK_NEAR(cpmc::CpuShare(before, after), 0.5, 1e-9);
	const cpmc::ProcessSample busy{ 12.0, 40'000'000, 0 };  // 4 s of CPU in 2 s: two cores
	CHECK_NEAR(cpmc::CpuShare(before, busy), 2.0, 1e-9);
	CHECK_NEAR(cpmc::CpuShare(after, after), 0.0, 1e-9);  // no time between them
}

TEST(ProcessWatchFollowsAProcessById)
{
	cpmc::ProcessWatch watch;
	CHECK(!watch.Known());
	watch.Sample(GetCurrentProcessId(), 1.0);
	CHECK(watch.Known());
	CHECK(watch.WorkingSet() > 1'000'000);
	CHECK_NEAR(watch.CpuShare(), 0.0, 1e-9);  // one sample: no share yet
	watch.Sample(GetCurrentProcessId(), 2.0);
	CHECK(watch.CpuShare() >= 0.0);
	watch.Sample(0, 3.0);  // none
	CHECK(!watch.Known());
	watch.Sample(0xFFFFFFF0, 4.0);  // no process has it
	CHECK(!watch.Known());
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestMeasurements.cpp
```
```cmake
	tests/TestMeasurements.cpp
	tests/TestProcessUsage.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'ProcessUsage.h'`.

- [x] **Step 4: Write `src/ProcessUsage.h`**

```cpp
#pragma once

#include <Windows.h>

#include <cstdint>
#include <optional>

// A process's CPU and memory, for the status (the spec's "Measurements": Cyberpunk's, and Minecraft's from mcPid).
namespace cpmc
{
struct ProcessSample
{
	double   at = 0.0;        // seconds, on the caller's steady clock
	uint64_t cpu100ns = 0;    // kernel and user time so far, in 100 ns units
	uint64_t workingSet = 0;  // bytes
};

// Nothing if it can't be read. aProcess needs PROCESS_QUERY_LIMITED_INFORMATION and PROCESS_VM_READ.
std::optional<ProcessSample> SampleProcess(HANDLE aProcess, double aNow);

// The CPU used between two samples, as a share of one core: 1 is a whole core busy all the time.
double CpuShare(const ProcessSample& aBefore, const ProcessSample& aAfter);

// One process, followed by its ID: sampled once a second, and opened again whenever the ID changes.
class ProcessWatch
{
public:
	ProcessWatch() = default;
	ProcessWatch(const ProcessWatch&) = delete;
	ProcessWatch& operator=(const ProcessWatch&) = delete;
	~ProcessWatch();

	void     Sample(uint32_t aPid, double aNow);  // aPid 0: none
	bool     Known() const { return last_.has_value(); }  // the last sample read the process
	double   CpuShare() const { return cpuShare_; }       // since the sample before; 0 until there are two
	uint64_t WorkingSet() const { return last_ ? last_->workingSet : 0; }

private:
	void Close();

	HANDLE                       handle_ = nullptr;
	uint32_t                     pid_ = 0;
	std::optional<ProcessSample> last_;
	double                       cpuShare_ = 0.0;
};
}
```

- [x] **Step 5: Write `src/ProcessUsage.cpp`**

```cpp
#include "ProcessUsage.h"

#include <psapi.h>

namespace cpmc
{
namespace
{
uint64_t Ticks(const FILETIME& aTime)
{
	return (uint64_t(aTime.dwHighDateTime) << 32) | aTime.dwLowDateTime;
}
}

std::optional<ProcessSample> SampleProcess(HANDLE aProcess, double aNow)
{
	FILETIME                   created{}, exited{}, kernel{}, user{};
	PROCESS_MEMORY_COUNTERS_EX memory{};
	if (!aProcess || !GetProcessTimes(aProcess, &created, &exited, &kernel, &user) ||
	    !GetProcessMemoryInfo(aProcess, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof memory))
	{
		return std::nullopt;
	}
	return ProcessSample{ aNow, Ticks(kernel) + Ticks(user), memory.WorkingSetSize };
}

double CpuShare(const ProcessSample& aBefore, const ProcessSample& aAfter)
{
	const double seconds = aAfter.at - aBefore.at;
	if (seconds <= 0.0 || aAfter.cpu100ns < aBefore.cpu100ns)
	{
		return 0.0;
	}
	return double(aAfter.cpu100ns - aBefore.cpu100ns) / (seconds * 1e7);
}

ProcessWatch::~ProcessWatch()
{
	Close();
}

void ProcessWatch::Close()
{
	if (handle_)
	{
		CloseHandle(handle_);
		handle_ = nullptr;
	}
	last_.reset();
	cpuShare_ = 0.0;
}

void ProcessWatch::Sample(uint32_t aPid, double aNow)
{
	if (aPid != pid_)
	{
		Close();
		pid_ = aPid;
	}
	if (!handle_ && pid_ != 0)
	{
		handle_ = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid_);  // again each second until it opens
	}
	const std::optional<ProcessSample> sample = SampleProcess(handle_, aNow);
	cpuShare_ = sample && last_ ? cpmc::CpuShare(*last_, *sample) : 0.0;
	last_ = sample;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/Measurements.cpp
)
```
```cmake
	src/Measurements.cpp
	src/ProcessUsage.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `199 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: both processes' CPU and memory" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 20: The link reports Minecraft's eye, the drain's time, and the render ring's types by name

Three things the status needs from Part 1's link (the spec's "Measurements"):
- **Minecraft's eye,** from `McState`, to compare with V's camera: the teleport puts it there.
- **The drain's time,** each update's, timed around the render ring's drain.
- **The render ring's message types by name.** `mclink` had its own names; they move into the link as
  `RenderTypeName`, so the status and `mclink` say the same.

`Overlay.h`'s comment on `Reset` still said it runs when Minecraft connects or comes back. It now says a new Minecraft
process, as the link has done since Part 1's deep review.

**Files:**
- Modify: `tests/TestLink.cpp`, `src/link/Link.h`, `src/link/Link.cpp`, `src/link/Overlay.h`,
  `tools/mclink/Main.cpp`

- [x] **Step 1: Edit `tests/TestLink.cpp` to test the eye, the drain's time and the names**

```cpp
		state.x = -119.5;
```
```cpp
		state.x = -119.5;
		state.eyeX = -119.5;
		state.eyeY = 7.76;
		state.eyeZ = 2007.5;
```
```cpp
	CHECK(sink.Logged("Minecraft has gone"));
```
```cpp
	CHECK(sink.Logged("Minecraft has gone"));
}

TEST(LinkReportsMinecraftsEyeAndTheDrainTime)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.State(proto::kMcInWorld);
	link.Update(At(1.0), kSettings, sink);
	CHECK_NEAR(link.State().minecraftEye.x, -119.5, 1e-9);
	CHECK_NEAR(link.State().minecraftEye.y, 7.76, 1e-9);
	CHECK(link.State().drainMs >= 0.0);
}

TEST(LinkNamesTheRenderTypes)
{
	CHECK_STR(RenderTypeName(proto::kRenSection), "section");
	CHECK_STR(RenderTypeName(proto::kRenAtlasRegion), "atlas patch");
	CHECK_STR(RenderTypeName(Link::kTypes - 1), "other");
	CHECK_STR(RenderTypeName(99), "other");
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'minecraftEye': is not a member of 'cpmc::link::Link::Status'`.

- [x] **Step 3: Edit `src/link/Link.h` to add them**

```cpp
class Link
```
```cpp
// A render message type's name, for reports: "section", "atlas patch" and so on, and "other" from Link::kTypes - 1 on.
const char* RenderTypeName(size_t aType);

class Link
```
```cpp
		Vec3d    minecraftFeet;        // Minecraft axes
```
```cpp
		Vec3d    minecraftFeet;        // Minecraft axes
		Vec3d    minecraftEye;         // Minecraft axes
```
```cpp
		uint64_t                     drainedBytes = 0;
```
```cpp
		uint64_t                     drainedBytes = 0;
		double                       drainMs = 0.0;  // the last update's drain
```

- [x] **Step 4: Edit `src/link/Link.cpp` to fill them in**

```cpp
#include <cmath>
```
```cpp
#include <chrono>
#include <cmath>
```
```cpp
	return std::sqrt((aA.x - aB.x) * (aA.x - aB.x) + (aA.y - aB.y) * (aA.y - aB.y) + (aA.z - aB.z) * (aA.z - aB.z));
}
}
```
```cpp
	return std::sqrt((aA.x - aB.x) * (aA.x - aB.x) + (aA.y - aB.y) * (aA.y - aB.y) + (aA.z - aB.z) * (aA.z - aB.z));
}
}

const char* RenderTypeName(size_t aType)
{
	static const char* const names[Link::kTypes] = { "pad",         "atlas",  "section", "clear-all", "texture", "avatar", "scene",
		                                             "atlas patch", "lights", "ragdoll", "solids",    "dug",     "other" };
	return names[aType < Link::kTypes ? aType : Link::kTypes - 1];
}
```
```cpp
		status_.minecraftFeet = { mc->x, mc->y, mc->z };
```
```cpp
		status_.minecraftFeet = { mc->x, mc->y, mc->z };
		status_.minecraftEye = { mc->eyeX, mc->eyeY, mc->eyeZ };
```
```cpp
	const auto drained = DrainByteRing(base_ + proto::kOffRenderRing, proto::kRenRingDataBytes, aSettings.ringBudget,
	                                   [&](const RingMessage& aMessage) { return Take(aMessage, aSettings, aSink); });
```
```cpp
	const auto drainStart = std::chrono::steady_clock::now();
	const auto drained = DrainByteRing(base_ + proto::kOffRenderRing, proto::kRenRingDataBytes, aSettings.ringBudget,
	                                   [&](const RingMessage& aMessage) { return Take(aMessage, aSettings, aSink); });
	status_.drainMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - drainStart).count();
```

- [x] **Step 5: Edit `src/link/Overlay.h` to say when the reset runs**

```cpp
	// When Minecraft connects or comes back: the state to 0, and our front to slot 2. Minecraft's writer starts at 1.
```
```cpp
	// For a new Minecraft process: the state to 0, and our front to slot 2. Its writer starts over at slot 1.
```

- [x] **Step 6: Edit `tools/mclink/Main.cpp` to use the link's names**

```cpp
}

const char* TypeName(size_t aType)
{
	static const char* names[Link::kTypes] = { "pad",    "atlas",  "section", "clear-all", "texture", "avatar", "scene",
		                                       "atlas patch", "lights", "ragdoll", "solids",    "dug",     "other" };
	return names[std::min(aType, Link::kTypes - 1)];
```
```cpp
```
```cpp
			types += std::format("{}{} {} ({:.2f} MB)", types.empty() ? "" : ", ", s.messages[type], TypeName(type), double(s.bytes[type]) / 1048576.0);
```
```cpp
			types += std::format("{}{} {} ({:.2f} MB)", types.empty() ? "" : ", ", s.messages[type], RenderTypeName(type),
			                     double(s.bytes[type]) / 1048576.0);
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `201 test(s), 0 failure(s)`.

- [x] **Step 8: Run mclink without Minecraft; it must start and stop cleanly**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && printf 'status\nquit\n' > build/selfcheck.txt \
  && ./build/RelWithDebInfo/mclink.exe build/selfcheck.txt > build/selfcheck.log; echo "exit: $?"; head -2 build/selfcheck.log
```
Expected: `exit: 0`, then lines like these (the process ID varies):
```
[   0.00] the mapping: created (process 12345); waiting for Minecraft
[   0.00] Minecraft: absent, feet 0.000 0.000 0.000, frame 0
```

- [x] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: the link reports Minecraft's eye, the drain's time, and the ring's types by name" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 21: The link's lines in the status

`status.txt` gains the spec's "Measurements" as 8 lines, after `request=`, once the plugin has tried the mapping:
- **`link=`:** linked or not; the mapping, created, reused, or refused and why; Minecraft's process, heartbeat, world,
  screen and frame rate, or absent; and whether the game side is in game;
- **`positions=`:** Minecraft's eye and V's camera, in Minecraft's axes, to compare; the teleports in the last second
  and the last target; the floor;
- **`ring=`:** MB in the last second and the most in one frame, the drain's time, messages and MB by type, and what
  was rejected or resynchronised;
- **`atlas=`:** its size, levels and source; translucent triangles dropped; `Update`'s time;
- **`hud=`:** the setting, the scale, and whether it's showing; images received (with their MB), dropped and drawn in
  the last second; the newest image's size and row order; the copy's and the pass's times; the pass's skips; and
  images the link refused;
- **`latency=`:** the last and the average, each in Minecraft's half and ours;
- **`burst=`:** the last burst's report;
- **`processes=`:** Cyberpunk's and Minecraft's CPU and memory.

The `blocks=` line says `Minecraft's` instead of a count while the sections are Minecraft's, whose blocks the plugin
doesn't count. The plugin fills it all in, in Task 24.

**Files:**
- Modify: `tests/TestStatus.cpp`, `src/Status.h`, `src/Status.cpp`

- [x] **Step 1: Edit `tests/TestStatus.cpp` to test the link's lines**

```cpp
	      std::string::npos);
}
```
```cpp
	      std::string::npos);
}

namespace
{
bool Has(const std::string& aStatus, const std::string& aLine)
{
	return aStatus.find(aLine + "\n") != std::string::npos;
}
}

TEST(StatusWritesTheLinksLines)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	link.mapping = "created";
	link.linked = true;
	link.minecraft = true;
	link.minecraftPid = 9052;
	link.heartbeatAgeMs = 16;
	link.inWorld = true;
	link.minecraftFps = 60.0;
	link.inGame = true;
	link.minecraftEye = { -119.5, 7.76, 2007.5 };
	link.camera = cpmc::Vec3d{ -119.5, 7.76, 2007.5 };
	link.teleports = 3;
	link.target = { -119.5, 6.14, 2007.5 };
	link.floorEpoch = 2;
	link.floorSent = true;
	link.floorCentre = { -119.5, 6.14, 2007.5 };
	link.ringMb = 7.52;
	link.ringMbWorstFrame = 0.15;
	link.drainMsAverage = 0.12;
	link.drainMsWorst = 0.9;
	link.ringTypes = "812 atlas patch (7.52 MB), 3 section (0.01 MB)";
	link.atlas = "2048 x 2576, 5 levels, Minecraft's";
	link.updateMsAverage = 0.05;
	link.updateMsWorst = 1.2;
	link.hudSetting = 2;
	link.hudWanted = true;
	link.hudReceived = 60;
	link.hudMb = 210.9375;
	link.hudDrawn = 59;
	link.hudSize = "1280 x 720, rows bottom-up";
	link.hudCopyMsAverage = 0.4;
	link.hudGpuMsAverage = 0.05;
	link.latencyLast = cpmc::LatencySample{ 120.0, 25.0, 2.0 };
	link.latencyAverage = cpmc::LatencySample{ 110.0, 22.0, 1.7 };
	link.latencySamples = 3;
	link.burst = cpmc::BurstReport{ "explosion", 61, 25, 5'000'000, 1.5, 6.0, 30.0 };
	link.cyberpunkCpu = 0.85;
	link.cyberpunkMemory = 6'100ull << 20;
	link.minecraftProcessKnown = true;
	link.minecraftCpu = 0.4;
	link.minecraftMemory = 1'900ull << 20;
	report.link = link;
	report.minecraftBlocks = true;
	report.sections = 81;
	report.vertices = 123'456;
	const std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "blocks=Minecraft's in 81 sections, 123456 vertices"));
	CHECK(Has(status, "link=linked; the mapping created; Minecraft process 9052, heartbeat 16 ms ago, in its world, 60 frames/s; in game"));
	CHECK(Has(status, "positions=Minecraft's eye -119.50 7.76 2007.50; V's camera -119.50 7.76 2007.50 (Minecraft axes); "
	                  "3 teleports in the last second, the last to -119.50 6.14 2007.50; floor 2 at height 6.14 around -119.5 2007.5"));
	CHECK(Has(status, "ring=7.52 MB in the last second, at most 0.15 MB in one frame; drain 0.120 ms average, 0.900 ms worst; "
	                  "812 atlas patch (7.52 MB), 3 section (0.01 MB); 0 rejected, 0 resyncs"));
	CHECK(Has(status, "atlas=2048 x 2576, 5 levels, Minecraft's; 0 translucent triangles dropped; update 0.050 ms average, "
	                  "1.200 ms worst"));
	CHECK(Has(status, "hud=always, scale 2, showing; 60 images received (210.9 MB), 0 dropped and 59 drawn in the last second, "
	                  "1280 x 720, rows bottom-up; copy 0.400 ms average; pass 0.050 ms average on the GPU; 0 skipped; "
	                  "0 refused since the start"));
	CHECK(Has(status, "latency=last: Minecraft 120 ms, ours 25.0 ms (2.0 frames); average of 3: Minecraft 110 ms, ours 22.0 ms "
	                  "(1.7 frames)"));
	CHECK(Has(status, "burst=explosion: 25 sections, 4.77 MB over 61 frames; worst drain 1.500 ms, update 6.000 ms, frame 30.0 ms"));
	CHECK(Has(status, "processes=Cyberpunk 85% of a core, 6100 MB; Minecraft 40% of a core, 1900 MB"));
}

TEST(StatusWritesTheLinkWithMinecraftAbsent)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	link.mapping = "refused: another game side (process 12) is using it";
	link.hudSkipped = 4;
	link.hudLastSkip = "no image yet";
	link.hudRefused = 2;
	link.cyberpunkCpu = 0.5;
	link.cyberpunkMemory = 100ull << 20;
	report.link = link;
	const std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "link=not linked; the mapping refused: another game side (process 12) is using it; Minecraft absent; not in game"));
	CHECK(Has(status, "positions=Minecraft's eye unknown; V's camera unknown; 0 teleports in the last second; floor not sent"));
	CHECK(Has(status, "atlas=none yet; 0 translucent triangles dropped; update not measured"));
	CHECK(Has(status, "hud=in build mode, scale 2, not showing; 0 images received (0.0 MB), 0 dropped and 0 drawn in the last "
	                  "second, no image yet; copy not measured; pass not measured; 4 skipped (no image yet); 2 refused since the "
	                  "start"));
	CHECK(Has(status, "latency=no samples yet: type a command while V stands still"));
	CHECK(Has(status, "burst=none yet"));
	CHECK(Has(status, "processes=Cyberpunk 50% of a core, 100 MB; Minecraft not running"));
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'LinkReport': is not a member of 'cpmc'`.

- [x] **Step 3: Edit `src/Status.h` to add the link's report**

```cpp
#include "Coords.h"

#include <cstdint>
```
```cpp
#include "Coords.h"
#include "Measurements.h"

#include <cstdint>
#include <optional>
```
```cpp
// folder; tools/blockrender_hud/init.lua shows it.
```
```cpp
// folder; tools/blockrender_hud/init.lua shows it.
// Spike 4's lines (the spec's "Measurements"): the link, the render ring, Minecraft's atlas, the HUD, latency, the last
// burst, and both games' processes. "In the last second" is from one status to the next.
struct LinkReport
{
	std::string          mapping = "not opened yet";  // created, reused, or refused and why
	bool                 linked = false;
	bool                 minecraft = false;  // its heartbeat is under 3 s old
	uint32_t             minecraftPid = 0;
	uint64_t             heartbeatAgeMs = 0;
	bool                 inWorld = false;
	bool                 screenOpen = false;
	double               minecraftFps = 0.0;  // its frame counter's, over the last second
	bool                 inGame = false;      // linked, with V found
	Vec3d                minecraftEye;        // Minecraft axes
	std::optional<Vec3d> camera;              // V's, Minecraft axes; none while V isn't found
	uint64_t             teleports = 0;       // in the last second
	Vec3d                target;              // the last teleport's, Minecraft axes
	uint32_t             floorEpoch = 0;
	bool                 floorSent = false;
	Vec3d                floorCentre;  // Minecraft axes; y is the floor's height
	// The render ring
	double      ringMb = 0.0;            // in the last second
	double      ringMbWorstFrame = 0.0;  // the most in one frame of it
	double      drainMsAverage = 0.0;
	double      drainMsWorst = 0.0;
	std::string ringTypes;     // messages and MB by type, in the last second; empty: none
	uint64_t    rejected = 0;  // since the start
	uint64_t    resyncs = 0;
	// Minecraft's blocks
	std::string atlas = "none yet";
	uint64_t    translucentDropped = 0;  // since the start
	double      updateMsAverage = -1.0;  // the renderer's Update; below 0: not measured
	double      updateMsWorst = -1.0;
	// The HUD
	int         hudSetting = 1;  // 0 never, 1 in build mode, 2 always
	uint32_t    hudScale = 2;
	bool        hudWanted = false;
	uint64_t    hudReceived = 0;  // in the last second
	double      hudMb = 0.0;      // of the images received
	uint64_t    hudDropped = 0;   // with no upload buffer free
	uint64_t    hudDrawn = 0;
	std::string hudSize;                  // the newest image's; empty: none yet
	double      hudCopyMsAverage = -1.0;  // SetHudImage's copy; below 0: not measured
	double      hudGpuMsAverage = -1.0;   // the pass's
	uint64_t    hudSkipped = 0;           // in the last second
	std::string hudLastSkip;
	uint64_t    hudRefused = 0;  // by the link, since the start: a bad slot or size
	// Latency, the last burst, and the processes
	std::optional<LatencySample> latencyLast;
	std::optional<LatencySample> latencyAverage;
	size_t                       latencySamples = 0;
	std::optional<BurstReport>   burst;
	double                       cyberpunkCpu = 0.0;   // a share of one core
	uint64_t                     cyberpunkMemory = 0;  // its working set, bytes
	bool                         minecraftProcessKnown = false;
	double                       minecraftCpu = 0.0;
	uint64_t                     minecraftMemory = 0;
};

```
```cpp
	size_t                 blocks = 0;
```
```cpp
	size_t                 blocks = 0;
	bool                   minecraftBlocks = false;  // the sections are Minecraft's, whose blocks aren't counted
```
```cpp
	std::string            lastRequest;
	std::vector<Structure> structures;
};

// Lines of "key=value": mode, frames, blocks, gpu, frame, streamline, hooks, request, then one structure line per
// structure: "structure=<name> <min x> <min y> <min z> <max x> <max y> <max z>".
```
```cpp
	std::string               lastRequest;
	std::vector<Structure>    structures;
	std::optional<LinkReport> link;  // spike 4's lines, once the plugin has tried the mapping
};

// Lines of "key=value": mode, frames, blocks, gpu, frame, streamline, hooks, request; with a link, its lines (link,
// positions, ring, atlas, hud, latency, burst and processes); then one structure line per structure:
// "structure=<name> <min x> <min y> <min z> <max x> <max y> <max z>".
```

- [x] **Step 4: Edit `src/Status.cpp` to write its lines**

```cpp
	return aText;
}
}
```
```cpp
	return aText;
}

std::string Position(const Vec3d& aPosition)
{
	return std::format("{:.2f} {:.2f} {:.2f}", aPosition.x, aPosition.y, aPosition.z);
}

std::string MsAverage(double aAverage, const char* aWhere = "")
{
	return aAverage < 0.0 ? "not measured" : std::format("{:.3f} ms average{}", aAverage, aWhere);
}

std::string Latency(const LatencySample& aSample)
{
	return std::format("Minecraft {:.0f} ms, ours {:.1f} ms ({:.1f} frames)", aSample.minecraftMs, aSample.oursMs, aSample.oursFrames);
}

std::string FormatLink(const LinkReport& aLink)
{
	std::string       out;
	const std::string minecraft = aLink.minecraft
	                                  ? std::format("Minecraft process {}, heartbeat {} ms ago{}{}, {:.0f} frames/s", aLink.minecraftPid,
	                                                aLink.heartbeatAgeMs, aLink.inWorld ? ", in its world" : "",
	                                                aLink.screenOpen ? ", a screen open" : "", aLink.minecraftFps)
	                                  : "Minecraft absent";
	out += std::format("link={}; the mapping {}; {}; {}\n", aLink.linked ? "linked" : "not linked", OneLine(aLink.mapping), minecraft,
	                   aLink.inGame ? "in game" : "not in game");
	const std::string floor = aLink.floorSent ? std::format("floor {} at height {:.2f} around {:.1f} {:.1f}", aLink.floorEpoch,
	                                                        aLink.floorCentre.y, aLink.floorCentre.x, aLink.floorCentre.z)
	                                          : "floor not sent";
	out += std::format("positions=Minecraft's eye {}; V's camera {}{}; {} teleports in the last second{}; {}\n",
	                   aLink.minecraft ? Position(aLink.minecraftEye) : "unknown", aLink.camera ? Position(*aLink.camera) : "unknown",
	                   aLink.minecraft || aLink.camera ? " (Minecraft axes)" : "", aLink.teleports,
	                   aLink.inGame || aLink.teleports > 0 ? ", the last to " + Position(aLink.target) : "", floor);
	out += std::format("ring={:.2f} MB in the last second, at most {:.2f} MB in one frame; drain {:.3f} ms average, {:.3f} ms worst; {}; "
	                   "{} rejected, {} resyncs\n",
	                   aLink.ringMb, aLink.ringMbWorstFrame, aLink.drainMsAverage, aLink.drainMsWorst,
	                   aLink.ringTypes.empty() ? "no messages" : aLink.ringTypes, aLink.rejected, aLink.resyncs);
	out += std::format("atlas={}; {} translucent triangles dropped; update {}\n", aLink.atlas, aLink.translucentDropped,
	                   Ms(aLink.updateMsAverage, aLink.updateMsWorst));
	out += std::format("hud={}, scale {}, {}; {} images received ({:.1f} MB), {} dropped and {} drawn in the last second, {}; "
	                   "copy {}; pass {}; {} skipped{}; {} refused since the start\n",
	                   aLink.hudSetting == 0 ? "never" : aLink.hudSetting == 2 ? "always" : "in build mode", aLink.hudScale,
	                   aLink.hudWanted ? "showing" : "not showing", aLink.hudReceived, aLink.hudMb, aLink.hudDropped, aLink.hudDrawn,
	                   aLink.hudSize.empty() ? "no image yet" : aLink.hudSize, MsAverage(aLink.hudCopyMsAverage),
	                   MsAverage(aLink.hudGpuMsAverage, " on the GPU"), aLink.hudSkipped,
	                   aLink.hudLastSkip.empty() ? "" : " (" + OneLine(aLink.hudLastSkip) + ")", aLink.hudRefused);
	out += aLink.latencyLast && aLink.latencyAverage
	           ? std::format("latency=last: {}; average of {}: {}\n", Latency(*aLink.latencyLast), aLink.latencySamples,
	                         Latency(*aLink.latencyAverage))
	           : std::string("latency=no samples yet: type a command while V stands still\n");
	out += aLink.burst ? std::format("burst={}: {} sections, {:.2f} MB over {} frames; worst drain {:.3f} ms, update {:.3f} ms, "
	                                 "frame {:.1f} ms\n",
	                                 OneLine(aLink.burst->what), aLink.burst->sections, double(aLink.burst->bytes) / 1048576.0,
	                                 aLink.burst->frames, aLink.burst->drainMsWorst, aLink.burst->updateMsWorst, aLink.burst->frameMsWorst)
	                   : std::string("burst=none yet\n");
	out += std::format("processes=Cyberpunk {:.0f}% of a core, {} MB; {}\n", aLink.cyberpunkCpu * 100.0, aLink.cyberpunkMemory >> 20,
	                   aLink.minecraftProcessKnown
	                       ? std::format("Minecraft {:.0f}% of a core, {} MB", aLink.minecraftCpu * 100.0, aLink.minecraftMemory >> 20)
	                       : std::string("Minecraft not running"));
	return out;
}
}
```
```cpp
	out += std::format("blocks={} in {} sections, {} vertices\n", aReport.blocks, aReport.sections, aReport.vertices);
```
```cpp
	out += std::format("blocks={} in {} sections, {} vertices\n",
	                   aReport.minecraftBlocks ? std::string("Minecraft's") : std::to_string(aReport.blocks), aReport.sections,
	                   aReport.vertices);
```
```cpp
	out += std::format("request={}\n", OneLine(aReport.lastRequest));
```
```cpp
	out += std::format("request={}\n", OneLine(aReport.lastRequest));
	if (aReport.link)
	{
		out += FormatLink(*aReport.link);
	}
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `203 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: the link's lines in the status" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 22: The renderer takes Minecraft's atlas and sections, and times them

The renderer's side of the spec's "Blocks" and "Measurements", without the HUD (Task 23):
- **`SetAtlas`** replaces whatever atlas is loaded, `atlas.bin`'s or Minecraft's, under the lock, and the next
  `Update` uploads it. `AtlasData()`'s pointer was good for the plugin's life. Now it's good until the next
  `LoadAtlas` or `SetAtlas`, so the main thread fetches it again each time.
- **Sections wait in `PendingSections`** (Task 14), only the latest of each. **`ClearSections`** drops every section,
  and every upload waiting.
- **`Update` times itself:** what it did before is now `Upload`, and its CPU time goes into the stats, with the
  uploads waiting and the atlas's size and levels.
- **The timed upload, for latency:** `TimeUpload` names a section. Its next upload notes when its list was submitted,
  and the first list that draws it notes when, and the game's frame count then (`Presents()`, counted at each
  Present). `TakeTimedUpload` hands that on once, after it's drawn. All on `SteadySeconds` (Task 18).

**Files:**
- Modify: `src/render/Renderer.h`, `src/render/Renderer.cpp`

- [x] **Step 1: Edit `src/render/Renderer.h`**

```cpp
#include "render/Gpu.h"
```
```cpp
#include "render/Gpu.h"
#include "render/PendingSections.h"
```
```cpp
	const Atlas* AtlasData() const;               // null until loaded; it stays valid, since the atlas is loaded once
```
```cpp
	void         SetAtlas(Atlas aAtlas);          // Minecraft's, say: replaces any atlas, and is uploaded at the next Update
	// Null until loaded. Valid until the next LoadAtlas or SetAtlas, so fetch it again each time it's needed.
	const Atlas* AtlasData() const;
```
```cpp
	void         Update();  // uploads, and frees what the GPU has finished with
```
```cpp
	void         ClearSections();  // every section, and every upload waiting
	void         Update();         // uploads, and frees what the GPU has finished with

	// Latency (the spec's "Measurements"): the next upload of aSection is timed, from its submission to the first list
	// recorded that draws it. It replaces any section still being timed.
	struct TimedUpload
	{
		double   submitted = 0.0;  // SteadySeconds()
		double   drawn = 0.0;
		uint64_t drawnFrame = 0;  // Presents() then
	};
	void                       TimeUpload(const SectionPos& aSection);
	std::optional<TimedUpload> TakeTimedUpload();  // once it has been drawn; then it's done
	uint64_t                   Presents() const { return presents_.load(); }  // the game's frames so far
```
```cpp
		size_t      vertices = 0;
```
```cpp
		size_t      vertices = 0;
		double      updateMsAverage = -1.0;  // Update's CPU time
		double      updateMsWorst = -1.0;
		size_t      pending = 0;  // uploads waiting
		uint32_t    atlasWidth = 0;  // 0 before there's an atlas
		uint32_t    atlasHeight = 0;
		uint32_t    atlasLevels = 0;
```
```cpp
	// All with the mutex held.
```
```cpp
	// All with the mutex held.
	void               Upload();         // Update's work
```
```cpp
	std::vector<std::pair<SectionPos, std::vector<Vertex>>>        pending_;
	std::vector<std::pair<gpu::ComPtr<ID3D12Resource>, uint64_t>> retired_;
```
```cpp
	PendingSections                                                pending_;
	std::vector<std::pair<gpu::ComPtr<ID3D12Resource>, uint64_t>> retired_;

	struct UploadWatch  // the section TimeUpload times
	{
		SectionPos            section;
		uint64_t              ready = 0;  // its upload's fence value, once submitted
		std::optional<double> submitted;
		std::optional<double> drawn;
		uint64_t              drawnFrame = 0;
	};
	std::optional<UploadWatch> watch_;
	std::atomic<uint64_t>      presents_{ 0 };
```
```cpp
	int64_t  lastPresent_ = 0;
```
```cpp
	int64_t  lastPresent_ = 0;
	double   updateMsSum_ = 0.0;
	uint64_t updateSamples_ = 0;
```

- [x] **Step 2: Edit `src/render/Renderer.cpp`**

```cpp
#include "hooks/D3D12Hooks.h"
```
```cpp
#include "Measurements.h"
#include "hooks/D3D12Hooks.h"
```
```cpp
#include <algorithm>
```
```cpp
#include <algorithm>
#include <chrono>
```
```cpp
const Atlas* Renderer::AtlasData() const
```
```cpp
void Renderer::SetAtlas(Atlas aAtlas)
{
	std::lock_guard lock(mutex_);
	atlas_ = std::move(aAtlas);
	atlasUploaded_ = false;
}

const Atlas* Renderer::AtlasData() const
```
```cpp
	pending_.emplace_back(aSection, std::move(aVertices));
```
```cpp
	pending_.Put(aSection, std::move(aVertices));
}

void Renderer::ClearSections()
{
	std::lock_guard lock(mutex_);
	for (auto& [section, gpuSection] : sections_)
	{
		Retire(gpuSection.buffer);
	}
	sections_.clear();
	pending_.Clear();
}

void Renderer::TimeUpload(const SectionPos& aSection)
{
	std::lock_guard lock(mutex_);
	watch_ = UploadWatch{ aSection };
}

std::optional<Renderer::TimedUpload> Renderer::TakeTimedUpload()
{
	std::lock_guard lock(mutex_);
	if (!watch_ || !watch_->submitted || !watch_->drawn)
	{
		return std::nullopt;
	}
	const TimedUpload timed{ *watch_->submitted, *watch_->drawn, watch_->drawnFrame };
	watch_.reset();
	return timed;
```
```cpp
{
	std::lock_guard lock(mutex_);
	if ((!fence_ && mode_.load() == Mode::Off) || !EnsureObjects())
```
```cpp
{
	const auto      start = std::chrono::steady_clock::now();
	std::lock_guard lock(mutex_);
	Upload();
	const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	updateMsSum_ += ms;
	++updateSamples_;
	stats_.updateMsWorst = std::max(stats_.updateMsWorst, ms);
}

void Renderer::Upload()
{
	if ((!fence_ && mode_.load() == Mode::Off) || !EnsureObjects())
```
```cpp
	if ((pending_.empty() && !atlasDue) || completed < uploadFence_)
```
```cpp
	if ((pending_.Size() == 0 && !atlasDue) || completed < uploadFence_)
```
```cpp
	for (auto& [section, vertices] : pending_)
```
```cpp
	bool watched = false;
	for (auto& [section, vertices] : pending_.Take())
```
```cpp
			sections_.erase(found);
```
```cpp
			sections_.erase(found);
		}
		if (watch_ && !watch_->submitted && watch_->section == section)
		{
			watch_->ready = ready;
			watched = true;
```
```cpp
	}
	pending_.clear();
```
```cpp
	}
```
```cpp
	uploadFence_ = Signal();
```
```cpp
	uploadFence_ = Signal();
	if (watched)
	{
		watch_->submitted = SteadySeconds();
	}
```
```cpp
	stats.frameMsAverage = frameSamples_ ? frameMsSum_ / double(frameSamples_) : -1.0;
```
```cpp
	stats.frameMsAverage = frameSamples_ ? frameMsSum_ / double(frameSamples_) : -1.0;
	stats.updateMsAverage = updateSamples_ ? updateMsSum_ / double(updateSamples_) : -1.0;
	stats.pending = pending_.Size();
	if (atlas_)
	{
		stats.atlasWidth = atlas_->width;
		stats.atlasHeight = atlas_->height;
		stats.atlasLevels = atlas_->Levels();
	}
```
```cpp
	gpuMsSum_ = frameMsSum_ = 0.0;
	gpuSamples_ = frameSamples_ = 0;
```
```cpp
	gpuMsSum_ = frameMsSum_ = updateMsSum_ = 0.0;
	gpuSamples_ = frameSamples_ = updateSamples_ = 0;
```
```cpp
		{
			continue;
		}
		const float sectionConstants[4] = { float(offset.x), float(offset.y), float(offset.z), 0.0f };
```
```cpp
		{
			continue;
		}
		if (watch_ && watch_->submitted && !watch_->drawn && watch_->section == section && watch_->ready == gpuSection.ready)
		{
			watch_->drawn = SteadySeconds();  // the first list that draws the timed upload
			watch_->drawnFrame = presents_.load();
		}
		const float sectionConstants[4] = { float(offset.x), float(offset.y), float(offset.z), 0.0f };
```
```cpp
	const int64_t   now = Now();
```
```cpp
	const int64_t now = Now();
	++presents_;
```
```cpp
	pending_.clear();
```
```cpp
	pending_.Clear();
	watch_.reset();
```

- [x] **Step 3: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `203 test(s), 0 failure(s)`.

- [x] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: the renderer takes Minecraft's atlas and sections, and times them" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 23: The HUD pass

The spec's "The HUD", in the renderer:
- **`SetHud`** says whether the pass draws at Present, in any drawing mode. The first time it's wanted, it compiles
  the shaders if drawing hasn't yet.
- **`SetHudImage`,** on the main thread, copies an image into one of 3 upload buffers through `HudSlots` (Task 15).
  It takes a slot under the lock, makes or grows the slot's buffer, which stays mapped, copies without the lock, and
  marks the slot the newest. It counts images received and dropped, their bytes, and the copy's time.
- **`BackBufferSize`** is the back buffer's size as the last Present saw it, for the size to ask Minecraft for.
- **The pass** runs at Present, after spike 3's drawing there, on the main queue only:
  1. it takes the newest image, if one has come since, and copies it into a buffer in GPU memory, grown as needed;
  2. it draws one triangle over the back buffer with Task 16's shaders and premultiplied blending (one, and one minus
     the source's alpha), without depth, at the scale `HudScaleFor` gives the image;
  3. timestamps give its GPU time, and a fence says when the GPU is done with the slot.

  A frame it can't draw (no image yet, another queue, no back buffer) is skipped and counted with the reason, apart
  from the blocks' skips. It never switches block drawing off.
- **`Shutdown`** releases the HUD's buffers with the rest.

**Files:**
- Modify: `src/render/Renderer.h`, `src/render/Renderer.cpp`

- [x] **Step 1: Edit `src/render/Renderer.h`**

```cpp
#include "render/Gpu.h"
```
```cpp
#include "render/Gpu.h"
#include "render/Hud.h"
```
```cpp
	struct Stats
```
```cpp
	// The HUD (the spec's "The HUD"), from the main thread. SetHud says whether the HUD pass draws at Present, in any
	// drawing mode; the first time it's wanted, the shaders are compiled if drawing hasn't. SetHudImage copies an image
	// (RGBA8, premultiplied alpha) into an upload buffer the GPU has finished with, for the next pass: false, and
	// counted as dropped, if none is free, or before the renderer has made its objects.
	void    SetHud(bool aWanted);
	bool    SetHudImage(uint32_t aWidth, uint32_t aHeight, bool aBottomUp, const uint8_t* aRgba);
	HudSize BackBufferSize() const { return { backWidth_.load(), backHeight_.load() }; }  // as the last Present saw it

	struct Stats
```
```cpp
		uint32_t    atlasLevels = 0;
```
```cpp
		uint32_t    atlasLevels = 0;
		uint64_t    hudReceived = 0;  // images SetHudImage took
		uint64_t    hudDropped = 0;   // and couldn't
		uint64_t    hudBytes = 0;     // of the images taken
		uint64_t    hudDrawn = 0;     // HUD passes
		uint64_t    hudSkipped = 0;
		std::string hudLastSkip;
		double      hudCopyMsAverage = -1.0;  // SetHudImage's copy
		double      hudGpuMsAverage = -1.0;   // the pass, on the GPU
		uint32_t    hudWidth = 0;  // the image drawn; 0 before one
		uint32_t    hudHeight = 0;
		bool        hudBottomUp = false;
```
```cpp
	// All with the mutex held.
```
```cpp
	// All with the mutex held.
	void               PresentBlocks(IDXGISwapChain* aSwapChain);  // mode present: the blocks, over the finished frame
	void               PresentHud(IDXGISwapChain* aSwapChain);     // Minecraft's HUD, over that
	bool               OnMainQueue(IDXGISwapChain* aSwapChain);    // whether the swap chain presents on the main queue
	bool               EnsureHudObjects();                         // the HUD pass's lists and timing queries
	bool               EnsureHudPipeline(DXGI_FORMAT aFormat);
	void               HudSkip(const std::string& aWhy);  // counted apart: the HUD never switches drawing off
	void               ReadHudTimings(uint32_t aSlot);
```
```cpp
	uint64_t updateSamples_ = 0;
```
```cpp
	uint64_t updateSamples_ = 0;

	// The HUD (SetHud, SetHudImage and the HUD pass)
	struct HudImage
	{
		uint32_t width = 0;
		uint32_t height = 0;
		bool     bottomUp = false;
	};
	std::atomic<bool>                                          hudWanted_{ false };
	bool                                                       hudShadersFailed_ = false;  // main thread: not again until a reload
	std::atomic<uint32_t>                                      backWidth_{ 0 };
	std::atomic<uint32_t>                                      backHeight_{ 0 };
	HudSlots                                                   hudSlots_;
	std::array<gpu::ComPtr<ID3D12Resource>, HudSlots::kSlots> hudUploads_;
	std::array<uint8_t*, HudSlots::kSlots>                     hudMapped_{};
	std::array<uint64_t, HudSlots::kSlots>                     hudUploadBytes_{};
	std::array<HudImage, HudSlots::kSlots>                     hudSlotImages_{};
	gpu::ComPtr<ID3D12Resource>                                hudImage_;  // in GPU memory: what the pass draws
	uint64_t                                                   hudImageBytes_ = 0;
	bool                                                       hudImageReadable_ = false;  // copied into, and a shader resource
	HudImage                                                   hudShown_;
	gpu::ComPtr<ID3D12PipelineState>                           hudPipeline_;
	DXGI_FORMAT                                                hudPipelineFormat_ = DXGI_FORMAT_UNKNOWN;
	std::array<gpu::ComPtr<ID3D12CommandAllocator>, kSlots>    hudAllocators_;
	std::array<gpu::ComPtr<ID3D12GraphicsCommandList>, kSlots> hudLists_;
	std::array<uint64_t, kSlots>                               hudSlotFence_{};
	uint64_t                                                   hudRecordings_ = 0;
	gpu::ComPtr<ID3D12QueryHeap>                               hudQueries_;
	gpu::ComPtr<ID3D12Resource>                                hudTimings_;  // readback: kSlots x 2 timestamps
	uint64_t*                                                  hudTimingsMapped_ = nullptr;
	double                                                     hudCopyMsSum_ = 0.0;
	uint64_t                                                   hudCopySamples_ = 0;
	double                                                     hudGpuMsSum_ = 0.0;
	uint64_t                                                   hudGpuSamples_ = 0;
```

- [x] **Step 2: Edit `src/render/Renderer.cpp`**

```cpp
	depthPipeline_.Reset();
	return true;
```
```cpp
	depthPipeline_.Reset();
	hudPipeline_.Reset();
	hudShadersFailed_ = false;
	return true;
}

void Renderer::SetHud(bool aWanted)
{
	if (aWanted && !hudWanted_.load())
	{
		bool haveShaders = false;
		{
			std::lock_guard lock(mutex_);
			haveShaders = shaders_.has_value();
		}
		std::string error;
		if (!haveShaders && (hudShadersFailed_ || !ReloadShaders(error)))
		{
			if (!hudShadersFailed_)
			{
				hudShadersFailed_ = true;
				log_.Push("renderer: the HUD needs the shaders, which didn't compile: " + error);
			}
			return;
		}
	}
	hudWanted_ = aWanted;
}

bool Renderer::SetHudImage(uint32_t aWidth, uint32_t aHeight, bool aBottomUp, const uint8_t* aRgba)
{
	const uint64_t bytes = uint64_t(aWidth) * aHeight * 4;
	uint32_t       slot = 0;
	{
		std::lock_guard               lock(mutex_);
		const std::optional<uint32_t> writable = fence_ && bytes > 0 ? hudSlots_.BeginWrite(fence_->GetCompletedValue()) : std::nullopt;
		if (!writable)
		{
			++stats_.hudDropped;
			return false;
		}
		slot = *writable;
		if (hudUploadBytes_[slot] < bytes)
		{
			// The GPU has finished with this slot, so its buffer can go at once.
			if (hudMapped_[slot])
			{
				hudUploads_[slot]->Unmap(0, nullptr);
				hudMapped_[slot] = nullptr;
			}
			hudUploadBytes_[slot] = 0;
			hudUploads_[slot] = gpu::MakeBuffer(device_, bytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
			void* mapped = nullptr;
			if (!hudUploads_[slot] || FAILED(hudUploads_[slot]->Map(0, nullptr, &mapped)))
			{
				hudUploads_[slot].Reset();
				hudSlots_.Abandon(slot);
				++stats_.hudDropped;
				return false;
			}
			hudUploads_[slot]->SetName(L"BlockRender HUD upload");
			hudMapped_[slot] = static_cast<uint8_t*>(mapped);
			hudUploadBytes_[slot] = bytes;
		}
	}
	// Without the lock: the slot is ours until EndWrite.
	const auto start = std::chrono::steady_clock::now();
	std::memcpy(hudMapped_[slot], aRgba, size_t(bytes));
	const double    ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	std::lock_guard lock(mutex_);
	hudSlotImages_[slot] = { aWidth, aHeight, aBottomUp };
	hudSlots_.EndWrite(slot);
	++stats_.hudReceived;
	stats_.hudBytes += bytes;
	hudCopyMsSum_ += ms;
	++hudCopySamples_;
	return true;
```
```cpp
	stats.updateMsAverage = updateSamples_ ? updateMsSum_ / double(updateSamples_) : -1.0;
```
```cpp
	stats.updateMsAverage = updateSamples_ ? updateMsSum_ / double(updateSamples_) : -1.0;
	stats.hudCopyMsAverage = hudCopySamples_ ? hudCopyMsSum_ / double(hudCopySamples_) : -1.0;
	stats.hudGpuMsAverage = hudGpuSamples_ ? hudGpuMsSum_ / double(hudGpuSamples_) : -1.0;
	stats.hudWidth = hudShown_.width;
	stats.hudHeight = hudShown_.height;
	stats.hudBottomUp = hudShown_.bottomUp;
```
```cpp
	gpuMsSum_ = frameMsSum_ = updateMsSum_ = 0.0;
	gpuSamples_ = frameSamples_ = updateSamples_ = 0;
```
```cpp
	gpuMsSum_ = frameMsSum_ = updateMsSum_ = hudCopyMsSum_ = hudGpuMsSum_ = 0.0;
	gpuSamples_ = frameSamples_ = updateSamples_ = hudCopySamples_ = hudGpuSamples_ = 0;
```
```cpp
	++presents_;
```
```cpp
	++presents_;
	DXGI_SWAP_CHAIN_DESC swapChainDesc{};
	if (SUCCEEDED(aSwapChain->GetDesc(&swapChainDesc)))
	{
		backWidth_ = swapChainDesc.BufferDesc.Width;
		backHeight_ = swapChainDesc.BufferDesc.Height;
	}
```
```cpp
	if (mode_.load() != Mode::Present)
	{
		return;
	}
```
```cpp
	if (mode_.load() == Mode::Present)
	{
		PresentBlocks(aSwapChain);
	}
	if (hudWanted_.load())
	{
		PresentHud(aSwapChain);
	}
}

bool Renderer::OnMainQueue(IDXGISwapChain* aSwapChain)
{
	if (aSwapChain != checkedSwapChain_)
	{
		checkedSwapChain_ = aSwapChain;
		swapChainOnMainQueue_ = d3d::PresentQueue(aSwapChain) == queue_;
		log_.Push(std::format("renderer: swap chain 0x{:x} presents on {}", reinterpret_cast<uintptr_t>(aSwapChain),
		                      swapChainOnMainQueue_ ? "the main queue" : "another queue"));
	}
	return swapChainOnMainQueue_;
}

void Renderer::PresentBlocks(IDXGISwapChain* aSwapChain)
{
```
```cpp
	if (aSwapChain != checkedSwapChain_)
	{
		checkedSwapChain_ = aSwapChain;
		swapChainOnMainQueue_ = d3d::PresentQueue(aSwapChain) == queue_;
		log_.Push(std::format("renderer: swap chain 0x{:x} presents on {}", reinterpret_cast<uintptr_t>(aSwapChain),
		                      swapChainOnMainQueue_ ? "the main queue" : "another queue"));
	}
	if (!swapChainOnMainQueue_)
```
```cpp
	if (!OnMainQueue(aSwapChain))
```
```cpp
void Renderer::Shutdown()
```
```cpp
bool Renderer::EnsureHudObjects()
{
	if (hudTimingsMapped_)
	{
		return true;
	}
	const D3D12_QUERY_HEAP_DESC queryDesc{ D3D12_QUERY_HEAP_TYPE_TIMESTAMP, kSlots * 2, 0 };
	if (!hudTimings_)
	{
		hudTimings_ = gpu::MakeBuffer(device_, kSlots * 2 * sizeof(uint64_t), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
	}
	if (!hudTimings_ || (!hudQueries_ && FAILED(device_->CreateQueryHeap(&queryDesc, IID_PPV_ARGS(&hudQueries_)))))
	{
		return false;
	}
	for (uint32_t slot = 0; slot < kSlots; ++slot)
	{
		if (hudLists_[slot])
		{
			continue;
		}
		if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&hudAllocators_[slot]))) ||
		    FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, hudAllocators_[slot].Get(), nullptr,
		                                      IID_PPV_ARGS(&hudLists_[slot]))))
		{
			hudLists_[slot].Reset();
			return false;
		}
		d3d::Original().close(hudLists_[slot].Get());
		hudLists_[slot]->SetName(L"BlockRender HUD");
	}
	void* mapped = nullptr;
	if (FAILED(hudTimings_->Map(0, nullptr, &mapped)))
	{
		return false;
	}
	hudTimings_->SetName(L"BlockRender HUD timings");
	hudTimingsMapped_ = static_cast<uint64_t*>(mapped);  // last: mapped timings mean everything is ready
	return true;
}

bool Renderer::EnsureHudPipeline(DXGI_FORMAT aFormat)
{
	if (hudPipeline_ && hudPipelineFormat_ == aFormat)
	{
		return true;
	}
	if (!shaders_)
	{
		return false;
	}
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.VS = { shaders_->hudVertex.data(), shaders_->hudVertex.size() };
	desc.PS = { shaders_->hud.data(), shaders_->hud.size() };
	D3D12_RENDER_TARGET_BLEND_DESC& blend = desc.BlendState.RenderTarget[0];
	blend.BlendEnable = TRUE;  // premultiplied alpha, as SkyCraft composites the image
	blend.SrcBlend = D3D12_BLEND_ONE;
	blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blend.BlendOp = D3D12_BLEND_OP_ADD;
	blend.SrcBlendAlpha = D3D12_BLEND_ONE;
	blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blend.LogicOp = D3D12_LOGIC_OP_NOOP;
	blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = BlockRasterizer(0);  // one triangle: nothing to cull
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = aFormat;
	desc.SampleDesc.Count = 1;
	gpu::ComPtr<ID3D12PipelineState> pipeline;
	if (FAILED(device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline))))
	{
		return false;
	}
	pipeline->SetName(L"BlockRender HUD pipeline");
	hudPipeline_ = pipeline;
	hudPipelineFormat_ = aFormat;
	return true;
}

void Renderer::HudSkip(const std::string& aWhy)
{
	++stats_.hudSkipped;
	stats_.hudLastSkip = aWhy;
}

void Renderer::ReadHudTimings(uint32_t aSlot)
{
	const uint64_t begin = hudTimingsMapped_[aSlot * 2];
	const uint64_t end = hudTimingsMapped_[aSlot * 2 + 1];
	if (end > begin && timestampFrequency_ > 0)
	{
		hudGpuMsSum_ += double(end - begin) * 1000.0 / double(timestampFrequency_);
		++hudGpuSamples_;
	}
}

void Renderer::PresentHud(IDXGISwapChain* aSwapChain)
{
	if (!EnsureObjects() || !EnsureHudObjects())
	{
		return HudSkip("not ready: the main queue isn't known yet");
	}
	const uint64_t completed = fence_->GetCompletedValue();
	if (completed == UINT64_MAX)
	{
		return HudSkip("the device was removed");
	}
	if (!OnMainQueue(aSwapChain))
	{
		return HudSkip("the swap chain presents on another queue than the main one");
	}
	gpu::ComPtr<IDXGISwapChain3> swapChain;
	gpu::ComPtr<ID3D12Resource>  backBuffer;
	if (FAILED(aSwapChain->QueryInterface(IID_PPV_ARGS(&swapChain))) ||
	    FAILED(swapChain->GetBuffer(swapChain->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&backBuffer))))
	{
		return HudSkip("no back buffer");
	}
	const D3D12_RESOURCE_DESC target = backBuffer->GetDesc();
	if (!EnsureHudPipeline(target.Format))
	{
		return HudSkip(std::format("no HUD pipeline for {}", DxgiFormatName(target.Format)));
	}
	const uint32_t slot = uint32_t(hudRecordings_ % kSlots);
	if (completed < hudSlotFence_[slot])
	{
		return HudSkip("the GPU is still busy with an earlier frame");
	}
	if (hudSlotFence_[slot] != 0)
	{
		ReadHudTimings(slot);
	}
	const std::optional<uint32_t> taken = hudSlots_.TakeNewest();
	if (!taken && !hudImageReadable_)
	{
		return HudSkip("no image yet");
	}
	uint64_t copyBytes = 0;
	if (taken)
	{
		const HudImage& image = hudSlotImages_[*taken];
		copyBytes = uint64_t(image.width) * image.height * 4;
		if (hudImageBytes_ < copyBytes)
		{
			if (hudImage_)
			{
				Retire(hudImage_);
			}
			hudImage_ = gpu::MakeBuffer(device_, copyBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
			hudImageBytes_ = hudImage_ ? copyBytes : 0;
			hudImageReadable_ = false;
			if (!hudImage_)
			{
				hudSlots_.Reading(*taken, 0);  // never read: free again
				return HudSkip("couldn't create the HUD image's buffer");
			}
			hudImage_->SetName(L"BlockRender HUD image");
		}
	}

	// Our own list: calls the hooks watch go through their originals, so nothing of ours reaches the tracker.
	const bool                 wasReadable = hudImageReadable_;  // as the buffer is now, grown or not
	const HudImage             wasShown = hudShown_;
	const d3d::Originals&      original = d3d::Original();
	ID3D12GraphicsCommandList* list = hudLists_[slot].Get();
	if (FAILED(hudAllocators_[slot]->Reset()) || FAILED(original.reset(list, hudAllocators_[slot].Get(), hudPipeline_.Get())))
	{
		if (taken)
		{
			hudSlots_.Reading(*taken, 0);
		}
		return HudSkip("couldn't reset its command list");
	}
	list->EndQuery(hudQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2);
	if (taken)
	{
		const D3D12_RESOURCE_STATES  before = hudImageReadable_ ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON;
		const D3D12_RESOURCE_BARRIER toCopy = Transition(hudImage_.Get(), before, D3D12_RESOURCE_STATE_COPY_DEST);
		original.resourceBarrier(list, 1, &toCopy);
		list->CopyBufferRegion(hudImage_.Get(), 0, hudUploads_[*taken].Get(), 0, copyBytes);
		const D3D12_RESOURCE_BARRIER toRead =
		    Transition(hudImage_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		original.resourceBarrier(list, 1, &toRead);
		hudImageReadable_ = true;
		hudShown_ = hudSlotImages_[*taken];
	}
	const D3D12_RESOURCE_BARRIER toTarget = Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	original.resourceBarrier(list, 1, &toTarget);
	const D3D12_CPU_DESCRIPTOR_HANDLE rtv = Offset(rtvHeap_->GetCPUDescriptorHandleForHeapStart(), kBackBufferRtv, rtvIncrement_);
	original.createRtv(device_, backBuffer.Get(), nullptr, rtv);
	original.omSetRenderTargets(list, 1, &rtv, FALSE, nullptr);
	const D3D12_VIEWPORT viewport{ 0.0f, 0.0f, float(target.Width), float(target.Height), 0.0f, 1.0f };
	const D3D12_RECT     scissor{ 0, 0, LONG(target.Width), LONG(target.Height) };
	list->RSSetViewports(1, &viewport);
	list->RSSetScissorRects(1, &scissor);
	list->SetGraphicsRootSignature(rootSignature_.Get());
	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	list->SetGraphicsRootConstantBufferView(0, constants_->GetGPUVirtualAddress());  // b0: the HUD's shaders don't read it
	const uint32_t     scale = HudScaleFor(uint32_t(target.Width), target.Height, hudShown_.width, hudShown_.height);
	const HudConstants hud{ hudShown_.width, hudShown_.height, scale, hudShown_.bottomUp ? 1u : 0u };
	list->SetGraphicsRoot32BitConstants(1, 4, &hud, 0);
	for (UINT parameter = 2; parameter < 5; ++parameter)  // t0 to t2: the image, which HudPS reads as t1
	{
		list->SetGraphicsRootShaderResourceView(parameter, hudImage_->GetGPUVirtualAddress());
	}
	original.drawInstanced(list, 3, 1, 0, 0);
	const D3D12_RESOURCE_BARRIER toPresent = Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	original.resourceBarrier(list, 1, &toPresent);
	list->EndQuery(hudQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2 + 1);
	list->ResolveQueryData(hudQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2, 2, hudTimings_.Get(), slot * 2 * sizeof(uint64_t));
	if (FAILED(original.close(list)))
	{
		hudImageReadable_ = wasReadable;  // nothing of the list ran
		hudShown_ = wasShown;
		if (taken)
		{
			hudSlots_.Reading(*taken, 0);
		}
		return HudSkip("couldn't close its command list");
	}
	ID3D12CommandList* lists[] = { list };
	original.executeCommandLists(queue_, 1, lists);
	const uint64_t fence = Signal();
	hudSlotFence_[slot] = fence;
	if (taken)
	{
		hudSlots_.Reading(*taken, fence);
	}
	++hudRecordings_;
	++stats_.hudDrawn;
}

void Renderer::Shutdown()
```
```cpp
		timingsMapped_ = nullptr;
	}
	sections_.clear();
```
```cpp
		timingsMapped_ = nullptr;
	}
	hudWanted_ = false;
	for (uint32_t slot = 0; slot < HudSlots::kSlots; ++slot)
	{
		if (hudMapped_[slot])
		{
			hudUploads_[slot]->Unmap(0, nullptr);
			hudMapped_[slot] = nullptr;
		}
		hudUploads_[slot].Reset();
		hudUploadBytes_[slot] = 0;
	}
	if (hudTimingsMapped_)
	{
		hudTimings_->Unmap(0, nullptr);
		hudTimingsMapped_ = nullptr;
	}
	hudImage_.Reset();
	hudPipeline_.Reset();
	hudLists_ = {};
	hudAllocators_ = {};
	hudQueries_.Reset();
	hudTimings_.Reset();
	sections_.clear();
```

- [x] **Step 3: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `203 test(s), 0 failure(s)`.

- [x] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: the HUD pass" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 24: The plugin links to Minecraft (0.9.0)

`Plugin.cpp` gains the link, around everything spike 3's plugin did, which stays as it was (the spec's
"`Plugin.cpp`", "The link" and "Blocks"):
- **The mapping** is opened at the first update, not at load: the heartbeat comes from `Link::Update`, once a frame,
  and the game doesn't update until it has loaded (see the spec's "Changes while planning"). It's tried again each
  second while it's refused or failed, with the reason in the status. Once it's open, the plugin makes the `Link` on
  it.
- **`OnUpdate`'s order:**
  1. once a second: the late patches and the device check, the mapping if it isn't open, and the requests;
  2. the link: V's pose, if `LocatePlayer` finds V and the camera; whether the HUD is wanted, and the size to ask for;
     then `Link::Update`, which hands what Minecraft sends to the sink below;
  3. the frame log and the dumps;
  4. spike 3's meshing, skipped while Minecraft's atlas is loaded, then the renderer's `Update`, timed;
  5. the burst's frame, the log, and once a second the status.
- **The sink:**
  - **the atlas:** `AtlasFromRgba`, then spike 3's world and structures cleared, every section cleared, and
    `SetAtlas`. If it can't be built, the log says why, and the atlas stays as it was;
  - **sections:** to `UploadSection`, the first after a latency mark timed with `TimeUpload`;
  - **clear-all:** `ClearSections`;
  - **explosions:** logged, and a burst started;
  - **HUD images:** to `SetHudImage`;
  - **the link's log lines:** into BlockRender's log.
- **Latency and bursts:** a typed command's Enter marks the latency and starts a burst. When the timed upload is
  drawn, the sample goes in the log and the status.
- **Requests:**
  - `link on` sets the in-game flag, and switches drawing to `gbuffer` if it's off; `link off` clears the flag. Before
    the mapping opens, `link on` takes effect once it does;
  - `floor` asks the link for a fresh floor; `mc` types into Minecraft's chat;
  - `clear` also clears Minecraft's sections, and `place` is refused while Minecraft's atlas is loaded;
  - `set` takes `translucent` (0 or 1), `teleportstep` and `teleportrate` (above 0), `ringmb` (1 to 64), `hud`
    (0, 1 or 2) and `hudscale` (1, 2 or 4), before spike 3's settings.
- **The status** gets the link's lines (Task 21): each second's figures from the link's counters, the renderer's
  stats, the latency, the last burst, and both processes, Minecraft's only while it's there.
- **On unload,** after the renderer and the hooks: the link, then the mapping. Minecraft sees the heartbeat stop.
- **The version** is 0.9.0.

The whole file follows; its unchanged functions are spike 3's as they were.

**Files:**
- Modify: `src/Plugin.cpp`, `CMakeLists.txt`

- [x] **Step 1: Write `src/Plugin.cpp`**

```cpp
// BlockRender: spike 3's RED4ext plugin, linked to Minecraft by spike 4. It hooks the game's Streamline calls and D3D12
// to map Cyberpunk's frame (spike 3's step 1: the frame log and the dumps), and draws Minecraft blocks into it (step 2:
// on the finished frame; step 3: before DLSS; step 4: into the G-buffer). Spike 4 links it to SkyCraft's Minecraft
// through SkyCraft's shared memory: Minecraft's player follows V, and Minecraft's blocks and HUD are drawn in the game.
// Commands come through blockrender.request, in its folder or the overlay's, and status.txt in the overlay's folder says
// what it's doing. See docs/superpowers/specs/2026-10-01-block-render-design.md and 2026-10-02-minecraft-link-design.md.
#include <RED4ext/RED4ext.hpp>

#include "LogQueue.h"
#include "Measurements.h"
#include "PlayerLocator.h"
#include "ProcessUsage.h"
#include "Requests.h"
#include "Status.h"
#include "hooks/D3D12Hooks.h"
#include "hooks/HookGroups.h"
#include "hooks/StreamlineHooks.h"
#include "link/Link.h"
#include "link/Mapping.h"
#include "log/Dumps.h"
#include "log/FrameLog.h"
#include "log/FrameLogData.h"
#include "render/Hud.h"
#include "render/Renderer.h"
#include "track/GpuApi.h"
#include "world/BlockWorld.h"
#include "world/Mesher.h"
#include "world/Structures.h"

#include <Windows.h>

#include <algorithm>
#include <array>
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

constexpr const char* kVersion = "0.9.0";

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
bool                                       g_trackingHeld = false;  // set tracking 1: to measure what tracking costs

// Spike 4: the link to SkyCraft's Minecraft (the spec's "The link"). Main thread only.
cpmc::link::Mapping              g_mapping;
cpmc::link::Link*                g_link = nullptr;  // once the mapping is open
std::string                      g_mappingState = "not opened yet";
cpmc::link::LinkSettings         g_linkSettings;     // set teleportstep, teleportrate, ringmb, translucent; and the viewport
bool                             g_linked = false;   // link on|off
int                              g_hudSetting = 1;   // set hud: 0 never, 1 in build mode, 2 always
uint32_t                         g_hudScale = 2;     // set hudscale: 1, 2 or 4
bool                             g_hudWanted = false;
bool                             g_minecraftAtlas = false;  // Minecraft's atlas has replaced atlas.bin's
std::optional<cpmc::Vec3d>       g_camera;                  // V's camera, Minecraft axes, as the link last had it
std::string                      g_lastTyped;               // the last mc request, which names its burst
std::optional<double>            g_lastEnter;               // the link's last Enter, once marked
cpmc::LatencyTracker             g_latency;
cpmc::BurstWindow                g_burst;
std::optional<cpmc::BurstReport> g_lastBurst;
cpmc::ProcessWatch               g_cyberpunkProcess;
cpmc::ProcessWatch               g_minecraftProcess;
double                           g_lastFrameAt = 0.0;  // SteadySeconds() at the last OnUpdate

// This frame's figures, for bursts.
struct FrameFigures
{
	uint64_t sections = 0;
	uint64_t bytes = 0;
	double   drainMs = 0.0;
};
FrameFigures g_frame;

// This second's, for the status.
struct SecondFigures
{
	double                                         start = 0.0;  // SteadySeconds()
	uint64_t                                       frames = 0;
	uint64_t                                       bytes = 0;
	uint64_t                                       bytesWorstFrame = 0;
	double                                         drainMsSum = 0.0;
	double                                         drainMsWorst = 0.0;
	uint64_t                                       teleportsAtStart = 0;
	uint64_t                                       minecraftFramesAtStart = 0;
	std::array<uint64_t, cpmc::link::Link::kTypes> messagesAtStart{};
	std::array<uint64_t, cpmc::link::Link::kTypes> bytesAtStart{};
};
SecondFigures g_second;

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

// What Minecraft sends, handed to the renderer (the spec's "Blocks" and "The HUD").
class MinecraftSink : public cpmc::link::LinkSink
{
public:
	void Atlas(const cpmc::link::AtlasImage& aAtlas) override
	{
		const double               start = cpmc::SteadySeconds();
		std::string                error;
		std::optional<cpmc::Atlas> atlas = cpmc::AtlasFromRgba(aAtlas.width, aAtlas.height, aAtlas.rgba, error);
		if (!atlas)
		{
			return ::Log("Minecraft's atlas can't be built ({}), so the atlas stays as it was", error);
		}
		// Spike 3's own structures go, with every section: their UVs belong to the atlas before. Spike 3's world isn't
		// meshed again while Minecraft's atlas is loaded, so its removals can't undo Minecraft's sections.
		g_world.Clear();
		g_world.TakeDirty();
		g_structures.clear();
		g_renderer->ClearSections();
		g_renderer->SetAtlas(std::move(*atlas));
		g_minecraftAtlas = true;
		::Log("Minecraft's atlas: {} x {}, {} levels, built in {:.1f} ms", aAtlas.width, aAtlas.height, cpmc::kMinecraftAtlasLevels,
		      (cpmc::SteadySeconds() - start) * 1000.0);
	}

	void Section(cpmc::link::SectionMesh&& aMesh) override
	{
		const cpmc::SectionPos section{ aMesh.x, aMesh.y, aMesh.z };
		++g_frame.sections;
		if (g_latency.SectionArrived(cpmc::SteadySeconds(), g_renderer->Presents()))
		{
			g_renderer->TimeUpload(section);  // the first section after a mark: timed to the list that first draws it
		}
		g_renderer->UploadSection(section, std::move(aMesh.vertices));
	}

	void ClearAll() override
	{
		g_renderer->ClearSections();
		::Log("Minecraft cleared its sections: it sends everything again");
	}

	void Explosion(const skycraft::proto::McEvent& aEvent) override
	{
		::Log("explosion at Minecraft {:.1f} {:.1f} {:.1f}, radius {:.1f}", aEvent.a, aEvent.b, aEvent.c, aEvent.d);
		g_burst.Start(cpmc::SteadySeconds(), "explosion");
	}

	void Hud(const cpmc::link::HudFrame& aFrame) override
	{
		g_renderer->SetHudImage(aFrame.width, aFrame.height, aFrame.bottomUp, aFrame.pixels);
	}

	void Log(const std::string& aLine) override
	{
		::Log("link: {}", aLine);
	}
};
MinecraftSink g_sink;

// The mapping, made as SkyCraft's Skyrim side makes it (the spec's "link/"), at the first update rather than at load:
// the heartbeat comes once a frame from Link::Update, and the game doesn't update until it has loaded, so Minecraft
// would link to a game side that then falls silent. While another game side uses it, it's refused, and tried again
// once a second.
void OpenMapping()
{
	using Outcome = cpmc::link::Mapping::Outcome;
	const cpmc::link::Mapping::Opened opened = g_mapping.Open(skycraft::proto::kMappingName, GetCurrentProcessId(), GetTickCount64());
	const bool                        open = opened.outcome == Outcome::Created || opened.outcome == Outcome::Reused;
	const std::string state = open ? opened.why : (opened.outcome == Outcome::Refused ? "refused: " : "failed: ") + opened.why;
	if (state != g_mappingState)
	{
		Log("the mapping: {}", state);
	}
	g_mappingState = state;
	if (open)
	{
		g_link = new cpmc::link::Link(g_mapping.Base());
		g_second = {};
		g_second.start = cpmc::SteadySeconds();
	}
}

// The link, once a frame (the spec's "The link"): V's pose and the HUD's size, then Link::Update, which hands what
// Minecraft sends to the sink.
void UpdateLink(double aNow)
{
	g_frame = {};
	if (!g_link)
	{
		return;
	}
	const cpmc::link::Link::Status&     state = g_link->State();
	std::optional<cpmc::link::GamePose> pose;
	if (const std::optional<cpmc::PlayerPose> player = cpmc::LocatePlayer(); player && player->camera)
	{
		pose = cpmc::link::GamePose{ player->feet, player->camera->position, player->camera->forward };
	}
	g_camera = pose ? std::optional<cpmc::Vec3d>(cpmc::McFromWorld(pose->camera)) : std::nullopt;
	g_hudWanted = cpmc::HudWanted(g_hudSetting, false, state.inWorld);  // build mode comes with input, in Part 3
	g_renderer->SetHud(g_hudWanted);
	const cpmc::HudSize back = g_renderer->BackBufferSize();
	const cpmc::HudSize viewport = cpmc::HudViewport(back.width, back.height, g_hudScale);
	g_linkSettings.viewportWidth = viewport.width;
	g_linkSettings.viewportHeight = viewport.height;

	cpmc::link::Link::Frame frame;
	frame.now = aNow;
	frame.tickMs = GetTickCount64();
	frame.pose = pose;
	frame.linked = g_linked;
	frame.buildMode = false;
	frame.takeHud = g_hudWanted;
	const uint64_t drainedBefore = state.drainedBytes;
	g_link->Update(frame, g_linkSettings, g_sink);
	g_frame.bytes = state.drainedBytes - drainedBefore;
	g_frame.drainMs = state.drainMs;
	++g_second.frames;
	g_second.bytes += g_frame.bytes;
	g_second.bytesWorstFrame = std::max(g_second.bytesWorstFrame, g_frame.bytes);
	g_second.drainMsSum += g_frame.drainMs;
	g_second.drainMsWorst = std::max(g_second.drainMsWorst, g_frame.drainMs);
	if (state.lastEnter && state.lastEnter != g_lastEnter)
	{
		// A typed command's Enter: a latency mark, and a burst's start.
		g_lastEnter = state.lastEnter;
		g_latency.Mark(*state.lastEnter);
		g_burst.Start(*state.lastEnter, "mc " + g_lastTyped);
	}
	if (const std::optional<cpmc::Renderer::TimedUpload> timed = g_renderer->TakeTimedUpload())
	{
		if (const std::optional<cpmc::LatencySample> sample = g_latency.Drawn(timed->drawn, timed->drawnFrame))
		{
			Log("latency: Minecraft {:.0f} ms, ours {:.1f} ms ({:.0f} frames)", sample->minecraftMs, sample->oursMs, sample->oursFrames);
		}
	}
}

void Place(const cpmc::Request& aRequest)
{
	if (g_minecraftAtlas)
	{
		return Report("place " + aRequest.what + ": refused while Minecraft's atlas is loaded");
	}
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
	const cpmc::BlockPos              origin = cpmc::PlacementOrigin(pose->feet, pose->forward);
	const cpmc::Atlas&                atlas = *g_renderer->AtlasData();
	const std::optional<cpmc::Placed> placed = aRequest.what == "field"
	                                               ? cpmc::PlaceField(g_world, atlas, origin, size_t(aRequest.number), error)
	                                               : cpmc::PlaceStructure(g_world, atlas, aRequest.what, origin, error);
	if (!placed)
	{
		return Report("place " + aRequest.what + ": " + error);
	}
	const auto [low, high] = cpmc::WorldBox(*placed);
	g_structures.push_back({ aRequest.what, low, high });
	Report(std::format("placed {}: {} blocks, Minecraft {} {} {} to {} {} {}", aRequest.what, placed->blocks, placed->min.x, placed->min.y,
	                   placed->min.z, placed->max.x, placed->max.y, placed->max.z));
}

// Spike 4's settings (the spec's "Requests"). True if aRequest names one of them.
bool SetLinkSetting(const cpmc::Request& aRequest)
{
	const std::string& name = aRequest.what;
	const double       value = aRequest.number;
	if (name == "translucent")
	{
		if (value != 0.0 && value != 1.0)
		{
			Report("set translucent: 0 drops translucent triangles, and 1 draws them as cut-outs");
			return true;
		}
		g_linkSettings.keepTranslucent = value == 1.0;
	}
	else if (name == "teleportstep" || name == "teleportrate")
	{
		if (value <= 0.0)
		{
			Report(std::format("set {}: it's above 0", name));
			return true;
		}
		(name == "teleportstep" ? g_linkSettings.teleportStep : g_linkSettings.teleportRate) = value;
	}
	else if (name == "ringmb")
	{
		if (value < 1.0 || value > 64.0)
		{
			Report("set ringmb: from 1 to 64");
			return true;
		}
		g_linkSettings.ringBudget = uint64_t(value * 1048576.0);
	}
	else if (name == "hud")
	{
		if (value != 0.0 && value != 1.0 && value != 2.0)
		{
			Report("set hud: 0 never, 1 in build mode, 2 always");
			return true;
		}
		g_hudSetting = int(value);
	}
	else if (name == "hudscale")
	{
		if (value != 1.0 && value != 2.0 && value != 4.0)
		{
			Report("set hudscale: 1, 2 or 4");
			return true;
		}
		g_hudScale = uint32_t(value);
	}
	else
	{
		return false;
	}
	Report(std::format("set {} {}", name, value));
	return true;
}

void Set(const cpmc::Request& aRequest)
{
	if (SetLinkSetting(aRequest))
	{
		return;
	}
	if (aRequest.what == "ao")
	{
		g_meshOptions.ambientOcclusion = aRequest.number != 0.0;
		g_world.MarkAllDirty();
		return Report(std::format("set ao {}", g_meshOptions.ambientOcclusion ? 1 : 0));
	}
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
		const bool hold = aRequest.number != 0.0;
		if (hold != g_trackingHeld)
		{
			hold ? cpmc::d3d::RequestTracking() : cpmc::d3d::ReleaseTracking();
			g_trackingHeld = hold;
		}
		return Report(std::format("set tracking {}: command lists are {}tracked", hold ? 1 : 0, cpmc::d3d::Tracking() ? "" : "not "));
	}
	std::string error;
	Report(g_renderer->Set(aRequest.what, aRequest.number, error) ? std::format("set {} {}", aRequest.what, aRequest.number)
	                                                              : "set: " + error);
}

// link on|off (the spec's "Linking"): linking also switches drawing to gbuffer if it's off.
void LinkRequest(bool aOn)
{
	g_linked = aOn;
	if (!aOn)
	{
		return Report("link off: Minecraft holds its player where it is, and the blocks stay");
	}
	std::string why;
	if (g_renderer->ModeName() == "off" && !g_renderer->SetMode("gbuffer", why))
	{
		return Report("link on, but drawing stays off: " + why);
	}
	const std::string link = g_link ? "Minecraft's player follows V" : "it starts once the mapping opens, which is " + g_mappingState;
	Report(std::format("link on: {}; drawing is {}", link, g_renderer->ModeName()));
}

void TypeRequest(const std::string& aWhat)
{
	if (!g_link)
	{
		return Report("mc: there's no link: the mapping is " + g_mappingState);
	}
	std::string error;
	if (!g_link->Type(aWhat, error))
	{
		return Report("mc: " + error);
	}
	g_lastTyped = aWhat;
	Report("mc " + aWhat + ": typing it into Minecraft's chat");
}

// The command in aFolder's blockrender.request, if there is one: in the plugin's folder, where the agent writes it, or
// the overlay's, where its buttons do. The overlay can't rename a finished file into place, so there a command counts
// only once its line ends (aWholeLine).
void RunRequest(const std::filesystem::path& aFolder, bool aWholeLine)
{
	const std::filesystem::path path = aFolder / "blockrender.request";
	std::error_code             error;
	if (!std::filesystem::exists(path, error))
	{
		return;
	}
	const std::string text = ReadSmallFile(path);
	if (aWholeLine && (text.empty() || text.back() != '\n'))
	{
		return;  // still being written: read it next second
	}
	std::filesystem::remove(path, error);
	std::string                        why;
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
		g_renderer->ClearSections();
		Report(g_minecraftAtlas ? "cleared: Minecraft's sections come back as they change, or when it next sends everything" : "cleared");
		break;
	case cpmc::Request::Kind::Reload: Report(g_renderer->ReloadShaders(why) ? "reload: shaders compiled" : "reload: " + why); break;
	case cpmc::Request::Kind::Set: Set(*request); break;
	case cpmc::Request::Kind::Link: LinkRequest(request->what == "on"); break;
	case cpmc::Request::Kind::Floor:
		if (!g_link)
		{
			Report("floor: there's no link: the mapping is " + g_mappingState);
			break;
		}
		g_link->RequestFloor();
		Report("floor: a fresh one at the next update, once Minecraft is there");
		break;
	case cpmc::Request::Kind::Mc: TypeRequest(request->what); break;
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

// The link's lines of the status (the spec's "Measurements"), from the second since the last status.
cpmc::LinkReport LinkLines(const cpmc::Renderer::Stats& aStats, double aNow)
{
	cpmc::LinkReport report;
	report.mapping = g_mappingState;
	report.linked = g_linked;
	report.camera = g_camera;
	if (g_link)
	{
		const cpmc::link::Link::Status& state = g_link->State();
		const double                    seconds = std::max(aNow - g_second.start, 0.001);
		const uint64_t                  frames = state.minecraftFrames >= g_second.minecraftFramesAtStart
		                                             ? state.minecraftFrames - g_second.minecraftFramesAtStart
		                                             : state.minecraftFrames;  // a new Minecraft counts from 0
		report.minecraft = state.minecraft;
		report.minecraftPid = state.minecraftPid;
		report.heartbeatAgeMs = state.heartbeatAgeMs;
		report.inWorld = state.inWorld;
		report.screenOpen = state.screenOpen;
		report.minecraftFps = double(frames) / seconds;
		report.inGame = state.inGame;
		report.minecraftEye = state.minecraftEye;
		report.teleports = state.teleports - g_second.teleportsAtStart;
		report.target = state.target;
		report.floorEpoch = state.floorEpoch;
		report.floorSent = state.floorSent;
		report.floorCentre = state.floorCentre;
		report.ringMb = double(g_second.bytes) / 1048576.0;
		report.ringMbWorstFrame = double(g_second.bytesWorstFrame) / 1048576.0;
		report.drainMsAverage = g_second.frames ? g_second.drainMsSum / double(g_second.frames) : 0.0;
		report.drainMsWorst = g_second.drainMsWorst;
		for (size_t type = 0; type < cpmc::link::Link::kTypes; ++type)
		{
			const uint64_t messages = state.messages[type] - g_second.messagesAtStart[type];
			if (messages != 0)
			{
				report.ringTypes += std::format("{}{} {} ({:.2f} MB)", report.ringTypes.empty() ? "" : ", ", messages,
				                                cpmc::link::RenderTypeName(type),
				                                double(state.bytes[type] - g_second.bytesAtStart[type]) / 1048576.0);
			}
		}
		report.rejected = state.rejected;
		report.resyncs = state.resyncs;
		report.translucentDropped = state.translucentDropped;
		report.hudRefused = state.hudRefused;
	}
	report.atlas = aStats.atlasWidth == 0 ? "none yet"
	                                      : std::format("{} x {}, {} levels, {}", aStats.atlasWidth, aStats.atlasHeight, aStats.atlasLevels,
	                                                    g_minecraftAtlas ? "Minecraft's" : "atlas.bin's");
	report.updateMsAverage = aStats.updateMsAverage;
	report.updateMsWorst = aStats.updateMsWorst;
	report.hudSetting = g_hudSetting;
	report.hudScale = g_hudScale;
	report.hudWanted = g_hudWanted;
	report.hudReceived = aStats.hudReceived;
	report.hudMb = double(aStats.hudBytes) / 1048576.0;
	report.hudDropped = aStats.hudDropped;
	report.hudDrawn = aStats.hudDrawn;
	report.hudSize = aStats.hudWidth == 0 ? ""
	                                      : std::format("{} x {}, rows {}", aStats.hudWidth, aStats.hudHeight,
	                                                    aStats.hudBottomUp ? "bottom-up" : "top-down");
	report.hudCopyMsAverage = aStats.hudCopyMsAverage;
	report.hudGpuMsAverage = aStats.hudGpuMsAverage;
	report.hudSkipped = aStats.hudSkipped;
	report.hudLastSkip = aStats.hudLastSkip;
	report.latencyLast = g_latency.Last();
	report.latencyAverage = g_latency.Average();
	report.latencySamples = g_latency.Samples();
	report.burst = g_lastBurst;
	report.cyberpunkCpu = g_cyberpunkProcess.CpuShare();
	report.cyberpunkMemory = g_cyberpunkProcess.WorkingSet();
	report.minecraftProcessKnown = g_minecraftProcess.Known();
	report.minecraftCpu = g_minecraftProcess.CpuShare();
	report.minecraftMemory = g_minecraftProcess.WorkingSet();
	return report;
}

// The next second's figures start from where the link is now.
void StartSecond(double aNow)
{
	g_second = {};
	g_second.start = aNow;
	if (g_link)
	{
		const cpmc::link::Link::Status& state = g_link->State();
		g_second.teleportsAtStart = state.teleports;
		g_second.minecraftFramesAtStart = state.minecraftFrames;
		g_second.messagesAtStart = state.messages;
		g_second.bytesAtStart = state.bytes;
	}
}

void WriteStatus(double aNow)
{
	g_cyberpunkProcess.Sample(GetCurrentProcessId(), aNow);
	g_minecraftProcess.Sample(g_link && g_link->State().minecraft ? g_link->State().minecraftPid : 0, aNow);  // not one that's gone
	const cpmc::Renderer::Stats stats = g_renderer->TakeStats();
	cpmc::StatusReport          report;
	report.mode = g_renderer->ModeName();
	report.framesDrawn = stats.drawn;
	report.framesPartial = stats.partial;
	report.framesSkipped = stats.skipped;
	report.lastSkip = stats.lastSkip;
	report.lateCamera = stats.lateCamera;
	report.sections = stats.sections;
	report.blocks = g_world.BlockCount();
	report.minecraftBlocks = g_minecraftAtlas;
	report.vertices = stats.vertices;
	report.gpuMsAverage = stats.gpuMsAverage;
	report.gpuMsWorst = stats.gpuMsWorst;
	report.frameMsAverage = stats.frameMsAverage;
	report.frameMsWorst = stats.frameMsWorst;
	const uint64_t tokens = cpmc::slhooks::FrameTokens();
	report.streamlineFrames = uint32_t(tokens - g_lastFrameTokens);
	g_lastFrameTokens = tokens;
	report.hooks = g_hooked ? std::format("{}; {}; {}; log {}; dump {}", cpmc::slhooks::Summary(), cpmc::d3d::Summary(),
	                                      cpmc::HookGroupsSummary(), g_frameLog->State(), g_dumps->State())
	                        : "not installed (see red4ext\\logs)";
	report.lastRequest = g_lastRequest;
	report.structures = g_structures;
	report.link = LinkLines(stats, aNow);
	StartSecond(aNow);
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

// Every frame on the main thread while the game runs (RED4ext ignores the return value in this state), in the spec's
// order: requests once a second, the link, the frame log and the dumps, meshing and the renderer's Update, the log,
// and the status once a second.
bool OnUpdate(RED4ext::CGameApplication*)
{
	const auto   now = Clock::now();
	const double seconds = cpmc::SteadySeconds();
	const double frameMs = g_lastFrameAt > 0.0 ? (seconds - g_lastFrameAt) * 1000.0 : 0.0;
	g_lastFrameAt = seconds;
	const bool newSecond = now - g_lastSecond >= 1s;
	if (newSecond)
	{
		g_lastSecond = now;
		if (g_hooked)
		{
			PatchLate();
			CheckDevice();
		}
		if (!g_link)
		{
			OpenMapping();
		}
		RunRequest(g_pluginDir, false);
		RunRequest(g_hudDir, true);
	}
	UpdateLink(seconds);
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
	if (g_minecraftAtlas)
	{
		g_world.TakeDirty();  // spike 3's world stays empty, and isn't meshed with Minecraft's atlas
	}
	else if (const cpmc::Atlas* atlas = g_renderer->AtlasData())
	{
		for (const cpmc::SectionPos& section : g_world.TakeDirty())
		{
			g_renderer->UploadSection(section, cpmc::MeshSection(g_world, section, *atlas, g_meshOptions));
		}
	}
	const double updateStart = cpmc::SteadySeconds();
	g_renderer->Update();
	const double updateMs = (cpmc::SteadySeconds() - updateStart) * 1000.0;
	const cpmc::BurstFrame burstFrame{ g_frame.sections, g_frame.bytes, g_frame.drainMs, updateMs, frameMs };
	if (const std::optional<cpmc::BurstReport> burst = g_burst.Frame(seconds, burstFrame))
	{
		g_lastBurst = burst;
		Log("burst after {}: {} sections, {:.2f} MB over {} frames; worst drain {:.3f} ms, update {:.3f} ms, frame {:.1f} ms", burst->what,
		    burst->sections, double(burst->bytes) / 1048576.0, burst->frames, burst->drainMsWorst, burst->updateMsWorst,
		    burst->frameMsWorst);
	}
	DrainLog();
	if (newSecond)
	{
		for (const std::string& line : cpmc::d3d::DebugMessages())
		{
			Log("{}", line);
		}
		WriteStatus(seconds);
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
		delete g_link;  // main thread only, as everything of the link's
		g_link = nullptr;
		g_mapping.Close();  // Minecraft sees the heartbeat stop
		DrainLog();
		break;
	}
	return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* aInfo)
{
	aInfo->name = L"BlockRender";
	aInfo->author = L"Cyberpunk-Minecraft";
	aInfo->version = RED4EXT_V1_SEMVER(0, 9, 0);
	aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
	aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
	return RED4EXT_API_VERSION_1;
}
```

- [x] **Step 2: Edit `CMakeLists.txt` for version 0.9.0**

```cmake
project(BlockRender VERSION 0.8.0 LANGUAGES C CXX)
```
```cmake
project(BlockRender VERSION 0.9.0 LANGUAGES C CXX)
```

- [x] **Step 3: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `203 test(s), 0 failure(s)`.

- [x] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: the plugin links to Minecraft (0.9.0)" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 25: The overlay's buttons for the link

The spec's "Status and the overlay": under the drawing mode buttons, which stay, a row with Link, Unlink and a HUD
button, and a row of the command sets. The HUD button shows the `hud` setting from the status, and each click asks
for the next: never, in build mode, always. It shows once the status has a `hud=` line. Each button writes its
request into the overlay's `blockrender.request`, as the mode buttons do. Build mode's line comes with input, in
Part 3.

**Files:**
- Modify: `tools/blockrender_hud/init.lua`

- [x] **Step 1: Edit `tools/blockrender_hud/init.lua` to add the buttons**

```lua
-- BlockRender HUD: what BlockRender (spike 3 of D:\Cyberpunk-Minecraft) is doing, from the status.txt it writes into
-- this folder once a second, plus V's position, heading and Minecraft block, and the nearest structure, and the frame
-- time as CET sees it, which works without BlockRender too and goes into frames.txt here as well. Its buttons switch
-- BlockRender's drawing mode, through blockrender.request in this folder. Delete this folder to remove it.
local status = nil       -- status.txt's lines, except the structures
local structures = {}    -- { name, min = { x, y, z }, max = { x, y, z } }, in world coordinates
```
```lua
-- BlockRender HUD: what BlockRender (spikes 3 and 4 of D:\Cyberpunk-Minecraft) is doing, from the status.txt it writes
-- into this folder once a second, plus V's position, heading and Minecraft block, and the nearest structure, and the
-- frame time as CET sees it, which works without BlockRender too and goes into frames.txt here as well. Its buttons
-- switch BlockRender's drawing mode, link it to Minecraft, type commands into Minecraft, and cycle when Minecraft's
-- HUD shows, through blockrender.request in this folder. Delete this folder to remove it.
local status = nil       -- status.txt's lines, except the structures
local structures = {}    -- { name, min = { x, y, z }, max = { x, y, z } }, in world coordinates
local hudSetting = nil   -- the hud line's setting: "never", "in build mode" or "always"
```
```lua
    { name = "gbuffer", label = "G-buffer" },
}
```
```lua
    { name = "gbuffer", label = "G-buffer" },
}

-- The named sets the buttons type into Minecraft's chat, as `mc <name>`.
local sets = { "creative", "tnt-kit", "tnt-ahead", "wall", "tnt-cube", "field", "wipe" }

-- The HUD button's cycle: after each `hud` setting, as the status names it, the one a click asks for.
local hudNext = { ["never"] = 1, ["in build mode"] = 2, ["always"] = 0 }
```
```lua
    structures = {}
    for line in file:lines() do
        local name, a, b, c, d, e, f = line:match("^structure=(%S+) (%S+) (%S+) (%S+) (%S+) (%S+) (%S+)$")
```
```lua
    structures = {}
    hudSetting = nil
    for line in file:lines() do
        local name, a, b, c, d, e, f = line:match("^structure=(%S+) (%S+) (%S+) (%S+) (%S+) (%S+) (%S+)$")
        hudSetting = line:match("^hud=([^,]+),") or hudSetting
```
```lua
        end
        ImGui.Separator()
```
```lua
        end
        ImGui.Text("Minecraft:")
        ImGui.SameLine()
        if ImGui.Button("Link") then
            request("link on")
        end
        ImGui.SameLine()
        if ImGui.Button("Unlink") then
            request("link off")
        end
        local nextHud = hudNext[hudSetting]
        if nextHud ~= nil then
            -- "###hud" keeps the button's ID the same while its label changes.
            ImGui.SameLine()
            if ImGui.Button("HUD: " .. hudSetting .. "###hud") then
                request("set hud " .. nextHud)
            end
        end
        ImGui.Text("Type:")
        for _, set in ipairs(sets) do
            ImGui.SameLine()
            if ImGui.Button(set) then
                request("mc " .. set)
            end
        end
        ImGui.Separator()
```

- [x] **Step 2: Check the overlay's syntax**

```bash
luac -p /d/Cyberpunk-Minecraft/spikes/block-render/tools/blockrender_hud/init.lua && echo "syntax ok"
```
Expected: `syntax ok`. (`luac` is Lua 5.4's; CET runs LuaJIT, and the overlay uses nothing that differs between them.)

- [x] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: the overlay's buttons for the link" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 26: A recorder for the status

Step 2 reads the status over time: five readings for each frame rate, and a minute of them while the user is in
another window, to see whether Cyberpunk kept running. `status_watch.py` prints, every interval, how long ago the
plugin last wrote `status.txt`, the status lines whose key matches a pattern, and CET's frame time from the overlay's
`frames.txt`. A status that stops getting younger means the game stopped updating.

**Files:**
- Create: `tools/status_watch.py`, `tools/test_status_watch.py`

- [x] **Step 1: Write the failing test, `tools/test_status_watch.py`**

```python
import status_watch


def test_a_sample_has_the_status_age_the_chosen_lines_and_cets_frame_time(tmp_path):
    status = tmp_path / "status.txt"
    status.write_text("mode=gbuffer\nframe=12.000 ms average, 20.000 ms worst\nlink=linked; the mapping created\n"
                      "processes=Cyberpunk 85% of a core, 6100 MB; Minecraft not running\n", encoding="utf-8")
    (tmp_path / "frames.txt").write_text("CET's frame time: 12.1 ms average (82.6 fps), 19.0 ms worst\n", encoding="utf-8")
    sample = status_watch.sample(tmp_path, "frame|processes", status.stat().st_mtime + 2.5)
    assert sample == ["status written 2.5 s ago",
                      "frame=12.000 ms average, 20.000 ms worst",
                      "processes=Cyberpunk 85% of a core, 6100 MB; Minecraft not running",
                      "CET's frame time: 12.1 ms average (82.6 fps), 19.0 ms worst"]


def test_keys_match_whole_keys_only(tmp_path):
    status = tmp_path / "status.txt"
    status.write_text("link=linked\nlinked=no such key\nhud=always, scale 2\n", encoding="utf-8")
    assert status_watch.sample(tmp_path, "link|hud", status.stat().st_mtime) == [
        "status written 0.0 s ago", "link=linked", "hud=always, scale 2"]


def test_without_a_status_it_says_so(tmp_path):
    assert status_watch.sample(tmp_path, "frame", 0.0) == ["status.txt missing"]
```

- [x] **Step 2: Run the new tool's test; it must fail**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render/tools && uv run --no-project -q --with pytest pytest -q -p no:cacheprovider test_status_watch.py 2>&1 | grep -E "^E |error in"
```
Expected: `E   ModuleNotFoundError: No module named 'status_watch'`, then `1 error in ...`.

- [x] **Step 3: Write `tools/status_watch.py`**

```python
"""Watch BlockRender's status in the game: every interval, the time, how long ago the plugin last wrote status.txt, the
status lines whose key matches a pattern, and CET's frame time from the overlay's frames.txt. A status that stops
getting younger means the game stopped updating.

    uv run --no-project python tools/status_watch.py <seconds> [interval] [keys]

The interval is in seconds, 1 by default. The keys are a regular expression, "frame|processes" by default. It reads
the overlay's folder in the game, E:\\Cyberpunk2077\\bin\\x64\\plugins\\cyber_engine_tweaks\\mods\\blockrender_hud.
"""
import re
import sys
import time
from pathlib import Path

HUD = Path("E:/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud")


def read_lines(path):
    try:
        return path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return None  # missing, or being replaced right now


def sample(folder, keys, now):
    status = folder / "status.txt"
    try:
        out = [f"status written {now - status.stat().st_mtime:.1f} s ago"]
    except OSError:
        return ["status.txt missing"]
    pattern = re.compile(rf"(?:{keys})=")
    out += [line for line in read_lines(status) or [] if pattern.match(line)]
    out += (read_lines(folder / "frames.txt") or [])[:1]
    return out


def main(argv):
    seconds = float(argv[1])
    interval = float(argv[2]) if len(argv) > 2 else 1.0
    keys = argv[3] if len(argv) > 3 else "frame|processes"
    start = time.time()
    while True:
        now = time.time()
        print(f"[{now - start:5.1f} s] " + " | ".join(sample(HUD, keys, now)), flush=True)
        if now + interval - start > seconds:
            break
        time.sleep(interval)


if __name__ == "__main__":
    main(sys.argv)
```

- [x] **Step 4: Run the tools' tests; they must pass**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render/tools && uv run --no-project -q --with numpy --with pillow --with pytest pytest -q -p no:cacheprovider 2>&1 | tail -1
```
Expected: `25 passed`, spike 3's 22 and these 3.

- [x] **Step 5: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: a recorder for the status" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 27: In the game: step 2

Step 2 of the spec's "The steps", with the user: BlockRender 0.9.0 in the game, linked to SkyCraft's Minecraft in
the `CyberCraft` instance, at spike 3's corner (world x −116.6, y −2005.2), with the same save.

The user plays, and the agent sends requests and reads what comes back. These commands come back again and again:
- **Send a request,** atomically:
  ```bash
  P=/e/Cyberpunk2077/red4ext/plugins/BlockRender; printf '%s\n' "link on" > "$P/request.tmp" && mv "$P/request.tmp" "$P/blockrender.request"
  ```
  The plugin runs it within a second. Replace `link on` with the command. Send the next only once the status's
  `request=` line shows the last.
- **The log's newest lines:**
  ```bash
  L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); tail -30 "/e/Cyberpunk2077/red4ext/logs/$L"
  ```
- **The status:**
  ```bash
  cat /e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud/status.txt
  ```
- **Five readings, a second apart** (other keys as needed):
  ```bash
  cd /d/Cyberpunk-Minecraft/spikes/block-render && uv run --no-project -q python tools/status_watch.py 5 1 "frame|processes"
  ```
- **Minecraft's side:**
  ```bash
  grep -E "SkyCraft:" "/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log" | tail -12
  ```

Note what you see as you go: Step 13 records it. If a check fails, stop and find the cause with
superpowers:systematic-debugging before going on. Fixes become new tasks, as in spike 3. A burst that hitches the
frame gets the spec's fix ("Blocks": sections suballocated from a few large buffers, and an upload budget per frame)
as a new task, and is measured again.

**Files:**
- Modify: `docs/spikes/2026-10-02-minecraft-link-results.md`

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
- the plugin's folder holds `BlockRender.dll`, `BlockRender.pdb` and `shaders`, with spike 3's `atlas.bin` and
  `atlas.json`, which Minecraft's atlas replaces once it links;
- the overlay's folder holds `init.lua`;
- then `installed`.

The second configure clears `CP2077_DIR` again.

- [x] **Step 2: Load, at the corner, without Minecraft**

Ask the user to check that Minecraft isn't running, then to launch the game, load the free-roam save, go to the
corner, and stand still with open street ahead. Then read the log's first lines and the status. Expected:
- `BlockRender 0.9.0 loaded` and `hooked`, then `the mapping: created`;
- in the status, `mode=off`, then `link=not linked; the mapping created; Minecraft absent; not in game`, V's camera
  in `positions=`, `atlas=none yet`, `hud=in build mode, scale 2, not showing`, and `Minecraft not running`;
- in the overlay's window, three rows of buttons: the drawing modes; Link, Unlink and `HUD: in build mode`; the seven
  sets.

Then the first frame rate, Cyberpunk without Minecraft: five readings, with `frame|processes`.

- [x] **Step 3: Start Minecraft**

Ask the user to start the `CyberCraft` instance from the Modrinth App, and to say when its window has gone: SkyCraft
hides it once it links. Then read Minecraft's side, the log and the status. Expected:
- Minecraft's side: `linked to Skyrim (pid …)` with Cyberpunk's process ID, `Skyrim link up`, and
  `opening mirror world`;
- the log: `link: Minecraft is there (process …)`, then `Minecraft's atlas: 2048 x 2576, 5 levels, built in … ms`.
  Note the time, which one frame takes (risk 5);
- the status: Minecraft's process, a fresh heartbeat, `in its world`, and its frames a second near Cyberpunk's, since
  it paces to `SkyState`; still `not in game`; `atlas=2048 x 2576, 5 levels, Minecraft's`; `blocks=Minecraft's`; and
  in `ring=`, atlas patches at about 8 MB a second.

Then five readings: Minecraft running, not linked.

- [x] **Step 4: Link**

Ask the user to open CET's overlay and click Link. Expected within two seconds:
- `request=link on: Minecraft's player follows V; drawing is gbuffer`, and `mode=gbuffer`;
- `link=linked; …; in game`;
- in `positions=`, Minecraft's eye and V's camera agreeing to within 0.1, teleports, and `floor 1 at height …` at
  V's feet.

Then ask the user to walk about 20 m along the street and back, watching `positions=` in the overlay. Expected: the
eye follows the camera, at most 10 teleports a second, and the floor's epoch goes up once V is 16 m from its centre.
Then five readings, linked and idle, with `frame|processes|link`.

- [x] **Step 5: The HUD**

Ask the user to click the HUD button once, so it says `HUD: always`. Expected:
- `request=set hud 2`, and `hud=always, scale 2, showing`, with about as many images received and drawn a second as
  the game's frames, at about 3.5 MB each, `1280 x 720, rows bottom-up`, the copy's and the pass's times, and nothing
  skipped or refused;
- on screen: Minecraft's hotbar, white crosshair and hand over Night City, sharp and the right way up, each of its GUI
  pixels 6 × 6 screen pixels (Minecraft's GUI scale 3 at 1280 × 720, doubled).

Ask for a screenshot, and check it. Then ask the user to turn and strafe for a few seconds while watching the HUD:
does it stay steady, or flicker, smear or lag? The game runs with frame generation on, and the HUD is drawn at
Present. If it isn't steady, ask the user to turn frame generation off in the game's graphics settings, look again,
and turn it back on. Note what each showed.

- [x] **Step 6: Building by command**

With V standing still and open street ahead:
1. ask the user to click `wall` in the overlay. Expected in the log: the link's typing lines, then a `latency:` line
   and a `burst after mc wall:` line. On screen: a stone-brick wall 5 wide and 4 tall, 4 blocks ahead, on the street
   and lit like it, as spike 3's blocks were. Ask for a screenshot;
2. send `mc tnt-ahead`. Expected about 4 seconds after its Enter: `explosion at Minecraft …`, a burst, and the wall
   broken around the TNT. Ask for a screenshot;
3. send `mc field`. Expected: `burst after mc field:` with 64 to 81 sections, and a grass layer 128 blocks square
   around V's feet. Ask for a screenshot;
4. send `mc wipe`. Expected: the layer and what's left of the wall gone, and a burst.

- [x] **Step 7: Latency**

With V standing still, send these two alternately, five times each, each once the last one's `latency:` line is in
the log:
```text
mc /execute rotated ~ 0 positioned ^ ^ ^3 run setblock ~ ~ ~ gold_block
mc /execute rotated ~ 0 positioned ^ ^ ^3 run setblock ~ ~ ~ diamond_block
```
Expected: a `latency:` line for each, and `latency=` in the status with the last and the average of 10. Note both
halves: Minecraft's, from the Enter to the section's arrival, and ours, from the arrival to the first list that draws
it, in milliseconds and frames. Ours is what the spike's question means by "a frame or two".

- [x] **Step 8: Bursts**

Send `mc tnt-cube`, then `mc field`, and after each, wait for its bursts in the log. `tnt-cube` sets off 125 TNT one
after another, so it gives several. Expected: each burst's sections, MB, worst drain, worst `Update` and worst frame.
A worst frame well above the frames around it is a hitch: see the top of this task. Then send `mc wipe`.

- [x] **Step 9: Frame rates, CPU and memory, with the HUD**

Five readings with `frame|processes|hud` for each:
1. `set hud 0`: the HUD off;
2. `set hud 2`: at `hudscale 2`;
3. `set hudscale 1`: Minecraft resizes to the full 2560 × 1440, and its images are about 14 MB each. Ask the user
   whether the HUD looks the same, only finer;
4. `set hudscale 2` again.

Wait three seconds after each request before reading.

- [x] **Step 10: Focus**

Start a recording of a minute, in the background:
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && uv run --no-project -q python tools/status_watch.py 60 0.5 "link|frame" > build/focus.log
```
Then ask the user to switch to Claude Code on the second monitor for 20 seconds, then back to the game, and to say
when they're back. Once the recording ends, read `build/focus.log` and Minecraft's side. Expected, either way:
- **Cyberpunk kept running:** `status written` stays under about a second old throughout, and `link=linked` with
  Minecraft there;
- **or it paused:** the status grows older while the user was away, Minecraft's side shows `Skyrim link down` 8
  seconds in, and `Skyrim link up` on return. Then the link must come back by itself, with the blocks and the HUD.

- [x] **Step 11: Minecraft closed and reopened while linked**

Send `mc wall`, then `mc /save-all flush`, so the wall is on disk. Ask the user to stop Minecraft from the Modrinth
App. Expected within 3 seconds: `Minecraft absent` in the status, `link: Minecraft has gone` in the log, the wall
still drawn, and the HUD gone. Then ask the user to start the instance again. Expected:
- Minecraft's side: linked, `Skyrim link up`, and `opening mirror world`;
- the log: `Minecraft is there` with a new process ID, then `Minecraft cleared its sections`, the atlas again, and a
  floor;
- the wall drawn again, and the HUD back once Minecraft is in its world.

- [x] **Step 12: Clean up**

Send `mc wipe`, `set hud 1`, `link off` and `mode off`, each once the last shows in the status. Expected:
`mode=off`, `link=not linked`, and no errors in the log. Then ask the user to quit the game, and to stop Minecraft
from the Modrinth App. BlockRender 0.9.0 stays installed. Like 0.8.0, nothing shows until the user links or picks a
drawing mode.

- [x] **Step 13: Record the results, and commit**

Add step 2 to `docs/spikes/2026-10-02-minecraft-link-results.md`, as step 1 is there:
- the game's settings and the build;
- each check above with what was seen, the screenshots' findings, and the numbers: the atlas's build time, latency's
  halves, each burst, and a table of frame times, CPU and memory for each condition (Minecraft closed, running,
  linked and idle, building, bursts, the HUD off, `hudscale 2` and `hudscale 1`);
- the HUD under frame generation, and focus;
- anything that surprised, and what it means for Part 3;
- the verdict for step 2.

Then commit:
```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes \
  && git commit -q -m "minecraft-link: step 2's results" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 28: Minecraft's yaw turns through north smoothly (0.9.1)

Added after step 2's run, which found the held item in Minecraft's HUD jumping whenever V turned through north
(heading 350 to 10), and nowhere else:
- **The cause:** the link sent Minecraft's yaw in (−180°, 180°], so it jumped from 180 to −180 facing north.
- **Why it showed:** SkyCraft puts the yaw straight into the player's `yRot` (`SkyClient.java`), and Minecraft's own
  yaw never wraps. Minecraft sways the first-person hand by the yaw's change, so a whole turn in one frame swung the
  held item, which then settled.

`LookFromForward` now returns the yaw within 180° of the last, in no fixed range, as Minecraft's own is: turning
through north goes 179, 180, 181. BlockRender becomes 0.9.1, so the log tells the builds apart.

**Files:**
- Modify: `tests/TestLook.cpp`, `src/link/Look.h`, `src/link/Look.cpp`, `src/Plugin.cpp`, `CMakeLists.txt`,
  `docs/superpowers/specs/2026-10-02-minecraft-link-design.md`, `docs/spikes/2026-10-02-minecraft-link-results.md`

- [x] **Step 1: Edit `tests/TestLook.cpp` to test turning through north**

```cpp
TEST(LookTeleportTargetPutsTheEyeAtTheCamera)
```
```cpp
TEST(LookTurnsThroughNorthWithoutAJump)
{
	// Each yaw is the one within 180 degrees of the last, as Minecraft's own: it never jumps a whole turn facing north.
	CHECK_NEAR(LookFromForward(ForwardFromLook({ 181.0f, 0.0f }), 179.0f)->yaw, 181.0, 1e-3);
	CHECK_NEAR(LookFromForward(ForwardFromLook({ 179.0f, 0.0f }), -179.0f)->yaw, -181.0, 1e-3);
	CHECK_NEAR(LookFromForward(ForwardFromLook({ 181.0f, 0.0f }), 539.0f)->yaw, 541.0, 1e-3);  // a turn and more on
	CHECK_NEAR(LookFromForward(ForwardFromLook({ -10.0f, 0.0f }), 10.0f)->yaw, -10.0, 1e-3);
}

TEST(LookTeleportTargetPutsTheEyeAtTheCamera)
```

- [x] **Step 2: Build and run the tests; the new one must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | grep -E "^FAIL|test\(s\)"
```
Expected: `FAIL LookTurnsThroughNorthWithoutAJump`, then `204 test(s), 3 failure(s)`.

- [x] **Step 3: Edit `src/link/Look.h` to say how the yaw goes on**

```cpp
// 0.001 of its length), the yaw is aLastYaw. Nothing for a zero vector. The yaw comes out in (-180, 180].
```
```cpp
// 0.001 of its length), the yaw is aLastYaw. Nothing for a zero vector. The yaw is the one within 180 degrees of
// aLastYaw, in no fixed range, as Minecraft's own: Minecraft sways its first-person hand by the yaw's change, so a
// yaw that wrapped from 180 to -180 facing north swung the held item.
```

- [x] **Step 4: Edit `src/link/Look.cpp` to keep the yaw within 180 degrees of the last**

```cpp
		yaw = float(std::atan2(-x, z) * kDegrees);
		if (yaw <= -180.0f)
		{
			yaw += 360.0f;
		}
```
```cpp
		// The turn from the last yaw, within 180 degrees either way (a half turn goes up).
		double turn = std::fmod(std::atan2(-x, z) * kDegrees - aLastYaw, 360.0);
		if (turn > 180.0)
		{
			turn -= 360.0;
		}
		else if (turn <= -180.0)
		{
			turn += 360.0;
		}
		yaw = float(aLastYaw + turn);
```

- [x] **Step 5: Edit `src/Plugin.cpp` for version 0.9.1**

```cpp
constexpr const char* kVersion = "0.9.0";
```
```cpp
constexpr const char* kVersion = "0.9.1";
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 9, 0);
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 9, 1);
```

- [x] **Step 6: Edit `CMakeLists.txt` for version 0.9.1**

```cmake
project(BlockRender VERSION 0.9.0 LANGUAGES C CXX)
```
```cmake
project(BlockRender VERSION 0.9.1 LANGUAGES C CXX)
```

- [x] **Step 7: Edit `docs/superpowers/specs/2026-10-02-minecraft-link-design.md` for the yaw**

```markdown
    straight up or down, where the forward vector's horizontal length is under 0.001, the last yaw is kept;
```
```markdown
    straight up or down, where the forward vector's horizontal length is under 0.001, the last yaw is kept;
  - the yaw is the one within 180° of the last, in no fixed range, as Minecraft's own is (see "Changes from step 2");
```
```markdown
  plugin only kept the heartbeat.
```
```markdown
  plugin only kept the heartbeat.

## Changes from step 2

Step 2's game run, in Part 2 of the plan, found these. The sections above already include them.
- **The yaw never wraps.** It was in (−180°, 180°], so it jumped from 180 to −180 whenever V turned through north.
  SkyCraft puts it straight into the player's yaw, where Minecraft's own never wraps, and Minecraft sways the
  first-person hand by the yaw's change. So the held item in the HUD swung and settled, at north only. Each yaw
  is now the one within 180° of the last.
```

- [x] **Step 8: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `204 test(s), 0 failure(s)`.

- [x] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render docs/superpowers/specs \
  && git commit -q -m "minecraft-link: Minecraft's yaw turns through north smoothly (0.9.1)" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

- [x] **Step 10: Deploy (the game must not be running)**

Run Task 27 Step 1's two commands. Expected: `0`, then the plugin's and the overlay's folders, and `installed`.

- [x] **Step 11: In the game: turn through north**

Minecraft can keep running from step 2, or the user starts the `CyberCraft` instance again. Ask the user to:
1. launch the game, load the free-roam save, and stand anywhere outdoors;
2. open CET's overlay, click Link, then the HUD button until it reads `HUD: always`;
3. turn slowly through heading 0 several times, both ways, then quickly once, watching the held item.

Expected:
- `BlockRender 0.9.1 loaded` in the log;
- the held item sways at north as it does at every other heading, without the jump.

Then send `set hud 1`, `link off` and `mode off`, each once the last shows in the status.

- [x] **Step 12: Record the check, and commit**

Add the fix and the check to step 2's results in `docs/spikes/2026-10-02-minecraft-link-results.md`, then commit:
```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes \
  && git commit -q -m "minecraft-link: the yaw fix, checked in the game" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 29: Close Part 2, and plan Part 3

- [x] **Step 1: Add "How Part 2 went" to this plan**

At the end: what ran as written, what changed, and what step 2 found.

- [x] **Step 2: Plan Part 3**

Use superpowers:writing-plans to add Part 3's tasks, from Task 30 on, to this plan: step 3 in the game, input. Verify
them as Part 2's were. Changes to the spec found while planning go in its "Changes while planning".

- [x] **Step 3: Commit**

Step 1 was committed on its own (`796ec82`), so this commit's message names only Part 3.
```bash
cd /d/Cyberpunk-Minecraft && git add docs/superpowers \
  && git commit -q -m "minecraft-link plan: Part 3 planned" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

## How Part 2 went

On 2026-10-02 and 03, inline, as the user asked.
- **Tasks 13 to 26 ran as written,** in the repo, by the runner that replayed them (`tools/plans/run_plan.py`,
  execute mode). Every build that had to fail failed with its expected message, and every count matched: 175 to 203
  C++ tests, and 25 Python ones, with no warnings. `mclink`'s self-check found Minecraft absent, and `luac` took the
  overlay. The 14 commits run from `372e718` to `a95b990`, and their 40 files are identical to the verified scratch
  copy's.
- **Task 27 ran as written,** with the user: [step 2's results](../../spikes/2026-10-02-minecraft-link-results.md).
  Every check passed. It departed from the plan in four places:
  - **`wipe` missed the field,** which had gone a block higher than V's feet. Two fills at its absolute coordinates
    cleared it;
  - **`/save-all` doesn't exist in singleplayer.** Minecraft had written the wall to disk at once anyway;
  - **`field` wasn't repeated for Step 8's bursts:** Step 6 had measured it, and a second would have gone a block off
    the same way;
  - **Step 13's results were written after Task 28's code,** so the user could check the fix in the same sitting.
- **Task 28 was added for the one defect step 2 found:** the held item jumped whenever V turned through north. It was
  written test-first, replayed from `a95b990`, executed (`e1f1c06`, BlockRender 0.9.1), and checked in the game
  (`2c35e2b`). The runner gained three step kinds for it: a test that must fail when run, steps done by hand, which
  stop the run, and ticking a task only up to its commit.
- **What step 2 found, for Part 3** (the results have the numbers):
  - **the link, the blocks and the HUD work in the game as designed:** our half of the latency is under a frame,
    bursts don't hitch, and the HUD costs 0.25 ms a frame at `hudscale 2`;
  - **Minecraft's eye leaves V's camera among blocks.** V can stand inside Minecraft's blocks, as in `field`'s layer.
    Minecraft then drops to crawling for about a second after V walks, and stays crawling under any block 1 to 2 m
    overhead, which moves its eye off V's camera and shows its in-block overlay in the HUD. Before Part 3 was
    planned, an `mclink` run settled it (the results' "Minecraft's eye among blocks"). Clicks follow the eye; in the
    open the eye stays at V's camera; and commands work from the held feet, not from our floor, as first guessed. So
    Part 3 builds on the open street. `mclink`'s status line gained the eye for that run (`tools/mclink/eye.txt`);
  - **latency samples from sets of several commands are wrong,** since each Enter replaces the mark. A click is one
    input, but the same rule applies to clicks in quick succession.

---

## Part 3: step 3, input

Tasks 30 to 40, planned after Part 2 closed (see "How Part 2 went"), from step 2's findings and the eye experiment.

**Goal:** BlockRender 0.10.0, with F8's build mode: the mouse buttons, the wheel and the keys 1 to 9 go to Minecraft
through SkyCraft's input ring, and are hidden from Cyberpunk, so V doesn't shoot, aim or switch weapons while the user
builds. Then run the spec's step 3 with the user: build mode on and off, each input blocked from the game and acted on
by Minecraft, building on the street and elsewhere, leaving through each menu key, click latency, and Alt-Tab with a
button held.

**How the game reads input,** from its code, read before writing any of this (the spec's "Cyberpunk's input", and its
"Changes while planning"): `Cyberpunk2077.exe` calls `GetRawInputData` in one place, its input system's handler for
`WM_INPUT`, into a 48-byte buffer, and ignores what the call returns. It takes the mouse's buttons, and whether the
wheel turned, only from `usButtonFlags`, and keys by virtual key, skipping 0xFF. So clearing a button's or the wheel's
flag hides it and keeps the movement, and a virtual key of 0xFF hides a key, as the spec planned.

**How it works:**
- **The patch** (Task 36) replaces the exe's import of `GetRawInputData`. It calls the original, then hands the event
  it copied to the router. The focus watch's thread starts with it.
- **Build mode** (Task 31) decides what each event does, from whether it's on, where the event's press went, whether
  the game is in front, and what the link said last frame:
  - F8 toggles it, and links first if the user hasn't;
  - in it, the mouse buttons, the wheel and the keys 1 to 9 are forwarded as SkyCraft's input events (with
    `Scancodes`, Task 30) and hidden from the game. The mouse buttons include the side buttons, which the user's
    Minecraft picks hotbar slots 8 and 9 with;
  - the keys that open the game's menus, and Shift+F6 for CET's overlay, leave it, and reach the game;
  - a press's repeats and release go where the press went;
  - it follows Minecraft's hotbar slot from the keys and the wheel.
- **The router** (Task 33) is what three threads share, behind one lock: whichever thread the game reads input on,
  through the patch; the focus watch's; and the main thread. It reads each `RAWINPUT` after checking its size, asks
  build mode, hides in place what Minecraft takes, and forwards it through the link. The main thread keeps it current
  once a frame, and build mode ends when Minecraft or V goes, or the link.
- **The focus watch** (Tasks 32 and 36): 10 times a second, whether the game has just lost the front. If it has, in
  build mode, Minecraft lets go of everything.
- **Requests and the status** (Tasks 34 and 35): `build on|off`, and a `build=` line.
- **The plugin** (Task 37) installs the patch at load, keeps the router current, shows the HUD in build mode
  (`hud 1`, the default), sets `SkyState`'s menu flag from it, and marks each click that goes to Minecraft for
  latency, as a typed command's Enter is. It's 0.10.0.
- **The overlay** (Task 38) shows build mode and the slot under its buttons.
- **The game run** (Task 39) follows the spec's "Step 3", on the open street, where Minecraft's eye is at V's camera.
  Task 40 closes Part 3 and the spike.

**Already verified:** as in Parts 1 and 2, all the code was compiled and tested in a scratch clone first. Then each of
the new units' tests was made to fail: 49 deliberate breaks of the new code, from F8's repeats to the wheel's
fractions and the router's size checks, each caught by the test meant for it. Tasks 30 to 38 were then replayed in a
fresh clone from `77dc397`, where Part 3's planning began, applying every step as written and checking every expected
result:
- zero compiler warnings at `/W4`;
- 232 C++ tests and 25 Python tests passing;
- `luac` accepting the overlay;
- the replayed files identical to the scratch clone's.

Tasks 36 and 37, the patch and the plugin, have no unit tests: only the game can test them, in Task 39. The overlay was
run once with CET stubbed out, showing the build line.

**The spec:** planning Part 3 changed it in eight places, which its "Changes while planning" lists after Part 2's.
Among them: how the game reads input, from its code; a press's repeats and release going where the press went; and
the side buttons, which the user chose to forward too.

**Test counts:** 204 at the start. Then 206, 219, 221, 230, 231 and 232 after Tasks 30 to 35, and 232 until Task 39.
The Python count stays at 25.

**The commands and rules** in "Before you start", and in Part 2's introduction for the game, apply here too. Part 3
adds:
- **The user's keys:** the game's bindings are its defaults, and the keyboard layout is Danish, whose letters and
  digits sit where a US keyboard's do, so the scan codes build mode watches are the keys the game binds. CET's overlay
  is Shift+F6 and FreeFly is F7 (`bindings.json`); F8 is free. The `CyberCraft` instance binds hotbar slots 8 and 9
  to the mouse's side buttons, not to the keys 8 and 9.
- **No faults:** the patch runs on whichever thread the game reads input on. The router checks every size before it
  reads or writes, and leaves alone anything it doesn't know.

## File map (Part 3)

| File | Responsibility |
|---|---|
| `src/input/Scancodes.*` | Keys to SDL's scancodes: SkyCraft's table |
| `src/input/BuildMode.*` | What each key and click does, in and out of build mode, and the hotbar slot |
| `src/input/FocusWatch.*` | Whether the game has just lost the front |
| `src/input/Router.*` | Each `RAWINPUT` event read, hidden and forwarded, between three threads |
| `src/input/RawInputHook.*` | The patch on the game's `GetRawInputData`, and the focus watch's thread |
| `src/Requests.*` | `build on\|off` |
| `src/Status.*` | The `build=` line |
| `src/Plugin.cpp` | The patch at load, the router once a frame, build mode's HUD and menu flag, click latency, and the unload |
| `tools/blockrender_hud/init.lua` | The overlay's line for build mode |
| `CMakeLists.txt` | The new sources and tests, and version 0.10.0 |
| `tests/TestScancodes.cpp`, `tests/TestBuildMode.cpp`, `tests/TestFocusWatch.cpp`, `tests/TestRouter.cpp` | One test file per new unit; the requests' and the status's tests grow in their own files |

---

### Task 30: Keys to SDL's scancodes, from SkyCraft's table

SkyCraft's input ring takes keys as SDL scancodes, which are USB HID usages. Raw Input reports a key's set-1 scan code
without its E0 prefix (`MakeCode`), and the prefix as a flag (`RI_KEY_E0`). SkyCraft's Skyrim side already has the
table, indexed by DirectInput's key codes, where an extended key's code is its scan code + 0x80 (`skse/src/Input.cpp`
at `bfcaf17`, MIT, as `extern/skycraft/LICENSE`): the spec's "`input/`". Build mode forwards only the keys 1 to 9 (SDL
30 to 38), but the table is SkyCraft's whole, every pair unchanged, for whatever is forwarded later.

**Files:**
- Create: `src/input/Scancodes.h`, `src/input/Scancodes.cpp`, `tests/TestScancodes.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestScancodes.cpp`**

```cpp
#include "Harness.h"

#include "input/Scancodes.h"

using cpmc::input::SdlScancode;

TEST(ScancodesMapTheKeysBuildModeForwards)
{
	for (uint16_t key = 1; key <= 9; ++key)
	{
		CHECK_EQ(SdlScancode(uint16_t(0x01 + key), false), uint16_t(29 + key));  // 1 to 9: SDL 30 to 38
	}
	CHECK_EQ(SdlScancode(0x0B, false), uint16_t(39));  // 0
	CHECK_EQ(SdlScancode(0x14, false), uint16_t(23));  // T, as Commands types it
	CHECK_EQ(SdlScancode(0x1C, false), uint16_t(40));  // Enter
	CHECK_EQ(SdlScancode(0x1E, false), uint16_t(4));   // A
	CHECK_EQ(SdlScancode(0x01, false), uint16_t(41));  // Esc
	CHECK_EQ(SdlScancode(0x42, false), uint16_t(65));  // F8
	CHECK_EQ(SdlScancode(0x57, false), uint16_t(68));  // F11
}

TEST(ScancodesTellExtendedKeysApart)
{
	CHECK_EQ(SdlScancode(0x1D, false), uint16_t(224));  // left Ctrl
	CHECK_EQ(SdlScancode(0x1D, true), uint16_t(228));   // right Ctrl
	CHECK_EQ(SdlScancode(0x1C, true), uint16_t(88));    // the keypad's Enter
	CHECK_EQ(SdlScancode(0x48, false), uint16_t(96));   // the keypad's 8
	CHECK_EQ(SdlScancode(0x48, true), uint16_t(82));    // Up
	CHECK_EQ(SdlScancode(0x2A, true), uint16_t(0));     // the fake Shift Windows sends around some extended keys
	CHECK_EQ(SdlScancode(0x59, false), uint16_t(0));    // not in the table
	CHECK_EQ(SdlScancode(0x9D, false), uint16_t(0));    // past set 1's keys: not the right Ctrl
	CHECK_EQ(SdlScancode(0xFF, true), uint16_t(0));     // a keyboard overrun
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestProcessUsage.cpp
```
```cmake
	tests/TestProcessUsage.cpp
	tests/TestScancodes.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'input/Scancodes.h'`.

- [x] **Step 4: Write `src/input/Scancodes.h`**

```cpp
#pragma once

#include <cstdint>

// Keys as Raw Input reports them, to the SDL scancodes (USB HID usages) SkyCraft's input ring takes (the spec's
// "input/"). Raw Input gives a key's set-1 scan code without its E0 prefix (RAWKEYBOARD's MakeCode), and the prefix as
// a flag (RI_KEY_E0). The table is SkyCraft's (skse/src/Input.cpp at bfcaf17, MIT, as extern/skycraft/LICENSE), which
// is indexed by DirectInput's key codes: an extended key's code is its scan code + 0x80.
namespace cpmc::input
{
// The key's SDL scancode, or 0 for a key SkyCraft's table doesn't have.
uint16_t SdlScancode(uint16_t aScanCode, bool aExtended);
}
```

- [x] **Step 5: Write `src/input/Scancodes.cpp`**

```cpp
#include "input/Scancodes.h"

#include <array>

namespace cpmc::input
{
namespace
{
// SkyCraft's kDikToSdl (skse/src/Input.cpp, MIT): DirectInput key code to SDL scancode.
constexpr auto kDikToSdl = [] {
	std::array<uint16_t, 256> t{};
	t[0x01] = 41;  // Esc
	for (int i = 0; i < 9; ++i)
	{
		t[0x02 + i] = uint16_t(30 + i);  // 1-9
	}
	t[0x0B] = 39;                                                          // 0
	t[0x0C] = 45, t[0x0D] = 46, t[0x0E] = 42, t[0x0F] = 43;                // - = Backspace Tab
	t[0x10] = 20, t[0x11] = 26, t[0x12] = 8, t[0x13] = 21, t[0x14] = 23;   // Q W E R T
	t[0x15] = 28, t[0x16] = 24, t[0x17] = 12, t[0x18] = 18, t[0x19] = 19;  // Y U I O P
	t[0x1A] = 47, t[0x1B] = 48, t[0x1C] = 40, t[0x1D] = 224;               // [ ] Enter LCtrl
	t[0x1E] = 4, t[0x1F] = 22, t[0x20] = 7, t[0x21] = 9, t[0x22] = 10;     // A S D F G
	t[0x23] = 11, t[0x24] = 13, t[0x25] = 14, t[0x26] = 15;                // H J K L
	t[0x27] = 51, t[0x28] = 52, t[0x29] = 53, t[0x2A] = 225, t[0x2B] = 49;   // ; ' ` LShift backslash
	t[0x2C] = 29, t[0x2D] = 27, t[0x2E] = 6, t[0x2F] = 25, t[0x30] = 5;      // Z X C V B
	t[0x31] = 17, t[0x32] = 16, t[0x33] = 54, t[0x34] = 55, t[0x35] = 56;    // N M , . /
	t[0x36] = 229, t[0x37] = 85, t[0x38] = 226, t[0x39] = 44, t[0x3A] = 57;  // RShift KP* LAlt Space Caps
	for (int i = 0; i < 10; ++i)
	{
		t[0x3B + i] = uint16_t(58 + i);  // F1-F10
	}
	t[0x45] = 83, t[0x46] = 71;                                               // NumLock ScrollLock
	t[0x47] = 95, t[0x48] = 96, t[0x49] = 97, t[0x4A] = 86;                   // KP7 KP8 KP9 KP-
	t[0x4B] = 92, t[0x4C] = 93, t[0x4D] = 94, t[0x4E] = 87;                   // KP4 KP5 KP6 KP+
	t[0x4F] = 89, t[0x50] = 90, t[0x51] = 91, t[0x52] = 98, t[0x53] = 99;     // KP1 KP2 KP3 KP0 KP.
	t[0x56] = 100, t[0x57] = 68, t[0x58] = 69;                                // OEM102 F11 F12
	t[0x9C] = 88, t[0x9D] = 228, t[0xB5] = 84, t[0xB7] = 70, t[0xB8] = 230;  // KPEnter RCtrl KP/ PrtSc RAlt
	t[0xC5] = 72, t[0xC7] = 74, t[0xC8] = 82, t[0xC9] = 75, t[0xCB] = 80;    // Pause Home Up PgUp Left
	t[0xCD] = 79, t[0xCF] = 77, t[0xD0] = 81, t[0xD1] = 78, t[0xD2] = 73;    // Right End Down PgDn Insert
	t[0xD3] = 76, t[0xDB] = 227, t[0xDC] = 231, t[0xDD] = 101;               // Delete LWin RWin Menu
	return t;
}();
}

uint16_t SdlScancode(uint16_t aScanCode, bool aExtended)
{
	return aScanCode < 0x80 ? kDikToSdl[aScanCode | (aExtended ? 0x80u : 0u)] : uint16_t(0);
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/ProcessUsage.cpp
)
```
```cmake
	src/ProcessUsage.cpp
	src/input/Scancodes.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `206 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: keys to SDL's scancodes, from SkyCraft's table" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 31: Build mode decides where each key and click goes

The spec's "Input", as a pure class that the router (Task 33) calls under its lock. Each keyboard or mouse event, with
where things stand (whether the game is in front, linked, V found, Minecraft there and in its world), becomes a
`Decision`: which mouse flags to clear, whether to make the key a no-op, what to forward to Minecraft's input ring,
and whether Minecraft must first let go of everything.
- **F8** toggles build mode, never reaches the game, and does nothing when it repeats. Unlinked, it asks the main
  thread to link first (`requestLink`), and the main thread then turns build mode on with `TurnOn`. Build mode goes on
  only with Minecraft in its world and V found; otherwise `Why()` says why it stayed off.
- **In build mode,** the left, right and middle buttons are forwarded as SDL's buttons 1, 3 and 2, the wheel as a
  scroll in units of 120 (positive up), and the keys 1 to 9 as SDL's 30 to 38. Each is hidden from the game.
- **The side buttons** go to Minecraft too, as SDL's buttons 4 and 5, which is how Minecraft 26.3 numbers
  `key.mouse.4` and `key.mouse.5` (`InputConstants`). The user's Minecraft picks hotbar slots 8 and 9 with them, and
  Cyberpunk binds neither (the spec's "Changes while planning"; the user chose this while Part 3 was planned). They
  change no block, so they aren't latency marks.
- **The menu keys** (Esc, I, M, J, P, K, H, T) and Shift+F6 leave build mode and reach the game. Shift is followed from
  every key event; the E0 "fake Shift" Windows sends around some extended keys isn't Shift.
- **A press's repeats and release go where the press went** (the spec's "Changes while planning"). Each button and
  number key remembers whether its press went to the game or to Minecraft. A button held as build mode goes on, V
  aiming say, stays the game's until released, so V stops aiming. When build mode ends, Minecraft lets go of
  everything at once, so its presses' repeats and releases then go nowhere: forwarding a repeat would press the key in
  Minecraft again. When the game loses the front, every press is forgotten, since it's let go of in another window.
- **The hotbar slot** is set by a forwarded number key's press, and moved by the wheel as Minecraft 26.3's
  `ScrollWheelHandler` moves it (its bytecode in the user's jar): notches accumulate, a turn the other way starts over,
  and an event moves the slot one step at most, by its whole notches' sign, up being to the left. Until a number key
  sets it, it's unknown, and a side button makes it unknown again: what it picks is Minecraft's own binding.
- **Nothing goes to Minecraft while the game isn't in front.** The game doesn't take Raw Input in the background, so
  this only guards against an event that arrives anyway.

**Files:**
- Create: `src/input/BuildMode.h`, `src/input/BuildMode.cpp`, `tests/TestBuildMode.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestBuildMode.cpp`**

```cpp
#include "Harness.h"

#include "input/BuildMode.h"

#include <Windows.h>

using namespace cpmc::input;

namespace
{
// Linked, V found, Minecraft in its world, and the game in front: build mode can go on.
Situation Ready()
{
	return { true, true, true, true, true };
}

RawKey Press(uint16_t aScanCode, bool aExtended = false)
{
	return { aScanCode, aExtended, false };
}

RawKey Release(uint16_t aScanCode, bool aExtended = false)
{
	return { aScanCode, aExtended, true };
}

constexpr uint16_t kF8 = 0x42;

// Build mode, turned on with F8.
BuildMode On()
{
	BuildMode mode;
	mode.Key(Press(kF8), Ready());
	mode.Key(Release(kF8), Ready());
	return mode;
}

bool Passes(const Decision& aDecision)
{
	return aDecision.hideButtons == 0 && !aDecision.hideKey && aDecision.forwarded == 0 && !aDecision.releaseAll;
}

bool Is(const proto::InputEvent& aEvent, uint16_t aType, uint16_t aCode, int32_t aA)
{
	return aEvent.type == aType && aEvent.code == aCode && aEvent.a == aA;
}
}

TEST(BuildModeStartsOffAndPassesEverything)
{
	BuildMode mode;
	CHECK(!mode.On());
	CHECK_STR(mode.Why(), "F8 turns it on");
	CHECK(!mode.Slot());
	CHECK(Passes(mode.Mouse({ RI_MOUSE_LEFT_BUTTON_DOWN, 0 }, Ready())));
	CHECK(Passes(mode.Mouse({ RI_MOUSE_LEFT_BUTTON_UP, 0 }, Ready())));
	CHECK(Passes(mode.Mouse({ RI_MOUSE_WHEEL, 120 }, Ready())));
	CHECK(Passes(mode.Mouse({ RI_MOUSE_BUTTON_4_DOWN, 0 }, Ready())));
	CHECK(Passes(mode.Key(Press(0x04), Ready())));  // 3
	CHECK(Passes(mode.Key(Press(0x01), Ready())));  // Esc
	CHECK_EQ(mode.Forwarded(), 0ull);
	CHECK_EQ(mode.Hidden(), 0ull);
}

TEST(BuildModeTogglesOnF8)
{
	BuildMode      mode;
	const Decision on = mode.Key(Press(kF8), Ready());
	CHECK(mode.On());
	CHECK(on.hideKey);  // F8 is build mode's alone
	CHECK(!on.releaseAll && on.forwarded == 0 && !on.requestLink);
	CHECK(mode.Key(Press(kF8), Ready()).hideKey);  // a repeat: hidden, and nothing else
	CHECK(mode.On());
	CHECK(mode.Key(Release(kF8), Ready()).hideKey);
	const Decision off = mode.Key(Press(kF8), Ready());
	CHECK(!mode.On());
	CHECK(off.hideKey && off.releaseAll);  // Minecraft lets go of everything
	CHECK_STR(mode.Why(), "F8 turned it off");
	CHECK(!mode.Key(Press(kF8, true), Ready()).hideKey);  // an extended 0x42 isn't F8
}

TEST(BuildModeF8LinksFirst)
{
	BuildMode mode;
	Situation unlinked = Ready();
	unlinked.linked = false;
	unlinked.inGame = false;
	const Decision first = mode.Key(Press(kF8), unlinked);
	CHECK(first.requestLink && first.hideKey);
	CHECK(!mode.On());
	CHECK(mode.TurnOn(Ready()));  // the main thread has linked
	CHECK(mode.On());
	CHECK(mode.Why().empty());
}

TEST(BuildModeGoesOnOnlyWithMinecraftInItsWorld)
{
	BuildMode mode;
	Situation situation = Ready();
	situation.minecraftInWorld = false;
	mode.Key(Press(kF8), situation);
	CHECK(!mode.On());
	CHECK_STR(mode.Why(), "Minecraft isn't in its world");
	situation.minecraft = false;
	CHECK(!mode.TurnOn(situation));
	CHECK_STR(mode.Why(), "Minecraft isn't there");
	situation = Ready();
	situation.inGame = false;
	CHECK(!mode.TurnOn(situation));
	CHECK_STR(mode.Why(), "V isn't found");
	CHECK(mode.TurnOn(Ready()));
}

TEST(BuildModeForwardsAndHidesTheButtons)
{
	BuildMode      mode = On();
	const Decision left = mode.Mouse({ RI_MOUSE_LEFT_BUTTON_DOWN, 0 }, Ready());
	CHECK_EQ(left.hideButtons, uint16_t(RI_MOUSE_LEFT_BUTTON_DOWN));
	CHECK_EQ(left.forwarded, size_t(1));
	CHECK(Is(left.forward[0], proto::kInMouseButton, 1, 1));
	CHECK(left.press);
	const Decision up = mode.Mouse({ RI_MOUSE_LEFT_BUTTON_UP, 0 }, Ready());
	CHECK(Is(up.forward[0], proto::kInMouseButton, 1, 0));
	CHECK(!up.press);
	// The right and middle buttons in one event: SDL numbers them 3 and 2.
	const Decision both = mode.Mouse({ RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_DOWN, 0 }, Ready());
	CHECK_EQ(both.hideButtons, uint16_t(RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_DOWN));
	CHECK_EQ(both.forwarded, size_t(2));
	CHECK(Is(both.forward[0], proto::kInMouseButton, 3, 1));
	CHECK(Is(both.forward[1], proto::kInMouseButton, 2, 1));
	CHECK(Passes(mode.Mouse({ RI_MOUSE_HWHEEL, 120 }, Ready())));  // the horizontal wheel stays the game's
	CHECK_STR(mode.Last(), "the middle button down");
}

TEST(BuildModeForwardsTheSideButtons)
{
	// Cyberpunk binds neither, and the user's Minecraft picks hotbar slots with them.
	BuildMode mode = On();
	mode.Key(Press(0x04), Ready());  // 3
	CHECK_EQ(mode.Slot().value_or(0), 3);
	const Decision back = mode.Mouse({ RI_MOUSE_BUTTON_4_DOWN, 0 }, Ready());
	CHECK_EQ(back.hideButtons, uint16_t(RI_MOUSE_BUTTON_4_DOWN));
	CHECK(Is(back.forward[0], proto::kInMouseButton, 4, 1));
	CHECK(!back.press);   // it changes no block: no latency to time
	CHECK(!mode.Slot());  // which slot it picks, if any, is Minecraft's binding
	const Decision both = mode.Mouse({ RI_MOUSE_BUTTON_4_UP | RI_MOUSE_BUTTON_5_DOWN, 0 }, Ready());
	CHECK_EQ(both.forwarded, size_t(2));
	CHECK(Is(both.forward[0], proto::kInMouseButton, 4, 0));
	CHECK(Is(both.forward[1], proto::kInMouseButton, 5, 1));
	CHECK_STR(mode.Last(), "mouse button 5 down");
	mode.Key(Press(0x0A), Ready());  // 9
	CHECK_EQ(mode.Slot().value_or(0), 9);
}

TEST(BuildModeForwardsTheWheelAndFollowsTheSlot)
{
	BuildMode      mode = On();
	const Decision three = mode.Key(Press(0x04), Ready());
	CHECK(three.hideKey);
	CHECK(Is(three.forward[0], proto::kInKey, 32, 1));  // SDL's 3
	CHECK(mode.Key(Release(0x04), Ready()).hideKey);
	CHECK_EQ(mode.Slot().value_or(0), 3);
	const Decision up = mode.Mouse({ RI_MOUSE_WHEEL, 120 }, Ready());
	CHECK_EQ(up.hideButtons, uint16_t(RI_MOUSE_WHEEL));
	CHECK(Is(up.forward[0], proto::kInScroll, 0, 120));
	CHECK_EQ(mode.Slot().value_or(0), 2);  // up is the slot to the left
	mode.Mouse({ RI_MOUSE_WHEEL, -120 }, Ready());
	CHECK_EQ(mode.Slot().value_or(0), 3);
	mode.Mouse({ RI_MOUSE_WHEEL, 360 }, Ready());  // three notches in one event move it one slot, as in Minecraft
	CHECK_EQ(mode.Slot().value_or(0), 2);
	mode.Mouse({ RI_MOUSE_WHEEL, 60 }, Ready());  // half a notch waits for the other half
	CHECK_EQ(mode.Slot().value_or(0), 2);
	mode.Mouse({ RI_MOUSE_WHEEL, 60 }, Ready());
	CHECK_EQ(mode.Slot().value_or(0), 1);
	mode.Mouse({ RI_MOUSE_WHEEL, 120 }, Ready());  // from 1 round to 9
	CHECK_EQ(mode.Slot().value_or(0), 9);
	mode.Mouse({ RI_MOUSE_WHEEL, 60 }, Ready());    // half a notch up waits,
	mode.Mouse({ RI_MOUSE_WHEEL, -120 }, Ready());  // but turning the other way starts over: a whole notch down
	CHECK_EQ(mode.Slot().value_or(0), 1);
	mode.Key(Press(0x0A), Ready());  // 9
	CHECK_EQ(mode.Slot().value_or(0), 9);
}

TEST(BuildModeTakesOnlyTheKeys1To9)
{
	BuildMode mode = On();
	CHECK(Is(mode.Key(Press(0x02), Ready()).forward[0], proto::kInKey, 30, 1));  // 1
	mode.Key(Press(0x0A), Ready());
	CHECK(Is(mode.Key(Release(0x0A), Ready()).forward[0], proto::kInKey, 38, 0));  // 9's release goes too
	CHECK(Passes(mode.Key(Release(0x05), Ready())));      // 4's release, its press never seen: the game's
	CHECK(Passes(mode.Key(Press(0x0B), Ready())));        // 0
	CHECK(Passes(mode.Key(Press(0x11), Ready())));        // W
	CHECK(Passes(mode.Key(Press(0x02, true), Ready())));  // an extended 0x02 isn't 1
	CHECK(Passes(mode.Key(Press(0x4F), Ready())));        // the keypad's 1
	CHECK(mode.On());
}

TEST(BuildModeLeavesOnTheMenuKeys)
{
	for (const int key : { 0x01, 0x17, 0x32, 0x24, 0x19, 0x25, 0x23, 0x14 })  // Esc I M J P K H T
	{
		BuildMode      mode = On();
		const Decision decision = mode.Key(Press(uint16_t(key)), Ready());
		CHECK(!mode.On());
		CHECK(decision.releaseAll);
		CHECK(!decision.hideKey && decision.forwarded == 0);  // the game opens its menu
	}
	BuildMode mode = On();
	mode.Key(Press(0x17), Ready());
	CHECK_STR(mode.Why(), "I left it");
	CHECK(Passes(mode.Key(Release(0x17), Ready())));
	mode = On();
	CHECK(Passes(mode.Key(Press(0x40), Ready())));  // F6 alone
	CHECK(mode.On());
	mode.Key(Press(0x36), Ready());                    // the right Shift
	CHECK(mode.Key(Press(0x40), Ready()).releaseAll);  // Shift+F6: CET's overlay
	CHECK_STR(mode.Why(), "Shift+F6 left it");
	mode = On();
	mode.Key(Press(0x2A, true), Ready());  // the fake Shift Windows sends around some extended keys isn't Shift
	CHECK(Passes(mode.Key(Press(0x40), Ready())));
	CHECK(mode.On());
	mode.Key(Press(0x01, true), Ready());  // an extended 0x01 isn't Esc
	CHECK(mode.On());
}

TEST(BuildModeSendsAReleaseWhereItsPressWent)
{
	BuildMode mode;
	CHECK(Passes(mode.Mouse({ RI_MOUSE_RIGHT_BUTTON_DOWN, 0 }, Ready())));  // V aiming before build mode
	CHECK(Passes(mode.Key(Press(0x03), Ready())));                          // and 2 held
	mode.Key(Press(kF8), Ready());
	mode.Key(Release(kF8), Ready());
	CHECK(mode.On());
	CHECK(Passes(mode.Key(Press(0x03), Ready())));                        // 2's repeat: still the game's
	CHECK(Passes(mode.Mouse({ RI_MOUSE_RIGHT_BUTTON_UP, 0 }, Ready())));  // so V stops aiming
	CHECK(mode.Key(Press(0x02), Ready()).hideKey);                        // 1, pressed now: Minecraft's
	// Minecraft's press, then build mode ends: Minecraft lets go of everything, so its repeats and release go nowhere.
	mode.Mouse({ RI_MOUSE_LEFT_BUTTON_DOWN, 0 }, Ready());
	CHECK(mode.Key(Press(kF8), Ready()).releaseAll);
	const Decision release = mode.Mouse({ RI_MOUSE_LEFT_BUTTON_UP, 0 }, Ready());
	CHECK_EQ(release.hideButtons, uint16_t(RI_MOUSE_LEFT_BUTTON_UP));
	CHECK_EQ(release.forwarded, size_t(0));
	const Decision repeat = mode.Key(Press(0x02), Ready());
	CHECK(repeat.hideKey && repeat.forwarded == 0);
	CHECK(mode.Key(Release(0x02), Ready()).hideKey);
	CHECK(Passes(mode.Key(Press(0x02), Ready())));  // a new press, with build mode off: the game's
}

TEST(BuildModeForwardsNothingWhileTheGameIsntInFront)
{
	BuildMode mode = On();
	Situation away = Ready();
	away.inFront = false;
	CHECK(Passes(mode.Mouse({ RI_MOUSE_LEFT_BUTTON_DOWN, 0 }, away)));
	CHECK(Passes(mode.Mouse({ RI_MOUSE_WHEEL, 120 }, away)));
	CHECK(Passes(mode.Key(Press(0x02), away)));
	CHECK(Passes(mode.Key(Press(kF8), away)));
	CHECK(mode.On());  // an F8 meant for another window doesn't toggle it
}

TEST(BuildModeLetsGoWhenTheGameLosesTheFront)
{
	BuildMode mode;
	mode.Mouse({ RI_MOUSE_RIGHT_BUTTON_DOWN, 0 }, Ready());  // the game's
	mode.Key(Press(kF8), Ready());
	mode.Key(Release(kF8), Ready());
	mode.Mouse({ RI_MOUSE_LEFT_BUTTON_DOWN, 0 }, Ready());  // Minecraft's
	CHECK(mode.LostFront());  // on: Minecraft lets go of everything
	CHECK(mode.On());         // and build mode stays on for when the user comes back
	// Both were let go of in another window. Back in the game, the next presses are Minecraft's.
	const Decision again = mode.Mouse({ RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_LEFT_BUTTON_DOWN, 0 }, Ready());
	CHECK_EQ(again.forwarded, size_t(2));
	CHECK(Is(again.forward[1], proto::kInMouseButton, 3, 1));
	mode.Key(Press(kF8), Ready());  // off, with F8 held as the front goes
	CHECK(!mode.On());
	CHECK(!mode.LostFront());       // off: nothing to let go of
	mode.Key(Press(kF8), Ready());  // F8 was let go of elsewhere: this is a press, not a repeat
	CHECK(mode.On());
}

TEST(BuildModeCountsWhatItForwardsAndHides)
{
	BuildMode mode = On();  // F8's press and release: 2 hidden
	mode.Mouse({ RI_MOUSE_RIGHT_BUTTON_DOWN, 0 }, Ready());
	mode.Mouse({ RI_MOUSE_RIGHT_BUTTON_UP, 0 }, Ready());
	mode.Mouse({ RI_MOUSE_WHEEL, -120 }, Ready());
	CHECK_EQ(mode.Forwarded(), 3ull);
	CHECK_EQ(mode.Hidden(), 5ull);
	CHECK_STR(mode.Last(), "the wheel, -120");
	CHECK(mode.TurnOff("a build off request"));
	CHECK(!mode.TurnOff("again"));
	CHECK_STR(mode.Why(), "a build off request");
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestScancodes.cpp
```
```cmake
	tests/TestScancodes.cpp
	tests/TestBuildMode.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'input/BuildMode.h'`.

- [x] **Step 4: Write `src/input/BuildMode.h`**

```cpp
#pragma once

#include <skycraft_protocol.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

// Build mode (the spec's "Input"): what becomes of each keyboard and mouse event the game reads. In build mode the mouse
// buttons, side buttons included, the wheel and the keys 1 to 9 go to Minecraft and are hidden from the game; F8
// toggles it; and the keys that open the game's menus leave it, and reach the game. A press's repeats and its release
// go where the press went, so neither side is left holding an input that only the other saw let go. Pure, and not
// thread-safe: the router (input/Router.h) holds its lock around every call.
namespace cpmc::input
{
namespace proto = skycraft::proto;

// A keyboard event, from RAWKEYBOARD.
struct RawKey
{
	uint16_t scanCode = 0;      // set 1, without its E0 prefix (MakeCode)
	bool     extended = false;  // the E0 prefix (RI_KEY_E0)
	bool     up = false;        // a release (RI_KEY_BREAK); a key held down repeats its press
};

// A mouse event's buttons and wheel, from RAWMOUSE. Its movement isn't build mode's business.
struct RawMouse
{
	uint16_t buttons = 0;  // usButtonFlags: RI_MOUSE_LEFT_BUTTON_DOWN and the rest, and RI_MOUSE_WHEEL
	int16_t  wheel = 0;    // usButtonData, with RI_MOUSE_WHEEL: notches x 120, positive away from the user (up)
};

// Where things stand: whether the game is in front, and what the link said last frame.
struct Situation
{
	bool inFront = false;    // the foreground window is the game's
	bool linked = false;     // the user's Link
	bool inGame = false;     // linked, with V found
	bool minecraft = false;  // Minecraft's heartbeat is fresh
	bool minecraftInWorld = false;
};

// What becomes of one event.
struct Decision
{
	static constexpr size_t kMost = 11;  // five buttons pressed and released, and the wheel

	uint16_t                             hideButtons = 0;      // mouse: the flags to clear, so the game doesn't see them
	bool                                 hideKey = false;      // keyboard: make the key a no-op for the game
	bool                                 releaseAll = false;   // build mode ended: Minecraft lets go of everything, first
	std::array<proto::InputEvent, kMost> forward{};            // then these go to Minecraft's input ring, in order
	size_t                               forwarded = 0;
	bool                                 press = false;        // a left, right or middle press went to Minecraft: a latency mark
	bool                                 requestLink = false;  // F8 while unlinked: the main thread links, then turns it on
};

class BuildMode
{
public:
	Decision Key(const RawKey& aKey, const Situation& aSituation);
	Decision Mouse(const RawMouse& aMouse, const Situation& aSituation);

	// On, from the main thread: a build request, or F8's link once it's done. True if it's on; if not, Why() says why.
	bool TurnOn(const Situation& aSituation);
	// Off, saying why: true if it was on, so Minecraft must let go of everything.
	bool TurnOff(std::string aWhy);
	// The game stopped being in front, so what's held is let go of in another window, where neither side sees it. True
	// if build mode is on, so Minecraft must let go of everything. Build mode stays on for when the user comes back.
	bool LostFront();

	bool               On() const { return on_; }
	const std::string& Why() const { return why_; }          // why it's off
	std::optional<int> Slot() const { return slot_; }        // Minecraft's hotbar slot, 1 to 9, as far as it's known
	uint64_t           Forwarded() const { return forwarded_; }  // events for Minecraft's input ring
	uint64_t           Hidden() const { return hidden_; }        // inputs hidden from the game
	const std::string& Last() const { return last_; }            // the last input forwarded

private:
	// Where a held input's press went.
	enum class Holder : uint8_t
	{
		None,
		Game,
		Minecraft,
		Neither,  // Minecraft's, until build mode ended and Minecraft let go of everything
	};

	static constexpr size_t kInputs = 14;  // the five mouse buttons, then the keys 1 to 9

	Holder Route(size_t aInput, bool aDown);
	void   Forward(Decision& aDecision, const proto::InputEvent& aEvent, std::string aWhat);
	void   Scroll(int16_t aWheel);

	bool                        on_ = false;
	std::string                 why_ = "F8 turns it on";
	std::array<Holder, kInputs> held_{};
	bool                        f8Down_ = false;
	bool                        leftShift_ = false;
	bool                        rightShift_ = false;
	std::optional<int>          slot_;
	double                      scroll_ = 0.0;  // the wheel's notches not yet whole, as Minecraft keeps them
	uint64_t                    forwarded_ = 0;
	uint64_t                    hidden_ = 0;
	std::string                 last_ = "nothing yet";
};
}
```

- [x] **Step 5: Write `src/input/BuildMode.cpp`**

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
constexpr uint16_t kF6 = 0x40;
constexpr uint16_t kF8 = 0x42;
constexpr uint16_t kLeftShift = 0x2A;
constexpr uint16_t kRightShift = 0x36;

// The keys that open the game's menus (Esc, I, M, J, P, K, H and T), and Shift with F6, which opens CET's overlay: its
// name, or null.
const char* MenuKey(const RawKey& aKey, bool aShift)
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
	case 0x14: return "T";
	case kF6: return aShift ? "Shift+F6" : nullptr;
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
constexpr size_t kButtonCount = std::size(kButtons);  // their holders come first, then the keys 1 to 9's
constexpr size_t kSideButtons = 3;                    // the first side button's index

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
	if (on_ && !aKey.up)
	{
		if (const char* menu = MenuKey(aKey, leftShift_ || rightShift_))
		{
			decision.releaseAll = TurnOff(std::format("{} left it", menu));
			return decision;  // and the game opens its menu
		}
	}
	if (!aKey.extended && aKey.scanCode >= 0x02 && aKey.scanCode <= 0x0A)
	{
		const int    digit = aKey.scanCode - 0x01;  // the keys 1 to 9
		const Holder to = Route(kButtonCount - 1 + size_t(digit), !aKey.up);
		if (to == Holder::Minecraft || to == Holder::Neither)
		{
			decision.hideKey = true;
			++hidden_;
		}
		if (to == Holder::Minecraft)
		{
			Forward(decision, { proto::kInKey, SdlScancode(aKey.scanCode, false), aKey.up ? 0 : 1, 0, 0 },
			        std::format("the key {} {}", digit, aKey.up ? "up" : "down"));
			if (!aKey.up)
			{
				slot_ = digit;
			}
		}
	}
	return decision;
}

Decision BuildMode::Mouse(const RawMouse& aMouse, const Situation& aSituation)
{
	Decision decision;
	if (!aSituation.inFront)
	{
		return decision;
	}
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
				if (i < kSideButtons)
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
	if ((aMouse.buttons & RI_MOUSE_WHEEL) && on_)
	{
		decision.hideButtons |= RI_MOUSE_WHEEL;
		++hidden_;
		Forward(decision, { proto::kInScroll, 0, aMouse.wheel, 0, 0 }, std::format("the wheel, {}", aMouse.wheel));
		Scroll(aMouse.wheel);
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
	else if (!aSituation.inGame)
	{
		why_ = aSituation.linked ? "V isn't found" : "it isn't linked";
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
	for (Holder& holder : held_)
	{
		if (holder == Holder::Minecraft)
		{
			holder = Holder::Neither;  // Minecraft lets go of it now: its repeats and release go nowhere
		}
	}
	return true;
}

bool BuildMode::LostFront()
{
	held_.fill(Holder::None);
	f8Down_ = false;
	leftShift_ = false;
	rightShift_ = false;
	return on_;
}

BuildMode::Holder BuildMode::Route(size_t aInput, bool aDown)
{
	static_assert(kInputs == kButtonCount + 9);
	Holder& holder = held_[aInput];
	if (aDown && holder == Holder::None)
	{
		holder = on_ ? Holder::Minecraft : Holder::Game;  // a press goes where build mode says
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

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/input/Scancodes.cpp
```
```cmake
	src/input/Scancodes.cpp
	src/input/BuildMode.cpp
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `219 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: build mode decides where each key and click goes" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 32: The focus watch's decision

The spec's `FocusWatch`: whether the game has just stopped being in front, true once, and not again until the game
has been back. Its thread, which checks ten times a second and tells the router, comes with the patch in Task 36. The
watch starts out as if the game were in front, so a plugin that loads while the user is in another window lets go
once, which costs nothing with build mode off.

**Files:**
- Create: `src/input/FocusWatch.h`, `src/input/FocusWatch.cpp`, `tests/TestFocusWatch.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestFocusWatch.cpp`**

```cpp
#include "Harness.h"

#include "input/FocusWatch.h"

using cpmc::input::FocusWatch;

TEST(FocusWatchLetsGoOnceOnLosingTheFront)
{
	FocusWatch watch;
	CHECK(!watch.LostFront(true));
	CHECK(watch.LostFront(false));
	CHECK(!watch.LostFront(false));  // still away: once is enough
	CHECK(!watch.LostFront(true));   // back
	CHECK(!watch.LostFront(true));
	CHECK(watch.LostFront(false));  // away again
}

TEST(FocusWatchLetsGoIfTheGameStartsBehind)
{
	FocusWatch watch;  // the plugin loaded while the user was in another window
	CHECK(watch.LostFront(false));
	CHECK(!watch.LostFront(false));
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestBuildMode.cpp
```
```cmake
	tests/TestBuildMode.cpp
	tests/TestFocusWatch.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'input/FocusWatch.h'`.

- [x] **Step 4: Write `src/input/FocusWatch.h`**

```cpp
#pragma once

// The focus watch's decision (the spec's "input/"): whether the game has just stopped being in front. Its thread
// (input/RawInputHook.h) checks ten times a second, so that a button held while the user switches windows is let go of
// in Minecraft even if the game stopped updating.
namespace cpmc::input
{
class FocusWatch
{
public:
	// True once when the game stops being in front, and not again until it has been back.
	bool LostFront(bool aInFront);

private:
	bool inFront_ = true;
};
}
```

- [x] **Step 5: Write `src/input/FocusWatch.cpp`**

```cpp
#include "input/FocusWatch.h"

namespace cpmc::input
{
bool FocusWatch::LostFront(bool aInFront)
{
	const bool lost = inFront_ && !aInFront;
	inFront_ = aInFront;
	return lost;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/input/BuildMode.cpp
```
```cmake
	src/input/BuildMode.cpp
	src/input/FocusWatch.cpp
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `221 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: the focus watch's decision" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 33: The router, between the hook, the focus watch and the main thread

Build mode's decisions, applied to the bytes `GetRawInputData` copied out, and shared by three threads (the spec's
"Input" and "Threads"):
- **`Inspect`,** from the patch on the game's input thread. A mouse event needs the header and a whole `RAWMOUSE`, a
  keyboard event the header and a whole `RAWKEYBOARD`; anything shorter, or of another type, is left alone. The
  struct is copied out and back with `memcpy`, since the game's buffer is just bytes. A mouse event with no button or
  wheel flag (movement, or the horizontal wheel) returns before taking the lock, so turning costs next to nothing.
- **Hiding,** in place: the hidden flags cleared from `usButtonFlags`, which keeps `lLastX` and `lLastY`, and a hidden
  key's virtual key set to 0xFF and its scan code to 0. The game skips both (the spec's "Cyberpunk's input").
- **Forwarding** goes through `Link::Push`, which may be called from any thread, under the router's lock. The link is
  `SetLink`'s, from the main thread: null until the mapping opens, and again at unload, so nothing pushes into the
  mapping after it's unmapped. A release-all goes first whenever build mode ends.
- **The main thread,** once a frame after the link's update, gives `Update` whether the user linked and what the link
  says. Build mode ends there, with a release-all, if Minecraft has gone or left its world, or the user unlinked, or V
  isn't found. It also turns build mode on and off (`TurnOn`, `TurnOff`), takes F8's link request, and takes the time
  of the last click that went to Minecraft, for latency.
- **The focus watch** calls `FrontLost`: in build mode, a release-all.
- **The log** gets a line when build mode goes on or off, or stays off for a new reason, when F8 asks for the link,
  and when the focus watch lets go. `LogQueue` takes lines from any thread.

The tests drive it with synthetic `RAWINPUT` events, through a real `Link` on the mapping's layout in plain memory,
and read what lands in its input ring.

**Files:**
- Create: `src/input/Router.h`, `src/input/Router.cpp`, `tests/TestRouter.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestRouter.cpp`**

```cpp
#include "Harness.h"

#include "input/Router.h"

#include <Windows.h>

#include <cstring>
#include <vector>

using namespace cpmc;
using namespace cpmc::input;

namespace
{
bool g_inFront = true;

bool FakeFront()
{
	return g_inFront;
}

// The mapping's layout in plain memory, with a link on it: what the router forwards lands in its input ring.
struct Mapping
{
	Mapping()
	{
		base = static_cast<uint8_t*>(VirtualAlloc(nullptr, proto::kMappingBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
		link = new link::Link(base);
	}
	~Mapping()
	{
		delete link;
		VirtualFree(base, 0, MEM_RELEASE);
	}

	std::vector<proto::InputEvent> Inputs()  // everything forwarded since the last call
	{
		uint8_t* const                 ring = base + proto::kOffInputRing;
		const uint64_t                 head = *reinterpret_cast<uint64_t*>(ring);
		uint64_t&                      tail = *reinterpret_cast<uint64_t*>(ring + 0x40);
		std::vector<proto::InputEvent> events;
		for (; tail < head; ++tail)
		{
			proto::InputEvent event;
			std::memcpy(&event, ring + 0x80 + (tail % proto::kInputRingEntries) * sizeof event, sizeof event);
			events.push_back(event);
		}
		return events;
	}

	uint8_t*    base = nullptr;
	link::Link* link = nullptr;
};

// What the link says once a frame: Minecraft there and in its world, and V found while linked.
link::Link::Status Linked()
{
	link::Link::Status state;
	state.minecraft = true;
	state.inWorld = true;
	state.inGame = true;
	return state;
}

// A router on a link, in build mode.
struct Building
{
	Building() : router(log, &FakeFront)
	{
		g_inFront = true;
		router.SetLink(mapping.link);
		router.Update(true, Linked());
		CHECK(!router.TurnOn());
	}

	LogQueue log;
	Mapping  mapping;
	Router   router;
};

// A mouse event as GetRawInputData copies it: the header, then RAWMOUSE.
std::vector<uint8_t> Mouse(USHORT aButtons, SHORT aWheel = 0, LONG aX = 0, LONG aY = 0)
{
	RAWINPUT input{};
	input.header.dwType = RIM_TYPEMOUSE;
	input.header.dwSize = sizeof(RAWINPUTHEADER) + sizeof(RAWMOUSE);
	input.data.mouse.usButtonFlags = aButtons;
	input.data.mouse.usButtonData = USHORT(aWheel);
	input.data.mouse.lLastX = aX;
	input.data.mouse.lLastY = aY;
	std::vector<uint8_t> bytes(input.header.dwSize);
	std::memcpy(bytes.data(), &input, bytes.size());
	return bytes;
}

// A key event: the header, then RAWKEYBOARD, with the virtual key the game reads.
std::vector<uint8_t> Key(USHORT aScanCode, USHORT aVirtualKey, bool aUp = false)
{
	RAWINPUT input{};
	input.header.dwType = RIM_TYPEKEYBOARD;
	input.header.dwSize = sizeof(RAWINPUTHEADER) + sizeof(RAWKEYBOARD);
	input.data.keyboard.MakeCode = aScanCode;
	input.data.keyboard.Flags = aUp ? RI_KEY_BREAK : RI_KEY_MAKE;
	input.data.keyboard.VKey = aVirtualKey;
	input.data.keyboard.Message = aUp ? WM_KEYUP : WM_KEYDOWN;
	std::vector<uint8_t> bytes(input.header.dwSize);
	std::memcpy(bytes.data(), &input, bytes.size());
	return bytes;
}

RAWMOUSE MouseOf(const std::vector<uint8_t>& aBytes)
{
	RAWMOUSE mouse;
	std::memcpy(&mouse, aBytes.data() + sizeof(RAWINPUTHEADER), sizeof mouse);
	return mouse;
}

RAWKEYBOARD KeyOf(const std::vector<uint8_t>& aBytes)
{
	RAWKEYBOARD key;
	std::memcpy(&key, aBytes.data() + sizeof(RAWINPUTHEADER), sizeof key);
	return key;
}

void Inspect(Router& aRouter, std::vector<uint8_t>& aEvent)
{
	aRouter.Inspect(aEvent.data(), uint32_t(aEvent.size()));
}

bool Logged(LogQueue& aLog, const std::string& aLine)
{
	for (const std::string& line : aLog.Drain())
	{
		if (line == aLine)
		{
			return true;
		}
	}
	return false;
}
}

TEST(RouterHidesAButtonAndKeepsItsMovement)
{
	Building building;
	auto     click = Mouse(RI_MOUSE_LEFT_BUTTON_DOWN, 0, 5, -3);
	Inspect(building.router, click);
	const RAWMOUSE mouse = MouseOf(click);
	CHECK_EQ(mouse.usButtonFlags, USHORT(0));  // the game sees no press
	CHECK_EQ(mouse.lLastX, LONG(5));           // but the look keeps working
	CHECK_EQ(mouse.lLastY, LONG(-3));
	const auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].type == proto::kInMouseButton && inputs[0].code == 1 && inputs[0].a == 1);
	CHECK(building.router.TakePress().has_value());  // a latency mark
	CHECK(!building.router.TakePress());
	// The release, with the horizontal wheel in the same event: that stays the game's.
	auto release = Mouse(RI_MOUSE_LEFT_BUTTON_UP | RI_MOUSE_HWHEEL, 120);
	Inspect(building.router, release);
	CHECK_EQ(MouseOf(release).usButtonFlags, USHORT(RI_MOUSE_HWHEEL));
	auto wheel = Mouse(RI_MOUSE_WHEEL, -120);
	Inspect(building.router, wheel);
	CHECK_EQ(MouseOf(wheel).usButtonFlags, USHORT(0));
	auto side = Mouse(RI_MOUSE_BUTTON_5_DOWN);  // the user's Minecraft picks hotbar slots with the side buttons
	Inspect(building.router, side);
	CHECK_EQ(MouseOf(side).usButtonFlags, USHORT(0));
	const auto rest = building.mapping.Inputs();
	CHECK(rest.size() == 3 && rest[0].a == 0 && rest[1].type == proto::kInScroll && rest[1].a == -120 && rest[2].code == 5);
}

TEST(RouterMakesAKeyANoOp)
{
	Building building;
	auto     one = Key(0x02, '1');
	Inspect(building.router, one);
	const RAWKEYBOARD key = KeyOf(one);
	CHECK_EQ(key.VKey, USHORT(0xFF));  // the game skips it
	CHECK_EQ(key.MakeCode, USHORT(0));
	CHECK_EQ(key.Message, UINT(WM_KEYDOWN));
	const auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].type == proto::kInKey && inputs[0].code == 30 && inputs[0].a == 1);
	CHECK(!building.router.TakePress());  // only buttons are marks
	auto w = Key(0x11, 'W');
	Inspect(building.router, w);
	CHECK_EQ(KeyOf(w).VKey, USHORT('W'));  // V still walks
	CHECK(building.mapping.Inputs().empty());
}

TEST(RouterLeavesEverythingElseAlone)
{
	Building building;
	auto     move = Mouse(0, 0, 7, 7);
	Inspect(building.router, move);
	CHECK_EQ(building.router.Report().read, 1ull);
	// Too short for its type: left as it is, and not read.
	auto short1 = Mouse(RI_MOUSE_LEFT_BUTTON_DOWN);
	building.router.Inspect(short1.data(), uint32_t(sizeof(RAWINPUTHEADER) + sizeof(RAWMOUSE) - 1));
	CHECK_EQ(MouseOf(short1).usButtonFlags, USHORT(RI_MOUSE_LEFT_BUTTON_DOWN));
	auto short2 = Key(0x02, '1');
	building.router.Inspect(short2.data(), uint32_t(sizeof(RAWINPUTHEADER) + sizeof(RAWKEYBOARD) - 1));
	CHECK_EQ(KeyOf(short2).VKey, USHORT('1'));
	building.router.Inspect(short2.data(), 0);
	building.router.Inspect(nullptr, 64);
	// Another device's type, with a press's bytes in it.
	auto hid = Mouse(RI_MOUSE_LEFT_BUTTON_DOWN);
	hid[0] = RIM_TYPEHID;
	Inspect(building.router, hid);
	CHECK_EQ(MouseOf(hid).usButtonFlags, USHORT(RI_MOUSE_LEFT_BUTTON_DOWN));
	CHECK(building.mapping.Inputs().empty());
	CHECK_EQ(building.router.Report().read, 1ull);
}

TEST(RouterPassesEverythingOutsideBuildMode)
{
	LogQueue log;
	Mapping  mapping;
	Router   router(log, &FakeFront);
	g_inFront = true;
	router.SetLink(mapping.link);
	router.Update(true, Linked());
	auto click = Mouse(RI_MOUSE_RIGHT_BUTTON_DOWN);
	Inspect(router, click);
	CHECK_EQ(MouseOf(click).usButtonFlags, USHORT(RI_MOUSE_RIGHT_BUTTON_DOWN));
	auto three = Key(0x04, '3');
	Inspect(router, three);
	CHECK_EQ(KeyOf(three).VKey, USHORT('3'));
	CHECK(mapping.Inputs().empty());
	CHECK(!router.On());
	auto f8 = Key(0x42, VK_F8);
	Inspect(router, f8);  // F8 turns it on, and is hidden
	CHECK(router.On());
	CHECK_EQ(KeyOf(f8).VKey, USHORT(0xFF));
	CHECK(Logged(log, "build mode on"));
}

TEST(RouterTakesF8sLinkRequest)
{
	LogQueue log;
	Mapping  mapping;
	Router   router(log, &FakeFront);
	g_inFront = true;
	router.SetLink(mapping.link);
	link::Link::Status state = Linked();
	state.inGame = false;
	router.Update(false, state);
	auto f8 = Key(0x42, VK_F8);
	Inspect(router, f8);
	CHECK(!router.On());
	CHECK(router.TakeLinkRequest());
	CHECK(!router.TakeLinkRequest());
	router.Update(true, Linked());  // the main thread linked
	CHECK(!router.TurnOn());
	CHECK(router.On());
	router.TurnOff("a build off request");
	state.inWorld = false;
	router.Update(true, state);
	CHECK_STR(router.TurnOn().value_or("on"), "Minecraft isn't in its world");
	CHECK(Logged(log, "build mode stays off: Minecraft isn't in its world"));
}

TEST(RouterEndsBuildModeWhenMinecraftOrVGoes)
{
	struct Case
	{
		bool        linked, minecraft, inWorld, inGame;
		const char* why;
	};
	for (const Case& c : { Case{ true, false, false, false, "Minecraft has gone" },
	                       Case{ true, true, false, true, "Minecraft left its world" }, Case{ false, true, true, false, "unlinked" },
	                       Case{ true, true, true, false, "V isn't found" } })
	{
		Building           building;
		link::Link::Status state;
		state.minecraft = c.minecraft;
		state.inWorld = c.inWorld;
		state.inGame = c.inGame;
		building.router.Update(c.linked, state);
		CHECK(!building.router.On());
		CHECK_STR(building.router.Report().why, c.why);
		const auto inputs = building.mapping.Inputs();
		CHECK(inputs.size() == 1 && inputs[0].type == proto::kInReleaseAll);  // Minecraft lets go of everything
	}
}

TEST(RouterLetsGoWhenTheGameLosesTheFront)
{
	Building building;
	auto     use = Mouse(RI_MOUSE_RIGHT_BUTTON_DOWN);
	Inspect(building.router, use);
	building.mapping.Inputs();
	building.router.FrontLost();
	const auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].type == proto::kInReleaseAll);
	CHECK(building.router.On());
	CHECK(Logged(building.log, "the game lost the front in build mode: Minecraft lets go of everything"));
	building.router.TurnOff("a build off request");
	building.mapping.Inputs();
	building.router.FrontLost();
	CHECK(building.mapping.Inputs().empty());  // off: nothing to let go of
	// A click in another window that reached the game anyway isn't Minecraft's.
	CHECK(!building.router.TurnOn());
	g_inFront = false;
	auto away = Mouse(RI_MOUSE_LEFT_BUTTON_DOWN);
	Inspect(building.router, away);
	CHECK_EQ(MouseOf(away).usButtonFlags, USHORT(RI_MOUSE_LEFT_BUTTON_DOWN));
	CHECK(building.mapping.Inputs().empty());
	g_inFront = true;
}

TEST(RouterPushesNothingOnceTheLinkIsGone)
{
	Building building;
	building.router.SetLink(nullptr);  // the plugin unloading
	auto click = Mouse(RI_MOUSE_LEFT_BUTTON_DOWN);
	Inspect(building.router, click);
	building.router.TurnOff("the plugin unloads");
	CHECK(building.mapping.Inputs().empty());
}

TEST(RouterReportsBuildMode)
{
	Building building;
	auto     three = Key(0x04, '3');
	Inspect(building.router, three);
	auto up = Mouse(RI_MOUSE_WHEEL, 120, 1, 1);
	Inspect(building.router, up);
	const InputReport on = building.router.Report();
	CHECK(on.on);
	CHECK_EQ(on.slot.value_or(0), 2);
	CHECK_EQ(on.read, 2ull);
	CHECK_EQ(on.forwarded, 2ull);
	CHECK_EQ(on.hidden, 2ull);
	CHECK_STR(on.last, "the wheel, 120");
	building.mapping.Inputs();
	auto esc = Key(0x01, VK_ESCAPE);
	Inspect(building.router, esc);
	CHECK_EQ(KeyOf(esc).VKey, USHORT(VK_ESCAPE));  // the game opens its menu
	const auto inputs = building.mapping.Inputs();
	CHECK(inputs.size() == 1 && inputs[0].type == proto::kInReleaseAll);  // and Minecraft lets go of everything
	CHECK(!building.router.On());
	CHECK_STR(building.router.Report().why, "Esc left it");
	CHECK(Logged(building.log, "build mode off: Esc left it"));
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestFocusWatch.cpp
```
```cmake
	tests/TestFocusWatch.cpp
	tests/TestRouter.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'input/Router.h'`.

- [x] **Step 4: Write `src/input/Router.h`**

```cpp
#pragma once

#include "LogQueue.h"
#include "input/BuildMode.h"
#include "link/Link.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

// Where the game's keyboard and mouse events go (the spec's "Input"), between three threads: the game's input thread,
// through the GetRawInputData patch (input/RawInputHook.h); the focus watch's; and the main thread, which keeps the link
// and what it says about Minecraft and V current, and turns build mode on and off. One lock guards build mode and the
// link, so once the main thread takes the link away, nothing pushes into the mapping.
namespace cpmc::input
{
// Whether the foreground window belongs to this process.
bool GameInFront();

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
};

class Router
{
public:
	using FrontCheck = bool (*)();
	explicit Router(LogQueue& aLog, FrontCheck aInFront = &GameInFront);

	// From the patch, once GetRawInputData has copied an event into aData, aBytes of it. A keyboard or mouse event that
	// build mode takes is hidden from the game in place, and forwarded. Anything else, or too short, is left alone.
	void Inspect(void* aData, uint32_t aBytes);

	// From the main thread. The link: once the mapping opens, and null again at unload.
	void SetLink(link::Link* aLink);
	// Once a frame, after the link's update: whether the user linked, and what the link says. Build mode ends if
	// Minecraft or V is gone, or the link.
	void Update(bool aLinked, const link::Link::Status& aState);
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

private:
	// With the lock held.
	void Apply(const Decision& aDecision, bool aWasOn, const std::string& aWhyBefore);
	bool Off(std::string aWhy);
	void Push(const proto::InputEvent& aEvent);

	LogQueue&             log_;
	FrontCheck            inFront_;
	mutable std::mutex    mutex_;
	BuildMode             mode_;
	link::Link*           link_ = nullptr;
	Situation             situation_;  // the main thread's part: the link, Minecraft and V
	bool                  linkRequested_ = false;
	std::optional<double> press_;
	std::atomic<uint64_t> read_{ 0 };
};
}
```

- [x] **Step 5: Write `src/input/Router.cpp`**

```cpp
#include "input/Router.h"

#include "Measurements.h"

#include <Windows.h>

#include <cstring>
#include <utility>

namespace cpmc::input
{
namespace
{
// The mouse flags build mode may take: the five buttons' presses and releases, and the wheel. An event without them is
// movement, or the horizontal wheel, and stays the game's without taking the lock.
constexpr USHORT kTaken = RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_LEFT_BUTTON_UP | RI_MOUSE_RIGHT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_UP |
                          RI_MOUSE_MIDDLE_BUTTON_DOWN | RI_MOUSE_MIDDLE_BUTTON_UP | RI_MOUSE_BUTTON_4_DOWN | RI_MOUSE_BUTTON_4_UP |
                          RI_MOUSE_BUTTON_5_DOWN | RI_MOUSE_BUTTON_5_UP | RI_MOUSE_WHEEL;
}

bool GameInFront()
{
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	return process == GetCurrentProcessId();
}

Router::Router(LogQueue& aLog, FrontCheck aInFront) : log_(aLog), inFront_(aInFront)
{
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
		if ((mouse.usButtonFlags & kTaken) == 0)
		{
			return;
		}
		const bool      inFront = inFront_();
		std::lock_guard lock(mutex_);
		Situation       situation = situation_;
		situation.inFront = inFront;
		const bool        wasOn = mode_.On();
		const std::string whyBefore = mode_.Why();
		const Decision    decision = mode_.Mouse({ mouse.usButtonFlags, int16_t(mouse.usButtonData) }, situation);
		if (decision.hideButtons != 0)
		{
			// The game reads buttons and the wheel only from these flags, and the movement from its own fields, which stay.
			mouse.usButtonFlags = USHORT(mouse.usButtonFlags & ~decision.hideButtons);
			std::memcpy(body, &mouse, sizeof mouse);
		}
		Apply(decision, wasOn, whyBefore);
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
		const std::string whyBefore = mode_.Why();
		const Decision    decision = mode_.Key({ key.MakeCode, (key.Flags & RI_KEY_E0) != 0, (key.Flags & RI_KEY_BREAK) != 0 }, situation);
		if (decision.hideKey)
		{
			// A no-op: the game skips a key whose virtual key is 0xFF.
			key.MakeCode = 0;
			key.VKey = 0xFF;
			std::memcpy(body, &key, sizeof key);
		}
		Apply(decision, wasOn, whyBefore);
	}
}

void Router::SetLink(link::Link* aLink)
{
	std::lock_guard lock(mutex_);
	link_ = aLink;
}

void Router::Update(bool aLinked, const link::Link::Status& aState)
{
	std::lock_guard lock(mutex_);
	situation_.linked = aLinked;
	situation_.inGame = aState.inGame;
	situation_.minecraft = aState.minecraft;
	situation_.minecraftInWorld = aState.inWorld;
	if (mode_.On())
	{
		const char* why = !aState.minecraft  ? "Minecraft has gone"
		                  : !aState.inWorld  ? "Minecraft left its world"
		                  : !aLinked         ? "unlinked"
		                  : !aState.inGame   ? "V isn't found"
		                                     : nullptr;
		if (why)
		{
			Off(why);
		}
	}
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
	return { mode_.On(), mode_.Why(), mode_.Slot(), read_.load(), mode_.Forwarded(), mode_.Hidden(), mode_.Last() };
}

void Router::FrontLost()
{
	std::lock_guard lock(mutex_);
	if (mode_.LostFront())
	{
		Push({ proto::kInReleaseAll, 0, 0, 0, 0 });
		log_.Push("the game lost the front in build mode: Minecraft lets go of everything");
	}
}

void Router::Apply(const Decision& aDecision, bool aWasOn, const std::string& aWhyBefore)
{
	if (aDecision.releaseAll)
	{
		Push({ proto::kInReleaseAll, 0, 0, 0, 0 });
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
}

bool Router::Off(std::string aWhy)
{
	if (!mode_.TurnOff(std::move(aWhy)))
	{
		return false;
	}
	Push({ proto::kInReleaseAll, 0, 0, 0, 0 });
	log_.Push("build mode off: " + mode_.Why());
	return true;
}

void Router::Push(const proto::InputEvent& aEvent)
{
	if (link_)
	{
		link_->Push(aEvent);  // a full ring drops it, and the link counts it
	}
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/input/FocusWatch.cpp
```
```cmake
	src/input/FocusWatch.cpp
	src/input/Router.cpp
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `230 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: the router, between the hook, the focus watch and the main thread" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 34: Requests for build mode

The spec's "Requests": `build on|off`, as F8. The plugin applies it in Task 37: on links first, if the user hasn't, and
goes on after the link's next update; off ends build mode, and Minecraft lets go of everything.

**Files:**
- Modify: `tests/TestRequests.cpp`, `src/Requests.h`, `src/Requests.cpp`

- [x] **Step 1: Edit `tests/TestRequests.cpp` to test the build requests**

```cpp
TEST(RequestsTypeIntoMinecraft)
```
```cpp
TEST(RequestsForBuildMode)
{
	std::string error;
	const auto  on = cpmc::ParseRequest("build on\n", error);
	CHECK(on.has_value());
	CHECK(on->kind == cpmc::Request::Kind::Build);
	CHECK_STR(on->what, "on");
	CHECK_STR(cpmc::ParseRequest("build off", error).value_or(cpmc::Request{}).what, "off");
	CHECK(!cpmc::ParseRequest("build", error));
	CHECK_STR(error, "build takes on or off");
	CHECK(!cpmc::ParseRequest("build on now", error));
}

TEST(RequestsTypeIntoMinecraft)
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2065: 'Build': undeclared identifier`.

- [x] **Step 3: Edit `src/Requests.h` to add the request**

```cpp
//   link on|off                (spike 4: the in-game flag)
```
```cpp
//   link on|off                (spike 4: the in-game flag)
//   build on|off               (build mode, as F8: on links first, if it isn't linked)
```
```cpp
		Link,
		Floor,
		Mc,
	};

	Kind        kind = Kind::Log;
```
```cpp
		Link,
		Floor,
		Mc,
		Build,
	};

	Kind        kind = Kind::Log;
```

- [x] **Step 4: Edit `src/Requests.cpp` to parse it as `link` is parsed**

```cpp
	if (command == "link")
	{
		if (arguments != 1 || !OneOf(words[1], { "on", "off" }))
		{
			return Fail(aError, "link takes on or off");
		}
		return Request{ Request::Kind::Link, std::string(words[1]) };
```
```cpp
	if (command == "link" || command == "build")
	{
		if (arguments != 1 || !OneOf(words[1], { "on", "off" }))
		{
			return Fail(aError, std::string(command) + " takes on or off");
		}
		return Request{ command == "link" ? Request::Kind::Link : Request::Kind::Build, std::string(words[1]) };
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `231 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: requests for build mode" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 35: Build mode's line in the status

The spec's "Measurements": a `build=` line after `link=`. On or off with why; the slot, or that a number key sets it;
then, since the start, the inputs forwarded to Minecraft and hidden from the game, the last one forwarded, those
dropped with the input ring full, and the keyboard and mouse events the patch has read. That last count climbs as soon
as the user moves the mouse, so it shows the patch is on the game's path before build mode is ever on. The latency
line's hint now says a click in build mode is a mark too.

**Files:**
- Modify: `tests/TestStatus.cpp`, `src/Status.h`, `src/Status.cpp`

- [x] **Step 1: Edit `tests/TestStatus.cpp` to test the build line**

```cpp
	CHECK(Has(status, "latency=no samples yet: type a command while V stands still"));
	CHECK(Has(status, "burst=none yet"));
	CHECK(Has(status, "processes=Cyberpunk 50% of a core, 100 MB; Minecraft not running"));
}
```
```cpp
	CHECK(Has(status, "latency=no samples yet: type a command, or click in build mode, while V stands still"));
	CHECK(Has(status, "burst=none yet"));
	CHECK(Has(status, "processes=Cyberpunk 50% of a core, 100 MB; Minecraft not running"));
}

TEST(StatusWritesBuildMode)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	link.inputHooked = true;
	link.buildMode = true;
	link.slot = 3;
	link.inputsRead = 5120;
	link.inputsForwarded = 14;
	link.inputsHidden = 15;
	link.lastInput = "the right button up";
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "build=on; slot 3; 14 inputs forwarded to Minecraft and 15 hidden from the game, the last: "
	                                      "the right button up; 0 dropped with the input ring full; GetRawInputData hooked, 5120 "
	                                      "keyboard and mouse events read"));
	link.inputHooked = false;
	link.buildMode = false;
	link.buildWhy = "Esc left it";
	link.slot.reset();
	link.inputsDropped = 2;
	report.link = link;
	const std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "build=off: Esc left it; slot unknown until a number key; 14 inputs forwarded to Minecraft and 15 hidden from "
	                  "the game, the last: the right button up; 2 dropped with the input ring full; GetRawInputData not hooked (see "
	                  "the log)"));
	CHECK(status.find("link=") < status.find("build=") && status.find("build=") < status.find("positions="));
}
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'inputHooked': is not a member of 'cpmc::LinkReport'`.

- [x] **Step 3: Edit `src/Status.h` to add build mode's fields**

```cpp
// Spike 4's lines (the spec's "Measurements"): the link, the render ring, Minecraft's atlas, the HUD, latency, the last
// burst, and both games' processes. "In the last second" is from one status to the next.
```
```cpp
// Spike 4's lines (the spec's "Measurements"): the link, build mode, the render ring, Minecraft's atlas, the HUD, latency,
// the last burst, and both games' processes. "In the last second" is from one status to the next.
```
```cpp
	Vec3d                floorCentre;  // Minecraft axes; y is the floor's height
```
```cpp
	Vec3d                floorCentre;  // Minecraft axes; y is the floor's height
	// Build mode, and the GetRawInputData patch; counts since the start
	bool               inputHooked = false;
	bool               buildMode = false;
	std::string        buildWhy;             // why it's off
	std::optional<int> slot;                 // Minecraft's hotbar slot, once a number key has set it
	uint64_t           inputsRead = 0;       // keyboard and mouse events
	uint64_t           inputsForwarded = 0;  // to Minecraft
	uint64_t           inputsHidden = 0;     // from the game
	uint64_t           inputsDropped = 0;    // with the input ring full
	std::string        lastInput;            // the last forwarded
```
```cpp
// Lines of "key=value": mode, frames, blocks, gpu, frame, streamline, hooks, request; with a link, its lines (link,
```
```cpp
// Lines of "key=value": mode, frames, blocks, gpu, frame, streamline, hooks, request; with a link, its lines (link, build,
```

- [x] **Step 4: Edit `src/Status.cpp` to write the line**

```cpp
	                   aLink.inGame ? "in game" : "not in game");
```
```cpp
	                   aLink.inGame ? "in game" : "not in game");
	out += std::format("build={}; {}; {} inputs forwarded to Minecraft and {} hidden from the game, the last: {}; {} dropped with the "
	                   "input ring full; {}\n",
	                   aLink.buildMode ? std::string("on") : "off: " + OneLine(aLink.buildWhy),
	                   aLink.slot ? std::format("slot {}", *aLink.slot) : std::string("slot unknown until a number key"),
	                   aLink.inputsForwarded, aLink.inputsHidden, OneLine(aLink.lastInput), aLink.inputsDropped,
	                   aLink.inputHooked ? std::format("GetRawInputData hooked, {} keyboard and mouse events read", aLink.inputsRead)
	                                     : std::string("GetRawInputData not hooked (see the log)"));
```
```cpp
	           : std::string("latency=no samples yet: type a command while V stands still\n");
```
```cpp
	           : std::string("latency=no samples yet: type a command, or click in build mode, while V stands still\n");
```

- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `232 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: build mode's line in the status" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 36: The patch on the game's GetRawInputData, and the focus watch's thread

The spec's `RawInputHook`, in `render_runtime`, since only the game can test it:
- **The patch:** `FindImport` finds `Cyberpunk2077.exe`'s import of `GetRawInputData` from `user32.dll`, as spike 3
  found Streamline's, and `ApplyImport` points it at the replacement. The original, and the router, are stored before
  the patch, so no call reaches the replacement without them. If something else patched the import first, the
  original is whatever was there.
- **The replacement** calls the original, and hands the router only an event copied out: `RID_INPUT`, a buffer, the
  header size this build knows, and a result neither 0 nor -1. A size query, a header-only call, or a failure passes
  untouched.
- **The focus watch's thread** (`CreateThread`, which reports a failure rather than throwing) checks ten times a second
  until `RemoveRawInputHook`. Nothing waits for it to end: the router it calls is never destroyed, and by then has no
  link.

**Files:**
- Create: `src/input/RawInputHook.h`, `src/input/RawInputHook.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write `src/input/RawInputHook.h`**

```cpp
#pragma once

#include "LogQueue.h"
#include "input/Router.h"

// The patch on Cyberpunk2077.exe's import of GetRawInputData (the spec's "Input"). The game reads every key and click
// through it, in one place: its input system's window-message handler, for each WM_INPUT. The replacement calls the
// original, then hands the event it copied to the router, which hides from the game what Minecraft takes. The focus
// watch's thread starts and stops with it. Only the game can test this file.
namespace cpmc::input
{
// Patches the import, and starts the focus watch. False, with nothing patched, if the exe doesn't import it by name.
bool InstallRawInputHook(Router& aRouter, LogQueue& aLog);

// Puts the import back, and stops the focus watch. A call already in the replacement finishes through the router,
// which outlives it, as the focus watch does.
void RemoveRawInputHook();
}
```

- [x] **Step 2: Write `src/input/RawInputHook.cpp`**

```cpp
#include "input/RawInputHook.h"

#include "hooks/ImportPatch.h"
#include "input/FocusWatch.h"

#include <Windows.h>

#include <atomic>
#include <optional>

namespace cpmc::input
{
namespace
{
using GetRawInputDataFn = UINT(WINAPI*)(HRAWINPUT, UINT, LPVOID, PUINT, UINT);

GetRawInputDataFn          g_original = nullptr;
Router*                    g_router = nullptr;
std::optional<ImportPatch> g_patch;
std::atomic<bool>          g_watching{ false };

UINT WINAPI GetRawInputDataHook(HRAWINPUT aInput, UINT aCommand, LPVOID aData, PUINT aSize, UINT aHeaderSize)
{
	const UINT copied = g_original(aInput, aCommand, aData, aSize, aHeaderSize);
	// Only an event copied out, with the header this build knows. A size query (no buffer), or a failure, passes.
	if (aCommand == RID_INPUT && aData && aHeaderSize == sizeof(RAWINPUTHEADER) && copied != UINT(-1) && copied != 0)
	{
		g_router->Inspect(aData, copied);
	}
	return copied;
}

// The focus watch: ten times a second, whether the game has just stopped being in front. Nothing waits for it to end.
DWORD WINAPI WatchFocus(LPVOID)
{
	FocusWatch watch;
	while (g_watching.load())
	{
		Sleep(100);
		if (watch.LostFront(GameInFront()) && g_watching.load())
		{
			g_router->FrontLost();
		}
	}
	return 0;
}
}

bool InstallRawInputHook(Router& aRouter, LogQueue& aLog)
{
	std::optional<ImportPatch> patch = FindImport(GetModuleHandleW(nullptr), "user32.dll", "GetRawInputData");
	if (!patch)
	{
		aLog.Push("the game doesn't import GetRawInputData by name: input isn't hooked, so build mode can't work");
		return false;
	}
	// Both before the patch, so no call reaches the replacement without them.
	g_router = &aRouter;
	g_original = reinterpret_cast<GetRawInputDataFn>(patch->original);
	if (!ApplyImport(*patch, reinterpret_cast<void*>(&GetRawInputDataHook)))
	{
		aLog.Push("GetRawInputData's import couldn't be patched: input isn't hooked, so build mode can't work");
		return false;
	}
	g_patch = *patch;
	g_watching = true;
	const HANDLE thread = CreateThread(nullptr, 0, &WatchFocus, nullptr, 0, nullptr);
	if (thread)
	{
		CloseHandle(thread);
	}
	aLog.Push(thread ? "GetRawInputData hooked; the focus watch runs"
	                 : "GetRawInputData hooked, but the focus watch didn't start: switching windows won't let go of held inputs");
	return true;
}

void RemoveRawInputHook()
{
	g_watching = false;
	if (g_patch)
	{
		RestoreImport(*g_patch);
		g_patch.reset();
	}
}
}
```

- [x] **Step 3: Edit `CMakeLists.txt` to add it to the runtime**

```cmake
	src/render/Renderer.cpp
)
```
```cmake
	src/render/Renderer.cpp
	src/input/RawInputHook.cpp
)
```

- [x] **Step 4: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `232 test(s), 0 failure(s)`.

- [x] **Step 5: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: the patch on the game's GetRawInputData, and the focus watch's thread" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 37: The plugin hands keys and clicks to Minecraft in build mode (0.10.0)

`Plugin.cpp` gains input (the spec's "`Plugin.cpp`" and "Input"):
- **At load,** before the Streamline and D3D12 hooks: the router, then the patch and the focus watch. Until the user
  links or presses F8, the router passes everything.
- **Once the mapping opens,** the router gets the link.
- **Each frame:**
  - F8's link request, if there is one, links (`F8: link on: …` in the request line) and asks for build mode;
  - `HudWanted` and `SkyState`'s menu flag take build mode from the router: the HUD shows in build mode (`hud 1`), and
    Minecraft stops releasing held keys every frame;
  - after the link's update, the router's: build mode ends if Minecraft or V has gone, or the link. Then build mode
    goes on, if F8 or `build on` asked for it, with the outcome in the request line;
  - a click that went to Minecraft marks latency, as a typed command's Enter does.
- **Requests:** `build on|off`, which `BuildRequest` applies as F8. `LinkRequest` takes what asked for the link, for
  the request line.
- **The status's build line,** from the router's report, whether the patch is in, and the link's dropped inputs.
- **At unload,** first: build mode off, so Minecraft lets go of everything; the patch put back and the focus watch
  stopped; then the router loses the link, before the link and the mapping go.
- **The version** is 0.10.0.

**Files:**
- Modify: `src/Plugin.cpp`, `CMakeLists.txt`

- [x] **Step 1: Edit `src/Plugin.cpp` to route input**

```cpp
// through SkyCraft's shared memory: Minecraft's player follows V, and Minecraft's blocks and HUD are drawn in the game.
```
```cpp
// through SkyCraft's shared memory: Minecraft's player follows V, Minecraft's blocks and HUD are drawn in the game, and
// in build mode (F8) the mouse buttons, the wheel and the keys 1 to 9 are Minecraft's.
```
```cpp
#include "hooks/StreamlineHooks.h"
```
```cpp
#include "hooks/StreamlineHooks.h"
#include "input/RawInputHook.h"
#include "input/Router.h"
```
```cpp
#include <string>
```
```cpp
#include <string>
#include <string_view>
```
```cpp
constexpr const char* kVersion = "0.9.1";
```
```cpp
constexpr const char* kVersion = "0.10.0";
```
```cpp
bool                      g_hooked = false;  // Streamline's imports are patched

// Made at load and never destroyed: the hooks may call them until the process ends.
cpmc::FrameLog* g_frameLog = nullptr;
cpmc::Dumps*    g_dumps = nullptr;
cpmc::Renderer* g_renderer = nullptr;
```
```cpp
bool                      g_hooked = false;       // Streamline's imports are patched
bool                      g_inputHooked = false;  // GetRawInputData's import is patched

// Made at load and never destroyed: the hooks may call them until the process ends.
cpmc::FrameLog*      g_frameLog = nullptr;
cpmc::Dumps*         g_dumps = nullptr;
cpmc::Renderer*      g_renderer = nullptr;
cpmc::input::Router* g_router = nullptr;  // where keys and clicks go (the spec's "Input")
```
```cpp
std::optional<double>            g_lastEnter;               // the link's last Enter, once marked
```
```cpp
std::optional<double>            g_lastEnter;               // the link's last Enter, once marked
bool                             g_buildWanted = false;     // F8's link, or build on: build mode, after the link's next update
```
```cpp
		g_link = new cpmc::link::Link(g_mapping.Base());
```
```cpp
		g_link = new cpmc::link::Link(g_mapping.Base());
		g_router->SetLink(g_link);
```
```cpp
	g_hudWanted = cpmc::HudWanted(g_hudSetting, false, state.inWorld);  // build mode comes with input, in Part 3
```
```cpp
	const bool buildMode = g_router->On();
	g_hudWanted = cpmc::HudWanted(g_hudSetting, buildMode, state.inWorld);
```
```cpp
	frame.buildMode = false;
	frame.takeHud = g_hudWanted;
	const uint64_t drainedBefore = state.drainedBytes;
	g_link->Update(frame, g_linkSettings, g_sink);
```
```cpp
	frame.buildMode = buildMode;
	frame.takeHud = g_hudWanted;
	const uint64_t drainedBefore = state.drainedBytes;
	g_link->Update(frame, g_linkSettings, g_sink);
	g_router->Update(g_linked, state);  // build mode ends if Minecraft or V has gone, or the link
	if (std::exchange(g_buildWanted, false))
	{
		const std::optional<std::string> why = g_router->TurnOn();
		Report(why ? "build on: it stays off: " + *why : "build on: the mouse buttons, the wheel and 1 to 9 are Minecraft's");
	}
```
```cpp
		g_burst.Start(*state.lastEnter, "mc " + g_lastTyped);
```
```cpp
		g_burst.Start(*state.lastEnter, "mc " + g_lastTyped);
	}
	if (const std::optional<double> press = g_router->TakePress())
	{
		g_latency.Mark(*press);  // a click that went to Minecraft
```
```cpp
// link on|off (the spec's "Linking"): linking also switches drawing to gbuffer if it's off.
void LinkRequest(bool aOn)
```
```cpp
// link on|off (the spec's "Linking"): linking also switches drawing to gbuffer if it's off. aBy says what asked, when it
// wasn't a link request.
void LinkRequest(bool aOn, std::string_view aBy = {})
```
```cpp
		return Report("link on, but drawing stays off: " + why);
	}
	const std::string link = g_link ? "Minecraft's player follows V" : "it starts once the mapping opens, which is " + g_mappingState;
	Report(std::format("link on: {}; drawing is {}", link, g_renderer->ModeName()));
```
```cpp
		return Report(std::format("{}link on, but drawing stays off: {}", aBy, why));
	}
	const std::string link = g_link ? "Minecraft's player follows V" : "it starts once the mapping opens, which is " + g_mappingState;
	Report(std::format("{}link on: {}; drawing is {}", aBy, link, g_renderer->ModeName()));
}

// build on|off (the spec's "Requests"): as F8. On links first, if it isn't linked, and goes on after the link's next
// update, once it knows whether V and Minecraft are there.
void BuildRequest(bool aOn)
{
	if (!aOn)
	{
		return Report(g_router->TurnOff("a build off request") ? "build off: Minecraft lets go of everything"
		                                                         : "build off: it's off already");
	}
	if (g_router->On())
	{
		return Report("build on: it's on already");
	}
	if (!g_linked)
	{
		LinkRequest(true, "build on: ");
	}
	g_buildWanted = true;
```
```cpp
	case cpmc::Request::Kind::Mc: TypeRequest(request->what); break;
```
```cpp
	case cpmc::Request::Kind::Mc: TypeRequest(request->what); break;
	case cpmc::Request::Kind::Build: BuildRequest(request->what == "on"); break;
```
```cpp
	report.camera = g_camera;
```
```cpp
	report.camera = g_camera;
	const cpmc::input::InputReport input = g_router->Report();
	report.inputHooked = g_inputHooked;
	report.buildMode = input.on;
	report.buildWhy = input.why;
	report.slot = input.slot;
	report.inputsRead = input.read;
	report.inputsForwarded = input.forwarded;
	report.inputsHidden = input.hidden;
	report.lastInput = input.last;
```
```cpp
		report.floorCentre = state.floorCentre;
```
```cpp
		report.floorCentre = state.floorCentre;
		report.inputsDropped = state.inputsDropped;
```
```cpp
	}
	UpdateLink(seconds);
```
```cpp
	}
	if (g_link && g_router->TakeLinkRequest())
	{
		LinkRequest(true, "F8: ");  // F8 while unlinked: the link, then build mode once the link has seen V
		g_buildWanted = true;
	}
	UpdateLink(seconds);
```
```cpp
		g_renderer = new cpmc::Renderer(g_log, g_pluginDir);
```
```cpp
		g_renderer = new cpmc::Renderer(g_log, g_pluginDir);
		// Input first: it passes everything until the user links or presses F8.
		g_router = new cpmc::input::Router(g_log);
		g_inputHooked = cpmc::input::InstallRawInputHook(*g_router, g_log);
```
```cpp
	case RED4ext::v1::EMainReason::Unload:
```
```cpp
	case RED4ext::v1::EMainReason::Unload:
		if (g_router)
		{
			g_router->TurnOff("the plugin unloads");  // Minecraft lets go of everything
			cpmc::input::RemoveRawInputHook();
			g_router->SetLink(nullptr);  // from here on, nothing pushes into the mapping
		}
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 9, 1);
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 10, 0);
```

- [x] **Step 2: Edit `CMakeLists.txt` for version 0.10.0**

```cmake
project(BlockRender VERSION 0.9.1 LANGUAGES C CXX)
```
```cmake
project(BlockRender VERSION 0.10.0 LANGUAGES C CXX)
```

- [x] **Step 3: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `232 test(s), 0 failure(s)`.

- [x] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: the plugin hands keys and clicks to Minecraft in build mode (0.10.0)" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 38: The overlay's line for build mode

The spec's "Status and the overlay": a line under the buttons with build mode and the hotbar slot, the first two parts
of the status's `build=` line: `Build mode (F8): on, slot 3`, or `Build mode (F8): off: Esc left it, slot unknown until
a number key`. There's no button for it: with CET's overlay open, its clicks would go to Minecraft, and Shift+F6 leaves
build mode anyway.

**Files:**
- Modify: `tools/blockrender_hud/init.lua`

- [x] **Step 1: Edit `tools/blockrender_hud/init.lua` to show build mode**

```lua
-- HUD shows, through blockrender.request in this folder. Delete this folder to remove it.
local status = nil       -- status.txt's lines, except the structures
local structures = {}    -- { name, min = { x, y, z }, max = { x, y, z } }, in world coordinates
local hudSetting = nil   -- the hud line's setting: "never", "in build mode" or "always"
```
```lua
-- HUD shows, through blockrender.request in this folder. A line under them shows build mode, which F8 toggles, and
-- Minecraft's hotbar slot. Delete this folder to remove it.
local status = nil       -- status.txt's lines, except the structures
local structures = {}    -- { name, min = { x, y, z }, max = { x, y, z } }, in world coordinates
local hudSetting = nil   -- the hud line's setting: "never", "in build mode" or "always"
local build = nil        -- the build line's first two parts: on, or off and why; and the slot
```
```lua
    hudSetting = nil
    for line in file:lines() do
        local name, a, b, c, d, e, f = line:match("^structure=(%S+) (%S+) (%S+) (%S+) (%S+) (%S+) (%S+)$")
        hudSetting = line:match("^hud=([^,]+),") or hudSetting
```
```lua
    hudSetting = nil
    build = nil
    for line in file:lines() do
        local name, a, b, c, d, e, f = line:match("^structure=(%S+) (%S+) (%S+) (%S+) (%S+) (%S+) (%S+)$")
        hudSetting = line:match("^hud=([^,]+),") or hudSetting
        local mode, slot = line:match("^build=([^;]*); ([^;]*);")
        if mode ~= nil then
            build = mode .. ", " .. slot
        end
```
```lua
        end
        ImGui.Separator()
```
```lua
        end
        if build ~= nil then
            ImGui.Text("Build mode (F8): " .. build)
        end
        ImGui.Separator()
```

- [x] **Step 2: Check the overlay's syntax**

```bash
luac -p /d/Cyberpunk-Minecraft/spikes/block-render/tools/blockrender_hud/init.lua && echo "syntax ok"
```
Expected: `syntax ok`.

- [x] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "minecraft-link: the overlay's line for build mode" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 39: In the game: step 3

Step 3 of the spec's "The steps", with the user: BlockRender 0.10.0 in the game, linked to SkyCraft's Minecraft in the
`CyberCraft` instance, at spike 3's corner (world x −116.6, y −2005.2) with the same save. The building stays on the
open street, where Minecraft's eye is at V's camera (the results' "Minecraft's eye among blocks").

Task 27's commands come back again and again: sending a request, the log's newest lines, the status, five readings a
second apart, and Minecraft's side. The overlay shows build mode under its buttons, and the status's `build=` line its
counts. The log's lines come in two streams, the request lines at once and the rest at the frame's end, so lines
listed together below may come in another order.

Note what you see as you go: Step 12 records it. If a check fails, stop and find the cause with
superpowers:systematic-debugging before going on. Fixes become new tasks, as in Part 2. An input that still reaches
the game gets risk 1's fallback as a new task, its events' type changed to HID instead, and is checked again.

**Files:**
- Modify: `docs/spikes/2026-10-02-minecraft-link-results.md`

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
- the plugin's folder holds `BlockRender.dll`, `BlockRender.pdb` and `shaders`, with spike 3's `atlas.bin` and
  `atlas.json`, and the logs and dumps;
- the overlay's folder holds `init.lua`;
- then `installed`.

The second configure clears `CP2077_DIR` again.

- [x] **Step 2: In the game: load, without Minecraft**

Ask the user to check that Minecraft isn't running, then to launch the game, load the free-roam save, go to the
corner, and stand still with open street ahead. Then read the log's first lines and the status. Expected:
- `GetRawInputData hooked; the focus watch runs`, `BlockRender 0.10.0 loaded` and `hooked`, then
  `the mapping: created`;
- in the status, `build=off: F8 turns it on; slot unknown until a number key; 0 inputs forwarded to Minecraft and 0
  hidden from the game, the last: nothing yet; 0 dropped with the input ring full; GetRawInputData hooked, … keyboard
  and mouse events read`;
- under the overlay's buttons, `Build mode (F8): off: F8 turns it on, slot unknown until a number key`.

Ask the user to move the mouse for a few seconds, and read the status again: the events read must have grown, which
shows the patch is on the game's path. Then ask the user to check that, with build mode off, the game takes input as
always: draw a weapon with 1 or the wheel, aim with the right button, and put the weapon away.

Then ask the user to press F8 once. Minecraft isn't running, so F8 links, and build mode stays off. Expected:
- in the log, `F8: linking first, then build mode`, `request: F8: link on: Minecraft's player follows V; drawing is
  gbuffer`, `build mode stays off: Minecraft isn't there`, and `request: build on: it stays off: Minecraft isn't there`;
- in the status, `mode=gbuffer`, `link=linked; …; Minecraft absent; in game`, and `build=off: Minecraft isn't there`.

- [x] **Step 3: In the game: start Minecraft, and fill the hotbar**

Ask the user to start the `CyberCraft` instance from the Modrinth App, and to say when its window has gone. The link is
on, so Minecraft's player goes straight to V. Expected:
- Minecraft's side: `linked to Skyrim (pid …)` with Cyberpunk's process ID, `Skyrim link up`, and
  `opening mirror world`;
- the log: `link: Minecraft is there (process …)`, a floor at V's feet, and `Minecraft's atlas: 2048 x 2576, 5 levels,
  built in … ms`;
- the status: `link=linked; …; in its world, … frames/s; in game`, Minecraft's eye at V's camera in `positions=`, and
  still `build=off: Minecraft isn't there`, the reason F8 last gave.

Then send these, each once the last shows in the status's `request=` line, and read the log's `typed:` lines:
```text
mc creative
mc /item replace entity @s hotbar.0 with stone 64
mc /item replace entity @s hotbar.1 with gold_block 64
mc /item replace entity @s hotbar.2 with diamond_block 64
```
In creative mode a click breaks a block at once, and the first three slots hold blocks that look different on the
street.

- [x] **Step 4: In the game: build mode on and off**

Ask the user to press F8, look, press F8 again, look, and press it once more, and to say what they saw each time.
Expected:
- the first: the log's `build mode on`; the status's `build=on; slot unknown until a number key; …`; the overlay's
  `Build mode (F8): on, …`; and Minecraft's HUD over the game, with stone, gold and diamond in its first three slots,
  and `hud=in build mode, scale 2, showing`;
- the second: `build mode off: F8 turned it off`, and the HUD gone;
- the third: on again, and the HUD back.

F8 itself goes to neither game: the counts' hidden inputs include F8's presses and releases.

- [x] **Step 5: In the game: each input, Minecraft's and not the game's**

With build mode off (F8), ask the user to draw a weapon that shows a stray input without alarming the street: a melee
weapon if V has one, which a left click that reached the game would swing, and a right click raise to block. If V's
combat gadget is a grenade, a middle click that reached the game would throw it, so nobody should be near. Then F8
again, with the street a few metres ahead. One input at a time, ask the user what Minecraft did, in its HUD or on the
street, and whether V did anything, and read the status's `build=` line after each:

| Input | Minecraft | V | The `build=` line |
|---|---|---|---|
| 1 | selects slot 1: stone | doesn't switch weapons | `slot 1`; the last `the key 1 up` |
| a right click on the street | places stone where the crosshair is | doesn't block or aim | the last `the right button up` |
| 2, then a right click | gold, where the crosshair is | | `slot 2` |
| 3, then a right click | diamond | | `slot 3` |
| a left click on the gold | breaks it | doesn't swing | the last `the left button up` |
| 2, then the wheel a notch down and a notch up | the selection to slot 3 and back to 2 | doesn't switch weapons | the slot follows: 3, then 2 |
| 1, then a middle click on the diamond | picks it: the selection jumps to slot 3 | throws nothing, tags nothing | still `slot 1`: a pick is Minecraft's own, as the spec says |
| 4 to 7 | each selects its slot | doesn't switch weapons | the slot follows |
| 8 and 9 | nothing: this instance binds slots 8 and 9 to the side buttons | doesn't switch weapons | `slot 8`, `slot 9`, though Minecraft's stays at 7 |
| the side button for slot 8, then the one for 9 | selects slot 8, then 9 | nothing | `slot unknown until a number key`; the last `mouse button 5 up` or `mouse button 4 up` |

Expected after all of them: every forwarded input hidden too, so the hidden count is the forwarded one plus F8's own
presses and releases, and `0 dropped`. Ask for a screenshot with the HUD and the blocks. If V acted on an input, stop:
see the top of this task.

- [x] **Step 6: In the game: building on the street, walking, and building elsewhere**

Ask the user to build freely for a minute on the open street ahead, placing and breaking blocks, but not to stand in
or under what they build: there Minecraft crawls, and its clicks land short (the results' "Minecraft's eye among
blocks"). Then to walk about 20 m along the street with build mode on, and build there too. Expected:
- blocks land where the crosshair points, on the street at V's feet height;
- in `positions=`, Minecraft's eye follows V's camera, and the floor's epoch goes up once V is 16 m from its centre;
- no `render ring:` lines and no new errors in the log.

Ask for a screenshot at each spot.

- [x] **Step 7: In the game: leaving through each menu key**

For each of Esc, I, M, J, P, K, H and T, then Shift+F6: with build mode on, ask the user to press the key, click
something in what opens (a tab, say), close it, and press F8 again. Read the status after each key. Expected for each:
- the log's `build mode off: <key> left it`, and the status's `build=off: <key> left it`;
- the HUD gone, and the menu open and taking clicks as usual;
- after F8, `build mode on` again.

Shift+F6 opens CET's overlay, whose buttons take clicks as usual too.

- [x] **Step 8: In the game: click latency**

With build mode on, slot 1 (stone), and V standing still with the street ahead: ask the user to right-click once on the
street, then left-click the block it placed, and so on, alternately, five times each, a second or two apart. Read the
log's `latency:` lines as they come. Expected: a line for each click, with both halves:
- **Minecraft's,** from the click to its section's arrival: up to a 20 Hz tick, 50 ms, and a frame or two more;
- **ours,** from the arrival to the first list that draws it: under a frame, as step 2 found for commands.

Single clicks only: each click replaces a mark still waiting for its section, as each Enter does.

- [x] **Step 9: In the game: Alt-Tab with a button held**

With build mode on, ask the user to look up at the sky, where nothing in Minecraft's world is in reach, and hold the
right button: Minecraft then raises the shield in its off hand, in the HUD, for as long as the button is held. Still
holding it, Alt-Tab to Claude Code on the second monitor, let go of the button there, wait five seconds, and come back
to the game. Expected:
- the log's `the game lost the front in build mode: Minecraft lets go of everything`;
- back in the game, the HUD's shield lowered, and still `build=on`;
- a right click on the street places a block, as before.

If the shield is still up, Minecraft still holds the button: stop, and see the top of this task.

- [x] **Step 10: In the game: Unlink, and Minecraft going**

Send `mc wipe`, which clears the layer at V's feet, so the blocks of both spots go if the street kept the same block
height. Then, with build mode on, send `link off`. Expected: `build mode off: unlinked`, the HUD gone, and
`link=not linked`. Ask the user to press F8: it links again, and build mode goes on, with
`request=build on: the mouse buttons, the wheel and 1 to 9 are Minecraft's`. Then ask the user to stop Minecraft from
the Modrinth App. Expected within 3 seconds: `link: Minecraft has gone` and `build mode off: Minecraft has gone`, and
the HUD gone. Ask the user to press F8 once more: `build=off: Minecraft isn't there`.

- [x] **Step 11: In the game: clean up**

Send `link off` and `mode off`, each once the last shows in the status. Expected: `mode=off`, `link=not linked`, and no
error lines in BlockRender's log for the session. Then ask the user to quit the game. BlockRender 0.10.0 stays
installed: as before, nothing shows until the user links or picks a drawing mode, and the patch passes every input
until F8.

- [x] **Step 12: Record the results, and commit**

Add step 3 to `docs/spikes/2026-10-02-minecraft-link-results.md`, as steps 1 and 2 are there:
- the build, and any change to the game's settings;
- each check above, with what the user saw and the status's counts: each input, Minecraft's and not the game's;
  building at both spots; each menu key; Alt-Tab with the button held; Unlink and Minecraft going;
- click latency, both halves, beside step 2's for commands;
- anything that surprised, and what it means for what comes next;
- the verdict for step 3.

Then commit:
```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes \
  && git commit -q -m "minecraft-link: step 3's results" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 40: Close Part 3, and the spike

- [x] **Step 1: Add "How Part 3 went" to this plan**

At the end: what ran as written, what changed, and what step 3 found.

- [x] **Step 2: Record the spike's verdict**

The spec's step 4. At the top of `docs/spikes/2026-10-02-minecraft-link-results.md`, the answer to the spike's
question (the spec's "Purpose"), from steps 1 to 3: whether Night City shows Minecraft's live world a frame or two
after Minecraft changes it, with the building done from inside Cyberpunk, and both games at acceptable frame rates.
Then what the spike leaves for the project: puppeting, Night City's collision, and what step 3 found.

- [x] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs \
  && git commit -q -m "minecraft-link: Part 3 closed, and the spike's verdict" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

- [ ] **Step 4: Finish the branch**

Use superpowers:finishing-a-development-branch. The user has chosen a local fast-forward merge of the spike branch
into `main` each time so far; ask again.

## How Part 3 went

On 2026-10-03, inline, as the user asked.
- **Tasks 30 to 38 ran as written,** in the repo, by the runner that replayed them (`tools/plans/run_plan.py`,
  execute mode). Every build that had to fail failed with its expected message, and every count matched: 204 to 232
  C++ tests, with no warnings, and `luac` took the overlay. The 9 commits run from `46d8cab` to `129087d`, and their 23
  files are identical to the verified scratch copy's and to the fresh replay's, line endings aside.
- **Task 39 ran as written,** with the user: [step 3's results](../../spikes/2026-10-02-minecraft-link-results.md).
  Every check passed. It departed from the plan in three places:
  - **Step 9's shield:** the player had none by now, so a typed `/item replace` put one in its off hand first;
  - **Step 10's clearing:** one `/fill` over the built area instead of `mc wipe`, which clears only the layer at V's
    feet;
  - **after Step 11,** with the game closed, `mclink` settled what the user had reported along the way: clicks that
    Minecraft refused with "The height limit for building is 1023".
- **What step 3 found** (the results have the numbers):
  - **build mode works as designed:** every input reached Minecraft and none reached the game, and F8, the menu keys,
    Alt-Tab, unlinking and Minecraft going all did what the spec says. Placing a block takes Minecraft 41 ms and us 17
    ms, within a frame;
  - **Minecraft's server refuses a click while a teleport is unconfirmed,** with a height message that misleads.
    SkyCraft teleports its player after V up to 10 times a second, so some clicks fail while V moves. The project's
    own Minecraft side should hold a click until the teleport is confirmed, or not teleport at all (puppeting);
  - **a break that empties a section isn't timed,** since such a section is never drawn again;
  - **the floor follows V up and down,** sent again for each metre of height, so a click in mid-jump places a block in
    the air;
  - **two sections stayed unexplained:** BlockRender can't list the sections it draws, and a request that does would
    have named them.
