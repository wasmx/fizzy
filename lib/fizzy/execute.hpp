#pragma once

#include "types.hpp"
#include <cstdint>

namespace fizzy
{
struct execution_result
{
    // true if execution resulted in a trap
    bool trapped;
    // the resulting stack (e.g. return values)
    // NOTE: this can be either 0 or 1 items
    std::vector<uint64_t> stack;
};

execution_result execute(const module& _module, funcidx _function, std::vector<uint64_t> _args);
}  // namespace fizzy
