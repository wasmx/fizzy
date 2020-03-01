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

## Benchmarking

Build with `-DFIZZY_TESTING=ON`.

### Tools

1. `fizzy-bench`
2. `fizzy-bench-internal`

### References

1. https://www.youtube.com/watch?v=zWxSZcpeS8Q
2. https://www.youtube.com/watch?v=Czr5dBfs72U
3. https://www.youtube.com/watch?v=2EWejmkKlxs
4. https://www.youtube.com/watch?v=nXaxk27zwlk

## License

Apache 2.0
