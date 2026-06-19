# Design: a modern C++23 Outpost 2 SDK - Layer 2 (the facade)

*Design notes for the TitanAPI facade (Layer 2).*

This describes the library we own: a clean, C++23, `std::expected`-based mission/mod API that sits on
top of the reverse-engineering substrate (Layer 1). It deliberately re-uses no third-party SDK *code or
expression* - only the **facts** (addresses, sizes, layouts) from the community's engine reverse-engineering.
It is the "Layer 2" that completes the mission/AI surface, with the historical order-API bug class designed out.

> Library: **TitanAPI**; namespace `op2` (facade) + `op2::abi` (Layer 1) + `op2::detail` (internal).

---

## 1. Goals & non-goals

**Goals**
- Correct by construction: the DoMove/DoDock/DoMiningRoute crash/no-op class is *impossible* to express.
- `std::expected` everywhere an operation can fail - no `void` orders that silently corrupt state.
- Multiplayer-deterministic by default: one command path, synced RNG only.
- Ergonomic & modern: ranges enumerators, value-type handles, `constexpr`, `std::span`, deducing `this`.
- One C++ core that OP2Lua (sol2), OP2DotNet (C++/CLI), and a future Python (CLIF) can all bind.
- Layer 1 is generated data we control (from the extracted RE facts), not a vendored third-party tree.

**Non-goals**
- Re-deriving the RE (we have it). 
- Wrapping the *entire* engine on day one - start with the mission/AI surface, grow outward.
- Source/binary compatibility with any prior SDK (clean break; we may offer a thin shim later).

---

## 2. Layering

```
        Outpost2.exe (32-bit, fixed addresses)         ← the binary
  ────────────────────────────────────────────────────
  Layer 1  op2::abi    RE substrate (FACTS, generated)
           • addresses.hpp   (constexpr addrs  ← addresses.csv)
           • raw/*.hpp       (POD struct mirrors, sizes static_assert'd)
           • command.hpp     (command-packet PODs)
           • enums.hpp       (engine enum values)
           • memory.hpp      (reloc, typed fn/global access, __thiscall thunks)
  ────────────────────────────────────────────────────
  Layer 2  op2          THE FACADE (what we author)
           • core: error/Result, Location, enums(typed), handle base
           • Game / Unit / Player / GameMap / Trigger / groups
           • enumerators (ranges views)
           • mission entry contract
  ────────────────────────────────────────────────────
  consumers   OP2Lua (sol2) · OP2DotNet (C++/CLI) · PyOP2-like (CLIF) · native authors
```

Rule of thumb: **`op2::abi` is mechanical and unsafe** (raw thunks/pointers, mirrors the binary).
**`op2` is safe, validated, and ergonomic.** The facade never lets an `abi` raw pointer or raw `Cmd*`
thunk escape to the user.

---

## 3. Folder / module layout

Header-only for the binding/thunk parts (the inline `__thiscall` thunks must instantiate at each call
site; this is the pragmatic choice for 32-bit MSVC, matching how every OP2 C++ lib works). The public
API is structured so it *could* be exposed as C++23 named modules later (one module per public header);
that's a post-1.0 nicety, not a v1 requirement.

