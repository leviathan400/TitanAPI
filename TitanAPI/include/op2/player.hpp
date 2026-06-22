#pragma once
// op2/player.hpp - Player: a value handle for a player slot, wrapping the engine's PlayerImpl.
//
// Setup writes live PlayerImpl state directly and calls a couple of engine members. The PlayerImpl array base
// is read from a hardcoded immediate in OP2 code at 0x4890C3 (each PlayerImpl is 3108 bytes; player[n] =
// base + n*3108). Field offsets were verified by offsetof against the engine struct (sizeof == 3108).
// Members: PlayerImpl::GoHuman() @0x4906C0 (thiscall); Research::SetTechLevel(num, level*1000) @0x473030 on
// the Research singleton @0x56C230. (All proven in-game by the Layer-1 sample.)

#include "op2/core/error.hpp"
#include "op2/core/location.hpp"
#include "op2/abi/memory.hpp"
#include "op2/abi/raw/command.hpp"
#include <vector>

namespace op2 {

class Unit;   // forward decl: Player::units() returns Units, but unit.hpp includes player.hpp (cycle), so the
              // definition lives out-of-line in game.hpp where both Player and Unit are complete.

class Player {
public:
  static constexpr int kMaxPlayers = 7;

  constexpr explicit Player(int index) : idx_{ index } {}

  [[nodiscard]] constexpr int  index() const noexcept { return idx_; }
  [[nodiscard]] constexpr bool valid() const noexcept { return idx_ >= 0 && idx_ < kMaxPlayers; }

  // ---- colony setup ----
  // Each writes live engine state and returns *this, so setup reads cleanly and chains:
  //     Game::player(0).goEden().setCommonOre(5000).setWorkers(20).setScientists(10);
  // Setup can only "fail" on an invalid player index (a programmer error), so there's no Result to check - unlike
  // the orders in unit.hpp. An invalid player is a safe no-op.
  Player& goEden()     { setFaction(true);  return *this; }  ///< faction = Eden
  Player& goPlymouth() { setFaction(false); return *this; }  ///< faction = Plymouth
  Player& goHuman();                                         ///< mark this a human-controlled colony
  Player& goAI();                                            ///< mark this a computer-controlled (AI) player
  Player& setPopulation(int workers, int scientists, int kids);
  Player& setCommonOre(int amount, int capacity = 0);        ///< capacity 0 = leave as-is (needs storage to persist)
  Player& setFood(int amount, int capacity = 0);
  Player& setRareOre(int amount);                            ///< set the rare-ore pool (needs storage to persist, like ore)
  Player& setWorkers(int n);                                 ///< set the worker pool (OP2Lua player.workers setter)
  Player& setScientists(int n);                              ///< set the scientist pool
  Player& setKids(int n);                                    ///< set the children pool
  Player& setTechLevel(int techLevel);                       ///< grants all techs up to techLevel

  // ---- runtime reads (parity with OP2Lua player.*) - live PlayerImpl fields ----
  [[nodiscard]] int  food()       const { return readi(OFF_foodStored);   }  ///< stored food
  [[nodiscard]] int  commonOre()  const { return readi(OFF_commonOre);    }  ///< stored common metals
  [[nodiscard]] int  rareOre()    const { return readi(OFF_rareOre);      }  ///< stored rare metals
  [[nodiscard]] int  workers()    const { return readi(OFF_numWorkers);   }  ///< number of workers
  [[nodiscard]] int  scientists() const { return readi(OFF_numScientists);}  ///< number of scientists
  [[nodiscard]] int  kids()       const { return readi(OFF_numKids);      }  ///< number of children
  [[nodiscard]] int  population() const { return workers() + scientists() + kids(); }  ///< total colonists
  [[nodiscard]] bool isEden()     const { return readi(OFF_isEden)  != 0; }  ///< this player is the Eden faction
  [[nodiscard]] bool isPlymouth() const { return valid() && readi(OFF_isEden) == 0; }  ///< this player is Plymouth
  [[nodiscard]] bool isHuman()    const { return readi(OFF_isHuman) != 0; }  ///< human-controlled
  [[nodiscard]] bool isAI()       const { return valid() && readi(OFF_isHuman) == 0; }  ///< computer-controlled

  /// The difficulty level chosen in the Colony Games menu (a per-player setting). Lets a mission tune itself -
  /// e.g. AI resources or disaster timing. PlayerImpl::difficulty_ @+12 (probe-verified). Mirrors OP2Lua
  /// player.difficulty.
  enum class Difficulty : int { Easy = 0, Normal = 1, Hard = 2 };
  [[nodiscard]] Difficulty difficulty() const { return Difficulty(readi(OFF_difficulty)); }

