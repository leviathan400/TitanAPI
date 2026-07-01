# Changelog

All notable changes to **Outpost 2 TitanAPI** are documented here. This is the public changelog; it begins at
the first public release.

## 0.6.5 - 2026-07-01 - One-call self-healing AI mining operation

### Added
- **`op2::createMiningOperation(owner, smelter, mine, idleTL, idleBR, numTrucks[, autoHeal])`** (new header
  `op2/mining.hpp`) - a complete, self-managing AI ore operation in a single call. It ensures a surveyed ore
  beacon and a Common Ore Mine at `mine`, a Common Ore Smelter at `smelter`, a `MiningGroup` hauling between them
  in the idle rect, and `numTrucks` Cargo Trucks on the haul. With `autoHeal` on (default), a per-frame
  **`op2::tickMiningOperations()`** call in AIProc keeps it alive: a destroyed mine or smelter is rebuilt (dozing
  the footprint first), lost trucks are replaced, the haul is rebound, and mining resumes on its own. Rebuilds use
  `createUnit` because the engine will not queue-build utility vehicles or complete a scripted ConVec build for an
  AI player without the full CanBuildUnit + research precondition chain (documented in the header). Verified
  in-game past mark 550, surviving scripted destruction of a truck and the smelter.

### Changed
- **Sample `samples/Mining` reworked to showcase `createMiningOperation`.** It now stands up the whole operation
  with the one call and stages a scripted "raid" (a truck, then the smelter) so you can watch auto-heal rebuild
  them and hauling resume. The previous 0.6.4 two-mine walkthrough is replaced; the `recordMine` / BuildingGroup
  mine pattern it demonstrated is preserved as a "Related" section in the sample README. Documents the key gotcha:
  a MiningGroup parks its trucks when the ore store is full, so the demo simulates colony consumption to keep ore
  demand up (a real colony spends ore as fast as it arrives).

## 0.6.4 - 2026-06-26 - Build a mine through a BuildingGroup

### Added
- **`Group::recordMine(Location beacon)`** - a documented, mine-specific way to have a BuildingGroup build an ore
  mine. It records `CommonOreMine` and lets the beacon under the location decide common vs rare (recording
  `RareOreMine` directly does not work - a long-standing OP2 engine quirk), and its docs cover the beacon +
  MiningGroup pairing. `recordBuilding` now points to it for mines.
