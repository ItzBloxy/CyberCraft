# NPCs and combat: results

Spike 6 brings Night City's NPCs into Minecraft as SkyCraft's stand-ins (Part 1), then combat both ways (Part 2), then
Minecraft's explosions in the game (Part 3). Step 1 ran SkyCraft's actor table with `mclink`, without Cyberpunk; later
steps add their sections as they run, and the spike's answer goes at the top once step 2 is in.

## The answer (Parts 1 to 3)

**Part 1, yes: Night City's NPCs are in Minecraft as SkyCraft's stand-ins, where the game puts them.** On a busy
pavement, every NPC within 80 m was listed, and the game's own targeting never found one the list lacked. A stone
pressure plate on the pavement lit its lamp whenever people crossed it, and missed none. Minecraft's sword hits on a
vendor and on a Militech drone came back to the game with the NPC's name and Minecraft's damage, criticals included. The
list costs the game 0.08 ms a frame. It took three fixes; the last was a crash, because the game calls NPCs' attach and
detach callbacks on many threads (16 in this run), so the list now lives in BlockRender behind a lock. Since Part 2's
first task the boxes reach 1.76 m, the top of the NPC's tallest capsule; drones have no capsule and stay person-sized.

**Part 2, yes: Minecraft's weapons fight Night City's NPCs as V, and in build mode the game's hits on V go to
Minecraft's player, whose death kills V.** A Minecraft hit starts the game's own damage pipeline at V's punch × the
square root of Minecraft's damage, and the game does the rest as for V's own strike: in step 4 the first test hit, one
point of Minecraft's damage, started at V's 104.87 and dealt 168.72 with V's crit, beside V's own crit strike of about
180 on the same ganger. Since step 4's fixes a fist starts at one punch and a diamond sword at 2.65 (277.46), and a
level-47 ganger of 594 health takes two or three sword hits, two axe hits or about five punches: the user found that
"perfect", and three-quarters of the way to step 4's scale far too strong. The hits carry V's Mantis Blades, drawn for
build mode, burn the NPCs as the blades do and earn V skill XP; NPCs flinch at every hit, stagger now and then, take
cover, fight back and die. In build mode nothing of V's is on screen, the game's crosshair and the blades' heat
included, and every hit on V there went to Minecraft's hearts instead (42 in step 4, 42 again in its fixes); each time
those ran out, Minecraft's death killed V. One part can't work for the user's V: our Immortal backstop. V carries three
Mortal god modes that quests left behind, and the game refuses even the override Second Heart uses, so in build mode
the damage wrap is V's only guard; it has held for every hit.

**Part 3, yes: Minecraft's blasts reach Night City through the game's own explosion code.** In step 5a a real TNT's
hits on stand-ins came back as the formula says, each told as the blast's own. In the game, a blast shows the game's
explosion effect and sound, pushes and damages cars and props, knocks down every NPC within its reach that the game
lets fall, and panics the crowd. Its own hits land as explosions, starting at V's punch × the square root of
Minecraft's damage, the pipeline dealing about half, and they kill residents and gangers, whose bodies the pushes throw.
The knockdowns took a fix after step 5b: the knockdown grenade's status effect fells NPCs in combat, but plays nothing
on a crowd NPC, so each blast now sends every living NPC within it the game's own forced knockdown reaction, and tells
the crowd with the game's explosion stimulus. In its check, every NPC within a blast fell but a friendly one, which the
game kept from both the knockdown and the damage (61 forced knockdowns over 26 blasts); those the blast didn't kill got
up again, the crowd fled, and no wanted level came. The follow-ups work: Minecraft's
hearts take V's share as build mode starts (one way, as the user chose), a bush is out of Minecraft's ground while walls
stay, and the blades' heat comes back after build mode. The overlay's Fun controls, asked for during the check, let the
user switch Minecraft's game mode and restock without the agent.

**What is left:**
- **Minecraft's inventory and its game mode switcher** in build mode, asked for during step 5b's fix's check, for a
  spike of their own ("Asked for, after Part 3");
- **Minecraft's health back to V:** it goes one way, into Minecraft as build mode starts, as the user chose;
- **our Immortal,** which can't work for the user's V (Part 2): the damage wrap stays V's only guard in build mode;
- **solid NPCs and props, and NPCs' routing,** which this spike left out from the start.

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

## Step 4: in the game, combat

