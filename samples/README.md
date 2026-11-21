# TitanAPI Samples

Complete, in-game-verified missions you can read, build, and adapt. Each is self-contained: it `#include`s the
library from `../../TitanAPI/include`, so it always builds against the current `op2::` API. New to TitanAPI?
Start with [`../docs/GETTING-STARTED.md`](../docs/GETTING-STARTED.md).

**Starting your own mission?** Copy [`Template/`](Template/) - a clean, minimal mission that builds and runs
as-is, with the structure laid out and marked for you to fill in.

| Sample | Builds | What it shows |
|---|---|---|
| [`Template/`](Template/) | `cTemplate.dll` | A copy-to-start skeleton: the exports, `InitProc`, `AIProc`, and crash/log setup, with TODO markers. The recommended starting point for a new mission. |
| [`Layer1/`](Layer1/) | `cLayer1.dll` | A minimal first mission - sets up a colony and places units directly through Layer 1 (`op2::abi`). The smallest thing that loads and runs. |
| [`SmokeTest/`](SmokeTest/) | `cTitanSmokeTest.dll` | The in-game smoke test, a copy of the library's test mission: a two-player colony exercising every module, a tick-3 self-test of every facade function (`SELF-TEST: n PASSED`), and tick-2500 checks that the orders actually completed (`OUTCOME CHECKS: n PASSED`). Used to validate every build. |
| [`Mining/`](Mining/) | `cMining.dll` | A human spectator watches a Plymouth AI run a complete ore operation set up with **one call** - `createMiningOperation(...)` (mine + smelter + hauling **MiningGroup** + Cargo Trucks). A scripted "raid" destroys a truck and then the smelter so you can watch **auto-heal** rebuild them (`tickMiningOperations()` in `AIProc`) and mining resume on its own. Covers the key gotcha: a MiningGroup parks its trucks when the ore store is full, so the demo simulates colony consumption to keep ore demand up. |
| [`ColdFront/`](ColdFront/) | `cColdFrontTitan.dll` | A single-player mission with a scripted AI opponent, a small `AIProc` scheduler, an erupting volcano (lava disaster), and a starship-evacuation victory. Dogfoods most of the facade in one mission. |
| [`Nostalgia2P/`](Nostalgia2P/) | `cNostalgia2P.dll` | A 2-player multiplayer (Last One Standing) mission - the BaseBuilder + multiplayer showcase: one `BaseLayout` stamped at two corners via `createBase(player, layout, offset)`. |
| [`Starship/`](Starship/) | `cStarship.dll` | The Eden/Plymouth **space-race victory system**: a Spaceport colony whose objectives are to build + launch every starship component. Shows the count-trigger + victory-condition recipe (and the "never write PlayerImpl+8" rule). |
| [`Population/`](Population/) | `cPopulation.dll` | A **population-goal** mission: grow the colony to 150 colonists to win. Shows a custom victory condition driven by the engine's population trigger (`onResource(Resource::Colonists, ...)`) - a single pending->green objective. |
| [`RisingFromTheAshes/`](RisingFromTheAshes/) | `cRisingAshes.dll` | **The most complex sample** - a full port of Vagabond's campaign mission: a provocation-gated Plymouth AI with escalating raid tiers + milestone strikes, AI base-building / mining / repair / reinforcement groups, the full disaster set + a volcano, and a 5-objective starship victory. Exercises the whole facade. (Needs `Ashes.map` + `multitekNoLavaWalls.txt`.) |

## Building a sample

Outpost 2 is a 32-bit process, so samples build as 32-bit MSVC / C++23:

```
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
cmake --build build --config Release
```

Run it from each sample's folder. The output `c<Name>.dll` drops into your Outpost 2 install and appears in the
game's mission list. See each sample's own `README.md` for specifics.
