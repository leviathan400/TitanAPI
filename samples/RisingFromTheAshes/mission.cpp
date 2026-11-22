// mission.cpp - "Rising From The Ashes", ported to TitanAPI (C++23).
//
// ORIGINAL: an Outpost 2 colony/space-race mission by Vagabond (Outpost Universe, 2016), written against the
// classic OP2MissionSDK (Outpost2DLL + HFL + OP2Helper). Source: github.com/OutpostUniverse/OP2MissionRisingFromTheAshes.
// This is a faithful, well-commented re-implementation on the modern TitanAPI facade.
//
// PREMISE: You command the Eden colony (player 0, Blue) at the NORTH end of the tall Haise Valley (Ashes.map,
// 64 x 256). The planet's resources can no longer sustain you - your only way out is to build and LAUNCH AN
// EVACUATION STARSHIP (a Skydock plus a transport carrying 200 colonists and 10,000 each of food / common /
// rare metals) before you run dry. A Plymouth AI (player 1, Red), led by Rachel Borges, holds a richer outpost
// in the SOUTH. Plymouth stays passive UNTIL PROVOKED (attack its southern outpost and "the war opens"), then
// escalates its raids as you progress: build a Spaceport -> heavier waves; research the Phoenix Module -> the
// heaviest waves, each milestone also drawing a dedicated strike. Meanwhile a disaster drizzle (meteors, quakes,
// storms, valley tornadoes) builds toward the guaranteed late-mission eruption of Mount Swigert (50,125).
//
// VICTORY: complete all five starship objectives (the engine ANDs them). DEFEAT: the colony becomes non-viable
// (too few colonists / no way to rebuild a Command Center / lost the space or Phoenix-Module tech path) and
// stays that way for 90 ticks.
//
// COORDINATES: the original used the OP2Helper X_=31 / Y_=-1 offset to turn its "game" coordinates into engine
// tiles. That offset IS TitanAPI's visible->engine pad exactly, so every original game coordinate maps 1:1 to a
// TitanAPI VISIBLE tile - the numbers below are the same numbers the original used.
//
// PORT NOTES (adaptations, all flagged inline):
//   * The original's engine time-triggers (CreateTimeTrigger with enable/disable/HasFired) are driven here from
//     AIProc with a small tick scheduler - the same behaviour, expressed as plain state. Victory still uses the
//     engine victory system (victoryWhen) so the objectives show in the status pane.
//   * The AI's OP2Helper BaseBuilder is reproduced with TitanAPI groups (createBuildingGroup / createMiningGroup
//     / createFightGroup). Tube/wall lines are laid directly (createTubeLine/createWallLine) rather than recorded
//     into the build group (TitanAPI groups don't record tubes).
//   * Cosmetic-only original bits with no facade equivalent (fumarole vents, the magic lava-flow animation tiles,
//     the Win32/odasl briefing dialog, the music playlist) are omitted and noted where they occurred.
//
// MAP DEPENDENCY: needs "Ashes.map" and the "multitekNoLavaWalls.txt" tech tree in your Outpost 2 / OPU install
// (both ship in the original mission's repo).

#include "op2.hpp"            // the op2:: facade (Game / Player / Unit / orders / BaseBuilder)
#include "op2/trigger.hpp"    // triggers + victory/defeat (include in exactly ONE .cpp)
#include "op2/base.hpp"       // declarative BaseLayout / createBase
#include "op2/groups.hpp"     // AI ScGroups (FightGroup / MiningGroup / BuildingGroup)
#include "op2_mission.hpp"    // the mission-DLL contract (DescBlock / SaveRegion types)
#include "op2_log.hpp"        // file logging
#include "op2_crash.hpp"      // SEH guards + crash logging

#include <vector>

using namespace op2;

// ============================================================================================================
// Constants
// ============================================================================================================
namespace {
constexpr int        kPhoenixModuleTech = 10401;   // research id that gates the End-tier waves + the win path
constexpr Location   kVolcanoEruption{ 50, 125 };  // Mount Swigert eruption point
constexpr int        kEruptionStrength = 80;        // lava spread/strength
constexpr int        kFailureGraceTicks = 90;       // a non-viable colony must persist this long to lose
constexpr int        kAIReinforceSouth_y = 256;     // AI vehicles spawn along the southern map edge

// Player indices, named for readability. (Non-const: the fluent setup setters return Player&.)
Player kHuman{ 0 };   // Eden, Blue (you)
Player kAI{ 1 };      // Plymouth, Red (the enemy)

// Difficulty (both players share the chosen game difficulty in a colony game).
Player::Difficulty diff() { return kHuman.difficulty(); }
bool isEasy()   { return diff() == Player::Difficulty::Easy; }
bool isNormal() { return diff() == Player::Difficulty::Normal; }
bool isHard()   { return diff() == Player::Difficulty::Hard; }
} // namespace

// ============================================================================================================
// Mission state - the modern equivalent of the original's saved `ScriptGlobal` struct. (A real save/load port
// would expose this as a save region; here it is process-static, which is enough to run the mission.)
// ============================================================================================================
namespace {
struct State {
  // --- AI groups (the BaseBuilder framework) ---
  Group periodicFightGroup;          // the recurring raid waves
  Group specialFightGroup;           // the milestone strikes (rare-mine / spaceport / phoenix)
  Group baseDefenseFightGroup;       // the standing southern-base guard force
  Group colonyExpansionBuildGroup;   // rebuilds the AI's full intended base over time
  Group reinforceVehicleBuildGroup;  // the Vehicle Factory replacing lost trucks/combat vehicles
  Group repairGroup;                 // holds the AI repair ConVec
  Unit  aiRepairConvec;              // continuously repairs the most-damaged AI building

  // --- beacons (the AI's two mining deposits) ---
  Location aiCommonBeacon, aiRareBeacon;
  Unit     aiCommonMine, aiRareMine;
  Unit     aiCommonSmelterW, aiCommonSmelterE, aiRareSmelterW, aiRareSmelterE;

