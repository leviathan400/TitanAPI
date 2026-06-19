#pragma once
// op2/unit.hpp - Unit: a value handle over an engine unit, with the order API (Module 1).
//
// Orders are issued the way the engine itself does it: build a CommandPacket and hand it to the OWNING
// player's PlayerImpl::ProcessCommandPacket (via Player::issue). The raw per-unit Cmd* thunks (CmdMove,
// CmdDock, ...) are never touched - that direct path is what crashes OP2's pathfinder and is the bug class
// TethysAPI's DoMove/DoDock/DoMiningRoute fell into. See re-reference/command-packets.md + op2missionsdk-api.md.
//
// The handle carries its owner (known at creation) so it can pick the right PlayerImpl to dispatch through.
// (Units obtained by enumeration later will read ownerNum_ from the MapObject - Module 2/3.)

#include "op2/core/error.hpp"
#include "op2/core/location.hpp"
#include "op2/player.hpp"
#include "op2/abi/cmd_builder.hpp"
#include "op2/abi/map_object.hpp"

namespace op2 {

/// A lightweight handle to a unit in the running game: the engine unit id ("stub index") + its owning player.
/// Copyable; default-constructed Unit is null. Orders return Result<void> (error-as-value; the engine applies
/// the order asynchronously, so success here means "accepted", not "completed").
class Unit {
public:
  constexpr Unit() = default;
  constexpr explicit Unit(int id, int owner = -1) : id_{ id }, owner_{ owner } {}

  [[nodiscard]] constexpr int  id()    const noexcept { return id_; }
  [[nodiscard]] constexpr bool valid() const noexcept { return id_ > 0; }

  friend constexpr bool operator==(Unit, Unit) = default;

  // ------------------------------------ state reads (Module 2) ------------------------------------
  // These read the live MapObject by id, so they work for ANY unit (created or enumerated), not just
  // ones we own. Safe defaults if the unit/map isn't there.

  [[nodiscard]] bool     isLive() const { return abi::moIsLive(abi::mapObject(id_)); }     ///< alive (not dead)?
  [[nodiscard]] bool     isVehicle()  const { return hasFlag(abi::kMoFlagVehicle);  }       ///< is a vehicle?
  [[nodiscard]] bool     isBuilding() const { return hasFlag(abi::kMoFlagBuilding); }       ///< is a building?
  [[nodiscard]] bool     isEMPed()    const { return hasFlag(abi::kMoFlagEMPed);    }       ///< currently EMP'd?

  // --- mining beacon (this Unit must be a beacon, e.g. from Game::createMine) ---
  /// Reveal this beacon for a player (the scripted equivalent of a Robo-Surveyor reaching it).
  Result<void> survey(int player) {
    char* p = abi::mapObject(id_);
    if (!p) return fail(err::NullHandle);
    abi::member<0x405530, void>(p, player);                 // MiningBeacon::Survey(playerNum)
    return {};
  }
  /// Has `player` surveyed this beacon yet?
  [[nodiscard]] bool isSurveyed(int player) const {
    char* p = abi::mapObject(id_);
    return p && abi::member<0x4055F0, int>(p, player) != 0;  // MiningBeacon::IsSurveyed(playerNum)
  }
  /// This beacon's ore type (0 = Common, 1 = Rare). Only meaningful on a MiningBeacon. MiningBeacon::GetOreType
  /// @0x405520 - virtual, but a beacon is a concrete type so the direct thunk resolves. Mirrors OP2Lua Unit.ore.
  [[nodiscard]] int oreType() const {
    char* p = abi::mapObject(id_);
    return p ? abi::member<0x405520, int>(p) : 0;
  }

