// mission.cpp - TitanAPI demo mission, built on the Layer 2 FACADE (op2::Game / op2::Player / op2::Unit).
//
// Same colony as the Layer-1 sample, but expressed through the ergonomic facade: Result-returning setup,
// value-handle Player/Unit, and in-game (visible) coordinates. The Layer-1-only version (raw op2::abi) is
// frozen at ../../samples/Layer1 for comparison.
//
// Status: builds on the facade; the underlying engine calls are the same ones verified in-game in the
// Layer-1 sample (CreateUnit @0x478780, PlayerImpl writes, GoHuman @0x4906C0, SetTechLevel @0x473030).

#include "op2.hpp"            // the TitanAPI Layer 2 facade: Result / Error / Location / Unit / Player / Game / MapID
#include "op2/trigger.hpp"    // Module 4: triggers + callback registry (defines exported stubs -> one TU only)
#include "op2_mission.hpp"    // the mission-DLL contract (ModDesc / ModDescEx / SaveRegion / MissionType)
#include "op2_log.hpp"        // append-only debug log -> <OP2>\OPU\logs\cTitanAPI.log
#include "op2_crash.hpp"      // SEH guards + unhandled-exception filter -> fault written to the log
#include <cstdio>             // std::snprintf (self-test summary messages)
#include <cstring>            // std::memcpy / std::memcmp (PlayerImpl byte-for-byte integrity checks)

using namespace op2::mission;
using op2::MapID;

// =====================================================================================================================
// Mission metadata - the exact exports Outpost 2 / op2ext read to list and configure the mission.
// MapName / TechtreeName must exist in your install ("on6_01.map" + "MULTITEK.TXT" is the stock Colony combo).
// =====================================================================================================================
extern "C" __declspec(dllexport) char      LevelDesc[]    = "TitanAPI Test Mission";
extern "C" __declspec(dllexport) char      MapName[]      = "cm01.map";
extern "C" __declspec(dllexport) char      TechtreeName[] = "MULTITEK.TXT";
extern "C" __declspec(dllexport) ModDesc   DescBlock      = { MissionType::Colony, /*numPlayers*/ 2, /*maxTechLevel*/ 12, /*unitMission*/ 0 };
extern "C" __declspec(dllexport) ModDescEx DescBlockEx    = { /*numMultiplayerAIs*/ 0, 0, 0, 0, 0, 0, 0, 0 };

/// Log a Result<void> setup step (and any error message). Demonstrates the facade's error-as-value API.
static void step(const op2::Result<void>& r, const char* what) {
  if (r) op2::log::linef("  %s: ok", what);
  else   op2::log::linef("  ! %s FAILED: %s", what, r.error().what);
}

// Units we keep around to demonstrate the order API from the game loop (see AIProc): one per player, so we
// prove orders dispatch through the correct owning player.
static op2::Unit g_scout;        // player 0 (human)
static op2::Unit g_aiLynx;       // player 1 (AI)
static op2::Group g_aiFightGroup;// Module 5: AI combat group (commands the AI Lynx to hunt the human)
static op2::Unit g_buildConvec;  // player 0 ConVec carrying an Agridome kit (build demo)
static op2::Unit g_mine;         // player 0 Common Ore Mine (mining-route demo)
static op2::Unit g_smelter;      // player 0 Common Ore Smelter
static op2::Unit g_truck1;       // player 0 Cargo Truck
static op2::Unit g_truck2;       // player 0 Cargo Truck
static op2::Unit g_enemyLynx;    // player 1 enemy laser Lynx @73,88 (target of the EMP-capture demo)
static op2::Unit g_empLynx;      // player 0 EMP Lynx (EMPs the enemy laser Lynx)
static op2::Unit g_empLynx2;     // player 0 second EMP Lynx
static op2::Unit g_reprogSpider; // player 0 Spider (reprograms the EMP'd enemy Lynx -> take ownership)
static op2::Unit g_redScout;     // player 1 (red) Scout - patrols 58,67 <-> 68,67
static op2::Unit g_orgBeacon;    // player 0 ORGANIC mining: the (initially unsurveyed) ore beacon
static op2::Unit g_surveyor;     // player 0 Robo-Surveyor (surveys the beacon by reaching it)
static op2::Unit g_roboMiner;    // player 0 Robo-Miner (builds the mine on the surveyed beacon)
static op2::Unit g_orgSmelter;   // player 0 smelter for the organic mine
static op2::Unit g_orgTruck1;    // player 0 Cargo Truck for the organic mine
static op2::Unit g_orgTruck2;    // player 0 Cargo Truck for the organic mine
static op2::Unit g_earthworker;  // player 0 Earthworker (builds a tube at mark 1)

// Module 7 - mission lifecycle / save state. This whole struct is registered with GetSaveRegions, so OP2 saves
// & restores it across a save/load. On a fresh start InitProc runs and stamps `magic`; on loading a saved game
// InitProc does NOT run, but this struct comes back with the saved values - so `aiProcTicks` keeps counting from
// where the save was taken (the proof that save state persists). `kMagic` distinguishes a real restore from a
// zero-initialized struct.
struct MissionState {
  unsigned magic;        // kMagic once InitProc has stamped it
  int      initProcRuns; // how many times InitProc ran (1 on a fresh start; stays 1 across reloads)
  int      aiProcTicks;  // counts every AIProc call - persists across save/load
};
static constexpr unsigned kMissionMagic = 0x71700007u;   // 'Titan' mission-state marker
static MissionState g_state{};                            // zero-initialized; lives for the DLL's lifetime

