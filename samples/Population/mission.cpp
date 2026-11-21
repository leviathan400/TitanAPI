// mission.cpp - TitanAPI POPULATION GOAL demo.
//
// A simple colony-growth mission: grow your Eden colony to 150 colonists to win. Shows a custom victory
// condition driven by a population trigger (CreateResourceTrigger on the "Colonists" resource) - a single
// objective that turns green and wins the moment the colony reaches the target.
//
// The colony is pre-built for growth: food (Agridomes), housing (Residences), a Nursery (births), a University
// (children grow into workers), a Medical Center (fewer deaths), and a Recreation Facility (morale). Morale is
// forced good so the colony actually grows. Tune the goal with kPopulationGoal (lower it to test the win fast).
//
// Build: cmake -S . -B build -G "Visual Studio 18 2026" -A Win32 && cmake --build build --config Release
// Output: cPopulation.dll  (logs to OPU\logs\cPopulation.log)

#include "op2.hpp"            // the op2:: facade (Game / Player / Unit / orders / BaseBuilder)
#include "op2/trigger.hpp"    // triggers + victory/defeat (include in exactly ONE .cpp)
#include "op2/base.hpp"       // declarative BaseLayout / createBase
#include "op2_mission.hpp"    // the mission-DLL contract (DescBlock / SaveRegion types)
#include "op2_log.hpp"        // file logging -> OPU\logs\cPopulation.log
#include "op2_crash.hpp"      // SEH guards + crash logging

using namespace op2;
using M = abi::MapID;

// ============================================================================================================
// Mission metadata. cm01.map + MULTITEK.TXT is a known-good colony combo. 2 players: player 0 is the colony,
// player 1 is a token Plymouth presence the map expects.
// ============================================================================================================
extern "C" __declspec(dllexport) char LevelDesc[]    = "TitanAPI Population Goal";
extern "C" __declspec(dllexport) char MapName[]      = "cm01.map";
extern "C" __declspec(dllexport) char TechtreeName[] = "MULTITEK.TXT";
extern "C" __declspec(dllexport) mission::ModDesc   DescBlock   = { mission::MissionType::Colony, 2, 12, 0 };
extern "C" __declspec(dllexport) mission::ModDescEx DescBlockEx = { 0, 0, 0, 0, 0, 0, 0, 0 };

namespace {

// The win target. Lower it (e.g. to a little above the starting population) to verify the win quickly.
constexpr int kPopulationGoal = 150;

Trigger g_victory;   // the population victory condition (kept for the progress log)

// One victory condition: fire when player 0's colonist count reaches the goal. CreateResourceTrigger
// (Resource::Colonists) is the engine's own population trigger, so this is a real, pending->green objective - no
// per-frame polling needed. Count trigger oneShot=TRUE; victory condition oneShot=FALSE (a one-shot victory
// condition is consumed when it fires and would not stay green - see samples/Starship).
void initVictoryConditions() {
  Trigger cond = onResource(Resource::Colonists, Compare::GreaterEqual, kPopulationGoal, {}, /*player=*/0,
                            /*oneShot=*/true);
  static char text[80];
  std::snprintf(text, sizeof(text), "Grow the colony to %d colonists", kPopulationGoal);
  g_victory = victoryWhen(cond, text, /*oneShot=*/false);
}

// Current colonist count (workers + scientists + children) - what the engine's "Colonists" resource counts.
int population(Player p) { return p.workers() + p.scientists() + p.kids(); }

} // namespace

