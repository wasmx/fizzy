#include <inttypes.h>
#include <string.h>

#define WASM_EXPORT __attribute__((visibility("default")))

WASM_EXPORT unsigned memset_bench(unsigned fill, unsigned length)
{
    unsigned char input[length];
    memset(input, (uint8_t)fill, length);

    unsigned ret = 0;
    for (int i = 0; i < length; i++)
        ret += input[i];
    return ret;
}
