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
    DOWNLOAD_NAME uvwasi-0.0.23.tar.gz
    DOWNLOAD_DIR ${prefix}/downloads
    SOURCE_DIR ${source_dir}
    BINARY_DIR ${binary_dir}
    URL https://github.com/nodejs/uvwasi/archive/v0.0.23.tar.gz
    URL_HASH SHA256=cdb148aac298883b51da887657deca910c7c02f35435e24f125cef536fe8d5e1
    CMAKE_ARGS
    ${toolchain_file}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DBUILD_SHARED_LIBS=OFF
    -DUVWASI_DEBUG_LOG=OFF
    -DCODE_COVERAGE=OFF
    -DASAN=OFF
    -DUVWASI_BUILD_TESTS=OFF
    # packaged libuv version depends on this
    -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE
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
