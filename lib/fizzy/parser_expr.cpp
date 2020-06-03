// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "instructions.hpp"
#include "module.hpp"
#include "parser.hpp"
#include "stack.hpp"
#include <cassert>

namespace fizzy
{
namespace
{
template <typename T>
inline void store(uint8_t* dst, T value) noexcept
{
    __builtin_memcpy(dst, &value, sizeof(value));
}

template <typename T>
inline void push(bytes& b, T value)
{
    uint8_t storage[sizeof(T)];
    store(storage, value);
    b.append(storage, sizeof(storage));
}

/// The control frame to keep information about labels and blocks as defined in
/// Wasm Validation Algorithm https://webassembly.github.io/spec/core/appendix/algorithm.html.
struct ControlFrame
{
    /// The instruction that created the label.
    const Instr instruction{Instr::unreachable};

    /// Number of results returned from the frame.
    const uint8_t arity{0};

    /// The target instruction code offset.
    const size_t code_offset{0};

    /// The immediates offset for block instructions.
    /// Non-const because else block changes it (to resolve jumping over else block).
    size_t immediates_offset{0};

    /// The frame stack height of the parent frame.
    /// TODO: Storing this is not strictly required, as the parent frame is available
    ///       in the control_stack.
    const int parent_stack_height{0};

    /// The frame stack height.
    int stack_height{0};

    /// Whether the remainder of the block is unreachable (used to handle stack-polymorphic typing
    /// after branches).
    bool unreachable{false};

    /// Offsets of br/br_if/br_table instruction immediates, to be filled at the end of the block
    std::vector<size_t> br_immediate_offsets{};

