

add_executable(test-roundeven roundeven_test.cpp)
target_link_libraries(test-roundeven PRIVATE fizzy::test-utils)
