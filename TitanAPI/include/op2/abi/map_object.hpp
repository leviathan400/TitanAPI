#pragma once
// map_object.hpp - read a unit's live MapObject by id (Layer 1 / op2::abi). The keystone for Module 2
// (unit state) and for orders/build that need to inspect a unit.
//
// Units live in a fixed array: the MapImpl OBJECT lives at 0x54F7F8 (it's a fixed singleton, not a pointer
// stored there - like Research @0x56C230). MapImpl holds pMapObjArray_ (@+80), an array of 120-byte
// (MapObjectSize) records indexed by unit id, and pMapObjListEnd_ (@+88) marks the end.
//   MapObject::GetInstance(id) == &pMapObjArray_[id]
// ALL offsets/sizes here are offsetof-verified against TethysAPI's real headers via a compiled probe
// (sizeof(MapObject)==0x48, sizeof(MapImpl)==1136, MapObjectSize==120). MapObject is polymorphic (vtable @0),
// so GetType() is a virtual at vtable index 0.

#include "op2/abi/memory.hpp"

namespace op2::abi {

inline constexpr int kMapObjectSize = 120;   ///< AnyMapObj array stride (== MapObjectSize)

/// MapObject field byte-offsets (offsetof-verified).
namespace mo {
  inline constexpr int pNext  = 4;    ///< MapObject* - a dead object's pNext_ is ~0
  inline constexpr int index  = 16;   ///< int
  inline constexpr int pixelX = 20;   ///< int (map pixel; tile = pixel/32)
  inline constexpr int pixelY = 24;   ///< int
  inline constexpr int owner  = 29;   ///< creatorAndOwnerNum_; owner = low nibble (& 0x0F)
  inline constexpr int damage = 30;   ///< int16 (hitpoints = maxHP - damage)
  inline constexpr int isBusy = 32;   ///< uint8  - derived layout: damage_(int16)@30 -> isBusy_@32
  inline constexpr int command = 33;  ///< uint8 CommandType - the unit's current command (command_@33, between isBusy & action)
  inline constexpr int action = 34;   ///< uint8 ActionType - what the unit is currently doing (action_@34)
  inline constexpr int cargo  = 36;   ///< uint16 (ConVec kit / weapon / truck cargo, by type)
  inline constexpr int actionTimer = 60; ///< int - ticks left to finish the current action (probe-verified @60)
  inline constexpr int flags  = 68;   ///< uint32 (MapObjectFlags)
}

// MapEntity (disaster) flag bits - distinct meanings from the unit flags above (see MoFlagEntity). These two
// "warning completed" bits + actionTimer_=6 are what MapObject::StartDisaster sets to skip the warning delay and
// strike immediately (the engine has no exported StartDisaster - it's inline, so we replicate it).
inline constexpr u32 kMoFlagEntDisasterDidFirstWarn  = 1u << 14;
inline constexpr u32 kMoFlagEntDisasterDidSecondWarn = 1u << 15;
/// Make a freshly-created disaster strike NOW instead of after its warning period. `p` is the Disaster MapObject
/// returned by a Game::create* disaster call. Mirrors the inline MapObject::StartDisaster().
inline void startDisasterNow(void* p) {
  if (!p) return;
  char* d = static_cast<char*>(p);
  *reinterpret_cast<u32*>(d + mo::flags) |= (kMoFlagEntDisasterDidFirstWarn | kMoFlagEntDisasterDidSecondWarn);
  *reinterpret_cast<int*>(d + mo::actionTimer) = 6;
}

// MapObject flag bits.
inline constexpr u32 kMoFlagLights   = 1u << 0;   ///< vehicle headlights on (for a building: 'active'/unidled)
inline constexpr u32 kMoFlagVehicle  = 1u << 1;
inline constexpr u32 kMoFlagBuilding = 1u << 2;
inline constexpr u32 kMoFlagFactory  = 1u << 3;   ///< [building] is a factory
inline constexpr u32 kMoFlagOffensive= 1u << 4;   ///< combat unit type (tank / Guard Post)
inline constexpr u32 kMoFlagBldPower      = 1u << 13;  ///< [building] has sufficient power
inline constexpr u32 kMoFlagBldWorkers    = 1u << 14;  ///< [building] has sufficient workers
inline constexpr u32 kMoFlagBldScientists = 1u << 15;  ///< [building] has sufficient scientists
inline constexpr u32 kMoFlagDead     = 1u << 17;
inline constexpr u32 kMoFlagInfected = 1u << 18;  ///< [building] Blight-infected
inline constexpr u32 kMoFlagEMPed    = 1u << 19;

// ---- the unit array ----------------------------------------------------------------------------------------

/// The MapImpl singleton object base (it lives AT 0x54F7F8).
inline char* mapImpl() { return reinterpret_cast<char*>(reloc(0x54F7F8)); }

/// Base of the MapObject array (AnyMapObj[]), or nullptr if the map isn't loaded yet.
inline char* mapObjArrayBase() {
  return *reinterpret_cast<char**>(mapImpl() + 80);                  // pMapObjArray_ @ +80
}

/// Number of unit slots: (pMapObjListEnd_ - pMapObjArray_) / 120.
inline int maxNumUnits() {
  char* begin = *reinterpret_cast<char**>(mapImpl() + 80);
  char* end   = *reinterpret_cast<char**>(mapImpl() + 88);           // pMapObjListEnd_ @ +88
  return (begin && end && end > begin) ? int((end - begin) / kMapObjectSize) : 0;
}

/// MapObject* (as char*) for unit id, or nullptr if out of range / map not loaded.
inline char* mapObject(int id) {
  char* base = mapObjArrayBase();
  return (base && id >= 0 && id < maxNumUnits()) ? base + std::size_t(id) * kMapObjectSize : nullptr;
}

// ---- typed field reads (safe on nullptr) -------------------------------------------------------------------

inline int  moOwner (char* p) { return p ? (*reinterpret_cast<u8*> (p + mo::owner)  & 0x0F) : -1; }
inline int  moPixelX(char* p) { return p ?  *reinterpret_cast<i32*>(p + mo::pixelX)        : 0;  }
inline int  moPixelY(char* p) { return p ?  *reinterpret_cast<i32*>(p + mo::pixelY)        : 0;  }
inline int  moDamage(char* p) { return p ?  *reinterpret_cast<i16*>(p + mo::damage)        : 0;  }
inline u16  moCargo (char* p) { return p ?  *reinterpret_cast<u16*>(p + mo::cargo)         : u16(0); }
inline u32  moFlags (char* p) { return p ?  *reinterpret_cast<u32*>(p + mo::flags)         : 0u; }
inline bool moIsBusy(char* p) { return p && (*reinterpret_cast<u8*> (p + mo::isBusy) != 0); }
inline int  moCommand(char* p){ return p ?  *reinterpret_cast<u8*> (p + mo::command)       : 0; }
inline int  moAction(char* p) { return p ?  *reinterpret_cast<u8*> (p + mo::action)        : 0; }
/// Creator player index (HIGH nibble of creatorAndOwnerNum_@29; the source of this unit's per-player stats).
inline int  moCreator(char* p) { return p ? (*reinterpret_cast<u8*>(p + mo::owner) >> 4) & 0x0F : -1; }

inline bool moIsLive(char* p) {
  if (!p) return false;
  const u32 next = *reinterpret_cast<u32*>(p + mo::pNext);
  return (next != 0xFFFFFFFFu) && ((moFlags(p) & kMoFlagDead) == 0);
}

/// Unit type id (MapID as int) via the virtual MapObject::GetType() (vtable index 0) -> MapObjectType::type_.
inline int moTypeId(char* p) {
  if (!p) return -1;
  void** vtbl = *reinterpret_cast<void***>(p);                       // vptr @ 0
  using GetTypeFn = char*(__thiscall*)(const void*);
  char* type = reinterpret_cast<GetTypeFn>(vtbl[0])(p);              // GetType()
  return type ? *reinterpret_cast<i32*>(type + 4) : -1;             // MapObjectType::type_ @ +4
}

// ---- the MapObjectType table (unit-type stats; used by build for the footprint) ----------------------------

// The MapObjectType* table runs from 0x4E1348 to its end at 0x4E1514, so the count is (end - begin)/4 = 115.
inline constexpr int kNumMapObjTypes = 115;   // == (0x4E1514 - 0x4E1348) / 4

/// MapObjectType* (as char*) for a MapID, or nullptr. Table base is the data array at 0x4E1348
/// (the table end is at 0x4E1514, giving kNumMapObjTypes = 115 entries).
inline char* mapObjType(int mapId) {
  if (mapId < 0 || mapId >= kNumMapObjTypes) return nullptr;
  char** table = reinterpret_cast<char**>(reloc(0x4E1348));          // MapObjectType* table[]
  return table ? table[mapId] : nullptr;
}

/// Building footprint (in tiles) for a building MapID. Zeroes if not a building / unknown.
inline void buildingSize(int mapId, int& width, int& height) {
  char* t = mapObjType(mapId);
  width  = t ? *reinterpret_cast<u16*>(t + 584) : 0;                 // stats_.building.width  @ +584
  height = t ? *reinterpret_cast<u16*>(t + 586) : 0;                 // stats_.building.height @ +586
}

/// Max hitpoints of the live unit at `p`: MapObjectType::playerStats_[creator].hp - the per-creator (upgraded)
/// stats. Offsets probe-verified: playerStats_ @+8, sizeof(PerPlayerUnitStats)==68, hp @+0 (stats_@584 matched).
inline int moMaxHp(char* p) {
  if (!p) return 0;
  char* mot = mapObjType(moTypeId(p));
  return mot ? *reinterpret_cast<i32*>(mot + 8 + moCreator(p) * 68) : 0;
}

// ---- docking (for dock / mining-route orders) --------------------------------------------------------------

// ---- players (for beacon retrieval) ------------------------------------------------------------------------

inline constexpr int kPlayerImplStride  = 3108;  ///< sizeof(PlayerImpl), offsetof-verified
inline constexpr int kPlayerBuildingList = 3096; ///< PlayerImpl::pBuildingList_
inline constexpr int kPlayerVehicleList = 3100;  ///< PlayerImpl::pVehicleList_ (also holds beacons/vents/wreckage)

/// PlayerImpl base (as char*) for player `idx` (0..6), or nullptr. Array base is a code immediate at 0x4890C3.
inline char* playerImpl(int idx) {
  char* base = ref<char*>(0x4890C3);
  return (base && idx >= 0 && idx < 7) ? base + std::size_t(idx) * kPlayerImplStride : nullptr;
}

/// Unit id of the head (newest) beacon in Gaia's (player 6) list - the just-created beacon after CreateMine.
inline int newestBeaconId() {
  char* gaia = playerImpl(6);
  char* beacon = gaia ? *reinterpret_cast<char**>(gaia + kPlayerVehicleList) : nullptr;
  return beacon ? *reinterpret_cast<i32*>(beacon + mo::index) : -1;   // MapObject::index_
}

/// Walk a player's building list (linked by pPlayerNext_ @+12) and return the id of the first building of
/// `mapType` whose tile is within `tol` of the given ENGINE tile, or -1. A small foretaste of Module 3
/// (enumeration); used here to locate a dynamically-built mine so trucks can be routed to it.
inline int findBuilding(int playerIdx, int mapType, int engTileX, int engTileY, int tol = 2) {
  char* impl = playerImpl(playerIdx);
  if (!impl) return -1;
  char* p = *reinterpret_cast<char**>(impl + kPlayerBuildingList);
  for (int guard = 0; p && reinterpret_cast<std::uintptr_t>(p) != ~std::uintptr_t(0) && guard < 4096; ++guard) {
    if (moTypeId(p) == mapType) {
      const int tx = moPixelX(p) / 32, ty = moPixelY(p) / 32;
      if (tx >= engTileX - tol && tx <= engTileX + tol && ty >= engTileY - tol && ty <= engTileY + tol)
        return *reinterpret_cast<i32*>(p + mo::index);
    }
    p = *reinterpret_cast<char**>(p + 12);   // MapObject::pPlayerNext_
  }
  return -1;
}

/// Count buildings in a player's PlayerImpl building list - the per-player list the engine's count triggers
/// read (distinct from a global MapObject scan). Returns -1 if the player is invalid.
inline int countPlayerBuildings(int playerIdx) {
  char* impl = playerImpl(playerIdx);
  if (!impl) return -1;
  char* p = *reinterpret_cast<char**>(impl + kPlayerBuildingList);
  int n = 0;
  for (int guard = 0; p && reinterpret_cast<std::uintptr_t>(p) != ~std::uintptr_t(0) && guard < 4096; ++guard) {
    ++n;
    p = *reinterpret_cast<char**>(p + 12);   // MapObject::pPlayerNext_
  }
  return n;
}

/// A building's dock tile in ENGINE coords (no visible pad). false if the unit isn't a dockable building.
inline bool dockTile(int unitId, int& engineX, int& engineY) {
  char* p = mapObject(unitId);
  if (!p) { engineX = engineY = -1; return false; }
  int out[2] = { -1, -1 };                                          // engine Location { int x, int y }
  member<0x482F40, int>(p, out);                                    // Building::GetDockLocation(Location*)
  engineX = out[0]; engineY = out[1];
  return engineX >= 0;
}

/// A building's dock tile packed as engine Waypoint bits (centered pixel), or 0 if unavailable. Engine coords
/// already include the offset, so no visible-pad is applied here.
inline u32 dockWaypointBits(int unitId) {
  int ex, ey;
  if (!dockTile(unitId, ex, ey)) return 0;
  const u32 px = u32(ex * 32 + 16), py = u32(ey * 32 + 16);
  return (px & 0x7FFFu) | ((py & 0x3FFFu) << 15);
}

// ---- tile read by ENGINE coords ----------------------------------------------------------------------------
// The packed TileData u32 for an engine tile. Mirrors MapImpl::Tile(): tileXMask_@12, log2TileHeight_@52,
// pTileArray_@1124 (offsets pinned by the Module-6 offsetof probe; GameMap::tileData uses the same formula but
// takes visible coords). unitIndex lives in bits 16-26 (the unit occupying the tile, 0 = none).
inline u32 tileDataEngine(int engineX, int engineY) {
  char* const map = mapImpl();
  const int mask  = *reinterpret_cast<int*>(map + 12);
  const u8  log2H = *reinterpret_cast<u8*> (map + 52);
  u32* const arr  = *reinterpret_cast<u32**>(map + 1124);
  if (!arr) return 0;
  const int mx  = mask & engineX;
  const int idx = (mx & 31) + ((((mx >> 5) << log2H) + engineY) << 5);
  return arr[idx];
}
/// The unit index occupying an engine tile (TileData bits 16-26), or 0 if none.
inline int tileUnitIndex(int engineX, int engineY) { return int((tileDataEngine(engineX, engineY) >> 16) & 0x7FFu); }

} // namespace op2::abi