  // ---- technology ----
  [[nodiscard]] bool hasTechnology(int techID) const {                      ///< PlayerImpl::HasTechnology @0x477570
    char* p = impl(); return p && (abi::member<0x477570, int>(p, techID) != 0);
  }
  void markResearchComplete(int techID) { if (char* p = impl()) abi::member<0x477510, void>(p, techID); }
  /// Recompute this player's cached CAPABILITY flags. The engine caches the results of canBuildSpace /
  /// canDoResearch / canBuildBuilding / ... ; they read as stale (often zero -> false) until recomputed, so you
  /// MUST call resetChecks() immediately before reading any of them. (PlayerImpl::ResetChecks @0x4776E0.)
  void resetChecks() { if (char* p = impl()) abi::member<0x4776E0, void>(p); }
  /// Can this player still build space-program structures? Goes false once the colony loses the ability to
  /// complete the starship (PlayerImpl::CanBuildSpace @0x477890). Call resetChecks() first. Used by starship-race
  /// failure conditions.
  [[nodiscard]] bool canBuildSpace() const { char* p = impl(); return p && (abi::member<0x477890, int>(p) != 0); }
  /// Can this player still research `techID` (has it, or the prerequisites remain reachable)?
  /// (PlayerImpl::CanDoResearch @0x477BF0.)
  [[nodiscard]] bool canDoResearch(int techID) const {
    char* p = impl(); return p && (abi::member<0x477BF0, int>(p, techID) != 0);
  }

  /// Clear this player's STARSHIP launch/evacuation flags - the `satellites` bitfield at PlayerImpl+8 (launched
  /// modules + Skydock + rare/common/food/evac/child cargo, each a 1-bit flag). The engine does NOT zero these
  /// when you set a player up via goEden()/goAI() during InitProc, so they read STALE - which makes the standard
  /// starship-race victory count-triggers (an `onUnitCount` on `MapID::Skydock` / `RareMetalsCargo` / ...) fire
  /// spuriously on load. Call this during setup of any starship/evacuation mission for a clean launch state.
  Player& clearStarshipState() { if (char* p = impl()) *reinterpret_cast<int*>(p + 8) = 0; return *this; }
  /// Raw value of the `satellites` launch/evac bitfield (PlayerImpl+8). For diagnostics - 0 means no launches.
  [[nodiscard]] int starshipFlags() const { char* p = impl(); return p ? *reinterpret_cast<int*>(p + 8) : -1; }

  // ---- alliances ----
  [[nodiscard]] bool isAlly(int playerNum) const {                          ///< fully allied both ways? @0x490D30
    char* p = impl(); return p && (abi::member<0x490D30, int>(p, playerNum) != 0);
  }
  void allyWith(int playerNum)         { if (char* p = impl()) abi::member<0x4774C0, void>(p, playerNum); }
  void mutuallyAllyWith(int playerNum) { allyWith(playerNum); Player{ playerNum }.allyWith(idx_); }

  /// Count of a launched satellite / Spaceport cargo MapID (drives Starship evacuation victories). @0x4908E0.
  [[nodiscard]] int satelliteCount(int objectMapId) const {
    char* p = impl(); return p ? abi::member<0x4908E0, int>(p, objectMapId) : 0;
  }

  /// This player's live units, optionally of a single type (MapID::Any = all). OP2Lua's `player:units(type)`.
  /// Built on the Module 3 enumeration; defined out-of-line in game.hpp (needs the complete Unit + Game). For a
  /// lazy/composable view instead of a vector, use `Game::unitsOf(index())`.
  [[nodiscard]] std::vector<Unit> units(abi::MapID type = abi::MapID::Any) const;

  /// Center this player's screen on a visible tile. PlayerImpl::CenterViewOn @0x477490 (Location by value).
  void centerViewOn(Location at) const {
    if (char* p = impl()) { struct EngineLoc { int x, y; } loc{ at.engineX(), at.engineY() };
                            abi::member<0x477490, void>(p, loc); }
  }

