# ABI mechanism - how we read Outpost 2's memory (and call into it)

*General documentation. This is the **starting point**: how to turn the reverse-engineered
addresses of `Outpost2.exe` into callable functions and readable/writable data. Everything else - reading
game state, issuing orders, writing missions in our own SDK - is built on this.*

`Outpost2.exe` has no source and no API for most of what a mission needs. To do anything from a mod DLL you
reach **directly into the running game's memory**: call its functions at their known addresses, read and
write its globals and objects. This doc explains the one technique that makes that safe and portable -
the classic `OP2Mem`/`OP2Thunk` pattern - and how our own `op2::abi` re-implements it.

---

## 1. The one idea

`Outpost2.exe` is a fixed 1997 binary: every function and global lives at a known address (preferred load
base `0x00400000`). To use them from a DLL you (a) **relocate** the address to wherever Windows actually
mapped the exe, and (b) reinterpret it as a correctly-typed function pointer or data pointer.

- **`OP2Mem`** resolves *data* - returns a pointer or a reference into the exe's memory.
- **`OP2Thunk`** resolves a *function* pointer and calls it. It is built entirely on `OP2Mem`.

---

## 2. The relocation primitive

Everything reduces to one line (`Common/Memory.h:27`):

```cpp
actualAddress = moduleBase + (preferredAddress - OP2Base)      // OP2Base = 0x00400000
```

`preferredAddress` is the address as if the exe loaded at `0x400000` - which is exactly what the generated
`addresses.hpp` stores. `(preferredAddress - OP2Base)` is
the **RVA** (offset into the image); adding the real module base yields a valid pointer regardless of where
the image was placed (ASLR / rebasing). In the common case the exe *is* at `0x400000`, so this is the
identity - but the subtraction is what makes it robust when it isn't. `GetModuleHandleA(nullptr)` returns
the main exe's `HMODULE`, which on Windows **is** its load base.

---

## 3. `OP2Mem` - three overloads

`OP2Mem` has three overloads selected by template parameters.

**(a) By pointer, runtime address** (`Common/Memory.h:26`)
```cpp
template <typename T = void*, bool Global = false, typename R = SelectPtr<T>>
R OP2Mem(uintptr address) { return R((uint8*)(base) + (address - OP2Base)); }
```
Casts the relocated address to a pointer type and returns it. `OP2Mem<Foo*>(addr)` → a `Foo*` into the exe.

**(b) By reference, runtime address** (`Common/Memory.h:30`)
```cpp
template <typename T, bool Global = false, typename = enable_if<is_reference_v<T>>>
T OP2Mem(uintptr address) { return T(*((uint8*)(base) + (address - OP2Base))); }
```
Same math, but dereferences → a *reference* to the variable at that address. This is the runtime-`OP2Mem`
form: `OP2Mem<PlayerImpl*&>(0x4890C3)` → a reference to the player-array pointer **variable** in the exe
(read or write it). These are the rows tagged `kind=global` in `addresses.csv`.

**(c) By compile-time address, cached** (`Common/Memory.h:34`) - the common one
```cpp
template <uintptr Address, typename T = void*>
T OP2Mem() { static const T p = OP2Mem<T, true>(Address);  return p; }
```
`Address` is a template parameter; the resolved pointer is cached in a function-local `static`, so the
`GetModuleHandle` + arithmetic runs **once per instantiation**, then it's a single load. It passes
`Global = true` (see §4), which is why the comment says *"always safe to use for globals."*

The two runtime overloads are SFINAE-disjoint (reference `T` → (b); pointer-ish `T` → (a)) via three
helpers (`Common/Memory.h:18`): `PtrArg<T>` = "is pointer/array/function type", `FnToPfn<T>` = "function
type → function-pointer type", `SelectPtr<T>` = `enable_if(PtrArg<T>, FnToPfn<T>)`.

---

## 4. The module-handle safety story (the `Global` flag)

Two ways to get the base, trading speed for safety (`Common/Memory.h:11-13`):

