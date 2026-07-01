#pragma once
// op2/mining.hpp - a self-managing AI MINING OPERATION in one call.
//
// createMiningOperation(owner, smelter, mine, idleTL, idleBR, numTrucks[, autoHeal]) sets up the whole thing:
//   * ensures an ore beacon at `mine` (surveyed for `owner`) and a Common Ore Mine on it,
//   * ensures a Common Ore Smelter at `smelter`,
//   * creates a MiningGroup hauling mine -> smelter (idle rect [idleTL,idleBR]),
//   * puts `numTrucks` Cargo Trucks on the haul.
//
// With autoHeal (default on), calling tickMiningOperations() each frame from AIProc keeps it alive: it rebuilds a
// destroyed mine or smelter and replaces lost trucks, rebinds the haul, and mining resumes on its own.
//
// WHY createUnit (not factory production): testing showed the engine will not queue-build utility vehicles
// (Cargo Truck / ConVec) or complete a scripted ConVec structure-build for an AI player without the full
// CanBuildUnit + research + factory-output precondition chain the campaign AI uses; createUnit is the reliable
// path and is functionally identical (the unit appears and joins the operation). See private/ notes.
//
// NOTE - the trucks stop when the ore store is FULL: a MiningGroup parks its trucks in the idle rect once the
// owner's Common Ore store hits its cap (the engine's own behaviour, not a fault). A live colony spends ore as
// fast as it comes in, so the trucks never idle for long; but a bare demo with no consumer will look "stopped".
// If you want continuous hauling without a real economy, drain the store yourself (see samples/Mining).
//
// Usage:
//   InitProc: op2::createMiningOperation(ai, {48,85}, {44,88}, {45,86}, {49,90}, 3);
//   AIProc:   op2::tickMiningOperations();

#include "op2.hpp"
#include "op2/groups.hpp"

#include <deque>
#include <vector>

namespace op2 {

class MiningOperation {
public:
  /// Build the operation (called by createMiningOperation). Reuses any mine/smelter already present at the tiles.
  void setup(Player owner, Location smelter, Location mine, Location idleTL, Location idleBR, int numTrucks, bool autoHeal) {
    owner_ = owner; smelterLoc_ = smelter; mineLoc_ = mine; idleTL_ = idleTL; idleBR_ = idleBR;
    numTrucks_ = numTrucks; autoHeal_ = autoHeal;
    ensureMine();
    ensureSmelter();
    group_ = createMiningGroup(owner_);
    rebindHaul();
    for (int i = 0; i < numTrucks_; ++i) spawnTruck(i);
    active_ = true;
  }

  /// Per-frame auto-heal (no-op if autoHeal is off): rebuild a missing mine/smelter, replace lost trucks.
  void update() {
    if (!active_ || !autoHeal_) return;
    const bool rebuilt = ensureMine() | ensureSmelter();   // (| not || - always run both)
    if (rebuilt) rebindHaul();                              // re-point the trucks at the fresh mine/smelter
    for (int i = liveTrucks(); i < numTrucks_; ++i) spawnTruck(i);
  }

  [[nodiscard]] bool  valid() const { return active_; }
  [[nodiscard]] Group group() const { return group_; }

private:
  Player owner_{ 0 };
  Location smelterLoc_{}, mineLoc_{}, idleTL_{}, idleBR_{};
  int   numTrucks_ = 0;
  bool  autoHeal_ = false, active_ = false;
  Group group_;
  Unit  mineUnit_, smelterUnit_;      // tracked by handle - the reliable way to tell "is it still there?"
  std::vector<int> truckIds_;

  static bool near(Location a, Location b, int d) { return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) <= d * d; }

  // Any live unit of `type` owned by us at/near `where` (used to adopt something the mission pre-placed).
  Unit ownedAt(MapID type, Location where, int d = 3) const {
    for (Unit u : Game::unitsOfType(type))
      if (u.ownerId() == owner_.index() && u.isLive() && near(u.location(), where, d)) return u;
    return Unit{};
  }

  bool beaconPresent() const {
    for (Unit b : Game::beacons()) if (near(b.location(), mineLoc_, 2)) return true;
    return false;
  }

  // Ensure a live mine exists; returns true if it had to (re)build one.
  bool ensureMine() {
    if (mineUnit_.isLive()) return false;
    if (Unit ex = ownedAt(MapID::CommonOreMine, mineLoc_); ex.valid()) { mineUnit_ = ex; return false; }
    if (!beaconPresent())
      if (auto b = Game::createMine(mineLoc_, abi::MineType::CommonOre)) ignore(b->survey(owner_.index()));
    if (auto m = Game::createUnit(MapID::CommonOreMine, mineLoc_, owner_)) { mineUnit_ = *m; return true; }
    return false;
  }
  // Ensure a live smelter exists; returns true if it had to (re)build one.
  bool ensureSmelter() {
    if (smelterUnit_.isLive()) return false;
    if (Unit ex = ownedAt(MapID::CommonOreSmelter, smelterLoc_); ex.valid()) { smelterUnit_ = ex; return false; }
    if (auto s = Game::createUnit(MapID::CommonOreSmelter, smelterLoc_, owner_)) { smelterUnit_ = *s; return true; }
    return false;
  }

  void rebindHaul() {
    if (mineUnit_.isLive() && smelterUnit_.isLive()) group_.setupMining(mineUnit_, smelterUnit_, idleTL_, idleBR_);
  }

  int liveTrucks() const {
    int n = 0;
    for (int id : truckIds_) if (Unit{ id, owner_.index() }.isLive()) ++n;
    return n;
  }
  void spawnTruck(int i) {
    const int w = (idleBR_.x - idleTL_.x) + 1, h = (idleBR_.y - idleTL_.y) + 1;
    const Location at{ idleTL_.x + (i % (w > 0 ? w : 1)), idleTL_.y + ((i / (w > 0 ? w : 1)) % (h > 0 ? h : 1)) };
    if (auto t = Game::createUnit(MapID::CargoTruck, at, owner_)) { group_.takeUnit(*t); truckIds_.push_back(t->id()); }
  }
};

namespace mining_detail {
/// The library owns every operation (stable addresses in a deque) so tickMiningOperations() can drive them all.
inline std::deque<MiningOperation>& all() { static std::deque<MiningOperation> a; return a; }
} // namespace mining_detail

/// Set up a self-managing mining operation for `owner`: a mine on `mine`, a smelter at `smelter`, a MiningGroup
/// hauling between them within the idle rect [idleTL,idleBR], and `numTrucks` Cargo Trucks. With `autoHeal` on
/// (default), tickMiningOperations() (call it from AIProc) rebuilds the mine/smelter and replaces lost trucks.
inline MiningOperation& createMiningOperation(Player owner, Location smelter, Location mine,
                                              Location idleTL, Location idleBR, int numTrucks, bool autoHeal = true) {
  MiningOperation& op = mining_detail::all().emplace_back();
  op.setup(owner, smelter, mine, idleTL, idleBR, numTrucks, autoHeal);
  return op;
}

/// Drive every auto-healing mining operation one step. Call once per frame from AIProc.
inline void tickMiningOperations() { for (MiningOperation& op : mining_detail::all()) op.update(); }

} // namespace op2
