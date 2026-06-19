// ColdFront/mission.cpp - "Cold Front", a TitanAPI sample mission (a port of OP2Lua's reference mission).
//
// You build an Eden Colony on on1_01.map beside a fully-scripted Plymouth AI, while a "dormant" volcano wakes
// and floods the lowlands with lava. Survive the eruption and EVACUATE to the starship to win; lose if your
// Command Center falls. A roaming green-Plymouth raiding party crosses the map twice.
//
// This sample dogfoods the whole Layer 2 facade in one real mission:
//   • the declarative **BaseBuilder** (op2::BaseLayout / createBase) for both colonies' starting layout,
//   • a tiny mark-based **scheduler** (atMark / everyMarks / when) built on the AIProc loop - the TitanAPI
//     equivalent of OP2Lua's event helpers,
//   • mining orders (survey → deploy → mining route), an **ScGroup patrol** (Module 5 setPatrolMode),
//   • the **lava disaster** (mark cells lava-possible, then erupt + accelerate the flow),
//   • **starship victory** (satellite counts) and a **lose-if-no-CC** failure, plus messages & steady morale.

#include "op2.hpp"            // the TitanAPI Layer 2 facade
#include "op2/base.hpp"       // Module 8 BaseBuilder (also pulled in by op2.hpp, listed for clarity)
#include "op2/trigger.hpp"    // Module 4 triggers + op2::win/lose (defines exported stubs - include in ONE TU)
#include "op2_mission.hpp"    // the mission-DLL contract
#include "op2_log.hpp"        // file logging
#include "op2_crash.hpp"      // SEH guards + crash logging

#include <vector>
#include <functional>

using op2::Location;
using MapID = op2::MapID;

// ---------------------------------------------------------------------------------------------------------------
// Mission DLL exports - Cold Front: a 3-player Colony game on on1_01.map.
// ---------------------------------------------------------------------------------------------------------------
extern "C" __declspec(dllexport) char      LevelDesc[]    = "Cold Front (TitanAPI)";
extern "C" __declspec(dllexport) char      MapName[]      = "on1_01.map";
extern "C" __declspec(dllexport) char      TechtreeName[] = "MULTITEK.TXT";
extern "C" __declspec(dllexport) op2::mission::ModDesc   DescBlock   = { op2::mission::MissionType::Colony, 3, 12, 0 };
extern "C" __declspec(dllexport) op2::mission::ModDescEx DescBlockEx = { 0, 0, 0, 0, 0, 0, 0, 0 };

// ---------------------------------------------------------------------------------------------------------------
// A tiny mark-based scheduler - the TitanAPI counterpart of OP2Lua's at_mark / every / when. AIProc ticks it
// each frame with the current game mark (100 ticks/mark). One-shot atMark/when entries retire after firing.
// ---------------------------------------------------------------------------------------------------------------
namespace {
struct Scheduler {
  struct Every { int interval; int last; std::function<void()> fn; };
  std::vector<std::pair<int, std::function<void()>>>          atMarks;   // fire once at/after mark
  std::vector<Every>                                         everies;   // fire once per `interval` marks
  std::vector<std::pair<std::function<bool()>, std::function<void()>>> whens;  // fire once when cond() is true

  void atMark(int m, std::function<void()> fn)        { atMarks.push_back({ m, std::move(fn) }); }
  void everyMarks(int n, std::function<void()> fn)    { everies.push_back({ n, -1, std::move(fn) }); }
  void when(std::function<bool()> c, std::function<void()> fn) { whens.push_back({ std::move(c), std::move(fn) }); }

