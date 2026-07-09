// Mining/mission.cpp - "AI Mining", a TitanAPI sample of a self-managing AI ore operation in ONE call.
//
// A human spectator watches a Plymouth AI run a complete mining operation set up with a single function:
//
//     op2::createMiningOperation(ai, smelterLoc, mineLoc, idleRect, numTrucks);
//
// That one call places the ore mine, the smelter, a MiningGroup hauling between them, and the Cargo Trucks. With
// auto-heal on (the default), the operation adopts the AI base's Structure Factory + Vehicle Factory + ConVec and
// hands rebuild/restock to the ENGINE's own AI build system: if the mine or smelter is destroyed, the ConVec pulls
// a kit from the Structure Factory and rebuilds it; if a truck is lost, the Vehicle Factory produces a replacement.
// Nothing is ever spawned into existence mid-game. A single op2::tickMiningOperations() in AIProc keeps the haul
// bound to whatever the engine rebuilds.
//
// To show that off, this sample stages a little "raid" - it destroys a truck, then the smelter - and you watch the
// AI's factories rebuild the operation the legitimate way. (See op2/mining.hpp for how it works.)

#include "op2.hpp"            // the TitanAPI Layer 2 facade
#include "op2/base.hpp"       // Module 8 BaseBuilder (createBase / BaseLayout)
#include "op2/mining.hpp"     // createMiningOperation / tickMiningOperations
#include "op2_mission.hpp"    // the mission-DLL contract
#include "op2_log.hpp"        // file logging
#include "op2_crash.hpp"      // SEH guards + crash logging

#include <vector>
#include <functional>

using op2::Location;
using MapID = op2::MapID;

// ---------------------------------------------------------------------------------------------------------------
// Mission DLL exports - a 2-player Colony game on the stock cm01.map (human spectator + a self-mining AI).
// ---------------------------------------------------------------------------------------------------------------
extern "C" __declspec(dllexport) char      LevelDesc[]    = "TitanAPI AI Mining";
extern "C" __declspec(dllexport) char      MapName[]      = "cm01.map";
extern "C" __declspec(dllexport) char      TechtreeName[] = "MULTITEK.TXT";
extern "C" __declspec(dllexport) op2::mission::ModDesc   DescBlock   = { op2::mission::MissionType::Colony, 2, 12, 0 };
extern "C" __declspec(dllexport) op2::mission::ModDescEx DescBlockEx = { 0, 0, 0, 0, 0, 0, 0, 0 };

// ---------------------------------------------------------------------------------------------------------------
// A tiny mark-based scheduler (the same helper Cold Front uses) - drives the demo "raid" + progress log.
// ---------------------------------------------------------------------------------------------------------------
namespace {
struct Scheduler {
  std::vector<std::pair<int, std::function<void()>>> atMarks;
  void atMark(int m, std::function<void()> fn) { atMarks.push_back({ m, std::move(fn) }); }
  void tick(int mark) {
    for (std::size_t i = 0; i < atMarks.size();)
      if (mark >= atMarks[i].first) { atMarks[i].second(); atMarks.erase(atMarks.begin() + i); } else ++i;
  }
};
Scheduler g_sched;

op2::Player g_you{ 0 }, g_ai{ 1 };

// The operation's tiles. The smelter sits just south of the AI base (powered + reachable by the ConVec); the mine
// is on a beacon further south; trucks stage/haul in the idle rect between them.
const Location kSmelter{ 48, 85 };
const Location kMine{ 44, 88 };
const op2::Region kIdle{ { 45, 86 }, { 49, 90 } };   // trucks stage + haul inside this rect

// The AI's colony CORE - createMiningOperation adds the mine/smelter/trucks, but auto-heal needs the AI to OWN a
// powered Structure Factory + Vehicle Factory + ConVec, so we give it a real (tube-powered) base with those.
op2::BaseLayout aiColony() {
  op2::BaseLayout b;
  b.buildings = {
    { { 48, 80 }, MapID::CommandCenter },
    { { 44, 80 }, MapID::Tokamak }, { { 52, 80 }, MapID::Tokamak },   // two Tokamaks: power for CC + both factories
    { { 56, 80 }, MapID::Agridome },
    { { 39, 82 }, MapID::StructureFactory },                          // the ConVec pulls smelter kits from here
    { { 39, 86 }, MapID::VehicleFactory },                            // restocks cargo trucks
  };
  b.vehicles = { { { 44, 84 }, MapID::ConVec } };                     // the builder the BuildingGroup uses
  b.tubes = {
    { { 42, 81 }, { 55, 81 } },   // spine under the CC row (Tokamak - CC - Tokamak - Agridome)
    { { 42, 81 }, { 42, 87 } },   // west trunk down to the two factories
    { { 48, 82 }, { 48, 84 } },   // trunk down to the smelter tile (48,85)
  };
  return b;
}
op2::BaseLayout humanColony() {   // a token spectator outpost: CC + Tokamak (in CC range) + a Scout. High food /
  op2::BaseLayout b;              // tiny population (set below) keep it stable without needing farms.
  b.buildings = { { { 70, 70 }, MapID::CommandCenter }, { { 72, 72 }, MapID::Tokamak } };
  b.vehicles  = { { { 74, 74 }, MapID::Scout } };
  return b;
}

// Count live (and, for buildings, powered) AI-owned units of a type - used by the progress log.
int countLive(MapID t, bool poweredOnly = false) {
  int n = 0;
  for (op2::Unit u : op2::Game::unitsOfType(t))
    if (u.ownerId() == 1 && u.isLive() && (!poweredOnly || !u.isDisabled())) ++n;
  return n;
}
} // namespace

