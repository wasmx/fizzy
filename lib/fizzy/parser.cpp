#include "parser.hpp"
#include "leb128.hpp"
#include "types.hpp"
#include <algorithm>

namespace fizzy
{
template <>
inline parser_result<uint8_t> parse(const uint8_t* pos)
{
    const auto result = *pos;
    ++pos;
    return {result, pos};
}

template <>
inline parser_result<FuncType> parse(const uint8_t* pos)
{
    if (*pos != 0x60)
        throw parser_error{
            "unexpected byte value " + std::to_string(*pos) + ", expected 0x60 for functype"};
    ++pos;

    FuncType result;
    std::tie(result.inputs, pos) = parse_vec<ValType>(pos);
    std::tie(result.outputs, pos) = parse_vec<ValType>(pos);
    return {result, pos};
}

std::tuple<bool, const uint8_t*> parse_global_type(const uint8_t* pos)
{
    // will throw if invalid type
    std::tie(std::ignore, pos) = parse<ValType>(pos);

    if (*pos != 0x00 && *pos != 0x01)
        throw parser_error{"unexpected byte value " + std::to_string(*pos) +
                           ", expected 0x00 or 0x01 for global mutability"};
    const bool is_mutable = (*pos == 0x01);
    ++pos;
    return {is_mutable, pos};
}

inline parser_result<ConstantExpression> parse_constant_expression(const uint8_t* pos)
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
            std::tie(result.value.global_index, pos) =
                leb128u_decode<uint32_t>(pos, pos + 4);  // FIXME: Bounds checking.
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

template <>
inline parser_result<Global> parse(const uint8_t* pos)
{
    Global result;
    std::tie(result.is_mutable, pos) = parse_global_type((pos));
    std::tie(result.expression, pos) = parse_constant_expression(pos);

    return {result, pos};
}

template <>
inline parser_result<Table> parse(const uint8_t* pos)
{
    const uint8_t kind = *pos++;
    if (kind != FuncRef)
        throw parser_error{"unexpected table elemtype: " + std::to_string(kind)};

    Limits limits;
    std::tie(limits, pos) = parse_limits(pos);

    return {{limits}, pos};
}

template <>
inline parser_result<Memory> parse(const uint8_t* pos)
{
    Limits limits;
    std::tie(limits, pos) = parse_limits(pos);
    return {{limits}, pos};
}

inline parser_result<std::string> parse_string(const uint8_t* pos)
{
    std::vector<uint8_t> value;
    std::tie(value, pos) = parse_vec<uint8_t>(pos);

    // FIXME: need to validate that string is a valid UTF-8

    return {std::string(value.begin(), value.end()), pos};
}

template <>
inline parser_result<Import> parse(const uint8_t* pos)
{
    Import result{};
    std::tie(result.module, pos) = parse_string(pos);
    std::tie(result.name, pos) = parse_string(pos);

    const uint8_t kind = *pos++;
    switch (kind)
    {
    case 0x00:
        result.kind = ExternalKind::Function;
        std::tie(result.desc.function_type_index, pos) =
            leb128u_decode<uint32_t>(pos, pos + 4);  // FIXME: Bounds checking.
        break;
    case 0x01:
        result.kind = ExternalKind::Table;
        std::tie(result.desc.table, pos) = parse<Table>(pos);
        break;
    case 0x02:
        result.kind = ExternalKind::Memory;
        std::tie(result.desc.memory, pos) = parse<Memory>(pos);
        break;
    case 0x03:
        result.kind = ExternalKind::Global;
        std::tie(result.desc.global_mutable, pos) = parse_global_type(pos);
        break;
    default:
        throw parser_error{"unexpected import kind value " + std::to_string(kind)};
    }

    return {result, pos};
}

template <>
inline parser_result<Export> parse(const uint8_t* pos)
{
    Export result;
    std::tie(result.name, pos) = parse_string(pos);

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

    std::tie(result.index, pos) =
        leb128u_decode<uint32_t>(pos, pos + 4);  // FIXME: Bounds checking.

    return {result, pos};
}

template <>
inline parser_result<Element> parse(const uint8_t* pos)
{
    TableIdx table_index;
    std::tie(table_index, pos) = leb128u_decode<uint32_t>(pos, pos + 4);  // FIXME: Bounds checking.
    if (table_index != 0)
        throw parser_error{"unexpected tableidx value " + std::to_string(table_index)};

    ConstantExpression offset;
    std::tie(offset, pos) = parse_constant_expression(pos);

    std::vector<FuncIdx> init;
    std::tie(init, pos) = parse_vec<FuncIdx>(pos);

    return {{offset, std::move(init)}, pos};
}

template <>
inline parser_result<Locals> parse(const uint8_t* pos)
{
    Locals result;
    std::tie(result.count, pos) =
        leb128u_decode<uint32_t>(pos, pos + 4);  // FIXME: Bounds checking.
    std::tie(result.type, pos) = parse<ValType>(pos);
    return {result, pos};
}

template <>
inline parser_result<Code> parse(const uint8_t* pos)
{
    const auto [size, pos1] = leb128u_decode<uint32_t>(pos, pos + 4);  // FIXME: Bounds checking.

    const auto [locals_vec, pos2] = parse_vec<Locals>(pos1);

    auto result = parse_expr(pos2);

    for (const auto& l : locals_vec)
        std::get<0>(result).local_count += l.count;

    return result;
}

template <>
inline parser_result<Data> parse(const uint8_t* pos)
{
    MemIdx memory_index;
    std::tie(memory_index, pos) =
        leb128u_decode<uint32_t>(pos, pos + 4);  // FIXME: Bounds checking.
    if (memory_index != 0)
        throw parser_error{"unexpected memidx value " + std::to_string(memory_index)};

    ConstantExpression offset;
    std::tie(offset, pos) = parse_constant_expression(pos);

    std::vector<uint8_t> init;
    std::tie(init, pos) = parse_vec<uint8_t>(pos);

    return {{offset, bytes(init.data(), init.size())}, pos};
}

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
        std::tie(size, it) = leb128u_decode<uint32_t>(it, it + 4);  // FIXME: Bounds checking.
        const auto expected_end_pos = it + size;
        switch (id)
        {
        case SectionId::type:
            std::tie(module.typesec, it) = parse_vec<FuncType>(it);
            break;
        case SectionId::import:
            std::tie(module.importsec, it) = parse_vec<Import>(it);
            break;
        case SectionId::function:
            std::tie(module.funcsec, it) = parse_vec<TypeIdx>(it);
            break;
        case SectionId::table:
            std::tie(module.tablesec, it) = parse_vec<Table>(it);
            break;
        case SectionId::memory:
            std::tie(module.memorysec, it) = parse_vec<Memory>(it);
            break;
        case SectionId::global:
            std::tie(module.globalsec, it) = parse_vec<Global>(it);
            break;
        case SectionId::export_:
            std::tie(module.exportsec, it) = parse_vec<Export>(it);
            break;
        case SectionId::start:
            std::tie(module.startfunc, it) =
                leb128u_decode<uint32_t>(it, it + 4);  // FIXME: Bounds checking.
            break;
        case SectionId::element:
            std::tie(module.elementsec, it) = parse_vec<Element>(it);
            break;
        case SectionId::code:
            std::tie(module.codesec, it) = parse_vec<Code>(it);
            break;
        case SectionId::data:
            std::tie(module.datasec, it) = parse_vec<Data>(it);
            break;
        case SectionId::custom:
            // These sections are ignored for now.
            it += size;
            break;
        default:
            throw parser_error{
                "unknown section encountered " + std::to_string(static_cast<int>(id))};
        }

