#pragma once
// op2/game.hpp - Game: the static facade for global operations (create units, get players, ...).

#include "op2/core/error.hpp"
#include "op2/core/location.hpp"
#include "op2/unit.hpp"
#include "op2/player.hpp"
#include "op2/abi/memory.hpp"
#include "op2/abi/enums.hpp"
#include <ranges>
#include <cstddef>
#include <vector>

namespace op2 {

using MapID         = abi::MapID;          ///< engine unit / building / weapon ids (op2::MapID::CommandCenter, ...)
using MoraleLevels  = abi::MoraleLevels;   ///< Excellent / Good / Fair / Poor / Terrible
using UnitDirection = abi::UnitDirection;  ///< unit facing: East=0, SouthEast, South, SouthWest, West, NorthWest, North, NorthEast

/// Engine map location - `{int x, int y}`, passed BY VALUE to engine functions that take a `Location`. It is
/// 8 bytes, so under __fastcall it is passed on the stack (the compiler matches the engine's ABI). Build it
/// from a visible Location's engine coordinates: `EngineLoc{ loc.engineX(), loc.engineY() }`.
struct EngineLoc { int x, y; };

// =====================================================================================================================
// Module 3 - enumeration. UnitRange is a std::ranges input view over every LIVE unit (it scans the engine's
// MapObject array and skips dead slots), yielding op2::Unit by value with its owner resolved - so the units
// are immediately orderable/readable. Compose with std::views, e.g.  Game::units() | std::views::filter(...).
// =====================================================================================================================
class UnitRange : public std::ranges::view_interface<UnitRange> {
public:
  struct sentinel {};
  class iterator {
  public:
    using value_type       = Unit;
    using difference_type   = std::ptrdiff_t;
    using iterator_concept  = std::input_iterator_tag;

    iterator() = default;
    iterator(int idx, int max) : idx_{ idx }, max_{ max } { skipDead(); }

    [[nodiscard]] Unit operator*() const { return Unit{ idx_, abi::moOwner(abi::mapObject(idx_)) }; }
    iterator& operator++()    { ++idx_; skipDead(); return *this; }
    void      operator++(int) { ++*this; }
    bool operator==(sentinel) const { return idx_ >= max_; }

  private:
    void skipDead() { while ((idx_ < max_) && !abi::moIsLive(abi::mapObject(idx_))) ++idx_; }
    int idx_ = 1;   // unit ids start at 1 (slot 0 is the null/reserved entry)
    int max_ = 1;
  };

  [[nodiscard]] iterator begin() const { return iterator{ 1, abi::maxNumUnits() }; }
  [[nodiscard]] sentinel end()   const { return {}; }
};

struct Game {
  /// Handle for player slot `n`.
  static Player player(int n) { return Player{ n }; }

  /// Current game tick (the smallest slice of game time). GameImpl singleton @0x56EA98, tick_ @+132.
  static int tick() { return *reinterpret_cast<int*>(reinterpret_cast<char*>(abi::reloc(0x56EA98)) + 132); }
  /// Current mark (tick / 100) - the time unit shown in the in-game message log ("Current Mark").
  static int mark() { return tick() / 100; }

  // --- enumeration (Module 3) - all return ranges, composable with std::views ---
  /// Every live unit in the game (any player).
  static UnitRange units() { return {}; }
  /// Live units owned by `playerNum`.
  static auto unitsOf(int playerNum) {
    return units() | std::views::filter([playerNum](Unit u) { return u.ownerId() == playerNum; });
  }
  /// Live units of a given type (any player).
  static auto unitsOfType(MapID type) {
    return units() | std::views::filter([type](Unit u) { return u.type() == type; });
  }
  /// Live units whose tile is within the visible rect [topLeft, bottomRight], inclusive.
  static auto unitsInRect(Location topLeft, Location bottomRight) {
    return units() | std::views::filter([topLeft, bottomRight](Unit u) {
      const Location l = u.location();
      return (l.x >= topLeft.x) && (l.x <= bottomRight.x) && (l.y >= topLeft.y) && (l.y <= bottomRight.y);
    });
  }

  /// True for unit types that REQUIRE a weapon - the combat-vehicle chassis (Lynx/Panther/Tiger) and the Guard
  /// Post. OP2 CRASHES if one of these is created weaponless, so createUnit auto-arms them (see below).
  static constexpr bool isWeaponPlatform(MapID type) {
    return type == MapID::Lynx || type == MapID::Panther || type == MapID::Tiger || type == MapID::GuardPost;
  }

