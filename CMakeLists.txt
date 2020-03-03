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

cmake_dependent_option(FIZZY_FUZZING "Enable Fizzy fuzzing" OFF "FIZZY_TESTING" OFF)

if(HUNTER_ENABLED)
    include(cmake/Hunter/init.cmake)
endif()

cable_set_build_type(DEFAULT Release CONFIGURATION_TYPES Debug;Release;Coverage)

project(fizzy LANGUAGES CXX C)

include(TestBigEndian)
test_big_endian(is_big_endian)
if(is_big_endian)
    message(FATAL_ERROR "${PROJECT_NAME} currently does not support big endian systems.")
endif()

cable_configure_compiler()

if(FIZZY_FUZZING)
    set(fuzzing_flags -fsanitize=fuzzer-no-link,undefined,address)
    add_compile_options(${fuzzing_flags})
    add_link_options(${fuzzing_flags})
endif()

set(include_dir ${PROJECT_SOURCE_DIR}/include)  # Public include directory.

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

add_subdirectory(lib)

if(FIZZY_TESTING)
    enable_testing()  # Enable CTest. Must be done in main CMakeLists.txt.
    add_subdirectory(test)
endif()
