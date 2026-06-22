# TitanAPI Samples

Complete, in-game-verified missions you can read, build, and adapt. Each is self-contained: it `#include`s the
library from `../../TitanAPI/include`, so it always builds against the current `op2::` API. New to TitanAPI?
Start with [`../docs/GETTING-STARTED.md`](../docs/GETTING-STARTED.md).

| Sample | Builds | What it shows |
|---|---|---|
| [`Layer1/`](Layer1/) | `cLayer1.dll` | A minimal first mission - sets up a colony and places units directly through Layer 1 (`op2::abi`). The smallest thing that loads and runs. |
| [`ColonyDemo/`](ColonyDemo/) | `cColonyDemo.dll` | The everyday facade: unit orders, mining, and `std::ranges` enumeration of live units. |
| [`ColdFront/`](ColdFront/) | `cColdFrontTitan.dll` | A single-player mission with a scripted AI opponent, a small `AIProc` scheduler, an erupting volcano (lava disaster), and a starship-evacuation victory. Dogfoods most of the facade in one mission. |
| [`Nostalgia2P/`](Nostalgia2P/) | `cNostalgia2P.dll` | A 2-player multiplayer (Last One Standing) mission - the BaseBuilder + multiplayer showcase: one `BaseLayout` stamped at two corners via `createBase(player, layout, offset)`. |

## Building a sample

Outpost 2 is a 32-bit process, so samples build as 32-bit MSVC / C++23:

```
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
cmake --build build --config Release
```

Run it from each sample's folder. The output `c<Name>.dll` drops into your Outpost 2 install and appears in the
game's mission list. See each sample's own `README.md` for specifics.
