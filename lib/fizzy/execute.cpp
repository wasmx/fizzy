// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "asserts.hpp"
#include "cxx20/bit.hpp"
#include "stack.hpp"
#include "trunc_boundaries.hpp"
#include "types.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <stack>

namespace fizzy
{
namespace
{
// code_offset + stack_drop
constexpr auto BranchImmediateSize = 2 * sizeof(uint32_t);

constexpr uint32_t F32AbsMask = 0x7fffffff;
constexpr uint32_t F32SignMask = ~F32AbsMask;
constexpr uint64_t F64AbsMask = 0x7fffffffffffffff;
constexpr uint64_t F64SignMask = ~F64AbsMask;

template <typename T>
inline T read(const uint8_t*& input) noexcept
{
    T ret;
    __builtin_memcpy(&ret, input, sizeof(ret));
    input += sizeof(ret);
    return ret;
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
inline constexpr DstT extend(SrcT in) noexcept
{
    if constexpr (std::is_same_v<SrcT, DstT>)
        return in;
    else if constexpr (std::is_signed<SrcT>::value)
    {
        using SignedDstT = typename std::make_signed<DstT>::type;
        return static_cast<DstT>(SignedDstT{in});
    }
    else
        return DstT{in};
}

template <typename DstT, typename SrcT = DstT>
inline bool load_from_memory(
    bytes_view memory, OperandStack& stack, const uint8_t*& immediates) noexcept
{
    const auto address = stack.top().as<uint32_t>();
    // NOTE: alignment is dropped by the parser
    const auto offset = read<uint32_t>(immediates);
    // Addressing is 32-bit, but we keep the value as 64-bit to detect overflows.
    if ((uint64_t{address} + offset + sizeof(SrcT)) > memory.size())
        return false;

    const auto ret = load<SrcT>(memory, address + offset);
    stack.top() = extend<DstT>(ret);
    return true;
}

template <typename DstT>
inline constexpr DstT shrink(Value value) noexcept
{
    if constexpr (std::is_floating_point_v<DstT>)
        return value.as<DstT>();
    else
    {
        static_assert(std::is_integral_v<DstT>);
        // Could use `.as<DstT>()` would we have overloads for uint8_t/uint16_t,
        // however it does not seem to be worth it for this single occasion.
        return static_cast<DstT>(value.i64);
    }
}

template <typename DstT>
inline bool store_into_memory(
    bytes& memory, OperandStack& stack, const uint8_t*& immediates) noexcept
{
    const auto value = shrink<DstT>(stack.pop());
    const auto address = stack.pop().as<uint32_t>();
    // NOTE: alignment is dropped by the parser
    const auto offset = read<uint32_t>(immediates);
    // Addressing is 32-bit, but we keep the value as 64-bit to detect overflows.
    if ((uint64_t{address} + offset + sizeof(DstT)) > memory.size())
        return false;

    store<DstT>(memory, address + offset, value);
    return true;
}

/// Checks that exception is one of the types expected to be thrown from bytes::resize().
/// We catch ... in memory.grow implementation for the sake of smaller binary code and assert it's
/// one of expected exceptions.
[[maybe_unused]] bool is_resize_exception(std::exception_ptr exception) noexcept
{
    try
    {
        std::rethrow_exception(exception);
    }
    catch (const std::bad_alloc&)
    {
        return true;
    }
    catch (const std::length_error&)
    {
        return true;
    }
    catch (...)
    {
        return false;
    }
}

/// Increases the size of memory by @a delta_pages.
/// @return    Number of memory pages before expansion if successful, otherwise 2^32-1 in case
///            requested resize goes above @a memory_pages_limit or if allocation failed.
inline uint32_t grow_memory(
    bytes& memory, uint32_t delta_pages, uint32_t memory_pages_limit) noexcept
{
    const auto cur_pages = memory.size() / PageSize;
    assert(cur_pages <= size_t(std::numeric_limits<int32_t>::max()));
    const auto new_pages = cur_pages + delta_pages;
    assert(new_pages >= cur_pages);
    if (new_pages > memory_pages_limit)
        return static_cast<uint32_t>(-1);

    try
    {
        memory.resize(new_pages * PageSize);
        return static_cast<uint32_t>(cur_pages);
    }
    catch (...)
    {
        assert(is_resize_exception(std::current_exception()));
        return static_cast<uint32_t>(-1);
    }
}

/// Converts the top stack item by truncating a float value to an integer value.
template <typename SrcT, typename DstT>
inline bool trunc(OperandStack& stack) noexcept
{
    static_assert(std::is_floating_point_v<SrcT>);
    static_assert(std::is_integral_v<DstT>);
    using boundaries = trunc_boundaries<SrcT, DstT>;

    const auto input = stack.top().as<SrcT>();
    if (input > boundaries::lower && input < boundaries::upper)
    {
        assert(!std::isnan(input));
        assert(input != std::numeric_limits<SrcT>::infinity());
        assert(input != -std::numeric_limits<SrcT>::infinity());
        stack.top() = static_cast<DstT>(input);
        return true;
    }
    return false;
}

/// Converts the top stack item from an integer value to a float value.
template <typename SrcT, typename DstT>
inline void convert(OperandStack& stack) noexcept
{
    static_assert(std::is_integral_v<SrcT>);
    static_assert(std::is_floating_point_v<DstT>);
    stack.top() = static_cast<DstT>(stack.top().as<SrcT>());
}

/// Performs a bit_cast from SrcT type to DstT type.
///
/// This should be optimized to empty function in assembly. Except for f32 -> i32 where pushing
/// the result i32 value to the stack requires zero-extension to 64-bit.
template <typename SrcT, typename DstT>
inline void reinterpret(OperandStack& stack) noexcept
{
    static_assert(std::is_integral_v<SrcT> == std::is_floating_point_v<DstT> ||
                  std::is_floating_point_v<SrcT> == std::is_integral_v<DstT>);
    stack.top() = bit_cast<DstT>(stack.top().as<SrcT>());
}

template <typename Op>
inline void unary_op(OperandStack& stack, Op op) noexcept
{
    using T = decltype(op({}));
    const auto result = op(stack.top().as<T>());
    stack.top() = Value{result};  // Convert to Value, also from signed integer types.
}

template <typename Op>
inline void binary_op(OperandStack& stack, Op op) noexcept
{
    using T = decltype(op({}, {}));
    const auto val2 = stack.pop().as<T>();
    const auto val1 = stack.top().as<T>();
    const auto result = op(val1, val2);
    stack.top() = Value{result};  // Convert to Value, also from signed integer types.
}

template <typename T, template <typename> class Op>
inline void comparison_op(OperandStack& stack, Op<T> op) noexcept
{
    const auto val2 = stack.pop().as<T>();
    const auto val1 = stack.top().as<T>();
    stack.top() = uint32_t{op(val1, val2)};
}

template <typename T>
inline constexpr T add(T a, T b) noexcept
{
    return a + b;
}

template <typename T>
inline constexpr T sub(T a, T b) noexcept
{
    return a - b;
}

template <typename T>
inline constexpr T mul(T a, T b) noexcept
{
    return a * b;
}

template <typename T>
inline constexpr T div(T a, T b) noexcept
{
    return a / b;
}

template <typename T>
inline constexpr T rem(T a, T b) noexcept
{
    return a % b;
}

template <typename T>
inline constexpr T shift_left(T lhs, T rhs) noexcept
{
    static_assert(std::is_integral_v<T>);

    constexpr T num_bits{sizeof(T) * 8};
    const auto k = rhs & (num_bits - 1);
    return lhs << k;
}

template <typename T>
inline constexpr T shift_right(T lhs, T rhs) noexcept
{
    static_assert(std::is_integral_v<T>);

    constexpr T num_bits{sizeof(T) * 8};
    const auto k = rhs & (num_bits - 1);
    return lhs >> k;
}

template <typename T>
inline constexpr T rotl(T lhs, T rhs) noexcept
{
    static_assert(std::is_integral_v<T>);

    constexpr T num_bits{sizeof(T) * 8};
    const auto k = rhs & (num_bits - 1);

    if (k == 0)
        return lhs;

    return (lhs << k) | (lhs >> (num_bits - k));
}

template <typename T>
inline constexpr T rotr(T lhs, T rhs) noexcept
{
    static_assert(std::is_integral_v<T>);

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

constexpr uint32_t popcnt32(uint32_t value) noexcept
{
    return static_cast<uint32_t>(popcount(value));
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

constexpr uint64_t popcnt64(uint64_t value) noexcept
{
    return static_cast<uint64_t>(popcount(value));
}

template <typename T>
T fabs(T value) noexcept = delete;

template <>
inline float fabs(float value) noexcept
{
    return bit_cast<float>(bit_cast<uint32_t>(value) & F32AbsMask);
}

template <>
inline double fabs(double value) noexcept
{
    return bit_cast<double>(bit_cast<uint64_t>(value) & F64AbsMask);
}

template <typename T>
T fneg(T value) noexcept = delete;

template <>
inline float fneg(float value) noexcept
{
    return bit_cast<float>(bit_cast<uint32_t>(value) ^ F32SignMask);
}

template <>
inline double fneg(double value) noexcept
{
    return bit_cast<double>(bit_cast<uint64_t>(value) ^ F64SignMask);
}

template <typename T>
inline T fceil(T value) noexcept
{
    static_assert(std::is_floating_point_v<T>);
    if (std::isnan(value))
        return std::numeric_limits<T>::quiet_NaN();  // Positive canonical NaN.

    // The FE_INEXACT error is ignored (whenever the implementation reports it at all).
    return std::ceil(value);
}

template <typename T>
inline T ffloor(T value) noexcept
{
    static_assert(std::is_floating_point_v<T>);
    if (std::isnan(value))
        return std::numeric_limits<T>::quiet_NaN();  // Positive canonical NaN.

    // The FE_INEXACT error is ignored (whenever the implementation reports it at all).
    const auto result = std::floor(value);

    // TODO: GCC BUG WORKAROUND:
    // GCC implements std::floor() with  __builtin_floor().
    // When rounding direction is set to FE_DOWNWARD
    // the __builtin_floor() outputs -0 where it should +0.
    // The following workarounds the issue by using the fact that the sign of
    // the output must always match the sign of the input value.
    return std::copysign(result, value);
}

template <typename T>
inline T ftrunc(T value) noexcept
{
    static_assert(std::is_floating_point_v<T>);
    if (std::isnan(value))
        return std::numeric_limits<T>::quiet_NaN();  // Positive canonical NaN.

    // The FE_INEXACT error is ignored (whenever the implementation reports it at all).
    return std::trunc(value);
}

template <typename T>
T fnearest(T value) noexcept
{
    static_assert(std::is_floating_point_v<T>);

    if (std::isnan(value))
        return std::numeric_limits<T>::quiet_NaN();  // Positive canonical NaN.

    // Check if the input integer (as floating-point type) is even.
    // There is a faster way of doing that by bit manipulations of mantissa and exponent,
    // but this one is compact and this function is rarely called.
    // The argument i is expected to contain an integer value.
    static constexpr auto is_even = [](T i) noexcept { return std::fmod(i, T{2}) == T{0}; };

    // This implementation is based on adjusting the result produced by trunc() by +-1 when needed.
    const auto t = std::trunc(value);
    if (const auto diff = std::abs(value - t); diff > T{0.5} || (diff == T{0.5} && !is_even(t)))
        return t + std::copysign(T{1}, value);
    else
        return t;
}

template <typename T>
__attribute__((no_sanitize("float-divide-by-zero"))) inline constexpr T fdiv(T a, T b) noexcept
{
    static_assert(std::is_floating_point_v<T>);
    static_assert(std::numeric_limits<T>::is_iec559);
    return a / b;  // For IEC 559 (IEEE 754) floating-point types division by 0 is defined.
}

template <typename T>
inline T fmin(T a, T b) noexcept
{
    static_assert(std::is_floating_point_v<T>);

    if (std::isnan(a) || std::isnan(b))
        return std::numeric_limits<T>::quiet_NaN();  // Positive canonical NaN.

    if (a == 0 && b == 0 && (std::signbit(a) == 1 || std::signbit(b) == 1))
        return -T{0};

    return b < a ? b : a;
}

template <typename T>
inline T fmax(T a, T b) noexcept
{
    static_assert(std::is_floating_point_v<T>);

    if (std::isnan(a) || std::isnan(b))
        return std::numeric_limits<T>::quiet_NaN();  // Positive canonical NaN.

    if (a == 0 && b == 0 && (std::signbit(a) == 0 || std::signbit(b) == 0))
        return T{0};

    return a < b ? b : a;
}

template <typename T>
T fcopysign(T a, T b) noexcept = delete;

template <>
inline float fcopysign(float a, float b) noexcept
{
    const auto a_u = bit_cast<uint32_t>(a);
    const auto b_u = bit_cast<uint32_t>(b);
    return bit_cast<float>((a_u & F32AbsMask) | (b_u & F32SignMask));
}

template <>
inline double fcopysign(double a, double b) noexcept
{
    const auto a_u = bit_cast<uint64_t>(a);
    const auto b_u = bit_cast<uint64_t>(b);
    return bit_cast<double>((a_u & F64AbsMask) | (b_u & F64SignMask));
}

__attribute__((no_sanitize("float-cast-overflow"))) inline constexpr float demote(
    double value) noexcept
{
    // The float-cast-overflow UBSan check disabled for this conversion. In older clang versions
    // (up to 8.0) it reports a failure when non-infinity f64 value is converted to f32 infinity.
    // Such behavior is expected.
    return static_cast<float>(value);
}

void branch(const Code& code, OperandStack& stack, const uint8_t*& pc, uint32_t arity) noexcept
{
    const auto code_offset = read<uint32_t>(pc);
    const auto stack_drop = read<uint32_t>(pc);

    pc = code.instructions.data() + code_offset;

    // When branch is taken, additional stack items must be dropped.
    assert(static_cast<int>(stack_drop) >= 0);
    assert(stack.size() >= stack_drop + arity);
    if (arity != 0)
    {
        assert(arity == 1);
        const auto result = stack.top();
        stack.drop(stack_drop);
        stack.top() = result;
    }
    else
        stack.drop(stack_drop);
}

inline bool invoke_function(const FuncType& func_type, uint32_t func_idx, Instance& instance,
    OperandStack& stack, int depth) noexcept
{
    const auto num_args = func_type.inputs.size();
    assert(stack.size() >= num_args);
    const auto call_args = stack.rend() - num_args;

    const auto ret = execute(instance, func_idx, call_args, depth + 1);
    // Bubble up traps
    if (ret.trapped)
        return false;

    stack.drop(num_args);

    const auto num_outputs = func_type.outputs.size();
    // NOTE: we can assume these two from validation
    assert(num_outputs <= 1);
    assert(ret.has_value == (num_outputs == 1));
    // Push back the result
    if (num_outputs != 0)
        stack.push(ret.value);

    return true;
}

}  // namespace

ExecutionResult execute(Instance& instance, FuncIdx func_idx, const Value* args, int depth) noexcept
{
    assert(depth >= 0);
    if (depth >= CallStackLimit)
        return Trap;

    const auto& func_type = instance.module->get_function_type(func_idx);

    assert(instance.module->imported_function_types.size() == instance.imported_functions.size());
    if (func_idx < instance.imported_functions.size())
        return instance.imported_functions[func_idx].function(instance, args, depth);

    const auto& code = instance.module->get_code(func_idx);
    auto* const memory = instance.memory.get();

    OperandStack stack(args, func_type.inputs.size(), code.local_count,
        static_cast<size_t>(code.max_stack_height));

    const uint8_t* pc = code.instructions.data();

    while (true)
    {
        const auto instruction = static_cast<Instr>(*pc++);
        switch (instruction)
        {
        case Instr::unreachable:
            goto trap;
        case Instr::nop:
        case Instr::block:
        case Instr::loop:
            break;
        case Instr::if_:
        {
            if (stack.pop().as<uint32_t>() != 0)
                pc += sizeof(uint32_t);  // Skip the immediate for else instruction.
            else
            {
                const auto target_pc = read<uint32_t>(pc);
                pc = code.instructions.data() + target_pc;
            }
            break;
        }
        case Instr::else_:
        {
            // We reach else only after executing if block ("then" part),
            // so we need to skip else block now.
            const auto target_pc = read<uint32_t>(pc);
            pc = code.instructions.data() + target_pc;
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
            const auto arity = read<uint32_t>(pc);

            // Check condition for br_if.
            if (instruction == Instr::br_if && stack.pop().as<uint32_t>() == 0)
            {
                pc += BranchImmediateSize;
                break;
            }

            branch(code, stack, pc, arity);
            break;
        }
        case Instr::br_table:
        {
            const auto br_table_size = read<uint32_t>(pc);
            const auto arity = read<uint32_t>(pc);

            const auto br_table_idx = stack.pop().as<uint32_t>();

            const auto label_idx_offset = br_table_idx < br_table_size ?
                                              br_table_idx * BranchImmediateSize :
                                              br_table_size * BranchImmediateSize;
            pc += label_idx_offset;

            branch(code, stack, pc, arity);
            break;
        }
        case Instr::call:
        {
            const auto called_func_idx = read<uint32_t>(pc);
            const auto& called_func_type = instance.module->get_function_type(called_func_idx);

            if (!invoke_function(called_func_type, called_func_idx, instance, stack, depth))
                goto trap;
            break;
        }
        case Instr::call_indirect:
        {
            assert(instance.table != nullptr);

            const auto expected_type_idx = read<uint32_t>(pc);
            assert(expected_type_idx < instance.module->typesec.size());

            const auto elem_idx = stack.pop().as<uint32_t>();
            if (elem_idx >= instance.table->size())
                goto trap;

            const auto called_func = (*instance.table)[elem_idx];
            if (!called_func.instance)  // Table element not initialized.
                goto trap;

            // check actual type against expected type
            const auto& actual_type =
                called_func.instance->module->get_function_type(called_func.func_idx);
            const auto& expected_type = instance.module->typesec[expected_type_idx];
            if (expected_type != actual_type)
                goto trap;

            if (!invoke_function(
                    actual_type, called_func.func_idx, *called_func.instance, stack, depth))
                goto trap;
            break;
        }
        case Instr::drop:
        {
            stack.pop();
            break;
        }
        case Instr::select:
        {
            const auto condition = stack.pop().as<uint32_t>();
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
            const auto idx = read<uint32_t>(pc);
            stack.push(stack.local(idx));
            break;
        }
        case Instr::local_set:
        {
            const auto idx = read<uint32_t>(pc);
            stack.local(idx) = stack.pop();
            break;
        }
        case Instr::local_tee:
        {
            const auto idx = read<uint32_t>(pc);
            stack.local(idx) = stack.top();
            break;
        }
        case Instr::global_get:
        {
            const auto idx = read<uint32_t>(pc);
            assert(idx < instance.imported_globals.size() + instance.globals.size());
            if (idx < instance.imported_globals.size())
            {
                stack.push(*instance.imported_globals[idx].value);
            }
            else
            {
                const auto module_global_idx = idx - instance.imported_globals.size();
                assert(module_global_idx < instance.module->globalsec.size());
                stack.push(instance.globals[module_global_idx]);
            }
            break;
        }
        case Instr::global_set:
        {
            const auto idx = read<uint32_t>(pc);
            if (idx < instance.imported_globals.size())
            {
                assert(instance.imported_globals[idx].type.is_mutable);
                *instance.imported_globals[idx].value = stack.pop();
            }
            else
            {
                const auto module_global_idx = idx - instance.imported_globals.size();
                assert(module_global_idx < instance.module->globalsec.size());
                assert(instance.module->globalsec[module_global_idx].type.is_mutable);
                instance.globals[module_global_idx] = stack.pop();
            }
            break;
        }
        case Instr::i32_load:
        {
            if (!load_from_memory<uint32_t>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::i64_load:
        {
            if (!load_from_memory<uint64_t>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::f32_load:
        {
            if (!load_from_memory<float>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::f64_load:
        {
            if (!load_from_memory<double>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::i32_load8_s:
        {
            if (!load_from_memory<uint32_t, int8_t>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::i32_load8_u:
        {
            if (!load_from_memory<uint32_t, uint8_t>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::i32_load16_s:
        {
            if (!load_from_memory<uint32_t, int16_t>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::i32_load16_u:
        {
            if (!load_from_memory<uint32_t, uint16_t>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::i64_load8_s:
        {
            if (!load_from_memory<uint64_t, int8_t>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::i64_load8_u:
        {
            if (!load_from_memory<uint64_t, uint8_t>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::i64_load16_s:
        {
            if (!load_from_memory<uint64_t, int16_t>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::i64_load16_u:
        {
            if (!load_from_memory<uint64_t, uint16_t>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::i64_load32_s:
        {
            if (!load_from_memory<uint64_t, int32_t>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::i64_load32_u:
        {
            if (!load_from_memory<uint64_t, uint32_t>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::i32_store:
        {
            if (!store_into_memory<uint32_t>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::i64_store:
        {
            if (!store_into_memory<uint64_t>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::f32_store:
        {
            if (!store_into_memory<float>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::f64_store:
        {
            if (!store_into_memory<double>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::i32_store8:
        case Instr::i64_store8:
        {
            if (!store_into_memory<uint8_t>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::i32_store16:
        case Instr::i64_store16:
        {
            if (!store_into_memory<uint16_t>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::i64_store32:
        {
            if (!store_into_memory<uint32_t>(*memory, stack, pc))
                goto trap;
            break;
        }
        case Instr::memory_size:
        {
            stack.push(static_cast<uint32_t>(memory->size() / PageSize));
            break;
        }
        case Instr::memory_grow:
        {
            stack.top() =
                grow_memory(*memory, stack.top().as<uint32_t>(), instance.memory_pages_limit);
            break;
        }
        case Instr::i32_const:
        case Instr::f32_const:
        {
            const auto value = read<uint32_t>(pc);
            stack.push(value);
            break;
        }
        case Instr::i64_const:
        case Instr::f64_const:
        {
            const auto value = read<uint64_t>(pc);
            stack.push(value);
            break;
        }
        case Instr::i32_eqz:
        {
            stack.top() = uint32_t{stack.top().as<uint32_t>() == 0};
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
            stack.top() = uint32_t{stack.top().i64 == 0};
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

        case Instr::f32_eq:
        {
            comparison_op(stack, std::equal_to<float>());
            break;
        }
        case Instr::f32_ne:
        {
            comparison_op(stack, std::not_equal_to<float>());
            break;
        }
        case Instr::f32_lt:
        {
            comparison_op(stack, std::less<float>());
            break;
        }
        case Instr::f32_gt:
        {
            comparison_op<float>(stack, std::greater<float>());
            break;
        }
        case Instr::f32_le:
        {
            comparison_op(stack, std::less_equal<float>());
            break;
        }
        case Instr::f32_ge:
        {
            comparison_op(stack, std::greater_equal<float>());
            break;
        }

        case Instr::f64_eq:
        {
            comparison_op(stack, std::equal_to<double>());
            break;
        }
        case Instr::f64_ne:
        {
            comparison_op(stack, std::not_equal_to<double>());
            break;
        }
        case Instr::f64_lt:
        {
            comparison_op(stack, std::less<double>());
            break;
        }
        case Instr::f64_gt:
        {
            comparison_op<double>(stack, std::greater<double>());
            break;
        }
        case Instr::f64_le:
        {
            comparison_op(stack, std::less_equal<double>());
            break;
        }
        case Instr::f64_ge:
        {
            comparison_op(stack, std::greater_equal<double>());
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
            binary_op(stack, add<uint32_t>);
            break;
        }
        case Instr::i32_sub:
        {
            binary_op(stack, sub<uint32_t>);
            break;
        }
        case Instr::i32_mul:
        {
            binary_op(stack, mul<uint32_t>);
            break;
        }
        case Instr::i32_div_s:
        {
            const auto rhs = stack.pop().as<int32_t>();
            const auto lhs = stack.top().as<int32_t>();
            if (rhs == 0 || (lhs == std::numeric_limits<int32_t>::min() && rhs == -1))
                goto trap;
            stack.top() = div(lhs, rhs);
            break;
        }
        case Instr::i32_div_u:
        {
            const auto rhs = stack.pop().as<uint32_t>();
            if (rhs == 0)
                goto trap;
            const auto lhs = stack.top().as<uint32_t>();
            stack.top() = div(lhs, rhs);
            break;
        }
        case Instr::i32_rem_s:
        {
            const auto rhs = stack.pop().as<int32_t>();
            if (rhs == 0)
                goto trap;
            const auto lhs = stack.top().as<int32_t>();
            if (lhs == std::numeric_limits<int32_t>::min() && rhs == -1)
                stack.top() = int32_t{0};
            else
                stack.top() = rem(lhs, rhs);
            break;
        }
        case Instr::i32_rem_u:
        {
            const auto rhs = stack.pop().as<uint32_t>();
            if (rhs == 0)
                goto trap;
            const auto lhs = stack.top().as<uint32_t>();
            stack.top() = rem(lhs, rhs);
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
            binary_op(stack, add<uint64_t>);
            break;
        }
        case Instr::i64_sub:
        {
            binary_op(stack, sub<uint64_t>);
            break;
        }
        case Instr::i64_mul:
        {
            binary_op(stack, mul<uint64_t>);
            break;
        }
        case Instr::i64_div_s:
        {
            const auto rhs = stack.pop().as<int64_t>();
            const auto lhs = stack.top().as<int64_t>();
            if (rhs == 0 || (lhs == std::numeric_limits<int64_t>::min() && rhs == -1))
                goto trap;
            stack.top() = div(lhs, rhs);
            break;
        }
        case Instr::i64_div_u:
        {
            const auto rhs = stack.pop().i64;
            if (rhs == 0)
                goto trap;
            const auto lhs = stack.top().i64;
            stack.top() = div(lhs, rhs);
            break;
        }
        case Instr::i64_rem_s:
        {
            const auto rhs = stack.pop().as<int64_t>();
            if (rhs == 0)
                goto trap;
            const auto lhs = stack.top().as<int64_t>();
            if (lhs == std::numeric_limits<int64_t>::min() && rhs == -1)
                stack.top() = int64_t{0};
            else
                stack.top() = rem(lhs, rhs);
            break;
        }
        case Instr::i64_rem_u:
        {
            const auto rhs = stack.pop().i64;
            if (rhs == 0)
                goto trap;
            const auto lhs = stack.top().i64;
            stack.top() = rem(lhs, rhs);
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

        case Instr::f32_abs:
        {
            unary_op(stack, fabs<float>);
            break;
        }
        case Instr::f32_neg:
        {
            unary_op(stack, fneg<float>);
            break;
        }
        case Instr::f32_ceil:
        {
            unary_op(stack, fceil<float>);
            break;
        }
        case Instr::f32_floor:
        {
            unary_op(stack, ffloor<float>);
            break;
        }
        case Instr::f32_trunc:
        {
            unary_op(stack, ftrunc<float>);
            break;
        }
        case Instr::f32_nearest:
        {
            unary_op(stack, fnearest<float>);
            break;
        }
        case Instr::f32_sqrt:
        {
            unary_op(stack, static_cast<float (*)(float)>(std::sqrt));
            break;
        }

        case Instr::f32_add:
        {
            binary_op(stack, add<float>);
            break;
        }
        case Instr::f32_sub:
        {
            binary_op(stack, sub<float>);
            break;
        }
        case Instr::f32_mul:
        {
            binary_op(stack, mul<float>);
            break;
        }
        case Instr::f32_div:
        {
            binary_op(stack, fdiv<float>);
            break;
        }
        case Instr::f32_min:
        {
            binary_op(stack, fmin<float>);
            break;
        }
        case Instr::f32_max:
        {
            binary_op(stack, fmax<float>);
            break;
        }
        case Instr::f32_copysign:
        {
            binary_op(stack, fcopysign<float>);
            break;
        }

        case Instr::f64_abs:
        {
            unary_op(stack, fabs<double>);
            break;
        }
        case Instr::f64_neg:
        {
            unary_op(stack, fneg<double>);
            break;
        }
        case Instr::f64_ceil:
        {
            unary_op(stack, fceil<double>);
            break;
        }
        case Instr::f64_floor:
        {
            unary_op(stack, ffloor<double>);
            break;
        }
        case Instr::f64_trunc:
        {
            unary_op(stack, ftrunc<double>);
            break;
        }
        case Instr::f64_nearest:
        {
            unary_op(stack, fnearest<double>);
            break;
        }
        case Instr::f64_sqrt:
        {
            unary_op(stack, static_cast<double (*)(double)>(std::sqrt));
            break;
        }

        case Instr::f64_add:
        {
            binary_op(stack, add<double>);
            break;
        }
        case Instr::f64_sub:
        {
            binary_op(stack, sub<double>);
            break;
        }
        case Instr::f64_mul:
        {
            binary_op(stack, mul<double>);
            break;
        }
        case Instr::f64_div:
        {
            binary_op(stack, fdiv<double>);
            break;
        }
        case Instr::f64_min:
        {
            binary_op(stack, fmin<double>);
            break;
        }
        case Instr::f64_max:
        {
            binary_op(stack, fmax<double>);
            break;
        }
        case Instr::f64_copysign:
        {
            binary_op(stack, fcopysign<double>);
            break;
        }

        case Instr::i32_wrap_i64:
        {
            stack.top() = static_cast<uint32_t>(stack.top().i64);
            break;
        }
        case Instr::i32_trunc_f32_s:
        {
            if (!trunc<float, int32_t>(stack))
                goto trap;
            break;
        }
        case Instr::i32_trunc_f32_u:
        {
            if (!trunc<float, uint32_t>(stack))
                goto trap;
            break;
        }
        case Instr::i32_trunc_f64_s:
        {
            if (!trunc<double, int32_t>(stack))
                goto trap;
            break;
        }
        case Instr::i32_trunc_f64_u:
        {
            if (!trunc<double, uint32_t>(stack))
                goto trap;
            break;
        }
        case Instr::i64_extend_i32_s:
        {
            stack.top() = int64_t{stack.top().as<int32_t>()};
            break;
        }
        case Instr::i64_extend_i32_u:
        {
            stack.top() = uint64_t{stack.top().i32};
            break;
        }
        case Instr::i64_trunc_f32_s:
        {
            if (!trunc<float, int64_t>(stack))
                goto trap;
            break;
        }
        case Instr::i64_trunc_f32_u:
        {
            if (!trunc<float, uint64_t>(stack))
                goto trap;
            break;
        }
        case Instr::i64_trunc_f64_s:
        {
            if (!trunc<double, int64_t>(stack))
                goto trap;
            break;
        }
        case Instr::i64_trunc_f64_u:
        {
            if (!trunc<double, uint64_t>(stack))
                goto trap;
            break;
        }
        case Instr::f32_convert_i32_s:
        {
            convert<int32_t, float>(stack);
            break;
        }
        case Instr::f32_convert_i32_u:
        {
            convert<uint32_t, float>(stack);
            break;
        }
        case Instr::f32_convert_i64_s:
        {
            convert<int64_t, float>(stack);
            break;
        }
        case Instr::f32_convert_i64_u:
        {
            convert<uint64_t, float>(stack);
            break;
        }
        case Instr::f32_demote_f64:
        {
            stack.top() = demote(stack.top().f64);
            break;
        }
        case Instr::f64_convert_i32_s:
        {
            convert<int32_t, double>(stack);
            break;
        }
        case Instr::f64_convert_i32_u:
        {
            convert<uint32_t, double>(stack);
            break;
        }
        case Instr::f64_convert_i64_s:
        {
            convert<int64_t, double>(stack);
            break;
        }
        case Instr::f64_convert_i64_u:
        {
            convert<uint64_t, double>(stack);
            break;
        }
        case Instr::f64_promote_f32:
        {
            stack.top() = double{stack.top().f32};
            break;
        }
        case Instr::i32_reinterpret_f32:
        {
            reinterpret<float, uint32_t>(stack);
            break;
        }
        case Instr::i64_reinterpret_f64:
        {
            reinterpret<double, uint64_t>(stack);
            break;
        }
        case Instr::f32_reinterpret_i32:
        {
            reinterpret<uint32_t, float>(stack);
            break;
        }
        case Instr::f64_reinterpret_i64:
        {
            reinterpret<uint64_t, double>(stack);
            break;
        }

        default:
            FIZZY_UNREACHABLE();
        }
    }

end:
    assert(pc == &code.instructions[code.instructions.size()]);  // End of code must be reached.
    assert(stack.size() == instance.module->get_function_type(func_idx).outputs.size());

    return stack.size() != 0 ? ExecutionResult{stack.top()} : Void;

trap:
    return Trap;
}
}  // namespace fizzy