Date: 2026-10-05, 23:47, to 2026-10-06, 00:35. BlockRender 0.18.0 at `f5214b7` (module version 4: the plan's Tasks 13
to 30, the deep review's included), Minecraft 26.3 and SkyCraft 0.1.2 in the `CyberCraft` instance as in step 3, the
game 2.31 with Codeware 1.19. V as in step 2: Mantis Blades installed (the Thermal, Legendary ones), 456 max health. The
user ran both games, and ran a few lines in CET's console; the agent sent the requests and read the logs and the status.
Plan: Task 31.

### Verdict

**Step 4: pass, with three fixes to make.** Every check passed as the plan has it, and the game settled what the deep
review had left open: the start set at the pipeline's entry holds; V's unarmed slot draws the Mantis Blades; the game
draws them while build mode places V; hiding V leaves none of V's meshes on screen, no shadow either; and an NPC's
health shows a hit's change only after the damage step, not at once. Three things don't work as the spec meant:
- **The damage is overkill.** Each point of Minecraft's damage is a whole blade swing, so a diamond sword (7) starts at
  734 and the axe (9) at 944, and each one-shot a level-47 ganger of 594 to 891 health. Fists, a point each, took four
  or five hits, which the user found right.
- **Our Immortal never took effect.** V carries three `Mortal` god modes from the game, which outrank `Immortal`: with
  ours on, `HasGodMode(Immortal)` stays false, and the damage pipeline's `GetImmortality` asks exactly that. So in build
  mode the damage wrap was V's only guard; it held for all 42 hits. The game's Second Heart forces Immortal with
  `EnableOverride` instead.
- **Hiding V leaves two things on screen:** the game's crosshair for the drawn blades, which is the HUD's, and the
  Thermal blades' heat effect, which is a visual effect, not a mesh.

### The checks

- **The module (Steps 2 and 3).** redscript compiled `BlockRender.reds` with Codeware's scripts (`Compilation complete`,
  no error), BlockRender added its seven natives, and at Link: `module: found BlockRenderNpcs version 4, its 10
  functions, the 14 fields List fills, and BlockRender's 7 natives`. Minecraft's player was already in survival
  (`/execute if entity @s[gamemode=survival]`: `Test passed. Count: 1`). After `set combat 1` the status read
  `combat=on: V's punch 104.87 from Items.AdvancedMantisBladesThermalLegendary, not drawn: hits skip the NPC's armour,
  weapon crits, stealth, blocks and skill XP; …` and `v=shown, and hidden in build mode`. An NPC with a capsule now
  reaches 1.76 m, the thin capsule's top (`its top another's: radius 0.06`), and the list lacked none.
- **What V punches with (Step 4).** `punch`: `V's punch 104.87, read from Items.AdvancedMantisBladesThermalLegendary;
  quick hits with MantisBladesAttacks.MantisBladesComboAttack1 (Melee), strong ones with
  MantisBladesAttacks.MantisBladesStrongAttack1 (StrongMelee); not drawn …`. Once the user picked weapon-wheel slot 4,
  the same item, `drawn, so hits carry it as their weapon`: the module's item is the one the game's unarmed slot draws,
  and its punch is the inventory's 105.
- **Build mode draws the blades and hides V (Step 5).** With the gun drawn, F8: `combat: V's Immortal on`, `asked the
  game to draw V's unarmed weapon`, and `build: V's meshes hidden: 82 meshes switched off (1.14 ms)`, the first call
  looking through V's inventory; 0.26 s later 112 (0.07 ms), the blades' 30 meshes hidden too, and a second later `V
  has it drawn`. The user saw nothing of V: no hands, blades, gun, legs, feet or shadow; only the blades' crosshair and
  their heat effect. F8 again: the blades put away, V's gun asked back, and `100 meshes switched on again (1.88 ms)`,
  since weapons destroyed meanwhile leave nothing to switch back on. V held the gun again, hands and legs showing. With
  `set hidev 0`, V stayed seen in build mode, the blades drawn. CET's overlay key, Shift+F6, ends build mode.
- **The game takes the blades back by itself.** Twice, 68 and 74 s into build mode, V's gun came back without any input
  reaching the game; the next second's `Arm` asked for the blades again, and they were drawn within 1.5 s, the gun's
  meshes hidden meanwhile. The cause isn't known.
- **V's Immortal (found in Step 5).** Each time build mode ended, the log said `V's Immortal off (the game says it
  already was)`. Lines in CET's console found why: `AddGodMode` returns true and its source is counted
  (`GetGodModeCount` 1), but `HasGodMode(Immortal)` stays false, while V has `Mortal` from three sources and
  `HasGodMode(Mortal)` is true. `RemoveGodMode` returns false even when it removes, so "already was" is only the log's
  wording. V had no Immortal left after any build mode.
- **One hit on an NPC, as V's own (Step 6).** At a 6th Street spot, NPC 147, listed as "NC Resident" until the user's
  scan named him Scott Fowler: level 47, 594 health. The user struck him twice with the blades, the second a crit of
  about 180. Then `hit 147 1`: `starting at 104.87, 1.00 x V's punch 104.87; queued`, and 16 ms later `landed: started
  at 104.87, 168.72 dealt (critical, killed it); its health 22 -> 22`. The start held, and V's crit came on top as on
  V's own strike. The health read just after the damage step hadn't changed yet; the next `npcs` didn't list him, so he
  was dead. Then `hit 146 3 strong` on Elizaveta Petrova: `starting at 314.61`, landed 1.4 s later, `336.37 dealt` (the
  spread, × 1.07). The user saw our hits set the NPCs burning, as the Thermal blades do.
- **Hits on V in build mode, and Minecraft's death (Steps 7 and 11).** F8 in the fight: ten hits from NPC 145, five
  power strikes in melee, `68.9 of V's health (0.1512), 3.02 of Minecraft's 20, melee (StrongMelee), from NPC 145, a
  power attack`, then, once it drew a pistol, five shots of 48.6 to 52.9, `projectile (Ranged)`: all taken for
  Minecraft. SkyCraft took each one whole, `Skyrim hit the player for 15.12 (3.024 Minecraft): health 20.0 ->
  17.276949`, the player's 4 points of armour taking a tenth. V's health didn't move. The last shot took Minecraft's
  player from 1.68 to 0 just as the user pressed F8: `Minecraft's player died, killed by NPC 145; so V dies: V's health
  set to its least`, 0.17 s after build mode ended, inside the 2 s in which Minecraft's death still counts, and V died.
  After the load, our Immortal came off as V was found again, and V took hits as usual.
- **The fights (Steps 8 and 9).** With a diamond sword, a diamond axe, a bow and fists (oak planks): the sword started
  at 734.08 and dealt 729.63 and 794.16, the axe 943.82 and 1,009.18, and an arrow 1,237.08 (critical, killed it), each
  a one-shot. Punches started at 62 to 105 by the swing's cooldown, and jump crits at 157.30: gangers of 444 to 891
  health took four or five, NPC 141 `170.51 (critical)`, `162.62 (critical)`, `105.42`, `290.77 (critical)` and
  `286.67 (critical, killed it)`. Sprint hits and jump crits went as strong attacks, and V's crit fired on about half
  the hits. All 21 of the session's hits landed, Step 6's two included; none was dodged, blocked, parried or cut by
  armour, and none dealt nothing; each one's "before" showed the last one's damage taken. The NPCs flinched, moved into
  cover, and stopped firing when blocks the user placed cut their line of sight. About 30 shots from three gangers went
  to Minecraft's hearts meanwhile, which the user kept up with golden apples; six of the first ten went through the
  sums. Skill XP wasn't checked.
- **Combat off in build mode (Step 10).** `set combat 0`: our Immortal off, the blades put away and V's gun asked back,
  the gun's meshes hidden within a quarter second (`v=hidden: 155 meshes switched off`). `set combat 1`: the blades
  drawn again within a second, and hidden.
- **The bodies, combat off and unlinking (Steps 12 and 13).** The blocks placed in the fight got bodies (6 boxes; 21
  made and 18 removed, none failed or lost), and V stood on one outside build mode. Then `combat=off`, and `none listed:
  not linked`. The module cost 0.06 ms a frame on average, 0.18 ms at worst.
- **No crash:** the game quit normally (`25 vtable entries restored`). The log has no failed call, no hit left undealt
  and no error.

### What it means

- **Combat both ways works through the game's own systems:** our hits are V's in everything but the swing, and V's
  hurts reach Minecraft whole. Three fixes come before Part 2 closes:
  - the damage's scale, so that Minecraft's weapons don't one-shot what V's own strikes take several for, while a punch
    stays about one of V's;
  - V's Immortal through the game's override (`EnableOverride`, as Second Heart does), so build mode has its backstop;
  - the blades' crosshair and heat effect hidden with V.
- **An NPC's health changes just after the damage step,** so a landing's "after" isn't the health left; the next hit's
  "before" is.
- **For Part 3:** a blast's hits start at the pipeline's entry too, and its scale needs the same thought, since a TNT
  blast is tens of Minecraft's damage.

## Step 4's fixes: in the game, combat again

Date: 2026-10-06, 01:47 to 02:20. BlockRender 0.18.1 at `0e23764` (module version 5: the plan's Tasks 32 to 35, which
ran as written, each commit's code identical to the version tested before), with Minecraft 26.3, SkyCraft 0.1.2 and the
game as in step 4, and V as in step 4: the Thermal, Legendary Mantis Blades, 456 max health. The user ran both games,
and a few lines in CET's console; the agent sent the requests and read the logs and the status. Plan: Task 36.

### Verdict

**Two fixes work; the third can't, for this V.**
- **Where hits start:** V's punch × the square root of Minecraft's damage. A fist starts at one punch, 104.87, and a
  diamond sword at 2.65, 277.46. Level-47 gangers of 594 health took two or three sword hits, two axe hits, or about
  five punches, and those of 891 about half as many again. The user: "0.5 is perfect"; `set hitpower 0.75` was "wayyy
  too high". 0.5 stays the default.
- **Hiding V:** the game's crosshair and the Thermal blades' heat now go with V, and the user saw nothing of V's in
  build mode. The crosshair comes back when build mode ends. The heat comes back only if the blades are drawn again: the
  game starts it as they're drawn, so it isn't one of the effects BlockRender restarts, which are those that start by
  themselves. That matters only when V held the blades before build mode; otherwise build mode's end puts them away.
- **V's Immortal:** the game refuses it on this V. V carries three Mortal god modes that quests left behind, and Mortal
  outranks Immortal, whether it's added (step 4) or forced by the override Second Heart uses (now: `EnableOverride`
  returns false). So the spec's fallback for its risk 18 holds: in build mode the damage wrap is V's only guard. It
  held again for every hit, and the log now says what the game says: `V's Immortal on: but the game says V isn't
  Immortal`.

### The checks

- **The module (Steps 2 and 3).** redscript compiled `BlockRender.reds` with Codeware's scripts, with no error;
  `BlockRender 0.18.1 loaded` and its seven natives added; at Link, `module: found BlockRenderNpcs version 5, its 10
  functions, the 16 fields List fills, and BlockRender's 7 natives`, and `combat: V's Immortal off: the game says V
  isn't Immortal`. After `set combat 1` the status read `combat=on: V's punch 104.87 from
  Items.AdvancedMantisBladesThermalLegendary, not drawn: …; hit power 0.50; hits 0 queued, 0 landed; …` and
  `v=shown, and hidden in build mode`. Minecraft's player was still in survival.
- **V's Immortal (Step 4).** F8 behind the car: `combat: V's Immortal on: but the game says V isn't Immortal`, and at
  its end, `V's Immortal off: the game says V isn't Immortal`. Whether the override comes on a frame late, as the plan
  asked, CET's console settled, outside build mode, under a test source name:
  - `EnableOverride` returned false; `HasGodMode(Immortal)` was false at once, and still false 3 s later;
  - V's god modes: Invulnerable from 0 sources, Immortal from 0, and Mortal from 3: `sq024_city_race`, `sq024_sd_race`
    and `Quest node set mortality (no source given)`. The first two are named for two of The Beast in Me's races
    (`sq024`); the third is a quest node that gave no source name;
  - V has no Second Heart (`HasSecondHeart` 0), so nothing clashed with its override;
  - `DisableOverride` returned false too, and V wasn't left Immortal.

  The damage pipeline asks only `HasGodMode` whether its target is Immortal (`CacheLocalVars` → `GetImmortality`), so
  on this V no god mode can be its backstop. A V without those leftovers would likely get one.
- **The crosshair and the heat (Step 5).** F8 with the gun drawn: `build: V's meshes hidden: 90 meshes switched off,
  258 effects stopped, the crosshair hidden (1.23 ms)`; a quarter second later, once the blades were drawn, `120 meshes
  switched off, 289 effects stopped, the crosshair hidden`, the blades' 31 effects among them. The user saw nothing of
  V's: no hands, blades, legs, shadow, game crosshair or heat; the white cross in the middle was Minecraft's own. F8
  again: `124 meshes switched on again, 0 effects started again, the crosshair shown again (1.45 ms)`, and the game's
  crosshair was back with the gun. With the blades drawn before build mode, their heat showed; in build mode, `89
  meshes switched off, 276 effects stopped, the crosshair hidden`, and the heat was gone; F8 again, `nothing to put
  back` and `0 effects started again`: the blades stayed drawn without their heat, which came back when the user
  switched to the gun and back to the blades.
- **The game puts the blades away by itself.** As in step 4, V's gun came back in build mode, the first time 23 s in,
  then about every 68 s, and `Arm` drew the blades again within a second each time. The user knows why: the game puts a
  melee weapon away after a while idle, and `Arm` draws it again without resetting that timer.
- **Where hits start (Step 6).** Ganger 29, 594 health, unaware of V: `hit 29 1` with the blades drawn, `unarmed:
  starting at 104.87 = V's punch 104.87 x 1.00^0.50; queued`, then `landed: started at 104.87, 123.93 dealt; its health
  594 -> 594`, the spread and a bonus on top, with no crit. Then in build mode, a diamond sword hit on Rada Titova, 594
  health: `blade: starting at 277.46 = V's punch 104.87 x 7.00^0.50; queued`, `293.64 dealt`, and she lived; in step 4
  the same swing started at 734.08.
