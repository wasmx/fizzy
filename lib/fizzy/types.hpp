#pragma once

#include <cstdint>
#include <string>
#include <vector>


namespace fizzy
{
using bytes = std::basic_string<uint8_t>;
using bytes_view = std::basic_string_view<uint8_t>;

// https://webassembly.github.io/spec/core/binary/types.html#binary-valtype
enum class valtype : uint8_t
{
    i32 = 0x7f,
    i64 = 0x7e,
};

// https://webassembly.github.io/spec/core/binary/types.html#binary-functype
struct functype
{
    std::vector<valtype> inputs;
    std::vector<valtype> outputs;
};

// https://webassembly.github.io/spec/core/binary/modules.html#binary-typeidx
using typeidx = uint32_t;

// https://webassembly.github.io/spec/core/binary/modules.html#binary-funcidx
using funcidx = uint32_t;

/// Function locals.
/// https://webassembly.github.io/spec/core/binary/modules.html#binary-local
struct locals
{
    uint32_t count;
    valtype type;
};

enum class instr : uint8_t
{
    unreachable = 0x00,
    nop = 0x01,
    end = 0x0b,
    local_get = 0x20,
    local_set = 0x21,
    local_tee = 0x22,
    i32_add = 0x6a,
    i32_sub = 0x6b,
    i32_mul = 0x6c,
    i32_div_s = 0x6d,
    i32_div_u = 0x6e,
    i32_rem_s = 0x6f,
    i32_rem_u = 0x70,
    i32_and = 0x71,
    i32_or = 0x72,
    i32_xor = 0x73,
    i32_shl = 0x74,
    i32_shr_s = 0x75,
    i32_shr_u = 0x76,
    i32_rotl = 0x77,
    i32_rotr = 0x78,
};

struct code
{
    uint32_t local_count = 0;

    // The instructions bytecode without immediate values.
    // https://webassembly.github.io/spec/core/binary/instructions.html
    std::vector<instr> instructions;

    // The decoded instructions' immediate values.
    // These are instruction-type dependent fixed size value in the order of instructions.
    bytes immediates;
};

enum class sectionid : uint8_t
{
    custom = 0,
    type = 1,
    import = 2,
    function = 3,
    table = 4,
    memory = 5,
    global = 6,
    export_ = 7,
    start = 8,
    element = 9,
    code = 10,
    data = 11
};

struct module
{
    // https://webassembly.github.io/spec/core/binary/modules.html#type-section
    std::vector<functype> typesec;
    // https://webassembly.github.io/spec/core/binary/modules.html#function-section
    std::vector<typeidx> funcsec;
    // https://webassembly.github.io/spec/core/binary/modules.html#code-section
    std::vector<code> codesec;
};

}  // namespace fizzy