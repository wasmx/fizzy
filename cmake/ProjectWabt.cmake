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

set(CPPFLAGS "-fvisibility=hidden")  # Flags for both C and C++ compilation.
set(CXXFLAGS "")                     # Flags for C++ compilation only.

if(CMAKE_CXX_FLAGS MATCHES "-stdlib=libc\\+\\+")
    set(CXXFLAGS "-stdlib=libc++ ${CXXFLAGS}")
endif()

if(CMAKE_CXX_FLAGS MATCHES "-m32")
    set(CPPFLAGS "-m32 ${CPPFLAGS}")
endif()

if(SANITIZE MATCHES address)
    # Instrument WABT with ASan - required for container-overflow checks.
    set(CPPFLAGS "-fsanitize=address ${CPPFLAGS}")
endif()

set(CXXFLAGS "${CPPFLAGS} ${CXXFLAGS}")

if(CMAKE_GENERATOR MATCHES Ninja)
    set(build_command BUILD_COMMAND ${CMAKE_COMMAND} --build . -j4)
endif()

if(CMAKE_TOOLCHAIN_FILE)
    set(toolchain_file -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()


ExternalProject_Add(
    wabt
    EXCLUDE_FROM_ALL 1
    PREFIX ${prefix}
    DOWNLOAD_NAME wabt-1.0.19.tar.gz
    DOWNLOAD_DIR ${prefix}/downloads
    SOURCE_DIR ${source_dir}
    BINARY_DIR ${binary_dir}
    URL https://github.com/WebAssembly/wabt/archive/1.0.19.tar.gz
    URL_HASH SHA256=134f2afc8205d0a3ab89c5f0d424ff3823e9d2769c39d2235aa37eba7abc15ba
    CMAKE_ARGS
    # TODO: This is ineffective. Seems flags from the toolchain file are overwritten by -DCMAKE_CXX_FLAGS=
    ${toolchain_file}
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DWITH_EXCEPTIONS=OFF
    -DBUILD_LIBWASM=OFF
    -DBUILD_TESTS=OFF
    -DBUILD_TOOLS=OFF
    -DCMAKE_POSITION_INDEPENDENT_CODE=FALSE
    -DCMAKE_CXX_FLAGS=${CXXFLAGS}
    -DCMAKE_C_FLAGS=${CPPFLAGS}
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
