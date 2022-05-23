# Fizzy: A fast WebAssembly interpreter
# Copyright 2019-2021 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

include(GoogleTest)

add_executable(fizzy-unittests-wasi)
target_link_libraries(fizzy-unittests-wasi PRIVATE fizzy::fizzy-internal fizzy::wasi fizzy::test-utils GTest::gtest_main GTest::gmock)

target_sources(
    fizzy-unittests-wasi PRIVATE
    wasi_test.cpp
)

gtest_discover_tests(
    fizzy-unittests-wasi
    TEST_PREFIX ${PROJECT_NAME}/unittests_wasi/
    PROPERTIES ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/unittests-wasi-%p.profraw
)
