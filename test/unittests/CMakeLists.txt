# Fizzy: A fast WebAssembly interpreter
# Copyright 2019-2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

include(GoogleTest)

add_executable(fizzy-unittests)
target_link_libraries(fizzy-unittests PRIVATE fizzy::fizzy-internal fizzy::test-utils GTest::gtest_main GTest::gmock)

target_sources(
    fizzy-unittests PRIVATE
    api_test.cpp
    asserts_test.cpp
    capi_execute_test.cpp
    capi_exports_test.cpp
    capi_instantiate_test.cpp
    capi_module_test.cpp
    capi_test.cpp
    constexpr_vector_test.cpp
    cxx20_bit_test.cpp
    cxx20_span_test.cpp
    cxx23_utility_test.cpp
    end_to_end_test.cpp
    execute_call_depth_test.cpp
    execute_call_test.cpp
    execute_control_test.cpp
    execute_death_test.cpp
    execute_floating_point_conversion_test.cpp
    execute_floating_point_test.cpp
    execute_floating_point_test.hpp
    execute_invalid_test.cpp
    execute_numeric_test.cpp
    execute_test.cpp
    floating_point_utils_test.cpp
    instantiate_test.cpp
    leb128_test.cpp
    module_test.cpp
    oom_test.cpp
    parser_expr_test.cpp
    parser_test.cpp
    stack_test.cpp
    test_utils_test.cpp
    typed_value_test.cpp
    types_test.cpp
    utf8_test.cpp
    validation_stack_test.cpp
    validation_stack_type_test.cpp
    validation_test.cpp
    value_test.cpp
    wasm_engine_test.cpp
)

if(CABLE_COMPILER_GNU AND (CMAKE_CXX_COMPILER_VERSION EQUAL "11.2.0"))
    # GCC11 generates false positive error when linking capi_instantiate_test.cpp with sanitizers.
    set_source_files_properties(capi_instantiate_test.cpp PROPERTIES COMPILE_OPTIONS "-fno-sanitize=pointer-compare")
endif()


gtest_discover_tests(
    fizzy-unittests
    TEST_PREFIX ${PROJECT_NAME}/unittests/
    PROPERTIES ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/unittests-%p.profraw
)