// =====================================================================================================================
// API SELF-TEST - exercises every TitanAPI facade function in-game and logs [PASS]/[FAIL] + a summary.
// Run once from the live game loop (AIProc). For orders, PASS = the facade built the packet and the engine
// accepted it (orders apply asynchronously, so this is an "accepted" check, not "completed"). For state
// reads, PASS = the value is sane. Creates its own unit roster in a test area so it is self-contained.
// =====================================================================================================================
namespace selftest {

static int g_pass = 0;
static int g_fail = 0;

// State the self-test (tick 3) sets up for the later OUTCOME phase (outcomes(), at kOutcomeTick = 2500) to verify.
static op2::Trigger g_tickTrigger;     // one-shot onTick(50) trigger
static int          g_tickCallbacks = 0;   // how many times its C++ callback ran
static int          g_redMinX = 9999, g_redMaxX = -1;   // red scout's x range while patrolling (AIProc tracks it)
static op2::Unit    g_dumpTruck;   // given cargo, then dumpCargo() -> must end up empty
static op2::Unit    g_standLynx;   // standGround({84,90}) -> must end up there
static op2::Unit    g_killMe;      // kill() -> must be dead
static op2::Unit    g_wallWorker;  // buildWall(Wall, {86,92}) -> a wall must stand there
static op2::Unit    g_idled;       // a Tokamak idle()d -> isIdled()
static op2::Unit    g_attackTarget0;   // enemy Scout a player-0 FightGroup doAttackUnit()s -> must be dead
static op2::Unit    g_attackTarget1;   // player-0 Scout an AI FightGroup doAttackUnit(target)s -> must be dead

/// Record a boolean check.
static void chk(bool cond, const char* name) {
  ++(cond ? g_pass : g_fail);
  op2::log::linef("  [%s] %s", cond ? "PASS" : "FAIL", name);
}

/// Record a Result<void> check (logs the error text on failure).
static void chkR(const op2::Result<void>& r, const char* name) {
  if (r) { ++g_pass; op2::log::linef("  [PASS] %s", name); }
  else   { ++g_fail; op2::log::linef("  [FAIL] %s: %s", name, r.error().what); }
}

/// Record an EXPECTED-FAILURE check: PASS when `r` is an error of exactly `want` (verifies the error-as-value
/// path - the class of bug this facade was built to prevent). Works for Result<void> and Result<Unit>.
template <class T>
static void chkErr(const op2::Result<T>& r, op2::Status want, const char* name) {
  const bool good = !r.has_value() && (r.error().status == want);
  ++(good ? g_pass : g_fail);
  op2::log::linef("  [%s] %s", good ? "PASS" : "FAIL", name);
}

/// Create a unit, record the creation as a check, and return the handle (null Unit on failure).
static op2::Unit mk(const char* name, op2::MapID type, op2::Location at, op2::Player owner,
                    op2::MapID weaponCargo = op2::MapID::None) {
  const op2::Result<op2::Unit> r = op2::Game::createUnit(type, at, owner, weaponCargo);
  chk(bool(r) && r->valid(), name);
  return r ? *r : op2::Unit{};
}

static void run() {
  using op2::Location;
  g_pass = 0;
  g_fail = 0;
  op2::log::line("==================== API SELF-TEST ====================");
  op2::Player p0 = op2::Game::player(0);
  op2::Player p1 = op2::Game::player(1);

  // ---- core/Location: pure coordinate transforms ----
  const Location L{ 55, 84 };                                   // engine = (55+31, 84-1) = (86, 83)
  chk(L.engineX() == 86 && L.engineY() == 83,                       "Location::engineX/engineY");
  chk(L.onMap() && !Location{ -1, 0 }.onMap(),                      "Location::onMap");
  chk(L.enginePixelX() == 86 * 32 + 16 && L.enginePixelY() == 83 * 32 + 16, "Location::enginePixelX/Y");
  const auto wb = L.waypointBits();
  chk((wb & 0x7FFFu) == unsigned(86 * 32 + 16) && ((wb >> 15) & 0x3FFFu) == unsigned(83 * 32 + 16),
                                                                    "Location::waypointBits");

  // ---- Player: faction toggles + setters (now fluent/chainable; each verified by reading the value back) ----
  p0.goEden();                       chk(p0.isEden(),     "Player::goEden");
  p0.goPlymouth();                   chk(p0.isPlymouth(), "Player::goPlymouth (restore)");
  p0.goAI();                         chk(p0.isAI(),       "Player::goAI");
  p0.goHuman();                      chk(p0.isHuman(),    "Player::goHuman (restore)");
  p0.setPopulation(20, 10, 10);      chk(p0.workers() == 20 && p0.scientists() == 10 && p0.kids() == 10, "Player::setPopulation");
  p0.setFood(5000, 10000);           chk(p0.food() == 5000,      "Player::setFood");
  p0.setCommonOre(5000, 10000);      chk(p0.commonOre() == 5000, "Player::setCommonOre");
  // setTechLevel grants techs up to L12; verify the tech read path round-trips (markResearchComplete -> hasTechnology).
  p0.setTechLevel(12); p0.markResearchComplete(2101); chk(p0.hasTechnology(2101), "Player::setTechLevel + hasTechnology");

  // ---- Game: world creators ----
  { auto beacon = op2::Game::createMine({ 60, 100 });
    chk(bool(beacon) && beacon->valid(), "Game::createMine");
    if (beacon) { chkR(beacon->survey(0), "Unit::survey"); chk(beacon->isSurveyed(0), "Unit::isSurveyed"); } }
  // (Game::createTube / createTubeLine are exercised by the colony tube line in InitProc - no stray tubes here.)

  // ---- triggers (Module 4): factories return valid handles; enable/disable round-trips ----
  chk(op2::onMark(999, [] {}).valid(), "onMark creates a trigger");
  chk(op2::onTick(99999, [] {}).valid(), "onTick creates a trigger");
  chk(op2::onBuildingCount(0, op2::Compare::GreaterEqual, 0).valid(), "onBuildingCount creates a trigger");
  chk(op2::onVehicleCount(0, op2::Compare::GreaterEqual, 0).valid(),  "onVehicleCount creates a trigger");
  chk(op2::onUnitCount(0, MapID::Scout, MapID::Any, op2::Compare::GreaterEqual, 0).valid(),
      "onUnitCount creates a trigger");
  chk(op2::onResearch(2101).valid(),                                                "onResearch creates a trigger");
  chk(op2::onResource(op2::Resource::CommonOre, op2::Compare::GreaterEqual, 1000).valid(),
      "onResource creates a trigger");
  // NOTE: onOperational is intentionally NOT exercised here - creating a CreateOperationalTrigger can flip the
  // game into "Last One Standing" victory mode, which then eliminates any player without an operational CC (the
  // AI's CC is unpowered), ending the mission instantly. It's a real factory, just one with a global side
  // effect, so it's unsafe to fire blindly in a self-test.
  // (victoryWhen/defeatWhen are exercised for real in InitProc - not here, so the self-test leaves no stray
  // objective in the list.)
  { op2::Trigger t = op2::onBuildingCount(0, op2::Compare::GreaterEqual, 0);
    const bool wasOn = t.isEnabled();
    t.disable(); const bool offNow = !t.isEnabled();
    t.enable();  const bool onAgain = t.isEnabled();
    chk(wasOn && offNow && onAgain, "Trigger enable/disable/isEnabled"); }

  // ---- world globals (Module 6): non-destructive reads only - real disasters are demoed via the mark-8 meteor ----
  { const int r = op2::Game::getRand(100); chk(r >= 0 && r < 100, "Game::getRand in [0,range)"); }
  chk(op2::Game::numPlayers() >= 1, "Game::numPlayers() >= 1");

  // Regression: a combat vehicle requested with NO weapon must be auto-armed (a weaponless Lynx crashes OP2).
  chk(op2::Game::createUnit(MapID::Lynx, { 47, 98 }, op2::Game::player(0)).has_value(),
      "createUnit(Lynx, no weapon) auto-arms (no crash)");

  // GameMap (Module 6): a terrain read - also exercises passing a Location struct by value to a __fastcall.
  chk(op2::GameMap::getTile({ 50, 80 }) >= 0, "GameMap::getTile");
  chk(op2::Game::createWreckage({ 52, 78 }, 8000).has_value(), "Game::createWreckage");
  // Tile-property reads (offsets pinned by probe). cellType is a 5-bit value (0..31); dims are positive; and
  // reading the player CC's own tile (48,80) must report a wall/building - validates coords + bit extraction.
  chk(int(op2::GameMap::getCellType({ 50, 80 })) >= 0 && int(op2::GameMap::getCellType({ 50, 80 })) < 32,
      "GameMap::getCellType in range");
  chk(op2::GameMap::getWidth() > 0 && op2::GameMap::getHeight() > 0, "GameMap::getWidth/getHeight > 0");
  chk(op2::GameMap::getWallOrBuilding({ 48, 80 }), "GameMap::getWallOrBuilding (CC tile = true)");

  // ---- unit state (Module 2 finish): new readers on a known combat unit ----
  chk(g_aiLynx.weapon() == MapID::Laser, "Unit::weapon (AI Lynx == Laser)");
  { (void)g_aiLynx.isBusy(); (void)g_aiLynx.isLightsOn(); (void)g_aiLynx.action();
    chk(true, "Unit isBusy/isLightsOn/action read (no crash)"); }

  // ---- AI groups (Module 5): create a FightGroup - validates the sret + _Player-by-value factory ABI ----
  { op2::Group g = op2::createFightGroup(op2::Game::player(1));
    chk(g.valid(), "createFightGroup returns a valid group");
    chk(g.totalUnitCount() == 0, "FightGroup::totalUnitCount (empty)");
    // FightGroup strategy setters on an empty group - exercise the ABI (no units -> no behavior change).
    g.setIdleRect({ 60, 66 }, { 66, 72 });  g.setFollowMode(0);  g.clearGuardedRects();
    g.setTargCount(MapID::Lynx, MapID::Laser, 0);  g.clearTargCount();
    chk(true, "FightGroup setIdleRect/setFollowMode/setTargCount (no crash)");
    g.setPatrolMode({ { 60, 67 }, { 64, 67 }, { 64, 70 }, { 60, 70 } });  g.clearPatrolMode();
    chk(true, "FightGroup setPatrolMode/clearPatrolMode (no crash)"); }
  // ---- the other group types + Pinwheel (factory ABI + a few ops; all on empty/throwaway groups) ----
  { op2::Group mg = op2::createMiningGroup(op2::Game::player(1));
    chk(mg.valid(), "createMiningGroup returns a valid group"); }
  { op2::Group bg = op2::createBuildingGroup(op2::Game::player(1));
    chk(bg.valid(),                  "createBuildingGroup returns a valid group");
    bg.setBuildRect({ 60, 64 }, { 70, 74 });
    bg.recordBuilding({ 62, 66 }, MapID::Agridome);     // record-only; group has no ConVecs, so nothing builds
    chk(true,                        "BuildingGroup setBuildRect/recordBuilding (no crash)"); }
  { op2::Pinwheel pw = op2::createPinwheel(op2::Game::player(1));
    chk(pw.valid(),                  "createPinwheel returns a valid Pinwheel (sret + _Player by-ref)");
    // CONFIGURE the waves (routes + composition) - now safe to do (v0.5.40). We still don't ARM the timer here
    // (a live wave would spawn AI attackers mid-self-test), but defining points+comp exercises the new ABI and
    // is exactly what makes a Pinwheel safe to arm in a real mission.
    pw.setPoints({ { { 62, 66 }, { 58, 78 }, { 54, 86 }, { 50, 90 }, 0, 0, 0 } });
    pw.setAttackComp(2, 4, { { MapID::Lynx, MapID::Laser }, { MapID::Panther, MapID::Microwave } });
    pw.setAttackFraction(50);  pw.setContactDelay(20);
    chk(true,                        "Pinwheel setPoints/setAttackComp/fraction/contactDelay (no crash)"); }

  // ---- v0.5.40: UnitBlock (batch creation + group AddUnits) ----
  { op2::UnitBlock block({
      { MapID::Scout,  { 80, 96 } },
      { MapID::Lynx,   { 80, 97 }, MapID::Laser },
      { MapID::Lynx,   { 81, 97 }, MapID::Microwave },
    });
    const int made = block.createUnits(op2::Game::player(0));
    chk(made >= 1,                   "UnitBlock::createUnits placed units");
    op2::Group fg = op2::createFightGroup(op2::Game::player(0));
    op2::UnitBlock reinforce({ { MapID::Lynx, { 82, 96 }, MapID::Laser } });
    fg.addUnits(reinforce);
    chk(fg.totalUnitCount() >= 1,    "Group::addUnits(UnitBlock) populated the group"); }

  // ---- Player reads (OP2Lua parity) - validates the new PlayerImpl offsets (rareOre@36, isHuman@40) ----
  { op2::Player p0 = op2::Game::player(0), p1 = op2::Game::player(1);
    chk(p0.commonOre() >= 0 && p0.population() >= 0, "Player::commonOre/population read");
    chk(p0.isHuman() && !p0.isAI(),  "Player::isHuman (player 0 = human)");
    chk(p1.isAI()    && p1.isEden(), "Player::isAI/isEden (player 1 = AI Eden)"); }

  // ---- more Unit state reads (OP2Lua parity) ----
  chk(g_aiLynx.isOffensive() && !g_aiLynx.isFactory(), "Unit::isOffensive/isFactory (Lynx)");
  chk(op2::Game::numHumans() >= 1, "Game::numHumans() >= 1");
  // ---- Unit health (maxHP from the stats table) ----
  chk(g_aiLynx.maxHitpoints() > 0, "Unit::maxHitpoints > 0");
  chk(g_aiLynx.health() > 0.0 && g_aiLynx.health() <= 1.0, "Unit::health in (0,1]");
  // ---- player-scoped enumeration (OP2Lua parity) ----
  chk(op2::Game::playerOwnsAny(1, MapID::CommandCenter), "Game::playerOwnsAny (AI owns a CC)");
  chk(op2::Game::playerUnitCount(0, MapID::Any) > 0,      "Game::playerUnitCount(0, Any) > 0");
  chk(op2::Game::footprint(MapID::CommandCenter).width > 0, "Game::footprint(CommandCenter).width > 0");
  { const auto pl = op2::Game::findPlace(MapID::ConVec, { 52, 80 });
    chk(pl.has_value(), "Game::findPlace(ConVec near base) found a spot"); }
  { op2::Region r{ { 48, 78 }, { 58, 88 } };
    chk(r.contains({ 50, 80 }) && !r.contains({ 10, 10 }), "Region::contains");
    chk(r.center().x == 53 && r.center().y == 83,          "Region::center"); }

  // ---- Game::createUnit: the test roster (each creation is a check) ----
  op2::Unit lynx    = mk("createUnit(Lynx)",             op2::MapID::Lynx,             { 58, 88 }, p0, op2::MapID::Microwave);
  op2::Unit spider  = mk("createUnit(Spider)",           op2::MapID::Spider,           { 59, 88 }, p0);
  op2::Unit repairV = mk("createUnit(RepairVehicle)",    op2::MapID::RepairVehicle,    { 60, 88 }, p0);
  op2::Unit convec  = mk("createUnit(ConVec+Agridome)",  op2::MapID::ConVec,           { 63, 88 }, p0, op2::MapID::Agridome);
  op2::Unit truck   = mk("createUnit(CargoTruck)",       op2::MapID::CargoTruck,       { 65, 88 }, p0);
  op2::Unit factory = mk("createUnit(VehicleFactory)",   op2::MapID::VehicleFactory,   { 58, 98 }, p0);
  op2::Unit garage  = mk("createUnit(Garage)",           op2::MapID::Garage,           { 63, 98 }, p0);
  op2::Unit smelter = mk("createUnit(CommonOreSmelter)", op2::MapID::CommonOreSmelter, { 67, 98 }, p0);
  op2::Unit throwB  = mk("createUnit(Agridome throwaway)", op2::MapID::Agridome,       { 71, 98 }, p0);
  op2::Unit gift    = mk("createUnit(Scout for transfer)", op2::MapID::Scout,          { 67, 88 }, p0);
  op2::Unit doomed  = mk("createUnit(Scout for selfDestruct)", op2::MapID::Scout,      { 69, 88 }, p0);
  op2::Unit enemy   = mk("createUnit(enemy Lynx, p1)",   op2::MapID::Lynx,             { 56, 90 }, p1, op2::MapID::Laser);
  op2::Unit convecNK= mk("createUnit(ConVec, no kit)",   op2::MapID::ConVec,           { 62, 88 }, p0);

  // ---- Unit: state reads ----
  chk(lynx.valid() && lynx.id() > 0,                "Unit::id/valid");
  chk(lynx.isLive(),                                "Unit::isLive");
  chk(lynx.ownerId() == 0,                          "Unit::ownerId");
  chk(int(lynx.type()) == int(op2::MapID::Lynx),    "Unit::type");
  chk(lynx.damage() == 0,                           "Unit::damage");
  const Location ll = lynx.location();
  chk(ll.x >= 0 && ll.y >= 0,                        "Unit::location");
  chk(convec.cargo() == int(op2::MapID::Agridome),  "Unit::cargo (ConVec kit)");
  chk(lynx.isVehicle()  && !factory.isVehicle(),    "Unit::isVehicle");
  chk(factory.isBuilding() && !lynx.isBuilding(),   "Unit::isBuilding");
  chk(!lynx.isEMPed(),                              "Unit::isEMPed (not EMP'd)");
  // GameMap::unitOnTile reads the tile's per-cell unit index: the smelter built at {49,88} is reported there.
  chk(op2::GameMap::unitOnTile({ 49, 88 }).id() == g_smelter.id(), "GameMap::unitOnTile");

  // ---- Unit: orders (PASS = built + accepted) ----
  chkR(lynx.move({ 59, 90 }),             "Unit::move");
  chkR(lynx.attack(enemy),                "Unit::attack(Unit)");
  chkR(lynx.attack(Location{ 66, 90 }),   "Unit::attack(Location)");
  chkR(lynx.guard(smelter),               "Unit::guard");
  chkR(lynx.setLights(true),              "Unit::setLights");
  chkR(lynx.stop(),                       "Unit::stop");
  chkR(lynx.patrol({ 58, 86 }, { 62, 86 }), "Unit::patrol");
  chkR(spider.reprogram(enemy),           "Unit::reprogram");   // reprogram targets an ENEMY vehicle (correct)
  chkR(repairV.repair(lynx),              "Unit::repair");      // repair targets a FRIENDLY unit (correct)
  chkR(convec.build({ 64, 89 }),          "Unit::build");
  chkR(g_earthworker.buildTube({ 61, 98 }), "Unit::buildTube"); // reuse the single demo Earthworker (same tile)
  chkR(convec.dismantle(throwB),          "Unit::dismantle");
  chkR(truck.mine(g_mine, g_smelter),     "Unit::mine");   // route to the real scripted mine (no self-test mine)
  chkR(truck.dock(smelter),               "Unit::dock");
  chkR(truck.dockAtGarage(garage),        "Unit::dockAtGarage");
  chkR(factory.produce(op2::MapID::Scout),"Unit::produce");
  chkR(factory.idle(),                    "Unit::idle");
  chkR(factory.unidle(),                  "Unit::unidle");
  chkR(gift.transfer(1),                  "Unit::transfer");
  chkR(doomed.selfDestruct(),             "Unit::selfDestruct");

  // ---- v0.5.26 OP2Lua-parity additions ----
  chk(!factory.underConstruction(),       "Unit::underConstruction (finished building == false)");
  { const int c = factory.command(); chk(c >= 0, "Unit::command read (no crash)"); }
  { op2::Unit tmp = mk("createUnit(Scout to remove)", op2::MapID::Scout, { 72, 96 }, p0);
    chkR(tmp.remove(),                    "Unit::remove (DoPoof)"); }                  // vanish, no explosion
  { auto b = op2::Game::createMine({ 70, 100 });                                       // default = CommonOre
    chk(bool(b) && b->valid(),            "Game::createMine (for oreType)");
    if (b) chk(b->oreType() == 0,         "Unit::oreType (common beacon == 0)"); }
  op2::Game::createBlight({ 73, 100 });  op2::Game::unsetBlight({ 73, 100 });
  chk(true,                               "Game::createBlight/unsetBlight (no crash)");
  { op2::Unit mkr = op2::Game::createMarker({ 70, 96 }, op2::Game::Marker::Beaker);
    (void)mkr;  chk(true,                 "Game::createMarker (no crash)"); }

  // ---- v0.5.27: area orders + dock/cargo parity (PASS = packet built + accepted) ----
  { op2::Unit ew = mk("createUnit(Earthworker for removeWall)", op2::MapID::Earthworker, { 71, 96 }, p0);
    chkR(ew.removeWall({ 71, 97 }, { 71, 97 }), "Unit::removeWall"); }
  { op2::Unit dz = mk("createUnit(RoboDozer)", op2::MapID::RoboDozer, { 73, 96 }, p0);
    chkR(dz.doze({ 73, 97 }, { 74, 97 }),       "Unit::doze"); }
  { op2::Unit sv = mk("createUnit(CargoTruck for salvage)", op2::MapID::CargoTruck, { 74, 96 }, p0);
    sv.setCargo(1, 500); chk(sv.truckCargoAmount() == 500, "Unit::setCargo/truckCargoAmount round-trip");
    chkErr(sv.salvage({ 74, 97 }, { 74, 97 }, op2::Unit{}), op2::Status::InvalidTarget, "salvage(null gorf)"); }
  { op2::Unit onDock = factory.unitOnDock(); (void)onDock;   // empty dock -> invalid Unit; just must not crash
    chk(true,                                   "Unit::unitOnDock (no crash)"); }
  chkR(g_aiLynx.attackMove(Location{ 60, 70 }), "Unit::attackMove(Location)");

  // ---- v0.5.28: probe-verified offsets (difficulty / disasters-enabled / immediate disaster) ----
  { const int d = int(p0.difficulty()); chk(d >= 0 && d <= 2, "Player::difficulty in {Easy,Normal,Hard}"); }
  { const bool de = op2::Game::disastersEnabled(); (void)de;  chk(true, "Game::disastersEnabled (no crash)"); }
  chkR(op2::Game::createMeteor({ 88, 30 }, 2, /*now=*/true), "Game::createMeteor(now=true) immediate strike");
  op2::Game::setLavaSpeed(45);  op2::Game::setBlightSpeed(45);   // OP2 "Slow" - global side effect, harmless here
  chk(true,                                     "Game::setLavaSpeed/setBlightSpeed (no crash)");

  // ---- v0.5.30: perception helpers (beacons / find_build_site / can_place) ----
  chk(!op2::GameMap::canPlace({ 48, 80 }),      "GameMap::canPlace (CC tile occupied -> false)");
  { auto site = op2::GameMap::findBuildSite(MapID::Agridome, { 52, 78 });
    chk(site.has_value(),                       "GameMap::findBuildSite (Agridome near base)");
    if (site) chk(op2::GameMap::canPlace(*site), "findBuildSite result tile is itself placeable"); }
  { auto bs = op2::Game::beacons();
    chk(!bs.empty(),                            "Game::beacons() finds mining beacons"); }
  chkErr(op2::GameMap::findBuildSite(MapID::Lynx, { 52, 78 }), op2::Status::WrongType, "findBuildSite(non-building)");

  // ---- accessors & equality ----
  chk(op2::Game::player(0).valid() && !op2::Game::player(99).valid(),         "Player::valid");
  chk(op2::Game::player(3).index() == 3,                                      "Player::index");
  chk(op2::Game::tick() >= 0,                                                 "Game::tick");
  chk(op2::Game::mark() == op2::Game::tick() / 100,                           "Game::mark");

  // ---- enumeration (Module 3): ranges over live units ----
  int allN = 0, p0N = 0, p1N = 0, vehN = 0;
  for (op2::Unit u : op2::Game::units())    { (void)u; ++allN; }
  for (op2::Unit u : op2::Game::unitsOf(0)) { (void)u; ++p0N; }
  for (op2::Unit u : op2::Game::unitsOf(1)) { (void)u; ++p1N; }
  for (op2::Unit u : op2::Game::units() | std::views::filter([](op2::Unit x) { return x.isVehicle(); }))
    { (void)u; ++vehN; }
  chk(allN > 0 && p0N > 0 && p1N > 0 && (p0N + p1N) <= allN, "Game::units / unitsOf enumeration");
  chk(vehN > 0,                                             "Game::units() | views::filter(isVehicle)");
  chk((lynx == lynx) && !(lynx == spider),                                    "Unit::operator==");
  chk((Location{ 5,6 } == Location{ 5,6 }) && !(Location{ 5,6 } == Location{ 6,5 }), "Location::operator==");

  // ---- negative / error-path tests: the facade must return clean Result errors, never crash ----
  chkErr(op2::Unit{}.move({ 1, 1 }),                              op2::Status::NullHandle,      "move() on null Unit");
  chkErr(lynx.attack(op2::Unit{}),                                op2::Status::InvalidTarget,   "attack(null target)");
  op2::Game::player(99).goHuman();  chk(!op2::Game::player(99).isHuman(), "goHuman() on bad player is a safe no-op");
  chkErr(op2::Game::createUnit(op2::MapID::Lynx, { -1, -1 }, p0), op2::Status::InvalidLocation, "createUnit() off-map");
  chkErr(op2::Game::createMine({ -1, -1 }),                       op2::Status::InvalidLocation, "createMine() off-map");
  chkErr(op2::Game::createUnit(op2::MapID::Lynx, { 9999, 9999 }, p0),
         op2::Status::InvalidLocation, "createUnit() beyond map bounds (GameMap::onMap)");
  chkErr(convecNK.build({ 62, 90 }),                             op2::Status::WrongType,       "build() with no kit");
  chkErr(truck.mine(op2::Unit{}, smelter),                       op2::Status::InvalidTarget,   "mine() invalid mine target");

  // ---- v0.5.32: Unit::enabled + Player resource/population setters ----
  chk(!lynx.enabled(),                          "Unit::enabled (vehicle -> false)");
  { const bool e = factory.enabled(); (void)e;  chk(true, "Unit::enabled building read (no crash)"); }
  // Exercise each setter by writing a changed value, verifying the read-back, then restoring (no net change).
  { const int v = p0.rareOre();    p0.setRareOre(v + 7);    chk(p0.rareOre() == v + 7,    "Player::setRareOre");    p0.setRareOre(v); }
  { const int v = p0.workers();    p0.setWorkers(v + 7);    chk(p0.workers() == v + 7,    "Player::setWorkers");    p0.setWorkers(v); }
  { const int v = p0.scientists(); p0.setScientists(v + 7); chk(p0.scientists() == v + 7, "Player::setScientists"); p0.setScientists(v); }
  { const int v = p0.kids();       p0.setKids(v + 7);       chk(p0.kids() == v + 7,       "Player::setKids");       p0.setKids(v); }
  // Player::units(type) - player-scoped enumeration (the last OP2Lua parity item).
  { auto pAll = p0.units(); auto pLynx = p0.units(MapID::Lynx);
    chk(!pAll.empty(),                          "Player::units() non-empty for the live colony");
    chk(pLynx.size() <= pAll.size(),            "Player::units(type) is a subset of units()"); }

  // ---- v0.5.35: Module 7 mission lifecycle / save state ----
  chk(g_state.magic == kMissionMagic,           "MissionState stamped by InitProc (save region)");
  chk(g_state.initProcRuns >= 1,                "MissionState initProcRuns >= 1");
  { op2::mission::Stream ns{ nullptr }; chk(!ns.valid(), "mission::Stream(null) reports invalid"); }

  // ---- v0.5.38: Module 8 BaseBuilder (declarative base layout) ----
  { op2::BaseLayout layout;
    layout.buildings = { { { 76, 96 }, MapID::Agridome } };
    layout.vehicles  = { { { 78, 96 }, MapID::Scout }, { { 78, 97 }, MapID::Lynx, MapID::Laser } };
    layout.tubes     = { { { 76, 96 }, { 78, 96 } } };
    const op2::BaseResult br = op2::createBase(p0, layout);
    chk(br.vehicles == 2,                       "createBase placed the 2 vehicles");
    chk(br.placedUnits() >= 2,                  "createBase placedUnits() >= 2");
    chk(br.tubeLines == 1,                      "createBase laid the tube line"); }

  // ---- v0.6.10: _Player-handle calls must not touch PlayerImpl; invalid handles are safe no-ops ----
  // resetChecks / canBuildSpace / canDoResearch / hasTechnology are _Player (handle) methods that cache results in
  // the handle's 8 check bytes. Before 0.6.10 they ran on PlayerImpl and wrote those bytes over PlayerImpl+4
  // (alliedBy) and +8 (the satellite/launch bitfield), which made starship objectives fire spuriously.
  { char* pi = op2::abi::ref<char*>(0x4890C3);                     // player 0's PlayerImpl (raw, for the check)
    const int ally0 = pi ? *reinterpret_cast<int*>(pi + 4) : 0, sat0 = pi ? *reinterpret_cast<int*>(pi + 8) : 0;
    p0.resetChecks();
    const bool cbs = p0.canBuildSpace(); const bool cdr = p0.canDoResearch(3401); const bool ht = p0.hasTechnology(3401);
    (void)cbs; (void)cdr; (void)ht;
    chk(pi && *reinterpret_cast<int*>(pi + 4) == ally0 && *reinterpret_cast<int*>(pi + 8) == sat0,
        "resetChecks/canBuildSpace/canDoResearch leave PlayerImpl+4/+8 untouched"); }
  // A never-created Group / Trigger (id 255) must not reach the engine: reads return 0/false, orders do nothing.
  { op2::Group none; op2::Trigger tnone;
    none.takeUnit(lynx); none.setLights(true); tnone.enable();
    chk(!none.valid() && none.totalUnitCount() == 0 && !none.isUnderAttack() && !tnone.isEnabled(),
        "invalid Group/Trigger calls are safe no-ops"); }

  // ---- alliances, view and satellite reads (the other _Player-handle / PlayerImpl calls) ----
  // Alliances can't be undone, so ally with slot 2 - unused in this 2-player mission - and leave player 1 (the
  // AI) an enemy, so the combat demos still play out.
  { op2::Player p2 = op2::Game::player(2);
    chk(!p0.isAlly(1),                          "Player::isAlly(1) false at start (the AI is an enemy)");
    p0.mutuallyAllyWith(2);
    chk(p0.isAlly(2) && p2.isAlly(0),           "Player::mutuallyAllyWith(2) -> isAlly both ways");
    chk(!p0.isAlly(1),                          "mutuallyAllyWith(2) leaves player 1 an enemy"); }
  // centerViewOn is a _Player method: it must leave the PlayerImpl record byte-for-byte unchanged.
  { char* pi = op2::abi::ref<char*>(0x4890C3);
    unsigned char before[64] = {};
    if (pi) std::memcpy(before, pi, sizeof(before));
    p0.centerViewOn({ 48, 80 });
    chk(pi && std::memcmp(before, pi, sizeof(before)) == 0, "Player::centerViewOn leaves PlayerImpl untouched"); }
  chk(p0.satelliteCount(int(MapID::SolarSatellite)) == 0 && p0.satelliteCount(int(MapID::EDWARDSatellite)) == 0
      && p0.satelliteCount(int(MapID::EvacuationModule)) == 0,
      "Player::satelliteCount reads 0 before any launch");

  // A one-shot time trigger at tick 50: its callback and hasFired are checked by the outcome phase (outcomes()).
  g_tickTrigger = op2::onTick(50, [] { ++g_tickCallbacks; });
  chk(g_tickTrigger.valid() && g_tickTrigger.isEnabled(), "onTick(50) trigger created and enabled");

  // ---- v0.6.10: the remaining Unit state reads, setters and orders ----
  // Fresh units in their own patch (x 77..88, y 88..98) so nothing above or the AIProc demos is disturbed.
  // Setters and reads are checked by reading the value back now; orders whose effect takes time get an
  // "accepted" check here and an effect check in outcomes().
  { // health: hitpoints / inflictDamage / heal
    op2::Unit u = mk("createUnit(Lynx for health)", MapID::Lynx, { 77, 88 }, p0, MapID::Microwave);
    const int maxHp = u.maxHitpoints();
    chk(maxHp > 0 && u.hitpoints() == maxHp,                     "Unit::hitpoints == maxHitpoints when undamaged");
    u.inflictDamage(10);
    chk(u.damage() == 10 && u.hitpoints() == maxHp - 10,          "Unit::inflictDamage(10)");
    u.heal(4);
    chk(u.damage() == 6,                                          "Unit::heal(4)");
    u.heal(1000);
    chk(u.damage() == 0 && u.hitpoints() == maxHp,                "Unit::heal clamps damage at 0"); }
  { // weapon / kit / truck-cargo setters (all write the unit's cargo word)
    op2::Unit w = mk("createUnit(Lynx for setWeapon)", MapID::Lynx, { 78, 88 }, p0, MapID::Microwave);
    w.setWeapon(MapID::Laser);
    chk(w.weapon() == MapID::Laser,                               "Unit::setWeapon(Laser) -> weapon()");
    op2::Unit k = mk("createUnit(ConVec for setKit)", MapID::ConVec, { 79, 88 }, p0);
    k.setKit(MapID::Tokamak);
    chk(k.cargo() == int(MapID::Tokamak),                         "Unit::setKit(Tokamak) -> cargo()");
    g_dumpTruck = mk("createUnit(CargoTruck for cargo)", MapID::CargoTruck, { 80, 88 }, p0);
    g_dumpTruck.setTruckCargo(2, 300);
    chk(g_dumpTruck.truckCargo() == 2 && g_dumpTruck.truckCargoAmount() == 300, "Unit::setTruckCargo -> truckCargo/Amount");
    chkR(g_dumpTruck.dumpCargo(),                                 "Unit::dumpCargo (accepted; emptied: outcomes)"); }
  { // load / unload: accepted (the truck isn't docked, so the engine just has nothing to move)
    op2::Unit t = mk("createUnit(CargoTruck for load/unload)", MapID::CargoTruck, { 81, 88 }, p0);
    chkR(t.loadCargo(),                                           "Unit::loadCargo (accepted)");
    chkR(t.unloadCargo(),                                         "Unit::unloadCargo (accepted)"); }
  { // factory cargo bays: write bay 5 and read it back, then clear it again
    factory.setFactoryCargo(5, MapID::Lynx, MapID::Laser);
    chk(factory.factoryCargo(5) == MapID::Lynx,                   "Unit::setFactoryCargo -> factoryCargo(5)");
    factory.setFactoryCargo(5, MapID::None);
    chk(factory.factoryCargo(5) == MapID::None && lynx.factoryCargo(0) == MapID::None,
        "Unit::factoryCargo cleared / None on a non-factory"); }
  { // building flags are gated on isBuilding: every one reads false on a vehicle
    chk(!lynx.hasPower() && !lynx.hasWorkers() && !lynx.hasScientists() && !lynx.isIdled() && !lynx.isInfected(),
        "Unit building flags read false on a vehicle");
    chk(!factory.isInfected(),                                    "Unit::isInfected false (no blight here)"); }
  // Orders with a visible effect - checked in outcomes():
  g_standLynx = mk("createUnit(Lynx for standGround)", MapID::Lynx, { 82, 88 }, p0, MapID::Microwave);
  chkR(g_standLynx.standGround({ 84, 90 }),                       "Unit::standGround (accepted; arrival: outcomes)");
  g_killMe = mk("createUnit(Scout for kill)", MapID::Scout, { 83, 88 }, p0);
  g_killMe.kill();                                                // death is checked in outcomes()
  g_wallWorker = mk("createUnit(Earthworker for buildWall)", MapID::Earthworker, { 84, 88 }, p0);
  chkR(g_wallWorker.buildWall(MapID::Wall, { 86, 92 }, { 86, 92 }), "Unit::buildWall (accepted; wall: outcomes)");
  g_idled = mk("createUnit(Tokamak to idle)", MapID::Tokamak, { 79, 96 }, p0);
  chkR(g_idled.idle(),                                            "Unit::idle on a Tokamak (accepted; isIdled: outcomes)");
  // Building-specific orders: accepted checks only. research/trainScientists/launch run on new, unstaffed
  // buildings, so the engine has nothing to do with them; transferCargo is deliberately NOT called - issuing
  // it without a vehicle on the factory dock crashes OP2.
  { op2::Unit lab = mk("createUnit(BasicLab)", MapID::BasicLab, { 83, 96 }, p0);
    chkR(lab.research(3401, 1),                                   "Unit::research (accepted)");
    op2::Unit uni = mk("createUnit(University)", MapID::University, { 87, 96 }, p0);
    chkR(uni.trainScientists(1),                                  "Unit::trainScientists (accepted)");
    op2::Unit sp = mk("createUnit(Spaceport)", MapID::Spaceport, { 83, 101 }, p0);
    chkR(sp.launch({ 90, 40 }),                                   "Unit::launch (accepted, empty pad)"); }

  // ---- v0.6.10: AI groups - roster, targets, objectives and recorded state ----
  // Each group call is checked by reading the engine's own group record back (ScGroupImpl and its subclasses;
  // field offsets from TethysAPI's Game/ScBase.h, offsetof-verified). The group table is at 0x56C35C.
  { auto rec = [](op2::Group g) -> char* {
      char* tbl = reinterpret_cast<char*>(op2::abi::reloc(0x56C35C));
      return g.valid() ? *reinterpret_cast<char**>(tbl + 4 * g.id()) : nullptr; };
    auto field = [&](op2::Group g, int off) { char* p = rec(g); return p ? *reinterpret_cast<int*>(p + off) : -999; };
    constexpr int kCombineFire = 776, kTargetGroup = 852, kTargetUnit = 860, kNumGuardedRects = 864,
                  kObjective = 996;                                  // FightGroupImpl
    constexpr int kMineUnit = 776, kSmelterUnit = 780;               // MineGroupImpl
    constexpr int kRecBuildings = 808, kRecMines = 812, kRecVehGroups = 820;   // BuildingGroupImpl::recordInfo_
    constexpr int kDeleteWhenEmptyTick = 760;                        // ScGroupImpl

    // Roster: takeUnit / removeUnit / takeAllUnits / unitCount.
    op2::Group fg = op2::createFightGroup(p0), fg2 = op2::createFightGroup(p0);
    op2::Unit a = mk("createUnit(Lynx A for group)", MapID::Lynx, { 60, 104 }, p0, MapID::Laser);
    op2::Unit b = mk("createUnit(Lynx B for group)", MapID::Lynx, { 61, 104 }, p0, MapID::Laser);
    op2::Unit c = mk("createUnit(Lynx C for group)", MapID::Lynx, { 62, 104 }, p0, MapID::Laser);
    fg.takeUnit(a); fg.takeUnit(b); fg.takeUnit(c);
    chk(fg.totalUnitCount() == 3,                                 "Group::takeUnit x3 -> totalUnitCount 3");
    chk(fg.unitCount(0) == 3,                                     "Group::unitCount(0 = Attack class) counts the 3 Lynx");
    fg.removeUnit(c);
    chk(fg.totalUnitCount() == 2,                                 "Group::removeUnit");
    fg2.takeUnit(c);
    fg.takeAllUnits(fg2);
    chk(fg.totalUnitCount() == 3 && fg2.totalUnitCount() == 0,    "Group::takeAllUnits moved the whole roster");

    // FightGroup settings.
    fg.setCombineFire();
    chk(field(fg, kCombineFire) != 0,                             "Group::setCombineFire");
    fg.clearCombineFire();
    chk(field(fg, kCombineFire) == 0,                             "Group::clearCombineFire");
    fg.setTargetGroup(fg2);
    chk(field(fg, kTargetGroup) == fg2.id(),                      "Group::setTargetGroup");
    const int rects0 = field(fg, kNumGuardedRects);
    fg.addGuardedRect({ 58, 102 }, { 64, 106 });
    chk(field(fg, kNumGuardedRects) == rects0 + 1,                "Group::addGuardedRect");
    fg.clearGuardedRects();
    chk(field(fg, kNumGuardedRects) == 0,                         "Group::clearGuardedRects");

    // FightGroup objectives - each do*() sets the group's objective (CombatGroupObjective in TethysAPI).
    fg.setIdleRect({ 58, 102 }, { 64, 106 });
    fg.doGuardRect();   const int oRect   = field(fg, kObjective);
    fg.setPatrolMode({ { 58, 104 }, { 64, 104 } });
    fg.doPatrolOnly();  const int oPatrol = field(fg, kObjective);
    fg.doGuardGroup();  const int oGGroup = field(fg, kObjective);
    op2::Unit decoy = mk("createUnit(enemy Scout, guard/attack target)", MapID::Scout, { 66, 104 }, p1);
    fg.setTargetUnit(decoy);
    chk(field(fg, kTargetUnit) == decoy.id(),                     "Group::setTargetUnit");
    fg.doGuardUnit();   const int oGUnit  = field(fg, kObjective);
    fg.doAttackUnit();  const int oAttack = field(fg, kObjective);
    op2::log::linef("    objectives: guardRect=%d patrol=%d guardGroup=%d guardUnit=%d attackUnit=%d",
                    oRect, oPatrol, oGGroup, oGUnit, oAttack);
    chk(oRect == 0 && oPatrol == 1 && oGGroup == 2 && oGUnit == 3 && oAttack == 4,
        "Group::doGuardRect/doPatrolOnly/doGuardGroup/doGuardUnit/doAttackUnit set the objective");
    // The engine's Do*() resets the target unit / group / attack type; TitanAPI's do*() must put them back.
    chk(field(fg, kTargetUnit) == decoy.id() && field(fg, kTargetGroup) == fg2.id(),
        "Group::do*() keep the target unit and target group set before the call");
    fg.doGuardRect();   // park the group in its idle rect again
    { op2::Group ex = op2::createFightGroup(p0);                  // doExitMap on its own (empty) group
      ex.doExitMap();
      chk(field(ex, kObjective) == 6,                             "Group::doExitMap sets the ExitMap objective"); }
    // doAttackUnit for real (outcomes() checks each target is destroyed): a player-0 group using the usual
    // "target, then order" sequence, and an AI-owned group using the one-call doAttackUnit(target). Each gets an
    // idle rect so it returns there after the kill. (Before 0.6.10 the first drove off to (0,0) and the second
    // crashed OP2.)
    { op2::Group atk = op2::createFightGroup(p0);
      for (int x = 60; x <= 62; ++x) atk.takeUnit(mk("createUnit(Lynx, p0 attack group)", MapID::Lynx, { x, 107 }, p0, MapID::Laser));
      g_attackTarget0 = mk("createUnit(enemy Scout, p0 group target)", MapID::Scout, { 66, 107 }, p1);
      atk.setIdleRect({ 58, 106 }, { 64, 108 });
      atk.setTargetUnit(g_attackTarget0);
      atk.doAttackUnit();
      chk(field(atk, kTargetUnit) == g_attackTarget0.id() && field(atk, kObjective) == 4,
          "p0 attack group: target kept + AttackUnit objective"); }
    { op2::Group atk = op2::createFightGroup(p1);
      for (int x = 60; x <= 62; ++x) atk.takeUnit(mk("createUnit(Lynx, AI attack group)", MapID::Lynx, { x, 110 }, p1, MapID::Laser));
      g_attackTarget1 = mk("createUnit(Scout, AI group target)", MapID::Scout, { 66, 110 }, p0);
      atk.setIdleRect({ 58, 109 }, { 64, 111 });
      atk.doAttackUnit(g_attackTarget1);
      chk(field(atk, kTargetUnit) == g_attackTarget1.id() && field(atk, kObjective) == 4,
          "AI attack group: doAttackUnit(target) sets target + objective"); }

    // MiningGroup: setupMining records the mine and smelter (no trucks, so it doesn't take over any routes).
    op2::Group mg = op2::createMiningGroup(p0);
    mg.setupMining(g_mine, g_smelter, { 44, 88 }, { 50, 92 });
    chk(field(mg, kMineUnit) == g_mine.id() && field(mg, kSmelterUnit) == g_smelter.id(),
        "Group::setupMining records the mine and smelter");

    // BuildingGroup: record-only calls bump the group's recorded counts (no ConVecs, so nothing is built).
    op2::Group bg = op2::createBuildingGroup(p0);
    const int bld0 = field(bg, kRecBuildings), mine0 = field(bg, kRecMines), veh0 = field(bg, kRecVehGroups);
    bg.recordBuilding({ 62, 108 }, MapID::Agridome);
    bg.recordMine({ 44, 88 });
    bg.recordVehReinforceGroup(fg, 1);
    op2::log::linef("    building group records: buildings %d->%d mines %d->%d vehGroups %d->%d", bld0,
                    field(bg, kRecBuildings), mine0, field(bg, kRecMines), veh0, field(bg, kRecVehGroups));
    chk(field(bg, kRecBuildings) == bld0 + 1,                     "Group::recordBuilding adds a building record");
    chk(field(bg, kRecMines) == mine0 + 1,                        "Group::recordMine adds a mine record");
    chk(field(bg, kRecVehGroups) == veh0 + 1,                     "Group::recordVehReinforceGroup adds a record");
    bg.unRecordVehGroup(fg);
    chk(field(bg, kRecVehGroups) == veh0,                         "Group::unRecordVehGroup removes it");
    { op2::UnitBlock blk({ { MapID::ConVec, { 62, 110 } } });
      bg.recordUnitBlock(blk);
      chk(bg.valid() && field(bg, kRecBuildings) >= bld0,         "Group::recordUnitBlock leaves the group intact"); }

    // setDeleteWhenEmpty, on its own empty group that is never used again (the engine may delete it).
    { op2::Group del = op2::createFightGroup(p0);
      const int before = field(del, kDeleteWhenEmptyTick);
      del.setDeleteWhenEmpty(true);
      const int after = field(del, kDeleteWhenEmptyTick);
      op2::log::linef("    deleteWhenEmptyTick: %d -> %d", before, after);
      chk(after != before,                                        "Group::setDeleteWhenEmpty updates the group"); } }
  // Pinwheel waves: setGuardComp / setSapperComp are record-only. setWavePeriod / sendWaveNow are deliberately NOT
  // called: they launch real attack waves, which could end this mission (destroy a Command Center) before the
  // outcome phase runs.
  { op2::Pinwheel pw = op2::createPinwheel(p1);
    pw.setPoints({ { { 62, 66 }, { 58, 78 }, { 54, 86 }, { 50, 90 }, 0, 0, 0 } });
    pw.setGuardComp(1, 2, { { MapID::Lynx, MapID::Laser } });
    pw.setSapperComp(1, 1, { { MapID::Lynx, MapID::EMP } });
    chk(pw.valid(),                                               "Pinwheel setGuardComp/setSapperComp on a live Pinwheel"); }

  op2::log::linef("==================== SELF-TEST: %d PASSED, %d FAILED ====================", g_pass, g_fail);

  // Surface the result in the in-game Communications log, with a Beep so it pings the player even amid the
  // welcome / disaster / combat messages. Show the version + the X/total count so PASS vs FAIL is unmistakable.
  const int total = g_pass + g_fail;
  char msg[96];
  if (g_fail == 0) std::snprintf(msg, sizeof(msg), "TitanAPI v%s self-test: %d/%d PASSED", TITANAPI_VERSION, g_pass, total);
  else             std::snprintf(msg, sizeof(msg), "TitanAPI v%s self-test: %d of %d FAILED - see log", TITANAPI_VERSION, g_fail, total);
  op2::Game::addMessage(msg, /*soundID=*/85 /* SoundID::Beep2 */);
}

/// OUTCOME phase - run once from AIProc at kOutcomeTick. The tick-3 self-test can only check that an order was
/// ACCEPTED (orders apply asynchronously); this checks, later, that the demo orders AIProc issued actually
/// HAPPENED in the game. Logged with its own summary line, separate from the self-test's.
/// Timing: the slowest outcome is the ConVec's Agridome - construction finishes at about tick 1870 (measured
/// in OP2TestHarness; while building, the ConVec's action reads ObjFroze). 2500 leaves margin and still falls
/// inside a 3000-tick harness run.
static constexpr int kOutcomeTick = 2500;
static void outcomes() {
  const int pass0 = g_pass, fail0 = g_fail;
  op2::log::line("==================== OUTCOME CHECKS ====================");
  const op2::Player p0 = op2::Game::player(0);

  // move(): the scout was ordered from (49,84) to (55,84) on the first AIProc call.
  { const op2::Location at = g_scout.location();
    op2::log::linef("    scout at vis(%d,%d)", at.x, at.y);
    chk(g_scout.isLive() && at.x == 55 && at.y == 84, "outcome: scout.move(55,84) arrived"); }

  // build(): the ConVec's Agridome kit with site corner (56,81) (border included) -> the 3x2 Agridome covers
  // (53..55, 79..80). It must exist AND have finished construction.
  { const op2::Unit b = op2::GameMap::unitOnTile({ 54, 80 });
    op2::log::linef("    tile (54,80): type=%d owner=%d underConstruction=%d disabled=%d | convec cmd=%d action=%d",
                    int(b.type()), b.ownerId(), b.underConstruction() ? 1 : 0, b.isDisabled() ? 1 : 0,
                    int(g_buildConvec.command()), int(g_buildConvec.action()));
    chk(b.isLive() && b.type() == MapID::Agridome && b.ownerId() == 0 && !b.underConstruction(),
        "outcome: convec.build -> Agridome built at (53..55, 79..80)"); }

  // buildTube(): the Earthworker was ordered at mark 1 to lay a tube at (61,98).
  { const int ct = int(op2::GameMap::getCellType({ 61, 98 }));
    op2::log::linef("    tile (61,98): cellType=%d", ct);
    // (op2::CellType is the terrain subset; the tube classes are in the full engine enum, op2::abi::CellType.)
    chk(ct >= int(op2::abi::CellType::Tube0) && ct <= int(op2::abi::CellType::Tube5),
        "outcome: earthworker.buildTube -> tube at (61,98)"); }

  // EMP capture: two EMP Lynxes EMP the enemy laser Lynx, then the Spider reprograms it -> player 0 owns it.
  op2::log::linef("    enemy lynx: live=%d owner=%d", g_enemyLynx.isLive() ? 1 : 0, g_enemyLynx.ownerId());
  chk(g_enemyLynx.isLive() && g_enemyLynx.ownerId() == 0, "outcome: EMP + reprogram captured the enemy Lynx");

  // mine(): two trucks haul ore mine -> smelter (and the organic mine's trucks too). Player 0 started with 5000.
  op2::log::linef("    player 0 common ore = %d (started at 5000)", p0.commonOre());
  chk(p0.commonOre() > 5000, "outcome: truck mining routes delivered ore (common ore rose)");

  // deploy(): the Robo-Miner built a Common Ore Mine on the surveyed organic beacon at (35,90).
  { const int mineId = op2::abi::findBuilding(0, int(MapID::CommonOreMine), op2::Location{ 35, 90 }.engineX(),
                                              op2::Location{ 35, 90 }.engineY(), 2);
    chk(mineId > 0, "outcome: robo-miner.deploy built a mine on the organic beacon"); }

  // patrol(): the red scout patrols 58,67 <-> 68,67 - it must have covered most of that span.
  op2::log::linef("    red scout x range seen: %d..%d", g_redMinX, g_redMaxX);
  chk(g_redMinX <= 60 && g_redMaxX >= 66, "outcome: redScout.patrol covered the 58..68 route");

  // onTick(50): the one-shot time trigger fired its C++ callback exactly once.
  op2::log::linef("    onTick(50) callback runs = %d, hasFired(0) = %d", g_tickCallbacks,
                  g_tickTrigger.hasFired(0) ? 1 : 0);
  chk(g_tickCallbacks == 1 && g_tickTrigger.hasFired(0), "outcome: onTick(50) fired once (callback + hasFired)");

  // The remaining Unit orders (issued in the self-test at tick 3):
  op2::log::linef("    dump truck cargo=%d amount=%d", g_dumpTruck.truckCargo(), g_dumpTruck.truckCargoAmount());
  chk(g_dumpTruck.isLive() && g_dumpTruck.truckCargoAmount() == 0, "outcome: dumpCargo emptied the truck");
  { const op2::Location at = g_standLynx.location();
    op2::log::linef("    standGround lynx at vis(%d,%d)", at.x, at.y);
    chk(g_standLynx.isLive() && at.x == 84 && at.y == 90, "outcome: standGround(84,90) arrived"); }
  chk(!g_killMe.isLive(),                                       "outcome: kill() - the unit is dead");
  { const int ct = int(op2::GameMap::getCellType({ 86, 92 }));
    op2::log::linef("    tile (86,92): cellType=%d", ct);
    chk(ct == int(op2::abi::CellType::NormalWall),              "outcome: buildWall -> wall at (86,92)"); }
  chk(g_idled.isLive() && g_idled.isIdled(),                    "outcome: idle() - the Tokamak reads isIdled");
  // FightGroup doAttackUnit: both attack groups set up in the self-test must have destroyed their targets.
  op2::log::linef("    attack targets: p0-group target live=%d, AI-group target live=%d",
                  g_attackTarget0.isLive() ? 1 : 0, g_attackTarget1.isLive() ? 1 : 0);
  chk(g_attackTarget0.valid() && !g_attackTarget0.isLive(),    "outcome: p0 FightGroup doAttackUnit destroyed its target");
  chk(g_attackTarget1.valid() && !g_attackTarget1.isLive(),    "outcome: AI FightGroup doAttackUnit(target) destroyed its target");
  // Building operational flags on the finished, tubed demo Agridome (checked above): powered and staffed.
  { const op2::Unit b = op2::GameMap::unitOnTile({ 54, 80 });
    op2::log::linef("    agridome flags: power=%d workers=%d scientists=%d idled=%d infected=%d", b.hasPower() ? 1 : 0,
                    b.hasWorkers() ? 1 : 0, b.hasScientists() ? 1 : 0, b.isIdled() ? 1 : 0, b.isInfected() ? 1 : 0);
    chk(b.hasPower() && b.hasWorkers() && !b.isIdled() && !b.isInfected(),
        "outcome: finished Agridome reads powered + staffed, not idled or infected"); }

  const int pass = g_pass - pass0, fail = g_fail - fail0;
  op2::log::linef("==================== OUTCOME CHECKS: %d PASSED, %d FAILED ====================", pass, fail);
  char msg[96];
  std::snprintf(msg, sizeof(msg), "TitanAPI outcome checks: %d/%d PASSED", pass, pass + fail);
  op2::Game::addMessage(msg, /*soundID=*/85);
}

} // namespace selftest

