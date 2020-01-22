add_library(test-utils STATIC)
add_library(fizzy::test-utils ALIAS test-utils)

target_sources(
    test-utils PRIVATE
    hex.cpp
    hex.hpp
)

target_compile_features(test-utils PUBLIC cxx_std_17)
target_include_directories(test-utils PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/lib/fizzy)
