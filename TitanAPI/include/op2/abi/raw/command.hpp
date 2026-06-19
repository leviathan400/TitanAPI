#pragma once
// command.hpp - the Outpost 2 command-packet wire structs (Layer 1 / op2::abi::raw).
//
// A "command" is one CommandPacket: a 14-byte header + a 102-byte type-dependent payload union. It is the
// unit of lockstep multiplayer - clients exchange these packets, not state - and the engine applies a unit
// order by building one of these and handing it to ProcessCommandPacket. So this header is the spec the
// Layer 2 order API is built on (see design/FACADE-DESIGN.md, re-reference/command-packets.md).
//
// FACTS, not borrowed code: these layouts (sizes/offsets/field order) are properties of Outpost2.exe's wire
// format, reverse-engineered by the community. Field names/comments here are our own. Every layout is pinned
// by static_assert at the bottom - CommandPacket is exactly 116 bytes.
//
// SIZE NOTE: the total is 116 (payload 0x66 = 102), per the battle-tested OP2MissionSDK Structs.h. TethysAPI
// reconstructs it as 113 (payload 99) by summing its CommandPacketData union (max member ChatCommand = 99).
// We follow OP2MissionSDK - the larger, proven value - so the struct never under-allocates on a bulk-copy
// (network/playback/save) path. Each typed payload below still fits (all < 102). See op2missionsdk-api.md.
//
// Everything is #pragma pack(1): no padding, enums sit at byte offsets. unitID[1] / waypoint[1] are FLEXIBLE
// arrays - the [1] is a placeholder; a real packet writes numUnits ids / numWaypoints waypoints inline.

#include <cstdint>
#include <cstddef>             // offsetof
#include "op2/abi/enums.hpp"   // MapID, CommandType, GameOpt, QuitMethod