        if (it != expected_end_pos)
        {
            throw parser_error{"incorrect section " + std::to_string(static_cast<int>(id)) +
                               " size, difference: " + std::to_string(it - expected_end_pos)};
        }
    }

    // Validation checks
    if (module.tablesec.size() > 1)
        throw parser_error{"too many table sections (at most one is allowed)"};

    if (module.memorysec.size() > 1)
        throw parser_error{"too many memory sections (at most one is allowed)"};

    const auto imported_mem_count = std::count_if(module.importsec.begin(), module.importsec.end(),
        [](const auto& import) noexcept { return import.kind == ExternalKind::Memory; });

    if (imported_mem_count > 1)
        throw parser_error{"too many imported memories (at most one is allowed)"};

    if (!module.memorysec.empty() && imported_mem_count > 0)
    {
        throw parser_error{
            "both module memory and imported memory are defined (at most one of them is allowed)"};
    }

    const auto imported_tbl_count = std::count_if(module.importsec.begin(), module.importsec.end(),
        [](const auto& import) noexcept { return import.kind == ExternalKind::Table; });

    if (imported_tbl_count > 1)
        throw parser_error{"too many imported tables (at most one is allowed)"};

    if (!module.tablesec.empty() && imported_tbl_count > 0)
    {
        throw parser_error{
            "both module table and imported table are defined (at most one of them is allowed)"};
    }

    if (!module.elementsec.empty() && module.tablesec.empty() && imported_tbl_count == 0)
        throw parser_error("element section encountered without a table section");

    const auto imported_func_count = std::count_if(module.importsec.begin(), module.importsec.end(),
        [](const auto& import) noexcept { return import.kind == ExternalKind::Function; });
    const auto total_func_count = static_cast<size_t>(imported_func_count) + module.funcsec.size();

    if (module.startfunc && *module.startfunc >= total_func_count)
        throw parser_error{"invalid start function index"};

    return module;
}
}  // namespace fizzy