  void tick(int mark) {
    for (std::size_t i = 0; i < atMarks.size();)                          // one-shot at_mark
      if (mark >= atMarks[i].first) { atMarks[i].second(); atMarks.erase(atMarks.begin() + i); } else ++i;
    for (Every& e : everies)                                             // every N marks
      if (mark != e.last && (mark % e.interval) == 0) { e.last = mark; e.fn(); }
    for (std::size_t i = 0; i < whens.size();)                           // one-shot when(cond)
      if (whens[i].first()) { whens[i].second(); whens.erase(whens.begin() + i); } else ++i;
  }
};
Scheduler g_sched;

// Mission-wide handles.
op2::Player  g_eden{ 0 }, g_plymouth{ 1 }, g_green{ 2 };
op2::Group   g_patrol;                 // the AI's RPG-Lynx fighting patrol (Module 5 FightGroup)
bool         g_won = false, g_lost = false;

// AI structure-build pipeline state (one ConVec builds one structure at a time, advancing through a plan).
struct BuildJob { MapID type; Location at; };
op2::Unit    g_builder;                // the ConVec currently on a build job (invalid = free for the next job)
Location     g_buildAnchor{};
MapID        g_buildType = MapID::None;
int          g_buildIdx  = 0;
bool nearTile(Location a, Location b, int d) { return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) <= d * d; }

// Nearest "Common" mining beacon to a tile (for the AI surveyor/miner to target).
op2::Unit nearestCommonBeacon(Location to) {
  op2::Unit best; long bestd = 0;
  for (op2::Unit b : op2::Game::beacons(/*Common*/ 0)) {
    const Location l = b.location();
    const long dx = l.x - to.x, dy = l.y - to.y, d = dx * dx + dy * dy;
    if (!best.valid() || d < bestd) { best = b; bestd = d; }
  }
  return best;
}
} // namespace

// ---------------------------------------------------------------------------------------------------------------
// Starting layouts - the placement data, expressed as TitanAPI BaseLayouts (faithful to ColdFront.placement.lua).
// ---------------------------------------------------------------------------------------------------------------
static op2::BaseLayout edenLayout() {
  op2::BaseLayout b;
  b.buildings = {
    { { 33,  8 }, MapID::CommandCenter }, { { 37,  8 }, MapID::Agridome },
    { { 41,  8 }, MapID::Nursery },       { { 45,  7 }, MapID::StructureFactory },
    { { 45, 17 }, MapID::CommonOreSmelter }, { { 54,  3 }, MapID::Tokamak },
  };
  b.vehicles = {
    { { 33, 12 }, MapID::Earthworker }, { { 36, 12 }, MapID::ConVec }, { { 38, 12 }, MapID::ConVec },
    { { 49, 18 }, MapID::CargoTruck },  { { 49, 20 }, MapID::CargoTruck },
    { { 42, 12 }, MapID::Scout },       { { 40, 12 }, MapID::RoboSurveyor },
  };
  b.tubes = { { { 45, 10 }, { 45, 15 } } };           // the smelter feed line
  return b;
}
static op2::BaseLayout plymouthLayout() {
  op2::BaseLayout b;
  b.buildings = {
    { { 34, 101 }, MapID::GuardPost, MapID::RPG }, { { 33, 111 }, MapID::GuardPost, MapID::RPG },
    { {  9, 106 }, MapID::CommandCenter }, { { 15, 104 }, MapID::StandardLab },
    { { 15, 113 }, MapID::Agridome },      { { 15, 116 }, MapID::Agridome },
    { { 28, 103 }, MapID::VehicleFactory },{ { 22, 103 }, MapID::StructureFactory },
    { { 15,  98 }, MapID::Residence },     { { 15,  95 }, MapID::Nursery },
    { { 15,  92 }, MapID::University },     { { 16, 121 }, MapID::CommonOreSmelter },
    { {  3,  90 }, MapID::Tokamak },        { {  3,  93 }, MapID::Tokamak },
  };
  b.vehicles = {
    { { 21, 119 }, MapID::CargoTruck }, { { 23, 119 }, MapID::CargoTruck },
    { { 36, 115 }, MapID::RoboSurveyor }, { { 33, 115 }, MapID::RoboMiner },
    { { 20, 108 }, MapID::ConVec }, { { 22, 108 }, MapID::ConVec }, { { 24, 108 }, MapID::ConVec },
    { { 43, 101 }, MapID::Lynx, MapID::RPG }, { { 43, 99 }, MapID::Lynx, MapID::RPG },
    { { 43,  97 }, MapID::Lynx, MapID::RPG }, { { 43, 95 }, MapID::Lynx, MapID::RPG },
    { { 29,  94 }, MapID::Lynx, MapID::Microwave }, { { 31, 94 }, MapID::Lynx, MapID::Microwave },
    { { 31,  96 }, MapID::Lynx, MapID::Microwave }, { { 29, 96 }, MapID::Lynx, MapID::Microwave },
    { {  9, 110 }, MapID::Earthworker },
  };
  // The Plymouth base tube network - connects the structures (ported from the placement's tube tiles, grouped
  // into the runs they form). Without these the AI buildings sit disconnected.
  b.tubes = {
    { { 12, 106 }, { 34, 106 } },   // the long south trunk (links the CC / labs / SF / VF area)
    { { 33, 106 }, { 33, 110 } },   // spur down to the guard posts
    { { 34, 103 }, { 34, 106 } },   // spur up to the Structure / Vehicle Factories
    { { 15, 100 }, { 15, 102 } },   // up to the Residence / Nursery / University column
    { { 15, 106 }, { 15, 111 } },   // down past the StandardLab
    { { 15, 118 }, { 15, 119 } },   // toward the lower Agridomes / smelter
  };
  return b;
}

