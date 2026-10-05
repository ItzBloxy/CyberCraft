# NPCs and combat: results

Spike 6 brings Night City's NPCs into Minecraft as SkyCraft's stand-ins (Part 1), then combat both ways (Part 2), then
Minecraft's explosions in the game (Part 3). Step 1 ran SkyCraft's actor table with `mclink`, without Cyberpunk; later
steps add their sections as they run, and the spike's answer goes at the top once step 2 is in.

## Step 1: made-up NPCs in SkyCraft's actor table, with `mclink`, without Cyberpunk

Date: 2026-10-05. No Cyberpunk: `mclink`, BlockRender's link code in a console program, ran against the user's
Minecraft with SkyCraft's unchanged mod. The versions:
- **Minecraft 26.3**, Fabric Loader 0.19.5, Fabric API 0.161.0+26.3 and Java 25, in the Modrinth App's `CyberCraft`
  instance, with `--enable-native-access=ALL-UNNAMED -Dskycraft.quitWithSkyrim=false`, and CyberCraft Tweaks 1.2.0;
- **SkyCraft 0.1.2**, the release jar, protocol version 11;
- **BlockRender's link code and `mclink`** at `984187c`: the plan's Tasks 1 to 3, built RelWithDebInfo, 408 unit tests
  passing.

The user started and stopped Minecraft. The agent ran `mclink` with step 1's script, `npcs.txt`, and read its output and
Minecraft's log. Plan: [2026-10-05-npcs-and-combat.md](../superpowers/plans/2026-10-05-npcs-and-combat.md), Task 4.

### Verdict