// =====================================================================================================================
// Mission entry points (exact names & __cdecl signatures Outpost 2 calls).
// =====================================================================================================================

extern "C" int __stdcall DllMain(void* /*hinst*/, unsigned long reason, void* /*reserved*/) {
  if (reason == 1) {  // DLL_PROCESS_ATTACH
    // Install crash diagnostics first, and route the game tick into every log line, before anything can fault.
    op2::crash::installHandler();
    op2::log::setTickSource([] { return op2::Game::tick(); });
    op2::log::linef("==================== TitanAPI mission DLL_PROCESS_ATTACH (pid=%lu) ====================", op2::log::pid());
    op2::log::timestamp();
#ifdef TITANAPI_VERSION
    op2::log::linef("TitanAPI v%s (Layer 2 facade)", TITANAPI_VERSION);
#endif
    op2::log::linef("log file: %s", op2::log::path());
  } else if (reason == 0) {  // DLL_PROCESS_DETACH
    op2::log::linef("TitanAPI mission DLL_PROCESS_DETACH (pid=%lu)", op2::log::pid());
  }
  return 1;
}

/// Set up a Plymouth human colony and place a small base - all through the TitanAPI facade.
/// The real work lives here; the exported InitProc wraps it in an SEH guard (see below).
static void initProcImpl() {
  op2::log::line("InitProc: enter (Layer 2 facade)");
  // Module 7: stamp the save-state on a FRESH start. (On a loaded save, InitProc is NOT called - the saved
  // MissionState is restored instead, so this stamp + the aiProcTicks count carry through the reload.)
  g_state.magic = kMissionMagic;
  g_state.initProcRuns += 1;
  op2::log::linef("  MissionState stamped: magic=0x%08X initProcRuns=%d", g_state.magic, g_state.initProcRuns);

  // Mission-load message (tick 0), and lock morale steady (OP2-idiom: forceMoraleGood(-1) / freeMoraleLevel(-1)).
  // Morale is forced at tick 0 (here) so the engine doesn't raise the "cheated game!" alert.
  op2::Game::addMessage("Welcome to TitanAPI");
  step(op2::Game::forceMoraleGood(-1), "forceMoraleGood(-1) [steady morale]");
  // (The engine->C++ time-trigger callback is still demonstrated by the mark-8 meteor below. Victory/defeat
  // conditions are created at the END of InitProc - AFTER the colonies exist - so count triggers don't latch.)

  // ---- player setup (fluent: setup is infallible, so it chains) ----
  op2::Player p0 = op2::Game::player(0);
  p0.goPlymouth().goHuman().setPopulation(20, 10, 10).setFood(5000, /*cap*/ 10000)
    .setCommonOre(5000, /*cap*/ 10000).setTechLevel(12);
  op2::log::line("  player 0 set up (Plymouth / Human / 40 colonists / tech 12)");

  // ---- units (in-game / visible coordinates) ----
  auto place = [&](MapID type, int x, int y, MapID weapon = MapID::None) {
    const op2::Result<op2::Unit> u = op2::Game::createUnit(type, { x, y }, p0, weapon);
    op2::log::linef("  createUnit type=%d at vis(%d,%d) -> %s id=%d",
                    int(type), x, y, u ? "ok" : "FAIL", u ? u->id() : -1);
  };
  place(MapID::CommandCenter, 48, 80);
  place(MapID::MHDGenerator,  38, 80);
  place(MapID::CommonStorage, 51, 80);
  place(MapID::ConVec,        50, 84);
  place(MapID::Lynx,          46, 84, MapID::Microwave);
  place(MapID::Lynx,          47, 84, MapID::Microwave);

  // Keep the scout so AIProc can order it to move (proves the order path from the live game loop).
  if (auto scout = op2::Game::createUnit(MapID::Scout, { 49, 84 }, p0)) {
    g_scout = *scout;
    op2::log::linef("  scout created id=%d (will be ordered to move in AIProc)", g_scout.id());
  } else {
    op2::log::line("  ! scout create FAILED");
  }

  // A ConVec carrying an Agridome kit - AIProc will order it to build (exercises the MapObject read path:
  // build() reads the carried kit + looks up its footprint from the type table).
  if (auto cv = op2::Game::createUnit(MapID::ConVec, { 52, 86 }, p0, MapID::Agridome)) {
    g_buildConvec = *cv;
    op2::log::linef("  build convec created id=%d (Agridome kit)", g_buildConvec.id());
  } else {
    op2::log::line("  ! build convec create FAILED");
  }

  // Tube lines connecting the colony.
  op2::Game::createTubeLine({ 48, 82 }, { 48, 86 });
  op2::log::line("  tube line 48,82 -> 48,86");
  op2::Game::createTubeLine({ 49, 91 }, { 49, 98 });
  op2::log::line("  tube line 49,91 -> 49,98");
  op2::Game::createTubeLine({ 50, 98 }, { 55, 98 });
  op2::log::line("  tube line 50,98 -> 55,98");

  auto recordU = [&](const op2::Result<op2::Unit>& r, op2::Unit& slot, const char* what) {
    if (r) { slot = *r; op2::log::linef("  %s -> ok id=%d", what, slot.id()); }
    else   { op2::log::linef("  ! %s FAILED", what); }
  };

  // ---- SCRIPTED mining: directly placed mine + smelter + 2 trucks (campaign-style pre-placement) ----
  { const auto bk = op2::Game::createMine({ 44, 88 });                // ore deposit under the mine
    op2::log::linef("  createMine(44,88): %s", bk ? "ok" : "FAILED"); }
  recordU(op2::Game::createUnit(MapID::CommonOreMine,    { 44, 88 }, p0), g_mine,    "mine(44,88)");
  recordU(op2::Game::createUnit(MapID::CommonOreSmelter, { 49, 88 }, p0), g_smelter, "smelter(49,88)");
  recordU(op2::Game::createUnit(MapID::CargoTruck,       { 46, 90 }, p0), g_truck1,  "truck1(46,90)");
  recordU(op2::Game::createUnit(MapID::CargoTruck,       { 47, 90 }, p0), g_truck2,  "truck2(47,90)");

  // ---- ORGANIC mining flow: unsurveyed beacon -> survey -> Robo-Miner builds the mine ----
  // (the real OP2 flow: a Robo-Surveyor reveals the beacon, then a Robo-Miner deploys a mine onto it.)
  if (auto bk = op2::Game::createMine({ 35, 90 })) {
    g_orgBeacon = *bk;
    op2::log::linef("  organic beacon id=%d  isSurveyed(p0)=%d (expect 0)",
                    g_orgBeacon.id(), g_orgBeacon.isSurveyed(0) ? 1 : 0);
    recordU(op2::Game::createUnit(MapID::RoboSurveyor, { 38, 91 }, p0), g_surveyor,  "robo-surveyor");
    // Reveal it now via the survey API (scripted) so the Robo-Miner can build immediately.
    step(g_orgBeacon.survey(0), "beacon.survey(player 0)");
    op2::log::linef("  organic beacon isSurveyed(p0)=%d (expect 1)", g_orgBeacon.isSurveyed(0) ? 1 : 0);
    recordU(op2::Game::createUnit(MapID::RoboMiner,        { 39, 91 }, p0), g_roboMiner,  "robo-miner");
    // The organic mine's smelter + trucks - placed now; the trucks get routed once the Robo-Miner has
    // actually built the mine (AIProc auto-detects it; see below).
    recordU(op2::Game::createUnit(MapID::CommonOreSmelter, { 31, 90 }, p0), g_orgSmelter, "organic smelter");
    recordU(op2::Game::createUnit(MapID::CargoTruck,       { 32, 92 }, p0), g_orgTruck1,  "organic truck1");
    recordU(op2::Game::createUnit(MapID::CargoTruck,       { 33, 92 }, p0), g_orgTruck2,  "organic truck2");
  } else {
    op2::log::line("  ! organic beacon createMine FAILED");
  }

  // Earthworker - AIProc orders it to build a tube at 61,98 once the clock reaches mark 1 (tick 100).
  recordU(op2::Game::createUnit(MapID::Earthworker, { 61, 94 }, p0), g_earthworker, "earthworker");

  // ---- player 1: a computer-controlled (AI) Eden outpost ----
  op2::Player p1 = op2::Game::player(1);
  op2::log::line("  --- player 1 (AI Eden) ---");
  p1.goEden().goAI().setPopulation(20, 10, 10).setFood(5000, 10000)
    .setCommonOre(5000, 10000).setTechLevel(12);
  op2::log::line("  player 1 set up (Eden / AI / tech 12)");

  auto place1 = [&](MapID type, int x, int y, MapID weapon = MapID::None) {
    const op2::Result<op2::Unit> u = op2::Game::createUnit(type, { x, y }, p1, weapon);
    op2::log::linef("  p1 createUnit type=%d at vis(%d,%d) -> %s id=%d",
                    int(type), x, y, u ? "ok" : "FAIL", u ? u->id() : -1);
  };
  place1(MapID::CommandCenter, 70, 70);
  place1(MapID::CommonStorage, 73, 70);
  place1(MapID::ConVec,        72, 74);
  // Keep one AI Lynx (Laser) so AIProc can order it too - proves orders route to player 1, not player 0.
  // It's also the target of the EMP-capture demo below.
  if (auto lynx = op2::Game::createUnit(MapID::Lynx, { 71, 74 }, p1, MapID::Laser)) {
    g_aiLynx = *lynx;
    op2::log::linef("  ai lynx (Laser) created id=%d", g_aiLynx.id());
  } else {
    op2::log::line("  ! ai lynx create FAILED");
  }

  // EMP-capture demo (classic OP2 tactic), all clustered ~(70-73, 88-91): a player-0 EMP Lynx EMPs a nearby
  // enemy laser Lynx, then a player-0 Spider reprograms it - player 0 takes ownership. The EMP Lynx is right
  // next to the target so it EMPs it immediately; the reprogram fires once the target reads EMP'd (AIProc).
  recordU(op2::Game::createUnit(MapID::Lynx,   { 73, 88 }, p1, MapID::Laser), g_enemyLynx,    "enemy laser lynx");
  recordU(op2::Game::createUnit(MapID::Lynx,   { 70, 91 }, p0, MapID::EMP),   g_empLynx,      "emp lynx");
  recordU(op2::Game::createUnit(MapID::Lynx,   { 73, 91 }, p0, MapID::EMP),   g_empLynx2,     "emp lynx 2");
  recordU(op2::Game::createUnit(MapID::Spider, { 72, 90 }, p0),               g_reprogSpider, "reprogram spider");
  // Red (AI) Scout that patrols the AI's northern approach.
  recordU(op2::Game::createUnit(MapID::Scout,  { 63, 67 }, p1),               g_redScout,     "red scout (patrol)");

  // DIAGNOSTIC (victory-on-load): count each player's LIVE buildings right now, via Module 3 enumeration -
  // ground truth for what onBuildingCount should see. If player1 reads 0 here, the colony victory is correct
  // to fire; if it reads 2, the trigger/eval is the problem.
  // FAILURE condition - you lose if your Command Center is destroyed.
  op2::defeatWhen(op2::onOperational(0, MapID::CommandCenter, op2::Compare::Equal, 0));
  // NOTE: OP2 *AND*s its victory conditions - the mission ends only when EVERY victory condition is met.
  // (Proven the hard way: a never-firing "display" victoryWhen sitting alongside op2::win() holds the
  // mission open even after op2::win() goes green - see the v0.5.23 log, CC destroyed at tick 1603 but no
  // end.) So we register NO standalone display victories. The ONE and only victory is op2::win() in AIProc;
  // being the sole victory, it ends the mission the instant it fires.  ← Kill CC = Win.
  op2::log::line("  defeat condition set; victory is the sole op2::win() path in AIProc");

  // Module 6: a timed DISASTER - a large meteor strikes near the AI base at mark 8 (tick 800). Proves the
  // disaster API end-to-end: engine fires the time trigger -> our C++ lambda -> Game::createMeteor.
  op2::onMark(8, [] {
    const op2::Result<void> r = op2::Game::createMeteor({ 66, 72 }, /*size*/ 0);
    op2::log::linef("  >>> mark 8 meteor at vis(66,72): %s", r ? "ok" : "FAIL");
    op2::Game::addMessage("Meteor strike incoming!");
  });
  op2::log::line("  onMark(8) meteor disaster registered");

  op2::log::line("InitProc: returning 1 (success)");
}

