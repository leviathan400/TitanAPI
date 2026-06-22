#pragma once
// op2/trigger.hpp - Module 4: the mission event/trigger system, with a C++ callback registry.
//
// THE PROBLEM: OP2 fires a trigger by looking up an EXPORTED FUNCTION NAME in the mission DLL
// (GetProcAddress) and calling it (void __cdecl, no args). Classic missions write `void MyFn() {...}` and
// export it. To let TitanAPI missions pass a C++ lambda / std::function instead, we bridge it:
//
//   * TitanAPI exports a fixed pool of dispatcher stubs:  TitanTrigger0 .. TitanTrigger15.
//   * Creating a trigger RESERVES a slot, stores your callback, and hands the engine that slot's stub name.
//   * When the engine fires the trigger it calls TitanTrigger<k>(), which invokes the stored callback.
//
// So the engine ends up calling back into your C++ - the half of mission scripting the order API can't do.
//
// NOTE: this header DEFINES exported symbols, so include it in exactly one translation unit (the mission's
// main .cpp). The stubs are `inline` so multiple inclusion is harmless, but keep it to one TU to be safe.

#include <functional>
#include "op2/abi/memory.hpp"
#include "op2/abi/enums.hpp"   // MapID (for onUnitCount)

namespace op2 {

/// Handle to an engine trigger (an `ScStub` index). Returned by the on*/when* factories below.
class Trigger {
public:
  /// The engine's invalid-stub sentinel (ScStubList::NilIndex == MaxNumScStubs). Valid indices are 0..254,
  /// so index 0 is a legitimate trigger - `valid()` must not treat it as null.
  static constexpr int kNilIndex = 255;

  constexpr Trigger() = default;                                 // id_ == kNilIndex -> invalid
  constexpr explicit Trigger(int id) : id_{ id } {}
  [[nodiscard]] constexpr int  id()    const noexcept { return id_; }
  [[nodiscard]] constexpr bool valid() const noexcept { return (id_ >= 0) && (id_ != kNilIndex); }

