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
- Support only Wasm binary encoding as an input (no support for WAT)

III) Conformance
- Should pass the official Wasm test suite

IV) First class support for determistic applications (*blockchain*)
- Support the bigint API
- Support runtime metering in the interpreter

## Testing

To read about testing see [fizzy-spectests](./test/spectests/README.md).

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