  [[nodiscard]] int      ownerId() const { return ownerIndex(); }                          ///< owning player index
  [[nodiscard]] abi::MapID type() const { return abi::MapID(abi::moTypeId(abi::mapObject(id_))); } ///< unit type
  [[nodiscard]] int      cargo() const { return abi::moCargo(abi::mapObject(id_)); }        ///< raw cargo/weapon word
  [[nodiscard]] int      damage() const { return abi::moDamage(abi::mapObject(id_)); }      ///< accumulated damage
  /// Weapon fitted to a combat vehicle / Guard Post (engine stores it in the same word as cargo). Meaningless
  /// for non-combat types - use on Lynx/Panther/Tiger/GuardPost.
  [[nodiscard]] abi::MapID weapon() const { return abi::MapID(abi::moCargo(abi::mapObject(id_))); }
  /// [Cargo Truck] cargo type carried (low 4 bits of the cargo word - a CargoType value).
  [[nodiscard]] int truckCargo()       const { return abi::moCargo(abi::mapObject(id_)) & 0x0F; }
  /// [Cargo Truck] amount of cargo carried (next 12 bits).
  [[nodiscard]] int truckCargoAmount() const { return (abi::moCargo(abi::mapObject(id_)) >> 4) & 0x0FFF; }
  [[nodiscard]] bool     isBusy()  const { return abi::moIsBusy(abi::mapObject(id_)); }      ///< busy executing an action?
  [[nodiscard]] bool     isLightsOn() const { return hasFlag(abi::kMoFlagLights); }          ///< vehicle headlights on?
  [[nodiscard]] int      action() const { return abi::moAction(abi::mapObject(id_)); }       ///< raw ActionType (what it's doing)
  [[nodiscard]] int      command() const { return abi::moCommand(abi::mapObject(id_)); }      ///< raw CommandType (its current order)
  /// [building] Still being built (or dismantled) - its command is Develop/UnDevelop. Wait for this to clear
  /// before transferring a kit / commanding it. (Mirrors OP2Lua Unit.under_construction.)
  [[nodiscard]] bool     underConstruction() const {
    const int c = command();
    return c == int(abi::CommandType::Develop) || c == int(abi::CommandType::UnDevelop);
  }
  [[nodiscard]] bool     isFactory()   const { return isBuilding() && hasFlag(abi::kMoFlagFactory);   } ///< a factory building?
  [[nodiscard]] bool     isOffensive() const { return isLive()     && hasFlag(abi::kMoFlagOffensive); } ///< a combat unit (tank/GP)?
  [[nodiscard]] bool     isInfected()  const { return isBuilding() && hasFlag(abi::kMoFlagInfected);  } ///< Blight-infected building?
  [[nodiscard]] bool     isIdled()     const { return isBuilding() && !hasFlag(abi::kMoFlagLights);   } ///< building idled (inactive)?
  /// [structure] The tile a vehicle docks on (visible coords), or {0,0} for non-buildings. Move a ConVec here
  /// to collect a kit. (Building::GetDockLocation @0x482F40.)
  [[nodiscard]] Location dockLocation() const {
    int ex = 0, ey = 0;
    return (isBuilding() && abi::dockTile(id_, ex, ey)) ? Location{ ex - kPadX, ey - kPadY } : Location{};
  }
  /// [structure] The vehicle currently sitting on this structure's dock, or an invalid Unit if none. Use to know
  /// a ConVec has finished docking BEFORE transferCargo (transferring early crashes OP2). Mirrors OP2Lua
  /// Unit.unit_on_dock: read the dock tile's occupant from the tile array; only return it if it's a vehicle.
  [[nodiscard]] Unit unitOnDock() const {
    int ex = 0, ey = 0;
    if (!isBuilding() || !abi::dockTile(id_, ex, ey)) return Unit{};
    const int idx = abi::tileUnitIndex(ex, ey);
    Unit u{ idx };
    return (idx > 0 && u.isVehicle()) ? u : Unit{};
  }
  /// Fit/replace a combat vehicle's weapon (writes the weapon word in place). Use on Lynx/Panther/Tiger/GuardPost.
  void setWeapon(abi::MapID w) { if (char* p = abi::mapObject(id_)) *reinterpret_cast<abi::u16*>(p + abi::mo::cargo) = abi::u16(int(w)); }
  /// [ConVec] Set the structure kit it carries. (Same union field as weapon/cargo - a unit is only ever one.)
  void setKit(abi::MapID kit) { setWeapon(kit); }
  /// [Cargo Truck] Set its cargo type (low 4 bits) and amount (next 12 bits) in the cargo word.
  void setTruckCargo(int cargoType, int amount) {
    if (char* p = abi::mapObject(id_))
      *reinterpret_cast<abi::u16*>(p + abi::mo::cargo) = abi::u16((cargoType & 0x0F) | ((amount & 0x0FFF) << 4));
  }
  /// [Cargo Truck] Alias of setTruckCargo matching OP2Lua's `set_cargo(cargoType, amount)`.
  void setCargo(int cargoType, int amount) { setTruckCargo(cargoType, amount); }
  // ---- building operational state (flags; gated by isBuilding) ----
  [[nodiscard]] bool hasPower()      const { return isBuilding() && hasFlag(abi::kMoFlagBldPower);      }
  [[nodiscard]] bool hasWorkers()    const { return isBuilding() && hasFlag(abi::kMoFlagBldWorkers);    }
  [[nodiscard]] bool hasScientists() const { return isBuilding() && hasFlag(abi::kMoFlagBldScientists); }
  /// Building lacks power (the "1 Structure Disabled" state). Non-buildings report false.
  [[nodiscard]] bool isDisabled()    const { return isBuilding() && !hasFlag(abi::kMoFlagBldPower);     }
  /// [structure] Is this building enabled (fully built, powered & staffed - actively operating)? Non-buildings
  /// report false. Building::IsEnabled @0x483710 (virtual; a concrete building resolves the thunk directly).
  /// Mirrors OP2Lua Unit.enabled.
  [[nodiscard]] bool enabled() const {
    char* p = abi::mapObject(id_);
    return p && isBuilding() && abi::member<0x483710, int>(p) != 0;
  }
  /// Order this unit to die (with the death animation/explosion). MapObject::DoDeath @0x43A990.
  void kill() { if (char* p = abi::mapObject(id_); p && abi::moIsLive(p)) abi::member<0x43A990, void>(p, 1); }

