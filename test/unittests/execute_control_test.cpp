#include "execute.hpp"
#include "hex.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>

using namespace fizzy;

TEST(execute_control, unreachable)
{
    Module module;
    module.codesec.emplace_back(Code{0, {Instr::unreachable, Instr::end}, {}});

    const auto [trap, ret] = execute(module, 0, {});

    ASSERT_TRUE(trap);
}

TEST(execute_control, nop)
{
    Module module;
    module.codesec.emplace_back(Code{0, {Instr::nop, Instr::end}, {}});

    const auto [trap, ret] = execute(module, 0, {});

    ASSERT_FALSE(trap);
    EXPECT_EQ(ret.size(), 0);
}

TEST(execute_control, block_br)
{
    // (local i32 i32)
    // block
    //   i32.const 0xa
    //   local.set 1
    //   br 0
    //   i32.const 0xb
    //   local.set 1
    // end
    // local.get 1

    Module module;
    module.codesec.emplace_back(Code{2,
        {Instr::block, Instr::i32_const, Instr::local_set, Instr::br, Instr::i32_const,
            Instr::local_set, Instr::end, Instr::local_get, Instr::end},
        from_hex("00"       /* arity */
                 "07000000" /* target_pc: 7 */
                 "1d000000" /* target_imm: 29 */
                 "0a000000" /* i32.const 0xa */
                 "01000000" /* local.set 1 */
                 "00000000" /* br 0 */
                 "0b000000" /* i32.const 0xb */
                 "01000000" /* local.set 1 */
                 "01000000" /* local.get 1 */
            )});

    const auto [trap, ret] = execute(module, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xa);
}

TEST(execute_control, loop_void_empty)
{
    // This wasm code is invalid - loop is not allowed to leave anything on the stack.
    Module module;
    module.codesec.emplace_back(
        Code{0, {Instr::loop, Instr::local_get, Instr::end, Instr::end}, {0, 0, 0, 0, 0}});

    const auto [trap, ret] = execute(module, 0, {1});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 1);
}

TEST(execute_control, block_void_empty)
{
    // This wasm code is invalid - block with type [] is not allowed to leave anything on the stack.
    Module module;
    module.codesec.emplace_back(Code{0, {Instr::block, Instr::local_get, Instr::end, Instr::end},
        from_hex("00"
                 "00000000"
                 "00000000"
                 "00000000")});

    const auto [trap, ret] = execute(module, 0, {100});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 100);
}

TEST(execute_control, loop_void_br_if_16)
{
    // This will try to loop 16 times.
    //
    // loop
    //   local.get 0 # This is the input argument.
    //   i32.const 1
    //   i32.sub
    //   local.tee 0
    //   br_if 0
    //   local.get 0
    // end
    Module module;
    module.codesec.emplace_back(Code{0,
        {Instr::loop, Instr::local_get, Instr::i32_const, Instr::i32_sub, Instr::local_tee,
            Instr::br_if, Instr::local_get, Instr::end, Instr::end},
        {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    const auto [trap, ret] = execute(module, 0, {16});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0);
}

TEST(execute_control, blocks_without_br)
{
    /*
    (func (result i32)
      (local i32)
      (block
        get_local 0
        i32.const 1
        i32.add
        set_local 0
        (loop
          get_local 0
          i32.const 1
          i32.add
          set_local 0
          (block
            get_local 0
            i32.const 1
            i32.add
            set_local 0
            (loop
              get_local 0
              i32.const 1
              i32.add
              set_local 0
            )
          )
        )
      )
      get_local 0
    )
    */
    const auto bin = from_hex(
        "0061736d010000000105016000017f030201000a30012e01017f0240200041016a21000340200041016a210002"
        "40200041016a21000340200041016a21000b0b0b0b20000b000c046e616d6502050100010000");

    const auto [trap, ret] = execute(parse(bin), 0, {});
    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 4);
}

TEST(execute_control, nested_blocks)
{
    /*
    (func (result i32)
      (local i32)
      (block
        get_local 0
        i32.const 1
        i32.or
        set_local 0
        (block
          get_local 0
          i32.const 2
          i32.or
          set_local 0
          (block
            get_local 0
            i32.const 4
            i32.or
            set_local 0
            br <<br_imm>>
            get_local 0
            i32.const 8
            i32.or
            set_local 0)
          get_local 0
          i32.const 16
          i32.or
          set_local 0)
        get_local 0
        i32.const 32
        i32.or
        set_local 0)
      get_local 0
      i32.const 64
      i32.or)
    */

    auto bin = from_hex(
        "0061736d010000000105016000017f030201000a41013f01017f"
        "0240200041017221000240200041027221000240200041047221000c"
        "77"  // <-- br's immediate value.
        "200041087221000b200041107221000b200041207221000b200041c000720b000c046e"
        "616d6502050100010000");
    constexpr auto br_imm_offset = 54;

    constexpr uint32_t expected_results[]{
        0b1110111,
        0b1100111,
        0b1000111,
    };

    // TODO: Value 3 can be added if implicit block for functions is implemented.
    for (auto br_imm : {0, 1, 2})
    {
        bin[br_imm_offset] = static_cast<uint8_t>(br_imm);

        const auto [trap, ret] = execute(parse(bin), 0, {});
        ASSERT_FALSE(trap);
        ASSERT_EQ(ret.size(), 1);
        EXPECT_EQ(ret[0], expected_results[br_imm]);
    }
}

TEST(execute_control, nested_br_if)
{
    /*
    (func (param i32) (result i32)
      (local i32)
      (block
        get_local 1
        i32.const 0x8000
        i32.or
        set_local 1
        (loop
          get_local 1
          i32.const 1
          i32.add
          set_local 1

          get_local 0
          i32.const 1
          i32.sub
          tee_local 0
          br_if 0

          get_local 1
          i32.const 0x4000
          i32.or
          set_local 1

          get_local 0
          i32.eqz
          br_if 1

          get_local 1
          i32.const 0x2000
          i32.or
          set_local 1)
        get_local 1
        i32.const 0x1000
        i32.or
        set_local 1)
      get_local 1
      i32.const 0x0800
      i32.or)
    */

    const auto bin = from_hex(
        "0061736d0100000001060160017f017f030201000a4a014801017f02402001418080027221010340200141016a"
        "2101200041016b22000d002001418080017221012000450d0120014180c0007221010b20014180207221010b20"
        "01418010720b000e046e616d65020701000200000100");

    for (auto loop_count : {1u, 2u})
    {
        const auto [trap, ret] = execute(parse(bin), 0, {loop_count});
        ASSERT_FALSE(trap);
        ASSERT_EQ(ret.size(), 1);
        EXPECT_EQ(ret[0], (0b11001000 << 8) + loop_count) << loop_count;
    }
}

TEST(execute_control, br_stack_cleanup)
{
    /*
    (func (result i32)
      i32.const 1
      (block
        i32.const 2
        br 0  ;; Should drop 2 from the stack.
      )
    )
    */

    const auto bin = from_hex(
        "0061736d010000000105016000017f030201000a0d010b004101024041020c000b0b000a046e616d6502030100"
        "00");

    const auto [trap, ret] = execute(parse(bin), 0, {});
    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 1);
}

