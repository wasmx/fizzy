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

# Should use a release, like https://github.com/wasm3/wasm3/archive/v0.4.6.tar.gz, once changes are upstreamed.

ExternalProject_Add(wasm3
    EXCLUDE_FROM_ALL 1
    PREFIX ${prefix}
    DOWNLOAD_NAME wasm3-ext-api.tar.gz
    DOWNLOAD_DIR ${prefix}/downloads
    SOURCE_DIR ${source_dir}
    BINARY_DIR ${binary_dir}
    URL https://github.com/axic/wasm3/archive/ext-api.tar.gz
    URL_HASH SHA256=bef4e5534737f6054614b80c986dc6c5330178a8d288f1d338aec3296dbd990c
    CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_BUILD_TYPE=Release
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
