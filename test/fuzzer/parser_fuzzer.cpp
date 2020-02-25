#include "parser.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t data_size) noexcept
{
    (void)data;
    (void)data_size;
    return 0;
}
