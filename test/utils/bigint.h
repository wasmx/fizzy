#ifndef BIGINT_H
#define BIGINT_H

#if !WASM
#include <stdint.h>
#else
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef unsigned __int128 uint128_t;
#endif

#define uint128_t __uint128_t

/*
This header-only library has three parameters:
BIGINT_BITS - the number of bits of the big integer
LIMB_BITS - the number of bits in each limb, must correspond to a uint*_t type
LIMB_BITS_OVERFLOW - the number of bits output by multiplication, i.e. 2*LIMB_BITS, must correspond
to a uint*_t type

To use this library, define a limb size and include it:
  #define BIGINT_BITS 256
  #define LIMB_BITS 64
  #define LIMB_BITS_OVERFLOW 128
  #include "bigint.h"
  #undef BIGINT_BITS
  #undef LIMB_BITS
  #undef LIMB_BITS_OVERFLOW

And if you need other sizes, define them:
  #define BIGINT_BITS 512
  #define LIMB_BITS 32
  #define LIMB_BITS_OVERFLOW 64
  #include "bigint.h"
  #undef BIGINT_BITS
  #undef LIMB_BITS
  #undef LIMB_BITS_OVERFLOW

Now you can use functions like:
  mulmodmont256_64bitlimbs(out,x,y,m,inv);
  sub512_32bitlimbs(out,a,b);

Warning: LIMB_BITS corresponds to the uint*_t type, and multiplication requires double the bits, for
example 64-bit limbs require type uint128_t, which may be unavailable on some targets like Wasm.
*/


// Define macros used in this file:

// define types UINT and UNT2, where UINT2 is for overflow of operations on UINT; for multiplication
// should be double the number of bits UINT is the limb type, uint*_t where * is the number of bits
// per limb, eg uint32_t UINT2 is also needed for multiplication UINTxUINT->UINT2, e.g.
// uint32_txuint32_t->uint64_t or uint64_txuint64_t->uint128_t
#define TYPE_(num) uint##num##_t
#define TYPE(num) TYPE_(num)
#define UINT TYPE(LIMB_BITS)
#define UINT2 TYPE(LIMB_BITS_OVERFLOW)

// define NUM_LIMBS to be the number of limbs
// eg UINT=uint32_t with NUM_LIMBS=8 limbs is for 256-bit
// eg UINT=uint64_t with NUM_LIMBS=8 limbs is for 512-bit
#define NUM_LIMBS (BIGINT_BITS / LIMB_BITS)

// define the function name, use concatenation
// eg BIGINT_BITS=256, LIMB_BITS=64: FUNCNAME(myname) is replaced with myname256_64bitlimbs
#define FUNCNAME__(name, A, B) name##A##_##B##bitlimbs
#define FUNCNAME_(name, A, B) FUNCNAME__(name, A, B)
#define FUNCNAME(name) FUNCNAME_(name, BIGINT_BITS, LIMB_BITS)


// add two numbers using two's complement for overflow, returning an overflow bit
// algorithm 14.7, Handbook of Applied Cryptography, http://cacr.uwaterloo.ca/hac/about/chap14.pdf
//   except we ignore the final carry in step 3 since we assume that there is no extra limb
UINT FUNCNAME(add)(UINT* const out, const UINT* const x, const UINT* const y)
{
    UINT c = 0;
#pragma unroll
    for (int i = 0; i < NUM_LIMBS; i++)
    {
        UINT temp = x[i] + c;
        out[i] = temp + y[i];
        c = (temp < c || out[i] < temp) ? 1 : 0;
    }
    return c;
}

// algorithm 14.9, Handbook of Applied Cryptography, http://cacr.uwaterloo.ca/hac/about/chap14.pdf
// the book says it computes x-y for x>=y, but actually it computes the 2's complement for x<y
// note: algorithm 14.9 allow adding c=-1, but we just subtract c=1 instead
UINT FUNCNAME(sub)(UINT* const out, const UINT* const x, const UINT* const y)
{
    UINT c = 0;
#pragma unroll
    for (int i = 0; i < NUM_LIMBS; i++)
    {
        UINT temp = x[i] - c;
        c = (temp < y[i] || x[i] < c) ? 1 : 0;
        out[i] = temp - y[i];
    }
    return c;
}