/// Exported entry point - runs initProcImpl under an SEH guard so a fault is logged (not silent) and the
/// engine still gets its return value.
extern "C" __declspec(dllexport) int InitProc() {
  op2::crash::guard("InitProc", &initProcImpl);
  return 1;
}

/// Called every 4 ticks; log the first call only (proves the mission update loop runs). Body in aiProcImpl;
/// the exported AIProc wraps it in an SEH guard.
static void aiProcImpl() {
  ++g_state.aiProcTicks;   // Module 7: counts every frame; persists across save/load via the save region
  static bool first = true;
  if (first) {
    first = false;
    op2::log::line("AIProc: first call - mission update loop is running");
    // Module 7: detect whether this session is a FRESH start or a LOADED save. On a fresh start InitProc ran
    // and stamped kMissionMagic; on a load, InitProc didn't run this session but the restored save region still
    // carries the magic + a non-trivial aiProcTicks from when the game was saved.
    if (g_state.magic == kMissionMagic)
      op2::log::linef("  save-state: %s (initProcRuns=%d, restored aiProcTicks=%d)",
                      (g_state.aiProcTicks > 1) ? "LOADED from save" : "fresh start",
                      g_state.initProcRuns, g_state.aiProcTicks);
    // Module 1 order API: move one unit per player, through the command-packet path. Each dispatches to its
    // OWN player's ProcessCommandPacket - proves the owner-carry mechanism routes correctly.
    const op2::Result<void> m0 = g_scout.move({ 55, 84 });   // player 0
    if (m0) op2::log::linef("  g_scout.move(55,84): ok (id=%d)", g_scout.id());
    else    op2::log::linef("  ! g_scout.move FAILED: %.*s",
                            m0.error().what);

    const op2::Result<void> m1 = g_aiLynx.move({ 65, 74 });  // player 1 (AI) - within its relocated base
    if (m1) op2::log::linef("  g_aiLynx.move(65,74): ok (id=%d)", g_aiLynx.id());
    else    op2::log::linef("  ! g_aiLynx.move FAILED: %.*s",
                            m1.error().what);

    // Module 5: form an AI combat group around the Lynx and send it to HUNT the human player. This is the
    // "script an AI" path - the engine commands the GROUP strategically, instead of us ordering each unit.
    if (g_aiLynx.valid()) {
      g_aiFightGroup = op2::createFightGroup(op2::Game::player(1));
      g_aiFightGroup.takeUnit(g_aiLynx);
      g_aiFightGroup.setAttackType(MapID::Lynx);   // prioritise enemy combat units
      g_aiFightGroup.doAttackEnemy();
      op2::log::linef("  AI FightGroup id=%d formed (%d unit[s]) -> doAttackEnemy",
                      g_aiFightGroup.id(), g_aiFightGroup.totalUnitCount());
    }

    // EMP-capture: both player-0 EMP Lynxes attack (EMP) the adjacent enemy laser Lynx at 73,88.
    { const op2::Result<void> e = g_empLynx.attack(g_enemyLynx);
      op2::log::linef("  g_empLynx.attack(enemy laser Lynx @73,88): %s", e ? "ok" : "FAIL"); }
    { const op2::Result<void> e2 = g_empLynx2.attack(g_enemyLynx);
      op2::log::linef("  g_empLynx2.attack(enemy laser Lynx @73,88): %s", e2 ? "ok" : "FAIL"); }

    // Red (AI) Scout patrols back and forth.
    { const op2::Result<void> pr = g_redScout.patrol({ 58, 67 }, { 68, 67 });
      op2::log::linef("  g_redScout.patrol(58,67<->68,67): %s", pr ? "ok" : "FAIL"); }

    // Module 3: enumerate units via C++ ranges.
    int total = 0, mine0 = 0;
    for (op2::Unit u : op2::Game::units())    { (void)u; ++total; }
    for (op2::Unit u : op2::Game::unitsOf(0)) { (void)u; ++mine0; }
    op2::log::linef("  enumerate: %d live units total, %d owned by player 0", total, mine0);

    // Module 2 read-back (proves the MapObject read path in-game): read the scout's live state.
    const op2::Location sl = g_scout.location();
    op2::log::linef("  scout state: live=%d owner=%d type=%d at vis(%d,%d) damage=%d",
                    g_scout.isLive() ? 1 : 0, g_scout.ownerId(), int(g_scout.type()), sl.x, sl.y, g_scout.damage());

    // Build demo: the ConVec builds its Agridome with bottom-right corner at visible (56,81).
    const op2::Result<void> built = g_buildConvec.build({ 56, 81 });
    if (built) op2::log::linef("  g_buildConvec.build(56,81): ok (id=%d, kit-cargo=%d)",
                               g_buildConvec.id(), g_buildConvec.cargo());
    else       op2::log::linef("  ! g_buildConvec.build FAILED: %.*s",
                               built.error().what);

    // Mining route: both trucks haul ore mine <-> smelter (CargoRoute, built correctly).
    auto route = [&](op2::Unit& t, const char* name) {
      const op2::Result<void> r = t.mine(g_mine, g_smelter);
      if (r) op2::log::linef("  %s.mine(mine=%d, smelter=%d): ok", name, g_mine.id(), g_smelter.id());
      else   op2::log::linef("  ! %s.mine FAILED: %s", name, r.error().what);
    };
    route(g_truck1, "truck1");
    route(g_truck2, "truck2");

    // Organic flow: send the Robo-Surveyor to the beacon (surveys en route) and the Robo-Miner to deploy
    // the mine onto it. The mine appears over the next many ticks as the units travel and build.
    { const auto sv = g_surveyor.move({ 35, 90 });
      op2::log::linef("  robo-surveyor move->beacon: %s", sv ? "ok" : "FAIL"); }
    { const auto dp = g_roboMiner.deploy({ 35, 90 });
      op2::log::linef("  robo-miner deploy->beacon: %s", dp ? "ok" : "FAIL"); }

    // Full API self-test (exercises every facade function; logs PASS/FAIL + a summary).
    selftest::run();
  }

  // Every tick until done: once the Robo-Miner has actually built the organic mine (it's created
  // dynamically, so we locate it by walking player 0's building list near the beacon), route its trucks.
  static bool orgRouted = false;
  if (!orgRouted && g_orgSmelter.valid() && g_orgTruck1.valid()) {
    const op2::Location bl{ 35, 90 };                                  // the organic beacon's tile
    const int mineId = op2::abi::findBuilding(0, int(op2::MapID::CommonOreMine), bl.engineX(), bl.engineY(), 2);
    if (mineId > 0) {
      const op2::Unit mine{ mineId, 0 };
      const bool ok1 = g_orgTruck1.mine(mine, g_orgSmelter).has_value();
      const bool ok2 = g_orgTruck2.mine(mine, g_orgSmelter).has_value();
      orgRouted = true;
      op2::Game::addMessage("Mine operational");   // sending an in-game message is one call
      op2::log::linef("  organic mine built (id=%d) - routed trucks (t1=%s t2=%s)",
                      mineId, ok1 ? "ok" : "FAIL", ok2 ? "ok" : "FAIL");
    }
  }

  // EMP-capture: once the EMP Lynx has EMP'd the enemy laser Lynx, the Spider reprograms it (player 0 takes
  // ownership). Fires once, when the target reads as EMP'd.
  static bool captured = false;
  if (!captured && g_reprogSpider.valid() && g_enemyLynx.isEMPed()) {
    const op2::Result<void> r = g_reprogSpider.reprogram(g_enemyLynx);
    captured = true;
    op2::Game::addMessage("Enemy Lynx captured");
    op2::log::linef("  enemy Lynx EMP'd -> reprogram: %s", r ? "ok" : "FAIL");
  }

  // PROGRAMMATIC VICTORY (checked once per mark, via the Module 3 enumeration which skips dead units):
  //   (1) the AI's Command Center is destroyed, OR (2) ALL the AI's buildings are destroyed.
  static int s_lastMark = -1;
  if (const int m = op2::Game::mark(); m != s_lastMark) {
    s_lastMark = m;
    static bool s_won = false;
    if (!s_won) {
      const int aiCCs = op2::Game::playerUnitCount(1, MapID::CommandCenter);
      int aiBuildings = 0;
      for (op2::Unit u : op2::Game::unitsOf(1)) if (u.isBuilding()) ++aiBuildings;
      if (aiCCs == 0) {
        s_won = true; op2::win("Destroy the enemy Command Center");
        op2::log::line("  >>> AI Command Center destroyed -> op2::win()");
      } else if (aiBuildings == 0) {
        s_won = true; op2::win("Destroy ALL enemy buildings");
        op2::log::line("  >>> all AI buildings destroyed -> op2::win()");
      }
    }
  }

  // Track the red scout's patrol span, then run the OUTCOME checks once the demo orders have had time to finish.
  if (g_redScout.isLive()) {
    const int x = g_redScout.location().x;
    if (x < selftest::g_redMinX) selftest::g_redMinX = x;
    if (x > selftest::g_redMaxX) selftest::g_redMaxX = x;
  }
  static bool outcomesDone = false;
  if (!outcomesDone && op2::Game::tick() >= selftest::kOutcomeTick) { outcomesDone = true; selftest::outcomes(); }

  // At mark 1 (tick 100), order the Earthworker to build a tube at 61,98. Fires once.
  static bool tubeAtMark1 = false;
  if (!tubeAtMark1 && g_earthworker.valid() && op2::Game::mark() >= 1) {
    const op2::Result<void> r = g_earthworker.buildTube({ 61, 98 });
    tubeAtMark1 = true;
    op2::log::linef("  mark %d (tick %d): earthworker.buildTube(61,98): %s",
                    op2::Game::mark(), op2::Game::tick(), r ? "ok" : "FAIL");
  }
}

