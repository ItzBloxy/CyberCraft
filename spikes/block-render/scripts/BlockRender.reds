// BlockRender's redscript module (spike 6, docs/superpowers/specs/2026-10-05-npcs-and-combat-design.md): the game's side
// of Night City's NPCs in Minecraft. BlockRender, a RED4ext plugin, adds this file to redscript's compile at the game's
// start; each frame, on the main thread, it finds the game session's BlockRenderNpcs through Current(), calls List, and
// reads the fields List fills by name, each field's type checked once. tools/check_reds.py compiles it offline.
//
// The game calls NPCPuppet's OnGameAttached and OnDetach on several threads at once, so the wraps at the end of this file
// touch nothing of the module's: they hand each NPC's EntityID to BlockRender, which keeps the list behind a lock (step 2
// crashed while the wraps kept it in the module's own arrays). Everything else here runs on the main thread, called by
// BlockRender, and keeps no reference to an NPC from one call to the next: each is found again by its EntityID.

// BlockRender's natives, added before the game loads its scripts; any thread may call them.
public native func BlockRenderNpcAttached(id: EntityID)
public native func BlockRenderNpcDetached(id: EntityID)
// For List: copies BlockRender's list and gives how many it holds; then the copy's IDs, by index.
public native func BlockRenderNpcCount() -> Int32
public native func BlockRenderNpcAt(i: Int32) -> EntityID

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

  // The module's version, which BlockRender checks: 2 since the list moved into BlockRender.
  public final static func Version() -> Int32 {
    return 2;
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
