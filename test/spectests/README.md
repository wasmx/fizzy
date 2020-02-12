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

## Preparing tests

Fizzy uses the official WebAssembly "[spec tests]", albeit not directly.
It requires the Wast files to be translated into a JSON format using [wabt]'s `wast2json` tool.

The reason for this is a design decision of Fizzy to not support the WebAssembly text format –– and unfortunately
the official test cases are in a text format.

In order to prepare the tests, run the following command for each file:
```sh
$ wast2json <file.wast> -o <file.json>
```

## Fizzy's fork of the spec tests

A further limitation of Fizzy is the decision to (initially) not support floating point instructions.
The official test suite has some files purely for floating point instructions, but unfortunately many general
files (such as `memory`, `traps`, etc.) also make use of them.

As an interim solution there is a fork for an [integer-only suite here](https://github.com/wasmx/wasm-spec/tree/nofp)
(observe the `nofp` branch).

For ease of use (and integration into CircleCI) there is also a different branch containing the
[translated JSON files](https://github.com/wasmx/wasm-spec/tree/nofp-json) (the `nofp-json` branch).

[spec tests]: https://github.com/WebAssembly/spec/tree/master/test/core
[wabt]: https://github.com/WebAssembly/wabt