```
TitanAPI/
├─ CMakeLists.txt                  # x86 / MSVC; C++23; produces the mission/mod DLL(s)
├─ include/op2/
│  ├─ op2.hpp                      # umbrella: #include everything public
│  ├─ core/
│  │  ├─ error.hpp                 # OrderStatus, OrderError, Result<T>, err:: constants
│  │  ├─ result.hpp               # Result helpers (ignore(), try-macros, logging glue)
│  │  ├─ location.hpp             # Location, MapRect, Waypoint, PackedLocation (constexpr)
│  │  ├─ enums.hpp                # TYPED facade enums (MapId, Direction, MineKind, …)
│  │  └─ handle.hpp               # Handle<Tag> base: id semantics, null, equality
│  ├─ game.hpp                    # Game: create*, disasters, messages, RNG, time, cheats
│  ├─ unit.hpp                    # Unit handle + orders (Result-returning)
│  ├─ player.hpp                  # Player handle: economy, tech, alliances, unit views
│  ├─ map.hpp                     # GameMap: tiles, terrain, lava, light
│  ├─ trigger.hpp                 # Trigger + Create*Trigger + typed callbacks
│  ├─ groups.hpp                  # ScGroup / Fight / Mining / Building
│  ├─ enumerators.hpp             # ranges views: player.units(), unitsInRange(), …
│  └─ mission.hpp                 # OP2_MISSION(...) export macro + lifecycle callbacks
├─ include/op2/abi/               # Layer 1 - RE substrate (namespace op2::abi)
│  ├─ memory.hpp                  # reloc(), fn<>(), global<>(), callMember<>() (__thiscall)
│  ├─ addresses.hpp               # GENERATED from addresses.csv
│  ├─ command.hpp                 # CommandPacket union + per-command PODs (from extracted RE data)
│  ├─ enums.hpp                   # raw engine enum values (CommandType, ActionType, MapId…)
│  └─ raw/
│     ├─ map_object.hpp           # MapObjectRaw (+ Vehicle/Building/… ) field mirrors
│     ├─ player_impl.hpp          # PlayerImplRaw
│     ├─ game_impl.hpp            # GameImplRaw, singleton accessor
│     └─ …                        # one per RE struct we need
├─ tools/
│  ├─ gen-addresses.ps1           # addresses.csv  →  abi/addresses.hpp
│  └─ gen-enums.ps1               # enums.csv      →  abi/enums.hpp
├─ tests/
│  ├─ static/abi_layout_test.cpp  # static_assert sizeof/offsetof vs extracted facts (ABI guard)
│  └─ ingame/                     # mission DLLs that exercise each order live (see §10)
└─ docs/FACADE-DESIGN.md          # this doc
```

**Single source of truth:** `abi/addresses.hpp` and `abi/enums.hpp` are *generated* from the extracted RE
data by `tools/gen-*.ps1`. Update the source facts → re-run the extractor → re-run the generators → the
binding constants update. No hand-edited address tables.

---

## 4. Core value types (brief)

