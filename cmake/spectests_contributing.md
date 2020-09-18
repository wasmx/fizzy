

## Run fuzzer

Run Fizzy vs WABT verification fuzzer, currently in `fuzzing` branch.
Start with small inputs.

```shell script
bin/fizzy-fuzz-parser corpus -max_len=13
```

In case it finds a failure you will see output like
```text
INFO: Seed: 770491456
INFO: Loaded 1 modules   (64068 inline 8-bit counters): 64068 [0xb8dc90, 0xb9d6d4), 
INFO: Loaded 1 PC tables (64068 PCs): 64068 [0xb9d6d8,0xc97b18), 
INFO:    10986 files found in corpus
INFO: seed corpus: files: 10986 min: 1b max: 27419b total: 740862b rss: 40Mb
#11160  INITED cov: 7782 ft: 9644 corp: 227/2677b exec/s: 0 rss: 186Mb
#65536  pulse  cov: 7782 ft: 9644 corp: 227/2677b lim: 13 exec/s: 32768 rss: 215Mb
#131072 pulse  cov: 7782 ft: 9644 corp: 227/2677b lim: 13 exec/s: 32768 rss: 248Mb
#262144 pulse  cov: 7782 ft: 9644 corp: 227/2677b lim: 13 exec/s: 32768 rss: 314Mb
  MALFORMED: invalid limits 4
/home/chfast/Projects/wasmx/fizzy/test/fuzzer/parser_fuzzer.cpp:47:9: runtime error: execution reached an unreachable program point
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior /home/chfast/Projects/wasmx/fizzy/test/fuzzer/parser_fuzzer.cpp:47:9 in 
MS: 1 ChangeBit-; base unit: 846f4928f92649789961543e93544b4c2ed6fc44
0x0,0x61,0x73,0x6d,0x1,0x0,0x0,0x0,0x5,0x3,0x1,0x4,0x2b,
\x00asm\x01\x00\x00\x00\x05\x03\x01\x04+
artifact_prefix='./'; Test unit written to ./crash-016934f781c41276bfacda9a90385cef85a88d5f
Base64: AGFzbQEAAAAFAwEEKw==
```

In this example Fizzy reported the error `MALFORMED: invalid limits 4`
while WABT considers the Wasm binary valid.
The test unit (Wasm binary) is also printed in various forms and saved as
`crash-016934f781c41276bfacda9a90385cef85a88d5f`.


## Check WebAssembly spec interpreter

In case of false negative validation result in WABT it is likely that
such case is missing in the WAST spec test.

Build the `wasm reference interpreter` following official instructions.

Rename `crash-016934f781c41276bfacda9a90385cef85a88d5f` to `crash.wasm`
as the interpreter need proper files extension to recognize file type.

```shell script
./wasm crash.wasm
```

Output:
```text
crash.wasm:0xb: decoding error: integer too large
```

Good news. This is effectively the same error reported as in Fizzy. In Wasm 1.0 `limits` kind
can be 0 or 1. The the value 4 in the `crash.wasm` is larger than the max 1.


## Inspect with WABT tools

You can use `wasm2wat` or `wasm-validate` tools to confirm the bug in WABT.
Make sure you use the same version as used for fuzzing.
Disable all extensions except "mutable globals".

In our example, `wasm2wat crash.wasm`:
```webassembly
(module
  (memory (;0;) 43 i64))
```
This indicates that the issue is in the memory section.

Unfortunately, we get different error as the data section also has invalid length.
Let's fix that.


## Prepare WAST test

Dump the test unit (`xxd -p -c1000 crash.wasm`):

```hexdump
0061736d01000000050301042b
```

Here are created WAST tests, placed in `new.wast` file. In the original `crash.wasm` the invalid 4
is followed by `2b` byte. We added 2 more variants of the test with byte `00` and without any 
following byte (remember to change the section length if you actually modify the length).

```wast
(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\05\03\01"                          ;; memory section
    "\04\2b"                             ;; malformed memory limit 4
  )
  "integer too large"
)

(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\05\03\01"                          ;; memory section
    "\04\00"                             ;; malformed memory limit 4
  )
  "integer too large"
)

(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\05\02\01"                          ;; memory section
    "\04"                                ;; malformed memory limit 4
  )
  "integer too large"
)
```

These tests can be rechecked with WABT.

```shell script
wast2json new.wast
wasm-validate new.0.wasm
wasm-validate new.1.wasm
wasm-validate new.2.wasm
```

The last test case is actually failing in WABT for some other reason, but it can stay as a valid
spectests addition.

```text
000000c: error: unable to read u32 leb128: memory initial page count
```


## Debug & fix WABT

Debug `wasm-validate new.0.wasm` to find out why it passes validation.
Binary loading code is in `binary-reader.cc`.

The example bug is fixed by https://github.com/WebAssembly/wabt/pull/1547.


## Send WAST tests upstream