
add_library(fizzy)
add_library(fizzy::fizzy ALIAS fizzy)

target_sources(
    fizzy PRIVATE
    dummy.cpp
    execute.cpp
    execute.hpp
    leb128.hpp
    types.hpp
    parser.cpp
    parser.hpp
    parser_expr.cpp
)
target_compile_features(fizzy PUBLIC cxx_std_17)
