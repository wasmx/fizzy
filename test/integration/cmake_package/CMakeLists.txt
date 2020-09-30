# Fizzy: A fast WebAssembly interpreter
# Copyright 2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

set(INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/install)
set(BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/build)
set(USE_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/use)

file(MAKE_DIRECTORY ${BUILD_DIR} ${USE_BUILD_DIR})

set(PREFIX ${PROJECT_NAME}/integration/cmake_package)

add_test(
    NAME ${PREFIX}/configure
    COMMAND ${CMAKE_COMMAND} -G ${CMAKE_GENERATOR} ${PROJECT_SOURCE_DIR} -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}
    WORKING_DIRECTORY ${BUILD_DIR}
)

add_test(
    NAME ${PREFIX}/install
    COMMAND ${CMAKE_COMMAND} --build ${BUILD_DIR} --target install
)
set_tests_properties(${PREFIX}/install PROPERTIES DEPENDS ${PREFIX}/configure)


add_test(
    NAME ${PREFIX}/use/configure
    COMMAND ${CMAKE_COMMAND} -G ${CMAKE_GENERATOR} ${CMAKE_CURRENT_SOURCE_DIR}/use_fizzy -DCMAKE_PREFIX_PATH=${INSTALL_DIR}
    WORKING_DIRECTORY ${USE_BUILD_DIR}
)
set_tests_properties(${PREFIX}/use/configure PROPERTIES DEPENDS ${PREFIX}/install)

add_test(
    NAME ${PREFIX}/use/build
    COMMAND ${CMAKE_COMMAND} --build ${USE_BUILD_DIR}
)
set_tests_properties(${PREFIX}/use/build PROPERTIES DEPENDS ${PREFIX}/use/configure)

