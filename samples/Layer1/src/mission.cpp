// mission.cpp — a complete, minimal Outpost 2 mission DLL built on Layer 1 ONLY (op2::abi).
//
// What it proves, end to end:
//   1. The DLL exports the mission contract, so Outpost 2 lists & loads it ("Layer1 Test Mission").
//   2. InitProc reaches straight into the running Outpost2.exe through Layer 1 (op2::abi) to create a
//      starting base. If those units appear in-game, Layer 1 works.
// No facade (Layer 2) — raw ABI only.
//
// Status: BUILT (VS2026 / MSVC 19.51, x86, /std:c++23preview, static CRT) and VERIFIED IN-GAME on OPU 1.4.1 —
// the DLL loads as a mission, sets up a Plymouth human colony (population / power / ore storage), and places
// 7 units; each step is recorded in <OP2>\OPU\logs\cLayer1.log. Addresses/offsets verified vs TethysAPI + re-reference.

#include "op2/abi/memory.hpp"   // op2::abi::callFast / reloc — the Layer-1 call mechanism
#include "op2/abi/enums.hpp"    // op2::abi::MapID (generated + verified; unscoped enums namespaced)
#include "op2_mission.hpp"      // the mission-DLL contract (ModDesc / ModDescEx / SaveRegion / MissionType)
#include "op2_log.hpp"          // append-only debug log -> <OP2>\OPU\logs\cLayer1.log

using op2::abi::MapID;          // engine unit/building ids — MapID::CommandCenter, MapID::MHDGenerator, ...

using namespace op2::mission;

// =====================================================================================================================
// Mission metadata — the exact exports Outpost 2 / op2ext read to list and configure the mission.
// (Contract & layout: TethysAPI API/Mission.h.)  Names and types must match exactly.
//
// MapName / TechtreeName must reference files present in your install. "on6_01.map" + "MULTITEK.TXT"
// =====================================================================================================================
extern "C" __declspec(dllexport) char      LevelDesc[]    = "Layer1 Test Mission";
extern "C" __declspec(dllexport) char      MapName[]      = "on6_01.map";
extern "C" __declspec(dllexport) char      TechtreeName[] = "MULTITEK.TXT";
extern "C" __declspec(dllexport) ModDesc   DescBlock      = { MissionType::Colony, /*numPlayers*/ 1, /*maxTechLevel*/ 12, /*unitMission*/ 0 };
extern "C" __declspec(dllexport) ModDescEx DescBlockEx    = { /*numMultiplayerAIs*/ 0, 0, 0, 0, 0, 0, 0, 0 };

// =====================================================================================================================
// Thin Layer-1 binding to one engine function — Game::CreateUnit.
//   ibool __fastcall CreateUnit(Unit& out, MapID, Location, int owner, MapID weaponCargo, UnitDirection)  @ 0x478780
//   (verified: TethysAPI API/Game.h:125).  `Unit` is a 4-byte {int id} handle; `Location` is {int x, int y}.
// We invoke it through op2::abi::callFast, which relocates 0x478780 to the live module base and calls it with
// the __fastcall ABI (1st arg in ECX, 2nd in EDX, the rest on the stack).
// =====================================================================================================================
namespace eng {

struct Location { int x, y; };   // engine-internal tile coords (mirrors Tethys Location; 8-byte POD, by value)

// Outpost 2 has an off-screen map border: the in-game ("visible") tile is offset from the engine's internal
// tile. Observed on this build: internal = visible + (32, 0). CreateUnit() takes VISIBLE coords and adds the
// pad, so mission source matches on-screen positions. Tweak if a placement lands off by one.
constexpr int kPadX = 32;
constexpr int kPadY = 0;

/// Creates a unit for `owner` at the in-game (visible) tile (visX, visY) — the off-screen pad is added below.
/// Returns the engine unit id (the "stub index"). `type` / `weaponCargo` take op2::abi::MapID values.
inline int CreateUnit(int type, int visX, int visY, int owner, int weaponCargo = 0, int rotation = 0) {
  int unitId = 0;  // receives the created unit's id (CreateUnit fills this Unit& out-param)
  op2::abi::callFast<0x478780, int>(&unitId, type, Location{ visX + kPadX, visY + kPadY }, owner, weaponCargo, rotation);
  return unitId;
}

} // namespace eng