```cpp
inline HMODULE g_hOP2 = GetOP2Handle();   // eager global - one cheap read, but...
inline HMODULE GetOP2Handle()             // ...lazy function-local static - always safe
  { static const auto h = []{ auto h = GetModuleHandleA(nullptr); return h ? h : HMODULE(OP2Base); }(); return h; }
```
- `g_hOP2` (used when `Global = false`) is a single global read - fast, but **UB if touched during another
  global's construction** (static-init-order fiasco): it might not be set yet.
- `GetOP2Handle()` (used when `Global = true`) defers to a function-local static, initialized thread-safely
  on first call - correct even from a global constructor.

The cached overload (3c) forces `Global = true`, so `OP2Mem<addr, T>()` is safe to initialize *your own*
globals with. The `h ? h : HMODULE(OP2Base)` fallback handles `GetModuleHandle` returning null (assume the
preferred base → no relocation).

---

## 5. `OP2Thunk` - call a function at an address

Two overloads, both one-liners over the cached `OP2Mem<Address,…>()` (`Common/Memory.h:38`):

```cpp
// (a) Fn is a function TYPE, e.g. void(int):
template <uintptr Address, typename Fn = void(), typename... Args>
auto OP2Thunk(Args&&... args) { return OP2Mem<Address, FnToPfn<Fn>>()(std::forward<Args>(args)...); }

// (b) Pfn is a VALUE, e.g. &OP2Alloc or a pointer-to-member-function:
template <uintptr Address, auto Pfn, typename... Args>
auto OP2Thunk(Args&&... args) { return OP2Mem<Address, decltype(Pfn)>()(std::forward<Args>(args)...); }
```
Resolve the address to a function pointer (typed from the explicit `Fn`, or inferred from the passed
function/PMF via `decltype`), then invoke. e.g. `OP2Thunk<0x4C21F0, &OP2Alloc>(size)`.

---

## 6. Member functions - the dominant `Thunk<0xADDR, &$::Method>` form

Calling an internal **member** function needs `this` and the right calling convention. That's
`OP2Class<Derived>::Thunk` (`Common/Memory.h:114`) + the `PmfTraitsImpl` trait (`Common/Memory.h:80`):

```cpp
// decompose a pointer-to-member-function type:
template <typename R, typename T, typename X, typename... A>
struct PmfTraitsImpl<R(T::*)(A...), X> { using This = T*;  using Pfn = R(THISCALL*)(This, A...); };
//                                  const → This = const T*

template <uintptr Address, auto Pmf, typename... Args>
auto Thunk(Args&&... args) const {
  return OP2Thunk<Address, PmfToPfnType<Pmf>>(PmfThisPtr<Pmf>(this), std::forward<Args>(args)...);
}
```

So `Thunk<0x478780, &$::CreateUnit>(args…)` (`$` = the class):
1. From the PMF `&$::CreateUnit`, derive `Pfn = R(__thiscall*)(This*, A...)` - a **free** function pointer
   whose **first explicit parameter is `this`**, marked `__thiscall`.
2. Resolve `0x478780` to that `Pfn` (via `OP2Thunk`/`OP2Mem`, cached).
3. Call it as `pfn(this, args…)`.

**Why this is the clean technique:** declaring a `__thiscall` pointer with `this` as the explicit first arg
lets MSVC place `this` in ECX per the ABI - calling the engine's member function directly. This is cleaner
than the older HFL approach, which cast to `__fastcall` and passed `this` in ECX plus a dummy EDX to fake it.
There are also non-template-address variants (`Thunk<&$::F>(0x..)`, `Thunk<void(int)>(0x..)`) that take the
address as a runtime argument - same machinery.

---

## 7. Calling-convention macros

