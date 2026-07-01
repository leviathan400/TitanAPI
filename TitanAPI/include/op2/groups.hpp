#pragma once
// op2/groups.hpp - Module 5: AI ScGroups. A Group is a roster of units the engine commands as a unit - combat
// FightGroups, mining MiningGroups, base BuildingGroups. This is how you "script an AI": assign units to a
// group and give the GROUP strategic orders (attack the enemy, guard, patrol) instead of micro-managing each
// unit. Groups are ScStub handles (exactly like Trigger): a single {int id_}.
//
// ABI notes:
//   * Ops are __thiscall members on the {int id_} handle (same proven pattern as Trigger::enable/hasFired).
//   * Unit arguments are the TethysAPI `Unit` = {int id_} (the MapObject index), so we pass `unit.id()`.
//   * The create* factories return a group BY VALUE -> the x86 sret ABI (hidden result pointer in ECX), and
//     take the engine's `_Player` handle ({int id_; uint8 check_[8]}, default-zeroed) BY VALUE on the stack.

#include "op2/unit.hpp"
#include "op2/player.hpp"
#include "op2/abi/memory.hpp"
#include "op2/abi/enums.hpp"
#include <vector>
#include <deque>
#include <initializer_list>

namespace op2 {

using MapID = abi::MapID;

/// How the engine categorizes a unit in a UnitBlock (drives the block's class sorting + a group's per-class
/// tracking). Attack covers armed Lynx/Panther/Tiger - the common combat-reinforcement case.
enum class UnitClass : int {
  Attack = 0, ESG = 1, EMP = 2, Stickyfoam = 3, Spider = 4, ConVec = 5, RepairVehicle = 6, CargoTruck = 7,
  Earthworker = 8, Colony = 9, VehicleFactory = 10, ArachnidFactory = 11, StructureFactory = 12,
};

/// One unit to batch-create in a UnitBlock. `cls` drives how the block/group categorizes it (default Attack).
struct UnitRecord {
  MapID type; Location at; MapID weapon = MapID::None; int rotation = 0;
  int cargoType = 0; int cargoAmount = 0; UnitClass cls = UnitClass::Attack;
};

namespace group_detail {
/// The engine's UnitRecord (probe-verified: 32 bytes; cargoType/Amount are uint16 @28/@30).
struct EngineUnitRecord {
  int unitType, x, y, field_0C, rotation, weaponType, cls; abi::u16 cargoType, cargoAmount;
};
static_assert(sizeof(EngineUnitRecord) == 32, "UnitRecord must be 32 bytes");

// FightGroup patrol routes. SetPatrolMode takes a PatrolRoute {int unknown1; LOCATION* waypoints} (8 bytes) and
// the engine KEEPS the pointer, so both the route struct AND the waypoint array must outlive the call. We park
// each (waypoints + route) in a std::deque so element addresses stay stable as it grows.
struct EngineLocXY { int x, y; };
struct EnginePatrolRoute { int unknown1; EngineLocXY* waypoints; };
static_assert(sizeof(EnginePatrolRoute) == 8, "PatrolRoute must be 8 bytes");
struct PatrolHolder { std::vector<EngineLocXY> wps; EnginePatrolRoute route; };
inline std::deque<PatrolHolder>& patrolStore() { static std::deque<PatrolHolder> s; return s; }
} // namespace group_detail

/// A batch of units = the engine `UnitBlock`. Construct from a list, then `createUnits()` them for a player, or
/// hand it to a Group (`addUnits` / `setTargCount` / `recordUnitBlock`). The engine object sorts and stores a
/// pointer into our record table, so a UnitBlock is **non-copyable** - keep it alive while a group uses it.
class UnitBlock {
public:
  explicit UnitBlock(std::initializer_list<UnitRecord> records) {
    table_.reserve(records.size() + 1);
    for (const UnitRecord& r : records)
      table_.push_back({ int(r.type), r.at.engineX(), r.at.engineY(), 0, r.rotation, int(r.weapon), int(r.cls),
                         abi::u16(r.cargoType), abi::u16(r.cargoAmount) });
    table_.push_back({ int(MapID::None), 0, 0, 0, 0, int(MapID::None), 0, 0, 0 });   // mapNone TERMINATOR - the
    // engine ctor takes only a pointer, so SortAndInit scans the table until this unitType==None record. Without
    // it, SortAndInit/CreateUnits read past the end and fault in FindUnitPlacementLocation (the v0.5.40 crash).
    abi::member<0x49D4A0, void>(block_, table_.data());          // engine ctor: SortAndInit(table_), fills classRange
  }
  UnitBlock(const UnitBlock&) = delete;
  UnitBlock& operator=(const UnitBlock&) = delete;