  // --- engine state (thiscall thunks on the {int id_} ScStub handle; `this` == &id_) ---
  void enable()  { int id = id_; abi::member<0x478C60, void>(&id); }   ///< Enable this trigger.
  void disable() { int id = id_; abi::member<0x478C70, void>(&id); }   ///< Disable this trigger.
  [[nodiscard]] bool isEnabled() const { int id = id_; return abi::member<0x478CA0, int>(&id) != 0; }
  /// Did this trigger fire for `playerNum`? (Do not pass AllPlayers / -1.)
  [[nodiscard]] bool hasFired(int playerNum) const {
    int id = id_; return abi::member<0x478CC0, int>(&id, playerNum) != 0;
  }

private:
  int id_ = kNilIndex;
};

namespace trigger_detail {

/// Stub names - these MUST match the exported TitanTrigger<n> functions defined just after this namespace. The
/// pool is sized generously (64) so even a large campaign mission with many timed / area / count / research
/// triggers never runs out. kMaxCallbacks is DERIVED from this array, so the names array (and the matching stub
/// list below) are the single source of truth; a static_assert after the stubs guards that they stay in sync.
inline const char* const g_names[] = {
  "TitanTrigger0",  "TitanTrigger1",  "TitanTrigger2",  "TitanTrigger3",  "TitanTrigger4",  "TitanTrigger5",  "TitanTrigger6",  "TitanTrigger7",
  "TitanTrigger8",  "TitanTrigger9",  "TitanTrigger10", "TitanTrigger11", "TitanTrigger12", "TitanTrigger13", "TitanTrigger14", "TitanTrigger15",
  "TitanTrigger16", "TitanTrigger17", "TitanTrigger18", "TitanTrigger19", "TitanTrigger20", "TitanTrigger21", "TitanTrigger22", "TitanTrigger23",
  "TitanTrigger24", "TitanTrigger25", "TitanTrigger26", "TitanTrigger27", "TitanTrigger28", "TitanTrigger29", "TitanTrigger30", "TitanTrigger31",
  "TitanTrigger32", "TitanTrigger33", "TitanTrigger34", "TitanTrigger35", "TitanTrigger36", "TitanTrigger37", "TitanTrigger38", "TitanTrigger39",
  "TitanTrigger40", "TitanTrigger41", "TitanTrigger42", "TitanTrigger43", "TitanTrigger44", "TitanTrigger45", "TitanTrigger46", "TitanTrigger47",
  "TitanTrigger48", "TitanTrigger49", "TitanTrigger50", "TitanTrigger51", "TitanTrigger52", "TitanTrigger53", "TitanTrigger54", "TitanTrigger55",
  "TitanTrigger56", "TitanTrigger57", "TitanTrigger58", "TitanTrigger59", "TitanTrigger60", "TitanTrigger61", "TitanTrigger62", "TitanTrigger63",
};
inline constexpr int kMaxCallbacks = int(sizeof(g_names) / sizeof(g_names[0]));   ///< dispatcher-stub pool size (64)
inline std::function<void()> g_callbacks[kMaxCallbacks];
inline int g_count = 0;

/// Forward-declared (like GetModuleHandleA in memory.hpp) so this header needn't pull in <windows.h>. Used to
/// surface pool exhaustion to a debugger / DebugView instead of failing silently.
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char*);

/// Invoke the callback registered in slot `i` (called by the exported TitanTrigger<i> stubs).
inline void fire(int i) {
  if ((i >= 0) && (i < kMaxCallbacks) && g_callbacks[i]) g_callbacks[i]();
}

/// Reserve a slot for `cb`; returns its exported-stub name, or nullptr if the pool is exhausted.
inline const char* reserve(std::function<void()> cb) {
  if (g_count >= kMaxCallbacks) {
    OutputDebugStringA("TitanAPI: trigger callback pool exhausted - raise kMaxCallbacks in trigger.hpp, "
                       "or use a named exported trigger function for the extra triggers.\n");
    return nullptr;
  }
  const int i = g_count++;
  g_callbacks[i] = std::move(cb);
  return g_names[i];
}

/// Resolve a callback to a dispatcher-stub name for the engine. An EMPTY callback -> NULLPTR, the engine's
/// "no function" sentinel (a bare CONDITION trigger, e.g. for victoryWhen/defeatWhen) - OP2 v1.4.1+. NOTE: it
/// must be nullptr, NOT "" - the engine treats "" as a function literally named "" and faults in its reference
/// resolver ("Couldn't resolve data reference") when it later evaluates the trigger. Otherwise reserve a slot;
/// if the pool is exhausted, also fall back to nullptr (no callback) rather than hand the engine a bad name.
inline const char* fnName(std::function<void()> cb) {
  if (!cb) return nullptr;
  const char* n = reserve(std::move(cb));
  return n ? n : nullptr;
}

} // namespace trigger_detail
} // namespace op2

// The exported dispatcher stubs. dllexport gives them undecorated export names (like InitProc) so OP2 can
// GetProcAddress them by name. Each just forwards to its registry slot.
#define OP2_TITAN_TRIGGER_STUB(n) \
  extern "C" __declspec(dllexport) inline void __cdecl TitanTrigger##n() { ::op2::trigger_detail::fire(n); }
