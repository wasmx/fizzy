// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "parser.hpp"
#include "asserts.hpp"
#include "leb128.hpp"
#include "limits.hpp"
#include "types.hpp"
#include "utf8.hpp"
#include <cassert>
#include <unordered_set>

namespace fizzy
{
namespace
{
void validate_constant_expression(
    const ConstantExpression& const_expr, const Module& module, ValType expected_type)
{
    if (const_expr.kind == ConstantExpression::Kind::Constant)
        return;

    const auto global_idx = const_expr.value.global_index;
    if (global_idx >= module.get_global_count())
        throw validation_error{"invalid global index in constant expression"};

    const auto global_type = module.get_global_type(global_idx);
    if (global_type.is_mutable)
        throw validation_error{"constant expression can use global.get only for const globals"};

    if (global_type.value_type != expected_type)
        throw validation_error{"constant expression type mismatch"};
}
}  // namespace

/// The reference to the `code` in the wasm binary.
///
/// The distinct type is needed for parse_vec<> specialization.
struct code_view : public bytes_view
{
    using bytes_view::bytes_view;
};

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

ValType validate_valtype(uint8_t byte)
{
    switch (byte)
    {
    case 0x7F:
        return ValType::i32;
    case 0x7E:
        return ValType::i64;
    case 0x7D:
        return ValType::f32;
    case 0x7C:
        return ValType::f64;
    default:
        throw parser_error{"invalid valtype " + std::to_string(byte)};
    }
}

template <>
parser_result<ValType> parse(const uint8_t* pos, const uint8_t* end)
{
    uint8_t byte;
    std::tie(byte, pos) = parse_byte(pos, end);
    const auto valtype = validate_valtype(byte);
    return {valtype, pos};
}

template <>
inline parser_result<FuncType> parse(const uint8_t* pos, const uint8_t* end)
{
    uint8_t kind;
    std::tie(kind, pos) = parse_byte(pos, end);
    if (kind != 0x60)
    {
        throw parser_error{
            "unexpected byte value " + std::to_string(kind) + ", expected 0x60 for functype"};
    }

    FuncType result;
    std::tie(result.inputs, pos) = parse_vec<ValType>(pos, end);
    std::tie(result.outputs, pos) = parse_vec<ValType>(pos, end);

    if (result.outputs.size() > 1)
        throw validation_error{"function has more than one result"};

    return {result, pos};
}

inline parser_result<GlobalType> parse_global_type(const uint8_t* pos, const uint8_t* end)
{
    GlobalType type;
    std::tie(type.value_type, pos) = parse<ValType>(pos, end);

    uint8_t mutability;
    std::tie(mutability, pos) = parse_byte(pos, end);
    if (mutability != 0x00 && mutability != 0x01)
    {
        throw parser_error{"unexpected byte value " + std::to_string(mutability) +
                           ", expected 0x00 or 0x01 for global mutability"};
    }

    type.is_mutable = (mutability == 0x01);
    return {type, pos};
}

inline parser_result<ConstantExpression> parse_constant_expression(
    ValType expected_type, const uint8_t* pos, const uint8_t* end)
{
    ConstantExpression result;
    // Module is needed to know the type of globals accessed with global.get,
    // therefore here we can validate the type only for const instructions.
    std::optional<ValType> constant_actual_type;

    uint8_t opcode;
    std::tie(opcode, pos) = parse_byte(pos, end);

    const auto instr = static_cast<Instr>(opcode);
    switch (instr)
    {
    default:
        throw validation_error{
            "unexpected instruction in the constant expression: " + std::to_string(*(pos - 1))};

    case Instr::end:
        throw validation_error{"constant expression is empty"};

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
        constant_actual_type = ValType::i32;
        break;
    }

    case Instr::i64_const:
    {
        result.kind = ConstantExpression::Kind::Constant;
        int64_t value;
        std::tie(value, pos) = leb128s_decode<int64_t>(pos, end);
        result.value.constant = static_cast<uint64_t>(value);
        constant_actual_type = ValType::i64;
        break;
    }
    case Instr::f32_const:
        result.kind = ConstantExpression::Kind::Constant;
        std::tie(result.value.constant, pos) = parse_value<uint32_t>(pos, end);
        constant_actual_type = ValType::f32;
        break;
    case Instr::f64_const:
        result.kind = ConstantExpression::Kind::Constant;
        std::tie(result.value.constant, pos) = parse_value<uint64_t>(pos, end);
        constant_actual_type = ValType::f64;
        break;
    }