// =====================================================================================================================
// Layer-1 player setup — write PlayerImpl fields + call a couple of engine member functions.
//   * Player array: GameImpl's PlayerImpl array base is a hardcoded immediate in OP2 code at 0x4890C3
//     (TethysAPI's GetPlayerArray trick). Each PlayerImpl is 3108 bytes; player(n) = base + n*3108.
//   * Field offsets verified by offsetof against TethysAPI Game/PlayerImpl.h (sizeof == 3108).
//   * GoHuman -> PlayerImpl::GoHuman() @0x4906C0 (thiscall). Tech -> Research::SetTechLevel(num, lvl*1000)
//     @0x473030 on the Research singleton @0x56C230.
// =====================================================================================================================
namespace plr {

constexpr int kStride = 3108;                                   // sizeof(PlayerImpl)
constexpr int OFF_foodStored  = 16,  OFF_maxFood       = 20,  OFF_maxCommonOre = 24,  OFF_commonOre = 32,
              OFF_isEden       = 44,  OFF_numWorkers    = 148, OFF_numScientists = 152, OFF_numKids   = 156,
              OFF_recalc       = 160;

/// PlayerImpl* (as char*) for the given player, or nullptr.
inline char* player(int n) {
  char* base = op2::abi::ref<char*>(0x4890C3);                  // player-array base (hardcoded immediate in OP2 code)
  return base ? base + n * kStride : nullptr;
}
inline void seti(char* p, int off, int val) { *reinterpret_cast<int*>(p + off) = val; }  // write 4-byte int field at byte offset `off`

inline void goHuman(char* p) { op2::abi::member<0x4906C0, void>(p); }       // PlayerImpl::GoHuman()

inline void setTechLevel(int playerNum, int techLevel) {                    // Research::SetTechLevel(num, lvl*1000)
  char* research = reinterpret_cast<char*>(op2::abi::reloc(0x56C230));      // Research singleton @ 0x56C230
  op2::abi::member<0x473030, void>(research, playerNum, techLevel * 1000);
}

} // namespace plr

// =====================================================================================================================
// Mission entry points — exact names & __cdecl signatures Outpost 2 calls (TethysAPI API/Mission.h).
// =====================================================================================================================

/// Outpost 2 (via the OS loader) calls DllMain when the mission DLL is loaded — the earliest sign of life.
/// If you see PROCESS_ATTACH in the log but never "InitProc: enter", the DLL loaded but OP2 didn't run it
/// as a mission (usually a metadata/map problem). MUST return non-zero on attach or loading fails.
extern "C" int __stdcall DllMain(void* /*hinstDll*/, unsigned long reason, void* /*reserved*/) {
  if (reason == 1) {  // DLL_PROCESS_ATTACH
    op2::log::line("==================== cLayer1.dll DLL_PROCESS_ATTACH ====================");
    op2::log::linef("log file: %s", op2::log::path());
  } else if (reason == 0) {  // DLL_PROCESS_DETACH
    op2::log::line("cLayer1.dll DLL_PROCESS_DETACH");
  }
  return 1;
}

