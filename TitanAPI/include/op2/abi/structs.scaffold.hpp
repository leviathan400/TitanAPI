// AUTO-GENERATED SCAFFOLD by tools/gen-structs.ps1 from re-reference/struct-fields.csv.
// *** NOT guaranteed complete or compilable. ***  Base-class fields are NOT inlined (see // base:),
// rare declarations are skipped, anonymous members are flattened. Curate against the live header;
// the commented static_assert(sizeof) lines (from struct-sizes.csv) are the completeness check.
#pragma once
#include <cstdint>

namespace op2::abi::raw {

// ---- forward declarations ----
struct _Player;
struct _Player___anon_;
struct AdaptiveHuffmanTree;
struct AIModDesc;
struct AllyCommand;
struct AnimationInfo;
struct AnyMapObj;
struct AreaIteratorBase;
struct AttackCommand;
struct BaseVBlkRWStream;
struct BaseVolFileStream;
struct BayButton;
struct BitmapCopyInfo;
struct BlightManager;
struct BuildCommand;
struct Building;
struct BuildingGroupImpl;
struct BuildingStats;
struct BuildWallCommand;
struct ButtonDisplayInfo;
struct CargoKit;
struct CargoRouteCommand;
struct CConfig;
struct CellTypeInfo;
struct ChatCommand;
struct ClosestEnumerator;
struct CmdPacketWriter;
struct CommandPacket;
struct CommandPacketData;
struct CommandPane;
struct CommandTarget;
struct CommandTarget___anon_;
struct CommandViewButton;
struct ConnectedTileMapping;
struct CreateCommand;
struct CreateCommand___anon_;
struct CreateGameInfo;
struct DayNightManager;
struct DefaultScGroupInfo;
struct DetailPane;
struct DozeCommand;
struct EchoExternalAddress;
struct FactoryBuilding;
struct FightGroupImpl;
struct FileRStream;
struct FileRWStream;
struct FileWStream;
struct FilterNode;
struct FilterPlayerUnitIterator;
struct Font;
struct FrameComponentInfo;
struct FrameInfo;
struct FrameInfo___anon_;
struct FrameOptionalInfo;
struct FuncReference;
struct Game;
struct GameFlags;
struct GameFrame;
struct GameImpl;
struct GameMap;
struct GameMessage;
struct GameOptCommand;
struct GameOptCommand___anon_;
struct GameServerPoke;
struct GameStartInfo;
struct GameStartInfo___anon_;
struct GameVersion;
struct GameVersion___anon_;
struct Garage;
struct GFXBitmap;
struct GFXCDSSurface;
struct GFXClippedSurface;
struct GFXLightAdjustedBitmap;
struct GFXMemSurface;
struct GFXPalette;
struct GFXSpriteBitmap;
struct GFXSurface;
struct GFXTilesetBitmap;
struct GlobalUnitStats;
struct GlobalUnitStats___anon_;
struct GlyphMetrics;
struct GroupFilter;
struct GroupIterator;
struct GUID;
struct GurManager;
struct HostedGameSearchQuery;
struct HostedGameSearchReply;
struct HostGameParameters;
struct HotKeyFilter;
struct IDlgWnd;
struct ImageInfo;
struct IniSettings;
struct InRangeEnumerator;
struct InRectEnumerator;
struct IWnd;
struct JoinHelpRequest;
struct JoinReply;
struct JoinRequest;
struct JoinReturned;
struct LaunchCommand;
struct Library;
struct Lightning;
struct LightToggleCommand;
struct ListItem;
struct ListStyle;
struct LoadGameDialog;
struct Location;
struct LocationEnumerator;
struct LongWaypoint;
struct LZHRStream;
struct LZHWStream;
struct LZRStream;
struct LZWStream;
struct MachineSettingsCommand;
struct MapChildEntity;
struct MapImpl;
struct MapObjDrawList;
struct MapObject;
struct MapObject___anon_;
struct MapObject___anon____anon_;
struct MapObjectManager;
struct MapObjectType;
struct MapRect;
struct MemoryMappedFile;
struct MemRWStream;
struct MenuState;
struct MessageLog;
struct MessageLogEntry;
struct MineGroupImpl;
struct MineManager;
struct MiniMap;
struct MiniMapPane;
struct MiningBeacon;
struct MissionManager;
struct MissionResults;
struct ModDesc;
struct ModDescEx;
struct MoraleManager;
struct MoraleModifier;
struct MoraleState;
struct MouseCommandFilter;
struct MouseCommandFilter___anon_;
struct MouseCommandFilter___anon____anon_;
struct MoveCommand;
struct MrRec;
struct MSG;
struct MsgBoxDlg;
struct MultiplayerLobbyDialog;
struct MusicManager;
struct NetBuffer;
struct NetGameSession;
struct NetPeerInfo;
struct NetTransportLayer;
struct OnChatArgs;
struct OnCreateUnitArgs;
struct OnDamageUnitArgs;
struct OnDestroyUnitArgs;
struct OnEndMissionArgs;
struct OnGameCommandArgs;
struct OnLoadMissionArgs;
struct OnLoadSavedGameArgs;
struct OnSaveGameArgs;
struct OnTransferUnitArgs;
struct OnTriggerArgs;
struct OnUnloadMissionArgs;
struct PackedLocation;
struct PackedMapRect;
struct PackedUnitGroup;
struct Packet;
struct Packet___anon_;
struct PacketHeader;
struct PathContext;
struct PathContext___anon_;
struct PathContextList;
struct PathFinder;
struct PatrolRoute;
struct PerPlayerUnitStats;
struct PerPlayerUnitStats___anon_;
struct PerPlayerUnitStats___anon____anon_;
struct PlayerBitmaskImpl;
struct PlayerBitmaskImpl___anon_;
struct PlayerControls;
struct PlayerEndInfo;
struct PlayerFlags;
struct PlayerGurInfo;
struct PlayerImpl;
struct PlayerImpl___anon_;
struct PlayersList;
struct PlayerStartInfo;
struct PlayerUnitEnumBase;
struct PlayerUnitIterator;
struct POINT;
struct ProduceCommand;
struct ProtocolControlMapping;
struct PWDef;
struct QuitCommand;
struct Random;
struct RecordedBuilding;
struct RecordedMine;
struct RecordedTubeWall;
struct RecordedVehGroup;
struct RecordInfo;
struct RECT;
struct RecycleCommand;
struct RemoveWallCommand;
struct RenderChunk;
struct RenderData;
struct RenderDataBase;
struct RepairCommand;
struct ReportPageButton;
struct RequestExternalAddress;
struct Research;
struct ResearchCommand;
struct ResearchState;
struct ResManager;
struct Rgb555;
struct Rgb555___anon_;
struct RLERStream;
struct RLEWStream;
struct SalvageCommand;
struct SatelliteCounts;
struct SaveGameDialog;
struct SaveRegion;
struct ScanlineCopyInfo;
struct ScBase;
struct ScGroupImpl;
struct ScriptDataBlock;
struct ScStub;
struct ScStubFactory;
struct ScStubList;
struct Sheet;
struct SheetParser;
struct SimpleCommand;
struct SingleUnitSimpleCommand;
struct SoundBufferInfo;
struct Spaceport;
struct Span;
struct SpriteManager;
struct StartupFlags;
struct StatusBar;
struct StatusUpdate;
struct StreamIO;
struct TApp;
struct TargetCount;
struct TCPGameSearchProtocol;
struct TCPGameSession;
struct TechInfo;
struct TechProperty;
struct TechProperty___anon_;
struct TechUpgradeInfo;
struct TerrainManager;
struct TerrainType;
struct TerrainType___anon_;
struct TerrainType___anon____anon_;
struct TextStream;
struct TFileDialog;
struct ThorsHammer;
struct TileData;
struct TileData___anon_;
struct TilesetMapping;
struct TPane;
struct TrafficCounters;
struct TrainScientistsCommand;
struct TransferCargoCommand;
struct TransferCommand;
struct TransportLayerHeader;
struct TransportLayerMessage;
struct TriggerImpl;
struct TruckCargo;
struct TruckLoadData;
struct TubeConnection;
struct TubeConnectionManager;
struct UIButton;
struct UICommandButton;
struct UIElement;
struct UIElementFilter;
struct UIFrameImages;
struct UIGraphicalButton;
struct UIListBox;
struct UIState;
struct Unit;
struct UnitBlock;
struct UnitGroup;
struct UnitNode;
struct UnitNode___anon_;
struct UnitRange;
struct UnitRecord;
struct UnitTypeTargetCount;
struct VBlkHeader;
struct Vehicle;
struct Vehicle___anon_;
struct VictoryConditionImpl;
struct Viewport;
struct VolFileRStream;
struct VolFileWStream;
struct VolIndexEntry;
struct Vortex;
struct Waypoint;
struct Waypoint___anon_;
struct WplInfo;
struct Wreckage;
struct YieldPercentData;

struct AreaIteratorBase {  // base: OP2Class<Iterator>
  MapObject* pCurrentUnit_;
  int field_04;
  int field_08;
  int field_0C;
  int field_10;
  int field_14;
  int field_18;
  int field_1C;
  int field_20;
  int field_24;
  int field_28;
  int field_2C;
  int field_30;
};

struct PlayerUnitIterator {  // base: TethysImpl::UnitIteratorBase
  MapObject* pMo_;
};

struct FilterPlayerUnitIterator {  // base: PlayerUnitIterator
  MapID type_;
};

struct PlayerUnitEnumBase {
  int playerNum_;
  MapID type_;
};

struct InRangeEnumerator {  // base: OP2Class<InRangeEnumerator>
  Location centerPoint_;
  int maxTileDistance_;
};

struct InRectEnumerator {  // base: OP2Class<InRectEnumerator>
  MapRect rect_;
};

struct LocationEnumerator {  // base: OP2Class<LocationEnumerator>
  Location location_;
};

struct ClosestEnumerator {  // base: OP2Class<ClosestEnumerator>
  Location location_;
};

struct Game {  // base: OP2Class<Game>
  std::uint8_t field_00;
};

struct GameMap {  // base: OP2Class<GameMap>
  std::uint8_t field_00;
};

struct Waypoint___anon_ {
  std::uint32_t x : 15;
  std::uint32_t y : 14;
};

struct Waypoint {
  std::uint32_t u32All;
};

struct PackedLocation {
  std::uint16_t x;
  std::uint16_t y;
};

struct PackedMapRect {
  std::uint16_t x1;
  std::uint16_t y1;
  std::uint16_t x2;
  std::uint16_t y2;
};

struct Location {  // base: OP2Class<Location>
  int x;
  int y;
};

struct MapRect {  // base: OP2Class<MapRect>
  int x1;
  int y1;
  int x2;
  int y2;
};

struct PatrolRoute {
  int field_00;
  const Location* pWaypoints;
};

struct ModDesc {
  MissionType missionType;
  int numPlayers;
  int maxTechLevel;
  int unitMission;
};

struct ModDescEx {
  int numMultiplayerAIs;
  int field_04;
  int field_08;
  int field_0C;
  int field_10;
  int field_14;
  int field_18;
  int field_1C;
};

struct SaveRegion {
  size_t size;
};

struct OnLoadMissionArgs {
  size_t structSize;
};

struct OnUnloadMissionArgs {
  size_t structSize;
};

struct OnEndMissionArgs {
  size_t structSize;
  MissionResults* pMissionResults;
};

struct OnSaveGameArgs {
  size_t structSize;
  StreamIO* pSavedGame;
};

struct OnLoadSavedGameArgs {
  size_t structSize;
  StreamIO* pSavedGame;
};

struct OnChatArgs {
  size_t structSize;
  char* pText;
  size_t bufferSize;
  int playerNum;
};

struct OnGameCommandArgs {
  size_t structSize;
  CommandPacket* pPacket;
  int playerNum;
};

struct _Player {  // base: OP2Class<_Player>
  int id_;
  } check_;
};

