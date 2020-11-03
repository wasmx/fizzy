#include <stdint.h>

#define WASM_EXPORT __attribute__((visibility("default")))

static uint64_t fnv1(uint64_t state, uint64_t input)
{
    return (state ^ input) * 0x100000001b3;
}

static uint64_t threeab(uint64_t state, uint64_t input)
{
    return (3 * state) + input;
}

typedef uint64_t (*hashfn)(uint64_t state, uint64_t input);

static hashfn fns[] = {fnv1, threeab};

WASM_EXPORT unsigned icall(unsigned steps)
{
    uint64_t input = 0x1234567890abcdef;

    uint64_t state = 0xcbf29ce484222325;
    for (unsigned i = 0; i < steps; i++)
        state = fns[i % 2](state, input);
    return state;
}