    ControlFrame(Instr _instruction, uint8_t _arity, int _parent_stack_height,
        size_t _code_offset = 0, size_t _immediates_offset = 0) noexcept
      : instruction{_instruction},
        arity{_arity},
        code_offset{_code_offset},
        immediates_offset{_immediates_offset},
        parent_stack_height{_parent_stack_height},
        stack_height{_parent_stack_height}
    {}
};

/// Parses blocktype.
///
/// Spec: https://webassembly.github.io/spec/core/binary/types.html#binary-blocktype.
/// @return The type arity (can be 0 or 1).
parser_result<uint8_t> parse_blocktype(const uint8_t* pos, const uint8_t* end)
{
    // The byte meaning an empty wasm result type.
    // https://webassembly.github.io/spec/core/binary/types.html#result-types
    constexpr uint8_t BlockTypeEmpty = 0x40;

    uint8_t type;
    std::tie(type, pos) = parse_byte(pos, end);

    if (type == BlockTypeEmpty)
        return {0, pos};

    validate_valtype(type);
    return {1, pos};
}

void update_caller_frame(ControlFrame& frame, const FuncType& func_type)
{
    const auto stack_height_required = static_cast<int>(func_type.inputs.size());

    if (!frame.unreachable &&
        (frame.stack_height - frame.parent_stack_height) < stack_height_required)
        throw validation_error{"call/call_indirect instruction stack underflow"};

    const auto stack_height_change =
        static_cast<int>(func_type.outputs.size()) - stack_height_required;
    frame.stack_height += stack_height_change;
}

void validate_result_count(const ControlFrame& frame)
{
    if (frame.unreachable)
        return;

    // This is checked by "stack underflow".
    assert(frame.stack_height >= frame.parent_stack_height);

    if (frame.stack_height < frame.parent_stack_height + frame.arity)
        throw validation_error{"missing result"};

    // TODO: Enable "too many results" check when having information about number of function
    //       results.
    // if (frame.stack_height > frame.parent_stack_height + frame.arity)
    //     throw validation_error{"too many results"};
}

inline uint8_t get_branch_arity(const ControlFrame& frame) noexcept
{
    // For loops arity is considered always 0, because br executed in loop jumps to the top,
    // resetting frame stack to 0, so it should not keep top stack value even if loop has a result.
    return frame.instruction == Instr::loop ? 0 : frame.arity;
}

void push_branch_immediates(const ControlFrame& frame, bytes& immediates)
{
    // Push frame start location as br immediates - these are final if frame is loop,
    // but for block/if/else these are just placeholders, to be filled at end instruction.
    push(immediates, static_cast<uint32_t>(frame.code_offset));
    push(immediates, static_cast<uint32_t>(frame.immediates_offset));
    push(immediates, static_cast<uint32_t>(frame.parent_stack_height));
    push(immediates, get_branch_arity(frame));
}

}  // namespace

parser_result<Code> parse_expr(
    const uint8_t* pos, const uint8_t* end, FuncIdx func_idx, const Module& module)
{
    Code code;

    // The stack of control frames allowing to distinguish between block/if/else and label
    // instructions as defined in Wasm Validation Algorithm.
    Stack<ControlFrame> control_stack;

    const auto func_type_idx = module.funcsec[func_idx];
    assert(func_type_idx < module.typesec.size());
    const auto function_arity = static_cast<uint8_t>(module.typesec[func_type_idx].outputs.size());
    // The function's implicit block.
    control_stack.emplace(Instr::block, function_arity, 0);

    const auto metrics_table = get_instruction_metrics_table();

    bool continue_parsing = true;
    while (continue_parsing)
    {
        uint8_t opcode;
        std::tie(opcode, pos) = parse_byte(pos, end);

        auto& frame = control_stack.top();
        const auto& metrics = metrics_table[opcode];

        if (!frame.unreachable)
        {
            if ((frame.stack_height - frame.parent_stack_height) < metrics.stack_height_required)
                throw validation_error{"stack underflow"};

            // Update code's max_stack_height using frame.stack_height of the previous instruction.
            // At this point frame.stack_height includes additional changes to the stack height
            // if the previous instruction is a call/call_indirect.
            // This way the update is skipped for end/else instructions (because their frame is
            // already popped/reset), but it does not matter, as these instructions do not modify
            // stack height anyway.
            code.max_stack_height = std::max(code.max_stack_height, frame.stack_height);
        }

        frame.stack_height += metrics.stack_height_change;

        const auto instr = static_cast<Instr>(opcode);
        switch (instr)
        {
        default:
            throw parser_error{"invalid instruction " + std::to_string(*(pos - 1))};

        // Floating point instructions are unsupported
        case Instr::f32_load:
        case Instr::f64_load:
        case Instr::f32_store:
        case Instr::f64_store:
            if (!module.has_memory())
                throw validation_error{"memory instructions require imported or defined memory"};
            // alignment
            std::tie(std::ignore, pos) = leb128u_decode<uint32_t>(pos, end);
            // offset
            std::tie(std::ignore, pos) = leb128u_decode<uint32_t>(pos, end);
            break;
        case Instr::f32_const:
            pos = skip(4, pos, end);
            break;
        case Instr::f64_const:
            pos = skip(8, pos, end);
            break;
        case Instr::f32_eq:
        case Instr::f32_ne:
        case Instr::f32_lt:
        case Instr::f32_gt:
        case Instr::f32_le:
        case Instr::f32_ge:
        case Instr::f64_eq:
        case Instr::f64_ne:
        case Instr::f64_lt:
        case Instr::f64_gt:
        case Instr::f64_le:
        case Instr::f64_ge:
        case Instr::f32_abs:
        case Instr::f32_neg:
        case Instr::f32_ceil:
        case Instr::f32_floor:
        case Instr::f32_trunc:
        case Instr::f32_nearest:
        case Instr::f32_sqrt:
        case Instr::f32_add:
        case Instr::f32_sub:
        case Instr::f32_mul:
        case Instr::f32_div:
        case Instr::f32_min:
        case Instr::f32_max:
        case Instr::f32_copysign:
        case Instr::f64_abs:
        case Instr::f64_neg:
        case Instr::f64_ceil:
        case Instr::f64_floor:
        case Instr::f64_trunc:
        case Instr::f64_nearest:
        case Instr::f64_sqrt:
        case Instr::f64_add:
        case Instr::f64_sub:
        case Instr::f64_mul:
        case Instr::f64_div:
        case Instr::f64_min:
        case Instr::f64_max:
        case Instr::f64_copysign:
        case Instr::i32_trunc_f32_s:
        case Instr::i32_trunc_f32_u:
        case Instr::i32_trunc_f64_s:
        case Instr::i32_trunc_f64_u:
        case Instr::i64_trunc_f32_s:
        case Instr::i64_trunc_f32_u:
        case Instr::i64_trunc_f64_s:
        case Instr::i64_trunc_f64_u:
        case Instr::f32_convert_i32_s:
        case Instr::f32_convert_i32_u:
        case Instr::f32_convert_i64_s:
        case Instr::f32_convert_i64_u:
        case Instr::f32_demote_f64:
        case Instr::f64_convert_i32_s:
        case Instr::f64_convert_i32_u:
        case Instr::f64_convert_i64_s:
        case Instr::f64_convert_i64_u:
        case Instr::f64_promote_f32:
        case Instr::i32_reinterpret_f32:
        case Instr::i64_reinterpret_f64:
        case Instr::f32_reinterpret_i32:
        case Instr::f64_reinterpret_i64:

        case Instr::unreachable:
            frame.unreachable = true;
            break;

        case Instr::nop:
        case Instr::drop:
        case Instr::select:
        case Instr::i32_eq:
        case Instr::i32_eqz:
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
        case Instr::i64_eqz:
        case Instr::i64_ne:
        case Instr::i64_lt_s:
        case Instr::i64_lt_u:
        case Instr::i64_gt_s:
        case Instr::i64_gt_u:
        case Instr::i64_le_s:
        case Instr::i64_le_u:
        case Instr::i64_ge_s:
        case Instr::i64_ge_u:
        case Instr::i32_clz:
        case Instr::i32_ctz:
        case Instr::i32_popcnt:
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
        case Instr::i64_clz:
        case Instr::i64_ctz:
        case Instr::i64_popcnt:
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
        case Instr::i32_wrap_i64:
        case Instr::i64_extend_i32_s:
        case Instr::i64_extend_i32_u:
            break;

        case Instr::block:
        {
            uint8_t arity;
            std::tie(arity, pos) = parse_blocktype(pos, end);

            // Push label with immediates offset after arity.
            control_stack.emplace(Instr::block, arity, frame.stack_height, code.instructions.size(),
                code.immediates.size());
            break;
        }

        case Instr::loop:
        {
            uint8_t arity;
            std::tie(arity, pos) = parse_blocktype(pos, end);

            control_stack.emplace(Instr::loop, arity, frame.stack_height, code.instructions.size(),
                code.immediates.size());

            break;
        }

        case Instr::if_:
        {
            uint8_t arity;
            std::tie(arity, pos) = parse_blocktype(pos, end);

            control_stack.emplace(Instr::if_, arity, frame.stack_height, code.instructions.size(),
                code.immediates.size());

            // Placeholders for immediate values, filled at the matching end or else instructions.
            push(code.immediates, uint32_t{0});  // Diff to the else instruction
            push(code.immediates, uint32_t{0});  // Diff for the immediates.

            break;
        }

        case Instr::else_:
        {
            if (frame.instruction != Instr::if_)
                throw parser_error{"unexpected else instruction (if instruction missing)"};

            validate_result_count(frame);  // else is the end of if.

            // Reset frame after if. The if result type validation not implemented yet.
            frame.stack_height = frame.parent_stack_height;
            frame.unreachable = false;
            const auto if_imm_offset = frame.immediates_offset;
            frame.immediates_offset = code.immediates.size();

            // Placeholders for immediate values, filled at the matching end instructions.
            push(code.immediates, uint32_t{0});  // Diff to the end instruction.
            push(code.immediates, uint32_t{0});  // Diff for the immediates

            // Fill in if's immediates with offsets of first instruction in else block.
            const auto target_pc = static_cast<uint32_t>(code.instructions.size() + 1);
            const auto target_imm = static_cast<uint32_t>(code.immediates.size());

            // Set the imm values for else instruction.
            auto* if_imm = code.immediates.data() + if_imm_offset;
            store(if_imm, target_pc);
            if_imm += sizeof(target_pc);
            store(if_imm, target_imm);

            break;
        }

        case Instr::end:
        {
            validate_result_count(frame);

            if (frame.instruction != Instr::loop)  // If end of block/if/else instruction.
            {
                // In case it's an outermost implicit function block,
                // we want br to jump to the final end of the function.
                // Otherwise jump to the next instruction after block's end.
                const auto target_pc = control_stack.size() == 1 ?
                                           static_cast<uint32_t>(code.instructions.size()) :
                                           static_cast<uint32_t>(code.instructions.size() + 1);
                const auto target_imm = static_cast<uint32_t>(code.immediates.size());

                if (frame.instruction == Instr::if_)
                {
                    // We're at the end instruction of the if block without else or at the end of
                    // else block. Fill in if/else's immediates with offsets of first instruction
                    // after if/else block.
                    auto* if_imm = code.immediates.data() + frame.immediates_offset;
                    store(if_imm, target_pc);
                    if_imm += sizeof(target_pc);
                    store(if_imm, target_imm);
                }

                // Fill in immediates all br/br_table instructions jumping out of this block.
                for (const auto br_imm_offset : frame.br_immediate_offsets)
                {
                    auto* br_imm = code.immediates.data() + br_imm_offset;
                    store(br_imm, static_cast<uint32_t>(target_pc));
                    br_imm += sizeof(uint32_t);
                    store(br_imm, static_cast<uint32_t>(target_imm));
                    // parent stack height and arity were already stored in br handler
                }
            }
            const auto arity = frame.arity;
            control_stack.pop();  // Pop the current frame.

            if (control_stack.empty())
                continue_parsing = false;
            else
                control_stack.top().stack_height += arity;  // The results of the popped frame.
            break;
        }

        case Instr::br:
        case Instr::br_if:
        {
            uint32_t label_idx;
            std::tie(label_idx, pos) = leb128u_decode<uint32_t>(pos, end);

            if (label_idx >= control_stack.size())
                throw validation_error{"invalid label index"};

            // Remember this br immediates offset to fill it at end instruction.
            auto& branch_frame = control_stack[label_idx];
            branch_frame.br_immediate_offsets.push_back(code.immediates.size());

            push_branch_immediates(branch_frame, code.immediates);

            if (instr == Instr::br)
                frame.unreachable = true;

            break;
        }

        case Instr::br_table:
        {
            std::vector<uint32_t> label_indices;
            std::tie(label_indices, pos) = parse_vec_i32(pos, end);
            uint32_t default_label_idx;
            std::tie(default_label_idx, pos) = leb128u_decode<uint32_t>(pos, end);

            for (auto label_idx : label_indices)
            {
                if (label_idx >= control_stack.size())
                    throw validation_error{"invalid label index"};
            }

            if (default_label_idx >= control_stack.size())
                throw validation_error{"invalid label index"};

            auto& default_branch_frame = control_stack[default_label_idx];
            const auto default_branch_arity = get_branch_arity(default_branch_frame);

            // Remember immediates offset for all br items to fill them at end instruction.
            push(code.immediates, static_cast<uint32_t>(label_indices.size()));
            for (const auto idx : label_indices)
            {
                auto& branch_frame = control_stack[idx];

                if (get_branch_arity(branch_frame) != default_branch_arity)
                    throw validation_error{"br_table labels have inconsistent types"};

                branch_frame.br_immediate_offsets.push_back(code.immediates.size());
                push_branch_immediates(branch_frame, code.immediates);
            }

            default_branch_frame.br_immediate_offsets.push_back(code.immediates.size());
            push_branch_immediates(default_branch_frame, code.immediates);

            frame.unreachable = true;

            break;
        }

        case Instr::return_:
        {
            // return is identical to br MAX
            assert(!control_stack.empty());
            const uint32_t label_idx = static_cast<uint32_t>(control_stack.size() - 1);

            auto& branch_frame = control_stack[label_idx];
            branch_frame.br_immediate_offsets.push_back(code.immediates.size());

            push_branch_immediates(control_stack[label_idx], code.immediates);

            frame.unreachable = true;
            break;
        }

        case Instr::call:
        {
            FuncIdx callee_func_idx;
            std::tie(callee_func_idx, pos) = leb128u_decode<uint32_t>(pos, end);

            if (callee_func_idx >= module.imported_function_types.size() + module.funcsec.size())
                throw validation_error{"invalid funcidx encountered with call"};

            const auto& func_type = module.get_function_type(callee_func_idx);
            update_caller_frame(frame, func_type);

            push(code.immediates, callee_func_idx);
            break;
        }

        case Instr::call_indirect:
        {
            if (!module.has_table())
                throw validation_error{"call_indirect without defined table"};

            TypeIdx type_idx;
            std::tie(type_idx, pos) = leb128u_decode<uint32_t>(pos, end);

            if (type_idx >= module.typesec.size())
                throw validation_error{"invalid type index with call_indirect"};

            const auto& func_type = module.typesec[type_idx];
            update_caller_frame(frame, func_type);

            push(code.immediates, type_idx);

            if (pos == end)
                throw parser_error{"unexpected EOF"};

            const uint8_t tableidx{*pos++};
            if (tableidx != 0)
                throw parser_error{"invalid tableidx encountered with call_indirect"};
            break;
        }

        case Instr::local_get:
        case Instr::local_set:
        case Instr::local_tee:
        {
            uint32_t local_idx;
            std::tie(local_idx, pos) = leb128u_decode<uint32_t>(pos, end);
            push(code.immediates, local_idx);
            break;
        }

        case Instr::global_get:
        case Instr::global_set:
        {
            uint32_t global_idx;
            std::tie(global_idx, pos) = leb128u_decode<uint32_t>(pos, end);

            if (global_idx >= module.get_global_count())
                throw validation_error{"accessing global with invalid index"};

            if (instr == Instr::global_set && !module.is_global_mutable(global_idx))
                throw validation_error{"trying to mutate immutable global"};

            push(code.immediates, global_idx);
            break;
        }

        case Instr::i32_const:
        {
            int32_t value;
            std::tie(value, pos) = leb128s_decode<int32_t>(pos, end);
            push(code.immediates, static_cast<uint32_t>(value));
            break;
        }

        case Instr::i64_const:
        {
            int64_t value;
            std::tie(value, pos) = leb128s_decode<int64_t>(pos, end);
            push(code.immediates, static_cast<uint64_t>(value));
            break;
        }

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
        case Instr::i32_store:
        case Instr::i64_store:
        case Instr::i32_store8:
        case Instr::i32_store16:
        case Instr::i64_store8:
        case Instr::i64_store16:
        case Instr::i64_store32:
        {
            // alignment
            std::tie(std::ignore, pos) = leb128u_decode<uint32_t>(pos, end);

            uint32_t offset;
            std::tie(offset, pos) = leb128u_decode<uint32_t>(pos, end);
            push(code.immediates, offset);

            if (!module.has_memory())
                throw validation_error{"memory instructions require imported or defined memory"};
            break;
        }
        case Instr::memory_size:
        case Instr::memory_grow:
        {
            if (pos == end)
                throw parser_error{"unexpected EOF"};

            const uint8_t memory_idx{*pos++};
            if (memory_idx != 0)
                throw parser_error{"invalid memory index encountered"};

            if (!module.has_memory())
                throw validation_error{"memory instructions require imported or defined memory"};
            break;
        }
        }
        code.instructions.emplace_back(instr);
    }
    assert(control_stack.empty());
    return {code, pos};
}
}  // namespace fizzy
