// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "asserts.hpp"
#include <ostream>

namespace fizzy
{
std::ostream& operator<<(std::ostream& os, execution_result result)
{
    if (result.trapped)
        return os << "trapped";

    os << "result(";
    std::string_view separator;
    for (const auto& x : result.stack)
    {
        os << separator << x;
        separator = ", ";
    }
    os << ")";
    return os;
}
}  // namespace fizzy