// checks whether x<y
uint8_t FUNCNAME(less_than)(const UINT* const x, const UINT* const y)
{
    for (int i = NUM_LIMBS - 1; i >= 0; i--)
    {
        if (x[i] > y[i])
            return 0;
        else if (x[i] < y[i])
            return 1;
    }
    // they are equal
    return 0;
}

// checks whether x<=y
uint8_t FUNCNAME(less_than_or_equal)(const UINT* const x, const UINT* const y)
{
    for (int i = NUM_LIMBS - 1; i >= 0; i--)
    {
        if (x[i] > y[i])
            return 0;
        else if (x[i] < y[i])
            return 1;
    }
    // they are equal
    return 1;
}


// computes quotient x/y and remainder x%y
// algorithm 14.20, Handbook of Applied Cryptography, http://cacr.uwaterloo.ca/hac/about/chap14.pdf
// it works, but the implementation is naive, see notes
// y = q*x + r
void FUNCNAME(div)(UINT* const q, UINT* const r, const UINT* const x, const UINT* const y)
{
    for (int i = 0; i < NUM_LIMBS; i++)
    {
        q[i] = 0;
    }

    // book has n and t given, we compute these
    int n = 0;  // idx of first significant("nonzero") limbs of x
    int t = 0;  // n minus the idx of first significant limb of y
    for (int i = NUM_LIMBS - 1; i >= 0; i--)
    {
        if (x[i] != 0)
        {
            n = i;
            break;
        }
    }
    for (int i = n; i >= 0; i--)
    {
        if (y[i] != 0)
        {
            t = n - i;
            break;
        }
    }

    // not in the textbook
    // special case for y=1, this hack is needed for now
    if (n - t == 0 && y[0] == 1)
    {
        for (int i = 0; i < NUM_LIMBS; i++)
        {
            r[i] = 0;
            q[i] = x[i];
        }
        return;
    }

    // save input x from getting clobbered below
    // note that x_ it will end up as remainder
    UINT* x_ = r;
    for (int i = 0; i < NUM_LIMBS; i++)
    {
        x_[i] = x[i];
    }

    /* WIP
    // step 1 in book
    for (int j=0;j<n-t;j++)
      q[i]=0;

    // step 2 in book
    // first get y*b^{n-t} by shifting y up by n-t limbs
    UINT y_n_t[NUM_LIMBS];
    for (int i=NUM_LIMBS;i>t;i--)
      y_n_t[i] = 0;
    for (int i=t;i>n-t;i--)
      y_n_t[i] = y[i+n-t];
    for (int i=n-t;i>=0;i--)
      y_n_t[i] = 0;
    // now the while subtract loop
    while (FUNCNAME(less_than_or_equal)(y_n_t,x)){
      q[n-t]+=1;
      FUNCNAME(sub(x_,x,y_n_t));
    }

    // step 3 in book
    // TODO
    */


    // THIS IS A NAIVE IMPLEMENTATION OF WHAT IS IN THE BOOK
    // naive loop: while( y<x_ ) { q++; x_=x_-y }

    // leq = (y<x_)
    UINT leq = 1;
    for (int i = n; i >= 0; i--)
    {
        if (y[i] > x_[i])
        {
            leq = 0;
            break;
        }
        else if (y[i] < x_[i])
        {
            leq = 1;
            break;
        }
    }

    while (leq)
    {
        // q = q + 1
        for (int i = 0; i <= n; i++)
        {
            q[i] += 1;
            if (q[i] != 0)
                break;
        }

        // x_ = x_ - y
        UINT c = 0;
        for (int i = 0; i <= n; i++)
        {
            UINT temp = x_[i] - c;
            c = (temp < y[i] || x_[i] < c) ? 1 : 0;
            x_[i] = temp - y[i];
        }

        // leq = (y<x_)
        for (int i = n; i >= 0; i--)
        {
            if (y[i] > x_[i])
            {
                leq = 0;
                break;
            }
            else if (y[i] < x_[i])
            {
                leq = 1;
                break;
            }
        }
    }
}