  /// Create all the block's units for `owner` (returns the count created). UnitBlock::CreateUnits @0x49D5D0.
  int createUnits(Player owner, bool lightsOn = true) {
    return abi::member<0x49D5D0, int>(block_, owner.index(), lightsOn ? 1 : 0);
  }
  /// The raw engine UnitBlock object pointer - used by the Group::addUnits / setTargCount / recordUnitBlock ops.
  [[nodiscard]] void* raw() { return block_; }

private:
  std::vector<group_detail::EngineUnitRecord> table_;            // must outlive block_ (engine holds the pointer)
  alignas(8) unsigned char block_[136] = {};                    // the engine UnitBlock object (probe-verified size)
};

/// Handle to an engine ScGroup (an ScStub index). FightGroup / MiningGroup / BuildingGroup all share this one
/// handle; the methods are grouped by which kind of group they apply to (a method only does something on the
/// matching group type - e.g. setupMining on a MiningGroup, recordBuilding on a BuildingGroup). Create one with
/// createFightGroup / createMiningGroup / createBuildingGroup below.
class Group {
public:
  static constexpr int kNilIndex = 255;   ///< ScStubList::NilIndex - note index 0 is a *valid* group
  constexpr Group() = default;
  constexpr explicit Group(int id) : id_{ id } {}
  [[nodiscard]] constexpr int  id()    const noexcept { return id_; }
  [[nodiscard]] constexpr bool valid() const noexcept { return (id_ >= 0) && (id_ != kNilIndex); }

  // --- roster (ScGroup) ---
  void takeUnit(Unit u)   { int id = id_; abi::member<0x479AF0, void>(&id, u.id()); }  ///< Move a unit into this group.
  void removeUnit(Unit u) { int id = id_; abi::member<0x479BD0, void>(&id, u.id()); }  ///< Remove a unit from this group.
  void setLights(bool on) { int id = id_; abi::member<0x479B60, void>(&id, on ? 1 : 0); }  ///< Headlights for the group.
  /// Auto-delete this group once it has no units left (vs. keeping it around to refill with reinforcements).
  void setDeleteWhenEmpty(bool s) { int id = id_; abi::member<0x479B80, void>(&id, s ? 1 : 0); }
  [[nodiscard]] int  totalUnitCount() const { int id = id_; return abi::member<0x479A10, int>(&id); }
  [[nodiscard]] bool isUnderAttack()  const { int id = id_; return abi::member<0x479AE0, int>(&id) != 0; }

