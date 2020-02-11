# Fizzy

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

Apache 2.0
