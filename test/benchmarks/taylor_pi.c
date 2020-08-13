#define WASM_EXPORT __attribute__((visibility("default")))

// Pretty bad Pi approximation using Taylor series.
// It is set to single-precision to test single-precision instructions.
static inline float _taylor_pi(unsigned n)
{
    float sum = 1.0f;
    int sign = -1;

    for (unsigned i = 1; i < n; i++) {
       sum += sign / (2.0f * i + 1.0f);
       sign = -sign;
    }

    // This is the result in float precision
    sum = 4.0f * sum;

    return sum;
}

WASM_EXPORT unsigned long long taylor_pi(unsigned n)
{
    // Display all 16 digits of float precision as a 64-bit integer
    return _taylor_pi(n) * 10000000000000000ULL;
}
