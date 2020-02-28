add_executable(fizzy-bench-internal)

target_sources(fizzy-bench-internal PRIVATE
    bench_internal.cpp
    experimental.cpp
    parser_benchmarks.cpp
    parser_noinline.cpp
)

target_link_libraries(fizzy-bench-internal PRIVATE fizzy::fizzy fizzy::test-utils benchmark::benchmark_main)
target_include_directories(fizzy-bench-internal PRIVATE ${fizzy_include_dir})
