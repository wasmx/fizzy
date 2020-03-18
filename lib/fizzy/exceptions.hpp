#pragma once

#include <stdexcept>

namespace fizzy
{
struct parser_error : public std::runtime_error
{
    using runtime_error::runtime_error;
};

struct instantiate_error : public std::runtime_error
{
    using runtime_error::runtime_error;
};

struct unsupported_feature : public std::runtime_error
{
    using runtime_error::runtime_error;
};

}  // namespace fizzy
