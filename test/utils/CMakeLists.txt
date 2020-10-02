# Fizzy: A fast WebAssembly interpreter
# Copyright 2019-2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

add_library(test-utils STATIC)
add_library(fizzy::test-utils ALIAS test-utils)

target_sources(
    test-utils PRIVATE
    adler32.hpp
    asserts.cpp
    asserts.hpp
    execute_helpers.hpp
    fizzy_engine.cpp
    floating_point_utils.hpp
    hex.cpp
    hex.hpp
    leb128_encode.cpp
    leb128_encode.hpp
    wabt_engine.cpp
    wasm3_engine.cpp
    wasm_binary.hpp
    wasm_engine.cpp
    wasm_engine.hpp
)

target_compile_features(test-utils PUBLIC cxx_std_17)
target_include_directories(test-utils PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(test-utils PUBLIC fizzy::fizzy-internal PRIVATE wabt::wabt wasm3::wasm3 GTest::gtest)
