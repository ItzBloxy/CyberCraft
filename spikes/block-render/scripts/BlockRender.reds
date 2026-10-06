// BlockRender's redscript module (spike 6, docs/superpowers/specs/2026-10-05-npcs-and-combat-design.md): the game's side
// of Night City's NPCs in Minecraft. BlockRender, a RED4ext plugin, adds this file to redscript's compile at the game's
// start; each frame, on the main thread, it finds the game session's BlockRenderNpcs through Current(), calls List, and
// reads the fields List fills by name, each field's type checked once. tools/check_reds.py compiles it offline.
//
// The game calls NPCPuppet's OnGameAttached and OnDetach on several threads at once, so the wraps at the end of this file
// touch nothing of the module's: they hand each NPC's EntityID to BlockRender, which keeps the list behind a lock (step 2
// crashed while the wraps kept it in the module's own arrays). Everything else here runs on the main thread, called by
// BlockRender, and keeps no reference to an NPC from one call to the next: each is found again by its EntityID.
//
// Combat (spike 6's Part 2): Hit makes one of Minecraft's hits into a hit of V's on an NPC, made with what V punches
// with, and marked as ours, with where it starts, on the hit itself. The wrap of the damage pipeline's ProcessPipeline, at
// the end of this file, starts it there, and the pipeline does to it all it does to V's own punches; the DealDamages wrap
// reports what it took off, and the ProcessPipeline wrap why it took nothing, if it didn't. The same DealDamages wrap
// offers the game's hits on V to BlockRender, which takes them for Minecraft in build mode. The pipeline may run on any
// thread, so those wraps too touch nothing of the module's: they read the hit itself, and call BlockRender's natives.
// For build mode, Arm draws V's unarmed weapon, so that hits carry it, and HideV hides V's meshes, stops V's effects and
// hides the game's crosshair, through Codeware where it's installed.
//
// Minecraft's blasts (spike 6's Part 3): Explode runs the game's knockdown grenade's attack over what a blast reaches,
// with the explosion's sound, knocks each living NPC within it down with the game's forced reaction, and tells the crowd
// with the game's explosion stimulus; BlastHit makes one of Minecraft's hits on an NPC within the reach into the blast's
// own, of type Explosion and from its centre; and Push, a quarter second later, throws the bodies and loose things.

// BlockRender's natives, added before the game loads its scripts; any thread may call them.
public native func BlockRenderNpcAttached(id: EntityID)
public native func BlockRenderNpcDetached(id: EntityID)
// For List: copies BlockRender's list and gives how many it holds; then the copy's IDs, by index.
public native func BlockRenderNpcCount() -> Int32
public native func BlockRenderNpcAt(i: Int32) -> EntityID
// For the damage pipeline's wraps: a hit on V, which BlockRender takes (true) for Minecraft while combat and build mode
// are on, unless the game won't let it kill V; one of our hits, dealt, with what the pipeline made of it, the NPC's
// health before and after, and what the pipeline did to it; and one of our hits the pipeline dealt nothing, with why.
public native func BlockRenderVHurt(amount: Float, attackType: Int32, instigator: EntityID, nonLethal: Bool) -> Bool
public native func BlockRenderHitLanded(target: EntityID, amount: Float, before: Float, after: Float, flags: Int32)
public native func BlockRenderHitEnded(target: EntityID, why: Int32)

public class BlockRenderNpcs extends ScriptableSystem {
  // What was last read of each NPC on BlockRender's list, in the list's order: parallel arrays, one entry an NPC, kept in
  // step with the list by List.
  private let knownIds: array<EntityID>;
  private let knownHealth: array<Float>;
  private let knownHealthMax: array<Float>;
  private let knownLevels: array<Float>;
  private let knownNames: array<String>;
  private let knownFlags: array<Int32>;
  private let knownReadAt: array<Float>;
  private let knownRead: array<Bool>;
  private let vReadAt: Float;
  private let vRead: Bool;
  // Arm's: whether it drew V's unarmed weapon, and what V held before, to put back. HideV's: whether V is hidden, and the
  // meshes it switched off.
  private let armed: Bool;
  private let armedFrom: ItemID;
  private let hiding: Bool;
  private let hidden: array<wref<IComponent>>;
  // HideV's too: the effects it stopped that start by themselves, and what each is on, to start again; and whether it
  // hid the game's crosshair.
  private let restartOn: array<wref<Entity>>;
  private let restartNames: array<CName>;
  private let crosshairHidden: Bool;