/// Exported per-frame entry point - runs aiProcImpl under an SEH guard (a fault is logged, the game survives).
extern "C" __declspec(dllexport) void AIProc() {
  op2::crash::guard("AIProc", &aiProcImpl);
}

/// Declares save-game state (Module 7): registers g_state so OP2 saves & restores it across save/load.
extern "C" __declspec(dllexport) void GetSaveRegions(op2::mission::SaveRegion* pSave) {
  static bool first = true;
  if (first) { first = false; op2::log::linef("GetSaveRegions: persisting MissionState (%d bytes)", int(sizeof(g_state))); }
  if (pSave) { pSave->pData = &g_state; pSave->size = sizeof(g_state); }
}

// Module 7 - typed save/load over StreamIO (OPU 1.4.0+). On save we stash a tag string + the live tick count;
// on load we read them back and log them. SEH-guarded: StreamIO's typed Write/Read go through the engine vtable
// (slots 6/7), so if that slot index is ever wrong the fault is logged and the save/load still completes. The
// crash guard takes a plain function pointer (its __try can't hold capturing lambdas), so the StreamIO* is
// parked in a file-scope pointer for the guarded impl to pick up.
static void* g_saveStream = nullptr;
static void onSaveGameImpl() {
  op2::mission::Stream s{ g_saveStream };
  const bool okStr = s.writeString("TitanAPI");                 // non-virtual entry - always safe
  const int  tick  = op2::Game::tick();
  const bool okVal = s.writeValue(tick);                        // virtual Write (vtable slot 6)
  op2::log::linef("OnSaveGame: wrote tag(%s) tick=%d val(%s)", okStr ? "ok" : "FAIL", tick, okVal ? "ok" : "FAIL");
}
static void onLoadSavedGameImpl() {
  op2::mission::Stream s{ g_saveStream };
  const char* tag = s.readString();                            // non-virtual entry - always safe
  int tick = -1; const bool okVal = s.readValue(tick);         // virtual Read (vtable slot 7)
  op2::log::linef("OnLoadSavedGame: tag=%s saved-tick=%d val(%s); restored aiProcTicks=%d",
                  tag ? tag : "(null)", tick, okVal ? "ok" : "FAIL", g_state.aiProcTicks);
}
extern "C" __declspec(dllexport) void OnSaveGame(op2::mission::OnSaveGameArgs* a) {
  if (!a || !a->pSavedGame) return;
  g_saveStream = a->pSavedGame;
  op2::crash::guard("OnSaveGame", &onSaveGameImpl);
}
extern "C" __declspec(dllexport) void OnLoadSavedGame(op2::mission::OnLoadSavedGameArgs* a) {
  if (!a || !a->pSavedGame) return;
  g_saveStream = a->pSavedGame;
  op2::crash::guard("OnLoadSavedGame", &onLoadSavedGameImpl);
}

