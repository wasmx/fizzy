// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "constexpr_vector.hpp"
#include "types.hpp"
#include <cassert>
#include <cstdint>

namespace fizzy
{
// Wasm 1.0 spec only has instructions which take at most 2 items and return at most 1 item.
struct InstructionType
{
    constexpr_vector<ValType, 2> inputs;
    constexpr_vector<ValType, 1> outputs;
};

const InstructionType* get_instruction_type_table() noexcept;

/// Returns the table of max alignment values for each instruction - the largest acceptable
/// alignment value satisfying `2 ** max_align < memory_width` where memory_width is the number of
/// bytes the instruction operates on.
///
/// It may contain invalid value for instructions not needing it.
const uint8_t* get_instruction_max_align_table() noexcept;

/// Returns the table of cost values for each instruction - how many ticks an instruction takes in
/// execution metering.
const int16_t* get_instruction_cost_table() noexcept;

}  // namespace fizzy