TEST(execute_control, br_if_stack_cleanup)
{
    /*
    (func (param i32) (result i64)
      i64.const 0
      (loop
        i64.const -2  ;; Additional stack item.
        i32.const -1
        get_local 0
        i32.add
        tee_local 0
        br_if 0       ;; Clean up stack.
        drop          ;; Drop the additional stack item.
      )
    )
    */

    const auto bin = from_hex(
        "0061736d0100000001060160017f017e030201000a1501130042000340427e417f20006a22000d001a0b0b000c"
        "046e616d6502050100010000");

    const auto [trap, ret] = execute(parse(bin), 0, {7});
    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0);
}

TEST(execute_control, br_multiple_blocks_stack_cleanup)
{
    /*
    (func
      (block
        i32.const 1
        (loop
          i64.const 2
          br 1
        )
        drop
      )
    )
    */

    const auto bin = from_hex(
        "0061736d01000000010401600000030201000a11010f0002404101034042020c010b1a0b0b000a046e616d6502"
        "03010000");

    const auto [trap, ret] = execute(parse(bin), 0, {7});
    ASSERT_FALSE(trap);
    EXPECT_EQ(ret.size(), 0);
}

TEST(execute_control, block_with_result)
{
    /*
    (func (result i64)
      (block (result i64)
        i64.const -1
      )
    )
    */
    const auto bin = from_hex(
        "0061736d010000000105016000017e030201000a09010700027e427f0b0b000a046e616d650203010000");

    const auto [trap, ret] = execute(parse(bin), 0, {});
    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], uint64_t(-1));
}

TEST(execute_control, br_with_result)
{
    /*
    (func (result i32)
      (block (result i32)
        i32.const 1
        i32.const 2
        i32.const 3
        br 0  ;; Takes the top item as the result, drops remaining 2 items.
      )
    )
    */
    const auto bin = from_hex(
        "0061736d010000000105016000017f03020100070801046d61696e00000a0f010d00027f4101410241030c000b"
        "0b000a046e616d650203010000");

    const auto [trap, ret] = execute(parse(bin), 0, {});
    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 3);
}

TEST(execute_control, br_if_with_result)
{
    /*
    (func (param i32) (result i32)
      (block (result i32)
        i32.const 1
        i32.const 2
        get_local 0
        br_if 0
        i32.xor
      )
    )
    */
    const auto bin = from_hex(
        "0061736d0100000001060160017f017f03020100070801046d61696e00000a10010e00027f4101410220000d00"
        "730b0b000c046e616d6502050100010000");

    for (const auto param : {0u, 1u})
    {
        constexpr uint64_t expected_results[]{
            3,  // br_if not taken, result: 1 xor 2 == 3.
            2,  // br_if taken, result: 2, remaining item dropped.
        };

        const auto [trap, ret] = execute(parse(bin), 0, {param});
        ASSERT_FALSE(trap);
        ASSERT_EQ(ret.size(), 1);
        EXPECT_EQ(ret[0], expected_results[param]);
    }
}
