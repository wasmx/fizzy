# Changelog

Documentation of all notable changes to the **Fizzy** project.

The format is based on [Keep a Changelog],
and this project adheres to [Semantic Versioning].

## [0.2.0] — unreleased

Firstly, this release implements many validation steps prescribed by the specification, with the exception of type checking.

It passes a large part of the [official test suite (spectest 1.0)]:
  - 4481 of 4490 binary parser and execution tests,
  - 659 of 942 validation tests,
  - 6381 skipped due to containing floating-point instructions or testing text format parser.

Secondly, two major optimisations are included: branch resolution is done once in the parser and not during execution; and an optimised stack implementation is introduced.

In many cases this results in v0.2.0 executing code twice as fast compared to v0.1.0.

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
[0.2.0]: https://github.com/wasmx/fizzy/compare/v0.1.0...master

[Keep a Changelog]: https://keepachangelog.com/en/1.0.0/
[Semantic Versioning]: https://semver.org
[official test suite (spectest 1.0)]: https://github.com/WebAssembly/spec/releases/tag/wg-1.0