  // --- the tick scheduler (replaces the engine time-triggers) ---
  // Periodic raids: disabled (-1) until the player provokes Plymouth, then a new random interval each fire.
  int  nextRaidTick = -1;
  // Disaster cycle: enabled from the start, random interval each fire.
  int  nextDisasterTick = 0;
  // Volcano: a single scheduled eruption.
  int  eruptionTick = 0;  bool erupted = false;
  // The three one-shot milestone strikes: armed (a fire-tick is set) when the condition is met, then fired once.
  int  rareMineStrikeTick = -1;  bool rareMineFired = false;
  int  spaceportStrikeTick = -1; bool spaceportFired = false;
  int  phoenixStrikeTick   = -1; bool phoenixFired   = false;
  // AI reinforcement drop: armed when the AI economy is crippled but its CC still stands.
  int  reinforceTick = -1;

  // --- failure gating ---
  int  failSinceTick = -1;  // tick when the colony first became non-viable (-1 = viable)
  bool lost = false;        // defeat fired (fire lose() exactly once)

  bool aiVehicleFactoryDestroyed = false;
} g;
} // namespace

// ============================================================================================================
// Small helpers
// ============================================================================================================
namespace {

// createUnit that returns a bare handle (null Unit on failure) - convenient for the many setup placements.
Unit mk(abi::MapID type, Location at, Player owner, abi::MapID weaponOrCargo = abi::MapID::None) {
  auto r = Game::createUnit(type, at, owner, weaponOrCargo);
  return r ? *r : Unit{};
}

// Spawn `count` vehicles of `type` (each fitted with `weapon`) in a line heading east from `start`, `spacing`
// tiles apart, collecting them into `out`. Mirrors the original VehicleBuilder::CreateLineOfVehicles.
void createLine(std::vector<Unit>& out, Player owner, Location start, int spacing,
                abi::MapID type, abi::MapID weapon, int count) {
  for (int i = 0; i < count; ++i)
    if (Unit u = mk(type, { start.x + i * spacing, start.y }, owner, weapon); u.valid()) out.push_back(u);
}

// Turret-list variant: one vehicle of `type` per weapon in `turrets` (a mixed column).
void createLine(std::vector<Unit>& out, Player owner, Location start, int spacing,
                abi::MapID type, std::initializer_list<abi::MapID> turrets) {
  int i = 0;
  for (abi::MapID w : turrets)
    if (Unit u = mk(type, { start.x + i++ * spacing, start.y }, owner, w); u.valid()) out.push_back(u);
}

// A random point along the southern map edge (x in 1..53, y = 256) - where every AI wave marches in from.
Location randSouthernStart() { return { 1 + Game::getRand(53), kAIReinforceSouth_y }; }

// A random standing player-0 building of one of the AI's preferred target types (CC, Tokamak, factories,
// smelters, mines, Spaceport, ...). Mirrors AIFightGroupMaker::GetAttackTarget. Returns a null Unit if the
// player has nothing left worth hitting.
Unit getAttackTarget() {
  static const abi::MapID kTargets[] = {
    abi::MapID::CommandCenter, abi::MapID::Tokamak, abi::MapID::StructureFactory, abi::MapID::CommonOreSmelter,
    abi::MapID::RareOreSmelter, abi::MapID::RareOreMine, abi::MapID::CommonOreMine, abi::MapID::Spaceport,
    abi::MapID::VehicleFactory, abi::MapID::AdvancedLab,
  };
  std::vector<Unit> standing;
  for (abi::MapID t : kTargets)
    for (Unit b : Game::unitsOfType(t))
      if (b.ownerId() == 0 && b.isLive()) standing.push_back(b);
  return standing.empty() ? Unit{} : standing[Game::getRand(int(standing.size()))];
}

// Add `units` to `group`, then send the group to attack. If a still-standing player building of `targetType`
// exists, attack it specifically; otherwise hunt the nearest enemy. Mirrors VehicleBuilder::PopulateFightGroup.
void sendWave(Group group, const std::vector<Unit>& units, Unit target) {
  for (Unit u : units) group.takeUnit(u);
  if (target.valid()) {
    group.setTargetUnit(target); group.doAttackUnit();
    log::ailinef("    wave dispatched: %d units -> attack target type=%d at vis(%d,%d)",
                 int(units.size()), int(target.type()), target.location().x, target.location().y);
  } else {
    group.setAttackType(abi::MapID::Any); group.doAttackEnemy();
    log::ailinef("    wave dispatched: %d units -> hunt enemy (no standing target)", int(units.size()));
  }
}

} // namespace