`Location`, `MapRect`, `Waypoint` are `constexpr` POD-ish value types. They own the coordinate
translation that bit everyone (OP2's hidden 32-tile horizontal pad), so authors "type what they see."

```cpp
// core/location.hpp
namespace op2 {
struct Location {
  int x = 0, y = 0;                                   // author-facing tile coords
  constexpr Location() = default;
  constexpr Location(int x_, int y_) : x{x_}, y{y_} {}

  [[nodiscard]] constexpr bool   onMap()   const noexcept;          // bounds check
  [[nodiscard]] abi::Waypoint    asWaypoint() const noexcept;       // → engine packed form (+pad)
  [[nodiscard]] constexpr Location operator+(Location o) const noexcept { return {x+o.x, y+o.y}; }
  friend constexpr bool operator==(Location, Location) = default;
};
}
```

Facade enums (`core/enums.hpp`) are **typed wrappers** over `abi` raw enum values, so the public API
never exposes raw ints: `enum class MapId : int { CommandCenter = abi::map::CommandCenter, … };`
(generated alongside `abi/enums.hpp`).

---

## 5. Error model & `std::expected` orders  ← centerpiece

### 5.1 Why this cures the bug class

Every bug we found was an order that returned `void` and either crashed (`DoMove`/`DoDock` via a raw
`Cmd*` thunk) or silently did nothing (`DoMiningRoute` malformed packet). The root causes:
1. orders bypassed the command-packet path, and
2. there was no return channel to signal failure.

The facade fixes both *structurally*:
- **One safe path.** Orders only ever build a validated `abi::CommandPacket` and dispatch it through a
  single `detail::issue()` → `ProcessCommandPacket`. Raw `Cmd*` thunks are not reachable from `op2`.
- **One result type.** Orders return `Result<void>` (`= std::expected<void, OrderError>`). Preconditions
  are checked *before* dispatch, returning a typed error instead of corrupting state.

### 5.2 The types

```cpp
// core/error.hpp
namespace op2 {

enum class OrderStatus : std::uint8_t {
  Ok = 0,
  NullHandle,        // handle was default/null
  DeadUnit,          // unit/target no longer live
  NotOwned,          // acting player doesn't own the unit (where required)
  WrongUnitType,     // e.g. miningRoute() on a non-truck
  InvalidTarget,     // target missing/incompatible
  InvalidLocation,   // off-map / impassable where the order requires passable
  Unsupported,       // order invalid for this unit in its current state
  EngineRejected,    // creation/allocation returned a failure sentinel
};

struct OrderError {
  OrderStatus      status = OrderStatus::Ok;
  std::string_view what   = {};   // static, cheap; for logs/bindings
};

template <class T = void>
using Result = std::expected<T, OrderError>;          // <expected>

// pre-built constants (no allocation)
namespace err {
inline constexpr OrderError NullHandle     {OrderStatus::NullHandle,     "null handle"};
inline constexpr OrderError DeadUnit       {OrderStatus::DeadUnit,       "unit not live"};
inline constexpr OrderError NotOwned       {OrderStatus::NotOwned,       "unit not owned by actor"};
inline constexpr OrderError WrongUnitType  {OrderStatus::WrongUnitType,  "wrong unit type for order"};
inline constexpr OrderError InvalidTarget  {OrderStatus::InvalidTarget,  "invalid target"};
inline constexpr OrderError InvalidLocation{OrderStatus::InvalidLocation,"location off-map/impassable"};
inline constexpr OrderError EngineRejected {OrderStatus::EngineRejected, "engine rejected request"};
}
}
```

`Result<void>` is tiny (a bool-ish discriminant + a 2-word `OrderError`). No heap, no exceptions -
which matters because **OP2 is compiled exception-disabled**; the facade must never throw across the
DLL boundary. Error-as-value is the only safe contract.

### 5.3 The single dispatch path

```cpp
// detail (internal)
namespace op2::detail {
inline Result<void> issue(int actingPlayer, const abi::CommandPacket& pkt) {
  // THE one safe path. Routes through the owning player's command processor, exactly like every
  // working order in the engine - deterministic / lockstep-safe. Never a raw Cmd* thunk.
  abi::ProcessCommandPacket(actingPlayer, pkt);   // Layer-1 thunk
  return {};                                       // engine call is void: success == "accepted & queued"
}
}
```

**Honesty about what `Result` can and can't see (important design note).** Because OP2 is deterministic
lockstep, `ProcessCommandPacket` queues the order and returns `void`; deep, *asynchronous* engine
outcomes ("the pathfinder later gave up") are not observable at call time. So `Result` captures the
failure modes we can detect **before dispatch** - which is *exactly the class that caused every bug we
hit* (dead handle, wrong type, malformed inputs, off-map). Asynchronous outcomes are observed by polling
state next tick (and by the in-game test harness, §10). This is a deliberate, documented boundary, not a
gap - and it's strictly more than the old `void`-and-pray API offered.

### 5.4 Order example (the fixed `move` / `dock` / `miningRoute`)

```cpp
// unit.hpp
inline Result<void> Unit::move(Location to) {
  if (id_ < 0)        return std::unexpected(err::NullHandle);
  if (!live())        return std::unexpected(err::DeadUnit);
  if (!to.onMap())    return std::unexpected(err::InvalidLocation);

  abi::CommandPacket p{};
  p.type              = abi::CommandType::Move;
  p.size              = sizeof(abi::MoveCommand);
  p.move.numUnits     = 1;
  p.move.unitId[0]    = id_;
  p.move.numWaypoints = 1;
  p.move.waypoint[0]  = to.asWaypoint();
  return detail::issue(ownerIndex(), p);
}

inline Result<void> Unit::dock(Unit structure) {
  if (id_ < 0 || !live())                return std::unexpected(err::DeadUnit);
  if (!structure.live())                 return std::unexpected(err::InvalidTarget);
  const auto dockTile = structure.dockLocation();
  if (!dockTile)                         return std::unexpected(err::InvalidTarget);

  abi::CommandPacket p{};
  p.type              = abi::CommandType::Dock;     // uses MoveCommand payload
  p.size              = sizeof(abi::MoveCommand);
  p.move.numUnits     = 1;
  p.move.unitId[0]    = id_;
  p.move.numWaypoints = 1;
  p.move.waypoint[0]  = dockTile->asWaypoint();
  return detail::issue(ownerIndex(), p);
}

inline Result<void> Unit::miningRoute(Unit mine, Unit smelter) {
  if (!live())                                   return std::unexpected(err::DeadUnit);
  if (!isType(MapId::CargoTruck))                return std::unexpected(err::WrongUnitType);
  if (!mine.live() || !smelter.live())           return std::unexpected(err::InvalidTarget);

  abi::CommandPacket p{};
  p.type                       = abi::CommandType::CargoRoute;
  p.size                       = sizeof(abi::CargoRouteCommand);
  p.cargoRoute.numUnits        = 1;                 // <- the historical bug set this to id_
  p.cargoRoute.unitId[0]       = id_;               // <- the historical bug wrote unitId[1]
  p.cargoRoute.numWaypoints    = 3;
  p.cargoRoute.waypoint[0]     = mine.dockLocation()->asWaypoint();
  p.cargoRoute.waypoint[1]     = smelter.dockLocation()->asWaypoint();
  p.cargoRoute.waypoint[2]     = mine.dockLocation()->asWaypoint();
  p.cargoRoute.mineWaypointIdx = 0;
  p.cargoRoute.smelterWaypointIdx = 1;
  p.cargoRoute.mineUnitId      = mine.id();
  p.cargoRoute.smelterUnitId   = smelter.id();
  return detail::issue(ownerIndex(), p);
}
```

The buggy packet-header pattern is impossible here because **one** helper fills the unit-list header and
every order uses it:

```cpp
// fill the SimpleCommand header correctly, once.
template <class Cmd>
constexpr void detail::oneUnit(Cmd& c, int unitId) { c.numUnits = 1; c.unitId[0] = unitId; }
```

### 5.5 Batch orders (C++23 `std::span`)

The engine's command packets are natively multi-unit. Expose it:

```cpp
Result<void> op2::move(std::span<const Unit> units, Location to);   // packs N unitIds into one packet
```

### 5.6 Ergonomics

Orders are `[[nodiscard]]`. Idioms:

```cpp
if (auto r = truck.miningRoute(mine, smelter); !r) log::warn(r.error().what);   // check
(void) lynx.move(rally);                                                         // deliberate ignore
convec.dock(factory).and_then([&]{ return convec.build(MapId::Agridome, here); }); // chain (C++23)
```

A `core/result.hpp` provides `op2::ignore(Result&&)`, an `OP2_TRY(expr)` macro (early-return on error,
for sequences inside `Result`-returning functions), and a `to_lua_error` / `to_hresult` adapter for the
binding layers.

---

## 6. Handles: how `Unit` / `Player` / `Game` wrap Layer 1  ← centerpiece

The facade types are **value-semantic handles** - small, copyable, never owning. They reference engine
objects by the engine's own identity (a `Unit` is an `int` "stub index"; a `Player` is an `int` 0-6),
*not* by cached raw pointers (units die and slots get reused, so a cached pointer is a dangling-pointer
bug waiting to happen). Raw pointers are resolved on demand, used immediately, never stored.

### 6.1 Layer 1 access primitives (`op2::abi`)

```cpp
// abi/memory.hpp  (our own clean implementation; the reloc technique is standard, not anyone's IP)
namespace op2::abi {
inline constexpr std::uintptr_t kImageBase = 0x00400000;            // from constants.csv
HMODULE module();                                                   // GetModuleHandle(nullptr)
inline std::uintptr_t reloc(std::uintptr_t a)                      // ASLR-safe
  { return reinterpret_cast<std::uintptr_t>(module()) + (a - kImageBase); }

template <class T> T& global(std::uintptr_t a) { return *reinterpret_cast<T*>(reloc(a)); }

// call an internal __thiscall member function at a fixed address
template <std::uintptr_t Addr, class R, class This, class... A>
R member(This* self, A... args) {
  using Pfn = R(__thiscall*)(This*, A...);
  return reinterpret_cast<Pfn>(reloc(Addr))(self, args...);
}
// call an internal __cdecl/free function
template <std::uintptr_t Addr, class R, class... A>
R call(A... args) { return reinterpret_cast<R(__cdecl*)(A...)>(reloc(Addr))(args...); }
}
```

```cpp
// abi/addresses.hpp  (GENERATED from addresses.csv - illustrative subset)
namespace op2::abi::addr {
inline constexpr std::uintptr_t Game_CreateUnit          = 0x478780; // global Game::CreateUnit
inline constexpr std::uintptr_t Game_ProcessCommand      = /*from csv: symbol ProcessCommandPacket*/;
inline constexpr std::uintptr_t MapObject_GetType        = /*from csv*/;
inline constexpr std::uintptr_t PlayerArray_ptr          = 0x4890C3; // global: ptr to player array
inline constexpr std::uintptr_t GameInstance_ptr         = /*from csv: GameImpl::GetInstance*/;
// … ~2,500 more, generated …
}
```

```cpp
// abi/raw/map_object.hpp  (field mirror - layout & size come from the extracted RE data; static_assert'd)
namespace op2::abi {
#pragma pack(push, 1)
struct MapObjectRaw {
  // fields at known offsets (filled from the field-layout extraction)
  std::uint16_t typeId;          // …
  std::uint8_t  ownerIndex;      // …
  /* … */
};
#pragma pack(pop)
static_assert(sizeof(MapObjectRaw) == /*size from struct-sizes.csv*/);

MapObjectRaw* unitFromId(int id);   // engine resolver (thunk) - id → live object or nullptr
}
```

### 6.2 `Unit`

```cpp
// unit.hpp
namespace op2 {
class Unit {
public:
  constexpr Unit() = default;
  constexpr explicit Unit(int id) : id_{id} {}

  // identity / queries  (read raw fields or call thunks; resolve pointer on demand)
  [[nodiscard]] constexpr int id() const noexcept { return id_; }
  [[nodiscard]] bool          live()  const { return abi::unitFromId(id_) != nullptr; }
  [[nodiscard]] MapId         type()  const {
    auto* r = abi::unitFromId(id_);
    return r ? static_cast<MapId>(r->typeId) : MapId::None;   // direct field read (hot path)
  }
  [[nodiscard]] bool          isType(MapId t) const { return type() == t; }
  [[nodiscard]] Player        owner() const;                  // wraps ownerIndex
  [[nodiscard]] Location      location() const;               // thunk or field read
  [[nodiscard]] std::optional<Location> dockLocation() const; // thunk

  // orders - all Result<void>, all via the packet path (see §5)
  [[nodiscard]] Result<void> move(Location to);
  [[nodiscard]] Result<void> attack(Unit target);
  [[nodiscard]] Result<void> dock(Unit structure);
  [[nodiscard]] Result<void> miningRoute(Unit mine, Unit smelter);
  [[nodiscard]] Result<void> build(MapId what, Location at);
  // …

  friend constexpr bool operator==(Unit, Unit) = default;
private:
  [[nodiscard]] int ownerIndex() const;
  int id_ = -1;                                               // -1 == null handle
};
}
```

Two access styles, used deliberately:
- **Queries → direct field reads** on `abi::unitFromId(id_)` (fast, no thunk) for hot, side-effect-free
  data (type, damage, cargo). Offsets come from the field-layout extraction.
- **Orders & anything with engine side effects → command packets / thunks** (the safe path), never field pokes.

### 6.3 `Player`

```cpp
// player.hpp
namespace op2 {
class Player {
public:
  constexpr explicit Player(int index) : idx_{index} {}
  [[nodiscard]] constexpr bool valid() const noexcept { return idx_ >= 0 && idx_ < 7; }
  [[nodiscard]] constexpr int  index() const noexcept { return idx_; }

  // economy / state - field reads on PlayerImplRaw, writes via thunks (engine keeps invariants)
  [[nodiscard]] int  ore() const;
  Result<void>       setOre(int amount);
  [[nodiscard]] int  kids() const;            // (HFL-style accessors, but first-class here)
  [[nodiscard]] bool isHuman() const;
  [[nodiscard]] bool isEden()  const;

  Result<void> ally(Player other);
  Result<void> markResearchComplete(int techId);

  // ranges view of this player's live units (see §7)
  [[nodiscard]] auto units() const;
private:
  abi::PlayerImplRaw* raw() const;            // resolve from GameInstance()->players[idx_]
  int idx_ = -1;
};

inline Player player(int i) { return Player{i}; }   // op2::player(0)
}
```

`Player::raw()` resolves the per-player struct from the game singleton's player array
(`abi::global<...>(addr::PlayerArray_ptr)` or `GameInstance()->players[idx_]`), again on demand.

### 6.4 `Game`

`Game` is a stateless static facade over the engine singleton (`abi::GameInstance()`), grouping the
"god" operations. Creation returns `Result<Unit>`; disasters/effects return `Result<void>`; RNG/time
are plain returns.

```cpp
// game.hpp
namespace op2 {
struct Game {
  static Result<Unit> createUnit(MapId type, Location at, Player owner,
                                 MapId weapon = MapId::None, Direction dir = Direction::North) {
    if (!owner.valid()) return std::unexpected(err::NotOwned);
    if (!at.onMap())    return std::unexpected(err::InvalidLocation);
    const int id = abi::call<abi::addr::Game_CreateUnit, int>(
                     int(type), at.x, at.y, owner.index(), int(weapon), int(dir));
    if (id < 0)         return std::unexpected(err::EngineRejected);
    return Unit{id};
  }
  static Result<Unit> createMine(Location at, MineKind kind);
  static Result<void> meteor(Location at, MeteorSize size);
  static Result<void> message(std::string_view text, Player to = {});   // std::string_view (C++23)

  static std::uint32_t rand();          // synced RNG ONLY (MP-safe) - wraps engine RNG
  static void          setSeed(std::uint32_t s);
  static std::uint32_t tick();          // game time
};
}
```

### 6.5 The wrapping pattern, summarized

| Facade thing | Holds | Resolves Layer 1 via | Reads | Writes / acts |
|---|---|---|---|---|
| `Unit` | `int id_` (stub index) | `abi::unitFromId(id_)` (on demand) | raw field reads (hot queries) | command packets only (§5) |
| `Player` | `int idx_` (0-6) | `abi::PlayerArray[idx_]` | raw field reads | thunks (engine keeps invariants) |
| `Game` | nothing (static) | `abi::GameInstance()` | thunks/fields | thunks; creation → `Result<Unit>` |

Invariant: **handles store identity, never pointers; pointers are resolved-used-discarded within a single
call.** This makes dangling impossible and keeps handles trivially copyable/`constexpr`.

---

## 7. Enumerators as ranges

The engine's `PlayerUnitIterator` / `InRange` / `InRect` / `Closest` enumerators become C++20/23 input-range views.
Default to a hand-written zero-alloc input iterator wrapping the engine's `GetNext` enumerator (hot game
loop ⇒ avoid allocation); `std::generator` is an option for convenience-only enumerators.