  /// Creates a unit/building for `owner` at a visible tile. Returns the new Unit (or an Error).
  /// Wraps Game::CreateUnit @0x478780 (__fastcall) - verified in-game.
  /// @note If `type` is a weapon platform (combat vehicle / Guard Post) and no `weaponCargo` is given, it is
  ///       auto-armed with a Laser - a weaponless combat vehicle crashes OP2, so this is never the author's fault.
  static Result<Unit> createUnit(MapID type, Location at, Player owner, MapID weaponCargo = MapID::None,
                                 UnitDirection facing = UnitDirection::East) {
    if (!owner.valid()) return fail(err::InvalidPlayer);
    if (!at.onMap())    return fail(err::InvalidLocation);

    // Guard against the weaponless-combat-vehicle crash: give it a Laser if the author didn't pick a weapon.
    if (weaponCargo == MapID::None && isWeaponPlatform(type)) weaponCargo = MapID::Laser;

    const EngineLoc loc{ at.engineX(), at.engineY() };   // engine Location is {int x, int y}, passed by value
    int id = 0;  // CreateUnit fills this (the Unit& out-param)
    abi::callFast<0x478780, int>(&id, int(type), loc, owner.index(), int(weaponCargo), int(facing));
    if (id < 0) return fail(err::EngineRejected);
    return Unit{ id, owner.index() };   // carry the owner so the unit can dispatch orders
  }

  /// Places a mining beacon (ore deposit) at a visible tile and returns it. The beacon starts UNSURVEYED:
  /// a Robo-Surveyor reveals it (or call `Unit::survey`), then a Robo-Miner builds a mine on it (`Unit::deploy`),
  /// then Cargo Trucks run a route to a smelter (`Unit::mine`). Wraps the engine's CreateMine core @0x478940
  /// (variant 0; skips the MineManager variant lookup). The new beacon is the head of Gaia's beacon list.
  static Result<Unit> createMine(Location at, abi::MineType ore = abi::MineType::CommonOre,
                                 abi::OreYield yield = abi::OreYield::Bar2) {
    if (!at.onMap()) return fail(err::InvalidLocation);
    if (!abi::callFast<0x478940, int>(int(abi::MapID::MiningBeacon), at.engineX(), at.engineY(),
                                      int(ore), int(yield), 0))
      return fail(err::EngineRejected);
    const int id = abi::newestBeaconId();
    if (id <= 0) return fail(err::EngineRejected);
    return Unit{ id, 6 };   // beacons belong to Gaia (player 6)
  }

  /// Place a single tube on a visible tile (wraps CreateWall/Tube @0x478AA0).
  static void createTube(Location at) {
    abi::callFast<0x478AA0, int>(at.engineX(), at.engineY(), 0, int(abi::MapID::Tube));
  }

  /// Lay a straight (horizontal or vertical) line of tubes between two visible tiles, inclusive. A non-straight
  /// span is laid as an L (horizontal leg along `from.y`, then vertical leg along `to.x`).
  static void createTubeLine(Location from, Location to) {
    const auto lo = [](int a, int b) { return a < b ? a : b; };
    const auto hi = [](int a, int b) { return a > b ? a : b; };
    if (from.x == to.x) {
      for (int y = lo(from.y, to.y); y <= hi(from.y, to.y); ++y) createTube({ from.x, y });
    } else if (from.y == to.y) {
      for (int x = lo(from.x, to.x); x <= hi(from.x, to.x); ++x) createTube({ x, from.y });
    } else {
      for (int x = lo(from.x, to.x); x <= hi(from.x, to.x); ++x) createTube({ x, from.y });
      for (int y = lo(from.y, to.y); y <= hi(from.y, to.y); ++y) createTube({ to.x, y });
    }
  }