// ============================================================================================================
// AI attack waves - the escalation tiers + the milestone strikes (ported from AIFightGroups.cpp verbatim).
// ============================================================================================================
namespace waves {
using M = abi::MapID;

// ---- Initial tier (before the player builds a Spaceport) ----
void initialMixed(Location s) {                                    // 8 Lynx: foam + 6 microwave + foam
  std::vector<Unit> u;
  createLine(u, kAI, s, 1, M::Lynx, { M::Stickyfoam, M::Microwave, M::Microwave, M::Microwave,
                                      M::Microwave, M::Microwave, M::Microwave, M::Stickyfoam });
  sendWave(g.periodicFightGroup, u, getAttackTarget());
}
void initialStealth(Location s) {                                  // 6 Lynx, lights OFF (surprise raid)
  std::vector<Unit> u;
  createLine(u, kAI, s, 1, M::Lynx, { M::EMP, M::Starflare, M::Stickyfoam, M::Starflare, M::RPG, M::RPG });
  sendWave(g.periodicFightGroup, u, getAttackTarget());
  g.periodicFightGroup.setLights(false);
}
void initialScorp(Location s) {                                    // 12 + 8 Scorpion swarm
  std::vector<Unit> u;
  createLine(u, kAI, s,                 1, M::Scorpion, M::EnergyCannon, 12);
  createLine(u, kAI, { s.x, s.y - 1 },  1, M::Scorpion, M::EnergyCannon, 8);
  sendWave(g.periodicFightGroup, u, getAttackTarget());
}

// ---- Middle tier (after the player builds a Spaceport) ----
void middleMixed(Location s) {                                     // Panther column (+3 turrets on Hard)
  std::vector<M> t{ M::Stickyfoam, M::Microwave, M::Microwave, M::RPG, M::EMP, M::EMP, M::Stickyfoam };
  if (isHard()) { t.push_back(M::ESG); t.push_back(M::ESG); t.push_back(M::EMP); }
  std::vector<Unit> u; int i = 0;
  for (M w : t) if (Unit v = mk(M::Panther, { s.x + i++, s.y }, kAI, w); v.valid()) u.push_back(v);
  sendWave(g.periodicFightGroup, u, getAttackTarget());
}
void middleStealth(Location s) {                                   // 8 Lynx w/ Supernova, lights OFF (+3 on Hard)
  std::vector<M> t{ M::EMP, M::Supernova, M::Stickyfoam, M::Supernova, M::RPG, M::RPG, M::Supernova, M::EMP };
  if (isHard()) { t.push_back(M::Supernova); t.push_back(M::EMP); t.push_back(M::ESG); }
  std::vector<Unit> u; int i = 0;
  for (M w : t) if (Unit v = mk(M::Lynx, { s.x + i++, s.y }, kAI, w); v.valid()) u.push_back(v);
  sendWave(g.periodicFightGroup, u, getAttackTarget());
  g.periodicFightGroup.setLights(false);
}
void middleScorp(Location s) {                                     // 12 + 12 + 8 Scorpion swarm
  std::vector<Unit> u;
  createLine(u, kAI, s,                1, M::Scorpion, M::EnergyCannon, 12);
  createLine(u, kAI, { s.x, s.y - 1 }, 1, M::Scorpion, M::EnergyCannon, 12);
  createLine(u, kAI, { s.x, s.y - 2 }, 1, M::Scorpion, M::EnergyCannon, 8);
  sendWave(g.periodicFightGroup, u, getAttackTarget());
}

// ---- End tier (after the player researches the Phoenix Module) ----
void endRpg(Location s) {                                          // 12 + 12 Tigers (RPG)
  std::vector<Unit> u;
  createLine(u, kAI, s,                1, M::Tiger, M::RPG, 12);
  createLine(u, kAI, { s.x, s.y - 1 }, 1, M::Tiger, M::RPG, 12);
  sendWave(g.periodicFightGroup, u, getAttackTarget());
}
void endMixed(Location s) {                                        // 12-turret Tiger column
  std::vector<Unit> u;
  createLine(u, kAI, s, 1, M::Tiger, { M::Stickyfoam, M::RPG, M::ESG, M::EMP, M::RPG, M::EMP,
                                       M::ESG, M::EMP, M::RPG, M::ESG, M::EMP, M::Microwave });
  sendWave(g.periodicFightGroup, u, getAttackTarget());
}

// Pick one random sub-type within the current tier (the original rolls GetRand(3); End rolls Mixed 2:1).
void randomWave() {
  const Location s = randSouthernStart();
  // tier: Initial by default, Middle once a Spaceport strike has fired, End once the Phoenix strike has fired.
  const char* tier = g.phoenixFired ? "End" : g.spaceportFired ? "Middle" : "Initial";
  log::ailinef("periodic raid [%s tier] spawning from south vis(%d,%d)", tier, s.x, s.y);
  if (g.phoenixFired) {        switch (Game::getRand(3)) { case 0: endRpg(s); break; default: endMixed(s); } }
  else if (g.spaceportFired) { switch (Game::getRand(3)) { case 0: middleMixed(s); break; case 1: middleScorp(s); break; default: middleStealth(s); } }
  else {                       switch (Game::getRand(3)) { case 0: initialMixed(s); break; case 1: initialScorp(s); break; default: initialStealth(s); } }
}

// ---- the three one-shot milestone strikes (target the player's rare mine / Spaceport) ----
void rareMineStrike() {                                            // 8 Lynx (RPG) - and THIS opens the war
  std::vector<Unit> u;
  createLine(u, kAI, randSouthernStart(), 1, M::Lynx, M::RPG, 8);
  Unit mine; for (Unit b : Game::unitsOfType(M::RareOreMine)) if (b.ownerId() == 0 && b.isLive()) { mine = b; break; }
  sendWave(g.specialFightGroup, u, mine.valid() ? mine : getAttackTarget());
}
void spaceportStrike() {                                           // 10 Panthers (+2 ESG on Hard)
  std::vector<M> t{ M::EMP, M::RPG, M::ESG, M::Stickyfoam, M::EMP, M::ESG, M::RPG, M::EMP, M::Stickyfoam, M::RPG };
  if (isHard()) { t.push_back(M::ESG); t.push_back(M::ESG); }
  std::vector<Unit> u; int i = 0;
  for (M w : t) if (Unit v = mk(M::Panther, { 1 + Game::getRand(53) + i, kAIReinforceSouth_y }, kAI, w); v.valid()) { u.push_back(v); ++i; }
  Unit sp; for (Unit b : Game::unitsOfType(M::Spaceport)) if (b.ownerId() == 0 && b.isLive()) { sp = b; break; }
  sendWave(g.specialFightGroup, u, sp.valid() ? sp : getAttackTarget());
}
void phoenixStrike() {                                             // 14 Tigers, heavy mixed turrets
  std::vector<Unit> u;
  createLine(u, kAI, randSouthernStart(), 1, M::Tiger, { M::EMP, M::RPG, M::RPG, M::Stickyfoam, M::ESG, M::RPG,
                                                         M::EMP, M::EMP, M::ESG, M::RPG, M::RPG, M::EMP, M::RPG, M::ESG });
  Unit sp; for (Unit b : Game::unitsOfType(M::Spaceport)) if (b.ownerId() == 0 && b.isLive()) { sp = b; break; }
  sendWave(g.specialFightGroup, u, sp.valid() ? sp : getAttackTarget());
}
} // namespace waves

