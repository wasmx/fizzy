#pragma once

#include "types.hpp"
#include <cstdint>

namespace fizzy
{
// The module instance.
struct Instance;

// Instantiate a module.
Instance instantiate(const Module& module);

// The result of an execution.
struct execution_result
{
    // true if execution resulted in a trap
    bool trapped;
    // the resulting stack (e.g. return values)
    // NOTE: this can be either 0 or 1 items
    std::vector<uint64_t> stack;
};

// Execute a function on an instance.
execution_result execute(Instance& instance, FuncIdx function, std::vector<uint64_t> args);

// TODO: remove this helper
execution_result execute(const Module& module, FuncIdx function, std::vector<uint64_t> args);
}  // namespace fizzy
