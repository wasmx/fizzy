#include "parser.hpp"
#include "leb128.hpp"
#include "types.hpp"
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

std::tuple<bool, const uint8_t*> parseGlobalType(const uint8_t* pos)
{
    // will throw if invalid type
    std::tie(std::ignore, pos) = parser<ValType>{}(pos);

    if (*pos != 0x00 && *pos != 0x01)
        throw parser_error{"unexpected byte value " + std::to_string(*pos) +
                           ", expected 0x00 or 0x01 for global mutability"};
    const bool is_mutable = (*pos == 0x01);
    ++pos;
    return {is_mutable, pos};
}

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
                result.kind = ConstantExpression::Kind::GlobalGet;
                std::tie(result.value.global_index, pos) = leb128u_decode<uint32_t>(pos);
                break;
            }

            case Instr::i32_const:
            {
                result.kind = ConstantExpression::Kind::Constant;
                int32_t value;
                std::tie(value, pos) = leb128s_decode<int32_t>(pos);
                result.value.constant = static_cast<uint32_t>(value);
                break;
            }

            case Instr::i64_const:
            {
                result.kind = ConstantExpression::Kind::Constant;
                int64_t value;
                std::tie(value, pos) = leb128s_decode<int64_t>(pos);
                result.value.constant = static_cast<uint64_t>(value);
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
        Global result;
        std::tie(result.is_mutable, pos) = parseGlobalType((pos));
        std::tie(result.expression, pos) = parser<ConstantExpression>{}(pos);

        return {result, pos};
    }
};

template <>
struct parser<std::string>
{
    parser_result<std::string> operator()(const uint8_t* pos)
    {
        std::vector<uint8_t> value;
        std::tie(value, pos) = parser<std::vector<uint8_t>>{}(pos);

        // FIXME: need to validate that string is a valid UTF-8

        return {std::string(value.begin(), value.end()), pos};
    }
};

template <>
struct parser<Import>
{
    parser_result<Import> operator()(const uint8_t* pos)
    {
        Import result{};
        std::tie(result.module, pos) = parser<std::string>{}(pos);
        std::tie(result.name, pos) = parser<std::string>{}(pos);

        const uint8_t kind = *pos++;
        switch (kind)
        {
        case 0x00:
            result.kind = ExternalKind::Function;
            std::tie(result.desc.function_type_index, pos) = leb128u_decode<uint32_t>(pos);
            break;
        case 0x01:
            result.kind = ExternalKind::Table;
            throw parser_error{"importing Tables is not implemented"};
            break;
        case 0x02:
            result.kind = ExternalKind::Memory;
            std::tie(result.desc.memory, pos) = parser<Memory>{}(pos);
            break;
        case 0x03:
            result.kind = ExternalKind::Global;
            std::tie(result.desc.global_mutable, pos) = parseGlobalType((pos));
            break;
        default:
            throw parser_error{"unexpected import kind value " + std::to_string(kind)};
        }

        return {result, pos};
    }
};

template <>
struct parser<Export>
{
    parser_result<Export> operator()(const uint8_t* pos)
    {
        Export result;
        std::tie(result.name, pos) = parser<std::string>{}(pos);

        const uint8_t kind = *pos++;
        switch (kind)
        {
        case 0x00:
            result.kind = ExternalKind::Function;
            break;
        case 0x01:
            result.kind = ExternalKind::Table;
            break;
        case 0x02:
            result.kind = ExternalKind::Memory;
            break;
        case 0x03:
            result.kind = ExternalKind::Global;
            break;
        default:
            throw parser_error{"unexpected export kind value " + std::to_string(kind)};
        }

        std::tie(result.index, pos) = leb128u_decode<uint32_t>(pos);

        return {result, pos};
    }
};

template <>
struct parser<Table>
{
    parser_result<Table> operator()(const uint8_t* pos)
    {
        const uint8_t kind = *pos++;
        if (kind != FuncRef)
            throw parser_error{"unexpected table elemtype: " + std::to_string(kind)};

        Limits limits;
        std::tie(limits, pos) = parser<Limits>{}(pos);

        return {{limits}, pos};
    }
};

template <>
struct parser<Data>
{
    parser_result<Data> operator()(const uint8_t* pos)
    {
        MemIdx memory_index;
        std::tie(memory_index, pos) = leb128u_decode<uint32_t>(pos);
        if (memory_index != 0)
            throw parser_error{"unexpected memidx value " + std::to_string(memory_index)};

        ConstantExpression offset;
        std::tie(offset, pos) = parser<ConstantExpression>{}(pos);

        std::vector<uint8_t> init;
        std::tie(init, pos) = parser<std::vector<uint8_t>>{}(pos);

        return {{offset, bytes(init.data(), init.size())}, pos};
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
        case SectionId::import:
            std::tie(module.importsec, it) = parser<std::vector<Import>>{}(it);
            break;
        case SectionId::function:
            std::tie(module.funcsec, it) = parser<std::vector<TypeIdx>>{}(it);
            break;
        case SectionId::table:
            std::tie(module.tablesec, it) = parser<std::vector<Table>>{}(it);
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
        case SectionId::data:
            std::tie(module.datasec, it) = parser<std::vector<Data>>{}(it);
            break;
        case SectionId::custom:
        case SectionId::element:
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

        if (module.tablesec.size() > 1)
            throw parser_error{"too many table sections (at most one is allowed)"};

        if (module.memorysec.size() > 1)
            throw parser_error{"too many memory sections (at most one is allowed)"};
    }

    return module;
}
}  // namespace fizzy
