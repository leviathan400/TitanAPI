// mission.cpp - a TitanAPI mission template (a clean starting point to copy).
//
// To start a new mission: copy this whole folder, rename it, change OUTPUT_NAME in CMakeLists.txt to
// "c<YourName>", then edit the five exports + the two procs below. Everything you call lives in the op2::
// facade, which is header-only - there is nothing to link. See ./README.md and ../../docs/GETTING-STARTED.md.

#include "op2.hpp"           // the op2:: facade (umbrella header)
#include "op2/trigger.hpp"   // triggers + win/lose  (include in exactly ONE .cpp of a mission)
#include "op2/base.hpp"      // the declarative BaseBuilder
#include "op2_mission.hpp"   // the mission-DLL contract (DescBlock / SaveRegion types)
#include "op2_log.hpp"       // file logging  -> OPU\logs\<dll>.log
#include "op2_crash.hpp"     // SEH guards + crash logging

using namespace op2;

// ============================================================================
// 1. The fixed exports the engine looks up by name.  TODO: fill these in.
// ============================================================================
extern "C" __declspec(dllexport) char LevelDesc[]    = "My TitanAPI Mission";   // shown in the mission list
extern "C" __declspec(dllexport) char MapName[]      = "on1_01.map";            // a .map present in your install
extern "C" __declspec(dllexport) char TechtreeName[] = "MULTITEK.TXT";          // tech-tree file

// DescBlock: missionType / numPlayers / maxTechLevel (12 = all techs) / unitOnlyMission(0)
extern "C" __declspec(dllexport) mission::ModDesc   DescBlock   = { mission::MissionType::Colony, 1, 12, 0 };
// DescBlockEx: numMultiplayerAIs (0 for single-player) + reserved fields
extern "C" __declspec(dllexport) mission::ModDescEx DescBlockEx = { 0, 0, 0, 0, 0, 0, 0, 0 };

// ============================================================================
// 2. InitProc - set up the world once, at mission start.  TODO: your setup.
// ============================================================================
static void initProc() {
    log::line("InitProc: starting");

    Player you = Game::player(0);
    you.goEden();                 // or you.goPlymouth();
    you.setCommonOre(5000);
    you.setWorkers(20);
    you.setScientists(10);

    // Stamp a starting colony from a declarative layout. Coordinates are VISIBLE tiles
    // (the same numbers shown on the in-game status bar).
    BaseLayout base;
    base.buildings = { { {10, 10}, MapID::CommandCenter },
                       { {14, 10}, MapID::Tokamak },
                       { {10, 14}, MapID::Agridome } };
    base.vehicles  = { { {12, 13}, MapID::ConVec } };
    createBase(you, base);

    // Orders return Result<void> - a failure is a value you can branch on, never an engine crash.
    for (Unit convec : Game::unitsOfType(MapID::ConVec))
        if (auto r = convec.move({ 12, 16 }); !r)
            log::linef("move rejected: %s", r.error().what);

    // Victory / defeat.  TODO: pick your real win condition (see op2/trigger.hpp).
    loseIfNoCommandCenter(0);     // you lose if your Command Center is destroyed

    op2::ignore(Game::forceMoraleGood());
    Game::addMessage("Welcome, commander.");
    log::line("InitProc: done");
}

// ============================================================================
// 3. AIProc - called every few ticks during play.  TODO: your per-frame logic.
// ============================================================================
static void aiProc() {
    // e.g. drive an AI opponent, check mission progress, fire scripted events.
}

// ============================================================================
// 4. Guarded exports + DLL setup.  (You normally do not need to edit below here.)
// ============================================================================
extern "C" __declspec(dllexport) int  InitProc() { crash::guard("InitProc", &initProc); return 1; }
extern "C" __declspec(dllexport) void AIProc()   { crash::guard("AIProc",   &aiProc); }

// Declare save-game state here (point at a persistent block) if your mission needs save/load. None by default.
extern "C" __declspec(dllexport) void GetSaveRegions(mission::SaveRegion* p) {
    if (p) { p->pData = nullptr; p->size = 0; }
}

extern "C" int __stdcall DllMain(void*, unsigned long reason, void*) {
    if (reason == 1 /* DLL_PROCESS_ATTACH */) {
        crash::installHandler();                          // log unhandled faults (address + game tick)
        log::setTickSource([] { return Game::tick(); });  // stamp every log line with the game tick
    }
    return 1;
}
