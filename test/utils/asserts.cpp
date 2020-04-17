#include "asserts.hpp"
#include <ostream>

namespace fizzy
{
std::ostream& operator<<(std::ostream& os, execution_result result)
{
    if (result.trapped)
        return os << "trapped";

    os << "result(";
    std::string_view separator;
    for (const auto& x : result.stack)
    {
        os << x << separator;
        separator = ", ";
    }
    os << ")";
    return os;
}
}  // namespace fizzy