```cpp
for (Unit u : player.units())                                    { /* … */ }
for (Unit u : player.units() | std::views::filter(&Unit::isCombat)) { /* … */ }
for (Unit u : Game::unitsInRange(center, 6))                     { /* … */ }
for (Unit u : Game::unitsInRect(rect))                          { /* … */ }
auto nearest = Game::closestUnit(center, pred);                  // std::optional<Unit>
```

```cpp
// enumerators.hpp (sketch)
class UnitView : public std::ranges::view_interface<UnitView> {
public:
  struct iterator {                       // std::input_iterator
    using value_type = Unit; using difference_type = std::ptrdiff_t;
    Unit operator*() const { return cur_; }
    iterator& operator++() { advance(); return *this; }
    bool operator==(std::default_sentinel_t) const { return done_; }
    /* … wraps engine enumerator state + GetNext … */
  };
  iterator begin() const;  std::default_sentinel_t end() const { return {}; }
};
```

---

## 8. Mission entry contract

`mission.hpp` provides the DLL export contract (a modern analogue of the classic `EXPORT_OP2_*` macros):

```cpp
OP2_MISSION({
  .name        = "Hold The Line",
  .map         = "ng4.map",
  .techtree    = "MULTITEK.TXT",
  .type        = MissionType::Colony,
  .humanPlayers = 1,
});

// strongly-typed lifecycle callbacks (registered, not magic-named):
op2::onInit([] -> Result<void> { /* set up bases/triggers */ return {}; });
op2::onTick([] { /* per AIProc */ });
op2::onCreateUnit([](Unit u, Player p) { /* OPU 1.4.x extended cb */ });
op2::onChat([](Player from, std::string_view msg) { /* … */ });
op2::saveState(myState);          // typed save/load (replaces GetSaveRegions hand-rolling)
```

