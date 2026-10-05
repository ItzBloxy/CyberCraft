# NPCs and Combat Implementation Plan, Part 1 (steps 1 and 2)

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

**Parts 2 and 3:** each planned after the step before it, as the spec says. Task 12 closes Part 1 and plans Part 2.

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
- **Test counts:** 396 at the start. Then 403, 406 and 408 after Tasks 1 to 3, 412 after Task 5, and 414 after Task 8.
  Tasks 6, 7, 9 and 10 add none.
- **What only Minecraft and the game can test:** the actor table against SkyCraft's real mod in Task 4, and the module
  and the NPCs in the game in Task 11, both with the user.
- **The user:**
  - **does everything in Minecraft, the Modrinth App and the game:** starting and stopping them, loading, moving,
    keys, clicks and screenshots. Never drive their mouse or keyboard: ask, then wait for them to say they're done;
  - **approves downloads first.** Nothing in Part 1 downloads anything.
- **The game:** builds deploy only with `CP2077_DIR` set, which only Task 11's deploy step sets, with the game closed,
  and clears straight after. `mclink` needs the game closed too, since the game would hold the mapping.
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

- [ ] **Step 4: Ask the user to stop Minecraft**

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

- [ ] **Step 1: Write the failing test, `tests/TestCharacters.cpp`**

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

- [ ] **Step 2: Edit `CMakeLists.txt` to add the test**

```cmake
	tests/TestActors.cpp
	tools/mclink/DumpObj.cpp
```
```cmake
	tests/TestActors.cpp
	tests/TestCharacters.cpp
	tools/mclink/DumpObj.cpp
```

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C1083: Cannot open include file: 'physx/Characters.h'`.

- [ ] **Step 4: Write `src/physx/Characters.h`**

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

- [ ] **Step 5: Write `src/physx/Characters.cpp`**

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

- [ ] **Step 6: Edit `src/link/Actors.h`**

```cpp
	void Forget(double aNow);
```
```cpp
	void Forget(double aNow);
	// aEntity's number, if it has one, without marking it listed.
	std::optional<uint32_t> IdOf(uint64_t aEntity) const;
```

- [ ] **Step 7: Edit `src/link/Actors.cpp`**

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

- [ ] **Step 8: Edit `src/physx/Export.h`**

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

- [ ] **Step 9: Edit `src/physx/Export.cpp`**

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

- [ ] **Step 10: Edit `CMakeLists.txt` to add the source**

```cmake
	src/physx/BodyPlan.cpp
	src/link/Drive.cpp
```
```cmake
	src/physx/BodyPlan.cpp
	src/physx/Characters.cpp
	src/link/Drive.cpp
```

- [ ] **Step 11: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `412 test(s), 0 failure(s)`.

- [ ] **Step 12: Commit**

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

- [ ] **Step 1: Write `tools/check_reds.py`**

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

- [ ] **Step 2: Write `scripts/BlockRender.reds`**

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

- [ ] **Step 3: Compile the module**

The game must be installed at `E:\Cyberpunk2077`; it needn't run.
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && uv run --no-project python tools/check_reds.py E:/Cyberpunk2077 scripts; \
  echo "exit: $?"
```
Expected: redscript's `Compilation complete`, then `compiled` and `exit: 0`.

- [ ] **Step 4: Check that the compile catches a slip**

A copy of the module calling a function `StatPoolsSystem` doesn't have:
```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && mkdir -p build/reds-slip \
  && sed 's/GetStatPoolMaxPointValue(stats/GetStatPoolMaxPointValueX(stats/' scripts/BlockRender.reds > build/reds-slip/BlockRender.reds \
  && uv run --no-project python tools/check_reds.py E:/Cyberpunk2077 build/reds-slip; echo "exit: $?"
```
Expected: `method 'GetStatPoolMaxPointValueX' not found on 'StatPoolsSystem'`, then `failed` and `exit: 1`.

- [ ] **Step 5: Commit**

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

- [ ] **Step 1: Write `src/ScriptTypes.h`**

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

- [ ] **Step 2: Write `src/ScriptTypes.cpp`**

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

- [ ] **Step 3: Edit `src/Puppet.cpp`: the checks move to `ScriptTypes`**

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

- [ ] **Step 4: Write `src/GameScript.h`**

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

- [ ] **Step 5: Write `src/GameScript.cpp`**

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

- [ ] **Step 6: Edit `CMakeLists.txt` to add the sources to the plugin**

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

- [ ] **Step 7: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `412 test(s), 0 failure(s)`.

- [ ] **Step 8: Commit**

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

- [ ] **Step 1: Edit `tests/TestRequests.cpp`: the failing test**

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

- [ ] **Step 2: Edit `tests/TestStatus.cpp`: the failing test**

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

