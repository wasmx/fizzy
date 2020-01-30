#include "execute.hpp"
#include "limits.hpp"
#include "stack.hpp"
#include "types.hpp"
#include <cassert>
#include <cstring>

namespace fizzy
{
namespace
{
struct LabelContext
{
    const Instr* pc = nullptr;
    const uint8_t* immediate = nullptr;
    size_t arity = 0;
    size_t stack_height = 0;
};

void branch(uint32_t label_idx, Stack<LabelContext>& labels, Stack<uint64_t>& stack,
    const Instr*& pc, const uint8_t*& immediates) noexcept
{
    assert(labels.size() > label_idx);
    labels.drop(label_idx);  // Drop skipped labels (does nothing for labelidx == 0).
    const auto label = labels.pop();

    pc = label.pc;
    immediates = label.immediate;

    // When branch is taken, additional stack items must be dropped.
    assert(stack.size() >= label.stack_height + label.arity);
    if (label.arity != 0)
    {
        assert(label.arity == 1);
        const auto result = stack.peek();
        stack.resize(label.stack_height);
        stack.push(result);
    }
    else
        stack.resize(label.stack_height);
}

bool invoke_function(
    uint32_t type_idx, uint32_t func_idx, Instance& instance, Stack<uint64_t>& stack)
{
    const auto num_inputs = instance.module.typesec[type_idx].inputs.size();
    assert(stack.size() >= num_inputs);
    std::vector<uint64_t> call_args(
        stack.rbegin(), stack.rbegin() + static_cast<ptrdiff_t>(num_inputs));
    stack.resize(stack.size() - num_inputs);

    const auto ret = execute(instance, func_idx, call_args);
    // Bubble up traps
    if (ret.trapped)
        return false;

    const auto num_outputs = instance.module.typesec[type_idx].outputs.size();
    // NOTE: we can assume these two from validation
    assert(ret.stack.size() == num_outputs);
    assert(num_outputs <= 1);
    // Push back the result
    if (num_outputs != 0)
        stack.push(ret.stack[0]);

    return true;
}

void match_imported_functions(const std::vector<TypeIdx>& module_imported_types,
    const std::vector<ImportedFunction>& imported_functions)
{
    if (module_imported_types.size() != imported_functions.size())
    {
        throw std::runtime_error("Module requires " + std::to_string(module_imported_types.size()) +
                                 " imported functions, " +
                                 std::to_string(imported_functions.size()) + " provided");
    }
}

void match_imported_globals(const std::vector<bool>& module_imports_mutability,
    const std::vector<ImportedGlobal>& imported_globals)
{
    if (module_imports_mutability.size() != imported_globals.size())
    {
        throw std::runtime_error(
            "Module requires " + std::to_string(module_imports_mutability.size()) +
            " imported globals, " + std::to_string(imported_globals.size()) + " provided");
    }

    for (size_t i = 0; i < imported_globals.size(); ++i)
    {
        if (imported_globals[i].is_mutable != module_imports_mutability[i])
        {
            throw std::runtime_error("Global " + std::to_string(i) +
                                     " mutability doesn't match module's global mutability");
        }
        if (imported_globals[i].value == nullptr)
        {
            throw std::runtime_error(
                "Global " + std::to_string(i) + " has a null pointer to value");
        }
    }
}

std::vector<TypeIdx> match_imports(const Module& module,
    const std::vector<ImportedFunction>& imported_functions,
    const std::vector<ImportedGlobal>& imported_globals)
{
    std::vector<TypeIdx> imported_function_types;
    std::vector<bool> imported_globals_mutability;
    for (auto const& import : module.importsec)
    {
        switch (import.kind)
        {
        case ExternalKind::Function:
            imported_function_types.emplace_back(import.desc.function_type_index);
            break;
        case ExternalKind::Global:
            imported_globals_mutability.emplace_back(import.desc.global_mutable);
            break;
        default:
            throw std::runtime_error("Import of type " +
                                     std::to_string(static_cast<uint8_t>(import.kind)) +
                                     " is not supported");
        }
    }

    match_imported_functions(imported_function_types, imported_functions);
    match_imported_globals(imported_globals_mutability, imported_globals);

    return imported_function_types;
}

uint64_t eval_constant_expression(ConstantExpression expr,
    const std::vector<ImportedGlobal>& imported_globals, const std::vector<Global>& global_types,
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
        throw std::runtime_error("Constant expression can use global_get only for const globals.");

    if (global_idx < imported_globals.size())
        return *imported_globals[global_idx].value;
    else
        return globals[global_idx - imported_globals.size()];
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

template <typename T>
inline T read(const uint8_t*& input) noexcept
{
    T ret;
    __builtin_memcpy(&ret, input, sizeof(ret));
    input += sizeof(ret);
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
inline bool load_from_memory(bytes_view memory, Stack<uint64_t>& stack, const uint8_t*& immediates)
{
    const auto address = static_cast<uint32_t>(stack.pop());
    // NOTE: alignment is dropped by the parser
    const auto offset = read<uint32_t>(immediates);
    if ((address + offset + sizeof(SrcT)) > memory.size())
        return false;

    const auto ret = load<SrcT>(memory, address + offset);
    stack.push(extend<DstT>(ret));
    return true;
}

template <typename DstT>
inline bool store_into_memory(bytes& memory, Stack<uint64_t>& stack, const uint8_t*& immediates)
{
    const auto value = static_cast<DstT>(stack.pop());
    const auto address = static_cast<uint32_t>(stack.pop());
    // NOTE: alignment is dropped by the parser
    const auto offset = read<uint32_t>(immediates);
    if ((address + offset + sizeof(DstT)) > memory.size())
        return false;

    store<DstT>(memory, address + offset, value);
    return true;
}

template <typename Op>
inline void unary_op(Stack<uint64_t>& stack, Op op) noexcept
{
    using T = decltype(op(stack.pop()));
    const auto a = static_cast<T>(stack.pop());
    stack.push(static_cast<uint64_t>(op(a)));
}

template <typename Op>
inline void binary_op(Stack<uint64_t>& stack, Op op) noexcept
{
    using T = decltype(op(stack.pop(), stack.pop()));
    const auto val2 = static_cast<T>(stack.pop());
    const auto val1 = static_cast<T>(stack.pop());
    stack.push(static_cast<uint64_t>(op(val1, val2)));
}

template <typename T, template <typename> class Op>
inline void comparison_op(Stack<uint64_t>& stack, Op<T> op) noexcept
{
    const auto val2 = static_cast<T>(stack.pop());
    const auto val1 = static_cast<T>(stack.pop());
    stack.push(uint32_t{op(val1, val2)});
}

template <typename T>
inline T shift_left(T lhs, T rhs) noexcept
{
    auto const k = rhs % std::numeric_limits<T>::digits;
    return lhs << k;
}

template <typename T>
inline T shift_right(T lhs, T rhs) noexcept
{
    auto const k = rhs % std::numeric_limits<T>::digits;
    return lhs >> k;
}

template <typename T>
inline T rotl(T lhs, T rhs) noexcept
{
    auto const k = rhs % std::numeric_limits<T>::digits;
    return (lhs << k) | (lhs >> (std::numeric_limits<T>::digits - k));
}

template <typename T>
inline T rotr(T lhs, T rhs) noexcept
{
    auto const k = rhs % std::numeric_limits<T>::digits;
    return (lhs >> k) | (lhs << (std::numeric_limits<T>::digits - k));
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
}  // namespace

Instance instantiate(const Module& module, std::vector<ImportedFunction> imported_functions,
    std::vector<ImportedGlobal> imported_globals)
{
    std::vector<TypeIdx> imported_function_types =
        match_imports(module, imported_functions, imported_globals);

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
            throw std::runtime_error(
                "Global can be initialized by another const global only if it's imported.");
        }

        const auto value = eval_constant_expression(
            global.expression, imported_globals, module.globalsec, globals);
        globals.emplace_back(value);
    }

    // Set up tables
    std::vector<FuncIdx> table;
    if (!module.tablesec.empty())
        table.resize(module.tablesec[0].limits.min);

    // Allocate memory
    size_t memory_min, memory_max;
    if (module.memorysec.size() > 1)
    {
        // FIXME: turn this into an assert if instantiate is not exposed externally and it only
        // takes validated modules
        throw std::runtime_error("Cannot support more than 1 memory section.");
    }
    else if (module.memorysec.size() == 1)
    {
        memory_min = module.memorysec[0].limits.min;
        if (module.memorysec[0].limits.max)
            memory_max = *module.memorysec[0].limits.max;
        else
            memory_max = MemoryPagesLimit;
        // FIXME: better error handling
        if ((memory_min > MemoryPagesLimit) || (memory_max > MemoryPagesLimit))
            throw std::runtime_error("Cannot exceed hard memory limit of " +
                                     std::to_string(MemoryPagesLimit * PageSize) + " bytes");
    }
    else
    {
        memory_min = 0;
        memory_max = MemoryPagesLimit;
    }
    // NOTE: fill it with zeroes
    bytes memory(memory_min * PageSize, 0);

    // Fill the table based on elements segment
    assert(module.elementsec.empty() || !module.tablesec.empty());
    for (const auto& element : module.elementsec)
    {
        uint64_t offset;
        if (element.offset.kind == ConstantExpression::Kind::Constant)
            offset = element.offset.value.constant;
        else
            throw std::runtime_error(
                "element initialization by imported global is not supported yet");

        // Overwrite table[offset..] with element.init
        assert((offset + element.init.size()) <= table.size());
        std::copy(element.init.begin(), element.init.end(), &table[offset]);
    }

    // Fill out memory based on data segments
    for (const auto& data : module.datasec)
    {
        const uint64_t offset =
            eval_constant_expression(data.offset, imported_globals, module.globalsec, globals);

        // NOTE: these instructions can overlap
        assert((offset + data.init.size()) <= (memory_max * PageSize));
        std::memcpy(memory.data() + offset, data.init.data(), data.init.size());
    }

    Instance instance = {module, std::move(memory), memory_max, std::move(table),
        std::move(globals), std::move(imported_functions), std::move(imported_function_types),
        std::move(imported_globals)};

    // Run start function if present
    if (module.startfunc)
    {
        if (execute(instance, *module.startfunc, {}).trapped)
            throw std::runtime_error("Start function failed to execute");
    }

    return instance;
}

execution_result execute(Instance& instance, FuncIdx func_idx, std::vector<uint64_t> args)
{
    if (func_idx < instance.imported_functions.size())
        return instance.imported_functions[func_idx](instance, std::move(args));

    const auto code_idx = func_idx - instance.imported_functions.size();
    assert(code_idx < instance.module.codesec.size());

    const auto& code = instance.module.codesec[code_idx];

    std::vector<uint64_t> locals = std::move(args);
    locals.resize(locals.size() + code.local_count);

    // TODO: preallocate fixed stack depth properly
    Stack<uint64_t> stack;

    Stack<LabelContext> labels;

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
            break;
        case Instr::block:
        {
            const auto arity = read<uint8_t>(immediates);
            const auto target_pc = read<uint32_t>(immediates);
            const auto target_imm = read<uint32_t>(immediates);
            LabelContext label{code.instructions.data() + target_pc,
                code.immediates.data() + target_imm, arity, stack.size()};
            labels.emplace_back(label);
            break;
        }
        case Instr::loop:
        {
            LabelContext label{pc - 1, immediates, 0, stack.size()};  // Target this instruction.
            labels.push_back(label);
            break;
        }
        case Instr::end:
        {
            if (!labels.empty())
                labels.pop_back();
            else
                goto end;
            break;
        }
        case Instr::br:
        case Instr::br_if:
        {
            const auto label_idx = read<uint32_t>(immediates);

            // Check condition for br_if.
            if (instruction == Instr::br_if && static_cast<uint32_t>(stack.pop()) == 0)
                break;

            if (label_idx == labels.size())
                goto case_return;

            branch(label_idx, labels, stack, pc, immediates);
            break;
        }
        case Instr::br_table:
        {
            // immediates are: size of label vector, labels, default label
            const auto br_table_size = read<uint32_t>(immediates);
            const auto br_table_idx = stack.pop();

            const auto label_idx_offset = br_table_idx < br_table_size ?
                                              br_table_idx * sizeof(uint32_t) :
                                              br_table_size * sizeof(uint32_t);
            immediates += label_idx_offset;

            const auto label_idx = read<uint32_t>(immediates);

            if (label_idx == labels.size())
                goto case_return;

            branch(label_idx, labels, stack, pc, immediates);
            break;
        }
        case Instr::call:
        {
            const auto called_func_idx = read<uint32_t>(immediates);
            assert(called_func_idx <
                   instance.imported_functions.size() + instance.module.funcsec.size());
            const auto type_idx =
                called_func_idx < instance.imported_functions.size() ?
                    instance.imported_function_types[called_func_idx] :
                    instance.module.funcsec[called_func_idx - instance.imported_functions.size()];
            assert(type_idx < instance.module.typesec.size());

            if (!invoke_function(type_idx, called_func_idx, instance, stack))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::call_indirect:
        {
            const auto expected_type_idx = read<uint32_t>(immediates);
            assert(expected_type_idx < instance.module.typesec.size());

            const auto elem_idx = stack.pop();
            if (elem_idx >= instance.table.size())
            {
                trap = true;
                goto end;
            }

            const auto called_func_idx = instance.table[elem_idx];
            assert(called_func_idx <
                   instance.imported_functions.size() + instance.module.funcsec.size());

            // check actual type against expected type
            const auto actual_type_idx =
                called_func_idx < instance.imported_functions.size() ?
                    instance.imported_function_types[called_func_idx] :
                    instance.module.funcsec[called_func_idx - instance.imported_functions.size()];
            assert(actual_type_idx < instance.module.typesec.size());
            const auto& expected_type = instance.module.typesec[expected_type_idx];
            const auto& actual_type = instance.module.typesec[actual_type_idx];
            if (expected_type.inputs != actual_type.inputs ||
                expected_type.outputs != actual_type.outputs)
            {
                trap = true;
                goto end;
            }

            if (!invoke_function(actual_type_idx, called_func_idx, instance, stack))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::return_:
        case_return:
        {
            // TODO: Not needed, but satisfies the assert in the end of the main loop.
            labels.clear();

            assert(code_idx < instance.module.funcsec.size());
            const auto type_idx = instance.module.funcsec[code_idx];
            assert(type_idx < instance.module.typesec.size());
            const bool have_result = !instance.module.typesec[type_idx].outputs.empty();

            if (have_result)
            {
                const auto result = stack.peek();
                stack.clear();
                stack.push(result);
            }
            else
                stack.clear();

            goto end;
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
            locals[idx] = stack.peek();
            break;
        }
        case Instr::global_get:
        {
            const auto idx = read<uint32_t>(immediates);
            assert(idx <= instance.globals.size());
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
            if (!load_from_memory<uint32_t>(instance.memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i64_load:
        {
            if (!load_from_memory<uint64_t>(instance.memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i32_load8_s:
        {
            if (!load_from_memory<uint32_t, int8_t>(instance.memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i32_load8_u:
        {
            if (!load_from_memory<uint32_t, uint8_t>(instance.memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i32_load16_s:
        {
            if (!load_from_memory<uint32_t, int16_t>(instance.memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i32_load16_u:
        {
            if (!load_from_memory<uint32_t, uint16_t>(instance.memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i64_load8_s:
        {
            if (!load_from_memory<uint64_t, int8_t>(instance.memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i64_load8_u:
        {
            if (!load_from_memory<uint64_t, uint8_t>(instance.memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i64_load16_s:
        {
            if (!load_from_memory<uint64_t, int16_t>(instance.memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i64_load16_u:
        {
            if (!load_from_memory<uint64_t, uint16_t>(instance.memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i64_load32_s:
        {
            if (!load_from_memory<uint64_t, int32_t>(instance.memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i64_load32_u:
        {
            if (!load_from_memory<uint64_t, uint32_t>(instance.memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i32_store:
        {
            if (!store_into_memory<uint32_t>(instance.memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i64_store:
        {
            if (!store_into_memory<uint64_t>(instance.memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i32_store8:
        case Instr::i64_store8:
        {
            if (!store_into_memory<uint8_t>(instance.memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i32_store16:
        case Instr::i64_store16:
        {
            if (!store_into_memory<uint16_t>(instance.memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::i64_store32:
        {
            if (!store_into_memory<uint32_t>(instance.memory, stack, immediates))
            {
                trap = true;
                goto end;
            }
            break;
        }
        case Instr::memory_size:
        {
            stack.push(static_cast<uint32_t>(instance.memory.size() / PageSize));
            break;
        }
        case Instr::memory_grow:
        {
            const auto delta = static_cast<uint32_t>(stack.pop());
            const auto cur_pages = instance.memory.size() / PageSize;
            assert(cur_pages <= size_t(std::numeric_limits<int32_t>::max()));
            const auto new_pages = cur_pages + delta;
            assert(new_pages >= cur_pages);
            uint32_t ret = static_cast<uint32_t>(cur_pages);
            try
            {
                if (new_pages > instance.memory_max_pages)
                    throw std::bad_alloc();
                instance.memory.resize(new_pages * PageSize);
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
            auto const rhs = static_cast<int32_t>(stack.peek(0));
            auto const lhs = static_cast<int32_t>(stack.peek(1));
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
            auto const rhs = static_cast<uint32_t>(stack.peek());
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
            auto const rhs = static_cast<int32_t>(stack.peek());
            if (rhs == 0)
            {
                trap = true;
                goto end;
            }
            binary_op(stack, std::modulus<int32_t>());
            break;
        }
        case Instr::i32_rem_u:
        {
            auto const rhs = static_cast<uint32_t>(stack.peek());
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
            auto const rhs = static_cast<int64_t>(stack.peek(0));
            auto const lhs = static_cast<int64_t>(stack.peek(1));
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
            auto const rhs = static_cast<uint64_t>(stack.peek());
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
            auto const rhs = static_cast<int64_t>(stack.peek());
            if (rhs == 0)
            {
                trap = true;
                goto end;
            }
            binary_op(stack, std::modulus<int64_t>());
            break;
        }
        case Instr::i64_rem_u:
        {
            auto const rhs = static_cast<uint64_t>(stack.peek());
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
        default:
            assert(false);
            break;
        }
    }

end:
    assert(labels.empty());
    // move allows to return derived Stack<uint64_t> instance into base vector<uint64_t> value
    return {trap, std::move(stack)};
}

execution_result execute(const Module& module, FuncIdx func_idx, std::vector<uint64_t> args)
{
    auto instance = instantiate(module);
    return execute(instance, func_idx, std::move(args));
}

std::optional<FuncIdx> find_exported_function(const Module& module, std::string_view name)
{
    for (const auto& export_ : module.exportsec)
    {
        if (export_.kind == ExternalKind::Function && name == export_.name)
            return export_.index;
    }

    return {};
}
}  // namespace fizzy
