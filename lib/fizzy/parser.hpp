// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "exceptions.hpp"
#include "leb128.hpp"
#include "module.hpp"
#include <tuple>

namespace fizzy
{
constexpr uint8_t wasm_prefix_data[]{0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
constexpr bytes_view wasm_prefix{wasm_prefix_data, sizeof(wasm_prefix_data)};

template <typename T>
using parser_result = std::tuple<T, const uint8_t*>;

Module parse(bytes_view input);

inline const uint8_t* skip(size_t num_bytes, const uint8_t* input, const uint8_t* end)
{
    const uint8_t* ret = input + num_bytes;
    if (ret >= end)
        throw parser_error{"unexpected EOF"};
    return ret;
}

inline parser_result<uint8_t> parse_byte(const uint8_t* pos, const uint8_t* end)
{
    if (pos == end)
        throw parser_error{"unexpected EOF"};

    return {*pos, pos + 1};
}

/// Parse `expr`, i.e. a function's instructions residing in the code section.
/// https://webassembly.github.io/spec/core/binary/instructions.html#binary-expr
///
/// @param input    The beginning of the expr binary input.
/// @param end      The end of the binary input.
/// @param module   Module that this code is part of.
parser_result<Code> parse_expr(const uint8_t* input, const uint8_t* end, const Module& module);

parser_result<std::string> parse_string(const uint8_t* pos, const uint8_t* end);

/// Parses the vec of i32 values.
/// This is used in parse_expr() (parser_expr.cpp).
parser_result<std::vector<uint32_t>> parse_vec_i32(const uint8_t* pos, const uint8_t* end);

/// Validates and converts the given byte to valtype.
ValType validate_valtype(uint8_t byte);
}  // namespace fizzy
