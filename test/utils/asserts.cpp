// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "asserts.hpp"
#include <ostream>

namespace fizzy
{
std::ostream& operator<<(std::ostream& os, ExecutionResult result)
{
    if (result.trapped)
        return os << "trapped";

    const auto format_flags = os.flags();
    os << "result(";
    if (result.has_value)
        os << result.value.i64 << " [0x" << std::hex << result.value.i64 << "]";
    os << ")";
    os.flags(format_flags);
    return os;
}
}  // namespace fizzy
