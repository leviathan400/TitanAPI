#pragma once
// cmd_builder.hpp — build a valid CommandPacket safely (Layer 1 / op2::abi).
//
// Every Outpost 2 order is one CommandPacket handed to PlayerImpl::ProcessCommandPacket (0x40E300). Whatever
// the command, its payload has the same shape:
//
//     [ unit-list header ] [ optional waypoint list ] [ fixed trailing fields ]
//
// This builder writes those parts into the 99-byte payload at the correct *running* offsets and tracks
// dataLength — so the order layer never hand-fills a header. unitID[]/waypoint[] are flexible arrays: with N
// units the waypoint list starts at 1 + 2*N, NOT at MoveCommand's nominal struct offset, which is exactly
// why hand-filling these went wrong. CmdBuilder owns the one correct place numUnits/unitID[] is written —
// the spot TethysAPI's DoMiningRoute got wrong (it set numUnits = the unit id). See
// re-reference/command-packets.md and design/FACADE-DESIGN.md.

#include <cstring>
#include <cstddef>
#include <span>
#include "op2/abi/raw/command.hpp"

namespace op2::abi {

/// Fluent, bounds-checked builder for a CommandPacket payload. Call the header helper, then any waypoints,
/// then field() for trailing fixed members, then build(). Reusable for every command shape.
class CmdBuilder {
public:
  explicit CmdBuilder(CommandType type) { packet_.type = type; }

  // --- unit-list header (SimpleCommand: numUnits + unitID[]) — the bug-prone header, written once here ---
  CmdBuilder& units(std::span<const raw::u16> ids) {
    put8(static_cast<raw::u8>(ids.size()));
    for (raw::u16 id : ids) put16(id);
    return *this;
  }
  CmdBuilder& oneUnit(raw::u16 id) { return units({&id, 1}); }

  // --- single-unit header (SingleUnitSimpleCommand: a bare unitID, no count) ---
  CmdBuilder& singleUnit(raw::u16 id) { put16(id); return *this; }

  // --- waypoint list (MoveCommand: numWaypoints + waypoint[]) ---
  CmdBuilder& waypoints(std::span<const raw::Waypoint> wps) {
    put16(static_cast<raw::u16>(wps.size()));
    for (const raw::Waypoint& w : wps) put(&w, sizeof(w));
    return *this;
  }
  CmdBuilder& oneWaypoint(raw::Waypoint w) { return waypoints({&w, 1}); }

  /// Append one trailing fixed field (PackedMapRect, CommandTarget, a u16, …). POD only.
  template <class T>
  CmdBuilder& field(const T& v) { put(&v, sizeof(v)); return *this; }

  /// True while everything written so far fits the 99-byte payload (no overflow).
  bool ok() const { return ok_; }
  raw::u16 dataLength() const { return static_cast<raw::u16>(len_); }

  /// Finalize: stamp dataLength (= payload bytes used) and return the packet. timeStamp/netID stay 0 —
  /// those are network-only; a locally-issued order leaves them zero.
  const raw::CommandPacket& build() {
    packet_.dataLength = static_cast<raw::u16>(len_);
    return packet_;
  }

private:
  void put(const void* p, std::size_t n) {
    if (len_ + n > raw::CommandPacketDataSize) { ok_ = false; return; }
    std::memcpy(packet_.data.buffer + len_, p, n);
    len_ += n;
  }
  void put8(raw::u8 v)   { put(&v, 1); }
  void put16(raw::u16 v) { put(&v, 2); }

  raw::CommandPacket packet_{};
  std::size_t        len_ = 0;
  bool               ok_  = true;
};

} // namespace op2::abi