  // What List fills, one entry for each NPC listed, for BlockRender to read by name.
  public let npcIds: array<EntityID>;
  public let npcPositions: array<Vector4>;
  public let npcForwards: array<Vector4>;
  public let npcHealth: array<Float>;
  public let npcHealthMax: array<Float>;
  public let npcLevels: array<Float>;
  public let npcNames: array<String>;
  public let npcFlags: array<Int32>;
  // NPCs on BlockRender's list: attached, and not detached; and how many of them this call found by their EntityIDs.
  public let attached: Int32;
  public let found: Int32;
  // V's punch, the damage per hit of what V punches with (what V's unarmed slot draws: UnarmedItem), that item, and
  // whether V has it drawn; V's max health, and V's health as a share of it. Read at most four times a second.
  public let punch: Float;
  public let punchItem: String;
  public let punchDrawn: Bool;
  public let vHealthMax: Float;
  public let vHealth: Float;
  // What HideV last did, besides the meshes it gives, for BlockRender to read after it. Hiding: the effects it has
  // stopped since V was hidden, and the crosshair (1 hidden, -1 not found among the HUD layer's game controllers).
  // Showing: the effects it started again, and the crosshair (1 shown again, 0 it hadn't hidden it).
  public let vEffects: Int32;
  public let vCrosshair: Int32;
  // What Push last found within the blast's reach, besides the NPCs it gives: how many of them the game has knocked down
  // (a Knockdown status effect on them), and how many are dead.
  public let pushDown: Int32;
  public let pushDead: Int32;
  // What Explode last did besides its attack and sound: how many living NPCs within the blast's reach it knocked down with
  // the game's forced reaction, how many it left standing as the game keeps them from being knocked down, and whether it
  // told the crowd.
  public let blastKnocked: Int32;
  public let blastImmune: Int32;
  public let blastPanic: Bool;

  // The module's version, which BlockRender checks: 2 since the list moved into BlockRender, 3 with combat, 4 since hits
  // go through the whole damage pipeline, with what V's unarmed slot draws, 5 since V's Immortal is the game's override,
  // and HideV stops V's effects and hides the game's crosshair, 6 with Minecraft's blasts and V's health share, 7 since a
  // blast knocks NPCs down with the game's forced reaction and tells the crowd.
  public final static func Version() -> Int32 {
    return 7;
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
    let count: Int32 = BlockRenderNpcCount();
    this.found = 0;
    let k: Int32 = 0;
    let i: Int32 = 0;
    while i < count {
      let id: EntityID = BlockRenderNpcAt(i);
      // BlockRender's list only loses IDs, and gains them at its end, so what's known of NPCs gone since comes before
      // the next one still there. (No index past the end, even if && doesn't stop at its left side.)
      while k < ArraySize(this.knownIds) {
        if this.knownIds[k] == id {
          break;
        };
        this.Forget(k);
      };
      if k == ArraySize(this.knownIds) {
        this.Know(id);
      };
      let npc: ref<NPCPuppet> = GameInstance.FindEntityByID(game, id) as NPCPuppet;
      if IsDefined(npc) {
        this.found += 1;
        let position: Vector4 = npc.GetWorldPosition();
        if Vector4.Distance(position, v) <= range && !npc.IsDead() {
          if !this.knownRead[k] || now - this.knownReadAt[k] >= 0.25 {
            this.ReadNpc(k, npc, player, game);
            // The first read spreads the later ones over the quarter second, so a crowd isn't read all in one frame.
            this.knownReadAt[k] = this.knownRead[k] ? now : now - RandRangeF(0.00, 0.25);
            this.knownRead[k] = true;
          };
          ArrayPush(this.npcIds, id);
          ArrayPush(this.npcPositions, position);
          ArrayPush(this.npcForwards, npc.GetWorldForward());
          ArrayPush(this.npcHealth, this.knownHealth[k]);
          ArrayPush(this.npcHealthMax, this.knownHealthMax[k]);
          ArrayPush(this.npcLevels, this.knownLevels[k]);
          ArrayPush(this.npcNames, this.knownNames[k]);
          ArrayPush(this.npcFlags, this.knownFlags[k]);
        };
      };
      k += 1;
      i += 1;
    };
    while k < ArraySize(this.knownIds) {
      this.Forget(k);
    };
    this.attached = count;
    return ArraySize(this.npcIds);
  }

