// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "cxx20/span.hpp"
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
inline void push(std::vector<uint8_t>& b, T value)
{
    uint8_t storage[sizeof(T)];
    store(storage, value);
    b.insert(b.end(), std::begin(storage), std::end(storage));
}

/// The control frame to keep information about labels and blocks as defined in
/// Wasm Validation Algorithm https://webassembly.github.io/spec/core/appendix/algorithm.html.
struct ControlFrame
{
    /// The instruction that created the label.
    const Instr instruction{Instr::unreachable};

    /// Return result type of the frame.
    const std::optional<ValType> type;

    /// The target instruction code offset.
    const size_t code_offset{0};

    /// The frame stack height of the parent frame.
    const int parent_stack_height{0};

    /// Whether the remainder of the block is unreachable (used to handle stack-polymorphic typing
    /// after branches).
    bool unreachable{false};

    /// Offsets of br/br_if/br_table instruction immediates, to be filled at the end of the block
    std::vector<size_t> br_immediate_offsets{};

    ControlFrame(Instr _instruction, std::optional<ValType> _type, int _parent_stack_height,
        size_t _code_offset = 0) noexcept
      : instruction{_instruction},
        type{_type},
        code_offset{_code_offset},
        parent_stack_height{_parent_stack_height}
    {}
};

enum class OperandStackType : uint8_t
{
    Unknown = 0,
    i32 = static_cast<uint8_t>(ValType::i32),
    i64 = static_cast<uint8_t>(ValType::i64),
    f32 = static_cast<uint8_t>(ValType::f32),
    f64 = static_cast<uint8_t>(ValType::f64),
};

inline OperandStackType from_valtype(ValType val_type) noexcept
{
    return static_cast<OperandStackType>(val_type);
}

inline bool type_matches(OperandStackType actual_type, ValType expected_type) noexcept
{
    if (actual_type == OperandStackType::Unknown)
        return true;

    return static_cast<ValType>(actual_type) == expected_type;
}

inline bool type_matches(OperandStackType actual_type, OperandStackType expected_type) noexcept
{
    if (expected_type == OperandStackType::Unknown || actual_type == OperandStackType::Unknown)
        return true;

    return expected_type == actual_type;
}

/// Parses blocktype.
///
/// Spec: https://webassembly.github.io/spec/core/binary/types.html#binary-blocktype.
/// @return optional type of the block result.
parser_result<std::optional<ValType>> parse_blocktype(const uint8_t* pos, const uint8_t* end)
{
    // The byte meaning an empty wasm result type.
    // https://webassembly.github.io/spec/core/binary/types.html#result-types
    constexpr uint8_t BlockTypeEmpty = 0x40;

    uint8_t type;
    std::tie(type, pos) = parse_byte(pos, end);

    if (type == BlockTypeEmpty)
        return {std::nullopt, pos};

    return {validate_valtype(type), pos};
}

void update_operand_stack(const ControlFrame& frame, Stack<OperandStackType>& operand_stack,
    span<const ValType> inputs, span<const ValType> outputs)
{
    const auto frame_stack_height = static_cast<int>(operand_stack.size());
    const auto inputs_size = static_cast<int>(inputs.size());

    // Stack is polymorphic after unreachable instruction: underflow is ignored,
    // but we need to count stack growth to detect extra values at the end of the block.
    if (!frame.unreachable && frame_stack_height < frame.parent_stack_height + inputs_size)
        throw validation_error{"stack underflow"};

    // Update operand_stack.
    for (auto it = inputs.rbegin(); it != inputs.rend(); ++it)
    {
        // Underflow is ignored for unreachable frame.
        if (frame.unreachable &&
            static_cast<int>(operand_stack.size()) == frame.parent_stack_height)
            break;

        const auto expected_type = *it;
        const auto actual_type = operand_stack.pop();
        if (!type_matches(actual_type, expected_type))
            throw validation_error{"type mismatch"};
    }
    // Push output values even if frame is unreachable.
    for (const auto output_type : outputs)
        operand_stack.push(from_valtype(output_type));
}

