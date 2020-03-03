#include "parser.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t data_size) noexcept
{
    try
    {
        fizzy::parse({data, data_size});
    }
    catch (const fizzy::parser_error&)
    {
    }
    return 0;
}
