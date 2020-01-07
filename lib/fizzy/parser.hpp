#pragma once

#include "leb128.hpp"
#include "types.hpp"
#include <stdexcept>
#include <tuple>

namespace fizzy
{
constexpr uint8_t wasm_prefix_data[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
constexpr bytes_view wasm_prefix{wasm_prefix_data, sizeof(wasm_prefix_data)};

struct parser_error : public std::runtime_error
{
    using runtime_error::runtime_error;
};

template <typename T>
using parser_result = std::tuple<T, const uint8_t*>;

module parse(bytes_view input);
parser_result<code> parse_expr(const uint8_t* input);

template <typename T>
struct parser
{
};

template <>
struct parser<valtype>
{
    parser_result<valtype> operator()(const uint8_t* pos)
    {
        const auto b = *pos++;
        switch (b)
        {
        case 0x7F:
            return {valtype::i32, pos};
        case 0x7E:
            return {valtype::i64, pos};
        default:
            throw parser_error{"invalid valtype " + std::to_string(b)};
        }
    }
};

template <>
struct parser<locals>
{
    parser_result<locals> operator()(const uint8_t* pos)
    {
        locals result;
        std::tie(result.count, pos) = leb128u_decode<uint32_t>(pos);
        std::tie(result.type, pos) = parser<valtype>{}(pos);
        return {result, pos};
    }
};

template <typename T>
struct parser<std::vector<T>>
{
    parser_result<std::vector<T>> operator()(const uint8_t* pos)
    {
        uint32_t size;
        std::tie(size, pos) = leb128u_decode<uint32_t>(pos);

        std::vector<T> result;
        result.reserve(size);
        auto inserter = std::back_inserter(result);
        for (uint32_t i = 0; i < size; ++i)
            std::tie(inserter, pos) = parser<T>{}(pos);
        return {result, pos};
    }
};

template <>
struct parser<code>
{
    parser_result<code> operator()(const uint8_t* pos)
    {
        const auto [size, pos1] = leb128u_decode<uint32_t>(pos);

        const auto [locals_vec, pos2] = parser<std::vector<locals>>{}(pos1);

        auto result = parse_expr(pos2);

        for (const auto& l : locals_vec)
            std::get<0>(result).local_count += l.count;

        return result;
    }
};
}  // namespace fizzy
