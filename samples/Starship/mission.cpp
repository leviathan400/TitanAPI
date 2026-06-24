// mission.cpp - TitanAPI STARSHIP VICTORY demo.
//
// Purpose: isolate and solve the Eden/Plymouth "space race" victory system end to end, in the simplest possible
// mission. A single Eden colony is given a Spaceport, full tech, and large stockpiles; every launchable starship
// component is a mission objective. Each objective goes GREEN the moment its component is launched from the
// Spaceport, and the mission is won when all are launched.
//
// The engine tracks launched components as 1-bit flags in PlayerImpl+8 (the same dword that holds the RLV / Solar
// / EDWARD satellite COUNTS in its low bits). The starship victory count-triggers read those launch bits. The
// rule we learned the hard way: NEVER write PlayerImpl+8 yourself - the engine owns and recomputes it. We just
// set up the colony the ordinary way and let the engine keep the launch state correct.
//
// Build: cmake -S . -B build -G "Visual Studio 18 2026" -A Win32 && cmake --build build --config Release
// Output: cStarship.dll  (logs to OPU\logs\cStarship.log)

#include "op2.hpp"            // the op2:: facade (Game / Player / Unit / orders / BaseBuilder)
#include "op2/trigger.hpp"    // triggers + victory/defeat (include in exactly ONE .cpp)
#include "op2/base.hpp"       // declarative BaseLayout / createBase
#include "op2_mission.hpp"    // the mission-DLL contract (DescBlock / SaveRegion types)
#include "op2_log.hpp"        // file logging -> OPU\logs\cStarship.log
#include "op2_crash.hpp"      // SEH guards + crash logging

#include <vector>

using namespace op2;
using M = abi::MapID;

// ============================================================================================================
// Mission metadata. cm01.map + MULTITEK.TXT is a known-good colony combo (the smoke test uses it). 2 players:
// player 0 is the Eden launch colony; player 1 is a token Plymouth presence the map expects.
// ============================================================================================================
extern "C" __declspec(dllexport) char LevelDesc[]    = "TitanAPI Starship Victory";
extern "C" __declspec(dllexport) char MapName[]      = "cm01.map";
extern "C" __declspec(dllexport) char TechtreeName[] = "MULTITEK.TXT";
extern "C" __declspec(dllexport) mission::ModDesc   DescBlock   = { mission::MissionType::Colony, 2, 12, 0 };
extern "C" __declspec(dllexport) mission::ModDescEx DescBlockEx = { 0, 0, 0, 0, 0, 0, 0, 0 };

namespace {

// Every launchable starship component, in a sensible build order. Each is one victory objective: it fires the
// instant the player launches one from the Spaceport (the engine sets that module's launch bit in PlayerImpl+8;
// the count-trigger on the type reads it). Win = all launched. (RLV / Solar / EDWARD satellites are launch
// COUNTS, not single-launch flags, so they're not part of this "build the starship" objective set.)
struct Component { M type; const char* objective; };
const Component kComponents[] = {
  { M::Skydock,           "Launch the Skydock"             },
  { M::CommandModule,     "Launch the Command Module"      },
  { M::HabitatRing,       "Launch the Habitat Ring"        },
  { M::SensorPackage,     "Launch the Sensor Package"      },
  { M::FuelingSystems,    "Launch the Fueling Systems"     },
  { M::StasisSystems,     "Launch the Stasis Systems"      },
  { M::OrbitalPackage,    "Launch the Orbital Package"     },
  { M::IonDriveModule,    "Launch the Ion Drive Module"    },
  { M::FusionDriveModule, "Launch the Fusion Drive Module" },
  { M::PhoenixModule,     "Launch the Phoenix Module"      },
  { M::RareMetalsCargo,   "Launch the Rare Metals Cargo"   },
  { M::CommonMetalsCargo, "Launch the Common Metals Cargo" },
  { M::FoodCargo,         "Launch the Food Cargo"          },
  { M::EvacuationModule,  "Launch the Evacuation Module"   },
  { M::ChildrenModule,    "Launch the Children Module"     },
};

std::vector<Trigger> g_counts;    // the COUNT condition triggers (fire when a component is launched)
std::vector<Trigger> g_victory;   // the VICTORY-CONDITION wrappers - what the objectives pane actually shows

// How many components are required. The full mission needs all of them launched; lower this (e.g. to 3) to test
// the WIN quickly by launching only the first few. Note you CANNOT pre-set "already launched": the engine
// recomputes the launch bitfield (PlayerImpl+8) from the actual Spaceport contents every cycle, so any bit you
// write by hand is overwritten next cycle - the very mechanism that keeps the objectives honest (red until a
// genuine launch).
constexpr std::size_t kObjectives = std::size(kComponents);

// Create one count + victory condition per required component, exactly as the original SDK does:
//   count trigger:     CreateCountTrigger(enabled, oneShot=TRUE,  player0, type, mapNone, 1, >=)
//   victory condition: CreateVictoryCondition(enabled, oneShot=FALSE, countTrigger, text)
// The victory condition MUST be oneShot=FALSE: a one-shot victory condition is consumed when it fires, so the
// objective would never stay green. For these "space" map IDs the count reads the module's launch bit in +8 (the
// cargo arg is ignored). The engine ANDs all victory conditions, so the mission is won once every one has fired.
void initVictoryConditions() {
  const std::size_t n = (kObjectives < std::size(kComponents)) ? kObjectives : std::size(kComponents);
  for (std::size_t i = 0; i < n; ++i) {
    const Component& c = kComponents[i];
    Trigger cond = onUnitCount(0, c.type, M::None, Compare::GreaterEqual, 1, {}, /*oneShot=*/true);
    Trigger vic  = victoryWhen(cond, c.objective, /*oneShot=*/false);
    g_counts.push_back(cond);
    g_victory.push_back(vic);
  }
  log::linef("  victory: %zu starship launch objectives", n);
}

} // namespace

