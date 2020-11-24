// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "asserts.hpp"
#include <ostream>

namespace
{
template <typename ResultT>
inline void output_result(std::ostream& os, ResultT result)
{
    if (result.trapped)
    {
        os << "trapped";
        return;
    }

    const auto format_flags = os.flags();
    os << "result(";
    if (result.has_value)
        os << result.value.i64 << " [0x" << std::hex << result.value.i64 << "]";
    os << ")";
    os.flags(format_flags);
}
}  // namespace

std::ostream& operator<<(std::ostream& os, FizzyExecutionResult result)
{
    output_result(os, result);
    return os;
}

namespace fizzy
{
std::ostream& operator<<(std::ostream& os, ExecutionResult result)
{
    output_result(os, result);
    return os;
}
}  // namespace fizzy