// ---------------------------------------------------------------------------------------------------------------
// InitProc - set up the players, stamp both colonies, and schedule the scripted mission.
// ---------------------------------------------------------------------------------------------------------------
static void initProcImpl() {
  op2::log::line("Cold Front (TitanAPI): InitProc");

  // ---- players (Eden human + two Plymouth AIs) ----
  g_eden.goEden();     g_eden.goHuman();
  g_eden.setCommonOre(1000); g_eden.setFood(1000);
  g_eden.setWorkers(20); g_eden.setScientists(8); g_eden.setKids(12); g_eden.setTechLevel(0);
  g_eden.markResearchComplete(3401); g_eden.markResearchComplete(3304);   // Cybernetic Teleoperation, Offspring Enh.
  g_plymouth.goPlymouth(); g_plymouth.goAI();
  g_plymouth.setCommonOre(4000); g_plymouth.setFood(1000);
  g_plymouth.setWorkers(14); g_plymouth.setScientists(8); g_plymouth.setKids(10);
  g_green.goPlymouth();    g_green.goAI();   g_green.setFood(1000); g_green.setTechLevel(12);

  // DEBUG head-start for the human (matches the Lua AI_TEST_DEBUG) so you can build quickly while testing.
  g_eden.setCommonOre(10000); g_eden.setFood(10000); g_eden.setWorkers(50);

  // ---- framing ----
  op2::Game::forceMoraleGood();                          // steady morale - it's about the fight, not micro
  op2::Game::setDaylightEverywhere(true);                // full daylight (no night cycle in this sample)
  op2::Game::addMessage("Cold Front: build your colony and evacuate to the starship before the volcano consumes you!");

  // ---- stamp both colonies + the resource beacons ----
  const op2::BaseResult e = op2::createBase(g_eden, edenLayout());
  const op2::BaseResult p = op2::createBase(g_plymouth, plymouthLayout());
  op2::log::linef("  placed Eden=%d units, Plymouth=%d units", e.placedUnits(), p.placedUnits());

  // AI channel: dump the Plymouth bot's starting state (mirrors OP2Lua's AI resource dump).
  op2::log::ai("=== Cold Front: Plymouth (AI) starting resources ===");
  op2::log::ailinef("  ore=%d rare=%d food=%d  pop=%d (kids=%d workers=%d scientists=%d)",
                    g_plymouth.commonOre(), g_plymouth.rareOre(), g_plymouth.food(),
                    g_plymouth.population(), g_plymouth.kids(), g_plymouth.workers(), g_plymouth.scientists());

  // Mining beacons across the map (2-bar deposits at the two bases, ordinary deposits in the wider world).
  struct B { Location at; int rare; }; static const B beacons[] = {
    { { 22, 125 }, 0 }, { { 46, 22 }, 0 }, { { 219, 119 }, 0 }, { { 213, 63 }, 0 }, { { 244, 14 }, 1 },
    { {  99, 121 }, 1 }, { { 137, 76 }, 1 }, { { 58, 113 }, 0 }, { { 15, 39 }, 1 }, { { 240, 40 }, 1 },
  };
  for (const B& bn : beacons)
    op2::Game::createMine(bn.at, bn.rare ? op2::abi::MineType::RareOre : op2::abi::MineType::CommonOre);

  // ---- AI mining: survey + deploy the 2-bar deposit by the Plymouth base, then route the trucks ----
  g_sched.atMark(1, [] {                                  // surveyor reveals the nearest common beacon
    for (op2::Unit s : op2::Game::unitsOfType(MapID::RoboSurveyor))
      if (s.ownerId() == 1) { op2::Unit b = nearestCommonBeacon(s.location());
                              if (b.valid()) { b.survey(1); s.move(b.location());
                                op2::log::ailinef("surveyor surveying beacon at (%d,%d)", b.location().x, b.location().y); }
                              break; }
  });
  g_sched.atMark(5, [] {                                  // RoboMiner deploys it into an operational mine
    for (op2::Unit m : op2::Game::unitsOfType(MapID::RoboMiner))
      if (m.ownerId() == 1) { op2::Unit b = nearestCommonBeacon(m.location());
                              if (b.valid()) { m.deploy(b.location());
                                op2::log::ailinef("RoboMiner deploying mine at (%d,%d)", b.location().x, b.location().y); }
                              break; }
  });
  g_sched.when([] {                                       // wait until the AI's mine has finished building
    for (op2::Unit m : op2::Game::unitsOfType(MapID::CommonOreMine))
      if (m.ownerId() == 1 && !m.underConstruction()) return true;
    return false;
  }, [] {
    // CRITICAL (per the OP2Lua original): do NOT route trucks the instant the mine reports built - its truck
    // dock isn't fully established yet, and the engine's route loop reads a bad dock waypoint and crashes
    // (eip 0047000a). Settle a few marks first.
    op2::log::ai("AI mine built - routing trucks in 3 marks");
    g_sched.atMark(op2::Game::mark() + 3, [] {
      op2::Unit mine, smelter;
      for (op2::Unit m : op2::Game::unitsOfType(MapID::CommonOreMine))    if (m.ownerId() == 1) { mine = m; break; }
      for (op2::Unit s : op2::Game::unitsOfType(MapID::CommonOreSmelter)) if (s.ownerId() == 1) { smelter = s; break; }
      if (mine.valid() && smelter.valid())
        for (op2::Unit t : op2::Game::unitsOfType(MapID::CargoTruck)) if (t.ownerId() == 1) t.mine(mine, smelter);
      op2::log::ai("2-bar mine online - original trucks routed to the smelter");
    });
  });

  // ---- AI patrol: a FightGroup of the 4 RPG Lynx patrols a big loop (Module 5 setPatrolMode) ----
  g_sched.atMark(15, [] {
    g_patrol = op2::createFightGroup(g_plymouth);
    for (op2::Unit l : op2::Game::unitsOfType(MapID::Lynx))
      if (l.ownerId() == 1 && l.weapon() == MapID::RPG) g_patrol.takeUnit(l);
    g_patrol.setPatrolMode({ { 91, 125 }, { 91, 75 }, { 2, 75 } });   // a fighting patrol around the lowlands
    op2::log::ailinef("RPG-Lynx patrol formed (%d units)", g_patrol.totalUnitCount());
  });

  // ---- AI production: the Vehicle Factory turns out reinforcements over time ----
  static const MapID prod[]  = { MapID::CargoTruck, MapID::Lynx, MapID::Lynx, MapID::Scout, MapID::Lynx };
  static const MapID prodW[] = { MapID::None, MapID::Microwave, MapID::Microwave, MapID::None, MapID::RPG };
  static int prodIdx = 0;
  g_sched.everyMarks(12, [] {
    if (prodIdx >= int(sizeof(prod) / sizeof(prod[0]))) return;
    for (op2::Unit vf : op2::Game::unitsOfType(MapID::VehicleFactory))
      if (vf.ownerId() == 1) { vf.produce(prod[prodIdx], prodW[prodIdx]);
                               op2::log::ailinef("VehicleFactory producing item %d (mark %d)", prodIdx, op2::Game::mark());
                               ++prodIdx; break; }
  });

  // ---- AI base expansion: a ConVec builds new structures over time ----
  // A streamlined build pipeline: one ConVec works one job at a time - load the kit (setKit), find a clear
  // footprint near the target (findBuildSite), and build there; advance once the structure stands. (The OP2Lua
  // original runs several ConVecs in parallel off the Structure Factory's dock; this single-builder version is
  // the readable equivalent and is what makes the AI visibly grow its base.)
  static const BuildJob plan[] = {
    { MapID::CommonOreSmelter, { 16, 125 } }, { MapID::Residence,  { 12, 98 } },
    { MapID::MedicalCenter,    { 12, 95 } },  { MapID::Agridome,   { 11, 113 } },
    { MapID::Agridome,         { 11, 116 } }, { MapID::Tokamak,    {  3, 87 } },
    { MapID::AdvancedLab,      { 38, 94 } },
  };
  static const int planN = int(sizeof(plan) / sizeof(plan[0]));
  g_sched.everyMarks(4, [] {
    if (g_buildIdx >= planN) return;
    // A job is in flight - wait for the structure to stand near the anchor, then advance.
    if (g_builder.valid() && g_builder.isLive()) {
      for (op2::Unit u : op2::Game::unitsOfType(g_buildType))
        if (u.ownerId() == 1 && nearTile(u.location(), g_buildAnchor, 3)) {
          op2::log::ailinef("ConVec built structure %d (mark %d)", g_buildIdx, op2::Game::mark());
          ++g_buildIdx; g_builder = op2::Unit{}; break;
        }
      return;
    }
    // Start the next job on the first available AI ConVec: load the kit, find a site, build.
    const BuildJob& j = plan[g_buildIdx];
    for (op2::Unit cv : op2::Game::unitsOfType(MapID::ConVec)) {
      if (cv.ownerId() != 1) continue;
      cv.setKit(j.type);                                            // load the structure kit directly
      const auto site = op2::GameMap::findBuildSite(j.type, j.at);  // nearest clear footprint to the target
      if (site) { cv.build(*site); g_builder = cv; g_buildAnchor = *site; g_buildType = j.type;
                  op2::log::ailinef("ConVec building structure %d near (%d,%d)", g_buildIdx, site->x, site->y); }
      break;
    }
  });

  // ---- Green Plymouth: two scripted raiding parties cross the map and leave ----
  auto spawnRaiders = [](int tag) {
    for (int i = 0; i < 2; ++i) op2::Game::createUnit(MapID::Lynx, { 161, 127 }, g_green, MapID::EMP);
    for (int i = 0; i < 4; ++i) op2::Game::createUnit(MapID::Lynx, { 161, 127 }, g_green, MapID::Microwave);
    op2::log::ailinef("green raiders wave %d spawned at (161,127)", tag);
  };
  auto marchRaiders = [] {                                 // send every green Lynx to the top exit, then remove
    for (op2::Unit u : op2::Game::unitsOfType(MapID::Lynx)) if (u.ownerId() == 2) u.move({ 200, 1 });
  };
  g_sched.atMark(60,  [spawnRaiders] { spawnRaiders(1); });
  g_sched.atMark(61,  [marchRaiders] { marchRaiders(); });
  g_sched.atMark(500, [spawnRaiders] { spawnRaiders(2); });
  g_sched.atMark(501, [marchRaiders] { marchRaiders(); });
  g_sched.everyMarks(2, [] {                               // when a green Lynx reaches the exit, poof it
    for (op2::Unit u : op2::Game::unitsOfType(MapID::Lynx)) {
      if (u.ownerId() != 2) continue;
      const Location l = u.location();
      if ((l.x - 200) * (l.x - 200) + (l.y - 1) * (l.y - 1) <= 25) u.remove();
    }
  });

  // ---- Lava: mark the dark "lava-rock" lowlands lava-possible, then wake the volcano ----
  // Slow1 (cellType 2) is the dark lava rock; DozedArea(21)/Rubble(22)/Tube0(26) are the substrate under bases,
  // included so the flow engulfs structures instead of leaving building-shaped islands.
  const Location lavaVent{ 21, 26 };
  int lavaCells = 0;
  const int mw = op2::GameMap::getWidth(), mh = op2::GameMap::getHeight();
  for (int y = 0; y < mh; ++y)
    for (int x = 0; x < mw; ++x) {
      const int ct = int(op2::GameMap::getCellType({ x, y }));
      if (ct == 2 || ct == 21 || ct == 22 || ct == 26) { op2::GameMap::setLavaPossible({ x, y }, true); ++lavaCells; }
    }
  op2::log::linef("  lava: marked %d lava-possible cells; vent (%d,%d)", lavaCells, lavaVent.x, lavaVent.y);

  g_sched.atMark(240, [] { op2::Game::addMessage("Our scientists have discovered the volcano is NOT dormant!"); });
  g_sched.atMark(250, [lavaVent] {                         // WARN - a slow ominous glow builds at the vent
    op2::Game::addMessage("WARNING: the old volcano is stirring - relocate your colony!");
    op2::Game::createEruption(lavaVent, /*VerySlow*/ 15, /*now=*/ true);
  });
  g_sched.atMark(260, [lavaVent] {                         // ERUPT - accelerate the flow into a violent flood
    op2::Game::setLavaSpeed(45);                            // "Slow" on OP2's named spread scale
    op2::Game::createEruption(lavaVent, 45, /*now=*/ true);
    op2::Game::addMessage("The volcano ERUPTS! Lava floods the lowlands!");
  });
  g_sched.atMark(275, [] { op2::Game::addMessage("Re-establish your Command Center before your last one falls!"); });

  // ---- Victory / defeat ----
  // STARSHIP: evacuate 10000 Rare + 10000 Common + 10000 Food + 200 Colonists (counted by launched cargo).
  op2::Game::addMessage("STARSHIP: evacuate 10000 Rare, 10000 Common, 10000 Food and 200 Colonists to escape Cold Front!");
  g_sched.everyMarks(2, [] {
    if (g_won || g_lost) return;
    const int rare = g_eden.satelliteCount(int(MapID::RareMetalsCargo))   * 1000;
    const int comm = g_eden.satelliteCount(int(MapID::CommonMetalsCargo)) * 1000;
    const int food = g_eden.satelliteCount(int(MapID::FoodCargo))         * 1000;
    const int pop  = g_eden.satelliteCount(int(MapID::EvacuationModule))  * 100;
    if (rare >= 10000 && comm >= 10000 && food >= 10000 && pop >= 200) {
      g_won = true;
      op2::Game::addMessage("The starship is loaded - your colony escapes Cold Front. Victory!", /*Beep2*/ 85);
      op2::win("Evacuate to the starship");
    }
  });
  // LOSE: your Command Center is gone.
  g_sched.when([] { return !op2::Game::playerOwnsAny(0, MapID::CommandCenter); }, [] {
    if (g_won) return;
    g_lost = true;
    op2::Game::addMessage("Your Command Center is destroyed. Mission failed.");
    op2::lose();
  });

  op2::log::line("Cold Front: setup complete");
}

// ---------------------------------------------------------------------------------------------------------------
// AIProc - drive the scheduler each frame with the current mark.
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
    op2::log::line("cColdFront.dll attached");
  }
  return 1;
}