The macro emits the required `LevelDesc`/`MapName`/`TechtreeName`/`DescBlock(Ex)` exports; the callback
registrars wire `InitProc`/`AIProc`/`On*` so the author never hand-writes the C ABI. `saveState` declares
the save region(s) in a typed way (a real gap in earlier mission toolkits, which had *no* save/load).

---

## 9. Determinism & safety invariants (enforced/encouraged)

1. **One command path.** Orders only go through `detail::issue → ProcessCommandPacket`. No raw `Cmd*` in
   `op2`. (Raw thunks live in `op2::abi`, clearly unsafe, for power users.)
2. **Synced RNG only.** `Game::rand()` wraps the engine RNG; the facade exposes no other randomness. (AI
   code that wants variety uses `Game::rand()`, never `std::rand`/`<random>` with a wall-clock seed.)
3. **No exceptions across the boundary.** Error-as-value (`Result`) throughout; the mission entry shims
   wrap user callbacks in a catch-all that converts escapes to logged errors (never propagates into OP2).
4. **Handles are values; pointers are ephemeral.** Never cache `abi` pointers across ticks.
5. **Main-thread affinity.** Engine calls happen on the game thread; provide `OP2_ASSERT_MAIN_THREAD()`
   (debug) like OP2Lua/OP2DotNet do. Off-thread planning must use a snapshot + scheduled apply.

