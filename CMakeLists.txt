cmake_minimum_required(VERSION 3.13)

option(FIZZY_TESTING "Enable Fizzy internal tests" OFF)

project(fizzy LANGUAGES CXX)

set(include_dir ${PROJECT_SOURCE_DIR}/include)  # Public include directory.

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

add_subdirectory(lib)

if(FIZZY_TESTING)
    enable_testing()  # Enable CTest. Must be done in main CMakeLists.txt.
    add_subdirectory(test)
endif()
