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

Module parse(bytes_view input);
parser_result<Code> parse_expr(const uint8_t* input);

template <typename T>
struct parser
{
};

template <>
struct parser<uint8_t>
{
    parser_result<uint8_t> operator()(const uint8_t* pos)
    {
        const auto result = *pos;
        ++pos;
        return {result, pos};
    }
};

template <>
struct parser<uint32_t>
{
    parser_result<uint32_t> operator()(const uint8_t* pos) { return leb128u_decode<uint32_t>(pos); }
};

template <>
struct parser<ValType>
{
    parser_result<ValType> operator()(const uint8_t* pos)
    {
        const auto b = *pos++;
        switch (b)
        {
        case 0x7F:
            return {ValType::i32, pos};
        case 0x7E:
            return {ValType::i64, pos};
        default:
            throw parser_error{"invalid valtype " + std::to_string(b)};
        }
    }
};

template <>
struct parser<Limits>
{
    parser_result<Limits> operator()(const uint8_t* pos)
    {
        Limits result;
        const auto b = *pos++;
        switch (b)
        {
        case 0x00:
            std::tie(result.min, pos) = leb128u_decode<uint32_t>(pos);
            result.max = std::numeric_limits<uint32_t>::max();
            return {result, pos};
        case 0x01:
            std::tie(result.min, pos) = leb128u_decode<uint32_t>(pos);
            std::tie(result.max, pos) = leb128u_decode<uint32_t>(pos);
            return {result, pos};
        default:
            throw parser_error{"invalid limits " + std::to_string(b)};
        }
    }
};

template <>
struct parser<Locals>
{
    parser_result<Locals> operator()(const uint8_t* pos)
    {
        Locals result;
        std::tie(result.count, pos) = leb128u_decode<uint32_t>(pos);
        std::tie(result.type, pos) = parser<ValType>{}(pos);
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
struct parser<Memory>
{
    parser_result<Memory> operator()(const uint8_t* pos)
    {
        Limits limits;
        std::tie(limits, pos) = parser<Limits>{}(pos);
        return {{limits}, pos};
    }
};

template <>
struct parser<Code>
{
    parser_result<Code> operator()(const uint8_t* pos)
    {
        const auto [size, pos1] = leb128u_decode<uint32_t>(pos);

        const auto [locals_vec, pos2] = parser<std::vector<Locals>>{}(pos1);

        auto result = parse_expr(pos2);

        for (const auto& l : locals_vec)
            std::get<0>(result).local_count += l.count;

        return result;
    }
};
}  // namespace fizzy