struct _Player___anon_ {
  std::uint8_t canBuildBuilding_;
  std::uint8_t canBuildVehicle_;
  std::uint8_t canBuildSpace_;
  std::uint8_t canAccumulateOre_;
  std::uint8_t canAccumulateRareOre_;
  std::uint8_t canResearchBasic_;
  std::uint8_t canResearchStandard_;
  std::uint8_t canResearchAdvanced_;
};

struct GroupIterator {  // base: TethysImpl::UnitIteratorBase
  const ScGroupImpl::UnitNode* pNode_;
};

struct MrRec {
  MapID unitType;
  MapID weaponType;
  int unknown1;
  int unknown2;
};

struct PWDef {
  int x1;
  int y1;
  int x2;
  int y2;
  int x3;
  int y3;
  int x4;
  int y4;
  int time1;
  int time2;
  int time3;
};

struct ScStub {  // base: OP2Class<ScStub>
  int id_;
};

struct OnTriggerArgs {
  size_t structSize;
  Trigger trigger;
  PlayerBitmask triggeredBy;
  PlayerBitmask prevTriggeredBy;
};

struct CargoKit {
  MapID unitType;
  MapID cargoOrWeaponType;
};

struct TruckCargo {
  CargoType cargoType;
  int amount;
};

struct Unit {  // base: OP2Class<Unit>
  int id_;
};

struct OnCreateUnitArgs {
  size_t structSize;
  Unit unit;
};

struct OnDestroyUnitArgs {
  size_t structSize;
  Unit unit;
};

struct OnDamageUnitArgs {
  size_t structSize;
  Unit sourceUnit;
  Unit targetUnit;
  int damage;
};

struct OnTransferUnitArgs {
  size_t structSize;
  Unit unit;
  int fromPlayerNum;
  int toPlayerNum;
};

struct UnitRecord {
  MapID unitType;
  int x;
  int y;
  int field_0C;
  int rotation;
  MapID weaponType;
  UnitClassification unitClassification;
  std::uint16_t cargoType;
  std::uint16_t cargoAmount;
};

struct UnitRange {
  int startIndex;
  int untilIndex;
};

struct UnitBlock {  // base: OP2Class<UnitBlock>
  int numUnits_;
  UnitRange classRange_[16];
  UnitRecord* pUnitRecordTable_;
};

struct Library {
  HMODULE hModule_;
  bool ownHandle_;
};

struct Span {
  const T* pData_;
  size_t length_;
};

struct POINT {
  LONG x;
  LONG y;
};

struct MSG {
  HWND hwnd;
  UINT message;
  WPARAM wParam;
  LPARAM lParam;
  DWORD time;
  POINT pt;
  DWORD lPrivate;
};

struct RECT {
  LONG left;
  LONG top;
  LONG right;
  LONG bottom;
};

struct GUID {
  std::uint32_t data1;
  std::uint16_t data2;
  std::uint16_t data3;
  std::uint8_t data4[8];
};

struct BlightManager {  // base: OP2Class<BlightManager>
  int currentPosition_;
  int field_08;
  int field_0C;
  int spreadSpeed_;
};

struct PlayerBitmaskImpl___anon_ {
  BitmaskType player0 : 1;
  BitmaskType player1 : 1;
  BitmaskType player2 : 1;
  BitmaskType player3 : 1;
  BitmaskType player4 : 1;
  BitmaskType player5 : 1;
  BitmaskType player6 : 1;
};

struct PlayerBitmaskImpl {
  BitmaskType mask;
};

struct CommandTarget___anon_ {
  std::uint16_t tileX;
  std::uint16_t unitID;
};

struct CommandTarget {
  std::uint16_t tileY;
};

struct SingleUnitSimpleCommand {
  std::uint16_t unitID;
};

struct SimpleCommand {
  std::uint8_t numUnits;
  std::uint16_t unitID[1];
};

struct MoveCommand {  // base: SimpleCommand
  std::uint16_t numWaypoints;
  Waypoint waypoint[1];
};

struct CargoRouteCommand {  // base: SimpleCommand
  std::uint16_t numWaypoints;
  Waypoint waypoint[3];
  std::uint16_t mineWaypointIndex;
  std::uint16_t smelterWaypointIndex;
  std::uint16_t mineUnitId;
  std::uint16_t smelterUnitId;
};

struct DozeCommand {  // base: SimpleCommand
  PackedMapRect rect;
};

struct BuildCommand {  // base: MoveCommand
  PackedMapRect rect;
  std::uint16_t unknown;  // off~0x6A
};

struct BuildWallCommand {  // base: SimpleCommand
  PackedMapRect rect;
  std::uint16_t tubeWallType;
  std::uint16_t unknown;
};

struct RemoveWallCommand {  // base: MoveCommand
  PackedMapRect rect;
};

struct ProduceCommand {  // base: SingleUnitSimpleCommand
  std::uint16_t itemToProduce;
  std::uint16_t weaponType;
  std::uint16_t scGroupIndex;
};

struct TransferCommand {  // base: SimpleCommand
  std::uint16_t toPlayerNum;
};

struct TransferCargoCommand {  // base: SingleUnitSimpleCommand
  std::uint16_t bay;
  std::uint16_t unknown;
};

struct RecycleCommand {  // base: SingleUnitSimpleCommand
  std::uint16_t unknown;
};

struct ResearchCommand {  // base: SingleUnitSimpleCommand
  std::uint16_t techNum;
  std::uint16_t numScientists;
};

struct TrainScientistsCommand {  // base: SingleUnitSimpleCommand
  std::uint16_t numScientists;
};

struct LaunchCommand {  // base: SingleUnitSimpleCommand
  std::uint16_t targetPixelX;
  std::uint16_t targetPixelY;
};

struct RepairCommand {  // base: SimpleCommand
  std::uint16_t unknown1;
  CommandTarget target;
};

struct SalvageCommand {  // base: SingleUnitSimpleCommand
  PackedMapRect rect;
  std::uint16_t unitIDGorf;
};

struct CreateCommand {
  std::uint16_t numUnits;
  } unit[8];
};

struct CreateCommand___anon_ {
  MapID unitType;
  std::uint16_t tileX;
  std::uint16_t tileY;
  MapID weaponOrCargo;
};

struct LightToggleCommand {  // base: SimpleCommand
  std::uint16_t headlightState;
};

struct AttackCommand {  // base: SimpleCommand
  std::uint16_t unknown;
  CommandTarget target;
};

struct GameOptCommand {
  std::uint16_t unknown;
  std::uint16_t value;
  std::uint16_t playerNum;
};

struct GameOptCommand___anon_ {
  GameOpt variableID;
  std::uint16_t offsetInDwords;
};

struct ChatCommand {
  std::uint8_t playerID;
  PackedPlayerBitmask dstPlayerMask;
};

struct QuitCommand {
  QuitMethod quitMethod;
  std::uint8_t delay;
};

struct AllyCommand {
  std::uint16_t fromPlayerID;
  std::uint16_t toPlayerID;
};

struct MachineSettingsCommand {
  std::uint16_t cpuSpeed;
  std::uint16_t memoryMB;
  std::uint16_t windowWidth;
  std::uint16_t windowHeight;
};

struct CommandPacketData {
  SimpleCommand simple;
  SingleUnitSimpleCommand singleUnitSimple;
  DozeCommand doze;
  MoveCommand move;
  BuildCommand build;
  BuildWallCommand buildWall;
  RemoveWallCommand removeWall;
  ProduceCommand produce;
  TransferCommand transfer;
  TransferCargoCommand transferCargo;
  RecycleCommand recycle;
  ResearchCommand research;
  TrainScientistsCommand trainScientists;
  LaunchCommand launch;
  RepairCommand repair;
  SalvageCommand salvage;
  CreateCommand create;
  LightToggleCommand lightToggle;
  AttackCommand attack;
  CargoRouteCommand cargoRoute;
  GameOptCommand gameOpt;
  ChatCommand chat;
  QuitCommand quit;
  AllyCommand ally;
  MachineSettingsCommand machineSettings;
  std::uint8_t buffer[CommandPacketDataSize];  // off~0x0E
};

struct CommandPacket {
  CommandType type;
  std::uint16_t dataLength;
  int timeStamp;
  int netID;
  CommandPacketData data;
};

struct CmdPacketWriter {
  CommandPacket packet_;
  uint8* pDataWriter_;
  std::uint16_t units_[32];
  size_t numUnits_;
  Waypoint waypoints_[8];
  size_t numWaypoints_;
};