- **The fights (Step 7).** All 21 hits at 0.5 landed, Step 6's two included; none dealt nothing, and V's crit fired on
  five:
  - **sword:** ganger 29, after the test hit, 271.98 and 309.62 (`killed it`); ganger 53, 891 health, a jump crit
    starting at 339.82 that dealt 387.86, then 469.21 and 481.42 on sprint hits with V's crit (`killed it`);
  - **axe:** Aleksandra Volkova, 594 health, 360.72 and a jump crit that started at 385.31 and dealt 448.18 (`killed
    it`); Rada Titova finished by a sprint hit with V's crit, 570.91;
  - **fists** (oak planks): ganger 54, 891 health, eight punches of 56.96 to 192.14, by the swing's cooldown and V's
    crits (`killed it`);
  - **arrows:** 272.84 to 297.34 each, on gangers of 449 and 891 health.

  As in step 4, no hit came back blocked, parried, cut by armour or dodged. The user saw the NPCs flinch at every hit
  and stagger now and then, and skill XP from the bow and the melee hits. 29 hits on V in these fights went to
  Minecraft's hearts, two of them an explosion's (`other (Explosion)`, with no attacker), and a ganger's shots took the
  last of them: `Minecraft's player died, killed by NPC 52; so V dies: V's health set to its least`, and V died. After
  the load, V was found again, not Immortal.
