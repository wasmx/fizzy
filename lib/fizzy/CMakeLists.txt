# Fizzy: A fast WebAssembly interpreter
# Copyright 2019-2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

add_library(fizzy)
add_library(fizzy::fizzy ALIAS fizzy)
target_compile_features(fizzy PUBLIC cxx_std_17)
target_include_directories(fizzy PUBLIC ${FIZZY_INCLUDE_DIR})

target_sources(
    fizzy PRIVATE
    ${FIZZY_INCLUDE_DIR}/fizzy/fizzy.h
    cxx20/bit.hpp
    cxx20/span.hpp
    bytes.hpp
    capi.cpp
    constexpr_vector.hpp
    exceptions.cpp
    exceptions.hpp
    execute.cpp
    execute.hpp
    instantiate.cpp
    instantiate.hpp
    instructions.cpp
    instructions.hpp
    leb128.hpp
    limits.hpp
    module.hpp
    parser.cpp
    parser.hpp
    parser_expr.cpp
    stack.hpp
    trunc_boundaries.hpp
    types.hpp
    utf8.cpp
    utf8.hpp
    value.hpp
)
