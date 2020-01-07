#pragma once

#include "leb128.hpp"
#include "types.hpp"
#include <stdexcept>
#include <tuple>

namespace fizzy
{
struct parser_error : public std::runtime_error
{
    using runtime_error::runtime_error;
};

template <typename T>
using parser_result = std::tuple<T, const uint8_t*>;

module parse(bytes_view bin);

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
}  // namespace fizzy