- **The hit power by feel (Step 8).** At 0.75 the sword started at 451.31 and the axe at 544.92, arrows at 498.84 to
  589.72, and an axe jump crit started at 738.58 and dealt 1,366.71, one-shotting a ganger of 891; with V's crits the
  sword killed a ganger of 594 in two hits and one of 449 in one. The user: "0.75 is wayyy too high compared to 0.5; 0.5
  is perfect". `set hitpower 0.5` again.
- **Combat off, unlinking and quitting (Steps 9 and 10).** `combat=off: Minecraft's hits are logged, not applied; V's
  health is V's`; then `none listed: not linked`; the game quit normally (`25 vtable entries restored`), with no new
  crash report.
- **The log, in all:** 32 hits queued and 32 landed, none dealt nothing or left undealt; 42 hurts taken for Minecraft;
  one death; eight times in build mode, each with `but the game says V isn't Immortal`; no error and no failed call.

### Asked for, after Part 2

- **V's health into Minecraft as build mode starts.** The two healths are apart by design: in build mode only
  Minecraft's hearts take hits, outside it only V's health, and F8 switches which counts. The user went into build mode
  at one heart with V at full health, and wants Minecraft's health set to V's share when build mode starts.
- **Bushes out of Minecraft's collision.** BlockRender gives Minecraft every static collision shape near V, and bushes
  have some, though V walks through them. Standing V in a bush, `shapes` found two spheres of about 2 m with query
  layer `0x80000` alone and an all-zero simulation filter, the shapes that likely hide V from NPCs' sight; and, in one
  bush, a capsule with real filters, whose own size the request doesn't print. The terrain's query layers include
  `0x80000` among others (`0x449c080`), as do a parked car's (`0x4680010`, `0x684000`), on which V stood. So a rule
  could drop static shapes whose only query layer is `0x80000`, or whose simulation filter is all zeros; which, and the
  capsule, wants a check in the game.

