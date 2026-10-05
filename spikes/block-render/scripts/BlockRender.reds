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
// Combat (spike 6's Part 2): Hit makes one of Minecraft's hits into a hit of V's on an NPC, marked as ours with its
// amount on the hit itself, and the wraps of the damage pipeline's Process and DealDamages, at the end of this file, deal
// it exactly; the same DealDamages wrap offers the game's hits on V to BlockRender, which takes them for Minecraft in
// build mode. The pipeline may run on any thread, so those wraps too touch nothing of the module's: they read the hit
// itself, and call BlockRender's natives.

// BlockRender's natives, added before the game loads its scripts; any thread may call them.
public native func BlockRenderNpcAttached(id: EntityID)
public native func BlockRenderNpcDetached(id: EntityID)
// For List: copies BlockRender's list and gives how many it holds; then the copy's IDs, by index.
public native func BlockRenderNpcCount() -> Int32
public native func BlockRenderNpcAt(i: Int32) -> EntityID
// For the DealDamages wrap: a hit on V, which BlockRender takes (true) for Minecraft while combat and build mode are on;
// and one of our hits, dealt, with the NPC's health before and after.
public native func BlockRenderVHurt(amount: Float, attackType: Int32, instigator: EntityID) -> Bool
public native func BlockRenderHitLanded(target: EntityID, amount: Float, before: Float, after: Float)

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
  // V's punch, the damage per hit of what V punches with (the item in the BaseFists area), and that item; and V's max
  // health. Read at most four times a second.
  public let punch: Float;
  public let punchItem: String;
  public let vHealthMax: Float;

  // The module's version, which BlockRender checks: 2 since the list moved into BlockRender, 3 with combat.
  public final static func Version() -> Int32 {
    return 3;
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