struct GameImpl {  // base: OP2Class<GameImpl>
  int unlimitedResources_;
  int produceAll_;
  int logMorale_;
  int quadDamage_;
  int fastUnits_;
  int fastProduction_;
  int showUnitPaths_;
  int allUnitsVisible_;
  int field_20;
  int field_24;
  int field_28;
  int field_2C;
  int field_30;
  int field_34;
  int forceDisableRCC_;
  int forceEnableRCC_;
  int dataChecking_;
  int strictMode_;
  int daylightMoves_;
  int daylightEverywhere_;
  int gameSpeed_;
  int numPlayers_;
  int numHumanPlayers_;
  int commandPacketProcessingInterval_;
  int log2CommandPacketProcessingInterval_;
  int networkCommandPacketArraySpace_;
  int commandPacketProcessingTick_;
  int commandPacketProcessingDelay_;
  int tick_;
  int tickOfLastSetGameOpt_;
  int field_8C;
  int localPlayer_;
  PlayerBitmask chatDstMask_;
  int startFadeOutTick_;
  GameTermReasons gameTermReasons_;
  int skipProgressSave_;
  GameStartInfo gameStartInfo_;
  int field_160[201];
  PlayerImpl player_[MaxPlayers];
  GameNetLayer* pGameNetLayer_;
};

struct StartupFlags {
  std::uint32_t disastersEnabled : 1;
  std::uint32_t dayNightEnabled : 1;
  std::uint32_t moraleEnabled : 1;
  std::uint32_t isCampaign : 1;
  std::uint32_t isMultiplayer : 1;
  std::uint32_t cheatsEnabled : 1;
  std::uint32_t maxPlayers : 3;
  std::uint32_t missionType : 5;
  std::uint32_t b1 : 3;
  std::uint32_t numInitialVehicles : 4;
};

struct GameFlags {
  std::uint32_t numPlayers : 3;
  std::uint32_t isReplay : 1;
};

struct PlayerFlags {
  std::uint32_t b1 : 1;
  std::uint32_t isHost : 1;
  std::uint32_t isEden : 1;
  std::uint32_t resources : 2;
  std::uint32_t color : 3;
};

struct GameStartInfo {  (size=186)
  StartupFlags startupFlags;
  int version;
  std::uint32_t field_08;
  GameFlags gameFlags;
  char scriptName[32];
  PlayerFlags playerFlags[6];
  int playerNetID[6];
  } playerName[6];
  int gameSpeed;
  std::uint32_t randomNumberSeed;
  std::uint32_t checksum;
  // static_assert(sizeof(GameStartInfo) == 186);  // verify after completing fields
};

struct GameStartInfo___anon_ {
  char str[13];
};

struct PlayerEndInfo {
  int structuresBuilt;
  int vehiclesBuilt;
  int structuresLost;
  int vehiclesLost;
  int enemyStructuresDestroyed;
  int enemyVehiclesDestroyed;
  int oreHarvested;
};

struct SatelliteCounts {
  std::uint32_t numRLV : 4;
  std::uint32_t numSolarSatellites : 4;
  std::uint32_t b1 : 1;
  std::uint32_t numEDWARDSatllites : 4;
};

struct PlayerStartInfo {
  int food;
  int commonOre;
  int rareOre;
  int workers;
  int scientists;
  int kids;
  SatelliteCounts satelliteCounts;
};

struct MoraleState {
  MoraleLevel moraleLevel;
  int eventMoraleModifier;
  int field_08;
  int morale;
  int field_10;
  int residenceDemandPercent;
  FoodStatus foodSupply;
  int disabledBuildingRatio;
  int recForumDemandPercent;
  int field_24;
  int medCenterDemandPercent;
  int isNurseryOperational;
  int isUniversityOperational;
  int numDIRTs;
  int dirtAvgDamagePrevention;
  int unoccupiedColonistsPercent;
  int scientistsAsWorkersPercent;
  int isGORFOperational;
};

struct ResearchState {
  int numTechs;
  std::uint16_t techLevel;
  std::uint8_t techNum[256];
  PackedPlayerBitmask playerBitVector[256];
};

struct MissionResults {  // base: GameStartInfo  (size=989)
  GameTermReasons gameTermReason;
  std::uint32_t field_BE;
  PlayerEndInfo playerEndInfo[6];
  TethysAPI::MissionType missionType;
  PlayerStartInfo playerStartInfo;
  std::uint8_t localPlayerID;
  int tick;
  MoraleState moraleState;
  ResearchState researchState;
  // static_assert(sizeof(MissionResults) == 989);  // verify after completing fields
};

struct TileData___anon_ {
  std::uint32_t cellType : 5;
  std::uint32_t tileIndex : 11;
  std::uint32_t unitIndex : 11;
  std::uint32_t lava : 1;
  std::uint32_t lavaPossible : 1;
  std::uint32_t expand : 1;
  std::uint32_t microbe : 1;
  std::uint32_t wallOrBuilding : 1;
};

struct TileData {
  std::uint32_t u32All;
};

struct CellTypeInfo {
  char* pName;
  int field_18;
  int blightSpeed;
  int field_20;
};

struct TilesetMapping {
  std::uint16_t tilesetIndex;
  std::uint16_t tileIndex;
  std::uint16_t numAnimation;
  std::uint16_t animationDelay;
};

struct ConnectedTileMapping {
  std::uint16_t leftRight;
  std::uint16_t topBottom;
  std::uint16_t leftBottom;
  std::uint16_t rightBottom;
  std::uint16_t leftTop;
  std::uint16_t rightTop;
  std::uint16_t leftRightTop;
  std::uint16_t leftRightBottom;
  std::uint16_t leftRightTopBottom;
  std::uint16_t leftTopBottom;
  std::uint16_t rightTopBottom;
  std::uint16_t bottom;
  std::uint16_t top;
  std::uint16_t right;
  std::uint16_t left;
  std::uint16_t none;
};

struct TerrainType {  (size=264)
  std::uint16_t firstTile;
  std::uint16_t lastTile;
  std::uint16_t bulldozed;
  std::uint16_t rubbleStart;
  std::uint16_t playerTube[6];
  ConnectedTileMapping lavaWall;
  ConnectedTileMapping microbeWall;
  ConnectedTileMapping wall;
  ConnectedTileMapping wallLightDamage;
  ConnectedTileMapping wallHeavyDamage;
  std::uint16_t lavaStart;
  std::uint16_t field_B6;
  std::uint16_t field_B8;
  std::uint16_t field_BA;
  ConnectedTileMapping tube;
  } scorched[1];
  int field_EA[7];
  // static_assert(sizeof(TerrainType) == 264);  // verify after completing fields
};

struct TerrainType___anon_ {
  std::uint16_t tile;
  } range[3];
};

struct TerrainType___anon____anon_ {
  std::uint16_t start;
  std::uint16_t end;
};

struct TerrainManager {  // base: OP2Class<TerrainManager>
  int numTilesetMappings_;
  TilesetMapping* pTilesetMappings_;
  int field_08;
  int numTerrainTypes_;
  TerrainType* pTerrainTypes_;
};

struct DayNightManager {  // base: OP2Class<DayNightManager>
  int field_00;
  int dayNight_;
  int field_08[5];
  int field_34[4];
  int actualPosition_;
  int position_;
  int field_4C;
};

struct MapImpl {  // base: OP2Class<MapImpl>  (size=1136)
  int field_00;
  std::uint32_t pixelWidthMask_;
  int pixelWidth_;
  std::uint32_t tileXMask_;
  int tileWidth_;
  std::uint8_t log2TileWidth_;
  int tileHeight_;
  MapRect clipRect_;
  int numTiles_;
  int numTilesets_;
  std::uint8_t log2TileHeight_;
  int paddingOffsetTileX_;
  int numUnits_;
  int lastUsedUnitIndex_;
  int nextFreeUnitIndex_;
  int firstFreeUnitIndex_;
  MapObject** ppMapObjFreeList_;
  AnyMapObj* pMapObjArray_;
  MapObject* pMapObjListBegin_;
  MapObject* pMapObjListEnd_;
  std::uint8_t lightLevelAdjustTable_[1024];
  std::uint32_t field_45C;
  char** pTilesetNames_;
  TileData* pTileArray_;
  GFXTilesetBitmap** ppTilesetBitmaps_;
  TerrainManager* pTerrainManager_;
  // static_assert(sizeof(MapImpl) == 1136);  // verify after completing fields
};

struct MapObject {  // base: OP2Class<MapObject>  (size=72)
  MapObject* pNext_;
  MapObject* pPrev_;
  MapObject* pPlayerNext_;
  int index_;
  int pixelX_;
  int pixelY_;
  std::uint8_t rotation_;
  std::int16_t damage_;
  std::uint8_t isBusy_;
  std::uint8_t command_;
  ActionType action_;
  ActionType executingAction_;
  std::uint16_t attackingUnitIndex_;
  int field_28;
  std::uint8_t unitTypeInstanceNum_;
  std::uint8_t field_2D;
  std::uint16_t reloadTimer_;
  std::uint8_t scGroupIndex_;
  std::uint8_t field_31;
  std::uint8_t field_32;
  std::uint8_t field_33;
  int actionTimer_;
  std::uint16_t animationIndex_;
  std::int16_t frameIndex_;
  std::uint32_t flags_;
  // static_assert(sizeof(MapObject) == 72);  // verify after completing fields
};

struct MapObject___anon____anon_ {
  std::uint16_t truckCargoType_ : 4;
  std::uint16_t targetTileX_;
  std::uint8_t ownerNum_ : 4;
  std::uint16_t targetTileY_;
  std::uint8_t creatorNum_ : 4;
  std::uint16_t truckCargoAmount_ : 12;
};

struct MapObject___anon_ {
  PathContext* pPathContext_;
  int targetPixelY_;
  std::uint16_t cargo_;
  std::uint8_t creatorAndOwnerNum_;
  int unkWaypointData_;
  int targetPixelX_;
  std::uint16_t weapon_;
  std::uint16_t tubeOrWallType_;
  TubeConnection* pTubeConnection_;
  std::uint16_t researchTimer_;
  std::uint16_t disasterDuration_;
  std::uint16_t disasterMagnitude_;
  std::uint16_t lavaSpeed_;
};

struct MapChildEntity {  // base: MapEntity
  int parentIndex_;
  CommandTarget target_;  // off~0xFFFF0000
  int field_50;
  int field_54;
};

struct Building {  // base: LandUnit
  std::uint8_t cargoBayCargoOrWeapon_[6];
  std::uint16_t field_4E;
  int field_50;
  std::uint16_t field_54;
  std::uint16_t timerEMP_;
  int field_58;
  int field_5C;
};

struct FactoryBuilding {  // base: Building
  std::uint8_t itemToProduce_;
  std::uint8_t cargoBayContents_[6];
  std::uint8_t field_67;
  int field_68;
  std::uint16_t field_6C;
};

