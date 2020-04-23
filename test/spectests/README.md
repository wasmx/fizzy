# Fizzy Spectests Runner

## Build and run

Fizzy must be built with the `FIZZY_TESTING` option turned on:
```sh
$ mkdir build && cd build
$ cmake -DFIZZY_TESTING=ON ..
$ cmake --build .
```

It can then be executed:
```sh
$ bin/fizzy-spectests <test directory>
```

This will execute all test cases, but since Fizzy does not implement the complete validation specification, most of
those cases will fail. It is possible to skip them:
```sh
$ bin/fizzy-spectests --skip-validation <test directory>
```

## Preparing tests

Fizzy uses the official WebAssembly "[spec tests]", albeit not directly.
It requires the Wast files to be translated into a JSON format using [wabt]'s `wast2json` tool.

The reason for this is a design decision of Fizzy to not support the WebAssembly text format –– and unfortunately
the official test cases are in a text format.

In order to prepare the tests, run the following command for each file:
```sh
$ wast2json <file.wast> -o <file.json>
```

For ease of use, we have placed the JSON files of the [spec tests] v1.1 release here: https://github.com/wasmx/wasm-spec/tree/vanilla-json

[spec tests]: https://github.com/WebAssembly/spec/tree/master/test/core
[wabt]: https://github.com/WebAssembly/wabt
