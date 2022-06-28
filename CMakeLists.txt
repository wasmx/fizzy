# Fizzy: A fast WebAssembly interpreter
# Copyright 2019-2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.15)

cmake_policy(SET CMP0077 NEW)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

include(cmake/cable.cmake)
include(CableBuildType)
include(CableCompileOptions)
include(CableCompilerSettings)
include(CMakeDependentOption)

option(FIZZY_TESTING "Enable Fizzy internal tests" OFF)
cmake_dependent_option(HUNTER_ENABLED "Enable Hunter package manager" ON
    "FIZZY_TESTING" OFF)

cmake_dependent_option(FIZZY_WASI "Enable WASI support" OFF "NOT FIZZY_TESTING" ON)

cmake_dependent_option(FIZZY_FUZZING "Enable Fizzy fuzzing" OFF "FIZZY_TESTING" OFF)

if(HUNTER_ENABLED)
    include(cmake/Hunter/init.cmake)
endif()

cable_set_build_type(DEFAULT Release CONFIGURATION_TYPES Debug;Release;Coverage)

project(fizzy LANGUAGES CXX C)
set(PROJECT_VERSION 0.9.0-dev)
set(CMAKE_CXX_EXTENSIONS OFF)  # Disable extensions to C++ standards in Fizzy targets.

include(TestBigEndian)
test_big_endian(is_big_endian)
if(is_big_endian)
    message(FATAL_ERROR "${PROJECT_NAME} currently does not support big endian systems.")
endif()

cable_configure_compiler()
cable_add_compile_options(
    -Wcast-qual
    -Wcast-align
    -Wmissing-declarations
    IF_SUPPORTED
    -Wextra-semi
    -Wold-style-cast
    -Wfinal-dtor-non-final-class
    -Wnewline-eof
    -Wsuggest-destructor-override
    -Wunreachable-code-break
    -Wduplicated-cond
    -Wduplicate-enum
    -Wlogical-op
    -Wno-unknown-attributes
)

if(CMAKE_CXX_COMPILER_ID MATCHES Clang)
    option(WEVERYTHING "Enable almost all compiler warnings" OFF)
    if(WEVERYTHING)
        add_compile_options(-Weverything)
        add_compile_options(
            -Wno-c++98-compat
            -Wno-c++98-compat-pedantic
            -Wno-covered-switch-default
            -Wno-double-promotion
            -Wno-float-equal
            -Wno-padded
            -Wno-switch-enum
        )
    endif()
endif()

# This currently is for checking 32-bit mode on x86_64.
# TODO: potentially include x86, i386, i686 here
if(CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    # On 32-bit opt-in for floating-point implementation with SSE2.
    # See the "x87 FPU" section in the README for explanation.
    add_compile_options(-msse2 -mfpmath=sse)
endif()

# An option to enable assertions in non-Debug build types.
# Disabling assertions in Debug build type has no effect (assertions are still enabled).
option(ENABLE_ASSERTIONS "Enable NDEBUG based assertions" OFF)
if(ENABLE_ASSERTIONS)
    foreach(flags_var_to_scrub
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS_MINSIZEREL
        CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_C_FLAGS_MINSIZEREL)
        string(REGEX REPLACE "(^| )[/-]D *NDEBUG($| )" " "
            "${flags_var_to_scrub}" "${${flags_var_to_scrub}}")
    endforeach()
endif()

if(FIZZY_FUZZING)
    set(fuzzing_flags -fsanitize=fuzzer-no-link,address,undefined,nullability,implicit-unsigned-integer-truncation,implicit-signed-integer-truncation)
    add_compile_options(${fuzzing_flags})
    add_link_options(${fuzzing_flags})
endif()


include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# Public include directory.
set(FIZZY_INCLUDE_DIR $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

add_subdirectory(lib)

if(FIZZY_TESTING)
    enable_testing()  # Enable CTest. Must be done in main CMakeLists.txt.
    add_subdirectory(test)
endif()

if(FIZZY_WASI)
    add_subdirectory(tools/wasi)
endif()

set(CMAKE_INSTALL_CMAKEPACKAGEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

write_basic_package_version_file(fizzyConfigVersion.cmake COMPATIBILITY ExactVersion)
configure_package_config_file(cmake/Config.cmake.in fizzyConfig.cmake INSTALL_DESTINATION ${CMAKE_INSTALL_CMAKEPACKAGEDIR})

install(TARGETS fizzy EXPORT fizzyTargets)
install(DIRECTORY include/fizzy TYPE INCLUDE)
install(EXPORT fizzyTargets NAMESPACE fizzy:: DESTINATION ${CMAKE_INSTALL_CMAKEPACKAGEDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fizzyConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/fizzyConfigVersion.cmake DESTINATION ${CMAKE_INSTALL_CMAKEPACKAGEDIR})
