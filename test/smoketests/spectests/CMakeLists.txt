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
    PASS_REGULAR_EXPRESSION "PASSED 27, FAILED 0, SKIPPED 3"
)

add_test(
    NAME fizzy/smoketests/spectests/skipvalidation
    COMMAND fizzy-spectests ${CMAKE_CURRENT_LIST_DIR}/default --skip-validation
)
set_tests_properties(
    fizzy/smoketests/spectests/skipvalidation
    PROPERTIES
    PASS_REGULAR_EXPRESSION "PASSED 26, FAILED 0, SKIPPED 4"
)

add_test(
        NAME fizzy/smoketests/spectests/showpassed
        COMMAND fizzy-spectests ${CMAKE_CURRENT_LIST_DIR}/default --show-passed
)
set_tests_properties(
        fizzy/smoketests/spectests/showpassed
        PROPERTIES
        PASS_REGULAR_EXPRESSION "PASSED 27, FAILED 0, SKIPPED 3"
)

add_test(
        NAME fizzy/smoketests/spectests/showskipped
        COMMAND fizzy-spectests ${CMAKE_CURRENT_LIST_DIR}/default --show-skipped
)
set_tests_properties(
        fizzy/smoketests/spectests/showskipped
        PROPERTIES
        PASS_REGULAR_EXPRESSION "PASSED 27, FAILED 0, SKIPPED 3"
)

add_test(
        NAME fizzy/smoketests/spectests/hidefailed
        COMMAND fizzy-spectests ${CMAKE_CURRENT_LIST_DIR}/default --hide-failed
)
set_tests_properties(
        fizzy/smoketests/spectests/hidefailed
        PROPERTIES
        PASS_REGULAR_EXPRESSION "PASSED 27, FAILED 0, SKIPPED 3"
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

add_test(
    NAME fizzy/smoketests/spectests/broken
    COMMAND fizzy-spectests ${CMAKE_CURRENT_LIST_DIR}/broken
)
set_tests_properties(
    fizzy/smoketests/spectests/broken
    PROPERTIES
    PASS_REGULAR_EXPRESSION "PASSED 1, FAILED 2, SKIPPED 6"
)

add_test(
    NAME fizzy/smoketests/spectests/cli-missing-dir-arg
    COMMAND fizzy-spectests
)
set_tests_properties(
    fizzy/smoketests/spectests/cli-missing-dir-arg
    PROPERTIES
    PASS_REGULAR_EXPRESSION "Missing DIR argument"
)

add_test(
    NAME fizzy/smoketests/spectests/cli-invalid-arg
    COMMAND fizzy-spectests ${CMAKE_CURRENT_LIST_DIR}/default --nonexisting_argument
)
set_tests_properties(
    fizzy/smoketests/spectests/cli-invalid-arg
    PROPERTIES
    PASS_REGULAR_EXPRESSION "Unknown argument: --nonexisting_argument"
)


# Dump coverage data to distinct files (otherwise file will be overwritten).
set_tests_properties(
    fizzy/smoketests/spectests/default
    fizzy/smoketests/spectests/skipvalidation
    fizzy/smoketests/spectests/showpassed
    fizzy/smoketests/spectests/showskipped
    fizzy/smoketests/spectests/hidefailed
    fizzy/smoketests/spectests/failures
    fizzy/smoketests/spectests/broken
    fizzy/smoketests/spectests/cli-missing-dir-arg
    fizzy/smoketests/spectests/cli-invalid-arg
    PROPERTIES
    ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/spectests-%p.profraw
)
