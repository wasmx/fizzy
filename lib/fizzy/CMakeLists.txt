# Fizzy: A fast WebAssembly interpreter
# Copyright 2019-2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

add_library(fizzy)
add_library(fizzy::fizzy ALIAS fizzy)

target_sources(
    fizzy PRIVATE
    bytes.hpp
    execute.cpp
    execute.hpp
    instructions.cpp
    instructions.hpp
    leb128.hpp
    limits.hpp
    module.hpp
    parser.cpp
    parser.hpp
    parser_expr.cpp
    stack.hpp
    types.hpp
    utf8.cpp
    utf8.hpp
)
target_compile_features(fizzy PUBLIC cxx_std_17)
