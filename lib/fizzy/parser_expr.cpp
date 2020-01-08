#include "parser.hpp"

namespace fizzy
{
namespace
{
template <typename T>
inline void push(bytes& b, T value)
{
    b.resize(b.size() + sizeof(value));
    __builtin_memcpy(&b[b.size() - sizeof(value)], &value, sizeof(value));
}
}  // namespace

parser_result<Code> parse_expr(const uint8_t* pos)
{
    Code code;
    Instr instr;
    do
    {
        instr = static_cast<Instr>(*pos++);
        switch (instr)
        {
        default:
            throw parser_error{"invalid instruction " + std::to_string(*pos)};

        case Instr::unreachable:
        case Instr::nop:
        case Instr::end:
        case Instr::drop:
        case Instr::select:
        case Instr::i32_eq:
        case Instr::i32_eqz:
        case Instr::i32_ne:
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
        case Instr::i32_wrap_i64:
        case Instr::i64_extend_i32_s:
        case Instr::i64_extend_i32_u:
            break;

        case Instr::local_get:
        case Instr::local_set:
        case Instr::local_tee:
        {
            uint32_t imm;
            std::tie(imm, pos) = leb128u_decode<uint32_t>(pos);
            push(code.immediates, imm);
            break;
        }
        }
        code.instructions.emplace_back(instr);
    } while (instr != Instr::end);
    return {code, pos};
}
}  // namespace fizzy
