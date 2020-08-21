# Fizzy: A fast WebAssembly interpreter
# Copyright 2019-2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

if(HUNTER_ENABLED)
    hunter_add_package(benchmark)
endif()
find_package(benchmark REQUIRED)

if(HUNTER_ENABLED)
    hunter_add_package(GTest)
endif()
find_package(GTest REQUIRED)

set(fizzy_include_dir ${PROJECT_SOURCE_DIR}/lib/fizzy)

add_subdirectory(utils)
add_subdirectory(bench)
add_subdirectory(bench_internal)
add_subdirectory(exhaustive)
add_subdirectory(spectests)
add_subdirectory(unittests)
add_subdirectory(smoketests)

if(FIZZY_FUZZING)
    add_subdirectory(fuzzer)
endif()
