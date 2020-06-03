# Fizzy: A fast WebAssembly interpreter
# Copyright 2019-2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

add_executable(fizzy-bench-internal)

target_sources(fizzy-bench-internal PRIVATE
    bench_internal.cpp
    execute_benchmarks.cpp
    experimental.cpp
    parser_benchmarks.cpp
    parser_noinline.cpp
)

target_link_libraries(fizzy-bench-internal PRIVATE fizzy::fizzy fizzy::test-utils benchmark::benchmark_main)
target_include_directories(fizzy-bench-internal PRIVATE ${fizzy_include_dir})
