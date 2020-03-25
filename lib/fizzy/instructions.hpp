#pragma once

#include "types.hpp"

namespace fizzy
{
struct InstructionMetrics
{
    /// The minimum number of the stack items required for the instruction.
    int8_t stack_height_required;

    /// The EVM stack height change caused by the instruction execution,
    /// i.e. stack height _after_ execution - stack height _before_ execution.
    int8_t stack_height_change;
};

InstructionMetrics* get_instruction_metrics_table() noexcept;

ssize_t instr_stack_change(Instr instr);

}  // namespace fizzy
