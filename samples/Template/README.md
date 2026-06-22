# Mission Template

A clean, minimal TitanAPI mission to copy and start from. It builds and runs as-is (an Eden colony on
`on1_01.map` with a small starting base), then you replace the marked pieces with your own.

TitanAPI is header-only, so there is nothing to link - the mission just `#include`s the `op2::` facade and
builds to a single `c<Name>.dll` you drop into your Outpost 2 install.

## Use it

1. **Copy** this `Template/` folder and rename it to your mission (e.g. `samples/MyMission/`).
2. **Rename the target:** in `CMakeLists.txt`, change every `Template` / `cTemplate` to your name
   (e.g. `MyMission` / `cMyMission`). The leading `c` files it under Colony Games in-game.
3. **Edit `mission.cpp`** - the four numbered sections walk you through it:
   - **1. Exports** - `LevelDesc`, `MapName`, `TechtreeName`, `DescBlock` (mission type / players / tech).
   - **2. `InitProc`** - set up the world: player faction, resources, your starting base, opening orders, the
     win/lose condition.
   - **3. `AIProc`** - per-frame logic (an AI opponent, scripted events, progress checks). Empty by default.
   - **4. Exports + DLL setup** - guarded entry points, save-game hook, crash handler + tick-stamped logging.
     You normally do not edit this part.

## Build

```
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
cmake --build build --config Release
```

`-A Win32` is required (Outpost 2 is a 32-bit process). The result is `build/Release/cTemplate.dll`. Copy it
into your Outpost 2 / OPU install, start a new Colony Game, and pick your mission by its `LevelDesc`. It logs to
`<OP2>\OPU\logs\cTemplate.log` (tick-stamped and flushed per line, so a crash leaves its last step on disk).

## What the scaffolding files are

- `op2_mission.hpp` - the mission-DLL export contract (`DescBlock` / save types).
- `op2_log.hpp` - the file logger (`op2::log::line` / `linef`).
- `op2_crash.hpp` - SEH guards + the unhandled-fault handler.

These three ship with every sample. The actual API (`Game` / `Player` / `Unit` / orders / triggers / BaseBuilder)
comes from the live library at `../../TitanAPI/include`. New to TitanAPI? Read
[`../../docs/GETTING-STARTED.md`](../../docs/GETTING-STARTED.md).
