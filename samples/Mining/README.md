# AI Mining - TitanAPI sample

A complete, self-managing **ore operation for an AI player in one call**. A human spectator watches a Plymouth AI
run a full mine - and heal it when it is attacked - all on its own (no human micro), set up with a single function:

```cpp
op2::createMiningOperation(ai, smelterLoc, mineLoc, idleTL, idleBR, /*numTrucks*/ 3, /*autoHeal*/ true);
```

That one call places the ore mine (surveying a beacon first), the smelter, a `MiningGroup` hauling between them,
and the Cargo Trucks. Then a single line in `AIProc` keeps it alive:

```cpp
op2::tickMiningOperations();   // rebuilds a destroyed mine/smelter, replaces lost trucks, resumes the haul
```

Builds `cMining.dll`, a 2-player Colony game (human spectator + AI) on the stock `cm01.map` + `MULTITEK.TXT`.

## What it sets up

- **Player 0 (Eden, human)** - a token outpost to spectate from (Command Center + Tokamak + a Scout, high food /
  tiny population so it stays stable without farms).
- **Player 1 (Plymouth, AI)** - a colony **core** only: Command Center, Tokamak (power), Agridome (food), tube-
  connected. The mine, smelter, MiningGroup, and trucks are **all added by the one `createMiningOperation` call** -
  that is the point of the sample.

## The demo: auto-heal under attack

To show the self-healing off, the mission stages a scripted "raid" and you watch the operation recover:

1. **mark 50** - a Cargo Truck is destroyed. `tickMiningOperations()` builds a replacement and puts it back on the
   haul.
2. **mark 110** - the smelter is destroyed. The tick dozes the footprint, rebuilds the smelter, rebinds the
   MiningGroup to it, and hauling resumes.

The log traces `mine=1 smelter=1 trucks=3 ore=...` holding steady through both raids as the operation repairs
itself.

## Two things worth knowing

- **A MiningGroup parks its trucks when the ore store is FULL** (unlike a human's `Unit::mine` CargoRoute, where a
  truck waits at the smelter *dock*). So the colony needs ongoing ore **demand**: this sample simulates
  consumption (`if (ore > 8000) setCommonOre(6000)` in `AIProc`), so the store never caps and the trucks haul
  indefinitely - exactly what a real AI does by spending ore on construction/production. Without a consumer the
  trucks would look "stopped" once the store fills. (`setCommonOre`'s value does not raise the storage cap - the
  engine recomputes capacity from storage buildings - so adding CommonStorage only delays the fill.)
- **Rebuilds use `createUnit`, not factory production.** The engine will not queue-build utility vehicles (Cargo
  Truck / ConVec) or complete a scripted ConVec structure-build for an AI player without the full CanBuildUnit +
  research + factory-output precondition chain the campaign AI uses. `createUnit` is the reliable path for
  auto-heal and is functionally identical (the unit appears and joins the operation). See the header
  `op2/mining.hpp` for the full note.

## Related: build a mine through a BuildingGroup (`recordMine`)

`createMiningOperation` places its mine directly. If instead you want an AI's **BuildingGroup** to construct a
mine as part of a base (its factories producing the builders), use `Group::recordMine`:

```cpp
auto bg = op2::createBuildingGroup(aiPlayer);
bg.takeUnit(conVec);               // a BuildingGroup builds nothing without builders it owns...
bg.takeUnit(roboMiner);            //   ...and a mine specifically needs a Robo-Miner (a ConVec alone won't do it)
bg.setBuildRect(rectTL, rectBR);   // the area it may build in
bg.recordMine(beacon);             // ALWAYS CommonOreMine - the beacon under it decides common vs rare
```

`recordMine` always records a `CommonOreMine`; the beacon decides common vs rare (recording `RareOreMine` directly
does not work - an OP2 engine quirk). A surveyed beacon must already exist under the location. See
`Group::recordMine` in `op2/groups.hpp`.

## Build

```
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
cmake --build build --config Release
```

-> `build/Release/cMining.dll`. Drop it into your Outpost 2 / OPU install; it appears as a Colony game
("TitanAPI AI Mining") and logs to `<OP2>\OPU\logs\cMining.log`.