    uint8_t end_opcode;
    std::tie(end_opcode, pos) = parse_byte(pos, end);

    if (static_cast<Instr>(end_opcode) != Instr::end)
        throw validation_error{"constant expression has multiple instructions"};

    if (constant_actual_type.has_value() && constant_actual_type != expected_type)
        throw validation_error{"constant expression type mismatch"};

    return {result, pos};
}

template <>
inline parser_result<Global> parse(const uint8_t* pos, const uint8_t* end)
{
    Global result;
    std::tie(result.type, pos) = parse_global_type(pos, end);
    std::tie(result.expression, pos) = parse_constant_expression(result.type.value_type, pos, end);

    return {result, pos};
}

inline parser_result<Limits> parse_limits(const uint8_t* pos, const uint8_t* end)
{
    Limits result;

    uint8_t kind;
    std::tie(kind, pos) = parse_byte(pos, end);
    switch (kind)
    {
    case 0x00:
        std::tie(result.min, pos) = leb128u_decode<uint32_t>(pos, end);
        return {result, pos};
    case 0x01:
        std::tie(result.min, pos) = leb128u_decode<uint32_t>(pos, end);
        std::tie(result.max, pos) = leb128u_decode<uint32_t>(pos, end);
        if (result.min > *result.max)
            throw validation_error{"malformed limits (minimum is larger than maximum)"};
        return {result, pos};
    default:
        throw parser_error{"invalid limits " + std::to_string(kind)};
    }
}

template <>
inline parser_result<Table> parse(const uint8_t* pos, const uint8_t* end)
{
    uint8_t elemtype;
    std::tie(elemtype, pos) = parse_byte(pos, end);
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
    if ((limits.min > MemoryPagesValidationLimit) ||
        (limits.max.has_value() && *limits.max > MemoryPagesValidationLimit))
        throw validation_error{"maximum memory page limit exceeded"};
    return {{limits}, pos};
}

parser_result<std::string> parse_string(const uint8_t* pos, const uint8_t* end)
{
    // NOTE: this is an optimised version of parse_vec<uint8_t>
    uint32_t size;
    std::tie(size, pos) = leb128u_decode<uint32_t>(pos, end);

    assert(pos <= end);
    if (static_cast<size_t>(end - pos) < size)
        throw parser_error{"unexpected EOF"};

    if (!utf8_validate(pos, pos + size))
        throw parser_error{"invalid UTF-8"};

    const auto str_end = pos + size;
    return {std::string{pos, str_end}, str_end};
}

template <>
inline parser_result<Import> parse(const uint8_t* pos, const uint8_t* end)
{
    Import result{};
    std::tie(result.module, pos) = parse_string(pos, end);
    std::tie(result.name, pos) = parse_string(pos, end);

    uint8_t kind;
    std::tie(kind, pos) = parse_byte(pos, end);
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
        std::tie(result.desc.global, pos) = parse_global_type(pos, end);
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

    uint8_t kind;
    std::tie(kind, pos) = parse_byte(pos, end);
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

    // TODO: The check should be table_index < num_of_tables (0 or 1),
    //       but access to the module is needed.
    if (table_index != 0)
    {
        throw validation_error{
            "invalid table index " + std::to_string(table_index) + " (only table 0 is allowed)"};
    }

    ConstantExpression offset;
    // Offset expression is required to have i32 result value
    // https://webassembly.github.io/spec/core/valid/modules.html#element-segments
    std::tie(offset, pos) = parse_constant_expression(ValType::i32, pos, end);

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
        throw parser_error{"unexpected EOF"};

    // Only record the code reference in wasm binary.
    return {{code_begin, code_size}, code_end};
}

inline Code parse_code(code_view code_binary, FuncIdx func_idx, const Module& module)
{
    const auto begin = code_binary.begin();
    const auto end = code_binary.end();
    const auto [locals_vec, pos1] = parse_vec<Locals>(begin, end);

    uint64_t local_count = 0;
    for (const auto& l : locals_vec)
    {
        local_count += l.count;
        if (local_count > std::numeric_limits<uint32_t>::max())
            throw parser_error{"too many local variables"};
    }

    // TODO: Clarify in spec what happens if count of locals and arguments exceed uint32_t::max()
    //       Leave this assert here for the time being.
    assert((uint64_t{local_count} + module.typesec[module.funcsec[func_idx]].inputs.size()) <=
           std::numeric_limits<uint32_t>::max());

    auto [code, pos2] = parse_expr(pos1, end, func_idx, locals_vec, module);

    // Size is the total bytes of locals and expressions.
    if (pos2 != end)
        throw parser_error{"malformed size field for function"};

    code.local_count = static_cast<uint32_t>(local_count);
    return code;
}