- [ ] **Step 3: Build; it must fail**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C" | sort -u | head -3
```
Expected: `error C2065: 'Npcs': undeclared identifier`.

- [ ] **Step 4: Edit `src/Requests.h`**

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

- [ ] **Step 5: Edit `src/Requests.cpp`**

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

- [ ] **Step 6: Edit `src/Status.h`**

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

- [ ] **Step 7: Edit `src/Status.cpp`**

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

- [ ] **Step 8: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `414 test(s), 0 failure(s)`.

- [ ] **Step 9: Commit**

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

- [ ] **Step 1: Edit `src/Plugin.cpp`**

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

- [ ] **Step 2: Edit `CMakeLists.txt`: version 0.17.0, and the module installed**

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

- [ ] **Step 3: Build and run the tests; they must pass**

```bash
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
cd /d/Cyberpunk-Minecraft/spikes/block-render && "$CMAKE" --preset default > /dev/null \
  && "$CMAKE" --build --preset release -- -m -v:minimal 2>&1 | grep -E "error C|warning C" | sort -u; \
  ./build/RelWithDebInfo/render_tests.exe | tail -1
```
Expected: no error or warning lines, then `414 test(s), 0 failure(s)`.

- [ ] **Step 4: Check that BlockRender imports nothing from PhysX**

```bash
DUMPBIN="$(ls "/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/"*/bin/Hostx64/x64/dumpbin.exe | head -1)"
cd /d/Cyberpunk-Minecraft/spikes/block-render && MSYS_NO_PATHCONV=1 "$DUMPBIN" -imports -nologo build\\RelWithDebInfo\\BlockRender.dll \
  | grep -ci "physx"
```
Expected: `0`.

- [ ] **Step 5: Commit**

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

- [ ] **Step 1: Edit `tools/blockrender_hud/init.lua`**

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

- [ ] **Step 2: Check the overlay's syntax**

```bash
cd /d/Cyberpunk-Minecraft/spikes/block-render && luac -p tools/blockrender_hud/init.lua && echo ok
```
Expected: `ok`.

- [ ] **Step 3: Commit**

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

- [ ] **Step 1: Deploy (the game must not be running)**

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

- [ ] **Step 2: In the game: the module compiles**

Ask the user to start the `CyberCraft` instance, then the game, and to wait at the game's main menu. If a "REDScript
compilation has failed" message box shows instead, ask the user to click OK and quit the game, and read redscript's log
for why: it names the file and line. Otherwise:
```bash
grep -E "BlockRender|Compilation complete|ERROR" /e/Cyberpunk2077/r6/logs/redscript_rCURRENT.log
```
Expected: the module's path, `…\red4ext\plugins\BlockRender\Scripts\BlockRender.reds`, among the files compiled, then
`Compilation complete`, and no `ERROR` line. In BlockRender's log: `BlockRender 0.17.0 loaded`, and `the redscript
module E:\Cyberpunk2077\red4ext\plugins\BlockRender\Scripts\BlockRender.reds: added to redscript's compile`.

- [ ] **Step 3: In the game: load, link, and the module found**

Ask the user to load the free-roam save, go to a pavement where many people walk, stand still at its edge, make a new
save there, and click the overlay's Link button. Expected:
- in the log, `module: found BlockRenderNpcs version 1, its 4 functions, and the 12 fields List fills`, then `link:
  Minecraft is there (process …)`;
- in the status, `npcs=on: … listed of … in the world, the nearest … at … m; capsules found for … of … looked for; …
  numbers given; the module … ms average, … ms worst a frame; 0 hits from Minecraft logged`, and the overlay's `NPCs:`
  line;
- counting the stand-ins (above): `[CHAT] Test passed. Count: …`, about as many as the status lists.