  // For the npcs request: the NPCs the game's targeting finds within range metres of V, searched as the game's own
  // effectors search for NPCs, against BlockRender's list as List last copied it.
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
        if !ArrayContains(this.knownIds, npc.GetEntityID()) {
          lacks += 1;
          lacking += (lacks > 1 ? "; " : "") + npc.GetDisplayName() + " at " + FloatToStringPrec(Vector4.Distance(npc.GetWorldPosition(), player.GetWorldPosition()), 1) + " m";
        };
      };
      i += 1;
    };
    return "GetTargetParts finds " + ToString(ArraySize(found)) + " NPCs within " + FloatToStringPrec(range, 0) + " m; the list lacks " + ToString(lacks) + (lacks > 0 ? ": " + lacking : "");
  }

  // One of Minecraft's hits on an NPC, as V's: amount is where the damage pipeline starts it, V's punch x Minecraft's
  // damage to the hit power, and strong picks the strong attack. Built as the game's SendHitEvent builds one, from V, with
  // the attack record of what V punches with, and the weapon object of it if V has it drawn (Strike). 1: queued; 0: V isn't
  // found; -1: the NPC is gone; -2: it's dead; -3: there's no attack record to make it with.
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
    let item: ItemID = this.UnarmedItem(player);
    let record: ref<Attack_Record> = this.AttackOf(item, strong);
    if !IsDefined(record) {
      return -3;
    };
    this.Strike(game, player, npc, record, this.PunchWeapon(player, game, item), amount, player.GetWorldPosition(), false);
    return 1;
  }

  // One of Minecraft's hits on an NPC within a blast's reach, as the blast's own: amount is where the damage pipeline
  // starts it, V's punch x Minecraft's damage to the blast power, and centre is the blast's, in world axes. Made as Hit
  // makes hits (Strike), but with the game's knockdown grenade's attack record, Attacks.FragGrenadeKnockdown, of type
  // Explosion: after the damage the pipeline knocks the NPC down with the record's status effect (PostProcess), so that one
  // the hit kills ragdolls at once and the push throws it. With no weapon, from the blast's centre, and flagged a weak
  // explosion: the game measures a strong one from the hit's source, V. 1: queued; 0: V isn't found; -1: the NPC is gone;
  // -2: it's dead; -3: there's no attack record to make it with.
  public final func BlastHit(id: EntityID, amount: Float, centre: Vector4) -> Int32 {
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
    let record: ref<Attack_Record> = TweakDBInterface.GetAttackRecord(t"Attacks.FragGrenadeKnockdown");
    if !IsDefined(record) || !IsDefined(record.AttackType()) {
      return -3;
    };
    this.Strike(game, player, npc, record, null, amount, centre, true);
    return 1;
  }

  // A blast of Minecraft's at centre (world axes), over what is within reach metres of it. The game's knockdown grenade's
  // attack, Attacks.FragGrenadeKnockdown, runs as a game effect at the centre with the reach as its radius, as
  // CombatGadgetHelper.SpawnAttack runs one, from V and dealing no damage: the pipeline still gives whatever it hits the
  // record's status effect (PostProcess), unless immune or friendly to V, so that one the blast then kills ragdolls at
  // once, and its hit on V stops at CheckForQuickExit. Its hits are flagged weak explosions, as BlastHit's are. With the
  // explosion's sound at the centre, as a claymore plays it. That status effect alone doesn't make an NPC fall (step 5b):
  // with knock, each living NPC on BlockRender's list within reach is knocked down by the game's forced reaction, as perks
  // force theirs, away from the centre, unless the game keeps it from being knocked down (KnockdownImmune); blastKnocked
  // and blastImmune count them. The crowd is told as the game's explosions tell it: the Explosion stimulus, 30 m around
  // the listed NPC nearest the centre within 30 m, with V as its instigator, as the game broadcasts a thrown body's
  // explosion from the body; blastPanic says whether. 1: started; 0: V isn't found; -1: there's no attack record or effect.
  public final func Explode(centre: Vector4, reach: Float, knock: Bool) -> Int32 {
    let game: GameInstance = this.GetGameInstance();
    let player: ref<GameObject> = GameInstance.GetPlayerSystem(game).GetLocalPlayerMainGameObject();
    if !IsDefined(player) {
      return 0;
    };
    let record: ref<Attack_Record> = TweakDBInterface.GetAttackRecord(t"Attacks.FragGrenadeKnockdown");
    if !IsDefined(record) {
      return -1;
    };
    let context: AttackInitContext;
    context.record = record;
    context.instigator = player;
    context.source = player;
    let attack: ref<Attack_GameEffect> = IAttack.Create(context) as Attack_GameEffect;
    if !IsDefined(attack) {
      return -1;
    };
    let effect: ref<EffectInstance> = attack.PrepareAttack(player);
    if !IsDefined(effect) {
      return -1;
    };
    let mods: array<ref<gameStatModifierData>>;
    attack.GetStatModList(mods);
    let flags: array<SHitFlag>;
    let flag: SHitFlag;
    flag.source = n"BlockRender blast";
    flag.flag = hitFlag.DealNoDamage;
    ArrayPush(flags, flag);
    flag.flag = hitFlag.WeakExplosion;
    ArrayPush(flags, flag);
    let shared: ref<EffectSharedDataDef> = GetAllBlackboardDefs().EffectSharedData;
    EffectData.SetFloat(effect.GetSharedData(), shared.radius, reach);
    EffectData.SetFloat(effect.GetSharedData(), shared.range, reach);
    EffectData.SetVector(effect.GetSharedData(), shared.position, centre);
    EffectData.SetVariant(effect.GetSharedData(), shared.attack, ToVariant(attack));
    EffectData.SetVariant(effect.GetSharedData(), shared.attackStatModList, ToVariant(mods));
    EffectData.SetVariant(effect.GetSharedData(), shared.flags, ToVariant(flags));
    attack.StartAttack();
    GameInstance.GetAudioSystem(game).PlayShockwave(n"explosion", centre);
    this.blastKnocked = 0;
    this.blastImmune = 0;
    this.blastPanic = false;
    let nearest: ref<NPCPuppet>;
    let nearestDistance: Float = 30.00;
    let count: Int32 = BlockRenderNpcCount();
    let i: Int32 = 0;
    while i < count {
      let npc: ref<NPCPuppet> = GameInstance.FindEntityByID(game, BlockRenderNpcAt(i)) as NPCPuppet;
      if IsDefined(npc) {
        let distance: Float = Vector4.Distance(npc.GetWorldPosition(), centre);
        if distance <= nearestDistance {
          nearest = npc;
          nearestDistance = distance;
        };
        if knock && !npc.IsDead() && distance <= reach {
          if this.KnockdownImmune(npc, player, game) {
            this.blastImmune += 1;
          } else {
            // A knockdown (the sub-action's type 4) at an explosion's intensity (3), hit from the centre's side, in one of
            // an explosion's variations, as the hit reaction component picks them for a strong explosion.
            let direction: Int32 = GameObject.GetLocalAngleForDirectionInInt(Vector4.Normalize(npc.GetWorldPosition() - centre), npc);
            AISubActionForceHitReaction_Record_Implementation.SendForcedHitDataToAIBehavior(npc, direction, 3, 4, 2, 0, RandRange(0, 3), 0);
            this.blastKnocked += 1;
          };
        };
      };
      i += 1;
    };
    if IsDefined(nearest) {
      let investigate: stimInvestigateData;
      investigate.attackInstigator = player;
      investigate.attackInstigatorPosition = player.GetWorldPosition();
      investigate.revealsInstigatorPosition = true;
      StimBroadcasterComponent.BroadcastStim(nearest, gamedataStimType.Explosion, 30.00, investigate);
      this.blastPanic = true;
    };
    return 1;
  }

  // A blast's push, a quarter second after it: the game effect physicalImpulseSphere at centre (world axes) with reach as
  // its radius, as CombatGadgetHelper.SpawnPhysicalImpulse runs it, which throws the bodies of the NPCs the blast killed
  // (knocked down as they died, they ragdolled at once) and pushes cars and loose things. Then the NPCs on BlockRender's
  // list within reach: how many, and in pushDown and pushDead how many of them the game has knocked down (a Knockdown
  // status effect) and how many are dead. -1 if V isn't found.
  public final func Push(centre: Vector4, reach: Float) -> Int32 {
    let game: GameInstance = this.GetGameInstance();
    let player: ref<GameObject> = GameInstance.GetPlayerSystem(game).GetLocalPlayerMainGameObject();
    this.pushDown = 0;
    this.pushDead = 0;
    if !IsDefined(player) {
      return -1;
    };
    let effect: ref<EffectInstance> = GameInstance.GetGameEffectSystem(game).CreateEffectStatic(n"physicalImpulseSphere", n"None", player);
    if IsDefined(effect) {
      let shared: ref<EffectSharedDataDef> = GetAllBlackboardDefs().EffectSharedData;
      EffectData.SetFloat(effect.GetSharedData(), shared.radius, reach);
      EffectData.SetVector(effect.GetSharedData(), shared.position, centre);
      effect.Run();
    };
    let near: Int32 = 0;
    let count: Int32 = BlockRenderNpcCount();
    let i: Int32 = 0;
    while i < count {
      let npc: ref<NPCPuppet> = GameInstance.FindEntityByID(game, BlockRenderNpcAt(i)) as NPCPuppet;
      if IsDefined(npc) && Vector4.Distance(npc.GetWorldPosition(), centre) <= reach {
        near += 1;
        if npc.IsDead() {
          this.pushDead += 1;
        } else {
          if StatusEffectSystem.ObjectHasStatusEffectOfType(npc, gamedataStatusEffectType.Knockdown) {
            this.pushDown += 1;
          };
        };
      };
      i += 1;
    };
    return near;
  }

  // V's god mode for build mode: Immortal forced on V by the game's override, enabled or disabled under BlockRender's own
  // source name only, as Second Heart forces it. An Immortal added as a god mode does nothing on V, whose Mortal god
  // modes outrank it (step 4). Off also removes an Immortal that module version 4 added. Whether the game then says V is
  // Immortal.
  public final func SetImmortal(on: Bool) -> Bool {
    let game: GameInstance = this.GetGameInstance();
    let player: ref<GameObject> = GameInstance.GetPlayerSystem(game).GetLocalPlayerMainGameObject();
    if !IsDefined(player) {
      return false;
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
    gods.DisableOverride(id, n"BlockRender");
    gods.RemoveGodMode(id, gameGodModeType.Immortal, n"BlockRender");
    let instigator: ref<GameObject>;
    if EntityID.IsDefined(killer) {
      instigator = GameInstance.FindEntityByID(game, killer) as GameObject;
    };
    GameInstance.GetStatPoolsSystem(game).RequestSettingStatPoolMinValue(Cast<StatsObjectID>(id), gamedataStatPoolType.Health, instigator);
    return !gods.HasGodMode(id, gameGodModeType.Immortal) && !gods.HasGodMode(id, gameGodModeType.Invulnerable);
  }

  // V's unarmed weapon, drawn for build mode's fights so that Minecraft's hits carry it, as V's own punches do (on); or
  // what V held before, put back (off). Drawing asks the equipment system for weapon-wheel slot 4 at once, as the slot's
  // key does; putting back asks for the wheel's slot V's weapon is in, or holsters if V held nothing, or something not on
  // the wheel. Called again while on, it asks again if the weapon isn't drawn yet. If V held its unarmed weapon already,
  // and HideV has stopped its effects, putting back puts it away and draws it again a second later, since the game starts
  // some of a weapon's effects only as it's drawn (the Thermal blades' heat). 1: asked; 2: nothing to ask; 3: put away,
  // to be drawn again; 0: V isn't found; -1: V has no unarmed weapon; -2: there's no equipment system.
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
      if this.hiding && ItemID.IsValid(held) && held == this.UnarmedItem(player) {
        request.requestType = EquipmentManipulationAction.UnequipWeapon;
        equipment.QueueRequest(request);
        let again: ref<BlockRenderDrawAgain> = new BlockRenderDrawAgain();
        again.player = player;
        GameInstance.GetDelaySystem(game).DelayCallback(again, 1.00, false);
        return 3;
      };
      return 2;
    };
    request.requestType = this.WheelRequest(player, this.armedFrom);
    equipment.QueueRequest(request);
    return 1;
  }

  // V for build mode: V's meshes, and those of everything V wears and holds, hidden (on), each switched off if it was on,
  // and remembered, so that showing V again (off) switches on exactly those and leaves off what the game had off. An
  // effect spawner's effects are stopped by name before it goes off, since switching it off leaves them running (step 4:
  // the Thermal blades' heat), and those that start by themselves are started again when V is shown. And the game's
  // crosshair, hidden with V and shown again if HideV hid it. Called again while on, it hides what has appeared since on
  // V and in V's hands, such as the weapon Arm drew, without looking through V's whole inventory again, and the crosshair
  // again if the game showed it. The meshes hidden, or shown again; -1 without Codeware, which lists an entity's
  // components; -2 if V isn't found. vEffects and vCrosshair say the rest.
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
      return shown;
    };
    if !BlockRenderCanHideV() {
      return -1;
    };
    let player: ref<GameObject> = GameInstance.GetPlayerSystem(this.GetGameInstance()).GetLocalPlayerMainGameObject();
    if !IsDefined(player) {
      return -2;
    };
    if !this.hiding {
      this.vEffects = 0;
    };
    let off: array<wref<IComponent>>;
    let startOn: array<wref<Entity>>;
    let startNames: array<CName>;
    this.vEffects += BlockRenderSwitchOffMeshes(player, !this.hiding, off, startOn, startNames);
    this.hiding = true;
    while i < ArraySize(off) {
      ArrayPush(this.hidden, off[i]);
      i += 1;
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
  }

  // For the punch request: V's punch, what it's read from, what Hit makes hits with, and whether V has it drawn.
  public final func Punch() -> String {
    let game: GameInstance = this.GetGameInstance();
    let player: ref<GameObject> = GameInstance.GetPlayerSystem(game).GetLocalPlayerMainGameObject();
    if !IsDefined(player) {
      return "V isn't found";
    };
    this.ReadV(player, game);
    let item: ItemID = this.UnarmedItem(player);
    return "V's punch " + FloatToStringPrec(this.punch, 2) + ", read from " + this.punchItem + "; quick hits with "
      + this.AttackName(this.AttackOf(item, false)) + ", strong ones with " + this.AttackName(this.AttackOf(item, true)) + "; "
      + (this.punchDrawn ? "drawn, so hits carry it as their weapon"
                         : "not drawn, so hits carry no weapon, and skip the NPC's armour, the weapon's crits, stealth, blocks and the skill XP");
  }

  // Where one of our hits starts, if it's ours: from its hitFlag.None marked "BlockRender <amount>". -1 if it isn't. Any
  // thread: it reads only the hit.
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

  // Whether the game keeps npc from being knocked down, as its hit reaction component decides for a hit: knockdown
  // immunity, an off-mesh link, friendly to V or a Cerberus (SetHitReactionImmunities); massive, or a leg dismembered
  // (GetReactionType).
  private final func KnockdownImmune(npc: ref<NPCPuppet>, player: ref<GameObject>, game: GameInstance) -> Bool {
    if GameInstance.GetStatsSystem(game).GetStatValue(Cast<StatsObjectID>(npc.GetEntityID()), gamedataStatType.KnockdownImmunity) > 0.00 || ScriptedPuppet.IsOnOffMeshLink(npc) {
      return true;
    };
    if Equals(GameObject.GetAttitudeTowards(npc, player), EAIAttitude.AIA_Friendly) || Equals(npc.GetNPCType(), gamedataNPCType.Cerberus) {
      return true;
    };
    return npc.IsMassive() || StatusEffectSystem.ObjectHasStatusEffectWithTag(npc, n"DismemberedLeg");
  }

  // A hit on npc's middle, built as the game's SendHitEvent builds one: from V, as its instigator and source, with record
  // and weapon (or none), its direction from the point from (world axes); marked as ours, with where it starts, amount, on
  // the hit itself, as the game marks its environment's attacks with hitFlag.None and a source name, so that the
  // pipeline's wraps read it back on whichever thread runs it; flagged a weak explosion if weak; and queued.
  private final func Strike(game: GameInstance, player: ref<GameObject>, npc: ref<NPCPuppet>, record: ref<Attack_Record>,
    weapon: ref<WeaponObject>, amount: Float, from: Vector4, weak: Bool) -> Void {
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
    let mark: String = "BlockRender " + FloatToStringPrec(amount, 2);
    hit.attackData.AddFlag(hitFlag.None, StringToName(mark));
    if weak {
      hit.attackData.AddFlag(hitFlag.WeakExplosion, n"BlockRender blast");
    };
    let middle: Vector4 = npc.GetWorldPosition() + new Vector4(0.00, 0.00, 1.00, 0.00);
    hit.hitPosition = middle;
    hit.hitDirection = Vector4.Normalize(middle - from);
    let shape: HitShapeData;
    shape.result.hitPositionEnter = middle;
    shape.result.hitPositionExit = middle;
    ArrayPush(hit.hitRepresentationResult.hitShapes, shape);
    GameInstance.GetDamageSystem(game).QueueHitEvent(hit, npc);
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
  }

  private final func AttackName(record: ref<Attack_Record>) -> String {
    if !IsDefined(record) {
      return "none";
    };
    let type: String = IsDefined(record.AttackType()) ? EnumValueToString("gamedataAttackType", Cast<Int64>(EnumInt(record.AttackType().Type()))) : "no type";
    return TDBID.ToStringDEBUG(record.GetID()) + " (" + type + ")";
  }

  private final func Know(id: EntityID) -> Void {
    ArrayPush(this.knownIds, id);
    ArrayPush(this.knownHealth, 0.00);
    ArrayPush(this.knownHealthMax, 0.00);
    ArrayPush(this.knownLevels, 0.00);
    ArrayPush(this.knownNames, "");
    ArrayPush(this.knownFlags, 0);
    ArrayPush(this.knownReadAt, 0.00);
    ArrayPush(this.knownRead, false);
  }

  private final func Forget(k: Int32) -> Void {
    ArrayErase(this.knownIds, k);
    ArrayErase(this.knownHealth, k);
    ArrayErase(this.knownHealthMax, k);
    ArrayErase(this.knownLevels, k);
    ArrayErase(this.knownNames, k);
    ArrayErase(this.knownFlags, k);
    ArrayErase(this.knownReadAt, k);
    ArrayErase(this.knownRead, k);
  }

  private final func ReadNpc(k: Int32, npc: ref<NPCPuppet>, player: ref<GameObject>, game: GameInstance) -> Void {
    let id: EntityID = this.knownIds[k];
    let stats: StatsObjectID = Cast<StatsObjectID>(id);
    let pools: ref<StatPoolsSystem> = GameInstance.GetStatPoolsSystem(game);
    let godModes: ref<GodModeSystem> = GameInstance.GetGodModeSystem(game);
    this.knownHealth[k] = pools.GetStatPoolValue(stats, gamedataStatPoolType.Health, false);
    this.knownHealthMax[k] = pools.GetStatPoolMaxPointValue(stats, gamedataStatPoolType.Health);
    this.knownLevels[k] = GameInstance.GetStatsSystem(game).GetStatValue(stats, gamedataStatType.PowerLevel);
    this.knownNames[k] = npc.GetDisplayName();
    // SkyCraft's flags: hostile 1, essential 4, in combat 8.
    let flags: Int32 = 0;
    if Equals(GameObject.GetAttitudeTowards(npc, player), EAIAttitude.AIA_Hostile) {
      flags += 1;
    };
    if godModes.HasGodMode(id, gameGodModeType.Immortal) || godModes.HasGodMode(id, gameGodModeType.Invulnerable) {
      flags += 4;
    };
    if NPCPuppet.IsInCombat(npc) {
      flags += 8;
    };
    this.knownFlags[k] = flags;
  }

  private final func ReadV(player: ref<GameObject>, game: GameInstance) -> Void {
    let stats: StatsObjectID = Cast<StatsObjectID>(player.GetEntityID());
    let pools: ref<StatPoolsSystem> = GameInstance.GetStatPoolsSystem(game);
    this.vHealthMax = pools.GetStatPoolMaxPointValue(stats, gamedataStatPoolType.Health);
    this.vHealth = pools.GetStatPoolValue(stats, gamedataStatPoolType.Health, true) / 100.00;
    this.punch = 0.00;
    this.punchDrawn = false;
    this.punchItem = "nothing in V's unarmed slot";
    let item: ItemID = this.UnarmedItem(player);
    if ItemID.IsValid(item) {
      this.punchItem = TDBID.ToStringDEBUG(ItemID.GetTDBID(item));
      let data: wref<gameItemData> = GameInstance.GetTransactionSystem(game).GetItemData(player, item);
      if IsDefined(data) {
        this.punch = data.GetStatValueByType(gamedataStatType.EffectiveDamagePerHit);
      } else {
        this.punchItem += ", without item data";
      };
      this.punchDrawn = IsDefined(this.PunchWeapon(player, game, item));
    };
  }
}

