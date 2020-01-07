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

enum class instr : uint8_t
{
    unreachable = 0x00,
    nop = 0x01,
    end = 0x0b,
    local_get = 0x20,
    local_set = 0x21,
    local_tee = 0x22,
    i32_add = 0x6a,
};

struct code
{
    uint32_t local_count;

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