// algorithm 14.12, Handbook of Applied Cryptography, http://cacr.uwaterloo.ca/hac/about/chap14.pdf
// but assume they both have the same number of limbs, this can be changed
// out should have double the number of limbs as the inputs
// num_limbs corresponds to n+1 in the book
void FUNCNAME(mul)(UINT* const out, const UINT* const x, const UINT* const y)
{
    UINT w[NUM_LIMBS * 2];
    for (int i = 0; i < 2 * NUM_LIMBS; i++)
        w[i] = 0;
#pragma unroll
    for (int i = 0; i < NUM_LIMBS; i++)
    {
        UINT c = 0;
#pragma unroll
        for (int j = 0; j < NUM_LIMBS; j++)
        {
            UINT2 uv = (UINT2)w[i + j] + (UINT2)x[j] * y[i];
            uv += c;
            UINT2 u = uv >> LIMB_BITS;
            UINT v = (UINT)uv;
            w[i + j] = v;
            c = (UINT)u;
        }
        w[i + NUM_LIMBS] = c;
    }
    for (int i = 0; i < 2 * NUM_LIMBS; i++)
        out[i] = w[i];
}

// algorithm 14.16, Handbook of Applied Cryptography, http://cacr.uwaterloo.ca/hac/about/chap14.pdf
// NUM_LIMBS is t (number of limbs) in the book, and the base is UINT*, usually uint32_t or uint64_t
// output out should have double the limbs of input x
void FUNCNAME(square)(UINT* const out, const UINT* const x)
{
    UINT w[NUM_LIMBS * 2];
    for (int i = 0; i < 2 * NUM_LIMBS; i++)
        w[i] = 0;
    for (int i = 0; i < NUM_LIMBS; i++)
    {
        UINT2 uv = (UINT2)(x[i]) * x[i] + w[2 * i];
        UINT u = (UINT)(uv >> LIMB_BITS);  // / base
        UINT v = (UINT)uv;                 // % base
        w[2 * i] = v;
        UINT c = u;
        for (int j = i + 1; j < NUM_LIMBS; j++)
        {
            UINT2 xixj = (UINT2)(x[i]) * x[j];
            UINT2 partial_sum = xixj + c + w[i + j];
            uv = xixj + partial_sum;
            u = (UINT)(uv >> LIMB_BITS);  // / base
            v = (UINT)uv;                 // % base
            w[i + j] = v;
            c = u;
            // may have more overflow, so keep carrying
            // this passes sume tests, but needs review
            if (uv < partial_sum)
            {
                int k = 2;
                while (i + j + k < NUM_LIMBS * 2 && w[i + j + k] == (UINT)0 - 1)
                {  // note 0-1 is 0xffffffff
                    w[i + j + k] = 0;
                    k++;
                }
                if (i + j + k < NUM_LIMBS * 2)
                    w[i + j + k] += 1;
            }
        }
        // this passes some tests, but not sure if += is correct
        w[i + NUM_LIMBS] += u;
    }
    for (int i = 0; i < 2 * NUM_LIMBS; i++)
        out[i] = w[i];
}


////////////////////////
// Modular arithmetic //
////////////////////////


// compute a+b (mod m), where x,y < m
// algorithm 14.27, Handbook of Applied Cryptography, http://cacr.uwaterloo.ca/hac/about/chap14.pdf
void FUNCNAME(addmod)(
    UINT* const out, const UINT* const x, const UINT* const y, const UINT* const m)
{
    UINT carry = FUNCNAME(add)(out, x, y);
    // In textbook 14.27, says addmod is add and an extra step: subtract m iff x+y>=m
    if (carry || FUNCNAME(less_than_or_equal)(m, out))
    {
        FUNCNAME(sub)(out, out, m);
    }
    // note: we don't consider the case x+y-m>m. Because, for our crypto application, we assume
    // x,y<m.
}

