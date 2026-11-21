# Getting Started with TitanAPI

This walks you from nothing to a running Outpost 2 mission written in C++23 with TitanAPI.

## 1. Prerequisites

- **Outpost 2** with the Outpost Universe (OPU) update installed (1.4.1+). That's where your mission DLL goes and
  where its log is written.
- **Visual Studio 2022/2026** (MSVC) with the **C++ desktop** workload. Outpost 2 is a 32-bit process, so you
  build a 32-bit (x86) DLL with genuine C++23 (`/std:c++23preview`).
- **CMake 3.21+** (the one bundled with Visual Studio is fine).

## 2. What a mission DLL is

An Outpost 2 mission is a small DLL that exports a fixed set of names the engine looks up: the level description,
the map and tech-tree files, a descriptor block, and the entry points `InitProc` (set up the world) and `AIProc`
(called every few ticks). TitanAPI gives you the typed contract for all of that, plus the `op2::` facade for
doing the actual work inside `InitProc`/`AIProc`.

## 3. Start from a sample (fastest)

The quickest start is to copy an existing sample folder and rename it:

1. Copy `samples/Template/` to `samples/MyMission/`.
2. In `CMakeLists.txt`, change the project name and `OUTPUT_NAME` to `cMyMission` (the leading `c` files it under
   Colony Games in-game).
3. Edit `mission.cpp` - the rest of this guide explains what's there.

Every sample `#include`s the live library from `../../TitanAPI/include`, so it always builds against the current
`op2::` API. The three small scaffolding headers (`op2_mission.hpp`, `op2_log.hpp`, `op2_crash.hpp`) are shared by
all samples from `samples/common/` - they are the mission-DLL contract and the logging/crash helpers (the sample's
`CMakeLists.txt` already has `../common` on the include path).

## 4. The shape of a mission

```cpp
#include "op2.hpp"           // the op2:: facade
#include "op2/trigger.hpp"   // triggers + win/lose (include in exactly one .cpp)
#include "op2_mission.hpp"   // the mission-DLL contract (DescBlock types)
#include "op2_log.hpp"       // file logging
#include "op2_crash.hpp"     // SEH guards + crash logging

using namespace op2;

// --- the exports the engine looks up by name ---
extern "C" __declspec(dllexport) char    LevelDesc[]    = "My First Mission";
extern "C" __declspec(dllexport) char    MapName[]      = "on1_01.map";   // a map present in your install
extern "C" __declspec(dllexport) char    TechtreeName[] = "MULTITEK.TXT";
extern "C" __declspec(dllexport) mission::ModDesc   DescBlock   = { mission::MissionType::Colony, 1, 12, 0 };
extern "C" __declspec(dllexport) mission::ModDescEx DescBlockEx = { 0, 0, 0, 0, 0, 0, 0, 0 };

// --- set up the world ---
static void initProc() {
    log::line("InitProc: starting");

    Player you = Game::player(0);
    you.goEden().setCommonOre(5000, 10000).setFood(5000, 10000);        // setup is fluent and chains
    you.setWorkers(20).setScientists(10).setKids(10);

    // Tube every building except power plants to the Command Center, or it stays idle.
    BaseLayout base;
    base.buildings = { { {10, 10}, MapID::CommandCenter }, { {16, 10}, MapID::Agridome },
                       { {10, 15}, MapID::Tokamak },       { {16, 15}, MapID::CommonStorage } };
    base.tubes     = { { {12, 10}, {16, 10} }, { {11, 11}, {11, 15} }, { {11, 15}, {16, 15} } };
    base.vehicles  = { { {13, 13}, MapID::ConVec } };
    createBase(you, base);

    op2::ignore(Game::forceMoraleGood());
    Game::addMessage("Welcome, commander.");
}

// --- called every few ticks during play ---
static void aiProc() {}

// --- guarded exports (a fault is logged, the game keeps going) ---
extern "C" __declspec(dllexport) int  InitProc() { crash::guard("InitProc", &initProc); return 1; }
extern "C" __declspec(dllexport) void AIProc()   { crash::guard("AIProc",   &aiProc); }
extern "C" __declspec(dllexport) void GetSaveRegions(mission::SaveRegion* p) { if (p) { p->pData = nullptr; p->size = 0; } }

extern "C" int __stdcall DllMain(void*, unsigned long reason, void*) {
    if (reason == 1 /*DLL_PROCESS_ATTACH*/) {
        crash::installHandler();
        log::setTickSource([] { return Game::tick(); });   // stamp every log line with the game tick
    }
    return 1;
}
```

A few things worth knowing:

- **Coordinates are visible tiles** - the same numbers you read off the in-game status bar.
- **A starting colony has to be viable.** Buildings other than power plants only work when tubed to the Command
  Center. Give the colony food (and a working Agridome), or the engine trims the population on the first tick and
  it then starves. Stored ore is capped by the colony's *active* storage, so ore needs a tubed Common Storage (or
  smelter) to stick - the capacity argument to `setCommonOre` alone isn't enough. `samples/Template` is a
  verified starting point.
- **Colony setup is fluent** - `goEden` / `setCommonOre` / `setWorkers` / ... return `Player&`, so they chain
  and never need a result check (setup can only fail on a bad player index, which is a safe no-op).
- **Orders return `[[nodiscard]] Result<void>`.** Check them: `if (auto r = unit.move({x, y}); !r) log::line(r.error().what);`.
  A failed order is a value, never a crash. For deliberate fire-and-forget, `op2::ignore(unit.move({x, y}));`.
- **Include `op2/trigger.hpp` in exactly one `.cpp`** - it defines the exported trigger-callback stubs.
- **The crash guard takes a plain function** (not a capturing lambda), so the SEH `__try` is valid.

## 5. Build

From the mission folder:

```
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
cmake --build build --config Release
```

`-A Win32` is required (32-bit). The result is `build/Release/cMyMission.dll`.

## 6. Deploy and run

Copy `cMyMission.dll` into your Outpost 2 / OPU install folder. Launch Outpost 2, start a new **Colony Game**,
and your mission appears in the list by its `LevelDesc`. Pick it and play.

The mission logs to `<OP2>\OPU\logs\cMyMission.log` (the name follows the DLL). Each line is stamped with the
game tick and flushed immediately, so if anything faults, the last "about to do X" line is on disk - that is how
you debug a mission.

## 7. Where to go next

- Read the [`samples/`](../samples/) - `ColdFront` (a scripted-AI single-player mission with a lava eruption and a
  starship victory) and `Nostalgia2P` (a 2-player multiplayer game) show most of the facade in real use.
- The full surface is organized into modules: unit orders, unit state, ranges enumeration, triggers + victory,
  AI groups, world & map, mission lifecycle, and the BaseBuilder. See the feature table in the
  [README](../README.md) and the design in [`FACADE-DESIGN.md`](FACADE-DESIGN.md).
- For multiplayer, set the mission type accordingly (e.g. `mission::MissionType::LastOneStanding`) and place a
  base per player - `createBase(player, layout, offset)` stamps the same layout at different corners. See
  `samples/Nostalgia2P`.
