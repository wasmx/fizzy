
include(GoogleTest)

if(HUNTER_ENABLED)
    hunter_add_package(GTest)
endif()
find_package(GTest REQUIRED)

add_executable(fizzy-unittests)
target_link_libraries(fizzy-unittests PRIVATE fizzy::fizzy fizzy::test-utils GTest::gtest_main)

target_sources(
    fizzy-unittests PRIVATE
    api_test.cpp
    end_to_end_test.cpp
    execute_call_test.cpp
    execute_control_test.cpp
    execute_numeric_test.cpp
    execute_test.cpp
    instantiate_test.cpp
    leb128_test.cpp
    parser_expr_test.cpp
    parser_test.cpp
    stack_test.cpp
    wasm_engine_test.cpp
    utf8_test.cpp
)

gtest_discover_tests(
    fizzy-unittests
    TEST_PREFIX ${PROJECT_NAME}/unittests/
    PROPERTIES ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/unittests-%p.profraw
)
