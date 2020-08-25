# Fizzy: A fast WebAssembly interpreter
# Copyright 2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

add_executable(fizzy-testfloat testfloat.cpp)
target_link_libraries(fizzy-testfloat PRIVATE fizzy::fizzy fizzy::test-utils)
target_include_directories(fizzy-testfloat PRIVATE ${PROJECT_SOURCE_DIR}/lib)

