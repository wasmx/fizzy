if(ProjectWasm3Included)
    return()
endif()
set(ProjectWasm3Included TRUE)

include(ExternalProject)

set(prefix ${CMAKE_BINARY_DIR}/_deps)
set(source_dir ${prefix}/src/wasm3)
set(binary_dir ${prefix}/src/wasm3-build)
set(include_dir ${source_dir})
set(wasm3_library ${binary_dir}/source/${CMAKE_STATIC_LIBRARY_PREFIX}m3${CMAKE_STATIC_LIBRARY_SUFFIX})

if(CMAKE_TOOLCHAIN_FILE)
    set(toolchain_file -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()

ExternalProject_Add(wasm3
    EXCLUDE_FROM_ALL 1
    PREFIX ${prefix}
    DOWNLOAD_NAME wasm3-11f813d7.tar.gz
    DOWNLOAD_DIR ${prefix}/downloads
    SOURCE_DIR ${source_dir}
    BINARY_DIR ${binary_dir}
    URL https://github.com/wasm3/wasm3/archive/11f813d7ed659ed7c5b3faf6df0ff6e8f715f4e5.tar.gz
    URL_HASH SHA256=e24849bcc69100c5d25f93b0079c8e9b229780ec06110dbc09d2b4a6a362a84a
    CMAKE_ARGS
    ${toolchain_file}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DBUILD_WASI=none
    -DBUILD_NATIVE=OFF
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${wasm3_library}
)

add_library(wasm3::wasm3 STATIC IMPORTED)

set_target_properties(
    wasm3::wasm3
    PROPERTIES
    IMPORTED_CONFIGURATIONS Release
    IMPORTED_LOCATION_RELEASE ${wasm3_library}
    INTERFACE_INCLUDE_DIRECTORIES "${include_dir}"
)

add_dependencies(wasm3::wasm3 wasm3)
