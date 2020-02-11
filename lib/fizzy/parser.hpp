#pragma once

#include "exceptions.hpp"
#include "leb128.hpp"
#include "types.hpp"
#include <tuple>

namespace fizzy
{
constexpr uint8_t wasm_prefix_data[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
constexpr bytes_view wasm_prefix{wasm_prefix_data, sizeof(wasm_prefix_data)};

template <typename T>
using parser_result = std::tuple<T, const uint8_t*>;

Module parse(bytes_view input);
parser_result<Code> parse_expr(const uint8_t* input);

template <typename T>
parser_result<T> parse(const uint8_t* pos);

template <>
inline parser_result<uint8_t> parse(const uint8_t* pos)
{
    const auto result = *pos;
    ++pos;
    return {result, pos};
}

template <>
inline parser_result<uint32_t> parse(const uint8_t* pos)
{
    return leb128u_decode<uint32_t>(pos);
}

template <>
inline parser_result<ValType> parse(const uint8_t* pos)
{
    const auto b = *pos++;
    switch (b)
    {
    case 0x7F:
        return {ValType::i32, pos};
    case 0x7E:
        return {ValType::i64, pos};
    case 0x7D:  // f32
    case 0x7C:  // f64
        throw parser_error{"unsupported valtype (floating point)"};
    default:
        throw parser_error{"invalid valtype " + std::to_string(b)};
    }
}

inline parser_result<Limits> parse_limits(const uint8_t* pos)
{
    Limits result;
    const auto b = *pos++;
    switch (b)
    {
    case 0x00:
        std::tie(result.min, pos) = leb128u_decode<uint32_t>(pos);
        return {result, pos};
    case 0x01:
        std::tie(result.min, pos) = leb128u_decode<uint32_t>(pos);
        std::tie(result.max, pos) = leb128u_decode<uint32_t>(pos);
        if (result.min > *result.max)
            throw parser_error("malformed limits (minimum is larger than maximum)");
        return {result, pos};
    default:
        throw parser_error{"invalid limits " + std::to_string(b)};
    }
}

template <typename T>
parser_result<std::vector<T>> parse_vec(const uint8_t* pos)
{
    uint32_t size;
    std::tie(size, pos) = leb128u_decode<uint32_t>(pos);

    std::vector<T> result;
    result.reserve(size);
    auto inserter = std::back_inserter(result);
    for (uint32_t i = 0; i < size; ++i)
        std::tie(inserter, pos) = parse<T>(pos);
    return {result, pos};
}
}  // namespace fizzy
