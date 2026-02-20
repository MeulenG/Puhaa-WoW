#pragma once

#include "game/opcode_table.hpp"

namespace pwow {
namespace game {

// Backwards-compatibility alias: existing code uses Opcode::X which now maps
// to LogicalOpcode::X (the expansion-agnostic logical enum).
// Wire values are resolved at runtime via OpcodeTable.
using Opcode = LogicalOpcode;

} // namespace game
} // namespace pwow
