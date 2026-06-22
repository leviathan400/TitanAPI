// mission.cpp - the TitanAPI in-game SMOKE TEST, built on the Layer 2 FACADE (op2::Game / op2::Player / op2::Unit).
//
// A two-player colony that exercises the ergonomic facade end to end (Result-returning setup, value-handle
// Player/Unit, in-game visible coordinates) and runs an in-mission self-test logging PASSED / FAILED. This is
// the mission used to validate every build of the library. The Layer-1-only version (raw op2::abi) is frozen
// at ../../samples/Layer1 for comparison.
//
// Status: builds on the facade; the underlying engine calls are the same ones verified in-game in the
// Layer-1 sample (CreateUnit @0x478780, PlayerImpl writes, GoHuman @0x4906C0, SetTechLevel @0x473030).

#include "op2.hpp"            // the TitanAPI Layer 2 facade: Result / Error / Location / Unit / Player / Game / MapID
#include "op2_mission.hpp"    // the mission-DLL contract (ModDesc / ModDescEx / SaveRegion / MissionType)
#include "op2_log.hpp"        // append-only debug log -> <OP2>\OPU\logs\cTitanSmokeTest.log

using namespace op2::mission;
using op2::MapID;

// =====================================================================================================================
// Mission metadata - the exact exports Outpost 2 / op2ext read to list and configure the mission.
// MapName / TechtreeName must exist in your install ("on6_01.map" + "MULTITEK.TXT" is the stock Colony combo).
// =====================================================================================================================
extern "C" __declspec(dllexport) char      LevelDesc[]    = "TitanAPI Smoke Test";
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
static op2::Unit g_buildConvec;  // player 0 ConVec carrying an Agridome kit (build demo)
static op2::Unit g_mine;         // player 0 Common Ore Mine (mining-route demo)
static op2::Unit g_smelter;      // player 0 Common Ore Smelter
static op2::Unit g_truck1;       // player 0 Cargo Truck
static op2::Unit g_truck2;       // player 0 Cargo Truck
static op2::Unit g_enemyLynx;    // player 1 enemy laser Lynx @73,88 (target of the EMP-capture demo)
static op2::Unit g_empLynx;      // player 0 EMP Lynx (EMPs the enemy laser Lynx)
static op2::Unit g_reprogSpider; // player 0 Spider (reprograms the EMP'd enemy Lynx -> take ownership)
static op2::Unit g_orgBeacon;    // player 0 ORGANIC mining: the (initially unsurveyed) ore beacon
static op2::Unit g_surveyor;     // player 0 Robo-Surveyor (surveys the beacon by reaching it)
static op2::Unit g_roboMiner;    // player 0 Robo-Miner (builds the mine on the surveyed beacon)
static op2::Unit g_orgSmelter;   // player 0 smelter for the organic mine
static op2::Unit g_orgTruck1;    // player 0 Cargo Truck for the organic mine
static op2::Unit g_orgTruck2;    // player 0 Cargo Truck for the organic mine
static op2::Unit g_earthworker;  // player 0 Earthworker (builds a tube at mark 1)

// =====================================================================================================================
// API SELF-TEST - exercises every TitanAPI facade function in-game and logs [PASS]/[FAIL] + a summary.
// Run once from the live game loop (AIProc). For orders, PASS = the facade built the packet and the engine
// accepted it (orders apply asynchronously, so this is an "accepted" check, not "completed"). For state
// reads, PASS = the value is sane. Creates its own unit roster in a test area so it is self-contained.
// =====================================================================================================================
namespace selftest {

static int g_pass = 0;
static int g_fail = 0;

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
  p0.setTechLevel(12); p0.markResearchComplete(2101); chk(p0.hasTechnology(2101), "Player::setTechLevel + hasTechnology");

  // ---- Game: world creators ----
  { auto beacon = op2::Game::createMine({ 60, 100 });
    chk(bool(beacon) && beacon->valid(), "Game::createMine");
    if (beacon) { chkR(beacon->survey(0), "Unit::survey"); chk(beacon->isSurveyed(0), "Unit::isSurveyed"); } }
  // (Game::createTube / createTubeLine are exercised by the colony tube line in InitProc - no stray tubes here.)

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

