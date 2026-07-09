#pragma once
// op2/mining.hpp - a self-managing AI MINING OPERATION in one call.
//
// createMiningOperation(owner, smelter, mine, idle, numTrucks[, autoHeal[, buildRect]]) sets up the whole thing:
//   * ensures an ore beacon at `mine` (surveyed for `owner`) and a Common Ore Mine on it,
//   * ensures a Common Ore Smelter at `smelter`,
//   * creates a MiningGroup hauling mine -> smelter (trucks stage/haul inside the `idle` rect),
//   * puts `numTrucks` Cargo Trucks on the haul.
// All of the above is START-OF-GAME PLACEMENT - the same as laying out a starting colony, and the only place this
// module ever creates a unit directly.
//
// AUTO-HEAL (default on) is done the LEGITIMATE way - through the engine's own AI build system, with NO units ever
// spawned into existence mid-game. createMiningOperation adopts the AI's existing powered Structure Factory +
// Vehicle Factory + ConVec into a BuildingGroup, sets a build rect, and RECORDS the smelter + mine and reinforces
// the truck haul. From then on the ENGINE does the work: if the smelter or mine is destroyed, the group's ConVec
// pulls a kit from the Structure Factory and rebuilds it at the recorded tile; if the haul loses a truck, the
// Vehicle Factory produces a replacement. tickMiningOperations() (call it from AIProc) only re-binds the
// MiningGroup to a mine/smelter the engine has rebuilt, so hauling resumes on its own. (This mirrors how the stock
// campaign missions drive an AI base - see RisingFromTheAshes.)
//
// REQUIREMENTS for auto-heal: the AI must own a powered (tube-connected) Structure Factory, a Vehicle Factory, and
// a ConVec. createMiningOperation ADOPTS them from the base when present; if the base has none it places them as a
// best-effort fallback (start placement), but an already-powered base factory is the intended path. NOTE: adopting
// a factory MOVES it into the operation's BuildingGroup - don't also drive those same factories from another group.
//
// WHY the engine's build system (not direct produce()/createUnit): the campaign AI build system rebuilds recorded
// structures and restocks reinforced groups on its own, with real factory production and no cheating. Driving
// produce()/build() by hand (as DotNetMissionSDK does) also works but reimplements what the engine already does.
//
// Usage:
//   InitProc: op2::createMiningOperation(ai, {48,85}, {44,88}, {{45,86},{49,90}}, 3);
//   AIProc:   op2::tickMiningOperations();

#include "op2.hpp"
#include "op2/groups.hpp"

#include <algorithm>
#include <deque>

namespace op2 {

class MiningOperation {
public:
  /// Build the operation (called by createMiningOperation). `buildRect` empty ({}) -> derived around mine+smelter.
  void setup(Player owner, Location smelter, Location mine, Region idle, int numTrucks, bool autoHeal, Region buildRect) {
    owner_ = owner; smelterLoc_ = smelter; mineLoc_ = mine; idle_ = idle; numTrucks_ = numTrucks; autoHeal_ = autoHeal;
    buildRect_ = isEmptyRect(buildRect) ? deriveBuildRect() : buildRect;

    // ---- start-of-game placement (allowed: this is initial colony layout) ----
    placeMine();
    placeSmelter();
    group_ = createMiningGroup(owner_);
    rebindHaul();
    for (int i = 0; i < numTrucks_; ++i) placeTruck(i);

    // ---- auto-heal: hand rebuild/restock to the engine's AI build system (no mid-game spawning) ----
    if (autoHeal_) setupHealing();
    active_ = true;
  }

  /// Per-frame upkeep (call from AIProc). Never creates a unit - the engine rebuilds/restocks. It only re-binds the
  /// MiningGroup to a mine/smelter the engine has rebuilt, so hauling resumes automatically.
  void update() {
    if (!active_ || !autoHeal_) return;
    rebindIfRebuilt();
  }

  [[nodiscard]] bool  valid() const { return active_; }
  [[nodiscard]] Group group() const { return group_; }
  [[nodiscard]] Group buildGroup() const { return buildGroup_; }

private:
  Player owner_{ 0 };
  Location smelterLoc_{}, mineLoc_{};
  Region idle_{}, buildRect_{};
  int   numTrucks_ = 0;
  bool  autoHeal_ = false, active_ = false;
  Group group_, buildGroup_;
  Unit  mineUnit_, smelterUnit_;      // tracked by handle - the reliable way to tell "is it still there?"

