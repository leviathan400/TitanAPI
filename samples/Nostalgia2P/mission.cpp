// Nostalgia2P/mission.cpp - a 2-player MULTIPLAYER (Last One Standing) TitanAPI sample, adapted from the classic
// C++ SDK "Nostalgia" mission. Two mirrored colonies start at opposite corners of opu_02.map; last player with a
// Command Center standing wins.
//
// The original SDK defined each base as RELATIVE-coordinate data (a BaseInfo of BuildingInfo/VehicleInfo/
// TubeWallInfo around a corner) and stamped it with CreateBase(player, x, y, base). That maps almost 1:1 onto
// TitanAPI's BaseBuilder: an op2::BaseLayout of relative coords, placed with createBase(player, layout, offset).
// This sample is the showcase for that offset reuse - the same layout is dropped at two map corners.

#include "op2.hpp"            // the TitanAPI Layer 2 facade
#include "op2/base.hpp"       // Module 8 BaseBuilder (BaseLayout / createBase)
#include "op2_mission.hpp"    // the mission-DLL contract
#include "op2_log.hpp"        // file logging
#include "op2_crash.hpp"      // SEH guards + crash logging

using op2::Location;
using MapID = op2::MapID;

// ---------------------------------------------------------------------------------------------------------------
// Mission DLL exports - a 2-player multiplayer Last One Standing game on opu_02.map.
//   DescBlock    = { LastOneStanding, numPlayers = 2, maxTechLevel = 12, unitMission = 0 }
//   DescBlockEx  = { numMultiplayerAIs = 0, ... }  (two human players)
// ---------------------------------------------------------------------------------------------------------------
extern "C" __declspec(dllexport) char      LevelDesc[]    = "2P LoS 'Nostalgia' (TitanAPI)";
extern "C" __declspec(dllexport) char      MapName[]      = "opu_02.map";
extern "C" __declspec(dllexport) char      TechtreeName[] = "MULTITEK.TXT";
extern "C" __declspec(dllexport) op2::mission::ModDesc   DescBlock   = { op2::mission::MissionType::LastOneStanding, 2, 12, 0 };
extern "C" __declspec(dllexport) op2::mission::ModDescEx DescBlockEx = { 0, 0, 0, 0, 0, 0, 0, 0 };

