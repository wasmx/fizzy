# Changelog

Documentation of all notable changes to the **Fizzy** project.

The format is based on [Keep a Changelog],
and this project adheres to [Semantic Versioning].

## [0.5.0] — unreleased

This release focuses on behind the scenes code and test cleanups, and preparation for a public API.

Fizzy passes all(*) of the [official test suite (spectest 1.0)](https://github.com/wasmx/wasm-spec/tree/w3c-1.0-jsontests-20200813/test/core/json):
  - 17911 of 17913 binary parser and execution tests,
  - 989 of 989 validation tests,
  - 477 skipped due to testing text format parser.

(*) The two failures are caused by, in our opinion, two bugs in the test cases.

A notable change is the upgrade from WABT 1.0.12 to 1.0.19 in our testing infrastructure.
While this allows us to conduct fuzzing, it must be noted there is a [significant speed regression](https://github.com/WebAssembly/wabt/issues/1550)
with the newer WABT.

There is a slight speed regression compared to Fizzy 0.4.0, which can be attributed to
the code layout restructuring in [#518](https://github.com/wasmx/fizzy/pull/518).

### Detailed benchmark results

#### Instantiation

```
                               |            fizzy            |   wabt  |  wasm3  |
                               |       0.5.0       |  0.4.0  |  1.0.19 |  0.4.7  |
                                abs. [µs]  rel. [%]  rel. [%]  rel. [%]  rel. [%]
geometric mean                                  100       101       279        27
blake2b                                28       100        99       306        38
ecpairing                            1399       100       101       305         3
keccak256                              47       100       103       289        18
memset                                 10       100       100       285        79
mul256_opt0                            12       100       101       246        56
ramanujan_pi                           28       100       101       260        30
sha1                                   43       100       101       284        19
sha256                                 69       100       100       297        12
taylor_pi                               7       100        99       246       116
```

#### Execution

```
                               |            fizzy            |        wabt       |  wasm3  |
                               |       0.5.0       |  0.4.0  |  1.0.19 |  1.0.12 |  0.4.7  |
                                abs. [µs]  rel. [%]  rel. [%]  rel. [%]  rel. [%]  rel. [%]
geometirc mean                                  100        95       508       167        26
blake2b/512_bytes_rounds_1             87       100        97       523       172        26
blake2b/512_bytes_rounds_16          1329       100        96       525       172        26
ecpairing/onepoint                 411770       100       102       516       172        38
keccak256/512_bytes_rounds_1          105       100        96       416       162        18
keccak256/512_bytes_rounds_16        1548       100        96       418       162        18
memset/256_bytes                        7       100        96       509       155        28
memset/60000_bytes                   1623       100        95       507       155        28
mul256_opt0/input0                     29       100        91       545       152        22
mul256_opt0/input1                     29       100        89       543       152        22
ramanujan_pi/33_runs                  138       100        91       483       163        22
sha1/512_bytes_rounds_1                94       100        97       563       181        29
sha1/512_bytes_rounds_16             1318       100        96       566       179        29
sha256/512_bytes_rounds_1              96       100        98       539       186        30
sha256/512_bytes_rounds_16           1326       100        97       540       186        30
taylor_pi/pi_1000000_runs           41668       100        96       465       154        29
```

### Added

- `fizzy-testfloat` tool to check floating-point instructions implementation against Berkeley TestFloat test suite.
  [#511](https://github.com/wasmx/fizzy/pull/511)
- `instantiate` function now has `memory_pages_limit` parameter to configure hard limit on memory allocation.
  `fizzy-spectests` uses the required 4 GB memory limit now.
  [#515](https://github.com/wasmx/fizzy/pull/515)
- `fizzy-spectests` tool can now run tests form a single `*.wast` file. [#525](https://github.com/wasmx/fizzy/pull/525)
- The `shellcheck` linter is now used on CI. [#537](https://github.com/wasmx/fizzy/pull/537)
- There is now `lib/fizzy/cxx20` directory with substitute implementations of C++20 standard library features, which Fizzy
  uses when compiled in pre-C++20 standard (default). Currently it contains `bit_cast` and `span` utilities.
  [#535](https://github.com/wasmx/fizzy/pull/535) [#541](https://github.com/wasmx/fizzy/pull/541)

### Changed

- Optimization in `execute` to allocate memory only once for all of the arguments, locals, and operand stack.
  [#382](https://github.com/wasmx/fizzy/pull/382)
- Trap handling in `execute` has been simplified. [#425](https://github.com/wasmx/fizzy/pull/425)
- `fizzy-bench` now uses WABT version 1.0.19. [#443](https://github.com/wasmx/fizzy/pull/443)
- New simpler implementation of `f32.nearest` and `f64.nearest`. [#504](https://github.com/wasmx/fizzy/pull/504)
- `execute.hpp` now includes declarations only for `execute` functions. Other functions of C++ API (`instantiate`,
  `resolve_imported_functions`, exports access) have been moved to `instantiate.hpp`.
  [#518](https://github.com/wasmx/fizzy/pull/518)
- More strict compiler warnings have been enabled. [#508](https://github.com/wasmx/fizzy/pull/508)
- Remove some less useful benchmarking cases. [#555](https://github.com/wasmx/fizzy/pull/555)
- CI improvements. [#465](https://github.com/wasmx/fizzy/pull/465) [#516](https://github.com/wasmx/fizzy/pull/516)
  [#519](https://github.com/wasmx/fizzy/pull/519) [#520](https://github.com/wasmx/fizzy/pull/520)
  [#523](https://github.com/wasmx/fizzy/pull/523) [#532](https://github.com/wasmx/fizzy/pull/532)
  [#549](https://github.com/wasmx/fizzy/pull/549)
- Code and test cleanups and refactoring. [#495](https://github.com/wasmx/fizzy/pull/495)
  [#505](https://github.com/wasmx/fizzy/pull/505) [#521](https://github.com/wasmx/fizzy/pull/521)
  [#522](https://github.com/wasmx/fizzy/pull/522)

### Fixed

- Incorrect results in some floating point operations with NaNs in GCC builds without optimization.
  [#513](https://github.com/wasmx/fizzy/pull/513)
- Potential undefined behaviour in pointer arithmetic in parser. [#539](https://github.com/wasmx/fizzy/pull/539)
- Ensure (in debug mode) that `execute()` receives the correct number of inputs and returns the correct number of output.
  [#547](https://github.com/wasmx/fizzy/pull/547)
- Incorrect failure logging in `fizzy-spectests` tool. [#548](https://github.com/wasmx/fizzy/pull/548)

## [0.4.0] — 2020-09-01

This release introduces complete floating-point support.

With that in place, Fizzy passes all(*) of the [official test suite (spectest 1.0)](https://github.com/wasmx/wasm-spec/tree/w3c-1.0-jsontests-20200813/test/core/json):
  - 17910 of 17913 binary parser and execution tests,
  - 989 of 989 validation tests,
  - 477 skipped due to testing text format parser.

(*) The three failures are caused by, in our opinion, two bugs in the test cases, and the last one
because we restrict memory to 256 Mb maximum during instantiation, while the test expects 4 Gb to succeed.

The addition of the new set of instructions causes changes to the execution times.
Effects of some changes are truly surprising, e.g.
[`67fd4c8`](https://github.com/wasmx/fizzy/commit/67fd4c8be8ff30477d4b1f51025ae5600ca4c35e#diff-47ed36245217950f22b14dcb2472adbaR60)
causes over -10% performance regression on GCC 10 by adding two bytes of unused code;
and [`bffe032`](https://github.com/wasmx/fizzy/commit/bffe032531933638232c5de883d787976c7ba10c),
which adds two new instructions, makes binaries built with Clang 10 much faster. In the end we decided
to start using Link-Time Optimized builds for benchmarking to mitigate some of these butterfly effects.
See also [README: Performance testing].

The performance regression of the execution time is +7% for GCC 10 and +2% for Clang 11.
This is expected due to increased code size of the interpreter loop (increased _iTLB-load-misses_
and _LLC-load-misses_ have been [observed](https://github.com/wasmx/fizzy/pull/510#issuecomment-679993073)).
We plan to mitigate the impact of the change in the future by using [Profile-Guided Optimization].

The performance of Wasm binary parsing and instantiation remains unchanged.

```
Comparing Fizzy 0.3.0 to 0.4.0 (Intel Haswell CPU 4.0 GHz, GCC 10, LTO)
Benchmark                                  CPU Time [µs]       Old       New
----------------------------------------------------------------------------

fizzy/execute/blake2b/512_bytes_rounds_1         +0.0857        78        84
fizzy/execute/blake2b/512_bytes_rounds_16        +0.0967      1164      1276
fizzy/execute/ecpairing/onepoint                 +0.0734    395683    424732
fizzy/execute/keccak256/512_bytes_rounds_1       +0.0457        94        98
fizzy/execute/keccak256/512_bytes_rounds_16      +0.0492      1374      1442
fizzy/execute/memset/256_bytes                   +0.1102         6         7
fizzy/execute/memset/60000_bytes                 +0.1106      1390      1544
fizzy/execute/mul256_opt0/input0                 +0.0205        25        26
fizzy/execute/mul256_opt0/input1                 +0.0198        25        26
fizzy/execute/sha1/512_bytes_rounds_1            +0.0477        86        90
fizzy/execute/sha1/512_bytes_rounds_16           +0.0467      1196      1252
fizzy/execute/sha256/512_bytes_rounds_1          +0.0885        83        91
fizzy/execute/sha256/512_bytes_rounds_16         +0.0951      1142      1250
fizzy/execute/micro/eli_interpreter/halt         +0.1457         0         0
fizzy/execute/micro/eli_interpreter/exec105      +0.1711         4         5
fizzy/execute/micro/factorial/10                 +0.0643         0         0
fizzy/execute/micro/factorial/20                 +0.0213         1         1
fizzy/execute/micro/fibonacci/24                 +0.0518      7131      7500
fizzy/execute/micro/host_adler32/1               +0.0242         0         0
fizzy/execute/micro/host_adler32/100             +0.0054         3         3
fizzy/execute/micro/host_adler32/1000            -0.0126        30        29
fizzy/execute/micro/spinner/1                    +0.0860         0         0
fizzy/execute/micro/spinner/1000                 +0.1976         8        10
```

### Added

- `f32.const`, `f64.const`, `f32.add`, `f64.add` instructions. [#424](https://github.com/wasmx/fizzy/pull/424) 
  [#467](https://github.com/wasmx/fizzy/pull/467)
- `f32.sub`, `f64.sub` instructions. [#474](https://github.com/wasmx/fizzy/pull/474)
- `f32.mul`, `f64.mul` instructions. [#473](https://github.com/wasmx/fizzy/pull/473)
- `f32.div`, `f64.div` instructions. [#468](https://github.com/wasmx/fizzy/pull/468)
- `f32.sqrt`, `f64.sqrt` instructions. [#480](https://github.com/wasmx/fizzy/pull/480)
- `f32.copysign`, `f64.copysign` instructions. [#471](https://github.com/wasmx/fizzy/pull/471)
- `f32.abs`, `f64.abs` instructions. [#476](https://github.com/wasmx/fizzy/pull/476)
- `f32.neg`, `f64.neg` instructions. [#477](https://github.com/wasmx/fizzy/pull/477)
- Floating-point comparison instructions: `f32.eq`, `f32.ne`, `f32.lt`, `f32.gt`, `f32.le`, `f64.ge`, `f64.eq`, 
   `f64.ne`, `f64.lt`, `f64.gt`, `f64.le`, `f64.ge`. [#449](https://github.com/wasmx/fizzy/pull/449)
- `f32.min`, `f32.max`, `f64.min`, `f64.max` instructions. [#472](https://github.com/wasmx/fizzy/pull/472)
- `f32.ceil`, `f32.floor`, `f32.trunc`, `f64.ceil`, `f64.floor`, `f64.trunc` instructions. 
  [#481](https://github.com/wasmx/fizzy/pull/481)
- `f32.nearest`, `f64.nearest` instructions. [#486](https://github.com/wasmx/fizzy/pull/486)
- Floating-point to integer truncation instructions: `i32.trunc_f32_s`, `i32.trunc_f32_u`, `i32.trunc_f64_s`, `i32.trunc_f64_u`, `i64.trunc_f32_s`, `i64.trunc_f32_u`, 
  `i64.trunc_f64_s`, `i64.trunc_f64_u`. [#447](https://github.com/wasmx/fizzy/pull/447)
- Integer to floating-point conversion instructions: `f32.convert_i32_s`, `f32.convert_i32_u`, `f32.convert_i64_s`, `f32.convert_i64_u`, 
  `f64.convert_i32_s`, `f64.convert_i32_u`, `f64.convert_i64_s`, `f64.convert_i64_u`. 
  [#455](https://github.com/wasmx/fizzy/pull/455)
- `f64.promote_f32` instruction. [#457](https://github.com/wasmx/fizzy/pull/457)
- `f32.demote_f64` instruction. [#458](https://github.com/wasmx/fizzy/pull/458)
- Reinterpret instructions: `i32.reinterpret_f32`, `i64.reinterpret_f64`, `f32.reinterpret_i32`, `f64.reinterpret_i64`. 
  [#459](https://github.com/wasmx/fizzy/pull/459)
- Floating-point memory instructions: `f32.load`, `f64.load`, `f32.store`, `f64.store`. [#456](https://github.com/wasmx/fizzy/pull/456)
- Floating-point globals support. [#446](https://github.com/wasmx/fizzy/pull/446)
- `fizzy-spectests` support for floating-point tests. [#445](https://github.com/wasmx/fizzy/pull/445)
  [#460](https://github.com/wasmx/fizzy/pull/460) [#484](https://github.com/wasmx/fizzy/pull/484)
- Comprehensive tests for all NaNs and all rounding directions. [#475](https://github.com/wasmx/fizzy/pull/475)
  [#487](https://github.com/wasmx/fizzy/pull/487) [#488](https://github.com/wasmx/fizzy/pull/488)
  [#500](https://github.com/wasmx/fizzy/pull/500)
- A single-precision floating-point benchmark approximating pi using Taylor's series. [#482](https://github.com/wasmx/fizzy/pull/482)
- A double-precision floating-point benchmark approximating pi using Ramanujan's algorithm. [#483](https://github.com/wasmx/fizzy/pull/483)

### Changed
- Execution API now uses union-based `Value` type instead of `uint64_t `to represent values of all supported types. 
  [#419](https://github.com/wasmx/fizzy/pull/419) [#423](https://github.com/wasmx/fizzy/pull/423) 
  [#448](https://github.com/wasmx/fizzy/pull/448) [#462](https://github.com/wasmx/fizzy/pull/462)
  [#464](https://github.com/wasmx/fizzy/pull/464)
- `fizzy-spectests` output can be configured for various verbosity. [#453](https://github.com/wasmx/fizzy/pull/453)
- `ExternalGlobal` now includes type of value, which is checked against module definition for imported globals. 
  [#493](https://github.com/wasmx/fizzy/pull/493) 
- Mark `execute()` completely exception free (`noexcept`). [#438](https://github.com/wasmx/fizzy/pull/438)


## [0.3.0] — 2020-07-29

This main focus for this release is to implement every WebAssembly validation rule from the specification.

It passes a large part of the [official test suite (spectest 1.0)](https://github.com/wasmx/wasm-spec/tree/w3c-1.0-jsontests-20200313/test/core/json):
  - 4481 of 4490 binary parser and execution tests,
  - 942 of 942 validation tests,
  - 6381 skipped due to containing floating-point instructions or testing text format parser.

Additionally, measurable speed improvements were made for branching and function calls.

#### Performance comparison to v0.2.0

The time of WebAssembly module instantiation (including binary loading and verification) has almost doubled, while the execution is 10% faster in heavy cases.

##### Detailed benchmark results

```
Comparing Fizzy 0.2.0 to 0.3.0 (Intel Haswell CPU, 4.0 GHz)
Benchmark                                  CPU Time [µs]       Old       New
----------------------------------------------------------------------------

fizzy/parse/blake2b                              +0.5960        14        23
fizzy/instantiate/blake2b                        +0.4412        19        28
fizzy/parse/ecpairing                            +0.8076       760      1373
fizzy/instantiate/ecpairing                      +0.7055       828      1412
fizzy/parse/keccak256                            +0.6071        26        42
fizzy/instantiate/keccak256                      +0.4818        32        47
fizzy/parse/memset                               +0.3710         4         6
fizzy/instantiate/memset                         +0.2013         8        10
fizzy/parse/mul256_opt0                          +0.6683         5         8
fizzy/instantiate/mul256_opt0                    +0.3370         9        12
fizzy/parse/sha1                                 +0.6466        23        38
fizzy/instantiate/sha1                           +0.4696        29        43
fizzy/parse/sha256                               +0.7765        36        63
fizzy/instantiate/sha256                         +0.6920        42        70
fizzy/parse/micro/eli_interpreter                +0.2003         3         4
fizzy/instantiate/micro/eli_interpreter          +0.0935         7         8
fizzy/parse/micro/factorial                      +0.2105         1         1
fizzy/instantiate/micro/factorial                +0.1369         1         1
fizzy/parse/micro/fibonacci                      +0.2773         1         1
fizzy/instantiate/micro/fibonacci                +0.1888         1         2
fizzy/parse/micro/host_adler32                   +0.1448         1         2
fizzy/instantiate/micro/host_adler32             +0.0674         4         4
fizzy/parse/micro/spinner                        +0.1232         1         1
fizzy/instantiate/micro/spinner                  +0.0774         1         1

fizzy/execute/blake2b/512_bytes_rounds_1         -0.0250        86        83
fizzy/execute/blake2b/512_bytes_rounds_16        -0.0197      1294      1268
fizzy/execute/ecpairing/onepoint                 -0.1027    479448    430202
fizzy/execute/keccak256/512_bytes_rounds_1       -0.0422       104       100
fizzy/execute/keccak256/512_bytes_rounds_16      -0.0352      1512      1458
fizzy/execute/memset/256_bytes                   -0.0318         7         7
fizzy/execute/memset/60000_bytes                 -0.0109      1609      1592
fizzy/execute/mul256_opt0/input0                 -0.0254        27        26
fizzy/execute/mul256_opt0/input1                 -0.0262        27        26
fizzy/execute/sha1/512_bytes_rounds_1            -0.0452        95        90
fizzy/execute/sha1/512_bytes_rounds_16           -0.0478      1318      1255
fizzy/execute/sha256/512_bytes_rounds_1          +0.0446        93        97
fizzy/execute/sha256/512_bytes_rounds_16         +0.0457      1280      1338
fizzy/execute/micro/eli_interpreter/halt         -0.3601         0         0
fizzy/execute/micro/eli_interpreter/exec105      -0.0299         5         5
fizzy/execute/micro/factorial/10                 -0.2383         1         0
fizzy/execute/micro/factorial/20                 -0.2363         1         1
fizzy/execute/micro/fibonacci/24                 -0.2760     10273      7438
fizzy/execute/micro/host_adler32/1               -0.5324         0         0
fizzy/execute/micro/host_adler32/100             -0.5275         6         3
fizzy/execute/micro/host_adler32/1000            -0.5303        63        30
fizzy/execute/micro/spinner/1                    -0.0677         0         0
fizzy/execute/micro/spinner/1000                 -0.0256        10        10
```

### Added

- Type validation of instructions. [#403](https://github.com/wasmx/fizzy/pull/403) 
  [#404](https://github.com/wasmx/fizzy/pull/404) [#411](https://github.com/wasmx/fizzy/pull/411)
  [#414](https://github.com/wasmx/fizzy/pull/414) [#415](https://github.com/wasmx/fizzy/pull/415)
- Type validation of block results. [#407](https://github.com/wasmx/fizzy/pull/407)
  [#429](https://github.com/wasmx/fizzy/pull/429)
- Type validation of branch instructions. [#408](https://github.com/wasmx/fizzy/pull/408)
- Type validation of select instruction. [#413](https://github.com/wasmx/fizzy/pull/413)
- Type validation in constant expressions. [#430](https://github.com/wasmx/fizzy/pull/430)
- Validation of `else` branch in `if` instruction. [#417](https://github.com/wasmx/fizzy/pull/417)
- Validation of frame types in `br_table` instruction. [#427](https://github.com/wasmx/fizzy/pull/427)
- Validation of function indices in element section. [#432](https://github.com/wasmx/fizzy/pull/432)
- Validation of functions' arity. [#433](https://github.com/wasmx/fizzy/pull/433)
- Validation of alignment for floating point memory instructions. [#437](https://github.com/wasmx/fizzy/pull/437)
- CI now includes compiling and testing with C++20 standard. [#405](https://github.com/wasmx/fizzy/pull/405)

### Changed

- API cleanup (`execute` function overload taking `Module` is used only internally).
  [#409](https://github.com/wasmx/fizzy/pull/409)
- `execution_result` changed to returning single value instead of the entire stack and renamed to `ExecutionResult`. 
  [#219](https://github.com/wasmx/fizzy/pull/219) [#428](https://github.com/wasmx/fizzy/pull/428)
- Optimization for branch instructions. [#354](https://github.com/wasmx/fizzy/pull/354)
- Optimization of passing arguments to functions (eliminate extra copy). `execute` function now takes the arguments as 
  `span<const uint64_t>`. [#359](https://github.com/wasmx/fizzy/pull/359) [#404](https://github.com/wasmx/fizzy/pull/404)  
- Better unit test coverage. [#381](https://github.com/wasmx/fizzy/pull/381) [#410](https://github.com/wasmx/fizzy/pull/410)
  [#431](https://github.com/wasmx/fizzy/pull/431) [#435](https://github.com/wasmx/fizzy/pull/435) 
  [#444](https://github.com/wasmx/fizzy/pull/444)

### Fixed

- `fizzy-bench` correctly handles errors during instantiation in wasm3. 
  [#381](https://github.com/wasmx/fizzy/pull/381) 


## [0.2.0] — 2020-06-29

Firstly, this release implements many validation steps prescribed by the specification, with the exception of type checking.

It passes a large part of the [official test suite (spectest 1.0)](https://github.com/wasmx/wasm-spec/tree/w3c-1.0-jsontests-20200313/test/core/json):
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
- `fizzy-bench` executes the start function on all engines (previously it did not on WABT).
  [#332](https://github.com/wasmx/fizzy/pull/332)
  [#349](https://github.com/wasmx/fizzy/pull/349) 
- `fizzy-bench` correctly handles errors during validation phase. [#340](https://github.com/wasmx/fizzy/pull/340)


## [0.1.0] — 2020-05-14

First release!

### Added

- Supports all instructions with the exception of floating-point ones.
- Passes a large part of the [official test suite (spectest 1.0)](https://github.com/wasmx/wasm-spec/tree/w3c-1.0-jsontests-20200313/test/core/json):
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
[0.3.0]: https://github.com/wasmx/fizzy/releases/tag/v0.3.0
[0.4.0]: https://github.com/wasmx/fizzy/releases/tag/v0.4.0
[0.5.0]: https://github.com/wasmx/fizzy/compare/v0.4.0...master

[Keep a Changelog]: https://keepachangelog.com/en/1.0.0/
[Semantic Versioning]: https://semver.org
[Profile-Guided Optimization]: https://en.wikipedia.org/wiki/Profile-guided_optimization
[README: Performance Testing]: ./README.md#Performance-testing
