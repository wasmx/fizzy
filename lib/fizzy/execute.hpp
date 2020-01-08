#pragma once

#include "types.hpp"
#include <cstdint>

namespace fizzy
{
// The module instance.
struct instance;

// Instantiate a module.
instance instantiate(const module& _module);

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
execution_result execute(instance& _instance, funcidx _function, std::vector<uint64_t> _args);

// TODO: remove this helper
execution_result execute(const module& _module, funcidx _function, std::vector<uint64_t> _args);
}  // namespace fizzy
