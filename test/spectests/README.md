# Fizzy Spectests Runner

## Build and run

Fizzy must be built with the `FIZZY_TESTING` option turned on:
```shell script
mkdir build && cd build
cmake -DFIZZY_TESTING=ON ..
cmake --build .
```

It can then be executed:
```shell script
bin/fizzy-spectests <test directory>
```

## Preparing tests

Fizzy uses the official WebAssembly "[spec tests]", albeit not directly.
It requires the Wast files to be translated into a JSON format using [wabt]'s `wast2json` tool.

The reason for this is a design decision of Fizzy to not support the WebAssembly text format, and unfortunately
the official test cases are in a text format.

In order to prepare the tests, run the following command for each file:
```shell script
wast2json <file.wast> -o <file.json>
```

Make sure to disable all WebAssembly extensions when using `wast2json`:
```shell script
wast2json --disable-saturating-float-to-int --disable-sign-extension --disable-multi-value
```

To convert all files at once:
```shell script
# Make sure $options here contains the above settings
find test/core -name '*.wast' -exec wast2json $options {} \;
```

For ease of use, we have placed the JSON files of the spec tests [w3c-v1.0 branch] here:
- [Used before Fizzy 0.4](https://github.com/wasmx/wasm-spec/tree/w3c-1.0-jsontests-20200313/test/core/json)
- [Used since Fizzy 0.4](https://github.com/wasmx/wasm-spec/tree/w3c-1.0-jsontests-20200813/test/core/json)

[spec tests]: https://github.com/WebAssembly/spec/tree/master/test/core
[w3c-v1.0 branch]: https://github.com/WebAssembly/spec/tree/w3c-1.0/test/core
[wabt]: https://github.com/WebAssembly/wabt
