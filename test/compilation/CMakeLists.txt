# EVMC: Ethereum Client-VM Connector API.
# Copyright 2018-2020 The EVMC Authors.
# Licensed under the Apache License, Version 2.0.

# This CMake script creates multiple additional targets to test the compilation of public headers
# with different C standards.

set(STANDARDS c_std_99 c_std_11)

foreach(STANDARD 99 11)
    set(target test-compile-c${STANDARD})
    add_library(${target} OBJECT compilation_test.c)
    target_link_libraries(${target} PRIVATE fizzy::fizzy)
    set_target_properties(${target} PROPERTIES C_STANDARD ${STANDARD} C_EXTENSIONS OFF C_STANDARD_REQUIRED TRUE)
endforeach()
