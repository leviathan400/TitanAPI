#pragma once
// op2_mission.hpp — the Outpost 2 mission-DLL contract (the Layer-1 "be a mission" ABI).
//
// These types/layouts are facts about how Outpost 2 / the OPU loader interface with a mission DLL
// (reference: TethysAPI API/Mission.h). A mission DLL must export, with exactly these names & layouts:
//     char      LevelDesc[]      // shown in the mission list
//     char      MapName[]        // .map file to load
//     char      TechtreeName[]   // tech tree file
//     ModDesc   DescBlock        // mission type / players / tech
//     ModDescEx DescBlockEx      // multiplayer-AI slots (0 for single-player)
// and may optionally export the entry points:
//     int  InitProc()                      // set up the world; return nonzero on success
//     void AIProc()                        // called every 4 ticks
//     void GetSaveRegions(SaveRegion*)     // declare save-game state
// All entry points are __cdecl (the project default) with C linkage.

#include <cstddef>

namespace op2::mission {

/// DescBlock.missionType. Positive = campaign mission; negative = colony / demo / tutorial / multiplayer.
enum class MissionType : int {
  Campaign1 = 1, Campaign2, Campaign3, Campaign4,  Campaign5,  Campaign6,
  Campaign7,     Campaign8, Campaign9, Campaign10, Campaign11, Campaign12,

  Colony          = -1,
  AutoDemo        = -2,
  Tutorial        = -3,
  LandRush        = -4,
  SpaceRace       = -5,
  ResourceRace    = -6,
  Midas           = -7,
  LastOneStanding = -8,
};

/// Exported as `DescBlock` (16 bytes). Tells Outpost 2 the mission's type / player count / tech.
struct ModDesc {
  MissionType missionType;
  int         numPlayers;    ///< 1-6; includes AIs on single-player maps, excludes them on multiplayer maps.
  int         maxTechLevel;  ///< 12 enables all techs for standard tech trees.
  int         unitMission;   ///< ibool; set to 1 to disable most reports (unit-only missions).
};

/// Exported as `DescBlockEx` (32 bytes). Multiplayer-AI slot count plus reserved fields. 0 for single-player.
struct ModDescEx {
  int numMultiplayerAIs;
  int field_04, field_08, field_0C, field_10, field_14, field_18, field_1C;
};

/// Passed to GetSaveRegions(): point pData/size at mission state to persist across save/load (none here).
struct SaveRegion {
  void*       pData;
  std::size_t size;
};

} // namespace op2::mission
