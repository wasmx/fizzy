#pragma once

#include "types.hpp"
#include <cstdint>

namespace fizzy
{
std::vector<uint64_t> execute(
    const module& _module, funcidx _function, std::vector<uint64_t> _args);
}  // namespace fizzy
