# This file setups Hunter package manager.

include(FetchContent)

FetchContent_Declare(
    huntergate
    URL https://github.com/cpp-pm/gate/archive/v0.9.2.tar.gz
    URL_HASH SHA256=edef54e8cec019a7b4e6b1e1d9e76346c431a6b64d0717422ec903dcd62b3dad
)

FetchContent_GetProperties(huntergate)
if(NOT huntergate_POPULATED)
    FetchContent_Populate(huntergate)
    include(${huntergate_SOURCE_DIR}/cmake/HunterGate.cmake)
endif()

if(NOT CMAKE_CONFIGURATION_TYPES)
    if(CMAKE_BUILD_TYPE STREQUAL Debug)
        set(HUNTER_CONFIGURATION_TYPES Debug)
    else()
        set(HUNTER_CONFIGURATION_TYPES Release)
    endif()
endif()

HunterGate(
    URL https://github.com/cpp-pm/hunter/archive/v0.23.239.tar.gz
    SHA1 135567a8493ab3499187bce1f2a8df9b449febf3
    LOCAL
)
