# NPCs and combat: results

Spike 6 brings Night City's NPCs into Minecraft as SkyCraft's stand-ins (Part 1), then combat both ways (Part 2), then
Minecraft's explosions in the game (Part 3). Step 1 ran SkyCraft's actor table with `mclink`, without Cyberpunk; later
steps add their sections as they run, and the spike's answer goes at the top once step 2 is in.

## The answer so far (Part 1, after step 2)

**Yes: Night City's NPCs are in Minecraft as SkyCraft's stand-ins, where the game puts them.** On a busy pavement, every
NPC within 80 m was listed, and the game's own targeting never found one the list lacked. A stone pressure plate on the
pavement lit its lamp whenever people crossed it, and missed none. Minecraft's sword hits on a vendor and on a Militech
drone came back to the game with the NPC's name and Minecraft's damage, criticals included. The list costs the game 0.08
ms a frame. It took three fixes; the last was a crash, because the game calls NPCs' attach and detach callbacks on many
threads (16 in this run), so the list now lives in BlockRender behind a lock. Open for Part 2: the boxes stop at the
shoulders (1.63 m), drones have no capsule and stay person-sized, and nothing guards V's health yet.

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

### The second run: every check passed

Date: 2026-10-05, 16:41 to 16:58. BlockRender 0.17.0 at `b68fe3b` (module version 2), the same Minecraft and SkyCraft as
step 1, the game 2.31. The user ran the game; the agent sent the requests and read the logs and the status.
- **The module and its natives (Step 2):** redscript compiled the module with the other mods' scripts (`Compilation
  complete`), and BlockRender added its four natives once the game had registered its types. No message box.
- **Which threads call the wraps:** at the main menu the wraps had already run twice, on two threads, neither the main
  thread. As the save loaded, within a second they had run on 16 threads, 15 workers and the main thread, one to four
  attaches each. By the last `npcs` request: 209 attaches and 166 detaches, all but 4 attaches and every detach on a
  worker. That
  settles the link crash's cause; this run crashed neither at the load nor at Link.
- **Found at Link (Step 3):** `module: found BlockRenderNpcs version 2, its 4 functions, the 13 fields List fills, and
  BlockRender's 4 natives`. The status listed 17 of the 37 NPCs in the world, and Minecraft counted 15 stand-ins
  (`Test passed. Count: 15`): people walk in and out of 80 m.
- **The list against the targeting (Step 4):** on the pavement, 15 listed of 35, and `GetTargetParts finds 13 NPCs
  within 80 m; the list lacks 0`. Each of the three times `npcs` ran (13, 27 and 25 found), the list lacked none. The
  module found every NPC on the wraps' list by its ID (35 of 35, later 42 of 42 and 43 of 43), and the list had no
  repeat, stray or overflow. All were "NC Resident"s: level 1 with 48 or 78 health on the pavement, level 47 with 449 to
  1,514 health a few streets on. V's punch read 102 from `Items.w_melee_004__fists_a`, and V's max health 456, as in the
  first run.
- **An NPC's capsule (Step 5):** beside the vendor (NPC 35, which the game names "NC Resident"), its box came from its
  controller: radius 0.30, half-height 0.43, query filter `0:0:10002000:0`, so 0.60 wide, from its position to 1.63 m
  above. `shapes` showed what stands on it, its feet at height 6.31:
  - the controller, a kinematic capsule from 0.17 to 1.62 m above the feet;
  - a kinematic capsule of radius 0.40, `0:0:2000000:0`, not a simulation shape, from 0.05 to 1.75 m;
  - a thin capsule of radius 0.06, `0:0:24000002:0`, a simulation shape, from 0.04 to 1.76 m;
  - a kinematic sphere of radius 1.2, `0:0:2:0`.

  A second NPC beside it had the same controller and the 0.40 capsule. So the controller stops at the shoulders, and
  the box is a head short of the 1.7 to 1.9 m the plan expected. Every one of the 27 NPCs listed there had its
  controller found. On the pavement, five of the first fifteen had none at first, one of them 1.7 m away; by the
  vendor's, 40 of the 42 looked for had been found.
- **The cost (Step 6):** with 27 listed of 42, the module took 0.08 ms a frame on average, 0.2 to 0.4 ms at worst (once
  0.79). Frames averaged 15.6 to 16.2 ms with NPCs on, off and on again, no difference beyond the crowd's own, and a
  worst frame of about 30 ms each second came with NPCs off too, so it isn't the module's. While off, the status said
  `npcs=off` and the count said `Test failed`; on again, the count was 27, as listed.
- **The plate (Step 7):** a stone pressure plate on the pavement and a redstone lamp beside it. The lamp lit whenever
  people crossed the plate, and the user saw it miss none. It also lit for people on the lamp's block, beside the plate:
  a stand-in is 0.6 m wide, so one standing within about 0.24 m of the plate's block presses it, as a Minecraft player
  would, and a stone plate stays down for a second after it's left.
- **Hits (Step 8, and the drone):** three sword hits on the vendor came back as `hit on NPC 35 (NC Resident) for 7.00 of
  Minecraft's damage, knockback 0.40; blade; not applied in Part 1`, and SkyCraft logged `hit NC Resident for 7.0
  (knockback 0.4)` for each. The vendor didn't react, as Part 1 applies nothing, and the module read no god mode on him.
  Then 17 hits on a Militech Wyvern drone: four of 7.00, with knockback 0.40 or 0.50; eleven of 10.50 `blade, critical`
  for jump attacks at full cooldown; and two of 9.21, critical with the cooldown not quite full. Twenty hits in all,
  each logged once.