template <>
inline parser_result<Data> parse(const uint8_t* pos, const uint8_t* end)
{
    MemIdx memory_index;
    std::tie(memory_index, pos) = leb128u_decode<uint32_t>(pos, end);

    // TODO: The check should be memory_index < num_of_memories (0 or 1),
    //       but access to the memory section of the module is needed.
    if (memory_index != 0)
    {
        throw validation_error{
            "invalid memory index " + std::to_string(memory_index) + " (only memory 0 is allowed)"};
    }

    ConstantExpression offset;
    // Offset expression is required to have i32 result value
    // https://webassembly.github.io/spec/core/valid/modules.html#data-segments
    std::tie(offset, pos) = parse_constant_expression(ValType::i32, pos, end);

    // NOTE: this is an optimised version of parse_vec<uint8_t>
    uint32_t size;
    std::tie(size, pos) = leb128u_decode<uint32_t>(pos, end);

    assert(pos <= end);
    if (static_cast<size_t>(end - pos) < size)
        throw parser_error{"unexpected EOF"};

    auto init = bytes(pos, pos + size);
    pos += size;

    return {{offset, std::move(init)}, pos};
}

std::unique_ptr<const Module> parse(bytes_view input)
{
    if (input.substr(0, wasm_prefix.size()) != wasm_prefix)
        throw parser_error{"invalid wasm module prefix"};

    input.remove_prefix(wasm_prefix.size());

    auto module{std::make_unique<Module>()};
    std::vector<code_view> code_binaries;
    SectionId last_id = SectionId::custom;
    for (auto it = input.begin(); it != input.end();)
    {
        const auto id = static_cast<SectionId>(*it++);
        if (id != SectionId::custom)
        {
            if (id <= last_id)
                throw parser_error{"unexpected out-of-order section type"};
            last_id = id;
        }

        uint32_t size;
        std::tie(size, it) = leb128u_decode<uint32_t>(it, input.end());

        assert(it <= input.end());
        if (static_cast<size_t>(input.end() - it) < size)
            throw parser_error{"unexpected EOF"};

        const auto expected_section_end = it + size;
        switch (id)
        {
        case SectionId::type:
            std::tie(module->typesec, it) = parse_vec<FuncType>(it, input.end());
            break;
        case SectionId::import:
            std::tie(module->importsec, it) = parse_vec<Import>(it, input.end());
            break;
        case SectionId::function:
            std::tie(module->funcsec, it) = parse_vec<TypeIdx>(it, input.end());
            break;
        case SectionId::table:
            std::tie(module->tablesec, it) = parse_vec<Table>(it, input.end());
            break;
        case SectionId::memory:
            std::tie(module->memorysec, it) = parse_vec<Memory>(it, input.end());
            break;
        case SectionId::global:
            std::tie(module->globalsec, it) = parse_vec<Global>(it, input.end());
            break;
        case SectionId::export_:
            std::tie(module->exportsec, it) = parse_vec<Export>(it, input.end());
            break;
        case SectionId::start:
            std::tie(module->startfunc, it) = leb128u_decode<uint32_t>(it, input.end());
            break;
        case SectionId::element:
            std::tie(module->elementsec, it) = parse_vec<Element>(it, input.end());
            break;
        case SectionId::code:
            std::tie(code_binaries, it) = parse_vec<code_view>(it, input.end());
            break;
        case SectionId::data:
            std::tie(module->datasec, it) = parse_vec<Data>(it, input.end());
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

    // Split imports by kind
    for (const auto& import : module->importsec)
    {
        switch (import.kind)
        {
        case ExternalKind::Function:
            if (import.desc.function_type_index >= module->typesec.size())
                throw validation_error{"invalid type index of an imported function"};
            module->imported_function_types.emplace_back(
                module->typesec[import.desc.function_type_index]);
            break;
        case ExternalKind::Table:
            module->imported_table_types.emplace_back(import.desc.table);
            break;
        case ExternalKind::Memory:
            module->imported_memory_types.emplace_back(import.desc.memory);
            break;
        case ExternalKind::Global:
            module->imported_global_types.emplace_back(import.desc.global);
            break;
        default:                  // LCOV_EXCL_LINE
            FIZZY_UNREACHABLE();  // LCOV_EXCL_LINE
        }
    }

    for (const auto type_idx : module->funcsec)
    {
        if (type_idx >= module->typesec.size())
            throw validation_error{"invalid function type index"};
    }

    if (module->tablesec.size() > 1)
        throw validation_error{"too many table sections (at most one is allowed)"};

    if (module->memorysec.size() > 1)
        throw validation_error{"too many memory sections (at most one is allowed)"};

    if (module->imported_memory_types.size() > 1)
        throw validation_error{"too many imported memories (at most one is allowed)"};

    if (!module->memorysec.empty() && !module->imported_memory_types.empty())
    {
        throw validation_error{
            "both module memory and imported memory are defined (at most one of them is allowed)"};
    }

    if (!module->datasec.empty() && !module->has_memory())
    {
        throw validation_error{
            "invalid memory index 0 (data section encountered without a memory section)"};
    }

    for (const auto& data : module->datasec)
    {
        // Offset expression is required to have i32 result value
        // https://webassembly.github.io/spec/core/valid/modules.html#data-segments
        validate_constant_expression(data.offset, *module, ValType::i32);
    }

    if (module->imported_table_types.size() > 1)
        throw validation_error{"too many imported tables (at most one is allowed)"};

    if (!module->tablesec.empty() && !module->imported_table_types.empty())
    {
        throw validation_error{
            "both module table and imported table are defined (at most one of them is allowed)"};
    }

    if (!module->elementsec.empty() && !module->has_table())
    {
        throw validation_error{
            "invalid table index 0 (element section encountered without a table section)"};
    }

    const auto total_func_count = module->get_function_count();

    for (const auto& element : module->elementsec)
    {
        // Offset expression is required to have i32 result value
        // https://webassembly.github.io/spec/core/valid/modules.html#element-segments
        validate_constant_expression(element.offset, *module, ValType::i32);
        for (const auto func_idx : element.init)
        {
            if (func_idx >= total_func_count)
                throw validation_error{"invalid function index in element section"};
        }
    }

    const auto total_global_count = module->get_global_count();
    for (const auto& global : module->globalsec)
    {
        validate_constant_expression(global.expression, *module, global.type.value_type);

        // Wasm spec section 3.3.7 constrains initialization by another global to const imports only
        // https://webassembly.github.io/spec/core/valid/instructions.html#expressions
        if (global.expression.kind == ConstantExpression::Kind::GlobalGet &&
            global.expression.value.global_index >= module->imported_global_types.size())
        {
            throw validation_error{
                "global can be initialized by another const global only if it's imported"};
        }
    }


    if (module->funcsec.size() != code_binaries.size())
        throw parser_error{"malformed binary: number of function and code entries must match"};

    // Validate exports.
    std::unordered_set<std::string_view> export_names;
    for (const auto& export_ : module->exportsec)
    {
        switch (export_.kind)
        {
        case ExternalKind::Function:
            if (export_.index >= total_func_count)
                throw validation_error{"invalid index of an exported function"};
            break;
        case ExternalKind::Table:
            if (export_.index != 0 || !module->has_table())
                throw validation_error{"invalid index of an exported table"};
            break;
        case ExternalKind::Memory:
            if (export_.index != 0 || !module->has_memory())
                throw validation_error{"invalid index of an exported memory"};
            break;
        case ExternalKind::Global:
            if (export_.index >= total_global_count)
                throw validation_error{"invalid index of an exported global"};
            break;
        default:                  // LCOV_EXCL_LINE
            FIZZY_UNREACHABLE();  // LCOV_EXCL_LINE
        }
        if (!export_names.emplace(export_.name).second)
            throw validation_error{"duplicate export name " + export_.name};
    }

    if (module->startfunc)
    {
        if (*module->startfunc >= total_func_count)
            throw validation_error{"invalid start function index"};

        const auto& func_type = module->get_function_type(*module->startfunc);
        if (!func_type.inputs.empty() || !func_type.outputs.empty())
            throw validation_error{"invalid start function type"};
    }

    // Process code. TODO: This can be done lazily.
    module->codesec.reserve(code_binaries.size());
    for (size_t i = 0; i < code_binaries.size(); ++i)
        module->codesec.emplace_back(
            parse_code(code_binaries[i], static_cast<FuncIdx>(i), *module));

    return module;
}

parser_result<std::vector<uint32_t>> parse_vec_i32(const uint8_t* pos, const uint8_t* end)
{
    return parse_vec<uint32_t>(pos, end);
}
}  // namespace fizzy
