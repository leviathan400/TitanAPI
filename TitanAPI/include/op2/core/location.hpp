#pragma once
// op2/core/location.hpp — map coordinate value type for the facade.
//
// Authors use IN-GAME (visible) tile coordinates — what you see on screen. Outpost 2 keeps an off-screen
// map border, so the engine's internal tile is offset; the facade adds the pad at the engine boundary.

#include <cstdint>

namespace op2 {

/// Visible-tile -> engine-internal-tile pad. Verified in-game across buildings AND a vehicle: with this pad,
/// a unit placed at visible (x,y) appears in-game at exactly (x,y). Matches OP2Helper's MkXY = (x+31, y-1).
inline constexpr int kPadX = 31;
inline constexpr int kPadY = -1;

/// An in-game (visible) tile coordinate.
struct Location {
  int x = 0, y = 0;

  constexpr Location() = default;
  constexpr Location(int x_, int y_) : x{x_}, y{y_} {}

  /// Engine-internal tile coords (visible + pad). The engine's own Location is a plain { int x, int y }.
  [[nodiscard]] constexpr int engineX() const { return x + kPadX; }
  [[nodiscard]] constexpr int engineY() const { return y + kPadY; }

  /// Basic sanity check (non-negative tile). A real bounds check needs the loaded map; refined when
  /// GameMap is wrapped.
  [[nodiscard]] constexpr bool onMap() const { return x >= 0 && y >= 0; }

  /// Engine pixel coordinates (tile*32, +16 when centered) — what the engine's pathfinder waypoints use.
  [[nodiscard]] constexpr int enginePixelX(bool centered = true) const { return engineX() * 32 + (centered ? 16 : 0); }
  [[nodiscard]] constexpr int enginePixelY(bool centered = true) const { return engineY() * 32 + (centered ? 16 : 0); }

  /// This location packed as an engine Waypoint's 32-bit word: pixelX in bits 0..14, pixelY in bits 15..28
  /// (the engine's `union Waypoint { uint32 x:15; uint32 y:14; uint32 :3; }`). Move uses both centered; Build
  /// uses x centered, y un-centered.
  [[nodiscard]] constexpr std::uint32_t waypointBits(bool xCentered = true, bool yCentered = true) const {
    return (std::uint32_t(enginePixelX(xCentered)) & 0x7FFFu) | ((std::uint32_t(enginePixelY(yCentered)) & 0x3FFFu) << 15);
  }

  friend constexpr bool operator==(Location, Location) = default;
};

} // namespace op2