  // ---- health (maxHP from the per-player unit-stats table; current HP = max - accumulated damage) ----
  [[nodiscard]] int    maxHitpoints() const { return abi::moMaxHp(abi::mapObject(id_)); }
  [[nodiscard]] int    hitpoints()    const { char* p = abi::mapObject(id_); return abi::moMaxHp(p) - abi::moDamage(p); }
  [[nodiscard]] double health()       const { char* p = abi::mapObject(id_); const int m = abi::moMaxHp(p);
                                              return (m > 0) ? double(m - abi::moDamage(p)) / m : 0.0; }   ///< 0..1
  /// Inflict `n` damage; the unit dies (DoDeath) if total damage reaches its max HP.
  void inflictDamage(int n) {
    char* p = abi::mapObject(id_);
    if (!p || !abi::moIsLive(p)) return;
    const int d = abi::moDamage(p) + n;
    *reinterpret_cast<abi::i16*>(p + abi::mo::damage) = abi::i16(d);
    if (d >= abi::moMaxHp(p)) abi::member<0x43A990, void>(p, 1);   // DoDeath
  }
  /// Repair `n` damage (clamped at 0).
  void heal(int n) {
    char* p = abi::mapObject(id_);
    if (!p || !abi::moIsLive(p)) return;
    const int d = abi::moDamage(p) - n;
    *reinterpret_cast<abi::i16*>(p + abi::mo::damage) = abi::i16(d < 0 ? 0 : d);
  }
  /// Current tile in VISIBLE coordinates (engine pixel -> engine tile -> visible).
  [[nodiscard]] Location location() const {
    char* p = abi::mapObject(id_);
    return p ? Location{ abi::moPixelX(p) / 32 - kPadX, abi::moPixelY(p) / 32 - kPadY } : Location{};
  }

  // ----------------------------------------- orders (Module 1) -----------------------------------------
  // Each builds the matching CommandPacket and dispatches through the owning player. Visible coordinates.

  /// Move to a tile (vehicles). Uses the command-packet path (CommandType::Move), never the raw CmdMove thunk.
  Result<void> move(Location to) {
    abi::CmdBuilder b{ abi::CommandType::Move };
    b.oneUnit(u16(id_)).oneWaypoint(abi::raw::Waypoint{ to.waypointBits() });
    return dispatch(b);
  }

