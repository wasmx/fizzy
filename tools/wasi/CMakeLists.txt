# Fizzy: A fast WebAssembly interpreter
# Copyright 2019-2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

include(ProjectUVWASI)

set(fizzy_include_dir ${PROJECT_SOURCE_DIR}/lib/fizzy)

add_executable(fizzy-wasi wasi.cpp)
target_compile_features(fizzy-wasi PRIVATE cxx_std_17)
target_link_libraries(fizzy-wasi PRIVATE fizzy::fizzy-internal uvwasi::uvwasi)
target_include_directories(fizzy-wasi PRIVATE ${fizzy_include_dir})

if(UNIX AND NOT APPLE)
    # For libstdc++ up to version 8 (included) this is needed for proper <filesystem> support.
    # Otherwise, the result program will crash on first use (no linker errors).
    # For libstdc++ 9, this is not needed, but cause no harm.
    # Clang compiler on linux may select libstdc++8 in some configurations.
    target_link_libraries(fizzy-wasi PRIVATE stdc++fs)
endif()
