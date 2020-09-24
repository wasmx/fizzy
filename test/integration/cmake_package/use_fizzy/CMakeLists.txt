# Fizzy: A fast WebAssembly interpreter
# Copyright 2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

# This example shows how to use the fizzy library from the fizzy CMake package.
cmake_minimum_required(VERSION 3.15)
project(use_fizzy LANGUAGES CXX)
find_package(fizzy CONFIG REQUIRED)

add_executable(use_fizzy use_fizzy.cpp)
target_link_libraries(use_fizzy PRIVATE fizzy::fizzy)
