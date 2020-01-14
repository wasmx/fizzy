#pragma once
#include <cstdint>
#include <vector>

namespace fizzy
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
}  // namespace fizzy