  /// Post a message to the in-game Communications log (local player, no map location). `soundID` is the alert
  /// sound played when it arrives (0 = the default message sound; e.g. SoundID Beep2=85 to ping the player).
  /// Wraps MessageLog::AddMessage @0x439070 on the MessageLog singleton @0x54FCF0.
  static void addMessage(const char* text, int soundID = 0) {
    char* log = reinterpret_cast<char*>(abi::reloc(0x54FCF0));      // MessageLog singleton object
    abi::member<0x439070, int>(log, 0, -1, text, soundID);         // AddMessage(pixelX=0, pixelY=-1, msg, soundID)
  }
  /// Post a Communications-log message tied to a map LOCATION - clicking it jumps the view to `at`. (OP2Helper's
  /// AddMapMessage.) Same MessageLog::AddMessage @0x439070, but with the tile's centered pixel coords.
  static void addMessage(Location at, const char* text, int soundID = 0) {
    char* log = reinterpret_cast<char*>(abi::reloc(0x54FCF0));
    abi::member<0x439070, int>(log, at.enginePixelX(), at.enginePixelY(), text, soundID);
  }

  /// Issue a ctGameOpt packet locally (engine poke - morale, daylight, …), built as a GameOptCommand and
  /// dispatched via the local player. Set at tick 0 (InitProc) to avoid the "cheated game!" alert.
  static Result<void> setGameOpt(abi::GameOpt opt, abi::u16 param1 = 0, abi::u16 param2 = 0) {
    abi::CmdBuilder b{ abi::CommandType::GameOpt };
    b.field<abi::u16>(0).field<abi::u16>(abi::u16(opt)).field<abi::u16>(param1).field<abi::u16>(param2);
    return player(0).issue(b.build());
  }

  // --- morale (OP2MissionSDK / OP2Lua-style named controls) -------------------------------------------------
  // `forceMorale*` LOCKS morale at a level so it stops fluctuating (i.e. holds it steady); `freeMoraleLevel`
  // lets it vary again. playerNum -1 = all players. Call at tick 0 (InitProc) - forcing morale after the game
  // has started raises the engine's "cheated game!" alert.
  static Result<void> forceMoraleGreat (int playerNum = -1) { return setGameOpt(abi::GameOpt::ForceMoraleExcellent, abi::u16(playerNum)); }
  static Result<void> forceMoraleGood  (int playerNum = -1) { return setGameOpt(abi::GameOpt::ForceMoraleGood,      abi::u16(playerNum)); }
  static Result<void> forceMoraleOK    (int playerNum = -1) { return setGameOpt(abi::GameOpt::ForceMoraleFair,      abi::u16(playerNum)); }
  static Result<void> forceMoralePoor  (int playerNum = -1) { return setGameOpt(abi::GameOpt::ForceMoralePoor,      abi::u16(playerNum)); }
  static Result<void> forceMoraleRotten(int playerNum = -1) { return setGameOpt(abi::GameOpt::ForceMoraleTerrible,  abi::u16(playerNum)); }
  static Result<void> freeMoraleLevel  (int playerNum = -1) { return setGameOpt(abi::GameOpt::FreeMoraleLevel,      abi::u16(playerNum)); }

  /// Generic form, if you have a MoraleLevels value in hand.
  static Result<void> forceMorale(MoraleLevels level, int playerNum = -1) {
    return setGameOpt(abi::GameOpt(int(abi::GameOpt::ForceMoraleExcellent) + int(level)), abi::u16(playerNum));
  }

  // --- Module 6: world / disasters / environment (TethysGame globals) ---------------------------------------
  // Disasters take VISIBLE tiles (converted to engine tiles) and play their normal warning, then strike. Each
  // engine creator returns the disaster's MapObject pointer in EAX (not sret); we report success/failure.

  // Each disaster returns the new Disaster MapObject pointer; if `now`, we strike immediately (skip the warning
  // period) via the probe-verified startDisasterNow (the engine's StartDisaster is inline, not an exported call).

  /// Meteor strike at `where`. size: 0 = large, 1 = medium, 2 = small, -1 = random. `now` strikes immediately
  /// instead of after the warning. (CreateMeteor @0x4783B0.)
  static Result<void> createMeteor(Location where, int size = -1, bool now = false) {
    if (!where.onMap()) return fail(err::InvalidLocation);
    void* d = abi::callFast<0x4783B0, void*>(where.engineX(), where.engineY(), size);
    if (!d) return fail(err::EngineRejected);
    if (now) abi::startDisasterNow(d);
    return {};
  }

