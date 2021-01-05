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
        os << std::dec << result.value.i64 << " [0x" << std::hex << result.value.i64 << "]";
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

namespace test
{
std::ostream& operator<<(std::ostream& os, const TypedExecutionResult& result)
{
    const auto format_flags = os.flags();

    if (result.has_value)
    {
        os << "result(";
        switch (result.type)
        {
        case ValType::i32:
            os << std::dec << result.value.as<uint32_t>() << " [0x" << std::hex
               << result.value.as<uint32_t>() << "] (i32)";
            break;
        case ValType::i64:
            os << std::dec << result.value.i64 << " [0x" << std::hex << result.value.i64
               << "] (i64)";
            break;
        case ValType::f32:
            os << result.value.f32 << " (f32)";
            break;
        case ValType::f64:
            os << result.value.f64 << " (f64)";
            break;
        }
        os << ")";
    }
    else
        os << ExecutionResult{result};

    os.flags(format_flags);
    return os;
}
}  // namespace test
}  // namespace fizzy
