#pragma once
// op2.hpp — TitanAPI Layer 2 facade umbrella. Include this to get the ergonomic API:
//   op2::Result<T> / op2::Error, op2::Location, op2::Unit, op2::Player, op2::Game, op2::MapID.
//
// The facade is built on Layer 1 (op2::abi). For raw engine access, include op2/abi/memory.hpp directly.

#include "op2/core/error.hpp"
#include "op2/core/location.hpp"
#include "op2/unit.hpp"
#include "op2/player.hpp"
#include "op2/game.hpp"
#include "op2/groups.hpp"   // Module 5: AI ScGroups (FightGroup / MiningGroup / BuildingGroup)
#include "op2/base.hpp"     // Module 8: declarative base layout (BaseLayout / createBase) + line/message helpers
