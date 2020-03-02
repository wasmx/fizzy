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

ExternalProject_Add(wabt
    EXCLUDE_FROM_ALL 1
    PREFIX ${prefix}
    DOWNLOAD_NAME wabt-1.0.12.tar.gz
    DOWNLOAD_DIR ${prefix}/downloads
    SOURCE_DIR ${source_dir}
    BINARY_DIR ${binary_dir}
    URL https://github.com/WebAssembly/wabt/archive/1.0.12.tar.gz
    URL_HASH SHA256=5333949ed4ae63808afa0d1f7d627cd7485ebeec339590571e5f2cb21e304f79
    CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DWITH_EXCEPTIONS=OFF
    -DBUILD_TESTS=OFF
    -DBUILD_TOOLS=OFF
    -DCMAKE_POSITION_INDEPENDENT_CODE=FALSE
    -DCMAKE_CXX_FLAGS=-fvisibility=hidden
    -DCMAKE_C_FLAGS=-fvisibility=hidden
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
