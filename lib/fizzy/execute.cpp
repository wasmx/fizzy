#include "execute.hpp"
#include "types.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>

namespace fizzy
{
template <typename T>
inline T load(const uint8_t* input) noexcept
{
    T ret;
    __builtin_memcpy(&ret, input, sizeof(ret));
    return ret;
}

std::vector<uint64_t> execute(const module& _module, funcidx _function, std::vector<uint64_t> _args)
{
    const auto& code = _module.codesec[_function];

    std::vector<uint64_t> locals = std::move(_args);
    locals.resize(locals.size() + code.local_count);

    // TODO: preallocate fixed stack depth properly
    std::vector<uint64_t> stack;

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
            const auto idx = load<uint32_t>(immediates);
            immediates += sizeof(uint32_t);
            stack.emplace_back(locals[idx]);
            break;
        }
        case instr::i32_add: {
            const auto a = static_cast<uint32_t>(stack.back());
            stack.pop_back();
            const auto b = static_cast<uint32_t>(stack.back());
            stack.pop_back();
            stack.emplace_back(a + b);
            break;
        }
        default:
            assert(false);
            break;
        }
    }

end:
    return stack;
}
}  // namespace fizzy