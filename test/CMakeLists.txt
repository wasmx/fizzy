if(HUNTER_ENABLED)
    hunter_add_package(benchmark)
endif()
find_package(benchmark REQUIRED)

set(fizzy_include_dir ${PROJECT_SOURCE_DIR}/lib/fizzy)

add_subdirectory(utils)
add_subdirectory(bench)
add_subdirectory(bench_internal)
add_subdirectory(spectests)
add_subdirectory(unittests)

if(FIZZY_FUZZING)
    add_subdirectory(fuzzer)
endif()