// ============================================================================================================
// Random disasters (ported from Disasters.cpp). Weighted roll: 15% none / 35% meteor / 20% quake / 20% storm /
// 10% vortex. Large meteors are suppressed before mark 200 and steered away from critical buildings.
// ============================================================================================================
namespace {
double dist(Location a, Location b) { double dx = a.x - b.x, dy = a.y - b.y; return dx * dx + dy * dy; }  // squared
Location randMapLoc() { return { Game::getRand(64), Game::getRand(256) }; }

void rollDisaster(const std::vector<Location>& critical) {
  const int r = Game::getRand(100);
  if (r < 15) return;                                                    // 15%: nothing this cycle

  if (r < 50) {                                                          // 35%: meteor
    Location at = randMapLoc();
    int sizeRoll = Game::getRand(10);
    int size = (sizeRoll <= 4) ? 0 : (sizeRoll <= 8) ? 1 : 2;           // ~50% small, ~40% medium, ~10% large
    if (size == 2) {
      if (Game::mark() < 200) size = 1;                                  // no catastrophic strikes early on
      for (std::size_t i = 0; i < critical.size(); ++i)                  // and never on a critical building
        if (dist(critical[i], at) < 25.0) { at = randMapLoc(); i = std::size_t(-1); }
    }
    ignore(Game::createMeteor(at, size));
  } else if (r < 70) {                                                   // 20%: minor earthquake
    ignore(Game::createEarthquake(randMapLoc(), Game::getRand(2)));
  } else if (r < 90) {                                                   // 20%: lightning storm crossing the valley
    Location a = randMapLoc(), b = randMapLoc();
    while (dist(a, b) < 3600.0) b = randMapLoc();                        // ends >= 60 tiles apart
    ignore(Game::createStorm(a, b, Game::getRand(55)));
  } else {                                                               // 10%: mid-valley tornado
    auto inBand = [] { return Location{ 1 + Game::getRand(63), 100 + Game::getRand(48) }; };  // rect (1,100)-(64,148)
    ignore(Game::createVortex(inBand(), inBand(), Game::getRand(45) + 5));
  }
}
} // namespace

