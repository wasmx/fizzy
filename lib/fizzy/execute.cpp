#include "execute.hpp"
#include "types.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>

namespace fizzy
{
std::vector<uint64_t> execute(const module& _module, funcidx _function, std::vector<uint64_t> _args)
{
    const auto& code = _module.codesec[_function];

    std::vector<uint64_t> locals = std::move(_args);
    locals.resize(locals.size() + code.local_count);

    // TODO: preallocate fixed stack depth properly
    std::vector<uint64_t> stack;

    const instr* pc = code.instructions.data();

    while (true)
    {
        const auto instruction = *pc++;
        switch (instruction)
        {
        case instr::end:
            goto end;
        default:
            assert(false);
            break;
        }
    }

end:
    return stack;
}
}  // namespace fizzy