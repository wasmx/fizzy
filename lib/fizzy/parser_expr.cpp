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
    b.resize(b.size() + sizeof(value));
    store(&b[b.size() - sizeof(value)], value);
}

struct LabelPosition
{
    Instr instruction = Instr::unreachable;  ///< The instruction that created the label.
    size_t immediates_offset{0};             ///< The immediates offset for block instructions.
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

    if (pos == end)
        throw parser_error{"Unexpected EOF"};

    const uint8_t type{*pos};

    if (type == BlockTypeEmpty)
        return {0, pos + 1};

    // Validate type.
    std::tie(std::ignore, pos) = parse<ValType>(pos, end);
    return {1, pos};
}
}  // namespace

parser_result<Code> parse_expr(const uint8_t* pos, const uint8_t* end)
{
    Code code;

    // The stack of labels allowing to distinguish between block/if/else and label instructions.
    // For a block/if/else instruction the value is the block/if/else's immediate offset.
    Stack<LabelPosition> label_positions;

    bool continue_parsing = true;
    while (continue_parsing)
    {
        if (pos == end)
            throw parser_error{"Unexpected EOF"};

        const auto instr = static_cast<Instr>(*pos++);
        switch (instr)
        {
        default:
            throw parser_error{"invalid instruction " + std::to_string(*(pos - 1))};

        // Floating point instructions are unsupported
        case Instr::f32_load:
        case Instr::f64_load:
        case Instr::f32_store:
        case Instr::f64_store:
        case Instr::f32_const:
        case Instr::f64_const:
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
            throw parser_error{
                "unsupported floating point instruction " + std::to_string(*(pos - 1))};

        case Instr::unreachable:
        case Instr::nop:
        case Instr::return_:
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

        case Instr::end:
        {
            if (!label_positions.empty())
            {
                const auto label_pos = label_positions.pop();
                if (label_pos.instruction != Instr::loop)  // If end of block/if/else instruction.
                {
                    const auto target_pc = static_cast<uint32_t>(code.instructions.size() + 1);
                    const auto target_imm = static_cast<uint32_t>(code.immediates.size());

                    // Set the imm values for block instruction.
                    auto* block_imm = code.immediates.data() + label_pos.immediates_offset;
                    store(block_imm, target_pc);
                    block_imm += sizeof(target_pc);
                    store(block_imm, target_imm);
                }
            }
            else
                continue_parsing = false;
            break;
        }

        case Instr::block:
        {
            uint8_t arity;
            std::tie(arity, pos) = parse_blocktype(pos, end);
            code.immediates.push_back(arity);

            // Push label with immediates offset after arity.
            label_positions.push_back({Instr::block, code.immediates.size()});

            // Placeholders for immediate values, filled at the matching end instruction.
            push(code.immediates, uint32_t{0});  // Diff to the end instruction.
            push(code.immediates, uint32_t{0});  // Diff for the immediates.
            break;
        }

        case Instr::loop:
        {
            std::tie(std::ignore, pos) = parse_blocktype(pos, end);
            label_positions.push_back({Instr::loop, 0});  // Mark as not interested.
            break;
        }

        case Instr::if_:
        {
            uint8_t arity;
            std::tie(arity, pos) = parse_blocktype(pos, end);
            code.immediates.push_back(arity);

            label_positions.push_back({Instr::if_, code.immediates.size()});

            // Placeholders for immediate values, filled at the matching end and else instructions.
            push(code.immediates, uint32_t{0});  // Diff to the end instruction.
            push(code.immediates, uint32_t{0});  // Diff for the immediates

            push(code.immediates, uint32_t{0});  // Diff to the else instruction
            push(code.immediates, uint32_t{0});  // Diff for the immediates.

            break;
        }

        case Instr::else_:
        {
            if (label_positions.empty())
                throw parser_error{"unexpected else instruction"};
            const auto label_pos = label_positions.peek();
            if (label_pos.instruction != Instr::if_)
                throw parser_error{"unexpected else instruction (if instruction missing)"};
            const auto target_pc = static_cast<uint32_t>(code.instructions.size() + 1);
            const auto target_imm = static_cast<uint32_t>(code.immediates.size());

            // Set the imm values for else instruction.
            auto* block_imm = code.immediates.data() + label_pos.immediates_offset +
                              sizeof(target_pc) + sizeof(target_imm);
            store(block_imm, target_pc);
            block_imm += sizeof(target_pc);
            store(block_imm, target_imm);

            break;
        }

        case Instr::local_get:
        case Instr::local_set:
        case Instr::local_tee:
        case Instr::global_get:
        case Instr::global_set:
        case Instr::br:
        case Instr::br_if:
        case Instr::call:
        {
            uint32_t imm;
            std::tie(imm, pos) = leb128u_decode<uint32_t>(pos, end);
            push(code.immediates, imm);
            break;
        }

        case Instr::br_table:
        {
            std::vector<uint32_t> label_indices;
            std::tie(label_indices, pos) = parse_vec<uint32_t>(pos, end);
            uint32_t default_label_idx;
            std::tie(default_label_idx, pos) = leb128u_decode<uint32_t>(pos, end);

            push(code.immediates, static_cast<uint32_t>(label_indices.size()));
            for (const auto idx : label_indices)
                push(code.immediates, idx);
            push(code.immediates, default_label_idx);
            break;
        }

        case Instr::call_indirect:
        {
            uint32_t imm;
            std::tie(imm, pos) = leb128u_decode<uint32_t>(pos, end);
            push(code.immediates, imm);

            if (pos == end)
                throw parser_error{"Unexpected EOF"};

            const uint8_t tableidx{*pos++};
            if (tableidx != 0)
                throw parser_error{"invalid tableidx encountered with call_indirect"};
            break;
        }

        case Instr::i32_const:
        {
            int32_t imm;
            std::tie(imm, pos) = leb128s_decode<int32_t>(pos, end);
            push(code.immediates, static_cast<uint32_t>(imm));
            break;
        }

        case Instr::i64_const:
        {
            int64_t imm;
            std::tie(imm, pos) = leb128s_decode<int64_t>(pos, end);
            push(code.immediates, static_cast<uint64_t>(imm));
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

            // offset
            uint32_t imm;
            std::tie(imm, pos) = leb128u_decode<uint32_t>(pos, end);
            push(code.immediates, imm);
            break;
        }
        case Instr::memory_size:
        case Instr::memory_grow:
        {
            if (pos == end)
                throw parser_error{"Unexpected EOF"};

            const uint8_t memory_idx{*pos++};
            if (memory_idx != 0)
                throw parser_error{"invalid memory index encountered"};
            break;
        }
        }
        code.instructions.emplace_back(instr);
    }
    assert(label_positions.empty());
    return {code, pos};
}
}  // namespace fizzy
