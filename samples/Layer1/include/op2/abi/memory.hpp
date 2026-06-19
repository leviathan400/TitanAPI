#pragma once
// op2/abi/memory.hpp - hand-written Layer-1 ABI substrate (NOT generated).
//
// Turns fixed Outpost2.exe addresses into callable functions / usable data, relocated from the preferred
// base (0x00400000) to the actual module base, so it works even if the exe was rebased (ASLR). This is our
// own clean re-implementation of the technique TethysAPI's OP2Mem/OP2Thunk use. Addresses come from the
// generated addresses.hpp, which is produced from D:\sdk\re-reference\ (facts about the binary).
//
// Mechanism (full write-up: re-reference/ABI-MECHANISM.md):
//   actualPtr = moduleBase + (address - kImageBase)        // moduleBase == kImageBase in the usual case
//   * Member functions are called via a __thiscall function pointer whose FIRST explicit parameter is
//     `this` (MSVC passes it in ECX) - the clean technique; no __fastcall + dummy-EDX hack.
//   * The module handle is resolved lazily via a function-local static, so abi calls are safe even from
//     static initializers (avoids the static-init-order fiasco).
//
// x86 / MSVC only (binds a 32-bit binary; relies on __thiscall / __cdecl / __fastcall).

#include <cstdint>
#include <type_traits>

namespace op2::abi {

inline constexpr std::uintptr_t kImageBase = 0x00400000;  ///< Outpost2.exe preferred load address.
inline constexpr std::uintptr_t kShellBase = 0x13000000;  ///< OP2Shell.dll preferred load address.

// Fixed-width aliases matching the engine's uintN naming used throughout the RE.
using u8  = std::uint8_t;  using u16 = std::uint16_t;  using u32 = std::uint32_t;  using u64 = std::uint64_t;
using i8  = std::int8_t;   using i16 = std::int16_t;   using i32 = std::int32_t;   using i64 = std::int64_t;

// Minimal import so this header doesn't drag in <windows.h>.
extern "C" __declspec(dllimport) void* __stdcall GetModuleHandleA(const char* lpModuleName);

/// Base address of the loaded Outpost2.exe. Lazy / init-once → safe to use from global constructors.
inline std::uintptr_t moduleBase() {
  static const std::uintptr_t base = [] {
    const auto h = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
    return h ? h : kImageBase;             // fall back to preferred base if not hosted by Outpost2.exe
  }();
  return base;
}

/// Relocate a preferred-base address to the actual load address.
inline std::uintptr_t reloc(std::uintptr_t address) { return moduleBase() + (address - kImageBase); }

/// Typed pointer to data living at a fixed address.
template <class T> T* ptr(std::uintptr_t address) { return reinterpret_cast<T*>(reloc(address)); }
/// Typed reference to data living at a fixed address.
template <class T> T& ref(std::uintptr_t address) { return *reinterpret_cast<T*>(reloc(address)); }
/// Init-once typed reference to a global (safe from static init).
template <std::uintptr_t Address, class T> T& global() { static T& r = ref<T>(Address); return r; }

#if defined(_MSC_VER)
/// Call an internal __cdecl / free function at a fixed address.
template <std::uintptr_t Address, class R, class... A>
R call(A... args) { return reinterpret_cast<R(__cdecl*)(A...)>(reloc(Address))(args...); }

/// Call an internal __stdcall function at a fixed address.
template <std::uintptr_t Address, class R, class... A>
R callStd(A... args) { return reinterpret_cast<R(__stdcall*)(A...)>(reloc(Address))(args...); }

/// Call an internal __fastcall function at a fixed address.
template <std::uintptr_t Address, class R, class... A>
R callFast(A... args) { return reinterpret_cast<R(__fastcall*)(A...)>(reloc(Address))(args...); }

/// Call an internal __thiscall MEMBER function at a fixed address (`this` passed explicitly → ECX).
template <std::uintptr_t Address, class R, class This, class... A>
R member(This* self, A... args) {
  return reinterpret_cast<R(__thiscall*)(This*, A...)>(reloc(Address))(self, args...);
}
#endif  // _MSC_VER

} // namespace op2::abi
