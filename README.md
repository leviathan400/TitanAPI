# TitanAPI

**A modern C++23 library for Outpost 2 missions.**

TitanAPI lets you write Outpost 2 missions and mods in clean, modern C++ — building straight on the
reverse-engineered game engine, with no dependency on the legacy SDK stack.

It's a clean-room project that aims to **complete what TethysAPI set out to do**: a tidy, correct, complete
modern C++ library for Outpost 2. It is a **separate, independent project** from TethysAPI (which is
Arklon's) — named as a deliberate sibling: **Tethys and Titan are both moons of Saturn**. (`Tethys` is the
Outpost 2 engine's internal name, which Arklon's library takes; `Titan` is its companion.)

> **Status (June 2026, v0.5.43): Layer 1 complete; Layer 2 Modules 1–8 all complete — verified in-game.** The
> facade reached **100% functional parity with OP2Lua** (the mature reference scripting API) and adds AI scripting
> (ScGroups, UnitBlocks, Pinwheel waves), the full mission lifecycle (typed save/load + callbacks), and a
> declarative **BaseBuilder** — validated every run by an in-game self-test of **155 checks (0 failures)**. A
> sample mission drives a two-player colony — orders, building, a full mining operation, a tube network, triggers,
> disasters, AI combat groups, save state, and a working victory condition — through the facade. See
> [`ROADMAP.md`](ROADMAP.md), [`CHANGES.md`](CHANGES.md), and the parity map
> [`re-reference/op2lua-parity.md`](re-reference/op2lua-parity.md).

## Two layers

- **Layer 1 — `op2::abi`** *(done)*: the reverse-engineering substrate. It relocates fixed `Outpost2.exe`
  addresses and calls into them (free / `__thiscall` thunks, global reads, struct-field access). Its headers
  are **generated from extracted *facts* about the game binary** (`re-reference/`), not hand-copied from any
  library. Proven end-to-end in-game by the `samples/Layer1` mission.
- **Layer 2 — the facade** *(Modules 1–8 complete)*: value-handle `Unit` / `Player` / `Game` with
  `std::expected`-based orders. The full unit order set (move/attack/build/mine/dock/produce/doze/salvage/
  removeWall/…) dispatches through one validated command-packet path — designing out the crash/no-op bug class
  that has bitten TethysAPI's order API (raw `Cmd*` thunks; fixed per-function on its `fix-unit-commands` branch)
  — and `Unit` reads live state (type/owner/location/cargo/health) from the
  engine. Also: C++23 **ranges enumeration** of units, an **event/trigger** system with a name→callback registry,
  **victory/defeat** conditions, **GameMap** tile/terrain queries, disasters, **AI ScGroups** (FightGroup/
  MiningGroup/BuildingGroup, UnitBlocks, Pinwheel waves), the **mission lifecycle** (typed save/load + the
  OnSaveGame/OnChat/OnEndMission/… callbacks), and a declarative **BaseBuilder**. Coverage is tracked against
  OP2Lua in [`re-reference/op2lua-parity.md`](re-reference/op2lua-parity.md). Design: [`design/FACADE-DESIGN.md`](design/FACADE-DESIGN.md).

## Repository layout

```
op2titanapi/
├─ README.md / ROADMAP.md / CHANGES.md   this overview, the plan, and the changelog
├─ TitanAPI/        THE library + main dev folder: op2::abi headers (Layer 1, include/), the generators
│                   (tools/gen-*.ps1), the current test mission (src/), and where the Layer 2 facade grows
├─ samples/
│  ├─ Layer1/       the Layer-1-only test mission, frozen as the first sample (vendors its own headers)
│  ├─ ColonyDemo/   full Layer-2 demo (orders, mining, EMP-capture, enumeration); #includes TitanAPI/include
│  ├─ ColdFront/    a showcase mission — a TitanAPI port of OP2Lua's "Cold Front" (BaseBuilder, scheduler,
│                   AI mining/patrol/production, the lava eruption, starship victory)
│  └─ Nostalgia2P/  a 2-player multiplayer (Last One Standing) sample — symmetric bases via createBase offset
├─ re-reference/    the reverse-engineering FACTS (addresses, struct sizes/fields, enums, command packets)
│                   + the extractors that produce them (RE-REFERENCE.md, command-packets.md, ABI-MECHANISM.md)
├─ design/          FACADE-DESIGN.md — the Layer 2 design
├─ docs/            ABI-MECHANISM.md (how op2::abi reads the game) + the ecosystem review report
└─ CommandPacket/   the order/command system + multiplayer wire-protocol reference
```

## Build & run a mission

`TitanAPI/` and `samples/Layer1/` are standalone CMake projects. **32-bit / MSVC / C++23** (MSVC
`/std:c++23preview`), static CRT — Outpost 2 is a 32-bit process. From a folder with a `CMakeLists.txt`:

```
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
cmake --build build --config Release
```
→ a `c<Name>.dll` you drop into your OPU `maps\` folder; it shows up as a Colony game. See that folder's
`README.md` and [`docs/ABI-MECHANISM.md`](docs/ABI-MECHANISM.md). For how TitanAPI relates to TethysAPI and when
to use which, see [`docs/TitanAPI-vs-TethysAPI.md`](docs/TitanAPI-vs-TethysAPI.md).

## Provenance & credits

The addresses, struct layouts, and enum values TitanAPI is generated from are **facts about `Outpost2.exe`**
(a fixed 1997 binary) — the product of the Outpost Universe community's reverse-engineering going back to
OP2Internal / HFL (2008+). **TethysAPI** (Arklon / Outpost Universe, BSD-3) is the most complete consolidated
source of those facts; TitanAPI reads them out into its own generated form and does not copy TethysAPI's
code or expression. Thanks to Arklon and the OPU community for the RE that makes any of this possible.

Namespace: `op2` (Layer 2) / `op2::abi` (Layer 1).