  /// Patrol a route, cycling through its waypoints (vehicles). Up to 8 waypoints (the engine's path limit).
  Result<void> patrol(std::span<const Location> route) {
    if (route.empty()) return fail(err::InvalidTarget);
    abi::raw::Waypoint wps[8];
    const std::size_t n = (route.size() < 8) ? route.size() : 8;
    for (std::size_t i = 0; i < n; ++i) wps[i] = abi::raw::Waypoint{ route[i].waypointBits() };
    abi::CmdBuilder b{ abi::CommandType::Patrol };
    b.oneUnit(u16(id_)).waypoints({ wps, n });
    return dispatch(b);
  }
  /// Patrol back and forth between two points.
  Result<void> patrol(Location a, Location b) { const Location r[]{ a, b }; return patrol(r); }

  /// Attack a target unit.
  Result<void> attack(Unit target)    { return targetUnit(abi::CommandType::Attack, target); }
  /// Attack a ground location.
  Result<void> attack(Location ground) {
    abi::raw::CommandTarget t{};
    t.tileX = u16(ground.engineX());
    t.tileY = u16(ground.engineY());
    return targeted(abi::CommandType::Attack, t);
  }
  /// OP2Lua's `attack_move`: advance aggressively toward a target, firing en route. Mapped to attack - a plain
  /// move (CmdMove) on a freshly-spawned unit can crash OP2's pathfinder, but attack-to-ground/unit does not, so
  /// this is the safe "go there fighting" order. (Identical to attack(); named for parity & intent.)
  Result<void> attackMove(Unit target)     { return attack(target); }
  Result<void> attackMove(Location ground) { return attack(ground); }

  Result<void> guard(Unit target)     { return targetUnit(abi::CommandType::Guard,     target); } ///< Guard a unit.
  Result<void> repair(Unit target)    { return targetUnit(abi::CommandType::RepairObj, target); } ///< Repair target.
  Result<void> reprogram(Unit target) { return targetUnit(abi::CommandType::Reprogram, target); } ///< [Spider] Reprogram.
  Result<void> dismantle(Unit target) { return targetUnit(abi::CommandType::Dismantle, target); } ///< [ConVec] Dismantle.

  Result<void> stop()         { return simple(abi::CommandType::Stop);         } ///< Stop the current command.
  Result<void> selfDestruct() { return simple(abi::CommandType::SelfDestruct); } ///< Order the unit to self-destruct.
  Result<void> idle()         { return single(abi::CommandType::Idle);         } ///< Idle a building.
  Result<void> unidle()       { return single(abi::CommandType::Unidle);       } ///< Resume an idled building.
  /// Remove this unit instantly with NO death animation/explosion (vs kill(), which explodes). DoPoof =
  /// SingleUnitSimpleCommand(Poof). Mirrors OP2Lua Unit.remove().
  Result<void> remove()       { return single(abi::CommandType::Poof);         }

  // ---- cargo truck commands (single-unit, same shape as idle/unidle) ----
  Result<void> loadCargo()   { return single(abi::CommandType::LoadCargo);   } ///< [Cargo Truck] load cargo at its tile.
  Result<void> unloadCargo() { return single(abi::CommandType::UnloadCargo); } ///< [Cargo Truck] unload its cargo.
  Result<void> dumpCargo()   { return single(abi::CommandType::DumpCargo);   } ///< [Cargo Truck] dump its cargo.

  /// Hold position and fire on anything that comes in range (vehicles). Same waypoint payload as move.
  Result<void> standGround(Location at) {
    abi::CmdBuilder b{ abi::CommandType::StandGround };
    b.oneUnit(u16(id_)).oneWaypoint(abi::raw::Waypoint{ at.waypointBits() });
    return dispatch(b);
  }

