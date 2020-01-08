#pragma once

#include <cstdint>
#include <string>
#include <vector>


namespace fizzy
{
using bytes = std::basic_string<uint8_t>;
using bytes_view = std::basic_string_view<uint8_t>;

// https://webassembly.github.io/spec/core/binary/types.html#binary-valtype
enum class ValType : uint8_t
{
    i32 = 0x7f,
    i64 = 0x7e,
};

// https://webassembly.github.io/spec/core/binary/types.html#binary-functype
struct FuncType
{
    std::vector<ValType> inputs;
    std::vector<ValType> outputs;
};

// https://webassembly.github.io/spec/core/binary/modules.html#binary-typeidx
using TypeIdx = uint32_t;

// https://webassembly.github.io/spec/core/binary/modules.html#binary-funcidx
using FuncIdx = uint32_t;

/// Function locals.
/// https://webassembly.github.io/spec/core/binary/modules.html#binary-local
struct Locals
{
    uint32_t count;
    ValType type;
};

enum class Instr : uint8_t
{
    unreachable = 0x00,
    nop = 0x01,
    end = 0x0b,
    drop = 0x1a,
    select = 0x1b,
    local_get = 0x20,
    local_set = 0x21,
    local_tee = 0x22,
    i32_const = 0x41,
    i64_const = 0x42,
    i32_eqz = 0x45,
    i32_eq = 0x46,
    i32_ne = 0x47,
    i32_clz = 0x67,
    i32_ctz = 0x68,
    i32_popcnt = 0x69,
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
    i64_clz = 0x79,
    i64_ctz = 0x7a,
    i64_popcnt = 0x7b,
    i64_add = 0x7c,
    i64_sub = 0x7d,
    i64_mul = 0x7e,
    i64_div_s = 0x7f,
    i64_div_u = 0x80,
    i64_rem_s = 0x81,
    i64_rem_u = 0x82,
    i64_and = 0x83,
    i64_or = 0x84,
    i64_xor = 0x85,
    i64_shl = 0x86,
    i64_shr_s = 0x87,
    i64_shr_u = 0x88,
    i64_rotl = 0x89,
    i64_rotr = 0x8a,
    i32_wrap_i64 = 0xa7,
    i64_extend_i32_s = 0xac,
    i64_extend_i32_u = 0xad,
};

struct Code
{
    uint32_t local_count = 0;

    // The instructions bytecode without immediate values.
    // https://webassembly.github.io/spec/core/binary/instructions.html
    std::vector<Instr> instructions;

    // The decoded instructions' immediate values.
    // These are instruction-type dependent fixed size value in the order of instructions.
    bytes immediates;
};

enum class SectionId : uint8_t
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

struct Module
{
    // https://webassembly.github.io/spec/core/binary/modules.html#type-section
    std::vector<FuncType> typesec;
    // https://webassembly.github.io/spec/core/binary/modules.html#function-section
    std::vector<TypeIdx> funcsec;
    // https://webassembly.github.io/spec/core/binary/modules.html#code-section
    std::vector<Code> codesec;
};

}  // namespace fizzy
