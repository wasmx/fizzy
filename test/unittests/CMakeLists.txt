
include(GoogleTest)

find_package(GTest CONFIG REQUIRED)

add_executable(fizzy-unittests)
target_link_libraries(fizzy-unittests PRIVATE fizzy::fizzy GTest::gtest_main)
target_include_directories(fizzy-unittests PRIVATE ${PROJECT_SOURCE_DIR}/lib/fizzy)

target_sources(
    fizzy-unittests PRIVATE
    leb128_test.cpp
    parser_test.cpp
    utils.cpp
    utils.hpp
)

gtest_discover_tests(
    fizzy-unittests
    TEST_PREFIX ${PROJECT_NAME}/unittests/
    PROPERTIES ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/unittests-%p.profraw
)