  /// Earthquake centred at `where` with the given `magnitude`. `now` strikes immediately. (CreateEarthquake @0x478320.)
  static Result<void> createEarthquake(Location where, int magnitude, bool now = false) {
    if (!where.onMap()) return fail(err::InvalidLocation);
    void* d = abi::callFast<0x478320, void*>(where.engineX(), where.engineY(), magnitude);
    if (!d) return fail(err::EngineRejected);
    if (now) abi::startDisasterNow(d);
    return {};
  }

  /// Volcanic eruption (spreading lava) at `where`. `now` erupts immediately. (CreateEruption @0x4782E0.)
  static Result<void> createEruption(Location where, int spreadSpeed, bool now = false) {
    if (!where.onMap()) return fail(err::InvalidLocation);
    void* d = abi::callFast<0x4782E0, void*>(where.engineX(), where.engineY(), spreadSpeed);
    if (!d) return fail(err::EngineRejected);
    if (now) abi::startDisasterNow(d);
    return {};
  }

  /// Electrical storm sweeping `start` -> `end` over `duration` ticks. `now` strikes immediately. (CreateLightning @0x4783E0.)
  static Result<void> createStorm(Location start, Location end, int duration, bool now = false) {
    void* d = abi::callFast<0x4783E0, void*>(start.engineX(), start.engineY(), duration,
                                             end.engineX(), end.engineY());
    if (!d) return fail(err::EngineRejected);
    if (now) abi::startDisasterNow(d);
    return {};
  }

  /// Vortex (tornado) sweeping `start` -> `end` over `duration` ticks. `now` strikes immediately. (CreateTornado @0x478350.)
  static Result<void> createVortex(Location start, Location end, int duration, bool now = false) {
    void* d = abi::callFast<0x478350, void*>(start.engineX(), start.engineY(), duration,
                                             end.engineX(), end.engineY(), 0 /*bImmediate*/);
    if (!d) return fail(err::EngineRejected);
    if (now) abi::startDisasterNow(d);
    return {};
  }

  /// Place a wall tile - `type` is MapID::Wall / LavaWall / MicrobeWall / Tube. (CreateWall @0x478AA0.)
  static void createWall(MapID type, Location at) {
    abi::callFast<0x478AA0, int>(at.engineX(), at.engineY(), 0, int(type));
  }
  /// Lay a straight (or L-shaped) line of walls of `type` between two visible tiles, inclusive - the wall
  /// counterpart of createTubeLine (horizontal leg along from.y, then vertical leg along to.x for a diagonal).
  static void createWallLine(MapID type, Location from, Location to) {
    const auto lo = [](int a, int b) { return a < b ? a : b; };
    const auto hi = [](int a, int b) { return a > b ? a : b; };
    if (from.x == to.x) {
      for (int y = lo(from.y, to.y); y <= hi(from.y, to.y); ++y) createWall(type, { from.x, y });
    } else if (from.y == to.y) {
      for (int x = lo(from.x, to.x); x <= hi(from.x, to.x); ++x) createWall(type, { x, from.y });
    } else {
      for (int x = lo(from.x, to.x); x <= hi(from.x, to.x); ++x) createWall(type, { x, from.y });
      for (int y = lo(from.y, to.y); y <= hi(from.y, to.y); ++y) createWall(type, { to.x, y });
    }
  }

  /// Spread Blight (microbe) onto the map at `where` - always immediate. (CreateBlight @0x476EA0, enable=1.)
  static void createBlight(Location where) {
    abi::callFast<0x476EA0, void, EngineLoc, int>(EngineLoc{ where.engineX(), where.engineY() }, 1);
  }
  /// Clear Blight (microbe) from the map at `where`. Same engine call as createBlight with enable=0 (the
  /// CreateBlight thunk's second arg toggles spread on/off). Mirrors OP2Lua game.unset_blight.
  static void unsetBlight(Location where) {
    abi::callFast<0x476EA0, void, EngineLoc, int>(EngineLoc{ where.engineX(), where.engineY() }, 0);
  }
  /// Set how fast LAVA spreads (OP2's named scale: VerySlow 15, Slow 45, Medium 75, Fast 105, VeryFast 135 -
  /// higher = faster). LavaManager is a fixed singleton object AT 0x54EFE0 (same object-at-address pattern as
  /// Random/SoundManager - OP2Mem<addr,T*> casts to the address, it does NOT deref a stored pointer); its
  /// SetLavaSpeed is @0x432F00. Mirrors OP2Lua game.set_lava_speed. (Proven working via the cold-front sample.)
  static void setLavaSpeed(int speed) {
    abi::member<0x432F00, void>(reinterpret_cast<void*>(abi::reloc(0x54EFE0)), speed);
  }
  /// Set how fast BLIGHT (microbe) spreads - same named scale as lava. BlightManager is a fixed singleton object
  /// AT 0x57B7D8; its SetSpreadSpeed is @0x49E310. Mirrors OP2Lua game.set_blight_speed.
  static void setBlightSpeed(int speed) {
    abi::member<0x49E310, void>(reinterpret_cast<void*>(abi::reloc(0x57B7D8)), speed);
  }

