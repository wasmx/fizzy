# Fizzy: A fast WebAssembly interpreter
# Copyright 2019-2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

if(WEVERYTHING)
    add_compile_options(
        -Wno-exit-time-destructors
        -Wno-global-constructors
        -Wno-unneeded-member-function  # Seems to be buggy.
        -Wno-unused-member-function    # Seems to be buggy.
        -Wno-used-but-marked-unused
    )
endif()

if(HUNTER_ENABLED)
    hunter_add_package(benchmark)
endif()
find_package(benchmark REQUIRED)

if(HUNTER_ENABLED)
    hunter_add_package(GTest)
endif()
find_package(GTest REQUIRED)

add_subdirectory(utils)
add_subdirectory(bench)
add_subdirectory(bench_internal)
add_subdirectory(compilation)
add_subdirectory(integration)
add_subdirectory(smoketests)
add_subdirectory(spectests)
add_subdirectory(testfloat)
add_subdirectory(unittests)

if(FIZZY_FUZZING)
    add_subdirectory(fuzzer)
endif()