---

## 10. Testing - the thing no prior mission SDK had

This is as important as the API. Two layers:

- **Compile-time ABI guard** (`tests/static/`): for every `abi::raw` struct, `static_assert` its
  `sizeof` and key `offsetof` against the extracted (generated) values. If a future RE
  re-extraction changes a size/offset, the build breaks loudly instead of corrupting memory at runtime.
- **In-game regression harness** (`tests/ingame/`): mission DLLs that issue each order and assert the
  in-game outcome - the MoveTest / feature-test pattern OP2Lua already uses, generalized. Each order
  (move/dock/miningRoute/build/attack/…) gets a scenario that fails before the fix and passes after, run
  on **both 1.3.6 and 1.4.1**. Wire it to the `run-and-watch.ps1`-style automation.

Result types make these assertions clean: a test can assert `r.error().status == OrderStatus::WrongUnitType`.

---

## 11. C++23 features, and where each earns its keep

| Feature | Used for |
|---|---|
| `std::expected` | order/creation results - the core of the design (§5) |
| ranges + `views` | enumerators (`player.units() \| views::filter(...)`) (§7) |
| deducing `this` | one definition for const/non-const handle accessors; trim CRTP boilerplate |
| `std::span` | batch orders, waypoint/unit-list params (§5.5) |
| `constexpr` (extended) | `Location`/`MapRect`/enum math fully compile-time |
| `std::print` / `std::format` | logging/diagnostics (the 3-log standard you already use) |
| `if consteval`, `[[assume]]` | small perf/clarity wins in hot inline thunks |
| `std::generator` *(opt)* | convenience enumerators where allocation is acceptable |
| named modules *(post-1.0)* | optional packaging; header-only stays the default for thunks |