  /// [Lab] Begin researching tech `techID` with `numScientists`. Converts techID->techNum (Research::GetTechNum
  /// @0x472D90 on the Research singleton @0x56C230), then a Research command packet (unitID, techNum, count).
  Result<void> research(int techID, int numScientists = 1) {
    const int techNum = abi::member<0x472D90, int>(reinterpret_cast<char*>(abi::reloc(0x56C230)), techID);
    if (techNum < 0) return fail(err::InvalidTarget);
    abi::CmdBuilder b{ abi::CommandType::Research };
    b.singleUnit(u16(id_)).field<u16>(u16(techNum)).field<u16>(u16(numScientists));
    return dispatch(b);
  }
  /// [University] Begin training `numToTrain` new scientists.
  Result<void> trainScientists(int numToTrain) {
    abi::CmdBuilder b{ abi::CommandType::TrainScientists };
    b.singleUnit(u16(id_)).field<u16>(u16(numToTrain));
    return dispatch(b);
  }
  /// [Spaceport] Launch the rocket on the pad toward `target` (Launch command - target is a map pixel).
  Result<void> launch(Location target) {
    abi::CmdBuilder b{ abi::CommandType::Launch };
    b.singleUnit(u16(id_)).field<u16>(u16(target.enginePixelX())).field<u16>(u16(target.enginePixelY()));
    return dispatch(b);
  }
  /// [Structure Factory] Transfer cargo bay `bay` (0..5) to the vehicle on the dock. ONLY call when a vehicle
  /// is actually docked (transferring early crashes OP2).
  Result<void> transferCargo(int bay) {
    abi::CmdBuilder b{ abi::CommandType::TransferCargo };
    b.singleUnit(u16(id_)).field<u16>(u16(bay)).field<u16>(0);
    return dispatch(b);
  }
  /// [Factory] The kit type queued in a 0-based cargo bay (MapID::None if empty). FactoryBuilding offsets
  /// probe-pinned: cargoBayContents_ @+97, cargoBayCargoOrWeapon_ @+72 (uint8[6]).
  [[nodiscard]] abi::MapID factoryCargo(int bay) const {
    if (!isFactory() || bay < 0 || bay >= 6) return abi::MapID::None;
    char* p = abi::mapObject(id_);
    return p ? abi::MapID(*reinterpret_cast<abi::u8*>(p + 97 + bay)) : abi::MapID::None;
  }
  /// [Factory] Preload a 0-based cargo bay (0..5) with a kit + optional cargo/weapon topping.
  void setFactoryCargo(int bay, abi::MapID kit, abi::MapID cargoOrWeapon = abi::MapID::None) {
    if (!isFactory() || bay < 0 || bay >= 6) return;
    if (char* p = abi::mapObject(id_)) {
      *reinterpret_cast<abi::u8*>(p + 97 + bay) = abi::u8(int(kit));
      *reinterpret_cast<abi::u8*>(p + 72 + bay) = abi::u8(int(cargoOrWeapon));
    }
  }

  /// [Factory / structure] Produce a unit, optionally with a weapon or cargo. scGroup defaults to none (-1).
  Result<void> produce(abi::MapID item, abi::MapID weaponOrCargo = abi::MapID::None) {
    abi::CmdBuilder b{ abi::CommandType::Produce };
    b.singleUnit(u16(id_))
     .field<u16>(u16(int(item)))
     .field<u16>(u16(int(weaponOrCargo)))
     .field<u16>(u16(0xFFFF));                          // scGroupIndex = -1 (no group)
    return dispatch(b);
  }

  /// [ConVec] Build the structure kit the ConVec carries, its bottom-right corner at `bottomRight`. The
  /// footprint is read from the building type's stats - no need to pass the size.
  Result<void> build(Location bottomRight) {
    char* p = abi::mapObject(id_);
    if (!p) return fail(err::NullHandle);
    const int kit = abi::moCargo(p);                    // building MapID the ConVec carries
    if (kit <= 0) return fail(err::WrongType);
    int w = 0, h = 0;
    abi::buildingSize(kit, w, h);
    if (w == 0 || h == 0) return fail(err::Unsupported);
    return buildCmd(bottomRight, /*xCentered*/ true, /*yCentered*/ false,
                    bottomRight.engineX() - (w + 1), bottomRight.engineY() - (h + 1),
                    bottomRight.engineX(),           bottomRight.engineY());
  }

  /// [Earthworker] Build walls/tubes of `type` over a tile area (from..to inclusive). The engine wants the
  /// bottom-right corner exclusive, so we add 1 - matching the proven DoBuildWall behaviour.
  Result<void> buildWall(abi::MapID type, Location from, Location to) {
    abi::CmdBuilder b{ abi::CommandType::BuildWall };
    b.oneUnit(u16(id_))
     .field(abi::raw::PackedMapRect{ u16(from.engineX()),     u16(from.engineY()),
                                     u16(to.engineX() + 1),   u16(to.engineY() + 1) })
     .field<u16>(u16(int(type)))                       // tubeWallType
     .field<u16>(0);                                   // unknown
    return dispatch(b);
  }
  /// [Earthworker] Build a single tube tile.
  Result<void> buildTube(Location at) { return buildWall(abi::MapID::Tube, at, at); }

