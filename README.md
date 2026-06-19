# Outpost 2 TitanAPI

**A modern C++23 SDK for writing Outpost 2 missions.**

TitanAPI lets you script Outpost 2 colony, combat, and multiplayer missions in clean, modern C++ - building
straight on the running game engine, with no legacy SDK stack to wrangle. Orders are checked and return errors
instead of crashing the game, units and players are simple value handles, and a declarative BaseBuilder turns a
starting colony into a few lines of data.

> **Status: v0.6.0 - feature-complete and verified in-game.** Eight modules cover the full
> mission-scripting surface (unit orders, state, enumeration, triggers + victory, AI groups, world & map,
> mission lifecycle, base building). Every build is validated by a 155-check in-game self-test, and four sample
> missions (single-player and multiplayer) ship in `samples/`.

---

## Why TitanAPI

- **Modern C++23.** `std::expected`-based error handling, `std::ranges` views over live units, `std::span`,
  deducing-this, designated initializers - the language as it is in 2026, not 2005.
- **Orders are safe.** Every unit order returns a `Result<void>`: a bad target or an off-map tile comes back as
  a typed error you can branch on, instead of a silent no-op or an engine crash. All orders dispatch through one
  validated command path.
- **It tells you when it breaks.** Built-in crash diagnostics (SEH guards + a process-wide fault filter + a
  tick-stamped, flushed-per-line log) turn "the game crashed" into "fault 0xC0000005 at 0x4367DA, tick 2" - a
  problem you can actually fix.
- **It's complete.** Unit orders, live state reads, ranges enumeration, the trigger/victory system, AI scripting
  (fight/mining/building groups, unit blocks, wave attacks), disasters and terrain, typed save/load, and a
  declarative base builder - all verified against the real engine.
- **Clean room.** Built from facts about the game binary (addresses, struct offsets) produced by the Outpost
  Universe community's reverse-engineering. No copied code.

---

## A mission in a few lines

```cpp
#include "op2.hpp"          // the TitanAPI facade
#include "op2/trigger.hpp"  // triggers + win/lose

using namespace op2;

static void initProc() {
    Player you = Game::player(0);
    you.goEden();
    you.setCommonOre(5000);
    you.setWorkers(20);
    you.setScientists(10);

    // Stamp a starting colony from a declarative layout (coordinates are visible tiles).
    BaseLayout base;
    base.buildings = { { {10, 10}, MapID::CommandCenter }, { {14, 10}, MapID::Tokamak },
                       { {10, 14}, MapID::Agridome } };
    base.vehicles  = { { {12, 13}, MapID::Lynx, MapID::Laser }, { {13, 13}, MapID::ConVec } };
    base.tubes     = { { {10, 12}, {14, 12} } };
    createBase(you, base);

    // Orders return Result<T> - failures are values, not crashes. Enumerate units with C++23 ranges.
    for (Unit lynx : Game::unitsOfType(MapID::Lynx))
        if (auto r = lynx.move({ 40, 40 }); !r)
            log::line(r.error().what);

    // Victory / defeat one-liners.
    loseIfNoCommandCenter(0);                          // you lose if YOUR CC falls
    winWhenColonyDestroyed(1, "Destroy the enemy");    // win when player 1 is wiped out

    Game::addMessage("Good luck, commander.");
}

// The mission DLL exports the engine calls (each guarded so a fault is logged, not silent).
extern "C" __declspec(dllexport) int  InitProc() { crash::guard("InitProc", &initProc); return 1; }
extern "C" __declspec(dllexport) void AIProc()   {}
```

See [`samples/`](samples/) for complete, in-game-verified missions, and
[`docs/GETTING-STARTED.md`](docs/GETTING-STARTED.md) for a step-by-step walkthrough.

---

## Features

| Area | What you get |
|---|---|
| **Unit orders** | `move`, `attack`, `build`, `deploy`, `mine`, `dock`, `produce`, `doze`, `salvage`, `removeWall`, `repair`, `dismantle`, `guard`, ... - all `Result<void>`, all through one validated command-packet path |
| **Unit state** | `isLive`, `type`, `owner`, `location`, `cargo`, `health`, `weapon`, `underConstruction`, `isFactory`, ... read live from the engine; works on any unit |
| **Enumeration** | `Game::units()` / `unitsOf(p)` / `unitsOfType(t)` / `unitsInRect(...)` as composable `std::ranges` views |
| **Triggers & victory** | time/count/resource/research triggers with C++ lambda callbacks; `victoryWhen` / `defeatWhen` / `win` / `lose` |
| **AI scripting** | `FightGroup` / `MiningGroup` / `BuildingGroup` strategies, `UnitBlock` batch creation, `Pinwheel` attack waves |
| **World & map** | disasters (meteor/quake/eruption/storm/vortex), lava control, terrain & cell queries, mining beacons, markers, sounds, messages |
| **Mission lifecycle** | typed entry contract, typed save/load (`GetSaveRegions` + a `Stream` wrapper), `OnChat` / `OnSaveGame` / `OnEndMission` / ... callbacks |
| **BaseBuilder** | declarative `BaseLayout` -> `createBase(player, layout, offset)`; tube/wall line helpers; map messages |
| **Player** | faction/human/AI, resources & population, research, alliances, difficulty, satellite counts, center-view |
| **Tooling** | a 155-check in-game self-test; crash diagnostics; tick-stamped logging |

---

## Getting started

```
cmake -S TitanAPI -B TitanAPI/build -G "Visual Studio 18 2026" -A Win32
cmake --build TitanAPI/build --config Release
```

Outpost 2 is a 32-bit process, so missions build as **32-bit MSVC / C++23** (`/std:c++23preview`, static CRT).
The output is a `c<Name>.dll` you drop into your Outpost 2 install; it appears in the game's mission list.

Full walkthrough - prerequisites, your first mission, building, deploying, and running - is in
**[`docs/GETTING-STARTED.md`](docs/GETTING-STARTED.md)**.

---

## Repository layout

```
op2titanapi/
  README.md / CHANGELOG.md   this overview and the changelog
  TitanAPI/        the library: op2:: facade headers (include/op2/) + the test mission (src/)
  samples/         complete, in-game-verified missions you can read and adapt
    Layer1/        a minimal first mission
    ColonyDemo/    orders, mining, enumeration
    ColdFront/     a single-player mission with a scripted AI, a lava eruption, and a starship victory
    Nostalgia2P/   a 2-player multiplayer (Last One Standing) mission
  docs/            GETTING-STARTED.md, FACADE-DESIGN.md (the design), ABI-MECHANISM.md (how the layer reads the game)
```

Include the whole facade with a single `#include "op2.hpp"`; add `op2/trigger.hpp` for triggers/victory and
`op2/base.hpp` for the BaseBuilder.

---

## Credits

TitanAPI is generated from **facts about `Outpost2.exe`** - a fixed 1997 binary - which are the product of the
Outpost Universe community's reverse-engineering work going back many years. Thanks to the OPU community for the
RE that makes a modern SDK possible.

Namespace: `op2`.