`Common/Types.h:41-63` defines (blanked under `SWIG`/CLIF builds, which don't model calling conventions):

| Macro | Expands to (MSVC) |
|---|---|
| `THISCALL` | `__thiscall` |
| `FASTCALL` | `__fastcall` |
| `STDCALL`  | `__stdcall`  |
| `CDECL`    | `__cdecl`    |

Most OP2 members are `__thiscall`; a few internal helpers are `__fastcall` (e.g. `SimpleCommand`'s size
helper, `OP2Thunk<0x4102C0, uint32 FASTCALL(const void*)>`); the cstdlib shims (`OP2Alloc` etc.) are `__cdecl`.

---

## 8. Worked examples

| Form | Resolves to | For |
|---|---|---|
| `OP2Mem<0x582F8C, HANDLE&>()` | reference to the heap-handle global | read a global variable |
| `OP2Mem<PlayerImpl*&>(0x4890C3)` | reference to the player-array pointer | read/write a global pointer |
| `OP2Thunk<0x4C21F0, &OP2Alloc>(n)` | `void*(__cdecl*)(size_t)` @ `0x4C21F0`, called `(n)` | free function |
| `Thunk<0x478780, &$::CreateUnit>(…)` | `Ret(__thiscall*)(This*, …)` @ `0x478780`, called `(this, …)` | member function |

---

## 9. How `op2::abi/memory.hpp` reimplements this

Our [`../TitanAPI/include/op2/abi/memory.hpp`](../TitanAPI/include/op2/abi/memory.hpp) is a clean, minimal
re-implementation of the same technique (the relocation arithmetic and `__thiscall`-with-explicit-`this`
are standard, not anyone's IP). Mapping:

| Classic `OP2Mem` pattern | `op2::abi` equivalent |
|---|---|
| `OP2Base` / relocation math | `kImageBase` + `reloc(addr)` |
| `g_hOP2` / `GetOP2Handle()` + `Global` flag | `moduleBase()` - single lazy function-local static (always the safe form) |
| `OP2Mem<T*>(addr)` (by pointer) | `ptr<T>(addr)` |
| `OP2Mem<T&>(addr)` (by reference) | `ref<T>(addr)` |
| `OP2Mem<Addr, T>()` (cached global) | `global<Addr, T>()` |
| `OP2Thunk<Addr, &Fn>(…)` (free, cdecl) | `call<Addr, R, Args...>(…)` (+ `callStd` / `callFast`) |
| `Thunk<Addr, &$::M>(…)` (member) | `member<Addr, R, This, Args...>(self, …)` |
| addresses inline in headers | **generated** `addr::Class::Method` constants (`tools/gen-addresses.ps1`) |

Deliberate differences: we use the lazy/safe handle **always** (no eager `g_hOP2` fast path - the cost is
negligible and it removes the static-init footgun); we skip the multi-overload SFINAE `OP2Mem` gymnastics
(the call sites state intent explicitly); and addresses come from generated constants, not hand-inlined
literals. A per-address function-pointer cache (like the classic `static` in the cached overload) can be
added later if profiling wants it - `moduleBase()` is already cached, so each call is just a subtract+add.

---

## 10. The command-packet type map (bonus)

`Game/CommandPacket.h` also encodes which payload struct each command uses, via
`OP2_CMD_PACKET_DATA_FOR_DEF(CommandType::Move, MoveCommand)` (→ `CmdPacketDataFor<CommandType::Move>`).
Our extractor captures these (53 entries), and `tools/gen-command-map.ps1` emits `op2/abi/command_map.hpp` as
`CmdPayload<CommandType::X>::type = raw::XCommand` (+ `CmdPayloadT<C>`). The facade uses this to build typed
command packets safely - the foundation of the `std::expected` order API (see
[`../docs/FACADE-DESIGN.md`](../docs/FACADE-DESIGN.md)).

---

## 11. Invariants to preserve when building Layer 1

1. **Init-order safety** - resolve the module handle lazily (function-local static), never an eager global,
   so `op2::abi` is usable from static initializers.
2. **`this` first, `__thiscall`** - model members as `R(__thiscall*)(This*, Args...)`; don't fake it with
   `__fastcall` + dummy EDX.
3. **x86 / MSVC only** - the addresses are 32-bit; the calling conventions are MSVC-specific.
4. **Match packing & sizes** - command/data structs are `#pragma pack(1)`-style; cross-check every raw
   struct's size with `static_assert(sizeof(...))` against the extracted facts.
5. **Don't cache resolved object pointers across ticks** - units die and array slots are reused; resolve
   from an id each time (this is a facade rule, but it starts here).