  // ---- Unit: orders (PASS = built + accepted) ----
  chkR(lynx.move({ 59, 90 }),             "Unit::move");
  chkR(lynx.attack(enemy),                "Unit::attack(Unit)");
  chkR(lynx.attack(Location{ 66, 90 }),   "Unit::attack(Location)");
  chkR(lynx.guard(smelter),               "Unit::guard");
  chkR(lynx.setLights(true),              "Unit::setLights");
  chkR(lynx.stop(),                       "Unit::stop");
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
  chkErr(convecNK.build({ 62, 90 }),                             op2::Status::WrongType,       "build() with no kit");
  chkErr(truck.mine(op2::Unit{}, smelter),                       op2::Status::InvalidTarget,   "mine() invalid mine target");

  op2::log::linef("==================== SELF-TEST: %d PASSED, %d FAILED ====================", g_pass, g_fail);
}

} // namespace selftest

// =====================================================================================================================
// Mission entry points (exact names & __cdecl signatures Outpost 2 calls).
// =====================================================================================================================

extern "C" int __stdcall DllMain(void* /*hinst*/, unsigned long reason, void* /*reserved*/) {
  if (reason == 1) {  // DLL_PROCESS_ATTACH
    op2::log::line("==================== TitanAPI mission DLL_PROCESS_ATTACH ====================");
#ifdef TITANAPI_VERSION
    op2::log::linef("TitanAPI v%s (Layer 2 facade)", TITANAPI_VERSION);
#endif
    op2::log::linef("log file: %s", op2::log::path());
  } else if (reason == 0) {  // DLL_PROCESS_DETACH
    op2::log::line("TitanAPI mission DLL_PROCESS_DETACH");
  }
  return 1;
}

/// Set up a Plymouth human colony and place a small base - all through the TitanAPI facade.
extern "C" __declspec(dllexport) int InitProc() {
  op2::log::line("InitProc: enter (Layer 2 facade)");

  // Mission-load message, and lock morale steady (OP2-idiom: forceMoraleGood(-1) / freeMoraleLevel(-1)).
  // Morale is forced at tick 0 (here) so the engine doesn't raise the "cheated game!" alert.
  op2::Game::addMessage("TitanAPI");
  step(op2::Game::forceMoraleGood(-1), "forceMoraleGood(-1) [steady morale]");

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
  recordU(op2::Game::createUnit(MapID::Spider, { 72, 90 }, p0),               g_reprogSpider, "reprogram spider");

  op2::log::line("InitProc: returning 1 (success)");
  return 1;
}

/// Called every 4 ticks; log the first call only (proves the mission update loop runs).
extern "C" __declspec(dllexport) void AIProc() {
  static bool first = true;
  if (first) {
    first = false;
    op2::log::line("AIProc: first call - mission update loop is running");
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

    // EMP-capture: the player-0 EMP Lynx attacks (EMPs) the adjacent enemy laser Lynx at 73,88.
    { const op2::Result<void> e = g_empLynx.attack(g_enemyLynx);
      op2::log::linef("  g_empLynx.attack(enemy laser Lynx @73,88): %s", e ? "ok" : "FAIL"); }

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

  // At mark 1 (tick 100), order the Earthworker to build a tube at 61,98. Fires once.
  static bool tubeAtMark1 = false;
  if (!tubeAtMark1 && g_earthworker.valid() && op2::Game::mark() >= 1) {
    const op2::Result<void> r = g_earthworker.buildTube({ 61, 98 });
    tubeAtMark1 = true;
    op2::log::linef("  mark %d (tick %d): earthworker.buildTube(61,98): %s",
                    op2::Game::mark(), op2::Game::tick(), r ? "ok" : "FAIL");
  }
}

/// Declares save-game state. This mission persists none.
extern "C" __declspec(dllexport) void GetSaveRegions(op2::mission::SaveRegion* pSave) {
  static bool first = true;
  if (first) { first = false; op2::log::line("GetSaveRegions: called"); }
  if (pSave) { pSave->pData = nullptr; pSave->size = 0; }
}
