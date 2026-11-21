# Rising From The Ashes (TitanAPI port)

A full TitanAPI re-implementation of Vagabond's Outpost 2 colony/space-race mission **Rising From The Ashes**
(originally written against the classic OP2MissionSDK). It is the most complex sample - a real campaign mission
that exercises the whole facade end to end. Builds to `cRisingAshes.dll`.

> **Credit:** the original mission and its design are by **Vagabond** (Outpost Universe). This port reproduces its
> logic on TitanAPI; the map (`Ashes.map`) and tech tree (`multitekNoLavaWalls.txt`) are the original's.

## The mission

You command the **Eden** colony (player 0) at the **north** end of the tall Haise Valley. The planet can no
longer sustain you - your only way out is to **build and launch an evacuation starship** (a Skydock plus a
transport carrying 200 colonists and 10,000 each of food / common / rare metals) before your resources run dry.

A **Plymouth AI** (player 1) holds a richer outpost in the **south**. It stays passive **until provoked** -
attack its southern outpost and the war opens. From then on it raids you on a timer, and the waves **escalate
with your progress**:

- **build a Spaceport** -> the raids step up to the Middle tier (Panthers, Supernova stealth Lynx, big Scorpion
  swarms), plus a dedicated 10-Panther strike,
- **research the Phoenix Module** -> the raids reach the End tier (Tiger columns), plus a 14-Tiger strike.

Meanwhile a **disaster drizzle** (meteors, quakes, lightning storms, valley tornadoes) builds toward the
guaranteed late-mission **eruption of Mount Swigert** at (50,125).

**Win:** complete all five starship objectives. **Lose:** let the colony become non-viable (too few colonists, no
way to rebuild a Command Center, or the space / Phoenix-Module tech path is lost) for 90 ticks.

## What it shows off (facade coverage)

- **BaseBuilder** - both colonies stamped from `BaseLayout` + direct placements.
- **AI ScGroups** - `FightGroup` raid waves and the standing base defense, `MiningGroup` haul routes, and a
  `BuildingGroup` that rebuilds the AI's base over time and reinforces lost units.
- **Triggers + victory** - the five starship objectives via `victoryWhen(onUnitCount(...))`.
- **World & map** - the full disaster set (`createMeteor` / `createEarthquake` / `createStorm` / `createVortex`)
  and the volcano (`createEruption`), plus daylight control and mining beacons.
- **Orders & state** - per-cycle AI micro: repair, retreat-when-threatened, reinforcement drops, provocation
  detection - all driven from `AIProc` with checked orders (and `op2::ignore` for fire-and-forget).
- **Player tech** - `hasTechnology` / `canDoResearch` / `canBuildSpace` gate the escalation and the defeat check.

It also demonstrates a **clean port pattern**: the original's engine time-triggers become a small tick scheduler
in `AIProc`; see the PORT NOTES at the top of `mission.cpp` for every adaptation (the only omissions are
cosmetic-only original bits with no facade equivalent: fumarole vents, the lava-flow animation tiles, the
Win32/odasl briefing dialog, and the music playlist).

## Build & run

```
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
cmake --build build --config Release
```

Copy `build/Release/cRisingAshes.dll` into your Outpost 2 / OPU install, along with **`Ashes.map`** and
**`multitekNoLavaWalls.txt`** (both ship in the original mission's repo). Start a new **Colony Game** and pick
"Colony Builder - Eden, Rising From the Ashes". It logs to `<OP2>\OPU\logs\cRisingAshes.log`.