### What it means

- **Part 2's combat is as the user wants it:** Minecraft's weapons work through the game's whole pipeline at a scale
  that keeps fists at one punch and swords at a few, and V's hurts reach Minecraft whole.
- **No Immortal backstop for this V,** since quests left it Mortal; the damage wrap has held for all 84 hits on V in
  build mode across steps 4 and the fixes. BlockRender asks for the override as before, and says what the game answers.
- **Left as they are, for now:** the heat that doesn't come back on blades V held before build mode, and the game's
  idle timer that puts the blades away.
- **For Part 3:** a blast's hits start where `Hit` marks them, and a TNT blast is tens of Minecraft's damage, so its
  start needs the hit power too: at the square root, 25 points of it start at five punches.

## Step 5a: Minecraft's blasts and health, with `mclink`, without the game

Date: 2026-10-06, 16:12. `mclink` at `cee26c3` (the plan's Task 40), running step 5a's script, `blasts.txt`, against the
`CyberCraft` instance: Minecraft 26.3, SkyCraft 0.1.2, and CyberCraft Tweaks 1.3.0 at `ca31bf4` (Task 41), installed in
place of 1.2.0, which is kept beside it as `cybercraft-tweaks-1.2.0.jar.disabled`. The user agreed to the install and to
what the script does in their world, and started Minecraft; the agent ran the script and read both logs. Plan: Task 42.

### Verdict

**Step 5a: pass.** A real TNT blast came first, then its hits on the four stand-ins within its reach, each told as the
blast's own, Minecraft's damage exactly 26.3's formula for stand-ins the blast saw whole; the push came a quarter second
after; nothing was built or broken. CyberCraft Tweaks 1.3.0 set Minecraft's player's health to each share sent, up and
down, and SkyCraft's hurts went on as before.

### The checks

- **The blast:** `explosion at 680.50 6.16 1997.50, radius 4.0: a blast reaching 8.0 m`. The TNT, summoned at 6.14 with
  no fuse, fell 0.04 in its one tick and went off 0.06 above its feet.
- **Its hits,** all in the same frame as the explosion, each `knockback 0.00`, `unarmed`, and `a blast's own, … 0.000 s
  after it`: actor 1, 1.50 m from the centre, 42.23; actor 2 at 3.00 m, 29.44; actor 3 at 5.00 m, 15.44; actor 4 at
  7.00 m, 4.94. The formula, (i² + i) ÷ 2 × 56 + 1 with i = 1 − d ÷ 8, gives exactly those: the floor hid none of the
  stand-ins. Actor 5, 9 m away, had no hit.
- **The push:** `the blast at world 680.50 -1997.50 6.16 would push now, 0.27 s after it, with 4 hits of its own`, in
  the first frame after the quarter second.
- **Nothing built or broken:** `Test passed. Count: 2601` before the TNT and after it: the 17 × 9 × 17 blocks around the
  point matched the sky's, all air, both times.
- **The health:** CyberCraft Tweaks' lines, `Minecraft's player's health set to V's share 0.5: 20.0 -> 10.0`, then
  `share 1.0: 11.5 -> 20.0`, `share 0.05: 20.0 -> 1.0` and `share 1.0: 2.0 -> 20.0`. `/data` read 11.5, 20.0 and 2.0 a
  second after each: Minecraft's own healing, from full hunger, added 1.5 and 1 in between.
- **A hurt after it:** `hurt sent at once: other 0.1000 of V's health, a = 1000, attacker 0`, and SkyCraft's `Skyrim hit
  the player for 10.0 (2.0 Minecraft): health 20.0 -> 18.0`; `/data` 18.5; then `share 1.0: 18.5 -> 20.0`.
- **The status:** `events: 1 explosions, 4 hits on actors, 0 deaths, 1 others` (SkyCraft's skill event for the hurt),
  `0 inputs dropped`, and `exit: 0`. Minecraft's player was left in survival at full health, the stand-ins removed.
- **Minecraft's log** had one warning, `Failed to present image, swapchain out of date`, as SkyCraft hid its window when
  the link came up; earlier runs had it too.

### What it means for step 5b

- **BlockRender sees a blast before its hits, in the same frame,** so the 0.15 s window has room to spare.
- **TNT's damage falls off as the spec read,** so in the game these hits would start at V's punch × the square root:
  681, 569, 412 and 233 for the user's V at 1.5, 3, 5 and 7 m, before the pipeline's own modifiers.
- **CyberCraft Tweaks 1.3.0 works in the running Minecraft:** its mixin applied, setting the health raises and lowers it,
  and SkyCraft's own hurts are untouched.

## Step 5b: in the game, explosions

Date: 2026-10-06, 16:16 to 16:44. BlockRender 0.19.0 at `d018b04` (module version 6: the plan's Tasks 43 to 47, which ran
as written, each commit's code identical to the version tested before), with Minecraft 26.3, SkyCraft 0.1.2 and
CyberCraft Tweaks 1.3.0, and V as before: the Thermal, Legendary Mantis Blades, 456 max health. On a pavement with
residents about, parked cars, bins and boxes. The user ran both games and a few lines in CET's console; the agent sent
the requests and read the logs and the status. Plan: Task 48.

### Verdict

**Step 5b: pass, but for living NPCs' knockdowns, which need a fix.** Blasts show the game's explosion, push and damage
cars and props, and their own hits land as explosions that kill and throw; the follow-ups work. Living NPCs within a
blast carry the knockdown grenade's status effect and don't fall: a crowd NPC falls only when its AI gets a knockdown
reaction, which the game's own forced reaction gave one in CET's console. Two more things for the user to decide: the
crowd doesn't panic at a blast, and the health carries only into Minecraft.

### The checks

- **The module (Steps 2 and 3).** redscript compiled `BlockRender.reds` with Codeware's scripts, with no error;
  `BlockRender 0.19.0 loaded` and its seven natives; at Link, `module: found BlockRenderNpcs version 6, its 13
  functions, the 19 fields List fills, and BlockRender's 7 natives`. After `set combat 1`, `blasts=on: 0 taken, blast
  power 0.50; none yet`, and the ground line `the bush rule leaves out sight-only shapes: 377 so far`.
- **Knockdowns (Step 4).** "TNT 5 m ahead" beside two residents: `explosion at Minecraft -120.6 5.9 2036.2, radius 4.0:
  a blast at world (-120.65, -2036.20, 5.94), reaching 8.0 m, 2 listed NPCs within it; its knockdown attack and sound
  started`, then `pushed, 0.26 s after it: 2 NPCs within it, 2 knocked down, 0 dead`. The game gave both the status
  effect, but the user saw them only flinch and slide with the push, without falling, and without fear or flight. With
  `set blastknock 1`, the status effect given directly too, the same. V, within the reach too, wasn't hurt.
- **Why (settled with CET's console).** The resident was a crowd NPC (`IsCrowd` true) with `KnockdownImmunity` 0,
  `KnockdownDamageThreshold` 33 and `KnockdownDamageThresholdImpulse` 0. An NPC falls when its hit reaction component
  sends its AI a knockdown (`SendDataToAIBehavior`), and `GetReactionType` gives one only for a strong explosion, a hit
  flagged `HighSpeedMelee`, or, with both knockdown thresholds over 0, a hit of reaction severity 3 or enough impulse and
  damage. A crowd NPC's impulse threshold is 0, and our hits are weak explosions with no damage, weapon or impulse, so
  they twitch; the status effect alone plays nothing on it. The game's own forced reaction,
  `AISubActionForceHitReaction_Record_Implementation.SendForcedHitDataToAIBehavior(t, 4, 3, 4, 2, 0, 0, 0)`, a
  knockdown at an explosion's intensity, as perks force reactions, knocked the resident down: "the anim looks pretty
  cool".
- **The look.** The game's explosion effect played at the blast, with its sound. Minecraft's own explosion doesn't show:
  `explode` makes no blast in Minecraft, and BlockRender draws Minecraft's blocks, not its particles.
- **The push (Step 5).** By a parked car with props: the car rocked a little, took damage and lost its windows, and the
  props moved as the game's own explosions move them.
- **Minecraft's health (Step 6).** V's health set to 60% in CET's console, then F8: Minecraft's log, `Minecraft's
  player's health set to V's share 0.6612: 20.0 -> 13.224` (V had regenerated to 66%), and the hearts showed it; later
  `share 0.3173: 17.72 -> 6.35`. In build mode V's own health went on regenerating while Minecraft's hearts stayed: the
  share goes one way, as build mode starts, and SkyCraft's `McState` has no player health to carry back.
- **The bush and the wall (Step 7).** In a bush, `shapes`: `sphere of radius 1.40 at (-18.58, -2272.71, 7.36); …;
  query filter 0:0:80000:0, simulation filter 0:0:0:0; the exporter leaves it out, with sight-only shapes`, beside the
  terrain's heightfield, kept; this bush had no capsule. Against a wall every shape was kept (query filters
  `0:0:449c004:0`, `0:0:405c004:0` and `0:0:480000:0`), boxes with their sizes. In build mode Minecraft's player walked
  through the bush and not through the wall.
- **TNT (Step 8).** With the user's leave, 16 TNT in hotbar slot 8 and a flint and steel in slot 9; the user set off a
  chain of 25 blasts among residents. Each blast hit landed as an explosion, starting at V's punch × the square root of
  Minecraft's damage: on NPC 160, a resident of 594 health, `40.75 of Minecraft's damage … a blast's own: starting at
  669.45`, 318.08 dealt; then 479.03 and 234.39; then 161.11 and 76.57, `killed it`. On NPC 161, of 543, 492.93 and
  271.40, then 217.60 and 162.20, `critical, killed it`. The pipeline dealt about half of each start, as it does to
  explosions. The later pushes threw the bodies (`1 NPCs within it, 0 knocked down, 1 dead`). V's health didn't move;
  Minecraft's player lost only 20 → 16.46, to NPC 161's strong melee hit on V, not to the blasts. The user: "Works
  perfectly". The user's punch on a fleeing resident, outside every blast's reach, landed as a punch.
- **The blast power (Step 9).** Not tried: 0.5 felt right.
- **The blades' heat (Step 10).** Blades drawn before build mode, then F8 twice: `V's unarmed weapon put away: put away,
  and drawn again in a second, so that the game starts the effects it starts as it's drawn`, and the heat was back.
- **Combat off, unlinking and quitting (Steps 11 and 12).** `blasts=off: Minecraft's blasts are logged, not applied`;
  the game quit normally (`25 vtable entries restored`), with no new crash report.
- **The log, in all:** 25 blasts and 25 pushes, each 0.26 or 0.27 s after; 5 blast hits queued and landed, 2 of them
  kills, none dealt nothing; 8 health carries.

### What it means

- **The knockdowns need the game's forced reaction:** each living NPC within a blast's reach sent a forced knockdown, its
  direction from the blast, skipping those the game makes immune. A fix, then a check in the game.
- **The crowd doesn't panic:** the game's explosions broadcast an `Explosion` stimulus (`TriggerSingleBroadcast`), and a
  blast of ours broadcasts none. For the user to decide.
- **The health carries one way:** for the user to decide whether Minecraft's health should come back to V as build mode
  ends, or V's own health stop regenerating in build mode.

## Step 5b's fix: in the game, explosions again

Date: 2026-10-06, 18:29 to 20:15. BlockRender 0.19.1 at `bd7acca` (module version 7: the plan's Tasks 49 to 51, which
ran as written, each commit's code identical to the version tested before), with Minecraft 26.3, SkyCraft 0.1.2 and
CyberCraft Tweaks 1.3.0, and V as before. On step 5b's pavement, with residents about, and gangers who fought V. The user
ran both games; the agent sent the requests and read the logs and the status. The user chose, after step 5b, to leave
the health one way. Plan: Task 52, and Task 53 during it.

### Verdict

**Step 5b's fix: pass.** Every living NPC within a blast falls, away from it, by the game's forced reaction, and gets up
again unless the blast kills it; the crowd flees, and no wanted level comes; TNT still kills. With `set blastknock 0` the
crowd stays standing, while gangers fighting V still fall: the knockdown grenade's status effect alone fells NPCs in
combat. The game kept a friendly NPC from both the knockdown and the damage. The overlay's Fun controls, asked for during
the check, worked.

### The checks

- **The module (Steps 2 and 3).** redscript compiled `BlockRender.reds` with Codeware's scripts, with no error;
  `BlockRender 0.19.1 loaded` and its seven natives; `module: found BlockRenderNpcs version 7, its 13 functions, the 22
  fields List fills, and BlockRender's 7 natives`. After `set combat 1`, `blasts=on: 0 taken, blast power 0.50; none
  yet`.
- **The forced knockdowns (Step 4).** "TNT 5 m ahead" five times among residents, with 14, 11, 8, 7 and 6 listed NPCs
  within: each time `N knocked down by the game's forced reaction, 0 immune to it; the crowd told, from the NPC nearest
  it`, all within knocked down, and the push 0.25 to 0.27 s after. The user: "Works perfectly". Everyone fell and got up
  again, so the knockdown, sent as the blast's attack starts, plays out: the attack's hits don't cut it short. The crowd
  ran (the nearest resident 41 m away after), and V got no wanted level.
- **`set blastknock 0` (Step 4).** The reply `set blastknock 0: a blast knocks no NPC down; its attack, sound, own hits
  and push stay`, and in the status `blast power 0.50, the forced knockdowns off`. Nine blasts, each `…; the forced
  knockdowns off; the crowd told, …`: the fleeing residents stayed standing, but the gangers fighting V still fell and
  got up. The pushes counted those with the knockdown's status effect: 8 of 8 and 4 of 4 where the gangers were; 0 of 4,
  2 and 1 elsewhere; then 0 of 11, 1 of 6, 9 of 9 and 5 of 5. So the grenade's status effect alone knocks down NPCs in
  combat, whose AI acts on it, while the crowd needs the forced reaction; the reply's "knocks no NPC down" holds for the
  crowd only. Then `set blastknock 1`.
- **TNT (Step 5).** From hotbar slot 8 in build mode, among four NPCs of 431 to 449 health, which the log names NC
  Residents and one Zane Gardner: all four knocked down by the forced reaction, and their blast hits landed as in step
  5b, Minecraft's damage 6.90 to 39.88 starting at 275.44 to 662.26, the pipeline dealing 207.29 to 361.11, one a
  critical. None died, and the push counted the four knocked down. The user: "working nicely". Two minutes later three of
  them, hurt, were among the gangers below.
- **Gangers (Step 6).** Four more TNT among the gangers fighting V, nine blasts with the chains: every NPC within reach
  knocked down, none immune. The user: "Gangers fell and got up, a couple did die". Blast hits killed four, each NPC's
  health at a hit being its health before less what the earlier hits dealt: Zane Gardner 438, then 77 after 361.11, then
  killed by 182.92. So the pipeline's dealt amounts all land. Then, elsewhere, one TNT killed three residents of 78
  health (376.02, 201.61 critical, and 320.65 dealt), and the push after had them dead within its reach.
- **A friendly NPC.** In the user's own play, four TNT beside a named NPC friendly to V (`LocKey#47098`): each time `0
  knocked down by the game's forced reaction, 1 immune to it`, and its blast hit `dealt nothing: friendly to V`. V's
  punch read 25.00 there, not 104.87, for a reason the log doesn't give.
- **Deaths in build mode,** in the user's play. A ganger's SMG fire, about ten hits a second at 0.36 of Minecraft's 20
  each, took Minecraft's last hearts, and Minecraft's death killed V, as in Part 2. After the load, build mode started
  with V about 47 m above the street (Minecraft y 52, falling): Minecraft's player was let go 1.48 s later, died of the
  fall, and killed V again. That's survival's fall damage, as expected.
- **The Fun controls (Task 53).** Installed with the game running, and loaded by CET's Reload All Mods. 19 commands
  typed through the queue, a second apart where buttons came in a row, none `not typed`, and Minecraft replied to
  each: `Replaced 1 slot(s) on ItzBloxy with [TNT]`, `[Flint and Steel]`, `[Golden Carrot]` and `[Golden Apple]`, the
  four diamond armor pieces, `[Diamond Sword]`, `[Oak Planks]` and `[Crossbow]`, with `Gave 64 [Arrow] to ItzBloxy`;
  `Applied effect Instant Health to ItzBloxy`; `Set own game mode to Creative Mode`, then `Survival Mode`; and four
  `[Air]`, the armor off. The user: "all buttons gave the items".
- **Combat off, unlinking and quitting (Step 7).** `blasts=off: Minecraft's blasts are logged, not applied`; the game
  quit normally (`25 vtable entries restored`), with no new crash report.
- **The log, in all:** 35 blasts, 26 with the forced knockdowns on and 9 off; 61 forced knockdowns, 4 immune (the
  friendly NPC); the crowd told 29 times, and 6 times no listed NPC within 30 m; 35 pushes, 0.25 to 0.27 s after; 19
  blast hits, 15 landed, 9 of them kills, and 4 dealt nothing, friendly to V.

### What it means

- **Blasts knock down every NPC the game lets fall:** the crowd by the forced reaction, NPCs in combat by the status
  effect as well, and friendly ones not at all, as the game wants.
- **The crowd panics at a blast,** with V as its instigator, and that alone brings no wanted level.
- **`set blastknock 0`'s reply overstates:** it stops the forced knockdowns, not the grenade's own on NPCs in combat. Left
  as it is: it's a switch for checks.

### Asked for, after Part 3

- **Minecraft's inventory, and its game mode switcher (F3+F4), in build mode,** asked for during this check (the spec's
  "Asked for during step 5b's fix"). SkyCraft's mod already takes a cursor and keys for Minecraft's screens, and the HUD
  image already shows them. The game's side would need E, F3 and F4 forwarded and hidden from the game; Esc to close a
  Minecraft screen; the mouse made Minecraft's cursor while a screen is open, hidden from the game; and a cursor drawn.
  The switcher's spectator mode would drag V through walls, since in build mode V follows Minecraft's player. For a
  spike of its own.
