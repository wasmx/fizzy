if(ProjectWabtIncluded)
    return()
endif()
set(ProjectWabtIncluded TRUE)

include(ExternalProject)

set(prefix ${CMAKE_BINARY_DIR}/_deps)
set(source_dir ${prefix}/src/wabt)
set(binary_dir ${prefix}/src/wabt-build)
set(include_dir ${source_dir})
set(wabt_library ${binary_dir}/${CMAKE_STATIC_LIBRARY_PREFIX}wabt${CMAKE_STATIC_LIBRARY_SUFFIX})

set(flags "${fuzzing_flags} -fvisibility=hidden")
if(SANITIZE MATCHES address)
    # Instrument WABT with ASan - required for container-overflow checks.
    set(flags "-fsanitize=address ${flags}")
endif()

if(CMAKE_GENERATOR MATCHES Ninja)
    set(build_command BUILD_COMMAND ${CMAKE_COMMAND} --build . -j4)
endif()

ExternalProject_Add(wabt
    EXCLUDE_FROM_ALL 1
    PREFIX ${prefix}
    SOURCE_DIR ${source_dir}
    BINARY_DIR ${binary_dir}
    GIT_REPOSITORY https://github.com/ewasm/wabt
    GIT_TAG fixes
    GIT_SUBMODULES "third_party/wasm-c-api"
    GIT_SHALLOW TRUE
    CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DWITH_EXCEPTIONS=OFF
    -DBUILD_LIBWASM=OFF
    -DBUILD_TESTS=OFF
    -DBUILD_TOOLS=OFF
    -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE
    -DCMAKE_CXX_FLAGS=${flags}
    -DCMAKE_C_FLAGS=${flags}
    ${build_command}
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${wabt_library}
)

add_library(wabt::wabt STATIC IMPORTED)

set_target_properties(
    wabt::wabt
    PROPERTIES
    IMPORTED_CONFIGURATIONS Release
    IMPORTED_LOCATION_RELEASE ${wabt_library}
    INTERFACE_INCLUDE_DIRECTORIES "${include_dir};${binary_dir}"
)

add_dependencies(wabt::wabt wabt)