  /// Submit a command packet AS this player - the single local order-dispatch path
  /// (PlayerImpl::ProcessCommandPacket @0x40E300). The order layer (Unit::move/attack/...) routes through
  /// here; nothing else can reach the raw per-unit Cmd* thunks. Returns EngineRejected if the engine refuses.
  Result<void> issue(const abi::raw::CommandPacket& packet) const {
    if (!valid()) return fail(err::InvalidPlayer);
    char* p = impl();
    if (!p)       return fail(err::EngineRejected);
    const int ok = abi::member<0x40E300, int>(p, &packet);   // const CommandPacket& is passed as a pointer
    return ok ? Result<void>{} : fail(err::EngineRejected);
  }

private:
  // PlayerImpl field byte-offsets (verified by offsetof; sizeof(PlayerImpl) == 3108).
  static constexpr int kStride        = 3108;
  static constexpr int OFF_difficulty = 12;  // PlayerImpl::difficulty_ (union w/ resourceLevel_), probe-verified
  static constexpr int OFF_foodStored = 16,  OFF_maxFood = 20, OFF_maxCommonOre = 24, OFF_commonOre = 32;
  static constexpr int OFF_rareOre    = 36,  OFF_isHuman = 40, OFF_isEden = 44;   // contiguous ints after commonOre
  static constexpr int OFF_numWorkers = 148, OFF_numScientists = 152, OFF_numKids = 156, OFF_recalc = 160;

  /// Read an int PlayerImpl field at byte offset `off` (0 if this player is invalid).
  [[nodiscard]] int readi(int off) const { char* p = impl(); return p ? *reinterpret_cast<int*>(p + off) : 0; }

  /// PlayerImpl* (as char*) for this player, or nullptr. Base read from the code immediate at 0x4890C3.
  /// MUST range-check idx_ first: the array has only kMaxPlayers entries, so `base + idx_*kStride` for an
  /// out-of-range index is a wild pointer - dereferencing it (e.g. a reader on Game::player(99)) faults.
  [[nodiscard]] char* impl() const {
    if (!valid()) return nullptr;
    char* base = abi::ref<char*>(0x4890C3);
    return base ? base + idx_ * kStride : nullptr;
  }
  static void seti(char* p, int off, int v) { *reinterpret_cast<int*>(p + off) = v; }

  void setFaction(bool eden) {
    if (!valid()) return;
    char* p = impl();
    if (!p)       return;
    seti(p, OFF_isEden, eden ? 1 : 0);
  }

  int idx_ = -1;
};

inline Player& Player::goHuman() {
  if (!valid()) return *this;
  char* p = impl();
  if (!p)       return *this;
  abi::member<0x4906C0, void>(p);                                  // PlayerImpl::GoHuman()
  return *this;
}

inline Player& Player::goAI() {
  if (!valid()) return *this;
  char* p = impl();
  if (!p)       return *this;
  abi::member<0x490680, void>(p);                                  // PlayerImpl::GoAI()
  return *this;
}

inline Player& Player::setPopulation(int workers, int scientists, int kids) {
  if (!valid()) return *this;
  char* p = impl();
  if (!p)       return *this;
  seti(p, OFF_numWorkers, workers);
  seti(p, OFF_numScientists, scientists);
  seti(p, OFF_numKids, kids);
  seti(p, OFF_recalc, 1);                                          // nudge the engine to recompute derived values
  return *this;
}

inline Player& Player::setCommonOre(int amount, int capacity) {
  if (!valid()) return *this;
  char* p = impl();
  if (!p)       return *this;
  if (capacity > 0) seti(p, OFF_maxCommonOre, capacity);
  seti(p, OFF_commonOre, amount);
  return *this;
}

inline Player& Player::setFood(int amount, int capacity) {
  if (!valid()) return *this;
  char* p = impl();
  if (!p)       return *this;
  if (capacity > 0) seti(p, OFF_maxFood, capacity);
  seti(p, OFF_foodStored, amount);
  return *this;
}

inline Player& Player::setRareOre(int amount) {
  if (!valid()) return *this;
  char* p = impl();
  if (!p)       return *this;
  seti(p, OFF_rareOre, amount);                                    // plain field write (matches Tethys SetRareOre)
  return *this;
}

inline Player& Player::setWorkers(int n) {
  if (!valid()) return *this;
  char* p = impl();
  if (!p)       return *this;
  seti(p, OFF_numWorkers, n);  seti(p, OFF_recalc, 1);             // nudge a recompute of derived population values
  return *this;
}

inline Player& Player::setScientists(int n) {
  if (!valid()) return *this;
  char* p = impl();
  if (!p)       return *this;
  seti(p, OFF_numScientists, n);  seti(p, OFF_recalc, 1);
  return *this;
}

inline Player& Player::setKids(int n) {
  if (!valid()) return *this;
  char* p = impl();
  if (!p)       return *this;
  seti(p, OFF_numKids, n);  seti(p, OFF_recalc, 1);
  return *this;
}

inline Player& Player::setTechLevel(int techLevel) {
  if (!valid()) return *this;
  char* research = reinterpret_cast<char*>(abi::reloc(0x56C230));  // Research singleton @ 0x56C230
  abi::member<0x473030, void>(research, idx_, techLevel * 1000);   // Research::SetTechLevel(num, level*1000)
  return *this;
}

} // namespace op2