// ============================================================================================================
// Base builders
// ============================================================================================================
namespace {

// ---- the human Eden colony in the north (PlayerBaseBuilder.cpp) ----
void buildPlayerBase() {
  // Starting resources scale down with difficulty (the "we can't sustain ourselves" premise). Common ore is
  // "ore"; Eden starts with NO rare ore and a tight worker count.
  int workers, food, ore;
  if      (isEasy())   { workers = 28; food = 3000; ore = 5000; }
  else if (isNormal()) { workers = 26; food = 2000; ore = 4000; }
  else                 { workers = 23; food = 1000; ore = 3000; }
  kHuman.setWorkers(workers).setScientists(13).setKids(15)
        .setCommonOre(ore, 50000).setFood(food, 50000).setRareOre(0);
  // Initialize the research/tech state: level 1 grants the starting techs and keeps the Phoenix Module (and the
  // rest of the tree) as things you still research. (The spuriously-green starship objectives once blamed on the
  // tech state were really TitanAPI <= 0.6.7 calling the _Player capability checks on PlayerImpl - fixed in 0.6.10.)
  kHuman.setTechLevel(1);

  BaseLayout base;
  base.buildings = { { {24, 48}, abi::MapID::CommandCenter }, { {32, 48}, abi::MapID::Agridome },
                     { {24, 55}, abi::MapID::StructureFactory }, { {32, 55}, abi::MapID::CommonOreSmelter },
                     { { 6, 55}, abi::MapID::Tokamak } };
  base.tubes     = { { {25, 48}, {31, 48} },    // CC -> Agridome
                     { {24, 49}, {24, 55} },    // CC -> Structure Factory
                     { {24, 55}, {32, 55} } };  // CC -> Smelter
  createBase(kHuman, base);

  // Starting vehicles: a 2-spacing line heading east from (22,59). Lights ON (matches the original - the colony
  // starts in darkness, so the starting fleet runs its headlights).
  const std::vector<abi::MapID> kit{ abi::MapID::ConVec, abi::MapID::ConVec, abi::MapID::Earthworker,
                                     abi::MapID::RoboMiner, abi::MapID::RoboSurveyor, abi::MapID::CargoTruck,
                                     abi::MapID::CargoTruck, abi::MapID::RoboDozer };
  for (std::size_t i = 0; i < kit.size(); ++i)
    if (Unit u = mk(kit[i], { 22 + int(i) * 2, 59 }, kHuman); u.valid()) op2::ignore(u.setLights(true));

  kHuman.centerViewOn({ 28, 51 });
}

// ---- the AI mining groups: trucks shuttle a mine -> a smelter ----
void setupAIMiningGroup(Unit mine, Unit smelter, int trucks) {
  Group mg = createMiningGroup(kAI);
  // seed cargo trucks next to the smelter and hand them to the group
  for (int i = 0; i < trucks; ++i)
    if (Unit t = mk(abi::MapID::CargoTruck, { smelter.location().x + i, smelter.location().y + 1 }, kAI); t.valid())
      mg.takeUnit(t);
  mg.setupMining(mine, smelter, { 35, 176 }, { 55, 182 });
  g.reinforceVehicleBuildGroup.recordVehReinforceGroup(mg, trucks >= 3 ? 3 : 2);  // VF keeps the haul stocked
}

// ---- the Plymouth AI base in the south (SetupAIBase.cpp) ----
void buildAIBase() {
  // Generous resources - Plymouth is the better-supplied colony.
  kAI.setCommonOre(24500, 80000).setRareOre(2000).setFood(99000, 200000).setWorkers(50).setScientists(20).setKids(50);

  // Core structures (kept as handles - the groups need them).
  Unit cc       = mk(abi::MapID::CommandCenter,   { 30, 208 }, kAI);
  Unit sf       = mk(abi::MapID::StructureFactory,{ 30, 202 }, kAI);
  Unit vf       = mk(abi::MapID::VehicleFactory,  { 18, 198 }, kAI);
  Unit tokamak  = mk(abi::MapID::Tokamak,         {  4, 177 }, kAI);   // original spawns this pre-damaged (flavor)
  (void)cc; (void)tokamak;
  g.aiRareSmelterE   = mk(abi::MapID::RareOreSmelter,   { 42, 185 }, kAI);
  g.aiRareSmelterW   = mk(abi::MapID::RareOreSmelter,   { 35, 185 }, kAI);
  g.aiCommonSmelterW = mk(abi::MapID::CommonOreSmelter, { 49, 185 }, kAI);
  g.aiCommonSmelterE = mk(abi::MapID::CommonOreSmelter, { 55, 185 }, kAI);
  mk(abi::MapID::Agridome,      { 30, 195 }, kAI);
  mk(abi::MapID::CommonStorage, { 25, 196 }, kAI);
  mk(abi::MapID::RareStorage,   { 23, 196 }, kAI);
  // Mine structures sit ON the beacons created in InitProc. NOTE: the original builds the RARE mine with the
  // *common* mine type (an engine quirk it documents as required); we keep that behaviour.
  g.aiCommonMine = mk(abi::MapID::CommonOreMine, g.aiCommonBeacon, kAI);
  g.aiRareMine   = mk(abi::MapID::CommonOreMine, g.aiRareBeacon,   kAI);

  // Tubes connecting the southern base (laid directly - see PORT NOTES).
  Game::createTubeLine({ 20, 188 }, { 55, 188 });   // rare smelters -> common smelters
  Game::createTubeLine({ 33, 189 }, { 33, 208 });   // smelters -> agridome -> SF -> CC
  Game::createTubeLine({ 32, 198 }, { 19, 198 });   // SF -> Vehicle Factory

  // AI starting vehicles.
  g.aiRepairConvec = mk(abi::MapID::ConVec, { 22, 200 }, kAI);   // continuously repairs damaged buildings
  Unit buildConvec = mk(abi::MapID::ConVec,      { 24, 200 }, kAI);
  Unit earthworker = mk(abi::MapID::Earthworker, { 26, 178 }, kAI);

  // The Vehicle Factory drives reinforcement: it keeps the mining/repair/defense groups stocked.
  g.reinforceVehicleBuildGroup = createBuildingGroup(kAI);
  g.reinforceVehicleBuildGroup.takeUnit(vf);

  setupAIMiningGroup(g.aiCommonMine, g.aiCommonSmelterW, 3);
  setupAIMiningGroup(g.aiCommonMine, g.aiCommonSmelterE, 3);
  setupAIMiningGroup(g.aiRareMine,   g.aiRareSmelterW,   3);
  setupAIMiningGroup(g.aiRareMine,   g.aiRareSmelterE,   3);

  // The colony-expansion build group: rebuilds the full intended base over time (the AI's "self-healing").
  const Location safeTL{ 10, 176 }, safeBR{ 46, 212 };
  g.colonyExpansionBuildGroup = createBuildingGroup(kAI);
  g.colonyExpansionBuildGroup.takeUnit(sf);
  g.colonyExpansionBuildGroup.takeUnit(vf);
  g.colonyExpansionBuildGroup.takeUnit(earthworker);
  g.colonyExpansionBuildGroup.takeUnit(buildConvec);
  g.colonyExpansionBuildGroup.setBuildRect(safeTL, safeBR);
  // Recorded expansion: a second power/farm tier, science, residences, storages, robot command, and a ring of
  // Guard Posts. The engine rebuilds any of these that fall.
  using B = abi::MapID;
  struct Rec { Location at; B type; B weapon; };
  static const Rec recs[] = {
    { {  4, 212 }, B::Tokamak,    B::None }, { { 30, 192 }, B::Agridome,   B::None },
    { { 36, 192 }, B::University, B::None }, { { 36, 195 }, B::Nursery,    B::None },
    { { 25, 191 }, B::Residence,  B::None }, { { 22, 191 }, B::Residence,  B::None },
    { { 27, 186 }, B::RareStorage,B::None }, { { 25, 186 }, B::RareStorage,B::None },
    { { 23, 186 }, B::RareStorage,B::None }, { { 23, 201 }, B::RareStorage,B::None },
    { { 36, 208 }, B::RobotCommand, B::None },
    { { 51, 164 }, B::GuardPost, B::ESG }, { { 15, 167 }, B::GuardPost, B::ESG },
    { { 10, 167 }, B::GuardPost, B::ESG }, { { 27, 164 }, B::GuardPost, B::ESG },
    { { 63, 169 }, B::GuardPost, B::ESG }, { { 35, 163 }, B::GuardPost, B::ESG },
    { { 44, 163 }, B::GuardPost, B::ESG }, { { 53, 170 }, B::GuardPost, B::Stickyfoam },
    { { 39, 166 }, B::GuardPost, B::Stickyfoam }, { { 23, 168 }, B::GuardPost, B::Stickyfoam },
  };
  for (const Rec& rb : recs) g.colonyExpansionBuildGroup.recordBuilding(rb.at, rb.type, rb.weapon);
  g.colonyExpansionBuildGroup.setTargCount(B::ConVec,      B::None, 1);
  g.colonyExpansionBuildGroup.setTargCount(B::Earthworker, B::None, 1);
  g.colonyExpansionBuildGroup.setTargCount(B::RoboMiner,   B::None, 1);
  g.colonyExpansionBuildGroup.recordVehReinforceGroup(g.colonyExpansionBuildGroup, 1);

  // A defensive lava-wall ring + connecting tubes for the northern guard posts (laid directly).
  Game::createWallLine(B::LavaWall, {  9, 165 }, { 17, 168 });
  Game::createWallLine(B::LavaWall, { 25, 162 }, { 37, 161 });
  Game::createWallLine(B::LavaWall, { 42, 161 }, { 45, 161 });
  Game::createTubeLine({ 10, 170 }, { 27, 170 });
  Game::createTubeLine({ 20, 171 }, { 20, 187 });   // colony -> western guard posts
  Game::createTubeLine({ 28, 166 }, { 51, 165 });

  // The repair group holds the repair ConVec; the VF reinforces it if it is lost.
  g.repairGroup = createBuildingGroup(kAI);
  g.repairGroup.setBuildRect(safeTL, safeBR);
  g.repairGroup.takeUnit(g.aiRepairConvec);
  g.repairGroup.setTargCount(B::ConVec, B::None, 1);
  g.reinforceVehicleBuildGroup.recordVehReinforceGroup(g.repairGroup, 2);
}

// ---- mining beacons (CreateMineBeacons) ----
void createMineBeacons() {
  // Player common beacon: random point in the northern rect (36,62)-(49,68).
  ignore(Game::createMine({ 36 + Game::getRand(14), 62 + Game::getRand(7) },
                                abi::MineType::CommonOre, abi::OreYield::Bar2));
  // AI rare + common beacons: random points in the southern rect (38,172)-(63,176), kept >= 4 tiles apart.
  auto inSouth = [] { return Location{ 38 + Game::getRand(26), 172 + Game::getRand(5) }; };
  g.aiRareBeacon = inSouth();
  g.aiCommonBeacon = g.aiRareBeacon;
  while (dist(g.aiCommonBeacon, g.aiRareBeacon) < 16.0) g.aiCommonBeacon = inSouth();
  ignore(Game::createMine(g.aiCommonBeacon, abi::MineType::CommonOre, abi::OreYield::Bar1));
  ignore(Game::createMine(g.aiRareBeacon,   abi::MineType::RareOre,   abi::OreYield::Bar1));
}

// ---- the standing southern-base guard force (SetupBaseDefenseFightGroup) ----
void setupBaseDefense() {
  g.baseDefenseFightGroup = createFightGroup(kAI);
  std::vector<Unit> u;
  createLine(u, kAI, { 11, 179 }, 2, abi::MapID::Lynx, abi::MapID::Microwave, 6);  // 12 Lynx on two rows
  createLine(u, kAI, { 11, 178 }, 2, abi::MapID::Lynx, abi::MapID::Microwave, 6);
  for (Unit v : u) g.baseDefenseFightGroup.takeUnit(v);
  g.baseDefenseFightGroup.setTargCount(abi::MapID::Lynx, abi::MapID::Microwave, 8);   // VF keeps it at this strength
  g.baseDefenseFightGroup.setTargCount(abi::MapID::Lynx, abi::MapID::RPG, 4);
  g.baseDefenseFightGroup.setTargCount(abi::MapID::Lynx, abi::MapID::EMP, 4);
  g.baseDefenseFightGroup.setTargCount(abi::MapID::Lynx, abi::MapID::Stickyfoam, 4);
  g.baseDefenseFightGroup.setIdleRect({ 1, 166 }, { 64, 217 });
  g.reinforceVehicleBuildGroup.recordVehReinforceGroup(g.baseDefenseFightGroup, 1);
}

} // namespace