// ============================================================================================================
// InitProc - build the launch colony once.
// ============================================================================================================
static void initProc() {
  log::stamp("Starship Victory - mission START");
  log::line("Starship Victory: InitProc");

  // Player 0: the Eden launch colony. Full tech so every starship module is buildable, large stockpiles (the
  // modules are expensive), and plenty of colonists to keep the colony active.
  Player you = Game::player(0);
  you.goEden();
  you.setTechLevel(12);
  you.setWorkers(60).setScientists(40).setKids(20);
  you.setCommonOre(50000, 60000).setRareOre(50000).setFood(50000, 60000);

  // The launch colony: Command Center, the Spaceport (build + launch starship parts here), power, food, ore
  // processing + storage, and a Structure Factory - connected by tubes so everything is active.
  BaseLayout base;
  base.buildings = {
    { {40, 80}, M::CommandCenter },
    { {47, 80}, M::Spaceport },
    { {54, 80}, M::StructureFactory },
    { {40, 85}, M::Tokamak },
    { {45, 85}, M::Agridome },
    { {50, 85}, M::Tokamak },
    { {55, 85}, M::CommonOreSmelter },
    { {40, 90}, M::RareOreSmelter },
    { {45, 90}, M::CommonStorage },
    { {50, 90}, M::RareStorage },
    { {55, 90}, M::CommonStorage },   // 2nd Common Metals storage (raises the common-metals cap)
    { {60, 90}, M::RareStorage },     // 2nd Rare Metals storage   (raises the rare-metals cap)
  };
  base.tubes = {
    { {43, 80}, {47, 80} },   // CC -> Spaceport
    { {50, 80}, {54, 80} },   // Spaceport -> Structure Factory
    { {41, 81}, {41, 85} },   // CC column -> power/agridome row
    { {41, 85}, {55, 85} },   // power/processing row
    { {41, 85}, {41, 90} },   // -> storage row
    { {41, 90}, {60, 90} },   // storage row (extended to reach both new storages)
  };
  base.vehicles = {
    { {41, 93}, M::ConVec }, { {43, 93}, M::ConVec },        // a couple of ConVecs to expand/repair
    { {45, 93}, M::CargoTruck }, { {47, 93}, M::CargoTruck },
  };
  const BaseResult r = createBase(you, base, { 0, 10 });   // shift the whole colony 10 tiles south
  log::linef("  colony placed: %d buildings, %d vehicles, %d tube runs", r.buildings, r.vehicles, r.tubeLines);

  you.centerViewOn({ 47, 95 });

  // Player 1: a token Plymouth presence (the 2-player colony map expects a second player). Not driven.
  Player other = Game::player(1);
  other.goPlymouth().goAI();
  ignore(Game::createUnit(M::CommandCenter, { 70, 70 }, other));

  ignore(Game::forceMoraleGood());

  // The objectives: launch every starship component. Created here in InitProc, the SDK way - we do NOT touch
  // PlayerImpl+8; the engine keeps every launch bit at 0 until you actually launch, so all objectives start RED.
  initVictoryConditions();
  loseIfNoCommandCenter(0);

  Game::addMessage("Build and launch every starship component from the Spaceport.");
  log::line("Starship Victory: InitProc done");
}

// ============================================================================================================
// AIProc - this mission has no per-frame AI. We use it only to mirror objective progress into the log: each time
// a component launches, its count trigger fires and (via the oneShot=false victory condition) its objective
// turns green. We log the two bit-strings on the first cycle and whenever they change, so the log tracks the same
// state as the in-game objectives pane - all '1's means every component is up and the mission is won.
// ============================================================================================================
static void aiProc() {
  // One char per objective, in kComponents order (Sky Cmd Hab Sen Fuel Sta Orb Ion Fus Phx RareC ComC Food Evac
  // Kids): '1' = launched / green, '0' = pending / red.
  char launched[64] = { 0 }, green[64] = { 0 };
  for (std::size_t i = 0; i < g_counts.size()  && i < sizeof(launched) - 1; ++i) launched[i] = g_counts[i].hasFired(0)  ? '1' : '0';
  for (std::size_t i = 0; i < g_victory.size() && i < sizeof(green) - 1;    ++i) green[i]    = g_victory[i].hasFired(0) ? '1' : '0';

  static bool first = true;
  static char prevL[64] = { 0 }, prevG[64] = { 0 };
  if (first || std::strcmp(launched, prevL) != 0 || std::strcmp(green, prevG) != 0) {
    first = false;
    std::strcpy(prevL, launched);  std::strcpy(prevG, green);
    log::linef("  [progress t%d] launched=%s green=%s", Game::tick(), launched, green);
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
    log::stamp("Starship Victory - mission END");
  }
  return 1;
}
