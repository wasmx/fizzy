#include "parser.hpp"
#include "leb128.hpp"
#include <cassert>

namespace fizzy
{
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
        default:
            break;
        }
        it += size;
    }

    return mod;
}
}  // namespace fizzy
