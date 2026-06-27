// Mining/mission.cpp - "AI Mining", a TitanAPI sample of how to script a WORKING ore operation for an AI player.
// You (a human) watch a Plymouth AI build TWO ore mines - one each way - and haul from the first:
//
//   1. Survey - the AI's Robo-Surveyor drives to deposit 1 (44,88); on arrival it is revealed.
//   2. Mine 1, the Robo-Miner way - once surveyed, the AI's Robo-Miner deploys a mine on deposit 1.
//   3. Mine 2, the BUILDING-GROUP way - a BuildingGroup (handed a ConVec + a Robo-Miner + a build rect) builds a
//      mine on deposit 2 (54,88) via recordMine. This is the "build a mine through a building group" pattern.
//   4. MiningGroup - once mine 1 stands, a MiningGroup hauls its ore: the AI creates the group, points it at the
//      mine + smelter with an idle rect (setupMining), then takes its Cargo Trucks (placed at load inside that
//      idle rect) into the group. The group hauls mine 1 -> smelter on its own.
//
// This is the AI counterpart to a human's per-truck Unit::mine route (see samples/ColdFront): the MiningGroup is
// the right tool when an AI owns the operation. NOTE on behavior: a MiningGroup parks its trucks in the idle rect
// when the ore store is FULL (vs a Unit::mine CargoRoute, where a truck waits at the smelter dock). So this demo
// simulates the colony CONSUMING refined ore (a real AI spends it on construction/production) to keep demand up,
// so the trucks haul indefinitely instead of parking once the store fills.
//
// MiningGroup recipe:
//   auto mg = createMiningGroup(aiPlayer);
//   mg.setupMining(mine, smelter, idleTL, idleBR);          // Setup FIRST
//   for (each truck in the idle rect) mg.takeUnit(truck);   // then take the trucks in
// The trucks must sit INSIDE the idle rect when taken in (this sample places them there at load; the campaign
// helper UnitHelper::SetupMiningGroup spawns them there fresh - either works).

#include "op2.hpp"            // the TitanAPI Layer 2 facade
#include "op2/base.hpp"       // Module 8 BaseBuilder (createBase / BaseLayout)
#include "op2/groups.hpp"     // Module 5 Groups: createMiningGroup / setupMining / takeUnit
#include "op2_mission.hpp"    // the mission-DLL contract
#include "op2_log.hpp"        // file logging
#include "op2_crash.hpp"      // SEH guards + crash logging

#include <vector>
#include <functional>

using op2::Location;
using MapID = op2::MapID;

// ---------------------------------------------------------------------------------------------------------------
// Mission DLL exports - a 2-player Colony game on the stock cm01.map (human player 0 + AI player 1).
// ---------------------------------------------------------------------------------------------------------------
extern "C" __declspec(dllexport) char      LevelDesc[]    = "TitanAPI AI Mining";
extern "C" __declspec(dllexport) char      MapName[]      = "cm01.map";
extern "C" __declspec(dllexport) char      TechtreeName[] = "MULTITEK.TXT";
extern "C" __declspec(dllexport) op2::mission::ModDesc   DescBlock   = { op2::mission::MissionType::Colony, 2, 12, 0 };
extern "C" __declspec(dllexport) op2::mission::ModDescEx DescBlockEx = { 0, 0, 0, 0, 0, 0, 0, 0 };