  // (Named isNear, not `near` - <windows.h> #defines `near` away, and a mission is free to include it.)
  static bool isNear(Location a, Location b, int d) { return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) <= d * d; }
  static bool isEmptyRect(Region r) { return r.topLeft.x == 0 && r.topLeft.y == 0 && r.bottomRight.x == 0 && r.bottomRight.y == 0; }

  // Any live unit of `type` owned by us at/near `where` (used to adopt something already standing there).
  Unit ownedAt(MapID type, Location where, int d = 3) const {
    for (Unit u : Game::unitsOfType(type))
      if (u.ownerId() == owner_.index() && u.isLive() && isNear(u.location(), where, d)) return u;
    return Unit{};
  }

  bool beaconPresent() const {
    for (Unit b : Game::beacons()) if (isNear(b.location(), mineLoc_, 2)) return true;
    return false;
  }

  // A build rect big enough to hold the smelter, the mine, and the idle rect (+ margin) so the engine may (re)build.
  Region deriveBuildRect() const {
    const int minx = std::min({ smelterLoc_.x, mineLoc_.x, idle_.topLeft.x, idle_.bottomRight.x });
    const int miny = std::min({ smelterLoc_.y, mineLoc_.y, idle_.topLeft.y, idle_.bottomRight.y });
    const int maxx = std::max({ smelterLoc_.x, mineLoc_.x, idle_.topLeft.x, idle_.bottomRight.x });
    const int maxy = std::max({ smelterLoc_.y, mineLoc_.y, idle_.topLeft.y, idle_.bottomRight.y });
    const int m = 4;
    return { { std::max(1, minx - m), std::max(1, miny - m) }, { maxx + m, maxy + m } };
  }

  // ---- start-of-game placement (createUnit is fine here - initial layout, not mid-game healing) ----
  void placeMine() {
    if (Unit ex = ownedAt(MapID::CommonOreMine, mineLoc_); ex.valid()) { mineUnit_ = ex; return; }
    if (!beaconPresent())
      if (auto b = Game::createMine(mineLoc_, abi::MineType::CommonOre)) ignore(b->survey(owner_.index()));
    if (auto m = Game::createUnit(MapID::CommonOreMine, mineLoc_, owner_)) mineUnit_ = *m;
  }
  void placeSmelter() {
    if (Unit ex = ownedAt(MapID::CommonOreSmelter, smelterLoc_); ex.valid()) { smelterUnit_ = ex; return; }
    if (auto s = Game::createUnit(MapID::CommonOreSmelter, smelterLoc_, owner_)) smelterUnit_ = *s;
  }
  void placeTruck(int i) {
    const int w = (idle_.bottomRight.x - idle_.topLeft.x) + 1, h = (idle_.bottomRight.y - idle_.topLeft.y) + 1;
    const Location at{ idle_.topLeft.x + (i % (w > 0 ? w : 1)), idle_.topLeft.y + ((i / (w > 0 ? w : 1)) % (h > 0 ? h : 1)) };
    if (auto t = Game::createUnit(MapID::CargoTruck, at, owner_)) group_.takeUnit(*t);
  }

  void rebindHaul() {
    if (mineUnit_.isLive() && smelterUnit_.isLive())
      group_.setupMining(mineUnit_, smelterUnit_, idle_.topLeft, idle_.bottomRight);
  }