// ============================================================================================================
// Victory & defeat
// ============================================================================================================
namespace {

// The standard Eden/Plymouth starship "space race": five objectives, each satisfied once the player owns >= 1
// of the matching evacuation-cargo / launch unit. The engine ANDs all victory conditions, so you win when all
// five are met. (victoryWhen wraps a count condition; the objective text shows in the status pane.)
void initVictoryConditions() {
  // Exactly the original SDK recipe: per starship-module type, a one-shot count trigger on player 0, cargo
  // mapNone, count >= 1. For these special "space" map IDs the engine's count reads its own launch-flag bits in
  // PlayerImpl+8 (which it owns and keeps at 0 until you actually launch), so the cargo arg is ignored - we pass
  // None to match the original. Win = all five met (the engine ANDs victory conditions).
  // Count trigger oneShot=TRUE; victory condition oneShot=FALSE. A one-shot victory condition is consumed when
  // it fires, so the objective never stays green - proven in samples/Starship. (victoryWhen defaults to false.)
  auto objective = [](abi::MapID type, const char* text) {
    Trigger cond = onUnitCount(0, type, abi::MapID::None, Compare::GreaterEqual, 1, {}, /*oneShot=*/true);
    victoryWhen(cond, text, /*oneShot=*/false);
  };
  objective(abi::MapID::EvacuationModule,  "Evacuate 200 colonists to the starship.");
  objective(abi::MapID::RareMetalsCargo,   "Evacuate 10000 units of Rare Metals to the starship.");
  objective(abi::MapID::CommonMetalsCargo, "Evacuate 10000 units of Common Metals to the starship.");
  objective(abi::MapID::FoodCargo,         "Evacuate 10000 units of food to the starship.");
  objective(abi::MapID::Skydock,           "Launch the Skydock.");
}

// You are on the path to losing if the colony can no longer complete the starship (CheckIfGameFailed). The
// caller gates this with a 90-tick grace period so a transient state doesn't instantly fail you.
bool colonyNonViable() {
  kHuman.resetChecks();   // CRITICAL: recompute the cached capability flags before reading canBuildSpace/canDoResearch
  const bool fewWorkers = kHuman.workers() < 8;
  const bool fewSci     = kHuman.scientists() < 3;
  // No Command Center, and no means to build one (a ConVec to carry a CC kit, or a Structure Factory to make it).
  const bool noCC = !(Game::playerOwnsAny(0, abi::MapID::CommandCenter)
                   || Game::playerOwnsAny(0, abi::MapID::ConVec)
                   || Game::playerOwnsAny(0, abi::MapID::StructureFactory));
  const bool noSpace = !kHuman.canBuildSpace();                               // lost the space-program path
  const bool noPhoenix = !kHuman.hasTechnology(kPhoenixModuleTech) && !kHuman.canDoResearch(kPhoenixModuleTech);
  const bool failed = fewWorkers || fewSci || noCC || noSpace || noPhoenix;
  // Log the FIRST time we go non-viable, so a mis-firing condition is obvious in the log (one line, not spam).
  static bool logged = false;
  if (failed && !logged) {
    logged = true;
    log::linef("  non-viable: workers<8=%d sci<3=%d noCC=%d !canBuildSpace=%d !phoenixPath=%d",
               fewWorkers, fewSci, noCC, noSpace, noPhoenix);
  }
  return failed;
}

} // namespace

// ============================================================================================================
// The five required mission-DLL exports + the engine entry points.
// ============================================================================================================
extern "C" __declspec(dllexport) char LevelDesc[]    = "Colony Builder - Eden, Rising From the Ashes";
extern "C" __declspec(dllexport) char MapName[]      = "Ashes.map";
extern "C" __declspec(dllexport) char TechtreeName[] = "multitekNoLavaWalls.txt";
// Colony game, 2 players (1 human + 1 scripted AI), all techs (12), not a unit-only mission.
extern "C" __declspec(dllexport) mission::ModDesc   DescBlock   = { mission::MissionType::Colony, 2, 12, 0 };
extern "C" __declspec(dllexport) mission::ModDescEx DescBlockEx = { 0, 0, 0, 0, 0, 0, 0, 0 };

