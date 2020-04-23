# Fizzy: A fast WebAssembly interpreter
# Copyright 2019-2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

add_executable(fizzy-bench bench.cpp)
target_compile_features(fizzy-bench PRIVATE cxx_std_17)
target_link_libraries(fizzy-bench PRIVATE fizzy::fizzy fizzy::test-utils benchmark::benchmark)
target_include_directories(fizzy-bench PRIVATE ${fizzy_include_dir})

if(UNIX AND NOT APPLE)
    # For libstdc++ up to version 8 (included) this is needed for proper <filesystem> support.
    # Otherwise, the result program will crash on first use (no linker errors).
    # For libstdc++ 9, this is not needed, but cause no harm.
    # Clang compiler on linux may select libstdc++8 in some configurations.
    target_link_libraries(fizzy-bench PRIVATE stdc++fs)
endif()