  /// Move ALL units out of `src` and into this group. (ScGroup::TakeAllUnits @0x479BA0 - takes ScGroup*.)
  void takeAllUnits(Group src) { int id = id_; int s = src.id_; abi::member<0x479BA0, void>(&id, &s); }
  /// Count the group's units of a given classification (0 = vehicles, 1 = buildings, ... ClassDefdInTethys).
  /// (ScGroup::UnitCount @0x4799F0.)
  [[nodiscard]] int unitCount(int classification) const { int id = id_; return abi::member<0x4799F0, int>(&id, classification); }
  /// How many of (unitType, weaponType) the group should keep on strength - drives reinforcement production from
  /// the matching BuildingGroup. (ScGroup::SetTargCount @0x479C70.)
  void setTargCount(MapID unitType, MapID weaponType, int count) {
    int id = id_; abi::member<0x479C70, void>(&id, int(unitType), int(weaponType), count);
  }
  void clearTargCount() { int id = id_; abi::member<0x479CB0, void>(&id); }  ///< Clear the target-count table.
  /// Create a whole UnitBlock's worth of units (for this group's owner) and add them. (AddUnits @0x4799D0.)
  void addUnits(UnitBlock& block) { int id = id_; abi::member<0x4799D0, void>(&id, block.raw()); }
  /// Set per-(type,weapon) target counts from a UnitBlock - drives reinforcement. (SetTargCount @0x479C40.)
  void setTargCount(UnitBlock& block) { int id = id_; abi::member<0x479C40, void>(&id, block.raw()); }
  /// [BuildingGroup] Record a UnitBlock for the group's factories to (re)produce. (RecordUnitBlock @0x47A330.)
  void recordUnitBlock(UnitBlock& block) { int id = id_; abi::member<0x47A330, void>(&id, block.raw()); }

  // --- FightGroup combat strategy ---
  void setAttackType(MapID t) { int id = id_; abi::member<0x479F60, void>(&id, int(t)); }  ///< Which unit types to attack.
  void setTargetUnit(Unit u)  { int id = id_; abi::member<0x479F00, void>(&id, u.id()); }  ///< Specific target for doAttackUnit.
  void setTargetGroup(Group g){ int id = id_; abi::member<0x479F90, void>(&id, g.id_); }   ///< Target group for doGuardGroup (by value).
  void doAttackEnemy()        { int id = id_; abi::member<0x47A080, void>(&id); }  ///< Hunt the enemy (pair with setAttackType).
  void doAttackUnit()         { int id = id_; abi::member<0x47A060, void>(&id); }  ///< Attack the set target unit.
  void doGuardUnit()          { int id = id_; abi::member<0x47A040, void>(&id); }  ///< Guard the set target unit.
  void doGuardGroup()         { int id = id_; abi::member<0x47A020, void>(&id); }  ///< Guard the set target group.
  void doGuardRect()          { int id = id_; abi::member<0x479FC0, void>(&id); }  ///< Guard the set idle rect.
  void doPatrolOnly()         { int id = id_; abi::member<0x47A000, void>(&id); }  ///< Patrol the assigned route only.
  void doExitMap()            { int id = id_; abi::member<0x479FE0, void>(&id); }  ///< March the group off the map edge.
  void setCombineFire()       { int id = id_; abi::member<0x479CC0, void>(&id); }  ///< Concentrate the group's fire.
  void clearCombineFire()     { int id = id_; abi::member<0x479CF0, void>(&id); }
  void setFollowMode(int m)   { int id = id_; abi::member<0x479F30, void>(&id, m); }       ///< Formation/follow mode.
  /// [FightGroup] Patrol a fixed loop through up to 8 waypoints (visible tiles); the group cycles them until given
  /// another order. The route is kept alive internally (the engine holds the pointer). (SetPatrolMode @0x479D20.)
  void setPatrolMode(std::initializer_list<Location> waypoints) {
    auto& h = group_detail::patrolStore().emplace_back();
    for (Location w : waypoints) { if (h.wps.size() >= 8) break; h.wps.push_back({ w.engineX(), w.engineY() }); }
    h.wps.push_back({ -1, 0 });                              // x = -1 terminates the waypoint list
    h.route = { 0, h.wps.data() };                           // unknown1 = 0 (length comes from the -1 terminator)
    int id = id_; abi::member<0x479D20, void>(&id, &h.route);
  }
  /// [FightGroup] Stop patrolling. (ClearPatrolMode @0x479D80.)
  void clearPatrolMode() { int id = id_; abi::member<0x479D80, void>(&id); }
  /// [FightGroup] The idle/home rect the group falls back to (SetRect @0x479DA0). Corners in visible tiles.
  void setIdleRect(Location tl, Location br) { int id = id_; int r[4]{ tl.engineX(), tl.engineY(), br.engineX(), br.engineY() }; abi::member<0x479DA0, void>(&id, &r[0]); }
  /// [FightGroup] Add a rect to guard / clear the guarded set (AddGuardedRect @0x479E60 / ClearGuardedRects @0x479EE0).
  void addGuardedRect(Location tl, Location br) { int id = id_; int r[4]{ tl.engineX(), tl.engineY(), br.engineX(), br.engineY() }; abi::member<0x479E60, void>(&id, &r[0]); }
  void clearGuardedRects()    { int id = id_; abi::member<0x479EE0, void>(&id); }