namespace {

// The two corner "anchors" of opu_02.map (the original's base0 / base3 - the Tokamak-corner of each layout).
// NOTE: the old SDK's CreateBase placed buildings with RAW LOCATION() coords (engine frame, no pad), whereas
// its beacons used XYPos() which adds the +31/-1 pad. TitanAPI's createUnit always pads visible->engine, so we
// convert the SDK's engine-frame corners to TitanAPI VISIBLE coords here: visible = (engineX - 31, engineY + 1).
// (Without this the whole base lands 31 tiles off, while the XYPos'd beacons sit correctly.)
constexpr Location kCornerTL{  33 - 31,   1 + 1 };   // SDK engine (33,1)   -> visible (2, 2)
constexpr Location kCornerBR{ 158 - 31, 125 + 1 };   // SDK engine (158,125)-> visible (127, 126)

// ---- the two base layouts, as RELATIVE coordinates (the original buildingSet1/unitSet1/tubeSet1 etc.). Each is
// stamped at its corner by createBase(player, layout, cornerOffset). The top-left layout grows +x/+y from its
// corner; the bottom-right one grows -x/-y (a mirror image), so the two starts are symmetric. ----

op2::BaseLayout topLeftLayout() {                       // buildingSet1 / unitSet1 / tubeSet1
  op2::BaseLayout b;
  b.buildings = {
    { {  2, 10 }, MapID::CommandCenter },   { {  8, 10 }, MapID::StructureFactory },
    { {  8,  6 }, MapID::CommonOreSmelter }, { {  2,  2 }, MapID::Tokamak },
    { {  2, 16 }, MapID::StandardLab },      { {  2, 13 }, MapID::Agridome },
    { { 21, 19 }, MapID::GuardPost, MapID::Microwave },
  };
  b.vehicles = {
    { {  5, 13 }, MapID::ConVec }, { {  7, 13 }, MapID::ConVec }, { {  9, 13 }, MapID::ConVec },
    { {  5, 19 }, MapID::CargoTruck }, { {  7, 19 }, MapID::CargoTruck }, { {  9, 19 }, MapID::CargoTruck },
    { { 16,  3 }, MapID::RoboSurveyor, MapID::None, 3 }, { { 18,  1 }, MapID::RoboMiner, MapID::None, 3 },
    { {  8, 16 }, MapID::Earthworker },
  };
  b.tubes = { { { 5, 10 }, { 5, 10 } }, { { 11, 10 }, { 19, 10 } }, { { 20, 10 }, { 20, 19 } } };
  return b;
}

op2::BaseLayout bottomRightLayout() {                   // buildingSet4 / unitSet4 / tubeSet4 (the mirror)
  op2::BaseLayout b;
  b.buildings = {
    { { -2, -9 }, MapID::CommandCenter },    { { -7, -10 }, MapID::StructureFactory },
    { { -12, -2 }, MapID::CommonOreSmelter }, { { -1, -1 }, MapID::Tokamak },
    { { -2, -15 }, MapID::StandardLab },      { { -2, -12 }, MapID::Agridome },
    { { -22, -18 }, MapID::GuardPost, MapID::Microwave },
  };
  b.vehicles = {
    { { -9, -7 }, MapID::ConVec }, { { -7, -7 }, MapID::ConVec }, { { -5, -7 }, MapID::ConVec },
    { { -9, -19 }, MapID::CargoTruck }, { { -7, -19 }, MapID::CargoTruck }, { { -5, -19 }, MapID::CargoTruck },
    { { -10, -5 }, MapID::RoboSurveyor, MapID::None, 1 }, { { -12, -7 }, MapID::RoboMiner, MapID::None, 1 },
    { { -8, -16 }, MapID::Earthworker },
  };
  b.tubes = { { { -10, -10 }, { -20, -10 } }, { { -21, -10 }, { -21, -18 } },
              { { -12, -4 }, { -12, -9 } }, { { -4, -10 }, { -4, -10 } } };
  return b;
}

// The mining-beacon field (the original extraSet) - base, side, and centre deposits. {at, rare, OreYield}; rare
// 1 = Rare ore, 0 = Common; yield Bar3(0) is richest, then Bar2(1), Bar1(2).
struct Beacon { Location at; int rare; int yield; };
const Beacon kBeacons[] = {
  // base common mines (Bar2)
  { {  15,   8 }, 0, 1 }, { { 120,   8 }, 0, 1 }, { {  15, 124 }, 0, 1 }, { { 120, 124 }, 0, 1 },
  // outside-base + side common mines (Bar2)
  { {  14,  38 }, 0, 1 }, { {  91,  13 }, 0, 1 }, { { 114,  92 }, 0, 1 }, { {  37, 114 }, 0, 1 },
  { {  65,   6 }, 0, 1 }, { { 122,  65 }, 0, 1 }, { {  63, 122 }, 0, 1 }, { {   7,  64 }, 0, 1 },
  // middle common mines (Bar2)
  { {  51,  57 }, 0, 1 }, { {  72,  51 }, 0, 1 }, { {  77,  72 }, 0, 1 }, { {  56,  78 }, 0, 1 },
  // outside-base rare mines (Bar1)
  { {  37,  15 }, 1, 2 }, { { 114,  37 }, 1, 2 }, { {  91, 114 }, 1, 2 }, { {  14,  92 }, 1, 2 },
  // side rare mines (Bar2)
  { {  76,   6 }, 1, 1 }, { { 122,  76 }, 1, 1 }, { {  52, 122 }, 1, 1 }, { {   7,  53 }, 1, 1 },
  // middle rare mines (Bar3 - richest)
  { {  64,  56 }, 1, 0 }, { {  64,  73 }, 1, 0 },
};

// Stock a player's Structure Factory bays with the kits the original queued (so the colony can expand at once).
void stockFactory(int player) {
  for (op2::Unit sf : op2::Game::unitsOfType(MapID::StructureFactory)) {
    if (sf.ownerId() != player) continue;
    sf.setFactoryCargo(0, MapID::Tokamak,        MapID::None);
    sf.setFactoryCargo(1, MapID::Nursery,        MapID::None);
    sf.setFactoryCargo(2, MapID::University,     MapID::None);
    sf.setFactoryCargo(3, MapID::VehicleFactory, MapID::None);
    sf.setFactoryCargo(4, MapID::RobotCommand,   MapID::None);
    sf.setFactoryCargo(5, MapID::Residence,      MapID::None);
    break;
  }
}

// Standard multiplayer starting resources + the common opening research, for `player`.
void setupPlayer(op2::Player p) {
  p.setCommonOre(8000); p.setFood(3000);
  p.setWorkers(25); p.setScientists(15); p.setKids(8); p.setTechLevel(0);
  p.markResearchComplete(3401);   // Cybernetic Teleoperation
  p.markResearchComplete(3305);   // Research Training Programs
  p.markResearchComplete(3304);   // Offspring Enhancement
  p.markResearchComplete(3303);   // Health Maintenance
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------
// InitProc - lay the beacon field, then stamp the two symmetric bases at opposite corners.
// ---------------------------------------------------------------------------------------------------------------
static void initProcImpl() {
  op2::log::line("Nostalgia2P (TitanAPI): InitProc");

  // The whole mining-beacon field.
  for (const Beacon& bn : kBeacons)
    op2::Game::createMine(bn.at, bn.rare ? op2::abi::MineType::RareOre : op2::abi::MineType::CommonOre,
                          op2::abi::OreYield(bn.yield));

  // Randomly assign the two players to the two diagonal corners (the original randomized starts too).
  const bool swap = (op2::Game::getRand(2) != 0);
  struct Start { Location corner; op2::BaseLayout layout; };
  const Start starts[2] = {
    { kCornerTL, topLeftLayout() }, { kCornerBR, bottomRightLayout() },
  };

  for (int i = 0; i < 2; ++i) {
    const op2::Player player = op2::Game::player(i);
    const Start& s = starts[swap ? (1 - i) : i];
    setupPlayer(player);
    const op2::BaseResult r = op2::createBase(player, s.layout, s.corner);
    player.centerViewOn(s.corner);
    stockFactory(i);
    op2::log::linef("  player %d: base at corner (%d,%d), %d units placed", i, s.corner.x, s.corner.y, r.placedUnits());
  }

  // Misc multiplayer setup (matches the original): steady morale, full daylight, a dim starting light level.
  op2::Game::forceMoraleGood();
  op2::Game::setDaylightEverywhere(true);
  op2::Game::setDaylightMoves(false);
  op2::GameMap::setInitialLightLevel(-32);

  op2::Game::addMessage("Last One Standing - destroy the enemy Command Center. Good luck, have fun!");
  op2::log::line("Nostalgia2P: setup complete");
}

static void aiProcImpl() {}   // Last One Standing is engine-driven; no per-frame scripting needed.

// ---- exported entry points ----
extern "C" __declspec(dllexport) int  InitProc() { op2::crash::guard("InitProc", &initProcImpl); return 1; }
extern "C" __declspec(dllexport) void AIProc()   { op2::crash::guard("AIProc",   &aiProcImpl); }
extern "C" __declspec(dllexport) void GetSaveRegions(op2::mission::SaveRegion* p) { if (p) { p->pData = nullptr; p->size = 0; } }

extern "C" int __stdcall DllMain(void*, unsigned long reason, void*) {
  if (reason == 1 /*DLL_PROCESS_ATTACH*/) {
    op2::crash::installHandler();
    op2::log::setTickSource([] { return op2::Game::tick(); });
    op2::log::line("cNostalgia2P.dll attached");
  }
  return 1;
}
