#include <math.h>

#define WASM_EXPORT __attribute__((visibility("default")))

static unsigned factorial(unsigned n)
{
    unsigned ret = 1;
    for (unsigned i = n; i > 1; --i)
        ret *= i;
    return ret;
}

static double _ramanujan_pi(unsigned n)
{
    // This dumb implementation of Ramanujan series calculates 1/pi
    double ret = 0.0;
    for (unsigned k = 0; k < n; k++) {
        double a = factorial(4 * k) / pow(factorial(k), 4.0);
        double b = (26390.0 * k + 1103.0) / pow(396.0, 4 * k);
        ret += a * b;
    }

    // Some intentional f32 -> f64 conversion here
    ret *= (2.0 * sqrt(2.0)) / pow(99.0, 2.0);

    // Return pi
    return 1.0 / ret;
}

WASM_EXPORT unsigned long long ramanujan_pi(unsigned n)
{
    // Display all 16 digits of double precision as a 64-bit integer
    return _ramanujan_pi(n) * 10000000000000000ULL;
}
