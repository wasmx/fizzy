# Fizzy: A fast WebAssembly interpreter
# Copyright 2019-2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.15)

cmake_policy(SET CMP0077 NEW)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

include(cmake/cable.cmake)
include(CableBuildType)
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
set(PROJECT_VERSION 0.8.0-dev)
set(CMAKE_CXX_EXTENSIONS OFF)  # Disable extensions to C++ standards in Fizzy targets.

include(TestBigEndian)
test_big_endian(is_big_endian)
if(is_big_endian)
    message(FATAL_ERROR "${PROJECT_NAME} currently does not support big endian systems.")
endif()

cable_configure_compiler()
add_compile_options(
    -Wcast-qual
    -Wcast-align
    -Wmissing-declarations
    $<$<COMPILE_LANGUAGE:CXX>:-Wextra-semi>
    $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>
)
cable_add_cxx_compiler_flag_if_supported(-Wfinal-dtor-non-final-class)
cable_add_cxx_compiler_flag_if_supported(-Wnewline-eof)
cable_add_cxx_compiler_flag_if_supported(-Wsuggest-destructor-override)
cable_add_cxx_compiler_flag_if_supported(-Wunreachable-code-break)
cable_add_cxx_compiler_flag_if_supported(-Wduplicated-cond)
cable_add_cxx_compiler_flag_if_supported(-Wduplicate-enum)
cable_add_cxx_compiler_flag_if_supported(-Wlogical-op)
cable_add_cxx_compiler_flag_if_supported(-Wno-unknown-attributes)

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
            -Wno-return-std-move-in-c++11
            -Wno-switch-enum
        )
    endif()
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