A `module: …` line saying something is missing stops the step: the class missing means redscript didn't compile the
module (Step 2); `the game session has no BlockRenderNpcs system` means the game doesn't make a mod's scriptable system
(the spec's risk 1), and a new task moves the list to a global function, found as `Puppet` finds `GetMountedVehicle`.

- [ ] **Step 4: In the game: the opening check, the list against the game's targeting**

Still in the crowd, send `npcs`, or ask the user to click "List them in the log" under "More controls". Expected, in the
log:
- `npcs: … listed of … in the world; V's punch …, read from Items.w_melee_004__fists_a; V's max health …`: ask the user
  to open the inventory, find the fists (or the Gorilla Arms, if V has them) and read their damage number, and compare;
- a line per listed NPC: its number, name and distance, its box and where it came from, its health, level and flags;
- `npcs: GetTargetParts finds … NPCs within 80 m; the list lacks …`: 0, or a few that have just come within 80 m. If it
  lacks many, the wraps miss NPCs (the spec's risk 2): note which, and a new task makes the list from `GetTargetParts`.

- [ ] **Step 5: In the game: the opening check, which capsule is an NPC's**

Ask the user to walk up to an NPC standing still (a vendor, or someone waiting), to stand about a metre from them, and
to say when they're there. Send `shapes`, then `npcs`. Expected:
- `shapes:` lines for every shape within 2 m of V's middle: V's two kinematic capsules (radius 0.32, query filter
  `0:0:2000:0`, and radius 1), and the NPC's, a kinematic capsule with its own filters and bounds;
- that NPC's `npcs:` line: its box `from its capsule's: radius …, half-height …, leaning 0 degrees, query filter …`,
  about 0.6 to 0.8 wide, from 0.00 below its position to about 1.7 to 1.9 above.

If most NPCs say `a person's: no capsule found`, or the capsule taken isn't the one `shapes` shows around the NPC, the
rule changes to what `shapes` shows (its filter, its kind, its size), as a new task.

- [ ] **Step 6: In the game: the opening check, what the list costs**

In the crowd, read the status's NPC line for 10 s: the module's time a frame, on average and at worst. Then ask the user
to click the overlay's `NPCs: on` button, so it reads `NPCs: off`, for 10 s, then on again. Read the status's `frame=`
line and the overlay's CET frame time each time, and count the stand-ins (above) while it's off and once it's on again.
Expected: the NPC line says `npcs=off` while off, and the count says `[CHAT] Test failed`; once it's on again, the count
is back.

- [ ] **Step 7: In the game: a plate on the pavement**

Send `mc creative`, `mc /item replace entity @s hotbar.0 with minecraft:stone_pressure_plate 4`, `mc /item replace
entity @s hotbar.1 with minecraft:redstone_lamp 4`, `mc /item replace entity @s hotbar.2 with minecraft:diamond_sword`
and `mc /item replace entity @s hotbar.3 with minecraft:stone 16`, each once the last shows in the status's `request=`
line: the plate is on 1, the lamp on 2, the sword on 3 and stone on 4. Ask the user to press F8, press 1 and place the
plate with a right click on the pavement where people walk, then press 2 and place the lamp right beside it; to press
F8 again and step back a few metres, so Minecraft's player, at V, isn't on the plate; and to watch as people cross it.
Expected: the lamp lights while someone is on the plate, and goes out after they've left it. Note how often it misses
someone crossing, and anyone it lights for who isn't on it.

- [ ] **Step 8: In the game: hits, logged**

Ask the user to walk up to an NPC who is standing still, press F8, press 3 for the sword, and hit the NPC two or three
times, a second or more apart, aiming at their body, then press F8 again. Expected: in the log, `hit on NPC … (…) for
7.00 of Minecraft's damage, knockback …; blade; not applied in Part 1` for each, with the NPC's name; the NPC doesn't
react in the game; the status's hits count goes up by as many; and Minecraft's log says `SkyCraft: hit … for 7.0 …`.

- [ ] **Step 9: In the game: sizes beside a drone, or a mech**

If a drone or a mech can be reached without a fight (some corporations' and gangs' grounds keep one standing), ask the
user to stand near it, then send `npcs`. Note its line: its box and where it came from. If none can be, skip this step
and say so in the results: Part 2's fights will meet them.

- [ ] **Step 10: In the game: the overlay, build mode and the bodies**

Ask the user to press F8, press 4 for stone and place a few blocks of it on the pavement, walk onto them in build mode,
then leave build mode. Expected: build mode, the drive and the bodies work as in spike 5: the bodies line counts them,
and V stands on them.

- [ ] **Step 11: In the game: unlinking empties the table**

Ask the user to click the overlay's Unlink button, then count the stand-ins (above). Expected: the NPC line says `none
listed: not linked`, and the count says `[CHAT] Test failed`: the heartbeat goes on while unlinked, so SkyCraft reads
the emptied table, and the typed command too.

- [ ] **Step 12: Ask the user to quit the game and stop Minecraft**

- [ ] **Step 13: Read the log once more**

```bash
L=$(ls -t /e/Cyberpunk2077/red4ext/logs | grep -i blockrender | head -1); grep -E "module:|npcs:|hit on NPC|shapes:" "/e/Cyberpunk2077/red4ext/logs/$L" | head -200
```
Everything the steps above logged, for the results.

- [ ] **Step 14: Record step 2's results in `docs/spikes/2026-10-05-npcs-and-combat-results.md`**

A "Step 2" section: each check above with its outcome and numbers: redscript's compile and the module's checks; the
list against the targeting; which capsule is an NPC's, with its filters and box; V's punch against the inventory; the
module's time and the frame rates with NPCs on and off; the plate; the hits; the drone or mech, if any; and anything that
surprised, and what it means for Part 2. Put the spike's answer so far at the top.

- [ ] **Step 15: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/spikes/2026-10-05-npcs-and-combat-results.md \
  && git commit -q -m "npcs-and-combat: step 2's results" \
       -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" && git log --oneline -1
```

---

### Task 12: Close Part 1, and plan Part 2

- [ ] **Step 1: Add "How Part 1 went" to this plan**

At the end, after "Found while planning": what ran as written, what changed, and what steps 1 and 2 found.

- [ ] **Step 2: Plan Part 2**

Use superpowers:writing-plans to add Part 2's tasks, from Task 13 on, to this plan: combat both ways (the spec's "Part
2", and steps 3 and 4). Verify them as Part 1's were. Changes to the spec found while planning go in its "Changes while
planning".

- [ ] **Step 3: Commit**

```bash
cd /d/Cyberpunk-Minecraft && git add docs/superpowers \
  && git commit -q -m "npcs-and-combat plan: Part 1 closed, Part 2 planned" \
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