struct Vehicle___anon_ {
  int field_48;
  MapObject* pTargetUnit_;
};

struct Vehicle {  // base: LandUnit
  std::uint16_t field_4C;
  std::uint8_t field_4E;
  std::uint8_t weaponOfCargo_;
  int field_50;
  std::uint16_t timerStickyfoam_;
  std::uint16_t timerEMP_;
  std::uint16_t timerESG_;
  std::uint16_t field_5A;
  int cargoToLoad_;
  std::uint16_t field_60;
  std::uint16_t field_62;
  int field_64;
  int field_68;
  std::uint16_t field_6C;
  int field_6E;
  int field_72;
};

struct Lightning {  // base: Disaster
  int field_48;
  int field_4C;
  int field_50;
  int field_54;
  int field_58;
  int field_5C;
  int field_60;
  std::uint16_t endTileX_;
  std::uint16_t endTileY_;
};

struct Vortex {  // base: Disaster
  int field_48;
  int directionChangeStartTick_;
  std::uint8_t direction_;  // off~0x00
  std::uint8_t field_51;
  std::uint8_t field_52;
  std::uint8_t field_53;
  int field_54;
  std::uint8_t field_58;
  std::uint8_t field_59;
  std::uint8_t field_5A;
  std::uint8_t field_5B;
  std::uint8_t field_5C;
  std::uint16_t field_5D;
  std::uint16_t field_5F;
  std::uint16_t field_61;
  std::uint16_t field_63;
  std::uint16_t endTileX_;
  std::uint16_t endTileY_;
};

struct MiningBeacon {  // base: MapChildEntity
  int numTruckLoads_;
  OreYield mineYield_;
  int mineVariant_;
  OreType mineType_;
  std::uint16_t field_65;
  PackedPlayerBitmask playerSurveyedMask_;
};

struct ThorsHammer {  // base: MapChildEntity
  int field_58;
};

struct Wreckage {  // base: MapChildEntity
  PlayerBitmask playerDiscoveredMask_;
  int techID_;
};

struct Garage {  // base: Building
  MapObject* pUnitInBay_[6];
};

struct Spaceport {  // base: FactoryBuilding
  MapID objectOnPad_;
  MapID launchCargo_;
};

struct AnyMapObj {
  MapObject object_;
  MapEntity entity_;
  Disaster disaster_;
  MapChildEntity childEntity_;
  Rocket rocket_;
  Explosive explosive_;
  Projectile projectile_;
  MapUnit unit_;
  AirUnit airUnit_;
  LandUnit landUnit_;
  Building building_;
  FactoryBuilding factoryBuilding_;
  LabBuilding labBuilding_;
  MineBuilding mineBuilding_;
  PowerBuilding powerBuilding_;
  Vehicle vehicle_;
  TankVehicle tankVehicle_;
  std::uint8_t raw_[MapObjectSize];
};

struct MapObjectManager {  // base: OP2Class<MapObjectManager>
  int field_00;
  int field_04;
};

struct PerPlayerUnitStats {  (size=68)
  int hp;
  int repairAmount;
  ArmorType armor;
  int commonCost;
  int rareCost;
  int buildTime;
  int sightRange;
  std::uint8_t numUnitsOfType;
  std::uint8_t bImproved;
  std::uint16_t field_1E;
  int field_20;
  int completedUpgradeTechIDs[2];
  // static_assert(sizeof(PerPlayerUnitStats) == 68);  // verify after completing fields
};

struct PerPlayerUnitStats___anon____anon_ {
  int moveSpeed;
  int moveSpeed;
  int powerRequired;
  int turnRate;
  int workersRequired;
  int concussionDamage;
  int productionRate;
  int penetrationDamage;
  int scientistsRequired;
  int storageCapacity;
  int reloadTime;
  int reloadTime;
  int weaponSightRange;
  int field_38;
  int productionCapacity;
  int field_3C;
  int cargoCapacity;
  int storageBays;
};

struct PerPlayerUnitStats___anon_ {
  } building;
  } vehicle;
  } weapon;
};

struct GlobalUnitStats___anon_ {
  std::uint16_t radius;
  std::uint16_t flags;
  std::uint16_t width;
  std::uint16_t height;
  std::uint16_t field_246;
  std::uint32_t pixelsSkipped;
  std::uint32_t flags;
  int edenAnimationIndex[10];
  int plyAnimationIndex[10];
  std::uint8_t resPriority;
  std::uint8_t rareRubble;
  std::uint8_t unk4;
  std::uint8_t commonRubble;
  std::uint8_t edenDockLocation;
  std::uint8_t plyDockLocation;
};

struct GlobalUnitStats {
  } building;
  } vehicle;
  } weapon;
};

struct MapObjectType {  // base: OP2Class<MapObjectType>
  MapID type_;
  PerPlayerUnitStats playerStats_[7];
  int requiredTechID_;
  TrackType trackType_;
  int field_1EC;
  SoundID selectSoundID_;
  std::uint32_t ownerFlags_;
  char unitName_[40];
  char produceName_[40];
  GlobalUnitStats stats_;
};

struct TruckLoadData {
  int field_00;
  int peakTruck;
  int minTruck;
};

struct MineManager {  // base: OP2Class<MineManager>
  } truckLoadInfo_[NumYields * NumVariants];
  } yieldPctInfo_[NumYields  * NumVariants];
};

struct YieldPercentData {
  int initialYield;
  int peakYield;
  int minYield;
};

struct AIModDesc {
  TethysAPI::ModDesc descBlock;
  char* pMapName;
  char* pLevelDesc;
  char* pTechtreeName;
  int checksum;
};

struct DefaultScGroupInfo {
  int defaultScGroupIndex;
  int isUserManaged;
};

struct MissionManager {  // base: OP2Class<MissionManager>
  HINSTANCE hModule_;
  TethysAPI::SaveRegion saveRegion_;
  char* pScriptName_;
  DefaultScGroupInfo defaultScGroupInfo_[6];
  AIModDesc* pDescBlock_;
};

struct MoraleModifier {
  char* pName;
  int field_04;
  int field_08;
};

struct MoraleManager {  // base: OP2Class<MoraleManager>
  int field_00;
};

struct LongWaypoint {
  std::uint32_t x : 15;
  std::uint32_t y : 14;
};

struct PathFinder {
  MapObject* pCurUnit_;
  int startTileX_;
  int startTileY_;
  int dstTileX_;
  int dstTileY_;
  int sqDistance_;
  int rangeTileX_;
  int rangeTileY_;
};

struct PathContext___anon_ {
  int numWaypoints;
  PathContext* pFreeListNext;
};

struct PathContext {  (size=512)
  int maxWaypointIndex;
  int currentWaypointIndex;
  int field_0C;
  int field_10;
  Waypoint waypoints[8];
  std::uint32_t flags;
  PathContext* pNextDestination;
  int field_3C;
  int field_40;
  std::uint16_t numPixelsMovedX;
  std::uint16_t numPixelsMovedY;
  int startPixelX;
  int startPixelY;
  int dstTileX;
  int dstTileY;
  std::uint16_t field_58;
  std::uint16_t numPixelsToMoveX;
  std::uint16_t field_5C;
  std::uint16_t numPixelsToMoveY;
  int rotationSpeed;
  int field_64;
  int field_68;
  int field_6C;
  int field_70;
  int numPathfinderPoints;
  int currentPathfinderPoint;
  std::uint8_t direction[64];
  int field_BC;
  int field_C0;
  int field_C4;
  int field_C8;
  int field_CC;
  int distance;
  std::uint8_t field_D4[0x200 - 0xD4];
  // static_assert(sizeof(PathContext) == 512);  // verify after completing fields
};

struct PathContextList {  // base: OP2Class<PathContextList>
  int field_00;
  PathContext* pBaseAllocAddr_;
  PathContext* pNextAllocAddr_;
  PathContext* pFreeListHead_;
};

struct BuildingStats {
  int numBuildings;
  int numActive;
  int numDisabled;
  int numIdle;
};

struct PlayerImpl {  // base: OP2Class<PlayerImpl>  (size=3108)
  int playerNum_;
  PlayerBitmask alliedBy_;
  SatelliteCounts satellites_;
  int foodStored_;
  int maxFood_;
  int maxCommonOre_;
  int maxRareOre_;
  int commonOre_;
  int rareOre_;
  int isHuman_;
  int isEden_;
  PlayerColor colorType_;
  MoraleState moraleState_;
  int scientistGrowthRemainder_;
  int workerGrowthRemainder_;
  int kidGrowthRemainder_;
  int kidDeathRemainder_;
  int scientistDeathRemainder_;
  int workerDeathRemainder_;
  int numWorkers_;
  int numScientists_;
  int numKids_;
  int recalculateValues_;
  int numAvailableWorkers_;
  int numAvailableScientists_;
  int amountPowerGenerated_;
  int inactivePowerCapacity_;
  int amountPowerConsumed_;
  int amountPowerAvailable_;
  int numIdleBuildings_;
  int numActiveBuildings_;
  int numBuildings_;
  int numUnpoweredStructures_;
  int numWorkersRequired_;
  int numScientistsRequired_;
  int numScientistsAsWorkers_;
  int numScientistsResearching_;
  int totalFoodProduction_;
  int totalFoodConsumption_;
  int foodLacking_;
  int netFoodProduction_;
  int numSolarSatellites_;
  int totalRecFacilityCapacity_;
  int totalForumCapacity_;
  int totalMedCenterCapacity_;
  int totalResidenceCapacity_;
  int numActiveCommandCenters_;
  int numActiveNurseries_;
  int numActiveUniversities_;
  int numActiveObservatories_;
  int numActiveMeteorDefenses_;
  int numActiveTradeCenters_;
  BuildingStats powerPlants_;
  BuildingStats agridomes_;
  BuildingStats commonOreSmelters_;
  BuildingStats commonOreStorage_;
  BuildingStats rareOreStorage_;
  BuildingStats rareOreSmelters_;
  BuildingStats gorfs_;
  BuildingStats commonOreMines_;
  BuildingStats rareOreMines_;
  BuildingStats robotCommandCenters_;
  int rccOperational_;
  PlayerEndInfo endInfo_;
  int field_1D8[5];
  int starvationCase_;
  PackedUnitGroup unitGroup_[11];
  std::uint8_t field_4BB;
  int playerNetID_;
  MachineSettings machineSettings_;
  CommandPacket commandPacketBuffer_[16];
  int unitIndex_[16];
  MapObject* pBuildingList_;
  MapObject* pVehicleList_;
  MapObject* pEntityList_;
  // static_assert(sizeof(PlayerImpl) == 3108);  // verify after completing fields
};

