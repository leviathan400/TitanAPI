#pragma once
// op2/core/error.hpp - the Result / error vocabulary for the TitanAPI facade (Layer 2).
//
// Operations that can fail return Result<T> = std::expected<T, Error> - error-as-value, never exceptions.
// (Outpost 2 is compiled exception-disabled, so the facade must not throw across the DLL boundary.)

#include <expected>
#include <cstdint>

namespace op2 {

/// Failure categories. Most are detectable BEFORE we touch the engine (the class of bug that bit TethysAPI's
/// order API); asynchronous engine outcomes are observed later by polling, not here.
enum class Status : std::uint8_t {
  Ok = 0,
  NullHandle,       ///< a default/empty handle was used
  DeadUnit,         ///< unit / target no longer live
  NotOwned,         ///< acting player doesn't own the unit
  WrongType,        ///< wrong unit type for the operation
  InvalidTarget,    ///< target missing / incompatible
  InvalidLocation,  ///< off-map / impassable where required
  InvalidPlayer,    ///< player index out of range
  Unsupported,      ///< operation not valid in this state
  EngineRejected,   ///< the engine returned a failure sentinel
};

/// A small, cheap error value (no heap). `what` points at a static, null-terminated string - a `const char*`
/// so it drops straight into `printf`-family, `op2::log::line`, and `OutputDebugStringA` without conversion.
struct Error {
  Status      status = Status::Ok;
  const char* what   = "";
};

/// The result of a fallible operation. `Result<void>` for orders; `Result<T>` for things that produce a value.
template <class T = void>
using Result = std::expected<T, Error>;

/// Pre-built error values (no allocation).
namespace err {
inline constexpr Error NullHandle      { Status::NullHandle,      "null handle" };
inline constexpr Error DeadUnit        { Status::DeadUnit,        "unit not live" };
inline constexpr Error NotOwned        { Status::NotOwned,        "unit not owned by actor" };
inline constexpr Error WrongType       { Status::WrongType,       "wrong unit type" };
inline constexpr Error InvalidTarget   { Status::InvalidTarget,   "invalid target" };
inline constexpr Error InvalidLocation { Status::InvalidLocation, "location off-map" };
inline constexpr Error InvalidPlayer   { Status::InvalidPlayer,   "invalid player index" };
inline constexpr Error Unsupported     { Status::Unsupported,     "operation not valid in this state" };
inline constexpr Error EngineRejected  { Status::EngineRejected,  "engine rejected request" };
} // namespace err

/// Build an `unexpected` Error to `return` from a `Result<T>`-returning function.
inline std::unexpected<Error> fail(Error e) { return std::unexpected(e); }

/// Explicitly discard a `[[nodiscard]]` Result when fire-and-forget is intended - e.g. an AI loop that issues an
/// order every tick and doesn't branch on a momentary engine rejection. Makes "I'm not checking this on purpose"
/// visible in the code, while an ACCIDENTAL bare discard still warns. Usage: `op2::ignore(unit.move(x, y));`
template <class T>
constexpr void ignore(T&&) noexcept {}

} // namespace op2
