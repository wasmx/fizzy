// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "exceptions.hpp"
#include "leb128.hpp"
#include "module.hpp"
#include <cstring>
#include <memory>

namespace fizzy
{
constexpr uint8_t wasm_prefix_data[]{0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
constexpr bytes_view wasm_prefix{wasm_prefix_data, sizeof(wasm_prefix_data)};

template <typename T>
using parser_result = std::pair<T, const uint8_t*>;

/// Parses `input` into a Module.
///
/// @param  input    The WebAssembly binary. No need to persist by the caller, since all relevant
///                  parts will be copied.
/// @return          The parsed module.
std::unique_ptr<const Module> parse(bytes_view input);

inline parser_result<uint8_t> parse_byte(const uint8_t* pos, const uint8_t* end)
{
    if (pos == end)
        throw parser_error{"unexpected EOF"};

    return {*pos, pos + 1};
}

template <typename T>
inline parser_result<T> parse_value(const uint8_t* pos, const uint8_t* end)
{
    constexpr ptrdiff_t size = sizeof(T);
    if ((end - pos) < size)
        throw parser_error{"unexpected EOF"};

    T value;
    memcpy(&value, pos, size);
    return {value, pos + size};
}

/// Parses `expr`, i.e. a function's instructions residing in the code section.
/// https://webassembly.github.io/spec/core/binary/instructions.html#binary-expr
///
/// @param  pos         The beginning of the expr binary input.
/// @param  end         The end of the binary input.
/// @param  func_idx    Index of the function being parsed.
/// @param  locals      Vector of local type and counts for the function being parsed.
/// @param  module      Module that this code is part of.
/// @return             The parsed code.
parser_result<Code> parse_expr(const uint8_t* pos, const uint8_t* end, FuncIdx func_idx,
    const std::vector<Locals>& locals, const Module& module);

/// Parses a string and validates it against UTF-8 encoding rules.
/// @param  pos    The beginning of the string input.
/// @param  end    The end of the string input.
/// @return        The parsed and UTF-8 validated string.
parser_result<std::string> parse_string(const uint8_t* pos, const uint8_t* end);

/// Parses the vec of i32 values.
/// This is used in parse_expr() (parser_expr.cpp).
parser_result<std::vector<uint32_t>> parse_vec_i32(const uint8_t* pos, const uint8_t* end);

/// Validates and converts the given byte to valtype.
ValType validate_valtype(uint8_t byte);
}  // namespace fizzy
