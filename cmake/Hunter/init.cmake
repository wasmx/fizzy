# This file setups Hunter package manager.

include(FetchContent)

FetchContent_Declare(
    huntergate
    URL https://github.com/cpp-pm/hunter/archive/v0.23.297.tar.gz
    URL_HASH SHA256=077e5d33db470dd5af3afef1f436539e050a84f5daffe80cbea7c3e16a931d8e
)

FetchContent_GetProperties(huntergate)
if(NOT huntergate_POPULATED)
    FetchContent_Populate(huntergate)
    include(${huntergate_SOURCE_DIR}/cmake/HunterGate.cmake)
endif()

if(NOT CMAKE_CONFIGURATION_TYPES)
    if(CMAKE_BUILD_TYPE STREQUAL Debug)
        set(CONFIG Debug)
    else()
        set(CONFIG Release)
    endif()
    set(HUNTER_CONFIGURATION_TYPES ${CONFIG} CACHE STRING "Build type of Hunter packages")
    unset(CONFIG)
endif()

HunterGate(
    URL https://github.com/cpp-pm/hunter/archive/v0.23.294.tar.gz
    SHA1 0dd1ee8723d54a15822519c17a877c1f281fce39
    LOCAL
)
