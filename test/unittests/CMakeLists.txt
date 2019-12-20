
include(GoogleTest)

find_package(GTest REQUIRED)

add_executable(fizzy-unittests)
target_link_libraries(fizzy-unittests PRIVATE fizzy::fizzy GTest::Main)
target_include_directories(fizzy-unittests PRIVATE ${PROJECT_SOURCE_DIR}/lib/fizzy)

target_sources(
    fizzy-unittests PRIVATE
    leb128_test.cpp
)

gtest_discover_tests(fizzy-unittests TEST_PREFIX ${PROJECT_NAME}/unittests/)
