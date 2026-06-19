# Nostalgia 2P - a TitanAPI multiplayer sample

A **2-player multiplayer (Last One Standing)** mission for Outpost 2, scripted in modern C++ with **TitanAPI** -
adapted from the classic C++ SDK "Nostalgia" mission. Two mirrored colonies start at opposite corners of
`opu_02.map`; the last player with a Command Center standing wins. A full mining-beacon field (base, side, and
centre deposits) fills the map.

It's the **multiplayer / BaseBuilder showcase**: the same relative-coordinate base layout is stamped at two map
corners via `createBase(player, layout, offset)` - exactly the offset-reuse the original SDK did with
`CreateBase(player, x, y, base)`, but as modern `op2::BaseLayout` data instead of C arrays + macros.

## What it demonstrates

| TitanAPI feature | Used for |
|---|---|
| **Multiplayer contract** | `DescBlock = { LastOneStanding, 2 players, … }`; `DescBlockEx` (0 multiplayer AIs - two humans) |
| **BaseBuilder offset reuse** (`createBase(player, layout, offset)`) | Two symmetric bases stamped at opposite corners from relative-coord layouts |
| **Beacon field** (`createMine`) | ~26 common/rare deposits at the original yields |
| **Factory stocking** (`setFactoryCargo`) | Each Structure Factory pre-loaded with expansion kits |
| **Player setup** (`setCommonOre`/`setWorkers`/`markResearchComplete`/`centerViewOn`) | Standard MP resources + opening research |
| **World setup** | Steady morale, full daylight, initial light level |

## How it maps from the old SDK

| Old C++ SDK (`BaseData.h` / `Main.cpp`) | TitanAPI |
|---|---|
| `struct BuildingInfo[] { dx, dy, mapId }` | `BaseLayout::buildings` - `{ {dx,dy}, MapID }` |
| `struct VehicleInfo[] { dx, dy, mapId, weapon, dir }` | `BaseLayout::vehicles` - `{ {dx,dy}, MapID, weapon, rotation }` |
| `struct TubeWallInfo[] { x1,y1,x2,y2 }` | `BaseLayout::tubes` - `{ {x1,y1}, {x2,y2} }` |
| `CreateBase(player, x, y, base)` | `createBase(player, layout, {x,y})` |
| `CreateBeacons(n, beacons)` | a loop of `Game::createMine(at, ore, yield)` |
| `factory.SetFactoryCargo(bay, kit, …)` | `Unit::setFactoryCargo(bay, kit, …)` |
| `Player[i].MarkResearchComplete(id)` | `Player::markResearchComplete(id)` |

## Build

```
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
cmake --build build --config Release
```

Output: `build/Release/cNostalgia2P.dll` - drop it into your OPU install (the leading `c` files it under the
multiplayer list). It logs to `OPU\logs\cNostalgia2P.log`. Requires `opu_02.map` (stock, loose) + `MULTITEK.TXT`.
Launch it from the **multiplayer** menu with two player slots.

> A 2-player subset of the original 4-corner "Nostalgia". Extending it to 3-4 players is just adding the other two
> corner layouts (top-right / bottom-left mirrors) and assigning players to corners.