Layer 1 (`op2::abi`) can stay at C++17/20 - it's mechanical mirror code, not worth churning; mix freely.

---

## 12. Build & packaging

- **MSVC, x86 (Win32) only** - mandatory (binds to a 32-bit binary, `__thiscall`/`__fastcall` ABI).
- **C++23 (genuine), no C++26.** Build with MSVC `/std:c++23preview` (its real C++23 mode). Gotcha: CMake `cxx_std_23` / `CMAKE_CXX_STANDARD 23` map to `/std:c++latest` on MSVC (the C++26 track), so set it directly - `target_compile_options(<tgt> PRIVATE /std:c++23preview)` (CMake records `<LanguageStandard>stdcpp23</LanguageStandard>`). **Decision (Jun 2026):** target C++23; the only C++26 feature that would help us is reflection (P2996), which MSVC hasn't shipped - revisit it for the binding/serialization layer when it lands. Verified: `std::expected`, `std::print`, `std::ranges`, and deducing-`this` all compile under `/std:c++23preview`.
- CMake produces the mission/mod DLL(s); the facade is header-only, so consumers just add `include/`.
- Loaded via the OPU `op2ext` loader (target OPU 1.4.x), same as the other modern SDKs.

---

## 13. Consumers - the payoff (one core under everything)