// ============================================================================================================
// InitProc - build a growth colony once.
// ============================================================================================================
static void initProc() {
  log::stamp("Population Goal - mission START");
  log::line("Population Goal: InitProc");

  // Player 0: an Eden colony set up to grow. Full tech (everything available), a healthy starting population,
  // and a big food + metals stockpile so growth is never starved.
  Player you = Game::player(0);
  you.goEden();
  you.setTechLevel(12);
  you.setWorkers(60).setScientists(25).setKids(15);          // 100 colonists to start - grow to the 150 goal
  you.setCommonOre(20000, 30000).setFood(50000, 60000);

  // The growth colony: power, food (Agridomes), housing (Residences), and the people-growth buildings - a
  // Nursery (children are born), a University (children become workers/scientists), a Medical Center (fewer
  // deaths), and a Recreation Facility (morale). Connected by tubes so everything is active.
  BaseLayout base;
  base.buildings = {
    { {40, 80}, M::CommandCenter },
    { {46, 80}, M::Agridome }, { {51, 80}, M::Agridome }, { {56, 80}, M::Agridome },
    { {40, 85}, M::Tokamak }, { {45, 85}, M::Residence }, { {50, 85}, M::Residence },
    { {55, 85}, M::Residence }, { {60, 85}, M::Tokamak },
    { {40, 90}, M::Nursery }, { {45, 90}, M::University }, { {50, 90}, M::MedicalCenter },
    { {55, 90}, M::RecreationFacility }, { {60, 90}, M::Tokamak },
    { {40, 95}, M::Residence }, { {45, 95}, M::Residence }, { {50, 95}, M::Residence },
    { {55, 95}, M::Residence }, { {60, 95}, M::Residence },   // extra housing so there is room for colonist #150
  };
  base.tubes = {
    { {44, 80}, {46, 80} },    // CC -> Agridome row
    { {41, 81}, {41, 85} },    // CC column down to the housing row
    { {41, 85}, {60, 85} },    // housing/power row
    { {41, 85}, {41, 90} },    // down to the growth-building row
    { {41, 90}, {60, 90} },    // Nursery / University / Medical / Rec / Tokamak row
    { {41, 90}, {41, 95} },    // down to the lower residences
    { {41, 95}, {60, 95} },    // lower residence row (extended to the extra residences)
  };
  const BaseResult r = createBase(you, base, { 0, 10 });     // shift the whole colony 10 tiles south
  log::linef("  colony placed: %d buildings, %d vehicles, %d tube runs", r.buildings, r.vehicles, r.tubeLines);

  you.centerViewOn({ 50, 95 });

  // Player 1: a token Plymouth presence (the 2-player colony map expects a second player). Not driven.
  Player other = Game::player(1);
  other.goPlymouth().goAI();
  ignore(Game::createUnit(M::CommandCenter, { 70, 70 }, other));

  // Lock morale at the top level (steady - it stops fluctuating), so the colony grows fast and overcrowding /
  // a missing rec facility can't drag morale down. With morale pinned, housing/residences barely matter.
  ignore(Game::forceMoraleGreat());

  // The objective: reach the population goal.
  initVictoryConditions();
  loseIfNoCommandCenter(0);

  Game::addMessage("Grow your colony to 150 colonists.");
  log::linef("Population Goal: InitProc done - starting colonists=%d, goal=%d", population(you), kPopulationGoal);
}

// ============================================================================================================
// AIProc - no per-frame logic; the population victory trigger does the work. We only log progress: the current
// colonist count whenever it changes, so the log tracks the climb toward the goal (and the win shows green).
// ============================================================================================================
static void aiProc() {
  const Player you = Game::player(0);
  static int prev = -1;
  const int pop = population(you);
  if (pop != prev) {
    prev = pop;
    log::linef("  [pop t%d] colonists=%d / %d  (victory fired=%d)",
               Game::tick(), pop, kPopulationGoal, int(g_victory.hasFired(0)));
  }
}

// ---- guarded engine entry points (a fault is logged, not silent) ----
extern "C" __declspec(dllexport) int  InitProc() { crash::guard("InitProc", &initProc); return 1; }
extern "C" __declspec(dllexport) void AIProc()   { crash::guard("AIProc",   &aiProc); }
extern "C" __declspec(dllexport) void GetSaveRegions(mission::SaveRegion* p) { if (p) { p->pData = nullptr; p->size = 0; } }

extern "C" int __stdcall DllMain(void*, unsigned long reason, void*) {
  if (reason == 1 /* DLL_PROCESS_ATTACH */) {
    crash::installHandler();
    log::setTickSource([] { return Game::tick(); });
  } else if (reason == 0 /* DLL_PROCESS_DETACH */) {
    log::stamp("Population Goal - mission END");
  }
  return 1;
}
