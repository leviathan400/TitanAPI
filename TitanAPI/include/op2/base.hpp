#pragma once
// op2/base.hpp — Module 8: declarative base layout (the OP2Helper "BaseBuilder", modernized for C++23).
//
// Describe a colony as DATA — beacons, buildings, tube/wall runs, vehicles — then stamp the whole thing onto the
// map for a player with one call. This is the author-ergonomics layer: instead of a hundred createUnit() calls,
// you write a BaseLayout once and reuse it (the `offset` arg lets the same layout seed several bases at different
// map locations — handy for symmetric multiplayer starts). Coordinates are VISIBLE tiles throughout.
//
// It's pure Layer-2 sugar: createBase just drives Game::createMine / createUnit / createTubeLine / createWallLine,
// so it inherits all their safety (off-map rejection, the weaponless-combat-vehicle auto-arm, etc.).

#include "op2/game.hpp"
#include <vector>

namespace op2 {

/// A mining beacon (ore deposit) to place. Defaults to a common-ore, Bar-2 deposit.
struct BeaconSpec   { Location at; abi::MineType ore = abi::MineType::CommonOre; abi::OreYield yield = abi::OreYield::Bar2; };
/// A structure to place. `cargo` preloads a kit/weapon where the type supports it (e.g. a Guard Post's weapon).
struct BuildingSpec { Location at; MapID type; MapID cargo = MapID::None; };
/// A vehicle to place, optionally armed (`weaponCargo`) and facing `rotation` (0-7).
struct VehicleSpec  { Location at; MapID type; MapID weaponCargo = MapID::None; int rotation = 0; };
/// A straight (or L-shaped) tube/wall run between two tiles, inclusive.
struct LineSpec     { Location from, to; };

/// A whole base described as data. Any list may be left empty. Build it with createBase().
struct BaseLayout {
  std::vector<BeaconSpec>   beacons;
  std::vector<BuildingSpec> buildings;
  std::vector<LineSpec>     tubes;                 // laid as MapID::Tube
  std::vector<LineSpec>     walls;                 // laid as MapID::Wall
  std::vector<VehicleSpec>  vehicles;
};

/// How many of each kind createBase() actually placed (a createUnit can reject an off-map / occupied tile).
struct BaseResult {
  int beacons = 0, buildings = 0, vehicles = 0, tubeLines = 0, wallLines = 0;
  [[nodiscard]] int placedUnits() const { return beacons + buildings + vehicles; }
};

/// Stamp `layout` onto the map for `owner`, every coordinate shifted by `offset` (default none). Returns the
/// per-kind placement counts. Order: beacons, then buildings, then vehicles, then tube runs, then wall runs.
inline BaseResult createBase(Player owner, const BaseLayout& layout, Location offset = { 0, 0 }) {
  BaseResult r;
  const auto shift = [&](Location p) { return Location{ p.x + offset.x, p.y + offset.y }; };

  for (const BeaconSpec& b : layout.beacons)
    if (Game::createMine(shift(b.at), b.ore, b.yield)) ++r.beacons;
  for (const BuildingSpec& b : layout.buildings)
    if (Game::createUnit(b.type, shift(b.at), owner, b.cargo)) ++r.buildings;
  for (const VehicleSpec& v : layout.vehicles)
    if (Game::createUnit(v.type, shift(v.at), owner, v.weaponCargo, v.rotation)) ++r.vehicles;
  for (const LineSpec& t : layout.tubes) { Game::createTubeLine(shift(t.from), shift(t.to)); ++r.tubeLines; }
  for (const LineSpec& w : layout.walls) { Game::createWallLine(MapID::Wall, shift(w.from), shift(w.to)); ++r.wallLines; }
  return r;
}

} // namespace op2