inline void drop_operand(const ControlFrame& frame, Stack<OperandStackType>& operand_stack,
    OperandStackType expected_type)
{
    if (!frame.unreachable &&
        static_cast<int>(operand_stack.size()) < frame.parent_stack_height + 1)
        throw validation_error{"stack underflow"};

    if (static_cast<int>(operand_stack.size()) == frame.parent_stack_height)
    {
        assert(frame.unreachable);  // implied from stack underflow check above
        return;
    }

    if (!type_matches(operand_stack.pop(), expected_type))
        throw validation_error{"type mismatch"};
}

inline void drop_operand(
    const ControlFrame& frame, Stack<OperandStackType>& operand_stack, ValType expected_type)
{
    return drop_operand(frame, operand_stack, from_valtype(expected_type));
}

void update_result_stack(const ControlFrame& frame, Stack<OperandStackType>& operand_stack)
{
    const auto frame_stack_height = static_cast<int>(operand_stack.size());

    // This is checked by "stack underflow".
    assert(frame_stack_height >= frame.parent_stack_height);

    const auto arity = frame.type.has_value() ? 1 : 0;

    if (frame_stack_height > frame.parent_stack_height + arity)
        throw validation_error{"too many results"};

    if (arity != 0)
        drop_operand(frame, operand_stack, from_valtype(*frame.type));
}

inline std::optional<ValType> get_branch_frame_type(const ControlFrame& frame) noexcept
{
    // For loops arity is considered always 0, because br executed in loop jumps to the top,
    // resetting frame stack to 0, so it should not keep top stack value even if loop has a result.
    return frame.instruction == Instr::loop ? std::nullopt : frame.type;
}

inline uint32_t get_branch_arity(const ControlFrame& frame) noexcept
{
    return get_branch_frame_type(frame).has_value() ? 1 : 0;
}

inline void update_branch_stack(const ControlFrame& current_frame, const ControlFrame& branch_frame,
    Stack<OperandStackType>& operand_stack)
{
    assert(static_cast<int>(operand_stack.size()) >= current_frame.parent_stack_height);

    const auto branch_frame_type = get_branch_frame_type(branch_frame);
    if (branch_frame_type.has_value())
        drop_operand(current_frame, operand_stack, from_valtype(*branch_frame_type));
}

void push_branch_immediates(
    const ControlFrame& branch_frame, int stack_height, std::vector<uint8_t>& instructions)
{
    // How many stack items to drop when taking the branch.
    const auto stack_drop = stack_height - branch_frame.parent_stack_height;

    // Push frame start location as br immediates - these are final if frame is loop,
    // but for block/if/else these are just placeholders, to be filled at end instruction.
    push(instructions, static_cast<uint32_t>(branch_frame.code_offset));
    push(instructions, static_cast<uint32_t>(stack_drop));
}

inline void mark_frame_unreachable(
    ControlFrame& frame, Stack<OperandStackType>& operand_stack) noexcept
{
    frame.unreachable = true;
    operand_stack.shrink(static_cast<size_t>(frame.parent_stack_height));
}

inline void push_operand(Stack<OperandStackType>& operand_stack, ValType type)
{
    operand_stack.push(from_valtype(type));
}

inline void push_operand(Stack<OperandStackType>& operand_stack, OperandStackType type)
{
    operand_stack.push(type);
}

ValType find_local_type(
    const std::vector<ValType>& params, const std::vector<Locals>& locals, LocalIdx idx)
{
    if (idx < params.size())
        return params[idx];

    // TODO: Consider more efficient algorithm with calculating cumulative sums only once and then
    // using binary search for each local instruction.
    const auto local_idx = idx - params.size();
    uint64_t local_count = 0;
    for (const auto& l : locals)
    {
        if (local_idx >= local_count && local_idx < local_count + l.count)
            return l.type;
        local_count += l.count;
    }

    throw validation_error{"invalid local index"};
}
}  // namespace

