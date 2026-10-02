# Minecraft Link Implementation Plan, Part 1 (step 1)

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
- Minecraft 26.3, Fabric Loader 0.19.5, Fabric API 0.161.0+26.3 and `skycraft-fabric-0.1.2.jar`, in the user's
  Modrinth instance.

**Spec:** [docs/superpowers/specs/2026-10-02-minecraft-link-design.md](../specs/2026-10-02-minecraft-link-design.md)

**Parts 2 and 3:** planned from what step 1 finds, as spike 3 planned its parts.
- **Part 2** is step 2 in the game: the plugin's link, the renderer's changes, the HUD pass, the status and the overlay.
- **Part 3** is step 3: input.

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
    - 171 C++ tests passing;
    - the replayed files identical to the scratch clone's.
  - Before this plan was written, `mclink` also ran against a stand-in for SkyCraft's Java side. That covered the
    commands' T, chat, text and Enter handshake, the floor, sections, an explosion and a HUD image.

  So a failure while following the plan most likely means a transcription slip: compare against the plan's code first.
- **Test counts:** 112 at the start. Then 123, 130, 134, 138, 143, 151, 155, 167 and 171 after Tasks 2 to 10.
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

- [ ] **Step 1: Check the branch**

```bash
cd /d/Cyberpunk-Minecraft && git switch spike/minecraft-link && git status --short && git log --oneline -1
```
Expected: no changed files, and this plan's own commit at the top.

- [ ] **Step 2: Download the header and the licence**

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

- [ ] **Step 3: Write `extern/skycraft/README.md`**

```markdown
# SkyCraft's shared-memory protocol

`skycraft_protocol.h` and `LICENSE` (MIT) copied unmodified from https://github.com/chasmlol/SkyCraft at commit
`bfcaf178524b92c2cdeb88e4ce0f13ef9ded6f32` (SkyCraft 0.1.2): `protocol/skycraft_protocol.h` and `LICENSE`.

It's the layout of the mapping `Local\SkyCraft_v1`, protocol version 11, that SkyCraft's Fabric mod
(`skycraft-fabric-0.1.2.jar`) opens. BlockRender plays the game side, as SkyCraft's Skyrim plugin does: see
`docs/superpowers/specs/2026-10-02-minecraft-link-design.md`. The header's own static checks pin the layout.
```

- [ ] **Step 4: Edit `CMakeLists.txt` to add SkyCraft's include folder**

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

- [ ] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `112 test(s), 0 failure(s)`.

- [ ] **Step 6: Commit**

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

- [ ] **Step 1: Write the failing test, `tests/TestRings.cpp`**

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

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestGbuffer.cpp
)
```
```cmake
	tests/TestGbuffer.cpp
	tests/TestRings.cpp
)
```

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Rings.h'`.

- [ ] **Step 4: Write `src/link/Rings.h`**

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

- [ ] **Step 5: Write `src/link/Rings.cpp`**

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

- [ ] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/render/Gbuffer.cpp
)
```
```cmake
	src/render/Gbuffer.cpp
	src/link/Rings.cpp
)
```

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `123 test(s), 0 failure(s)`.

- [ ] **Step 8: Commit**

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

- [ ] **Step 1: Write the failing test, `tests/TestMessages.cpp`**

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

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestRings.cpp
)
```
```cmake
	tests/TestRings.cpp
	tests/TestMessages.cpp
)
```

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Messages.h'`.

- [ ] **Step 4: Write `src/link/Messages.h`**

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

- [ ] **Step 5: Write `src/link/Messages.cpp`**

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

- [ ] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Rings.cpp
)
```
```cmake
	src/link/Rings.cpp
	src/link/Messages.cpp
)
```

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `130 test(s), 0 failure(s)`.

- [ ] **Step 8: Commit**

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

- [ ] **Step 1: Write the failing test, `tests/TestOverlay.cpp`**

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

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestMessages.cpp
)
```
```cmake
	tests/TestMessages.cpp
	tests/TestOverlay.cpp
)
```

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Overlay.h'`.

- [ ] **Step 4: Write `src/link/Overlay.h`**

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

- [ ] **Step 5: Write `src/link/Overlay.cpp`**

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

