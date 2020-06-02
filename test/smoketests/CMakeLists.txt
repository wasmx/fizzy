# Fizzy: A fast WebAssembly interpreter
# Copyright 2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(benchmarks)
add_subdirectory(spectests)
if(FIZZY_WASI)
    add_subdirectory(wasi)
endif()
