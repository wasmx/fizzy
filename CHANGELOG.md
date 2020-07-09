# Changelog

Documentation of all notable changes to the **Fizzy** project.

The format is based on [Keep a Changelog],
and this project adheres to [Semantic Versioning].

## [0.2.0] — 2020-06-29

Firstly, this release implements many validation steps prescribed by the specification, with the exception of type checking.

It passes a large part of the [official test suite (spectest 1.0)]:
  - 4481 of 4490 binary parser and execution tests,
  - 659 of 942 validation tests,
  - 6381 skipped due to containing floating-point instructions or testing text format parser.

Secondly, two major optimisations are included: branch resolution is done once in the parser and not during execution; and an optimised stack implementation is introduced.

#### Performance comparison to v0.1.0

The time of WebAssembly module instantiation (including binary loading and verification) has increased by 15–30%, while the execution is approximately twice as fast.

##### Detailed benchmark results
```
Comparing Fizzy 0.1.0 to 0.2.0 (Intel Haswell CPU, 4.0 GHz)
Benchmark                                  CPU Time [µs]       Old       New
----------------------------------------------------------------------------

fizzy/parse/blake2b                              +0.1637        12        14
fizzy/instantiate/blake2b                        +0.1793        16        19
fizzy/parse/ecpairing                            +0.1551       681       786
fizzy/instantiate/ecpairing                      +0.1352       730       828
fizzy/parse/keccak256                            +0.2674        20        26
fizzy/instantiate/keccak256                      +0.2974        24        31
fizzy/parse/memset                               +0.1975         3         4
fizzy/instantiate/memset                         +0.1580         7         8
fizzy/parse/mul256_opt0                          +0.1739         4         5
fizzy/instantiate/mul256_opt0                    +0.1262         8         9
fizzy/parse/sha1                                 +0.2054        19        23
fizzy/instantiate/sha1                           +0.2159        23        28
fizzy/parse/sha256                               +0.1333        31        35
fizzy/instantiate/sha256                         +0.1518        35        41
fizzy/parse/micro/eli_interpreter                +0.4832         2         3
fizzy/instantiate/micro/eli_interpreter          +0.2681         6         7
fizzy/parse/micro/factorial                      +0.2279         1         1
fizzy/instantiate/micro/factorial                +0.5395         1         1
fizzy/parse/micro/fibonacci                      +0.2599         1         1
fizzy/instantiate/micro/fibonacci                +0.5027         1         1
fizzy/parse/micro/spinner                        +0.2709         1         1
fizzy/instantiate/micro/spinner                  +0.6040         1         1

fizzy/execute/blake2b/512_bytes_rounds_1         -0.4871       167        86
fizzy/execute/blake2b/512_bytes_rounds_16        -0.4922      2543      1291
fizzy/execute/ecpairing/onepoint                 -0.4883    932374    477057
fizzy/execute/keccak256/512_bytes_rounds_1       -0.4542       185       101
fizzy/execute/keccak256/512_bytes_rounds_16      -0.4642      2712      1453
fizzy/execute/memset/256_bytes                   -0.4643        14         7
fizzy/execute/memset/60000_bytes                 -0.4680      3020      1607
fizzy/execute/mul256_opt0/input0                 -0.4134        46        27
fizzy/execute/mul256_opt0/input1                 -0.4119        46        27
fizzy/execute/sha1/512_bytes_rounds_1            -0.4815       182        95
fizzy/execute/sha1/512_bytes_rounds_16           -0.4836      2547      1315
fizzy/execute/sha256/512_bytes_rounds_1          -0.5236       196        93
fizzy/execute/sha256/512_bytes_rounds_16         -0.5289      2719      1281
fizzy/execute/micro/eli_interpreter/halt         -0.5890         0         0
fizzy/execute/micro/eli_interpreter/exec105      -0.5095        10         5
fizzy/execute/micro/factorial/10                 -0.4531         1         1
fizzy/execute/micro/factorial/20                 -0.4632         2         1
fizzy/execute/micro/fibonacci/24                 -0.4107     17374     10238
fizzy/execute/micro/spinner/1                    -0.5706         0         0
fizzy/execute/micro/spinner/1000                 -0.4801        20        10
```


### Added

