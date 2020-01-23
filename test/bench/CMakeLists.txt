find_package(benchmark REQUIRED)

set(fizzy_include_dir ${PROJECT_SOURCE_DIR}/lib/fizzy)

add_executable(fizzy-bench bench.cpp)
target_compile_features(fizzy-bench PRIVATE cxx_std_17)
target_link_libraries(fizzy-bench PRIVATE fizzy::fizzy fizzy::test-utils benchmark::benchmark)
target_include_directories(fizzy-bench PRIVATE ${fizzy_include_dir})
