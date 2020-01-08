#include "execute.hpp"
#include "types.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>

namespace fizzy
{
namespace
{
class uint64_stack : public std::vector<uint64_t>
{
public:
    using vector::vector;

    void push(uint64_t val) { emplace_back(val); }

    uint64_t pop()
    {
        auto const res = back();
        pop_back();
        return res;
    }

    uint64_t peek(difference_type depth = 1) const noexcept { return *(end() - depth); }
};

template <typename T>
inline T read(const uint8_t*& input) noexcept
{
    T ret;
    __builtin_memcpy(&ret, input, sizeof(ret));
    input += sizeof(ret);
    return ret;
}

template <typename Op>
inline void binary_op(uint64_stack& stack, Op op) noexcept
{
    using T = decltype(op(stack.pop(), stack.pop()));
    const auto a = static_cast<T>(stack.pop());
    const auto b = static_cast<T>(stack.pop());
    stack.push(static_cast<uint64_t>(op(a, b)));
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
}  // namespace

execution_result execute(const module& _module, funcidx _function, std::vector<uint64_t> _args)
{
    const auto& code = _module.codesec[_function];

    std::vector<uint64_t> locals = std::move(_args);
    locals.resize(locals.size() + code.local_count);

    // TODO: preallocate fixed stack depth properly
    uint64_stack stack;

    bool trap = false;

    const instr* pc = code.instructions.data();
    const uint8_t* immediates = code.immediates.data();

    while (true)
    {
        const auto instruction = *pc++;
        switch (instruction)
        {
        case instr::unreachable:
            trap = true;
            goto end;
        case instr::nop:
            break;
        case instr::end:
            goto end;
        case instr::drop:
        {
            stack.pop();
            break;
        }
        case instr::select:
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
        case instr::local_get:
        {
            const auto idx = read<uint32_t>(immediates);
            assert(idx <= locals.size());
            stack.push(locals[idx]);
            break;
        }
        case instr::local_set:
        {
            const auto idx = read<uint32_t>(immediates);
            assert(idx <= locals.size());
            locals[idx] = stack.pop();
            break;
        }
        case instr::local_tee:
        {
            const auto idx = read<uint32_t>(immediates);
            assert(idx <= locals.size());
            locals[idx] = stack.back();
            break;
        }
        case instr::i32_eqz:
        {
            const auto value = static_cast<uint32_t>(stack.pop());
            stack.push(value == 0);
            break;
        }
        case instr::i32_eq:
        {
            const auto lhs = static_cast<uint32_t>(stack.pop());
            const auto rhs = static_cast<uint32_t>(stack.pop());
            stack.push(lhs == rhs);
            break;
        }
        case instr::i32_ne:
        {
            const auto lhs = static_cast<uint32_t>(stack.pop());
            const auto rhs = static_cast<uint32_t>(stack.pop());
            stack.push(lhs != rhs);
            break;
        }
        case instr::i32_add:
        {
            binary_op(stack, std::plus<uint32_t>());
            break;
        }
        case instr::i32_sub:
        {
            binary_op(stack, std::minus<uint32_t>());
            break;
        }
        case instr::i32_mul:
        {
            binary_op(stack, std::multiplies<uint32_t>());
            break;
        }
        case instr::i32_div_s:
        {
            auto const lhs = static_cast<int32_t>(stack.peek(1));
            auto const rhs = static_cast<int32_t>(stack.peek(2));
            if (rhs == 0 || (lhs == std::numeric_limits<int32_t>::min() && rhs == -1))
            {
                trap = true;
                goto end;
            }
            binary_op(stack, std::divides<int32_t>());
            break;
        }
        case instr::i32_div_u:
        {
            auto const rhs = static_cast<uint32_t>(stack.peek(2));
            if (rhs == 0)
            {
                trap = true;
                goto end;
            }
            binary_op(stack, std::divides<uint32_t>());
            break;
        }
        case instr::i32_rem_s:
        {
            auto const rhs = static_cast<int32_t>(stack.peek(2));
            if (rhs == 0)
            {
                trap = true;
                goto end;
            }
            binary_op(stack, std::modulus<int32_t>());
            break;
        }
        case instr::i32_rem_u:
        {
            auto const rhs = static_cast<uint32_t>(stack.peek(2));
            if (rhs == 0)
            {
                trap = true;
                goto end;
            }
            binary_op(stack, std::modulus<uint32_t>());
            break;
        }
        case instr::i32_and:
        {
            binary_op(stack, std::bit_and<uint32_t>());
            break;
        }
        case instr::i32_or:
        {
            binary_op(stack, std::bit_or<uint32_t>());
            break;
        }
        case instr::i32_xor:
        {
            binary_op(stack, std::bit_xor<uint32_t>());
            break;
        }
        case instr::i32_shl:
        {
            binary_op(stack, shift_left<uint32_t>);
            break;
        }
        case instr::i32_shr_s:
        {
            binary_op(stack, shift_right<int32_t>);
            break;
        }
        case instr::i32_shr_u:
        {
            binary_op(stack, shift_right<uint32_t>);
            break;
        }
        case instr::i32_rotl:
        {
            binary_op(stack, rotl<uint32_t>);
            break;
        }
        case instr::i32_rotr:
        {
            binary_op(stack, rotr<uint32_t>);
            break;
        }
        case instr::i32_wrap_i64:
        {
            stack.push(static_cast<uint32_t>(stack.pop()));
            break;
        }
        case instr::i64_extend_i32_s:
        {
            const auto value = static_cast<int32_t>(stack.pop());
            stack.push(static_cast<uint64_t>(int64_t{value}));
            break;
        }
        case instr::i64_extend_i32_u:
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
    // move allows to return derived uint64_stack instance into base vector<uint64_t> value
    return {trap, std::move(stack)};
}
}  // namespace fizzy
