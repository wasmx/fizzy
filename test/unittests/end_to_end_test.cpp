// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/execute_helpers.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;
using namespace fizzy::test;

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
    auto instance = instantiate(parse(wasm));

    EXPECT_THAT(execute(*instance, 0, {20, 22}), Result(20 + 22 + 20));
}

TEST(end_to_end, milestone2)
{
    // from https://gist.github.com/poemm/356ba2c6826c6f5953db874e37783417#file-mul256_opt0-wat
    /* wat2wasm
    (module
      (type (;0;) (func (param i32 i32 i32)))
      (func $mul256 (type 0) (param i32 i32 i32)
        (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32
    i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32
    i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32
    i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i64 i64 i64 i64 i64 i64 i64
    i64 i64 i64 i64 i64 i64)
        i32.const 66560
        local.set 3
        i32.const 64
        local.set 4
        local.get 3
        local.get 4
        i32.sub
        local.set 5
        i32.const 0
        local.set 6
        local.get 5
        local.get 0
        i32.store offset=60
        local.get 5
        local.get 1
        i32.store offset=56
        local.get 5
        local.get 2
        i32.store offset=52
        local.get 5
        i32.load offset=60
        local.set 7
        local.get 5
        local.get 7
        i32.store offset=48
        local.get 5
        local.get 6
        i32.store offset=44
        block  ;; label = @1
          loop  ;; label = @2
            i32.const 16
            local.set 8
            local.get 5
            i32.load offset=44
            local.set 9
            local.get 9
            local.set 10
            local.get 8
            local.set 11
            local.get 10
            local.get 11
            i32.lt_s
            local.set 12
            i32.const 1
            local.set 13
            local.get 12
            local.get 13
            i32.and
            local.set 14
            local.get 14
            i32.eqz
            br_if 1 (;@1;)
            i32.const 0
            local.set 15
            local.get 5
            i32.load offset=48
            local.set 16
            local.get 5
            i32.load offset=44
            local.set 17
            i32.const 2
            local.set 18
            local.get 17
            local.get 18
            i32.shl
            local.set 19
            local.get 16
            local.get 19
            i32.add
            local.set 20
            local.get 20
            local.get 15
            i32.store
            local.get 5
            i32.load offset=44
            local.set 21
            i32.const 1
            local.set 22
            local.get 21
            local.get 22
            i32.add
            local.set 23
            local.get 5
            local.get 23
            i32.store offset=44
            br 0 (;@2;)
            unreachable
          end
          unreachable
        end
        i32.const 0
        local.set 24
        local.get 5
        local.get 24
        i32.store offset=40
        block  ;; label = @1
          loop  ;; label = @2
            i32.const 8
            local.set 25
            local.get 5
            i32.load offset=40
            local.set 26
            local.get 26
            local.set 27
            local.get 25
            local.set 28
            local.get 27
            local.get 28
            i32.lt_s
            local.set 29
            i32.const 1
            local.set 30
            local.get 29
            local.get 30
            i32.and
            local.set 31
            local.get 31
            i32.eqz
            br_if 1 (;@1;)
            i32.const 0
            local.set 32
            local.get 5
            local.get 32
            i32.store offset=36
            local.get 5
            local.get 32
            i32.store offset=32
            block  ;; label = @3
              loop  ;; label = @4
                i32.const 8
                local.set 33
                local.get 5
                i32.load offset=32
                local.set 34
                local.get 34
                local.set 35
                local.get 33
                local.set 36
                local.get 35
                local.get 36
                i32.lt_s
                local.set 37
                i32.const 1
                local.set 38
                local.get 37
                local.get 38
                i32.and
                local.set 39
                local.get 39
                i32.eqz
                br_if 1 (;@3;)
                local.get 5
                i32.load offset=48
                local.set 40
                local.get 5
                i32.load offset=40
                local.set 41
                local.get 5
                i32.load offset=32
                local.set 42
                local.get 41
                local.get 42
                i32.add
                local.set 43
                i32.const 2
                local.set 44
                local.get 43
                local.get 44
                i32.shl
                local.set 45
                local.get 40
                local.get 45
                i32.add
                local.set 46
                local.get 46
                i32.load
                local.set 47
                local.get 47
                local.set 48
                local.get 48
                i64.extend_i32_u
                local.set 89
                local.get 5
                i32.load offset=56
                local.set 49
                local.get 5
                i32.load offset=32
                local.set 50
                i32.const 2
                local.set 51
                local.get 50
                local.get 51
                i32.shl
                local.set 52
                local.get 49
                local.get 52
                i32.add
                local.set 53
                local.get 53
                i32.load
                local.set 54
                local.get 54
                local.set 55
                local.get 55
                i64.extend_i32_u
                local.set 90
                local.get 5
                i32.load offset=52
                local.set 56
                local.get 5
                i32.load offset=40
                local.set 57
                i32.const 2
                local.set 58
                local.get 57
                local.get 58
                i32.shl
                local.set 59
                local.get 56
                local.get 59
                i32.add
                local.set 60
                local.get 60
                i32.load
                local.set 61
                local.get 61
                local.set 62
                local.get 62
                i64.extend_i32_u
                local.set 91
                local.get 90
                local.get 91
                i64.mul
                local.set 92
                local.get 89
                local.get 92
                i64.add
                local.set 93
                local.get 5
                local.get 93
                i64.store offset=24
                local.get 5
                i32.load offset=36
                local.set 63
                local.get 63
                local.set 64
                local.get 64
                i64.extend_i32_u
                local.set 94
                local.get 5
                i64.load offset=24
                local.set 95
                local.get 95
                local.get 94
                i64.add
                local.set 96
                local.get 5
                local.get 96
                i64.store offset=24
                local.get 5
                i64.load offset=24
                local.set 97
                i64.const 32
                local.set 98
                local.get 97
                local.get 98
                i64.shr_u
                local.set 99
                local.get 5
                local.get 99
                i64.store offset=16
                local.get 5
                i64.load offset=24
                local.set 100
                local.get 100
                i32.wrap_i64
                local.set 65
                local.get 5
                local.get 65
                i32.store offset=12
                local.get 5
                i32.load offset=12
                local.set 66
                local.get 5
                i32.load offset=48
                local.set 67
                local.get 5
                i32.load offset=40
                local.set 68
                local.get 5
                i32.load offset=32
                local.set 69
                local.get 68
                local.get 69
                i32.add
                local.set 70
                i32.const 2
                local.set 71
                local.get 70
                local.get 71
                i32.shl
                local.set 72
                local.get 67
                local.get 72
                i32.add
                local.set 73
                local.get 73
                local.get 66
                i32.store
                local.get 5
                i64.load offset=16
                local.set 101
                local.get 101
                i32.wrap_i64
                local.set 74
                local.get 5
                local.get 74
                i32.store offset=36
                local.get 5
                i32.load offset=32
                local.set 75
                i32.const 1
                local.set 76
                local.get 75
                local.get 76
                i32.add
                local.set 77
                local.get 5
                local.get 77
                i32.store offset=32
                br 0 (;@4;)
                unreachable
              end
              unreachable
            end
            local.get 5
            i32.load offset=36
            local.set 78
            local.get 5
            i32.load offset=48
            local.set 79
            local.get 5
            i32.load offset=40
            local.set 80
            i32.const 8
            local.set 81
            local.get 80
            local.get 81
            i32.add
            local.set 82
            i32.const 2
            local.set 83
            local.get 82
            local.get 83
            i32.shl
            local.set 84
            local.get 79
            local.get 84
            i32.add
            local.set 85
            local.get 85
            local.get 78
            i32.store
            local.get 5
            i32.load offset=40
            local.set 86
            i32.const 1
            local.set 87
            local.get 86
            local.get 87
            i32.add
            local.set 88
            local.get 5
            local.get 88
            i32.store offset=40
            br 0 (;@2;)
            unreachable
          end
          unreachable
        end
        return)
      (memory 2)
      (export "memory" (memory 0))
      (export "mul256" (func $mul256))
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001070160037f7f7f00030201000503010002071302066d656d6f72790200066d756c323536"
        "00000a850601820602567f0d7e41808804210341c0002104200320046b2105410021062005200036023c200520"
        "0136023820052002360234200528023c2107200520073602302005200636022c0240034041102108200528022c"
        "21092009210a2008210b200a200b48210c4101210d200c200d71210e200e450d014100210f2005280230211020"
        "0528022c21114102211220112012742113201020136a21142014200f360200200528022c211541012116201520"
        "166a21172005201736022c0c00000b000b410021182005201836022802400340410821192005280228211a201a"
        "211b2019211c201b201c48211d4101211e201d201e71211f201f450d0141002120200520203602242005202036"
        "022002400340410821212005280220212220222123202121242023202448212541012126202520267121272027"
        "450d0120052802302128200528022821292005280220212a2029202a6a212b4102212c202b202c74212d202820"
        "2d6a212e202e280200212f202f21302030ad215920052802382131200528022021324102213320322033742134"
        "203120346a213520352802002136203621372037ad215a20052802342138200528022821394102213a2039203a"
        "74213b2038203b6a213c203c280200213d203d213e203ead215b205a205b7e215c2059205c7c215d2005205d37"
        "03182005280224213f203f21402040ad215e2005290318215f205f205e7c216020052060370318200529031821"
        "61422021622061206288216320052063370310200529031821642064a721412005204136020c200528020c2142"
        "200528023021432005280228214420052802202145204420456a21464102214720462047742148204320486a21"
        "4920492042360200200529031021652065a7214a2005204a3602242005280220214b4101214c204b204c6a214d"
        "2005204d3602200c00000b000b2005280224214e2005280230214f2005280228215041082151205020516a2152"
        "4102215320522053742154204f20546a21552055204e3602002005280228215641012157205620576a21582005"
        "20583602280c00000b000b0f0b");

    const auto module = parse(wasm);

    const auto func_idx = find_exported_function(*module, "mul256");
    ASSERT_TRUE(func_idx);

    auto instance = instantiate(*module);

    auto& memory = *instance->memory;
    // This performs uint256 x uint256 -> uint512 multiplication.
    // Arg1: 2^255 + 1
    memory[0] = 1;
    memory[31] = 0x80;
    // Arg2: 2^255 + 2^254 + 0xff
    memory[32] = 0xff;
    memory[63] = 0xc0;
    EXPECT_THAT(execute(*instance, *func_idx, {64, 0, 32}), Result());
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
    /* wat2wasm
    (module
      (func (export "test") (param i32 i32 i32) (result i32)
        (local i32 i32 i32 i32 i32)
        block  ;; label = @1
          block  ;; label = @2
            local.get 0
            i32.const 1
            i32.lt_s
            br_if 0 (;@2;)
            local.get 1
            i32.const 1
            i32.lt_s
            br_if 1 (;@1;)
            local.get 1
            i32.const 7
            i32.mul
            i32.const 1
            i32.add
            local.set 3
            i32.const 0
            local.set 4
            i32.const 0
            local.set 5
            block  ;; label = @3
              loop  ;; label = @4
                block  ;; label = @5
                  block  ;; label = @6
                    local.get 2
                    i32.const 0
                    i32.le_s
                    br_if 0 (;@6;)
                    local.get 5
                    i32.const 1
                    i32.add
                    local.set 5
                    i32.const 0
                    local.set 6
                    loop  ;; label = @7
                      local.get 5
                      i32.const 7
                      i32.add
                      local.set 5
                      i32.const 0
                      local.set 7
                      loop  ;; label = @8
                        local.get 5
                        i32.const -2
                        i32.and
                        i32.const 8
                        i32.eq
                        br_if 5 (;@3;)
                        local.get 5
                        i32.const 2
                        i32.div_s
                        local.set 5
                        local.get 7
                        i32.const 1
                        i32.add
                        local.tee 7
                        local.get 2
                        i32.lt_s
                        br_if 0 (;@8;)
                      end
                      local.get 6
                      i32.const 1
                      i32.add
                      local.tee 6
                      local.get 1
                      i32.lt_s
                      br_if 0 (;@7;)
                      br 2 (;@5;)
                    end
                  end
                  local.get 3
                  local.get 5
                  i32.add
                  local.set 5
                end
                local.get 4
                i32.const 1
                i32.add
                local.tee 4
                local.get 0
                i32.lt_s
                br_if 0 (;@4;)
              end
              local.get 5
              return
            end
            i32.const 4
            return
          end
          i32.const 0
          return
        end
        local.get 0)
      (memory 1)
      (export "memory" (memory 0))
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001080160037f7f7f017f03020100050301000107110204746573740000066d656d6f727902"
        "000aa50101a20101057f0240024020004101480d0020014101480d01200141076c41016a210341002104410021"
        "050240034002400240200241004c0d00200541016a2105410021060340200541076a2105410021070340200541"
        "7e714108460d05200541026d2105200741016a22072002480d000b200641016a22062001480d000c020b0b2003"
        "20056a21050b200441016a22042000480d000b20050f0b41040f0b41000f0b20000b");

    const auto module = parse(wasm);

    const auto func_idx = find_exported_function(*module, "test");
    ASSERT_TRUE(func_idx);

    auto instance = instantiate(*module);
    EXPECT_THAT(execute(*instance, *func_idx, {10, 2, 5}), Result(4));
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

    const auto func_idx = find_exported_function(*module, "test");
    ASSERT_TRUE(func_idx);

    auto instance = instantiate(*module);
    EXPECT_THAT(execute(*instance, *func_idx, {0, 2}), Result());
    EXPECT_EQ(hex(instance->memory->substr(0, 2 * sizeof(int))), "d2040000d2040000");
}
