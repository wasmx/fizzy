// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "limits.hpp"
#include "module.hpp"
#include "stack.hpp"
#include "types.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <stack>

namespace fizzy
{
namespace
{
// code_offset + imm_offset + stack_height + arity
constexpr auto BranchImmediateSize = 3 * sizeof(uint32_t) + sizeof(uint8_t);

void match_imported_functions(const std::vector<FuncType>& module_imported_types,
    const std::vector<ExternalFunction>& imported_functions)
{
    if (module_imported_types.size() != imported_functions.size())
    {
        throw instantiate_error("module requires " + std::to_string(module_imported_types.size()) +
                                " imported functions, " +
                                std::to_string(imported_functions.size()) + " provided");
    }

    for (size_t i = 0; i < imported_functions.size(); ++i)
    {
        if (module_imported_types[i] != imported_functions[i].type)
        {
            throw instantiate_error("function " + std::to_string(i) +
                                    " type doesn't match module's imported function type");
        }
    }
}

void match_limits(const Limits& external_limits, const Limits& module_limits)
{
    if (external_limits.min < module_limits.min)
        throw instantiate_error("provided import's min is below import's min defined in module");

    if (!module_limits.max.has_value())
        return;

    if (external_limits.max.has_value() && *external_limits.max <= *module_limits.max)
        return;

    throw instantiate_error("provided import's max is above import's max defined in module");
}

void match_imported_tables(const std::vector<Table>& module_imported_tables,
    const std::vector<ExternalTable>& imported_tables)
{
    assert(module_imported_tables.size() <= 1);

    if (imported_tables.size() > 1)
        throw instantiate_error("only 1 imported table is allowed");

    if (module_imported_tables.empty())
    {
        if (!imported_tables.empty())
        {
            throw instantiate_error(
                "trying to provide imported table to a module that doesn't define one");
        }
    }
    else
    {
        if (imported_tables.empty())
            throw instantiate_error("module defines an imported table but none was provided");

        match_limits(imported_tables[0].limits, module_imported_tables[0].limits);

        if (imported_tables[0].table == nullptr)
            throw instantiate_error("provided imported table has a null pointer to data");

        const auto size = imported_tables[0].table->size();
        const auto min = imported_tables[0].limits.min;
        const auto& max = imported_tables[0].limits.max;
        if (size < min || (max.has_value() && size > *max))
            throw instantiate_error("provided imported table doesn't fit provided limits");
    }
}

void match_imported_memories(const std::vector<Memory>& module_imported_memories,
    const std::vector<ExternalMemory>& imported_memories)
{
    assert(module_imported_memories.size() <= 1);

    if (imported_memories.size() > 1)
        throw instantiate_error("only 1 imported memory is allowed");

    if (module_imported_memories.empty())
    {
        if (!imported_memories.empty())
        {
            throw instantiate_error(
                "trying to provide imported memory to a module that doesn't define one");
        }
    }
    else
    {
        if (imported_memories.empty())
            throw instantiate_error("module defines an imported memory but none was provided");

        match_limits(imported_memories[0].limits, module_imported_memories[0].limits);

        if (imported_memories[0].data == nullptr)
            throw instantiate_error("provided imported memory has a null pointer to data");

        const auto size = imported_memories[0].data->size();
        const auto min = imported_memories[0].limits.min;
        const auto& max = imported_memories[0].limits.max;
        if (size < min * PageSize || (max.has_value() && size > *max * PageSize))
            throw instantiate_error("provided imported memory doesn't fit provided limits");
    }
}

void match_imported_globals(const std::vector<bool>& module_imports_mutability,
    const std::vector<ExternalGlobal>& imported_globals)
{
    if (module_imports_mutability.size() != imported_globals.size())
    {
        throw instantiate_error(
            "module requires " + std::to_string(module_imports_mutability.size()) +
            " imported globals, " + std::to_string(imported_globals.size()) + " provided");
    }

    for (size_t i = 0; i < imported_globals.size(); ++i)
    {
        if (imported_globals[i].is_mutable != module_imports_mutability[i])
        {
            throw instantiate_error("global " + std::to_string(i) +
                                    " mutability doesn't match module's global mutability");
        }
        if (imported_globals[i].value == nullptr)
        {
            throw instantiate_error("global " + std::to_string(i) + " has a null pointer to value");
        }
    }
}

std::tuple<table_ptr, Limits> allocate_table(
    const std::vector<Table>& module_tables, const std::vector<ExternalTable>& imported_tables)
{
    static const auto table_delete = [](table_elements* t) noexcept { delete t; };
    static const auto null_delete = [](table_elements*) noexcept {};

    assert(module_tables.size() + imported_tables.size() <= 1);

    if (module_tables.size() == 1)
    {
        return {table_ptr{new table_elements(module_tables[0].limits.min), table_delete},
            module_tables[0].limits};
    }
    else if (imported_tables.size() == 1)
        return {table_ptr{imported_tables[0].table, null_delete}, imported_tables[0].limits};
    else
        return {table_ptr{nullptr, null_delete}, Limits{}};
}

std::tuple<bytes_ptr, Limits> allocate_memory(const std::vector<Memory>& module_memories,
    const std::vector<ExternalMemory>& imported_memories)
{
    static const auto bytes_delete = [](bytes* b) noexcept { delete b; };
    static const auto null_delete = [](bytes*) noexcept {};

    assert(module_memories.size() + imported_memories.size() <= 1);

    if (module_memories.size() == 1)
    {
        const auto memory_min = module_memories[0].limits.min;
        const auto memory_max = module_memories[0].limits.max;

        // TODO: better error handling
        if ((memory_min > MemoryPagesLimit) ||
            (memory_max.has_value() && *memory_max > MemoryPagesLimit))
        {
            throw instantiate_error("cannot exceed hard memory limit of " +
                                    std::to_string(MemoryPagesLimit * PageSize) + " bytes");
        }

        // NOTE: fill it with zeroes
        bytes_ptr memory{new bytes(memory_min * PageSize, 0), bytes_delete};
        return {std::move(memory), module_memories[0].limits};
    }
    else if (imported_memories.size() == 1)
    {
        const auto memory_min = imported_memories[0].limits.min;
        const auto memory_max = imported_memories[0].limits.max;

        // TODO: better error handling
        if ((memory_min > MemoryPagesLimit) ||
            (memory_max.has_value() && *memory_max > MemoryPagesLimit))
        {
            throw instantiate_error("imported memory limits cannot exceed hard memory limit of " +
                                    std::to_string(MemoryPagesLimit * PageSize) + " bytes");
        }

        bytes_ptr memory{imported_memories[0].data, null_delete};
        return {std::move(memory), imported_memories[0].limits};
    }
    else
    {
        bytes_ptr memory{nullptr, null_delete};
        return {std::move(memory), Limits{}};
    }
}

uint64_t eval_constant_expression(ConstantExpression expr,
    const std::vector<ExternalGlobal>& imported_globals, const std::vector<Global>& global_types,
    const std::vector<uint64_t>& globals)
{
    if (expr.kind == ConstantExpression::Kind::Constant)
        return expr.value.constant;

    assert(expr.kind == ConstantExpression::Kind::GlobalGet);

    const auto global_idx = expr.value.global_index;
    const bool is_mutable = (global_idx < imported_globals.size() ?
                                 imported_globals[global_idx].is_mutable :
                                 global_types[global_idx - imported_globals.size()].is_mutable);
    if (is_mutable)
        throw instantiate_error("constant expression can use global_get only for const globals");

    if (global_idx < imported_globals.size())
        return *imported_globals[global_idx].value;
    else
        return globals[global_idx - imported_globals.size()];
}

template <typename T>
inline T read(const uint8_t*& input) noexcept
{
    T ret;
    __builtin_memcpy(&ret, input, sizeof(ret));
    input += sizeof(ret);
    return ret;
}

void branch(
    const Code& code, OperandStack& stack, const Instr*& pc, const uint8_t*& immediates) noexcept
{
    const auto code_offset = read<uint32_t>(immediates);
    const auto imm_offset = read<uint32_t>(immediates);
    const auto stack_height = static_cast<size_t>(read<int>(immediates));
    const auto arity = read<uint8_t>(immediates);

    pc = code.instructions.data() + code_offset;
    immediates = code.immediates.data() + imm_offset;

    // When branch is taken, additional stack items must be dropped.
    assert(stack.size() >= stack_height + arity);
    if (arity != 0)
    {
        assert(arity == 1);
        const auto result = stack.top();
        stack.shrink(stack_height);
        stack.push(result);
    }
    else
        stack.shrink(stack_height);
}

template <class F>
bool invoke_function(
    const FuncType& func_type, const F& func, Instance& instance, OperandStack& stack, int depth)
{
    const auto num_args = func_type.inputs.size();
    assert(stack.size() >= num_args);
    std::vector<uint64_t> call_args{stack.rend() - num_args, stack.rend()};
    stack.shrink(stack.size() - num_args);

    const auto ret = func(instance, std::move(call_args), depth + 1);
    // Bubble up traps
    if (ret.trapped)
        return false;

    const auto num_outputs = func_type.outputs.size();
    // NOTE: we can assume these two from validation
    assert(ret.stack.size() == num_outputs);
    assert(num_outputs <= 1);
    // Push back the result
    if (num_outputs != 0)
        stack.push(ret.stack[0]);

    return true;
}

inline bool invoke_function(const FuncType& func_type, uint32_t func_idx, Instance& instance,
    OperandStack& stack, int depth)
{
    const auto func = [func_idx](Instance& _instance, std::vector<uint64_t> args, int _depth) {
        return execute(_instance, func_idx, std::move(args), _depth);
    };
    return invoke_function(func_type, func, instance, stack, depth);
}

template <typename T>
inline void store(bytes& input, size_t offset, T value) noexcept
{
    __builtin_memcpy(input.data() + offset, &value, sizeof(value));
}

template <typename T>
inline T load(bytes_view input, size_t offset) noexcept
{
    T ret;
    __builtin_memcpy(&ret, input.data() + offset, sizeof(ret));
    return ret;
}

template <typename DstT, typename SrcT>
inline DstT extend(SrcT in) noexcept
{
    if constexpr (std::is_signed<SrcT>::value)
    {
        using SignedDstT = typename std::make_signed<DstT>::type;
        return static_cast<DstT>(SignedDstT{in});
    }
    else
        return DstT{in};
}

template <typename DstT, typename SrcT = DstT>
inline bool load_from_memory(bytes_view memory, OperandStack& stack, const uint8_t*& immediates)
{
    const auto address = static_cast<uint32_t>(stack.pop());
    // NOTE: alignment is dropped by the parser
    const auto offset = read<uint32_t>(immediates);
    // Addressing is 32-bit, but we keep the value as 64-bit to detect overflows.
    if ((uint64_t{address} + offset + sizeof(SrcT)) > memory.size())
        return false;

    const auto ret = load<SrcT>(memory, address + offset);
    stack.push(extend<DstT>(ret));
    return true;
}

template <typename DstT>
inline bool store_into_memory(bytes& memory, OperandStack& stack, const uint8_t*& immediates)
{
    const auto value = static_cast<DstT>(stack.pop());
    const auto address = static_cast<uint32_t>(stack.pop());
    // NOTE: alignment is dropped by the parser
    const auto offset = read<uint32_t>(immediates);
    // Addressing is 32-bit, but we keep the value as 64-bit to detect overflows.
    if ((uint64_t{address} + offset + sizeof(DstT)) > memory.size())
        return false;

    store<DstT>(memory, address + offset, value);
    return true;
}

template <typename Op>
inline void unary_op(OperandStack& stack, Op op) noexcept
{
    using T = decltype(op(stack.top()));
    stack.top() = op(static_cast<T>(stack.top()));
}

template <typename Op>
inline void binary_op(OperandStack& stack, Op op) noexcept
{
    using T = decltype(op(stack.top(), stack.top()));
    const auto val2 = static_cast<T>(stack.pop());
    const auto val1 = static_cast<T>(stack.top());
    stack.top() = static_cast<std::make_unsigned_t<T>>(op(val1, val2));
}

template <typename T, template <typename> class Op>
inline void comparison_op(OperandStack& stack, Op<T> op) noexcept
{
    const auto val2 = static_cast<T>(stack.pop());
    const auto val1 = static_cast<T>(stack.top());
    stack.top() = uint32_t{op(val1, val2)};
}

template <typename T>
inline T shift_left(T lhs, T rhs) noexcept
{
    constexpr T num_bits{sizeof(T) * 8};
    const auto k = rhs & (num_bits - 1);
    return lhs << k;
}

template <typename T>
inline T shift_right(T lhs, T rhs) noexcept
{
    constexpr T num_bits{sizeof(T) * 8};
    const auto k = rhs & (num_bits - 1);
    return lhs >> k;
}

template <typename T>
inline T rotl(T lhs, T rhs) noexcept
{
    constexpr T num_bits{sizeof(T) * 8};
    const auto k = rhs & (num_bits - 1);

    if (k == 0)
        return lhs;

    return (lhs << k) | (lhs >> (num_bits - k));
}

template <typename T>
inline T rotr(T lhs, T rhs) noexcept
{
    constexpr T num_bits{sizeof(T) * 8};
    const auto k = rhs & (num_bits - 1);

    if (k == 0)
        return lhs;

    return (lhs >> k) | (lhs << (num_bits - k));
}

inline uint32_t clz32(uint32_t value) noexcept
{
    // NOTE: Wasm specifies this case, but C/C++ intrinsic leaves it as undefined.
    if (value == 0)
        return 32;
    return static_cast<uint32_t>(__builtin_clz(value));
}

inline uint32_t ctz32(uint32_t value) noexcept
{
    // NOTE: Wasm specifies this case, but C/C++ intrinsic leaves it as undefined.
    if (value == 0)
        return 32;
    return static_cast<uint32_t>(__builtin_ctz(value));
}

inline uint32_t popcnt32(uint32_t value) noexcept
{
    return static_cast<uint32_t>(__builtin_popcount(value));
}

inline uint64_t clz64(uint64_t value) noexcept
{
    // NOTE: Wasm specifies this case, but C/C++ intrinsic leaves it as undefined.
    if (value == 0)
        return 64;
    return static_cast<uint64_t>(__builtin_clzll(value));
}

inline uint64_t ctz64(uint64_t value) noexcept
{
    // NOTE: Wasm specifies this case, but C/C++ intrinsic leaves it as undefined.
    if (value == 0)
        return 64;
    return static_cast<uint64_t>(__builtin_ctzll(value));
}

inline uint64_t popcnt64(uint64_t value) noexcept
{
    return static_cast<uint64_t>(__builtin_popcountll(value));
}

std::optional<uint32_t> find_export(const Module& module, ExternalKind kind, std::string_view name)
{
    const auto it = std::find_if(module.exportsec.begin(), module.exportsec.end(),
        [kind, name](const auto& export_) { return export_.kind == kind && export_.name == name; });

    return (it != module.exportsec.end() ? std::make_optional(it->index) : std::nullopt);
}

}  // namespace

std::unique_ptr<Instance> instantiate(Module module,
    std::vector<ExternalFunction> imported_functions, std::vector<ExternalTable> imported_tables,
    std::vector<ExternalMemory> imported_memories, std::vector<ExternalGlobal> imported_globals)
{
    assert(module.funcsec.size() == module.codesec.size());

    match_imported_functions(module.imported_function_types, imported_functions);
    match_imported_tables(module.imported_table_types, imported_tables);
    match_imported_memories(module.imported_memory_types, imported_memories);
    match_imported_globals(module.imported_globals_mutability, imported_globals);

    // Init globals
    std::vector<uint64_t> globals;
    globals.reserve(module.globalsec.size());
    for (auto const& global : module.globalsec)
    {
        // Wasm spec section 3.3.7 constrains initialization by another global to const imports only
        // https://webassembly.github.io/spec/core/valid/instructions.html#expressions
        if (global.expression.kind == ConstantExpression::Kind::GlobalGet &&
            global.expression.value.global_index >= imported_globals.size())
        {
            throw instantiate_error(
                "global can be initialized by another const global only if it's imported");
        }

        const auto value = eval_constant_expression(
            global.expression, imported_globals, module.globalsec, globals);
        globals.emplace_back(value);
    }

    auto [table, table_limits] = allocate_table(module.tablesec, imported_tables);

    auto [memory, memory_limits] = allocate_memory(module.memorysec, imported_memories);

    // Before starting to fill memory and table,
    // check that data and element segments are within bounds.
    std::vector<uint64_t> datasec_offsets;
    datasec_offsets.reserve(module.datasec.size());
    for (const auto& data : module.datasec)
    {
        const uint64_t offset =
            eval_constant_expression(data.offset, imported_globals, module.globalsec, globals);

        if (offset + data.init.size() > memory->size())
            throw instantiate_error("data segment is out of memory bounds");

        datasec_offsets.emplace_back(offset);
    }

    assert(module.elementsec.empty() || table != nullptr);
    std::vector<ptrdiff_t> elementsec_offsets;
    elementsec_offsets.reserve(module.elementsec.size());
    for (const auto& element : module.elementsec)
    {
        const uint64_t offset =
            eval_constant_expression(element.offset, imported_globals, module.globalsec, globals);

        if (offset + element.init.size() > table->size())
            throw instantiate_error("element segment is out of table bounds");

        elementsec_offsets.emplace_back(offset);
    }

    // Fill out memory based on data segments
    for (size_t i = 0; i < module.datasec.size(); ++i)
    {
        // NOTE: these instructions can overlap
        std::copy(module.datasec[i].init.begin(), module.datasec[i].init.end(),
            memory->data() + datasec_offsets[i]);
    }

    // We need to create instance before filling table,
    // because table functions will capture the pointer to instance.
    auto instance = std::make_unique<Instance>(std::move(module), std::move(memory), memory_limits,
        std::move(table), table_limits, std::move(globals), std::move(imported_functions),
        std::move(imported_globals));

    // Fill the table based on elements segment
    for (size_t i = 0; i < instance->module.elementsec.size(); ++i)
    {
        // Overwrite table[offset..] with element.init
        auto it_table = instance->table->begin() + elementsec_offsets[i];
        for (const auto idx : instance->module.elementsec[i].init)
        {
            auto func = [idx, &instance_ref = *instance](
                            fizzy::Instance&, std::vector<uint64_t> args, int depth) {
                return execute(instance_ref, idx, std::move(args), depth);
            };

            *it_table++ =
                ExternalFunction{std::move(func), instance->module.get_function_type(idx)};
        }
    }

    // Run start function if present
    if (instance->module.startfunc)
    {
        const auto funcidx = *instance->module.startfunc;
        assert(funcidx < instance->imported_functions.size() + instance->module.funcsec.size());
        if (execute(*instance, funcidx, {}).trapped)
        {
            // When element section modified imported table, and then start function trapped,
            // modifications to the table are not rolled back.
            // Instance in this case is not being returned to the user, so it needs to be kept alive
            // as long as functions using it are alive in the table.
            if (!imported_tables.empty() && !instance->module.elementsec.empty())
            {
                // Instance may be used by several functions added to the table,
                // so we need a shared ownership here.
                std::shared_ptr<Instance> shared_instance = std::move(instance);

                for (size_t i = 0; i < shared_instance->module.elementsec.size(); ++i)
                {
                    auto it_table = shared_instance->table->begin() + elementsec_offsets[i];
                    for ([[maybe_unused]] auto _ : shared_instance->module.elementsec[i].init)
                    {
                        // Wrap the function with the lambda capturing shared instance
                        auto& table_function = (*it_table)->function;
                        table_function = [shared_instance, func = std::move(table_function)](
                                             fizzy::Instance& _instance, std::vector<uint64_t> args,
                                             int depth) {
                            return func(_instance, std::move(args), depth);
                        };
                        ++it_table;
                    }
                }
            }
            throw instantiate_error("start function failed to execute");
        }
    }

    return instance;
}

execution_result execute(
    Instance& instance, FuncIdx func_idx, std::vector<uint64_t> args, int depth)
{
    assert(depth >= 0);
    if (depth > CallStackLimit)
        return {true, {}};

    if (func_idx < instance.imported_functions.size())
        return instance.imported_functions[func_idx].function(instance, std::move(args), depth);

    const auto code_idx = func_idx - instance.imported_functions.size();
    assert(code_idx < instance.module.codesec.size());

    const auto& code = instance.module.codesec[code_idx];
    auto* const memory = instance.memory.get();

    std::vector<uint64_t> locals = std::move(args);
    locals.resize(locals.size() + code.local_count);

    OperandStack stack(static_cast<size_t>(code.max_stack_height));

    bool trap = false;

    const Instr* pc = code.instructions.data();
    const uint8_t* immediates = code.immediates.data();

    while (true)
    {
        const auto instruction = *pc++;
        switch (instruction)
        {
        case Instr::unreachable:
            trap = true;
            goto end;
        case Instr::nop:
        case Instr::block:
        case Instr::loop:
            break;
        case Instr::if_:
        {
            if (static_cast<uint32_t>(stack.pop()) != 0)
                immediates += 2 * sizeof(uint32_t);  // Skip the immediates for else instruction.
            else
            {
                const auto target_pc = read<uint32_t>(immediates);
                const auto target_imm = read<uint32_t>(immediates);

                pc = code.instructions.data() + target_pc;
                immediates = code.immediates.data() + target_imm;
            }
            break;
        }
        case Instr::else_:
        {
            // We reach else only after executing if block ("then" part),
            // so we need to skip else block now.
            const auto target_pc = read<uint32_t>(immediates);
            const auto target_imm = read<uint32_t>(immediates);

            pc = code.instructions.data() + target_pc;
            immediates = code.immediates.data() + target_imm;

            break;
        }
        case Instr::end:
        {
            // End execution if it's a final end instruction.
            if (pc == &code.instructions[code.instructions.size()])
                goto end;
            break;
        }
        case Instr::br:
        case Instr::br_if:
        case Instr::return_:
        {
            // Check condition for br_if.
            if (instruction == Instr::br_if && static_cast<uint32_t>(stack.pop()) == 0)
            {
                immediates += BranchImmediateSize;
                break;
            }

            branch(code, stack, pc, immediates);
            break;
        }
        case Instr::br_table:
        {
            const auto br_table_size = read<uint32_t>(immediates);
            const auto br_table_idx = stack.pop();

            const auto label_idx_offset = br_table_idx < br_table_size ?
                                              br_table_idx * BranchImmediateSize :
                                              br_table_size * BranchImmediateSize;
            immediates += label_idx_offset;

            branch(code, stack, pc, immediates);
            break;
        }
        case Instr::call:
        {
            const auto called_func_idx = read<uint32_t>(immediates);
            const auto& func_type = instance.module.get_function_type(called_func_idx);

            if (!invoke_function(func_type, called_func_idx, instance, stack, depth))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::call_indirect:
        {
            assert(instance.table != nullptr);

            const auto expected_type_idx = read<uint32_t>(immediates);
            assert(expected_type_idx < instance.module.typesec.size());

            const auto elem_idx = stack.pop();
            if (elem_idx >= instance.table->size())
            {
                trap = true;
                goto end;
            }

            const auto called_func = (*instance.table)[elem_idx];
            if (!called_func.has_value())
            {
                trap = true;
                goto end;
            }

            // check actual type against expected type
            const auto& actual_type = called_func->type;
            const auto& expected_type = instance.module.typesec[expected_type_idx];
            if (expected_type != actual_type)
            {
                trap = true;
                goto end;
            }

            if (!invoke_function(actual_type, called_func->function, instance, stack, depth))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::drop:
        {
            stack.pop();
            break;
        }
        case Instr::select:
        {
            const auto condition = static_cast<uint32_t>(stack.pop());
            // NOTE: these two are the same type (ensured by validation)
            const auto val2 = stack.pop();
            const auto val1 = stack.pop();
            if (condition == 0)
                stack.push(val2);
            else
                stack.push(val1);
            break;
        }
        case Instr::local_get:
        {
            const auto idx = read<uint32_t>(immediates);
            assert(idx <= locals.size());
            stack.push(locals[idx]);
            break;
        }
        case Instr::local_set:
        {
            const auto idx = read<uint32_t>(immediates);
            assert(idx <= locals.size());
            locals[idx] = stack.pop();
            break;
        }
        case Instr::local_tee:
        {
            const auto idx = read<uint32_t>(immediates);
            assert(idx <= locals.size());
            locals[idx] = stack.top();
            break;
        }
        case Instr::global_get:
        {
            const auto idx = read<uint32_t>(immediates);
            assert(idx < instance.imported_globals.size() + instance.globals.size());
            if (idx < instance.imported_globals.size())
            {
                stack.push(*instance.imported_globals[idx].value);
            }
            else
            {
                const auto module_global_idx = idx - instance.imported_globals.size();
                assert(module_global_idx < instance.module.globalsec.size());
                stack.push(instance.globals[module_global_idx]);
            }
            break;
        }
        case Instr::global_set:
        {
            const auto idx = read<uint32_t>(immediates);
            if (idx < instance.imported_globals.size())
            {
                assert(instance.imported_globals[idx].is_mutable);
                *instance.imported_globals[idx].value = stack.pop();
            }
            else
            {
                const auto module_global_idx = idx - instance.imported_globals.size();
                assert(module_global_idx < instance.module.globalsec.size());
                assert(instance.module.globalsec[module_global_idx].is_mutable);
                instance.globals[module_global_idx] = stack.pop();
            }
            break;
        }
        case Instr::i32_load:
        {
            if (!load_from_memory<uint32_t>(*memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i64_load:
        {
            if (!load_from_memory<uint64_t>(*memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i32_load8_s:
        {
            if (!load_from_memory<uint32_t, int8_t>(*memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i32_load8_u:
        {
            if (!load_from_memory<uint32_t, uint8_t>(*memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i32_load16_s:
        {
            if (!load_from_memory<uint32_t, int16_t>(*memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i32_load16_u:
        {
            if (!load_from_memory<uint32_t, uint16_t>(*memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i64_load8_s:
        {
            if (!load_from_memory<uint64_t, int8_t>(*memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i64_load8_u:
        {
            if (!load_from_memory<uint64_t, uint8_t>(*memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i64_load16_s:
        {
            if (!load_from_memory<uint64_t, int16_t>(*memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i64_load16_u:
        {
            if (!load_from_memory<uint64_t, uint16_t>(*memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i64_load32_s:
        {
            if (!load_from_memory<uint64_t, int32_t>(*memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i64_load32_u:
        {
            if (!load_from_memory<uint64_t, uint32_t>(*memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i32_store:
        {
            if (!store_into_memory<uint32_t>(*memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i64_store:
        {
            if (!store_into_memory<uint64_t>(*memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i32_store8:
        case Instr::i64_store8:
        {
            if (!store_into_memory<uint8_t>(*memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i32_store16:
        case Instr::i64_store16:
        {
            if (!store_into_memory<uint16_t>(*memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i64_store32:
        {
            if (!store_into_memory<uint32_t>(*memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::memory_size:
        {
            stack.push(static_cast<uint32_t>(memory->size() / PageSize));
            break;
        }
        case Instr::memory_grow:
        {
            const auto delta = static_cast<uint32_t>(stack.pop());
            const auto cur_pages = memory->size() / PageSize;
            assert(cur_pages <= size_t(std::numeric_limits<int32_t>::max()));
            const auto new_pages = cur_pages + delta;
            assert(new_pages >= cur_pages);
            uint32_t ret = static_cast<uint32_t>(cur_pages);
            try
            {
                const size_t memory_max_pages =
                    (instance.memory_limits.max.has_value() ? *instance.memory_limits.max :
                                                              MemoryPagesLimit);
                if (new_pages > memory_max_pages)
                    throw std::bad_alloc();
                memory->resize(new_pages * PageSize);
            }
            catch (std::bad_alloc const&)
            {
                ret = static_cast<uint32_t>(-1);
            }
            stack.push(ret);
            break;
        }
        case Instr::i32_const:
        {
            const auto value = read<uint32_t>(immediates);
            stack.push(value);
            break;
        }
        case Instr::i64_const:
        {
            const auto value = read<uint64_t>(immediates);
            stack.push(value);
            break;
        }
        case Instr::i32_eqz:
        {
            const auto value = static_cast<uint32_t>(stack.pop());
            stack.push(value == 0);
            break;
        }
        case Instr::i32_eq:
        {
            comparison_op(stack, std::equal_to<uint32_t>());
            break;
        }
        case Instr::i32_ne:
        {
            comparison_op(stack, std::not_equal_to<uint32_t>());
            break;
        }
        case Instr::i32_lt_s:
        {
            comparison_op(stack, std::less<int32_t>());
            break;
        }
        case Instr::i32_lt_u:
        {
            comparison_op(stack, std::less<uint32_t>());
            break;
        }
        case Instr::i32_gt_s:
        {
            comparison_op(stack, std::greater<int32_t>());
            break;
        }
        case Instr::i32_gt_u:
        {
            comparison_op(stack, std::greater<uint32_t>());
            break;
        }
        case Instr::i32_le_s:
        {
            comparison_op(stack, std::less_equal<int32_t>());
            break;
        }
        case Instr::i32_le_u:
        {
            comparison_op(stack, std::less_equal<uint32_t>());
            break;
        }
        case Instr::i32_ge_s:
        {
            comparison_op(stack, std::greater_equal<int32_t>());
            break;
        }
        case Instr::i32_ge_u:
        {
            comparison_op(stack, std::greater_equal<uint32_t>());
            break;
        }
        case Instr::i64_eqz:
        {
            stack.push(stack.pop() == 0);
            break;
        }
        case Instr::i64_eq:
        {
            comparison_op(stack, std::equal_to<uint64_t>());
            break;
        }
        case Instr::i64_ne:
        {
            comparison_op(stack, std::not_equal_to<uint64_t>());
            break;
        }
        case Instr::i64_lt_s:
        {
            comparison_op(stack, std::less<int64_t>());
            break;
        }
        case Instr::i64_lt_u:
        {
            comparison_op(stack, std::less<uint64_t>());
            break;
        }
        case Instr::i64_gt_s:
        {
            comparison_op(stack, std::greater<int64_t>());
            break;
        }
        case Instr::i64_gt_u:
        {
            comparison_op(stack, std::greater<uint64_t>());
            break;
        }
        case Instr::i64_le_s:
        {
            comparison_op(stack, std::less_equal<int64_t>());
            break;
        }
        case Instr::i64_le_u:
        {
            comparison_op(stack, std::less_equal<uint64_t>());
            break;
        }
        case Instr::i64_ge_s:
        {
            comparison_op(stack, std::greater_equal<int64_t>());
            break;
        }
        case Instr::i64_ge_u:
        {
            comparison_op(stack, std::greater_equal<uint64_t>());
            break;
        }
        case Instr::i32_clz:
        {
            unary_op(stack, clz32);
            break;
        }
        case Instr::i32_ctz:
        {
            unary_op(stack, ctz32);
            break;
        }
        case Instr::i32_popcnt:
        {
            unary_op(stack, popcnt32);
            break;
        }
        case Instr::i32_add:
        {
            binary_op(stack, std::plus<uint32_t>());
            break;
        }
        case Instr::i32_sub:
        {
            binary_op(stack, std::minus<uint32_t>());
            break;
        }
        case Instr::i32_mul:
        {
            binary_op(stack, std::multiplies<uint32_t>());
            break;
        }
        case Instr::i32_div_s:
        {
            auto const rhs = static_cast<int32_t>(stack[0]);
            auto const lhs = static_cast<int32_t>(stack[1]);
            if (rhs == 0 || (lhs == std::numeric_limits<int32_t>::min() && rhs == -1))
            {
                trap = true;
                goto end;
            }
            binary_op(stack, std::divides<int32_t>());
            break;
        }
        case Instr::i32_div_u:
        {
            auto const rhs = static_cast<uint32_t>(stack.top());
            if (rhs == 0)
            {
                trap = true;
                goto end;
            }
            binary_op(stack, std::divides<uint32_t>());
            break;
        }
        case Instr::i32_rem_s:
        {
            auto const rhs = static_cast<int32_t>(stack.top());
            if (rhs == 0)
            {
                trap = true;
                goto end;
            }
            auto const lhs = static_cast<int32_t>(stack[1]);
            if (lhs == std::numeric_limits<int32_t>::min() && rhs == -1)
            {
                stack.pop();
                stack.top() = 0;
            }
            else
                binary_op(stack, std::modulus<int32_t>());
            break;
        }
        case Instr::i32_rem_u:
        {
            auto const rhs = static_cast<uint32_t>(stack.top());
            if (rhs == 0)
            {
                trap = true;
                goto end;
            }
            binary_op(stack, std::modulus<uint32_t>());
            break;
        }
        case Instr::i32_and:
        {
            binary_op(stack, std::bit_and<uint32_t>());
            break;
        }
        case Instr::i32_or:
        {
            binary_op(stack, std::bit_or<uint32_t>());
            break;
        }
        case Instr::i32_xor:
        {
            binary_op(stack, std::bit_xor<uint32_t>());
            break;
        }
        case Instr::i32_shl:
        {
            binary_op(stack, shift_left<uint32_t>);
            break;
        }
        case Instr::i32_shr_s:
        {
            binary_op(stack, shift_right<int32_t>);
            break;
        }
        case Instr::i32_shr_u:
        {
            binary_op(stack, shift_right<uint32_t>);
            break;
        }
        case Instr::i32_rotl:
        {
            binary_op(stack, rotl<uint32_t>);
            break;
        }
        case Instr::i32_rotr:
        {
            binary_op(stack, rotr<uint32_t>);
            break;
        }
        case Instr::i64_clz:
        {
            unary_op(stack, clz64);
            break;
        }
        case Instr::i64_ctz:
        {
            unary_op(stack, ctz64);
            break;
        }
        case Instr::i64_popcnt:
        {
            unary_op(stack, popcnt64);
            break;
        }
        case Instr::i64_add:
        {
            binary_op(stack, std::plus<uint64_t>());
            break;
        }
        case Instr::i64_sub:
        {
            binary_op(stack, std::minus<uint64_t>());
            break;
        }
        case Instr::i64_mul:
        {
            binary_op(stack, std::multiplies<uint64_t>());
            break;
        }
        case Instr::i64_div_s:
        {
            auto const rhs = static_cast<int64_t>(stack[0]);
            auto const lhs = static_cast<int64_t>(stack[1]);
            if (rhs == 0 || (lhs == std::numeric_limits<int64_t>::min() && rhs == -1))
            {
                trap = true;
                goto end;
            }
            binary_op(stack, std::divides<int64_t>());
            break;
        }
        case Instr::i64_div_u:
        {
            auto const rhs = static_cast<uint64_t>(stack.top());
            if (rhs == 0)
            {
                trap = true;
                goto end;
            }
            binary_op(stack, std::divides<uint64_t>());
            break;
        }
        case Instr::i64_rem_s:
        {
            auto const rhs = static_cast<int64_t>(stack.top());
            if (rhs == 0)
            {
                trap = true;
                goto end;
            }
            auto const lhs = static_cast<int64_t>(stack[1]);
            if (lhs == std::numeric_limits<int64_t>::min() && rhs == -1)
            {
                stack.pop();
                stack.top() = 0;
            }
            else
                binary_op(stack, std::modulus<int64_t>());
            break;
        }
        case Instr::i64_rem_u:
        {
            auto const rhs = static_cast<uint64_t>(stack.top());
            if (rhs == 0)
            {
                trap = true;
                goto end;
            }
            binary_op(stack, std::modulus<uint64_t>());
            break;
        }
        case Instr::i64_and:
        {
            binary_op(stack, std::bit_and<uint64_t>());
            break;
        }
        case Instr::i64_or:
        {
            binary_op(stack, std::bit_or<uint64_t>());
            break;
        }
        case Instr::i64_xor:
        {
            binary_op(stack, std::bit_xor<uint64_t>());
            break;
        }
        case Instr::i64_shl:
        {
            binary_op(stack, shift_left<uint64_t>);
            break;
        }
        case Instr::i64_shr_s:
        {
            binary_op(stack, shift_right<int64_t>);
            break;
        }
        case Instr::i64_shr_u:
        {
            binary_op(stack, shift_right<uint64_t>);
            break;
        }
        case Instr::i64_rotl:
        {
            binary_op(stack, rotl<uint64_t>);
            break;
        }
        case Instr::i64_rotr:
        {
            binary_op(stack, rotr<uint64_t>);
            break;
        }
        case Instr::i32_wrap_i64:
        {
            stack.push(static_cast<uint32_t>(stack.pop()));
            break;
        }
        case Instr::i64_extend_i32_s:
        {
            const auto value = static_cast<int32_t>(stack.pop());
            stack.push(static_cast<uint64_t>(int64_t{value}));
            break;
        }
        case Instr::i64_extend_i32_u:
        {
            // effectively no-op
            break;
        }
        case Instr::f32_load:
        case Instr::f64_load:
        case Instr::f32_store:
        case Instr::f64_store:
        case Instr::f32_const:
        case Instr::f64_const:
        case Instr::f32_eq:
        case Instr::f32_ne:
        case Instr::f32_lt:
        case Instr::f32_gt:
        case Instr::f32_le:
        case Instr::f32_ge:
        case Instr::f64_eq:
        case Instr::f64_ne:
        case Instr::f64_lt:
        case Instr::f64_gt:
        case Instr::f64_le:
        case Instr::f64_ge:
        case Instr::f32_abs:
        case Instr::f32_neg:
        case Instr::f32_ceil:
        case Instr::f32_floor:
        case Instr::f32_trunc:
        case Instr::f32_nearest:
        case Instr::f32_sqrt:
        case Instr::f32_add:
        case Instr::f32_sub:
        case Instr::f32_mul:
        case Instr::f32_div:
        case Instr::f32_min:
        case Instr::f32_max:
        case Instr::f32_copysign:
        case Instr::f64_abs:
        case Instr::f64_neg:
        case Instr::f64_ceil:
        case Instr::f64_floor:
        case Instr::f64_trunc:
        case Instr::f64_nearest:
        case Instr::f64_sqrt:
        case Instr::f64_add:
        case Instr::f64_sub:
        case Instr::f64_mul:
        case Instr::f64_div:
        case Instr::f64_min:
        case Instr::f64_max:
        case Instr::f64_copysign:
        case Instr::i32_trunc_f32_s:
        case Instr::i32_trunc_f32_u:
        case Instr::i32_trunc_f64_s:
        case Instr::i32_trunc_f64_u:
        case Instr::i64_trunc_f32_s:
        case Instr::i64_trunc_f32_u:
        case Instr::i64_trunc_f64_s:
        case Instr::i64_trunc_f64_u:
        case Instr::f32_convert_i32_s:
        case Instr::f32_convert_i32_u:
        case Instr::f32_convert_i64_s:
        case Instr::f32_convert_i64_u:
        case Instr::f32_demote_f64:
        case Instr::f64_convert_i32_s:
        case Instr::f64_convert_i32_u:
        case Instr::f64_convert_i64_s:
        case Instr::f64_convert_i64_u:
        case Instr::f64_promote_f32:
        case Instr::i32_reinterpret_f32:
        case Instr::i64_reinterpret_f64:
        case Instr::f32_reinterpret_i32:
        case Instr::f64_reinterpret_i64:
            throw unsupported_feature("Floating point instruction.");
        default:
            assert(false);
            break;
        }
    }

end:
    assert(pc == &code.instructions[code.instructions.size()] || trap);
    return {trap, {stack.rbegin(), stack.rend()}};
}

execution_result execute(const Module& module, FuncIdx func_idx, std::vector<uint64_t> args)
{
    auto instance = instantiate(module);
    return execute(*instance, func_idx, std::move(args));
}

std::vector<ExternalFunction> resolve_imported_functions(
    const Module& module, std::vector<ImportedFunction> imported_functions)
{
    std::vector<ExternalFunction> external_functions;
    for (const auto& import : module.importsec)
    {
        if (import.kind != ExternalKind::Function)
            continue;

        const auto it = std::find_if(
            imported_functions.begin(), imported_functions.end(), [&import](const auto& func) {
                return import.module == func.module && import.name == func.name;
            });

        if (it == imported_functions.end())
        {
            throw instantiate_error(
                "imported function " + import.module + "." + import.name + " is required");
        }

        assert(import.desc.function_type_index < module.typesec.size());
        const auto& module_func_type = module.typesec[import.desc.function_type_index];

        if (module_func_type.inputs != it->inputs)
        {
            throw instantiate_error("function " + import.module + "." + import.name +
                                    " input types don't match imported function in module");
        }
        if (module_func_type.outputs.empty() && it->output.has_value())
        {
            throw instantiate_error("function " + import.module + "." + import.name +
                                    " has output but is defined void in module");
        }
        if (!module_func_type.outputs.empty() &&
            (!it->output.has_value() || module_func_type.outputs[0] != *it->output))
        {
            throw instantiate_error("function " + import.module + "." + import.name +
                                    " output type doesn't match imported function in module");
        }

        external_functions.emplace_back(
            ExternalFunction{std::move(it->function), module_func_type});
    }

    return external_functions;
}

std::optional<FuncIdx> find_exported_function(const Module& module, std::string_view name)
{
    return find_export(module, ExternalKind::Function, name);
}

std::optional<ExternalFunction> find_exported_function(Instance& instance, std::string_view name)
{
    const auto opt_index = find_export(instance.module, ExternalKind::Function, name);
    if (!opt_index.has_value())
        return std::nullopt;

    const auto idx = *opt_index;
    auto func = [idx, &instance](fizzy::Instance&, std::vector<uint64_t> args, int depth) {
        return execute(instance, idx, std::move(args), depth);
    };

    return ExternalFunction{std::move(func), instance.module.get_function_type(idx)};
}

std::optional<ExternalGlobal> find_exported_global(Instance& instance, std::string_view name)
{
    const auto opt_index = find_export(instance.module, ExternalKind::Global, name);
    if (!opt_index.has_value())
        return std::nullopt;

    const auto global_idx = *opt_index;
    if (global_idx < instance.imported_globals.size())
    {
        // imported global is reexported
        return ExternalGlobal{instance.imported_globals[global_idx].value,
            instance.imported_globals[global_idx].is_mutable};
    }
    else
    {
        // global owned by instance
        const auto module_global_idx = global_idx - instance.imported_globals.size();
        return ExternalGlobal{&instance.globals[module_global_idx],
            instance.module.globalsec[module_global_idx].is_mutable};
    }
}

std::optional<ExternalTable> find_exported_table(Instance& instance, std::string_view name)
{
    const auto& module = instance.module;

    // Index returned from find_export is discarded, because there's no more than 1 table
    if (!find_export(module, ExternalKind::Table, name))
        return std::nullopt;

    return ExternalTable{instance.table.get(), instance.table_limits};
}

std::optional<ExternalMemory> find_exported_memory(Instance& instance, std::string_view name)
{
    const auto& module = instance.module;

    // Index returned from find_export is discarded, because there's no more than 1 memory
    if (!find_export(module, ExternalKind::Memory, name))
        return std::nullopt;

    return ExternalMemory{instance.memory.get(), instance.memory_limits};
}

}  // namespace fizzy
