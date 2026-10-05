# NPCs and combat: design (spike 6)

Date: 2026-10-05. Status: agreed with the user section by section in the brainstorm. "Found while writing", near the
end, lists what writing it out added to what was agreed. Then revised after two deep self-reviews, which checked each claim
against its source and walked through each step as the user would play it: "Changes from the deep review" and "Changes
from the second deep review", at the end, list what changed and what was checked. The sections above them already include
those changes. Planning Part 1 changed or settled more, which "Changes while planning" lists, as do step 2's first run
("Changes from step 2's first run") and planning Part 2 ("Changes while planning Part 2"); the sections above include
those too. A deep review of Part 2's plan, before step 4, changed how Minecraft's hits land, with the user's decisions:
"Changes from the deep review of Part 2" says what and why, and the sections above include it. Step 4 then passed
with three fixes to make, which the user chose how to make: "Changes from step 4", at the very end, says what they are,
and the sections above include them.

## Purpose

This is spike 6 of the project that recreates [SkyCraft](https://github.com/chasmlol/SkyCraft) (Minecraft in Skyrim)
for Cyberpunk 2077. Spike 5 let the user build as Minecraft's player in build mode, gave Minecraft Night City's shape,
and made Minecraft's blocks solid for V. It left Night City's NPCs out:
- Minecraft's weapons pass through NPCs, and Minecraft's player walks through them;
- in build mode, the game's attacks on V hurt V, not the player the user is playing as.

SkyCraft's own model answers this:
- every Skyrim actor near the player gets an invisible stand-in in Minecraft, which Minecraft's own combat hits
  (swords, crits, sweeps, bows, tridents), and each hit goes back to the real actor;
- Minecraft owns the player's health: Skyrim's hits on the player are taken off Minecraft's player, and Minecraft's
  death is Skyrim's;
- Minecraft's explosions knock Skyrim's people down and throw loose things.

The user chose that model for build mode, with the numbers read from the game rather than tuned by hand:

> In build mode, can Night City's NPCs be fought with Minecraft's weapons, each NPC a stand-in that Minecraft hits,
> with the hits landing on the real NPC as V's own, against its real health? Do the game's hits on V go to
> Minecraft's player instead, with its armour and shield, and its death killing V? And do Minecraft's explosions
> knock down and throw NPCs and props in the game?

It has three parts:
1. **NPCs in Minecraft:** a redscript module lists the NPCs around V, and BlockRender writes them into SkyCraft's
   actor table every frame.
2. **Combat both ways:** Minecraft's hits land on NPCs as V's; in build mode the game's hits on V go to Minecraft's
   player, and Minecraft's death kills V.
3. **Explosions:** Minecraft's blasts knock down and throw NPCs, and push cars and props, through the game's own
   explosion code.

Each is tested against something real as soon as it's built (see "The steps").

## Decisions from the brainstorm

- **Scope: NPCs in Minecraft and combat both ways, with Minecraft's explosions as the last part.** The user chose these
  over solid NPCs and props for Minecraft's player, and NPCs routing around Minecraft's blocks, which wait for later.
  Drawing Minecraft's own mobs in Night City waits too.
- **Minecraft owns the health in build mode, SkyCraft's way.** In build mode V is Immortal, the game's damage to V goes
  to Minecraft's player, and Minecraft's death kills V. Outside build mode V's health is V's own. F8 switches which
  health counts.
- **The numbers come from the game, not hand-tuned tables.** The user asked for research into what the game exposes,
  and for the NPC's real health and the damage V would really do:
  - a Minecraft hit does Minecraft's damage × V's punch, the damage per hit of what V punches with, and the NPC's real
    health takes it. Since the deep review of Part 2, that's where the hit starts in the game's damage pipeline, which
    then does to it all it does to V's own punches: V's crits, perks and cyberware, the NPC's armour, resistances,
    blocks and dodges. Since step 4, which found swords and axes one-shotting gangers, it starts at V's punch ×
    Minecraft's damage to the power 0.5, the square root, so that a fist stays one punch;
  - a hit on V costs Minecraft's player the same share of its 20 health as it would have cost V of V's max health.
- **A redscript module for the game's side.** The user chose it over doing everything in C++ through RTTI, and over a
  CET Lua mod. The game's logic is written as the game's own scripts are, and checked against the game's declarations
  when it compiles; BlockRender keeps the link, the protocol and the unit tests.
- **Agreed while presenting the design:**
  - NPCs' sizes from their PhysX capsules, found once per NPC, and a person's size until then (Part 1);
  - V's damage taken out of the game's damage pipeline before it's dealt, rather than refunded after as SkyCraft must
    in Skyrim, so V's health never dips (Part 2);
  - hurts summed into one every half second, since Minecraft ignores most damage within half a second of the last
    (Part 2 waits 11 of Minecraft's ticks, a little over its half second);
  - crits, and hits with knockback over SkyCraft's 0.45, land as strong attacks;
  - a blast's own hits land as explosion hits, so the game throws the bodies (Part 3).
- **The parts in order, each planned after the step before it,** as spikes 4 and 5 were. Each in-game step opens by
  checking its own unknowns with the least code.

## Non-goals

- **Solid NPCs and props for Minecraft's player.** Minecraft's player walks through NPCs, cars and kinematic props such
  as spike 5's guard rail: SkyCraft's stand-ins can't be collided with.
- **NPCs routing around Minecraft's blocks.** The game's `NavigationSystem.AddObstacle` is noted for that ("The game's
  scripts").
- **Drawing Minecraft's entities in Night City:** its mobs, items, arrows and the player's model (`kRenScene`,
  `kRenAvatar`, the world entities).
- **SkyCraft's arrows stuck in NPCs** (`kEvArrowStuck`) and **Skyrim's skills** (`kEvSkillUse`): drained and counted,
  nothing more.
- **Minecraft's fire on NPCs:** a hit's fire flag (Fire Aspect, flaming arrows) isn't carried over, and NPCs don't burn
  in Minecraft's lava or fire as SkyCraft's do.
- **Vehicles as targets,** and anything Minecraft's player gets from a kill (loot, experience).
- **Changes to SkyCraft's mod.** Protocol 11 stays. CyberCraft Tweaks changes only if Minecraft has to; none is planned.
- **Only these versions:** game 2.31, SkyCraft 0.1.2, Minecraft 26.3, and the PhysX DLLs spike 1 audited.

## Environment (checked on 2026-10-05)

Each fact below was read in its source:
- SkyCraft's code at `bfcaf17` (the clone in an earlier session's scratchpad) and the protocol header;
- the game's scripts as decompiled for 2.31, the game version installed: [adamsmasher/cyberpunk](https://codeberg.org/adamsmasher/cyberpunk)
  on Codeberg, at its commit "2.31 (#27)";
- the user's Minecraft 26.3 jar (`%APPDATA%\.minecraft\versions\26.3\26.3.jar`);
- the RED4ext SDK in `extern/`, redscript's log in the game, and BlockRender's code at `980c2e5`.

### The game and BlockRender

- **BlockRender 0.16.0** (`d5ed7e0`) is installed: build mode, Night City's shape, and bodies for Minecraft's blocks
  around V, on by default. Its log line says 0.16.0, but the version `Query` reports to RED4ext is still 0.15.0
  (`RED4EXT_V1_SEMVER(0, 15, 0)`, set at `a377189`).
- **Its frame** (`Plugin.cpp`, `OnUpdate`): requests once a second; `UpdateLink`, which finds V (`LocatePlayer`), reads
  V's yaw and what keeps V from being placed (`Puppet`), runs `Link::Update`, the router (build mode) and `PlaceV`, then
  the city's reading and the bodies; then the renderer.
- **Events** (`Link::Update`): the event ring is drained (`DrainEvents`); `kEvExplosion` goes to the sink
  (`MinecraftSink::Explosion`, which logs it), and every other event is only counted (`otherEvents`). Nothing is written
  to the actor table, and `kInHurt` is never sent.
- **The mapping** (`link/Mapping.cpp`) clears the actor table, the event ring's head and tail, and the world entities
  when it creates them.
- **Build mode ends when Minecraft's player dies** (`input/Router.cpp`): the router counts build mode blocked while
  McState's flags have `kMcDead`, as it does for V in a vehicle, in a workspot or dead.
- **`Puppet`** calls the game's script functions by name: each is found once, picked by its parameter and return types,
  logged, and never called with other types; main thread only. The module's calls follow that pattern.
- **The look** (`link/Look.h`): `LookFromForward` gives Minecraft's yaw (0 south, 90 west) from a forward vector in the
  world's axes.
- **PhysX** (`physx/Export`): `PhysxGround::Prepared` gives scene 0 and PhysX's origin once a frame. `Describe`, the
  `shapes` request, lists every shape within a radius of a point, static or dynamic, with its geometry, actor, bounds,
  flags and filters. V's controller is found as "the kinematic capsule most nearly under V" (`Origin`).

### SkyCraft's Minecraft side, for actors and combat

From `combat/SkyCombat.java`, `combat/SkyrimActorEntity.java`, `client/ProxySync.java`, `link/SkyLink.java`,
`client/InputBridge.java` and the mixins named:
- **Stand-ins** (`SkyCombat.serverTick`, at the end of every server tick):
  - they exist while the game's heartbeat is under 8 s old (`SkyLink.active`, `HEARTBEAT_TIMEOUT_MS = 8000`) and a
    player is in the world; otherwise all are removed;
  - the table is read under its seqlock (`SkyLink.readActors`: up to 16 tries, the count clamped to 256);
  - a record not flagged dead gets a stand-in, made at its feet with its size and yaw, and named when its name isn't
    empty; a stand-in whose record is gone, or dead, is removed;
  - each tick, stand-ins are moved to their records (`setPos`, the yaw), and press pressure plates and trip tripwires
    they overlap (`stepOnTriggers`);
  - on the client, `ProxySync.frame` moves the client's copies to the table every frame, so the crosshair and reach
    match what's on screen.
- **What a stand-in is** (`SkyrimActorEntity`, the entity type `skycraft:skyrim_actor`, 0.6 × 1.8 unless its record
  says otherwise, `noSave`, `noSummon`, `noLootTable`, tracked 10 chunks out): a `LivingEntity`, invisible and silent,
  with no gravity or physics, not pushable, and not collidable (`canBeCollidedWith` is false). Its health is set back to
  full every tick.
- **Its hits** (`actuallyHurt`, `knockback`): what Minecraft deals it, after Minecraft's own modifiers (crits,
  enchantments, strength, the attack cooldown, invulnerability frames), is added up over the tick, with the projectile
  and fire flags, the weapon class, the strongest knockback's direction and strength, and the crit flag
  (`ServerPlayerMixin`, on `crit`). At the tick's end each stand-in hit sends one `kEvHitActor`: the formId, `a` = the
  damage, `b` and `c` = the push's direction in x and z, `d` = its strength, the flags and the weapon class. A tick with
  knockback but no damage still sends one.
- **Plates and blocks on the city's ground:** pressure plates notice anything in their whole block, not just its bottom
  quarter (`PressurePlateMixin`); an air block over the game's ground counts as sturdy from above
  (`BlockStateBaseMixin`), so plates, torches and the like can be placed on it.
- **`kInHurt`** (`InputBridge`): `a` / 100 is "Skyrim damage", applied on the integrated server by
  `SkyCombat.hurtPlayer`:
  - nothing if the player is dead or the damage isn't over 0;
  - the attacker is the stand-in with that formId, unless there's none or it's more than 24 blocks away;
  - the damage source: melee `mobAttack(attacker)`, projectile `mobProjectile(attacker, attacker)`, magic
    `indirectMagic(attacker, attacker)`; with no attacker, `generic()` for melee and projectile and `magic()` for
    magic; other is `generic()`;
  - Minecraft's damage is Skyrim's ÷ 5 (`SKYRIM_TO_MC_DAMAGE`), applied with `hurtServer`;
  - a power attack (flag 2) with an attacker, when it hurt and the player wasn't blocking, shoves the player
    (`knockback(0.5, …)`).
- **The player's death** (`ServerPlayerMixin`, on `die`): `kEvPlayerDied`, with the formId of the stand-in behind the
  killing damage, or 0.
- **Explosions** (`ServerExplosionMixin`): at the end of `ServerExplosion.explode`, after the blast has hurt every
  entity it reached, `kEvExplosion` with its centre and radius (the explosion's power: 4 for TNT). The stand-ins' hits
  from it follow at that tick's end.
- **Of a record, SkyCraft's Minecraft side reads** only the formId, the feet, yaw, size, name and dead flag. The health,
  level and other flags aren't read.

### SkyCraft's game side, as a reference

From `skse/src/Combat.cpp` and `Link.cpp`:
- **The table** (`WriteActorTable`, every frame while Minecraft owns the player): actors within 80 blocks
  (`kActorRange`), not disabled, loaded, not ghosts; height and width from the actor's height and bound radius, clamped;
  the health fraction, level and display name. `Link::WriteActors` writes it: the sequence made odd, a release fence,
  the count and records, then the sequence made even with release.
- **Hits** (`ApplyHit`): damage × (5 + 0.25 × level), through Skyrim's own hit function, with a vanilla weapon standing
  in for each weapon class. A crit or knockback over 0.45 staggers (`push > 0.45f || crit`); ordinary swings don't,
  since "Minecraft's attack rate would otherwise stun-lock everything". Combat starts against the player.
- **The player's damage** (`BridgePlayerDamage`): the player is essential, and every frame any health lost is restored
  and sent as `kInHurt`; melee and projectile at once, magic and other summed for half a second (`kDotFlushSeconds`,
  "Minecraft i-frames").
- **Death** (`KillPlayer`): on `kEvPlayerDied`, the player stops being essential and is killed, by the killer.
- **Explosions** (`ApplyExplosion`): a quarter of a second after the event, people within twice the radius are knocked
  away (`KnockExplosion`) and loose bodies get an impulse; anyone the blast killed is flung once they ragdoll.

### The game's scripts

From the decompiled 2.31 scripts; every function named here was read there.
- **Finding NPCs:**
  - `NPCPuppet` is a script class (`cyberpunk/NPC/NPCPuppet.swift`, `extends ScriptedPuppet`) with the callbacks
    `OnGameAttached()` and `OnDetach()`. No script class extends it, and only it and `PlayerPuppet` extend
    `ScriptedPuppet`, so a wrap of its callbacks sees every NPC the scripts know;
  - `TargetingSystem.GetTargetParts(instigator, query: TargetSearchQuery, out parts) -> Bool` searches around an
    object; the HUD's nameplates use it with a maximum distance (`hudManager.swift`).
- **Reading an NPC:**
  - `Entity` (`core/entity/entity.swift`): `GetEntityID()`, `GetWorldPosition() -> Vector4`,
    `GetWorldForward() -> Vector4`. For V, `GetWorldPosition` is what BlockRender calls V's feet (`PlayerLocator.cpp`),
    and V's controller is centred 1 m above it;
  - `GameObject` (`core/entity/gameObject.swift`): `GetDisplayName() -> String`, `IsDead()`, and the static
    `GetAttitudeTowards(first, second) -> EAIAttitude` (`AIA_Hostile` is 2);
  - `NPCPuppet.IsInCombat(npc)`, static;
  - `StatsSystem.GetStatValue(id, gamedataStatType)`: `PowerLevel` for the level, as the game reads a target's level
    (`quickHackableHelper.swift`, the telemetry's enemy level), and `Health` for the max health, as
    `DamageSystem.ProcessRagdollHit` reads it;
  - `StatPoolsSystem.GetStatPoolValue(id, type, opt perc)` and `GetStatPoolMaxPointValue(id, type)`;
  - `GodModeSystem.HasGodMode(id, type)`, and `GameInstance.FindEntityByID(game, id) -> ref<Entity>`;
  - `EntityID.GetHash` gives 32 bits; an `EntityID` is a 64-bit hash.
- **V's punch** (`cyberpunk/systems/equipmentSystem.swift`): what V punches with is what V's unarmed slot draws,
  weapon-wheel slot 4: `GetWeaponSlotItem(4)` is `GetMeleewareOrFistsItemID()`, V's arm cyberware weapon if V has one
  (`EquipmentSystemPlayerData.GetActiveMeleeWare()`: the first item in the `ArmsCW` area whose category is a weapon),
  otherwise the item in the `BaseFists` equipment area. That's the base fists, `Items.w_melee_004__fists_a`, which the
  game gives V, or the Gorilla Arms while they're installed: `HandleStrongArmsEquip` puts the arms' item there, and
  `EquipBaseFists` puts the fists back. Weapon cycling's bare-handed weapon is the same, and the fists alone are drawn
  only under a quest's `Fists` restriction (`RequestFists`); holstered, an attack draws the last weapon used
  (`upperBodyTransitions.swift`). `EquipmentSystem.GetItemsInArea(owner, area) -> [ItemID]` (static) lists an area, empty
  slots included. `TransactionSystem.GetItemData(obj, itemID)` and
  `gameItemData.GetStatValueByType(gamedataStatType.EffectiveDamagePerHit)` read its damage per hit, the number the
  inventory shows (`InventoryItem.swift`): the item's own, before anything the damage pipeline adds.
- **Drawing a weapon by script** (`statusEffectSystem.swift`'s `StatusEffectHelper.RequestFists` and its neighbours): an
  `EquipmentSystemWeaponManipulationRequest`, with its `owner`, a `requestType` (`RequestWeaponSlot1` to `4`,
  `UnequipWeapon`, …) and an `equipAnimType` (`Instant`), queued on `EquipmentSystem.GetInstance(owner)`; the equipment
  system resolves the item (`GetItemIDfromEquipmentManipulationAction`) and asks the player's state machine to draw it.
- **Hiding V:** `IComponent.Toggle(on)` and `IsEnabled()` are the game's own (`orphans.swift`), and every mesh V and its
  items are drawn with (plain, skinned, garment and morph-target meshes) is an `IVisualComponent` (the RED4ext SDK's
  classes). The game's scripts can't list an entity's components; Codeware's `Entity.GetComponents()` can (Codeware
  1.19, installed). `TransactionSystem.GetItemList` and `GetItemInSlotByItemID` give the objects of what V wears and
  holds.
- **Effects:** an object's effects come from its effect spawners, `ent::EffectSpawnerComponent`, which like
  `ent::ParticlesComponent` is an `IVisualComponent` (the SDK's classes). Switching a spawner off doesn't stop the
  effects it has started (step 4: the Thermal blades' heat stayed with V hidden). The game stops and starts an object's
  effect by name: `entKillEffectEvent` (`effectName`, `breakAllLoops`) and `entSpawnEffectEvent` (`effectName`,
  `persistOnDetach`), queued on the object (`GameObject.StopReplicatedEffectEvent` and `StartReplicatedEffectEvent`,
  `gameObject.swift`; `AIActionHelper.KillWeaponEffect` on a weapon). The game's scripts don't declare a spawner's
  effects; Codeware does: `entEffectSpawnerComponent.effectDescs`, each `entEffectDesc` with its `effectName` and
  `isAutoSpawn`.
- **The game's crosshair** (`crosshairContainerController.swift`): `gameuiCrosshairContainerController`, an
  `inkHUDGameController`, holds the crosshair of whatever is drawn, and hides it for the scanner by switching its root
  widget off (`OnPSMVisionStateChanged`: `GetRootWidget().SetVisible(value == 0)`). Codeware lists a UI layer's game
  controllers: `GameInstance.GetInkSystem().GetLayer(name).GetGameControllers()`.
- **God mode and health:**
  - `GodModeSystem.AddGodMode(id, type, sourceInfo)` and `RemoveGodMode(id, type, sourceInfo)` keep each mode per
    source name: the game's menus add and remove `Invulnerable` on V under their own (`n"HubMenu"`, `n"WorldMap"`);
    `GetGodModeCount` and `GetGodModeSources` count and name them. `RemoveGodMode` returns false even when it removes
    (step 4);
  - `gameGodModeType` is `Invulnerable`, `Immortal` or `Mortal`. V carries `Mortal` from three sources (step 4, read
    through CET's console), and it outranks `Immortal`: with an `Immortal` source added, `HasGodMode(Immortal)` stays
    false. The damage pipeline asks exactly that (`GetImmortality`, `godModeSystem.swift`: `HasGodMode(Invulnerable)`,
    then `HasGodMode(Immortal)`), so on V an added `Immortal` does nothing;
  - an override forces a mode over the sources: `EnableOverride(id, type, sourceInfo)` and `DisableOverride(id,
    sourceInfo)`. Second Heart keeps V alive with it: `SecondHeartStatListener` (`playerListeners.swift`) enables
    `Immortal` under `n"SecondHeart"` while the implant has a charge, and disables it when it has none;
  - in the damage pipeline, `Invulnerable` makes a hit deal no damage (`InvulnerabilityCheck`), while `Immortal` only
    flags it `ImmortalTarget` (`ImmortalityCheck`) and lets it go on;
  - `StatPoolsSystem.RequestSettingStatPoolMinValue(id, type, instigator)` sets a pool to its minimum. It's how the
    damage pipeline's own instant kill kills (`ProcessInstantKill`: the target's `Health`, with the attack's
    instigator), and how a security turret kills itself, with `null` as the instigator.
- **The damage pipeline** (`cyberpunk/damage/damageSystem.swift`, mostly script):
  - `DamageSystem.QueueHitEvent(evt: ref<gameHitEvent>, receiver)` takes a hit. `ProcessPipeline` then runs
    `PreProcess`, `Process`, `ProcessHitReaction` (`target.ReactToHitProcess`) and `PostProcess`, with synchronous
    listener callbacks before each;
  - `PreProcess` runs `ConvertDPSToHitDamage`, `CalculateDamageVariants` (V's hits on NPCs × 0.9 to 1.1),
    `CheckForQuickExit`, the god-mode checks and `DeathCheck`, then `ModifyHitData`. That runs
    `DamageManager.ModifyHitData` (the NPC's block or parry, `ProcessDefensiveState`; the stealth flag when V isn't in
    combat), then marks a hit with no health damage `DealNoDamage`, with the source name `n"no_valid_damage"`, and only
    then `ProcessDamageReduction` (the NPC's `MeleeResistance`), `ProcessLocalizedDamage`, `ProcessInstantKill`,
    `ProcessDodge`, `ProcessEvasion` and `ProcessMitigation`: the last three also give `DealNoDamage`, or cut the damage;
  - **what the pipeline does to V's punch** (the deep review of Part 2): besides the spread, `Process` applies V's
    bonuses against bosses, elites, rares and mechanicals, and Berserk's melee bonus
    (`DamageManager.CalculateSourceModifiers`); V's crits, V's and the weapon's `CritChance` and `CritDamage` (`ProcessCriticalHit`,
    `ProcessDamageMultipliers`); `AllDamageDonePercentBonus`, `MeleeDamagePercentBonus` and the bonuses against enemies
    at full health, below half and burning (`ProcessCyberwareModifiers`); the stealth bonus (`ProcessStealthAttack`); the
    NPC's `DamageReductionMelee` and `Armor`, at 1 ÷ (1 + armour × effectiveness) (`CalculateTargetModifiers`,
    `ProcessArmor`); a block or parry, at × 0 (`ProcessBlockAndDeflect`); and story mode's scaling (`ScalePlayerDamage`).
    Perks hook the pipeline too: `ModifyDamageEffector`s, behind hit prereqs registered as synchronous listeners at a
    stage, `Process`'s by default (`genericHitPrereq.swift`). Armour, the weapon's crits, the stealth bonus, blocks, the
    skill XP from damage (`RPGManager.AwardExperienceFromDamage`) and the prereqs that check a weapon all read the hit's
    weapon object, and skip a hit without one. `ProcessLevelDifference` is never called in 2.31;
  - `CheckForQuickExit`: a hit on its own instigator without `CanDamageSelf` deals nothing and stops the pipeline
    there; a hit on someone friendly to its instigator deals nothing (`FriendlyFireIgnored`), unless it's flagged
    `FriendlyFire`;
  - `Process` returns at once for `DealNoDamage`. Otherwise it applies the modifiers, then special cases
    (`ProcessRagdollHit` sets an exact amount for ragdoll impacts), then `DealDamages`, the last step, which applies the
    computed damage (`StatPoolsManager.ApplyDamage`) and sends the damage events. `ApplyDamage` still changes it: bosses
    and MaxTac take at most `MaxPercentDamageTakenPerHit` a hit; a target's overshield, then its armour pool, take
    their part first; V's finisher perk can leave an NPC at 1 health for a finisher (`IsFinisherGrace`); a hit flagged
    `CannotKillPlayer` leaves V 1 health; and a target's minimum health (`GetMinimumHealthPercent`, 1% when Immortal)
    holds;
  - `gameHitEvent` holds `attackData`, `target`, `hitPosition`, `hitDirection`, `hitRepresentationResult` and
    `attackComputed`. `gameAttackComputed` has `GetTotalAttackValue(statPoolType)`, `SetAttackValue(value, opt
    damageType)` and `SetAttackValues`. `AttackData` has `SetAttackDefinition`, `SetInstigator`, `SetSource`,
    `SetWeapon`, `SetAttackType`, `SetHitReactionSeverityMin` and `Max`, `AddFlag(flag, sourceName)`,
    `RemoveFlag(flag, sourceName)` and `HasFlag`;
  - **one entry a flag:** `AddFlag` does nothing when the flag is already set (`attackData.swift`), so a flag keeps
    its first source's name, and `RemoveFlag(flag, sourceName)` removes it only under that name. Friendly fire, an
    invulnerable target and a dead one give `DealNoDamage` before `ModifyHitData` sets `n"no_valid_damage"`, but instant
    kills, dodges and evasion give it after, so on a hit already marked `no_valid_damage` theirs never goes in (the
    deep review of Part 2 found this; the spec first said every other reason came before);
  - **hits made by script:** `SendHitEvent` (`triggerAttackOnNearbyEnemiesEffector.swift`) builds one: an
    `AttackInitContext` (record, instigator, source, weapon), `IAttack.Create`, a `gameHitEvent` with a new
    `AttackData`, its target, and a hit shape at the hit's position, then `QueueHitEvent`.
    `NPCPuppet.OnStartRagdollDamageEvent` builds the ragdoll impact the same way, flagged `DeterministicDamage` and
    `CannotModifyDamage`, and `ProcessRagdollHit` then sets its amount on its `attackComputed`: so a hit built by
    script gets an `attackComputed` from the game;
  - **wrapping the pipeline:** redscript's own hook tutorial wraps `DamageSystem.ProcessLocalizedDamage`, one of the
    pipeline's private methods, with `@wrapMethod(DamageSystem)` (the redmodding wiki's "How to create a hook");
  - `gamedataAttackType`: `QuickMelee`, `StrongMelee`, `Melee`, `GuardBreak`, `WhipAttack`, `ChargedWhipAttack`,
    `Reflect`, `Ranged`, `Thrown`, `Direct`, `Hack`, `Explosion`, `PressureWave`, `Effect` and `ForceKill`.
- **Explosions** (`cyberpunk/devices/explosive/ExplosiveDevice.swift`,
  `cyberpunk/items/combat_gadgets/combatGadgetHelper.swift`, `cyberpunk/managers/rpgManager.swift`):
  - an exploding device makes its attack in `DoAttack`: `RPGManager.PrepareGameEffectAttack(game, instigator, source,
    attack record, position, hit flags)`, then `StartAttack`;
  - and its push in `DoPhysicsPulse`: `CombatGadgetHelper.SpawnPhysicalImpulse`, which runs the game effect
    `physicalImpulseSphere` with a radius and a position;
  - its effects come from its own effect resources (`SpawnVFXs`);
  - `PrepareGameEffectAttack` takes the radius from the attack record and puts the hit flags in the effect's data;
    `CombatGadgetHelper.SpawnAttack` sets a radius of its own.
- **Scriptable systems** (`core/systems/scriptableSystem.swift`): a script class extending `ScriptableSystem` gets
  `OnAttach`, `OnDetach` and `OnRestored`, and is found with `GameInstance.GetScriptableSystemsContainer(game).Get(name)`.
  Only fields marked `persistent` are saved. Many of the game's own systems are plain script classes like that
  (`PreventionSystem`, `CraftingSystem`, `FastTravelSystem`), found by name; what makes one per game session is native,
  so step 2's opening check confirms a mod's is made too.
- **For later:** `NavigationSystem.AddObstacle(position, radius, height, agentSize) -> ref<NavigationObstacle>`
  (`core/systems/navigationSystem.swift`), for NPCs routing around blocks.

### redscript and RED4ext

- **RED4ext lets a plugin add to redscript's compile** a script folder or file: `Sdk::scripts->Add(handle, path)`
  (`extern/RED4ext.SDK/include/RED4ext/Api/v1/Scripts.hpp`).
- **A plugin adds native global functions** once the game has registered its types: `CRTTISystem`'s
  `AddPostRegisterCallback`, then `CGlobalFunction::Create` and `RegisterFunction` (the SDK's
  `examples/native_globals_redscript`). Scripts declare them as `public native func Name(params) -> Type`, as Codeware
  declares its own (`Codeware.Global.reds`).
- **Script callbacks run on several threads:** a crash report in a Codeware issue (psiberx/cp2077-codeware#51) has
  `OnGameAttached` and `OnTakeControl` running on other threads, and one script function on five at once; Cyber Engine
  Tweaks 1.18.1 fixed crashes in very frequently called overrides, such as `GameObject.OnGameAttached` and `OnDetach` on
  a reload, around when it put its Lua VM behind a lock. Step 2's first run crashed the same way (the results).
- **redscript's last log** (`r6/logs/redscript_rCURRENT.log`) lists the files it compiled at the game's start: the mods'
  in `r6/scripts`, and TweakXL's, ArchiveXL's, Codeware's and mod_settings' from their plugin folders; then
  `Compilation complete`, and the `final.redscripts.modded` it wrote.
- **redscript's compiler** is the game's `engine/tools/scc.exe`, with `scc_lib.dll`. Run alone with `--help`, it prints
  only "not compile". Settled while planning: `scc_lib.dll`'s C API compiles a script folder against a given
  `final.redscripts`, with its error popup off, so the plan checks the module before deploying (`tools/check_reds.py`);
  `scc.exe` itself shows a message box when a compile fails.

### PhysX, for NPCs' sizes

- **V's controller** (spike 1, spike 5's step 3) is a kinematic capsule (radius 0.32, half-height 0.4) centred 1 m
  above V's feet, with query filter `0:0:2000:0`; a second kinematic capsule, radius 1, encloses V. NPCs' capsules
  haven't been looked at.
- **The calls that would read them are audited** and already used: the overlap query, the actors' `getGlobalPose`,
  `getNbShapes`, `getShapes` and `getRigidBodyFlags`, and the shapes' geometry type, capsule geometry, local pose and
  filters (`docs/spikes/2026-10-01-physx-abi-audit.md`, its first table and its addenda).

### Minecraft 26.3

From the user's jar:
- **Damage that passes armour** (`data/minecraft/tags/damage_type/bypasses_armor.json`) includes `generic`, `magic`,
  `indirect_magic` and `generic_kill`; `bypasses_shield` takes all of those too, and only `sonic_boom` passes
  Protection (`bypasses_enchantments`). So of what `hurtPlayer` makes, armour and a shield reduce only `mobAttack` and
  `mobProjectile`: melee and projectile hits whose attacker has a stand-in within 24 blocks.
- **`/kill` on a living entity** (`LivingEntity.kill`): `hurtServer` with `genericKill` and `Float.MAX_VALUE`. On a
  stand-in it comes back as a `kEvHitActor` of 3.4 × 10³⁸.
- **Respawning at once, and keeping the inventory:** the game rules `immediate_respawn` and `keep_inventory`
  (`gamerules/GameRules`). Without the second, a death drops what the player carries.
- **Weapons:** a fist does 1, the player's base attack damage. A diamond sword is `sword(DIAMOND, 3.0, -2.4)`, and
  diamond's attack bonus is 3.0, so it does 1 + 3 + 3 = 7, at 1.6 attacks a second; Minecraft scales a hit down when the
  attack cooldown isn't full.
- **Explosions:** `ServerExplosion.explode` works out the blocks, hurts the entities (`hurtEntities`), then breaks the
  blocks and sets fires, and returns; SkyCraft's `kEvExplosion` comes at that return.
- **Creative mode:** a creative player takes no damage from what `hurtPlayer` makes, so the game's hits on V cost
  Minecraft's player nothing unless it's in survival.

## The protocol, as this spike uses it

SkyCraft's header (`extern/skycraft/skycraft_protocol.h`, version 11) stays unchanged.
- **The actor table, written:** `ActorTable` at `0x12000`: `seq`, `count`, then up to 256 `ActorRecord`s of 64 bytes:
  `formId`; `flags` (hostile 1, dead 2, essential 4, in combat 8); the feet `x y z`, in Minecraft's axes; `yaw`;
  `width` and `height` in blocks; `healthFrac`; `level` (16 bits); and a 24-byte UTF-8 name, NUL-terminated.
- **The event ring, read:** `kEvHitActor` (1), `kEvPlayerDied` (2) and `kEvExplosion` (3). `kEvArrowStuck` (4) and
  `kEvSkillUse` (5) are drained and counted.
- **The input ring, written:** `kInHurt` (7): `code` = the kind (melee 0, projectile 1, magic 2, other 3), `a` = the
  damage × 100, `b` = the attacker's formId, `c` = the flags (blocked 1, power attack 2).

## Repository layout

```
docs/superpowers/specs/2026-10-05-npcs-and-combat-design.md   this file
docs/superpowers/plans/2026-10-05-npcs-and-combat.md          the plan, written part by part
docs/spikes/2026-10-05-npcs-and-combat-results.md             the results
spikes/block-render/
  scripts/BlockRender.reds   the redscript module: the NPC list, hits, V's damage, god mode, V's death, V's weapon and
                             look in build mode, explosions
  tools/check_reds.py        the module compiled offline against the game's own scripts, with and without Codeware's
  src/GameScript.*           the module's calls from C++, each found and checked by name and type once
  src/Natives.*              BlockRender's natives, which the module's wraps and List call
  src/NpcRegistry.*          the NPCs the wraps report, in attach order, behind a lock (pure)
  src/CombatInbox.*          hits on V offered and our hits landed, from the pipeline's wraps, behind a lock (pure)
  src/ScriptTypes.*          the type checks GameScript and Puppet share
  src/link/Actors.*          NPC IDs, the nearest 256, their records, and the actor table (pure)
  src/link/Combat.*          Minecraft's hits, V's hurts, deaths and blasts: amounts, encodings, sums, windows (pure)
  src/physx/Characters.*     NPCs' capsules, for their sizes
  src/link/Link.*            + kEvHitActor and kEvPlayerDied to the sink, the actor table, kInHurt
  src/Plugin.cpp, Requests.*, Status.*   + the requests, the status lines and the frame order
  tools/mclink/              + the actor, actor-remove and hurt commands, and events printed
  tools/blockrender_hud/init.lua         + the NPC and combat lines and buttons
  tests/                     + the new unit tests
```

The names are proposals; the plan settles them.

## Build

- **The libraries:** `render_core` gains the pure parts, `link/Actors` and `link/Combat`, and `physx/Characters`,
  which calls PhysX only through its tables, as `Export` does. The plugin's own sources gain `GameScript` and
  `ScriptTypes`, beside `Puppet`.
- **The script:** the install copies `scripts/BlockRender.reds` into the plugin folder's `Scripts/`, and BlockRender
  adds that folder to redscript's compile at load.
- **The versions:** Part 1 deploys 0.17.0, Part 2 0.18.0 (0.18.1 with step 4's fixes), and Part 3 0.19.0, in the log
  line and in `Query`, which from now on takes its version from `kVersion`.

## The frame

After `UpdateLink`, in `OnUpdate`, while linked and V is found:
1. Minecraft's hits, deaths and blasts, as the sink collected them during the link's update, go through `Combat` in the
   ring's order (a blast before its own hits), and with combat on, to the module.
2. The module lists the NPCs, and the actor table is written.
3. With combat on, V's hurts since the last frame go out as `kInHurt`.

## Part 1: NPCs in Minecraft

### The module's list

- **Kept by the wraps:** the module wraps `NPCPuppet.OnGameAttached` to add each NPC to the list, and `OnDetach` to
  take it off. So every NPC the game puts in the world is known without a search each frame: people, drones, mechs,
  robots and animals alike, crowds included.
- **The list is BlockRender's, behind a lock:** the game calls those callbacks on several threads at once, so the wraps
  touch nothing of the module's. They pass the NPC's `EntityID` to two of BlockRender's natives, which keep the IDs in
  the order attached, behind a mutex (`NpcRegistry`): a detach takes its ID out without moving the others, and an ID
  already there stays where it is. Each call, `List` copies the list through two more natives and keeps what it read of
  each NPC in step with it, on the main thread, finding each NPC again by its `EntityID` (`FindEntityByID`). The
  natives count their calls by thread, for the log. (Step 2's first run crashed while the wraps kept the list in the
  module's own arrays.)
- **Each frame,** BlockRender asks the module for the NPCs within 80 m of V in a straight line (SkyCraft's 80 blocks, at
  a metre a block) that aren't dead. For each, the module gives:
  - every frame: its `EntityID`, position and forward vector;
  - read again at most four times a second, by the game's sim time: its health and max health, level, name and flags.
    Each NPC's first read puts its next at a random point of the quarter second, so a crowd isn't read in one frame.

  With them come V's punch and V's max health, read four times a second: Part 2 uses them, and `npcs` logs them in
  Part 1, so step 2 compares the punch with the inventory's number.
- **The fallback:** `TargetingSystem.GetTargetParts`, searching for NPCs as the game's own effectors do (`TSF_NPC()`
  over the complete set; the HUD's nameplate search keeps only quickhackable targets). Step 2's opening check compares
  the two in a crowd; if the wraps miss NPCs, the search takes over.
- **What crosses to C++:** the module fills one object of its own, kept between frames, with parallel arrays of plain
  types (`EntityID`, `Vector4`, `Float`, `Int32`, `String`). BlockRender reads its fields by name through RTTI, each
  field's type checked once, as `Puppet` checks functions. The module's static functions are found among the game's
  global functions, as `BlockRenderNpcs::Version;` and `BlockRenderNpcs::Current;`, where the game keeps a script
  class's static functions; its member functions and fields are found in its class. The other way, the module calls
  BlockRender's natives (above): `BlockRenderNpcAttached(id)`, `BlockRenderNpcDetached(id)`, `BlockRenderNpcCount()`
  and `BlockRenderNpcAt(i)`.
- **A new system after every load:** a load makes a new game session, and with it a new module system, so BlockRender
  finds the session's system on every call, through the module's static `Current()`, which uses the game's global
  `GetGameInstance()`.

### Records

- **IDs:** BlockRender numbers NPCs from 1 as it first lists them, keyed by their 64-bit `EntityID`, and never reuses a
  number in a session. An NPC unlisted for a minute is forgotten, and gets a new number if it comes back. A hit's
  formId maps back to the `EntityID`.
- **Which:** the nearest 256 within 80 m, by distance from V.
- **Feet:** the NPC's position, its feet as V's is, in Minecraft's axes (`McFromWorld`); with a capsule found, the
  lower of that and the capsule's bottom.
- **Yaw:** from its forward vector, by `LookFromForward`, against 0, so it stays between -180 and 180. The last yaw
  would keep a turning yaw from wrapping, which matters for Minecraft's hand sway as V turns but not for an invisible
  stand-in, and stand in for a vertical forward vector, which an NPC's never is.
- **Size:** from its capsule (below); until one is found, or without one, 0.6 by 1.8, the stand-in's own default.
- **Health:** current over max; 0 when the max isn't over 0.
- **Level:** its `PowerLevel`, rounded and held to 0 to 65,535.
- **Name:** its display name, cut to at most 23 bytes at the start of a character, then a NUL.
- **Flags:** hostile when its attitude towards V is `AIA_Hostile`; in combat by `NPCPuppet.IsInCombat`; essential when
  it has an `Immortal` or `Invulnerable` god mode. Dead NPCs aren't listed.

### Sizes

- **Looked for** when an NPC is first listed, and again every 2 s until one is found, at most eight a frame, those never
  looked for first, nearest first: an overlap of a 1 m sphere centred 1 m above its position, keeping kinematic
  actors' capsules that are a character controller's, with the query filter's third word holding the controller bit
  `0x2000` (V's `0:0:2000:0`; a vendor's `0:0:10002000:0`), that lean at most 25 degrees from upright and whose axis
  passes within 0.2 m of its position; of several, the narrowest. An NPC has other kinematic capsules around it, one of
  radius 0.06, so the narrowest of all would be wrong.
  The retries are for an NPC the game gives a capsule only near V, or a little after it appears, since NPCs are first
  listed up to 80 m away. An NPC forgotten with its number is looked for afresh if it comes back.
- **Its box:** twice the controller's radius wide, from the lower of the NPC's position and the controller's bottom, up
  to the top of the tallest capsule standing on the NPC's axis as its controller does (leaning at most 25 degrees, its
  axis within 0.2 m of the feet), whatever its filters: the controller stops at the shoulders. A controller's capsule
  doesn't reach the feet either: V's, centred 1 m above V's feet with half-height 0.4 and radius 0.32, spans 0.28 to
  1.72 m above them.
- **Which capsule is an NPC's,** by its filters, was step 2's opening question (spike 5's risk 4): `shapes` beside a
  vendor showed its controller (radius 0.30, 1.45 m tall) with the controller bit, and two other kinematic capsules
  without it.

### The table

- **Written every frame** while linked, with V found, `npcs` on and the module found, as SkyCraft's `WriteActors`
  writes it: the sequence made odd, a release fence, the count and records, then the sequence made even with release.
- **Emptied once** when any of those stops: unlinking, a load, `set npcs 0`, the module gone.

### The link

`kEvHitActor` and `kEvPlayerDied` go to the sink, as `kEvExplosion` does. In Part 1 a hit is only logged: its NPC's ID
and name, Minecraft's damage, the flags and the weapon class.

### Requests and status

- `set npcs 0|1` (on by default: listing NPCs changes nothing in the game); `npcs` logs V's punch and max health, every
  listed NPC (ID, name, distance, size and where it came from, health, level, flags), and, for step 2's comparison, how
  many NPCs `GetTargetParts` finds within 80 m and which of them the list lacks.
- **The status's NPC line:** listed, and in the world; the nearest; capsules found; IDs given; the module's time a
  frame; Minecraft's hits logged; and why none are listed, while none can be.

## Part 2: combat both ways

### Minecraft's hits on NPCs

- **Which hits:** a `kEvHitActor` whose formId was listed in the last 2 s (a hit can trail its stand-in's removal), with
  damage that's finite and over 0. A hit with only knockback is skipped. The damage is capped at 1,000, since `/kill`
  on a stand-in sends 3.4 × 10³⁸.
- **Where it starts:** V's punch × Minecraft's damage to the power `p`, `set hitpower` (0.5 by default, from 0 to 1).
  At 0.5, the square root, a fist (1) starts at one punch, a diamond sword (7) at 2.65 punches and an axe (9) at 3; at
  1, as step 4 had it, each point of Minecraft's damage is a whole punch, and a sword one-shot level-47 gangers; at 0,
  every hit is one punch. Minecraft's own hurt cooldown (a target takes a new hit only after 10 of its ticks) keeps weak
  spammed hits from beating timed ones.
- **Strong:** a crit, or knockback over 0.45, as SkyCraft staggers; the rest are quick.
- **What V punches with:** what V's unarmed slot draws: V's arm cyberware weapon if it has a melee attack to make hits
  with, otherwise the fists or Gorilla Arms (`BaseFists`). V's punch is its damage per hit, the inventory's number.
- **The hit** (the module's `Hit`):
  - the NPC is found again by `FindEntityByID`; if it's gone or dead, the hit is dropped and counted;
  - it's built as `SendHitEvent` builds one: an attack from V, as its instigator and source, with the attack record of
    what V punches with, from its item's `WeaponItem_Record.Attacks()`: its `StrongMelee` attack for a strong hit, its
    light one, `Melee`, otherwise (`QuickMelee` is a gun's quick melee), of neither its first melee attack; and the
    weapon object of what V punches with if V has it drawn (the item in `AttachmentSlots.WeaponRight`), otherwise none;
    the NPC as its target; its position at the NPC's middle, with a hit shape there; its direction from V; then
    `QueueHitEvent`;
  - it's marked as ours on the hit itself, before it's queued: `hitFlag.None` under the source name `n"BlockRender
    <start>"`, as the game marks its environment's attacks with `hitFlag.None` and a source name. The pipeline's wraps
    read the mark back from the hit, on whichever thread runs the pipeline, so nothing keeps the hit by reference.
- **In the pipeline, as V's own punch:**
  - **`ProcessPipeline` is wrapped,** the pipeline's entry, before even its first stage's callbacks: our hit's damage is
    set to its start, split across damage types as the attack splits its own (all physical when it has none, as without
    a weapon). Everything the pipeline does to V's punches then applies (the game's scripts' "what the pipeline does to
    V's punch"): the spread, V's crits, perks, cyberware and Berserk, the stealth bonus, story mode; the NPC's
    resistances, armour, mitigation, evasion, dodges, blocks and parries; and `ApplyDamage`'s caps, overshield and
    finisher grace. A Minecraft crit is already in Minecraft's damage, and V's crit chance rolls on top: the user's
    choice, since the one is the swing and the other V's build. After the pipeline, a hit it dealt nothing reports why
    (`BlockRenderHitEnded`), from its `DealNoDamage` flag's source name: friendly to V, invulnerable, already dead,
    dodged, evaded, killed at once;
  - **`DealDamages` is wrapped:** for our hits it reports what the pipeline made of the hit, after every modifier, the
    NPC's health before and after, and what the pipeline did to it: critical, blocked, parried, the NPC's armour,
    mitigated, killed it (`BlockRenderHitLanded`). The game's own steps run around it: its hit reaction, death and
    ragdoll, the XP, the police;
  - BlockRender counts a hit the game neither dealt nor said anything about within 2 s as not dealt.
- **The status** keeps the last hit: the NPC, Minecraft's damage, where it started, what was dealt and what the pipeline
  did to it, and the NPC's health before and after; and the hits not applied by reason, ours and the pipeline's.

### The game's hits on V

- **Taken out before they're dealt:** in build mode, with combat on, `DealDamages`'s wrap takes a hit on V's total
  health damage (`GetTotalAttackValue`), after V's armour and the difficulty, and sets it to 0 before the game applies
  it. V's health doesn't drop, so no low-health effect or perk fires. The wrap offers BlockRender each hit on V
  (`BlockRenderVHurt`): the amount, the attack type and the instigator's `EntityID`. BlockRender takes it, behind a lock,
  while combat and build mode are on, V is found and V's max health is read; one it doesn't take is V's, so none is lost
  on its way.
- **V's overshield and the game's non-lethal hits** (the deep review of Part 2): the part of a hit V's overshield would
  take stays in the game, and drains the overshield, so only the rest, what V's health would have lost, is offered. A
  hit the game won't let kill V (`CannotKillPlayer`, or a minimum health left) is offered as non-lethal, and stays V's:
  sent to Minecraft, it could kill Minecraft's player, and so V. The status counts those apart.
- **Two armours:** a hit's share is what it would cost V after V's armour, and Minecraft's armour then reduces it again
  for melee and projectile hits from an attacker in range. SkyCraft does the same (it measures the health Skyrim took).
- **Immortal**, forced by the game's override under our own source name, stays on meanwhile, as SkyCraft's essential
  flag does: a backstop if a hit on V gets past the wrap. Step 4 found an added `Immortal` doing nothing on V, whose
  three `Mortal` god modes outrank it; the override is how Second Heart keeps V alive. Health taken outside the pipeline
  isn't caught at all; the status shows V's health in build mode, so it would be seen.
- **To Minecraft, as `kInHurt`:**
  - **amount:** its share of V's max health, as the same share of Minecraft's 20: `a` = share × 20 × 5 × 100, that is
    share × 10,000, rounded, for SkyCraft's ÷100 and ÷5;
  - **kind:** melee for `QuickMelee`, `StrongMelee`, `Melee`, `GuardBreak`, `WhipAttack`, `ChargedWhipAttack` and
    `Reflect`; projectile for `Ranged`, `Thrown` and `Direct`; magic for `Hack`; other for the rest (`Explosion`,
    `PressureWave`, `Effect`, `ForceKill`);
  - **power attack** for `StrongMelee`, `ChargedWhipAttack` and `GuardBreak`; "blocked" is never set, since V can't
    block in build mode;
  - **attacker:** its formId if it's listed, else 0.

  The status shows each hurt's attack type, so step 4 checks the kinds against what actually hit V.
- **Summed:** for 10 of its ticks after a hurt, Minecraft takes from any other hurt only what's more than the first. A
  hurt goes at once when none went in the last 11 of Minecraft's ticks, by McState's `tickMs` (0.55 s at its usual 50 ms); otherwise
  it's added to a sum, which goes when those 11 ticks are up. The extra tick covers Minecraft applying each hurt at its
  next tick, so two sends 10 ticks apart could land 9 apart. A sum takes its biggest part's kind and attacker, and a
  power attack if any part had one.
- **What Minecraft does with it:** armour and a shield reduce melee and projectile hits whose attacker has a stand-in
  within 24 blocks. The rest pass both, as `generic` or magic damage in 26.3, and only Protection reduces them. In
  creative mode nothing hurts Minecraft's player, so the game's hits on V cost nothing: combat both ways needs survival.

### Deaths

- **`kEvPlayerDied`,** with combat on, while build mode is on or has ended in the last 2 s: the module takes our
  Immortal override off and kills V through the game's own death, `RequestSettingStatPoolMinValue` of V's health, with
  the NPC behind the killer's formId as the instigator, or none. Build mode ends as soon as McState says Minecraft's
  player is dead, which can come before the event.
- **Minecraft respawns by itself:** the `immediate_respawn` game rule, set once in the user's world in step 3; it's
  saved with the world. The status shows when Minecraft's player is dead, and for how long.
- **The inventory:** a death drops what Minecraft's player carries, unless `keep_inventory` is on. That's the user's
  call for their world; the steps that die ask first.

### V's god mode

- **On** while build mode and combat are both on, whichever came first. **Off** when build mode ends, every way it can;
  when combat is turned off; and whenever V is found again after not being found, as after a load, in case a save kept
  it.
- **The game's override,** as Second Heart's: `EnableOverride(V, Immortal, n"BlockRender")` and `DisableOverride(V,
  n"BlockRender")`, since V's `Mortal` god modes outrank an added `Immortal` (step 4). After each change the module
  asks the game whether V is Immortal (`HasGodMode`), and the log says what the game says, not what was asked.
- Only ever enabled and disabled under our source name, `n"BlockRender"`, and never touched at unload.

### V's weapon and look in build mode

The deep review of Part 2 added these, at the user's choice.
- **V's unarmed weapon, drawn:** while combat and build mode are both on, the module draws what V's unarmed slot draws
  (`Arm`): the equipment system's request for weapon-wheel slot 4, at once (`Instant`), asked again each second until it's
  drawn. So hits carry it as their weapon, as V's own punches do: the NPC's armour, the weapon's crits, the stealth
  bonus, blocks, the skill XP and the perks that check the weapon all need it. When either ends, V's own weapon is put
  back: the wheel slot it was in, or holstered if V held nothing, or something not on the wheel. A load starts V as its
  save left it.
- **V hidden:** while build mode is on, with `set hidev 1` (the default), every visual component of V's, and of each item
  V wears and holds, that's on is switched off and remembered (`HideV`), again every quarter second so that what appears
  meanwhile, the drawn weapon, goes too; when build mode ends, exactly those are switched on again, so what the game had
  off stays off. Build mode then shows only Minecraft's view: no hands, no weapon, no legs, no shadow of V's. Codeware
  lists the components; the module's parts that use it compile only where it's installed
  (`@if(ModuleExists("Codeware"))`), and without it V just isn't hidden, and the status says why.
- **V's effects and the game's crosshair, hidden with V** (step 4 saw both with V hidden). Before it switches an effect
  spawner off, `HideV` stops each of its effects by name (`entKillEffectEvent`, all loops), and remembers those that
  start by themselves (`isAutoSpawn`); showing V again switches the spawner on and starts those again on objects still
  there (`entSpawnEffectEvent`). And it hides the HUD's crosshair container as the scanner does, its root widget off,
  found among the HUD layer's game controllers; again at each quarter second, since the game may show it; showing V
  shows it again, if `HideV` hid it. The rest of the game's HUD stays: the user chose the crosshair only.

### Combat on and off

- `set combat 0|1`, off until step 4 passes. Off, hits are logged and not applied, as in Part 1; V's damage is V's;
  there's no god mode; and V's unarmed weapon isn't drawn.
- `hit <id> <damage> [strong]` acts as if Minecraft hit that NPC for `<damage>` of Minecraft's, for step 4's opening
  check. `punch` logs V's punch, what it's read from, the attack records, and whether V has it drawn.
- `set hidev 0|1` keeps V seen in build mode (0), or hides it (1).
- `set hitpower <0 to 1>` sets where a hit starts: V's punch × Minecraft's damage to that power (0.5 by default).

## Part 3: explosions

### The blast in the game

For each `kEvExplosion`, with combat on, its centre goes into world axes (`WorldFromMc`), and its reach is twice its
radius, in metres, as SkyCraft's is. The module (`Explode`) makes:
- **the push:** the game effect `physicalImpulseSphere` at the centre, the reach as its radius, as
  `SpawnPhysicalImpulse` runs it. It runs a quarter of a second after the blast, as SkyCraft waits, so the bodies of
  NPCs the blast's hits killed are ragdolls by then, and it throws them too;
- **knockdowns:** an explosion attack at the centre with the reach as its radius, as `SpawnAttack` makes one, from V and
  flagged `DealNoDamage`. Its hit on V stops at `CheckForQuickExit`, and the game's hit reactions knock down or ragdoll
  the NPCs it catches;
- **the look:** an explosion effect at the centre.

Which attack record and which effect, and whether a hit that deals no damage still knocks an NPC down, are for Part 3's
planning and step 5's opening check.

### Blast hits

Hits within 0.15 s after a blast, on NPCs within its reach by their last listed feet, are blast hits. The module applies
them as hits of type `Explosion`, their direction from the blast's centre. So an NPC the blast kills dies of an
explosion, and the game throws its body.

### Fallback

If a hit that deals no damage doesn't knock NPCs down, the module knocks down each NPC within reach itself, as SkyCraft's
`KnockExplosion` does; with which of the game's calls, planning finds.

### Request

`explode <metres> <radius>`: a blast that far ahead of V, at the height of V's feet, with a radius as Minecraft gives one
(4 for TNT), as if it were Minecraft's, for step 5's opening check.

## The steps

Each step ends with what the user saw, and the agent's reading of the status, the logs and the measurements. Each
in-game step also checks that the overlay, build mode and the bodies still work.

### Step 1: `mclink`, without the game (Part 1)

The user starts Minecraft, and the agent runs `mclink` scripts. Checks:
- **Stand-ins:** three made-up records near the stand-in V give three stand-ins: Minecraft's log says `SkyCraft: 3
  Skyrim actors mirrored as hittable stand-ins`, and `/execute if entity @e[type=skycraft:skyrim_actor]` counts them.
- **Hits:** in build mode, looking at a stand-in 2 blocks away, a left click brings back `kEvHitActor` with its formId
  and 1, a fist's damage; with a diamond sword, given by command, 7 once the cooldown is full.
- **A plate:** a stone block with a stone pressure plate on it and a redstone lamp beside the plate, all set by command,
  so the plate stands whatever the ground under it. A record moved onto the plate, its feet on the block's top, lights
  the lamp (`/execute if block … minecraft:redstone_lamp[lit=true]`); moved off, the lamp goes out.
- **Removal:** a record removed removes its stand-in, and the table emptied removes them all.

### Step 2: in the game, NPCs in Minecraft (Part 1)

BlockRender 0.17.0. The opening check, with the least code:
- the module compiles (redscript's log lists it), and BlockRender finds its system and functions (its log names them);
- in a crowd, the wraps' list against `GetTargetParts`;
- `shapes` beside an NPC: its capsule and filters, for sizes;
- the module's time a frame.

Then:
- **a plate:** a stone pressure plate with a lamp, on a busy pavement, lights as NPCs cross it. The stand-ins are
  invisible, so the plate is what shows them;
- **hits:** sword hits on an NPC are logged with its ID and name, and not applied;
- **sizes:** `npcs` beside a drone, and a mech if one is found.

### Step 3: `mclink`, Minecraft's player hurt (Part 2)

- **Setting up:** `/gamerule immediate_respawn true`, once, in the user's world, and Minecraft's player in survival
  (`/gamemode survival`), set back afterwards. With the user's agreement, `keep_inventory` on for the deaths, so nothing
  is dropped, and set back after.
- `kInHurt`s of known shares and kinds: Minecraft's log line `SkyCraft: Skyrim hit the player for …: health … -> …`
  shows each. With armour on, put on by command, a melee hit from a stand-in within 24 blocks is reduced, and an `other`
  hit isn't.
- A burst of hurts, summed by `Combat` every half second, all land.
- `/kill` brings back `kEvPlayerDied`, and Minecraft's player respawns at once.

### Step 4: in the game, combat (Part 2)

BlockRender 0.18.0, with `set combat 1`, and Minecraft's player in survival. The opening check:
- what V punches with, as the module reads it, against what the game's unarmed slot draws (V has Mantis Blades);
- build mode draws V's unarmed weapon and hides V, and leaving it puts V's own weapon back and shows V;
- `hit N 1` on an NPC lands as V's own strike with the same weapon does, on the same NPC, with the game's damage numbers
  on: what the pipeline deals in the same range, and its health before and after (and whether the change shows at once);
- one hit on V in build mode: V's health unchanged, Minecraft's down by the share.

Then:
- fights with a sword, an axe, a bow and fists: flinches, staggers on crits and sprint hits, what the pipeline does
  (V's crits, the NPC's armour, blocks, dodges), NPCs fighting back, deaths and XP;
- combat off in build mode: V's own weapon back, V still hidden;
- shot at in build mode: Minecraft's hearts drop in the HUD, and V's health bar doesn't move;
- dying in Minecraft kills V, with `keep_inventory` as the user chose in step 3; after the load, V isn't Immortal: a
  hit outside build mode takes V's health.

### Step 4's fixes, in the game (Part 2)

BlockRender 0.18.1, with step 4's three fixes ("Changes from step 4"). The opening check:
- in build mode with combat on, the game says V is Immortal (our override), and out of it, that V isn't;
- in build mode, the blades' crosshair and their heat effect are gone with V, and back when build mode ends;
- `hit N 1` starts at one punch, and a diamond sword's hit at 2.65 punches.

Then the fights again, now that Minecraft's weapons don't one-shot: blocks, dodges, the NPC's armour, staggers on crits
and sprint hits, and the skill XP; and `set hitpower` at another value, to feel the difference.

### Step 5: in the game, explosions (Part 3)

BlockRender 0.19.0. The opening check: `explode` beside an NPC (knocked down?) and beside a parked car (pushed?), and the
effect at the point. Then TNT among NPCs on a pavement, by a parked car, bins and boxes:
- NPCs at the edge are knocked down, those in the middle killed and thrown;
- the car and the props are pushed, and the effect shows where the TNT was;
- V and Minecraft's player, in survival, are hurt only by Minecraft's own blast.

### Step 6: the results

`docs/spikes/2026-10-05-npcs-and-combat-results.md`, with the spike's answer at its top.

## Requests

As in spike 5: one command per file in `blockrender.request`, written atomically and checked once a second, from the
plugin's folder or the overlay's. The new commands:

| Command | Does |
|---|---|
| `set npcs 0\|1` | NPCs into the actor table (1) |
| `npcs` | Logs V's punch and max health; every listed NPC: ID, name, distance, size and where it came from, health, level, flags; and how many NPCs `GetTargetParts` finds within 80 m, with those the list lacks |
| `set combat 0\|1` | Minecraft's hits on NPCs, the game's hits on V to Minecraft, V's god mode and unarmed weapon, deaths and blasts (0 until step 4 passes) |
| `hit <id> <damage> [strong]` | As if Minecraft hit NPC `<id>` for `<damage>` |
| `punch` | Logs V's punch, what it's read from, the attack records, and whether V has it drawn |
| `set hidev 0\|1` | V's meshes hidden in build mode (1, the default) or seen (0) |
| `set hitpower <0 to 1>` | Where a hit starts: V's punch × Minecraft's damage to this power (0.5, the default: a fist one punch, a diamond sword 2.65) |
| `explode <metres> <radius>` | A blast that far ahead of V, as if it were Minecraft's |

The overlay gains the NPC and combat lines, and two buttons, NPCs and Combat; and, under "More controls", ones that
send `npcs` and `punch`, and one that switches `set hidev`. Its build line says whether V is hidden.

## Measurements and the status

- **NPCs:** on or off; listed, of how many in the world; the nearest (distance, name); capsules found; IDs given; the
  module's time a frame, on average and at worst; and why not, if the module isn't found.
- **Combat:** on or off; V's punch, what it's read from, and whether V has it drawn, or what hits skip without it; the
  hit power; hits queued, landed, and not applied by reason, ours and the pipeline's; the last hit (the NPC, Minecraft's
  damage, where it started, what was dealt and what the pipeline did to it, its health before and after); V's hurts
  taken, sent, summed, left to V and left to V as non-lethal, and the last (amount, share, Minecraft's damage, kind,
  attack type, attacker); Immortal on or off, as the game last said; deaths; Minecraft's player dead, and for how long;
  and why combat can't do all it should, while it can't.
- **V's look:** hidden in build mode, with the meshes switched off, the effects stopped and whether the crosshair is
  hidden; or seen, and why.
- **Blasts:** how many; the last (centre, reach, NPCs within it, blast hits).
- **Frame rates:** with NPCs and combat on and off.

## Safety

- **No faults,** as in every spike, since the game's crash handler freezes the game on any:
  - the module's system and functions are found by name and type once, as `Puppet`'s are; a failed check keeps NPCs
    and combat off, and the status says why;
  - `GameScript` checks every handle before using it, and the module checks every object with `IsDefined`;
  - NPCs' capsules are read through the audited tables, under scene 0's read lock and the checks the exporter's reads
    use.
- **Main thread:** every call is made in `OnUpdate`, as `Puppet`'s are, and none while V isn't found. The natives are
  the exception: the wraps call them on whichever thread the game runs `NPCPuppet`'s callbacks and the damage pipeline
  on, and they only take a lock (the registry's, or the combat inbox's); the module's state is touched on the main
  thread alone.
- **Minecraft is untrusted:** a hit's formId must be one we listed, and its damage finite, over 0 and capped; an
  `EntityID` is found again (`FindEntityByID`) before every use.
- **V's god mode:** the game's override, only under our source name, and disabled whenever build mode ends, combat goes
  off, or V is found again.
- **V's weapon and meshes:** V's own weapon is put back when combat or build mode ends; only the meshes the module
  switched off are switched on again, only the effects it stopped that start by themselves are started again, and the
  crosshair is shown again only if the module hid it; none of it is saved, so a load starts V as its save left it.
- **Codeware is optional:** the module's parts that use it compile only where it's installed, so a missing Codeware
  can't fail the compile that every mod's scripts share (risk 9); `check_reds.py` compiles the module both ways.
- **Off by default:** combat, until step 4 passes. NPCs are on, since listing them changes nothing in the game. Hiding V
  is on, since it changes only what's drawn, and only in build mode.
- **Builds:** a deploy runs only when `tasklist` shows no `Cyberpunk2077.exe`.

## Error handling

- **The module isn't there, or a function's types differ:** NPCs and combat stay off, logged once, and the status says
  which.
- **A hit for an unknown or stale formId, or an NPC gone or dead:** dropped, and counted.
- **V's punch can't be read, or is 0:** hits aren't applied, and the status says why.
- **V's max health can't be read:** hurts aren't sent, and the status says why.
- **V doesn't die on `kEvPlayerDied`,** say from another god mode or in a scene: logged.
- **The input ring is full:** the hurt waits for the next frame, and is counted.
- **V's unarmed weapon can't be drawn** (no unarmed weapon, a place where the game restricts weapons): logged, asked
  again each second, and the status says the weapon isn't drawn, and what hits skip without it.
- **Codeware isn't installed:** V isn't hidden, nor V's effects and the crosshair, and the status says why.
- **The crosshair container isn't found** among the HUD layer's game controllers: V is hidden without it, and the
  status says the crosshair isn't hidden.

## Testing

### Unit tests (ctest, no game)

- **Part 1:** the IDs (from 1, kept, never reused, forgotten after a minute); the nearest 256 within 80 m; a record from
  an NPC (the axes, the yaw, the size and its feet, health, level, flags, and a name cut at the start of a character);
  the table byte for byte, its sequence order, and emptied once when it stops; `kEvHitActor` and `kEvPlayerDied` reaching
  the sink; the wraps' list (attach order kept through detaches, repeats and strays counted, at most 1,024, a copy for
  `List`, calls counted by thread, and eight threads at once while another copies it); a capsule's box (from the NPC's
  feet, or the capsule's bottom if lower, to its top; the controller's, by its query filter bit, the narrowest of
  several); `mclink`'s new commands parsed.
- **Part 2:** which hits count (listed, finite, over 0, capped); the amount; strong or quick; the kind and power flag
  from each attack type; the `kInHurt` encoding; the sums, 11 of Minecraft's ticks apart at `tickMs`, with their kind
  and attacker; death's 2-second window; god mode on and off, whichever of build mode and combat comes on first, and off
  once when V is found again; the combat inbox (taking only while told to, at most 256 waiting, and eight threads at
  once); the box's top from the tallest capsule; `mclink`'s `hurt`; `hit` and `punch`; and the status's combat line.
  Since the deep review of Part 2: what the pipeline did to our hits and why it dealt some nothing, in words; the inbox's
  endings and non-lethal hurts; V's switches for its weapon and meshes (on, off, refreshed, and lost with V); and the
  status's v line. Since step 4: where a hit starts by the hit power (0, 0.5, 1; a fist, a sword, the cap); and the v
  line's effects and crosshair.
- **Part 3:** the blast window (0.15 s, the reach) and the centre in world axes.

### `mclink`

Steps 1 and 3, against the user's Minecraft. The new commands: `actor <id> <x> <y> <z> [yaw width height name]`,
`actor-remove <id>|all` and `hurt <kind> <share> [attacker] [power]`, which goes through `Combat`'s sums and encoding;
and events printed as they arrive.

### In the game

Steps 2, 4, step 4's fixes, and 5. The module's code runs only in the game.

## Risks

1. **BlockRender can't call the module, or read what it fills.** Step 2's opening check comes first. The fallback is a
   global script function, found as `Puppet` finds `GetMountedVehicle`.
2. **The wraps miss NPCs,** such as ones attached before the module's system was made. `GetTargetParts` replaces them.
3. **Our hit doesn't land as V's own:** it might not reach `DealDamages`, or the start set at the pipeline's entry might
   not hold, if the game filled the hit's damage in later. Step 4's opening check compares it with V's own strike on the
   same NPC. The last resort is taking the amount off the NPC's health directly (`RequestChangingStatPoolValue`), which
   loses the hit reaction and every modifier.
4. **Scripted hits don't make NPCs react or fight back.** Accepted if the damage lands; the attack's hit reaction
   severity can be set (`SetHitReactionSeverityMin` and `Max`).
5. **Something takes V's health outside the pipeline.** That damage stays on V, and isn't sent to Minecraft; the status
   shows V's health in build mode, so step 4 would see it.
6. **Killing V doesn't bring the game's own death.** Step 4 tries it. The fallback is a hit flagged `Kill` from the
   killer, as the game kills a vehicle's driver.
7. **A save keeps our Immortal.** It's disabled whenever V is found again, so a load clears it.
8. **The damage pipeline runs off the main thread,** racing BlockRender's calls into the module. Since step 2, which
   found the game's script callbacks running on worker threads, it's taken to: the pipeline's wraps read only the hit
   and call BlockRender's natives, behind a lock. Step 4 watches for hits lost or doubled.
9. **redscript fails to compile BlockRender's script** at the game's start, which may take the other mods' scripts with
   it. The plan compiles it against the game's own scripts before deploying, through redscript's compiler library
   (`tools/check_reds.py`), and the first deploy is still checked by starting the game to its menu and reading
   redscript's log, since the other mods' scripts are compiled with it there.
10. **Minecraft's other damage reaches NPCs as V's:** a skeleton's arrow or the user's TNT hits a stand-in, and
    `kEvHitActor` doesn't say who did it. Accepted, as SkyCraft does.
11. **Not every NPC has a capsule:** a drone's or a mech's collision may be something else, and it stays person-sized.
12. **The list costs too much in crowds.** Measured in step 2. The slow fields are read at most four times a second an
    NPC.
13. **A hit that deals no damage doesn't knock NPCs down** (Part 3): the fallback above.
14. **Accepted limits:** Minecraft's armour doesn't reduce hits from attackers more than 24 blocks away, or blasts;
    Minecraft's player walks through NPCs, and can hit a driver through a car, since cars aren't solid for Minecraft;
    NPCs walk through blocks. Minecraft's arrows land as V's melee hits, so an NPC's melee block and resistance apply to
    them too, and its facing against V's decides a block, not Minecraft's player's.
15. **V's unarmed weapon isn't drawn in build mode,** where V is placed by teleports every frame, or the game refuses it
    there. Step 4's opening check sees it in `punch` and the status; without it, hits still land, but skip the NPC's
    armour, the weapon's crits, stealth, blocks and the skill XP, and the status says so.
16. **Hiding V misses something, or switching meshes off has a side effect** (V's shadow, a visual the game needs on).
    Step 4's opening check looks around and down in build mode, and back out of it; `set hidev 0` turns it off.
17. **The NPC's health shows a hit's change only later,** if the game applies the drain after `DealDamages`. Step 4
    found it does: a landing's "after" is the health before it, and the next hit's "before" shows the change.
18. **The override doesn't make V Immortal either,** or it clashes with Second Heart's, which uses the same mechanism
    under its own source name. The fixes' opening check reads what the game says in build mode; if V isn't Immortal,
    the damage wrap stays V's only guard, as it was in step 4, where it held for all 42 hits.
19. **The heat isn't a spawner's effect,** or a spawner switched off ignores the kill. The fixes' opening check looks
    for it with V hidden, and the log counts the effects stopped; if it stays, the module's next try is stopping the
    effects without switching their spawner off.
20. **The crosshair container isn't among the HUD layer's game controllers.** The status says the crosshair isn't
    hidden; the next try is the other layers Codeware lists.

## Found while writing

Writing the spec out against the sources added these to what the brainstorm agreed:
- **Minecraft's armour and shields** reduce only SkyCraft's melee and projectile hits with an attacker in range, since in
  26.3 `generic` and magic damage pass both. Part 2's presentation said armour applied to everything.
- **`/kill` on a stand-in** sends 3.4 × 10³⁸ damage (`LivingEntity.kill`): hence the cap.
- **SkyCraft keeps stand-ins 8 s** after the game's heartbeat stops, not the 3 s BlockRender counts Minecraft gone after.
- **`Query`'s version lags** the log's: 0.15.0 against 0.16.0.
- **Build mode ends on Minecraft's death** before `kEvPlayerDied` can arrive (`Router.cpp`): hence the 2-second window.
- **`EntityID.GetHash` is 32 bits,** so IDs cross to C++ as whole `EntityID`s.
- **A hit with no damage of its own is marked `n"no_valid_damage"`** before `Process`, which would skip `DealDamages`:
  hence the `Process` wrap.
- **The game rule** for respawning at once is `immediate_respawn` in 26.3.
- **What V punches with** is whatever is in the `BaseFists` area, Gorilla Arms included.
- **SkyCraft sends a hit with knockback but no damage,** which is skipped.

## Changes from the deep review

The review traced each claim above to its source, and walked through steps 1 to 5 as the user would play them.

### What changed

1. **Creative mode:** a creative player takes no damage (`Player.hurtServer` refuses while `abilities.invulnerable`),
   so the game's hits on V would cost Minecraft's player nothing. Part 2 says combat both ways needs survival, and steps
   3 to 5 put Minecraft's player in survival.
2. **The inventory:** a death in Minecraft drops what its player carries unless `keep_inventory` is on. Part 2 and step
   3 leave that to the user, asked before the deaths are tested.
3. **A new module system after every load:** a load makes a new game session, so BlockRender looks the system up again
   whenever V is found again (Part 1).
4. **Step 2's comparison had no request:** `npcs` now also logs what `GetTargetParts` finds within 80 m, and which of
   those the list lacks.
5. **Immortal was over-claimed:** it was said to guard against health taken outside the damage pipeline, which nothing
   read supports. It's a backstop for hits that get past the wrap; risk 5 now says health taken outside the pipeline
   stays on V and shows in the status.
6. **The hit's weapon object** is what V punches with, when V has it drawn, otherwise none: not any weapon V holds,
   which could be a gun.
7. **Step 1's plate** stands on a stone block set by command, so it holds on any ground, and the record's feet are on
   the block's top.
8. **Step 4:** V's health bar doesn't move in build mode; it was said to stay full, which it needn't be.
9. **The kinds of hurt:** nothing read shows what uses `Direct`, so the status shows each hurt's attack type and step 4
   checks the mapping against what hit V.
10. **The frame:** Minecraft's hits go through `Combat` always, and to the module only with combat on; V's hurts are
    sent only with combat on.

### What was checked

- **SkyCraft** (`bfcaf17`): the stand-ins' lifetime (`HEARTBEAT_TIMEOUT_MS` 8000), `readActors`' seqlock, `sync`,
  `stepOnTriggers`, the entity type's settings, `actuallyHurt`, `knockback`, `takeHit` and the event's fields,
  `markCritical` from `ServerPlayerMixin.crit`, `hurtPlayer` and `InputBridge.hurt`, `ServerPlayerMixin.die`,
  `ServerExplosionMixin`, `PressurePlateMixin` and `BlockStateBaseMixin`; on the game side, `WriteActorTable`,
  `Link::WriteActors`, `ApplyHit`, `BridgePlayerDamage`, `KillPlayer` and `ApplyExplosion`. Only the formId, feet, yaw,
  size, name and dead flag of a record are read in Minecraft (a search of the whole Fabric source).
- **The game's scripts** (decompiled 2.31): every declaration named, read in its file: `Entity`'s and `GameObject`'s
  getters, `NPCPuppet`'s callbacks and `IsInCombat`, `StatsSystem`, `StatPoolsSystem`, `GodModeSystem`,
  `TransactionSystem`, `gameItemData`, `EquipmentSystem.GetItemsInArea` and the fists' handling, `GameInstance`'s
  system getters and `FindEntityByID`, `EntityID.GetHash`, `IAttack.Create`, `AttackInitContext`, `AttackData`,
  `gameAttackComputed`, `gameHitEvent`, the `hitFlag`, `gamedataAttackType`, `gameGodModeType` and `EAIAttitude` enums,
  `DamageSystem`'s pipeline (`ProcessPipeline`, `PreProcess`, `ModifyHitData`, `CheckForQuickExit`, the god-mode
  checks, `Process`, `ProcessRagdollHit`, `DealDamages`), `SendHitEvent`, `OnStartRagdollDamageEvent`,
  `ExplosiveDevice.Explode` and `DoAttack`, `CombatGadgetHelper`, `RPGManager.PrepareGameEffectAttack`,
  `ScriptableSystem`, `NavigationSystem.AddObstacle`. A first summary of two files by a web fetch didn't match their
  text in one of the two; every line used here was read from the files themselves.
- **Minecraft 26.3** (the user's jar): the damage-type tags, `LivingEntity.kill`, the game rules' names, the diamond
  sword (`ToolMaterial.DIAMOND`'s bonus 3.0 and `sword(DIAMOND, 3.0F, -2.4F)`), `ServerExplosion.explode`'s order, and
  `Player.hurtServer`'s refusal in creative.
- **BlockRender** (`980c2e5`): the frame in `Plugin.cpp`, `Link::Update`'s events, `Mapping.cpp`'s clearing,
  `Router::Update` ending build mode when Minecraft's player is dead, `Look.h`, `Export.h`, and `Query`'s version.
- **RED4ext and redscript:** `Scripts::Add` in the SDK; `ent::EntityID` is one `uint64_t`; redscript's log lists plugin
  folders' scripts.
- **The steps, as played:**
  - step 1: three records give three stand-ins (SkyCraft logs counts under 5), a fist hit gives 1 and a diamond sword 7
    at full cooldown, a record on the plate presses it through `stepOnTriggers` and its box fills the plate's block;
  - step 2: a plate placed in build mode on the pavement sits in the block the pavement's surface is in, which a
    stand-in's box, from the feet up, overlaps;
  - step 4: a hit on an NPC passes `Process`'s wrap and `DealDamages`'s; the NPC's return fire reaches `DealDamages` with
    V as its target; Minecraft's death ends build mode at once, inside the 2-second window;
  - step 5: the knockdown attack's hit on V stops at `CheckForQuickExit`, since V is its instigator.

### Not settled by reading

Left to planning and the steps' opening checks: the attack records and the explosion effect; that the game makes the
module's system; that `RequestSettingStatPoolMinValue` brings V's death; whether a hit that deals no damage knocks an NPC
down; whether `scc.exe` can compile outside the game; and which thread the damage pipeline runs on.

## Changes from the second deep review

The user asked for a second pass. It read every section again for what the plan will need but the spec left vague,
checked the numbers against Minecraft's and the game's code, and traced the claims the first pass had taken on trust.

### What changed

1. **The NPC's box was wrong.** Its feet were to be the capsule's bottom, but a controller's capsule doesn't reach the
   feet: V's, centred 1 m above V's feet with half-height 0.4 and radius 0.32, spans 0.28 to 1.72 m. Every stand-in
   would have floated 0.28 m and been 1.44 m tall. The box now runs from the NPC's position, its feet, or the capsule's
   bottom if that's lower, up to the capsule's top.
2. **Of several capsules, the smallest:** V has a second kinematic capsule, radius 1, around its controller, and the
   rule would have taken either.
3. **The overlap's shape** wasn't given: a 1 m sphere centred 1 m above the NPC's position.
4. **The sums wait 11 of Minecraft's ticks, not half a second.** For 10 ticks after a hurt, Minecraft takes only what's
   more than it (`LivingEntity.hurtServer`: `invulnerableTime > 10.0F`, then `damage - lastHurt`), and it applies each
   hurt at its next tick, so sends half a second apart could land 9 ticks apart and lose most of the second. The wait is
   counted in McState's `tickMs`.
5. **God mode comes on whichever is turned on first,** build mode or combat. As written, turning combat on during build
   mode left V mortal.
6. **A hit is recorded as ours before it's queued,** in case the pipeline runs inside `QueueHitEvent`.
7. **Events are handled in the ring's order,** so a blast comes before its own hits, which the blast window needs.
8. **The push waits a quarter of a second** (Part 3): the blast's hits arrive just after the blast, so a push at once
   would come before the NPCs it killed are ragdolls. SkyCraft waits for the same reason.
9. **Precisions:** distances are straight-line; `kInHurt`'s "blocked" is never set; `explode`'s radius is Minecraft's;
   capsules are read under scene 0's read lock; and Minecraft's player can hit a driver through a car (an accepted
   limit).

### What was checked, newly

- **`AddFlag` keeps one entry a flag,** the first source's, and `RemoveFlag(flag, sourceName)` removes it only under
  that name (`attackData.swift`). Every other reason for `DealNoDamage` is set earlier in `PreProcess`, so the `Process`
  wrap can't remove one.
- **A hit built by script gets an `attackComputed`:** `ProcessRagdollHit` sets its amount on the one the ragdoll
  impact's hit gets.
- **`RequestSettingStatPoolMinValue` of `Health`** is the pipeline's own instant kill (`ProcessInstantKill`), and a
  security turret's way of killing itself, with a `null` instigator.
- **No script class extends `NPCPuppet`,** and only it and `PlayerPuppet` extend `ScriptedPuppet`.
- **`PowerLevel` is the level the game reads for a target** (`quickHackableHelper.swift`, `telemetry.swift`).
- **The redscript wiki's hook tutorial** wraps `DamageSystem`'s private `ProcessLocalizedDamage` with
  `@wrapMethod(DamageSystem)`, as the module wraps `Process` and `DealDamages`.
- **V's feet in BlockRender are V's `GetWorldPosition`** (`PlayerLocator.cpp`), the point spike 1 found V's controller
  centred 1 m above.
- **SkyCraft moves the client's stand-ins every frame:** `SkyClient` calls `ProxySync.frame`.
- **The steps, re-walked:** step 2's plate still lights, since the box still starts at the NPC's feet; in step 4,
  turning combat on during build mode now makes V Immortal; in step 5, the push comes after the blast's hits.

## Changes while planning

Part 1's plan (`docs/superpowers/plans/2026-10-05-npcs-and-combat.md`) changed or settled these. The sections above
already include them.
- **redscript compiles offline:** `scc_lib.dll`, redscript's compiler library in the game's `engine/tools`, has a C API
  that compiles a script folder against a given `final.redscripts` with its error popup off. `tools/check_reds.py`
  compiles the module against a copy of the game's untouched cache before every deploy, so risk 9 is checked first. Run
  alone, `scc.exe` shows a "REDScript compilation has failed" message box on the user's screen, as it did once while
  planning.
- **The module's system on every call:** its static `Current()` finds the session's through the game's global
  `GetGameInstance()`, so BlockRender needs no `GameInstance`, and a load's new system is found on the next call.
- **The module's shape:** `List(range)` fills twelve fields (eight parallel arrays, the count of NPCs in the world, V's
  punch, the item it's read from, and V's max health), and returns -1 when it doesn't find V; `Compare(range)` returns
  the comparison as a string for the log; `Version()` must be 1. Since step 2's first run: thirteen fields, with how many
  NPCs `List` found by their IDs, and version 2.
- **The yaw** is taken against 0, not the NPC's last yaw, so no per-NPC yaw is kept.
- **The slow fields** are read per NPC four times a second by the game's sim time, each NPC's first read putting its
  next at a random point of the quarter second.
- **V's punch and max health** are logged by `npcs` in Part 1, so step 2 compares the punch with the inventory first.
- **Sizes:** a capsule leans at most 25 degrees from upright to be an NPC's; NPCs are looked for nearest first, again
  every 2 s until a capsule is found (the brainstorm said once), and forgotten with their numbers.
- **The comparison's search** uses `TSF_NPC()` over the complete set, as the game's own effectors search for nearby
  NPCs, not the HUD's nameplate search, which keeps only quickhackable targets.
- **Where the C++ goes:** `GameScript` and `ScriptTypes`, the type checks it shares with `Puppet`, go in the plugin's
  own sources beside `Puppet`, not in `render_runtime`. `HitWords` moves from `mclink` to `link/Actors`, so the plugin
  logs hits in `mclink`'s words.
- **`mclink`:** an `actor` without a yaw and size faces south and is person-sized, its name is the rest of the line, and
  `actor-remove all` empties the table.
- **`Query`'s version** is parsed from `kVersion` at compile time.
- **The overlay** gains a button that sends `npcs`, beside the NPCs button.

## Changes from step 2's first run

Step 2's first run (the results, and the plan's Task 11) found three faults. Each was fixed before going on, and the
sections above include the fixes.
- **The module's static functions are global functions:** the game keeps a script class's static functions among its
  global functions, named `BlockRenderNpcs::Version;`, not with the class, so `GameScript` looks them up there
  (`da1be78`).
- **An NPC's capsule is its controller's, by the query filter's controller bit** `0x2000`, not the narrowest kinematic
  capsule around it, which beside a vendor was a 0.06 m one (`1fa17f4`).
- **The list moved into BlockRender, behind a lock:** the game crashed when Link first called `List`, on a handle in the
  module's list with no object, and once during a load before any link, in the engine's allocator. The wraps, which
  pushed onto and erased from the module's arrays, run on several threads at once, and a script array has no lock. The
  wraps now only pass `EntityID`s to BlockRender's natives, `NpcRegistry` keeps them behind a mutex, and `List` copies
  them on the main thread. The module is version 2, with a thirteenth field, `found`; the natives count their calls by
  thread, so the next run shows which threads the game uses.

## Changes while planning Part 2

Part 2's plan (`docs/superpowers/plans/2026-10-05-npcs-and-combat.md`, Tasks 13 to 24 as first planned; the deep review
of Part 2 later added Tasks 23 to 30, which made step 4 and the close Tasks 31 and 32) changed or settled these, with
what steps 1 and 2 found. The sections above already include them.
- **Our hits are marked on the hit itself,** `hitFlag.None` under `n"BlockRender <amount>"`, not recorded by reference:
  the game marks its environment's attacks the same way (`environmentDamageSystem.swift`), and `AttackData.GetFlags()`
  gives each flag's source. The pipeline may run on any thread, and its wraps then need nothing of the module's.
- **The attack record** is the `BaseFists` item's `StrongMelee` attack for a strong hit, and its `Melee` attack
  otherwise: `QuickMelee` is a gun's quick melee (`attackData.swift`'s `IsLightMelee`). The player's own melee finds its
  records by name on the drawn weapon object (`meleeTransitions.swift`).
- **The pipeline's wrapped methods** are `damageSystem.swift`'s `private final func Process(hitEvent: ref<gameHitEvent>,
  cache: ref<CacheData>)` and `private final func DealDamages(hitEvent: ref<gameHitEvent>)`; a wrap of a method that
  isn't there fails the compile.
- **V's hurts cross through a native,** `BlockRenderVHurt`, into a combat inbox behind a lock, which takes one only while
  combat and build mode are on, V is found and V's max health is read, and keeps at most 256; one it doesn't take is V's.
  Our hits' landings come back through `BlockRenderHitLanded`, with the NPC's health before and after.
- **The box's top** comes from the tallest capsule on the NPC's axis: step 2 found the controller stops at the
  shoulders.
- **The knockback threshold** compares floats, `> 0.45f`, as SkyCraft's `push > 0.45f` does.
- **`hit <id> <damage> [strong]`** makes an event as Minecraft's would: knockback 0.4, and a crit for strong.
- **V's god mode** goes off once whenever V is found again, the first time included.
- **The module is version 3,** with `Hit`, `SetImmortal`, `KillV` and `Punch`, and BlockRender has six natives.
- **Step 3's checks** read SkyCraft's own line for each hurt, `SkyCraft: Skyrim hit the player for <damage> (<Minecraft
  damage> Minecraft): health <before> -> <after>` (`SkyCombat.hurtPlayer`).

## Changes from the deep review of Part 2

After Part 2's Tasks 13 to 22 had run, and before step 4, the user asked for a deep review of Part 2's plan, naming one
hole: V's damage is changed by much the plan didn't count. The review read the damage pipeline end to end in the
decompiled 2.31 scripts, and the equipment and player state machine's code; walked step 4 through as the user would
play it, with V as step 2 found V (Mantis Blades installed, a punch of 102 from the fists, 456 max health); and checked
the plan's claims against their sources. The user then decided four questions. The sections above include the outcome;
the plan's "The deep review of Part 2" lists what was checked, and its Tasks 23 to 30 make the changes.

### What the review found

1. **V's damage lost everything the game adds** (the user's hole). The `DealDamages` wrap set our hit to exactly
   Minecraft's damage × V's punch after the whole pipeline, so the spread, V's crits, perks, cyberware and Berserk, the
   stealth bonus and story mode were lost, and so were the NPC's resistances, armour, mitigation, evasion, dodges, blocks
   and parries. V's crit roll still marked the hit critical, so its number showed as a crit with no crit damage; a
   blocked hit, which the game takes to 0, dealt it all. It wasn't even exact: `ApplyDamage`, after the wrap, still caps
   bosses' hits, lets an overshield take its part, and can leave an NPC at 1 health for a finisher. SkyCraft does the
   same in Skyrim: it overwrites the damage with Minecraft's × (5 + 0.25 × the actor's level), with nothing resisted, so
   the exact amount came from the reference, not from the game.
2. **What V punches with was left open.** Step 2 said "Part 2 decides what V punches with when arm cyberware is
   installed", and Part 2 didn't: the module read the fists in `BaseFists`, while the game's unarmed slot draws V's arm
   cyberware when V has it. And holstering doesn't draw the fists: an attack while holstered draws the last weapon. So
   step 4's punch check would have failed for this V.
3. **Hits carried a weapon only by chance.** The hit carried V's weapon only if the fists were drawn, which for this V
   never happens; armour, the weapon's crits, the stealth bonus, blocks, the skill XP and the perks that check the
   weapon all skip a hit without one. Build mode never touched V's weapon.
4. **The `Process` wrap let dodged and evaded hits land.** This spec said every other reason for `DealNoDamage` came
   before `no_valid_damage`; instant kills, dodges and evasion come after it, and `AddFlag` keeps one entry a flag, so on
   a hit without damage of its own (all of ours) their reason never went in, and the wrap's removal let the hit through.
5. **V's hurts:** a hit the game won't let kill V could kill Minecraft's player, and so V; and V's overshield never took
   its part. (Two armours, V's then Minecraft's, are kept: SkyCraft does the same.)
6. **Step 4's checks:** the punch check compared the game's damage number, after every modifier, with the inventory's,
   before any; and the health read after `DealDamages` assumed the change applies at once.

### What the user decided

- **The game's pipeline:** our hit starts at Minecraft's damage × V's punch, and the pipeline does the rest, as to V's
  own punch.
- **The game's unarmed slot:** V's arm cyberware weapon if it has a melee attack, otherwise the fists or Gorilla Arms.
- **Drawn automatically:** V's unarmed weapon is drawn while combat and build mode are on, and V's own weapon put back
  after; and, in the user's words, "look into if it's possible to hide the player model completely when in build mode,
  so the HUD doesn't draw V's hands". It is (below), so V's meshes are hidden in build mode.
- **Both crits:** Minecraft's crit is in Minecraft's damage, and V's crit chance rolls on top.

### What changed

- **The hit** starts at the pipeline's entry (`ProcessPipeline`'s wrap), split as the attack splits its own damage; the
  `Process` wrap is gone; `DealDamages`' wrap reports what the pipeline made of it and did to it; a hit it dealt
  nothing reports why. The status says where a hit started, what was dealt, and what the pipeline did.
- **What V punches with** is what V's unarmed slot draws; `punch` and the status say whether V has it drawn.
- **V's weapon and look in build mode,** a new section: `Arm` and `HideV`, `set hidev`.
- **V's hurts:** the overshield's part stays in the game; non-lethal hits stay V's, counted apart.
- **Three natives change** (`BlockRenderVHurt`, `BlockRenderHitLanded`) **or are new** (`BlockRenderHitEnded`): seven in
  all. The module is version 4, with 10 functions and 14 fields.
- **Risks** 3 and 14 are restated; 15 to 17 are new. Step 4's opening check follows.

### What was checked

- **The damage pipeline** (`damageSystem.swift`, read whole): `ProcessPipeline`, `PreProcess`, `ModifyHitData` and each
  step it calls, `Process` and each modifier it calls (`CalculateSourceModifiers`, `CalculateTargetModifiers`,
  `CalculateSourceVsTargetModifiers`, `CalculateGlobalModifiers`, `ProcessArmor`, `ProcessCriticalHit`,
  `ProcessDamageMultipliers`, `ProcessCyberwareModifiers`, `ProcessStealthAttack`, `ProcessBlockAndDeflect`,
  `ScalePlayerDamage`, `ProcessOneShotProtection`, `ProcessPlayerFixedPercentageOverride`), `DealDamages` and
  `PostProcess`; `damageManager.swift` (whole); `statPoolsManager.swift`'s `ApplyDamage`, `ApplyDamageSingle`, the
  overshield and armour pools, `IsFinisherGrace` and `DrainStatPool`; `attackData.swift`'s `AddFlag`, `RemoveFlag`,
  `IsMelee`, `GetMinimumHealthPercent`; `modifyDamageEffector.swift` and `genericHitPrereq.swift`, with the hit prereq
  conditions that read the hit's weapon; `RPGManager.AwardExperienceFromDamage`; and a search showing
  `ProcessLevelDifference` is never called.
- **The unarmed slot** (`equipmentSystem.swift`): `GetWeaponSlotItem`, `GetMeleewareOrFistsItemID`,
  `GetActiveMeleeWare`, `CycleWeapon`, `GetItemsInArea`, `OnEquipmentSystemWeaponManipulationRequest` and
  `GetItemIDfromEquipmentManipulationAction`; `upperBodyTransitions.swift`'s draw on an attack while holstered; and
  `StatusEffectHelper`'s requests, the pattern `Arm` follows.
- **Hiding V:** `IComponent.Toggle` and `IsEnabled`, `IVisualComponent.TemporaryHide` (no getter, so not used), the
  SDK's mesh classes, all `IVisualComponent`s; `AnimationSystemForcedVisibilityManager`, which forces V to keep
  animating off screen and hides nothing; Codeware 1.19's `Entity.GetComponents` in its installed scripts; and the
  installed redscript's `@if(ModuleExists("Codeware"))`, both ways, by compiling a probe with a broken line in its
  Codeware branch: compiled without Codeware's scripts, failed with them.
- **SkyCraft's `ApplyHit`** (`skse/src/Combat.cpp`): `Populate`, then the damage overwritten.
- **The code:** Tasks 23 to 30 written and run in a scratch clone: zero warnings, 439 tests, the module compiling with
  and without Codeware, two slips failing it (one only with Codeware), the overlay under Lua with CET stubbed, and 22
  mutations of the new code each failing its test (one by a crash: the inbox without its lock); then replayed from the
  plan's text in a fresh clone.

### Not settled by reading

Left to step 4's opening check: whether the start set at `ProcessPipeline`'s entry holds (the hit's damage could be
filled in later by the game); whether the equipment system draws V's unarmed weapon while build mode places V; whether
hiding V's meshes leaves anything of V on screen, a shadow included; whether the arm cyberware is what V's unarmed slot
draws (it depends on how the game files it); and whether an NPC's health shows a hit's change at once.

## Changes from step 4

Step 4 ran on 2026-10-05 and 06 (the results' "Step 4"). Every check passed, and the game settled what the deep review
of Part 2 had left open: the start set at `ProcessPipeline`'s entry holds; V's unarmed slot draws the Mantis Blades;
the game draws them while build mode places V; hiding V leaves none of V's meshes on screen; and an NPC's health shows a
hit's change only after `DealDamages`. Three things didn't work as this spec meant, and the user chose how to fix them.
The sections above include the fixes.

### What step 4 found

1. **The damage was overkill.** Each point of Minecraft's damage was a whole blade swing: a diamond sword (7) started at
   734 and the axe (9) at 944, and each one-shot a level-47 ganger of 594 to 891 health. Fists, a point each, took four
   or five hits, which the user found right. One multiplier can't fix both: any factor that tames the sword weakens
   the fists as much.
2. **Our Immortal never took effect.** V carries `Mortal` from three sources, and it outranks `Immortal`: with ours
   added, `HasGodMode(Immortal)` stayed false, and the damage pipeline's `GetImmortality` asks exactly that. In build
   mode the damage wrap was V's only guard; it held for all 42 hits. And `RemoveGodMode` returns false even when it
   removes, so the log's "the game says it already was" was wrong each time.
3. **Hiding V left two things on screen:** the game's crosshair for the drawn blades, which is the HUD's, and the
   Thermal blades' heat effect, which an effect spawner started: switching the spawner off doesn't stop it.

### What the user decided

- **The damage's curve:** the square root. A hit starts at V's punch × Minecraft's damage to the power 0.5, so a fist
  stays one punch, a diamond sword is 2.65 and an axe 3; `set hitpower` changes the power while the game runs.
- **The game's HUD in build mode:** only the crosshair is hidden; the minimap, the quest tracker and the rest stay.
- **Fix all three before Part 2 closes,** then fight again in the game.

### What changed

- **Where a hit starts:** V's punch × Minecraft's damage to the power `set hitpower` (0.5), in `link/Combat`, with the
  power in the hit's log line and in the status.
- **V's Immortal:** the game's override (`EnableOverride` and `DisableOverride`), as Second Heart's; the log says
  whether the game then says V is Immortal.
- **Hiding V:** effects stopped by name before their spawners go off, and those that start by themselves started again
  when V is shown; the HUD's crosshair container hidden with V. The status's v line counts the effects and says whether
  the crosshair is hidden.
- **BlockRender 0.18.1, the module's version 5,** and a step for the fixes in the game, after step 4: its opening check
  reads the game's word on V's Immortal, looks for the crosshair and the heat, and checks where hits start; then the
  fights again.
- **Risks** 17 is restated; 18 to 20 are new.

### What was checked

- **In the game, through CET's console** (step 4): `AddGodMode` returns true and counts the source, while
  `HasGodMode(Immortal)` stays false and `HasGodMode(Mortal)` is true, from three sources; `RemoveGodMode` returns false
  as it removes.
- **The game's scripts:** `GodModeSystem`'s natives (`orphans.swift`: `AddGodMode`, `RemoveGodMode`, `HasGodMode`,
  `GetGodModeCount`, `GetGodModeSources`, `ClearGodMode`, `EnableOverride`, `DisableOverride`); `GetImmortality`
  (`godModeSystem.swift`); `SecondHeartStatListener` (`playerListeners.swift`), the only script that uses an override;
  `gameuiCrosshairContainerController` (`crosshairContainerController.swift`) and the crosshair controllers it holds;
  `entKillEffectEvent`, `entSpawnEffectEvent` and `entBreakEffectLoopEvent` (`orphans.swift`), and how the game stops
  and starts effects with them (`GameObject.StopReplicatedEffectEvent` and `StartReplicatedEffectEvent`,
  `AIActionHelper.KillWeaponEffect`); and again `ConvertDPSToHitDamage` and `CalculateDamageVariants`, which don't
  change our hit's start beyond the spread.
- **Codeware 1.19's installed scripts:** `inkSystem.GetLayer` and `inkLayerWrapper.GetGameControllers`;
  `entEffectSpawnerComponent.effectDescs` and `entEffectDesc`'s `effectName` and `isAutoSpawn`.
- **The RED4ext SDK:** `ent::EffectSpawnerComponent` and `ent::ParticlesComponent` are `IVisualComponent`s, so step 4's
  `HideV` already switched them off.

### Not settled by reading

Left to the fixes' opening check in the game: whether the override makes V Immortal at once, and how it sits with
Second Heart's own, if V has the implant; whether the heat is one of the blades' spawners' effects, and whether a kill
reaches a spawner that is then switched off; and the name of the layer that holds the crosshair container, taken to be
the HUD's (`inkHUDLayer`).