// ---- one-time setup (the original's InitProc) ----
static void initProc() {
  log::stamp("Rising From The Ashes - mission START");   // wall-clock startup time
  log::line("Rising From The Ashes: InitProc");

  // Factions. (Original also shows a Win32/odasl briefing dialog here - omitted; we surface the orders as a
  // Comms-log message instead.)
  kHuman.goEden();
  kAI.goPlymouth().goAI();
  ignore(Game::forceMoraleGood(1));                 // the AI colony starts content

  // Beacons, then both bases (the bases build mines on the beacons).
  createMineBeacons();
  buildPlayerBase();
  buildAIBase();
  setupBaseDefense();
  // AI-action channel header (cRisingAshes-AI.log) - the Plymouth AI's moves are tracked here, not the main log.
  log::ai("==== AI action log - Rising From The Ashes (Plymouth AI) ====");
  log::ailinef("AI base built in the south; passive until provoked. Groups: base-defense FightGroup, 4 MiningGroups,"
               " colony-expansion + reinforce-VF + repair BuildingGroups.");

  // The recurring raid + special-strike fight groups exist from the start (empty until they spawn waves).
  g.periodicFightGroup = createFightGroup(kAI);
  g.specialFightGroup  = createFightGroup(kAI);

  // Lighting: daylight off, rotating, random initial position. (Original: fumarole vents placed here - omitted.)
  ignore(Game::setDaylightEverywhere(false));
  ignore(Game::setDaylightMoves(true));
  GameMap::setInitialLightLevel(Game::getRand(64));

  // Victory objectives, created here in InitProc exactly as the original SDK mission does (CreateCountTrigger per
  // starship module type). We do NOT touch PlayerImpl+8 - the engine owns the launch-flag bitfield; writing it
  // ourselves was what made the engine recompute it to garbage. Defeat is checked per-cycle in AIProc.
  initVictoryConditions();

  // Schedule the recurring disaster cycle (enabled now) and Mount Swigert's eruption (a single late set piece).
  // Random-interval cycles below are driven from AIProc; here we just seed the first fire ticks.
  {
    int dMin = 2500, dMax = 6500;                   // disaster interval
    if (isNormal()) { dMin = 2000; dMax = 6000; }
    else if (isHard()) { dMin = 1500; dMax = 5500; }
    g.nextDisasterTick = dMin + Game::getRand(dMax - dMin);
  }
  {
    int marks = 1350;                               // eruption schedule (later on easier difficulties)
    if (isEasy()) marks += 250; else if (isNormal()) marks += 125;
    g.eruptionTick = marks * 100;
  }
  // The periodic raid interval is computed once and used each time the raids fire (they stay OFF until provoked).
  // (Stored implicitly via raidInterval() below.)

  Game::addMessage("Build the evacuation starship before our resources run out. Avoid provoking Plymouth if you can.");
  log::linef("  bases built; eruption at tick %d; difficulty=%d", g.eruptionTick, int(diff()));
}

// The random interval between periodic raids (a fresh roll each fire), per difficulty.
static int raidInterval() {
  int lo = 5000, hi = 8000;
  if (isNormal()) { lo = 3000; hi = 5500; }
  else if (isHard()) { lo = 2000; hi = 4000; }
  return lo + Game::getRand(hi - lo);
}
static int disasterInterval() {
  int lo = 2500, hi = 6500;
  if (isNormal()) { lo = 2000; hi = 6000; }
  else if (isHard()) { lo = 1500; hi = 5500; }
  return lo + Game::getRand(hi - lo);
}