  /// Marker decal graphic for createMarker(). Mirrors Tethys MarkerType.
  enum class Marker : int { Circle = 0, DNA = 1, Beaker = 2 };
  /// Place a visual marker decal on the map at `where` and return its Unit handle (use .remove() to clear it).
  /// CreateMarker @0x478BB0 - FASTCALL(Unit& out, int x, int y, int type); fills `out` like CreateUnit's sret.
  /// Mirrors OP2Lua game.create_marker.
  static Unit createMarker(Location where, Marker type = Marker::Circle) {
    int id = 0;
    abi::callFast<0x478BB0, int>(&id, where.engineX(), where.engineY(), int(type));
    return Unit{ id };
  }

  /// Random integer in [0, range), from the network-synced gameplay RNG (Random @0x56BE20, Rand @0x46F060).
  static int getRand(int range) {
    return abi::member<0x46F060, int>(reinterpret_cast<void*>(abi::reloc(0x56BE20)), range);
  }

  /// Number of players in the game (human + AI). GameImpl singleton @0x56EA98, numPlayers_ @+104.
  static int numPlayers() {
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(abi::reloc(0x56EA98)) + 104);
  }
  /// Number of HUMAN players. GameImpl::numHumanPlayers_ @+108.
  static int numHumans() {
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(abi::reloc(0x56EA98)) + 108);
  }
  /// Whether disasters are enabled for this game (the Colony Games "disasters" menu setting). Lets a mission's
  /// random-disaster director respect the player's choice. GameImpl::gameStartInfo_.startupFlags @+164 (probe-
  /// verified); StartupFlags.disastersEnabled is bit 0. Mirrors OP2Lua game.disasters_enabled.
  static bool disastersEnabled() {
    return (*reinterpret_cast<abi::u32*>(reinterpret_cast<char*>(abi::reloc(0x56EA98)) + 164) & 1u) != 0;
  }

  /// Count `player`'s LIVE units of `type` (use MapID::Any for all types). Built on the Module 3 enumeration.
  /// (OP2Lua's `player:unit_count`.)
  static int playerUnitCount(int player, MapID type) {
    int n = 0;
    for (Unit u : unitsOf(player)) if ((type == MapID::Any) || (u.type() == type)) ++n;
    return n;
  }
  /// Does `player` own any live unit of `type`? (MapID::Any = any unit at all.) OP2Lua's `player:owns_any`.
  static bool playerOwnsAny(int player, MapID type) { return playerUnitCount(player, type) > 0; }

  /// A structure type's tile footprint (core size). OP2Lua's `game.footprint`. Reads the type stats table.
  struct Footprint { int width, height; };
  static Footprint footprint(MapID structureType) {
    int w = 0, h = 0; abi::buildingSize(int(structureType), w, h); return { w, h };
  }

  /// All mining beacons on the map, optionally filtered by ore type (-1 = any, 0 = Common, 1 = Rare). Mirrors
  /// OP2Lua's `game.beacons` - the "perception" query a surveyor AI uses to find ore deposits. Built on the
  /// Module 3 enumeration (scans the live unit array for MiningBeacons), so it naturally skips wreckage /
  /// fumaroles / magma vents.
  static std::vector<Unit> beacons(int oreType = -1) {
    std::vector<Unit> out;
    for (Unit u : unitsOfType(MapID::MiningBeacon))
      if (oreType < 0 || u.oreType() == oreType) out.push_back(u);
    return out;
  }

  /// Find a clear tile near `near` where a unit/structure of `type` can be placed (OP2Lua's `game.find_place`).
  /// Returns the visible tile, or an error if none found. MapImpl::FindUnitPlacementLocation @0x4367D0
  /// (thiscall: unitType, `where` by value, out-pointer).
  static Result<Location> findPlace(MapID type, Location near) {
    char* map = reinterpret_cast<char*>(abi::reloc(0x54F7F8));        // MapImpl singleton
    EngineLoc out{ 0, 0 };
    const int ok = abi::member<0x4367D0, int>(map, int(type),
                                              EngineLoc{ near.engineX(), near.engineY() }, &out);
    if (!ok) return fail(err::InvalidLocation);
    return Location{ out.x - kPadX, out.y - kPadY };                  // engine tile -> visible tile
  }

  /// Drop salvageable wreckage that grants tech `techID` (must be 8000..12095) when carried to a Spaceport.
  /// (CreateWreckage @0x4789F0.)
  static Result<void> createWreckage(Location where, int techID, bool discovered = false) {
    if (!where.onMap())                  return fail(err::InvalidLocation);
    if (techID < 8000 || techID > 12095) return fail(err::InvalidTarget);
    return abi::callFast<0x4789F0, int>(where.engineX(), where.engineY(), techID, discovered ? 1 : 0)
             ? Result<void>{} : fail(err::EngineRejected);
  }

  /// Play a non-positional game sound for `toPlayer` (-1 = all players). SoundManager singleton @0x56D250,
  /// AddGameSound @0x47EFD0.
  static void addGameSound(int soundID, int toPlayer = -1) {
    char* mgr = reinterpret_cast<char*>(abi::reloc(0x56D250));
    abi::member<0x47EFD0, void>(mgr, soundID, toPlayer);
  }

  /// Play a sound positioned at a visible tile (the engine pans/attenuates it by location). AddMapSound @0x47EDB0.
  static void addMapSound(int soundID, Location at) {
    char* mgr = reinterpret_cast<char*>(abi::reloc(0x56D250));
    abi::member<0x47EDB0, void>(mgr, at.enginePixelX(), at.enginePixelY(), soundID);
  }

  /// Light the entire map (no night anywhere). (GameOpt::DaylightEverywhere - set during InitProc.)
  static Result<void> setDaylightEverywhere(bool on) {
    return setGameOpt(abi::GameOpt::DaylightEverywhere, abi::u16(on ? 1 : 0));
  }
  /// Make the day/night terminator sweep across the map. (GameOpt::DaylightMoves - set during InitProc.)
  static Result<void> setDaylightMoves(bool on) {
    return setGameOpt(abi::GameOpt::DaylightMoves, abi::u16(on ? 1 : 0));
  }
};