**Step 1: pass.** The actor table works against SkyCraft's real mod as written: nothing in the code changed after
running it, and every check passed. Two of the plan's expectations had the wrong words, not the wrong behaviour:
- Minecraft 26.3's reply to a passing `/execute if entity` reads `Test passed. Count: 3`, not `Test passed, count: 3`;
- SkyCraft logs its stand-in count only when stand-ins are added, never when they're removed, so the plan's expected
  `2` and `0 Skyrim actors mirrored as hittable stand-ins` lines don't exist. `SkyCombat.sync` removes stale stand-ins
  before it counts them (SkyCraft 0.1.2's code). The `/execute` counts showed both removals.

### The checks

- **Linking:** `Minecraft is there` 0.35 s after `mclink` started, then `floor 1 at height 6.14 around Minecraft 520.5
  2007.5` at 1.94 s. Minecraft was already in its world.
- **Stand-ins:** the three records went in at 5.05 to 5.08 s. SkyCraft logged `1` and then `3 Skyrim actors mirrored as
  hittable stand-ins` (its server tick saw the first record before the other two), and `/execute if entity
  @e[type=skycraft:skyrim_actor]` replied `Test passed. Count: 3`.
- **Hits:** in build mode, looking 180 20 at actor 1, 2 blocks north of the stand-in V:
  - a left click with an empty hand came back as `hit: actor 1 for 1.00, knockback 0.40 towards x -0.00 z -1.00;
    unarmed`, a second after the click;
  - with the diamond sword, its cooldown full after 2 s, `hit: actor 1 for 7.00, knockback 0.40 towards x -0.00 z
    -1.00; blade`;
  - SkyCraft logged `hit Ganger One for 1.0 (knockback 0.4)` and `hit Ganger One for 7.0 (knockback 0.4)`: the
    stand-in carries the record's name, and the knockback pushes it away from the player, north.
- **The plate:** the stone block, the stone pressure plate on it and the redstone lamp beside the plate were set by
  command, and Minecraft sent the section back after each. The lamp's three checks all passed: out at first; lit 2 s
  after actor 1 moved onto the plate, its feet on the block's top at y 7; out again 3 s after it moved back. SkyCraft's
  stand-in pressed the plate through its whole-block check, as the spec read.
- **Removal:** `actor-remove 2` left 2 records, and `/execute if entity` counted 2. `actor-remove all` emptied the table
  (`0 actors in the table`), and `/execute unless entity @e[type=skycraft:skyrim_actor]` passed: SkyCraft removed every
  stand-in once the table was empty.
- **The status:** `events: 0 explosions, 2 hits on actors, 0 deaths, 0 others; 0 actors in the table; 0 inputs dropped;
  …`.
- **No `Test failed`** in Minecraft's log, and `exit: 0`.

The render ring carried 21,264 atlas patches (197 MB, Minecraft's animated textures, as in spike 5), 7 sections, 1,519
HUD images and 26 `ragdoll` messages (SkyCraft's model of the player's body, sent about once a second while it's alive),
with no resyncs and nothing rejected.

### What it means for step 2

- **The table, the stand-ins and their hits work as SkyCraft's own game side's do,** so step 2 changes only where the
  records come from: the game's NPCs, through the module.
- **Minecraft's damage arrives exactly:** 1 for a fist and 7 for a diamond sword at full cooldown, so Part 2's amount,
  Minecraft's damage × V's punch, starts from Minecraft's own numbers.
- **Counting stand-ins:** SkyCraft's log shows stand-ins being added, not removed, so step 2 counts them with `mc
  /execute if entity @e[type=skycraft:skyrim_actor]` instead of waiting for a log line. SkyCraft reads typed input
  whenever BlockRender's heartbeat is fresh, which it stays while unlinked, so the count works then too. The plan's
  Task 11, steps 3, 6 and 11, now says so.
- **A plate in a stand-in's block lights its lamp:** on the pavement, a plate placed in build mode sits in the block the
  pavement's surface is in, which a stand-in's box overlaps from the NPC's feet up.

## Step 2: the game's NPCs in Minecraft

Date: 2026-10-05. BlockRender 0.17.0 in the game (2.31), linked to the same Minecraft as step 1, on a pavement near
spike 3's corner. Plan: Task 11. This section is written as the step runs.

### The first run: three faults, fixed

- **redscript compiled the module** at the game's start, with the other mods' scripts (ArchiveXL, TweakXL, Codeware,
  mod_settings and those in `r6/scripts`): `…\BlockRender\Scripts\BlockRender.reds` was among the files, then
  `Compilation complete`. BlockRender's log said `the redscript module …: added to redscript's compile`.
- **A crash on the first load,** before any link: an access violation in the engine's allocator
  (`red::memory::DefaultAllocator::AllocateAligned`, reached from `red::DynamicBuffer::ResizeBuffer` while the game ticked
  its `DelaySystem`), 95 s after the game started, with 76 NPCs attached. The allocator's free list held data where a
  pointer belonged (`0x3f400000000c0002`), so something had written over freed memory earlier, off this stack. The
  second load of the same save didn't crash. BlockRender's C++ calls nothing new while unlinked; the module's wraps did
  run for each NPC attached. Read from the crash dump with a small script (no debugger is installed) and the game's
  `cyberpunk2077_addresses.json`. The link crash (below) explains it too: the wraps raced on the module's arrays, and a
  load attaches many NPCs at once; an array grown on one thread while another writes to it leaves writes in freed
  memory, which is what the allocator met.
- **The module wasn't found:** `module: BlockRenderNpcs.Version isn't there with the types BlockRender passes`. A
  throwaway CET mod listed the class through Codeware's `Reflection`: its static list is empty, and its static
  functions are global functions, `BlockRenderNpcs::Version;`, `BlockRenderNpcs::Get;GameInstance` and
  `BlockRenderNpcs::Current;`. Its member functions and all twelve fields were as `GameScript` expects. Fixed: static
  functions are looked up among the global functions (`da1be78`).
- **The module itself works,** called from that CET mod: the session's system exists (the spec's risk 1), `Version()`
  is 1, and `List(80)` gave 12 NPCs, 33 attached in all, every array 12 long: "NC Resident"s at 6 to 63 m, most at level
  47 with 449 to 1,514 health, some at level 1 with 48 or 78, none hostile. `Compare(80)` said `GetTargetParts finds 11
  NPCs within 80 m; the list lacks 0`: the wraps miss none here (risk 2).
- **V's punch** reads 102 from `Items.w_melee_004__fists_a`, and V's max health 455.6. V has Mantis Blades installed,
  which the inventory shows at 105; the fists' own number isn't shown there, so the two aren't the same item. Part 2
  decides what V punches with when arm cyberware is installed.
- **An NPC's capsule:** `shapes` beside a vendor showed three kinematic capsules standing around it: the controller,
  radius 0.30 and 1.45 m tall, query filter `0:0:10002000:0`, not a simulation shape; one of radius 0.40, `0:0:2000000:0`;
  and a thin one of radius 0.06, `0:0:24000002:0`, a simulation shape; and a kinematic sphere of radius 1.2. V's
  controller's filter, `0:0:2000:0`, shares the `0x2000` bit with the vendor's. The narrowest-first rule would have
  taken the 0.06 m capsule; the rule now takes the one with that bit (`1fa17f4`).
- **The game crashed when Link first called the module** (the second start, after the two fixes above): an access
  violation reading address 0 on the main thread, 650 s after the game started, with 74 NPCs attached
  (`Cyberpunk2077-20261005-152636-13064-29680`). BlockRender's log ended at `link: the city starts over, as collision
  epoch 2`, the line Link writes. Read from the dump:
  - **where:** the game's handle release (`+0x2f0548`), which took a strong count from 1 to 0 and then called the
    object's `CanBeDestructed` (vtable slot `0xD0`) through a null object pointer; called from the handle assignment
    (`+0x149150`) inside the script VM (`rtti::Function::InternalCall`), under RED4ext's `ExecuteFunction`, from
    `GameScript::List` (`GameScript.cpp:190`), `ListNpcs` and `OnUpdate`. So the module's `List`, on its first call.
  - **what:** `List`'s frame held the range, 80, V's position, and a local handle being assigned whose old value had no
    object but a count block with one strong reference. The VM zeroes a function's locals before it runs (its frame
    setup clears them, then constructs each), so that value came from the module's list, `known`, read by an earlier
    pass of the loop. A handle with no object and a live count can't come from the module's code run on one thread.
  - **why:** the wraps of `NPCPuppet.OnGameAttached` and `OnDetach` pushed onto and erased from `known`, and the game
    runs those callbacks on several threads at once: a Codeware issue's crash report (psiberx/cp2077-codeware#51) shows
    `OnGameAttached` and `OnTakeControl` running on other threads, and one script function on five threads at once, and
    Cyber Engine Tweaks 1.18.1 fixed crashes in very frequently called overrides, `GameObject.OnGameAttached` and
    `OnDetach` on a reload. A script array has no lock. Neither dump shows another thread inside the script VM when it
    was written: the race happened earlier, and a dump shows only the moment of the crash.
  - **the fix (`b68fe3b`):** the wraps pass only `EntityID`s to BlockRender's natives, `BlockRenderNpcAttached` and
    `BlockRenderNpcDetached`; `NpcRegistry` keeps them in attach order behind a mutex and counts the calls by thread;
    `List` copies the list on the main thread (`BlockRenderNpcCount`, `BlockRenderNpcAt`), finds each NPC again by its
    ID, and keeps what it read of each in step with the list. The module is version 2, with a thirteenth field, `found`,
    and BlockRender writes the module's check to the log before its first call. Unit tests: 423, including eight
    threads attaching and detaching while another copies the list; without the lock, that test crashes. The merge that
    keeps the module's reads in step with the list was ported to Python and checked against 2,000 random runs of
    attaches, detaches and re-attaches. The next run's log says which threads the game calls the wraps on.
