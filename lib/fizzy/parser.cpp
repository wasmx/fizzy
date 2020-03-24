#include "parser.hpp"
#include "leb128.hpp"
#include "types.hpp"
#include "utf8.hpp"
#include <algorithm>

namespace fizzy
{
template <typename T>
parser_result<T> parse(const uint8_t* pos, const uint8_t* end);

template <typename T>
inline parser_result<std::vector<T>> parse_vec(const uint8_t* pos, const uint8_t* end)
{
    uint32_t size;
    std::tie(size, pos) = leb128u_decode<uint32_t>(pos, end);

    std::vector<T> result;

    // Reserve memory for vec elements if `size` value is reasonable.
    // TODO: Adjust the limit constant by inspecting max vec size value
    //       appearing in big set of example wasm binaries.
    if (size < 128)
        result.reserve(size);

    auto inserter = std::back_inserter(result);
    for (uint32_t i = 0; i < size; ++i)
        std::tie(inserter, pos) = parse<T>(pos, end);
    return {result, pos};
}

template <>
inline parser_result<uint32_t> parse(const uint8_t* pos, const uint8_t* end)
{
    return leb128u_decode<uint32_t>(pos, end);
}

/* FIXME: use this in functions parsing a single byte (and rename this to parse_byte) OR remove this
template <>
inline parser_result<uint8_t> parse(const uint8_t* pos, const uint8_t* end)
{
    if (pos == end)
        throw parser_error{"Unexpected EOF"};

    return {*pos, pos + 1};
}
*/

template <>
parser_result<ValType> parse(const uint8_t* pos, const uint8_t* end)
{
    if (pos == end)
        throw parser_error{"Unexpected EOF"};

    const auto b = *pos++;
    switch (b)
    {
    case 0x7F:
        return {ValType::i32, pos};
    case 0x7E:
        return {ValType::i64, pos};
    case 0x7D:  // f32
        return {ValType::f32, pos};
    case 0x7C:  // f64
        return {ValType::f64, pos};
    default:
        throw parser_error{"invalid valtype " + std::to_string(b)};
    }
}

const uint8_t* validate_valtype(const uint8_t* pos, const uint8_t* end)
{
    const auto [_, next_pos] = parse<ValType>(pos, end);
    return next_pos;
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
    pos = validate_valtype(pos, end);

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
        case Instr::f32_const:
            // FIXME: support this once floating points are implemented
            result.kind = ConstantExpression::Kind::Constant;
            result.value.constant = 0;
            pos = skip(4, pos, end);
            break;
        case Instr::f64_const:
            // FIXME: support this once floating points are implemented
            result.kind = ConstantExpression::Kind::Constant;
            result.value.constant = 0;
            pos = skip(8, pos, end);
            break;
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

inline parser_result<Limits> parse_limits(const uint8_t* pos, const uint8_t* end)
{
    if (pos == end)
        throw parser_error{"Unexpected EOF"};

    Limits result;
    const auto b = *pos++;
    switch (b)
    {
    case 0x00:
        std::tie(result.min, pos) = leb128u_decode<uint32_t>(pos, end);
        return {result, pos};
    case 0x01:
        std::tie(result.min, pos) = leb128u_decode<uint32_t>(pos, end);
        std::tie(result.max, pos) = leb128u_decode<uint32_t>(pos, end);
        if (result.min > *result.max)
            throw parser_error("malformed limits (minimum is larger than maximum)");
        return {result, pos};
    default:
        throw parser_error{"invalid limits " + std::to_string(b)};
    }
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

parser_result<std::string> parse_string(const uint8_t* pos, const uint8_t* end)
{
    // NOTE: this is an optimised version of parse_vec<uint8_t>
    uint32_t size;
    std::tie(size, pos) = leb128u_decode<uint32_t>(pos, end);

    if ((pos + size) > end)
        throw parser_error{"Unexpected EOF"};

    if (!utf8_validate(pos, pos + size))
        throw parser_error{"Invalid UTF-8"};

    const auto char_begin = reinterpret_cast<const char*>(pos);
    auto ret = std::string(char_begin, char_begin + size);
    pos += size;

    return {std::move(ret), pos};
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
inline parser_result<code_view> parse(const uint8_t* pos, const uint8_t* end)
{
    const auto [code_size, code_begin] = leb128u_decode<uint32_t>(pos, end);
    const auto code_end = code_begin + code_size;
    if (code_end > end)
        throw parser_error{"Unexpected EOF"};

    // Only record the code reference in wasm binary.
    return {{code_begin, code_size}, code_end};
}

inline Code parse_code(code_view code_binary)
{
    const auto begin = code_binary.begin();
    const auto end = code_binary.end();
    const auto [locals_vec, pos1] = parse_vec<Locals>(begin, end);

    auto [code, pos2] = parse_expr(pos1, end);

    // Size is the total bytes of locals and expressions.
    if (pos2 != end)
        throw parser_error{"malformed size field for function"};

    uint64_t local_count = 0;
    for (const auto& l : locals_vec)
    {
        local_count += l.count;
        if (local_count > std::numeric_limits<uint32_t>::max())
            throw parser_error{"too many local variables"};
    }
    code.local_count = static_cast<uint32_t>(local_count);
    return code;
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

    // NOTE: this is an optimised version of parse_vec<uint8_t>
    uint32_t size;
    std::tie(size, pos) = leb128u_decode<uint32_t>(pos, end);

    if ((pos + size) > end)
        throw parser_error{"Unexpected EOF"};

    auto init = bytes(pos, pos + size);
    pos += size;

    return {{offset, std::move(init)}, pos};
}

Module parse(bytes_view input)
{
    if (input.substr(0, wasm_prefix.size()) != wasm_prefix)
        throw parser_error{"invalid wasm module prefix"};

    input.remove_prefix(wasm_prefix.size());

    Module module;
    std::vector<code_view> code_binaries;
    SectionId last_id = SectionId::custom;
    for (auto it = input.begin(); it != input.end();)
    {
        const auto id = static_cast<SectionId>(*it++);
        if (id != SectionId::custom)
        {
            if (id <= last_id)
                throw parser_error{"Unexpected out-of-order section type"};
            last_id = id;
        }

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
            std::tie(code_binaries, it) = parse_vec<code_view>(it, input.end());
            break;
        case SectionId::data:
            std::tie(module.datasec, it) = parse_vec<Data>(it, input.end());
            break;
        case SectionId::custom:
            // NOTE: this section can be ignored, but the name must be parseable (and valid UTF-8)
            parse_string(it, expected_section_end);
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

    if (module.funcsec.size() != code_binaries.size())
        throw parser_error("malformed binary: number of function and code entries must match");

    const auto imported_func_count = std::count_if(module.importsec.begin(), module.importsec.end(),
        [](const auto& import) noexcept { return import.kind == ExternalKind::Function; });
    const auto total_func_count = static_cast<size_t>(imported_func_count) + module.funcsec.size();

    if (module.startfunc && *module.startfunc >= total_func_count)
        throw parser_error{"invalid start function index"};

    // Process code. TODO: This can be done lazily.
    module.codesec.reserve(code_binaries.size());
    for (auto& code_binary : code_binaries)
        module.codesec.emplace_back(parse_code(code_binary));

    return module;
}

parser_result<std::vector<uint32_t>> parse_vec_i32(const uint8_t* pos, const uint8_t* end)
{
    return parse_vec<uint32_t>(pos, end);
}
}  // namespace fizzy
