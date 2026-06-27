# AI Mining - TitanAPI sample

How to script a **working ore operation for an AI player**. You (a human) watch a Plymouth AI build **two ore
mines - one each way** - and haul from the first, all on its own (no human micro):
- **mine 1** the Robo-Miner way (survey -> deploy), then a `MiningGroup` hauls it;
- **mine 2** the BuildingGroup way (`recordMine`) - the *"build a mine through a building group"* pattern.

Builds `cMining.dll`, a 2-player Colony game (human + AI) on the stock `cm01.map` + `MULTITEK.TXT`.

## What it sets up

- **Player 0 (Eden, human)** - a token outpost to spectate from.
- **Player 1 (Plymouth, AI)** - a minimal colony: **Command Center, Tokamak** (power), **Agridome** (food),
  **Common Ore Smelter**, a **Robo-Surveyor**, two **Robo-Miners**, a **ConVec**, and three **Cargo Trucks**
  (placed at load inside the mining idle rect; the MiningGroup takes them in once mine 1 stands).

## The flow (each step gated on the previous, via the mark scheduler)

1. **Survey** - the AI's Robo-Surveyor drives to deposit 1; on arrival it is revealed.
2. **Mine 1 (Robo-Miner)** - once surveyed, the AI's Robo-Miner deploys a mine on deposit 1.
3. **Mine 2 (BuildingGroup)** - a `BuildingGroup` handed a ConVec + a Robo-Miner + a build rect builds a mine on
   deposit 2 via `recordMine` (a ConVec alone can't build a mine - the group needs a Robo-Miner).
4. **MiningGroup** - once mine 1 finishes, the AI wires up the haul group:

```cpp
auto mg = op2::createMiningGroup(aiPlayer);
mg.setupMining(mine, smelter, idleTL, idleBR);          // Setup FIRST (mine + smelter + an idle rect)
for (each Cargo Truck sitting in the idle rect) mg.takeUnit(truck);   // THEN take the trucks in
```

The group then hauls mine -> smelter on its own.

## Two things that bite

- **The trucks must be INSIDE the idle rect when the group takes them in.** This sample places them there at load;
  the campaign helper (`UnitHelper::SetupMiningGroup`) spawns them there fresh - either works. Trucks staged
  elsewhere are not reliably picked up.
- **A MiningGroup parks its trucks in the idle rect when the ore store is FULL** (unlike a human's `Unit::mine`
  CargoRoute, where a truck waits at the smelter *dock*). So the colony needs ongoing ore DEMAND: this sample
  simulates consumption (it drains the AI's ore every few marks), so the store never fills and the trucks haul
  indefinitely - exactly what a real AI does by spending ore on construction/production. (Aside: `setCommonOre`'s
  cap argument does **not** raise the storage cap - the engine recomputes capacity from storage buildings - so
  adding CommonStorage only delays the fill, it does not prevent it.)

## Mine 2 - building a mine through a BuildingGroup

Mine 2 is built the *"build a mine through a building group"* way: hand a **BuildingGroup** its builders + a build
rect, then `recordMine`, and the group builds a `CommonOreMine` on the surveyed deposit on its own:

```cpp
auto bg = op2::createBuildingGroup(aiPlayer);
bg.takeUnit(conVec);               // a BuildingGroup builds nothing without builders it owns...
bg.takeUnit(roboMiner);            //   ...and a mine specifically needs a Robo-Miner (a ConVec alone won't do it)
bg.setBuildRect(rectTL, rectBR);   // the area it may build in
bg.recordMine(beacon);             // ALWAYS CommonOreMine - the beacon under it decides common vs rare
```

`recordMine` always records a `CommonOreMine`; the beacon decides common vs rare (recording `RareOreMine` directly
does not work - an OP2 engine quirk). The mine must already have a surveyed beacon under it. See `Group::recordMine`
in `op2/groups.hpp`. (A full BuildingGroup with a Structure/Vehicle Factory would also produce its own ConVecs +
Robo-Miners and build a whole base - this sample just hands it the two units it needs for the one mine.)

## Build

```
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
cmake --build build --config Release
```

→ `build/Release/cMining.dll`. Drop it into your Outpost 2 / OPU install; it appears as a Colony game
("TitanAPI AI Mining") and logs to `<OP2>\OPU\logs\cMining.log`. The log traces `SURVEYED` -> `mine BUILT` ->
`mining group … (3 trucks)` -> `[mark N] AI … ore=…` climbing as the group hauls.
