// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <stdexcept>
#include <string>

namespace fizzy::test
{
void validate_function_signature(std::string_view signature)
{
    if (signature.find_first_of(":") == std::string::npos)
        throw std::runtime_error{"Missing ':' delimiter"};
    if (signature.find_first_of(":") != signature.find_last_of(":"))
        throw std::runtime_error{"Multiple occurrences of ':' found in signature"};
    // Only allow i (i32) I (i64) as types
    if (signature.find_first_not_of(":iI") != std::string::npos)
        throw std::runtime_error{"Invalid type found in signature"};
}
}  // namespace fizzy::test
