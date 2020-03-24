#include "execute.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/hex.hpp>

using namespace fizzy;

TEST(end_to_end, milestone1)
{
    /* wat2wasm
    (module
      (func $add (param $lhs i32) (param $rhs i32) (result i32)
        (local $local1 i32)
        local.get $lhs
        local.get $rhs
        i32.add
        local.get $local1
        i32.add
        local.tee $local1
        local.get $lhs
        i32.add
      )
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001070160027f7f017f030201000a13011101017f200020016a20026a220220006a0b");
    const auto module = parse(wasm);

    const auto [trap, ret] = execute(module, 0, {20, 22});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 20 + 22 + 20);
}

TEST(end_to_end, milestone2)
{
    // from https://gist.github.com/poemm/356ba2c6826c6f5953db874e37783417#file-mul256_opt0-wat
    // but replaced global with const and removed return statement
    const auto bin = from_hex(
        "0061736d0100000001070160037f7f7f000302010005030100020a840601"
        "810602567f0d7e41808804210341c0002104200320046b21054100210620"
        "05200036023c2005200136023820052002360234200528023c2107200520"
        "073602302005200636022c0240034041102108200528022c21092009210a"
        "2008210b200a200b48210c4101210d200c200d71210e200e450d01410021"
        "0f20052802302110200528022c2111410221122011201274211320102013"
        "6a21142014200f360200200528022c211541012116201520166a21172005"
        "201736022c0c00000b000b41002118200520183602280240034041082119"
        "2005280228211a201a211b2019211c201b201c48211d4101211e201d201e"
        "71211f201f450d0141002120200520203602242005202036022002400340"
        "410821212005280220212220222123202121242023202448212541012126"
        "202520267121272027450d01200528023021282005280228212920052802"
        "20212a2029202a6a212b4102212c202b202c74212d2028202d6a212e202e"
        "280200212f202f21302030ad215920052802382131200528022021324102"
        "213320322033742134203120346a213520352802002136203621372037ad"
        "215a20052802342138200528022821394102213a2039203a74213b203820"
        "3b6a213c203c280200213d203d213e203ead215b205a205b7e215c205920"
        "5c7c215d2005205d3703182005280224213f203f21402040ad215e200529"
        "0318215f205f205e7c216020052060370318200529031821614220216220"
        "61206288216320052063370310200529031821642064a721412005204136"
        "020c200528020c2142200528023021432005280228214420052802202145"
        "204420456a21464102214720462047742148204320486a21492049204236"
        "0200200529031021652065a7214a2005204a3602242005280220214b4101"
        "214c204b204c6a214d2005204d3602200c00000b000b2005280224214e20"
        "05280230214f2005280228215041082151205020516a2152410221532052"
        "2053742154204f20546a21552055204e3602002005280228215641012157"
        "205620576a2158200520583602280c00000b000b0b");

    const auto module = parse(bin);

    auto instance = instantiate(module);

    auto& memory = *instance->memory;
    // This performs uint256 x uint256 -> uint512 multiplication.
    // Arg1: 2^255 + 1
    memory[0] = 1;
    memory[31] = 0x80;
    // Arg2: 2^255 + 2^254 + 0xff
    memory[32] = 0xff;
    memory[63] = 0xc0;
    // TODO: use find_exported_function
    const auto [trap, ret] = execute(*instance, 0, {64, 0, 32});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    EXPECT_EQ(hex(memory.substr(64, 64)),
        "ff00000000000000000000000000000000000000000000000000000000000040"
        "8000000000000000000000000000000000000000000000000000000000000060");
}

TEST(end_to_end, nested_loops_in_c)
{
    /*
    int test(int a, int b, int c)
    {
        int ret = 0;
        for (int i = 0; i < a; i++)
        {
            ret++;
            for (int j = 0; j < b; j++)
            {
                ret += 7;
                for (int k = 0; k < c; k++)
                {
                    ret /= 2;
                    if (ret == 4)
                        return ret;
                }
            }
        }
        return ret;
    }
    */

    const auto bin = from_hex(
        "0061736d01000000010b0260000060037f7f7f017f030201010405017001"
        "010105030100020615037f01418088040b7f00418088040b7f004180080b"
        "072c0404746573740000066d656d6f727902000b5f5f686561705f626173"
        "6503010a5f5f646174615f656e6403020aa50101a20101057f0240024020"
        "004101480d0020014101480d01200141076c41016a210341002104410021"
        "050240034002400240200241004c0d00200541016a210541002106034020"
        "0541076a21054100210703402005417e714108460d05200541026d210520"
        "0741016a22072002480d000b200641016a22062001480d000c020b0b2003"
        "20056a21050b200441016a22042000480d000b20050f0b41040f0b41000f"
        "0b20000b");

    const auto module = parse(bin);

    const auto func_idx = find_exported_function(module, "test");
    ASSERT_TRUE(func_idx);

    // Ignore the results for now
    const auto [trap, ret] = execute(module, *func_idx, {10, 2, 5});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 4);
}

TEST(end_to_end, memset)
{
    /* wat2wasm
    (module
      (func (export "test") (param $p0 i32) (param $p1 i32)
        block $B0
          local.get $p1
          i32.const 1
          i32.lt_s
          br_if $B0
          loop $L1
            local.get $p0
            i32.const 1234
            i32.store
            local.get $p0
            i32.const 4
            i32.add
            local.set $p0
            local.get $p1
            i32.const -1
            i32.add
            local.tee $p1
            br_if $L1
        end
      end)
      (memory 1)
      (export "memory" (memory 0))
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160027f7f0003020100050301000107110204746573740000066d656d6f727902000a"
        "29012700024020014101480d000340200041d209360200200041046a21002001417f6a22010d000b0b0b");

    const auto module = parse(wasm);

    const auto func_idx = find_exported_function(module, "test");
    ASSERT_TRUE(func_idx);

    auto instance = instantiate(module);
    const auto [trap, ret] = execute(*instance, *func_idx, {0, 2});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    EXPECT_EQ(hex(instance->memory->substr(0, 2 * sizeof(int))), "d2040000d2040000");
}
