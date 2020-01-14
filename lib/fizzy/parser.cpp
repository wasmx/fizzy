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

template <>
struct parser<ConstantExpression>
{
    parser_result<ConstantExpression> operator()(const uint8_t* pos)
    {
        ConstantExpression result;

        Instr instr;
        do
        {
            instr = static_cast<Instr>(*pos++);
            switch (instr)
            {
            default:
                throw parser_error{"unexpected instruction in the global initializer expression: " +
                                   std::to_string(*(pos - 1))};

            case Instr::end:
                break;

            case Instr::global_get:
            {
                result.init_type = GlobalInitType::global;
                std::tie(result.init.global_index, pos) = leb128u_decode<uint32_t>(pos);
                break;
            }

            case Instr::i32_const:
            {
                result.init_type = GlobalInitType::constant;
                int32_t value;
                std::tie(value, pos) = leb128s_decode<int32_t>(pos);
                result.init.value = static_cast<uint32_t>(value);
                break;
            }

            case Instr::i64_const:
            {
                result.init_type = GlobalInitType::constant;
                int64_t value;
                std::tie(value, pos) = leb128s_decode<int64_t>(pos);
                result.init.value = static_cast<uint64_t>(value);
                break;
            }
            }
        } while (instr != Instr::end);

        return {result, pos};
    }
};

template <>
struct parser<Global>
{
    parser_result<Global> operator()(const uint8_t* pos)
    {
        ValType type;
        std::tie(type, pos) = parser<ValType>{}(pos);

        if (*pos != 0x00 && *pos != 0x01)
            throw parser_error{"unexpected byte value " + std::to_string(*pos) +
                               ", expected 0x00 or 0x01 for global mutability"};

        Global result;
        result.is_mutable = (*pos == 0x01);
        ++pos;

        std::tie(result.expression, pos) = parser<ConstantExpression>{}(pos);

        return {result, pos};
    }
};

template <>
struct parser<Export>
{
    parser_result<Export> operator()(const uint8_t* pos)
    {
        std::vector<uint8_t> name;
        std::tie(name, pos) = parser<std::vector<uint8_t>>{}(pos);

        Export result;
        result.name.assign(name.begin(), name.end());

        const uint8_t type = *pos++;
        switch (type)
        {
        case 0x00:
            result.type = ExportType::Function;
            break;
        case 0x01:
            result.type = ExportType::Table;
            break;
        case 0x02:
            result.type = ExportType::Memory;
            break;
        case 0x03:
            result.type = ExportType::Global;
            break;
        default:
            throw parser_error{"unexpected export type value " + std::to_string(type)};
        }

        std::tie(result.index, pos) = leb128u_decode<uint32_t>(pos);

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
        case SectionId::function:
            std::tie(module.funcsec, it) = parser<std::vector<TypeIdx>>{}(it);
            break;
        case SectionId::memory:
            std::tie(module.memorysec, it) = parser<std::vector<Memory>>{}(it);
            break;
        case SectionId::global:
            std::tie(module.globalsec, it) = parser<std::vector<Global>>{}(it);
            break;
        case SectionId::export_:
            std::tie(module.exportsec, it) = parser<std::vector<Export>>{}(it);
            break;
        case SectionId::start:
            std::tie(module.startfunc, it) = leb128u_decode<uint32_t>(it);
            break;
        case SectionId::code:
            std::tie(module.codesec, it) = parser<std::vector<Code>>{}(it);
            break;
        case SectionId::custom:
        case SectionId::import:
        case SectionId::table:
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
