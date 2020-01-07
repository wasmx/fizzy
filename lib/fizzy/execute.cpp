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
};
}  // namespace

template <typename T>
inline T read(const uint8_t*& input) noexcept
{
    T ret;
    __builtin_memcpy(&ret, input, sizeof(ret));
    input += sizeof(ret);
    return ret;
}

std::vector<uint64_t> execute(const module& _module, funcidx _function, std::vector<uint64_t> _args)
{
    const auto& code = _module.codesec[_function];

    std::vector<uint64_t> locals = std::move(_args);
    locals.resize(locals.size() + code.local_count);

    // TODO: preallocate fixed stack depth properly
    uint64_stack stack;

    const instr* pc = code.instructions.data();
    const uint8_t* immediates = code.immediates.data();

    while (true)
    {
        const auto instruction = *pc++;
        switch (instruction)
        {
        case instr::end:
            goto end;
        case instr::local_get: {
            const auto idx = read<uint32_t>(immediates);
            assert(idx <= locals.size());
            stack.push(locals[idx]);
            break;
        }
        case instr::local_set: {
            const auto idx = read<uint32_t>(immediates);
            assert(idx <= locals.size());
            locals[idx] = stack.pop();
            break;
        }
        case instr::local_tee: {
            const auto idx = read<uint32_t>(immediates);
            assert(idx <= locals.size());
            locals[idx] = stack.back();
            break;
        }
        case instr::i32_add: {
            const auto a = static_cast<uint32_t>(stack.pop());
            const auto b = static_cast<uint32_t>(stack.pop());
            stack.push(a + b);
            break;
        }
        default:
            assert(false);
            break;
        }
    }

end:
    // move allows to return derived uint64_stack instance into base vector<uint64_t> value
    return std::move(stack);
}
}  // namespace fizzy