  /// [Earthworker] Remove walls/tubes over a tile area (from..to inclusive). RemoveWallCommand is a MoveCommand
  /// (unit list + waypoints) followed by the rect; we send one waypoint = the area's top-left, like build/deploy.
  Result<void> removeWall(Location from, Location to) {
    abi::CmdBuilder b{ abi::CommandType::RemoveWall };
    b.oneUnit(u16(id_))
     .oneWaypoint(abi::raw::Waypoint{ from.waypointBits() })
     .field(abi::raw::PackedMapRect{ u16(from.engineX()), u16(from.engineY()),
                                     u16(to.engineX()),   u16(to.engineY()) });   // inclusive (no +1)
    return dispatch(b);
  }
  /// [Robo-Dozer] Bulldoze a tile area to bare rock (from..to inclusive). DozeCommand = unit list + rect.
  Result<void> doze(Location from, Location to) {
    abi::CmdBuilder b{ abi::CommandType::Doze };
    b.oneUnit(u16(id_))
     .field(abi::raw::PackedMapRect{ u16(from.engineX()), u16(from.engineY()),
                                     u16(to.engineX()),   u16(to.engineY()) });   // inclusive
    return dispatch(b);
  }
  /// [Cargo Truck] Salvage rubble in a tile area (from..to inclusive), hauling it to `gorf` (a GORF / Garbage
  /// disposal). SalvageCommand = single unit + rect + the GORF's unit id.
  Result<void> salvage(Location from, Location to, Unit gorf) {
    if (!gorf.valid()) return fail(err::InvalidTarget);
    abi::CmdBuilder b{ abi::CommandType::Salvage };
    b.singleUnit(u16(id_))
     .field(abi::raw::PackedMapRect{ u16(from.engineX()), u16(from.engineY()),
                                     u16(to.engineX()),   u16(to.engineY()) })    // inclusive
     .field<u16>(u16(gorf.id_));
    return dispatch(b);
  }

  /// [Robo-Miner / GeoCon] Deploy into a building at `center`.
  Result<void> deploy(Location center) {
    return buildCmd(center, /*xCentered*/ false, /*yCentered*/ false,
                    center.engineX() - 1, center.engineY(), center.engineX(), center.engineY());
  }

  /// [Cargo Truck] Set up a mine <-> smelter cargo route (haul ore back and forth). Built CORRECTLY
  /// (numUnits=1, unitID[0]=this truck) - TethysAPI's DoMiningRoute *still* sets numUnits = the unit id and
  /// writes unitID[1], the historical bug this facade designs out.
  Result<void> mine(Unit mineUnit, Unit smelter) {
    if (!mineUnit.valid() || !smelter.valid()) return fail(err::InvalidTarget);
    const abi::raw::Waypoint mw{ abi::dockWaypointBits(mineUnit.id_) };
    const abi::raw::Waypoint sw{ abi::dockWaypointBits(smelter.id_) };
    const abi::raw::Waypoint wps[3] = { mw, sw, mw };             // mine -> smelter -> mine loop
    abi::CmdBuilder b{ abi::CommandType::CargoRoute };
    b.oneUnit(u16(id_))                                          // numUnits=1, unitID[0]=truck (the fix)
     .waypoints({ wps, 3 })
     .field<u16>(0)                                              // mineWaypointIndex
     .field<u16>(1)                                              // smelterWaypointIndex
     .field<u16>(u16(mineUnit.id_))                              // mineUnitId
     .field<u16>(u16(smelter.id_));                              // smelterUnitId
    return dispatch(b);
  }

  /// Dock this vehicle at a structure (e.g. a smelter or spaceport) - its docking tile.
  Result<void> dock(Unit at) {
    if (!at.valid()) return fail(err::InvalidTarget);
    abi::CmdBuilder b{ abi::CommandType::Dock };
    b.oneUnit(u16(id_)).oneWaypoint(abi::raw::Waypoint{ abi::dockWaypointBits(at.id_) });
    return dispatch(b);
  }

