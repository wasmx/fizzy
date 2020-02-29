// Source: https://gist.github.com/poemm/356ba2c6826c6f5953db874e37783417

typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

#define NUM_LIMBS 8
#define LIMB_BITS 32



// algorithm 14.12, Handbook of Applied Cryptography, http://cacr.uwaterloo.ca/hac/about/chap14.pdf
// but assume they both have the same number of limbs, this can be changed
// out should have double the limbs of inputs
// num_limbs corresponds to n+1 in the book
void mul256(uint32_t* out, uint32_t* x, uint32_t* y){
  uint32_t* w = out;
  for (int i=0; i<2*NUM_LIMBS; i++)
    w[i]=0;
  for (int i=0; i<NUM_LIMBS; i++){
    uint32_t c = 0;
    for (int j=0; j<NUM_LIMBS; j++){
      uint64_t uv = (uint64_t)w[i+j] + (uint64_t)x[j]*y[i];
      uv += c;
      uint64_t u = uv >> LIMB_BITS;
      uint32_t v = uv;
      w[i+j] = v;
      c = u;
    }
    w[i+NUM_LIMBS] = c;
  }
}