- [ ] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Messages.cpp
)
```
```cmake
	src/link/Messages.cpp
	src/link/Overlay.cpp
)
```

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `134 test(s), 0 failure(s)`.

- [ ] **Step 8: Commit**

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

- [ ] **Step 1: Write the failing test, `tests/TestFloor.cpp`**

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

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestOverlay.cpp
)
```
```cmake
	tests/TestOverlay.cpp
	tests/TestFloor.cpp
)
```

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Floor.h'`.

- [ ] **Step 4: Write `src/link/Floor.h`**

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

- [ ] **Step 5: Write `src/link/Floor.cpp`**

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

- [ ] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Overlay.cpp
)
```
```cmake
	src/link/Overlay.cpp
	src/link/Floor.cpp
)
```

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `138 test(s), 0 failure(s)`.

- [ ] **Step 8: Commit**

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

- [ ] **Step 1: Write the failing test, `tests/TestLook.cpp`**

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

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestFloor.cpp
)
```
```cmake
	tests/TestFloor.cpp
	tests/TestLook.cpp
)
```

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Look.h'`.

- [ ] **Step 4: Write `src/link/Look.h`**

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

- [ ] **Step 5: Write `src/link/Look.cpp`**

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

- [ ] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Floor.cpp
)
```
```cmake
	src/link/Floor.cpp
	src/link/Look.cpp
)
```

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `143 test(s), 0 failure(s)`.

- [ ] **Step 8: Commit**

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

- [ ] **Step 1: Write the failing test, `tests/TestCommands.cpp`**

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

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestLook.cpp
)
```
```cmake
	tests/TestLook.cpp
	tests/TestCommands.cpp
)
```

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Commands.h'`.

- [ ] **Step 4: Write `src/link/Commands.h`**

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

- [ ] **Step 5: Write `src/link/Commands.cpp`**

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

- [ ] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Look.cpp
)
```
```cmake
	src/link/Look.cpp
	src/link/Commands.cpp
)
```

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `151 test(s), 0 failure(s)`.

- [ ] **Step 8: Commit**

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

- [ ] **Step 1: Write the failing test, `tests/TestMapping.cpp`**

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

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestCommands.cpp
)
```
```cmake
	tests/TestCommands.cpp
	tests/TestMapping.cpp
)
```

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Mapping.h'`.

- [ ] **Step 4: Write `src/link/Mapping.h`**

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

- [ ] **Step 5: Write `src/link/Mapping.cpp`**

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

- [ ] **Step 6: Edit `CMakeLists.txt` to add the source and `advapi32`**

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

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `155 test(s), 0 failure(s)`.

- [ ] **Step 8: Commit**

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

Writing it settled one detail the spec left loose: what a bad message does to the ring. The last step records it in the
spec's "Changes while planning", with the other corrections planning Part 1 found.

**Files:**
- Create: `src/link/Link.h`, `src/link/Link.cpp`, `tests/TestLink.cpp`
- Modify: `CMakeLists.txt`, `docs/superpowers/specs/2026-10-02-minecraft-link-design.md`

- [ ] **Step 1: Write the failing test, `tests/TestLink.cpp`**

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

	uint8_t* base = nullptr;
	uint32_t hudBack = 1;
};

struct Sink : LinkSink
{
	void Atlas(const AtlasImage& aAtlas) override { atlases.push_back({ aAtlas.width, aAtlas.height }); }
	void Section(SectionMesh&& aMesh) override { sections.push_back(std::move(aMesh)); }
	void ClearAll() override { ++clears; }
	void Explosion(const proto::McEvent& aEvent) override { explosions.push_back(aEvent); }
	void Hud(const HudFrame& aFrame) override { huds.push_back({ aFrame.width, aFrame.height }); }
	void Log(const std::string& aLine) override { log.push_back(aLine); }

	bool Logged(const std::string& aStart) const
	{
		for (const auto& line : log)
		{
			if (line.rfind(aStart, 0) == 0)
			{
				return true;
			}
		}
		return false;
	}

	std::vector<std::pair<uint32_t, uint32_t>> atlases;
	std::vector<SectionMesh>                   sections;
	int                                        clears = 0;
	std::vector<proto::McEvent>                explosions;
	std::vector<std::pair<uint32_t, uint32_t>> huds;
	std::vector<std::string>                   log;
};

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
	link.Update(At(1.0), kSettings, sink);  // Minecraft connects: the overlay's swap starts over
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

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestMapping.cpp
)
```
```cmake
	tests/TestMapping.cpp
	tests/TestLink.cpp
)
```

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Link.h'`.