// Player::units - defined here (not in player.hpp) because it returns Units and iterates Game::unitsOf, both of
// which are only complete at this point (player.hpp can't include unit.hpp/game.hpp without a cycle).
inline std::vector<Unit> Player::units(abi::MapID type) const {
  std::vector<Unit> out;
  if (!valid()) return out;
  for (Unit u : Game::unitsOf(index()))
    if (type == abi::MapID::Any || u.type() == type) out.push_back(u);
  return out;
}

// =====================================================================================================================
// Module 6 - GameMap: terrain tile access (wraps the engine's GameMap over the MapImpl singleton). Coordinates
// are VISIBLE tiles. Tile-property READS (cellType / lava / microbe) need the engine's padded tile-array index
// math and are deferred to a later pass; the direct-thunk graphics/light/lava-flag setters are here now.
// =====================================================================================================================

/// Terrain cell types - the passability class the engine assigns each tile (subset of the engine's CellType).
enum class CellType : int {
  FastPassible1 = 0,   ///< rock vegetation
  Impassible2,         ///< meteor craters, cracks/crevasses
  SlowPassible1,       ///< lava rock (dark)
  SlowPassible2,       ///< rippled dirt / lava rock bumps
  MediumPassible1,     ///< dirt
  MediumPassible2,     ///< lava rock
  Impassible1,         ///< dirt/rock/lava-rock mound / ice / volcano
  FastPassible2,       ///< rock
  NorthCliffs,
  CliffsHighSide,
  CliffsLowSide,
  VentsAndFumaroles,   ///< fumaroles (passable by GeoCons)
};

struct GameMap {
  /// Graphics (mapping) tile index at `where`. (GameMap::GetTile @0x476D00 - takes a Location by value.)
  static int getTile(Location where) {
    return abi::callFast<0x476D00, int, EngineLoc>(EngineLoc{ where.engineX(), where.engineY() });
  }

