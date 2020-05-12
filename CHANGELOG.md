# Changelog

Documentation of all notable changes to the **Fizzy** project.

The format is based on [Keep a Changelog],
and this project adheres to [Semantic Versioning].


## [0.1.0] — unreleased

First release!

### Added

- Supports all instructions with the exception of floating-point ones.
- Passes a large part of the [official test suite (spectest 1.0)](https://github.com/WebAssembly/spec/releases/tag/wg-1.0):
  - 4482 of 4490 binary parser tests,
  - 301 of 942 validation tests (no full validation implemented yet),
  - 6381 skipped due to containing floating-point instructions or testing text format parser.
- Tools included:
  - `fizzy-bench` for benchmarking
  - `fizzy-spectests` for executing the official test suite
  - `fizzy-unittests` for running unit tests
- It is missing a public API (the embedder API) and thus not ready yet for integration.

[0.1.0]: https://github.com/wasmx/fizzy/compare/2366fff3...master

[Keep a Changelog]: https://keepachangelog.com/en/1.0.0/
[Semantic Versioning]: https://semver.org
