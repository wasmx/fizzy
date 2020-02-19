#include "parser.hpp"
#include "leb128.hpp"
#include "types.hpp"
#include <algorithm>

namespace fizzy
{
template <>
inline parser_result<uint8_t> parse(const uint8_t* pos, const uint8_t* end)
{
    if (pos == end)
        throw parser_error{"Unexpected EOF"};

    return {*pos, pos + 1};
}

template <>
inline parser_result<FuncType> parse(const uint8_t* pos, const uint8_t* end)
{
    if (pos == end)
        throw parser_error{"Unexpected EOF"};

    const uint8_t kind = *pos++;
    if (kind != 0x60)
    {
        throw parser_error{
            "unexpected byte value " + std::to_string(kind) + ", expected 0x60 for functype"};
    }

    FuncType result;
    std::tie(result.inputs, pos) = parse_vec<ValType>(pos, end);
    std::tie(result.outputs, pos) = parse_vec<ValType>(pos, end);
    return {result, pos};
}

inline std::tuple<bool, const uint8_t*> parse_global_type(const uint8_t* pos, const uint8_t* end)
{
    // will throw if invalid type
    std::tie(std::ignore, pos) = parse<ValType>(pos, end);

    if (pos == end)
        throw parser_error{"Unexpected EOF"};

    const uint8_t mutability = *pos++;
    if (mutability != 0x00 && mutability != 0x01)
    {
        throw parser_error{"unexpected byte value " + std::to_string(mutability) +
                           ", expected 0x00 or 0x01 for global mutability"};
    }

    const bool is_mutable = (mutability == 0x01);
    return {is_mutable, pos};
}

inline parser_result<ConstantExpression> parse_constant_expression(
    const uint8_t* pos, const uint8_t* end)
{
    ConstantExpression result;

    Instr instr;
    do
    {
        if (pos == end)
            throw parser_error{"Unexpected EOF"};

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
            std::tie(result.value.global_index, pos) = leb128u_decode<uint32_t>(pos, end);
            break;
        }

        case Instr::i32_const:
        {
            result.kind = ConstantExpression::Kind::Constant;
            int32_t value;
            std::tie(value, pos) = leb128s_decode<int32_t>(pos, end);
            result.value.constant = static_cast<uint32_t>(value);
            break;
        }

        case Instr::i64_const:
        {
            result.kind = ConstantExpression::Kind::Constant;
            int64_t value;
            std::tie(value, pos) = leb128s_decode<int64_t>(pos, end);
            result.value.constant = static_cast<uint64_t>(value);
            break;
        }
        }
    } while (instr != Instr::end);

    return {result, pos};
}

template <>
inline parser_result<Global> parse(const uint8_t* pos, const uint8_t* end)
{
    Global result;
    std::tie(result.is_mutable, pos) = parse_global_type(pos, end);
    std::tie(result.expression, pos) = parse_constant_expression(pos, end);

    return {result, pos};
}

template <>
inline parser_result<Table> parse(const uint8_t* pos, const uint8_t* end)
{
    if (pos == end)
        throw parser_error{"Unexpected EOF"};

    const uint8_t elemtype = *pos++;
    if (elemtype != FuncRef)
        throw parser_error{"unexpected table elemtype: " + std::to_string(elemtype)};

    Limits limits;
    std::tie(limits, pos) = parse_limits(pos, end);

    return {{limits}, pos};
}

template <>
inline parser_result<Memory> parse(const uint8_t* pos, const uint8_t* end)
{
    Limits limits;
    std::tie(limits, pos) = parse_limits(pos, end);
    return {{limits}, pos};
}

inline parser_result<std::string> parse_string(const uint8_t* pos, const uint8_t* end)
{
    std::vector<uint8_t> value;
    std::tie(value, pos) = parse_vec<uint8_t>(pos, end);

    // FIXME: need to validate that string is a valid UTF-8

    return {std::string(value.begin(), value.end()), pos};
}

template <>
inline parser_result<Import> parse(const uint8_t* pos, const uint8_t* end)
{
    Import result{};
    std::tie(result.module, pos) = parse_string(pos, end);
    std::tie(result.name, pos) = parse_string(pos, end);

    if (pos == end)
        throw parser_error{"Unexpected EOF"};

    const uint8_t kind = *pos++;
    switch (kind)
    {
    case 0x00:
        result.kind = ExternalKind::Function;
        std::tie(result.desc.function_type_index, pos) = leb128u_decode<uint32_t>(pos, end);
        break;
    case 0x01:
        result.kind = ExternalKind::Table;
        std::tie(result.desc.table, pos) = parse<Table>(pos, end);
        break;
    case 0x02:
        result.kind = ExternalKind::Memory;
        std::tie(result.desc.memory, pos) = parse<Memory>(pos, end);
        break;
    case 0x03:
        result.kind = ExternalKind::Global;
        std::tie(result.desc.global_mutable, pos) = parse_global_type(pos, end);
        break;
    default:
        throw parser_error{"unexpected import kind value " + std::to_string(kind)};
    }

    return {result, pos};
}

