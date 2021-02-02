# Fizzy: A fast WebAssembly interpreter
# Copyright 2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

# The toolchain file for arm64 cross-compilation.
# It selects default version of GCC targeting aarch64-linux-gnu, tested with GCC 10.

set(CMAKE_SYSTEM_PROCESSOR arm64)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Emulation with qemu. Used for test discovery and execution. Debian specific.
set(CMAKE_CROSSCOMPILING_EMULATOR qemu-aarch64-static;-L;/usr/aarch64-linux-gnu)

set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
