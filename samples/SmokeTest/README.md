# SmokeTest - the TitanAPI in-game smoke test

The mission used to validate every build of the library: a full colony that exercises the Layer 2 facade
end to end and checks it in two phases. It is a copy of the library's own test mission
(`TitanAPI/src/mission.cpp`), kept here so anyone can drop it in and confirm a build is healthy.

On `cm01.map` with two players (human Plymouth + an AI Eden) it exercises:

- **Player setup** - faction / human / AI / population / resources / tech, steady morale, alliances.
- **Unit orders (Module 1)** - move, attack, build, mine, deploy, patrol, dock, produce, and an **EMP-capture**
  (EMP Lynxes → `isEMPed()` → a Spider reprograms the enemy Lynx → you take ownership).
- **Unit state (Module 2)** - `isLive` / `ownerId` / `type` / `location` / `cargo` / `isEMPed` / ... reads.
- **Enumeration (Module 3)** - `Game::units()` / `unitsOf()` as C++ ranges.
- **Triggers and victory (Module 4)** - a defeat condition, `onMark` / `onTick` time triggers with C++ callbacks,
  and the `op2::win()` victory path.
- **AI groups (Module 5)** - an AI FightGroup that hunts the human player, plus Building/Mining groups and a
  Pinwheel in the self-test.
- **World (Module 6)** - scripted + **organic** mining (beacon → survey → Robo-Miner builds → trucks auto-route
  once the mine exists), tube networks, an Earthworker that lays a tube at **mark 1**, a meteor at mark 8, and
  in-game messages.
- **Mission lifecycle (Module 7)** - save-state, typed save/load and the lifecycle callbacks.
- **BaseBuilder (Module 8)** - `createBase` from a declarative layout.

The two check phases:

- **Self-test (tick 3)** - calls every facade function and logs `[PASS]` / `[FAIL]` lines and a
  `SELF-TEST: N PASSED, M FAILED` summary. Orders are checked as *accepted* here, because the engine applies
  them later.
- **Outcome checks (tick 2500)** - confirms the orders actually *happened*: the scout arrived, the ConVec's
  Agridome finished, the Earthworker's tube exists, the EMP capture took the Lynx, the trucks delivered ore, the
  Robo-Miner built its mine, the red scout patrolled, and a time trigger fired once. Logs
  `OUTCOME CHECKS: N PASSED, M FAILED`.

Unlike `samples/Layer1` (which vendors its own frozen headers), this sample `#include`s the **live** library
from `../../TitanAPI/include`, so it always builds against the current `op2::` API.

## Build

**32-bit / MSVC / C++23** (Outpost 2 is a 32-bit process):

```
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
cmake --build build --config Release
```

→ `build/Release/cTitanSmokeTest.dll`. Drop it into your OPU `maps\` folder; it appears as a Colony game
("TitanAPI Smoke Test") and logs to `<OP2>\OPU\logs\cTitanSmokeTest.log`. A healthy build logs
`SELF-TEST: 197 PASSED, 0 FAILED` at tick 3 and, if you let the game run to tick 2500 (mark 25),
`OUTCOME CHECKS: 14 PASSED, 0 FAILED`. Both results also show in the in-game Communications log.

To refresh this sample after the test mission changes, copy `TitanAPI/src/mission.cpp` here and re-apply the
three differences listed at the top of `mission.cpp` (header comment, `LevelDesc`, log-name comment).