OP2_TITAN_TRIGGER_STUB(0)  OP2_TITAN_TRIGGER_STUB(1)  OP2_TITAN_TRIGGER_STUB(2)  OP2_TITAN_TRIGGER_STUB(3)
OP2_TITAN_TRIGGER_STUB(4)  OP2_TITAN_TRIGGER_STUB(5)  OP2_TITAN_TRIGGER_STUB(6)  OP2_TITAN_TRIGGER_STUB(7)
OP2_TITAN_TRIGGER_STUB(8)  OP2_TITAN_TRIGGER_STUB(9)  OP2_TITAN_TRIGGER_STUB(10) OP2_TITAN_TRIGGER_STUB(11)
OP2_TITAN_TRIGGER_STUB(12) OP2_TITAN_TRIGGER_STUB(13) OP2_TITAN_TRIGGER_STUB(14) OP2_TITAN_TRIGGER_STUB(15)
OP2_TITAN_TRIGGER_STUB(16) OP2_TITAN_TRIGGER_STUB(17) OP2_TITAN_TRIGGER_STUB(18) OP2_TITAN_TRIGGER_STUB(19)
OP2_TITAN_TRIGGER_STUB(20) OP2_TITAN_TRIGGER_STUB(21) OP2_TITAN_TRIGGER_STUB(22) OP2_TITAN_TRIGGER_STUB(23)
OP2_TITAN_TRIGGER_STUB(24) OP2_TITAN_TRIGGER_STUB(25) OP2_TITAN_TRIGGER_STUB(26) OP2_TITAN_TRIGGER_STUB(27)
OP2_TITAN_TRIGGER_STUB(28) OP2_TITAN_TRIGGER_STUB(29) OP2_TITAN_TRIGGER_STUB(30) OP2_TITAN_TRIGGER_STUB(31)
OP2_TITAN_TRIGGER_STUB(32) OP2_TITAN_TRIGGER_STUB(33) OP2_TITAN_TRIGGER_STUB(34) OP2_TITAN_TRIGGER_STUB(35)
OP2_TITAN_TRIGGER_STUB(36) OP2_TITAN_TRIGGER_STUB(37) OP2_TITAN_TRIGGER_STUB(38) OP2_TITAN_TRIGGER_STUB(39)
OP2_TITAN_TRIGGER_STUB(40) OP2_TITAN_TRIGGER_STUB(41) OP2_TITAN_TRIGGER_STUB(42) OP2_TITAN_TRIGGER_STUB(43)
OP2_TITAN_TRIGGER_STUB(44) OP2_TITAN_TRIGGER_STUB(45) OP2_TITAN_TRIGGER_STUB(46) OP2_TITAN_TRIGGER_STUB(47)
OP2_TITAN_TRIGGER_STUB(48) OP2_TITAN_TRIGGER_STUB(49) OP2_TITAN_TRIGGER_STUB(50) OP2_TITAN_TRIGGER_STUB(51)
OP2_TITAN_TRIGGER_STUB(52) OP2_TITAN_TRIGGER_STUB(53) OP2_TITAN_TRIGGER_STUB(54) OP2_TITAN_TRIGGER_STUB(55)
OP2_TITAN_TRIGGER_STUB(56) OP2_TITAN_TRIGGER_STUB(57) OP2_TITAN_TRIGGER_STUB(58) OP2_TITAN_TRIGGER_STUB(59)
OP2_TITAN_TRIGGER_STUB(60) OP2_TITAN_TRIGGER_STUB(61) OP2_TITAN_TRIGGER_STUB(62) OP2_TITAN_TRIGGER_STUB(63)
#undef OP2_TITAN_TRIGGER_STUB

// Tripwire: if you resize g_names, update the stub list above to match (and this literal). The pool is DERIVED
// from g_names, so these must stay the same length or the engine would be handed a name with no backing stub.
static_assert(::op2::trigger_detail::kMaxCallbacks == 64,
              "TitanTrigger stub list and g_names must stay the same length - update both.");

