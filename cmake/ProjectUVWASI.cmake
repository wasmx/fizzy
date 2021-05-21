if(ProjectUVWASIIncluded)
    return()
endif()
set(ProjectUVWASIIncluded TRUE)

include(ExternalProject)

set(prefix ${CMAKE_BINARY_DIR}/_deps)
set(source_dir ${prefix}/src/uvwasi)
set(binary_dir ${prefix}/src/uvwasi-build)
set(include_dir ${source_dir}/include)
set(uvwasi_library ${binary_dir}/${CMAKE_STATIC_LIBRARY_PREFIX}uvwasi_a${CMAKE_STATIC_LIBRARY_SUFFIX})
set(uv_library ${binary_dir}/_deps/libuv-build/${CMAKE_STATIC_LIBRARY_PREFIX}uv_a${CMAKE_STATIC_LIBRARY_SUFFIX})

if(UNIX AND NOT APPLE)
    set(system_libs "pthread;dl;rt")
endif()

if(CMAKE_TOOLCHAIN_FILE)
    set(toolchain_file -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()

ExternalProject_Add(uvwasi
    EXCLUDE_FROM_ALL 1
    PREFIX ${prefix}
    DOWNLOAD_NAME uvwasi-0.0.10.tar.gz
    DOWNLOAD_DIR ${prefix}/downloads
    SOURCE_DIR ${source_dir}
    BINARY_DIR ${binary_dir}
    URL https://github.com/nodejs/uvwasi/archive/v0.0.10.tar.gz
    URL_HASH SHA256=39135f4dd4a44013399ceed7166391ffc5c09655e4bfbf851da2be039e6985df
    CMAKE_ARGS
    ${toolchain_file}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DUVWASI_DEBUG_LOG=OFF
    -DCODE_COVERAGE=OFF
    -DASAN=OFF
    -DUVWASI_BUILD_TESTS=OFF
    BUILD_BYPRODUCTS ${uvwasi_library} ${uv_library}
)

add_library(uvwasi::uvwasi STATIC IMPORTED)

file(MAKE_DIRECTORY ${include_dir})

set_target_properties(
    uvwasi::uvwasi
    PROPERTIES
    IMPORTED_CONFIGURATIONS Release
    IMPORTED_LOCATION_RELEASE ${uvwasi_library}
    INTERFACE_INCLUDE_DIRECTORIES "${include_dir};${binary_dir}"
    INTERFACE_LINK_LIBRARIES "${uv_library};${system_libs}"
)

add_dependencies(uvwasi::uvwasi uvwasi)