struct PlayerImpl___anon_ {
  DifficultyLevel difficulty_;
  ResourceLevel resourceLevel_;
};

struct Random {  // base: OP2Class<Random>
  std::uint64_t seed_;
};

struct TechProperty {
  TechUpgradeType type;
  char* pUpgradeType;
};

struct TechProperty___anon_ {
  size_t offsetToProp;
};

struct TechUpgradeInfo {
  TechProperty* pType;
  MapID unitType;
  int newValue;
};

struct TechInfo {  // base: OP2Class<TechInfo>
  int techID;
  TechCategory category;
  int techLevel;
  int plymouthCost;
  int edenCost;
  int maxScientists;
  TechLabType lab;
  PlayerBitmask playerHasTechMask;
  int numUpgrades;
  int numRequiredTechs;
  char* pTechName;
  char* pDescription;
  char* pTeaser;
  char* pImproveDesc;
  int* pRequiredTechNum;
  TechUpgradeInfo* pUpgrades;
  int numDependentTechs;
  int* pDependentTechNums;
};

struct Research {  // base: OP2Class<Research>
  int numTechs_;
  TechInfo** ppTechInfos_;
  int maxTechID_;
  int field_10;
  int field_14;
  int field_18;
  int field_1C;
  int field_20;
  int field_24;
};

struct ScStubList {  // base: OP2Class<ScStubList>
  std::uint32_t lastCreatedIndex_;
  ScBase* pScStubArray_[MaxNumScStubs];
};

struct ScStubFactory {
  ScStubFactory* pParent_;
  const char* pName_;
  std::uint32_t elementSizeInBytes_;
};

struct ScBase {  // base: OP2Class<ScBase>
  int index_;
  int isEnabled_;
  int field_0C;
  int field_10;
};

struct ScriptDataBlock {  // base: ScBase
  int field_14;
  int field_18;
  int useLevelModule_;
  char* pFuncName_;
};

struct FuncReference {  // base: ScriptDataBlock
  int field_28;
  int field_2C;
};

struct TriggerImpl {  // base: ScBase
  int field_14;
  TriggerImpl* pNext_;
  int isOneShot_;
  PlayerBitmask playerVectorHasFired_;
  FuncReference* pFuncRef_;
};

struct VictoryConditionImpl {  // base: TriggerImpl
  int field_24;
  int field_2C;
  const char* pObjectiveText_;
  int victoryTriggerIndex_;
};

struct UnitTypeTargetCount {
  MapID unitType;
  MapID weaponType;
  int targetCount;
  int field_0C;
};

struct TargetCount {  (size=20)
  int numAllocatedUnitTypeTargetCounts_;
  UnitTypeTargetCount* pUnitTypeTargetCounts_;
  int groupScStubIndex_;
  int numUnitTypeTargetCounts_;
  int field_10;
  // static_assert(sizeof(TargetCount) == 20);  // verify after completing fields
};

struct UnitNode___anon_ {
  int nextFreeIndex;
  UnitNode* pPrev;
};

struct UnitNode {
  UnitNode* pNext;
  MapObject* pUnit;
  int issueCommandTick;  // off~0xFFF00000
  UnitClassification classification;
};

struct ScGroupImpl {  // base: ScBase
  int field_14;
  int field_18;
  int lastFreeUnitNodeIndex_;
  int field_20;
  UnitNode unitNode_[32];
  TargetCount* pTargetCount_;
  UnitNode* pUnitListHead_;
  UnitNode* pUnitListTail_;
  int numUnits_;
  int ownerPlayerNum_;
  int deleteWhenEmptyTick_;
  int setLights_;
  int field_300;
  int field_304;
};

struct RecordedBuilding {
  MapRect buildingTileRect;
  MapID buildingType;
  MapID weaponType;
  int groupScStubIndex;
};

struct RecordedMine {
  MapRect mineRectInTiles;
  MapID mineType;
  int buildGroupScStubIndex;
  int minerUnitIndex;
};

struct RecordedTubeWall {
  std::uint16_t tileX;
  std::uint16_t tileY;
  CellType cellType;
  int field_08;
};

struct RecordedVehGroup {
  int targetGroupScStubIndex;
  int priority;
  int unitIndex;
};

struct RecordInfo {
  int numRecordedBuildings;
  int numRecordedMines;
  int numRecordedTubesWalls;
  int numRecordedVehGroup;
  RECT defaultRectInPixels;
  int field_348;
  int convecUnitIndex;
  int factoryUnitIndex;
  int recordedBuildingIndex;
  int factoryBayIndex;
};

struct BuildingGroupImpl {  // base: ScGroupImpl
  int numAllocatedRecordedBuildings_;
  RecordedBuilding* pRecordedBuildings_;
  int numAllocatedRecordedMines_;
  RecordedMine* pRecordedMines_;
  int numAllocatedRecordedTubesWalls_;
  RecordedTubeWall* pRecordedTubesWalls_;
  int numAllocatedRecordedVehGroups_;
  RecordedVehGroup* pRecordedVehGroups_;
  RecordInfo recordInfo_;
};

struct FightGroupImpl {  // base: ScGroupImpl  (size=1000)
  int combineFire_;
  int combineFireUnitIndex_;
  int field_310;  // off~0xFFF00000
  int numWaypoints_;
  int field_318;
  Waypoint waypointList_[8];
  int patrolMode_;
  int followMode_;
  RECT pixelRect_;
  int targetGroupIndex_;
  MapID attackType_;
  int targetUnitIndex_;
  int numGuardedRects_;
  RECT guardedRect_[8];
  int field_3E4;
  // static_assert(sizeof(FightGroupImpl) == 1000);  // verify after completing fields
};

struct MineGroupImpl {  // base: ScGroupImpl
  int mineUnitIndex_;
  int smelterUnitIndex_;
  int minePixelX_;
  int minePixelY_;
  int smelterPixelX_;
  int smelterPixelY_;
  MapID mineType_;
  MapID smelterType_;
  RECT mineGroupPixelRect_;
};

struct Sheet {  // base: OP2Class<Sheet>
  int field_00;
  int field_04;
};

struct GameVersion___anon_ {
  std::uint8_t stepping;
  std::uint8_t unused;
  std::uint8_t minor;
  std::uint8_t major;
};

struct GameVersion {
  std::uint32_t version;
};

struct TApp {  // base: OP2Class<TApp>  (size=396)
  HINSTANCE hInstance_;
  HINSTANCE hOut2ResLib_;
  int nShowCmd_;
  int bShowShell_;
  char* pSavedGameName_;
  int field_14;
  int playback_;
  int field_1C;
  char* pNetProtocolName_;
  MSG msg_;
  HHOOK hHelpHook_;
  TFrame* pMainWindow_;
  HINSTANCE hShellLib_;
  HWND hShellWnd_;
  GurManager* pGurManager_;
  TLobby* pNetProtocolManager_;
  NetTransportLayer* pNetTransportLayer_;
  char str_[MAX_PATH];
  int cursorWaitCount_;
  HCURSOR hOldCursor_;
  int canUseMMX_;
  int gameInitialized_;
  int isDrawing_;
  BOOL fActive_;
  int paused_;
  DWORD pDirectDrawCreate_;
  IDirectDraw* pDirectDraw_;
  HINSTANCE hDDrawLib_;
  DirectDrawWindow* pDirectDrawWindow_;
  // static_assert(sizeof(TApp) == 396);  // verify after completing fields
};

struct TubeConnectionManager {  // base: OP2Class<TubeConnectionManager>
  int field_00;
};

struct TubeConnection {  // base: OP2Class<TubeConnection>
  int field_00;
};

struct UnitGroup {  // base: OP2Class<UnitGroup>  (size=132)
  int numUnits_;
  int unitIndex_[32];
  // static_assert(sizeof(UnitGroup) == 132);  // verify after completing fields
};

struct PackedUnitGroup {  (size=65)
  std::uint8_t numUnits;
  std::uint16_t unitIndex[32];
  // static_assert(sizeof(PackedUnitGroup) == 65);  // verify after completing fields
};

struct PlayerGurInfo {
  int playerNetID;
  int b1[7];
  int timeOfLastReceivedPacket;
  int b2;
  int b3;
  int numResentPackets;
};

struct NetBuffer {
  NetBuffer* pPrev_;
  NetBuffer* pNext_;
  int b1[5];
  Packet packet_;
};

struct GurManager {  // base: OP2Class<GurManager>
  int field_04;
  NetBuffer* pCurBuffer_;
  int field_0C;
  NetBuffer* pHead_;
  NetBuffer* pTail_;
  NetBuffer netBuffer_[37];
  NetTransportLayer* pNetTransportLayer_;
  int field_52C8;
  int field_52CC;
  int numPlayers_;
  PlayerGurInfo playerInfo_[5];
  int numRetries_;
  int timeOut_;
  int timeOutD4_;
  int field_53D0;
  int field_53D4;
  int field_53D8;
  int field_53DC;
};

struct ProtocolControlMapping {
  int controlID;
  NetGameProtocol* pNetGameProtocol;
};

struct TCPGameSearchProtocol {  // base: NetGameSearchProtocol
  int field_04;
  int specifiedIP_;
  HANDLE hBroadcastThread_;
  int field_10;
  int field_14;
  int field_1C;
  NetGameSession* pSession_;
};

struct NetGameSession {  // base: OP2Class<NetGameSession>
  int field_04;
  int field_08;
  char* pHostName_;
  int field_10;
  int field_14;
  int field_18;
  int field_1C;
  int maxPlayers_;
};

struct TCPGameSession {  // base: NetGameSession  (size=68)
  int field_24;
  int hostAddress_;
  GUID sessionIdentifier_;
  int ping_;
  int pingDivisor_;
  // static_assert(sizeof(TCPGameSession) == 68);  // verify after completing fields
};

struct TrafficCounters {
  int timeOfLastReset;
  int numPacketsSent;
  int numBytesSent;
  int numPacketsReceived;
  int numBytesReceived;
};

struct NetTransportLayer {  // base: OP2Class<NetTransportLayer>
  int playerNetID_;
};

struct PacketHeader {
  int srcPlayerNetID;
  int dstPlayerNetID;
  std::uint8_t sizeOfPayload;
  std::uint8_t type;
  std::uint32_t checksum;
};