// ---------------------------------------------------------------------------------------------------------------
static void initProcImpl() {
  op2::log::line("AI Mining (TitanAPI): InitProc");

  // ---- players: human Eden spectator + Plymouth AI ----
  g_you.goEden().goHuman().setTechLevel(12).setCommonOre(3000).setFood(50000).setWorkers(3).setScientists(0).setKids(0);
  g_ai.goPlymouth().goAI().setTechLevel(12).setCommonOre(5000).setFood(8000).setWorkers(30).setScientists(15).setKids(15);
  op2::ignore(op2::Game::forceMoraleGood());
  op2::ignore(op2::Game::setDaylightEverywhere(true));
  op2::Game::addMessage("AI Mining: one createMiningOperation() call runs the whole mine - the AI's factories heal it if attacked.");

  // ---- stamp the colony cores ----
  op2::ignore(op2::createBase(g_ai,  aiColony()));
  op2::ignore(op2::createBase(g_you, humanColony()));

  // ---- THE ONE CALL: a complete, self-managing mining operation for the AI ----
  op2::createMiningOperation(g_ai, kSmelter, kMine, kIdle, /*numTrucks*/ 3, /*autoHeal*/ true);
  op2::log::line("  createMiningOperation: mine(44,88) -> smelter(48,85), 3 trucks, auto-heal via AI factories ON");

  // ---- DEMO "raid": destroy a truck, then the smelter, so you can watch the AI's factories rebuild them ----
  g_sched.atMark(50, [] {
    for (op2::Unit t : op2::Game::unitsOfType(MapID::CargoTruck))
      if (t.ownerId() == 1 && t.isLive()) { t.kill(); op2::Game::addMessage("Raid: a Cargo Truck was destroyed! (VF should restock it)"); break; }
  });
  g_sched.atMark(110, [] {
    for (op2::Unit s : op2::Game::unitsOfType(MapID::CommonOreSmelter))
      if (s.ownerId() == 1 && s.isLive()) { s.kill(); op2::Game::addMessage("Raid: the smelter was destroyed! (a ConVec should rebuild it)"); break; }
  });

  // ---- progress log: the AI should keep a powered SF/VF, and heal mine=1 smelter=1 trucks=3 through the raids ----
  for (int m = 10; m <= 260; m += 10)
    g_sched.atMark(m, [m] {
      op2::log::linef("  [mark %d] mine=%d smelter=%d trucks=%d | SF(pwr)=%d VF(pwr)=%d convec=%d | ore=%d",
                      m, countLive(MapID::CommonOreMine), countLive(MapID::CommonOreSmelter), countLive(MapID::CargoTruck),
                      countLive(MapID::StructureFactory, true), countLive(MapID::VehicleFactory, true),
                      countLive(MapID::ConVec), g_ai.commonOre());
    });

  op2::log::line("AI Mining: setup complete");
}

// ---------------------------------------------------------------------------------------------------------------
static void aiProcImpl() {
  static bool first = true;
  if (first) { first = false; op2::log::linef("AIProc: first call (tick %d)", op2::Game::tick()); }
  op2::tickMiningOperations();       // <-- keeps the haul bound to whatever the engine rebuilds (never spawns)
  // TEST: top up ore every frame so the engine-native ScGroup rebuild/restock can NEVER be starved (removes the
  // economy confound - we are testing whether recordBuilding/recordVehReinforceGroup rebuild the smelter + trucks).
  g_ai.setCommonOre(20000);
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
