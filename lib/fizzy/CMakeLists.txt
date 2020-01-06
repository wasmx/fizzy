
add_library(fizzy)
add_library(fizzy::fizzy ALIAS fizzy)

target_sources(
    fizzy PRIVATE
    dummy.cpp
    leb128.hpp
    types.hpp
)
target_compile_features(fizzy PUBLIC cxx_std_17)
