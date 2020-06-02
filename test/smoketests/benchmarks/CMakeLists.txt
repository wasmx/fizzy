# Fizzy: A fast WebAssembly interpreter
# Copyright 2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

add_test(
    NAME fizzy/smoketests/bench/benchmarks
    COMMAND fizzy-bench ${CMAKE_CURRENT_LIST_DIR} --benchmark_min_time=0.01
)

add_test(
    NAME fizzy/smoketests/bench/cli-missing-dir-arg
    COMMAND fizzy-bench
)
set_tests_properties(
    fizzy/smoketests/bench/cli-missing-dir-arg
    PROPERTIES
    PASS_REGULAR_EXPRESSION "Missing DIR argument"
)

add_test(
    NAME fizzy/smoketests/bench/cli-invalid-arg
    COMMAND fizzy-bench ${CMAKE_CURRENT_LIST_DIR} --nonexisting_argument
)
set_tests_properties(
    fizzy/smoketests/bench/cli-invalid-arg
    PROPERTIES
    PASS_REGULAR_EXPRESSION "error: unrecognized command-line flag: --nonexisting_argument"
)


# Dump coverage data to distinct files (otherwise file will be overwritten).
set_tests_properties(
    fizzy/smoketests/bench/benchmarks
    fizzy/smoketests/bench/cli-missing-dir-arg
    fizzy/smoketests/bench/cli-invalid-arg
    PROPERTIES
    ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/bench-%p.profraw
)