/// Called once at mission start; return nonzero on success. Sets up player 0 (faction / human / population /
/// resources / tech) and places a small starting base — all by calling into Outpost2.exe through Layer 1.
extern "C" __declspec(dllexport) int InitProc() {
  op2::log::line("InitProc: enter");
  constexpr int player0 = 0;   // the human player in this 1-player colony

  // ---- player setup: make player 0 a Plymouth human colony with starting population / resources / tech ----
  char* p0 = plr::player(player0);
  op2::log::linef("  PlayerImpl(player 0) = %p", reinterpret_cast<void*>(p0));
  if (p0) {
    plr::seti(p0, plr::OFF_isEden, 0);            op2::log::line("  set isEden_=0 (Plymouth)");
    op2::log::line("  GoHuman() ...");
    plr::goHuman(p0);                             op2::log::line("  -> GoHuman done");
    plr::seti(p0, plr::OFF_numWorkers,     20);
    plr::seti(p0, plr::OFF_numScientists,  10);
    plr::seti(p0, plr::OFF_numKids,        10);
    plr::seti(p0, plr::OFF_maxFood,      10000);
    plr::seti(p0, plr::OFF_foodStored,    5000);
    plr::seti(p0, plr::OFF_maxCommonOre, 10000);
    plr::seti(p0, plr::OFF_commonOre,     5000);
    plr::seti(p0, plr::OFF_recalc,           1);  // nudge the engine to recompute derived values
    op2::log::line("  population: workers=20 scientists=10 kids=10; food=5000 ore=5000");
    op2::log::line("  SetTechLevel(0, 12) ...");
    plr::setTechLevel(player0, 12);               op2::log::line("  -> SetTechLevel done");
  } else {
    op2::log::line("  !! PlayerImpl(0) is null — skipped player setup");
  }

  // Coordinates below are IN-GAME (visible) tiles — eng::CreateUnit adds the off-screen pad (kPadX,kPadY).
  // Each call is logged BEFORE and AFTER: if an engine call crashes the game, the "before" line is on disk.
  int id = 0;
  op2::log::line ("  CreateUnit CommandCenter vis(48,80) p0 ...");
  id = eng::CreateUnit(MapID::CommandCenter, 48, 80, player0);                  op2::log::linef("  -> CommandCenter id=%d", id);
  op2::log::line ("  CreateUnit MHDGenerator vis(38,80) p0 ...");
  id = eng::CreateUnit(MapID::MHDGenerator,  38, 80, player0);                  op2::log::linef("  -> MHDGenerator id=%d", id);
  op2::log::line ("  CreateUnit CommonStorage vis(51,78) p0 ...");
  id = eng::CreateUnit(MapID::CommonStorage, 51, 78, player0);                  op2::log::linef("  -> CommonStorage id=%d", id);
  op2::log::line ("  CreateUnit ConVec vis(50,84) p0 ...");
  id = eng::CreateUnit(MapID::ConVec,        50, 84, player0);                  op2::log::linef("  -> ConVec id=%d", id);
  op2::log::line ("  CreateUnit Lynx/Microwave vis(46,84) p0 ...");
  id = eng::CreateUnit(MapID::Lynx,          46, 84, player0, MapID::Microwave); op2::log::linef("  -> Lynx id=%d", id);
  op2::log::line ("  CreateUnit Lynx/Microwave vis(47,84) p0 ...");
  id = eng::CreateUnit(MapID::Lynx,          47, 84, player0, MapID::Microwave); op2::log::linef("  -> Lynx id=%d", id);
  op2::log::line ("  CreateUnit Scout vis(49,84) p0 ...");
  id = eng::CreateUnit(MapID::Scout,         49, 84, player0);                  op2::log::linef("  -> Scout id=%d", id);

  op2::log::line("InitProc: returning 1 (success)");
  return 1;
}

/// Called every 4 ticks during gameplay. Log only the first call (proves the mission update loop runs).
extern "C" __declspec(dllexport) void AIProc() {
  static bool first = true;
  if (first) { first = false; op2::log::line("AIProc: first call — mission update loop is running"); }
}

/// Declares save-game state. This mission persists none.
extern "C" __declspec(dllexport) void GetSaveRegions(op2::mission::SaveRegion* pSave) {
  static bool first = true;
  if (first) { first = false; op2::log::line("GetSaveRegions: called"); }
  if (pSave) { pSave->pData = nullptr; pSave->size = 0; }
}
