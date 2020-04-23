# Fizzy

[![webassembly badge]][webassembly]
[![readme style standard badge]][standard readme]
[![circleci badge]][circleci]
[![codecov badge]][codecov]
[![license badge]][Apache License, Version 2.0]

Fizzy aims to be a fast integer-only WebAssembly interpreter written in C++.

## Goals

I) Code quality
- Clean and modern C++17 codebase
- Easily embeddable (*and take part of the standardisation of the "C embedding API"*)

II) Simplicity
- Interpreter only
- Provide no support for floating point operations (initially)
- Support only WebAssembly binary encoding as an input (no support for WAT)

III) Conformance
- Should pass the official WebAssembly test suite

IV) First class support for determistic applications (*blockchain*)
- Support an efficient big integer API (256-bit and perhaps 384-bit)
- Support runtime metering in the interpreter
- Support enforcing a call depth bound
- Further restrictions of complexity (e.g. number of locals, number of function parameters, number of labels, etc.)

## Building and using

Fizzy uses CMake as a build system:
```sh
$ mkdir build && cd build
$ cmake ..
$ cmake --build .
```

This will build Fizzy as a library and since there is no public API
(the so called *embedder API* in WebAssembly) yet, this is not very useful.

Building with the `FIZZY_TESTING` option will output a few useful utilities:

```sh
$ mkdir build && cd build
$ cmake -DFIZZY_TESTING=ON ..
$ cmake --build .
```

These utilities are as follows:

### fizzy-bench

This can be used to run benchmarks available in the `test/benchmarks` directory.
Read [this guide](./test/benchmarks/README.md) for a short introduction.

### fizzy-spectests

Fizzy is capable of executing the official WebAssembly tests.
Follow [this guide](./test/spectests/README.md) for using the tool.

### fizzy-unittests

This is the unit tests suite of Fizzy.

## License

[![license badge]][Apache License, Version 2.0]

Licensed under the [Apache License, Version 2.0].

[webassembly]: https://webassembly.org/
[standard readme]: https://github.com/RichardLitt/standard-readme
[circleci]: https://circleci.com/gh/wasmx/fizzy/tree/master
[codecov]: https://codecov.io/gh/wasmx/fizzy/
[Apache License, Version 2.0]: LICENSE

[webassembly badge]: https://img.shields.io/badge/WebAssembly-Engine-informational.svg?logo=webassembly
[readme style standard badge]: https://img.shields.io/badge/readme%20style-standard-brightgreen.svg
[circleci badge]: https://img.shields.io/circleci/project/github/wasmx/fizzy/master.svg?logo=circleci
[codecov badge]: https://img.shields.io/codecov/c/github/wasmx/fizzy.svg?logo=codecov
[license badge]: https://img.shields.io/github/license/wasmx/fizzy.svg?logo=apache
