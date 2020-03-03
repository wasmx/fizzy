#pragma once

#include "exceptions.hpp"
#include "leb128.hpp"
#include "types.hpp"
#include <tuple>

namespace fizzy
{
constexpr uint8_t wasm_prefix_data[]{0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
constexpr bytes_view wasm_prefix{wasm_prefix_data, sizeof(wasm_prefix_data)};

template <typename T>
using parser_result = std::tuple<T, const uint8_t*>;

Module parse(bytes_view input);

parser_result<Code> parse_expr(const uint8_t* input, const uint8_t* end);

template <typename T>
parser_result<T> parse(const uint8_t* pos, const uint8_t* end);

parser_result<std::string> parse_string(const uint8_t* pos, const uint8_t* end);

inline parser_result<Limits> parse_limits(const uint8_t* pos, const uint8_t* end)
{
    if (pos == end)
        throw parser_error{"Unexpected EOF"};

    Limits result;
    const auto b = *pos++;
    switch (b)
    {
    case 0x00:
        std::tie(result.min, pos) = leb128u_decode<uint32_t>(pos, end);
        return {result, pos};
    case 0x01:
        std::tie(result.min, pos) = leb128u_decode<uint32_t>(pos, end);
        std::tie(result.max, pos) = leb128u_decode<uint32_t>(pos, end);
        if (result.min > *result.max)
            throw parser_error("malformed limits (minimum is larger than maximum)");
        return {result, pos};
    default:
        throw parser_error{"invalid limits " + std::to_string(b)};
    }
}

/// Parses the vec of i32 values.
/// This is used in parse_expr() (parser_expr.cpp).
parser_result<std::vector<uint32_t>> parse_vec_i32(const uint8_t* pos, const uint8_t* end);

const uint8_t* validate_valtype(const uint8_t* pos, const uint8_t* end);
}  // namespace fizzy
