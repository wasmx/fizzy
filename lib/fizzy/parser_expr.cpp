#include "parser.hpp"

namespace fizzy
{
parser_result<code> parse_expr(const uint8_t* input)
{
    if (*input != 0x0b)
        throw parser_error{"parse_expr() not implemented"};

    code c;
    c.instructions.emplace_back(instr::end);
    ++input;
    return {c, input};
}
}  // namespace fizzy