  // --- MiningGroup ---
  /// [MiningGroup] Run an ore haul: trucks shuttle `mine` -> `smelter`, unloading in the rect by the smelter.
  /// The Unit form (pass the mine + smelter buildings) is the simplest. (MiningGroup::Setup @0x47A100.)
  void setupMining(Unit mine, Unit smelter, Location areaTL, Location areaBR) {
    int id = id_; int r[4]{ areaTL.engineX(), areaTL.engineY(), areaBR.engineX(), areaBR.engineY() };
    abi::member<0x47A100, void>(&id, mine.id(), smelter.id(), &r[0]);    // void(Unit, Unit, const MapRect&)
  }

  // --- BuildingGroup ---
  /// [BuildingGroup] The base-area rect the group builds within. (SetRect @0x47A2E0.)
  void setBuildRect(Location tl, Location br) { int id = id_; int r[4]{ tl.engineX(), tl.engineY(), br.engineX(), br.engineY() }; abi::member<0x47A2E0, void>(&id, &r[0]); }
  /// [BuildingGroup] Record a structure for the group to (re)build at `where`. (RecordBuilding @0x47A3E0.)
  /// For an ore mine use `recordMine` instead - mines need the CommonOreMine quirk handled (see below).
  void recordBuilding(Location where, MapID type, MapID cargoOrWeapon = MapID::None) {
    int id = id_; int loc[2]{ where.engineX(), where.engineY() };
    abi::member<0x47A3E0, void>(&id, &loc[0], int(type), int(cargoOrWeapon));   // void(Location&, MapID, MapID)
  }
  /// [BuildingGroup] Record a mine for the group to build on the mining beacon at `beacon`. Use this for BOTH
  /// common AND rare ore mines: the recorded type is ALWAYS `CommonOreMine` and the BEACON sitting under `beacon`
  /// decides whether the built mine ends up common or rare. Recording `RareOreMine` directly does NOT work - a
  /// long-standing OP2 engine quirk that the original missions flag with a literal "BUG: set mine type to common
  /// even for a rare ore mine" comment (RisingFromTheAshes/SetupAIBase.cpp). A beacon must already exist at
  /// `beacon` (Game::createMine, a BeaconSpec in a Base, or a map deposit). Pair with a MiningGroup (setupMining)
  /// for trucks to actually haul the ore. (Thin wrapper over recordBuilding -> RecordBuilding @0x47A3E0.)
  void recordMine(Location beacon) { recordBuilding(beacon, MapID::CommonOreMine); }
  /// [BuildingGroup] Have this group's factories reinforce `target` FightGroup. `priority` 1..0xFFFF (high =
  /// more urgent). NOTE: priority 0 HANGS OP2, so we clamp to >= 1. (RecordVehReinforceGroup @0x47A440.)
  void recordVehReinforceGroup(Group target, int priority) {
    if (priority < 1) priority = 1;
    int id = id_; int t = target.id_; abi::member<0x47A440, void>(&id, &t, priority);   // void(ScGroup&, int)
  }
  /// [BuildingGroup] Stop reinforcing `target`. (UnRecordVehGroup @0x47A460.)
  void unRecordVehGroup(Group target) { int id = id_; int t = target.id_; abi::member<0x47A460, void>(&id, &t); }

private:
  int id_ = kNilIndex;
};

namespace group_detail {
/// Mirror of the engine's `_Player` handle ({int id_; uint8 check_[8]}). Passed BY VALUE to the group factories.
struct PlayerArg { int id; unsigned char check[8]; };

/// Create a group for `playerIndex` via the sret factory at `Addr`.
template <std::uintptr_t Addr>
inline Group create(int playerIndex) {
  PlayerArg pa{ playerIndex, {} };     // _Player{playerIndex}: id set, capability flags default-zero
  int id = Group::kNilIndex;
  abi::callFastSret<Addr>(&id, pa);    // sret pointer in ECX (&id), _Player by value on the stack
  return Group{ id };
}
} // namespace group_detail

/// Create a combat group owned by `owner` (CreateFightGroup @0x47A0A0). Add units, then doAttackEnemy()/guard/...
inline Group createFightGroup(Player owner)    { return group_detail::create<0x47A0A0>(owner.index()); }
/// Create a mining group owned by `owner` (CreateMiningGroup @0x47A0D0).
inline Group createMiningGroup(Player owner)   { return group_detail::create<0x47A0D0>(owner.index()); }
/// Create a base-building group owned by `owner` (CreateBuildingGroup @0x47A2B0).
inline Group createBuildingGroup(Player owner) { return group_detail::create<0x47A2B0>(owner.index()); }

/// One unit type that can appear in a Pinwheel wave (a Pinwheel MrRec). A composition is a list of these.
struct WaveUnit { MapID type; MapID weapon = MapID::None; };
/// A Pinwheel wave route (a PWDef): up to four waypoints the wave travels through, plus three timing values.
/// Leave later waypoints equal to the last real one for a shorter path.
struct WavePath { Location p1, p2, p3, p4; int time1 = 0, time2 = 0, time3 = 0; };

namespace pinwheel_detail {
/// Engine MrRec (probe-verified 16 bytes); a -1 in `u1` terminates the list.
struct EngineMrRec { int unitType, weaponType, u1, u2; };
static_assert(sizeof(EngineMrRec) == 16, "MrRec must be 16 bytes");
/// Engine PWDef (probe-verified 44 bytes); x1 = -1 terminates the list.
struct EnginePWDef { int x1, y1, x2, y2, x3, y3, x4, y4, time1, time2, time3; };
static_assert(sizeof(EnginePWDef) == 44, "PWDef must be 44 bytes");

// The engine KEEPS a pointer to these wave tables (real missions declare them static), so the table we pass must
// outlive the call - for the whole mission. We park each table in an append-only store; the data pointers stay
// stable across growth because moving a std::vector preserves its heap buffer. (Set-once at init; tiny + leak-free
// at DLL unload.)
inline std::vector<std::vector<EnginePWDef>>& pwStore() { static std::vector<std::vector<EnginePWDef>> s; return s; }
inline std::vector<std::vector<EngineMrRec>>& mrStore() { static std::vector<std::vector<EngineMrRec>> s; return s; }
} // namespace pinwheel_detail

/// The Pinwheel wave-attack coordinator: a separate ScStub strategy that launches scripted attack waves at the
/// human player. Define the wave routes (setPoints) and unit composition (setAttackComp) FIRST, then launch them
/// on a timer (setWavePeriod) or on demand (sendWaveNow). (Its own {int id_} handle, like Group.)
class Pinwheel {
public:
  static constexpr int kNilIndex = 255;
  constexpr Pinwheel() = default;
  constexpr explicit Pinwheel(int id) : id_{ id } {}
  [[nodiscard]] constexpr int  id()    const noexcept { return id_; }
  [[nodiscard]] constexpr bool valid() const noexcept { return (id_ >= 0) && (id_ != kNilIndex); }