namespace op2::abi::raw {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using i32 = std::int32_t;

// Wire constants (OP2MissionSDK Structs.h: CommandPacket is 116, payload buffer is 0x66).
inline constexpr int CommandPacketSize     = 116;        // sizeof(CommandPacket)
inline constexpr int CommandPacketDataSize = 0x66;       // 102 = 116 - 0x0E header; the payload union's size
inline constexpr int AllPlayers            = -1;         // "applies to every player"

#pragma pack(push, 1)

// ---------------------------------------------------------------------------------------------------------
// Supporting packed value types (API\Location.h)
// ---------------------------------------------------------------------------------------------------------

/// A waypoint, packed into 32 bits (pixel coordinates). Bit layout is decoded by the builder layer; here we
/// only mirror the storage word so packet sizes are exact.
struct Waypoint {
  u32 u32All;
};

/// Map rectangle as four 16-bit tile coordinates. Inclusivity differs per command (BuildWall's BR is NOT
/// inclusive - see command-packets.md §4); the order API normalizes that.
struct PackedMapRect {
  u16 x1, y1, x2, y2;
};

/// Target of Attack/Guard/Repair: either a tile (tileX,tileY) or a unit (unitID, with tileY set to -1).
struct CommandTarget {
  union { u16 tileX; u16 unitID; };
  u16 tileY;
};

/// 7-player bitmask (one bit per player). 32-bit form and the 8-bit "packed" form used by Chat's dest mask.
struct PlayerBitmask {
  union {
    struct { u32 player0:1, player1:1, player2:1, player3:1, player4:1, player5:1, player6:1; };
    u32 mask;
  };
};
struct PackedPlayerBitmask {
  union {
    struct { u8 player0:1, player1:1, player2:1, player3:1, player4:1, player5:1, player6:1; };
    u8 mask;
  };
};

// ---------------------------------------------------------------------------------------------------------
// Unit-list headers - most orders begin with one of these (Game\CommandPacket.h)
// ---------------------------------------------------------------------------------------------------------

/// Exactly one unit.
struct SingleUnitSimpleCommand {
  u16 unitID;
};

/// A LIST of units: numUnits, then unitID[0..numUnits-1] inline. The single helper that fills this header is
/// where TethysAPI's DoMiningRoute bug lived (it wrote numUnits = the unit id) - the order API owns it now.
struct SimpleCommand {
  u8  numUnits;
  u16 unitID[1];          // flexible array
};

/// A unit list plus a waypoint list (pixels). Base of Build/RemoveWall, payload of Move/Dock/Patrol/etc.
struct MoveCommand : SimpleCommand {
  u16      numWaypoints;
  Waypoint waypoint[1];   // flexible array
};

// ---------------------------------------------------------------------------------------------------------
// Payload structs (Game\CommandPacket.h) - declaration order mirrors the source
// ---------------------------------------------------------------------------------------------------------

/// Mine -> smelter -> mine cargo loop (3 waypoints) plus the route's mine/smelter indices and unit ids.
struct CargoRouteCommand : SimpleCommand {
  u16      numWaypoints;        // = 3
  Waypoint waypoint[3];         // mine, smelter, mine
  u16      mineWaypointIndex;
  u16      smelterWaypointIndex;
  u16      mineUnitId;
  u16      smelterUnitId;
};

struct DozeCommand : SimpleCommand {
  PackedMapRect rect;           // tiles, inclusive
};

struct BuildCommand : MoveCommand {
  PackedMapRect rect;
  u16           unknown;        // -> MapObj+0x6A, usually -1
};

struct BuildWallCommand : SimpleCommand {
  PackedMapRect rect;           // BR NOT inclusive
  u16           tubeWallType;   // MapID
  u16           unknown;        // = 0
};

struct RemoveWallCommand : MoveCommand {
  PackedMapRect rect;
};

struct ProduceCommand : SingleUnitSimpleCommand {
  u16 itemToProduce;            // MapID
  u16 weaponType;              // MapID
  u16 scGroupIndex;            // -1 = none
};

struct TransferCommand : SimpleCommand {
  u16 toPlayerNum;
};

struct TransferCargoCommand : SingleUnitSimpleCommand {
  u16 bay;
  u16 unknown;                  // 0
};

struct RecycleCommand : SingleUnitSimpleCommand {
  u16 unknown;
};

struct ResearchCommand : SingleUnitSimpleCommand {
  u16 techNum;
  u16 numScientists;
};

struct TrainScientistsCommand : SingleUnitSimpleCommand {
  u16 numScientists;
};

struct LaunchCommand : SingleUnitSimpleCommand {
  u16 targetPixelX;
  u16 targetPixelY;
};

struct RepairCommand : SimpleCommand {
  u16           unknown1;       // 0
  CommandTarget target;
};

struct SalvageCommand : SingleUnitSimpleCommand {
  PackedMapRect rect;
  u16           unitIDGorf;
};

/// God-mode spawn: up to 8 units in one packet.
struct CreateCommand {
  u16 numUnits;
  struct Item {
    MapID unitType;
    u16   tileX;
    u16   tileY;
    MapID weaponOrCargo;
  } unit[8];
};

struct LightToggleCommand : SimpleCommand {
  u16 headlightState;
};

struct AttackCommand : SimpleCommand {
  u16           unknown;        // 0
  CommandTarget target;
};

// ---- engine / session commands (not unit orders) ----

/// Engine poke / cheat surface (UnlimitedResources, resource grants, game speed, raw GameImpl dwords...).
struct GameOptCommand {
  u16 unknown;
  union { GameOpt variableID; u16 offsetInDwords; };
  u16 value;                    // param1
  u16 playerNum;                // param2
};

struct ChatCommand {
  u8                  playerID;
  PackedPlayerBitmask dstPlayerMask;
  char                message[97];
};

struct QuitCommand {
  QuitMethod quitMethod;
  u8         delay;             // ~2
};

struct AllyCommand {
  u16 fromPlayerID;
  u16 toPlayerID;
};

struct MachineSettingsCommand {
  u16 cpuSpeed;
  u16 memoryMB;                 // x4
  u16 windowWidth;
  u16 windowHeight;
};

// ---------------------------------------------------------------------------------------------------------
// The packet
// ---------------------------------------------------------------------------------------------------------

/// The payload union: exactly one member is live, selected by CommandPacket::type. buffer[] pins it to 102.
union CommandPacketData {
  SimpleCommand           simple;
  SingleUnitSimpleCommand singleUnitSimple;
  DozeCommand             doze;
  MoveCommand             move;
  BuildCommand            build;
  BuildWallCommand        buildWall;
  RemoveWallCommand       removeWall;
  ProduceCommand          produce;
  TransferCommand         transfer;
  TransferCargoCommand    transferCargo;
  RecycleCommand          recycle;
  ResearchCommand         research;
  TrainScientistsCommand  trainScientists;
  LaunchCommand           launch;
  RepairCommand           repair;
  SalvageCommand          salvage;
  CreateCommand           create;
  LightToggleCommand      lightToggle;
  AttackCommand           attack;
  CargoRouteCommand       cargoRoute;
  GameOptCommand          gameOpt;
  ChatCommand             chat;
  QuitCommand             quit;
  AllyCommand             ally;
  MachineSettingsCommand  machineSettings;
  u8                      buffer[CommandPacketDataSize];
};

/// One command. 14-byte header (type/dataLength/timeStamp/netID) + 102-byte payload. timeStamp/netID are
/// only meaningful on the network; a locally-issued order leaves them 0.
struct CommandPacket {
  CommandType       type;       // selects the live CommandPacketData member
  u16               dataLength; // bytes of payload actually used
  i32               timeStamp;  // game tick (network)
  i32               netID;      // player net id (network)
  CommandPacketData data;
};

#pragma pack(pop)

// ---------------------------------------------------------------------------------------------------------
// Layout pins - these are the whole point of this header. If any fires, the wire format diverged.
// ---------------------------------------------------------------------------------------------------------

// Value types
static_assert(sizeof(Waypoint)            == 4,  "Waypoint");
static_assert(sizeof(PackedMapRect)       == 8,  "PackedMapRect");
static_assert(sizeof(CommandTarget)       == 4,  "CommandTarget");
static_assert(sizeof(PlayerBitmask)       == 4,  "PlayerBitmask");
static_assert(sizeof(PackedPlayerBitmask) == 1,  "PackedPlayerBitmask");

// Headers
static_assert(sizeof(SingleUnitSimpleCommand) == 2, "SingleUnitSimpleCommand");
static_assert(sizeof(SimpleCommand)           == 3, "SimpleCommand");
static_assert(sizeof(MoveCommand)             == 9, "MoveCommand");

// Payloads
static_assert(sizeof(CargoRouteCommand)      == 25, "CargoRouteCommand");
static_assert(sizeof(DozeCommand)            == 11, "DozeCommand");
static_assert(sizeof(BuildCommand)           == 19, "BuildCommand");
static_assert(sizeof(BuildWallCommand)       == 15, "BuildWallCommand");
static_assert(sizeof(RemoveWallCommand)      == 17, "RemoveWallCommand");
static_assert(sizeof(ProduceCommand)         ==  8, "ProduceCommand");
static_assert(sizeof(TransferCommand)        ==  5, "TransferCommand");
static_assert(sizeof(TransferCargoCommand)   ==  6, "TransferCargoCommand");
static_assert(sizeof(RecycleCommand)         ==  4, "RecycleCommand");
static_assert(sizeof(ResearchCommand)        ==  6, "ResearchCommand");
static_assert(sizeof(TrainScientistsCommand) ==  4, "TrainScientistsCommand");
static_assert(sizeof(LaunchCommand)          ==  6, "LaunchCommand");
static_assert(sizeof(RepairCommand)          ==  9, "RepairCommand");
static_assert(sizeof(SalvageCommand)         == 12, "SalvageCommand");
static_assert(sizeof(CreateCommand)          == 98, "CreateCommand");
static_assert(sizeof(LightToggleCommand)     ==  5, "LightToggleCommand");
static_assert(sizeof(AttackCommand)          ==  9, "AttackCommand");
static_assert(sizeof(GameOptCommand)         ==  8, "GameOptCommand");
static_assert(sizeof(ChatCommand)            == 99, "ChatCommand");           // largest typed payload (fits the 102 buffer)
static_assert(sizeof(QuitCommand)            ==  2, "QuitCommand");
static_assert(sizeof(AllyCommand)            ==  4, "AllyCommand");
static_assert(sizeof(MachineSettingsCommand) ==  8, "MachineSettingsCommand");

// The packet itself - header offsets + total size.
static_assert(sizeof(CommandPacketData) == CommandPacketDataSize, "CommandPacketData == 102");
static_assert(sizeof(CommandPacket)     == CommandPacketSize,     "CommandPacket == 116");
static_assert(offsetof(CommandPacket, type)       == 0x00, "type @ 0x00");
static_assert(offsetof(CommandPacket, dataLength) == 0x04, "dataLength @ 0x04");
static_assert(offsetof(CommandPacket, timeStamp)  == 0x06, "timeStamp @ 0x06");
static_assert(offsetof(CommandPacket, netID)      == 0x0A, "netID @ 0x0A");
static_assert(offsetof(CommandPacket, data)       == 0x0E, "data @ 0x0E");

} // namespace op2::abi::raw
