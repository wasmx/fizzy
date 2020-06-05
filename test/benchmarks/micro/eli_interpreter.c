/*
 * Example interpreter from Eli Bendersky's article "Computed goto for efficient dispatch tables".
 * https://eli.thegreenplace.net/2012/07/12/computed-goto-for-efficient-dispatch-tables
 *
 * clang -target wasm32 eli_interpreter.c -O3 --no-standard-libraries -Xlinker --no-entry -Xlinker --export-all -o eli_interpreter.wasm
 *
 * Inputs utilize a sequence of 8 instructions which increases the value by 5:
 * OP_INC, OP_MUL2, OP_ADD7, OP_NEG, OP_DEC, OP_DEC, OP_NEG, OP_DIV2
 * hex: 0103050602020604.
 */

#define OP_HALT     0x0
#define OP_INC      0x1
#define OP_DEC      0x2
#define OP_MUL2     0x3
#define OP_DIV2     0x4
#define OP_ADD7     0x5
#define OP_NEG      0x6


int interp_switch(unsigned char* code, int initval) {
    int pc = 0;
    int val = initval;

    while (1) {
        switch (code[pc++]) {
        case OP_HALT:
            return val;
        case OP_INC:
            val++;
            break;
        case OP_DEC:
            val--;
            break;
        case OP_MUL2:
            val *= 2;
            break;
        case OP_DIV2:
            val /= 2;
            break;
        case OP_ADD7:
            val += 7;
            break;
        case OP_NEG:
            val = -val;
            break;
        default:
            __builtin_trap();
        }
    }
}
