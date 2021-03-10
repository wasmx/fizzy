# Fizzy: A fast WebAssembly interpreter
# Copyright 2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

add_test(
    NAME fizzy/smoketests/wasi/cli-missing-arg
    COMMAND fizzy-wasi
)
set_tests_properties(
    fizzy/smoketests/wasi/cli-missing-arg
    PROPERTIES
    PASS_REGULAR_EXPRESSION "Missing executable argument"
)

add_test(
    NAME fizzy/smoketests/wasi/cli-invalid-file
    COMMAND fizzy-wasi ${CMAKE_CURRENT_LIST_DIR}
)
set_tests_properties(
    fizzy/smoketests/wasi/cli-invalid-file
    PROPERTIES
    PASS_REGULAR_EXPRESSION "Not a file:"
)

add_test(
    NAME fizzy/smoketests/wasi/cli-missing-file
    COMMAND fizzy-wasi ${CMAKE_CURRENT_LIST_DIR}/helloworld.nonexistent.wasm
)
set_tests_properties(
    fizzy/smoketests/wasi/cli-missing-file
    PROPERTIES
    PASS_REGULAR_EXPRESSION "File does not exist:"
)

add_test(
    NAME fizzy/smoketests/wasi/helloworld
    COMMAND fizzy-wasi ${CMAKE_CURRENT_LIST_DIR}/helloworld.wasm
)
set_tests_properties(
    fizzy/smoketests/wasi/helloworld
    PROPERTIES
    PASS_REGULAR_EXPRESSION "hello world"
)

add_test(
    NAME fizzy/smoketests/wasi/missing-start
    COMMAND fizzy-wasi ${CMAKE_CURRENT_LIST_DIR}/missingstart.wasm
)
set_tests_properties(
    fizzy/smoketests/wasi/missing-start
    PROPERTIES
    PASS_REGULAR_EXPRESSION "File is not WASI compatible \\(_start not found\\)"
)

add_test(
    NAME fizzy/smoketests/wasi/invalid-start
    COMMAND fizzy-wasi ${CMAKE_CURRENT_LIST_DIR}/invalidstart.wasm
)
set_tests_properties(
    fizzy/smoketests/wasi/invalid-start
    PROPERTIES
    PASS_REGULAR_EXPRESSION "File is not WASI compatible \\(_start has invalid signature\\)"
)

add_test(
    NAME fizzy/smoketests/wasi/missing-memory
    COMMAND fizzy-wasi ${CMAKE_CURRENT_LIST_DIR}/missingmemory.wasm
)
set_tests_properties(
    fizzy/smoketests/wasi/missing-memory
    PROPERTIES
    PASS_REGULAR_EXPRESSION "File is not WASI compatible \\(no memory exported\\)"
)

add_test(
    NAME fizzy/smoketests/wasi/trap
    COMMAND fizzy-wasi ${CMAKE_CURRENT_LIST_DIR}/trap.wasm
)
set_tests_properties(
    fizzy/smoketests/wasi/trap
    PROPERTIES
    PASS_REGULAR_EXPRESSION "Execution aborted with WebAssembly trap"
)

# Dump coverage data to distinct files (otherwise file will be overwritten).
set_tests_properties(
    fizzy/smoketests/wasi/cli-missing-arg
    fizzy/smoketests/wasi/cli-missing-file
    fizzy/smoketests/wasi/cli-invalid-file
    fizzy/smoketests/wasi/helloworld
    fizzy/smoketests/wasi/missing-start
    fizzy/smoketests/wasi/invalid-start
    fizzy/smoketests/wasi/missing-memory
    fizzy/smoketests/wasi/trap
    PROPERTIES
    ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/wasi-%p.profraw
)
