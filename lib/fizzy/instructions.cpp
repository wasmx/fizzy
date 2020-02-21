#include "instructions.hpp"

namespace fizzy
{
ssize_t instr_stack_change(Instr instr)
{
    switch (instr)
    {
    case Instr::unreachable:
    case Instr::nop:
    case Instr::i32_eqz:
    case Instr::i64_eqz:
    case Instr::i32_clz:
    case Instr::i32_ctz:
    case Instr::i32_popcnt:
    case Instr::i64_clz:
    case Instr::i64_ctz:
    case Instr::i64_popcnt:
    case Instr::i32_wrap_i64:
    case Instr::i64_extend_i32_s:
    case Instr::i64_extend_i32_u:
        return 0;
    case Instr::return_:
    case Instr::drop:
    case Instr::select:  // is this correct?
    case Instr::i32_eq:
    case Instr::i32_ne:
    case Instr::i32_lt_s:
    case Instr::i32_lt_u:
    case Instr::i32_gt_s:
    case Instr::i32_gt_u:
    case Instr::i32_le_s:
    case Instr::i32_le_u:
    case Instr::i32_ge_s:
    case Instr::i32_ge_u:
    case Instr::i64_eq:
    case Instr::i64_ne:
    case Instr::i64_lt_s:
    case Instr::i64_lt_u:
    case Instr::i64_gt_s:
    case Instr::i64_gt_u:
    case Instr::i64_le_s:
    case Instr::i64_le_u:
    case Instr::i64_ge_s:
    case Instr::i64_ge_u:
    case Instr::i32_add:
    case Instr::i32_sub:
    case Instr::i32_mul:
    case Instr::i32_div_s:
    case Instr::i32_div_u:
    case Instr::i32_rem_s:
    case Instr::i32_rem_u:
    case Instr::i32_and:
    case Instr::i32_or:
    case Instr::i32_xor:
    case Instr::i32_shl:
    case Instr::i32_shr_s:
    case Instr::i32_shr_u:
    case Instr::i32_rotl:
    case Instr::i32_rotr:
    case Instr::i64_add:
    case Instr::i64_sub:
    case Instr::i64_mul:
    case Instr::i64_div_s:
    case Instr::i64_div_u:
    case Instr::i64_rem_s:
    case Instr::i64_rem_u:
    case Instr::i64_and:
    case Instr::i64_or:
    case Instr::i64_xor:
    case Instr::i64_shl:
    case Instr::i64_shr_s:
    case Instr::i64_shr_u:
    case Instr::i64_rotl:
    case Instr::i64_rotr:
        return -1;
    case Instr::local_get:
    case Instr::global_get:
        return 1;
    case Instr::local_set:
    case Instr::global_set:
        return -1;
    case Instr::local_tee:
        return 0;
    case Instr::end:
    case Instr::block:
    case Instr::loop:
    case Instr::if_:
    case Instr::else_:
    case Instr::br:
    case Instr::br_if:
    case Instr::call:
    case Instr::br_table:
    case Instr::call_indirect:
        // FIXME: these are special
        return 0;
    case Instr::i32_const:
    case Instr::i64_const:
        return 1;
    case Instr::i32_load:
    case Instr::i64_load:
    case Instr::i32_load8_s:
    case Instr::i32_load8_u:
    case Instr::i32_load16_s:
    case Instr::i32_load16_u:
    case Instr::i64_load8_s:
    case Instr::i64_load8_u:
    case Instr::i64_load16_s:
    case Instr::i64_load16_u:
    case Instr::i64_load32_s:
    case Instr::i64_load32_u:
        return 0;
    case Instr::i32_store:
    case Instr::i64_store:
    case Instr::i32_store8:
    case Instr::i32_store16:
    case Instr::i64_store8:
    case Instr::i64_store16:
    case Instr::i64_store32:
        return -1;
    case Instr::memory_size:
        return 1;
    case Instr::memory_grow:
        return 0;
    default:
        assert(false);
    }
    return 0;
}

}  // namespace fizzy