namespace op2 {

// ---- trigger factories (Module 4, first cut) -----------------------------------------------------------------
// Each reserves a callback slot and creates the matching engine trigger, handing it the slot's stub name.

/// Engine time is measured in TICKS; the in-game clock shows MARKS. 1 mark = 100 ticks. (No seconds/minutes
/// anywhere in the API - author timed events in marks, or ticks for fine control.)
inline constexpr int kTicksPerMark = 100;

/// Run `cb` after `tick` engine ticks. NOTE: the engine's time is a RELATIVE INTERVAL measured from when the
/// trigger is created, NOT an absolute game clock - onTick(100) means "100 ticks from now". When created in
/// InitProc (game time ~0) the interval doubles as an absolute tick, which is why onMark works there; but a
/// trigger created mid-game fires `tick` ticks LATER, so use a small interval (e.g. op2::win uses 1). oneShot
/// fires once then disables. Wraps CreateTimeTrigger @0x478D00.
inline Trigger onTick(int tick, std::function<void()> cb, bool oneShot = true) {
  const char* fn = trigger_detail::fnName(std::move(cb));
  // CreateTimeTrigger returns a Trigger BY VALUE; ScStub's non-trivial dtor forces the x86 sret ABI (a hidden
  // result pointer as the implicit first arg). callFastSret supplies it; the engine writes back the stub index.
  int id = Trigger::kNilIndex;
  abi::callFastSret<0x478D00>(&id, 1 /*enabled*/, oneShot ? 1 : 0, tick, fn);
  return Trigger{ id };
}

/// Run `cb` at game MARK `mark` (the common form; e.g. a welcome message at mark 5). 1 mark = 100 ticks.
inline Trigger onMark(int mark, std::function<void()> cb, bool oneShot = true) {
  return onTick(mark * kTicksPerMark, std::move(cb), oneShot);
}

// ---- enums / constants used by the condition factories below --------------------------------------------------

inline constexpr int kAllPlayers = -1;   ///< apply a trigger to every player (engine AllPlayers sentinel)

/// Comparison used by the count / resource triggers.
enum class Compare  : int { Equal = 0, LowerEqual, GreaterEqual, Lower, Greater };

/// Resource kinds for onResource (CreateResourceTrigger).
enum class Resource : int { Food = 0, CommonOre, RareOre, Kids, Workers, Scientists, Colonists };

// ---- condition factories --------------------------------------------------------------------------------------
// Each takes an OPTIONAL callback: pass one to "do X when this happens", or omit it to make a bare CONDITION
// trigger to feed victoryWhen / defeatWhen. A slot is reserved only when a callback is supplied. All return an
// op2::Trigger handle (sret ABI, like onTick).

/// Fires when `player`'s building count `cmp` `count`. (CreateBuildingCountTrigger @0x4793A0.)
inline Trigger onBuildingCount(int player, Compare cmp, int count,
                               std::function<void()> cb = {}, bool oneShot = false) {
  const char* fn = trigger_detail::fnName(std::move(cb));
  int id = Trigger::kNilIndex;
  abi::callFastSret<0x4793A0>(&id, 1, oneShot ? 1 : 0, player, count, static_cast<int>(cmp), fn);
  return Trigger{ id };
}

/// Fires when `player`'s vehicle count `cmp` `count`. (CreateVehicleCountTrigger @0x479440.)
inline Trigger onVehicleCount(int player, Compare cmp, int count,
                              std::function<void()> cb = {}, bool oneShot = false) {
  const char* fn = trigger_detail::fnName(std::move(cb));
  int id = Trigger::kNilIndex;
  abi::callFastSret<0x479440>(&id, 1, oneShot ? 1 : 0, player, count, static_cast<int>(cmp), fn);
  return Trigger{ id };
}

/// Fires when `player`'s count of `unitType` carrying `cargoOrWeapon` `cmp` `count`. (CreateCountTrigger
/// @0x479110.) Pass `MapID::Any` (-1) for "any weapon/cargo, or none" - this is the wildcard real missions
/// use (OP2Helper). @warning `MapID::None` (0) matches *nothing* and reads 0 - not what you want for "any".
/// Also counts structure types (e.g. CommandCenter) by existence, so it's the way to do "destroy the CC".
inline Trigger onUnitCount(int player, abi::MapID unitType, abi::MapID cargoOrWeapon, Compare cmp, int count,
                           std::function<void()> cb = {}, bool oneShot = false) {
  const char* fn = trigger_detail::fnName(std::move(cb));
  int id = Trigger::kNilIndex;
  abi::callFastSret<0x479110>(&id, 1, oneShot ? 1 : 0, player,
                              static_cast<int>(unitType), static_cast<int>(cargoOrWeapon),
                              count, static_cast<int>(cmp), fn);
  return Trigger{ id };
}

/// Fires once `player` has researched tech `techID`. (CreateResearchTrigger @0x478E90.)
inline Trigger onResearch(int techID, std::function<void()> cb = {},
                          int player = kAllPlayers, bool oneShot = false) {
  const char* fn = trigger_detail::fnName(std::move(cb));
  int id = Trigger::kNilIndex;
  abi::callFastSret<0x478E90>(&id, 1, oneShot ? 1 : 0, techID, player, fn);
  return Trigger{ id };
}

/// Fires when `player`'s `res` amount `cmp` `amount`. (CreateResourceTrigger @0x478DE0.)
inline Trigger onResource(Resource res, Compare cmp, int amount, std::function<void()> cb = {},
                          int player = kAllPlayers, bool oneShot = false) {
  const char* fn = trigger_detail::fnName(std::move(cb));
  int id = Trigger::kNilIndex;
  abi::callFastSret<0x478DE0>(&id, 1, oneShot ? 1 : 0, static_cast<int>(res), amount, player,
                              static_cast<int>(cmp), fn);
  return Trigger{ id };
}

/// Fires when `player`'s count of OPERATIONAL (built + powered) `structureType` `cmp` `count`. The OP2 "Last
/// One Standing" idiom. (CreateOperationalTrigger @0x479880.) @warning Do not pass kAllPlayers.
/// @warning GLOBAL SIDE EFFECT: creating an operational trigger can flip the game into Last-One-Standing
/// victory mode, in which any player with no operational CC is eliminated. Only use it when you actually want
/// that mode. @note counts *operational* structures - an unpowered/disabled one is not counted (unlike
/// onUnitCount, which counts existence). Pick onBuildingCount/onUnitCount if you just mean "exists/destroyed".
inline Trigger onOperational(int player, abi::MapID structureType, Compare cmp, int count,
                             std::function<void()> cb = {}, bool oneShot = false) {
  const char* fn = trigger_detail::fnName(std::move(cb));
  int id = Trigger::kNilIndex;
  abi::callFastSret<0x479880>(&id, 1, oneShot ? 1 : 0, player,
                              static_cast<int>(structureType), count, static_cast<int>(cmp), fn);
  return Trigger{ id };
}

// ---- victory / defeat (each WRAPS a condition trigger) --------------------------------------------------------

/// Declare a victory condition: when `condition` fires, the player(s) win; `objective` is shown in the
/// objectives list. (CreateVictoryCondition @0x479930 - also an sret return, with the condition passed by ref.)
inline Trigger victoryWhen(Trigger condition, const char* objective, bool oneShot = false) {
  int condId = condition.id();   // engine takes the condition as `const Trigger&` -> pass &id_
  int id     = Trigger::kNilIndex;
  abi::callFastSret<0x479930>(&id, 1, oneShot ? 1 : 0, &condId, objective);
  return Trigger{ id };
}

/// Declare a failure condition: when `condition` fires, the player(s) lose. (CreateFailureCondition @0x479980.)
inline Trigger defeatWhen(Trigger condition) {
  int condId = condition.id();
  int id     = Trigger::kNilIndex;
  abi::callFastSret<0x479980>(&id, 1, 0, &condId, "");
  return Trigger{ id };
}

// ---- programmatic win / lose (decide victory in your own code) -----------------------------------------------
// These end the mission almost immediately by wrapping a one-shot time trigger that fires on the next eval.
// CRITICAL: CreateTimeTrigger's time is a RELATIVE INTERVAL (ticks AFTER the trigger is created), NOT an
// absolute game tick. So the correct value when called mid-game is a small interval like 1 ("fire next tick"),
// exactly as OP2Lua's proven mission.win() does:
//     CreateVictoryCondition(CreateTimeTrigger(1, "OP2LuaTrigger", true, true), "...", true, true);
// (An earlier version passed nowTick()+4 here, mistaking the interval for an absolute tick. Mid-game that
// scheduled the victory ~thousands of ticks into the FUTURE, so it never fired before the player quit - the
// real bug behind "op2::win() logged but no victory". Verified against the v0.5.24 log: CC killed at tick 4803,
// trigger would not have fired until ~9610.) Call these once your own logic decides the outcome.

/// Win the mission, showing `objective` as the completed goal. Mirrors OP2Lua mission.win(): a one-shot time
/// trigger (interval 1, real no-op callback) wrapped in a one-shot victory condition. Being a sole victory it
/// ends the mission on the next tick; if other victory conditions exist, OP2 ANDs them - all must be met.
inline Trigger win(const char* objective = "Mission accomplished") {
  return victoryWhen(onTick(1, [] {}), objective, /*oneShot=*/true);
}
/// Lose the mission (fires next tick). Mirrors OP2Lua mission.lose().
inline Trigger lose() {
  return defeatWhen(onTick(1, [] {}));
}

// ---- campaign victory/defeat one-liners (Module 8 - OP2Helper-style conveniences) ----

/// Lose the moment `player` loses their last OPERATIONAL Command Center - the classic "defend your CC" failure.
/// Proven for the human player. (onOperational + defeatWhen.)
inline Trigger loseIfNoCommandCenter(int player) {
  return defeatWhen(onOperational(player, abi::MapID::CommandCenter, Compare::Equal, 0));
}

/// Win, showing `objective`, when `enemyPlayer` is reduced to zero standing structures. NOTE: onBuildingCount
/// reads an UNPOWERED enemy base as 0 and would fire immediately - so use this only against a powered enemy
/// colony; otherwise drive the win from your own enumeration + op2::win() (see the count-trigger notes).
inline Trigger winWhenColonyDestroyed(int enemyPlayer, const char* objective = "Destroy the enemy colony") {
  return victoryWhen(onBuildingCount(enemyPlayer, Compare::Lower, 1), objective);
}

} // namespace op2