// ---------------------------------------------------------------------------------------------------------------
// A tiny mark-based scheduler (the same helper Cold Front uses).
// ---------------------------------------------------------------------------------------------------------------
namespace {
struct Scheduler {
  struct Every { int interval; int last; std::function<void()> fn; };
  std::vector<std::pair<int, std::function<void()>>>                   atMarks;   // fire once at/after mark
  std::vector<Every>                                                  everies;   // fire once per `interval` marks
  std::vector<std::pair<std::function<bool()>, std::function<void()>>> whens;     // fire once when cond() is true
  void atMark(int m, std::function<void()> fn)                 { atMarks.push_back({ m, std::move(fn) }); }
  void everyMarks(int n, std::function<void()> fn)             { everies.push_back({ n, -1, std::move(fn) }); }
  void when(std::function<bool()> c, std::function<void()> fn) { whens.push_back({ std::move(c), std::move(fn) }); }
  void tick(int mark) {
    for (std::size_t i = 0; i < atMarks.size();)
      if (mark >= atMarks[i].first) { atMarks[i].second(); atMarks.erase(atMarks.begin() + i); } else ++i;
    for (Every& e : everies)
      if (mark != e.last && (mark % e.interval) == 0) { e.last = mark; e.fn(); }
    for (std::size_t i = 0; i < whens.size();)
      if (whens[i].first()) { whens[i].second(); whens.erase(whens.begin() + i); } else ++i;
  }
};
Scheduler g_sched;

op2::Player g_you{ 0 }, g_ai{ 1 };
op2::Unit   g_aiSurveyor, g_aiMiner, g_aiSmelter, g_aiBeacon, g_beacon2;   // grabbed after createBase / createMine
op2::Group  g_mining;                                          // the AI's MiningGroup
op2::Group  g_buildGroup;                                      // the AI's BuildingGroup (builds the 2nd mine)
bool        g_mgSetUp = false;

const Location kBeacon { 44, 88 };                             // mine 1 - deployed by the Robo-Miner
const Location kBeacon2{ 54, 88 };                            // mine 2 - built by the BuildingGroup (recordMine)
const Location kIdleTL{ 45, 86 }, kIdleBR{ 49, 90 };          // the MiningGroup idle rect (trucks stage/spawn here)

// First live unit of `type` owned by `player` (or a null Unit).
op2::Unit firstOf(MapID type, int player) {
  for (op2::Unit u : op2::Game::unitsOfType(type)) if (u.ownerId() == player && u.isLive()) return u;
  return op2::Unit{};
}
bool nearTile(Location a, Location b, int d) { return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) <= d * d; }

// The AI's colony: CC + power + food + smelter, plus its vehicles - a Robo-Surveyor + Robo-Miner for mine 1, a
// ConVec + a second Robo-Miner for the BuildingGroup that builds mine 2, and three Cargo Trucks for the haul.
// (The ore store is kept drained by the consumption sim, so no extra storage is needed - the trucks always have
// headroom to deliver into.)
op2::BaseLayout aiColony() {
  op2::BaseLayout b;
  b.buildings = {
    { { 48, 80 }, MapID::CommandCenter }, { { 44, 82 }, MapID::Tokamak }, { { 52, 82 }, MapID::Agridome },
    { { 48, 85 }, MapID::CommonOreSmelter },
  };
  b.vehicles = {
    { { 50, 84 }, MapID::RoboSurveyor }, { { 51, 84 }, MapID::RoboMiner },
    { { 53, 84 }, MapID::ConVec }, { { 55, 86 }, MapID::RoboMiner },   // builders for the BuildingGroup (mine 2)
    // The haul trucks, placed at load INSIDE the mining idle rect; the mining group takes them once the mine stands.
    { { 46, 89 }, MapID::CargoTruck }, { { 47, 89 }, MapID::CargoTruck }, { { 48, 89 }, MapID::CargoTruck },
  };
  b.tubes = {
    { { 45, 82 }, { 51, 82 } },   // row: Tokamak - CC - Agridome
    { { 48, 82 }, { 48, 84 } },   // trunk down to the smelter
  };
  return b;
}
// A token human outpost to watch from (no lose condition in this sample - it just needs to exist).
op2::BaseLayout humanColony() {
  op2::BaseLayout b;
  b.buildings = { { { 70, 70 }, MapID::CommandCenter }, { { 73, 70 }, MapID::Tokamak }, { { 70, 73 }, MapID::Agridome } };
  b.vehicles  = { { { 72, 74 }, MapID::Scout } };
  return b;
}
} // namespace

