if(HUNTER_ENABLED)
    hunter_add_package(nlohmann_json)
endif()
find_package(nlohmann_json REQUIRED)

add_executable(fizzy-spectests spectests.cpp)
target_compile_features(fizzy-spectests PRIVATE cxx_std_17)
target_link_libraries(fizzy-spectests PRIVATE fizzy::fizzy fizzy::test-utils nlohmann_json::nlohmann_json)
target_include_directories(fizzy-spectests PRIVATE ${fizzy_include_dir})

if(UNIX AND NOT APPLE)
    # For libstdc++ up to version 8 (included) this is needed for proper <filesystem> support.
    # Otherwise, the result program will crash on first use (no linker errors).
    # For libstdc++ 9, this is not needed, but cause no harm.
    # Clang compiler on linux may select libstdc++8 in some configurations.
    target_link_libraries(fizzy-spectests PRIVATE stdc++fs)
endif()