  /// Dock this vehicle inside a Garage (DockEG).
  Result<void> dockAtGarage(Unit garage) {
    if (!garage.valid()) return fail(err::InvalidTarget);
    abi::CmdBuilder b{ abi::CommandType::DockEG };
    b.oneUnit(u16(id_)).oneWaypoint(abi::raw::Waypoint{ abi::dockWaypointBits(garage.id_) });
    return dispatch(b);
  }

  /// Turn a vehicle's headlights on/off.
  Result<void> setLights(bool on) {
    abi::CmdBuilder b{ abi::CommandType::LightToggle };
    b.oneUnit(u16(id_)).field<u16>(on ? 1 : 0);
    return dispatch(b);
  }

  /// Transfer ownership of this unit to another player.
  Result<void> transfer(int toPlayer) {
    abi::CmdBuilder b{ abi::CommandType::Transfer };
    b.oneUnit(u16(id_)).field<u16>(u16(toPlayer));
    return dispatch(b);
  }

private:
  using u16 = abi::u16;

  /// True if this unit's MapObject has the given flag bit set.
  [[nodiscard]] bool hasFlag(abi::u32 flag) const {
    char* p = abi::mapObject(id_);
    return p && (abi::moFlags(p) & flag) != 0;
  }

  /// Attack / Guard / Repair / Reprogram / Dismantle all share the layout: unit list + u16(0) + CommandTarget
  /// (AttackCommand and RepairCommand are byte-identical). Only the CommandType differs.
  Result<void> targeted(abi::CommandType type, const abi::raw::CommandTarget& t) {
    abi::CmdBuilder b{ type };
    b.oneUnit(u16(id_)).field<u16>(0).field(t);
    return dispatch(b);
  }
  /// Same, targeting a unit (tileY = -1 marks "target is a unit, not a tile").
  Result<void> targetUnit(abi::CommandType type, Unit target) {
    if (!target.valid()) return fail(err::InvalidTarget);
    abi::raw::CommandTarget t{};
    t.unitID = u16(target.id_);
    t.tileY  = 0xFFFF;
    return targeted(type, t);
  }
  /// SimpleCommand family (unit list only): Stop / SelfDestruct / Scatter ...
  Result<void> simple(abi::CommandType type) {
    abi::CmdBuilder b{ type };
    b.oneUnit(u16(id_));
    return dispatch(b);
  }
  /// SingleUnitSimpleCommand family (bare unitID, no count): Idle / Unidle / LoadCargo ...
  Result<void> single(abi::CommandType type) {
    abi::CmdBuilder b{ type };
    b.singleUnit(u16(id_));
    return dispatch(b);
  }

  /// BuildCommand: unit list + waypoint + footprint rect + unknown(-1). Used by build() and deploy().
  Result<void> buildCmd(Location waypoint, bool xC, bool yC, int rx1, int ry1, int rx2, int ry2) {
    abi::CmdBuilder b{ abi::CommandType::Build };
    b.oneUnit(u16(id_))
     .oneWaypoint(abi::raw::Waypoint{ waypoint.waypointBits(xC, yC) })
     .field(abi::raw::PackedMapRect{ u16(rx1), u16(ry1), u16(rx2), u16(ry2) })
     .field<u16>(0xFFFF);                               // unknown = -1
    return dispatch(b);
  }

  /// Owner player index: the one we were created with, else read live from the MapObject (so orders work on
  /// enumerated/foreign units too).
  int ownerIndex() const { return owner_ >= 0 ? owner_ : abi::moOwner(abi::mapObject(id_)); }

  /// Validate and route a built packet through the owning player's ProcessCommandPacket.
  Result<void> dispatch(abi::CmdBuilder& b) const {
    if (!valid())   return fail(err::NullHandle);
    if (!b.ok())    return fail(err::EngineRejected);    // payload overflow (shouldn't happen for one unit)
    const int owner = ownerIndex();
    if (owner < 0)  return fail(err::NotOwned);
    return Player{ owner }.issue(b.build());
  }

  int id_    = -1;   ///< engine unit id / stub index
  int owner_ = -1;   ///< owning player index (-1 = resolve from MapObject on demand)
};

} // namespace op2
