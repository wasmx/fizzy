add_library(fizzy)
add_library(fizzy::fizzy ALIAS fizzy)

target_sources(
    fizzy PRIVATE
    bytes.hpp
    execute.cpp
    execute.hpp
    leb128.hpp
    limits.hpp
    parser.cpp
    parser.hpp
    parser_expr.cpp
    stack.hpp
    types.hpp
    utf8.cpp
    utf8.hpp
)
target_compile_features(fizzy PUBLIC cxx_std_17)
