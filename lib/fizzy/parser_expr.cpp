#include "parser.hpp"
#include "stack.hpp"

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
}  // namespace

parser_result<Code> parse_expr(const uint8_t* pos)
{
    Code code;

    // The stack of labels allowing to distinguish between block/if/else and label instructions.
    // For a block/if/else instruction the value is the block/if/else's immediate offset.
    Stack<LabelPosition> label_positions;

    bool continue_parsing = true;
    while (continue_parsing)
    {
        const auto instr = static_cast<Instr>(*pos++);
        switch (instr)
        {
        default:
            throw parser_error{"invalid instruction " + std::to_string(*(pos - 1))};

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
            const uint8_t type{*pos};
            uint8_t arity;
            if (type == BlockTypeEmpty)
            {
                arity = 0;
                ++pos;
            }
            else
            {
                std::tie(std::ignore, pos) = parser<ValType>{}(pos);  // Report incorrect type.
                arity = 1;
            }

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
            const uint8_t type{*pos};
            if (type == BlockTypeEmpty)
                ++pos;
            else
                std::tie(std::ignore, pos) = parser<ValType>{}(pos);  // Report incorrect type.
            label_positions.push_back({Instr::loop, 0});              // Mark as not interested.
            break;
        }

        case Instr::if_:
        {
            const uint8_t type{*pos};
            uint8_t arity;
            if (type == BlockTypeEmpty)
            {
                arity = 0;
                ++pos;
            }
            else
            {
                // Will throw in case of incorrect type.
                std::tie(std::ignore, pos) = parser<ValType>{}(pos);
                arity = 1;
            }

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
            std::tie(imm, pos) = leb128u_decode<uint32_t>(pos);
            push(code.immediates, imm);
            break;
        }

        case Instr::br_table:
        {
            std::vector<uint32_t> label_indices;
            std::tie(label_indices, pos) = parser<std::vector<uint32_t>>{}(pos);
            uint32_t default_label_idx;
            std::tie(default_label_idx, pos) = leb128u_decode<uint32_t>(pos);

            push(code.immediates, static_cast<uint32_t>(label_indices.size()));
            for (const auto idx : label_indices)
                push(code.immediates, idx);
            push(code.immediates, default_label_idx);
            break;
        }

        case Instr::call_indirect:
        {
            uint32_t imm;
            std::tie(imm, pos) = leb128u_decode<uint32_t>(pos);
            push(code.immediates, imm);

            const uint8_t tableidx{*pos++};
            if (tableidx != 0)
                throw parser_error{"invalid tableidx encountered with call_indirect"};
            break;
        }

        case Instr::i32_const:
        {
            int32_t imm;
            std::tie(imm, pos) = leb128s_decode<int32_t>(pos);
            push(code.immediates, static_cast<uint32_t>(imm));
            break;
        }

        case Instr::i64_const:
        {
            int64_t imm;
            std::tie(imm, pos) = leb128s_decode<int64_t>(pos);
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
            std::tie(std::ignore, pos) = leb128u_decode<uint32_t>(pos);

            // offset
            uint32_t imm;
            std::tie(imm, pos) = leb128u_decode<uint32_t>(pos);
            push(code.immediates, imm);
            break;
        }
        case Instr::memory_size:
        case Instr::memory_grow:
        {
            const uint8_t memory_idx{*pos++};
            if (memory_idx != 0)
                throw parser_error{"invalid memory index encountered"};
            break;
        }
        }
        code.instructions.emplace_back(instr);
    }
    if (!label_positions.empty())
        throw parser_error{"code is not balanced (missing end statements)"};
    return {code, pos};
}
}  // namespace fizzy
