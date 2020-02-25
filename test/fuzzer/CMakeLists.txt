
add_executable(fizzy-fuzz-parser parser_fuzzer.cpp)
target_link_options(fizzy-fuzz-parser PRIVATE -fsanitize=fuzzer)
target_link_libraries(fizzy-fuzz-parser PRIVATE fizzy::fizzy)
target_include_directories(fizzy-fuzz-parser PRIVATE ${fizzy_include_dir})