parser_result<Code> parse_expr(const uint8_t* pos, const uint8_t* end, FuncIdx func_idx,
    const std::vector<Locals>& locals, const Module& module)
{
    Code code;

    // The stack of control frames allowing to distinguish between block/if/else and label
    // instructions as defined in Wasm Validation Algorithm.
    Stack<ControlFrame> control_stack;

    Stack<OperandStackType> operand_stack;

    const auto func_type_idx = module.funcsec[func_idx];
    assert(func_type_idx < module.typesec.size());
    const auto& func_type = module.typesec[func_type_idx];

    const auto& func_inputs = func_type.inputs;
    const auto& func_outputs = func_type.outputs;
    // The function's implicit block.
    control_stack.emplace(Instr::block,
        func_outputs.empty() ? std::nullopt : std::optional<ValType>{func_outputs[0]}, 0);

    const auto type_table = get_instruction_type_table();
    const auto max_align_table = get_instruction_max_align_table();

    code.instructions.reserve(static_cast<size_t>(end - pos) * 3);

    bool continue_parsing = true;
    while (continue_parsing)
    {
        uint8_t opcode;
        std::tie(opcode, pos) = parse_byte(pos, end);

        auto& frame = control_stack.top();
        const auto& type = type_table[opcode];
        const auto max_align = max_align_table[opcode];

        // Update code's max_stack_height using frame.stack_height of the previous instruction.
        // At this point frame.stack_height includes additional changes to the stack height
        // if the previous instruction is a call/call_indirect.
        // This way the update is skipped for end/else instructions (because their frame is
        // already popped/reset), but it does not matter, as these instructions do not modify
        // stack height anyway.
        if (!frame.unreachable)
            code.max_stack_height =
                std::max(code.max_stack_height, static_cast<int>(operand_stack.size()));

        update_operand_stack(frame, operand_stack, type.inputs, type.outputs);

        const auto instr = static_cast<Instr>(opcode);
        switch (instr)
        {
        default:
            throw parser_error{"invalid instruction " + std::to_string(*(pos - 1))};

        case Instr::unreachable:
            mark_frame_unreachable(frame, operand_stack);
            break;

        case Instr::drop:
            drop_operand(frame, operand_stack, OperandStackType::Unknown);
            break;

        case Instr::select:
        {
            const auto frame_stack_height = static_cast<int>(operand_stack.size());

            // Two operands are expected, because the selector operand was already popped
            // according to instruction type table
            if (!frame.unreachable && frame_stack_height < frame.parent_stack_height + 2)
                throw validation_error{"stack underflow"};


            const auto operand_type = frame_stack_height > frame.parent_stack_height ?
                                          operand_stack[0] :
                                          OperandStackType::Unknown;

            drop_operand(frame, operand_stack, operand_type);
            drop_operand(frame, operand_stack, operand_type);
            push_operand(operand_stack, operand_type);
            break;
        }

        case Instr::nop:
        case Instr::i32_eqz:
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
        case Instr::i64_eqz:
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
        case Instr::i32_wrap_i64:
        case Instr::i32_trunc_f32_s:
        case Instr::i32_trunc_f32_u:
        case Instr::i32_trunc_f64_s:
        case Instr::i32_trunc_f64_u:
        case Instr::i64_extend_i32_s:
        case Instr::i64_extend_i32_u:
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
            break;

        case Instr::block:
        {
            std::optional<ValType> block_type;
            std::tie(block_type, pos) = parse_blocktype(pos, end);

            // Push label with immediates offset after arity.
            control_stack.emplace(Instr::block, block_type, static_cast<int>(operand_stack.size()),
                code.instructions.size());
            break;
        }

        case Instr::loop:
        {
            std::optional<ValType> loop_type;
            std::tie(loop_type, pos) = parse_blocktype(pos, end);

            control_stack.emplace(Instr::loop, loop_type, static_cast<int>(operand_stack.size()),
                code.instructions.size());
            break;
        }

        case Instr::if_:
        {
            std::optional<ValType> if_type;
            std::tie(if_type, pos) = parse_blocktype(pos, end);

            control_stack.emplace(Instr::if_, if_type, static_cast<int>(operand_stack.size()),
                code.instructions.size());

            // Placeholders for immediate values, filled at the matching end or else instructions.
            code.instructions.push_back(opcode);
            push(code.instructions, uint32_t{0});  // Diff to the else instruction
            continue;
        }

        case Instr::else_:
        {
            if (frame.instruction != Instr::if_)
                throw parser_error{"unexpected else instruction (if instruction missing)"};

            update_result_stack(frame, operand_stack);  // else is the end of if.

            const auto if_imm_offset = frame.code_offset + 1;
            const auto frame_type = frame.type;
            auto frame_br_immediate_offsets = std::move(frame.br_immediate_offsets);

            control_stack.pop();
            control_stack.emplace(Instr::else_, frame_type, static_cast<int>(operand_stack.size()),
                code.instructions.size());
            // br immediates from `then` branch will need to be filled at the end of `else`
            control_stack.top().br_immediate_offsets = std::move(frame_br_immediate_offsets);

            // Placeholders for immediate values, filled at the matching end instructions.
            code.instructions.push_back(opcode);
            push(code.instructions, uint32_t{0});  // Diff to the end instruction.

            // Fill in if's immediates with offsets of first instruction in else block.
            const auto target_pc = static_cast<uint32_t>(code.instructions.size());

            // Set the imm values for if instruction.
            auto* if_imm = code.instructions.data() + if_imm_offset;
            store(if_imm, target_pc);
            continue;
        }

        case Instr::end:
        {
            update_result_stack(frame, operand_stack);

            if (frame.type.has_value() && frame.instruction == Instr::if_)
                throw validation_error{"missing result in else branch"};

            if (frame.instruction != Instr::loop)  // If end of block/if/else instruction.
            {
                // In case it's an outermost implicit function block,
                // we want br to jump to the final end of the function.
                // Otherwise jump to the next instruction after block's end.
                const auto target_pc = control_stack.size() == 1 ?
                                           static_cast<uint32_t>(code.instructions.size()) :
                                           static_cast<uint32_t>(code.instructions.size() + 1);

                if (frame.instruction == Instr::if_ || frame.instruction == Instr::else_)
                {
                    // We're at the end instruction of the if block without else or at the end of
                    // else block. Fill in if/else's immediates with offsets of first instruction
                    // after if/else block.
                    auto* if_imm = code.instructions.data() + frame.code_offset + 1;
                    store(if_imm, target_pc);
                }

                // Fill in immediates all br/br_table instructions jumping out of this block.
                for (const auto br_imm_offset : frame.br_immediate_offsets)
                {
                    auto* br_imm = code.instructions.data() + br_imm_offset;
                    store(br_imm, static_cast<uint32_t>(target_pc));
                    // stack drop and arity were already stored in br handler
                }
            }
            const auto frame_type = frame.type;
            operand_stack.shrink(static_cast<size_t>(frame.parent_stack_height));
            control_stack.pop();  // Pop the current frame.

            if (control_stack.empty())
                continue_parsing = false;
            else if (frame_type.has_value())
                push_operand(operand_stack, *frame_type);
            break;
        }

        case Instr::br:
        case Instr::br_if:
        {
            uint32_t label_idx;
            std::tie(label_idx, pos) = leb128u_decode<uint32_t>(pos, end);

            if (label_idx >= control_stack.size())
                throw validation_error{"invalid label index"};

            auto& branch_frame = control_stack[label_idx];

            update_branch_stack(frame, branch_frame, operand_stack);

            code.instructions.push_back(opcode);
            push(code.instructions, get_branch_arity(branch_frame));

            // Remember this br immediates offset to fill it at end instruction.
            branch_frame.br_immediate_offsets.push_back(code.instructions.size());

            push_branch_immediates(
                branch_frame, static_cast<int>(operand_stack.size()), code.instructions);

            if (instr == Instr::br)
                mark_frame_unreachable(frame, operand_stack);
            else
            {
                // For the case when branch is not taken for br_if,
                // we push back the block result value, that was popped in update_branch_stack.
                const auto branch_frame_type = get_branch_frame_type(branch_frame);
                if (branch_frame_type.has_value())
                    push_operand(operand_stack, *branch_frame.type);
            }

            continue;
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

            code.instructions.push_back(opcode);
            push(code.instructions, static_cast<uint32_t>(label_indices.size()));

            auto& default_branch_frame = control_stack[default_label_idx];
            const auto default_branch_type = get_branch_frame_type(default_branch_frame);

            update_branch_stack(frame, default_branch_frame, operand_stack);

            // arity is the same for all indices, so we push it once
            push(code.instructions, get_branch_arity(default_branch_frame));

            // Remember immediates offset for all br items to fill them at end instruction.
            for (const auto idx : label_indices)
            {
                auto& branch_frame = control_stack[idx];

                if (get_branch_frame_type(branch_frame) != default_branch_type)
                    throw validation_error{"br_table labels have inconsistent types"};

                branch_frame.br_immediate_offsets.push_back(code.instructions.size());
                push_branch_immediates(
                    branch_frame, static_cast<int>(operand_stack.size()), code.instructions);
            }
            default_branch_frame.br_immediate_offsets.push_back(code.instructions.size());
            push_branch_immediates(
                default_branch_frame, static_cast<int>(operand_stack.size()), code.instructions);

            mark_frame_unreachable(frame, operand_stack);

            continue;
        }

        case Instr::return_:
        {
            // return is identical to br MAX
            assert(!control_stack.empty());
            const uint32_t label_idx = static_cast<uint32_t>(control_stack.size() - 1);

            auto& branch_frame = control_stack[label_idx];

            update_branch_stack(frame, branch_frame, operand_stack);

            code.instructions.push_back(opcode);
            push(code.instructions, get_branch_arity(branch_frame));

            branch_frame.br_immediate_offsets.push_back(code.instructions.size());

            push_branch_immediates(
                branch_frame, static_cast<int>(operand_stack.size()), code.instructions);

            mark_frame_unreachable(frame, operand_stack);
            continue;
        }

        case Instr::call:
        {
            FuncIdx callee_func_idx;
            std::tie(callee_func_idx, pos) = leb128u_decode<uint32_t>(pos, end);

            if (callee_func_idx >= module.imported_function_types.size() + module.funcsec.size())
                throw validation_error{"invalid funcidx encountered with call"};

            const auto& callee_func_type = module.get_function_type(callee_func_idx);
            update_operand_stack(
                frame, operand_stack, callee_func_type.inputs, callee_func_type.outputs);

            code.instructions.push_back(opcode);
            push(code.instructions, callee_func_idx);
            continue;
        }

        case Instr::call_indirect:
        {
            if (!module.has_table())
                throw validation_error{"call_indirect without defined table"};

            TypeIdx callee_type_idx;
            std::tie(callee_type_idx, pos) = leb128u_decode<uint32_t>(pos, end);

            if (callee_type_idx >= module.typesec.size())
                throw validation_error{"invalid type index with call_indirect"};

            const auto& callee_func_type = module.typesec[callee_type_idx];
            update_operand_stack(
                frame, operand_stack, callee_func_type.inputs, callee_func_type.outputs);

            uint8_t table_idx;
            std::tie(table_idx, pos) = parse_byte(pos, end);
            if (table_idx != 0)
                throw parser_error{"invalid tableidx encountered with call_indirect"};

            code.instructions.push_back(opcode);
            push(code.instructions, callee_type_idx);
            continue;
        }

        case Instr::local_get:
        {
            LocalIdx local_idx;
            std::tie(local_idx, pos) = leb128u_decode<uint32_t>(pos, end);

            push_operand(operand_stack, find_local_type(func_inputs, locals, local_idx));

            code.instructions.push_back(opcode);
            push(code.instructions, local_idx);
            continue;
        }

        case Instr::local_set:
        {
            LocalIdx local_idx;
            std::tie(local_idx, pos) = leb128u_decode<uint32_t>(pos, end);

            drop_operand(frame, operand_stack, find_local_type(func_inputs, locals, local_idx));

            code.instructions.push_back(opcode);
            push(code.instructions, local_idx);
            continue;
        }

        case Instr::local_tee:
        {
            LocalIdx local_idx;
            std::tie(local_idx, pos) = leb128u_decode<uint32_t>(pos, end);

            const auto local_type = find_local_type(func_inputs, locals, local_idx);
            drop_operand(frame, operand_stack, local_type);
            push_operand(operand_stack, local_type);

            code.instructions.push_back(opcode);
            push(code.instructions, local_idx);
            continue;
        }

        case Instr::global_get:
        {
            GlobalIdx global_idx;
            std::tie(global_idx, pos) = leb128u_decode<uint32_t>(pos, end);

            if (global_idx >= module.get_global_count())
                throw validation_error{"accessing global with invalid index"};

            push_operand(operand_stack, module.get_global_type(global_idx).value_type);

            code.instructions.push_back(opcode);
            push(code.instructions, global_idx);
            continue;
        }

        case Instr::global_set:
        {
            GlobalIdx global_idx;
            std::tie(global_idx, pos) = leb128u_decode<uint32_t>(pos, end);

            if (global_idx >= module.get_global_count())
                throw validation_error{"accessing global with invalid index"};

            if (!module.get_global_type(global_idx).is_mutable)
                throw validation_error{"trying to mutate immutable global"};

            drop_operand(frame, operand_stack, module.get_global_type(global_idx).value_type);

            code.instructions.push_back(opcode);
            push(code.instructions, global_idx);
            continue;
        }

        case Instr::i32_const:
        {
            int32_t value;
            std::tie(value, pos) = leb128s_decode<int32_t>(pos, end);
            code.instructions.push_back(opcode);
            push(code.instructions, static_cast<uint32_t>(value));
            continue;
        }

        case Instr::i64_const:
        {
            int64_t value;
            std::tie(value, pos) = leb128s_decode<int64_t>(pos, end);
            code.instructions.push_back(opcode);
            push(code.instructions, static_cast<uint64_t>(value));
            continue;
        }

        case Instr::f32_const:
        {
            uint32_t value;
            std::tie(value, pos) = parse_value<uint32_t>(pos, end);
            code.instructions.push_back(opcode);
            push(code.instructions, value);
            continue;
        }

        case Instr::f64_const:
        {
            uint64_t value;
            std::tie(value, pos) = parse_value<uint64_t>(pos, end);
            code.instructions.push_back(opcode);
            push(code.instructions, value);
            continue;
        }

        case Instr::i32_load:
        case Instr::i64_load:
        case Instr::f32_load:
        case Instr::f32_store:
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
        case Instr::f64_load:
        case Instr::f64_store:
        case Instr::i32_store8:
        case Instr::i32_store16:
        case Instr::i64_store8:
        case Instr::i64_store16:
        case Instr::i64_store32:
        {
            uint32_t align;
            std::tie(align, pos) = leb128u_decode<uint32_t>(pos, end);
            // NOTE: [0, 3] is the correct range (the hard limit is log2(64 / 8)) and checking it to
            // avoid overflows
            if (align > max_align)
                throw validation_error{"alignment cannot exceed operand size"};

            uint32_t offset;
            std::tie(offset, pos) = leb128u_decode<uint32_t>(pos, end);
            code.instructions.push_back(opcode);
            push(code.instructions, offset);

            if (!module.has_memory())
                throw validation_error{"memory instructions require imported or defined memory"};
            continue;
        }

        case Instr::memory_size:
        case Instr::memory_grow:
        {
            uint8_t memory_idx;
            std::tie(memory_idx, pos) = parse_byte(pos, end);
            if (memory_idx != 0)
                throw parser_error{"invalid memory index encountered"};

            if (!module.has_memory())
                throw validation_error{"memory instructions require imported or defined memory"};
            break;
        }
        }
        code.instructions.emplace_back(opcode);
    }
    assert(control_stack.empty());
    return {code, pos};
}
}  // namespace fizzy
