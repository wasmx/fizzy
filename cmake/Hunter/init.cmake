# This file setups Hunter package manager.

set(HUNTER_URL https://github.com/cpp-pm/hunter/archive/v0.23.239.tar.gz)
set(HUNTER_SHA1 135567a8493ab3499187bce1f2a8df9b449febf3)
set(HUNTER_PACKAGES GTest benchmark)

include(FetchContent)
FetchContent_Declare(
    SetupHunter
    URL https://github.com/cpp-pm/gate/archive/v0.9.1.tar.gz
    URL_HASH SHA256=33b6c9fdb47f364cd833baccc55100f9b8e9223387edcdfa616ed9661f897ec0
)
FetchContent_MakeAvailable(SetupHunter)
