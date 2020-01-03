include(FetchContent)
FetchContent_Declare(
    wabt
    URL https://github.com/WebAssembly/wabt/archive/1.0.12.tar.gz
    URL_HASH SHA256=5333949ed4ae63808afa0d1f7d627cd7485ebeec339590571e5f2cb21e304f79
)
set(BUILD_TESTS OFF)
set(BUILD_TOOLS OFF)
FetchContent_MakeAvailable(wabt)
target_include_directories(wabt INTERFACE ${wabt_SOURCE_DIR} ${wabt_BINARY_DIR})
add_library(wabt::wabt ALIAS wabt)