  // ---- wave SETUP (call these BEFORE arming/launching) ----
  // CRITICAL: define the routes (setPoints) and a composition (setAttackComp) before setWavePeriod/sendWaveNow.
  // Arming or launching a Pinwheel with no waves defined makes the engine divide by zero assembling a zero-unit
  // wave (INT_DIVIDE_BY_ZERO near MapUnit::CalculateStrength).

  /// Define the wave ROUTES - the paths a wave can travel (Pinwheel::SetPoints @0x47A8B0). At least one route is
  /// required before launching. Points are visible tiles.
  void setPoints(std::initializer_list<WavePath> routes) {
    auto& store = pinwheel_detail::pwStore();
    store.emplace_back();
    auto& a = store.back();                                        // persists for the mission (engine keeps the ptr)
    a.reserve(routes.size() + 1);
    for (const WavePath& r : routes)
      a.push_back({ r.p1.engineX(), r.p1.engineY(), r.p2.engineX(), r.p2.engineY(), r.p3.engineX(), r.p3.engineY(),
                    r.p4.engineX(), r.p4.engineY(), r.time1, r.time2, r.time3 });
    a.push_back({ -1, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1 });          // x1 = -1 terminates the list
    int id = id_; abi::member<0x47A8B0, void>(&id, a.data());
  }
  /// Define the ATTACK-wave unit composition: each wave fields between `minUnits` and `maxUnits` drawn from `mix`.
  /// (Pinwheel::SetAttackComp @0x47A9B0.)
  void setAttackComp(int minUnits, int maxUnits, std::initializer_list<WaveUnit> mix) { comp<0x47A9B0>(minUnits, maxUnits, mix); }
  /// Guard-wave composition (SetGuardComp @0x47A9E0) / sapper-wave composition (SetSapperComp @0x47AA10).
  void setGuardComp(int minUnits, int maxUnits, std::initializer_list<WaveUnit> mix)  { comp<0x47A9E0>(minUnits, maxUnits, mix); }
  void setSapperComp(int minUnits, int maxUnits, std::initializer_list<WaveUnit> mix) { comp<0x47AA10>(minUnits, maxUnits, mix); }

