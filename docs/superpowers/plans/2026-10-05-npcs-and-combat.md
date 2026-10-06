# NPCs and Combat Implementation Plan, Parts 1 and 2 (steps 1 to 4)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> In this project, plans run inline with superpowers:executing-plans, never through sub-agents.

**Goal:** Part 1 of spike 6, Night City's NPCs in Minecraft. BlockRender writes the NPCs around V into SkyCraft's actor
table every frame, so SkyCraft's mod gives each an invisible stand-in that Minecraft's weapons hit and that presses
Minecraft's pressure plates. Step 1 checks the table's side with `mclink` against the user's Minecraft; step 2 checks it
in the game, on a busy pavement.

**Architecture:** the records are plain C++ in `render_core`, tested without the game or Minecraft.
- **`link/Actors`:** NPCs numbered from 1, never the same number twice; the nearest 256 within 80 m made into
  SkyCraft's 64-byte records; and a hit's words for the logs.
- **`link/Rings` and `link/Link`:** the actor table written under its seqlock, and emptied once when the NPCs stop;
  `kEvHitActor` and `kEvPlayerDied` passed to the sink.
- **`physx/Characters`:** an NPC's box from its PhysX capsule, picked among the kinematic capsules around it, looked for
  when the NPC is first listed and again every 2 s until found, at most eight a frame; `PhysxGround::Capsules` reads
  them through the exporter's checks.
- **`Requests` and `Status`:** the `npcs` request and the status's NPC line.

Then what only the game can run:
- **`scripts/BlockRender.reds`:** the redscript module. It wraps `NPCPuppet.OnGameAttached` and `OnDetach` to keep
  every NPC the game attaches, and its `List` fills parallel arrays with those within 80 m of V. It compiles offline,
  against the game's own scripts, through `tools/check_reds.py`.
- **`GameScript`:** the module's calls from C++, each function and field checked by its types once, as `Puppet`'s are;
  `ScriptTypes` holds the checks both share.
- **`Plugin.cpp`:** BlockRender 0.17.0 lists the NPCs after the link's update, writes the table, looks for capsules,
  logs Minecraft's hits, and adds the module to redscript's compile at load.
- **The overlay:** an NPC line, an NPCs button, and a button that lists them in the log.

`mclink` gains made-up NPCs (`actor`, `actor-remove`) and prints Minecraft's hits on them, so step 1 runs against the
real Minecraft before anything goes into the game.

**Tech stack:**
- C++20, MSVC 14.44 (Visual Studio 2022), CMake 3.31 (bundled with Visual Studio);
- redscript as the game runs it, through the compiler library the game ships (`engine/tools/scc_lib.dll`), and Python
  3 with `uv` for the offline compile;
- SkyCraft's protocol (version 11) and its Fabric mod 0.1.2, unchanged, in the user's `CyberCraft` instance (Minecraft
  26.3);
- Cyberpunk 2077 2.31 with RED4ext 1.30.0 and the RED4ext SDK, header-only; CET for the overlay.

**Spec:** [docs/superpowers/specs/2026-10-05-npcs-and-combat-design.md](../specs/2026-10-05-npcs-and-combat-design.md)

**Part 2** (Tasks 13 to 37) makes Minecraft's hits on NPCs land as V's, and in build mode gives the game's hits on V to
Minecraft's player, whose death kills V; it was planned after step 2, and its own section below says how it's built. A
deep review of it, after Task 22 had run and before step 4, added Tasks 23 to 30, which send Minecraft's hits through
the game's whole damage pipeline as V's own punches go, made with what V's unarmed slot draws, and draw that weapon and
hide V in build mode; "The deep review of Part 2", at the end, says why. Step 4 is Task 31. It passed with three fixes
to make, which Tasks 32 to 35 make and Task 36 checks in the game: hits start at V's punch × the square root of
Minecraft's damage, V's Immortal is the game's override, and hiding V hides V's effects and the game's crosshair too;
"Step 4's fixes", at the end, says why.
**Part 3** is planned after step 4's fixes, as the spec says: Task 37 closes Part 2 and, at the user's choice, writes a
handoff for planning Part 3 in a new session (`docs/superpowers/handoffs/2026-10-06-spike-6-part-3-planning.md`).

---

## Before you start

- **The branch:** work on `spike/npcs-and-combat`, which holds the spec and this plan.
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
  - Tasks 1 to 3 and 5 to 10 were then replayed in a fresh clone by a script that applied every file step exactly as
    written, and checked every build, test, offline compile, `mclink` and overlay step's expected result. The
    results:
    - zero compiler warnings at `/W4`;
    - 414 C++ tests passing;
    - the module compiling against the game's own scripts, and a slip in it failing that compile;
    - the replayed files identical to the scratch clone's.
  - Each new test was broken once, by a mutation of the code it covers, and failed as it should: 39 mutations.
  - The overlay was run under Lua 5.4 with CET's functions stubbed, against sample NPC lines (on with NPCs listed, on
    with none and why, and off): it showed the NPC line and the NPCs button as Task 10 says.
  - `GameScript`'s calls follow `Puppet`'s, and every address the SDK resolves on their path that `Puppet`'s doesn't
    (`CClass::GetProperty` and `CClass::InitializeProperties`) was found in the game's `cyberpunk2077_addresses.json`.
    Only the game can run them: Task 11's first check is that the module and its functions and fields are found.

  So a failure while following the plan most likely means a transcription slip: compare against the plan's code first.
- **Part 2, verified the same way:** Tasks 13 to 15 and 17 to 22 were written in a scratch clone, then replayed in a
  fresh clone from this plan's text, every build, test count, offline compile, slip check and overlay step checked:
  zero warnings, 435 tests, and the files identical to the scratch clone's. Each new test was broken once: 34
  mutations, all caught, one by a crash (the inbox's many-threads test without its lock). The module, its wraps
  included, compiles against the game's own scripts, and three slips each fail it, a wrap of a method that isn't there
  among them. The overlay ran under Lua with CET's functions stubbed, with combat off, on, and Minecraft's player dead.
- **Part 2's deep review, verified the same way:** Tasks 23 to 30 were written in a scratch clone on top of Task 22's
  commit, then replayed in a fresh clone from this plan's text: zero warnings, 439 tests, the files identical to the
  scratch clone's. Each new or changed test was broken once: 22 mutations, all caught, one by a crash (the inbox's
  endings without their lock). The module compiles against the game's own scripts both without and with Codeware's,
  and two slips fail it, one of them only with Codeware's. The overlay ran under Lua with CET's functions stubbed, with
  V's weapon drawn and V hidden, not drawn with `hidev` 0, and combat off.
- **Step 4's fixes, verified the same way:** Tasks 32 to 35 were written in the scratch clone on top of Task 30's
  commit, then replayed in a fresh clone from this plan's text: zero warnings, 440 tests, the files identical to the
  scratch clone's. Each new or changed test was broken once: 9 mutations, all caught. The module compiles against the
  game's own scripts both without and with Codeware's, and a slip in its Codeware parts fails only with Codeware's.
- **Test counts:** 396 at the start. Then 403, 406 and 408 after Tasks 1 to 3, 412 after Task 5, and 414 after Task 8.
  Tasks 6, 7, 9 and 10 add none.
- **What only Minecraft and the game can test:** the actor table against SkyCraft's real mod in Task 4, and the module
  and the NPCs in the game in Task 11; in Part 2, the hurts against SkyCraft's mod in Task 16, and combat in the game in
  Tasks 31 and 36. All with the user.
- **The user:**
  - **does everything in Minecraft, the Modrinth App and the game:** starting and stopping them, loading, moving,
    keys, clicks and screenshots. Never drive their mouse or keyboard: ask, then wait for them to say they're done;
  - **approves downloads first.** Nothing in Parts 1 and 2 downloads anything.
- **The game:** builds deploy only with `CP2077_DIR` set, which only the deploy steps set (Tasks 11, 31 and 36), with the
  game closed, and clear straight after. `mclink` needs the game closed too, since the game would hold the mapping.
- **redscript:** a script that doesn't compile at the game's start shows a message box, and that run the mods' scripts
  don't load, the other mods' included. Task 6's offline compile runs the game's own compiler library on the module,
  with its message box off, before anything is deployed. Don't run the game's `engine/tools/scc.exe` by hand: on a
  failed compile it shows that message box on the user's screen.
- **No faults:** the game's crash handler sees a fault before any `__try`, and freezes the game. `GameScript` calls only
  functions whose parameter and return types it has checked, reads only fields whose types it has checked, and checks
  every handle before using it; the module checks every object with `IsDefined`.
- **Commits** go on `spike/npcs-and-combat`, and end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## File map (Part 1)

All paths under `src/`, `tests/`, `tools/` and `scripts/` are relative to `spikes/block-render/`.

| File | Responsibility |
|---|---|
| `CMakeLists.txt` | The new sources and tests, `GameScript` in the plugin, the module installed, and version 0.17.0 |
| `src/link/Actors.*` | NPCs' numbers, the nearest 256 as SkyCraft's records, a hit's words |
| `src/link/Rings.*`, `src/link/Link.*` | The actor table written and emptied; `kEvHitActor` and `kEvPlayerDied` to the sink |
| `src/physx/Characters.*` | An NPC's capsule picked, its box, and which NPCs to look for |
| `src/physx/Export.*` | `PhysxGround::Capsules`: the kinematic capsules around a point |
| `scripts/BlockRender.reds` | The redscript module: the NPC list, V's punch and max health, and the targeting comparison |
| `tools/check_reds.py` | The module compiled offline against the game's own scripts |
| `src/ScriptTypes.*` | The type checks `Puppet` and `GameScript` share, moved out of `Puppet.cpp` |
| `src/GameScript.*` | The module's calls and fields from C++ |
| `src/Requests.*`, `src/Status.*` | The `npcs` request; the status's NPC line |
| `src/Plugin.cpp` | 0.17.0: the NPCs listed each frame, `set npcs`, `npcs`, hits logged, the module added at load |
| `tools/mclink/*` | `actor` and `actor-remove`, hits and deaths printed, and step 1's script `npcs.txt` |
| `tools/blockrender_hud/init.lua` | The NPC line, the NPCs button, and the list button |
| `tests/TestActors.cpp`, `tests/TestCharacters.cpp` | One test file per new unit; the others gain tests |

---

### Task 1: NPCs numbered, and made into SkyCraft's actor records

SkyCraft's mod reads the actor table each server tick and gives each record a stand-in (the spec's "SkyCraft's
Minecraft side"). This task makes the records, from NPCs as the module will list them (the spec's "Records"):
- **numbers:** `ActorIds` numbers NPCs from 1 as they're first listed, keyed by their 64-bit `EntityID`, and never gives
  a number twice. An NPC unlisted for a minute is forgotten, and gets a new number if it comes back. A hit's number maps
  back to its NPC for 2 s after it was last listed, since a hit can trail its stand-in's removal;
- **which:** the nearest 256 within 80 m of V, straight-line, numbered nearest first, the entity breaking ties;
- **a record:** the feet in Minecraft's axes (`McFromWorld`), lowered by the box's reach below the NPC's position; the
  yaw from the forward vector (`LookFromForward`); the box's size, a person's 0.6 by 1.8 until Task 5 finds the NPC's
  capsule; the health over its max, 0 when the max isn't over 0; the level, rounded and held to 0 to 65,535; the name
  cut to 23 bytes at the start of a UTF-8 character; and only the hostile, essential and in-combat flags.

The yaw is taken against 0 rather than the NPC's last yaw, so no per-NPC yaw is kept. `LookFromForward` uses the last
yaw for two things: to keep a turning yaw from wrapping between 180 and -180, which mattered for Minecraft's hand sway
as V turns but not for an invisible stand-in; and as the yaw of a forward vector pointing straight up or down, which an
NPC's never does. An NPC's yaw stays between -180 and 180.

**Files:**
- Create: `src/link/Actors.h`, `src/link/Actors.cpp`, `tests/TestActors.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestActors.cpp`**

```cpp
#include "Harness.h"

#include "link/Actors.h"

#include <cstring>
#include <limits>

using namespace cpmc;
using namespace cpmc::link;

namespace
{
// An NPC on the road at spike 3's corner, facing north (the world's +y), as the module would list it.
NpcSample Npc(uint64_t aEntity, double aEast = 0.0, double aNorth = 0.0)
{
	NpcSample npc;
	npc.entity = aEntity;
	npc.position = { -119.5 + aEast, -2007.5 + aNorth, 6.14 };
	npc.forward = { 0.0f, 1.0f, 0.0f };
	npc.health = 50.0f;
	npc.healthMax = 200.0f;
	npc.level = 12.6f;
	npc.name = "Tyger Claws Ganger";
	npc.flags = proto::kActorHostile | proto::kActorInCombat;
	return npc;
}

const Vec3d kV{ -119.5, -2007.5, 6.14 };  // V, where the NPCs are measured from
}

TEST(ActorsNumberNpcsFromOneAndNeverTheSameNumberTwice)
{
	ActorIds ids;
	CHECK_EQ(ids.Id(0xA, 0.0), 1u);
	CHECK_EQ(ids.Id(0xB, 0.0), 2u);
	CHECK_EQ(ids.Id(0xA, 30.0), 1u);  // kept while it's listed
	CHECK_EQ(ids.Known(), size_t(2));
	ids.Forget(61.0);  // B, unlisted since 0 s, goes; A, listed at 30 s, stays
	CHECK_EQ(ids.Known(), size_t(1));
	CHECK_EQ(ids.Id(0xB, 61.0), 3u);  // a new number: 2 is never given again
	CHECK_EQ(ids.Given(), 3u);
	CHECK_EQ(ids.Id(0xA, 61.0), 1u);
}

TEST(ActorsMapAHitBackToItsNpcForTwoSeconds)
{
	ActorIds ids;
	CHECK_EQ(ids.Id(0xA, 10.0), 1u);
	CHECK(ids.Entity(1, 11.9) == std::optional<uint64_t>(0xA));  // a hit can trail its stand-in's removal
	CHECK(!ids.Entity(1, 12.1).has_value());                     // but not by long
	CHECK(!ids.Entity(2, 10.0).has_value());                     // never given
	ids.Forget(70.1);
	CHECK(!ids.Entity(1, 10.5).has_value());  // forgotten
}

TEST(ActorsCutNamesAtTheStartOfACharacter)
{
	CHECK_STR(CutName("Tyger Claws Ganger", 23), "Tyger Claws Ganger");
	CHECK_STR(CutName("Maelstrom Cyberpsycho Lieutenant", 23), "Maelstrom Cyberpsycho L");
	// "é" is two bytes, C3 A9: one that would end on its first byte is left out whole.
	CHECK_STR(CutName("Ripperdoc Viktor Vektor\xC3\xA9", 24), "Ripperdoc Viktor Vektor");
	CHECK_STR(CutName("\xE2\x82\xAC\xE2\x82\xAC", 4), "\xE2\x82\xAC");  // two euro signs, three bytes each
	CHECK_STR(CutName("", 23), "");
}

TEST(ActorsMakeARecordFromAnNpc)
{
	const proto::ActorRecord record = MakeActorRecord(Npc(0xA), 7, kPersonBox);
	CHECK_EQ(record.formId, 7u);
	CHECK_EQ(record.flags, uint32_t(proto::kActorHostile | proto::kActorInCombat));
	CHECK_NEAR(record.x, -119.5, 1e-4);  // Minecraft's axes: x east, y up, z south
	CHECK_NEAR(record.y, 6.14, 1e-4);
	CHECK_NEAR(record.z, 2007.5, 1e-3);
	CHECK_NEAR(record.yaw, 180.0, 1e-3);  // facing north
	CHECK_NEAR(record.width, 0.6, 1e-6);
	CHECK_NEAR(record.height, 1.8, 1e-6);
	CHECK_NEAR(record.healthFrac, 0.25, 1e-6);
	CHECK_EQ(record.level, 13);
	CHECK_STR(record.name, "Tyger Claws Ganger");
	CHECK_EQ(record.name[23], '\0');
}

TEST(ActorsTakeAnNpcsBoxFromItsCapsule)
{
	// A controller's capsule doesn't reach the feet: the box goes from the position up to the capsule's top.
	const proto::ActorRecord person = MakeActorRecord(Npc(0xA), 1, NpcBox{ 0.64, 0.0, 1.72 });
	CHECK_NEAR(person.y, 6.14, 1e-4);
	CHECK_NEAR(person.width, 0.64, 1e-6);
	CHECK_NEAR(person.height, 1.72, 1e-6);
	// A drone's capsule can reach below its position: the box starts there.
	const proto::ActorRecord drone = MakeActorRecord(Npc(0xB), 2, NpcBox{ 1.0, 0.3, 0.4 });
	CHECK_NEAR(drone.y, 5.84, 1e-4);
	CHECK_NEAR(drone.height, 0.7, 1e-6);
}

TEST(ActorsHoldOddNumbersToTheRecordsRange)
{
	NpcSample npc = Npc(0xA);
	npc.healthMax = 0.0f;  // no health pool
	npc.level = 70000.0f;
	npc.flags = proto::kActorDead | proto::kActorEssential;  // the dead aren't listed, and never flagged so
	npc.forward = { 0.0f, 0.0f, 0.0f };
	proto::ActorRecord record = MakeActorRecord(npc, 1, kPersonBox);
	CHECK_EQ(record.healthFrac, 0.0f);
	CHECK_EQ(record.level, 65535);
	CHECK_EQ(record.flags, uint32_t(proto::kActorEssential));
	CHECK_EQ(record.yaw, 0.0f);
	npc.health = 300.0f;
	npc.healthMax = 200.0f;
	npc.level = std::numeric_limits<float>::quiet_NaN();
	record = MakeActorRecord(npc, 1, kPersonBox);
	CHECK_EQ(record.healthFrac, 1.0f);
	CHECK_EQ(record.level, 0);
	npc.health = std::numeric_limits<float>::infinity();
	npc.level = -4.0f;
	record = MakeActorRecord(npc, 1, kPersonBox);
	CHECK_EQ(record.healthFrac, 0.0f);
	CHECK_EQ(record.level, 0);
}

TEST(ActorsKeepTheNearest256Within80Metres)
{
	std::vector<NpcSample> npcs;
	for (uint64_t i = 1; i <= 300; ++i)
	{
		npcs.push_back(Npc(i, 0.25 * double(301 - i)));  // 300 NPCs 0.25 to 75 m east: the last listed is the nearest
	}
	npcs.push_back(Npc(1000, 80.0));   // at 80 m: in
	npcs.push_back(Npc(1001, 0.0, 80.01));  // past 80 m: out
	ActorIds                         ids;
	std::unordered_map<uint64_t, NpcBox> boxes{ { 300, NpcBox{ 0.5, 0.0, 1.5 } } };
	const std::vector<proto::ActorRecord> records = ActorRecords(npcs, kV, boxes, ids, 5.0);
	CHECK_EQ(records.size(), size_t(256));
	CHECK_EQ(records.front().formId, 1u);  // the nearest is numbered first
	CHECK_NEAR(records.front().x, -119.25, 1e-4);
	CHECK_NEAR(records.front().width, 0.5, 1e-6);  // its box, by entity
	CHECK_NEAR(records.back().x, -119.5 + 0.25 * 256, 1e-3);
	CHECK_NEAR(records[1].width, 0.6, 1e-6);  // the rest person-sized
	CHECK_EQ(ids.Given(), 256u);  // only those in the table are numbered
	npcs.resize(2);
	npcs.push_back(Npc(1000, 80.0));
	npcs.push_back(Npc(1001, 0.0, 80.01));
	const std::vector<proto::ActorRecord> few = ActorRecords(npcs, kV, {}, ids, 6.0);
	CHECK_EQ(few.size(), size_t(3));
	CHECK_NEAR(few.back().x, -39.5, 1e-4);  // the NPC at exactly 80 m
	CHECK_EQ(ids.Given(), 259u);            // three NPCs the first table had no room for
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestDumpObj.cpp
	tools/mclink/DumpObj.cpp
```
```cmake
	tests/TestDumpObj.cpp
	tests/TestActors.cpp
	tools/mclink/DumpObj.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Actors.h'`.

- [x] **Step 4: Write `src/link/Actors.h`**

```cpp
#pragma once

#include "Coords.h"
#include "link/Rings.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Night City's NPCs as SkyCraft's actor records (spike 6's "Part 1"): BlockRender numbers the NPCs the redscript module
// lists, and writes the nearest into SkyCraft's actor table, where SkyCraft's mod gives each an invisible stand-in that
// Minecraft's weapons hit and that presses pressure plates. Pure: the tests give it NPCs of their own.
namespace cpmc::link
{
// One NPC as the module lists it, in the game's world axes (metres, z up).
struct NpcSample
{
	uint64_t    entity = 0;  // its EntityID, all 64 bits: EntityID.GetHash gives only 32
	Vec3d       position;    // GetWorldPosition: its feet, as V's is
	Vec3f       forward;     // GetWorldForward
	float       health = 0.0f;  // its health pool, now and at most
	float       healthMax = 0.0f;
	float       level = 0.0f;  // its PowerLevel
	std::string name;          // GetDisplayName, UTF-8
	uint32_t    flags = 0;     // proto::ActorFlags: hostile, essential, in combat
};

// An NPC's box, from its capsule (physx/Characters): its width, and how far it reaches below and above the NPC's
// position, which moves with it.
struct NpcBox
{
	double width = 0.6;
	double below = 0.0;
	double above = 1.8;
};

// The box of an NPC without a capsule: the stand-in's own default size, 0.6 by 1.8 (SkyCraft's sized(0.6F, 1.8F)).
constexpr NpcBox kPersonBox{};

constexpr double kActorRange = 80.0;   // metres, straight-line: SkyCraft's 80 blocks
constexpr double kActorForget = 60.0;  // seconds unlisted before an NPC's number is forgotten
constexpr double kHitTrail = 2.0;      // seconds a hit may trail its stand-in's removal

// Numbers NPCs from 1 as each is first listed, keyed by entity, and never gives the same number twice in a run.
class ActorIds
{
public:
	// aEntity's number, given now if it has none; aNow (seconds) marks it listed.
	uint32_t Id(uint64_t aEntity, double aNow);
	// The entity behind aId, if it was listed within the last kHitTrail seconds.
	std::optional<uint64_t> Entity(uint32_t aId, double aNow) const;
	// Forgets the NPCs unlisted for kActorForget seconds: listed again, they get new numbers.
	void Forget(double aNow);

	size_t   Known() const { return byEntity_.size(); }
	uint32_t Given() const { return next_ - 1; }  // numbers given since the start

private:
	struct Entry
	{
		uint32_t id = 0;
		double   listed = 0.0;
	};

	std::unordered_map<uint64_t, Entry>    byEntity_;
	std::unordered_map<uint32_t, uint64_t> byId_;
	uint32_t                               next_ = 1;
};

// aName cut to at most aBytes bytes, at the start of a UTF-8 character.
std::string CutName(std::string_view aName, size_t aBytes);

// An NPC's record: its feet in Minecraft's axes, aBox's reach below its position under it; its yaw from its forward
// vector; aBox's size; its health over its max, or 0 when the max isn't over 0; its level, rounded and held to 0 to
// 65,535; its name cut to 23 bytes; its hostile, essential and in-combat flags.
proto::ActorRecord MakeActorRecord(const NpcSample& aNpc, uint32_t aId, const NpcBox& aBox);

// The records of the nearest proto::kMaxActors NPCs within kActorRange of aV (world axes), nearest first, numbered by
// aIds. aBoxes holds the boxes found so far, by entity; the other NPCs are person-sized.
std::vector<proto::ActorRecord> ActorRecords(std::span<const NpcSample> aNpcs, const Vec3d& aV,
                                             const std::unordered_map<uint64_t, NpcBox>& aBoxes, ActorIds& aIds, double aNow);
}
```

- [x] **Step 5: Write `src/link/Actors.cpp`**

```cpp
#include "link/Actors.h"

#include "link/Look.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace cpmc::link
{
uint32_t ActorIds::Id(uint64_t aEntity, double aNow)
{
	const auto found = byEntity_.find(aEntity);
	if (found != byEntity_.end())
	{
		found->second.listed = aNow;
		return found->second.id;
	}
	const uint32_t id = next_++;
	byEntity_.emplace(aEntity, Entry{ id, aNow });
	byId_.emplace(id, aEntity);
	return id;
}

std::optional<uint64_t> ActorIds::Entity(uint32_t aId, double aNow) const
{
	const auto entity = byId_.find(aId);
	if (entity == byId_.end())
	{
		return std::nullopt;
	}
	const auto entry = byEntity_.find(entity->second);
	if (entry == byEntity_.end() || aNow - entry->second.listed > kHitTrail)
	{
		return std::nullopt;
	}
	return entity->second;
}

void ActorIds::Forget(double aNow)
{
	for (auto entry = byEntity_.begin(); entry != byEntity_.end();)
	{
		if (aNow - entry->second.listed >= kActorForget)
		{
			byId_.erase(entry->second.id);
			entry = byEntity_.erase(entry);
		}
		else
		{
			++entry;
		}
	}
}

std::string CutName(std::string_view aName, size_t aBytes)
{
	if (aName.size() <= aBytes)
	{
		return std::string(aName);
	}
	size_t cut = aBytes;
	while (cut > 0 && (uint8_t(aName[cut]) & 0xC0) == 0x80)
	{
		--cut;  // aName[cut] continues a character that starts before it: that character is left out whole
	}
	return std::string(aName.substr(0, cut));
}

proto::ActorRecord MakeActorRecord(const NpcSample& aNpc, uint32_t aId, const NpcBox& aBox)
{
	proto::ActorRecord record{};
	record.formId = aId;
	record.flags = aNpc.flags & (proto::kActorHostile | proto::kActorEssential | proto::kActorInCombat);
	const Vec3d feet = McFromWorld({ aNpc.position.x, aNpc.position.y, aNpc.position.z - aBox.below });
	record.x = float(feet.x);
	record.y = float(feet.y);
	record.z = float(feet.z);
	const std::optional<McLook> look = LookFromForward(aNpc.forward, 0.0f);
	record.yaw = look ? look->yaw : 0.0f;
	record.width = float(aBox.width);
	record.height = float(aBox.below + aBox.above);
	record.healthFrac = aNpc.healthMax > 0.0f && std::isfinite(aNpc.health) && std::isfinite(aNpc.healthMax)
	                        ? std::clamp(aNpc.health / aNpc.healthMax, 0.0f, 1.0f)
	                        : 0.0f;
	record.level = std::isfinite(aNpc.level) ? uint16_t(std::clamp(std::round(aNpc.level), 0.0f, 65535.0f)) : uint16_t(0);
	const std::string name = CutName(aNpc.name, sizeof record.name - 1);
	std::memcpy(record.name, name.data(), name.size());  // the rest stays 0: the NUL
	return record;
}

std::vector<proto::ActorRecord> ActorRecords(std::span<const NpcSample> aNpcs, const Vec3d& aV,
                                             const std::unordered_map<uint64_t, NpcBox>& aBoxes, ActorIds& aIds, double aNow)
{
	struct Near
	{
		double           distance = 0.0;
		const NpcSample* npc = nullptr;
	};
	std::vector<Near> near;
	for (const NpcSample& npc : aNpcs)
	{
		const double dx = npc.position.x - aV.x, dy = npc.position.y - aV.y, dz = npc.position.z - aV.z;
		const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
		if (distance <= kActorRange)
		{
			near.push_back({ distance, &npc });
		}
	}
	// Nearest first; the entity breaks a tie, so the same NPCs number the same way every run.
	std::sort(near.begin(), near.end(), [](const Near& aA, const Near& aB) {
		return aA.distance != aB.distance ? aA.distance < aB.distance : aA.npc->entity < aB.npc->entity;
	});
	near.resize(std::min<size_t>(near.size(), proto::kMaxActors));
	std::vector<proto::ActorRecord> records;
	records.reserve(near.size());
	for (const Near& entry : near)
	{
		const auto box = aBoxes.find(entry.npc->entity);
		records.push_back(MakeActorRecord(*entry.npc, aIds.Id(entry.npc->entity, aNow), box != aBoxes.end() ? box->second : kPersonBox));
	}
	return records;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Solids.cpp
	src/physx/GameAbi.cpp
```
```cmake
	src/link/Solids.cpp
	src/link/Actors.cpp
	src/physx/GameAbi.cpp
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `403 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: NPCs numbered and made into SkyCraft's actor records" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 2: The link writes the actor table, and passes on hits and Minecraft's death

The table is written as SkyCraft's own game side writes it (`Link::WriteActors`, the spec's "The table"): the
sequence made odd, a release fence, the count and the records, then the sequence made even with release, so SkyCraft's
seqlock reader never takes a half-written table. At most 256 records go in.
- **`Link::WriteActors`** writes it every frame the plugin has NPCs; **`ClearActors`** writes it empty once when they
  stop, and nothing more until they're written again, so a table left behind can't keep stand-ins alive.
- **Events:** `kEvHitActor` and `kEvPlayerDied` go to the sink, as `kEvExplosion` does, and are counted in the status
  (`hitEvents`, `deathEvents`); `kEvArrowStuck` and `kEvSkillUse` are still only counted (the spec's "The protocol").

The tests' Minecraft gains a general `Event`, which `Explode` now uses, and a reader of the actor table.

**Files:**
- Modify: `src/link/Rings.h`, `src/link/Rings.cpp`, `src/link/Link.h`, `src/link/Link.cpp`, `tests/TestRings.cpp`,
  `tests/TestLink.cpp`

- [x] **Step 1: Edit `tests/TestRings.cpp`: the failing test**

```cpp
	CHECK_EQ(written.teleportSeq, 7u);
```
```cpp
	CHECK_EQ(written.teleportSeq, 7u);
}

TEST(RingsActorTableCountsUpByTwoAndHoldsAtMost256)
{
	Memory memory(proto::kOffActorTable + sizeof(proto::ActorTable) + 64);
	std::memset(memory.Bytes(), 0xCD, memory.words.size() * 8);
	std::memset(memory.Bytes() + proto::kOffActorTable, 0, sizeof(proto::ActorTable));
	std::vector<proto::ActorRecord> records(2);
	records[0].formId = 1;
	records[0].x = -119.5f;
	records[1].formId = 2;
	records[1].name[0] = 'A';
	WriteActorTable(memory.Bytes(), records);
	proto::ActorTable table;
	std::memcpy(&table, memory.Bytes() + proto::kOffActorTable, sizeof table);
	CHECK_EQ(table.seq, 2u);
	CHECK_EQ(table.count, 2u);
	CHECK(std::memcmp(table.actors, records.data(), 2 * sizeof(proto::ActorRecord)) == 0);
	WriteActorTable(memory.Bytes(), {});  // empty: SkyCraft reads only the count's records
	std::memcpy(&table, memory.Bytes() + proto::kOffActorTable, sizeof table);
	CHECK_EQ(table.seq, 4u);
	CHECK_EQ(table.count, 0u);
	records.resize(300);
	records[299].formId = 300;
	WriteActorTable(memory.Bytes(), records);
	std::memcpy(&table, memory.Bytes() + proto::kOffActorTable, sizeof table);
	CHECK_EQ(table.seq, 6u);
	CHECK_EQ(table.count, proto::kMaxActors);
	CHECK_EQ(memory.Bytes()[proto::kOffActorTable + sizeof(proto::ActorTable)], 0xCD);  // nothing past the table
	CHECK_EQ(memory.Bytes()[proto::kOffActorTable - 1], 0xCD);                          // nor before it
```

- [x] **Step 2: Edit `tests/TestLink.cpp`: the failing tests**

```cpp
	void Explode(float aX, float aY, float aZ, float aRadius)
	{
		auto*          ring = base + proto::kOffEventRing;
		uint64_t&      head = *reinterpret_cast<uint64_t*>(ring);
		proto::McEvent event{ proto::kEvExplosion, 0, aX, aY, aZ, aRadius, 0, 0 };
		std::memcpy(ring + 0x80 + (head % proto::kEventRingEntries) * sizeof event, &event, sizeof event);
		++head;
```
```cpp
	void Event(const proto::McEvent& aEvent)
	{
		auto*     ring = base + proto::kOffEventRing;
		uint64_t& head = *reinterpret_cast<uint64_t*>(ring);
		std::memcpy(ring + 0x80 + (head % proto::kEventRingEntries) * sizeof aEvent, &aEvent, sizeof aEvent);
		++head;
	}

	void Explode(float aX, float aY, float aZ, float aRadius) { Event({ proto::kEvExplosion, 0, aX, aY, aZ, aRadius, 0, 0 }); }

	proto::ActorTable Actors()
	{
		proto::ActorTable table;
		std::memcpy(&table, base + proto::kOffActorTable, sizeof table);
		return table;
```
```cpp
	void Explosion(const proto::McEvent& aEvent) override { explosions.push_back(aEvent); }
```
```cpp
	void Explosion(const proto::McEvent& aEvent) override { explosions.push_back(aEvent); }
	void HitActor(const proto::McEvent& aEvent) override { hits.push_back(aEvent); }
	void PlayerDied(const proto::McEvent& aEvent) override { deaths.push_back(aEvent); }
```
```cpp
	std::vector<proto::McEvent>                explosions;
```
```cpp
	std::vector<proto::McEvent>                explosions;
	std::vector<proto::McEvent>                hits;
	std::vector<proto::McEvent>                deaths;
```
```cpp
	CHECK_EQ(link.State().hudImages, 1ull);
```
```cpp
	CHECK_EQ(link.State().hudImages, 1ull);
}

TEST(LinkPassesOnHitsOnNpcsAndMinecraftsDeath)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Beat(77, 1000);
	// A critical sprint hit with a diamond sword, as SkyCombat sends a tick's hits; an arrow stuck; then the player dies.
	minecraft.Event({ proto::kEvHitActor, 3, 10.5f, 0.0f, -1.0f, 0.5f, proto::kHitCritical, proto::kWeaponBlade });
	minecraft.Event({ proto::kEvArrowStuck, 3, 1.0f, 2.0f, 3.0f, 90.0f, 0, 0 });
	minecraft.Event({ proto::kEvPlayerDied, 3, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0 });
	link.Update(At(1.0), kSettings, sink);
	CHECK_EQ(sink.hits.size(), size_t(1));
	CHECK_EQ(sink.deaths.size(), size_t(1));
	if (sink.hits.size() == 1 && sink.deaths.size() == 1)
	{
		CHECK_EQ(sink.hits[0].formId, 3u);
		CHECK_EQ(sink.hits[0].a, 10.5f);
		CHECK_EQ(sink.hits[0].c, -1.0f);
		CHECK_EQ(sink.hits[0].d, 0.5f);
		CHECK_EQ(sink.hits[0].flags, uint32_t(proto::kHitCritical));
		CHECK_EQ(sink.hits[0].weapon, uint32_t(proto::kWeaponBlade));
		CHECK_EQ(sink.deaths[0].formId, 3u);
	}
	CHECK_EQ(link.State().hitEvents, 1ull);
	CHECK_EQ(link.State().deathEvents, 1ull);
	CHECK_EQ(link.State().otherEvents, 1ull);  // the arrow
}

TEST(LinkWritesTheActorTableAndEmptiesItOnce)
{
	FakeMinecraft                   minecraft;
	Link                            link(minecraft.base);
	std::vector<proto::ActorRecord> records(2);
	records[0].formId = 1;
	records[1].formId = 2;
	link.WriteActors(records);
	CHECK_EQ(minecraft.Actors().seq, 2u);
	CHECK_EQ(minecraft.Actors().count, 2u);
	CHECK_EQ(minecraft.Actors().actors[1].formId, 2u);
	CHECK_EQ(link.State().actors, size_t(2));
	link.ClearActors();
	CHECK_EQ(minecraft.Actors().seq, 4u);
	CHECK_EQ(minecraft.Actors().count, 0u);
	CHECK_EQ(link.State().actors, size_t(0));
	link.ClearActors();  // once: the table isn't written again
	CHECK_EQ(minecraft.Actors().seq, 4u);
	link.WriteActors(records);
	CHECK_EQ(minecraft.Actors().seq, 6u);
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'WriteActors': is not a member of 'cpmc::link::Link'`.

- [x] **Step 4: Edit `src/link/Rings.h`**

```cpp
void WriteSkyState(uint8_t* aBase, const proto::SkyState& aState);
```
```cpp
void WriteSkyState(uint8_t* aBase, const proto::SkyState& aState);

// SkyCraft's actor table, at aBase + kOffActorTable (spike 6): as SkyCraft's Skyrim side writes it (Link::WriteActors),
// the sequence goes odd, then the count and the records, at most kMaxActors of them, then even, 2 up from where it was.
void WriteActorTable(uint8_t* aBase, std::span<const proto::ActorRecord> aRecords);
```

- [x] **Step 5: Edit `src/link/Rings.cpp`**

```cpp
#include <atomic>
```
```cpp
#include <algorithm>
#include <atomic>
```
```cpp
	std::memcpy(aBase + proto::kOffSkyState + 4, reinterpret_cast<const uint8_t*>(&aState) + 4, sizeof(proto::SkyState) - 4);
```
```cpp
	std::memcpy(aBase + proto::kOffSkyState + 4, reinterpret_cast<const uint8_t*>(&aState) + 4, sizeof(proto::SkyState) - 4);
	seq.store(count + 2, std::memory_order_release);
}

void WriteActorTable(uint8_t* aBase, std::span<const proto::ActorRecord> aRecords)
{
	auto           seq = At<uint32_t>(aBase, proto::kOffActorTable);
	const uint32_t count = seq.load(std::memory_order_relaxed);
	seq.store(count + 1, std::memory_order_relaxed);
	std::atomic_thread_fence(std::memory_order_release);
	auto* const    table = reinterpret_cast<proto::ActorTable*>(aBase + proto::kOffActorTable);
	const uint32_t records = uint32_t(std::min<size_t>(aRecords.size(), proto::kMaxActors));
	table->count = records;
	if (records != 0)
	{
		std::memcpy(table->actors, aRecords.data(), sizeof(proto::ActorRecord) * records);
	}
```

- [x] **Step 6: Edit `src/link/Link.h`**

```cpp
// changes; and Minecraft's solid blocks by section (its "Part 3"), for V's bodies. It works on the mapping's memory, so
// the tests give it plain memory with a fake Minecraft.
```
```cpp
// changes; and Minecraft's solid blocks by section (its "Part 3"), for V's bodies. Spike 6 writes Night City's NPCs into
// SkyCraft's actor table, and passes on Minecraft's hits on them and its player's death. It works on the mapping's
// memory, so the tests give it plain memory with a fake Minecraft.
```
```cpp
	virtual void Explosion(const proto::McEvent&) {}
```
```cpp
	virtual void Explosion(const proto::McEvent&) {}
	virtual void HitActor(const proto::McEvent&) {}    // spike 6: Minecraft hit an NPC's stand-in this tick
	virtual void PlayerDied(const proto::McEvent&) {}  // spike 6: Minecraft's player died, killed by formId, or 0
```
```cpp
	void RequestFloor();                                               // a fresh floor at the next update
```
```cpp
	void RequestFloor();                                               // a fresh floor at the next update
	// Spike 6: the NPCs' records (Actors.h) into SkyCraft's actor table, every frame they're listed; ClearActors empties
	// the table when listing stops, once.
	void WriteActors(std::span<const proto::ActorRecord> aRecords);
	void ClearActors();
```
```cpp
		uint64_t              explosions = 0;
```
```cpp
		uint64_t              explosions = 0;
		uint64_t              hitEvents = 0;    // spike 6: kEvHitActor
		uint64_t              deathEvents = 0;  // spike 6: kEvPlayerDied
```
```cpp
		std::optional<double> lastHold;             // seconds Minecraft held its player when build mode last started
```
```cpp
		std::optional<double> lastHold;             // seconds Minecraft held its player when build mode last started
		// The NPCs (spike 6)
		size_t actors = 0;  // records in SkyCraft's actor table now
```
```cpp
	std::deque<GroundWorker::Message> outbox_;  // the city's messages, waiting for room in the collision ring
```
```cpp
	std::deque<GroundWorker::Message> outbox_;  // the city's messages, waiting for room in the collision ring
	bool                              actorsWritten_ = false;  // the actor table holds records ClearActors would empty
```

- [x] **Step 7: Edit `src/link/Link.cpp`**

```cpp
#include <chrono>
```
```cpp
#include <algorithm>
#include <chrono>
```
```cpp
		}
		else
		{
			++status_.otherEvents;
```
```cpp
		}
		else if (aEvent.type == proto::kEvHitActor)
		{
			++status_.hitEvents;
			aSink.HitActor(aEvent);
		}
		else if (aEvent.type == proto::kEvPlayerDied)
		{
			++status_.deathEvents;
			aSink.PlayerDied(aEvent);
		}
		else
		{
			++status_.otherEvents;  // arrows stuck in NPCs, Skyrim's skills
```
```cpp
	floorRequested_ = true;
}
}
```
```cpp
	floorRequested_ = true;
}

void Link::WriteActors(std::span<const proto::ActorRecord> aRecords)
{
	WriteActorTable(base_, aRecords);
	status_.actors = std::min<size_t>(aRecords.size(), proto::kMaxActors);
	actorsWritten_ = true;
}

void Link::ClearActors()
{
	if (actorsWritten_)
	{
		WriteActorTable(base_, {});
		status_.actors = 0;
		actorsWritten_ = false;
	}
}
}
```

- [x] **Step 8: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `406 test(s), 0 failure(s)`.

- [x] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: the link writes SkyCraft's actor table, and passes on hits on NPCs and Minecraft's death" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 3: `mclink` lists made-up NPCs, and prints Minecraft's hits on them

Step 1 runs against SkyCraft's real mod without the game, so `mclink` writes records of its own (the spec's
"`mclink`"):
- **`actor <id> <x> <y> <z> [<yaw> <width> <height> [<name>]]`:** a record with a number from 1 to 4,294,967,295, its
  feet in Minecraft's axes, and optionally its yaw, size and name; without them it faces south and is person-sized, 0.6
  by 1.8. The name is the rest of the line;
- **`actor-remove <id>|all`:** one record, or all of them;
- the table is written every frame while it holds records, and emptied once when it holds none, as the plugin does.

`mclink` prints each `kEvHitActor` (the actor, Minecraft's damage, the knockback, the weapon and flags) and
`kEvPlayerDied`, and the status's events line counts them. A hit's words come from `HitWords`, which moves from
`mclink` into `link/Actors`, so the plugin's log says them the same way in Task 9.

`tools/mclink/npcs.txt` is step 1's script, at a fresh spot 40 blocks east of `blast.txt`'s:
- three records around the stand-in V, 2 blocks north, 3 east and 3 west, which `/execute if entity` counts;
- in build mode, looking at the one to the north: a left click with an empty hand, then with a diamond sword given by
  `/item replace`, its cooldown full;
- a stone block 4 blocks south with a stone pressure plate on it and a redstone lamp beside the plate, all set by
  command; the north record moved onto the plate, its feet on the block's top, lights the lamp, and moved off, the lamp
  goes out (`/execute if block`);
- one record removed, then all of them, and the blocks cleared.

**Files:**
- Create: `tools/mclink/npcs.txt`
- Modify: `tools/mclink/McLinkScript.h`, `tools/mclink/McLinkScript.cpp`, `tools/mclink/Main.cpp`,
  `src/link/Actors.h`, `src/link/Actors.cpp`, `tests/TestMcLinkScript.cpp`, `tests/TestActors.cpp`

- [x] **Step 1: Edit `tests/TestMcLinkScript.cpp`: the failing tests**

```cpp
	for (const char* name : { "step1.txt", "again.txt", "eye.txt", "drive.txt", "relink.txt", "city.txt" })
```
```cpp
	for (const char* name : { "step1.txt", "again.txt", "eye.txt", "drive.txt", "relink.txt", "city.txt", "npcs.txt" })
```
```cpp
TEST(ScriptParsesTheGround)
```
```cpp
TEST(ScriptParsesMadeUpNpcs)
{
	std::string error;
	const auto  actions = ParseScript("actor 1 520.5 6.14 2005.5\nactor 2 523.5 6.14 2007.5 90 0.64 1.72 Ganger  Two \n"
	                                  "actor-remove 2\nactor-remove all\n",
	                                  error);
	CHECK(actions.has_value());
	CHECK_EQ(actions->size(), size_t(4));
	const auto& a = *actions;
	CHECK(a[0].kind == Kind::Actor);
	CHECK_EQ(a[0].numbers[0], 1.0);
	CHECK_EQ(a[0].numbers[3], 2005.5);
	CHECK_EQ(a[0].numbers[4], 0.0);  // facing south
	CHECK_EQ(a[0].numbers[5], 0.6);  // person-sized
	CHECK_EQ(a[0].numbers[6], 1.8);
	CHECK_STR(a[0].text, "");
	CHECK_EQ(a[1].numbers[4], 90.0);
	CHECK_EQ(a[1].numbers[5], 0.64);
	CHECK_EQ(a[1].numbers[6], 1.72);
	CHECK_STR(a[1].text, "Ganger  Two");  // as written, but for the line's ends
	CHECK(a[2].kind == Kind::ActorRemove);
	CHECK_EQ(a[2].numbers[0], 2.0);
	CHECK(a[3].kind == Kind::ActorRemove);
	CHECK_EQ(a[3].numbers[0], 0.0);  // all
	CHECK(!ParseScript("actor 0 1 2 3", error).has_value());
	CHECK(!ParseScript("actor 4294967296 1 2 3", error).has_value());
	CHECK(!ParseScript("actor 1.5 1 2 3", error).has_value());
	CHECK(!ParseScript("actor 1 1 2", error).has_value());
	CHECK(!ParseScript("actor 1 1 2 3 90", error).has_value());        // a yaw needs a size with it
	CHECK(!ParseScript("actor 1 1 2 3 90 0 1.8", error).has_value());  // no width
	CHECK(!ParseScript("actor-remove", error).has_value());
	CHECK(!ParseScript("actor-remove some", error).has_value());
	CHECK_STR(error, "line 1: actor-remove takes a number from 1 to 4294967295, or all: \"actor-remove some\"");
}

TEST(ScriptParsesTheGround)
```

- [x] **Step 2: Edit `tests/TestActors.cpp`: the failing test**

```cpp
	CHECK_EQ(ids.Given(), 259u);            // three NPCs the first table had no room for
}
```
```cpp
	CHECK_EQ(ids.Given(), 259u);            // three NPCs the first table had no room for
}

TEST(ActorsDescribeAHitsWeaponAndFlags)
{
	proto::McEvent hit{};
	hit.type = proto::kEvHitActor;
	hit.weapon = proto::kWeaponBlade;
	hit.flags = proto::kHitCritical | proto::kHitSweep;
	CHECK_STR(HitWords(hit), "blade, critical, sweep");
	hit.weapon = proto::kWeaponArrow;
	hit.flags = proto::kHitProjectile | proto::kHitFire;
	CHECK_STR(HitWords(hit), "arrow, projectile, fire");
	hit.weapon = 9;  // a class SkyCraft doesn't have
	hit.flags = 0;
	CHECK_STR(HitWords(hit), "weapon 9");
}
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C3861: 'HitWords': identifier not found`.

- [x] **Step 4: Edit `tools/mclink/McLinkScript.h`**

```cpp
//   wait-ground [seconds]          until the city around the centre is all read and in the ring (30 s unless given)
```
```cpp
//   wait-ground [seconds]          until the city around the centre is all read and in the ring (30 s unless given)
//   actor <id> <x> <y> <z> [<yaw> <width> <height> [<name>]]
//                                  a made-up NPC in SkyCraft's actor table (spike 6), or one moved: its number, from 1
//                                  to 4294967295; its feet, in Minecraft blocks; its yaw, in Minecraft degrees, and its
//                                  size, in blocks above 0 (0, 0.6 and 1.8 unless given); then a name to the line's end
//   actor-remove <id>|all          one made-up NPC out of the table, or all of them
```
```cpp
		WaitGround,
```
```cpp
		WaitGround,
		Actor,
		ActorRemove,
```
```cpp
	Kind        kind = Kind::Quit;
	double      numbers[3]{};  // seconds; x, y, z; yaw and pitch; the scancode; the button (1 left, 2 middle, 3 right)
	bool        on = false;    // link, build and hud
	std::string text;          // the command, the file, or the ground
```
```cpp
	Kind   kind = Kind::Quit;
	double numbers[7]{};  // seconds; x, y, z; yaw and pitch; the scancode; the button (1 left, 2 middle, 3 right); an
	                      // actor's number, x, y, z, yaw, width and height; the actor to remove, or 0 for all
	bool        on = false;  // link, build and hud
	std::string text;        // the command, the file, the ground, or an actor's name
```

- [x] **Step 5: Edit `tools/mclink/McLinkScript.cpp`**

```cpp
	return error == std::errc() && end == aWord.data() + aWord.size() && std::isfinite(aOut);
```
```cpp
	return error == std::errc() && end == aWord.data() + aWord.size() && std::isfinite(aOut);
}

// An actor's number: a whole one from 1 to 4294967295, as SkyCraft's 32-bit formId holds it, 0 meaning none.
bool WholeId(double aValue)
{
	return aValue >= 1.0 && aValue <= 4294967295.0 && aValue == std::floor(aValue);
```
```cpp
		}
		else if (verb == "floor" || verb == "status" || verb == "quit")
```
```cpp
		}
		else if (verb == "actor")
		{
			action.kind = Action::Kind::Actor;
			action.numbers[5] = 0.6;
			action.numbers[6] = 1.8;
			const size_t given = words.size() >= 8 ? 7 : words.size() - 1;
			bool         ok = words.size() == 5 || words.size() >= 8;
			for (size_t i = 0; ok && i < given; ++i)
			{
				ok = Number(words[i + 1], action.numbers[i]);
			}
			if (!ok || !WholeId(action.numbers[0]) || action.numbers[5] <= 0 || action.numbers[6] <= 0)
			{
				return fail("actor takes a number from 1 to 4294967295, x, y and z; then a yaw, a width and a height above 0; then a name");
			}
			if (words.size() > 8)
			{
				action.text = std::string(Trim(line.substr(size_t(words[8].data() - line.data()))));  // the name, as written
			}
		}
		else if (verb == "actor-remove")
		{
			action.kind = Action::Kind::ActorRemove;
			if (!(words.size() == 2 && words[1] == "all") && (!numbers(1) || !WholeId(action.numbers[0])))
			{
				return fail("actor-remove takes a number from 1 to 4294967295, or all");
			}
		}
		else if (verb == "floor" || verb == "status" || verb == "quit")
```

- [x] **Step 6: Edit `src/link/Actors.h`**

```cpp
// The records of the nearest proto::kMaxActors NPCs within kActorRange of aV (world axes), nearest first, numbered by
```
```cpp
// A hit's weapon class and flags, as SkyCraft's SkyrimActorEntity sets them: "blade, critical, sweep".
std::string HitWords(const proto::McEvent& aEvent);

// The records of the nearest proto::kMaxActors NPCs within kActorRange of aV (world axes), nearest first, numbered by
```

- [x] **Step 7: Edit `src/link/Actors.cpp`**

```cpp
#include <cstring>
```
```cpp
#include <cstring>
#include <format>
#include <iterator>
#include <utility>
```
```cpp
std::vector<proto::ActorRecord> ActorRecords(std::span<const NpcSample> aNpcs, const Vec3d& aV,
```
```cpp
std::string HitWords(const proto::McEvent& aEvent)
{
	static const char* const weapons[] = { "unarmed", "blade", "axe", "blunt", "pierce", "arrow" };
	std::string words = aEvent.weapon < std::size(weapons) ? weapons[aEvent.weapon] : std::format("weapon {}", aEvent.weapon);
	for (const auto& [flag, word] : { std::pair{ proto::kHitCritical, "critical" }, std::pair{ proto::kHitProjectile, "projectile" },
	                                  std::pair{ proto::kHitSweep, "sweep" }, std::pair{ proto::kHitFire, "fire" } })
	{
		if (aEvent.flags & flag)
		{
			words += std::string(", ") + word;
		}
	}
	return words;
}

std::vector<proto::ActorRecord> ActorRecords(std::span<const NpcSample> aNpcs, const Vec3d& aV,
```

- [x] **Step 8: Edit `tools/mclink/Main.cpp`**

```cpp
// mclink: a console stand-in for Cyberpunk's side of SkyCraft's protocol, for spike 4's step 1 and spike 5's steps 1
// and 3. It runs BlockRender's link code against the real Minecraft, without the game
// (docs/superpowers/specs/2026-10-02-minecraft-link-design.md and 2026-10-03-collision-and-movement-design.md).
```
```cpp
// mclink: a console stand-in for Cyberpunk's side of SkyCraft's protocol, for spike 4's step 1, spike 5's steps 1 and 3,
// and spike 6's steps 1 and 3. It runs BlockRender's link code against the real Minecraft, without the game
// (docs/superpowers/specs/2026-10-02-minecraft-link-design.md, 2026-10-03-collision-and-movement-design.md and
// 2026-10-05-npcs-and-combat-design.md).
```
```cpp
// It reports what Minecraft sends, its solid blocks among it, and refuses to start while another game side uses the
// mapping.
```
```cpp
// Made-up NPCs go into SkyCraft's actor table, as the plugin writes the game's. It reports what Minecraft sends, its solid
// blocks and its hits on the NPCs among it, and refuses to start while another game side uses the mapping.
```
```cpp
#include "Coords.h"
```
```cpp
#include "Coords.h"
#include "link/Actors.h"
```
```cpp
#include <cstdio>
#include <format>
#include <fstream>
```
```cpp
#include <cstdio>
#include <cstring>
#include <format>
#include <fstream>
#include <map>
```
```cpp
		Say("explosion at {:.1f} {:.1f} {:.1f}, radius {:.1f}", aEvent.a, aEvent.b, aEvent.c, aEvent.d);
```
```cpp
		Say("explosion at {:.1f} {:.1f} {:.1f}, radius {:.1f}", aEvent.a, aEvent.b, aEvent.c, aEvent.d);
	}

	void HitActor(const proto::McEvent& aEvent) override
	{
		Say("hit: actor {} for {:.2f}, knockback {:.2f} towards x {:.2f} z {:.2f}; {}", aEvent.formId, aEvent.a, aEvent.d, aEvent.b, aEvent.c,
		    HitWords(aEvent));
	}

	void PlayerDied(const proto::McEvent& aEvent) override
	{
		if (aEvent.formId != 0)
		{
			Say("Minecraft's player died, killed by actor {}", aEvent.formId);
		}
		else
		{
			Say("Minecraft's player died, killed by none of the actors");
		}
```
```cpp
	Say("events: {} explosions, {} others; {} inputs dropped; {} commands waiting; {} HUD images, {} refused", s.explosions, s.otherEvents,
	    s.inputsDropped, s.commandsWaiting, s.hudImages, s.hudRefused);
```
```cpp
	Say("events: {} explosions, {} hits on actors, {} deaths, {} others; {} actors in the table; {} inputs dropped; {} commands waiting; "
	    "{} HUD images, {} refused",
	    s.explosions, s.hitEvents, s.deathEvents, s.otherEvents, s.actors, s.inputsDropped, s.commandsWaiting, s.hudImages, s.hudRefused);
```
```cpp
	DumpGround dump;
```
```cpp
	DumpGround dump;
	std::map<uint32_t, proto::ActorRecord> actors;  // the made-up NPCs, by number
```
```cpp
		link.ReadGround(now, dump, 1.5);  // the plugin's budget a frame
```
```cpp
		link.ReadGround(now, dump, 1.5);  // the plugin's budget a frame
		if (actors.empty())
		{
			link.ClearActors();
		}
		else
		{
			std::vector<proto::ActorRecord> records;
			for (const auto& [id, record] : actors)
			{
				records.push_back(record);
			}
			link.WriteActors(records);  // every frame, as the plugin writes the game's NPCs
		}
```
```cpp
			}
			case Action::Kind::Status: PrintStatus(link, report, feet); break;
```
```cpp
			}
			case Action::Kind::Actor:
			{
				proto::ActorRecord record{};
				record.formId = uint32_t(action.numbers[0]);
				record.x = float(action.numbers[1]);
				record.y = float(action.numbers[2]);
				record.z = float(action.numbers[3]);
				record.yaw = float(action.numbers[4]);
				record.width = float(action.numbers[5]);
				record.height = float(action.numbers[6]);
				record.healthFrac = 1.0f;
				const std::string name = CutName(action.text, sizeof record.name - 1);
				std::memcpy(record.name, name.data(), name.size());
				actors[record.formId] = record;
				Say("actor {} at {:.2f} {:.2f} {:.2f}, {:.2f} by {:.2f}{}", record.formId, record.x, record.y, record.z, record.width,
				    record.height, name.empty() ? "" : ": " + name);
				break;
			}
			case Action::Kind::ActorRemove:
				if (action.numbers[0] == 0.0)
				{
					actors.clear();
				}
				else
				{
					actors.erase(uint32_t(action.numbers[0]));
				}
				Say("{} actors left in the table", actors.size());
				break;
			case Action::Kind::Status: PrintStatus(link, report, feet); break;
```

- [x] **Step 9: Write `tools/mclink/npcs.txt`**

```text
# Spike 6's step 1: made-up NPCs in SkyCraft's actor table, against SkyCraft's real mod, without the game. At a fresh
# spot 40 blocks east of blast.txt's, three stand-ins go around the stand-in V: Minecraft's log counts them; a fist and
# then a diamond sword hit one, and come back as hits; one stands on a stone pressure plate and lights the lamp beside
# it; and removing their records removes them. Start Minecraft first; this waits for its world.
status
wait-world 180
at 520.5 6.14 2007.5
look 180 0
link on
wait 3
type /gamemode creative
# Three stand-ins: 2 blocks north of the stand-in V, 3 east and 3 west.
actor 1 520.5 6.14 2005.5 0 0.6 1.8 Ganger One
actor 2 523.5 6.14 2007.5 90 0.6 1.8 Ganger Two
actor 3 517.5 6.14 2007.5 -90 0.6 1.8 Ganger Three
wait 2
type /execute if entity @e[type=skycraft:skyrim_actor]
# The hits: in build mode, looking north and a little down at actor 1, a fist, then a diamond sword with its cooldown full.
build on
wait 2
look 180 20
type /item replace entity @s weapon.mainhand with minecraft:air
wait 1
click left
wait 2
type /item replace entity @s weapon.mainhand with minecraft:diamond_sword
wait 2
click left
wait 2
build off
# The plate: a stone block 4 blocks south, a stone pressure plate on it, and a redstone lamp beside the plate.
type /setblock 520 6 2011 minecraft:stone
type /setblock 520 7 2011 minecraft:stone_pressure_plate
type /setblock 521 7 2011 minecraft:redstone_lamp
wait 1
type /execute if block 521 7 2011 minecraft:redstone_lamp[lit=false]
# Actor 1 onto the plate, its feet on the stone block's top, then off it again.
actor 1 520.5 7 2011.5 0 0.6 1.8 Ganger One
wait 2
type /execute if block 521 7 2011 minecraft:redstone_lamp[lit=true]
actor 1 520.5 6.14 2005.5 0 0.6 1.8 Ganger One
wait 3
type /execute if block 521 7 2011 minecraft:redstone_lamp[lit=false]
# Removal: one record, then all of them.
actor-remove 2
wait 2
type /execute if entity @e[type=skycraft:skyrim_actor]
actor-remove all
wait 2
type /execute unless entity @e[type=skycraft:skyrim_actor]
type /fill 520 6 2011 521 7 2011 minecraft:air
status
quit
```

- [x] **Step 10: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `408 test(s), 0 failure(s)`.

- [x] **Step 11: Run mclink without Minecraft**

Minecraft and the game must not be running.
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && printf 'status\nquit\n' > build/selfcheck.txt \
  && ./build/RelWithDebInfo/mclink.exe build/selfcheck.txt; echo "exit: $?"
```
Expected: `the mapping: created (process …); waiting for Minecraft`, then the status with `Minecraft: absent` and
`events: 0 explosions, 0 hits on actors, 0 deaths, 0 others; 0 actors in the table; …`. Then `exit: 0`.

- [x] **Step 12: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: mclink lists made-up NPCs for SkyCraft, and prints Minecraft's hits on them" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 4: In Minecraft: step 1

Step 1 of the spec's "The steps", with the user: made-up NPCs in SkyCraft's actor table, from `mclink`, against
SkyCraft's unchanged mod in the `CyberCraft` instance, without the game.
- **The instance** has had SkyCraft 0.1.2, Fabric API and CyberCraft Tweaks since spike 5, and the Java arguments
  `--enable-native-access=ALL-UNNAMED -Dskycraft.quitWithSkyrim=false`, so Minecraft keeps running between `mclink`
  runs.
- **What SkyCraft logs** (its `SkyCombat`, at 0.1.2): `SkyCraft: N Skyrim actors mirrored as hittable stand-ins` when
  the count changes and is under 5 or a multiple of 5, and `SkyCraft: hit <name> for <damage> (knockback <k>)` for each
  hit it sends. The chat's replies to the script's `/execute` checks go into Minecraft's log too, after `[CHAT]`.
- **If Minecraft doesn't link,** its log says why, in lines starting `SkyCraft:`.
- **If a check fails,** stop and find the cause with superpowers:systematic-debugging before going on. Fixes become new
  tasks.

**Files:**
- Create: `docs/spikes/2026-10-05-npcs-and-combat-results.md`

- [x] **Step 1: Check that the game isn't running**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game: it would hold the mapping.

- [x] **Step 2: Ask the user to start Minecraft**

The user starts the `CyberCraft` instance from the Modrinth App, and says when its title screen shows. Then:
```bash
grep -E "SkyCraft:" "/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log" | tail -3
```
Expected: `SkyCraft: can't open Skyrim's shared memory yet (Windows error 2: …)`: waiting for a game side.

- [x] **Step 3: Run step 1's script**

```bash
L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"; before=$(wc -l < "$L"); \
  cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/step1 && cd build/step1 \
  && ../RelWithDebInfo/mclink.exe ../../tools/mclink/npcs.txt | tee npcs.log; echo "exit: ${PIPESTATUS[0]}"; \
  tail -n +$((before + 1)) "$L" | grep -E "SkyCraft: (hit|[0-9]+ Skyrim)|\[CHAT\]" | tee minecraft.log
```
It takes about half a minute once Minecraft is in its world, and up to four if it opens the world first: give the
command 6 minutes. Expected, against the spec's "Step 1":
- **Linking:** `Minecraft is there (process …)`, then `floor 1 at height 6.14 around Minecraft 520.5 2007.5`.
- **Stand-ins:** `actor 1 at 520.50 6.14 2005.50, 0.60 by 1.80: Ganger One`, and the two others; in Minecraft's lines,
  `SkyCraft: 3 Skyrim actors mirrored as hittable stand-ins` and `[CHAT] Test passed, count: 3`.
- **Hits:** `hit: actor 1 for 1.00, …; unarmed`, then `hit: actor 1 for 7.00, …; blade`, each with Minecraft's
  knockback (0.4 for a hit that isn't sprinting); in Minecraft's lines, `SkyCraft: hit Ganger One for 1.0 (knockback
  …)` and `SkyCraft: hit Ganger One for 7.0 (knockback …)`. A smaller number means the cooldown wasn't full; no hit
  means the click missed actor 1, which fills the crosshair when looking 180 20.
- **The plate:** three `[CHAT] Test passed` replies to the lamp's checks: out, lit with actor 1 on the plate, and out
  again once it's off.
- **Removal:** `2 actors left in the table`, then `SkyCraft: 2 Skyrim actors mirrored as hittable stand-ins` and
  `[CHAT] Test passed, count: 2`; after `actor-remove all`, `0 actors left in the table`, `SkyCraft: 0 Skyrim actors
  mirrored as hittable stand-ins`, and a last `[CHAT] Test passed`.
- **The status:** `events: 0 explosions, 2 hits on actors, 0 deaths, 0 others; 0 actors in the table; …`.
- **No `[CHAT] Test failed`,** and `exit: 0`.

- [x] **Step 4: Ask the user to stop Minecraft**

Its window is hidden while `mclink` drives it, so the user stops it from the Modrinth App.

- [x] **Step 5: Write step 1's results, `docs/spikes/2026-10-05-npcs-and-combat-results.md`**

From `build/step1/npcs.log` and `build/step1/minecraft.log`, as spike 5's results doc does: the versions; each check
above with its outcome and numbers (the hits' damage and knockback, the lamp's three checks, the counts); anything that
surprised, and what it means for step 2. The spike's answer goes at the top once step 2 is in.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-05-npcs-and-combat-results.md \
  && git commit -q -m "npcs-and-combat: step 1's results" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 5: NPCs' sizes from their capsules in PhysX

An NPC's stand-in is person-sized until its own size is known (the spec's "Sizes"). An NPC's character controller is,
as V's is, a kinematic capsule standing around it; V's is centred 1 m above its feet, with radius 0.32 and half-height
0.4, so it spans 0.28 to 1.72 m above them.
- **Looked for** when an NPC is first listed, and again every 2 s until one is found, at most eight a frame, those
  never looked for first, nearest first (`NpcSizes::ToLookFor`): an overlap of a 1 m sphere centred 1 m above its
  feet, keeping kinematic actors' capsules (`PhysxGround::Capsules`). The retries are for an NPC the game gives a
  capsule only near V, or a little after it appears: NPCs are first listed up to 80 m away.
- **Its own** (`PickCapsule`): one whose axis passes within 0.2 m of its feet and leans at most 25 degrees from upright,
  so a body lying down or a prop's capsule nearby isn't taken; of several, the narrowest, since V has a second
  kinematic capsule, radius 1, around its controller.
- **Its box** (`BoxOf`): twice the radius wide, from the lower of the feet and the capsule's bottom up to its top.
  `ActorRecords` takes the boxes found by entity (Task 1); the others stay person-sized.
- **Forgotten** with the NPC's number, a minute after it was last listed, so one that comes back is looked for again:
  `ActorIds::IdOf` says whether an entity still has a number, without marking it listed.
- **For the `npcs` request** (Task 9), `DescribeNpc` gives an NPC's line: its number, name, distance, box and where the
  box came from (a capsule's radius, half-height, lean and query filter, or a person's), health, level and flags.

Which capsule is an NPC's is step 2's opening question: `shapes` beside an NPC lists every shape there with its
filters, and `npcs` says which capsule each box came from.

**Files:**
- Create: `src/physx/Characters.h`, `src/physx/Characters.cpp`, `tests/TestCharacters.cpp`
- Modify: `src/link/Actors.h`, `src/link/Actors.cpp`, `src/physx/Export.h`, `src/physx/Export.cpp`, `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestCharacters.cpp`**

```cpp
#include "Harness.h"

#include "physx/Characters.h"

#include <cmath>
#include <numbers>
#include <string>
#include <vector>

using namespace cpmc;
using namespace cpmc::px;

namespace
{
// An NPC's feet on the road at spike 3's corner.
constexpr Vec3d kFeet{ -119.5, -2007.5, 6.14 };

// A capsule standing at aX, aY (world axes), its centre aUp metres above the road.
CapsuleSeen Standing(double aX, double aY, double aUp, double aRadius, double aHalfHeight)
{
	CapsuleSeen capsule;
	capsule.centre = { aX, aY, kFeet.z + aUp };
	capsule.radius = aRadius;
	capsule.halfHeight = aHalfHeight;
	return capsule;
}

// aCapsule leaning aDegrees from upright, its top towards the world's +x; with aThroughTheFeet, moved along x so its axis
// passes through the feet.
CapsuleSeen Leaning(CapsuleSeen aCapsule, double aDegrees, bool aThroughTheFeet)
{
	const double angle = aDegrees * std::numbers::pi / 180.0;
	aCapsule.axis = { std::sin(angle), 0.0, std::cos(angle) };
	if (aThroughTheFeet)
	{
		aCapsule.centre.x = kFeet.x + std::tan(angle) * (aCapsule.centre.z - kFeet.z);
	}
	return aCapsule;
}

// V's controller (spike 1): radius 0.32, half-height 0.4, centred 1 m above its feet.
CapsuleSeen Controller()
{
	return Standing(kFeet.x, kFeet.y, 1.0, 0.32, 0.4);
}
}

TEST(CharactersPickTheNpcsOwnCapsule)
{
	// V's pair: its controller, and a wide capsule, radius 1, around it. The narrower is the controller.
	const std::vector<CapsuleSeen>   pair{ Standing(kFeet.x, kFeet.y, 1.0, 1.0, 0.4), Controller() };
	const std::optional<CapsuleSeen> picked = PickCapsule(pair, kFeet);
	CHECK(picked.has_value());
	CHECK_NEAR(picked ? picked->radius : 0.0, 0.32, 1e-9);
	// Another NPC's, 0.25 m off across: not this one's. 0.15 m off: this one's.
	CHECK(!PickCapsule(std::vector{ Standing(kFeet.x + 0.25, kFeet.y, 1.0, 0.32, 0.4) }, kFeet).has_value());
	CHECK(PickCapsule(std::vector{ Standing(kFeet.x, kFeet.y + 0.15, 1.0, 0.32, 0.4) }, kFeet).has_value());
	// Leaning 20 degrees with its axis through the feet: kept. Leaning 30: lying too far over for a controller.
	CHECK(PickCapsule(std::vector{ Leaning(Controller(), 20.0, true) }, kFeet).has_value());
	CHECK(!PickCapsule(std::vector{ Leaning(Controller(), 30.0, true) }, kFeet).has_value());
	// Leaning 20 degrees, centred over the feet: its axis passes 0.34 m from them.
	CHECK(!PickCapsule(std::vector{ Leaning(Controller(), 20.0, false) }, kFeet).has_value());
	CHECK(!PickCapsule(std::vector<CapsuleSeen>{}, kFeet).has_value());
}

TEST(CharactersBoxReachesFromTheFeetOrTheCapsulesBottomToItsTop)
{
	// V's controller spans 0.28 to 1.72 m above the feet, so the box starts at the feet.
	const link::NpcBox v = BoxOf(Controller(), kFeet);
	CHECK_NEAR(v.width, 0.64, 1e-9);
	CHECK_NEAR(v.below, 0.0, 1e-9);
	CHECK_NEAR(v.above, 1.72, 1e-9);
	// A capsule reaching 0.12 m below the feet: the box starts at its bottom.
	const link::NpcBox low = BoxOf(Standing(kFeet.x, kFeet.y, 0.6, 0.32, 0.4), kFeet);
	CHECK_NEAR(low.below, 0.12, 1e-9);
	CHECK_NEAR(low.above, 1.32, 1e-9);
	// Leaning 20 degrees: its top is lower by the lean.
	const link::NpcBox leaning = BoxOf(Leaning(Controller(), 20.0, false), kFeet);
	CHECK_NEAR(leaning.above, 1.0 + 0.4 * std::cos(20.0 * std::numbers::pi / 180.0) + 0.32, 1e-9);
}

TEST(CharactersLookForNpcsAtMostEightAFrameAndAgainUntilFound)
{
	std::vector<uint64_t> listed;
	link::ActorIds        ids;
	for (uint64_t entity = 100; entity < 120; ++entity)
	{
		listed.push_back(entity);
		ids.Id(entity, 0.0);
	}
	NpcSizes                    sizes;
	const std::vector<uint64_t> first = sizes.ToLookFor(listed, 0.0);
	CHECK_EQ(first.size(), size_t(8));
	CHECK(!first.empty() && first.front() == uint64_t(100));  // the nearest first, as listed
	const std::vector<uint64_t> second = sizes.ToLookFor(listed, 0.0);
	CHECK(!second.empty() && second.front() == uint64_t(108));
	CHECK_EQ(sizes.ToLookFor(listed, 0.5).size(), size_t(4));
	CHECK(sizes.ToLookFor(listed, 1.0).empty());  // all looked for, and none due again yet
	sizes.Found(101, Controller(), kFeet);
	CHECK_EQ(sizes.FoundCount(), size_t(1));
	CHECK_EQ(sizes.LookedForCount(), size_t(20));
	CHECK_NEAR(sizes.Boxes().contains(101) ? sizes.Boxes().at(101).width : 0.0, 0.64, 1e-9);
	CHECK(sizes.Capsule(101) != nullptr);
	CHECK(sizes.Capsule(100) == nullptr);  // looked for, and none found: a person's size
	// 2 s after their last look, those without a capsule are looked for again, nearest first; 101's was found.
	const std::vector<uint64_t> again = sizes.ToLookFor(listed, 2.0);
	CHECK_EQ(again.size(), size_t(8));
	CHECK(again.size() >= 2 && again[0] == uint64_t(100) && again[1] == uint64_t(102));
	CHECK_EQ(sizes.ToLookFor(listed, 2.0).size(), size_t(7));  // the rest of those looked for at 0 s
	CHECK(sizes.ToLookFor(listed, 2.0).empty());               // those looked for at 0.5 s are due at 2.5 s
	CHECK_EQ(sizes.ToLookFor(listed, 2.5).size(), size_t(4));
	// A minute on, only 101 is still listed: the others are forgotten with their numbers, and looked for at once if
	// they're listed again.
	ids.Id(101, 59.0);
	ids.Forget(60.0);
	sizes.Forget(ids);
	CHECK_EQ(sizes.LookedForCount(), size_t(1));
	CHECK_EQ(sizes.FoundCount(), size_t(1));
	CHECK_EQ(sizes.ToLookFor(std::vector<uint64_t>{ 100, 101 }, 60.0).size(), size_t(1));
}

TEST(CharactersDescribeAnNpcForTheNpcsRequest)
{
	link::NpcSample npc;
	npc.entity = 7;
	npc.name = "Ganger One";
	npc.health = 80.0f;
	npc.healthMax = 100.0f;
	npc.level = 12.0f;
	npc.flags = link::proto::kActorHostile | link::proto::kActorInCombat;
	NpcSizes sizes;
	CHECK_STR(DescribeNpc(npc, 3, 4.3, sizes), "3 Ganger One, 4.3 m: 0.60 wide, from 0.00 below its position to 1.80 above, a person's "
	                                            "until its capsule is looked for; health 80 of 100, level 12; hostile, in combat");
	sizes.ToLookFor(std::vector<uint64_t>{ 7 }, 0.0);
	CHECK(DescribeNpc(npc, 3, 4.3, sizes).find(" 1.80 above, a person's: no capsule found; ") != std::string::npos);
	CapsuleSeen capsule = Controller();
	capsule.queryFilter = { 0, 0, 0x2000, 0 };
	sizes.Found(7, capsule, kFeet);
	CHECK_STR(DescribeNpc(npc, 3, 4.3, sizes), "3 Ganger One, 4.3 m: 0.64 wide, from 0.00 below its position to 1.72 above, its "
	                                            "capsule's: radius 0.32, half-height 0.40, leaning 0 degrees, query filter "
	                                            "0:0:2000:0; health 80 of 100, level 12; hostile, in combat");
	npc.name.clear();
	npc.flags = link::proto::kActorEssential;
	CHECK(DescribeNpc(npc, 3, 4.3, sizes).starts_with("3 (no name), 4.3 m: "));
	CHECK(DescribeNpc(npc, 3, 4.3, sizes).ends_with("; essential"));
	npc.flags = 0;
	CHECK(DescribeNpc(npc, 3, 4.3, sizes).ends_with("; no flags"));
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestActors.cpp
	tools/mclink/DumpObj.cpp
```
```cmake
	tests/TestActors.cpp
	tests/TestCharacters.cpp
	tools/mclink/DumpObj.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'physx/Characters.h'`.

- [x] **Step 4: Write `src/physx/Characters.h`**

```cpp
#pragma once

#include "Coords.h"
#include "link/Actors.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

// NPCs' sizes from their capsules (spike 6's "Sizes"): an NPC's character controller is, as V's is, a kinematic capsule
// standing around it, which PhysxGround::Capsules reads through the exporter's checks. Each NPC's is looked for when
// it's first listed, and again every 2 s until it's found, in case the game gives an NPC its capsule only near V, or a
// little after it appears. Pure: the tests give it capsules of their own.
namespace cpmc::px
{
constexpr double kCapsuleSearchRadius = 1.0;  // the overlap: a sphere of this radius,
constexpr double kCapsuleSearchHeight = 1.0;  // centred this far above the NPC's feet
constexpr double kCapsuleAcross = 0.2;        // how far an NPC's capsule's axis may pass from its feet
constexpr double kCapsuleTilt = 25.0;         // and how far it may lean from upright, in degrees
constexpr size_t kCapsulesPerFrame = 8;       // NPCs looked for in one frame
constexpr double kCapsuleRetry = 2.0;         // seconds before an NPC whose capsule wasn't found is looked for again

// A kinematic actor's capsule, in the world's axes.
struct CapsuleSeen
{
	Vec3d                   centre;
	Vec3d                   axis{ 0.0, 0.0, 1.0 };  // along its length: the shape's x axis, posed
	double                  radius = 0.0;
	double                  halfHeight = 0.0;  // half the length between its caps' centres
	std::array<uint32_t, 4> queryFilter{};     // its shape's, for the npcs request
};

// The NPC's own among aSeen, for an NPC whose feet are at aFeet: one leaning at most kCapsuleTilt from upright, whose
// axis passes within kCapsuleAcross of the feet; of several, the narrowest, since V has a second kinematic capsule,
// radius 1, around its controller. Nothing if none is.
std::optional<CapsuleSeen> PickCapsule(std::span<const CapsuleSeen> aSeen, const Vec3d& aFeet);

// Its box: twice the radius wide, reaching from the lower of the feet and the capsule's bottom up to its top.
link::NpcBox BoxOf(const CapsuleSeen& aCapsule, const Vec3d& aFeet);

// Which NPCs' capsules to look for, and the boxes found.
class NpcSizes
{
public:
	// Of aListed, the entities listed this frame, nearest first, at most kCapsulesPerFrame to look for at aNow (seconds):
	// those never looked for, then those whose capsule wasn't found when last looked for, kCapsuleRetry or more ago.
	// Each counts as looked for at aNow.
	std::vector<uint64_t> ToLookFor(std::span<const uint64_t> aListed, double aNow);
	// aEntity's capsule, found with its feet at aFeet: its box from now on.
	void Found(uint64_t aEntity, const CapsuleSeen& aCapsule, const Vec3d& aFeet);
	// Forgets the NPCs aIds has forgotten, so one listed again is looked for again.
	void Forget(const link::ActorIds& aIds);

	const std::unordered_map<uint64_t, link::NpcBox>& Boxes() const { return boxes_; }
	// The capsule aEntity's box came from; null if it has none.
	const CapsuleSeen* Capsule(uint64_t aEntity) const;
	bool               LookedFor(uint64_t aEntity) const { return looked_.contains(aEntity); }
	size_t             LookedForCount() const { return looked_.size(); }
	size_t             FoundCount() const { return capsules_.size(); }

private:
	std::unordered_map<uint64_t, double>       looked_;  // when each was last looked for
	std::unordered_map<uint64_t, link::NpcBox> boxes_;
	std::unordered_map<uint64_t, CapsuleSeen>  capsules_;
};

// An NPC's line for the npcs request: its number, name and distance (metres); its box and where it came from; its health,
// level and flags.
std::string DescribeNpc(const link::NpcSample& aNpc, uint32_t aId, double aDistance, const NpcSizes& aSizes);
}
```

- [x] **Step 5: Write `src/physx/Characters.cpp`**

```cpp
#include "physx/Characters.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>
#include <utility>

namespace cpmc::px
{
namespace
{
double Length(const Vec3d& aVector)
{
	return std::sqrt(aVector.x * aVector.x + aVector.y * aVector.y + aVector.z * aVector.z);
}

// How far the capsule's axis leans from upright, as the cosine of the angle: 1 standing, 0 lying down.
double Upright(const CapsuleSeen& aCapsule)
{
	const double length = Length(aCapsule.axis);
	return length > 0.0 ? std::fabs(aCapsule.axis.z) / length : 0.0;
}
}

std::optional<CapsuleSeen> PickCapsule(std::span<const CapsuleSeen> aSeen, const Vec3d& aFeet)
{
	const double               upright = std::cos(kCapsuleTilt * std::numbers::pi / 180.0);
	std::optional<CapsuleSeen> best;
	for (const CapsuleSeen& capsule : aSeen)
	{
		if (Upright(capsule) < upright)
		{
			continue;
		}
		// The axis's distance from the feet: the length of (feet - centre) x axis, over the axis's length.
		const Vec3d  to{ aFeet.x - capsule.centre.x, aFeet.y - capsule.centre.y, aFeet.z - capsule.centre.z };
		const Vec3d& axis = capsule.axis;
		const Vec3d  cross{ to.y * axis.z - to.z * axis.y, to.z * axis.x - to.x * axis.z, to.x * axis.y - to.y * axis.x };
		if (Length(cross) / Length(axis) > kCapsuleAcross)
		{
			continue;
		}
		if (!best || capsule.radius < best->radius)
		{
			best = capsule;
		}
	}
	return best;
}

link::NpcBox BoxOf(const CapsuleSeen& aCapsule, const Vec3d& aFeet)
{
	const double reach = Upright(aCapsule) * aCapsule.halfHeight + aCapsule.radius;  // up and down from its centre
	const double bottom = aCapsule.centre.z - reach;
	const double top = aCapsule.centre.z + reach;
	return { 2.0 * aCapsule.radius, std::max(0.0, aFeet.z - bottom), top - aFeet.z };
}

std::vector<uint64_t> NpcSizes::ToLookFor(std::span<const uint64_t> aListed, double aNow)
{
	std::vector<uint64_t> out;
	for (const uint64_t entity : aListed)  // never looked for
	{
		if (out.size() == kCapsulesPerFrame)
		{
			break;
		}
		if (looked_.try_emplace(entity, aNow).second)
		{
			out.push_back(entity);
		}
	}
	for (const uint64_t entity : aListed)  // then not found, and due again
	{
		if (out.size() == kCapsulesPerFrame)
		{
			break;
		}
		const auto looked = looked_.find(entity);
		if (looked != looked_.end() && !capsules_.contains(entity) && aNow - looked->second >= kCapsuleRetry)
		{
			looked->second = aNow;
			out.push_back(entity);
		}
	}
	return out;
}

void NpcSizes::Found(uint64_t aEntity, const CapsuleSeen& aCapsule, const Vec3d& aFeet)
{
	boxes_[aEntity] = BoxOf(aCapsule, aFeet);
	capsules_[aEntity] = aCapsule;
}

void NpcSizes::Forget(const link::ActorIds& aIds)
{
	std::erase_if(looked_, [&](const auto& aEntry) { return !aIds.IdOf(aEntry.first); });
	std::erase_if(boxes_, [&](const auto& aEntry) { return !aIds.IdOf(aEntry.first); });
	std::erase_if(capsules_, [&](const auto& aEntry) { return !aIds.IdOf(aEntry.first); });
}

const CapsuleSeen* NpcSizes::Capsule(uint64_t aEntity) const
{
	const auto found = capsules_.find(aEntity);
	return found != capsules_.end() ? &found->second : nullptr;
}

std::string DescribeNpc(const link::NpcSample& aNpc, uint32_t aId, double aDistance, const NpcSizes& aSizes)
{
	const auto         found = aSizes.Boxes().find(aNpc.entity);
	const link::NpcBox box = found != aSizes.Boxes().end() ? found->second : link::kPersonBox;
	std::string        from;
	if (const CapsuleSeen* capsule = aSizes.Capsule(aNpc.entity))
	{
		from = std::format("its capsule's: radius {:.2f}, half-height {:.2f}, leaning {:.0f} degrees, query filter {:x}:{:x}:{:x}:{:x}",
		                   capsule->radius, capsule->halfHeight, std::acos(std::min(Upright(*capsule), 1.0)) * 180.0 / std::numbers::pi,
		                   capsule->queryFilter[0], capsule->queryFilter[1], capsule->queryFilter[2], capsule->queryFilter[3]);
	}
	else
	{
		from = aSizes.LookedFor(aNpc.entity) ? "a person's: no capsule found" : "a person's until its capsule is looked for";
	}
	std::string flags;
	for (const auto& [flag, word] : { std::pair{ link::proto::kActorHostile, "hostile" }, std::pair{ link::proto::kActorEssential, "essential" },
	                                  std::pair{ link::proto::kActorInCombat, "in combat" } })
	{
		if (aNpc.flags & flag)
		{
			flags += (flags.empty() ? "" : ", ") + std::string(word);
		}
	}
	return std::format("{} {}, {:.1f} m: {:.2f} wide, from {:.2f} below its position to {:.2f} above, {}; health {:.0f} of {:.0f}, level "
	                   "{:.0f}; {}",
	                   aId, aNpc.name.empty() ? "(no name)" : aNpc.name, aDistance, box.width, box.below, box.above, from, aNpc.health,
	                   aNpc.healthMax, aNpc.level, flags.empty() ? "no flags" : flags);
}
}
```

- [x] **Step 6: Edit `src/link/Actors.h`**

```cpp
	void Forget(double aNow);
```
```cpp
	void Forget(double aNow);
	// aEntity's number, if it has one, without marking it listed.
	std::optional<uint32_t> IdOf(uint64_t aEntity) const;
```

- [x] **Step 7: Edit `src/link/Actors.cpp`**

```cpp
			++entry;
		}
	}
}

std::string CutName(std::string_view aName, size_t aBytes)
```
```cpp
			++entry;
		}
	}
}

std::optional<uint32_t> ActorIds::IdOf(uint64_t aEntity) const
{
	const auto found = byEntity_.find(aEntity);
	return found != byEntity_.end() ? std::optional<uint32_t>(found->second.id) : std::nullopt;
}

std::string CutName(std::string_view aName, size_t aBytes)
```

- [x] **Step 8: Edit `src/physx/Export.h`**

```cpp
#include "link/Ground.h"
```
```cpp
#include "link/Ground.h"
#include "physx/Characters.h"
```
```cpp
	// Scene 0 and PhysX's origin this frame, as the reads have them, for part 3's bodies: read once a frame, whoever asks
```
```cpp
	// The capsules of kinematic actors within aRadius of aWorld (world axes), for NPCs' sizes (spike 6), read under scene
	// 0's read lock through the same checks. None if scene 0 or PhysX's origin can't be had this frame.
	std::vector<CapsuleSeen> Capsules(const Vec3d& aWorld, double aRadius);

	// Scene 0 and PhysX's origin this frame, as the reads have them, for part 3's bodies: read once a frame, whoever asks
```

- [x] **Step 9: Edit `src/physx/Export.cpp`**

`Capsules` reads as `Describe` and `Origin` do: under scene 0's read lock, each actor and shape checked against the
audit before anything is called on it, and only the calls the audit lists (the overlap, the actor's global pose and
flags, the shape's geometry, local pose and query filter). A capsule's length runs along its shape's x axis.

```cpp
	return lines;
}
}
```
```cpp
	return lines;
}

std::vector<CapsuleSeen> PhysxGround::Capsules(const Vec3d& aWorld, double aRadius)
{
	std::vector<CapsuleSeen> seen;
	if (!Prepare())
	{
		return seen;
	}
	ReadableMemory&    memory = session_.Memory();
	const ModuleBases& bases = session_.Bases();
	const Vec3d        origin = *report_.origin;
	const PxVec3       at(float(aWorld.x - origin.x), float(aWorld.y - origin.y), float(aWorld.z - origin.z));
	scene_->lockRead();
	for (const PxOverlapHit& hit : Overlap(*scene_, PxSphereGeometry(float(aRadius)), PxTransform(at), PxQueryFlag::eDYNAMIC))
	{
		const ObjectCheck actor = CheckObject(memory, bases, hit.actor, kActorBytes);
		const ObjectCheck shape = CheckObject(memory, bases, hit.shape);
		PxCapsuleGeometry capsule;
		if (actor.verdict != ObjectVerdict::Ok || actor.cls != AuditedClass::RigidDynamic || shape.verdict != ObjectVerdict::Ok ||
		    shape.cls != AuditedClass::Shape ||
		    !static_cast<const PxRigidBody&>(*hit.actor).getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC) ||
		    hit.shape->getGeometryType() != PxGeometryType::eCAPSULE || !hit.shape->getCapsuleGeometry(capsule))
		{
			continue;
		}
		const PxTransform  pose = hit.actor->getGlobalPose() * hit.shape->getLocalPose();
		const PxVec3       axis = pose.q.getBasisVector0();  // a capsule's length runs along its x axis
		const PxFilterData filter = hit.shape->getQueryFilterData();
		seen.push_back({ { pose.p.x + origin.x, pose.p.y + origin.y, pose.p.z + origin.z },
		                 { axis.x, axis.y, axis.z },
		                 capsule.radius,
		                 capsule.halfHeight,
		                 { filter.word0, filter.word1, filter.word2, filter.word3 } });
	}
	scene_->unlockRead();
	return seen;
}
}
```

- [x] **Step 10: Edit `CMakeLists.txt` to add the source**

```cmake
	src/physx/BodyPlan.cpp
	src/link/Drive.cpp
```
```cmake
	src/physx/BodyPlan.cpp
	src/physx/Characters.cpp
	src/link/Drive.cpp
```

- [x] **Step 11: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `412 test(s), 0 failure(s)`.

- [x] **Step 12: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: NPCs' sizes from their capsules in PhysX" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 6: The redscript module, compiled offline

The game's side of the NPC list (the spec's "The module's list"), as a redscript file the plugin adds to redscript's
compile (Task 9):
- **`BlockRenderNpcs`,** a scriptable system: the game makes one of each script class extending `ScriptableSystem` per
  game session, as it does its own, and a load makes a new one; step 2's opening check confirms it does for a mod's.
  Its static `Current()` finds the session's through the game's global `GetGameInstance()`, so the plugin needs no
  `GameInstance` of its own and picks up a load's new system at once. `Version()` says which BlockRender it's
  for: 1, Part 1's.
- **The wraps:** `NPCPuppet.OnGameAttached` adds each NPC the game attaches, people, drones, mechs and animals alike,
  crowds included, and `OnDetach` takes it off. `NPCPuppet` has no subclasses in the game's scripts. An NPC whose weak
  reference has gone is dropped too.
- **`List(range)`** fills parallel arrays with the NPCs within `range` metres of V, straight-line, that aren't dead, and
  returns how many, or -1 without V: their `EntityID`s, positions and forward vectors, read every call; and their
  health and max health, `PowerLevel`, display name and flags, read at most four times a second an NPC, by the game's
  sim time. Each NPC's first read puts its next one at a random point of the quarter second after it, so a crowd isn't
  read all in one frame. The flags are SkyCraft's: hostile (1) when its attitude towards V is `AIA_Hostile`, essential
  (4) with an `Immortal` or `Invulnerable` god mode, in combat (8) by `NPCPuppet.IsInCombat`.
- **V's punch and max health** come with the list, also four times a second: the damage per hit
  (`EffectiveDamagePerHit`) of the item in V's `BaseFists` area, and the item's name, so step 2 can compare it with the
  number the inventory shows before Part 2 uses it.
- **`Compare(range)`,** for the `npcs` request: the NPCs the game's targeting finds within `range` (`GetTargetParts` with
  `TSF_NPC()` over the complete set, as the game's own effectors search for nearby NPCs), and which of them the wraps'
  list lacks, with their names and distances.

`tools/check_reds.py` compiles it against a copy of the game's untouched `r6/cache/final.redscripts`, through
redscript's compiler library in the game's `engine/tools` with its error popup off, so a slip shows here, not as a
message box at the game's start. It reads the game's files and changes none; its work goes in `build/reds-check`.

**Files:**
- Create: `scripts/BlockRender.reds`, `tools/check_reds.py`

- [x] **Step 1: Write `tools/check_reds.py`**

```python
"""Compiles BlockRender's redscript module against the game's own scripts, without starting the game.

    uv run --no-project python tools/check_reds.py <game folder> <scripts folder>

It loads redscript's compiler library from the game (engine/tools/scc_lib.dll), with its error popup off, and compiles
the scripts folder against a copy of the game's untouched r6/cache/final.redscripts, so the game's own files aren't
touched. It prints "compiled" and exits 0, or prints redscript's errors and exits 1. Its work goes in build/reds-check.
The game must be installed; it needn't run, and may.
"""
import ctypes
import shutil
import sys
from pathlib import Path

if len(sys.argv) != 3:
    sys.exit(__doc__)
game, scripts = Path(sys.argv[1]), Path(sys.argv[2]).resolve()
work = Path("build").resolve() / "reds-check"
r6 = work / "r6"
for sub in ("scripts", "cache", "logs"):
    (r6 / sub).mkdir(parents=True, exist_ok=True)
blob = r6 / "cache" / "final.redscripts"
shutil.copyfile(game / "r6" / "cache" / "final.redscripts", blob)
out = work / "out.redscripts"
out.unlink(missing_ok=True)

lib = ctypes.CDLL(str(game / "engine" / "tools" / "scc_lib.dll"))
lib.scc_settings_new.restype = ctypes.c_void_p
lib.scc_settings_new.argtypes = [ctypes.c_char_p]
for name in ("scc_settings_set_custom_cache_file", "scc_settings_set_output_cache_file", "scc_settings_add_script_path"):
    getattr(lib, name).argtypes = [ctypes.c_void_p, ctypes.c_char_p]
lib.scc_settings_disable_error_popup.argtypes = [ctypes.c_void_p]
lib.scc_compile.restype = ctypes.c_void_p
lib.scc_compile.argtypes = [ctypes.c_void_p]
lib.scc_get_success.restype = ctypes.c_void_p
lib.scc_get_success.argtypes = [ctypes.c_void_p]
lib.scc_copy_error.restype = ctypes.c_size_t
lib.scc_copy_error.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]
lib.scc_free_result.argtypes = [ctypes.c_void_p]

settings = lib.scc_settings_new(str(r6).encode())
lib.scc_settings_set_custom_cache_file(settings, str(blob).encode())
lib.scc_settings_set_output_cache_file(settings, str(out).encode())
lib.scc_settings_add_script_path(settings, str(scripts).encode())
lib.scc_settings_disable_error_popup(settings)
result = lib.scc_compile(settings)
ok = bool(lib.scc_get_success(result)) and out.exists()
error = ctypes.create_string_buffer(65536)
size = lib.scc_copy_error(result, error, len(error))
lib.scc_free_result(result)
if ok:
    print("compiled")
else:
    print("failed")
    print(error.raw[:size].decode("utf-8", "replace"))
sys.exit(0 if ok else 1)
```

- [x] **Step 2: Write `scripts/BlockRender.reds`**

```swift
// BlockRender's redscript module (spike 6, docs/superpowers/specs/2026-10-05-npcs-and-combat-design.md): the game's side
// of Night City's NPCs in Minecraft. BlockRender, a RED4ext plugin, adds this file to redscript's compile at the game's
// start; each frame, on the main thread, it finds the game session's BlockRenderNpcs through Current(), calls List, and
// reads the fields List fills by name, each field's type checked once. tools/check_reds.py compiles it offline.

// One NPC the game has attached, and what was last read of it.
public class BlockRenderNpc {
  public let npc: wref<NPCPuppet>;
  public let id: EntityID;
  public let health: Float;
  public let healthMax: Float;
  public let level: Float;
  public let name: String;
  public let flags: Int32;
  public let readAt: Float;
  public let read: Bool;
}

public class BlockRenderNpcs extends ScriptableSystem {
  // Every NPC the game has attached and not yet detached, kept by the wraps at the end of this file.
  private let known: array<ref<BlockRenderNpc>>;
  private let vReadAt: Float;
  private let vRead: Bool;

  // What List fills, one entry for each NPC listed, for BlockRender to read by name.
  public let npcIds: array<EntityID>;
  public let npcPositions: array<Vector4>;
  public let npcForwards: array<Vector4>;
  public let npcHealth: array<Float>;
  public let npcHealthMax: array<Float>;
  public let npcLevels: array<Float>;
  public let npcNames: array<String>;
  public let npcFlags: array<Int32>;
  // NPCs in the world: attached, and not detached.
  public let attached: Int32;
  // V's punch, the damage per hit of what V punches with (the item in the BaseFists area), and that item; and V's max
  // health. Read at most four times a second.
  public let punch: Float;
  public let punchItem: String;
  public let vHealthMax: Float;

  // The module's version, which BlockRender checks: 1 is Part 1's.
  public final static func Version() -> Int32 {
    return 1;
  }

  public final static func Get(game: GameInstance) -> ref<BlockRenderNpcs> {
    let container: ref<ScriptableSystemsContainer> = GameInstance.GetScriptableSystemsContainer(game);
    if !IsDefined(container) {
      return null;
    };
    return container.Get(n"BlockRenderNpcs") as BlockRenderNpcs;
  }

  // The game session's system, for BlockRender, which has no GameInstance to pass.
  public final static func Current() -> ref<BlockRenderNpcs> {
    return BlockRenderNpcs.Get(GetGameInstance());
  }

  public final func Attach(npc: ref<NPCPuppet>) -> Void {
    let id: EntityID = npc.GetEntityID();
    let i: Int32 = 0;
    while i < ArraySize(this.known) {
      if this.known[i].id == id {
        this.known[i].npc = npc;
        return;
      };
      i += 1;
    };
    let entry: ref<BlockRenderNpc> = new BlockRenderNpc();
    entry.npc = npc;
    entry.id = id;
    ArrayPush(this.known, entry);
  }

  public final func Detach(npc: ref<NPCPuppet>) -> Void {
    let id: EntityID = npc.GetEntityID();
    let i: Int32 = 0;
    while i < ArraySize(this.known) {
      if this.known[i].id == id {
        ArrayErase(this.known, i);
        return;
      };
      i += 1;
    };
  }

  // Fills the fields above with the NPCs within range metres of V, straight-line, that aren't dead, and returns how many;
  // -1 if V isn't found. An NPC's position and forward vector are read every call, the rest at most four times a second.
  public final func List(range: Float) -> Int32 {
    let game: GameInstance = this.GetGameInstance();
    let player: ref<GameObject> = GameInstance.GetPlayerSystem(game).GetLocalPlayerMainGameObject();
    ArrayClear(this.npcIds);
    ArrayClear(this.npcPositions);
    ArrayClear(this.npcForwards);
    ArrayClear(this.npcHealth);
    ArrayClear(this.npcHealthMax);
    ArrayClear(this.npcLevels);
    ArrayClear(this.npcNames);
    ArrayClear(this.npcFlags);
    if !IsDefined(player) {
      return -1;
    };
    let now: Float = EngineTime.ToFloat(GameInstance.GetSimTime(game));
    let v: Vector4 = player.GetWorldPosition();
    if !this.vRead || now - this.vReadAt >= 0.25 {
      this.ReadV(player, game);
      this.vRead = true;
      this.vReadAt = now;
    };
    let i: Int32 = 0;
    while i < ArraySize(this.known) {
      let entry: ref<BlockRenderNpc> = this.known[i];
      let npc: ref<NPCPuppet> = entry.npc;
      if !IsDefined(npc) {
        // Gone without a detach: dropped.
        ArrayErase(this.known, i);
      } else {
        let position: Vector4 = npc.GetWorldPosition();
        if Vector4.Distance(position, v) <= range && !npc.IsDead() {
          if !entry.read || now - entry.readAt >= 0.25 {
            this.ReadNpc(entry, npc, player, game);
            // The first read spreads the later ones over the quarter second, so a crowd isn't read all in one frame.
            entry.readAt = entry.read ? now : now - RandRangeF(0.00, 0.25);
            entry.read = true;
          };
          ArrayPush(this.npcIds, entry.id);
          ArrayPush(this.npcPositions, position);
          ArrayPush(this.npcForwards, npc.GetWorldForward());
          ArrayPush(this.npcHealth, entry.health);
          ArrayPush(this.npcHealthMax, entry.healthMax);
          ArrayPush(this.npcLevels, entry.level);
          ArrayPush(this.npcNames, entry.name);
          ArrayPush(this.npcFlags, entry.flags);
        };
        i += 1;
      };
    };
    this.attached = ArraySize(this.known);
    return ArraySize(this.npcIds);
  }

  // For the npcs request: the NPCs the game's targeting finds within range metres of V, searched as the game's own
  // effectors search for NPCs, against the wraps' list.
  public final func Compare(range: Float) -> String {
    let game: GameInstance = this.GetGameInstance();
    let player: ref<GameObject> = GameInstance.GetPlayerSystem(game).GetLocalPlayerMainGameObject();
    if !IsDefined(player) {
      return "V isn't found";
    };
    let query: TargetSearchQuery;
    query.testedSet = TargetingSet.Complete;
    query.searchFilter = TSF_NPC();
    query.maxDistance = range;
    query.filterObjectByDistance = true;
    query.includeSecondaryTargets = false;
    query.ignoreInstigator = true;
    let parts: array<TS_TargetPartInfo>;
    GameInstance.GetTargetingSystem(game).GetTargetParts(player, query, parts);
    let found: array<EntityID>;
    let lacking: String = "";
    let lacks: Int32 = 0;
    let i: Int32 = 0;
    while i < ArraySize(parts) {
      let component: wref<TargetingComponent> = TS_TargetPartInfo.GetComponent(parts[i]);
      let npc: ref<NPCPuppet>;
      if IsDefined(component) {
        npc = component.GetEntity() as NPCPuppet;
      };
      if IsDefined(npc) && !npc.IsDead() && !ArrayContains(found, npc.GetEntityID()) {
        ArrayPush(found, npc.GetEntityID());
        if !this.Knows(npc.GetEntityID()) {
          lacks += 1;
          lacking += (lacks > 1 ? "; " : "") + npc.GetDisplayName() + " at " + FloatToStringPrec(Vector4.Distance(npc.GetWorldPosition(), player.GetWorldPosition()), 1) + " m";
        };
      };
      i += 1;
    };
    return "GetTargetParts finds " + ToString(ArraySize(found)) + " NPCs within " + FloatToStringPrec(range, 0) + " m; the list lacks " + ToString(lacks) + (lacks > 0 ? ": " + lacking : "");
  }

  private final func Knows(id: EntityID) -> Bool {
    let i: Int32 = 0;
    while i < ArraySize(this.known) {
      if this.known[i].id == id {
        return true;
      };
      i += 1;
    };
    return false;
  }

  private final func ReadNpc(entry: ref<BlockRenderNpc>, npc: ref<NPCPuppet>, player: ref<GameObject>, game: GameInstance) -> Void {
    let stats: StatsObjectID = Cast<StatsObjectID>(entry.id);
    let pools: ref<StatPoolsSystem> = GameInstance.GetStatPoolsSystem(game);
    let godModes: ref<GodModeSystem> = GameInstance.GetGodModeSystem(game);
    entry.health = pools.GetStatPoolValue(stats, gamedataStatPoolType.Health, false);
    entry.healthMax = pools.GetStatPoolMaxPointValue(stats, gamedataStatPoolType.Health);
    entry.level = GameInstance.GetStatsSystem(game).GetStatValue(stats, gamedataStatType.PowerLevel);
    entry.name = npc.GetDisplayName();
    // SkyCraft's flags: hostile 1, essential 4, in combat 8.
    entry.flags = 0;
    if Equals(GameObject.GetAttitudeTowards(npc, player), EAIAttitude.AIA_Hostile) {
      entry.flags += 1;
    };
    if godModes.HasGodMode(entry.id, gameGodModeType.Immortal) || godModes.HasGodMode(entry.id, gameGodModeType.Invulnerable) {
      entry.flags += 4;
    };
    if NPCPuppet.IsInCombat(npc) {
      entry.flags += 8;
    };
  }

  private final func ReadV(player: ref<GameObject>, game: GameInstance) -> Void {
    this.vHealthMax = GameInstance.GetStatPoolsSystem(game).GetStatPoolMaxPointValue(Cast<StatsObjectID>(player.GetEntityID()), gamedataStatPoolType.Health);
    this.punch = 0.00;
    this.punchItem = "nothing in BaseFists";
    let items: array<ItemID> = EquipmentSystem.GetItemsInArea(player, gamedataEquipmentArea.BaseFists);
    if ArraySize(items) > 0 {
      this.punchItem = TDBID.ToStringDEBUG(ItemID.GetTDBID(items[0]));
      let data: wref<gameItemData> = GameInstance.GetTransactionSystem(game).GetItemData(player, items[0]);
      if IsDefined(data) {
        this.punch = data.GetStatValueByType(gamedataStatType.EffectiveDamagePerHit);
      } else {
        this.punchItem += ", without item data";
      };
    };
  }
}

// Every NPC the game puts in the world, people, drones, mechs and animals alike, crowds included: NPCPuppet has no
// subclasses in the game's scripts.
@wrapMethod(NPCPuppet)
protected cb func OnGameAttached() -> Bool {
  let result: Bool = wrappedMethod();
  let npcs: ref<BlockRenderNpcs> = BlockRenderNpcs.Get(this.GetGame());
  if IsDefined(npcs) {
    npcs.Attach(this);
  };
  return result;
}

@wrapMethod(NPCPuppet)
protected cb func OnDetach() -> Bool {
  let npcs: ref<BlockRenderNpcs> = BlockRenderNpcs.Get(this.GetGame());
  if IsDefined(npcs) {
    npcs.Detach(this);
  };
  return wrappedMethod();
}
```

- [x] **Step 3: Compile the module**

The game must be installed at `E:\Cyberpunk2077`; it needn't run.
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && uv run --no-project python tools/check_reds.py E:/Cyberpunk2077 scripts; \
  echo "exit: $?"
```
Expected: redscript's `Compilation complete`, then `compiled` and `exit: 0`.

- [x] **Step 4: Check that the compile catches a slip**

A copy of the module calling a function `StatPoolsSystem` doesn't have:
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/reds-slip \
  && sed 's/GetStatPoolMaxPointValue(stats/GetStatPoolMaxPointValueX(stats/' scripts/BlockRender.reds > build/reds-slip/BlockRender.reds \
  && uv run --no-project python tools/check_reds.py E:/Cyberpunk2077 build/reds-slip; echo "exit: $?"
```
Expected: `method 'GetStatPoolMaxPointValueX' not found on 'StatPoolsSystem'`, then `failed` and `exit: 1`.

- [x] **Step 5: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: the redscript module lists the NPCs around V, and compiles offline" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 7: `GameScript` calls the module from C++

The module's calls, as `Puppet` makes the game's (the spec's "What crosses to C++" and "Safety"):
- **Checked once,** the first time the plugin asks, and logged: the class `BlockRenderNpcs` (missing: redscript didn't
  compile the module, and its log says why); its version, which must be 1; its four functions, each picked by its
  parameters' and return types (`Version` and `Current` static, `List` and `Compare` taking a `Float`); and the twelve
  fields `List` fills, each by its type (`array:entEntityID`, `array:Vector4`, `array:Float`, `array:String`,
  `array:Int32`, `Int32`, `Float`, `String`). Anything missing keeps NPCs off, and says why.
- **Each call** finds the session's system through `Current()`, checks it's a `BlockRenderNpcs`, calls `List`, and
  reads the arrays straight from the system's fields, which must all hold as many entries as `List` returned.
- **The type checks** `Puppet` made in its own file move to `ScriptTypes`, unchanged but for `Matches` taking the
  parameter and return types rather than `Puppet`'s signature, so both use them.

Only the game can run this: Task 11's first check is the log's `module: found BlockRenderNpcs version 1, …` line.

**Files:**
- Create: `src/ScriptTypes.h`, `src/ScriptTypes.cpp`, `src/GameScript.h`, `src/GameScript.cpp`
- Modify: `src/Puppet.cpp`, `CMakeLists.txt`

- [x] **Step 1: Write `src/ScriptTypes.h`**

```cpp
#pragma once

#include <string>
#include <vector>

namespace RED4ext
{
struct CBaseFunction;
namespace rtti
{
struct IType;
}
}

// The game's script functions and fields, checked by their types' names before BlockRender calls or reads them (Puppet,
// GameScript): a call with other types than a function takes, or a field read as the wrong type, would fault, and the
// game freezes on any fault.
namespace cpmc
{
// Whether aType is named aAccepted; a name ending in '*' takes any type whose name starts with the rest.
bool TypeIs(const RED4ext::rtti::IType* aType, const char* aAccepted);
// Whether aFunction takes exactly as many parameters as aParams, each of one of the types named for it, and returns one
// of aReturns, or nothing when aReturns is empty.
bool Matches(const RED4ext::CBaseFunction& aFunction, const std::vector<std::vector<const char*>>& aParams,
             const std::vector<const char*>& aReturns);
// A type's name, or "?".
std::string TypeName(const RED4ext::rtti::IType* aType);
// "(handle:gameObject, Vector4) -> Bool", for the log.
std::string Describe(const RED4ext::CBaseFunction& aFunction);
}
```

- [x] **Step 2: Write `src/ScriptTypes.cpp`**

```cpp
#include "ScriptTypes.h"

#include <RED4ext/RED4ext.hpp>

#include <algorithm>
#include <string_view>

namespace cpmc
{
bool TypeIs(const RED4ext::rtti::IType* aType, const char* aAccepted)
{
	if (!aType)
	{
		return false;
	}
	const std::string_view accepted(aAccepted);
	if (!accepted.ends_with('*'))
	{
		return aType->GetName() == RED4ext::CName(aAccepted);
	}
	const char* name = aType->GetName().ToString();
	return name && std::string_view(name).starts_with(accepted.substr(0, accepted.size() - 1));
}

bool Matches(const RED4ext::CBaseFunction& aFunction, const std::vector<std::vector<const char*>>& aParams,
             const std::vector<const char*>& aReturns)
{
	if (aFunction.params.Size() != aParams.size())
	{
		return false;
	}
	for (uint32_t i = 0; i < aFunction.params.Size(); ++i)
	{
		const RED4ext::CProperty* param = aFunction.params[i];
		const auto&               accepted = aParams[i];
		if (!param || std::none_of(accepted.begin(), accepted.end(), [&](const char* aName) { return TypeIs(param->type, aName); }))
		{
			return false;
		}
	}
	if (aReturns.empty())
	{
		return aFunction.returnType == nullptr;
	}
	return aFunction.returnType &&
	       std::any_of(aReturns.begin(), aReturns.end(), [&](const char* aName) { return TypeIs(aFunction.returnType->type, aName); });
}

std::string TypeName(const RED4ext::rtti::IType* aType)
{
	const char* name = aType ? aType->GetName().ToString() : nullptr;
	return name ? name : "?";
}

std::string Describe(const RED4ext::CBaseFunction& aFunction)
{
	std::string out = "(";
	for (uint32_t i = 0; i < aFunction.params.Size(); ++i)
	{
		const RED4ext::CProperty* param = aFunction.params[i];
		out += (i == 0 ? "" : ", ") + TypeName(param ? param->type : nullptr);
	}
	return out + ")" + (aFunction.returnType ? " -> " + TypeName(aFunction.returnType->type) : "");
}
}
```

- [x] **Step 3: Edit `src/Puppet.cpp`: the checks move to `ScriptTypes`**

```cpp
#include "Puppet.h"
```
```cpp
#include "Puppet.h"

#include "ScriptTypes.h"
```
```cpp
#include <algorithm>
#include <cmath>
#include <format>
#include <string_view>
```
```cpp
#include <cmath>
#include <format>
```
```cpp
}

bool TypeIs(const RED4ext::rtti::IType* aType, const char* aAccepted)
{
	if (!aType)
	{
		return false;
	}
	const std::string_view accepted(aAccepted);
	if (!accepted.ends_with('*'))
	{
		return aType->GetName() == CName(aAccepted);
	}
	const char* name = aType->GetName().ToString();
	return name && std::string_view(name).starts_with(accepted.substr(0, accepted.size() - 1));
}

bool Matches(const RED4ext::CBaseFunction& aFunction, const Signature& aSignature)
{
	if (aFunction.params.Size() != aSignature.params.size())
	{
		return false;
	}
	for (uint32_t i = 0; i < aFunction.params.Size(); ++i)
	{
		const RED4ext::CProperty* param = aFunction.params[i];
		const auto&               accepted = aSignature.params[i];
		if (!param || std::none_of(accepted.begin(), accepted.end(), [&](const char* aName) { return TypeIs(param->type, aName); }))
		{
			return false;
		}
	}
	if (aSignature.returns.empty())
	{
		return aFunction.returnType == nullptr;
	}
	return aFunction.returnType && std::any_of(aSignature.returns.begin(), aSignature.returns.end(), [&](const char* aName) {
		       return TypeIs(aFunction.returnType->type, aName);
	       });
}

std::string TypeName(const RED4ext::rtti::IType* aType)
{
	const char* name = aType ? aType->GetName().ToString() : nullptr;
	return name ? name : "?";
}

// "(handle:gameObject, Vector4) -> Bool", for the log.
std::string Describe(const RED4ext::CBaseFunction& aFunction)
{
	std::string out = "(";
	for (uint32_t i = 0; i < aFunction.params.Size(); ++i)
	{
		const RED4ext::CProperty* param = aFunction.params[i];
		out += (i == 0 ? "" : ", ") + TypeName(param ? param->type : nullptr);
	}
	return out + ")" + (aFunction.returnType ? " -> " + TypeName(aFunction.returnType->type) : "");
```
```cpp
```
```cpp
		if (Matches(*aCandidate, signature))
```
```cpp
		if (Matches(*aCandidate, signature.params, signature.returns))
```

- [x] **Step 4: Write `src/GameScript.h`**

```cpp
#pragma once

#include "LogQueue.h"
#include "link/Actors.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace RED4ext
{
struct CBaseFunction;
struct CClass;
struct CProperty;
}

// BlockRender's redscript module, scripts/BlockRender.reds (spike 6's "The module's list"), called from C++. Its class,
// BlockRenderNpcs, is checked once, the first time it's needed: its version; its functions, picked by their types as
// Puppet's are; and the fields List fills, by their types. Its system is the game session's, found through its static
// Current() on every call, since a load makes a new one. Main thread only, while V is found.
namespace cpmc
{
constexpr int32_t kModuleVersion = 1;  // BlockRenderNpcs.Version(), as this BlockRender needs it

// What the module's List gives.
struct NpcList
{
	std::vector<link::NpcSample> npcs;          // within the range asked for, and not dead
	int32_t                      attached = 0;  // NPCs in the world
	float                        punch = 0.0f;  // V's punch, read from punchItem
	std::string                  punchItem;
	float                        vHealthMax = 0.0f;
};

class GameScript
{
public:
	explicit GameScript(LogQueue& aLog) : log_(aLog) {}

	// The NPCs within aRange metres of V. Nothing, with aWhy saying why, if the module can't be called or read.
	std::optional<NpcList> List(double aRange, std::string& aWhy);
	// What the game's targeting finds within aRange metres of V, and which of those the list lacks: for the npcs request.
	std::string Compare(double aRange);

private:
	enum Function : size_t
	{
		kVersion,
		kCurrent,
		kList,
		kCompare,
		kFunctions,
	};

	enum Field : size_t
	{
		kIds,
		kPositions,
		kForwards,
		kHealth,
		kHealthMax,
		kLevels,
		kNames,
		kFlags,
		kAttached,
		kPunch,
		kPunchItem,
		kVHealthMax,
		kFields,
	};

	// Checks the class, its version, functions and fields the first time, and logs what it found. Empty: all there.
	const std::string& Check();

	LogQueue&                                       log_;
	bool                                            checked_ = false;
	std::string                                     missing_;  // why the module can't be used; empty: it can
	RED4ext::CClass*                                class_ = nullptr;
	std::array<RED4ext::CBaseFunction*, kFunctions> functions_{};
	std::array<RED4ext::CProperty*, kFields>        fields_{};
};
}
```

- [x] **Step 5: Write `src/GameScript.cpp`**

```cpp
#include "GameScript.h"

#include "ScriptTypes.h"

#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/Generated/Vector4.hpp>
#include <RED4ext/Scripting/Natives/Generated/ent/EntityID.hpp>

#include <format>
#include <initializer_list>
#include <utility>

namespace cpmc
{
namespace
{
using RED4ext::CName;
using Strong = RED4ext::Handle<RED4ext::IScriptable>;

constexpr const char* kClassName = "BlockRenderNpcs";

// A function as BlockRender calls it: static or not, and its parameters' and return types, as scripts/BlockRender.reds
// declares them.
struct FunctionTypes
{
	const char*                           name;
	bool                                  isStatic;
	std::vector<std::vector<const char*>> params;
	std::vector<const char*>              returns;
};

// In GameScript::Function's order.
const std::vector<FunctionTypes>& Functions()
{
	static const std::vector<FunctionTypes> kFunctionTypes{
		{ "Version", true, {}, { "Int32" } },
		{ "Current", true, {}, { "handle:BlockRenderNpcs" } },
		{ "List", false, { { "Float" } }, { "Int32" } },
		{ "Compare", false, { { "Float" } }, { "String" } },
	};
	return kFunctionTypes;
}

// In GameScript::Field's order: the fields List fills, and their types.
const std::vector<std::pair<const char*, const char*>>& Fields()
{
	static const std::vector<std::pair<const char*, const char*>> kFieldTypes{
		{ "npcIds", "array:entEntityID" }, { "npcPositions", "array:Vector4" }, { "npcForwards", "array:Vector4" },
		{ "npcHealth", "array:Float" },    { "npcHealthMax", "array:Float" },   { "npcLevels", "array:Float" },
		{ "npcNames", "array:String" },    { "npcFlags", "array:Int32" },       { "attached", "Int32" },
		{ "punch", "Float" },              { "punchItem", "String" },           { "vHealthMax", "Float" },
	};
	return kFieldTypes;
}

// A static function's call: no instance.
constexpr void* kNoInstance = nullptr;

// aClass's function aTypes names, with those types; null if there's none, with aOthers listing those it has by that name.
RED4ext::CBaseFunction* FindFunction(RED4ext::CClass& aClass, const FunctionTypes& aTypes, std::string& aOthers)
{
	const CName             name(aTypes.name);
	RED4ext::CBaseFunction* found = nullptr;
	const auto              consider = [&](RED4ext::CBaseFunction* aCandidate) {
		if (!aCandidate || found || aCandidate->shortName != name)
		{
			return;
		}
		if (Matches(*aCandidate, aTypes.params, aTypes.returns))
		{
			found = aCandidate;
		}
		else
		{
			aOthers += (aOthers.empty() ? "" : "; ") + Describe(*aCandidate);
		}
	};
	if (aTypes.isStatic)
	{
		for (RED4ext::CClassStaticFunction* function : aClass.staticFuncs)
		{
			consider(function);
		}
	}
	else
	{
		for (RED4ext::CClassFunction* function : aClass.funcs)
		{
			consider(function);
		}
	}
	return found;
}

// The game session's system, through the module's Current(); none, with aWhy saying why.
Strong SessionSystem(RED4ext::CBaseFunction& aCurrent, RED4ext::CClass& aClass, std::string& aWhy)
{
	Strong system;
	if (!RED4ext::ExecuteFunction(kNoInstance, &aCurrent, &system) || !system)
	{
		aWhy = "the game session has no BlockRenderNpcs system";
		return {};
	}
	RED4ext::CClass* const type = system->GetType();
	if (!type || !type->IsA(&aClass))
	{
		aWhy = "BlockRenderNpcs.Current() gave something else";
		return {};
	}
	return system;
}

// A field of the module's system, as the type its check found it to be.
template<class T>
const T& FieldOf(RED4ext::CProperty& aField, RED4ext::IScriptable& aSystem)
{
	return *aField.GetValuePtr<T>(&aSystem);
}
}

const std::string& GameScript::Check()
{
	if (checked_)
	{
		return missing_;
	}
	checked_ = true;
	const std::vector<FunctionTypes>&                         functions = Functions();
	const std::vector<std::pair<const char*, const char*>>& fields = Fields();
	auto* const                                               rtti = RED4ext::CRTTISystem::Get();
	class_ = rtti ? rtti->GetClass(CName(kClassName)) : nullptr;
	if (functions.size() != kFunctions || fields.size() != kFields)
	{
		missing_ = "GameScript's tables don't match its enums";
	}
	else if (!class_)
	{
		missing_ = "the game has no BlockRenderNpcs, so redscript didn't compile BlockRender.reds: r6/logs/redscript_rCURRENT.log says why";
	}
	for (size_t i = 0; missing_.empty() && i < kFunctions; ++i)
	{
		std::string others;
		functions_[i] = FindFunction(*class_, functions[i], others);
		if (!functions_[i])
		{
			missing_ = std::format("BlockRenderNpcs.{} isn't there with the types BlockRender passes{}", functions[i].name,
			                       others.empty() ? "" : "; the module has " + others);
		}
		else if (i == kVersion)
		{
			int32_t version = 0;
			if (!RED4ext::ExecuteFunction(kNoInstance, functions_[kVersion], &version) || version != kModuleVersion)
			{
				missing_ = std::format("BlockRender.reds is version {}, and this BlockRender needs version {}: they come from different builds",
				                       version, kModuleVersion);
			}
		}
	}
	for (size_t i = 0; missing_.empty() && i < kFields; ++i)
	{
		RED4ext::CProperty* const field = class_->GetProperty(CName(fields[i].first));
		if (!field || !TypeIs(field->type, fields[i].second))
		{
			missing_ = std::format("BlockRenderNpcs.{} isn't there as {}{}", fields[i].first, fields[i].second,
			                       field ? ": it's " + TypeName(field->type) : std::string());
		}
		fields_[i] = field;
	}
	log_.Push(missing_.empty() ? std::format("module: found BlockRenderNpcs version {}, its {} functions, and the {} fields List fills",
	                                         kModuleVersion, size_t(kFunctions), size_t(kFields))
	                           : "module: " + missing_);
	return missing_;
}

std::optional<NpcList> GameScript::List(double aRange, std::string& aWhy)
{
	if (const std::string& missing = Check(); !missing.empty())
	{
		aWhy = missing;
		return std::nullopt;
	}
	const Strong system = SessionSystem(*functions_[kCurrent], *class_, aWhy);
	if (!system)
	{
		return std::nullopt;
	}
	float                range = float(aRange);
	int32_t              count = 0;
	RED4ext::StackArgs_t args{ { nullptr, &range } };
	if (!RED4ext::ExecuteFunction(system.GetPtr(), functions_[kList], &count, args))
	{
		aWhy = "BlockRenderNpcs.List failed";
		return std::nullopt;
	}
	if (count < 0)
	{
		aWhy = "the module doesn't find V";
		return std::nullopt;
	}
	RED4ext::IScriptable& instance = *system.GetPtr();
	const auto&           ids = FieldOf<RED4ext::DynArray<RED4ext::ent::EntityID>>(*fields_[kIds], instance);
	const auto&           positions = FieldOf<RED4ext::DynArray<RED4ext::Vector4>>(*fields_[kPositions], instance);
	const auto&           forwards = FieldOf<RED4ext::DynArray<RED4ext::Vector4>>(*fields_[kForwards], instance);
	const auto&           health = FieldOf<RED4ext::DynArray<float>>(*fields_[kHealth], instance);
	const auto&           healthMax = FieldOf<RED4ext::DynArray<float>>(*fields_[kHealthMax], instance);
	const auto&           levels = FieldOf<RED4ext::DynArray<float>>(*fields_[kLevels], instance);
	const auto&           names = FieldOf<RED4ext::DynArray<RED4ext::CString>>(*fields_[kNames], instance);
	const auto&           flags = FieldOf<RED4ext::DynArray<int32_t>>(*fields_[kFlags], instance);
	const uint32_t        size = uint32_t(count);
	for (const uint32_t length : { ids.Size(), positions.Size(), forwards.Size(), health.Size(), healthMax.Size(), levels.Size(),
	                               names.Size(), flags.Size() })
	{
		if (length != size)
		{
			aWhy = "the module's lists disagree in length";
			return std::nullopt;
		}
	}
	NpcList list;
	list.npcs.resize(size);
	for (uint32_t i = 0; i < size; ++i)
	{
		link::NpcSample& npc = list.npcs[i];
		npc.entity = ids[i].hash;
		npc.position = { positions[i].X, positions[i].Y, positions[i].Z };
		npc.forward = { forwards[i].X, forwards[i].Y, forwards[i].Z };
		npc.health = health[i];
		npc.healthMax = healthMax[i];
		npc.level = levels[i];
		npc.name = names[i].c_str();
		npc.flags = uint32_t(flags[i]);
	}
	list.attached = FieldOf<int32_t>(*fields_[kAttached], instance);
	list.punch = FieldOf<float>(*fields_[kPunch], instance);
	list.punchItem = FieldOf<RED4ext::CString>(*fields_[kPunchItem], instance).c_str();
	list.vHealthMax = FieldOf<float>(*fields_[kVHealthMax], instance);
	return list;
}

std::string GameScript::Compare(double aRange)
{
	if (const std::string& missing = Check(); !missing.empty())
	{
		return missing;
	}
	std::string  why;
	const Strong system = SessionSystem(*functions_[kCurrent], *class_, why);
	if (!system)
	{
		return why;
	}
	float                range = float(aRange);
	RED4ext::CString     result;
	RED4ext::StackArgs_t args{ { nullptr, &range } };
	if (!RED4ext::ExecuteFunction(system.GetPtr(), functions_[kCompare], &result, args))
	{
		return "BlockRenderNpcs.Compare failed";
	}
	return result.c_str();
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the sources to the plugin**

```cmake
	src/Puppet.cpp
)
```
```cmake
	src/Puppet.cpp
	src/ScriptTypes.cpp
	src/GameScript.cpp
)
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `412 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: GameScript calls the module from C++, its functions and fields checked by type" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 8: The `npcs` request, and the status's NPC line

The spec's "Requests and status":
- **`npcs`** logs every listed NPC and the targeting comparison (Task 9 runs it); **`set npcs 0|1`** is an ordinary
  `set`, which the plugin handles in Task 9.
- **The NPC line,** after the bodies' line: on or off; how many are listed of how many are in the world; the nearest,
  with its distance; capsules found of those looked for; the numbers given since the start; the module's time a frame,
  on average and at worst over the last second; Minecraft's hits logged; and, while none can be listed, why.

**Files:**
- Modify: `src/Requests.h`, `src/Requests.cpp`, `src/Status.h`, `src/Status.cpp`, `tests/TestRequests.cpp`,
  `tests/TestStatus.cpp`

- [x] **Step 1: Edit `tests/TestRequests.cpp`: the failing test**

```cpp
	CHECK(!cpmc::ParseRequest("shapes all", error));
}
```
```cpp
	CHECK(!cpmc::ParseRequest("shapes all", error));
}

TEST(RequestsForTheNpcs)
{
	std::string error;
	const auto  npcs = cpmc::ParseRequest("npcs", error);
	CHECK(npcs && npcs->kind == cpmc::Request::Kind::Npcs);
	CHECK(!cpmc::ParseRequest("npcs all", error));
	CHECK_STR(error, "npcs takes nothing after it");
	const auto off = cpmc::ParseRequest("set npcs 0", error);
	CHECK(off && off->kind == cpmc::Request::Kind::Set && off->what == "npcs" && off->number == 0.0);
}
```

- [x] **Step 2: Edit `tests/TestStatus.cpp`: the failing test**

```cpp
	          "0 unreadable"));
}
```
```cpp
	          "0 unreadable"));
}

TEST(StatusWritesTheNpcs)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	report.link = link;
	const std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "npcs=on: 0 listed of 0 in the world; capsules found for 0 of 0 looked for; 0 numbers given; the module not "
	                  "called; 0 hits from Minecraft logged"));
	CHECK(status.find("bodies=") < status.find("npcs=") && status.find("npcs=") < status.find("ring="));
	link.npcsWhy = "the game has no BlockRenderNpcs, so redscript didn't compile BlockRender.reds";
	report.link = link;
	CHECK(cpmc::FormatStatus(report).find("; 0 hits from Minecraft logged; none listed: the game has no BlockRenderNpcs, so redscript "
	                                      "didn't compile BlockRender.reds") != std::string::npos);
	link.npcsWhy.clear();
	link.npcsListed = 12;
	link.npcsInWorld = 40;
	link.nearestNpc = 3.26;
	link.nearestNpcName = "Ganger One";
	link.capsulesFound = 10;
	link.capsulesLookedFor = 12;
	link.npcIdsGiven = 57;
	link.moduleMsAverage = 0.1234;
	link.moduleMsWorst = 0.4567;
	link.npcHits = 3;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "npcs=on: 12 listed of 40 in the world, the nearest Ganger One at 3.3 m; capsules found for 10 "
	                                      "of 12 looked for; 57 numbers given; the module 0.123 ms average, 0.457 ms worst a frame; 3 "
	                                      "hits from Minecraft logged"));
	link.npcsOn = false;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "npcs=off"));
}
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2065: 'Npcs': undeclared identifier`.

- [x] **Step 4: Edit `src/Requests.h`**

```cpp
//   box on|off                 (spike 5's step 4: a test box 3 m ahead of V, made as the bodies for blocks are)
```
```cpp
//   box on|off                 (spike 5's step 4: a test box 3 m ahead of V, made as the bodies for blocks are)
//   npcs                       (spike 6: every NPC listed, into the log, and what the game's targeting finds)
```
```cpp
		Shapes,
		Box,
	};
```
```cpp
		Shapes,
		Box,
		Npcs,
	};
```

- [x] **Step 5: Edit `src/Requests.cpp`**

```cpp
	if (command == "log" || command == "clear" || command == "reload" || command == "floor" || command == "shapes")
```
```cpp
	if (command == "log" || command == "clear" || command == "reload" || command == "floor" || command == "shapes" || command == "npcs")
```
```cpp
		                           : command == "shapes" ? Request::Kind::Shapes
```
```cpp
		                           : command == "shapes" ? Request::Kind::Shapes
		                           : command == "npcs"   ? Request::Kind::Npcs
```

- [x] **Step 6: Edit `src/Status.h`**

```cpp
	std::optional<Vec3d> box;               // the test box's centre, world axes, while it stands
```
```cpp
	std::optional<Vec3d> box;               // the test box's centre, world axes, while it stands
	// NPCs in Minecraft (spike 6's "Part 1"): the numbers given and the hits are since the start
	bool                  npcsOn = true;      // set npcs 1
	std::string           npcsWhy;            // why none are listed, while none can be
	size_t                npcsListed = 0;     // in SkyCraft's actor table
	int32_t               npcsInWorld = 0;    // the module's count of NPCs the game has attached
	std::optional<double> nearestNpc;         // metres; none: none listed
	std::string           nearestNpcName;
	size_t                capsulesFound = 0;  // of the NPCs known
	size_t                capsulesLookedFor = 0;
	uint32_t              npcIdsGiven = 0;
	double                moduleMsAverage = -1.0;  // the module's time a frame, in the last second; below 0: not called
	double                moduleMsWorst = -1.0;
	uint64_t              npcHits = 0;  // Minecraft's hits on them, logged
```
```cpp
// drive, positions, ground, solids, bodies, ring, atlas, hud, latency, burst and processes); then one structure line per
// structure:
```
```cpp
// drive, positions, ground, solids, bodies, npcs, ring, atlas, hud, latency, burst and processes); then one structure line
// per structure:
```

- [x] **Step 7: Edit `src/Status.cpp`**

```cpp
std::string FormatLink(const LinkReport& aLink)
```
```cpp
// NPCs in Minecraft: how many are listed, the nearest, their sizes, the module's time, and Minecraft's hits on them.
std::string FormatNpcs(const LinkReport& aLink)
{
	if (!aLink.npcsOn)
	{
		return "npcs=off\n";
	}
	std::string out = std::format("npcs=on: {} listed of {} in the world", aLink.npcsListed, aLink.npcsInWorld);
	if (aLink.nearestNpc)
	{
		out += std::format(", the nearest {} at {:.1f} m", aLink.nearestNpcName.empty() ? "(no name)" : OneLine(aLink.nearestNpcName),
		                   *aLink.nearestNpc);
	}
	out += std::format("; capsules found for {} of {} looked for; {} numbers given; the module {}; {} hits from Minecraft logged",
	                   aLink.capsulesFound, aLink.capsulesLookedFor, aLink.npcIdsGiven,
	                   aLink.moduleMsAverage < 0.0 ? std::string("not called") : Ms(aLink.moduleMsAverage, aLink.moduleMsWorst) + " a frame",
	                   aLink.npcHits);
	if (!aLink.npcsWhy.empty())
	{
		out += "; none listed: " + OneLine(aLink.npcsWhy);
	}
	return out + "\n";
}

std::string FormatLink(const LinkReport& aLink)
```
```cpp
	out += FormatBodies(aLink);
```
```cpp
	out += FormatBodies(aLink);
	out += FormatNpcs(aLink);
```

- [x] **Step 8: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `414 test(s), 0 failure(s)`.

- [x] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: the npcs request, and the status's NPC line" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 9: BlockRender 0.17.0 lists Night City's NPCs into SkyCraft's actor table

The spec's "The frame", "The table", "The link" and "Requests and status", in the plugin:
- **Each frame, after the link's update** (`ListNpcs`): while linked, with V found and `npcs` on, the module lists the
  NPCs within 80 m (`GameScript::List`, timed for the status), the nearest 256 are numbered and written into the table,
  and up to eight NPCs have their capsules looked for, those never looked for first, then those still without one 2 s
  after their last look, while scene 0 and PhysX's origin can be read this frame. Numbers unlisted for a minute are forgotten once a second, with their boxes. When any of that stops (unlinked,
  V not found, `set npcs 0`, the module missing), the table is emptied, once. `UpdateLink` now returns V's feet, which
  it already found, for this.
- **Hits are only logged** in Part 1, with the NPC's number and name as the table last had it, Minecraft's damage, the
  knockback and `HitWords`; Minecraft's player's death is logged with its killer's number.
- **`set npcs 0|1`,** and **`npcs`:** V's punch and max health with the item they're read from, a line per listed NPC
  (`DescribeNpc`), and the module's targeting comparison.
- **The status's NPC line** from all that.
- **At load,** the module's folder, `Scripts` in the plugin's folder, is added to redscript's compile
  (`Sdk::scripts->Add`), and the log says whether it was; the install copies `scripts/BlockRender.reds` there.
- **The version:** 0.17.0, in the log line and, from now on, in `Query` too, which takes its numbers from `kVersion`
  (it said 0.15.0 while the log said 0.16.0).

**Files:**
- Modify: `src/Plugin.cpp`, `CMakeLists.txt`

- [x] **Step 1: Edit `src/Plugin.cpp`**

```cpp
// for V to stand on.
// Commands come through blockrender.request, in its folder or the overlay's, and status.txt in the overlay's folder says
// what it's doing. See docs/superpowers/specs/2026-10-01-block-render-design.md, 2026-10-02-minecraft-link-design.md and
// 2026-10-03-collision-and-movement-design.md.
#include <RED4ext/RED4ext.hpp>

```
```cpp
// for V to stand on. Spike 6 lists Night City's NPCs into SkyCraft's actor table, through its redscript module
// (scripts/BlockRender.reds), so that Minecraft's weapons hit them and they press its pressure plates.
// Commands come through blockrender.request, in its folder or the overlay's, and status.txt in the overlay's folder says
// what it's doing. See docs/superpowers/specs/2026-10-01-block-render-design.md, 2026-10-02-minecraft-link-design.md,
// 2026-10-03-collision-and-movement-design.md and 2026-10-05-npcs-and-combat-design.md.
#include <RED4ext/RED4ext.hpp>

#include "GameScript.h"
```
```cpp
#include "input/Router.h"
```
```cpp
#include "input/Router.h"
#include "link/Actors.h"
```
```cpp
#include "physx/Bodies.h"
```
```cpp
#include "physx/Bodies.h"
#include "physx/Characters.h"
```
```cpp
constexpr const char* kVersion = "0.16.0";
```
```cpp
constexpr const char* kVersion = "0.17.0";

// A number of a version "major.minor.patch", aIndex 0 being the major, so that Query reports kVersion, as the log does.
constexpr uint32_t VersionNumber(std::string_view aVersion, size_t aIndex)
{
	uint32_t number = 0;
	size_t   index = 0;
	for (const char ch : aVersion)
	{
		if (ch == '.')
		{
			if (index == aIndex)
			{
				return number;
			}
			++index;
			number = 0;
		}
		else
		{
			number = number * 10 + uint32_t(ch - '0');
		}
	}
	return index == aIndex ? number : 0;
}
static_assert(VersionNumber("1.23.456", 0) == 1 && VersionNumber("1.23.456", 1) == 23 && VersionNumber("1.23.456", 2) == 456);
```
```cpp
// This frame's figures, for bursts.
```
```cpp
// Spike 6's NPCs in Minecraft (its spec's "Part 1"). Main thread only.
cpmc::GameScript                          g_script{ g_log };      // the redscript module's calls
bool                                      g_npcs = true;          // set npcs 0|1: on, since listing NPCs changes nothing in the game
cpmc::link::ActorIds                      g_actorIds;             // the NPCs' numbers in SkyCraft's actor table
cpmc::px::NpcSizes                        g_npcSizes;             // their boxes, from their capsules
std::optional<cpmc::NpcList>              g_npcList;              // the module's last list
std::optional<cpmc::Vec3d>                g_npcsFeet;             // V's feet when it was made
std::vector<skycraft::proto::ActorRecord> g_actorRecords;         // the table as last written, nearest first
std::vector<uint64_t>                     g_listedEntities;       // the NPC behind each of those records
std::string                               g_npcsWhy;              // why none are listed, while none can be
uint64_t                                  g_npcHits = 0;          // Minecraft's hits on them, logged
double                                    g_npcsForgotten = 0.0;  // when NPCs unlisted for a minute were last forgotten

// This frame's figures, for bursts.
```
```cpp
	uint64_t collisionBytesAtStart = 0;
```
```cpp
	uint64_t collisionBytesAtStart = 0;
	// The module's
	double   moduleMsSum = 0.0;
	double   moduleMsWorst = 0.0;
	uint64_t moduleCalls = 0;
```
```cpp
	void Hud(const cpmc::link::HudFrame& aFrame) override
```
```cpp
	// Spike 6's Part 1 only logs Minecraft's hits on NPCs (its spec's "The link"), and Minecraft's player's death.
	void HitActor(const skycraft::proto::McEvent& aEvent) override
	{
		++g_npcHits;
		const auto record = std::find_if(g_actorRecords.begin(), g_actorRecords.end(),
		                                 [&](const skycraft::proto::ActorRecord& aRecord) { return aRecord.formId == aEvent.formId; });
		::Log("hit on NPC {} ({}) for {:.2f} of Minecraft's damage, knockback {:.2f}; {}; not applied in Part 1", aEvent.formId,
		      record != g_actorRecords.end() ? std::string(record->name) : std::string("not listed now"), aEvent.a, aEvent.d,
		      cpmc::link::HitWords(aEvent));
	}

	void PlayerDied(const skycraft::proto::McEvent& aEvent) override
	{
		::Log("Minecraft's player died{}", aEvent.formId != 0 ? std::format(", killed by NPC {}", aEvent.formId) : std::string());
	}

	void Hud(const cpmc::link::HudFrame& aFrame) override
```
```cpp
// Minecraft sends to the sink. In build mode, V then goes where the link puts it (spike 5).
void UpdateLink(double aNow, double aFrameMs)
```
```cpp
// Minecraft sends to the sink. In build mode, V then goes where the link puts it (spike 5). V's feet, if V is found.
std::optional<cpmc::Vec3d> UpdateLink(double aNow, double aFrameMs)
```
```cpp
	if (!g_link)
	{
		return;
	}
```
```cpp
	if (!g_link)
	{
		return std::nullopt;
	}
```
```cpp
			Log("latency: Minecraft {:.0f} ms, ours {:.1f} ms ({:.0f} frames)", sample->minecraftMs, sample->oursMs, sample->oursFrames);
		}
	}
}
```
```cpp
			Log("latency: Minecraft {:.0f} ms, ours {:.1f} ms ({:.0f} frames)", sample->minecraftMs, sample->oursMs, sample->oursFrames);
		}
	}
	return feet;
}

// Spike 6's NPCs in Minecraft (its spec's "The frame", "The table" and "Sizes"), after the link's update: while linked
// with V found and npcs on, the module lists the NPCs within 80 m, the nearest 256 go into SkyCraft's actor table, and
// up to eight NPCs listed for the first time have their capsules looked for. When any of that stops, the table is
// emptied, once.
void ListNpcs(double aNow, const std::optional<cpmc::Vec3d>& aFeet)
{
	g_npcList.reset();
	g_npcsFeet = aFeet;
	if (!g_npcs || !g_link || !g_linked || !aFeet)
	{
		g_npcsWhy = !g_npcs ? "" : !g_link ? "the mapping isn't open" : !g_linked ? "not linked" : "V isn't found";
	}
	else
	{
		std::string  why;
		const double start = cpmc::SteadySeconds();
		g_npcList = g_script.List(cpmc::link::kActorRange, why);
		const double ms = (cpmc::SteadySeconds() - start) * 1000.0;
		g_npcsWhy = why;
		g_second.moduleMsSum += ms;
		g_second.moduleMsWorst = std::max(g_second.moduleMsWorst, ms);
		++g_second.moduleCalls;
	}
	if (!g_npcList)
	{
		g_actorRecords.clear();
		g_listedEntities.clear();
		if (g_link)
		{
			g_link->ClearActors();  // once: an empty table isn't written again
		}
		return;
	}
	g_actorRecords = cpmc::link::ActorRecords(g_npcList->npcs, *aFeet, g_npcSizes.Boxes(), g_actorIds, aNow);
	g_link->WriteActors(g_actorRecords);
	g_listedEntities.clear();
	for (const skycraft::proto::ActorRecord& record : g_actorRecords)
	{
		g_listedEntities.push_back(g_actorIds.Entity(record.formId, aNow).value_or(0));
	}
	// Capsules only while scene 0 and PhysX's origin can be read: an NPC looked for waits 2 s before it's looked for again.
	cpmc::Vec3d origin;
	if (g_ground.Prepared(origin))
	{
		for (const uint64_t entity : g_npcSizes.ToLookFor(g_listedEntities, aNow))
		{
			const auto npc = std::find_if(g_npcList->npcs.begin(), g_npcList->npcs.end(),
			                              [&](const cpmc::link::NpcSample& aNpc) { return aNpc.entity == entity; });
			if (npc == g_npcList->npcs.end())
			{
				continue;
			}
			const cpmc::Vec3d&                       feet = npc->position;
			const std::vector<cpmc::px::CapsuleSeen> seen =
			    g_ground.Capsules({ feet.x, feet.y, feet.z + cpmc::px::kCapsuleSearchHeight }, cpmc::px::kCapsuleSearchRadius);
			if (const std::optional<cpmc::px::CapsuleSeen> capsule = cpmc::px::PickCapsule(seen, feet))
			{
				g_npcSizes.Found(entity, *capsule, feet);
			}
		}
	}
	if (aNow - g_npcsForgotten >= 1.0)
	{
		g_npcsForgotten = aNow;
		g_actorIds.Forget(aNow);
		g_npcSizes.Forget(g_actorIds);
	}
}

// The NPC behind the table's record aIndex, as the module last listed it; null if there's none.
const cpmc::link::NpcSample* ListedNpc(size_t aIndex)
{
	if (!g_npcList || aIndex >= g_listedEntities.size())
	{
		return nullptr;
	}
	const auto npc = std::find_if(g_npcList->npcs.begin(), g_npcList->npcs.end(),
	                              [&](const cpmc::link::NpcSample& aNpc) { return aNpc.entity == g_listedEntities[aIndex]; });
	return npc != g_npcList->npcs.end() ? &*npc : nullptr;
}

// An NPC's distance from V's feet, straight-line, when it was listed.
double DistanceFromV(const cpmc::link::NpcSample& aNpc)
{
	const cpmc::Vec3d feet = g_npcsFeet.value_or(aNpc.position);
	return std::hypot(aNpc.position.x - feet.x, aNpc.position.y - feet.y, aNpc.position.z - feet.z);
}
```
```cpp
		return Report(std::format("set ao {}", g_meshOptions.ambientOcclusion ? 1 : 0));
```
```cpp
		return Report(std::format("set ao {}", g_meshOptions.ambientOcclusion ? 1 : 0));
	}
	if (aRequest.what == "npcs")
	{
		if (aRequest.number != 0.0 && aRequest.number != 1.0)
		{
			return Report("set npcs: 0 empties SkyCraft's actor table, and 1 lists the NPCs around V into it");
		}
		g_npcs = aRequest.number == 1.0;
		return Report(g_npcs ? "set npcs 1: the NPCs around V go into SkyCraft's actor table while linked"
		                     : "set npcs 0: SkyCraft's actor table is emptied, and Minecraft's stand-ins go");
```
```cpp
// box on|off (spike 5's step 4): a test box 3 m ahead of V, made as the bodies for blocks are, before any block is.
```
```cpp
// npcs (spike 6's "Requests and status"): every NPC listed, into the log, with its number, name, distance, box and where
// the box came from, health, level and flags; V's punch and max health as the module reads them; and how many NPCs the
// game's targeting finds within 80 m, with those the list lacks.
void NpcsRequest()
{
	if (!g_npcList)
	{
		return Report("npcs: none listed: " + (g_npcsWhy.empty() ? std::string("set npcs is 0") : g_npcsWhy));
	}
	Log("npcs: {} listed of {} in the world; V's punch {:.2f}, read from {}; V's max health {:.0f}", g_actorRecords.size(),
	    g_npcList->attached, g_npcList->punch, g_npcList->punchItem, g_npcList->vHealthMax);
	for (size_t i = 0; i < g_actorRecords.size(); ++i)
	{
		if (const cpmc::link::NpcSample* npc = ListedNpc(i))
		{
			Log("npcs: {}", cpmc::px::DescribeNpc(*npc, g_actorRecords[i].formId, DistanceFromV(*npc), g_npcSizes));
		}
	}
	Log("npcs: {}", g_script.Compare(cpmc::link::kActorRange));
	Report(std::format("npcs: {} NPCs in the log, and what the game's targeting finds", g_actorRecords.size()));
}

// box on|off (spike 5's step 4): a test box 3 m ahead of V, made as the bodies for blocks are, before any block is.
```
```cpp
	case cpmc::Request::Kind::Box: BoxRequest(request->what == "on"); break;
```
```cpp
	case cpmc::Request::Kind::Box: BoxRequest(request->what == "on"); break;
	case cpmc::Request::Kind::Npcs: NpcsRequest(); break;
```
```cpp
	report.box = bodies.box;
```
```cpp
	report.box = bodies.box;
	report.npcsOn = g_npcs;
	report.npcsWhy = g_npcsWhy;
	report.npcsListed = g_actorRecords.size();
	report.npcsInWorld = g_npcList ? g_npcList->attached : 0;
	if (const cpmc::link::NpcSample* nearest = ListedNpc(0))
	{
		report.nearestNpc = DistanceFromV(*nearest);
		report.nearestNpcName = nearest->name;
	}
	report.capsulesFound = g_npcSizes.FoundCount();
	report.capsulesLookedFor = g_npcSizes.LookedForCount();
	report.npcIdsGiven = g_actorIds.Given();
	report.moduleMsAverage = g_second.moduleCalls ? g_second.moduleMsSum / double(g_second.moduleCalls) : -1.0;
	report.moduleMsWorst = g_second.moduleCalls ? g_second.moduleMsWorst : -1.0;
	report.npcHits = g_npcHits;
```
```cpp
	UpdateLink(seconds, frameMs);
```
```cpp
	const std::optional<cpmc::Vec3d> feet = UpdateLink(seconds, frameMs);
	ListNpcs(seconds, feet);
```
```cpp
		aSdk->gameStates->Add(aHandle, RED4ext::EGameStateType::Running, &running);
```
```cpp
		aSdk->gameStates->Add(aHandle, RED4ext::EGameStateType::Running, &running);
		// Spike 6's redscript module, which redscript compiles with the game's scripts: the install puts it in Scripts.
		const std::filesystem::path module = g_pluginDir / "Scripts" / "BlockRender.reds";
		std::error_code             error;
		const bool                  present = std::filesystem::exists(module, error);
		const bool                  added = present && aSdk->scripts && aSdk->scripts->Add(aHandle, module.parent_path().c_str());
```
```cpp
		                                : std::string("nothing remembered, so build mode turns once V has turned 5 times"));
```
```cpp
		                                : std::string("nothing remembered, so build mode turns once V has turned 5 times"));
		Log("the redscript module {}: {}", module.string(),
		    added ? "added to redscript's compile" : present ? "NOT added to redscript's compile" : "missing, so NPCs can't be listed");
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(0, 15, 0);
```
```cpp
	aInfo->version = RED4EXT_V1_SEMVER(uint8_t(VersionNumber(kVersion, 0)), uint16_t(VersionNumber(kVersion, 1)), VersionNumber(kVersion, 2));
```

- [x] **Step 2: Edit `CMakeLists.txt`: version 0.17.0, and the module installed**

```cmake
project(BlockRender VERSION 0.16.0 LANGUAGES C CXX)
```
```cmake
project(BlockRender VERSION 0.17.0 LANGUAGES C CXX)
```
```cmake
		COMMAND ${CMAKE_COMMAND} -E make_directory "${PLUGIN_OUT}/shaders" "${HUD_OUT}"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:BlockRender>" "$<TARGET_PDB_FILE:BlockRender>" "${PLUGIN_OUT}/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/shaders/blocks.hlsl" "${PLUGIN_OUT}/shaders/"
```
```cmake
		COMMAND ${CMAKE_COMMAND} -E make_directory "${PLUGIN_OUT}/shaders" "${PLUGIN_OUT}/Scripts" "${HUD_OUT}"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:BlockRender>" "$<TARGET_PDB_FILE:BlockRender>" "${PLUGIN_OUT}/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/shaders/blocks.hlsl" "${PLUGIN_OUT}/shaders/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/scripts/BlockRender.reds" "${PLUGIN_OUT}/Scripts/"
```

- [x] **Step 3: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `414 test(s), 0 failure(s)`.

- [x] **Step 4: Check that BlockRender imports nothing from PhysX**

```bash
DUMPBIN="$(ls "/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/"*/bin/Hostx64/x64/dumpbin.exe | head -1)"
cd /d/Cyberpunk-Minecraft/spikes/block-render && MSYS_NO_PATHCONV=1 "$DUMPBIN" -imports -nologo build\\RelWithDebInfo\\BlockRender.dll \
  | grep -ci "physx"
```
Expected: `0`.

- [x] **Step 5: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: BlockRender 0.17.0 lists Night City's NPCs into SkyCraft's actor table" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 10: The overlay's NPC line, and its buttons

The overlay shows the status's NPC line in short: on or off, how many are listed of how many are in the world, and the
nearest, or why none are listed. An NPCs button in the top row switches `set npcs`; "List them in the log", under "More
controls", sends `npcs`.

**Files:**
- Modify: `tools/blockrender_hud/init.lua`

- [x] **Step 1: Edit `tools/blockrender_hud/init.lua`**

```lua
-- Kept small: one row of buttons (link, Minecraft's blocks as bodies, step 4's test box, the turn), and a line each for
-- the last request's reply, build mode (F8) and its drive, the bodies, and V. Two folded sections hold the rest: "More
-- controls" (the drawing mode, Minecraft's HUD, commands typed into Minecraft, the drive log, the ground and its
-- shapes) and "Full status" (every status line, and the frame time).
```
```lua
-- Kept small: one row of buttons (link, Minecraft's blocks as bodies, step 4's test box, the turn, NPCs in Minecraft),
-- and a line each for the last request's reply, build mode (F8) and its drive, the bodies, the NPCs, and V. Two folded
-- sections hold the rest: "More controls" (the drawing mode, Minecraft's HUD, commands typed into Minecraft, the drive
-- log, the ground and its shapes, and the NPCs' list) and "Full status" (every status line, and the frame time).
```
```lua
local boxOn = false      -- whether the test box stands
```
```lua
local boxOn = false      -- whether the test box stands
local npcs = nil         -- the npcs line: on or off, how many are listed and the nearest, or why none are
local npcsOn = nil       -- whether NPCs go into SkyCraft's actor table: "on" or "off"
```
```lua
    boxOn = false
```
```lua
    boxOn = false
    npcs = nil
    npcsOn = nil
```
```lua
                .. (boxOn and ", the test box standing" or "")
```
```lua
                .. (boxOn and ", the test box standing" or "")
        end
        local npcState = line:match("^npcs=(%a+)")
        if npcState ~= nil then
            npcsOn = npcState
            local listed = line:match("^npcs=on: (%d+ listed of %-?%d+ in the world)")
            local nearest, distance = line:match(", the nearest (.-) at ([%d.]+ m);")
            local why = line:match("; none listed: (.*)$")
            npcs = npcState .. (listed and (", " .. listed) or "")
                .. (nearest and (", the nearest " .. nearest .. " at " .. distance) or "")
                .. (why and (", none listed: " .. why) or "")
```
```lua
        end
        if status == nil then
```
```lua
        end
        if npcsOn ~= nil then
            ImGui.SameLine()
            if ImGui.Button("NPCs: " .. npcsOn .. "###npcs") then
                request("set npcs " .. (npcsOn == "on" and 0 or 1))
            end
        end
        if status == nil then
```
```lua
            ImGui.Text("Bodies: " .. bodies)
```
```lua
            ImGui.Text("Bodies: " .. bodies)
        end
        if npcs ~= nil then
            ImGui.Text("NPCs: " .. npcs)
```
```lua
                    request("shapes")
                end
            end
        end
```
```lua
                    request("shapes")
                end
            end
            if npcsOn ~= nil then
                ImGui.Text("NPCs:")
                ImGui.SameLine()
                if ImGui.Button("List them in the log") then
                    request("npcs")
                end
            end
        end
```

- [x] **Step 2: Check the overlay's syntax**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && luac -p tools/blockrender_hud/init.lua && echo ok
```
Expected: `ok`.

- [x] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: the overlay's NPC line, and its NPCs and list buttons" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 11: In the game: step 2

Step 2 of the spec's "The steps", with the user, on BlockRender 0.17.0, linked to SkyCraft's Minecraft in the
`CyberCraft` instance, on a busy pavement. The opening check comes first, with the least code: that the module compiles
and is found, that its list holds what the game's targeting finds, which capsule is an NPC's, and what the list costs.
Then the plate, the hits and the sizes. Nobody can see a stand-in: the plate's lamp shows them, and the log shows the
hits.

FreeFly (F7) stays off. Note what you see as you go: Step 14 records it. If the game freezes, read the logs and the
newest folder in `%LOCALAPPDATA%\REDEngine\ReportQueue`, and force-close it only when the user asks. If a check fails,
stop and find the cause with superpowers:systematic-debugging before going on. Fixes become new tasks.

These commands come back, as in spike 5's in-game steps:
- **Send a request,** atomically: the plugin runs it within a second. Replace `npcs` with the command, and send the next
  only once the status's `request=` line shows the last.
  ```bash
  P=/e/Cyberpunk2077/red4ext/plugins/BlockRender; printf '%s\n' "npcs" > "$P/request.tmp" && mv "$P/request.tmp" "$P/blockrender.request"
  ```
- **The log's newest lines:**
  ```bash
  L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); tail -30 "/e/Cyberpunk2077/red4ext/logs/$L"
  ```
- **The status:**
  ```bash
  cat /e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud/status.txt
  ```
- **Minecraft's log, for SkyCraft's lines and the chat's replies:**
  ```bash
  grep -E "SkyCraft: (hit|[0-9]+ Skyrim)|\[CHAT\]" "/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log" | tail -10
  ```
- **Counting the stand-ins:** send `mc /execute if entity @e[type=skycraft:skyrim_actor]`. Minecraft's log then has
  `[CHAT] Test passed. Count: …`, or `[CHAT] Test failed` when there are none. SkyCraft's own `… Skyrim actors mirrored
  as hittable stand-ins` lines come only as stand-ins are added, and only for some counts (step 1's results).

**Files:**
- Modify: `docs/spikes/2026-10-05-npcs-and-combat-results.md`

- [x] **Step 1: Deploy (the game must not be running)**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game first, because a running game locks the DLL.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
P=/e/Cyberpunk2077/red4ext/plugins/BlockRender
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default -DCP2077_DIR=E:/Cyberpunk2077 > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C|error MSB"; \
  "$CMAKE" --preset default -DCP2077_DIR= > /dev/null; \
  cmp build/RelWithDebInfo/BlockRender.dll "$P/BlockRender.dll" && cmp scripts/BlockRender.reds "$P/Scripts/BlockRender.reds" \
  && echo installed
```
Expected: `installed`. The second configure clears `CP2077_DIR` again.

- [x] **Step 2: In the game: the module compiles**

Ask the user to start the `CyberCraft` instance, then the game, and to wait at the game's main menu. If a "REDScript
compilation has failed" message box shows instead, ask the user to click OK and quit the game, and read redscript's log
for why: it names the file and line. Otherwise:
```bash
grep -E "BlockRender|Compilation complete|ERROR" /e/Cyberpunk2077/r6/logs/redscript_rCURRENT.log
```
Expected: the module's path, `…\red4ext\plugins\BlockRender\Scripts\BlockRender.reds`, among the files compiled, then
`Compilation complete`, and no `ERROR` line. In BlockRender's log: `BlockRender 0.17.0 loaded`, and `the redscript
module E:\Cyberpunk2077\red4ext\plugins\BlockRender\Scripts\BlockRender.reds: added to redscript's compile`. Since the
link crash's fix (below), that line ends `its natives are added once the game has registered its types`, and once the
menu is up the log has `natives: BlockRenderNpcAttached, BlockRenderNpcDetached, BlockRenderNpcCount and
BlockRenderNpcAt added`, then `npcs: the wraps have run on … threads: …` lines as the game attaches NPCs.

- [x] **Step 3: In the game: load, link, and the module found**

Ask the user to load the free-roam save, go to a pavement where many people walk, stand still at its edge, make a new
save there, and click the overlay's Link button. Expected:
- in the log, `module: found BlockRenderNpcs version 2, its 4 functions, the 13 fields List fills, and BlockRender's 4
  natives`, then `link: Minecraft is there (process …)`; and the `npcs: the wraps have run on … threads: …` lines so
  far, each thread with its attaches and detaches, the main thread marked;
- in the status, `npcs=on: … listed of … in the world, the nearest … at … m; capsules found for … of … looked for; …
  numbers given; the module … ms average, … ms worst a frame; 0 hits from Minecraft logged`, and the overlay's `NPCs:`
  line;
- counting the stand-ins (above): `[CHAT] Test passed. Count: …`, about as many as the status lists.

A `module: …` line saying something is missing stops the step: the class missing means redscript didn't compile the
module (Step 2); `the game session has no BlockRenderNpcs system` means the game doesn't make a mod's scriptable system
(the spec's risk 1), and a new task moves the list to a global function, found as `Puppet` finds `GetMountedVehicle`.

- [x] **Step 4: In the game: the opening check, the list against the game's targeting**

Still in the crowd, send `npcs`, or ask the user to click "List them in the log" under "More controls". Expected, in the
log:
- `npcs: … listed of … in the world; V's punch …, read from Items.w_melee_004__fists_a; V's max health …`: ask the user
  to open the inventory, find the fists (or the Gorilla Arms, if V has them) and read their damage number, and compare;
- a line per listed NPC: its number, name and distance, its box and where it came from, its health, level and flags;
- `npcs: GetTargetParts finds … NPCs within 80 m; the list lacks …`: 0, or a few that have just come within 80 m. If it
  lacks many, the wraps miss NPCs (the spec's risk 2): note which, and a new task makes the list from `GetTargetParts`;
- `npcs: the wraps' list holds …, of which the module found … by their EntityIDs; … attached and … detached since the
  game started, … already there, … not there, 0 past the list's 1024`: found close to the list's size (NPCs not found
  are ones the game dropped without a detach), and `npcs: the wraps ran on …`, the threads.

- [x] **Step 5: In the game: the opening check, which capsule is an NPC's**

Ask the user to walk up to an NPC standing still (a vendor, or someone waiting), to stand about a metre from them, and
to say when they're there. Send `shapes`, then `npcs`. Expected:
- `shapes:` lines for every shape within 2 m of V's middle: V's two kinematic capsules (radius 0.32, query filter
  `0:0:2000:0`, and radius 1), and the NPC's, a kinematic capsule with its own filters and bounds;
- that NPC's `npcs:` line: its box `from its capsule's: radius …, half-height …, leaning 0 degrees, query filter …`,
  about 0.6 to 0.8 wide, from 0.00 below its position to about 1.7 to 1.9 above. Since the first run (below), the
  capsule taken is the one whose query filter's third word has the controller bit `0x2000`: the vendor's radius 0.30,
  `0:0:10002000:0`.

If most NPCs say `a person's: no capsule found`, or the capsule taken isn't the one `shapes` shows around the NPC, the
rule changes to what `shapes` shows (its filter, its kind, its size), as a new task.

- [x] **Step 6: In the game: the opening check, what the list costs**

In the crowd, read the status's NPC line for 10 s: the module's time a frame, on average and at worst. Then ask the user
to click the overlay's `NPCs: on` button, so it reads `NPCs: off`, for 10 s, then on again. Read the status's `frame=`
line and the overlay's CET frame time each time, and count the stand-ins (above) while it's off and once it's on again.
Expected: the NPC line says `npcs=off` while off, and the count says `[CHAT] Test failed`; once it's on again, the count
is back.

- [x] **Step 7: In the game: a plate on the pavement**

Send `mc creative`, `mc /item replace entity @s hotbar.0 with minecraft:stone_pressure_plate 4`, `mc /item replace
entity @s hotbar.1 with minecraft:redstone_lamp 4`, `mc /item replace entity @s hotbar.2 with minecraft:diamond_sword`
and `mc /item replace entity @s hotbar.3 with minecraft:stone 16`, each once the last shows in the status's `request=`
line: the plate is on 1, the lamp on 2, the sword on 3 and stone on 4. Ask the user to press F8, press 1 and place the
plate with a right click on the pavement where people walk, then press 2 and place the lamp right beside it; to press
F8 again and step back a few metres, so Minecraft's player, at V, isn't on the plate; and to watch as people cross it.
Expected: the lamp lights while someone is on the plate, and goes out after they've left it. Note how often it misses
someone crossing, and anyone it lights for who isn't on it.

- [x] **Step 8: In the game: hits, logged**

Ask the user to walk up to an NPC who is standing still, press F8, press 3 for the sword, and hit the NPC two or three
times, a second or more apart, aiming at their body, then press F8 again. Expected: in the log, `hit on NPC … (…) for
7.00 of Minecraft's damage, knockback …; blade; not applied in Part 1` for each, with the NPC's name; the NPC doesn't
react in the game; the status's hits count goes up by as many; and Minecraft's log says `SkyCraft: hit … for 7.0 …`.

- [x] **Step 9: In the game: sizes beside a drone, or a mech**

If a drone or a mech can be reached without a fight (some corporations' and gangs' grounds keep one standing), ask the
user to stand near it, then send `npcs`. Note its line: its box and where it came from. If none can be, skip this step
and say so in the results: Part 2's fights will meet them.

- [x] **Step 10: In the game: the overlay, build mode and the bodies**

Ask the user to press F8, press 4 for stone and place a few blocks of it on the pavement, walk onto them in build mode,
then leave build mode. Expected: build mode, the drive and the bodies work as in spike 5: the bodies line counts them,
and V stands on them.

- [x] **Step 11: In the game: unlinking empties the table**

Ask the user to click the overlay's Unlink button, then count the stand-ins (above). Expected: the NPC line says `none
listed: not linked`, and the count says `[CHAT] Test failed`: the heartbeat goes on while unlinked, so SkyCraft reads
the emptied table, and the typed command too.

- [x] **Step 12: Ask the user to quit the game and stop Minecraft**

- [x] **Step 13: Read the log once more**

```bash
L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); grep -E "module:|npcs:|hit on NPC|shapes:" "/e/Cyberpunk2077/red4ext/logs/$L" | head -200
```
Everything the steps above logged, for the results.

- [x] **Step 14: Record step 2's results in `docs/spikes/2026-10-05-npcs-and-combat-results.md`**

A "Step 2" section: each check above with its outcome and numbers: redscript's compile and the module's checks; the
list against the targeting; which capsule is an NPC's, with its filters and box; V's punch against the inventory; the
module's time and the frame rates with NPCs on and off; the plate; the hits; the drone or mech, if any; and anything that
surprised, and what it means for Part 2. Put the spike's answer so far at the top.

- [x] **Step 15: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-05-npcs-and-combat-results.md \
  && git commit -q -m "npcs-and-combat: step 2's results" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

**Step 2's first run (2026-10-05)** stopped at Step 3, and found three faults, each fixed as its own commit before going
on, with a redeploy (Step 1 again):
- **The module wasn't found:** `module: BlockRenderNpcs.Version isn't there with the types BlockRender passes`. A
  throwaway CET mod, using Codeware's `Reflection`, showed the game keeps a script class's static functions among the
  global functions, as `BlockRenderNpcs::Version;` and `BlockRenderNpcs::Current;`, not in the class's static list.
  `GameScript` now looks them up there (`da1be78`). The rest checked out from Lua: the session's system exists,
  `Version()` is 1, `List(80)` gave 12 NPCs with all twelve fields filled, and `Compare(80)` said `GetTargetParts finds
  11 NPCs within 80 m; the list lacks 0`.
- **The capsule rule took the wrong capsule:** `shapes` beside a vendor showed three kinematic capsules around it, the
  controller (radius 0.30, query filter `0:0:10002000:0`), one of radius 0.40 and one of 0.06; narrowest-first took the
  0.06 m one. The rule now takes the capsule with the controller's query filter bit `0x2000`, which V's controller has
  too (`1fa17f4`, with the vendor's capsules as a test).
- **The game crashed when Link first called `List`** (the second start, after the two fixes): an access violation in
  the game's handle release, inside `List`, called from `GameScript::List`. Read from the crash dump: a `ref` in the
  module's own list held no object but a live count, so the module's arrays had been corrupted before the call. The
  wraps pushed onto and erased from those arrays, and the game runs `NPCPuppet`'s callbacks on several threads at once
  (a Codeware issue's crash report shows `OnGameAttached` on other threads; Cyber Engine Tweaks fixed crashes in
  overrides of `OnGameAttached` and `OnDetach`). A script array has no lock. Fixed by moving the list into BlockRender
  (`b68fe3b`): the wraps pass only `EntityID`s to BlockRender's natives, `NpcRegistry` keeps them behind a mutex and
  counts the calls by thread, and `List` copies them on the main thread, finding each NPC again by its ID. Module
  version 2, with a thirteenth field, `found`; the module's check is logged before its first call; 423 tests, and each
  new one broken once (`NpcRegistry`'s many-threads test crashes without the lock). Steps 2, 3 and 4 above now expect
  the new lines.

The first load also crashed once, in the engine's allocator during the load, before any link; the second load didn't.
The wraps' race fits it too: a load attaches many NPCs at once, and an array grown on one thread while another writes
to it leaves writes in freed memory. The results record it.

---

### Task 12: Close Part 1, and plan Part 2

- [x] **Step 1: Add "How Part 1 went" to this plan**

At the end, after "Found while planning": what ran as written, what changed, and what steps 1 and 2 found.

- [x] **Step 2: Plan Part 2**

Use superpowers:writing-plans to add Part 2's tasks, from Task 13 on, to this plan: combat both ways (the spec's "Part
2", and steps 3 and 4). Verify them as Part 1's were. Changes to the spec found while planning go in its "Changes while
planning".

- [x] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/superpowers \
  && git commit -q -m "npcs-and-combat plan: Part 1 closed, Part 2 planned" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

## Part 2: combat both ways (steps 3 and 4)

Part 2 makes Minecraft's hits on NPCs land as V's, and in build mode gives the game's hits on V to Minecraft's player,
whose death kills V (the spec's "Part 2: combat both ways"). It was planned after step 2 (Task 12), with what steps 1
and 2 found: the boxes stop at the shoulders, and the game runs script callbacks on worker threads. So the damage
pipeline is taken to run on any thread too, as the NPCs' callbacks do (the spec's risk 8): its wraps read the hit itself
and call BlockRender's natives, and nothing in them touches the module's state.
- **Task 13** gives an NPC's box the top of its tallest capsule.
- **Tasks 14 and 15** make the plain C++ of combat, and `mclink`'s hurts; **Task 16** is step 3, against Minecraft,
  without the game.
- **Tasks 17 to 22** make the game's side: the natives' inbox, the module's combat, `GameScript`'s calls, the requests
  and status, BlockRender 0.18.0's frame, and the overlay.
- **Tasks 23 to 30** come from the deep review of Part 2, after Task 22 had run: Minecraft's hits start the game's damage
  pipeline at Minecraft's damage × V's punch, and the pipeline does the rest, as to V's own punches; V punches with
  what its unarmed slot draws; and in build mode V's unarmed weapon is drawn and V is hidden. They replace Task 18's
  `Process` wrap and its exact amount. **Task 31** is step 4, in the game.
- **Tasks 32 to 35** make step 4's three fixes: hits start at V's punch × Minecraft's damage to the hit power, the
  square root by default; V's Immortal is the game's override; and hiding V stops V's effects and hides the game's
  crosshair. **Task 36** checks them in the game, and fights again.
- **Task 37** closes Part 2, with a handoff for planning Part 3.

Test counts: 423 at the start of Part 2. Then 424 after Task 13, 429 after Task 14, 431 after Task 15, 434 after Task 17,
and 435 after Task 20; 437 after Task 23, 438 after Task 24, 439 after Task 28, and 440 after Task 32. Tasks 18, 19, 21,
22, 25 to 27, 29, 30 and 33 to 35 add none.

## File map (Part 2)

| File | Responsibility |
|---|---|
| `src/physx/Characters.*` | An NPC's box reaching the top of its tallest capsule |
| `src/link/Combat.*` | Which hits count and where they start, V's punch × Minecraft's damage to the hit power; what the pipeline did to them, and why it dealt some nothing, in words; V's hurts as `kInHurt`, and their sums; the death window; V's god mode; V's switches for its weapon and meshes |
| `src/link/Link.*` | McState's tick length in the status |
| `tools/mclink/*` | The `hurt` action, through the sums, and step 3's script `hurts.txt` |
| `src/CombatInbox.*` | Hits on V offered, non-lethal ones left to V; our hits landed or dealt nothing; from any thread, behind a lock |
| `src/Natives.*` | `BlockRenderVHurt`, `BlockRenderHitLanded` and `BlockRenderHitEnded` |
| `scripts/BlockRender.reds` | Version 5: `Hit`, `SetImmortal` (the game's override), `KillV`, `Punch`, `Arm` and `HideV` (V's meshes and effects, and the game's crosshair); what V's unarmed slot draws; the wraps of `ProcessPipeline` and `DealDamages` |
| `tools/check_reds.py` | The module compiled offline, with and without Codeware's scripts |
| `src/GameScript.*` | The module's new calls, whether V's unarmed weapon is drawn, what `HideV` did, and seven natives checked |
| `src/Requests.*`, `src/Status.*` | `hit` and `punch`; the status's combat line, with the hit power, and the v line, with V's effects and the crosshair |
| `src/Plugin.cpp`, `CMakeLists.txt` | 0.18.0, then 0.18.1: `Fight`, `SwitchV`, `SendHurts`, `set combat`, `set hidev`, `set hitpower`, `hit` and `punch`, and the combat and v lines |
| `tools/blockrender_hud/init.lua` | The combat line, the Combat button, V's punch in the log, V's weapon drawn or not, and V hidden or seen |
| `tests/TestCombat.cpp`, `tests/TestCombatInbox.cpp` | One test file per new unit; the others gain tests |

---

### Task 13: An NPC's box reaches the top of its tallest capsule

Step 2 found an NPC's controller stops at the shoulders: beside the vendor it reached 1.62 m above its feet, while two
other capsules on its axis reached 1.75 and 1.76 m (the results' "An NPC's capsule"). The box keeps the controller's
width, and its bottom (the lower of the feet and the controller's bottom), and now reaches the top of the tallest
capsule that stands on the NPC's axis as its controller does: leaning at most 25 degrees, its axis within 0.2 m of the
feet, whatever its filters. `npcs` names that capsule when it isn't the controller.

**Files:**
- Modify: `src/physx/Characters.h`, `src/physx/Characters.cpp`, `src/Plugin.cpp`, `tests/TestCharacters.cpp`

- [x] **Step 1: Edit `tests/TestCharacters.cpp`: the vendor's capsules, and the new calls**

```cpp
TEST(CharactersBoxReachesFromTheFeetOrTheCapsulesBottomToItsTop)
{
	// V's controller spans 0.28 to 1.72 m above the feet, so the box starts at the feet.
	const link::NpcBox v = BoxOf(Controller(), kFeet);
```
```cpp
TEST(CharactersTakeTheBoxsTopFromTheTallestCapsuleOnTheAxis)
{
	// Step 2's vendor's capsules, as shapes showed them (2026-10-05), from its feet: the controller from 0.17 to 1.62 m
	// above them; one of radius 0.40 from 0.05 to 1.75 m; and a thin one, radius 0.06, from 0.04 to 1.76 m.
	const CapsuleSeen              controller = Filtered(Standing(kFeet.x, kFeet.y, 0.895, 0.30, 0.425), 0x10002000);
	const CapsuleSeen              wide = Filtered(Standing(kFeet.x, kFeet.y, 0.90, 0.40, 0.45), 0x2000000);
	const CapsuleSeen              thin = Filtered(Standing(kFeet.x, kFeet.y, 0.90, 0.06, 0.80), 0x24000002);
	const std::vector<CapsuleSeen> seen{ controller, wide, thin };
	const CapsuleSeen              tallest = TallestCapsule(seen, controller, kFeet);
	CHECK_NEAR(tallest.radius, 0.06, 1e-9);
	const link::NpcBox box = BoxOf(controller, tallest, kFeet);
	CHECK_NEAR(box.width, 0.60, 1e-9);  // the controller's
	CHECK_NEAR(box.below, 0.0, 1e-9);
	CHECK_NEAR(box.above, 1.76, 1e-9);  // the thin one's top, not the controller's 1.62
	// A taller capsule 0.3 m off, another NPC's, and one leaning 30 degrees: neither is this NPC's.
	const CapsuleSeen beside = Filtered(Standing(kFeet.x + 0.3, kFeet.y, 1.0, 0.35, 0.8), 0x2000000);
	const CapsuleSeen leaning = Leaning(Filtered(Standing(kFeet.x, kFeet.y, 1.0, 0.20, 1.0), 0x2000000), 30.0, true);
	CHECK_NEAR(TallestCapsule(std::vector{ controller, beside, leaning }, controller, kFeet).radius, 0.30, 1e-9);
	// None taller: the controller itself, and its own top.
	CHECK_NEAR(TallestCapsule(std::vector<CapsuleSeen>{}, controller, kFeet).radius, 0.30, 1e-9);
	CHECK_NEAR(BoxOf(controller, controller, kFeet).above, 1.62, 1e-9);
}

TEST(CharactersBoxReachesFromTheFeetOrTheCapsulesBottomToItsTop)
{
	// V's controller spans 0.28 to 1.72 m above the feet, so the box starts at the feet.
	const link::NpcBox v = BoxOf(Controller(), Controller(), kFeet);
```
```cpp
	const link::NpcBox low = BoxOf(Standing(kFeet.x, kFeet.y, 0.6, 0.32, 0.4), kFeet);
	CHECK_NEAR(low.below, 0.12, 1e-9);
	CHECK_NEAR(low.above, 1.32, 1e-9);
	// Leaning 20 degrees: its top is lower by the lean.
	const link::NpcBox leaning = BoxOf(Leaning(Controller(), 20.0, false), kFeet);
```
```cpp
	const CapsuleSeen  lowCapsule = Standing(kFeet.x, kFeet.y, 0.6, 0.32, 0.4);
	const link::NpcBox low = BoxOf(lowCapsule, lowCapsule, kFeet);
	CHECK_NEAR(low.below, 0.12, 1e-9);
	CHECK_NEAR(low.above, 1.32, 1e-9);
	// Leaning 20 degrees: its top is lower by the lean.
	const CapsuleSeen  leaningCapsule = Leaning(Controller(), 20.0, false);
	const link::NpcBox leaning = BoxOf(leaningCapsule, leaningCapsule, kFeet);
```
```cpp
	sizes.Found(101, Controller(), kFeet);
```
```cpp
	sizes.Found(101, Controller(), Controller(), kFeet);
```
```cpp
	sizes.Found(7, capsule, kFeet);
	CHECK_STR(DescribeNpc(npc, 3, 4.3, sizes), "3 Ganger One, 4.3 m: 0.64 wide, from 0.00 below its position to 1.72 above, its "
	                                            "capsule's: radius 0.32, half-height 0.40, leaning 0 degrees, query filter "
	                                            "0:0:2000:0; health 80 of 100, level 12; hostile, in combat");
```
```cpp
	sizes.Found(7, capsule, capsule, kFeet);
	CHECK_STR(DescribeNpc(npc, 3, 4.3, sizes), "3 Ganger One, 4.3 m: 0.64 wide, from 0.00 below its position to 1.72 above, its "
	                                            "capsule's: radius 0.32, half-height 0.40, leaning 0 degrees, query filter "
	                                            "0:0:2000:0; health 80 of 100, level 12; hostile, in combat");
	// Its top from a taller capsule on its axis: that one's said too.
	sizes.Found(7, capsule, Filtered(Standing(kFeet.x, kFeet.y, 0.90, 0.06, 0.80), 0x24000002), kFeet);
	CHECK_STR(DescribeNpc(npc, 3, 4.3, sizes), "3 Ganger One, 4.3 m: 0.64 wide, from 0.00 below its position to 1.76 above, its "
	                                            "capsule's: radius 0.32, half-height 0.40, leaning 0 degrees, query filter "
	                                            "0:0:2000:0, its top another's: radius 0.06, query filter 0:0:24000002:0; health 80 "
	                                            "of 100, level 12; hostile, in combat");
```

- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2660: 'cpmc::px::BoxOf': function does not take 3 arguments`, and
`error C3861: 'TallestCapsule': identifier not found`.

- [x] **Step 3: Edit `src/physx/Characters.h`**

```cpp
// Its box: twice the radius wide, reaching from the lower of the feet and the capsule's bottom up to its top.
link::NpcBox BoxOf(const CapsuleSeen& aCapsule, const Vec3d& aFeet);
```
```cpp
// The capsule that reaches highest of those in aSeen standing as the NPC's controller aController does, whatever their
// filters: leaning at most kCapsuleTilt from upright, their axes within kCapsuleAcross of aFeet. The controller stops at
// the shoulders: step 2's vendor's reached 1.62 m above its feet, and two other capsules on its axis 1.75 and 1.76 m.
// aController itself if none reaches higher.
CapsuleSeen TallestCapsule(std::span<const CapsuleSeen> aSeen, const CapsuleSeen& aController, const Vec3d& aFeet);

// Its box: twice the controller's radius wide, reaching from the lower of the feet and the controller's bottom up to the
// tallest capsule's top.
link::NpcBox BoxOf(const CapsuleSeen& aController, const CapsuleSeen& aTallest, const Vec3d& aFeet);
```
```cpp
	// aEntity's capsule, found with its feet at aFeet: its box from now on.
	void Found(uint64_t aEntity, const CapsuleSeen& aCapsule, const Vec3d& aFeet);
```
```cpp
	// aEntity's controller and tallest capsule, found with its feet at aFeet: its box from now on.
	void Found(uint64_t aEntity, const CapsuleSeen& aController, const CapsuleSeen& aTallest, const Vec3d& aFeet);
```
```cpp
	// The capsule aEntity's box came from; null if it has none.
	const CapsuleSeen* Capsule(uint64_t aEntity) const;
```
```cpp
	// The controller aEntity's box came from, and the capsule its top came from; null if it has none.
	const CapsuleSeen* Capsule(uint64_t aEntity) const;
	const CapsuleSeen* Tallest(uint64_t aEntity) const;
```
```cpp
	std::unordered_map<uint64_t, CapsuleSeen>  capsules_;
```
```cpp
	std::unordered_map<uint64_t, CapsuleSeen>  capsules_;
	std::unordered_map<uint64_t, CapsuleSeen>  tallest_;
```

- [x] **Step 4: Edit `src/physx/Characters.cpp`**

```cpp
	return length > 0.0 ? std::fabs(aCapsule.axis.z) / length : 0.0;
}
}
```
```cpp
	return length > 0.0 ? std::fabs(aCapsule.axis.z) / length : 0.0;
}

// Whether the capsule stands as an NPC's: leaning at most kCapsuleTilt from upright, its axis within kCapsuleAcross of
// aFeet.
bool StandsAt(const CapsuleSeen& aCapsule, const Vec3d& aFeet)
{
	if (Upright(aCapsule) < std::cos(kCapsuleTilt * std::numbers::pi / 180.0))
	{
		return false;
	}
	// The axis's distance from the feet: the length of (feet - centre) x axis, over the axis's length.
	const Vec3d  to{ aFeet.x - aCapsule.centre.x, aFeet.y - aCapsule.centre.y, aFeet.z - aCapsule.centre.z };
	const Vec3d& axis = aCapsule.axis;
	const Vec3d  cross{ to.y * axis.z - to.z * axis.y, to.z * axis.x - to.x * axis.z, to.x * axis.y - to.y * axis.x };
	return Length(cross) / Length(axis) <= kCapsuleAcross;
}

// How far the capsule reaches up and down from its centre.
double Reach(const CapsuleSeen& aCapsule)
{
	return Upright(aCapsule) * aCapsule.halfHeight + aCapsule.radius;
}
}
```
```cpp
	const double               upright = std::cos(kCapsuleTilt * std::numbers::pi / 180.0);
	std::optional<CapsuleSeen> best;
	for (const CapsuleSeen& capsule : aSeen)
	{
		if ((capsule.queryFilter[2] & kControllerQueryBit) == 0 || Upright(capsule) < upright)
		{
			continue;
		}
		// The axis's distance from the feet: the length of (feet - centre) x axis, over the axis's length.
		const Vec3d  to{ aFeet.x - capsule.centre.x, aFeet.y - capsule.centre.y, aFeet.z - capsule.centre.z };
		const Vec3d& axis = capsule.axis;
		const Vec3d  cross{ to.y * axis.z - to.z * axis.y, to.z * axis.x - to.x * axis.z, to.x * axis.y - to.y * axis.x };
		if (Length(cross) / Length(axis) > kCapsuleAcross)
```
```cpp
	std::optional<CapsuleSeen> best;
	for (const CapsuleSeen& capsule : aSeen)
	{
		if ((capsule.queryFilter[2] & kControllerQueryBit) == 0 || !StandsAt(capsule, aFeet))
```
```cpp
link::NpcBox BoxOf(const CapsuleSeen& aCapsule, const Vec3d& aFeet)
{
	const double reach = Upright(aCapsule) * aCapsule.halfHeight + aCapsule.radius;  // up and down from its centre
	const double bottom = aCapsule.centre.z - reach;
	const double top = aCapsule.centre.z + reach;
	return { 2.0 * aCapsule.radius, std::max(0.0, aFeet.z - bottom), top - aFeet.z };
```
```cpp
CapsuleSeen TallestCapsule(std::span<const CapsuleSeen> aSeen, const CapsuleSeen& aController, const Vec3d& aFeet)
{
	CapsuleSeen tallest = aController;
	for (const CapsuleSeen& capsule : aSeen)
	{
		if (StandsAt(capsule, aFeet) && capsule.centre.z + Reach(capsule) > tallest.centre.z + Reach(tallest))
		{
			tallest = capsule;
		}
	}
	return tallest;
}

link::NpcBox BoxOf(const CapsuleSeen& aController, const CapsuleSeen& aTallest, const Vec3d& aFeet)
{
	const double bottom = aController.centre.z - Reach(aController);
	const double top = std::max(aController.centre.z + Reach(aController), aTallest.centre.z + Reach(aTallest));
	return { 2.0 * aController.radius, std::max(0.0, aFeet.z - bottom), top - aFeet.z };
```
```cpp
void NpcSizes::Found(uint64_t aEntity, const CapsuleSeen& aCapsule, const Vec3d& aFeet)
{
	boxes_[aEntity] = BoxOf(aCapsule, aFeet);
	capsules_[aEntity] = aCapsule;
```
```cpp
void NpcSizes::Found(uint64_t aEntity, const CapsuleSeen& aController, const CapsuleSeen& aTallest, const Vec3d& aFeet)
{
	boxes_[aEntity] = BoxOf(aController, aTallest, aFeet);
	capsules_[aEntity] = aController;
	tallest_[aEntity] = aTallest;
```
```cpp
	std::erase_if(capsules_, [&](const auto& aEntry) { return !aIds.IdOf(aEntry.first); });
```
```cpp
	std::erase_if(capsules_, [&](const auto& aEntry) { return !aIds.IdOf(aEntry.first); });
	std::erase_if(tallest_, [&](const auto& aEntry) { return !aIds.IdOf(aEntry.first); });
```
```cpp
	return found != capsules_.end() ? &found->second : nullptr;
```
```cpp
	return found != capsules_.end() ? &found->second : nullptr;
}

const CapsuleSeen* NpcSizes::Tallest(uint64_t aEntity) const
{
	const auto found = tallest_.find(aEntity);
	return found != tallest_.end() ? &found->second : nullptr;
```
```cpp
		                   capsule->queryFilter[0], capsule->queryFilter[1], capsule->queryFilter[2], capsule->queryFilter[3]);
```
```cpp
		                   capsule->queryFilter[0], capsule->queryFilter[1], capsule->queryFilter[2], capsule->queryFilter[3]);
		const CapsuleSeen* tallest = aSizes.Tallest(aNpc.entity);
		if (tallest && tallest->centre.z + Reach(*tallest) > capsule->centre.z + Reach(*capsule))
		{
			from += std::format(", its top another's: radius {:.2f}, query filter {:x}:{:x}:{:x}:{:x}", tallest->radius,
			                    tallest->queryFilter[0], tallest->queryFilter[1], tallest->queryFilter[2], tallest->queryFilter[3]);
		}
```

- [x] **Step 5: Edit `src/Plugin.cpp`: the tallest capsule, where capsules are looked for**

```cpp
				g_npcSizes.Found(entity, *capsule, feet);
```
```cpp
				g_npcSizes.Found(entity, *capsule, cpmc::px::TallestCapsule(seen, *capsule, feet), feet);
```

- [x] **Step 6: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `424 test(s), 0 failure(s)`.

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: an NPC's box reaches the top of its tallest capsule, not its controller's" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 14: Which of Minecraft's hits count, V's hurts for Minecraft, their sums, deaths and god mode

The spec's Part 2, the parts that don't need the game, in `render_core` as `link/Combat`:
- **Which hits count** ("Minecraft's hits on NPCs"): a `kEvHitActor` for a stand-in listed in the last 2 s, with damage
  that's finite and over 0, capped at 1,000 (`/kill` sends 3.4 × 10³⁸), and V's punch read. Its amount is Minecraft's
  damage × V's punch; it's strong for a crit, or knockback over 0.45, as SkyCraft staggers. `HitDrop` says why one
  doesn't count, and `HitResultName` what the module's `Hit` made of one.
- **V's hurts** ("The game's hits on V"): the module's wrap hands BlockRender each one in the game's terms (`GameHurt`:
  the health it would take, its `gamedataAttackType` as a number, its instigator). `HurtFrom` makes it Minecraft's: its
  share of V's max health; its kind (melee for `QuickMelee`, `StrongMelee`, `Melee`, `GuardBreak`, `WhipAttack`,
  `ChargedWhipAttack` and `Reflect`; projectile for `Ranged`, `Thrown` and `Direct`; magic for `Hack`; other for the
  rest); a power attack for `StrongMelee`, `ChargedWhipAttack` and `GuardBreak`; and its attacker's number, or 0.
  `HurtInput` encodes it as `kInHurt`: `a` = the share × 10,000, for SkyCraft's ÷ 100 and ÷ 5 of Minecraft's 20.
- **The sums:** `HurtSums` sends a hurt at once when none went in the last 11 of Minecraft's ticks, at McState's
  `tickMs`, and otherwise adds it to a sum that goes when those ticks are up, with its biggest part's kind and attacker
  and a power attack if any part had one. Minecraft takes from a second hurt within 10 ticks only what's more than the
  first, and applies each at its next tick.
- **Deaths** ("Deaths"): Minecraft's death kills V with combat on, while build mode is on or within 2 s of its end.
- **God mode** ("V's god mode"): on while combat and build mode are both on, whichever came first; off when either
  ends; and off once whenever V is found again, in case a save kept it.

The game's attack types are numbered as in 2.31's `gamedataAttackType` (the decompiled scripts' `orphans.swift`):
`ChargedWhipAttack` 0, `Direct` 1, `Effect` 2, `Explosion` 3, `ForceKill` 4, `GuardBreak` 5, `Hack` 6, `Melee` 7,
`PressureWave` 8, `QuickMelee` 9, `Ranged` 10, `Reflect` 11, `StrongMelee` 12, `Thrown` 13, `WhipAttack` 14.

**Files:**
- Create: `src/link/Combat.h`, `src/link/Combat.cpp`, `tests/TestCombat.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Write the failing test, `tests/TestCombat.cpp`**

```cpp
#include "Harness.h"

#include "link/Combat.h"

#include <cmath>
#include <limits>

using namespace cpmc;
using namespace cpmc::link;

namespace
{
// Minecraft's hit on stand-in aFormId: aDamage, with knockback aKnockback, and the crit flag if aCrit.
proto::McEvent Hit(uint32_t aFormId, float aDamage, float aKnockback = 0.4f, bool aCrit = false)
{
	proto::McEvent event{};
	event.type = proto::kEvHitActor;
	event.formId = aFormId;
	event.a = aDamage;
	event.d = aKnockback;
	event.flags = aCrit ? proto::kHitCritical : 0u;
	event.weapon = proto::kWeaponBlade;
	return event;
}

// gamedataAttackType's values in the game 2.31.
constexpr int32_t kQuickMelee = 9;
constexpr int32_t kStrongMelee = 12;
constexpr int32_t kRanged = 10;
constexpr int32_t kHack = 6;
constexpr int32_t kExplosion = 3;
}

TEST(CombatCountsHitsOnListedNpcsWithDamage)
{
	ActorIds       ids;
	const uint32_t id = ids.Id(0xABC, 10.0);
	HitDrop        drop = HitDrop::Count;
	// A diamond sword at full cooldown, and V's punch of 102: 714 of the NPC's health.
	const std::optional<NpcHit> hit = AcceptHit(Hit(id, 7.0f), ids, 10.5, 102.0, drop);
	CHECK(hit.has_value());
	CHECK(hit && hit->entity == 0xABC && hit->formId == id && !hit->strong);
	CHECK_NEAR(hit ? hit->amount : 0.0, 714.0, 1e-9);
	// Strong: a crit, or knockback over 0.45 (a sprint hit); 0.45 itself isn't, compared as SkyCraft compares, in float.
	CHECK(AcceptHit(Hit(id, 10.5f, 0.4f, true), ids, 10.5, 102.0, drop).value_or(NpcHit{}).strong);
	CHECK(AcceptHit(Hit(id, 7.0f, 0.5f), ids, 10.5, 102.0, drop).value_or(NpcHit{}).strong);
	CHECK(!AcceptHit(Hit(id, 7.0f, 0.45f), ids, 10.5, 102.0, drop).value_or(NpcHit{ 0, 0, 0.0, 0.0, true }).strong);
	// /kill's 3.4e38 is capped at 1,000.
	const std::optional<NpcHit> kill = AcceptHit(Hit(id, std::numeric_limits<float>::max()), ids, 10.5, 102.0, drop);
	CHECK_NEAR(kill ? kill->damage : 0.0, 1000.0, 1e-9);
	CHECK_NEAR(kill ? kill->amount : 0.0, 102000.0, 1e-6);
	// A stand-in listed 2 s ago still counts; 2.5 s ago it doesn't, and nor does a number never given.
	CHECK(AcceptHit(Hit(id, 7.0f), ids, 12.0, 102.0, drop).has_value());
	CHECK(!AcceptHit(Hit(id, 7.0f), ids, 12.5, 102.0, drop).has_value());
	CHECK(drop == HitDrop::NotListed);
	CHECK(!AcceptHit(Hit(id + 1, 7.0f), ids, 10.5, 102.0, drop).has_value());
	CHECK(drop == HitDrop::NotListed);
	// Knockback only, or damage that isn't finite: skipped.
	CHECK(!AcceptHit(Hit(id, 0.0f), ids, 10.5, 102.0, drop).has_value());
	CHECK(drop == HitDrop::NoDamage);
	CHECK(!AcceptHit(Hit(id, std::numeric_limits<float>::quiet_NaN()), ids, 10.5, 102.0, drop).has_value());
	CHECK(drop == HitDrop::NoDamage);
	CHECK(!AcceptHit(Hit(id, std::numeric_limits<float>::infinity()), ids, 10.5, 102.0, drop).has_value());
	CHECK(drop == HitDrop::NoDamage);
	// V's punch not read: no hit.
	CHECK(!AcceptHit(Hit(id, 7.0f), ids, 10.5, 0.0, drop).has_value());
	CHECK(drop == HitDrop::NoPunch);
	CHECK_STR(HitDropName(HitDrop::NoPunch), "no punch");
	CHECK_STR(HitResultName(-2), "the NPC is dead");
	CHECK_STR(HitResultName(7), "result 7");
}

TEST(CombatMakesTheGamesHurtsIntoMinecraftsKinds)
{
	CHECK(HurtKindOf(kQuickMelee) == proto::kHurtMelee);
	CHECK(HurtKindOf(kStrongMelee) == proto::kHurtMelee);
	for (const int32_t melee : { 0, 5, 7, 11, 14 })  // ChargedWhipAttack, GuardBreak, Melee, Reflect, WhipAttack
	{
		CHECK(HurtKindOf(melee) == proto::kHurtMelee);
	}
	for (const int32_t projectile : { 1, 10, 13 })  // Direct, Ranged, Thrown
	{
		CHECK(HurtKindOf(projectile) == proto::kHurtProjectile);
	}
	CHECK(HurtKindOf(kHack) == proto::kHurtMagic);
	for (const int32_t other : { 2, 3, 4, 8, 15, 16, -1 })  // Effect, Explosion, ForceKill, PressureWave, and none
	{
		CHECK(HurtKindOf(other) == proto::kHurtOther);
	}
	CHECK(PowerAttack(kStrongMelee) && PowerAttack(0) && PowerAttack(5));
	CHECK(!PowerAttack(kQuickMelee) && !PowerAttack(kRanged));
	CHECK_STR(AttackTypeName(kRanged), "Ranged");
	CHECK_STR(AttackTypeName(14), "WhipAttack");
	CHECK_STR(AttackTypeName(17), "attack type 17");
	CHECK_STR(HurtKindName(proto::kHurtProjectile), "projectile");
	// A gunshot of 45 on V, with 456 max health, from a listed ganger: about a tenth of Minecraft's 20.
	ActorIds       ids;
	const uint32_t ganger = ids.Id(0x51, 0.0);
	const Hurt     shot = HurtFrom(GameHurt{ 45.0, kRanged, 0x51 }, 456.0, ids);
	CHECK_NEAR(shot.share, 45.0 / 456.0, 1e-12);
	CHECK(shot.kind == proto::kHurtProjectile && shot.attacker == ganger && !shot.power);
	const proto::InputEvent input = HurtInput(shot);
	CHECK_EQ(input.type, uint16_t(proto::kInHurt));
	CHECK_EQ(input.code, uint16_t(proto::kHurtProjectile));
	CHECK_EQ(input.a, int32_t(std::lround(45.0 / 456.0 * 10000.0)));  // 987: SkyCraft's 9.87, / 5 = 1.97 of Minecraft's 20
	CHECK_EQ(input.b, int32_t(ganger));
	CHECK_EQ(input.c, 0);
	// From someone not listed, or nobody: no attacker. A power attack is flagged. More than V's max health is held to it.
	const Hurt slam = HurtFrom(GameHurt{ 900.0, kStrongMelee, 0x99 }, 456.0, ids);
	CHECK(slam.attacker == 0 && slam.power);
	CHECK_NEAR(slam.share, 1.0, 1e-12);
	CHECK_EQ(HurtInput(slam).a, 10000);
	CHECK_EQ(HurtInput(slam).c, int32_t(proto::kHurtPowerAttack));
	CHECK_EQ(HurtFrom(GameHurt{ 10.0, kExplosion, 0 }, 456.0, ids).attacker, 0u);
	CHECK_NEAR(HurtFrom(GameHurt{ 10.0, kExplosion, 0 }, 0.0, ids).share, 0.0, 1e-12);  // no max health: nothing
}

TEST(CombatSumsHurtsElevenOfMinecraftsTicksApart)
{
	HurtSums   sums;
	const Hurt small{ 0.05, proto::kHurtProjectile, 4, false };
	const Hurt big{ 0.20, proto::kHurtMelee, 9, false };
	const Hurt power{ 0.01, proto::kHurtMelee, 3, true };
	const Hurt last{ 0.02, proto::kHurtMagic, 5, false };
	// The first goes at once.
	CHECK(sums.Add(small, 10.0, 50.0).has_value());
	// Within 11 ticks (0.55 s at 50 ms), the rest wait in a sum, which takes its biggest part's kind and attacker, and a
	// power attack if any part had one.
	CHECK(!sums.Add(big, 10.1, 50.0).has_value());
	CHECK(!sums.Add(power, 10.2, 50.0).has_value());
	CHECK(!sums.Add(last, 10.3, 50.0).has_value());
	CHECK(sums.Waiting());
	CHECK_EQ(sums.Summed(), uint64_t(3));
	CHECK(!sums.Due(10.54, 50.0).has_value());
	const std::optional<Hurt> sum = sums.Due(10.56, 50.0);
	CHECK(sum.has_value());
	CHECK_NEAR(sum ? sum->share : 0.0, 0.23, 1e-12);
	CHECK(sum && sum->kind == proto::kHurtMelee && sum->attacker == 9 && sum->power);
	CHECK(!sums.Waiting());
	CHECK(!sums.Due(20.0, 50.0).has_value());
	// The sum's send counts as the last: a hurt 0.3 s after it waits again, until 11 ticks after the sum.
	CHECK(!sums.Add(small, 10.85, 50.0).has_value());
	CHECK(!sums.Due(11.10, 50.0).has_value());
	CHECK(sums.Due(11.12, 50.0).has_value());
	// Long after, at once again.
	CHECK(sums.Add(small, 30.0, 50.0).has_value());
	// At 100 ms a tick, the wait is 1.1 s; a tick length outside 1 to 1,000 ms counts as 50.
	CHECK(!sums.Add(small, 31.0, 100.0).has_value());
	CHECK(!sums.Due(31.09, 100.0).has_value());
	CHECK(sums.Due(31.11, 100.0).has_value());
	CHECK(sums.Add(small, 31.67, 0.0).has_value());
}

TEST(CombatLetsMinecraftsDeathKillVInBuildModeOrJustAfter)
{
	CHECK(DeathKillsV(true, true, std::nullopt, 5.0));
	CHECK(!DeathKillsV(false, true, std::nullopt, 5.0));  // combat off
	CHECK(!DeathKillsV(true, false, std::nullopt, 5.0));  // build mode never on
	// Build mode ends as soon as Minecraft's player is dead, which can come before the event.
	CHECK(DeathKillsV(true, false, 3.0, 5.0));
	CHECK(!DeathKillsV(true, false, 2.9, 5.0));
}

TEST(CombatPutsVsGodModeOnWithBuildModeAndCombat)
{
	GodMode god;
	CHECK(!god.Frame(true, true, false).has_value());  // V not found: nothing
	// V found: off first, in case a save kept it; then on, since both are on.
	CHECK(god.Frame(true, true, true) == std::optional<bool>(false));
	CHECK(god.Frame(true, true, true) == std::optional<bool>(true));
	CHECK(god.On());
	CHECK(!god.Frame(true, true, true).has_value());
	// Build mode ends: off. Combat on first, then build mode: on.
	CHECK(god.Frame(true, false, true) == std::optional<bool>(false));
	CHECK(!god.Frame(true, false, true).has_value());
	CHECK(god.Frame(true, true, true) == std::optional<bool>(true));
	// Combat off during build mode: off.
	CHECK(god.Frame(false, true, true) == std::optional<bool>(false));
	CHECK(god.Frame(true, true, true) == std::optional<bool>(true));
	// V lost and found again, after a load: off once, then on again.
	CHECK(!god.Frame(true, true, false).has_value());
	CHECK(god.Frame(true, true, true) == std::optional<bool>(false));
	CHECK(!god.On());
	CHECK(god.Frame(true, true, true) == std::optional<bool>(true));
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestNpcRegistry.cpp
	tools/mclink/DumpObj.cpp
```
```cmake
	tests/TestNpcRegistry.cpp
	tests/TestCombat.cpp
	tools/mclink/DumpObj.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'link/Combat.h'`.

- [x] **Step 4: Write `src/link/Combat.h`**

```cpp
#pragma once

#include "link/Actors.h"
#include "link/Rings.h"

#include <cstdint>
#include <optional>
#include <string>

// Combat both ways (spike 6's "Part 2"), the parts that don't need the game: which of Minecraft's hits on NPCs count and
// how much each does; V's hurts as SkyCraft's kInHurt, summed so that Minecraft takes them all; when Minecraft's death
// kills V; and when V's god mode goes on and off. Pure: the tests give it hits and hurts of their own.
namespace cpmc::link
{
constexpr double kHitCap = 1000.0;         // Minecraft's damage, at most: /kill on a stand-in sends 3.4e38
constexpr double kStrongKnockback = 0.45;  // SkyCraft staggers above this knockback, or on a crit, comparing floats
constexpr double kDeathWindow = 2.0;       // seconds after build mode ends that Minecraft's death still kills V
constexpr int    kHurtTicks = 11;          // Minecraft's ticks between kInHurts: within 10 it takes only the excess

// A hit of Minecraft's on an NPC, as it's applied.
struct NpcHit
{
	uint32_t formId = 0;
	uint64_t entity = 0;    // the NPC, as listed
	double   damage = 0.0;  // Minecraft's, capped
	double   amount = 0.0;  // damage x V's punch
	bool     strong = false;
};

// Why a hit doesn't count.
enum class HitDrop
{
	NotListed,  // its formId wasn't listed in the last kHitTrail seconds
	NoDamage,   // knockback only, or damage that isn't finite
	NoPunch,    // V's punch isn't read, or isn't over 0
	Count,
};

const char* HitDropName(HitDrop aDrop);

// Whether a kEvHitActor counts: its NPC listed in the last kHitTrail seconds (aIds), its damage finite and over 0, capped
// at kHitCap, and V's punch, aPunch, over 0. Strong: a crit, or knockback over kStrongKnockback. Nothing, with aDrop
// saying why, if it doesn't count.
std::optional<NpcHit> AcceptHit(const proto::McEvent& aEvent, const ActorIds& aIds, double aNow, double aPunch, HitDrop& aDrop);

// What the module's Hit says about a hit it was given: 1 queued; 0 V isn't found; -1 the NPC is gone; -2 it's dead; -3
// there's no attack record to make it with.
std::string HitResultName(int32_t aResult);

// A hit on V that the module's DealDamages wrap took, in the game's terms: the health it would have taken, after V's
// armour and the difficulty; its gamedataAttackType, as a number; and its instigator's entity, or 0.
struct GameHurt
{
	double   amount = 0.0;
	int32_t  attackType = 0;
	uint64_t instigator = 0;
};

// The same, for Minecraft: its share of V's max health, kind and attacker (its stand-in's formId, or 0), and whether it's
// a power attack.
struct Hurt
{
	double          share = 0.0;
	proto::HurtKind kind = proto::kHurtOther;
	uint32_t        attacker = 0;
	bool            power = false;
};

// SkyCraft's kind for a gamedataAttackType: melee for QuickMelee, StrongMelee, Melee, GuardBreak, WhipAttack,
// ChargedWhipAttack and Reflect; projectile for Ranged, Thrown and Direct; magic for Hack; other for the rest.
proto::HurtKind HurtKindOf(int32_t aAttackType);
// A power attack: StrongMelee, ChargedWhipAttack and GuardBreak.
bool PowerAttack(int32_t aAttackType);
// The gamedataAttackType's name, "QuickMelee"; or "attack type 17" for one the game 2.31 doesn't have.
std::string AttackTypeName(int32_t aAttackType);
const char* HurtKindName(proto::HurtKind aKind);  // "melee", "projectile", "magic", "other"

// aHurt for Minecraft: its share of aMaxHealth, V's max health, held to 0 to 1; its kind and power from its attack type;
// its attacker's number if aIds has the instigator listed, else 0.
Hurt HurtFrom(const GameHurt& aHurt, double aMaxHealth, const ActorIds& aIds);

// kInHurt: code = the kind, a = the share x 10,000 rounded (the same share of Minecraft's 20 health, x 5 and x 100 for
// SkyCraft's / 5 and / 100), b = the attacker, c = the power attack flag. "Blocked" is never set: V can't block in build
// mode.
proto::InputEvent HurtInput(const Hurt& aHurt);

// Hurts summed so that Minecraft takes them all: for 10 of its ticks after a hurt it takes from another only what's more
// than the first, and it applies each at its next tick. A hurt goes at once when none went in the last kHurtTicks ticks;
// otherwise it joins a sum, which goes when those ticks are up, with its biggest part's kind and attacker, and a power
// attack if any part had one.
class HurtSums
{
public:
	// aHurt at aNow (seconds). aTickMs: Minecraft's milliseconds a tick (McState's tickMs; 50 if it isn't from 1 to 1,000).
	// It, if it goes now.
	std::optional<Hurt> Add(const Hurt& aHurt, double aNow, double aTickMs);
	// The sum, once its ticks are up.
	std::optional<Hurt> Due(double aNow, double aTickMs);

	bool     Waiting() const { return sum_.has_value(); }
	uint64_t Summed() const { return summed_; }  // hurts that joined a sum, since the start

private:
	struct Sum
	{
		Hurt   hurt;
		double biggest = 0.0;  // its biggest part's share
	};

	std::optional<double> lastSent_;
	std::optional<Sum>    sum_;
	uint64_t              summed_ = 0;
};

// Whether Minecraft's player's death kills V: with combat on, while build mode is on, or within kDeathWindow seconds of
// its end (aBuildEndedAt; build mode ends as soon as McState says Minecraft's player is dead, which can come first).
bool DeathKillsV(bool aCombat, bool aBuildMode, std::optional<double> aBuildEndedAt, double aNow);

// V's god mode, Immortal under BlockRender's own source name: on while combat and build mode are both on, whichever came
// first; off when either ends; and off once whenever V is found again after not being found, as after a load, in case a
// save kept it.
class GodMode
{
public:
	// The frame's state. The change to make now, through the module: on (true) or off (false). Nothing while V isn't found.
	std::optional<bool> Frame(bool aCombat, bool aBuildMode, bool aVFound);
	bool                On() const { return on_; }

private:
	bool on_ = false;
	bool vFound_ = false;
	bool clearDue_ = false;
};
}
```

- [x] **Step 5: Write `src/link/Combat.cpp`**

```cpp
#include "link/Combat.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>

namespace cpmc::link
{
namespace
{
// gamedataAttackType in the game 2.31, by value (the decompiled scripts' orphans.swift).
constexpr std::array<const char*, 15> kAttackTypes{ "ChargedWhipAttack", "Direct", "Effect",       "Explosion",  "ForceKill",
	                                                 "GuardBreak",        "Hack",   "Melee",        "PressureWave", "QuickMelee",
	                                                 "Ranged",            "Reflect", "StrongMelee", "Thrown",     "WhipAttack" };
enum AttackType : int32_t
{
	kChargedWhipAttack = 0,
	kDirect = 1,
	kGuardBreak = 5,
	kHack = 6,
	kMelee = 7,
	kQuickMelee = 9,
	kRanged = 10,
	kReflect = 11,
	kStrongMelee = 12,
	kThrown = 13,
	kWhipAttack = 14,
};

// The sums' wait, in seconds: kHurtTicks of Minecraft's ticks.
double Window(double aTickMs)
{
	const double tickMs = aTickMs >= 1.0 && aTickMs <= 1000.0 ? aTickMs : 50.0;
	return kHurtTicks * tickMs / 1000.0;
}
}

const char* HitDropName(HitDrop aDrop)
{
	switch (aDrop)
	{
	case HitDrop::NotListed:
		return "not listed";
	case HitDrop::NoDamage:
		return "no damage";
	case HitDrop::NoPunch:
		return "no punch";
	default:
		return "?";
	}
}

std::optional<NpcHit> AcceptHit(const proto::McEvent& aEvent, const ActorIds& aIds, double aNow, double aPunch, HitDrop& aDrop)
{
	const std::optional<uint64_t> entity = aIds.Entity(aEvent.formId, aNow);
	if (!entity)
	{
		aDrop = HitDrop::NotListed;
		return std::nullopt;
	}
	const double damage = aEvent.a;
	if (!std::isfinite(damage) || damage <= 0.0)
	{
		aDrop = HitDrop::NoDamage;
		return std::nullopt;
	}
	if (!std::isfinite(aPunch) || aPunch <= 0.0)
	{
		aDrop = HitDrop::NoPunch;
		return std::nullopt;
	}
	NpcHit hit;
	hit.formId = aEvent.formId;
	hit.entity = *entity;
	hit.damage = std::min(damage, kHitCap);
	hit.amount = hit.damage * aPunch;
	hit.strong = (aEvent.flags & proto::kHitCritical) != 0 || aEvent.d > float(kStrongKnockback);  // as SkyCraft's push > 0.45f
	return hit;
}

std::string HitResultName(int32_t aResult)
{
	switch (aResult)
	{
	case 1:
		return "queued";
	case 0:
		return "V isn't found";
	case -1:
		return "the NPC is gone";
	case -2:
		return "the NPC is dead";
	case -3:
		return "no attack record to make it with";
	default:
		return std::format("result {}", aResult);
	}
}

proto::HurtKind HurtKindOf(int32_t aAttackType)
{
	switch (aAttackType)
	{
	case kQuickMelee:
	case kStrongMelee:
	case kMelee:
	case kGuardBreak:
	case kWhipAttack:
	case kChargedWhipAttack:
	case kReflect:
		return proto::kHurtMelee;
	case kRanged:
	case kThrown:
	case kDirect:
		return proto::kHurtProjectile;
	case kHack:
		return proto::kHurtMagic;
	default:
		return proto::kHurtOther;
	}
}

bool PowerAttack(int32_t aAttackType)
{
	return aAttackType == kStrongMelee || aAttackType == kChargedWhipAttack || aAttackType == kGuardBreak;
}

std::string AttackTypeName(int32_t aAttackType)
{
	return aAttackType >= 0 && size_t(aAttackType) < kAttackTypes.size() ? kAttackTypes[size_t(aAttackType)]
	                                                                     : std::format("attack type {}", aAttackType);
}

const char* HurtKindName(proto::HurtKind aKind)
{
	switch (aKind)
	{
	case proto::kHurtMelee:
		return "melee";
	case proto::kHurtProjectile:
		return "projectile";
	case proto::kHurtMagic:
		return "magic";
	default:
		return "other";
	}
}

Hurt HurtFrom(const GameHurt& aHurt, double aMaxHealth, const ActorIds& aIds)
{
	Hurt hurt;
	hurt.share = aMaxHealth > 0.0 && std::isfinite(aHurt.amount) ? std::clamp(aHurt.amount / aMaxHealth, 0.0, 1.0) : 0.0;
	hurt.kind = HurtKindOf(aHurt.attackType);
	hurt.power = PowerAttack(aHurt.attackType);
	hurt.attacker = aHurt.instigator != 0 ? aIds.IdOf(aHurt.instigator).value_or(0) : 0;
	return hurt;
}

proto::InputEvent HurtInput(const Hurt& aHurt)
{
	proto::InputEvent input{};
	input.type = proto::kInHurt;
	input.code = aHurt.kind;
	input.a = int32_t(std::lround(std::clamp(aHurt.share, 0.0, 1.0) * 10000.0));
	input.b = int32_t(aHurt.attacker);
	input.c = aHurt.power ? int32_t(proto::kHurtPowerAttack) : 0;
	return input;
}

std::optional<Hurt> HurtSums::Add(const Hurt& aHurt, double aNow, double aTickMs)
{
	if (!sum_ && (!lastSent_ || aNow - *lastSent_ >= Window(aTickMs)))
	{
		lastSent_ = aNow;
		return aHurt;
	}
	++summed_;
	if (!sum_)
	{
		sum_ = Sum{ aHurt, aHurt.share };
		return std::nullopt;
	}
	Sum& sum = *sum_;
	if (aHurt.share > sum.biggest)
	{
		sum.biggest = aHurt.share;
		sum.hurt.kind = aHurt.kind;
		sum.hurt.attacker = aHurt.attacker;
	}
	sum.hurt.share += aHurt.share;
	sum.hurt.power = sum.hurt.power || aHurt.power;
	return std::nullopt;
}

std::optional<Hurt> HurtSums::Due(double aNow, double aTickMs)
{
	if (!sum_ || (lastSent_ && aNow - *lastSent_ < Window(aTickMs)))
	{
		return std::nullopt;
	}
	const Hurt hurt = sum_->hurt;
	sum_.reset();
	lastSent_ = aNow;
	return hurt;
}

bool DeathKillsV(bool aCombat, bool aBuildMode, std::optional<double> aBuildEndedAt, double aNow)
{
	return aCombat && (aBuildMode || (aBuildEndedAt && aNow - *aBuildEndedAt <= kDeathWindow));
}

std::optional<bool> GodMode::Frame(bool aCombat, bool aBuildMode, bool aVFound)
{
	if (!aVFound)
	{
		vFound_ = false;
		return std::nullopt;
	}
	if (!vFound_)
	{
		vFound_ = true;
		clearDue_ = true;
	}
	if (clearDue_)
	{
		clearDue_ = false;
		on_ = false;
		return false;
	}
	const bool wanted = aCombat && aBuildMode;
	if (wanted == on_)
	{
		return std::nullopt;
	}
	on_ = wanted;
	return wanted;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/link/Actors.cpp
	src/physx/GameAbi.cpp
```
```cmake
	src/link/Actors.cpp
	src/link/Combat.cpp
	src/physx/GameAbi.cpp
```

- [x] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `429 test(s), 0 failure(s)`.

- [x] **Step 8: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: which of Minecraft's hits count, V's hurts for Minecraft, their sums, deaths and god mode" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 15: `mclink` hurts Minecraft's player as the plugin will, through its sums

Step 3 sends V's hurts to Minecraft from `mclink`, without the game, as BlockRender will send them:
- **The link** reports McState's `tickMs`, Minecraft's milliseconds a tick, which the sums wait in.
- **`hurt <kind> <share> [<attacker>] [power]`,** a new script action: melee, projectile, magic or other; a share of V's
  health, over 0 and up to 1; the made-up NPC behind it, or 0; and `power` for a power attack. It goes through
  `HurtSums` and `HurtInput`, and `mclink` says whether it went at once or joined a sum, and each sum as it goes.
- **`tools/mclink/hurts.txt`,** step 3's script, at step 1's spot in survival, from full health each time: a quarter,
  melee, from nobody; a tenth, other; in diamond armour, a quarter, melee, from a stand-in 2 blocks away, and a quarter,
  other; out of the armour, a burst of five of 0.05 a tenth of a second apart, from that stand-in, the last a power
  attack; and `/kill`. It turns `immediate_respawn` on, which stays, and `keep_inventory` on for the death and off at the
  end.

**Files:**
- Create: `tools/mclink/hurts.txt`
- Modify: `src/link/Link.h`, `src/link/Link.cpp`, `tests/TestLink.cpp`, `tools/mclink/McLinkScript.h`,
  `tools/mclink/McLinkScript.cpp`, `tools/mclink/Main.cpp`, `tests/TestMcLinkScript.cpp`

- [x] **Step 1: Edit `tests/TestLink.cpp`: the tick length**

```cpp
	CHECK(link.State().drainMs >= 0.0);
```
```cpp
	CHECK(link.State().drainMs >= 0.0);
}

TEST(LinkReportsMinecraftsTickLength)
{
	FakeMinecraft minecraft;
	Link          link(minecraft.base);
	Sink          sink;
	minecraft.Beat(77, 1000);
	minecraft.Ticked(0, { -119.5, 6.14, 2007.5 }, 1.0);
	link.Update(At(1.0), kSettings, sink);
	CHECK_NEAR(link.State().minecraftTickMs, 50.0, 1e-9);
```

- [x] **Step 2: Edit `tests/TestMcLinkScript.cpp`: the hurt action, and step 3's script**

```cpp
	for (const char* name : { "step1.txt", "again.txt", "eye.txt", "drive.txt", "relink.txt", "city.txt", "npcs.txt" })
```
```cpp
	for (const char* name : { "step1.txt", "again.txt", "eye.txt", "drive.txt", "relink.txt", "city.txt", "npcs.txt", "hurts.txt" })
```
```cpp
TEST(ScriptParsesMadeUpNpcs)
```
```cpp
TEST(ScriptParsesHurts)
{
	std::string error;
	const auto  actions = ParseScript("hurt melee 0.25\nhurt projectile 0.05 3 power\nhurt other 1 0\nhurt magic 0.1 power\n", error);
	CHECK(actions.has_value());
	CHECK_EQ(actions ? actions->size() : 0, size_t(4));
	if (actions && actions->size() == 4)
	{
		const auto& a = *actions;
		CHECK(a[0].kind == Kind::Hurt);
		CHECK_EQ(a[0].numbers[0], 0.0);  // melee
		CHECK_EQ(a[0].numbers[1], 0.25);
		CHECK_EQ(a[0].numbers[2], 0.0);  // no attacker
		CHECK(!a[0].on);
		CHECK_EQ(a[1].numbers[0], 1.0);  // projectile
		CHECK_EQ(a[1].numbers[2], 3.0);
		CHECK(a[1].on);  // a power attack
		CHECK_EQ(a[2].numbers[0], 3.0);  // other
		CHECK_EQ(a[2].numbers[1], 1.0);
		CHECK_EQ(a[3].numbers[0], 2.0);  // magic
		CHECK(a[3].on);
	}
	CHECK(!ParseScript("hurt fire 0.1", error).has_value());
	CHECK(!ParseScript("hurt melee 0", error).has_value());
	CHECK(!ParseScript("hurt melee 1.5", error).has_value());
	CHECK(!ParseScript("hurt melee 0.1 1.5", error).has_value());
	CHECK(!ParseScript("hurt melee 0.1 3 hard", error).has_value());
	CHECK(!ParseScript("hurt melee", error).has_value());
	CHECK(!ParseScript("hurt melee 0.1 3 power extra", error).has_value());
}

TEST(ScriptParsesMadeUpNpcs)
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'minecraftTickMs': is not a member of 'cpmc::link::Link::Status'`, and
`error C2065: 'Hurt': undeclared identifier`.

- [x] **Step 4: Edit `src/link/Link.h`**

```cpp
		uint32_t              minecraftFlags = 0;  // McState's: on ground, sneaking, sprinting, flying and the rest
```
```cpp
		uint32_t              minecraftFlags = 0;  // McState's: on ground, sneaking, sprinting, flying and the rest
		float                 minecraftTickMs = 0.0f;  // McState's: Minecraft's milliseconds a tick (spike 6's sums)
```

- [x] **Step 5: Edit `src/link/Link.cpp`**

```cpp
		status_.minecraftFlags = mc->flags;
```
```cpp
		status_.minecraftFlags = mc->flags;
		status_.minecraftTickMs = mc->tickMs;
```

- [x] **Step 6: Edit `tools/mclink/McLinkScript.h`**

```cpp
//   actor-remove <id>|all          one made-up NPC out of the table, or all of them
```
```cpp
//   actor-remove <id>|all          one made-up NPC out of the table, or all of them
//   hurt <kind> <share> [<attacker>] [power]
//                                  V hurt, as the plugin sends it (spike 6's Part 2): melee, projectile, magic or other;
//                                  its share of V's health, over 0 and up to 1; the made-up NPC that did it, or 0; and
//                                  power for a power attack. It goes through the plugin's sums: at once, or in a sum
```
```cpp
		ActorRemove,
```
```cpp
		ActorRemove,
		Hurt,
```
```cpp
	                      // actor's number, x, y, z, yaw, width and height; the actor to remove, or 0 for all
	bool        on = false;  // link, build and hud
```
```cpp
	                      // actor's number, x, y, z, yaw, width and height; the actor to remove, or 0 for all; a hurt's
	                      // kind (proto::HurtKind), share and attacker
	bool        on = false;  // link, build and hud; a hurt's power attack
```

- [x] **Step 7: Edit `tools/mclink/McLinkScript.cpp`**

```cpp
#include <charconv>
```
```cpp
#include <algorithm>
#include <array>
#include <charconv>
```
```cpp
		}
		else if (verb == "floor" || verb == "status" || verb == "quit")
```
```cpp
		}
		else if (verb == "hurt")
		{
			action.kind = Action::Kind::Hurt;
			size_t given = words.size();
			action.on = given > 3 && words[given - 1] == "power";
			given -= action.on ? 1 : 0;
			const std::array<std::string_view, 4> kinds{ "melee", "projectile", "magic", "other" };
			const auto kind = given >= 2 ? std::find(kinds.begin(), kinds.end(), words[1]) : kinds.end();
			bool       ok = kind != kinds.end() && (given == 3 || given == 4) && Number(words[2], action.numbers[1]) &&
			          action.numbers[1] > 0.0 && action.numbers[1] <= 1.0;
			if (ok && given == 4)
			{
				ok = Number(words[3], action.numbers[2]) && (action.numbers[2] == 0.0 || WholeId(action.numbers[2]));
			}
			if (!ok)
			{
				return fail("hurt takes melee, projectile, magic or other; a share over 0 and up to 1; then an actor's number, or 0; "
				            "then power");
			}
			action.numbers[0] = double(kind - kinds.begin());
		}
		else if (verb == "floor" || verb == "status" || verb == "quit")
```

- [x] **Step 8: Edit `tools/mclink/Main.cpp`: hurts through the sums**

```cpp
#include "link/Actors.h"
```
```cpp
#include "link/Actors.h"
#include "link/Combat.h"
```
```cpp
	std::map<uint32_t, proto::ActorRecord> actors;  // the made-up NPCs, by number
```
```cpp
	std::map<uint32_t, proto::ActorRecord> actors;  // the made-up NPCs, by number
	HurtSums                               sums;    // V's hurts, as the plugin sums them
	const auto send = [&](const Hurt& aHurt, const char* aHow) {
		const proto::InputEvent input = HurtInput(aHurt);
		const bool              sent = link.Push(input);
		Say("hurt {}: {} {:.4f} of V's health, a = {}, attacker {}{}{}", aHow, HurtKindName(aHurt.kind), aHurt.share, input.a, aHurt.attacker,
		    aHurt.power ? ", power attack" : "", sent ? "" : "; DROPPED, the input ring is full");
	};
```
```cpp
		const auto& state = link.State();
```
```cpp
		const auto& state = link.State();
		if (const std::optional<Hurt> due = sums.Due(now, state.minecraftTickMs))
		{
			send(*due, "sent, summed");
		}
```
```cpp
				break;
			case Action::Kind::Status: PrintStatus(link, report, feet); break;
```
```cpp
				break;
			case Action::Kind::Hurt:
			{
				Hurt hurt;
				hurt.kind = proto::HurtKind(action.numbers[0]);
				hurt.share = action.numbers[1];
				hurt.attacker = uint32_t(action.numbers[2]);
				hurt.power = action.on;
				if (const std::optional<Hurt> now_ = sums.Add(hurt, now, link.State().minecraftTickMs))
				{
					send(*now_, "sent at once");
				}
				else
				{
					Say("hurt joins a sum: {} {:.4f} of V's health, attacker {}{}", HurtKindName(hurt.kind), hurt.share, hurt.attacker,
					    hurt.power ? ", power attack" : "");
				}
				break;
			}
			case Action::Kind::Status: PrintStatus(link, report, feet); break;
```

- [x] **Step 9: Write step 3's script, `tools/mclink/hurts.txt`**

```text
# Spike 6's step 3: Minecraft's player hurt as the plugin will hurt it for V, through its sums, without the game. At step
# 1's spot, in survival, each from full health: a quarter of V's health, melee, from nobody, then a tenth, other; then in
# diamond armour, a quarter, melee, from a stand-in 2 blocks away (the armour takes some) and a quarter, other (it takes
# none); out of the armour, a burst of five, summed; and /kill, which comes back as Minecraft's player's death. Start
# Minecraft first; this waits for its world. immediate_respawn stays on in the world; keep_inventory is on for the death,
# and off at the end.
status
wait-world 180
at 520.5 6.14 2007.5
look 180 0
link on
wait 3
type /gamerule immediate_respawn true
type /gamerule keep_inventory true
type /gamemode survival
# Full health, then a quarter of V's health, melee, from nobody: 5 of Minecraft's 20.
type /effect give @s minecraft:instant_health 1 10
wait 2
hurt melee 0.25
wait 2
# A tenth, other: 2.
type /effect give @s minecraft:instant_health 1 10
wait 2
hurt other 0.1
wait 2
# Diamond armour, and a stand-in 2 blocks north: a quarter, melee, from it, which the armour reduces; then a quarter,
# other, which passes the armour.
actor 1 520.5 6.14 2005.5 0 0.6 1.8 Ganger One
type /item replace entity @s armor.chest with minecraft:diamond_chestplate
type /item replace entity @s armor.legs with minecraft:diamond_leggings
type /effect give @s minecraft:instant_health 1 10
wait 2
hurt melee 0.25 1
wait 2
type /effect give @s minecraft:instant_health 1 10
wait 2
hurt other 0.25
wait 2
# Out of the armour, a burst of five, 0.05 each, a tenth of a second apart, from the stand-in, the last a power attack:
# the first at once, the other four in one sum 11 ticks later.
type /item replace entity @s armor.chest with minecraft:air
type /item replace entity @s armor.legs with minecraft:air
type /effect give @s minecraft:instant_health 1 10
wait 2
hurt projectile 0.05 1
wait 0.1
hurt projectile 0.05 1
wait 0.1
hurt projectile 0.05 1
wait 0.1
hurt projectile 0.05 1
wait 0.1
hurt projectile 0.05 1 power
wait 2
# Death: /kill comes back as the event, killed by none of the actors, and Minecraft's player respawns at once.
type /kill @s
wait 3
actor-remove all
type /gamerule keep_inventory false
type /gamemode creative
status
quit
```

- [x] **Step 10: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `431 test(s), 0 failure(s)`.

- [x] **Step 11: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: mclink hurts Minecraft's player as the plugin will, through its sums" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 16: In Minecraft: step 3

Step 3 of the spec's "The steps", with the user: Minecraft's player hurt as BlockRender will hurt it for V, from
`mclink`, against SkyCraft's unchanged mod, without the game.
- **What SkyCraft logs** for each hurt (its `SkyCombat.hurtPlayer`, at 0.1.2): `SkyCraft: Skyrim hit the player for
  <Skyrim damage> (<Minecraft damage> Minecraft): health <before> -> <after>`, with ` (blocked/immune)` after it when
  nothing was taken. The Skyrim damage is `a` ÷ 100, the share × 100; Minecraft's is that ÷ 5. Armour and a shield
  reduce only melee and projectile hurts whose attacker has a stand-in within 24 blocks (the spec's "Minecraft 26.3").
- **The death** comes back as `kEvPlayerDied`, which `mclink` prints.
- **If a check fails,** stop and find the cause with superpowers:systematic-debugging before going on. Fixes become new
  tasks.

**Files:**
- Modify: `docs/spikes/2026-10-05-npcs-and-combat-results.md`

- [x] **Step 1: Check that the game isn't running**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game: it would hold the mapping.

- [x] **Step 2: Ask the user about `keep_inventory`, then to start Minecraft**

The script kills Minecraft's player once. Ask the user whether `keep_inventory` may be on for that death and off at the
end, as `hurts.txt` does, so nothing is dropped; if their world keeps it on already, delete the script's line `type
/gamerule keep_inventory false` first. Then the user starts the `CyberCraft` instance from the Modrinth App, and says
when its title screen shows.

- [x] **Step 3: Run step 3's script**

```bash
L="/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log"; before=$(wc -l < "$L"); \
  cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/step3 && cd build/step3 \
  && ../RelWithDebInfo/mclink.exe ../../tools/mclink/hurts.txt | tee hurts.log; echo "exit: ${PIPESTATUS[0]}"; \
  tail -n +$((before + 1)) "$L" | grep -E "SkyCraft: Skyrim hit|\[CHAT\]" | tee minecraft.log
```
Give the command 6 minutes. Expected, against the spec's "Step 3":
- **Each hurt's share:** `hurt sent at once: melee 0.2500 of V's health, a = 2500, attacker 0` and `SkyCraft: Skyrim hit
  the player for 25.0 (5.0 Minecraft): health 20.0 -> 15.0`; then `hurt sent at once: other 0.1000 of V's health, a =
  1000, attacker 0` and `… for 10.0 (2.0 Minecraft): health 20.0 -> 18.0`.
- **Armour:** in diamond armour, the melee hurt from actor 1, `… for 25.0 (5.0 Minecraft): health 20.0 -> …`,
  takes less than 5 (about 17.5 is left); the other hurt, `… for 25.0 (5.0 Minecraft): health 20.0 -> 15.0`, takes
  all 5.
- **The burst:** `hurt sent at once: projectile 0.0500 …`, four `hurt joins a sum: …`, then `hurt sent, summed:
  projectile 0.2000 of V's health, a = 2000, attacker 1, power attack`; SkyCraft's `… for 5.0 (1.0 Minecraft): health
  20.0 -> 19.0` and `… for 20.0 (4.0 Minecraft): health 19.0 -> 15.0`: all five landed.
- **The death:** `Minecraft's player died, killed by none of the actors`.
- **The status:** `events: 0 explosions, 0 hits on actors, 1 deaths, …` (the others are SkyCraft's armour skill
  events, drained and counted), `0 inputs dropped`, and `exit: 0`.

- [x] **Step 4: Ask the user to stop Minecraft**

Its window is hidden while `mclink` drives it, so the user stops it from the Modrinth App.

- [x] **Step 5: Write step 3's results in `docs/spikes/2026-10-05-npcs-and-combat-results.md`**

A "Step 3" section, from `build/step3/hurts.log` and `build/step3/minecraft.log`: the versions; each check above with
its numbers (each hurt's share, what Minecraft took, the armour's part, the burst's two sends); anything that
surprised, and what it means for step 4.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-05-npcs-and-combat-results.md \
  && git commit -q -m "npcs-and-combat: step 3's results" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 17: The combat inbox, and the natives the damage pipeline's wraps call

The damage pipeline may run on any thread, so what its wraps report comes to BlockRender through natives, behind a
lock, as the NPC list does:
- **`CombatInbox`** keeps the hits on V the wrap offers, taken only while the main thread says so (combat and build
  mode on, V found, V's max health read), and at most 256 waiting; a hit it doesn't take stays V's. It keeps our hits
  as they land, with the NPC's health before and after, for the status. The main thread takes both each frame.
- **Two more natives:** `BlockRenderVHurt(amount, attackType, instigator) -> Bool`, true when taken, and
  `BlockRenderHitLanded(target, amount, before, after)`. `GameScript` checks all six natives are there.

**Files:**
- Create: `src/CombatInbox.h`, `src/CombatInbox.cpp`, `tests/TestCombatInbox.cpp`
- Modify: `CMakeLists.txt`, `src/Natives.h`, `src/Natives.cpp`, `src/GameScript.cpp`

- [x] **Step 1: Write the failing test, `tests/TestCombatInbox.cpp`**

```cpp
#include "Harness.h"

#include "CombatInbox.h"

#include <thread>

using namespace cpmc;

TEST(CombatInboxTakesHurtsOnlyWhileTaking)
{
	CombatInbox inbox;
	// Not taking: the hit is left to V.
	CHECK(!inbox.Offer({ 45.0, 10, 0x51 }));
	CHECK(inbox.TakeHurts().empty());
	inbox.SetTaking(true);
	CHECK(inbox.Taking());
	CHECK(inbox.Offer({ 45.0, 10, 0x51 }));
	CHECK(inbox.Offer({ 12.0, 9, 0 }));
	const std::vector<link::GameHurt> hurts = inbox.TakeHurts();
	CHECK_EQ(hurts.size(), size_t(2));
	CHECK(hurts.size() == 2 && hurts[0].amount == 45.0 && hurts[0].attackType == 10 && hurts[0].instigator == 0x51);
	CHECK(hurts.size() == 2 && hurts[1].amount == 12.0);
	CHECK(inbox.TakeHurts().empty());
	// With kMaxWaiting waiting, the next is left to V rather than lost.
	for (size_t i = 0; i < CombatInbox::kMaxWaiting; ++i)
	{
		CHECK(inbox.Offer({ 1.0, 9, 0 }));
	}
	CHECK(!inbox.Offer({ 1.0, 9, 0 }));
	inbox.SetTaking(false);
	CHECK(!inbox.Offer({ 1.0, 9, 0 }));
	const CombatInbox::Counts counts = inbox.Count();
	CHECK_EQ(counts.taken, uint64_t(2 + CombatInbox::kMaxWaiting));
	CHECK_EQ(counts.refused, uint64_t(3));
	CHECK_EQ(inbox.TakeHurts().size(), CombatInbox::kMaxWaiting);
}

TEST(CombatInboxKeepsLandingsUntilTaken)
{
	CombatInbox inbox;
	inbox.Land({ 7, 714.0, 1514.0, 800.0 });
	inbox.Land({ 8, 102.0, 48.0, 0.0 });
	const std::vector<CombatInbox::Landed> landed = inbox.TakeLanded();
	CHECK_EQ(landed.size(), size_t(2));
	CHECK(landed.size() == 2 && landed[0].target == 7 && landed[0].amount == 714.0 && landed[0].before == 1514.0 &&
	      landed[0].after == 800.0);
	CHECK(landed.size() == 2 && landed[1].target == 8);
	CHECK(inbox.TakeLanded().empty());
	for (size_t i = 0; i <= CombatInbox::kMaxWaiting; ++i)
	{
		inbox.Land({ 9, 1.0, 2.0, 1.0 });
	}
	CHECK_EQ(inbox.TakeLanded().size(), CombatInbox::kMaxWaiting);
	CHECK_EQ(inbox.Count().landed, uint64_t(2 + CombatInbox::kMaxWaiting));
	CHECK_EQ(inbox.Count().dropped, uint64_t(1));
}

// As the game does: the pipeline on several threads at once, while the main thread takes what they report.
TEST(CombatInboxTakesReportsFromManyThreadsAtOnce)
{
	constexpr int            kThreads = 8;
	constexpr int            kEach = 2000;
	CombatInbox              inbox;
	std::vector<std::thread> threads;
	std::atomic<int>         taken{ 0 };
	inbox.SetTaking(true);
	for (int t = 0; t < kThreads; ++t)
	{
		threads.emplace_back([&inbox, &taken, t] {
			for (int i = 0; i < kEach; ++i)
			{
				if (inbox.Offer({ 1.0, 9, uint64_t(t + 1) }))
				{
					++taken;
				}
				inbox.Land({ uint64_t(t + 1), 1.0, 2.0, 1.0 });
			}
		});
	}
	size_t hurts = 0;
	size_t landed = 0;
	for (int read = 0; read < 2000; ++read)
	{
		hurts += inbox.TakeHurts().size();
		landed += inbox.TakeLanded().size();
	}
	for (std::thread& thread : threads)
	{
		thread.join();
	}
	hurts += inbox.TakeHurts().size();
	landed += inbox.TakeLanded().size();
	const CombatInbox::Counts counts = inbox.Count();
	CHECK_EQ(hurts, size_t(taken.load()));
	CHECK_EQ(counts.taken + counts.refused, uint64_t(kThreads * kEach));
	CHECK_EQ(counts.taken, uint64_t(taken.load()));
	CHECK_EQ(uint64_t(landed), counts.landed);
	CHECK_EQ(counts.landed + counts.dropped, uint64_t(kThreads * kEach));
}
```

- [x] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestCombat.cpp
	tools/mclink/DumpObj.cpp
```
```cmake
	tests/TestCombat.cpp
	tests/TestCombatInbox.cpp
	tools/mclink/DumpObj.cpp
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'CombatInbox.h'`.

- [x] **Step 4: Write `src/CombatInbox.h`**

```cpp
#pragma once

#include "link/Combat.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace cpmc
{
// What the module's damage-pipeline wraps report through BlockRender's natives (spike 6's "Part 2"), on whichever thread
// the game runs its damage pipeline on: hits on V, taken only while the main thread says so, so that V's health stays
// V's otherwise; and our hits on NPCs as they land, with the NPC's health before and after. The main thread takes both
// each frame.
class CombatInbox
{
public:
	static constexpr size_t kMaxWaiting = 256;  // of each: a hurt past it is left to V, a landing is only counted

	// One of our hits, as DealDamages dealt it.
	struct Landed
	{
		uint64_t target = 0;
		double   amount = 0.0;
		double   before = 0.0;  // the NPC's health
		double   after = 0.0;
	};

	struct Counts
	{
		uint64_t taken = 0;    // hits on V taken from the game
		uint64_t refused = 0;  // offered while not taking, or with kMaxWaiting waiting: left to V
		uint64_t landed = 0;   // our hits reported as dealt
		uint64_t dropped = 0;  // landings past kMaxWaiting
	};

	void SetTaking(bool aTaking);  // main thread: with combat and build mode on
	bool Taking() const;
	// Any thread: a hit on V, taken (true), so the wrap deals none of it, or left to V.
	bool Offer(const link::GameHurt& aHurt);
	void Land(const Landed& aLanded);  // any thread

	std::vector<link::GameHurt> TakeHurts();  // main thread, oldest first
	std::vector<Landed>         TakeLanded();
	Counts                      Count() const;

private:
	std::atomic<bool>           taking_{ false };
	mutable std::mutex          mutex_;
	std::vector<link::GameHurt> hurts_;
	std::vector<Landed>         landed_;
	Counts                      counts_;
};
}
```

- [x] **Step 5: Write `src/CombatInbox.cpp`**

```cpp
#include "CombatInbox.h"

#include <utility>

namespace cpmc
{
void CombatInbox::SetTaking(bool aTaking)
{
	taking_.store(aTaking, std::memory_order_release);
}

bool CombatInbox::Taking() const
{
	return taking_.load(std::memory_order_acquire);
}

bool CombatInbox::Offer(const link::GameHurt& aHurt)
{
	const std::lock_guard lock(mutex_);
	if (!Taking() || hurts_.size() >= kMaxWaiting)
	{
		++counts_.refused;
		return false;
	}
	hurts_.push_back(aHurt);
	++counts_.taken;
	return true;
}

void CombatInbox::Land(const Landed& aLanded)
{
	const std::lock_guard lock(mutex_);
	if (landed_.size() >= kMaxWaiting)
	{
		++counts_.dropped;
		return;
	}
	landed_.push_back(aLanded);
	++counts_.landed;
}

std::vector<link::GameHurt> CombatInbox::TakeHurts()
{
	const std::lock_guard lock(mutex_);
	return std::exchange(hurts_, {});
}

std::vector<CombatInbox::Landed> CombatInbox::TakeLanded()
{
	const std::lock_guard lock(mutex_);
	return std::exchange(landed_, {});
}

CombatInbox::Counts CombatInbox::Count() const
{
	const std::lock_guard lock(mutex_);
	return counts_;
}
}
```

- [x] **Step 6: Edit `CMakeLists.txt` to add the source**

```cmake
	src/NpcRegistry.cpp
)
```
```cmake
	src/NpcRegistry.cpp
	src/CombatInbox.cpp
)
```

- [x] **Step 7: Edit `src/Natives.h`**

```cpp
#include "LogQueue.h"
```
```cpp
#include "CombatInbox.h"
#include "LogQueue.h"
```
```cpp
// those on, and its List reads the list back on the main thread (spike 6's "The module's list"). They're added once the
// game has registered its own types, which is before it loads its scripts.
```
```cpp
// those on, and its List reads the list back on the main thread (spike 6's "The module's list"). Its wraps of the damage
// pipeline offer hits on V and report our hits landing through them, on whichever thread runs the pipeline (Part 2).
// They're added once the game has registered its own types, which is before it loads its scripts.
```
```cpp
NpcRegistry& NpcsReported();
```
```cpp
NpcRegistry& NpcsReported();
// The hits on V the pipeline's wraps offer, and our hits as they land.
CombatInbox& CombatReported();
```

- [x] **Step 8: Edit `src/Natives.cpp`: the two natives**

```cpp
NpcRegistry g_reported;
```
```cpp
NpcRegistry g_reported;
CombatInbox g_combat;
```
```cpp
		aOut->hash = g_reported.At(index);
	}
}
```
```cpp
		aOut->hash = g_reported.At(index);
	}
}

// BlockRenderVHurt(amount: Float, attackType: Int32, instigator: EntityID) -> Bool, from the wrap of DealDamages, on any
// thread: a hit on V, taken (true) while combat and build mode are on, so the wrap deals none of it.
void VHurt(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
{
	float                  amount = 0.0f;
	int32_t                attackType = 0;
	RED4ext::ent::EntityID instigator;
	RED4ext::GetParameter(aFrame, &amount);
	RED4ext::GetParameter(aFrame, &attackType);
	RED4ext::GetParameter(aFrame, &instigator);
	++aFrame->code;
	const bool taken = g_combat.Offer({ double(amount), attackType, instigator.hash });
	if (aOut)
	{
		*aOut = taken;
	}
}

// BlockRenderHitLanded(target: EntityID, amount: Float, before: Float, after: Float), from the wrap of DealDamages, on any
// thread: one of our hits, dealt, with the NPC's health before and after.
void HitLanded(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, void*, int64_t)
{
	RED4ext::ent::EntityID target;
	float                  amount = 0.0f;
	float                  before = 0.0f;
	float                  after = 0.0f;
	RED4ext::GetParameter(aFrame, &target);
	RED4ext::GetParameter(aFrame, &amount);
	RED4ext::GetParameter(aFrame, &before);
	RED4ext::GetParameter(aFrame, &after);
	++aFrame->code;
	g_combat.Land({ target.hash, double(amount), double(before), double(after) });
}
```
```cpp
	typed = Add<RED4ext::ent::EntityID*>(*rtti, "BlockRenderNpcAt", &NpcAt, { { "Int32", "i" } }, "entEntityID") && typed;
	if (g_nativesLog)
	{
		g_nativesLog->Push(typed ? "natives: BlockRenderNpcAttached, BlockRenderNpcDetached, BlockRenderNpcCount and BlockRenderNpcAt added"
```
```cpp
	typed = Add<RED4ext::ent::EntityID*>(*rtti, "BlockRenderNpcAt", &NpcAt, { { "Int32", "i" } }, "entEntityID") && typed;
	typed = Add<bool*>(*rtti, "BlockRenderVHurt", &VHurt, { { "Float", "amount" }, { "Int32", "attackType" }, { "entEntityID", "instigator" } },
	                   "Bool") &&
	        typed;
	typed = Add<void*>(*rtti, "BlockRenderHitLanded", &HitLanded,
	                   { { "entEntityID", "target" }, { "Float", "amount" }, { "Float", "before" }, { "Float", "after" } }, nullptr) &&
	        typed;
	if (g_nativesLog)
	{
		g_nativesLog->Push(typed ? "natives: BlockRenderNpcAttached, BlockRenderNpcDetached, BlockRenderNpcCount, BlockRenderNpcAt, "
		                           "BlockRenderVHurt and BlockRenderHitLanded added"
```
```cpp
	return g_reported;
}
}
```
```cpp
	return g_reported;
}

CombatInbox& CombatReported()
{
	return g_combat;
}
}
```

- [x] **Step 9: Edit `src/GameScript.cpp`: six natives to check for**

```cpp
constexpr std::array<const char*, 4> kNatives{ "BlockRenderNpcAttached", "BlockRenderNpcDetached", "BlockRenderNpcCount",
	                                           "BlockRenderNpcAt" };
```
```cpp
constexpr std::array<const char*, 6> kNatives{ "BlockRenderNpcAttached", "BlockRenderNpcDetached", "BlockRenderNpcCount",
	                                           "BlockRenderNpcAt",       "BlockRenderVHurt",       "BlockRenderHitLanded" };
```

- [x] **Step 10: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `434 test(s), 0 failure(s)`.

- [x] **Step 11: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: the combat inbox, and the natives the damage pipeline's wraps call" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 18: The module makes Minecraft's hits V's, takes V's hurts, and keeps V's god mode and death

The module's side of Part 2, in `scripts/BlockRender.reds`, version 3:
- **`Hit(id, amount, strong)`** builds a hit as the game's `SendHitEvent` does (`triggerAttackOnNearbyEnemiesEffector`):
  an `AttackInitContext` from V, as instigator and source, with the attack record of what V punches with and its
  weapon object if V has it drawn; `IAttack.Create`; a `gameHitEvent` with a new `AttackData`, the NPC as its target,
  its position at the NPC's middle and a hit shape there, its direction from V; then `QueueHitEvent`. The record is from
  the item in the `BaseFists` area, its `WeaponItem_Record.Attacks()`: its `StrongMelee` attack for a strong hit, its
  light one, `Melee`, otherwise (`QuickMelee` is a gun's quick melee: `attackData.swift`'s `IsLightMelee`); of neither,
  its first melee attack. 1: queued; 0: V isn't found; -1: the NPC is gone; -2: dead; -3: no attack record.
- **Ours, on the hit itself:** the game marks its environment's attacks with `hitFlag.None` and a source name
  (`environmentDamageSystem.swift`); the module marks ours `hitFlag.None` under `n"BlockRender <amount>"`, and
  `AmountOf` reads the amount back. The pipeline's wraps read only the hit, on whichever thread runs it.
- **`Process` is wrapped:** for ours, `DealNoDamage` under `n"no_valid_damage"` comes off; any other reason stays.
- **`DealDamages` is wrapped:** ours deals exactly its amount, as physical damage, and `BlockRenderHitLanded` reports
  the NPC's health before and after. A hit on V with health damage is offered to `BlockRenderVHurt`; taken, it deals
  nothing.
- **`SetImmortal(on)`** adds or removes Immortal on V under `n"BlockRender"` only; **`KillV(killer)`** removes ours and
  sets V's health to its least with the killer as instigator, as the pipeline's `ProcessInstantKill` does; **`Punch()`**
  says V's punch, its item, and the attack records hits are made with.

`Process`'s and `DealDamages`'s signatures are `damageSystem.swift`'s; a wrap of a method that isn't there fails the
compile, which Step 3 checks.

**Files:**
- Modify: `scripts/BlockRender.reds`

- [x] **Step 1: Edit `scripts/BlockRender.reds`**

```swift
// BlockRender, and keeps no reference to an NPC from one call to the next: each is found again by its EntityID.
```
```swift
// BlockRender, and keeps no reference to an NPC from one call to the next: each is found again by its EntityID.
//
// Combat (spike 6's Part 2): Hit makes one of Minecraft's hits into a hit of V's on an NPC, marked as ours with its
// amount on the hit itself, and the wraps of the damage pipeline's Process and DealDamages, at the end of this file, deal
// it exactly; the same DealDamages wrap offers the game's hits on V to BlockRender, which takes them for Minecraft in
// build mode. The pipeline may run on any thread, so those wraps too touch nothing of the module's: they read the hit
// itself, and call BlockRender's natives.
```
```swift
public native func BlockRenderNpcAt(i: Int32) -> EntityID
```
```swift
public native func BlockRenderNpcAt(i: Int32) -> EntityID
// For the DealDamages wrap: a hit on V, which BlockRender takes (true) for Minecraft while combat and build mode are on;
// and one of our hits, dealt, with the NPC's health before and after.
public native func BlockRenderVHurt(amount: Float, attackType: Int32, instigator: EntityID) -> Bool
public native func BlockRenderHitLanded(target: EntityID, amount: Float, before: Float, after: Float)
```
```swift
  // The module's version, which BlockRender checks: 2 since the list moved into BlockRender.
  public final static func Version() -> Int32 {
    return 2;
```
```swift
  // The module's version, which BlockRender checks: 2 since the list moved into BlockRender, 3 with combat.
  public final static func Version() -> Int32 {
    return 3;
```
```swift
  private final func Know(id: EntityID) -> Void {
```
```swift
  // One of Minecraft's hits on an NPC, as V's: amount is what it takes off the NPC's health, and strong picks the strong
  // attack. Built as the game's SendHitEvent builds one, from V, with the attack record of what V punches with, and the
  // weapon object of it if V has it drawn. 1: queued; 0: V isn't found; -1: the NPC is gone; -2: it's dead; -3: there's no
  // attack record to make it with.
  public final func Hit(id: EntityID, amount: Float, strong: Bool) -> Int32 {
    let game: GameInstance = this.GetGameInstance();
    let player: ref<GameObject> = GameInstance.GetPlayerSystem(game).GetLocalPlayerMainGameObject();
    if !IsDefined(player) {
      return 0;
    };
    let npc: ref<NPCPuppet> = GameInstance.FindEntityByID(game, id) as NPCPuppet;
    if !IsDefined(npc) {
      return -1;
    };
    if npc.IsDead() {
      return -2;
    };
    let record: ref<Attack_Record> = this.PunchAttack(player, strong);
    if !IsDefined(record) {
      return -3;
    };
    let weapon: ref<WeaponObject> = this.PunchWeapon(player, game);
    let context: AttackInitContext;
    context.record = record;
    context.instigator = player;
    context.source = player;
    context.weapon = weapon;
    let hit: ref<gameHitEvent> = new gameHitEvent();
    hit.attackData = new AttackData();
    hit.target = npc;
    hit.attackData.SetAttackDefinition(IAttack.Create(context));
    hit.attackData.SetInstigator(player);
    hit.attackData.SetSource(player);
    hit.attackData.SetWeapon(weapon);
    hit.attackData.SetAttackType(record.AttackType().Type());
    // Ours, with its amount, on the hit itself, as the game marks its environment's attacks with hitFlag.None and a
    // source name: the pipeline's wraps read it back on whichever thread runs it.
    let mark: String = "BlockRender " + FloatToStringPrec(amount, 2);
    hit.attackData.AddFlag(hitFlag.None, StringToName(mark));
    let middle: Vector4 = npc.GetWorldPosition() + new Vector4(0.00, 0.00, 1.00, 0.00);
    hit.hitPosition = middle;
    hit.hitDirection = Vector4.Normalize(middle - player.GetWorldPosition());
    let shape: HitShapeData;
    shape.result.hitPositionEnter = middle;
    shape.result.hitPositionExit = middle;
    ArrayPush(hit.hitRepresentationResult.hitShapes, shape);
    GameInstance.GetDamageSystem(game).QueueHitEvent(hit, npc);
    return 1;
  }

  // V's god mode for build mode: Immortal on V, added or removed under BlockRender's own source name only. Whether the
  // game says it changed.
  public final func SetImmortal(on: Bool) -> Bool {
    let game: GameInstance = this.GetGameInstance();
    let player: ref<GameObject> = GameInstance.GetPlayerSystem(game).GetLocalPlayerMainGameObject();
    if !IsDefined(player) {
      return false;
    };
    let gods: ref<GodModeSystem> = GameInstance.GetGodModeSystem(game);
    if on {
      return gods.AddGodMode(player.GetEntityID(), gameGodModeType.Immortal, n"BlockRender");
    };
    return gods.RemoveGodMode(player.GetEntityID(), gameGodModeType.Immortal, n"BlockRender");
  }

  // Minecraft's death, as V's: our Immortal off, then V's health to its least, as the pipeline's own instant kill does,
  // killer as the instigator if it's still there. False if V isn't found, or another god mode keeps V alive.
  public final func KillV(killer: EntityID) -> Bool {
    let game: GameInstance = this.GetGameInstance();
    let player: ref<GameObject> = GameInstance.GetPlayerSystem(game).GetLocalPlayerMainGameObject();
    if !IsDefined(player) {
      return false;
    };
    let id: EntityID = player.GetEntityID();
    let gods: ref<GodModeSystem> = GameInstance.GetGodModeSystem(game);
    gods.RemoveGodMode(id, gameGodModeType.Immortal, n"BlockRender");
    let instigator: ref<GameObject>;
    if EntityID.IsDefined(killer) {
      instigator = GameInstance.FindEntityByID(game, killer) as GameObject;
    };
    GameInstance.GetStatPoolsSystem(game).RequestSettingStatPoolMinValue(Cast<StatsObjectID>(id), gamedataStatPoolType.Health, instigator);
    return !gods.HasGodMode(id, gameGodModeType.Immortal) && !gods.HasGodMode(id, gameGodModeType.Invulnerable);
  }

  // For the punch request: V's punch, what it's read from, and what Hit makes hits with.
  public final func Punch() -> String {
    let game: GameInstance = this.GetGameInstance();
    let player: ref<GameObject> = GameInstance.GetPlayerSystem(game).GetLocalPlayerMainGameObject();
    if !IsDefined(player) {
      return "V isn't found";
    };
    this.ReadV(player, game);
    let drawn: ref<WeaponObject> = this.PunchWeapon(player, game);
    return "V's punch " + FloatToStringPrec(this.punch, 2) + ", read from " + this.punchItem + "; quick hits with "
      + this.AttackName(this.PunchAttack(player, false)) + ", strong ones with " + this.AttackName(this.PunchAttack(player, true))
      + "; " + (IsDefined(drawn) ? "drawn, so hits carry it as their weapon" : "not drawn, so hits carry no weapon");
  }

  // A hit's amount, if it's ours: from its hitFlag.None marked "BlockRender <amount>". -1 if it isn't. Any thread: it
  // reads only the hit.
  public final static func AmountOf(data: ref<AttackData>) -> Float {
    if !IsDefined(data) {
      return -1.00;
    };
    let flags: array<SHitFlag> = data.GetFlags();
    let i: Int32 = 0;
    while i < ArraySize(flags) {
      if Equals(flags[i].flag, hitFlag.None) {
        let source: String = NameToString(flags[i].source);
        if StrBeginsWith(source, "BlockRender ") {
          return StringToFloat(StrAfterFirst(source, " "), -1.00);
        };
      };
      i += 1;
    };
    return -1.00;
  }

  // Every damage type to 0, then physical damage to amount, if it's over 0.
  public final static func SetDamage(computed: ref<gameAttackComputed>, amount: Float) -> Void {
    let values: array<Float> = computed.GetAttackValues();
    let i: Int32 = 0;
    while i < ArraySize(values) {
      values[i] = 0.00;
      i += 1;
    };
    computed.SetAttackValues(values);
    if amount > 0.00 {
      computed.SetAttackValue(amount, gamedataDamageType.Physical);
    };
  }

  // The attack record of what V punches with, from its item's record: its strong melee attack for a strong hit, its light
  // one otherwise (gamedataAttackType.Melee: QuickMelee is a gun's quick melee); of neither, its first melee attack.
  private final func PunchAttack(player: ref<GameObject>, strong: Bool) -> ref<Attack_Record> {
    let items: array<ItemID> = EquipmentSystem.GetItemsInArea(player, gamedataEquipmentArea.BaseFists);
    if ArraySize(items) == 0 {
      return null;
    };
    let weapon: ref<WeaponItem_Record> = TweakDBInterface.GetWeaponItemRecord(ItemID.GetTDBID(items[0]));
    if !IsDefined(weapon) {
      return null;
    };
    let attacks: array<wref<Attack_Record>>;
    weapon.Attacks(attacks);
    let wanted: gamedataAttackType = strong ? gamedataAttackType.StrongMelee : gamedataAttackType.Melee;
    let fallback: ref<Attack_Record>;
    let i: Int32 = 0;
    while i < ArraySize(attacks) {
      let attack: ref<Attack_Record> = attacks[i];
      if IsDefined(attack) && IsDefined(attack.AttackType()) {
        let type: gamedataAttackType = attack.AttackType().Type();
        if Equals(type, wanted) {
          return attack;
        };
        if !IsDefined(fallback) && AttackData.IsMelee(type) {
          fallback = attack;
        };
      };
      i += 1;
    };
    return fallback;
  }

  // What V punches with, as the weapon object V holds, if V has it drawn; none otherwise.
  private final func PunchWeapon(player: ref<GameObject>, game: GameInstance) -> ref<WeaponObject> {
    let items: array<ItemID> = EquipmentSystem.GetItemsInArea(player, gamedataEquipmentArea.BaseFists);
    let held: ref<WeaponObject> = GameInstance.GetTransactionSystem(game).GetItemInSlot(player, t"AttachmentSlots.WeaponRight") as WeaponObject;
    if ArraySize(items) == 0 || !IsDefined(held) {
      return null;
    };
    if ItemID.GetTDBID(held.GetItemID()) == ItemID.GetTDBID(items[0]) {
      return held;
    };
    return null;
  }

  private final func AttackName(record: ref<Attack_Record>) -> String {
    if !IsDefined(record) {
      return "none";
    };
    let type: String = IsDefined(record.AttackType()) ? EnumValueToString("gamedataAttackType", Cast<Int64>(EnumInt(record.AttackType().Type()))) : "no type";
    return TDBID.ToStringDEBUG(record.GetID()) + " (" + type + ")";
  }

  private final func Know(id: EntityID) -> Void {
```
```swift
  return wrappedMethod();
}
```
```swift
  return wrappedMethod();
}

// Our hits: a hit with no damage of its own from the attack record is marked DealNoDamage under n"no_valid_damage" before
// Process, which would then skip DealDamages; that mark comes off. Any other reason stays: friendly fire, god mode, a dead
// target. Any thread.
@wrapMethod(DamageSystem)
private final func Process(hitEvent: ref<gameHitEvent>, cache: ref<CacheData>) -> Void {
  if BlockRenderNpcs.AmountOf(hitEvent.attackData) >= 0.00 {
    hitEvent.attackData.RemoveFlag(hitFlag.DealNoDamage, n"no_valid_damage");
  };
  wrappedMethod(hitEvent, cache);
}

// The last step before the damage is applied, after every modifier. Our hit deals exactly its amount, as physical
// damage, and BlockRender hears what it took off. A hit on V that BlockRender takes for Minecraft deals nothing. Any
// thread.
@wrapMethod(DamageSystem)
private final func DealDamages(hitEvent: ref<gameHitEvent>) -> Void {
  let target: ref<GameObject> = hitEvent.target;
  let amount: Float = BlockRenderNpcs.AmountOf(hitEvent.attackData);
  if amount >= 0.00 && IsDefined(target) && IsDefined(hitEvent.attackComputed) {
    let stats: StatsObjectID = Cast<StatsObjectID>(target.GetEntityID());
    let pools: ref<StatPoolsSystem> = GameInstance.GetStatPoolsSystem(target.GetGame());
    let before: Float = pools.GetStatPoolValue(stats, gamedataStatPoolType.Health, false);
    BlockRenderNpcs.SetDamage(hitEvent.attackComputed, amount);
    wrappedMethod(hitEvent);
    BlockRenderHitLanded(target.GetEntityID(), amount, before, pools.GetStatPoolValue(stats, gamedataStatPoolType.Health, false));
    return;
  };
  if IsDefined(target) && target.IsPlayer() && IsDefined(hitEvent.attackComputed) && IsDefined(hitEvent.attackData) {
    let total: Float = hitEvent.attackComputed.GetTotalAttackValue(gamedataStatPoolType.Health);
    if total > 0.00 {
      let from: EntityID;
      let instigator: ref<GameObject> = hitEvent.attackData.GetInstigator();
      if IsDefined(instigator) {
        from = instigator.GetEntityID();
      };
      if BlockRenderVHurt(total, EnumInt(hitEvent.attackData.GetAttackType()), from) {
        BlockRenderNpcs.SetDamage(hitEvent.attackComputed, 0.00);
      };
    };
  };
  wrappedMethod(hitEvent);
}
```

- [x] **Step 2: Compile the module against the game's scripts**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && uv run --no-project python tools/check_reds.py E:/Cyberpunk2077 scripts; \
  echo "exit: $?"
```
Expected: redscript's `Compilation complete`, then `compiled` and `exit: 0`.

- [x] **Step 3: Check that the compile catches a slip: a wrap of a method that isn't there**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/reds-slip \
  && sed 's/private final func DealDamages(hitEvent/private final func DealDamagesX(hitEvent/' scripts/BlockRender.reds > build/reds-slip/BlockRender.reds \
  && uv run --no-project python tools/check_reds.py E:/Cyberpunk2077 build/reds-slip; echo "exit: $?"
```
Expected: `[UNRESOLVED_METHOD]` at the wrap's line, then `failed` and `exit: 1`.

- [x] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: the module makes Minecraft's hits V's, takes V's hurts, and keeps V's god mode and death" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 19: `GameScript` calls the module's `Hit`, `SetImmortal`, `KillV` and `Punch`

Each is found by its types once, with the others, as Part 1's are: `Hit(EntityID, Float, Bool) -> Int32`,
`SetImmortal(Bool) -> Bool`, `KillV(EntityID) -> Bool` and `Punch() -> String`, members of the session's system, which
`Current()` finds on every call. The module must now be version 3.

**Files:**
- Modify: `src/GameScript.h`, `src/GameScript.cpp`

- [x] **Step 1: Edit `src/GameScript.h`**

```cpp
constexpr int32_t kModuleVersion = 2;  // BlockRenderNpcs.Version(), as this BlockRender needs it
```
```cpp
constexpr int32_t kModuleVersion = 3;  // BlockRenderNpcs.Version(), as this BlockRender needs it
```
```cpp
	std::string Compare(double aRange);
```
```cpp
	std::string Compare(double aRange);
	// Combat (spike 6's "Part 2"). One of Minecraft's hits on the NPC aEntity, as V's: aAmount off its health, the strong
	// attack if aStrong. The module's Hit says what became of it (link::HitResultName). Nothing, with aWhy saying why, if
	// the module can't be called.
	std::optional<int32_t> Hit(uint64_t aEntity, double aAmount, bool aStrong, std::string& aWhy);
	// V's Immortal, under BlockRender's own source name: on or off. Whether the game says it changed.
	std::optional<bool> SetImmortal(bool aOn, std::string& aWhy);
	// Minecraft's death, as V's, killed by aKiller (an entity, or 0). False if V isn't found or another god mode keeps V
	// alive.
	std::optional<bool> KillV(uint64_t aKiller, std::string& aWhy);
	// V's punch, what it's read from, and what hits are made with: for the punch request.
	std::string Punch();
```
```cpp
		kCompare,
```
```cpp
		kCompare,
		kHit,
		kSetImmortal,
		kKillV,
		kPunchReport,
```

- [x] **Step 2: Edit `src/GameScript.cpp`**

```cpp
		{ "Compare", nullptr, { { "Float" } }, { "String" } },
```
```cpp
		{ "Compare", nullptr, { { "Float" } }, { "String" } },
		{ "Hit", nullptr, { { "entEntityID" }, { "Float" }, { "Bool" } }, { "Int32" } },
		{ "SetImmortal", nullptr, { { "Bool" } }, { "Bool" } },
		{ "KillV", nullptr, { { "entEntityID" } }, { "Bool" } },
		{ "Punch", nullptr, {}, { "String" } },
```
```cpp
	return result.c_str();
}
}
```
```cpp
	return result.c_str();
}

std::optional<int32_t> GameScript::Hit(uint64_t aEntity, double aAmount, bool aStrong, std::string& aWhy)
{
	if (const std::string& missing = Check(); !missing.empty())
	{
		aWhy = missing;
		return std::nullopt;
	}
	const Strong system = SessionSystem(*functions_[kCurrent], *class_, aWhy);
	if (!system)
	{
		return std::nullopt;
	}
	RED4ext::ent::EntityID npc(aEntity);
	float                  amount = float(aAmount);
	bool                   strong = aStrong;
	int32_t                result = 0;
	RED4ext::StackArgs_t   args{ { nullptr, &npc }, { nullptr, &amount }, { nullptr, &strong } };
	if (!RED4ext::ExecuteFunction(system.GetPtr(), functions_[kHit], &result, args))
	{
		aWhy = "BlockRenderNpcs.Hit failed";
		return std::nullopt;
	}
	return result;
}

std::optional<bool> GameScript::SetImmortal(bool aOn, std::string& aWhy)
{
	if (const std::string& missing = Check(); !missing.empty())
	{
		aWhy = missing;
		return std::nullopt;
	}
	const Strong system = SessionSystem(*functions_[kCurrent], *class_, aWhy);
	if (!system)
	{
		return std::nullopt;
	}
	bool                 on = aOn;
	bool                 changed = false;
	RED4ext::StackArgs_t args{ { nullptr, &on } };
	if (!RED4ext::ExecuteFunction(system.GetPtr(), functions_[kSetImmortal], &changed, args))
	{
		aWhy = "BlockRenderNpcs.SetImmortal failed";
		return std::nullopt;
	}
	return changed;
}

std::optional<bool> GameScript::KillV(uint64_t aKiller, std::string& aWhy)
{
	if (const std::string& missing = Check(); !missing.empty())
	{
		aWhy = missing;
		return std::nullopt;
	}
	const Strong system = SessionSystem(*functions_[kCurrent], *class_, aWhy);
	if (!system)
	{
		return std::nullopt;
	}
	RED4ext::ent::EntityID killer(aKiller);
	bool                   killed = false;
	RED4ext::StackArgs_t   args{ { nullptr, &killer } };
	if (!RED4ext::ExecuteFunction(system.GetPtr(), functions_[kKillV], &killed, args))
	{
		aWhy = "BlockRenderNpcs.KillV failed";
		return std::nullopt;
	}
	return killed;
}

std::string GameScript::Punch()
{
	if (const std::string& missing = Check(); !missing.empty())
	{
		return missing;
	}
	std::string  why;
	const Strong system = SessionSystem(*functions_[kCurrent], *class_, why);
	if (!system)
	{
		return why;
	}
	RED4ext::CString result;
	if (!RED4ext::ExecuteFunction(system.GetPtr(), functions_[kPunchReport], &result))
	{
		return "BlockRenderNpcs.Punch failed";
	}
	return result.c_str();
}
}
```

- [x] **Step 3: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `434 test(s), 0 failure(s)`.

- [x] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: GameScript calls the module's Hit, SetImmortal, KillV and Punch, checked by their types" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 20: The hit and punch requests, and the status's combat line

The spec's "Requests" and "Measurements and the status", for Part 2:
- **`hit <id> <damage> [strong]`:** as if Minecraft hit NPC `<id>` for `<damage>`; its number from 1, its damage over 0.
- **`punch`:** V's punch, what it's read from, and the attack records hits are made with, into the log.
- **The combat line:** on or off; V's punch; hits queued, landed and dropped by reason, and the last; V's hurts taken,
  sent, summed and left to V, and the last; Immortal; deaths; Minecraft's player dead and for how long; and why combat
  can't do all it should, while it can't. `set combat` needs no parsing of its own.

**Files:**
- Modify: `src/Requests.h`, `src/Requests.cpp`, `src/Status.h`, `src/Status.cpp`, `tests/TestRequests.cpp`,
  `tests/TestStatus.cpp`

- [x] **Step 1: Edit `tests/TestRequests.cpp`: hit and punch**

```cpp
	CHECK_STR(error, "npcs takes nothing after it");
```
```cpp
	CHECK_STR(error, "npcs takes nothing after it");
	const auto hit = cpmc::ParseRequest("hit 3 7", error);
	CHECK(hit && hit->kind == cpmc::Request::Kind::Hit && hit->id == 3 && hit->number == 7.0 && hit->what == "quick");
	const auto strong = cpmc::ParseRequest("hit 4294967295 10.5 strong", error);
	CHECK(strong && strong->id == 4294967295u && strong->number == 10.5 && strong->what == "strong");
	for (const char* bad : { "hit", "hit 3", "hit 0 7", "hit 1.5 7", "hit 4294967296 7", "hit 3 0", "hit 3 -1", "hit 3 7 hard",
	                         "hit 3 7 strong more", "hit x 7" })
	{
		CHECK(!cpmc::ParseRequest(bad, error));
	}
	CHECK_STR(error, "hit takes an NPC's number from 1, a damage over 0, and strong if it's a strong hit");
	const auto punch = cpmc::ParseRequest("punch", error);
	CHECK(punch && punch->kind == cpmc::Request::Kind::Punch);
	CHECK(!cpmc::ParseRequest("punch now", error));
```

- [x] **Step 2: Edit `tests/TestStatus.cpp`: the combat line**

```cpp
	CHECK(Has(cpmc::FormatStatus(report), "npcs=off"));
}
```
```cpp
	CHECK(Has(cpmc::FormatStatus(report), "npcs=off"));
}

TEST(StatusWritesCombat)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	report.link = link;
	const std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "combat=off: Minecraft's hits are logged, not applied; V's health is V's"));
	CHECK(status.find("npcs=") < status.find("combat=") && status.find("combat=") < status.find("ring="));
	link.combatOn = true;
	link.punch = 102.0;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "combat=on: V's punch 102.00; hits 0 queued, 0 landed; V's hurts: 0 taken, 0 sent, 0 summed, 0 "
	                                      "left to V; Immortal off; 0 deaths; Minecraft's player alive"));
	link.hitsQueued = 5;
	link.hitsLanded = 4;
	link.hitsDropped = "1 not listed";
	link.lastHit = "NPC 35 (NC Resident): 7.00 of Minecraft's damage, 714.00 of its health; landed, 1514 -> 800";
	link.hurtsTaken = 3;
	link.hurtsSent = 2;
	link.hurtsSummed = 1;
	link.hurtsLeft = 7;
	link.lastHurt = "45.0 of V's health (0.0987), 1.97 of Minecraft's 20, projectile (Ranged), from NPC 12";
	link.immortal = true;
	link.deaths = 1;
	link.minecraftDeadFor = 0.84;
	link.combatWhy = "V's max health isn't read, so V's hurts aren't taken";
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "combat=on: V's punch 102.00; hits 5 queued, 4 landed, dropped: 1 not listed; the last NPC 35 "
	                                      "(NC Resident): 7.00 of Minecraft's damage, 714.00 of its health; landed, 1514 -> 800; V's "
	                                      "hurts: 3 taken, 2 sent, 1 summed, 7 left to V; the last 45.0 of V's health (0.0987), 1.97 of "
	                                      "Minecraft's 20, projectile (Ranged), from NPC 12; Immortal on; 1 deaths; Minecraft's player "
	                                      "dead for 0.8 s; V's max health isn't read, so V's hurts aren't taken"));
}
```

- [x] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'id': is not a member of 'cpmc::Request'`, and `error C2065: 'Hit': undeclared identifier`.

- [x] **Step 4: Edit `src/Requests.h`**

```cpp
#include <optional>
```
```cpp
#include <cstdint>
#include <optional>
```
```cpp
//   npcs                       (spike 6: every NPC listed, into the log, and what the game's targeting finds)
```
```cpp
//   npcs                       (spike 6: every NPC listed, into the log, and what the game's targeting finds)
//   hit <id> <damage> [strong] (spike 6: as if Minecraft hit NPC <id>, from 1, for <damage>, over 0, of its damage)
//   punch                      (spike 6: V's punch, what it's read from, and what hits are made with, into the log)
```
```cpp
		Npcs,
	};

	Kind        kind = Kind::Log;
	std::string what;          // the dump target, mode, structure, setting name, on or off, or what to type
	double      number = 0.0;  // the field's block count, the setting's value, or the drive log's seconds
```
```cpp
		Npcs,
		Hit,
		Punch,
	};

	Kind        kind = Kind::Log;
	std::string what;          // the dump target, mode, structure, setting name, on or off, what to type, or a hit's
	                           // strong or quick
	double      number = 0.0;  // the field's block count, the setting's value, the drive log's seconds, or a hit's damage
	uint32_t    id = 0;        // a hit's NPC: its number in SkyCraft's actor table
```

- [x] **Step 5: Edit `src/Requests.cpp`**

```cpp
	if (command == "log" || command == "clear" || command == "reload" || command == "floor" || command == "shapes" || command == "npcs")
```
```cpp
	if (command == "log" || command == "clear" || command == "reload" || command == "floor" || command == "shapes" || command == "npcs" ||
	    command == "punch")
```
```cpp
		                           : command == "npcs"   ? Request::Kind::Npcs
```
```cpp
		                           : command == "npcs"   ? Request::Kind::Npcs
		                           : command == "punch"  ? Request::Kind::Punch
```
```cpp
		return Request{ kind, std::string(words[1]) };
```
```cpp
		return Request{ kind, std::string(words[1]) };
	}
	if (command == "hit")
	{
		const std::optional<double> id = arguments >= 2 ? Number(words[1]) : std::nullopt;
		const std::optional<double> damage = arguments >= 2 ? Number(words[2]) : std::nullopt;
		if (arguments < 2 || arguments > 3 || !id || *id < 1.0 || *id > 4294967295.0 || *id != std::floor(*id) || !damage || *damage <= 0.0 ||
		    (arguments == 3 && words[3] != "strong"))
		{
			return Fail(aError, "hit takes an NPC's number from 1, a damage over 0, and strong if it's a strong hit");
		}
		Request request{ Request::Kind::Hit, arguments == 3 ? "strong" : "quick", *damage };
		request.id = uint32_t(*id);
		return request;
```

- [x] **Step 6: Edit `src/Status.h`**

```cpp
	uint64_t              npcHits = 0;  // Minecraft's hits on them, logged
```
```cpp
	uint64_t              npcHits = 0;  // Minecraft's hits on them, logged
	// Combat both ways (spike 6's "Part 2"): the counts are since the start
	bool                  combatOn = false;  // set combat 1
	double                punch = 0.0;       // V's punch, as last read
	uint64_t              hitsQueued = 0;    // Minecraft's hits made V's, and queued in the game
	uint64_t              hitsLanded = 0;    // and dealt by its damage pipeline
	std::string           hitsDropped;       // by reason: "2 not listed, 1 the NPC is dead"; empty: none
	std::string           lastHit;           // the NPC, Minecraft's damage, the amount, and how it went
	uint64_t              hurtsTaken = 0;    // the game's hits on V, taken for Minecraft
	uint64_t              hurtsSent = 0;     // kInHurts sent
	uint64_t              hurtsSummed = 0;   // hurts that went in a sum
	uint64_t              hurtsLeft = 0;     // offered while not taken: left to V
	std::string           lastHurt;          // its amount and share, Minecraft's damage, kind, attack type and attacker
	bool                  immortal = false;  // our Immortal on V
	uint64_t              deaths = 0;        // Minecraft's deaths that killed V
	std::optional<double> minecraftDeadFor;  // seconds; none: alive
	std::string           combatWhy;         // why combat can't do all it should, while it can't
```

- [x] **Step 7: Edit `src/Status.cpp`**

```cpp
std::string FormatLink(const LinkReport& aLink)
```
```cpp
std::string FormatCombat(const LinkReport& aLink)
{
	if (!aLink.combatOn)
	{
		return "combat=off: Minecraft's hits are logged, not applied; V's health is V's\n";
	}
	std::string out = std::format("combat=on: V's punch {:.2f}; hits {} queued, {} landed{}", aLink.punch, aLink.hitsQueued, aLink.hitsLanded,
	                              aLink.hitsDropped.empty() ? "" : ", dropped: " + OneLine(aLink.hitsDropped));
	if (!aLink.lastHit.empty())
	{
		out += "; the last " + OneLine(aLink.lastHit);
	}
	out += std::format("; V's hurts: {} taken, {} sent, {} summed, {} left to V", aLink.hurtsTaken, aLink.hurtsSent, aLink.hurtsSummed,
	                   aLink.hurtsLeft);
	if (!aLink.lastHurt.empty())
	{
		out += "; the last " + OneLine(aLink.lastHurt);
	}
	out += std::format("; Immortal {}; {} deaths; Minecraft's player {}", aLink.immortal ? "on" : "off", aLink.deaths,
	                   aLink.minecraftDeadFor ? std::format("dead for {:.1f} s", *aLink.minecraftDeadFor) : std::string("alive"));
	if (!aLink.combatWhy.empty())
	{
		out += "; " + OneLine(aLink.combatWhy);
	}
	return out + "\n";
}

std::string FormatLink(const LinkReport& aLink)
```
```cpp
	out += FormatNpcs(aLink);
```
```cpp
	out += FormatNpcs(aLink);
	out += FormatCombat(aLink);
```

- [x] **Step 8: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `435 test(s), 0 failure(s)`.

- [x] **Step 9: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: the hit and punch requests, and the status's combat line" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 21: BlockRender 0.18.0 lands Minecraft's hits on NPCs as V's, and gives V's hurts to Minecraft in build mode

The spec's "The frame", "Minecraft's hits on NPCs", "The game's hits on V", "Deaths", "V's god mode" and "Combat on
and off", in the plugin:
- **The sink keeps** Minecraft's hits and deaths in the ring's order; after the link's update, **`Fight`** takes them:
  every hit is logged; with combat on, one that counts (`AcceptHit`) goes to the module's `Hit` while V is found and
  linked, and one that doesn't is counted by why. A death kills V through `KillV`, with combat on, in build mode or
  within 2 s of its end. Then our hits the game has dealt (the inbox's landings, with the NPC's health before and
  after), and those it hasn't within 2 s, counted as not dealt: friendly, invulnerable or dead by then. Then V's god
  mode (`GodMode`), and whether the inbox takes V's hurts: combat and build mode on, V found, V's max health read.
- **After the list,** **`SendHurts`** makes the hurts the inbox took Minecraft's, through the sums, and sends them;
  one the input ring has no room for waits for the next frame.
- **V's punch and max health** are kept from each list, for the hits and the hurts.
- **`set combat 0|1`** (off by default), **`hit`** (an event like Minecraft's, strong as a crit is) and **`punch`**.
- **The status's combat line** from all that, and the version: 0.18.0.

**Files:**
- Modify: `src/Plugin.cpp`, `CMakeLists.txt`

- [x] **Step 1: Edit `src/Plugin.cpp`**

```cpp
// (scripts/BlockRender.reds), so that Minecraft's weapons hit them and they press its pressure plates.
```
```cpp
// (scripts/BlockRender.reds), so that Minecraft's weapons hit them and they press its pressure plates; with combat on,
// Minecraft's hits land on them as V's, and in build mode the game's hits on V go to Minecraft's player, whose death
// kills V.
```
```cpp
#include "link/Actors.h"
```
```cpp
#include "link/Actors.h"
#include "link/Combat.h"
```
```cpp
#include <initializer_list>
```
```cpp
#include <initializer_list>
#include <map>
```
```cpp
constexpr const char* kVersion = "0.17.0";
```
```cpp
constexpr const char* kVersion = "0.18.0";
```
```cpp
size_t                                    g_wrapThreads = 0;      // the wraps' threads, as last logged
```
```cpp
size_t                                    g_wrapThreads = 0;      // the wraps' threads, as last logged

// Spike 6's combat both ways (its spec's "Part 2"). Main thread only: the natives' inbox takes what the damage
// pipeline's wraps report, on whichever thread runs it.
bool                                  g_combat = false;      // set combat 0|1: off until step 4 passes
std::vector<skycraft::proto::McEvent> g_events;              // Minecraft's hits and deaths this frame, in the ring's order
double                                g_punch = 0.0;         // V's punch and max health, as the module last read them
double                                g_vHealthMax = 0.0;
cpmc::link::HurtSums                  g_hurtSums;            // V's hurts, summed for Minecraft
std::vector<cpmc::link::Hurt>         g_hurtsOut;            // waiting for room in the input ring
cpmc::link::GodMode                   g_godMode;             // V's Immortal, under our own source name
bool                                  g_immortal = false;    // as the game last said
bool                                  g_wasBuild = false;
std::optional<double>                 g_buildEndedAt;        // when build mode last ended, for Minecraft's death
std::optional<double>                 g_minecraftDiedAt;     // since when McState has said Minecraft's player is dead
// One of our hits, queued and not yet dealt by the game.
struct PendingHit
{
	uint64_t entity = 0;
	uint32_t formId = 0;
	double   damage = 0.0;
	double   amount = 0.0;
	double   at = 0.0;
};
std::vector<PendingHit>         g_pendingHits;
std::map<std::string, uint64_t> g_hitDrops;  // hits not applied, by reason
uint64_t                        g_hitsQueued = 0;
uint64_t                        g_hitsLanded = 0;
uint64_t                        g_hurtsSent = 0;
uint64_t                        g_deaths = 0;
std::string                     g_lastHit;
std::string                     g_lastHurt;
```
```cpp
	// Spike 6's Part 1 only logs Minecraft's hits on NPCs (its spec's "The link"), and Minecraft's player's death.
	void HitActor(const skycraft::proto::McEvent& aEvent) override
	{
		++g_npcHits;
		const auto record = std::find_if(g_actorRecords.begin(), g_actorRecords.end(),
		                                 [&](const skycraft::proto::ActorRecord& aRecord) { return aRecord.formId == aEvent.formId; });
		::Log("hit on NPC {} ({}) for {:.2f} of Minecraft's damage, knockback {:.2f}; {}; not applied in Part 1", aEvent.formId,
		      record != g_actorRecords.end() ? std::string(record->name) : std::string("not listed now"), aEvent.a, aEvent.d,
		      cpmc::link::HitWords(aEvent));
	}

	void PlayerDied(const skycraft::proto::McEvent& aEvent) override
	{
		::Log("Minecraft's player died{}", aEvent.formId != 0 ? std::format(", killed by NPC {}", aEvent.formId) : std::string());
	}
```
```cpp
	// Spike 6: Minecraft's hits on NPCs and its player's death, kept in the ring's order for Fight, after the link's update.
	void HitActor(const skycraft::proto::McEvent& aEvent) override { g_events.push_back(aEvent); }
	void PlayerDied(const skycraft::proto::McEvent& aEvent) override { g_events.push_back(aEvent); }
```
```cpp
	}
	g_actorRecords = cpmc::link::ActorRecords(g_npcList->npcs, *aFeet, g_npcSizes.Boxes(), g_actorIds, aNow);
```
```cpp
	}
	g_punch = g_npcList->punch;
	g_vHealthMax = g_npcList->vHealthMax;
	g_actorRecords = cpmc::link::ActorRecords(g_npcList->npcs, *aFeet, g_npcSizes.Boxes(), g_actorIds, aNow);
```
```cpp
		g_npcSizes.Forget(g_actorIds);
	}
}
```
```cpp
		g_npcSizes.Forget(g_actorIds);
	}
}

// The name SkyCraft's actor table last had for stand-in aFormId.
std::string NpcName(uint32_t aFormId)
{
	const auto record = std::find_if(g_actorRecords.begin(), g_actorRecords.end(),
	                                 [&](const skycraft::proto::ActorRecord& aRecord) { return aRecord.formId == aFormId; });
	return record != g_actorRecords.end() ? std::string(record->name) : std::string("not listed now");
}

void DropHit(const std::string& aHit, const std::string& aWhy)
{
	++g_hitDrops[aWhy];
	Log("{}; not applied: {}", aHit, aWhy);
}

// One of Minecraft's hits on an NPC (spike 6's "Minecraft's hits on NPCs"): logged, and with combat on, made V's through
// the module: Minecraft's damage x V's punch off the NPC's health.
void HitOnNpc(const skycraft::proto::McEvent& aEvent, double aNow, bool aVFound)
{
	++g_npcHits;
	const std::string hit = std::format("hit on NPC {} ({}) for {:.2f} of Minecraft's damage, knockback {:.2f}; {}", aEvent.formId,
	                                    NpcName(aEvent.formId), aEvent.a, aEvent.d, cpmc::link::HitWords(aEvent));
	if (!g_combat)
	{
		return Log("{}; not applied: combat is off", hit);
	}
	cpmc::link::HitDrop                     drop = cpmc::link::HitDrop::Count;
	const std::optional<cpmc::link::NpcHit> accepted = cpmc::link::AcceptHit(aEvent, g_actorIds, aNow, g_punch, drop);
	if (!accepted)
	{
		return DropHit(hit, cpmc::link::HitDropName(drop));
	}
	if (!aVFound)
	{
		return DropHit(hit, "V isn't found");
	}
	std::string                  why;
	const std::optional<int32_t> result = g_script.Hit(accepted->entity, accepted->amount, accepted->strong, why);
	if (!result || *result != 1)
	{
		return DropHit(hit, result ? cpmc::link::HitResultName(*result) : "the module can't be called: " + why);
	}
	++g_hitsQueued;
	g_pendingHits.push_back({ accepted->entity, accepted->formId, accepted->damage, accepted->amount, aNow });
	g_lastHit = std::format("NPC {} ({}): {:.2f} of Minecraft's damage, {:.2f} of its health{}; queued", accepted->formId,
	                        NpcName(accepted->formId), accepted->damage, accepted->amount, accepted->strong ? ", strong" : "");
	Log("{}: {:.2f} of its health, {:.2f} x V's punch {:.2f}{}; queued", hit, accepted->amount, accepted->damage, g_punch,
	    accepted->strong ? ", strong" : "");
}

// Minecraft's player's death (spike 6's "Deaths"): with combat on, in build mode or just after it, V's death too.
void MinecraftDied(const skycraft::proto::McEvent& aEvent, double aNow, bool aVFound)
{
	const std::string died =
	    std::format("Minecraft's player died{}", aEvent.formId != 0 ? std::format(", killed by NPC {}", aEvent.formId) : std::string());
	if (!cpmc::link::DeathKillsV(g_combat, g_router->On(), g_buildEndedAt, aNow))
	{
		return Log("{}; V lives: {}", died, g_combat ? "build mode wasn't on" : "combat is off");
	}
	if (!aVFound)
	{
		return Log("{}; V lives: V isn't found", died);
	}
	const uint64_t            killer = aEvent.formId != 0 ? g_actorIds.Entity(aEvent.formId, aNow).value_or(0) : 0;
	std::string               why;
	const std::optional<bool> killed = g_script.KillV(killer, why);
	++g_deaths;
	g_immortal = false;
	Log("{}; so V dies: {}", died, killed ? (*killed ? "V's health set to its least" : "but another god mode keeps V alive") : "not done: " + why);
}

// Spike 6's combat, after the link's update and before the NPCs are listed (its spec's "The frame"): Minecraft's hits and
// deaths in the ring's order; our hits the game has dealt, and those it hasn't within kHitTrail; V's god mode; and
// whether V's hurts are taken.
void Fight(double aNow, const std::optional<cpmc::Vec3d>& aFeet)
{
	const bool build = g_router->On();
	if (g_wasBuild && !build)
	{
		g_buildEndedAt = aNow;
	}
	g_wasBuild = build;
	const bool vFound = aFeet.has_value();
	for (const skycraft::proto::McEvent& event : std::exchange(g_events, {}))
	{
		if (event.type == skycraft::proto::kEvHitActor)
		{
			HitOnNpc(event, aNow, vFound && g_linked);
		}
		else if (event.type == skycraft::proto::kEvPlayerDied)
		{
			MinecraftDied(event, aNow, vFound);
		}
	}
	for (const cpmc::CombatInbox::Landed& landed : cpmc::CombatReported().TakeLanded())
	{
		++g_hitsLanded;
		const auto pending = std::find_if(g_pendingHits.begin(), g_pendingHits.end(),
		                                  [&](const PendingHit& aHit) { return aHit.entity == landed.target; });
		const uint32_t formId = pending != g_pendingHits.end() ? pending->formId : g_actorIds.IdOf(landed.target).value_or(0);
		const double   damage = pending != g_pendingHits.end() ? pending->damage : 0.0;
		if (pending != g_pendingHits.end())
		{
			g_pendingHits.erase(pending);
		}
		g_lastHit = std::format("NPC {} ({}): {:.2f} of Minecraft's damage, {:.2f} of its health; landed, {:.0f} -> {:.0f}", formId,
		                        NpcName(formId), damage, landed.amount, landed.before, landed.after);
		Log("hit on NPC {} landed: {:.2f} of its health, {:.0f} -> {:.0f}", formId, landed.amount, landed.before, landed.after);
	}
	std::erase_if(g_pendingHits, [&](const PendingHit& aHit) {
		if (aNow - aHit.at <= cpmc::link::kHitTrail)
		{
			return false;
		}
		++g_hitDrops["not dealt by the game"];
		Log("hit on NPC {} ({:.2f} of its health) wasn't dealt by the game within {:.0f} s: friendly, invulnerable or dead by then",
		    aHit.formId, aHit.amount, cpmc::link::kHitTrail);
		return true;
	});
	if (const std::optional<bool> change = g_godMode.Frame(g_combat, build, vFound))
	{
		std::string               why;
		const std::optional<bool> changed = g_script.SetImmortal(*change, why);
		if (changed)
		{
			g_immortal = *change;
		}
		Log("combat: V's Immortal {}{}", *change ? "on" : "off",
		    changed ? (*changed ? std::string() : std::string(" (the game says it already was)")) : ": not done: " + why);
	}
	cpmc::CombatReported().SetTaking(g_combat && build && vFound && g_vHealthMax > 0.0);
	const bool dead = g_link && (g_link->State().minecraftFlags & skycraft::proto::kMcDead) != 0;
	if (!dead)
	{
		g_minecraftDiedAt.reset();
	}
	else if (!g_minecraftDiedAt)
	{
		g_minecraftDiedAt = aNow;
	}
}

// V's hurts the pipeline's wrap took since the last frame (spike 6's "The game's hits on V"), to Minecraft as kInHurt,
// through the sums. What the input ring has no room for waits for the next frame.
void SendHurts(double aNow)
{
	if (!g_link)
	{
		return;
	}
	const double tickMs = g_link->State().minecraftTickMs;
	for (const cpmc::link::GameHurt& taken : cpmc::CombatReported().TakeHurts())
	{
		const cpmc::link::Hurt hurt = cpmc::link::HurtFrom(taken, g_vHealthMax, g_actorIds);
		g_lastHurt = std::format("{:.1f} of V's health ({:.4f}), {:.2f} of Minecraft's 20, {} ({}){}{}", taken.amount, hurt.share,
		                         hurt.share * 20.0, cpmc::link::HurtKindName(hurt.kind), cpmc::link::AttackTypeName(taken.attackType),
		                         hurt.attacker != 0 ? std::format(", from NPC {}", hurt.attacker) : std::string(),
		                         hurt.power ? ", a power attack" : "");
		Log("combat: V hurt, taken for Minecraft: {}", g_lastHurt);
		if (const std::optional<cpmc::link::Hurt> now = g_hurtSums.Add(hurt, aNow, tickMs))
		{
			g_hurtsOut.push_back(*now);
		}
	}
	if (const std::optional<cpmc::link::Hurt> due = g_hurtSums.Due(aNow, tickMs))
	{
		g_hurtsOut.push_back(*due);
	}
	size_t sent = 0;
	while (sent < g_hurtsOut.size() && g_link->Push(cpmc::link::HurtInput(g_hurtsOut[sent])))
	{
		++sent;
	}
	g_hurtsSent += sent;
	g_hurtsOut.erase(g_hurtsOut.begin(), g_hurtsOut.begin() + ptrdiff_t(sent));
}

// The hits not applied, by reason: "2 not listed, 1 the NPC is dead".
std::string HitDrops()
{
	std::string text;
	for (const auto& [why, count] : g_hitDrops)
	{
		text += std::format("{}{} {}", text.empty() ? "" : ", ", count, why);
	}
	return text;
}

// Why combat can't do all it should, while combat is on.
std::string CombatWhy()
{
	if (!g_combat)
	{
		return {};
	}
	if (g_punch <= 0.0)
	{
		return "V's punch isn't read yet, so hits aren't applied";
	}
	if (g_vHealthMax <= 0.0)
	{
		return "V's max health isn't read yet, so V's hurts aren't taken";
	}
	return {};
}
```
```cpp
		return Report(std::format("set ao {}", g_meshOptions.ambientOcclusion ? 1 : 0));
```
```cpp
		return Report(std::format("set ao {}", g_meshOptions.ambientOcclusion ? 1 : 0));
	}
	if (aRequest.what == "combat")
	{
		if (aRequest.number != 0.0 && aRequest.number != 1.0)
		{
			return Report("set combat: 1 lands Minecraft's hits on NPCs as V's, and in build mode gives the game's hits on V to Minecraft; "
			              "0 only logs hits");
		}
		g_combat = aRequest.number == 1.0;
		return Report(g_combat ? "set combat 1: Minecraft's hits land on NPCs as V's; in build mode V is Immortal, the game's hits on V go "
		                         "to Minecraft's player, and its death kills V"
		                       : "set combat 0: Minecraft's hits are only logged, and V's health is V's");
```
```cpp
// box on|off (spike 5's step 4): a test box 3 m ahead of V, made as the bodies for blocks are, before any block is.
```
```cpp
// hit <id> <damage> [strong] (spike 6's step 4): as if Minecraft hit NPC <id> for <damage>, through all its hits go
// through; strong as a crit is.
void HitRequest(const cpmc::Request& aRequest)
{
	skycraft::proto::McEvent event{};
	event.type = skycraft::proto::kEvHitActor;
	event.formId = aRequest.id;
	event.a = float(aRequest.number);
	event.d = 0.4f;  // Minecraft's own knockback
	event.flags = aRequest.what == "strong" ? skycraft::proto::kHitCritical : 0u;
	event.weapon = skycraft::proto::kWeaponUnarmed;
	g_events.push_back(event);
	Report(std::format("hit: as if Minecraft hit NPC {} for {:.2f}{}; the log says how it went", aRequest.id, aRequest.number,
	                   aRequest.what == "strong" ? ", strong" : ""));
}

// box on|off (spike 5's step 4): a test box 3 m ahead of V, made as the bodies for blocks are, before any block is.
```
```cpp
	case cpmc::Request::Kind::Npcs: NpcsRequest(); break;
```
```cpp
	case cpmc::Request::Kind::Npcs: NpcsRequest(); break;
	case cpmc::Request::Kind::Hit: HitRequest(*request); break;
	case cpmc::Request::Kind::Punch:
		Log("punch: {}", g_script.Punch());
		Report("punch: V's punch, what it's read from, and what hits are made with, in the log");
		break;
```
```cpp
	report.npcHits = g_npcHits;
```
```cpp
	report.npcHits = g_npcHits;
	report.combatOn = g_combat;
	report.punch = g_punch;
	report.hitsQueued = g_hitsQueued;
	report.hitsLanded = g_hitsLanded;
	report.hitsDropped = HitDrops();
	report.lastHit = g_lastHit;
	const cpmc::CombatInbox::Counts combat = cpmc::CombatReported().Count();
	report.hurtsTaken = combat.taken;
	report.hurtsSent = g_hurtsSent;
	report.hurtsSummed = g_hurtSums.Summed();
	report.hurtsLeft = combat.refused;
	report.lastHurt = g_lastHurt;
	report.immortal = g_immortal;
	report.deaths = g_deaths;
	report.minecraftDeadFor = g_minecraftDiedAt ? std::optional<double>(aNow - *g_minecraftDiedAt) : std::nullopt;
	report.combatWhy = CombatWhy();
```
```cpp
	const std::optional<cpmc::Vec3d> feet = UpdateLink(seconds, frameMs);
	ListNpcs(seconds, feet);
```
```cpp
	const std::optional<cpmc::Vec3d> feet = UpdateLink(seconds, frameMs);
	Fight(seconds, feet);
	ListNpcs(seconds, feet);
	SendHurts(seconds);
```

- [x] **Step 2: Edit `CMakeLists.txt`: version 0.18.0**

```cmake
project(BlockRender VERSION 0.17.0 LANGUAGES C CXX)
```
```cmake
project(BlockRender VERSION 0.18.0 LANGUAGES C CXX)
```

- [x] **Step 3: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `435 test(s), 0 failure(s)`.

- [x] **Step 4: Check that BlockRender imports nothing from PhysX**

```bash
DUMPBIN="$(ls "/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/"*/bin/Hostx64/x64/dumpbin.exe | head -1)"
cd /d/Cyberpunk-Minecraft/spikes/block-render && MSYS_NO_PATHCONV=1 "$DUMPBIN" -imports -nologo build\\RelWithDebInfo\\BlockRender.dll \
  | grep -ci "physx"
```
Expected: `0`.

- [x] **Step 5: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: BlockRender 0.18.0 lands Minecraft's hits on NPCs as V's and gives V's hurts to Minecraft in build mode" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 22: The overlay's combat line, its Combat button, and V's punch in the log

The overlay shows the status's combat line in short: on or off, hits queued and landed, V's hurts taken and sent,
Immortal, and Minecraft's player dead and for how long. A Combat button in the top row switches `set combat`; "Punch in
the log", under "More controls", sends `punch`.

**Files:**
- Modify: `tools/blockrender_hud/init.lua`

- [x] **Step 1: Edit `tools/blockrender_hud/init.lua`**

```lua
-- Kept small: one row of buttons (link, Minecraft's blocks as bodies, step 4's test box, the turn, NPCs in Minecraft),
-- and a line each for the last request's reply, build mode (F8) and its drive, the bodies, the NPCs, and V. Two folded
-- sections hold the rest: "More controls" (the drawing mode, Minecraft's HUD, commands typed into Minecraft, the drive
-- log, the ground and its shapes, and the NPCs' list) and "Full status" (every status line, and the frame time).
```
```lua
-- Kept small: one row of buttons (link, Minecraft's blocks as bodies, step 4's test box, the turn, NPCs in Minecraft,
-- combat), and a line each for the last request's reply, build mode (F8) and its drive, the bodies, the NPCs, combat,
-- and V. Two folded sections hold the rest: "More controls" (the drawing mode, Minecraft's HUD, commands typed into
-- Minecraft, the drive log, the ground and its shapes, the NPCs' list and V's punch) and "Full status" (every status
-- line, and the frame time).
```
```lua
local npcsOn = nil       -- whether NPCs go into SkyCraft's actor table: "on" or "off"
```
```lua
local npcsOn = nil       -- whether NPCs go into SkyCraft's actor table: "on" or "off"
local combat = nil       -- the combat line: on or off; hits queued and landed; V's hurts; Immortal; Minecraft's death
local combatOn = nil     -- whether Minecraft's hits land as V's and V's hurts go to Minecraft: "on" or "off"
```
```lua
    npcsOn = nil
```
```lua
    npcsOn = nil
    combat = nil
    combatOn = nil
```
```lua
                .. (why and (", none listed: " .. why) or "")
```
```lua
                .. (why and (", none listed: " .. why) or "")
        end
        local combatState = line:match("^combat=(%a+)")
        if combatState ~= nil then
            combatOn = combatState
            local hits = line:match("; hits (%d+ queued, %d+ landed)")
            local hurts = line:match("; V's hurts: (%d+ taken, %d+ sent)")
            local immortal = line:match("; Immortal (%a+);")
            local dead = line:match("; Minecraft's player (dead for [%d.]+ s)")
            combat = combatState .. (hits and (", hits " .. hits) or "") .. (hurts and (", V's hurts " .. hurts) or "")
                .. (immortal and (", Immortal " .. immortal) or "") .. (dead and (", Minecraft's player " .. dead) or "")
```
```lua
        end
        if status == nil then
```
```lua
        end
        if combatOn ~= nil then
            ImGui.SameLine()
            if ImGui.Button("Combat: " .. combatOn .. "###combat") then
                request("set combat " .. (combatOn == "on" and 0 or 1))
            end
        end
        if status == nil then
```
```lua
            ImGui.Text("NPCs: " .. npcs)
```
```lua
            ImGui.Text("NPCs: " .. npcs)
        end
        if combat ~= nil then
            ImGui.Text("Combat: " .. combat)
```
```lua
                    request("npcs")
                end
            end
        end
```
```lua
                    request("npcs")
                end
            end
            if combatOn ~= nil then
                ImGui.Text("Combat:")
                ImGui.SameLine()
                if ImGui.Button("Punch in the log") then
                    request("punch")
                end
            end
        end
```

- [x] **Step 2: Check the overlay's syntax**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && luac -p tools/blockrender_hud/init.lua && echo ok
```
Expected: `ok`.

- [x] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: the overlay's combat line, its Combat button, and V's punch in the log" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 23: What the pipeline did to our hits, in words; V's non-lethal hurts; and V's switches for its weapon and meshes

The deep review of Part 2 (at the end of this plan) changed how Minecraft's hits land: they now start the game's damage
pipeline at Minecraft's damage × V's punch, and the pipeline does to them all it does to V's own punches, so what they
take off isn't known in advance. This task is the plain C++ for what the module will report, in `link/Combat`:
- **What the pipeline did to one of our hits,** as flags with its landing: critical (V's crit, rolled by the game),
  blocked, parried, the NPC's armour (the hit type `Glance`, of a hit that isn't critical), mitigated, and killed the
  NPC. `LandedWords` says them: `critical, armour`.
- **Why the pipeline dealt one of our hits nothing,** from the source name of its `DealNoDamage` flag: friendly to V,
  invulnerable, already dead, dodged, evaded, killed at once, no damage left, or another of the game's reasons.
  `HitEndedName` says it.
- **A hit on V the game won't let kill V** (`CannotKillPlayer`, or a minimum health left) is marked `nonLethal`, so the
  inbox leaves it V's (Task 24): sent to Minecraft, it could kill Minecraft's player, and so V.
- **`VSwitch`:** one of V's states switched through the module while it's wanted: V's unarmed weapon drawn (with combat
  and build mode on), and V's meshes hidden (in build mode). Wanted, it's switched on, and on again every `aRefresh`
  seconds while it stays wanted, so that the weapon, once drawn, is hidden too; not wanted, off, once. Losing V, as
  during a load, counts as off with nothing to switch back.

**Files:**
- Modify: `src/link/Combat.h`, `src/link/Combat.cpp`, `tests/TestCombat.cpp`

- [x] **Step 1: Edit `tests/TestCombat.cpp`: the words, and V's switches**

```cpp
	CHECK(god.Frame(true, true, true) == std::optional<bool>(true));
}
```
```cpp
	CHECK(god.Frame(true, true, true) == std::optional<bool>(true));
}

TEST(CombatSaysWhatThePipelineDidToOurHits)
{
	CHECK_STR(LandedWords(0), "");
	CHECK_STR(LandedWords(kLandedCritical | kLandedArmour), "critical, armour");
	CHECK_STR(LandedWords(kLandedBlocked | kLandedParried | kLandedMitigated | kLandedKilled), "blocked, parried, mitigated, killed it");
	CHECK_STR(LandedWords(64), "");  // a bit it doesn't know
	CHECK_STR(HitEndedName(4), "dodged");
	CHECK_STR(HitEndedName(5), "evaded");
	CHECK_STR(HitEndedName(1), "friendly to V");
	CHECK_STR(HitEndedName(0), "another of the game's reasons");
	CHECK_STR(HitEndedName(9), "reason 9");
}

TEST(CombatSwitchesVsWeaponAndMeshesWhileWanted)
{
	// Drawing V's unarmed weapon: on once, off once.
	VSwitch arm;
	CHECK(!arm.Frame(true, false, 0.0).has_value());  // V not found: nothing
	CHECK(arm.Frame(true, true, 1.0) == std::optional<bool>(true));
	CHECK(arm.On());
	CHECK(!arm.Frame(true, true, 9.0).has_value());
	CHECK(arm.Frame(false, true, 10.0) == std::optional<bool>(false));
	CHECK(!arm.Frame(false, true, 11.0).has_value());
	// Lost while on, as during a load: off, with nothing to switch back; found again while wanted: on.
	CHECK(arm.Frame(true, true, 12.0) == std::optional<bool>(true));
	CHECK(!arm.Frame(true, false, 13.0).has_value());
	CHECK(!arm.On());
	CHECK(!arm.Frame(false, true, 14.0).has_value());
	CHECK(arm.Frame(true, true, 15.0) == std::optional<bool>(true));
	// Hiding V's meshes: on again every half second while wanted, so that a weapon drawn meanwhile is hidden too.
	VSwitch hide(0.5);
	CHECK(hide.Frame(true, true, 1.0) == std::optional<bool>(true));
	CHECK(!hide.Frame(true, true, 1.4).has_value());
	CHECK(hide.Frame(true, true, 1.5) == std::optional<bool>(true));
	CHECK(!hide.Frame(true, true, 1.9).has_value());
	CHECK(hide.Frame(false, true, 2.0) == std::optional<bool>(false));
	CHECK(!hide.Frame(false, true, 2.6).has_value());
}
```
- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C3861: 'LandedWords': identifier not found`, and `error C2065: 'VSwitch': undeclared identifier`.

- [x] **Step 3: Edit `src/link/Combat.h`**

```cpp
// Combat both ways (spike 6's "Part 2"), the parts that don't need the game: which of Minecraft's hits on NPCs count and
// how much each does; V's hurts as SkyCraft's kInHurt, summed so that Minecraft takes them all; when Minecraft's death
// kills V; and when V's god mode goes on and off. Pure: the tests give it hits and hurts of their own.
```
```cpp
// Combat both ways (spike 6's "Part 2"), the parts that don't need the game: which of Minecraft's hits on NPCs count, what
// each starts at, and what the game's damage pipeline made of it; V's hurts as SkyCraft's kInHurt, summed so that
// Minecraft takes them all; when Minecraft's death kills V; and when V's god mode, V's unarmed weapon and V's hidden
// meshes go on and off. Pure: the tests give it hits and hurts of their own.
```
```cpp
	double   amount = 0.0;  // damage x V's punch
```
```cpp
	double   amount = 0.0;  // damage x V's punch: where the game's damage pipeline starts it
```
```cpp
// A hit on V that the module's DealDamages wrap took, in the game's terms: the health it would have taken, after V's
// armour and the difficulty; its gamedataAttackType, as a number; and its instigator's entity, or 0.
```
```cpp
// What the game's damage pipeline did to one of our hits, as the module's DealDamages wrap reports it when it lands.
enum LandedFlag : uint32_t
{
	kLandedCritical = 1,    // V's crit, rolled by the game (hitFlag.CriticalHit)
	kLandedBlocked = 2,     // the NPC blocked it (WasBlocked)
	kLandedParried = 4,     // the NPC parried it (WasDeflected)
	kLandedArmour = 8,      // the NPC's armour took some, of a hit that isn't critical (the hit type Glance)
	kLandedMitigated = 16,  // the NPC's mitigation took some (WasMitigated)
	kLandedKilled = 32,     // it killed the NPC (WasKillingBlow)
};

// The flags in words, "critical, armour"; empty for none. Bits it doesn't know are left out.
std::string LandedWords(uint32_t aFlags);

// Why the game's damage pipeline dealt one of our hits nothing, as the module's wrap of ProcessPipeline reports it, from
// the source name of the hit's DealNoDamage flag: 1 friendly to V ("FriendlyFireIgnored"), 2 invulnerable, 3 already dead,
// 4 dodged ("ProcessDodge"), 5 evaded ("ProcessEvasion"), 6 killed at once ("instant_kill"), 7 no damage left
// ("no_valid_damage"); 0 another of the game's reasons. "dodged"; "reason 9" for one it doesn't know.
std::string HitEndedName(int32_t aWhy);

// A hit on V that the module's DealDamages wrap took, in the game's terms: the health it would have taken, after V's
// armour, the difficulty and V's overshield; its gamedataAttackType, as a number; its instigator's entity, or 0; and
// whether the game won't let it kill V (hitFlag.CannotKillPlayer, or a minimum health left), so it stays V's.
```
```cpp
	uint64_t instigator = 0;
```
```cpp
	uint64_t instigator = 0;
	bool     nonLethal = false;
```
```cpp
	bool clearDue_ = false;
};
}
```
```cpp
	bool clearDue_ = false;
};

// One of V's states that BlockRender switches through the module while it's wanted: V's unarmed weapon drawn (the
// module's Arm), and V's meshes hidden (HideV). Wanted, it's switched on, and on again every aRefresh seconds while it
// stays wanted, so that what appears meanwhile, such as the weapon once it's drawn, is caught too; not wanted, off, once.
// Losing V, as during a load, counts as off with nothing to switch back: the next session's V starts as its save left it.
class VSwitch
{
public:
	explicit VSwitch(double aRefresh = 0.0) : refresh_(aRefresh) {}

	// The frame's state at aNow (seconds). The call to make now, through the module: on (true) or off (false). Nothing
	// while V isn't found.
	std::optional<bool> Frame(bool aWanted, bool aVFound, double aNow);
	bool                On() const { return on_; }

private:
	double refresh_ = 0.0;  // seconds between calls while on; 0: once
	bool   on_ = false;
	double onAt_ = 0.0;     // when on was last called
};
}
```
- [x] **Step 4: Edit `src/link/Combat.cpp`**

```cpp
#include <format>
```
```cpp
#include <format>
#include <utility>
```
```cpp
		return std::format("result {}", aResult);
```
```cpp
		return std::format("result {}", aResult);
	}
}

std::string LandedWords(uint32_t aFlags)
{
	constexpr std::array<std::pair<uint32_t, const char*>, 6> kWords{ { { kLandedCritical, "critical" },
		                                                                { kLandedBlocked, "blocked" },
		                                                                { kLandedParried, "parried" },
		                                                                { kLandedArmour, "armour" },
		                                                                { kLandedMitigated, "mitigated" },
		                                                                { kLandedKilled, "killed it" } } };
	std::string words;
	for (const auto& [flag, word] : kWords)
	{
		if ((aFlags & flag) != 0)
		{
			words += (words.empty() ? "" : ", ") + std::string(word);
		}
	}
	return words;
}

std::string HitEndedName(int32_t aWhy)
{
	switch (aWhy)
	{
	case 0:
		return "another of the game's reasons";
	case 1:
		return "friendly to V";
	case 2:
		return "invulnerable";
	case 3:
		return "already dead";
	case 4:
		return "dodged";
	case 5:
		return "evaded";
	case 6:
		return "killed at once";
	case 7:
		return "no damage left";
	default:
		return std::format("reason {}", aWhy);
```
```cpp
	return wanted;
}
}
```
```cpp
	return wanted;
}

std::optional<bool> VSwitch::Frame(bool aWanted, bool aVFound, double aNow)
{
	if (!aVFound)
	{
		on_ = false;
		return std::nullopt;
	}
	if (aWanted && (!on_ || (refresh_ > 0.0 && aNow - onAt_ >= refresh_)))
	{
		on_ = true;
		onAt_ = aNow;
		return true;
	}
	if (!aWanted && on_)
	{
		on_ = false;
		return false;
	}
	return std::nullopt;
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
Expected: no error or warning lines, then `437 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: what the pipeline did to our hits, in words; V's non-lethal hurts; and V's switches for its weapon and meshes" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 24: The inbox keeps what the pipeline did to our hits and why it dealt some nothing, and leaves V's non-lethal hurts to V

The natives the pipeline's wraps call change with what they report, and the inbox keeps it:
- **`BlockRenderHitLanded(target, amount, before, after, flags)`:** `amount` is now what the pipeline made of our hit,
  after all its modifiers, and `flags` what it did to it (Task 23's `LandedFlag`).
- **`BlockRenderHitEnded(target, why)`,** new: one of our hits the pipeline dealt nothing, and why (`HitEndedName`). The
  inbox keeps these as endings, at most 256 waiting, as it keeps landings.
- **`BlockRenderVHurt(amount, attackType, instigator, nonLethal)`:** a hit on V the game won't let kill V is refused while
  the inbox takes hurts, and counted apart: it stays V's.

`GameScript` checks all seven natives. The module's declarations and calls follow here, so that it keeps compiling against
them; Task 26 rewrites the wraps that call them.

**Files:**
- Modify: `src/CombatInbox.h`, `src/CombatInbox.cpp`, `tests/TestCombatInbox.cpp`, `src/Natives.h`, `src/Natives.cpp`,
  `src/GameScript.cpp`, `scripts/BlockRender.reds`

- [x] **Step 1: Edit `tests/TestCombatInbox.cpp`: landings' flags, endings and non-lethal hurts**

```cpp
	inbox.Land({ 7, 714.0, 1514.0, 800.0 });
```
```cpp
	inbox.Land({ 7, 714.0, 1514.0, 800.0, link::kLandedCritical | link::kLandedArmour });
```
```cpp
	      landed[0].after == 800.0);
```
```cpp
	      landed[0].after == 800.0 && landed[0].flags == (link::kLandedCritical | link::kLandedArmour));
```
```cpp
	CHECK_EQ(inbox.Count().dropped, uint64_t(1));
```
```cpp
	CHECK_EQ(inbox.Count().dropped, uint64_t(1));
}

TEST(CombatInboxKeepsEndingsAndLeavesNonLethalHurtsToV)
{
	CombatInbox inbox;
	// Our hits the pipeline dealt nothing, with why: dodged, and friendly to V.
	inbox.End({ 7, 4 });
	inbox.End({ 8, 1 });
	const std::vector<CombatInbox::Ended> ended = inbox.TakeEnded();
	CHECK_EQ(ended.size(), size_t(2));
	CHECK(ended.size() == 2 && ended[0].target == 7 && ended[0].why == 4 && ended[1].target == 8 && ended[1].why == 1);
	CHECK(inbox.TakeEnded().empty());
	for (size_t i = 0; i <= CombatInbox::kMaxWaiting; ++i)
	{
		inbox.End({ 9, 5 });
	}
	CHECK_EQ(inbox.TakeEnded().size(), CombatInbox::kMaxWaiting);
	CHECK_EQ(inbox.Count().ended, uint64_t(2 + CombatInbox::kMaxWaiting));
	CHECK_EQ(inbox.Count().dropped, uint64_t(1));
	// A hit on V that the game won't let kill V stays V's while taking, counted apart; while not taking, it's refused as
	// any other is.
	inbox.SetTaking(true);
	CHECK(!inbox.Offer({ 30.0, 7, 0x51, true }));
	CHECK(inbox.Offer({ 30.0, 7, 0x51, false }));
	inbox.SetTaking(false);
	CHECK(!inbox.Offer({ 30.0, 7, 0x51, true }));
	const CombatInbox::Counts counts = inbox.Count();
	CHECK_EQ(counts.nonLethal, uint64_t(1));
	CHECK_EQ(counts.refused, uint64_t(1));
	CHECK_EQ(counts.taken, uint64_t(1));
	CHECK_EQ(inbox.TakeHurts().size(), size_t(1));
```
```cpp
				inbox.Land({ uint64_t(t + 1), 1.0, 2.0, 1.0 });
```
```cpp
				inbox.Land({ uint64_t(t + 1), 1.0, 2.0, 1.0 });
				inbox.End({ uint64_t(t + 1), 4 });
```
```cpp
	size_t landed = 0;
```
```cpp
	size_t landed = 0;
	size_t ended = 0;
```
```cpp
		landed += inbox.TakeLanded().size();
```
```cpp
		landed += inbox.TakeLanded().size();
		ended += inbox.TakeEnded().size();
```
```cpp
	landed += inbox.TakeLanded().size();
	const CombatInbox::Counts counts = inbox.Count();
```
```cpp
	landed += inbox.TakeLanded().size();
	ended += inbox.TakeEnded().size();
	const CombatInbox::Counts counts = inbox.Count();
```
```cpp
	CHECK_EQ(counts.landed + counts.dropped, uint64_t(kThreads * kEach));
```
```cpp
	CHECK_EQ(uint64_t(ended), counts.ended);
	CHECK_EQ(counts.landed + counts.ended + counts.dropped, uint64_t(2 * kThreads * kEach));
```
- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'End': is not a member of 'cpmc::CombatInbox'`.

- [x] **Step 3: Edit `src/CombatInbox.h`**

```cpp
// V's otherwise; and our hits on NPCs as they land, with the NPC's health before and after. The main thread takes both
// each frame.
class CombatInbox
{
public:
	static constexpr size_t kMaxWaiting = 256;  // of each: a hurt past it is left to V, a landing is only counted
```
```cpp
// V's otherwise; our hits on NPCs as they land, with what the pipeline made of them and the NPC's health before and
// after; and those the pipeline dealt nothing, with why. The main thread takes them each frame.
class CombatInbox
{
public:
	static constexpr size_t kMaxWaiting = 256;  // of each: a hurt past it is left to V, a landing or an ending is only counted
```
```cpp
		double   amount = 0.0;
		double   before = 0.0;  // the NPC's health
		double   after = 0.0;
```
```cpp
		double   amount = 0.0;  // what the pipeline made of it, before the NPC's health took it
		double   before = 0.0;  // the NPC's health
		double   after = 0.0;
		uint32_t flags = 0;     // link::LandedFlag
	};

	// One of our hits that the pipeline dealt nothing.
	struct Ended
	{
		uint64_t target = 0;
		int32_t  why = 0;  // link::HitEndedName
```
```cpp
		uint64_t taken = 0;    // hits on V taken from the game
		uint64_t refused = 0;  // offered while not taking, or with kMaxWaiting waiting: left to V
		uint64_t landed = 0;   // our hits reported as dealt
		uint64_t dropped = 0;  // landings past kMaxWaiting
```
```cpp
		uint64_t taken = 0;      // hits on V taken from the game
		uint64_t refused = 0;    // offered while not taking, or with kMaxWaiting waiting: left to V
		uint64_t nonLethal = 0;  // offered while taking, but the game won't let them kill V: left to V
		uint64_t landed = 0;     // our hits reported as dealt
		uint64_t ended = 0;      // our hits reported as dealt nothing
		uint64_t dropped = 0;    // landings and endings past kMaxWaiting
```
```cpp
	void Land(const Landed& aLanded);  // any thread

	std::vector<link::GameHurt> TakeHurts();  // main thread, oldest first
	std::vector<Landed>         TakeLanded();
```
```cpp
	void Land(const Landed& aLanded);  // any thread
	void End(const Ended& aEnded);     // any thread

	std::vector<link::GameHurt> TakeHurts();  // main thread, oldest first
	std::vector<Landed>         TakeLanded();
	std::vector<Ended>          TakeEnded();
```
```cpp
	std::vector<Landed>         landed_;
```
```cpp
	std::vector<Landed>         landed_;
	std::vector<Ended>          ended_;
```
- [x] **Step 4: Edit `src/CombatInbox.cpp`**

```cpp
	const std::lock_guard lock(mutex_);
	if (!Taking() || hurts_.size() >= kMaxWaiting)
```
```cpp
	const std::lock_guard lock(mutex_);
	if (Taking() && aHurt.nonLethal)
	{
		++counts_.nonLethal;
		return false;
	}
	if (!Taking() || hurts_.size() >= kMaxWaiting)
```
```cpp
std::vector<link::GameHurt> CombatInbox::TakeHurts()
```
```cpp
void CombatInbox::End(const Ended& aEnded)
{
	const std::lock_guard lock(mutex_);
	if (ended_.size() >= kMaxWaiting)
	{
		++counts_.dropped;
		return;
	}
	ended_.push_back(aEnded);
	++counts_.ended;
}

std::vector<link::GameHurt> CombatInbox::TakeHurts()
```
```cpp
CombatInbox::Counts CombatInbox::Count() const
```
```cpp
std::vector<CombatInbox::Ended> CombatInbox::TakeEnded()
{
	const std::lock_guard lock(mutex_);
	return std::exchange(ended_, {});
}

CombatInbox::Counts CombatInbox::Count() const
```
- [x] **Step 5: Edit `src/Natives.cpp`: the three natives the pipeline's wraps call**

```cpp
// BlockRenderVHurt(amount: Float, attackType: Int32, instigator: EntityID) -> Bool, from the wrap of DealDamages, on any
// thread: a hit on V, taken (true) while combat and build mode are on, so the wrap deals none of it.
```
```cpp
// BlockRenderVHurt(amount: Float, attackType: Int32, instigator: EntityID, nonLethal: Bool) -> Bool, from the wrap of
// DealDamages, on any thread: a hit on V, taken (true) while combat and build mode are on, so the wrap deals none of it;
// one the game won't let kill V stays V's.
```
```cpp
	RED4ext::ent::EntityID instigator;
	RED4ext::GetParameter(aFrame, &amount);
	RED4ext::GetParameter(aFrame, &attackType);
	RED4ext::GetParameter(aFrame, &instigator);
	++aFrame->code;
	const bool taken = g_combat.Offer({ double(amount), attackType, instigator.hash });
```
```cpp
	RED4ext::ent::EntityID instigator;
	bool                   nonLethal = false;
	RED4ext::GetParameter(aFrame, &amount);
	RED4ext::GetParameter(aFrame, &attackType);
	RED4ext::GetParameter(aFrame, &instigator);
	RED4ext::GetParameter(aFrame, &nonLethal);
	++aFrame->code;
	const bool taken = g_combat.Offer({ double(amount), attackType, instigator.hash, nonLethal });
```
```cpp
// BlockRenderHitLanded(target: EntityID, amount: Float, before: Float, after: Float), from the wrap of DealDamages, on any
// thread: one of our hits, dealt, with the NPC's health before and after.
```
```cpp
// BlockRenderHitLanded(target: EntityID, amount: Float, before: Float, after: Float, flags: Int32), from the wrap of
// DealDamages, on any thread: one of our hits, dealt: what the pipeline made of it, the NPC's health before and after,
// and what the pipeline did to it (link::LandedFlag).
```
```cpp
	float                  after = 0.0f;
```
```cpp
	float                  after = 0.0f;
	int32_t                flags = 0;
```
```cpp
	RED4ext::GetParameter(aFrame, &after);
	++aFrame->code;
	g_combat.Land({ target.hash, double(amount), double(before), double(after) });
```
```cpp
	RED4ext::GetParameter(aFrame, &after);
	RED4ext::GetParameter(aFrame, &flags);
	++aFrame->code;
	g_combat.Land({ target.hash, double(amount), double(before), double(after), uint32_t(flags) });
}

// BlockRenderHitEnded(target: EntityID, why: Int32), from the wrap of ProcessPipeline, on any thread: one of our hits, dealt
// nothing, and why (link::HitEndedName).
void HitEnded(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, void*, int64_t)
{
	RED4ext::ent::EntityID target;
	int32_t                why = 0;
	RED4ext::GetParameter(aFrame, &target);
	RED4ext::GetParameter(aFrame, &why);
	++aFrame->code;
	g_combat.End({ target.hash, why });
```
```cpp
	typed = Add<bool*>(*rtti, "BlockRenderVHurt", &VHurt, { { "Float", "amount" }, { "Int32", "attackType" }, { "entEntityID", "instigator" } },
	                   "Bool") &&
	        typed;
	typed = Add<void*>(*rtti, "BlockRenderHitLanded", &HitLanded,
	                   { { "entEntityID", "target" }, { "Float", "amount" }, { "Float", "before" }, { "Float", "after" } }, nullptr) &&
	        typed;
	if (g_nativesLog)
	{
		g_nativesLog->Push(typed ? "natives: BlockRenderNpcAttached, BlockRenderNpcDetached, BlockRenderNpcCount, BlockRenderNpcAt, "
		                           "BlockRenderVHurt and BlockRenderHitLanded added"
```
```cpp
	typed = Add<bool*>(*rtti, "BlockRenderVHurt", &VHurt,
	                   { { "Float", "amount" }, { "Int32", "attackType" }, { "entEntityID", "instigator" }, { "Bool", "nonLethal" } }, "Bool") &&
	        typed;
	typed = Add<void*>(*rtti, "BlockRenderHitLanded", &HitLanded,
	                   { { "entEntityID", "target" }, { "Float", "amount" }, { "Float", "before" }, { "Float", "after" }, { "Int32", "flags" } },
	                   nullptr) &&
	        typed;
	typed = Add<void*>(*rtti, "BlockRenderHitEnded", &HitEnded, { { "entEntityID", "target" }, { "Int32", "why" } }, nullptr) && typed;
	if (g_nativesLog)
	{
		g_nativesLog->Push(typed ? "natives: BlockRenderNpcAttached, BlockRenderNpcDetached, BlockRenderNpcCount, BlockRenderNpcAt, "
		                           "BlockRenderVHurt, BlockRenderHitLanded and BlockRenderHitEnded added"
```
- [x] **Step 6: Edit `src/Natives.h`**

```cpp
// pipeline offer hits on V and report our hits landing through them, on whichever thread runs the pipeline (Part 2).
```
```cpp
// pipeline offer hits on V, and report our hits landing or dealt nothing, through them, on whichever thread runs the
// pipeline (Part 2).
```
```cpp
// The hits on V the pipeline's wraps offer, and our hits as they land.
```
```cpp
// The hits on V the pipeline's wraps offer, and our hits as they land or end dealt nothing.
```
- [x] **Step 7: Edit `src/GameScript.cpp`: seven natives to check for**

```cpp
constexpr std::array<const char*, 6> kNatives{ "BlockRenderNpcAttached", "BlockRenderNpcDetached", "BlockRenderNpcCount",
	                                           "BlockRenderNpcAt",       "BlockRenderVHurt",       "BlockRenderHitLanded" };
```
```cpp
constexpr std::array<const char*, 7> kNatives{ "BlockRenderNpcAttached", "BlockRenderNpcDetached", "BlockRenderNpcCount",
	                                           "BlockRenderNpcAt",       "BlockRenderVHurt",       "BlockRenderHitLanded",
	                                           "BlockRenderHitEnded" };
```
- [x] **Step 8: Edit `scripts/BlockRender.reds`: the natives as they are now**

```swift
// For the DealDamages wrap: a hit on V, which BlockRender takes (true) for Minecraft while combat and build mode are on;
// and one of our hits, dealt, with the NPC's health before and after.
public native func BlockRenderVHurt(amount: Float, attackType: Int32, instigator: EntityID) -> Bool
public native func BlockRenderHitLanded(target: EntityID, amount: Float, before: Float, after: Float)
```
```swift
// For the damage pipeline's wraps: a hit on V, which BlockRender takes (true) for Minecraft while combat and build mode
// are on, unless the game won't let it kill V; one of our hits, dealt, with what the pipeline made of it, the NPC's
// health before and after, and what the pipeline did to it; and one of our hits the pipeline dealt nothing, with why.
public native func BlockRenderVHurt(amount: Float, attackType: Int32, instigator: EntityID, nonLethal: Bool) -> Bool
public native func BlockRenderHitLanded(target: EntityID, amount: Float, before: Float, after: Float, flags: Int32)
public native func BlockRenderHitEnded(target: EntityID, why: Int32)
```
```swift
    BlockRenderHitLanded(target.GetEntityID(), amount, before, pools.GetStatPoolValue(stats, gamedataStatPoolType.Health, false));
```
```swift
    BlockRenderHitLanded(target.GetEntityID(), amount, before, pools.GetStatPoolValue(stats, gamedataStatPoolType.Health, false), 0);
```
```swift
      if BlockRenderVHurt(total, EnumInt(hitEvent.attackData.GetAttackType()), from) {
```
```swift
      if BlockRenderVHurt(total, EnumInt(hitEvent.attackData.GetAttackType()), from, false) {
```
- [x] **Step 9: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `438 test(s), 0 failure(s)`.

- [x] **Step 10: Compile the module against the game's scripts**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && uv run --no-project python tools/check_reds.py E:/Cyberpunk2077 scripts; \
  echo "exit: $?"
```
Expected: redscript's `Compilation complete`, then `compiled` and `exit: 0`.

- [x] **Step 11: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: the inbox keeps what the pipeline did to our hits and why it dealt some nothing, and leaves V's non-lethal hurts to V" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 25: `check_reds.py` compiles the module with Codeware's scripts too

Hiding V's meshes (Task 26) needs Codeware's `Entity.GetComponents`, which lists an entity's components: the game's own
scripts can't. Codeware 1.19 is installed, but the module mustn't fail to compile without it, since a failed compile
takes every mod's scripts with it (the spec's risk 9). So the parts that use Codeware are compiled only where it's
installed, with redscript's `@if(ModuleExists("Codeware"))`, and the parts for its absence otherwise. The installed
redscript honours that both ways (checked while reviewing: a broken line in a Codeware-only function compiled without
Codeware's scripts, and failed with them). `check_reds.py` takes more script folders after the module's, so both ways
can be compiled before deploying.

**Files:**
- Modify: `tools/check_reds.py`

- [x] **Step 1: Edit `tools/check_reds.py`**

```python
    uv run --no-project python tools/check_reds.py <game folder> <scripts folder>

It loads redscript's compiler library from the game (engine/tools/scc_lib.dll), with its error popup off, and compiles
the scripts folder against a copy of the game's untouched r6/cache/final.redscripts, so the game's own files aren't
touched. It prints "compiled" and exits 0, or prints redscript's errors and exits 1. Its work goes in build/reds-check.
The game must be installed; it needn't run, and may.
```
```python
    uv run --no-project python tools/check_reds.py <game folder> <scripts folder> [<more script folders>...]

It loads redscript's compiler library from the game (engine/tools/scc_lib.dll), with its error popup off, and compiles
the scripts folder, with any more folders given, against a copy of the game's untouched r6/cache/final.redscripts, so the
game's own files aren't touched. Codeware's Scripts folder, given too, compiles the module's parts that need Codeware
(@if(ModuleExists("Codeware"))), as the game does with Codeware installed; without it, the parts for its absence. It
prints "compiled" and exits 0, or prints redscript's errors and exits 1. Its work goes in build/reds-check. The game must
be installed; it needn't run, and may.
```
```python
if len(sys.argv) != 3:
    sys.exit(__doc__)
game, scripts = Path(sys.argv[1]), Path(sys.argv[2]).resolve()
```
```python
if len(sys.argv) < 3:
    sys.exit(__doc__)
game, scripts = Path(sys.argv[1]), Path(sys.argv[2]).resolve()
more = [Path(folder).resolve() for folder in sys.argv[3:]]
```
```python
lib.scc_settings_add_script_path(settings, str(scripts).encode())
```
```python
lib.scc_settings_add_script_path(settings, str(scripts).encode())
for folder in more:
    lib.scc_settings_add_script_path(settings, str(folder).encode())
```
- [x] **Step 2: Compile the module against the game's scripts, without and with Codeware**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && uv run --no-project python tools/check_reds.py E:/Cyberpunk2077 scripts | tail -1; \
  echo "without Codeware: exit ${PIPESTATUS[0]}"; \
  uv run --no-project python tools/check_reds.py E:/Cyberpunk2077 scripts E:/Cyberpunk2077/red4ext/plugins/Codeware/Scripts | tail -1; \
  echo "with Codeware: exit ${PIPESTATUS[0]}"
```
Expected: `compiled`, `without Codeware: exit 0` and `with Codeware: exit 0`.

- [x] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: check_reds compiles the module with Codeware's scripts too" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 26: The module starts our hits at the pipeline's entry, punches with V's unarmed slot, draws it and hides V for build mode

The module's side of the review, in `scripts/BlockRender.reds`, version 4:
- **Our hits go through the whole pipeline.** A wrap of `DamageSystem.ProcessPipeline` (`damageSystem.swift`'s `private
  final func ProcessPipeline(hitEvent: ref<gameHitEvent>, cache: ref<CacheData>)`), the pipeline's entry, before even
  its first stage's callbacks, starts our hit at the amount `Hit` marked on it, Minecraft's damage × V's punch, split
  across damage types as the attack splits its own (`StartAt`; all physical when it has none, as without a weapon). The
  pipeline then does to it all it does to V's own punches. After it, a hit flagged `DealNoDamage` reports why
  (`BlockRenderHitEnded`, from the flag's source name: `WhyNoDamage`).
- **The `Process` wrap goes:** a hit that starts with damage is never marked `n"no_valid_damage"`. (It also let dodged and
  evaded hits through: `ProcessDodge` and `ProcessEvasion` set `DealNoDamage` after `no_valid_damage`, and `AddFlag`
  keeps one entry a flag, so theirs never went in.)
- **The `DealDamages` wrap** no longer sets our hit's damage: it reports what the pipeline made of it
  (`GetTotalAttackValue(Health)`), the NPC's health before and after, and what the pipeline did to it (`LandedFlags`).
  For a hit on V, the part V's overshield would take stays in the game, and only the rest is offered; a hit the game
  won't let kill V is offered as non-lethal, which the inbox leaves V's.
- **What V punches with** is what V's unarmed slot draws (`UnarmedItem`): weapon-wheel slot 4, the game's
  `GetMeleewareOrFistsItemID`, is V's arm cyberware weapon if V has one (`EquipmentSystemPlayerData.GetActiveMeleeWare`),
  or the fists or Gorilla Arms in `BaseFists`. Arm cyberware without a melee attack to make hits with (a Monowire, the
  Projectile Launch System) falls back to `BaseFists`. V's punch, the attack records and the weapon object all come
  from it, and `List` says whether V has it drawn (`punchDrawn`).
- **`Arm(on)`** draws V's unarmed weapon for build mode's fights, so that hits carry it, as V's own punches do: the
  NPC's armour, the weapon's crits, the stealth bonus, blocks, the skill XP and the perks that check the weapon all read
  the hit's weapon. It asks the equipment system for weapon-wheel slot 4 at once (`RequestWeaponSlot4`, `Instant`), as
  `StatusEffectHelper`'s requests do; off, it asks for the wheel slot V's weapon was in, or holsters
  (`UnequipWeapon`).
- **`HideV(on)`** hides V for build mode: every visual component (`IVisualComponent`) of V's, and of each item V wears
  and holds, that's on, is switched off (`IComponent.Toggle`) and remembered, so that showing V again switches on
  exactly those, and leaves off what the game had off. The first call finds what V wears among V's whole inventory;
  called again while on, it hides what has appeared since on V and in its hands and arm slot, such as the weapon `Arm`
  drew, without looking through the inventory again. Codeware lists the components; without it, `HideV` says so (-1)
  and V stays seen.

The pipeline's methods are `damageSystem.swift`'s; a wrap of a method that isn't there fails the compile, and so does a
slip in the parts for Codeware, but only where Codeware is installed: Steps 3 and 4 check both.

**Files:**
- Modify: `scripts/BlockRender.reds`

- [x] **Step 1: Edit `scripts/BlockRender.reds`**

```swift
// Combat (spike 6's Part 2): Hit makes one of Minecraft's hits into a hit of V's on an NPC, marked as ours with its
// amount on the hit itself, and the wraps of the damage pipeline's Process and DealDamages, at the end of this file, deal
// it exactly; the same DealDamages wrap offers the game's hits on V to BlockRender, which takes them for Minecraft in
// build mode. The pipeline may run on any thread, so those wraps too touch nothing of the module's: they read the hit
// itself, and call BlockRender's natives.
```
```swift
// Combat (spike 6's Part 2): Hit makes one of Minecraft's hits into a hit of V's on an NPC, made with what V punches
// with, and marked as ours, with where it starts, on the hit itself. The wrap of the damage pipeline's ProcessPipeline, at
// the end of this file, starts it there, and the pipeline does to it all it does to V's own punches; the DealDamages wrap
// reports what it took off, and the ProcessPipeline wrap why it took nothing, if it didn't. The same DealDamages wrap
// offers the game's hits on V to BlockRender, which takes them for Minecraft in build mode. The pipeline may run on any
// thread, so those wraps too touch nothing of the module's: they read the hit itself, and call BlockRender's natives.
// For build mode, Arm draws V's unarmed weapon, so that hits carry it, and HideV hides V's meshes, through Codeware where
// it's installed.
```
```swift
  private let vRead: Bool;
```
```swift
  private let vRead: Bool;
  // Arm's: whether it drew V's unarmed weapon, and what V held before, to put back. HideV's: whether V is hidden, and the
  // meshes it switched off.
  private let armed: Bool;
  private let armedFrom: ItemID;
  private let hiding: Bool;
  private let hidden: array<wref<IComponent>>;
```
```swift
  // V's punch, the damage per hit of what V punches with (the item in the BaseFists area), and that item; and V's max
  // health. Read at most four times a second.
  public let punch: Float;
  public let punchItem: String;
  public let vHealthMax: Float;

  // The module's version, which BlockRender checks: 2 since the list moved into BlockRender, 3 with combat.
  public final static func Version() -> Int32 {
    return 3;
```
```swift
  // V's punch, the damage per hit of what V punches with (what V's unarmed slot draws: UnarmedItem), that item, and
  // whether V has it drawn; and V's max health. Read at most four times a second.
  public let punch: Float;
  public let punchItem: String;
  public let punchDrawn: Bool;
  public let vHealthMax: Float;

  // The module's version, which BlockRender checks: 2 since the list moved into BlockRender, 3 with combat, 4 since hits
  // go through the whole damage pipeline, with what V's unarmed slot draws.
  public final static func Version() -> Int32 {
    return 4;
```
```swift
  // One of Minecraft's hits on an NPC, as V's: amount is what it takes off the NPC's health, and strong picks the strong
  // attack. Built as the game's SendHitEvent builds one, from V, with the attack record of what V punches with, and the
  // weapon object of it if V has it drawn. 1: queued; 0: V isn't found; -1: the NPC is gone; -2: it's dead; -3: there's no
  // attack record to make it with.
```
```swift
  // One of Minecraft's hits on an NPC, as V's: amount is where the damage pipeline starts it, Minecraft's damage x V's
  // punch, and strong picks the strong attack. Built as the game's SendHitEvent builds one, from V, with the attack record
  // of what V punches with, and the weapon object of it if V has it drawn. 1: queued; 0: V isn't found; -1: the NPC is
  // gone; -2: it's dead; -3: there's no attack record to make it with.
```
```swift
    let record: ref<Attack_Record> = this.PunchAttack(player, strong);
    if !IsDefined(record) {
      return -3;
    };
    let weapon: ref<WeaponObject> = this.PunchWeapon(player, game);
```
```swift
    let item: ItemID = this.UnarmedItem(player);
    let record: ref<Attack_Record> = this.AttackOf(item, strong);
    if !IsDefined(record) {
      return -3;
    };
    let weapon: ref<WeaponObject> = this.PunchWeapon(player, game, item);
```
```swift
    // Ours, with its amount, on the hit itself, as the game marks its environment's attacks with hitFlag.None and a
```
```swift
    // Ours, with where it starts, on the hit itself, as the game marks its environment's attacks with hitFlag.None and a
```
```swift
  // For the punch request: V's punch, what it's read from, and what Hit makes hits with.
```
```swift
  // V's unarmed weapon, drawn for build mode's fights so that Minecraft's hits carry it, as V's own punches do (on); or
  // what V held before, put back (off). Drawing asks the equipment system for weapon-wheel slot 4 at once, as the slot's
  // key does; putting back asks for the wheel's slot V's weapon is in, or holsters if V held nothing, or something not on
  // the wheel. Called again while on, it asks again if the weapon isn't drawn yet. 1: asked; 2: nothing to ask; 0: V isn't
  // found; -1: V has no unarmed weapon; -2: there's no equipment system.
  public final func Arm(on: Bool) -> Int32 {
    let game: GameInstance = this.GetGameInstance();
    let player: ref<GameObject> = GameInstance.GetPlayerSystem(game).GetLocalPlayerMainGameObject();
    if !IsDefined(player) {
      return 0;
    };
    let equipment: ref<EquipmentSystem> = EquipmentSystem.GetInstance(player);
    if !IsDefined(equipment) {
      return -2;
    };
    let held: ItemID;
    let heldObject: ref<ItemObject> = GameInstance.GetTransactionSystem(game).GetItemInSlot(player, t"AttachmentSlots.WeaponRight");
    if IsDefined(heldObject) {
      held = heldObject.GetItemID();
    };
    let request: ref<EquipmentSystemWeaponManipulationRequest> = new EquipmentSystemWeaponManipulationRequest();
    request.owner = player;
    request.equipAnimType = gameEquipAnimationType.Instant;
    if on {
      let item: ItemID = this.UnarmedItem(player);
      if !ItemID.IsValid(item) {
        return -1;
      };
      if !this.armed {
        this.armed = true;
        this.armedFrom = held;
      };
      if held == item {
        return 2;
      };
      request.requestType = EquipmentManipulationAction.RequestWeaponSlot4;
      equipment.QueueRequest(request);
      return 1;
    };
    if !this.armed {
      return 2;
    };
    this.armed = false;
    if held == this.armedFrom {
      return 2;
    };
    request.requestType = this.WheelRequest(player, this.armedFrom);
    equipment.QueueRequest(request);
    return 1;
  }

  // V's meshes for build mode: V's own, and those of everything V wears and holds, hidden (on), each switched off if it
  // was on, and remembered, so that showing V again (off) switches on exactly those and leaves off what the game had off.
  // Called again while on, it hides what has appeared since on V and in V's hands, such as the weapon Arm drew, without
  // looking through V's whole inventory again. The meshes hidden, or shown again; -1 without Codeware, which lists an
  // entity's components; -2 if V isn't found.
  public final func HideV(on: Bool) -> Int32 {
    let i: Int32 = 0;
    if !on {
      let shown: Int32 = 0;
      while i < ArraySize(this.hidden) {
        let component: ref<IComponent> = this.hidden[i];
        if IsDefined(component) {
          component.Toggle(true);
          shown += 1;
        };
        i += 1;
      };
      ArrayClear(this.hidden);
      this.hiding = false;
      return shown;
    };
    if !BlockRenderCanHideV() {
      return -1;
    };
    let player: ref<GameObject> = GameInstance.GetPlayerSystem(this.GetGameInstance()).GetLocalPlayerMainGameObject();
    if !IsDefined(player) {
      return -2;
    };
    let off: array<wref<IComponent>> = BlockRenderSwitchOffMeshes(player, !this.hiding);
    this.hiding = true;
    while i < ArraySize(off) {
      ArrayPush(this.hidden, off[i]);
      i += 1;
    };
    return ArraySize(this.hidden);
  }

  // For the punch request: V's punch, what it's read from, what Hit makes hits with, and whether V has it drawn.
```
```swift
    let drawn: ref<WeaponObject> = this.PunchWeapon(player, game);
    return "V's punch " + FloatToStringPrec(this.punch, 2) + ", read from " + this.punchItem + "; quick hits with "
      + this.AttackName(this.PunchAttack(player, false)) + ", strong ones with " + this.AttackName(this.PunchAttack(player, true))
      + "; " + (IsDefined(drawn) ? "drawn, so hits carry it as their weapon" : "not drawn, so hits carry no weapon");
  }

  // A hit's amount, if it's ours: from its hitFlag.None marked "BlockRender <amount>". -1 if it isn't. Any thread: it
  // reads only the hit.
```
```swift
    let item: ItemID = this.UnarmedItem(player);
    return "V's punch " + FloatToStringPrec(this.punch, 2) + ", read from " + this.punchItem + "; quick hits with "
      + this.AttackName(this.AttackOf(item, false)) + ", strong ones with " + this.AttackName(this.AttackOf(item, true)) + "; "
      + (this.punchDrawn ? "drawn, so hits carry it as their weapon"
                         : "not drawn, so hits carry no weapon, and skip the NPC's armour, the weapon's crits, stealth, blocks and the skill XP");
  }

  // Where one of our hits starts, if it's ours: from its hitFlag.None marked "BlockRender <amount>". -1 if it isn't. Any
  // thread: it reads only the hit.
```
```swift
  // The attack record of what V punches with, from its item's record: its strong melee attack for a strong hit, its light
  // one otherwise (gamedataAttackType.Melee: QuickMelee is a gun's quick melee); of neither, its first melee attack.
  private final func PunchAttack(player: ref<GameObject>, strong: Bool) -> ref<Attack_Record> {
    let items: array<ItemID> = EquipmentSystem.GetItemsInArea(player, gamedataEquipmentArea.BaseFists);
    if ArraySize(items) == 0 {
      return null;
    };
    let weapon: ref<WeaponItem_Record> = TweakDBInterface.GetWeaponItemRecord(ItemID.GetTDBID(items[0]));
```
```swift
  // The attack's damage set to amount in all, split across damage types as the attack splits its own; all physical if it
  // has none, as without a weapon. Any thread.
  public final static func StartAt(computed: ref<gameAttackComputed>, amount: Float) -> Void {
    let values: array<Float> = computed.GetAttackValues();
    let total: Float = 0.00;
    let i: Int32 = 0;
    while i < ArraySize(values) {
      if values[i] > 0.00 {
        total += values[i];
      };
      i += 1;
    };
    if total <= 0.00 {
      BlockRenderNpcs.SetDamage(computed, amount);
      return;
    };
    i = 0;
    while i < ArraySize(values) {
      values[i] = values[i] > 0.00 ? values[i] * amount / total : 0.00;
      i += 1;
    };
    computed.SetAttackValues(values);
  }

  // What the pipeline did to one of our hits, for BlockRenderHitLanded: critical 1, blocked 2, parried 4, the NPC's armour
  // 8 (the hit type Glance, of a hit that isn't critical), mitigated 16, killed the NPC 32. Any thread.
  public final static func LandedFlags(data: ref<AttackData>) -> Int32 {
    let flags: Int32 = 0;
    if data.HasFlag(hitFlag.CriticalHit) {
      flags += 1;
    };
    if data.HasFlag(hitFlag.WasBlocked) {
      flags += 2;
    };
    if data.HasFlag(hitFlag.WasDeflected) {
      flags += 4;
    };
    if Equals(data.GetHitType(), gameuiHitType.Glance) {
      flags += 8;
    };
    if data.HasFlag(hitFlag.WasMitigated) {
      flags += 16;
    };
    if data.HasFlag(hitFlag.WasKillingBlow) {
      flags += 32;
    };
    return flags;
  }

  // Why the pipeline dealt one of our hits nothing, from the source name its DealNoDamage flag was set under: 1 friendly to
  // V, 2 invulnerable, 3 already dead, 4 dodged, 5 evaded, 6 killed at once, 7 no damage left; 0 another reason. Any thread.
  public final static func WhyNoDamage(data: ref<AttackData>) -> Int32 {
    let flags: array<SHitFlag> = data.GetFlags();
    let i: Int32 = 0;
    while i < ArraySize(flags) {
      if Equals(flags[i].flag, hitFlag.DealNoDamage) {
        let source: CName = flags[i].source;
        if Equals(source, n"FriendlyFireIgnored") {
          return 1;
        };
        if Equals(source, n"invulnerable") || Equals(source, n"invulnerable stat flag") {
          return 2;
        };
        if Equals(source, n"dead") {
          return 3;
        };
        if Equals(source, n"ProcessDodge") {
          return 4;
        };
        if Equals(source, n"ProcessEvasion") {
          return 5;
        };
        if Equals(source, n"instant_kill") {
          return 6;
        };
        if Equals(source, n"no_valid_damage") {
          return 7;
        };
        return 0;
      };
      i += 1;
    };
    return 0;
  }

  // What V punches with: what V's unarmed slot draws (weapon-wheel slot 4: the game's GetMeleewareOrFistsItemID), V's arm
  // cyberware weapon if it has a melee attack to make hits with, otherwise what's in the BaseFists area, the fists or
  // Gorilla Arms. None if there's neither.
  private final func UnarmedItem(player: ref<GameObject>) -> ItemID {
    let data: ref<EquipmentSystemPlayerData> = EquipmentSystem.GetData(player);
    if IsDefined(data) {
      let ware: ItemID = data.GetActiveMeleeWare();
      if ItemID.IsValid(ware) && IsDefined(this.AttackOf(ware, false)) {
        return ware;
      };
    };
    let fists: array<ItemID> = EquipmentSystem.GetItemsInArea(player, gamedataEquipmentArea.BaseFists);
    if ArraySize(fists) > 0 {
      return fists[0];
    };
    return ItemID.None();
  }

  // The attack record to make a hit with, from item's record: its strong melee attack for a strong hit, its light one
  // otherwise (gamedataAttackType.Melee: QuickMelee is a gun's quick melee); of neither, its first melee attack.
  private final func AttackOf(item: ItemID, strong: Bool) -> ref<Attack_Record> {
    if !ItemID.IsValid(item) {
      return null;
    };
    let weapon: ref<WeaponItem_Record> = TweakDBInterface.GetWeaponItemRecord(ItemID.GetTDBID(item));
```
```swift
  // What V punches with, as the weapon object V holds, if V has it drawn; none otherwise.
  private final func PunchWeapon(player: ref<GameObject>, game: GameInstance) -> ref<WeaponObject> {
    let items: array<ItemID> = EquipmentSystem.GetItemsInArea(player, gamedataEquipmentArea.BaseFists);
    let held: ref<WeaponObject> = GameInstance.GetTransactionSystem(game).GetItemInSlot(player, t"AttachmentSlots.WeaponRight") as WeaponObject;
    if ArraySize(items) == 0 || !IsDefined(held) {
      return null;
    };
    if ItemID.GetTDBID(held.GetItemID()) == ItemID.GetTDBID(items[0]) {
      return held;
    };
    return null;
```
```swift
  // What V punches with, item, as the weapon object V holds, if V has it drawn; none otherwise.
  private final func PunchWeapon(player: ref<GameObject>, game: GameInstance, item: ItemID) -> ref<WeaponObject> {
    let held: ref<WeaponObject> = GameInstance.GetTransactionSystem(game).GetItemInSlot(player, t"AttachmentSlots.WeaponRight") as WeaponObject;
    if !ItemID.IsValid(item) || !IsDefined(held) {
      return null;
    };
    if ItemID.GetTDBID(held.GetItemID()) == ItemID.GetTDBID(item) {
      return held;
    };
    return null;
  }

  // The request that draws item from the weapon wheel's first three slots, by its slot; or holsters, if it isn't there or
  // is nothing.
  private final func WheelRequest(player: ref<GameObject>, item: ItemID) -> EquipmentManipulationAction {
    if ItemID.IsValid(item) {
      let wheel: array<ItemID> = EquipmentSystem.GetItemsInArea(player, gamedataEquipmentArea.WeaponWheel);
      if ArraySize(wheel) > 0 && wheel[0] == item {
        return EquipmentManipulationAction.RequestWeaponSlot1;
      };
      if ArraySize(wheel) > 1 && wheel[1] == item {
        return EquipmentManipulationAction.RequestWeaponSlot2;
      };
      if ArraySize(wheel) > 2 && wheel[2] == item {
        return EquipmentManipulationAction.RequestWeaponSlot3;
      };
    };
    return EquipmentManipulationAction.UnequipWeapon;
```
```swift
    this.punchItem = "nothing in BaseFists";
    let items: array<ItemID> = EquipmentSystem.GetItemsInArea(player, gamedataEquipmentArea.BaseFists);
    if ArraySize(items) > 0 {
      this.punchItem = TDBID.ToStringDEBUG(ItemID.GetTDBID(items[0]));
      let data: wref<gameItemData> = GameInstance.GetTransactionSystem(game).GetItemData(player, items[0]);
```
```swift
    this.punchDrawn = false;
    this.punchItem = "nothing in V's unarmed slot";
    let item: ItemID = this.UnarmedItem(player);
    if ItemID.IsValid(item) {
      this.punchItem = TDBID.ToStringDEBUG(ItemID.GetTDBID(item));
      let data: wref<gameItemData> = GameInstance.GetTransactionSystem(game).GetItemData(player, item);
```
```swift
        this.punchItem += ", without item data";
      };
    };
  }
}
```
```swift
        this.punchItem += ", without item data";
      };
      this.punchDrawn = IsDefined(this.PunchWeapon(player, game, item));
    };
  }
}

// Whether Codeware is installed: it lists an entity's components (Entity.GetComponents), which hiding V's meshes needs.
// Without it, the module still compiles, and V just isn't hidden.
@if(ModuleExists("Codeware"))
public func BlockRenderCanHideV() -> Bool {
  return true;
}

@if(!ModuleExists("Codeware"))
public func BlockRenderCanHideV() -> Bool {
  return false;
}

// V's meshes, and those of the items V wears and holds: each visual component that's on, switched off; those it switched
// off. With everything, every item V wears and holds, found among V's whole inventory; otherwise only what's in V's
// hands and arm slot, which is all that changes in build mode. Main thread, from HideV.
@if(ModuleExists("Codeware"))
public func BlockRenderSwitchOffMeshes(player: ref<GameObject>, everything: Bool) -> array<wref<IComponent>> {
  let off: array<wref<IComponent>>;
  BlockRenderSwitchOffEntity(player, off);
  let transactions: ref<TransactionSystem> = GameInstance.GetTransactionSystem(player.GetGame());
  if !everything {
    BlockRenderSwitchOffEntity(transactions.GetItemInSlot(player, t"AttachmentSlots.WeaponRight"), off);
    BlockRenderSwitchOffEntity(transactions.GetItemInSlot(player, t"AttachmentSlots.WeaponLeft"), off);
    BlockRenderSwitchOffEntity(transactions.GetItemInSlot(player, t"AttachmentSlots.RightArm"), off);
    return off;
  };
  let items: array<wref<gameItemData>>;
  transactions.GetItemList(player, items);
  let i: Int32 = 0;
  while i < ArraySize(items) {
    if IsDefined(items[i]) {
      let item: ref<ItemObject> = transactions.GetItemInSlotByItemID(player, items[i].GetID());
      if IsDefined(item) {
        BlockRenderSwitchOffEntity(item, off);
      };
    };
    i += 1;
  };
  return off;
}

@if(ModuleExists("Codeware"))
public func BlockRenderSwitchOffEntity(entity: ref<Entity>, out off: array<wref<IComponent>>) -> Void {
  if !IsDefined(entity) {
    return;
  };
  let components: array<ref<IComponent>> = entity.GetComponents();
  let i: Int32 = 0;
  while i < ArraySize(components) {
    let component: ref<IComponent> = components[i];
    if IsDefined(component as IVisualComponent) && component.IsEnabled() {
      component.Toggle(false);
      ArrayPush(off, component);
    };
    i += 1;
  };
}

@if(!ModuleExists("Codeware"))
public func BlockRenderSwitchOffMeshes(player: ref<GameObject>, everything: Bool) -> array<wref<IComponent>> {
  let off: array<wref<IComponent>>;
  return off;
}
```
```swift
// Our hits: a hit with no damage of its own from the attack record is marked DealNoDamage under n"no_valid_damage" before
// Process, which would then skip DealDamages; that mark comes off. Any other reason stays: friendly fire, god mode, a dead
// target. Any thread.
@wrapMethod(DamageSystem)
private final func Process(hitEvent: ref<gameHitEvent>, cache: ref<CacheData>) -> Void {
  if BlockRenderNpcs.AmountOf(hitEvent.attackData) >= 0.00 {
    hitEvent.attackData.RemoveFlag(hitFlag.DealNoDamage, n"no_valid_damage");
  };
  wrappedMethod(hitEvent, cache);
}

// The last step before the damage is applied, after every modifier. Our hit deals exactly its amount, as physical
// damage, and BlockRender hears what it took off. A hit on V that BlockRender takes for Minecraft deals nothing. Any
// thread.
@wrapMethod(DamageSystem)
private final func DealDamages(hitEvent: ref<gameHitEvent>) -> Void {
  let target: ref<GameObject> = hitEvent.target;
  let amount: Float = BlockRenderNpcs.AmountOf(hitEvent.attackData);
  if amount >= 0.00 && IsDefined(target) && IsDefined(hitEvent.attackComputed) {
    let stats: StatsObjectID = Cast<StatsObjectID>(target.GetEntityID());
    let pools: ref<StatPoolsSystem> = GameInstance.GetStatPoolsSystem(target.GetGame());
    let before: Float = pools.GetStatPoolValue(stats, gamedataStatPoolType.Health, false);
    BlockRenderNpcs.SetDamage(hitEvent.attackComputed, amount);
    wrappedMethod(hitEvent);
    BlockRenderHitLanded(target.GetEntityID(), amount, before, pools.GetStatPoolValue(stats, gamedataStatPoolType.Health, false), 0);
```
```swift
// Our hits start the whole pipeline where Hit marked them, Minecraft's damage x V's punch, split across damage types as
// the attack splits its own; the pipeline then does to them all it does to V's own punches, from the spread and V's crits,
// perks and cyberware to the NPC's armour, resistances, blocks and dodges. One it dealt nothing says why. Any thread.
@wrapMethod(DamageSystem)
private final func ProcessPipeline(hitEvent: ref<gameHitEvent>, cache: ref<CacheData>) -> Void {
  let amount: Float = BlockRenderNpcs.AmountOf(hitEvent.attackData);
  if amount < 0.00 || !IsDefined(hitEvent.attackComputed) {
    wrappedMethod(hitEvent, cache);
    return;
  };
  BlockRenderNpcs.StartAt(hitEvent.attackComputed, amount);
  wrappedMethod(hitEvent, cache);
  if hitEvent.attackData.HasFlag(hitFlag.DealNoDamage) && IsDefined(hitEvent.target) {
    BlockRenderHitEnded(hitEvent.target.GetEntityID(), BlockRenderNpcs.WhyNoDamage(hitEvent.attackData));
  };
}

// The last step before the damage is applied, after every modifier. Our hit is reported with what the pipeline made of
// it, what it took off the NPC's health, and what the pipeline did to it. A hit on V that BlockRender takes for Minecraft
// deals V nothing but what V's overshield takes; one the game won't let kill V stays V's. Any thread.
@wrapMethod(DamageSystem)
private final func DealDamages(hitEvent: ref<gameHitEvent>) -> Void {
  let target: ref<GameObject> = hitEvent.target;
  if IsDefined(target) && IsDefined(hitEvent.attackComputed) && BlockRenderNpcs.AmountOf(hitEvent.attackData) >= 0.00 {
    let stats: StatsObjectID = Cast<StatsObjectID>(target.GetEntityID());
    let pools: ref<StatPoolsSystem> = GameInstance.GetStatPoolsSystem(target.GetGame());
    let before: Float = pools.GetStatPoolValue(stats, gamedataStatPoolType.Health, false);
    let amount: Float = hitEvent.attackComputed.GetTotalAttackValue(gamedataStatPoolType.Health);
    wrappedMethod(hitEvent);
    BlockRenderHitLanded(target.GetEntityID(), amount, before, pools.GetStatPoolValue(stats, gamedataStatPoolType.Health, false),
      BlockRenderNpcs.LandedFlags(hitEvent.attackData));
```
```swift
    if total > 0.00 {
```
```swift
    let pools: ref<StatPoolsSystem> = GameInstance.GetStatPoolsSystem(target.GetGame());
    let shield: Float = MinF(total, MaxF(pools.GetStatPoolValue(Cast<StatsObjectID>(target.GetEntityID()), gamedataStatPoolType.Overshield, false), 0.00));
    if total > shield {
```
```swift
      if BlockRenderVHurt(total, EnumInt(hitEvent.attackData.GetAttackType()), from, false) {
        BlockRenderNpcs.SetDamage(hitEvent.attackComputed, 0.00);
```
```swift
      let nonLethal: Bool = hitEvent.attackData.HasFlag(hitFlag.CannotKillPlayer) || hitEvent.attackData.GetMinimumHealthPercent() > 0.00;
      if BlockRenderVHurt(total - shield, EnumInt(hitEvent.attackData.GetAttackType()), from, nonLethal) {
        BlockRenderNpcs.SetDamage(hitEvent.attackComputed, shield);
```
- [x] **Step 2: Compile the module against the game's scripts, without and with Codeware**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && uv run --no-project python tools/check_reds.py E:/Cyberpunk2077 scripts | tail -1; \
  echo "without Codeware: exit ${PIPESTATUS[0]}"; \
  uv run --no-project python tools/check_reds.py E:/Cyberpunk2077 scripts E:/Cyberpunk2077/red4ext/plugins/Codeware/Scripts | tail -1; \
  echo "with Codeware: exit ${PIPESTATUS[0]}"
```
Expected: `compiled`, `without Codeware: exit 0` and `with Codeware: exit 0`.

- [x] **Step 3: Check that the compile catches a slip: a wrap of a method that isn't there**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/reds-slip \
  && sed 's/private final func ProcessPipeline(hitEvent/private final func ProcessPipelineX(hitEvent/' scripts/BlockRender.reds > build/reds-slip/BlockRender.reds \
  && uv run --no-project python tools/check_reds.py E:/Cyberpunk2077 build/reds-slip; echo "exit: $?"
```
Expected: `[UNRESOLVED_METHOD]` at the wrap's line, then `failed` and `exit: 1`.

- [x] **Step 4: Check that the compile catches a slip in the parts for Codeware, only with Codeware**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/reds-slip \
  && sed 's/= entity.GetComponents();/= entity.GetComponentsX();/' scripts/BlockRender.reds > build/reds-slip/BlockRender.reds \
  && uv run --no-project python tools/check_reds.py E:/Cyberpunk2077 build/reds-slip | grep -E "^compiled|^failed|UNRESOLVED"; \
  echo "without Codeware: exit ${PIPESTATUS[0]}"; \
  uv run --no-project python tools/check_reds.py E:/Cyberpunk2077 build/reds-slip E:/Cyberpunk2077/red4ext/plugins/Codeware/Scripts \
    | grep -E "^compiled|^failed|UNRESOLVED"; echo "with Codeware: exit ${PIPESTATUS[0]}"
```
Expected: `compiled` and `without Codeware: exit 0`; then redscript's `[UNRESOLVED_METHOD]` at the slip's line, `failed`
and `with Codeware: exit 1`.

- [x] **Step 5: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: the module starts our hits at the pipeline's entry and lets it do the rest, punches with V's unarmed slot, draws it and hides V for build mode, and leaves V's non-lethal hurts and overshield to V" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 27: `GameScript` calls the module's `Arm` and `HideV`, and reads whether V's unarmed weapon is drawn

Both are found by their types once, with the others: `Arm(Bool) -> Int32` and `HideV(Bool) -> Int32`, through one
shared call. `List`'s fourteenth field, `punchDrawn` (`Bool`), says whether V has its unarmed weapon drawn. The module
must now be version 4: 10 functions, 14 fields, and seven natives.

**Files:**
- Modify: `src/GameScript.h`, `src/GameScript.cpp`

- [x] **Step 1: Edit `src/GameScript.h`**

```cpp
constexpr int32_t kModuleVersion = 3;  // BlockRenderNpcs.Version(), as this BlockRender needs it
```
```cpp
constexpr int32_t kModuleVersion = 4;  // BlockRenderNpcs.Version(), as this BlockRender needs it
```
```cpp
	float                        punch = 0.0f;  // V's punch, read from punchItem
	std::string                  punchItem;
```
```cpp
	float                        punch = 0.0f;  // V's punch, read from punchItem, what V's unarmed slot draws
	std::string                  punchItem;
	bool                         punchDrawn = false;  // V has it drawn, so hits carry it as their weapon
```
```cpp
	// Combat (spike 6's "Part 2"). One of Minecraft's hits on the NPC aEntity, as V's: aAmount off its health, the strong
	// attack if aStrong. The module's Hit says what became of it (link::HitResultName). Nothing, with aWhy saying why, if
	// the module can't be called.
	std::optional<int32_t> Hit(uint64_t aEntity, double aAmount, bool aStrong, std::string& aWhy);
```
```cpp
	// Combat (spike 6's "Part 2"). One of Minecraft's hits on the NPC aEntity, as V's: aAmount where the game's damage
	// pipeline starts it, the strong attack if aStrong. The module's Hit says what became of it (link::HitResultName).
	// Nothing, with aWhy saying why, if the module can't be called.
	std::optional<int32_t> Hit(uint64_t aEntity, double aAmount, bool aStrong, std::string& aWhy);
	// V's unarmed weapon drawn (aOn), or V's own weapon put back: the module's Arm says how it went (1 asked, 2 nothing to
	// ask, 0 V isn't found, -1 no unarmed weapon, -2 no equipment system).
	std::optional<int32_t> Arm(bool aOn, std::string& aWhy);
	// V's meshes hidden (aOn), or shown again: the module's HideV gives how many (-1 without Codeware, -2 V isn't found).
	std::optional<int32_t> HideV(bool aOn, std::string& aWhy);
```
```cpp
		kPunchReport,
```
```cpp
		kPunchReport,
		kArm,
		kHideV,
```
```cpp
		kPunchItem,
		kVHealthMax,
		kFields,
	};
```
```cpp
		kPunchItem,
		kPunchDrawn,
		kVHealthMax,
		kFields,
	};

	// A member function of the module's taking a Bool and giving an Int32: Arm and HideV.
	std::optional<int32_t> Switch(Function aFunction, const char* aName, bool aOn, std::string& aWhy);
```
- [x] **Step 2: Edit `src/GameScript.cpp`**

```cpp
		{ "Punch", nullptr, {}, { "String" } },
```
```cpp
		{ "Punch", nullptr, {}, { "String" } },
		{ "Arm", nullptr, { { "Bool" } }, { "Int32" } },
		{ "HideV", nullptr, { { "Bool" } }, { "Int32" } },
```
```cpp
		{ "vHealthMax", "Float" },
```
```cpp
		{ "punchDrawn", "Bool" },          { "vHealthMax", "Float" },
```
```cpp
	list.punchItem = FieldOf<RED4ext::CString>(*fields_[kPunchItem], instance).c_str();
```
```cpp
	list.punchItem = FieldOf<RED4ext::CString>(*fields_[kPunchItem], instance).c_str();
	list.punchDrawn = FieldOf<bool>(*fields_[kPunchDrawn], instance);
```
```cpp
	return result.c_str();
}
}
```
```cpp
	return result.c_str();
}

std::optional<int32_t> GameScript::Arm(bool aOn, std::string& aWhy)
{
	return Switch(kArm, "Arm", aOn, aWhy);
}

std::optional<int32_t> GameScript::HideV(bool aOn, std::string& aWhy)
{
	return Switch(kHideV, "HideV", aOn, aWhy);
}

std::optional<int32_t> GameScript::Switch(Function aFunction, const char* aName, bool aOn, std::string& aWhy)
{
	if (const std::string& missing = Check(); !missing.empty())
	{
		aWhy = missing;
		return std::nullopt;
	}
	const Strong system = SessionSystem(*functions_[kCurrent], *class_, aWhy);
	if (!system)
	{
		return std::nullopt;
	}
	bool                 on = aOn;
	int32_t              result = 0;
	RED4ext::StackArgs_t args{ { nullptr, &on } };
	if (!RED4ext::ExecuteFunction(system.GetPtr(), functions_[aFunction], &result, args))
	{
		aWhy = std::format("BlockRenderNpcs.{} failed", aName);
		return std::nullopt;
	}
	return result;
}
}
```
- [x] **Step 3: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `438 test(s), 0 failure(s)`.

- [x] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: GameScript calls the module's Arm and HideV and reads whether V's unarmed weapon is drawn, from module version 4" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 28: The status says what V punches with and whether it's drawn, V's non-lethal hurts, and whether V is hidden

- **The combat line** names what V's punch is read from, and says whether V has it drawn, or what hits skip without
  it: `combat=on: V's punch 105.00 from Items.…, drawn; …`. Its hurts count those left to V as non-lethal. The last hit
  says where it started and what was dealt, with what the pipeline did to it (written by Task 29).
- **A new line, `v=`,** after the drive line: `hidden: 23 meshes switched off`; `shown, and hidden in build mode`; why
  it can't be hidden; or `shown: set hidev is 0`.

**Files:**
- Modify: `src/Status.h`, `src/Status.cpp`, `tests/TestStatus.cpp`

- [x] **Step 1: Edit `tests/TestStatus.cpp`: the combat line, and the v line**

```cpp
	CHECK(Has(cpmc::FormatStatus(report), "combat=on: V's punch 102.00; hits 0 queued, 0 landed; V's hurts: 0 taken, 0 sent, 0 summed, 0 "
	                                      "left to V; Immortal off; 0 deaths; Minecraft's player alive"));
	link.hitsQueued = 5;
	link.hitsLanded = 4;
	link.hitsDropped = "1 not listed";
	link.lastHit = "NPC 35 (NC Resident): 7.00 of Minecraft's damage, 714.00 of its health; landed, 1514 -> 800";
```
```cpp
	CHECK(Has(cpmc::FormatStatus(report), "combat=on: V's punch 102.00, not drawn: hits skip the NPC's armour, weapon crits, stealth, "
	                                      "blocks and skill XP; hits 0 queued, 0 landed; V's hurts: 0 taken, 0 sent, 0 summed, 0 left to V, "
	                                      "0 non-lethal left to V; Immortal off; 0 deaths; Minecraft's player alive"));
	link.punch = 105.0;
	link.punchItem = "Items.mantis_blade";
	link.punchDrawn = true;
	link.hitsQueued = 5;
	link.hitsLanded = 3;
	link.hitsDropped = "1 dodged, 1 not listed";
	link.lastHit = "NPC 35 (NC Resident): 7.00 of Minecraft's damage, started at 735.00, 612.40 dealt (critical, armour); 1514 -> 902";
```
```cpp
	link.hurtsLeft = 7;
```
```cpp
	link.hurtsLeft = 7;
	link.hurtsNonLethal = 2;
```
```cpp
	CHECK(Has(cpmc::FormatStatus(report), "combat=on: V's punch 102.00; hits 5 queued, 4 landed, dropped: 1 not listed; the last NPC 35 "
	                                      "(NC Resident): 7.00 of Minecraft's damage, 714.00 of its health; landed, 1514 -> 800; V's "
	                                      "hurts: 3 taken, 2 sent, 1 summed, 7 left to V; the last 45.0 of V's health (0.0987), 1.97 of "
	                                      "Minecraft's 20, projectile (Ranged), from NPC 12; Immortal on; 1 deaths; Minecraft's player "
	                                      "dead for 0.8 s; V's max health isn't read, so V's hurts aren't taken"));
}
```
```cpp
	CHECK(Has(cpmc::FormatStatus(report), "combat=on: V's punch 105.00 from Items.mantis_blade, drawn; hits 5 queued, 3 landed, dropped: 1 "
	                                      "dodged, 1 not listed; the last NPC 35 (NC Resident): 7.00 of Minecraft's damage, started at "
	                                      "735.00, 612.40 dealt (critical, armour); 1514 -> 902; V's hurts: 3 taken, 2 sent, 1 summed, 7 "
	                                      "left to V, 2 non-lethal left to V; the last 45.0 of V's health (0.0987), 1.97 of Minecraft's "
	                                      "20, projectile (Ranged), from NPC 12; Immortal on; 1 deaths; Minecraft's player dead for 0.8 "
	                                      "s; V's max health isn't read, so V's hurts aren't taken"));
}

TEST(StatusWritesWhetherVIsHidden)
{
	cpmc::StatusReport report;
	cpmc::LinkReport   link;
	report.link = link;
	std::string status = cpmc::FormatStatus(report);
	CHECK(Has(status, "v=shown, and hidden in build mode"));
	CHECK(status.find("drive=") < status.find("v=") && status.find("v=") < status.find("positions="));
	link.vHidden = true;
	link.vMeshes = 23;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "v=hidden: 23 meshes switched off"));
	link.vMeshes = -1;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "v=shown: hiding V's meshes needs Codeware, which isn't installed"));
	link.vMeshes = -2;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "v=shown: V isn't found"));
	link.hideV = false;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "v=shown: set hidev is 0"));
}
```
- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'punchItem': is not a member of 'cpmc::LinkReport'`.

- [x] **Step 3: Edit `src/Status.h`**

```cpp
	uint32_t              minecraftFlags = 0;        // McState's
```
```cpp
	uint32_t              minecraftFlags = 0;        // McState's
	// V's meshes in build mode (spike 6's "Part 2")
	bool    hideV = true;     // set hidev 1: hidden while build mode is on
	bool    vHidden = false;  // they are now
	int32_t vMeshes = 0;      // as the module's HideV last said: the meshes it switched off; -1 no Codeware; -2 V isn't found
```
```cpp
	double                punch = 0.0;       // V's punch, as last read
	uint64_t              hitsQueued = 0;    // Minecraft's hits made V's, and queued in the game
	uint64_t              hitsLanded = 0;    // and dealt by its damage pipeline
	std::string           hitsDropped;       // by reason: "2 not listed, 1 the NPC is dead"; empty: none
	std::string           lastHit;           // the NPC, Minecraft's damage, the amount, and how it went
```
```cpp
	double                punch = 0.0;       // V's punch, as last read
	std::string           punchItem;         // what it's read from: what V's unarmed slot draws
	bool                  punchDrawn = false;  // V has it drawn, so hits carry it as their weapon
	uint64_t              hitsQueued = 0;    // Minecraft's hits made V's, and queued in the game
	uint64_t              hitsLanded = 0;    // and dealt by its damage pipeline
	std::string           hitsDropped;       // by reason, ours or the pipeline's: "2 not listed, 1 dodged"; empty: none
	std::string           lastHit;           // the NPC, Minecraft's damage, where it started, and how it went
```
```cpp
	uint64_t              hurtsLeft = 0;     // offered while not taken: left to V
```
```cpp
	uint64_t              hurtsLeft = 0;     // offered while not taken: left to V
	uint64_t              hurtsNonLethal = 0;  // offered while taken, but the game won't let them kill V: left to V
```
- [x] **Step 4: Edit `src/Status.cpp`**

```cpp
// The ground's worker: how long a region took in the last second, of how many triangles, and how much of that time its
```
```cpp
// V's meshes in build mode (spike 6's "Part 2"): hidden while build mode is on, unless set hidev is 0.
std::string FormatV(const LinkReport& aLink)
{
	if (!aLink.hideV)
	{
		return "v=shown: set hidev is 0\n";
	}
	if (!aLink.vHidden)
	{
		return "v=shown, and hidden in build mode\n";
	}
	switch (aLink.vMeshes)
	{
	case -1:
		return "v=shown: hiding V's meshes needs Codeware, which isn't installed\n";
	case -2:
		return "v=shown: V isn't found\n";
	default:
		return std::format("v=hidden: {} meshes switched off\n", aLink.vMeshes);
	}
}

// The ground's worker: how long a region took in the last second, of how many triangles, and how much of that time its
```
```cpp
	std::string out = std::format("combat=on: V's punch {:.2f}; hits {} queued, {} landed{}", aLink.punch, aLink.hitsQueued, aLink.hitsLanded,
	                              aLink.hitsDropped.empty() ? "" : ", dropped: " + OneLine(aLink.hitsDropped));
```
```cpp
	std::string out = std::format("combat=on: V's punch {:.2f}{}{}; hits {} queued, {} landed{}", aLink.punch,
	                              aLink.punchItem.empty() ? "" : " from " + OneLine(aLink.punchItem),
	                              aLink.punchDrawn ? ", drawn" : ", not drawn: hits skip the NPC's armour, weapon crits, stealth, blocks and skill XP",
	                              aLink.hitsQueued, aLink.hitsLanded, aLink.hitsDropped.empty() ? "" : ", dropped: " + OneLine(aLink.hitsDropped));
```
```cpp
	out += std::format("; V's hurts: {} taken, {} sent, {} summed, {} left to V", aLink.hurtsTaken, aLink.hurtsSent, aLink.hurtsSummed,
	                   aLink.hurtsLeft);
```
```cpp
	out += std::format("; V's hurts: {} taken, {} sent, {} summed, {} left to V, {} non-lethal left to V", aLink.hurtsTaken, aLink.hurtsSent,
	                   aLink.hurtsSummed, aLink.hurtsLeft, aLink.hurtsNonLethal);
```
```cpp
	out += FormatDrive(aLink);
```
```cpp
	out += FormatDrive(aLink);
	out += FormatV(aLink);
```
- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `439 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: the status says what V punches with and whether it's drawn, V's non-lethal hurts, and whether V is hidden" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 29: BlockRender 0.18.0 reports what the pipeline made of our hits, draws V's unarmed weapon for combat in build mode, and hides V there

In the plugin, still 0.18.0, since 0.18.0 was never deployed:
- **`Fight`** takes the inbox's landings, with where each hit started and what the pipeline dealt and did to it, and
  its endings, counted with the hits not applied by their reason (`1 dodged`). A hit the game neither dealt nor said
  anything about within 2 s is still counted as not dealt.
- **`SwitchV`,** in `Fight` after V's god mode: V's unarmed weapon drawn while combat and build mode are on, asked again
  each second until it's drawn (`VSwitch` with a 1 s refresh); V's meshes hidden while build mode is on and `set hidev`
  is 1, hidden again every quarter second, so the weapon, once drawn, goes too. Each logged when what the module says
  changes, the hiding with the time its call took: the first looks through V's whole inventory.
- **`set hidev 0|1`,** on by default; `npcs` says whether V's unarmed weapon is drawn; the status gets what V punches
  with, whether it's drawn, V's non-lethal hurts, and the v line.

**Files:**
- Modify: `src/Plugin.cpp`

- [x] **Step 1: Edit `src/Plugin.cpp`**

```cpp
// Minecraft's hits land on them as V's, and in build mode the game's hits on V go to Minecraft's player, whose death
// kills V.
```
```cpp
// Minecraft's hits land on them as V's, through the game's whole damage pipeline, and in build mode the game's hits on V
// go to Minecraft's player, whose death kills V. In build mode V's meshes are hidden, and with combat on V's unarmed
// weapon is drawn, so that hits carry it.
```
```cpp
double                                g_vHealthMax = 0.0;
```
```cpp
double                                g_vHealthMax = 0.0;
std::string                           g_punchItem;           // what V's unarmed slot draws, which the punch is read from
bool                                  g_punchDrawn = false;  // V has it drawn, so hits carry it as their weapon
cpmc::link::VSwitch                   g_armed{ 1.0 };        // V's unarmed weapon, drawn while combat and build mode are on
cpmc::link::VSwitch                   g_hidden{ 0.25 };      // V's meshes, hidden while build mode is on and hidev is 1
bool                                  g_hideV = true;        // set hidev 0|1
std::optional<int32_t>                g_lastArm;             // what Arm and HideV last said, so that a refresh logs a change
std::optional<int32_t>                g_lastHide;
int32_t                               g_vMeshes = 0;         // the meshes HideV has switched off
```
```cpp
	double   amount = 0.0;
```
```cpp
	double   amount = 0.0;  // where the pipeline starts it
```
```cpp
	g_punch = g_npcList->punch;
```
```cpp
	g_punch = g_npcList->punch;
	g_punchItem = g_npcList->punchItem;
	g_punchDrawn = g_npcList->punchDrawn;
```
```cpp
// the module: Minecraft's damage x V's punch off the NPC's health.
```
```cpp
// the module, starting the game's damage pipeline at Minecraft's damage x V's punch.
```
```cpp
	g_lastHit = std::format("NPC {} ({}): {:.2f} of Minecraft's damage, {:.2f} of its health{}; queued", accepted->formId,
	                        NpcName(accepted->formId), accepted->damage, accepted->amount, accepted->strong ? ", strong" : "");
	Log("{}: {:.2f} of its health, {:.2f} x V's punch {:.2f}{}; queued", hit, accepted->amount, accepted->damage, g_punch,
	    accepted->strong ? ", strong" : "");
```
```cpp
	g_lastHit = std::format("NPC {} ({}): {:.2f} of Minecraft's damage, starting at {:.2f}{}; queued", accepted->formId,
	                        NpcName(accepted->formId), accepted->damage, accepted->amount, accepted->strong ? ", strong" : "");
	Log("{}: starting at {:.2f}, {:.2f} x V's punch {:.2f}{}{}; queued", hit, accepted->amount, accepted->damage, g_punch,
	    accepted->strong ? ", strong" : "", g_punchDrawn ? "" : ", without V's weapon drawn");
```
```cpp
// Spike 6's combat, after the link's update and before the NPCs are listed (its spec's "The frame"): Minecraft's hits and
// deaths in the ring's order; our hits the game has dealt, and those it hasn't within kHitTrail; V's god mode; and
// whether V's hurts are taken.
```
```cpp
// What the module's Arm said, in words.
std::string ArmWords(int32_t aResult, bool aOn)
{
	switch (aResult)
	{
	case 1:
		return aOn ? "asked the game to draw V's unarmed weapon" : "asked the game for V's own weapon back";
	case 2:
		return aOn ? "V has it drawn" : "nothing to put back";
	case 0:
		return "V isn't found";
	case -1:
		return "V has no unarmed weapon";
	case -2:
		return "there's no equipment system";
	default:
		return std::format("result {}", aResult);
	}
}

// What the module's HideV said, in words.
std::string HideWords(int32_t aResult, bool aOn)
{
	switch (aResult)
	{
	case -1:
		return "not done: hiding V's meshes needs Codeware, which isn't installed";
	case -2:
		return "not done: V isn't found";
	default:
		return std::format("{} meshes {}", aResult, aOn ? "switched off" : "switched on again");
	}
}

// V's unarmed weapon, drawn while combat and build mode are on (spike 6's "V's weapon in build mode"), and V's meshes,
// hidden while build mode is on (its "V unseen in build mode"): each through the module, logged when what it says changes.
void SwitchV(bool aBuild, bool aVFound, double aNow)
{
	if (const std::optional<bool> arm = g_armed.Frame(g_combat && aBuild, aVFound, aNow))
	{
		std::string                  why;
		const std::optional<int32_t> result = g_script.Arm(*arm, why);
		if (!*arm || result != g_lastArm)
		{
			Log("combat: V's unarmed weapon {}: {}", *arm ? "drawn" : "put away", result ? ArmWords(*result, *arm) : "not done: " + why);
		}
		g_lastArm = *arm ? result : std::nullopt;
	}
	if (const std::optional<bool> hide = g_hidden.Frame(g_hideV && aBuild, aVFound, aNow))
	{
		std::string                  why;
		const double                 start = cpmc::SteadySeconds();
		const std::optional<int32_t> result = g_script.HideV(*hide, why);
		const double                 ms = (cpmc::SteadySeconds() - start) * 1000.0;
		if (!*hide || result != g_lastHide)
		{
			Log("build: V's meshes {}: {} ({:.2f} ms)", *hide ? "hidden" : "shown", result ? HideWords(*result, *hide) : "not done: " + why,
			    ms);
		}
		g_lastHide = *hide ? result : std::nullopt;
		g_vMeshes = *hide ? result.value_or(0) : 0;
	}
	else if (!g_hidden.On())
	{
		g_vMeshes = 0;
	}
}

// Spike 6's combat, after the link's update and before the NPCs are listed (its spec's "The frame"): Minecraft's hits and
// deaths in the ring's order; our hits the game has dealt, those it dealt nothing and why, and those it hasn't said
// within kHitTrail; V's god mode, V's unarmed weapon and V's meshes; and whether V's hurts are taken.
```
```cpp
		const double   damage = pending != g_pendingHits.end() ? pending->damage : 0.0;
```
```cpp
		const double   damage = pending != g_pendingHits.end() ? pending->damage : 0.0;
		const double   start = pending != g_pendingHits.end() ? pending->amount : 0.0;
```
```cpp
		g_lastHit = std::format("NPC {} ({}): {:.2f} of Minecraft's damage, {:.2f} of its health; landed, {:.0f} -> {:.0f}", formId,
		                        NpcName(formId), damage, landed.amount, landed.before, landed.after);
		Log("hit on NPC {} landed: {:.2f} of its health, {:.0f} -> {:.0f}", formId, landed.amount, landed.before, landed.after);
```
```cpp
		const std::string how = cpmc::link::LandedWords(landed.flags);
		g_lastHit = std::format("NPC {} ({}): {:.2f} of Minecraft's damage, started at {:.2f}, {:.2f} dealt{}; {:.0f} -> {:.0f}", formId,
		                        NpcName(formId), damage, start, landed.amount, how.empty() ? "" : " (" + how + ")", landed.before,
		                        landed.after);
		Log("hit on NPC {} landed: started at {:.2f}, {:.2f} dealt{}; its health {:.0f} -> {:.0f}", formId, start, landed.amount,
		    how.empty() ? "" : " (" + how + ")", landed.before, landed.after);
	}
	for (const cpmc::CombatInbox::Ended& ended : cpmc::CombatReported().TakeEnded())
	{
		const auto pending = std::find_if(g_pendingHits.begin(), g_pendingHits.end(),
		                                  [&](const PendingHit& aHit) { return aHit.entity == ended.target; });
		const uint32_t formId = pending != g_pendingHits.end() ? pending->formId : g_actorIds.IdOf(ended.target).value_or(0);
		if (pending != g_pendingHits.end())
		{
			g_pendingHits.erase(pending);
		}
		const std::string why = cpmc::link::HitEndedName(ended.why);
		++g_hitDrops[why];
		Log("hit on NPC {} dealt nothing: {}", formId, why);
```
```cpp
		Log("hit on NPC {} ({:.2f} of its health) wasn't dealt by the game within {:.0f} s: friendly, invulnerable or dead by then",
		    aHit.formId, aHit.amount, cpmc::link::kHitTrail);
```
```cpp
		Log("hit on NPC {} (starting at {:.2f}) wasn't dealt by the game within {:.0f} s, nor said why", aHit.formId, aHit.amount,
		    cpmc::link::kHitTrail);
```
```cpp
	}
	cpmc::CombatReported().SetTaking(g_combat && build && vFound && g_vHealthMax > 0.0);
```
```cpp
	}
	SwitchV(build, vFound, aNow);
	cpmc::CombatReported().SetTaking(g_combat && build && vFound && g_vHealthMax > 0.0);
```
```cpp
		return Report(g_combat ? "set combat 1: Minecraft's hits land on NPCs as V's; in build mode V is Immortal, the game's hits on V go "
		                         "to Minecraft's player, and its death kills V"
		                       : "set combat 0: Minecraft's hits are only logged, and V's health is V's");
```
```cpp
		return Report(g_combat ? "set combat 1: Minecraft's hits land on NPCs as V's; in build mode V is Immortal with its unarmed weapon "
		                         "drawn, the game's hits on V go to Minecraft's player, and its death kills V"
		                       : "set combat 0: Minecraft's hits are only logged, and V's health is V's");
	}
	if (aRequest.what == "hidev")
	{
		if (aRequest.number != 0.0 && aRequest.number != 1.0)
		{
			return Report("set hidev: 1 hides V's meshes while build mode is on, so only Minecraft's view shows; 0 leaves V seen");
		}
		g_hideV = aRequest.number == 1.0;
		return Report(g_hideV ? "set hidev 1: V's meshes are hidden while build mode is on" : "set hidev 0: V stays seen in build mode");
```
```cpp
	Log("npcs: {} listed of {} in the world; V's punch {:.2f}, read from {}; V's max health {:.0f}", g_actorRecords.size(),
	    g_npcList->attached, g_npcList->punch, g_npcList->punchItem, g_npcList->vHealthMax);
```
```cpp
	Log("npcs: {} listed of {} in the world; V's punch {:.2f}, read from {}, {}; V's max health {:.0f}", g_actorRecords.size(),
	    g_npcList->attached, g_npcList->punch, g_npcList->punchItem, g_npcList->punchDrawn ? "drawn" : "not drawn", g_npcList->vHealthMax);
```
```cpp
	report.punch = g_punch;
```
```cpp
	report.punch = g_punch;
	report.punchItem = g_punchItem;
	report.punchDrawn = g_punchDrawn;
```
```cpp
	report.hurtsLeft = combat.refused;
```
```cpp
	report.hurtsLeft = combat.refused;
	report.hurtsNonLethal = combat.nonLethal;
```
```cpp
	report.health = g_linked ? g_puppet.Health() : std::nullopt;  // once a second, so fall damage shows
```
```cpp
	report.health = g_linked ? g_puppet.Health() : std::nullopt;  // once a second, so fall damage shows
	report.hideV = g_hideV;
	report.vHidden = g_hidden.On();
	report.vMeshes = g_vMeshes;
```
- [x] **Step 2: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `439 test(s), 0 failure(s)`.

- [x] **Step 3: Check that BlockRender imports nothing from PhysX**

```bash
DUMPBIN="$(ls "/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/"*/bin/Hostx64/x64/dumpbin.exe | head -1)"
cd /d/Cyberpunk-Minecraft/spikes/block-render && MSYS_NO_PATHCONV=1 "$DUMPBIN" -imports -nologo build\\RelWithDebInfo\\BlockRender.dll \
  | grep -ci "physx"
```
Expected: `0`.

- [x] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: BlockRender 0.18.0 reports what the pipeline made of our hits and why some dealt nothing, draws V's unarmed weapon for combat in build mode, and hides V there" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 30: The overlay shows whether V's unarmed weapon is drawn and whether V is hidden, and switches hidev

The combat line's summary says whether V's unarmed weapon is drawn; the build line says whether V is hidden; and under
"More controls", "V in build mode: Hidden" switches `set hidev`.

**Files:**
- Modify: `tools/blockrender_hud/init.lua`

- [x] **Step 1: Edit `tools/blockrender_hud/init.lua`**

```lua
-- combat), and a line each for the last request's reply, build mode (F8) and its drive, the bodies, the NPCs, combat,
-- and V. Two folded sections hold the rest: "More controls" (the drawing mode, Minecraft's HUD, commands typed into
-- Minecraft, the drive log, the ground and its shapes, the NPCs' list and V's punch) and "Full status" (every status
-- line, and the frame time).
```
```lua
-- combat), and a line each for the last request's reply, build mode (F8) with its drive and whether V is hidden, the
-- bodies, the NPCs, combat, and V. Two folded sections hold the rest: "More controls" (the drawing mode, Minecraft's HUD,
-- commands typed into Minecraft, the drive log, the ground and its shapes, the NPCs' list, V's punch, and V hidden or
-- seen in build mode) and "Full status" (every status line, and the frame time).
```
```lua
local combat = nil       -- the combat line: on or off; hits queued and landed; V's hurts; Immortal; Minecraft's death
local combatOn = nil     -- whether Minecraft's hits land as V's and V's hurts go to Minecraft: "on" or "off"
```
```lua
local combat = nil       -- the combat line: on or off; V's weapon drawn or not; hits queued and landed; V's hurts;
                         -- Immortal; Minecraft's death
local combatOn = nil     -- whether Minecraft's hits land as V's and V's hurts go to Minecraft: "on" or "off"
local vLook = nil        -- the v line: V hidden in build mode, or seen and why
local hideV = nil        -- whether V's meshes are hidden in build mode: "on" or "off"
```
```lua
    combatOn = nil
```
```lua
    combatOn = nil
    vLook = nil
    hideV = nil
```
```lua
            turn = line:match("; turn (%a+),")
```
```lua
            turn = line:match("; turn (%a+),")
        end
        local look = line:match("^v=(.*)$")
        if look ~= nil then
            vLook = look
            hideV = look:match("^shown: set hidev is 0") and "off" or "on"
```
```lua
            combatOn = combatState
```
```lua
            combatOn = combatState
            local drawn = line:match("^combat=on: V's punch [^;]*, (not drawn)") or line:match("^combat=on: V's punch [^;]*, (drawn)")
```
```lua
            combat = combatState .. (hits and (", hits " .. hits) or "") .. (hurts and (", V's hurts " .. hurts) or "")
                .. (immortal and (", Immortal " .. immortal) or "") .. (dead and (", Minecraft's player " .. dead) or "")
```
```lua
            combat = combatState .. (drawn and (", V's weapon " .. drawn) or "") .. (hits and (", hits " .. hits) or "")
                .. (hurts and (", V's hurts " .. hurts) or "") .. (immortal and (", Immortal " .. immortal) or "")
                .. (dead and (", Minecraft's player " .. dead) or "")
```
```lua
            ImGui.Text("Build mode (F8): " .. build .. (driveState and ("; drive " .. driveState) or ""))
```
```lua
            ImGui.Text("Build mode (F8): " .. build .. (driveState and ("; drive " .. driveState) or "")
                .. (vLook and ("; V " .. vLook) or ""))
```
```lua
                    request("punch")
                end
            end
        end
```
```lua
                    request("punch")
                end
            end
            if hideV ~= nil then
                ImGui.Text("V in build mode:")
                ImGui.SameLine()
                if ImGui.Button((hideV == "on" and "Hidden" or "Seen") .. "###hidev") then
                    request("set hidev " .. (hideV == "on" and 0 or 1))
                end
            end
        end
```
- [x] **Step 2: Check the overlay's syntax**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && luac -p tools/blockrender_hud/init.lua && echo ok
```
Expected: `ok`.

- [x] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: the overlay shows whether V's unarmed weapon is drawn and whether V is hidden, and switches hidev" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 31: In the game: step 4

Step 4 of the spec's "The steps", with the user, on BlockRender 0.18.0 with `set combat 1`, linked to SkyCraft's
Minecraft in the `CyberCraft` instance, its player in survival. The opening checks come first, with the least code, and
settle what the review left to the game:
- **what V punches with,** as the module reads it, against the game's own unarmed slot (V has Mantis Blades installed,
  which step 2 saw at 105, so it should be them, not the fists' 102);
- **build mode draws V's unarmed weapon and hides V,** and leaving it puts V's own weapon back and shows V again;
- **one hit on an NPC lands as V's own does:** `hit N 1` starts at 1 × V's punch, and what the pipeline deals should be
  in the same range as V's own strike with the same weapon on the same NPC; whether the NPC's health shows the change at
  once;
- **one hit on V in build mode.**

Then fights, being shot at in build mode, and dying in Minecraft.

FreeFly (F7) stays off. Note what you see as you go: Step 15 records it. If the game freezes, read the logs and the
newest folder in `%LOCALAPPDATA%\REDEngine\ReportQueue`, and force-close it only when the user asks. If a check fails,
stop and find the cause with superpowers:systematic-debugging before going on. Fixes become new tasks.

From Step 7 on, the hits V takes in build mode go to Minecraft's player, in survival: if its hearts run out, V dies
too, and the game wants a load. Watch its hearts; if it dies early, that's Step 11's check, done early: note it, load the
save on the pavement, link, and go on.

These commands come back, as in step 2:
- **Send a request,** atomically: the plugin runs it within a second. Replace `punch` with the command, and send the next
  only once the status's `request=` line shows the last.
  ```bash
  P=/e/Cyberpunk2077/red4ext/plugins/BlockRender; printf '%s\n' "punch" > "$P/request.tmp" && mv "$P/request.tmp" "$P/blockrender.request"
  ```
- **The log's newest lines:**
  ```bash
  L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); tail -30 "/e/Cyberpunk2077/red4ext/logs/$L"
  ```
- **The status:**
  ```bash
  cat /e/Cyberpunk2077/bin/x64/plugins/cyber_engine_tweaks/mods/blockrender_hud/status.txt
  ```
- **Minecraft's log, for SkyCraft's lines and the chat's replies:**
  ```bash
  grep -E "SkyCraft: (Skyrim hit|hit|[0-9]+ Skyrim)|\[CHAT\]" "/c/Users/BLOXY/AppData/Roaming/ModrinthApp/profiles/CyberCraft/logs/latest.log" | tail -10
  ```

**Files:**
- Modify: `docs/spikes/2026-10-05-npcs-and-combat-results.md`

- [x] **Step 1: Deploy (the game must not be running)**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game first, because a running game locks the DLL.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
P=/e/Cyberpunk2077/red4ext/plugins/BlockRender
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default -DCP2077_DIR=E:/Cyberpunk2077 > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C|error MSB"; \
  "$CMAKE" --preset default -DCP2077_DIR= > /dev/null; \
  cmp build/RelWithDebInfo/BlockRender.dll "$P/BlockRender.dll" && cmp scripts/BlockRender.reds "$P/Scripts/BlockRender.reds" \
  && echo installed
```
Expected: `installed`. The second configure clears `CP2077_DIR` again.

- [x] **Step 2: In the game: the module and its natives**

Ask the user to start the `CyberCraft` instance, then the game, and to wait at the game's main menu. If a "REDScript
compilation has failed" message box shows instead, ask the user to click OK and quit the game, and read redscript's log
for why. Otherwise:
```bash
grep -E "BlockRender|Codeware.Global|Compilation complete|ERROR" /e/Cyberpunk2077/r6/logs/redscript_rCURRENT.log; \
  L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); grep -E "loaded|natives:" "/e/Cyberpunk2077/red4ext/logs/$L"
```
Expected: the module's path and Codeware's among the files compiled, `Compilation complete`, and no `ERROR` line; in
BlockRender's log, `BlockRender 0.18.0 loaded` and `natives: BlockRenderNpcAttached, BlockRenderNpcDetached,
BlockRenderNpcCount, BlockRenderNpcAt, BlockRenderVHurt, BlockRenderHitLanded and BlockRenderHitEnded added`.

- [x] **Step 3: In the game: load, link, and combat on**

Ask the user to load the save on the busy pavement, with a gun drawn (weapon-wheel slot 1, 2 or 3), and click Link.
Then send `mc /gamemode survival`, and `set combat 1` (or ask the user to click the overlay's `Combat: off`). Expected:
- in the log, `module: found BlockRenderNpcs version 4, its 10 functions, the 14 fields List fills, and BlockRender's 7
  natives`, and `combat: V's Immortal off (the game says it already was)` once V is found;
- in the status, `combat=on: V's punch … from Items.…, not drawn: hits skip …; hits 0 queued, 0 landed; V's hurts: 0
  taken, 0 sent, 0 summed, … left to V, 0 non-lethal left to V; Immortal off; 0 deaths; Minecraft's player alive`, and
  `v=shown, and hidden in build mode`; in the overlay its `Combat:` line, `V's weapon not drawn`;
- send `npcs`: an NPC with a capsule now reaches about 1.75 m above its position, `its top another's: radius …`
  (Task 13).

- [x] **Step 4: In the game: the opening check, what V punches with**

Send `punch`. Expected: `punch: V's punch …, read from Items.…; quick hits with Attacks.… (Melee), strong ones with
Attacks.… (StrongMelee); not drawn, so hits carry no weapon, …` (V's numbers as V has them). V has Mantis Blades
installed, which the inventory showed at 105 in step 2: if the game files them as a weapon, they're V's unarmed slot,
and the item is theirs; otherwise it's `Items.w_melee_004__fists_a`, at 102. If either attack says `none`, `Hit` can't
make hits (`-3`): stop, and a new task finds the attack another way. Then ask the user to pick weapon-wheel slot 4 (V's
unarmed slot), and send `punch` again. Expected: the same item, now `drawn, so hits carry it as their weapon`, and the
status's combat line `…, drawn; …`: the module's item is the one the game's own slot drew. If it says `not drawn`, the
two disagree: stop, and find which item the slot drew. Ask the user to draw the gun again.

- [x] **Step 5: In the game: the opening check, build mode draws V's unarmed weapon and hides V**

Ask the user to press F8 on the pavement, with the gun drawn. Expected:
- in the log, `combat: V's Immortal on`, `combat: V's unarmed weapon drawn: asked the game to draw V's unarmed weapon`,
  and `build: V's meshes hidden: N meshes switched off (… ms)`, the time it took to look through V's inventory (note
  it: a few milliseconds at most); within a second, `build: V's meshes hidden: M meshes switched off (… ms)`, M more
  than N, once the drawn weapon's meshes are hidden too, and `combat: V's unarmed weapon drawn: V has it drawn` at the
  next second's ask;
- in the status, `combat=on: V's punch … drawn; …` and `v=hidden: M meshes switched off`;
- on screen, Minecraft's view with nothing of V's: no hands, no blades or gun; looking down, no legs or feet; and no
  shadow of V's. Ask the user to look around and down, and to say what they see of V, if anything.

Then ask the user to press F8 again. Expected: `combat: V's unarmed weapon put away: asked the game for V's own weapon
back`, `build: V's meshes shown: M meshes switched on again (… ms)`, and `combat: V's Immortal off`; V holds the gun again, and
V's hands and legs show as before. Then send `set hidev 0`, ask the user to press F8 and look down, and F8 again: V is
seen in build mode, its unarmed weapon drawn, and `v=shown: set hidev is 0`. Send `set hidev 1`.

- [x] **Step 6: In the game: the opening check, one hit on an NPC, as V's own**

Send `npcs`, and pick a listed NPC near V who isn't a vendor, by its number `N` (a ganger is best: civilians and vendors
may be friendly to V, and the game deals friendly hits nothing). Ask the user to pick weapon-wheel slot 4, walk up to
it, and, outside build mode, strike it once with V's own unarmed weapon, with the game's damage numbers on, and say the
number it shows. It's in combat with V from then on. Then send `hit N 1`. Expected, in the log:
- `hit on NPC N (…) for 1.00 of Minecraft's damage, knockback 0.40; unarmed: starting at P, 1.00 x V's punch P; queued`,
  P being V's punch;
- then either `hit on NPC N landed: started at P, X dealt (…); its health H -> H'`, with X in the same range as the
  number V's own strike showed (the game spreads hits by a tenth either way, and a crit is more: the flags say
  `critical`), and H' = H − X, or 0; or `hit on NPC N dealt nothing: …`, with why (`dodged`, `evaded`, `friendly to V`);
  if it's that, send `hit N 1` again;
- the status's combat line, `hits 1 queued, 1 landed` and the last hit as logged; and in the game, the NPC reacts.

If H' is H while X is over 0, the game applies the change after `DealDamages`: send `npcs` and read N's health there,
and note it for the results. If the log says `wasn't dealt by the game within 2 s, nor said why` instead, the hit didn't
get through the pipeline: try a ganger; if none land, stop (the spec's risk 3). Then send `hit N 3 strong`, and note how
the NPC reacts, and what the landing says.

- [x] **Step 7: In the game: the opening check, one hit on V in build mode**

Ask the user to find something that hits V (the ganger from Step 6, or the hostile drone from step 2), press F8 near
it, let it hit V once or twice, and press F8 again. Expected:
- in the log, `combat: V's Immortal on` as build mode starts, then `combat: V hurt, taken for Minecraft: <amount> of
  V's health (<share>), <x> of Minecraft's 20, <kind> (<attack type>), from NPC N`, and `combat: V's Immortal off` as it
  ends;
- in Minecraft's log, `SkyCraft: Skyrim hit the player for … (<x> Minecraft): health 20.0 -> …`;
- V's health unchanged (the status's drive line, `V's health …%`), and Minecraft's hearts down in its HUD;
- the attack type names what hit V: check the kind against it (the spec's "To Minecraft, as `kInHurt`").

- [x] **Step 8: In the game: fights**

Send `mc /item replace entity @s hotbar.0 with minecraft:diamond_sword`, `mc /item replace entity @s hotbar.1 with
minecraft:diamond_axe`, `mc /item replace entity @s hotbar.2 with minecraft:bow` and `mc /item replace entity @s
hotbar.3 with minecraft:arrow 64`, each once the last shows in the status's `request=` line; slot 5 stays empty, for
fists. Ask the user to fight NPCs in build mode, gangers best, with the sword, the axe, the bow and fists, with jump
attacks (Minecraft's crits) and sprint hits. Expected: `landed` lines for each hit, with what the pipeline did to it:
`critical` now and then (V's crits, apart from Minecraft's), `armour` on armoured NPCs, `blocked` or `parried` when an
NPC defends; some hits `dealt nothing: dodged` or `evaded`; NPCs flinch, stagger on Minecraft's crits and sprint hits,
fight back, and die, the last landing `killed it`; the game's skill XP for V's unarmed weapon comes (Reflexes for
blades, Body for fists and Gorilla Arms: `RPGManager.AwardExperienceFromDamage`); Minecraft's hearts drop as they hit
back. Note how they react, and anything that doesn't land.

- [x] **Step 9: In the game: shot at in build mode**

Ask the user to stay in build mode in a gunfight a while. Expected: Minecraft's hearts drop in its HUD with each hit,
V's health bar doesn't move, and the status's hurts count up, some summed.

- [x] **Step 10: In the game: V's weapon when combat goes off in build mode**

In build mode, send `set combat 0`. Expected: `combat: V's unarmed weapon put away: …` and `combat: V's Immortal off`;
V stays hidden (`v=hidden: …`). Send `set combat 1`: the weapon is drawn and V is Immortal again.

- [x] **Step 11: In the game: dying in Minecraft kills V**

Ask the user whether V may die now (it ends in the game's death screen, and a load), with `keep_inventory` as they chose
in step 3; and if so, to let Minecraft's player die in build mode (or send `mc /kill @s`). Expected: `Minecraft's player
died…; so V dies: V's health set to its least`; V dies in the game; Minecraft's player respawns at once. Then ask the
user to load the last save and link again. Expected: `combat: V's Immortal off …` once V is found; outside build mode,
a hit on V takes V's health; V is seen, and holds what the save had.

- [x] **Step 12: In the game: the overlay, build mode and the bodies**

Ask the user to place a few blocks of stone in build mode and walk onto them, then leave build mode. Expected: as in
step 2: the bodies line counts them, and V stands on them.

- [x] **Step 13: In the game: combat off, and unlinking**

Send `set combat 0`, then ask the user to click Unlink. Expected: `combat=off: Minecraft's hits are logged, not applied;
V's health is V's`; then `none listed: not linked`.

- [x] **Step 14: Ask the user to quit the game and stop Minecraft**

- [x] **Step 15: Read the log once more**

```bash
L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); grep -E "module:|combat:|build: V's|hit on NPC|punch:|Minecraft's player died" "/e/Cyberpunk2077/red4ext/logs/$L" | head -300
```
Everything the steps above logged, for the results.

- [x] **Step 16: Record step 4's results in `docs/spikes/2026-10-05-npcs-and-combat-results.md`**

A "Step 4" section: each check above with its outcome and numbers: what V punches with, against the game's unarmed
slot; build mode's draw and what of V the user still saw; the hit's start, what the pipeline dealt and did to it, and
V's own strike's number beside it; whether the NPC's health showed the change at once; the hurt on V, its kind and
share, and Minecraft's health; the fights (flags, endings, reactions, crits and sprint hits, return fire, deaths, XP);
being shot at; combat off in build mode; the death and the load; and anything that surprised, and what it means for
Part 3. Update the spike's answer at the top.

- [x] **Step 17: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-05-npcs-and-combat-results.md \
  && git commit -q -m "npcs-and-combat: step 4's results" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 32: Hits start at V's punch × Minecraft's damage to the hit power

Step 4 found Minecraft's weapons one-shotting level-47 gangers: each point of Minecraft's damage was a whole blade swing,
so a diamond sword (7) started at 734 and an axe (9) at 944 (the results' "Step 4"). The user chose the square root (the
spec's "Changes from step 4"): a hit now starts at V's punch × Minecraft's damage to the power `p`, the hit power, 0.5
by default (`kHitPower`). A fist (1) stays one punch, a diamond sword is 2.65 punches and an axe 3; `set hitpower`
(Task 35) changes `p` while the game runs, from 0 (every hit one punch) to 1 (step 4's). The cap of 1,000 comes first.
Until Task 35 adds the setting, the plugin passes `kHitPower`.

**Files:**
- Modify: `src/link/Combat.h`, `src/link/Combat.cpp`, `tests/TestCombat.cpp`, `src/Plugin.cpp`

- [x] **Step 1: Edit `tests/TestCombat.cpp`: where hits start, by the hit power**

```cpp
	// A diamond sword at full cooldown, and V's punch of 102: 714 of the NPC's health.
	const std::optional<NpcHit> hit = AcceptHit(Hit(id, 7.0f), ids, 10.5, 102.0, drop);
	CHECK(hit.has_value());
	CHECK(hit && hit->entity == 0xABC && hit->formId == id && !hit->strong);
	CHECK_NEAR(hit ? hit->amount : 0.0, 714.0, 1e-9);
	// Strong: a crit, or knockback over 0.45 (a sprint hit); 0.45 itself isn't, compared as SkyCraft compares, in float.
	CHECK(AcceptHit(Hit(id, 10.5f, 0.4f, true), ids, 10.5, 102.0, drop).value_or(NpcHit{}).strong);
	CHECK(AcceptHit(Hit(id, 7.0f, 0.5f), ids, 10.5, 102.0, drop).value_or(NpcHit{}).strong);
	CHECK(!AcceptHit(Hit(id, 7.0f, 0.45f), ids, 10.5, 102.0, drop).value_or(NpcHit{ 0, 0, 0.0, 0.0, true }).strong);
	// /kill's 3.4e38 is capped at 1,000.
	const std::optional<NpcHit> kill = AcceptHit(Hit(id, std::numeric_limits<float>::max()), ids, 10.5, 102.0, drop);
	CHECK_NEAR(kill ? kill->damage : 0.0, 1000.0, 1e-9);
	CHECK_NEAR(kill ? kill->amount : 0.0, 102000.0, 1e-6);
	// A stand-in listed 2 s ago still counts; 2.5 s ago it doesn't, and nor does a number never given.
	CHECK(AcceptHit(Hit(id, 7.0f), ids, 12.0, 102.0, drop).has_value());
	CHECK(!AcceptHit(Hit(id, 7.0f), ids, 12.5, 102.0, drop).has_value());
	CHECK(drop == HitDrop::NotListed);
	CHECK(!AcceptHit(Hit(id + 1, 7.0f), ids, 10.5, 102.0, drop).has_value());
	CHECK(drop == HitDrop::NotListed);
	// Knockback only, or damage that isn't finite: skipped.
	CHECK(!AcceptHit(Hit(id, 0.0f), ids, 10.5, 102.0, drop).has_value());
	CHECK(drop == HitDrop::NoDamage);
	CHECK(!AcceptHit(Hit(id, std::numeric_limits<float>::quiet_NaN()), ids, 10.5, 102.0, drop).has_value());
	CHECK(drop == HitDrop::NoDamage);
	CHECK(!AcceptHit(Hit(id, std::numeric_limits<float>::infinity()), ids, 10.5, 102.0, drop).has_value());
	CHECK(drop == HitDrop::NoDamage);
	// V's punch not read: no hit.
	CHECK(!AcceptHit(Hit(id, 7.0f), ids, 10.5, 0.0, drop).has_value());
```
```cpp
	// A diamond sword at full cooldown, and V's punch of 102, at the hit power by default: 102 x the square root of 7.
	const std::optional<NpcHit> hit = AcceptHit(Hit(id, 7.0f), ids, 10.5, 102.0, kHitPower, drop);
	CHECK(hit.has_value());
	CHECK(hit && hit->entity == 0xABC && hit->formId == id && !hit->strong);
	CHECK_NEAR(hit ? hit->amount : 0.0, 102.0 * std::sqrt(7.0), 1e-9);
	// Strong: a crit, or knockback over 0.45 (a sprint hit); 0.45 itself isn't, compared as SkyCraft compares, in float.
	CHECK(AcceptHit(Hit(id, 10.5f, 0.4f, true), ids, 10.5, 102.0, kHitPower, drop).value_or(NpcHit{}).strong);
	CHECK(AcceptHit(Hit(id, 7.0f, 0.5f), ids, 10.5, 102.0, kHitPower, drop).value_or(NpcHit{}).strong);
	CHECK(!AcceptHit(Hit(id, 7.0f, 0.45f), ids, 10.5, 102.0, kHitPower, drop).value_or(NpcHit{ 0, 0, 0.0, 0.0, true }).strong);
	// /kill's 3.4e38 is capped at 1,000 before the power.
	const std::optional<NpcHit> kill = AcceptHit(Hit(id, std::numeric_limits<float>::max()), ids, 10.5, 102.0, kHitPower, drop);
	CHECK_NEAR(kill ? kill->damage : 0.0, 1000.0, 1e-9);
	CHECK_NEAR(kill ? kill->amount : 0.0, 102.0 * std::sqrt(1000.0), 1e-9);
	// A stand-in listed 2 s ago still counts; 2.5 s ago it doesn't, and nor does a number never given.
	CHECK(AcceptHit(Hit(id, 7.0f), ids, 12.0, 102.0, kHitPower, drop).has_value());
	CHECK(!AcceptHit(Hit(id, 7.0f), ids, 12.5, 102.0, kHitPower, drop).has_value());
	CHECK(drop == HitDrop::NotListed);
	CHECK(!AcceptHit(Hit(id + 1, 7.0f), ids, 10.5, 102.0, kHitPower, drop).has_value());
	CHECK(drop == HitDrop::NotListed);
	// Knockback only, or damage that isn't finite: skipped.
	CHECK(!AcceptHit(Hit(id, 0.0f), ids, 10.5, 102.0, kHitPower, drop).has_value());
	CHECK(drop == HitDrop::NoDamage);
	CHECK(!AcceptHit(Hit(id, std::numeric_limits<float>::quiet_NaN()), ids, 10.5, 102.0, kHitPower, drop).has_value());
	CHECK(drop == HitDrop::NoDamage);
	CHECK(!AcceptHit(Hit(id, std::numeric_limits<float>::infinity()), ids, 10.5, 102.0, kHitPower, drop).has_value());
	CHECK(drop == HitDrop::NoDamage);
	// V's punch not read: no hit.
	CHECK(!AcceptHit(Hit(id, 7.0f), ids, 10.5, 0.0, kHitPower, drop).has_value());
```
```cpp
	CHECK_STR(HitResultName(7), "result 7");
```
```cpp
	CHECK_STR(HitResultName(7), "result 7");
}

TEST(CombatStartsHitsAtVsPunchTimesDamageToTheHitPower)
{
	ActorIds       ids;
	const uint32_t id = ids.Id(0xABC, 10.0);
	HitDrop        drop = HitDrop::Count;
	const auto     start = [&](float aDamage, double aPower) {
		return AcceptHit(Hit(id, aDamage), ids, 10.5, 104.87, aPower, drop).value_or(NpcHit{}).amount;
	};
	// The square root by default (step 4's fix): a fist stays one of V's punches, a diamond sword is 2.65, an axe 3, and a
	// swing before its cooldown is full is more than its share of a punch.
	CHECK_NEAR(kHitPower, 0.5, 1e-12);
	CHECK_NEAR(start(1.0f, kHitPower), 104.87, 1e-9);
	CHECK_NEAR(start(7.0f, kHitPower), 104.87 * 2.6457513110645907, 1e-9);
	CHECK_NEAR(start(9.0f, kHitPower), 314.61, 1e-9);
	CHECK_NEAR(start(0.25f, kHitPower), 52.435, 1e-9);
	// 1: each point of Minecraft's damage a whole punch, as step 4 had it; 0: every hit one punch.
	CHECK_NEAR(start(7.0f, 1.0), 734.09, 1e-9);
	CHECK_NEAR(start(7.0f, 0.0), 104.87, 1e-9);
```
- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2065: 'kHitPower': undeclared identifier`.

- [x] **Step 3: Edit `src/link/Combat.h`**

```cpp
constexpr double kHitCap = 1000.0;         // Minecraft's damage, at most: /kill on a stand-in sends 3.4e38
```
```cpp
constexpr double kHitCap = 1000.0;         // Minecraft's damage, at most: /kill on a stand-in sends 3.4e38
constexpr double kHitPower = 0.5;          // set hitpower's default: a hit starts at V's punch x damage^0.5
```
```cpp
	double   amount = 0.0;  // damage x V's punch: where the game's damage pipeline starts it
```
```cpp
	double   amount = 0.0;  // V's punch x damage^the hit power: where the game's damage pipeline starts it
```
```cpp
// at kHitCap, and V's punch, aPunch, over 0. Strong: a crit, or knockback over kStrongKnockback. Nothing, with aDrop
// saying why, if it doesn't count.
std::optional<NpcHit> AcceptHit(const proto::McEvent& aEvent, const ActorIds& aIds, double aNow, double aPunch, HitDrop& aDrop);
```
```cpp
// at kHitCap, and V's punch, aPunch, over 0. It starts at aPunch x the damage to the power aPower, the hit power, from 0
// to 1: kHitPower, the square root, keeps a fist at one punch and makes a diamond sword 2.65 (step 4's fix: at 1, each
// point of Minecraft's damage a whole punch, a sword one-shot level-47 gangers). Strong: a crit, or knockback over
// kStrongKnockback. Nothing, with aDrop saying why, if it doesn't count.
std::optional<NpcHit> AcceptHit(const proto::McEvent& aEvent, const ActorIds& aIds, double aNow, double aPunch, double aPower,
                                HitDrop& aDrop);
```
- [x] **Step 4: Edit `src/link/Combat.cpp`**

```cpp
std::optional<NpcHit> AcceptHit(const proto::McEvent& aEvent, const ActorIds& aIds, double aNow, double aPunch, HitDrop& aDrop)
```
```cpp
std::optional<NpcHit> AcceptHit(const proto::McEvent& aEvent, const ActorIds& aIds, double aNow, double aPunch, double aPower,
                                HitDrop& aDrop)
```
```cpp
	hit.amount = hit.damage * aPunch;
```
```cpp
	hit.amount = aPunch * std::pow(hit.damage, aPower);
```
- [x] **Step 5: Edit `src/Plugin.cpp`: the hit power by default, until Task 35 makes it a setting**

```cpp
	const std::optional<cpmc::link::NpcHit> accepted = cpmc::link::AcceptHit(aEvent, g_actorIds, aNow, g_punch, drop);
```
```cpp
	const std::optional<cpmc::link::NpcHit> accepted = cpmc::link::AcceptHit(aEvent, g_actorIds, aNow, g_punch, cpmc::link::kHitPower, drop);
```
- [x] **Step 6: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `440 test(s), 0 failure(s)`.

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: hits start at V's punch x Minecraft's damage to the hit power, the square root by default" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 33: The module makes V Immortal through the game's override, and hiding V stops V's effects and hides the game's crosshair

The module's side of step 4's fixes, in `scripts/BlockRender.reds`, version 5:
- **V's Immortal is the game's override.** On V, an Immortal added as a god mode did nothing: V carries `Mortal` from
  three sources, which outrank it, and the damage pipeline asks `HasGodMode` (step 4, through CET's console). So
  `SetImmortal` enables and disables the override, `EnableOverride(V, Immortal, n"BlockRender")` and
  `DisableOverride(V, n"BlockRender")`, as Second Heart does (`SecondHeartStatListener`), and returns whether the game
  then says V is Immortal. Off also removes an Immortal that version 4 added; `KillV` disables the override too.
- **Hiding V stops V's effects.** Switching an effect spawner off leaves the effects it started running (step 4: the
  Thermal blades' heat), so before it switches a spawner off, `HideV` stops each of its effects by name
  (`entKillEffectEvent`, all loops), as `GameObject.StopReplicatedEffectEvent` does. Codeware gives the names
  (`entEffectSpawnerComponent.effectDescs`). Those that start by themselves (`isAutoSpawn`) are remembered with what
  they're on, and started again when V is shown (`entSpawnEffectEvent`), on objects still there.
- **Hiding V hides the game's crosshair**, as the game hides it for the scanner: the HUD's crosshair container
  (`gameuiCrosshairContainerController`), found among the HUD layer's game controllers through Codeware, has its root
  widget switched off; again at each quarter-second call, in case the game shows it; showing V shows it again, if
  `HideV` hid it.
- **Two new fields** tell BlockRender what `HideV` did besides its meshes: `vEffects`, the effects stopped since V was
  hidden (or started again), and `vCrosshair` (1 hidden or shown again; -1 not found; 0 not touched).

As before, the parts that need Codeware compile only where it's installed: Step 3 checks that a slip in them fails only
with Codeware's scripts.

**Files:**
- Modify: `scripts/BlockRender.reds`

- [x] **Step 1: Edit `scripts/BlockRender.reds`**

```swift
// For build mode, Arm draws V's unarmed weapon, so that hits carry it, and HideV hides V's meshes, through Codeware where
// it's installed.
```
```swift
// For build mode, Arm draws V's unarmed weapon, so that hits carry it, and HideV hides V's meshes, stops V's effects and
// hides the game's crosshair, through Codeware where it's installed.
```
```swift
  private let hidden: array<wref<IComponent>>;
```
```swift
  private let hidden: array<wref<IComponent>>;
  // HideV's too: the effects it stopped that start by themselves, and what each is on, to start again; and whether it
  // hid the game's crosshair.
  private let restartOn: array<wref<Entity>>;
  private let restartNames: array<CName>;
  private let crosshairHidden: Bool;
```
```swift
  public let vHealthMax: Float;

  // The module's version, which BlockRender checks: 2 since the list moved into BlockRender, 3 with combat, 4 since hits
  // go through the whole damage pipeline, with what V's unarmed slot draws.
  public final static func Version() -> Int32 {
    return 4;
```
```swift
  public let vHealthMax: Float;
  // What HideV last did, besides the meshes it gives, for BlockRender to read after it. Hiding: the effects it has
  // stopped since V was hidden, and the crosshair (1 hidden, -1 not found among the HUD layer's game controllers).
  // Showing: the effects it started again, and the crosshair (1 shown again, 0 it hadn't hidden it).
  public let vEffects: Int32;
  public let vCrosshair: Int32;

  // The module's version, which BlockRender checks: 2 since the list moved into BlockRender, 3 with combat, 4 since hits
  // go through the whole damage pipeline, with what V's unarmed slot draws, 5 since V's Immortal is the game's override,
  // and HideV stops V's effects and hides the game's crosshair.
  public final static func Version() -> Int32 {
    return 5;
```
```swift
  // V's god mode for build mode: Immortal on V, added or removed under BlockRender's own source name only. Whether the
  // game says it changed.
```
```swift
  // V's god mode for build mode: Immortal forced on V by the game's override, enabled or disabled under BlockRender's own
  // source name only, as Second Heart forces it. An Immortal added as a god mode does nothing on V, whose Mortal god
  // modes outrank it (step 4). Off also removes an Immortal that module version 4 added. Whether the game then says V is
  // Immortal.
```
```swift
    };
    let gods: ref<GodModeSystem> = GameInstance.GetGodModeSystem(game);
    if on {
      return gods.AddGodMode(player.GetEntityID(), gameGodModeType.Immortal, n"BlockRender");
    };
    return gods.RemoveGodMode(player.GetEntityID(), gameGodModeType.Immortal, n"BlockRender");
```
```swift
    };
    let id: EntityID = player.GetEntityID();
    let gods: ref<GodModeSystem> = GameInstance.GetGodModeSystem(game);
    if on {
      gods.EnableOverride(id, gameGodModeType.Immortal, n"BlockRender");
    } else {
      gods.DisableOverride(id, n"BlockRender");
      gods.RemoveGodMode(id, gameGodModeType.Immortal, n"BlockRender");
    };
    return gods.HasGodMode(id, gameGodModeType.Immortal);
```
```swift
    let gods: ref<GodModeSystem> = GameInstance.GetGodModeSystem(game);
    gods.RemoveGodMode(id, gameGodModeType.Immortal, n"BlockRender");
```
```swift
    let gods: ref<GodModeSystem> = GameInstance.GetGodModeSystem(game);
    gods.DisableOverride(id, n"BlockRender");
    gods.RemoveGodMode(id, gameGodModeType.Immortal, n"BlockRender");
```
```swift
  // V's meshes for build mode: V's own, and those of everything V wears and holds, hidden (on), each switched off if it
  // was on, and remembered, so that showing V again (off) switches on exactly those and leaves off what the game had off.
  // Called again while on, it hides what has appeared since on V and in V's hands, such as the weapon Arm drew, without
  // looking through V's whole inventory again. The meshes hidden, or shown again; -1 without Codeware, which lists an
  // entity's components; -2 if V isn't found.
```
```swift
  // V for build mode: V's meshes, and those of everything V wears and holds, hidden (on), each switched off if it was on,
  // and remembered, so that showing V again (off) switches on exactly those and leaves off what the game had off. An
  // effect spawner's effects are stopped by name before it goes off, since switching it off leaves them running (step 4:
  // the Thermal blades' heat), and those that start by themselves are started again when V is shown. And the game's
  // crosshair, hidden with V and shown again if HideV hid it. Called again while on, it hides what has appeared since on
  // V and in V's hands, such as the weapon Arm drew, without looking through V's whole inventory again, and the crosshair
  // again if the game showed it. The meshes hidden, or shown again; -1 without Codeware, which lists an entity's
  // components; -2 if V isn't found. vEffects and vCrosshair say the rest.
```
```swift
      };
      ArrayClear(this.hidden);
      this.hiding = false;
```
```swift
      };
      this.vEffects = 0;
      i = 0;
      while i < ArraySize(this.restartOn) {
        let entity: ref<Entity> = this.restartOn[i];
        if IsDefined(entity) {
          let start: ref<entSpawnEffectEvent> = new entSpawnEffectEvent();
          start.effectName = this.restartNames[i];
          entity.QueueEvent(start);
          this.vEffects += 1;
        };
        i += 1;
      };
      this.vCrosshair = this.crosshairHidden ? BlockRenderShowCrosshair(true) : 0;
      ArrayClear(this.hidden);
      ArrayClear(this.restartOn);
      ArrayClear(this.restartNames);
      this.hiding = false;
      this.crosshairHidden = false;
```
```swift
    let off: array<wref<IComponent>> = BlockRenderSwitchOffMeshes(player, !this.hiding);
```
```swift
    if !this.hiding {
      this.vEffects = 0;
    };
    let off: array<wref<IComponent>>;
    let startOn: array<wref<Entity>>;
    let startNames: array<CName>;
    this.vEffects += BlockRenderSwitchOffMeshes(player, !this.hiding, off, startOn, startNames);
```
```swift
    };
    return ArraySize(this.hidden);
```
```swift
    };
    i = 0;
    while i < ArraySize(startOn) {
      ArrayPush(this.restartOn, startOn[i]);
      ArrayPush(this.restartNames, startNames[i]);
      i += 1;
    };
    let crosshair: Int32 = BlockRenderShowCrosshair(false);
    if crosshair == 1 {
      this.crosshairHidden = true;
    };
    this.vCrosshair = crosshair < 0 ? -1 : 1;
    return ArraySize(this.hidden);
```
```swift
// V's meshes, and those of the items V wears and holds: each visual component that's on, switched off; those it switched
// off. With everything, every item V wears and holds, found among V's whole inventory; otherwise only what's in V's
// hands and arm slot, which is all that changes in build mode. Main thread, from HideV.
@if(ModuleExists("Codeware"))
public func BlockRenderSwitchOffMeshes(player: ref<GameObject>, everything: Bool) -> array<wref<IComponent>> {
  let off: array<wref<IComponent>>;
  BlockRenderSwitchOffEntity(player, off);
  let transactions: ref<TransactionSystem> = GameInstance.GetTransactionSystem(player.GetGame());
  if !everything {
    BlockRenderSwitchOffEntity(transactions.GetItemInSlot(player, t"AttachmentSlots.WeaponRight"), off);
    BlockRenderSwitchOffEntity(transactions.GetItemInSlot(player, t"AttachmentSlots.WeaponLeft"), off);
    BlockRenderSwitchOffEntity(transactions.GetItemInSlot(player, t"AttachmentSlots.RightArm"), off);
    return off;
```
```swift
// V's meshes, and those of the items V wears and holds: each visual component that's on, switched off, into off; an
// effect spawner's effects stopped by name first, those that start by themselves into restartOn and restartNames. With
// everything, every item V wears and holds, found among V's whole inventory; otherwise only what's in V's hands and arm
// slot, which is all that changes in build mode. The effects stopped. Main thread, from HideV.
@if(ModuleExists("Codeware"))
public func BlockRenderSwitchOffMeshes(player: ref<GameObject>, everything: Bool, out off: array<wref<IComponent>>,
  out restartOn: array<wref<Entity>>, out restartNames: array<CName>) -> Int32 {
  let stopped: Int32 = BlockRenderSwitchOffEntity(player, off, restartOn, restartNames);
  let transactions: ref<TransactionSystem> = GameInstance.GetTransactionSystem(player.GetGame());
  if !everything {
    stopped += BlockRenderSwitchOffEntity(transactions.GetItemInSlot(player, t"AttachmentSlots.WeaponRight"), off, restartOn, restartNames);
    stopped += BlockRenderSwitchOffEntity(transactions.GetItemInSlot(player, t"AttachmentSlots.WeaponLeft"), off, restartOn, restartNames);
    stopped += BlockRenderSwitchOffEntity(transactions.GetItemInSlot(player, t"AttachmentSlots.RightArm"), off, restartOn, restartNames);
    return stopped;
```
```swift
        BlockRenderSwitchOffEntity(item, off);
```
```swift
        stopped += BlockRenderSwitchOffEntity(item, off, restartOn, restartNames);
```
```swift
  return off;
}

@if(ModuleExists("Codeware"))
public func BlockRenderSwitchOffEntity(entity: ref<Entity>, out off: array<wref<IComponent>>) -> Void {
  if !IsDefined(entity) {
    return;
  };
```
```swift
  return stopped;
}

// One entity's visual components that are on: each switched off, into off. An effect spawner's effects are stopped
// first, each by name with all its loops (switching the spawner off leaves them running), and those that start by
// themselves go into restartOn and restartNames. The effects stopped.
@if(ModuleExists("Codeware"))
public func BlockRenderSwitchOffEntity(entity: ref<Entity>, out off: array<wref<IComponent>>,
  out restartOn: array<wref<Entity>>, out restartNames: array<CName>) -> Int32 {
  if !IsDefined(entity) {
    return 0;
  };
  let stopped: Int32 = 0;
```
```swift
    if IsDefined(component as IVisualComponent) && component.IsEnabled() {
```
```swift
    if IsDefined(component as IVisualComponent) && component.IsEnabled() {
      let spawner: ref<entEffectSpawnerComponent> = component as entEffectSpawnerComponent;
      if IsDefined(spawner) {
        let k: Int32 = 0;
        while k < ArraySize(spawner.effectDescs) {
          let effect: ref<entEffectDesc> = spawner.effectDescs[k];
          if IsDefined(effect) && IsNameValid(effect.effectName) {
            let stop: ref<entKillEffectEvent> = new entKillEffectEvent();
            stop.effectName = effect.effectName;
            stop.breakAllLoops = true;
            entity.QueueEvent(stop);
            stopped += 1;
            if effect.isAutoSpawn {
              ArrayPush(restartOn, entity);
              ArrayPush(restartNames, effect.effectName);
            };
          };
          k += 1;
        };
      };
```
```swift
  };
}

@if(!ModuleExists("Codeware"))
public func BlockRenderSwitchOffMeshes(player: ref<GameObject>, everything: Bool) -> array<wref<IComponent>> {
  let off: array<wref<IComponent>>;
  return off;
```
```swift
  };
  return stopped;
}

// The game's crosshair, hidden with V (show false) or shown again (true), as the game hides it for the scanner: the
// HUD's crosshair container, its root widget switched off or on. 1: switched; 0: it already was as asked; -1: the
// container isn't among the HUD layer's game controllers. Main thread, from HideV.
@if(ModuleExists("Codeware"))
public func BlockRenderShowCrosshair(show: Bool) -> Int32 {
  let ink: ref<inkSystem> = GameInstance.GetInkSystem();
  if !IsDefined(ink) {
    return -1;
  };
  let layer: ref<inkLayerWrapper> = ink.GetLayer(n"inkHUDLayer");
  if !IsDefined(layer) {
    return -1;
  };
  let controllers: array<wref<inkIGameController>> = layer.GetGameControllers();
  let i: Int32 = 0;
  while i < ArraySize(controllers) {
    let crosshair: ref<gameuiCrosshairContainerController> = controllers[i] as gameuiCrosshairContainerController;
    if IsDefined(crosshair) {
      let root: ref<inkWidget> = crosshair.GetRootWidget();
      if !IsDefined(root) {
        return -1;
      };
      if Equals(root.IsVisible(), show) {
        return 0;
      };
      root.SetVisible(show);
      return 1;
    };
    i += 1;
  };
  return -1;
}

@if(!ModuleExists("Codeware"))
public func BlockRenderSwitchOffMeshes(player: ref<GameObject>, everything: Bool, out off: array<wref<IComponent>>,
  out restartOn: array<wref<Entity>>, out restartNames: array<CName>) -> Int32 {
  return 0;
}

@if(!ModuleExists("Codeware"))
public func BlockRenderShowCrosshair(show: Bool) -> Int32 {
  return -1;
```
- [x] **Step 2: Compile the module against the game's scripts, without and with Codeware**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && uv run --no-project python tools/check_reds.py E:/Cyberpunk2077 scripts | tail -1; \
  echo "without Codeware: exit ${PIPESTATUS[0]}"; \
  uv run --no-project python tools/check_reds.py E:/Cyberpunk2077 scripts E:/Cyberpunk2077/red4ext/plugins/Codeware/Scripts | tail -1; \
  echo "with Codeware: exit ${PIPESTATUS[0]}"
```
Expected: `compiled`, `without Codeware: exit 0` and `with Codeware: exit 0`.

- [x] **Step 3: Check that the compile catches a slip in the parts for Codeware, only with Codeware**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/reds-slip \
  && sed 's/= layer.GetGameControllers();/= layer.GetGameControllerz();/' scripts/BlockRender.reds > build/reds-slip/BlockRender.reds \
  && uv run --no-project python tools/check_reds.py E:/Cyberpunk2077 build/reds-slip | grep -E "^compiled|^failed|UNRESOLVED"; \
  echo "without Codeware: exit ${PIPESTATUS[0]}"; \
  uv run --no-project python tools/check_reds.py E:/Cyberpunk2077 build/reds-slip E:/Cyberpunk2077/red4ext/plugins/Codeware/Scripts \
    | grep -E "^compiled|^failed|UNRESOLVED"; echo "with Codeware: exit ${PIPESTATUS[0]}"
```
Expected: `compiled` and `without Codeware: exit 0`; then redscript's `[UNRESOLVED_METHOD]` at the slip's line, `failed`
and `with Codeware: exit 1`.

- [x] **Step 4: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: the module makes V Immortal through the game's override, and hiding V stops V's effects and hides the game's crosshair" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 34: The status shows the hit power, and the effects stopped and the crosshair while V is hidden

- **The combat line** gains its own part for the hit power, `; hit power 0.50;`, after V's punch and before the hits.
  The overlay's reading of the line doesn't change: it finds `; hits` and `drawn` as before.
- **The v line,** while V is hidden, counts the effects stopped and says whether the crosshair is hidden:
  `v=hidden: 112 meshes switched off, 3 effects stopped, the crosshair hidden`, or `…, the crosshair not found`.

**Files:**
- Modify: `src/Status.h`, `src/Status.cpp`, `tests/TestStatus.cpp`

- [x] **Step 1: Edit `tests/TestStatus.cpp`: the hit power, and the v line's effects and crosshair**

```cpp
	                                      "blocks and skill XP; hits 0 queued, 0 landed; V's hurts: 0 taken, 0 sent, 0 summed, 0 left to V, "
	                                      "0 non-lethal left to V; Immortal off; 0 deaths; Minecraft's player alive"));
```
```cpp
	                                      "blocks and skill XP; hit power 0.50; hits 0 queued, 0 landed; V's hurts: 0 taken, 0 sent, 0 "
	                                      "summed, 0 left to V, 0 non-lethal left to V; Immortal off; 0 deaths; Minecraft's player alive"));
	link.hitPower = 1.0;
```
```cpp
	CHECK(Has(cpmc::FormatStatus(report), "combat=on: V's punch 105.00 from Items.mantis_blade, drawn; hits 5 queued, 3 landed, dropped: 1 "
	                                      "dodged, 1 not listed; the last NPC 35 (NC Resident): 7.00 of Minecraft's damage, started at "
```
```cpp
	CHECK(Has(cpmc::FormatStatus(report), "combat=on: V's punch 105.00 from Items.mantis_blade, drawn; hit power 1.00; hits 5 queued, 3 "
	                                      "landed, dropped: 1 dodged, 1 not listed; the last NPC 35 (NC Resident): 7.00 of Minecraft's "
	                                      "damage, started at "
```
```cpp
	CHECK(Has(cpmc::FormatStatus(report), "v=hidden: 23 meshes switched off"));
```
```cpp
	CHECK(Has(cpmc::FormatStatus(report), "v=hidden: 23 meshes switched off, 0 effects stopped"));
	link.vEffects = 3;
	link.vCrosshair = 1;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "v=hidden: 23 meshes switched off, 3 effects stopped, the crosshair hidden"));
	link.vCrosshair = -1;
	report.link = link;
	CHECK(Has(cpmc::FormatStatus(report), "v=hidden: 23 meshes switched off, 3 effects stopped, the crosshair not found"));
```
- [x] **Step 2: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2039: 'hitPower': is not a member of 'cpmc::LinkReport'`.

- [x] **Step 3: Edit `src/Status.h`**

```cpp
	int32_t vMeshes = 0;      // as the module's HideV last said: the meshes it switched off; -1 no Codeware; -2 V isn't found
```
```cpp
	int32_t vMeshes = 0;      // as the module's HideV last said: the meshes it switched off; -1 no Codeware; -2 V isn't found
	int32_t vEffects = 0;     // and the effects it has stopped since V was hidden
	int32_t vCrosshair = 0;   // and the game's crosshair: 1 hidden; -1 not found among the HUD layer's game controllers
```
```cpp
	bool                  punchDrawn = false;  // V has it drawn, so hits carry it as their weapon
```
```cpp
	bool                  punchDrawn = false;  // V has it drawn, so hits carry it as their weapon
	double                hitPower = 0.5;    // set hitpower (link::kHitPower): hits start at V's punch x damage^this
```
- [x] **Step 4: Edit `src/Status.cpp`**

```cpp
		return std::format("v=hidden: {} meshes switched off\n", aLink.vMeshes);
```
```cpp
		return std::format("v=hidden: {} meshes switched off, {} effects stopped{}\n", aLink.vMeshes, aLink.vEffects,
		                   aLink.vCrosshair > 0   ? ", the crosshair hidden"
		                   : aLink.vCrosshair < 0 ? ", the crosshair not found"
		                                          : "");
```
```cpp
	std::string out = std::format("combat=on: V's punch {:.2f}{}{}; hits {} queued, {} landed{}", aLink.punch,
	                              aLink.punchItem.empty() ? "" : " from " + OneLine(aLink.punchItem),
	                              aLink.punchDrawn ? ", drawn" : ", not drawn: hits skip the NPC's armour, weapon crits, stealth, blocks and skill XP",
	                              aLink.hitsQueued, aLink.hitsLanded, aLink.hitsDropped.empty() ? "" : ", dropped: " + OneLine(aLink.hitsDropped));
```
```cpp
	std::string out = std::format("combat=on: V's punch {:.2f}{}{}; hit power {:.2f}; hits {} queued, {} landed{}", aLink.punch,
	                              aLink.punchItem.empty() ? "" : " from " + OneLine(aLink.punchItem),
	                              aLink.punchDrawn ? ", drawn" : ", not drawn: hits skip the NPC's armour, weapon crits, stealth, blocks and skill XP",
	                              aLink.hitPower, aLink.hitsQueued, aLink.hitsLanded,
	                              aLink.hitsDropped.empty() ? "" : ", dropped: " + OneLine(aLink.hitsDropped));
```
- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `440 test(s), 0 failure(s)`.

- [x] **Step 6: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: the status shows the hit power, and the effects stopped and the crosshair while V is hidden" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 35: BlockRender 0.18.1 starts hits by set hitpower, says what the game says of V's Immortal, and reads what hiding V did

`GameScript` needs module version 5: 10 functions and 16 fields, `vEffects` and `vCrosshair` (`Int32`) the new ones.
Its `HideV` now gives what the module's did, a `VLook`: the meshes, as before, then the two fields read just after the
call. In the plugin, BlockRender 0.18.1:
- **`set hitpower <0 to 1>`**, 0.5 by default, which `AcceptHit` takes, and the hit's log line shows:
  `starting at 277.46 = V's punch 104.87 x 7.00^0.50`;
- **V's Immortal in the game's words:** the module now says whether the game says V is Immortal, so the log says
  `combat: V's Immortal on: the game says V is Immortal`, or `but the game says V isn't Immortal`; off, `the game says
  V isn't Immortal`, or `but the game says V is still Immortal, from another source such as Second Heart`. The status's
  Immortal is the game's word too;
- **what hiding V did,** in the log, `build: V's meshes hidden: 112 meshes switched off, 3 effects stopped, the
  crosshair hidden (0.07 ms)`, logged when any of it changes, and in the status's v line.

**Files:**
- Modify: `src/GameScript.h`, `src/GameScript.cpp`, `src/Plugin.cpp`, `CMakeLists.txt`

- [x] **Step 1: Edit `src/GameScript.h`**

```cpp
constexpr int32_t kModuleVersion = 4;  // BlockRenderNpcs.Version(), as this BlockRender needs it
```
```cpp
constexpr int32_t kModuleVersion = 5;  // BlockRenderNpcs.Version(), as this BlockRender needs it
```
```cpp
	float                        vHealthMax = 0.0f;
```
```cpp
	float                        vHealthMax = 0.0f;
};

// What the module's HideV did: the meshes hidden, or shown again (-1 without Codeware, -2 V isn't found). Hiding: the
// effects stopped since V was hidden, and the game's crosshair (1 hidden, -1 not found among the HUD layer's game
// controllers). Showing: the effects started again, and the crosshair (1 shown again, 0 HideV hadn't hidden it).
struct VLook
{
	int32_t meshes = 0;
	int32_t effects = 0;
	int32_t crosshair = 0;

	bool operator==(const VLook&) const = default;
```
```cpp
	// V's meshes hidden (aOn), or shown again: the module's HideV gives how many (-1 without Codeware, -2 V isn't found).
	std::optional<int32_t> HideV(bool aOn, std::string& aWhy);
	// V's Immortal, under BlockRender's own source name: on or off. Whether the game says it changed.
```
```cpp
	// V hidden (aOn), or shown again: what the module's HideV did, with its meshes, effects and the game's crosshair.
	std::optional<VLook> HideV(bool aOn, std::string& aWhy);
	// V's Immortal, the game's override under BlockRender's own source name: on or off. Whether the game then says V is
	// Immortal.
```
```cpp
		kVHealthMax,
```
```cpp
		kVHealthMax,
		kVEffects,
		kVCrosshair,
```
- [x] **Step 2: Edit `src/GameScript.cpp`**

```cpp
// In GameScript::Field's order: the fields List fills, and their types.
```
```cpp
// In GameScript::Field's order: the fields List fills, and those HideV fills, and their types.
```
```cpp
		{ "punchDrawn", "Bool" },          { "vHealthMax", "Float" },
```
```cpp
		{ "punchDrawn", "Bool" },          { "vHealthMax", "Float" },           { "vEffects", "Int32" },
		{ "vCrosshair", "Int32" },
```
```cpp
std::optional<int32_t> GameScript::HideV(bool aOn, std::string& aWhy)
{
	return Switch(kHideV, "HideV", aOn, aWhy);
```
```cpp
std::optional<VLook> GameScript::HideV(bool aOn, std::string& aWhy)
{
	const std::optional<int32_t> meshes = Switch(kHideV, "HideV", aOn, aWhy);
	if (!meshes)
	{
		return std::nullopt;
	}
	const Strong system = SessionSystem(*functions_[kCurrent], *class_, aWhy);
	if (!system)
	{
		return std::nullopt;
	}
	RED4ext::IScriptable& instance = *system.GetPtr();
	return VLook{ *meshes, FieldOf<int32_t>(*fields_[kVEffects], instance), FieldOf<int32_t>(*fields_[kVCrosshair], instance) };
```
- [x] **Step 3: Edit `src/Plugin.cpp`**

```cpp
// go to Minecraft's player, whose death kills V. In build mode V's meshes are hidden, and with combat on V's unarmed
// weapon is drawn, so that hits carry it.
```
```cpp
// go to Minecraft's player, whose death kills V. In build mode V's meshes and effects and the game's crosshair are
// hidden, and with combat on V's unarmed weapon is drawn, so that hits carry it.
```
```cpp
constexpr const char* kVersion = "0.18.0";
```
```cpp
constexpr const char* kVersion = "0.18.1";
```
```cpp
bool                                  g_punchDrawn = false;  // V has it drawn, so hits carry it as their weapon
```
```cpp
bool                                  g_punchDrawn = false;  // V has it drawn, so hits carry it as their weapon
double                                g_hitPower = cpmc::link::kHitPower;  // set hitpower: hits start at the punch x damage^this
```
```cpp
std::optional<int32_t>                g_lastHide;
int32_t                               g_vMeshes = 0;         // the meshes HideV has switched off
```
```cpp
std::optional<cpmc::VLook>            g_lastHide;
cpmc::VLook                           g_vLook;               // what HideV has done while V is hidden: meshes, effects, crosshair
```
```cpp
// the module, starting the game's damage pipeline at Minecraft's damage x V's punch.
```
```cpp
// the module, starting the game's damage pipeline at V's punch x Minecraft's damage to the hit power.
```
```cpp
	const std::optional<cpmc::link::NpcHit> accepted = cpmc::link::AcceptHit(aEvent, g_actorIds, aNow, g_punch, cpmc::link::kHitPower, drop);
```
```cpp
	const std::optional<cpmc::link::NpcHit> accepted = cpmc::link::AcceptHit(aEvent, g_actorIds, aNow, g_punch, g_hitPower, drop);
```
```cpp
	Log("{}: starting at {:.2f}, {:.2f} x V's punch {:.2f}{}{}; queued", hit, accepted->amount, accepted->damage, g_punch,
	    accepted->strong ? ", strong" : "", g_punchDrawn ? "" : ", without V's weapon drawn");
```
```cpp
	Log("{}: starting at {:.2f} = V's punch {:.2f} x {:.2f}^{:.2f}{}{}; queued", hit, accepted->amount, g_punch, accepted->damage,
	    g_hitPower, accepted->strong ? ", strong" : "", g_punchDrawn ? "" : ", without V's weapon drawn");
```
```cpp
// What the module's HideV said, in words.
std::string HideWords(int32_t aResult, bool aOn)
{
	switch (aResult)
```
```cpp
// What the module's HideV did, in words.
std::string HideWords(const cpmc::VLook& aLook, bool aOn)
{
	switch (aLook.meshes)
```
```cpp
		return std::format("{} meshes {}", aResult, aOn ? "switched off" : "switched on again");
	}
```
```cpp
		break;
	}
	if (aOn)
	{
		return std::format("{} meshes switched off, {} effects stopped, the crosshair {}", aLook.meshes, aLook.effects,
		                   aLook.crosshair > 0 ? "hidden" : "not found among the HUD layer's game controllers");
	}
	return std::format("{} meshes switched on again, {} effects started again{}", aLook.meshes, aLook.effects,
	                   aLook.crosshair > 0 ? ", the crosshair shown again" : "");
}

// What the game says of V's Immortal after the module's SetImmortal, in words.
std::string ImmortalWords(bool aOn, bool aImmortal)
{
	if (aOn)
	{
		return aImmortal ? "the game says V is Immortal" : "but the game says V isn't Immortal";
	}
	return aImmortal ? "but the game says V is still Immortal, from another source such as Second Heart" : "the game says V isn't Immortal";
```
```cpp
		std::string                  why;
		const double                 start = cpmc::SteadySeconds();
		const std::optional<int32_t> result = g_script.HideV(*hide, why);
		const double                 ms = (cpmc::SteadySeconds() - start) * 1000.0;
		if (!*hide || result != g_lastHide)
		{
			Log("build: V's meshes {}: {} ({:.2f} ms)", *hide ? "hidden" : "shown", result ? HideWords(*result, *hide) : "not done: " + why,
			    ms);
		}
		g_lastHide = *hide ? result : std::nullopt;
		g_vMeshes = *hide ? result.value_or(0) : 0;
	}
	else if (!g_hidden.On())
	{
		g_vMeshes = 0;
```
```cpp
		std::string                      why;
		const double                     start = cpmc::SteadySeconds();
		const std::optional<cpmc::VLook> look = g_script.HideV(*hide, why);
		const double                     ms = (cpmc::SteadySeconds() - start) * 1000.0;
		if (!*hide || look != g_lastHide)
		{
			Log("build: V's meshes {}: {} ({:.2f} ms)", *hide ? "hidden" : "shown", look ? HideWords(*look, *hide) : "not done: " + why, ms);
		}
		g_lastHide = *hide ? look : std::nullopt;
		g_vLook = *hide ? look.value_or(cpmc::VLook{}) : cpmc::VLook{};
	}
	else if (!g_hidden.On())
	{
		g_vLook = {};
```
```cpp
		const std::optional<bool> changed = g_script.SetImmortal(*change, why);
		if (changed)
		{
			g_immortal = *change;
		}
		Log("combat: V's Immortal {}{}", *change ? "on" : "off",
		    changed ? (*changed ? std::string() : std::string(" (the game says it already was)")) : ": not done: " + why);
```
```cpp
		const std::optional<bool> immortal = g_script.SetImmortal(*change, why);
		if (immortal)
		{
			g_immortal = *immortal;
		}
		Log("combat: V's Immortal {}: {}", *change ? "on" : "off", immortal ? ImmortalWords(*change, *immortal) : "not done: " + why);
```
```cpp
		return Report(g_hideV ? "set hidev 1: V's meshes are hidden while build mode is on" : "set hidev 0: V stays seen in build mode");
```
```cpp
		return Report(g_hideV ? "set hidev 1: V's meshes are hidden while build mode is on" : "set hidev 0: V stays seen in build mode");
	}
	if (aRequest.what == "hitpower")
	{
		if (aRequest.number < 0.0 || aRequest.number > 1.0)
		{
			return Report("set hitpower: from 0 to 1, the power of Minecraft's damage that V's punch is multiplied by: 0.5, the default, is "
			              "the square root; 1 makes each point of damage a whole punch, 0 every hit one punch");
		}
		g_hitPower = aRequest.number;
		return Report(std::format("set hitpower {:.2f}: a hit starts at V's punch x Minecraft's damage^{:.2f}: a fist (1) one punch, a "
		                          "diamond sword (7) {:.2f} punches",
		                          g_hitPower, g_hitPower, std::pow(7.0, g_hitPower)));
```
```cpp
	report.punchDrawn = g_punchDrawn;
```
```cpp
	report.punchDrawn = g_punchDrawn;
	report.hitPower = g_hitPower;
```
```cpp
	report.vMeshes = g_vMeshes;
```
```cpp
	report.vMeshes = g_vLook.meshes;
	report.vEffects = g_vLook.effects;
	report.vCrosshair = g_vLook.crosshair;
```
- [x] **Step 4: Edit `CMakeLists.txt`: version 0.18.1**

```cmake
project(BlockRender VERSION 0.18.0 LANGUAGES C CXX)
```
```cmake
project(BlockRender VERSION 0.18.1 LANGUAGES C CXX)
```
- [x] **Step 5: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `440 test(s), 0 failure(s)`.

- [x] **Step 6: Check that BlockRender imports nothing from PhysX**

```bash
DUMPBIN="$(ls "/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/"*/bin/Hostx64/x64/dumpbin.exe | head -1)"
cd /d/Cyberpunk-Minecraft/spikes/block-render && MSYS_NO_PATHCONV=1 "$DUMPBIN" -imports -nologo build\\RelWithDebInfo\\BlockRender.dll \
  | grep -ci "physx"
```
Expected: `0`.

- [x] **Step 7: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add spikes/block-render \
  && git commit -q -m "npcs-and-combat: BlockRender 0.18.1 starts hits by set hitpower, says what the game says of V's Immortal, and reads what hiding V did to its effects and the crosshair, from module version 5" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 36: In the game: step 4's fixes

The spec's "Step 4's fixes, in the game", with the user, on BlockRender 0.18.1 with `set combat 1`, linked to SkyCraft's
Minecraft in the `CyberCraft` instance, its player in survival, at the 6th Street gang spot of step 4 (the save behind
the car). The opening checks settle what the fixes left to the game:
- **V's Immortal:** whether the game says V is Immortal in build mode with combat on, through our override, and not out
  of it (the spec's risk 18);
- **the crosshair and the heat:** whether both are gone with V in build mode, and back when it ends, the heat restarted
  on blades V still holds (risks 19 and 20);
- **where hits start:** `hit N 1` at one punch, and a diamond sword's hit at 2.65 punches.

Then the fights again, now that Minecraft's weapons don't one-shot: what step 4 couldn't see, blocks, dodges, the
NPC's armour, staggers and the skill XP; and `set hitpower` at another value.

FreeFly (F7) stays off. Note what you see as you go: Step 12 records it. If the game freezes, read the logs and the
newest folder in `%LOCALAPPDATA%\REDEngine\ReportQueue`, and force-close it only when the user asks. If a check fails,
stop and find the cause with superpowers:systematic-debugging before going on. Fixes become new tasks. In build mode
Minecraft's death kills V, as step 4 found: watch its hearts (the user has golden apples in slot 6 and golden carrots in
slot 7). CET's overlay key ends build mode, so CET's console is for checks outside it.

The commands of Task 31 come back: sending a request, the log's newest lines, the status, and Minecraft's log.

**Files:**
- Modify: `docs/spikes/2026-10-05-npcs-and-combat-results.md`

- [x] **Step 1: Deploy (the game must not be running)**

```bash
tasklist | grep -i -c "cyberpunk2077.exe"
```
Expected: `0`. If it's `1`, ask the user to quit the game first, because a running game locks the DLL.
```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
P=/e/Cyberpunk2077/red4ext/plugins/BlockRender
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default -DCP2077_DIR=E:/Cyberpunk2077 > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C|error MSB"; \
  "$CMAKE" --preset default -DCP2077_DIR= > /dev/null; \
  cmp build/RelWithDebInfo/BlockRender.dll "$P/BlockRender.dll" && cmp scripts/BlockRender.reds "$P/Scripts/BlockRender.reds" \
  && echo installed
```
Expected: `installed`. The second configure clears `CP2077_DIR` again.

- [x] **Step 2: In the game: the module and its natives**

Ask the user to start the `CyberCraft` instance, then the game, and to wait at the game's main menu. If a "REDScript
compilation has failed" message box shows instead, ask the user to click OK and quit the game, and read redscript's log
for why. Otherwise:
```bash
grep -E "BlockRender|Codeware.Global|Compilation complete|ERROR" /e/Cyberpunk2077/r6/logs/redscript_rCURRENT.log; \
  L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); grep -E "loaded|natives:" "/e/Cyberpunk2077/red4ext/logs/$L"
```
Expected: the module's path and Codeware's among the files compiled, `Compilation complete`, and no `ERROR` line; in
BlockRender's log, `BlockRender 0.18.1 loaded` and the seven natives added.

- [x] **Step 3: In the game: load, link, and combat on**

Ask the user to load the save behind the 6th Street car, or another spot with gangers who haven't seen V, with the gun
drawn, and click Link. Then send `set combat 1`.
Expected:
- in the log, `module: found BlockRenderNpcs version 5, its 10 functions, the 16 fields List fills, and BlockRender's 7
  natives`, and `combat: V's Immortal off: the game says V isn't Immortal` once V is found;
- in the status, `combat=on: V's punch 104.87 from Items.AdvancedMantisBladesThermalLegendary, not drawn: hits skip …;
  hit power 0.50; hits 0 queued, 0 landed; …` and `v=shown, and hidden in build mode`.

- [x] **Step 4: In the game: the opening check, V's Immortal by the game's word**

Ask the user to press F8 behind the car, stay a few seconds, and press F8 again. Expected, in the log: `combat: V's
Immortal on: the game says V is Immortal` as build mode starts, and `combat: V's Immortal off: the game says V isn't
Immortal` as it ends; the status's combat line says `Immortal on` meanwhile. If it says `but the game says V isn't
Immortal`, the override doesn't make V Immortal at once: stop, and ask the user to check with CET's console, outside
build mode, whether `EnableOverride` takes effect a frame later.

- [x] **Step 5: In the game: the opening check, the crosshair and the heat gone with V**

With the gun drawn, ask the user to press F8 and look around and down. Expected:
- in the log, `build: V's meshes hidden: N meshes switched off, E effects stopped, the crosshair hidden (… ms)`, and
  within a second, once the blades are drawn, `… M meshes switched off, F effects stopped, the crosshair hidden`, F more
  than E;
- in the status, `v=hidden: M meshes switched off, F effects stopped, the crosshair hidden`;
- on screen, nothing of V's, no crosshair and no heat: ask the user what they see.

Then F8 again. Expected: `build: V's meshes shown: … meshes switched on again, … effects started again, the crosshair
shown again`; the crosshair back with the gun. Then ask the user to draw the blades (slot 4), outside build mode, and
see their heat; press F8 (`combat: V's unarmed weapon drawn: V has it drawn`), check it's gone, and F8 again: the
blades stay drawn (`nothing to put back`), and their heat comes back (`… effects started again`). If the heat or the
crosshair stays in build mode, note which, and what the log's counts say (the spec's risks 19 and 20).

- [x] **Step 6: In the game: the opening check, where hits start**

Send `npcs`, and pick a ganger near V by its number `N`. Send `hit N 1`, with V holding the blades from Step 5.
Expected: `hit on NPC N (…) for 1.00 of Minecraft's damage, knockback 0.40; unarmed: starting at 104.87 = V's punch
104.87 x 1.00^0.50; queued` (with `, without V's weapon drawn` before `; queued` if V holds the gun), then its landing. Then ask the user to press F8 and hit a ganger once with the diamond sword (slot 1) at full cooldown, without a
jump. Expected: `…; blade: starting at 277.46 = V's punch 104.87 x 7.00^0.50; queued`, and a landing that doesn't kill
a ganger at full health (594 or more).

- [x] **Step 7: In the game: fights**

Ask the user to fight the gangers in build mode with the sword, the axe (slot 2), the bow (slot 3, arrows in 4) and the
planks in slot 5 as fists, with jump attacks and sprint hits. Expected, for a ganger of 594 health: two to three sword
hits, two axe hits, and four to six punches, about half as many again for one of 891; `landed` lines with what the
pipeline did to each: `critical` now and then, and, now that NPCs
live long enough, `blocked`, `parried` or `armour`, and some `dealt nothing: dodged`; NPCs flinch, stagger on crits and
sprint hits, take cover, fight back, and die, the last landing `killed it`. Ask the user to watch for skill XP
(Reflexes, for the blades) as hits land. Minecraft's hearts drop as they hit back.

- [x] **Step 8: In the game: the hit power by feel**

Send `set hitpower 0.75`. Expected: `set hitpower 0.75: a hit starts at V's punch x Minecraft's damage^0.75: a fist (1)
one punch, a diamond sword (7) 4.30 punches`, and the status's `hit power 0.75`. Ask the user to fight a while, and say
how it feels against 0.5. Then send `set hitpower 0.5`, or the value the user prefers, and note it.

- [x] **Step 9: In the game: combat off, and unlinking**

Send `set combat 0`, then ask the user to click Unlink. Expected: `combat=off: Minecraft's hits are logged, not applied;
V's health is V's`; then `none listed: not linked`.

- [x] **Step 10: Ask the user to quit the game and stop Minecraft**

- [x] **Step 11: Read the log once more**

```bash
L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); grep -E "module:|combat:|build: V's|hit on NPC|punch:|hitpower|Minecraft's player died" "/e/Cyberpunk2077/red4ext/logs/$L" | head -300
```
Everything the steps above logged, for the results.

- [x] **Step 12: Record the results in `docs/spikes/2026-10-05-npcs-and-combat-results.md`**

A "Step 4's fixes" section: each check above with its outcome and numbers: V's Immortal by the game's word, in and out
of build mode; the crosshair and the heat, with the log's counts and what the user saw; where hits started, and what the
pipeline dealt; the fights (hits a kill by weapon, flags, endings, reactions, staggers, XP, return fire); the hit power
by feel, and the value kept; and anything that surprised, and what it means for Part 3. Update the spike's answer at the
top.

- [x] **Step 13: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-05-npcs-and-combat-results.md docs/superpowers/plans/2026-10-05-npcs-and-combat.md \
  && git commit -q -m "npcs-and-combat: step 4's fixes in the game" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 37: Close Part 2, and hand off Part 3's planning

The user chose to plan Part 3 in a new session, so this task's second step, which was to plan it, writes a handoff for
planning it instead.

- [x] **Step 1: Add "How Part 2 went" to this plan**

At the end, after "Step 4's fixes": what ran as written, what changed, and what steps 3 and 4 and the fixes' check
found.

- [x] **Step 2: Write a handoff for planning Part 3**

`docs/superpowers/handoffs/2026-10-06-spike-6-part-3-planning.md`, as spike 4's handoff for planning its Part 3 was:
what Part 3 builds (the spec's "Part 3", and step 5), what Parts 1 and 2 left for it, the game and Minecraft as they
are, how Parts 1 and 2 were planned and verified, and the rules to keep. Part 3's tasks start at Task 38, and its
planning uses superpowers:writing-plans and verifies them as Parts 1 and 2 were; changes to the spec found while
planning go in its "Changes while planning". A blast's hits go through the whole damage pipeline too: they start where
`Hit` marks them, so the blast's own modifiers (`ExplosionResistance`, the distance curve) apply, and their start needs
the hit power's thought, since a TNT blast is tens of Minecraft's damage. Spike 6's plan tooling goes into
`tools/plans/` with it, since the scratchpad it lived in is temporary.

- [x] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/superpowers tools/plans \
  && git commit -q -m "npcs-and-combat: Part 2 closed, with a handoff for planning Part 3 and spike 6's plan tooling" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

## Found while planning

The spec's "Changes while planning" records these too.
- **redscript compiles offline:** the game's `engine/tools/scc_lib.dll`, redscript's compiler library, has a C API that
  compiles a script folder against a given `final.redscripts`, and can turn its error popup off. `tools/check_reds.py`
  uses it on a copy of the game's untouched cache (Task 6), so the spec's risk 9 is checked before deploying. Run alone,
  `scc.exe` shows a "REDScript compilation has failed" message box on the user's screen; it did once while planning.
- **The module's system each call:** its static `Current()` finds the session's through the game's global
  `GetGameInstance()`, so the plugin needs no `GameInstance`, and a load's new system is picked up on the next call, not
  only when V is found again.
- **The order:** the tasks follow the testable steps. The records, the table and `mclink` (Tasks 1 to 3) come first, so
  step 1 (Task 4) runs against SkyCraft's real mod before anything goes into the game.
- **The yaw** is taken against 0, not the NPC's last yaw: the last yaw keeps a turning yaw from wrapping, which matters
  for Minecraft's hand sway as V turns, not for an invisible stand-in, and stands in for a vertical forward vector,
  which an NPC's never is.
- **V's punch and max health** are read in Part 1, and `npcs` logs them, so step 2 compares the punch with the
  inventory's number before Part 2 relies on it.
- **Capsules lean at most 25 degrees** from upright to be an NPC's; the overlap reads only kinematic actors.
- **Capsules are looked for again** every 2 s until found, not once: NPCs are first listed up to 80 m away, or as they
  appear, when the game may not have given them a capsule yet. At most eight a frame, those never looked for first,
  nearest first, and forgotten with their numbers.
- **The slow fields' reads are spread:** each NPC's are read four times a second by the game's sim time, its first read
  putting the next at a random point of the quarter second, so a crowd's aren't all read in one frame.
- **The comparison's search** is `GetTargetParts` with `TSF_NPC()` over the complete set, as the game's own effectors
  search for nearby NPCs, rather than the HUD's nameplate search, which keeps only quickhackable targets.
- **`Compare` returns a string** for the log, and `List` returns -1 when the module doesn't find V.
- **`mclink`:** `actor` faces south and is person-sized without a yaw and size, its name is the rest of the line, and
  `actor-remove all` empties the table.
- **Shared code:** `HitWords` moves from `mclink` to `link/Actors`, so the plugin logs hits in the same words; `Puppet`'s
  type checks move to `ScriptTypes`, so `GameScript` uses them too.
- **`npcs`** also logs where each box came from, with the capsule's lean and query filter, for step 2's opening check;
  the overlay gains a button for it.
- **`Query`'s version** is parsed from `kVersion` at compile time, with a `static_assert` on the parser.

## How Part 1 went

Part 1 ran from 10:58 to 17:01 on 2026-10-05. The results:
[2026-10-05-npcs-and-combat-results.md](../../spikes/2026-10-05-npcs-and-combat-results.md).
- **As written:** Tasks 1 to 3 and 5 to 10 ran as the plan has them, each build without a warning, and the test counts
  as listed (396 to 414).
- **Step 1 (Task 4) passed,** against SkyCraft's real mod, with two expected lines in the wrong words: Minecraft 26.3
  replies `Test passed. Count: 3`, and SkyCraft logs its stand-in count only when it adds stand-ins. Task 11 then
  counted stand-ins with `/execute` (`edf0743`).
- **Step 2 (Task 11) passed on its second run.** Its first run found three faults, each fixed as its own commit:
  - the module's static functions are global functions in the game, named `BlockRenderNpcs::Version;`, so `GameScript`
    looks them up there (`da1be78`);
  - an NPC's capsule is its controller's, found by the query filter's controller bit `0x2000`, not the narrowest
    kinematic capsule around it, which beside a vendor was a 0.06 m one (`1fa17f4`; 415 tests);
  - the game crashed when Link first called `List`: the wraps had pushed onto and erased from the module's own arrays,
    and the game calls `NPCPuppet`'s `OnGameAttached` and `OnDetach` on many threads. The list moved into BlockRender:
    the wraps pass `EntityID`s to its natives, `NpcRegistry` keeps them behind a mutex, and `List` copies them on the
    main thread. Module version 2, with a thirteenth field, `found` (`b68fe3b`; 423 tests). The first load's crash in
    the engine's allocator was most likely the same race.
- **What step 2 measured:** the wraps ran on 16 threads, 15 of them workers; the game's targeting never found an NPC the
  list lacked; the module costs 0.08 ms a frame; the plate lit for everyone who crossed it; and 20 hits from Minecraft
  came back, crits included.
- **Not as expected:** the boxes stop at the controller's top, 1.63 m, a head short (the plan expected 1.7 to 1.9 m);
  the NPC's two other upright capsules reach 1.75 and 1.76 m. Drones have no capsule, so they're person-sized from
  their position.
- **For Part 2:**
  - the box's top from the tallest upright capsule on the NPC's axis comes first, with the vendor's capsules as its
    test;
  - script callbacks run on worker threads, so the spec's risk 8 (the damage pipeline off the main thread) is likely,
    not unlikely: wraps in the pipeline hand their hits to BlockRender through natives, behind a lock, as the list's
    wraps do, and keep nothing of the module's;
  - nothing guards V's health yet: a drone took V to 63% in step 2.

## Found while planning Part 2

The spec's "Changes while planning Part 2" records these too.
- **Our hits are marked on the hit itself:** the game marks its environment's attacks with `hitFlag.None` and a source
  name (`environmentDamageSystem.swift`), and `AttackData.GetFlags()` gives every flag with its source. The module marks
  ours `hitFlag.None` under `n"BlockRender <amount>"`, and the pipeline's wraps, on whichever thread runs it, read the
  mark and the amount from the hit. So no hit is recorded by reference, as the spec first had it; BlockRender keeps its
  own list of hits queued, only to count those the game hasn't dealt within 2 s.
- **The attack record:** the `BaseFists` item's `WeaponItem_Record.Attacks()`, its `StrongMelee` attack for a strong hit
  and its `Melee` attack for the rest: `QuickMelee` is a gun's quick melee (`attackData.swift`: `IsLightMelee` is
  `Melee`). The player's own melee finds its records by name on the drawn weapon object (`meleeTransitions.swift`'s
  `GetAttackRecord`). With neither, the first melee attack; with none, `Hit` says so (-3), and `punch` names the records.
- **The weapon object:** the item in `AttachmentSlots.WeaponRight`, if it's the `BaseFists` item.
- **The wrapped methods:** `damageSystem.swift`'s `private final func Process(hitEvent: ref<gameHitEvent>, cache:
  ref<CacheData>)` and `private final func DealDamages(hitEvent: ref<gameHitEvent>)`. A wrap of a method that isn't
  there fails the compile, which Task 18 checks.
- **The damage set exactly:** `gameAttackComputed.GetAttackValues()`, each set to 0 and put back with `SetAttackValues`,
  then `SetAttackValue(amount, gamedataDamageType.Physical)`.
- **V's hurts through a native into an inbox behind a lock,** taken only while combat and build mode are on, V is found
  and V's max health is read, at most 256 waiting: one it doesn't take stays V's, so none is lost on its way. Our hits'
  landings come back the same way, with the NPC's health before and after.
- **The knockback threshold compares floats,** `> 0.45f`, as SkyCraft's own `push > 0.45f`: a float knockback is never
  the double 0.45 exactly, so a double comparison couldn't tell `>` from `>=`, and its test couldn't either (a mutation
  found it).
- **`hit <id> <damage> [strong]`** is an event as Minecraft's would be: knockback 0.4, and the crit flag for strong.
- **V's god mode** goes off once whenever V is found again, the first time too, when the game says there was nothing to
  remove.
- **Step 3's checks** read SkyCraft's own line for each hurt (`SkyCombat.hurtPlayer`): `SkyCraft: Skyrim hit the player
  for <damage> (<Minecraft damage> Minecraft): health <before> -> <after>`. Its script takes Minecraft's player out of
  the armour before the burst, so the burst's two sends are exactly 1 and 4.
- **The module is version 3:** 8 functions, 13 fields, and six natives.

## The deep review of Part 2

On 2026-10-05, after Tasks 13 to 22 had run (to `eeef938`) and before step 4, the user asked for a deep review of Part
2, naming one hole: V's damage is changed by much the plan didn't count. The spec's "Changes from the deep review of
Part 2" says what was found and decided, and what was checked in the game's scripts; this records what it did to the
plan.

### What it found, most serious first

1. **Task 18's `DealDamages` wrap threw away everything the game adds to V's punch.** It set our hit to exactly
   Minecraft's damage × V's punch after the whole pipeline. Read in `damageSystem.swift` and `damageManager.swift`, that
   pipeline gives V's punch its spread (× 0.9 to 1.1), V's crits (V's and the weapon's `CritChance` and `CritDamage`),
   `AllDamageDonePercentBonus`, `MeleeDamagePercentBonus`, bonuses against enemies at full health, below half and
   burning, against elites, rares, bosses and mechanicals, Berserk's melee bonus, the stealth bonus, story mode's
   scaling, and every perk hooked in as a `ModifyDamageEffector`; and on the NPC's side `MeleeResistance`,
   `DamageReductionMelee`, its `Armor`, mitigation, evasion, dodges, blocks and parries. All lost; a blocked hit, which
   the game takes to 0, dealt it all; V's own crit roll still marked our hit critical. And `ApplyDamage`, after the wrap,
   still capped bosses' hits, let an overshield take its part, and could leave an NPC at 1 health, so "exactly" didn't
   hold either. (`ProcessLevelDifference` is never called in 2.31.) SkyCraft overwrites Skyrim's damage the same way,
   which is where the design came from.
2. **What V punches with:** step 2's open question, "Part 2 decides what V punches with when arm cyberware is
   installed", never reached Part 2's tasks. The game's unarmed slot draws V's arm cyberware when V has it
   (`GetMeleewareOrFistsItemID`), and holstering draws nothing: an attack while holstered draws the last weapon. With
   V's Mantis Blades, Task 23's Step 4 (as it was) couldn't have found the fists drawn.
3. **The hit's weapon object** was there only if the fists were drawn: never, for this V. The NPC's armour, the
   weapon's crits, the stealth bonus, blocks, the skill XP from damage and weapon perks all skip a hit without one.
4. **Task 18's `Process` wrap let dodged and evaded hits land:** `ProcessDodge`, `ProcessEvasion` and
   `ProcessInstantKill` set `DealNoDamage` after `no_valid_damage`, and `AddFlag` keeps one entry a flag, so on our hits,
   which had no damage of their own, their reason never went in, and the wrap's removal let the hit through. The spec had
   said every other reason came first.
5. **V's hurts:** a hit the game won't let kill V (`CannotKillPlayer`, a minimum health) could kill Minecraft's player,
   and so V; V's overshield never took its part.
6. **Step 4's checks:** the punch check compared the game's damage number, after every modifier, with the inventory's,
   before any; the hit's check expected the NPC's health to fall by exactly the amount, at once.

### What the user decided

The game's whole pipeline for our hits, as for V's own punches; V's unarmed slot for what V punches with; V's unarmed
weapon drawn automatically in build mode with combat on, and, at the user's request, V hidden in build mode, which the
review found possible (Codeware's `GetComponents`, the game's `IComponent.Toggle`); and V's crit chance rolling on top
of Minecraft's crits.

### What changed in the plan

- **New tasks, 23 to 30,** after Task 22, which had run: the plain C++ of what the pipeline did to our hits, V's
  non-lethal hurts and V's switches (23); the inbox's endings and the natives (24); `check_reds.py` with Codeware's
  scripts (25); the module, version 4 (26); `GameScript` (27); the status (28); the plugin's frame (29); the overlay
  (30). They replace Task 18's `Process` wrap and exact amount; Tasks 13 to 22 stay as they ran.
- **Step 4, now Task 31,** opens by checking what the review left to the game: what V punches with against the game's
  slot; build mode drawing V's unarmed weapon and hiding V; `hit N 1` against V's own strike with the same weapon on the
  same NPC; and whether the NPC's health shows the change at once. It gains a step for combat off in build mode.
- **The close is Task 32,** and Part 3 starts at Task 33: its blast hits go through the whole pipeline too.
- **BlockRender stays 0.18.0,** never deployed; the module is version 4.

### What was checked

- Every claim above was read in its source: the decompiled 2.31 scripts (`damageSystem.swift`, `damageManager.swift`,
  `statPoolsManager.swift`, `attackData.swift`, `damageSystemHelper.swift`, `equipmentSystem.swift`,
  `upperBodyTransitions.swift`, `statusEffectSystem.swift`, `rpgManager.swift`, `modifyDamageEffector.swift`,
  `genericHitPrereq.swift` and its weapon conditions, `animationSystemForcedVisibilityManager.swift`, `orphans.swift`),
  read from the raw files; the RED4ext SDK's mesh classes; Codeware's installed scripts; and SkyCraft's `ApplyHit`.
- The installed redscript's `@if(ModuleExists("Codeware"))`, by compiling a probe both ways, with a broken line in its
  Codeware branch.
- Step 4 walked through with V as step 2 found it, and each expected log line checked against the code that writes it.
- Tasks 23 to 30 as "Before you start" says: written and tested in a scratch clone, 22 mutations, then replayed from
  this text in a fresh clone, every expectation met and the files identical.

### Left to step 4

Whether the start set at `ProcessPipeline`'s entry holds; whether the equipment system draws V's unarmed weapon while
build mode places V; whether anything of V stays on screen when hidden, a shadow included; whether the Mantis Blades
are what V's unarmed slot draws; and whether an NPC's health shows a hit's change at once.

## Step 4's fixes

Step 4 (Task 31) ran on 2026-10-05 and 06 and passed every check, settling all the review had left to it (the results'
"Step 4"). It found three things that didn't work as the spec meant, and the user chose how to fix them; the spec's
"Changes from step 4" says what and why. This records what it did to the plan.

### What step 4 found

1. **The damage was overkill:** each point of Minecraft's damage was a whole blade swing, so swords, axes and arrows
   one-shot level-47 gangers of 594 to 891 health, while fists, a point each, took four or five hits.
2. **Our Immortal never took effect:** V carries `Mortal` from three sources, which outrank an `Immortal` added as a god
   mode, and the damage pipeline asks `HasGodMode`. `RemoveGodMode` returns false even when it removes, so the log's
   "the game says it already was" was only its wording.
3. **Hiding V left two things on screen:** the game's crosshair for the blades, and the Thermal blades' heat effect,
   which keeps running when its spawner is switched off.

### What the user decided

The square root for where hits start, with `set hitpower` to change it while the game runs; only the game's crosshair
hidden, not the rest of its HUD; and all three fixes made before Part 2 closes, then the fights again in the game.

### What changed in the plan

- **New tasks, 32 to 36,** after Task 31, which had run: where hits start, by the hit power (32); the module, version 5,
  with V's Immortal through the game's override and hiding V's effects and the game's crosshair (33); the status (34);
  `GameScript` and BlockRender 0.18.1 (35); and the fixes in the game, with the fights again (36).
- **The close is Task 37,** and Part 3 starts at Task 38: its blasts' hits need the hit power's thought too.

### What was checked

- The game's god modes through CET's console during step 4, and in the decompiled scripts `GodModeSystem`'s natives,
  `GetImmortality` and Second Heart's override; the crosshair container and the effect events, and how the game uses
  them; Codeware's `inkSystem` and effect spawner declarations; the SDK's effect classes. The spec's "Changes from step
  4" lists each with its source.
- Tasks 32 to 35 as "Before you start" says: written and tested in the scratch clone, 9 mutations, the module compiled
  both ways with a slip that fails only with Codeware's scripts, then replayed from this text in a fresh clone, every
  expectation met and the files identical.

### Left to the game

Whether the override makes V Immortal at once, and how it sits with Second Heart's own; whether the heat is one of the
blades' spawners' effects, and whether a kill reaches a spawner that's then switched off; and whether the crosshair
container is among the HUD layer's game controllers. Task 36 opens with all three.

## How Part 2 went

Part 2 ran from 18:11 on 2026-10-05, when it was planned, to 02:23 on 2026-10-06. The results:
[2026-10-05-npcs-and-combat-results.md](../../spikes/2026-10-05-npcs-and-combat-results.md), steps 3 and 4 and "Step
4's fixes".
- **As written:** every task ran as the plan has it, each build without a warning and the test counts as listed (423
  to 440):
  - Tasks 13 to 15 and 17 to 22, `8466cec` to `eeef938`;
  - the deep review's Tasks 23 to 30, `454c003` to `f5214b7`;
  - step 4's fixes, Tasks 32 to 35, `cfa03ae` to `0e23764`.

  Tasks 23 to 30 and 32 to 35 were executed by `run_plan.py`, which checked every expected result, and each of their
  commits holds exactly the code tested before in the scratch clone.
- **What changed:**
  - the deep review of Part 2, after Task 22 and before step 4, replaced Task 18's `Process` wrap and exact amount with
    the game's whole damage pipeline: Tasks 23 to 30, and step 4 rewritten as Task 31 ("The deep review of Part 2");
  - step 4 passed with three fixes to make, made as Tasks 32 to 35 and checked in the game as Task 36 ("Step 4's
    fixes");
  - Task 37's second step became a handoff for planning Part 3 in a new session, at the user's choice.
- **Step 3 (Task 16) passed,** against SkyCraft's real mod: each hurt reached Minecraft's player as the share sent, and
  the sums landed whole. Two expected numbers were off, for the player's world: it already wore 4 points of armour, and
  it healed half a point between two hurts.
- **Step 4 (Task 31) passed,** and settled all the review had left to it: the start set at `ProcessPipeline`'s entry
  holds; V's unarmed slot draws the Mantis Blades, and the game draws them while build mode places V; hiding V leaves no
  mesh, nor V's shadow; an NPC's health shows a hit's change only after `DealDamages`. It found three things to fix:
  each point of Minecraft's damage was a whole blade swing, so weapons one-shot gangers; our Immortal never took
  effect; and the game's crosshair and the Thermal blades' heat stayed on screen.
- **The fixes' check (Task 36):**
  - the square root keeps a fist at one punch and makes a diamond sword 2.65; the user found it "perfect", and 0.75 far
    too strong, so 0.5 stays the default;
  - the crosshair and the heat go with V; the heat isn't restarted when build mode ends, so blades V held before it
    come back without their heat until they're drawn again;
  - our Immortal can't work for the user's V: three Mortal god modes that quests left behind outrank it, and the game
    refuses even the override Second Heart uses. The damage wrap is V's only guard in build mode, as the spec's risk 18
    foresaw, and it held for all 84 hits on V there, over steps 4 and its fixes.
- **Not as expected:**
  - **the game puts V's blades away** after a while idle, about every 68 s in build mode; `Arm` draws them again within
    a second (the user found the cause);
  - **no NPC ever blocked, parried, dodged or cut a hit with its armour** in the 53 hits of steps 4 and the fixes, all
    on 6th Street gangers;
  - **hits take a moment, sometimes:** a strong hit once landed 1.4 s after it was queued.
- **For Part 3:**
  - a blast's hits start where `Hit` marks them, and the hit power applies to them too; a TNT blast is tens of
    Minecraft's damage;
  - the user asked for two follow-ups, to come after Part 2 or with Part 3 as the user chooses: Minecraft's health set
    to V's share when build mode starts, and bushes kept out of the city's collision for Minecraft (the results' "Asked
    for, after Part 2").