// V's unarmed weapon drawn again, a second after Arm put it away as build mode ended, so that the game starts the effects it
// starts as a weapon is drawn, which HideV stopped. The game's delay system calls it.
public class BlockRenderDrawAgain extends DelayCallback {
  public let player: wref<GameObject>;

  public func Call() -> Void {
    let player: ref<GameObject> = this.player;
    if !IsDefined(player) {
      return;
    };
    let equipment: ref<EquipmentSystem> = EquipmentSystem.GetInstance(player);
    if !IsDefined(equipment) {
      return;
    };
    let request: ref<EquipmentSystemWeaponManipulationRequest> = new EquipmentSystemWeaponManipulationRequest();
    request.owner = player;
    request.requestType = EquipmentManipulationAction.RequestWeaponSlot4;
    request.equipAnimType = gameEquipAnimationType.Instant;
    equipment.QueueRequest(request);
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
  };
  let items: array<wref<gameItemData>>;
  transactions.GetItemList(player, items);
  let i: Int32 = 0;
  while i < ArraySize(items) {
    if IsDefined(items[i]) {
      let item: ref<ItemObject> = transactions.GetItemInSlotByItemID(player, items[i].GetID());
      if IsDefined(item) {
        stopped += BlockRenderSwitchOffEntity(item, off, restartOn, restartNames);
      };
    };
    i += 1;
  };
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
  let components: array<ref<IComponent>> = entity.GetComponents();
  let i: Int32 = 0;
  while i < ArraySize(components) {
    let component: ref<IComponent> = components[i];
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
      component.Toggle(false);
      ArrayPush(off, component);
    };
    i += 1;
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
}

