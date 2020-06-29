// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "types.hpp"
#include <cassert>
#include <cstdint>
#include <initializer_list>

namespace fizzy
{
struct InstructionMetrics
{
    /// The minimum number of the stack items required for the instruction.
    int8_t stack_height_required;

    /// The stack height change caused by the instruction execution,
    /// i.e. stack height _after_ execution - stack height _before_ execution.
    int8_t stack_height_change;

    /// The largest acceptable alignment value satisfying `2 ** max_align < memory_width` where
    /// memory_width is the number of bytes the instruction operates on.
    ///
    /// This field may contain invalid value for instructions not needing it.
    uint8_t max_align;

    InstructionMetrics() = default;

    constexpr InstructionMetrics(
        int8_t _stack_height_required, int8_t _stack_height_change, uint8_t _max_align = 0) noexcept
      : stack_height_required(_stack_height_required),
        stack_height_change(_stack_height_change),
        max_align(_max_align)
    {
        // The valid range is between 0 and 3.
        assert(max_align <= 3);
    }
};

const InstructionMetrics* get_instruction_metrics_table() noexcept;

struct InstructionType
{
    std::initializer_list<ValType> inputs;
    std::initializer_list<ValType> outputs;
};

const InstructionType* get_instruction_type_table() noexcept;

}  // namespace fizzy