- **New sample `samples/Mining`** (`cMining.dll`) - a Plymouth AI builds two ore mines side by side, showing both
  group types in their roles (human + AI): mine 1 is deployed by a Robo-Miner and hauled by a `MiningGroup`
  (`setupMining` + Cargo Trucks taken from the idle rect); mine 2 is built by a `BuildingGroup` via `recordMine`
  (the "build a mine through a building group" pattern - it needs a Robo-Miner, as a ConVec can't build a mine).
  Documents the gotchas - trucks must sit in the idle rect to bind, and a MiningGroup parks when the ore store is
  full (so the demo simulates colony consumption to keep demand up). Verified in-game to mark 200.

## 0.6.3 - 2026-06-24 - Typed unit facing + starship-victory sample

### Added
- **`Game::createUnit` and `BaseLayout` vehicles now take a `UnitDirection`** (`op2::UnitDirection`: `East=0` ..
  `NorthEast=7`) instead of a raw `int` rotation - type-safe facing:
  `createUnit(MapID::Lynx, at, you, MapID::Laser, UnitDirection::North)`.
- **`GameMap::unitOnTile(location)`** - returns the structure occupying a tile, read from the engine's per-tile
  MapUnit index (`TileData.unitIndex`). O(1) and footprint-aware (every tile a building covers reports it), so it
  is the right way to answer "what is built on this tile": `GameMap::unitOnTile(at).type() == MapID::Spaceport`.
  Returns a null Unit when no wall/building occupies the tile.
- **New sample `samples/Starship`** (`cStarship.dll`) - the Eden/Plymouth space-race victory system: a Spaceport
  colony whose objectives are to build and launch every starship component. Documents the working recipe (a
  per-component count trigger `oneShot=true` wrapped in a victory condition `oneShot=false`).

### Removed
- **`Player::clearStarshipState` / `Player::starshipFlags`** - these wrote/read `PlayerImpl+8` directly. The
  engine owns and recomputes that satellite/launch bitfield every cycle from the actual Spaceport, so writing it
  by hand makes the engine rebuild from a stale source and the starship objectives fire spuriously. Set the colony
  up the ordinary way (including `setTechLevel`, which initializes the space-program state) and leave `+8` alone.

## 0.6.2 - 2026-06-22 - Engine-fact corrections

Accuracy fixes to the `op2::abi` layer (the decoded view of the game binary). No public API surface change.

- Decoded two `CellTypeInfo` terrain fields - `blocksLineOfSight` (@0x18) and `minimapTopographicColor` (@0x20).
- Corrected `StartupFlags.moraleEnabled` -> `moraleSteady` (the flag's sense was reversed: set = morale is locked
  steady), and `MapObjectType.requiredTechID_` -> `requiredTechNum_`.
- Documented the `MapObjectType` table end (`0x4E1514`), confirming the 115-entry count.

## 0.6.1 - 2026-06-20 - Ergonomics & correctness pass

A hardening pass so real, non-trivial missions stay warning-free and readable, with a couple of genuine fixes
surfaced by review.

### Changed (small API refinements)

- **Colony setup is now fluent.** `Player::goEden` / `goPlymouth` / `goHuman` / `goAI` / `setPopulation` /
  `setCommonOre` / `setFood` / `setRareOre` / `setWorkers` / `setScientists` / `setKids` / `setTechLevel` now
  return `Player&` instead of `Result<void>`. Setup can only "fail" on an invalid player index (a programmer
  error, treated as a safe no-op), so there was no meaningful status to check - and discarding it on every setup
  line produced `[[nodiscard]]` warnings. Setup now chains and stays warning-free:
  `Game::player(0).goEden().setCommonOre(5000).setWorkers(20).setScientists(10);`
- **`Error::what` is now `const char*`** (was `std::string_view`). It always points at a static literal, and as
  a C string it drops straight into `printf`-family, `op2::log::line`, and `OutputDebugStringA` with no
  conversion.

### Added

- **`op2::ignore(...)`** - explicitly discard a `[[nodiscard]]` order `Result` when fire-and-forget is intended
  (e.g. an AI loop that issues an order every tick and doesn't branch on a momentary rejection). Makes intent
  visible while keeping an *accidental* discard a warning. Orders stay `[[nodiscard]]` and checkable by default.

### Fixed

- **Trigger callback pool raised from 16 to 64.** A non-trivial mission with many time / area / count / research
  triggers could silently exhaust the 16-slot dispatcher pool; the 17th trigger was lost. The pool is now 64
  (derived from the stub list, with a compile-time sync `static_assert` and a loud `OutputDebugString` warning if
  ever exhausted).
- **The README's headline example now compiles cleanly (0 errors, 0 warnings).** It previously referenced
  `log::line(error.what)` (a `string_view` that wouldn't pass to a `const char*` API) and omitted two includes.
- **`Player` readers on an out-of-range player index no longer fault.** `Player::impl()` computed
  `base + index*stride` for any index, so a reader like `Game::player(99).isHuman()` dereferenced a wild pointer
  (`0xC0000005`). `impl()` now range-checks the index first and returns null (the reader then yields a safe 0).

### Internal

- The in-game self-test still runs **155 checks** - the 13 setup checks were converted from "call returned OK"
  to value read-backs (stronger: they confirm the value actually landed), and all samples were swept clean of
  discarded-`Result` warnings.

## 0.6.0 - 2026-06-19 - First public milestone

The first public release of TitanAPI, a modern C++23 SDK for writing Outpost 2 missions. The library is
feature-complete for mission and AI scripting and has been verified in-game.

### What TitanAPI is

TitanAPI lets you script Outpost 2 colony, combat, and multiplayer missions in clean, modern C++ that builds
directly on the running game engine - no legacy SDK stack to wrangle. Orders are checked and return errors
instead of crashing the game, units and players are simple value handles, and a declarative BaseBuilder turns a
starting colony into a few lines of data.

### Capabilities at this milestone

- **Unit orders** - `move`, `attack`, `build`, `deploy`, `mine`, `dock`, `produce`, `doze`, `repair`,
  `dismantle`, `guard`, and more. Every order returns a `Result<void>` (a bad target is a value you can branch
  on, never a silent no-op or an engine crash) and dispatches through one validated command path.
- **Unit & player state** - live reads of any unit (type, owner, location, cargo, health, weapon,
  under-construction, ...) and any player (faction, resources, population, research, alliances, difficulty).
- **Enumeration** - composable `std::ranges` views over live units: `Game::units()`, `unitsOf(player)`,
  `unitsOfType(type)`, `unitsInRect(...)`.
- **Triggers & victory** - time / count / resource / research triggers with C++ lambda callbacks, plus
  `victoryWhen` / `defeatWhen` / `win` / `lose`.
- **AI scripting** - `FightGroup` / `MiningGroup` / `BuildingGroup` strategies, `UnitBlock` batch creation, and
  `Pinwheel` attack waves to script a computer opponent.
- **World & map** - disasters (meteor / quake / eruption / storm / vortex), lava control, terrain and cell
  queries, mining beacons, markers, sounds, and messages.
- **Mission lifecycle** - a typed entry contract, typed save/load, and the full `OnChat` / `OnSaveGame` /
  `OnEndMission` / ... callback set.
- **BaseBuilder** - a declarative `BaseLayout` stamped for a player with `createBase(player, layout, offset)`,
  plus tube/wall line helpers and map messages.

### Quality and tooling

- **100% functional parity with OP2Lua** across the game / mission / Unit / Player / Region surface, with AI
  scripting, the mission lifecycle, and the BaseBuilder added on top.
- **An in-game self-test of 155 checks** runs on every load (0 failures).
- **Crash diagnostics built in** - SEH guards, a process-wide fault filter, and a tick-stamped, flushed-per-line
  log turn "the game crashed" into a fault address and game tick you can act on.
- **Four sample missions** ship in `samples/`: `Layer1` (minimal first mission), `ColonyDemo`
  (orders / mining / enumeration), `ColdFront` (single-player scripted AI, a lava eruption, starship victory),
  and `Nostalgia2P` (2-player multiplayer, Last One Standing).

### Built on

Generated from facts about `Outpost2.exe` (a fixed 1997 binary) produced by the Outpost Universe community's
reverse-engineering. Clean room: no third-party SDK code is reused, only the facts. Builds as genuine C++23
(`/std:c++23preview`), 32-bit MSVC, static CRT.