  /// Set the graphics tile index at `where`. (GameMap::SetTile @0x476D80.)
  static void setTile(Location where, int tileIndex) {
    abi::callFast<0x476D80, void, EngineLoc, int>(EngineLoc{ where.engineX(), where.engineY() }, tileIndex);
  }

  /// Mark whether lava is allowed to flow onto `where`. (GameMap::SetLavaPossible @0x476F20.)
  static void setLavaPossible(Location where, bool possible) {
    abi::callFast<0x476F20, void, EngineLoc, int>(EngineLoc{ where.engineX(), where.engineY() }, possible ? 1 : 0);
  }

  /// Set the initial daylight position (0 .. map width). Call during mission setup. (SetInitialLightLevel @0x476F90.)
  static void setInitialLightLevel(int lightPosition) {
    abi::callFast<0x476F90, void, int>(lightPosition);
  }

  // --- tile-property reads (MapImpl::Tile) - offsets pinned by offsetof-probe; pMapObjArray_=80 validated ---

  /// Map height in tiles. (MapImpl::tileHeight_ @+24.)
  static int getHeight() { return mapInt(24); }
  /// Map width in tiles (with off-screen padding folded out, matching the in-game status-bar coordinate space).
  /// (MapImpl::tileWidth_ @+16 / paddingOffsetTileX_ @+56.)
  static int getWidth() { return mapInt(16) / ((mapInt(56) != 0) ? 2 : 1); }

  /// Terrain passability class at `where`. (TileData.cellType - bits 0-4.)
  static CellType getCellType(Location where)     { return CellType(tileData(where) & 0x1Fu); }
  /// Is lava currently on `where`? (TileData.lava - bit 27.)
  static bool getLavaPresent(Location where)      { return (tileData(where) >> 27) & 1u; }
  /// Can lava flow onto `where`? (TileData.lavaPossible - bit 28.)
  static bool getLavaPossible(Location where)     { return (tileData(where) >> 28) & 1u; }
  /// Is microbe (Blight) on `where`? (TileData.microbe - bit 30.)
  static bool getMicrobe(Location where)          { return (tileData(where) >> 30) & 1u; }
  /// Is a wall or building on `where`? (TileData.wallOrBuilding - bit 31.)
  static bool getWallOrBuilding(Location where)   { return (tileData(where) >> 31) & 1u; }

  /// The unit occupying `where` - the engine records a MapUnit index per tile (TileData.unitIndex, bits 16-26).
  /// This is the O(1), FOOTPRINT-AWARE way to answer "what is built on this tile": every tile a building covers
  /// holds that building's index, so it matches anywhere on the footprint (unlike `Unit::location()`, which is
  /// only the building's anchor tile). Maintained for STRUCTURES (buildings / walls / tubes); returns a null Unit
  /// when no wall or building occupies the tile. (Vehicles move and are not tracked per-tile - enumerate for
  /// those.) Example: `GameMap::unitOnTile(at).type() == MapID::Spaceport`.
  static Unit unitOnTile(Location where) {
    const abi::u32 t = tileData(where);
    if (!((t >> 31) & 1u)) return Unit{};               // bit 31: no wall/building here -> null handle
    return Unit{ int((t >> 16) & 0x7FFu) };             // bits 16-26: the MapUnit index on this tile
  }

  /// Is this single tile buildable - on-map, passable terrain, not already occupied by a wall/building, and no
  /// lava? (OP2Lua's `game.can_place`.) Reads tile flags directly; deliberately NOT the engine's
  /// CanPlaceStructure thunk, which dereferences script-invalid global state and crashes even for a valid tile
  /// (OP2Lua's hard-won note). Check every tile of a footprint to validate a whole site.
  static bool canPlace(Location where) {
    return where.onMap() && (where.x < getWidth()) && (where.y < getHeight())
           && cellPassable(int(getCellType(where))) && !getWallOrBuilding(where) && !getLavaPresent(where);
  }

