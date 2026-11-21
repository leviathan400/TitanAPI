# Population Goal - TitanAPI sample

A simple colony-growth mission: **grow your Eden colony to 150 colonists to win.** It demonstrates a custom
victory condition driven by the engine's population trigger - a single objective that turns green and wins the
moment the colony reaches the target.

Builds `cPopulation.dll`, on the stock `cm01.map` + `MULTITEK.TXT` (present in a standard install).

## What it sets up

- **Player 0** - an Eden colony pre-built for growth: power (Tokamaks), food (Agridomes), housing (Residences),
  and the people-growth buildings - a **Nursery** (children are born), a **University** (children become workers
  / scientists), a **Medical Center** (fewer deaths), and a **Recreation Facility** (morale). Morale is forced
  good so births outpace deaths and the colony actually grows. Starts at ~100 colonists.
- **One objective** - "Grow the colony to 150 colonists." Win when reached.

## How the objective works

The win is a real engine trigger, not a polled check - so the objective shows red and turns green on its own:

```cpp
victoryWhen(onResource(Resource::Colonists, Compare::GreaterEqual, 150, {}, /*player=*/0, /*oneShot=*/true),
            "Grow the colony to 150 colonists", /*oneShot=*/false);
```

`onResource(Resource::Colonists, ...)` wraps the engine's `CreateResourceTrigger` on the Colonists resource
(workers + scientists + children). As with every persistent objective, the **victory condition is `oneShot=false`**
(a one-shot victory condition is consumed when it fires and would not stay green).

To test the win without waiting for a full colony to grow, lower **`kPopulationGoal`** in `mission.cpp` (e.g. to
just above the starting population).

## Build

```
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
cmake --build build --config Release
```

→ `build/Release/cPopulation.dll`. Drop it into your Outpost 2 / OPU install; it appears as a Colony game
("TitanAPI Population Goal") and logs to `<OP2>\OPU\logs\cPopulation.log`. The log's `[pop t…] colonists=N / 150`
lines track the climb toward the goal.