struct NetPeerInfo {
  int ip;
  std::uint16_t port;
  PeerStatus status;
  int playerNetID;
};

struct TransportLayerHeader {
  TransportLayerCommand commandType;
};

struct JoinRequest {  // base: TransportLayerHeader
  GUID sessionIdentifier;
  int returnPortNum;
  char password[12];
};

struct JoinReply {  // base: TransportLayerHeader
  GUID sessionIdentifier;
  int newPlayerNetID;
};

struct JoinReturned {  // base: TransportLayerHeader
  int newPlayerNetID;
  int unused1;
  int unused2;
  int unused3;
};

struct PlayersList {  // base: TransportLayerHeader
  int numPlayers;
  NetPeerInfo netPeerInfo[6];
};

struct StatusUpdate {  // base: TransportLayerHeader
  PeerStatus newStatus;
};

struct CreateGameInfo {
  StartupFlags startupFlags;
  char gameCreatorName[15];
};

struct HostedGameSearchQuery {  // base: TransportLayerHeader
  GUID gameIdentifier;
  std::uint32_t timeStamp;
  char password[12];
};

struct HostedGameSearchReply {  // base: TransportLayerHeader
  GUID gameIdentifier;
  std::uint32_t timeStamp;
  GUID sessionIdentifier;
  CreateGameInfo createGameInfo;
  sockaddr_in hostAddress;
};

struct GameServerPoke {  // base: TransportLayerHeader
  PokeStatusCode statusCode;
  int randValue;
};

struct JoinHelpRequest {  // base: TransportLayerHeader
  GUID sessionIdentifier;
  int returnPortNum;
  char password[12];
  sockaddr_in clientAddr;
};

struct RequestExternalAddress {  // base: TransportLayerHeader
  std::uint16_t internalPort;
};

struct EchoExternalAddress {  // base: TransportLayerHeader
  std::uint16_t replyPort;
  sockaddr_in addr;
};

struct TransportLayerMessage {
  TransportLayerHeader tlHeader;
  JoinRequest joinRequest;
  JoinReply joinReply;
  JoinReturned joinReturned;
  PlayersList playersList;
  StatusUpdate statusUpdate;
  HostedGameSearchQuery searchQuery;
  HostedGameSearchReply searchReply;
  GameServerPoke gameServerPoke;
  JoinHelpRequest joinHelpRequest;
  RequestExternalAddress requestExternalAddress;
  EchoExternalAddress echoExternalAddress;
};

struct GameMessage {
  GameMessageHeader gmHeader;
};

struct Packet {  // base: OP2Class<Packet>
  PacketHeader header;
};

struct Packet___anon_ {
  std::uint8_t payloadData[0x212];
  TransportLayerMessage tlMessage;
  GameMessage gameMessage;
};

struct CConfig {  // base: OP2Class<CConfig>  (size=260)
  char iniPath_[MAX_PATH];
  // static_assert(sizeof(CConfig) == 260);  // verify after completing fields
};

struct AdaptiveHuffmanTree {  // base: OP2Class<AdaptiveHuffmanTree>  (size=22)
  StreamIO* pStream_;
  uint16* pCount;
  uint16* field_08;
  uint16* pTree_;
  std::uint16_t readWordBitBuffer_;
  std::uint8_t numBitsAvailable_;
  std::uint16_t writeWordBitBuffer_;
  std::uint8_t numBitsPending_;
  // static_assert(sizeof(AdaptiveHuffmanTree) == 22);  // verify after completing fields
};

struct LZHRStream {  // base: StreamIO  (size=44)
  StreamIO* pWrappedStream_;
  AdaptiveHuffmanTree* pHuffmanTree_;
  uint8* ringBuffer_;
  int matchIndex_;
  int numCharsInRun_;
  int numCharsCopied_;
  int writeIndex_;
  size_t streamPosition_;
  // static_assert(sizeof(LZHRStream) == 44);  // verify after completing fields
};

struct LZHWStream {  // base: StreamIO  (size=72)
  StreamIO* pWrappedStream_;
  AdaptiveHuffmanTree* pHuffmanTree_;
  int field_14;
  int writeIndex_;
  int field_1C;
  int field_20;
  int field_24;
  int field_28;
  int runOffset_;
  int runLength_;
  size_t streamPosition_;
  uint8* ringBuffer_;
  uint8* field_3C;
  uint8* field_40;
  uint8* field_44;
  // static_assert(sizeof(LZHWStream) == 72);  // verify after completing fields
};

struct LZRStream {  // base: StreamIO  (size=48)
  StreamIO* pWrappedStream_;
  size_t streamPosition_;
  uint8* pBuffer_;
  int writeIndex_;
  std::uint8_t currentByte_;
  std::uint8_t field_1D;
  std::uint16_t currentBitMask_;  // off~0x80
  int numCharsRepeated_;
  int repeating_;
  int repeatIndex_;
  int repeatedRunLength_;
  // static_assert(sizeof(LZRStream) == 48);  // verify after completing fields
};

struct LZWStream {  // base: StreamIO  (size=40)
  StreamIO* pWrappedStream_;
  size_t streamPosition_;
  uint8* pBuffer_;
  uint8* field_18;
  int field_1C;
  int field_20;
  std::uint8_t field_24;
  std::uint8_t field_25;
  short field_26;
  // static_assert(sizeof(LZWStream) == 40);  // verify after completing fields
};

struct RLERStream {  // base: StreamIO  (size=152)
  StreamIO* wrappedStream_;
  std::uint8_t charToRepeat_;
  std::uint8_t numTimesToRepeat_;
  std::uint8_t chunkSize_;
  std::uint8_t field_13[0x7B];
  size_t streamPosition_;
  std::uint16_t field_96;
  // static_assert(sizeof(RLERStream) == 152);  // verify after completing fields
};

struct RLEWStream {  // base: StreamIO  (size=152)
  StreamIO* pWrappedStream_;
  std::uint8_t charToRepeat_;
  std::uint8_t numTimesToRepeat_;
  std::uint8_t chunkSize_;
  std::uint8_t field_13[0x7B];
  size_t streamPosition_;
  std::uint16_t field_96;
  // static_assert(sizeof(RLEWStream) == 152);  // verify after completing fields
};

struct GlyphMetrics {  (size=28)
  POINT gmptGlyphOrigin;
  int gmCellIncX;
  int gmCellIncY;
  UINT gmBlackBoxX;
  UINT gmBlackBoxY;
  int bufferIndex;
  // static_assert(sizeof(GlyphMetrics) == 28);  // verify after completing fields
};

struct Font {  // base: FontBase  (size=7264)
  LOGFONT createInfo_;
  int field_40;
  int tmHeight_;
  int tmAscent_;
  int tmDescent_;
  int tmInternalLeading_;
  int tmExternalLeading_;
  int tmMaxCharWidth_;
  GlyphMetrics glyphMetrics_[256];
  uint8* pCharacterImageBuffer_;
  // static_assert(sizeof(Font) == 7264);  // verify after completing fields
};

struct RenderChunk {
  int xOffset;
  int stringStart;
  int stringLen;
  int isEOL;
  COLORREF color;
};

struct RenderDataBase {
  int numChunks;
  int numLines;
  int numChunksUsed;
};

struct RenderData {  // base: RenderDataBase
  RenderChunk renderChunk[N];
};

struct GFXBitmap {  // base: OP2Class<GFXBitmap>
  int width_;
  int height_;
  int pitch_;
  int bpp_;
  int imageBufferSize_;
  int attributes_;
};

struct GFXLightAdjustedBitmap {  // base: GFXBitmap
  int numLightLevels_;
  int field_24;
  int drawMethod_;
  std::uint8_t mappedPlayerColorIndex_[NumLightLevels];
  int field_50;
  int field_D4;
  int field_D8;
};

struct GFXTilesetBitmap {  // base: GFXLightAdjustedBitmap
  int numTiles_;
  int numTilesRequested_;
  int field_E4;
  int field_E8;
  int tileHeight_;
  int tilesetHeight_;
  int bytesPerTile_;
  uint8* pPixelData_;
};

struct GFXSpriteBitmap {  // base: GFXLightAdjustedBitmap
  uint8* pBmpFileData_;
  int fileSize_;
  uint8* pPixelData_;
  int field_F0;
  int field_F4;
  int field_F8;
};

struct Rgb555___anon_ {
  std::uint16_t b : 5;
  std::uint16_t g : 5;
  std::uint16_t r : 5;
  std::uint16_t a : 1;
};

struct Rgb555 {
  std::uint16_t u16All;
};

struct GFXPalette {  // base: OP2Class<GFXPalette>  (size=1108)
  int version_;
  int numColors_;
  int colors_[256];
  int field_404;
  int field_408;
  int field_40C;
  int field_410;
  int field_414;
  int field_418;
  int field_41C;
  int field_420;
  int field_424;
  int field_428;
  int field_42C;
  int field_430;
  int field_434;
  int field_438;
  int field_43C;
  int field_440;
  int field_444;
  int field_448;
  int field_44C;
  // static_assert(sizeof(GFXPalette) == 1108);  // verify after completing fields
};

struct GFXSurface {  // base: OP2Class<GFXSurface>  (size=120)
  int field_04;
  int field_08;
  int field_0C;
  int maxX_;
  int maxY_;
  int field_18;
  int field_1C;
  int field_20;
  int field_24;
  int field_28;
  int field_2C;
  int field_30;
  int pitch_;
  int width_;
  int height_;
  int bpp_;
  int field_48;
  int field_4C;
  int field_50;
  int field_54;
  int field_58;
  int surfaceType_;
  int field_60;
  int lockCount_;
  int field_68;
  HDC hLockedDC_;
  int field_70;
  int field_74;
  // static_assert(sizeof(GFXSurface) == 120);  // verify after completing fields
};

struct GFXCDSSurface {  // base: GFXSurface  (size=152)
  HBITMAP hDibSection_;
  HDC hWindowDC_;
  HDC hMemoryDC_;
  HPALETTE hPalette_;
  HPALETTE hOldPalette_;
  HBITMAP hOldDibSection_;
  HWND hDstWnd_;
  // static_assert(sizeof(GFXCDSSurface) == 152);  // verify after completing fields
};

struct GFXMemSurface {  // base: GFXSurface  (size=128)
  int isBitmapOwned_;
  // static_assert(sizeof(GFXMemSurface) == 128);  // verify after completing fields
};