  /// Ring outward from `from` (up to `maxRadius` tiles) for the nearest spot where a structure of `type`'s WHOLE
  /// footprint - including its 1-tile build-clearance border - is buildable, and return the DoBuild anchor (the
  /// building's bottom-right tile, visible coords) ready to hand to `Unit::build()`. Error if none found.
  /// Mirrors OP2Lua's `game.find_build_site`. MapObjectType::GetTileRect is pure arithmetic (size ± a 2-tile
  /// border, centered on the tile), so we compute the rects here from the type's footprint - no engine call.
  static Result<Location> findBuildSite(MapID type, Location from, int maxRadius = 30) {
    int w = 0, h = 0;
    abi::buildingSize(int(type), w, h);
    if (w <= 0 || h <= 0) return fail(err::WrongType);                       // not a building type
    for (int r = 0; r <= maxRadius; ++r)
      for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx) {
          const int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
          if ((ax > ay ? ax : ay) != r) continue;                           // only the ring shell at radius r
          const Location center{ from.x + dx, from.y + dy };
          const int tw = w + 2, th = h + 2;                                 // total rect = footprint + border
          const int tx1 = center.x - tw / 2, ty1 = center.y - th / 2;
          bool ok = true;
          for (int ty = ty1; ok && ty < ty1 + th; ++ty)
            for (int tx = tx1; tx < tx1 + tw; ++tx)
              if (!canPlace(Location{ tx, ty })) { ok = false; break; }
          if (ok) {
            // The DoBuild anchor Unit::build() expects is the building's TOTAL-footprint bottom-right (the core
            // plus the 1-tile build border) - build()'s rect spans (anchor-(w+1) .. anchor), which centers the
            // core on `center`. Returning the *core* bottom-right (one tile short) shifts the structure up-left
            // by (1,1) and can make its rect collide with a neighbour, so the build is silently rejected.
            const int cx1 = center.x - w / 2, cy1 = center.y - h / 2;        // core top-left
            return Location{ cx1 + w, cy1 + h };                             // total-footprint bottom-right
          }
        }
    return fail(err::InvalidLocation);
  }

private:
  /// Terrain that blocks building/movement: the two impassibles, the three cliff types, and the three wall
  /// cell-types (matches OP2Lua's CellPassable). Everything else is traversable/buildable terrain.
  static bool cellPassable(int ct) {
    switch (ct) {
      case 1: case 6:                 // Impassible2 / Impassible1
      case 8: case 9: case 10:        // NorthCliffs / CliffsHighSide / CliffsLowSide
      case 23: case 24: case 25:      // NormalWall / MicrobeWall / LavaWall
        return false;
      default: return true;
    }
  }
  /// Read an int field of the MapImpl singleton (@0x54F7F8) at byte offset `off`.
  static int mapInt(int off) { return *reinterpret_cast<int*>(reinterpret_cast<char*>(abi::reloc(0x54F7F8)) + off); }

  /// Read the packed TileData u32 at `where` (visible -> engine tile). Mirrors MapImpl::Tile():
  ///   offset = ((mask&x)&31) + ((((mask&x)>>5) << log2TileHeight_) + y) << 5),  arr = pTileArray_.
  static abi::u32 tileData(Location where) {
    char* const map = reinterpret_cast<char*>(abi::reloc(0x54F7F8));
    const int     mask  = *reinterpret_cast<int*>     (map + 12);     // tileXMask_
    const abi::u8 log2H = *reinterpret_cast<abi::u8*>  (map + 52);     // log2TileHeight_
    abi::u32* const arr = *reinterpret_cast<abi::u32**>(map + 1124);   // pTileArray_
    const int mx  = mask & where.engineX();
    const int idx = (mx & 31) + ((((mx >> 5) << log2H) + where.engineY()) << 5);
    return arr[idx];
  }
};

/// A rectangular map region in visible tiles (OP2Lua's `Region`): query the live units inside, test
/// containment, get the center. Build from two corners - `Region{ {x1,y1}, {x2,y2} }`.
struct Region {
  Location topLeft, bottomRight;
  [[nodiscard]] bool contains(Location p) const {
    return (p.x >= topLeft.x) && (p.x <= bottomRight.x) && (p.y >= topLeft.y) && (p.y <= bottomRight.y);
  }
  [[nodiscard]] Location center() const {
    return { (topLeft.x + bottomRight.x) / 2, (topLeft.y + bottomRight.y) / 2 };
  }
  /// Live units inside this region (a std::ranges view, via Game::unitsInRect).
  [[nodiscard]] auto units() const { return Game::unitsInRect(topLeft, bottomRight); }
};

} // namespace op2