- [ ] **Step 4: Write `src/link/Link.h`**

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

	bool Type(std::string_view aCommandOrName, std::string& aError);  // queues a command, or a named set
	bool Push(const proto::InputEvent& aEvent);                        // the input ring, from any thread; false if dropped
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
};
}
```

- [ ] **Step 5: Write `src/link/Link.cpp`**

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
		// the floor and a teleport, and restarts the overlay's swap, as SkyCraft's Game.cpp does.
		overlay_.Reset();
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
		aSink.Log("render ring: " + drained.why);
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

- [ ] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Mapping.cpp
)
```
```cmake
	src/link/Mapping.cpp
	src/link/Link.cpp
)
```

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `167 test(s), 0 failure(s)`.

- [ ] **Step 8: Edit `docs/superpowers/specs/2026-10-02-minecraft-link-design.md` for the changes while planning**

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
- **`mclink`'s input:** a script, one action a line (`tools/mclink/McLinkScript.h`), so step 1's runs can change
  without a rebuild.
```

- [ ] **Step 9: Commit**

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

- [ ] **Step 1: Write the failing test, `tests/TestMcLinkScript.cpp`**

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

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test and its include folder**

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

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'McLinkScript.h'`.

- [ ] **Step 4: Write `tools/mclink/McLinkScript.h`**

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

- [ ] **Step 5: Write `tools/mclink/McLinkScript.cpp`**

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

- [ ] **Step 6: Write `tools/mclink/Main.cpp`**

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

	// The section's blocks, as the extent of its vertices, and the atlas texel its first vertex's UV lands on.
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
			const Vertex&  v = aMesh.vertices[0];
			const uint32_t x = std::min(uint32_t(std::clamp(v.u, 0.0f, 1.0f) * atlasWidth), atlasWidth - 1);
			const uint32_t y = std::min(uint32_t(std::clamp(v.v, 0.0f, 1.0f) * atlasHeight), atlasHeight - 1);
			const uint8_t* texel = &atlas[(size_t(y) * atlasWidth + x) * 4];
			sample = std::format("; its first vertex's texel ({}, {}) is rgba({}, {}, {}, {})", x, y, texel[0], texel[1], texel[2], texel[3]);
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

- [ ] **Step 7: Write `tools/mclink/step1.txt`**

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
# the section again.
type /summon tnt ~2 ~1 ~
wait 6
# Unlinking holds Minecraft's player; linking again teleports it back.
link off
wait 2
link on
wait 2
status
quit
```

- [ ] **Step 8: Write `tools/mclink/again.txt`**

```text
# Step 1, again: a new game side. Minecraft must send everything again: its clear, its atlas, and its sections.
wait-world 30
link on
wait 3
status
quit
```

- [ ] **Step 9: Edit `CMakeLists.txt` to add the script parser to the tests, and mclink**

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

- [ ] **Step 10: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `171 test(s), 0 failure(s)`.

- [ ] **Step 11: Run mclink without Minecraft; it must start and stop cleanly**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && printf 'status\nquit\n' > build/selfcheck.txt \
  && ./build/RelWithDebInfo/mclink.exe build/selfcheck.txt > build/selfcheck.log; echo "exit: $?"; head -2 build/selfcheck.log
```
Expected: `exit: 0`, then lines like these (the process ID varies):
```
[   0.00] the mapping: created (process 12345); waiting for Minecraft
[   0.00] Minecraft: absent, feet 0.000 0.000 0.000, frame 0
```

- [ ] **Step 12: Commit**

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

- [ ] **Step 1: Put SkyCraft's jar in the instance**

```bash
M="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/mods" \
  && curl -fsSL -o "$M/skycraft-fabric-0.1.2.jar" https://github.com/chasmlol/SkyCraft/releases/download/v0.1.2/skycraft-fabric-0.1.2.jar \
  && sha256sum "$M/skycraft-fabric-0.1.2.jar" && wc -c < "$M/skycraft-fabric-0.1.2.jar"
```
Expected: `ea429c73dcec49d39b6a0bb9b17ad08a6b0290a39a96ccf6f6c07d581cf992d9`, then `247869`.

- [ ] **Step 2: Ask the user to finish the instance and start Minecraft**