The facade is shaped to be the single C++ core under all front-ends:

- **OP2Lua** - sol2 binds the facade instead of a raw SDK directly. `Result` maps cleanly to a Lua error
  (or a `(ok, err)` pair); validation already lives in the facade, so the Lua layer shrinks. You stop
  carrying a separately-patched SDK fork.
- **OP2DotNet** - the C++/CLI bridge wraps the facade; `Result` → typed C# exception or `bool TryX`.
  Re-basing the .NET bridge here is what finally kills the classic-vs-modern foundation split.
- **Python (CLIF/pybind)** - same surface; `Result` → Python exception. (The PyOP2 approach, on a core
  you control and that's actually complete.)

`std::expected` is the lingua franca: each binding maps it to that language's idiom once, centrally.

---

## 14. Open decisions (for you)

1. **Name & namespace** - settled: **TitanAPI** (library) / `op2` (namespace).
2. **Handle null model** - `int id_ = -1` sentinel (shown) vs `std::optional`-wrapped handles. I lean
   sentinel (cheaper, matches engine).
3. **`[[nodiscard]]` strictness on orders** - warn-on-ignore (shown) vs an explicit `try_*` tier. I lean
   single `[[nodiscard]]` API + `(void)` / `op2::ignore()` for deliberate fire-and-forget.
4. **Enumerator engine** - hand-written input iterator (zero-alloc, shown) vs `std::generator` default.
   I lean hand-written for the hot loop.
5. **How much of `op2::abi` to generate now** - addresses + enums + the structs the mission/AI surface
   needs, vs the whole engine. I lean "mission/AI surface first, grow on demand."

---

## 15. Roadmap

1. **Step 2 (done):** RE reference finished - struct **field layouts**, **command-packet** structs, and
   **enums** are captured as data (`struct-fields.csv`, `enums.csv`), so `op2::abi` is generatable from the
   extracted facts alone.
2. Write `tools/gen-addresses.ps1` + `gen-enums.ps1`; emit `op2::abi` (addresses, enums, the core
   structs: `MapObjectRaw`, `PlayerImplRaw`, `GameImplRaw`, command packets) with `static_assert` guards.
3. Implement `op2` core: `error/Result`, `Location`, `Unit` (move/attack/dock/build/miningRoute),
   `Player`, `Game`. Stand up the in-game harness; prove the bug-class scenarios pass on 1.3.6 + 1.4.1.
4. Enumerators, triggers, groups, mission entry + typed save/load.
5. Point OP2Lua at it (sol2) as the first real consumer; iterate the surface from real use.
6. Later: OP2DotNet re-base; Python bindings; optional modules; broaden engine coverage.
```
