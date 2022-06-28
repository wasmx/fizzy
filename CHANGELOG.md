# Changelog

Documentation of all notable changes to the **Fizzy** project.

The format is based on [Keep a Changelog],
and this project adheres to [Semantic Versioning].

## [0.8.0] - 2022-06-28

With this release we are introducing support for runtime metering (i.e. deterministic execution). This is only available via the C API.

Additionally we have made a lot of improvements to both the C and Rust APIs.

Fizzy passes all the official WebAssembly 1.0 tests. We are maintaining the WebAssembly 1.0 test suite
with corrections and additions backported from the WebAssembly specification master branch. For this
Fizzy release [the snapshot from 2022-06-23](https://github.com/wasmx/wasm-spec/tree/w3c-1.0-tests-backported-20220623/test/core) is used:
- 19062 of 19062 binary parser and execution tests,
- 1049 of 1049 validation tests,
- 499 skipped due to testing text format parser.

Comparing with the previous release, the performance of instantiation remains unchanged (within 1% performance regression). 
However, we were able to increase execution performance by around **7%**.

<details><summary>Detailed benchmark results</summary>

```
Comparing Fizzy 0.7.0 to 0.8.0 (Intel Haswell CPU 4.4 GHz, Clang 14)
Benchmark                                  CPU Time [µs]       Old       New
----------------------------------------------------------------------------
fizzy/parse/blake2b                              +0.0142        23        23
fizzy/parse/ecpairing                            +0.0082      1313      1324
fizzy/parse/keccak256                            +0.0075        42        42
fizzy/parse/memset                               -0.0004         6         6
fizzy/parse/mul256_opt0                          -0.0056         7         7
fizzy/parse/ramanujan_pi                         +0.0023        24        24
fizzy/parse/sha1                                 +0.0065        38        38
fizzy/parse/sha256                               +0.0052        63        63
fizzy/parse/taylor_pi                            -0.0070         2         2
GEOMETRIC MEAN                                   +0.0034

fizzy/instantiate/blake2b                        +0.0141        26        26
fizzy/instantiate/ecpairing                      +0.0156      1363      1385
fizzy/instantiate/keccak256                      +0.0105        46        46
fizzy/instantiate/memset                         +0.0049         9         9
fizzy/instantiate/mul256_opt0                    +0.0084        11        11
fizzy/instantiate/ramanujan_pi                   +0.0082        27        27
fizzy/instantiate/sha1                           +0.0031        42        42
fizzy/instantiate/sha256                         +0.0079        66        67
fizzy/instantiate/taylor_pi                      +0.0075         6         6
GEOMETRIC MEAN                                   +0.0089

fizzy/execute/blake2b/512_bytes_rounds_1         -0.0976        76        68
fizzy/execute/blake2b/512_bytes_rounds_16        -0.1006      1149      1033
fizzy/execute/ecpairing/onepoint                 -0.0600    366609    344611
fizzy/execute/keccak256/512_bytes_rounds_1       -0.0594        89        84
fizzy/execute/keccak256/512_bytes_rounds_16      -0.0477      1291      1230
fizzy/execute/memset/256_bytes                   -0.0972         6         6
fizzy/execute/memset/60000_bytes                 -0.0944      1395      1263
fizzy/execute/mul256_opt0/input1                 -0.1092        25        23
fizzy/execute/ramanujan_pi/33_runs               -0.1095       103        92
fizzy/execute/sha1/512_bytes_rounds_1            -0.1069        84        75
fizzy/execute/sha1/512_bytes_rounds_16           -0.1071      1174      1048
fizzy/execute/sha256/512_bytes_rounds_1          -0.0275        77        75
fizzy/execute/sha256/512_bytes_rounds_16         -0.0375      1062      1023
fizzy/execute/taylor_pi/pi_1000000_runs          +0.0016     37519     37581
GEOMETRIC MEAN                                   -0.0759


Comparing Fizzy 0.7.0 to 0.8.0 (Intel Haswell CPU 4.4 GHz, GCC 12)
Benchmark                                  CPU Time [µs]       Old       New
----------------------------------------------------------------------------
fizzy/parse/blake2b                              +0.0046        22        22
fizzy/parse/ecpairing                            +0.0059      1296      1304
fizzy/parse/keccak256                            +0.0091        41        42
fizzy/parse/memset                               +0.0036         5         5
fizzy/parse/mul256_opt0                          +0.0041         7         7
fizzy/parse/ramanujan_pi                         -0.0008        23        23
fizzy/parse/sha1                                 +0.0006        37        37
fizzy/parse/sha256                               +0.0090        61        62
fizzy/parse/taylor_pi                            -0.0106         2         2
GEOMETRIC MEAN                                   +0.0028

fizzy/instantiate/blake2b                        +0.0068        26        26
fizzy/instantiate/ecpairing                      +0.0063      1348      1357
fizzy/instantiate/keccak256                      +0.0019        45        45
fizzy/instantiate/memset                         +0.0053         9         9
fizzy/instantiate/mul256_opt0                    -0.0019        11        11
fizzy/instantiate/ramanujan_pi                   -0.0010        27        27
fizzy/instantiate/sha1                           +0.0085        41        41
fizzy/instantiate/sha256                         +0.0019        65        66
fizzy/instantiate/taylor_pi                      -0.0019         6         6
GEOMETRIC MEAN                                   +0.0029

fizzy/execute/blake2b/512_bytes_rounds_1         -0.0474        71        68
fizzy/execute/blake2b/512_bytes_rounds_16        -0.0463      1076      1026
fizzy/execute/ecpairing/onepoint                 -0.0192    338070    331572
fizzy/execute/keccak256/512_bytes_rounds_1       -0.1336        76        66
fizzy/execute/keccak256/512_bytes_rounds_16      -0.1376      1116       962
fizzy/execute/memset/256_bytes                   -0.0681         6         6
fizzy/execute/memset/60000_bytes                 -0.0720      1371      1272
fizzy/execute/mul256_opt0/input1                 +0.0030        24        24
fizzy/execute/ramanujan_pi/33_runs               -0.1126       102        91
fizzy/execute/sha1/512_bytes_rounds_1            -0.0990        79        71
fizzy/execute/sha1/512_bytes_rounds_16           -0.1016      1106       994
fizzy/execute/sha256/512_bytes_rounds_1          -0.0810        78        72
fizzy/execute/sha256/512_bytes_rounds_16         -0.0803      1074       988
fizzy/execute/taylor_pi/pi_1000000_runs          +0.0003     38233     38244
GEOMETRIC MEAN                                   -0.0721
```

</details>

### Added

- Runtime metering support. [#626](https://github.com/wasmx/fizzy/pull/626)
- Doxygen documentation. [#692](https://github.com/wasmx/fizzy/pull/692)
- Unit tests for WASI implementation. [#713](https://github.com/wasmx/fizzy/pull/713) 
  [#790](https://github.com/wasmx/fizzy/pull/790) [#792](https://github.com/wasmx/fizzy/pull/792)
  [#796](https://github.com/wasmx/fizzy/pull/796)
- Support building on 32-bit platforms. [#745](https://github.com/wasmx/fizzy/pull/745)
- Floating point support on 32-bit platforms requires to use SSE2 instructions. [#785](https://github.com/wasmx/fizzy/pull/785)
- Check that limits of imported table/memory do not have min above max. [#762](https://github.com/wasmx/fizzy/pull/762)
- Minor documentation improvements. [#763](https://github.com/wasmx/fizzy/pull/763)
- Unit tests for out of memory errors. [#773](https://github.com/wasmx/fizzy/pull/773)
- README clarifications. [#823](https://github.com/wasmx/fizzy/pull/823) [#835](https://github.com/wasmx/fizzy/pull/835)
- Public C API:
  - Detailed error reporting (error code and message). [#772](https://github.com/wasmx/fizzy/pull/772)
    [#783](https://github.com/wasmx/fizzy/pull/783) [#784](https://github.com/wasmx/fizzy/pull/784)
    [#788](https://github.com/wasmx/fizzy/pull/788)
  - Support for custom hard memory limit in C API. [#780](https://github.com/wasmx/fizzy/pull/780)
  - Access to `ExecutionContext` members, including metering and call depth. [#799](https://github.com/wasmx/fizzy/pull/799)
  - Static assertions to check consistency of C value type constants with C++ `fizzy:ValType`.
    [#821](https://github.com/wasmx/fizzy/pull/821)
- Rust bindings: 
  - Detailed error reporting. [#735](https://github.com/wasmx/fizzy/pull/735) [#769](https://github.com/wasmx/fizzy/pull/769)
    [#772](https://github.com/wasmx/fizzy/pull/772) [#781](https://github.com/wasmx/fizzy/pull/781)
    [#824](https://github.com/wasmx/fizzy/pull/824)
  - Introduce `ConstNotNull` wrapper. [#720](https://github.com/wasmx/fizzy/pull/720)
  - Test additions. [#822](https://github.com/wasmx/fizzy/pull/822) [#825](https://github.com/wasmx/fizzy/pull/825)

### Changed

- Use C++20 bit-manipulating functions instead of intrinsics if available. [#542](https://github.com/wasmx/fizzy/pull/542)
  [#689](https://github.com/wasmx/fizzy/pull/689)
- Minor floating point instruction optimization. [#592](https://github.com/wasmx/fizzy/pull/592)
- Hard memory limit is capped at 4 GB. [#748](https://github.com/wasmx/fizzy/pull/748)
- C++ API:
  - Refactoring to make `execute()` not throwing exceptions. [#716](https://github.com/wasmx/fizzy/pull/716)
    [#738](https://github.com/wasmx/fizzy/pull/738)
  - Introduce `ExecutionContext` instead of `depth` parameter to `execute()`. [#756](https://github.com/wasmx/fizzy/pull/756)
    [#757](https://github.com/wasmx/fizzy/pull/757) [#761](https://github.com/wasmx/fizzy/pull/761)
    [#779](https://github.com/wasmx/fizzy/pull/779)
    [#766](https://github.com/wasmx/fizzy/pull/766)
  - Other minor improvements. [#746](https://github.com/wasmx/fizzy/pull/746) 
    [#776](https://github.com/wasmx/fizzy/pull/776)
- Public C API:  
  - Functions marked with `noexcept`. [#775](https://github.com/wasmx/fizzy/pull/775)
  - Unit tests are split into several files. [#800](https://github.com/wasmx/fizzy/pull/800)
- Rust API: minor stylistic improvements. [#815](https://github.com/wasmx/fizzy/pull/815)
- Floating point instruction tests fixes and improvements. [#786](https://github.com/wasmx/fizzy/pull/786)
  [#793](https://github.com/wasmx/fizzy/pull/793) [#795](https://github.com/wasmx/fizzy/pull/795)
- Other unit test improvements. [#787](https://github.com/wasmx/fizzy/pull/787) 
  [#830](https://github.com/wasmx/fizzy/pull/830)
- CMake files optimizations and refactorings. [#770](https://github.com/wasmx/fizzy/pull/770)
  [#837](https://github.com/wasmx/fizzy/pull/837)
- CI fixes and improvements. [#774](https://github.com/wasmx/fizzy/pull/774)
  [#810](https://github.com/wasmx/fizzy/pull/810) [#814](https://github.com/wasmx/fizzy/pull/814)
  [#818](https://github.com/wasmx/fizzy/pull/818) [#827](https://github.com/wasmx/fizzy/pull/827) 
  [#829](https://github.com/wasmx/fizzy/pull/829) [#831](https://github.com/wasmx/fizzy/pull/831)
  [#832](https://github.com/wasmx/fizzy/pull/832) [#834](https://github.com/wasmx/fizzy/pull/834)
  [#839](https://github.com/wasmx/fizzy/pull/839)
- Dependency upgrades. [#798](https://github.com/wasmx/fizzy/pull/798) [#802](https://github.com/wasmx/fizzy/pull/802)
  [#804](https://github.com/wasmx/fizzy/pull/804)
- Updated WebAssembly spec test suite. [#836](https://github.com/wasmx/fizzy/pull/836)

### Fixed

- Incorrect types in parser leading to build issues on 32-bit architectures. [#744](https://github.com/wasmx/fizzy/pull/744)
- Validating that imported memory is multiple of page size. [#749](https://github.com/wasmx/fizzy/pull/749)
- UTF-8 validation on 32-bit architectures and dead store warning in implementation. [#752](https://github.com/wasmx/fizzy/pull/752)
- Memory allocation and `memory.grow` integer overflow fixes. [#747](https://github.com/wasmx/fizzy/pull/747)
  [#808](https://github.com/wasmx/fizzy/pull/808) [#809](https://github.com/wasmx/fizzy/pull/809)
- Exported memory min limit must be equal to current memory size. [#755](https://github.com/wasmx/fizzy/pull/755)
- CMake error when including Fizzy into a project as a subdirectory. [#758](https:/github.com/wasmx/fizzy/pull/758)
- Potential Undefined Behaviour in interpreter loop. [#813](https://github.com/wasmx/fizzy/pull/813)

## [0.7.0] — 2021-03-01

With this release we aim to provide a much improved C and Rust API, including a clear separation of i32 and i64 types.

Fizzy passes all of the official WebAssembly 1.0 tests. We are maintaining the WebAssembly 1.0 test suite
with corrections and additions backported from the WebAssembly specification master branch. For this
Fizzy release [the snapshot from 2021-02-12](https://github.com/wasmx/wasm-spec/tree/w3c-1.0-tests-backported-20210212/test/core) is used:
- 19044 of 19044 binary parser and execution tests,
- 1049 of 1049 validation tests,
- 499 skipped due to testing text format parser.

There are no performance changes expected nor observed in this release.
With one exception: the `i32` member added to the `Value` union causes machine code being generated with different layout.
This may impact the execution performance depending on the compiler and build configuration.
In some isolated cases differences up to ±30% were observed.

### Added

- `Value` union now has a separate `i32` member instead of putting 32-bit integers into `i64`.
  [#517](https://github.com/wasmx/fizzy/pull/517) [#702](https://github.com/wasmx/fizzy/pull/702)
- New function to resolve imported globals by name similar to `resolve_imported_functions`.
  [#637](https://github.com/wasmx/fizzy/pull/637) [#697](https://github.com/wasmx/fizzy/pull/697)
- Public C API:
  - Inspecting module's type and global definitions. [#675](https://github.com/wasmx/fizzy/pull/675)
  - Check if module has table, memory, or start function.
  [#684](https://github.com/wasmx/fizzy/pull/684) [#685](https://github.com/wasmx/fizzy/pull/685)
  - Inspecting module's import definitions. [#683](https://github.com/wasmx/fizzy/pull/683)
  - Inspecting module's export definitions. [#686](https://github.com/wasmx/fizzy/pull/686)
  - `fizzy_resolve_instantiate` now resolves imported globals besides functions.
    [#660](https://github.com/wasmx/fizzy/pull/660)
  - Example of usage added to [README.md](./README.md#building-and-using).
    [#682](https://github.com/wasmx/fizzy/pull/682)
- Rust bindings:
  - Memory manipulation functions added. [#609](https://github.com/wasmx/fizzy/pull/609)
  - Safe `execute` function with typed value and execution added. It performs type checking on function and passed arguments.
    [#652](https://github.com/wasmx/fizzy/pull/652) [#705](https://github.com/wasmx/fizzy/pull/705)
    [#725](https://github.com/wasmx/fizzy/pull/725)
  - `Module` can be cloned. [#719](https://github.com/wasmx/fizzy/pull/719)
  - Example of usage added. [#724](https://github.com/wasmx/fizzy/pull/724)
  - Errors are returned as string error messages.[#743](https://github.com/wasmx/fizzy/pull/743)
- Doxygen config added and documentation generated on CI. [#692](https://github.com/wasmx/fizzy/pull/692)
  [#703](https://github.com/wasmx/fizzy/pull/703)

### Changed

- Changed maximum call depth level to 2047. [#669](https://github.com/wasmx/fizzy/pull/669)
- `fizzy::ExternalFunction` now uses `span` type to represent input and output types of a function.
  [#668](https://github.com/wasmx/fizzy/pull/668)
- C API minor optimizations. [#699](https://github.com/wasmx/fizzy/pull/699)
- Rust bindings build and CI improvements. [#706](https://github.com/wasmx/fizzy/pull/706)
  [#717](https://github.com/wasmx/fizzy/pull/717) [#731](https://github.com/wasmx/fizzy/pull/731)
- Support building for ARM architecture and test it on CI. [#714](https://github.com/wasmx/fizzy/pull/714)
- Unit tests are now checking the types of arguments passed to `execute()` and the type of returned result.
  [#655](https://github.com/wasmx/fizzy/pull/655) [#659](https://github.com/wasmx/fizzy/pull/659)
  [#687](https://github.com/wasmx/fizzy/pull/687)
- Other unit test additions and improvements. [#648](https://github.com/wasmx/fizzy/pull/648)
  [#679](https://github.com/wasmx/fizzy/pull/679) [#680](https://github.com/wasmx/fizzy/pull/680)
  [#688](https://github.com/wasmx/fizzy/pull/688) [#701](https://github.com/wasmx/fizzy/pull/701)
  [#711](https://github.com/wasmx/fizzy/pull/711) [#712](https://github.com/wasmx/fizzy/pull/712)
  [#732](https://github.com/wasmx/fizzy/pull/732) [#733](https://github.com/wasmx/fizzy/pull/733)
  [#742](https://github.com/wasmx/fizzy/pull/742)
- Test utils improvements. [#651](https://github.com/wasmx/fizzy/pull/651)
  [#691](https://github.com/wasmx/fizzy/pull/691) [#695](https://github.com/wasmx/fizzy/pull/695)
  [#704](https://github.com/wasmx/fizzy/pull/704) [#715](https://github.com/wasmx/fizzy/pull/715)
  [#718](https://github.com/wasmx/fizzy/pull/718) [#721](https://github.com/wasmx/fizzy/pull/721)
  [#741](https://github.com/wasmx/fizzy/pull/741)
- Updated WebAssembly spec test suite. [#690](https://github.com/wasmx/fizzy/pull/690)
- Documentation comments improvements. [#681](https://github.com/wasmx/fizzy/pull/681)
  [#707](https://github.com/wasmx/fizzy/pull/707)
- Hunter dependencies updated. [#736](https://github.com/wasmx/fizzy/pull/736)
- uvwasi library repository address updated. [#693](https://github.com/wasmx/fizzy/pull/693)

### Fixed

- Potential undefined behaviour for functions without locals. [#630](https://github.com/wasmx/fizzy/pull/630)
- Make sure that `memory.grow` implementation doesn't throw exceptions. [#737](https://github.com/wasmx/fizzy/pull/737)

## [0.6.0] — 2020-12-24

With this release we focus on introducing three major features:
- a public C API,
- an initial version of Rust binding,
- partial support for WASI (via the `fizzy-wasi` tool).

All these features are work-in-progress with differing levels of completeness. More progress
to be made in the next release.

Fizzy passes all the official WebAssembly 1.0 tests. We are maintaining the WebAssembly 1.0 test suite
with corrections and additions backported from the WebAssembly specification master branch. For this
Fizzy release [the snapshot from 2020-11-13](https://github.com/wasmx/wasm-spec/tree/w3c-1.0-tests-backported-20201113/test/core) is used:
  - 18979 of 18979 binary parser and execution tests,
  - 999 of 999 validation tests,
  - 499 skipped due to testing text format parser.

We continued working on performance improvements. Worth noting, the internal program representation
has been changed to favour execution time (8% faster) instead of instantiation time (9% slower).

<details><summary>Detailed benchmark results</summary>

```
Comparing Fizzy 0.5.0 to 0.6.0 (Intel Haswell CPU 4.0 GHz, GCC 10, LTO)
Benchmark                                  CPU Time [µs]       Old       New
----------------------------------------------------------------------------


fizzy/parse/blake2b                              +0.1726        23        27
fizzy/instantiate/blake2b                        +0.1986        27        33
fizzy/execute/blake2b/512_bytes_rounds_1         -0.0506        82        77
fizzy/execute/blake2b/512_bytes_rounds_16        -0.0971      1237      1117
fizzy/parse/ecpairing                            +0.1519      1335      1538
fizzy/instantiate/ecpairing                      +0.1237      1414      1589
fizzy/execute/ecpairing/onepoint                 -0.0637    373103    349353
fizzy/parse/keccak256                            +0.1373        43        48
fizzy/instantiate/keccak256                      +0.1022        48        53
fizzy/execute/keccak256/512_bytes_rounds_1       -0.0183        92        91
fizzy/execute/keccak256/512_bytes_rounds_16      -0.0310      1355      1313
fizzy/parse/memset                               +0.1286         6         6
fizzy/instantiate/memset                         +0.0223        10        10
fizzy/execute/memset/256_bytes                   -0.0969         6         6
fizzy/execute/memset/60000_bytes                 -0.0847      1396      1278
fizzy/parse/mul256_opt0                          +0.1449         8         9
fizzy/instantiate/mul256_opt0                    +0.0724        12        13
fizzy/execute/mul256_opt0/input1                 -0.0668        25        23
fizzy/parse/ramanujan_pi                         +0.1732        23        27
fizzy/instantiate/ramanujan_pi                   +0.1099        28        31
fizzy/execute/ramanujan_pi/33_runs               -0.1707       116        96
fizzy/parse/sha1                                 +0.1457        38        44
fizzy/instantiate/sha1                           +0.1198        43        48
fizzy/execute/sha1/512_bytes_rounds_1            -0.0791        84        78
fizzy/execute/sha1/512_bytes_rounds_16           -0.0727      1172      1087
fizzy/parse/sha256                               +0.1235        64        72
fizzy/instantiate/sha256                         +0.0967        70        77
fizzy/execute/sha256/512_bytes_rounds_1          -0.0804        85        78
fizzy/execute/sha256/512_bytes_rounds_16         -0.0764      1169      1080
fizzy/parse/taylor_pi                            +0.0569         2         3
fizzy/instantiate/taylor_pi                      -0.0291         7         6
fizzy/execute/taylor_pi/pi_1000000_runs          -0.0805     40094     36867
fizzy/parse/micro/eli_interpreter                +0.0734         4         4
fizzy/instantiate/micro/eli_interpreter          -0.0023         8         8
fizzy/execute/micro/eli_interpreter/exec105      -0.0513         4         4
fizzy/parse/micro/factorial                      -0.0070         1         1
fizzy/instantiate/micro/factorial                -0.1451         1         1
fizzy/execute/micro/factorial/20                 +0.0159         1         1
fizzy/parse/micro/fibonacci                      -0.0011         1         1
fizzy/instantiate/micro/fibonacci                -0.1177         2         1
fizzy/execute/micro/fibonacci/24                 +0.0163      4912      4992
fizzy/parse/micro/host_adler32                   +0.0457         2         2
fizzy/instantiate/micro/host_adler32             -0.1045         4         4
fizzy/execute/micro/host_adler32/1               +0.0094         0         0
fizzy/execute/micro/host_adler32/1000            -0.0178        29        28
fizzy/parse/micro/icall_hash                     +0.0178         3         3
fizzy/instantiate/micro/icall_hash               -0.0945         8         7
fizzy/execute/micro/icall_hash/1000_steps        -0.3885        98        60
fizzy/parse/micro/spinner                        +0.0124         1         1
fizzy/instantiate/micro/spinner                  -0.1366         1         1
fizzy/execute/micro/spinner/1                    -0.0931         0         0
fizzy/execute/micro/spinner/1000                 -0.1721         9         7
fizzy/parse/stress/guido-fuzzer-find-1           +0.1121       125       139
fizzy/instantiate/stress/guido-fuzzer-find-1     +0.0784       156       168
```
</details>

Note that in previous releases there was unnecessary copying of the module data during benchmarking instantiation 
by Fizzy, and [eliminating it](https://github.com/wasmx/fizzy/pull/581) in this release by itself resulted in some 
improvement in the instantiation performance as measured by the `fizzy-bench` tool. 

### Added

- `fizzy-wasi` command-line tool to execute Wasm code using WASI interface. (WASI support is not complete yet, but can 
  be easily extended.) [#329](https://github.com/wasmx/fizzy/pull/329)
- Public C API:
    - Validating a module. [#530](https://github.com/wasmx/fizzy/pull/530)
    - Parsing, instantiation and execution. [#576](https://github.com/wasmx/fizzy/pull/576) 
      [#624](https://github.com/wasmx/fizzy/pull/624) [#635](https://github.com/wasmx/fizzy/pull/635)
      [#664](https://github.com/wasmx/fizzy/pull/664) [#676](https://github.com/wasmx/fizzy/pull/676)
    - Accessing memory of an instance. [#596](https://github.com/wasmx/fizzy/pull/596) 
      [#665](https://github.com/wasmx/fizzy/pull/665)
    - Finding exports by name. [#601](https://github.com/wasmx/fizzy/pull/601) 
      [#627](https://github.com/wasmx/fizzy/pull/627) [#628](https://github.com/wasmx/fizzy/pull/628) 
      [#631](https://github.com/wasmx/fizzy/pull/631) [#633](https://github.com/wasmx/fizzy/pull/633)
    - Getting type of a function. [#557](https://github.com/wasmx/fizzy/pull/557)
    - Getting module of an instance. [#599](https://github.com/wasmx/fizzy/pull/599)
    - Cloning a module. [#674](https://github.com/wasmx/fizzy/pull/674)
    - Instantiation with resolving imported functions by names. [#608](https://github.com/wasmx/fizzy/pull/608)
    - `fizzy-bench` now includes `fizzyc` engine to benchmark Fizzy with C API. 
      [#561](https://github.com/wasmx/fizzy/pull/561)
- Rust bindings:
    - Building and crates.io package. [#584](https://github.com/wasmx/fizzy/pull/584) 
      [#585](https://github.com/wasmx/fizzy/pull/585) [#586](https://github.com/wasmx/fizzy/pull/586) 
      [#587](https://github.com/wasmx/fizzy/pull/587) [#589](https://github.com/wasmx/fizzy/pull/589)
      [#591](https://github.com/wasmx/fizzy/pull/591) [#595](https://github.com/wasmx/fizzy/pull/595)
    - Validating a module. [#538](https://github.com/wasmx/fizzy/pull/538)
    - Parsing, instantiation, (unsafe) execution. [#566](https://github.com/wasmx/fizzy/pull/566) 
- CMake Package for easy Fizzy integration. [#553](https://github.com/wasmx/fizzy/pull/553)
- New benchmarks. [#234](https://github.com/wasmx/fizzy/pull/234) [#632](https://github.com/wasmx/fizzy/pull/632)
- Support for building with libc++ on Linux by using CMake toolchain file. 
  [#597](https://github.com/wasmx/fizzy/pull/597)
- `fizzy-spectests` logs error message for passed tests that expect an error 
  [#672](https://github.com/wasmx/fizzy/pull/672)

### Changed

- Optimization in `execute()`: now arguments are passed by a pointer (`const Value*`) instead of `span<const Value>`.
  [#552](https://github.com/wasmx/fizzy/pull/552) [#573](https://github.com/wasmx/fizzy/pull/573) 
  [#574](https://github.com/wasmx/fizzy/pull/574)
- Optimization in `fabs`, `fneg`, `fcopysign` instructions implementation. 
  [#560](https://github.com/wasmx/fizzy/pull/560)
- `popcnt` instructions implementation now uses standard library function when built with C++20. 
  [#551](https://github.com/wasmx/fizzy/pull/551) 
- Optimization to store instruction immediate arguments together with instructions in a single array. 
  [#618](https://github.com/wasmx/fizzy/pull/618) [#620](https://github.com/wasmx/fizzy/pull/620)
- Parsing now allocates `Module` dynamically and `std::unique_ptr<const Module>` is used in the API to represent a 
  module. [#575](https://github.com/wasmx/fizzy/pull/575) 
- Refactoring and simplification of table representation. [#616](https://github.com/wasmx/fizzy/pull/616)
- `execute()` variant taking arguments as `std::initializer_list<Value>` is moved to test utils. 
  [#657](https://github.com/wasmx/fizzy/pull/657)
- CMake project restructuring: `fizzy::fizzy` library now includes only public headers, only `fizzy::fizzy-internal` 
  provides access to internal headers. [#550](https://github.com/wasmx/fizzy/pull/550)
- CI improvements. [#559](https://github.com/wasmx/fizzy/pull/559) [#590](https://github.com/wasmx/fizzy/pull/590)
  [#598](https://github.com/wasmx/fizzy/pull/598) [#600](https://github.com/wasmx/fizzy/pull/600) 
  [#603](https://github.com/wasmx/fizzy/pull/603) [#605](https://github.com/wasmx/fizzy/pull/605) 
  [#625](https://github.com/wasmx/fizzy/pull/625) [#629](https://github.com/wasmx/fizzy/pull/629)
  [#646](https://github.com/wasmx/fizzy/pull/646) [#649](https://github.com/wasmx/fizzy/pull/649)
- New unit tests. [#571](https://github.com/wasmx/fizzy/pull/571) [#580](https://github.com/wasmx/fizzy/pull/580) 
  [#641](https://github.com/wasmx/fizzy/pull/641) [#648](https://github.com/wasmx/fizzy/pull/648)
  [#662](https://github.com/wasmx/fizzy/pull/662) [#666](https://github.com/wasmx/fizzy/pull/666)
- Update WebAssembly spec test suite. [#534](https://github.com/wasmx/fizzy/pull/534)
- Test utils improvements. [#579](https://github.com/wasmx/fizzy/pull/579) 
  [#658](https://github.com/wasmx/fizzy/pull/658) [#661](https://github.com/wasmx/fizzy/pull/661)
- Code and test cleanups and refactoring. [#527](https://github.com/wasmx/fizzy/pull/527) [#562](https://github.com/wasmx/fizzy/pull/562) 
  [#567](https://github.com/wasmx/fizzy/pull/567) [#578](https://github.com/wasmx/fizzy/pull/578) 
  [#606](https://github.com/wasmx/fizzy/pull/606) [#607](https://github.com/wasmx/fizzy/pull/607) 
  [#611](https://github.com/wasmx/fizzy/pull/611) [#617](https://github.com/wasmx/fizzy/pull/617)
  [#619](https://github.com/wasmx/fizzy/pull/619) [#623](https://github.com/wasmx/fizzy/pull/623)
  [#656](https://github.com/wasmx/fizzy/pull/656) [#663](https://github.com/wasmx/fizzy/pull/663)
  [#670](https://github.com/wasmx/fizzy/pull/670) [#671](https://github.com/wasmx/fizzy/pull/671) 
  [#673](https://github.com/wasmx/fizzy/pull/673)

### Fixed

- Remove unnecessary module copying when measuring instantiation performance for Fizzy in `fizzy-bench`. 
  [#581](https://github.com/wasmx/fizzy/pull/581)
- 32-bit instructions always clean (fill with zeroes) the higher 32-bit of the stack item. 
  [#612](https://github.com/wasmx/fizzy/pull/612) [#613](https://github.com/wasmx/fizzy/pull/613)
- Validating Data and Element sections now throw `validation_error` instead of `parser_error`.
  [#546](https://github.com/wasmx/fizzy/pull/546)
- `resolving_imported_functions` now behaves correctly when the same function is imported multiple times. 
  [#639](https://github.com/wasmx/fizzy/pull/639)  

## [0.5.0] — 2020-09-30

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
[0.5.0]: https://github.com/wasmx/fizzy/releases/tag/v0.5.0
[0.6.0]: https://github.com/wasmx/fizzy/releases/tag/v0.6.0
[0.7.0]: https://github.com/wasmx/fizzy/releases/tag/v0.7.0
[0.8.0]: https://github.com/wasmx/fizzy/releases/tag/v0.8.0

[Keep a Changelog]: https://keepachangelog.com/en/1.0.0/
[Semantic Versioning]: https://semver.org
[Profile-Guided Optimization]: https://en.wikipedia.org/wiki/Profile-guided_optimization
[README: Performance Testing]: ./README.md#Performance-testing
