# Starship Victory - TitanAPI sample

A focused demo of the Eden/Plymouth **"space race" victory system**: a single Eden colony with a **Spaceport**,
where the mission objectives are to build and launch every starship component. Each objective turns green the
instant its component is launched; the mission is won when all are up.

Builds `cStarship.dll`, on the stock `cm01.map` + `MULTITEK.TXT` (present in a standard install).

## What it sets up

- **Player 0** - an Eden colony with full tech, large metal/food stockpiles, and a launch base: Command Center,
  **Spaceport**, two Tokamaks, Agridome, ore smelters, two Common + two Rare Metals storages, and a Structure
  Factory, plus a few ConVecs/Cargo Trucks - all tubed together so it runs from tick 0.
- **15 launch objectives** - one per starship component (Skydock, Command Module, Habitat Ring, Sensor Package,
  Fueling Systems, Stasis Systems, Orbital Package, Ion/Fusion Drive Modules, Phoenix Module, Rare/Common Metals
  Cargo, Food Cargo, Evacuation Module, Children Module). Win = all launched.

To try the win without launching all 15, lower `kObjectives` in `mission.cpp` (e.g. to 3) - then only the first
few components are required.

## How the victory system works (the recipe)

The engine tracks launched components as 1-bit flags in `PlayerImpl+8` (the dword that also holds the RLV / Solar
/ EDWARD satellite *counts* in its low bits). For these "space" map IDs a count trigger reads that launch bit, so
each objective is just:

```cpp
// count trigger oneShot=TRUE, victory condition oneShot=FALSE
victoryWhen(onUnitCount(0, type, MapID::None, Compare::GreaterEqual, 1, {}, /*oneShot=*/true),
            objectiveText, /*oneShot=*/false);
```

Two rules, both learned the hard way:

1. **The victory condition must be `oneShot=false`.** A one-shot victory condition is consumed when it fires, so
   the objective never stays green. (The *count* trigger is `oneShot=true`; only the victory wrapper is false.)
2. **Never write `PlayerImpl+8` yourself.** The engine owns it and recomputes it from the actual Spaceport every
   cycle - writing it makes the engine rebuild the field from a stale source and objectives fire spuriously.
   Initialize the colony the ordinary way (including `setTechLevel`, so the research/space-program state is set
   up) and the engine keeps every launch bit at 0 until a genuine launch.

## Build

```
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
cmake --build build --config Release
```

→ `build/Release/cStarship.dll`. Drop it into your Outpost 2 / OPU install; it appears as a Colony game
("TitanAPI Starship Victory") and logs to `<OP2>\OPU\logs\cStarship.log`. The log's `[progress t…]
launched=… green=…` lines mirror the objectives pane - all `1`s means every component is up and the mission is
won.
