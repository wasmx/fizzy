// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"

auto generate_wasm()
{
    /* wat2wasm
    (memory (import "" "m") 1 1)
    (func (result f32)
      (f32.load offset=0 (i32.const 0))

      (f32.load offset=4 (i32.const 0))
      (f32.add)
      (f32.load offset=8 (i32.const 0))
      (f32.add)
      (f32.load offset=12 (i32.const 0))
      (f32.add)
      (f32.load offset=16 (i32.const 0))
      (f32.add)
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017d02080100016d02010101030201000a21011f0041002a020041002a020492"
        "41002a02089241002a020c9241002a0210920b");
}