struct Viewport {  // base: OP2Class<Viewport>  (size=2124)
  int drawingEnabled_;
  int redrawBitVectorLineWidth_;
  std::uint32_t redrawBitVectorSize_;
  int scrollY_;
  int scrollX_;
  int height_;
  int width_;
  int field_1C;
  int lightUpdateTileX_;
  RECT relativeTileRect_;
  std::uint32_t redrawBitVector_[100];
  std::uint32_t oldRedrawBitVector_[100];
  std::uint32_t unknownBitVector_[100];
  std::uint32_t lightBitVector_[100];
  std::uint32_t oldLightBitVector_[100];
  int totalBackgroundRedraw_;
  int maxTileX_;
  int maxTileY_;
  int oldPixelX_;
  int oldPixelY_;
  int pixelX_;
  int pixelY_;
  int tileStartPixelOffsetX_;
  int tileStartPixelOffsetY_;
  int tileX_;
  int tileY_;
  int field_848;
  // static_assert(sizeof(Viewport) == 2124);  // verify after completing fields
};

struct GFXClippedSurface {  // base: GFXCDSSurface  (size=2396)
  int field_98;
  int field_9C;
  int field_A0;
  int field_A4;
  int field_A8;
  int field_AC;
  RECT drawRect_;
  int screenRequiresUpdate_;
  int markBackgroundOnRedraw_;
  BITMAPINFO bitmapInfo_;
  BITMAPINFO* pBitmapInfo_;
  RECT redrawRect_;
  int field_108;
  int zoom_;
  Viewport viewport_;
  // static_assert(sizeof(GFXClippedSurface) == 2396);  // verify after completing fields
};

struct BitmapCopyInfo {
  int overlayBitOffset;
  int srcWidth;
  int srcHeight;
  int srcPitch;
  int dstPitch;
  int drawMethod;
  uint16* pDarkPal16;
  int blightOverlay;
  uint16* pLightPal16;
};

struct ScanlineCopyInfo {
  uint16* pPalette;
  int field_08;
  int width;
};

struct MemoryMappedFile {  // base: OP2Class<MemoryMappedFile>
  HANDLE hFile_;
  size_t size_;
  HANDLE hObject_;
  HANDLE hFileMapping_;
  int field_14;
};

struct WplInfo {
  std::uint32_t structSize;
  std::uint32_t structSize;
  std::uint32_t field_04;
  std::uint32_t field_04;
  HINSTANCE hAppInst;
  HINSTANCE hAppInst;
  HINSTANCE hResDllInst;
  HINSTANCE hResDllInst;
  std::uint32_t startResId;
  std::uint32_t startResId;
  std::uint32_t field_14;
  std::uint32_t field_14;
  std::uint32_t field_18;
  std::uint32_t field_18;
  std::uint32_t field_1C;
  std::uint32_t field_1C;
  std::uint32_t field_20[7];
  std::uint32_t field_20[7];
};

struct ResManager {  // base: OP2Class<ResManager>
  char installedDir_[MAX_PATH];
  char cdDir_[MAX_PATH];
};

struct SoundBufferInfo {  (size=36)
  SoundID soundID;
  int field_04;
  int pixelY;
  int pixelX;
  int endTime;
  DirectSoundBuffer* pDirectSoundBuffer;
  int volume;
  int pan;
  int field_20;
  // static_assert(sizeof(SoundBufferInfo) == 36);  // verify after completing fields
};

struct MusicManager {  // base: OP2Class<MusicManager>
  DirectSoundBuffer* pDirectSoundBuffer_;
  int timerID_;
  int playing_;
  int field_0C;
  int volumeIndex_;
  int pauseLockCount_;
  int playMusic_;
  HANDLE hClmFile_;
  int totalFileHeaderSize_;
  CRITICAL_SECTION timerCriticalSection_;
  int field_A8;
  CRITICAL_SECTION criticalSection_;
  int numPlaylistEntries_;
  int repeatStartIndex_;
  SongID* pPlaylist_;
  int currentPlayingSongIndex_;
  int hasBegunPlayback_;
  int currentSongFileIndex_;
  int currentSongPosition_;
};

struct ImageInfo {
  int scanlineByteWidth;
  int dataOffset;
  int height;
  int width;
  std::uint16_t typeFlags;
  std::uint16_t paletteIndex;
};

struct FrameComponentInfo {
  std::uint16_t pixelXOffset;
  std::uint16_t pixelYOffset;
  std::uint16_t imageIndex;
};

struct FrameOptionalInfo {
  std::int8_t offsetX;
  std::int8_t offsetY;
  std::int8_t offsetX2;
  std::int8_t offsetY2;
};

struct FrameInfo___anon_ {
  std::uint16_t left;  // off~0x7FFE
  std::uint16_t top;
  std::uint16_t right;
  std::uint16_t bottom;
};

struct FrameInfo {  (size=14)
  } rect;
  std::uint16_t numFrameComponents;
  FrameComponentInfo* pFrameComponent;
  // static_assert(sizeof(FrameInfo) == 14);  // verify after completing fields
};

struct AnimationInfo {  (size=112)
  int field_00;
  RECT selectionBox;
  int pixelXDisplacement;
  int pixelYDisplacement;
  int field_1C;
  int numFrames;
  FrameInfo* pFrameInfo;
  std::uint16_t frameOptionalInfoStartIndex;
  std::uint8_t padding[112 - 42];
  // static_assert(sizeof(AnimationInfo) == 112);  // verify after completing fields
};

struct SpriteManager {  // base: OP2Class<SpriteManager>
  uint8* pPaletteData_;
  int paletteSize_;
  ImageInfo imageInfo_[MaxNumImages];
  int numImages_;
  AnimationInfo animationInfo_[MaxNumAnimations];
  int numAnimations_;
  GFXSpriteBitmap* pBitmap_;
  FrameInfo** ppFrameInfo_;
  FrameComponentInfo** ppFrameComponentInfo_;
  FrameOptionalInfo** ppFrameOptionalInfo_;
};

struct StreamIO {  // base: OP2Class<StreamIO>
  size_t position_;
  int status_;
};

struct FileRStream {  // base: StreamIO  (size=2080)
  std::uint8_t buffer_[2048];
  size_t positionOfLastReadChunkStart_;
  size_t positionOfLastReadChunkEnd_;
  size_t streamPosition_;
  HANDLE hFile_;
  int ownsFile_;
  // static_assert(sizeof(FileRStream) == 2080);  // verify after completing fields
};

struct FileWStream {  // base: StreamIO  (size=21)
  HANDLE hFile_;
  int ownsFile_;
  std::uint8_t field_14;
  // static_assert(sizeof(FileWStream) == 21);  // verify after completing fields
};

struct FileRWStream {  // base: StreamIO  (size=20)
  HANDLE hFile_;
  int ownsFile_;
  // static_assert(sizeof(FileRWStream) == 20);  // verify after completing fields
};

struct MemRWStream {  // base: StreamIO  (size=24)
  uint8* begin_;
  uint8* end_;
  uint8* currentPos_;
  // static_assert(sizeof(MemRWStream) == 24);  // verify after completing fields
};

struct TextStream {  // base: OP2Class<TextStream>
  StreamIO* pStream_;
  int position_;
};

struct SheetParser {  // base: OP2Class<SheetParser>  (size=544)
  StreamIO* pStream_;
  int opened_;
  int field_08;
  int curField_;
  uint8* pEOL_;
  std::uint8_t delimiter_;
  int numFields_;
  std::uint8_t buffer[512];
  uint8* pBuffer_;
  // static_assert(sizeof(SheetParser) == 544);  // verify after completing fields
};

struct VolIndexEntry {
  std::uint32_t fileNameOffset;
  std::uint32_t dataOffset;
  std::uint32_t dataLength;
  CompressionCode compressionCode;
  std::uint8_t bUsed;
};

struct VBlkHeader {
  std::uint32_t tag;
  std::uint32_t size : 31;
  std::uint32_t alignment : 1;
};

struct BaseVBlkRWStream {  // base: StreamIO  (size=32)
  StreamIO* pContainerStream_;
  VBlkHeader header_;
  size_t positionOffset_;
  size_t startOffset_;
  // static_assert(sizeof(BaseVBlkRWStream) == 32);  // verify after completing fields
};

struct BaseVolFileStream {  // base: StreamIO  (size=348)
  int numIndexEntries_;
  VolIndexEntry* pIndexTable_;
  HANDLE hFileMapping_;
  HANDLE hFile_;
  size_t fileSize_;
  StreamIO* pOpenedInternalStream_;
  MemRWStream memRWStream_;
  VBlkRWStream vBlkRWStream_;
  RLERStream rleRStream_;
  LZHRStream lzhRStream_;
  LZRStream lzRStream_;
  int setFileSize_;
  // static_assert(sizeof(BaseVolFileStream) == 348);  // verify after completing fields
};

struct VolFileRStream {  // base: BaseVolFileStream  (size=2432)
  uint8* field_15C;
  FileRStream fileStream_;
  // static_assert(sizeof(VolFileRStream) == 2432);  // verify after completing fields
};

struct VolFileWStream {  // base: BaseVolFileStream
  int field_15C;
  int field_160;
  int field_164;
  int field_168;
  FileRWStream fileRWStream_;
  RLEWStream rleWStream_;
  LZHWStream lzhWStream_;
  LZWStream lzWStream_;  // off~0x260
};

struct CommandViewButton {  // base: UIGraphicalButton
  CommandPaneView* pView_;
  int deselectUnits_;
};

struct CommandPane {  // base: TPane  (size=1176)
  CommandViewButton reportButtons_[6];
  RECT redrawRect_;
  int field_484;
  int controlNum_;
  int selectedControl_;
  // static_assert(sizeof(CommandPane) == 1176);  // verify after completing fields
};

struct MessageLogEntry {
  int timestamp;
  int pixelX;
  int pixelY;
  char message[MessageLen];
};

struct MessageLog {  // base: OP2Class<MessageLog>
  int rbBegin_;
  int numRbElements_;
  MessageLogEntry<64> entryRb_[64];
  DWORD timestamps_[10];
  std::uint8_t field_1330[800];
};

struct BayButton {  // base: UIGraphicalButton
  int bayNum_;
};

struct ReportPageButton {  // base: CommandViewButton
  ReportPage reportPageIndex_;
};

struct DetailPane {  // base: TPane
  HDC hDstDC_;
  int field_70[15];
  char largeMessage_[132];
  HFONT hLargeMessageFont_;
  int field_134;
  int field_138;
  int field_13C;
  RECT viewPosition_;
};