// ---- per-cycle logic (the original's AIProc) ----
static void aiProc() {
  const int now = Game::tick();

  // TEMP probe: P0 satellite counts (GetSatelliteCount reads +8). 0 = clean; 15 = garbage (objectives would be
  // spuriously green). Confirms setTechLevel(1) keeps +8 clean. First few cycles only; remove once verified.
  { static int n = 0; if (n < 6) { ++n;
      log::linef("  [sat t%d] P0 rlv=%d solar=%d edward=%d", now,
                 kHuman.satelliteCount(0x6a), kHuman.satelliteCount(0x59), kHuman.satelliteCount(0x58)); } }

  // Stealth (lights-off) raids light up once they take fire.
  if (g.periodicFightGroup.valid() && g.periodicFightGroup.isUnderAttack())
    g.periodicFightGroup.setLights(true);

  // --- provocation: attacking Plymouth's southern outpost opens the war ---
  // If the AI's rare mine OR command center OR structure factory is gone, arm the rare-mine strike (which, when
  // it fires, also switches ON the recurring raids).
  if (!g.rareMineFired && g.rareMineStrikeTick < 0) {
    const bool provoked = !Game::playerOwnsAny(1, abi::MapID::RareOreMine)
                       || !Game::playerOwnsAny(1, abi::MapID::CommandCenter)
                       || !Game::playerOwnsAny(1, abi::MapID::StructureFactory);
    if (provoked) {
      g.rareMineStrikeTick = now + 1000 + Game::getRand(1200);
      log::ailinef("PROVOKED: player struck the southern outpost - rare-mine strike armed for tick %d", g.rareMineStrikeTick);
    }
  }
  if (g.rareMineStrikeTick >= 0 && !g.rareMineFired && now >= g.rareMineStrikeTick) {
    log::ai("STRIKE: rare-mine (8 Lynx/RPG)");
    waves::rareMineStrike();
    g.rareMineFired = true;
    g.nextRaidTick = now + raidInterval();          // <-- the war is now open: periodic raids begin
    log::ai("WAR OPENED: periodic raids now ON");
  }

  // --- milestone: Spaceport built (or, on Hard, time pressure past mark 1150) -> heavier (Middle) waves ---
  if (!g.spaceportFired && g.spaceportStrikeTick < 0) {
    const bool built = Game::playerOwnsAny(0, abi::MapID::Spaceport);
    if (built || (isHard() && Game::mark() > 1150)) g.spaceportStrikeTick = now + 1000 + Game::getRand(800);
  }
  if (g.spaceportStrikeTick >= 0 && !g.spaceportFired && now >= g.spaceportStrikeTick) {
    log::ai("STRIKE: spaceport (10 Panthers)");
    waves::spaceportStrike();
    g.spaceportFired = true;                         // periodic raids advance to the Middle tier
    log::ai("ESCALATION: periodic raids -> Middle tier");
  }

  // --- milestone: Phoenix Module researched -> the heaviest (End) waves ---
  if (!g.phoenixFired && g.phoenixStrikeTick < 0 && kHuman.hasTechnology(kPhoenixModuleTech))
    g.phoenixStrikeTick = now + 1000 + Game::getRand(1200);
  if (g.phoenixStrikeTick >= 0 && !g.phoenixFired && now >= g.phoenixStrikeTick) {
    log::ai("STRIKE: phoenix-module (14 Tigers)");
    waves::phoenixStrike();
    g.phoenixFired = true;                           // periodic raids advance to the End tier
    log::ai("ESCALATION: periodic raids -> End tier");
  }

  // --- recurring raids (once the war is open) ---
  if (g.nextRaidTick >= 0 && now >= g.nextRaidTick) {
    waves::randomWave();
    g.nextRaidTick = now + raidInterval();
  }

  // --- recurring disasters ---
  if (now >= g.nextDisasterTick) {
    std::vector<Location> critical;
    for (abi::MapID t : { abi::MapID::CommandCenter, abi::MapID::StructureFactory, abi::MapID::CommonOreSmelter })
      for (Unit b : Game::unitsOfType(t)) if (b.ownerId() == 0 && b.isLive()) critical.push_back(b.location());
    for (abi::MapID t : { abi::MapID::CommandCenter, abi::MapID::StructureFactory })
      for (Unit b : Game::unitsOfType(t)) if (b.ownerId() == 1 && b.isLive()) critical.push_back(b.location());
    rollDisaster(critical);
    g.nextDisasterTick = now + disasterInterval();
  }

  // --- Mount Swigert erupts (one-shot) ---
  if (!g.erupted && now >= g.eruptionTick) {
    ignore(Game::createEruption(kVolcanoEruption, kEruptionStrength, /*now=*/true));
    g.erupted = true;
    Game::addMessage("Mount Swigert is erupting!");
    log::line("  MOUNT SWIGERT ERUPTS");
  }

  // --- AI reinforcement: if the AI's vehicle economy is wiped out but its CC still stands, drop a recovery team ---
  if (g.reinforceTick < 0
      && g.reinforceVehicleBuildGroup.totalUnitCount() == 0
      && g.repairGroup.totalUnitCount() == 0
      && Game::playerOwnsAny(1, abi::MapID::CommandCenter))
    g.reinforceTick = now + 1000 + Game::getRand(1200);
  if (g.reinforceTick >= 0 && now >= g.reinforceTick) {
    Location s = randSouthernStart();
    if (Unit cv = mk(abi::MapID::ConVec, s, kAI); cv.valid()) { g.aiRepairConvec = cv; g.repairGroup.takeUnit(cv); }
    if (Unit rm = mk(abi::MapID::RoboMiner, { s.x + 1, s.y }, kAI); rm.valid()) g.colonyExpansionBuildGroup.takeUnit(rm);
    std::vector<Unit> lynx; createLine(lynx, kAI, { s.x + 2, s.y }, 1, abi::MapID::Lynx, abi::MapID::Microwave, 3);
    for (Unit l : lynx) g.baseDefenseFightGroup.takeUnit(l);
    g.reinforceTick = -1;                            // one-shot; re-arms only if the AI is crippled again
    log::ailinef("REINFORCEMENT DROP: repair ConVec + RoboMiner + 3 Lynx from south vis(%d,%d)", s.x, s.y);
  }

  // --- AI repair: keep the repair ConVec mending the most-damaged AI building (simple priority sweep) ---
  if (g.aiRepairConvec.isLive() && !g.aiRepairConvec.isBusy()) {
    Unit worst; int worstDmg = 0;
    for (abi::MapID t : { abi::MapID::CommandCenter, abi::MapID::StructureFactory, abi::MapID::VehicleFactory,
                          abi::MapID::Tokamak, abi::MapID::RareOreSmelter, abi::MapID::CommonOreSmelter,
                          abi::MapID::Agridome })
      for (Unit b : Game::unitsOfType(t))
        if (b.ownerId() == 1 && b.isLive() && b.damage() > worstDmg) { worst = b; worstDmg = b.damage(); }
    if (worst.valid()) {
      ignore(g.aiRepairConvec.repair(worst));
      static int s_lastRepairId = -1;   // log only when the repair target CHANGES (avoid per-cycle spam)
      if (worst.id() != s_lastRepairId) {
        s_lastRepairId = worst.id();
        log::ailinef("repair: ConVec -> building type=%d (damage %d) at vis(%d,%d)",
                     int(worst.type()), worstDmg, worst.location().x, worst.location().y);
      }
    }
  }

  // --- retreat AI economy vehicles south if a player combat unit gets close (anti-farming) ---
  int southernCombatantY = 0;
  for (Unit u : Game::unitsOf(0)) if (u.isOffensive() && u.location().y > southernCombatantY) southernCombatantY = u.location().y;
  int retreated = 0;
  for (Unit u : Game::unitsOf(1)) {
    const abi::MapID t = u.type();
    const bool economy = (t == abi::MapID::ConVec && u.cargo() != int(abi::MapID::GuardPost))
                       || t == abi::MapID::RoboMiner || t == abi::MapID::CargoTruck;
    if (economy && u.location().y < southernCombatantY + 8) { ignore(u.move({ 33, 210 })); ++retreated; }
  }
  if (retreated) log::ailinef("retreat: %d economy vehicle(s) pulled south (player combat near y=%d)", retreated, southernCombatantY);

  // --- periodic AI status snapshot (every 10 marks) -> AI log, for tracking the AI economy/forces over time ---
  static int s_lastStatusMark = -1;
  if (const int m = Game::mark(); m != s_lastStatusMark && (m % 10 == 0)) {
    s_lastStatusMark = m;
    int aiB = 0, aiV = 0;
    for (Unit u : Game::unitsOf(1)) { if (u.isBuilding()) ++aiB; else if (u.isVehicle()) ++aiV; }
    log::ailinef("[mark %d] AI: buildings=%d vehicles=%d | groups: defense=%d periodic=%d special=%d repair=%d reinforceVF=%d",
                 m, aiB, aiV, g.baseDefenseFightGroup.totalUnitCount(), g.periodicFightGroup.totalUnitCount(),
                 g.specialFightGroup.totalUnitCount(), g.repairGroup.totalUnitCount(), g.reinforceVehicleBuildGroup.totalUnitCount());
  }

  // --- defeat: a non-viable colony that persists kFailureGraceTicks ticks loses ---
  if (colonyNonViable()) {
    if (g.failSinceTick < 0) g.failSinceTick = now;
    else if (!g.lost && now - g.failSinceTick >= kFailureGraceTicks) { g.lost = true; lose(); log::line("  DEFEAT: colony non-viable"); }
  } else {
    g.failSinceTick = -1;                            // recovered in time
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
    log::stamp("Rising From The Ashes - mission END");   // wall-clock end time on mission unload
  }
  return 1;
}
