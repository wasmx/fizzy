include(ProjectWabt)
include(ProjectWasm3)

add_library(test-utils STATIC)
add_library(fizzy::test-utils ALIAS test-utils)

target_sources(
    test-utils PRIVATE
    asserts.hpp
    fizzy_engine.cpp
    hex.cpp
    hex.hpp
    wabt_engine.cpp
    wasm3_engine.cpp
    wasm_engine.hpp
)

target_compile_features(test-utils PUBLIC cxx_std_17)
target_include_directories(test-utils PUBLIC ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/lib/fizzy)
target_link_libraries(test-utils PRIVATE fizzy::fizzy wabt::wabt wasm3::wasm3)