// ---------------------------------------------------------------------------------------------------------------
static void initProcImpl() {
  op2::log::line("AI Mining (TitanAPI): InitProc");

  // ---- players: human Eden (spectator) + Plymouth AI (builds + works both mines) ----
  // Tiny human population + lots of food so the spectator outpost does not slowly starve (it is just there to
  // make this a 2-player game; it has no mine and is not the point of the demo).
  g_you.goEden().goHuman().setTechLevel(12).setCommonOre(3000).setFood(20000).setWorkers(5).setScientists(0).setKids(0);
  // Modest starting ore; the consumption sim below keeps it drained so the mining group always has demand.
  g_ai.goPlymouth().goAI().setTechLevel(12).setCommonOre(2000).setFood(8000).setWorkers(30).setScientists(15).setKids(15);

  op2::ignore(op2::Game::forceMoraleGood());
  op2::ignore(op2::Game::setDaylightEverywhere(true));
  op2::Game::addMessage("AI Mining demo: the Plymouth AI builds two ore mines (a Robo-Miner one + a BuildingGroup one) and hauls with a MiningGroup.");

  // ---- stamp both colonies ----
  const op2::BaseResult h = op2::createBase(g_you, humanColony());
  const op2::BaseResult a = op2::createBase(g_ai,  aiColony());
  op2::log::linef("  placed human=%d units, AI=%d units", h.placedUnits(), a.placedUnits());
  g_aiSmelter  = firstOf(MapID::CommonOreSmelter, 1);
  g_aiSurveyor = firstOf(MapID::RoboSurveyor, 1);
  g_aiMiner    = firstOf(MapID::RoboMiner, 1);

  // ---- the AI's ore deposit (UNSURVEYED) ----
  if (auto bk = op2::Game::createMine(kBeacon, op2::abi::MineType::CommonOre)) {
    g_aiBeacon = *bk;
    op2::log::linef("  beacon (%d,%d): surveyed(AI)=%d (expect 0)", kBeacon.x, kBeacon.y, g_aiBeacon.isSurveyed(1) ? 1 : 0);
  } else {
    op2::log::line("  ! beacon createMine FAILED");
  }

  // =============================================================================================================
  // MINE 2 - built by a BUILDING GROUP via recordMine (the "build a mine through a building group" pattern).
  // A BuildingGroup constructs structures; handed a builder + a build rect + a recorded mine, it builds the mine
  // on its own. (A BuildingGroup does NOT haul - that is the MiningGroup's job, on mine 1 below.)
  // =============================================================================================================
  if (auto bk2 = op2::Game::createMine(kBeacon2, op2::abi::MineType::CommonOre)) {
    g_beacon2 = *bk2; op2::ignore(g_beacon2.survey(1));   // surveyed for the AI so the group may build on it
    op2::log::linef("  beacon2 (%d,%d) surveyed for the BuildingGroup", kBeacon2.x, kBeacon2.y);
  }
  g_buildGroup = op2::createBuildingGroup(g_ai);
  for (op2::Unit cv : op2::Game::unitsOfType(MapID::ConVec))
    if (cv.ownerId() == 1 && cv.isLive()) g_buildGroup.takeUnit(cv);   // hand the group its builders
  for (op2::Unit rm : op2::Game::unitsOfType(MapID::RoboMiner))        // ...incl. a Robo-Miner (mines need one)
    if (rm.ownerId() == 1 && rm.isLive() && rm.id() != g_aiMiner.id()) g_buildGroup.takeUnit(rm);
  g_buildGroup.setBuildRect({ 50, 82 }, { 60, 92 });
  g_buildGroup.recordMine(kBeacon2);                                   // the group builds a CommonOreMine here
  op2::log::line("  BuildingGroup recorded a mine at the 2nd deposit (54,88)");
  g_sched.when([] {                                                    // announce mine 2 when the group finishes it
    for (op2::Unit m : op2::Game::unitsOfType(MapID::CommonOreMine))
      if (m.ownerId() == 1 && !m.underConstruction() && nearTile(m.location(), kBeacon2, 2)) return true;
    return false;
  }, [] {
    op2::Game::addMessage("The BuildingGroup finished the second mine.");
    op2::log::line("  >>> BuildingGroup BUILT mine 2 (54,88) <<<");
  });

  // =============================================================================================================
  // MINE 1 - the Robo-Miner way: survey the deposit, then deploy a mine on it.
  // =============================================================================================================
  // step 1: AI Robo-Surveyor drives to the deposit at (44,88)
  g_sched.atMark(1, [] {
    if (g_aiSurveyor.valid()) { op2::ignore(g_aiSurveyor.move(kBeacon)); op2::log::line("  AI surveyor -> beacon (44,88)"); }
  });
  // ---- step 1b: survey only once the surveyor has actually REACHED the deposit (mark-30 safety net) ----
  g_sched.when([] {
    return g_aiBeacon.valid() && !g_aiBeacon.isSurveyed(1)
        && ((g_aiSurveyor.valid() && nearTile(g_aiSurveyor.location(), kBeacon, 2)) || op2::Game::mark() >= 30);
  }, [] {
    op2::ignore(g_aiBeacon.survey(1));
    op2::Game::addMessage("AI surveyed the deposit - deploying a mine.");
    op2::log::line("  >>> AI surveyor reached (44,88); SURVEYED <<<");
  });
  // ---- step 2: AI Robo-Miner deploys the mine onto the surveyed beacon ----
  g_sched.when([] { return g_aiBeacon.valid() && g_aiBeacon.isSurveyed(1); }, [] {
    op2::log::line("  AI robo-miner deploying mine");
    if (g_aiMiner.valid()) op2::ignore(g_aiMiner.deploy(kBeacon));
  });

  // =============================================================================================================
  // HAUL MINE 1 - a MiningGroup: once mine 1 stands, set up the group and take the load-placed trucks into it.
  // =============================================================================================================
  g_sched.when([] {
    for (op2::Unit m : op2::Game::unitsOfType(MapID::CommonOreMine))
      if (m.ownerId() == 1 && !m.underConstruction()) return true;
    return false;
  }, [] {
    op2::log::line("  >>> AI mine BUILT <<<");
    // Settle a few marks - the fresh mine's truck dock waypoint isn't ready the instant it reports built.
    g_sched.atMark(op2::Game::mark() + 3, [] {
      op2::Unit mine = firstOf(MapID::CommonOreMine, 1);
      if (!mine.valid() || !g_aiSmelter.valid()) return;
      g_mining = op2::createMiningGroup(g_ai);
      g_mining.setupMining(mine, g_aiSmelter, kIdleTL, kIdleBR);     // Setup FIRST, then take the load-placed trucks
      int trucks = 0;
      for (op2::Unit t : op2::Game::unitsOfType(MapID::CargoTruck))
        if (t.ownerId() == 1 && t.isLive()) { g_mining.takeUnit(t); ++trucks; }
      g_mgSetUp = true;
      op2::Game::addMessage("AI mining group online - hauling ore.");
      op2::log::linef("  mining group %d: mine=%d -> smelter=%d (%d trucks)", g_mining.id(), mine.id(), g_aiSmelter.id(), trucks);
    });
  });

  // ---- simulate the colony CONSUMING refined ore so the mining group always has demand. A MiningGroup parks its
  //      trucks in the idle rect once the ore store is FULL; a real AI spends ore on construction/production, so
  //      here we drain it to keep headroom and the trucks hauling indefinitely. ----
  g_sched.everyMarks(3, [] {
    const int ore = g_ai.commonOre();
    if (g_mgSetUp && ore > 4000) g_ai.setCommonOre(ore - 2500);
  });

  // ---- progress log: every 20 marks, the AI's ore + live mines + active trucks. Under the consumption sim the
  //      ore stays low and trucks hold at 3 (mines reaches 2) - i.e. both mines stand and the haul runs steadily.
  for (int m = 20; m <= 200; m += 20)
    g_sched.atMark(m, [m] {
      int trucks = 0, mines = 0;
      for (op2::Unit u : op2::Game::unitsOfType(MapID::CargoTruck))    if (u.ownerId() == 1 && u.isLive()) ++trucks;
      for (op2::Unit u : op2::Game::unitsOfType(MapID::CommonOreMine)) if (u.ownerId() == 1 && u.isLive()) ++mines;
      op2::log::linef("  [mark %d] AI ore=%d mines=%d trucks=%d", m, g_ai.commonOre(), mines, trucks);
    });

  op2::log::line("AI Mining: setup complete");
}

// ---------------------------------------------------------------------------------------------------------------
static void aiProcImpl() {
  static bool first = true;
  if (first) { first = false; op2::log::linef("AIProc: first call (tick %d)", op2::Game::tick()); }
  g_sched.tick(op2::Game::mark());
}

// ---- exported entry points (SEH-guarded so a fault is logged, not silent) ----
extern "C" __declspec(dllexport) int  InitProc() { op2::crash::guard("InitProc", &initProcImpl); return 1; }
extern "C" __declspec(dllexport) void AIProc()   { op2::crash::guard("AIProc",   &aiProcImpl); }
extern "C" __declspec(dllexport) void GetSaveRegions(op2::mission::SaveRegion* p) { if (p) { p->pData = nullptr; p->size = 0; } }

extern "C" int __stdcall DllMain(void*, unsigned long reason, void*) {
  if (reason == 1 /*DLL_PROCESS_ATTACH*/) {
    op2::crash::installHandler();
    op2::log::setTickSource([] { return op2::Game::tick(); });
    op2::log::line("cMining.dll attached");
  }
  return 1;
}