- **A drone (Step 9):** NPC 130, a Militech Wyvern, level 47, 297 health, hostile and in combat. No capsule was found,
  so it got a person's box, 0.60 by 1.80, from its position up; the two other Wyverns listed had none either. The hits
  landed, so the box overlaps the drone, but its own shape wasn't seen: `shapes` said `V's controller isn't found`
  while V was in the air in build mode. Meanwhile the drone shot V down to 63% health: nothing guards V in Part 1.
- **Build mode and the bodies (Step 10):** blocks of stone on the pavement, and V walked onto them in build mode, as in
  spike 5 (154 bodies made and removed over the run).
- **Unlinking (Step 11):** `none listed: not linked`, the module not called, and the count said `Test failed`.
- **No crash:** the game quit normally; BlockRender's last line was `25 vtable entries restored`.

### What it means for Part 2

- **The list, the stand-ins and the hits work in the game,** so Part 2 starts from them: a hit's damage on the NPC
  (Minecraft's damage times V's punch), and the game's hits on V back to Minecraft.
- **The box's top:** take it from the tallest upright capsule on the NPC's axis, which beside the vendor reached 1.76 m,
  not the controller's 1.63 m, and keep the controller's width. A small change in `Characters`, with the vendor's
  capsules as its test: Part 2's first task, checked in its first run.
- **Drones have no capsule,** so they stay person-sized from their position (the spec's risk 11). Their own shape is
  still to be seen, with V standing still beside one.
- **V needs Part 2's guard:** the drone took V to 63% while V fought it in build mode.
- **Whether the vendor can be hurt** shows with Part 2's first applied hit: the module read no god mode on him.

## Step 3: Minecraft's player hurt as BlockRender will hurt it for V, with `mclink`, without the game

Date: 2026-10-05, 19:11. `mclink` at `0e95e91` (the plan's Task 15), running step 3's script, `hurts.txt`, against the
`CyberCraft` instance: Minecraft 26.3 and SkyCraft 0.1.2, as in step 1. The user started Minecraft and agreed to the
script's handling of `keep_inventory`; the agent ran the script and read both logs. Plan: Task 16.

### Verdict

**Step 3: pass.** Every hurt reached Minecraft's player as the share sent, through SkyCraft's own `hurtPlayer`, and the
sums landed whole. Two of the plan's expected numbers were off, both because of the player's world, not the code: the
player already wore 4 points of armour, which reduced the hurts that had an attacker, and it healed half a point
between two of them.

### The checks

- **Each hurt's share:** `hurt sent at once: melee 0.2500 of V's health, a = 2500, attacker 0`, and SkyCraft's
  `Skyrim hit the player for 25.0 (5.0 Minecraft): health 20.0 -> 15.0`; then `other 0.1000 …, a = 1000`, and `10.0
  (2.0 Minecraft): health 20.0 -> 18.0`. Exactly those shares of Minecraft's 20.
- **Armour:** in a diamond chestplate and leggings, a quarter, melee, from actor 1 two blocks away: `25.0 (5.0
  Minecraft): health 20.0 -> 18.266666`, about a third of it taken; then a quarter, other: `20.0 -> 15.0`, all of it.
  Armour reduces only melee and projectile hurts that have an attacker, as the spec read. The plan expected about 17.5
  left. Minecraft's formula, damage × (1 − (armour − 4 × damage ÷ (toughness + 8)) ÷ 25), gives 18.267 exactly with 18
  points and toughness 4: the diamond pieces' 14 and 4, and the 4 points the player already wore.
- **The burst:** five hurts of 0.05, a tenth of a second apart, from actor 1. The first went at once, at 24.27 s; the
  other four joined one sum, `hurt sent, summed: projectile 0.2000 …, a = 2000, attacker 1, power attack`, at 24.83 s,
  11 of Minecraft's ticks later. SkyCraft: `5.0 (1.0 Minecraft): health 20.0 -> 19.14`, then `20.0 (4.0 Minecraft):
  health 19.64 -> 15.959999`. Both landed whole, less the worn armour's 14% and 8% (the formula's exactly), since the sum
  came after the 10 ticks in which Minecraft takes only what's more than the last hurt. The half point between them was
  Minecraft's own healing, from full hunger.
- **The death:** `/kill` came back as `Minecraft's player died, killed by none of the actors`. The chat said `ItzBloxy
  was killed while fighting Ganger One`: Minecraft's message names the last attacker, while SkyCraft's event names the
  stand-in behind the killing damage, which for `/kill` is none. Minecraft's player respawned at once.
- **The status:** `events: 0 explosions, 0 hits on actors, 1 deaths, 6 others` (the others are SkyCraft's armour skill
  events, drained and counted), `0 inputs dropped`, and `exit: 0`.
- **The world's rules:** the chat said `immediate_respawn is already set to true` and `keep_inventory is already set to
  true`: the user's world had both on. The script's last `keep_inventory false` turned the second off; a short `mclink`
  run set it back on, as the world had it.

### What it means for step 4

- **The encoding and the sums work against SkyCraft's real mod,** so V's hurts land on Minecraft's player in proportion,
  and a burst of gunfire isn't lost to Minecraft's invulnerability ticks.
- **Minecraft's player's own armour counts** against hits from listed NPCs within 24 blocks, as Minecraft's armour does
  against its mobs. The player in the user's world wears some.
- **`keep_inventory` is on in the user's world,** so step 4's death drops nothing.