  // ---- auto-heal setup: the engine's AI build system rebuilds/restocks from here on ----
  // Nearest live, powered, ready factory/ConVec of `type` we own (to adopt into the build group).
  Unit adopt(MapID type) const {
    Unit best; int bestD = 1 << 30;
    for (Unit u : Game::unitsOfType(type)) {
      if (u.ownerId() != owner_.index() || !u.isLive() || u.isDisabled() || u.underConstruction()) continue;
      const Location p = u.location();
      const int d = (p.x - smelterLoc_.x) * (p.x - smelterLoc_.x) + (p.y - smelterLoc_.y) * (p.y - smelterLoc_.y);
      if (d < bestD) { bestD = d; best = u; }
    }
    return best;
  }
  void setupHealing() {
    buildGroup_ = createBuildingGroup(owner_);

    // Adopt the AI base's existing powered builders; fall back to placing them only if the base has none.
    Unit sf = adopt(MapID::StructureFactory);
    Unit vf = adopt(MapID::VehicleFactory);
    Unit cv = adopt(MapID::ConVec);
    if (!sf.isLive()) if (auto u = Game::createUnit(MapID::StructureFactory, { smelterLoc_.x + 2, smelterLoc_.y }, owner_)) sf = *u;
    if (!vf.isLive()) if (auto u = Game::createUnit(MapID::VehicleFactory,   { smelterLoc_.x + 5, smelterLoc_.y }, owner_)) vf = *u;
    if (!cv.isLive()) if (auto u = Game::createUnit(MapID::ConVec,           { smelterLoc_.x + 1, smelterLoc_.y + 1 }, owner_)) cv = *u;
    if (sf.isLive()) buildGroup_.takeUnit(sf);
    if (vf.isLive()) buildGroup_.takeUnit(vf);
    if (cv.isLive()) buildGroup_.takeUnit(cv);

    buildGroup_.setBuildRect(buildRect_.topLeft, buildRect_.bottomRight);
    buildGroup_.recordBuilding(smelterLoc_, MapID::CommonOreSmelter);   // engine rebuilds the smelter if destroyed
    buildGroup_.recordMine(mineLoc_);                                   // engine rebuilds the mine if destroyed
    buildGroup_.recordVehReinforceGroup(group_, 3);                     // Vehicle Factory restocks lost trucks
  }

  // If the engine rebuilt a mine/smelter we track, adopt the fresh handle and re-bind the haul (no creation).
  void rebindIfRebuilt() {
    bool changed = false;
    if (!mineUnit_.isLive())    if (Unit u = ownedAt(MapID::CommonOreMine, mineLoc_);       u.valid()) { mineUnit_ = u; changed = true; }
    if (!smelterUnit_.isLive()) if (Unit u = ownedAt(MapID::CommonOreSmelter, smelterLoc_); u.valid()) { smelterUnit_ = u; changed = true; }
    if (changed) rebindHaul();
  }
};

namespace mining_detail {
/// The library owns every operation (stable addresses in a deque) so tickMiningOperations() can drive them all.
inline std::deque<MiningOperation>& all() { static std::deque<MiningOperation> a; return a; }
} // namespace mining_detail

/// Set up a self-managing mining operation for `owner`: a mine on `mine`, a smelter at `smelter`, a MiningGroup
/// hauling between them within the idle rect `idle`, and `numTrucks` Cargo Trucks. With `autoHeal` on (default),
/// the engine's AI build system rebuilds the mine/smelter and restocks trucks (no mid-game spawning) - call
/// tickMiningOperations() from AIProc to keep the haul bound. `buildRect` bounds where the engine may (re)build;
/// leave it {} to auto-derive an area around the mine + smelter. This is the canonical form.
inline MiningOperation& createMiningOperation(Player owner, Location smelter, Location mine, Region idle,
                                              int numTrucks, bool autoHeal = true, Region buildRect = {}) {
  MiningOperation& op = mining_detail::all().emplace_back();
  op.setup(owner, smelter, mine, idle, numTrucks, autoHeal, buildRect);
  return op;
}

/// Two-corner overload of createMiningOperation - the idle rect is given as its top-left and bottom-right tiles
/// instead of a Region. Equivalent to the Region form; kept for callers that pass corners directly.
inline MiningOperation& createMiningOperation(Player owner, Location smelter, Location mine,
                                              Location idleTL, Location idleBR, int numTrucks, bool autoHeal = true) {
  return createMiningOperation(owner, smelter, mine, Region{ idleTL, idleBR }, numTrucks, autoHeal);
}

/// Drive every auto-healing mining operation one step. Call once per frame from AIProc.
inline void tickMiningOperations() { for (MiningOperation& op : mining_detail::all()) op.update(); }

} // namespace op2
