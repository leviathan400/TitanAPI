# TitanAPI - the library

The TitanAPI library + main dev folder. A modern C++23 library for Outpost 2 missions, built on its own
reverse-engineering substrate. (Project overview, roadmap, and changelog are at the repo root.)

## Two layers

- **Layer 1 - `op2::abi`** *(done, verified in-game)*: relocates fixed `Outpost2.exe` addresses and calls
  into them (free / `__thiscall` thunks, global reads, struct-field access). Headers under `include/op2/abi/`
  are **generated** from the maintainer's internal RE data by `tools/gen-*.ps1`; `memory.hpp` is hand-written.
- **Layer 2 - the facade** *(Modules 1-8 all complete, verified in-game; 100% OP2Lua functional parity,
  155-check in-game self-test)*: an ergonomic, `std::expected`-based API under `include/op2/`: `Result<T>`
  / `Error` (`core/error.hpp`), `Location` (`core/location.hpp`, visible-tile coords), and value-handle `Unit` /
  `Player` / `Game`. Include it all via `include/op2.hpp`.
  - **Module 1 - orders**: `Unit::move / attack / guard / repair / produce / build / deploy / mine / dock / doze
    / salvage / removeWall / stop / transfer / …` → `Result<void>`, each built as a validated `CommandPacket`
    (`abi/cmd_builder.hpp` + `abi/raw/command.hpp`) dispatched through one path (`Player::issue` →
    `ProcessCommandPacket`). The raw per-unit `Cmd*` thunks stay sealed in `op2::abi` - designing out earlier mission SDKs'
    order-API bug class.
  - **Module 2 - unit state**: `Unit::isLive / ownerId / type / cargo / damage / location / health /
    underConstruction / …` read the live `MapObject` (`abi/map_object.hpp`), so they work on any unit. Owner is
    resolved from the object, so orders work on enumerated units too.
  - **Module 3 - enumeration**: C++23 ranges over live units - `Game::units() / unitsOf(p) / unitsOfType(t) /
    unitsInRect(...)`, composable with `std::views`.
  - **Module 4 - triggers + victory**: `op2::onTick / onMark / onBuildingCount / …` with an engine→C++
    name→callback registry, and `op2::win / lose` (`trigger.hpp`). Verified ending the mission in-game.
  - **Module 5 - AI ScGroups** (`groups.hpp`): `FightGroup` / `MiningGroup` / `BuildingGroup` strategy ops,
    `UnitBlock` batch creation, and the `Pinwheel` wave-attack coordinator - script a computer opponent.
  - **Module 6 - world & map**: `Game` disasters / sounds / markers / blight / wreckage and `GameMap` tile &
    terrain queries.
  - **Module 7 - mission lifecycle** (`src/op2_mission.hpp`): the typed entry contract, typed save/load
    (`GetSaveRegions` + `op2::mission::Stream`), and the `OnSaveGame` / `OnChat` / `OnEndMission` / … callbacks.
  - **Module 8 - BaseBuilder** (`base.hpp`): declarative base layout - `BaseLayout` → `createBase(owner, …)` -
    plus tube/wall line helpers, map messages, and campaign victory one-liners.

  Design rationale: `../docs/FACADE-DESIGN.md`.

## Folder contents

```
TitanAPI/
├─ CMakeLists.txt          32-bit / MSVC / C++23 (/std:c++23preview), static CRT -> the test-mission DLL
├─ include/
│  ├─ op2.hpp              umbrella - include this to get the Layer 2 facade
│  └─ op2/
│     ├─ core/            error.hpp (Result/Error), location.hpp (visible-tile coords + pad)
│     ├─ game.hpp player.hpp unit.hpp   the facade handles (Game/Player/Unit + orders & state)
│     └─ abi/             Layer 1: memory.hpp (hand-written) + generated addresses/enums/command_map;
│                         hand-written cmd_builder.hpp, map_object.hpp, raw/command.hpp
├─ tools/                 gen-*.ps1 - regenerate the op2/abi/* headers from internal RE data (maintainer-only)
└─ src/                   the current test mission (built on the facade)
```

## Build the test mission

**32-bit / MSVC / C++23** (Outpost 2 is a 32-bit process):

```
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
cmake --build build --config Release
```
→ `build/Release/cTitanAPI.dll`. Drop it into your OPU `maps\` folder; it appears as a Colony game ("TitanAPI
Test Mission") and logs to `<OP2>\OPU\logs\cTitanAPI.log`. The frozen Layer-1-only version of this mission
lives at `../samples/Layer1` (`cLayer1.dll`).

## Regenerate the Layer 1 headers (maintainer-only)

The `op2/abi/*` address/enum/struct headers are committed, so building never needs regeneration. They are
produced from the maintainer's internal RE data by the `tools/gen-*.ps1` scripts; do not hand-edit the
generated headers. See `../docs/ABI-MECHANISM.md` for how `op2::abi` reads the game.