In the Modrinth App, the user:
1. adds Fabric API 0.161.0+26.3 to the instance (its Content tab);
2. sets the instance's Java arguments to `--enable-native-access=ALL-UNNAMED -Dskycraft.quitWithSkyrim=false` (its
   settings, Java and memory);
3. starts the instance, and says when Minecraft reaches its title screen.

Its window shows until the link comes up, then SkyCraft hides it.

- [ ] **Step 3: Check that SkyCraft loaded**

```bash
L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log" \
  && grep -E "Loading [0-9]+ mods|- (skycraft|fabric-api) |SkyCraft:" "$L" | head -12
```
Expected:
- the mods loaded, with `skycraft 0.1.2` and `fabric-api 0.161.0+26.3` among them;
- `SkyCraft: can't open Skyrim's shared memory yet (Windows error 2: Skyrim hasn't created it yet)`: waiting for a
  game side.

- [ ] **Step 4: Run step 1's script**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/step1 && cd build/step1 \
  && ../RelWithDebInfo/mclink.exe ../../tools/mclink/step1.txt | tee step1.log; echo "exit: ${PIPESTATUS[0]}"
```
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
  - its first vertex's texel isn't transparent.
- **The click:** after it, `section -8 0 125` again, now also covering x -120 to -119, y 6 to 7 and z 2006 to 2007:
  the block placed on the floor where the crosshair met it.
- **The HUD image:** `HUD images: 1280 x 720, rows bottom-up`. It's saved to `hud.dds`, with `0 with a colour above
  their alpha`.
- **TNT:** it lands on the stone, then `explosion at` about -117.5 7.0 2007.5, radius 4.0, and `section -8 0 125`
  again, with the stone and the clicked block gone.
- **Unlinking and linking again:** the last status is in game, with at least 2 teleports.
- **Traffic:** the lines every 5 seconds show Minecraft's frames a second near 60 (paced to `mclink`), the render
  ring's MB a second, and HUD images a second.
- `exit: 0`.

- [ ] **Step 5: Look at the atlas and the HUD image**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && uv run --no-project -q --with numpy --with pillow python tools/view_dump.py build/step1
```
Expected: `atlas.dds` and `hud.dds` converted, both `R8G8B8A8_UNORM`. Read `build/step1/atlas.png` and
`build/step1/hud.png`, and check:
- **`atlas.png`:** Minecraft's block textures, with its item textures below them;
- **`hud.png`:** the right way up, with the hotbar (stone in the first slot, which is selected), the crosshair in
  white, and chat showing the commands' replies.

- [ ] **Step 6: Run it again, as a new game side**

Wait 10 seconds after Step 4. Minecraft drops the link after 8 s, and keeps running, since it was told not to quit
with the game side. Then:
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render/build/step1 \
  && ../RelWithDebInfo/mclink.exe ../../tools/mclink/again.txt | tee again.log; echo "exit: ${PIPESTATUS[0]}"
```
Expected:
- `Minecraft is there`, then `clear all`, `atlas: …`, and `section -8 0 125` again: Step 4's blocks are still in
  SkyCraft's world, and Minecraft sends everything again to a new game side;
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

- [ ] **Step 7: Ask the user to stop Minecraft**

Its window is hidden, so the user stops it from the Modrinth App.

- [ ] **Step 8: Write step 1's results, `docs/spikes/2026-10-02-minecraft-link-results.md`**

From `build/step1/step1.log`, `build/step1/again.log`, the two PNGs and Minecraft's log, as spike 3's results doc
does:
- the versions;
- each check above with its outcome and numbers: the stray, the atlas's size, Minecraft's frames a second, the render
  ring's MB a second and HUD images a second. Latency is step 2's, measured in the game;
- anything that surprised, and what it means for Part 2.

- [ ] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-02-minecraft-link-results.md \
  && git commit -q -m "minecraft-link: step 1's results" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 12: Close Part 1, and plan Part 2

- [ ] **Step 1: Add "How Part 1 went" to this plan**

Below this task: what ran as written, what changed, and what step 1 found.

- [ ] **Step 2: Plan Part 2**

Use superpowers:writing-plans to add Part 2's tasks, from Task 13 on, to this plan: step 2 in the game. Verify them as
Part 1's were. Changes to the spec found while planning go in its "Changes while planning".

- [ ] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/superpowers \
  && git commit -q -m "minecraft-link plan: Part 1 closed, Part 2 planned" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```
