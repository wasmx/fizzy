# Fizzy: A fast WebAssembly interpreter
# Copyright 2019-2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

if(HUNTER_ENABLED)
    hunter_add_package(nlohmann_json)
endif()
find_package(nlohmann_json REQUIRED)

add_executable(fizzy-spectests spectests.cpp)
target_compile_features(fizzy-spectests PRIVATE cxx_std_17)
target_link_libraries(fizzy-spectests PRIVATE fizzy::fizzy-internal fizzy::test-utils nlohmann_json::nlohmann_json)

if(UNIX AND NOT APPLE)
    # For libstdc++ up to version 8 (included) this is needed for proper <filesystem> support.
    # Otherwise, the result program will crash on first use (no linker errors).
    # For libstdc++ 9, this is not needed, but cause no harm.
    # Clang compiler on linux may select libstdc++8 in some configurations.
    target_link_libraries(fizzy-spectests PRIVATE stdc++fs)
endif()
