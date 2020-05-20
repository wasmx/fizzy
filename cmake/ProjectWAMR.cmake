if(ProjectWAMRIncluded)
    return()
endif()
set(ProjectWAMRIncluded TRUE)

include(ExternalProject)

set(prefix ${CMAKE_BINARY_DIR}/_deps)
set(source_dir ${prefix}/src/wamr)
set(binary_dir ${prefix}/src/wamr-build)
set(include_dir ${source_dir}/core/iwasm/include)
set(wamr_library ${binary_dir}/${CMAKE_STATIC_LIBRARY_PREFIX}vmlib${CMAKE_STATIC_LIBRARY_SUFFIX})

ExternalProject_Add(wamr
    EXCLUDE_FROM_ALL 1
    PREFIX ${prefix}
    DOWNLOAD_NAME WAMR-04-15-2020.tar.gz
    DOWNLOAD_DIR ${prefix}/downloads
    SOURCE_DIR ${source_dir}
    BINARY_DIR ${binary_dir}
    URL https://github.com/bytecodealliance/wasm-micro-runtime/archive/WAMR-04-15-2020.tar.gz
    URL_HASH SHA256=46f74568caec7abf51e7192e1eb73619cf2bf44f987ea22d86a1b103c6184751
    PATCH_COMMAND sh ${CMAKE_CURRENT_LIST_DIR}/patch_wamr.sh
    # different dir based on linux vs macos..
    SOURCE_SUBDIR product-mini/platforms/darwin
    CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DWAMR_BUILD_INTERP=1
    -DWAMR_BUILD_FAST_INTERP=0
    -DWAMR_BUILD_AOT=0
    -DWAMR_BUILD_AOT=0
    -DWAMR_BUILD_JIT=0
    -DWAMR_BUILD_LIBC_BUILTIN=0
    -DWAMR_BUILD_LIBC_WASI=0
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${wamr_library}
)

add_library(wamr::wamr STATIC IMPORTED)

set_target_properties(
    wamr::wamr
    PROPERTIES
    IMPORTED_CONFIGURATIONS Release
    IMPORTED_LOCATION_RELEASE ${wamr_library}
    INTERFACE_INCLUDE_DIRECTORIES "${include_dir}"
)

add_dependencies(wamr::wamr wamr)