// compute x-y (mod m) for x,y < m
// uses fact 14.27, Handbook of Applied Cryptography, http://cacr.uwaterloo.ca/hac/about/chap14.pdf
void FUNCNAME(submod)(
    UINT* const out, const UINT* const x, const UINT* const y, const UINT* const m)
{
    UINT c = FUNCNAME(sub)(out, x, y);
    // if c, then x<y, so result is negative, need to get it's magnitude and subtract it from m
    if (c)
    {
        FUNCNAME(add)(out, m, out);  // add m to overflow back
    }
    // note: we don't consider the case x-y>m. Because, for our crypto application, we assume x,y<m.
}


// returns (aR * bR) % m, where aR and bR are in Montgomery form
// algorithm 14.32, Handbook of Applied Cryptography, http://cacr.uwaterloo.ca/hac/about/chap14.pdf
// T has 2*NUM_LIMBS limbs, otherwise pad most significant bits with zeros
void FUNCNAME(montreduce)(UINT* const out, const UINT* const T, const UINT* const m, const UINT inv)
{
    UINT A[NUM_LIMBS * 2 + 1];
    for (int i = 0; i < 2 * NUM_LIMBS; i++)
        A[i] = T[i];
    A[NUM_LIMBS * 2] = 0;
    for (int i = 0; i < NUM_LIMBS; i++)
    {
        UINT ui = A[i] * inv;
        UINT carry = 0;
        int j;
        // add ui*m*b^i to A in a loop, since m is NUM_LIMBS long
        for (j = 0; j < NUM_LIMBS; j++)
        {
            UINT2 sum = (UINT2)ui * m[j] + A[i + j] + carry;
            A[i + j] = (UINT)sum;              // % b;
            carry = (UINT)(sum >> LIMB_BITS);  // / b
        }
        // carry may be nonzero, so keep carrying
        int k = 0;
        while (carry && i + j + k < 2 * NUM_LIMBS + 1)
        {
            UINT2 sum = (UINT2)(A[i + j + k]) + carry;
            A[i + j + k] = (UINT)sum;          // % b
            carry = (UINT)(sum >> LIMB_BITS);  // / b
            k += 1;
        }
    }

    // instead of right shift, we just get the correct values
    for (int i = 0; i < NUM_LIMBS; i++)
        out[i] = A[i + NUM_LIMBS];

    // final subtraction, first see if necessary
    if (A[NUM_LIMBS * 2] || FUNCNAME(less_than_or_equal)(m, out))
    {
        FUNCNAME(sub)(out, out, m);
    }
}

// algorithm 14.16 followed by 14.32
// this might be faster than algorithm 14.36, as described in remark 14.40
void FUNCNAME(montsquare)(UINT* const out, const UINT* const x, const UINT* const m, const UINT inv)
{
    UINT out_internal[NUM_LIMBS * 2];
    FUNCNAME(square)(out_internal, x);
    FUNCNAME(montreduce)(out, out_internal, m, inv);
}

// algorithm 14.12 followed by 14.32
// this might be slower than algorithm 14.36, which interleaves these steps
// Known as the Separated Operand Scanning (SOS) Method
void FUNCNAME(mulmodmontSOS)(
    UINT* const out, const UINT* const x, const UINT* const y, const UINT* const m, const UINT inv)
{
    UINT out_internal[NUM_LIMBS * 2];
    FUNCNAME(mul)(out_internal, x, y);
    FUNCNAME(montreduce)(out, out_internal, m, inv);
}

