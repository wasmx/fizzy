#include "parser.hpp"
#include "leb128.hpp"
#include <cassert>

namespace fizzy
{
template <>
struct parser<functype>
{
    parser_result<functype> operator()(const uint8_t* pos)
    {
        if (*pos != 0x60)
            throw parser_error{
                "unexpected byte value " + std::to_string(*pos) + ", expected 0x60 for functype"};
        ++pos;

        functype result;
        std::tie(result.inputs, pos) = parser<std::vector<valtype>>{}(pos);
        std::tie(result.outputs, pos) = parser<std::vector<valtype>>{}(pos);
        return {result, pos};
    }
};

module parse(bytes_view input)
{
    if (input.substr(0, wasm_prefix.size()) != wasm_prefix)
        throw parser_error{"invalid wasm module prefix"};

    input.remove_prefix(wasm_prefix.size());

    module mod;
    for (auto it = input.begin(); it != input.end();)
    {
        const auto id = static_cast<sectionid>(*it++);
        const auto [size, new_pos] = leb128u_decode<uint32_t>(it);
        it = new_pos;
        switch (id)
        {
        case sectionid::type:
            std::tie(mod.typesec, std::ignore) = parser<std::vector<functype>>{}(it);
            break;
        default:
            break;
        }
        it += size;
    }

    return mod;
}
}  // namespace fizzy
