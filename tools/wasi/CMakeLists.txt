# Fizzy: A fast WebAssembly interpreter
# Copyright 2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

include(ProjectUVWASI)

add_library(wasi)
add_library(fizzy::wasi ALIAS wasi)
target_include_directories(wasi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wasi PUBLIC fizzy::fizzy-internal PRIVATE uvwasi::uvwasi)
target_sources(wasi PRIVATE wasi.cpp wasi.hpp wasi_uvwasi.cpp wasi_uvwasi.hpp)

add_executable(fizzy-wasi)
target_link_libraries(fizzy-wasi PRIVATE fizzy::wasi)
target_sources(fizzy-wasi PRIVATE main.cpp)

if(UNIX AND NOT APPLE)
    # For libstdc++ up to version 8 (included) this is needed for proper <filesystem> support.
    # Otherwise, the result program will crash on first use (no linker errors).
    # For libstdc++ 9, this is not needed, but cause no harm.
    # Clang compiler on linux may select libstdc++8 in some configurations.
    target_link_libraries(fizzy-wasi PRIVATE stdc++fs)
endif()