// algorithm 14.36, Handbook of Applied Cryptography, http://cacr.uwaterloo.ca/hac/about/chap14.pdf
void FUNCNAME(mulmodmontHAC)(
    UINT* const out, const UINT* const x, const UINT* const y, const UINT* const m, const UINT inv)
{
    UINT A[NUM_LIMBS * 2 + 1];
    for (int i = 0; i < NUM_LIMBS * 2 + 1; i++)
        A[i] = 0;
#pragma unroll  // this unroll increases binary size by a lot
    for (int i = 0; i < NUM_LIMBS; i++)
    {
        UINT ui = (A[i] + x[i] * y[0]) * inv;
        UINT carry = 0;
#pragma unroll
        for (int j = 0; j < NUM_LIMBS; j++)
        {
            UINT2 xiyj = (UINT2)x[i] * y[j];
            UINT2 uimj = (UINT2)ui * m[j];
            UINT2 partial_sum = xiyj + carry;
            UINT2 sum = uimj + A[i + j] + partial_sum;
            A[i + j] = (UINT)sum;
            carry = (UINT)(sum >> LIMB_BITS);
            // if there was overflow in the sum beyond the carry:
            if (sum < partial_sum)
            {
                int k = 2;
                while (i + j + k < NUM_LIMBS * 2 && A[i + j + k] == (UINT)0 - 1)
                {  // note 0-1 is 0xffffffff
                    // this is rare, need limb to be all 1's
                    A[i + j + k] = 0;
                    k++;
                }
                if (i + j + k < NUM_LIMBS * 2 + 1)
                {
                    A[i + j + k] += 1;
                }
            }
            // printf("%d %d %llu %llu %llu %llu %llu %llu %llu %llu
            // %llu\n",i,j,x[i],x[i]*y[0],ui,xiyj,uimj,partial_sum,sum,A[i+j],carry);
        }
        A[i + NUM_LIMBS] += carry;
    }

    // instead of right shift, we just get the correct values
    for (int i = 0; i < NUM_LIMBS; i++)
        out[i] = A[i + NUM_LIMBS];

    // final subtraction, first see if necessary
    if (A[NUM_LIMBS * 2] > 0 || FUNCNAME(less_than_or_equal)(m, out))
        FUNCNAME(sub)(out, out, m);
}

// From paper Çetin K. Koç; Tolga Acar; Burton S. Kaliski, Jr. (June 1996). "Analyzing and Comparing
// Montgomery Multiplication Algorithms". IEEE Micro. 16 (3): 26–33.
void FUNCNAME(mulmodmontFIOS)(UINT* const out, const UINT* const a, const UINT* const b,
    const UINT* const mod, const UINT inv)
{
    UINT t[NUM_LIMBS + 2];
    for (int i = 0; i < NUM_LIMBS + 2; i++)
        t[i] = 0;
#pragma unroll  // this unroll increases binary size by a lot
    for (int i = 0; i < NUM_LIMBS; i++)
    {
        UINT carry = 0;
        UINT2 sum = 0;
        sum = (UINT2)(t[0]) + (UINT2)(a[0]) * b[i];
        carry = (UINT)(sum >> LIMB_BITS);
        int k = 1;
        while (carry && k <= NUM_LIMBS + 1)
        {
            UINT2 temp = (UINT2)t[k] + carry;
            t[k] = (UINT)temp;
            carry = (UINT)(temp >> LIMB_BITS);
            k++;
        }
        UINT m = ((UINT)sum) * inv;
        sum = (UINT)sum + (UINT2)m * mod[0];  // lower limb of sum should be zero
        carry = (UINT)(sum >> LIMB_BITS);
#pragma unroll
        for (int j = 1; j < NUM_LIMBS; j++)
        {
            sum = (UINT2)t[j] + (UINT2)a[j] * b[i] + carry;
            carry = (UINT)(sum >> LIMB_BITS);
            k = j + 1;
            while (carry && k <= NUM_LIMBS + 1)
            {
                UINT2 temp = (UINT2)t[k] + carry;
                t[k] = (UINT)temp;
                carry = (UINT)(temp >> LIMB_BITS);
                k++;
            }
            sum = (UINT)sum + (UINT2)m * mod[j];
            carry = (UINT)(sum >> LIMB_BITS);
            t[j - 1] = (UINT)sum;
        }
        sum = (UINT2)t[NUM_LIMBS] + carry;
        carry = (UINT)(sum >> LIMB_BITS);
        t[NUM_LIMBS - 1] = (UINT)sum;
        t[NUM_LIMBS] = t[NUM_LIMBS + 1] + carry;
        t[NUM_LIMBS + 1] = 0;
    }

    // output correct values
    for (int i = 0; i < NUM_LIMBS; i++)
        out[i] = t[i];

    // final subtraction, first see if necessary
    if (t[NUM_LIMBS] > 0 || FUNCNAME(less_than_or_equal)(mod, out))
        FUNCNAME(sub)(out, out, mod);
}

