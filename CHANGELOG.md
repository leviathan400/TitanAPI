# Changelog

All notable changes to **Outpost 2 TitanAPI** are documented here. This is the public changelog; it begins at
the first public release. Versions follow [Semantic Versioning](https://semver.org).

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
