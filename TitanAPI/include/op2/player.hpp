#pragma once
// op2/player.hpp — Player: a value handle for a player slot, wrapping the engine's PlayerImpl.
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

  // ---- colony setup (each writes live engine state) ----
  Result<void> goEden()     { return setFaction(true);  }   ///< faction = Eden
  Result<void> goPlymouth() { return setFaction(false); }   ///< faction = Plymouth
  Result<void> goHuman();                                   ///< mark this a human-controlled colony
  Result<void> goAI();                                      ///< mark this a computer-controlled (AI) player
  Result<void> setPopulation(int workers, int scientists, int kids);
  Result<void> setCommonOre(int amount, int capacity = 0);  ///< capacity 0 = leave as-is (needs storage to persist)
  Result<void> setFood(int amount, int capacity = 0);
  Result<void> setRareOre(int amount);                      ///< set the rare-ore pool (needs storage to persist, like ore)
  Result<void> setWorkers(int n);                           ///< set the worker pool (OP2Lua player.workers setter)
  Result<void> setScientists(int n);                        ///< set the scientist pool
  Result<void> setKids(int n);                              ///< set the children pool
  Result<void> setTechLevel(int techLevel);                 ///< grants all techs up to techLevel

  // ---- runtime reads (parity with OP2Lua player.*) — live PlayerImpl fields ----
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

  /// The difficulty level chosen in the Colony Games menu (a per-player setting). Lets a mission tune itself —
  /// e.g. AI resources or disaster timing. PlayerImpl::difficulty_ @+12 (probe-verified). Mirrors OP2Lua
  /// player.difficulty.
  enum class Difficulty : int { Easy = 0, Normal = 1, Hard = 2 };
  [[nodiscard]] Difficulty difficulty() const { return Difficulty(readi(OFF_difficulty)); }

  // ---- technology ----
  [[nodiscard]] bool hasTechnology(int techID) const {                      ///< PlayerImpl::HasTechnology @0x477570
    char* p = impl(); return p && (abi::member<0x477570, int>(p, techID) != 0);
  }
  void markResearchComplete(int techID) { if (char* p = impl()) abi::member<0x477510, void>(p, techID); }

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

  /// Submit a command packet AS this player — the single local order-dispatch path
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
  [[nodiscard]] char* impl() const {
    char* base = abi::ref<char*>(0x4890C3);
    return base ? base + idx_ * kStride : nullptr;
  }
  static void seti(char* p, int off, int v) { *reinterpret_cast<int*>(p + off) = v; }

  Result<void> setFaction(bool eden) {
    if (!valid())          return fail(err::InvalidPlayer);
    char* p = impl();
    if (!p)                return fail(err::EngineRejected);
    seti(p, OFF_isEden, eden ? 1 : 0);
    return {};
  }

  int idx_ = -1;
};

inline Result<void> Player::goHuman() {
  if (!valid()) return fail(err::InvalidPlayer);
  char* p = impl();
  if (!p)       return fail(err::EngineRejected);
  abi::member<0x4906C0, void>(p);                                  // PlayerImpl::GoHuman()
  return {};
}

inline Result<void> Player::goAI() {
  if (!valid()) return fail(err::InvalidPlayer);
  char* p = impl();
  if (!p)       return fail(err::EngineRejected);
  abi::member<0x490680, void>(p);                                  // PlayerImpl::GoAI()
  return {};
}

inline Result<void> Player::setPopulation(int workers, int scientists, int kids) {
  if (!valid()) return fail(err::InvalidPlayer);
  char* p = impl();
  if (!p)       return fail(err::EngineRejected);
  seti(p, OFF_numWorkers, workers);
  seti(p, OFF_numScientists, scientists);
  seti(p, OFF_numKids, kids);
  seti(p, OFF_recalc, 1);                                          // nudge the engine to recompute derived values
  return {};
}

inline Result<void> Player::setCommonOre(int amount, int capacity) {
  if (!valid()) return fail(err::InvalidPlayer);
  char* p = impl();
  if (!p)       return fail(err::EngineRejected);
  if (capacity > 0) seti(p, OFF_maxCommonOre, capacity);
  seti(p, OFF_commonOre, amount);
  return {};
}

inline Result<void> Player::setFood(int amount, int capacity) {
  if (!valid()) return fail(err::InvalidPlayer);
  char* p = impl();
  if (!p)       return fail(err::EngineRejected);
  if (capacity > 0) seti(p, OFF_maxFood, capacity);
  seti(p, OFF_foodStored, amount);
  return {};
}

inline Result<void> Player::setRareOre(int amount) {
  if (!valid()) return fail(err::InvalidPlayer);
  char* p = impl();
  if (!p)       return fail(err::EngineRejected);
  seti(p, OFF_rareOre, amount);                                    // plain field write (matches Tethys SetRareOre)
  return {};
}

inline Result<void> Player::setWorkers(int n) {
  if (!valid()) return fail(err::InvalidPlayer);
  char* p = impl();
  if (!p)       return fail(err::EngineRejected);
  seti(p, OFF_numWorkers, n);  seti(p, OFF_recalc, 1);             // nudge a recompute of derived population values
  return {};
}

inline Result<void> Player::setScientists(int n) {
  if (!valid()) return fail(err::InvalidPlayer);
  char* p = impl();
  if (!p)       return fail(err::EngineRejected);
  seti(p, OFF_numScientists, n);  seti(p, OFF_recalc, 1);
  return {};
}

inline Result<void> Player::setKids(int n) {
  if (!valid()) return fail(err::InvalidPlayer);
  char* p = impl();
  if (!p)       return fail(err::EngineRejected);
  seti(p, OFF_numKids, n);  seti(p, OFF_recalc, 1);
  return {};
}

inline Result<void> Player::setTechLevel(int techLevel) {
  if (!valid()) return fail(err::InvalidPlayer);
  char* research = reinterpret_cast<char*>(abi::reloc(0x56C230));  // Research singleton @ 0x56C230
  abi::member<0x473030, void>(research, idx_, techLevel * 1000);   // Research::SetTechLevel(num, level*1000)
  return {};
}

} // namespace op2