// see description for mulmodmontCIOS
void FUNCNAME(mulmodmont)(
    UINT* const out, const UINT* const x, const UINT* const y, const UINT* const m, const UINT inv)
{
    UINT A[NUM_LIMBS + 2];
    for (int i = 0; i < NUM_LIMBS + 2; i++)
        A[i] = 0;
#pragma unroll  // this unroll increases binary size by a lot
    for (int i = 0; i < NUM_LIMBS; i++)
    {
        UINT carry = 0;
        UINT2 sum = 0;
#pragma unroll
        for (int j = 0; j < NUM_LIMBS; j++)
        {
            sum = (UINT2)A[j] + (UINT2)x[i] * y[j] + carry;
            carry = (UINT)(sum >> LIMB_BITS);
            A[j] = (UINT)sum;
        }
        sum = (UINT2)(A[NUM_LIMBS]) + carry;
        carry = (UINT)(sum >> LIMB_BITS);
        A[NUM_LIMBS] = (UINT)sum;
        A[NUM_LIMBS + 1] = carry;
        UINT A0inv = A[0] * inv;
        sum = (UINT2)(A[0]) + (UINT2)A0inv * m[0];
        carry = (UINT)(sum >> LIMB_BITS);
#pragma unroll
        for (int j = 1; j < NUM_LIMBS; j++)
        {
            sum = (UINT2)(A[j]) + (UINT2)A0inv * m[j] + carry;
            carry = (UINT)(sum >> LIMB_BITS);
            A[j - 1] = (UINT)sum;
        }
        sum = (UINT2)(A[NUM_LIMBS]) + carry;
        carry = (UINT)(sum >> LIMB_BITS);
        A[NUM_LIMBS - 1] = (UINT)sum;
        A[NUM_LIMBS] = A[NUM_LIMBS + 1] + carry;
    }

    // copy to out
    for (int i = 0; i < NUM_LIMBS; i++)
        out[i] = A[i];

    // final subtraction, first see if necessary
    if (A[NUM_LIMBS] > 0 || FUNCNAME(less_than_or_equal)(m, out))
        FUNCNAME(sub)(out, out, m);
}

// Uses CIOS method for montgomery multiplication, based on algorithm from (but using notation of
// above mulmodmont) Çetin K. Koç; Tolga Acar; Burton S. Kaliski, Jr. (June 1996). "Analyzing and
// Comparing Montgomery Multiplication Algorithms". IEEE Micro. 16 (3): 26–33. Known as the Coarsely
// Integrated Operand Scanning (CIOS)
void FUNCNAME(mulmodmontCIOS)(
    UINT* const out, const UINT* const x, const UINT* const y, const UINT* const m, const UINT inv)
{
    FUNCNAME(mulmodmont)(out, x, y, m, inv);
}

// like mulmodmont, but with two of the args hard-coded
void FUNCNAME(mulmodmont_3args_)(UINT* const out, const UINT* const x, const UINT* const y)
{
    UINT* m = (UINT*)4444444;  // hard-code m or address to m here
    UINT inv = 6666666;        // hard-code inv here
    FUNCNAME(mulmodmont)(out, x, y, m, inv);
}

#endif