struct MapObjDrawList {  // base: OP2Class<MapObjDrawList>
  Viewport* pViewport_;
  int field_04;
  int numUnits_;
  int numEntities_;
  MapObject* pUnitDrawList_[MaxPerGroup];
  MapObject* pEntityDrawList_[MaxPerGroup];
};

struct TFileDialog {  // base: IDlgWnd
  int field_20;
  int field_24;
  int field_28;
};

struct SaveGameDialog {  // base: TFileDialog  (size=69536)
  int field_2C[17373];
  // static_assert(sizeof(SaveGameDialog) == 69536);  // verify after completing fields
};

struct LoadGameDialog {  // base: TFileDialog  (size=69540)
  int field_2C[17374];
  // static_assert(sizeof(LoadGameDialog) == 69540);  // verify after completing fields
};

struct FilterNode {
  FilterNode* pPrev;
  FilterNode* pNext;
  Filter* pFilter;
  int data;
  FilterOptions options;
};

struct GroupFilter {  // base: Filter
  SubFilter* pSubFilter_;
};

struct MouseCommandFilter___anon____anon_ {
  POINT mapMousePos_;
  int overlayPixelWidth_;
  int overlayPixelHeight_;
};

struct MouseCommandFilter___anon_ {
  RECT pixelRegion_;
};

struct MouseCommandFilter {  // base: SubFilter
  int mouseOverUnitIndex_;
  int mouseOverTick_;
  int field_1C;
  int field_20;
  BehaviorType behaviorType_;
  IWnd* pCaptureWnd_;
  MouseCommand* pCommand_;
  int field_30;
  int maxNumWaypoints_;
  int field_38;
  int field_3C;
};

struct HotKeyFilter {  // base: Filter
  UIElement* pHotKeyTable_[256];
  int pauseCount_;
};

struct IniSettings {
  int scrollRate;
  int zoom;
  int showAmbientAnimations;
  int showComputerOverlay;
  int showShadows;
  int showStationaryShadows;
  int showMobileShadows;
  int showCompletedObjectives;
  int field_18;
  int frameSkip;
};

struct StatusBar {  // base: OP2Class<StatusBar>  (size=176)
  int field_00;
  int maxCharWidth_;
  int fontHeight_;
  char message[4];
  int field_0C[24];
  int gameTick_;
  int isTypingChat_;
  size_t strLen_;
  RECT rect_;
  int lastDrawWidth_;
  int field_94[4];
  HFONT hFont_;
  HBRUSH hBrushWhite;
  HBRUSH hBrushRed;
  // static_assert(sizeof(StatusBar) == 176);  // verify after completing fields
};

struct UIFrameImages {
  T upperLeftCorner;
  T leftEdgeFull;
  T titleBarBlankArea;
  T upperRightCorner;
  T middleSplitterVertical;
  T rightEdgeFull;
  T lowerLeftCorner;
  T bottomEdge;
  T vertSplitterPart;
  T lowerRightCorner;
  T rightEdge;
  T leftEdge;
  T horzSplitterTop;
  T titleActive;
  T titleInactive;
  T minimizeNormal;
  T minimizePressed;
  T maximizeNormal;
  T maximizePressed;
  T closeNormal;
  T closePressed;
  T restoreNormal;
  T restorePressed;
  T commonOreIndicator;
  T rareOreIndicator;
};

struct GameFrame {  // base: TFrame
  HFONT hChatFont_;
  char chatMessage_[80];
  int chatLength_;
  COLORREF chatColor_;
  RECT chatRect_;
  int field_80;
  int field_84;
  int field_88;
  int field_8C;
  int field_90;
  int textY_;
  int field_98;
  int field_9C;
  int displayedCommonOre_;
  int displayedRareOre_;
  int field_A8;
  int field_AC;
  LARGE_INTEGER performanceFreq_;
  int lastProcessingLoopMs_;
  int desiredMsPerGameTick_;
  int msPerGameTick_;
  int estimatedPacketLag_;
  int lastNetExchangeMs_;
  int numNetExchanges_;
  LARGE_INTEGER performanceCounts_[16];
  int performanceCountIndex_;
  int field_154;
  LARGE_INTEGER performanceCounts2_[16];
  int performanceCountIndex2_;
  int field_1DC;
  int gamePausedCount_;
  HDC hMemDC_;
  HACCEL hViewAccel_;
  HACCEL hUnitAccel_;
  HACCEL hPauseAccel_;
  DetailPane detailPane_;
  MiniMapPane miniMapPane_;
  CommandPane commandPane_;
  StatusBar statusBar_;
  IniSettings iniSettings_;
  Font* pFont1_;
};

struct IWnd {  // base: OP2Class<IWnd>
  int isNotWindowOwner_;
  HWND hWnd_;
  FilterNode* pTailFilterNode_;
  FilterNode* pHeadFilterNode_;
};

struct IDlgWnd {  // base: IWnd
  IDlgWnd* pPrev_;
  IDlgWnd* pNext_;
  int flags_;
};

struct TPane {  // base: IWnd
  GFXSurface* pGfxSurface_;
  int numControls_;
};

struct MiniMap {  // base: OP2Class<MiniMap>
  int field_00[11];
  uint16* pUpdatedPixel_[1023];
  int numUpdatedPixels_;
  RECT viewRectNew_;
  RECT viewRectOld_;
  int invertMapColors_;
  int maxZoomOut_;
  int zoom_;
  int field_1058[2];
  std::uint16_t playerColor_[8];
  int field_1070[4];
  GFXClippedSurface* pSurface_;
  std::uint32_t flags_;
  int globeView_;
  POINT miniMapClickPos_;
  POINT miniMapScrollPos_;
  int field_109C;
  GFXBitmap* pMiniMapBackgroud_;
  GFXBitmap* pSurfaceBackBuffer_;
  GFXClippedSurface* pSurface2_;
  uint16** ppBrightness_;
  int field_10B0;
};

struct MiniMapPane {  // base: TPane  (size=788)
  int field_6C;
  int field_70;
  int field_74;
  int field_78;
  int field_7C;
  MiniMapButton buttons_[4];
  int buttonPosition_;
  // static_assert(sizeof(MiniMapPane) == 788);  // verify after completing fields
};

struct MsgBoxDlg {  // base: IDlgWnd
  int field_20;
  int field_24;
  int field_28;
  int field_2C;
  int field_30;
  int field_34;
  int field_38;
  HFONT hFont1_;
  HFONT hFont2_;
  HFONT hFont3_;
  int field_48;
  int field_4C;
  int field_50;
  int field_54;
  int field_58;
  int field_5C;
  int field_60;
  int field_64;
  int field_68;
  int field_6C;
  int field_70;
  int field_74;
  char title_[256];
  char message_[256];
  char buttonText_[256];
};

struct HostGameParameters {
  StartupFlags startupFlags;
  int unused[2];
  char gameCreatorName[13];
};

struct PlayerControls {
  HWND hPlayerNameTextBoxWnd;
  HWND hColorComboBoxWnd;
  HWND hColonyCheckBoxWnd;
  HWND hResourceComboBoxWnd;
  HWND hReadyCheckBoxWnd;
  HWND hEjectButtonWnd;
};

struct MultiplayerLobbyDialog {  // base: IDlgWnd
  UINT timerID_;
  HWND hEnterChatTextBoxWnd_;
  WNDPROC lpPrevWndFunc_;
  HWND hMessageTextBoxWnd_;
  HWND hLevelComboBoxWnd_;
  HWND hStartGameButtonWnd_;
  PlayerControls playerControls_[6];
  GameStartInfo gameStartInfo_;
  std::uint16_t field_182;
  int field_184[3];
  int purgeDroppedPlayers_;
  int needRedraw_;
  int gameStarting_;
  int time_;
  int hostPlayerNetID_;
  TethysAPI::MissionType maxMissionType_;
  TethysAPI::MissionType minMissionType_;
};

struct UIElement {  // base: OP2Class<UIElement>  (size=24)
  int flags_;
  RECT position_;
  // static_assert(sizeof(UIElement) == 24);  // verify after completing fields
};

struct UIElementFilter {  // base: Filter
  UIElement* pControl_;
};

struct UIButton {  // base: UIElement  (size=28)
  int hotkey_;
  // static_assert(sizeof(UIButton) == 28);  // verify after completing fields
};

struct ButtonDisplayInfo {
  int animationIndex;
  std::uint16_t normalFrameIndex;
  std::uint16_t activeFrameIndex;
  std::uint16_t disabledFrameIndex;
  std::uint16_t unknownFrameIndex;
  char* pHelpText;
  char* pButtonText;
  Font* pFont;
  std::uint16_t b1;
  std::uint16_t b2;
  std::uint16_t b3;
  std::uint16_t padding;
};

struct UIGraphicalButton {  // base: UIButton  (size=164)
  int buttonTextStringLength_;
  std::uint16_t field_20;
  std::uint16_t field_22;
  int field_24;
  int field_28;
  std::uint8_t field_2C[88];
  ButtonDisplayInfo buttonDisplayInfo_;
  // static_assert(sizeof(UIGraphicalButton) == 164);  // verify after completing fields
};

struct UICommandButton {  // base: UIGraphicalButton  (size=172)
  int commandParam_;
  UICommand* pCommand_;
  // static_assert(sizeof(UICommandButton) == 172);  // verify after completing fields
};

struct ListItem {
  int listItemIndex;
  char text[120];
};

struct ListStyle {
  ListData* pListData;
  Font* pFont;
  int selectedBorderWidth;
  std::uint16_t field_0C;
  std::uint16_t backColor16;
  int field_10;
};

struct UIListBox {  // base: UIElement
  int numLinesVisible_;
  int lineHeight_;
  int numLinesDisplayed_;
  int numItems_;
  ListItem* pListItems_;
  int scrollFirstListIndex_;
  int scrollLastListIndex_;
  std::uint16_t field_34;
  std::uint16_t field_36;
  int currentIndex_;
  int lastClickTime_;
  int lastClickedOnIndex_;
  ListStyle listStyle_;
  ListData* pListData_;
  Font* pFont_;
  int selectedBorderWidth_;
  std::uint16_t field_50;
  std::uint16_t backColor16_;
  int field_54;
};

struct UIState {  // base: OP2Class<UIState>
  std::uint32_t controlID_;
};

struct MenuState {  // base: UIState
  HMENU hMenu_;
};

} // namespace op2::abi::raw