template <>
inline parser_result<Export> parse(const uint8_t* pos, const uint8_t* end)
{
    Export result;
    std::tie(result.name, pos) = parse_string(pos, end);

    if (pos == end)
        throw parser_error{"Unexpected EOF"};

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

    std::tie(result.index, pos) = leb128u_decode<uint32_t>(pos, end);

    return {result, pos};
}

template <>
inline parser_result<Element> parse(const uint8_t* pos, const uint8_t* end)
{
    TableIdx table_index;
    std::tie(table_index, pos) = leb128u_decode<uint32_t>(pos, end);
    if (table_index != 0)
        throw parser_error{"unexpected tableidx value " + std::to_string(table_index)};

    ConstantExpression offset;
    std::tie(offset, pos) = parse_constant_expression(pos, end);

    std::vector<FuncIdx> init;
    std::tie(init, pos) = parse_vec<FuncIdx>(pos, end);

    return {{offset, std::move(init)}, pos};
}

template <>
inline parser_result<Locals> parse(const uint8_t* pos, const uint8_t* end)
{
    Locals result;
    std::tie(result.count, pos) = leb128u_decode<uint32_t>(pos, end);
    std::tie(result.type, pos) = parse<ValType>(pos, end);
    return {result, pos};
}

template <>
inline parser_result<Code> parse(const uint8_t* pos, const uint8_t* end)
{
    const auto [size, pos1] = leb128u_decode<uint32_t>(pos, end);

    const auto [locals_vec, pos2] = parse_vec<Locals>(pos1, end);

    auto [code, pos3] = parse_expr(pos2, end);

    // Size is the total bytes of locals and expressions
    if (size != (pos3 - pos1))
        throw parser_error{"malformed size field for function"};

    uint64_t local_count = 0;
    for (const auto& l : locals_vec)
    {
        local_count += l.count;
        if (local_count > std::numeric_limits<uint32_t>::max())
            throw parser_error{"too many local variables"};
    }
    code.local_count = static_cast<uint32_t>(local_count);

    return {std::move(code), pos3};
}

template <>
inline parser_result<Data> parse(const uint8_t* pos, const uint8_t* end)
{
    MemIdx memory_index;
    std::tie(memory_index, pos) = leb128u_decode<uint32_t>(pos, end);
    if (memory_index != 0)
        throw parser_error{"unexpected memidx value " + std::to_string(memory_index)};

    ConstantExpression offset;
    std::tie(offset, pos) = parse_constant_expression(pos, end);

    std::vector<uint8_t> init;
    std::tie(init, pos) = parse_vec<uint8_t>(pos, end);

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
        std::tie(size, it) = leb128u_decode<uint32_t>(it, input.end());

        const auto expected_section_end = it + size;
        if (expected_section_end > input.end())
            throw parser_error("Unexpected EOF");

        switch (id)
        {
        case SectionId::type:
            std::tie(module.typesec, it) = parse_vec<FuncType>(it, input.end());
            break;
        case SectionId::import:
            std::tie(module.importsec, it) = parse_vec<Import>(it, input.end());
            break;
        case SectionId::function:
            std::tie(module.funcsec, it) = parse_vec<TypeIdx>(it, input.end());
            break;
        case SectionId::table:
            std::tie(module.tablesec, it) = parse_vec<Table>(it, input.end());
            break;
        case SectionId::memory:
            std::tie(module.memorysec, it) = parse_vec<Memory>(it, input.end());
            break;
        case SectionId::global:
            std::tie(module.globalsec, it) = parse_vec<Global>(it, input.end());
            break;
        case SectionId::export_:
            std::tie(module.exportsec, it) = parse_vec<Export>(it, input.end());
            break;
        case SectionId::start:
            std::tie(module.startfunc, it) = leb128u_decode<uint32_t>(it, input.end());
            break;
        case SectionId::element:
            std::tie(module.elementsec, it) = parse_vec<Element>(it, input.end());
            break;
        case SectionId::code:
            std::tie(module.codesec, it) = parse_vec<Code>(it, input.end());
            break;
        case SectionId::data:
            std::tie(module.datasec, it) = parse_vec<Data>(it, input.end());
            break;
        case SectionId::custom:
            // NOTE: this section can be ignored, but the name must be parseable (and valid UTF-8)
            parse_string(it, input.end());
            // These sections are ignored for now.
            it += size;
            break;
        default:
            throw parser_error{
                "unknown section encountered " + std::to_string(static_cast<int>(id))};
        }

        if (it != expected_section_end)
        {
            throw parser_error{"incorrect section " + std::to_string(static_cast<int>(id)) +
                               " size, difference: " + std::to_string(it - expected_section_end)};
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
