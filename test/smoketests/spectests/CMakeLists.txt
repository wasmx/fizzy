# Fizzy: A fast WebAssembly interpreter
# Copyright 2019-2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

add_test(
    NAME fizzy/smoketests/spectests/default
    COMMAND fizzy-spectests ${CMAKE_CURRENT_LIST_DIR}/default
)
set_tests_properties(
    fizzy/smoketests/spectests/default
    PROPERTIES
    PASS_REGULAR_EXPRESSION "PASSED 23, FAILED 0, SKIPPED 7"
)

add_test(
    NAME fizzy/smoketests/spectests/skipvalidation
    COMMAND fizzy-spectests ${CMAKE_CURRENT_LIST_DIR}/default --skip-validation
)
set_tests_properties(
    fizzy/smoketests/spectests/skipvalidation
    PROPERTIES
    PASS_REGULAR_EXPRESSION "PASSED 22, FAILED 0, SKIPPED 8"
)

add_test(
    NAME fizzy/smoketests/spectests/failures
    COMMAND fizzy-spectests ${CMAKE_CURRENT_LIST_DIR}/failures
)
set_tests_properties(
    fizzy/smoketests/spectests/failures
    PROPERTIES
    PASS_REGULAR_EXPRESSION "PASSED 4, FAILED 18, SKIPPED 2"
)


# Dump coverage data to distinct files (otherwise file will be overwritten).
set_tests_properties(
    fizzy/smoketests/spectests/default
    fizzy/smoketests/spectests/skipvalidation
    fizzy/smoketests/spectests/failures
    PROPERTIES
    ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/spectests-%p.profraw
)
