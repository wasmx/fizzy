#include "parser.hpp"
#include "leb128.hpp"
#include <cassert>

namespace fizzy
{
template <>
struct parser<FuncType>
{
    parser_result<FuncType> operator()(const uint8_t* pos)
    {
        if (*pos != 0x60)
            throw parser_error{
                "unexpected byte value " + std::to_string(*pos) + ", expected 0x60 for functype"};
        ++pos;

        FuncType result;
        std::tie(result.inputs, pos) = parser<std::vector<ValType>>{}(pos);
        std::tie(result.outputs, pos) = parser<std::vector<ValType>>{}(pos);
        return {result, pos};
    }
};

Module parse(bytes_view input)
{
    if (input.substr(0, wasm_prefix.size()) != wasm_prefix)
        throw parser_error{"invalid wasm module prefix"};

    input.remove_prefix(wasm_prefix.size());

    Module module;
    for (auto it = input.begin(); it != input.end();)
    {
        const auto id = static_cast<SectionId>(*it++);
        uint32_t size;
        std::tie(size, it) = leb128u_decode<uint32_t>(it);
        const auto expected_end_pos = it + size;
        switch (id)
        {
        case SectionId::type:
            std::tie(module.typesec, it) = parser<std::vector<FuncType>>{}(it);
            break;
        case SectionId::memory:
            std::tie(module.memorysec, it) = parser<std::vector<Memory>>{}(it);
            break;
        case SectionId::code:
            std::tie(module.codesec, it) = parser<std::vector<Code>>{}(it);
            break;
        case SectionId::custom:
        case SectionId::import:
        case SectionId::function:
        case SectionId::table:
        case SectionId::global:
        case SectionId::export_:
        case SectionId::start:
        case SectionId::element:
        case SectionId::data:
            // These sections are ignored for now.
            it += size;
            break;
        default:
            throw parser_error{
                "unknown section encountered " + std::to_string(static_cast<int>(id))};
        }

        if (it != expected_end_pos)
            throw parser_error{"incorrect section " + std::to_string(static_cast<int>(id)) +
                               " size, difference: " + std::to_string(it - expected_end_pos)};
    }

    return module;
}
}  // namespace fizzy
