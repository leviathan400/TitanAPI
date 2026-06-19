# Cold Front - a TitanAPI sample mission

A single-player **Colony** mission for Outpost 2, scripted in modern C++ with **TitanAPI** - a port of OP2Lua's
"Cold Front" reference mission. You build an Eden colony on `on1_01.map` next to a scripted **Plymouth AI**
while a "dormant" volcano wakes and floods the lowlands with lava. Survive the eruption and **evacuate to the
starship** to win; you lose if your Command Center falls. A roaming green-Plymouth raiding party crosses the map
twice.

This is the **showcase sample** for the TitanAPI Layer 2 facade - one real mission that exercises most of it.

## What it demonstrates

| TitanAPI feature | Used for |
|---|---|
| **BaseBuilder** (`op2::BaseLayout` / `createBase`) | Both colonies' starting layout, as data |
| **Scheduler** (`atMark` / `everyMarks` / `when`) | A ~40-line mark-based event loop over `AIProc` - the C++ counterpart of OP2Lua's `at_mark`/`every`/`when` |
| **Mining orders** | The AI surveys → deploys → routes trucks to its 2-bar deposit |
| **ScGroup patrol** (`createFightGroup` + `setPatrolMode`) | The AI's RPG-Lynx fighting patrol loops the lowlands |
| **Structure building** (`setKit` + `findBuildSite` + `build`) | A ConVec expands the AI base - smelter, residences, agridomes, a Tokamak, labs |
| **Production** (`Unit::produce`) | The Vehicle Factory turns out reinforcements over time |
| **Disasters / lava** (`setLavaPossible` + `createEruption` + `setLavaSpeed`) | The volcano erupts and floods the dark lava-rock terrain |
| **Victory / defeat** (`satelliteCount` + `op2::win` / `playerOwnsAny` + `op2::lose`) | Starship evacuation win; lose-if-no-CC |
| **Messages / morale** | Mission narration and steady morale |

## Players (3)

| # | Player | Role |
|---|---|---|
| 1 | Eden (you, blue) | Build, survive the eruption, evacuate to the starship. |
| 2 | Plymouth (red, AI) | A scripted computer base - mining, a patrol, production. |
| 3 | Plymouth (green, AI) | No base; two scripted raiding parties that cross the map and leave. |

## Objective

Evacuate **10000 Rare Metals, 10000 Common Metals, 10000 Food, and 200 Colonists** to the starship. You **lose**
if you have no Command Center left (the eruption can destroy it - rebuild before your last one falls).

## Build

```
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
cmake --build build --config Release
```

Output: `build/Release/cColdFrontTitan.dll` - drop it into your OPU install (the leading `c` files it under
Colony Games). It logs to `OPU\logs\cColdFrontTitan.log`. Requires `on1_01.map` (stock, loose) + `MULTITEK.TXT`.

> The output is **`cColdFrontTitan.dll`**, not `cColdFront.dll`, so it can coexist with the OP2Lua "Cold Front"
> sample (which builds a `cColdFront.dll`). In-game it appears as **"Cold Front (TitanAPI)"**.

### Logs

Two files in `OPU\logs\` (the names follow the `-DOP2LOG_NAME` build define):

- **`cColdFrontTitan.log`** - the main mission log (setup, victory/defeat, faults).
- **`cColdFrontTitan-AI.log`** - a dedicated **AI-action channel** (the Plymouth bot's resource dump, mining,
  patrol, production, raiders) - the same monitor/debug split OP2Lua uses, via `op2::log::ai(...)`.

## Relation to the OP2Lua original

This is a faithful adaptation, not a line-for-line clone. The starting layout (incl. the AI's tube network), the
lava sequence, the raiders, and the victory/defeat conditions match the OP2Lua mission. The Plymouth AI mines,
patrols, produces, **and expands its base** (a ConVec builds new structures over time). It is **streamlined**:
the build pipeline runs one ConVec sequentially through a fixed plan, rather than reproducing the original's full
multi-ConVec, Structure-Factory-dock-juggling economy (that hand-tuned 700-line brain is the OP2Lua reference's
domain). The point here is to show TitanAPI driving a real mission cleanly, not to re-tune an AI line by line.
