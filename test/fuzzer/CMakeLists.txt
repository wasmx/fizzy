# Fizzy: A fast WebAssembly interpreter
# Copyright 2019-2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

add_executable(fizzy-fuzz-parser parser_fuzzer.cpp)
target_link_options(fizzy-fuzz-parser PRIVATE -fsanitize=fuzzer)
target_link_libraries(fizzy-fuzz-parser PRIVATE fizzy::fizzy-internal)
