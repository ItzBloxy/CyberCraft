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
