# SmokeTest - the TitanAPI in-game smoke test

The mission used to validate every build of the library: a full colony that exercises the Layer 2 facade
end to end and runs an **in-mission self-test** logging a `PASSED / FAILED` summary. A snapshot of the
development test mission, kept here so anyone can drop it in and confirm a build is healthy.

It demonstrates, on `cm01.map` with two players (human Plymouth + an AI Eden):

- **Player setup** - faction / human / AI / population / resources / tech, steady morale (`forceMoraleGood`).
- **Unit orders (Module 1)** - move, attack, build, mine, dock, produce, and an **EMP-capture** (EMP Lynx →
  `isEMPed()` → Spider reprograms the enemy Lynx → you take ownership).
- **Unit state (Module 2)** - `isLive` / `ownerId` / `type` / `location` / `cargo` / `isEMPed` reads.
- **Enumeration (Module 3)** - `Game::units()` / `unitsOf()` as C++ ranges (`| std::views::filter(...)`).
- **World** - scripted + **organic** mining (beacon → survey → Robo-Miner builds → trucks auto-route once
  the mine exists), tube networks, an Earthworker that builds a tube at **mark 1** (`Game::mark()`), and
  in-game messages (`Game::addMessage`).
- **In-mission self-test** - exercises every facade function and logs a `PASSED / FAILED` summary.

Unlike `samples/Layer1` (which vendors its own frozen headers), this sample `#include`s the **live** library
from `../../TitanAPI/include`, so it always builds against the current `op2::` API.

## Build

**32-bit / MSVC / C++23** (Outpost 2 is a 32-bit process):

```
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
cmake --build build --config Release
```

→ `build/Release/cTitanSmokeTest.dll`. Drop it into your OPU `maps\` folder; it appears as a Colony game
("TitanAPI Smoke Test") and logs to `<OP2>\OPU\logs\cTitanSmokeTest.log` - check the tail for the
`SELF-TEST: N PASSED, 0 FAILED` line.