- Introduced a Fizzy logo. [#326](https://github.com/wasmx/fizzy/pull/326) [#344](https://github.com/wasmx/fizzy/pull/344)
  [#399](https://github.com/wasmx/fizzy/pull/399)
- API for resolving imported function names into function types. [#318](https://github.com/wasmx/fizzy/pull/318)
- Validation of alignment argument in memory instructions. [#310](https://github.com/wasmx/fizzy/pull/310)
- Validation of `br_table` instruction. [#368](https://github.com/wasmx/fizzy/pull/368)
- Validation of constant expressions. [#352](https://github.com/wasmx/fizzy/pull/352) 
  [#393](https://github.com/wasmx/fizzy/pull/393) [#394](https://github.com/wasmx/fizzy/pull/394) 
  [#397](https://github.com/wasmx/fizzy/pull/397)
- Validation of data section. [#317](https://github.com/wasmx/fizzy/pull/317)
- Validation of element section. [#350](https://github.com/wasmx/fizzy/pull/350)
- Validation of export section. [#357](https://github.com/wasmx/fizzy/pull/357)
- Validation of `global.get`/`global.set` instructions. [#351](https://github.com/wasmx/fizzy/pull/351)
- Validation of `local.get`/`local.set`/`local.tee` instructions. [#363](https://github.com/wasmx/fizzy/pull/363)
- Validation of stack height at the end of blocks and functions, at branch instructions, and after unreachable
  instructions.  [#319](https://github.com/wasmx/fizzy/pull/319) [#356](https://github.com/wasmx/fizzy/pull/356)
  [#343](https://github.com/wasmx/fizzy/pull/343) [#384](https://github.com/wasmx/fizzy/pull/384)
  [#386](https://github.com/wasmx/fizzy/pull/386)
- Validation of start section. [#353](https://github.com/wasmx/fizzy/pull/353)
- `fizzy-bench` now requires `.inputs` files to include the signature of the called function.
  [#331](https://github.com/wasmx/fizzy/pull/331)
- Better unit tests coverage. [#320](https://github.com/wasmx/fizzy/pull/320)
  [#335](https://github.com/wasmx/fizzy/pull/335) [#337](https://github.com/wasmx/fizzy/pull/337)
  [#346](https://github.com/wasmx/fizzy/pull/346) [#364](https://github.com/wasmx/fizzy/pull/364)
  [#378](https://github.com/wasmx/fizzy/pull/378) [#380](https://github.com/wasmx/fizzy/pull/380)
- CI improvements. [#322](https://github.com/wasmx/fizzy/pull/322) [#330](https://github.com/wasmx/fizzy/pull/330)
  [#333](https://github.com/wasmx/fizzy/pull/333) [#342](https://github.com/wasmx/fizzy/pull/342)
  [#336](https://github.com/wasmx/fizzy/pull/336) [#348](https://github.com/wasmx/fizzy/pull/348)
  [#361](https://github.com/wasmx/fizzy/pull/361)
- Micro-benchmark for executing interpreter switch. [#374](https://github.com/wasmx/fizzy/pull/374)
  [#396](https://github.com/wasmx/fizzy/pull/396)

### Changed

- Exception error messages cleaned up. [#315](https://github.com/wasmx/fizzy/pull/315)
- `fizzy-bench` now reports more useful error messages. [#316](https://github.com/wasmx/fizzy/pull/316)
- Operand stack optimizations. [#247](https://github.com/wasmx/fizzy/pull/247)
  [#265](https://github.com/wasmx/fizzy/pull/265) [#339](https://github.com/wasmx/fizzy/pull/339)
- Optimization to resolve all jump destinations during parsing instead of having control stack in execution.
  [#304](https://github.com/wasmx/fizzy/pull/304) [#312](https://github.com/wasmx/fizzy/pull/312)
  [#341](https://github.com/wasmx/fizzy/pull/341) [#373](https://github.com/wasmx/fizzy/pull/373)
  [#391](https://github.com/wasmx/fizzy/pull/391)
- Better execution time measurement in `fizzy-bench`. [#383](https://github.com/wasmx/fizzy/pull/383)

### Fixed

- Validation of stack height for call instructions. [#338](https://github.com/wasmx/fizzy/pull/338)
  [#387](https://github.com/wasmx/fizzy/pull/387)
- Validation of type indices in function section. [#334](https://github.com/wasmx/fizzy/pull/334)
- `fizzy-bench` executes the start function on all engines (previously it did not on wabt).
  [#332](https://github.com/wasmx/fizzy/pull/332)
  [#349](https://github.com/wasmx/fizzy/pull/349) 
- `fizzy-bench` correctly handles errors during validation phase. [#340](https://github.com/wasmx/fizzy/pull/340)

## [0.1.0] — 2020-05-14

First release!

### Added

- Supports all instructions with the exception of floating-point ones.
- Passes a large part of the [official test suite (spectest 1.0)]:
  - 4482 of 4490 binary parser tests,
  - 301 of 942 validation tests (no full validation implemented yet),
  - 6381 skipped due to containing floating-point instructions or testing text format parser.
- Tools included:
  - `fizzy-bench` for benchmarking
  - `fizzy-spectests` for executing the official test suite
  - `fizzy-unittests` for running unit tests
- It is missing a public API (the embedder API) and thus not ready yet for integration.

[0.1.0]: https://github.com/wasmx/fizzy/releases/tag/v0.1.0
[0.2.0]: https://github.com/wasmx/fizzy/releases/tag/v0.2.0

[Keep a Changelog]: https://keepachangelog.com/en/1.0.0/
[Semantic Versioning]: https://semver.org
[official test suite (spectest 1.0)]: https://github.com/WebAssembly/spec/releases/tag/wg-1.0