  /// Fraction (0..100) of available units committed per attack wave. (SetAttackFraction @0x47AA60.)
  void setAttackFraction(int pct) { int id = id_; abi::member<0x47AA60, void>(&id, pct); }
  /// Ticks the wave waits at a contact point before pressing on. (SetContactDelay @0x47A990.)
  void setContactDelay(int ticks) { int id = id_; abi::member<0x47A990, void>(&id, ticks); }

  // ---- launch (only AFTER setPoints + a composition) ----
  /// Launch an attack wave right now. (Pinwheel::SendWaveNow @0x47AA40.)
  void sendWaveNow(int waveIndex = 0) { int id = id_; abi::member<0x47AA40, void>(&id, waveIndex); }
  /// Set the random interval (in ticks) between automatic waves. (Pinwheel::SetWavePeriod @0x47A970.)
  void setWavePeriod(int minTicks, int maxTicks) { int id = id_; abi::member<0x47A970, void>(&id, minTicks, maxTicks); }

private:
  /// Shared builder for the *Comp methods: build a -1-terminated MrRec table and call `Addr(minUnits, maxUnits, table)`.
  template <std::uintptr_t Addr>
  void comp(int minUnits, int maxUnits, std::initializer_list<WaveUnit> mix) {
    auto& store = pinwheel_detail::mrStore();
    store.emplace_back();
    auto& a = store.back();                                        // persists for the mission (engine keeps the ptr)
    a.reserve(mix.size() + 1);
    for (const WaveUnit& u : mix) a.push_back({ int(u.type), int(u.weapon), 0, 0 });
    a.push_back({ int(MapID::None), int(MapID::None), -1, -1 });    // u1 = -1 terminates the list
    int id = id_; abi::member<Addr, void>(&id, minUnits, maxUnits, a.data());
  }
  int id_ = kNilIndex;
};

/// Create a Pinwheel wave-attacker owned by `owner` (CreatePinwheel @0x47A880). NOTE: unlike the Create*Group
/// factories (which take `_Player` BY VALUE), CreatePinwheel takes it BY CONST REFERENCE - so we pass &pa.
inline Pinwheel createPinwheel(Player owner) {
  group_detail::PlayerArg pa{ owner.index(), {} };
  int id = Pinwheel::kNilIndex;
  abi::callFastSret<0x47A880>(&id, &pa);   // sret ptr in ECX (&id); _Player by const ref -> &pa
  return Pinwheel{ id };
}

} // namespace op2