// Module 7 - the remaining lifecycle callbacks (OPU 1.4.0+). Simple log demos that prove each hook fires; they
// only read fields of the args struct OPU hands us (and OnChat's null-checked engine string), so they're safe
// without a per-call SEH guard - the process-wide unhandled-exception filter is the backstop.
extern "C" __declspec(dllexport) int OnLoadMission(op2::mission::OnLoadMissionArgs*) {
  op2::log::line("OnLoadMission: DLL loaded");   return 1;   // return 0 would abort the mission load
}
extern "C" __declspec(dllexport) int OnUnloadMission(op2::mission::OnUnloadMissionArgs*) {
  op2::log::line("OnUnloadMission: DLL unloading");  return 1;
}
extern "C" __declspec(dllexport) void OnEndMission(op2::mission::OnEndMissionArgs* a) {
  op2::log::linef("OnEndMission: mission ended (results %s)", (a && a->pMissionResults) ? "present" : "none");
}
extern "C" __declspec(dllexport) void OnChat(op2::mission::OnChatArgs* a) {
  if (a && a->pText) op2::log::linef("OnChat: player %d: \"%s\"", a->playerNum, a->pText);
}
extern "C" __declspec(dllexport) void OnGameCommand(op2::mission::OnGameCommandArgs* a) {
  static bool first = true;   // fires for EVERY command packet - log only the first to avoid flooding the log
  if (first && a) { first = false; op2::log::linef("OnGameCommand: first command packet processed (player %d)", a->playerNum); }
}