// Every NPC the game puts in the world, people, drones, mechs and animals alike, crowds included: NPCPuppet has no
// subclasses in the game's scripts. On any thread: only the NPC's EntityID goes to BlockRender.
@wrapMethod(NPCPuppet)
protected cb func OnGameAttached() -> Bool {
  let result: Bool = wrappedMethod();
  BlockRenderNpcAttached(this.GetEntityID());
  return result;
}

@wrapMethod(NPCPuppet)
protected cb func OnDetach() -> Bool {
  BlockRenderNpcDetached(this.GetEntityID());
  return wrappedMethod();
}

// Our hits start the whole pipeline where Hit or BlastHit marked them, V's punch x Minecraft's damage to the hit power or
// the blast power, split across damage types as the attack splits its own; the pipeline then does to them all it does to
// V's own punches, from the spread and V's crits, perks and cyberware to the NPC's armour, resistances, blocks and dodges,
// and to a blast's hits all it does to explosions, the NPC's explosion resistance included. One it dealt nothing says
// why. Any thread.
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
    return;
  };
  if IsDefined(target) && target.IsPlayer() && IsDefined(hitEvent.attackComputed) && IsDefined(hitEvent.attackData) {
    let total: Float = hitEvent.attackComputed.GetTotalAttackValue(gamedataStatPoolType.Health);
    let pools: ref<StatPoolsSystem> = GameInstance.GetStatPoolsSystem(target.GetGame());
    let shield: Float = MinF(total, MaxF(pools.GetStatPoolValue(Cast<StatsObjectID>(target.GetEntityID()), gamedataStatPoolType.Overshield, false), 0.00));
    if total > shield {
      let from: EntityID;
      let instigator: ref<GameObject> = hitEvent.attackData.GetInstigator();
      if IsDefined(instigator) {
        from = instigator.GetEntityID();
      };
      let nonLethal: Bool = hitEvent.attackData.HasFlag(hitFlag.CannotKillPlayer) || hitEvent.attackData.GetMinimumHealthPercent() > 0.00;
      if BlockRenderVHurt(total - shield, EnumInt(hitEvent.attackData.GetAttackType()), from, nonLethal) {
        BlockRenderNpcs.SetDamage(hitEvent.attackComputed, shield);
      };
    };
  };
  wrappedMethod(hitEvent);
}
